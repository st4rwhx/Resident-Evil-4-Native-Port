#pragma once
// Menu Nightjar « style RE4 » (26/10/2026, docs/MENU_NIGHTJAR_STYLE_RE4.md) : widgets dessines par ImDrawList, pilotes par des DONNEES.
//
// Une option = une ligne de donnees (libelle, genre, pointeur sur la valeur, noms des choix, aide, apercu). Le meme tableau sert a
// dessiner le menu, a le naviguer (doigt, croix, clavier), a chercher une option et a montrer l'aide / l'apercu a droite.
// Habillage : par defaut sombre rouille et creme (repli sans disque) ; Skin::tex_* recoit plus tard l'atlas genere chez le joueur
// (ui_skin.cpp) : rien de Capcom n'est dans ce fichier.
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "imgui.h"

namespace nj::rmenu {

enum class Kind : uint8_t { Choice, Toggle, Slider, Info, Action };

struct Option {
    const char *label = "";              // cle de traduction (TXT)
    Kind kind = Kind::Choice;
    int *value = nullptr;                // Choice : valeur courante (une des values[]) ; Toggle : 0/1 ; Slider : entier
    const char *const *names = nullptr;  // Choice : un nom par choix
    const int *values = nullptr;         // Choice : valeur de chaque choix (nullptr : 0..n-1)
    int n = 0;
    int lo = 0, hi = 100, step = 5;      // Slider
    const char *hint = nullptr;          // phrase d'aide (cle de traduction), montree a droite
    const char *warn = nullptr;          // avertissement rouge (cle), montre si warn_when() est vrai
    bool (*warn_when)(int value) = nullptr;
    // Apercu dessine par l'appelant dans le rectangle [a, b] a droite de la liste (nullptr : aucun) ; `dp` = taille d'un dp
    std::function<void(ImDrawList *dl, ImVec2 a, ImVec2 b, float dp)> preview_fn;
    std::function<std::vector<std::pair<std::string, std::string>>()> table_fn;   // apercu en table : touche -> action (police du jeu)
    float preview_h = 0.625f;            // hauteur de l'apercu / sa largeur
    const char *preview_cap = nullptr;   // legende (cle de traduction) sous l'apercu
    int preview = 0;                     // 0 : aucun ; sinon identifiant d'apercu (voir Preview)
    bool (*enabled)() = nullptr;         // nullptr : toujours actif
    void (*on_change)(int value) = nullptr;   // appelee apres chaque changement (ex. SDL_GL_SetSwapInterval)
    const char *(*text)() = nullptr;     // Info : texte a droite ; Action : etiquette du bouton
    void (*run)() = nullptr;             // Action : appelee au toucher
    // Variantes a fermeture (pages construites a chaque image : mods, sauvegardes...) :
    std::string label_str;               // libelle deja pret (non traduit) ; remplace `label` s'il n'est pas vide
    std::function<std::string()> text_fn;          // Info / Action : texte dynamique (non traduit)
    std::function<void()> run_fn;                  // Action
    std::function<void(int)> change_fn;            // apres chaque changement
    std::vector<std::pair<std::string, std::function<void()>>> links;   // petits mots cliquables apres la valeur (HAUT / BAS / RETIRER)
};

struct Category {
    const char *label = "";
    int glyph = 0;                       // petit pictogramme dessine par nous (Glyph)
    // Colonne de la page principale (18/11) : 0 = « Le jeu » (les rubriques du menu OPTIONS d'origine), 1 = « Nightjar ».
    // Des qu'une rubrique est en colonne 1 et que l'ecran est large, la page principale s'affiche en deux colonnes, sans
    // defilement : toutes les rubriques se voient d'un coup (on n'oublie plus celles du bas).
    int column = 0;
    std::vector<Option> options;
    // Categorie libre (Mods, Sauvegardes...) : le contenu est dessine par l'appelant avec des widgets ImGui dans la zone donnee.
    std::function<void(ImVec2 size)> custom;
};

enum Glyph { kGlyphScreen, kGlyphEye, kGlyphPad, kGlyphBox, kGlyphDisk, kGlyphFlag };

// Police du jeu (font/sscrn_p.fnt, lue sur le disque du joueur) : atlas de cases de 32 px, une lettre par case. Les rectangles et les
// avances sont calcules sur l'atlas (build) ; l'ordre des cases est celui de nos notes (METHODS.md §4).
struct GlyphFont {
    ImTextureID tex = 0;
    int tw = 0, th = 0;
    // x, y, pw : rectangle dans l'atlas AGRANDI ; w, adv : largeur et avance en pixels d'origine (cases de 32 px)
    struct G { uint16_t x, y, pw, w; int16_t adv; };
    int scale = 3;                                   // agrandissement de l'atlas (bicubique + bords reprecises)
    int cell = 96;                                   // hauteur d'une case dans l'atlas agrandi
    std::vector<uint8_t> pixels;                     // RGBA de l'atlas agrandi, a envoyer a la carte graphique (tw x th)
    std::unordered_map<uint32_t, G> map;
    bool build(const uint8_t *rgba, int w, int h);   // faux si l'atlas n'a pas la taille attendue
    bool supports(const char *utf8) const;           // tous les caracteres du texte sont dans la police ?
};

struct Skin {
    const GlyphFont *gfont = nullptr;                // police du jeu (nullptr : serif systeme)
    ImU32 bg_col = IM_COL32(12, 9, 8, 255);
    ImU32 panel_col = IM_COL32(30, 22, 19, 255);
    ImU32 panel_hi = IM_COL32(52, 36, 29, 255);
    ImU32 rust = IM_COL32(122, 52, 33, 255);
    ImU32 line = IM_COL32(96, 70, 52, 255);
    ImU32 cream = IM_COL32(232, 220, 190, 255);
    ImU32 dim = IM_COL32(150, 134, 112, 255);
    ImU32 red = IM_COL32(196, 48, 38, 255);
    ImU32 off = IM_COL32(86, 76, 66, 255);
    ImU32 title = IM_COL32(176, 38, 34, 255);       // titres : rouge du jeu
    ImU32 cream_hi = IM_COL32(255, 252, 240, 255);   // entree choisie
    ImU32 dim_text = IM_COL32(150, 140, 128, 255);   // entrees non choisies
    ImFont *font = nullptr;                          // serif de l'interface (nullptr : la police par defaut)
    // Textures du jeu (generees chez le joueur, ui_skin) ; id == 0 : repli dessine.
    struct Tex { ImTextureID id = 0; int w = 0, h = 0; };
    Tex bg;       // fond : cloison rouillee
    Tex frame;    // cadre metallique a engrenage (centre transparent)
    Tex row;      // ligne de liste sombre filetee (ss_file : 228x28), 9 tranches
    Tex head;     // en-tete a fleches ◀ ▶ (ss_file : 241x28), 9 tranches
    Tex paper;    // panneau « papier » gris a grain (ss_file : 272x232), 9 tranches
    Tex rack;     // rack vertical de la mallette (ss_item : 160x288), 9 tranches
    Tex scroll;   // glissiere verticale du jeu (16x104, embouts en haut et en bas)
    Tex g_stick, g_lr, g_orb, g_a, g_r, g_b, g_y, g_start;   // g_orb = bouton X ; g_lr = gachette L   // pictos de boutons du jeu (24x24)
    // Apercu des reglages : image du jeu a l'ouverture du menu et image en direct (captures de l'ecran, a l'envers comme OpenGL les lit).
    Tex shot_before, shot_live;
    bool touch = false;         // dernier appareil = doigt : l'aide du bas parle de toucher
};

struct State {
    int category = 0;
    int page = -1;                       // -1 : liste des categories ; sinon la categorie ouverte
    int seen_row = -1;                   // derniere ligne dont on a verifie la visibilite
    int row = 0;                         // ligne selectionnee (navigation manette / clavier)
    float scroll = 0.0f, velocity = 0.0f;   // defilement avec inertie
    float tab_anim = 0.0f;
    float split = 0.5f;                  // position de la separation avant / maintenant (0..1)
    bool preview_active = false;         // l'option choisie a un apercu : l'appelant rafraichit shot_live
    char search[48] = {};
};

// Texte traduit : fourni par l'appelant (TXT dans le jeu, identite dans le banc d'essai).
using Translate = const char *(*)(const char *);

// Dessine le menu dans la zone [pos, pos+size] du ImDrawList courant ; retourne vrai si une valeur a change. `dp` = taille d'un dp en pixels.
// quitter : mis a vrai quand l'utilisateur demande le retour (bouton ◀ en haut a gauche, B, Echap).
bool draw(std::vector<Category> &cats, State &st, const Skin &sk, Translate tr, ImVec2 pos, ImVec2 size, float dp, bool &quitter);

// Ecran d'accueil (menu titre de Nightjar, 18/11) : une colonne courte d'entrees sur l'image du titre du jeu ; la premiere peut
// etre « grande » (CONTINUER) avec une ligne d'informations dessous. Rend l'indice de l'entree choisie (doigt, A, Entree) ou -1.
struct HomeItem {
    std::string label;       // deja traduit
    std::string sub;         // ligne d'informations (vide : aucune)
    bool big = false;
    bool enabled = true;
    ImTextureID img = 0;     // image de la partie (sauvegarde) a gauche du texte ; 0 : aucune
    float img_aspect = 320.0f / 224.0f;
};
int draw_home(const std::vector<HomeItem> &items, int &cursor, const Skin &sk, ImVec2 pos, ImVec2 size, float dp, const char *heading, bool left = false);

// Aide pour les essais : texte d'un choix.
const char *choice_name(const Option &o);

}  // namespace nj::rmenu
