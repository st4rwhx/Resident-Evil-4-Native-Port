#include "runtime/gx/efb_peek.h"
#include "runtime/log.h"
#include "runtime/game_version.h"
#include <cmath>
#include "runtime/gfx_settings.h"
#include <chrono>
#include "runtime/env.h"
#include "runtime/dev_tour.h"
#include "runtime/perf_journal.h"
#include "runtime/cpu_context.h"
#include "runtime/gx/fifo_parser.h"
#include "runtime/gx/frame_renderer.h"
#include "runtime/gx/tev_shader_gen.h"
#include "runtime/gx_state.h"
#include "runtime/texture_cache_gl.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "common/gl.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "runtime/obf.h"
#include "common/platform.h"
#include <filesystem>
#include <fstream>

using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

namespace nj::runtime {
extern MMU *g_guest_mem;
extern GuestCpu *g_guest;
} // namespace nj::runtime

void gx_clear_rgba(unsigned char rgba[4]);

// Set by the host loop when F12 is pressed: the renderer then logs every draw
// of the next frame it renders (see LogDraw).
std::atomic<bool> g_gx_framelog_request{false};

namespace nj::runtime::hle { extern uint64_t g_tex_frame; }
namespace nj::runtime::gx {

static bool gx_trace_on() {
  static bool t = NJ_ENV("NJ_GXTRACE") != nullptr;
  return t;
}

// Programme genere (tev_shader_gen.cpp) et emplacements de ses uniformes, par role (-1 : le
// programme n'utilise pas cet uniforme).
enum UniformRole : int {
  kUProjMtx,
  kUTevColor,
  kUTevKColor,
  kUTGRow,
  kUTGPost,
  kUTMBank,
  kUIndMtx,
  kUTexSize,
  kUMatColor,
  kUAmbColor,
  kUChanCtrl,
  kULightCol,
  kULightPos,
  kULightDir,
  kULightCosAtt,
  kULightDistAtt,
  kUFogF,
  kUFogI,
  kUFogColor,
  kUAlphaRef,
  kUPosMtx,
  kUNrmMtx,
  kUniformCount
};
struct Program {
  GLuint id = 0;
  GLint loc[kUniformCount];
  GLint sampler[8];   // uTex0..7
  GLint texmtx[8];    // uTexMtx[0..7]
  Program() {
    std::fill(std::begin(loc), std::end(loc), -1);
    std::fill(std::begin(sampler), std::end(sampler), -1);
    std::fill(std::begin(texmtx), std::end(texmtx), -1);
  }
};
static std::unordered_map<uint64_t, Program> s_shader_cache;

uint64_t g_st_hash0 = 0;
uint64_t g_st_shaders = 0;

// Sommet du lot, tel que le GPU le lit (attributs : SetupVertexAttribs).
struct PackedVtx {
  float pos[3];       // position dans la vue
  float color[4];
  float uv[2];        // coordonnees de texture brutes (generateurs dans le nuanceur)
  float nrm[3];       // normale dans la vue
  float obj_pos[3];   // position d'objet (texgen POS)
  float obj_nrm[3];   // normale d'objet (texgen NRM)
  uint8_t tidx[8];    // index de matrice de texture de chaque generateur (par sommet)
};

static std::vector<PackedVtx> s_pend;
// Triangle indices into s_pend. GX quads, strips and fans used to be expanded
// by copying vertices (a strip of n vertices emitted 3(n-2) of them), which put
// put_vertex at the top of the render thread profile; each vertex is now
// emitted once and the triangles reference it.
static std::vector<uint32_t> s_indices;
// Le lot repart a vide (sommets et indices).
static inline void ClearBatch() {
  s_pend.clear();
  s_indices.clear();
}
// Mode OpenGL du lot en cours : un lot ne melange jamais deux types de primitive GX.
// Lignes (0xA8, 0xB0) et points (0xB8) : quadrillage de la mallette (Sscrn/ss_item_draw),
// traineees (esp09)... Epaisseur : BP 0x22 (SU_LPSIZE), en sixiemes de pixel d'EFB.
std::atomic<uint32_t> g_jr_gldraw{0}, g_jr_gluni{0}, g_jr_setup{0};   // journal de perf (phase A) : glDraw*, glUniform*, changements d'etat par image
static GLenum s_batch_glmode = GL_TRIANGLES;
static GLuint s_ebo = 0;
static float s_proj_mtx[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
static GLuint s_va = 0, s_vb = 0;
static Program s_active_shader;
// NJ_RPROF=1 : ou part le temps du rendu, par etape, moyenne par image toutes les 2 s
// (ligne [RPROF], logcat sur Android). NJ_RPROF=2 : plus un glFinish en fin d'image, pour
// separer le temps du processeur (appels) de celui du GPU (attente).
static uint64_t s_rp_xfb = 0;
namespace rprof {
enum { TOTAL, SETUP, EMIT, FLUSH, UPLOAD, DRAW, BP, COPY, XF, FINISH, N };
static uint64_t acc[N];
static uint64_t cnt_draws, cnt_flush, cnt_verts, cnt_copies, cnt_bp, cnt_setup;
static int env_mode() {
  static const int m = [] {
    const char *e = NJ_ENV("NJ_RPROF");
    return e ? std::atoi(e) : 0;
  }();
  return m;
}
// 27/10 : chronos pour le journal sur 1 image sur 8 seulement (une horloge lue par ecriture BP, ~13 600 par image, faussait la mesure
// sur S5). NJ_RPROF=<n> garde l'ancien comportement (toutes les images ; n >= 2 ajoute glFinish, intrusif).
static bool s_on = false;
static uint32_t s_frame_no = 0, s_sampled = 0;
static void frame_begin() {
  s_on = (++s_frame_no & 7u) == 0;
  if (s_on) ++s_sampled;
}
static int mode() {
  const int e = env_mode();
  return e ? e : (s_on ? 1 : 0);
}
static inline uint64_t now_us() {
  return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch()).count();
}
struct Scope {
  int k;
  uint64_t t0;
  explicit Scope(int kk) : k(kk), t0(mode() ? now_us() : 0) {}
  ~Scope() {
    if (mode()) acc[k] += now_us() - t0;
  }
};
static void report(uint64_t frames) {
  static const char *names[N] = {"total", "etats", "sommets", "vidage", "envoi", "dessin",
                                 "bp", "copies", "xf", "finish"};
  char line[1024];
  int o = std::snprintf(line, sizeof line, "[RPROF] images=%llu ms/image:",
                        (unsigned long long)frames);
  for (int i = 0; i < N; ++i)
    o += std::snprintf(line + o, sizeof line - o, " %s=%.1f", names[i],
                       frames ? acc[i] / 1000.0 / frames : 0.0);
  // Reste de la boucle : total moins les etapes mesurees (vidage contient envoi et dessin).
  const double autre = (double)acc[TOTAL] - acc[SETUP] - acc[EMIT] - acc[FLUSH] - acc[BP] -
                       acc[COPY] - acc[XF] - acc[FINISH];
  o += std::snprintf(line + o, sizeof line - o, " autre=%.1f", frames ? autre / 1000.0 / frames : 0.0);
  std::snprintf(line + o, sizeof line - o,
                " | par image : dessins=%llu lots=%llu sommets=%llu copies=%llu bp=%llu etats=%llu",
                (unsigned long long)(frames ? cnt_draws / frames : 0),
                (unsigned long long)(frames ? cnt_flush / frames : 0),
                (unsigned long long)(frames ? cnt_verts / frames : 0),
                (unsigned long long)(frames ? cnt_copies / frames : 0),
                (unsigned long long)(frames ? cnt_bp / frames : 0),
                (unsigned long long)(frames ? cnt_setup / frames : 0));
  NJ_LOG << line << std::endl;
  for (auto &a : acc) a = 0;
  cnt_draws = cnt_flush = cnt_verts = cnt_copies = cnt_bp = cnt_setup = 0;
}
}  // namespace rprof
// Journal de perf : temps par poste du rendu, en ms par image, depuis le dernier appel (remet a zero).
std::string rprof_journal_fields(double) {
  using namespace rprof;
  const double frames = (double)s_sampled;
  if (frames <= 0.0) return std::string();   // aucune image echantillonnee cette seconde
  char b[200];
  std::snprintf(b, sizeof b, " rt=%.0f rs=%.0f re=%.0f rf=%.0f ru=%.0f rd=%.0f rb=%.0f rk=%.0f rx=%.0f"_g,
                acc[TOTAL] / 1000.0 / frames, acc[SETUP] / 1000.0 / frames, acc[EMIT] / 1000.0 / frames, acc[FLUSH] / 1000.0 / frames,
                acc[UPLOAD] / 1000.0 / frames, acc[DRAW] / 1000.0 / frames, acc[BP] / 1000.0 / frames, acc[COPY] / 1000.0 / frames,
                acc[XF] / 1000.0 / frames);
  std::memset(acc, 0, sizeof acc);
  s_sampled = 0;
  return std::string(b);
}
// Jumelle haute resolution d'une copie EFB (resolution interne S > 1). La copie elle-meme reste a
// la densite GameCube : les flous, glows et profondeurs de champ naissent de sa reduction
// (docs/EFFETS_RESOLUTION.md, section 2). Mais les dessins en perspective qui la projettent
// (reflets de l'eau, miroirs, ombres) la liraient alors en basse definition, nettement plus
// floue que la scene autour. La jumelle, tiree de la meme image intermediaire que la copie
// (aucune lecture de l'EFB en plus), a S fois plus de texels ; elle ne sert qu'aux dessins en
// perspective. Les quads d'ecran (orthographiques : passes de flou, composition) gardent la
// copie fidele. NJ_NOCOPYTWIN=1 : pas de jumelle.
// Generation de la banque de matrices XF (0x000-0x0FF) : la banque vue par les generateurs de
// coordonnees (uTMBank) n'est reconstruite et renvoyee que quand elle change.
static uint64_t s_xf_bank_gen = 1;
#if !defined(NJ_GLES)
// Lectures de profondeur asynchrones (voir le traitement de BP 0x45) : un tampon GPU par lot de
// points, une barriere, et la reprise quand le GPU l'a passee.
struct PeekRead {
  GLuint tex = 0;            // profondeur des points, copiee cote GPU (N x 1, DEPTH_COMPONENT24)
  GLint cap = 0;
  GLsync fence = nullptr;
  int rank = 0;
  std::vector<std::pair<int, int>> pts;
  std::vector<uint32_t> ref;   // NJ_PEEKCHECK : lecture directe (synchrone) pour comparaison
};
static std::vector<PeekRead> s_peek_pending;
static std::vector<PeekRead> s_peek_free;
static GLuint s_peek_fbo = 0;
// Lectures de profondeur sans attente (01/10). Lire le tampon de profondeur de l'EFB par
// glReadPixels attend que le GPU ait tout dessine, meme vers un tampon GPU (pilote NVIDIA :
// 12 a 21 ms par image au chateau). Les points sont donc d'abord copies cote GPU dans une petite
// texture de profondeur (glCopyTexSubImage2D, en file comme un dessin), puis relus seulement quand
// la barriere posee derriere est passee : le GPU a fini, la lecture ne l'attend plus.
static void peek_async_issue(int rank, const std::vector<std::pair<int, int>> &pts) {
  PeekRead r;
  if (!s_peek_free.empty()) {
    r = std::move(s_peek_free.back());
    s_peek_free.pop_back();
  }
  const GLint n = (GLint)pts.size();
  if (!s_peek_fbo)
    glGenFramebuffers(1, &s_peek_fbo);
  if (!r.tex)
    glGenTextures(1, &r.tex);
  if (r.cap < n) {
    glBindTexture(GL_TEXTURE_2D, r.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, n, 1, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,
                 nullptr);
    nj_tex_filter(GL_NEAREST);
    r.cap = n;
  }
  r.rank = rank;
  r.pts = pts;
  const GLint S = nj::runtime::gfx::g_scale;
  // Copie cote GPU, point par point, depuis la profondeur du tampon lu (l'EFB) vers la texture
  // de profondeur (glCopyTexSubImage2D : ni test de decoupe, ni second tampon a completer).
  GLint prev_tex = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
  glBindTexture(GL_TEXTURE_2D, r.tex);
  for (GLint i = 0; i < n; ++i) {
    const GLint x = pts[i].first * S + S / 2, y = (480 - pts[i].second) * S - S / 2 - 1;
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, i, 0, x, y, 1, 1);
  }
  glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
  // NJ_PEEKCHECK=1 : relit aussi chaque point a l'ancienne (synchrone) ; la reprise compare.
  static const bool s_pk_check = NJ_ENV("NJ_PEEKCHECK") != nullptr;
  r.ref.clear();
  if (s_pk_check)
    for (GLint i = 0; i < n; ++i) {
      GLuint v = 0;
      glReadPixels(pts[i].first * S + S / 2, (480 - pts[i].second) * S - S / 2 - 1, 1, 1,
                   GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, &v);
      r.ref.push_back(v);
    }
  r.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  s_peek_pending.push_back(std::move(r));
}
// Reprend les lectures dont la barriere est passee (dans l'ordre).
static void peek_async_collect(bool wait) {
  while (!s_peek_pending.empty()) {
    PeekRead &r = s_peek_pending.front();
    const GLenum st = glClientWaitSync(r.fence, 0, wait ? 1000000000ull : 0);
    if (st != GL_ALREADY_SIGNALED && st != GL_CONDITION_SATISFIED)
      break;
    glDeleteSync(r.fence);
    r.fence = nullptr;
    const GLint n = (GLint)r.pts.size();
    std::vector<uint32_t> v((size_t)n, 0xFFFFFFFFu);
    GLint rd = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &rd);
    nj_fbo(GL_READ_FRAMEBUFFER, s_peek_fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, r.tex, 0);
    glReadBuffer(GL_NONE);   // profondeur seule : sans cela le tampon est incomplet en lecture
    glReadPixels(0, 0, n, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, v.data());
    if (!r.ref.empty()) {
      static uint64_t lots = 0, points = 0, diff = 0, proches = 0;
      ++lots;
      for (GLint i = 0; i < n && i < (GLint)r.ref.size(); ++i) {
        ++points;
        diff += (v[i] >> 8) != (r.ref[i] >> 8);   // 24 bits utiles
        proches += r.ref[i] != 0xFFFFFFFFu;
      }
      if (lots % 200 == 0)
        NJ_PRINTF("[PeekGPU] %llu lots, %llu points (%llu hors fond), %llu differents\n",
                    (unsigned long long)lots, (unsigned long long)points,
                    (unsigned long long)proches, (unsigned long long)diff);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)rd);
    std::vector<float> depth((size_t)n);
    for (GLint i = 0; i < n; ++i)
      depth[i] = (float)((double)v[i] / 4294967295.0);
    nj::runtime::gx::peek::render_store(r.rank, r.pts, depth);
    s_peek_free.push_back(std::move(r));
    s_peek_pending.erase(s_peek_pending.begin());
    wait = false;
  }
}
#endif
// NJ_BPPROF=1 : temps et nombre d'ecritures BP par registre, les 10 plus couteux toutes les
// 300 images (ligne [BPPROF], ms par image).
namespace bpprof {
static uint64_t us[256], cnt[256];
static bool on() {
  static const bool o = NJ_ENV("NJ_BPPROF") != nullptr;
  return o;
}
struct Scope {
  uint8_t r;
  std::chrono::steady_clock::time_point t0;
  explicit Scope(uint8_t rr) : r(rr) {
    if (on())
      t0 = std::chrono::steady_clock::now();
  }
  ~Scope() {
    if (!on())
      return;
    us[r] += (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
                 std::chrono::steady_clock::now() - t0).count();
    ++cnt[r];
    static unsigned frames = 0;
    if (r == 0x52 && (++frames % 300) == 0) {
      int idx[256];
      for (int i = 0; i < 256; ++i) idx[i] = i;
      std::sort(idx, idx + 256, [](int a, int b) { return us[a] > us[b]; });
      char line[1024];
      int o = std::snprintf(line, sizeof line, "[BPPROF] ms/image :");
      for (int k = 0; k < 10; ++k)
        o += std::snprintf(line + o, sizeof line - o, " %02X=%.2f(%llu)", idx[k], us[idx[k]] / 1000.0 / 300 * 11,
                           (unsigned long long)(cnt[idx[k]] * 11 / 300));
      NJ_PRINTF("%s\n", line);
      std::fflush(stdout);
      for (int i = 0; i < 256; ++i) us[i] = cnt[i] = 0;
    }
  }
};
}  // namespace bpprof
struct CopyTwin { GLuint tex = 0; GLint w = 0, h = 0; bool valid = false; };
static std::unordered_map<GLuint, CopyTwin> s_copy_twin;
static unsigned s_twin_realloc = 0, s_twin_made = 0;
// Le jeu copie souvent vers la meme adresse a des tailles differentes (chateau : ~30 % des copies
// changent de taille). Reallouer la jumelle (glTexImage2D de 17 a 45 Mo en 8x) a chaque fois
// forcait le pilote a attendre le GPU : chateau 8x a 0,1 i/s, 25 sans jumelles (01/10). On garde
// donc une texture par taille et par copie (4 au plus) et on passe de l'une a l'autre.
static std::unordered_map<GLuint, std::vector<CopyTwin>> s_twin_pool;
static CopyTwin &twin_for(GLuint tex, GLint w, GLint h) {
  CopyTwin &cur = s_copy_twin[tex];
  ++s_twin_made;
  if (cur.tex && cur.w == w && cur.h == h)
    return cur;
  auto &pool = s_twin_pool[tex];
  for (const CopyTwin &c : pool)
    if (c.w == w && c.h == h) {
      cur.tex = c.tex;
      cur.w = w;
      cur.h = h;
      return cur;
    }
  ++s_twin_realloc;
  if (pool.size() >= 4) {
    glDeleteTextures(1, &pool.front().tex);
    pool.erase(pool.begin());
  }
  CopyTwin n;
  glGenTextures(1, &n.tex);
  glBindTexture(GL_TEXTURE_2D, n.tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  nj_tex_filter(GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  n.w = w;
  n.h = h;
  pool.push_back(n);
  cur.tex = n.tex;
  cur.w = w;
  cur.h = h;
  return cur;
}
// NJ_TWINSTAT=1 : nombre de jumelles et memoire video qu'elles tiennent, toutes les 120 copies.
static void twin_stat() {
  static const bool on = NJ_ENV("NJ_TWINSTAT") != nullptr;
  static unsigned n = 0;
  if (!on || (++n % 120) != 0)
    return;
  uint64_t bytes = 0;
  for (const auto &kv : s_copy_twin)
    bytes += (uint64_t)kv.second.w * kv.second.h * 4;
  NJ_PRINTF("[TWIN] %zu jumelles, %.1f Mo, %u faites, %u reallocations (120 copies)\n",
              s_copy_twin.size(), bytes / 1048576.0, s_twin_made, s_twin_realloc);
  s_twin_made = s_twin_realloc = 0;
  std::fflush(stdout);
}

// Cache des uniformes : un dessin sur deux renvoyait au pilote les memes valeurs (matrices de
// texture a l'identite, lumieres, brouillard, couleurs TEV...). ~540 vidages par image et une
// quinzaine d'uniformes chacun : le fil interne du pilote en etait sature, et le profil du fil
// hote montrait Render en attente de lui dans ntdll. Ici on ne transmet que ce qui change,
// par programme et par emplacement (les valeurs d'un programme GL lui restent attachees).
#include <unordered_map>
#include <unordered_set>
// Reglages en direct de knobs.txt (build de dev) lus une fois par image (draw_frame) : knob()
// prend un verrou et cherche dans une table ; appele pour chaque uniforme et chaque dessin, il
// coutait ~10 000 appels par image sur le S5.
static struct FrameKnobs {
  int nounicache = 0, drawstart = 0, drawstop = -1, logdraw = -1, texnearest = 0;
} s_fk;
static GLuint s_cur_prog = 0;
static std::unordered_map<uint64_t, std::vector<uint32_t>> s_uni_cache;
static bool uni_changed(GLint loc, const void *data, size_t bytes) {
  static const bool off_env = NJ_ENV("NJ_NOUNICACHE") != nullptr;
  const bool off = off_env || s_fk.nounicache != 0;   // A/B
  if (off)
    return true;
  const uint64_t key = ((uint64_t)s_cur_prog << 32) | (uint32_t)loc;
  auto &v = s_uni_cache[key];
  const size_t n = bytes / 4;
  if (v.size() == n && std::memcmp(v.data(), data, bytes) == 0)
    return false;
  v.assign((const uint32_t *)data, (const uint32_t *)data + n);
  return true;
}
static void cu_UseProgram(GLuint id) {
  if (id != s_cur_prog) {
    glUseProgram(id);
    s_cur_prog = id;
  }
}
static void cu_Uniform1i(GLint loc, GLint a) {
  if (loc >= 0 && uni_changed(loc, &a, 4)) { glUniform1i(loc, a); g_jr_gluni.fetch_add(1, std::memory_order_relaxed); }
}
static void cu_Uniform2f(GLint loc, float a, float b) {
  const float v[2] = {a, b};
  if (loc >= 0 && uni_changed(loc, v, 8)) { glUniform2fv(loc, 1, v); g_jr_gluni.fetch_add(1, std::memory_order_relaxed); }
}
static void cu_Uniform4f(GLint loc, float a, float b, float c, float d) {
  const float v[4] = {a, b, c, d};
  if (loc >= 0 && uni_changed(loc, v, 16)) { glUniform4fv(loc, 1, v); g_jr_gluni.fetch_add(1, std::memory_order_relaxed); }
}
static void cu_Uniform4i(GLint loc, GLint a, GLint b, GLint c, GLint d) {
  const GLint v[4] = {a, b, c, d};
  if (loc >= 0 && uni_changed(loc, v, 16)) { glUniform4iv(loc, 1, v); g_jr_gluni.fetch_add(1, std::memory_order_relaxed); }
}
static void cu_Uniform4fv(GLint loc, GLsizei n, const float *v) {
  if (loc >= 0 && uni_changed(loc, v, (size_t)n * 16)) { glUniform4fv(loc, n, v); g_jr_gluni.fetch_add(1, std::memory_order_relaxed); }
}
static void cu_Uniform1iv(GLint loc, GLsizei n, const GLint *v) {
  if (loc >= 0 && uni_changed(loc, v, (size_t)n * 4)) { glUniform1iv(loc, n, v); g_jr_gluni.fetch_add(1, std::memory_order_relaxed); }
}
static void cu_UniformMatrix4fv(GLint loc, GLsizei n, GLboolean t, const float *v) {
  // La transposition entre dans la cle par le bit de poids fort de l'emplacement.
  if (loc >= 0 && uni_changed(loc | (t ? 0x40000000 : 0), v, (size_t)n * 64))
    glUniformMatrix4fv(loc, n, t, v);
}
static void cu_UniformMatrix3fv(GLint loc, GLsizei n, GLboolean t, const float *v) {
  if (loc >= 0 && uni_changed(loc | (t ? 0x40000000 : 0), v, (size_t)n * 36))
    glUniformMatrix3fv(loc, n, t, v);
}

// A generated shader that failed to compile or link used to be cached and
// bound anyway, so every draw using it vanished without a trace. Report the
// driver's log (first few failures, with the source of the first two).
static void ReportGlslFailure(const char *stage, GLuint obj, bool program,
                              const char *src) {
  static int failures = 0;
  if (failures >= 12)
    return;
  ++failures;
  char log[2048] = {0};
  GLsizei len = 0;
  if (program)
    glGetProgramInfoLog(obj, sizeof(log) - 1, &len, log);
  else
    glGetShaderInfoLog(obj, sizeof(log) - 1, &len, log);
  NJ_PRINTF("[GLSL] %s failed (#%d): %s\n", stage, failures,
         len ? log : "(no log)");
  if (src && failures <= 2)
    NJ_PRINTF("[GLSL] ---- source ----\n%s\n[GLSL] ---- end ----\n", src);
  fflush(stdout);
}

static uint32_t s_prog_cache_hits = 0;   // programmes rechargés depuis le cache disque (GL ES)
static uint32_t s_prog_cache_saved = 0, s_prog_cache_nolen = 0, s_prog_cache_nofile = 0;   // diagnostic : écrits, longueur de binaire nulle, fichier impossible

// Un etage de nuanceur compile ; l'echec est journalise avec la source.
static GLuint CompileStage(GLenum kind, const char *src, const char *what) {
  const GLuint sh = glCreateShader(kind);
  glShaderSource(sh, 1, &src, nullptr);
  glCompileShader(sh);
  GLint status = 0;
  glGetShaderiv(sh, GL_COMPILE_STATUS, &status);
  if (status == 0)
    ReportGlslFailure(what, sh, false, src);
  return sh;
}

// Cache disque des programmes lies (GL ES, 20/10/2026) : la compilation d'un nuanceur TEV coute des dizaines de ms au pilote (MuMu, telephones),
// et le budget par image (SelectProgram) saute les dessins tant que leur programme n'est pas pret : sur une installation neuve, des milliers de
// dessins manquent pendant des minutes (journal « gl f=1800 skips=2662 » ; defaut du point d'apparition au village). Le binaire du programme, une fois lie,
// est range dans <donnees>/shaders/<pilote>/<empreinte>.bin et recharge aux lancements suivants sans aucune compilation. Le dossier porte l'empreinte
// du pilote (GL_RENDERER + GL_VERSION) : un autre pilote ou une mise a jour ne rechargent pas un binaire etranger.
#ifdef NJ_GLES
static std::string prog_cache_dir() {
  static const std::string dir = [] {
    const char *r = (const char *)glGetString(GL_RENDERER), *v = (const char *)glGetString(GL_VERSION);
    uint64_t h = 1469598103934665603ull;
    for (const char *p : {r ? r : "", v ? v : ""})
      for (; *p; ++p) h = (h ^ (uint8_t)*p) * 1099511628211ull;
    char b[32];
    std::snprintf(b, sizeof b, "shaders/%016llx", (unsigned long long)h);
    return nj::writable_path(b);
  }();
  return dir;
}
static uint64_t prog_key(const char *vs, const char *fs) {
  uint64_t h = 1469598103934665603ull;
  for (const char *p : {vs, fs}) {
    for (; *p; ++p) h = (h ^ (uint8_t)*p) * 1099511628211ull;
    h = (h ^ 0xFF) * 1099511628211ull;
  }
  return h;
}
static GLuint load_cached_program(uint64_t key) {
  char name[40];
  std::snprintf(name, sizeof name, "/%016llx.bin", (unsigned long long)key);
  std::ifstream f(prog_cache_dir() + name, std::ios::binary);
  if (!f) return 0;
  uint32_t fmt = 0;
  f.read((char *)&fmt, 4);
  std::vector<char> bin((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (!f && !f.eof()) return 0;
  if (bin.empty()) return 0;
  const GLuint prog = glCreateProgram();
  glProgramBinary(prog, fmt, bin.data(), (GLsizei)bin.size());
  GLint linked = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &linked);
  if (!linked) {
    glDeleteProgram(prog);
    return 0;
  }
  return prog;
}
static void save_cached_program(uint64_t key, GLuint prog) {
  GLint len = 0;
  glGetProgramiv(prog, GL_PROGRAM_BINARY_LENGTH, &len);
  if (len <= 0 || len > (8 << 20)) {
    ++s_prog_cache_nolen;
    return;
  }
  std::vector<char> bin((size_t)len);
  GLsizei got = 0;
  GLenum fmt = 0;
  glGetProgramBinary(prog, len, &got, &fmt, bin.data());
  if (got <= 0) {
    ++s_prog_cache_nolen;
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(prog_cache_dir(), ec);
  char name[40];
  std::snprintf(name, sizeof name, "/%016llx.bin", (unsigned long long)key);
  std::ofstream f(prog_cache_dir() + name, std::ios::binary | std::ios::trunc);
  if (!f) {
    ++s_prog_cache_nofile;
    return;
  }
  const uint32_t fm = (uint32_t)fmt;
  f.write((const char *)&fm, 4);
  f.write(bin.data(), got);
  ++s_prog_cache_saved;
}
#endif

// Programme a deux etages ; les objets d'etage sont rendus au pilote une fois lies.
static GLuint link_program(const char *vs, const char *fs) {
#ifdef NJ_GLES
  const uint64_t cache_key = prog_key(vs, fs);
  if (const GLuint cached = load_cached_program(cache_key)) {
    ++s_prog_cache_hits;
    return cached;
  }
#endif
  GLuint stages[2] = {0, 0}, prog = 0;
  GLint linked = 0;
  // Deux essais : le pilote Adreno 420 du S5 rate parfois une compilation sans aucun journal (coeur natif,
  // 12/11 : le premier programme, celui de tout l'ecran titre) ; les memes sources passent au second essai.
  for (int attempt = 0; attempt < 2 && linked == 0; ++attempt) {
    if (prog) {
      for (GLuint st : stages) glDeleteShader(st);
      glDeleteProgram(prog);
      while (glGetError() != GL_NO_ERROR) {}
      NJ_PRINTF("[GLSL] nouvel essai du programme\n");
    }
    stages[0] = CompileStage(GL_VERTEX_SHADER, vs, "vertex shader compile");
    stages[1] = CompileStage(GL_FRAGMENT_SHADER, fs, "fragment shader compile");
    prog = glCreateProgram();
#ifdef NJ_GLES
    glProgramParameteri(prog, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
#endif
    for (GLuint st : stages)
      glAttachShader(prog, st);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (linked == 0)
      ReportGlslFailure("program link", prog, true, nullptr);
  }
#ifdef NJ_GLES
  if (linked != 0)
    save_cached_program(cache_key, prog);
#endif
  for (GLuint st : stages)
    glDeleteShader(st);
  return prog;
}

// Combien de vidages de lot par image, et pourquoi.
//
// Angle mort cree le 2026-09-21 : un vidage sur changement de matrice a ete
// ajoute sans jamais compter ce qu'il declenche. Quand une tentative
// d'optimisation a fait tripler le temps de rendu, rien ne permettait de dire
// si c'etait la cause. Un compteur qu'on n'a pas est une panne qu'on ne peut
// pas diagnostiquer.
std::atomic<uint64_t> g_prof_flush{0};      // vidages reels (lot non vide)
std::atomic<uint64_t> g_prof_flush_verts{0};
std::atomic<uint64_t> g_prof_flush_prim{0}; // cause : changement de primitive
std::atomic<uint64_t> g_prof_flush_mtx{0};  // cause : changement de matrice

// Geometrie residente (NJ_RESIDENT=0 : coupee).
//
// Les sommets decodes des dessins transformes par le nuanceur ne dependent pas de la camera : le
// meme decor produit les memes sommets d'une image a l'autre. Les reemettre et les renvoyer a
// chaque image coutait 330 ms sur 580 au Galaxy S5 (29/09). Un lot dont tous les dessins ont une
// empreinte de contenu (GxOp::geom_hash, calculee par le decodeur) est reconnu par la suite
// de ces empreintes : la premiere fois il est dessine normalement et range dans un tampon GPU
// permanent (verse en fin d'image, un seul envoi) ; ensuite il est dessine depuis ce tampon, sans
// emission ni envoi. Les sommets sont ceux de la premiere fois a l'octet pres : meme image.
static void push_prim(const GxOp &cmd);
// Attributs du sommet de lot, dans l'ordre des emplacements du nuanceur de sommets : position,
// couleur, coordonnees, normale (espace camera), position et normale d'objet (generateurs de
// coordonnees), puis les huit indices de matrices de texture en deux paquets de quatre octets.
static void SetupVertexAttribs() {
  struct Attr {
    GLint size;
    GLenum type;
    size_t offset;
  };
  static const Attr kAttrs[] = {
      {3, GL_FLOAT, offsetof(PackedVtx, pos)},     {4, GL_FLOAT, offsetof(PackedVtx, color)},
      {2, GL_FLOAT, offsetof(PackedVtx, uv)},      {3, GL_FLOAT, offsetof(PackedVtx, nrm)},
      {3, GL_FLOAT, offsetof(PackedVtx, obj_pos)}, {3, GL_FLOAT, offsetof(PackedVtx, obj_nrm)},
      {4, GL_UNSIGNED_BYTE, offsetof(PackedVtx, tidx)},
      {4, GL_UNSIGNED_BYTE, offsetof(PackedVtx, tidx) + 4},
  };
  GLuint slot = 0;
  for (const Attr &a : kAttrs) {
    glEnableVertexAttribArray(slot);
    glVertexAttribPointer(slot, a.size, a.type, GL_FALSE, sizeof(PackedVtx),
                          reinterpret_cast<const void *>(a.offset));
    ++slot;
  }
}
// Invalidation de la destination d'un blit qui la reecrit en entier (copies EFB : texture de la
// copie, jumelles, niveaux de reduction, profondeur). Sur un GPU a tuiles (Adreno), l'ancien contenu
// n'est alors plus recharge en memoire de tuile avant le blit : aucun effet sur l'image, la zone
// etant entierement ecrite. NJ_NOINVAL=1 : sans (A/B). GL ES seulement.
static inline void nj_inval_draw(GLenum att) {
#ifdef NJ_GLES
  static const bool off = NJ_ENV("NJ_NOINVAL") != nullptr;
  if (!off && !(nj::runtime::gfx::g_compat_mask & 1)) glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 1, &att);
#else
  (void)att;
#endif
}

namespace resident {
static bool on() {
  static const bool v = [] {
    const char *e = NJ_ENV("NJ_RESIDENT");
    return !(e && e[0] == '0') && !(nj::runtime::gfx::g_compat_mask & 4);
  }();
  return v;
}
struct Entry {
  uint32_t vfirst, vcount, ifirst, icount;
  bool committed;
};
static std::unordered_map<uint64_t, Entry> map;
static std::unordered_set<uint64_t> seen;
static GLuint vao = 0, vbo = 0, ebo = 0;
static size_t vused = 0, iused = 0;
static const size_t kVerts = 600000;       // ~48 Mo
static const size_t kIdx = kVerts * 3;
// Lot en attente : les dessins ne sont emis qu'au vidage, une fois le lot reconnu ou non.
static std::vector<const GxOp *> pend;
static uint64_t pend_hash = 0x84222325CBF29CE4ull;
static bool pend_ok = true;
// Entrees creees pendant l'image, versees en fin d'image (Render).
static std::vector<PackedVtx> stage_v;
static std::vector<uint32_t> stage_i;
static size_t stage_v0 = 0, stage_i0 = 0;   // position de stage_v / stage_i dans le tampon
static std::vector<uint64_t> stage_keys;
static uint64_t hits = 0, hit_verts = 0, misses = 0;
static void reset_pending() {
  pend.clear();
  pend_hash = 0x84222325CBF29CE4ull;
  pend_ok = true;
}
static void ensure() {
  if (vao) return;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, kVerts * sizeof(PackedVtx), nullptr, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, kIdx * sizeof(uint32_t), nullptr, GL_STATIC_DRAW);
  SetupVertexAttribs();
}
// Fin d'image : verser les nouvelles entrees (un envoi pour les sommets, un pour les indices).
static void commit() {
  if (stage_keys.empty()) return;
  ensure();
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  // Versement par copie cote GPU (defaut) : les octets partent dans un tampon de transit neuf
  // (glBufferData, stockage frais : aucune attente), puis glCopyBufferSubData les place dans le
  // grand tampon, dans le flux de commandes. glBufferSubData directement dans le grand tampon,
  // que le GPU lit encore, faisait attendre le pilote de l'Adreno 420 la fin de l'image
  // precedente (EsxGfxMem::WaitOnTimestamp) : ~30 % des echantillons de pile du fil de rendu au
  // Galaxy S5 (30/09), le CPU et le GPU travaillaient chacun leur tour.
  // NJ_RESCOMMIT=subdata : l'ancien versement, pour comparer.
  static const bool s_subdata = [] {
    const char *e = NJ_ENV("NJ_RESCOMMIT");
    return e && std::strcmp(e, "subdata") == 0;
  }();
  const GLsizeiptr vbytes = (GLsizeiptr)(stage_v.size() * sizeof(PackedVtx));
  const GLsizeiptr ibytes = (GLsizeiptr)(stage_i.size() * sizeof(uint32_t));
  if (s_subdata) {
    glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(stage_v0 * sizeof(PackedVtx)), vbytes,
                    stage_v.data());
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)(stage_i0 * sizeof(uint32_t)), ibytes,
                    stage_i.data());
  } else {
    static GLuint s_transit = 0;
    if (!s_transit) glGenBuffers(1, &s_transit);
    glBindBuffer(GL_COPY_READ_BUFFER, s_transit);
    glBufferData(GL_COPY_READ_BUFFER, vbytes, stage_v.data(), GL_STREAM_DRAW);
    glBindBuffer(GL_COPY_WRITE_BUFFER, vbo);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0,
                        (GLintptr)(stage_v0 * sizeof(PackedVtx)), vbytes);
    glBufferData(GL_COPY_READ_BUFFER, ibytes, stage_i.data(), GL_STREAM_DRAW);
    glBindBuffer(GL_COPY_WRITE_BUFFER, ebo);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0,
                        (GLintptr)(stage_i0 * sizeof(uint32_t)), ibytes);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
  }
  for (uint64_t k : stage_keys) {
    auto it = map.find(k);
    if (it != map.end()) it->second.committed = true;
  }
  stage_v.clear();
  stage_i.clear();
  stage_keys.clear();
  stage_v0 = vused;
  stage_i0 = iused;
}
// Ranger le lot qui vient d'etre emis (s_pend / s_indices relatifs) sous la cle `key`.
static void store(uint64_t key) {
  const size_t nv = s_pend.size(), ni = s_indices.size();
  if (vused + nv > kVerts || iused + ni > kIdx) {
    // Plein : tout recommencer (une salle nouvelle). Les versements en attente partent aussi.
    map.clear();
    seen.clear();
    stage_v.clear();
    stage_i.clear();
    stage_keys.clear();
    vused = iused = stage_v0 = stage_i0 = 0;
    if (nv > kVerts || ni > kIdx) return;
    if (vao) {   // nouveau stockage : le GPU peut encore lire l'ancien
      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glBufferData(GL_ARRAY_BUFFER, kVerts * sizeof(PackedVtx), nullptr, GL_STATIC_DRAW);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, kIdx * sizeof(uint32_t), nullptr, GL_STATIC_DRAW);
    }
  }
  Entry e{(uint32_t)vused, (uint32_t)nv, (uint32_t)iused, (uint32_t)ni, false};
  stage_v.insert(stage_v.end(), s_pend.begin(), s_pend.end());
  for (uint32_t ix : s_indices) stage_i.push_back(ix + (uint32_t)vused);
  vused += nv;
  iused += ni;
  map.emplace(key, e);
  stage_keys.push_back(key);
}
}  // namespace resident

// Tableau de sommets du lot et ses deux tampons (sommets, indices), crees au premier vidage ; le
// tampon d'indices est retenu par le tableau de sommets.
static void BindBatchObjects() {
  if (s_va == 0) {
    GLuint bufs[2];
    glGenVertexArrays(1, &s_va);
    glGenBuffers(2, bufs);
    s_vb = bufs[0];
    s_ebo = bufs[1];
    glBindVertexArray(s_va);
    glBindBuffer(GL_ARRAY_BUFFER, s_vb);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_ebo);
    SetupVertexAttribs();
  } else {
    glBindVertexArray(s_va);
  }
  glBindBuffer(GL_ARRAY_BUFFER, s_vb);
}

// ---------------------------------------------------------------- sommets tires par le nuanceur (NJ_PULL=1, A1)
// documentation interne Le lot garde, par dessin, une fiche de 16 mots, les octets bruts du
// flux d'index (gros-boutistes) et ceux des zones de tableaux copiees a l'analyse (GxOp::pull_blob) ; au vidage, une
// zone d'un anneau R32UI lu par texelFetch, des indices (dessin << 16 | sommet) et UN glDrawElements.
// Modele : Aurora 2026 (lib/gx/command_processor.cpp push_gx_draw). Banc A0 : 0 pixel d'ecart sur l'Adreno 420.
}  // namespace nj::runtime::gx
#if defined(NJ_GLES) && !defined(__APPLE__)
#include <EGL/egl.h>
#endif
namespace nj::runtime::gx {
#if defined(NJ_GLES) && !defined(__APPLE__)
// NJ_GPUTIME=1 (14/11) : temps GPU de draw_frame (GL_EXT_disjoint_timer_query, present sur l'Adreno 420), moyenne
// sur 30 images, ligne [GPUTIME]. Sert a trancher : la carte graphique est-elle le goulot au chateau ? Quatre
// requetes en anneau, lues quand elles sont pretes (jamais d'attente).
namespace gputime {
typedef void (GL_APIENTRYP PGen)(GLsizei, GLuint *);
typedef void (GL_APIENTRYP PBeg)(GLenum, GLuint);
typedef void (GL_APIENTRYP PEnd)(GLenum);
typedef void (GL_APIENTRYP PGetU)(GLuint, GLenum, GLuint *);
typedef void (GL_APIENTRYP PGet64)(GLuint, GLenum, GLuint64 *);
static PGen s_gen; static PBeg s_beg; static PEnd s_end; static PGetU s_getu; static PGet64 s_get64;
static GLuint s_q[4];
static int s_head = 0, s_inflight = 0, s_n = 0, s_disj = 0;
static bool s_active = false;
static double s_acc = 0, s_max = 0;
static bool ready() {
  static const int st = [] {
    if (!NJ_ENV("NJ_GPUTIME")) return 0;
    s_gen = (PGen)eglGetProcAddress("glGenQueriesEXT");
    s_beg = (PBeg)eglGetProcAddress("glBeginQueryEXT");
    s_end = (PEnd)eglGetProcAddress("glEndQueryEXT");
    s_getu = (PGetU)eglGetProcAddress("glGetQueryObjectuivEXT");
    s_get64 = (PGet64)eglGetProcAddress("glGetQueryObjectui64vEXT");
    if (!s_gen || !s_beg || !s_end || !s_getu || !s_get64) { NJ_LOG << "[GPUTIME] extension absente" << std::endl; return 0; }
    s_gen(4, s_q);
    return 1;
  }();
  return st == 1;
}
static void collect() {
  while (s_inflight) {
    const GLuint q = s_q[(s_head - s_inflight) & 3];
    GLuint avail = 0;
    s_getu(q, 0x8867 /*QUERY_RESULT_AVAILABLE*/, &avail);
    if (!avail) break;
    GLuint64 ns = 0;
    s_get64(q, 0x8866 /*QUERY_RESULT*/, &ns);
    GLint disj = 0;
    glGetIntegerv(0x8FBB /*GPU_DISJOINT*/, &disj);
    --s_inflight;
    if (disj) { ++s_disj; continue; }
    const double ms = ns / 1e6;
    s_acc += ms; if (ms > s_max) s_max = ms;
    if (++s_n == 30) {
      char b[160];
      std::snprintf(b, sizeof b, "[GPUTIME] GPU (draw_frame) %.1f ms par image, max %.1f (30 images, %d ecartees)", s_acc / 30, s_max, s_disj);
      NJ_LOG << b << std::endl;
      s_acc = s_max = 0; s_n = s_disj = 0;
    }
  }
}
struct Scope {
  Scope() {
    if (!ready()) return;
    collect();
    if (s_inflight < 4) { s_beg(0x88BF /*TIME_ELAPSED*/, s_q[s_head]); s_active = true; }
  }
  ~Scope() {
    if (!s_active) return;
    s_end(0x88BF);
    s_head = (s_head + 1) & 3; ++s_inflight; s_active = false;
  }
};
}  // namespace gputime
#endif
extern uint16_t g_vs_pull_variant;
bool PullOn();
namespace pull {
#ifndef GL_TEXTURE_BUFFER
#define GL_TEXTURE_BUFFER 0x8C2A
#endif
typedef void (*PfnTexBuffer)(GLenum, GLenum, GLuint);
static PfnTexBuffer s_texbuf = nullptr;
static bool s_ready = false, s_failed = false;
static const int kUnit = 15;   // unite de texture du tampon (les etages TEV prennent 0-7)

struct Ring {   // anneau ecrit sans synchronisation, protege par une barriere par huitieme
  GLuint buf = 0;
  size_t size = 0, cur = 0;
  int chunk_now = 0;
  GLsync fence[8] = {};
  void init(size_t n) {
    size = n;
    glGenBuffers(1, &buf);
    glBindBuffer(GL_COPY_WRITE_BUFFER, buf);
    glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)n, nullptr, GL_STREAM_DRAW);
  }
  void enter(int c) {   // passage dans le huitieme c : barriere pour celui qu'on quitte, attente de celui qu'on prend
    if (fence[chunk_now]) glDeleteSync(fence[chunk_now]);
    fence[chunk_now] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (fence[c]) {
      glClientWaitSync(fence[c], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ull);
      glDeleteSync(fence[c]);
      fence[c] = nullptr;
    }
    chunk_now = c;
  }
  // Decalage d'une zone de n octets, ou SIZE_MAX si n depasse un huitieme.
  size_t alloc(size_t n) {
    const size_t ch = size / 8;
    n = (n + 15) & ~(size_t)15;
    if (n > ch) return SIZE_MAX;
    if (cur + n > size) { cur = 0; enter(0); }
    const int c_end = (int)((cur + n - 1) / ch);
    while (chunk_now != c_end) enter((chunk_now + 1) & 7);
    const size_t off = cur;
    cur += n;
    return off;
  }
  void write(size_t off, const void *a, size_t na, const void *b = nullptr, size_t nb = 0) {
    glBindBuffer(GL_COPY_WRITE_BUFFER, buf);
    void *p = glMapBufferRange(GL_COPY_WRITE_BUFFER, (GLintptr)off, (GLsizeiptr)(na + nb),
                               GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
    if (p) {
      std::memcpy(p, a, na);
      if (nb) std::memcpy((uint8_t *)p + na, b, nb);
      glUnmapBuffer(GL_COPY_WRITE_BUFFER);
    }
  }
};
static Ring s_data, s_idx;
static GLuint s_vao = 0, s_tex = 0;
static std::vector<uint32_t> rec, idx;
static std::vector<uint8_t> dat;
static uint32_t ndraw = 0;
static bool open = false;
uint64_t st_draws = 0, st_bytes = 0, st_flush = 0, st_lost = 0;

static bool on() {
#if defined(__APPLE__)
  return false;   // GL ES 3.0 d'iOS : pas de texture tampon
#else
  return PullOn() && !s_failed;
#endif
}
static bool ensure() {
  if (s_ready) return true;
  if (s_failed) return false;
#if defined(NJ_GLES) && !defined(__APPLE__)
  s_texbuf = (PfnTexBuffer)eglGetProcAddress("glTexBuffer");
  if (!s_texbuf) s_texbuf = (PfnTexBuffer)eglGetProcAddress("glTexBufferEXT");
#elif !defined(NJ_GLES)
  s_texbuf = (PfnTexBuffer)glTexBuffer;
#endif
  if (!s_texbuf) { s_failed = true; NJ_LOG << "[PULL] glTexBuffer absent : ancien chemin" << std::endl; return false; }
  s_data.init(32u << 20);
  s_idx.init(16u << 20);
  glGenVertexArrays(1, &s_vao);
  glBindVertexArray(s_vao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_idx.buf);
  glGenTextures(1, &s_tex);
  glActiveTexture(GL_TEXTURE0 + kUnit);
  glBindTexture(GL_TEXTURE_BUFFER, s_tex);
  s_texbuf(GL_TEXTURE_BUFFER, GL_R32UI, s_data.buf);
  glActiveTexture(GL_TEXTURE0);
  s_ready = true;
  return true;
}
// Variante du nuanceur pour ce dessin (0 : pas tire).
static uint16_t variant(const GxOp &c) {
  if (!c.pull || !c.raw) return 0;
  const VtxFormat &f = c.raw->vat;
  uint8_t v = 0x80;
  if (f.nrm.mode != AttrMode::None) v |= 1 | (f.nrm.comp == CompType::S8 ? 8 : 0);
  if (f.clr[0].mode != AttrMode::None) v |= 2 | (uint8_t)(((int)f.clr[0].comp & 7) << 4);
  if (f.tex[0].mode != AttrMode::None) v |= 4;
  return v;
}
// Copies de tableaux deja envoyees pendant l'image : une seule fois par image (adresse dans l'anneau).
// La table garde une reference sur chaque copie jusqu'a la fin de l'image : une adresse liberee puis reprise par une
// autre copie dans la meme image ne peut pas rendre une vieille zone.
static std::unordered_map<const void *, std::pair<std::shared_ptr<const std::vector<uint8_t>>, uint32_t>> s_sent;
static void frame_end() { s_sent.clear(); }
static bool send_blob(const std::shared_ptr<const std::vector<uint8_t>> &b, uint32_t &where) {
  auto it = s_sent.find(b.get());
  if (it != s_sent.end()) { where = it->second.second; return true; }
  const size_t off = s_data.alloc(b->size());
  if (off == SIZE_MAX) return false;
  s_data.write(off, b->data(), b->size());
  st_bytes += b->size();
  s_sent.emplace(b.get(), std::make_pair(b, (uint32_t)off));
  where = (uint32_t)off;
  return true;
}
// Envoi groupe (14/11) : tous les tableaux que les dessins de l'image vont lire partent en UN glMapBufferRange par
// huitieme d'anneau, avant le premier dessin, au lieu d'un par tableau. Banc S5 (NJ_GLBENCH) : un map non
// synchronise coute ~40 us sur l'Adreno 420, un dessin 4,4 us. send_blob les retrouve ensuite dans s_sent.
// NJ_PRELOAD=1 active l'envoi groupe (aucun gain mesure le 14/11) ; coupe par defaut : un envoi par tableau.
static void preload(const std::vector<GxOp> &cmds) {
  static const bool off_env = [] { const char *e = NJ_ENV("NJ_PRELOAD"); return !(e && e[0] == '1'); }();
  if (off_env || !on() || !ensure()) return;
  using Blob = std::shared_ptr<const std::vector<uint8_t>>;
  static std::vector<std::pair<const Blob *, size_t>> todo;
  static std::unordered_set<const void *> seen;
  todo.clear();
  seen.clear();
  const size_t ch = s_data.size / 8;
  size_t total = 0;
  auto send_group = [&]() {
    if (todo.empty()) return;
    const size_t off = s_data.alloc(total);
    if (off != SIZE_MAX) {
      glBindBuffer(GL_COPY_WRITE_BUFFER, s_data.buf);
      auto *p = (uint8_t *)glMapBufferRange(GL_COPY_WRITE_BUFFER, (GLintptr)off, (GLsizeiptr)total,
                                            GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
      if (p) {
        size_t o = 0;
        for (auto &t : todo) {
          const Blob &b = *t.first;
          std::memcpy(p + o, b->data(), b->size());
          s_sent.emplace(b.get(), std::make_pair(b, (uint32_t)(off + o)));
          st_bytes += b->size();
          o += t.second;
        }
        glUnmapBuffer(GL_COPY_WRITE_BUFFER);
      }
    }
    todo.clear();
    total = 0;
  };
  for (const GxOp &c : cmds) {
    if (c.type != GxOpKind::DrawPrimitive || !(c.pull || c.list)) continue;
    for (int a = 0; a < 4; ++a) {
      const Blob &b = c.pull_blob[a];
      if (!b || s_sent.count(b.get()) || !seen.insert(b.get()).second) continue;
      const size_t n = (b->size() + 15) & ~(size_t)15;
      if (n > ch) continue;   // trop gros pour un huitieme : send_blob le refusera comme avant
      if (total + n > ch) send_group();
      todo.emplace_back(&b, n);
      total += n;
    }
  }
  send_group();
}
static void append(const GxOp &c) {
  const RawDraw &r = *c.raw;
  const VtxFormat &f = r.vat;
  const bool hn = f.nrm.mode != AttrMode::None, hc = f.clr[0].mode != AttrMode::None, ht = f.tex[0].mode != AttrMode::None;
  const uint32_t vsz = 2u * (1u + hn + hc + ht), count = r.count;
  if (!r.bytes || r.bytes->size() < (size_t)vsz * count || !c.pull_blob[0] || count == 0 || !ensure()) return;
  const uint8_t prim = c.prim_type;
  if (prim < 0x80 || prim > 0xB8 || (prim & 7) != 0 || prim == 0x88) return;
  static const int arr[4] = {0, 1, 2, 4};
  uint32_t w[16] = {};
  for (int a = 0; a < 4; ++a) {   // tableaux : adresse absolue dans l'anneau
    uint32_t at = 0;
    if (c.pull_blob[a] && !send_blob(c.pull_blob[a], at)) { ++st_lost; return; }
    w[2 + 2 * a] = (uint32_t)((int32_t)at + c.pull_bias[a]);
    w[3 + 2 * a] = r.arr.stride[arr[a]];
  }
  const uint32_t d = ndraw++;
  // flux d'index du lot (decalage relatif, complete au vidage), aligne sur 4 octets
  const uint32_t so = (uint32_t)dat.size();
  dat.insert(dat.end(), r.bytes->data(), r.bytes->data() + (size_t)vsz * count);
  dat.resize((dat.size() + 3) & ~(size_t)3, 0);
  w[0] = so;
  w[1] = vsz;
  const float ip = 1.0f / (float)(1 << f.pos.frac), it = 1.0f / (float)(1 << f.tex[0].frac);
  std::memcpy(&w[10], &ip, 4);
  std::memcpy(&w[11], &it, 4);
  std::memcpy(&w[12], r.mtx_idx_tex, 8);
  rec.insert(rec.end(), w, w + 16);
  const uint32_t b = d << 16;
  auto tri = [&](uint32_t x, uint32_t y, uint32_t z) { idx.push_back(b + x); idx.push_back(b + y); idx.push_back(b + z); };
  switch (prim) {   // memes motifs que push_prim
  case 0x80: for (uint32_t q = 0; q + 3 < count; q += 4) { tri(q, q + 1, q + 2); tri(q + 2, q + 3, q); } break;
  case 0x90: for (uint32_t t = 0; t + 2 < count; t += 3) tri(t, t + 1, t + 2); break;
  case 0x98: for (uint32_t k = 2; k < count; ++k) { if (k % 2) tri(k - 1, k - 2, k); else tri(k - 2, k - 1, k); } break;
  case 0xA0: for (uint32_t k = 2; k < count; ++k) tri(0, k - 1, k); break;
  case 0xA8: for (uint32_t k = 0; k + 1 < count; k += 2) { idx.push_back(b + k); idx.push_back(b + k + 1); } break;
  case 0xB0: for (uint32_t k = 1; k < count; ++k) { idx.push_back(b + k - 1); idx.push_back(b + k); } break;
  default: for (uint32_t k = 0; k < count; ++k) idx.push_back(b + k); break;
  }
  ++st_draws;
  open = true;
}
static void reset() { rec.clear(); idx.clear(); dat.clear(); ndraw = 0; open = false; }

// ---- listes compilees (V2) : flux d'index et indices residents, une passe d'envoi par liste (generation s_gen)
static GLuint s_res_buf = 0, s_res_idx = 0, s_res_tex = 0, s_res_vao = 0;
static size_t s_res_used = 0, s_res_iused = 0;
static uint32_t s_gen = 1;
static const size_t kResBytes = 16u << 20, kResIdx = 24u << 20;
uint64_t st_lists = 0, st_list_up = 0;
static bool res_ensure() {
  if (s_res_vao) return true;
  if (!ensure()) return false;
  glGenBuffers(1, &s_res_buf);
  glBindBuffer(GL_COPY_WRITE_BUFFER, s_res_buf);
  glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)kResBytes, nullptr, GL_STATIC_DRAW);
  glGenBuffers(1, &s_res_idx);
  glBindBuffer(GL_COPY_WRITE_BUFFER, s_res_idx);
  glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)kResIdx, nullptr, GL_STATIC_DRAW);
  glGenVertexArrays(1, &s_res_vao);
  glBindVertexArray(s_res_vao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_res_idx);
  glGenTextures(1, &s_res_tex);
  glActiveTexture(GL_TEXTURE0 + kUnit - 1);
  glBindTexture(GL_TEXTURE_BUFFER, s_res_tex);
  s_texbuf(GL_TEXTURE_BUFFER, GL_R32UI, s_res_buf);
  glActiveTexture(GL_TEXTURE0);
  return true;
}
static void res_put(GLuint buf, size_t off, const void *p, size_t n) {
  glBindBuffer(GL_COPY_WRITE_BUFFER, buf);
  void *d = glMapBufferRange(GL_COPY_WRITE_BUFFER, (GLintptr)off, (GLsizeiptr)n, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
  if (d) { std::memcpy(d, p, n); glUnmapBuffer(GL_COPY_WRITE_BUFFER); }
}
static void draw_list(const GxOp &c, GLuint prog, GLenum mode) {
  const ListMesh &m = *c.list;
  if (!res_ensure()) return;
  if (m.gpu_gen != s_gen) {   // premiere fois (ou magasin repris) : envoi unique du flux et des indices
    const size_t sb = (m.stream.size() + 15) & ~(size_t)15, ib = m.idx.size() * 4;
    if (s_res_used + sb > kResBytes || s_res_iused + ib > kResIdx) {   // plein : on repart a vide (stockage neuf)
      ++s_gen;
      s_res_used = s_res_iused = 0;
      glBindBuffer(GL_COPY_WRITE_BUFFER, s_res_buf);
      glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)kResBytes, nullptr, GL_STATIC_DRAW);
      glBindBuffer(GL_COPY_WRITE_BUFFER, s_res_idx);
      glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)kResIdx, nullptr, GL_STATIC_DRAW);
      if (sb > kResBytes || ib > kResIdx) return;
    }
    res_put(s_res_buf, s_res_used, m.stream.data(), m.stream.size());
    res_put(s_res_idx, s_res_iused, m.idx.data(), ib);
    m.gpu_stream = (uint32_t)s_res_used;
    m.gpu_idx = (uint32_t)s_res_iused;
    m.gpu_gen = s_gen;
    s_res_used += sb;
    s_res_iused += (ib + 15) & ~(size_t)15;
    ++st_list_up;
  }
  const RawDraw &r = *c.raw;
  const VtxFormat &f = r.vat;
  static const int arr[4] = {0, 1, 2, 4};
  GLint ao[4] = {0, 0, 0, 0}, as[4] = {0, 0, 0, 0};
  for (int a = 0; a < 4; ++a) {
    if (!c.pull_blob[a]) continue;
    uint32_t at = 0;
    if (!send_blob(c.pull_blob[a], at)) { ++st_lost; return; }
    ao[a] = (GLint)at + c.pull_bias[a];
    as[a] = (GLint)r.arr.stride[arr[a]];
  }
  GLint misc[4] = {(GLint)m.gpu_stream, (GLint)m.vsz, 0, 0};
  std::memcpy(&misc[2], r.mtx_idx_tex, 8);
  const float sc[2] = {1.0f / (float)(1 << f.pos.frac), 1.0f / (float)(1 << f.tex[0].frac)};
  struct Locs { GLint buf, res, arr, str, misc, scale; };
  static std::unordered_map<GLuint, Locs> locs;
  auto it = locs.find(prog);
  if (it == locs.end())
    it = locs.emplace(prog, Locs{glGetUniformLocation(prog, "uPullBuf"), glGetUniformLocation(prog, "uPullRes"),
                                 glGetUniformLocation(prog, "uLArr"), glGetUniformLocation(prog, "uLStr"),
                                 glGetUniformLocation(prog, "uLMisc"), glGetUniformLocation(prog, "uLScale")}).first;
  const Locs &L = it->second;
  // P3 : uniformes par le cache (rien n'est renvoye s'il n'a pas change) ; les unites 14/15 gardent leurs tampons
  // depuis leur creation (personne d'autre ne s'en sert) : plus de liaison par dessin (~13 -> ~3 appels GL par liste).
  cu_Uniform1i(L.buf, kUnit);
  cu_Uniform1i(L.res, kUnit - 1);
  cu_Uniform4i(L.arr, ao[0], ao[1], ao[2], ao[3]);
  cu_Uniform4i(L.str, as[0], as[1], as[2], as[3]);
  cu_Uniform4i(L.misc, misc[0], misc[1], misc[2], misc[3]);
  cu_Uniform2f(L.scale, sc[0], sc[1]);
  glBindVertexArray(s_res_vao);
  g_jr_gldraw.fetch_add(1, std::memory_order_relaxed);
  glDrawElements(mode, (GLsizei)m.idx.size(), GL_UNSIGNED_INT, (const void *)(uintptr_t)m.gpu_idx);
  ++st_lists;
}
static void flush(GLuint prog, GLenum mode) {
  if (!open) return;
  if (idx.empty() || !ensure()) { reset(); return; }
  const size_t recb = rec.size() * 4, total = recb + dat.size();
  const size_t off = s_data.alloc(total);
  const size_t ioff = s_idx.alloc(idx.size() * 4);
  if (off == SIZE_MAX || ioff == SIZE_MAX) { ++st_lost; reset(); return; }   // lot geant : perdu, compte au journal
  s_data.write(off, rec.data(), recb, dat.data(), dat.size());
  s_idx.write(ioff, idx.data(), idx.size() * 4);
  st_bytes += total + idx.size() * 4;
  struct Locs { GLint buf, rec, dat; };
  static std::unordered_map<GLuint, Locs> locs;
  auto it = locs.find(prog);
  if (it == locs.end())
    it = locs.emplace(prog, Locs{glGetUniformLocation(prog, "uPullBuf"), glGetUniformLocation(prog, "uPullRec"),
                                 glGetUniformLocation(prog, "uPullDat")}).first;
  glUniform1i(it->second.buf, kUnit);
  glUniform1i(it->second.rec, (GLint)(off / 4));
  glUniform1i(it->second.dat, (GLint)(off + recb));
  glActiveTexture(GL_TEXTURE0 + kUnit);
  glBindTexture(GL_TEXTURE_BUFFER, s_tex);
  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(s_vao);
  g_jr_gldraw.fetch_add(1, std::memory_order_relaxed);
  glDrawElements(mode, (GLsizei)idx.size(), GL_UNSIGNED_INT, (const void *)(uintptr_t)ioff);
  ++st_flush;
  reset();
}
}  // namespace pull

static void submit_pending() {
  if (pull::open) {
    rprof::Scope rp_pull(rprof::DRAW);
    ++rprof::cnt_flush;
    pull::flush(s_active_shader.id, s_batch_glmode);
    return;
  }
  uint64_t res_store_key = 0;
  if (!resident::pend.empty()) {
    const uint64_t key = resident::pend_hash ^ ((uint64_t)s_batch_glmode << 56);
    if (resident::pend_ok) {
      auto it = resident::map.find(key);
      if (it != resident::map.end() && it->second.committed) {
        const resident::Entry &e = it->second;
        rprof::Scope rp_res(rprof::DRAW);
        ++rprof::cnt_flush;
        ++resident::hits;
        resident::hit_verts += e.vcount;
        glBindVertexArray(resident::vao);
        g_jr_gldraw.fetch_add(1, std::memory_order_relaxed);
        glDrawElements(s_batch_glmode, (GLsizei)e.icount, GL_UNSIGNED_INT,
                       (const void *)(uintptr_t)(e.ifirst * sizeof(uint32_t)));
        resident::reset_pending();
        return;
      }
      // Rangé seulement a sa deuxieme apparition : un lot qui ne revient pas (effet, texte
      // anime) ne remplit pas le tampon.
      if (it == resident::map.end()) {
        if (resident::seen.count(key)) res_store_key = key;
        else resident::seen.insert(key);
      }
    }
    {
      rprof::Scope rp_emit(rprof::EMIT);
      for (const GxOp *c : resident::pend) push_prim(*c);
    }
    ++resident::misses;
    resident::reset_pending();
    if (res_store_key && !s_pend.empty()) resident::store(res_store_key);
  }
  if (s_pend.empty())
    return;
  rprof::Scope rp_flush(rprof::FLUSH);
  ++rprof::cnt_flush;
  rprof::cnt_verts += s_pend.size();
  ++g_prof_flush;
  g_prof_flush_verts += s_pend.size();

  BindBatchObjects();
  // Tampons en anneau, ecrits sans synchronisation a la suite les uns des autres, et
  // reallouees (orphelins) seulement quand ils sont pleins.
  //
  // Avant : deux glBufferData par vidage de lot, soit des milliers de reallocations par image.
  // Le profil du fil hote (NJ_HOSTPROF) mettait 30 % du temps du fil dans le pilote, en
  // attente dans ntdll, appele depuis Render : le pilote se synchronisait sur le GPU.
  // Taille de l'anneau en sommets (NJ_RINGVERTS, indices : x3). Defaut 1 M (~80 Mo).
  static const size_t kRingVerts = [] {
    const char *e = NJ_ENV("NJ_RINGVERTS");
    const long v = e ? std::atol(e) : 0;
    return v > 4096 ? (size_t)v : (size_t)1u << 20;
  }();
  static const size_t kRingIdx = kRingVerts * 3;
  static size_t s_ring_v = kRingVerts, s_ring_i = kRingIdx;   // plein : premiere allocation
  const size_t nv = s_pend.size(), ni = s_indices.size();
  // NJ_UPLOAD=orphan : un stockage neuf par lot (glBufferData), jamais d'attente du GPU.
  // Defaut en GL ES : mesure sur le Galaxy S5 (Adreno 420, 29/09), rendu de 1 150 a 580 ms par
  // image - le pilote attendait la fin des dessins en cours avant chaque ecriture dans l'anneau
  // (~890 attentes par image), l'ecriture non synchronisee (map) ne l'evitait pas.
  static const bool s_upload_orphan = [] {
    const char *e = NJ_ENV("NJ_UPLOAD");
    return e && std::strcmp(e, "orphan") == 0;
  }();
  // NJ_RPROF=3 : glFinish avant chaque lot (le temps passe a attendre le GPU, compte a part).
  if (rprof::mode() >= 3) {
    const uint64_t f0 = rprof::now_us();
    glFinish();
    rprof::acc[rprof::FINISH] += rprof::now_us() - f0;
  }
  if (nv > kRingVerts || ni > kRingIdx || s_upload_orphan) {
    const uint64_t rp_o0 = rprof::mode() ? rprof::now_us() : 0;
    // Lot hors norme : l'ancien chemin, tel quel.
    glBufferData(GL_ARRAY_BUFFER, nv * sizeof(PackedVtx), s_pend.data(), GL_STREAM_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ni * sizeof(uint32_t), s_indices.data(),
                 GL_STREAM_DRAW);
    s_ring_v = kRingVerts;
    s_ring_i = kRingIdx;
    const uint64_t rp_o1 = rprof::mode() ? rprof::now_us() : 0;
    if (rprof::mode()) rprof::acc[rprof::UPLOAD] += rp_o1 - rp_o0;
    g_jr_gldraw.fetch_add(1, std::memory_order_relaxed);
    glDrawElements(s_batch_glmode, (GLsizei)ni, GL_UNSIGNED_INT, nullptr);
    if (rprof::mode()) rprof::acc[rprof::DRAW] += rprof::now_us() - rp_o1;
    ClearBatch();
    return;
  }
  if (s_ring_v + nv > kRingVerts || s_ring_i + ni > kRingIdx) {
    glBufferData(GL_ARRAY_BUFFER, kRingVerts * sizeof(PackedVtx), nullptr, GL_STREAM_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, kRingIdx * sizeof(uint32_t), nullptr, GL_STREAM_DRAW);
    s_ring_v = s_ring_i = 0;
  }
  // glBufferSubData dans une zone que le GPU n'utilise pas : chemin rapide du pilote (la copie
  // part dans le flux de commandes). glMapBufferRange non synchronise, essaye d'abord, coutait
  // plus cher que l'ancien chemin (render 15 -> 30 ms par image).
  //
  // Sur l'Adreno 420 (Galaxy S5), ce meme glBufferSubData coutait 800 ms par image sur 1 130 :
  // le pilote synchronise ou recopie le tampon a chaque envoi (~890 par image). La zone ecrite
  // n'etant jamais lue par le GPU avant son tour de l'anneau, on peut l'ecrire sans
  // synchronisation : glMapBufferRange(UNSYNCHRONIZED), par defaut en GL ES sur mobile.
  // NJ_UPLOAD=subdata | map : forcer l'un ou l'autre (A/B sur les deux plateformes).
  static const bool s_upload_map = [] {
    const char *e = NJ_ENV("NJ_UPLOAD");
    if (e) return std::strcmp(e, "map") == 0 || std::strcmp(e, "mapnoinv") == 0;
#if defined(NJ_GLES) || defined(__ANDROID__)
    return true;   // Actif par defaut sur Android pour eviter le blocage pilote Adreno
#else
    return false;
#endif
  }();
  // sans GL_MAP_INVALIDATE_RANGE_BIT par defaut : l'anneau garantit deja que la zone
  // n'est pas lue par le GPU ; sur l'Adreno, l'invalidation forcerait une attente.
  static const GLbitfield s_map_flags = [] {
    const char *e = NJ_ENV("NJ_UPLOAD");
    if (e && std::strcmp(e, "map") == 0)
      return GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_RANGE_BIT;
    return GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT;
  }();
  auto upload = [](GLenum target, size_t off, size_t bytes, const void *src) {
    if (s_upload_map) {
      void *dst = glMapBufferRange(target, (GLintptr)off, (GLsizeiptr)bytes, s_map_flags);
      if (dst) {
        std::memcpy(dst, src, bytes);
        glUnmapBuffer(target);
        return;
      }
    }
    glBufferSubData(target, (GLintptr)off, (GLsizeiptr)bytes, src);
  };
  const uint64_t rp_up0 = rprof::mode() ? rprof::now_us() : 0;
  upload(GL_ARRAY_BUFFER, s_ring_v * sizeof(PackedVtx), nv * sizeof(PackedVtx),
         s_pend.data());
  // Les indices deviennent absolus dans l'anneau (pas de glDrawElementsBaseVertex en ES 3.0).
  if (s_ring_v)
    for (auto &ix : s_indices)
      ix += (uint32_t)s_ring_v;
  upload(GL_ELEMENT_ARRAY_BUFFER, s_ring_i * sizeof(uint32_t), ni * sizeof(uint32_t),
         s_indices.data());
  const size_t idx_byte_off = s_ring_i * sizeof(uint32_t);
  s_ring_v += nv;
  s_ring_i += ni;

  static const bool s_gxtrace1 = NJ_ENV("NJ_GXTRACE") != nullptr;
  if (s_gxtrace1) {
    static int dn = 0;
    if (dn++ < 5)
      NJ_PRINTF("[GXTRACE] flush #%d verts=%zu\n", dn, s_pend.size());
  }
  const uint64_t rp_dr0 = rprof::mode() ? rprof::now_us() : 0;
  if (rprof::mode()) rprof::acc[rprof::UPLOAD] += rp_dr0 - rp_up0;
  // NJ_GPUTEST=pix (diagnostic) : dessins limites a 4 x 4 pixels, pour separer le cout des
  // sommets de celui des pixels sur le GPU.
  static const bool s_gputest_pix = [] {
    const char *e = NJ_ENV("NJ_GPUTEST");
    return e && std::strcmp(e, "pix") == 0;
  }();
  if (s_gputest_pix) {
    nj_cap(GL_SCISSOR_TEST, true);
    glScissor(0, 0, 4, 4);
  }
  g_jr_gldraw.fetch_add(1, std::memory_order_relaxed);
  glDrawElements(s_batch_glmode, (GLsizei)ni, GL_UNSIGNED_INT, (const void *)idx_byte_off);
  if (s_gputest_pix) nj_cap(GL_SCISSOR_TEST, false);
  if (rprof::mode()) rprof::acc[rprof::DRAW] += rprof::now_us() - rp_dr0;
  ClearBatch();
}

// Matrice de projection GL (colonnes) depuis la projection XF. Perspective : z du GX dans
// [-1, 0] etire sur [-1, 1], w = -z. Orthographique : meme etirement de z (comme Dolphin, qui
// prend les parametres 4 et 5 tels quels pour les deux types), les dessins des deux sortes
// partagent ainsi une echelle de profondeur. Sans projection chargee : identite.
static void upload_proj(GLint uProj) {
  if (uProj < 0)
    return;
  // Six coefficients GX (XF 0x1020-0x1025), type 0 perspective, 1 orthographique. Matrice GL en
  // colonnes ; la profondeur GX [-1, 0] est etiree sur [-1, 1].
  const Projection &pr = g_gxregs.proj;
  const float *c = pr.m;
  float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  if (pr.set) {
    const bool persp = pr.type == 0;
    m[0] = c[0];
    m[5] = c[2];
    m[persp ? 8 : 12] = c[1];
    m[persp ? 9 : 13] = c[3];
    m[10] = persp ? 2.0f * c[4] - 1.0f : 2.0f * c[4];
    m[14] = persp ? 2.0f * c[5] : 2.0f * c[5] + 1.0f;
    m[11] = persp ? -1.0f : 0.0f;
    m[15] = persp ? 0.0f : 1.0f;
  }
  std::memcpy(s_proj_mtx, m, sizeof(s_proj_mtx));
  cu_UniformMatrix4fv(uProj, 1, GL_FALSE, m);
}

static bool CurrentIsPerspective() { return g_gxregs.proj.set && g_gxregs.proj.type == 0; }
// Image propre (18/11, images des sauvegardes) : appele une fois par image, au premier dessin orthographique qui suit
// la scene 3D (plus de 50 dessins en perspective) : HUD, textes, barres du debug et bandes ne sont pas encore dessines.
extern "C" { void (*nj_clean_frame_hook)(void) = nullptr; }
static int s_clean_persp = 0;
static bool s_clean_done = false;

// PE_ZMODE (BP 0x40) : bit 0 test, bits 1-3 comparaison, bit 4 ecriture. La projection garde
// l'ordre de profondeur du GX (proche -> 0), donc les huit comparaisons passent telles quelles.
// Sans test, le GX n'ecrit pas non plus la profondeur (OpenGL non plus) : le clearZbuffer du
// sous-ecran (quad orthographique, ALWAYS, ecriture) en depend (Sscrn/ss_main.cpp).
static void set_depth_state() {
  static const GLenum kDepthFn[8] = {GL_NEVER,   GL_LESS,     GL_EQUAL,  GL_LEQUAL,
                                     GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS};
  const uint32_t z = g_gxregs.bp[0x40];
  const bool test = (z & 1) != 0;
  if (test) {
    nj_cap(GL_DEPTH_TEST, true);
    glDepthFunc(kDepthFn[(z >> 1) & 7]);
  } else {
    nj_cap(GL_DEPTH_TEST, false);
  }
  glDepthMask(test && ((z >> 4) & 1) ? GL_TRUE : GL_FALSE);
}

// PE_CMODE0 (BP 0x41) : melange, soustraction, facteurs, masques d'ecriture couleur (bit 3) et
// alpha (bit 4) ; PE_CMODE1 (BP 0x42) : alpha de destination constant. RE4 garde le facteur de
// flou de profondeur dans l'alpha de l'EFB : un format de pixel sans alpha (tout sauf RGBA6_Z24,
// BP 0x43) ne l'ecrit jamais (Dolphin). Tables des facteurs : ordre du GX (la source utilise
// DST_COLOR aux places 2/3, la destination SRC_COLOR).
static void set_blend_state() {
  static const GLenum kSrcFactor[8] = {
      GL_ZERO, GL_ONE, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA,
      GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA,
  };
  static const GLenum kDstFactor[8] = {
      GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_SRC_ALPHA,
      GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA,
  };
  const uint32_t c0 = g_gxregs.bp[0x41], c1 = g_gxregs.bp[0x42];
  const bool blend = (c0 & 1) != 0;
  const bool sub = ((c0 >> 11) & 1) != 0;
  const bool write_rgb = ((c0 >> 3) & 1) != 0;
  const bool write_a = ((c0 >> 4) & 1) && (g_gxregs.bp[0x43] & 7) == 1;
  glColorMask(write_rgb, write_rgb, write_rgb, write_a);
  const bool const_alpha = ((c1 >> 8) & 1) && write_a;
  if (!blend && !const_alpha) {
    nj_cap(GL_BLEND, false);
    return;
  }
  nj_cap(GL_BLEND, true);
  // La couleur garde ses facteurs du GX (passage direct si le melange est coupe) ; avec l'alpha
  // de destination constant, l'alpha ecrit est la constante.
  GLenum src = GL_ONE, dst = GL_ZERO, eq = GL_FUNC_ADD;
  if (blend && sub) {
    dst = GL_ONE;
    eq = GL_FUNC_REVERSE_SUBTRACT;
  } else if (blend) {
    src = kSrcFactor[(c0 >> 8) & 7];
    dst = kDstFactor[(c0 >> 5) & 7];
  }
  if (const_alpha) {
    glBlendColor(0.f, 0.f, 0.f, (c1 & 0xFF) / 255.f);
    glBlendEquationSeparate(eq, GL_FUNC_ADD);
    glBlendFuncSeparate(src, dst, GL_CONSTANT_ALPHA, GL_ZERO);
  } else {
    glBlendEquation(eq);
    glBlendFunc(src, dst);
  }
}

// GEN_MODE (BP 0x00) bits 14-15 : aucune, arriere, avant, toutes les faces. Notre projection
// inverse Y : un triangle direct sur la console arrive indirect ici. Les faces avant sont donc
// declarees dans le sens horaire et la cible reste la face arriere ; eliminer l'avant revient a
// declarer le sens inverse. (Changer glCullFace en gardant GL_CCW retire l'autre moitie de la
// scene : feuillages et textes avaient disparu au premier essai.)
static void set_cull_state() {
  const uint8_t mode = g_gxregs.cull;
  if (mode == 0) {
    nj_cap(GL_CULL_FACE, false);
    return;
  }
  nj_cap(GL_CULL_FACE, true);
  glCullFace(mode == 3 ? GL_FRONT_AND_BACK : GL_BACK);
  glFrontFace(mode == 2 ? GL_CCW : GL_CW);
}

// Ligne `row` d'une matrice 3x4 (memoire XF) appliquee au point (x, y, z, 1). L'ordre des
// operations est celui du rendu d'origine : le resultat doit rester identique au bit pres.
static inline float XfRow4(const float *m, int row, float x, float y, float z) {
  const float *r = m + row * 4;
  return x * r[0] + y * r[1] + z * r[2] + r[3];
}
// Ligne `row` d'une matrice 3x3 appliquee au vecteur v.
static inline float XfRow3(const float *m, int row, const float v[3]) {
  const float *r = m + row * 3;
  return v[0] * r[0] + v[1] * r[1] + v[2] * r[2];
}

// Ajoute un sommet au lot et rend son indice. Les sommets transformes au decodage (pretransformed,
// instantane des matrices du dessin) passent tels quels ; les autres prennent leur matrice de
// position (indice * 4) et leur matrice de normales (3 * (indice & 31)) dans la memoire XF.
static uint32_t put_vertex(const GxVertex &vtx) {
  const size_t idx = s_pend.size();
  s_pend.resize(idx + 1);
  PackedVtx &v = s_pend[idx];
  if (vtx.has_color)
    std::copy_n(vtx.color, 4, v.color);
  else
    std::fill_n(v.color, 4, 1.f);
  if (vtx.has_nrm) {
    const int nb = 3 * (vtx.posMtxIdx & 31);
    const bool xf = !vtx.pretransformed && nb + 9 <= 96;
    for (int k = 0; k < 3; ++k)
      v.nrm[k] = xf ? XfRow3(g_gxregs.xfm.nrm + nb, k, vtx.norm) : vtx.norm[k];
    std::copy_n(vtx.obj_norm, 3, v.obj_nrm);
  }
  if (vtx.has_tex[0])
    std::copy_n(vtx.tex[0], 2, v.uv);
  std::copy_n(vtx.obj_pos, 3, v.obj_pos);
  std::memcpy(v.tidx, vtx.texMtxIdx, 8);
  const int pb = 4 * vtx.posMtxIdx;
  const bool xf = !vtx.pretransformed && vtx.has_pos && pb + 12 <= 256;
  for (int k = 0; k < 3; ++k)
    v.pos[k] = xf ? XfRow4(g_gxregs.xfm.pos + pb, k, vtx.pos[0], vtx.pos[1], vtx.pos[2]) : vtx.pos[k];
  return (uint32_t)idx;
}

// Matrice portee par le lot en cours ; sert a decider du vidage.
static float s_batch_xf_pos[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
// Index de matrice de texture du generateur 0 porte par les sommets du lot (uniforme).
static uint8_t s_batch_tidx0 = 0xFF;
static uint16_t s_batch_variant = 0;
static uint64_t g_why_closed = 0, g_why_mtx = 0, g_why_other = 0;   // raisons de reposer l'etat (P3)
// Commandes d'etat qui ont ferme le lot, par type (BP / CP / XF) et par registre BP le plus frequent (P3).
static uint64_t g_close_bp = 0, g_close_cp = 0, g_close_xf = 0, g_close_bpreg[256];
// Ecritures « autres » (non classees texture / melange) entre deux dessins : BP par registre, XF par zone
// (0 matrices position/texture, 1 normales, 2 post-matrices, 3 lumieres, 4 registres 0x1000+).
static uint64_t g_other_bpreg[256], g_other_xf[5];   // variante « sommets tires » du lot ouvert (0 : chemin classique)
static uint8_t DrawTexIdx0(const GxOp &cmd) {
  return cmd.verts().empty() ? g_gxregs.mtx_idx_tex[0] : cmd.verts()[0].texMtxIdx[0];
}
static float s_batch_xf_nrm[9]  = {1,0,0, 0,1,0, 0,0,1};
static bool s_pend_open = false;

// Le lot en cours (sommets d'un meme etat) part au GPU ; le prochain dessin en ouvrira un autre.
// P3a (documentation interne) : ce qui a change depuis la derniere pose complete de l'etat.
// Texture seule (BP 0x80-0xBB) et/ou melange/profondeur (BP 0x40-0x42) : pose legere (pas de choix de programme ni
// de renvoi de tous les uniformes). Tout le reste, et toute fermeture non classee (copies EFB, lectures, fin
// d'image) : pose complete. NJ_FULLSTATE=1 : toujours complete (A/B).
enum : uint32_t { kDirtyTex = 1, kDirtyPe = 2, kDirtyOther = 4 };
static uint32_t s_dirty = kDirtyOther;
static bool s_full_ok = false;   // une pose complete a eu lieu depuis le dernier changement « autre »
static uint64_t g_bind_light = 0, g_bind_full = 0;
static void CloseBatchClassified(uint32_t dirty);
static void CloseBatch() {
  s_dirty |= kDirtyOther;
  if (!s_pend_open)
    return;
  submit_pending();
  s_pend_open = false;
}
// Fermeture par une ecriture d'etat dont on connait la categorie (boucle de draw_frame).
static void CloseBatchClassified(uint32_t dirty) {
  s_dirty |= dirty;
  if (!s_pend_open)
    return;
  submit_pending();
  s_pend_open = false;
}
static uint8_t s_pend_prim = 0;

// Diagnostic (16/10, defaut intermittent du point d'apparition sur MuMu) : erreurs du pilote GL et dessins sautes faute de
// programme pret, resumes dans le journal de performance (au plus un evenement par minute, seulement s'il y a quelque chose).
static uint32_t s_diag_skips_total = 0, s_diag_errs[8] = {0};
static GLenum s_diag_err_codes[8] = {0};
#if defined(NJ_GLES) && defined(NJ_DEVTOOLS)
#include <GLES3/gl32.h>
// KHR_debug (GL ES 3.2, build de developpement) : le pilote dit lui-meme quel appel a produit l'erreur ; les 40 premiers messages d'erreur vont au journal.
static void GL_APIENTRY gl_debug_cb(GLenum, GLenum type, GLuint id, GLenum, GLsizei len, const GLchar *msg, const void *) {
  static int n = 0;
  if (type != GL_DEBUG_TYPE_ERROR || n >= 40) return;
  ++n;
  char b[300];
  std::snprintf(b, sizeof b, "glerr #%d id=%u %.*s", n, (unsigned)id, len > 230 ? 230 : (int)len, msg);
  nj::perf::event(b);
}
#endif
static uint32_t s_gl_frames = 0, s_warm_until = 1500, s_room_seen = 0;   // voir shader_warmup()
static void DiagFrame() {
  static uint32_t frame = 0, last_emit = 0;
  ++frame;
  s_gl_frames = frame;
#if defined(NJ_DEVTOOLS)
  {   // knobs.txt « texflush N » : vide le cache de textures a chaque changement de N (textures perimees ?)
    static int last_flush = 0;
    if (const int ka = nj::dev::knob("aniso", -1); ka >= 0) nj::runtime::gfx::g_aniso = ka;   // knobs.txt : anisotropie en direct (1 = coupee)
    const int k = nj::dev::knob("texflush", 0);
    if (k != last_flush) {
      last_flush = k;
      nj::runtime::hle::TexCache::get().clear();
      nj::perf::event("dev : cache de textures vide (texflush)");
    }
  }
#endif
#if defined(NJ_GLES) && defined(NJ_DEVTOOLS)
  if (frame == 1) {
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(gl_debug_cb, nullptr);
  }
#endif
  if (frame == 2) {
    const char *r = (const char *)glGetString(GL_RENDERER), *v = (const char *)glGetString(GL_VERSION);
    char b[220];
    std::snprintf(b, sizeof b, "gl renderer=%s | %s"_g, r ? r : "?", v ? v : "?");
    nj::perf::event(b);
  }
  if ((frame & 15) == 0) {
    for (int i = 0; i < 4; ++i) {
      const GLenum e = glGetError();
      if (e == GL_NO_ERROR) break;
      int k = 0;
      while (k < 8 && s_diag_err_codes[k] && s_diag_err_codes[k] != e) ++k;
      if (k < 8) {
        s_diag_err_codes[k] = e;
        ++s_diag_errs[k];
      }
    }
  }
  if (frame - last_emit >= 1800) {
    last_emit = frame;
    char b[220];
    int n = std::snprintf(b, sizeof b, "gl f=%u skips=%u pc=%u/%u/%u/%u"_g, frame, s_diag_skips_total, s_prog_cache_hits, s_prog_cache_saved, s_prog_cache_nolen, s_prog_cache_nofile);
    bool any = s_diag_skips_total > 0;
    for (int k = 0; k < 8 && s_diag_err_codes[k]; ++k) {
      n += std::snprintf(b + n, sizeof b - (size_t)n, " err%04X=%u", (unsigned)s_diag_err_codes[k], s_diag_errs[k]);
      any = true;
    }
    if (any) nj::perf::event(b);
  }
}

// 20/10 : pas de dessin saute pendant le chargement. Le budget de compilation (ci-dessous) saute les dessins dont le programme n'est pas pret ; au demarrage et
// a chaque changement de salle, l'ecran est noir ou en fondu : on compile tout (a-coup d'une image) plutot que d'afficher une salle sans ses decors pendant des
// secondes (defaut du point d'apparition apres une installation neuve : cache de nuanceurs du pilote vide).
static bool shader_warmup() {
  const uint32_t r = nj::perf::g_room_serial.load(std::memory_order_relaxed);
  if (r != s_room_seen) {
    s_room_seen = r;
    s_warm_until = s_gl_frames + 200;   // ~6 s au changement de salle
  }
  return s_gl_frames < s_warm_until;
}

static int s_shader_jobs = 0;
static const int kShaderBudget = 24;
// Compiling and linking a multi-stage TEV program costs tens of milliseconds
// on the driver, so a count-only budget of 12 per frame could stall a frame
// for seconds once RE4's multi-stage shaders started compiling. Compile at
// least one per frame, then stop once this much time went into compiles.
static double s_compile_ms_this_frame = 0.0;
static const double kCompileBudgetMs = 20.0;   // 20/10 : 8 ms (un programme par image sur telephone) laissait des milliers de dessins sautes au demarrage
// NJ_NOBUDGET : compile tout, sans plafond. Le plafond de 8 ms depend de la
// charge de la machine, donc le nombre de dessins sautes change d une execution
// a l autre : une image rejouee n etait pas reproductible, ce qui interdit de
// s en servir comme reference. Le rejeu FIFO est hors ligne, il peut attendre.
static bool no_compile_budget() {
  static const bool v = NJ_ENV("NJ_NOBUDGET") != nullptr;
  return v;
}
// Many GX states generate identical GLSL (the state hash keys on whole
// registers); programs are shared by source so each source is built once.
static std::unordered_map<uint64_t, Program> s_program_by_source;
// Set by bind_draw_state when the draw's shader isn't ready: DrawPrimitive then
// n'emet pas les sommets du dessin plutot que de les tracer avec un programme faux.
static bool s_drop_draw = false;

static void bind_draw_state(const GxOp &cmd);
static void bind_light(const GxOp &cmd, uint32_t dirty);
static void push_prim(const GxOp &cmd);

// One-frame draw log, armed by F12 (g_gx_framelog_request). For each draw it
// records what decides visibility: where the vertices land in NDC, whether
// depth, blending or the colour mask can drop the pixels, whether the program
// linked or the draw was skipped, what stage 0 samples (EFB copy or not), and
// the lighting-channel colours feeding the TEV.
static int s_draw_index = 0; // index de primitive, pour NJ_DRAWSTART/STOP
static bool s_framelog = false;
static int s_framelog_draws = 0;
static int s_framelog_onscreen = 0, s_framelog_offscreen = 0;
static int s_framelog_unlinked = 0, s_framelog_skipped = 0;

static uint32_t XfBits(int index) {
  uint32_t v = 0;
  std::memcpy(&v, &g_gxregs.xf[index], 4);
  return v;
}

// Ecriture dans la memoire du XF a partir de base. Plan de la memoire : matrices de position
// (0x000-0x0FF), de normale (0x400-0x45F), de post-transformation (0x500-0x5FF), lumieres (0x600-0x67F,
// 16 mots chacune), registres (0x1000-0x10FF) dont la vue (0x101A-0x101F) et la projection
// (0x1020-0x1026, le dernier mot en est le type). Dit si la vue ou la projection ont change.
struct XfTouched {
  bool viewport = false, projection = false;
};
// P3a : vrai si chaque mot de l'ecriture XF vaut deja ce qui est stocke (comparaison bit a bit), adresses connues seulement.
static uint64_t g_xf_noop = 0, g_xf_real = 0, g_bp_noop_skip = 0;
static bool xf_noop(const GxOp &c) {
  static const bool off = NJ_ENV("NJ_XFNOOPOFF") != nullptr;
  if (off) return false;
  const PayloadVec &w = c.payload;
  if (w.size() == 0) return false;
  for (size_t k = 0; k < w.size(); ++k) {
    const int a = (int)c.reg + (int)k;
    const float v = w[k];
    const float *cur = nullptr;
    uint32_t reg32 = 0;
    if (a >= 0 && a < 0x100) cur = &g_gxregs.xfm.pos[a];
    else if (a >= 0x400 && a < 0x460) cur = &g_gxregs.xfm.nrm[a - 0x400];
    else if (a >= 0x500 && a < 0x600) cur = &g_gxregs.xfm.post[a - 0x500];
    else if (a >= 0x600 && a < 0x680) cur = &g_gxregs.xfm.light[(a - 0x600) / 16][(a - 0x600) % 16];
    else if (a >= 0x1000 && a < 0x1100) reg32 = g_gxregs.xf[a - 0x1000];
    else { ++g_xf_real; return false; }
    uint32_t nv, ov;
    std::memcpy(&nv, &v, 4);
    if (cur) std::memcpy(&ov, cur, 4);
    else ov = reg32;
    if (nv != ov) { ++g_xf_real; return false; }
  }
  ++g_xf_noop;
  return true;
}

static XfTouched WriteXF(int base, const PayloadVec &words) {
  XfTouched t;
  for (size_t k = 0; k < words.size(); ++k) {
    const int a = base + (int)k;
    const float w = words[k];
    if (a >= 0 && a < 0x100) {
      if (g_gxregs.xfm.pos[a] != w) {   // banque de matrices changee : les sommets se recalculent
        g_gxregs.xfm.pos[a] = w;
        ++s_xf_bank_gen;
      }
    } else if (a >= 0x400 && a < 0x460) {
      g_gxregs.xfm.nrm[a - 0x400] = w;
    } else if (a >= 0x500 && a < 0x600) {
      g_gxregs.xfm.post[a - 0x500] = w;
    } else if (a >= 0x600 && a < 0x680) {
      const int l = a - 0x600;
      g_gxregs.xfm.light[l / 16][l % 16] = w;
      ++g_gxregs.xf_gen;
    } else if (a >= 0x1000 && a < 0x1100) {
      std::memcpy(&g_gxregs.xf[a - 0x1000], &w, 4);
      if (a >= 0x100A && a <= 0x1011)   // couleurs ambiante / materiau, controle des canaux
        ++g_gxregs.xf_gen;
      if (a >= 0x101A && a <= 0x101F) {
        t.viewport = true;
      } else if (a >= 0x1020 && a <= 0x1026) {
        g_gxregs.proj.m[a - 0x1020] = w;
        if (a == 0x1026)
          std::memcpy(&g_gxregs.proj.type, &w, 4);
        t.projection = true;
      }
    }
  }
  return t;
}

// GX viewport (XF 0x101A-0x101F) onto the 640x480 EFB. Without it every pass
// drew over the whole framebuffer: RE4 renders its frame at 512x448 and its
// 256x224 / 128x112 bloom downsamples covered the scene. The origins carry
// the hardware's 342 offset (Dolphin SetViewport), and GL's y runs upward.
static void ApplyViewport() {
  float v[6];
  for (int i = 0; i < 6; ++i)
    std::memcpy(&v[i], &g_gxregs.xf[0x1A + i], 4);
  const float wd = v[0], ht = v[1], xo = v[3], yo = v[4];
  if (wd == 0.0f || ht == 0.0f) { // never set: whole EFB
    glViewport(0, 0, nj::runtime::gfx::efb_w(), nj::runtime::gfx::efb_h());
    return;
  }
  float x = xo - wd - 342.0f, y = yo + ht - 342.0f;
  float w = 2.0f * wd, h = -2.0f * ht;
  if (w < 0) { x += w; w = -w; }
  if (h < 0) { y += h; h = -h; }
  // Resolution interne : l'EFB fait 640*S x 480*S (runtime/gfx_settings.h).
  const float S = (float)nj::runtime::gfx::g_scale;
  glViewport((GLint)std::lround(x * S), (GLint)std::lround((480.0f - (y + h)) * S),
             (GLsizei)std::lround(w * S), (GLsizei)std::lround(h * S));
  // Depth range: window z = zFar + zRange * z_ndc with z_ndc in [-1, 0], in
  // 24-bit units (XF 0x101C zRange, 0x101F zFar). Our NDC [-1, 1] is that
  // range stretched, so it maps onto [zFar - zRange, zFar]. Ignoring it gave
  // RE4's sky dome ordinary near depth: the whole village failed the depth
  // test behind it and the frame kept the sky colour.
  const float zrange = v[2], zfar = v[5];
  auto unit = [](float d) {
    d /= 16777215.0f;
    return d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d);
  };
#ifdef NJ_GLES
  glDepthRangef(unit(zfar - zrange), unit(zfar));
  // GL ES n'a le depth clamp que par GL_EXT_depth_clamp (0x864F). Sans lui,
  // l'appel echoue sans effet et les passes hors [near, far] sont coupees.
  // 13/10 : plus de GL_EXT_depth_clamp (il gardait aussi les modeles trop proches de la camera,
  // visage vu a travers les cheveux) : decoupe near / far du GPU + bornage des passes orthographiques
  // dans le vertex shader (tev_shader_gen.cpp), comme sur PC.
#else
  glDepthRange(unit(zfar - zrange), unit(zfar));
  // GX hardware clamps depth rather than clipping (Dolphin uses depth clamp
  // too): screen-space passes placed outside [near, far] must still draw.
  // NJ_GLESCLIP : comportement de GL ES (essai de diagnostic) : ni depth clamp ni distances de decoupe,
  // le GPU decoupe near / far lui-meme ; les passes orthographiques sont bornees dans le vertex shader.
  static const bool s_gles_clip = NJ_ENV("NJ_GLESCLIP") != nullptr;
  nj_cap(GL_DEPTH_CLAMP, !s_gles_clip);
  // Decoupe near / far des dessins en perspective (distances ecrites par le vertex shader,
  // tev_shader_gen.cpp). NJ_NOZCLIP : l'ancien comportement (clamp seul).
  static const bool s_no_zclip = NJ_ENV("NJ_NOZCLIP") != nullptr;
  if (!s_no_zclip && !s_gles_clip && !nj::dev::knob("nozclip", 0)) {
    nj_cap(GL_CLIP_DISTANCE0, true);
    nj_cap(GL_CLIP_DISTANCE1, true);
  }
#endif
}

static void LogDraw(const GxOp &cmd) {
  const bool persp = CurrentIsPerspective();
  const float *sp = g_gxregs.proj.m;
  float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
  int behind = 0;
  for (const auto &vtx : cmd.verts()) {
    float x = vtx.pos[0];
    float y = vtx.pos[1];
    float z = vtx.pos[2];
    const int mb = vtx.posMtxIdx * 4;
    if (!vtx.pretransformed && vtx.has_pos && mb + 12 <= 256) {
      const float *m = g_gxregs.xfm.pos;
      const float tx = x * m[mb] + y * m[mb + 1] + z * m[mb + 2] + m[mb + 3];
      const float ty = x * m[mb + 4] + y * m[mb + 5] + z * m[mb + 6] + m[mb + 7];
      const float tz = x * m[mb + 8] + y * m[mb + 9] + z * m[mb + 10] + m[mb + 11];
      x = tx;
      y = ty;
      z = tz;
    }
    static const bool s_orthoz0_log = NJ_ENV("NJ_ORTHOZ0") != nullptr;
    if (s_orthoz0_log && !persp)
      z = 0.0f; // as put_vertex does
    float c[4];
    if (!g_gxregs.proj.set) {
      c[0] = x;
      c[1] = y;
      c[2] = z;
      c[3] = 1.0f;
    } else if (g_gxregs.proj.type == 0) { // same matrix as upload_proj
      c[0] = sp[0] * x + sp[1] * z;
      c[1] = sp[2] * y + sp[3] * z;
      c[2] = (2.0f * sp[4] - 1.0f) * z + 2.0f * sp[5];
      c[3] = -z;
    } else {
      c[0] = sp[0] * x + sp[1];
      c[1] = sp[2] * y + sp[3];
      c[2] = 2.0f * (sp[4] * z + sp[5]) + 1.0f;
      c[3] = 1.0f;
    }
    if (c[3] <= 0.0f) {
      ++behind;
      continue;
    }
    for (int k = 0; k < 3; ++k) {
      const float ndc = c[k] / c[3];
      if (ndc < lo[k])
        lo[k] = ndc;
      if (ndc > hi[k])
        hi[k] = ndc;
    }
  }
  const bool any = lo[0] <= hi[0];
  const bool onscreen = any && hi[0] >= -1 && lo[0] <= 1 && hi[1] >= -1 &&
                        lo[1] <= 1 && hi[2] >= -1 && lo[2] <= 1;
  if (onscreen)
    ++s_framelog_onscreen;
  else
    ++s_framelog_offscreen;
  {   // diagnostic N3 (coeur natif, monde noir) : les premiers dessins hors ecran, sommet brut et matrices
    static int s_off_logged = 0;
    if (s_framelog_draws == 0) s_off_logged = 0;
    if (!onscreen && s_off_logged < 24 && !cmd.verts().empty()) {
      ++s_off_logged;
      const auto &v0 = cmd.verts()[0];
      const int mb = v0.posMtxIdx * 4;
      const float *m = g_gxregs.xfm.pos;
      NJ_PRINTF("[FrameLog] hors %d : n=%zu persp=%d pre=%d has=%d pos=(%g %g %g) idx=%d m0=(%g %g %g %g) m1=(%g %g %g %g) m2=(%g %g %g %g) proj=(%g %g %g %g %g %g) ndc x[%g %g] y[%g %g] z[%g %g] derriere=%d\n",
                  s_framelog_draws, cmd.verts().size(), (int)persp, (int)v0.pretransformed, (int)v0.has_pos, v0.pos[0], v0.pos[1], v0.pos[2],
                  (int)v0.posMtxIdx, m[mb], m[mb + 1], m[mb + 2], m[mb + 3], m[mb + 4], m[mb + 5], m[mb + 6], m[mb + 7],
                  m[mb + 8], m[mb + 9], m[mb + 10], m[mb + 11], sp[0], sp[1], sp[2], sp[3], sp[4], sp[5],
                  lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], behind);
    }
  }

  GLint linked = 0;
  if (s_active_shader.id)
    glGetProgramiv(s_active_shader.id, GL_LINK_STATUS, &linked);
  if (!linked)
    ++s_framelog_unlinked;
  if (s_drop_draw)
    ++s_framelog_skipped;

  const int idx = s_framelog_draws++;
  {
    // Who touches the viewport centre: perspective draws whose NDC box holds
    // (0,0), plus every draw of an optional index window (NJ_FLWINDOW=a:b).
    // The pixel is read before each such draw (after flushing), so a change
    // is pinned to the draws in between; the first centre-covering draws also
    // report the state that can reject their fragments.
    static unsigned char s_last_px[4] = {0, 0, 0, 0};
    static int s_last_idx = -1, s_reads = 0, s_covers_logged = 0;
    if (idx == 0) {
      s_last_idx = -1;
      s_reads = 0;
      s_covers_logged = 0;
    }
    const bool covers = persp && any && lo[0] <= 0.0f && hi[0] >= 0.0f &&
                        lo[1] <= 0.0f && hi[1] >= 0.0f &&
                        behind < (int)cmd.verts().size();
    static int win[2] = {-1, -1};
    static bool win_parsed = false;
    if (!win_parsed) {
      win_parsed = true;
      if (const char *e = NJ_ENV("NJ_FLWINDOW"))
        std::sscanf(e, "%d:%d", &win[0], &win[1]);
    }
    const bool window = win[0] >= 0 && idx >= win[0] && idx <= win[1];
    if ((covers || window) && s_reads < 3000) {
      CloseBatch();
      unsigned char px[4] = {0, 0, 0, 0};
      glReadPixels(256, 480 - 224, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
      ++s_reads;
      if (s_last_idx >= 0 && std::memcmp(px, s_last_px, 4) != 0)
        NJ_PRINTF("[FL] centre %02X%02X%02X%02X -> %02X%02X%02X%02X by draws %d..%d\n",
               s_last_px[0], s_last_px[1], s_last_px[2], s_last_px[3], px[0], px[1],
               px[2], px[3], s_last_idx, idx - 1);
      std::memcpy(s_last_px, px, 4);
      s_last_idx = idx;
      if (covers && s_covers_logged++ < 40)
        NJ_PRINTF("[FL] draw %d covers centre: prim=%02X n=%u z[%.3f,%.3f] behind=%d "
               "zmode=%06X cull=%u at=%06X cmode0=%06X tex0=%08X %ux%u f%u prog=%u "
               "chan0=%08X\n",
               idx, (unsigned)cmd.prim_type, (unsigned)cmd.verts().size(), lo[2], hi[2],
               behind, (unsigned)g_gxregs.bp[0x40], (unsigned)g_gxregs.cull,
               (unsigned)g_gxregs.bp[0xF3], (unsigned)g_gxregs.bp[0x41],
               (unsigned)g_gxregs.tex_units[0].data_addr,
               (unsigned)g_gxregs.tex_units[0].width, (unsigned)g_gxregs.tex_units[0].height,
               (unsigned)g_gxregs.tex_units[0].format, (unsigned)s_active_shader.id,
               XfBits(0x0E));
    if (covers && s_covers_logged <= 12 && !cmd.verts().empty()) {
      // Inputs of the lit colour channel for this draw: a zero or missing
      // normal, material, ambient or light colour all end up black.
      const auto &v0 = cmd.verts()[0];
      NJ_PRINTF("[FL]   v0 has_nrm=%d norm=(%.3f,%.3f,%.3f) has_color=%d "
             "color=(%.2f,%.2f,%.2f,%.2f) mat0=%08X amb0=%08X numTevStages=%u",
             (int)v0.has_nrm, v0.norm[0], v0.norm[1], v0.norm[2], (int)v0.has_color,
             v0.color[0], v0.color[1], v0.color[2], v0.color[3], XfBits(0x0C),
             XfBits(0x0A), (unsigned)g_gxregs.n_tev);
      const uint32_t c0 = XfBits(0x0E);
      const uint32_t lmask = ((c0 >> 2) & 0xF) | (((c0 >> 11) & 0xF) << 4);
      for (int i = 0, shown = 0; i < 8 && shown < 3; ++i) {
        if (!(lmask & (1u << i)))
          continue;
        const float *L = g_gxregs.xfm.light[i];
        uint32_t col = 0;
        std::memcpy(&col, &L[3], 4);
        NJ_PRINTF(" | L%d col=%08X pos=(%.0f,%.0f,%.0f) dir=(%.2f,%.2f,%.2f)", i, col,
               L[10], L[11], L[12], L[13], L[14], L[15]);
        ++shown;
      }
      NJ_PRINTF("\n");
    }
    }
  }
  if ((idx % 500) == 0) {
    // Sample the 512x448 frame centre as the frame builds up, to see when the
    // scene appears and what wipes it. Flush first so the pixels are current.
    CloseBatch();
    unsigned char px[4] = {0, 0, 0, 0};
    glReadPixels(256, 480 - 224, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    NJ_PRINTF("[FL] probe before draw %d (%s) centre=%02X%02X%02X%02X\n", idx,
           persp ? "persp" : "ortho", px[0], px[1], px[2], px[3]);
  }
  const uint32_t zm = g_gxregs.bp[0x40], cm = g_gxregs.bp[0x41];
  const auto &t0 = g_gxregs.tex_units[0];
  const bool efb_tex =
      t0.data_addr &&
      nj::runtime::hle::TexCache::get().find_efb_texture(t0.data_addr);
  // The first 300 draws are mostly sky; past them keep what can hide the
  // scene (2D passes, draws sampling an EFB copy) plus a sparse sample.
  static int s_late_logged = 0;
  if (idx == 0)
    s_late_logged = 0;
  // Draws inside NJ_FLWINDOW=a:b are always printed in full: the 400-line
  // cap below was used up by the ortho passes at the start of the frame, so
  // the end-of-frame post chain never showed.
  static int s_full_win[2] = {-1, -1};
  static bool s_full_win_parsed = false;
  if (!s_full_win_parsed) {
    s_full_win_parsed = true;
    if (const char *e = NJ_ENV("NJ_FLWINDOW"))
      std::sscanf(e, "%d:%d", &s_full_win[0], &s_full_win[1]);
  }
  const bool in_full_win =
      s_full_win[0] >= 0 && idx >= s_full_win[0] && idx <= s_full_win[1];
  // Draws decoded from a flushed-buffer copy (CPU-skinned models) are always
  // printed: they are what goes missing on screen.
  if (idx >= 300 && !in_full_win && !cmd.volatile_src) {
    if (!(efb_tex || !persp || (idx % 100) == 0) || s_late_logged >= 400)
      return;
    ++s_late_logged;
  }
  NJ_PRINTF("[FL %03d]%s prim=%02X n=%u %s ndc x[%.2f,%.2f] y[%.2f,%.2f] z[%.2f,%.2f] "
         "behind=%d %s",
         idx, cmd.volatile_src ? " VOL" : "", (unsigned)cmd.prim_type,
         (unsigned)cmd.verts().size(), persp ? "persp" : "ortho", lo[0], hi[0],
         lo[1], hi[1], lo[2], hi[2], behind, onscreen ? "ON" : "OFF");
  NJ_PRINTF(" | z=%s/f%u/%s blend=%d cupd=%d aupd=%d at=%06X",
         (zm & 1) ? "test" : "notest", (unsigned)((zm >> 1) & 7),
         (zm & 0x10) ? "upd" : "noupd", (int)(cm & 1), (int)((cm >> 3) & 1),
         (int)((cm >> 4) & 1), (unsigned)g_gxregs.bp[0xF3]);
  // Raw PE_CMODE0/1 (logic op enable is bit 1, its mode bits 12-15) and the
  // TEV stages' texture maps: composites can blend through a logic op or
  // sample several copies, neither visible in the fields above.
  NJ_PRINTF(" cm0=%06X cm1=%06X tevs=%u", (unsigned)cm, (unsigned)g_gxregs.bp[0x42],
         (unsigned)g_gxregs.n_tev);
  // Registres de couleur (C0..C2, PREV) et constantes du TEV, tels que le shader les recoit.
  NJ_PRINTF(" tg=%u info0=%05X def0=%u vtxidx0=%u dual=%u", (unsigned)g_gxregs.n_texgen,
         (unsigned)g_gxregs.xf[0x40], (unsigned)g_gxregs.mtx_idx_tex[0],
         cmd.verts().empty() ? 255u : (unsigned)cmd.verts()[0].texMtxIdx[0],
         (unsigned)(g_gxregs.xf[0x12] & 1));
  NJ_PRINTF(" chanctl=%03X/%03X/%03X/%03X", (unsigned)g_gxregs.xf[0x0E], (unsigned)g_gxregs.xf[0x0F],
         (unsigned)g_gxregs.xf[0x10], (unsigned)g_gxregs.xf[0x11]);
  NJ_PRINTF(" creg=%06X/%06X,%06X/%06X kreg=%06X/%06X ksel=%06X",
         (unsigned)g_gxregs.tev_reg[2], (unsigned)g_gxregs.tev_reg[3],
         (unsigned)g_gxregs.tev_reg[0], (unsigned)g_gxregs.tev_reg[1],
         (unsigned)g_gxregs.tev_konst[0], (unsigned)g_gxregs.tev_konst[1],
         (unsigned)g_gxregs.bp[0xF6]);
  {
    // Where this draw can land: the GL viewport in force and the GX scissor
    // (BP 0x20 top-left, 0x21 bottom-right, 0x59 offset) the renderer ignores.
    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);
    NJ_PRINTF(" vp=(%d,%d %dx%d) sc=%06X/%06X/%06X", vp[0], vp[1], vp[2], vp[3],
           (unsigned)g_gxregs.bp[0x20], (unsigned)g_gxregs.bp[0x21],
           (unsigned)g_gxregs.bp[0x59]);
  }
  for (unsigned s = 0; s < g_gxregs.n_tev && s < 4; ++s)
    NJ_PRINTF(" s%u(map=%u tc=%u cc=%06X ac=%06X)", s,
           (unsigned)g_gxregs.tev[s].tex_map, (unsigned)g_gxregs.tev[s].tex_coord,
           (unsigned)g_gxregs.bp[0xC0 + s * 2], (unsigned)g_gxregs.bp[0xC1 + s * 2]);
  NJ_PRINTF(" | prog=%u linked=%d skip=%d | tex0=%08X %ux%u f%u%s",
         (unsigned)s_active_shader.id, (int)linked, (int)s_drop_draw,
         (unsigned)t0.data_addr, (unsigned)t0.width, (unsigned)t0.height,
         (unsigned)t0.format, efb_tex ? " EFBCOPY" : "");
  NJ_PRINTF(" | nch=%08X amb0=%08X mat0=%08X ctl0=%08X", XfBits(0x09),
         XfBits(0x0A), XfBits(0x0C), XfBits(0x0E));
  const uint32_t ctl0 = XfBits(0x0E);
  if (ctl0 & 2) { // lit: the colours of the lights in its mask
    const uint32_t lmask = ((ctl0 >> 2) & 0xF) | (((ctl0 >> 11) & 0xF) << 4);
    NJ_PRINTF(" L:");
    for (int i = 0; i < 8; ++i) {
      if (!(lmask & (1u << i)))
        continue;
      uint32_t c = 0;
      std::memcpy(&c, &g_gxregs.xfm.light[i][3], 4);
      NJ_PRINTF(" %d=%08X", i, (unsigned)c);
    }
  }
  if (!cmd.verts().empty() && cmd.verts()[0].has_color) {
    const auto &c0 = cmd.verts()[0].color;
    NJ_PRINTF(" vcol=%.2f,%.2f,%.2f,%.2f", c0[0], c0[1], c0[2], c0[3]);
  }
  // Vertex 0 and its live position matrix: a view matrix whose rotation rows
  // are not orthonormal points at the CPU math that built it, a sane one at
  // the vertex data or the projection.
  if (!cmd.verts().empty()) {
    const auto &v0 = cmd.verts()[0];
    NJ_PRINTF(" | v0 idx=%u prexf=%d pos=(%.1f,%.1f,%.1f)", (unsigned)v0.posMtxIdx,
           (int)v0.pretransformed, v0.pos[0], v0.pos[1], v0.pos[2]);
    {   // coordonnees de texture des trois premiers sommets et generation (XF 0x1040.., 0x1050..) : sol uniforme (natif, 12/11)
      const auto &v = cmd.verts();
      NJ_PRINTF(" | tc has=%d", (int)v0.has_tex[0]);
      for (size_t k = 0; k < v.size() && k < 3; ++k)
        NJ_PRINTF(" (%.3f,%.3f)", v[k].tex[0][0], v[k].tex[0][1]);
      NJ_PRINTF(" gen=%08X/%08X post=%08X/%08X", XfBits(0x40), XfBits(0x41), XfBits(0x50), XfBits(0x51));
    }
    const unsigned mb = (unsigned)v0.posMtxIdx * 4;
    if (mb + 12 <= 256) {
      const float *m = &g_gxregs.xfm.pos[mb];
      auto len = [](const float *r) {
        return std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
      };
      const float d01 = m[0] * m[4] + m[1] * m[5] + m[2] * m[6];
      NJ_PRINTF(" mtx=[%.3f %.3f %.3f %.1f | %.3f %.3f %.3f %.1f | %.3f %.3f %.3f %.1f]"
             " |r|=%.3f,%.3f,%.3f r0.r1=%.3f",
             m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10],
             m[11], len(m), len(m + 4), len(m + 8), d01);
    }
  }
  NJ_PRINTF("\n");
}

static void draw_prim(const GxOp &cmd) {
  const auto &v = cmd.verts();
  const size_t n = std::size(v);
  // sommets tires (A1) ; liste compilee (V2) : variante portee par la liste
  const uint16_t var = !(cmd.pull && pull::on()) ? 0 : cmd.list ? cmd.list->variant : pull::variant(cmd);
  if (n == 0 && !var)
    return;

  // La matrice fait desormais partie de l'etat du lot : deux dessins qui
  // n'ont pas la meme ne peuvent plus etre regroupes. Mesure prealable :
  // 328 changements par image seulement, le flux etant deja groupe par
  // matrice - d'ou le choix d'un simple uniforme plutot qu'un tableau
  // indexe par le sommet.
  const bool shader_uses_mtx = (s_active_shader.loc[kUPosMtx] >= 0 || s_active_shader.loc[kUNrmMtx] >= 0);
  const bool mtx_changed = shader_uses_mtx &&
      (std::memcmp(s_batch_xf_pos, cmd.xf_pos, sizeof s_batch_xf_pos) != 0 ||
       std::memcmp(s_batch_xf_nrm, cmd.xf_nrm, sizeof s_batch_xf_nrm) != 0);
  const bool tidx_changed = s_batch_tidx0 != DrawTexIdx0(cmd);
  // Classe de primitive : quads, triangles, bandes et eventails finissent tous en triangles
  // indexes, seuls les lignes et les points changent de mode GL (couper a chaque type GX faisait
  // ~415 lots de plus par image au Galaxy S5).
  auto prim_class = [](uint8_t p) -> uint8_t {
    return p == 0xB8 ? 2 : (p == 0xA8 || p == 0xB0) ? 1 : 0;
  };
  const bool prim_changed = prim_class(s_pend_prim) != prim_class(cmd.prim_type);
  const bool var_changed = s_batch_variant != var;
  if (!s_pend_open || prim_changed || mtx_changed || tidx_changed || var_changed) {
    // P3 : pourquoi l'etat est repose (journal [GLCALLS]) : lot ferme par une commande d'etat, ou matrice seule, ...
    if (!s_pend_open) ++g_why_closed;
    else if (!prim_changed && !tidx_changed && !var_changed) ++g_why_mtx;
    else ++g_why_other;
    // La primitive l'emporte dans le comptage : quand les deux changent, le
    // vidage aurait eu lieu de toute facon, donc l'attribuer a la matrice
    // surestimerait le cout du deport.
    if (s_pend_open && prim_changed)
      ++g_prof_flush_prim;
    else if (s_pend_open && mtx_changed)
      ++g_prof_flush_mtx;
    submit_pending();
    std::memcpy(s_batch_xf_pos, cmd.xf_pos, sizeof s_batch_xf_pos);
    std::memcpy(s_batch_xf_nrm, cmd.xf_nrm, sizeof s_batch_xf_nrm);
    s_batch_tidx0 = DrawTexIdx0(cmd);
    {
      rprof::Scope rp(rprof::SETUP);
      ++rprof::cnt_setup;
      g_jr_setup.fetch_add(1, std::memory_order_relaxed);
      // P3a : pose legere si seuls texture / melange (ou la matrice) ont change depuis la derniere pose complete.
      static const bool s_full_state = NJ_ENV("NJ_FULLSTATE") != nullptr;
      const bool light = !s_full_state && s_full_ok && !s_drop_draw && !(s_dirty & kDirtyOther) && !prim_changed &&
                         !tidx_changed && !var_changed;
      if (light) {
        bind_light(cmd, s_dirty);
        ++g_bind_light;
      } else {
        s_batch_variant = var;
        g_vs_pull_variant = var;   // lu par le generateur et la cle du programme (bind_draw_state)
        bind_draw_state(cmd);
        g_vs_pull_variant = 0;
        s_full_ok = !s_drop_draw;
        ++g_bind_full;
      }
      s_dirty = 0;
    }
    s_pend_open = true;
    s_pend_prim = (uint8_t)cmd.prim_type;
    const GLint S = nj::runtime::gfx::g_scale;
    if (s_pend_prim == 0xA8 || s_pend_prim == 0xB0) {
      s_batch_glmode = GL_LINES;
      // Epaisseur bornee a ce que le pilote accepte (profil core : souvent 1 seulement).
      static float s_lw_max = 0.0f;
      if (s_lw_max == 0.0f) {
        GLfloat r[2] = {1.0f, 1.0f};
        glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, r);
        s_lw_max = r[1] >= 1.0f ? r[1] : 1.0f;
      }
      float w = (float)(g_gxregs.bp[0x22] & 0xFF) / 6.0f * (float)S;
      w = w < 1.0f ? 1.0f : (w > s_lw_max ? s_lw_max : w);
      glLineWidth(w);
    } else if (s_pend_prim == 0xB8) {
      s_batch_glmode = GL_POINTS;
#if !defined(NJ_GLES)
      const float ps = (float)((g_gxregs.bp[0x22] >> 8) & 0xFF) / 6.0f * (float)S;
      glPointSize(ps < 1.0f ? 1.0f : ps);
#endif
    } else {
      s_batch_glmode = GL_TRIANGLES;
    }
  }
  if (s_framelog)
    LogDraw(cmd);
  if (s_drop_draw)
    return; // programme de cet etat pas encore pret
  ++rprof::cnt_draws;
  if (var) {
    if (cmd.list) pull::draw_list(cmd, s_active_shader.id, s_batch_glmode);   // tout de suite : etat deja pose
    else pull::append(cmd);
    return;
  }
  if (resident::on()) {
    resident::pend.push_back(&cmd);
    resident::pend_ok = resident::pend_ok && cmd.geom_hash != 0;
    resident::pend_hash = (resident::pend_hash ^ cmd.geom_hash) * 0x100000001B3ull;
    return;
  }
  rprof::Scope rp(rprof::EMIT);
  push_prim(cmd);
}

// Emplacements des uniformes d'un programme fraichement lie : nom dans le GLSL genere et nom de
// secours (uniforme scalaire au lieu d'un tableau).
static void LocateUniforms(Program &p) {
  struct Name {
    UniformRole role;
    const char *name, *alt;
  };
  const Name names[] = {
      {kUProjMtx, "mvp", nullptr},
      {kUTevColor, "uTevColor[0]"_g, "uTevColor"_g},
      {kUTevKColor, "uTevKColor[0]"_g, "uTevKColor"_g},
      {kUTGRow, "uTGRow[0]"_g, nullptr},
      {kUTGPost, "uTGPost[0]"_g, nullptr},
      {kUTMBank, "uTMBank[0]"_g, nullptr},
      {kUIndMtx, "uIndMtx[0]"_g, nullptr},
      {kUTexSize, "uTexSize[0]"_g, nullptr},
      {kUMatColor, "uMatColor[0]"_g, nullptr},
      {kUAmbColor, "uAmbColor[0]"_g, nullptr},
      {kUChanCtrl, "uChanCtrl[0]"_g, nullptr},
      {kULightCol, "uLightCol[0]"_g, nullptr},
      {kULightPos, "uLightPos[0]"_g, nullptr},
      {kULightDir, "uLightDir[0]"_g, nullptr},
      {kULightCosAtt, "uLightCosAtt[0]"_g, nullptr},
      {kULightDistAtt, "uLightDistAtt[0]"_g, nullptr},
      {kUFogF, "uFogF"_g, nullptr},
      {kUFogI, "uFogI"_g, nullptr},
      {kUFogColor, "uFogColor"_g, nullptr},
      {kUAlphaRef, "uAlphaRef"_g, nullptr},
      {kUPosMtx, "uPosMtx"_g, nullptr},
      {kUNrmMtx, "uNrmMtx"_g, nullptr},
  };
  for (const Name &n : names) {
    GLint l = glGetUniformLocation(p.id, n.name);
    if (l < 0 && n.alt)
      l = glGetUniformLocation(p.id, n.alt);
    p.loc[n.role] = l;
  }
  for (int i = 0; i < 8; ++i) {
    p.sampler[i] = glGetUniformLocation(p.id, ("uTex"_g + std::to_string(i)).c_str());
    p.texmtx[i] = glGetUniformLocation(p.id, ("uTexMtx[" + std::to_string(i) + "]").c_str());
  }
}

// Programme de l'etat courant : par cle d'etat, puis par source (plusieurs etats produisent le
// meme GLSL, chaque source n'est compilee qu'une fois), sinon compile dans le budget de l'image
// (12 programmes, 8 ms). Hors budget : false, le dessin est saute plutot que dessine avec le
// mauvais nuanceur (TEV, textures, uniformes faux) ; il apparait des que son programme est pret.
static bool SelectProgram(uint64_t key, uint8_t prim) {
  if (auto hit = s_shader_cache.find(key); hit != s_shader_cache.end()) {
    s_active_shader = hit->second;
    return true;
  }
  const auto src = build_tev_program(g_gxregs, prim);
  const uint64_t src_key = std::hash<std::string>{}(src.vs_text) * 1099511628211ull ^
                           std::hash<std::string>{}(src.fs_text);
  if (auto same = s_program_by_source.find(src_key); same != s_program_by_source.end()) {
    s_shader_cache[key] = same->second;
    s_active_shader = same->second;
    return true;
  }
  const bool over_budget =
      s_shader_jobs >= kShaderBudget ||
      (s_shader_jobs > 0 && s_compile_ms_this_frame >= kCompileBudgetMs);
  if (over_budget && !no_compile_budget() && !shader_warmup()) {
    ++s_diag_skips_total;
    return false;
  }
  ++s_shader_jobs;
  const auto t0 = std::chrono::steady_clock::now();
  Program prog;
  prog.id = link_program(src.vs_text.c_str(), src.fs_text.c_str());
  LocateUniforms(prog);
  s_shader_cache[key] = prog;
  s_program_by_source[src_key] = prog;
  s_active_shader = prog;
  s_compile_ms_this_frame +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  return true;
}

// Couleur RGBA8 du XF (rouge dans l'octet de poids fort) en quatre flottants 0..1.
static inline void UnpackRGBA8(uint32_t c, float *out) {
  const uint8_t b[4] = {uint8_t(c >> 24), uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c)};
  for (int k = 0; k < 4; ++k)
    out[k] = b[k] / 255.f;
}

// Canaux de couleur (XF 0x0A..0x11 : ambiantes, materiaux, controles) et huit lumieres (XF
// 0x0600 : couleur au mot 3, puis cosatt, distatt, position, direction, trois mots chacun),
// envoyes au programme sous forme de tableaux de vec4.
static void UploadChannelsAndLights(const Program &prog) {
  // XF 0x0A-0x0B : ambiantes des canaux 0 et 1 ; 0x0C-0x0D : leurs materiaux.
  float chan[2][8];   // [0] ambiantes, [1] materiaux
  for (int i = 0; i < 4; ++i)
    UnpackRGBA8(g_gxregs.xf[0x00A + i], chan[i >> 1] + (i & 1) * 4);
  if (prog.loc[kUAmbColor] >= 0)
    cu_Uniform4fv(prog.loc[kUAmbColor], 2, chan[0]);
  if (prog.loc[kUMatColor] >= 0)
    cu_Uniform4fv(prog.loc[kUMatColor], 2, chan[1]);
  if (prog.loc[kUChanCtrl] >= 0) {
    GLint ctrl[4];
    for (int k = 0; k < 4; ++k)
      ctrl[k] = (GLint)g_gxregs.xf[0x00E + k];
    cu_Uniform1iv(prog.loc[kUChanCtrl], 4, ctrl);
  }

  // Champs vectoriels d'une lumiere : role de l'uniforme et premier mot dans la lumiere.
  static const struct {
    UniformRole role;
    int word;
  } kFields[] = {{kULightCosAtt, 4}, {kULightDistAtt, 7}, {kULightPos, 10}, {kULightDir, 13}};
  float colors[32];
  float fields[4][32];
  for (int l = 0; l < 8; ++l) {
    const float *src = g_gxregs.xfm.light[l];
    uint32_t rgba;
    std::memcpy(&rgba, &src[3], 4);
    UnpackRGBA8(rgba, colors + l * 4);
    for (int f = 0; f < 4; ++f) {
      float *dst = fields[f] + l * 4;
      dst[0] = src[kFields[f].word];
      dst[1] = src[kFields[f].word + 1];
      dst[2] = src[kFields[f].word + 2];
      dst[3] = 0.f;
    }
  }
  if (prog.loc[kULightCol] >= 0)
    cu_Uniform4fv(prog.loc[kULightCol], 8, colors);
  for (int f = 0; f < 4; ++f)
    if (prog.loc[kFields[f].role] >= 0)
      cu_Uniform4fv(prog.loc[kFields[f].role], 8, fields[f]);
}

// Une couleur du TEV : deux registres BP (rouge/alpha puis bleu/vert), composantes signees sur 11
// bits a l'echelle 255.
static inline float TevComponent(uint32_t reg, int shift) {
  int v = (int)((reg >> shift) & 0x7FF);
  if (v & 0x400)
    v -= 0x800;
  return static_cast<float>(v) / 255.0f;
}
static inline void DecodeTevPair(uint32_t ra, uint32_t bg, float *rgba) {
  rgba[0] = TevComponent(ra, 0);
  rgba[1] = TevComponent(bg, 12);
  rgba[2] = TevComponent(bg, 0);
  rgba[3] = TevComponent(ra, 12);
}

// Tailles des textures liees (uTexSize) : depend seulement des registres de texture (et des copies EFB).
static void upload_texsize() {
  if (s_active_shader.loc[kUTexSize] < 0) return;
  float ts[8 * 4];
  for (int t = 0; t < 8; ++t) {
    const auto &st = g_gxregs.tex_units[t];
    ts[t * 4 + 0] = st.width ? (float)st.width : 1.0f;
    ts[t * 4 + 1] = st.height ? (float)st.height : 1.0f;
    // .zw : taille declaree / taille de la copie EFB (1 hors copie ; NJ_EFBSTRETCH : 1).
    static const bool s_efb_stretch = NJ_ENV("NJ_EFBSTRETCH") != nullptr;
    uint32_t cw = 0, ch = 0;
    if (!s_efb_stretch && st.width && st.height &&
        nj::runtime::hle::TexCache::get().efb_dims(st.data_addr, cw, ch) && cw && ch) {
      ts[t * 4 + 2] = (float)st.width / (float)cw;
      ts[t * 4 + 3] = (float)st.height / (float)ch;
    } else {
      ts[t * 4 + 2] = ts[t * 4 + 3] = 1.0f;
    }
  }
  cu_Uniform4fv(s_active_shader.loc[kUTexSize], 8, ts);
}

// P3a : pose legere - meme programme, memes uniformes ; seulement ce que touchent les categories marquees, plus les
// matrices du dessin. Appelee a la place de bind_draw_state quand seuls texture et/ou melange ont change.
static void bind_light(const GxOp &cmd, uint32_t dirty) {
  if (dirty & kDirtyPe) {
    set_depth_state();
    set_blend_state();
  }
  if (dirty & kDirtyTex) {
    upload_texsize();
    for (int unit = 0; unit < 8; ++unit)
      if (const GLint smp = s_active_shader.sampler[unit]; smp >= 0 && g_gxregs.tex_units[unit].data_addr)
        cu_Uniform1i(smp, unit);
  }
  if (const GLint at = s_active_shader.loc[kUPosMtx]; at >= 0) {
    float rows[16];
    std::memcpy(rows, cmd.xf_pos, 12 * sizeof(float));
    rows[12] = rows[13] = rows[14] = 0.f;
    rows[15] = 1.f;
    cu_UniformMatrix4fv(at, 1, GL_TRUE, rows);
  }
  if (const GLint at = s_active_shader.loc[kUNrmMtx]; at >= 0)
    cu_UniformMatrix3fv(at, 1, GL_TRUE, cmd.xf_nrm);
}

static void bind_draw_state(const GxOp &cmd) {
  const uint64_t key = g_gxregs.GetShaderHash(cmd.prim_type) ^ ((uint64_t)g_vs_pull_variant * 0x9E3779B97F4A7C15ull);
  if (!g_st_hash0)
    g_st_hash0 = key;
  g_st_shaders = (uint64_t)s_shader_cache.size();
  s_drop_draw = !SelectProgram(key, cmd.prim_type);
  if (s_drop_draw)
    return;

  cu_UseProgram(s_active_shader.id);
  // Les references du test alpha sont un uniforme depuis qu'elles ont ete
  // retirees de la source generee : RE4 les fait varier a chaque fondu, et
  // elles ne doivent donc plus multiplier les variantes de nuanceur.
  if (s_active_shader.loc[kUAlphaRef] >= 0)
    cu_Uniform2f(s_active_shader.loc[kUAlphaRef], g_gxregs.atest.ref[0] / 255.0f,
                g_gxregs.atest.ref[1] / 255.0f);
  // La transformation modele -> camera, quand le decodeur la laisse au
  // nuanceur (NJ_XFGPU=1). Identite sinon : les sommets arrivent deja
  // transformes et le nuanceur ne doit pas les transformer deux fois.
  set_depth_state();
  set_blend_state();
  set_cull_state();
  upload_proj(s_active_shader.loc[kUProjMtx]);
  if (const GLint at = s_active_shader.loc[kUPosMtx]; at >= 0) {
    float rows[16];   // XF : 3 lignes de 4 ; la 4e ligne (0 0 0 1) est ajoutee ici
    std::memcpy(rows, cmd.xf_pos, 12 * sizeof(float));
    rows[12] = rows[13] = rows[14] = 0.f;
    rows[15] = 1.f;
    cu_UniformMatrix4fv(at, 1, GL_TRUE, rows);
  }
  if (const GLint at = s_active_shader.loc[kUNrmMtx]; at >= 0)
    cu_UniformMatrix3fv(at, 1, GL_TRUE, cmd.xf_nrm);
  // unite de texture i -> echantillonneur i, pour les seules unites alimentees
  for (int unit = 0; unit < 8; ++unit)
    if (const GLint smp = s_active_shader.sampler[unit]; smp >= 0 && g_gxregs.tex_units[unit].data_addr)
      cu_Uniform1i(smp, unit);

  // L'etat de canal XF et les lumieres changent par image - fondus de
  // materiau, lumieres qui bougent - et non par dessin. Les reexpedier a
  // chacun des ~8 700 changements d'etat d'une image coutait 640 octets de
  // tableaux plus le deballage de 8 x 16 flottants, pour des valeurs
  // identiques. On ne les renvoie donc que si g_gxregs.xf_gen a bouge depuis le
  // dernier envoi **a ce programme** : en OpenGL les uniformes appartiennent au
  // programme, donc changer de nuanceur oblige a renvoyer.
  {
    static std::unordered_map<GLuint, uint32_t> s_prog_xf_gen;
    uint32_t &seen = s_prog_xf_gen[s_active_shader.id];
    const bool xf_dirty = (seen != g_gxregs.xf_gen);
    seen = g_gxregs.xf_gen;
    if (xf_dirty)
      UploadChannelsAndLights(s_active_shader);
  }

  // GX fog parameters (Dolphin BPMemory FogParams): A and C are 20-bit
  // floats (11-bit mantissa, 8-bit exponent, sign); both exponents at 255
  // is the NaN case, where A = 0 and C = -inf/+inf by the signs.
  if (s_active_shader.loc[kUFogF] >= 0) {
    const uint32_t f0 = g_gxregs.bp[0xEE], f3 = g_gxregs.bp[0xF1];
    auto fogfloat = [](uint32_t reg) {
      const uint32_t bits = (((reg >> 19) & 1u) << 31) |
                            (((reg >> 11) & 0xFFu) << 23) | ((reg & 0x7FFu) << 12);
      float f;
      std::memcpy(&f, &bits, 4);
      return f;
    };
    float a = fogfloat(f0), c = fogfloat(f3);
    if (((f0 >> 11) & 0xFF) == 255 && ((f3 >> 11) & 0xFF) == 255) {
      a = 0.0f;
      c = (!((f0 >> 19) & 1) && !((f3 >> 19) & 1)) ? -1e30f : 1e30f;
    }
    static const bool s_nofog = NJ_ENV("NJ_NOFOG") != nullptr;   // essai : brouillard coupe (sol uniforme du natif, 12/11)
    if (s_nofog) {   // facteur k = clamp(ze - c) : c tres grand -> k = 0, aucun brouillard
      a = 0.0f;
      c = 1e30f;
    }
    if (s_framelog) {   // les registres du brouillard tels que le jeu les a poses
      static int logged = 0;
      if (logged++ < 6)
        NJ_PRINTF("[FL] brouillard EE=%06X EF=%06X F0=%06X F1=%06X F2=%06X -> a=%g c=%g\n", (unsigned)(f0 & 0xFFFFFF),
                  (unsigned)(g_gxregs.bp[0xEF] & 0xFFFFFF), (unsigned)(g_gxregs.bp[0xF0] & 0xFFFFFF),
                  (unsigned)(f3 & 0xFFFFFF), (unsigned)(g_gxregs.bp[0xF2] & 0xFFFFFF), a, c);
    }
    cu_Uniform4f(s_active_shader.loc[kUFogF], a, c, 0.0f, 0.0f);
    if (s_active_shader.loc[kUFogI] >= 0)
      cu_Uniform4i(s_active_shader.loc[kUFogI], 0, (GLint)(g_gxregs.bp[0xEF] & 0xFFFFFF), 0,
                  (GLint)(g_gxregs.bp[0xF0] & 0x1F));
    if (s_active_shader.loc[kUFogColor] >= 0) {
      const uint32_t fc = g_gxregs.bp[0xF2];
      cu_Uniform4f(s_active_shader.loc[kUFogColor], ((fc >> 16) & 0xFF) / 255.f,
                  ((fc >> 8) & 0xFF) / 255.f, (fc & 0xFF) / 255.f, 1.0f);
    }
  }

  // Matrices de texture du nuanceur : identite (les generateurs prennent les leurs dans la banque).
  static const float kIdentity4[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  for (GLint loc : s_active_shader.texmtx)
    if (loc >= 0)
      cu_UniformMatrix4fv(loc, 1, GL_FALSE, kIdentity4);
  // Generateurs de coordonnees : pour chacun, les 3 lignes de sa matrice dans la banque XF
  // (index par defaut, MATINDEX A/B -> defTexMtxIdx, adresse = index x 4) et celles de sa
  // post-matrice (XF 0x0500 + index x 4, POSTMTXINFO bits 0-5). Une matrice jamais chargee
  // (tout a zero) vaut l'identite, comme le faisait l'ancien generateur.
  // Banque XF (64 lignes) vue par les generateurs a index par sommet : une matrice d'index
  // < 30 (matrice de position) ou jamais chargee (tout a zero) lue comme l'identite, la regle
  // du chemin par lot. Seuls les index multiples de 3 designent une matrice (GX_TEXMTX0..9 =
  // 30..57, GX_IDENTITY = 60).
  if (s_active_shader.loc[kUTMBank] >= 0) {
    // Reconstruite a chaque changement de la banque, envoyee a chaque programme qui ne l'a pas
    // (un programme GL garde ses uniformes) : ni copie ni comparaison de 1 Ko par dessin.
    static float bank[64 * 4];
    static uint64_t bank_gen = 0;
    static std::unordered_map<GLuint, uint64_t> prog_gen;
    if (bank_gen != s_xf_bank_gen) {
      std::memcpy(bank, g_gxregs.xfm.pos, sizeof bank);
      for (int idx = 0; idx + 3 <= 64; idx += 3) {
        bool any = false;
        for (int k = 0; k < 12; ++k)
          any |= g_gxregs.xfm.pos[idx * 4 + k] != 0.0f;
        if (idx < 30 || !any) {
          float *m = &bank[idx * 4];
          std::memset(m, 0, 12 * sizeof(float));
          m[0] = m[5] = m[10] = 1.0f;
        }
      }
      bank_gen = s_xf_bank_gen;
    }
    uint64_t &pg = prog_gen[s_active_shader.id];
    if (pg != bank_gen) {
      glUniform4fv(s_active_shader.loc[kUTMBank], 64, bank);
      pg = bank_gen;
    }
  }
  if (s_active_shader.loc[kUTGRow] >= 0 || s_active_shader.loc[kUTGPost] >= 0) {
    float rows[24 * 4], post[24 * 4];
    const int ntg = g_gxregs.n_texgen > 8 ? 8 : g_gxregs.n_texgen;
    for (int i = 0; i < 8; ++i) {
      float *r = &rows[i * 12];
      float *q = &post[i * 12];
      std::memset(r, 0, 12 * sizeof(float));
      std::memset(q, 0, 12 * sizeof(float));
      r[0] = r[5] = r[10] = 1.0f;
      q[0] = q[5] = q[10] = 1.0f;
      if (i >= ntg)
        continue;
      // Generateur 0 : l'index porte par les sommets (attribut TEX0MTXIDX, ou le defaut
      // recopie au decodage) ; les autres : l'index par defaut (MATINDEX A/B).
      const uint32_t idx = i == 0 ? s_batch_tidx0 : g_gxregs.mtx_idx_tex[i];
      const uint32_t base = idx * 4;
      const bool proj = (g_gxregs.xf[0x40 + i] >> 1) & 1;
      const uint32_t n = proj ? 12 : 8;
      // Index < 30 : matrice de position de la banque (modeles animes : l'index par defaut
      // n'est pas celui d'une matrice de texture) -> identite, comme l'ancien generateur.
      if (idx >= 30 && base + n <= 256) {
        bool any = false;
        for (uint32_t k = 0; k < n; ++k)
          any |= g_gxregs.xfm.pos[base + k] != 0.0f;
        if (any)
          std::memcpy(r, &g_gxregs.xfm.pos[base], n * sizeof(float));
      }
      const uint32_t pbase = (g_gxregs.xf[0x50 + i] & 0x3F) * 4;
      if (pbase + 12 <= 256) {
        bool any = false;
        for (uint32_t k = 0; k < 12; ++k)
          any |= g_gxregs.xfm.post[pbase + k] != 0.0f;
        if (any)
          std::memcpy(q, &g_gxregs.xfm.post[pbase], 12 * sizeof(float));
      }
    }
    cu_Uniform4fv(s_active_shader.loc[kUTGRow], 24, rows);
    if (s_active_shader.loc[kUTGPost] >= 0)
      cu_Uniform4fv(s_active_shader.loc[kUTGPost], 24, post);
  }
  // Matrices indirectes (BP 0x06..0x0E, 3 matrices x 3 mots) : elements signes sur 11 bits
  // (/1024), exposant sur 6 bits reparti en 2+2+2 bits (valeur - 17). Tailles des textures.
  if (s_active_shader.loc[kUIndMtx] >= 0) {
    float im[6 * 4];
    for (int m = 0; m < 3; ++m) {
      const uint32_t a = g_gxregs.bp[0x06 + m * 3], b = g_gxregs.bp[0x07 + m * 3],
                     c = g_gxregs.bp[0x08 + m * 3];
      auto s11 = [](uint32_t v) {
        int x = (int)(v & 0x7FF);
        if (x & 0x400) x -= 0x800;
        return (float)x / 1024.0f;
      };
      const int e = (int)(((a >> 22) & 3) | (((b >> 22) & 3) << 2) | (((c >> 22) & 3) << 4)) - 17;
      float *r0 = &im[m * 8], *r1 = &im[m * 8 + 4];
      r0[0] = s11(a); r0[1] = s11(b); r0[2] = s11(c); r0[3] = std::ldexp(1.0f, e);
      r1[0] = s11(a >> 11); r1[1] = s11(b >> 11); r1[2] = s11(c >> 11); r1[3] = 0.0f;
    }
    cu_Uniform4fv(s_active_shader.loc[kUIndMtx], 6, im);
  }
  upload_texsize();

  // Registres de couleur et constantes, gardes separement (gx_state.h, tev_reg / tev_konst) : lus
  // depuis bp[], la derniere ecriture effacait l'autre jeu et le glow de RE4 (filter00, (TEXC -
  // C0) x 2) perdait son seuil C0 : Leon blanchi, « transparent » (salle 3-17).
  float kcolor[16], color[16];
  for (int r = 0; r < 4; ++r) {
    DecodeTevPair(g_gxregs.tev_reg[r * 2], g_gxregs.tev_reg[r * 2 + 1], color + r * 4);
    DecodeTevPair(g_gxregs.tev_konst[r * 2], g_gxregs.tev_konst[r * 2 + 1], kcolor + r * 4);
  }
  if (s_active_shader.loc[kUTevKColor] >= 0)
    cu_Uniform4fv(s_active_shader.loc[kUTevKColor], 4, kcolor);
  if (s_active_shader.loc[kUTevColor] >= 0)
    cu_Uniform4fv(s_active_shader.loc[kUTevColor], 4, color);
}

// Une primitive GX dans le lot : chaque sommet une fois, puis ses indices. Quads en deux triangles
// (0 1 2, 2 3 0), bandes en alternant le sens pour garder l'orientation, eventails autour du
// premier sommet ; lignes, bandes de lignes et points tels quels. Type inconnu : rien.
static void push_prim(const GxOp &cmd) {
  const uint8_t prim = (uint8_t)cmd.prim_type;
  if (prim < 0x80 || prim > 0xB8 || (prim & 7) != 0 || prim == 0x88)
    return;
  const auto &verts = cmd.verts();
  const uint32_t count = (uint32_t)verts.size();
  const uint32_t first = (uint32_t)s_pend.size();
  s_pend.reserve(s_pend.size() + count);
  s_indices.reserve(s_indices.size() + count * 3);
  for (const auto &vtx : verts)
    put_vertex(vtx);
  auto tri = [&](uint32_t a, uint32_t b, uint32_t c) {
    s_indices.push_back(first + a);
    s_indices.push_back(first + b);
    s_indices.push_back(first + c);
  };
  switch (prim) {
  case 0x80:   // quads
    for (uint32_t q = 0; q + 3 < count; q += 4) {
      tri(q, q + 1, q + 2);
      tri(q + 2, q + 3, q);
    }
    break;
  case 0x90:   // triangles
    for (uint32_t t = 0; t + 2 < count; t += 3)
      tri(t, t + 1, t + 2);
    break;
  case 0x98:   // bande de triangles
    for (uint32_t k = 2; k < count; ++k) {
      if (k % 2)
        tri(k - 1, k - 2, k);
      else
        tri(k - 2, k - 1, k);
    }
    break;
  case 0xA0:   // eventail
    for (uint32_t k = 2; k < count; ++k)
      tri(0, k - 1, k);
    break;
  case 0xA8:   // lignes
    for (uint32_t k = 0; k + 1 < count; k += 2) {
      s_indices.push_back(first + k);
      s_indices.push_back(first + k + 1);
    }
    break;
  case 0xB0:   // bande de lignes
    for (uint32_t k = 1; k < count; ++k) {
      s_indices.push_back(first + k - 1);
      s_indices.push_back(first + k);
    }
    break;
  default:     // 0xB8 : points
    for (uint32_t k = 0; k < count; ++k)
      s_indices.push_back(first + k);
    break;
  }
}

// Effacement qui suit une copie de l'EFB (bit 11 de BP 0x52) : seulement le rectangle copie, et
// seulement les plans dont l'ecriture est permise (couleur, alpha si le format en a, profondeur),
// comme Dolphin (ClearScreen). Tout effacer noircissait la scene de RE4 des sa premiere petite
// copie (128 x 128).
static void ClearCopyRect() {
  const uint32_t mode = g_gxregs.bp[0x41], zmode = g_gxregs.bp[0x40];
  const bool has_alpha = (g_gxregs.bp[0x43] & 7) == 1;
  const GLboolean rgb = (mode & 0x08) ? GL_TRUE : GL_FALSE;
  const GLboolean alpha = ((mode & 0x10) && has_alpha) ? GL_TRUE : GL_FALSE;
  glColorMask(rgb, rgb, rgb, alpha);
  glDepthMask((zmode & 0x10) ? GL_TRUE : GL_FALSE);
  uint8_t rgba[4];
  ::gx_clear_rgba(rgba);
  glClearColor(rgba[0] / 255.f, rgba[1] / 255.f, rgba[2] / 255.f, rgba[3] / 255.f);
  const GLint S = nj::runtime::gfx::g_scale;
  const auto &c = g_gxregs.copy;
  nj_cap(GL_SCISSOR_TEST, true);
  glScissor((GLint)c.x * S, (480 - (GLint)(c.y + c.h)) * S, (GLsizei)c.w * S, (GLsizei)c.h * S);
  nj_wipe(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  nj_cap(GL_SCISSOR_TEST, false);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  nj_depth_write();
}

// Copie de l'EFB vers une texture de la memoire du jeu (BP 0x52 sans le bit 14) : la texture GL
// est rangee sous l'adresse de destination, et les lectures de texture a cette adresse la
// prennent directement (fonds de l'ecran titre, portraits, effets). Le rectangle de la copie part
// du coin haut gauche alors que l'EFB est une image GL a l'endroit ; la ligne 0 de la texture est
// le haut du rectangle (comme pour les textures du jeu decodees). Bit 9 : copie reduite de moitie.
static GLuint s_frame_efb_fbo = 0;   // EFB de l'image en cours (draw_frame)
static GLint s_copy_swizzle[4];       // aiguillage pose sur la texture de la copie en cours
static void CopyEfbToTexture(uint32_t val) {
  const uint32_t dest = g_gxregs.copy.dest, w = g_gxregs.copy.w, h = g_gxregs.copy.h;
  if (!dest || !w || !h || w > 1024 || h > 1024)
    return;
  const bool half = (val & 0x200) != 0;
  // Densite de la copie. Fidele (defaut) : celle du GameCube, quelle que soit la
  // resolution interne S. Les flous, glows, profondeur de champ, vision thermique de
  // RE4 naissent de la reduction de ces copies (docs/EFFETS_RESOLUTION.md) : a S fois
  // plus de texels, ils retrecissaient de S et leurs passes decalees se dedoublaient
  // (defaut identique dans Dolphin, « Scaled EFB Copy »). NJ_COPYHR=1 : copies a
  // la resolution interne, comme avant (plus nettes, flous trop faibles).
  const GLint S = nj::runtime::gfx::g_scale;
  static const bool s_copy_hr = NJ_ENV("NJ_COPYHR") != nullptr;
  const GLint Sc = s_copy_hr ? S : 1;
  const GLsizei tw = (half ? (GLsizei)(w > 1 ? w / 2 : 1) : (GLsizei)w) * Sc;
  const GLsizei th = (half ? (GLsizei)(h > 1 ? h / 2 : 1) : (GLsizei)h) * Sc;
  GLuint tex =
      nj::runtime::hle::TexCache::get().efb_slot(dest);
  bool fresh_tex = false;
  if (!tex) {
    tex = nj_new_tex();
    fresh_tex = true;   // un nom recycle d'une texture detruite : pas de stockage
  }
  nj_tex2d(tex);
  // Reallouer la texture a chaque copie (une vingtaine par image) forcait le pilote a
  // lui trouver un nouveau stockage a chaque fois ; on garde celui qui a deja la
  // bonne taille. NJ_COPYREALLOC=1 : l'ancien comportement, pour comparer.
  static std::unordered_map<GLuint, uint64_t> s_copy_size;
  static const bool s_copy_realloc = NJ_ENV("NJ_COPYREALLOC") != nullptr;
  const uint64_t want_size = ((uint64_t)tw << 32) | (uint32_t)th;
  auto cs = s_copy_size.find(tex);
  if (s_copy_realloc || fresh_tex || cs == s_copy_size.end() || cs->second != want_size) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    s_copy_size[tex] = want_size;
  }
  nj_tex_filter(GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // Target pixel format (bits 3-6, Dolphin tp_realFormat). RE4's
  // depth-of-field chain copies only the alpha channel (A8) and then
  // samples the copy as an intensity texture; keeping RGB made the
  // composite use the scene colour instead of the blurred alpha.
  {
    const uint32_t tpf = (val >> 3) & 0xF;
    const uint32_t fmt = tpf / 2 + (tpf & 1) * 8;
    GLint sw[4] = {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA};
    switch (fmt) {
    case 0: case 1: case 8: // R4, R8, R8: one channel read as I
      sw[0] = sw[1] = sw[2] = sw[3] = GL_RED;
      break;
    case 2: case 3: // RA4, RA8
      sw[0] = sw[1] = sw[2] = GL_RED;
      break;
    case 4: // RGB565: no alpha
      sw[3] = GL_ONE;
      break;
    case 7: // A8
      sw[0] = sw[1] = sw[2] = sw[3] = GL_ALPHA;
      break;
    case 9: // G8
      sw[0] = sw[1] = sw[2] = sw[3] = GL_GREEN;
      break;
    case 10: // B8
      sw[0] = sw[1] = sw[2] = sw[3] = GL_BLUE;
      break;
    default: // RGB5A3, RGBA8, RG8, GB8: as rendered
      break;
    }
#ifdef NJ_GLES
    // ES 3.0 n'a que les quatre parametres separes.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, sw[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, sw[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, sw[2]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, sw[3]);
#else
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, sw);
#endif
    std::memcpy(s_copy_swizzle, sw, sizeof sw);   // relu par les jumelles, sans demander au pilote
  }
  {
    auto tw_it = s_copy_twin.find(tex);
    if (tw_it != s_copy_twin.end())
      tw_it->second.valid = false;
    twin_stat();
  }
  // L'EFB lie au debut de l'image (draw_frame). Le redemander au pilote a chaque copie forcait le
  // pilote multifil de NVIDIA a vider sa file : ~1 ms par copie apres 17 000 dessins, 9 a 10 ms
  // par image au village (mesure du 07/10).
  const GLint efb_fbo = (GLint)s_frame_efb_fbo;
  static GLuint s_copy_fbo = 0;
  if (!s_copy_fbo)
    glGenFramebuffers(1, &s_copy_fbo);
  nj_fbo(GL_DRAW_FRAMEBUFFER, s_copy_fbo);
  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, tex, 0);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)efb_fbo);
  const GLint x0 = (GLint)g_gxregs.copy.x * S;
  const GLint y_top = (480 - (GLint)g_gxregs.copy.y) * S;
  const GLint rw = (GLint)w * S, rh = (GLint)h * S;   // zone source, en pixels GL
  // Copie de la PROFONDEUR : le SDK passe le format de pixel en Z24 (BP 0x43, bits 0-2
  // = 3) juste avant une copie vers un format Z (GXCopyTex, cpTexZ). La profondeur de
  // champ de RE4 (filter02) copie ainsi le Z en Z8 et le relit en C8 a travers une
  // palette : copier la couleur a la place faisait disparaitre l'effet. Le z 24 bits
  // est ecrit en octets (haut, milieu, bas) dans R, G, B : l'aiguillage de canaux
  // ci-dessus choisit Z8 / Z8M / Z8L / Z16 comme pour les formats couleur.
  const bool depth_copy = (g_gxregs.bp[0x43] & 7) == 3;
  if (depth_copy) {
    static GLuint s_zt_tex = 0, s_zt_fbo = 0, s_zconv = 0;
    static GLint s_zt_w = 0, s_zt_h = 0;
    if (!s_zt_fbo) {
      glGenFramebuffers(1, &s_zt_fbo);
      s_zt_tex = nj_new_tex();
      const std::string hdr = nj::runtime::gx::glsl_header_string();
      const std::string vs = hdr + R"GLSL(
out vec2 uv;
void main() {
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  uv = p;
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";
      const std::string fs = hdr + R"GLSL(
in vec2 uv;
uniform sampler2D zt;
out vec4 o;
void main() {
  float d = clamp(texture(zt, uv).r, 0.0, 1.0);
  uint z = uint(d * 16777215.0 + 0.5);
  o = vec4(float((z >> 16u) & 255u), float((z >> 8u) & 255u), float(z & 255u), 255.0) / 255.0;
}
)GLSL";
      s_zconv = link_program(vs.c_str(), fs.c_str());
    }
    nj_tex2d(s_zt_tex);
    if (tw != s_zt_w || th != s_zt_h) {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, tw, th, 0, GL_DEPTH_COMPONENT,
                   GL_UNSIGNED_INT, nullptr);
      nj_tex_filter(GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
      s_zt_w = tw;
      s_zt_h = th;
    }
    nj_fbo(GL_DRAW_FRAMEBUFFER, s_zt_fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           s_zt_tex, 0);
    {
      const GLenum none = GL_NONE;
      glDrawBuffers(1, &none);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)efb_fbo);
    nj_inval_draw(GL_DEPTH_ATTACHMENT);
    glBlitFramebuffer(x0, y_top - rh, x0 + rw, y_top, 0, th, tw, 0,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    // z -> octets dans la texture de la copie
    nj_fbo(GL_FRAMEBUFFER, s_copy_fbo);
    nj_inval_draw(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, tw, th);
    nj_cap(GL_DEPTH_TEST, false);
    nj_cap(GL_BLEND, false);
    nj_cap(GL_SCISSOR_TEST, false);
    nj_cap(GL_CULL_FACE, false);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(s_zconv);
    glActiveTexture(GL_TEXTURE0);
    nj_tex2d(s_zt_tex);
    static const GLint s_zt_loc = glGetUniformLocation(s_zconv, "zt");   // une fois
    glUniform1i(s_zt_loc, 0);
    glBindVertexArray(s_va);
#ifndef NJ_GLES
    nj_cap(GL_CLIP_DISTANCE0, false);   // ce shader n'ecrit pas de distance de decoupe
    nj_cap(GL_CLIP_DISTANCE1, false);
#endif
    glDrawArrays(GL_TRIANGLES, 0, 3);
    // etat du rendu GX
    s_cur_prog = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)efb_fbo);
    nj_fbo(GL_DRAW_FRAMEBUFFER, s_copy_fbo);
    ApplyViewport();
    set_depth_state();
    set_blend_state();
    set_cull_state();
    nj_tex2d(tex);
  } else if (Sc < S && ((GLint)tw * 2 <= rw || (GLint)th * 2 <= rh)) {
    // Reduction par moities successives (NJ_OLDREDUCE=1 : l'ancienne, mipmaps de toute la
    // zone). Chaque moitie est une copie lineaire 2:1, dont chaque pixel tombe au milieu de
    // 2 x 2 texels : moyenne exacte, comme un niveau de mipmap, mais seulement jusqu'au
    // niveau utile, et dans des textures gardees d'une image a l'autre (une par taille).
    // L'ancienne reallouait sa texture a chaque changement de taille et calculait toute la
    // chaine a chaque copie : 4x 9,8 i/s contre 28,5 sans reduction (salle 1-00).
    static const bool s_old_reduce = NJ_ENV("NJ_OLDREDUCE") != nullptr;
    if (!s_old_reduce) {
      struct RedLevel { GLuint tex = 0, fbo = 0; };
      static std::unordered_map<uint64_t, RedLevel> s_red_pool;
      auto level_of = [](GLint w, GLint h) -> RedLevel & {
        RedLevel &L = s_red_pool[((uint64_t)w << 32) | (uint32_t)h];
        if (!L.tex) {
          glGenTextures(1, &L.tex);
          glGenFramebuffers(1, &L.fbo);
          glBindTexture(GL_TEXTURE_2D, L.tex);
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                       nullptr);
          nj_tex_filter(GL_LINEAR);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
          glBindFramebuffer(GL_DRAW_FRAMEBUFFER, L.fbo);
          glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                 L.tex, 0);
        }
        return L;
      };
      static const bool s_no_twin2 = NJ_ENV("NJ_NOCOPYTWIN") != nullptr;
      const GLint hw = (GLint)tw * S, hh = (GLint)th * S;
      int hl = 0;
      while ((rw >> (hl + 1)) >= hw && (rh >> (hl + 1)) >= hh)
        ++hl;
      GLuint src_fbo = (GLuint)efb_fbo;
      GLint sx = x0, sy = y_top - rh, sw = rw, sh = rh;
      // Jumelle haute resolution (voir CopyTwin) : l'etage de la chaine a S x la taille.
      auto make_twin = [&]() {
        GLint swz[4] = {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA};
        nj_tex2d(tex);
        std::memcpy(swz, s_copy_swizzle, sizeof swz);   // pose par nous juste avant, pas relu
        CopyTwin &twn = twin_for(tex, sw, sh);
        glBindTexture(GL_TEXTURE_2D, twn.tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, swz[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, swz[1]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, swz[2]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, swz[3]);
        static GLuint s_twin_fbo2 = 0;
        if (!s_twin_fbo2)
          glGenFramebuffers(1, &s_twin_fbo2);
        nj_fbo(GL_READ_FRAMEBUFFER, src_fbo);
        nj_fbo(GL_DRAW_FRAMEBUFFER, s_twin_fbo2);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               twn.tex, 0);
        nj_inval_draw(GL_COLOR_ATTACHMENT0);
        glBlitFramebuffer(sx, sy, sx + sw, sy + sh, 0, sh, sw, 0, GL_COLOR_BUFFER_BIT,
                          GL_NEAREST);
        twn.valid = true;
      };
      int level = 0;
      if (!s_no_twin2 && hl == 0)
        make_twin();
      while ((sw >> 1) >= (GLint)tw && (sh >> 1) >= (GLint)th) {
        const GLint nw = sw >> 1, nh = sh >> 1;
        RedLevel &L = level_of(nw, nh);
        nj_fbo(GL_READ_FRAMEBUFFER, src_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, L.fbo);
        nj_inval_draw(GL_COLOR_ATTACHMENT0);
        glBlitFramebuffer(sx, sy, sx + sw, sy + sh, 0, 0, nw, nh, GL_COLOR_BUFFER_BIT,
                          GL_LINEAR);
        src_fbo = L.fbo;
        sx = sy = 0;
        sw = nw;
        sh = nh;
        ++level;
        if (!s_no_twin2 && level == hl)
          make_twin();
      }
      nj_fbo(GL_READ_FRAMEBUFFER, src_fbo);
      nj_fbo(GL_DRAW_FRAMEBUFFER, s_copy_fbo);
      nj_inval_draw(GL_COLOR_ATTACHMENT0);
      glBlitFramebuffer(sx, sy, sx + sw, sy + sh, 0, th, tw, 0, GL_COLOR_BUFFER_BIT,
                        GL_LINEAR);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)efb_fbo);
      nj_tex2d(tex);
    } else {
      // (en 1x, ou en copies haute resolution : la copie lineaire d'origine, inchangee)
      // Reduction d'un facteur >= 2 : une copie lineaire directe ne lirait que 2 x 2
      // pixels sur S x S (scintillement). Moyenne exacte : la zone a pleine resolution
      // dans une texture temporaire, ses mipmaps (moyennes 2 x 2 successives), puis le
      // niveau le plus proche au-dessus de la taille visee, recopie lineairement.
      static GLuint s_red_tex = 0, s_red_fbo = 0;
      static GLint s_red_w = 0, s_red_h = 0;
      if (!s_red_tex) {
        s_red_tex = nj_new_tex();
        glGenFramebuffers(1, &s_red_fbo);
      }
      nj_tex2d(s_red_tex);
      if (rw != s_red_w || rh != s_red_h) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rw, rh, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        nj_tex_mag(GL_LINEAR);
        s_red_w = rw;
        s_red_h = rh;
      }
      nj_fbo(GL_DRAW_FRAMEBUFFER, s_red_fbo);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                             s_red_tex, 0);
      nj_inval_draw(GL_COLOR_ATTACHMENT0);
      glBlitFramebuffer(x0, y_top - rh, x0 + rw, y_top, 0, 0, rw, rh,
                        GL_COLOR_BUFFER_BIT, GL_NEAREST);
      glGenerateMipmap(GL_TEXTURE_2D);
      // Jumelle haute resolution : niveau de la zone le plus proche de S x la taille de
      // la copie (niveau 1 pour une copie a mi-taille), meme retournement vertical.
      static const bool s_no_twin = NJ_ENV("NJ_NOCOPYTWIN") != nullptr;
      if (!s_no_twin) {
        const GLint hw = (GLint)tw * S, hh = (GLint)th * S;
        int hl = 0;
        while ((rw >> (hl + 1)) >= hw && (rh >> (hl + 1)) >= hh)
          ++hl;
        const GLint sw_ = (rw >> hl) > 0 ? (rw >> hl) : 1;
        const GLint sh_ = (rh >> hl) > 0 ? (rh >> hl) : 1;
        // Meme selection de canaux que la copie (formats I8, A8... : relue sur `tex`).
        GLint swz[4] = {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA};
        nj_tex2d(tex);
        std::memcpy(swz, s_copy_swizzle, sizeof swz);   // pose par nous juste avant, pas relu
        CopyTwin &twn = twin_for(tex, sw_, sh_);
        glBindTexture(GL_TEXTURE_2D, twn.tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, swz[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, swz[1]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, swz[2]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, swz[3]);
        static GLuint s_twin_fbo = 0;
        if (!s_twin_fbo)
          glGenFramebuffers(1, &s_twin_fbo);
        nj_fbo(GL_READ_FRAMEBUFFER, s_red_fbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               s_red_tex, hl);
        nj_fbo(GL_DRAW_FRAMEBUFFER, s_twin_fbo);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               twn.tex, 0);
        nj_inval_draw(GL_COLOR_ATTACHMENT0);
        glBlitFramebuffer(0, 0, sw_, sh_, 0, sh_, sw_, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        twn.valid = true;
        nj_tex2d(s_red_tex);
      }
      int lvl = 0;
      while ((rw >> (lvl + 1)) >= (GLint)tw && (rh >> (lvl + 1)) >= (GLint)th)
        ++lvl;
      nj_fbo(GL_READ_FRAMEBUFFER, s_red_fbo);
      glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                             s_red_tex, lvl);
      nj_fbo(GL_DRAW_FRAMEBUFFER, s_copy_fbo);
      const GLint lw = (rw >> lvl) > 0 ? (rw >> lvl) : 1;
      const GLint lh = (rh >> lvl) > 0 ? (rh >> lvl) : 1;
      nj_inval_draw(GL_COLOR_ATTACHMENT0);
      glBlitFramebuffer(0, 0, lw, lh, 0, th, tw, 0, GL_COLOR_BUFFER_BIT, GL_LINEAR);
      // etat attendu par la suite : lecture sur l'EFB, ecriture dans la copie
      glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                             s_red_tex, 0);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)efb_fbo);
      nj_tex2d(tex);
    }
  } else {
    nj_inval_draw(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(x0, y_top - rh, x0 + rw, y_top, 0, th,
                      tw, 0, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  }
  // NJ_DUMPCOPIES=<dir>: during the F12 frame log, write every EFB
  // copy (raw RGBA as stored, before the format swizzle) as a 32-bit
  // TGA, to compare with Dolphin's DumpEFBTarget output.
  static const char *s_dumpcopies = NJ_ENV("NJ_DUMPCOPIES");
  if (s_framelog && s_dumpcopies) {
    static int copy_n = 0;
    std::vector<unsigned char> px((size_t)tw * (size_t)th * 4);
    nj_fbo(GL_READ_FRAMEBUFFER, s_copy_fbo);
    glReadPixels(0, 0, tw, th, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    const uint32_t tpf_d = (val >> 3) & 0xF;
    char name[512];
    std::snprintf(name, sizeof(name), "%s/copy_%03d_%dx%d_f%u_%08X.tga",
                  s_dumpcopies, copy_n++, (int)tw, (int)th,
                  (unsigned)(tpf_d / 2 + (tpf_d & 1) * 8), (unsigned)dest);
    if (FILE *tf = std::fopen(name, "wb")) {
      const unsigned char hdr[18] = {
          0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          (unsigned char)(tw & 0xFF), (unsigned char)(tw >> 8),
          (unsigned char)(th & 0xFF), (unsigned char)(th >> 8), 32, 8};
      std::fwrite(hdr, 1, sizeof(hdr), tf);
      for (size_t i = 0; i < px.size(); i += 4)
        std::swap(px[i], px[i + 2]); // RGBA -> BGRA; rows are bottom-up
      std::fwrite(px.data(), 1, px.size(), tf);
      std::fclose(tf);
    }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)efb_fbo);
  nj::runtime::hle::TexCache::get().register_efb_texture(
      dest, tex, (uint32_t)(tw / Sc), (uint32_t)(th / Sc), (uint32_t)tw, (uint32_t)th);
  if (val & 0x800) {
    ClearCopyRect();
    g_gxregs.clear_pending = false;
  }
}

class GlRenderer : public FrameRenderer {
public:

  void attach(void *) override {}

  void draw_frame(const std::vector<GxOp> &commands) override {
    s_dirty |= kDirtyOther;   // P3a : l'etat GL a pu bouger entre deux images (presentation, copies) : pose complete
    s_clean_persp = 0;
    s_clean_done = false;
    s_full_ok = false;
    ++nj::runtime::hle::g_tex_frame;   // empreintes de textures reprises dans cette image seulement (texture_cache_gl.cpp)
    rprof::frame_begin();
    {   // reglages en direct (knobs.txt) : une lecture par image (voir FrameKnobs)
      static const char *ds = NJ_ENV("NJ_DRAWSTART");
      static const char *de = NJ_ENV("NJ_DRAWSTOP");
      s_fk.nounicache = nj::dev::knob("nounicache", 0);
      s_fk.drawstart = nj::dev::knob("drawstart", ds ? atoi(ds) : 0);
      s_fk.drawstop = nj::dev::knob("drawstop", de ? atoi(de) : -1);
      s_fk.logdraw = nj::dev::knob("logdraw", -1);
      s_fk.texnearest = nj::dev::knob("texnearest", 0);
    }
    rprof::Scope rp_total(rprof::TOTAL);
#ifdef NJ_GLES
    // Filtre des etats GL redondants (common/gl.h), pour cette image seulement. NJ_GLSHADOW=1 : actif. Mesure du 14/11
    // au chateau (S5, A/B/A) : 2 230 appels evites par image, AUCUN gain mesurable (8-10 i/s dans les trois essais) ;
    // laisse coupe par defaut tant que l'image n'a pas ete comparee au pixel.
    static const bool s_gls = [] { const char *e = NJ_ENV("NJ_GLSHADOW"); return e && e[0] == '1'; }();
    struct GlsScope { explicit GlsScope(bool on) { ::nj::gls::begin(on); } ~GlsScope() { ::nj::gls::end(); } } gls_scope(s_gls);
#endif
#if defined(NJ_GLES) && !defined(__APPLE__)
    gputime::Scope gpu_scope;   // NJ_GPUTIME=1 : temps GPU de l'image
#endif
    pull::preload(commands);   // tableaux de sommets de l'image : un envoi groupe avant les dessins
    {  // EFB lie par la boucle hote : une seule question au pilote par image, file encore vide
      GLint fb = 0;
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb);
      s_frame_efb_fbo = (GLuint)fb;
    }
#ifdef NJ_GLES
    // [GLCOUNT] (avec NJ_FRAMELOG) : moyenne par image, sur 30 images, des appels GL comptes par
    // common/gl.h, presentation comprise (main.cpp).
    {
      static const bool on = NJ_ENV("NJ_FRAMELOG") != nullptr;
      static int frames = 0;
      if (on && ++frames == 30) {
        namespace c = nj::glc;
        NJ_PRINTF("[GLCOUNT] par image : bindfb=%.1f clear=%.1f draw=%.1f/%.1f teximage=%.1f "
                    "texsub=%.1f tex_ko=%.0f readpixels=%.1f finish=%.1f flush=%.1f clientwait=%.1f "
                    "fence=%.1f blit=%.1f copytexsub=%.1f invalidate=%.1f useprogram=%.1f "
                    "bindtexture=%.1f uniform=%.1f enable/disable=%.1f texparam=%.1f autre_etat=%.1f evites=%.1f map=%.1f\n",
                    c::n[c::BindFB] / 30.0, c::n[c::Clear] / 30.0, c::n[c::DrawElems] / 30.0,
                    c::n[c::DrawArrays] / 30.0, c::n[c::TexImage] / 30.0, c::n[c::TexSubImage] / 30.0,
                    c::tex_bytes / 30.0 / 1024.0, c::n[c::ReadPixels] / 30.0, c::n[c::Finish] / 30.0,
                    c::n[c::Flush] / 30.0, c::n[c::ClientWait] / 30.0, c::n[c::Fence] / 30.0,
                    c::n[c::Blit] / 30.0, c::n[c::CopyTexSub] / 30.0, c::n[c::Invalidate] / 30.0,
                    c::n[c::UseProgram] / 30.0, c::n[c::BindTexture] / 30.0, c::n[c::Uniform] / 30.0,
                    c::n[c::EnableDisable] / 30.0, c::n[c::TexParam] / 30.0, c::n[c::OtherState] / 30.0,
                    c::n[c::Skipped] / 30.0, c::n[c::Map] / 30.0);
        for (auto &v : c::n) v = 0;
        c::tex_bytes = 0;
        frames = 0;
      }
    }
#endif
    s_cur_prog = 0;   // ImGui et le calque tactile changent de programme entre deux images
    s_framelog = g_gx_framelog_request.exchange(false);
    s_draw_index = 0;
    if (s_framelog) {
      s_framelog_draws = s_framelog_onscreen = s_framelog_offscreen = 0;
      s_framelog_unlinked = s_framelog_skipped = 0;
      unsigned char cc[4] = {};
      ::gx_clear_rgba(cc);
      NJ_PRINTF("[FrameLog] begin: %u commands clear_pending=%d clear=%02X%02X%02X%02X "
             "proj type=%u set=%d [%.4f %.4f %.4f %.4f %.4f %.4f]\n",
             (unsigned)commands.size(), (int)g_gxregs.clear_pending, cc[0],
             cc[1], cc[2], cc[3], (unsigned)g_gxregs.proj.type,
             (int)g_gxregs.proj.set, g_gxregs.proj.m[0], g_gxregs.proj.m[1],
             g_gxregs.proj.m[2], g_gxregs.proj.m[3], g_gxregs.proj.m[4],
             g_gxregs.proj.m[5]);
      // Every psq_l/psq_st in the SDK matrix library uses GQR0 and PSMTXConcat
      // reads its {0,1} constant from memory: either one wrong skews all the
      // model-view matrices the game builds.
      if (g_guest) {
        NJ_PRINTF("[FrameLog] gqr=");
        for (int i = 0; i < 8; ++i)
          NJ_PRINTF("%08X%s", (unsigned)g_guest->gqr[i], i < 7 ? "," : "");
        NJ_PRINTF(" Unit01@80314598=(%g,%g)\n",
               g_guest->mmu.read_f32(nj::runtime::ga(0x80314598)), g_guest->mmu.read_f32(nj::runtime::ga(0x8031459C)));
      }
    }

    // Clears must not inherit the write mask the last draw left behind:
    // the host clear of depth wants everything, GX clears follow PE_CMODE0.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    nj_depth_write();
    nj_wipe(GL_DEPTH_BUFFER_BIT);
    s_shader_jobs = 0; // reset the per-frame shader compile budget
    DiagFrame();
    s_compile_ms_this_frame = 0.0;
    ApplyViewport(); // the host loop resets the GL viewport every frame
    if (g_gxregs.clear_pending) { // deferred clear of the last XFB copy
      g_gxregs.clear_pending = false;
      ClearCopyRect();
    }

    // Ecriture BP sans effet : registre de pur etat (pas un declencheur - copie, effacement,
    // jeton, chargement de palette ou de texture - ni un registre de couleur TEV, dont la valeur
    // se range dans deux tableaux) et valeur identique a celle en place. Elle ne coupe plus le lot :
    // bp_apply ne change alors rien a l'etat. ~13 600 ecritures BP par image au Galaxy S5,
    // une bonne part redondantes. NJ_BPFLUSHALL=1 : chaque ecriture coupe le lot, comme avant.
    static const bool s_bp_flush_all = NJ_ENV("NJ_BPFLUSHALL") != nullptr;
    auto bp_noop = [](const GxOp &c) -> bool {
      if (s_bp_flush_all || c.type != GxOpKind::BPRegister) return false;
      const uint8_t r = (uint8_t)c.reg;
      const bool pure = r == 0x00 || (r >= 0x06 && r <= 0x0E) || (r >= 0x10 && r <= 0x44) ||
                        (r >= 0x59 && r <= 0x5A) || (r >= 0x80 && r <= 0xBB) ||
                        (r >= 0xC0 && r <= 0xDF) || (r >= 0xE8 && r <= 0xFD);
      return pure && g_gxregs.bp[r] == c.val;
    };
    for (const GxOp &cmd : commands) {

      // P3a : ecriture XF qui ne change rien (le jeu renvoie tout son materiau a chaque partie : ~8 000 par image au
      // chateau) : ni traitement, ni fermeture de lot. NJ_XFNOOPOFF=1 : comme avant (A/B).
      if (cmd.type == GxOpKind::XFRegister && xf_noop(cmd))
        continue;
      // Meme chose pour BP : registre « pur » (sans effet de bord) reecrit a l'identique -> rien a appliquer.
      static const bool s_bp_apply_all = NJ_ENV("NJ_BPAPPLYALL") != nullptr;
      if (cmd.type == GxOpKind::BPRegister && !s_bp_apply_all && bp_noop(cmd)) {
        ++g_bp_noop_skip;
        continue;
      }
      if (cmd.type != GxOpKind::DrawPrimitive && cmd.type != GxOpKind::CPRegister && !bp_noop(cmd)) {
        if (s_pend_open) {   // P3 : qui ferme un lot ouvert
          if (cmd.type == GxOpKind::BPRegister) { ++g_close_bp; ++g_close_bpreg[(uint8_t)cmd.reg]; }
          else if (cmd.type == GxOpKind::XFRegister) ++g_close_xf;
          else ++g_close_cp;
        }
        uint32_t cls = kDirtyOther;   // P3a : categorie de l'ecriture
        if (cmd.type == GxOpKind::BPRegister) {
          const uint8_t r = (uint8_t)cmd.reg;
          if (r >= 0x80 && r <= 0xBB) cls = kDirtyTex;          // modes, images, TLUT des 8 unites de texture
          else if (r == 0x64 || r == 0x65) cls = kDirtyTex;     // chargement de palette (TLUT) : textures relues
          else if (r >= 0x40 && r <= 0x42) cls = kDirtyPe;      // profondeur, melange, alpha de destination
          else if (r == 0x0F || (r >= 0x30 && r <= 0x3F)) cls = 0;   // lus par personne ici (echelles de coordonnees)
          else ++g_other_bpreg[r];
        } else if (cmd.type == GxOpKind::XFRegister) {
          ++g_other_xf[cmd.reg < 0x400 ? 0 : cmd.reg < 0x500 ? 1 : cmd.reg < 0x600 ? 2 : cmd.reg < 0x1000 ? 3 : 4];
        }
        CloseBatchClassified(cls);
      }
      if (cmd.type == GxOpKind::XFRegister) {
        rprof::Scope rp_xf(rprof::XF);
        const XfTouched touched = WriteXF(cmd.reg, cmd.payload);
        if (touched.viewport) {
          ApplyViewport();
        }
        if (touched.projection) {
          g_gxregs.proj.set = true;
          upload_proj(s_active_shader.loc[kUProjMtx]);
          if (s_framelog)
            NJ_PRINTF("[FL] projection type=%u [%.4f %.4f %.4f %.4f %.4f %.4f]\n",
                   (unsigned)g_gxregs.proj.type, g_gxregs.proj.m[0],
                   g_gxregs.proj.m[1], g_gxregs.proj.m[2],
                   g_gxregs.proj.m[3], g_gxregs.proj.m[4],
                   g_gxregs.proj.m[5]);
        }
      } else if (cmd.type == GxOpKind::BPRegister) {
        // Lectures de profondeur du jeu, mesure par point (efb_peek.h) : la scene est terminee,
        // la copie vers l'image affichee va effacer l'EFB. Tous les points demandes pendant
        // l'image sont lus ici.
        if (cmd.reg == 0x52 && (cmd.val & 0x4000) && !nj::runtime::gx::peek::rank_mode()) {
          std::vector<std::pair<int, int>> fpts;
          if (nj::runtime::gx::peek::frame_points(fpts)) {
            CloseBatch();
#if !defined(NJ_GLES)
            static const bool s_sync_peek_f = NJ_ENV("NJ_SYNCPEEK") != nullptr;
            if (!s_sync_peek_f) {
              peek_async_issue(-1, fpts);
            } else {
              const GLint S = nj::runtime::gfx::g_scale;
              std::vector<float> dv(fpts.size(), 1.0f);
              for (size_t i = 0; i < fpts.size(); ++i)
                glReadPixels(fpts[i].first * S + S / 2, (480 - fpts[i].second) * S - S / 2 - 1, 1, 1,
                             GL_DEPTH_COMPONENT, GL_FLOAT, &dv[i]);
              nj::runtime::gx::peek::render_store(-1, fpts, dv);
            }
#endif
          }
        }
        rprof::Scope rp_bp(cmd.reg == 0x52 ? rprof::COPY : rprof::BP);
        bpprof::Scope bpp((uint8_t)cmd.reg);
        ++rprof::cnt_bp;
        if (cmd.reg == 0x52) {
          ++rprof::cnt_copies;
          if (cmd.val & 0x4000) ++s_rp_xfb;   // copie vers l'image affichee : une image du jeu
        }
        bp_apply((uint8_t)cmd.reg, cmd.val);
        if (cmd.reg == 0x40u) {
          set_depth_state();
        }
        // Lectures de profondeur du jeu (GXPeekZ, runtime/gx/efb_peek.h) : au meme rang de
        // GXDrawDone que dans l'image precedente, lire les points qu'il a demandes.
#if !defined(NJ_GLES)
        // Lectures de profondeur sans attente (01/10) : glReadPixels vers la memoire attendait
        // que le GPU ait tout dessine jusque-la, a chaque test de lumiere du jeu (chateau 8x :
        // ~13 ms par image dans « bp »). Les points partent dans un tampon GPU avec une barriere ;
        // on les reprend quand le GPU a fini, sans jamais l'attendre. Le jeu recevait deja la
        // mesure de l'image d'avant (efb_peek.h). NJ_SYNCPEEK=1 : l'ancienne lecture.
        static const bool s_sync_peek = NJ_ENV("NJ_SYNCPEEK") != nullptr;
        if (!s_sync_peek && cmd.reg == 0x45 && (cmd.val & 0xF) == 2) {
          peek_async_collect(false);
          int rank = 0;
          std::vector<std::pair<int, int>> pts;
          if (nj::runtime::gx::peek::rank_mode() && nj::runtime::gx::peek::render_points(rank, pts))
            peek_async_issue(rank, pts);
        } else if (!s_sync_peek && cmd.reg == 0x52 && (cmd.val & 0x4000)) {
          peek_async_collect(false);
          nj::runtime::gx::peek::render_xfb_copy();
        } else
#endif
        if (cmd.reg == 0x45 && (cmd.val & 0xF) == 2 && nj::runtime::gx::peek::rank_mode()) {
          nj::runtime::gx::peek::render_finish([](int x, int y) {
            const GLint S = nj::runtime::gfx::g_scale;
            GLfloat d = 1.0f;
#if !defined(NJ_GLES)
            glReadPixels(x * S + S / 2, (480 - y) * S - S / 2 - 1, 1, 1, GL_DEPTH_COMPONENT,
                         GL_FLOAT, &d);
#else
            (void)x; (void)y; (void)S;   // GLES ne lit pas la profondeur : le fond (visible)
#endif
            return (float)d;
          });
        } else if (cmd.reg == 0x52 && (cmd.val & 0x4000)) {
          nj::runtime::gx::peek::render_xfb_copy();
        }
        // Copies de l'EFB : journal de l'image (NJ_FRAMELOG), puis copie vers une texture.
        if (s_framelog && cmd.reg == 0x52) {
          // What the copy is about to read: the centre of its source rect.
          CloseBatch();
          unsigned char px[4] = {0, 0, 0, 0};
          glReadPixels((GLint)(g_gxregs.copy.x + g_gxregs.copy.w / 2) * nj::runtime::gfx::g_scale,
                       (GLint)(480 - (g_gxregs.copy.y + g_gxregs.copy.h / 2)) * nj::runtime::gfx::g_scale, 1, 1,
                       GL_RGBA, GL_UNSIGNED_BYTE, px);
          NJ_PRINTF("[FL] EFB centre before copy rgba=%02X%02X%02X%02X bp41=%06X bp42=%06X "
                 "bp43=%06X\n",
                 px[0], px[1], px[2], px[3], (unsigned)g_gxregs.bp[0x41],
                 (unsigned)g_gxregs.bp[0x42], (unsigned)g_gxregs.bp[0x43]);
        }
        if (s_framelog && cmd.reg == 0x52)
          NJ_PRINTF("[FL] EFB copy val=%06X %s clear=%d src=(%u,%u) size=%ux%u "
                 "dest=%08X filtre bp53=%06X bp54=%06X ech=%06X/%06X/%06X/%06X\n",
                 (unsigned)cmd.val, (cmd.val & 0x4000) ? "to-XFB" : "to-texture",
                 (int)((cmd.val & 0x800) != 0), (unsigned)g_gxregs.copy.x,
                 (unsigned)g_gxregs.copy.y, (unsigned)g_gxregs.copy.w,
                 (unsigned)g_gxregs.copy.h, (unsigned)g_gxregs.copy.dest,
                 (unsigned)g_gxregs.bp[0x53] & 0xFFFFFF, (unsigned)g_gxregs.bp[0x54] & 0xFFFFFF,
                 (unsigned)g_gxregs.bp[0x01] & 0xFFFFFF, (unsigned)g_gxregs.bp[0x02] & 0xFFFFFF,
                 (unsigned)g_gxregs.bp[0x03] & 0xFFFFFF, (unsigned)g_gxregs.bp[0x04] & 0xFFFFFF);
        if (cmd.reg == 0x52u && (cmd.val & 0x4000u) == 0) {
          CloseBatch();   // les dessins en attente font partie de ce qui est copie
          CopyEfbToTexture(cmd.val);
        }
      } else if (cmd.type == GxOpKind::DrawPrimitive) {
        if (nj_clean_frame_hook && !s_clean_done) {   // image propre : avant le premier dessin 2D apres la 3D
          if (CurrentIsPerspective()) ++s_clean_persp;
          else if (s_clean_persp > 50) { s_clean_done = true; nj_clean_frame_hook(); }
        }
        // Plage d'objets, comme celle du FIFO Player de Dolphin : ne
        // rasteriser que les primitives [NJ_DRAWSTART, NJ_DRAWSTOP[. Tout
        // le reste du flux (etat, copies EFB) est traite normalement, sinon la
        // comparaison avec la reference ne porterait plus sur la meme scene.
        // C'est le dichotomiseur de l'oracle : « le sol est noir » devient
        // « le dessin n' X diverge ».
        {
          const int first = s_fk.drawstart;   // knobs.txt : bisection en direct (lu par image, draw_frame)
          const int last = s_fk.drawstop;
          const int idx = s_draw_index++;
          if (idx == s_fk.logdraw) LogDraw(cmd);
          if (idx < first || (last >= 0 && idx >= last)) {
            CloseBatch();
            continue;
          }
        }

        if (!s_pend_open && g_guest) {
          auto &texcache = nj::runtime::hle::TexCache::get();
          static const bool tex_nearest_env = NJ_ENV("NJ_TEXNEAREST") != nullptr;
          const bool tex_nearest = tex_nearest_env || s_fk.texnearest != 0;
          // Sampler state last applied to each GL texture, so rebinding the
          // same texture with the same GX mode costs no GL calls.
          static std::unordered_map<GLuint, uint64_t> s_sampler_sig;
          static uint64_t s_sampler_gen = 0;
          if (s_sampler_gen != texcache.generation()) {
            s_sampler_gen = texcache.generation();
            s_sampler_sig.clear(); // freed names may now belong to new textures
          }
          // Unites que le nuanceur de ce dessin lit vraiment (tev_shader_gen.cpp : etages TEV actifs
          // avec une texture, etages indirects). Les autres gardent ce qui y etait lie : rien ne les
          // echantillonne. Avant, les 8 unites etaient cherchees dans le cache a chaque lot
          // (~2550 recherches pour ~320 dessins par image au banc). NJ_TEXALLUNITS=1 : l'ancien parcours.
          static const bool s_all_units = NJ_ENV("NJ_TEXALLUNITS") != nullptr;
          uint32_t used_units = 0xFFu;
          if (!s_all_units) {
            used_units = 0;
            const int stages = g_gxregs.n_tev ? g_gxregs.n_tev : 1;
            for (int s = 0; s < stages && s < 16; ++s)
              if (g_gxregs.tev[s].tex_map != 0xFF) used_units |= 1u << (g_gxregs.tev[s].tex_map & 7);
            const int indirect = (int)((g_gxregs.bp[0x00] >> 16) & 7);
            for (int k = 0; k < indirect && k < 4; ++k) used_units |= 1u << ((g_gxregs.bp[0x27] >> (6 * k)) & 7);
          }
          for (int i = 0; i < 8; ++i) {
            const auto &stage = g_gxregs.tex_units[i];
            if (!(used_units & (1u << i)))
              continue;
            if (stage.data_addr == 0 || stage.width == 0 || stage.height == 0)
              continue;
            // TexMode0/1 for stages 0-3 at BP 0x80/0x84, stages 4-7 at 0xA0/0xA4.
            const uint32_t tm0 = g_gxregs.bp[(i < 4 ? 0x80 : 0xA0) + (i & 3)];
            const uint32_t tm1 = g_gxregs.bp[(i < 4 ? 0x84 : 0xA4) + (i & 3)];
            const uint32_t min_filter = (tm0 >> 5) & 7; // Dolphin TexMode0
            const bool min_linear = (min_filter & 4) != 0;
            const uint32_t mip_mode = min_filter & 3;   // 0 none, 1 near, 2/3 linear
            const float max_lod = ((tm1 >> 8) & 0xFF) / 16.0f;
            const float min_lod = (tm1 & 0xFF) / 16.0f;
            uint32_t levels = 1;
            if (mip_mode != 0 && !tex_nearest) {
              uint32_t full = 1, dim = std::max(stage.width, stage.height);
              while (dim > 1) { dim >>= 1; ++full; }
              levels = std::min(full, 1u + (uint32_t)max_lod);
            }
            GLuint tex = texcache.get_texture(*g_guest, stage, levels);
            const bool efb = texcache.find_efb_texture(stage.data_addr) == tex;
            // Dessin en perspective : la jumelle haute resolution de la copie, si elle existe.
            if (efb && CurrentIsPerspective()) {
              auto tw_it = s_copy_twin.find(tex);
              if (tw_it != s_copy_twin.end() && tw_it->second.valid)
                tex = tw_it->second.tex;
            }
            nj_unit(i);
            nj_tex2d(tex);
#if defined(NJ_DEVTOOLS)
            if (s_draw_index - 1 == s_fk.logdraw) {   // knobs.txt : ce que ce dessin lie (journal, evenement « dd »)
              char b[300];
              std::snprintf(b, sizeof b, "dd #%d u%d addr=%08X fmt=%u %ux%u efb=%d lv=%u/%u tm0=%06X tm1=%06X nv=%u persp=%d", s_draw_index - 1, i, stage.data_addr,
                            (unsigned)stage.format, stage.width, stage.height, (int)efb, texcache.levels_of(tex), levels, tm0 & 0xFFFFFF, tm1 & 0xFFFFFF,
                            (unsigned)cmd.verts().size(), (int)CurrentIsPerspective());
              nj::perf::event(b);
              if (i == 0) {   // etat du TEV + texte du nuanceur de fragments, dans dd_fs.txt (dossier de donnees)
                std::snprintf(b, sizeof b, "dd #%d tev=%u texgen=%u genmode=%06X ind=%u iref=%06X sc=%06X/%06X", s_draw_index - 1, (unsigned)g_gxregs.n_tev,
                              (unsigned)g_gxregs.n_texgen, g_gxregs.bp[0] & 0xFFFFFF, (g_gxregs.bp[0] >> 16) & 7, g_gxregs.bp[0x27] & 0xFFFFFF,
                              g_gxregs.bp[0x25] & 0xFFFFFF, g_gxregs.bp[0x26] & 0xFFFFFF);
                nj::perf::event(b);
                const auto sp = nj::runtime::gx::build_tev_program(g_gxregs, 0);
                std::ofstream f(nj::writable_path("dd_fs.txt"), std::ios::trunc);
                f << sp.vs_text << "\n//-----\n" << sp.fs_text;
                {   // matrice et premier sommet : valeurs brutes (objet dessine avec la mauvaise matrice ?)
                  const auto &vv = cmd.verts();
                  const float *m = cmd.xf_gpu ? cmd.xf_pos : g_gxregs.xfm.pos + (vv.empty() ? 0 : (int)(vv[0].posMtxIdx & 63) * 4);
                  std::snprintf(b, sizeof b, "dm #%d xfgpu=%d nv=%u v0=%.1f,%.1f,%.1f mi=%d pre=%d M=%.3g,%.3g,%.3g,%.1f|%.3g,%.3g,%.3g,%.1f|%.3g,%.3g,%.3g,%.1f",
                                s_draw_index - 1, (int)cmd.xf_gpu, (unsigned)vv.size(), vv.empty() ? 0.f : vv[0].pos[0], vv.empty() ? 0.f : vv[0].pos[1],
                                vv.empty() ? 0.f : vv[0].pos[2], vv.empty() ? -1 : (int)vv[0].posMtxIdx, vv.empty() ? -1 : (int)vv[0].pretransformed, m[0], m[1], m[2],
                                m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11]);
                  nj::perf::event(b);
                  std::snprintf(b, sizeof b, "dm #%d proj type=%d set=%d P=%.3g,%.3g,%.3g,%.3g,%.3g,%.3g", s_draw_index - 1, (int)g_gxregs.proj.type,
                                (int)g_gxregs.proj.set, g_gxregs.proj.m[0], g_gxregs.proj.m[1], g_gxregs.proj.m[2], g_gxregs.proj.m[3], g_gxregs.proj.m[4],
                                g_gxregs.proj.m[5]);
                  nj::perf::event(b);
                }
              }
            }
#endif
            // NJ_BINDLOG : ce qui est vraiment lie a chaque unite. Sonde de
            // l oracle FIFO, a combiner avec NJ_DRAWSTART/STOP pour isoler.
            static const bool s_bindlog = NJ_ENV("NJ_BINDLOG") != nullptr;
            if (s_bindlog)
              NJ_PRINTF("[BIND] dessin=%d unite=%d addr=%08X fmt=%u %ux%u gl=%u efb=%d tm0=%06X tm1=%06X "
                     "niveaux=%u/%u\n",
                     s_draw_index, i, stage.data_addr, (unsigned)stage.format,
                     stage.width, stage.height, tex, (int)efb, tm0 & 0xFFFFFF, tm1 & 0xFFFFFF,
                     texcache.levels_of(tex), levels);
            const uint64_t sig = (((uint64_t)tm0 << 32) | tm1 | ((uint64_t)tex_nearest << 63)) ^
                                 ((uint64_t)nj::runtime::gfx::g_aniso << 58);
            auto sit = s_sampler_sig.find(tex);
            if (sit != s_sampler_sig.end() && sit->second == sig)
              continue;
            s_sampler_sig[tex] = sig;
            const GLint uploaded = (GLint)texcache.levels_of(tex);
            GLint gl_min, gl_mag;
            if (tex_nearest) {
              gl_min = gl_mag = GL_NEAREST;
            } else {
              gl_mag = ((tm0 >> 4) & 1) ? GL_LINEAR : GL_NEAREST;
              if (mip_mode == 0 || uploaded <= 1)
                gl_min = min_linear ? GL_LINEAR : GL_NEAREST;
              else if (mip_mode == 1)
                gl_min = min_linear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST;
              else
                gl_min = min_linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;
            }
            // Texture HD (textures_hd/) : ses mips a elle, trilineaire, sans les bornes de LOD
            // ecrites pour la chaine GameCube (plus courte).
            const bool hd_tex = texcache.is_hd(tex);
            if (hd_tex && !tex_nearest)
              gl_min = GL_LINEAR_MIPMAP_LINEAR;
            auto wrap = [](uint32_t m) -> GLint {
              return m == 0 ? GL_CLAMP_TO_EDGE : (m == 2 ? GL_MIRRORED_REPEAT : GL_REPEAT);
            };
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_min);
            nj_tex_mag(gl_mag);
            if (!efb) { // EFB copies keep their own clamp and single level
              glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap(tm0 & 3));
              glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap((tm0 >> 2) & 3));
              // Menu F10 « Anisotropic filtering » : textures du jeu filtrees en lineaire
              // seulement (GL_TEXTURE_MAX_ANISOTROPY, GL 4.6 / EXT_texture_filter_anisotropic).
              static const float s_aniso_max = [] {
                float m = 0.0f;
                glGetFloatv(0x84FF /* GL_MAX_TEXTURE_MAX_ANISOTROPY */, &m);
                while (glGetError() != GL_NO_ERROR) {
                }
                return m;
              }();
              if (s_aniso_max >= 1.0f) {
                const float a = (gl_min == GL_NEAREST || gl_min == GL_NEAREST_MIPMAP_NEAREST)
                                    ? 1.0f
                                    : std::min((float)nj::runtime::gfx::g_aniso, s_aniso_max);
                glTexParameterf(GL_TEXTURE_2D, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, a);
              }
            }
            if (uploaded > 1) {
#ifndef NJ_GLES   // GL ES n'a pas GL_TEXTURE_LOD_BIAS (GL_INVALID_ENUM a chaque texture, 16/10)
              static constexpr GLenum kTextureLodBias = 0x8501; // GL_TEXTURE_LOD_BIAS
              const float bias = (float)(int8_t)((tm0 >> 9) & 0xFF) / 32.0f;
              glTexParameterf(GL_TEXTURE_2D, kTextureLodBias, bias);
#endif
              glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, hd_tex ? 0.0f : min_lod);
              glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, hd_tex ? 1000.0f : max_lod);
            }
          }
        }
        draw_prim(cmd);
      }
    }
    CloseBatch();
    // Hand the host loop default GL write masks: it presents, dumps and
    // clears the EFB after Render returns. A leftover colour mask from the
    // last draw leaked into those paths (whole-EFB white dumps).
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    nj_depth_write();
#ifndef NJ_GLES
    // Distances de decoupe : seulement pour les shaders GX (le menu, le calque tactile, la
    // presentation n'en ecrivent pas).
    nj_cap(GL_CLIP_DISTANCE0, false);
    nj_cap(GL_CLIP_DISTANCE1, false);
#endif
    if (s_framelog) {
      unsigned char px[4] = {0, 0, 0, 0};
      glReadPixels(256, 480 - 224, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
      NJ_PRINTF("[FrameLog] frame centre rgba=%02X%02X%02X%02X bp41=%06X bp42=%06X bp43=%06X\n",
             px[0], px[1], px[2], px[3], (unsigned)g_gxregs.bp[0x41],
             (unsigned)g_gxregs.bp[0x42], (unsigned)g_gxregs.bp[0x43]);
      NJ_PRINTF("[FrameLog] end: draws=%d on-screen=%d off-screen=%d unlinked=%d "
             "skipped=%d\n",
             s_framelog_draws, s_framelog_onscreen, s_framelog_offscreen,
             s_framelog_unlinked, s_framelog_skipped);
      fflush(stdout);
      s_framelog = false;
    }
    {
      // Journal continu des couleurs (recherche de la teinte jaune de r101) : toutes les 20 images de rendu,
      // 9 points de l'ecran (moyenne), couleur du brouillard (BP 0xF2), registres couleur du TEV (0xE0-0xE7),
      // ambiance et matiere des canaux 0 et 1 (XF 0x100A-0x100D). 600 lignes au plus, une ligne par mesure.
      static int s_clog_n = 0, s_clog_lines = 0;
      if (++s_clog_n % 20 == 0 && s_clog_lines < 600) {
        ++s_clog_lines;
        unsigned sum[3] = {0, 0, 0};
        for (int j = 1; j <= 3; ++j)
          for (int i = 1; i <= 3; ++i) {
            unsigned char px[4] = {0, 0, 0, 0};
            glReadPixels(i * 128, 480 - j * 120, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
            sum[0] += px[0]; sum[1] += px[1]; sum[2] += px[2];
          }
        NJ_PRINTF("[Couleurs] n=%d moy=%u,%u,%u fog=%06X tev=%06X,%06X,%06X,%06X,%06X,%06X,%06X,%06X "
                  "amb=%08X,%08X mat=%08X,%08X\n",
                  s_clog_n, sum[0] / 9, sum[1] / 9, sum[2] / 9, (unsigned)g_gxregs.bp[0xF2] & 0xFFFFFF,
                  (unsigned)g_gxregs.bp[0xE0] & 0xFFFFFF, (unsigned)g_gxregs.bp[0xE1] & 0xFFFFFF,
                  (unsigned)g_gxregs.bp[0xE2] & 0xFFFFFF, (unsigned)g_gxregs.bp[0xE3] & 0xFFFFFF,
                  (unsigned)g_gxregs.bp[0xE4] & 0xFFFFFF, (unsigned)g_gxregs.bp[0xE5] & 0xFFFFFF,
                  (unsigned)g_gxregs.bp[0xE6] & 0xFFFFFF, (unsigned)g_gxregs.bp[0xE7] & 0xFFFFFF,
                  XfBits(0x0A), XfBits(0x0B), XfBits(0x0C), XfBits(0x0D));
      }
    }
    if (s_pend_open || !resident::pend.empty()) {
      submit_pending();
      s_pend_open = false;
    }
    resident::commit();
    pull::frame_end();
    if (rprof::mode()) {
      if (rprof::mode() >= 2) {
        const uint64_t f0 = rprof::now_us();
        glFinish();
        rprof::acc[rprof::FINISH] += rprof::now_us() - f0;
      }
      static uint64_t last = rprof::now_us(), frames = 0;
      frames += s_rp_xfb;
      s_rp_xfb = 0;
      if (rprof::now_us() - last >= 2000000) {
        NJ_LOG << "[RESIDENT] lots reconnus=" << resident::hits << " sommets=" << resident::hit_verts
                  << " emis=" << resident::misses << " entrees=" << resident::map.size()
                  << " occupation=" << resident::vused << "/" << resident::kVerts << std::endl;
        resident::hits = resident::hit_verts = resident::misses = 0;
        {   // P3 : appels GL par image (compteurs du journal de perf, lus ici en mode natif)
          const double fr = frames ? (double)frames : 1.0;
          NJ_LOG << "[GLCALLS] par image : dessins GL " << g_jr_gldraw.exchange(0) / fr << ", uniformes " << g_jr_gluni.exchange(0) / fr
                 << ", changements d'etat " << g_jr_setup.exchange(0) / fr << " (" << frames << " images)" << std::endl;
          int top[4] = {-1, -1, -1, -1};
          for (int r = 0; r < 256; ++r)
            for (int k = 0; k < 4; ++k)
              if (top[k] < 0 || g_close_bpreg[r] > g_close_bpreg[top[k]]) {
                for (int m = 3; m > k; --m) top[m] = top[m - 1];
                top[k] = r;
                break;
              }
          char tb[160];
          std::snprintf(tb, sizeof tb, "BP %02X:%.0f %02X:%.0f %02X:%.0f %02X:%.0f", top[0], g_close_bpreg[top[0]] / fr, top[1],
                        g_close_bpreg[top[1]] / fr, top[2], g_close_bpreg[top[2]] / fr, top[3], g_close_bpreg[top[3]] / fr);
          NJ_LOG << "[GLCALLS]   poses legeres " << g_bind_light / fr << ", completes " << g_bind_full / fr
                 << " ; XF sans effet ignorees " << g_xf_noop / fr << ", appliquees " << g_xf_real / fr
                 << " ; BP sans effet ignorees " << g_bp_noop_skip / fr << std::endl;
          g_bind_light = g_bind_full = g_xf_noop = g_xf_real = g_bp_noop_skip = 0;
          {
            std::string o = "[GLCALLS]   ecritures autres par image : XF mat " + std::to_string((int)(g_other_xf[0] / fr)) + " nrm " +
                            std::to_string((int)(g_other_xf[1] / fr)) + " post " + std::to_string((int)(g_other_xf[2] / fr)) + " lum " +
                            std::to_string((int)(g_other_xf[3] / fr)) + " reg " + std::to_string((int)(g_other_xf[4] / fr)) + " ; BP";
            for (int pass = 0; pass < 10; ++pass) {
              int best = -1;
              for (int r = 0; r < 256; ++r)
                if (g_other_bpreg[r] && (best < 0 || g_other_bpreg[r] > g_other_bpreg[best])) best = r;
              if (best < 0) break;
              char b[24];
              std::snprintf(b, sizeof b, " %02X:%.0f", best, g_other_bpreg[best] / fr);
              o += b;
              g_other_bpreg[best] = 0;
            }
            NJ_LOG << o << std::endl;
            std::memset(g_other_bpreg, 0, sizeof g_other_bpreg);
            std::memset(g_other_xf, 0, sizeof g_other_xf);
          }
          NJ_LOG << "[GLCALLS]   etat repose : lot ferme par une commande " << g_why_closed / fr << ", matrice seule " << g_why_mtx / fr
                 << ", autre " << g_why_other / fr << " ; fermetures BP " << g_close_bp / fr << " CP " << g_close_cp / fr << " XF "
                 << g_close_xf / fr << " ; " << tb << std::endl;
          g_why_closed = g_why_mtx = g_why_other = g_close_bp = g_close_cp = g_close_xf = 0;
          std::memset(g_close_bpreg, 0, sizeof g_close_bpreg);
        }
        if (pull::on()) {
          NJ_LOG << "[PULL] dessins tires=" << pull::st_draws << " lots=" << pull::st_flush << " octets="
                 << pull::st_bytes << " lots perdus=" << pull::st_lost << " (fenetre de 2 s, " << frames << " images)" << std::endl;
          pull::st_draws = pull::st_flush = pull::st_bytes = pull::st_lost = 0;
        }
        if (NJ_ENV("NJ_RPROF")) rprof::report(frames);   // sinon le journal lit et remet a zero les chronos (rprof_journal_fields)
        frames = 0;
        last = rprof::now_us();
      }
    }
  }



};

std::unique_ptr<FrameRenderer> make_gl_renderer() {
  return std::make_unique<GlRenderer>();
}

} // namespace nj::runtime::gx

uint64_t nj_stat_hash0() { return nj::runtime::gx::g_st_hash0; }
uint64_t nj_stat_shaders() { return nj::runtime::gx::g_st_shaders; }


