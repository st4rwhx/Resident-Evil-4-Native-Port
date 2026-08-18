#pragma once

// Commandes tactiles completes : on joue sans manette.
//
// Trois pieces :
//  - touch_event_feed() recoit les evenements SDL_FINGER* de la boucle hote ;
//  - TouchSource (une PadSource) les transforme en manette GameCube virtuelle, selon ce que
//    le jeu est en train de faire (re4_context.h) : stick, course, visee, invites d'action, QTE,
//    menus ;
//  - touch_overlay_frame() dessine les icones correspondantes par-dessus l'image.
//
// Tout tourne sur le fil principal : evenements, InputHub::update() et presentation.

#include "input/input_source.h"

#include <string>
#include <utility>
#include <vector>

union SDL_Event;

namespace nj::runtime::input {

void touch_event_feed(const SDL_Event& e);

// Commandes Wii (17/11) : acceleration totale de la « Wiimote » en g pour le coeur natif (actions de secousse 48/49/50,
// couteau Wii). Accelerometre du telephone (norme, ~1 g au repos comme la Wiimote) ; glisser vite au doigt pendant que
// le couteau est sorti = 2,5 g pendant 80 ms. 0 sans source.
float wii_accel_g();

// Vrai des qu'un doigt a touche l'ecran : la couche tactile s'active d'elle-meme, et reste
// invisible sur un PC qui n'en recoit jamais.
bool touch_seen();

// Disposition personnalisee des commandes de jeu (19/10, demande des joueurs) : chaque bouton garde un decalage (fraction de
// l'ecran) et un facteur de taille par rapport a sa place d'origine. Modifier la disposition : mode d'edition plein ecran
// (glisser un bouton, +/-, grille, remise a zero, valider).
void layout_edit_begin();
bool layout_editing();
// Regle un bouton par son nom (config.toml [touch_layout]) ; faux si le nom est inconnu.
bool layout_set_by_name(const std::string& name, float dx, float dy, float scale);
// Tous les reglages : (nom, « dx,dy,echelle »), pour config.toml.
std::vector<std::pair<std::string, std::string>> layout_export();
// Appele quand le joueur valide l'edition (le menu y enregistre config.toml).
void layout_set_saved_callback(void (*cb)());

// Appele apres le blit de l'image, avant l'echange des tampons.
void touch_overlay_frame(int draw_w, int draw_h);

class TouchSource : public PadSource {
public:
    void poll(PadState pads[4]) override;
};

} // namespace nj::runtime::input
