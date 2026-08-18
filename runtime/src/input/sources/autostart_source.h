#pragma once
#include "input/input_source.h"

#include <chrono>
#include <cstdint>

namespace nj::runtime::input {

// Traverse la facade toute seule.
//
// Mesure du 2026-09-21 : il faut 185 secondes de logos, d'ecran-titre et de
// menu de debug avant d'atteindre la premiere seconde interessante. C'est le
// cout dominant de **chaque** experience de performance, bien devant la mesure
// elle-meme.
//
// Rien de tout cela ne demande de modifier le jeu. Les ecrans de logo
// s'annulent sur une pression, et titleExit (D:/Bio4/Prog/title.cpp) attend
// simplement A ou Start :
//
//     while (!(Joy[0].trg & 0x1100)) { titleDebugMenu(w); TaskSleep(1); }
//
// Le menu de debug y est deja pre-rempli depuis Debug/config.txt - [STAGE],
// [ROOM] et [JUMP_POINT] sont lus dans pG->stage_no, pG->room_no et pG->x4F9F.
// Il ne manque donc que le bouton, et le runtime tient la manette.
//
// Start uniquement, jamais A : le dialogue "No Memory Card found in Slot A"
// arrive avec son curseur sur "Retry", et A y boucle sans fin. Voir update().
//
// Le jeu lit le **front** (Joy.trg), pas l'etat maintenu : garder Start enfonce
// ne produirait qu'une seule pression. On alterne donc appui et relachement, et
// on le cadence en millisecondes - jamais en appels a update(), qui viennent
// d'une boucle hote a ~62 Hz sans rapport avec la cadence du jeu.
//
// NJ_AUTOSTART=<secondes> : duree pendant laquelle on appuie (defaut 150).
// Passe ce delai la source se tait, sinon Start ouvrirait le menu pause en
// boucle une fois en jeu.
class AutoStartSource : public PadSource {
public:
    AutoStartSource();
    ~AutoStartSource() override = default;

    void poll(PadState pads[4]) override;

    static bool enabled();

private:
    std::chrono::steady_clock::time_point start_;
    double seconds_ = 0.0;
    int presses_ = 0;
    uint64_t last_frames_ = 0, last_draws_ = 0;
    bool was_down_ = false;
    bool done_ = false;
};

}
