// -*- coding: utf-8 -*-
// Couche VI de la cible PC : l'horloge d'image, branchee sur aurora.
//
// Sur GameCube, VI est l'horloge de tout le jeu : la boucle principale de RE4 se bloque une
// fois par image dans `VIWaitForRetrace`. aurora, elle, attend qu'on l'appelle - son VI n'a
// aucune pompe (`VIFlush()` y est vide) et c'est a l'application de faire tourner la fenetre.
// Les deux se rejoignent naturellement : le retrace du jeu **est** la frontiere d'image
// d'aurora. On y ferme l'image en cours, on vide la file d'evenements, on en ouvre une autre.
//
// aurora fournit deja VIInit, VIConfigure, VIConfigurePan, VIFlush et VIGetTvFormat ; on ne
// redefinit que les sept fonctions qu'elle n'a pas, sans quoi l'editeur de liens verrait
// double. C'est aussi pourquoi ce fichier remplace `port/vi.cpp` sur cette cible.
#include <aurora/aurora.h>
#include <aurora/event.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <time.h>

// Pas de <dolphin.h> ici. Les en-tetes du decompile portent leur propre libc (include/libc/),
// qui redefinit FILE ; SDL3, tire par <aurora/event.h>, amene celle du systeme, et les deux
// declarations s'excluent. Ce fichier n'a besoin que de trois types du SDK, repris a
// l'identique de include/dolphin/types.h et include/dolphin/vi/vitypes.h - si l'un d'eux
// changeait, l'editeur de liens ne dirait rien, alors ils sont recopies tels quels.
typedef unsigned long u32;
typedef unsigned long long u64;
typedef int BOOL;
#define FALSE 0
typedef void (*VIRetraceCallback)(u32 retraceCount);

// Liaison C : include/dolphin/vi/vifuncs.h place tout VI sous `extern "C"`, et c est ce nom-la
// que le reste du jeu appelle.
extern "C" {
void VISetNextFrameBuffer(void *fb);
void VISetBlack(BOOL black);
u32 VIGetNextField(void);
u32 VIGetDTVStatus(void);
VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb);
VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback cb);
void VIWaitForRetrace(void);
}


namespace {

// NTSC : 59,94 Hz entrelace, soit un champ toutes les ~16683 microsecondes. On garde cette
// cadence meme quand aurora est en vsync : elle sert de plafond si la presentation rend la
// main trop vite (fenetre masquee, vsync desactivee par le pilote).
const u64 FIELD_NS = 16683333ull;

VIRetraceCallback g_post_cb = nullptr;
VIRetraceCallback g_pre_cb = nullptr;
void *g_fb_pending = nullptr;
void *g_fb_current = nullptr;
u32 g_retrace = 0;
BOOL g_black = FALSE;
u64 g_next_ns = 0;
bool g_frame_open = false;
bool g_paused = false;
bool g_exiting = false;

u64 now_ns()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
}

void poll_events()
{
    const AuroraEvent *ev = aurora_update();
    while (ev != nullptr && ev->type != AURORA_NONE) {
        switch (ev->type) {
        case AURORA_EXIT:     g_exiting = true; break;
        case AURORA_PAUSED:   g_paused = true;  break;
        case AURORA_UNPAUSED: g_paused = false; break;
        default: break;
        }
        ++ev;
    }
}

void sleep_ns(u64 ns)
{
    timespec ts;
    ts.tv_sec = (time_t)(ns / 1000000000ull);
    ts.tv_nsec = (long)(ns % 1000000000ull);
    nanosleep(&ts, nullptr);
}

} // namespace

// La premiere image doit etre ouverte avant que le jeu ne dessine quoi que ce soit : main_port
// l'appelle juste apres aurora_initialize.
extern "C" void port_vi_first_frame()
{
    poll_events();
    g_frame_open = aurora_begin_frame();
    g_next_ns = now_ns() + FIELD_NS;
}

extern "C" void *port_vi_framebuffer() { return g_fb_current; }
extern "C" u32 port_vi_retrace_count() { return g_retrace; }

extern "C" void VISetNextFrameBuffer(void *fb) { g_fb_pending = fb; }
extern "C" void VISetBlack(BOOL black) { g_black = black; }
extern "C" u32 VIGetNextField(void) { return g_retrace & 1u; }
extern "C" u32 VIGetDTVStatus(void) { return 0; }   // pas de sortie composante : le jeu reste en 4:3

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb)
{
    VIRetraceCallback old = g_post_cb;
    g_post_cb = cb;
    return old;
}

VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback cb)
{
    VIRetraceCallback old = g_pre_cb;
    g_pre_cb = cb;
    return old;
}

extern "C" void VIWaitForRetrace(void)
{
    if (g_pre_cb)
        g_pre_cb(g_retrace);

    // Fin de l'image : tout ce que le jeu a soumis a GX depuis le dernier retrace part ici.
    if (g_frame_open) {
        aurora_end_frame();
        g_frame_open = false;
    }

    poll_events();
    if (g_exiting) {
        // Fermer la fenetre doit arreter le jeu. Le decompile n'a aucun chemin de sortie
        // propre - sa boucle principale est infinie - donc on sort d'ici.
        std::printf("[PORT] fenetre fermee, arret apres %u images\n", g_retrace);
        std::fflush(stdout);
        aurora_shutdown();
        std::_Exit(0);
    }

    // Plafond de cadence, sur base de temps absolue : viser le prochain instant de champ
    // plutot que dormir une duree fixe, sinon le cout de l'image s'ajoute a l'attente.
    const u64 t = now_ns();
    if (g_next_ns > t) {
        sleep_ns(g_next_ns - t);
        g_next_ns += FIELD_NS;
    } else {
        g_next_ns = t + FIELD_NS;
    }

    ++g_retrace;
    if (g_fb_pending)
        g_fb_current = g_fb_pending;

    // RE4 compte son temps et arme la lecture des manettes ici : l'oublier donne un jeu qui
    // tourne mais n'avance pas.
    if (g_post_cb)
        g_post_cb(g_retrace);

    // Image suivante. En pause, aurora refuse d'en ouvrir une ; le jeu continuerait alors a
    // tourner sans rien presenter, donc on attend ici que la fenetre revienne.
    while (!g_exiting) {
        if (!g_paused) {
            g_frame_open = aurora_begin_frame();
            if (g_frame_open)
                break;
        }
        sleep_ns(FIELD_NS);
        poll_events();
    }
}
