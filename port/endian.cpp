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

extern "C" int Fcc_next_axis_addr(int type, int n);   // src/game/motion.cpp

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
bool conv_cam(const Span &s, u32 at, u32 end);
bool conv_motion(const Span &s, u32 at, u32 end);
u32 conv_ofs_archive(const Span &s, const char *name);
bool conv_blob(const Span &s, u32 at, u32 end, int depth, const char **kind);

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
bool swap_arc(const Span &s, bool core = false, bool blobs = false)
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
        // Des entrees peuvent partager les memes donnees (SS/<langue>/ss_pzzl.dat, l'inventaire : 0x34e0 deux fois) :
        // decalages croissants ou egaux. Refusee, l'archive restait gros-boutiste et l'inventaire plantait (14/11).
        if (o < prev || o >= s.size)
            return false;
        prev = o;
    }

    u32 last_conv = 0;   // donnees deja converties pour une entree precedente au meme decalage
    swap_u32(s.base);
    for (u32 i = 0; i < n; ++i) {
        u8 *e = s.base + 0x10 + i * 4;
        const u32 o = read_be32(e);
        swap_u32(e);
        if (!o || o == last_conv)
            continue;
        last_conv = o;
        // Chaque convertisseur se reconnait a sa propre signature - le numero de version d un
        // TPL, le 0xB d un paquet de textures, les caracteres "2.00" d une table d unites. On
        // les propose dans l ordre, aucun ne touche ce qui n est pas a lui.
        // core.das : les entrees 1 (0x14) et 16 (0x50) sont des donnees d'effets (EspDataLoad),
        // dont la version vaut 0xB comme celle d'un paquet de textures "id" - la signature ne
        // suffit pas, il faut les nommer.
        if (core && i >= 18 && i <= 22) {       // ofs_58..ofs_68 : lumieres de l'examen d'objet (ItemExamine::init, cLit)
            // Meme format que ofs_2C ; trouvees non converties en comparant au Core.dat du port PS2 de Capcom (14/11).
            if (!conv_lit(s, o, s.size))
                std::printf("[PORT] endian : lumieres d'examen %u a +0x%x inattendues\n", (unsigned) (i - 18), (unsigned) o);
            continue;
        }
        if (core && i == 7) {                   // ofs_2C : lumieres du decor commun (cLit)
            if (!conv_lit(s, o, s.size))
                std::printf("[PORT] endian : lumieres communes a +0x%x inattendues\n", (unsigned) o);
            continue;
        }
        if (core && i == 8) {                   // ofs_30 : cameras communes
            if (!conv_cam(s, o, s.size))
                std::printf("[PORT] endian : cameras communes a +0x%x inattendues\n", (unsigned) o);
            continue;
        }
        if (core && i == 11) {                  // ofs_3C : chemins de lumiere (Light05, cLightMgr::getPathPtr)
            // LightPathHeader : u8 nombre, 3 octets, puis u32 decalage[nombre] depuis l'en-tete (0 = aucun) ; chaque
            // chemin est une suite d'octets terminee par 0xFF (rien a retourner). Non convertie : Light05() INVALID PATH DATA.
            const u32 np = s.base[o];
            bool good = s.has(o + 4, np * 4);
            for (u32 k = 0; good && k < np; ++k) {
                const u32 po = read_be32(s.base + o + 4 + k * 4);
                good = po == 0 || (po >= 4 + np * 4 && s.has(o + po, 1));
            }
            if (good)
                for (u32 k = 0; k < np; ++k)
                    swap_u32(s.base + o + 4 + k * 4);
            else
                std::printf("[PORT] endian : chemins de lumiere a +0x%x inattendus\n", (unsigned) o);
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
        if (swap_tpl(s, o)) {
            if (g_trace_on && (core || blobs)) std::printf("[ARC]   entree %u +0x%x : TPL\n", (unsigned) i, (unsigned) o);
            continue;
        }
        if (swap_id_tex(s, o)) {
            if (g_trace_on && (core || blobs)) std::printf("[ARC]   entree %u +0x%x : paquet id/tex\n", (unsigned) i, (unsigned) o);
            continue;
        }
        if (swap_id_data(s, o)) {
            if (g_trace_on && (core || blobs)) std::printf("[ARC]   entree %u +0x%x : id data\n", (unsigned) i, (unsigned) o);
            continue;
        }
        if (core || blobs) {
            // ni TPL ni paquet "id" : modele, lumieres, mouvement... reconnus par leur forme
            u32 e = s.size;
            for (u32 j = 0; j < n; ++j) {
                // les entrees deja traitees sont retournees, les suivantes encore gros-boutistes
                const u32 oj = (j > i) ? read_be32(s.base + 0x10 + j * 4) : read_le32(s.base + 0x10 + j * 4);
                if (oj > o && oj < e)
                    e = oj;
            }
            const char *kind = "?";
            const bool okb = conv_blob(s, o, e, 0, &kind);
            if (okb && g_trace_on)
                std::printf("[ARC]   entree %u +0x%x : %s\n", (unsigned) i, (unsigned) o, kind);
            if (!okb && std::getenv("RE4_TRACE_ENDIAN"))
                std::printf("[ARC] archive : entree %u +0x%x (%u octets) non reconnue (%s) : %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                            (unsigned) i, (unsigned) o, (unsigned) (e - o), kind, s.base[o], s.base[o + 1], s.base[o + 2], s.base[o + 3],
                            s.base[o + 4], s.base[o + 5], s.base[o + 6], s.base[o + 7], s.base[o + 8], s.base[o + 9], s.base[o + 10], s.base[o + 11]);
        }
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
    // Table vide (MDT des salles sans texte : 6 langues, toutes au decalage de fin) : seul l'en-tete est a retourner.
    if (first >= 8 && !(first & 3) && first / 4 - 1 == read_be32(s.base + at) && at + first <= end && at + first + 8 > end) {
        bool empty = true;
        for (u32 k = 0; k < first / 4 - 1 && empty; ++k)
            empty = read_be32(s.base + at + 4 + k * 4) == first;
        if (empty) {
            for (u32 k = 0; k < first / 4; ++k)
                swap_u32(s.base + at + k * 4);
            return true;
        }
    }
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
namespace {
bool conv_evd(const Span &s, u32 at, u32 end);
void snd_swap32n(u8 *p, u32 n);
void snd_fix_str_hed(const Span &s);
void snd_fix_words_then_halves(const Span &s);
}
extern "C" void port_room_arc_fix(void *arc, u32 size);
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

    // Son (phase C) : en-tetes de flux, table des musiques, sons de portes, musiques par salle.
    if (ends_with(name, ".evd")) {
        track_begin(data, size, name);
        if (!conv_evd(s, 0, size))
            std::printf("[PORT] endian : %s : cinematique non convertie (ou deja faite)\n", name);
        return;
    }
    if (ends_with(name, "bio4str.hed")) { snd_fix_str_hed(s); return; }
    if (ends_with(name, "bio4midi.hed")) { snd_swap32n(s.base, s.size / 4); return; }
    if (ends_with(name, "doorse.hed") || ends_with(name, "bgmtbl.dat")) { snd_fix_words_then_halves(s); return; }
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
            conv_ofs_archive(s, name);                  // retourne la table (dont le mot 1)
            if (ofs && s.has(ofs, 0x40))
                port_rel_prepare(s.base + ofs);
        }
        return;
    }
    // op/opNN.das : appels radio (Sscrn ss_term.cpp). 0x400 octets de remplissage, puis une archive a table de decalages
    // (n, trois mots nuls, ofs[n] ; SsArc::ofs[4..] = OP_ARC_PTR 4..29) : entrees 4..16 tables de messages (MesData type 2),
    // 17..29 sequences TermSeq de 0x10 octets (u16, s16, int time, int mesNo, int arg ; arg = -1 termine). Non convertie,
    // la radio suivait des decalages sauvages (plantage dans SsTermMain::OpeMesMove, 12/11).
    if (std::strstr(name, "op/op") && ends_with(name, ".das")) {
        if (size <= 0x400)
            return;
        const Span a = { s.base + 0x400, size - 0x400 };
        if (!swap_arc(a)) {
            std::printf("[PORT] endian : %s : archive radio inattendue\n", name);
            return;
        }
        const u32 n = read_le32(a.base);
        for (u32 k = 0; k < n; ++k) {
            const u32 o = read_le32(a.base + 0x10 + 4 * k);
            if (!o)
                continue;
            const u32 end = arc_chunk_end(a, o);
            const u32 idx = k + 4;   // OP_ARC_PTR(wk, idx) lit SsArc::ofs[idx], soit l'entree k de la table
            if (idx <= 16) {
                if (!swap_mes_tbl(a, o, end))
                    std::printf("[PORT] endian : %s : messages %u inattendus\n", name, (unsigned) idx);
            } else {
                for (u32 t = o; t + 0x10 <= end; t += 0x10) {
                    u8 *e = a.base + t;
                    const u32 arg = read_be32(e + 0x0C);
                    swap_u16(e);
                    swap_u16(e + 2);
                    swap_u32(e + 4);
                    swap_u32(e + 8);
                    swap_u32(e + 0x0C);
                    if (arg == 0xFFFFFFFFu)
                        break;
                }
            }
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
        // Blocs du decor (stN/rXXX_NN.dat, charges par cBlock) : meme forme que les archives de salle (DataExtHeader :
        // nombre, trois mots nuls, decalages, etiquettes « SMD »...). Non convertis, ils laissaient le sol absent et une
        // collision a l'envers (nPoly 0xE305) qui debordait polyBit et remettait Block a zero (jeu fige).
        const char *base_nm = std::strrchr(name, '/');
        base_nm = base_nm ? base_nm + 1 : name;
        const size_t bl = std::strlen(base_nm);
        const bool block_name = (base_nm[0] == 'r' || base_nm[0] == 'R') && bl == 11 && base_nm[4] == '_' &&
                                std::strstr(name, "st") != nullptr;   // rXXX_NN.dat sous stN/
        if (block_name && s.has(0, 0x18)) {
            const u32 n = read_be32(s.base);
            bool arc = n >= 1 && n <= 0x40 && read_be32(s.base + 4) == 0 && read_be32(s.base + 8) == 0 &&
                       read_be32(s.base + 12) == 0 && s.has(0x10, n * 8);
            for (u32 i = 0; arc && i < n; ++i) {
                const u32 o = read_be32(s.base + 0x10 + 4 * i);
                const u8 *tg = s.base + 0x10 + n * 4 + 4 * i;
                arc = o >= 0x10 + n * 8 && o < s.size && tg[0] >= 'A' && tg[0] <= 'Z' && tg[1] >= 'A' && tg[1] <= 'Z';
            }
            if (arc) {
                port_room_arc_fix(s.base, s.size);
                return;
            }
        }
        if (swap_arc(s, false, std::strstr(name, "ss_") != nullptr || std::strstr(name, "SS_") != nullptr))
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
    const u32 shapeOfs = read_be32(h + 0x2C);   // table des formes (shape.cpp), 0 = aucune
    const bool ext = version > 0x20030817u && s.has(at, 0x48);
    const u32 blendTbl = ext ? read_be32(h + 0x40) : 0, flipTbl = ext ? read_be32(h + 0x44) : 0;
    const u32 flags = read_be32(h + 0x20);
    const u32 nParts = (u32) ((h[0x1A] << 8) | h[0x1B]);
    const u32 x2A = (u32) ((h[0x2A] << 8) | h[0x2B]);
    const u32 nVtx = (u32) ((h[0x38] << 8) | h[0x39]);
    const u32 nNrm = (u32) ((h[0x3A] << 8) | h[0x3B]);
    const u32 size_all = end - at;

    // Bornes de chaque region : la suivante dans le fichier.
    const u32 offs[11] = { pHead, pClr, pTex, pWeight, pParts, vtxOrig, nrmOrig, blendTbl, flipTbl, shapeOfs, size_all };
    auto region_end = [&](u32 o) {
        u32 e = size_all;
        for (u32 k = 0; k < 11; ++k)
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
    // Table des formes (visages qui parlent : radio, cinematiques ; shape.cpp CalculateShape_new) : u32 nombre, puis
    // `nombre` entrees {u32 decalage depuis la table (= shapeOfs + 4), s32 n}, chacune vers n x 4 s16 (indice de sommet,
    // dx, dy, dz). Plusieurs entrees peuvent viser la meme liste : chaque liste est retournee une fois, avec le plus
    // grand n. Non convertie, la radio plantait (12/11).
    if (shapeOfs >= 0x40 && ok(shapeOfs, 4)) {
        const u32 cnt = read_be32(h + shapeOfs);
        const u32 tbl = shapeOfs + 4;
        if (cnt > 0 && cnt <= 0x400 && ok(tbl, cnt * 8)) {
            u32 lo[0x400], ln[0x400], nl = 0;
            bool good = true;
            for (u32 i = 0; i < cnt && good; ++i) {
                const u32 o = read_be32(h + tbl + i * 8), n = read_be32(h + tbl + i * 8 + 4);
                if (n > 0x10000 || !ok(tbl + o, n * 8)) { good = false; break; }
                u32 k = 0;
                while (k < nl && lo[k] != o) ++k;
                if (k == nl) { lo[nl] = o; ln[nl] = n; ++nl; }
                else if (n > ln[k]) ln[k] = n;
            }
            if (good) {
                swap_u32(h + shapeOfs);
                for (u32 i = 0; i < cnt; ++i) {
                    swap_u32(h + tbl + i * 8);
                    swap_u32(h + tbl + i * 8 + 4);
                }
                for (u32 k = 0; k < nl; ++k)
                    for (u32 j = 0; j < ln[k] * 4; ++j)
                        swap_u16(h + tbl + lo[k] + j * 2);
            } else {
                std::printf("[PORT] endian : BIN a +0x%x : table des formes incoherente\n", (unsigned) at);
            }
        }
    }
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
            if (blendTbl + 4 <= be) {                  // s32 compte, puis (dst, a, b, pourcentage) en u16
                swap_u32(h + blendTbl);
                for (u32 o = blendTbl + 4; o + 2 <= be; o += 2)
                    swap_u16(h + o);
            }
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
                        u32 mo[0x100];
                        for (u32 k = 0; k < mn; ++k)
                            mo[k] = read_be32(m + 4 + 4 * k);
                        swap_u32(m);
                        for (u32 k = 0; k < mn; ++k)
                            swap_u32(m + 4 + 4 * k);
                        for (u32 k = 0; k < mn; ++k) {              // chaque mouvement finit ou commence le suivant
                            u32 me = s.size;
                            for (u32 j = 0; j < mn; ++j)
                                if (mo[j] > mo[k] && t + eo[3] + mo[j] < me)
                                    me = t + eo[3] + mo[j];
                            conv_motion(s, t + eo[3] + mo[k], me);
                        }
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
    // Blocs deja convertis pendant cet appel : plusieurs entrees d'une table peuvent viser le meme bloc.
    u32 done[0x400];
    u32 ndone = 0;
    auto first_time = [&](u32 t) {
        for (u32 i = 0; i < ndone; ++i)
            if (done[i] == t)
                return false;
        if (ndone < 0x400)
            done[ndone++] = t;
        return true;
    };
    // EspSeqData (include/esp.h) : en-tete de 0x30 puis `num` enregistrements EspGenWork de 0x12C. Chaque champ
    // a un seul type (les unions x40/x64/xD8 ne portent que des flottants) ; prm (0xCC..0xD3) est lu comme deux
    // mots gros-boutistes dont le jeu prend parfois la moitie ou l'octet bas : ces vues ont une disposition
    // propre au portage dans esp.h. Les zones dont on ne connait pas le type (0x100..0x103, 0x112..0x117,
    // 0x24..0x2F de l'en-tete) restent telles quelles.
    auto conv_seq = [&](u32 t) {
        track_label("eff:seq");
        if (!s.has(t, 0x30))
            return;
        u8 *h = s.base + t;
        const u32 num = (u32) ((h[0] << 8) | h[1]);
        if (num == 0 || num > 0x200 || !s.has(t, 0x30 + num * 0x12C))
            return;
        swap_u16(h);              // num
        swap_u16(h + 0x08);       // flags
        for (u32 o = 0x0C; o < 0x24; o += 4)
            swap_u32(h + o);      // pos, rot
        for (u32 r = 0; r < num; ++r) {
            u8 *e = h + 0x30 + r * 0x12C;
            swap_u16(e + 0x04);                                   // x4 : temps de la sequence
            swap_u32(e + 0x08);                                   // flags
            for (u32 o = 0x0C; o < 0x9C; o += 4) swap_u32(e + o); // flottants 0x0C..0x9B (x0C..x98)
            for (u32 o = 0xA0; o < 0xB0; o += 4) swap_u32(e + o); // xA0..xAC
            for (u32 o = 0xB0; o < 0xBC; o += 2) swap_u16(e + o); // xB0..xBA
            swap_u16(e + 0xBE);                                   // xBE
            swap_u32(e + 0xCC);                                   // prm.w.xCC
            swap_u32(e + 0xD0);                                   // prm.w.xD0
            swap_u32(e + 0xD4);                                   // xD4
            for (u32 o = 0xD8; o < 0xFC; o += 4) swap_u32(e + o); // xD8..xF8
            swap_u16(e + 0x110);                                  // x110
            for (u32 o = 0x118; o < 0x124; o += 4) swap_u32(e + o); // x118 (Vec)
        }
    };
    // Path (include/path.h) : u16 num, 2 octets, puis num sommets de 0x28 (pos, nrm, dist en flottants, octets).
    auto conv_path = [&](u32 t) {
        track_label("eff:path");
        if (!s.has(t, 4))
            return;
        u8 *h = s.base + t;
        const u32 num = (u32) ((h[0] << 8) | h[1]);
        if (num == 0 || num > 0x1000 || !s.has(t + 4, num * 0x28))
            return;
        swap_u16(h);
        for (u32 v = 0; v < num; ++v)
            for (u32 o = 0; o < 0x1C; o += 4)
                swap_u32(h + 4 + v * 0x28 + o);
    };
    // SstData : x0, puis n decalages (depuis l'en-tete) vers les blocs ; kind 0 sequences, 1 chemins.
    auto data_tbl = [&](u32 ofs, u32 n, int kind) {
        track_label("eff:data");
        if (!n || !s.has(at + ofs, 4 + n * 4))
            return;
        for (u32 i = 0; i < n + 1; ++i) {
            u8 *e = s.base + at + ofs + 4 * i;
            const u32 o = read_be32(e);
            swap_u32(e);
            if (i == 0 || o == 0)
                continue;
            const u32 t = at + ofs + o;
            if (t >= s.size || !first_time(t))
                continue;
            if (kind == 0)
                conv_seq(t);
            else
                conv_path(t);
        }
    };
    const u32 ntex = id_tbl(f[1]);                     // identifiants de textures
    ofs_tbl(f[6], 0, ntex);                            // TPL
    ofs_tbl(f[7], 1, ntex);                            // animations
    const u32 ne = list_tbl(f[2]);                     // EST
    const u32 ns = list_tbl(f[3]);                     // SST
    const u32 np = list_tbl(f[4]);                     // chemins
    data_tbl(f[8], ne, 0);
    data_tbl(f[9], ns, 0);
    data_tbl(f[10], np, 1);
    const u32 nefm = id_tbl(f[5]);                     // identifiants de modeles d'effets
    ofs_tbl(f[11], 2, nefm);                           // modeles d'effets
    return true;
}

// ---- MOT : un mouvement (MotionData, include/model.h ; flux de cles lu par HermiteInterpolation)
//
//   u16 maxFrame (14 bits) ; u8 nParts ; u16 infos[nParts] (octets 3.., non alignes) ;
//   u8 numeros[nParts] ; remplissage a 4 ; u32 mot de tete ; u32 cles[nParts] (decalages)
// Chaque cle est trois axes : u16 n ; u16 images[n] ; n valeurs dont la taille depend du type
// (bits 12 a 15 de l'info de la partie). Les **valeurs** sont lues octet par octet par le jeu
// (Fcc_get_data_*), donc laissees telles quelles : seuls n et la table des images se retournent.
bool conv_motion(const Span &s, u32 at, u32 end)
{
    track_label("MOT");
    if (end > s.size)
        end = s.size;
    if (!s.has(at, 4))
        return false;
    u8 *h = s.base + at;
    if (h[0] == 0 && h[1] == 0 && h[2] == 0 && h[3] == 0)
        return true;                               // entree vide
    const u32 nParts = h[2];
    u32 tbl = 3 + nParts * 2 + nParts;
    tbl = (tbl + 3) & ~3u;
    if (nParts == 0 || at + tbl + 4 + nParts * 4 > end) {
        std::printf("[PORT] endian : MOT a +0x%x : %u parties, en-tete deborde\n", (unsigned) at, (unsigned) nParts);
        return false;
    }
    // verification : chaque flux doit tenir et se terminer a l'interieur de l'entree
    u16 infos[256];
    if (nParts > 256)
        return false;
    for (u32 i = 0; i < nParts; ++i)
        infos[i] = (u16) ((h[3 + i * 2] << 8) | h[4 + i * 2]);
    u32 keys[256];
    for (u32 i = 0; i < nParts; ++i) {
        keys[i] = read_be32(h + tbl + 4 + i * 4);
        if (keys[i] + 2 > end - at) {
            std::printf("[PORT] endian : MOT a +0x%x : cle %u hors de l'entree\n", (unsigned) at, (unsigned) i);
            return false;
        }
    }
    swap_u16(h);
    for (u32 i = 0; i < nParts; ++i)
        swap_u16(h + 3 + i * 2);
    for (u32 i = 0; i < nParts + 1; ++i)
        swap_u32(h + tbl + i * 4);
    for (u32 i = 0; i < nParts; ++i) {
        const int type = (infos[i] >> 12) & 0xF;
        u32 p = keys[i];
        for (int axis = 0; axis < 3; ++axis) {
            if (p + 2 > end - at)
                break;
            const u32 n = (u32) ((h[p] << 8) | h[p + 1]);
            if (p + 2 + n * 2 > end - at)
                break;
            swap_u16_once(h + p);
            for (u32 k = 0; k < n; ++k)
                swap_u16_once(h + p + 2 + k * 2);
            const int sz = Fcc_next_axis_addr(type, (int) n);
            if (sz < 0)
                break;
            p += 2 + n * 2 + (u32) sz;
        }
    }
    return true;
}

// ---- Archive a table de decalages (pl*.drs, em*.drs, wep*.drs : PlArc, include/global.h)
//
// u32 ofs[n] au debut du fichier ; le mot 1 est le module REL (code, deja dans le binaire). Les
// autres entrees sont des modeles (BIN), des TPL, des mouvements (MOT) ou des donnees propres au
// personnage (visages, tables). Aucune etiquette : on reconnait le TPL et le BIN a leur numero de
// version, le mouvement a la coherence de son en-tete ; le reste est signale, entree par entree.
bool looks_like_motion(const Span &s, u32 at, u32 end)
{
    if (!s.has(at, 8) || end > s.size || at + 8 >= end)
        return false;
    const u8 *h = s.base + at;
    const u32 nParts = h[2];
    if (nParts == 0 || nParts > 200)
        return false;
    u32 tbl = (3 + nParts * 3 + 3) & ~3u;
    if (at + tbl + 4 + nParts * 4 > end)
        return false;
    const u32 maxFrame = (u32) ((h[0] << 8) | h[1]) & 0x3FFF;
    if (maxFrame == 0 || maxFrame > 20000)
        return false;
    for (u32 i = 0; i < nParts; ++i) {
        const u32 k = read_be32(h + tbl + 4 + i * 4);
        if (k < tbl + 4 + nParts * 4 || k + 2 > end - at)
            return false;
    }
    return true;
}

// Table de sequence d'un mouvement (MotionSeqKey, include/model.h) : u16 compte, deux octets, puis
// `compte` cles de quatre octets (u16 image en 10.6, deux octets).
bool looks_like_seqtbl(const Span &s, u32 at, u32 end)
{
    if (!s.has(at, 8) || end > s.size || at + 8 > end)
        return false;
    const u8 *h = s.base + at;
    const u32 n = (u32) ((h[0] << 8) | h[1]);
    if (n == 0 || n > 2000 || 4 + n * 4 > end - at || h[2] > 1)
        return false;
    if (h[4] != 0 || h[5] != 0)                 // la premiere cle est a l'image 0
        return false;
    u32 prev = 0;
    for (u32 i = 0; i < n; ++i) {
        const u32 fr = (u32) ((h[4 + i * 4] << 8) | h[5 + i * 4]);
        if (fr < prev)
            return false;
        prev = fr;
    }
    return true;
}

bool conv_seqtbl(const Span &s, u32 at)
{
    track_label("SEQ");
    u8 *h = s.base + at;
    const u32 n = (u32) ((h[0] << 8) | h[1]);
    swap_u16_once(h);
    for (u32 i = 0; i < n; ++i)
        swap_u16_once(h + 4 + i * 4);
    return true;
}

// Etiquette SEQ d'une archive de salle (sequences des objets animes, lues par le code de la salle via ROOM_ARC_PTR).
bool conv_seq_tag(const Span &s, u32 at, u32 end)
{
    if (end > s.size)
        end = s.size;
    return looks_like_seqtbl(s, at, end) && conv_seqtbl(s, at);
}

// Table de decalages : u32 compte puis `compte` decalages croissants, relatifs a la table.
bool looks_like_ofstbl(const Span &s, u32 at, u32 end)
{
    if (!s.has(at, 8) || end > s.size)
        return false;
    const u32 n = read_be32(s.base + at);
    if (n == 0 || n > 0x100 || at + 4 + n * 4 > end)
        return false;
    u32 prev = 0;
    for (u32 i = 0; i < n; ++i) {
        const u32 o = read_be32(s.base + at + 4 + i * 4);
        if (o < 4 + n * 4 || o >= end - at || o < prev)
            return false;
        prev = o;
    }
    return true;
}

// Reconnait un bloc a sa forme et le convertit ; rend vrai si c'est fait.
bool conv_blob(const Span &s, u32 at, u32 end, int depth, const char **kind)
{
    const char *k = "?";
    bool done = false;
    u32 allz = 1;
    for (u32 i = 0; i < 16 && at + i < s.size; ++i)
        if (s.base[at + i] != 0 && s.base[at + i] != 0xCD)
            allz = 0;
    if (allz) {
        k = "vide";
        done = true;
    } else if (s.has(at, 12) && (read_be32(s.base + at) == 0x0020AF30u || read_le32(s.base + at) == 0x0020AF30u)) {
        k = "TPL";
        done = swap_tpl(s, at);
    } else if (s.has(at, 0x40) && (read_be32(s.base + at + 0x3C) == 0x20010801u || read_be32(s.base + at + 0x3C) == 0x20030817u ||
                                   read_be32(s.base + at + 0x3C) == 0x20030818u)) {
        k = "BIN";
        done = conv_model_bin(s, at, end);
    } else if (looks_like_motion(s, at, end)) {
        k = "MOT";
        done = conv_motion(s, at, end);
    } else if (looks_like_seqtbl(s, at, end)) {
        k = "SEQ";
        done = conv_seqtbl(s, at);
    } else if (s.has(at, 8) && (s.base[at] << 8 | s.base[at + 1]) >= 1 && (s.base[at] << 8 | s.base[at + 1]) <= 32 &&
               s.base[at + 2] >= 0x20 && s.base[at + 2] <= 0x2C &&
               read_be32(s.base + at + 4) == 4 + 4 * (u32) (s.base[at] << 8 | s.base[at + 1]) + (read_be32(s.base + at + 4) - 4 - 4 * (u32) (s.base[at] << 8 | s.base[at + 1]))
               && read_be32(s.base + at + 4) >= 4 + 4 * (u32) (s.base[at] << 8 | s.base[at + 1]) && read_be32(s.base + at + 4) < 0x100) {
        k = "LIT";
        done = conv_lit(s, at, end);
    } else if (depth < 3 && looks_like_ofstbl(s, at, end)) {
        k = "TABLE";
        const u32 n = read_be32(s.base + at);
        u32 offs[0x100];
        for (u32 i = 0; i < n; ++i)
            offs[i] = read_be32(s.base + at + 4 + i * 4);
        swap_u32_once(s.base + at);
        for (u32 i = 0; i < n; ++i)
            swap_u32_once(s.base + at + 4 + i * 4);
        done = true;
        for (u32 i = 0; i < n; ++i) {
            u32 e = end;
            for (u32 j = 0; j < n; ++j)
                if (at + offs[j] > at + offs[i] && at + offs[j] < e)
                    e = at + offs[j];
            const char *sub = "?";
            if (!conv_blob(s, at + offs[i], e, depth + 1, &sub))
                done = false;
        }
    }
    if (kind)
        *kind = k;
    return done;
}

u32 conv_ofs_archive(const Span &s, const char *name)
{
    track_begin(s.base, s.size, name);
    track_label("drs");
    // taille de la table : elle s'arrete au plus petit decalage
    u32 limit = s.size;
    u32 n = 0;
    while (n < 0x800 && (n * 4 + 4) <= limit && s.has(n * 4, 4)) {
        const u32 w = read_be32(s.base + n * 4);
        if (n >= 1 && w && w >= 8 && w < limit)     // le mot 0 est un compte, pas un decalage
            limit = w;
        ++n;
    }
    if (n < 2 || limit > 0x4000) {
        if (g_trace_on)
            std::printf("[ARC] %s : table de decalages non reconnue (n=%u, limite=%x, octets %02x %02x %02x %02x %02x %02x %02x %02x)\n", name,
                        (unsigned) n, (unsigned) limit, s.base[0], s.base[1], s.base[2], s.base[3], s.base[4], s.base[5], s.base[6], s.base[7]);
        return 0;
    }
    u32 offs[0x800];
    for (u32 i = 0; i < n; ++i)
        offs[i] = read_be32(s.base + i * 4);
    for (u32 i = 0; i < n; ++i)
        swap_u32(s.base + i * 4);
    u32 unknown = 0;
    for (u32 i = 0; i < n; ++i) {
        const u32 o = offs[i];
        if (!o || i == 0 || i == 1 || o >= s.size)
            continue;
        u32 e = s.size;
        for (u32 j = 1; j < n; ++j)
            if (offs[j] > o && offs[j] < e)
                e = offs[j];
        const char *kind = "?";
        const bool done = conv_blob(s, o, e, 0, &kind);
        if (g_trace_on)
            std::printf("[ARC] %s entree %u +0x%x (%u octets) : %s %s%s", name, (unsigned) i, (unsigned) o, (unsigned) (e - o), kind,
                        done ? "converti" : "NON converti", done ? "\n" : "  :");
        if (g_trace_on && !done) {
            for (u32 k = 0; k < 24 && o + k < s.size; ++k)
                std::printf(" %02x", s.base[o + k]);
            std::printf("\n");
        }
        if (!done)
            ++unknown;
    }
    return unknown;
}

// ---- SHD : modeles d'ombre / de decor secondaires (src/game/shadow.cpp, include/shadow.h)
//
// u8 version, u8, u16 nombre, u32 decalage de la table des modeles, remplissage a 0x10, puis des
// entrees de 0x48 octets (position, rotation, echelle : neuf flottants, puis des octets). La table
// de modeles est un tableau de decalages relatifs a lui-meme, vers des BIN.
bool conv_shd(const Span &s, u32 at, u32 end)
{
    track_label("SHD");
    if (end > s.size)
        end = s.size;
    if (!s.has(at, 0x10))
        return false;
    u8 *h = s.base + at;
    const u32 num = (u32) ((h[2] << 8) | h[3]);
    const u32 tblOfs = read_be32(h + 4);
    if (num > 0x200 || !s.has(at + 0x10, num * 0x48))
        return false;
    swap_u16(h + 2);
    swap_u32(h + 4);
    u32 maxModel = 0;
    for (u32 i = 0; i < num; ++i) {
        u8 *e = h + 0x10 + i * 0x48;
        if (e[0x24] + 1u > maxModel)
            maxModel = e[0x24] + 1u;
        for (int k = 0; k < 9; ++k)
            swap_u32(e + 4 * k);
    }
    if (maxModel == 0 || !s.has(at + tblOfs, maxModel * 4))
        return true;
    u8 *tbl = h + tblOfs;
    u32 offs[256];
    if (maxModel > 256)
        maxModel = 256;
    for (u32 i = 0; i < maxModel; ++i)
        offs[i] = read_be32(tbl + 4 * i);
    for (u32 i = 0; i < maxModel; ++i)
        swap_u32_once(tbl + 4 * i);
    bool ok = true;
    for (u32 i = 0; i < maxModel; ++i) {
        const u32 a = at + tblOfs + offs[i];
        u32 e = end;
        for (u32 j = 0; j < maxModel; ++j)
            if (at + tblOfs + offs[j] > a && at + tblOfs + offs[j] < e)
                e = at + tblOfs + offs[j];
        if (!conv_model_bin(s, a, e))
            ok = false;
    }
    return ok;
}

// ---- ITM : modeles d'objets (src/game/item_model.cpp)
//
// u32 x0, u32 decalages vers la table des identifiants (u32 compte, puis u16 id + 6 octets), la
// table des modeles (u32 compte, decalages relatifs a la table) et la table des TPL, idem.
bool conv_itm(const Span &s, u32 at, u32 end)
{
    track_label("ITM");
    if (end > s.size)
        end = s.size;
    if (!s.has(at, 0x10))
        return false;
    u8 *h = s.base + at;
    const u32 ofsId = read_be32(h + 4), ofsBin = read_be32(h + 8), ofsTpl = read_be32(h + 12);
    for (int k = 0; k < 4; ++k)
        swap_u32(h + 4 * k);
    if (!s.has(at + ofsId, 4))
        return false;
    const u32 n = read_be32(s.base + at + ofsId);
    if (n > 0x100 || !s.has(at + ofsId + 4, n * 8))
        return false;
    swap_u32(s.base + at + ofsId);
    for (u32 i = 0; i < n; ++i) {
        u8 *e = s.base + at + ofsId + 4 + i * 8;
        swap_u16(e);
        swap_u32(e + 4);
    }
    bool ok = true;
    for (int pass = 0; pass < 2; ++pass) {
        const u32 t = pass == 0 ? ofsBin : ofsTpl;
        if (!s.has(at + t, 4 + n * 4))
            return false;
        u32 offs[0x100];
        for (u32 i = 0; i < n; ++i)
            offs[i] = read_be32(s.base + at + t + 4 + i * 4);
        swap_u32(s.base + at + t);
        for (u32 i = 0; i < n; ++i)
            swap_u32_once(s.base + at + t + 4 + i * 4);
        for (u32 i = 0; i < n; ++i) {
            const u32 a = at + t + offs[i];
            u32 e = end;
            for (u32 j = 0; j < n; ++j)
                if (at + t + offs[j] > a && at + t + offs[j] < e)
                    e = at + t + offs[j];
            if (pass == 0) {
                // un BIN finit au plus tard ou commence la table des TPL
                if (at + ofsTpl > a && at + ofsTpl < e)
                    e = at + ofsTpl;
                if (!conv_model_bin(s, a, e))
                    ok = false;
            } else if (!swap_tpl(s, a)) {
                ok = false;
            }
        }
    }
    return ok;
}

// ---- ETM : archive de fichiers nommes des objets decoratifs (src/game/EtcModel.cpp, EtcArc)
//
// u32 nombre, remplissage jusqu'a 0x20, puis des fichiers : u32 taille (en-tete de 0x40 compris),
// nom sur 0x20 octets a partir de +0x20, donnees a +0x40. L'extension dit le format.
bool conv_etm(const Span &s, u32 at, u32 end)
{
    track_label("ETM");
    if (end > s.size)
        end = s.size;
    if (!s.has(at, 0x20))
        return false;
    u8 *h = s.base + at;
    const u32 n = read_be32(h);
    if (n > 0x200)
        return false;
    swap_u32(h);
    u32 f = at + 0x20;
    bool ok = true;
    for (u32 i = 0; i < n; ++i) {
        if (!s.has(f, 0x40))
            return false;
        u8 *fh = s.base + f;
        const u32 size = read_be32(fh);
        swap_u32(fh);
        if (size < 0x40 || f + size > end)
            return false;
        const char *name = (const char *) fh + 0x20;
        const u32 dat = f + 0x40, dend = f + size;
        const size_t ln = strnlen(name, 0x20);
        bool r = true;
        if (ln > 4 && !std::strcmp(name + ln - 4, ".eff"))
            r = swap_eff_data(s, dat);
        else if (ln > 4 && !std::strcmp(name + ln - 4, ".bin"))
            r = conv_model_bin(s, dat, dend);
        else if (ln > 4 && !std::strcmp(name + ln - 4, ".tpl"))
            r = swap_tpl(s, dat);
        else if (ln > 4 && !std::strcmp(name + ln - 4, ".fcv"))
            r = conv_motion(s, dat, dend);
        else if (ln > 4 && !std::strcmp(name + ln - 4, ".seq"))   // sequences de l'echelle (Et06_init, et0600x0.seq)
            r = conv_seqtbl(s, dat);
        else {
            if (g_trace_on)
                std::printf("[ARC] ETM : fichier '%.32s' (%u octets) sans convertisseur\n", name, (unsigned) (size - 0x40));
            r = false;
        }
        if (!r)
            ok = false;
        f += size;
    }
    return ok;
}

// ---- ETS : liste des objets decoratifs de la salle (EtcList : u16 nombre, entrees de 0x28 octets des 0x10)
bool conv_ets(const Span &s, u32 at, u32 end)
{
    track_label("ETS");
    (void) end;
    if (!s.has(at, 0x10))
        return false;
    u8 *h = s.base + at;
    const u32 n = (u32) ((h[0] << 8) | h[1]);
    if (n > 0x200 || !s.has(at + 0x10, n * 0x28))
        return false;
    swap_u16(h);
    for (u32 i = 0; i < n; ++i) {
        u8 *e = h + 0x10 + i * 0x28;
        swap_u16(e);
        swap_u16(e + 2);
        for (int k = 0; k < 6; ++k)
            swap_u32(e + 0x10 + 4 * k);
    }
    return true;
}

// ---- Paquet de textures (tag TEX) : un TexData du systeme "id"
bool conv_tex(const Span &s, u32 at, u32 end)
{
    (void) end;
    return swap_id_tex(s, at);
}

bool conv_eff_tag(const Span &s, u32 at, u32 end)
{
    (void) end;
    return swap_eff_data(s, at);
}

// ---- CAM : cameras de la salle (src/game/cam_ctrl.cpp, include/cam_ctrl.h ; "B40x")
//
// En-tete de 0x10 (version en quatre lettres, nombres de coupes / zones / liens), puis
// CameraAreaRec[0x10], CameraAreaInfo[0x30], CameraCut[0x34], CameraLerp[0x10]. Les decalages sont
// relatifs a l'en-tete (calcAddr les transforme en pointeurs) ; les tableaux de points, positions,
// visees, roulis, champ de vue et images viennent apres. Les versions B400/B401 ne sont pas
// relogees par le jeu : elles gardent un autre format, laisse tel quel.
bool conv_cam(const Span &s, u32 at, u32 end)
{
    track_label("CAM");
    if (end > s.size)
        end = s.size;
    if (!s.has(at, 0x10))
        return false;
    u8 *h = s.base + at;
    if (h[0] == 'E' && h[1] == 'M' && h[2] == 'P')
        return true;                               // entree vide ("EMPTY")
    if (h[0] != 'B' || h[1] != '4' || h[2] != '0') {
        std::printf("[PORT] endian : CAM a +0x%x : version %02x %02x %02x %02x\n", (unsigned) at, h[0], h[1], h[2], h[3]);
        return false;
    }
    if (h[3] < '2')
        return true;                               // anciennes versions : pas de relogement
    const u32 nCut = h[4], nArea = h[5], nLerp = h[6];
    const u32 recs = 0x10, infos = recs + nArea * 0x10, cuts = infos + nArea * 0x30, lerps = cuts + nCut * 0x34;
    if (!s.has(at, lerps + nLerp * 0x10))
        return false;
    auto vec_arr = [&](u32 ofs, u32 n) {
        if (ofs && s.has(at + ofs, n * 12))
            for (u32 k = 0; k < n * 3; ++k)
                swap_u32_once(h + ofs + 4 * k);
    };
    auto f32_arr = [&](u32 ofs, u32 n) {
        if (ofs && s.has(at + ofs, n * 4))
            for (u32 k = 0; k < n; ++k)
                swap_u32_once(h + ofs + 4 * k);
    };
    for (u32 i = 0; i < nArea; ++i) {
        u8 *r = h + recs + i * 0x10;
        swap_u32(r + 8);
        swap_u32(r + 0xC);
        u8 *a = h + infos + i * 0x30;
        const u32 num = read_be32(a + 0x28), pts = read_be32(a + 0x2C);
        swap_u32(a + 4);
        swap_u32(a + 0x20);
        swap_u32(a + 0x24);
        swap_u32(a + 0x28);
        swap_u32(a + 0x2C);
        vec_arr(pts, num);
    }
    for (u32 i = 0; i < nCut; ++i) {
        u8 *c = h + cuts + i * 0x34;
        const u32 num = read_be32(c + 0x20), frames = read_be32(c + 0x10);
        const u32 pos = read_be32(c + 0x24), tgt = read_be32(c + 0x28), roll = read_be32(c + 0x2C), fovy = read_be32(c + 0x30);
        for (u32 k = 1; k <= 3; ++k)                // aim_ofs
            swap_u32(c + 4 * k);
        swap_u32(c + 0x10);
        swap_u32(c + 0x14);
        static const u32 cw[] = { 0x20u, 0x24u, 0x28u, 0x2Cu, 0x30u };
        for (u32 o : cw)
            swap_u32(c + o);
        vec_arr(pos, num);
        vec_arr(tgt, num);
        f32_arr(roll, num);
        f32_arr(fovy, num);
        if (frames && s.has(at + frames, num * 2))
            for (u32 k = 0; k < num; ++k)
                swap_u16_once(h + frames + 2 * k);
    }
    for (u32 i = 0; i < nLerp; ++i)
        swap_u32(h + lerps + i * 0x10 + 8);
    return true;
}

// ---- STB : en-tete sonore de la salle (src/game/snd.cpp, include/snd.h : SndRoomHdr)
//
// Deux blocs de reverberation (4 x u16 puis 6 flottants), puis quatre tableaux de 32 decalages
// (selection de courbes, volume, hauteur, filtre) vers des SndCurveTbl : u32 compte, f32 echelle,
// entrees de 8 octets (f32 distance, u16, u16 valeur). La valeur est lue comme s16 pour la hauteur
// mais comme **un octet a +7** pour le volume et le filtre : on ne la retourne que pour la hauteur.
bool conv_stb(const Span &s, u32 at, u32 end)
{
    track_label("STB");
    (void) end;
    if (!s.has(at, 0x240))
        return false;
    u8 *h = s.base + at;
    for (int e = 0; e < 2; ++e) {
        u8 *p = h + e * 0x20;
        for (int k = 0; k < 4; ++k)
            swap_u16(p + 2 * k);
        for (int k = 2; k < 8; ++k)
            swap_u32(p + 4 * k);
    }
    u32 offs[4][32];
    for (int t = 0; t < 4; ++t)
        for (int i = 0; i < 32; ++i)
            offs[t][i] = read_be32(h + 0x40 + t * 0x80 + i * 4);
    for (int k = 0; k < 128; ++k)
        swap_u32(h + 0x40 + 4 * k);
    bool ok = true;
    for (int t = 1; t < 4; ++t) {                  // 1 volume, 2 hauteur, 3 filtre
        for (int i = 0; i < 32; ++i) {
            const u32 o = offs[t][i];
            if (!o)
                continue;
            if (!s.has(at + o, 8)) {
                ok = false;
                continue;
            }
            u8 *c = h + o;
            const u32 n = read_be32(c);
            if (n > 0x400 || !s.has(at + o + 8, n * 8)) {
                ok = false;
                continue;
            }
            if (was_swapped(c, 4))
                continue;                          // courbe partagee par plusieurs entrees
            swap_u32(c);
            swap_u32(c + 4);
            for (u32 k = 0; k < n; ++k) {
                u8 *en = c + 8 + k * 8;
                swap_u32(en);
                swap_u16(en + 4);
                if (t == 2)
                    swap_u16(en + 6);
            }
        }
    }
    return ok;
}

// ---- DSE : numeros de bruitage des portes (SndDoorSe : u32 nombre, puis (u16 salle, u16 porte[5]))
bool conv_dse(const Span &s, u32 at, u32 end)
{
    track_label("DSE");
    (void) end;
    if (!s.has(at, 4))
        return false;
    u8 *h = s.base + at;
    const u32 n = read_be32(h);
    if (n > 0x400 || !s.has(at + 4, n * 12))
        return false;
    swap_u32(h);
    for (u32 i = 0; i < n * 6; ++i)
        swap_u16(h + 4 + i * 2);
    return true;
}

// ---- SAT / EAT : collisions fixes (src/game/atari.cpp, include/atari.h : cSatHeader / cSatFile)
//
// Une table (octet 0 avec le bit 7 : u32 decalages des fichiers a partir de +4) ou directement un
// fichier : 0xFF, un octet, neuf demi-mots (nombres de sommets, normales, aretes, polygones...),
// puis Vec vtx[], nrm[], edge[], AtPoly[] de 0x14 octets, puis le partitionnement spatial :
// cSatBlock (six flottants, quatre demi-mots, un decalage `next`, des indices u16 ou - bit 0 de
// `flag` - un bloc fils), chaines par `next` relatif.
static bool conv_sat_blocks(const Span &s, u32 at, u32 end, int depth)
{
    if (depth > 8)
        return false;
    u32 guard = 0;
    for (;;) {
        if (!s.has(at, 0x24) || at + 0x24 > end || ++guard > 100000)
            return false;
        u8 *b = s.base + at;
        const u32 n0 = (u32) ((b[0x18] << 8) | b[0x19]), n1 = (u32) ((b[0x1A] << 8) | b[0x1B]);
        const u32 n2 = (u32) ((b[0x1C] << 8) | b[0x1D]), flag = (u32) ((b[0x1E] << 8) | b[0x1F]);
        const u32 next = read_be32(b + 0x20);
        for (int k = 0; k < 6; ++k)
            swap_u32(b + 4 * k);
        for (int k = 0; k < 4; ++k)
            swap_u16(b + 0x18 + 2 * k);
        swap_u32(b + 0x20);
        if (flag & 1) {
            if (!conv_sat_blocks(s, at + 0x24, end, depth + 1))
                return false;
        } else {
            const u32 n = n0 + n1 + n2;
            if (!s.has(at + 0x24, n * 2))
                return false;
            for (u32 i = 0; i < n; ++i)
                swap_u16(b + 0x24 + 2 * i);
        }
        if (next == 0)
            return true;
        if (at + next >= end)
            return false;
        at += next;
    }
}

static bool conv_sat_file(const Span &s, u32 at, u32 end, bool from_table = false)
{
    track_label("SAT");
    if (end > s.size)
        end = s.size;
    if (!s.has(at, 0x14))
        return false;
    u8 *h = s.base + at;
    // Fichier seul : id 0xFF. Dans une table (salle, EAT) l'id vaut autre chose (0x20 dans r100) : le jeu ne le
    // verifie pas (cSatFile::dataCheck : nPoly <= 0x1FFF), on ne l'exige donc pas non plus.
    if (h[0] != 0xFF && !from_table)
        return false;
    u32 v[9];
    for (int k = 0; k < 9; ++k)
        v[k] = (u32) ((h[2 + 2 * k] << 8) | h[3 + 2 * k]);
    const u32 nVtx = v[0], nNrm = v[1], nEdge = v[2], nPoly = v[4];
    const u32 vecs = nVtx + nNrm + nEdge;
    if (nPoly > 0x2000 || !s.has(at + 0x14, vecs * 12 + nPoly * 0x14)) {
        std::printf("[PORT] endian : SAT a +0x%x : %u sommets %u polygones hors du fichier\n", (unsigned) at, (unsigned) nVtx, (unsigned) nPoly);
        return false;
    }
    for (int k = 0; k < 9; ++k)
        swap_u16(h + 2 + 2 * k);
    for (u32 i = 0; i < vecs * 3; ++i)
        swap_u32(h + 0x14 + 4 * i);
    u8 *poly = h + 0x14 + vecs * 12;
    for (u32 i = 0; i < nPoly; ++i) {
        u8 *p = poly + i * 0x14;
        for (int k = 0; k < 7; ++k)
            swap_u16(p + 2 * k);
        swap_u32(p + 0x10);
    }
    return conv_sat_blocks(s, at + 0x14 + vecs * 12 + nPoly * 0x14, end, 0);
}

bool conv_sat(const Span &s, u32 at, u32 end)
{
    if (end > s.size)
        end = s.size;
    if (!s.has(at, 8))
        return false;
    u8 *h = s.base + at;
    if (h[0] == 0xFF)
        return conv_sat_file(s, at, end);
    // Fichier seul dont l'id n'est pas 0xFF (0x20 : collisions des objets mobiles de r109, r119..., lues par le code de
    // la salle via ROOM_ARC_PTR). L'etiquette dit deja que c'est une collision ; le jeu ne verifie pas l'id.
    if (!(h[0] & 0x80))
        return conv_sat_file(s, at, end, true);
    const u32 first = read_be32(h + 4);
    if (first < 8 || (first & 3) || first > 0x100)
        return false;
    const u32 n = (first - 4) / 4;
    u32 offs[64];
    if (n > 64 || !s.has(at + 4, n * 4))
        return false;
    for (u32 i = 0; i < n; ++i)
        offs[i] = read_be32(h + 4 + 4 * i);
    for (u32 i = 0; i < n; ++i)
        swap_u32(h + 4 + 4 * i);
    bool ok = true;
    for (u32 i = 0; i < n; ++i) {
        u32 e = end;
        for (u32 j = 0; j < n; ++j)
            if (at + offs[j] > at + offs[i] && at + offs[j] < e)
                e = at + offs[j];
        if (!conv_sat_file(s, at + offs[i], e, true))
            ok = false;
    }
    return ok;
}

// ---- RTP : points de passage des ennemis (src/game/route_ck.cpp, include/route_ck.h : RtpData)
//
// En-tete : 6 octets, u16 nombre de points a +6, u32 decalages des points (+0xC), des liens
// (+0x10) et du tableau des sauts suivants (+0x14, octets). Points : Vec, u16, u16 (16 octets).
// Liens : s16, u16 (4 octets), en nombre = max(debut + compte) des points.
bool conv_rtp(const Span &s, u32 at, u32 end)
{
    track_label("RTP");
    (void) end;
    if (!s.has(at, 0x18))
        return false;
    u8 *h = s.base + at;
    const u32 n = (u32) ((h[6] << 8) | h[7]);
    const u32 pOfs = read_be32(h + 0xC), lOfs = read_be32(h + 0x10);
    if (n == 0) {                                  // salle sans chemin d'ennemis : en-tete seul
        swap_u16(h + 6);
        swap_u32(h + 0xC);
        swap_u32(h + 0x10);
        swap_u32(h + 0x14);
        return true;
    }
    if (n > 0x400 || !s.has(at + pOfs, n * 16)) {
        std::printf("[PORT] endian : RTP a +0x%x : %u points\n", (unsigned) at, (unsigned) n);
        return false;
    }
    u32 nLink = 0;
    for (u32 i = 0; i < n; ++i) {
        const u8 *p = h + pOfs + i * 16;
        const u32 lo = (u32) ((p[12] << 8) | p[13]), lc = (u32) ((p[14] << 8) | p[15]);
        if (lo + lc > nLink)
            nLink = lo + lc;
    }
    if (!s.has(at + lOfs, nLink * 4))
        return false;
    swap_u16(h + 6);
    swap_u32(h + 0xC);
    swap_u32(h + 0x10);
    swap_u32(h + 0x14);
    for (u32 i = 0; i < n; ++i) {
        u8 *p = h + pOfs + i * 16;
        for (int k = 0; k < 3; ++k)
            swap_u32(p + 4 * k);
        swap_u16(p + 12);
        swap_u16(p + 14);
    }
    for (u32 i = 0; i < nLink * 2; ++i)
        swap_u16(h + lOfs + 2 * i);
    return true;
}

// ---- Zones declencheuses (include/area.h : AreaData, 0x30 octets)
//   u8 drapeau, u8 type, u16, puis 11 flottants dont le sens depend du type.
static void swap_area(u8 *a)
{
    swap_u16(a + 2);
    for (int k = 0; k < 11; ++k)
        swap_u32(a + 4 + 4 * k);
}

// ---- BLK : blocs de la salle (include/block.h, game/block.cpp cBlock::roomInit). En-tete : "BLK\0", u16 version (0x100),
// u8, u8 nBlock, u16 nArea, u16 nConnect, u32 ofsLink, ofsArea, ofsConnect. BlockLink (12 o) et BlockConnect (0x14 o) ne
// sont que des octets ; BlockArea (0x38 o) : u32 lien OT, AreaData, 4 octets. Non convertie, « BLOCK AREA DATA IS OLD
// VERSION » : aucun bloc cree, donc aucun decor dans les salles a blocs (r100, 12/11).
bool conv_blk(const Span &s, u32 at, u32 end)
{
    track_label("BLK");
    (void) end;
    if (!s.has(at, 0x18))
        return false;
    u8 *h = s.base + at;
    if (std::memcmp(h, "BLK", 4) != 0)
        return false;
    if (((h[4] << 8) | h[5]) != 0x100)
        return false;
    const u32 nArea = (u32) ((h[8] << 8) | h[9]);
    const u32 ofsArea = read_be32(h + 0x10);
    if (nArea > 0x400 || (nArea && !s.has(at + ofsArea, nArea * 0x38)))
        return false;
    swap_u16(h + 4);
    swap_u16(h + 8);
    swap_u16(h + 0xA);
    swap_u32(h + 0xC);
    swap_u32(h + 0x10);
    swap_u32(h + 0x14);
    for (u32 i = 0; i < nArea; ++i) {
        u8 *a = h + ofsArea + i * 0x38;
        swap_u32(a);
        swap_area(a + 4);
    }
    return true;
}

// ---- AEV / ITA : zones de scenario et objets poses (include/sce_at.h SceAtWork, src/game/sce_at.cpp SceAtFileHead).
// En-tete 0x10 : "AEV\0" / "ITA\0", u16 version (0x104 / 0x105), u16 num, 8 octets ; puis num SceAtWork de 0x9C.
// Partie commune : u32 lien OT, AreaData (0x04), octets 0x34..0x3B, int arg (0x3C), fonction (0x40, posee par le jeu),
// octets 0x44..0x4B, modele parent (0x4C, pose par le jeu), s16 parentParts (0x50), octets. Puis l'union de 0x5C, selon le
// type x35 (sceAtFunc_tbl) : porte, objet, drapeau, message, sauvegarde, ombre, degats, collision, camera, champ, echelle,
// objet utilise, cachette, saut. Les pointeurs et fonctions (poses a l'execution) ne sont pas touches. Non convertie :
// « SceAt DATA IS OLD VERSION » / « SceItem DATA IS OLD VERSION » : ni portes, ni actions, ni objets (12/11).
static void swap_f32n(u8 *p, int n)
{
    for (int k = 0; k < n; ++k)
        swap_u32(p + 4 * k);
}

bool conv_sceat(const Span &s, u32 at, u32 end)
{
    track_label("AEV");
    (void) end;
    if (!s.has(at, 0x10))
        return false;
    u8 *h = s.base + at;
    const bool aev = std::memcmp(h, "AEV", 4) == 0, ita = std::memcmp(h, "ITA", 4) == 0;
    if (!aev && !ita)
        return false;
    const u32 ver = (u32) ((h[4] << 8) | h[5]), num = (u32) ((h[6] << 8) | h[7]);
    if ((aev && ver != 0x104) || (ita && ver != 0x105))
        return false;
    const u32 rec = 0x9C;
    if (num > 0x400 || !s.has(at + 0x10, num * rec))
        return false;
    swap_u16(h + 4);
    swap_u16(h + 6);
    for (u32 i = 0; i < num; ++i) {
        u8 *w = h + 0x10 + i * rec;
        swap_u32(w);                // lien OT
        swap_area(w + 0x04);
        swap_u32(w + 0x3C);         // arg
        swap_u32(w + 0x40);         // fonction (nulle sur le disque)
        swap_u32(w + 0x4C);         // modele parent (nul sur le disque)
        swap_u16(w + 0x50);         // parentParts
        u8 *u = w + 0x5C;
        switch (w[0x35]) {
        case 0x01:                  // porte : Vec, f32, octets, fonction, octets, int arg
            swap_f32n(u, 4);
            swap_u32(u + 0x14);
            swap_u32(u + 0x1C);
            break;
        case 0x03:                  // objet : Vec pos, modele, Vec ofs, u16 x4, octets, s16 saveNo, f32 size, Vec rot
            swap_f32n(u, 3);
            swap_u32(u + 0x0C);
            swap_f32n(u + 0x10, 3);
            for (int k = 0; k < 4; ++k) swap_u16(u + 0x1C + 2 * k);
            swap_u16(u + 0x2A);
            swap_f32n(u + 0x2C, 4);
            break;
        case 0x04:                  // drapeau : u8, u8, u16 no
            swap_u16(u + 2);
            break;
        case 0x05:                  // message : s16 type, s16 no, u8, u8, u16
            swap_u16(u);
            swap_u16(u + 2);
            swap_u16(u + 6);
            break;
        case 0x08:                  // sauvegarde : int
        case 0x0D:                  // champ : int, modele
            swap_u32(u);
            swap_u32(u + 4);
            break;
        case 0x09:                  // ombre : u16 no
            swap_u16(u);
            break;
        case 0x0A:                  // degats : int time, octets, int arg, f32 power
            swap_u32(u);
            swap_u32(u + 8);
            swap_u32(u + 0x0C);
            break;
        case 0x0B:                  // collision : 2 pointeurs, octets, int attr, attr2, u32 flags, int flag
            swap_u32(u);
            swap_u32(u + 4);
            swap_f32n(u + 0x0C, 4);
            break;
        case 0x0C:                  // camera : Vec, f32, octets, f32 range, f32 range2
            swap_f32n(u, 4);
            swap_f32n(u + 0x14, 2);
            break;
        case 0x10:                  // echelle : Vec, f32, octets
            swap_f32n(u, 4);
            break;
        case 0x11:                  // objet utilise : u16 useItem[2]
            swap_u16(u);
            swap_u16(u + 2);
            break;
        case 0x12:                  // cachette : octets, Vec pos (0x24), fonction (0x30)
            swap_f32n(u + 0x24, 4);
            break;
        case 0x13:                  // saut : Vec
            swap_f32n(u, 3);
            break;
        default:                    // 0/6/7/0x14 normal (pointeurs poses par le jeu), 2 exec, 0xE, 0xF : rien sur le disque
            break;
        }
    }
    return true;
}

// ---- CNS : constantes de la salle (src/game/cons.cpp ConsRoom) : u32 num, u32 bits[(num >> 5) + 1], u32 valeurs[num].
bool conv_cns(const Span &s, u32 at, u32 end)
{
    track_label("CNS");
    (void) end;
    if (!s.has(at, 4))
        return false;
    const u32 num = read_be32(s.base + at);
    const u32 words = 1 + (num >> 5) + 1 + num;
    if (num > 0x1000 || !s.has(at, words * 4))
        return false;
    for (u32 i = 0; i < words; ++i)
        swap_u32(s.base + at + 4 * i);
    return true;
}

// ---- EMI : points des ennemis (include/embarrel.h EmiData) : int n, 4 octets, puis n entrees de 0x40 : octets, Vec pos
// (0x04), f32 rotY (0x10). Le reste de l'entree (0x14..0x3F) n'a pas de type connu dans le decompile : laisse tel quel.
bool conv_emi(const Span &s, u32 at, u32 end)
{
    track_label("EMI");
    (void) end;
    if (!s.has(at, 8))
        return false;
    const u32 n = read_be32(s.base + at);
    if (n > 0x400 || !s.has(at + 8, n * 0x40))
        return false;
    swap_u32(s.base + at);
    for (u32 i = 0; i < n; ++i)
        for (u32 k = 0; k < 4; ++k)
            swap_u32(s.base + at + 8 + i * 0x40 + 4 + 4 * k);
    return true;
}

// ---- EAR : zones d'affichage des effets (SstArea, include/espgen.h) : u32 nombre, entrees de 0x98 des 0x10
bool conv_ear(const Span &s, u32 at, u32 end)
{
    track_label("EAR");
    (void) end;
    if (!s.has(at, 0x10))
        return false;
    u8 *h = s.base + at;
    const u32 n = read_be32(h);
    if (n > 0x200 || !s.has(at + 0x10, n * 0x98))
        return false;
    swap_u32(h);
    for (u32 i = 0; i < n; ++i) {
        u8 *e = h + 0x10 + i * 0x98;
        swap_area(e + 4);
        swap_u32(e + 0x34);
    }
    return true;
}

// ---- SAR : zones de lumiere (LightAreaHed, src/game/light_area.cpp) : u32 nombre, entrees de 0xD8 des 0x10
bool conv_sar(const Span &s, u32 at, u32 end)
{
    track_label("SAR");
    (void) end;
    if (!s.has(at, 0x10))
        return false;
    u8 *h = s.base + at;
    const u32 n = read_be32(h);
    if (n > 0x200 || !s.has(at + 0x10, n * 0xD8))
        return false;
    swap_u32(h);
    for (u32 i = 0; i < n; ++i)
        swap_area(h + 0x10 + i * 0xD8 + 4);
    return true;
}

// ---- FSE : attributs de sol sonores (FlrAtHead + FlrAt de 0x84, include/flr_at.h)
bool conv_fse(const Span &s, u32 at, u32 end)
{
    track_label("FSE");
    (void) end;
    if (!s.has(at, 0x10))
        return false;
    u8 *h = s.base + at;
    const u32 n = (u32) ((h[6] << 8) | h[7]);
    if (n > 0x400 || !s.has(at + 0x10, n * 0x84))
        return false;
    swap_u16(h + 4);
    swap_u16(h + 6);
    for (u32 i = 0; i < n; ++i) {
        u8 *e = h + 0x10 + i * 0x84;
        swap_area(e + 0x14);
        swap_u32(e + 0x48);
        swap_u32(e + 0x4C);
        swap_u16(e + 0x50);
        swap_u16(e + 0x52);
        swap_u32(e + 0x54);
    }
    return true;
}

// ---- ESE : sons de zone (SeAtHead + SeAt de 0x2C, include/snd.h)
bool conv_ese(const Span &s, u32 at, u32 end)
{
    track_label("ESE");
    (void) end;
    if (!s.has(at, 0x10))
        return false;
    u8 *h = s.base + at;
    const u32 n = (u32) ((h[6] << 8) | h[7]);
    if (n > 0x400 || !s.has(at + 0x10, n * 0x2C))
        return false;
    swap_u16(h + 4);
    swap_u16(h + 6);
    for (u32 i = 0; i < n; ++i) {
        u8 *e = h + 0x10 + i * 0x2C;
        swap_u16(e + 2);
        for (int k = 0; k < 3; ++k)
            swap_u32(e + 4 + 4 * k);
        for (int k = 0; k < 10; ++k)
            swap_u16(e + 0x10 + 2 * k);
    }
    return true;
}

// ---- MDT : textes de la salle (table de messages, memes regles que core.das)
bool conv_mdt(const Span &s, u32 at, u32 end)
{
    if (end > s.size)
        end = s.size;
    return swap_mes_tbl(s, at, end);
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
            else if (kind == 2)
                r = conv_motion(s, at + ofs + o, next_start(at + ofs + o));
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

// ---- EVD : fichier de cinematique (include/event.h EvtHeader, « event ») et EVS (evenements de salle = EVD embarques).
// En-tete : mots a 0x34..0x4C ; flux de paquets (EvtPacket : id, drapeau, cut/frame/taille sur 16 bits, puis selon
// l'id : noms en octets, pos = 7 mots, val = 3 mots, esp.type = 1 mot) ; table des fichiers embarques (nom 0x30 +
// decalage) convertis chacun par son convertisseur (bin, tpl, fcv, lit, eff, mdt).
bool conv_evd(const Span &s, u32 at, u32 end)
{
    track_label("EVD");
    if (end > s.size)
        end = s.size;
    if (!s.has(at, 0x50) || std::memcmp(s.base + at, "event", 5) != 0)
        return false;
    u8 *h = s.base + at;
    const u32 pac = read_be32(h + 0x40), psz = read_be32(h + 0x44), nb = read_be32(h + 0x48), bo = read_be32(h + 0x4C);
    if (pac >= end - at || psz > end - at || nb > 0x400 || bo >= end - at)
        return false;                                   // deja converti (ou pas un EVD)
    for (u32 o = 0x34; o <= 0x4C; o += 4)
        swap_u32(h + o);
    // paquets
    for (u32 o = at + pac; o + 0x10 <= at + pac + psz;) {
        u8 *q = s.base + o;
        const s32 id = (s32) read_be32(q);
        const s16 size = (s16) ((q[0x0C] << 8) | q[0x0D]);
        if (size < 0x10 || o + (u32) size > end)
            break;
        swap_u32(q);
        swap_u32(q + 4);
        swap_u16(q + 8);
        swap_u16(q + 0x0A);
        swap_u16(q + 0x0C);
        swap_u16(q + 0x0E);
        switch (id) {
        case 9:                                         // Pos, PosPl : pos[3], rot[3], partsNo
        case 10:
            if (size >= 0x44)
                for (u32 k = 0x28; k < 0x44; k += 4)
                    swap_u32(q + k);
            break;
        case 13:                                        // Esp : type (parts = octet 0x23, laisse tel quel)
            if (size >= 0x20)
                swap_u32(q + 0x1C);
            break;
        case 15: case 16: case 17: case 18: case 29:    // Str, Se, Mes, Func, Fade : val.no, arg, time
            if (size >= 0x1C)
                for (u32 k = 0x10; k < 0x1C; k += 4)
                    swap_u32(q + k);
            break;
        default:
            break;
        }
        o += (u32) size;
    }
    // fichiers embarques
    u32 offs[0x400];
    for (u32 i = 0; i < nb; ++i) {
        u8 *e = s.base + at + bo + i * 0x40;
        if (!s.has(at + bo + i * 0x40, 0x40))
            return false;
        swap_u32(e + 0x30);
        offs[i] = read_le32(e + 0x30);
    }
    bool ok = true;
    for (u32 i = 0; i < nb; ++i) {
        const char *name = (const char *) (s.base + at + bo + i * 0x40);
        const u32 dat = at + offs[i];
        u32 dend = end;
        for (u32 j = 0; j < nb; ++j)
            if (offs[j] > offs[i] && at + offs[j] < dend)
                dend = at + offs[j];
        if (bo > offs[i] && at + bo < dend)
            dend = at + bo;
        const size_t ln = strnlen(name, 0x30);
        const char *x = ln > 4 ? name + ln - 4 : "";
        bool r = true;
        if (!std::strcmp(x, ".bin"))
            r = conv_model_bin(s, dat, dend);
        else if (!std::strcmp(x, ".tpl"))
            r = swap_tpl(s, dat);
        else if (!std::strcmp(x, ".fcv"))
            r = conv_motion(s, dat, dend);
        else if (!std::strcmp(x, ".lit"))
            r = conv_lit(s, dat, dend);
        else if (!std::strcmp(x, ".eff"))
            r = swap_eff_data(s, dat);
        else if (!std::strcmp(x, ".mdt"))
            r = conv_mdt(s, dat, dend);
        else {
            std::printf("[EVD] fichier '%.48s' sans convertisseur\n", name);
            r = false;
        }
        if (!r) {
            std::printf("[EVD] conversion de '%.48s' ratee\n", name);
            ok = false;
        }
    }
    return ok;
}

bool conv_evs(const Span &s, u32 at, u32 end)
{
    track_label("EVS");
    if (end > s.size)
        end = s.size;
    if (!s.has(at, 8))
        return false;
    u8 *h = s.base + at;
    const u32 num = read_be32(h), tbl = read_be32(h + 4);
    if (num > 0x100 || tbl >= end - at || !s.has(at + tbl, num * 8))
        return false;
    swap_u32(h);
    swap_u32(h + 4);
    u32 offs[0x100];
    for (u32 i = 0; i < num; ++i) {
        swap_u32(h + tbl + i * 8);
        swap_u32(h + tbl + i * 8 + 4);
        offs[i] = read_le32(h + tbl + i * 8);
    }
    bool ok = true;
    for (u32 i = 0; i < num; ++i) {
        u32 e = end;
        for (u32 j = 0; j < num; ++j)
            if (offs[j] > offs[i] && at + offs[j] < e)
                e = at + offs[j];
        if (!conv_evd(s, at + offs[i], e))
            ok = false;
    }
    return ok;
}

const TagConv kRoomTags[] = {
    { "TPL", conv_tpl },
    { "BIN", conv_model_bin },
    { "SMD", conv_smd },
    { "SMX", conv_smx },
    { "LIT", conv_lit },
    { "SHD", conv_shd },
    { "MDT", conv_mdt },
    { "EAR", conv_ear },
    { "SAR", conv_sar },
    { "BLK", conv_blk },
    { "AEV", conv_sceat },
    { "ITA", conv_sceat },
    { "CNS", conv_cns },
    { "EMI", conv_emi },
    { "FSE", conv_fse },
    { "ESE", conv_ese },
    { "FCV", conv_motion },
    { "RTP", conv_rtp },
    { "SAT", conv_sat },
    { "EAT", conv_sat },
    { "STB", conv_stb },
    { "DSE", conv_dse },
    { "CAM", conv_cam },
    { "ITM", conv_itm },
    { "ETM", conv_etm },
    { "ETS", conv_ets },
    { "TEX", conv_tex },
    { "EFF", conv_eff_tag },
    { "EVS", conv_evs },
    { "SEQ", conv_seq_tag },
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
    if (trace)
        std::printf("[ARC] archive de salle a %p (%u octets)\n", arc, (unsigned) size);
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
        if (trace && !done && s.has(at, 48)) {
            std::printf("[ARC]   octets :");
            for (int k = 0; k < 48; ++k)
                std::printf(" %02x", s.base[at + k]);
            std::printf("\n");
        }
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

// ---------------------------------------------------------------------------------------------------------------------
// Son (phase C du plan natif). Formats lus par le pilote de Capcom (src/game/snd*.cpp) et la bibliotheque SYN du SDK.
// Les echantillons DSP-ADPCM restent gros-boutistes : ils vivent en ARAM et seul le « DSP » (interpreteur AX) les lit.
namespace {
void snd_swap32n(u8 *p, u32 n) { for (u32 i = 0; i < n; ++i) *(u32 *) (p + 4 * i) = bswap32(*(u32 *) (p + 4 * i)); }
void snd_swap16n(u8 *p, u32 n) { for (u32 i = 0; i < n; ++i) *(u16 *) (p + 2 * i) = bswap16(*(u16 *) (p + 2 * i)); }

// bgm/bio4str.hed : deux blocs de flux (SND_STR_BLK : nombre, table des en-tetes SHD, table RIT).
void snd_fix_str_hed(const Span &s)
{
    if (!s.has(0, 8))
        return;
    snd_swap32n(s.base, 2);
    for (int b = 0; b < 2; ++b) {
        const u32 f = read_le32(s.base + 4 * b);
        if (!s.has(f, 12))
            continue;
        snd_swap32n(s.base + f, 3);
        const u32 num = read_le32(s.base + f), shd = f + read_le32(s.base + f + 4), rit = f + read_le32(s.base + f + 8);
        if (s.has(rit, num * 0x10))
            for (u32 i = 0; i < num; ++i) {              // SND_RIT : shd_no s16, octets, flag u16 a 0x0C
                snd_swap16n(s.base + rit + i * 0x10, 1);
                snd_swap16n(s.base + rit + i * 0x10 + 0x0C, 1);
            }
        if (!s.has(shd, 4))
            continue;
        const u32 first = read_be32(s.base + shd);       // la table des decalages s'arrete au premier en-tete
        const u32 nshd = first / 4;
        if (!first || !s.has(shd, first))
            continue;
        snd_swap32n(s.base + shd, nshd);
        for (u32 i = 0; i < nshd; ++i) {
            const u32 o = read_le32(s.base + shd + 4 * i);
            if (!o || !s.has(shd + o, 0x80))
                continue;
            snd_swap32n(s.base + shd + o, 8);            // SND_SHD : 8 mots puis demi-mots (coefficients, etat ADPCM)
            snd_swap16n(s.base + shd + o + 0x20, 0x30);
        }
    }
}

// bgm/doorse.hed : decalages (mots) jusqu'a la liste de demi-mots ; bgm/bgmtbl.dat : mots jusqu'a la liste des salles.
void snd_fix_words_then_halves(const Span &s)
{
    if (!s.has(0, 8))
        return;
    const u32 half = read_be32(s.base + 4);
    if (half > s.size || (half & 3))
        return;
    snd_swap32n(s.base, half / 4);
    snd_swap16n(s.base + half, (s.size - half) / 2);
}
}  // namespace

// Banque de sons d'un bloc (SndBlkInit, src/game/snd.cpp) : appele une fois par chargement, avant Snd_iss_blk_init.
// Blocs autres que 3/4 : en-tete externe { decalage du bloc ISS, decalage de la table des sons aleatoires }.
// Bloc ISS : { nombre de SIT, table d'ondes (SND_WT_HDR + tables SYN), SIT[], table des sequences } ; sequences = octets.
extern "C" void port_snd_bank_fix(u8 *mram, int blk)
{
    const u32 kMax = 0x400000;
    if (!mram)
        return;
    u8 *iss = mram;
    u32 rnd = 0;
    if (blk != 3 && blk != 4) {
        snd_swap32n(mram, 2);
        if (read_le32(mram) >= kMax)
            return;
        iss = mram + read_le32(mram);
        rnd = read_le32(mram + 4);
    }
    snd_swap32n(iss, 4);
    const u32 num = read_le32(iss), dls_o = read_le32(iss + 4), sit_o = read_le32(iss + 8), seq_o = read_le32(iss + 12);
    if (num > 0x1000 || dls_o >= kMax || sit_o >= kMax || seq_o >= kMax) {
        std::printf("[SND] banque du bloc %d inattendue (%u SIT)\n", blk, (unsigned) num);
        return;
    }
    // SIT (0x18 octets)
    int max_rnd = 0;
    for (u32 i = 0; i < num; ++i) {
        u8 *t = iss + sit_o + i * 0x18;
        snd_swap16n(t, 1);
        snd_swap16n(t + 0x0A, 2);
        snd_swap16n(t + 0x16, 1);
        if ((s8) t[0x13] > max_rnd)
            max_rnd = (s8) t[0x13];
    }
    // table des sequences : nombre puis un decalage par banque
    {
        u8 *q = iss + seq_o;
        const u32 n = bswap32(*(u32 *) q);
        if (n < 0x400)
            snd_swap32n(q, n + 1);
    }
    // table d'ondes
    u8 *wt = iss + dls_o;
    snd_swap32n(wt, 6);
    const u32 perc = read_le32(wt), mel = read_le32(wt + 4), rgn = read_le32(wt + 8), art = read_le32(wt + 12),
              smp = read_le32(wt + 16), adp = read_le32(wt + 20);
    const u32 inst = perc < mel ? perc : mel;
    if (inst <= rgn && rgn <= art && art <= smp && smp <= adp && adp < kMax) {
        snd_swap16n(wt + inst, (rgn - inst) / 2);                   // WTINST : demi-mots
        for (u32 o = rgn; o + 0x18 <= art; o += 0x18) {             // WTREGION
            snd_swap16n(wt + o + 2, 1);
            snd_swap32n(wt + o + 4, 5);
        }
        snd_swap32n(wt + art, (smp - art) / 4);                     // WTART : mots
        u32 nadp = 0;
        for (u32 o = smp; o + 0x10 <= adp; o += 0x10) {             // WTSAMPLE
            snd_swap16n(wt + o, 2);
            snd_swap32n(wt + o + 4, 2);
            snd_swap16n(wt + o + 0x0C, 1);
            const u32 k = *(u16 *) (wt + o + 0x0C) + 1u;
            if (k > nadp && k < 0x1000)
                nadp = k;
        }
        snd_swap16n(wt + adp, nadp * 0x2E / 2);                     // WTADPCM : demi-mots
    } else {
        std::printf("[SND] table d'ondes du bloc %d inattendue\n", blk);
    }
    // sons aleatoires (seRandomCheck) : table de decalages indexee par SIT.rnd_no -> { num, last, e[num] }
    if (rnd && rnd < kMax && max_rnd > 0) {
        u8 *tbl = mram + rnd;
        snd_swap32n(tbl, (u32) max_rnd + 1);
        u32 done[128];
        int ndone = 0;
        for (int g = 1; g <= max_rnd; ++g) {
            const u32 o = read_le32(tbl + 4 * g);
            if (!o || o >= kMax)
                continue;
            bool seen = false;
            for (int k = 0; k < ndone; ++k)
                seen |= done[k] == o;
            if (seen)
                continue;
            if (ndone < 128)
                done[ndone++] = o;
            snd_swap16n(tbl + o, 2);
            const u32 n = *(u16 *) (tbl + o);
            if (n < 0x400)
                snd_swap16n(tbl + o + 4, n);
        }
    }
}
