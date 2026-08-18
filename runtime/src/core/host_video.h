#pragma once
// Fenetre, contexte GL, tampon d'image du GX (EFB : couleur + profondeur, a l'echelle interne),
// texture des videos (XFB converti) et sortie audio de l'hote.
#include <SDL.h>

#include "common/gl.h"

namespace nj::runtime {

struct HostVideo {
  SDL_Window *window{};
  SDL_GLContext gl = nullptr;
  SDL_AudioDeviceID audio = 0;
  GLuint efb_fbo = 0, efb_color = 0, efb_depth = 0;
  GLuint xfb_texture = 0;
};

// Ouvre tout ; false (message deja journalise) si SDL, la fenetre ou le contexte GL echouent.
bool host_video_open(HostVideo &hv);
// Realloue l'EFB a l'echelle interne courante (gfx::efb_w / efb_h).
void host_video_resize_efb(const HostVideo &hv);
void host_video_close(HostVideo &hv);

}  // namespace nj::runtime
