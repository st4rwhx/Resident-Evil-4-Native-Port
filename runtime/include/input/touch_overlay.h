#pragma once

// Incrustation des commandes tactiles : des icones en couleur dessinees par-dessus l'image du jeu,
// apres le blit de l'EFB et avant l'echange des tampons. Tout se passe sur le fil principal (celui
// qui possede le contexte GL et qui recoit les evenements SDL), comme TouchSource::update.
//
// Les coordonnees sont en pixels du tampon d'affichage (SDL_GL_GetDrawableSize), origine en haut
// a gauche, comme les evenements tactiles une fois multiplies par la taille.

#include <cstdint>
#include <string>
#include <vector>

namespace nj::runtime::input::touch {

// Indice dans l'atlas genere par tools/touch_icons.py (liste NAMES, meme ordre).
enum Icon : uint8_t {
    IconRing, IconKnob, IconRun, IconAim, IconFire, IconKnife, IconReload, IconAction,
    IconCase, IconMap, IconPause, IconSkip, IconLook, IconTap, IconSwipe, IconDisc,
    IconA, IconB, IconX, IconY, IconL, IconR, IconZ, IconStart,
    IconArrow, IconLock, IconTurn, IconCheck, IconCross, IconDpad, IconMash, IconKick,
    IconStick, IconAshley, IconZoom, IconGrenade, IconTalk, IconBack, IconOk, IconBlank,
    // Couches des icones a main, animees separement (touch_source.cpp) : la main seule, puis
    // les effets poses autour du bout de l'index, au meme endroit que dans l'icone complete.
    IconHand, IconFxRays3, IconFxWaves, IconFxSwipe, IconFxRays5, IconFxX5,
    // Curseur de visee de la Wii Edition (texture 0xF9 du HUD Wii), blanc : teinte au dessin.
    IconWiiCursor,
};

struct Sprite {
    Icon icon;
    float x, y;        // centre
    float size;        // cote, en pixels
    uint32_t rgba;     // teinte, 0xRRGGBBAA
    float angle = 0;   // rotation en radians (fleches)
    // Icones animees image par image (kFlipbooks de l'atlas, Leon qui court...) : temps en
    // secondes dans l'animation, qui boucle. Negatif : horloge commune (SDL_GetTicks).
    float anim = -1.0f;
    bool flip = false; // miroir horizontal (main droite / main gauche de Leon)
    int cell = -1;     // case explicite de l'atlas (bandeau de l'ecran titre), sinon l'icone
    uint8_t span = 1;  // largeur en cases (bandes) : le quad fait size * span sur size
    bool additive = false;  // melange additif (halos, comme blendType 1 du jeu)
    int8_t half = 0;        // -1 : moitie gauche de l'icone seulement, +1 : moitie droite (deux silhouettes d'Ashley, 20/10)
    uint8_t atlas = 0;      // 0 : atlas principal ; 1 : atlas des icones d'action (touch_action_atlas.inc), `cell` y est une case
};

// Dessine la liste dans le tampon par defaut. Sauvegarde et restaure l'etat GL qu'elle touche :
// le renderer GX ne suppose rien de l'etat entre deux images, mais rien ne coute de rester neutre.
// Case de l'atlas de la gerbe de touche de la visee Wii (tools/atlas_hitmarker.py).
int overlay_hitmarker_cell();
// Pieces des styles tactiles (tools/touch_style_atlas.py) : 0 lentille pierre, 1 cerclage pierre,
// 2 lentille or, 3 cerclage or, 4 sillon d'etat, 5 pommeau pierre, 6 pommeau or.
int overlay_style_cell(int k);
int overlay_zoom_out_cell();
// Atlas principal decode (alpha droit, non premultiplie), pour dessiner les memes boutons ailleurs (apercu du menu). Cases de 192, 8 par ligne.
bool overlay_atlas_rgba(std::vector<unsigned char>& rgba, int& w, int& h);
// Icone d'arme generee chez le joueur (icon_gen.cpp, cache local) par numero d'objet (0x21 pistolet de depart, 0x23 Red9) ; image carree `size` x `size`.
bool weapon_icon_rgba(int item_id, std::vector<uint8_t>& rgba, int& size);
// Icones d'action de Leon (tools/touch_action_icons.py) : numero de l'animation d'apres son nom (-1 : absente), puis case de
// l'image courante au temps t (secondes, boucle). Les sprites de ces cases portent atlas = 1.
int overlay_action_book(const char* name);
// Icones generees chez le joueur (icon_gen.cpp) : noms (« w_it23 »...), image RGBA droite w x h faite de cases cell x cell, 8 par ligne. Remplace
// l'atlas des armes ; a appeler depuis le fil de rendu (le chargement GL se fait a l'image suivante).
void overlay_set_dynamic_icons(const std::vector<std::string>& names, const std::vector<uint8_t>& rgba, int w, int h, int cell);
// Lance (une fois) la generation des icones d'armes a partir du disque et les remet a l'ecran quand elles sont pretes.
void icon_gen_poll();
int overlay_action_cell(int book, float t);
bool overlay_action_ring(int book);
int overlay_action_atlas(int book);   // 1 : cases de 192 (animations, dessins) ; 2 : cases de 256 (armes)   // l'anneau est dans les images (animations de Leon) ; sinon on le pose   // loupe « - » (carte du jeu, 14/10)
void overlay_draw(const std::vector<Sprite>& sprites, int draw_w, int draw_h);

// Rectangle (en pixels du tampon) ou l'image du jeu est presentee, 4:3 centre. Le reste de
// l'ecran d'un telephone (bandes laterales) accueille les commandes sans masquer le jeu.
//
// 16:9 (comme la Wii Edition en mode large) : quand les bandes cinesco du jeu sont la, la bande
// d'image entre elles (lignes 56..392 des 448 de l'XFB) est exactement un 16:9 ; on presente
// cette bande seule, ecran rempli, sans deformation. game_rect rend alors le 4:3 virtuel qui la
// contient (plus haut que l'ecran), pour que le toucher et le bandeau du titre restent justes.
struct Rect { float x, y, w, h; };
Rect game_rect(int draw_w, int draw_h);
constexpr float kWideTop = 56.0f / 448.0f, kWideH = 336.0f / 448.0f;
void set_wide(bool on);
bool wide();
// Rectangle ou la bande 16:9 est presentee (valable quand wide()).
Rect wide_rect(int draw_w, int draw_h);

// Bandeau « native / ANDROID (WINDOWS) PORT BY AYANO » de l'ecran titre, anime comme le logo du
// jeu. title_menu : le menu du titre existe ; menu_alpha : son opacite (le bandeau s'allume quand
// le menu apparait et le suit) ; fade_cover : opacite du fondu du jeu (0..1).
void title_banner_draw(bool title_menu, float menu_alpha, float fade_cover, int draw_w, int draw_h);
// Meme bandeau, place par l'appelant (menu titre de Nightjar, 18/11) : cx = centre, cy = centre de « native »,
// lh = hauteur des lettres (pixels de l'image), t = secondes depuis l'apparition, vis = opacite.
void title_banner_draw_at(float cx, float cy, float lh, float t, float vis, int draw_w, int draw_h);

} // namespace nj::runtime::input::touch
