// -*- coding: utf-8 -*-
// GX natif, etape 5 : textures indirectes (refraction, distorsions) et copies d'image (EFB). Inclus dans port/gxn.cpp.

namespace {

struct IndTev {
    u8 ind_stage, format, bias, alpha, mtx, wrap_s, wrap_t, add_prev, utc_lod;
};
struct IndMtx {
    float off[2][3];
    s8 scale_exp;
    u8 set;
};
struct Ind {
    IndTev tev[16];
    IndMtx mtx[3];
    u8 scale[4][2];        // s, t
    u8 order[4][2];        // coordonnee, unite
    u8 n_ind;
} I;

struct Copy {
    u16 disp_src[4], tex_src[4];   // gauche, haut, largeur, hauteur
    u16 disp_w;                    // largeur de l'image affichee (pas = 2 octets par point)
    u16 tex_w, tex_h;
    u8 tex_fmt, tex_mip;
    u8 clamp, gamma, f2f;
    float yscale;
    u8 yscale_set;
    u8 clear_c[4];
    u32 clear_z;
    u8 clear_set;
    u8 aa, vf, pattern[12][2], vfilter[7], filter_set;
    u8 last_kind;   // 0 aucune, 1 vers l'affichage, 2 vers une texture
    u32 last_dest;
    u8 last_clear;
    u32 copies;
} C;

void reset5()
{
    std::memset(&I, 0, sizeof I);
    std::memset(&C, 0, sizeof C);
}

int ind_mtx_index(u8 id)
{
    if (id >= GX_ITM_0 && id <= GX_ITM_2) return id - 1;
    if (id >= GX_ITM_S0 && id <= GX_ITM_S2) return id - 5;
    if (id >= GX_ITM_T0 && id <= GX_ITM_T2) return id - 9;
    return 0;
}

void set_ind(GXTevStageID s, GXIndTexStageID ind, GXIndTexFormat fmt, GXIndTexBiasSel bias, GXIndTexMtxID mtx, GXIndTexWrap ws,
             GXIndTexWrap wt, GXBool add, GXBool utc, GXIndTexAlphaSel alpha)
{
    if (s >= 16) return;
    I.tev[s] = {(u8) ind, (u8) fmt, (u8) bias, (u8) alpha, (u8) mtx, (u8) ws, (u8) wt, (u8) add, (u8) utc};
}

void tile_shift(u32 fmt, u32& r, u32& c)
{
    switch (fmt) {
    case GX_TF_I4: case 0x8: case GX_TF_CMPR: case GX_CTF_R4: case GX_CTF_Z4: r = 3, c = 3; break;
    case GX_TF_I8: case GX_TF_IA4: case 0x9: case GX_TF_Z8: case GX_CTF_RA4: case GX_TF_A8: case GX_CTF_R8: case GX_CTF_G8:
    case GX_CTF_B8: case GX_CTF_Z8M: case GX_CTF_Z8L: r = 3, c = 2; break;
    case GX_TF_IA8: case GX_TF_RGB565: case GX_TF_RGB5A3: case GX_TF_RGBA8: case 0xA: case GX_TF_Z16: case GX_TF_Z24X8:
    case GX_CTF_RA8: case GX_CTF_RG8: case GX_CTF_GB8: case GX_CTF_Z16L: r = 2, c = 2; break;
    default: r = c = 0; break;
    }
}

void compare5(const unsigned* bp)
{
    // indirect
    for (int s = 0; s < 16; ++s) {
        const IndTev& t = I.tev[s];
        cmp(2, 0x10 + s, (t.ind_stage & 3) | (u32) (t.format & 3) << 2 | (u32) (t.bias & 7) << 4 | (u32) (t.alpha & 3) << 7 |
                             (u32) (t.mtx & 15) << 9 | (u32) (t.wrap_s & 7) << 13 | (u32) (t.wrap_t & 7) << 16 |
                             (u32) (t.utc_lod & 1) << 19 | (u32) (t.add_prev & 1) << 20,
            bp[0x10 + s], 0x1FFFFF, "etage indirect");
    }
    for (int m = 0; m < 3; ++m) {
        const IndMtx& x = I.mtx[m];
        if (!x.set) continue;
        const s8 e = (s8) (x.scale_exp + 17);
        for (int c = 0; c < 3; ++c) {
            const u32 a = (u32) ((int) (1024.0f * x.off[0][c]) & 0x7FF), b = (u32) ((int) (1024.0f * x.off[1][c]) & 0x7FF);
            cmp(2, 6 + 3 * m + c, a | b << 11 | (u32) ((e >> (2 * c)) & 3) << 22, bp[6 + 3 * m + c], 0xFFFFFF, "matrice indirecte");
        }
    }
    cmp(2, 0x25, (I.scale[0][0] & 15) | (u32) (I.scale[0][1] & 15) << 4 | (u32) (I.scale[1][0] & 15) << 8 | (u32) (I.scale[1][1] & 15) << 12,
        bp[0x25], 0xFFFF, "echelle indirecte 0-1");
    cmp(2, 0x26, (I.scale[2][0] & 15) | (u32) (I.scale[2][1] & 15) << 4 | (u32) (I.scale[3][0] & 15) << 8 | (u32) (I.scale[3][1] & 15) << 12,
        bp[0x26], 0xFFFF, "echelle indirecte 2-3");
    u32 iref = 0;
    for (int i = 0; i < 4; ++i) iref |= (u32) (I.order[i][1] & 7) << (6 * i) | (u32) (I.order[i][0] & 7) << (6 * i + 3);
    cmp(2, 0x27, iref, bp[0x27], 0xFFFFFF, "ordre indirect");
    cmp(2, 0x00, (u32) (I.n_ind & 7) << 16, bp[0x00], 7u << 16, "nombre d'etages indirects");
    // copies d'image : derniere copie executee
    if (C.last_kind) {
        const bool disp = C.last_kind == 1;
        const u16* src = disp ? C.disp_src : C.tex_src;
        cmp(2, 0x49, (src[0] & 0x3FF) | (u32) (src[1] & 0x3FF) << 10, bp[0x49], 0xFFFFF, "copie : coin source");
        cmp(2, 0x4A, ((src[2] - 1) & 0x3FF) | (u32) ((src[3] - 1) & 0x3FF) << 10, bp[0x4A], 0xFFFFF, "copie : taille");
        cmp(2, 0x4B, ((C.last_dest & 0x3FFFFFFF) >> 5) & 0x1FFFFF, bp[0x4B], 0x1FFFFF, "copie : destination");
        u32 stride, ctl;
        const u32 clampT = (C.clamp & GX_CLAMP_TOP) == 1, clampB = (C.clamp & GX_CLAMP_BOTTOM) == 2;
        if (disp) {
            stride = ((u16) (C.disp_w * 2) >> 5) & 0x3FF;
            const u32 iscale = C.yscale_set ? ((u32) (256.0f / C.yscale) & 0x1FF) : 256;
            ctl = clampT | clampB << 1 | (u32) (C.gamma & 3) << 7 | (u32) (iscale != 256) << 10 | (u32) C.last_clear << 11 |
                  (u32) (C.f2f & 3) << 12 | 1u << 14;
        } else {
            u32 r, c;
            tile_shift(C.tex_fmt, r, c);
            const u32 w = C.tex_w ? C.tex_w : 1;
            const u32 rows = (w + (1u << r) - 1) >> r, cmpt = (C.tex_fmt == GX_TF_RGBA8 || C.tex_fmt == GX_TF_Z24X8) ? 2 : 1;
            stride = (rows * cmpt) & 0x3FF;
            u32 pf = C.tex_fmt & 0xF;
            if (C.tex_fmt == GX_TF_Z16) pf = 0xB;
            u32 ccv = 2;
            switch (C.tex_fmt) {
            case GX_TF_I4: case GX_TF_I8: case GX_TF_IA4: case GX_TF_IA8: case GX_CTF_YUVA8: ccv = 3; break;
            default: break;
            }
            ctl = clampT | clampB << 1 | ((pf >> 3) & 1) << 3 | (pf & 7) << 4 | (u32) (C.tex_mip & 1) << 9 | (u32) C.last_clear << 11 |
                  ccv << 15;
        }
        cmp(2, 0x4D, stride, bp[0x4D], 0x3FF, "copie : pas");
        cmp(2, 0x52, ctl, bp[0x52], 0x1FFFF, disp ? "copie vers l'affichage : reglages" : "copie vers une texture : reglages");
    }
    if (C.yscale_set) cmp(2, 0x4E, (u32) (256.0f / C.yscale) & 0x1FF, bp[0x4E], 0x1FF, "echelle verticale de l'affichage");
    if (C.clear_set) {
        cmp(2, 0x4F, C.clear_c[0] | (u32) C.clear_c[3] << 8, bp[0x4F], 0xFFFF, "couleur d'effacement RA");
        cmp(2, 0x50, C.clear_c[2] | (u32) C.clear_c[1] << 8, bp[0x50], 0xFFFF, "couleur d'effacement BG");
        cmp(2, 0x51, C.clear_z & 0xFFFFFF, bp[0x51], 0xFFFFFF, "profondeur d'effacement");
    }
    if (C.filter_set) {
        for (int k = 0; k < 4; ++k) {
            u32 v = 0x666666;
            if (C.aa) {
                v = 0;
                for (int j = 0; j < 3; ++j)
                    v |= (u32) (C.pattern[3 * k + j][0] & 15) << (8 * j) | (u32) (C.pattern[3 * k + j][1] & 15) << (8 * j + 4);
            }
            cmp(2, 1 + k, v, bp[1 + k], 0xFFFFFF, "motif d'echantillonnage");
        }
        static const u8 def[7] = {0, 0, 21, 22, 21, 0, 0};
        const u8* f = C.vf ? C.vfilter : def;
        cmp(2, 0x53, (f[0] & 63) | (u32) (f[1] & 63) << 6 | (u32) (f[2] & 63) << 12 | (u32) (f[3] & 63) << 18, bp[0x53], 0xFFFFFF,
            "filtre vertical 0-3");
        cmp(2, 0x54, (f[4] & 63) | (u32) (f[5] & 63) << 6 | (u32) (f[6] & 63) << 12, bp[0x54], 0x3FFFF, "filtre vertical 4-6");
    }
}

void control5(const unsigned* bp)
{
    OSReport("[GXN] controle indirect/copies : etages indirects %u, etage 0 indirect flux %06x ; copies %u, derniere %s,"
             " destination flux %06x, reglages flux %05x\n",
             (unsigned) I.n_ind, bp[0x10] & 0xFFFFFF, (unsigned) C.copies,
             C.last_kind == 1 ? "affichage" : C.last_kind == 2 ? "texture" : "aucune", bp[0x4B] & 0x1FFFFF, bp[0x52] & 0x1FFFF);
}

}  // namespace

extern "C" {

REAL(void, GXSetTevIndirect, (GXTevStageID, GXIndTexStageID, GXIndTexFormat, GXIndTexBiasSel, GXIndTexMtxID, GXIndTexWrap,
                              GXIndTexWrap, GXBool, GXBool, GXIndTexAlphaSel))
REAL(void, GXSetTevDirect, (GXTevStageID))
REAL(void, GXSetTevIndWarp, (GXTevStageID, GXIndTexStageID, u8, u8, GXIndTexMtxID))
REAL(void, GXSetTevIndBumpXYZ, (GXTevStageID, GXIndTexStageID, GXIndTexMtxID))
REAL(void, GXSetIndTexMtx, (GXIndTexMtxID, const f32[2][3], s8))
REAL(void, GXSetIndTexCoordScale, (GXIndTexStageID, GXIndTexScale, GXIndTexScale))
REAL(void, GXSetIndTexOrder, (GXIndTexStageID, GXTexCoordID, GXTexMapID))
REAL(void, GXSetNumIndStages, (u8))
REAL(void, GXSetDispCopySrc, (u16, u16, u16, u16))
REAL(void, GXSetTexCopySrc, (u16, u16, u16, u16))
REAL(void, GXSetDispCopyDst, (u16, u16))
REAL(void, GXSetTexCopyDst, (u16, u16, GXTexFmt, GXBool))
REAL(void, GXSetDispCopyFrame2Field, (GXCopyMode))
REAL(void, GXSetCopyClamp, (GXFBClamp))
REAL(u32, GXSetDispCopyYScale, (f32))
REAL(void, GXSetCopyClear, (GXColor, u32))
REAL(void, GXSetCopyFilter, (GXBool, const u8[12][2], GXBool, const u8[7]))
REAL(void, GXSetDispCopyGamma, (GXGamma))
REAL(void, GXCopyDisp, (void*, GXBool))
REAL(void, GXCopyTex, (void*, GXBool))

void __wrap_GXSetTevIndirect(GXTevStageID s, GXIndTexStageID ind, GXIndTexFormat fmt, GXIndTexBiasSel bias, GXIndTexMtxID mtx,
                             GXIndTexWrap ws, GXIndTexWrap wt, GXBool add, GXBool utc, GXIndTexAlphaSel alpha)
{
    if (on()) note("GXSetTevIndirect"), set_ind(s, ind, fmt, bias, mtx, ws, wt, add, utc, alpha);
    __real_GXSetTevIndirect(s, ind, fmt, bias, mtx, ws, wt, add, utc, alpha);
}
void __wrap_GXSetTevDirect(GXTevStageID s)
{
    if (on())
        note("GXSetTevDirect"), set_ind(s, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_NONE, GX_ITM_OFF, GX_ITW_OFF, GX_ITW_OFF, GX_FALSE, GX_FALSE,
                                        GX_ITBA_OFF);
    __real_GXSetTevDirect(s);
}
void __wrap_GXSetTevIndWarp(GXTevStageID s, GXIndTexStageID ind, u8 signed_ofs, u8 replace, GXIndTexMtxID mtx)
{
    if (on()) {
        note("GXSetTevIndWarp");
        const GXIndTexWrap w = replace ? GX_ITW_0 : GX_ITW_OFF;
        set_ind(s, ind, GX_ITF_8, signed_ofs ? GX_ITB_STU : GX_ITB_NONE, mtx, w, w, GX_FALSE, GX_FALSE, GX_ITBA_OFF);
    }
    __real_GXSetTevIndWarp(s, ind, signed_ofs, replace, mtx);
}
void __wrap_GXSetTevIndBumpXYZ(GXTevStageID s, GXIndTexStageID ind, GXIndTexMtxID mtx)
{
    if (on()) note("GXSetTevIndBumpXYZ"), set_ind(s, ind, GX_ITF_8, GX_ITB_STU, mtx, GX_ITW_OFF, GX_ITW_OFF, GX_FALSE, GX_FALSE, GX_ITBA_OFF);
    __real_GXSetTevIndBumpXYZ(s, ind, mtx);
}
void __wrap_GXSetIndTexMtx(GXIndTexMtxID id, const f32 off[2][3], s8 scale_exp)
{
    if (on()) {
        note("GXSetIndTexMtx");
        IndMtx& m = I.mtx[ind_mtx_index((u8) id)];
        for (int r = 0; r < 2; ++r)
            for (int c = 0; c < 3; ++c) m.off[r][c] = off[r][c];
        m.scale_exp = scale_exp, m.set = 1;
    }
    __real_GXSetIndTexMtx(id, off, scale_exp);
}
void __wrap_GXSetIndTexCoordScale(GXIndTexStageID ind, GXIndTexScale s, GXIndTexScale t)
{
    if (on() && ind < 4) note("GXSetIndTexCoordScale"), I.scale[ind][0] = (u8) s, I.scale[ind][1] = (u8) t;
    __real_GXSetIndTexCoordScale(ind, s, t);
}
void __wrap_GXSetIndTexOrder(GXIndTexStageID ind, GXTexCoordID coord, GXTexMapID map)
{
    if (on() && ind < 4) {
        note("GXSetIndTexOrder");
        I.order[ind][0] = (u8) (coord == GX_TEXCOORD_NULL ? GX_TEXCOORD0 : coord);
        I.order[ind][1] = (u8) (map == GX_TEXMAP_NULL ? GX_TEXMAP0 : map);
    }
    __real_GXSetIndTexOrder(ind, coord, map);
}
void __wrap_GXSetNumIndStages(u8 n)
{
    if (on()) note("GXSetNumIndStages"), I.n_ind = n;
    __real_GXSetNumIndStages(n);
}
void __wrap_GXSetDispCopySrc(u16 l, u16 t, u16 w, u16 h)
{
    if (on()) note("GXSetDispCopySrc"), C.disp_src[0] = l, C.disp_src[1] = t, C.disp_src[2] = w, C.disp_src[3] = h;
    __real_GXSetDispCopySrc(l, t, w, h);
}
void __wrap_GXSetTexCopySrc(u16 l, u16 t, u16 w, u16 h)
{
    if (on()) note("GXSetTexCopySrc"), C.tex_src[0] = l, C.tex_src[1] = t, C.tex_src[2] = w, C.tex_src[3] = h;
    __real_GXSetTexCopySrc(l, t, w, h);
}
void __wrap_GXSetDispCopyDst(u16 w, u16 h)
{
    if (on()) note("GXSetDispCopyDst"), C.disp_w = w;
    __real_GXSetDispCopyDst(w, h);
}
void __wrap_GXSetTexCopyDst(u16 w, u16 h, GXTexFmt fmt, GXBool mip)
{
    if (on()) note("GXSetTexCopyDst"), C.tex_w = w, C.tex_h = h, C.tex_fmt = (u8) fmt, C.tex_mip = mip;
    __real_GXSetTexCopyDst(w, h, fmt, mip);
}
void __wrap_GXSetDispCopyFrame2Field(GXCopyMode m)
{
    if (on()) note("GXSetDispCopyFrame2Field"), C.f2f = (u8) m;
    __real_GXSetDispCopyFrame2Field(m);
}
void __wrap_GXSetCopyClamp(GXFBClamp c)
{
    if (on()) note("GXSetCopyClamp"), C.clamp = (u8) c;
    __real_GXSetCopyClamp(c);
}
u32 __wrap_GXSetDispCopyYScale(f32 v)
{
    if (on()) note("GXSetDispCopyYScale"), C.yscale = v, C.yscale_set = 1;
    return __real_GXSetDispCopyYScale(v);
}
void __wrap_GXSetCopyClear(GXColor c, u32 z)
{
    if (on()) note("GXSetCopyClear"), C.clear_c[0] = c.r, C.clear_c[1] = c.g, C.clear_c[2] = c.b, C.clear_c[3] = c.a, C.clear_z = z, C.clear_set = 1;
    __real_GXSetCopyClear(c, z);
}
void __wrap_GXSetCopyFilter(GXBool aa, const u8 pattern[12][2], GXBool vf, const u8 vfilter[7])
{
    if (on()) {
        note("GXSetCopyFilter");
        C.aa = aa, C.vf = vf, C.filter_set = 1;
        if (aa) std::memcpy(C.pattern, pattern, sizeof C.pattern);
        if (vf) std::memcpy(C.vfilter, vfilter, sizeof C.vfilter);
    }
    __real_GXSetCopyFilter(aa, pattern, vf, vfilter);
}
void __wrap_GXSetDispCopyGamma(GXGamma g)
{
    if (on()) note("GXSetDispCopyGamma"), C.gamma = (u8) g;
    __real_GXSetDispCopyGamma(g);
}
void __wrap_GXCopyDisp(void* dest, GXBool clear)
{
    if (on()) note("GXCopyDisp"), C.last_kind = 1, C.last_dest = (u32) dest, C.last_clear = clear, ++C.copies;
    __real_GXCopyDisp(dest, clear);
}
void __wrap_GXCopyTex(void* dest, GXBool clear)
{
    if (on()) note("GXCopyTex"), C.last_kind = 2, C.last_dest = (u32) dest, C.last_clear = clear, ++C.copies;
    __real_GXCopyTex(dest, clear);
}

}  // extern "C"
