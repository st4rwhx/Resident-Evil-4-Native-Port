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

// Detecteur de double conversion (RE4_TRACE_ENDIAN=1) : retourner deux fois les memes octets les
// remet a l'envers, et l'erreur n'apparait que bien plus tard. Chaque conversion marque ses octets ;
// en retrouver un deja marque est signale avec son decalage.
bool g_trace_on = false;

struct Track {
    u8 *base = nullptr;
    u32 size = 0;
    u8 *mark = nullptr;
    int reported = 0;
    const char *what = "";
    const char *labels[64] = { nullptr };
    int nlabels = 1;
    int cur = 0;
} g_trk;

// Nomme la conversion en cours : une double conversion dit alors qui avait deja touche ces octets.
inline void track_label(const char *l)
{
    if (!g_trk.base)
        return;
    for (int i = 1; i < g_trk.nlabels; ++i)
        if (g_trk.labels[i] == l) { g_trk.cur = i; return; }
    if (g_trk.nlabels < 63) {
        g_trk.labels[g_trk.nlabels] = l;
        g_trk.cur = g_trk.nlabels++;
    }
}


inline void track_hit(void *p, u32 n)
{
    if (!g_trk.base)
        return;
    u8 *b = (u8 *) p;
    if (b < g_trk.base || b + n > g_trk.base + g_trk.size)
        return;
    const u32 o = (u32) (b - g_trk.base);
    int prev = 0;
    for (u32 i = 0; i < n; ++i) {
        if (g_trk.mark[o + i])
            prev = g_trk.mark[o + i];
        g_trk.mark[o + i] = (u8) g_trk.cur;
    }
    if (prev && g_trace_on && g_trk.reported < 12) {
        ++g_trk.reported;
        std::printf("[ENDIAN] DOUBLE conversion de %u octets a +0x%x (%s) : deja touche par '%s', reconverti par '%s'\n",
                    (unsigned) n, (unsigned) o, g_trk.what, g_trk.labels[prev] ? g_trk.labels[prev] : "?",
                    g_trk.labels[g_trk.cur] ? g_trk.labels[g_trk.cur] : "?");
    }
}

void track_begin(void *base, u32 size, const char *what)
{
    g_trace_on = std::getenv("RE4_TRACE_ENDIAN") != nullptr;
    g_trk.base = (u8 *) base;
    g_trk.size = size;
    std::free(g_trk.mark);
    g_trk.mark = (u8 *) std::calloc(size ? size : 1, 1);
    g_trk.reported = 0;
    g_trk.what = what;
}

inline void swap_u16(void *p) { track_hit(p, 2); *(u16 *) p = bswap16(*(u16 *) p); }
inline void swap_u32(void *p) { track_hit(p, 4); *(u32 *) p = bswap32(*(u32 *) p); }

// Deja retourne ? (blocs que plusieurs tables se partagent, parfois en se chevauchant.)
inline bool was_swapped(void *p, u32 n)
{
    if (!g_trk.base)
        return false;
    u8 *b = (u8 *) p;
    if (b < g_trk.base || b + n > g_trk.base + g_trk.size)
        return false;
    for (u32 i = 0; i < n; ++i)
        if (g_trk.mark[(u32) (b - g_trk.base) + i])
            return true;
    return false;
}
inline void swap_u16_once(void *p) { if (!was_swapped(p, 2)) swap_u16(p); }
inline void swap_u32_once(void *p) { if (!was_swapped(p, 4)) swap_u32(p); }

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
    track_label("TPL");
    if (!s.has(at, 12))
        return false;
    u8 *pal = s.base + at;
    if (read_le32(pal) == 0x0020AF30u)
        return true;    // deja retourne (le meme TPL est parfois liste deux fois)
    if (read_be32(pal) != 0x0020AF30u)
        return false;   // pas un TPL : on ne touche a rien

    const u32 ndesc = read_be32(pal + 4);
    const u32 desc_ofs = read_be32(pal + 8);
    if (ndesc == 0) {   // TPL vide (marqueur)
        swap_u32(pal);
        swap_u32(pal + 4);
        swap_u32(pal + 8);
        return true;
    }
    if (ndesc > 4096 || !s.has(at + desc_ofs, ndesc * 8))
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
bool swap_eff_data(const Span &s, u32 at);
bool conv_lit(const Span &s, u32 at, u32 end);
bool swap_vib_tbl(const Span &s, u32 at);

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
bool swap_arc(const Span &s, bool core = false)
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
        // core.das : les entrees 1 (0x14) et 16 (0x50) sont des donnees d'effets (EspDataLoad),
        // dont la version vaut 0xB comme celle d'un paquet de textures "id" - la signature ne
        // suffit pas, il faut les nommer.
        if (core && i == 7) {                   // ofs_2C : lumieres du decor commun (cLit)
            if (!conv_lit(s, o, s.size))
                std::printf("[PORT] endian : lumieres communes a +0x%x inattendues\n", (unsigned) o);
            continue;
        }
        if (core && i == 3) {                   // ofs_1C : vibrations
            if (!swap_vib_tbl(s, o))
                std::printf("[PORT] endian : table de vibrations inattendue\n");
            continue;
        }
        if (core && (i == 6 || i == 17))        // ofs_28 / ofs_54 : tables de messages, convesties ensuite
            continue;
        if (core && (i == 1 || i == 16)) {
            if (!swap_eff_data(s, o))
                std::printf("[PORT] endian : donnees d'effets a +0x%x inattendues\n", (unsigned) o);
            continue;
        }
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
    track_label("hermite");
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
    track_label("funcpath");
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
    track_label("id_data");
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

// Table des vibrations (VibDataTbl, include/pad.h) : u32 nombre, u32 decalages ; chaque VibData =
// u32 nombre puis des VibDataEntry de 8 octets (3 x u16, puis deux octets de niveau - sans
// ordre). Lue par VibSetData des l'ecran titre : non convertie, elle envoie le jeu hors memoire.
bool swap_vib_tbl(const Span &s, u32 at)
{
    track_label("vibrations");
    if (!s.has(at, 4))
        return false;
    const u32 n = read_be32(s.base + at);
    if (n > 0x100 || !s.has(at + 4, n * 4))
        return false;
    swap_u32(s.base + at);
    u32 done[0x100];
    u32 ndone = 0;
    for (u32 i = 0; i < n; ++i) {
        u8 *e = s.base + at + 4 + i * 4;
        const u32 o = read_be32(e);
        swap_u32(e);
        if (!o || !s.has(at + o, 4))
            continue;
        bool seen = false;                          // plusieurs entrees partagent parfois le meme bloc
        for (u32 k = 0; k < ndone; ++k)
            if (done[k] == o)
                seen = true;
        if (seen)
            continue;
        done[ndone++] = o;
        const u32 m = read_be32(s.base + at + o);
        if (m > 0x40 || !s.has(at + o + 4, m * 8))
            continue;
        swap_u32_once(s.base + at + o);
        for (u32 k = 0; k < m; ++k) {
            u8 *v = s.base + at + o + 4 + k * 8;
            swap_u16_once(v);
            swap_u16_once(v + 2);
            swap_u16_once(v + 4);
        }
    }
    return true;
}

// debug/roomInfo.dat (cRoomJmp, src/game/room_jmp.cpp) : u32 nombre d'etages, u32 decalages par
// etage ; chaque etage = u32 nombre de pieces puis des CRoomInfo de 0x20 octets
// (u16 drapeaux, u16 salle, 4 x f32 position+angle, 3 decalages de chaine). Les chaines restent
// des octets ; leurs decalages sont des mots a retourner.
bool swap_room_info(const Span &s)
{
    if (!s.has(0, 4))
        return false;
    const u32 n = read_be32(s.base);
    if (n == 0 || n > 0x40 || !s.has(4, n * 4))
        return false;
    for (u32 st = 0; st < n; ++st) {
        const u32 o = read_be32(s.base + 4 + st * 4);
        if (!o)
            continue;
        if (!s.has(o, 4))
            return false;
        const u32 cnt = read_be32(s.base + o);
        if (cnt > 0x100 || !s.has(o + 4, cnt * 0x20))
            return false;
    }
    swap_u32(s.base);
    for (u32 st = 0; st < n; ++st) {
        u8 *po = s.base + 4 + st * 4;
        const u32 o = read_be32(po);
        swap_u32(po);
        if (!o)
            continue;
        const u32 cnt = read_be32(s.base + o);
        swap_u32(s.base + o);
        for (u32 i = 0; i < cnt; ++i) {
            u8 *e = s.base + o + 4 + i * 0x20;
            swap_u16(e);
            swap_u16(e + 2);
            for (int k = 4; k < 0x20; k += 4)
                swap_u32(e + k);
        }
    }
    return true;
}

bool swap_id_tex(const Span &s, u32 at)
{
    track_label("id_tex");
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
// Fin du morceau d'archive qui commence a `at` : le plus petit decalage superieur, sinon la
// fin du fichier. La table des decalages est deja retournee quand on arrive ici.
u32 arc_chunk_end(const Span &s, u32 at)
{
    u32 end = s.size;
    if (!s.has(0, 0x10))
        return end;
    const u32 n = read_le32(s.base);
    if (n == 0 || n > 0x400 || !s.has(0x10, n * 4))
        return end;
    for (u32 i = 0; i < n; ++i) {
        const u32 o = read_le32(s.base + 0x10 + i * 4);
        if (o > at && o < end)
            end = o;
    }
    return end;
}

// `end` borne la table. Sans lui, un decalage aberrant envoie le parcours de texte n'importe ou
// dans le fichier, et il y retourne jusqu'a huit kilo-octets d'autre chose : c'est exactement ce
// qui ecrasait le TPL speculaire de core.das.
bool mes_trace(void)
{
    static const bool t = std::getenv("RE4_TRACE_ENDIAN") != 0;
    return t;
}

bool swap_mes_tbl(const Span &s, u32 at, u32 end)
{
    track_label("mes_tbl");
    if (end > s.size)
        end = s.size;
    if (at + 8 > end)
        return false;
    const u32 first = read_be32(s.base + at + 4);
    if (first < 8 || (first & 3) || at + first + 8 > end)
        return false;
    const u32 nlang = first / 4 - 1;
    if (nlang == 0 || nlang > 16)
        return false;

    // Verification avant toute ecriture : un seul decalage aberrant et on ne touche a rien.
    for (u32 k = 0; k < nlang; ++k) {
        const u32 b = read_be32(s.base + at + 4 + k * 4);
        if (b < first || at + b + 8 > end)
            return false;
    }

    // Chaque langue occupe une tranche contigue de la table : ses textes s'arretent au plus tard
    // la ou commence la langue suivante. Sans cette borne, un texte sans terminateur deborde sur
    // l'en-tete du bloc voisin et le retourne - c'est le `count=2d` devenu `count=2d00` qu'on
    // lisait sur memcard.das.
    u32 bofs[16];
    for (u32 k = 0; k < nlang; ++k)
        bofs[k] = read_be32(s.base + at + 4 + k * 4);

    u32 done[4096];
    u32 ndone = 0;

    swap_u32(s.base + at);
    for (u32 k = 0; k < nlang; ++k) {
        u8 *pb = s.base + at + 4 + k * 4;
        const u32 b = bofs[k];
        swap_u32(pb);

        u32 blk_end = end;
        for (u32 j = 0; j < nlang; ++j) {
            const u32 o = at + bofs[j];
            if (o > at + b && o < blk_end)
                blk_end = o;
        }

        u8 *blk = s.base + at + b;
        const u32 count = read_be32(blk + 4);
        if (count > 0x4000 || at + b + 8 + count * 4 > blk_end)
            continue;
        swap_u32(blk);
        swap_u32(blk + 4);

        for (u32 i = 0; i < count; ++i) {
            u8 *pe = blk + 8 + i * 4;
            const u32 mo = read_be32(pe);
            swap_u32(pe);
            const u32 abs = at + b + mo;
            // Un texte est un tableau de u16 dans le bloc de sa langue : adresse paire, apres le
            // tableau de decalages, et avant le bloc suivant.
            if (!mo || (abs & 1) || abs < at + b + 8 + count * 4 || abs + 2 > blk_end)
                continue;

            bool seen = false;
            for (u32 j = 0; j < ndone; ++j) {
                if (done[j] == abs) { seen = true; break; }
            }
            if (seen)
                continue;
            if (ndone < 4096)
                done[ndone++] = abs;

            for (u32 w = 0;; ++w) {
                u8 *c = s.base + abs + w * 2;
                if (abs + w * 2 + 2 > blk_end)
                    break;
                const u16 v = (u16) ((c[0] << 8) | c[1]);
                swap_u16_once(c);
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
extern "C" const void *port_rel_prepare(void *hdr);
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
        track_begin(data, size, "core.das");
        const bool ok = swap_arc(s, true);
        if (trace && s.has(0x17dde0, 32)) {
            const u32 *q = (const u32 *) (s.base + 0x17dde0);
            std::printf("[ENDIAN] apres swap_arc, ofs_48 : %08x %08x %08x %08x %08x %08x\n",
                        q[0], q[1], q[2], q[3], q[4], q[5]);
        }
        if (ok) {
            const u32 mes012 = read_le32(s.base + 0x28);   // ArcFile::ofs_28, deja retourne
            const u32 mes3 = read_le32(s.base + 0x54);     // ArcFile::ofs_54
            if (mes012 && !swap_mes_tbl(s, mes012, arc_chunk_end(s, mes012)))
                std::printf("[PORT] endian : table de messages 0x28 inattendue\n");
            if (mes3 && mes3 != mes012 && !swap_mes_tbl(s, mes3, arc_chunk_end(s, mes3)))
                std::printf("[PORT] endian : table de messages 0x54 inattendue\n");
        }
        return;
    }
    // etc/memcard.das : l archive de l ecran de carte memoire. Son entree 1 est une table de
    // messages (src/game/card.cpp, cCard::initSub), sans plus de signature que celles de
    // core.das - donc nommee elle aussi.
    if (ends_with(name, "memcard.das")) {
        if (swap_arc(s)) {
            const u32 mes = read_le32(s.base + 0x14);   // CardArc::ofs[1]
            if (mes && !swap_mes_tbl(s, mes, arc_chunk_end(s, mes)))
                std::printf("[PORT] endian : table de messages de memcard.das inattendue\n");
        }
        return;
    }
    // Modules REL : l'en-tete donne l'identifiant, le code est deja dans le binaire (port/rel.cpp).
    if (ends_with(name, ".rel")) {
        port_rel_prepare(s.base);
        return;
    }
    // .drs : archive de donnees suivie d'un module REL ; le mot 1 donne le decalage du module.
    if (ends_with(name, ".drs")) {
        if (s.has(4, 4)) {
            const u32 ofs = read_be32(s.base + 4);
            swap_u32(s.base + 4);
            if (ofs && s.has(ofs, 0x40))
                port_rel_prepare(s.base + ofs);
        }
        return;
    }
    if (ends_with(name, "roominfo.dat")) {
        if (!swap_room_info(s))
            std::printf("[PORT] endian : %s n a pas la forme attendue\n", name);
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


// ---------------------------------------------------------------- archives de salle
//
// Une salle (`stN/rXXX.das`) est un paquet compresse (yz2) qui, une fois decompresse, ressemble a
// `DataExtHeader` (src/game/read.cpp) : un compte, trois mots de remplissage, `num` decalages, puis
// `num` etiquettes de quatre octets ("RTP", "MDT", "EMI"...). Chaque etiquette a son propre format
// interne : on retourne l'en-tete ici, puis chaque morceau par son convertisseur.
//
// Le **journal de couverture** (`RE4_TRACE_ENDIAN=1`) dit pour chaque etiquette demandee par le jeu
// si elle a un convertisseur : un morceau lu sans convertisseur se paie en plantages tres loin de
// la cause, donc il se lit ici d'abord.
namespace {

struct TagConv {
    const char *tag;
    bool (*fn)(const Span &s, u32 at, u32 end);
};
// ---- TPL (tag TPL) : textures de la salle
bool conv_tpl(const Span &s, u32 at, u32 end) { (void) end; return swap_tpl(s, at); }

// ---- BIN : un modele (ModelData, src/game/model.h)
//
// L'en-tete, les tableaux de sommets/normales/coordonnees de texture, les poids de peau, les
// enregistrements de parties et l'en-tete de chaque morceau d'instructions sont lus par le code
// du jeu : on les retourne. Les listes d'affichage elles-memes (le flux GX apres chaque entete de
// morceau) restent telles quelles - c'est la couche GX qui les lira gros-boutistes.
bool conv_model_bin(const Span &s, u32 at, u32 end)
{
    track_label("BIN");
    if (!s.has(at, 0x40))
        return false;
    u8 *h = s.base + at;
    const u32 version = read_be32(h + 0x3C);
    {
        const u32 v = read_le32(h + 0x3C);
        if (v == 0x20010801u || v == 0x20030817u || v == 0x20030818u)
            return true;    // deja retourne (modele partage par plusieurs travaux)
    }
    if (version != 0x20010801u && version != 0x20030817u && version != 0x20030818u) {
        std::printf("[PORT] endian : BIN a +0x%x : version inattendue %08x\n", (unsigned) at, (unsigned) version);
        return false;
    }
    if (end > s.size)
        end = s.size;
    const u32 pHead = read_be32(h + 0x00), pClr = read_be32(h + 0x0C), pTex = read_be32(h + 0x10);
    const u32 pWeight = read_be32(h + 0x14), pParts = read_be32(h + 0x1C), vtxOrig = read_be32(h + 0x30);
    const u32 nrmOrig = read_be32(h + 0x34);
    const bool ext = version > 0x20030817u && s.has(at, 0x48);
    const u32 blendTbl = ext ? read_be32(h + 0x40) : 0, flipTbl = ext ? read_be32(h + 0x44) : 0;
    const u32 flags = read_be32(h + 0x20);
    const u32 nParts = (u32) ((h[0x1A] << 8) | h[0x1B]);
    const u32 x2A = (u32) ((h[0x2A] << 8) | h[0x2B]);
    const u32 nVtx = (u32) ((h[0x38] << 8) | h[0x39]);
    const u32 nNrm = (u32) ((h[0x3A] << 8) | h[0x3B]);
    const u32 size_all = end - at;

    // Bornes de chaque region : la suivante dans le fichier.
    const u32 offs[10] = { pHead, pClr, pTex, pWeight, pParts, vtxOrig, nrmOrig, blendTbl, flipTbl, size_all };
    auto region_end = [&](u32 o) {
        u32 e = size_all;
        for (u32 k = 0; k < 10; ++k)
            if (offs[k] > o && offs[k] < e)
                e = offs[k];
        return e;
    };
    auto ok = [&](u32 o, u32 n) { return o >= 0x40 && (u64) o + n <= size_all; };

    // Verifications d'abord : on ne touche a rien si une region deborde.
    // Les enregistrements d'articulations ne sont pas comptes par `nParts` (qui compte les morceaux
    // d'affichage) : leur nombre est ce qui tient entre le tableau et la region suivante.
    const u32 nHead = pHead >= 0x40 ? (region_end(pHead) - pHead) / 16 : 0;
    if (!ok(pHead, nHead * 16) || !ok(vtxOrig, nVtx * 8) || !ok(pParts, 0x20)) {
        std::printf("[PORT] endian : BIN a +0x%x : regions incoherentes (taille %u, nParts %u pHead %x, nVtx %u vtx %x, pParts %x, nNrm %u nrm %x, flags %08x)\n",
                    (unsigned) at, (unsigned) size_all, (unsigned) nParts, (unsigned) pHead, (unsigned) nVtx, (unsigned) vtxOrig,
                    (unsigned) pParts, (unsigned) nNrm, (unsigned) nrmOrig, (unsigned) flags);
        return false;
    }
    const bool nrm8 = (flags & 0x20000000u) != 0;
    if (!ok(nrmOrig, nNrm * (nrm8 ? 4u : 8u))) {
        std::printf("[PORT] endian : BIN a +0x%x : normales hors du fichier\n", (unsigned) at);
        return false;
    }

    if (g_trace_on)
        std::printf("[ARC] BIN +0x%x (%u octets) : head %x clr %x tex %x wgt %x parts %x vtx %x nrm %x blend %x flip %x nParts %u nVtx %u nNrm %u x2A %u flags %08x\n",
                    (unsigned) at, (unsigned) size_all, (unsigned) pHead, (unsigned) pClr, (unsigned) pTex, (unsigned) pWeight,
                    (unsigned) pParts, (unsigned) vtxOrig, (unsigned) nrmOrig, (unsigned) blendTbl, (unsigned) flipTbl,
                    (unsigned) nParts, (unsigned) nVtx, (unsigned) nNrm, (unsigned) x2A, (unsigned) flags);
    // en-tete
    static const u32 words[] = { 0x00u, 0x04u, 0x08u, 0x0Cu, 0x10u, 0x14u, 0x1Cu, 0x20u, 0x24u, 0x2Cu, 0x30u, 0x34u, 0x3Cu };
    for (u32 w : words)
        swap_u32(h + w);
    if (ext) {
        swap_u32(h + 0x40);
        swap_u32(h + 0x44);
    }
    swap_u16(h + 0x1A);
    swap_u16(h + 0x2A);
    swap_u16(h + 0x38);
    swap_u16(h + 0x3A);

    // enregistrements de parties : 4 octets d'indices puis la position
    for (u32 i = 0; i < nHead; ++i) {
        u8 *r = h + pHead + i * 16;
        for (int k = 1; k < 4; ++k)
            swap_u32(r + 4 * k);
    }
    // positions des sommets : s16 x, y, z puis l'indice de matrice
    for (u32 i = 0; i < nVtx * 4; ++i)
        swap_u16(h + vtxOrig + i * 2);
    // normales : s8 x4, ou s16 x4
    if (!nrm8)
        for (u32 i = 0; i < nNrm * 4; ++i)
            swap_u16(h + nrmOrig + i * 2);
    // coordonnees de texture : f32 par paires, ou s16 par paires
    if (pTex >= 0x40) {
        const u32 te = region_end(pTex);
        if (te > pTex) {
            if (flags & 0x80000000u) {
                for (u32 o = pTex; o + 2 <= te; o += 2)
                    swap_u16(h + o);
            } else {
                for (u32 o = pTex; o + 4 <= te; o += 4)
                    swap_u32(h + o);
            }
        }
    }
    // poids de peau etendus (les petits sont des octets : rien a retourner)
    if (pWeight >= 0x40 && x2A > 0xFF) {
        for (u32 i = 0; i < x2A && pWeight + (i + 1) * 12 <= size_all; ++i) {
            u8 *w = h + pWeight + i * 12;
            for (int k = 0; k < 4; ++k)
                swap_u16(w + 2 * k);
        }
    }
    // tables de fusion de mouvement (version 0x20030818)
    if (version > 0x20030817u) {
        if (blendTbl >= 0x40) {
            const u32 be = region_end(blendTbl);
            for (u32 o = blendTbl; o + 2 <= be; o += 2)
                swap_u16(h + o);
        }
        if (flipTbl >= 0x40) {
            const u32 fe = region_end(flipTbl);
            if (flipTbl + 4 <= fe) {
                swap_u32(h + flipTbl);
                for (u32 o = flipTbl + 4; o + 2 <= fe; o += 2)
                    swap_u16(h + o);
            }
        }
    }
    // morceaux : 0x20 octets d'en-tete puis `size` octets de liste d'affichage
    u32 po = pParts;
    for (u32 i = 0; i < nParts; ++i) {
        if (!ok(po, 0x20))
            break;
        u8 *p = h + po;
        const u32 sz = read_be32(p + 0x18);
        swap_u32(p + 0x18);
        swap_u32(p + 0x1C);
        if (!ok(po + 0x20, sz))
            break;
        po += 0x20 + sz;
    }
    return true;
}

// ---- Donnees d'effets (src/game/eff_sys.cpp, EffData version 0xB)
//
// Un en-tete de douze mots, puis des tables : identifiants de textures, TPL, animations, listes
// et donnees d'effets (EST/SST), chemins, modeles d'effets (BIN + TPL + mouvements). Ici : tout ce
// que l'enregistrement lit tout de suite. Les sequences d'effets elles-memes (EspSeqData) ne sont
// lues qu'au declenchement d'un effet : elles attendent leur convertisseur, signale a l'execution.
bool conv_model_bin(const Span &s, u32 at, u32 end);

bool swap_eff_data(const Span &s, u32 at)
{
    track_label("EFF");
    if (!s.has(at, 0x30))
        return false;
    u8 *h = s.base + at;
    if (read_be32(h) != 0xBu)
        return false;
    u32 f[12];
    for (int k = 0; k < 12; ++k)
        f[k] = read_be32(h + 4 * k);
    for (int k = 0; k < 12; ++k)
        swap_u32(h + 4 * k);
    auto id_tbl = [&](u32 ofs) -> u32 {
        track_label("eff:id_tbl");
        if (!s.has(at + ofs, 4))
            return 0;
        const u32 n = read_be32(s.base + at + ofs);
        if (n > 0x400 || !s.has(at + ofs + 4, n * 8))
            return 0;
        swap_u32(s.base + at + ofs);
        for (u32 i = 0; i < n; ++i) {
            u8 *e = s.base + at + ofs + 4 + i * 8;
            swap_u16(e);
            swap_u16(e + 2);
            swap_u32(e + 4);
        }
        return n;
    };
    // `limit` : le jeu ne parcourt que autant d'entrees que la table d'identifiants en annonce
    // (EspDataLoad boucle sur `ids->num`) ; la table de decalages peut en porter davantage, et
    // les suivre mene au milieu d'autres donnees.
    auto ofs_tbl = [&](u32 ofs, int kind, u32 limit) {
        track_label("eff:ofs_tbl");
        if (!s.has(at + ofs, 4))
            return;
        u32 n = read_be32(s.base + at + ofs);
        if (n > 0x400 || !s.has(at + ofs + 4, n * 4))
            return;
        swap_u32(s.base + at + ofs);
        if (n > limit)
            n = limit;
        for (u32 i = 0; i < n; ++i) {
            u8 *e = s.base + at + ofs + 4 + i * 4;
            const u32 o = read_be32(e);
            swap_u32(e);
            if (!o)
                continue;
            const u32 t = at + ofs + o;
            if (kind == 0)
                swap_tpl(s, t);
            else if (kind == 1)
                swap_tex_anm(s, t);
            else if (kind == 2) {                       // EffEfmEnt : cinq mots, puis modele / TPL / mouvements
                if (!s.has(t, 20))
                    continue;
                const u32 eo[5] = { read_be32(s.base + t), read_be32(s.base + t + 4), read_be32(s.base + t + 8),
                                    read_be32(s.base + t + 12), read_be32(s.base + t + 16) };
                for (int k = 0; k < 5; ++k)
                    swap_u32(s.base + t + 4 * k);
                if (std::getenv("RE4_TRACE_ENDIAN"))
                    std::printf("[ARC] EFM %u a +0x%x : x0=%x modele +%x tpl +%x mot +%x x +%x\n", (unsigned) i, (unsigned) t, (unsigned) eo[0], (unsigned) eo[1], (unsigned) eo[2], (unsigned) eo[3], (unsigned) eo[4]);
                if (eo[1]) {
                    u32 mend = s.size;                  // le modele finit ou commence le TPL, les mouvements ou l'entree suivante
                    const u32 cand[3] = { eo[2], eo[3], eo[4] };
                    for (int k = 0; k < 3; ++k)
                        if (cand[k] > eo[1] && t + cand[k] < mend)
                            mend = t + cand[k];
                    conv_model_bin(s, t + eo[1], mend);
                }
                if (eo[2])
                    swap_tpl(s, t + eo[2]);
                if (eo[3] && s.has(t + eo[3], 4)) {     // table de mouvements : num + decalages
                    u8 *m = s.base + t + eo[3];
                    const u32 mn = read_be32(m);
                    if (mn <= 0x100 && s.has(t + eo[3] + 4, mn * 4)) {
                        swap_u32(m);
                        for (u32 k = 0; k < mn; ++k)
                            swap_u32(m + 4 + 4 * k);
                    }
                }
            }
        }
    };
    auto list_tbl = [&](u32 ofs) -> u32 {
        track_label("eff:list");              // SstList : num, puis (no, type, x4) x num
        if (!s.has(at + ofs, 4))
            return 0;
        const u32 n = read_be32(s.base + at + ofs);
        if (n > 0x1000 || !s.has(at + ofs + 4, n * 8))
            return 0;
        swap_u32(s.base + at + ofs);
        for (u32 i = 0; i < n; ++i) {
            u8 *e = s.base + at + ofs + 4 + i * 8;
            swap_u16(e);
            swap_u16(e + 2);
            swap_u32(e + 4);
        }
        return n;
    };
    auto data_tbl = [&](u32 ofs, u32 n) {
        track_label("eff:data");              // SstData : x0, puis n decalages vers les sequences
        if (!n || !s.has(at + ofs, 4 + n * 4))
            return;
        for (u32 i = 0; i < n + 1; ++i)
            swap_u32(s.base + at + ofs + 4 * i);
    };
    const u32 ntex = id_tbl(f[1]);                     // identifiants de textures
    ofs_tbl(f[6], 0, ntex);                            // TPL
    ofs_tbl(f[7], 1, ntex);                            // animations
    const u32 ne = list_tbl(f[2]);                     // EST
    const u32 ns = list_tbl(f[3]);                     // SST
    const u32 np = list_tbl(f[4]);                     // chemins
    data_tbl(f[8], ne);
    data_tbl(f[9], ns);
    data_tbl(f[10], np);
    const u32 nefm = id_tbl(f[5]);                     // identifiants de modeles d'effets
    ofs_tbl(f[11], 2, nefm);                           // modeles d'effets
    PORT_TODO("sequences d'effets (EST/SST/chemins)");
    return true;
}

// ---- LIT : lumieres (cLit, include/light.h)
//
// u16 nombre de coupes, deux octets (version, lumieres max), puis `nCut` decalages ; chaque coupe
// est un cLightEnv (0x104 octets : brouillard, reglages d'image, vent) suivi de `nLight` cLightWork
// de 0x12C octets. Les couleurs sont des quadruplets d'octets (GXColor) : on n'y touche pas. Les
// zones propres a chaque type de lumiere (cLightWork+0x6C, 0xEC) attendent leur convertisseur.
bool conv_lit(const Span &s, u32 at, u32 end)
{
    track_label("LIT");
    (void) end;
    if (!s.has(at, 4))
        return false;
    u8 *h = s.base + at;
    const u32 nCut = (u32) ((h[0] << 8) | h[1]);
    if (nCut > 0x100 || !s.has(at + 4, nCut * 4)) {
        std::printf("[PORT] endian : LIT a +0x%x : %u coupes (octets %02x %02x %02x %02x, deja touche : %s)\n", (unsigned) at, (unsigned) nCut,
                    h[0], h[1], h[2], h[3], was_swapped(h, 2) ? g_trk.labels[g_trk.mark[at]] : "non");
        return false;
    }
    swap_u16(h);
    u32 done[0x100];
    u32 ndone = 0;
    for (u32 c = 0; c < nCut; ++c) {
        u8 *e = h + 4 + c * 4;
        const u32 o = read_be32(e);
        swap_u32(e);
        if (!o)
            continue;
        bool seen = false;
        for (u32 k = 0; k < ndone; ++k)
            if (done[k] == o)
                seen = true;
        if (seen)
            continue;
        if (ndone < 0x100)
            done[ndone++] = o;
        if (!s.has(at + o, 0x104)) {
            std::printf("[PORT] endian : LIT a +0x%x : coupe %u hors du fichier (+0x%x)\n", (unsigned) at, (unsigned) c, (unsigned) o);
            return false;
        }
        u8 *cut = h + o;
        const u32 nLight = read_be32(cut + 4);
        if (nLight > 0x100 || !s.has(at + o + 0x104, nLight * 0x12C)) {
            std::printf("[PORT] endian : LIT a +0x%x : coupe %u : %u lumieres\n", (unsigned) at, (unsigned) c, (unsigned) nLight);
            return false;
        }
        static const u32 envw[] = { 0x04u, 0x08u, 0x0Cu, 0x10u, 0x18u, 0x1Cu, 0x20u, 0x28u, 0x44u, 0xF8u };
        for (u32 w : envw)
            swap_u32(cut + w);
        for (u32 i = 0; i < nLight; ++i) {
            u8 *w = cut + 0x104 + i * 0x12C;
            static const u32 ww[] = { 0x04u, 0x08u, 0x0Cu, 0x10u, 0x18u, 0x20u, 0x28u, 0x2Cu, 0x30u, 0x34u, 0x38u,
                                      0x3Cu, 0x40u, 0x44u, 0x48u, 0x4Cu };
            for (u32 o2 : ww)
                swap_u32(w + o2);
            swap_u16(w + 0x24);
            swap_u16(w + 0x26);
        }
    }
    return true;
}

// ---- SMD : la carte de decor de la salle (src/game/scroll.cpp, include/scroll.h)
bool conv_smd(const Span &s, u32 at, u32 end)
{
    if (!s.has(at, 0x14))
        return false;
    u8 *h = s.base + at;
    const u8 flags = h[1];
    const u32 nWork = (u32) ((h[2] << 8) | h[3]);
    const u32 ofsBin = read_be32(h + 4), ofsTpl = read_be32(h + 8), ofsMot = read_be32(h + 0xC);
    swap_u16(h + 2);
    for (int k = 1; k <= 3; ++k)
        swap_u32(h + 4 * k);

    u32 total = nWork;
    u32 wofs = 0x10;
    if (flags & 1) {
        const u32 ng = read_be32(h + 0x10);
        if (ng > 0x100 || !s.has(at + 0x14, ng * 4))
            return false;
        swap_u32(h + 0x10);
        for (u32 i = 0; i < ng; ++i) {
            swap_u32(h + 0x14 + 4 * i);
        }
        wofs = 0x14 + ng * 4;
    }
    u32 maxBin = 0, maxTpl = 0, maxMot = 0;
    for (u32 i = 0; i < total; ++i) {
        const u32 o = wofs + i * 0x48;
        if (!s.has(at + o, 0x48))
            return false;
        u8 *w = h + o;
        const u8 id = w[0x27];
        const u32 f = read_be32(w + 0x44);
        for (int k = 0; k < 9; ++k)
            swap_u32(w + 4 * k);
        swap_u32(w + 0x44);
        if (id == 0xFF || (f & 0x10))
            continue;
        if (w[0x24] < 0xFE && (u32) w[0x24] + 1 > maxBin) maxBin = (u32) w[0x24] + 1;
        if (w[0x25] < 0xFE && (u32) w[0x25] + 1 > maxTpl) maxTpl = (u32) w[0x25] + 1;
        if (w[0x26] < 0xFE && (u32) w[0x26] + 1 > maxMot) maxMot = (u32) w[0x26] + 1;
    }
    // Tables d'adresses : pour la carte commune (sans travaux propres) le nombre vient du premier
    // decalage, qui pointe juste apres la table.
    auto table_n = [&](u32 ofs, u32 used) -> u32 {
        if (!ofs || !s.has(at + ofs, 4))
            return 0u;
        const u32 first = read_be32(s.base + at + ofs);
        u32 n = first / 4;
        if ((first & 3) || n == 0 || n > 0x200)
            n = used;
        return n < used ? used : n;
    };
    const u32 nb = table_n(ofsBin, maxBin), nt = table_n(ofsTpl, maxTpl), nm = table_n(ofsMot, maxMot);
    if (std::getenv("RE4_TRACE_ENDIAN"))
        std::printf("[ARC] SMD a +0x%x : %u travaux (+%u), tables bin %u@%x tpl %u@%x mot %u@%x\n", (unsigned) at, (unsigned) total, (unsigned) nWork,
                    (unsigned) nb, (unsigned) ofsBin, (unsigned) nt, (unsigned) ofsTpl, (unsigned) nm, (unsigned) ofsMot);
    bool all = true;
    // Les entrees des trois tables, triees par adresse : chacune finit ou commence la suivante.
    u32 starts[1024];
    u32 nstarts = 0;
    {
        const u32 tbs[3] = { ofsBin, ofsTpl, ofsMot };
        const u32 cnt[3] = { nb, nt, nm };
        for (int k = 0; k < 3; ++k) {
            if (!tbs[k] || !s.has(at + tbs[k], cnt[k] * 4))
                continue;
            for (u32 i = 0; i < cnt[k] && nstarts < 1024; ++i) {
                const u32 o = read_be32(s.base + at + tbs[k] + i * 4);
                if (o)
                    starts[nstarts++] = at + tbs[k] + o;
            }
        }
    }
    auto next_start = [&](u32 a) {
        u32 e = end;
        for (u32 i = 0; i < nstarts; ++i)
            if (starts[i] > a && starts[i] < e)
                e = starts[i];
        return e;
    };
    auto fix_table = [&](u32 ofs, u32 n, int kind) {
        if (!ofs || !n || !s.has(at + ofs, n * 4))
            return;
        for (u32 i = 0; i < n; ++i) {
            u8 *e = s.base + at + ofs + i * 4;
            const u32 o = read_be32(e);
            swap_u32(e);
            if (!o || !s.has(at + ofs + o, 4))
                continue;
            bool r = true;
            if (kind == 0)
                r = conv_model_bin(s, at + ofs + o, next_start(at + ofs + o));
            else if (kind == 1)
                r = swap_tpl(s, at + ofs + o);
            if (!r) {
                all = false;
                std::printf("[PORT] endian : SMD a +0x%x : entree %s %u (+0x%x) refusee\n", (unsigned) at, kind == 0 ? "BIN" : "TPL", (unsigned) i, (unsigned) (at + ofs + o));
            }
        }
    };
    fix_table(ofsBin, nb, 0);
    fix_table(ofsTpl, nt, 1);
    fix_table(ofsMot, nm, 2);   // les mouvements attendent leur convertisseur
    return all;
}

// ---- SMX : parametres par objet du decor
bool conv_smx(const Span &s, u32 at, u32 end)
{
    (void) end;
    if (!s.has(at, 0x10))
        return false;
    u8 *h = s.base + at;
    const u32 n = h[1];
    if (!s.has(at + 0x10, n * 0x90))
        return false;
    for (u32 i = 0; i < n; ++i) {
        u8 *w = h + 0x10 + i * 0x90;
        static const u32 offs[] = { 0x04u, 0x08u, 0x0Cu, 0x84u, 0x88u, 0x8Cu };
        for (u32 o : offs)
            swap_u32(w + o);
    }
    return true;
}

const TagConv kRoomTags[] = {
    { "TPL", conv_tpl },
    { "BIN", conv_model_bin },
    { "SMD", conv_smd },
    { "SMX", conv_smx },
    { "LIT", conv_lit },
    { nullptr, nullptr }
};

bool room_tag_converted(const char *tag)
{
    for (const TagConv *c = kRoomTags; c->tag; ++c)
        if (!std::strncmp(c->tag, tag, 3))
            return true;
    return false;
}

} // namespace

extern "C" void port_room_arc_fix(void *arc, u32 size)
{
    const Span s = { (u8 *) arc, size };
    track_begin(arc, size, "salle");
    if (!arc || !s.has(0, 0x10)) {
        std::printf("[PORT] endian : archive de salle vide\n");
        return;
    }
    const u32 n = read_be32(s.base);
    if (n == 0 || n > 0x400 || !s.has(0x10, n * 8)) {
        std::printf("[PORT] endian : archive de salle illisible (num=%08x)\n", (unsigned) n);
        return;
    }
    for (int k = 0; k < 4; ++k)
        swap_u32(s.base + 4 * k);
    for (u32 i = 0; i < n; ++i)
        swap_u32(s.base + 0x10 + 4 * i);

    static const bool trace = std::getenv("RE4_TRACE_ENDIAN") != 0;
    for (u32 i = 0; i < n; ++i) {
        const u8 *tg = s.base + 0x10 + n * 4 + i * 4;
        const u32 at = read_le32(s.base + 0x10 + 4 * i);
        u32 end = size;
        for (u32 j = 0; j < n; ++j) {
            const u32 o = read_le32(s.base + 0x10 + 4 * j);
            if (o > at && o < end)
                end = o;
        }
        bool done = false;
        for (const TagConv *c = kRoomTags; c->tag; ++c) {
            if (!std::strncmp(c->tag, (const char *) tg, 3)) {
                done = c->fn(s, at, end);
                break;
            }
        }
        if (trace)
            std::printf("[ARC] entree %u : %.3s a +0x%x (%u octets) %s\n", (unsigned) i,
                        (const char *) tg, (unsigned) at, (unsigned) (end - at), done ? "converti" : "NON converti");
    }
}

// Appele par GetDataExt : chaque etiquette demandee, une fois, avec son etat de conversion.
extern "C" void port_arc_access(void *arc, const char *tag, int no, void *result)
{
    static const bool trace = std::getenv("RE4_TRACE_ENDIAN") != 0;
    if (!trace)
        return;
    static char seen[64][4];
    static int nseen = 0;
    char key[4] = { tag[0], tag[1], tag[2], (char) no };
    for (int i = 0; i < nseen; ++i)
        if (!std::memcmp(seen[i], key, 4))
            return;
    if (nseen < 64)
        std::memcpy(seen[nseen++], key, 4);
    std::printf("[ARC] jeu demande %.3s #%d : %s, %s\n", tag, no,
                result ? "present" : "absent", room_tag_converted(tag) ? "converti" : "NON converti");
}
