// -*- coding: utf-8 -*-
// Ce que les modules REL reclament et que ni le DOL ni un autre module ne definit.
//
// Deux cas :
//  * une fonction que le decompile ecrit en ligne dans une classe (`Event::FlgOnStatus`) mais que
//    les salles appellent "hors ligne" par son nom d'epoque : on force clang a l'emettre ;
//  * les outils de debogage (fenetres de reglage, mesures) dont le module n'est pas compile dans
//    le jeu de joueur : leurs points d'entree rendent 0 et le disent une fois.
#include "port_log.h"
#include "types.h"
#include "event.h"

// Event::FlgOnStatus (en ligne dans event.h) sous son nom d'epoque, tel que les salles l'appellent.
extern "C" void port_event_flgon(Event *e, u32 no) __asm__("FlgOnStatus__5EventUl");
extern "C" void port_event_flgon(Event *e, u32 no)
{
    u32 *f = &e->status;
    f[no >> 5] |= 0x80000000 >> (no & 0x1F);
}

int TutilGet3DPosXZ_All(Vec *, Vec *, Vec *) { PORT_TODO("TutilGet3DPosXZ_All"); return 0; }
int TutilGet3DPosXZ_Mov(Vec *, Vec *, Vec *) { PORT_TODO("TutilGet3DPosXZ_Mov"); return 0; }
int SetToolLight(int) { PORT_TODO("SetToolLight"); return 0; }

struct cDbgWindow {
    void Init(int, int, const char *);
};
void cDbgWindow::Init(int, int, const char *) { PORT_TODO("cDbgWindow::Init"); }

namespace t_esp_namespace {
struct TOOL_WINDOW {
    void *operator new(unsigned int);
};
void *TOOL_WINDOW::operator new(unsigned int) { PORT_TODO("TOOL_WINDOW::operator new"); return 0; }
} // namespace t_esp_namespace
