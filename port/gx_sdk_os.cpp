// -*- coding: utf-8 -*-
// Ce que la bibliotheque GX du SDK (et texPalette.c) demande au systeme, en version portage (phase N3).
// Volontairement sans en-tetes du jeu : seuls les noms comptent au lien, et les prototypes des en-tetes
// du SDK se contredisent parfois. Les fonctions qui ignorent leurs arguments sont appelables depuis
// n'importe quel prototype sur l'ABI ARM.
#include <stdint.h>

extern "C" {

// contextes de thread : le portage n'en change pas (fils = pthreads cooperatifs, thread.cpp)
void OSClearContext(void*) {}
void OSSetCurrentContext(void*) {}

// adresses : l'arene du jeu est a 0x80000000 (adresse = pointeur) ; les regions materielles (EFB,
// registres) ne passent plus par ici (GXPeek*/GXPoke* et port_gx_reg_base)
void* OSPhysicalToUncached(uint32_t pa) { return (void*) (uintptr_t) (pa | 0x80000000u); }
uint32_t OSUncachedToPhysical(void* p) { return (uint32_t) (uintptr_t) p & 0x3FFFFFFFu; }

void OSRegisterResetFunction(void*) {}
void OSRegisterVersion(const char*) {}
uint32_t __OSUnmaskInterrupts(uint32_t) { return 0; }

// registres du PowerPC (tampon d'ecriture groupee) : sans objet
uint32_t PPCMfhid2(void) { return 0; }
void PPCMthid2(uint32_t) {}
uint32_t PPCMfwpar(void) { return 0; }
void PPCMtwpar(uint32_t) {}

// verification du SDK en mode DEBUG : les macros VERIF_* ecrivent dans *__gxVerif ; on lui donne une zone
static uint64_t g_verif_area[64 * 1024 / 8];
void* __gxVerif = g_verif_area;
void* GXSetVerifyCallback(void*) { return 0; }
void GXSetVerifyLevel(int) {}
void __GXShadowDispList(void*, uint32_t) {}
void __GXShadowIndexState(uint32_t, uint32_t) {}
void __GXVerifyState(void) {}
void __GXVerifyVATImm(uint32_t, uint32_t, uint32_t) {}

// fin d'une primitive (GXGeometry.h en portage) : le materiel ne fait rien de plus
void port_gx_end(void) {}

// cache de palettes de charPipeline (texPalette.c) : pas de cache, lecture directe
int DOCacheInitialized = 0;
uint64_t DODisplayCache[64];
void* DSGetCacheObj(void*, const char*) { return 0; }
void DSAddCacheNode(void*, const char*, void*, void*) {}
void DSReleaseCacheObj(void*, void*) {}
int DVDReadAsyncPrio(void* fi, void* addr, int length, int offset, void* cb, int prio);
int DVDReadPrio(void* fi, void* addr, int length, int offset, int prio)
{
    return DVDReadAsyncPrio(fi, addr, length, offset, 0, prio) ? length : -1;
}

}
