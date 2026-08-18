#pragma once
#include <cstdint>
// Resolution interne et menu des parametres video (F10), a la maniere des options de l'UHD.
//
// L'EFB de la GameCube fait 640 x 480. Le rendu le dessine a l'echelle : un EFB de
// 640*S x 480*S, et toutes les coordonnees d'EFB du jeu (viewport, effacement, copies,
// presentation) sont multipliees par S. S = 0 dans le reglage veut dire « natif » : le plus
// petit S dont l'image couvre les pixels de la fenetre, recalcule quand elle change de taille.
//
// Les copies d'EFB vers une texture gardent l'echelle (texture S fois plus grande, lue en
// coordonnees normalisees) : le wiki Dolphin note que les flous de RE4 s'affinent d'autant,
// leur rayon etant exprime en texels d'EFB.

struct SDL_Window;

namespace nj::runtime::gfx {

constexpr int kEfbW = 640, kEfbH = 480, kMaxScale = 8;
// Version des reglages par defaut (config.toml, [meta] settings_version). Quand elle monte, le premier lancement
// retire du fichier les reglages propres a l'autre plateforme (voir config.cpp) : les defauts de l'appareil reviennent.
constexpr int kSettingsVersion = 3;   // 3 (18/11) : style Or et animation « toujours » par defaut sur mobile
extern bool g_settings_reset_notice;   // reglages remis par defaut a ce lancement : le menu F10 le dit une fois

extern int g_scale;          // echelle appliquee ce frame-ci (1..kMaxScale)
extern int g_scale_setting;  // reglage : 0 natif, 1..kMaxScale
extern bool g_menu_open;
extern bool g_title_home;   // ecran d'accueil de Nightjar affiche (menu titre, 18/11) : il garde les evenements
// Cadence d'affichage (i/s) : 30 = celle du jeu, plus = images intermediaires
// (src/hle/gx/frame_interp.cpp), 0 = sans limite. La logique du jeu reste a 30.
extern int g_fps_target;
// Images neuves du jeu montrees (coeur natif, gx.cpp) : le compteur d'i/s les compte, pas les presentations a l'ecran
// (la cadence fixe presente a chaque balayage, 60 fois par seconde, meme quand le jeu en produit 20 ; 15/11).
extern unsigned g_game_images;

// Faut-il presenter une image a ce tour de boucle ? new_game_frame : une image du jeu vient
// d'etre rendue. A 30 (la cadence du jeu), chaque tour presente, comme avant ; au-dessus, a
// l'echeance de la cadence choisie ; sans limite, a chaque tour.
bool present_due(bool new_game_frame);     // le menu F10 a la main : le jeu ne recoit plus la souris ni le clavier

// Ameliorations d'affichage (menu F10, config.toml [graphics]). Aucune ne touche l'image que le
// jeu rend dans l'EFB ; les copies d'EFB (effets) gardent leur filtrage d'origine.
// output_resampling : 0 bilineaire (reduction du GPU, comme Dolphin par defaut), 1 surface (defaut ; moyenne
// de tous les texels couverts, en lumiere lineaire, comme l'« Area Sampling » de Dolphin).
extern int g_resample;
// anisotropy : 1 (desactive), 2, 4, 8, 16 ; textures du jeu seulement.
extern int g_aniso;
// 21/10 : brouillard coupe (1) : tres vieux telephones seulement, non recommande, desactive par defaut. Agit en direct
// (cle des programmes, shader_key.cpp, et texte du nuanceur, tev_shader_gen.cpp).
extern int g_no_fog;
extern int g_compat_mask;   // compatibilite de rendu (bits, voir settings_menu.cpp) : 1 sans invalidation, 2 sans XF GPU, 4 sans residente, 8 sans caches
// aspect : 0 16:9 (Wii Edition, bande du jeu sur tout l'ecran), 1 4:3 (GameCube, avec ses bandes).
extern int g_aspect;
// Format de l'image sur l'ecran (19/10) : g_screen_fit 0 automatique (proportions gardees, un peu etiree sur les ecrans plus larges),
// 1 ajuste (barres), 2 remplit (recadre), 3 etire ; g_screen_ratio : 0 proportions du jeu, sinon une des kScreenRatios ci-dessous
// (l'image est alors montree a ce rapport, ajustee dans l'ecran).
extern int g_screen_fit, g_screen_ratio;
constexpr int kScreenRatioCount = 9;
float screen_ratio_value(int index);   // largeur / hauteur ; 0 : proportions du jeu
// hd_textures : 1 = textures HD de l'UHD (dossier textures_hd/, tools/re4/hdtex_build.py), 0 = origine.
extern int g_hd_textures;
// hd_cutscenes : 1 = textures HD aussi pour ce qui n'existe que dans les cinematiques temps reel
// (textures_hd/cutscene_keys.txt). Les textures communes au jeu suivent hd_textures.
extern int g_hd_cutscenes;
// hd_interface : 1 = textures HD de l'UHD pour les menus, l'ecran titre, la mallette, les objets et
// les documents (textures_hd/interface_keys.txt). L'interface de l'UHD a ete retouchee par endroits.
extern int g_hd_interface;
// hd_environment : decors (salles St*, textures_hd/room_keys.txt) ; hd_items : objets a ramasser et
// objets de l'interface (textures_hd/item_keys.txt). Les autres suivent hd_textures.
extern int g_hd_environment;
extern int g_hd_items;
// hd_weapons : armes (en main et dans la mallette, textures partagees, weapon_keys.txt) ;
// hd_case : mallette (cadre, grille, objets, case_keys.txt) ; hd_hud : HUD et effets communs
// (etc/core.das, hud_keys.txt). hd_textures ne couvre plus que les personnages et ennemis.
extern int g_hd_weapons;
extern int g_hd_case;
extern int g_hd_hud;
// uhd_case_models : modeles de la mallette de l'UHD (textures_hd/models/ss_pzzl.dat, fait par
// tools/re4/uhd_case_models.py) servis a la place de ss/Eng/ss_pzzl.dat ; pris au demarrage.
extern int g_uhd_case_models;
// early_drawdone (15/10, docs/PLAN_RADICAL_2026-10-15.md, levier A) : 1 = les signaux de fin de
// dessin (GXDrawDone, jetons) partent des l'analyse du flux, avant le decodage des sommets ; le jeu
// n'attend plus le decodage. Reglage cache (config.toml seulement), 0 par defaut.
extern int g_early_drawdone;
// Boutons affiches (HUD, invites des menus) : 0 GameCube, 1 Xbox, 2 PlayStation, 3 clavier,
// 4 mobile. Images dans textures_hd/buttons/<style>/<cle>.png (tools/re4/hud_buttons.py).
extern int g_button_style;
// Disposition de la manette : 0 GameCube (A->A... d'origine), 1 2 3 = Type I II III de RE4 UHD
// (BIO4/input.ini, ###XINPUT_CONTROLLER_A/B/C). hle/pad.cpp reecrit Key_type_tbl en consequence.
extern int g_controller_layout;
// Options du jeu lues et ecrites dans SystemSave (decompile : global.h SYSTEM_SAVE_WORK ; hle/pad.cpp, sur le fil du jeu) : valeur courante
// (-1 : pas encore lue) et demande du menu (-1 : rien a faire). Luminosite = octet 0x0A (34..114, 64 par defaut).
extern int g_game_cur_brightness, g_game_cur_reverse, g_game_cur_vibration, g_game_cur_knife;
extern int g_game_set_brightness, g_game_set_reverse, g_game_set_vibration, g_game_set_knife;
extern int g_game_cur_snd, g_game_cur_retry_ok;   // son : 0 mono, 1 stereo, 2 Dolby Pro Logic II ; retry possible (-1 inconnu, 0 non, 1 oui)
extern int g_game_set_snd;    // -1 rien, sinon le mode a appliquer (SndSetOutputMode du jeu, via call_game)
extern int g_game_set_menu;   // 1 : ouvrir le menu OPTIONS du jeu sur « Retry » ; 2 : sur « Load game » (le jeu fait le reste, avec ses confirmations)
extern int g_game_set_reset;   // 1 : retour a l'ecran titre (comme « GAME RESET » du jeu)
// Commandes tactiles (13/10) : style 0 classique (anneaux blancs), 1 pierre (jauge de vie de RE4),
// 2 or (overlay de stream) ; opacite au repos en pour cent (20..100), opaques sous le doigt.
extern int g_touch_style;
extern int g_title_menu;
extern int g_debug_mode;   // mode debug (Rapport) : aussi les barres et textes du debug du jeu natif (NJ_DEBUG_HUD)
extern int g_leon_outfit;  // tenue de Leon (18/11) : -1 automatique (le jeu), 0 veste, 1 chemise, 2 gilet, 3 RPD
extern int g_title_scene;  // 1 : le fond du menu titre est une vraie salle du jeu, en direct (18/11)   // 1 : menu titre de Nightjar (18/11) a la place de celui du jeu ; 0 : menu d'origine
extern int g_stick_aim;   // visee GameCube au stick : 0 d'origine, 1 rapide, 2 tres rapide (14/10)
extern int g_haptics;   // vibrations : 0 non, 1 legeres, 2 fortes (14/10)
void haptic_tap();      // petit retour au toucher d'un bouton (tactile)
extern int g_touch_opacity;
extern int g_touch_action;   // bouton Action : 0 toujours, 1 seulement quand il sert (14/10)
// Dernier peripherique utilise, comme LastUsedDevice de l'UHD : 0 clavier/souris, 1 manette.
extern volatile int g_last_device;
// Octets de ss_pzzl.dat au-dela des 0xE4000 que le jeu lui reserve avant le tas de l'ecran
// (sscrn.cpp heapOfs) : hle/case_models.cpp decale le tas d'autant. 0 = rien a faire.
extern uint32_t g_case_pzzl_extra;
// aim_mode : 0 = visee de la Wii Edition (la souris est la Wiimote, src/hle/wii_aim.cpp),
// 1 = visee GameCube (le stick fait bouger la visee, souris comprise), 2 = visee PC de l'UHD
// (souris « Modern » : le deplacement vise directement, le corps tourne). [wii_aiming] mode.
extern int g_aim_mode;
// aim_detach : visee PC, camera detachee de la visee (re4_tweaks DetachCameraFromAim : le bras
// vise dans sa plage, le corps ne tourne qu'au-dela). [wii_aiming] detach_camera.
extern int g_aim_detach;
// character : -1 = celui que l'histoire donne ; 0 Leon, 1 Ashley, 2 Ada, 3 HUNK, 4 Krauser,
// 5 Wesker (pG->x4FB8), applique au chargement de salle suivant (src/hle/char_select.cpp).
// [player] character.
extern int g_character;

// Presente la zone [sx0,sx1] x [sy0,sy1] (texels, origine en bas) de la texture tex dans la zone
// [dx0,dx1] x [dy0,dy1] du tampon draw_fbo (0 = la fenetre) par moyenne de surface. Faux si le programme n'a
// pas pu etre construit (l'appelant garde alors glBlitFramebuffer).
bool present_area(unsigned tex, int tex_w, int tex_h, int sx0, int sy0, int sx1, int sy1,
                  int dx0, int dy0, int dx1, int dy1, unsigned draw_fbo = 0);

inline int efb_w() { return kEfbW * g_scale; }
inline int efb_h() { return kEfbH * g_scale; }

// L'echelle que le reglage demande pour une fenetre de draw_h pixels de haut.
int wanted_scale(int draw_w, int draw_h);

// Chemin du config.toml ou le menu enregistre ses choix.
void set_config_path(const char *path);

// Menu F10 : a appeler une fois par image, apres la presentation et avant l'echange. Les
// evenements SDL lui passent par menu_event() ; F10 ouvre et ferme.
void menu_event(const void *sdl_event);
// Ouvre ou ferme le menu (bouton tactile des parametres, sur telephone).
void menu_toggle();
// Sauvegarde (.gci / .raw) deposee sur la fenetre : proposee a l'import dans l'onglet Saves (13/10).
void saves_offer(const char *path);
extern bool g_show_fps;
void fps_overlay_frame(SDL_Window *window);
void menu_frame(SDL_Window *window);
// Ecran d'accueil (menu titre de Nightjar, 18/11) : quand le jeu natif attend au titre (NJS_TITLE_WAIT).
void title_home_frame(SDL_Window *window);
// Ecran de preparation du premier lancement (P3, moteur seul) ; rien si aucune preparation.
void prep_frame(SDL_Window *window);

}  // namespace nj::runtime::gfx
