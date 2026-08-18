#pragma once
// Clavier et souris -> manette GameCube du port 0 (touches de bindings.cpp, visee a la souris).
#include "input/input_source.h"

namespace nj::runtime::input {

class KbmSource : public PadSource {
 public:
  void poll(PadState pads[4]) override;
};

}  // namespace nj::runtime::input
