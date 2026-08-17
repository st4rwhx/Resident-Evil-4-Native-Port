// -*- coding: utf-8 -*-
// Couche PAD du portage : la manette GameCube, alimentee plus tard par le tactile.
//
// Le jeu lit quatre manettes a chaque image. On en declare **une seule branchee**, la 0, et on
// rend `PAD_ERR_NO_CONTROLLER` pour les trois autres : declarer quatre manettes ferait apparaitre
// des joueurs fantomes dans les menus a deux, et n'en declarer aucune bloque RE4 sur son ecran
// "connectez une manette".
//
// L'etat vient de `port_pad_set()`, que la couche tactile appellera. Tant qu'elle n'existe pas,
// la manette est branchee et au repos - le jeu tourne, personne n'appuie sur rien. C'est ce qu'il
// faut pour atteindre le premier ecran sans dependre de l'interface.
//
// Une chose a ne pas rater : sur GameCube les sticks sont **centres sur zero et signes**, et le
// jeu applique sa propre zone morte par-dessus. Un tactile qui enverrait 0..255 non signe ferait
// courir Leon en permanence vers le coin haut-droit.
#include "port_log.h"
#include "njlink.h"
extern "C" void njstate_fill(NjState* out);   // port/njstate.cpp

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dolphin.h>
extern "C" int port_studio_pad(PADStatus* s);   // port/studio.cpp
extern "C" int port_menu_scene_active(void);    // port/arm_hooks.cpp : scene du menu titre de Nightjar

namespace {

PADStatus g_state;          // etat de la manette 0, alimente par le tactile
bool g_inited = false;

s8 clamp_stick(int v)
{
    if (v > 127) return (s8)127;
    if (v < -128) return (s8)-128;
    return (s8)v;
}

} // namespace

// ---------------------------------------------------------------- entree du tactile
//
// `buttons` utilise les bits PAD_BUTTON_* du SDK. Les sticks sont signes, -128..127, centres
// sur zero. Les gachettes sont 0..255 non signees : sur GameCube elles sont analogiques, et
// RE4 s'en sert pour viser (L) et courir (R), donc une valeur binaire 0/255 marche mais perd
// la nuance.
extern "C" void port_pad_set(u16 buttons, int stickX, int stickY,
                             int subX, int subY, int trigL, int trigR)
{
    g_state.button = buttons;
    g_state.stickX = clamp_stick(stickX);
    g_state.stickY = clamp_stick(stickY);
    g_state.substickX = clamp_stick(subX);
    g_state.substickY = clamp_stick(subY);
    g_state.triggerLeft = (u8)(trigL < 0 ? 0 : (trigL > 255 ? 255 : trigL));
    g_state.triggerRight = (u8)(trigR < 0 ? 0 : (trigR > 255 ? 255 : trigR));
    g_state.analogA = (buttons & PAD_BUTTON_A) ? 255 : 0;
    g_state.analogB = (buttons & PAD_BUTTON_B) ? 255 : 0;
    g_state.err = PAD_ERR_NONE;
}

// ---------------------------------------------------------------- API PAD

BOOL PADInit()
{
    std::memset(&g_state, 0, sizeof(g_state));
    g_state.err = PAD_ERR_NONE;
    g_inited = true;
    std::printf("[PORT] PADInit : une manette sur le canal 0\n");
    return TRUE;
}

static unsigned long g_pad_reads_total = 0;
static unsigned long pad_reads_total() { return g_pad_reads_total; }

u32 PADRead(PADStatus *status)
{
    if (!status)
        return 0;
    ++g_pad_reads_total;
    if (!g_inited)
        PADInit();

    status[0] = g_state;
    {   // appli Nightjar : sa manette (tactile ou physique) sur le canal 0
        NjPad np;
        // RE4_PAD_NOHOST=1 (banc de mesure, 14/11) : manette de l'appli ignoree, scene immobile. Sans lui, Leon bougeait
        // seul pendant les essais automatiques : on journalise les etats non neutres recus pour en trouver la source.
        static const bool nohost = getenv("RE4_PAD_NOHOST") != nullptr;
        const bool got = njlink_pad(&np);
        if (got) {
            static unsigned logged = 0;
            const bool neutral = np.buttons == 0 && std::abs((int)np.stick_x) < 8 && std::abs((int)np.stick_y) < 8 &&
                                 std::abs((int)np.substick_x) < 8 && std::abs((int)np.substick_y) < 8 && np.trigger_l < 16 &&
                                 np.trigger_r < 16;
            if (!neutral && logged < 12) {
                ++logged;
                std::printf("[PORT] PAD de l'appli non neutre (lecture %lu) : boutons %04x stick %d,%d c-stick %d,%d gachettes %u,%u%s\n",
                            pad_reads_total(), (unsigned)np.buttons, (int)np.stick_x, (int)np.stick_y, (int)np.substick_x,
                            (int)np.substick_y, (unsigned)np.trigger_l, (unsigned)np.trigger_r, nohost ? " (ignore)" : "");
            }
        }
        if (got && !nohost) {
            status[0].button = np.buttons;
            status[0].stickX = np.stick_x;
            status[0].stickY = np.stick_y;
            status[0].substickX = np.substick_x;
            status[0].substickY = np.substick_y;
            status[0].triggerLeft = np.trigger_l;
            status[0].triggerRight = np.trigger_r;
        }
    }
    {   // scene du menu titre de Nightjar (18/11) : Leon ne bouge pas, l'accueil de l'appli a la main
        if (port_menu_scene_active()) {
            status[0].button = 0;
            status[0].stickX = status[0].stickY = status[0].substickX = status[0].substickY = 0;
            status[0].triggerLeft = status[0].triggerRight = 0;
        }
    }
    port_studio_pad(&status[0]);   // Nightjar Studio (port/studio.cpp) : manette imposee par une commande « pad »
    // Banc d'essai : RE4_AUTOPAD=1 appuie sur Start puis sur A a intervalles reguliers, pour faire
    // avancer le jeu sans manette (utilise par les mesures sur appareil, jamais dans une diffusion).
    // RE4_PADFILE=<fichier> : un parcours scripte, une ligne par appui :
    //   <lecture de depart> <nombre de lectures> <masque hex> [stickX stickY]
    // (une lecture = un appel de PADRead, soit une image de jeu). Les masques sont ceux de PAD_BUTTON_*.
    static unsigned long reads = 0;
    static int nscript = -1;
    struct Step { unsigned long start, len; unsigned mask; int sx, sy; };
    static Step script[256];
    if (nscript < 0) {
        nscript = 0;
        const char *pf = getenv("RE4_PADFILE");
        std::FILE *f = pf ? std::fopen(pf, "r") : nullptr;
        if (f) {
            char line[128];
            while (nscript < 256 && std::fgets(line, sizeof(line), f)) {
                Step st = { 0, 0, 0, 0, 0 };
                if (std::sscanf(line, "%lu %lu %x %d %d", &st.start, &st.len, &st.mask, &st.sx, &st.sy) >= 3)
                    script[nscript++] = st;
            }
            std::fclose(f);
            std::printf("[PORT] PAD : %d etapes scriptees\n", nscript);
        }
    }
    if (nscript > 0) {
        for (int i = 0; i < nscript; ++i)
            if (reads >= script[i].start && reads < script[i].start + script[i].len) {
                status[0].button |= (u16)script[i].mask;
                status[0].stickX = (s8)script[i].sx;
                status[0].stickY = (s8)script[i].sy;
            }
        ++reads;
    }
    // Banc d'essai : RE4_AUTOLOAD=1 charge la sauvegarde 01 en suivant l'etat du jeu (njstate.cpp), pas l'horloge :
    // logos et « PRESS START » -> Start ; menu du titre -> haut puis A (NEW GAME) ; ecran de la carte -> une fois
    // haut (le curseur est sur 02) puis A ; texte qui attend -> A. Une fois en jeu, plus rien : Leon reste immobile.
    static const bool autoload = getenv("RE4_AUTOLOAD") != nullptr;
    if (autoload) {
        static unsigned k = 0, card_reads = 0;
        static bool in_game = false, up_done = false;
        NjState st;
        njstate_fill(&st);
        ++k;
        const bool pulse = (k % 40) < 4;   // un appui de 4 lectures toutes les 40
        if (st.mode == 1 && (st.bits & NJS_PLAYER))
            in_game = true;
        if (st.bits & NJS_CARD) {
            ++card_reads;
            if (!up_done && card_reads == 120) { status[0].button |= PAD_BUTTON_UP; up_done = true; }
            else if (card_reads > 160 && pulse) status[0].button |= PAD_BUTTON_A;
        } else if ((st.bits & NJS_TEXT_WAIT) && !in_game) {   // 18/11 : plus d'appui A une fois en jeu (le commentaire
            // le promettait ; A = bouton d'action de Leon : il se declenchait tout seul a chaque texte en attente)
            if (pulse) status[0].button |= PAD_BUTTON_A;
        } else if (!in_game) {
            if (st.bits & NJS_TITLE_MENU) {   // NEW GAME est au-dessus de LOAD (curseur par defaut) : une fois haut, puis A
                // RE4_AUTOLOAD=2 : LOAD (curseur par defaut, pas de haut) -> ecran de carte -> sauvegarde 01 (scene avec
                // ennemis, verification n 2 du 13/11) ; sinon NEW GAME.
                static const bool load = getenv("RE4_AUTOLOAD")[0] == '2';
                static unsigned menu_reads = 0;
                static bool menu_up = load;
                if (st.title_menu_alpha > 0.9f) ++menu_reads;
                if (!menu_up && menu_reads == 60) { status[0].button |= PAD_BUTTON_UP; menu_up = true; }
                else if (menu_up && menu_reads > 100 && pulse) status[0].button |= PAD_BUTTON_A;
            }
            else if (st.mode == 6 && pulse) status[0].button |= PAD_BUTTON_START;
        }
        // RE4_AUTOWALK=<lectures> : en jeu, Leon avance tout droit ce nombre de lectures de manette (zones d'evenement
        // de la salle, ex. l'appel radio de r100), puis plus rien.
        static const char* walk_env = getenv("RE4_AUTOWALK");
        static unsigned walk_left = walk_env ? (unsigned) atoi(walk_env) : 0;
        if (in_game && walk_left && !(st.bits & NJS_TEXT_WAIT)) {
            --walk_left;
            status[0].stickY = 90;
        }
    }
    static const bool autopad = getenv("RE4_AUTOPAD") != nullptr;
    if (autopad) {
        static unsigned cnt = 0;
        const unsigned k = cnt++ % 300;
        if (k >= 100 && k < 106) status[0].button |= PAD_BUTTON_START;
        if (k >= 200 && k < 206) status[0].button |= PAD_BUTTON_A;
    }

    // Les trois autres canaux sont vides. `PAD_ERR_NO_CONTROLLER` plutot que des zeros : avec
    // err = PAD_ERR_NONE le jeu croirait trois manettes branchees au repos.
    for (int i = 1; i < PAD_MAX_CONTROLLERS; ++i) {
        std::memset(&status[i], 0, sizeof(PADStatus));
        status[i].err = PAD_ERR_NO_CONTROLLER;
    }

    // La valeur de retour est un masque des canaux dont la lecture a echoue ce tour-ci ; rien
    // n'echoue chez nous.
    return 0;
}

// Zone morte et mise a l'echelle des sticks. Le materiel rend des valeurs brutes decentrees et
// le SDK les redresse ; notre source est deja propre, donc il n'y a qu'a borner.
void PADClamp(PADStatus *status)
{
    if (!status)
        return;
    for (int i = 0; i < PAD_MAX_CONTROLLERS; ++i) {
        if (status[i].err != PAD_ERR_NONE)
            continue;
        status[i].stickX = clamp_stick(status[i].stickX);
        status[i].stickY = clamp_stick(status[i].stickY);
        status[i].substickX = clamp_stick(status[i].substickX);
        status[i].substickY = clamp_stick(status[i].substickY);
    }
}

int PADReset(u32 mask) { (void)mask; if (!g_inited) PADInit(); return 1; }
BOOL PADRecalibrate(u32 mask) { (void)mask; return TRUE; }

// Le mode analogique choisit lesquels des champs analogiques sont renseignes. On les remplit
// tous, donc le mode n'a pas d'effet ici.
void PADSetAnalogMode(u32 mode) { (void)mode; }

// Pas de vibration : un telephone a bien un vibreur, mais le declencher a la cadence d'une
// manette GameCube donnerait un buzz continu. A reprendre avec la couche tactile.
void PADControlMotor(s32 chan, u32 command) { (void)chan; (void)command; }
