#pragma once
#include "input/input_source.h"

#include <chrono>

namespace nj::runtime::input {

// Mode debug coupe : le menu de developpeur du titre passe tout seul.
//
// Le disque est le build de debug. Sur le titre, START (nouvelle partie) mene a titleExit
// (D:/Bio4/Prog/title.cpp), qui boucle sur le menu de developpeur tant que A ou Start n'est pas
// presse :
//
//     while (!(Joy[0].trg & 0x1100)) { titleDebugMenu(w); TaskSleep(1); }
//
// Son choix par defaut, DEBUG GAME, lance la salle de debug/config.txt ([STAGE] 0x01, [ROOM]
// 0x20 = r120, point 0) : le debut de la partie, comme NEW GAME dans le jeu vendu. Avec
// l'affichage de debug coupe (DiscView::release_config) ce menu serait en plus invisible,
// un ecran noir qui attend un bouton.
//
// Le menu s'ecrit par eprintf, qui garde ses lignes dans le tampon mess_ptr_buff meme quand
// rien n'est dessine (eprintf_main ne teste que la page). On y cherche « DEBUG GAME » et on
// presse A tant qu'il y est. NJ_DEBUG_HUD=1 garde le menu.
class DebugMenuSkipSource : public PadSource {
public:
    void poll(PadState pads[4]) override;
    static bool enabled();

private:
    std::chrono::steady_clock::time_point next_{};
    std::chrono::steady_clock::time_point release_{};
    bool down_ = false;
};

} // namespace nj::runtime::input
