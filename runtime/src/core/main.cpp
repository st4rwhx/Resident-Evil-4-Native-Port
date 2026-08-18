// Nightjar : point d'entree. Fenetre et audio (host_video), sources d'entree (input_setup),
// lanceur (disque du joueur), demarrage de la console (console_boot), fil du jeu (run_game, code
// traduit ou moteur seul P3), fil de lecture du FIFO GX, puis la boucle hote : evenements,
// rendu de ce que le GX a produit, presentation, audio.
#include <algorithm>
#if defined(__APPLE__)
#include <dirent.h>   // iOS : recherche du disque dans les Documents
#endif
#if defined(__ANDROID__) || defined(__APPLE__)
#include <unistd.h>
#endif
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <cstring>
#include <iostream>
#include <memory>
#include <cctype>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <SDL.h>

#include "boot/program.h"
#include "common/gl.h"
#include "common/platform.h"
#include "host_video.h"
#include "input/input_manager.h"
#include "input/mouse_events.h"
#include "input/re4_context.h"
#include "input/touch_overlay.h"
#include "input/touch_source.h"
#include "launcher/host.h"
#include "launcher/launcher.h"
#include "runtime/disc_image.h"
#include "runtime/native/disc_extract.h"
#include "runtime/native/native_core.h"
#include "runtime/p3_restart.h"
#include "runtime/config.h"
#include "runtime/console_boot.h"
#include "runtime/cpu_context.h"
#include "runtime/env.h"
#include "runtime/game_version.h"
#include "runtime/gpu_select.h"
#include "runtime/gfx_settings.h"
#include "runtime/gx_state.h"
#include "runtime/gx/fifo_record.h"
#include "runtime/host_clock.h"
#include "runtime/hw/hw.h"
#include "runtime/hw/io_bus.h"
#include "runtime/log.h"
#include "runtime/dev_tour.h"
#include "runtime/njprof.h"
#include "runtime/texture_cache_gl.h"
#include "runtime/crash_report.h"
#include "runtime/diag.h"
#include "runtime/dispatch.h"
#include "runtime/mods.h"
#include "runtime/perf_journal.h"
#include "runtime/timers.h"
#include "runtime/virtual_disc.h"

extern "C" void run_game(nj::runtime::GuestCpu &ctx);
extern void gx_pump_fifo();
extern void gx_native_pump();
extern size_t gx_carry_size();
extern size_t gx_parse_pending();
extern uint64_t g_st_frames;
extern std::atomic<uint64_t> g_fl_xfb_copies;
namespace nj::runtime::gx { extern std::atomic<uint64_t> g_tex_copies; }
extern bool GXInterpTick();
extern void GX_StartParseThread();
extern void gx_xfb_info(uint32_t *, unsigned *, unsigned *, unsigned *);
extern uint64_t g_st_frames, g_stat_draws, g_st_parse_us, g_st_render_us;
extern uint64_t g_fl_swap_us, g_fl_swaps;
extern uint64_t nj_stat_shaders();
extern std::atomic<bool> g_gx_framelog_request;
extern std::atomic<int> g_tex_verify_all;   // texture_cache_gl.cpp
#if defined(_WIN32) && !defined(NJ_RELEASE) && !defined(NJ_MOBILE)
extern void nj_live_publish(GLuint, int, int, int, int);   // live_frame.cpp
#endif

namespace nj::runtime {
// Etat global partage avec le reste du moteur.
uint32_t g_debug_pc = 0;
MMU *g_guest_mem = nullptr;
GuestCpu *g_guest = nullptr;
extern uint64_t g_ppc_calls;      // os_dispatch.cpp
extern uint64_t g_interp_insts;   // interpreteur
namespace hw { uint32_t vi_xfb_top_address(); extern uint32_t g_vi_top_base, g_vi_btm_field_base; }
namespace input {
extern std::atomic<int> g_shot_request;
void install_input_sources();     // input_setup.cpp
}
}  // namespace nj::runtime

namespace rt = nj::runtime;
namespace gfx = nj::runtime::gfx;

#ifdef NJ_OS_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// Carte dediee par defaut sur les PC a deux cartes (12/10) : les pilotes NVIDIA (Optimus) et AMD
// (PowerXpress) lisent ces exports de l'exe au lancement ; sans carte dediee, sans effet.
// runtime/gpu_select.h : le choix « economie » du joueur les remet a 0 avant le contexte GL.
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// Host sampling profiler (NJ_HOSTPROF=path): suspends the CPU thread about
// every millisecond and histograms where the host is executing. Nothing that
// can take a lock (heap, loader) runs while the thread is suspended -- only
// GetThreadContext and a copy of the top of its stack. Samples outside the
// executable (ntdll, ucrt, ...) are also attributed to the first return
// address on that stack copy that lies in our .text. Every 10 s the window's
// histogram is written as image-relative addresses for nm to symbolize.
static void host_profiler_loop(nj::runtime::GuestCpu *ctx, HANDLE th,
                               const char *path) {
  const uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
  auto *dos = (IMAGE_DOS_HEADER *)base;
  auto *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
  uintptr_t text_lo = base, text_hi = base + nt->OptionalHeader.SizeOfImage;
  IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
  for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i)
    if (std::memcmp(sec[i].Name, ".text", 5) == 0) {
      text_lo = base + sec[i].VirtualAddress;
      text_hi = text_lo + sec[i].Misc.VirtualSize;
    }
  FILE *f = std::fopen(path, "w");
  if (!f) return;
  std::fprintf(f, "base %llx\n", (unsigned long long)base);
  std::unordered_map<uint64_t, uint32_t> self_hist, ext_caller, ext_mod;
  uint64_t samples = 0, ext = 0;
  auto t_last = std::chrono::steady_clock::now();
  int window = 0;
  uint64_t stack[256];
  while (ctx->alive) {
    Sleep(1);
    if (SuspendThread(th) == (DWORD)-1) break;
    CONTEXT c;
    c.ContextFlags = CONTEXT_CONTROL;
    const bool ok = GetThreadContext(th, &c) != 0;
    size_t n = 0;
    if (ok) {
      // Only read committed stack: stop at the page containing Rsp.
      uint64_t room = 0x1000 - (c.Rsp & 0xFFF);
      n = (size_t)std::min<uint64_t>(sizeof(stack), room) / 8;
      std::memcpy(stack, (void *)c.Rsp, n * 8);
    }
    ResumeThread(th);
    if (!ok) continue;
    ++samples;
    const uint64_t rip = c.Rip;
    if (rip >= text_lo && rip < text_hi) {
      ++self_hist[rip - base];
    } else {
      ++ext;
      ++ext_mod[rip >> 16];
      uint64_t caller = 0;
      for (size_t k = 0; k < n; ++k)
        if (stack[k] >= text_lo && stack[k] < text_hi) {
          caller = stack[k] - base;
          break;
        }
      ++ext_caller[caller];
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - t_last < std::chrono::seconds(10)) continue;
    t_last = now;
    window += 10;
    auto dump = [&](const char *tag, std::unordered_map<uint64_t, uint32_t> &h,
                    size_t top) {
      std::vector<std::pair<uint64_t, uint32_t>> v(h.begin(), h.end());
      std::sort(v.begin(), v.end(),
                [](auto &a, auto &b) { return a.second > b.second; });
      for (size_t k = 0; k < v.size() && k < top; ++k)
        std::fprintf(f, "%s %llx %u\n", tag, (unsigned long long)v[k].first,
                     v[k].second);
      h.clear();
    };
    std::fprintf(f, "W %d samples=%llu ext=%llu inst=%llu\n", window,
                 (unsigned long long)samples, (unsigned long long)ext,
                 (unsigned long long)ctx->retired);
    // Name the modules behind external samples (safe now: thread resumed).
    std::vector<std::pair<uint64_t, uint32_t>> mods(ext_mod.begin(), ext_mod.end());
    std::sort(mods.begin(), mods.end(),
              [](auto &a, auto &b) { return a.second > b.second; });
    for (size_t k = 0; k < mods.size() && k < 8; ++k) {
      char name[MAX_PATH] = "?";
      HMODULE hm;
      if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                 GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             (LPCSTR)(uintptr_t)(mods[k].first << 16), &hm))
        GetModuleFileNameA(hm, name, MAX_PATH);
      std::fprintf(f, "M %s %u\n", name, mods[k].second);
    }
    ext_mod.clear();
    dump("S", self_hist, SIZE_MAX);
    dump("E", ext_caller, 30);
    std::fflush(f);
    samples = ext = 0;
  }
  std::fclose(f);
}
#endif  // NJ_OS_WINDOWS

// Fil du jeu termine (sortie de Windows, 12/10).
static std::atomic<bool> g_cpu_done{false};

// Fil du jeu : le code traduit tourne jusqu'a l'arret demande par la boucle hote.
static void cpu_thread_main(rt::GuestCpu *ctx) {
  rt::g_guest = ctx;
  rt::njprof::register_thread("jeu");
#ifdef _WIN32
  if (const char *prof = NJ_ENV("NJ_HOSTPROF")) {
    HANDLE self = nullptr;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &self, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    std::thread(host_profiler_loop, ctx, self, prof).detach();
  }
#endif
  NJ_LOG << "[Thread] CPU Core started. pc=" << std::hex << ctx->pc << std::dec << std::endl;
  run_game(*ctx);
  if (ctx->alive)
    NJ_LOG << "[Thread] CPU Core CRASHED (jumped to 0). Final PC: 0x" << std::hex << ctx->pc << ", LR: 0x"
             << ctx->lr << std::dec << std::endl;
  else
    NJ_LOG << "[Thread] CPU Core exited: shutdown requested (window closed or quit). NOT a crash." << std::endl;
  g_cpu_done = true;
}

// NJ_THREADS=1 : a la sortie, une fois le fil CPU arrete, parcourt la liste des threads de
// l'OS invite et imprime etat, file d'attente, pc sauve et chaine de piles : le plus court chemin
// pour voir qui est bloque sur quoi. NJ_THREADS=<s> (s > 1) : meme vidage pendant la partie,
// une fois, s secondes apres le depart (instantane non gele, suffisant pour les threads endormis).
// Decalages : OSThread/OSContext du SDK Dolphin.
static void dump_guest_threads(nj::runtime::MMU &m) {

  
  auto in_ram = [](uint32_t a) { return a >= 0x80000000u && a < 0x81800000u; };
  const uint32_t cur = m.read32(0x800000E4);
  NJ_LOG << std::hex << "[Threads] current=0x" << cur << " curctx=0x"
        << m.read32(0x800000D4) << "\n";
  uint32_t th = m.read32(0x800000DC);
  for (int n = 0; in_ram(th) && n < 32; ++n) {
    const uint32_t r1 = m.read32(th + 0x04);
    NJ_LOG << "  thread 0x" << th << (th == cur ? " *" : "")
          << " state=" << std::dec << m.read16(th + 0x2C8)
          << " suspend=" << (int32_t)m.read32(th + 0x2CC)
          << " prio=" << m.read32(th + 0x2D0) << std::hex
          << " waitq=0x" << m.read32(th + 0x2DC)
          << " mutex=0x" << m.read32(th + 0x2F0)
          << " pc=0x" << m.read32(th + 0x198)
          << " lr=0x" << m.read32(th + 0x84) << " r1=0x" << r1
          << " r3=0x" << m.read32(th + 0x0C) << " stack=0x"
          << m.read32(th + 0x308) << "-0x" << m.read32(th + 0x304)
          << "\n    backchain:";
    for (uint32_t sp = r1, f = 0; in_ram(sp) && f < 16; ++f) {
    const uint32_t next = m.read32(sp);
    if (!in_ram(next) || next <= sp)
      break;
    NJ_LOG << " 0x" << m.read32(next + 4);
    sp = next;
    }
    NJ_LOG << "\n";
    th = m.read32(th + 0x2FC);
  }
  NJ_LOG << std::dec << std::flush;
  }


namespace {

// Les singletons partages entre fils sont construits ici, avant tout autre fil : le moteur
// Android est compile sans gardes thread-safe des statiques locales.
void construct_singletons() {
  (void)rt::Config::get();
  (void)rt::input::InputHub::get();
  (void)rt::DiscView::get();
  (void)rt::Timers::get();
  (void)rt::hle::TexCache::get();
  (void)rt::IoBus::get();
}

// Build de diffusion : aucun journal nulle part (flux ecartes, meme rediriges), et sous Windows
// les DLL chargees en route ne se cherchent que dans System32.
void silence_release_build() {
#if defined(NJ_RELEASE) && !defined(NJ_DEVTOOLS)
#ifdef _WIN32
  SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
  std::freopen("NUL", "w", stdout);
  std::freopen("NUL", "w", stderr);
#else
  std::freopen("/dev/null", "w", stdout);
  std::freopen("/dev/null", "w", stderr);
#endif
  for (std::ostream *s : {&std::cout, &std::cerr, &std::clog})
    s->setstate(std::ios::badbit);
#endif
}

// Reglages d'essai par l'environnement : echelle interne (NJ_IRES ; captures NJ_SCREENSHOT a
// l'echelle 1, comparables a Dolphin) et cadence d'affichage (NJ_FPS, 0 = sans limite).
void apply_display_overrides() {
  if (const char *ir = NJ_ENV("NJ_IRES"))
    gfx::g_scale_setting = std::clamp(std::atoi(ir), 0, gfx::kMaxScale);
  else if (NJ_ENV("NJ_SCREENSHOT"))
    gfx::g_scale_setting = 1;
  if (const char *fps = NJ_ENV("NJ_FPS"))
    gfx::g_fps_target = std::max(0, std::atoi(fps));
}

// Ce que la boucle hote garde d'un tour a l'autre.
struct HostLoop {
  rt::HostVideo hv;
  rt::GuestCpu *ctx = nullptr;
  bool quit = false;
  int64_t last_input_us = 0;
  // image affichee par la VI (XFB) : adresse, taille, pas
  uint32_t xfb_addr = 0;
  unsigned xw = 0, xh = 0, xstride = 0;
  // videos Sofdec : XFB converti en RGBA, texture a jour
  std::vector<unsigned char> xfb_rgba;
  unsigned xfb_tex_w = 0, xfb_tex_h = 0;
  bool movie_frame = false;
};

// Evenements et entrees au plus toutes les 2 ms (NJ_INPUT_US) : la boucle tourne ~1000 fois
// par seconde, le jeu lit la manette a 60 Hz, et pomper les messages Windows a chaque tour
// prenait 18 % du fil hote. Souris et boutons viennent des evenements (le sondage rendait un
// masque vide en capture relative).
// Moteur natif (NJ_NATIVE=1) : le noyau du jeu tourne a la place du code recompile.
bool g_native = false;
// Moteur natif demande : par NJ_NATIVE, ou d'office dans la variante « natif seul » (NJ_NATIVE_ONLY,
// build d'essai sans code recompile, lancee par son icone).
bool native_requested() {
#ifdef NJ_NATIVE_ONLY
  return true;
#else
  return NJ_ENV("NJ_NATIVE") != nullptr;
#endif
}

void pump_input(HostLoop &L) {
  static const int64_t period_us = [] {
    const char *e = NJ_ENV("NJ_INPUT_US");
    return e ? (int64_t)std::atoll(e) : (int64_t)2000;
  }();
  const int64_t now = nj::host_coarse_us();
  if (now - L.last_input_us < period_us)
    return;
  L.last_input_us = now;
  // Essais seulement (NJ_ENV : rien en diffusion) : Leon invincible, pour mesurer la cadence au
  // contact des ennemis (banc « mura »). Disque debug G4BE08 (decompile) : pPL en 0x803159F4,
  // cEm::hp en +0x320 remis a cEm::hp_max (+0x322) a chaque tour.
  static const bool god = NJ_ENV("NJ_GODMODE") != nullptr;
  if (god && L.ctx) {
    auto &m = L.ctx->mmu;
    const uint32_t pl = m.read32(0x803159F4u);
    if (pl >= 0x80000000u && pl < 0x81800000u)
      m.write16(pl + 0x320, m.read16(pl + 0x322));
  }
  SDL_Event ev;
#if defined(__APPLE__)
  // iPhone : journal des changements de fenetre (Centre de controle, rotation, retour d'application) :
  // taille de la fenetre en points, taille reelle de dessin en pixels, zone sure, focus. 300 lignes au plus.
  static int win_logged = 0;
  auto log_window = [&](const char *why) {
    if (win_logged >= 300) return;
    ++win_logged;
    int ww = 0, wh = 0, dw = 0, dh = 0;
    SDL_GetWindowSize(L.hv.window, &ww, &wh);
    SDL_GL_GetDrawableSize(L.hv.window, &dw, &dh);
    SDL_Rect safe{};
    SDL_GetDisplayUsableBounds(0, &safe);
    SDL_DisplayMode dm{};
    SDL_GetCurrentDisplayMode(0, &dm);
    NJ_LOG << "[Fenetre] " << why << " : fenetre " << ww << "x" << wh << ", dessin " << dw << "x" << dh << ", zone sure "
           << safe.x << "," << safe.y << " " << safe.w << "x" << safe.h << ", ecran " << dm.w << "x" << dm.h
           << ", drapeaux 0x" << std::hex << SDL_GetWindowFlags(L.hv.window) << std::dec << std::endl;
  };
  {
    static int last_w = -1, last_h = -1, last_dw = -1, last_dh = -1;
    int ww = 0, wh = 0, dw = 0, dh = 0;
    SDL_GetWindowSize(L.hv.window, &ww, &wh);
    SDL_GL_GetDrawableSize(L.hv.window, &dw, &dh);
    if (ww != last_w || wh != last_h || dw != last_dw || dh != last_dh) {
      last_w = ww; last_h = wh; last_dw = dw; last_dh = dh;
      log_window("tailles changees");
    }
  }
#endif
  while (SDL_PollEvent(&ev)) {
#if defined(__APPLE__)
    if (ev.type == SDL_WINDOWEVENT) {
      char why[64];
      std::snprintf(why, sizeof why, "evenement fenetre %d (%d, %d)", int(ev.window.event), ev.window.data1, ev.window.data2);
      log_window(why);
    } else if (ev.type == SDL_DISPLAYEVENT) {
      char why[64];
      std::snprintf(why, sizeof why, "evenement ecran %d (%d)", int(ev.display.event), ev.display.data1);
      log_window(why);
    } else if (ev.type >= SDL_APP_TERMINATING && ev.type <= SDL_APP_DIDENTERFOREGROUND) {
      char why[64];
      std::snprintf(why, sizeof why, "evenement application %d", int(ev.type));
      log_window(why);
    }
#endif
    if (ev.type == SDL_QUIT)
      L.quit = true;
    if (ev.type == SDL_DROPFILE) {   // un mod depose sur la fenetre : installe, menu F10 ouvert
      if (ev.drop.file) {
        std::string dropped = ev.drop.file;
        std::string ext = dropped.size() > 4 ? dropped.substr(dropped.size() - 4) : std::string();
        for (char &ch : ext) ch = char(std::tolower((unsigned char)ch));
        if (ext == ".gci" || ext == ".raw")   // une sauvegarde : proposee dans l'onglet Saves (13/10)
          gfx::saves_offer(ev.drop.file);
        else
          nj::mods::install_async(ev.drop.file);
        SDL_free(ev.drop.file);
        if (!gfx::g_menu_open)
          gfx::menu_toggle();
      }
      continue;
    }
    gfx::menu_event(&ev);
    if (gfx::g_menu_open || gfx::g_title_home) {   // le menu F10 (et l'ecran d'accueil) garde tout, sauf la fin d'un toucher
      if (ev.type == SDL_FINGERUP)
        rt::input::touch_event_feed(ev);
      continue;
    }
    rt::input::mouse_event_feed(ev);
    rt::input::touch_event_feed(ev);
  }
  rt::input::InputHub::get().update();
  if (g_native) {
    const auto p = rt::input::InputHub::get().pad(0);
    // commandes Wii : accelerometre ou glisser au couteau, en centiemes de g (touch_source.cpp)
    const float g = rt::input::wii_accel_g();
    const uint16_t cg = (uint16_t)(g <= 0.0f ? 0.0f : g >= 600.0f ? 60000.0f : g * 100.0f);
    nj::native::pad({p.buttons, p.stick_x, p.stick_y, p.substick_x, p.substick_y, p.trigger_l, p.trigger_r, cg});
  }
}

// Echelle interne voulue pour la fenetre d'aujourd'hui (menu F10, redimensionnement) : l'EFB est
// realloue entre deux images ; puis il redevient la cible du rendu.
void bind_efb(HostLoop &L) {
  // Essai (12/10, plantage au passage 720p -> 1080p) : NJ_TEST_RESIZE=<s>:<l>x<h>[,<s>:<l>x<h>...]
  // redimensionne la fenetre comme le menu F10, <s> secondes apres le premier passage ici. Rien
  // en diffusion (NJ_ENV y vaut nul).
  if (static const char *rs = NJ_ENV("NJ_TEST_RESIZE"); rs) {
    static const auto t0 = std::chrono::steady_clock::now();
    static const char *next = rs;
    int sec = 0, w = 0, h = 0, used = 0;
    if (next && std::sscanf(next, "%d:%dx%d%n", &sec, &w, &h, &used) == 3 &&
        std::chrono::steady_clock::now() - t0 >= std::chrono::seconds(sec)) {
      next = next[used] == ',' ? next + used + 1 : nullptr;
      NJ_LOG << "[Essai] fenetre -> " << w << "x" << h << std::endl;
      SDL_SetWindowSize(L.hv.window, w, h);
      SDL_SetWindowPosition(L.hv.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
  }
  int dw = 0, dh = 0;
  SDL_GL_GetDrawableSize(L.hv.window, &dw, &dh);
  const int want = gfx::wanted_scale(dw, dh);
  if (want != gfx::g_scale && dw > 0 && dh > 0) {
    gfx::g_scale = want;
    rt::host_video_resize_efb(L.hv);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, L.hv.efb_fbo);
  glViewport(0, 0, gfx::efb_w(), gfx::efb_h());
}

// F12 : journal de tous les dessins de l'image suivante (renderer_gl.cpp).
void poll_framelog_key() {
  static bool was_down = false;
  const Uint8 *keys = SDL_GetKeyboardState(nullptr);
  const bool down = keys && keys[SDL_SCANCODE_F12];
  if (down && !was_down) {
    g_gx_framelog_request = true;
    g_tex_verify_all = 1;
    NJ_LOG << "[FrameLog] requested" << std::endl;
  }
  was_down = down;
  {
    // Enregistrement .dff du flux graphique (fifo_record.h) : F11, ou quatre doigts poses 0,7 s (telephones).
    static bool f11_was = false;
    const bool f11 = keys && keys[SDL_SCANCODE_F11];
    bool go = f11 && !f11_was;
    f11_was = f11;
    static int fingers_ms_start = -1;
    int fingers = 0;
    for (int t = 0, nt = SDL_GetNumTouchDevices(); t < nt; ++t) fingers += SDL_GetNumTouchFingers(SDL_GetTouchDevice(t));
    const int now_ms = (int)(SDL_GetTicks() & 0x7FFFFFFF);
    if (fingers >= 4) {
      if (fingers_ms_start < 0) fingers_ms_start = now_ms;
      if (now_ms - fingers_ms_start > 700 && now_ms - fingers_ms_start < 100000) {
        go = true;
        fingers_ms_start = now_ms + 100000;   // une seule demande par appui
      }
    } else {
      fingers_ms_start = -1;
    }
    if (go && !nj::runtime::gx::rec::active() && !nj::runtime::gx::rec::armed()) {
      nj::runtime::gx::rec::request(6);
      g_gx_framelog_request = true;   // journal de tous les dessins de l'image en cours, a comparer au rejeu PC (NJ_FIFOLOG)
      NJ_LOG << "[FIFO] enregistrement demande (6 images)" << std::endl;
    }
  }
#if defined(__APPLE__)
  // iPhone : pas de touche F12, la verification complete des textures se declenche toute seule toutes les 15 s
  // (3 images chacune, 40 passages au plus) ; le journal [TexVerif] dit alors si une texture etait perimee.
  static bool shown = false;
#ifndef NJ_RELEASE   // les noms des interrupteurs de dev ne doivent pas figurer dans un build de diffusion
  if (!shown) {
    shown = true;
    const char *names[] = {"NJ_NOVOLCOPY", "NJ_VOLBIG", "NJ_VOLTRIM", "NJ_DECCACHE", "NJ_NOINVAL", "NJ_VOLSITES"};
    std::string s = "[Reglages] dev_env.txt :";
    for (const char *n : names) {
      const char *v = std::getenv(n);
      s += std::string(" ") + n + "=" + (v ? v : "-");
    }
    NJ_LOG << s << std::endl;
  }
#endif
  {
    static uint32_t last_room = 0xFFFFFFFFu;
    const uint32_t room = nj::perf::current_room();
    if (room != last_room) {
      last_room = room;
      char b[48];
      std::snprintf(b, sizeof b, "[Salle] id=0x%03X", (unsigned)room);
      NJ_LOG << b << std::endl;
    }
  }
  static auto last = std::chrono::steady_clock::now();
  static int passes = 0;
  const auto now = std::chrono::steady_clock::now();
  if (passes < 40 && now - last > std::chrono::seconds(15)) {
    last = now;
    ++passes;
    g_tex_verify_all = 1;
  }
#endif
}

// Videos Sofdec : le decodeur du jeu ecrit chaque image dans le XFB (YUYV) que la VI affiche,
// sans passer par le GX ; presenter l'EFB montrait une image figee ou le fond de la fenetre. Pendant
// une video, le XFB est converti (BT.601) et presente a sa place.
void capture_movie_xfb(HostLoop &L) {
  gx_xfb_info(&L.xfb_addr, &L.xw, &L.xh, &L.xstride);
  const uint32_t gx_xfb = L.xfb_addr;
  if (const uint32_t top = rt::hw::vi_xfb_top_address())
    L.xfb_addr = top;
  const bool movie = rt::input::re4_context_read().mode == rt::input::Re4Mode::Movie;
  // Releve des videos (13/10, journal chiffre) : une fois par seconde pendant une video, les
  // registres de trame bruts, l'adresse retenue, la taille et le pas, et combien d'octets non nuls on
  // trouve a l'adresse retenue et a celle de la derniere copie EFB -> XFB (image verte = zeros).
  if (movie) {
    static uint64_t last_ms = 0;
    const uint64_t now = SDL_GetTicks64();
    if (now - last_ms >= 1000) {
      last_ms = now;
      auto nonzero = [&](uint32_t a) -> unsigned {
        if (!a) return 0;
        unsigned nz = 0;
        for (uint32_t i = 0; i < 4096; i += 4) nz += L.ctx->mmu.read32((a | 0x80000000u) + 64 * 1024 + i) != 0;
        return nz;
      };
      NJ_DIAG("[VI] video : tfbl=%08x bfbl=%08x adresse=%08x gx=%08x %ux%u pas=%u non-nuls=%u gx-non-nuls=%u%s",
              rt::hw::g_vi_top_base, rt::hw::g_vi_btm_field_base, L.xfb_addr, gx_xfb, L.xw, L.xh, L.xstride,
              nonzero(L.xfb_addr), nonzero(gx_xfb), "\n");
      // file des commandes GX et ce qui la vide : images rendues, copies vers l'XFB et vers une texture
      static uint64_t l_frames = 0, l_xfb = 0, l_tex = 0;
      const uint64_t fr = g_st_frames, xc = g_fl_xfb_copies.load(), tc = rt::gx::g_tex_copies.load();
      NJ_DIAG("[GX] video : file=%zu attente=%zu images=%llu copies-xfb=%llu copies-texture=%llu%s",
              gx_carry_size(), gx_parse_pending(), (unsigned long long)(fr - l_frames),
              (unsigned long long)(xc - l_xfb), (unsigned long long)(tc - l_tex), "\n");
      l_frames = fr;
      l_xfb = xc;
      l_tex = tc;
    }
  }
  L.movie_frame = false;
  if (!L.xfb_addr || !L.xw || !L.xh || L.xw > 720 || L.xh > 576 || !(movie || NJ_ENV("NJ_XFB")))
    return;
  // 13/10 : sur les disques commerciaux la video passe par le GX (copie EFB -> XFB, gardee sur la carte
  // graphique) : la memoire de l'XFB reste a zero et la lire donnait un ecran vert. Image GX normale
  // dans ce cas ; le chemin XFB ne sert qu'a une image ecrite par le processeur (disque debug).
  {
    unsigned nz = 0;
    for (uint32_t i = 0; i < 4096 && !nz; i += 4)
      nz += L.ctx->mmu.read32((L.xfb_addr | 0x80000000u) + 64 * 1024 + i) != 0;
    if (!nz)
      return;
  }
  L.movie_frame = movie;
  const unsigned w = L.xw, h = L.xh;
  L.xfb_rgba.resize((size_t)w * h * 4);
  auto sat = [](float v) -> unsigned char { return (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v); };
  rt::MMU &m = L.ctx->mmu;
  for (unsigned y = 0; y < h; ++y) {
    const uint32_t row = (L.xfb_addr | 0x80000000u) + y * L.xstride;
    const uint8_t *span = m.get_span(row, (uint64_t)w * 2);
    auto at = [&](unsigned i) -> uint8_t { return span ? span[i] : m.read8(row + i); };
    for (unsigned x = 0; x < w; x += 2) {
      const unsigned i = x * 2;
      const float cb = (float)at(i + 1) - 128.0f, cr = (float)at(i + 3) - 128.0f;
      const uint8_t luma[2] = {at(i), at(i + 2)};
      for (unsigned k = 0; k < 2 && x + k < w; ++k) {
        const float yy = (float)luma[k];
        unsigned char *px = &L.xfb_rgba[((size_t)y * w + x + k) * 4];
        px[0] = sat(yy + 1.371f * cr);
        px[1] = sat(yy - 0.336f * cb - 0.698f * cr);
        px[2] = sat(yy + 1.732f * cb);
        px[3] = 255;
      }
    }
  }
  glBindTexture(GL_TEXTURE_2D, L.hv.xfb_texture);
  if (w != L.xfb_tex_w || h != L.xfb_tex_h) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, L.xfb_rgba.data());
    L.xfb_tex_w = w;
    L.xfb_tex_h = h;
  } else {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, L.xfb_rgba.data());
  }
}

// Le flux GX est vide aussi vite que possible (le jeu attend ce vidage) ; seule la presentation suit
// la cadence choisie. Au-dessus de 30 : presenter du neuf (image du jeu ou intermediaire), sinon une
// fois par 1/60 s pour le curseur (echanger a 144 Hz une image deja montree fait attendre le pilote).
// NJ_HOSTLOOP=1 : boucle allegee de l'hote (essai du 14/11, aucun gain mesure) ; coupee par defaut.
bool host_loop_lean() {
  static const bool lean = [] { const char *e = NJ_ENV("NJ_HOSTLOOP"); return e && e[0] == '1'; }();
  return lean;
}

// NJ_HOSTWAIT=0 : ancienne attente active (A/B).
bool host_wait() {
  static const bool on = [] { const char *e = NJ_ENV("NJ_HOSTWAIT"); return !(e && e[0] == '0'); }();
  return on;
}

bool render_gx(HostLoop &L) {
  const uint64_t before = g_st_frames;
  if (g_native)
    gx_native_pump();
  else
    gx_pump_fifo();
  const bool fresh = g_st_frames != before;
  bool due = gfx::present_due(fresh);
  // Coeur natif, cadence fixe (gx.cpp, gx_native_pump) : presenter a chaque balayage (la synchro verticale cadence la
  // boucle) pour que l'image neuve en reserve soit montree au bon balayage. Sans synchro verticale, ancien comportement
  // (sinon la boucle tournerait a vide). NJ_PACING=0 : ancien comportement.
  static const bool pacing = [] { const char *e = NJ_ENV("NJ_PACING"); return !(e && e[0] == '0'); }();
#if !defined(__APPLE__)
  if (g_native && pacing && rt::Config::get().vsync_on && gfx::g_fps_target <= 30)
    due = true;
#endif
  bool interp = false;
  if (due && gfx::g_fps_target != 30 && !gfx::g_menu_open) {
    static std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    // Coeur natif (14/11) : il produit ses images lui-meme (au plus 30 par seconde) ; reafficher la meme a 60 Hz ne
    // servait qu'au calque tactile et prenait du processeur au jeu sur le S5 (3-4 coeurs brides partages). Image
    // inchangee : au plus toutes les 33 ms. Seulement avec NJ_HOSTLOOP=1.
    const long min_us = (g_native && host_loop_lean()) ? 33300 : 16600;
    interp = !fresh && GXInterpTick();
    due = fresh || interp || now - last >= std::chrono::microseconds(min_us);
    if (due)
      last = now;
  }
  if (g_native) {
    const auto now = std::chrono::steady_clock::now();   // compteur de presentation (15/11, « images qui reviennent en arriere ») : toutes les 5 s
      static unsigned n_fresh = 0, n_pres = 0, n_interp = 0, per = 0, hist[4] = {0, 0, 0, 0};
      static std::chrono::steady_clock::time_point t_fresh = now, t_rep = now;
      static double gap_max = 0, gap_sum = 0;
      if (interp) ++n_interp;
      if (due) { ++n_pres; ++per; }
      if (fresh) {
        const double gap = std::chrono::duration<double, std::milli>(now - t_fresh).count();
        t_fresh = now;
        if (n_fresh) { gap_sum += gap; if (gap > gap_max) gap_max = gap; }
        ++n_fresh;
        ++hist[per > 3 ? 3 : per];   // presentations pendant l'image precedente (2 attendues a 60 Hz)
        per = 0;
      }
      if (now - t_rep >= std::chrono::seconds(5)) {
        NJ_LOG << "[PRESENT] 5 s : images neuves " << n_fresh << ", presentations " << n_pres << ", interpolees " << n_interp
               << " ; presentations par image 0:" << hist[0] << " 1:" << hist[1] << " 2:" << hist[2] << " 3+:" << hist[3]
               << " ; ecart moyen " << (n_fresh > 1 ? gap_sum / (n_fresh - 1) : 0.0) << " ms, max " << gap_max << " ms"
               << " (cible fps " << gfx::g_fps_target << ")" << std::endl;
        n_fresh = n_pres = n_interp = 0; hist[0] = hist[1] = hist[2] = hist[3] = 0; gap_max = gap_sum = 0;
        t_rep = now;
      }
    }
  return due;
}

// Toutes les 5 s : images, dessins, temps d'analyse et de rendu, nuanceurs ; instructions du jeu
// par seconde et part interpretee ; NJ_INTERPLOG (bilan de l'interpreteur).
void report_stats(HostLoop &L) {
  static std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  static int windows = 0;
  static uint64_t last_inst = 0, last_ppc = 0, last_interp = 0;
  const auto now = std::chrono::steady_clock::now();
  if (now - t0 < std::chrono::seconds(5))
    return;
  t0 = now;
  ++windows;
  NJ_LOG << "[STAT] " << windows * 5 << "s frames=" << g_st_frames << " draws=" << g_stat_draws
           << " parse_ms=" << g_st_parse_us / 1000 << " render_ms=" << g_st_render_us / 1000
           << " shaders=" << nj_stat_shaders() << std::endl;
  g_st_parse_us = 0;
  g_st_render_us = 0;
  if (const char *thr = NJ_ENV("NJ_THREADS"); thr && std::atoi(thr) > 1) {
    static bool done;
    if (!done && windows * 5 >= std::atoi(thr)) {
      done = true;
      dump_guest_threads(L.ctx->mmu);
    }
  }
  rt::interp_report(std::cout);
  const uint64_t inst = L.ctx->retired, ppc = rt::g_ppc_calls, interp = rt::g_interp_insts;
  const double d = (double)(inst - last_inst);
  NJ_LOG << "[PERF] guest_mips=" << d / 5.0 / 1e6
           << " interp_share=" << (d > 0 ? 100.0 * (double)(interp - last_interp) / d : 0.0) << "%"
           << " ppc_calls_per_s=" << (ppc - last_ppc) / 5 << std::endl;
  last_inst = inst;
  last_ppc = ppc;
  last_interp = interp;
}

// Rectangle de l'EFB que le jeu copie dans son XFB (RE4 : 512 x 448 depuis le haut gauche), en
// pixels de l'EFB a l'echelle interne.
void presented_size(const HostLoop &L, int &w, int &h) {
  w = ((L.xw > 0 && L.xw <= 640) ? (int)L.xw : 640) * gfx::g_scale;
  h = ((L.xh > 0 && L.xh <= 480) ? (int)L.xh : 480) * gfx::g_scale;
}

// Bascule 16:9 : le jeu affiche ses bandes cinema (re4_context) ; un changement doit durer 6
// images (pas de va-et-vient pendant un fondu). NJ_WIDE=0 ou menu F10 « 4:3 » : jamais.
bool update_wide_mode() {
  namespace tch = rt::input::touch;
  static const bool allowed = [] {
    const char *v = NJ_ENV("NJ_WIDE");
    return !(v && v[0] == '0');
  }();
  const bool wide_on = allowed && gfx::g_aspect == 0;
  static int streak = 0;
  const bool want = wide_on && rt::input::re4_context_read().wide;
  streak = want != tch::wide() ? streak + 1 : 0;
  if (streak >= 6) {
    tch::set_wide(want);
    streak = 0;
  }
  return wide_on;
}

// Presentation : l'image du jeu garde ses proportions, centree (4:3, les bandes laterales
// accueillant les commandes tactiles, ou 16:9 pendant les bandes cinema) ; pendant une video, le
// XFB converti (bande 16:9 du milieu, lignes h/8..7h/8, si le 16:9 est permis).
void present_image(HostLoop &L, int draw_w, int draw_h) {
  namespace tch = rt::input::touch;
  nj_fbo(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, draw_w, draw_h);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  nj_wipe(GL_COLOR_BUFFER_BIT);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, L.hv.efb_fbo);
  nj_fbo(GL_DRAW_FRAMEBUFFER, 0);
  const bool wide_on = update_wide_mode();

  if (L.movie_frame && L.xfb_tex_w && L.xfb_tex_h) {
    static GLuint movie_fbo = 0;
    if (!movie_fbo)
      glGenFramebuffers(1, &movie_fbo);
    nj_fbo(GL_READ_FRAMEBUFFER, movie_fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, L.hv.xfb_texture, 0);
    const GLint h = (GLint)L.xfb_tex_h;
    const bool movie_wide = tch::wide();
    const GLint top = movie_wide ? h / 8 : 0, bottom = movie_wide ? h - h / 8 : h;
    const auto r = movie_wide ? tch::wide_rect(draw_w, draw_h) : tch::game_rect(draw_w, draw_h);
    // texture : ligne 0 = haut, d'ou la source retournee
    bool movie_drawn = false;
#if defined(__APPLE__) || defined(__ANDROID__)
    movie_drawn = gfx::present_area(L.hv.xfb_texture, (int)L.xfb_tex_w, (int)L.xfb_tex_h, 0, bottom, (int)L.xfb_tex_w, top,
                                    (GLint)r.x, (GLint)r.y, (GLint)(r.x + r.w), (GLint)(r.y + r.h));
#endif
    if (!movie_drawn)
      glBlitFramebuffer(0, bottom, (GLint)L.xfb_tex_w, top, (GLint)r.x, (GLint)r.y, (GLint)(r.x + r.w),
                        (GLint)(r.y + r.h), GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, L.hv.efb_fbo);
    return;
  }

  int src_w = 0, src_h = 0;
  presented_size(L, src_w, src_h);
  const GLint efb_top = 480 * gfx::g_scale;
  GLint y0 = efb_top - src_h, y1 = efb_top;
  if (tch::wide()) {
    y1 = efb_top - (GLint)(src_h * tch::kWideTop + 0.5f);
    y0 = efb_top - (GLint)(src_h * (tch::kWideTop + tch::kWideH) + 0.5f);
  }
  const auto r = tch::wide() ? tch::wide_rect(draw_w, draw_h) : tch::game_rect(draw_w, draw_h);
  const GLint dx0 = (GLint)r.x, dy0 = (GLint)r.y, dx1 = (GLint)(r.x + r.w), dy1 = (GLint)(r.y + r.h);
  // Menu F10 « Downscaling » : moyenne de surface quand l'image interne depasse la fenetre
  // (present_area.cpp), sinon la reduction bilineaire du GPU.
  // Sur mobile / OpenGLES (notamment iOS sous Metal), glBlitFramebuffer entre une texture FBO
  // et le framebuffer CAEAGLLayer echoue souvent en GL_INVALID_OPERATION (ecran noir).
  // Le dessin par polygone texture (present_area) est toujours supporte et fait un echantillonnage propre.
  const bool shrinking = src_w > dx1 - dx0 || y1 - y0 > dy1 - dy0;
  bool area = (gfx::g_resample == 1 && shrinking);
#if defined(__APPLE__) || defined(__ANDROID__)
  area = true;
#endif
  if (area)
    area = gfx::present_area(L.hv.efb_color, 640 * gfx::g_scale, 480 * gfx::g_scale, 0, y0, src_w, y1,
                             dx0, dy0, dx1, dy1);
  if (!area)
    glBlitFramebuffer(0, y0, src_w, y1, dx0, dy0, dx1, dy1, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  {   // diagnostic natif (ecran noir en jeu alors que l'image recue est allumee, 12/11)
    static unsigned n = 0;
    if (g_native && (++n % 300) == 0) {
      unsigned char a[4] = {}, b[4] = {};
      nj_fbo(GL_READ_FRAMEBUFFER, 0);
      glReadPixels((dx0 + dx1) / 2, (dy0 + dy1) / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, b);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, L.hv.efb_fbo);
      glReadPixels(src_w / 2, (y0 + y1) / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, a);
      NJ_LOG << "[PRESENT] src " << src_w << "x" << src_h << " xw=" << L.xw << " xh=" << L.xh << " wide=" << tch::wide()
             << " y0=" << y0 << " y1=" << y1 << " dst " << dx0 << "," << dy0 << "-" << dx1 << "," << dy1 << " area=" << area
             << " movie=" << (int)L.movie_frame << " efb_milieu=" << (int)a[0] << "," << (int)a[1] << "," << (int)a[2]
             << " ecran_milieu=" << (int)b[0] << "," << (int)b[1] << "," << (int)b[2] << std::endl;
    }
  }
}

// Captures d'essai (NJ_SCREENSHOT=<prefixe>) : periodiques (NJ_SCREENSHOT_EVERY images,
// a partir de NJ_SCREENSHOT_START) et a la demande du script de touches ('P', nommees par
// l'instant du script). Exactement le rectangle presente, en BMP opaque (l'alpha de l'EFB est
// une donnee du jeu, le facteur de profondeur de champ, que les visionneuses prennent pour une
// transparence).
void maybe_screenshot(HostLoop &L) {
  std::string dev_path;   // capture demandee par la visite des salles (NJ_DEVTOOLS) : reduite, dans le dossier de donnees
  const bool dev = nj::dev::shot_pending(dev_path);
  const char *prefix = NJ_ENV("NJ_SCREENSHOT");
  if (!prefix && !dev)
    return;
  if (!prefix)
    prefix = "";
  static int frame = 0;
  static const int every = [] {
    const char *e = NJ_ENV("NJ_SCREENSHOT_EVERY");
    const int n = e ? std::atoi(e) : 300;
    return n > 0 ? n : 300;
  }();
  static const int start = [] {
    const char *e = NJ_ENV("NJ_SCREENSHOT_START");
    return e ? std::atoi(e) : 0;
  }();
  const int request = rt::input::g_shot_request.exchange(0);
  ++frame;
  if (!request && !dev && (frame % every != 0 || frame < start))
    return;
  int w = 0, h = 0;
  presented_size(L, w, h);
  std::vector<unsigned char> px((size_t)w * h * 4), img(px.size());
  glBindFramebuffer(GL_READ_FRAMEBUFFER, L.hv.efb_fbo);
  glReadPixels(0, gfx::efb_h() - h, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
  nj_fbo(GL_READ_FRAMEBUFFER, 0);
  const size_t row = (size_t)w * 4;
  for (int y = 0; y < h; ++y)
    std::memcpy(&img[(size_t)y * row], &px[(size_t)(h - 1 - y) * row], row);
  for (size_t i = 3; i < img.size(); i += 4)
    img[i] = 0xFF;
  if (dev) {   // reduction par moyenne de blocs jusqu'a ~480 px de large, puis ecriture sous le nom demande
    const int f = std::max(1, w / 480);
    const int ow = w / f, oh = h / f;
    std::vector<unsigned char> small((size_t)ow * oh * 4);
    for (int y = 0; y < oh; ++y)
      for (int x = 0; x < ow; ++x) {
        unsigned acc[3] = {0, 0, 0};
        for (int dy = 0; dy < f; ++dy)
          for (int dx = 0; dx < f; ++dx) {
            const unsigned char *q = &img[((size_t)(y * f + dy) * w + (x * f + dx)) * 4];
            acc[0] += q[0];
            acc[1] += q[1];
            acc[2] += q[2];
          }
        unsigned char *o = &small[((size_t)y * ow + x) * 4];
        o[0] = (unsigned char)(acc[0] / (f * f));
        o[1] = (unsigned char)(acc[1] / (f * f));
        o[2] = (unsigned char)(acc[2] / (f * f));
        o[3] = 0xFF;
      }
    if (SDL_Surface *s = SDL_CreateRGBSurfaceFrom(small.data(), ow, oh, 32, ow * 4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000)) {
      SDL_SaveBMP(s, dev_path.c_str());
      SDL_FreeSurface(s);
    }
    return;
  }
  NJ_PRINTF("[Shot] frames=%llu %dx%d\n", (unsigned long long)g_st_frames, w, h);
  if (SDL_Surface *s = SDL_CreateRGBSurfaceFrom(img.data(), w, h, 32, w * 4, 0x000000FF, 0x0000FF00, 0x00FF0000,
                                                0xFF000000)) {
    const std::string name = request ? std::string(prefix) + "_t" + std::to_string(request - 1) + ".bmp"
                                     : std::string(prefix) + "_" + std::to_string(frame) + ".bmp";
    SDL_SaveBMP(s, name.c_str());
    SDL_FreeSurface(s);
  }
}

// Audio : la file de la carte son est tenue a deux ou trois blocs de 1024 echantillons stereo,
// remplie depuis l'interface audio de la console (NJ_AUDIO_MUTE : meme rythme, silence).
// Avant (12/10) : un seul bloc par tour de boucle, soit 30 720 echantillons/s a 30 i/s pour 32 000
// produits ; l'attente montait jusqu'a la limite de la file du jeu (1 s) : son et voix en retard sur
// l'image. Maintenant : autant de blocs qu'il faut, et le son en avance de plus de ~120 ms sur ce
// que la carte joue est jete (le plus ancien d'abord).
void feed_audio(HostLoop &L) {
  if (L.hv.audio <= 0)
    return;
  constexpr Uint32 kBlockBytes = 1024 * 4;
  if (!g_native)
    rt::hw::audio_trim(4096);
  static int16_t block[1024 * 2];
  static const bool mute = NJ_ENV("NJ_AUDIO_MUTE") != nullptr;
  for (;;) {
    const Uint32 queued = SDL_GetQueuedAudioSize(L.hv.audio);
    // au-dessous d'un bloc, on remplit quoi qu'il arrive (silence si le jeu n'a rien) : pas de trou
    if (queued >= 3 * kBlockBytes || (queued >= kBlockBytes && rt::hw::audio_buffered() < 1024))
      break;
    if (g_native) {
      if (nj::native::audio(block, 1024) <= 0)
        std::memset(block, 0, sizeof block);
    } else {
      rt::hw::audio_pull(block, 1024);
    }
    if (mute)
      std::memset(block, 0, sizeof block);
    SDL_QueueAudio(L.hv.audio, block, sizeof block);
  }
}

// Apres chaque tour : la VI peut etre cadencee par l'hote (NJ_HOSTVI), et le jeu est invite a
// regarder ses interruptions (sauf l'oracle CPU deterministe).
void nudge_guest(rt::GuestCpu &ctx) {
  if (NJ_ENV("NJ_HOSTVI"))
    ctx.vblank_due = true;
  if (!NJ_ENV("NJ_DETERMINISTIC"))
    rt::hw::poll_now.store(true, std::memory_order_relaxed);
}

void present_frame(HostLoop &L) {
  int draw_w{}, draw_h{};
  SDL_GL_GetDrawableSize(L.hv.window, &draw_w, &draw_h);
  present_image(L, draw_w, draw_h);
  maybe_screenshot(L);
#if defined(_WIN32) && !defined(NJ_RELEASE) && !defined(NJ_MOBILE)
  {
    // Image du jeu pour le pont de debug (live_frame.cpp), avant menus et incrustations.
    int w = 0, h = 0;
    presented_size(L, w, h);
    nj_live_publish(L.hv.efb_fbo, 0, gfx::efb_h() - h, w, h);
  }
#endif
  // preparation finie (code traduit installe) : on relance une fois, dans un processus neuf (p3_restart.h)
#ifdef NJ_RELEASE   // builds de diffusion seulement : les bancs d'essai du moteur de developpement ne se relancent pas
  if (rt::g_p3_progress.phase.load() == 0 && nj::runtime::g_p3_restart_wanted.exchange(false)) {
    namespace fs = std::filesystem;
    const std::string flag = nj::launcher::host::data_dir() + "/prep_restart.flag";
    std::error_code ec;
    bool recent = false;
    if (fs::exists(fs::u8path(flag), ec)) {
      const auto age = fs::file_time_type::clock::now() - fs::last_write_time(fs::u8path(flag), ec);
      recent = !ec && age < std::chrono::minutes(20);
    }
    if (!recent) {
      if (FILE *f = std::fopen(flag.c_str(), "wb")) std::fclose(f);
      nj::launcher::host::restart_app();   // faux : le jeu continue ici
    }
  }
#endif
  // pendant la preparation (premier lancement), pas de commandes tactiles : rien a commander
  if (rt::g_p3_progress.phase.load() == 0 && !gfx::g_title_home)
    rt::input::touch_overlay_frame(draw_w, draw_h);
  gfx::fps_overlay_frame(L.hv.window);
  gfx::title_home_frame(L.hv.window);   // menu titre de Nightjar (18/11)
  gfx::menu_frame(L.hv.window);
  gfx::prep_frame(L.hv.window);
#ifdef __ANDROID__
  // 13/10 : la surface Android a un canal alpha ; ce que les melanges y laissent sous 1 rend la
  // fenetre transparente (le bureau apparaissait derriere l'ecran de preparation). Alpha force a 1.
  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  nj_wipe(GL_COLOR_BUFFER_BIT);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#endif
  const auto s0 = std::chrono::steady_clock::now();
  nj_before_swap();
  SDL_GL_SwapWindow(L.hv.window);
  g_fl_swap_us +=
      (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - s0).count();
  ++g_fl_swaps;
  if (g_native)
    nj::native::vsync((int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::steady_clock::now().time_since_epoch()).count());
}

void run_windowed(HostLoop &L) {
  while (!L.quit && L.ctx->alive) {
    if (g_native) {
      // Le noyau est tombe ou a fini : on le dit et on quitte, au lieu de rester sur un ecran noir.
      const auto st = nj::native::state();
      if (st == nj::native::State::Failed || st == nj::native::State::Ended) {
        NJ_ERR << "[Natif] le noyau s'est arrete (" << (st == nj::native::State::Failed ? "echec" : "fin")
               << ") : " << nj::native::message() << std::endl;
        break;
      }
    }
    pump_input(L);
    bind_efb(L);
    poll_framelog_key();
    capture_movie_xfb(L);
    const bool due = render_gx(L);
    report_stats(L);
    if (due)
      present_frame(L);
    else if (g_native && host_wait())   // coeur natif : dormir sur sa prise, reveil a la prochaine image (4 ms au plus : tactile)
      nj::native::wait_message(4);
    else   // avant le 14/11 : 250 us de sommeil, ~4 000 tours par seconde (NJ_HOSTLOOP=1 : 2 ms)
      nj::precise_sleep_us((g_native && host_loop_lean()) ? 2000 : 250);
    feed_audio(L);
    nudge_guest(*L.ctx);
  }
}

// Sans fenetre (mesures) : entrees et flux GX toutes les 16 ms.
void run_headless(rt::GuestCpu &ctx) {
  while (ctx.alive) {
    std::this_thread::sleep_for(std::chrono::milliseconds{16});
    rt::input::InputHub::get().update();
    gx_pump_fifo();
    nudge_guest(ctx);
  }
}

}  // namespace

extern "C" const volatile unsigned char nj_build_tag[16];

int main(int argc, char **argv) {
#if defined(__APPLE__) && !(defined(NJ_RELEASE) && !defined(NJ_DEVTOOLS))
  // iPhone, build de dev : dev_env.txt (une ligne NOM=valeur par interrupteur, dossier de l'appli) est pose dans l'environnement
  // avant le premier NJ_ENV, comme la coque Android. Permet de changer d'option sans refaire l'IPA (fichier ecrit par USB).
  if (FILE *fe = std::fopen("dev_env.txt", "r")) {
    char line[256];
    while (std::fgets(line, sizeof line, fe)) {
      char *eq = std::strchr(line, '=');
      if (!eq || line[0] == '#') continue;
      *eq = 0;
      char *val = eq + 1;
      val[std::strcspn(val, "\r\n")] = 0;
      setenv(line, val, 1);
    }
    std::fclose(fe);
  }
#endif
#ifdef __ANDROID__
  // Android : SDL garde sa fenetre d'un lancement a l'autre dans le meme processus ; relance
  // apres un retour de main, SDL_CreateWindow refusait (« Android only supports one window ») et
  // l'activite bouclait. Detruit en dernier : le processus finit avec main.
  struct EndProcess {
    ~EndProcess() { _exit(0); }
  } end_process;
#endif
  nj::crash::install();
  // retour normal de main : exit() n'est plus « anormal » pour le rapport de plantage (12/10)
  struct QuitOk {
    ~QuitOk() { nj::crash::quit_requested(); }
  } quit_ok;
#ifdef NJ_OS_WINDOWS
  // Une seule instance (12/10) : pendant l'installation, la fenetre semblait figee, le joueur
  // relancait, et trois installations concurrentes se partageaient le cache et les coeurs.
  // NJ_MULTI=1 (developpement) : plusieurs instances permises.
  if (!NJ_ENV("NJ_MULTI")) {
    static HANDLE single = CreateMutexW(nullptr, TRUE, L"Local\\NightjarSingleInstance");
    DWORD single_err = GetLastError();
    // relance apres la preparation (host::restart_app) : le processus precedent finit de sortir
    if (wcsstr(GetCommandLineW(), L"--restarted"))
      for (int i = 0; i < 40 && single && single_err == ERROR_ALREADY_EXISTS; ++i) {
        CloseHandle(single);
        Sleep(250);
        single = CreateMutexW(nullptr, TRUE, L"Local\\NightjarSingleInstance");
        single_err = GetLastError();
      }
    if (single && single_err == ERROR_ALREADY_EXISTS) {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Nightjar", "Nightjar is already running.", nullptr);
      return 0;
    }
  }
#endif
  (void)nj_build_tag[0];   // filigrane garde dans l'executable (core/build_tag.cpp)
  rt::hw::dump_threads_hook = &dump_guest_threads;
  rt::njprof::register_thread("hote");
  construct_singletons();
  rt::njprof::start();
  {
    const char *clock = nj::host_clock_calibrate();
    NJ_LOG << "[HostClock] " << clock << " (" << nj::g_tsc_clock.us_per_tick << " us par pas)" << std::endl;
  }
  silence_release_build();
#if defined(__APPLE__)
  // iPhone : stdout/stderr ne vont nulle part de lisible. Le journal du moteur est ecrit dans
  // <dossier de donnees>/journal.txt (visible dans l'application Fichiers), ligne par ligne.
  {
    const std::string log_path = nj::launcher::host::data_dir() + "journal.txt";
    if (std::freopen(log_path.c_str(), "w", stdout)) {
      // stderr sur le MEME fichier ouvert (meme position d'ecriture) : deux ouvertures separees
      // s'ecrasaient mutuellement et des lignes disparaissaient.
      dup2(fileno(stdout), fileno(stderr));
      std::setvbuf(stdout, nullptr, _IOLBF, 0);
      std::setvbuf(stderr, nullptr, _IONBF, 0);
      std::cout << "[iOS] journal : " << log_path << std::endl;
    }
  }
#endif

#if defined(__APPLE__)
  // iOS (port natif, 18/11) : le dossier de l'appli est celui des Documents (visible dans Fichiers)
  if (!NJ_ENV("NJ_FILES_DIR")) {
    std::string d = nj::launcher::host::data_dir();
    while (!d.empty() && d.back() == '/') d.pop_back();
    setenv("NJ_FILES_DIR", d.c_str(), 1);
  }
#endif

  // Dossier des donnees du jeu en mode natif pour tester si le disque est pret
  const char *early_data = NJ_ENV("NJ_NATIVE_DATA");
  const char *early_files = NJ_ENV("NJ_FILES_DIR");
#ifdef NJ_NATIVE_ONLY
  const char *default_leaf = "/game";
#else
  const char *default_leaf = "/native_data";
#endif
  const std::string data_dir = early_data ? early_data :
      (early_files ? std::string(early_files) + default_leaf :
       nj::launcher::host::data_dir() + (default_leaf + 1));

  const bool native_mode = native_requested();
  const bool native_disc_ok = native_mode && nj::native::disc_ready(data_dir, 0);

  // Avec le lanceur, le disque vient de l'ecran « insert your disc », pas de la ligne de commande.
  // En mode natif, le lanceur s'active si aucun disque n'est encore extrait sur l'appareil.
  const bool use_launcher = nj::launcher::enabled() && (!native_mode || !native_disc_ok);
  if (argc < 2 && !use_launcher && !native_mode) {
    NJ_ERR << "Usage: " << argv[0] << " <dossier du disque extrait> [config.toml]\n";
    return 1;
  }
  std::string game_dir = argc >= 2 ? argv[1] : "";
  const char *config_path = argc >= 3 ? argv[2] : "config.toml";
  rt::Config &cfg = rt::Config::get();
  cfg.load(config_path);
  rt::hw::attach_hardware(rt::IoBus::get());
  cfg.game_dir = game_dir;
  rt::input::install_input_sources();

  const char *headless_env = NJ_ENV("NJ_HEADLESS");
  const bool headless = headless_env && headless_env[0] == '1';
#ifdef __ANDROID__
  // Sans ecran (mesures, wakelock partiel) : la boucle continue quand l'activite passe en
  // arriere-plan, sinon le rendu ne vide plus la file et le jeu attend.
  if (headless)
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");
#endif
  gfx::set_config_path(config_path);
  apply_display_overrides();

  HostLoop loop;
  nj::gpu::apply_at_startup();   // avant le contexte GL : la carte se choisit a sa creation
  if (!headless && !rt::host_video_open(loop.hv))
    return 1;

  if (use_launcher) {
    // Version a jour, accueil, disque verifie : sans les trois, le jeu ne demarre pas.
    std::string disc1, disc2;
    if (headless || !loop.hv.window || !nj::launcher::run(loop.hv.window, game_dir, disc1, disc2))
      return 0;
    cfg.game_dir = game_dir;
    cfg.disc_image = disc1;
    cfg.disc2_image = disc2;
  }
  // Essais : disques imposes (avant le premier DiscView::init, qui monte les deux).
  if (const char *d2 = NJ_ENV("NJ_DISC2"))
    cfg.disc2_image = d2;
  if (const char *d1 = NJ_ENV("NJ_DISC_IMAGE"))
    cfg.disc_image = d1;

  auto ctx = std::make_unique<rt::GuestCpu>();
  rt::g_guest_mem = &ctx->mmu;
  rt::g_guest = ctx.get();   // le rendu lit les textures par *g_guest (renseigne ici car le fil du jeu recompile n'existe pas en mode natif)
  nj::boot::Program exe;
  std::thread cpu;
  g_native = native_requested();
  if (g_native) {
    // Moteur natif : le noyau charge depuis NJ_NATIVE_LIBS (defaut : dossier des bibliotheques), le
    // disque extrait dans NJ_NATIVE_DATA (dossier contenant disc0/sys et disc0/files).
    const char *libs = NJ_ENV("NJ_NATIVE_LIBS");
    const char *data = NJ_ENV("NJ_NATIVE_DATA");
    // Reglages du noyau (RE4_*=valeur;...) : son processus herite de l'environnement de celui-ci.
    const char *st = NJ_ENV("NJ_NATIVE_SETTINGS");
    // 15/11 (parcours du joueur) : plus de salle de test imposee en dur ; les raccourcis de test passent par dev_env.txt
    // (NJ_NATIVE_SETTINGS). Sans eux : logos, titre, nouvelle partie, cinematique d'ouverture, comme le jeu.
    if (st) {
      std::string all = st;
      for (size_t pos = 0; pos < all.size();) {
        size_t end = all.find(';', pos);
        if (end == std::string::npos) end = all.size();
        const std::string kv = all.substr(pos, end - pos);
        const size_t eq = kv.find('=');
        if (eq != std::string::npos && eq > 0) {
#ifdef _WIN32
          _putenv_s(kv.substr(0, eq).c_str(), kv.substr(eq + 1).c_str());
#else
          setenv(kv.substr(0, eq).c_str(), kv.substr(eq + 1).c_str(), 1);
#endif
        }
        pos = end + 1;
      }
    }
    std::string err;
#if defined(__APPLE__)
    // iOS (port natif, 18/11) : le dossier de l'appli est celui des Documents (visible dans Fichiers)
    if (!NJ_ENV("NJ_FILES_DIR")) {
      std::string d = nj::launcher::host::data_dir();
      while (!d.empty() && d.back() == '/') d.pop_back();
      setenv("NJ_FILES_DIR", d.c_str(), 1);
    }
#endif
    const char *files_dir = NJ_ENV("NJ_FILES_DIR");
    #ifdef NJ_NATIVE_ONLY
    const char *default_leaf = "/game";
#else
    const char *default_leaf = "/native_data";
#endif
    std::string data_dir = data ? data : (files_dir ? std::string(files_dir) + default_leaf : std::string(default_leaf + 1));
    // Premiere fois : les fichiers du disque du joueur sont extraits une seule fois, dans disc1/ (et disc2/) :
    // c'est la que le coeur natif les lit (native_core.cpp, data_dir + "/disc1").
    auto extraire = [&](const std::string &src, int n) -> bool {
      NJ_LOG << "[Natif] extraction du disque " << n << " : " << src << std::endl;
      int last = -10;
      if (!nj::native::extract_disc(src, data_dir, n, [&](int p) {
            if (p >= last + 10) { last = p; NJ_LOG << "[Natif] extraction " << p << " %" << std::endl; }
          }, err)) {
        NJ_ERR << "[Natif] " << err << std::endl;
        return false;
      }
      return true;
    };
#if defined(__APPLE__)
    // iOS (19/11) : chaque image des Documents est reconnue par son en-tete (octet 6 : 0 = disque 1, 1 = disque 2),
    // pas par l'ordre du dossier (le 1er essai avait pris le disque 2). Le disque 2 est extrait aussi s'il est la.
    std::string img1, img2;
    if (cfg.disc_image.empty() && game_dir.empty()) {
      const std::string docs = nj::launcher::host::data_dir();
      if (DIR *dd = opendir(docs.c_str())) {
        while (dirent *e = readdir(dd)) {
          std::string n = e->d_name, l = n;
          for (char &c : l) c = (char)std::tolower((unsigned char)c);
          if (!(l.size() > 4 && (l.compare(l.size() - 4, 4, ".iso") == 0 || l.compare(l.size() - 4, 4, ".rvz") == 0 ||
                                 l.compare(l.size() - 4, 4, ".gcm") == 0)))
            continue;
          const std::string path = docs + (docs.empty() || docs.back() == '/' ? "" : "/") + n;
          uint8_t h[8] = {};
          auto im = nj::runtime::DiscImage::open(path);
          if (!im || !im->read(0, h, sizeof h) || std::memcmp(h, "G4BE", 4) != 0) {
            NJ_ERR << "[Natif] disque ignore (pas Resident Evil 4 GameCube USA) : " << n << std::endl;
            continue;
          }
          (h[6] == 1 ? img2 : img1) = path;
        }
        closedir(dd);
      }
      // reste d'une extraction inachevee vers l'ancien dossier disc0/ (1er essai) : place rendue
      std::error_code ec;
      const std::filesystem::path d0 = std::filesystem::u8path(data_dir) / "disc0";
      if (std::filesystem::is_directory(d0, ec) && !std::filesystem::exists(d0 / "ready.properties", ec)) {
        std::filesystem::remove_all(d0, ec);
        NJ_LOG << "[Natif] dossier disc0 inacheve supprime" << std::endl;
      }
    }
#endif
    if (!nj::native::disc_ready(data_dir, 0)) {
      std::string src = cfg.disc_image.empty() ? game_dir : cfg.disc_image;
#if defined(__APPLE__)
      if (src.empty()) {
        if (img1.empty()) {
          NJ_ERR << (img2.empty() ? "[Natif] aucun disque : copie ton disque (.iso, .rvz ou .gcm) dans les Documents de l'appli"
                                  : "[Natif] disque 1 manquant : seul le disque 2 est dans les Documents") << std::endl;
          return 1;
        }
        src = img1;
      }
#endif
      if (!extraire(src, 1)) return 1;
    }
    const std::string d2_src = !cfg.disc2_image.empty() ? cfg.disc2_image :
#if defined(__APPLE__)
        img2;
#else
        "";
#endif
    if (!d2_src.empty() &&
        !std::filesystem::exists(std::filesystem::u8path(data_dir) / "disc2" / "ready.properties")) {
      if (!extraire(d2_src, 2)) NJ_ERR << "[Natif] disque 2 non extrait : la 2e partie du jeu le redemandera" << std::endl;
    }
    // Resolution interne du coeur natif (15/11) : il rend lui-meme et n'applique pas wanted_scale ; on lui donne
    // l'echelle voulue pour cet ecran (reglage « Internal resolution », « native » compris). Lue par port/njgfx.
    if (!headless && loop.hv.window) {
      int dw = 0, dh = 0;
      SDL_GL_GetDrawableSize(loop.hv.window, &dw, &dh);
      const int s = gfx::wanted_scale(dw, dh);
#ifdef _WIN32
      _putenv_s("NJ_SCALE", std::to_string(s).c_str());
#else
      setenv("NJ_SCALE", std::to_string(s).c_str(), 1);
#endif
      NJ_LOG << "[Natif] echelle interne " << s << " (ecran " << dw << "x" << dh << ")" << std::endl;
    }
    if (!nj::native::load(libs ? libs : "", err) || !nj::native::start(data_dir, libs ? libs : "", err)) {
      NJ_ERR << "[Natif] " << err << std::endl;
      return 1;
    }
    NJ_LOG << "[Natif] noyau demarre" << std::endl;
  } else {
  {
    std::string game_id;
    if (!nj::boot::load_program(cfg.disc_image.empty() ? game_dir : cfg.disc_image, exe, game_id))
      return 1;
    if (!game_id.empty())
      cfg.game_id = game_id;
    // Version du jeu (debug, USA, Europe) : les adresses que le moteur cite en dependent.
    const auto v = rt::detect_game_version(exe.file.data(), exe.file.size());
    rt::set_game_version(v);
    NJ_LOG << "[Version] " << rt::game_version_name(v) << "\n";
  }
  rt::boot_console(*ctx, exe, game_dir);
  rt::GX_EnableStaging(NJ_ENV("NJ_NOSTAGE") == nullptr);

  cpu = std::thread(cpu_thread_main, ctx.get());
  GX_StartParseThread();   // lecture du FIFO sur son propre fil (gx.cpp)
  }
#ifdef _WIN32
  if (const char *prof = NJ_ENV("NJ_HOSTPROF")) {
    static const std::string main_prof = std::string(prof) + ".main";
    HANDLE self = nullptr;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &self, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    std::thread(host_profiler_loop, ctx.get(), self, main_prof.c_str()).detach();
  }
#endif

  loop.ctx = ctx.get();
  if (headless) {
    run_headless(*ctx);
  } else {
    run_windowed(loop);
    rt::host_video_close(loop.hv);
  }

  ctx->alive = false;
  ctx->pc = 0;
#if defined(_WIN32) && defined(NJ_RELEASE)
  // Fermeture (12/10) : la fenetre partait mais le processus restait en arriere-plan (un fil
  // bloque, ou le demontage du compilateur embarque a la sortie), et la relance suivante restait
  // figee au chargement. La carte memoire et les reglages sont deja ecrits a chaque changement :
  // on laisse 2 s au fil du jeu, on vide le journal, puis on quitte sans rien demonter, comme
  // Android.
  for (int i = 0; i < 200 && !g_cpu_done.load(); ++i)
    SDL_Delay(10);
  nj::perf::flush();
  nj::crash::quit_requested();
  std::fflush(nullptr);
  TerminateProcess(GetCurrentProcess(), 0);
#endif
  if (cpu.joinable())
    cpu.join();
  // NJ_THREADS=1 : etat des fils de l'OS invite une fois le jeu arrete (instantane coherent).
  if (const char *env = NJ_ENV("NJ_THREADS"); env && env[0] == '1' && env[1] == 0)
    dump_guest_threads(ctx->mmu);
  rt::interp_report(std::cout);
  return 0;
}
