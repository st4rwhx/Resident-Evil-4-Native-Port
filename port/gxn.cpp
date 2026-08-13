// -*- coding: utf-8 -*-
// GX natif, etape 1 (nj-work/docs/INVENTAIRE_GX_NATIF_2026-11-14.md) : etat type des familles Attributs, Geometrie et
// Transformation, tenu par notre code a chaque appel GX du jeu, sans registres ni octets.
//
// Double chemin pendant la mise au point : le lien remplace chaque fonction GX listee dans port/gxn_wrap.rsp par
// __wrap_GXxxx (ici), qui met a jour l'etat type puis appelle la bibliotheque de Nintendo (__real_GXxxx), laquelle
// continue d'alimenter le rendu actuel par le flux. A chaque soumission (fin de primitive, appel de liste), apres la
// lecture du flux par le rendu, l'etat type est compare a l'etat decode du flux (l'oracle, port/njgfx) : tout ecart est
// compte et les premiers sont journalises ([GXN]). NJ_GXN=1 active le tout ; sans lui, chaque enveloppe ne fait
// qu'appeler la bibliotheque.
//
// Les registres ne servent ici qu'a la comparaison (gxn_expect_* : ce que l'etat type signifie pour le materiel). Le
// rendu lira l'etat type directement a l'etape 6.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dolphin.h>

extern "C" int njgfx_oracle(const unsigned** cp, const unsigned** bp, const unsigned** xf, const unsigned char** xfok);
extern "C" unsigned port_gx_vtx_bytes(void);    // gx_sdk.cpp : octets ecrits par les fonctions de sommets (GXPosition...)

namespace {

// ---------------------------------------------------------------- etat type
struct AttrFmt {
    u8 cnt, type, frac;
};
struct TexGen {
    u8 func, src, normalize;
    u32 mtx, pt_mtx;
};
struct State {
    // Attributs
    u8 desc[GX_VA_MAX_ATTR];   // GXAttrType de chaque attribut (indices de matrices, position, couleurs, coordonnees)
    bool has_nrm, has_nbt;     // normales seules, ou normale + binormale + tangente
    u8 nrm_type;               // GXAttrType des normales
    AttrFmt fmt[GX_MAX_VTXFMT][GX_VA_TEX7 + 1];   // format de chaque attribut, par format de sommet
    bool nbt3[GX_MAX_VTXFMT];                     // NBT en trois indices
    u32 arr_base[16];          // tableaux (GX_VA_POS..GX_LIGHT_ARRAY) : adresse physique et pas
    u8 arr_stride[16];
    u8 n_texgens;
    TexGen tg[8];
    // Geometrie
    u8 cull;                   // GXCullMode
    u8 line_w, line_ofs, point_sz, point_ofs;
    u8 tofs_line[8], tofs_point[8];
    // Transformation : memoire des matrices adressee par les identifiants GX (GX_PNMTXn, GX_TEXMTXn = ligne * 4)
    float mtx[256];
    float nrm[96];
    float post[256];           // GX_PTTEXMTXn
    u8 mtx_set[256], nrm_set[96], post_set[256];
    u32 cur_mtx;
    float proj[6];
    u32 proj_type;
    float vp[6];               // gauche, haut (decalage de trame compris), largeur, hauteur, proche, loin
    float zscale, zoffset;
    u32 sc[4];                 // ciseaux : gauche, haut, largeur, hauteur
    s32 sc_ox, sc_oy;
    // dessin en cours
    u8 prim, vfmt;
    u16 nverts;
    unsigned begin_pos;
    void* begin_ra;            // appelant de GXBegin (diagnostic)
    bool in_begin;
    // versions par categorie (pour le rendu, etape 6)
    u32 ver_attr, ver_geom, ver_xf;
};
State S;

int g_on = -1;
inline bool on()
{
    if (g_on < 0) g_on = getenv("NJ_GXN") != nullptr;
    return g_on != 0;
}

// dernier appel GX vu (contexte des ecarts)
const char* g_last[4];
int g_last_i;
void close_prim();
// appele en tete de chaque enveloppe, avant la bibliotheque : le jeu n'appelle jamais GXEnd (macro vide sur GameCube),
// la primitive ouverte se termine donc au premier appel GX qui suit
inline void note(const char* f)
{
    if (S.in_begin) close_prim();
    g_last[g_last_i++ & 3] = f;
}

// ---------------------------------------------------------------- compteurs
struct Stats {
    u32 frames, submits, compares, mismatches, vtx_ok, vtx_bad, lists, list_bytes, list_idx, list_cp, list_xf, list_bp,
        list_unknown, get_bad, oracle_missing;
} st;
struct RegCount {
    u16 space, reg;
    u32 n;
};
RegCount g_regs[64];
int g_nregs, g_logged;
u32 g_shown = ~0u;
u8 g_list_seen[3][256];   // registres d'etat vus dans des listes (journalises une fois)

void count_reg(int space, unsigned reg)
{
    for (int i = 0; i < g_nregs; ++i)
        if (g_regs[i].space == space && g_regs[i].reg == reg) {
            ++g_regs[i].n;
            return;
        }
    if (g_nregs < 64) g_regs[g_nregs++] = {(u16) space, (u16) reg, 1};
}
const char* kSpace[] = {"CP", "XF", "BP", "MTX", "NRM", "POST", "GET"};

void mismatch(int space, unsigned reg, u32 mine, u32 theirs, const char* why)
{
    ++st.mismatches;
    count_reg(space, reg);
    if (g_logged < 60) {
        ++g_logged;
        OSReport("[GXN] ecart %s %03x : natif %08x flux %08x (%s) ; derniers appels %s %s %s %s ; image %u\n", kSpace[space],
                 reg, (unsigned) mine, (unsigned) theirs, why, g_last[(g_last_i - 1) & 3] ? g_last[(g_last_i - 1) & 3] : "-",
                 g_last[(g_last_i - 2) & 3] ? g_last[(g_last_i - 2) & 3] : "-",
                 g_last[(g_last_i - 3) & 3] ? g_last[(g_last_i - 3) & 3] : "-",
                 g_last[(g_last_i - 4) & 3] ? g_last[(g_last_i - 4) & 3] : "-", (unsigned) st.frames);
    }
}

inline u32 fbits(float f)
{
    u32 u;
    std::memcpy(&u, &f, 4);
    return u;
}

// ---------------------------------------------------------------- tailles de sommets (etat type)
inline int comp_size(u8 type) { return type == GX_U8 || type == GX_S8 ? 1 : type == GX_U16 || type == GX_S16 ? 2 : 4; }
inline int idx_size(u8 mode) { return mode == GX_INDEX8 ? 1 : 2; }
int vertex_size(int f)
{
    int n = 0;
    for (int a = GX_VA_PNMTXIDX; a <= GX_VA_TEX7MTXIDX; ++a)
        if (S.desc[a] != GX_NONE) n += 1;
    const AttrFmt* F = S.fmt[f];
    switch (S.desc[GX_VA_POS]) {
    case GX_DIRECT: n += (F[GX_VA_POS].cnt ? 3 : 2) * comp_size(F[GX_VA_POS].type); break;
    case GX_INDEX8: case GX_INDEX16: n += idx_size(S.desc[GX_VA_POS]); break;
    default: break;
    }
    if (S.has_nrm || S.has_nbt) {
        const bool nine = F[GX_VA_NRM].cnt != 0 || S.nbt3[f];
        if (S.nrm_type == GX_DIRECT) n += (nine ? 9 : 3) * comp_size(F[GX_VA_NRM].type);
        else if (S.nrm_type != GX_NONE) n += idx_size(S.nrm_type) * (S.nbt3[f] ? 3 : 1);
    }
    static const u8 clr_size[] = {2, 3, 4, 2, 3, 4, 4, 4};
    for (int a = GX_VA_CLR0; a <= GX_VA_CLR1; ++a) {
        if (S.desc[a] == GX_DIRECT) n += clr_size[F[a].type & 7];
        else if (S.desc[a] != GX_NONE) n += idx_size(S.desc[a]);
    }
    for (int a = GX_VA_TEX0; a <= GX_VA_TEX7; ++a) {
        if (S.desc[a] == GX_DIRECT) n += (F[a].cnt ? 2 : 1) * comp_size(F[a].type);
        else if (S.desc[a] != GX_NONE) n += idx_size(S.desc[a]);
    }
    return n;
}

// etape 2 (gxn_tev.inc, inclus a la fin)
void reset2();
void compare2(const unsigned* bp);
void control2(const unsigned* bp);
// etape 3 (gxn_light.inc)
void reset3();
void compare3(const unsigned* bp, const unsigned* xf, const unsigned char* ok);
void control3(const unsigned* xf);
void light_word(int i, int w, u32 bits);
// etape 4 (gxn_tex.inc)
void reset4();
void compare4(const unsigned* bp);
void control4(const unsigned* bp);
// etape 5 (gxn_ind.inc)
void reset5();
void compare5(const unsigned* bp);
void control5(const unsigned* bp);

// ---------------------------------------------------------------- operations natives
void reset()
{
    reset2();
    reset3();
    reset4();
    reset5();
    std::memset(&S, 0, sizeof S);
    S.desc[GX_VA_POS] = GX_DIRECT;
    S.zscale = 1.6777216E7f;
    S.zoffset = 0.0f;
}

void set_desc(GXAttr a, GXAttrType t)
{
    if (a == GX_VA_NRM) {
        if (t != GX_NONE) S.has_nrm = true, S.has_nbt = false, S.nrm_type = (u8) t;
        else S.has_nrm = false;
    } else if (a == GX_VA_NBT) {
        if (t != GX_NONE) S.has_nbt = true, S.has_nrm = false, S.nrm_type = (u8) t;
        else S.has_nbt = false;
    } else if (a < GX_VA_MAX_ATTR) {
        S.desc[a] = (u8) t;
    }
    ++S.ver_attr;
}

void set_fmt(int f, GXAttr a, GXCompCnt cnt, GXCompType type, u8 frac)
{
    if (a == GX_VA_NBT) a = GX_VA_NRM;
    if (f < 0 || f >= GX_MAX_VTXFMT || a < GX_VA_POS || a > GX_VA_TEX7) return;
    AttrFmt& F = S.fmt[f][a];
    F.type = (u8) type;
    if (a == GX_VA_NRM) {
        S.nbt3[f] = cnt == GX_NRM_NBT3;
        F.cnt = cnt == GX_NRM_NBT3 ? 1 : (u8) cnt;
    } else {
        F.cnt = (u8) cnt;
        if (a == GX_VA_POS || a >= GX_VA_TEX0) F.frac = frac;
    }
    ++S.ver_attr;
}

void load_mtx(float* mem, u8* set, int cap, unsigned addr, const float* src, int n)
{
    for (int k = 0; k < n && (int) addr + k < cap; ++k) {
        mem[addr + k] = src[k];
        set[addr + k] = 1;
    }
    ++S.ver_xf;
}

// Chargement par indice (GXLoad*MtxIndx, et commandes 0x20-0x38 des listes) : la matrice est lue dans le tableau du jeu
// (memoire dans l'ordre de la machine, comme le reste du coeur natif).
void load_indexed(int which, u32 reg)
{
    const unsigned addr = reg & 0xFFF, n = ((reg >> 12) & 0xF) + 1, idx = reg >> 16;
    const int arr = 12 + which;   // GX_POS_MTX_ARRAY - GX_VA_POS + which
    const u32 phys = S.arr_base[arr] + idx * S.arr_stride[arr];
    const float* src = (const float*) (0x80000000u | phys);
    if (addr < 0x100) load_mtx(S.mtx, S.mtx_set, 256, addr, src, (int) n);
    else if (addr >= 0x400 && addr < 0x460) load_mtx(S.nrm, S.nrm_set, 96, addr - 0x400, src, (int) n);
    else if (addr >= 0x500 && addr < 0x600) load_mtx(S.post, S.post_set, 256, addr - 0x500, src, (int) n);
    else if (addr >= 0x600 && addr < 0x680) {   // lumieres (etape 3)
        const u32* w = (const u32*) src;
        for (unsigned k = 0; k < n && addr + k < 0x680; ++k) light_word((addr + k - 0x600) >> 4, (addr + k) & 15, w[k]);
    }
}

inline u32 be32(const u8* p) { return ((u32) p[0] << 24) | ((u32) p[1] << 16) | ((u32) p[2] << 8) | p[3]; }
inline u16 be16(const u8* p) { return (u16) ((p[0] << 8) | p[1]); }

void list_state(int space, unsigned reg)
{
    if (reg < 256 && !g_list_seen[space][reg]) {
        g_list_seen[space][reg] = 1;
        OSReport("[GXN] etat dans une liste : %s %03x\n", space == 0 ? "CP" : space == 1 ? "XF" : "BP", reg);
    }
}

// Liste d'affichage (donnees des modeles, gros-boutistes) : les chargements de matrices sont appliques a l'etat type,
// les autres commandes d'etat comptees (converties en operations typees a l'etape 5), les dessins sautes avec la taille
// de sommet de l'etat type.
void scan_list(const u8* p, u32 n, int depth)
{
    ++st.lists;
    st.list_bytes += n;
    const u8* end = p + n;
    while (p < end) {
        const u8 op = *p;
        if (op == 0x00 || op == 0x48) {
            ++p;
        } else if (op == 0x08) {
            if (p + 6 > end) break;
            ++st.list_cp;
            list_state(0, p[1]);
            p += 6;
        } else if (op == 0x10) {
            if (p + 5 > end) break;
            const unsigned len = be16(p + 1) + 1u, addr = be16(p + 3);
            if (p + 5 + 4 * len > end) break;
            if (addr < 0x100 || (addr >= 0x400 && addr < 0x460) || (addr >= 0x500 && addr < 0x600)) {
                float tmp[16];
                for (unsigned k = 0; k < len && k < 16; ++k) {
                    const u32 w = be32(p + 5 + 4 * k);
                    std::memcpy(&tmp[k], &w, 4);
                }
                if (addr < 0x100) load_mtx(S.mtx, S.mtx_set, 256, addr, tmp, (int) len);
                else if (addr < 0x460) load_mtx(S.nrm, S.nrm_set, 96, addr - 0x400, tmp, (int) len);
                else load_mtx(S.post, S.post_set, 256, addr - 0x500, tmp, (int) len);
            } else {
                ++st.list_xf;
                list_state(1, addr >= 0x1000 ? addr - 0x1000 : 0xFF);
            }
            p += 5 + 4 * len;
        } else if (op == 0x20 || op == 0x28 || op == 0x30 || op == 0x38) {
            if (p + 5 > end) break;
            ++st.list_idx;
            load_indexed((op - 0x20) >> 3, be32(p + 1));
            p += 5;
        } else if (op == 0x40) {
            if (p + 9 > end) break;
            if (depth < 4) scan_list((const u8*) be32(p + 1), be32(p + 5), depth + 1);
            p += 9;
        } else if (op == 0x61) {
            if (p + 5 > end) break;
            ++st.list_bp;
            list_state(2, p[1]);
            p += 5;
        } else if (op >= 0x80 && op <= 0xBF) {
            if (p + 3 > end) break;
            p += 3 + (u32) be16(p + 1) * (u32) vertex_size(op & 7);
        } else {
            ++st.list_unknown;
            break;
        }
    }
}

// ---------------------------------------------------------------- ce que l'etat type signifie (comparaison seulement)
u32 expect_vcd_lo()
{
    u32 v = 0;
    for (int a = GX_VA_PNMTXIDX; a <= GX_VA_TEX7MTXIDX; ++a) v |= (u32) (S.desc[a] & 1) << a;
    v |= (u32) (S.desc[GX_VA_POS] & 3) << 9;
    v |= (u32) ((S.has_nrm || S.has_nbt) ? (S.nrm_type & 3) : 0) << 11;
    v |= (u32) (S.desc[GX_VA_CLR0] & 3) << 13;
    v |= (u32) (S.desc[GX_VA_CLR1] & 3) << 15;
    return v;
}
u32 expect_vcd_hi()
{
    u32 v = 0;
    for (int t = 0; t < 8; ++t) v |= (u32) (S.desc[GX_VA_TEX0 + t] & 3) << (2 * t);
    return v;
}
void expect_vat(int f, u32& a, u32& b, u32& c)
{
    const AttrFmt* F = S.fmt[f];
    a = (F[GX_VA_POS].cnt & 1) | (u32) (F[GX_VA_POS].type & 7) << 1 | (u32) (F[GX_VA_POS].frac & 31) << 4 |
        (u32) (F[GX_VA_NRM].cnt & 1) << 9 | (u32) (F[GX_VA_NRM].type & 7) << 10 | (u32) (F[GX_VA_CLR0].cnt & 1) << 13 |
        (u32) (F[GX_VA_CLR0].type & 7) << 14 | (u32) (F[GX_VA_CLR1].cnt & 1) << 17 | (u32) (F[GX_VA_CLR1].type & 7) << 18 |
        (u32) (F[GX_VA_TEX0].cnt & 1) << 21 | (u32) (F[GX_VA_TEX0].type & 7) << 22 | (u32) (F[GX_VA_TEX0].frac & 31) << 25 |
        1u << 30 | (u32) S.nbt3[f] << 31;
    b = 1u << 31;
    for (int t = 1; t <= 3; ++t) {
        const AttrFmt& T = F[GX_VA_TEX0 + t];
        const int lo = 9 * (t - 1);
        b |= (u32) (T.cnt & 1) << lo | (u32) (T.type & 7) << (lo + 1) | (u32) (T.frac & 31) << (lo + 4);
    }
    b |= (u32) (F[GX_VA_TEX4].cnt & 1) << 27 | (u32) (F[GX_VA_TEX4].type & 7) << 28;
    c = F[GX_VA_TEX4].frac & 31;
    for (int t = 5; t <= 7; ++t) {
        const AttrFmt& T = F[GX_VA_TEX0 + t];
        const int lo = 5 + 9 * (t - 5);
        c |= (u32) (T.cnt & 1) << lo | (u32) (T.type & 7) << (lo + 1) | (u32) (T.frac & 31) << (lo + 4);
    }
}
u32 expect_matidx_a()
{
    return (S.cur_mtx & 63) | (S.tg[0].mtx & 63) << 6 | (S.tg[1].mtx & 63) << 12 | (S.tg[2].mtx & 63) << 18 |
           (S.tg[3].mtx & 63) << 24;
}
u32 expect_matidx_b()
{
    return (S.tg[4].mtx & 63) | (S.tg[5].mtx & 63) << 6 | (S.tg[6].mtx & 63) << 12 | (S.tg[7].mtx & 63) << 18;
}
u32 expect_texgen(int i)
{
    const TexGen& g = S.tg[i];
    u32 row = 5, form = 0, reg = 0;
    switch (g.src) {
    case GX_TG_POS: row = 0; form = 1; break;
    case GX_TG_NRM: row = 1; form = 1; break;
    case GX_TG_BINRM: row = 3; form = 1; break;
    case GX_TG_TANGENT: row = 4; form = 1; break;
    case GX_TG_COLOR0: case GX_TG_COLOR1: row = 2; break;
    default:
        if (g.src >= GX_TG_TEX0 && g.src <= GX_TG_TEX7) row = 5 + (g.src - GX_TG_TEX0);
        break;
    }
    if (g.func == GX_TG_MTX2x4) reg = form << 2 | row << 7;
    else if (g.func == GX_TG_MTX3x4) reg = 1u << 1 | form << 2 | row << 7;
    else if (g.func >= GX_TG_BUMP0 && g.func <= GX_TG_BUMP7)
        reg = form << 2 | 1u << 4 | row << 7 | (u32) ((g.src - 12) & 7) << 12 | (u32) ((g.func - 2) & 7) << 15;
    else if (g.func == GX_TG_SRTG) reg = form << 2 | (g.src == GX_TG_COLOR0 ? 2u : 3u) << 4 | 2u << 7;
    return reg;
}
u32 expect_post(int i) { return ((S.tg[i].pt_mtx - 64) & 63) | (u32) (S.tg[i].normalize & 1) << 8; }
u32 expect_xf_specs()
{
    const u32 cols = (S.desc[GX_VA_CLR0] ? 1 : 0) + (S.desc[GX_VA_CLR1] ? 1 : 0);
    const u32 nrm = S.has_nbt ? 2 : S.has_nrm ? 1 : 0;
    u32 tex = 0;
    for (int t = 0; t < 8; ++t) tex += S.desc[GX_VA_TEX0 + t] ? 1 : 0;
    return cols | nrm << 2 | tex << 4;
}
void expect_viewport(float out[6])
{
    // meme calcul, dans le meme ordre, que la bibliotheque (-ffp-contract=off des deux cotes)
    const float sx = S.vp[2] / 2.0f, sy = -S.vp[3] / 2.0f;
    const float ox = 342.0f + (S.vp[0] + (S.vp[2] / 2.0f)), oy = 342.0f + (S.vp[1] + (S.vp[3] / 2.0f));
    const float zmin = S.vp[4] * S.zscale, zmax = S.vp[5] * S.zscale;
    out[0] = sx; out[1] = sy; out[2] = zmax - zmin; out[3] = ox; out[4] = oy; out[5] = zmax + S.zoffset;
}

// ---------------------------------------------------------------- comparaison a chaque soumission
inline void cmp(int space, unsigned reg, u32 mine, u32 theirs, u32 mask, const char* why)
{
    if ((mine ^ theirs) & mask) mismatch(space, reg, mine & mask, theirs & mask, why);
}

void cmp_mem(int space, const float* mem, const u8* set, int n, unsigned base, const unsigned* xf, const unsigned char* ok)
{
    for (int i = 0; i < n; ++i) {
        const unsigned a = base + i;
        if (set[i] && ok[a]) cmp(space, i, fbits(mem[i]), xf[a], ~0u, "valeur");
        else if (set[i] != ok[a]) mismatch(space, i, set[i], ok[a], set[i] ? "connue du natif seul" : "connue du flux seul");
    }
}

void compare()
{
    const unsigned *cp, *bp, *xf;
    const unsigned char* ok;
    if (!njgfx_oracle(&cp, &bp, &xf, &ok)) {
        ++st.oracle_missing;
        return;
    }
    ++st.compares;
    if (st.compares == 1 || (st.frames % 1200 == 0 && st.frames != g_shown)) {
        g_shown = st.frames;
        unsigned known = 0, mine = 0;
        for (int i = 0; i < 256; ++i) known += ok[i], mine += S.mtx_set[i];
        u32 a, b, c;
        expect_vat(S.vfmt, a, b, c);
        OSReport("[GXN] controle : VCD natif %05x flux %05x ; format %u A natif %08x flux %08x ; XF 1008 natif %02x flux %02x ;"
                 " cases de matrices connues natif %u flux %u ; projection[0] natif %08x flux %08x\n",
                 (unsigned) expect_vcd_lo(), cp[0x50] & 0x1FFFF, (unsigned) S.vfmt, (unsigned) a, cp[0x70 + S.vfmt],
                 (unsigned) expect_xf_specs(), xf[0x1008], mine, known, (unsigned) fbits(S.proj[0]), xf[0x1020]);
        control2(bp);   // etape 2 : valeurs TEV et pixels
        control3(xf);   // etape 3 : canaux et lumieres
        control4(bp);   // etape 4 : textures
        control5(bp);   // etape 5 : indirect et copies
    }
    // Attributs (CP)
    cmp(0, 0x50, expect_vcd_lo(), cp[0x50], 0x1FFFF, "descripteur bas");
    cmp(0, 0x60, expect_vcd_hi(), cp[0x60], 0xFFFF, "descripteur haut");
    if (S.vfmt < GX_MAX_VTXFMT) {   // format du dernier dessin : la bibliotheque n'envoie que les formats modifies
        u32 a, b, c;
        expect_vat(S.vfmt, a, b, c);
        cmp(0, 0x70 + S.vfmt, a, cp[0x70 + S.vfmt], ~0u, "format A");
        cmp(0, 0x80 + S.vfmt, b, cp[0x80 + S.vfmt], ~0u, "format B");
        cmp(0, 0x90 + S.vfmt, c, cp[0x90 + S.vfmt], ~0u, "format C");
    }
    for (int i = 0; i < 16; ++i) {
        if (!S.arr_base[i] && !cp[0xA0 + i]) continue;
        cmp(0, 0xA0 + i, S.arr_base[i], cp[0xA0 + i], 0x3FFFFFFF, "tableau");
        cmp(0, 0xB0 + i, S.arr_stride[i], cp[0xB0 + i], 0xFF, "pas");
    }
    cmp(0, 0x30, expect_matidx_a(), cp[0x30], 0x3FFFFFFF, "indices de matrices A");
    cmp(0, 0x40, expect_matidx_b(), cp[0x40], 0xFFFFFF, "indices de matrices B");
    // XF : specifications de sommets, indices, generateurs, vue, projection
    struct X {
        unsigned a;
        u32 v;
        const char* why;
    } xs[48];
    int nx = 0;
    xs[nx++] = {0x08, expect_xf_specs(), "nombre de couleurs/normales/coordonnees"};
    xs[nx++] = {0x18, expect_matidx_a(), "indices de matrices A"};
    xs[nx++] = {0x19, expect_matidx_b(), "indices de matrices B"};
    xs[nx++] = {0x3F, S.n_texgens, "nombre de generateurs"};
    for (int i = 0; i < 8; ++i) {
        xs[nx++] = {0x40u + i, expect_texgen(i), "generateur"};
        xs[nx++] = {0x50u + i, expect_post(i), "post-matrice"};
    }
    float vp[6];
    expect_viewport(vp);
    for (int i = 0; i < 6; ++i) xs[nx++] = {0x1Au + i, fbits(vp[i]), "vue"};
    for (int i = 0; i < 6; ++i) xs[nx++] = {0x20u + i, fbits(S.proj[i]), "projection"};
    xs[nx++] = {0x26, S.proj_type, "type de projection"};
    for (int i = 0; i < nx; ++i) {
        const unsigned a = 0x1000 + xs[i].a;
        if (ok[a]) cmp(1, xs[i].a, xs[i].v, xf[a], ~0u, xs[i].why);
    }
    // matrices
    cmp_mem(3, S.mtx, S.mtx_set, 256, 0x000, xf, ok);
    cmp_mem(4, S.nrm, S.nrm_set, 96, 0x400, xf, ok);
    cmp_mem(5, S.post, S.post_set, 256, 0x500, xf, ok);
    // BP : mode general (generateurs, elimination), ciseaux, lignes/points
    static const u8 hw_cull[] = {GX_CULL_NONE, GX_CULL_BACK, GX_CULL_FRONT, GX_CULL_ALL};
    cmp(2, 0x00, (u32) S.n_texgens | (u32) hw_cull[S.cull & 3] << 14, bp[0x00], 0xF | 3u << 14, "mode general");
    const u32 tp = S.sc[1] + 342, lf = S.sc[0] + 342;
    cmp(2, 0x20, tp | lf << 12, bp[0x20], 0x7FF7FF, "ciseaux haut/gauche");
    cmp(2, 0x21, (tp + S.sc[3] - 1) | (lf + S.sc[2] - 1) << 12, bp[0x21], 0x7FF7FF, "ciseaux bas/droite");
    cmp(2, 0x59, (((u32) (S.sc_ox + 342) >> 1) & 0x3FF) | (((u32) (S.sc_oy + 342) >> 1) & 0x3FF) << 10, bp[0x59], 0xFFFFF,
        "decalage des ciseaux");
    cmp(2, 0x22, (u32) S.line_w | (u32) S.point_sz << 8 | (u32) (S.line_ofs & 7) << 16 | (u32) (S.point_ofs & 7) << 19,
        bp[0x22], 0x3FFFFF, "lignes/points");
    for (int i = 0; i < 8; ++i)
        cmp(2, 0x30 + 2 * i, (u32) (S.tofs_line[i] & 1) << 18 | (u32) (S.tofs_point[i] & 1) << 19, bp[0x30 + 2 * i],
            3u << 18, "decalages de coordonnees");
    compare2(bp);
    compare3(bp, xf, ok);
    compare4(bp);
    compare5(bp);
}

// octets envoyes depuis GXBegin = sommets x taille de sommet de l'etat type
void close_prim()
{
    S.in_begin = false;
    const unsigned got = port_gx_vtx_bytes() - S.begin_pos, want = (unsigned) S.nverts * (unsigned) vertex_size(S.vfmt);
    if (got == want) {
        ++st.vtx_ok;
    } else {
        ++st.vtx_bad;
        if (st.vtx_bad <= 8)
            OSReport("[GXN] primitive fausse : type %02x format %u, %u sommets, appelant %p\n", (unsigned) S.prim,
                     (unsigned) S.vfmt, (unsigned) S.nverts, S.begin_ra);
        mismatch(6, 0x100 + S.vfmt, want, got, "octets ecrits par les fonctions de sommets");
    }
}

}  // namespace

// ---------------------------------------------------------------- crochets du portage (gx_sdk.cpp, gx_sdk_os.cpp)
extern "C" {

// fin de primitive explicite (GXEnd du portage, rarement appele)
void gxn_end(void)
{
    if (on() && S.in_begin) close_prim();
}

// apres la lecture du flux par le rendu (port_gx_submit)
void gxn_submitted(void)
{
    if (!on()) return;
    ++st.submits;
    if (S.in_begin) close_prim();
    compare();
}

void gxn_frame(void)
{
    if (!on()) return;
    if (S.in_begin) close_prim();
    if (++st.frames % 120) return;
    OSReport("[GXN] %u images : %u soumissions, %u comparaisons, %u ecarts ; sommets %u justes %u faux ; listes %u (%u Ko) : "
             "%u chargements par indice, etat CP %u XF %u BP %u, inconnus %u ; oracle absent %u\n",
             (unsigned) st.frames, (unsigned) st.submits, (unsigned) st.compares, (unsigned) st.mismatches,
             (unsigned) st.vtx_ok, (unsigned) st.vtx_bad, (unsigned) st.lists, (unsigned) (st.list_bytes >> 10),
             (unsigned) st.list_idx, (unsigned) st.list_cp, (unsigned) st.list_xf, (unsigned) st.list_bp,
             (unsigned) st.list_unknown, (unsigned) st.oracle_missing);
    if (g_nregs) {
        char line[512];
        int n = 0;
        for (int i = 0; i < g_nregs && n < 480; ++i)
            n += snprintf(line + n, sizeof line - n, " %s%03x:%u", kSpace[g_regs[i].space], g_regs[i].reg,
                          (unsigned) g_regs[i].n);
        OSReport("[GXN] ecarts par registre :%s\n", line);
    }
}

// ---------------------------------------------------------------- enveloppes (-Wl,--wrap, port/gxn_wrap.rsp)
#define REAL(ret, name, args) ret __real_##name args;
REAL(GXFifoObj*, GXInit, (void*, u32))
REAL(void, GXSetVtxDesc, (GXAttr, GXAttrType))
REAL(void, GXSetVtxDescv, (const GXVtxDescList*))
REAL(void, GXClearVtxDesc, (void))
REAL(void, GXSetVtxAttrFmt, (GXVtxFmt, GXAttr, GXCompCnt, GXCompType, u8))
REAL(void, GXSetVtxAttrFmtv, (GXVtxFmt, const GXVtxAttrFmtList*))
REAL(void, GXSetArray, (GXAttr, void*, u8))
REAL(void, GXSetNumTexGens, (u8))
REAL(void, GXSetTexCoordGen2, (GXTexCoordID, GXTexGenType, GXTexGenSrc, u32, GXBool, u32))
REAL(void, GXBegin, (GXPrimitive, GXVtxFmt, u16))
REAL(void, GXSetCullMode, (GXCullMode))
REAL(void, GXSetLineWidth, (u8, GXTexOffset))
REAL(void, GXSetPointSize, (u8, GXTexOffset))
REAL(void, GXEnableTexOffsets, (GXTexCoordID, u8, u8))
REAL(void, GXLoadPosMtxImm, (const f32[3][4], u32))
REAL(void, GXLoadNrmMtxImm, (const f32[3][4], u32))
REAL(void, GXLoadNrmMtxImm3x3, (const f32[3][3], u32))
REAL(void, GXLoadTexMtxImm, (const f32[][4], u32, GXTexMtxType))
REAL(void, GXLoadPosMtxIndx, (u16, u32))
REAL(void, GXLoadNrmMtxIndx3x3, (u16, u32))
REAL(void, GXLoadTexMtxIndx, (u16, u32, GXTexMtxType))
REAL(void, GXSetCurrentMtx, (u32))
REAL(void, GXSetProjection, (const f32[4][4], GXProjectionType))
REAL(void, GXSetProjectionv, (const f32*))
REAL(void, GXGetProjectionv, (f32*))
REAL(void, GXSetViewport, (f32, f32, f32, f32, f32, f32))
REAL(void, GXSetViewportJitter, (f32, f32, f32, f32, f32, f32, u32))
REAL(void, GXGetViewportv, (f32*))
REAL(void, GXSetZScaleOffset, (f32, f32))
REAL(void, GXSetScissor, (u32, u32, u32, u32))
REAL(void, GXSetScissorBoxOffset, (s32, s32))
REAL(void, GXCallDisplayList, (void*, u32))

GXFifoObj* __wrap_GXInit(void* base, u32 size)
{
    if (on()) reset(), note("GXInit");
    return __real_GXInit(base, size);
}

void __wrap_GXSetVtxDesc(GXAttr attr, GXAttrType type)
{
    if (on()) set_desc(attr, type), note("GXSetVtxDesc");
    __real_GXSetVtxDesc(attr, type);
}
void __wrap_GXSetVtxDescv(const GXVtxDescList* l)
{
    if (on()) {
        for (const GXVtxDescList* p = l; p->attr != GX_VA_NULL; ++p) set_desc(p->attr, p->type);
        note("GXSetVtxDescv");
    }
    __real_GXSetVtxDescv(l);
}
void __wrap_GXClearVtxDesc(void)
{
    if (on()) {
        std::memset(S.desc, 0, sizeof S.desc);
        S.desc[GX_VA_POS] = GX_DIRECT;
        S.has_nrm = S.has_nbt = false;
        ++S.ver_attr;
        note("GXClearVtxDesc");
    }
    __real_GXClearVtxDesc();
}
void __wrap_GXSetVtxAttrFmt(GXVtxFmt f, GXAttr a, GXCompCnt cnt, GXCompType type, u8 frac)
{
    if (on()) set_fmt(f, a, cnt, type, frac), note("GXSetVtxAttrFmt");
    __real_GXSetVtxAttrFmt(f, a, cnt, type, frac);
}
void __wrap_GXSetVtxAttrFmtv(GXVtxFmt f, const GXVtxAttrFmtList* l)
{
    if (on()) {
        for (const GXVtxAttrFmtList* p = l; p->attr != GX_VA_NULL; ++p) set_fmt(f, p->attr, p->cnt, p->type, p->frac);
        note("GXSetVtxAttrFmtv");
    }
    __real_GXSetVtxAttrFmtv(f, l);
}
void __wrap_GXSetArray(GXAttr attr, void* base, u8 stride)
{
    if (on()) {
        const int a = (attr == GX_VA_NBT ? GX_VA_NRM : attr) - GX_VA_POS;
        if (a >= 0 && a < 16) S.arr_base[a] = (u32) base & 0x3FFFFFFF, S.arr_stride[a] = stride;
        ++S.ver_attr;
        note("GXSetArray");
    }
    __real_GXSetArray(attr, base, stride);
}
void __wrap_GXSetNumTexGens(u8 n)
{
    if (on()) S.n_texgens = n, ++S.ver_attr, note("GXSetNumTexGens");
    __real_GXSetNumTexGens(n);
}
void __wrap_GXSetTexCoordGen2(GXTexCoordID dst, GXTexGenType func, GXTexGenSrc src, u32 mtx, GXBool norm, u32 pt)
{
    if (on() && dst < 8) {
        S.tg[dst] = {(u8) func, (u8) src, (u8) norm, mtx, pt};
        ++S.ver_attr;
        note("GXSetTexCoordGen2");
    }
    __real_GXSetTexCoordGen2(dst, func, src, mtx, norm, pt);
}

void __wrap_GXBegin(GXPrimitive type, GXVtxFmt fmt, u16 n)
{
    if (on()) note("GXBegin");   // ferme la primitive precedente avant que la bibliotheque n'ecrive l'en-tete
    __real_GXBegin(type, fmt, n);
    if (on()) {
        S.prim = (u8) type, S.vfmt = (u8) fmt, S.nverts = n;
        S.begin_pos = port_gx_vtx_bytes();
        S.begin_ra = __builtin_return_address(0);
        S.in_begin = true;
    }
}
void __wrap_GXSetCullMode(GXCullMode m)
{
    if (on()) S.cull = (u8) m, ++S.ver_geom, note("GXSetCullMode");
    __real_GXSetCullMode(m);
}
void __wrap_GXSetLineWidth(u8 w, GXTexOffset o)
{
    if (on()) S.line_w = w, S.line_ofs = (u8) o, ++S.ver_geom, note("GXSetLineWidth");
    __real_GXSetLineWidth(w, o);
}
void __wrap_GXSetPointSize(u8 s, GXTexOffset o)
{
    if (on()) S.point_sz = s, S.point_ofs = (u8) o, ++S.ver_geom, note("GXSetPointSize");
    __real_GXSetPointSize(s, o);
}
void __wrap_GXEnableTexOffsets(GXTexCoordID c, u8 line, u8 point)
{
    if (on() && c < 8) S.tofs_line[c] = line, S.tofs_point[c] = point, ++S.ver_geom, note("GXEnableTexOffsets");
    __real_GXEnableTexOffsets(c, line, point);
}

void __wrap_GXLoadPosMtxImm(const f32 m[3][4], u32 id)
{
    if (on()) load_mtx(S.mtx, S.mtx_set, 256, id * 4, &m[0][0], 12), note("GXLoadPosMtxImm");
    __real_GXLoadPosMtxImm(m, id);
}
void __wrap_GXLoadNrmMtxImm(const f32 m[3][4], u32 id)
{
    if (on()) {
        const float n[9] = {m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2]};
        load_mtx(S.nrm, S.nrm_set, 96, id * 3, n, 9);
        note("GXLoadNrmMtxImm");
    }
    __real_GXLoadNrmMtxImm(m, id);
}
void __wrap_GXLoadNrmMtxImm3x3(const f32 m[3][3], u32 id)
{
    if (on()) load_mtx(S.nrm, S.nrm_set, 96, id * 3, &m[0][0], 9), note("GXLoadNrmMtxImm3x3");
    __real_GXLoadNrmMtxImm3x3(m, id);
}
void __wrap_GXLoadTexMtxImm(const f32 m[][4], u32 id, GXTexMtxType type)
{
    if (on()) {
        const int n = type == GX_MTX2x4 ? 8 : 12;
        if (id >= GX_PTTEXMTX0) load_mtx(S.post, S.post_set, 256, (id - GX_PTTEXMTX0) * 4, &m[0][0], n);
        else load_mtx(S.mtx, S.mtx_set, 256, id * 4, &m[0][0], n);
        note("GXLoadTexMtxImm");
    }
    __real_GXLoadTexMtxImm(m, id, type);
}
void __wrap_GXLoadPosMtxIndx(u16 i, u32 id)
{
    if (on()) load_indexed(0, (u32) i << 16 | 11u << 12 | (id * 4)), note("GXLoadPosMtxIndx");
    __real_GXLoadPosMtxIndx(i, id);
}
void __wrap_GXLoadNrmMtxIndx3x3(u16 i, u32 id)
{
    if (on()) load_indexed(1, (u32) i << 16 | 8u << 12 | (id * 3 + 0x400)), note("GXLoadNrmMtxIndx3x3");
    __real_GXLoadNrmMtxIndx3x3(i, id);
}
void __wrap_GXLoadTexMtxIndx(u16 i, u32 id, GXTexMtxType type)
{
    if (on()) {
        const u32 addr = id >= GX_PTTEXMTX0 ? (id - GX_PTTEXMTX0) * 4 + 0x500 : id * 4;
        load_indexed(2, (u32) i << 16 | (u32) (type == GX_MTX2x4 ? 7 : 11) << 12 | addr);
        note("GXLoadTexMtxIndx");
    }
    __real_GXLoadTexMtxIndx(i, id, type);
}
void __wrap_GXSetCurrentMtx(u32 id)
{
    if (on()) S.cur_mtx = id, ++S.ver_xf, note("GXSetCurrentMtx");
    __real_GXSetCurrentMtx(id);
}
void __wrap_GXSetProjection(const f32 m[4][4], GXProjectionType type)
{
    if (on()) {
        S.proj_type = type;
        S.proj[0] = m[0][0];
        S.proj[2] = m[1][1];
        S.proj[4] = m[2][2];
        S.proj[5] = m[2][3];
        S.proj[1] = type == GX_ORTHOGRAPHIC ? m[0][3] : m[0][2];
        S.proj[3] = type == GX_ORTHOGRAPHIC ? m[1][3] : m[1][2];
        ++S.ver_xf;
        note("GXSetProjection");
    }
    __real_GXSetProjection(m, type);
}
void __wrap_GXSetProjectionv(const f32* p)
{
    if (on()) {
        S.proj_type = p[0] == 0.0f ? GX_PERSPECTIVE : GX_ORTHOGRAPHIC;
        for (int i = 0; i < 6; ++i) S.proj[i] = p[1 + i];
        ++S.ver_xf;
        note("GXSetProjectionv");
    }
    __real_GXSetProjectionv(p);
}
void __wrap_GXGetProjectionv(f32* p)
{
    __real_GXGetProjectionv(p);
    if (on()) {
        const float t = S.proj_type != GX_PERSPECTIVE ? 1.0f : 0.0f;
        if (fbits(t) != fbits(p[0])) ++st.get_bad, mismatch(6, 0x20, fbits(t), fbits(p[0]), "GXGetProjectionv type");
        for (int i = 0; i < 6; ++i)
            if (fbits(S.proj[i]) != fbits(p[1 + i])) ++st.get_bad, mismatch(6, 0x21 + i, fbits(S.proj[i]), fbits(p[1 + i]), "GXGetProjectionv");
    }
}
static void set_viewport(f32 l, f32 t, f32 w, f32 h, f32 n, f32 f, u32 field)
{
    if (field == 0) t -= 0.5f;
    S.vp[0] = l, S.vp[1] = t, S.vp[2] = w, S.vp[3] = h, S.vp[4] = n, S.vp[5] = f;
    ++S.ver_xf;
}
void __wrap_GXSetViewport(f32 l, f32 t, f32 w, f32 h, f32 n, f32 f)
{
    if (on()) set_viewport(l, t, w, h, n, f, 1), note("GXSetViewport");
    __real_GXSetViewport(l, t, w, h, n, f);
}
void __wrap_GXSetViewportJitter(f32 l, f32 t, f32 w, f32 h, f32 n, f32 f, u32 field)
{
    if (on()) set_viewport(l, t, w, h, n, f, field), note("GXSetViewportJitter");
    __real_GXSetViewportJitter(l, t, w, h, n, f, field);
}
void __wrap_GXGetViewportv(f32* v)
{
    __real_GXGetViewportv(v);
    if (on())
        for (int i = 0; i < 6; ++i)
            if (fbits(S.vp[i]) != fbits(v[i])) ++st.get_bad, mismatch(6, 0x1A + i, fbits(S.vp[i]), fbits(v[i]), "GXGetViewportv");
}
void __wrap_GXSetZScaleOffset(f32 scale, f32 offset)
{
    if (on()) S.zoffset = offset * 1.6777215e7f, S.zscale = (scale * 1.6777215e7f) + 1.0f, ++S.ver_xf, note("GXSetZScaleOffset");
    __real_GXSetZScaleOffset(scale, offset);
}
void __wrap_GXSetScissor(u32 l, u32 t, u32 w, u32 h)
{
    if (on()) S.sc[0] = l, S.sc[1] = t, S.sc[2] = w, S.sc[3] = h, ++S.ver_geom, note("GXSetScissor");
    __real_GXSetScissor(l, t, w, h);
}
void __wrap_GXSetScissorBoxOffset(s32 x, s32 y)
{
    if (on()) S.sc_ox = x, S.sc_oy = y, ++S.ver_geom, note("GXSetScissorBoxOffset");
    __real_GXSetScissorBoxOffset(x, y);
}
void __wrap_GXCallDisplayList(void* list, u32 nbytes)
{
    if (on()) scan_list((const u8*) list, nbytes, 0), note("GXCallDisplayList");
    __real_GXCallDisplayList(list, nbytes);   // la bibliotheque soumet : comparaison apres la lecture du flux
}

}  // extern "C"

#include "gxn_tev.inc"   // etape 2 : TEV et pixels
#include "gxn_light.inc"   // etape 3 : canaux et lumieres
#include "gxn_ind.inc"     // etape 5 : indirect et copies d'image
#include "gxn_tex.inc"     // etape 4 : textures
