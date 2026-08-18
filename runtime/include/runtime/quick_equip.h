#pragma once
// Changer d'arme sans ouvrir la mallette (input/quick_equip.cpp, 20/10).
#include <cstdint>

namespace nj::runtime {

struct GuestCpu;
// Demande (n'importe quel fil) : equiper l'objet `item_id` a la prochaine lecture de la manette.
void quick_equip_request(uint16_t item_id);
// Fil du jeu, dans PADRead : execute la demande en attente.
void quick_equip_run(GuestCpu& ctx);

}  // namespace nj::runtime
