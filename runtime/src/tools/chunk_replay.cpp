// Moteur natif : rejoue des images enregistrees du noyau (RE4GXCH1) dans notre rendu et ecrit l'EFB
// en PPM, sans telephone ni noyau.
//
//   ChunkReplay <prefixe_sortie> <image.bin>...
//
// Chaque image enregistree repart de zero (etat complet, textures comprises) : chacune est donc
// rejouee dans un rendu et une memoire invitee neufs.
#include <SDL.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "common/gl.h"
#include "runtime/config.h"
#include "runtime/cpu_context.h"
#include "runtime/env.h"
#include "runtime/gfx_settings.h"
#include "runtime/gx/chunk_adapter.h"
#include "runtime/gx/frame_renderer.h"
#include "runtime/gx_state.h"
#include "runtime/hw/hw.h"

namespace nj::runtime {
uint32_t g_debug_pc = 0;
MMU *g_guest_mem = nullptr;
GuestCpu *g_guest = nullptr;
}  // namespace nj::runtime

extern void gx_bind_window(void *);

#ifdef _WIN32
#include <windows.h>
// lus par core/gpu_select.cpp (choix de la carte graphique), definis d'habitude par main.cpp
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace {

void write_ppm(const std::string &path, int w, int h, const std::vector<uint8_t> &rgba) {
  FILE *f = std::fopen(path.c_str(), "wb");
  if (!f) return;
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int y = h - 1; y >= 0; --y)   // l'EFB GL a son origine en bas
    for (int x = 0; x < w; ++x) std::fwrite(&rgba[((size_t)y * w + x) * 4], 1, 3, f);
  std::fclose(f);
}

}  // namespace

int main(int argc, char **argv) {
  if (!std::getenv("NJ_NOBUDGET")) putenv(const_cast<char *>("NJ_NOBUDGET=1"));
  if (argc < 3) {
    std::cerr << "usage: ChunkReplay <prefixe_sortie> <image.bin>...\n";
    return 2;
  }
  if (SDL_Init(SDL_INIT_VIDEO) < 0) return 2;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_Window *win = SDL_CreateWindow("ChunkReplay", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480,
                                     SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  SDL_GLContext gl = win ? SDL_GL_CreateContext(win) : nullptr;
  if (!gl || !gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
    std::cerr << "fenetre GL : " << SDL_GetError() << "\n";
    return 2;
  }
  std::cout << "[GL] " << (const char *)glGetString(GL_RENDERER) << std::endl;

  nj::runtime::Config::get().load("config.toml");
  nj::runtime::hw::attach_hardware(nj::runtime::IoBus::get());
  static nj::runtime::GuestCpu ctx;
  nj::runtime::g_guest = &ctx;
  nj::runtime::g_guest_mem = &ctx.mmu;
  nj::runtime::gfx::g_scale = 1;
  gx_bind_window(win);

  // EFB 640 x 480
  GLuint fbo = 0, color = 0, depth = 0;
  glGenTextures(1, &color);
  glBindTexture(GL_TEXTURE_2D, color);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glGenRenderbuffers(1, &depth);
  glBindRenderbuffer(GL_RENDERBUFFER, depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 640, 480);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);

  auto renderer = nj::runtime::gx::make_gl_renderer();
  renderer->attach(win);

  const std::string prefix = argv[1];
  // Comme en direct : le traducteur et le rendu gardent leur etat d'une image a la suivante ; une image
  // marquee RESET repart de zero. Une suite de fichiers consecutifs rejoue donc le flux du noyau.
  nj::runtime::gx::chunk::Adapter adapter;
  for (int a = 2; a < argc; ++a) {
    nj::runtime::gx::chunk::Image img;
    if (!nj::runtime::gx::chunk::read_file(argv[a], img)) {
      std::cerr << "illisible : " << argv[a] << "\n";
      continue;
    }
    if (img.flags & 1u) adapter = nj::runtime::gx::chunk::Adapter();
    std::vector<nj::runtime::gx::GxOp> frame;
    const auto t_tr0 = std::chrono::steady_clock::now();
    adapter.translate(nj::runtime::gx::chunk::view_of(img), frame);
    const double ms_translate = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t_tr0).count();
    size_t draws = 0, verts = 0, bps = 0, xfs = 0;
    for (const auto &c : frame) {
      if (c.type == nj::runtime::gx::GxOpKind::DrawPrimitive) { ++draws; verts += c.vertices.size(); }
      else if (c.type == nj::runtime::gx::GxOpKind::BPRegister) ++bps;
      else if (c.type == nj::runtime::gx::GxOpKind::XFRegister) ++xfs;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 640, 480);
    glClearColor(0, 0, 0, 1);
    glClearDepth(1.0);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const auto t_dr0 = std::chrono::steady_clock::now();
    renderer->draw_frame(frame);
    const double ms_cpu = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t_dr0).count();
    glFinish();
    const double ms_gpu = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t_dr0).count();
    std::printf("[temps] image %d : traduction %.2f ms, rendu CPU %.2f ms, rendu total (GPU compris) %.2f ms\n", a - 1,
                ms_translate, ms_cpu, ms_gpu);
    std::printf("[cache] dessins repris %llu, traduits %llu\n", (unsigned long long)adapter.cache_hits, (unsigned long long)adapter.cache_misses);
    std::vector<uint8_t> px((size_t)640 * 480 * 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, 640, 480, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    size_t black = 0;
    for (size_t i = 0; i < px.size(); i += 4)
      if (px[i] + px[i + 1] + px[i + 2] < 12) ++black;
    std::cout << "[" << argv[a] << "] ops=" << frame.size() << " bp=" << bps << " xf=" << xfs << " dessins=" << draws
              << " sommets=" << verts << " noir=" << black << "/" << 640 * 480 << " glerr=0x" << std::hex
              << glGetError() << std::dec << std::endl;
    char name[48];
    std::snprintf(name, sizeof name, "_%02d.ppm", a - 1);
    write_ppm(prefix + name, 640, 480, px);
  }
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(win);
  return 0;
}
