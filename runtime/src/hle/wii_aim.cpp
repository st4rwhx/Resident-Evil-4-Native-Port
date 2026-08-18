// Visee au pointeur de la Wii Edition, portee sur le disque GameCube (souris sur PC).
//
// Deux fonctions du jeu passent par ici ([hle_wraps] de re4_config.toml, tools/apply_wraps.py) :
//
//   PlWepLockCtrl        0x8017A154   la pose de visee (lacet x400, tangage m3r, taille, rotation)
//   cObjWep::getMarkerPos 0x800443B4  la ligne du laser et du tir
//
// Tant que la souris ne vise pas (manette, clavier seul, NJ_AIM_WII=0), le code d'origine
// tourne tel quel. Sinon on execute ce que fait le DOL Wii quand une Wiimote + Nunchuk est
// branchee (type de manette 1, [r13-0x680C]) - desassemble, pas devine :
//
// Curseur (0x8012A290) : le pointeur KPAD pos.x/pos.y (-1..1) devient un point de l'ecran
// 512 x 448, x = 256 + 256 * pos.x borne a 0..512, y = 224 + 224 * pos.y borne a 56..392 (la
// bande entre les bandes noires). Ici pos vient de la souris (wii_pointer_get).
//
// Pose (0x8013B530, appelee en tete de PlWepLockCtrl, 0x8013AF58) :
//   - repCtr (partage avec la visee GameCube) monte a 7 tant que le stick du Nunchuk bouge ;
//   - rien hors mise en joue (joyKamae / joyLKamae) ;
//   - le stick en Y deplace un decalage de tangage a part (un second m3r, 0x8034D268), le stick
//     en X fait tourner Leon (rot.y) en continu : c'est le Nunchuk qui tourne, pas le bord ;
//   - le lacet x400 et le tangage viennent du curseur, en absolu, par une table par arme
//     (0x802BAFC4, 6 flottants) : x 0..512 -> +0,55..-0,65 rad, y -> +0,4..-0,48 ;
//   - PlWepLockRand(pl, 1, ...) : memorise sans tremblement ;
//   - m3r.set(tangage), lissage m3r.blend = 0,65 - 0,1 * |ecart| / plage ;
//   - (0x8013C538) m3r.update, mot3.move, taille vers x400 a 0,4 - 0,105 * ecart.
// Les armes sans visee au pointeur (fusils a lunette...) gardent la visee GameCube : table
// 0x802BAF90, et Leon au couteau (xFD 0xF) l'a toujours (0x80139FA8).
//
// Ligne de tir (0x8005EB40) : hors lunette, la ligne part de la camera (Cam.param.pos) et passe
// par le curseur (0x8008C75C : ecran -> repere camera, focale 240 / tan(fovy / 2), puis
// PSMTXMultVecSR par Cam.mat), longueur inchangee. Le reticule et les balles suivent le curseur.
//
// Structure du joueur : la Wii ajoute 0x24 octets avant x400 (Wii 0x424 = GC 0x400, 0x7B4 =
// pWaist 0x790), et 0x10 dans Global (wep_no 0x4FC0 = GC 0x4FB0). La nuque (0x801350F0, des
// champs que la version GameCube de cPlNeck n'a pas) n'est pas reproduite.

#include "input/wii_pointer.h"
#include "runtime/game_version.h"
#include "runtime/config.h"
#include "runtime/gfx_settings.h"
#include "runtime/cpu_context.h"

#include <chrono>
#include <cmath>
#include <cstdint>

using nj::runtime::GuestCpu;
namespace nj::runtime { extern GuestCpu* g_guest; }   // main.cpp
using GuestFn = void (*)(GuestCpu&);

extern "C" {
void nj_dispatch_one(GuestCpu& cpu);   // main_output.cpp (recompilateur)
}

namespace {

constexpr nj::runtime::GameAddr kPG {0x80314BC8};       // Global* pG
constexpr nj::runtime::GameAddr kPSys {0x80314BCC};     // SystemWork* pSys (flags +0, bit 31 : visee inversee)
constexpr nj::runtime::GameAddr kPPL {0x803159F4};      // cPlayer* pPL
constexpr nj::runtime::GameAddr kJoy {0x802E8B30};      // JOY Joy[0] : sx +0, sy +1
constexpr nj::runtime::GameAddr kRepCtr {0x80314D5C};   // static f32 repCtr de PlWepLockCtrl
constexpr nj::runtime::GameAddr kM3r {0x8027692C};      // f32 m3r[3] : actuel, cible, lissage
constexpr nj::runtime::GameAddr kMot3 {0x80276844};     // cMot3 mot3
constexpr nj::runtime::GameAddr kFnLockRand {0x8017A7C0};
constexpr nj::runtime::GameAddr kFnMot3Move {0x80175A7C};
constexpr nj::runtime::GameAddr kFnKamae {0x80178494};
constexpr nj::runtime::GameAddr kFnLKamae {0x801785DC};
constexpr nj::runtime::GameAddr kFnWepTargetPos {0x800F3C78};
constexpr uint32_t kReturn = 0xFFFFFFF0;   // adresse de retour factice des appels au jeu

constexpr float kPi = 3.14159274f;

// Table 0x802BAFC4 : lacet au centre, tangage au centre, lacet au bord droit, au bord gauche,
// tangage en haut, en bas. Identique pour toutes les armes sauf ces exceptions.
struct AimRange {
    float yaw0, pitch0, yawR, yawL, pitchTop, pitchBot;
};
AimRange aim_range(int wep) {
    AimRange r{0.0f, 0.0f, -0.649999976f, 0.550000012f, -0.400000006f, 0.479999989f};
    if (wep == 0) {
        r.pitchTop = -0.349999994f;
    } else if (wep >= 1 && wep <= 4) {
        r.pitchTop = -0.349999994f;
        r.pitchBot = 0.400000006f;
    } else if (wep == 5 || wep == 6) {
        r.pitchTop = -0.239999995f;
    } else if (wep == 7) {
        r.yawR = -0.550000012f;
        r.yawL = 0.649999976f;
        r.pitchTop = -0.280000001f;
    } else if (wep == 8) {
        r.pitchTop = -0.280000001f;
        r.pitchBot = 0.400000006f;
    } else if (wep == 0xE) {
        r.yaw0 = 0.119999997f;
    } else if (wep == 0x21) {
        r.pitchTop = -0.280000001f;
    }
    return r;
}

// Table 0x802BAF90 : armes visees au pointeur.
bool pointer_weapon(int wep) {
    static const uint8_t t[0x34] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1,
                                    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0,
                                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0};
    return wep >= 0 && wep < 0x34 && t[wep];
}

// Vitesses de PlWepLockCtrl (spd : rotation, spd2 : tangage), memes valeurs sur Wii.
void weapon_speeds(int wep, int type, float& spd, float& spd2) {
    spd = spd2 = 1.0f;
    switch (wep) {
    case 4: spd = 1.5f; spd2 = 1.2f; break;
    case 7: spd = spd2 = 0.8f; break;
    case 8: spd = spd2 = 0.9f; break;
    case 0x21: spd = spd2 = 1.04f; break;
    case 0xB: case 0xC: case 0x1B: case 0x1D: case 0x27: spd = spd2 = 1.15f; break;
    case 0xE: case 0x13:
        if (type == 0)
            break;
        [[fallthrough]];
    case 0x16: case 0x17: case 0x19: case 0x29: spd = 1.2f; spd2 = 1.5f; break;
    case 0x10: spd = spd2 = 1.2f; break;
    default: break;
    }
}

bool heap_ptr(uint32_t p) { return p >= 0x80000000u && p < 0x81800000u; }

// Appel d'une fonction recompilee du jeu, depuis le milieu d'une autre : cpu.pc a l'entree,
// retour factice, et le lr du crochet rendu ensuite.
//
// La fonction recompilee peut rendre la main avant d'avoir fini : quand elle appelle du code
// d'un module REL (joyKamae -> pObj->keyKamae(), methode virtuelle de l'arme, dans le module
// de l'arme), elle revient avec cpu.pc sur ce code pour que le repartiteur le fasse
// interpreter. Continuer ici comme si l'appel etait termine laissait r1 dans le cadre de
// l'appele (joyKamae : stwu r1,-8), et la fonction REL qui nous avait appeles relisait son
// adresse de retour 8 octets trop bas : saut dans les donnees des que la visee tenait.
// On termine donc l'appel instruction par instruction jusqu'au retour factice.
void call(GuestCpu& cpu, uint32_t addr) {
    const uint32_t lr = cpu.lr;
    cpu.lr = kReturn;
    cpu.pc = addr;
    // Interruptions masquees le temps de l'appel : un changement de fil au milieu nous ferait
    // executer un autre fil ici.
    const uint32_t msr = cpu.msr;
    cpu.msr &= ~0x8000u;
    nj_dispatch_one(cpu);   // fonction a addr, trouvee par le repartiteur (P3 : pas de nom)
    for (int guard = 0; cpu.pc != kReturn && cpu.alive && guard < 2000000; ++guard)
        nj_dispatch_one(cpu);
    cpu.msr = (cpu.msr & ~0x8000u) | (msr & 0x8000u);
    cpu.lr = lr;
}

// Curseur Wii (512 x 448) depuis le pointeur de la souris.
void cursor(float& cx, float& cy) {
    const nj::runtime::input::WiiPointer p = nj::runtime::input::wii_pointer_get();
    cx = std::fmin(512.0f, std::fmax(0.0f, 256.0f + 256.0f * p.x));
    cy = std::fmin(392.0f, std::fmax(56.0f, 224.0f + 224.0f * p.y));
}

bool pointer_active() {
    return nj::runtime::input::wii_aim_enabled() && nj::runtime::input::wii_pointer_get().on;
}

// Decalage de tangage du stick (second m3r de la Wii) : actuel, cible ; lissage nul.
float g_stick_pitch = 0.0f;

// ---------------------------------------------------------------- repris du mod UHD
// (mod_voiture/uhd/visee_wii_uhd.cpp, mesures du 26/09 sur l'UHD, meme code de jeu).

uint32_t now_ms() {
    using namespace std::chrono;
    // Oracle CPU (NJ_DETERMINISTIC) : l'heure de la visee suit l'horloge emulee (base de temps,
    // compteur d'instructions avec NJ_INSTTB), jamais l'heure hote.
    static const bool det = NJ_ENV("NJ_DETERMINISTIC") != nullptr;
    if (det) {
        if (GuestCpu* c = nj::runtime::g_guest)
            return (uint32_t)(c->timebase_now() / (c->timebase_hz / 1000));
    }
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Camera de l'image que le joueur voit hors visee : la routine de visee ne demarre qu'apres
// l'animation de mise en joue, quand la camera a deja bouge (le rayon lance alors visait 15 deg
// trop bas). Copiee par nj_wii_frame tant que Leon ne vise pas.
struct CamCopy {
    float mat[12], pos[3], fovy, cx, cy;
    bool ok;
} g_cam_before{};

bool g_new_aim = false;            // nj_wii_frame : Leon vient de passer a l'etat vise (xFD 6)
uint32_t g_pose_ms = 0;            // derniere image ou la pose Wii a tourne (getPitch, laser)
bool g_push_y = false;             // le joueur pousse en hauteur : la trajectoire rend la main

// Point garde (KeepPoint) : a la mise en joue, le point du monde sous le curseur ; le tangage de
// visee glisse vers lui en 0,3 s (1 - (1-t)^3), 0,65 s au plus.
bool g_point_on = false, g_point_posed = false, g_pitch_done = false;
float g_point[3];
uint32_t g_point_t = 0;
float g_pitch_from = 0.0f, g_pitch_to = 0.0f;

// Rayon camera -> curseur (ecran 512 x 448) contre le decor et les ennemis : GetWepTargetPos
// (0x800F3C78), *p1 recoit le point touche. Rend son resultat (0 rien, 1 decor, 2/3 ennemi),
// -1 si le rayon n'a pu etre construit. far : le point a 30 m sur le rayon.
int ray(GuestCpu& cpu, const float* mat, const float* pos, float fovy, float cx, float cy, int wep,
        float* P, float* far, uint32_t* em) {
    auto& m = cpu.mmu;
    const float h = fovy * 0.5f * kPi / 180.0f;
    if (h <= 0.0f || h >= 1.5f)
        return -1;
    const float v[3] = {(cx - 256.0f) * (640.0f / 512.0f), -(cy - 224.0f) * (480.0f / 448.0f),
                        -240.0f * std::cos(h) / std::sin(h)};
    float w[3], n = 0.0f;
    for (int i = 0; i < 3; ++i) {
        w[i] = mat[i * 4] * v[0] + mat[i * 4 + 1] * v[1] + mat[i * 4 + 2] * v[2];
        n += w[i] * w[i];
    }
    n = std::sqrt(n);
    if (n <= 0.0f)
        return -1;
    // Vecteurs sous la pile du jeu : r1 descend pour que l'appele ne les ecrase pas.
    const uint32_t r1 = cpu.gpr[1];
    const uint32_t sp = (r1 - 0x100) & ~0xFu;
    const uint32_t p0a = sp + 0x40, p1a = sp + 0x50, ema = sp + 0x60, attra = sp + 0x64;
    for (int i = 0; i < 3; ++i) {
        m.write_f32(p0a + i * 4, pos[i]);
        m.write_f32(p1a + i * 4, pos[i] + w[i] / n * 50000.0f);
        far[i] = pos[i] + w[i] / n * 30000.0f;
    }
    m.write32(ema, 0);
    m.write32(attra, 0);
    cpu.gpr[1] = sp;
    cpu.gpr[3] = p0a;
    cpu.gpr[4] = p1a;
    cpu.gpr[5] = 0;
    cpu.gpr[6] = (uint32_t)wep;
    cpu.gpr[7] = ema;
    cpu.gpr[8] = attra;
    call(cpu, kFnWepTargetPos);
    const int res = (int)cpu.gpr[3];
    cpu.gpr[1] = r1;
    for (int i = 0; i < 3; ++i)
        P[i] = m.read_f32(p1a + i * 4);
    *em = m.read32(ema);
    return res;
}

// Le point sous le curseur, vu par la camera d'avant la mise en joue. Un impact plus proche que
// Leon + 1,5 m (curseur pres de lui, camera derriere) est remplace par un point lointain.
bool point_of_cursor(GuestCpu& cpu, int wep, uint32_t pl, float* P) {
    if (!g_cam_before.ok)
        return false;
    const CamCopy& c = g_cam_before;
    float far[3];
    uint32_t em = 0;
    if (ray(cpu, c.mat, c.pos, c.fovy, c.cx, c.cy, wep, P, far, &em) < 0)
        return false;
    float dl = 0.0f, dh = 0.0f;
    for (int i = 0; i < 3; ++i) {
        const float a = cpu.mmu.read_f32(pl + 0x94 + i * 4) - c.pos[i], b = P[i] - c.pos[i];
        dl += a * a;
        dh += b * b;
    }
    const float seuil = std::sqrt(dl) + 1500.0f;
    if (dh < seuil * seuil)
        for (int i = 0; i < 3; ++i)
            P[i] = far[i];
    return true;
}

// Mise en joue : pose le point garde, puis fait glisser le tangage de visee (le decalage du
// Nunchuk, que la camera de visee suit) vers lui. Relations mesurees sur l'UHD (1082 images) :
// tangage camera = 1,051 x tangage Nunchuk - 0,055 ; tangage vise = 1,19 x cible - 0,023 ; le
// curseur reste en place, on retire son angle dans la camera de visee (champ ~45 deg).
void keep_point(GuestCpu& cpu, int wep, uint32_t pl, float cy) {
    const uint32_t now = now_ms();
    if (g_new_aim) {
        g_new_aim = false;
        g_point_on = nj::runtime::Config::get().wii_keep_point && point_of_cursor(cpu, wep, pl, g_point);
        g_point_t = now;
        g_point_posed = false;
        g_pitch_done = false;
        g_pitch_from = g_pitch_to = g_stick_pitch;
    }
    if (!g_point_on)
        return;
    const uint32_t elapsed = now - g_point_t;
    if (elapsed > 650) {
        g_point_on = false;
        return;
    }
    auto& m = cpu.mmu;
    if (!g_point_posed) {
        g_point_posed = true;
        const float dx = g_point[0] - m.read_f32(pl + 0x94), dz = g_point[2] - m.read_f32(pl + 0x9C);
        const float dy = g_point[1] - (m.read_f32(pl + 0x98) + 1500.0f);   // yeux ~1,5 m
        const float hz = std::sqrt(dx * dx + dz * dz);
        if (hz > 100.0f) {
            const float f45 = 240.0f / 0.41421356f;
            const float decal = std::atan2((224.0f - cy) * 480.0f / 448.0f, f45);
            const float tangage = (std::atan2(dy, hz) + 0.023f) / 1.19f - decal;
            g_pitch_to = std::fmin(1.0f, std::fmax(-1.0f, (tangage + 0.055f) / 1.051f));
            g_pitch_from = g_stick_pitch;
        }
    }
    if (g_push_y)
        g_pitch_done = true;
    if (!g_pitch_done) {
        const float t = std::fmin(1.0f, (float)elapsed / 300.0f), u = 1.0f - t;
        g_stick_pitch = g_pitch_from + (g_pitch_to - g_pitch_from) * (1.0f - u * u * u);
    }
}

// Routine 0x8013B530. false : l'arme n'a pas la visee au pointeur (code d'origine).
bool pointer_lock_ctrl(GuestCpu& cpu) {
    auto& m = cpu.mmu;
    const uint32_t g = m.read32(kPG), pl = m.read32(kPPL);
    if (!heap_ptr(g) || !heap_ptr(pl))
        return false;
    const int wep = m.read8(g + nj::runtime::pg(0x4FB0)), type = m.read8(g + nj::runtime::pg(0x4FB1));
    const bool knife = m.read8(pl + 0xFC) == 0 && m.read8(pl + 0xFD) == 0xB;
    if (!knife && !pointer_weapon(wep))
        return false;
    // Le couteau a sa propre routine au pointeur sur Wii (0x8013C3C0) : non portee, on garde
    // la visee GameCube pour lui.
    if (knife)
        return false;

    float spd, spd2;
    weapon_speeds(wep, type, spd, spd2);

    const int sx = (int8_t)m.read8(kJoy), sy = (int8_t)m.read8(kJoy + 1);
    float rep = m.read_f32(kRepCtr);
    if (sx != 0 || sy != 0) {
        if (rep < 7.0f)
            m.write_f32(kRepCtr, rep + 1.0f);
    } else {
        m.write_f32(kRepCtr, 0.0f);
    }
    rep = m.read_f32(kRepCtr);

    call(cpu, kFnKamae);
    bool kamae = cpu.gpr[3] != 0;
    if (!kamae) {
        call(cpu, kFnLKamae);
        kamae = cpu.gpr[3] != 0;
    }
    if (!kamae) {
        g_stick_pitch = 0.0f;
        return true;
    }

    // Stick Y : decalage de tangage.
    const bool invert = (int32_t)m.read32(m.read32(kPSys)) < 0;
    float d = spd2 * (float)sy * rep * 0.15f / 200.0f / 10.0f;
    if (invert)
        d = -d;
    if (g_stick_pitch > 0.0f)
        d *= 0.8f;
    g_stick_pitch = std::fmin(1.0f, std::fmax(-1.0f, g_stick_pitch + d));

    // Stick X : Leon tourne.
    float roty = m.read_f32(pl + 0xA4);
    roty -= spd * (float)sx * rep * kPi / 10.0f / 200.0f / 20.0f;

    // Reprise de la visee UHD (mod_voiture/uhd/visee_wii_uhd.cpp), reglages [wii_aiming] :
    //  - EdgeTurn : ce que la souris pousse au-dela du bord tourne Leon (ou incline la visee) de
    //    l'angle que le curseur aurait parcouru, a la vitesse de la main. Camera de visee : un
    //    point du curseur (ecran 512 x 448) vaut 1,25 x tan(champ/2) / 240 rad en largeur,
    //    1,07 x ... en hauteur ; tangage camera = 1,051 x decalage de tangage (mesure UHD) ;
    //  - EdgeZone : curseur dans la bande du bord, Leon tourne en continu, plus vite au bord.
    {
        const auto& cf = nj::runtime::Config::get();
        float ox = 0.0f, oy = 0.0f;
        bool zone_y_on = false;
        nj::runtime::input::wii_pointer_take_overflow(ox, oy);
        const float h = m.read_f32(g + 0x134) * 0.5f * kPi / 180.0f;
        const float t = (h > 0.0f && h < 1.5f) ? std::tan(h) : 0.4f;
        const float s = (float)cf.wii_turn_sensitivity / 100.0f;
        roty -= ox * 256.0f * 1.25f * t / 240.0f * s;
        // tangage positif = vers le haut ; oy < 0 : pousse au-dessus du bord
        g_stick_pitch -= oy * 224.0f * 1.0714286f * t / 240.0f * s / 1.051f;
        if (cf.wii_edge_zone > 0) {
            float cx, cy;
            cursor(cx, cy);
            const float zx = 512.0f * (float)cf.wii_edge_zone / 100.0f;
            const float zy = 336.0f * (float)cf.wii_edge_zone / 100.0f;
            const float v = 127.0f * (float)cf.wii_edge_zone_speed / 100.0f;
            float zone_x = 0.0f, zone_y = 0.0f;
            if (cx < zx) zone_x = -v * (zx - cx) / zx;
            else if (cx > 512.0f - zx) zone_x = v * (cx - (512.0f - zx)) / zx;
            if (cy < 56.0f + zy) zone_y = v * (56.0f + zy - cy) / zy;          // haut : vers le haut
            else if (cy > 392.0f - zy) zone_y = -v * (cy - (392.0f - zy)) / zy;
            zone_y_on = zone_y != 0.0f;
            roty -= spd * zone_x * 7.0f * kPi / 10.0f / 200.0f / 20.0f;
            g_stick_pitch += spd2 * zone_y * 7.0f * 0.15f / 200.0f / 10.0f;
        }
        g_push_y = oy != 0.0f || zone_y_on || sy != 0;
        g_stick_pitch = std::fmin(1.0f, std::fmax(-1.0f, g_stick_pitch));
    }
    m.write_f32(pl + 0xA4, roty);
    {
        float cx, cy;
        cursor(cx, cy);
        keep_point(cpu, wep, pl, cy);
    }
    g_pose_ms = now_ms();

    // Curseur -> lacet et tangage.
    float cx, cy;
    cursor(cx, cy);
    const AimRange r = aim_range(wep);
    float yaw = cx < 256.0f ? r.yaw0 - (cx - 256.0f) * (r.yawL - r.yaw0) / 256.0f
                            : r.yaw0 + (cx - 256.0f) * (r.yawR - r.yaw0) / 256.0f;
    yaw = std::fmin(1.0f, std::fmax(-1.0f, yaw));
    m.write_f32(pl + 0x400, yaw);

    float pitch;
    if (cy > 256.0f) {
        const float c = std::fmin(cy, 400.0f);
        pitch = r.pitch0 - (c - 40.0f - 180.0f) * (r.pitchBot - r.pitch0) / 180.0f;
    } else {
        const float c = std::fmax(cy, 40.0f);
        pitch = r.pitch0 + (c - 40.0f - 180.0f) * (r.pitchTop - r.pitch0) / 180.0f;
    }
    pitch = std::fmin(1.0f, std::fmax(-1.0f, pitch + g_stick_pitch));
    const float gap = std::fabs(pitch - m.read_f32(kM3r));

    // PlWepLockRand(pl, 1, &pitch, &pl->x400) : la cible de m3r sert de flottant du jeu.
    m.write_f32(kM3r + 4, pitch);
    cpu.gpr[3] = pl;
    cpu.gpr[4] = 1;
    cpu.gpr[5] = kM3r + 4;
    cpu.gpr[6] = pl + 0x400;
    call(cpu, kFnLockRand);
    pitch = m.read_f32(kM3r + 4);

    // m3r.set(pitch), puis son lissage.
    m.write_f32(kM3r + 4, pitch);
    if (m.read_f32(kM3r + 8) == 0.0f)
        m.write_f32(kM3r, pitch);
    float k = 0.65f + (-0.0999999642f * gap) / (r.pitchBot - r.pitchTop);
    k = std::fmin(1.0f, std::fmax(0.0f, k));
    m.write_f32(kM3r + 8, k);

    // 0x8013C538 : m3r.update, mot3.move, taille.
    const float cur = m.read_f32(kM3r) * k + m.read_f32(kM3r + 4) * (1.0f - k);
    m.write_f32(kM3r, cur);
    cpu.gpr[3] = kMot3;
    cpu.fpr[1] = cpu.ps1[1] = cur;
    call(cpu, kFnMot3Move);
    const uint32_t waist = m.read32(pl + 0x790);
    if (heap_ptr(waist)) {
        const float x400 = m.read_f32(pl + 0x400);
        const float wcur = m.read_f32(waist + 4);
        float f = 1.05263162f * (0.099999994f * (x400 - wcur));
        if (f < 0.0f)
            f *= 0.100000001f;
        const float rate = 0.4f - f;
        m.write_f32(waist + 4, wcur * (1.0f - rate) + x400 * rate);
    }

    // m3r.clamp(-1, 1)
    float tgt = m.read_f32(kM3r + 4);
    tgt = std::fmin(1.0f, std::fmax(-1.0f, tgt));
    m.write_f32(kM3r + 4, tgt);
    if (m.read_f32(kM3r + 8) == 0.0f)
        m.write_f32(kM3r, tgt);
    return true;
}


// ---------------------------------------------------------------------------------------------
// Visee PC (RE4 UHD, souris en mode « Modern ») : menu F10 « Aiming » = PC (UHD).
//
// bio4.exe PlWepLockCtrl (0x688090), desassemble et execute dans un emulateur x87 pour en tirer
// les formules (30/09) : c'est la routine GameCube, le stick remplace par le deplacement de la
// souris (entiers 0xC63008 / 0xC6300C, NON bornes a +-127 au clavier-souris), le tout multiplie
// par deltaTime (1 a 30 i/s : notre logique). Par arme, lim / spd / spd2 comme sur GameCube.
//   hauteur : m3r[1] += dY * spd2 * repCtr * 0,15 / 200 / 10 (inversee si pSys bit 31),
//             x 0,8 si m3r[0] > 0, bornee a +-1 ; touches haut/bas : +-0,035 * repCtr / 7 ;
//   « Modern » (octet 0x1222868, clavier-souris) : le bras est fige a -0,8 * lim et LE CORPS
//             tourne : rot.y -= 2 * spd * dX * repCtr * pi / 40000 (le code passe par les deux
//             butees, d'ou le 2) ; touches : rot.y +-= spd * repCtr * pi / 560 ;
//   « Classic » (re4_tweaks DetachCameraFromAim) : lacet x400 -= dX * repCtr * pi / 40000, borne a
//             [-0,8 lim, lim], le corps ne tourne qu'au-dela (comme le GameCube).
//   repCtr : +1 par image (jusqu'a 7) tant qu'il y a une entree, 0 sinon.
// La fin est celle du jeu (pl_wep.cpp) : PlWepLockRand(pl, bouge, &tmp, &x400) - bouge : la main
// memorise le point, sinon elle tremble autour -, lissage de m3r, mot3.move, taille a 0,4.
// Le stick (manette, clavier) s'ajoute a la souris avec l'echelle du GameCube. Couteau et
// verrouillage automatique : routine GameCube d'origine.

constexpr nj::runtime::GameAddr kLockCtr {0x80315B30};   // u8 lockCtr (pl_wep)

float weapon_lim(int wep, int type) {
    switch (wep) {
    case 4: return 0.062831853f;
    case 7: case 0x10: case 0x21: return 0.10471976f;
    case 8: return 0.052359879f;
    case 0xB: case 0xC: case 0x1B: case 0x1D: case 0x27: case 0xD:
    case 0x16: case 0x17: case 0x19: case 0x29: return 0.0065449847f;
    case 0xE: case 0x13: return type == 0 ? 0.20943952f : 0.0065449847f;
    default: return 0.20943952f;
    }
}

bool pc_lock_ctrl(GuestCpu& cpu) {
    auto& m = cpu.mmu;
    const uint32_t g = m.read32(kPG), pl = m.read32(kPPL);
    if (!heap_ptr(g) || !heap_ptr(pl))
        return false;
    if (m.read8(pl + 0xFC) == 0 && m.read8(pl + 0xFD) == 0xB)
        return false;                                         // couteau : routine d'origine
    if (heap_ptr(m.read32(pl + 0x508)) && m.read8(kLockCtr) != 0 && (m.read32(g + 0x68) & 0x40000))
        return false;                                         // verrouillage auto : idem

    const int wep = m.read8(g + nj::runtime::pg(0x4FB0)), type = m.read8(g + nj::runtime::pg(0x4FB1));
    float spd, spd2;
    weapon_speeds(wep, type, spd, spd2);
    const float lim = weapon_lim(wep, type);

    float mdx = 0.0f, mdy = 0.0f;
    nj::runtime::input::pc_mouse_take(mdx, mdy);
    const float dX = mdx + (float)(int8_t)m.read8(kJoy);
    const float dY = mdy + (float)(int8_t)m.read8(kJoy + 1);
    const uint32_t on = m.read32(kJoy + 0x10);

    float rep = m.read_f32(kRepCtr);
    if (dX != 0.0f || dY != 0.0f || (on & 0xF)) {
        if (rep < 7.0f)
            rep += 1.0f;
    } else {
        rep = 0.0f;
    }
    m.write_f32(kRepCtr, rep);

    call(cpu, kFnKamae);
    bool kamae = cpu.gpr[3] != 0;
    if (!kamae) {
        call(cpu, kFnLKamae);
        kamae = cpu.gpr[3] != 0;
    }
    if (!kamae)
        return false;                                         // hors joue : rien a faire ici

    bool moved = false;
    // Hauteur.
    const bool invert = (int32_t)m.read32(m.read32(kPSys)) < 0;
    float d = 0.0f;
    const float key = 0.035f * rep / 7.0f;
    if (on & 8) d += invert ? -key : key;
    if (on & 4) d -= invert ? -key : key;
    const float md = spd2 * dY * rep * 0.15f / 200.0f / 10.0f;
    d += invert ? -md : md;
    if (m.read_f32(kM3r) > 0.0f)
        d *= 0.8f;
    if (d != 0.0f)
        moved = true;
    float t = std::fmin(1.0f, std::fmax(-1.0f, m.read_f32(kM3r + 4) + d));
    m.write_f32(kM3r + 4, t);
    if (m.read_f32(kM3r + 8) == 0.0f)
        m.write_f32(kM3r, t);

    // Lacet / corps.
    const float turn = dX * rep * kPi / 40000.0f;
    float yd = -turn;
    if (on & 2) yd -= 0.03f;
    if (on & 1) yd += 0.03f;
    if (yd != 0.0f)
        moved = true;
    float roty = m.read_f32(pl + 0xA4);
    float x400 = m.read_f32(pl + 0x400);
    const float kstep = spd * rep * kPi / 560.0f;
    if (nj::runtime::gfx::g_aim_detach == 0) {
        x400 = -lim * 0.8f;                                   // « Modern » : camera collee
        roty -= 2.0f * spd * turn;
        if (on & 1) roty += kstep;
        if (on & 2) roty -= kstep;
    } else {
        x400 += yd;
        if (x400 > lim) {
            x400 = lim;
            roty += (on & 1) ? kstep : -spd * turn;
        }
        if (x400 < -lim * 0.8f) {
            x400 = -lim * 0.8f;
            roty += (on & 2) ? -kstep : -spd * turn;
        }
    }
    m.write_f32(pl + 0xA4, roty);
    m.write_f32(pl + 0x400, x400);

    // Fin de PlWepLockCtrl : PlWepLockRand(pl, bouge, &tmp, &x400), tmp = m3r[0] pose dans m3r[1].
    m.write_f32(kM3r + 4, m.read_f32(kM3r));
    cpu.gpr[3] = pl;
    cpu.gpr[4] = moved ? 1 : 0;
    cpu.gpr[5] = kM3r + 4;
    cpu.gpr[6] = pl + 0x400;
    call(cpu, kFnLockRand);
    const float tmp = m.read_f32(kM3r + 4);
    const float k = m.read_f32(kM3r + 8);
    if (k == 0.0f)
        m.write_f32(kM3r, tmp);
    const float cur = m.read_f32(kM3r) * k + tmp * (1.0f - k);
    m.write_f32(kM3r, cur);
    cpu.gpr[3] = kMot3;
    cpu.fpr[1] = cpu.ps1[1] = cur;
    call(cpu, kFnMot3Move);
    const uint32_t waist = m.read32(pl + 0x790);               // pWaist->set(x400, 0,4)
    if (heap_ptr(waist))
        m.write_f32(waist + 4, m.read_f32(waist + 4) * 0.6f + m.read_f32(pl + 0x400) * 0.4f);
    return true;
}

} // namespace

namespace nj::runtime::input {
bool wii_pointer_weapon(int wep_no) { return pointer_weapon(wep_no); }
}

void nj_wii_PlWepLockCtrl(GuestCpu& cpu, GuestFn orig) {
    const uint32_t lr = cpu.lr;
    if (pointer_active() && pointer_lock_ctrl(cpu)) {
        cpu.lr = lr;
        cpu.pc = lr;
        return;
    }
    if (nj::runtime::input::pc_aim_enabled()) {
        // pc_lock_ctrl appelle des fonctions du jeu (joyKamae...) avant de savoir s'il prend la
        // main : s'il la rend, la routine d'origine doit retrouver ses arguments (r3 = pl) et
        // ses registres intacts, sinon elle travaille sur un joueur a l'adresse 0 (plantage au
        // relachement de la visee, 30/09).
        const auto gpr = cpu.gpr;
        const auto fpr = cpu.fpr;
        const auto ps1 = cpu.ps1;
        const auto cr = cpu.cr;
        const uint32_t xer = cpu.xer, fpscr = cpu.fpscr;
        if (pc_lock_ctrl(cpu)) {
            cpu.lr = lr;
            cpu.pc = lr;
            return;
        }
        cpu.gpr = gpr;
        cpu.fpr = fpr;
        cpu.ps1 = ps1;
        cpu.cr = cr;
        cpu.xer = xer;
        cpu.fpscr = fpscr;
        cpu.lr = lr;
    }
    cpu.lr = lr;
    cpu.pc = nj::runtime::ga(0x8017A154);
    orig(cpu);
}

void nj_wii_getMarkerPos(GuestCpu& cpu, GuestFn orig) {
    const uint32_t pos = cpu.gpr[4], at = cpu.gpr[5];
    orig(cpu);
    if (!pointer_active())
        return;
    auto& m = cpu.mmu;
    const uint32_t g = m.read32(kPG);
    if (!heap_ptr(g) || !heap_ptr(pos) || !heap_ptr(at))
        return;
    // Lunettes : la ligne reste celle de la camera de visee (0x8005EB6C).
    const int wep = m.read8(g + nj::runtime::pg(0x4FB0)), type = m.read8(g + nj::runtime::pg(0x4FB1));
    if (wep == 9 || wep == 10 || wep == 0xD || (wep == 0xE && type == 1))
        return;

    const float px = m.read_f32(pos), py = m.read_f32(pos + 4), pz = m.read_f32(pos + 8);
    const float ax = m.read_f32(at) - px, ay = m.read_f32(at + 4) - py, az = m.read_f32(at + 8) - pz;
    const float len = std::sqrt(ax * ax + ay * ay + az * az);

    // 0x8008C75C : point de l'ecran 512 x 448 -> direction dans le repere camera (640 x 480,
    // focale 240 / tan(fovy / 2)), puis Cam.mat (pG + 0x74).
    float cx, cy;
    cursor(cx, cy);
    const float fovy = m.read_f32(g + 0x134) * 0.5f * kPi / 180.0f;
    const float v[3] = {(cx - 256.0f) * (640.0f / 512.0f), -(cy - 224.0f) * (480.0f / 448.0f),
                        -240.0f * std::cos(fovy) / std::sin(fovy)};
    float w[3];
    for (int i = 0; i < 3; ++i) {
        const uint32_t row = g + 0x74 + i * 16;
        w[i] = m.read_f32(row) * v[0] + m.read_f32(row + 4) * v[1] + m.read_f32(row + 8) * v[2];
    }
    const float n = std::sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
    if (n <= 0.0f)
        return;
    const float cam[3] = {m.read_f32(g + 0x118), m.read_f32(g + 0x11C), m.read_f32(g + 0x120)};
    for (int i = 0; i < 3; ++i) {
        m.write_f32(pos + i * 4, cam[i]);
        m.write_f32(at + i * 4, cam[i] + w[i] / n * len);
    }
}

// cPlWep::getPitch (0x8017978C) : le tangage que suit la camera de visee (cam_qfps.cpp,
// calcDepressionRatio). Sur GameCube c'est m3r[0], le tangage des bras ; en mode pointeur la Wii
// rend le decalage du Nunchuk (UHD 0x685550, test 0x68557D) : le curseur bouge les bras, pas la
// camera, qui ne s'incline qu'au stick, au bord ou vers le point garde.
void nj_wii_getPitch(GuestCpu& cpu, GuestFn orig) {
    orig(cpu);
    if (!pointer_active() || now_ms() - g_pose_ms > 150)
        return;
    auto& m = cpu.mmu;
    const uint32_t pl = m.read32(kPPL);
    if (!heap_ptr(pl) || m.read8(pl + 0xFC) != 0 || m.read8(pl + 0xFD) != 6 || m.read8(pl + 0xFE) == 3)
        return;
    cpu.fpr[1] = cpu.ps1[1] = g_stick_pitch;
}

// cObjWep::drawLaserSight (0x80043E5C) : en mode pointeur, ni la ligne ni le point du laser (UHD
// 0x73EA60 : 0x73EC6C et 0x73EB3E sautes), seul le curseur montre la visee, comme sur la Wii. Le
// calcul (cible, marqueur, drapeau de tir) est garde : draw = 0.
void nj_wii_drawLaserSight(GuestCpu& cpu, GuestFn orig) {
    if (pointer_active() && now_ms() - g_pose_ms <= 150)
        cpu.gpr[4] = 0;
    orig(cpu);
}

// Chaque image, sur le fil du jeu (PADRead) : camera d'avant la mise en joue, passage a l'etat
// vise, et les touches pour le marqueur (un ennemi perd des points de vie pendant la visee ou
// 0,8 s apres ; EmMgr 0x802DE700 : tableau +0, nombre +4, taille +8 ; cEm be_flag +0 bit 0,
// hp +0x320).
void nj_wii_frame(GuestCpu& cpu) {
    auto& m = cpu.mmu;
    const uint32_t g = m.read32(kPG), pl = m.read32(kPPL);
    if (!heap_ptr(g) || !heap_ptr(pl))
        return;
    static uint8_t fd_before = 0;
    const uint8_t fd = m.read8(pl + 0xFD);
    if (fd == 6 && fd_before != 6)
        g_new_aim = true;
    fd_before = fd;
    const uint32_t now = now_ms();
    static uint32_t aim_ms = 0;
    if (fd == 6 && m.read8(pl + 0xFC) == 0) {
        aim_ms = now;
    } else {
        for (int i = 0; i < 12; ++i)
            g_cam_before.mat[i] = m.read_f32(g + 0x74 + i * 4);
        for (int i = 0; i < 3; ++i)
            g_cam_before.pos[i] = m.read_f32(g + 0x118 + i * 4);
        g_cam_before.fovy = m.read_f32(g + 0x134);
        cursor(g_cam_before.cx, g_cam_before.cy);
        g_cam_before.ok = true;
    }
    static uint32_t who[256];
    static int16_t hp[256];
    const uint32_t arr = m.read32(nj::runtime::ga(0x802DE700)), size = m.read32(nj::runtime::ga(0x802DE708));
    int n = (int)m.read32(nj::runtime::ga(0x802DE704));
    if (!heap_ptr(arr) || size == 0 || size > 0x4000 || n <= 0)
        return;
    n = n > 256 ? 256 : n;
    const bool recent = aim_ms && now - aim_ms < 800;
    for (int i = 0; i < n; ++i) {
        const uint32_t e = arr + size * (uint32_t)i;
        if (!(m.read32(e) & 1) || e == pl) {
            who[i] = 0;
            continue;
        }
        const int16_t v = (int16_t)m.read16(e + 0x320);
        if (who[i] == e && v < hp[i] && recent)
            nj::runtime::input::wii_pointer_set_hit(now);
        who[i] = e;
        hp[i] = v;
    }
}

// PlWepHitCheck2 (0x80179024 : pl r3, p0 r4, p1 r5, type, flag, len f1) : la balle. Sur GameCube
// elle part du canon dans l'axe de l'arme (wep02_r3_fire00 : p1 = canon + 50 m selon l'os 10) et
// touche la ou l'arme pointe, pas sous le curseur. La Wii recale sa direction vers le point que
// touche la ligne camera -> curseur (UHD 0x687300 puis 0x683520) : on fait de meme, la balle part
// toujours du canon.
void nj_wii_PlWepHitCheck2(GuestCpu& cpu, GuestFn orig) {
    auto& m = cpu.mmu;
    const uint32_t pl = m.read32(kPPL), g = m.read32(kPG);
    const uint32_t p0a = cpu.gpr[4], p1a = cpu.gpr[5];
    if (pointer_active() && now_ms() - g_pose_ms <= 300 && cpu.gpr[3] == pl && heap_ptr(pl) &&
        heap_ptr(g) && heap_ptr(p0a) && heap_ptr(p1a)) {
        const auto gsave = cpu.gpr;
        const auto fsave = cpu.fpr;
        const auto psave = cpu.ps1;
        const auto crsave = cpu.cr;
        const uint32_t lr = cpu.lr;
        float mat[12], pos[3], cx, cy, P[3], far[3];
        for (int i = 0; i < 12; ++i)
            mat[i] = m.read_f32(g + 0x74 + i * 4);
        for (int i = 0; i < 3; ++i)
            pos[i] = m.read_f32(g + 0x118 + i * 4);
        cursor(cx, cy);
        uint32_t em = 0;
        const int res = ray(cpu, mat, pos, m.read_f32(g + 0x134), cx, cy, m.read8(g + nj::runtime::pg(0x4FB0)), P, far, &em);
        cpu.gpr = gsave;
        cpu.fpr = fsave;
        cpu.ps1 = psave;
        cpu.cr = crsave;
        cpu.lr = lr;
        if (res >= 0) {
            float p0[3], d[3], len = 0.0f, n = 0.0f;
            for (int i = 0; i < 3; ++i) {
                p0[i] = m.read_f32(p0a + i * 4);
                const float e = m.read_f32(p1a + i * 4) - p0[i];
                len += e * e;
                d[i] = P[i] - p0[i];
                n += d[i] * d[i];
            }
            len = std::sqrt(len);
            n = std::sqrt(n);
            if (n > 1.0f && len > 1.0f)
                for (int i = 0; i < 3; ++i)
                    m.write_f32(p1a + i * 4, p0[i] + d[i] / n * len);
        }
    }
    cpu.pc = nj::runtime::ga(0x80179024);
    orig(cpu);
}
