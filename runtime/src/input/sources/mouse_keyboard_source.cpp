#include "input/re4_context.h"
#include "runtime/gfx_settings.h"
#include "runtime/log.h"
#include "runtime/game_version.h"
#include <chrono>
#include <mutex>
#include "mouse_keyboard_source.h"

#include "../bindings.h"
#include "input/mouse_turn.h"
#include "input/mouse_events.h"
#include "input/wii_pointer.h"
#include "input/touch_overlay.h"
#include "runtime/cpu_context.h"
#include "runtime/config.h"
#include "runtime/env.h"

#include <SDL.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// Compteurs du runtime, hors namespace (src/hle/gx.cpp).
extern uint64_t g_st_frames;
extern uint64_t g_stat_draws;

namespace nj::runtime {
extern GuestCpu* g_guest;
}

namespace nj::runtime::input {

namespace {

// Pointeur Wii : ecrit par update() (fil de lecture des manettes), lu par l'incrustation.
std::atomic<bool> g_ptr_on{false};
std::atomic<float> g_ptr_x{0.0f}, g_ptr_y{0.0f};

// PADClamp (src/lib/Padclamp.c) : ClampRegion, stick gauche = min, max, xy aux octets 2..4.
constexpr nj::runtime::GameAddr kClampRegion {0x8024DC30};
constexpr uint8_t kClampGc[10] = {30, 180, 15, 72, 40, 15, 59, 31, 56, 44};

// Ouvre (Wii : pas de zone morte ici, plage 127, xy = max donc pas d'octogone) ou rend la table
// du GameCube. Ecrit seulement si la table est bien celle attendue a cette adresse.
void clamp_wide(bool wide) {
    static int state = -1;
    if (!g_guest || state == (int)wide)
        return;
    MMU& m = g_guest->mmu;
    for (int i = 0; i < 10; ++i) {
        if (i >= 2 && i <= 4)
            continue;
        if (m.read8(kClampRegion + i) != kClampGc[i]) {
            state = (int)wide;
            NJ_PRINTF("[Visee Wii] ClampRegion introuvable a %08X : plage du stick inchangee\n",
                        kClampRegion);
            return;
        }
    }
    m.write8(kClampRegion + 2, wide ? 0 : 15);
    m.write8(kClampRegion + 3, wide ? 127 : 72);
    m.write8(kClampRegion + 4, wide ? 127 : 40);
    state = (int)wide;
}

// Amplitude d'un stick GameCube : le jeu attend -128..127.
constexpr int kStickFull = 96;   // un peu en deca du maximum, comme un vrai stick

} // namespace

bool wii_aim_enabled() {
    static const bool forced_off = [] {
        const char* v = NJ_ENV("NJ_AIM_WII");
        return v && v[0] == '0';
    }();
    return !forced_off && nj::runtime::gfx::g_aim_mode == 0;   // menu F10 « Aiming »
}

void wii_pointer_set(bool on, float x, float y) {
    g_ptr_x = x;
    g_ptr_y = y;
    g_ptr_on = on;
}

WiiPointer wii_pointer_get() { return {g_ptr_on.load(), g_ptr_x.load(), g_ptr_y.load()}; }

bool pc_aim_enabled() { return nj::runtime::gfx::g_aim_mode == 2; }

namespace {
std::mutex g_pc_mtx;
float g_pc_dx = 0.0f, g_pc_dy = 0.0f;
}

void pc_mouse_add(float dx, float dy) {
    std::lock_guard<std::mutex> l(g_pc_mtx);
    g_pc_dx += dx;
    g_pc_dy += dy;
}

void pc_mouse_take(float& dx, float& dy) {
    std::lock_guard<std::mutex> l(g_pc_mtx);
    dx = g_pc_dx;
    dy = g_pc_dy;
    g_pc_dx = g_pc_dy = 0.0f;
}

namespace {
std::mutex g_over_mtx;
float g_over_x = 0.0f, g_over_y = 0.0f;
std::atomic<bool> g_on_target{false};
} // namespace

void wii_pointer_add_overflow(float ox, float oy) {
    std::lock_guard<std::mutex> lk(g_over_mtx);
    g_over_x += ox;
    g_over_y += oy;
}

void wii_pointer_take_overflow(float& ox, float& oy) {
    std::lock_guard<std::mutex> lk(g_over_mtx);
    ox = g_over_x;
    oy = g_over_y;
    g_over_x = g_over_y = 0.0f;
}

void wii_pointer_set_target(bool on) { g_on_target = on; }

// Visee au doigt (16/10, retour des testeurs : « la visee sent l'emulateur »). Le doigt qui glisse pendant la visee
// pousse le meme pointeur que la souris, en « unites de souris » : memes regles (vitesse, accroche sur un ennemi,
// tour au bord). La couche tactile cumule ici, la source souris consomme a son tour de lecture.
namespace {
std::mutex g_touch_mtx;
float g_touch_dx = 0.0f, g_touch_dy = 0.0f;
std::atomic<bool> g_touch_aiming{false};
} // namespace
void wii_touch_add(float dx, float dy) {
    std::lock_guard<std::mutex> lk(g_touch_mtx);
    g_touch_dx += dx;
    g_touch_dy += dy;
}
void wii_touch_set_aiming(bool on) { g_touch_aiming = on; }
static void wii_touch_take(float& dx, float& dy) {
    std::lock_guard<std::mutex> lk(g_touch_mtx);
    dx = g_touch_dx;
    dy = g_touch_dy;
    g_touch_dx = g_touch_dy = 0.0f;
}

namespace {
std::atomic<uint32_t> g_hit_ms{0};
}
void wii_pointer_set_hit(uint32_t ms) { g_hit_ms = ms ? ms : 1; }
uint32_t wii_pointer_hit() { return g_hit_ms.load(); }
uint32_t wii_now_ms() {
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
bool wii_pointer_target() { return g_on_target.load(); }

void KbmSource::poll(PadState pads[4]) {
    // Menu F10 ouvert : la souris et le clavier sont a lui, le jeu recoit une manette au repos.
    if (nj::runtime::gfx::g_menu_open) {
        int ddx, ddy;
        mouse_take_motion(&ddx, &ddy);
        pads[0] = PadState{};
        return;
    }
    // Boutons et position pris sur les evenements : SDL_GetMouseState
    // dependait du focus et du mode de capture, et rendait un masque vide.
    int mx = 0, my = 0;
    mouse_position(&mx, &my);
    const uint32_t mb = mouse_buttons();
    const Uint8* keys = SDL_GetKeyboardState(nullptr);

    pads[0].err = 0;

    float sx = 0.f, sy = 0.f, cx = 0.f, cy = 0.f;
    bool aim_by_mouse = false;

    int tl = 0, tr = 0;

    size_t nact = 0;
    const Action* acts = actions_table(&nact);
    // Quelles liaisons se sont declenchees. Sans ce detail, un bouton qui
    // clignote ne dit pas *d'ou* il vient, et on corrige au hasard.
    static std::vector<const char*> dbg_names;
    dbg_names.clear();

    // NJ_FAKEAIM=1 : maintient la visee comme si le clic droit etait
    // enfonce, des que le jeu dessine une scene. Le clic synthetise depuis
    // PowerShell n'atteignait la fenetre qu'une fois sur trois - un test qui
    // echoue silencieusement est pire que pas de test. Ici il n'y a ni focus
    // ni fenetre dans la boucle.
    // NJ_FAKEAIM=1 : maintient la visee des que le jeu dessine une scene,
    // sans dependre du focus de la fenetre. Utilise la **meme** detection que
    // le reste du runtime : la copie locale qu'il y avait ici n'a jamais
    // arme, et son silence ressemblait a un resultat.
    const bool fake_aim = NJ_ENV("NJ_FAKEAIM") != nullptr && game_in_scene();

    for (const Binding& b : bindings_get()) {
        bool down = false;
        if (fake_aim && b.action >= 0 && (size_t) b.action < nact &&
            acts[b.action].target == Target::TriggerR)
            down = true;
        switch (b.src.kind) {
        case Source::Key:
            down = keys && keys[b.src.code] != 0;
            break;
        case Source::MouseButton:
            down = (mb & SDL_BUTTON(b.src.code)) != 0;
            break;
        default:
            // La molette arrive par evenement, pas par etat : elle sera
            // branchee quand la boucle d'evenements la relaiera ici. La
            // liaison existe deja dans la table, donc rien a changer alors.
            break;
        }
        if (!down || b.action < 0 || (size_t) b.action >= nact)
            continue;
        nj::runtime::gfx::g_last_device = 0;   // comme LastUsedDevice de l'UHD
        if (dbg_names.size() < 12)
            dbg_names.push_back(acts[b.action].name);

        const Action& a = acts[b.action];
        switch (a.target) {
        case Target::Button:    pads[0].buttons |= a.bits; break;
        case Target::StickX:    sx += a.sign; break;
        case Target::StickY:    sy += a.sign; break;
        case Target::SubStickX: cx += a.sign; break;
        case Target::SubStickY: cy += a.sign; break;
        case Target::TriggerL:  tl = 255; pads[0].buttons |= a.bits; break;
        case Target::TriggerR:  tr = 255; pads[0].buttons |= a.bits; break;
        }
    }

    // --- Visee a la souris -------------------------------------------------
    //
    // Ce qui manquait vraiment. L'entree de mise en joue arrivait deja intacte
    // (mesure NJ_INPUTDBG : boutons=0020, la gachette R que
    // Key_type_tbl[0][4] attend), mais la **direction** de la visee se prenait
    // au stick, donc aux touches de deplacement. Sur un clavier c'est
    // injouable.
    //
    // RE4 deplace le reticule a une vitesse proportionnelle a l'inclinaison du
    // stick. Un deplacement de souris est deja une vitesse : on le convertit
    // donc en inclinaison, avec une remanence.
    //
    // La remanence n'est pas un effet de style. update() tourne a ~62 Hz et le
    // jeu lit sa manette a ~20 : sans elle, deux lectures sur trois tomberaient
    // entre deux mouvements et la visee avancerait par a-coups.
    {
        // NJ_MOUSE_SENS est un **multiplicateur**, le meme que pour la
        // rotation du corps (mouse_turn.cpp, 1.0 par defaut). Il etait lu
        // ici comme un gain par pixel, par defaut 0,020 : jouer.ps1 posant
        // toujours 1.0, la visee recevait un gain 50 fois trop fort. Un seul
        // pixel mettait le stick en butee, sur les deux axes a la fois -
        // mesure (visee_auto.ps1, 6 px en X et 2 en Y) : stick=(67,-67) puis
        // 49, 35, 25, et de nouveau 67. Le reticule avancait par a-coups et
        // toujours en diagonale : c'est « la visee saute ».
        constexpr float kAimGainPerPixel = 0.020f;
        static const float sens = [] {
            const char* e = NJ_ENV("NJ_MOUSE_SENS");
            const float v = e ? (float) std::atof(e) : 0.0f;
            return kAimGainPerPixel * (v > 0.0f ? v : 1.0f);
        }();
        static const float inv = NJ_ENV("NJ_AIM_INV") ? -1.0f : 1.0f;
        static float ax = 0.f, ay = 0.f;
        static bool grabbed = false;

        int dx = 0, dy = 0;
        mouse_take_motion(&dx, &dy);
        // NJ_FAKEPTR=<s> (essais) : apres s secondes, visee tenue et souris simulee qui promene le
        // curseur (cercle de 6 s, puis une poussee au-dela du bord droit), sans toucher a la vraie.
        static const char* fake_env = NJ_ENV("NJ_FAKEPTR");
        static const uint32_t fake_t0 = SDL_GetTicks();
        bool fake_ptr = false;
        if (fake_env && SDL_GetTicks() - fake_t0 > (uint32_t) std::atoi(fake_env) * 1000u) {
            fake_ptr = true;
            const float t = (float) (SDL_GetTicks() - fake_t0) / 1000.0f;
            dx = (int) std::lround(60.0f * std::cos(t * 1.047f));
            dy = (int) std::lround(40.0f * std::sin(t * 1.047f));
            if (std::fmod(t, 12.0f) > 9.0f)
                dx = 90;
        }

        // Rotation directe dans la structure du joueur, a la maniere de
        // re4_tweaks : la souris n'est plus bornee par l'amplitude du stick.
        // Quand elle est active, elle remplace le pilotage du stick en X.
        {
            static Uint64 last = 0;
            const Uint64 now = SDL_GetPerformanceCounter();
            const double freq = (double) SDL_GetPerformanceFrequency();
            const float dt = last ? (float) ((now - last) / freq) : 0.0f;
            last = now;
            mouse_turn_apply(dx, dy, dt, tr == 255 || fake_ptr);
        }

        // Visee au doigt : seulement avec le pointeur Wii (la visee GameCube garde le stick tactile).
        float touch_dx = 0.0f, touch_dy = 0.0f;
        wii_touch_take(touch_dx, touch_dy);
        const bool touch_aim = wii_aim_enabled() && g_touch_aiming.load();
        const bool aiming = (tr == 255) || fake_ptr || touch_aim;
        if (fake_ptr) {
            tr = 255;
            pads[0].buttons |= 0x0020;
        }

        // La capture de souris ne doit **pas** suivre l'etat de visee.
        //
        // Premiere version : on basculait SDL_SetRelativeMouseMode a chaque
        // entree et sortie de visee. Cela creait une boucle qui s'entretenait
        // toute seule - le changement de mode perturbe la lecture du bouton,
        // le bouton parait relache, la visee tombe, on repasse en mode
        // absolu, le bouton est relu enfonce, et ainsi de suite. Mesure : sur
        // huit captures en six secondes, Leon immobile, arme levee deux fois
        // et baissee six. C'est le "il annule son tir", et il venait de moi.
        //
        // La capture est donc posee **une seule fois** et ne bouge plus.
        // --- Capture de la souris : la cause racine probable -------------
        //
        // Deux symptomes rapportes ensemble : "la souris dirige bien" mais
        // les boutons ne passent pas. Ils viennent de deux fonctions SDL qui
        // ne reagissent pas pareil quand le curseur sort de la fenetre -
        // le deplacement relatif continue d'etre rendu, l'etat des boutons
        // tombe a zero. Donc la capture echouait, le curseur partait, et les
        // clics atterrissaient sur le bureau.
        //
        // Mesure : clic droit reel maintenu 25 s, boutons=0000 sur 38 a 59
        // lectures par fenetre. Et l'ancienne version marquait la capture
        // comme acquise **meme quand SDL la refusait**, donc sans reprise :
        // la panne devenait definitive.
        //
        // On retente donc tant que ca echoue, avec un repli qui enferme au
        // moins le curseur dans la fenetre pour que les clics y tombent.
        // Mobile (13/10) : jamais de capture. Sous Android, le mode relatif demande la capture du
        // pointeur, qui cache le curseur (emulateur, souris branchee) et fausse les touchers que SDL
        // traduit en souris pour le menu des reglages. Le curseur du systeme reste donc visible quand
        // on utilise une souris, absent au doigt.
#ifdef NJ_MOBILE
        grabbed = true;
#endif
        if (!grabbed) {
            if (SDL_Window* w = SDL_GetKeyboardFocus()) {
                const bool rel = SDL_SetRelativeMouseMode(SDL_TRUE) == 0;
                if (!rel)
                    SDL_SetWindowGrab(w, SDL_TRUE);
                grabbed = true;
                ax = ay = 0.f;
                NJ_PRINTF("[Souris] capture %s%s\n",
                            rel ? "relative" : "confinee (mode relatif refuse : ",
                            rel ? "" : SDL_GetError());
                std::fflush(stdout);
            }
        }

        // Visee Wii : la souris est la Wiimote. Le pointeur est un point de l'image (-1..1),
        // deplace au pixel pres (un point de souris = un pixel de l'image affichee), remis au
        // centre a chaque mise en joue et borne en hauteur a la bande entre les bandes noires
        // (y 56..392 sur 448, 0x8012A290). Le jeu en tire la pose, le reticule et la ligne de
        // tir (src/hle/wii_aim.cpp) ; le stick reste au clavier, comme le Nunchuk.
        // NJ_WII_SENS multiplie la vitesse du pointeur (1 par defaut).
        static float px = 0.f, py = 0.f;
        static bool was_aiming = false;
        if (!wii_aim_enabled()) {
            // Visee GameCube (ou bascule dans F10 en pleine visee) : plus de pointeur, et la table
            // du stick du GameCube revient (clamp_wide ne reecrit qu'au changement).
            wii_pointer_set(false, px, py);
            // Visee GameCube « Fast » (14/10) : en joue, plage du stick ouverte (jusqu'a 2x plus vite).
            clamp_wide(nj::runtime::gfx::g_aim_mode == 1 && nj::runtime::gfx::g_stick_aim > 0 &&
                       nj::runtime::input::re4_aiming());
        }
        if (pc_aim_enabled()) {
            // Visee PC de l'UHD : en joue, le deplacement brut va a PlWepLockCtrl (wii_aim.cpp),
            // pas au stick. Echelle de re4_tweaks (UseRawMouseInput) : X = brut / 10 x MOUSE_SENS,
            // Y = -brut / 7 x MOUSE_SENS, a 60 i/s avec deltaTime 0,5 ; a nos 30 i/s de logique
            // (deltaTime 1) : X = brut x S / 20, Y = -brut x S / 14. S = 15 (MOUSE_SENS de
            // l'utilisateur, usr_input.ini) x [input] mouse_sensitivity.
            static bool pc_prev_aim = false;
            if (aiming) {
                const float S = 15.0f * nj::runtime::Config::get().mouse_sensitivity;
                pc_mouse_add((float) dx * S / 20.0f, -(float) dy * S / 14.0f * inv);
            } else if (pc_prev_aim) {
                float a, b;
                pc_mouse_take(a, b);                      // rien ne reste d'une visee precedente
            }
            pc_prev_aim = aiming;
        } else if (wii_aim_enabled()) {
            static const float wsens = [] {
                const char* e = NJ_ENV("NJ_WII_SENS");
                const float v = e ? (float) std::atof(e) : 0.0f;
                return v > 0.0f ? v : 1.0f;
            }();
            // Comme la Wiimote, le pointeur bouge tout le temps (curseur gris hors visee) et ne
            // revient pas au centre a la mise en joue : il est deja la ou l'on pointe.
            {
                int ww = 1280, wh = 720;
                if (SDL_Window* win = SDL_GetKeyboardFocus())
                    SDL_GetWindowSize(win, &ww, &wh);
                const touch::Rect r = touch::game_rect(ww > 0 ? ww : 1, wh > 0 ? wh : 1);
                // Accroche (UHD Friction) : sur un ennemi, le curseur ralentit, il n'est jamais attire.
                const auto& cf = nj::runtime::Config::get();
                const float fr = (aiming && wii_pointer_target())
                                     ? std::max(0.0f, 1.0f - (float) cf.wii_friction / 100.0f) : 1.0f;
                const float k = wsens * cf.wii_sensitivity * fr;
                // Vitesse du mod UHD : 0,15 point de l'ecran Wii (512 x 448) par unite de JOY, et
                // (0,5 + MOUSE_SENS/10). Mais JOY n'est pas la souris brute : re4_tweaks y met
                // brut/10 x MOUSE_SENS x 0,5 / deltaTime (0,5 a 60 i/s), soit 1,5 x brut au reglage
                // 15 de l'utilisateur, et le facteur de sensibilite vaut alors 2 : 0,45 point par
                // unite brute. En 0,15 le curseur natif etait trois fois plus lent.
                (void) r;
                constexpr float kUhdPerCount = 0.15f * 1.5f * 2.0f;
                const float bx = px + ((float) dx + touch_dx) * k * kUhdPerCount / 256.0f;
                const float by = py + ((float) dy + touch_dy) * k * inv * kUhdPerCount / 224.0f;
                px = bx < -1.f ? -1.f : (bx > 1.f ? 1.f : bx);
                py = by < -0.75f ? -0.75f : (by > 0.75f ? 0.75f : by);
                // Au bord, ce qui depasse fait tourner Leon (UHD EdgeTurn) : wii_aim.cpp le consomme.
                if (aiming && cf.wii_edge_turn && (bx != px || by != py))
                    wii_pointer_add_overflow(bx - px, by - py);
            }
            was_aiming = aiming;
            wii_pointer_set(aiming, px, py);
            // Nunchuk : stick +-127 sans octogone (KPAD 0,05 / 0,95) pendant la visee.
            clamp_wide(aiming);
        } else if (aiming) {
            ax += (float) dx * sens;
            ay -= (float) dy * sens * inv;
            ax = ax < -1.f ? -1.f : (ax > 1.f ? 1.f : ax);
            ay = ay < -1.f ? -1.f : (ay > 1.f ? 1.f : ay);
            // La souris prend la main sur les touches : en visee, ZQSD ne doit
            // pas tirer le reticule en meme temps que le poignet.
            // En visee, la souris pilote le reticule par le stick : c'est
            // ainsi que le jeu attend cette commande. La rotation directe du
            // corps ne sert que hors visee.
            sx = ax;
            sy = ay;
            aim_by_mouse = true;
            ax *= 0.72f;
            ay *= 0.72f;
        }
    }

    // Deux touches opposees enfoncees s'annulent, et une diagonale garde la
    // meme amplitude qu'une direction seule - sinon on court plus vite en
    // biais, ce qui se sent tout de suite a la manette.
    auto apply = [](float x, float y, int8_t& ox, int8_t& oy) {
        const float len = std::sqrt(x * x + y * y);
        if (len > 1.0f) { x /= len; y /= len; }
        ox = (int8_t) (x * kStickFull);
        oy = (int8_t) (y * kStickFull);
    };
    apply(sx, sy, pads[0].stick_x, pads[0].stick_y);
    apply(cx, cy, pads[0].substick_x, pads[0].substick_y);

    // Zone morte du GameCube, compensee **par axe**. src/lib/Padclamp.c :
    // ClampStick(x, y, max=72, xy=40, min=15) met chaque axe a zero sous 15,
    // independamment de l'autre, puis retire 15. Une souris n'a pas de zone
    // morte : on envoie 15 + amplitude x 72, et le jeu recoit exactement
    // amplitude x 72 - lineaire, sans trou pour les gestes fins, direction
    // conservee. Sans cela, un petit deplacement vertical pendant un grand
    // horizontal etait efface par l'axe.
    if (aim_by_mouse) {
        float x = sx, y = sy;
        const float len = std::sqrt(x * x + y * y);
        if (len > 1.0f) { x /= len; y /= len; }
        auto past_deadzone = [](float a) -> int8_t {
            const float m = std::fabs(a) * 72.0f;
            if (m < 0.05f)
                return 0;
            int v = (int) std::lround(m);
            if (v < 1) v = 1;
            if (v > 72) v = 72;
            return (int8_t) (a < 0.0f ? -(15 + v) : (15 + v));
        };
        pads[0].stick_x = past_deadzone(x);
        pads[0].stick_y = past_deadzone(y);
    }

    pads[0].trigger_l = (uint8_t) tl;
    pads[0].trigger_r = (uint8_t) tr;

    // NJ_INPUTDBG=1 : dit ce que la manette presentee au jeu contient, des
    // qu'elle change. Sans ca, "on ne peut pas viser" est indiscernable de
    // "la touche n'arrive pas", "le bit est faux" ou "le jeu l'ignore" - et
    // on corrige au hasard. Le bit 0x20 est la gachette R, c'est-a-dire la
    // mise en joue : Key_type_tbl[0][4] = 0x00000020 dans src/game/pad.cpp
    // du decompile, et pl_wep.h fait keyKamae() = (Key.on >> 4) & 1.
    static const bool dbg = NJ_ENV("NJ_INPUTDBG") != nullptr;
    if (dbg) {
        static uint16_t last_b = 0xFFFF;
        static int8_t last_x = 127, last_y = 127;
        if (pads[0].buttons != last_b || pads[0].stick_x != last_x ||
            pads[0].stick_y != last_y) {
            last_b = pads[0].buttons;
            last_x = pads[0].stick_x;
            last_y = pads[0].stick_y;
            NJ_PRINTF("[Input] boutons=%04X%s stick=(%d,%d) gachettes=(%d,%d) souris=%02X",
                   pads[0].buttons, (pads[0].buttons & 0x20) ? " VISEE(R)" : "",
                   pads[0].stick_x, pads[0].stick_y, tl, tr, mb);
            for (const char* n : dbg_names)
                NJ_PRINTF(" %s", n);
            NJ_PRINTF("\n");
            fflush(stdout);
        }
    }
}

} // namespace nj::runtime::input
