// -*- coding: utf-8 -*-
// GX natif, etape 4 : textures. Inclus a la fin de port/gxn.cpp (voir son en-tete).
// Un objet texture natif = ses parametres (image, taille, format, repetition, filtres, niveaux de detail, palette),
// sans memoire de texture (TMEM) ni regions. Pendant le double chemin, la bibliotheque garde le contenu de GXTexObj
// (ses registres) : nos descriptions sont dans une table indexee par l'adresse de l'objet. A l'etape 6, la description
// prendra la place des registres dans l'objet lui-meme.

namespace {

struct TexDesc {
    u32 image;            // adresse de l'image
    u16 w, h;
    u8 fmt, wrap_s, wrap_t, mipmap;
    u8 min_filt, mag_filt;     // GXTexFilter
    u8 lmin, lmax;             // niveaux de detail en 1/16 (quantifies comme la bibliotheque)
    s8 lbias;                  // biais en 1/32
    u8 bias_clamp, edge_lod, aniso;
    u8 ci;                     // indexee (palette)
    u32 tlut_name;
};
struct TlutDesc {
    u32 data;
    u8 fmt;
    u16 n;
};
struct TexEntry {
    u32 key;   // adresse de l'objet du jeu (0 = libre)
    TexDesc d;
    u32 raw[8];   // contenu de l'objet laisse par la bibliotheque : reconnait les copies (cTexChg::move, trans.cpp)
};
struct TlutEntry {
    u32 key;
    TlutDesc d;
};
const int kTexSlots = 8192, kTlutSlots = 1024;
TexEntry g_tex[kTexSlots];
TlutEntry g_tlut_objs[kTlutSlots];
struct Tex {
    TexDesc unit[8];
    u8 unit_set[8];
    TlutDesc tlut[32];      // palettes chargees par nom (GXLoadTlut)
    u8 tlut_set[32];
    u32 last_tlut_data;     // derniere palette chargee (registre de chargement)
    u8 tlut_loaded;
    u32 unknown_obj, table_full, copies;
} T;

void reset4()
{
    std::memset(&T, 0, sizeof T);
    std::memset(g_tex, 0, sizeof g_tex);
    std::memset(g_tlut_objs, 0, sizeof g_tlut_objs);
}

template <class E, int N> E* slot(E (&tab)[N], u32 key, bool create)
{
    u32 h = (key >> 5) * 2654435761u;
    for (int k = 0; k < N; ++k) {
        E& e = tab[(h + k) & (N - 1)];
        if (e.key == key) return &e;
        if (e.key == 0) {
            if (!create) return nullptr;
            e.key = key;
            return &e;
        }
    }
    if (create) ++T.table_full;
    return nullptr;
}

void init_tex(GXTexObj* obj, void* image, u16 w, u16 h, u8 fmt, u8 ws, u8 wt, u8 mip, u8 ci, u32 tlut)
{
    TexEntry* e = slot(g_tex, (u32) obj, true);
    if (!e) return;
    TexDesc& d = e->d;
    std::memset(&d, 0, sizeof d);
    d.image = (u32) image, d.w = w, d.h = h, d.fmt = fmt, d.wrap_s = ws, d.wrap_t = wt, d.mipmap = mip;
    d.mag_filt = GX_LINEAR;
    d.edge_lod = 1;
    if (mip) {
        d.min_filt = (fmt == 8 || fmt == 9 || fmt == 10) ? GX_LIN_MIP_NEAR : GX_LIN_MIP_LIN;
        const u32 big = w > h ? w : h;
        const u32 maxlod = 31 - __builtin_clz(big);
        d.lmax = (u8) (16.0f * maxlod);
    } else {
        d.min_filt = GX_LINEAR;
    }
    d.ci = ci, d.tlut_name = tlut;
}

void keep_raw(const GXTexObj* obj)
{
    TexEntry* e = slot(g_tex, (u32) obj, false);
    if (e) std::memcpy(e->raw, obj, 32);
}

// meme objet ? (GXLoadTexObj reecrit l'octet haut des quatre premiers mots : numero de registre de l'unite)
bool same_obj(const u32* a, const GXTexObj* obj)
{
    u32 b[8];
    std::memcpy(b, obj, 32);
    for (int k = 0; k < 8; ++k)
        if ((a[k] ^ b[k]) & (k < 4 ? 0x00FFFFFFu : ~0u)) return false;
    return true;
}

// Objet charge : sa description, ou celle de l'objet dont il est une copie (le jeu copie des objets entiers).
const TexDesc* resolve(const GXTexObj* obj)
{
    TexEntry* e = slot(g_tex, (u32) obj, false);
    if (e && same_obj(e->raw, obj)) return &e->d;
    for (int i = 0; i < kTexSlots; ++i)
        if (g_tex[i].key && g_tex[i].key != (u32) obj && same_obj(g_tex[i].raw, obj)) {
            ++T.copies;
            TexEntry* n = e ? e : slot(g_tex, (u32) obj, true);
            if (!n) return &g_tex[i].d;
            n->d = g_tex[i].d;
            std::memcpy(n->raw, obj, 32);
            return &n->d;
        }
    return nullptr;
}

u32 expect_mode0(const TexDesc& d)
{
    static const u8 g2h[6] = {0x00, 0x04, 0x01, 0x05, 0x02, 0x06};
    return (d.wrap_s & 3) | (u32) (d.wrap_t & 3) << 2 | (u32) (d.mag_filt == GX_LINEAR) << 4 | (u32) g2h[d.min_filt % 6] << 5 |
           (u32) (d.edge_lod ? 0 : 1) << 8 | (u32) (u8) d.lbias << 9 | (u32) (d.aniso & 3) << 19 | (u32) (d.bias_clamp & 1) << 21;
}
u32 expect_image0(const TexDesc& d) { return ((d.w - 1) & 0x3FF) | ((u32) (d.h - 1) & 0x3FF) << 10 | (u32) (d.fmt & 15) << 20; }

void compare4(const unsigned* bp)
{
    static const u8 m0[8] = {0x80, 0x81, 0x82, 0x83, 0xA0, 0xA1, 0xA2, 0xA3};
    for (int i = 0; i < 8; ++i) {
        if (!T.unit_set[i]) continue;
        const TexDesc& d = T.unit[i];
        const unsigned r = m0[i];
        cmp(2, r, expect_mode0(d), bp[r], 0x3FFFFF, "texture : repetition, filtres, biais");
        cmp(2, r + 4, d.lmin | (u32) d.lmax << 8, bp[r + 4], 0xFFFF, "texture : niveaux de detail");
        cmp(2, r + 8, expect_image0(d), bp[r + 8], 0xFFFFFF, "texture : taille et format");
        cmp(2, r + 0x14, (d.image >> 5) & 0x1FFFFF, bp[r + 0x14], 0x1FFFFF, "texture : adresse de l'image");
        if (d.ci && d.tlut_name < 32 && T.tlut_set[d.tlut_name])
            cmp(2, r + 0x18, (u32) (T.tlut[d.tlut_name].fmt & 3) << 10, bp[r + 0x18], 3u << 10, "texture : format de palette");
    }
    if (T.tlut_loaded) cmp(2, 0x64, ((T.last_tlut_data & 0x3FFFFFFF) >> 5) & 0x1FFFFF, bp[0x64], 0x1FFFFF, "palette : adresse");
    // echelle des coordonnees (calculee par la bibliotheque au dessin) : pour chaque etage TEV actif qui lit une texture,
    // taille de la texture de son unite ; le dernier etage qui utilise une coordonnee l'emporte (meme ordre)
    u32 ts0[8], ts1[8];
    u8 have[8] = {0};
    for (int s = 0; s < P.n_stages && s < 16; ++s) {
        const TevStage& t = P.st[s];
        const u32 tmap = t.map & ~GX_TEX_DISABLE;
        if (tmap == 0xFF || t.coord >= 8 || tmap >= 8 || !T.unit_set[tmap]) continue;
        const TexDesc& d = T.unit[tmap];
        ts0[t.coord] = ((d.w - 1) & 0x3FF) | (u32) (d.wrap_s == 1) << 16;
        ts1[t.coord] = ((d.h - 1) & 0x3FF) | (u32) (d.wrap_t == 1) << 16;
        have[t.coord] = 1;
    }
    for (int c = 0; c < 8; ++c) {
        if (!have[c]) continue;
        cmp(2, 0x30 + 2 * c, ts0[c], bp[0x30 + 2 * c], 0x1FFFF, "echelle de coordonnee S");
        cmp(2, 0x31 + 2 * c, ts1[c], bp[0x31 + 2 * c], 0x1FFFF, "echelle de coordonnee T");
    }
}

void control4(const unsigned* bp)
{
    int n = 0;
    for (int i = 0; i < kTexSlots; ++i) n += g_tex[i].key != 0;
    const TexDesc& d = T.unit[0];
    OSReport("[GXN] controle textures : objets decrits %d, copies reconnues %u, inconnus %u, table pleine %u ; unite 0 taille/format natif %06x flux %06x,"
             " filtres natif %06x flux %06x, image natif %06x flux %06x\n",
             n, (unsigned) T.copies, (unsigned) T.unknown_obj, (unsigned) T.table_full, (unsigned) expect_image0(d), bp[0x88] & 0xFFFFFF,
             (unsigned) expect_mode0(d), bp[0x80] & 0x3FFFFF, (unsigned) ((d.image >> 5) & 0x1FFFFF), bp[0x94] & 0x1FFFFF);
}

}  // namespace

extern "C" {

REAL(void, GXInitTexObj, (GXTexObj*, void*, u16, u16, GXTexFmt, GXTexWrapMode, GXTexWrapMode, u8))
REAL(void, GXInitTexObjCI, (GXTexObj*, void*, u16, u16, GXCITexFmt, GXTexWrapMode, GXTexWrapMode, u8, u32))
REAL(void, GXInitTexObjLOD, (GXTexObj*, GXTexFilter, GXTexFilter, f32, f32, f32, GXBool, GXBool, GXAnisotropy))
REAL(void, GXLoadTexObj, (GXTexObj*, GXTexMapID))
REAL(void, GXInitTlutObj, (GXTlutObj*, void*, GXTlutFmt, u16))
REAL(void, GXLoadTlut, (GXTlutObj*, u32))
REAL(void, GXInvalidateTexAll, (void))
REAL(void*, GXGetTexObjData, (const GXTexObj*))

void __wrap_GXInitTexObj(GXTexObj* obj, void* image, u16 w, u16 h, GXTexFmt fmt, GXTexWrapMode ws, GXTexWrapMode wt, u8 mip)
{
    if (on()) note("GXInitTexObj"), init_tex(obj, image, w, h, (u8) fmt, (u8) ws, (u8) wt, mip, 0, 0);
    __real_GXInitTexObj(obj, image, w, h, fmt, ws, wt, mip);
    if (on()) keep_raw(obj);
}
void __wrap_GXInitTexObjCI(GXTexObj* obj, void* image, u16 w, u16 h, GXCITexFmt fmt, GXTexWrapMode ws, GXTexWrapMode wt, u8 mip,
                           u32 tlut)
{
    if (on()) note("GXInitTexObjCI"), init_tex(obj, image, w, h, (u8) fmt, (u8) ws, (u8) wt, mip, 1, tlut);
    __real_GXInitTexObjCI(obj, image, w, h, fmt, ws, wt, mip, tlut);
    if (on()) keep_raw(obj);
}
void __wrap_GXInitTexObjLOD(GXTexObj* obj, GXTexFilter minf, GXTexFilter magf, f32 min_lod, f32 max_lod, f32 bias, GXBool bclamp,
                            GXBool edge, GXAnisotropy aniso)
{
    if (on()) {
        note("GXInitTexObjLOD");
        TexEntry* e = slot(g_tex, (u32) obj, false);
        if (e) {
            TexDesc& d = e->d;
            // memes bornes et quantification que la bibliotheque
            if (bias < -4.0f) bias = -4.0f;
            else if (bias >= 4.0f) bias = 3.99f;
            d.lbias = (s8) (u8) (32.0f * bias);
            if (min_lod < 0.0f) min_lod = 0.0f;
            else if (min_lod > 10.0f) min_lod = 10.0f;
            if (max_lod < 0.0f) max_lod = 0.0f;
            else if (max_lod > 10.0f) max_lod = 10.0f;
            d.lmin = (u8) (16.0f * min_lod), d.lmax = (u8) (16.0f * max_lod);
            d.min_filt = (u8) minf, d.mag_filt = (u8) magf, d.bias_clamp = bclamp, d.edge_lod = edge, d.aniso = (u8) aniso;
        } else {
            ++T.unknown_obj;
        }
    }
    __real_GXInitTexObjLOD(obj, minf, magf, min_lod, max_lod, bias, bclamp, edge, aniso);
    if (on()) keep_raw(obj);
}
void __wrap_GXLoadTexObj(GXTexObj* obj, GXTexMapID id)
{
    if (on() && id < 8) {
        note("GXLoadTexObj");
        const TexDesc* d = resolve(obj);
        if (d) {
            T.unit[id] = *d, T.unit_set[id] = 1;
        } else {
            T.unit_set[id] = 0;   // objet copie par le jeu sans passer par GXInitTexObj : non compare
            if (++T.unknown_obj <= 8) OSReport("[GXN] objet texture inconnu %p (appelant %p)\n", obj, __builtin_return_address(0));
        }
    }
    __real_GXLoadTexObj(obj, id);
}
void __wrap_GXInitTlutObj(GXTlutObj* obj, void* lut, GXTlutFmt fmt, u16 n)
{
    if (on()) {
        note("GXInitTlutObj");
        TlutEntry* e = slot(g_tlut_objs, (u32) obj, true);
        if (e) e->d.data = (u32) lut, e->d.fmt = (u8) fmt, e->d.n = n;
    }
    __real_GXInitTlutObj(obj, lut, fmt, n);
}
void __wrap_GXLoadTlut(GXTlutObj* obj, u32 name)
{
    if (on()) {
        note("GXLoadTlut");
        TlutEntry* e = slot(g_tlut_objs, (u32) obj, false);
        if (e && name < 32) T.tlut[name] = e->d, T.tlut_set[name] = 1, T.last_tlut_data = e->d.data, T.tlut_loaded = 1;
        else if (!e) ++T.unknown_obj;
    }
    __real_GXLoadTlut(obj, name);
}
void __wrap_GXInvalidateTexAll(void)
{
    if (on()) note("GXInvalidateTexAll");   // memoire de texture : rien a faire en natif
    __real_GXInvalidateTexAll();
}
void* __wrap_GXGetTexObjData(const GXTexObj* obj)
{
    void* r = __real_GXGetTexObjData(obj);
    if (on()) {
        TexEntry* e = slot(g_tex, (u32) obj, false);
        if (e && ((e->d.image ^ (u32) r) & 0x03FFFFE0)) ++st.get_bad, mismatch(6, 0x94, e->d.image, (u32) r, "GXGetTexObjData");
    }
    return r;
}

}  // extern "C"
