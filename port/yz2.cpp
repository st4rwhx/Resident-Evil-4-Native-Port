// -*- coding: utf-8 -*-
// Le decodeur yz2 : decompression des archives de salle (st1/r100.das ...).
//
// L'original est de l'assembleur PowerPC ecrit a la main (`src/game/yz2asm.s`) ; ceci en est la
// transcription, instruction par instruction, vers du C++ - memes structures (celles de
// `src/game/yz2code.cpp`, qui prepare les tables), meme ordre d'operations, memes cas limites.
//
// Le principe, tel qu'il se lit dans le code :
//  * un decodeur arithmetique ("range coder") a deux modeles de frequences adaptatives :
//    `m1` (0x500 symboles) choisit le type de bloc, `m2` (0x100 symboles) donne les octets des
//    longueurs ;
//  * les symboles 0x400..0x4FF sont des octets litteraux ; 0x200..0x3FF designent une entree du
//    dictionnaire de contexte (la longueur y est deja enregistree) ; 0..0x1FF designent une
//    entree dont la longueur est lue ensuite ;
//  * le dictionnaire est indexe par l'octet precedent : 0x100 entrees de 0x200 couples
//    (adresse source, longueur), en tampon circulaire.
//
// Les registres du PowerPC qui gardaient l'etat entre deux appels de la fonction de frequence
// (total, nombre de bits, drapeau "table construite") sont ici les champs de `Model`.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <dolphin.h>

#include "port_log.h"

namespace {

struct Freq {            // Yz2Freq (src/game/yz2code.cpp)
    P32<u16> freq;       // 0x00  (N7 : P32 comme src/game/yz2code.cpp)
    u32 max;             // 0x04
    int bits;            // 0x08
    u32 range;           // 0x0C
    P32<u16> cum;        // 0x10  paires (largeur, debut)
    int n;               // 0x14
    u32 acc;             // 0x18
};

struct Dec;

struct ModelMem {        // Yz2Model
    P32<Dec> dec;        // 0x00
    Freq fd;             // 0x04
    P32<u32> table;      // 0x20  fente de 15 bits -> symbole
};

struct Dec {             // Yz2Dec
    u32 mask;            // 0x00  etendue courante
    u32 code;            // 0x04  valeur lue
    ModelMem m1;         // 0x08
    ModelMem m2;         // 0x2C
};

struct DicEnt {          // Yz2DicEnt
    u32 cnt;             // 0x00
    u32 src[0x200];      // 0x04  adresses
    u32 len[0x200];      // 0x804 longueurs
};

struct Ctx {             // Yz2Ctx
    P32<DicEnt> dic;     // 0x00
    Dec d;               // 0x04
    u32 pad_54;
    int n;
};

struct InEv {            // Yz2InEv
    P32<u8> src;
    P32<u8> heap;
    P32<u8> free_;
    u32 size0;
    u32 size1;
};

// Etat d'un modele pendant le decodage : ce que l'assembleur gardait en registres.
struct Model {
    Dec *dec;
    u16 *cum;
    u16 *freq;
    u32 max;
    int bits;
    bool table_ok;       // le registre flottant f2/f3 de l'original
    ModelMem *mem;
};

// Un symbole : lit les octets necessaires, retrouve la fente, met a jour les frequences.
u32 decode_symbol(Model &m, const u8 *&src)
{
    Dec *d = m.dec;
    u32 range = d->mask;
    u32 code = d->code;

    if (range <= 0x800000u) {                    // renormalisation
        if (range > 0x8000u) {
            code = (code << 8) | *src++;
            range <<= 8;
        } else if (range > 0x80u) {
            code = (code << 16) | ((u32) src[0] << 8) | src[1];
            src += 2;
            range <<= 16;
        } else {
            code = (code << 24) | ((u32) src[0] << 16) | ((u32) src[1] << 8) | src[2];
            src += 3;
            range <<= 24;
        }
    }
    range >>= 14;
    d->code = code;
    d->mask = range;
    u32 slot = range ? code / range : 0;

    Freq &fd = m.mem->fd;
    u32 *table = m.mem->table;
    if (!m.table_ok) {                           // table fente -> symbole
        u32 end = 0;
        for (int i = 0; i < fd.n; ++i) {
            const u32 begin = end;
            end = (u32) m.cum[i * 2 + 1] + m.cum[i * 2];
            for (u32 k = begin; (int) (end - k) > 0; ++k)
                table[k] = (u32) i;
        }
        m.table_ok = true;
    }
    const u32 sym = table[slot];

    // soustraction de la borne basse, reduction de l'etendue
    d->code = d->code - d->mask * m.cum[sym * 2 + 1];
    d->mask = (d->mask * m.cum[sym * 2]) >> 1;
    m.freq[sym] = (u16) (m.freq[sym] + 1);
    m.max += 1;

    if (m.bits <= 14) {
        if (m.max == fd.range) {                 // le total atteint la puissance de deux : on met a l'echelle
            const u32 scale = 1u << (15 - m.bits);
            u32 start = 0;
            m.table_ok = false;
            for (int i = 0; i < fd.n; ++i) {
                const u32 w = scale * m.freq[i];
                m.cum[i * 2 + 1] = (u16) start;
                start += w;
                m.cum[i * 2] = (u16) w;
            }
            m.bits += 1;
            fd.range = 1u << m.bits;
        }
    } else if (m.max > 0x7fff) {                 // trop de comptes : on les divise par deux
        m.table_ok = false;
        m.max = 0;
        u32 start = 0;
        for (int i = 0; i < fd.n; ++i) {
            u32 f = m.freq[i];
            m.cum[i * 2] = (u16) f;
            m.cum[i * 2 + 1] = (u16) start;
            start += f;
            if (f > 1) {
                f >>= 1;
                m.freq[i] = (u16) f;
            }
            m.max += f;
        }
    }
    return sym;
}

} // namespace

extern "C" {

void yz2Decode_Decode(void *ctxp, void *dstp, u32 size, void *evp)
{
    Ctx *ctx = (Ctx *) ctxp;
    InEv *ev = (InEv *) evp;
    u8 *out = (u8 *) dstp;
    u8 *const end = out + size;
    u8 *hist = out;                              // r21 : octet de contexte courant
    const u8 *src = ev->src;
    DicEnt *dic = ctx->dic;

    // N7 : `dec` designe toujours ctx->d ; ctx est sur la pile de Yz2DecodeExec, hors de la fenetre de 4 Go en 64 bits,
    // donc le champ P32 `dec` ne peut pas la porter : on prend l'adresse directement.
    Model m1 = { &ctx->d, ctx->d.m1.fd.cum, ctx->d.m1.fd.freq, ctx->d.m1.fd.max, ctx->d.m1.fd.bits, false, &ctx->d.m1 };
    Model m2 = { &ctx->d, ctx->d.m2.fd.cum, ctx->d.m2.fd.freq, ctx->d.m2.fd.max, ctx->d.m2.fd.bits, false, &ctx->d.m2 };

    while (out < end) {
        const u32 sym = decode_symbol(m1, src);
        u32 len;

        if (sym > 0x3ff) {                       // octet litteral
            *out++ = (u8) sym;
            len = 1;
        } else {
            DicEnt *e = &dic[*hist];
            const u32 idx = (sym + (e->cnt & 0xFFFFu)) & 0x1FFu;
            const u8 *from = (const u8 *) port_ptr64(e->src[idx]);   // N7 : adresse 32 bits -> fenetre

            if (sym > 0x1ff) {                   // entree connue : longueur enregistree
                len = e->len[idx];
            } else {                             // longueur explicite, en 1 a 4 octets
                u32 v = decode_symbol(m2, src);
                if (v <= 2) {
                    u32 acc;
                    if (v == 2) {
                        acc = decode_symbol(m2, src) << 8;
                    } else if (v == 1) {
                        acc = decode_symbol(m2, src) << 16;
                        acc |= decode_symbol(m2, src) << 8;
                    } else {
                        acc = decode_symbol(m2, src) << 24;
                        acc |= decode_symbol(m2, src) << 16;
                        acc |= decode_symbol(m2, src) << 8;
                    }
                    v = acc | decode_symbol(m2, src);
                }
                len = v - 1;
            }
            for (u32 i = 0; i < len; ++i)
                out[i] = from[i];
            out += len;
        }

        // enregistre dans le dictionnaire de l'octet de contexte precedent
        u8 *last = out - 1;
        if (hist < last) {
            DicEnt *e = &dic[*hist];
            const u32 k = e->cnt;
            e->src[k & 0x1FFu] = (u32) (uintptr_t) (hist + 1);
            e->len[k & 0x1FFu] = len;
            e->cnt = (k + 1) & 0x1FFu;
            hist = last;
        }
    }
}

} // extern "C"
