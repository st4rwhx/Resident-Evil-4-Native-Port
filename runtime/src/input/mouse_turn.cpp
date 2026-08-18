#include "input/mouse_turn.h"
#include "runtime/log.h"
#include "runtime/game_version.h"

#include "runtime/cpu_context.h"
#include "runtime/config.h"
#include "runtime/env.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace nj::runtime {
extern MMU* g_guest_mem;
}

// Compteurs du runtime, hors namespace (src/hle/gx.cpp).
extern uint64_t g_st_frames;
extern uint64_t g_stat_draws;

namespace nj::runtime::input {

namespace {

// Releves dans la decompilation, pas devines. Voir l'en-tete.
constexpr nj::runtime::GameAddr kPlayerPtr {0x803159F4};   // pPL, .sbss
// L'etat d'entree **du jeu**, pas le notre. Key = .bss:0x802E94D0
// (config/G4BE08/symbols.txt) ; KeyWork::on a +0x10, trg a +0x18
// (include/main.h). Lire ce que le jeu croit recevoir est le seul moyen de
// savoir pourquoi il annule une mise en joue.
constexpr nj::runtime::GameAddr kKey       {0x802E94D0};
constexpr uint32_t kKeyOn     = 0x10;
// cEm::flags_420 (include/em.h:201). Le bit 0x1000 **interdit de viser** :
// joyKamae() rend 0 tant qu'il est mis (src/game/pl_sub.cpp:796). Il bascule
// sur la touche logique 11, qui est la gachette L dans Key_type_tbl[0]
// (src/game/player.cpp:374).
constexpr uint32_t kFlags420  = 0x420;
constexpr uint32_t kRotOffset = 0xA0;         // cModel::rot
constexpr uint32_t kRotY      = kRotOffset + 4;
// Visee verticale : cEm::pWep (0x788) -> cPlWep::pitch (0x28).
constexpr uint32_t kWepPtr    = 0x788;
constexpr uint32_t kPitch     = 0x28;
// Butee prudente. Le jeu borne lui-meme le tangage ; on evite seulement de
// lui donner une valeur absurde entre deux de ses corrections.
constexpr float kPitchLimit   = 1.0f;

// La MEM1 du GameCube fait 24 Mo : 0x80000000 a 0x81800000. Un test du seul
// octet de poids fort contre 0x80 rejette donc tout le dernier tiers. C'est
// ce qui faisait echouer la visee verticale en silence : pWep valait
// 0x8118FC80, une adresse parfaitement valide, et le pointeur joueur ne
// passait que par chance.
inline bool in_mem1(uint32_t a) {
    return a >= 0x80000000u && a < 0x81800000u;
}

// Constantes de re4_tweaks (MouseTurning.cpp, type B) :
//   rotation -= (dx / 450.0) * sensibilite,   deplacement de moins de 3 pixels ignore
// 450 y est une base normalisee sur la cadence. On garde le chiffre et on
// normalise nous aussi, sinon la visee serait deux fois plus rapide a 40
// images par seconde qu'a 20 - et notre cadence varie beaucoup.
constexpr float kSpeedBase = 450.0f;
constexpr int   kDeadzone  = 3;
constexpr float kRefDt     = 1.0f / 60.0f;

float sensitivity() {
    static const float s = [] {
        const char* e = NJ_ENV("NJ_MOUSE_SENS");
        const float v = e ? (float) std::atof(e) : 0.0f;
        return v > 0.0f ? v : nj::runtime::Config::get().mouse_sensitivity;
    }();
    return s;
}

bool in_scene() {
    // Meme critere que l'autostart : la facade dessine quelques centaines de
    // primitives par image, le jeu plusieurs milliers. Ecrire dans la
    // structure du joueur pendant un menu n'aurait pas de sens.
    static uint64_t lf = 0, ld = 0;
    static bool yes = false;
    const uint64_t f = ::g_st_frames, d = ::g_stat_draws;
    if (f > lf + 15) {
        yes = ((d - ld) / (f - lf)) > 3000;
        lf = f;
        ld = d;
    }
    return yes;
}

} // namespace

// Le jeu dessine-t-il une vraie scene ? Expose pour que les sources
// d entree partagent cette detection au lieu d en recopier une chacune -
// la copie locale de NJ_FAKEAIM n a jamais arme, et son silence
// ressemblait a un resultat.
bool game_in_scene() { return in_scene(); }

// Signale tout changement de l'etat d'entree vu par le jeu, et surtout du
// drapeau qui interdit la visee. Sans ca, "Leon annule son tir" reste une
// impression ; avec, c'est un bit nomme a un instant precis.
// Tant que la visee est demandee, on efface le drapeau qui l'interdit.
//
// Le jeu ne l'efface que sur un **nouvel appui** (Key.trg bit 4,
// src/game/player.cpp:371). Quelqu'un qui maintient le clic droit n'en
// produit jamais : une fois le drapeau mis, la visee reste morte jusqu'au
// relachement. Sur manette le bouton L est deliberement une bascule ; au
// clavier c'est un piege invisible, et rien ne l'expose. On le neutralise
// donc pendant la visee plutot que d'esperer que personne n'y touche.
bool mouse_turn_enabled();   // definie plus bas

void unblock_aim(bool aiming) {
    if (!aiming || !mouse_turn_enabled() || !nj::runtime::g_guest_mem)
        return;
    MMU& mmu = *nj::runtime::g_guest_mem;
    const uint32_t pl = mmu.read32(kPlayerPtr);
    if (!in_mem1(pl))
        return;
    const uint32_t fl = mmu.read32(pl + kFlags420);
    if (fl & 0x1000)
        mmu.write32(pl + kFlags420, fl & ~0x1000u);
}

void game_state_watch() {
    static const bool dbg = NJ_ENV("NJ_INPUTDBG") != nullptr;
    if (!dbg || !nj::runtime::g_guest_mem)
        return;
    MMU& mmu = *nj::runtime::g_guest_mem;
    const uint32_t on_hi = mmu.read32(kKey + kKeyOn);
    const uint32_t on_lo = mmu.read32(kKey + kKeyOn + 4);
    uint32_t fl = 0;
    const uint32_t pl = mmu.read32(kPlayerPtr);
    if (in_mem1(pl))
        fl = mmu.read32(pl + kFlags420);

    static uint32_t l_hi = 0xFFFFFFFF, l_lo = 0xFFFFFFFF, l_fl = 0xFFFFFFFF;
    const bool blocked = (fl & 0x1000) != 0;
    if (on_hi != l_hi || on_lo != l_lo || (fl & 0x1000) != (l_fl & 0x1000)) {
        l_hi = on_hi; l_lo = on_lo; l_fl = fl;
        static int n = 0;
        if (n++ < 120)
            NJ_PRINTF("[Jeu] Key.on=%08X%08X  viser=%d  BLOQUE=%d (flags_420=%08X)\n",
                        on_hi, on_lo, (int) ((on_lo >> 4) & 1), (int) blocked, fl);
        std::fflush(stdout);
    }
}

bool mouse_turn_enabled() {
    // Actif par defaut (config.toml [input] mouse_turn) ; NJ_MOUSETURN le force pour les essais.
    static const bool on = NJ_ENV("NJ_MOUSETURN") != nullptr ||
                           (nj::runtime::Config::get().mouse_turn &&
                            NJ_ENV("NJ_NOMOUSETURN") == nullptr);
    return on;
}

void mouse_turn_apply(int dx, int dy, float dt, bool aiming) {
    // **Avant tout retour anticipe.** Places plus bas, ces deux appels ne
    // s'executaient que si la souris bougeait et que la rotation directe
    // etait active : les deux mouchards sont restes muets tout un run, et
    // j'ai conclu a tort que rien ne se passait. Un instrument derriere une
    // condition n'est pas un instrument.
    game_state_watch();
    unblock_aim(aiming);

    // NJ_FAKETURN=<dx> : injecte un deplacement constant, pour verifier
    // toute la chaine sans dependre du focus de la fenetre. Le clic et le
    // mouvement synthetises depuis PowerShell n'arrivaient qu'une fois sur
    // trois, et un test qui echoue en silence est pire que pas de test.
    {
        static const int fake = [] {
            const char* e = NJ_ENV("NJ_FAKETURN");
            return e ? std::atoi(e) : 0;
        }();
        if (fake) {
            dx = fake;
            dy = fake;
            // On force aussi la mise en joue : sans elle le tangage n'est
            // jamais exerce et le test ne couvrirait que l'axe horizontal.
            aiming = true;
        }
    }
    if (!mouse_turn_enabled() || !nj::runtime::g_guest_mem)
        return;
    const bool moveX = dx <= -kDeadzone || dx >= kDeadzone;
    const bool moveY = dy <= -kDeadzone || dy >= kDeadzone;
    if (!moveX && !moveY)
        return;
    if (!in_scene())
        return;

    MMU& mmu = *nj::runtime::g_guest_mem;
    const uint32_t pl = mmu.read32(kPlayerPtr);
    // Le pointeur doit viser MEM1. Un zero veut dire "pas de joueur" (facade,
    // chargement), et n'importe quoi d'autre veut dire qu'on lit au mauvais
    // endroit : dans les deux cas on ne touche a rien.
    if (!in_mem1(pl))
        return;

    if (dt <= 0.0f || dt > 0.25f)
        dt = kRefDt;
    const float scale = sensitivity() * (dt / kRefDt) / kSpeedBase;

    // **Ne pas tourner le corps pendant la mise en joue.**
    //
    // En RE4 le stick ne fait pas pivoter Leon quand il vise : il deplace le
    // reticule. Ecrire rot.y a ce moment-la met notre code et la logique de
    // visee du jeu en concurrence sur le meme angle, et la mise en joue
    // saute - le defaut signale comme "Leon annule son tir". Hors visee, en
    // revanche, rot.y est bien l'angle du corps et l'ecriture directe est
    // exactement ce qu'on veut.
    const bool turn_body = !aiming;

    float before = 0.0f, after = 0.0f;
    uint32_t dbg_wep = 0, dbg_raw = 0xDEADBEEF;
    float dbg_p0 = 0.0f, dbg_p1 = 0.0f;
    if (moveX && turn_body) {
        before = mmu.read_f32(pl + kRotY);
        if (!std::isfinite(before))
            return;
        after = before - (float) dx * scale;
        mmu.write_f32(pl + kRotY, after);
    }

    // **Le tangage n'est plus ecrit ici.**
    //
    // Il l'etait, en meme temps que la source d'entree pilotait le reticule
    // par le stick : deux commandes sur le meme axe, qui se disputent. Le
    // tangage partait a la butee et Leon pointait le sol ou le ciel - ce
    // qui, a l'ecran, ressemble exactement a une visee qui se defait. C'est
    // l'explication du "il annule son tir".
    //
    // La visee verticale passe donc par le stick, comme le jeu l'attend, et
    // comme l'horizontale pendant la mise en joue. L'ecriture directe ne
    // sert plus qu'a tourner le corps **hors** visee, la ou aucune logique
    // du jeu ne la dispute.
    (void) dy;

    static const bool dbg = NJ_ENV("NJ_INPUTDBG") != nullptr;
    if (dbg) {
        static int n = 0;
        if (n++ < 40) {
            NJ_PRINTF("[Turn] rot.y %.4f->%.4f | pWep=%08X brut=%08X pitch %.4f->%.4f\n",
                        before, after, dbg_wep, dbg_raw, dbg_p0, dbg_p1);
            std::fflush(stdout);
        }
    }
}

} // namespace nj::runtime::input
