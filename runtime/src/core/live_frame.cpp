// Image en direct pour le pont de debug (tools/re4/live_debug.py), Windows et hors version publique.
//
// La capture de fenetre (PrintWindow) ramassait aussi les incrustations posees au moment de la
// presentation (MSI Afterburner / RTSS : GPU, CPU, i/s en haut a gauche) : inutilisable pour
// comparer a Dolphin. Ici on publie l'image du jeu elle-meme, prise dans l'EFB (le rectangle que
// le jeu presente, comme NJ_SCREENSHOT), avant toute incrustation, dans une memoire partagee
// « Local\nj_live_<pid> » que le pont lit.
//
// Rien ne se fait tant que personne ne regarde : le lecteur ecrit son horloge (GetTickCount) dans
// l'en-tete ; sans lecture depuis 2 s, la fonction rend la main tout de suite. Quand quelqu'un
// regarde : reduction par le GPU a 1024 de large au plus, lecture asynchrone (deux PBO : on lit
// celle de l'image precedente, pas d'attente du GPU), au plus 30 fois par seconde.
#if defined(_WIN32) && !defined(NJ_RELEASE) && !defined(NJ_MOBILE)
#include "common/gl.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace nj::live {

namespace {
struct Header {
  uint32_t magic;    // 'NWLF'
  uint32_t seq;      // incremente a chaque image publiee
  uint32_t w, h;     // taille de l'image (RGBA, ligne 0 = haut)
  uint32_t want_ms;  // ecrit par le lecteur : GetTickCount de sa derniere lecture
  uint32_t src_w, src_h;
  uint32_t pad[9];
};
constexpr int kMaxW = 1024, kMaxH = 768;
constexpr size_t kSize = sizeof(Header) + (size_t)kMaxW * kMaxH * 4;

Header *g_hdr = nullptr;
bool g_failed = false;
GLuint g_fbo = 0, g_tex = 0, g_pbo[2] = {0, 0};
int g_w[2] = {0, 0}, g_h[2] = {0, 0};
int g_idx = 0;
bool g_pending[2] = {false, false};
DWORD g_last = 0;

bool open_map() {
  if (g_hdr || g_failed)
    return g_hdr != nullptr;
  const std::string name = "Local\\nj_live_" + std::to_string(GetCurrentProcessId());
  HANDLE h = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD)kSize, name.c_str());
  if (!h) {
    g_failed = true;
    return false;
  }
  g_hdr = (Header *)MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, kSize);
  if (!g_hdr) {
    g_failed = true;
    return false;
  }
  g_hdr->magic = 0x4E574C46;
  return true;
}
}  // namespace

// efb_fbo : l'EFB ; (src_x, src_y, src_w, src_h) : rectangle presente, coordonnees GL.
void publish(GLuint efb_fbo, int src_x, int src_y, int src_w, int src_h) {
  if (!open_map())
    return;
  const DWORD now = GetTickCount();
  if (now - g_hdr->want_ms > 2000 || now - g_last < 33 || src_w <= 0 || src_h <= 0)
    return;
  g_last = now;
  GLint prev_read = 0, prev_draw = 0, prev_pack = 0;
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw);
  glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &prev_pack);
  if (!g_fbo) {
    glGenFramebuffers(1, &g_fbo);
    g_tex = nj_new_tex();
    nj_tex2d(g_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kMaxW, kMaxH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    nj_tex2d(0);
    nj_fbo(GL_DRAW_FRAMEBUFFER, g_fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_tex, 0);
    glGenBuffers(2, g_pbo);
    for (GLuint b : g_pbo) {
      glBindBuffer(GL_PIXEL_PACK_BUFFER, b);
      glBufferData(GL_PIXEL_PACK_BUFFER, (GLsizeiptr)kMaxW * kMaxH * 4, nullptr, GL_STREAM_READ);
    }
  }
  // taille de sortie : proportions gardees, 1024 x 768 au plus
  int w = src_w, h = src_h;
  if (w > kMaxW) {
    h = h * kMaxW / w;
    w = kMaxW;
  }
  if (h > kMaxH) {
    w = w * kMaxH / h;
    h = kMaxH;
  }
  nj_fbo(GL_READ_FRAMEBUFFER, efb_fbo);
  nj_fbo(GL_DRAW_FRAMEBUFFER, g_fbo);
  glBlitFramebuffer(src_x, src_y, src_x + src_w, src_y + src_h, 0, 0, w, h, GL_COLOR_BUFFER_BIT,
                    (w == src_w && h == src_h) ? GL_NEAREST : GL_LINEAR);
  nj_fbo(GL_READ_FRAMEBUFFER, g_fbo);
  const int cur = g_idx;
  glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[cur]);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  g_w[cur] = w;
  g_h[cur] = h;
  g_pending[cur] = true;
  // l'autre PBO contient l'image precedente : elle est prete, on la publie
  const int old = cur ^ 1;
  if (g_pending[old]) {
    glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[old]);
    const int ow = g_w[old], oh = g_h[old];
    if (const auto *px = (const uint8_t *)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, (GLsizeiptr)ow * oh * 4,
                                                            GL_MAP_READ_BIT)) {
      uint8_t *dst = (uint8_t *)(g_hdr + 1);
      for (int y = 0; y < oh; ++y)   // GL : ligne 0 = bas ; la memoire partagee : haut d'abord
        std::memcpy(dst + (size_t)y * ow * 4, px + (size_t)(oh - 1 - y) * ow * 4, (size_t)ow * 4);
      glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
      g_hdr->w = (uint32_t)ow;
      g_hdr->h = (uint32_t)oh;
      g_hdr->src_w = (uint32_t)src_w;
      g_hdr->src_h = (uint32_t)src_h;
      MemoryBarrier();
      g_hdr->seq = g_hdr->seq + 1;
    }
    g_pending[old] = false;
  }
  g_idx = old;
  glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)prev_pack);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_read);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prev_draw);
}

}  // namespace nj::live
#endif

#if defined(_WIN32) && !defined(NJ_RELEASE) && !defined(NJ_MOBILE)
void nj_live_publish(GLuint efb_fbo, int x, int y, int w, int h) { nj::live::publish(efb_fbo, x, y, w, h); }
#endif
