#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace nj::runtime::input {

// Assignations clavier/souris, au format de l'UHD.
//
// Le format et le vocabulaire d'actions sont repris de RE4 UHD
// (BIO4/input.ini) plutot qu'inventes : deux contextes - MENUS et GAMEPLAY -
// une section par peripherique, un nom d'action par ligne, `00000` pour
// "non assignee", et plusieurs lignes possibles pour une meme action. Les
// noms sont ceux du jeu : KAMAE pour viser, CK pour l'action contextuelle,
// SSCRN pour l'inventaire, ST pour Start.
//
// Cela donne gratuitement la liste exacte des actions a couvrir, et un
// fichier que quelqu'un qui connait l'UHD sait lire tout de suite.
//
// Ce que le runtime fournit au jeu reste une manette GameCube : chaque action
// porte donc le bit de bouton (ou l'axe) qu'elle actionne.

enum class Target : uint8_t {
    Button,     // un bit de PAD
    StickX, StickY,        // stick principal, -1..+1
    SubStickX, SubStickY,  // stick C
    TriggerL, TriggerR,    // gachettes analogiques
};

struct Action {
    const char* name;   // "KEY_A", "KEY_KAMAE", ...
    Target target;
    uint16_t bits;      // bit PAD si Button
    int8_t sign;        // sens pour un axe
};

// Une source : touche du clavier, bouton de souris, ou mouvement de molette.
struct Source {
    enum Kind : uint8_t { None, Key, MouseButton, WheelUp, WheelDown } kind = None;
    int code = 0;       // SDL_Scancode, ou SDL_BUTTON_*
};

struct Binding {
    int action;         // indice dans la table des actions
    Source src;
};

// Charge les assignations. Sans fichier, les defauts sont ceux de l'UHD.
void bindings_load(const std::string& path);
const std::vector<Binding>& bindings_get();
const Action* actions_table(size_t* count);

// Nom lisible d'une source, pour l'ecran de reassignation et pour choisir
// l'icone a afficher ("E", "SPACE", "MB_LEFT"...).
std::string source_name(const Source& s);

}
