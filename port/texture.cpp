// -*- coding: utf-8 -*-
// Formats de texture du GameCube -> RGBA8 -> textures OpenGL ES.
//
// Le decodeur est la transcription **exacte** de tools/re4tpl.py du depot RE4444, qui a decode
// des milliers de textures reelles du jeu et dont chaque piege a deja ete paye :
//   * la taille de tuile depend du format et n'est pas toujours 8x8 (I8 et IA4 sont 8x4) ;
//     l'erreur dedouble l'image horizontalement sans rien casser d'autre ;
//   * RGBA8 n'est pas fait de quadruplets : une tuile 4x4 tient en 64 octets, 32 de paires AR
//     puis 32 de paires GB. Lu en quadruplets, le resultat reste lisible mais raye de rose.
// Transcrire plutot que reinventer est ici le choix le moins risque, et cela rend le portage
// verifiable octet a octet contre le Python (voir port/test_texture.cpp).
#include <GLES3/gl3.h>

#include <cstdio>
#include <cstring>

#include <dolphin.h>

#include "port_log.h"

namespace {

// ---------------------------------------------------------------- conversions elementaires

inline void rgb565(unsigned short c, unsigned char *out)
{
    const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    out[0] = (unsigned char)(r << 3 | r >> 2);
    out[1] = (unsigned char)(g << 2 | g >> 4);
    out[2] = (unsigned char)(b << 3 | b >> 2);
}

// RGB5A3 : le bit de poids fort decide du sens des 15 autres. Opaque en 5-5-5, translucide en
// 3-4-4-4. C'est le format des textures a fondu de RE4.
inline void rgb5a3(unsigned short c, unsigned char *p)
{
    if (c & 0x8000) {
        const int r = (c >> 10) & 31, g = (c >> 5) & 31, b = c & 31;
        p[0] = (unsigned char)(r << 3 | r >> 2);
        p[1] = (unsigned char)(g << 3 | g >> 2);
        p[2] = (unsigned char)(b << 3 | b >> 2);
        p[3] = 255;
    } else {
        const int a = (c >> 12) & 7, r = (c >> 8) & 15, g = (c >> 4) & 15, b = c & 15;
        p[0] = (unsigned char)(r * 17);
        p[1] = (unsigned char)(g * 17);
        p[2] = (unsigned char)(b * 17);
        p[3] = (unsigned char)(a * 255 / 7);
    }
}

inline unsigned short be16(const unsigned char *d) { return (unsigned short)((d[0] << 8) | d[1]); }

inline unsigned int be32(const unsigned char *d)
{
    return ((unsigned int)d[0] << 24) | ((unsigned int)d[1] << 16) |
           ((unsigned int)d[2] << 8) | (unsigned int)d[3];
}

struct Out {
    unsigned char *px;
    int w, h;
    void put(int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        if (x < 0 || y < 0 || x >= w || y >= h)
            return;
        unsigned char *p = px + (y * w + x) * 4;
        p[0] = r; p[1] = g; p[2] = b; p[3] = a;
    }
};

// Palette deja depliee en RGBA par l'appelant : 4 octets par entree.
struct Tlut {
    const unsigned char *rgba;
    int count;
};

void decode_cmpr(const unsigned char *d, Out &o)
{
    const int bw = (o.w + 7) / 8 * 8, bh = (o.h + 7) / 8 * 8;
    const unsigned char *p = d;
    for (int by = 0; by < bh; by += 8)
        for (int bx = 0; bx < bw; bx += 8)
            for (int sub = 0; sub < 4; ++sub, p += 8) {
                const unsigned short c0 = be16(p), c1 = be16(p + 2);
                const unsigned int bits = be32(p + 4);
                unsigned char a[3], b[3], pal[4][4];
                rgb565(c0, a);
                rgb565(c1, b);
                for (int k = 0; k < 3; ++k) { pal[0][k] = a[k]; pal[1][k] = b[k]; }
                pal[0][3] = pal[1][3] = 255;
                if (c0 > c1) {
                    for (int k = 0; k < 3; ++k) {
                        pal[2][k] = (unsigned char)((2 * a[k] + b[k]) / 3);
                        pal[3][k] = (unsigned char)((a[k] + 2 * b[k]) / 3);
                    }
                    pal[2][3] = pal[3][3] = 255;
                } else {
                    for (int k = 0; k < 3; ++k) {
                        pal[2][k] = (unsigned char)((a[k] + b[k]) / 2);
                        pal[3][k] = 0;
                    }
                    pal[2][3] = 255;
                    pal[3][3] = 0;   // c'est ici que vit la decoupe alpha 1 bit de CMPR
                }
                const int sx = bx + (sub % 2) * 4, sy = by + (sub / 2) * 4;
                for (int yy = 0; yy < 4; ++yy)
                    for (int xx = 0; xx < 4; ++xx) {
                        const int idx = (bits >> (30 - 2 * (yy * 4 + xx))) & 3;
                        o.put(sx + xx, sy + yy, pal[idx][0], pal[idx][1], pal[idx][2], pal[idx][3]);
                    }
            }
}

void decode_ci(int fmt, const unsigned char *d, Out &o, const Tlut &t)
{
    const int tw = (fmt == GX_TF_C14X2) ? 4 : 8;
    const int th = (fmt == GX_TF_C4) ? 8 : 4;
    const int bw = (o.w + tw - 1) / tw * tw, bh = (o.h + th - 1) / th * th;
    const unsigned char *p = d;
    for (int by = 0; by < bh; by += th)
        for (int bx = 0; bx < bw; bx += tw, p += 32)
            for (int yy = 0; yy < th; ++yy)
                for (int xx = 0; xx < tw; ++xx) {
                    const int k = yy * tw + xx;
                    int idx;
                    if (fmt == GX_TF_C4)
                        idx = (k % 2 == 0) ? (p[k / 2] >> 4) : (p[k / 2] & 15);
                    else if (fmt == GX_TF_C8)
                        idx = p[k];
                    else
                        idx = be16(p + 2 * k) & 0x3FFF;
                    // Un index hors palette rend un magenta franc plutot qu'un noir : une palette
                    // mal chargee se voit alors tout de suite, au lieu de passer pour de l'ombre.
                    if (idx < t.count) {
                        const unsigned char *c = t.rgba + 4 * idx;
                        o.put(bx + xx, by + yy, c[0], c[1], c[2], c[3]);
                    } else {
                        o.put(bx + xx, by + yy, 255, 0, 255, 255);
                    }
                }
}

} // namespace

// Decode une image GX en RGBA8. out doit faire w*h*4 octets.
// Rendu public parce que le harnais le compare directement au decodeur Python de reference.
extern "C" void port_tex_decode(int fmt, const void *data, int w, int h,
                                const void *tlut_rgba, int tlut_count, void *out)
{
    Out o = { (unsigned char *)out, w, h };
    std::memset(out, 0, (size_t)w * h * 4);
    const unsigned char *d = (const unsigned char *)data;

    if (fmt == GX_TF_CMPR) {
        decode_cmpr(d, o);
        return;
    }
    if (fmt == GX_TF_C4 || fmt == GX_TF_C8 || fmt == GX_TF_C14X2) {
        const Tlut t = { (const unsigned char *)tlut_rgba, tlut_rgba ? tlut_count : 0 };
        decode_ci(fmt, d, o, t);
        return;
    }

    // Taille de tuile par format : I4 8x8, I8 et IA4 8x4, tout ce qui fait 16 ou 32 bits 4x4.
    static const int TW[] = { 8, 8, 8, 4, 4, 4, 4 };
    static const int TH[] = { 8, 4, 4, 4, 4, 4, 4 };
    if (fmt < 0 || fmt > 6) {
        PORT_TODO("port_tex_decode : format inconnu");
        return;
    }
    const int tw = TW[fmt], th = TH[fmt];
    const int bw = (w + tw - 1) / tw * tw, bh = (h + th - 1) / th * th;
    const int step = (fmt == GX_TF_RGBA8) ? 64 : 32;
    const unsigned char *p = d;

    for (int by = 0; by < bh; by += th)
        for (int bx = 0; bx < bw; bx += tw, p += step)
            for (int yy = 0; yy < th; ++yy)
                for (int xx = 0; xx < tw; ++xx) {
                    const int k = yy * tw + xx;
                    const int x = bx + xx, y = by + yy;
                    switch (fmt) {
                    case GX_TF_I4: {
                        int v = (k % 2 == 0) ? (p[k / 2] >> 4) : (p[k / 2] & 15);
                        v *= 17;
                        o.put(x, y, (unsigned char)v, (unsigned char)v, (unsigned char)v, 255);
                        break;
                    }
                    case GX_TF_I8:
                        o.put(x, y, p[k], p[k], p[k], 255);
                        break;
                    case GX_TF_IA4: {
                        const unsigned char a = (unsigned char)((p[k] >> 4) * 17);
                        const unsigned char i = (unsigned char)((p[k] & 15) * 17);
                        o.put(x, y, i, i, i, a);
                        break;
                    }
                    case GX_TF_IA8: {
                        const unsigned char *q = p + 2 * k;   // alpha d'abord, puis intensite
                        o.put(x, y, q[1], q[1], q[1], q[0]);
                        break;
                    }
                    case GX_TF_RGB565: {
                        unsigned char c[3];
                        rgb565(be16(p + 2 * k), c);
                        o.put(x, y, c[0], c[1], c[2], 255);
                        break;
                    }
                    case GX_TF_RGB5A3: {
                        unsigned char c[4];
                        rgb5a3(be16(p + 2 * k), c);
                        o.put(x, y, c[0], c[1], c[2], c[3]);
                        break;
                    }
                    case GX_TF_RGBA8: {
                        // Deux moities dans la tuile : 32 octets de paires AR, puis 32 de GB.
                        const unsigned char *q = p + 2 * k;
                        o.put(x, y, q[1], q[32], q[33], q[0]);
                        break;
                    }
                    default:
                        break;
                    }
                }
}

// Deplie une palette (IA8 / RGB565 / RGB5A3, en big endian) en RGBA8.
extern "C" void port_tlut_expand(int tlut_fmt, const void *lut, int n, void *out)
{
    const unsigned char *d = (const unsigned char *)lut;
    unsigned char *o = (unsigned char *)out;
    for (int i = 0; i < n; ++i, o += 4) {
        const unsigned short c = be16(d + 2 * i);
        if (tlut_fmt == GX_TL_IA8) {
            const unsigned char v = (unsigned char)(c & 0xFF);
            o[0] = o[1] = o[2] = v;
            o[3] = (unsigned char)(c >> 8);
        } else if (tlut_fmt == GX_TL_RGB565) {
            rgb565(c, o);
            o[3] = 255;
        } else {
            rgb5a3(c, o);
        }
    }
}
