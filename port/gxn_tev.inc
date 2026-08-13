// -*- coding: utf-8 -*-
// GX natif, etape 2 : TEV (combinaison des couleurs) et pixels (melange, profondeur, test alpha, brouillard, format).
// Inclus a la fin de port/gxn.cpp (meme unite : memes outils de comparaison). Voir l'en-tete de gxn.cpp.

namespace {

struct TevComb {
    u8 a, b, c, d;
    u8 op, bias, scale, clamp, dest;
};
struct TevStage {
    TevComb col, alp;
    u8 ras_swap, tex_swap;
    u8 coord, chan;   // GXTexCoordID, GXChannelID
    u16 map;          // GXTexMapID (avec GX_TEX_DISABLE)
    u8 kcsel, kasel;
};
struct Pix {
    TevStage st[16];
    u8 n_stages;
    s16 tev_reg[4][4];   // GX_TEVPREV..GX_TEVREG2 : r, g, b, a (S10 possible)
    u8 kcolor[4][4];     // couleurs constantes : r, g, b, a
    u8 last_k[4];        // dernier ecrit a l'adresse partagee E0+2i/E1+2i : 0 couleur, 1 constante
    u8 swap[4][4];       // tables d'echange : rouge, vert, bleu, alpha
    // test alpha
    u8 acmp0, aref0, aop, acmp1, aref1;
    // pixels
    u8 blend_type, blend_src, blend_dst, logic_op;
    u8 color_update, alpha_update, dither;
    u8 z_enable, z_func, z_update;
    u8 zcomp_before_tex;
    u8 dst_alpha_en, dst_alpha;
    u8 pix_fmt, z_fmt, pix_set;
    // brouillard
    u8 fog_set, fog_type;
    float fog_start, fog_end, fog_near, fog_far;
    u8 fog_color[4];
} P;

void reset2()
{
    std::memset(&P, 0, sizeof P);
}

// GXSetTevOp : modes predefinis (memes valeurs que les tables de la bibliotheque, ecrites en champs)
const u8 kTevOpCol[2][5][4] = {
    {{15, 8, 10, 15}, {10, 8, 9, 15}, {10, 12, 8, 15}, {15, 15, 15, 8}, {15, 15, 15, 10}},   // etage 0
    {{15, 8, 0, 15}, {0, 8, 9, 15}, {0, 12, 8, 15}, {15, 15, 15, 8}, {15, 15, 15, 0}},       // etages suivants
};
const u8 kTevOpAlp[2][5][4] = {
    {{7, 4, 5, 7}, {7, 7, 7, 5}, {7, 4, 5, 7}, {7, 7, 7, 4}, {7, 7, 7, 5}},
    {{7, 4, 0, 7}, {7, 7, 7, 0}, {7, 4, 0, 7}, {7, 7, 7, 4}, {7, 7, 7, 0}},
};

void set_comb_in(TevComb& c, u8 a, u8 b, u8 cc, u8 d) { c.a = a, c.b = b, c.c = cc, c.d = d; }
void set_comb_op(TevComb& c, u8 op, u8 bias, u8 scale, u8 clamp, u8 dest)
{
    c.op = op, c.bias = bias, c.scale = scale, c.clamp = clamp, c.dest = dest;
}

// ---------------------------------------------------------------- ce que l'etat signifie pour le materiel
u32 comb_upper(const TevComb& c)
{
    u32 v = (u32) (c.op & 1) << 18 | (u32) (c.clamp & 1) << 19 | (u32) (c.dest & 3) << 22;
    if (c.op <= 1) v |= (u32) (c.scale & 3) << 20 | (u32) (c.bias & 3) << 16;
    else v |= (u32) ((c.op >> 1) & 3) << 20 | 3u << 16;
    return v;
}
u32 expect_tevc(int i)
{
    const TevComb& c = P.st[i].col;
    return (c.d & 15) | (u32) (c.c & 15) << 4 | (u32) (c.b & 15) << 8 | (u32) (c.a & 15) << 12 | comb_upper(c);
}
u32 expect_teva(int i)
{
    const TevComb& c = P.st[i].alp;
    return (P.st[i].ras_swap & 3) | (u32) (P.st[i].tex_swap & 3) << 2 | (u32) (c.d & 7) << 4 | (u32) (c.c & 7) << 7 |
           (u32) (c.b & 7) << 10 | (u32) (c.a & 7) << 13 | comb_upper(c);
}
u32 expect_tref_half(int s)
{
    static const u8 c2r[] = {0, 1, 0, 1, 0, 1, 7, 5, 6};
    const TevStage& t = P.st[s];
    u32 tmap = t.map & ~GX_TEX_DISABLE;
    if (tmap >= GX_MAX_TEXMAP) tmap = GX_TEXMAP0;
    const u32 tc = t.coord >= GX_MAX_TEXCOORD ? GX_TEXCOORD0 : t.coord;
    const u32 ch = t.chan == GX_COLOR_NULL ? 7 : (t.chan < 9 ? c2r[t.chan] : 0);
    const u32 en = t.map != GX_TEXMAP_NULL && !(t.map & GX_TEX_DISABLE);
    return tmap | tc << 3 | en << 6 | ch << 7;
}
u32 expect_ksel(int i)
{
    const int t = i / 2;
    u32 v = (i & 1) ? ((P.swap[t][2] & 3) | (u32) (P.swap[t][3] & 3) << 2) : ((P.swap[t][0] & 3) | (u32) (P.swap[t][1] & 3) << 2);
    v |= (u32) (P.st[2 * i].kcsel & 31) << 4 | (u32) (P.st[2 * i].kasel & 31) << 9;
    v |= (u32) (P.st[2 * i + 1].kcsel & 31) << 14 | (u32) (P.st[2 * i + 1].kasel & 31) << 19;
    return v;
}
void expect_tev_color(int id, u32& ra, u32& bg)
{
    if (P.last_k[id]) {
        const u8* k = P.kcolor[id];
        ra = k[0] | (u32) k[3] << 12 | 8u << 20;
        bg = k[2] | (u32) k[1] << 12 | 8u << 20;
    } else {
        const s16* c = P.tev_reg[id];
        ra = (c[0] & 0x7FF) | (u32) (c[3] & 0x7FF) << 12;
        bg = (c[2] & 0x7FF) | (u32) (c[1] & 0x7FF) << 12;
    }
}
u32 expect_cmode0()
{
    return (u32) (P.blend_type == GX_BM_BLEND || P.blend_type == GX_BM_SUBTRACT) | (u32) (P.blend_type == GX_BM_LOGIC) << 1 |
           (u32) (P.dither & 1) << 2 | (u32) (P.color_update & 1) << 3 | (u32) (P.alpha_update & 1) << 4 |
           (u32) (P.blend_dst & 7) << 5 | (u32) (P.blend_src & 7) << 8 | (u32) (P.blend_type == GX_BM_SUBTRACT) << 11 |
           (u32) (P.logic_op & 15) << 12;
}
// Brouillard : meme calcul, dans le meme ordre, que la bibliotheque (-ffp-contract=off des deux cotes).
void expect_fog(u32 out[5])
{
    const u32 fsel = P.fog_type & 7, proj = (P.fog_type >> 3) & 1;
    const float startz = P.fog_start, endz = P.fog_end, nearz = P.fog_near, farz = P.fog_far;
    float a, c;
    out[1] = out[2] = 0;
    if (proj) {
        if (farz == nearz || endz == startz) {
            a = 0.0f;
            c = 0.0f;
        } else {
            const float A = (1.0f / (endz - startz));
            a = A * (farz - nearz);
            c = A * (startz - nearz);
        }
    } else {
        float A, B, C;
        if (farz == nearz || endz == startz) {
            A = 0.0f;
            B = 0.5f;
            C = 0.0f;
        } else {
            A = (farz * nearz) / ((farz - nearz) * (endz - startz));
            B = farz / (farz - nearz);
            C = startz / (endz - startz);
        }
        float B_mant = B;
        u32 B_expn = 0;
        while (B_mant > 1.0) {
            B_mant /= 2.0f;
            B_expn++;
        }
        while (B_mant > 0.0f && B_mant < 0.5) {
            B_mant *= 2.0f;
            B_expn--;
        }
        a = A / (f32) (1 << (B_expn + 1));
        const u32 b_m = (u32) (8.388638e6f * B_mant);
        c = C;
        out[1] = b_m & 0xFFFFFF;
        out[2] = (B_expn + 1) & 31;
    }
    const u32 ah = fbits(a), ch = fbits(c);
    out[0] = ((ah >> 12) & 0x7FF) | ((ah >> 23) & 0xFF) << 11 | (ah >> 31) << 19;
    out[3] = ((ch >> 12) & 0x7FF) | ((ch >> 23) & 0xFF) << 11 | (ch >> 31) << 19 | proj << 20 | fsel << 21;
    out[4] = (u32) P.fog_color[0] << 16 | (u32) P.fog_color[1] << 8 | P.fog_color[2];
}

// preuve que la comparaison porte sur de vraies valeurs (ligne de controle de gxn.cpp)
void control2(const unsigned* bp)
{
    u32 f[5] = {0, 0, 0, 0, 0};
    if (P.fog_set) expect_fog(f);
    OSReport("[GXN] controle TEV : etages natif %u ; etage 0 couleur natif %06x flux %06x, alpha natif %06x flux %06x ;"
             " ordre 0 natif %06x flux %06x ; melange natif %04x flux %04x ; test alpha natif %06x flux %06x ;"
             " brouillard C natif %06x flux %06x\n",
             (unsigned) P.n_stages, (unsigned) expect_tevc(0), bp[0xC0] & 0xFFFFFF, (unsigned) expect_teva(0),
             bp[0xC1] & 0xFFFFFF, (unsigned) (expect_tref_half(0) | expect_tref_half(1) << 12), bp[0x28] & 0xFFFFFF,
             (unsigned) expect_cmode0(), bp[0x41] & 0xFFFF,
             (unsigned) (P.aref0 | (u32) P.aref1 << 8 | (u32) (P.acmp0 & 7) << 16 | (u32) (P.acmp1 & 7) << 19 | (u32) (P.aop & 3) << 22),
             bp[0xF3] & 0xFFFFFF, (unsigned) f[3], bp[0xF1] & 0xFFFFFF);
}

void compare2(const unsigned* bp)
{
    for (int i = 0; i < 16; ++i) {
        cmp(2, 0xC0 + 2 * i, expect_tevc(i), bp[0xC0 + 2 * i], 0xFFFFFF, "etage TEV couleur");
        cmp(2, 0xC1 + 2 * i, expect_teva(i), bp[0xC1 + 2 * i], 0xFFFFFF, "etage TEV alpha");
    }
    for (int i = 0; i < 8; ++i) {
        cmp(2, 0x28 + i, expect_tref_half(2 * i) | expect_tref_half(2 * i + 1) << 12, bp[0x28 + i], 0xFFFFFF, "ordre TEV");
        cmp(2, 0xF6 + i, expect_ksel(i), bp[0xF6 + i], 0xFFFFFF, "constantes et echanges");
    }
    for (int i = 0; i < 4; ++i) {
        u32 ra, bg;
        expect_tev_color(i, ra, bg);
        cmp(2, 0xE0 + 2 * i, ra, bp[0xE0 + 2 * i], 0xFFFFFF, P.last_k[i] ? "constante RA" : "registre TEV RA");
        cmp(2, 0xE1 + 2 * i, bg, bp[0xE1 + 2 * i], 0xFFFFFF, P.last_k[i] ? "constante BG" : "registre TEV BG");
    }
    cmp(2, 0x00, (u32) ((P.n_stages - 1) & 15) << 10, bp[0x00], 15u << 10, "nombre d'etages TEV");
    cmp(2, 0xF3, P.aref0 | (u32) P.aref1 << 8 | (u32) (P.acmp0 & 7) << 16 | (u32) (P.acmp1 & 7) << 19 | (u32) (P.aop & 3) << 22,
        bp[0xF3], 0xFFFFFF, "test alpha");
    cmp(2, 0x40, (P.z_enable & 1) | (u32) (P.z_func & 7) << 1 | (u32) (P.z_update & 1) << 4, bp[0x40], 0x1F, "profondeur");
    cmp(2, 0x41, expect_cmode0(), bp[0x41], 0xFFFF, "melange");
    cmp(2, 0x42, P.dst_alpha | (u32) (P.dst_alpha_en & 1) << 8, bp[0x42], 0x1FF, "alpha de destination");
    if (P.pix_set) {
        static const u8 p2f[8] = {0, 1, 2, 3, 4, 4, 4, 5};
        cmp(2, 0x43, p2f[P.pix_fmt & 7] | (u32) (P.z_fmt & 7) << 3 | (u32) (P.zcomp_before_tex & 1) << 6, bp[0x43], 0x7F,
            "format de pixel");
    } else {
        cmp(2, 0x43, (u32) (P.zcomp_before_tex & 1) << 6, bp[0x43], 0x40, "profondeur avant texture");
    }
    if (P.fog_set) {
        u32 f[5];
        expect_fog(f);
        cmp(2, 0xEE, f[0], bp[0xEE], 0xFFFFFF, "brouillard A");
        if (!((P.fog_type >> 3) & 1)) {
            cmp(2, 0xEF, f[1], bp[0xEF], 0xFFFFFF, "brouillard B");
            cmp(2, 0xF0, f[2], bp[0xF0], 0x1F, "brouillard B exposant");
        }
        cmp(2, 0xF1, f[3], bp[0xF1], 0xFFFFFF, "brouillard C et type");
        cmp(2, 0xF2, f[4], bp[0xF2], 0xFFFFFF, "couleur du brouillard");
    }
}

}  // namespace

extern "C" {

REAL(void, GXSetTevOp, (GXTevStageID, GXTevMode))
REAL(void, GXSetTevColorIn, (GXTevStageID, GXTevColorArg, GXTevColorArg, GXTevColorArg, GXTevColorArg))
REAL(void, GXSetTevAlphaIn, (GXTevStageID, GXTevAlphaArg, GXTevAlphaArg, GXTevAlphaArg, GXTevAlphaArg))
REAL(void, GXSetTevColorOp, (GXTevStageID, GXTevOp, GXTevBias, GXTevScale, GXBool, GXTevRegID))
REAL(void, GXSetTevAlphaOp, (GXTevStageID, GXTevOp, GXTevBias, GXTevScale, GXBool, GXTevRegID))
REAL(void, GXSetTevColor, (GXTevRegID, GXColor))
REAL(void, GXSetTevColorS10, (GXTevRegID, GXColorS10))
REAL(void, GXSetTevKColor, (GXTevKColorID, GXColor))
REAL(void, GXSetTevKColorSel, (GXTevStageID, GXTevKColorSel))
REAL(void, GXSetTevKAlphaSel, (GXTevStageID, GXTevKAlphaSel))
REAL(void, GXSetTevSwapMode, (GXTevStageID, GXTevSwapSel, GXTevSwapSel))
REAL(void, GXSetTevSwapModeTable, (GXTevSwapSel, GXTevColorChan, GXTevColorChan, GXTevColorChan, GXTevColorChan))
REAL(void, GXSetAlphaCompare, (GXCompare, u8, GXAlphaOp, GXCompare, u8))
REAL(void, GXSetTevOrder, (GXTevStageID, GXTexCoordID, GXTexMapID, GXChannelID))
REAL(void, GXSetNumTevStages, (u8))
REAL(void, GXSetBlendMode, (GXBlendMode, GXBlendFactor, GXBlendFactor, GXLogicOp))
REAL(void, GXSetColorUpdate, (GXBool))
REAL(void, GXSetAlphaUpdate, (GXBool))
REAL(void, GXSetZMode, (GXBool, GXCompare, GXBool))
REAL(void, GXSetZCompLoc, (GXBool))
REAL(void, GXSetPixelFmt, (GXPixelFmt, GXZFmt16))
REAL(void, GXSetDither, (GXBool))
REAL(void, GXSetDstAlpha, (GXBool, u8))
REAL(void, GXSetFog, (GXFogType, f32, f32, f32, f32, GXColor))

void __wrap_GXSetTevOp(GXTevStageID id, GXTevMode mode)
{
    if (on() && id < 16 && mode < 5) {
        note("GXSetTevOp");
        const int k = id == GX_TEVSTAGE0 ? 0 : 1;
        TevStage& s = P.st[id];
        const u8* c = kTevOpCol[k][mode];
        const u8* a = kTevOpAlp[k][mode];
        set_comb_in(s.col, c[0], c[1], c[2], c[3]);
        set_comb_in(s.alp, a[0], a[1], a[2], a[3]);
        set_comb_op(s.col, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);
        set_comb_op(s.alp, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);
    }
    __real_GXSetTevOp(id, mode);
}
void __wrap_GXSetTevColorIn(GXTevStageID s, GXTevColorArg a, GXTevColorArg b, GXTevColorArg c, GXTevColorArg d)
{
    if (on() && s < 16) note("GXSetTevColorIn"), set_comb_in(P.st[s].col, a, b, c, d);
    __real_GXSetTevColorIn(s, a, b, c, d);
}
void __wrap_GXSetTevAlphaIn(GXTevStageID s, GXTevAlphaArg a, GXTevAlphaArg b, GXTevAlphaArg c, GXTevAlphaArg d)
{
    if (on() && s < 16) note("GXSetTevAlphaIn"), set_comb_in(P.st[s].alp, a, b, c, d);
    __real_GXSetTevAlphaIn(s, a, b, c, d);
}
void __wrap_GXSetTevColorOp(GXTevStageID s, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out)
{
    if (on() && s < 16) note("GXSetTevColorOp"), set_comb_op(P.st[s].col, op, bias, scale, clamp, out);
    __real_GXSetTevColorOp(s, op, bias, scale, clamp, out);
}
void __wrap_GXSetTevAlphaOp(GXTevStageID s, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out)
{
    if (on() && s < 16) note("GXSetTevAlphaOp"), set_comb_op(P.st[s].alp, op, bias, scale, clamp, out);
    __real_GXSetTevAlphaOp(s, op, bias, scale, clamp, out);
}
void __wrap_GXSetTevColor(GXTevRegID id, GXColor c)
{
    if (on() && id < 4) {
        note("GXSetTevColor");
        P.tev_reg[id][0] = c.r, P.tev_reg[id][1] = c.g, P.tev_reg[id][2] = c.b, P.tev_reg[id][3] = c.a;
        P.last_k[id] = 0;
    }
    __real_GXSetTevColor(id, c);
}
void __wrap_GXSetTevColorS10(GXTevRegID id, GXColorS10 c)
{
    if (on() && id < 4) {
        note("GXSetTevColorS10");
        P.tev_reg[id][0] = c.r, P.tev_reg[id][1] = c.g, P.tev_reg[id][2] = c.b, P.tev_reg[id][3] = c.a;
        P.last_k[id] = 0;
    }
    __real_GXSetTevColorS10(id, c);
}
void __wrap_GXSetTevKColor(GXTevKColorID id, GXColor c)
{
    if (on() && id < 4) {
        note("GXSetTevKColor");
        P.kcolor[id][0] = c.r, P.kcolor[id][1] = c.g, P.kcolor[id][2] = c.b, P.kcolor[id][3] = c.a;
        P.last_k[id] = 1;
    }
    __real_GXSetTevKColor(id, c);
}
void __wrap_GXSetTevKColorSel(GXTevStageID s, GXTevKColorSel sel)
{
    if (on() && s < 16) note("GXSetTevKColorSel"), P.st[s].kcsel = (u8) sel;
    __real_GXSetTevKColorSel(s, sel);
}
void __wrap_GXSetTevKAlphaSel(GXTevStageID s, GXTevKAlphaSel sel)
{
    if (on() && s < 16) note("GXSetTevKAlphaSel"), P.st[s].kasel = (u8) sel;
    __real_GXSetTevKAlphaSel(s, sel);
}
void __wrap_GXSetTevSwapMode(GXTevStageID s, GXTevSwapSel ras, GXTevSwapSel tex)
{
    if (on() && s < 16) note("GXSetTevSwapMode"), P.st[s].ras_swap = (u8) ras, P.st[s].tex_swap = (u8) tex;
    __real_GXSetTevSwapMode(s, ras, tex);
}
void __wrap_GXSetTevSwapModeTable(GXTevSwapSel t, GXTevColorChan r, GXTevColorChan g, GXTevColorChan b, GXTevColorChan a)
{
    if (on() && t < 4) note("GXSetTevSwapModeTable"), P.swap[t][0] = r, P.swap[t][1] = g, P.swap[t][2] = b, P.swap[t][3] = a;
    __real_GXSetTevSwapModeTable(t, r, g, b, a);
}
void __wrap_GXSetAlphaCompare(GXCompare c0, u8 r0, GXAlphaOp op, GXCompare c1, u8 r1)
{
    if (on()) note("GXSetAlphaCompare"), P.acmp0 = c0, P.aref0 = r0, P.aop = op, P.acmp1 = c1, P.aref1 = r1;
    __real_GXSetAlphaCompare(c0, r0, op, c1, r1);
}
void __wrap_GXSetTevOrder(GXTevStageID s, GXTexCoordID coord, GXTexMapID map, GXChannelID chan)
{
    if (on() && s < 16) note("GXSetTevOrder"), P.st[s].coord = (u8) coord, P.st[s].map = (u16) map, P.st[s].chan = (u8) chan;
    __real_GXSetTevOrder(s, coord, map, chan);
}
void __wrap_GXSetNumTevStages(u8 n)
{
    if (on()) note("GXSetNumTevStages"), P.n_stages = n;
    __real_GXSetNumTevStages(n);
}
void __wrap_GXSetBlendMode(GXBlendMode type, GXBlendFactor src, GXBlendFactor dst, GXLogicOp op)
{
    if (on()) note("GXSetBlendMode"), P.blend_type = type, P.blend_src = src, P.blend_dst = dst, P.logic_op = op;
    __real_GXSetBlendMode(type, src, dst, op);
}
void __wrap_GXSetColorUpdate(GXBool e)
{
    if (on()) note("GXSetColorUpdate"), P.color_update = e;
    __real_GXSetColorUpdate(e);
}
void __wrap_GXSetAlphaUpdate(GXBool e)
{
    if (on()) note("GXSetAlphaUpdate"), P.alpha_update = e;
    __real_GXSetAlphaUpdate(e);
}
void __wrap_GXSetZMode(GXBool en, GXCompare fn, GXBool upd)
{
    if (on()) note("GXSetZMode"), P.z_enable = en, P.z_func = fn, P.z_update = upd;
    __real_GXSetZMode(en, fn, upd);
}
void __wrap_GXSetZCompLoc(GXBool before)
{
    if (on()) note("GXSetZCompLoc"), P.zcomp_before_tex = before;
    __real_GXSetZCompLoc(before);
}
void __wrap_GXSetPixelFmt(GXPixelFmt pf, GXZFmt16 zf)
{
    if (on()) note("GXSetPixelFmt"), P.pix_fmt = pf, P.z_fmt = zf, P.pix_set = 1;
    __real_GXSetPixelFmt(pf, zf);
}
void __wrap_GXSetDither(GXBool d)
{
    if (on()) note("GXSetDither"), P.dither = d;
    __real_GXSetDither(d);
}
void __wrap_GXSetDstAlpha(GXBool en, u8 a)
{
    if (on()) note("GXSetDstAlpha"), P.dst_alpha_en = en, P.dst_alpha = a;
    __real_GXSetDstAlpha(en, a);
}
void __wrap_GXSetFog(GXFogType type, f32 startz, f32 endz, f32 nearz, f32 farz, GXColor color)
{
    if (on()) {
        note("GXSetFog");
        P.fog_set = 1, P.fog_type = (u8) type;
        P.fog_start = startz, P.fog_end = endz, P.fog_near = nearz, P.fog_far = farz;
        P.fog_color[0] = color.r, P.fog_color[1] = color.g, P.fog_color[2] = color.b, P.fog_color[3] = color.a;
    }
    __real_GXSetFog(type, startz, endz, nearz, farz, color);
}

}  // extern "C"
