#pragma once
// Le rendu vu du reste du runtime : on lui confie la fenetre, puis chaque image du jeu (la liste
// des commandes GX decodees). Une seule implementation, OpenGL / OpenGL ES (renderer_gl.cpp).
#include <memory>
#include <vector>

#include "runtime/gx_command.h"

namespace nj::runtime::gx {

struct FrameRenderer {
  virtual ~FrameRenderer() = default;
  virtual void attach(void *window) = 0;
  virtual void draw_frame(const std::vector<GxOp> &frame) = 0;
};

std::unique_ptr<FrameRenderer> make_gl_renderer();

}  // namespace nj::runtime::gx
