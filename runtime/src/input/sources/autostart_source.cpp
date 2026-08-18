#include "autostart_source.h"
#include "runtime/log.h"

#include "input/touch_source.h"
#include "input/re4_context.h"
#include "runtime/env.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>

// Compteurs du runtime, hors namespace (src/hle/gx.cpp).
extern uint64_t g_st_frames;
extern uint64_t g_stat_draws;

namespace nj::runtime::input {

namespace {
// Bit de la manette GameCube, comme ailleurs dans le runtime. A n'y est pas :
// voir update(), il boucle sur le dialogue de carte memoire.
constexpr uint16_t kButtonStart = 0x1000;
constexpr uint16_t kButtonA     = 0x0100;
constexpr uint16_t kButtonUp    = 0x0008;

// La sequence, et pourquoi elle est dans cet ordre.
//
// Avant la facade le jeu affiche "No Memory Card found in Slot A". Le
// decompile donne le comportement exact (src/game/card.cpp, mode 2, case 2) :
//
//     cMes.mes[0].cursor = 1;
//
// Le curseur arrive donc sur la **seconde** option, "Retry". Un A seul
// relance la recherche de carte, qui echoue, et le dialogue revient - la
// boucle sans fin mesuree a 167 tirages par image pendant 100 s.
//
// Dans le meme fichier, `case 3:` montre que `result == 1` (la premiere
// option, "Continue without saving") mene a `mode = 4`, c'est-a-dire la
// suite. Il faut donc **remonter le curseur avant de valider**.
//
// Haut est idempotent : un curseur deja en haut y reste. La sequence peut
// donc tourner en rond sans jamais se desynchroniser, quel que soit l'ecran
// sur lequel elle tombe.
const uint16_t kSequence[] = {kButtonUp, kButtonA, kButtonStart};
constexpr int kSeqLen = (int) (sizeof(kSequence) / sizeof(kSequence[0]));

// Cadence en **temps reel**, pas en appels a update().
//
// Premiere version : deux appels d'appui sur dix. Faux. update() est appele
// depuis une boucle hote en sleep_for(16 ms), donc a ~62 Hz, et depuis deux
// endroits ; le jeu, lui, lit la manette a sa propre cadence, 22 a 28 images
// par seconde. Un carre a 62 Hz echantillonne a 25 Hz se replie : le jeu lit
// "appuye" deux fois de suite et croit a un maintien. C'est ce qu'on voyait -
// A reste enfonce.
//
// Avec une periode en millisecondes, le motif ne depend plus de qui appelle ni
// a quelle frequence. 150 ms d'appui sur 500 : meme a 10 images par seconde -
// une image toutes les 100 ms - le jeu voit au moins un echantillon appuye et
// trois relaches, donc un front franc, deux fois par seconde.
// Un appui toutes les 2,5 s, pas deux par seconde.
//
// Premiere version : 500 ms de periode et six appuis au plafond, donc les six
// etaient consommes dans les trois premieres secondes - avant meme que le
// dialogue de carte memoire n'apparaisse. Un run sur deux gagnait la course,
// l'autre restait bloque sur le dialogue : 230 tirages par image pendant
// 115 secondes. Le plafond etait bon, la cadence etait fausse.
//
// Les confirmations que le jeu attend sont espacees de plusieurs secondes -
// dialogue de carte, menu titre, menu de debug - et chacune arrive apres un
// chargement de duree variable. Huit appuis a 2,5 s couvrent vingt secondes,
// ce qui laisse de la marge sans jamais ressembler a un martelement.
constexpr double kPeriodMs = 2500.0;
constexpr double kHoldMs = 150.0;
constexpr int kMaxPresses = 60;  // filet de securite seulement

double configured_seconds() {
    const char* v = NJ_ENV("NJ_AUTOSTART");
    if (v == nullptr || *v == '\0')
        return 0.0;
    const double s = std::atof(v);
    return s > 0.0 ? s : 150.0;   // "=1" veut dire "oui", pas "une seconde"
}
}

bool AutoStartSource::enabled() { return configured_seconds() > 0.0; }

// P3 : le jeu ne demarre qu'apres la preparation (traduction, cache) ; la sequence repart de la,
// sinon ses appuis tombent sur d'autres ecrans que dans l'executable complet (06/10).
std::atomic<bool> g_autostart_restart{false};

AutoStartSource::AutoStartSource()
    : start_(std::chrono::steady_clock::now()), seconds_(configured_seconds()) {
    NJ_LOG << "[AutoStart] traverse la facade pendant " << seconds_
              << " s (Haut/A/Start, " << kHoldMs << " ms sur " << kPeriodMs
              << " ms)\n";
}

void AutoStartSource::poll(PadState pads[4]) {
    if (done_ || seconds_ <= 0.0)
        return;
    if (g_autostart_restart.exchange(false)) {
        start_ = std::chrono::steady_clock::now();
        presses_ = 0;
        was_down_ = false;
    }
    // Un doigt sur l'ecran : quelqu'un joue, la source se tait. Sur telephone, la facade tourne
    // si lentement que le seuil de tirages ci-dessous peut mettre une minute a se declarer, et
    // Haut/A/Start continueraient en jeu (mallette ouverte, pause).
    if (nj::runtime::input::touch_seen()) {
        done_ = true;
        NJ_LOG << "[AutoStart] contact tactile, la source se tait\n";
        return;
    }

    // Condition d'arret : **le jeu dessine une vraie scene**, pas un minuteur.
    //
    // Les appuis a l'aveugle sur une duree fixe rataient un run sur deux : les
    // confirmations que le jeu attend arrivent apres des chargements de duree
    // variable, donc huit appuis espaces de 2,5 s tombaient juste ou a cote
    // selon la charge de la machine. Une mesure sur un run qui n'atteint pas
    // la scene ne vaut rien, et c'est ce qui a casse chaque comparaison.
    //
    // La facade tourne a quelques centaines de tirages par image, le jeu a
    // plusieurs milliers : le seuil est franc. Le runtime tient deja ces deux
    // compteurs, donc aucune lecture de memoire invitee n'est necessaire.
    // Premier critere : l'etat du jeu lu en memoire (Leon jouable), le meme que la couche tactile.
    // Le seuil de tirages ci-dessous ne se declarait jamais dans les petites salles (moins de
    // 3000 tirages par image) : les appuis continuaient en jeu et ouvraient le menu pause.
    // L'etat doit durer 2 s d'affilee : a l'arrivee dans une salle qui ouvre sur une video (1-20 :
    // l'introduction), Leon existe deja un instant avant que la video ne demarre.
    {
        static std::chrono::steady_clock::time_point since{};
        const Re4Context ctx = re4_context_read();
        const auto now = std::chrono::steady_clock::now();
        if (ctx.valid && ctx.mode == Re4Mode::Gameplay && ctx.player) {
            if (since == std::chrono::steady_clock::time_point{})
                since = now;
            if (now - since > std::chrono::seconds(2)) {
                done_ = true;
                NJ_LOG << "[AutoStart] jeu jouable (etat du jeu), la source se tait\n";
                return;
            }
        } else {
            since = {};
        }
    }
    {
        const uint64_t f = ::g_st_frames, d = ::g_stat_draws;
        if (f > last_frames_ + 30) {
            const uint64_t per = (d - last_draws_) / (f - last_frames_);
            last_frames_ = f;
            last_draws_ = d;
            if (per > 3000) {
                done_ = true;
                NJ_LOG << "[AutoStart] scene atteinte (" << per
                          << " tirages/image), la source se tait\n";
                return;
            }
        }
    }

    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    if (elapsed >= seconds_) {
        done_ = true;
        NJ_LOG << "[AutoStart] termine apres " << elapsed << " s\n";
        return;
    }

    // Nombre d'appuis **borne**. Le jeu en demande deux : valider le menu
    // titre, puis le menu de debug. On en donne quelques-uns de marge et on se
    // tait ensuite pour de bon. Un marteleur sans fin ouvrirait le menu pause
    // en boucle une fois en jeu, et surtout il ne saurait jamais s'arreter.
    const double phase = std::fmod(elapsed * 1000.0, kPeriodMs);
    const bool down = phase < kHoldMs;
    if (down && !was_down_) {
        if (++presses_ > kMaxPresses) {
            done_ = true;
            NJ_LOG << "[AutoStart] " << kMaxPresses
                      << " appuis donnes, la source se tait\n";
            return;
        }
    }
    was_down_ = down;
    if (down) {
        // Haut, A, Start, en boucle : voir kSequence plus haut.
        pads[0].buttons |= kSequence[(presses_ - 1 + kSeqLen) % kSeqLen];
        pads[0].err = 0;   // au cas ou aucune autre source ne soit active
    }
}

}
