// -*- coding: utf-8 -*-
// GX natif, etape 3 : canaux de couleur et huit lumieres. Inclus a la fin de port/gxn.cpp (voir son en-tete).
// GXInitLight* (src/lib/GXLight.c) ne remplissent qu'un objet du jeu en memoire ordinaire : rien a remplacer.

namespace {

struct ChanCtrl {
    u8 enable, amb_src, mat_src, diff_fn, attn_fn, set;
    u8 light_mask;
};
struct Light {
    u8 color[4];   // r, g, b, a
    float a[3], k[3], pos[3], dir[3];
    u8 set;
};
struct Lit {
    u8 n_chans;
    u8 amb[2][4], mat[2][4];   // COLOR0/COLOR1 : r, g, b, a
    u8 amb_set[2], mat_set[2];
    ChanCtrl ctrl[4];          // COLOR0, COLOR1, ALPHA0, ALPHA1
    Light light[8];
} L;

void reset3() { std::memset(&L, 0, sizeof L); }

// meme disposition que l'objet de la bibliotheque (__GXLightObjInt_struct)
struct LightObjInt {
    u32 reserved[3];
    u32 color;   // 0xRRGGBBAA (portage)
    f32 a[3], k[3], lpos[3], ldir[3];
};

void set_color(u8 dst[2][4], u8 setf[2], GXChannelID chan, GXColor c)
{
    switch (chan) {
    case GX_COLOR0: case GX_COLOR1:
        dst[chan][0] = c.r, dst[chan][1] = c.g, dst[chan][2] = c.b, setf[chan] = 1;
        break;
    case GX_ALPHA0: case GX_ALPHA1:
        dst[chan - GX_ALPHA0][3] = c.a, setf[chan - GX_ALPHA0] = 1;
        break;
    case GX_COLOR0A0: case GX_COLOR1A1: {
        const int i = chan - GX_COLOR0A0;
        dst[i][0] = c.r, dst[i][1] = c.g, dst[i][2] = c.b, dst[i][3] = c.a, setf[i] = 1;
        break;
    }
    default: break;
    }
}

// chargement par indice (commande 0x38 des listes) : 16 mots de la lumiere, dans l'ordre du materiel
void light_word(int i, int w, u32 bits)
{
    Light& l = L.light[i];
    float f;
    std::memcpy(&f, &bits, 4);
    if (w == 3) l.color[0] = bits >> 24, l.color[1] = bits >> 16, l.color[2] = bits >> 8, l.color[3] = bits;
    else if (w >= 4 && w < 7) l.a[w - 4] = f;
    else if (w >= 7 && w < 10) l.k[w - 7] = f;
    else if (w >= 10 && w < 13) l.pos[w - 10] = f;
    else if (w >= 13) l.dir[w - 13] = f;
    l.set = 1;
}

u32 expect_ctrl(const ChanCtrl& c)
{
    return (u32) (c.mat_src & 1) | (u32) (c.enable & 1) << 1 | (u32) (c.light_mask & 0xF) << 2 | (u32) (c.amb_src & 1) << 6 |
           (u32) ((c.attn_fn == 0 ? 0 : c.diff_fn) & 3) << 7 | (u32) (c.attn_fn != 2) << 9 | (u32) (c.attn_fn != 0) << 10 |
           (u32) ((c.light_mask >> 4) & 0xF) << 11;
}
inline u32 rgba(const u8* c) { return (u32) c[0] << 24 | (u32) c[1] << 16 | (u32) c[2] << 8 | c[3]; }

void compare3(const unsigned* bp, const unsigned* xf, const unsigned char* ok)
{
    cmp(2, 0x00, (u32) (L.n_chans & 7) << 4, bp[0x00], 7u << 4, "nombre de canaux");
    if (ok[0x1009]) cmp(1, 0x09, L.n_chans, xf[0x1009], ~0u, "nombre de canaux");
    for (int i = 0; i < 2; ++i) {
        if (L.amb_set[i] && ok[0x100A + i]) cmp(1, 0x0A + i, rgba(L.amb[i]), xf[0x100A + i], ~0u, "couleur ambiante");
        if (L.mat_set[i] && ok[0x100C + i]) cmp(1, 0x0C + i, rgba(L.mat[i]), xf[0x100C + i], ~0u, "couleur de matiere");
    }
    for (int i = 0; i < 4; ++i)
        if (L.ctrl[i].set && ok[0x100E + i]) cmp(1, 0x0E + i, expect_ctrl(L.ctrl[i]), xf[0x100E + i], ~0u, "canal");
    for (int i = 0; i < 8; ++i) {
        const Light& l = L.light[i];
        if (!l.set) continue;
        const unsigned b = 0x600 + 16 * i;
        u32 w[16] = {0, 0, 0, rgba(l.color)};
        for (int k = 0; k < 3; ++k) w[4 + k] = fbits(l.a[k]), w[7 + k] = fbits(l.k[k]), w[10 + k] = fbits(l.pos[k]), w[13 + k] = fbits(l.dir[k]);
        for (int k = 3; k < 16; ++k)
            if (ok[b + k]) cmp(1, b + k, w[k], xf[b + k], ~0u, "lumiere");
            else mismatch(1, b + k, w[k], 0, "lumiere connue du natif seul");
    }
}

void control3(const unsigned* xf)
{
    int n = 0;
    for (int i = 0; i < 8; ++i) n += L.light[i].set;
    OSReport("[GXN] controle lumieres : canaux %u ; canal 0 natif %05x flux %05x ; ambiante 0 natif %08x flux %08x ;"
             " matiere 0 natif %08x flux %08x ; lumieres chargees %d ; lumiere 0 position x natif %08x flux %08x\n",
             (unsigned) L.n_chans, (unsigned) expect_ctrl(L.ctrl[0]), xf[0x100E], (unsigned) rgba(L.amb[0]), xf[0x100A],
             (unsigned) rgba(L.mat[0]), xf[0x100C], n, (unsigned) fbits(L.light[0].pos[0]), xf[0x60A]);
}

}  // namespace

extern "C" {

REAL(void, GXSetNumChans, (u8))
REAL(void, GXSetChanCtrl, (GXChannelID, GXBool, GXColorSrc, GXColorSrc, u32, GXDiffuseFn, GXAttnFn))
REAL(void, GXSetChanAmbColor, (GXChannelID, GXColor))
REAL(void, GXSetChanMatColor, (GXChannelID, GXColor))
REAL(void, GXLoadLightObjImm, (const GXLightObj*, GXLightID))

void __wrap_GXSetNumChans(u8 n)
{
    if (on()) note("GXSetNumChans"), L.n_chans = n;
    __real_GXSetNumChans(n);
}
void __wrap_GXSetChanCtrl(GXChannelID chan, GXBool en, GXColorSrc amb, GXColorSrc mat, u32 mask, GXDiffuseFn diff, GXAttnFn attn)
{
    if (on()) {
        note("GXSetChanCtrl");
        const ChanCtrl c = {(u8) en, (u8) amb, (u8) mat, (u8) diff, (u8) attn, 1, (u8) mask};
        if (chan == GX_COLOR0A0) L.ctrl[0] = c, L.ctrl[2] = c;
        else if (chan == GX_COLOR1A1) L.ctrl[1] = c, L.ctrl[3] = c;
        else if (chan < 4) L.ctrl[chan] = c;
    }
    __real_GXSetChanCtrl(chan, en, amb, mat, mask, diff, attn);
}
void __wrap_GXSetChanAmbColor(GXChannelID chan, GXColor c)
{
    if (on()) note("GXSetChanAmbColor"), set_color(L.amb, L.amb_set, chan, c);
    __real_GXSetChanAmbColor(chan, c);
}
void __wrap_GXSetChanMatColor(GXChannelID chan, GXColor c)
{
    if (on()) note("GXSetChanMatColor"), set_color(L.mat, L.mat_set, chan, c);
    __real_GXSetChanMatColor(chan, c);
}
void __wrap_GXLoadLightObjImm(const GXLightObj* obj, GXLightID id)
{
    if (on()) {
        note("GXLoadLightObjImm");
        int i = 0;   // comme ConvLightID2Num : un seul bit, sinon lumiere 0
        for (int b = 0; b < 8; ++b)
            if ((u32) id == (1u << b)) i = b;
        const LightObjInt* o = (const LightObjInt*) obj;
        Light& l = L.light[i];
        l.color[0] = o->color >> 24, l.color[1] = o->color >> 16, l.color[2] = o->color >> 8, l.color[3] = o->color;
        for (int k = 0; k < 3; ++k) l.a[k] = o->a[k], l.k[k] = o->k[k], l.pos[k] = o->lpos[k], l.dir[k] = o->ldir[k];
        l.set = 1;
    }
    __real_GXLoadLightObjImm(obj, id);
}

}  // extern "C"
