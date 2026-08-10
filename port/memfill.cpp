// -*- coding: utf-8 -*-
// Les trois remplissages ecrits a la main en assembleur PowerPC (`src/game/memset_2.s`).
//
// L'original deroule des ecritures de mot par blocs de 16 octets quand la destination est
// alignee, et retombe sur un remplissage octet par octet sinon. C'etait une optimisation pour
// le Gekko ; ici la bibliotheque C de la plateforme fait mieux, avec les memes effets observables
// (aucune de ces fonctions ne lit ce qu'elle ecrit, et aucune ne recouvre ses operandes).
//
// Une seule subtilite est conservee : `memset_asm` **tronque** la valeur a 8 bits avant de la
// repliquer (`clrlwi r4, r4, 24`), ce que memset fait aussi. Et `mtxclr_asm` compte des matrices
// 3x4, pas des octets - se tromper la remplirait un douzieme de ce qu'il faut.
#include <cstring>

#include <dolphin.h>

extern "C" {

void memset_asm(void *dst, int c, u32 n)
{
    std::memset(dst, c, n);
}

void memclr_asm(void *dst, u32 n)
{
    std::memset(dst, 0, n);
}

void mtxclr_asm(void *dst, u32 count)
{
    std::memset(dst, 0, (size_t) count * 12 * sizeof(f32));
}

}
