// -*- coding: utf-8 -*-
// Ce qui n'appartient a aucune couche : le journal du SDK et le mixeur audio.
#include <cstdarg>
#include <cstdio>

#include <dolphin.h>

#include "port_log.h"

struct AXVPB;

extern "C" {

// OSReport est la sortie de debogage du SDK. Le jeu s'en sert pour signaler ses propres erreurs
// (chemins invalides, depassements de tas) : la rediriger vers la sortie standard rend ces
// messages visibles des la premiere execution, et ils valent de l'or pour savoir ou il en est.
void OSReport(const char *msg, ...)
{
    std::va_list ap;
    va_start(ap, msg);
    std::printf("[JEU] ");
    std::vprintf(msg, ap);
    va_end(ap);
    std::fflush(stdout);
}

void OSVReport(const char *msg, std::va_list list)
{
    std::printf("[JEU] ");
    std::vprintf(msg, list);
    std::fflush(stdout);
}

// Mixeur AX : le volume, la position stereo et la profondeur d'une voix. Sans couche audio, il
// n'y a rien a regler ; ces appels sont frequents et silencieux par construction.
void MIXInitChannel(AXVPB *p, u32 mode, int input, int auxA, int auxB, int pan, int span,
                    int fader)
{
    (void) p; (void) mode; (void) input; (void) auxA; (void) auxB;
    (void) pan; (void) span; (void) fader;
}

void MIXReleaseChannel(AXVPB *p) { (void) p; }

void MIXSetInput(AXVPB *p, int dB) { (void) p; (void) dB; }
void MIXSetPan(AXVPB *p, int pan) { (void) p; (void) pan; }
void MIXSetSPan(AXVPB *p, int span) { (void) p; (void) span; }

}
