#include "runtime/perf_journal.h"
#include "runtime/gfx_settings.h"
#include "runtime/log.h"
#include "runtime/config.h"
#include "input/touch_source.h"

#include "input/re4_context.h"
#include "input/wii_pointer.h"
#include "input/touch_overlay.h"
#include "runtime/env.h"

#include <SDL.h>
#include <string>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <deque>
#include <vector>

// Compteur d'images du runtime (src/hle/gx.cpp) : cadence des appuis synthetiques.
extern uint64_t g_st_frames;

namespace nj::runtime::gfx { extern int g_touch_simple, g_touch_controls, g_touch_anim, g_touch_size, g_touch_lock; }  // settings_menu.cpp
namespace nj::runtime::input {
void wii_touch_add(float dx, float dy);       // sources/mouse_keyboard_source.cpp
void wii_touch_set_aiming(bool on);

namespace {

using namespace touch;

// Bits de la manette GameCube (PADStatus.button).
constexpr uint16_t kPadLeft = 0x0001, kPadRight = 0x0002, kPadDown = 0x0004, kPadUp = 0x0008;
constexpr uint16_t kPadZ = 0x0010, kPadR = 0x0020, kPadL = 0x0040;
constexpr uint16_t kPadA = 0x0100, kPadB = 0x0200, kPadX = 0x0400, kPadY = 0x0800;
constexpr uint16_t kPadStart = 0x1000;

// Commandes de RE4 GameCube (type de touches 0, Key_type_tbl[0] dans src/game/pad.cpp) :
// courir = B maintenu, viser = R, couteau = L, action/tir = A, mallette = Y (Key 0x100000),
// carte = Z (Key 0x200000), options = Start, recharger = B pendant que R est tenu
// (PlKeyReloadType 0), demi-tour = bas + B, passer une cinematique = Start.
constexpr uint16_t kRun = kPadB, kAim = kPadR, kKnife = kPadL, kAction = kPadA;
constexpr uint16_t kReload = kPadB, kInventory = kPadY, kMap = kPadZ, kPause = kPadStart;
constexpr uint16_t kSkip = kPadStart;
constexpr uint16_t kAshley = kPadX;  // PlSetWhistle : Key.trg 0x200 = Key_type_tbl[0][9] = X
// Boutons Oui / Non du « Continue? » (valeurs hors manette : traites au toucher, 14/10)
constexpr uint16_t kDeathYes = 0xFFF1, kDeathNo = 0xFFF2;
constexpr uint16_t kZoomIn = 0xFFF3, kZoomOut = 0xFFF4;
constexpr uint16_t kBinoExit = 0xFFF5;   // baisser les jumelles (B puis Y : la touche annulation du jeu)

// ---------------------------------------------------------------------------------------------
// Doigts, alimentes par la boucle hote.

struct Finger {
    SDL_FingerID id = 0;
    bool down = false;
    float x = 0, y = 0;    // en pixels du tampon d'affichage
    float x0 = 0, y0 = 0;  // point de contact
    uint32_t t0 = 0;       // SDL_GetTicks au contact
    int owner = -1;        // commande qui a pris le doigt
    uint32_t okey = 0;     // identite du bouton tenu (icone, touche) : la liste est reconstruite a chaque image (voir owner_remap)
    bool fresh = true;     // contact pas encore attribue
    bool released = false;
    float px = 0, py = 0;  // position a la lecture precedente (visee au doigt)
    bool has_prev = false;
};

std::array<Finger, 10> g_fingers;
bool g_seen = false;
int g_w = 1, g_h = 1;
std::vector<Sprite> g_sprites;

Finger* find(SDL_FingerID id) {
    for (auto& f : g_fingers)
        if (f.down && f.id == id)
            return &f;
    return nullptr;
}

void finger_down(SDL_FingerID id, float nx, float ny) {
    g_seen = true;
    nj::runtime::gfx::g_last_device = 0;   // comme la souris : la manette ne l'est plus
    for (auto& f : g_fingers)
        if (!f.down && !f.released) {
            f = Finger{};
            f.id = id;
            f.down = true;
            f.x = f.x0 = nx * g_w;
            f.y = f.y0 = ny * g_h;
            f.t0 = SDL_GetTicks();
            return;
        }
}

void finger_move(SDL_FingerID id, float nx, float ny) {
    if (Finger* f = find(id)) {
        f->x = nx * g_w;
        f->y = ny * g_h;
    }
}

void finger_up(SDL_FingerID id) {
    if (Finger* f = find(id)) {
        f->down = false;
        f->released = true;  // traite une fois par update() (tap, balayage), puis libere
    }
}

// ---------------------------------------------------------------------------------------------
// Appuis synthetiques (taps, sequences de menu).
//
// Le jeu lit le front de chaque bouton (Joy.trg) a sa propre cadence, qui n'a rien a voir avec
// celle de la boucle hote et qui peut tomber sous 20 images/s sur un telephone. Un appui est donc
// tenu jusqu'a ce que le jeu ait rendu au moins deux images, puis relache au moins une image :
// sinon un tap bref tombe entre deux lectures et disparait.

struct Pulse {
    std::uint16_t buttons;
    int8_t sx, sy;
};

struct PulseQueue {
    std::deque<Pulse> q;
    bool holding = false, gap = false;
    uint64_t frame0 = 0;
    uint32_t t0 = 0;

    void push(uint16_t b, int8_t sx = 0, int8_t sy = 0) {
        if (q.size() < 32)
            q.push_back({b, sx, sy});
    }
    // Ajoute l'appui courant a la manette et avance la file.
    void apply(uint16_t& buttons, int8_t& sx, int8_t& sy) {
        const uint64_t fr = g_st_frames;
        const uint32_t now = SDL_GetTicks();
        auto elapsed = [&](uint64_t frames, uint32_t min_ms) {
            // Deux conditions : assez d'images vues par le jeu, et un minimum de temps reel au cas
            // ou le compteur d'images serait fige (chargement) - sans quoi on attendrait toujours.
            return (fr - frame0 >= frames && now - t0 >= min_ms) || now - t0 >= 400;
        };
        if (gap) {
            if (!elapsed(1, 34))
                return;
            gap = false;
        }
        if (q.empty())
            return;
        if (!holding) {
            holding = true;
            frame0 = fr;
            t0 = now;
        }
        const Pulse& p = q.front();
        buttons |= p.buttons;
        if (p.sx || p.sy) {
            sx = p.sx;
            sy = p.sy;
        }
        if (elapsed(2, 60)) {
            q.pop_front();
            holding = false;
            gap = true;
            frame0 = fr;
            t0 = now;
        }
    }
};

PulseQueue g_pulses;
struct TurnState {
    int phase = 0;          // 0 inactif, 1 relache B + stick bas, 2 appui B + stick bas
    uint64_t frame0 = 0;    // image invitee de debut
    uint32_t t0 = 0;        // milliseconde de debut
};
static TurnState s_turn;

// ---------------------------------------------------------------------------------------------
// Commandes a l'ecran.

enum Kind : uint8_t {
    KButton,  // maintenu tant que le doigt est dessus
    KTap,     // une impulsion par contact
    KToggle,  // double tap = verrouille ; tap simple pendant le verrou = deverrouille
};

struct Button {
    Icon icon;
    Icon label;       // lettre GameCube affichee en petit (IconDisc = aucune)
    uint16_t pad;
    Kind kind;
    float x, y, r;    // centre et rayon de contact, en pixels
    uint32_t tint;
    bool pressed = false;
    float angle = 0;  // fleches de la croix
    bool flip = false;  // icone en miroir (fleche de rotation vers la droite)
    int cell = -1;      // case d'atlas a la place de l'icone (loupe « - »)
    int book = -1;      // animation d'action de Leon (atlas des icones d'action) a la place de l'icone
    int book2 = -1;     // seconde couche (munitions sous les fleches de rechargement)
    int role = -1;      // bouton de la disposition personnalisable (Role), -1 : aucun
    float bx = 0, by = 0, br = 0;   // place et rayon d'origine, avant la disposition personnalisee
};

struct State {
    // Verrous (double tap), conserves d'une image a l'autre
    bool run_lock = false, aim_lock = false, knife_lock = false;   // couteau verrouillable (14/10)
    uint32_t run_last_tap = 0, aim_last_tap = 0, knife_last_tap = 0;
    uint32_t run_tap_end = 0, aim_tap_end = 0, knife_tap_end = 0;   // fin du dernier appui court (style double tap)
    // 16/10 : un appui court (< 300 ms, sans glisser) bascule le verrou, un appui long est momentane. Doigt tenu = bouton tenu.
    bool run_was_locked = false, aim_was_locked = false, knife_was_locked = false;   // verrou au moment de l'appui
    bool aim_down = false, knife_down = false;   // doigt sur le bouton a l'image precedente (affiche le bouton de tir)
    // Stick flottant
    int stick_finger = -1;
    float sx0 = 0, sy0 = 0;
    // Visee au pouce : doigt de la touche viser qui glisse
    int aim_finger = -1;
    // Regard (C-stick)
    int look_finger = -1;
    int pinch_finger = -1;       // 2e doigt libre aux jumelles : pincer = zoom
    float pinch_d = -1.0f;       // ecart des deux doigts a l'image precedente
    uint32_t shake_last = 0;     // dernier coup de secousse accepte (QTE « SHAKE OFF »)
    Re4Mode last_mode = Re4Mode::Unknown;
};

State g_st;

// ---- disposition personnalisee (19/10) -------------------------------------------------------------------------------------
enum Role { RoleAct, RoleAim, RoleKnife, RoleRun, RoleReload, RoleTurn, RoleCase, RoleMap, RoleAshley, RoleGear, RoleStart, kRoleCount };
const char* const kRoleName[kRoleCount] = {"action", "aim", "knife", "run", "reload", "turn", "case", "map", "ashley", "gear", "start"};
struct LayoutOv { float dx = 0, dy = 0, sc = 1; };
LayoutOv g_ov[kRoleCount];
bool g_ov_active = false;    // layout() : section « jeu » en cours, les reglages s'appliquent
bool g_edit = false, g_edit_snap = true;
int g_edit_sel = -1;
LayoutOv g_edit_start;       // reglage du bouton au debut du glissement
void (*g_edit_done_cb)() = nullptr;
enum EditPad : uint16_t { kEditPlus = 0xE001, kEditMinus = 0xE002, kEditGrid = 0xE003, kEditReset = 0xE004, kEditDone = 0xE005 };

int role_of(Icon icon, uint16_t pad) {
    (void)pad;
    switch (icon) {
    case IconAction: case IconFire: return RoleAct;
    case IconAim: return RoleAim;
    case IconKnife: return RoleKnife;
    case IconRun: return RoleRun;
    case IconReload: return RoleReload;
    case IconTurn: return RoleTurn;
    case IconCase: return RoleCase;
    case IconMap: return RoleMap;
    case IconAshley: return RoleAshley;
    case IconPause: return RoleGear;
    case IconStart: return RoleStart;
    default: return -1;
    }
}

float unit() { return std::min(g_w, g_h) / 9.0f * (nj::runtime::gfx::g_touch_size / 100.0f); }

bool inside(const Button& b, float x, float y) {
    const float dx = x - b.x, dy = y - b.y;
    return dx * dx + dy * dy <= b.r * b.r;
}

// premiere piece du style courant (lentille ; cerclage = +1)
int g_touch_style_base() { return nj::runtime::gfx::g_touch_style == 2 ? 2 : 0; }

uint32_t with_alpha(uint32_t rgb_a, float a) {
    return (rgb_a & 0xFFFFFF00u) | (uint32_t)std::clamp(a * 255.0f, 0.0f, 255.0f);
}

constexpr uint32_t kWhite = 0xFFFFFFFFu, kYellow = 0xFFD84AFFu, kRed = 0xFF5A4AFFu;
constexpr uint32_t kGreen = 0x6CFF7AFFu, kOrange = 0xFFA030FFu, kCyan = 0x6CE4FFFFu;
// Teinte d'un bouton enfonce : le gris des icones vire au jaune pale.
constexpr uint32_t kPressed = 0xFFE9A0FFu;

// ---------------------------------------------------------------------------------------------
// Animation des icones. Rien n'est calcule en 3D ici : les images de Leon sont precalculees dans
// l'atlas (kFlipbooks, choisies par touch_overlay), le reste n'est que position, taille,
// rotation et opacite de tuiles, fonction du temps. Le cout ne depend pas de la cadence du jeu :
// l'animation suit l'horloge, meme si le jeu tourne lentement.

// Bout de l'index dans les icones a main (tools/touch_icons.py, HAND_TIP), en fraction de la
// case, par rapport au centre.
constexpr float kTipDx = 0.44f - 0.5f, kTipDy = 0.23f - 0.5f;

float frac(float x) { return x - std::floor(x); }
float ease(float x) { x = std::clamp(x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }
// bosse 0 -> 1 -> 0 sur [0, 1]
float bump(float x) { return (x <= 0.0f || x >= 1.0f) ? 0.0f : std::sin(x * 3.14159265f); }

uint32_t scale_alpha(uint32_t rgba, float k) {
    const float a = (rgba & 0xFFu) / 255.0f * std::clamp(k, 0.0f, 1.0f);
    return (rgba & 0xFFFFFF00u) | (uint32_t)(a * 255.0f + 0.5f);
}

// Styles pierre / or (13/10) : les anneaux blancs (IconRing) ne sont plus dessines, le cerclage du
// style les remplace (et couvre celui que portent certaines icones de l'atlas).
bool g_styled = false;
// Consigne d'Ashley (Re4Context::ashley_follow) pour son icone : la silhouette de l'etat actuel brille (20/10)
int g_ashley_follow = -1;

void push(Icon icon, float x, float y, float size, uint32_t rgba, float angle = 0.0f, float anim = -1.0f,
          bool flip = false) {
    if ((rgba & 0xFFu) == 0)
        return;
    if (g_styled && icon == IconRing)
        return;
    Sprite s{icon, x, y, size, rgba, angle};
    s.anim = anim;
    s.flip = flip;
    g_sprites.push_back(s);
}

// Main affichee dans les icones a main. L'atlas porte la main gauche de Leon (os 15/16, le jeu
// nomme ses mains a l'envers) ; la droite est la meme case lue en miroir. On change de main a
// chaque nouvel evenement (invite d'action, QTE), voir TouchSource::update.
bool g_hand_right = true;

// Couche d'effet mise a l'echelle k autour du bout de l'index (et non du centre de la case),
// en miroir avec la main.
void push_fx(Icon fx, float x, float y, float size, uint32_t rgba, float k, float dx = 0, float dy = 0) {
    const float sx = g_hand_right ? -1.0f : 1.0f;
    const float ox = sx * kTipDx * size, oy = kTipDy * size;
    push(fx, x + sx * dx + ox - ox * k, y + dy + oy - oy * k, size * k, rgba, 0.0f, -1.0f, g_hand_right);
}

void push_hand(float x, float y, float size, uint32_t rgba) {
    push(IconHand, x, y, size, rgba, 0.0f, -1.0f, g_hand_right);
}

void emit_icon(Icon icon, float x, float y, float size, uint32_t rgba, float angle, float t,
               bool pressed, bool fast) {
    // dephasage propre a chaque icone : les boutons ne bougent pas tous en meme temps
    const float seed = (float)icon * 0.37f;
    const float sx = g_hand_right ? -1.0f : 1.0f;  // sens de la main (miroir)
    switch (icon) {
    case IconAction: {
        // l'index tape (la main descend puis remonte) et trois traits d'impact jaillissent
        const float p = pressed ? 0.12f : frac(t / 1.8f + seed);
        const float press = pressed ? 1.0f : bump(p / 0.24f);
        push(IconRing, x, y, size, rgba);
        push_hand(x, y + size * 0.035f * press, size, rgba);
        const float q = (p - 0.10f) / 0.40f;
        if (pressed || (q > 0 && q < 1))
            push_fx(IconFxRays3, x, y, size, scale_alpha(rgba, pressed ? 1.0f : 1.0f - ease(q)),
                    pressed ? 1.05f : 0.85f + 0.35f * ease(q));
        return;
    }
    case IconTap: {
        // toucher : l'index tape en rythme, deux ondes s'elargissent et s'effacent
        const float p = frac(t / 0.9f);
        push(IconRing, x, y, size, rgba);
        push_hand(x, y + size * 0.03f * bump(p / 0.25f), size, rgba);
        for (int k = 0; k < 2; ++k) {
            const float q = frac(p + k * 0.5f);
            push_fx(IconFxWaves, x, y, size, scale_alpha(rgba, 1.0f - q), 0.6f + 0.8f * q);
        }
        return;
    }
    case IconSwipe: {
        // glisser : la main traverse le bouton (vers l'exterieur de la main), la fleche s'allume
        const float p = frac(t / 1.3f);
        const float m = ease(p / 0.6f);
        const float fade = p < 0.6f ? 1.0f : 1.0f - ease((p - 0.6f) / 0.3f);
        push(IconRing, x, y, size, rgba);
        push_hand(x + sx * size * (-0.12f + 0.24f * m), y, size, scale_alpha(rgba, std::max(fade, 0.25f)));
        push(IconFxSwipe, x, y, size, scale_alpha(rgba, 0.35f + 0.65f * bump(p / 0.7f)), 0.0f, -1.0f,
             g_hand_right);
        return;
    }
    case IconMash: {
        // marteler : l'index tape tres vite, les eclats clignotent, « x5 » bat la mesure
        const float p = frac(t / 0.22f);
        push(IconRing, x, y, size, rgba);
        push_hand(x, y + size * 0.03f * bump(p / 0.5f), size, rgba);
        push_fx(IconFxRays5, x, y, size, scale_alpha(rgba, 0.4f + 0.6f * bump(p)), 0.9f + 0.2f * bump(p));
        // « x5 » bat sur place, du cote oppose a la main ; jamais en miroir (le texte resterait a
        // l'envers) : son centre est en (0,73 ; 0,50) de la case, on le deplace a (0,27 ; 0,50)
        const float k = 1.0f + 0.12f * bump(frac(t / 0.44f)), ox = 0.23f * size;
        const float cx = x + (g_hand_right ? -0.46f * size : 0.0f);
        push(IconFxX5, cx + ox - ox * k, y, size * k, rgba);
        return;
    }
    case IconRun:
        // Leon qui court (images precalculees, kFlipbooks) ; plus vite quand la course est bloquee
        push(icon, x, y, size, rgba, angle, t * (fast ? 1.35f : 1.0f));
        return;
    case IconFire: {
        // tir : recul (le dessin saute vers le haut en pivotant) et eclair de bouche au bout de
        // la balle, toutes les 1,6 s ; en rafale si appuye
        const float p = frac(t / (pressed ? 0.25f : 1.6f) + seed);
        const float k = p < 0.5f ? std::exp(-p * 14.0f) : 0.0f;
        push(IconRing, x, y, size, rgba);
        push(icon, x, y - size * 0.04f * k, size * (1.0f + 0.05f * k), rgba, angle - 0.18f * k);
        if (p < 0.12f)
            push_fx(IconFxRays3, x, y, size, scale_alpha(rgba, 1.0f - p / 0.12f), 0.8f + 2.0f * p);
        return;
    }
    case IconReload: {
        // recharger : les fleches font un tour complet, lisse (en boucle si appuye)
        const float p = frac(t / (pressed ? 0.7f : 2.4f) + seed);
        const float turn = pressed ? p : ease(p / 0.3f);
        push(IconRing, x, y, size, rgba);
        push(icon, x, y, size, rgba, angle + turn * 6.2831853f);
        return;
    }
    case IconKnife: {
        // couteau : la lame arme (-0,3 rad), tranche (+0,35) et revient
        const float p = frac(t / 2.2f + seed);
        const float q = pressed ? frac(t / 0.35f) : p / 0.16f;
        float a = 0.0f;
        if (q > 0 && q < 0.3f)
            a = -0.3f * ease(q / 0.3f);
        else if (q >= 0.3f && q < 0.55f)
            a = -0.3f + 0.65f * ease((q - 0.3f) / 0.25f);
        else if (q >= 0.55f && q < 1)
            a = 0.35f * (1.0f - ease((q - 0.55f) / 0.45f));
        push(IconRing, x, y, size, rgba);
        push(icon, x, y, size, rgba, angle + a);
        return;
    }
    case IconTurn: {
        // demi-tour : la fleche fait un tour complet (le « 180 » revient a l'endroit)
        const float p = frac(t / 3.0f + seed);
        push(IconRing, x, y, size, rgba);
        push(icon, x, y, size, rgba, angle - 6.2831853f * ease(p / 0.35f));
        return;
    }
    case IconAim: {
        // viser : le viseur arrive large et se resserre sur la cible, avec un leger rebond
        const float p = frac(t / (pressed ? 0.8f : 2.4f) + seed);
        const float q = p / 0.45f;
        const float k = q < 1 ? 1.0f + 0.14f * (1.0f - ease(q)) - 0.04f * bump(q) : 1.0f;
        push(IconRing, x, y, size, rgba);
        push(icon, x, y, size * k, rgba, angle);
        return;
    }
    case IconCase: {
        // mallette : petit saut, puis elle se pose en oscillant
        const float p = frac(t / 2.6f + seed);
        const float hop = bump(p / 0.22f);
        const float q = (p - 0.22f) / 0.3f;
        const float wob = (q > 0 && q < 1) ? 0.10f * std::sin(q * 18.0f) * (1.0f - q) : 0.0f;
        push(IconRing, x, y, size, rgba);
        push(icon, x, y - size * 0.06f * hop, size, rgba, angle + wob);
        return;
    }
    case IconMap: {
        // carte : double rebond, et une onde « ping » part du repere
        const float p = frac(t / 2.8f + seed);
        const float b = bump(p / 0.14f) + 0.45f * bump((p - 0.14f) / 0.10f);
        push(IconRing, x, y, size, rgba);
        push(icon, x, y - size * 0.035f * b, size, rgba, angle);
        const float q = (p - 0.05f) / 0.5f;
        if (q > 0 && q < 1)  // repere de la carte en (0,50 ; 0,36) ; les ondes sont dessinees en (0,44 ; 0,23)
            push_fx(IconFxWaves, x, y, size, scale_alpha(rgba, 1.0f - q), 0.6f + 0.7f * q,
                    size * 0.06f, size * 0.13f);
        return;
    }
    case IconAshley: {
        // appeler Ashley : ondes d'appel au-dessus des tetes, les silhouettes se redressent
        const float p = frac(t / 2.2f + seed);
        push(IconRing, x, y, size, rgba);
        const float yb = y - size * 0.02f * bump(p / 0.3f);
        if (g_ashley_follow < 0) {
            push(icon, x, yb, size, rgba, angle);
        } else {
            // Les deux silhouettes se tiennent la main : on ne les decoupe pas. La consigne en cours passe devant (a gauche) en
            // retournant toute l'icone en miroir : normale = elle attend (silhouette « attends » a gauche), miroir = elle suit.
            // Fondu de 0,25 s entre les deux.
            static float swap = -1.0f, last_t = 0.0f;
            const float want = g_ashley_follow ? 1.0f : 0.0f;
            if (swap < 0) swap = want;
            swap += std::clamp(want - swap, -(t - last_t) * 4.0f, (t - last_t) * 4.0f);
            last_t = t;
            const float k = ease(std::clamp(swap, 0.0f, 1.0f));
            if (k < 1.0f) push(icon, x, yb, size, scale_alpha(rgba, 1.0f - k), angle);
            if (k > 0.0f) push(icon, x, yb, size, scale_alpha(rgba, k), angle, -1.0f, true);
        }
        for (int k = 0; k < 2; ++k) {
            const float q = (p - k * 0.18f) / 0.55f;
            if (q > 0 && q < 1)
                push_fx(IconFxWaves, x, y, size, scale_alpha(rgba, 1.0f - q), 0.5f + 0.8f * q,
                        size * 0.02f, size * 0.02f);
        }
        return;
    }
    case IconKick: {
        // coup de pied : la botte part vers l'avant en pivotant, puis revient
        const float p = frac(t / 2.0f + seed);
        const float q = pressed ? 0.3f : p / 0.35f;
        const float k = q < 0.3f ? ease(q / 0.3f) : (q < 1 ? 1.0f - ease((q - 0.3f) / 0.7f) : 0.0f);
        push(IconRing, x, y, size, rgba);
        push(icon, x + size * 0.05f * k, y, size, rgba, angle + 0.3f * k);
        return;
    }
    case IconGrenade: {
        // grenade : balancement de pendule, la goupille prete a partir
        push(IconRing, x, y, size, rgba);
        push(icon, x, y, size, rgba, angle + 0.16f * std::sin(t * 5.2f + seed));
        return;
    }
    case IconSkip: {
        // passer : les fleches filent vers la droite et reviennent
        const float p = frac(t / 1.6f + seed);
        push(IconRing, x, y, size, rgba);
        push(icon, x + size * 0.06f * bump(p / 0.35f), y, size, rgba, angle);
        return;
    }
    case IconZoom: {
        // loupe : grossit comme si elle zoomait, puis revient
        const float p = frac(t / 2.2f + seed);
        push(IconRing, x, y, size, rgba);
        push(icon, x, y, size * (1.0f + 0.14f * bump(p / 0.4f)), rgba, angle);
        return;
    }
    case IconTalk: {
        // parler : la bulle apparait d'un coup (petit « pop ») puis se pose
        const float p = frac(t / 2.4f + seed);
        const float q = p / 0.3f;
        const float k = q < 1 ? 1.0f + 0.12f * std::sin(q * 9.4f) * (1.0f - q) : 1.0f;
        push(IconRing, x, y, size, rgba);
        push(icon, x, y, size * k, rgba, angle);
        return;
    }
    case IconLook: {
        // regarder : l'oeil cligne (bref fondu), deux fois de suite
        const float p = frac(t / 3.2f + seed);
        const float blink = bump(p / 0.06f) + bump((p - 0.1f) / 0.06f);
        push(IconRing, x, y, size, rgba);
        push(icon, x, y, size, scale_alpha(rgba, 1.0f - 0.8f * std::min(blink, 1.0f)), angle);
        return;
    }
    case IconBack: {
        // retour : la fleche recule d'un quart de tour et revient
        const float p = frac(t / 2.4f + seed);
        push(IconRing, x, y, size, rgba);
        push(icon, x, y, size, rgba, angle - 0.6f * bump(p / 0.35f));
        return;
    }
    default:
        push(icon, x, y, size, rgba, angle);
        return;
    }
}

Icon pad_label(uint16_t pad) {
    switch (pad) {
    case kPadA: return IconA;
    case kPadB: return IconB;
    case kPadX: return IconX;
    case kPadY: return IconY;
    case kPadL: return IconL;
    case kPadR: return IconR;
    case kPadZ: return IconZ;
    case kPadStart: return IconStart;
    default: return IconDisc;
    }
}

// Commandes visibles pour le contexte courant. Refaites a chaque image : la liste depend de ce
// que fait le jeu (viser change A en « tirer », un menu remplace l'action par valider/annuler).
// Texte qui attend A pendant le jeu ou une cinematique (examen « He's not a zombie », cheminee...).
bool text_only(const Re4Context& ctx) {
    // une invite d'action (prendre, examiner, sauter par la fenetre) affiche aussi un texte : c'est le
    // jeu normal, avec la main de Leon (retour du 14/10)
    return ctx.text_wait && ctx.qte == Re4Qte::None && ctx.choice_count == 0 && ctx.prompt == 0 &&
           (ctx.mode == Re4Mode::Gameplay || ctx.mode == Re4Mode::Event);
}

// Situation vue par le tactile, ecrite dans le journal a chaque changement (14/10) : les essais et
// les rapports des joueurs disent quel ecran a recu quelle disposition, sans capture d'ecran.
const char* situation(const Re4Context& ctx) {
    if (text_only(ctx)) return "texte (A)";
    if (ctx.qte != Re4Qte::None && (ctx.mode == Re4Mode::Gameplay || ctx.mode == Re4Mode::Event)) return "QTE";
    if (ctx.death) return ctx.death_continue ? "mort : Continue Oui/Non" : "mort";
    if (ctx.binoculars) return "jumelles";
    if (ctx.card) return "carte memoire";
    if (ctx.map) return "carte du jeu";
    if (ctx.inventory) return ctx.grid ? "mallette (grille)" : "mallette";
    if (ctx.choice_count > 0) return "choix Oui/Non";
    switch (ctx.mode) {
    case Re4Mode::Gameplay: return ctx.aiming ? "jeu : visee" : ctx.prompt ? "jeu : action proposee" : "jeu";
    case Re4Mode::Event: return ctx.skippable ? "cinematique passable" : "cinematique non passable";
    case Re4Mode::Movie: return "video";
    case Re4Mode::SubScreen: return "sous-ecran";
    case Re4Mode::Shop: return "marchand";
    case Re4Mode::Menu: return ctx.title_menu ? "titre" : "menu";
    default: return "inconnu";
    }
}

// Icone de l'invite d'action selon son type (ActBtn.kind, libelles de etc/core.das table 6 : 2 JUMP OUT, 7 KICK, 8 CLIMB UP...).
// Rend -1 : la main du gant (examiner, parler, ramasser...).
int action_book(int kind) {
    const char* n = nullptr;
    switch (kind) {
    case 2: case 3: case 5: case 0xC: case 0x12: case 0x1A: case 0x3E: n = "jump_window"; break;   // sauter par la fenetre, par-dessus
    case 4: case 0x1B: case 0x40: n = "jump_down"; break;                                       // sauter en bas, glisser
    case 8: case 0x19: n = "climb_up"; break;
    case 9: n = "climb_down"; break;
    case 6: n = "push"; break;
    case 7: case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: n = "kick"; break; // coups de pied et prises sur un ennemi
    case 0x10: n = "open"; break;
    case 0xF: case 0x13: case 0x20: case 0x26: n = "crouch"; break;                               // se plaquer, s'accroupir, se cacher
    case 0x21: case 0x22: case 0x1E: n = "whistle"; break;                                        // suis-moi, attends
    default: return -1;
    }
    return nj::runtime::input::touch::overlay_action_book(n);
}

// Icone de munition du HUD pour l'arme en main : le groupe du jeu (cockpit.cpp dispBulletIconMarkNo, marques 0x31..0x3A) donne
// la balle, la cartouche, le missile... Textures du HUD : tools/touch_hud_textures.py.
const char* ammo_name(int wep) {
    switch (wep) {
    case 1: case 2: case 3: case 4: case 5: case 6: case 0xB: case 0xC: case 0xF: case 0x11: return "g_am_pistol";   // 0x31
    case 7: case 8: case 0x21: return "g_am_shell";                                                              // 0x37
    case 9: case 0xA: case 0x1D: case 0x15: case 0x19: case 0x1F: case 0x20: return "g_am_rifle";                  // 0x36, 0x33, 0x3A
    case 0xD: return "g_am_missile";                                                                             // 0x38
    case 0xE: return "g_am_dart";                                                                                // 0x39
    case 0x13: case 0x16: case 0x17: return "g_am_grenade";                                                      // 0x32
    }
    return nullptr;
}

// Couteau de l'icone : Leon echange son couteau avec Krauser a la fin du duel du chapitre 5-3 (le jeu ne le montre pas ; retour du
// 17/10). Le chapitre 5-3 est le 15e : 14 chapitres termines pendant qu'on le joue, 15 apres. Pendant le 5-3, la piece du couloir
// aux lasers (apres le duel) n'est pas encore relevee : le journal ecrit « room rXXX chap=N » a chaque piece pour la trouver.
const char* knife_icon(const Re4Context& ctx) {
    // Leon : l'objet 0x38 comme avant (la tentative du 21/10 avec wep16 donnait un etui vu par la tranche) ; Krauser : wep26 rendu
    // chez le joueur (icon_gen.cpp), meme vue que l'ancienne icone w_krauser.
    return ctx.chapters_ended >= 15 ? "w_it126" : "w_it38";
}

std::vector<Button> layout(const Re4Context& ctx) {
    const float u = unit(), w = (float)g_w, h = (float)g_h;
    std::vector<Button> b;
    g_ov_active = false;
    auto add = [&](Icon icon, uint16_t pad, Kind k, float x, float y, float size, uint32_t tint) {
        b.push_back({icon, pad_label(pad), pad, k, x, y, size * 0.5f, tint});
        Button& nb = b.back();
        nb.bx = x;
        nb.by = y;
        nb.br = nb.r;
        if (g_ov_active)
            if (const int rl = role_of(icon, pad); rl >= 0) {
                nb.role = rl;
                nb.x = x + g_ov[rl].dx * w;
                nb.y = y + g_ov[rl].dy * h;
                nb.r *= g_ov[rl].sc;
            }
    };
    const bool menu = ctx.mode == Re4Mode::SubScreen || ctx.mode == Re4Mode::Shop ||
                      ctx.mode == Re4Mode::Menu || ctx.mode == Re4Mode::Unknown;
    const bool cine = ctx.mode == Re4Mode::Event || ctx.mode == Re4Mode::Movie;
    auto gear = [&] { add(IconPause, 0, KTap, 1.1f * u, 1.1f * u, 1.1f * u, kWhite); };
    // QTE (16/10) : le grand bouton d'action en bas a droite devient l'icone du geste (taper, balayer, secouer le telephone),
    // dans le style choisi ; plus d'icone au milieu de l'image, sur le texte et la jauge du jeu. Toucher le bouton ou n'importe
    // ou ailleurs repond au QTE (les gestes de poll()), le telephone secoue aussi.
    auto qte_button = [&] {
        const uint16_t pad = ctx.prompt_pad ? ctx.prompt_pad : (ctx.qte == Re4Qte::Pair ? (uint16_t)(kPadL | kPadR) : kPadA);
        const Icon ic = ctx.qte == Re4Qte::Pair ? IconSwipe : ctx.qte == Re4Qte::Mash ? IconMash : IconTap;
        add(ic, pad, KTap, w - 1.8f * u, h - 1.8f * u, 2.1f * u, kPressed);
        b.back().label = pad & (kPadB | kPadX | kPadY) && ctx.qte == Re4Qte::Single ? pad_label(pad) : IconDisc;
        if (ctx.prompt_kind == 0x2B)
            b.back().book = nj::runtime::input::touch::overlay_action_book("g_phone");
    };
    // Texte qui attend A (examen, dialogue), hors QTE : un seul bouton « suivant » (14/10).
    if (text_only(ctx)) {
        add(IconAction, kPadA, KTap, w - 1.8f * u, h - 1.8f * u, 2.1f * u, kWhite);
        gear();
        return b;
    }

    if (cine) {
        if (ctx.qte != Re4Qte::None) {
            qte_button();   // QTE pendant une cinematique : tout l'ecran sert au geste
            return b;
        }
        // Passer seulement si le jeu l'accepte (kSkip = Start).
        if (ctx.mode == Re4Mode::Movie || ctx.skippable)
            add(IconSkip, kSkip, KTap, w - 1.4f * u, 1.1f * u, 1.5f * u, kWhite);
        return b;
    }
    // Mort : « Continue? » Oui (gauche puis A) / Non (droite puis A) ; avant, rien (toucher = A).
    if (ctx.death) {
        if (ctx.death_continue) {
            add(IconCheck, kDeathYes, KTap, w * 0.5f - 2.2f * u, h - 1.15f * u, 2.0f * u, kGreen);
            add(IconCross, kDeathNo, KTap, w * 0.5f + 2.2f * u, h - 1.15f * u, 2.0f * u, kRed);
        }
        return b;
    }
    // Carte memoire (machine a ecrire, charger) : croix, valider, annuler.
    // Mallette : valider, annuler, X, Y, L, R ; la croix seulement hors de la grille (toucher la case).
    if (ctx.card || ctx.inventory || ctx.map) {
        if (ctx.map)
            add(IconAction, kPadA, KButton, w - 1.8f * u, h - 1.8f * u, 2.1f * u, kWhite);   // la main de Leon
        else
            add(IconCheck, kPadA, KButton, w - 1.6f * u, h - 1.9f * u, 1.8f * u, kGreen);
        add(IconCross, kPadB, KButton, w - 3.6f * u, h - 1.1f * u, 1.4f * u, kRed);
        {   // croix toujours la (14/10 : le toucher de la grille est retire)
            const float dcx = 2.6f * u, dcy = h - 2.6f * u, dd = 1.05f * u;
            const struct { uint16_t pad; float dx, dy, ang; } arrows[4] = {
                {kPadUp, 0, -1, 0.0f}, {kPadRight, 1, 0, 1.5707963f},
                {kPadDown, 0, 1, 3.1415927f}, {kPadLeft, -1, 0, -1.5707963f}};
            for (const auto& a : arrows) {
                add(IconArrow, a.pad, KTap, dcx + a.dx * dd, dcy + a.dy * dd, 1.1f * u, kWhite);
                b.back().label = IconDisc;
                b.back().angle = a.ang;
            }
        }
        if (ctx.inventory) {
            add(IconX, kPadX, KButton, w - 1.2f * u, h - 4.0f * u, 1.1f * u, kWhite);
            add(IconCase, kPadY, KButton, w - 2.6f * u, h - 3.5f * u, 1.1f * u, kWhite);   // Y = mallette
            {   // 20/10 : la meme mallette qu'en jeu (niveau S, M, L, LL), plus l'ancienne icone dessinee
                char cn[16];
                std::snprintf(cn, sizeof cn, "c_case%d", std::clamp(ctx.case_level, 0, 3));
                b.back().book = overlay_action_book(cn);
            }
            // L / R tournent l'objet : fleches de rotation
            add(IconBack, kPadL, KButton, 1.0f * u, 1.1f * u, 1.1f * u, kWhite);
            b.back().label = IconDisc;
            add(IconBack, kPadR, KButton, w - 1.0f * u, 1.1f * u, 1.1f * u, kWhite);
            b.back().label = IconDisc;
            b.back().flip = true;
            add(IconPause, 0, KTap, 2.4f * u, 1.1f * u, 1.1f * u, kWhite);
        } else if (ctx.map) {
            // carte : L / R = dezoomer / zoomer ; Y ne sert pas
            add(IconX, kPadX, KButton, w - 1.2f * u, h - 4.0f * u, 1.1f * u, kWhite);
            add(IconZoom, kPadL, KButton, 1.0f * u, 1.1f * u, 1.1f * u, kWhite);
            b.back().label = IconDisc;
            b.back().cell = overlay_zoom_out_cell();
            add(IconZoom, kPadR, KButton, w - 1.0f * u, 1.1f * u, 1.1f * u, kWhite);
            b.back().label = IconDisc;
            add(IconPause, 0, KTap, 2.4f * u, 1.1f * u, 1.1f * u, kWhite);
        } else {
            gear();
        }
        return b;
    }
    if (menu) {
        const bool title = ctx.title_menu;   // titre : ni L, R, X, Y, ni Start (14/10)
        add(IconCheck, kPadA, KButton, w - 1.6f * u, h - 1.9f * u, 1.8f * u, kGreen);
        add(IconCross, kPadB, KButton, w - 3.6f * u, h - 1.1f * u, 1.4f * u, kRed);
        // Croix : quatre fleches en bas a gauche (les menus se jouent aussi au doigt, voir
        // menu_tap ; la croix sert aux ecrans sans texte a toucher, options, titre).
        const float dcx = 2.6f * u, dcy = h - 2.6f * u, dd = 1.05f * u;
        const struct { uint16_t pad; float dx, dy, ang; } arrows[4] = {
            {kPadUp, 0, -1, 0.0f}, {kPadRight, 1, 0, 1.5707963f},
            {kPadDown, 0, 1, 3.1415927f}, {kPadLeft, -1, 0, -1.5707963f}};
        for (const auto& a : arrows) {
            add(IconArrow, a.pad, KTap, dcx + a.dx * dd, dcy + a.dy * dd, 1.1f * u, kWhite);
            b.back().label = IconDisc;
            b.back().angle = a.ang;
        }
        if (title) {
            gear();
            return b;
        }
        add(IconX, kPadX, KButton, w - 1.2f * u, h - 4.0f * u, 1.1f * u, kWhite);
        add(IconY, kPadY, KButton, w - 2.6f * u, h - 3.5f * u, 1.1f * u, kWhite);
        add(IconL, kPadL, KButton, 1.0f * u, 1.1f * u, 1.1f * u, kWhite);
        add(IconR, kPadR, KButton, w - 1.0f * u, 1.1f * u, 1.1f * u, kWhite);
        add(IconStart, kPadStart, KTap, w * 0.5f, 0.8f * u, 1.1f * u, kWhite);
        add(IconPause, 0, KTap, 2.4f * u, 1.1f * u, 1.1f * u, kWhite);  // parametres
        return b;
    }

    // Jumelles (16/10) : glisser = regarder (stick), pincer ou boutons +/- = zoom, croix = baisser. Rien d'autre.
    if (ctx.binoculars) {
        add(IconCross, kBinoExit, KTap, w - 1.8f * u, h - 1.8f * u, 1.8f * u, kRed);
        add(IconZoom, kZoomIn, KButton, w - 1.4f * u, h - 6.0f * u, 1.3f * u, kWhite);
        add(IconZoom, kZoomOut, KButton, w - 1.4f * u, h - 4.4f * u, 1.3f * u, kWhite);
        b.back().cell = overlay_zoom_out_cell();
        add(IconPause, 0, KTap, 1.1f * u, 1.1f * u, 1.1f * u, kWhite);
        return b;
    }

    // Jeu
    g_ov_active = true;   // seuls les boutons de jeu suivent la disposition personnalisee
    const bool prompt = ctx.prompt != 0 && ctx.qte == Re4Qte::None;
    const bool qte_on = ctx.qte != Re4Qte::None;
    // Tir / coup de couteau : des que R (viser) ou L (couteau) est engage, meme si le jeu ne dit pas « en visee »
    // (le couteau n'est pas une arme en main pour pl+0xFD) : sinon, avec « Action button : seulement si besoin », plus de bouton A.
    const bool armed = ctx.aiming || g_st.aim_lock || g_st.knife_lock || g_st.aim_down || g_st.knife_down;
    if (qte_on) {
        qte_button();
    } else if (armed && !prompt) {
        add(IconFire, kAction, KButton, w - 1.8f * u, h - 1.8f * u, 2.1f * u, kWhite);
        // tir : les munitions de l'arme ; avec le couteau, le couteau
        if (g_st.knife_lock || g_st.knife_down)
            b.back().book = overlay_action_book(knife_icon(ctx));
        else if (const char* am = ammo_name(ctx.wep_no))
            b.back().book = overlay_action_book(am);
    } else if (prompt || nj::runtime::gfx::g_touch_action == 0) {   // Controls > Action button (14/10)
        add(IconAction, kAction, KButton, w - 1.8f * u, h - 1.8f * u, 2.1f * u, prompt ? kPressed : kWhite);
        if (prompt) {
            b.back().book = ctx.prompt_kind == 0x2F ? overlay_action_book("g_save") : action_book(ctx.prompt_kind);
        }
    }
    const bool simple = nj::runtime::gfx::g_touch_simple != 0;
    if (!simple)
        add(IconRun, kRun, KToggle, w - 4.1f * u, h - 1.2f * u, 1.5f * u, g_st.run_lock ? kPressed : kWhite);
    add(IconAim, kAim, KToggle, w - 1.4f * u, h - 4.3f * u, 1.6f * u,
        (ctx.aiming || g_st.aim_lock) ? kPressed : kWhite);
    {   // viser : le modele de l'arme en main, accessoire compris (silencieux, lunette...), a la place du viseur
        char nm[16];
        std::snprintf(nm, sizeof nm, "w_it%02x", ctx.wep_item < 0 ? 0 : ctx.wep_item);
        b.back().book = overlay_action_book(nm);
        if (b.back().book < 0)   // arme sans modele (grenades, lance-roquettes...) : ses munitions, sinon le viseur
            if (const char* am = ammo_name(ctx.wep_no)) b.back().book = overlay_action_book(am);
        if (b.back().book < 0)
            b.back().book = overlay_action_book("g_aim");
    }
    add(IconKnife, kKnife, KToggle, w - 3.6f * u, h - 3.5f * u, 1.3f * u, g_st.knife_lock ? kPressed : kWhite);
    b.back().book = overlay_action_book(knife_icon(ctx));
    if (ctx.aiming) {
        add(IconReload, kReload, KTap, w - 5.9f * u, h - 1.2f * u, 1.3f * u, kWhite);  // R tenu + B
        b.back().book = overlay_action_book("g_reload");
        if (const char* am = ammo_name(ctx.wep_no))
            b.back().book2 = overlay_action_book(am);
    } else if (!simple)
        add(IconTurn, 0, KTap, w - 5.9f * u, h - 1.2f * u, 1.3f * u, kWhite);
    add(IconCase, kInventory, KTap, w - 1.2f * u, 1.1f * u, 1.3f * u, kWhite);
    {   // la mallette montre son niveau (S, M, L, LL ; la marchande l'ameliore), comme dans le menu du marchand
        char cn[16];
        std::snprintf(cn, sizeof cn, "c_case%d", std::clamp(ctx.case_level, 0, 3));
        b.back().book = overlay_action_book(cn);
    }
    add(IconMap, kMap, KTap, w - 2.8f * u, 1.1f * u, 1.3f * u, kWhite);
    b.back().book = overlay_action_book("c_map");
    g_ashley_follow = ctx.ashley_follow;
    if (ctx.ashley) {   // seulement quand Ashley est la (pSUB, type 3)
        static int last = -2;
        if (ctx.ashley_follow != last) {   // journal : verifier en jeu le sens de l'invite (Wait = elle suit)
            last = ctx.ashley_follow;
            char jb[48];
            std::snprintf(jb, sizeof jb, "tactile : ashley suit=%d", ctx.ashley_follow);
            nj::perf::event(jb);
        }
        add(IconAshley, kAshley, KTap, w - 4.4f * u, 1.1f * u, 1.3f * u, kWhite);  // appeler Ashley (sifflet, X)
        b.back().book = overlay_action_book("whistle");
    }
    add(IconPause, 0, KTap, 1.1f * u, 1.1f * u, 1.1f * u, kWhite);  // parametres (resolution...)
    add(IconStart, kPadStart, KTap, 2.4f * u, 1.1f * u, 1.1f * u, kWhite);  // menu pause du jeu
    for (Button& x : b)   // plus de petite lettre de manette sur les boutons de jeu (retour du 16/10)
        x.label = IconDisc;
    return b;
}

// Mode d'edition : tous les boutons de jeu (meme ceux que le contexte cache : Ashley, recharger, demi-tour) et la barre d'outils.
std::vector<Button> edit_layout() {
    Re4Context e;
    e.valid = true;
    e.mode = Re4Mode::Gameplay;
    e.ashley = true;
    const int keep_action = nj::runtime::gfx::g_touch_action;
    nj::runtime::gfx::g_touch_action = 0;
    std::vector<Button> out = layout(e);
    e.aiming = true;   // le bouton de recharge n'existe qu'en visee
    for (const Button& x : layout(e)) {
        bool dup = false;
        for (const Button& y : out)
            dup |= x.role >= 0 && y.role == x.role;
        if (!dup && x.role == RoleReload)
            out.push_back(x);
    }
    nj::runtime::gfx::g_touch_action = keep_action;
    for (Button& x : out) {
        x.pressed = false;
        x.label = IconDisc;
    }
    const float u = unit(), w = (float)g_w;
    auto tool = [&](Icon ic, EditPad p, float dx, uint32_t tint) {
        out.push_back({ic, IconDisc, p, KTap, w * 0.5f + dx * u, 1.4f * u, 0.65f * u, tint});
        return &out.back();
    };
    tool(IconZoom, kEditMinus, -3.0f, kWhite)->cell = overlay_zoom_out_cell();
    tool(IconZoom, kEditPlus, -1.5f, kWhite);
    tool(IconDisc, kEditGrid, 0.0f, g_edit_snap ? kGreen : kWhite);
    tool(IconCross, kEditReset, 1.5f, kRed);
    tool(IconCheck, kEditDone, 3.0f, kGreen);
    return out;
}

void edit_toolbar(uint16_t pad) {
    if (pad == kEditDone) {
        g_edit = false;
        g_edit_sel = -1;
        if (g_edit_done_cb) g_edit_done_cb();
    } else if (pad == kEditGrid) {
        g_edit_snap = !g_edit_snap;
    } else if (pad == kEditReset) {
        if (g_edit_sel >= 0) g_ov[g_edit_sel] = LayoutOv{};
        else for (LayoutOv& o : g_ov) o = LayoutOv{};
    } else if (g_edit_sel >= 0) {
        LayoutOv& o = g_ov[g_edit_sel];
        o.sc = std::clamp(o.sc * (pad == kEditPlus ? 1.1f : 1.0f / 1.1f), 0.5f, 2.2f);
    }
}

// Glissement d'un bouton : la place visee suit le doigt, alignee sur la grille (un demi-bouton) si elle est active.
void edit_drag(const Button& b, const Finger& f) {
    LayoutOv& o = g_ov[b.role];
    const float w = (float)g_w, h = (float)g_h;
    float px = b.bx + g_edit_start.dx * w + (f.x - f.x0);
    float py = b.by + g_edit_start.dy * h + (f.y - f.y0);
    if (g_edit_snap) {
        const float gs = std::max(8.0f, unit() * 0.5f);
        px = std::round(px / gs) * gs;
        py = std::round(py / gs) * gs;
    }
    px = std::clamp(px, b.r, w - b.r);
    py = std::clamp(py, b.r, h - b.r);
    o.dx = (px - b.bx) / w;
    o.dy = (py - b.by) / h;
}

// ---- mallette au doigt (19/10) ---------------------------------------------------------------------------------------------
// Case (en cases reelles, centre de la case (0,0) = 0) de la planche touchee par un point de l'ecran ; strict : seulement a l'interieur.
bool case_at(const Re4Context& ctx, float x, float y, int& board, float& fi, float& fj, bool strict) {
    const Rect gr = game_rect(g_w, g_h);
    const float gx = (x - gr.x) / gr.w * 640.0f, gy = (y - gr.y) / gr.h * 480.0f;
    float best = 1e9f;
    bool found = false;
    for (int k = 0; k < 2; ++k) {
        const Re4Context::Grid& G = ctx.grids[k];
        if (!G.valid)
            continue;
        const float det = G.ax[0] * G.ay[1] - G.ax[1] * G.ay[0];
        if (std::fabs(det) < 1e-3f)
            continue;
        const float dx = gx - G.origin[0], dy = gy - G.origin[1];
        const float i = (dx * G.ay[1] - dy * G.ay[0]) / det, j = (G.ax[0] * dy - G.ax[1] * dx) / det;
        const float over = std::max({-0.5f - i, i - (G.w - 0.5f), -0.5f - j, j - (G.h - 0.5f), 0.0f});
        if (strict && over > 0.0f)
            continue;
        if (over < best) {
            best = over;
            board = k;
            fi = i;
            fj = j;
            found = true;
        }
    }
    return found;
}

struct CaseGesture { bool picked = false; } g_case;

// Un doigt pose sur une grille : appui long = prendre la piece (curseur sur la case, puis X) ; piece en main = elle suit le doigt, relacher
// pose (A) ; un appui court sur une case place le curseur puis valide (A). Un second doigt tourne la piece (R).
void case_gesture(const Re4Context& ctx, const Finger& f, uint32_t now) {
    const float u = unit();
    const float dist = std::hypot(f.x - f.x0, f.y - f.y0);
    if (f.down) {
        if (!ctx.hand && !g_case.picked && now - f.t0 > 220 && dist < 0.5f * u) {
            int bd = 0;
            float fi = 0, fj = 0;
            if (case_at(ctx, f.x0, f.y0, bd, fi, fj, true)) {
                re4_case_set_cursor(bd, (int)std::lround(fi), (int)std::lround(fj));
                g_pulses.push(kPadX);
                g_case.picked = true;
            }
        }
        if (ctx.hand) {
            int bd = 0;
            float fi = 0, fj = 0;
            if (case_at(ctx, f.x, f.y, bd, fi, fj, false)) {
                const Re4Context::Grid& G = ctx.grids[bd];
                // meme parite que la piece : un nombre entier de cases de large -> centre sur un demi-entier, sinon entier
                auto snap = [](float v, float cur) {
                    const float par = std::fabs(cur - std::floor(cur)) > 0.25f ? 0.5f : 0.0f;
                    return std::round(v - par) + par;
                };
                const float x = std::clamp(snap(fi, ctx.hand_x), 0.0f, (float)G.w - 1.0f);
                const float y = std::clamp(snap(fj, ctx.hand_y), 0.0f, (float)G.h - 1.0f);
                re4_case_set_hand(bd, x, y);
            }
        }
    } else if (f.released) {
        if (ctx.hand) {
            g_pulses.push(kPadA);   // poser (PzzlThinking : A ou X)
        } else if (!g_case.picked && now - f.t0 < 300 && dist < 0.4f * u) {
            int bd = 0;
            float fi = 0, fj = 0;
            if (case_at(ctx, f.x0, f.y0, bd, fi, fj, true)) {
                re4_case_set_cursor(bd, (int)std::lround(fi), (int)std::lround(fj));
                g_pulses.push(kPadA);
            }
        }
        g_case.picked = false;
    }
}

// Tap dans un menu : selectionne le choix ou la case touche(e). Le jeu ne connait que la
// manette, donc on rejoue ce que ferait un joueur : croix jusqu'a la cible, puis A. Rend faux si
// le tap ne tombe sur rien de connu (il vaut alors A, comme avant).
bool menu_tap(const Re4Context& ctx, float x, float y) {
    const Rect gr = game_rect(g_w, g_h);
    const float gx = (x - gr.x) / gr.w * 640.0f, gy = (y - gr.y) / gr.h * 480.0f;
    if (ctx.choice_count > 0) {
        // Le glyphe curseur precede le texte du choix : on vise un peu a sa droite.
        int best = -1;
        float bd = 1e9f;
        for (int i = 0; i < ctx.choice_count; ++i) {
            const float dx = gx - (ctx.choices[i].x + 50.0f), dy = gy - (ctx.choices[i].y + 10.0f);
            const float d = dx * dx * 0.35f + dy * dy;
            if (d < bd) {
                bd = d;
                best = i;
            }
        }
        if (best < 0 || bd > 70.0f * 70.0f)
            return false;
        const int cur = std::max(0, ctx.choice_cursor);
        const uint16_t next = ctx.choice_vertical ? kPadDown : kPadRight;
        const uint16_t prev = ctx.choice_vertical ? kPadUp : kPadLeft;
        for (int i = cur; i < best; ++i) g_pulses.push(next);
        for (int i = cur; i > best; --i) g_pulses.push(prev);
        g_pulses.push(kPadA);
        return true;
    }
    if (ctx.grid && false) {   // retire le 14/10 (retour : peu precis) ; les fleches font le travail
        // Case touchee : inverse du repere affine de la grille projetee.
        const float px = gx - ctx.grid_origin[0], py = gy - ctx.grid_origin[1];
        const float a = ctx.grid_ax[0], b = ctx.grid_ay[0], c = ctx.grid_ax[1], d = ctx.grid_ay[1];
        const float det = a * d - b * c;
        if (std::fabs(det) < 1e-3f)
            return false;
        const int cx = (int)std::lround((d * px - b * py) / det);
        const int cy = (int)std::lround((a * py - c * px) / det);
        if (cx < 0 || cy < 0 || cx >= ctx.grid_w || cy >= ctx.grid_h)
            return false;
        if (cx == ctx.grid_cx && cy == ctx.grid_cy) {
            g_pulses.push(kPadA);  // deuxieme tap sur l'objet : menu Equiper / Examiner / Jeter
            return true;
        }
        for (int i = ctx.grid_cx; i < cx; ++i) g_pulses.push(kPadRight);
        for (int i = ctx.grid_cx; i > cx; --i) g_pulses.push(kPadLeft);
        for (int i = ctx.grid_cy; i < cy; ++i) g_pulses.push(kPadDown);
        for (int i = ctx.grid_cy; i > cy; --i) g_pulses.push(kPadUp);
        return true;
    }
    return false;
}

// Zone du stick flottant : tiers gauche, sous la bande du haut.
bool in_stick_zone(float x, float y) { return x < g_w * 0.40f && y > unit() * 2.0f; }

void stick_from(float dx, float dy, float radius, int8_t& sx, int8_t& sy) {
    float nx = dx / radius, ny = -dy / radius;
    const float m = std::sqrt(nx * nx + ny * ny);
    if (m > 1.0f) {
        nx /= m;
        ny /= m;
    }
    // Petite zone morte : le pouce ne tient jamais parfaitement immobile.
    if (m < 0.12f)
        nx = ny = 0;
    sx = (int8_t)std::lround(nx * 127.0f);
    sy = (int8_t)std::lround(ny * 127.0f);
}

// Commandes Wii (17/11) : glisser vite pendant que le couteau est sorti = coup de Wiimote (2,5 g pendant 80 ms). Vitesse en
// fractions d'ecran par seconde, mesuree sur les mouvements de chaque doigt (quel que soit le bouton qui le possede).
uint32_t g_swipe_until = 0;
struct SwipeTrack { SDL_FingerID id; float x, y; uint32_t t; bool used; };
SwipeTrack g_swipe[10];

void swipe_motion(const SDL_TouchFingerEvent& f, uint32_t now) {
    SwipeTrack* s = nullptr;
    for (auto& k : g_swipe)
        if (k.used && k.id == f.fingerId) s = &k;
    if (!s) {
        for (auto& k : g_swipe)
            if (!k.used) { s = &k; break; }
        if (!s) return;
        *s = {f.fingerId, f.x, f.y, now, true};
        return;
    }
    const uint32_t dt = now - s->t;
    if (dt >= 8) {
        const float dx = f.x - s->x, dy = f.y - s->y;
        const float v = std::sqrt(dx * dx + dy * dy) * 1000.0f / (float)dt;
        if (v > 3.0f && (g_st.knife_down || g_st.knife_lock))
            g_swipe_until = now + 80;
        s->x = f.x; s->y = f.y; s->t = now;
    }
}

} // namespace

float wii_accel_g() {
    static SDL_Sensor* sensor = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        if (SDL_InitSubSystem(SDL_INIT_SENSOR) == 0)
            for (int i = 0; i < SDL_NumSensors(); ++i)
                if (SDL_SensorGetDeviceType(i) == SDL_SENSOR_ACCEL) {
                    sensor = SDL_SensorOpen(i);   // SDL rend le meme capteur que shake_hit (compte de references)
                    break;
                }
    }
    float g = 0.0f;
    float a[3];
    if (sensor && SDL_SensorGetData(sensor, a, 3) == 0)
        g = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]) / 9.80665f;
    if (SDL_GetTicks() < g_swipe_until)
        g = std::max(g, 2.5f);
    return std::round(g * 10.0f) / 10.0f;   // au dixieme : le message manette ne part que s'il change
}

void touch_event_feed(const SDL_Event& e) {
    if (e.type == SDL_FINGERMOTION)
        swipe_motion(e.tfinger, SDL_GetTicks());
    else if (e.type == SDL_FINGERUP)
        for (auto& k : g_swipe)
            if (k.used && k.id == e.tfinger.fingerId) k.used = false;
    // NJ_TOUCH_LOG=1 : chaque evenement de doigt, pour les tests par adb.
    static const bool log = NJ_ENV("NJ_TOUCH_LOG") != nullptr;
    if (log && (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERUP || e.type == SDL_FINGERMOTION) &&
        e.type != SDL_FINGERMOTION) {
        NJ_PRINTF("[TOUCH] %s id=%lld %.3f %.3f (%dx%d)\n", e.type == SDL_FINGERDOWN ? "down" : "up",
                    (long long)e.tfinger.fingerId, e.tfinger.x, e.tfinger.y, g_w, g_h);
        std::fflush(stdout);
    }
    switch (e.type) {
    case SDL_FINGERDOWN:
        finger_down(e.tfinger.fingerId, e.tfinger.x, e.tfinger.y);
        break;
    case SDL_FINGERMOTION:
        finger_move(e.tfinger.fingerId, e.tfinger.x, e.tfinger.y);
        break;
    case SDL_FINGERUP:
        finger_up(e.tfinger.fingerId);
        break;
    default:
        break;
    }
    // NJ_TOUCH_MOUSE=1 : la souris joue un doigt, pour essayer la couche tactile sur PC.
    static const bool mouse_finger = NJ_ENV("NJ_TOUCH_MOUSE") != nullptr;
    if (mouse_finger) {
        constexpr SDL_FingerID kMouse = (SDL_FingerID)-77;
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT && e.button.which != SDL_TOUCH_MOUSEID)
            finger_down(kMouse, (float)e.button.x / g_w, (float)e.button.y / g_h);
        else if (e.type == SDL_MOUSEMOTION && e.motion.which != SDL_TOUCH_MOUSEID)
            finger_move(kMouse, (float)e.motion.x / g_w, (float)e.motion.y / g_h);
        else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && e.button.which != SDL_TOUCH_MOUSEID)
            finger_up(kMouse);
    }
}

bool touch_seen() { return g_seen; }

#ifdef NJ_LAB
// Labo (tools/lab) : etat interne des doigts et des verrous, pour les scenarios de test.
std::string touch_debug() {
    std::string r = "look=" + std::to_string(g_st.look_finger) + " stick=" + std::to_string(g_st.stick_finger) + " pinch=" +
                    std::to_string(g_st.pinch_finger) + " locks=" + (g_st.run_lock ? "R" : "-") + (g_st.aim_lock ? "A" : "-") +
                    (g_st.knife_lock ? "K" : "-");
    for (size_t i = 0; i < g_fingers.size(); ++i)
        if (g_fingers[i].down || g_fingers[i].released)
            r += " [" + std::to_string(i) + ": own=" + std::to_string(g_fingers[i].owner) + (g_fingers[i].down ? " d" : " r") + "]";
    return r;
}
#endif

// Secousse du telephone (16/10) : pour les QTE a marteler (« SHAKE OFF », rocher...), secouer vaut un appui. Accelerometre SDL
// (Android, tablettes) ; sans capteur (PC) rien ne se passe. Rend vrai a chaque coup franc, au plus un tous les 90 ms.
static bool shake_hit(uint32_t now) {
    static SDL_Sensor* sensor = nullptr;
    static bool tried = false;
    static float last[3] = {0, 0, 0};
    static bool have_last = false;
    if (!tried) {
        tried = true;
        if (SDL_InitSubSystem(SDL_INIT_SENSOR) == 0)
            for (int i = 0; i < SDL_NumSensors(); ++i)
                if (SDL_SensorGetDeviceType(i) == SDL_SENSOR_ACCEL) {
                    sensor = SDL_SensorOpen(i);
                    break;
                }
    }
    if (!sensor)
        return false;
    float a[3];
    if (SDL_SensorGetData(sensor, a, 3) != 0)
        return false;
    const float dx = a[0] - last[0], dy = a[1] - last[1], dz = a[2] - last[2];
    const bool had = have_last;
    last[0] = a[0]; last[1] = a[1]; last[2] = a[2];
    have_last = true;
    if (!had)
        return false;
    if (std::sqrt(dx * dx + dy * dy + dz * dz) < 7.0f)   // m/s^2 : un vrai coup, pas le bruit de la main
        return false;
    if (now - g_st.shake_last < 90)
        return false;
    g_st.shake_last = now;
    return true;
}

void TouchSource::poll(PadState pads[4]) {
    // NJ_TOUCH_SHOW=1 : commandes affichees sans attendre un doigt (captures sur PC).
    static const bool show = NJ_ENV("NJ_TOUCH_SHOW") != nullptr;
    if (show)
        g_seen = true;
    if (!g_seen) {
        g_sprites.clear();
        return;
    }
    Re4Context ctx = re4_context_read();
    const float u = unit();
    const uint32_t now = SDL_GetTicks();
    {   // L'indicateur « invite affichee cette image » du jeu (ActBtn+0x42) est remis a zero a chaque image : lu a un moment
        // quelconque, il manque une fois sur deux, et la disposition clignotait entre « jeu » et « texte (A) » (16/10).
        // On garde la derniere invite 300 ms.
        static uint32_t last_seen = 0;
        static Re4Context mem_ctx;
        if (ctx.prompt != 0) {
            if (now - last_seen < 300 && mem_ctx.prompt_kind > ctx.prompt_kind)
                ctx.prompt_kind = mem_ctx.prompt_kind;   // le type lu change d'une image a l'autre : on garde le plus specifique
            last_seen = now;
            mem_ctx = ctx;
        } else if (last_seen && now - last_seen < 300 && ctx.mode == mem_ctx.mode && ctx.qte == Re4Qte::None) {
            ctx.prompt = mem_ctx.prompt;
            ctx.prompt_pad = mem_ctx.prompt_pad;
            ctx.prompt_kind = mem_ctx.prompt_kind;
            ctx.qte = mem_ctx.qte;
        }
    }
    PadState& pad = pads[0];
    pad.err = 0;

    if (ctx.mode != g_st.last_mode) {
        // Changer de contexte relache les verrous : courir verrouille pendant une cinematique puis
        // dans l'inventaire, ce serait B maintenu = annuler en boucle.
        if (ctx.mode != Re4Mode::Gameplay)
            g_st.run_lock = g_st.aim_lock = g_st.knife_lock = false;
        g_st.last_mode = ctx.mode;
    }
    // Mort / ecran « Continue? » : plus aucun verrou (sinon Leon reapparait bloque en visee ou en course, 16/10).
    if (ctx.death)
        g_st.run_lock = g_st.aim_lock = g_st.knife_lock = false;
    {   // 20/10 (demande de l'utilisateur) : un verrou ne survit ni a un coup recu (les points de vie baissent), ni au debut d'un QTE :
        // garder viser ou courir pendant que Leon encaisse, se fait saisir ou esquive est illogique et le relance en visee aussitot apres.
        static int last_hp = -1;
        static bool last_qte = false;
        const bool qte_now = ctx.qte != Re4Qte::None;
        if ((last_hp >= 0 && ctx.hp >= 0 && ctx.hp < last_hp) || (qte_now && !last_qte))
            g_st.run_lock = g_st.aim_lock = g_st.knife_lock = false;
        last_hp = ctx.hp;
        last_qte = qte_now;
    }

    {
        static const char* last = nullptr;
        const char* s = situation(ctx);
        if (s != last) {
            last = s;
            char b[64];
            std::snprintf(b, sizeof b, "tactile : %s", s);
            nj::perf::event(b);
        }
    }
    {   // journal : type et bouton de chaque invite d'action (16/10), pour verifier les icones d'action en jeu
        static int last_kind = -2, last_btn = -2;
        if (ctx.prompt_kind != last_kind || (int)ctx.prompt != last_btn) {
            last_kind = ctx.prompt_kind;
            last_btn = ctx.prompt;
            char b[64];
            std::snprintf(b, sizeof b, "tactile : invite kind=%d btn=%d wep=%d", ctx.prompt_kind, (int)ctx.prompt, ctx.wep_no);
            nj::perf::event(b);
        }
    }
    {   // journal : chapitre en cours et piece (pour reperer la fin du duel de Krauser, 5-3)
        static int last_room = -1, last_chap = -1;
        if (ctx.room != last_room || ctx.chapters_ended != last_chap) {
            last_room = ctx.room;
            last_chap = ctx.chapters_ended;
            char b[64];
            std::snprintf(b, sizeof b, "tactile : piece r%03x chapitres finis=%d", ctx.room, ctx.chapters_ended);
            nj::perf::event(b);
        }
    }
    std::vector<Button> buttons = layout(ctx);
    const bool editing = g_edit;
    if (editing)
        buttons = edit_layout();
    // Commandes cachees : en automatique quand une manette vient de servir, ou a la demande (seul l'engrenage reste).
    const int tmode = nj::runtime::gfx::g_touch_controls;
    const bool touch_off = tmode == 2;
    if (!editing && (touch_off || (tmode == 0 && nj::runtime::gfx::g_last_device == 1))) {
        std::vector<Button> keep;
        if (touch_off)
            for (const Button& x : buttons)
                if (x.icon == IconPause) keep.push_back(x);
        buttons.swap(keep);
    }
    const bool text = !editing && text_only(ctx);   // toucher n'importe ou = A
    const bool gameplay = !editing && ctx.mode == Re4Mode::Gameplay && !text;
    const bool qte = !editing && (ctx.mode == Re4Mode::Gameplay || ctx.mode == Re4Mode::Event) && ctx.qte != Re4Qte::None;
    // cinematique (hors QTE) : toucher n'importe ou = A, comme dans un menu (texte, appel radio)
    const bool menu_mode = !editing && (text || (!gameplay && ctx.mode != Re4Mode::Movie && !qte));

    uint16_t held = 0;
    int8_t sx = 0, sy = 0, cx = 0, cy = 0;

    // Visee au doigt : pointeur Wii (reglage « Aim with ») pendant une visee en jeu.
    const bool point_aim = gameplay && ctx.aiming && wii_aim_enabled();
    wii_touch_set_aiming(point_aim);

    // Secouer le telephone = marteler le bouton du QTE
    if (qte && ctx.qte == Re4Qte::Mash && shake_hit(now))
        g_pulses.push(ctx.prompt_pad ? ctx.prompt_pad : kPadA);

    // 20/10 : un doigt deja pose suit SON bouton, pas une position dans la liste. La liste change d'une image a l'autre
    // (viser fait apparaitre le bouton de tir en tete) : tenir « viser » pointait ensuite sur « courir » (Leon baissait
    // l'arme, le tir disparaissait, et ainsi de suite : « ca spamme ») et tenir le couteau pointait sur « viser ».
    auto bkey = [](const Button& b) { return ((uint32_t)b.icon << 16) | (uint32_t)b.pad; };
    for (Finger& f : g_fingers) {
        if (f.fresh || !(f.down || f.released) || f.okey == 0) continue;
        const int base = f.owner >= 5000 ? 5000 : 0;
        const int idx = f.owner - base;
        if (f.owner < 0 || (base == 0 && f.owner >= 1000)) continue;   // stick, regard, menus... : pas un bouton
        if (idx >= 0 && idx < (int)buttons.size() && bkey(buttons[idx]) == f.okey) continue;
        int found = -1;
        for (int k = 0; k < (int)buttons.size(); ++k)
            if (bkey(buttons[k]) == f.okey) { found = k; break; }
        f.owner = found < 0 ? -1 : base + found;   // bouton disparu : le doigt ne tient plus rien
    }

    // Attribution des nouveaux contacts : bouton > QTE > stick > regard.
    for (int i = 0; i < (int)g_fingers.size(); ++i) {
        Finger& f = g_fingers[i];
        if (!(f.down || f.released) || !f.fresh)
            continue;
        f.fresh = false;
        f.owner = -1;
        f.okey = 0;
        for (int k = 0; k < (int)buttons.size(); ++k)
            if (inside(buttons[k], f.x0, f.y0)) {
                f.owner = k;
                f.okey = bkey(buttons[k]);
                break;
            }
        if (f.owner >= 0 && editing) {   // edition : un bouton se choisit (et se glisse), la barre d'outils agit ; rien n'arrive au jeu
            const Button& eb = buttons[f.owner];
            if (eb.pad >= kEditPlus && eb.pad <= kEditDone) {
                edit_toolbar(eb.pad);
            } else if (eb.role >= 0) {
                g_edit_sel = eb.role;
                g_edit_start = g_ov[eb.role];
            }
            f.owner = 5000 + f.owner;
            continue;
        }
        if (f.owner >= 0) {
            Button& b = buttons[f.owner];
            {   // journal : chaque toucher de bouton (essais automatiques par adb)
                char jb[64];
                std::snprintf(jb, sizeof jb, "tactile : bouton pad=%04x kind=%d", (unsigned)b.pad, (int)b.kind);
                nj::perf::event(jb);
            }
            nj::runtime::gfx::haptic_tap();   // petit retour au toucher (Controls > Vibration)
            // 20/10 : declencher une action (echelle, fenetre, coup de pied, porte...) lache les verrous : Leon joue une animation,
            // il ne doit pas en ressortir en visee ou en course
            if (b.icon == IconAction && ctx.prompt != 0 && ctx.qte == Re4Qte::None)
                g_st.run_lock = g_st.aim_lock = g_st.knife_lock = false;
            if (b.kind == KTap) {
                if (b.icon == IconPause) {
                    nj::runtime::gfx::menu_toggle();   // menu des parametres, pas le jeu
                } else if (b.pad == kDeathYes || b.pad == kDeathNo) {
                    // Continue? : le curseur d'abord (gauche = Oui, droite = Non), puis A
                    g_pulses.push(b.pad == kDeathYes ? kPadLeft : kPadRight);
                    g_pulses.push(kPadA);
                } else if (b.pad == kBinoExit) {
                    g_pulses.push(kPadB);
                    g_pulses.push(kPadY);
                } else if (b.icon == IconTurn) {
                    // Demi-tour : bas + B (manuel de RE4). Declenche la sequence de front montant.
                    s_turn.phase = 1;
                    s_turn.frame0 = g_st_frames;
                    s_turn.t0 = now;
                } else {
                    g_pulses.push(b.pad);
                }
            } else if (b.kind == KToggle) {
                bool& lock = b.pad == kRun ? g_st.run_lock : b.pad == kKnife ? g_st.knife_lock : g_st.aim_lock;
                bool& was = b.pad == kRun ? g_st.run_was_locked : b.pad == kKnife ? g_st.knife_was_locked : g_st.aim_was_locked;
                uint32_t& last = b.pad == kRun ? g_st.run_last_tap : b.pad == kKnife ? g_st.knife_last_tap : g_st.aim_last_tap;
                was = lock;
                if (lock)
                    lock = false;   // toucher un bouton verrouille le relache tout de suite
                last = now;
            }
            if (b.pad == kAim && gameplay)
                g_st.aim_finger = i;
            continue;
        }
        if (touch_off)
            continue;   // commandes cachees : un doigt hors de l'engrenage ne fait rien
        if (qte) {
            f.owner = 1000;  // geste de QTE, resolu au relachement ou en direct (martelement)
            if (ctx.qte == Re4Qte::Mash || ctx.qte == Re4Qte::Single)
                g_pulses.push(ctx.prompt_pad ? ctx.prompt_pad : kPadA);
            continue;
        }
        if (gameplay && !ctx.binoculars && in_stick_zone(f.x0, f.y0) && g_st.stick_finger < 0) {
            if (NJ_ENV("NJ_TOUCH_LOG"))
                NJ_PRINTF("[TOUCH] stick <- doigt %d (%.0f,%.0f)\n", i, f.x0, f.y0);
            g_st.stick_finger = i;
            // Stick fixe (demande du 07/10) quand le doigt se pose sur l'anneau visible du stick : la base reste a sa
            // place de repos. Stick flottant (demande du 29/10) quand il se pose ailleurs dans la zone gauche : la
            // base apparait sous le pouce (sinon, loin au-dessus de la base, la poussee est toujours « haut » et
            // balayer vers la gauche ne tournait presque pas : mesure [Mesure] du 28/10, 173 lignes haut-gauche).
            {
                const float sr = 1.5f * unit();
                const float rest_x = 2.6f * unit(), rest_y = (float)g_h - 2.6f * unit();
                const float ddx = f.x0 - rest_x, ddy = f.y0 - rest_y;
                if (ddx * ddx + ddy * ddy <= (1.6f * sr) * (1.6f * sr)) {
                    g_st.sx0 = rest_x;
                    g_st.sy0 = rest_y;
                } else {
                    g_st.sx0 = std::clamp(f.x0, sr, (float)g_w - sr);
                    g_st.sy0 = std::clamp(f.y0, sr, (float)g_h - sr);
                }
            }
            f.owner = 2000;
            continue;
        }
        if (gameplay && g_st.look_finger < 0) {
            g_st.look_finger = i;
            f.owner = 3000;
            continue;
        }
        if (gameplay && ctx.binoculars && g_st.pinch_finger < 0) {   // 2e doigt : pincer pour zoomer
            g_st.pinch_finger = i;
            g_st.pinch_d = -1.0f;
            f.owner = 3100;
            continue;
        }
        if (ctx.inventory && ctx.hand) {   // piece en main : un second doigt la tourne (R)
            bool other = false;
            for (int j = 0; j < (int)g_fingers.size(); ++j)
                other |= j != i && g_fingers[j].down && g_fingers[j].owner == 6000;
            if (other) {
                g_pulses.push(kPadR);
                f.owner = 6001;
                continue;
            }
        }
        if (menu_mode) {
            int bd = 0;
            float fi = 0, fj = 0;
            if (ctx.inventory && ctx.grids[0].valid && case_at(ctx, f.x0, f.y0, bd, fi, fj, true))
                f.owner = 6000;   // mallette : geste de la piece (case_gesture)
            else
                f.owner = 4000;  // geste de menu (balayage = croix, tap = valider), au relachement
        }
    }

    // Etat continu et relachements
    const float stick_r = 1.5f * u;
    for (int i = 0; i < (int)g_fingers.size(); ++i) {
        Finger& f = g_fingers[i];
        if (!(f.down || f.released))
            continue;
        const float dx = f.x - f.x0, dy = f.y - f.y0;
        const float dist = std::sqrt(dx * dx + dy * dy);
        // Deplacement depuis la lecture precedente, en « unites de souris » du pointeur Wii : un glissement de la largeur
        // de l'ecran = la largeur du pointeur (1138 unites, voir mouse_keyboard_source.cpp).
        float mdx = 0, mdy = 0;
        if (f.down) {
            if (f.has_prev) {
                const float k = 1138.0f / std::max(1.0f, (float)g_w);
                mdx = (f.x - f.px) * k;
                mdy = (f.y - f.py) * k;
            }
            f.px = f.x;
            f.py = f.y;
            f.has_prev = true;
        }
        if (editing && f.owner >= 5000 && f.down && f.owner - 5000 < (int)buttons.size() && buttons[f.owner - 5000].role >= 0)
            edit_drag(buttons[f.owner - 5000], f);
        if (f.owner >= 0 && f.owner < (int)buttons.size()) {
            Button& b = buttons[f.owner];
            if (f.released && b.kind == KToggle) {
                // appui court sans glisser : bascule le verrou (couteau et viser s'excluent)
                const bool tap = now - f.t0 < 300 && dist < 0.4f * u;
                const bool was = b.pad == kRun ? g_st.run_was_locked : b.pad == kKnife ? g_st.knife_was_locked : g_st.aim_was_locked;
                // 19/10 : le test « deja en visee » (ctx.aiming) refusait le verrou de visee : pendant l'appui, le doigt tient
                // viser, Leon leve l'arme en quelques images, et au relachement ctx.aiming etait deja vrai. Le verrou de
                // depart (was) suffit a distinguer « je verrouille » de « je deverrouille ».
                if (tap && !was) {
                    // Style (Controls > Lock style) : 0 un appui court verrouille, 1 deux appuis courts de suite (400 ms).
                    uint32_t& end = b.pad == kRun ? g_st.run_tap_end : b.pad == kKnife ? g_st.knife_tap_end : g_st.aim_tap_end;
                    const bool arm = nj::runtime::gfx::g_touch_lock == 0 || (end != 0 && now - end < 400);
                    if (arm) {
                        end = 0;
                        if (b.pad == kRun) g_st.run_lock = true;
                        else if (b.pad == kKnife) { g_st.knife_lock = true; g_st.aim_lock = false; }
                        else { g_st.aim_lock = true; g_st.knife_lock = false; }
                    } else {
                        end = now;
                    }
                }
            }
            if (f.down) {
                b.pressed = true;
                if (b.pad == kZoomIn) {
                    cy = 127;
                } else if (b.pad == kZoomOut) {
                    cy = -127;
                } else if (b.kind == KButton || b.kind == KToggle) {
                    held |= b.pad;
                }
                // 19/10 (demande des joueurs) : glisser le pouce depuis viser / couteau ne bouge plus la camera ni le pointeur ;
                // la visee se fait au stick, ou avec l'autre pouce.
            }
        } else if (f.owner == 1000 && f.released) {
            const uint32_t dt = now - f.t0;
            if (ctx.qte == Re4Qte::Pair && (dist > 1.2f * u || dt < 350))
                g_pulses.push(ctx.prompt_pad ? ctx.prompt_pad : (kPadL | kPadR));
        } else if (f.owner == 2000 && f.down) {
            stick_from(f.x - g_st.sx0, f.y - g_st.sy0, stick_r, sx, sy);
        } else if (f.owner == 3000 && f.down && ctx.binoculars) {
            stick_from(dx, dy, 2.0f * u, sx, sy);   // jumelles : le pas du stick principal oriente la vue
        } else if (f.owner == 3000 && f.down) {
            if (point_aim)
                wii_touch_add(mdx, mdy);           // en visee, le doigt libre pousse le pointeur (pas la camera)
            else
                stick_from(dx, dy, 2.0f * u, cx, cy);
        } else if (f.owner == 6000) {
            case_gesture(ctx, f, now);
        } else if (f.owner == 4000 && f.released) {
            if (dist > 0.8f * u) {
                // Balayage en menu : une impulsion de croix dans la direction dominante.
                if (std::fabs(dx) > std::fabs(dy))
                    g_pulses.push(dx > 0 ? kPadRight : kPadLeft);
                else
                    g_pulses.push(dy > 0 ? kPadDown : kPadUp);
            } else if (now - f.t0 < 400 && !menu_tap(ctx, f.x0, f.y0)) {
                g_pulses.push(kPadA);
            }
        }
        if (f.released) {
            if (i == g_st.stick_finger) g_st.stick_finger = -1;
            if (i == g_st.aim_finger) g_st.aim_finger = -1;
            if (i == g_st.look_finger) g_st.look_finger = -1;
            if (i == g_st.pinch_finger) { g_st.pinch_finger = -1; g_st.pinch_d = -1.0f; }
            f = Finger{};
        }
    }

    if (gameplay && ctx.binoculars && g_st.look_finger >= 0 && g_st.pinch_finger >= 0) {
        const Finger& a = g_fingers[g_st.look_finger];
        const Finger& c = g_fingers[g_st.pinch_finger];
        const float d = std::hypot(a.x - c.x, a.y - c.y);
        if (g_st.pinch_d >= 0.0f)
            cy = (int8_t)std::clamp((d - g_st.pinch_d) / u * 500.0f, -127.0f, 127.0f);   // ecarter = zoom avant (ssy > 0)
        g_st.pinch_d = d;
    } else {
        g_st.pinch_d = -1.0f;
    }
    {   // quels boutons verrouillables sont tenus par un doigt (pour afficher le bouton de tir a l'image suivante)
        bool ad = false, kd = false;
        for (const Finger& f : g_fingers)
            if (f.down && f.owner >= 0 && f.owner < (int)buttons.size()) {
                ad |= buttons[f.owner].pad == kAim && buttons[f.owner].kind == KToggle;
                kd |= buttons[f.owner].pad == kKnife && buttons[f.owner].kind == KToggle;
            }
        g_st.aim_down = ad;
        g_st.knife_down = kd;
    }
    {   // Releve (19/10) : quel mouvement le jeu donne a Leon apres une invite d'action ? Sert a choisir les animations des icones
        // d'apres ce que fait vraiment le jeu, et a nommer les mouvements du Motion Viewer. Journal : « mouvement kind=K rang=R ».
        static int pk = -1, pn = 0;
        static uint32_t pt = 0, pm = 0;
        if (ctx.prompt_kind >= 0) {
            if (ctx.prompt_kind != pk) pn = 0;
            pk = ctx.prompt_kind;
            pt = now;
        }
        if (ctx.leon_mot && ctx.leon_mot != pm) {
            pm = ctx.leon_mot;
            if (pk >= 0 && now - pt < 1200 && pn < 4) {
                ++pn;
                char jb[96];
                std::snprintf(jb, sizeof jb, "tactile : mouvement kind=%d rang=%d dt=%u", pk, re4_motion_index(pm), (unsigned)(now - pt));
                nj::perf::event(jb);
            }
        }
    }
    if (gameplay && !ctx.aiming && g_st.run_lock)
        held |= kRun;
    if (gameplay && g_st.aim_lock)
        held |= kAim;
    if (gameplay && g_st.knife_lock)
        held |= kKnife;
    // Commandes simples : le stick pousse a fond tient B (course) ; avec le stick vers le bas, c'est le demi-tour
    // rapide de RE4 (B + bas). Hysteresis : on engage a 92 %, on lache a 80 % (le pouce ne tient pas un rayon).
    {
        static bool auto_run = false;
        const float m = std::sqrt((float)sx * sx + (float)sy * sy) / 127.0f;
        if (nj::runtime::gfx::g_touch_simple && gameplay && !ctx.aiming && !ctx.binoculars && g_st.stick_finger >= 0)
            auto_run = auto_run ? m >= 0.80f : m >= 0.92f;
        else
            auto_run = false;
        if (auto_run)
            held |= kRun;
    }

    // Demi-tour 180 : front montant sur B (0 -> 1) pendant que le stick est oriente vers le bas.
    // Si run_lock est actif, B etait maintenu en continu (pas de front montant) et Leon reculait
    // sans se retourner. On force :
    // - Phase 1 : B relache (held sans kPadB) et stick vers le bas (sy = -127) pendant au moins 2 images invitees
    // - Phase 2 : B enfonce (held avec kPadB) et stick vers le bas (sy = -127) pendant au moins 3 images invitees
    if (s_turn.phase > 0) {
        const uint64_t fr = g_st_frames;
        auto elapsed = [&](uint64_t frames, uint32_t min_ms) {
            return (fr - s_turn.frame0 >= frames && now - s_turn.t0 >= min_ms) || now - s_turn.t0 >= 400;
        };
        if (s_turn.phase == 1) {
            held &= ~kPadB;
            sx = 0;
            sy = -127;
            if (elapsed(2, 60)) {
                s_turn.phase = 2;
                s_turn.frame0 = fr;
                s_turn.t0 = now;
            }
        } else if (s_turn.phase == 2) {
            held |= kPadB;
            sx = 0;
            sy = -127;
            if (elapsed(3, 90)) {
                s_turn.phase = 0;
            }
        }
    }

    g_pulses.apply(held, sx, sy);

#if defined(__APPLE__)
    {   // iPhone : valeurs envoyees au jeu quand le stick change (400 lignes au plus), pour comparer avec ce que fait Leon
        static int logged = 0;
        static int last_sx = 0, last_sy = 0;
        if (logged < 400 && (sx != last_sx || sy != last_sy)) {
            ++logged;
            last_sx = sx;
            last_sy = sy;
            NJ_PRINTF("[Stick] sx=%d sy=%d doigt=%d jeu=%d visee=%d jumelles=%d boutons=0x%x\n", (int)sx, (int)sy,
                      g_st.stick_finger, (int)gameplay, (int)ctx.aiming, (int)ctx.binoculars, (unsigned)held);
        }
    }
#endif

    pad.buttons |= held;
    if (sx || sy) {
        pad.stick_x = sx;
        pad.stick_y = sy;
    }
    if (cx || cy) {
        pad.substick_x = cx;
        pad.substick_y = cy;
    }
    if (held & kPadR) pad.trigger_r = 255;
    if (held & kPadL) pad.trigger_l = 255;

    // ------------------------------------------------------------------ icones
    g_sprites.clear();
    // Chaque nouvel evenement (une invite d'action apparait, un QTE commence) change la main de
    // Leon montree par les icones a main : droite, puis gauche, puis droite...
    {
        static bool was_event = false;
        const bool event = qte || (gameplay && ctx.prompt != 0);
        if (event && !was_event)
            g_hand_right = !g_hand_right;
        was_event = event;
    }
    const float pulse = 0.5f + 0.5f * std::sin(now * 0.012f);
    // Animation des icones : « seulement quand on s'en sert » (defaut) = sous le doigt, verrouillee (course, viser, couteau),
    // en visee, ou avec une action proposee (teinte « appuye » posee par layout()). Sinon l'icone reste a sa premiere image.
    auto icon_time = [&](const Button& b) -> float {
        const int m = nj::runtime::gfx::g_touch_anim;
        if (m == 1) return now * 0.001f;
        if (m == 0) return 0.0f;
        const bool used = b.pressed || b.tint == kPressed || (b.pad == kRun && g_st.run_lock) ||
                          (b.pad == kAim && (g_st.aim_lock || ctx.aiming)) || (b.pad == kKnife && g_st.knife_lock);
        return used ? now * 0.001f : 0.0f;
    };
    // Style et transparence (F10, 13/10) : pierre = jauge de vie de RE4, or = overlay ; icone teintee
    // creme ou or, orange sous le doigt (choix selectionne des menus du jeu), sillon vert si verrouille.
    g_styled = nj::runtime::gfx::g_touch_style != 0;
    const float op = std::clamp(nj::runtime::gfx::g_touch_opacity, 20, 100) / 100.0f;
    const int piece = g_touch_style_base();
    const bool gold = nj::runtime::gfx::g_touch_style == 2;
    // Teintes multipliees par les gris des icones (~0,6) : plus claires que la couleur voulue, pour
    // retrouver l'or et le creme des maquettes (13/10, retours en jeu).
    const uint32_t ink = gold ? 0xFFD884FFu : 0xFFF6E2FFu;
    // Appui et invite dans la couleur du style (or vif / creme), vert seulement pour « verrouille ».
    const uint32_t kHot = gold ? 0xFFF0D0FFu : 0xFFFFFFFFu;
    const uint32_t kGroove = gold ? 0xE8C470FFu : 0xEDE4CEFFu;
    constexpr uint32_t kLocked = 0x2E9C42FFu;
    // le fond reste une carte pleine : la transparence ne l'allege qu'en partie
    const float lens_k = 0.35f + 0.65f * op;
    auto cell = [&](int k, float x, float y, float size, uint32_t rgba) {
        Sprite s{IconBlank, x, y, size, rgba};
        s.cell = overlay_style_cell(k);
        g_sprites.push_back(s);
    };
    // Dessin d'un bouton : son icone animee, ou l'animation d'action de Leon quand l'invite en a une
    auto emit_btn = [&](const Button& b, uint32_t rgba, float t) {
        if (b.book >= 0) {
            namespace tc = nj::runtime::input::touch;
            const float size = b.r * 2.0f;
            const bool ring_in = tc::overlay_action_ring(b.book);
            if (!ring_in)
                push(IconRing, b.x, b.y, size, rgba);   // le style choisi remplace l'anneau blanc
            if (b.icon == IconReload) {   // recharger : les munitions de l'arme, deux fleches qui tournent autour
                const float p = frac(t / (b.pressed ? 0.7f : 2.4f));
                if (b.book2 >= 0) {
                    Sprite am{IconBlank, b.x, b.y, size * 0.62f, rgba};
                    am.atlas = (uint8_t)tc::overlay_action_atlas(b.book2);
                    am.cell = tc::overlay_action_cell(b.book2, 0.0f);
                    g_sprites.push_back(am);
                }
                Sprite ar{IconBlank, b.x, b.y, size, rgba, (b.pressed ? p : ease(p / 0.3f)) * 6.2831853f};
                ar.atlas = (uint8_t)tc::overlay_action_atlas(b.book);
                ar.cell = tc::overlay_action_cell(b.book, 0.0f);
                g_sprites.push_back(ar);
                return;
            }
            float dy = 0.0f, ang = 0.0f, grow = 1.0f;
            if (b.icon == IconFire) {   // tir : recul et eclair de bouche, comme l'icone de tir d'avant
                const float p = frac(t / (b.pressed ? 0.25f : 1.6f));
                const float k = p < 0.5f ? std::exp(-p * 14.0f) : 0.0f;
                dy = -size * 0.04f * k;
                ang = -0.18f * k;
                grow = 1.0f + 0.05f * k;
                if (p < 0.12f)
                    push_fx(IconFxRays3, b.x, b.y, size, scale_alpha(rgba, 1.0f - p / 0.12f), 0.8f + 2.0f * p);
            }
            Sprite sp{IconBlank, b.x, b.y + dy, size * grow, rgba, ang};
            sp.atlas = (uint8_t)tc::overlay_action_atlas(b.book);
            sp.cell = tc::overlay_action_cell(b.book, b.icon == IconFire ? 0.0f : t);
            g_sprites.push_back(sp);
            return;
        }
        emit_icon(b.icon, b.x, b.y, b.r * 2.0f, rgba, b.angle, t, b.pressed, b.pad == kRun && g_st.run_lock);
    };
    for (const Button& b : buttons) {
        const size_t first_sprite = g_sprites.size();
        auto glyph_fix = [&] {   // miroir / case d'atlas imposee : seulement le dessin du bouton
            if (!b.flip && b.cell < 0)
                return;
            for (size_t i = first_sprite; i < g_sprites.size(); ++i)
                if (g_sprites[i].icon == b.icon) {
                    g_sprites[i].flip = b.flip;
                    if (b.cell >= 0)
                        g_sprites[i].cell = b.cell;
                }
        };
        if (g_styled) {
            const bool locked = (b.pad == kRun && g_st.run_lock) || (b.pad == kAim && g_st.aim_lock) ||
                                (b.pad == kKnife && g_st.knife_lock);
            const float a = b.pressed ? 1.0f : op;
            const uint32_t tint = b.pressed ? kHot : ((b.tint != kWhite && b.tint != kPressed) ? b.tint : ink);   // etats : le sillon
            cell(piece, b.x, b.y, b.r * 2.0f, with_alpha(kWhite, b.pressed ? 1.0f : lens_k));
            emit_btn(b, with_alpha(tint, std::min(1.0f, a + 0.15f)), icon_time(b));
            glyph_fix();
            cell(piece + 1, b.x, b.y, b.r * 2.0f, with_alpha(kWhite, a));
            if (b.pressed || locked)
                cell(4, b.x, b.y, b.r * 2.0f, with_alpha(b.pressed ? kGroove : kLocked, 1.0f));
            continue;
        }
        // L'icone porte son propre anneau (atlas). Appuye, le bouton devient opaque et prend la
        // teinte du bouton (blanc par defaut = couleurs d'origine). emit_icon l'anime.
        const float a = b.pressed ? 1.0f : op;
        emit_btn(b, with_alpha(b.pressed ? kWhite : b.tint, a), icon_time(b));
        glyph_fix();
        if (b.label != IconDisc && b.label != b.icon)
            g_sprites.push_back({b.label, b.x + b.r * 0.74f, b.y + b.r * 0.74f, b.r * 0.62f,
                                 with_alpha(kWhite, a * 0.9f)});
        if ((b.pad == kRun && g_st.run_lock) || (b.pad == kAim && g_st.aim_lock) || (b.pad == kKnife && g_st.knife_lock))
            g_sprites.push_back({IconLock, b.x - b.r * 0.74f, b.y - b.r * 0.74f, b.r * 0.62f,
                                 with_alpha(kYellow, 1.0f)});
    }
    if (editing) {   // grille (points) et bouton choisi
        if (g_edit_snap) {
            const float gs = std::max(8.0f, u * 0.5f);
            for (float gx = gs; gx < g_w; gx += gs)
                for (float gy = gs; gy < g_h; gy += gs)
                    g_sprites.push_back({IconDisc, gx, gy, 0.09f * u, with_alpha(kWhite, 0.22f)});
        }
        for (const Button& x : buttons)
            if (x.role >= 0 && x.role == g_edit_sel)
                g_sprites.push_back({IconRing, x.x, x.y, x.r * (2.5f + 0.2f * pulse), with_alpha(kYellow, 0.55f + 0.35f * pulse)});
    }
    // Invite d'action : l'icone du jeu est deja a l'ecran, on fait pulser le bouton qui y repond.
    if (gameplay && ctx.prompt && ctx.qte == Re4Qte::None && !buttons.empty()) {
        if (g_styled)   // le sillon du bouton bat en orange
            cell(4, buttons[0].x, buttons[0].y, buttons[0].r * (2.0f + 0.12f * pulse),
                 with_alpha(kGroove, 0.45f + 0.55f * pulse));
        else
            g_sprites.push_back({IconRing, buttons[0].x, buttons[0].y, buttons[0].r * (2.2f + 0.3f * pulse),
                                 with_alpha(kYellow, 0.4f + 0.5f * pulse)});
    }
    // Stick (jeu seulement ; les menus ont leur croix)
    if (gameplay && !ctx.binoculars) {
        float bx = 2.6f * u, by = g_h - 2.6f * u, kx = bx, ky = by;
        if (g_st.stick_finger >= 0) {
            const Finger& f = g_fingers[g_st.stick_finger];
            bx = g_st.sx0;
            by = g_st.sy0;
            kx = f.x;
            ky = f.y;
            const float dx = kx - bx, dy = ky - by, d = std::sqrt(dx * dx + dy * dy);
            if (d > stick_r) {
                kx = bx + dx / d * stick_r;
                ky = by + dy / d * stick_r;
            }
        }
        const float a = g_st.stick_finger >= 0 ? 0.8f : 0.4f;
        if (g_styled) {
            const float sa = g_st.stick_finger >= 0 ? 1.0f : op;
            cell(piece, bx, by, stick_r * 2.0f, with_alpha(kWhite, g_st.stick_finger >= 0 ? 1.0f : lens_k * 0.9f));
            cell(piece + 1, bx, by, stick_r * 2.0f, with_alpha(kWhite, sa));
            cell(gold ? 6 : 5, kx, ky, stick_r * 0.9f, with_alpha(kWhite, std::max(sa, lens_k)));
        } else {
            g_sprites.push_back({IconStick, bx, by, stick_r * 2.0f, with_alpha(kWhite, (a + 0.15f) * op / 0.72f)});
            g_sprites.push_back({IconKnob, kx, ky, stick_r * 0.9f, with_alpha(kWhite, (a + 0.1f) * op / 0.72f)});
        }
    }
    (void)kCyan;
    (void)kPause;
}

void touch_overlay_frame(int draw_w, int draw_h) {
    g_w = std::max(1, draw_w);
    g_h = std::max(1, draw_h);
    touch::icon_gen_poll();   // icones d'armes generees chez le joueur (cache, puis fil de fond)
    // Bandeau du portage sur l'ecran titre : sur toutes les plateformes, tactile ou non.
    const Re4Context ctx = re4_context_read();
    title_banner_draw(ctx.valid && ctx.title_menu, ctx.title_menu_alpha, ctx.fade, draw_w, draw_h);
    // Curseur de la Wii Edition pendant la visee au pointeur (Wii 0x80091084, UHD 0x5A3E50) :
    // la texture 0xF9 du HUD Wii, 32 px d'un ecran de 640, blanche teintee vert (40,FF,40) en
    // joue et rouge (FF,40,40) quand la ligne de visee touche un ennemi. NJ_WII_CURSOR=0 l'ote.
    static const bool show_ptr = [] {
        const char* v = NJ_ENV("NJ_WII_CURSOR");
        return !(v && v[0] == '0');
    }();
    const WiiPointer wp = wii_pointer_get();
    wii_pointer_set_target(ctx.valid && ctx.aiming && ctx.aim_target);
    // Hors visee, gris (80,80,80) des qu'une arme visee au pointeur est en main, en jeu (0x5A3E50).
    const bool en_jeu = ctx.valid && (ctx.mode == Re4Mode::Gameplay || ctx.aiming);
    if (show_ptr && wii_aim_enabled() && en_jeu && (ctx.aiming || wii_pointer_weapon(ctx.wep_no))) {
        const Rect r = game_rect(draw_w, draw_h);
        const float x = r.x + r.w * 0.5f * (1.0f + wp.x), y = r.y + r.h * 0.5f * (1.0f + wp.y);
        std::vector<Sprite> ptr = g_sprites;
        const uint32_t col = !(wp.on && ctx.aiming) ? 0x808080FFu : ctx.aim_target ? 0xFF4040FFu : 0x40FF40FFu;
        // Marqueur de touche (UHD HitMarker 3) : un ennemi perd des points de vie pendant la visee
        // (nj_wii_frame) : le curseur pulse 180 ms (1 + 0,35 u^2) et la gerbe de touche (texture
        // 0xDD du HUD Wii, 64/640 de large) s'efface en 250 ms.
        const uint32_t hit = wii_pointer_hit(), dt = hit ? wii_now_ms() - hit : 100000u;
        const bool marker = nj::runtime::Config::get().wii_hit_marker;
        float size = r.w * 32.0f / 640.0f;
        if (marker && dt < 180) {
            const float u = 1.0f - (float)dt / 180.0f;
            size *= 1.0f + 0.35f * u * u;
        }
        ptr.push_back({IconWiiCursor, x, y, size, col});
        if (marker && dt < 250) {
            Sprite s{IconWiiCursor, x, y, r.w * 64.0f / 640.0f, 0xFFFFFF00u |
                     (uint32_t)(255.0f * (1.0f - (float)dt / 250.0f))};
            s.cell = overlay_hitmarker_cell();
            ptr.push_back(s);
        }
        overlay_draw(ptr, draw_w, draw_h);
        return;
    }
    overlay_draw(g_sprites, draw_w, draw_h);
}


void layout_edit_begin() {
    g_edit = true;
    g_edit_sel = -1;
}
bool layout_editing() { return g_edit; }

bool layout_set_by_name(const std::string& name, float dx, float dy, float scale) {
    for (int i = 0; i < kRoleCount; ++i)
        if (name == kRoleName[i]) {
            g_ov[i].dx = std::clamp(dx, -1.0f, 1.0f);
            g_ov[i].dy = std::clamp(dy, -1.0f, 1.0f);
            g_ov[i].sc = std::clamp(scale, 0.5f, 2.2f);
            return true;
        }
    return false;
}

std::vector<std::pair<std::string, std::string>> layout_export() {
    std::vector<std::pair<std::string, std::string>> out;
    for (int i = 0; i < kRoleCount; ++i) {
        char b[64];
        std::snprintf(b, sizeof b, "\"%.4f,%.4f,%.3f\"", g_ov[i].dx, g_ov[i].dy, g_ov[i].sc);
        out.push_back({kRoleName[i], b});
    }
    return out;
}

void layout_set_saved_callback(void (*cb)()) { g_edit_done_cb = cb; }

} // namespace nj::runtime::input
