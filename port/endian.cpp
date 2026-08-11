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
#include <cstdlib>
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
// Apres conversion, les memes octets se lisent dans le sens de la machine.
inline u32 read_le32(const void *p) { return *(const u32 *) p; }

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

// Les convertisseurs du systeme "id", definis plus bas : une archive les propose a chacune
// de ses entrees, et chacun se reconnait a sa propre signature.
bool swap_id_tex(const Span &s, u32 at);
bool swap_id_data(const Span &s, u32 at);

// ---------------------------------------------------------------- archive a table d offsets
//
// `etc/core.das` une fois en memoire est un `ArcFile` (include/global.h) : un nombre d entrees,
// puis autant de **decalages** que le jeu additionne a l adresse de l archive (`ARC_PTR`). Rien
// d autre - pas de magie, pas de tailles. C est ce que lit `CoreDataRead` a la ligne qui suit
// la lecture, avant meme d avoir rendu une image.
//
//   u32 nombre      (0x00)
//   u32 x3          (0x04..0x0C, nuls sur le disque)
//   u32 ofs[nombre] (0x10...)
//
// Comme aucun octet ne signe ce format, **on le valide au lieu de le croire** : un nombre
// plausible, des decalages strictement croissants et contenus dans le fichier. Une archive de
// salle, dont le premier mot est du texte, echoue a la premiere condition et repart intacte.
//
// Les sous-fichiers pointes sont pour beaucoup des TPL (speculaire, illumination globale,
// textures d interface). `swap_tpl` se reconnait lui-meme a son numero de version : on le
// propose a chaque entree, il ne touche que celles qui en sont.
bool swap_arc(const Span &s)
{
    if (!s.has(0, 0x10))
        return false;
    const u32 n = read_be32(s.base);
    if (n == 0 || n > 0x400 || !s.has(0x10, n * 4))
        return false;

    u32 prev = 0;
    for (u32 i = 0; i < n; ++i) {
        const u32 o = read_be32(s.base + 0x10 + i * 4);
        if (o == 0)
            continue;               // entree vide : le jeu ne la lit pas
        if (o <= prev || o >= s.size)
            return false;
        prev = o;
    }

    swap_u32(s.base);
    for (u32 i = 0; i < n; ++i) {
        u8 *e = s.base + 0x10 + i * 4;
        const u32 o = read_be32(e);
        swap_u32(e);
        if (!o)
            continue;
        // Chaque convertisseur se reconnait a sa propre signature - le numero de version d un
        // TPL, le 0xB d un paquet de textures, les caracteres "2.00" d une table d unites. On
        // les propose dans l ordre, aucun ne touche ce qui n est pas a lui.
        if (swap_tpl(s, o))
            continue;
        if (swap_id_tex(s, o))
            continue;
        swap_id_data(s, o);
    }
    return true;
}

// ---------------------------------------------------------------- courbe de Hermite
//
// `Hermite1` : un compte, puis autant de cles de quatre flottants (temps, valeur, tangente
// sortante, tangente entrante). Tout y est en mots de quatre octets, sans exception.
bool swap_hermite(const Span &s, u32 at)
{
    if (!s.has(at, 4))
        return false;
    const u32 n = read_be32(s.base + at);
    if (n == 0 || n > 0x4000 || !s.has(at + 4, n * 16))
        return false;
    swap_u32(s.base + at);
    for (u32 i = 0; i < n * 4; ++i)
        swap_u32(s.base + at + 4 + i * 4);
    return true;
}

// ---------------------------------------------------------------- chemin B-spline
//
// `FuncPathData` : l ordre de la spline et le nombre de points de controle sont des **octets**
// (offsets 0x00 et 0x07) - ils traversent l endianness sans y toucher. Seuls les points, trois
// flottants chacun, doivent etre retournes. Le second pointeur du couple (`path1`) est un
// tampon de sortie que `FuncPathParametrize` remplit : il ne contient rien a convertir.
bool swap_funcpath(const Span &s, u32 at)
{
    if (!s.has(at, 8))
        return false;
    const u8 n = s.base[at + 7];
    if (n == 0 || !s.has(at + 8, (u32) n * 12))
        return false;
    for (u32 i = 0; i < (u32) n * 3; ++i)
        swap_u32(s.base + at + 8 + i * 4);
    return true;
}

// ---------------------------------------------------------------- table d unites "id"
//
// Le systeme "id" dessine toute l interface du jeu : logos, menus, compteurs, l ecran-titre.
// Une table commence par sa version en toutes lettres - "1.00" ou "2.00" - suivie du nombre
// d unites sur un octet. C est une signature franche : aucune autre donnee du jeu ne commence
// par ces cinq caracteres.
//
// Dans une unite, la majorite des champs sont des octets (identifiants, types de fusion, axes)
// et ne bougent pas. Ce qui doit etre retourne : la position, les quatre sommets, les deux
// tailles, la rotation, et les six decalages de fin - deux chemins et quatre courbes - que le
// jeu transforme en pointeurs. Les deux versions ne different que par la presence d une
// seconde couleur, qui decale tout ce qui suit de quatre octets.
bool swap_id_data(const Span &s, u32 at)
{
    if (!s.has(at, 8))
        return false;
    const u8 *v = s.base + at;
    const bool v1 = (v[0] == '1' && v[1] == '.' && v[2] == '0' && v[3] == '0' && v[4] == 0);
    const bool v2 = (v[0] == '2' && v[1] == '.' && v[2] == '0' && v[3] == '0' && v[4] == 0);
    if (!v1 && !v2)
        return false;

    const u32 num = v[5];
    const u32 entry = v1 ? 0x88u : 0x8Cu;      // include/id_sys.h : IdData / IdData2
    const u32 rot = v1 ? 0x5Cu : 0x60u;
    const u32 ofs = v1 ? 0x70u : 0x74u;
    if (num == 0 || !s.has(at + 8, num * entry))
        return true;                            // en-tete seul : rien a convertir, mais reconnu

    for (u32 i = 0; i < num; ++i) {
        u8 *e = s.base + at + 8 + i * entry;
        for (u32 k = 0; k < 3; ++k)             // pos
            swap_u32(e + 0x14 + k * 4);
        for (u32 k = 0; k < 12; ++k)            // vtx[4]
            swap_u32(e + 0x20 + k * 4);
        swap_u32(e + 0x50);                     // sizeX
        swap_u32(e + 0x54);                     // sizeY
        // col0 (et col1 en version 2) sont quatre octets chacun : rien a faire.
        for (u32 k = 0; k < 3; ++k)             // rot
            swap_u32(e + rot + k * 4);
        for (u32 k = 0; k < 6; ++k)
            swap_u32(e + ofs + k * 4);

        // Les six decalages pointent maintenant dans le bon sens : deux chemins, quatre
        // courbes. Ils partent du debut de la table, pas du fichier.
        const u32 path0 = *(const u32 *) (e + ofs);
        if (path0)
            swap_funcpath(s, at + path0);
        // e + ofs + 4 : `path1`, tampon de sortie, laisse tel quel.
        for (u32 k = 2; k < 6; ++k) {
            const u32 c = *(const u32 *) (e + ofs + k * 4);
            if (c)
                swap_hermite(s, at + c);
        }
    }
    return true;
}

// ---------------------------------------------------------------- paquet de textures "id"
//
// Version 0xB (`IdTexData`, id_tex.cpp) ou 3 (`TexData`, texture.cpp) : trois tables de
// decalages - les identifiants, les TPL, les animations. Les TPL sont des TPL ordinaires ;
// une animation est une suite de champs 16 bits (dimensions, nombre de motifs) suivie d une
// table d octets.
bool swap_tex_anm(const Span &s, u32 at)
{
    if (!s.has(at, 0x10))
        return false;
    for (u32 k = 0; k < 5; ++k)                 // largeur, hauteur, w, h, nombre de motifs
        swap_u16(s.base + at + k * 2);
    return true;
}

bool swap_ofs_tbl(const Span &s, u32 at, bool tpl)
{
    if (!s.has(at, 4))
        return false;
    const u32 n = read_be32(s.base + at);
    if (n > 0x1000 || !s.has(at + 4, n * 4))
        return false;
    swap_u32(s.base + at);
    for (u32 i = 0; i < n; ++i) {
        u8 *e = s.base + at + 4 + i * 4;
        const u32 o = read_be32(e);
        swap_u32(e);
        if (!o)
            continue;
        if (tpl)
            swap_tpl(s, at + o);
        else
            swap_tex_anm(s, at + o);
    }
    return true;
}

bool swap_id_tex(const Span &s, u32 at)
{
    if (!s.has(at, 0x20))
        return false;
    const u32 ver = read_be32(s.base + at);
    u32 o_id, o_tpl, o_anm;
    if (ver == 0xB) {
        o_id = 0x04;
        o_tpl = 0x18;
        o_anm = 0x1C;
    } else if (ver == 3) {
        o_id = 0x04;
        o_tpl = 0x08;
        o_anm = 0x0C;
    } else {
        return false;
    }

    const u32 a_id = read_be32(s.base + at + o_id);
    const u32 a_tpl = read_be32(s.base + at + o_tpl);
    const u32 a_anm = read_be32(s.base + at + o_anm);
    if (!s.has(at + a_id, 4) || !s.has(at + a_tpl, 4) || !s.has(at + a_anm, 4))
        return false;

    swap_u32(s.base + at);
    swap_u32(s.base + at + o_id);
    swap_u32(s.base + at + o_tpl);
    swap_u32(s.base + at + o_anm);

    // La table des identifiants : un compte, puis des entrees de huit octets dont un u16
    // d identifiant, un u16 et un u32.
    {
        const u32 base = at + a_id;
        const u32 n = read_be32(s.base + base);
        if (n <= 0x1000 && s.has(base + 4, n * 8)) {
            swap_u32(s.base + base);
            for (u32 i = 0; i < n; ++i) {
                u8 *e = s.base + base + 4 + i * 8;
                swap_u16(e);
                swap_u16(e + 2);
                swap_u32(e + 4);
            }
        }
    }
    swap_ofs_tbl(s, at + a_tpl, true);
    swap_ofs_tbl(s, at + a_anm, false);
    return true;
}


// ---------------------------------------------------------------- tables de messages
//
// Les textes du jeu sont dans `etc/core.das`, a deux entrees de l'archive : celle de l'offset
// 0x28 (types 0 a 2) et celle de 0x54 (type 3). Voir src/game/mes.cpp, MessageData::setPtr.
//
// La forme est :
//
//   u32 x0
//   u32 langue[N]        decalages, depuis le debut de la table, vers un bloc par langue
//   bloc : u32 x0 ; u32 count ; u32 ofs[count] ; puis les textes
//   texte : des u16, termines par 0xFFFF
//
// N n'est ecrit nulle part - mais le premier decalage marque la fin du tableau de decalages,
// donc N = langue[0] / 4 - 1. La table se decrit elle-meme.
//
// 0xFFFF se lit pareil dans les deux sens : on peut donc parcourir un texte avant de le
// retourner. Plusieurs langues peuvent pointer le meme texte, d'ou le releve des adresses deja
// traitees - le retourner deux fois le remettrait a l envers.
bool swap_mes_tbl(const Span &s, u32 at)
{
    if (!s.has(at, 8))
        return false;
    const u32 first = read_be32(s.base + at + 4);
    if (first < 8 || (first & 3) || !s.has(at + first, 8))
        return false;
    const u32 nlang = first / 4 - 1;
    if (nlang == 0 || nlang > 16)
        return false;

    // Verification avant toute ecriture : un seul decalage aberrant et on ne touche a rien.
    for (u32 k = 0; k < nlang; ++k) {
        const u32 b = read_be32(s.base + at + 4 + k * 4);
        if (b < first || !s.has(at + b, 8))
            return false;
    }

    u32 done[4096];
    u32 ndone = 0;

    swap_u32(s.base + at);
    for (u32 k = 0; k < nlang; ++k) {
        u8 *pb = s.base + at + 4 + k * 4;
        const u32 b = read_be32(pb);
        swap_u32(pb);

        u8 *blk = s.base + at + b;
        const u32 count = read_be32(blk + 4);
        if (count > 0x4000 || !s.has(at + b + 8, count * 4))
            continue;
        swap_u32(blk);
        swap_u32(blk + 4);

        for (u32 i = 0; i < count; ++i) {
            u8 *pe = blk + 8 + i * 4;
            const u32 mo = read_be32(pe);
            swap_u32(pe);
            const u32 abs = at + b + mo;
            if (!mo || !s.has(abs, 2))
                continue;

            bool seen = false;
            for (u32 j = 0; j < ndone; ++j) {
                if (done[j] == abs) { seen = true; break; }
            }
            if (seen)
                continue;
            if (ndone < 4096)
                done[ndone++] = abs;

            for (u32 w = 0; w < 4096; ++w) {
                u8 *c = s.base + abs + w * 2;
                if (!s.has(abs + w * 2, 2))
                    break;
                const u16 v = (u16) ((c[0] << 8) | c[1]);
                swap_u16(c);
                if (v == 0xFFFF)
                    break;
            }
        }
    }
    return true;
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

    // RE4_TRACE_ENDIAN=1 : dire ce qui passe ici. Un fichier qui n atteint jamais ce point se
    // lit ensuite a l envers, et le symptome apparait tres loin de la cause - c est la panne la
    // plus couteuse a diagnostiquer de tout le portage, donc elle merite son interrupteur.
    static const bool trace = std::getenv("RE4_TRACE_ENDIAN") != 0;
    if (trace)
        std::printf("[ENDIAN] %s  %u octets a %p\n", name, (unsigned) size, data);

    if (ends_with(name, ".fnt")) {
        if (!swap_fnt(s))
            std::printf("[PORT] endian : %s n a pas la forme attendue d une police\n", name);
        return;
    }
    // Les tables de messages n'ont aucune signature : rien ne permet de les reconnaitre parmi
    // les entrees d'une archive. Mais on sait ou elles sont - le decompile le documente - donc
    // on les nomme plutot que de les deviner. C'est la seule conversion ciblee par fichier.
    if (ends_with(name, "core.das")) {
        const bool ok = swap_arc(s);
        if (ok) {
            const u32 mes012 = read_le32(s.base + 0x28);   // ArcFile::ofs_28, deja retourne
            const u32 mes3 = read_le32(s.base + 0x54);     // ArcFile::ofs_54
            if (mes012 && !swap_mes_tbl(s, mes012))
                std::printf("[PORT] endian : table de messages 0x28 inattendue\n");
            if (mes3 && mes3 != mes012 && !swap_mes_tbl(s, mes3))
                std::printf("[PORT] endian : table de messages 0x54 inattendue\n");
        }
        return;
    }
    if (ends_with(name, ".das") || ends_with(name, ".dat")) {
        // La meme table sert a plusieurs extensions : `etc/core.das`, mais aussi les ecrans
        // `SS/<langue>/title.dat` et `option.dat` (`TitleArc`, qui indexe a partir de zero et
        // retombe donc sur les memes mots). Les archives de salle, etiquetees par tag, et les
        // tables simples comme `sizetbl.dat` echouent a la validation et repartent intactes,
        // vers le journal du reste a faire.
        if (swap_arc(s))
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

