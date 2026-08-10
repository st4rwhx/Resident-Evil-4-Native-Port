// -*- coding: utf-8 -*-
// Les lectures et ecritures **quantifiees** du Gekko, en C.
//
// Le processeur du GameCube sait charger un entier court et le convertir en flottant en une
// instruction (`psq_l`), en consultant un registre de quantification (GQR) qui porte deux
// choses : le **type** entier et un **exposant d'echelle**. Le jeu s'en sert pour garder ses
// sommets et ses normales en 8 ou 16 bits en memoire tout en calculant en flottant.
//
// Deux details valent d'etre ecrits, parce qu'ils ne se devinent pas et qu'une erreur y est
// silencieuse - un modele legerement deforme, pas un plantage :
//
//  * **l'exposant est signe sur 6 bits.** Une valeur de 50 dans le champ ne veut pas dire 2^-50
//    mais 2^+14 (50 - 64 = -14). C'est exactement ce que fait Dolphin, dont les tables de
//    (de)quantification servent ici de reference.
//  * **la conversion vers l'entier sature** aux bornes du type au lieu de rebrousser. Un
//    debordement non sature enverrait un sommet a l'autre bout de la salle.
#ifndef PORT_PPC_QUANTIZE_H
#define PORT_PPC_QUANTIZE_H

#include <dolphin/types.h>

enum {
    PORT_GQ_F32 = 0,
    PORT_GQ_U8 = 4,
    PORT_GQ_U16 = 5,
    PORT_GQ_S8 = 6,
    PORT_GQ_S16 = 7
};

// Champs du GQR, disposition du Gekko : ST_TYPE bits 0-2, ST_SCALE 8-13, LD_TYPE 16-18,
// LD_SCALE 24-29.
static inline int port_gqr_ld_type(u32 g) { return (int) ((g >> 16) & 7); }
static inline int port_gqr_st_type(u32 g) { return (int) (g & 7); }

// Exposant signe sur 6 bits.
static inline int port_gqr_ld_scale(u32 g)
{
    const int s = (int) ((g >> 24) & 0x3F);
    return (s < 32) ? s : (s - 64);
}

static inline int port_gqr_st_scale(u32 g)
{
    const int s = (int) ((g >> 8) & 0x3F);
    return (s < 32) ? s : (s - 64);
}

static inline f32 port_pow2f(int e)
{
    f32 r = 1.0f;
    int i;
    if (e >= 0) {
        for (i = 0; i < e; ++i)
            r *= 2.0f;
    } else {
        for (i = 0; i < -e; ++i)
            r *= 0.5f;
    }
    return r;
}

// Lit une valeur quantifiee a l'adresse p et rend le flottant correspondant.
static inline f32 port_dequantize(const void *p, int type, int scale)
{
    const f32 k = port_pow2f(-scale);
    switch (type) {
    case PORT_GQ_U8:  return (f32) *(const u8 *) p * k;
    case PORT_GQ_U16: return (f32) *(const u16 *) p * k;
    case PORT_GQ_S8:  return (f32) *(const s8 *) p * k;
    case PORT_GQ_S16: return (f32) *(const s16 *) p * k;
    default:          return *(const f32 *) p;
    }
}

static inline f32 port_clampf(f32 v, f32 lo, f32 hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

// Ecrit un flottant sous forme quantifiee a l'adresse p.
static inline void port_quantize(void *p, f32 v, int type, int scale)
{
    const f32 s = v * port_pow2f(scale);
    switch (type) {
    case PORT_GQ_U8:  *(u8 *) p = (u8) port_clampf(s, 0.0f, 255.0f); break;
    case PORT_GQ_U16: *(u16 *) p = (u16) port_clampf(s, 0.0f, 65535.0f); break;
    case PORT_GQ_S8:  *(s8 *) p = (s8) port_clampf(s, -128.0f, 127.0f); break;
    case PORT_GQ_S16: *(s16 *) p = (s16) port_clampf(s, -32768.0f, 32767.0f); break;
    default:          *(f32 *) p = v; break;
    }
}

// Taille en octets d'une valeur du type donne.
static inline int port_gq_size(int type)
{
    switch (type) {
    case PORT_GQ_U8:
    case PORT_GQ_S8:  return 1;
    case PORT_GQ_U16:
    case PORT_GQ_S16: return 2;
    default:          return 4;
    }
}

#endif
