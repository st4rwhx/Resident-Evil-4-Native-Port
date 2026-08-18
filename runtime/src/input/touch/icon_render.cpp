#include "input/icon_render.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace nj::icons {
namespace {

uint16_t be16(const std::vector<uint8_t>& d, size_t o) { return o + 2 <= d.size() ? (uint16_t)((d[o] << 8) | d[o + 1]) : 0; }
int16_t bes16(const std::vector<uint8_t>& d, size_t o) { return (int16_t)be16(d, o); }
uint32_t be32(const std::vector<uint8_t>& d, size_t o) {
    return o + 4 <= d.size() ? ((uint32_t)d[o] << 24) | ((uint32_t)d[o + 1] << 16) | ((uint32_t)d[o + 2] << 8) | d[o + 3] : 0;
}

struct Px { uint8_t r, g, b, a; };

Px c565(uint16_t v) {
    const int r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
    return {(uint8_t)((r << 3) | (r >> 2)), (uint8_t)((g << 2) | (g >> 4)), (uint8_t)((b << 3) | (b >> 2)), 255};
}

Px c5a3(uint16_t v) {
    if (v & 0x8000) {
        const int r = (v >> 10) & 31, g = (v >> 5) & 31, b = v & 31;
        return {(uint8_t)((r << 3) | (r >> 2)), (uint8_t)((g << 3) | (g >> 2)), (uint8_t)((b << 3) | (b >> 2)), 255};
    }
    const int a = (v >> 12) & 7, r = (v >> 8) & 15, g = (v >> 4) & 15, b = v & 15;
    return {(uint8_t)(r * 17), (uint8_t)(g * 17), (uint8_t)(b * 17), (uint8_t)((a << 5) | (a << 2) | (a >> 1))};
}

// Decodage d'une image TPL (tuiles GX : I4 / C4 8x8 ; I8, IA4, C8 8x4 ; IA8, RGB565, RGB5A3, RGBA8, C14X2 4x4 ; CMPR 8x8 de 4 sous-blocs).
bool decode_image(const std::vector<uint8_t>& d, size_t off, int w, int h, int fmt, const std::vector<Px>& pal, Image& out) {
    out.w = w;
    out.h = h;
    out.rgba.assign((size_t)w * h * 4, 0);
    auto put = [&](int x, int y, Px p) {
        if (x < w && y < h) {
            uint8_t* q = &out.rgba[((size_t)y * w + x) * 4];
            q[0] = p.r; q[1] = p.g; q[2] = p.b; q[3] = p.a;
        }
    };
    auto at = [&](size_t o) -> uint8_t { return o < d.size() ? d[o] : 0; };
    size_t p = off;
    if (fmt == 6) {   // RGBA8 : blocs 4x4, 16 paires (A,R) puis 16 paires (G,B)
        for (int by = 0; by < h; by += 4)
            for (int bx = 0; bx < w; bx += 4) {
                for (int i = 0; i < 16; ++i)
                    put(bx + i % 4, by + i / 4, {at(p + 2 * i + 1), at(p + 32 + 2 * i), at(p + 32 + 2 * i + 1), at(p + 2 * i)});
                p += 64;
            }
        return true;
    }
    if (fmt == 14) {   // CMPR
        for (int by = 0; by < h; by += 8)
            for (int bx = 0; bx < w; bx += 8)
                for (int sb = 0; sb < 4; ++sb) {
                    const uint16_t c0 = (uint16_t)((at(p) << 8) | at(p + 1)), c1 = (uint16_t)((at(p + 2) << 8) | at(p + 3));
                    const uint32_t bits = ((uint32_t)at(p + 4) << 24) | ((uint32_t)at(p + 5) << 16) | ((uint32_t)at(p + 6) << 8) | at(p + 7);
                    p += 8;
                    const Px a = c565(c0), b = c565(c1);
                    Px q[4] = {a, b, {}, {}};
                    if (c0 > c1) {
                        q[2] = {(uint8_t)((2 * a.r + b.r) / 3), (uint8_t)((2 * a.g + b.g) / 3), (uint8_t)((2 * a.b + b.b) / 3), 255};
                        q[3] = {(uint8_t)((a.r + 2 * b.r) / 3), (uint8_t)((a.g + 2 * b.g) / 3), (uint8_t)((a.b + 2 * b.b) / 3), 255};
                    } else {
                        q[2] = {(uint8_t)((a.r + b.r) / 2), (uint8_t)((a.g + b.g) / 2), (uint8_t)((a.b + b.b) / 2), 255};
                        q[3] = {0, 0, 0, 0};
                    }
                    const int sx = bx + (sb % 2) * 4, sy = by + (sb / 2) * 4;
                    for (int i = 0; i < 16; ++i) put(sx + i % 4, sy + i / 4, q[(bits >> (30 - 2 * i)) & 3]);
                }
        return true;
    }
    int bw = 4, bh = 4;
    switch (fmt) {
    case 0: case 8: bw = 8; bh = 8; break;
    case 1: case 2: case 9: bw = 8; bh = 4; break;
    case 3: case 4: case 5: case 10: bw = 4; bh = 4; break;
    default: return false;
    }
    size_t block = 0;
    switch (fmt) {
    case 0: case 8: block = (size_t)bw * bh / 2; break;
    case 1: case 2: case 9: block = (size_t)bw * bh; break;
    default: block = (size_t)bw * bh * 2; break;
    }
    auto palette = [&](int i) { return i >= 0 && i < (int)pal.size() ? pal[i] : Px{0, 0, 0, 0}; };
    for (int by = 0; by < h; by += bh)
        for (int bx = 0; bx < w; bx += bw) {
            for (int iy = 0; iy < bh; ++iy)
                for (int ix = 0; ix < bw; ++ix) {
                    const size_t k = (size_t)iy * bw + ix;
                    Px px{};
                    switch (fmt) {
                    case 0: { const int v = ((at(p + k / 2) >> (k % 2 == 0 ? 4 : 0)) & 15) * 17; px = {(uint8_t)v, (uint8_t)v, (uint8_t)v, 255}; break; }
                    case 1: { const uint8_t v = at(p + k); px = {v, v, v, 255}; break; }
                    case 2: { const uint8_t v = at(p + k); const int i = (v & 15) * 17; px = {(uint8_t)i, (uint8_t)i, (uint8_t)i, (uint8_t)((v >> 4) * 17)}; break; }
                    case 3: { const uint8_t a = at(p + k * 2), i = at(p + k * 2 + 1); px = {i, i, i, a}; break; }
                    case 4: px = c565((uint16_t)((at(p + k * 2) << 8) | at(p + k * 2 + 1))); break;
                    case 5: px = c5a3((uint16_t)((at(p + k * 2) << 8) | at(p + k * 2 + 1))); break;
                    case 8: px = palette((at(p + k / 2) >> (k % 2 == 0 ? 4 : 0)) & 15); break;
                    case 9: px = palette(at(p + k)); break;
                    case 10: px = palette(((at(p + k * 2) << 8) | at(p + k * 2 + 1)) & 0x3FFF); break;
                    }
                    put(bx + ix, by + iy, px);
                }
            p += block;
        }
    return true;
}

struct Vert { int pos, nrm, uv; };
struct Mesh { int tex; std::vector<Vert> tris; };   // sommets par trois

struct Model {
    std::vector<float> pos;   // x, y, z
    std::vector<float> nrm;
    std::vector<float> uv;
    std::vector<Mesh> meshes;
};

// Modele d'objet (idmXXX.bin) : positions s16 +0x30, normales s16 +0x34 (16384 = 1), UV s16 +0x10 (fraction de 8 bits), materiaux +0x1C.
bool parse_model(const std::vector<uint8_t>& d, Model& m) {
    if (d.size() < 0x40)
        return false;
    const uint32_t uv_off = be32(d, 0x10), uv_end = be32(d, 0x14), mat_off = be32(d, 0x1C), pos_off = be32(d, 0x30), nrm_off = be32(d, 0x34);
    const int nmat = d[0x1B], npos = be16(d, 0x38), nnrm = be16(d, 0x3A);
    if (pos_off + (size_t)npos * 8 > d.size() || nrm_off + (size_t)nnrm * 8 > d.size() || uv_end < uv_off || uv_end > d.size())
        return false;
    for (int i = 0; i < npos; ++i)
        for (int k = 0; k < 3; ++k) m.pos.push_back((float)bes16(d, pos_off + 8 * i + 2 * k));
    for (int i = 0; i < nnrm; ++i)
        for (int k = 0; k < 3; ++k) m.nrm.push_back(bes16(d, nrm_off + 8 * i + 2 * k) / 16384.0f);
    for (uint32_t o = uv_off; o + 4 <= uv_end; o += 4) {
        m.uv.push_back(bes16(d, o) / 256.0f);
        m.uv.push_back(bes16(d, o + 2) / 256.0f);
    }
    size_t o = mat_off;
    for (int mi = 0; mi < nmat; ++mi) {
        if (o + 0x20 > d.size())
            break;
        Mesh mesh;
        mesh.tex = d[o + 0x0C];
        const uint32_t dl = be32(d, o + 0x18);
        size_t p = o + 0x20;
        const size_t end = std::min(d.size(), (size_t)(o + 0x20 + dl));
        while (p + 3 <= end) {
            const uint8_t op = d[p];
            if (op != 0x80 && op != 0x90 && op != 0x98 && op != 0xA0)
                break;
            const int cnt = be16(d, p + 1);
            p += 3;
            if (p + (size_t)cnt * 8 > d.size())
                break;
            std::vector<Vert> vs((size_t)cnt);
            for (int i = 0; i < cnt; ++i)
                vs[i] = {be16(d, p + 8 * i), be16(d, p + 8 * i + 2), be16(d, p + 8 * i + 6)};
            p += (size_t)cnt * 8;
            auto tri = [&](int a, int b, int c) { mesh.tris.push_back(vs[a]); mesh.tris.push_back(vs[b]); mesh.tris.push_back(vs[c]); };
            if (op == 0x98)
                for (int i = 0; i + 2 < cnt; ++i) i % 2 == 0 ? tri(i, i + 1, i + 2) : tri(i + 1, i, i + 2);
            else if (op == 0x90)
                for (int i = 0; i + 2 < cnt; i += 3) tri(i, i + 1, i + 2);
            else if (op == 0x80)
                for (int i = 0; i + 3 < cnt; i += 4) { tri(i, i + 1, i + 2); tri(i, i + 2, i + 3); }
            else
                for (int i = 1; i + 1 < cnt; ++i) tri(0, i, i + 1);
        }
        m.meshes.push_back(std::move(mesh));
        o = (o + 0x20 + dl + 3) & ~(size_t)3;
    }
    return !m.meshes.empty();
}

struct Mat3 { float m[3][3]; };

// Rz * Rx * Ry, angles en degres (idm_render.py : rot).
Mat3 rotation(float yaw, float pitch, float roll) {
    const float k = 3.14159265f / 180.0f;
    const float y = yaw * k, p = pitch * k, r = roll * k;
    const Mat3 Ry = {{{std::cos(y), 0, std::sin(y)}, {0, 1, 0}, {-std::sin(y), 0, std::cos(y)}}};
    const Mat3 Rx = {{{1, 0, 0}, {0, std::cos(p), -std::sin(p)}, {0, std::sin(p), std::cos(p)}}};
    const Mat3 Rz = {{{std::cos(r), -std::sin(r), 0}, {std::sin(r), std::cos(r), 0}, {0, 0, 1}}};
    auto mul = [](const Mat3& a, const Mat3& b) {
        Mat3 c{};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int t = 0; t < 3; ++t) c.m[i][j] += a.m[i][t] * b.m[t][j];
        return c;
    };
    return mul(Rz, mul(Rx, Ry));
}

// Rendu logiciel (idm_render.py : render) : image `size` x `size` en alpha droit, suréchantillonnée `ss` fois.
bool render_model(const Model& m, const std::vector<Image>& texs, int size, int ss, float yaw, float pitch, float roll, Image& out) {
    const int S = size * ss;
    const Mat3 R = rotation(yaw, pitch, roll);
    const size_t np = m.pos.size() / 3, nn = m.nrm.size() / 3, nu = m.uv.size() / 2;
    std::vector<float> P(np * 3), N(nn * 3);
    for (size_t i = 0; i < np; ++i)   // v @ R.T : composante i = ligne i de R . v
        for (int a = 0; a < 3; ++a)
            P[i * 3 + a] = R.m[a][0] * m.pos[i * 3] + R.m[a][1] * m.pos[i * 3 + 1] + R.m[a][2] * m.pos[i * 3 + 2];
    for (size_t i = 0; i < nn; ++i)
        for (int a = 0; a < 3; ++a)
            N[i * 3 + a] = R.m[a][0] * m.nrm[i * 3] + R.m[a][1] * m.nrm[i * 3 + 1] + R.m[a][2] * m.nrm[i * 3 + 2];
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    bool any = false;
    for (const Mesh& me : m.meshes)
        for (const Vert& v : me.tris)
            if ((size_t)v.pos < np) {
                any = true;
                for (int a = 0; a < 3; ++a) {
                    lo[a] = std::min(lo[a], P[v.pos * 3 + a]);
                    hi[a] = std::max(hi[a], P[v.pos * 3 + a]);
                }
            }
    if (!any)
        return false;
    const float cx = (lo[0] + hi[0]) * 0.5f, cy = (lo[1] + hi[1]) * 0.5f;
    const float scale = (S * 0.9f) / std::max(std::max(hi[0] - lo[0], hi[1] - lo[1]), 1e-6f);
    std::vector<float> color((size_t)S * S * 4, 0.0f), zbuf((size_t)S * S, -1e30f);
    float light[3] = {-0.4f, 0.6f, 0.7f};
    const float ll = std::sqrt(light[0] * light[0] + light[1] * light[1] + light[2] * light[2]);
    for (float& l : light) l /= ll;
    for (const Mesh& me : m.meshes) {
        const Image* tex = me.tex < (int)texs.size() ? &texs[me.tex] : (texs.empty() ? nullptr : &texs[0]);
        for (size_t t = 0; t + 2 < me.tris.size(); t += 3) {
            const Vert v[3] = {me.tris[t], me.tris[t + 1], me.tris[t + 2]};
            if ((size_t)v[0].pos >= np || (size_t)v[1].pos >= np || (size_t)v[2].pos >= np)
                continue;
            float sx[3], sy[3], sz[3], tn[3][3], tu[3][2];
            for (int k = 0; k < 3; ++k) {
                sx[k] = (P[v[k].pos * 3] - cx) * scale + S / 2.0f;
                sy[k] = S / 2.0f - (P[v[k].pos * 3 + 1] - cy) * scale;
                sz[k] = P[v[k].pos * 3 + 2];
                for (int a = 0; a < 3; ++a) tn[k][a] = (size_t)v[k].nrm < nn ? N[v[k].nrm * 3 + a] : (a == 2 ? 1.0f : 0.0f);
                tu[k][0] = (size_t)v[k].uv < nu ? m.uv[v[k].uv * 2] : 0.0f;
                tu[k][1] = (size_t)v[k].uv < nu ? m.uv[v[k].uv * 2 + 1] : 0.0f;
            }
            const int x0 = std::max(0, (int)std::floor(std::min({sx[0], sx[1], sx[2]}))), y0 = std::max(0, (int)std::floor(std::min({sy[0], sy[1], sy[2]})));
            const int x1 = std::min(S - 1, (int)std::ceil(std::max({sx[0], sx[1], sx[2]}))), y1 = std::min(S - 1, (int)std::ceil(std::max({sy[0], sy[1], sy[2]})));
            if (x1 < x0 || y1 < y0)
                continue;
            const float ax = sx[0], ay = sy[0], bx = sx[1], by = sy[1], ccx = sx[2], ccy = sy[2];
            const float den = (by - ccy) * (ax - ccx) + (ccx - bx) * (ay - ccy);
            if (std::fabs(den) < 1e-9f)
                continue;
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x) {
                    const float px = x + 0.5f, py = y + 0.5f;
                    const float w0 = ((by - ccy) * (px - ccx) + (ccx - bx) * (py - ccy)) / den;
                    const float w1 = ((ccy - ay) * (px - ccx) + (ax - ccx) * (py - ccy)) / den;
                    const float w2 = 1.0f - w0 - w1;
                    if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f)
                        continue;
                    const float z = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
                    float& zb = zbuf[(size_t)y * S + x];
                    if (!(z > zb))
                        continue;
                    const float u = w0 * tu[0][0] + w1 * tu[1][0] + w2 * tu[2][0], vv = w0 * tu[0][1] + w1 * tu[1][1] + w2 * tu[2][1];
                    float texel[3] = {0.7f, 0.7f, 0.7f};
                    if (tex && tex->w > 0 && tex->h > 0) {   // echantillon bilineaire, texture repetee
                        const float fu = (u - std::floor(u)) * tex->w - 0.5f, fv = (vv - std::floor(vv)) * tex->h - 0.5f;
                        const int i0 = (int)std::floor(fu), j0 = (int)std::floor(fv);
                        const float du = fu - i0, dv = fv - j0;
                        auto sample = [&](int i, int j, int ch) {
                            i = ((i % tex->w) + tex->w) % tex->w;
                            j = ((j % tex->h) + tex->h) % tex->h;
                            return tex->rgba[((size_t)j * tex->w + i) * 4 + ch] / 255.0f;
                        };
                        for (int ch = 0; ch < 3; ++ch)
                            texel[ch] = (sample(i0, j0, ch) * (1 - du) + sample(i0 + 1, j0, ch) * du) * (1 - dv) +
                                        (sample(i0, j0 + 1, ch) * (1 - du) + sample(i0 + 1, j0 + 1, ch) * du) * dv;
                    }
                    float n[3];
                    for (int a = 0; a < 3; ++a) n[a] = w0 * tn[0][a] + w1 * tn[1][a] + w2 * tn[2][a];
                    const float nl = std::max(std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]), 1e-6f);
                    const float lam = std::fabs((n[0] * light[0] + n[1] * light[1] + n[2] * light[2]) / nl);
                    const float shade = 0.45f + 0.75f * lam;
                    float* c = &color[((size_t)y * S + x) * 4];
                    for (int ch = 0; ch < 3; ++ch) c[ch] = std::min(1.0f, std::max(0.0f, texel[ch] * shade));
                    c[3] = 1.0f;
                    zb = z;
                }
        }
    }
    out.w = out.h = size;
    out.rgba.assign((size_t)size * size * 4, 0);
    const float inv = 1.0f / (ss * ss);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            float acc[4] = {0, 0, 0, 0};
            for (int j = 0; j < ss; ++j)
                for (int i = 0; i < ss; ++i)
                    for (int ch = 0; ch < 4; ++ch) acc[ch] += color[((size_t)(y * ss + j) * S + (x * ss + i)) * 4 + ch];
            const float a = acc[3] * inv;
            uint8_t* o = &out.rgba[((size_t)y * size + x) * 4];
            for (int ch = 0; ch < 3; ++ch) o[ch] = (uint8_t)std::min(255.0f, std::max(0.0f, a > 1e-6f ? acc[ch] * inv / a * 255.0f : 0.0f));
            o[3] = (uint8_t)std::min(255.0f, a * 255.0f);
        }
    return true;
}

// Rogne l'image, la met a l'echelle pour que son point le plus eloigne du centre reste dans le cercle du bouton (rayon 0,44 de la case),
// au plus 0,84 de la case, puis la cerne de blanc (touch_action_icons.py : fit_in_circle, outline_on_canvas).
bool place_in_cell(const Image& src, int cell, Image& out) {
    int x0 = src.w, y0 = src.h, x1 = -1, y1 = -1;
    for (int y = 0; y < src.h; ++y)
        for (int x = 0; x < src.w; ++x)
            if (src.rgba[((size_t)y * src.w + x) * 4 + 3] > 16) {
                x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y);
            }
    if (x1 < x0)
        return false;
    const int cw = x1 - x0 + 1, ch = y1 - y0 + 1;
    const float ccx = (cw - 1) / 2.0f, ccy = (ch - 1) / 2.0f;
    float rmax = 1.0f;
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x)
            if (src.rgba[((size_t)(y0 + y) * src.w + (x0 + x)) * 4 + 3] > 16)
                rmax = std::max(rmax, std::hypot(x - ccx, y - ccy));
    const float target = std::min(cell * 0.84f, 0.44f * cell / rmax * std::max(cw, ch));
    const float k = target / (float)std::max(cw, ch);
    const int nw = std::max(1, (int)std::lround(cw * k)), nh = std::max(1, (int)std::lround(ch * k));
    // mise a l'echelle par moyenne de surface (reduction) ou bilineaire
    Image small;
    small.w = nw;
    small.h = nh;
    small.rgba.assign((size_t)nw * nh * 4, 0);
    for (int y = 0; y < nh; ++y)
        for (int x = 0; x < nw; ++x) {
            const float sx0 = x0 + x / k, sx1 = x0 + (x + 1) / k, sy0 = y0 + y / k, sy1 = y0 + (y + 1) / k;
            float acc[4] = {0, 0, 0, 0}, wsum = 0.0f;
            for (int yy = (int)std::floor(sy0); yy < (int)std::ceil(sy1) && yy < src.h; ++yy)
                for (int xx = (int)std::floor(sx0); xx < (int)std::ceil(sx1) && xx < src.w; ++xx) {
                    if (xx < 0 || yy < 0) continue;
                    const float wx = std::min(sx1, xx + 1.0f) - std::max(sx0, (float)xx), wy = std::min(sy1, yy + 1.0f) - std::max(sy0, (float)yy);
                    const float w = std::max(0.0f, wx) * std::max(0.0f, wy);
                    const uint8_t* p = &src.rgba[((size_t)yy * src.w + xx) * 4];
                    const float a = p[3] / 255.0f;
                    acc[0] += p[0] * a * w; acc[1] += p[1] * a * w; acc[2] += p[2] * a * w; acc[3] += a * w;
                    wsum += w;
                }
            uint8_t* o = &small.rgba[((size_t)y * nw + x) * 4];
            if (wsum > 0 && acc[3] > 1e-6f) {
                for (int c = 0; c < 3; ++c) o[c] = (uint8_t)std::min(255.0f, acc[c] / acc[3]);
                o[3] = (uint8_t)std::min(255.0f, acc[3] / wsum * 255.0f);
            }
        }
    out.w = out.h = cell;
    out.rgba.assign((size_t)cell * cell * 4, 0);
    const int px = (int)std::lround(cell / 2.0f - nw / 2.0f), py = (int)std::lround(cell / 2.0f - nh / 2.0f);
    const int rad = std::max(2, cell * 9 / 1024 + 1);   // cerne de 9 px a 1024
    std::vector<uint8_t> mask((size_t)cell * cell, 0);
    for (int y = 0; y < nh; ++y)
        for (int x = 0; x < nw; ++x)
            if (px + x >= 0 && px + x < cell && py + y >= 0 && py + y < cell)
                mask[(size_t)(py + y) * cell + px + x] = small.rgba[((size_t)y * nw + x) * 4 + 3];
    // dilatation (maximum sur le disque de rayon rad) -> cerne blanc
    std::vector<uint8_t> dil((size_t)cell * cell, 0);
    for (int y = 0; y < cell; ++y)
        for (int x = 0; x < cell; ++x) {
            uint8_t mx = 0;
            for (int dy = -rad; dy <= rad; ++dy)
                for (int dx = -rad; dx <= rad; ++dx) {
                    if (dx * dx + dy * dy > rad * rad) continue;
                    const int xx = x + dx, yy = y + dy;
                    if (xx >= 0 && xx < cell && yy >= 0 && yy < cell) mx = std::max(mx, mask[(size_t)yy * cell + xx]);
                }
            dil[(size_t)y * cell + x] = mx;
        }
    for (int y = 0; y < cell; ++y)
        for (int x = 0; x < cell; ++x) {
            uint8_t* o = &out.rgba[((size_t)y * cell + x) * 4];
            o[0] = o[1] = o[2] = 255;
            o[3] = dil[(size_t)y * cell + x];
            const int sx = x - px, sy = y - py;   // l'icone par-dessus le cerne
            if (sx >= 0 && sx < nw && sy >= 0 && sy < nh) {
                const uint8_t* s = &small.rgba[((size_t)sy * nw + sx) * 4];
                const float a = s[3] / 255.0f, ba = o[3] / 255.0f;
                const float oa = a + ba * (1 - a);
                if (oa > 1e-6f) {
                    for (int c = 0; c < 3; ++c) o[c] = (uint8_t)std::min(255.0f, (s[c] * a + o[c] * ba * (1 - a)) / oa);
                    o[3] = (uint8_t)std::min(255.0f, oa * 255.0f);
                }
            }
        }
    return true;
}

}  // namespace

const std::vector<int>& weapon_item_ids() {
    static const std::vector<int> ids = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x03, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x6B, 0x99,
                                         0x2F, 0x6C, 0x51, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x17, 0x6D, 0x36, 0xAB, 0x37, 0x38, 0x01, 0x02,
                                         0x0E, 0x08, 0x09, 0x0A, 0x94, 0x52, 0x3E};
    return ids;
}

bool decode_tpl(const std::vector<uint8_t>& d, std::vector<Image>& out) {
    if (d.size() < 12 || be32(d, 0) != 0x0020AF30u)
        return false;
    const uint32_t n = be32(d, 4), tbl = be32(d, 8);
    if (n > 64 || tbl + (size_t)n * 8 > d.size())
        return false;
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t ih = be32(d, tbl + i * 8), ph = be32(d, tbl + i * 8 + 4);
        if (ih + 12 > d.size()) {
            out.emplace_back();
            continue;
        }
        const int h = be16(d, ih), w = be16(d, ih + 2), fmt = (int)be32(d, ih + 4);
        const uint32_t off = be32(d, ih + 8);
        std::vector<Px> pal;
        if (ph && ph + 12 <= d.size()) {
            const int cnt = be16(d, ph), pfmt = (int)be32(d, ph + 4);
            const uint32_t poff = be32(d, ph + 8);
            for (int k = 0; k < cnt; ++k) {
                const uint16_t v = be16(d, poff + 2 * k);
                pal.push_back(pfmt == 0 ? Px{(uint8_t)(v & 255), (uint8_t)(v & 255), (uint8_t)(v & 255), (uint8_t)(v >> 8)}
                              : pfmt == 1 ? c565(v) : c5a3(v));
            }
        }
        Image im;
        if (w <= 0 || h <= 0 || w > 2048 || h > 2048 || !decode_image(d, off, w, h, fmt, pal, im))
            im = Image{};
        out.push_back(std::move(im));
    }
    return true;
}

bool render_item_icon(const std::vector<uint8_t>& bin, const std::vector<uint8_t>& tpl, int item, int cell, Image& out) {
    Model m;
    if (!parse_model(bin, m))
        return false;
    std::vector<Image> texs;
    decode_tpl(tpl, texs);
    const bool flat = item == 0x29 || item == 0x2A || item == 0x2B;
    const bool front = item == 0x01 || item == 0x02 || item == 0x0E || item == 0x08 || item == 0x09 || item == 0x0A || item == 0x38 ||
                       item == 0x31 || item == 0x32 || item == 0x33;
    const float yaw = flat || front ? 0.0f : 180.0f, pitch = flat ? 90.0f : 0.0f;
    Image big;
    if (!render_model(m, texs, 512, 2, yaw, pitch, 0.0f, big))
        return false;
    return place_in_cell(big, cell, out);
}

int model_vertex_count(const std::vector<uint8_t>& bin) {
    Model m;
    return parse_model(bin, m) ? (int)(m.pos.size() / 3) : -1;
}

// Couteau (21/10) : le couteau n'est pas un objet de la mallette, son modele est dans l'archive d'arme (em/wep16.drs Leon,
// em/wep26.drs Krauser ; l'objet 0x38 est un autre couteau, lame doree). Vue de face, puis miroir : lame a gauche, comme
// les armes du bouton viser.
bool render_knife_icon(const std::vector<uint8_t>& bin, const std::vector<uint8_t>& tpl, int cell, Image& out) {
    Model m;
    if (!parse_model(bin, m))
        return false;
    std::vector<Image> texs;
    decode_tpl(tpl, texs);
    Image big;
    if (!render_model(m, texs, 512, 2, 0.0f, 0.0f, 0.0f, big))
        return false;
    for (int y = 0; y < big.h; ++y)
        for (int x = 0; x < big.w / 2; ++x)
            for (int k = 0; k < 4; ++k)
                std::swap(big.rgba[((size_t)y * big.w + x) * 4 + k], big.rgba[((size_t)y * big.w + (big.w - 1 - x)) * 4 + k]);
    return place_in_cell(big, cell, out);
}

bool render_archive_icon(const std::vector<uint8_t>& bin, const std::vector<uint8_t>& tpl, int cell, float yaw, float pitch, float roll,
                         bool mirror, Image& out) {
    Model m;
    if (!parse_model(bin, m))
        return false;
    std::vector<Image> texs;
    decode_tpl(tpl, texs);
    Image big;
    if (!render_model(m, texs, 512, 2, yaw, pitch, roll, big))
        return false;
    if (mirror)
        for (int y = 0; y < big.h; ++y)
            for (int x = 0; x < big.w / 2; ++x)
                for (int k = 0; k < 4; ++k)
                    std::swap(big.rgba[((size_t)y * big.w + x) * 4 + k], big.rgba[((size_t)y * big.w + (big.w - 1 - x)) * 4 + k]);
    return place_in_cell(big, cell, out);
}

// Mallette (28/10) : la marchande l'ameliore (S, M, L, LL) et l'icone du bouton suit. Les quatre modeles viennent de ss_pzzl.dat du
// disque du joueur ; meme rendu que le couteau, vue de trois quarts (tools/touch_action_icons.py render_cases : rotation 25, -25).
bool render_case_icon(const std::vector<uint8_t>& bin, const std::vector<uint8_t>& tpl, int cell, float yaw, float pitch, Image& out) {
    Model m;
    if (!parse_model(bin, m))
        return false;
    std::vector<Image> texs;
    decode_tpl(tpl, texs);
    Image big;
    if (!render_model(m, texs, 512, 2, yaw, pitch, 0.0f, big))
        return false;
    return place_in_cell(big, cell, out);
}

}  // namespace nj::icons
