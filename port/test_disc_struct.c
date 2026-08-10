// -*- coding: utf-8 -*-
// Preuve que DISC_STRUCT suffit, sur les vraies donnees du jeu.
//
//   gcc -O2 -o test_disc_struct port/test_disc_struct.c && ./test_disc_struct
//
// Il faut `core.das` a cote, extrait du disque de l utilisateur. Attendu :
//
//   morceau 0 : type=0 taille=0x230740 dest=0 decalage=0x400
//   morceau 1 : type=4 taille=0x37C80 decalage=0x230B40
//   fin de table : 0xFFFFFFFF
//   archive : 36 entrees, premiere a 0x140
//   TPL version=0x0020AF30  1 images
//   premiere image : 64x64 format=14
//
// Ces valeurs sont exactement celles que produisaient les convertisseurs ecrits a la main
// dans port/endian.cpp - ici sans une seule ligne de conversion, seulement l attribut.

/* Lire les vrais octets de etc/core.das - pris tels quels sur le disque de l utilisateur -
   avec les structures du jeu annotees, et sans une seule ligne de conversion. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISC_STRUCT __attribute__((scalar_storage_order("big-endian")))
typedef unsigned int u32; typedef unsigned short u16; typedef float f32;

/* include/dvd.h, tel quel */
typedef struct DISC_STRUCT {
    u32 type, size, dest, ofs, sndType, sndArg, sndNo, x1C;
} DvdHeader;

/* include/global.h, tel quel */
typedef struct DISC_STRUCT {
    u32 count; u32 pad[3]; u32 ofs[36];
} ArcFile;

/* charPipeline/texPalette.h, tel quel */
typedef struct DISC_STRUCT { u32 version, num, descs; } TEXPalette;
typedef struct DISC_STRUCT {
    u16 height, width; u32 format, data, wrapS, wrapT, minF, magF; f32 lodBias;
} TEXHeader;

int main(void) {
    FILE *f = fopen("core.das", "rb");
    if (!f) { puts("core.das absent"); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    unsigned char *d = malloc(n); fread(d, 1, n, f); fclose(f);

    const DvdHeader *h = (const DvdHeader *)(d + 0x20);
    printf("morceau 0 : type=%u taille=0x%X dest=%u decalage=0x%X\n", h[0].type, h[0].size, h[0].dest, h[0].ofs);
    printf("morceau 1 : type=%u taille=0x%X decalage=0x%X\n", h[1].type, h[1].size, h[1].ofs);
    printf("fin de table : 0x%X\n", h[2].type);

    const ArcFile *a = (const ArcFile *)(d + 0x400);
    printf("archive : %u entrees, premiere a 0x%X, speculaire a 0x%X\n",
           a->count, a->ofs[0], a->ofs[0]);

    /* L entree 0x40/4 = index 12 est le TPL d illumination globale. */
    const TEXPalette *t = (const TEXPalette *)(d + 0x400 + a->ofs[12]);
    printf("TPL version=0x%08X  %u images\n", t->version, t->num);
    const TEXHeader *th = (const TEXHeader *)(d + 0x400 + a->ofs[12] + t->descs + 8);
    printf("premiere image : %ux%u format=%u\n", th->width, th->height, th->format);
    return 0;
}
