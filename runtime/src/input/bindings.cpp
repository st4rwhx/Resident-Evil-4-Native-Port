#include "bindings.h"
#include "runtime/log.h"

#include <SDL.h>

#include <cctype>
#include <fstream>
#include <iostream>
#include <string>

namespace nj::runtime::input {

namespace {

// Bits de la manette GameCube (include/dolphin/pad.h du decompile).
constexpr uint16_t B_LEFT = 1 << 0, B_RIGHT = 1 << 1, B_DOWN = 1 << 2,
                   B_UP = 1 << 3, T_Z = 1 << 4, T_R = 1 << 5, T_L = 1 << 6,
                   B_A = 1 << 8, B_B = 1 << 9, B_X = 1 << 10, B_Y = 1 << 11,
                   B_START = 1 << 12;

// Les actions de RE4, nommees comme dans l'UHD. La colonne de droite dit ce
// que chacune actionne sur la manette que le runtime presente au jeu.
const Action kActions[] = {
    // deplacement : le stick principal, pas la croix
    {"KEY_FORWARD",     Target::StickY,    0,       +1},
    {"KEY_BACK",        Target::StickY,    0,       -1},
    {"KEY_LEFT",        Target::StickX,    0,       -1},
    {"KEY_RIGHT",       Target::StickX,    0,       +1},
    // viser : la gachette droite, a fond quand la touche est enfoncee
    {"KEY_KAMAE",       Target::TriggerR,  T_R,     +1},
    // Couteau : gachette L, pas B. src/game/pl_sub.cpp joyLKamae() teste
    // Key.on & 0x800, et Key_type_tbl[0][11] (pad.cpp) relie cette touche
    // logique au bit 0x40 de la manette, PAD_TRIGGER_L. Envoye sur B, le
    // couteau faisait courir ou annuler, et ne sortait jamais.
    {"KEY_KAMAE_KNIFE", Target::TriggerL,  T_L,     +1},
    // les boutons, tels quels
    {"KEY_A",           Target::Button,    B_A,      0},
    {"KEY_B",           Target::Button,    B_B,      0},
    {"KEY_X",           Target::Button,    B_X,      0},
    {"KEY_Y",           Target::Button,    B_Y,      0},
    {"KEY_Z",           Target::Button,    T_Z,      0},
    {"KEY_LB",          Target::TriggerL,  T_L,     +1},
    {"KEY_RB",          Target::TriggerR,  T_R,     +1},
    {"KEY_ST",          Target::Button,    B_START,  0},
    // alias semantiques de l'UHD : meme bouton, autre nom
    {"KEY_FIRE",        Target::Button,    B_A,      0},
    {"KEY_CK",          Target::Button,    B_A,      0},
    {"KEY_RUN",         Target::Button,    B_B,      0},
    {"KEY_CANCEL",      Target::Button,    B_B,      0},
    {"KEY_OK",          Target::Button,    B_A,      0},
    {"KEY_EV_CANCEL",   Target::Button,    B_B,      0},
    {"KEY_SSCRN",       Target::Button,    B_START,  0},
    {"KEY_OPTION",      Target::Button,    B_START,  0},
    // croix directionnelle
    {"KEY_U",           Target::Button,    B_UP,     0},
    {"KEY_D",           Target::Button,    B_DOWN,   0},
    {"KEY_L",           Target::Button,    B_LEFT,   0},
    {"KEY_R",           Target::Button,    B_RIGHT,  0},
    // stick C
    {"KEY_C_U",         Target::SubStickY, 0,       +1},
    {"KEY_C_D",         Target::SubStickY, 0,       -1},
    {"KEY_C_L",         Target::SubStickX, 0,       -1},
    {"KEY_C_R",         Target::SubStickX, 0,       +1},
    // navigation de menu
    {"KEY_LU",          Target::Button,    B_UP,     0},
    {"KEY_LD",          Target::Button,    B_DOWN,   0},
    {"KEY_LL",          Target::Button,    B_LEFT,   0},
    {"KEY_LR",          Target::Button,    B_RIGHT,  0},
};
constexpr size_t kActionCount = sizeof(kActions) / sizeof(kActions[0]);

// Defauts : la disposition relevee dans RE4 UHD (BIO4/input.ini, section
// GAMEPLAY / KEYBOARD). Conventions PC ordinaires - WASD, clic droit pour
// viser, clic gauche pour tirer - mais relevees plutot qu'inventees, pour que
// quelqu'un qui vient de l'UHD ne soit pas depayse.
const char* kDefaults[][2] = {
    {"KEY_FORWARD", "W"},         {"KEY_BACK", "S"},
    {"KEY_LEFT", "A"},            {"KEY_RIGHT", "D"},
    {"KEY_KAMAE", "MB_RIGHT"},    {"KEY_FIRE", "MB_LEFT"},
    {"KEY_RUN", "LSHIFT"},        {"KEY_CK", "E"},
    {"KEY_A", "E"},               {"KEY_B", "SPACE"},
    {"KEY_KAMAE_KNIFE", "MB_MIDDLE"}, {"KEY_ST", "TAB"},
    {"KEY_SSCRN", "TAB"},         {"KEY_OPTION", "ESCAPE"},
    {"KEY_CANCEL", "ESCAPE"},     {"KEY_EV_CANCEL", "ESCAPE"},
    {"KEY_OK", "RETURN"},         {"KEY_OK", "MB_LEFT"},
    {"KEY_U", "UP"},              {"KEY_D", "DOWN"},
    {"KEY_L", "LEFT"},            {"KEY_R", "RIGHT"},
    {"KEY_LU", "UP"},             {"KEY_LD", "DOWN"},
    {"KEY_LL", "LEFT"},           {"KEY_LR", "RIGHT"},
    {"KEY_C_U", "MA_WU"},         {"KEY_C_D", "MA_WD"},
    {"KEY_X", "R"},               {"KEY_Y", "Q"},
    {"KEY_Z", "Z"},
    // KEY_LB n'est **pas** assigne par defaut, et ce n'est pas un oubli.
    // Il arrive sur la gachette L, et la touche logique 11 que L declenche
    // bascule cEm::flags_420 bit 0x1000 (src/game/player.cpp:374), lequel
    // **interdit de viser** (joyKamae, src/game/pl_sub.cpp:796). Le jeu ne
    // l'efface que sur un nouvel appui de visee : quelqu'un qui maintient le
    // clic droit reste bloque sans comprendre pourquoi. Mesure a l'appui :
    // BLOQUE=1, flags_420=00001800, viser=0.
};
constexpr size_t kDefaultCount = sizeof(kDefaults) / sizeof(kDefaults[0]);

std::vector<Binding> g_bindings;

int action_index(const std::string& n) {
    for (size_t i = 0; i < kActionCount; ++i)
        if (n == kActions[i].name)
            return (int) i;
    return -1;
}

// Les noms de touches de l'UHD ne sont pas ceux de SDL (LSHIFT contre
// "Left Shift"), d'ou cette table ; tout le reste passe par SDL, qui connait
// deja les lettres, les chiffres et les touches de fonction.
struct KeyAlias { const char* uhd; const char* sdl; };
const KeyAlias kAliases[] = {
    {"LSHIFT", "Left Shift"},   {"RSHIFT", "Right Shift"},
    {"LCONTROL", "Left Ctrl"},  {"RCONTROL", "Right Ctrl"},
    {"LALT", "Left Alt"},       {"RALT", "Right Alt"},
    {"RETURN", "Return"},       {"ESCAPE", "Escape"},
    {"SPACE", "Space"},         {"TAB", "Tab"},
    {"BACKSPACE", "Backspace"}, {"DELETE", "Delete"},
    {"INSERT", "Insert"},       {"HOME", "Home"},
    {"END", "End"},             {"PGUP", "PageUp"},
    {"PGDN", "PageDown"},       {"UP", "Up"},
    {"DOWN", "Down"},           {"LEFT", "Left"},
    {"RIGHT", "Right"},         {"CAPSLOCK", "CapsLock"},
};

Source parse_source(std::string v) {
    Source s;
    for (char& c : v)
        c = (char) std::toupper((unsigned char) c);
    if (v.empty() || v == "00000")
        return s;
    if (v == "MB_LEFT")   { s.kind = Source::MouseButton; s.code = SDL_BUTTON_LEFT;   return s; }
    if (v == "MB_RIGHT")  { s.kind = Source::MouseButton; s.code = SDL_BUTTON_RIGHT;  return s; }
    if (v == "MB_MIDDLE") { s.kind = Source::MouseButton; s.code = SDL_BUTTON_MIDDLE; return s; }
    if (v == "MA_WU")     { s.kind = Source::WheelUp;   return s; }
    if (v == "MA_WD")     { s.kind = Source::WheelDown; return s; }
    // MA_UP/DOWN/LEFT/RIGHT sont les axes de la souris : ils ne passent pas
    // par une liaison booleenne, la visee les lira directement.
    if (v.rfind("MA_", 0) == 0)
        return s;
    const char* sdl = v.c_str();
    for (const auto& a : kAliases)
        if (v == a.uhd) { sdl = a.sdl; break; }
    const SDL_Scancode sc = SDL_GetScancodeFromName(sdl);
    if (sc == SDL_SCANCODE_UNKNOWN)
        return s;
    s.kind = Source::Key;
    s.code = (int) sc;
    return s;
}

// `quiet` pour les axes de souris, seule omission legitime : ils ne passent
// pas par une liaison booleenne. Tout le reste qui tombe est signale - un
// chargeur qui jette des lignes sans le dire cache ses propres bugs.
void add(const std::string& action, const std::string& value, bool warn = true) {
    const int a = action_index(action);
    if (a < 0) {
        if (warn)
            NJ_LOG << "[Input] action inconnue : " << action << std::endl;
        return;
    }
    const Source s = parse_source(value);
    if (s.kind == Source::None) {
        if (warn && value != "00000" && value.rfind("MA_", 0) != 0)
            NJ_LOG << "[Input] touche non reconnue : " << action << " = "
                      << value << std::endl;
        return;
    }
    g_bindings.push_back({a, s});
}

} // namespace

const Action* actions_table(size_t* count) {
    if (count)
        *count = kActionCount;
    return kActions;
}

const std::vector<Binding>& bindings_get() { return g_bindings; }

std::string source_name(const Source& s) {
    switch (s.kind) {
    case Source::Key: {
        const char* n = SDL_GetScancodeName((SDL_Scancode) s.code);
        std::string out = n ? n : "";
        for (const auto& a : kAliases)
            if (out == a.sdl)
                return a.uhd;
        for (char& c : out)
            c = (char) std::toupper((unsigned char) c);
        return out;
    }
    case Source::MouseButton:
        return s.code == SDL_BUTTON_LEFT  ? "MB_LEFT"
             : s.code == SDL_BUTTON_RIGHT ? "MB_RIGHT"
                                          : "MB_MIDDLE";
    case Source::WheelUp:   return "MA_WU";
    case Source::WheelDown: return "MA_WD";
    default:                return "00000";
    }
}

void bindings_load(const std::string& path) {
    g_bindings.clear();
    std::ifstream f(path);
    if (f) {
        // Le fichier de l'UHD a deux contextes. On ne lit pour l'instant que
        // GAMEPLAY : le runtime ne sait pas encore si le jeu est dans un menu,
        // et inventer cette information ferait plus de mal que de bien. Les
        // liaisons de menu suivront quand elle sera disponible.
        bool in_kb = false, in_gameplay = false;
        std::string line;
        int n = 0;
        while (std::getline(f, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
                line.pop_back();
            if (line.rfind("###", 0) == 0) {
                const std::string sec = line.substr(3);
                if (sec == "GAMEPLAY")      { in_gameplay = true;  in_kb = false; }
                else if (sec == "MENUS")    { in_gameplay = false; in_kb = false; }
                else if (sec == "KEYBOARD") { in_kb = in_gameplay; }
                else                        { in_kb = false; }
                continue;
            }
            const size_t eq = line.find('=');
            if (eq == std::string::npos || !in_kb)
                continue;
            std::string k = line.substr(0, eq), v = line.substr(eq + 1);
            auto trim = [](std::string& t) {
                while (!t.empty() && std::isspace((unsigned char) t.front()))
                    t.erase(t.begin());
                while (!t.empty() && std::isspace((unsigned char) t.back()))
                    t.pop_back();
            };
            trim(k);
            trim(v);
            const size_t before = g_bindings.size();
            add(k, v);
            if (g_bindings.size() != before)
                ++n;
        }
        NJ_LOG << "[Input] " << n << " liaisons lues dans " << path << std::endl;
    }
    if (g_bindings.empty()) {
        for (size_t i = 0; i < kDefaultCount; ++i)
            add(kDefaults[i][0], kDefaults[i][1]);
        NJ_LOG << "[Input] " << g_bindings.size() << " liaisons sur "
                  << kDefaultCount << " defauts (disposition relevee sur RE4 UHD)"
                  << std::endl;
        for (const Binding& b : g_bindings)
            NJ_LOG << "[Input]   " << kActions[b.action].name << " = "
                      << source_name(b.src) << std::endl;
    }
}

} // namespace nj::runtime::input
