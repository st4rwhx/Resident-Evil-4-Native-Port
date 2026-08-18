#include "runtime/hw/hw.h"

namespace nj::runtime::hw {

// Tous les blocs dont Resident Evil 4 se sert. Le MI (protection memoire) n'est pas emule : le jeu
// n'y touche pas.
void attach_hardware(IoBus &bus) {
  attach_cp(bus);
  attach_pi(bus);
  attach_pe(bus);
  attach_vi(bus);
  attach_dsp(bus);
  attach_ai(bus);
  attach_di(bus);
  attach_si(bus);
  attach_exi(bus);
}

}  // namespace nj::runtime::hw
