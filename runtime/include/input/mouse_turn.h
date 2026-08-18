#pragma once

// Rotation du joueur a la souris, ecrite directement dans la memoire de
// l'invite - la methode de re4_tweaks, transposee a notre recompilation.
//
// re4_tweaks (nipkownix/re4_tweaks, licence zlib) obtient sa visee en ecrivant
// dans la structure du joueur du RE4 UHD plutot qu'en simulant une manette.
// C'est ce qui fait la difference : une manette virtuelle ne peut pas depasser
// la resolution du stick, alors qu'une ecriture directe suit la souris au
// pixel. L'algorithme ci-dessous en reprend l'idee et les constantes ; le code
// est le notre, la cible n'etant pas le meme binaire.
//
// Ce qui rend la transposition possible, verifie et non suppose :
//   pPL      = 0x803159F4   (re4-decomp-upstream/config/G4BE08/symbols.txt)
//   cModel   Vec rot        au decalage 0xA0   (include/model.h:25)
//   cEm      cPlWep* pWep   au decalage 0x788  (include/em.h:284)
//   cPlWep   f32 pitch      au decalage 0x28   (include/pl_wep.h:111)
// soit rot.y a pPL->0xA4. re4_tweaks nomme ce meme champ `ang_A0` dans l'UHD :
// les deux versions partagent la disposition a cet endroit.

namespace nj::runtime::input {

// Appelee une fois par tour de boucle hote, avec le deplacement horizontal de
// la souris depuis l'appel precedent. Ne fait rien si NJ_MOUSETURN n'est pas
// mis, si le pointeur joueur est nul, ou si le jeu n'est pas en scene.
void mouse_turn_apply(int dx, int dy, float dt, bool aiming);

bool mouse_turn_enabled();

// Le jeu dessine-t-il une vraie scene (et non la facade) ?
bool game_in_scene();

} // namespace nj::runtime::input
