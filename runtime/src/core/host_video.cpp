// Fenetre, contexte GL, EFB et audio de l'hote (host_video.h).
#include "host_video.h"

#include "runtime/config.h"
#include "runtime/env.h"
#include "runtime/gfx_settings.h"
#include "runtime/log.h"
#if defined(__APPLE__)
#include <SDL_syswm.h>
#endif

extern void gx_bind_window(void *);
#if defined(__APPLE__)
GLuint g_nj_default_fbo = 0;   // globales : declarees dans common/gl.h
GLuint g_nj_default_rb = 0;
#endif

namespace nj::runtime {
namespace {

// Contexte demande : GL 3.2 core sur ordinateur ; GL ES 3.0 sur telephone, qui couvre tout ce que
// le rendu utilise (VAO, indices 32 bits, textures non puissance de deux) depuis Android 4.3.
// Tampon par defaut RGBA8, profondeur 24, gabarit 8, double tampon.
void request_gl_attributes() {
  struct Attr {
    SDL_GLattr attr;
    int value;
  };
  static const Attr kAttrs[] = {
#ifdef NJ_GLES
      {SDL_GL_CONTEXT_MAJOR_VERSION, 3}, {SDL_GL_CONTEXT_MINOR_VERSION, 0},
      {SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES},
#else
      {SDL_GL_CONTEXT_MAJOR_VERSION, 3}, {SDL_GL_CONTEXT_MINOR_VERSION, 2},
      {SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE},
#endif
      {SDL_GL_RED_SIZE, 8},   {SDL_GL_GREEN_SIZE, 8},   {SDL_GL_BLUE_SIZE, 8},
      {SDL_GL_ALPHA_SIZE, 8}, {SDL_GL_DEPTH_SIZE, 24},  {SDL_GL_STENCIL_SIZE, 8},
      {SDL_GL_DOUBLEBUFFER, 1},
  };
  for (const Attr &a : kAttrs)
    SDL_GL_SetAttribute(a.attr, a.value);
#if defined(NJ_DEVTOOLS) && defined(NJ_GLES)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);   // KHR_debug : messages d'erreur du pilote (renderer_gl.cpp), build de dev seulement
#endif
#if defined(__APPLE__) && !defined(NJ_GLES)
  // Sans sens en ES, et refuse par certains pilotes mobiles.
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
}

void log_display() {
  SDL_DisplayMode mode;
  if (SDL_GetCurrentDisplayMode(0, &mode) == 0)
    NJ_LOG << "[GL] ecran 0 : " << mode.w << "x" << mode.h << " " << SDL_GetPixelFormatName(mode.format)
             << std::endl;
  else
    NJ_ERR << "[GL] aucun mode d'affichage (ecran verrouille ou absent ?) : " << SDL_GetError() << std::endl;
}

// Texture couleur et tampon de profondeur de l'EFB, a l'echelle interne.
void allocate_efb_storage(const HostVideo &hv) {
  glBindTexture(GL_TEXTURE_2D, hv.efb_color);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gfx::efb_w(), gfx::efb_h(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glBindRenderbuffer(GL_RENDERBUFFER, hv.efb_depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, gfx::efb_w(), gfx::efb_h());
}

void linear_filtering() {
  for (GLenum p : {GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER})
    glTexParameteri(GL_TEXTURE_2D, p, GL_LINEAR);
}

}  // namespace

bool host_video_open(HostVideo &hv) {
  // Un doigt ne joue pas aussi la souris (sur telephone la souris vise et clique : chaque geste
  // tactile serait double) ; paysage seulement.
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) != 0) {
    NJ_ERR << "SDL_Init : " << SDL_GetError() << "\n";
    return false;
  }
  request_gl_attributes();
  log_display();

  const Config &cfg = Config::get();
  uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL;
  if (cfg.fullscreen)
    flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#ifdef NJ_MOBILE
  flags |= SDL_WINDOW_FULLSCREEN;   // plein ecran immersif, la taille demandee est ignoree
#endif
  hv.window = SDL_CreateWindow("Nightjar", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, cfg.window_width,
                               cfg.window_height, flags);
  if (!hv.window) {
    NJ_ERR << "SDL_CreateWindow : " << SDL_GetError() << "\n";
    return false;
  }
  gx_bind_window(hv.window);
  hv.gl = SDL_GL_CreateContext(hv.window);
  if (!hv.gl) {
    NJ_ERR << "SDL_GL_CreateContext : " << SDL_GetError() << "\n";
    return false;
  }
  if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    NJ_ERR << "glad : fonctions GL introuvables\n";
    return false;
  }
  NJ_LOG << "[GL] " << (const char *)glGetString(GL_RENDERER) << " | " << (const char *)glGetString(GL_VERSION)
           << std::endl;
  // NJ_NOVSYNC=1 : la boucle hote ne suit plus le rafraichissement de l'ecran.
  SDL_GL_SetSwapInterval(NJ_ENV("NJ_NOVSYNC") ? 0 : 1);

#if defined(__APPLE__)
  {   // framebuffer de la fenetre iOS
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    if (SDL_GetWindowWMInfo(hv.window, &wm)) {
      g_nj_default_fbo = wm.info.uikit.framebuffer;
      g_nj_default_rb = wm.info.uikit.colorbuffer;
    }
    NJ_LOG << "[GL] framebuffer de la fenetre : " << g_nj_default_fbo << ", renderbuffer couleur : " << g_nj_default_rb << std::endl;
  }
#endif
  // EFB : texture couleur (filtree lineairement a la presentation) + profondeur.
  glGenFramebuffers(1, &hv.efb_fbo);
  glGenTextures(1, &hv.efb_color);
  glGenRenderbuffers(1, &hv.efb_depth);
  allocate_efb_storage(hv);
  glBindTexture(GL_TEXTURE_2D, hv.efb_color);
  linear_filtering();
  glBindFramebuffer(GL_FRAMEBUFFER, hv.efb_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hv.efb_color, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, hv.efb_depth);
  const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  NJ_LOG << "[GL] EFB : " << (status == GL_FRAMEBUFFER_COMPLETE ? "complet" : "INCOMPLET") << std::endl;
  nj_fbo(GL_FRAMEBUFFER, 0);

  // Videos : XFB converti en RGBA, televerse a chaque image de video.
  glGenTextures(1, &hv.xfb_texture);
  glBindTexture(GL_TEXTURE_2D, hv.xfb_texture);
  linear_filtering();

  // Audio de la console : 32 kHz, stereo 16 bits, par blocs de 1024 echantillons.
  SDL_AudioSpec asked{}, got{};
  asked.freq = 32000;
  asked.format = AUDIO_S16SYS;
  asked.channels = 2;
  asked.samples = 1024;
  hv.audio = SDL_OpenAudioDevice(nullptr, 0, &asked, &got, 0);
  if (hv.audio > 0)
    SDL_PauseAudioDevice(hv.audio, 0);
  return true;
}

void host_video_resize_efb(const HostVideo &hv) {
  allocate_efb_storage(hv);
  NJ_PRINTF("[Rendu] resolution interne %dx : %d x %d\n", gfx::g_scale, gfx::efb_w(), gfx::efb_h());
}

void host_video_close(HostVideo &hv) {
  if (hv.audio > 0)
    SDL_CloseAudioDevice(hv.audio);
  if (hv.gl)
    SDL_GL_DeleteContext(hv.gl);
  if (hv.window)
    SDL_DestroyWindow(hv.window);
  SDL_Quit();
  hv = HostVideo{};
}

}  // namespace nj::runtime
