// -*- coding: utf-8 -*-
// Les objets texture de GX, poses sur des textures OpenGL ES.
//
// GX ne "cree" pas de texture : le jeu decrit une zone de memoire (GXInitTexObj) puis la rend
// courante (GXLoadTexObj), et le materiel lit les tuiles a la volee. GLES veut au contraire un
// objet possede par le pilote. Le portage fait donc la conversion au premier usage et la garde
// en cache, sinon chaque salle redecoderait ses textures a chaque image.
//
// GXTexObj fait 32 octets dans l'en-tete du jeu : la structure ci-dessous doit y tenir, y
// compris avec des pointeurs 64 bits (iOS), d'ou le static_assert.
#include <GLES3/gl3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dolphin.h>

#include "port_log.h"

extern "C" void port_tex_decode(int fmt, const void *data, int w, int h,
                                const void *tlut_rgba, int tlut_count, void *out);
extern "C" void port_tlut_expand(int tlut_fmt, const void *lut, int n, void *out);

namespace {

struct PortTexObj {
    void *img;
    unsigned short w, h;
    unsigned int fmt;
    unsigned char wrap_s, wrap_t, min_filt, mag_filt;
    unsigned int tlut_name;
    unsigned int magic;
};
static_assert(sizeof(PortTexObj) <= sizeof(GXTexObj), "PortTexObj deborde de GXTexObj");

struct PortTlutObj {
    void *lut;
    unsigned short n;
    unsigned short fmt;
};
static_assert(sizeof(PortTlutObj) <= sizeof(GXTlutObj), "PortTlutObj deborde de GXTlutObj");

const unsigned int TEXOBJ_MAGIC = 0x52453454u;   // "RE4T"

// Une palette chargee par le jeu, rangee par nom de TLUT comme le fait le materiel.
const int MAX_TLUTS = 32;
struct TlutSlot {
    unsigned char rgba[16384 * 4];
    int count;
    bool used;
};
TlutSlot *g_tluts[MAX_TLUTS] = { nullptr };

// Cache des textures deja televersees. Pas de conteneur standard ici : l'operateur new global
// du decompile casse libc++, piege deja paye dans port/dvd.cpp.
const int MAX_CACHE = 2048;
struct CacheEntry {
    const void *img;
    unsigned int fmt, tlut_name;
    unsigned short w, h;
    GLuint gl;
};
CacheEntry g_cache[MAX_CACHE];
int g_ncache = 0;

GLuint g_bound[8] = { 0 };
unsigned char g_bound_wrap[8][2] = { { 0 } };

GLenum gl_wrap(unsigned char m)
{
    switch (m) {
    case GX_CLAMP:  return GL_CLAMP_TO_EDGE;
    case GX_MIRROR: return GL_MIRRORED_REPEAT;
    default:        return GL_REPEAT;
    }
}

// GX_NEAR / GX_LINEAR valent 0 et 1 ; les quatre modes mip suivent. Sans mipmaps televerses,
// un filtre mip demanderait des niveaux absents et rendrait du noir : on retombe sur le
// filtrage simple, ce qui est visuellement correct et jamais faux.
GLenum gl_filter(unsigned char f)
{
    return (f == GX_NEAR || f == GX_NEAR_MIP_NEAR || f == GX_NEAR_MIP_LIN) ? GL_NEAREST : GL_LINEAR;
}

GLuint upload(const PortTexObj *t)
{
    for (int i = 0; i < g_ncache; ++i) {
        const CacheEntry &c = g_cache[i];
        if (c.img == t->img && c.fmt == t->fmt && c.w == t->w && c.h == t->h &&
            c.tlut_name == t->tlut_name)
            return c.gl;
    }

    const int w = t->w, h = t->h;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) {
        PORT_TODO("GXLoadTexObj : dimensions hors bornes");
        return 0;
    }

    unsigned char *rgba = (unsigned char *)std::malloc((size_t)w * h * 4);
    if (!rgba)
        return 0;

    const TlutSlot *ts = (t->tlut_name < MAX_TLUTS) ? g_tluts[t->tlut_name] : nullptr;
    port_tex_decode((int)t->fmt, t->img, w, h,
                    ts ? ts->rgba : nullptr, ts ? ts->count : 0, rgba);

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    std::free(rgba);

    if (g_ncache < MAX_CACHE) {
        CacheEntry &c = g_cache[g_ncache++];
        c.img = t->img;
        c.fmt = t->fmt;
        c.w = (unsigned short)w;
        c.h = (unsigned short)h;
        c.tlut_name = t->tlut_name;
        c.gl = id;
    }
    return id;
}

} // namespace

// ---------------------------------------------------------------- construction des objets

void GXInitTexObj(GXTexObj *obj, void *image_ptr, u16 width, u16 height, GXTexFmt format,
                  GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, u8 mipmap)
{
    (void)mipmap;
    PortTexObj *t = (PortTexObj *)obj;
    std::memset(t, 0, sizeof(GXTexObj));
    t->img = image_ptr;
    t->w = width;
    t->h = height;
    t->fmt = (unsigned int)format;
    t->wrap_s = (unsigned char)wrap_s;
    t->wrap_t = (unsigned char)wrap_t;
    t->min_filt = GX_LINEAR;
    t->mag_filt = GX_LINEAR;
    t->tlut_name = 0xFFFFFFFFu;
    t->magic = TEXOBJ_MAGIC;
}

void GXInitTexObjCI(GXTexObj *obj, void *image_ptr, u16 width, u16 height, GXCITexFmt format,
                    GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, u8 mipmap, u32 tlut_name)
{
    GXInitTexObj(obj, image_ptr, width, height, (GXTexFmt)format, wrap_s, wrap_t, mipmap);
    ((PortTexObj *)obj)->tlut_name = tlut_name;
}

void GXInitTexObjData(GXTexObj *obj, void *image_ptr) { ((PortTexObj *)obj)->img = image_ptr; }

void GXInitTexObjTlut(GXTexObj *obj, u32 tlut_name) { ((PortTexObj *)obj)->tlut_name = tlut_name; }

void GXInitTexObjWrapMode(GXTexObj *obj, GXTexWrapMode s, GXTexWrapMode t)
{
    ((PortTexObj *)obj)->wrap_s = (unsigned char)s;
    ((PortTexObj *)obj)->wrap_t = (unsigned char)t;
}

void GXInitTexObjFilter(GXTexObj *obj, GXTexFilter min_filt, GXTexFilter mag_filt)
{
    ((PortTexObj *)obj)->min_filt = (unsigned char)min_filt;
    ((PortTexObj *)obj)->mag_filt = (unsigned char)mag_filt;
}

void GXInitTexObjLOD(GXTexObj *obj, GXTexFilter min_filt, GXTexFilter mag_filt,
                     f32 min_lod, f32 max_lod, f32 lod_bias, GXBool bias_clamp,
                     GXBool do_edge_lod, GXAnisotropy max_aniso)
{
    (void)min_lod; (void)max_lod; (void)lod_bias;
    (void)bias_clamp; (void)do_edge_lod; (void)max_aniso;
    GXInitTexObjFilter(obj, min_filt, mag_filt);
}

// ---------------------------------------------------------------- palettes

void GXInitTlutObj(GXTlutObj *tlut_obj, void *lut, GXTlutFmt fmt, u16 n_entries)
{
    PortTlutObj *t = (PortTlutObj *)tlut_obj;
    t->lut = lut;
    t->fmt = (unsigned short)fmt;
    t->n = n_entries;
}

void GXLoadTlut(GXTlutObj *tlut_obj, u32 tlut_name)
{
    const PortTlutObj *t = (const PortTlutObj *)tlut_obj;
    if (tlut_name >= MAX_TLUTS) {
        PORT_TODO("GXLoadTlut : nom de TLUT hors bornes");
        return;
    }
    if (!g_tluts[tlut_name]) {
        g_tluts[tlut_name] = (TlutSlot *)std::calloc(1, sizeof(TlutSlot));
        if (!g_tluts[tlut_name])
            return;
    }
    TlutSlot *s = g_tluts[tlut_name];
    s->count = (t->n > 16384) ? 16384 : t->n;
    port_tlut_expand(t->fmt, t->lut, s->count, s->rgba);
    s->used = true;

    // Une palette rechargee sous le meme nom change ce que valent les textures deja mises en
    // cache pour ce nom : il faut les oublier, sinon une salle garde les couleurs de la
    // precedente. C'est exactement ce que fait le materiel en invalidant son TMEM.
    int k = 0;
    for (int i = 0; i < g_ncache; ++i) {
        if (g_cache[i].tlut_name == tlut_name)
            glDeleteTextures(1, &g_cache[i].gl);
        else
            g_cache[k++] = g_cache[i];
    }
    g_ncache = k;
}

// ---------------------------------------------------------------- liaison

void GXLoadTexObj(GXTexObj *obj, GXTexMapID id)
{
    PortTexObj *t = (PortTexObj *)obj;
    if (t->magic != TEXOBJ_MAGIC) {
        PORT_TODO("GXLoadTexObj : objet non initialise");
        return;
    }
    const int unit = (int)id;
    if (unit < 0 || unit >= 8)
        return;

    const GLuint gl = upload(t);
    g_bound[unit] = gl;
    glActiveTexture((GLenum)(GL_TEXTURE0 + unit));
    glBindTexture(GL_TEXTURE_2D, gl);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)gl_wrap(t->wrap_s));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint)gl_wrap(t->wrap_t));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)gl_filter(t->min_filt));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint)gl_filter(t->mag_filt));
    g_bound_wrap[unit][0] = t->wrap_s;
    g_bound_wrap[unit][1] = t->wrap_t;
    glActiveTexture(GL_TEXTURE0);
}

void GXLoadTexObjPreLoaded(GXTexObj *obj, GXTexRegion *region, GXTexMapID id)
{
    (void)region;
    GXLoadTexObj(obj, id);
}

void GXInvalidateTexAll(void)
{
    for (int i = 0; i < g_ncache; ++i)
        glDeleteTextures(1, &g_cache[i].gl);
    g_ncache = 0;
    for (int i = 0; i < 8; ++i)
        g_bound[i] = 0;
}

void GXInvalidateTexRegion(GXTexRegion *region) { (void)region; GXInvalidateTexAll(); }

// ---------------------------------------------------------------- taille memoire

// La formule du materiel : on arrondit aux tuiles, et on compte les octets par tuile.
// Le jeu s'en sert pour dimensionner ses allocations, donc une valeur trop petite corrompt le
// tas bien avant que le rendu ne soit en cause.
u32 GXGetTexBufferSize(u16 width, u16 height, u32 format, u8 mipmap, u8 max_lod)
{
    int tw = 4, th = 4, bpt = 32;
    switch (format & 0x0F) {
    case GX_TF_I4:   tw = 8; th = 8; bpt = 32; break;
    case GX_TF_CMPR: tw = 8; th = 8; bpt = 32; break;
    case GX_TF_C4:   tw = 8; th = 8; bpt = 32; break;
    case GX_TF_I8:   tw = 8; th = 4; bpt = 32; break;
    case GX_TF_IA4:  tw = 8; th = 4; bpt = 32; break;
    case GX_TF_C8:   tw = 8; th = 4; bpt = 32; break;
    case GX_TF_RGBA8: tw = 4; th = 4; bpt = 64; break;
    default:         tw = 4; th = 4; bpt = 32; break;
    }
    u32 total = 0;
    int w = width, h = height;
    const int levels = mipmap ? ((max_lod > 0) ? max_lod : 1) : 1;
    for (int l = 0; l < levels; ++l) {
        total += (u32)(((w + tw - 1) / tw) * ((h + th - 1) / th) * bpt);
        w = (w > 1) ? w / 2 : 1;
        h = (h > 1) ? h / 2 : 1;
    }
    return total;
}

// Ce que le portage expose a la couche GX pour savoir s'il y a une texture a echantillonner.
extern "C" unsigned int port_gx_bound_texture(int unit)
{
    return (unit >= 0 && unit < 8) ? g_bound[unit] : 0;
}
