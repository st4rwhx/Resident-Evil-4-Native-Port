// -*- coding: utf-8 -*-
// Couche VI du portage : l'horloge d'image.
//
// VI est le composant le plus sous-estime de la liste. Il n'affiche rien ici - la presentation
// viendra avec GX/GLES - mais `VIWaitForRetrace` est **la cadence de tout le jeu** : la boucle
// principale de RE4 s'y bloque une fois par image. Deux facons de se tromper, symetriques :
//
//   * rendre la main immediatement : le jeu tourne a la vitesse du processeur, la physique et
//     les animations s'emballent, et le portage a l'air casse sans qu'on sache pourquoi ;
//   * ne jamais rendre la main : le jeu se fige au premier appel.
//
// On attend donc vraiment, sur une base de temps absolue plutot qu'en dormant une duree fixe :
// dormir 16,7 ms par image accumule la derive de tout ce que l'image a coute. Le compteur de
// retraces avance meme si on a pris du retard, sinon le jeu croirait le temps arrete.
//
// La callback de post-retrace est appelee ici. RE4 s'en sert pour son propre comptage de temps
// et pour armer la lecture des manettes ; l'oublier donne un jeu qui tourne mais n'avance pas.
#include <cstdlib>
#include <cstdio>
#include "port_log.h"

#include <cstdio>
#include <ctime>
#include <time.h>

#include <dolphin.h>

namespace {

// NTSC : 59,94 Hz entrelace, soit un champ toutes les ~16683 microsecondes.
const u64 FIELD_NS = 16683333ull;

VIRetraceCallback g_post_cb = nullptr;
void *g_fb_pending = nullptr;
void *g_fb_current = nullptr;
u32 g_retrace = 0;
BOOL g_black = FALSE;
bool g_inited = false;
u64 g_next_ns = 0;
u32 g_width = 640, g_height = 448;

u64 now_ns()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
}

} // namespace

// Ce que la couche de presentation (GX/GLES) viendra lire pour savoir quoi afficher.
extern "C" void *port_vi_framebuffer() { return g_fb_current; }
extern "C" u32 port_vi_retrace_count() { return g_retrace; }
extern "C" void port_vi_size(u32 *w, u32 *h) { if (w) *w = g_width; if (h) *h = g_height; }

void VIInit(void)
{
    g_inited = true;
    g_retrace = 0;
    g_next_ns = now_ns();
    std::printf("[PORT] VIInit : cadence %llu us par champ\n",
                (unsigned long long)(FIELD_NS / 1000ull));
}

void VIConfigure(const GXRenderModeObj *rm)
{
    if (rm) {
        g_width = rm->fbWidth;
        g_height = rm->xfbHeight;
        std::printf("[PORT] VIConfigure : %ux%u\n", g_width, g_height);
    }
}

void VIConfigurePan(u16 xOrg, u16 yOrg, u16 width, u16 height)
{
    (void)xOrg; (void)yOrg;
    if (width) g_width = width;
    if (height) g_height = height;
}

void VISetNextFrameBuffer(void *fb) { g_fb_pending = fb; }

// Sur le materiel, VIFlush valide les registres ecrits depuis le dernier flush ; ici cela se
// reduit a rendre effectif le tampon suivant.
// Image finie (le jeu a copie l'EFB vers le tampon d'affichage puis valide) : rendu natif (gx_sdk.cpp).
extern "C" void port_gx_frame(void);
void VIFlush(void)
{
    if (g_fb_pending) g_fb_current = g_fb_pending;
    port_gx_frame();
}

void VIWaitForRetrace(void)
{
    if (!g_inited)
        VIInit();

    // Base de temps absolue : on vise le prochain instant de champ au lieu de dormir une duree
    // fixe, sinon le cout de l'image s'ajoute a l'attente et le jeu derive lentement.
    const u64 t = now_ns();
    // Mesure (N1) : RE4_NOPACE=1 supprime l'attente du retrace pour voir le cout reel de la logique;
    // toutes les 120 images on imprime le temps moyen par retrace.
    static const bool nopace = getenv("RE4_NOPACE") != nullptr;
    static u64 t_mark = 0; static u32 n_mark = 0;
    if (t_mark == 0) t_mark = t;
    if (++n_mark == 120) {
        std::printf("[PERF] %u retraces : %.2f ms chacun\n", n_mark, (double)(t - t_mark) / 1e6 / n_mark);
        std::fflush(stdout);
        t_mark = t; n_mark = 0;
    }
    if (nopace) {
        g_next_ns = t + FIELD_NS;
    } else if (g_next_ns > t) {
        const u64 wait = g_next_ns - t;
        timespec ts;
        ts.tv_sec = (time_t)(wait / 1000000000ull);
        ts.tv_nsec = (long)(wait % 1000000000ull);
        nanosleep(&ts, nullptr);
        g_next_ns += FIELD_NS;
    } else {
        // En retard : on ne rattrape pas en n'attendant pas du tout pendant dix images, on
        // recale la cible sur maintenant. Le compteur avance quand meme.
        g_next_ns = t + FIELD_NS;
    }

    ++g_retrace;
    if (g_fb_pending)
        g_fb_current = g_fb_pending;

    // RE4 compte son temps et arme la lecture des manettes ici : l'oublier donne un jeu qui
    // tourne mais n'avance pas.
    if (g_post_cb)
        g_post_cb(g_retrace);
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb)
{
    VIRetraceCallback old = g_post_cb;
    g_post_cb = cb;
    return old;
}

void VISetBlack(BOOL black) { g_black = black; }

// Entrelace : le champ alterne a chaque retrace.
u32 VIGetNextField(void) { return g_retrace & 1u; }
u32 VIGetTvFormat(void) { return VI_NTSC; }
u32 VIGetDTVStatus(void) { return 0; }   // pas de sortie composante : le jeu reste en 4:3
