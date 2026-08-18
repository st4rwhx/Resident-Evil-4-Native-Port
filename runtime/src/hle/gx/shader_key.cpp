// Cle du cache de nuanceurs (gx_state.h, GxRegs::GetShaderHash) : FNV-1a 64 bits sur ce que le
// generateur de nuanceurs lit, et sur rien d'autre.
#include "runtime/gx_state.h"
#include "runtime/gfx_settings.h"

#include <cstring>

namespace nj::runtime::gx {
namespace {

struct KeyHasher {
  uint64_t h = 0xCBF29CE484222325ull;
  void byte(uint8_t b) { h = (h ^ b) * 0x100000001B3ull; }
  void word(uint32_t w) {   // petit-boutiste
    for (int s = 0; s < 32; s += 8) byte(uint8_t(w >> s));
  }
  void bytes(const void* p, size_t n) {
    const auto* b = static_cast<const uint8_t*>(p);
    while (n--) byte(*b++);
  }
};

}  // namespace

uint64_t GxRegs::GetShaderHash(uint8_t) const {
  KeyHasher k;

  // Etages TEV (le generateur en force au moins un) et leur selection KONST, un registre pour
  // deux etages.
  const int stages = n_tev == 0 ? 1 : (n_tev > 16 ? 16 : n_tev);
  k.byte(uint8_t(stages));
  for (int s = 0; s < stages; ++s) {
    k.bytes(&tev[s], sizeof(TevStage));
    if (s % 2 == 0) k.word(bp[0xF6 + s / 2]);
    k.byte(uint8_t(bp[0xC1 + 2 * s] & 0xF));   // tables de permutation choisies (ras, texture)
  }
  // les 4 tables de permutation (bits 0-3 des 8 registres KSEL), quel que soit le nombre d'etages
  for (int r = 0; r < 8; ++r) k.byte(uint8_t((bp[0xF6 + r] & 0xF) | ((bp[0xF6 + r] >> 31) << 4)));

  // Brouillard : type et projection seulement (bits 20-23) ; le reste est en uniformes.
  k.word(nj::runtime::gfx::g_no_fog ? 0u : (bp[0xF1] & 0x00F00000u));   // brouillard coupe (21/10) : programme sans

  // Test alpha : comparateurs et operation ; les references sont un uniforme (uAlphaRef).
  k.byte(atest.cmp[0]);
  k.byte(atest.cmp[1]);
  k.byte(atest.op);

  // Generateurs de coordonnees : nombre, TEXMTXINFO (projection, forme, type, source), dual
  // texture et normalisation. Les matrices sont des uniformes.
  const int gens = n_texgen > 8 ? 8 : n_texgen;
  const bool dual = (xf[0x12] & 1) != 0;
  k.byte(uint8_t(gens));
  k.byte(dual ? 1 : 0);
  for (int g = 0; g < gens; ++g) {
    k.word(xf[0x40 + g] & 0x3FFFEu);
    if (dual) k.byte(uint8_t((xf[0x50 + g] >> 8) & 1));
  }

  // Textures indirectes : nombre d'etages (GEN_MODE bits 16-18), ordre, echelles, et commande
  // indirecte de chaque etage TEV. Les matrices sont des uniformes.
  const uint32_t ind = (bp[0x00] >> 16) & 7;
  k.byte(uint8_t(ind));
  if (ind != 0) {
    k.word(bp[0x27] & 0xFFFFFFu);
    k.word(bp[0x25] & 0xFFFFu);
    k.word(bp[0x26] & 0xFFFFu);
    for (int s = 0; s < stages; ++s) k.word(bp[0x10 + s] & 0x1FFFFFu);
  }
  return k.h;
}

}  // namespace nj::runtime::gx
