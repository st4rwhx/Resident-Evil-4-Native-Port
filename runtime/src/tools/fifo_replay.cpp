// Rejoue un log FIFO de Dolphin (.dff) dans le pipeline GX de nWiiRuntime.
//
// But : un oracle de rendu. Dolphin enregistre le flux GX **exact** d'une
// scene ; son rasteriseur logiciel en donne une image de reference
// deterministe. Ce harnais fait passer le meme flux, octet pour octet, par
// ParseFifo + renderer_gl, et ecrit l'EFB en PPM. La comparaison des deux
// images donne un ecart chiffre, par image puis par commande, la ou on jugeait
// a l'oeil.
//
//   FifoReplay <log.dff> <prefixe_sortie> [1re_image] [nb]
//
// Le format .dff est celui de Source/Core/Core/FifoPlayer/FifoDataFile.cpp
// (version 6, petit-boutiste, #pragma pack(1)). L'etat initial est reinjecte
// comme le fait FifoPlayer::LoadRegisters : commandes CP 0x08, BP 0x61 et XF
// 0x10 fabriquees, avec les memes registres ecartes (ShouldLoadBP /
// ShouldLoadXF) - les ecrire declencherait une copie EFB ou un jeton PE au
// mauvais moment.

#include "runtime/env.h"
#include "runtime/config.h"
#include "runtime/cpu_context.h"
#include "runtime/hw/hw.h"
#include "runtime/gx_state.h"
#include "runtime/gfx_settings.h"
#include "runtime/gx/fifo_record.h"
#include "runtime/gx/fifo_parser.h"

#include <utility>
#include <SDL.h>
#include "common/gl.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <atomic>

// Definis ici parce que main.cpp, qui les porte d'habitude, n'est pas lie.
namespace nj::runtime {
uint32_t g_debug_pc = 0;
MMU *g_guest_mem = nullptr;
GuestCpu *g_guest = nullptr;
}

extern void gx_bind_window(void *);
extern void gx_pump_fifo();
extern bool GXInterpTestFinal();
#include <memory>
extern void GXReplayVolatile(uint32_t phys, std::shared_ptr<const std::vector<uint8_t>> bytes);
extern void gx_xfb_info(uint32_t *, unsigned *, unsigned *, unsigned *);
extern std::atomic<bool> g_gx_framelog_request;

namespace {

#pragma pack(push, 1)
struct DffHeader {
  uint32_t fileId, file_version, min_loader_version;
  uint64_t bpMemOffset;   uint32_t bpMemSize;
  uint64_t cpMemOffset;   uint32_t cpMemSize;
  uint64_t xfMemOffset;   uint32_t xfMemSize;
  uint64_t xfRegsOffset;  uint32_t xfRegsSize;
  uint64_t frameListOffset; uint32_t frameCount, flags;
  uint64_t texMemOffset;  uint32_t texMemSize;
  uint32_t ram_size_real, exram_size_real;
  char gameId[8];
  uint8_t reserved[24];
};
struct DffFrameInfo {
  uint64_t fifoDataOffset;
  uint32_t fifoDataSize, fifoStart, fifoEnd;
  uint64_t memoryUpdatesOffset;
  uint32_t numMemoryUpdates;
  uint8_t reserved[32];
};
struct DffMemUpdate {
  uint32_t fifoPosition, address;
  uint64_t dataOffset;
  uint32_t dataSize;
  uint8_t type;
  uint8_t reserved[3];
};
#pragma pack(pop)

static_assert(sizeof(DffHeader) == 128, "en-tete .dff");
static_assert(sizeof(DffFrameInfo) == 64, "table d images .dff");
static_assert(sizeof(DffMemUpdate) == 24, "mise a jour memoire .dff");

std::vector<uint8_t> g_dff;

const DffHeader &hdr() { return *reinterpret_cast<const DffHeader *>(g_dff.data()); }

// --- fabrication des commandes GX ------------------------------------------
std::vector<uint8_t> g_pre;
void put8(uint8_t b) { g_pre.push_back(b); }
void put32(uint32_t v) {  // le FIFO est gros-boutiste
  g_pre.push_back((uint8_t)(v >> 24));
  g_pre.push_back((uint8_t)(v >> 16));
  g_pre.push_back((uint8_t)(v >> 8));
  g_pre.push_back((uint8_t)v);
}
void loadCP(uint8_t reg, uint32_t val) { put8(0x08); put8(reg); put32(val); }
void loadBP(uint8_t reg, uint32_t val) {
  put8(0x61);
  put32(((uint32_t)reg << 24) | (val & 0x00FFFFFFu));
}
void loadXFReg(uint16_t reg, uint32_t val) {
  put8(0x10); put32((reg & 0x0FFFu) | 0x1000u); put32(val);
}
void loadXFMem16(uint16_t addr, const uint32_t *d) {
  put8(0x10); put32(0x000F0000u | (addr & 0xFFFFu));
  for (int i = 0; i < 16; ++i) put32(d[i]);
}

// ShouldLoadBP de FifoPlayer.cpp : ces registres declenchent un effet de bord
// (fin de dessin, jeton PE, copie EFB, chargement de TLUT, compteur) et ne
// doivent pas etre rejoues comme un simple etat.
bool shouldLoadBP(uint8_t r) {
  switch (r) {
  case 0x45:  // BPMEM_SETDRAWDONE
  case 0x47:  // BPMEM_PE_TOKEN_ID
  case 0x48:  // BPMEM_PE_TOKEN_INT_ID
  case 0x52:  // BPMEM_TRIGGER_EFB_COPY
  case 0x63:  // BPMEM_PRELOAD_MODE
  case 0x65:  // BPMEM_LOADTLUT1
  case 0x67:  // BPMEM_PERF1
    return false;
  default:
    return true;
  }
}
// ShouldLoadXF : 0x1007 et les trois groupes inconnus.
bool shouldLoadXF(uint16_t reg) {
  const uint16_t a = (uint16_t)(reg + 0x1000);
  if (a == 0x1007) return false;
  if (a >= 0x1013 && a <= 0x1017) return false;
  if (a >= 0x1027 && a <= 0x103E) return false;
  if (a >= 0x1048 && a <= 0x104F) return false;
  return true;
}

void buildPreamble() {
  const DffHeader &h = hdr();
  const uint32_t *bp = reinterpret_cast<const uint32_t *>(g_dff.data() + h.bpMemOffset);
  const uint32_t *cp = reinterpret_cast<const uint32_t *>(g_dff.data() + h.cpMemOffset);
  const uint32_t *xf = reinterpret_cast<const uint32_t *>(g_dff.data() + h.xfMemOffset);
  const uint32_t *xr = reinterpret_cast<const uint32_t *>(g_dff.data() + h.xfRegsOffset);

  g_pre.clear();
  for (uint32_t i = 0; i < h.bpMemSize; ++i)
    if (shouldLoadBP((uint8_t)i)) loadBP((uint8_t)i, bp[i]);

  loadCP(0x30, cp[0x30]);  // MATINDEX_A
  loadCP(0x40, cp[0x40]);  // MATINDEX_B
  loadCP(0x50, cp[0x50]);  // VCD_LO
  loadCP(0x60, cp[0x60]);  // VCD_HI
  for (int i = 0; i < 8; ++i) {
    loadCP((uint8_t)(0x70 + i), cp[0x70 + i]);
    loadCP((uint8_t)(0x80 + i), cp[0x80 + i]);
    loadCP((uint8_t)(0x90 + i), cp[0x90 + i]);
  }
  for (int i = 0; i < 16; ++i) {
    loadCP((uint8_t)(0xA0 + i), cp[0xA0 + i]);
    loadCP((uint8_t)(0xB0 + i), cp[0xB0 + i]);
  }
  for (uint32_t i = 0; i < h.xfMemSize; i += 16) loadXFMem16((uint16_t)i, xf + i);
  for (uint32_t i = 0; i < h.xfRegsSize; ++i)
    if (shouldLoadXF((uint16_t)i)) loadXFReg((uint16_t)i, xr[i]);
}

void writePPM(const std::string &path, int w, int h, const std::vector<uint8_t> &rgba) {
  FILE *f = fopen(path.c_str(), "wb");
  if (!f) { std::cerr << "ecriture impossible: " << path << "\n"; return; }
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  // glReadPixels rend la premiere ligne en bas ; le PPM la veut en haut.
  std::vector<uint8_t> row((size_t)w * 3);
  for (int y = h - 1; y >= 0; --y) {
    const uint8_t *s = rgba.data() + (size_t)y * w * 4;
    for (int x = 0; x < w; ++x) {
      row[x * 3 + 0] = s[x * 4 + 0];
      row[x * 3 + 1] = s[x * 4 + 1];
      row[x * 3 + 2] = s[x * 4 + 2];
    }
    fwrite(row.data(), 1, row.size(), f);
  }
  fclose(f);
}

} // namespace

#ifdef _WIN32
#include <windows.h>
#include <atomic>
#include <map>
#include <thread>
// Definis par l'exe du jeu (core/main.cpp) et lus par gpu_select.cpp : le rejoueur donne les siens.
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
// NJ_REPLAY_PROF=<fichier> : echantillonne le fil principal (analyse + decodage + rendu du rejeu)
// toutes les ~1 ms ; a la fin, « decalage_dans_l_exe nombre » par ligne, pour nm. Mesure du fil de
// lecture sur PC sans telephone (30/09).
static std::atomic<bool> s_prof_stop{false};
static std::map<uint64_t, uint64_t> s_prof_hist;
static std::thread s_prof_thread;
static void replay_prof_start() {
  const char *out = std::getenv("NJ_REPLAY_PROF");
  if (!out) return;
  HANDLE th = nullptr;
  DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &th, 0, FALSE,
                  DUPLICATE_SAME_ACCESS);
  s_prof_thread = std::thread([th] {
    timeBeginPeriod(1);
    while (!s_prof_stop.load()) {
      if (SuspendThread(th) != (DWORD)-1) {
        CONTEXT c{};
        c.ContextFlags = CONTEXT_CONTROL;
        if (GetThreadContext(th, &c)) ++s_prof_hist[c.Rip];
        ResumeThread(th);
      }
      Sleep(1);
    }
  });
}
static void replay_prof_stop() {
  const char *out = std::getenv("NJ_REPLAY_PROF");
  if (!out || !s_prof_thread.joinable()) return;
  s_prof_stop = true;
  s_prof_thread.join();
  const uint64_t base = (uint64_t)GetModuleHandleA(nullptr);
  if (FILE *f = std::fopen(out, "w")) {
    for (const auto &kv : s_prof_hist)
      std::fprintf(f, "%llx %llu\n", (unsigned long long)(kv.first - base), (unsigned long long)kv.second);
    std::fclose(f);
  }
}
#else
static void replay_prof_start() {}
static void replay_prof_stop() {}
#endif

// Fenetre cachee et contexte OpenGL 3.2 core (RGBA 8 bits, profondeur 24, gabarit 8) : le
// rejoueur n'affiche rien, il ecrit des images. nullptr en cas d'echec (message ecrit).
static SDL_Window *open_hidden_gl(SDL_GLContext &ctx) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL : " << SDL_GetError() << "\n";
    return nullptr;
  }
  static const std::pair<SDL_GLattr, int> kAttrs[] = {
      {SDL_GL_CONTEXT_MAJOR_VERSION, 3}, {SDL_GL_CONTEXT_MINOR_VERSION, 2},
      {SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE},
      {SDL_GL_RED_SIZE, 8}, {SDL_GL_GREEN_SIZE, 8}, {SDL_GL_BLUE_SIZE, 8}, {SDL_GL_ALPHA_SIZE, 8},
      {SDL_GL_DEPTH_SIZE, 24}, {SDL_GL_STENCIL_SIZE, 8}};
  for (const auto &[attr, value] : kAttrs)
    SDL_GL_SetAttribute(attr, value);
  SDL_Window *w = SDL_CreateWindow("FifoReplay", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  ctx = w ? SDL_GL_CreateContext(w) : nullptr;
  if (!ctx) {
    std::cerr << "fenetre GL : " << SDL_GetError() << "\n";
    return nullptr;
  }
  if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
    std::cerr << "chargeur GL (glad) en echec\n";
    return nullptr;
  }
  return w;
}

// EFB du rejoueur : couleur (texture, relue par present_area) et profondeur, 640 x 480 x S.
struct Efb {
  GLuint fbo = 0, color = 0, depth = 0;
};
static Efb make_efb(int S) {
  Efb e;
  const GLsizei w = 640 * S, h = 480 * S;
  glGenTextures(1, &e.color);
  glBindTexture(GL_TEXTURE_2D, e.color);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  for (GLenum f : {GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER})
    glTexParameteri(GL_TEXTURE_2D, f, GL_LINEAR);
  glGenRenderbuffers(1, &e.depth);
  glBindRenderbuffer(GL_RENDERBUFFER, e.depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
  glGenFramebuffers(1, &e.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, e.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, e.color, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, e.depth);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "EFB incomplet\n";
    e.fbo = 0;
  }
  return e;
}

int main(int argc, char **argv) {
  // Un oracle doit etre reproductible. Le plafond de compilation de shaders du
  // renderer est exprime en millisecondes, donc le nombre de dessins sautes
  // change avec la charge de la machine : deux rejeux du meme log donnaient
  // deux images differentes (132 596 contre 242 405 pixels noirs). En rejeu on
  // compile tout, aussi longtemps qu il le faut.
  // SDL_setenv passe par SetEnvironmentVariable ; std::getenv, dont depend
  // NJ_ENV, lit la copie privee du CRT, que Win32 ne met pas a jour. Le
  // garde-fou ne prenait donc jamais. putenv ecrit du bon cote.
  if (!std::getenv("NJ_NOBUDGET"))
    putenv(const_cast<char *>("NJ_NOBUDGET=1"));
  if (argc < 3) {
    std::cerr << "usage: FifoReplay <log.dff> <prefixe_sortie> [1re_image] [nb]\n";
    return 2;
  }
  // NJ_REPLAY_INTERP=<fps> : active l'interpolation (cadence figee, budget toujours abordable).
  bool interp_test = false;
  if (const char *e = std::getenv("NJ_REPLAY_INTERP")) {
    const int f = std::atoi(e);
    if (f == 0 || f > 30) {
      putenv(const_cast<char *>("NJ_FI_TEST=1"));
      nj::runtime::gfx::g_fps_target = f;
      interp_test = true;
    }
  }
  const std::string dff_path = argv[1];
  const std::string out_prefix = argv[2];
  const uint32_t first = argc > 3 ? (uint32_t)atoi(argv[3]) : 0;
  const uint32_t want = argc > 4 ? (uint32_t)atoi(argv[4]) : 1;
  // NJ_REPLAY_RECORD=<n> : l'enregistreur du moteur (fifo_record.h) reenregistre n images pendant le rejeu ;
  // le fichier produit (fifo_*.dff, dossier d'ecriture) se rejoue pour verifier l'enregistreur.
  if (const char *e = std::getenv("NJ_REPLAY_RECORD")) nj::runtime::gx::rec::request(std::atoi(e));
  // NJ_NATIVE_ARRAYS=1 : enregistrement du coeur natif (port/njgfx) : tableaux de sommets et matrices dans
  // l'ordre de la machine, comme dans le coeur (fifo_parser g_native_arrays)
  if (std::getenv("NJ_NATIVE_ARRAYS")) nj::runtime::gx::g_native_arrays = true;

  {
    FILE *f = fopen(dff_path.c_str(), "rb");
    if (!f) { std::cerr << "ouverture impossible: " << dff_path << "\n"; return 2; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    g_dff.resize((size_t)n);
    if (fread(g_dff.data(), 1, (size_t)n, f) != (size_t)n) {
      std::cerr << "lecture courte\n";
      return 2;
    }
    fclose(f);
  }
  const DffHeader &h = hdr();
  if (h.fileId != 0x0D01F1F0u) { std::cerr << "pas un .dff\n"; return 2; }
  std::cout << "[dff] " << std::string(h.gameId, strnlen(h.gameId, 8))
            << " v" << h.file_version << " " << h.frameCount << " images"
            << " bp=" << h.bpMemSize << " cp=" << h.cpMemSize
            << " xf=" << h.xfMemSize << " xfregs=" << h.xfRegsSize << std::endl;

  // --- hote GL ---------------------------------------------------------
  SDL_GLContext ctx_gl = nullptr;
  SDL_Window *win = open_hidden_gl(ctx_gl);
  if (!win)
    return 2;
  std::cout << "[GL] " << (const char *)glGetString(GL_RENDERER) << std::endl;

  // NJ_IRES=<S> : EFB a S fois la resolution (comme le jeu), image reduite par moyenne a 640 x 480
  // avant ecriture, pour rester comparable a la reference Dolphin en 1x.
  int S = 1;
  if (const char *e = NJ_ENV("NJ_IRES")) S = std::atoi(e) > 0 ? std::atoi(e) : 1;
  // NJ_HDTEX=1 : textures HD du dossier textures_hd/ du repertoire courant (menu F10 : jeu,
  // cinematiques et interface ensemble).
  if (NJ_ENV("NJ_HDTEX"))
    nj::runtime::gfx::g_hd_textures = nj::runtime::gfx::g_hd_cutscenes = nj::runtime::gfx::g_hd_interface =
        nj::runtime::gfx::g_hd_environment = nj::runtime::gfx::g_hd_items = nj::runtime::gfx::g_hd_weapons =
            nj::runtime::gfx::g_hd_case = nj::runtime::gfx::g_hd_hud = 1;
  nj::runtime::gfx::g_scale = S;
  const Efb efb = make_efb(S);
  if (!efb.fbo)
    return 2;

  // --- etat invite ------------------------------------------------------
  nj::runtime::Config::get().load("config.toml");
  nj::runtime::hw::attach_hardware(nj::runtime::IoBus::get());
  // Le cache de textures prend son MMU dans le GuestCpu, et le renderer ne
  // lie ses textures que si g_guest est non nul. Le harnais ne posait qu un
  // MMU nu : aucune texture n etait liee de toute la rediffusion, et chaque
  // dessin texture echantillonnait l objet GL 0, incomplet, donc noir. C est
  // ce qui faisait le sol noir, pas une divergence de TEV.
  static nj::runtime::GuestCpu ctx;
  nj::runtime::g_guest = &ctx;
  nj::runtime::g_guest_mem = &ctx.mmu;
  nj::runtime::MMU &mmu = ctx.mmu;
  gx_bind_window(win);

  buildPreamble();
  std::cout << "[etat] preambule " << g_pre.size() << " octets" << std::endl;

  // Couleur et profondeur d'effacement telles que le jeu les avait posees au
  // moment de l'enregistrement. CLEAR_AR : alpha en 8-15, rouge en 0-7 ;
  // CLEAR_GB : vert en 8-15, bleu en 0-7 ; CLEAR_Z : 24 bits.
  const uint32_t *bpblob = reinterpret_cast<const uint32_t *>(g_dff.data() + h.bpMemOffset);
  const unsigned clear_a = (bpblob[0x4F] >> 8) & 0xFF, clear_r = bpblob[0x4F] & 0xFF;
  const unsigned clear_g = (bpblob[0x50] >> 8) & 0xFF, clear_b = bpblob[0x50] & 0xFF;
  const uint32_t clear_z = bpblob[0x51] & 0xFFFFFF;
  std::cout << "[etat] effacement rgba=" << clear_r << "," << clear_g << ","
            << clear_b << "," << clear_a << " z=0x" << std::hex << clear_z
            << std::dec << std::endl;

  const DffFrameInfo *frames =
      reinterpret_cast<const DffFrameInfo *>(g_dff.data() + h.frameListOffset);

  auto feed = [](const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; ++i) nj::runtime::GX_WGPIPE_Write8(p[i]);
  };

  // NJ_REPLAY_LOOPS=<n> : rejoue les images demandees n fois de suite (mesure du fil de lecture
  // en regime etabli, caches pleins : ligne [PARSE] par image, analyse + decodage).
  const uint32_t loops = [] {
    const char *e = NJ_ENV("NJ_REPLAY_LOOPS");
    return e && std::atoi(e) > 0 ? (uint32_t)std::atoi(e) : 1u;
  }();
  const uint32_t span = std::min<uint32_t>(want, h.frameCount > first ? h.frameCount - first : 0);
  extern uint64_t g_fl_parse_us;
  replay_prof_start();
  for (uint32_t it = 0; span && it < loops * span; ++it) {
    const uint32_t fi = first + it % span;
    const uint64_t parse0 = g_fl_parse_us;
    const auto t_img0 = std::chrono::steady_clock::now();
    const DffFrameInfo &f = frames[fi];
    const uint8_t *fifo = g_dff.data() + f.fifoDataOffset;
    const DffMemUpdate *ups =
        reinterpret_cast<const DffMemUpdate *>(g_dff.data() + f.memoryUpdatesOffset);

    glBindFramebuffer(GL_FRAMEBUFFER, efb.fbo);
    glViewport(0, 0, 640 * S, 480 * S);
    // Sur console l'EFB est efface par la copie vers le XFB de l'image
    // **precedente** (bit 0x800 de BPMEM_TRIGGER_EFB_COPY, couleur dans
    // CLEAR_AR/CLEAR_GB, profondeur dans CLEAR_Z). La premiere image d'un log
    // FIFO n'a pas de precedente : sans cet effacement, la profondeur reste
    // indefinie et le test en Z rejetait tout - l'EFB ressortait noir. On
    // refait donc ici ce que la copie manquante aurait fait.
    //   NJ_FIFOMAGENTA=1 met un fond temoin a la place, pour distinguer
    //   « rien dessine » de « dessine en noir ».
    if (NJ_ENV("NJ_FIFOMAGENTA"))
      glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    else
      glClearColor(clear_r / 255.0f, clear_g / 255.0f, clear_b / 255.0f,
                   clear_a / 255.0f);
    glClearDepth(clear_z / 16777215.0);
    nj_wipe(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    while (glGetError() != GL_NO_ERROR) {}
    if (NJ_ENV("NJ_FIFOLOG")) g_gx_framelog_request = true;

    if (it == 0) {
      feed(g_pre.data(), g_pre.size());
      gx_pump_fifo();
    }

    // Les mises a jour memoire doivent etre appliquees **a leur position dans
    // le flux** : 2116 des 2549 de l'image 0 se recouvrent (RE4 reecrit son
    // tampon de sommets peaufines a chaque modele). On vide donc le FIFO
    // jusqu'a cette position avant d'ecraser la RAM, pour que le decodage des
    // dessins deja emis lise encore les bons octets.
    uint32_t cursor = 0;
    size_t applied = 0, upd_bytes = 0;
    for (uint32_t k = 0; k < f.numMemoryUpdates; ++k) {
      const DffMemUpdate &u = ups[k];
      if (u.fifoPosition > cursor) {
        // decoupe aussi tous les 256 Kio : la contre-pression du FIFO fait
        // tourner l'hote a vide si on accumule trop sans vidange.
        while (u.fifoPosition - cursor > (256u << 10)) {
          feed(fifo + cursor, 256u << 10);
          cursor += 256u << 10;
          gx_pump_fifo();
        }
        feed(fifo + cursor, u.fifoPosition - cursor);
        cursor = u.fifoPosition;
      }
      gx_pump_fifo();
      // Copie volatile de l'enregistreur du moteur : type 0x80, ou (NJ_REPLAY_VOLATILE=1, anciens fichiers) toute mise a
      // jour a une position > 0. Elle passe par le mecanisme du moteur au lieu d'ecrire la memoire.
      static const bool vol_env = std::getenv("NJ_REPLAY_VOLATILE") != nullptr;
      if (u.type == 0x80 || (vol_env && u.fifoPosition > 0)) {
        auto bytes = std::make_shared<const std::vector<uint8_t>>(g_dff.data() + u.dataOffset,
                                                                  g_dff.data() + u.dataOffset + u.dataSize);
        GXReplayVolatile(u.address, std::move(bytes));
        ++applied;
        continue;
      }
      if (uint8_t *dst = mmu.get_ptr(0x80000000u | (u.address & 0x03FFFFFFu))) {
        memcpy(dst, g_dff.data() + u.dataOffset, u.dataSize);
        ++applied;
        upd_bytes += u.dataSize;
      }
    }
    while (f.fifoDataSize - cursor > (256u << 10)) {
      feed(fifo + cursor, 256u << 10);
      cursor += 256u << 10;
      gx_pump_fifo();
    }
    feed(fifo + cursor, f.fifoDataSize - cursor);
    gx_pump_fifo();
    { extern void FmtStatDump_ext(unsigned); FmtStatDump_ext(fi); }
    std::cout << "[PARSE] image " << fi << " tour " << it / span << " lecture_ms="
              << (g_fl_parse_us - parse0) / 1000.0 << " image_ms="
              << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t_img0).count()
              << std::endl;

    unsigned xw = 0, xh = 0, xs = 0;
    uint32_t xa = 0;
    gx_xfb_info(&xa, &xw, &xh, &xs);
    std::cout << "[image " << fi << "] fifo=" << f.fifoDataSize
              << " maj=" << applied << "/" << f.numMemoryUpdates
              << " (" << upd_bytes << " o)"
              << " xfb=" << xw << "x" << xh << " @0x" << std::hex << xa << std::dec
              << std::endl;

    // NJ_REPLAY_INTERP=<fps> : cadence demandee ; l'image qui vient d'etre rendue est alors l'image
    // intermediaire (t = 0,5), gardee en _NNN_mid.ppm ; B exacte est ensuite redessinee comme _NNN.ppm.
    if (interp_test) {
      glBindFramebuffer(GL_READ_FRAMEBUFFER, efb.fbo);
      std::vector<uint8_t> mid((size_t)640 * S * 480 * S * 4);
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      glReadPixels(0, 0, 640 * S, 480 * S, GL_RGBA, GL_UNSIGNED_BYTE, mid.data());
      const bool had_mid = GXInterpTestFinal();
      std::cout << "        interp: " << (had_mid ? "intermediaire + B exacte" : "refusee (B seule)") << std::endl;
      if (had_mid) {
        char name_mid[64];
        snprintf(name_mid, sizeof name_mid, "_%03u_mid.ppm", fi);
        writePPM(out_prefix + name_mid, 640 * S, 480 * S, mid);
      }
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, efb.fbo);
    std::vector<uint8_t> px((size_t)640 * 480 * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    if (S == 1) {
      glReadPixels(0, 0, 640, 480, GL_RGBA, GL_UNSIGNED_BYTE, &px[0]);
    } else {
      std::vector<uint8_t> big((size_t)640 * S * 480 * S * 4);
      glReadPixels(0, 0, 640 * S, 480 * S, GL_RGBA, GL_UNSIGNED_BYTE, big.data());
      for (int y = 0; y < 480; ++y)
        for (int x = 0; x < 640; ++x)
          for (int c = 0; c < 4; ++c) {
            unsigned acc = 0;
            for (int j = 0; j < S; ++j)
              for (int i = 0; i < S; ++i)
                acc += big[(((size_t)(y * S + j) * 640 * S) + (size_t)(x * S + i)) * 4 + c];
            px[((size_t)y * 640 + x) * 4 + c] = (uint8_t)(acc / (unsigned)(S * S));
          }
    }
    size_t magenta = 0, black = 0;
    for (size_t i = 0; i < px.size(); i += 4) {
      if (px[i] > 200 && px[i + 1] < 50 && px[i + 2] > 200) ++magenta;
      else if (px[i] + px[i + 1] + px[i + 2] < 12) ++black;
    }
    GLenum ge = glGetError();
    std::cout << "        efb: magenta=" << magenta << " noir=" << black
              << " autre=" << (640 * 480 - magenta - black)
              << " glerr=0x" << std::hex << ge << std::dec << std::endl;
    char name[64];
    snprintf(name, sizeof name, "_%03u.ppm", fi);
    writePPM(out_prefix + name, 640, 480, px);
    // Avec plusieurs tours (NJ_REPLAY_LOOPS), chaque tour garde son image (_NNN_tN.ppm) : meme donnees, donc
    // toute difference entre tours est un defaut de cache ou d'etat.
    if (loops > 1) {
      char nt[64];
      snprintf(nt, sizeof nt, "_%03u_t%u.ppm", fi, (unsigned)(it / span));
      writePPM(out_prefix + nt, 640, 480, px);
    }
    // NJ_REPLAYFULL : aussi l'EFB a pleine resolution interne (_NNN_full.ppm)
    if (S > 1 && NJ_ENV("NJ_REPLAYFULL")) {
      std::vector<uint8_t> big((size_t)640 * S * 480 * S * 4);
      glReadPixels(0, 0, 640 * S, 480 * S, GL_RGBA, GL_UNSIGNED_BYTE, big.data());
      snprintf(name, sizeof name, "_%03u_full.ppm", fi);
      writePPM(out_prefix + name, 640 * S, 480 * S, big);
    }
    // NJ_REPLAY_PRESENT=LxH : la zone XFB presentee dans L x H comme le jeu le fait, par le
    // bilineaire du GPU (_NNN_bil.ppm) et par la moyenne de surface du menu F10 (_NNN_area.ppm).
    if (const char *pe = NJ_ENV("NJ_REPLAY_PRESENT")) {
      int pw = 0, ph = 0;
      if (std::sscanf(pe, "%dx%d", &pw, &ph) == 2 && pw > 0 && ph > 0) {
        GLuint pfbo = 0, ptex = 0;
        ptex = nj_new_tex();
        nj_tex2d(ptex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pw, ph, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenFramebuffers(1, &pfbo);
        nj_fbo(GL_FRAMEBUFFER, pfbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ptex, 0);
        const int sw = (int)(xw ? xw : 640) * S, sh = (int)(xh ? xh : 480) * S;
        std::vector<uint8_t> out((size_t)pw * ph * 4);
        for (int mode = 0; mode < 2; ++mode) {
          nj_fbo(GL_DRAW_FRAMEBUFFER, pfbo);
          glClearColor(0.f, 0.f, 0.f, 1.f);
          nj_wipe(GL_COLOR_BUFFER_BIT);
          bool ok = true;
          if (mode == 0) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, efb.fbo);
            glBlitFramebuffer(0, 480 * S - sh, sw, 480 * S, 0, 0, pw, ph, GL_COLOR_BUFFER_BIT, GL_LINEAR);
          } else {
            ok = nj::runtime::gfx::present_area(efb.color, 640 * S, 480 * S, 0, 480 * S - sh, sw,
                                                  480 * S, 0, 0, pw, ph, pfbo);
          }
          nj_fbo(GL_READ_FRAMEBUFFER, pfbo);
          glReadPixels(0, 0, pw, ph, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
          snprintf(name, sizeof name, mode == 0 ? "_%03u_bil.ppm" : "_%03u_area.ppm", fi);
          if (ok) writePPM(out_prefix + name, pw, ph, out);
        }
        glDeleteFramebuffers(1, &pfbo);
        glDeleteTextures(1, &ptex);
      }
    }
  }

  std::cout << "[fin]" << std::endl;
  SDL_GL_DeleteContext(ctx_gl);
  SDL_DestroyWindow(win);
  SDL_Quit();
  replay_prof_stop();
  return 0;
}
