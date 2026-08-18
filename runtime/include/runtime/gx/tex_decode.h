#pragma once
// Images de texture du GX en memoire invitee -> RGBA 8 bits, lignes de haut en bas.
// Formats : 0 I4, 1 I8, 2 IA4, 3 IA8, 4 RGB565, 5 RGB5A3, 6 RGBA8, 8 C4, 9 C8, 10 C14X2,
// 14 CMPR. Les texels sont ranges par tuiles (8x8, 8x4 ou 4x4 selon le format), tuiles de gauche
// a droite puis de haut en bas.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "runtime/cpu_context.h"
#include "runtime/gx_state.h"

namespace nj::runtime::gx {

struct Texel {
    uint8_t r, g, b, a;
};

// Octets occupes par une image (tuiles entieres).
uint32_t tex_bytes(uint32_t width, uint32_t height, uint32_t format);

// Decode l'image decrite par `st` (adresse, taille, format, palette). `tlut` : memoire des
// palettes. Faux pour un format inconnu (image magenta).
bool decode_tex(MMU& mem, const TexUnit& st, const uint8_t* tlut, size_t tlut_size, std::vector<Texel>& out);

// Entree de palette (format 0 IA8, 1 RGB565, 2 RGB5A3).
Texel palette_entry(uint16_t v, uint32_t tlut_format);

}  // namespace nj::runtime::gx
