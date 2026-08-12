// -*- coding: utf-8 -*-
// Rendu natif, cote moteur (phase N3 du plan natif, nj-work/docs/PLAN_NATIF_NIGHTJAR_2026-11-11.md).
//
// Compile avec les options et les en-tetes de Nightjar (C++20), lie avec libnjruntime.a : le lecteur de flux
// et le rendu de Nightjar tournent DANS le coeur, sur la memoire du jeu (arene a 0x80000000 : une adresse
// GameCube y est un pointeur). Le portage (port/gx_sdk.cpp) lui passe le flux du SDK ; l'image finie part
// a l'appli par njlink (images partagees).
#include "njgfx_bridge.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#include "common/platform.h"
#include "runtime/config.h"
#include "runtime/cpu_context.h"
#include "runtime/gfx_settings.h"
#include "runtime/gx/fifo_parser.h"
#include "runtime/gx/frame_renderer.h"
#include "runtime/hw/hw.h"

namespace nj::runtime {
uint32_t g_debug_pc = 0;
MMU *g_guest_mem = nullptr;
GuestCpu *g_guest = nullptr;
}  // namespace nj::runtime

extern "C" void njlink_gl_ready(int w, int h);
extern "C" void njlink_frame_fbo(unsigned fbo, int x, int y, int w, int h);

extern std::atomic<bool> g_gx_framelog_request;   // renderer_gl.cpp (F12 dans l'appli)
extern "C" int port_host_scope;   // port/arm_hooks.cpp : allocations C++ hors du tas du jeu

namespace {
struct HostScope {   // tout ce que fait le rendu alloue avec malloc, pas dans le tas du jeu
  HostScope() { ++port_host_scope; }
  ~HostScope() { --port_host_scope; }
};
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

// Lit les commandes completes et decode tout de suite leurs sommets (le jeu peut reecrire sa memoire
// des le retour).
void take(const unsigned char *bytes, unsigned size) {
  if (!init()) return;
  g_stream.insert(g_stream.end(), bytes, bytes + size);
  gx::ParseFifo(g_stream, g_ops);
  for (; g_decoded < g_ops.size(); ++g_decoded)
    if (g_ops[g_decoded].type == gx::GxOpKind::DrawPrimitive) gx::DecodeDraw(g_ops[g_decoded]);
}

}  // namespace

extern "C" void njgfx_sync(const unsigned char *bytes, unsigned size) {
  HostScope hs;
  take(bytes, size);
}

extern "C" void njgfx_frame(const unsigned char *bytes, unsigned size) {
  HostScope hs;
  take(bytes, size);
  if (!g_ready) return;
  glBindFramebuffer(GL_FRAMEBUFFER, g_efb_fbo);
  glViewport(0, 0, gfx::efb_w(), gfx::efb_h());
  const size_t nops = g_ops.size();
  size_t ndraws = 0, nbp = 0, nxf = 0;
  for (const auto &c : g_ops) {
    ndraws += c.type == gx::GxOpKind::DrawPrimitive;
    nbp += c.type == gx::GxOpKind::BPRegister;
    nxf += c.type == gx::GxOpKind::XFRegister;
  }
  if (g_frames + 1 == 2100) g_gx_framelog_request = true;   // diagnostic N3 : journal complet d une image en jeu (r100)
  g_renderer->draw_frame(g_ops);
  g_ops.clear();
  g_decoded = 0;
  gx::DecodeEpochAdvance();
  const int s = gfx::g_scale;
  ++g_frames;
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
  njlink_frame_fbo(g_efb_fbo, 0, gfx::efb_h() - 480 * s, 640 * s, 480 * s);
}
