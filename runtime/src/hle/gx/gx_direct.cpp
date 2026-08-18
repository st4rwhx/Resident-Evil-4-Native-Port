// Fonctions GX du SDK servies en natif (hle_natives de la config njrecomp) : elles ecrivent dans
// le tuyau GX exactement les octets que le SDK y aurait mis, a partir de leurs arguments. Rien
// n'est applique tout de suite : l'analyseur lit le flux par lots et doit voir ces ecritures a
// leur place, entre les dessins.
//
// Paquets (YAGCD § GX FIFO) : registre BP = 0x61, numero (8 bits), valeur (24 bits) ;
// chargement XF = 0x10, nombre de mots - 1 (16 bits), adresse XF (16 bits), puis les mots.
//
// Ne sont servies ici que les fonctions qui ecrivent un registre ENTIER : celles qui n'en
// possedent qu'une partie (GXSetCullMode, GXSetBlendMode, GXSetCurrentMtx...) recomposent le mot
// depuis la copie que le SDK garde en memoire ; les servir a part ecrasait des champs voisins.
// Elles tournent recompilees.
#include "runtime/cpu_context.h"

namespace {
using nj::runtime::GuestCpu;

void put8(uint8_t v) { nj::runtime::GX_WGPIPE_Write8(v); }
void put16(uint16_t v) { nj::runtime::GX_WGPIPE_Write16(v); }
void put32(uint32_t v) { nj::runtime::GX_WGPIPE_Write32(v); }

void bp(uint8_t reg, uint32_t v) {
  put8(0x61);
  put8(reg);
  put8((uint8_t)(v >> 16));
  put8((uint8_t)(v >> 8));
  put8((uint8_t)v);
}

// `n` mots lus en memoire invitee a `words`, a l'adresse XF `xf`.
void xf_words(GuestCpu &c, uint16_t xf, const uint32_t *offsets, int n, uint32_t words) {
  put8(0x10);
  put16((uint16_t)(n - 1));
  put16(xf);
  for (int i = 0; i < n; ++i) put32(c.mmu.read32(words + offsets[i]));
}

// Matrice 3x4 du SDK (Mtx) : 12 flottants, ligne par ligne.
constexpr uint32_t kMtx34[12] = {0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44};
// Bloc 3x3 en haut a gauche de la meme matrice (matrices des normales).
constexpr uint32_t kMtx33[9] = {0, 4, 8, 16, 20, 24, 32, 36, 40};
}  // namespace

extern "C" {

// GXSetZMode(enable, func, update) -> BP 0x40 : bit 0, bits 1-3, bit 4.
void GXSetZMode(GuestCpu &c) {
  bp(0x40, (c.gpr[3] & 1) | (c.gpr[4] & 7) << 1 | (c.gpr[5] & 1) << 4);
}

// GXSetAlphaCompare(comp0, ref0, op, comp1, ref1) -> BP 0xF3 : ref0 0-7, ref1 8-15, comp0 16-18,
// comp1 19-21, operation 22-23.
void GXSetAlphaCompare(GuestCpu &c) {
  bp(0xF3, (c.gpr[4] & 0xFF) | (c.gpr[7] & 0xFF) << 8 | (c.gpr[3] & 7) << 16 | (c.gpr[6] & 7) << 19 |
               (c.gpr[5] & 3) << 22);
}

// GXSetDstAlpha(enable, alpha) -> BP 0x42 : alpha 0-7, enable 8.
void GXSetDstAlpha(GuestCpu &c) { bp(0x42, (c.gpr[4] & 0xFF) | (c.gpr[3] & 1) << 8); }

// GXSetScissor(x, y, w, h) -> BP 0x20 (haut gauche) et 0x21 (bas droit), y dans les bits 0-10, x
// dans 12-22, chacun decale de 342 (origine du materiel).
void GXSetScissor(GuestCpu &c) {
  const uint32_t x0 = c.gpr[3] + 342, y0 = c.gpr[4] + 342;
  const uint32_t x1 = x0 + c.gpr[5] - 1, y1 = y0 + c.gpr[6] - 1;
  bp(0x20, (y0 & 0x7FF) | (x0 & 0x7FF) << 12);
  bp(0x21, (y1 & 0x7FF) | (x1 & 0x7FF) << 12);
}

// GXLoadPosMtxImm(mtx, id) : 12 mots a l'adresse XF id * 4.
void GXLoadPosMtxImm(GuestCpu &c) { xf_words(c, (uint16_t)((c.gpr[4] & 0x3F) * 4), kMtx34, 12, c.gpr[3]); }

// GXLoadNrmMtxImm(mtx, id) : le 3x3 de la matrice, banc des normales 0x400 + 3 * id.
void GXLoadNrmMtxImm(GuestCpu &c) { xf_words(c, (uint16_t)(0x400 + 3 * (c.gpr[4] & 31)), kMtx33, 9, c.gpr[3]); }

}  // extern "C"
