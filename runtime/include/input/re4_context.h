#pragma once

// Ce que fait le jeu en ce moment, lu dans la memoire invitee de RE4 (G4BE08) a chaque image.
// C'est ce qui rend les commandes tactiles contextuelles : les bons boutons au bon moment, les
// QTE en icones, les menus au doigt. Les adresses et offsets viennent du decompile
// (re4-decomp-upstream) ; chacun est cite a sa lecture dans re4_context.cpp.

#include <cstdint>

namespace nj::runtime::input {

enum class Re4Mode : uint8_t {
    Unknown,    // memoire pas encore initialisee, logos
    Gameplay,   // Leon controlable
    Event,      // cinematique temps reel (passable)
    Movie,      // video precalculee
    SubScreen,  // mallette, carte, fichiers, objets cles
    Shop,       // marchand
    Menu,       // ecran titre, options, pause, fin de partie, dialogue Oui/Non
};

enum class Re4Qte : uint8_t {
    None,
    Single,     // un bouton (A, B, X ou Y) : un tap
    Pair,       // deux boutons ensemble (L+R ou A+B) : un balayage
    Mash,       // martelement : taps repetes
};

struct Re4Context {
    bool valid = false;
    Re4Mode mode = Re4Mode::Unknown;
    // Situations precises (14/10, docs/TACTILE_ECRANS.md) : chaque ecran n'affiche que ses boutons.
    bool skippable = false;       // cinematique que Start annule vraiment (EventMgr::Run)
    bool text_wait = false;       // texte qui attend A (examen, dialogue ; code08 sans choix)
    bool card = false;            // ecran de la carte memoire (machine a ecrire, charger)
    bool death = false;           // mort de Leon / d'Ashley (gameDiedemo)
    bool death_continue = false;  //   ... avec « Continue? » Oui / Non affiche
    bool inventory = false;       // mallette (sous-ecran 1)
    bool map = false;             // carte (sous-ecran 2, ou onglet Map de la mallette)
    bool binoculars = false;      // jumelles (flags_420 bit 2 ou binoMode != 0)
    // Ecran titre avec son menu (START / LOAD / OPTIONS) affiche, et opacite du fondu du jeu
    // (Fade[], 0 = rien, 1 = ecran couvert) : pour le bandeau du portage.
    bool title_menu = false;
    float title_menu_alpha = 0.0f;  // opacite des unites du menu (le menu arrive en fondu)
    float fade = 0.0f;
    // Image en 16:9 : bandes cinesco du jeu actives (en jeu, cinematique) ou image du titre.
    bool wide = false;
    // Leon existe (pPL pointe dans le tas). Gameplay est le mode par defaut : pendant les logos et
    // le dialogue de carte, avant la tache du titre, il vaut aussi Gameplay sans joueur.
    bool player = false;
    bool aiming = false;
    int ashley_follow = -1;  // consigne donnee a Ashley (invite du jeu) : 1 elle suit (invite « Wait » 0x1E), 0 elle attend (« Follow me » 0x21), -1 inconnu
    bool ashley = false;   // Ashley est la : pSUB existe et son type (+0x100) vaut 3 (SubCharGetStatus)
    // La ligne de visee touche un ennemi (cObjWep wep.target, GetWepTargetPos) : curseur rouge.
    bool aim_target = false;
    uint32_t aim_target_em = 0;  // l'ennemi vise (wep.target)
    int aim_target_hp = 0;       // ses points de vie (cEm::hp, +0x320)
    int wep_no = -1;   // pG->wep_no, arme en main (0 : aucune)
    int wep_type = 0;  // pG->wep_type : variante (silencieux, lunette, crosse...)
    int case_level = 0;       // mallette : 0 S, 1 M, 2 L, 3 LL (objets 0x7C..0x7F de l'inventaire ; la marchande l'ameliore)
    int chapters_ended = 0;   // pG+0x4F8A : chapitres termines (SceChapterEnd), 14 = on joue le 5-3
    int room = 0;             // pG->room_id (stage << 8 | piece)
    int hp = -1;              // points de vie de Leon (pG +0x4FA4, comme la visite des salles), -1 : inconnu (20/10 : un coup lache les verrous)
    uint32_t leon_mot = 0;    // pPL->pMotion (cModel +0x1D8) : mouvement courant de Leon, 0 : aucun (releve kind -> mouvement, journal)
    int wep_item = -1; // identifiant d'objet de l'arme en main avec son accessoire (table wep_info de item.cpp), -1 : inconnu
    // Invite d'action affichee (Cckpt.action.no, 0 = aucune) et bouton(s) GameCube attendus.
    uint8_t prompt = 0;
    uint16_t prompt_pad = 0;
    int prompt_kind = -1;      // ActBtnWork.kind de l'invite (numero du libelle : 7 KICK, 2 JUMP OUT, 8 CLIMB UP...), -1 : aucune
    Re4Qte qte = Re4Qte::None;
    // Mallette : entree du menu principal (0 liste, 1 mallette, 2 carte, 3 fichiers, 4 sortie).
    int cursor = -1;
    int cursor_count = 0;
    // Choix d'un message a l'ecran (Oui/Non, machine a ecrire, marchand...) : position du
    // curseur de chaque choix, en pixels d'une image 640x480, pour les selectionner au doigt.
    struct Choice {
        float x, y;
    };
    Choice choices[8] = {};
    int choice_count = 0;
    int choice_cursor = -1;
    bool choice_vertical = false;  // haut/bas deplace le curseur (sinon gauche/droite)
    // Les deux grilles de la mallette (19/10) : 0 la mallette, 1 la zone verte du marchand (rangement temporaire d'un achat sans place).
    // Centre de la case (i, j) = origin + i * ax + j * ay, en pixels d'une image 640x480.
    struct Grid {
        bool valid = false;
        int w = 0, h = 0, cx = 0, cy = 0;   // taille et case du curseur
        float origin[2] = {}, ax[2] = {}, ay[2] = {};
    };
    Grid grids[2];
    bool hand = false;          // une piece est en main (pzlPlayer.hand), le jeu attend « poser »
    int cur_board = 0;          // grille du curseur
    float hand_x = 0, hand_y = 0;   // centre de la piece en main, en cases (la partie fractionnaire donne la parite de sa taille)
    // Grille de la mallette : centre de la case (x, y) = origin + x * ax + y * ay (640x480).
    bool grid = false;
    int grid_w = 0, grid_h = 0, grid_cx = 0, grid_cy = 0;
    float grid_origin[2] = {}, grid_ax[2] = {}, grid_ay[2] = {};
};

// Lit l'etat courant. Sans effet de bord ; appelable a chaque image depuis le fil principal (les
// lectures concurrentes du fil CPU invite sont des octets isoles : au pire une image en retard).
Re4Context re4_context_read();
// Rang d'un mouvement de Leon dans ses tables (pMotTbl +0x424, pRegistMot +0x428) : 1000 * table + rang, -1 si absent.
int re4_motion_index(uint32_t motion);
// Ecritures dans le jeu pour deplacer une piece de la mallette au doigt (puzzle.h : pzlPlayer, pzlBoard, pzlPiece). board : 0 mallette, 1 zone verte.
void re4_case_set_cursor(int board, int i, int j);              // grille et case du curseur (prendre une piece : puis X)
void re4_case_set_hand(int board, float x, float y);            // centre de la piece en main et grille courante
// Moteur de vibration demande par le jeu (Joy[0].vib_state, PADControlMotor) : 0 arret, 1 marche,
// 2 arret franc. Le jeu module l'intensite en allumant / eteignant d'une image a l'autre (14/10).
int re4_vib_state();
// Leon vise (pPL->xFD == 6), lecture seule et rapide (visee au stick, 14/10).
bool re4_aiming();

} // namespace nj::runtime::input
