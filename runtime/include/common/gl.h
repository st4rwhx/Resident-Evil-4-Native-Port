// Point d'entree unique vers OpenGL, desktop ou mobile.
//
// Sur desktop le runtime charge les pointeurs de fonction avec GLAD, parce que Windows n'expose
// rien au-dela de GL 1.1 et qu'il faut les resoudre a l'execution. Sur Android et iOS il n'y a
// rien a charger : GL ES 3.0 est une bibliotheque systeme dont les symboles sont lies
// directement, et GLAD n'a pas lieu d'etre. Les trois fichiers qui incluaient `<glad/glad.h>`
// passent donc par ici.
//
// Ce que cela coute en pratique est faible : le renderer n'utilise que ce qui existe des GL ES
// 3.0 - VAO, VBO, `glDrawElements` avec des indices `GL_UNSIGNED_INT`, textures non puissance de
// deux, et des shaders qui ne demandent que `layout(location=)`, `in`/`out` et `uniform`
// (voir gx/tev_shader_gen.cpp). ES 3.0 est present sur tout iPhone depuis l'A7 et sur
// Android 4.3, donc la contrainte ne se voit pas.
//
// Les rares appels sans equivalent en ES - `glPolygonMode`, `glGetTexImage`, la lecture de
// l'EFB par `glGetTexImage` - doivent etre gardes par `NJ_GLES` la ou ils apparaissent ;
// `NJ_GL_HAS(x)` sert a les ecrire lisiblement.
#pragma once

#include "common/platform.h"

#ifdef NJ_GLES
#  if defined(__APPLE__)   // iOS : OpenGL ES 3.0 du systeme (14/10)
#    include <OpenGLES/ES3/gl.h>
#    include <OpenGLES/ES3/glext.h>
#  else
#    include <GLES3/gl3.h>
#    include <GLES3/gl3ext.h>
#  endif
   // Pas de chargeur : les symboles ES sont lies directement. On garde la signature de GLAD pour
   // que `main.cpp` n'ait pas a savoir sur quelle plateforme il tourne.
   using GLADloadproc = void *(*)(const char *);
   inline int gladLoadGLLoader(GLADloadproc) { return 1; }
#  define NJ_GL_HAS(feature) 0
#else
#  include <glad/glad.h>
#  define NJ_GL_HAS(feature) 1
#endif

// Compteurs d'appels GL par image (GL ES seulement : sous GLAD ces noms sont deja des macros).
// Lus par GlRenderer::Render (ligne [GLCOUNT], avec NJ_FRAMELOG). Une incrementation par appel,
// sur le seul fil qui parle a GL. Demande du 30/09 : qu'est-ce qui fait attendre le GPU ?
#ifdef NJ_GLES
#include <cstdint>
namespace nj::glc {
enum { BindFB, Clear, TexImage, TexSubImage, ReadPixels, Finish, Flush, ClientWait, Blit, CopyTexSub,
       Fence, DrawElems, DrawArrays, Invalidate, UseProgram, BindTexture, Uniform, EnableDisable, TexParam,
       OtherState, Skipped, Map, N };
inline uint32_t n[N];
inline uint64_t tex_bytes;
}
#define NJ_GLC(k) (++::nj::glc::n[::nj::glc::k])
// Filtre des etats GL redondants (14/11, methode Dolphin / Melee Flip) : pendant draw_frame seulement
// (begin/end), on retient l'etat deja pose et on n'envoie pas un appel qui ne change rien. Hors de
// draw_frame tout passe tel quel (ImGui et la presentation touchent l'etat sans passer par ici) ; begin
// remet tout a « inconnu ». Chateau S5 : ~5 800 appels GL par image pour ~600 dessins.
// NJ_GLSHADOW=1 : actif (coupe par defaut : aucun gain mesure le 14/11). Appels evites : [GLCOUNT] evites=.
namespace nj::gls {
constexpr uint32_t U = 0xFFFFFFFFu;
inline bool on = false;
inline uint32_t cap[10], tex[32], unit_, dmask, dfunc, cmask, blend[4], vp[4], sc[4];
inline void reset() {
  for (auto &v : cap) v = U;
  for (auto &v : tex) v = U;
  for (auto &v : blend) v = U;
  for (auto &v : vp) v = U;
  for (auto &v : sc) v = U;
  unit_ = dmask = dfunc = cmask = U;
}
inline void begin(bool enable) { reset(); on = enable; }
inline void end() { on = false; }
inline int cap_idx(GLenum c) {
  switch (c) {
    case GL_BLEND: return 0; case GL_DEPTH_TEST: return 1; case GL_CULL_FACE: return 2;
    case GL_SCISSOR_TEST: return 3; case GL_POLYGON_OFFSET_FILL: return 4; case GL_DITHER: return 5;
    case GL_STENCIL_TEST: return 6; case GL_RASTERIZER_DISCARD: return 7;
    case GL_SAMPLE_ALPHA_TO_COVERAGE: return 8; case GL_PRIMITIVE_RESTART_FIXED_INDEX: return 9;
    default: return -1;
  }
}
inline bool same(uint32_t &slot, uint32_t v) {   // vrai : deja pose, appel evite
  if (on && slot == v) { NJ_GLC(Skipped); return true; }
  slot = on ? v : U;
  return false;
}
inline void Enable(GLenum c) { const int i = cap_idx(c); if (i >= 0 && same(cap[i], 1)) return; NJ_GLC(EnableDisable); glEnable(c); }
inline void Disable(GLenum c) { const int i = cap_idx(c); if (i >= 0 && same(cap[i], 0)) return; NJ_GLC(EnableDisable); glDisable(c); }
inline void ActiveTexture(GLenum t) { if (same(unit_, t)) return; NJ_GLC(OtherState); glActiveTexture(t); }
inline void BindTexture(GLenum target, GLuint name) {
  const uint32_t u = unit_ - GL_TEXTURE0;
  if (target == GL_TEXTURE_2D && unit_ != U && u < 32) { if (same(tex[u], name)) return; }
  NJ_GLC(BindTexture); glBindTexture(target, name);
}
inline void DeleteTextures(GLsizei n, const GLuint *t) {   // un nom libere peut revenir : on oublie les liaisons
  for (auto &v : tex) v = U;
  glDeleteTextures(n, t);
}
inline void DepthMask(GLboolean f) { if (same(dmask, f)) return; NJ_GLC(OtherState); glDepthMask(f); }
inline void DepthFunc(GLenum f) { if (same(dfunc, f)) return; NJ_GLC(OtherState); glDepthFunc(f); }
inline void ColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
  if (same(cmask, (r ? 1u : 0u) | (g ? 2u : 0u) | (b ? 4u : 0u) | (a ? 8u : 0u))) return;
  NJ_GLC(OtherState); glColorMask(r, g, b, a);
}
inline bool same4(uint32_t *s, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
  if (on && s[0] == a && s[1] == b && s[2] == c && s[3] == d) { NJ_GLC(Skipped); return true; }
  if (on) { s[0] = a; s[1] = b; s[2] = c; s[3] = d; } else s[0] = s[1] = s[2] = s[3] = U;
  return false;
}
inline void BlendFuncSeparate(GLenum a, GLenum b, GLenum c, GLenum d) {
  if (same4(blend, a, b, c, d)) return;
  NJ_GLC(OtherState); glBlendFuncSeparate(a, b, c, d);
}
inline void BlendFunc(GLenum a, GLenum b) {
  if (same4(blend, a, b, a, b)) return;
  NJ_GLC(OtherState); glBlendFunc(a, b);
}
inline void Viewport(GLint x, GLint y, GLsizei w, GLsizei h) {
  if (same4(vp, (uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h)) return;
  NJ_GLC(OtherState); glViewport(x, y, w, h);
}
inline void Scissor(GLint x, GLint y, GLsizei w, GLsizei h) {
  if (same4(sc, (uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h)) return;
  NJ_GLC(OtherState); glScissor(x, y, w, h);
}
}  // namespace nj::gls
#define glBindFramebuffer(...) (NJ_GLC(BindFB), glBindFramebuffer(__VA_ARGS__))
#define glClear(...) (NJ_GLC(Clear), glClear(__VA_ARGS__))
#define glTexImage2D(t, l, i, w, h, b, f, ty, p) \
  (NJ_GLC(TexImage), ::nj::glc::tex_bytes += (uint64_t)(w) * (uint64_t)(h) * 4u, \
   glTexImage2D(t, l, i, w, h, b, f, ty, p))
#define glTexSubImage2D(t, l, x, y, w, h, f, ty, p) \
  (NJ_GLC(TexSubImage), ::nj::glc::tex_bytes += (uint64_t)(w) * (uint64_t)(h) * 4u, \
   glTexSubImage2D(t, l, x, y, w, h, f, ty, p))
#define glReadPixels(...) (NJ_GLC(ReadPixels), glReadPixels(__VA_ARGS__))
#define glFinish(...) (NJ_GLC(Finish), glFinish(__VA_ARGS__))
#define glFlush(...) (NJ_GLC(Flush), glFlush(__VA_ARGS__))
#define glClientWaitSync(...) (NJ_GLC(ClientWait), glClientWaitSync(__VA_ARGS__))
#define glBlitFramebuffer(...) (NJ_GLC(Blit), glBlitFramebuffer(__VA_ARGS__))
#define glCopyTexSubImage2D(...) (NJ_GLC(CopyTexSub), glCopyTexSubImage2D(__VA_ARGS__))
#define glFenceSync(...) (NJ_GLC(Fence), glFenceSync(__VA_ARGS__))
#define glDrawElements(...) (NJ_GLC(DrawElems), glDrawElements(__VA_ARGS__))
#define glDrawArrays(...) (NJ_GLC(DrawArrays), glDrawArrays(__VA_ARGS__))
#define glInvalidateFramebuffer(...) (NJ_GLC(Invalidate), glInvalidateFramebuffer(__VA_ARGS__))
// Etat par lot (demande du 30/09 : combien d'appels GL par dessin ?).
#define glUseProgram(...) (NJ_GLC(UseProgram), glUseProgram(__VA_ARGS__))
#define glBindTexture(...) ::nj::gls::BindTexture(__VA_ARGS__)
#define glDeleteTextures(...) ::nj::gls::DeleteTextures(__VA_ARGS__)
#define glUniform1i(...) (NJ_GLC(Uniform), glUniform1i(__VA_ARGS__))
#define glUniform1iv(...) (NJ_GLC(Uniform), glUniform1iv(__VA_ARGS__))
#define glUniform2fv(...) (NJ_GLC(Uniform), glUniform2fv(__VA_ARGS__))
#define glUniform4fv(...) (NJ_GLC(Uniform), glUniform4fv(__VA_ARGS__))
#define glUniform4iv(...) (NJ_GLC(Uniform), glUniform4iv(__VA_ARGS__))
#define glUniformMatrix3fv(...) (NJ_GLC(Uniform), glUniformMatrix3fv(__VA_ARGS__))
#define glUniformMatrix4fv(...) (NJ_GLC(Uniform), glUniformMatrix4fv(__VA_ARGS__))
#define glEnable(...) ::nj::gls::Enable(__VA_ARGS__)
#define glDisable(...) ::nj::gls::Disable(__VA_ARGS__)
#define glTexParameteri(...) (NJ_GLC(TexParam), glTexParameteri(__VA_ARGS__))
#define glDepthMask(...) ::nj::gls::DepthMask(__VA_ARGS__)
#define glDepthFunc(...) ::nj::gls::DepthFunc(__VA_ARGS__)
#define glColorMask(...) ::nj::gls::ColorMask(__VA_ARGS__)
#define glBlendFunc(...) ::nj::gls::BlendFunc(__VA_ARGS__)
#define glBlendFuncSeparate(...) ::nj::gls::BlendFuncSeparate(__VA_ARGS__)
#define glViewport(...) ::nj::gls::Viewport(__VA_ARGS__)
#define glScissor(...) ::nj::gls::Scissor(__VA_ARGS__)
#define glActiveTexture(...) ::nj::gls::ActiveTexture(__VA_ARGS__)
#define glMapBufferRange(...) (NJ_GLC(Map), glMapBufferRange(__VA_ARGS__))
#endif

// Raccourcis de Nightjar pour les etats qu'on pose partout (filtre d'une texture 2D, bascule d'un
// etat, texture liee) : une ligne au lieu de deux et le meme vocabulaire dans tout le rendu.
inline void nj_tex_filter(GLint how) {
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, how);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, how);
}
inline void nj_tex_mag(GLint how) { glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, how); }
inline void nj_cap(GLenum what, bool on) {
  if (on) glEnable(what);
  else glDisable(what);
}
inline void nj_tex2d(GLuint name) { glBindTexture(GL_TEXTURE_2D, name); }
inline void nj_unit(int slot) { glActiveTexture(GLenum(GL_TEXTURE0 + slot)); }
inline void nj_depth_write() { glDepthMask(GL_TRUE); }
inline void nj_wipe(GLbitfield planes) { glClear(planes); }
#if defined(NJ_GLES) && defined(__APPLE__)
// iOS : le « framebuffer 0 » n'existe pas, SDL dessine dans un framebuffer a lui (SDL_GetWindowWMInfo).
// Lier 0 donnait GL_INVALID_FRAMEBUFFER_OPERATION (0x506) a chaque image : ecran noir.
extern GLuint g_nj_default_fbo;
extern GLuint g_nj_default_rb;   // buffer couleur de la fenetre (CAEAGLLayer)
inline void nj_fbo(GLenum target, GLuint fb) { glBindFramebuffer(target, fb ? fb : g_nj_default_fbo); }
// SDL_GL_SwapWindow presente le renderbuffer LIE a GL_RENDERBUFFER a cet instant (README iOS de SDL) : il faut
// lier celui de la fenetre, pas le tampon de profondeur de l'EFB resté lie par le moteur.
inline void nj_before_swap() {
  glBindFramebuffer(GL_FRAMEBUFFER, g_nj_default_fbo);
  glBindRenderbuffer(GL_RENDERBUFFER, g_nj_default_rb);
}
#else
inline void nj_fbo(GLenum target, GLuint fb) { glBindFramebuffer(target, fb); }
inline void nj_before_swap() {}
#endif
inline GLuint nj_new_tex() {
  GLuint t = 0;
  glGenTextures(1, &t);
  return t;
}
