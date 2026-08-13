#include <chrono>
// -*- coding: utf-8 -*-
// Rendu natif, cote moteur (phase N3 du plan natif, nj-work/docs/PLAN_NATIF_NIGHTJAR_2026-11-11.md).
//
// Compile avec les options et les en-tetes de Nightjar (C++20), lie avec libnjruntime.a : le lecteur de flux
// et le rendu de Nightjar tournent DANS le coeur, sur la memoire du jeu (arene a 0x80000000 : une adresse
// GameCube y est un pointeur). Le portage (port/gx_sdk.cpp) lui passe le flux du SDK ; l'image finie part
// a l'appli par njlink (images partagees).
#include "njgfx_bridge.h"
#include "../njlink.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include <unistd.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>

#include "common/platform.h"
#include "runtime/config.h"
#include "runtime/cpu_context.h"
#include "runtime/gfx_settings.h"
#include "runtime/gx/fifo_parser.h"
#include "runtime/gx/fifo_record.h"
#include "runtime/gx/frame_renderer.h"
#include "runtime/hw/hw.h"

extern std::atomic<uint64_t> g_fl_xfb_copies;   // fifo_parser.cpp (gx.cpp) : copies EFB -> XFB analysees
#include "glbench.cpp"   // banc des couts GL (NJ_GLBENCH)
namespace nj::runtime::gx {
extern std::atomic<uint64_t> g_prof_xf_bank, g_prof_xf_single, g_prof_xf_none;   // fifo_parser.cpp : chemins de transformation
extern uint64_t g_prof_dl_calls, g_prof_dl_hits;                                  // listes d'affichage developpees / en cache
}
namespace nj::runtime {
uint32_t g_debug_pc = 0;
MMU *g_guest_mem = nullptr;
GuestCpu *g_guest = nullptr;
}  // namespace nj::runtime

extern "C" void njlink_gl_ready(int w, int h);
extern "C" void njlink_frame_fbo(unsigned fbo, int x, int y, int w, int h);
// meme chose, image placee sur les lignes [dy, dy + dh) des 480 de l'image partagee (le reste en noir)
extern "C" void njlink_frame_fbo2(unsigned fbo, int x, int y, int w, int h, int dy, int dh);
extern "C" void port_vi_mode(unsigned out[6]);   // port/vi.cpp
extern "C" void njlink_set_xfb(unsigned w, unsigned h);   // port/njlink.cpp : zone de l'EFB copiee dans l'XFB

extern std::atomic<bool> g_gx_framelog_request;   // renderer_gl.cpp (F12 dans l'appli)
extern "C" int port_host_scope;
extern "C" void njstate_fill(NjState *out);
extern "C" void njstate_debug_ids(void);   // port/arm_hooks.cpp : allocations C++ hors du tas du jeu

namespace {
struct HostScope {   // tout ce que fait le rendu alloue avec malloc, pas dans le tas du jeu
  HostScope() { ++port_host_scope; }
  ~HostScope() { --port_host_scope; }
};
// Les fils du jeu sont de vrais fils (passes un a un par un jeton) : njgfx_sync vient de
// OSDisableInterrupts, appele depuis n'importe lequel. Le contexte GL n'est courant que sur un fil :
// sans lui, chaque appel GL ne fait rien (nuanceur du titre « rate sans journal », copies noires).
// Diagnostic : on releve les fils qui entrent dans le rendu et l'etat du contexte.
void note_thread(const char *who);

}  // namespace

namespace {

namespace rt = nj::runtime;
namespace gx = nj::runtime::gx;
namespace gfx = nj::runtime::gfx;

bool g_ready, g_failed;
EGLDisplay g_dpy;
EGLContext g_ctx;
EGLSurface g_surf;
GLuint g_efb_fbo, g_efb_color, g_efb_depth;
std::unique_ptr<gx::FrameRenderer> g_renderer;
std::vector<uint8_t> g_stream;   // octets pas encore lus (commande coupee en fin de tampon)
std::vector<gx::GxOp> g_ops;     // commandes de l'image en cours, dessins deja decodes
size_t g_decoded;                // g_ops[0..g_decoded) decodes
unsigned g_frames;
unsigned g_skipped;   // images sans dessin, non envoyees

bool init() {
  if (g_ready || g_failed) return g_ready;
  g_failed = true;
  g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (g_dpy == EGL_NO_DISPLAY || !eglInitialize(g_dpy, nullptr, nullptr)) return false;
  const EGLint ca[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR, EGL_RED_SIZE, 8,
                       EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE};
  EGLConfig cfg;
  EGLint n = 0;
  if (!eglChooseConfig(g_dpy, ca, &cfg, 1, &n) || n == 0) return false;
  const EGLint sa[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
  g_surf = eglCreatePbufferSurface(g_dpy, cfg, sa);
  const EGLint xa[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, xa);
  if (!eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) return false;
  std::printf("[NJGFX] GL : %s\n", (const char *)glGetString(GL_RENDERER));

  // la memoire du jeu, telle quelle : MMU sans allocation (cpu_context.h)
  rt::MMU::external_mem1 = reinterpret_cast<uint8_t *>(0x80000000u);
  static rt::GuestCpu *ctx = new rt::GuestCpu;
  rt::g_guest = ctx;
  rt::g_guest_mem = &ctx->mmu;
  gx::g_native_arrays = true;
  if (const char *sd = std::getenv("NJ_STORAGE_DIR")) nj::set_writable_dir(sd);   // appli : son dossier
  rt::Config::get().load("config.toml");
  rt::hw::attach_hardware(rt::IoBus::get());

  const int w = gfx::efb_w(), h = gfx::efb_h();
  glGenTextures(1, &g_efb_color);
  glBindTexture(GL_TEXTURE_2D, g_efb_color);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glGenRenderbuffers(1, &g_efb_depth);
  glBindRenderbuffer(GL_RENDERBUFFER, g_efb_depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
  glGenFramebuffers(1, &g_efb_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, g_efb_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_efb_color, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_efb_depth);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::printf("[NJGFX] EFB incomplet\n");
    return false;
  }
  g_renderer = gx::make_gl_renderer();
  g_renderer->attach(nullptr);
  // ce que l'appli presente : 640 x 480 a l'echelle interne (comme presented_size)
  njlink_gl_ready(640 * gfx::g_scale, 480 * gfx::g_scale);
  std::printf("[NJGFX] rendu Nightjar pret : EFB %dx%d, echelle %d\n", w, h, gfx::g_scale);
  g_failed = false;
  g_ready = true;
  return true;
}

void note_thread(const char *who) {
  static int seen[8], nseen;
  const int tid = (int)gettid();
  for (int i = 0; i < nseen; ++i)
    if (seen[i] == tid) {
      if (g_ready && eglGetCurrentContext() != g_ctx) {
        static int bad;
        if (bad++ < 10) std::printf("[NJGFX] %s : fil %d sans le contexte GL\n", who, tid);
      }
      return;
    }
  if (nseen < 8) seen[nseen++] = tid;
  std::printf("[NJGFX] %s : nouveau fil %d (contexte %s)\n", who, tid,
              !g_ready ? "pas encore cree" : eglGetCurrentContext() == g_ctx ? "courant" : "ABSENT");
}

// Contexte GL pris par le fil qui entre dans le rendu, rendu a la sortie : le prochain appel peut venir
// d'un autre fil du jeu (nj30 : contexte cree sur le fil 12114, images envoyees par le 12116).
double g_gl_take_ms = 0, g_gl_rel_ms = 0, g_ph_take = 0;
bool g_ph_draw_seen = false;
struct PhaseAcc { double take = 0, draw = 0, post = 0, all = 0; unsigned n = 0, empty = 0; } g_ph;   // mesure (enquete Render_swap, 13/11)
struct GlScope {
  bool took = false;
  GlScope() {
    const auto t0 = std::chrono::steady_clock::now();
    if (g_ready && eglGetCurrentContext() != g_ctx) took = eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx) == EGL_TRUE;
    g_gl_take_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  }
  ~GlScope() {
    const auto t0 = std::chrono::steady_clock::now();
    if (g_ready) eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    g_gl_rel_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  }
};

// Lit les commandes completes et decode tout de suite leurs sommets (le jeu peut reecrire sa memoire
// des le retour).
// Mesure (enquete lenteur r100, 13/11) : temps de lecture du flux et de decodage par image, appels et octets.
struct TakeStat { double parse_ms = 0, dec_ms = 0; unsigned calls = 0, draws = 0; size_t bytes = 0, verts = 0, stream_max = 0, ops_max = 0; };
TakeStat g_ts;
void take(const unsigned char *bytes, unsigned size) {
  if (!init()) return;
  using clk = std::chrono::steady_clock;
  const auto t0 = clk::now();
  g_stream.insert(g_stream.end(), bytes, bytes + size);
  if (g_stream.size() > g_ts.stream_max) g_ts.stream_max = g_stream.size();
  gx::ParseFifo(g_stream, g_ops);
  const auto t1 = clk::now();
  for (; g_decoded < g_ops.size(); ++g_decoded)
    if (g_ops[g_decoded].type == gx::GxOpKind::DrawPrimitive) {
      gx::DecodeDraw(g_ops[g_decoded]);
      ++g_ts.draws;
      g_ts.verts += g_ops[g_decoded].shared_vertices ? g_ops[g_decoded].shared_vertices->size() : g_ops[g_decoded].vertices.size();
    }
  const auto t2 = clk::now();
  if (g_ops.size() > g_ts.ops_max) g_ts.ops_max = g_ops.size();
  g_ts.parse_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
  g_ts.dec_ms += std::chrono::duration<double, std::milli>(t2 - t1).count();
  ++g_ts.calls;
  g_ts.bytes += size;
}
// NJ_PERFCNT=1 (14/11) : mesure independante de la frequence. Compteurs materiels du fil du jeu (cycles,
// instructions) ouverts a la premiere image (security.perf_harden doit valoir 0 a cet instant ; le banc le remet a 1
// ensuite, les compteurs ouverts restent valides), et temps sur le processeur / attente de son tour
// (/proc/self/task/<tid>/schedstat). Ligne [PERFCNT] toutes les 120 images rendues : par image, exact.
// Pourquoi : la frequence du S5 varie du simple au double d'un essai a l'autre (chauffe) ; les i/s ne comparent rien.
static void perfcnt_frame() {
  static const bool on = std::getenv("NJ_PERFCNT") != nullptr;
  if (!on) return;
  static int fd_cyc = -2, fd_ins = -1;
  static unsigned n = 0;
  static uint64_t c0 = 0, i0 = 0, e0 = 0, w0 = 0, sw0 = 0;
  static char sched_path[64];
  auto rd = [](int fd) -> uint64_t { uint64_t v = 0; return fd >= 0 && read(fd, &v, sizeof v) == (ssize_t)sizeof v ? v : 0; };
  auto sched = [&](uint64_t &e, uint64_t &w, uint64_t &s) {
    e = w = s = 0;
    if (FILE *f = std::fopen(sched_path, "r")) {
      unsigned long long a = 0, b = 0, c = 0;
      if (std::fscanf(f, "%llu %llu %llu", &a, &b, &c) == 3) { e = a; w = b; s = c; }
      std::fclose(f);
    }
  };
  if (fd_cyc == -2) {
    // fil principal du processus (profil du 13/11 : 97 % du coeur y tourne, jeu et rendu) ; la premiere image
    // peut venir d'un autre fil, d'ou le choix explicite
    const int tid = (int)getpid();
    std::snprintf(sched_path, sizeof sched_path, "/proc/self/task/%d/schedstat", tid);
    auto open_hw = [tid](uint64_t cfg) -> int {
      struct perf_event_attr a;
      std::memset(&a, 0, sizeof a);
      a.type = PERF_TYPE_HARDWARE;
      a.size = sizeof a;
      a.config = cfg;
      a.exclude_hv = 1;
      return (int)syscall(__NR_perf_event_open, &a, tid, -1, -1, 0);
    };
    fd_cyc = open_hw(PERF_COUNT_HW_CPU_CYCLES);
    fd_ins = open_hw(PERF_COUNT_HW_INSTRUCTIONS);
    std::printf("[PERFCNT] compteurs : cycles %s, instructions %s (fil %d, image vue du fil %d)\n",
                fd_cyc >= 0 ? "ouverts" : "refuses (perf_harden ?)", fd_ins >= 0 ? "ouverts" : "refuses", tid, (int)gettid());
    c0 = rd(fd_cyc); i0 = rd(fd_ins); sched(e0, w0, sw0);
    return;
  }
  if (++n < 120) return;
  const uint64_t c1 = rd(fd_cyc), i1 = rd(fd_ins);
  uint64_t e1, w1, sw1;
  sched(e1, w1, sw1);
  std::printf("[PERFCNT] par image (%u images) : %.1f Mcycles, %.1f Minstructions, processeur %.1f ms, attente de son tour %.1f ms,"
              " %.0f tranches ; %.2f GHz\n", n, (c1 - c0) / 1e6 / n, (i1 - i0) / 1e6 / n, (e1 - e0) / 1e6 / n, (w1 - w0) / 1e6 / n,
              (double)(sw1 - sw0) / n, e1 > e0 ? (double)(c1 - c0) / (double)(e1 - e0) : 0.0);
  c0 = c1; i0 = i1; e0 = e1; w0 = w1; sw0 = sw1;
  n = 0;
}

void take_stat_frame(unsigned frame) {
  perfcnt_frame();
  static unsigned n = 0;
  static TakeStat acc;
  acc.parse_ms += g_ts.parse_ms; acc.dec_ms += g_ts.dec_ms; acc.calls += g_ts.calls; acc.draws += g_ts.draws;
  acc.bytes += g_ts.bytes; acc.verts += g_ts.verts;
  if (g_ts.stream_max > acc.stream_max) acc.stream_max = g_ts.stream_max;
  if (g_ts.ops_max > acc.ops_max) acc.ops_max = g_ts.ops_max;
  g_ts = TakeStat();
  if (++n < 30) return;
  std::printf("[TAKE] image %u, par image : lecture %.1f ms, decodage %.1f ms, %u appels, %u dessins, %zu sommets, %zu octets ;"
              " flux max %zu, commandes max %zu\n", frame, acc.parse_ms / n, acc.dec_ms / n, acc.calls / n, acc.draws / n,
              acc.verts / n, acc.bytes / n, acc.stream_max, acc.ops_max);
  {
    namespace g = nj::runtime::gx;
    const uint64_t b = g::g_prof_xf_bank.exchange(0), s1 = g::g_prof_xf_single.exchange(0), z = g::g_prof_xf_none.exchange(0);
    std::printf("[TAKE]   sommets par image : banque de matrices (CPU) %llu, matrice unique (GPU) %llu, sans matrice %llu ;"
                " listes %llu dont %llu en cache\n", (unsigned long long) (b / n), (unsigned long long) (s1 / n),
                (unsigned long long) (z / n), (unsigned long long) (g::g_prof_dl_calls / n), (unsigned long long) (g::g_prof_dl_hits / n));
    g::g_prof_dl_calls = 0;
    g::g_prof_dl_hits = 0;
  }
  acc = TakeStat();
  n = 0;
}

}  // namespace

extern "C" void njgfx_sync(const unsigned char *bytes, unsigned size) {
  HostScope hs;
  if (!g_ready) {   // premiere fois : init() cree le contexte GL
    GlScope gs;
    take(bytes, size);
    return;
  }
  take(bytes, size);   // lecture du flux et decodage des sommets : aucun appel GL
}

extern "C" void njgfx_frame(const unsigned char *bytes, unsigned size) {
  using clk = std::chrono::steady_clock;
  struct FtEnd {   // mesure : njgfx_frame entier (liberation du contexte comprise), moyenne sur 30 images
    clk::time_point f0 = clk::now();
    ~FtEnd() {
      static double tot = 0;
      static unsigned n = 0;
      const double all = std::chrono::duration<double, std::milli>(clk::now() - f0).count();
      tot += all;
      if (g_ph_draw_seen) {
        g_ph.take += g_ph_take;
        g_ph.all += all;
        if (++g_ph.n == 30) {
          std::printf("[TAKE] appels avec dessins : %.1f ms (reste du flux %.1f, draw_frame %.1f, apres %.1f) ; appels vides %u\n",
                      g_ph.all / 30, g_ph.take / 30, g_ph.draw / 30, (g_ph.all - g_ph.take - g_ph.draw) / 30, g_ph.empty);
          g_ph = PhaseAcc();
        }
      } else {
        ++g_ph.empty;
      }
      g_ph_draw_seen = false;
      if (++n == 30) {
        std::printf("[TAKE] njgfx_frame %.1f ms par image ; contexte GL : prise %.2f, liberation %.2f ms par image\n",
                    tot / n, g_gl_take_ms / n, g_gl_rel_ms / n);
        tot = 0;
        n = 0;
        g_gl_take_ms = g_gl_rel_ms = 0;
      }
    }
  } ft_end;
  HostScope hs;
  note_thread("image");
  GlScope gs;
  {   // NJ_GLBENCH=1 : banc des couts GL du pilote, une fois (glbench.cpp, question 4 de la recherche du 13/11)
    static const bool bench = std::getenv("NJ_GLBENCH") != nullptr;
    static bool done = false;
    if (bench && !done && g_ready && g_frames >= 120) {
      done = true;
      njgfx_glbench();
      glViewport(0, 0, gfx::efb_w(), gfx::efb_h());
    }
  }
  const auto tk0 = clk::now();
  take(bytes, size);
  g_ph_take = std::chrono::duration<double, std::milli>(clk::now() - tk0).count();
  if (!g_ready) return;
  {   // image sans dessin (le jeu calcule a 30 i/s, VIFlush tombe a 60 Hz) : rien a montrer. Le rendu aurait
      // fait l'effacement differe de l'EFB et l'appli aurait recu une image noire une fois sur deux (nj23) ;
      // le VI du GameCube reaffiche le dernier XFB. Les commandes d'etat restent pour l'image suivante.
    bool any_draw = false;
    for (const auto &c : g_ops)
      if (c.type == gx::GxOpKind::DrawPrimitive) { any_draw = true; break; }
    if (!any_draw) {
      ++g_skipped;
      return;
    }
    // Pas de copie vers l'image affichee (EFB -> XFB, BP 0x52 bit 0x4000) depuis le dernier rendu : ce VIFlush ne
    // termine pas une image du jeu (ScreenReSize en milieu d'image pendant les cinematiques). Rendre ici envoyait une
    // demi-image (souvent presque noire) puis le reste : deux rendus par image et le clignotement noir (13/11).
    // Les commandes restent pour le VIFlush qui suit la copie.
    static uint64_t last_copies = 0;
    const uint64_t copies = g_fl_xfb_copies.load(std::memory_order_relaxed);
    static const bool always = std::getenv("NJ_RENDER_EVERY_FLUSH") != nullptr;
    if (copies == last_copies && !always) {
      ++g_skipped;
      return;
    }
    last_copies = copies;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, g_efb_fbo);
  glViewport(0, 0, gfx::efb_w(), gfx::efb_h());
  const size_t nops = g_ops.size();
  size_t ndraws = 0, nbp = 0, nxf = 0;
  for (const auto &c : g_ops) {
    ndraws += c.type == gx::GxOpKind::DrawPrimitive;
    nbp += c.type == gx::GxOpKind::BPRegister;
    nxf += c.type == gx::GxOpKind::XFRegister;
  }
  {   // diagnostic : NJ_FRAMELOG_AT=<image> journal complet d'une image (dev_env.txt)
    static const unsigned at = std::getenv("NJ_FRAMELOG_AT") ? (unsigned)std::atoi(std::getenv("NJ_FRAMELOG_AT")) : 0;
    if (at && g_frames + 1 == at) g_gx_framelog_request = true;
  }
  {   // NJ_DFF_AT=<image> : enregistrement .dff de 6 images (fifo_record.cpp), a comparer aux enregistrements Dolphin
    static const unsigned dff = std::getenv("NJ_DFF_AT") ? (unsigned)std::atoi(std::getenv("NJ_DFF_AT")) : 0;
    if (dff && g_frames + 1 == dff) nj::runtime::gx::rec::request(6);
  }
  {
    using clk = std::chrono::steady_clock;
    const auto t0 = clk::now();
    g_ph_draw_seen = true;
    g_renderer->draw_frame(g_ops);
    g_ph.draw += std::chrono::duration<double, std::milli>(clk::now() - t0).count();
    static double acc = 0;
    static unsigned n = 0;
    acc += std::chrono::duration<double, std::milli>(clk::now() - t0).count();
    if (++n == 30) { std::printf("[TAKE] rendu GL (draw_frame) %.1f ms par image\n", acc / n); acc = 0; n = 0; }
  }
  take_stat_frame(g_frames);
  g_ops.clear();
  g_decoded = 0;
  gx::DecodeEpochAdvance();
  const int s = gfx::g_scale;
  ++g_frames;
  if (g_frames % 300 == 0) {   // diagnostic N3 : 64 points de l'EFB (moyenne RGB, points non noirs) et mode du jeu
    unsigned char px[4];
    unsigned sum[3] = {0, 0, 0}, lit = 0;
    GLint cur = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &cur);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_efb_fbo);
    for (int j = 0; j < 8; ++j)
      for (int i = 0; i < 8; ++i) {
        glReadPixels((40 + 80 * i) * s, gfx::efb_h() - (30 + 60 * j) * s, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        for (int k = 0; k < 3; ++k) sum[k] += px[k];
        lit += (px[0] | px[1] | px[2]) > 8;
      }
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)cur);
    NjState st;
    njstate_fill(&st);
    std::printf("[NJGFX] mesure %u : moyenne %u,%u,%u, %u/64 points allumes, %zu dessins, mode %u salle %03x bits %05x qte %u\n", g_frames,
                sum[0] / 64, sum[1] / 64, sum[2] / 64, lit, ndraws, (unsigned)st.mode, (unsigned)st.room, (unsigned)st.bits, (unsigned)st.qte);
    if (st.mode == 6 && (g_frames == 600 || g_frames == 900)) njstate_debug_ids();   // diagnostic : titre
    // diagnostic : NJ_TEXDUMP="adresse:taille,..." (hexadecimal) : octets de textures tels qu'en memoire, a l'image 2400
    static std::vector<std::pair<unsigned, unsigned>> tx = [] {
      std::vector<std::pair<unsigned, unsigned>> v;
      if (const char *e = std::getenv("NJ_TEXDUMP"))
        for (const char *q = e; *q;) {
          unsigned a = 0, n = 0;
          if (std::sscanf(q, "%x:%x", &a, &n) == 2) v.push_back({a, n});
          const char *c = std::strchr(q, ',');
          if (!c) break;
          q = c + 1;
        }
      return v;
    }();
    if (g_frames == 2400 && !tx.empty()) {
      const char *d = std::getenv("NJ_STORAGE_DIR");
      for (const auto &t0 : tx) {
        const struct { unsigned addr, size; } t = {t0.first, t0.second};
        char path[300];
        std::snprintf(path, sizeof path, "%s/tex_%08x.bin", d ? d : ".", t.addr);
        if (FILE *f = std::fopen(path, "wb")) {
          std::fwrite(reinterpret_cast<const void *>(t.addr), 1, t.size, f);
          std::fclose(f);
        }
      }
    }
  }
  if (g_frames <= 3 || g_frames % 600 == 0) {   // diagnostic N3 : ce que le rendu a recu et ce qu'il a dessine
    unsigned char px[4] = {0, 0, 0, 0};
    GLint cur = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &cur);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_efb_fbo);
    glReadPixels(320 * s, gfx::efb_h() - 240 * s, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)cur);
    std::printf("[NJGFX] image %u : %zu commandes (%zu dessins, %zu BP, %zu XF), %u octets lus, pixel central %02x%02x%02x%02x\n",
                g_frames, nops, ndraws, nbp, nxf, size, px[0], px[1], px[2], px[3]);
    {   // diagnostic N3 : debut de la texture du titre (CMPR 640x360) telle qu'elle est en memoire
      const uint8_t *t = reinterpret_cast<const uint8_t *>(0x811E3100u);
      std::printf("[NJGFX] texture 011E3100 :");
      for (int i = 0; i < 32; ++i) std::printf(" %02x", t[i]);
      std::printf("\n");
    }
  }
  {   // ce que montre le VI : la zone rendue (fbWidth x efbHeight, en haut de l'EFB) etiree sur toute la largeur,
      // ses lignes centrees dans les 480 de l'ecran (viYOrigin)
    unsigned vm[6];
    port_vi_mode(vm);
    // 12/11 : l'appli presente elle-meme, comme en 0.1.9 (present_image : zone copiee dans l'XFB, 16:9,
    // place des commandes) ; on lui envoie l'EFB tel quel et la taille de cette zone.
    const unsigned fw = vm[0] >= 16 && vm[0] <= 640 ? vm[0] : 640;
    const unsigned eh = vm[1] >= 16 && vm[1] <= 480 ? vm[1] : 480;
    njlink_set_xfb(fw, eh);
    njlink_frame_fbo(g_efb_fbo, 0, gfx::efb_h() - 480 * s, 640 * s, 480 * s);
  }
}
