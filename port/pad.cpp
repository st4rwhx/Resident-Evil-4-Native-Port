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

#include <cstdio>
#include <cstring>

#include <dolphin.h>

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

u32 PADRead(PADStatus *status)
{
    if (!status)
        return 0;
    if (!g_inited)
        PADInit();

    status[0] = g_state;

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
