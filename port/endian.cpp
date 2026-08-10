// -*- coding: utf-8 -*-
// Retourner les octets des donnees du jeu, format par format.
//
// Le GameCube est gros-boutiste et ARM ne l'est pas. Le code decompile lit ses fichiers avec
// des acces ordinaires - `header->width`, `*p++` - parce que son processeur avait la meme
// convention que son disque. Ici il faut la lui rendre. Le raisonnement complet, et les deux
// voies ecartees, sont dans port/ENDIANNESS.md.
//
// **On ne peut pas retourner "en gros".** Un fichier de RE4 melange des entiers 32 bits, des
// 16 bits, des flottants, des chaines et des pixels. Retourner tout par mots de quatre
// casserait les chaines ; ne rien retourner casse tout le reste. Chaque format a donc son
// convertisseur, qui sait ou sont ses entiers.
//
// **Et surtout : les pixels ne se retournent pas.** Le decodeur de textures du portage
// (port/texture.cpp) lit deja ses valeurs 16 bits en gros-boutiste, explicitement, comme le
// materiel. Retourner les pixels ici les casserait deux fois. Ce fichier ne touche qu'aux
// **en-tetes** - ce que le code du jeu lit, lui, avec des acces natifs.
#include <cstdio>
#include <cstring>

#include <dolphin.h>

#include "port_log.h"

namespace {

inline u16 bswap16(u16 v) { return (u16) ((v >> 8) | (v << 8)); }

inline u32 bswap32(u32 v)
{
    return ((v >> 24) & 0xFFu) | ((v >> 8) & 0xFF00u) |
           ((v << 8) & 0xFF0000u) | ((v << 24) & 0xFF000000u);
}

inline void swap_u16(void *p) { *(u16 *) p = bswap16(*(u16 *) p); }
inline void swap_u32(void *p) { *(u32 *) p = bswap32(*(u32 *) p); }

inline u32 read_be32(const void *p) { return bswap32(*(const u32 *) p); }

// Bornes : un fichier tronque ou mal identifie ne doit pas nous faire ecrire ailleurs.
struct Span {
    u8 *base;
    u32 size;
    bool has(u32 off, u32 n) const { return (u64) off + n <= size; }
};

// ---------------------------------------------------------------- TPL
//
// Le format de texture de Nintendo, et de loin le plus repandu dans RE4 : polices, interface,
// decors, personnages. Sa disposition, telle que la lisent charPipeline/texPalette.h et le
// decodeur Python de reference :
//
//   TEXPalette   : u32 version (0x0020AF30), u32 nombre, u32 -> tableau de descripteurs
//   TEXDescriptor: u32 -> en-tete d'image, u32 -> en-tete de palette (0 s'il n'y en a pas)
//   TEXHeader    : u16 hauteur, u16 largeur, u32 format, u32 -> pixels, 4x u32 (habillage,
//                  filtres), f32 biais, puis 4 octets
//   CLUTHeader   : u16 nombre, 2 octets, u32 format, u32 -> couleurs
//
// Les champs "->" sont des decalages dans le fichier que le jeu convertit ensuite en adresses
// (CalcTplAddr). Ils doivent donc etre justes **avant** cette conversion.
bool swap_tpl(const Span &s, u32 at)
{
    if (!s.has(at, 12))
        return false;
    u8 *pal = s.base + at;
    if (read_be32(pal) != 0x0020AF30u)
        return false;   // pas un TPL : on ne touche a rien

    const u32 ndesc = read_be32(pal + 4);
    const u32 desc_ofs = read_be32(pal + 8);
    if (ndesc == 0 || ndesc > 4096 || !s.has(at + desc_ofs, ndesc * 8))
        return false;

    swap_u32(pal);
    swap_u32(pal + 4);
    swap_u32(pal + 8);

    for (u32 i = 0; i < ndesc; ++i) {
        u8 *d = s.base + at + desc_ofs + i * 8;
        const u32 img_ofs = read_be32(d);
        const u32 clut_ofs = read_be32(d + 4);
        swap_u32(d);
        swap_u32(d + 4);

        if (img_ofs && s.has(at + img_ofs, 0x20)) {
            u8 *h = s.base + at + img_ofs;
            swap_u16(h);        // hauteur
            swap_u16(h + 2);    // largeur
            swap_u32(h + 4);    // format
            swap_u32(h + 8);    // pixels
            swap_u32(h + 12);   // habillage S
            swap_u32(h + 16);   // habillage T
            swap_u32(h + 20);   // filtre min
            swap_u32(h + 24);   // filtre max
            swap_u32(h + 28);   // biais de LOD (f32, meme largeur)
            // Les quatre octets suivants (edgeLOD, minLOD, maxLOD, unpacked) n'ont pas
            // d'endianness.
        }
        if (clut_ofs && s.has(at + clut_ofs, 0x0C)) {
            u8 *c = s.base + at + clut_ofs;
            swap_u16(c);        // nombre d'entrees
            // c[2], c[3] : un octet de forme et un de remplissage
            swap_u32(c + 4);    // format
            swap_u32(c + 8);    // couleurs
        }
    }
    return true;
}

// ---------------------------------------------------------------- police
//
// Un `.fnt` est un TPL precede de deux decalages : la position du TPL et celle de la table des
// largeurs de caracteres (mes.cpp, MesFontFile).
bool swap_fnt(const Span &s)
{
    if (!s.has(0, 8))
        return false;
    const u32 tpl_ofs = read_be32(s.base);
    swap_u32(s.base);
    swap_u32(s.base + 4);
    return swap_tpl(s, tpl_ofs);
}

bool ends_with(const char *name, const char *suffix)
{
    const size_t n = std::strlen(name), m = std::strlen(suffix);
    if (m > n)
        return false;
    for (size_t i = 0; i < m; ++i) {
        char a = name[n - m + i], b = suffix[i];
        if (a >= 'A' && a <= 'Z')
            a = (char) (a - 'A' + 'a');
        if (a != b)
            return false;
    }
    return true;
}

} // namespace

// Le bloc d'en-tete d'une archive du jeu : un tableau de `DvdHeader` de 32 octets (huit u32),
// termine par un `type` a 0xFFFFFFFF. C'est la table des morceaux - ou ils sont dans le
// fichier, quelle taille ils font, ou ils doivent aller.
//
// C'est la structure la plus critique du chargement : une seule valeur mal lue et le jeu
// demande un transfert de deux gigaoctets. 0xFFFFFFFF etant symetrique, la fin de table se
// reconnait avant comme apres conversion.
extern "C" void port_endian_fix_headers(void *block, u32 size)
{
    if (!block)
        return;
    u8 *p = (u8 *) block;
    const u32 n = size / 32;
    for (u32 i = 0; i < n; ++i, p += 32) {
        if (*(const u32 *) p == 0xFFFFFFFFu)
            break;
        for (int k = 0; k < 8; ++k)
            swap_u32(p + 4 * k);
    }
}

// Appele par la couche DVD quand un fichier vient d'arriver entier en memoire principale.
// `name` est le chemin que le jeu a demande.
extern "C" void port_endian_fix(const char *name, void *data, u32 size)
{
    if (!name || !data || size < 8)
        return;
    const Span s = { (u8 *) data, size };

    if (ends_with(name, ".fnt")) {
        if (!swap_fnt(s))
            std::printf("[PORT] endian : %s n a pas la forme attendue d une police\n", name);
        return;
    }
    if (ends_with(name, ".tpl")) {
        if (!swap_tpl(s, 0))
            std::printf("[PORT] endian : %s n a pas la forme attendue d un TPL\n", name);
        return;
    }

    // Tout le reste attend son convertisseur. On le dit une fois par format plutot qu'a chaque
    // fichier : c'est la liste de ce qu'il reste a faire, et elle se lit a l'execution.
    const char *dot = std::strrchr(name, '.');
    if (dot)
        PORT_TODO(dot);
}

