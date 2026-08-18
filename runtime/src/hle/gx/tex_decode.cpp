#include "runtime/gx/tex_decode.h"

namespace nj::runtime::gx {

namespace {

// Canal de n bits etendu a 8 par repetition de ses bits (0 -> 0, max -> 255).
inline uint8_t widen(uint32_t v, int n) {
    uint32_t r = 0;
    for (int sh = 8 - n; sh > -n; sh -= n) r |= sh >= 0 ? v << sh : v >> -sh;
    return (uint8_t)r;
}

inline uint32_t field(uint32_t v, int lo, int n) { return (v >> lo) & ((1u << n) - 1u); }

Texel from_565(uint16_t c) {
    return {widen(field(c, 11, 5), 5), widen(field(c, 5, 6), 6), widen(field(c, 0, 5), 5), 255};
}

// RGB5A3 : bit 15 = opaque RGB555, sinon RGB444 avec alpha sur 3 bits.
Texel from_5a3(uint16_t c) {
    if (c & 0x8000) return {widen(field(c, 10, 5), 5), widen(field(c, 5, 5), 5), widen(field(c, 0, 5), 5), 255};
    return {widen(field(c, 8, 4), 4), widen(field(c, 4, 4), 4), widen(field(c, 0, 4), 4), widen(field(c, 12, 3), 3)};
}

// Disposition en tuiles : bloc bw x bh, `bits` par texel.
struct Tiling {
    uint32_t bw, bh, bits;
};

// Position en octets du texel (x, y) dans l'image.
inline uint32_t texel_offset(const Tiling& t, uint32_t x, uint32_t y, uint32_t width) {
    const uint32_t per_row = (width + t.bw - 1) / t.bw;
    const uint32_t tile = (y / t.bh) * per_row + x / t.bw;
    const uint32_t inside = (y % t.bh) * t.bw + x % t.bw;
    return tile * t.bw * t.bh * t.bits / 8 + inside * t.bits / 8;
}

// Disposition utilisee pour le calcul de taille (blocs du materiel : 8x8 pour 4 et 8 bits).
bool size_tiling(uint32_t format, Tiling& t) {
    switch (format) {
    case 0: case 8: case 14: t = {8, 8, 4}; return true;    // I4, C4, CMPR
    case 1: case 2: case 9: t = {8, 8, 8}; return true;     // I8, IA4, C8
    case 3: case 4: case 5: case 10: t = {4, 4, 16}; return true;
    case 6: t = {4, 4, 32}; return true;                    // RGBA8
    default: t = {4, 4, 16}; return false;
    }
}

// Remplit chaque texel de l'image par `fetch(x, y)`.
template <class F>
void each_texel(const TexUnit& st, std::vector<Texel>& out, F fetch) {
    for (uint32_t y = 0; y < st.height; ++y)
        for (uint32_t x = 0; x < st.width; ++x) out[(size_t)y * st.width + x] = fetch(x, y);
}

// CMPR : tuiles de 8x8 faites de 2x2 blocs de 4x4 ; chaque bloc : deux couleurs RGB565 et 16 index
// de 2 bits. c0 > c1 : deux couleurs intermediaires (1/3, 2/3) ; sinon la moyenne et un transparent.
void decode_cmpr(MMU& mem, const TexUnit& st, std::vector<Texel>& out) {
    const uint32_t base = st.data_addr, w = st.width, h = st.height;
    const uint32_t per_row = (w + 7) / 8;
    for (uint32_t ty = 0; ty * 8 < h; ++ty)
        for (uint32_t tx = 0; tx * 8 < w; ++tx)
            for (uint32_t b = 0; b < 4; ++b) {
                const uint32_t at = base + (ty * per_row + tx) * 32 + b * 8;
                const uint16_t c0 = mem.read16(at), c1 = mem.read16(at + 2);
                const uint32_t idx = mem.read32(at + 4);
                Texel pal[4] = {from_565(c0), from_565(c1), {}, {}};
                auto mixc = [&](int k0, int k1, int d) {
                    return Texel{(uint8_t)((k0 * pal[0].r + k1 * pal[1].r) / d), (uint8_t)((k0 * pal[0].g + k1 * pal[1].g) / d),
                                 (uint8_t)((k0 * pal[0].b + k1 * pal[1].b) / d), 255};
                };
                if (c0 > c1) {
                    pal[2] = mixc(2, 1, 3);
                    pal[3] = mixc(1, 2, 3);
                } else {
                    pal[2] = mixc(1, 1, 2);
                    pal[3] = pal[2];
                    pal[3].a = 0;
                }
                const uint32_t ox = tx * 8 + (b & 1) * 4, oy = ty * 8 + (b >> 1) * 4;
                for (uint32_t k = 0; k < 16; ++k) {
                    const uint32_t x = ox + (k & 3), y = oy + (k >> 2);
                    if (x < w && y < h) out[(size_t)y * w + x] = pal[(idx >> (30 - 2 * k)) & 3];
                }
            }
}

}  // namespace

uint32_t tex_bytes(uint32_t width, uint32_t height, uint32_t format) {
    Tiling t;
    size_tiling(format, t);
    return ((width + t.bw - 1) / t.bw) * ((height + t.bh - 1) / t.bh) * t.bw * t.bh * t.bits / 8;
}

Texel palette_entry(uint16_t v, uint32_t tlut_format) {
    if (tlut_format == 0) {   // IA8 : intensite basse, alpha haut
        const uint8_t i = v & 0xFF;
        return {i, i, i, (uint8_t)(v >> 8)};
    }
    return tlut_format == 1 ? from_565(v) : from_5a3(v);
}

bool decode_tex(MMU& mem, const TexUnit& st, const uint8_t* tlut, size_t tlut_size, std::vector<Texel>& out) {
    out.assign((size_t)st.width * st.height, Texel{0, 0, 0, 0});
    const uint32_t a = st.data_addr, w = st.width;
    // Les formats 8 bits se lisent par tuiles de 8x4 (la taille, elle, compte des blocs de 8x8).
    static constexpr Tiling k4{8, 8, 4}, k8{8, 4, 8}, k16{4, 4, 16};
    auto pal = [&](uint32_t index) -> Texel {
        const size_t off = st.tlut_addr + (size_t)index * 2;
        if (off + 1 >= tlut_size) return {255, 0, 255, 255};
        return palette_entry((uint16_t)((tlut[off] << 8) | tlut[off + 1]), st.tlut_fmt);
    };
    auto nibble = [&](uint32_t x, uint32_t y) {
        const uint8_t v = mem.read8(a + texel_offset(k4, x & ~1u, y, w));
        return (uint32_t)((x & 1) ? (v & 0xF) : (v >> 4));
    };
    switch (st.format) {
    case 0:   // I4
        each_texel(st, out, [&](uint32_t x, uint32_t y) {
            const uint8_t i = widen(nibble(x, y), 4);
            return Texel{i, i, i, i};
        });
        return true;
    case 1:   // I8
        each_texel(st, out, [&](uint32_t x, uint32_t y) {
            const uint8_t i = mem.read8(a + texel_offset(k8, x, y, w));
            return Texel{i, i, i, i};
        });
        return true;
    case 2:   // IA4 : alpha haut, intensite basse
        each_texel(st, out, [&](uint32_t x, uint32_t y) {
            const uint8_t v = mem.read8(a + texel_offset(k8, x, y, w));
            const uint8_t i = widen(v & 0xF, 4);
            return Texel{i, i, i, widen(v >> 4, 4)};
        });
        return true;
    case 3:   // IA8 : alpha puis intensite
        each_texel(st, out, [&](uint32_t x, uint32_t y) {
            const uint32_t o = a + texel_offset(k16, x, y, w);
            const uint8_t i = mem.read8(o + 1);
            return Texel{i, i, i, mem.read8(o)};
        });
        return true;
    case 4:
        each_texel(st, out, [&](uint32_t x, uint32_t y) { return from_565(mem.read16(a + texel_offset(k16, x, y, w))); });
        return true;
    case 5:
        each_texel(st, out, [&](uint32_t x, uint32_t y) { return from_5a3(mem.read16(a + texel_offset(k16, x, y, w))); });
        return true;
    case 6:   // RGBA8 : tuile de 64 octets, AR sur les 32 premiers, GB sur les 32 suivants
        each_texel(st, out, [&](uint32_t x, uint32_t y) {
            const uint32_t tile = ((y / 4) * ((w + 3) / 4) + x / 4) * 64, in = ((y % 4) * 4 + x % 4) * 2;
            const uint16_t ar = mem.read16(a + tile + in), gb = mem.read16(a + tile + 32 + in);
            return Texel{(uint8_t)(ar & 0xFF), (uint8_t)(gb >> 8), (uint8_t)(gb & 0xFF), (uint8_t)(ar >> 8)};
        });
        return true;
    case 8:   // C4
        each_texel(st, out, [&](uint32_t x, uint32_t y) { return pal(nibble(x, y)); });
        return true;
    case 9:   // C8
        each_texel(st, out, [&](uint32_t x, uint32_t y) { return pal(mem.read8(a + texel_offset(k8, x, y, w))); });
        return true;
    case 10:   // C14X2
        each_texel(st, out, [&](uint32_t x, uint32_t y) {
            return pal(mem.read16(a + texel_offset(k16, x, y, w)) & 0x3FFF);
        });
        return true;
    case 14:
        decode_cmpr(mem, st, out);
        return true;
    default:
        out.assign(out.size(), Texel{255, 0, 255, 255});
        return false;
    }
}

}  // namespace nj::runtime::gx
