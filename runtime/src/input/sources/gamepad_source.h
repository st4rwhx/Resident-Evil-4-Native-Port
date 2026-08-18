#pragma once
// Manettes de l'hote (SDL GameController) -> manettes GameCube.
#include "input/input_source.h"

namespace nj::runtime::input {

class JoypadInput : public PadSource {
 public:
  void poll(PadState pads[4]) override;
};

}  // namespace nj::runtime::input
