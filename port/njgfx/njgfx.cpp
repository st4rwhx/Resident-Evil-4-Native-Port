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
#include <condition_variable>
#include <mutex>
#include <pthread.h>
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
extern "C" void njlink_src_size(int w, int h);
extern "C" void njlink_capture_state(void);   // port/njlink.cpp
extern "C" void port_studio_event(const char *fmt, ...);   // port/studio.cpp
extern "C" int port_studio_capture(void);   // port/studio.cpp   // port/njlink.cpp : taille de l EFB (copie par dessin)
extern "C" void njlink_frame_fbo(unsigned fbo, int x, int y, int w, int h);
// meme chose, image placee sur les lignes [dy, dy + dh) des 480 de l'image partagee (le reste en noir)
extern "C" void njlink_frame_fbo2(unsigned fbo, int x, int y, int w, int h, int dy, int dh);
extern "C" void port_vi_mode(unsigned out[6]);   // port/vi.cpp
extern "C" void njlink_set_xfb(unsigned w, unsigned h);   // port/njlink.cpp : zone de l'EFB copiee dans l'XFB

extern std::atomic<bool> g_gx_framelog_request;   // renderer_gl.cpp (F12 dans l'appli)
// Studio (port/studio.cpp, commande « fifo [n] ») : .dff de n images + journal [FL] de l'image en cours.
extern "C" void njgfx_studio_fifo(int frames) {
  nj::runtime::gx::rec::request(frames);
  g_gx_framelog_request = true;
}
extern "C" int port_host_scope;
extern "C" unsigned long port_render_thread;   // port/arm_hooks.cpp : fil de rendu (NJ_RTHREAD=1)
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

// N7 (18/11) : en 64 bits la memoire du jeu est dans une fenetre de 4 Go alignee (port/os.cpp) : base | adresse.
#if defined(__LP64__)
extern "C" uintptr_t g_port_base;
static inline uintptr_t njg_base() { return g_port_base; }
#else
static inline uintptr_t njg_base() { return 0; }
#endif
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
  rt::MMU::external_mem1 = reinterpret_cast<uint8_t *>(njg_base() | 0x80000000u);
  static rt::GuestCpu *ctx = new rt::GuestCpu;
  rt::g_guest = ctx;
  rt::g_guest_mem = &ctx->mmu;
  gx::g_native_arrays = true;
  if (const char *sd = std::getenv("NJ_STORAGE_DIR")) nj::set_writable_dir(sd);   // appli : son dossier
  rt::Config::get().load("config.toml");
  // Resolution interne (15/11) : le reglage « Internal resolution » de Nightjar n'arrivait pas ici (echelle 1 fixe :
  // seule la boucle de l'appli appliquait wanted_scale, et le rendu natif n'y passe pas). NJ_SCALE : echelle calculee
  // par l'appli au lancement (elle connait la taille de l'ecran, utile pour « native ») ; sinon le reglage lu ci-dessus.
  {
    int s = gfx::g_scale_setting;
    if (const char *e = std::getenv("NJ_SCALE")) s = std::atoi(e);
    if (s < 1) s = 1;
    if (s > gfx::kMaxScale) s = gfx::kMaxScale;
    gfx::g_scale = s;
  }
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
  njlink_src_size(w, h);
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
// Fil de rendu (14/11, NJ_RTHREAD=0 pour le couper) : le contexte appartient au fil de rendu ; le fil du jeu ne le tient que pendant init().
bool rthread_on() {
  static const bool v = [] { const char *e = std::getenv("NJ_RTHREAD"); return !(e && e[0] == '0'); }();   // actif par defaut depuis le 14/11 (r100, village, 10 min)
  return v;
}
struct GlScope {
  bool took = false;
  GlScope() {
    const auto t0 = std::chrono::steady_clock::now();
    if (g_ready && !rthread_on() && eglGetCurrentContext() != g_ctx) took = eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx) == EGL_TRUE;
    g_gl_take_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  }
  ~GlScope() {
    const auto t0 = std::chrono::steady_clock::now();
    if (g_ready && (!rthread_on() || eglGetCurrentContext() == g_ctx)) eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
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

// GX natif (port/gxn.cpp, NJ_GXN=1) : l'etat decode du flux, oracle de l'etat type.
extern "C" int njgfx_oracle(const unsigned **cp, const unsigned **bp, const unsigned **xf, const unsigned char **xfok) {
  const uint32_t *c, *b, *x;
  const uint8_t *k;
  gx::OracleRegs(&c, &b, &x, &k);
  *cp = c;
  *bp = b;
  *xf = x;
  *xfok = k;
  return 1;
}

static void render_frame(std::vector<gx::GxOp> &ops, unsigned size, unsigned frame, const unsigned vm_in[6]);

// Fil de rendu (NJ_RTHREAD=1, 14/11 ; modele OpenGOAL : le fil du jeu passe l'image finie au fil graphique).
// Mesure du 14/11 (r100, S5) : jeu, plomberie GX et pilote GL tournaient en serie sur un seul coeur (~0,85).
// Le fil du jeu lit et decode l'image N (etat de lecture g_pstate, distinct de celui du rendu), puis la passe ici ;
// le contexte GL reste a ce fil. Profondeur 1 : le jeu n'attend que si l'image N-1 n'est pas finie a la passation.
// Le cache de decodage avance d'une generation a la passation, quand plus personne ne lit l'image N-1.
// Fil cree par pthread_create (pas std::thread) : aucune allocation avant que port_render_thread soit pose.
namespace rth {
std::mutex mu;
std::condition_variable cv;
bool busy = false, started = false;
std::vector<gx::GxOp> job;
unsigned job_size = 0, job_frame = 0, job_vm[6];
double wait_ms = 0, render_ms = 0;
unsigned n = 0;

// NJ_PERFCNT=1 : compteurs materiels du fil de rendu lui-meme, exacts par image rendue (comme perfcnt_frame pour le fil
// du jeu). Le banc qui divisait des cycles comptes sur 15 s par des images estimees dans le journal variait de +-15 %
// (village, 14/11) : il ne pouvait pas trancher un gain de moins de 10 %. perf_harden doit valoir 0 a l'ouverture.
struct RenderCnt {
  int fd = -2;
  uint64_t c0 = 0, e0 = 0, w0 = 0;
  unsigned n = 0;
  char path[64];
  static uint64_t rd(int f) { uint64_t v = 0; return f >= 0 && read(f, &v, sizeof v) == (ssize_t) sizeof v ? v : 0; }
  void sched(uint64_t &e, uint64_t &w) {
    e = w = 0;
    if (FILE *f = std::fopen(path, "r")) {
      unsigned long long a = 0, b = 0;
      if (std::fscanf(f, "%llu %llu", &a, &b) == 2) { e = a; w = b; }
      std::fclose(f);
    }
  }
  void frame() {
    static const bool on = std::getenv("NJ_PERFCNT") != nullptr;
    if (!on) return;
    if (fd == -2) {
      const int tid = (int) gettid();
      std::snprintf(path, sizeof path, "/proc/self/task/%d/schedstat", tid);
      struct perf_event_attr a;
      std::memset(&a, 0, sizeof a);
      a.type = PERF_TYPE_HARDWARE;
      a.size = sizeof a;
      a.config = PERF_COUNT_HW_CPU_CYCLES;
      a.exclude_hv = 1;
      fd = (int) syscall(__NR_perf_event_open, &a, tid, -1, -1, 0);
      std::printf("[PERFCNT-R] compteur du fil de rendu %d : %s\n", tid, fd >= 0 ? "ouvert" : "refuse (perf_harden ?)");
      c0 = rd(fd);
      sched(e0, w0);
      return;
    }
    if (++n < 120) return;
    const uint64_t c1 = rd(fd);
    uint64_t e1, w1;
    sched(e1, w1);
    std::printf("[PERFCNT-R] par image (%u images) : %.1f Mcycles, processeur %.1f ms, attente de son tour %.1f ms ; %.2f GHz\n", n,
                (c1 - c0) / 1e6 / n, (e1 - e0) / 1e6 / n, (w1 - w0) / 1e6 / n, e1 > e0 ? (double) (c1 - c0) / (double) (e1 - e0) : 0.0);
    c0 = c1; e0 = e1; w0 = w1;
    n = 0;
  }
};

void *loop(void *) {
  port_render_thread = (unsigned long) pthread_self();
  eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx);
  std::printf("[RTHREAD] fil de rendu %d, contexte %s\n", (int) gettid(), eglGetCurrentContext() == g_ctx ? "courant" : "ABSENT");
  static RenderCnt cnt;
  for (;;) {
    {
      std::unique_lock<std::mutex> lk(mu);
      cv.wait(lk, [] { return busy; });
    }
    const auto t0 = std::chrono::steady_clock::now();
    render_frame(job, job_size, job_frame, job_vm);
    cnt.frame();
    job.clear();
    render_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    {
      std::lock_guard<std::mutex> lk(mu);
      busy = false;
    }
    cv.notify_all();
  }
  return nullptr;
}
}  // namespace rth

static void rth_submit(unsigned size, unsigned frame, const unsigned vm[6]) {
  const auto t0 = std::chrono::steady_clock::now();
  {
    std::unique_lock<std::mutex> lk(rth::mu);
    rth::cv.wait(lk, [] { return !rth::busy; });
  }
  rth::wait_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  njlink_capture_state();     // image N-1 finie : etat du jeu copie sur le fil du jeu, le fil de rendu l'enverra
  gx::DecodeEpochAdvance();   // image N-1 finie : ce qu'elle retenait peut partir
  {
    std::lock_guard<std::mutex> lk(rth::mu);
    rth::job.swap(g_ops);       // g_ops recoit le vecteur vide (et sa capacite) de l'image rendue
    g_ops.clear();
    rth::job_size = size;
    rth::job_frame = frame;
    std::memcpy(rth::job_vm, vm, sizeof rth::job_vm);
    rth::busy = true;
  }
  g_decoded = 0;
  if (!rth::started) {
    rth::started = true;
    pthread_t t;
    pthread_create(&t, nullptr, rth::loop, nullptr);
    pthread_detach(t);
  }
  rth::cv.notify_all();
  if (++rth::n == 120) {
    std::printf("[RTHREAD] par image : attente du jeu %.1f ms, rendu %.1f ms (120 images)\n", rth::wait_ms / 120, rth::render_ms / 120);
    rth::wait_ms = rth::render_ms = 0;
    rth::n = 0;
  }
}

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
    if (bench && !done && g_ready && g_frames >= 120 && !rthread_on()) {
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
  {   // diagnostic : NJ_FRAMELOG_AT=<image> journal complet d'une image (dev_env.txt)
    static const unsigned at = std::getenv("NJ_FRAMELOG_AT") ? (unsigned)std::atoi(std::getenv("NJ_FRAMELOG_AT")) : 0;
    if (at && g_frames + 1 == at) g_gx_framelog_request = true;
  }
  {   // NJ_DFF_AT=<image> : enregistrement .dff de 6 images (fifo_record.cpp), a comparer aux enregistrements Dolphin
    static const unsigned dff = std::getenv("NJ_DFF_AT") ? (unsigned)std::atoi(std::getenv("NJ_DFF_AT")) : 0;
    if (dff && g_frames + 1 == dff) nj::runtime::gx::rec::request(6);
  }
  unsigned vm[6];
  port_vi_mode(vm);
  take_stat_frame(g_frames);
  const unsigned frame = ++g_frames;
  if (rthread_on()) {   // fil de rendu : il dessine cette image pendant que le jeu prepare la suivante
    rth_submit(size, frame, vm);
    return;
  }
  render_frame(g_ops, size, frame, vm);
  g_ops.clear();
  g_decoded = 0;
  gx::DecodeEpochAdvance();
}

// Dessin d'une image lue et decodee (fil du jeu, ou fil de rendu avec NJ_RTHREAD=1) et envoi a l'appli.
static void render_frame(std::vector<gx::GxOp> &ops, unsigned size, unsigned frame, const unsigned vm_in[6]) {
  glBindFramebuffer(GL_FRAMEBUFFER, g_efb_fbo);
  glViewport(0, 0, gfx::efb_w(), gfx::efb_h());
  const size_t nops = ops.size();
  size_t ndraws = 0, nbp = 0, nxf = 0;
  for (const auto &c : ops) {
    ndraws += c.type == gx::GxOpKind::DrawPrimitive;
    nbp += c.type == gx::GxOpKind::BPRegister;
    nxf += c.type == gx::GxOpKind::XFRegister;
  }
  {
    using clk = std::chrono::steady_clock;
    const auto t0 = clk::now();
    g_ph_draw_seen = true;
    g_renderer->draw_frame(ops);
    g_ph.draw += std::chrono::duration<double, std::milli>(clk::now() - t0).count();
    static double acc = 0;
    static unsigned n = 0;
    acc += std::chrono::duration<double, std::milli>(clk::now() - t0).count();
    if (++n == 30) { std::printf("[TAKE] rendu GL (draw_frame) %.1f ms par image\n", acc / n); acc = 0; n = 0; }
  }
  const int s = gfx::g_scale;
  {   // NJ_DUMP_AT=2400,3000,... : EFB complet de ces images dans <stockage>/efb_<n>.ppm (banc A/B au pixel, G1, 14/11).
      // L'EFB plutot que l'image partagee : avant presentation, et lisible partout (MuMu relit noire l'image partagee).
    static unsigned at[16];
    static const int n_at = [] {
      const char *e = std::getenv("NJ_DUMP_AT");
      int n = 0;
      for (const char *q = e; q && *q && n < 16;) {
        at[n++] = (unsigned)std::strtoul(q, nullptr, 10);
        const char *c = std::strchr(q, ',');
        if (!c) break;
        q = c + 1;
      }
      return n;
    }();
    bool want = false;
    for (int k = 0; k < n_at; ++k) want |= frame == at[k];
    if (want) {
      const int w = gfx::efb_w(), h = gfx::efb_h();
      std::vector<unsigned char> img((size_t)w * h * 4);
      GLint cur = 0;
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &cur);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, g_efb_fbo);
      glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img.data());
      glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)cur);
      char path[320];
      const char *d = std::getenv("NJ_STORAGE_DIR");
      std::snprintf(path, sizeof path, "%s/efb_%u.ppm", d ? d : ".", frame);
      if (FILE *f = std::fopen(path, "wb")) {
        std::fprintf(f, "P6 %d %d 255\n", w, h);
        for (int y = h - 1; y >= 0; --y)
          for (int x = 0; x < w; ++x) std::fwrite(&img[4 * ((size_t)y * w + x)], 1, 3, f);
        std::fclose(f);
        std::printf("[NJGFX] EFB de l'image %u ecrit : %s\n", frame, path);
      }
    }
  }
  if (port_studio_capture()) {   // Studio : commande « capture » -> studio_<image>.ppm, 640x480 (un point sur s)
    const int w = gfx::efb_w(), h = gfx::efb_h();
    std::vector<unsigned char> img((size_t)w * h * 4);
    GLint cur = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &cur);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_efb_fbo);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img.data());
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)cur);
    char path[320];
    const char *d = std::getenv("NJ_STORAGE_DIR");
    std::snprintf(path, sizeof path, "%s/studio_%u.ppm", d ? d : ".", frame);
    if (FILE *f = std::fopen(path, "wb")) {
      std::fprintf(f, "P6 %d %d 255\n", w / s, h / s);
      for (int y = h - 1; y >= 0; y -= s)
        for (int x = 0; x < w; x += s) std::fwrite(&img[4 * ((size_t)y * w + x)], 1, 3, f);
      std::fclose(f);
      port_studio_event("capture %s", path);
    }
  }
  {   // NJ_UNIFORME=1 (15/11, ecran beige d'une cinematique en temps reel de r100) : toutes les 15 images, 9 points de
      // l'EFB ; image uniforme et non noire -> EFB ecrit une fois (efb_uniforme_<n>.ppm, 3 au plus) et etat journalise.
    static const bool on = std::getenv("NJ_UNIFORME") != nullptr;
    static int dumps = 0;
    if (on && dumps < 3 && frame % 15 == 0) {
      unsigned char px[4];
      int lo[3] = {255, 255, 255}, hi[3] = {0, 0, 0};
      GLint cur = 0;
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &cur);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, g_efb_fbo);
      for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 3; ++i) {
          glReadPixels((120 + 200 * i) * s, gfx::efb_h() - (100 + 140 * j) * s, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
          for (int k = 0; k < 3; ++k) { lo[k] = std::min(lo[k], (int)px[k]); hi[k] = std::max(hi[k], (int)px[k]); }
        }
      const bool uniform = hi[0] - lo[0] < 6 && hi[1] - lo[1] < 6 && hi[2] - lo[2] < 6 && hi[0] + hi[1] + hi[2] > 150;
      if (uniform) {
        ++dumps;
        NjState st;
        njstate_fill(&st);
        port_studio_event("ecran_uniforme couleur=%d,%d,%d dessins=%zu salle=%03x", hi[0], hi[1], hi[2], ndraws, (unsigned)st.room);
        std::printf("[UNIFORME] image %u : couleur %d,%d,%d partout, %zu dessins, mode %u salle %03x bits %05x\n", frame, hi[0], hi[1],
                    hi[2], ndraws, (unsigned)st.mode, (unsigned)st.room, (unsigned)st.bits);
        // premiere detection : enregistrement .dff des 6 images suivantes (rejeu FifoReplay sur PC, regle du 28/10 :
        // un defaut vu sur l'appareil se rejoue d'abord sur PC, puis se compare dessin par dessin)
        if (dumps == 1) {
          nj::runtime::gx::rec::request(6);
          g_gx_framelog_request = true;
          std::printf("[UNIFORME] enregistrement .dff de 6 images demande\n");
        }
        const int w = gfx::efb_w(), h = gfx::efb_h();
        std::vector<unsigned char> img((size_t)w * h * 4);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img.data());
        char path[320];
        const char *d = std::getenv("NJ_STORAGE_DIR");
        std::snprintf(path, sizeof path, "%s/efb_uniforme_%u.ppm", d ? d : ".", frame);
        if (FILE *f = std::fopen(path, "wb")) {
          std::fprintf(f, "P6 %d %d 255\n", w / s, h / s);   // reduite a 640x480 (un point sur s)
          for (int y = h - 1; y >= 0; y -= s)
            for (int x = 0; x < w; x += s) std::fwrite(&img[4 * ((size_t)y * w + x)], 1, 3, f);
          std::fclose(f);
        }
      }
      glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)cur);
    }
  }
  if (frame % 300 == 0) {   // diagnostic N3 : 64 points de l'EFB (moyenne RGB, points non noirs) et mode du jeu
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
    std::printf("[NJGFX] mesure %u : moyenne %u,%u,%u, %u/64 points allumes, %zu dessins, mode %u salle %03x bits %05x qte %u\n", frame,
                sum[0] / 64, sum[1] / 64, sum[2] / 64, lit, ndraws, (unsigned)st.mode, (unsigned)st.room, (unsigned)st.bits, (unsigned)st.qte);
    if (st.mode == 6 && (frame == 600 || frame == 900)) njstate_debug_ids();   // diagnostic : titre
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
    if (frame == 2400 && !tx.empty()) {
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
  if (frame <= 3 || frame % 600 == 0) {   // diagnostic N3 : ce que le rendu a recu et ce qu'il a dessine
    unsigned char px[4] = {0, 0, 0, 0};
    GLint cur = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &cur);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_efb_fbo);
    glReadPixels(320 * s, gfx::efb_h() - 240 * s, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)cur);
    std::printf("[NJGFX] image %u : %zu commandes (%zu dessins, %zu BP, %zu XF), %u octets lus, pixel central %02x%02x%02x%02x\n",
                frame, nops, ndraws, nbp, nxf, size, px[0], px[1], px[2], px[3]);
    {   // diagnostic N3 : debut de la texture du titre (CMPR 640x360) telle qu'elle est en memoire
      const uint8_t *t = reinterpret_cast<const uint8_t *>(njg_base() | 0x811E3100u);
      std::printf("[NJGFX] texture 011E3100 :");
      for (int i = 0; i < 32; ++i) std::printf(" %02x", t[i]);
      std::printf("\n");
    }
  }
  {   // ce que montre le VI : la zone rendue (fbWidth x efbHeight, en haut de l'EFB) etiree sur toute la largeur,
      // ses lignes centrees dans les 480 de l'ecran (viYOrigin)
    const unsigned *vm = vm_in;
    // 12/11 : l'appli presente elle-meme, comme en 0.1.9 (present_image : zone copiee dans l'XFB, 16:9,
    // place des commandes) ; on lui envoie l'EFB tel quel et la taille de cette zone.
    const unsigned fw = vm[0] >= 16 && vm[0] <= 640 ? vm[0] : 640;
    const unsigned eh = vm[1] >= 16 && vm[1] <= 480 ? vm[1] : 480;
    njlink_set_xfb(fw, eh);
    njlink_frame_fbo(g_efb_fbo, 0, gfx::efb_h() - 480 * s, 640 * s, 480 * s);
  }
}
