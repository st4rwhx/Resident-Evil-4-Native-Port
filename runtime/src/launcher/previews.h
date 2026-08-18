#pragma once
// Apercus du menu Nightjar (rmenu) : petits dessins vectoriels animes qui montrent l'effet d'un reglage a droite de la liste.
// Tout est dessine par nous (ImDrawList) : aucun fichier du jeu ici. Chaque fonction recoit le rectangle [a, b] et des entiers (pas de
// globales) pour rester utilisable dans le banc d'essai.
#include "imgui.h"

namespace nj::rmenu::pv {

using Tr = const char *(*)(const char *);

struct Img { ImTextureID id = 0; int w = 0, h = 0; };
struct IconSet { Img stick, a, b, x, y, l, r, start, gun; };   // gun : l'arme du jeu (rendue chez le joueur)

// Atlas tactile de Nightjar (le meme que celui que dessine le jeu) : cases de `cell` pixels ; `style_first` = premiere case des pieces de style.
struct Atlas { ImTextureID id = 0; int w = 0, h = 0, cell = 192, style_first = 104; };

struct Ctx {
    ImDrawList *dl;
    ImVec2 a, b;        // rectangle de l'apercu
    float dp;
    ImFont *font;       // police de l'interface
    Tr tr;              // traduction
    ImU32 cream, dim, red, line, panel;
    ImTextureID shot = 0;   // image du jeu a l'ouverture (a l'envers, comme OpenGL la lit) ; 0 : repli gris
    double t = 0.0;         // temps (animation)
    const Atlas *atlas = nullptr;
    const IconSet *icons = nullptr;   // boutons du jeu (stick...) quand le disque est lu
};

// Visee et vibration : dessinees dans le style du HUD (image assombrie rouge, contour blanc a liseré noir, laser rouge) avec le vrai stick du jeu.
void aim_demo(const Ctx &c, int mode, int speed, int what, int cam);   // mode 0 pointeur, 1 stick, 2 souris ; speed 0..2 ; what 0 mode, 1 vitesse, 2 camera
void vibration(const Ctx &c, int level);                            // 0 non, 1 legere, 2 forte
// Boutons tactiles : le rendu exact du jeu (touch_source.cpp, emit des boutons) : lentille, icone teintee, cerclage ; memes positions
// (unite = 1/9 de la hauteur), memes teintes, meme transparence. style : 0 classique, 1 pierre, 2 or.
void touch_buttons(const Ctx &c, int style, int opacity, int simple, int size, int action_always);
// Display / Graphics
void screen_fit(const Ctx &c, float screen_w, float screen_h, int aspect43, int fit, int ratio);
void fps_corner(const Ctx &c, int on);
// Boutons de manette du jeu (HUD) : lus chez le joueur, donc seulement le style GameCube ; un autre style n'a pas d'image reelle.
void button_icons(const Ctx &c, const IconSet &s, int style);   // style : 0 GameCube (boutons du HUD), 4 mobile (atlas tactile) ; les autres n'ont pas d'image reelle

}  // namespace nj::rmenu::pv
