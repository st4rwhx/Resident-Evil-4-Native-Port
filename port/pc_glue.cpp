// -*- coding: utf-8 -*-
// Le peu qu'aurora ne fournit pas et que le jeu reclame quand meme, sur la cible PC.
//
// Trois fonctions seulement. Elles sont ici plutot que dans les talons engendres parce
// qu'elles ne sont pas des talons : elles font vraiment leur travail.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

// Pas <dolphin/os.h> : il declare OSReport en `__declspec(weak)`, que MinGW traduit par un
// alias faible au nom decore (.weak.OSReport....) - notre definition ne repondait alors a
// personne, et l editeur de liens reclamait toujours OSReport. On declare donc a la main.
// Les types (u32, BOOL) viennent de <dolphin/types.h>, tire par texPalette.h.
#define PORT_LC_ALIGN __attribute__((aligned(32)))
#include <charPipeline/texPalette.h>

extern "C" {

// Le journal de la console de developpement. RE4 l'appelle pour ses messages d'erreur, et les
// perdre reviendrait a se priver du peu que le jeu dit de lui-meme au demarrage.
void OSReport(const char *msg, ...)
{
    std::va_list ap;
    va_start(ap, msg);
    std::vprintf(msg, ap);
    va_end(ap);
    std::fflush(stdout);
}

// Pas de debogueur materiel ici. Le jeu s'en sert pour decider s'il peut s'arreter sur un
// point d'arret ; repondre non est la reponse juste, pas un pis-aller.
BOOL DBIsDebuggerPresent(void) { return FALSE; }

// Rend le descripteur numero `id` d'une palette de textures. `calcTplAddr` a deja transforme
// les decalages du fichier en adresses, donc `descriptorArray` est ici un vrai pointeur.
// (Meme implementation que dans port/texobj.cpp, qui est la couche GLES du telephone et
// n'entre pas dans la construction PC.)
TEXDescriptorPtr TEXGet(TEXPalettePtr pal, u32 id)
{
    if (!pal || id >= pal->numDescriptors)
        return (TEXDescriptorPtr) 0;
    return &pal->descriptorArray[id];
}

} // extern "C"

// ---------------------------------------------------------------------------------------
// Ce que le SDK d'aurora ne couvre pas.

extern "C" {

// Interruptions. Le jeu les coupe autour de ses sections critiques - manipulation de listes
// d'objets, arrangement du tas. Notre ordonnancement est cooperatif (port/thread.cpp : des
// coroutines a jeton unique), donc rien ne peut s'intercaler : il n'y a rien a couper, et
// rendre l'ancien etat suffit a ce que les paires enable/restore restent coherentes.
BOOL OSDisableInterrupts(void) { return TRUE; }
BOOL OSEnableInterrupts(void) { return TRUE; }
BOOL OSRestoreInterrupts(BOOL level) { return level; }

// Le MSR et la barriere de synchronisation du PowerPC. Sans objet hors du Gekko.
void PPCMtmsr(u32 newMSR) { (void) newMSR; }
void PPCSync(void) {}

// 0x00000001 : une console de developpement. C'est ce que le jeu attend pour activer ses
// menus de debogage, et c'est la machine que nous sommes.
u32 OSGetConsoleType(void) { return 0x00000001u; }
u32 OSGetConsoleSimulatedMemSize(void) { return 24u * 1024u * 1024u; }

// Pas de bouton reset sur un PC.
BOOL OSGetResetButtonState(void) { return FALSE; }

void OSPanic(const char *file, int line, const char *msg, ...)
{
    std::va_list ap;
    va_start(ap, msg);
    std::fprintf(stderr, "[PANIC] %s:%d ", file, line);
    std::vfprintf(stderr, msg, ap);
    std::fputc('\n', stderr);
    va_end(ap);
    std::fflush(stderr);
    std::abort();
}

// Le locked cache : 16 Ko de SRAM du Gekko que RE4 utilise comme brouillon rapide. Ici, de la
// memoire ordinaire alignee. Le jeu n'en attend aucune propriete particuliere hors la vitesse.
void *port_lc_base(void)
{
    static PORT_LC_ALIGN unsigned char lc[16 * 1024];
    return lc;
}

} // extern "C"

// ---------------------------------------------------------------------------------------
// Le write-gather pipe, sur la cible PC.
//
// Une partie du jeu - le rendu de texte (mes.cpp) et le module de debogage - n'appelle pas de
// fonction pour poser un sommet : elle ecrit les composantes a la suite dans une fenetre
// materielle du Gekko, et le GPU les interprete selon le descripteur de sommet courant
// (voir port/wgpipe.h). Reproduire cette machine a etats demande de connaitre le descripteur,
// ce que sait l'interpreteur d'aurora et pas nous.
//
// Ces ecritures tombent donc dans un puits pour l'instant, en le disant une fois. Consequence
// attendue et delimitee : le texte du jeu ne s'affichera pas tant que ce chemin n'est pas
// rebranche sur la soumission de sommets d'aurora. Rien d'autre ne passe par ici.
#include "port_log.h"

extern "C" {

void port_wg_push_u8(u8 v) { (void) v; PORT_TODO("write-gather pipe (u8)"); }
void port_wg_push_s8(s8 v) { (void) v; PORT_TODO("write-gather pipe (s8)"); }
void port_wg_push_u16(u16 v) { (void) v; PORT_TODO("write-gather pipe (u16)"); }
void port_wg_push_s16(s16 v) { (void) v; PORT_TODO("write-gather pipe (s16)"); }
void port_wg_push_u32(u32 v) { (void) v; PORT_TODO("write-gather pipe (u32)"); }
void port_wg_push_s32(s32 v) { (void) v; PORT_TODO("write-gather pipe (s32)"); }
void port_wg_push_f32(f32 v) { (void) v; PORT_TODO("write-gather pipe (f32)"); }

// Deux fonctions GX qu'aurora n'a pas. L'index de matrice par sommet appartient au meme
// chemin de soumission directe que ci-dessus ; le masque indirect gouverne les etages de
// texture indirecte, que RE4 configure mais dont l'absence ne bloque aucun rendu de base.
void GXMatrixIndex1u8(u8 idx) { (void) idx; PORT_TODO("GXMatrixIndex1u8"); }
void __GXSetIndirectMask(u32 mask) { (void) mask; PORT_TODO("__GXSetIndirectMask"); }

} // extern "C"

// ---------------------------------------------------------------------------------------
// Qui a le droit d'allouer dans le tas du jeu.
//
// RE4 remplace l'operateur new global, donc chaque `new` du programme entier y passe - y
// compris ceux des fils d'aurora. Or ce tas n'a pas de verrou : sur GameCube, couper les
// interruptions suffisait. On marque donc le fil du jeu, et lui seul y alloue.
#include <thread>

namespace {
std::thread::id g_game_thread;
bool g_game_thread_set = false;
}

extern "C" void port_mark_game_thread(void)
{
    g_game_thread = std::this_thread::get_id();
    g_game_thread_set = true;
}

extern "C" int port_on_game_thread(void)
{
    return (!g_game_thread_set || std::this_thread::get_id() == g_game_thread) ? 1 : 0;
}

// Un mot sur la sortie standard pour le code du jeu, qui ne peut pas inclure <stdio.h> : ses
// propres en-tetes declarent deja printf et sprintf, autrement.
extern "C" void port_note(const char *fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    std::vprintf(fmt, ap);
    va_end(ap);
}
