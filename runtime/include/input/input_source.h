#pragma once
// Une manette GameCube telle que le jeu la lit (PADStatus du SDK) et les sources qui la
// remplissent (manette de l'hote, clavier-souris, tactile, scripts d'essai...).
#include <cstdint>

namespace nj::runtime::input {

struct PadState {
  uint16_t buttons = 0;   // bits PAD_BUTTON_* du SDK
  int8_t stick_x = 0;
  int8_t stick_y = 0;
  int8_t substick_x = 0;  // C-stick
  int8_t substick_y = 0;
  uint8_t trigger_l = 0;
  uint8_t trigger_r = 0;
  uint8_t analog_a = 0;
  uint8_t analog_b = 0;
  int8_t err = -1;        // 0 : branchee ; -1 : pas de manette
};

// Chaque source ajoute ce qu'elle voit aux quatre manettes, une fois par image d'entrees.
class PadSource {
 public:
  virtual ~PadSource() = default;
  virtual void poll(PadState pads[4]) = 0;
};

}  // namespace nj::runtime::input
