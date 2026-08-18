#include "runtime/cpu_context.h"
#include "runtime/log.h"
#include "runtime/game_version.h"
#include "input/input_manager.h"
#include "input/mouse_events.h"
#include "runtime/env.h"
#include "runtime/gfx_settings.h"
#include "runtime/dev_tour.h"
#include "runtime/game_call.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>

using namespace nj::runtime;
using namespace nj::runtime::input;

void nj_wii_frame(nj::runtime::GuestCpu& ctx);  // hle/wii_aim.cpp

namespace {

// Dispositions de manette de RE4 UHD (F10 « Controller layout »).
//
// Le jeu GameCube passe la manette (bits PAD) aux touches logiques (Key, 64 bits, noms KEY_* de
// l'UHD : re4_tweaks SDK/pad.h) par Key_type_tbl[0] (game/pad.cpp, 0x80266AC0 dans la DOL) :
// entree i = bits PAD qui allument la touche logique i. L'UHD, lui, lie chaque touche logique a
// des boutons XInput par disposition : BIO4/input.ini, sections ###XINPUT_CONTROLLER_A, _B, _C
// (Type I, II, III ; B et C identiques). Ses numeros QKEY_GC_Bn : B1-B4 croix G D B H, B5 RT,
// B6 RB, B7 LB, B8 LT, B9 X, B10 B, B11 A, B12 Y, B13 Start, B14 Back, B17-B20 stick gauche
// G D B H, B21-B24 stick droit (deduits de ses invites « LB RB Zoom », « Y Move selection »,
// « A Detail view » et des roles). La manette envoie un bit par bouton (gamepad_source.cpp :
// LB 0x0080, RB 0x2000, Back 0x4000 en plus des bits GameCube ; RT et LT par les gachettes, bits
// R 0x20 et L 0x40) et on ecrit ici la table de la disposition tant que la manette est le dernier
// peripherique utilise ; le clavier retrouve la table d'origine des qu'il sert (LastUsedDevice).
constexpr nj::runtime::GameAddr kKeyTypeTbl {0x80266AC0};
constexpr int kKeys = 38;
constexpr uint32_t DL = 0x1, DR = 0x2, DD = 0x4, DU = 0x8, RT = 0x20, LT = 0x40, LB = 0x80, A = 0x100,
                   B = 0x200, X = 0x400, Y = 0x800, ST = 0x1000, RB = 0x2000, BK = 0x4000;
constexpr uint32_t SL = 0x10000, SR = 0x20000, SD = 0x40000, SU = 0x80000;
constexpr uint32_t CL = 0x100000, CR = 0x200000, CD = 0x400000, CU = 0x800000;

// Table d'origine (game/pad.cpp, Key_type_tbl[0]).
constexpr uint32_t kGameCube[kKeys] = {
    0x00080008, 0x00040004, 0x00020002, 0x00010001, 0x00000020, 0x00000040, 0x00000200, 0x00000100,
    0x00000000, 0x00000400, 0x00000100, 0x00000040, 0x00001000, 0x00001000, 0x00800000, 0x00400000,
    0x00000800, 0x00000400, 0x00000200, 0x00000100, 0x00000800, 0x00000010, 0x00000040, 0x00000020,
    0x00080008, 0x00040004, 0x00020002, 0x00010001, 0x00000010, 0x00001000, 0x00000200, 0x00000100,
    0x00000001, 0x00000002, 0x00000004, 0x00000008, 0x00200000, 0x00100000,
};

// Type I (###XINPUT_CONTROLLER_A) : viser RT, tirer A, couteau LT.
constexpr uint32_t kTypeI[kKeys] = {
    DU | SU, DD | SD, DR | SR, DL | SL,   // FORWARD BACK RIGHT LEFT
    RT,        // KAMAE
    LB,        // RELOCKON
    X,         // RUN
    A,         // FIRE
    0,         // 180TURN
    RB,        // ASHLEY
    A,         // CK
    LT,        // KAMAE_KNIFE
    ST,        // ST
    BK,        // OPTION
    CU, CD,    // C_U C_D
    A,         // KEY_Y
    Y,         // KEY_X
    X,         // KEY_B
    A,         // KEY_A
    ST,        // SSCRN
    Y,         // MAP
    LB, RB,    // LB RB
    DU | SU, DD | SD, DR | SR, DL | SL,   // U D R L
    RT | LT,   // Z
    BK,        // EV_CANCEL
    B,         // CANCEL
    X | A,     // OK
    DL, DR, DD, DU,   // LL LR LD LU
    CR, CL,    // C_R C_L
};

// Types II et III (_B, _C) : viser LT, tirer RT, couteau LB, demi-tour X.
constexpr uint32_t kTypeII[kKeys] = {
    DU | SU, DD | SD, DR | SR, DL | SL,
    LT,        // KAMAE
    LB,        // RELOCKON
    X,         // RUN
    RT,        // FIRE
    X,         // 180TURN
    RB,        // ASHLEY
    A,         // CK
    LB,        // KAMAE_KNIFE
    ST, BK, CU, CD,
    A, Y, X, A,   // KEY_Y KEY_X KEY_B KEY_A
    ST, Y, LB, RB,
    DU | SU, DD | SD, DR | SR, DL | SL,
    RT | LT, BK, B, X | A,
    DL, DR, DD, DU,
    CR, CL,
};

bool heap_ptr(uint32_t p) { return p >= 0x80000000u && p < 0x81800000u; }

// PadRead est appele par la boucle d'image du jeu (main) : c'est le point de passage sur d'un appel direct a une fonction du jeu
// (meme verification que hle/quick_equip.cpp).
bool in_main_loop(GuestCpu& ctx) {
    const uint32_t up = ctx.mmu.read32(ctx.gpr[1]);
    const uint32_t ret = heap_ptr(up) ? ctx.mmu.read32(up + 4) : 0;
    const uint32_t m0 = static_cast<uint32_t>(nj::runtime::GameAddr{0x80152FC8}), m1 = static_cast<uint32_t>(nj::runtime::GameAddr{0x8015333C});
    return m0 && m1 && ret >= m0 && ret < m1;
}

// « Retry from a continue point » et « Load game » : on ouvre le vrai menu OPTIONS du jeu et on y navigue en appuyant sur ses touches,
// guides par son etat (pG->Rno0 == 6 et Rno1 == 1 : menu ouvert ; OptScrn._rno0 menu / sous-menu, _rno1 entree du haut, _rno2 curseur,
// _rno3 etat). Le jeu fait ensuite tout lui-meme (boite Yes / No du retry, ecran de la carte memoire du chargement) : memes chemins, memes protections.
//
// Regles (retours du 26/10 : le retry se validait tout seul et « Load game » faisait un retry) :
//  - UNE seule image d'appui par touche, puis 8 images sans rien injecter : jamais de touche maintenue quand la boite Yes / No apparait ;
//  - on ne decide que sur l'etat lu apres ce temps d'attente (le jeu a traite l'appui) ;
//  - la boite Yes / No (_rno3 == 1) ou le chargement (_rno3 == 2) termine le script : la suite est au joueur ;
//  - une seule demande a la fois, et 2 s de repos ensuite (une pression longue du menu ne rejoue pas le script).
struct OptScript { int what = 0, wait = 0, frames = 0, open_tries = 0, rest = 0; } g_os;

void drive_option_menu(GuestCpu& ctx, PadState& pad) {
    namespace g = nj::runtime::gfx;
    if (g_os.rest > 0) --g_os.rest;
    if (g_os.what == 0) {
        const int req = g::g_game_set_menu;
        g::g_game_set_menu = 0;
        if (req <= 0 || g_os.rest > 0) return;
        g_os = OptScript{};
        g_os.what = req;
    } else {
        g::g_game_set_menu = 0;   // demande pendant un script : ignoree
    }
    auto stop = [&] { g_os.what = 0; g_os.rest = 120; };
    if (++g_os.frames > 700) { stop(); return; }
    if (g_os.wait > 0) { --g_os.wait; return; }
    const uint32_t pgp = ctx.mmu.read32(static_cast<uint32_t>(nj::runtime::GameAddr{0x80314BC8}));
    if (!heap_ptr(pgp)) { stop(); return; }
    const uint32_t opt = static_cast<uint32_t>(nj::runtime::GameAddr{0x802767C0});
    const uint8_t rno0 = ctx.mmu.read8(pgp + 0x20), rno1 = ctx.mmu.read8(pgp + 0x21);
    const uint8_t o0 = ctx.mmu.read8(opt), o1 = ctx.mmu.read8(opt + 1), o2 = ctx.mmu.read8(opt + 2), o3 = ctx.mmu.read8(opt + 3);
    auto press = [&](uint32_t bit) { pad.buttons |= (uint16_t)bit; g_os.wait = 8; };   // une image d'appui, puis le jeu a 8 images pour reagir
    if (rno0 != 6) {   // menu pas ouvert : on appuie sur sa touche (Back dans les dispositions Type I a III, Start sinon)
        if (g_os.open_tries >= 6) { stop(); return; }   // le jeu ne l'ouvre pas (cinematique, titre...) : on abandonne
        ++g_os.open_tries;
        press((g::g_controller_layout >= 1 && g::g_controller_layout <= 3 && g::g_last_device == 1) ? BK : ST);
        g_os.wait = 24;   // le menu met un moment a s'ouvrir : on ne retape pas Start pendant ce temps
        return;
    }
    if (rno1 != 1) return;   // ouverture en cours
    if (o0 == 0) {           // menu du haut : « Retry or load » est la premiere entree ; si le jeu l'a grisee (curseur ailleurs), on s'arrete menu ouvert
        if (o1 == 0) press(A); else stop();
        return;
    }
    if (o3 != 0) { stop(); return; }   // boite Yes / No (retry) ou chargement (load) : la suite est au joueur / au jeu
    if (g_os.what == 1) {              // Retry : le curseur du sous-menu est sur « RETRY from a CONTINUE POINT »
        if (o2 == 0) press(A); else stop();
    } else {                           // Load : une fois vers le bas, puis A
        if (o2 == 0) press(DD);
        else if (o2 == 1) press(A);
        else stop();
    }
}

// Options du jeu (menu F10) : le menu depose des demandes, on les ecrit ici dans SystemSave (0x802E8AF8 du decompile, 0x38 octets) et on publie
// les valeurs courantes. Config_flg : bits de poids fort en premier (CFG_AIM_REVERSE 0 = 0x80000000, CFG_VIBRATION 4 = 0x08000000, CFG_KNIFE_MODE 5 =
// 0x04000000). Luminosite : octet 0x0A, copie dans pRK->base_brightness (0x13). Retour au titre : pG->System_flg (0x54) |= SYS_SOFT_RESET (0x04000000).
void sync_game_options(GuestCpu& ctx, PadState& pad) {
    namespace g = nj::runtime::gfx;
    const uint32_t ss = static_cast<uint32_t>(nj::runtime::GameAddr{0x802E8AF8});
    if (ss < 0x80000000u || ss >= 0x81800000u) return;
    uint32_t cfg = ctx.mmu.read32(ss), w08 = ctx.mmu.read32(ss + 8);
    bool dirty = false;
    auto flag = [&](int& req, uint32_t bit) {
        if (req < 0) return;
        const uint32_t n = req ? (cfg | bit) : (cfg & ~bit);
        if (n != cfg) { cfg = n; dirty = true; }
        req = -1;
    };
    flag(g::g_game_set_reverse, 0x80000000u);
    flag(g::g_game_set_vibration, 0x08000000u);
    flag(g::g_game_set_knife, 0x04000000u);
    if (g::g_game_set_brightness >= 0) {
        const uint32_t v = (uint32_t)std::clamp(g::g_game_set_brightness, 64 - 30, 64 + 50);
        w08 = (w08 & ~0xFF00u) | (v << 8);
        dirty = true;
        const uint32_t prk = ctx.mmu.read32(static_cast<uint32_t>(nj::runtime::GameAddr{0x80315AD4}));
        if (prk >= 0x80000000u && prk < 0x81800000u)
            ctx.mmu.write32(prk + 0x10, (ctx.mmu.read32(prk + 0x10) & ~0xFFu) | v);
        g::g_game_set_brightness = -1;
    }
    if (dirty) {
        ctx.mmu.write32(ss, cfg);
        ctx.mmu.write32(ss + 8, w08);
    }
    if (g::g_game_set_snd >= 0 && in_main_loop(ctx)) {   // le jeu change lui-meme de mode (SndSetOutputMode charge les donnees du mode)
        call_game(ctx, static_cast<uint32_t>(nj::runtime::GameAddr{0x8005EF4C}), {(uint32_t)g::g_game_set_snd, 0u});
        g::g_game_set_snd = -1;
    }
    drive_option_menu(ctx, pad);
    if (g::g_game_set_reset > 0) {
        const uint32_t pg = ctx.mmu.read32(static_cast<uint32_t>(nj::runtime::GameAddr{0x80314BC8}));
        if (pg >= 0x80000000u && pg < 0x81800000u) ctx.mmu.write32(pg + 0x54, ctx.mmu.read32(pg + 0x54) | 0x04000000u);
        g::g_game_set_reset = 0;
    }
    g::g_game_cur_reverse = (cfg & 0x80000000u) != 0;
    g::g_game_cur_vibration = (cfg & 0x08000000u) != 0;
    g::g_game_cur_knife = (cfg & 0x04000000u) != 0;
    g::g_game_cur_brightness = (int)((w08 >> 8) & 0xFFu);
    g::g_game_cur_snd = (int)(ctx.mmu.read32(ss + 0xC) >> 24);
    {   // Retry possible : Status_flg[2] 0x8000 grise l'entree dans le menu du jeu (pG + 0x500C, +0xC sur les versions commerciales)
        const uint32_t pgp2 = ctx.mmu.read32(static_cast<uint32_t>(nj::runtime::GameAddr{0x80314BC8}));
        g::g_game_cur_retry_ok = heap_ptr(pgp2) ? ((ctx.mmu.read32(pgp2 + nj::runtime::pg(0x5014)) & 0x8000u) == 0 ? 1 : 0) : -1;
    }
}

void apply_layout(GuestCpu& ctx) {
    static int applied = -1;
    const int layout = gfx::g_controller_layout;
    const int want = (layout >= 1 && layout <= 3 && gfx::g_last_device == 1) ? layout : 0;
    if (want == applied)
        return;
    // la table est a sa place seulement si elle contient ce qu'on y attend (autre DOL : rien)
    static const bool ours = [&] {
        for (int i = 0; i < 8; ++i)
            if (ctx.mmu.read32(kKeyTypeTbl + 4 * i) != kGameCube[i])
                return false;
        return true;
    }();
    if (!ours)
        return;
    const uint32_t* t = want == 0 ? kGameCube : want == 1 ? kTypeI : kTypeII;
    for (int i = 0; i < kKeys; ++i)
        ctx.mmu.write32(kKeyTypeTbl + 4 * i, t[i]);
    applied = want;
    NJ_PRINTF("[Manette] disposition %s\n", want == 0 ? "GameCube (clavier)" : want == 1 ? "Type I (UHD)"
                                                  : want == 2 ? "Type II (UHD)" : "Type III (UHD)");
    std::fflush(stdout);
}

}  // namespace

// PADStatus du SDK (12 octets) : boutons (16 bits), stick X/Y, C-stick X/Y, gachettes L/R,
// boutons analogiques A/B, erreur (0 branchee, -1 absente), un octet libre.
static void put_pad_status(GuestCpu& ctx, uint32_t at, const PadState& p) {
    const uint8_t bytes[12] = {
        (uint8_t)(p.buttons >> 8), (uint8_t)p.buttons,
        (uint8_t)p.stick_x, (uint8_t)p.stick_y, (uint8_t)p.substick_x, (uint8_t)p.substick_y,
        p.trigger_l, p.trigger_r,
        (uint8_t)((p.buttons & 0x0100) ? 0xFF : 0), (uint8_t)((p.buttons & 0x0200) ? 0xFF : 0),
        (uint8_t)p.err, 0};
    for (uint32_t i = 0; i < 12; ++i) ctx.mmu.write8(at + i, bytes[i]);
}

extern "C" {

void PADInit(GuestCpu&) {}

void PADRead(GuestCpu& ctx) {
    const uint32_t pad_status_array_addr = ctx.gpr[3];
    nj_wii_frame(ctx);
    // NJ_PLACE="x,y,z,ry" : place Leon (position et cap) comme dol_place.py le fait dans Dolphin,
    // pour comparer une meme vue (lens flare de la salle 1-1b, 01/10). Apres 120 lectures de manette
    // avec un pPL valide, ecrit pendant 60 lectures puis laisse le jeu.
    if (const char* place = NJ_ENV("NJ_PLACE")) {
        static int seen = 0, written = 0;
        static float v[4];
        static const bool ok = std::sscanf(place, "%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3]) == 4;
        const uint32_t pl = ctx.mmu.read32(nj::runtime::ga(0x803159F4));
        if (ok && pl >= 0x80000000u && pl < 0x81800000u && written < 60 && ++seen > 120) {
            ctx.mmu.write_f32(pl + 0x94, v[0]);
            ctx.mmu.write_f32(pl + 0x98, v[1]);
            ctx.mmu.write_f32(pl + 0x9C, v[2]);
            ctx.mmu.write_f32(pl + 0xA4, v[3]);
            if (++written == 60)
                NJ_PRINTF("[PLACE] Leon place a %.1f,%.1f,%.1f cap %.4f\n", v[0], v[1], v[2], v[3]);
        }
    }
    if (NJ_ENV("NJ_DTDBG")) {
        static int n = 0;
        const uint32_t g = ctx.mmu.read32(nj::runtime::ga(0x80314BC8));
        if (const char* dt = NJ_ENV("NJ_DT"); dt && g >= 0x80000000u && g < 0x81800000u)
            ctx.mmu.write_f32(g + 0x70, (float) std::atof(dt));
        if ((n++ % 10) == 0) {
            const uint32_t pl = ctx.mmu.read32(nj::runtime::ga(0x803159F4));
            if (pl >= 0x80000000u && pl < 0x81800000u)
                NJ_PRINTF("[DTPL] frame=%.3f pos=%.1f,%.1f\n", ctx.mmu.read_f32(pl + 0x1D8 + 0x24),
                            ctx.mmu.read_f32(pl + 0x94), ctx.mmu.read_f32(pl + 0x9C));
            if (g >= 0x80000000u && g < 0x81800000u)
                NJ_PRINTF("[DT] g=%08X +70=%08X (%f) +5C=%u\n", g, ctx.mmu.read32(g + 0x70),
                            ctx.mmu.read_f32(g + 0x70), ctx.mmu.read32(g + 0x5C));
        }
    }
    PadState state = InputHub::get().pad(0);
    nj::dev::pad_hook(ctx, state);   // enregistreur / rejoueur d'entrees (NJ_DEVTOOLS, sinon vide)
    apply_layout(ctx);
    sync_game_options(ctx, state);

    put_pad_status(ctx, pad_status_array_addr, state);

#if defined(__APPLE__)
    // iPhone : mesure du stick, du doigt jusqu'a Leon. Toutes les 6 lectures tant que le stick est pousse :
    // valeur envoyee au jeu, deplacement reel de Leon (distance et cap) depuis la ligne precedente.
    {
        static int lines = 0, reads = 0, idle = 0;
        static float px = 0, pz = 0;
        static bool had = false;
        const bool pushed = state.stick_x != 0 || state.stick_y != 0;
        ++reads;
        if (lines < 500 && ((pushed && reads % 6 == 0) || (pushed != had))) {
            const uint32_t pl = ctx.mmu.read32(nj::runtime::ga(0x803159F4));
            if (pl >= 0x80000000u && pl < 0x81800000u) {
                const float x = ctx.mmu.read_f32(pl + 0x94), z = ctx.mmu.read_f32(pl + 0x9C);
                NJ_PRINTF("[Mesure] envoi=(%d,%d) boutons=0x%x leon dx=%.1f dz=%.1f cap=%.2f lectures=%d\n",
                          (int)state.stick_x, (int)state.stick_y, (unsigned)state.buttons, x - px, z - pz,
                          ctx.mmu.read_f32(pl + 0xA4), reads);
                px = x; pz = z;
            }
            ++lines;
            reads = 0;
        }
        (void)idle;
        had = pushed;
    }
#endif

    // NJ_PADDBG=1 : combien de fois le jeu lit la manette, et ce qu'il y
    // trouve. On a passe une soiree a chercher pourquoi la visee ne tenait
    // pas en instrumentant tout **sauf** ce point de passage.
    {
        static const bool dbg = NJ_ENV("NJ_PADDBG") != nullptr;
        if (dbg) {
            static uint64_t calls = 0, with_aim = 0;
            static std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
            ++calls;
            if (state.buttons & 0x20)
                ++with_aim;
            const auto now = std::chrono::steady_clock::now();
            if (now - t0 >= std::chrono::seconds(2)) {
                // Key.on lu **sur le fil du jeu**, ici, au moment ou il vient
                // de reconstruire son etat. Lu depuis le fil hote il tombait
                // regulierement a zero, et j'ai cru a une perte d'entree :
                // c'etait seulement la reconstruction surprise en cours
                // (pad.cpp fait U64Set(Key.on, 0) avant de la remplir).
                const uint32_t key_lo = ctx.mmu.read32(nj::runtime::ga(0x802E94D0) + 0x14);
                // La routine du joueur. pl_class.cpp:590 refuse la mise en
                // joue des l'entree si xFC != 0, quoi que dise joyKamae() -
                // et joyKamae() dit oui en permanence. C'est donc ici qu'il
                // faut regarder, pas dans l'entree.
                //   cEm::xFC 0xFC, xFD 0xFD, xFE 0xFE  (include/em.h)
                unsigned ev_seen = 0, ev_btn = 0;
                nj::runtime::input::mouse_event_stats(&ev_seen, &ev_btn);
                uint32_t f420 = 0xFFFFFFFFu;
                uint32_t rout = 0xFFFFFFFFu;
                {
                    const uint32_t pl = ctx.mmu.read32(nj::runtime::ga(0x803159F4));
                    if (pl >= 0x80000000u && pl < 0x81800000u) {
                        f420 = ctx.mmu.read32(pl + 0x420);
                        rout = ctx.mmu.read32(pl + 0xFC);   // xFC..xFF d'un bloc
                    }
                }
                NJ_PRINTF("[PadRead] %llu lectures, dont %llu avec la visee "
                            "(boutons=%04X) viser=%d  ev_souris=%u clics=%u  xFC=%u xFD=%u xFE=%u\n",
                            (unsigned long long) calls, (unsigned long long) with_aim,
                            state.buttons, (int) ((key_lo >> 4) & 1), ev_seen, ev_btn,
                            (rout >> 24) & 0xFF, (rout >> 16) & 0xFF, (rout >> 8) & 0xFF);
                std::fflush(stdout);
                calls = with_aim = 0;
                t0 = now;
            }
        }
    }

    // NJ_RNODBG=1 : chaque changement de la routine du joueur (xFC..xFF),
    // lu sur le fil du jeu. « La visee saute » montre l'arme repartir de la
    // pose levee : si c'est une sortie de visee, on la voit ici, avec l'image
    // et l'etat de la touche de visee au meme instant.
    {
        static const bool dbg = NJ_ENV("NJ_RNODBG") != nullptr;
        if (dbg) {
            static uint64_t n = 0;
            static uint32_t last = 0xFFFFFFFFu, last_k = 0xFFFFFFFFu;
            ++n;
            const uint32_t pl = ctx.mmu.read32(nj::runtime::ga(0x803159F4));
            if (pl >= 0x80000000u && pl < 0x81800000u) {
                const uint32_t rno = ctx.mmu.read32(pl + 0xFC);
                const uint32_t k = (ctx.mmu.read32(nj::runtime::ga(0x802E94D0) + 0x14) >> 4) & 1;
                if (rno != last || k != last_k) {
                    NJ_PRINTF("[RNO] lecture=%llu RNO=%02X %02X %02X %02X kamae=%u fr=%.2f f420=%08X\n",
                                (unsigned long long) n, rno >> 24, (rno >> 16) & 0xFF,
                                (rno >> 8) & 0xFF, rno & 0xFF, k, ctx.mmu.read_f32(pl + 0x290),
                                ctx.mmu.read32(pl + 0x420));
                    std::fflush(stdout);
                    last = rno;
                    last_k = k;
                }
                // Dans la mise en joue (xFD=6), le detail de l'animation a
                // chaque image : MotionWork a +0x1D8 (include/model.h).
                static int detail = 0;
                if (((rno >> 16) & 0xFF) == 6 && detail < 150) {
                    ++detail;
                    const uint32_t m = pl + 0x1D8;
                    NJ_PRINTF("[MOT] lecture=%llu RNO=%08X seqFrame=%.3f frame=%.3f max=%.1f "
                                "speed=%.3f seqMax=%u flags=%04X state=%04X data=%08X seq=%08X\n",
                                (unsigned long long) n, rno, ctx.mmu.read_f32(m + 0xB8),
                                ctx.mmu.read_f32(m + 0x24), ctx.mmu.read_f32(m + 0x20),
                                ctx.mmu.read_f32(m + 0xC0), ctx.mmu.read16(m + 0xBC),
                                ctx.mmu.read16(m + 0x40), ctx.mmu.read16(m + 0x42),
                                ctx.mmu.read32(m + 0x00), ctx.mmu.read32(m + 0xA8));
                    std::fflush(stdout);
                }
            }
        }
    }

    for (uint32_t port = 1; port < 4; ++port)   // une seule manette : les autres absentes
        put_pad_status(ctx, pad_status_array_addr + 12 * port, PadState{});
}

} 
