// VI (interface video, 0xCC002000) : ce que le jeu lit et ecrit vraiment. L'image elle-meme est
// presentee par le rendu (le XFB n'est pas balaye) ; ici restent les adresses des trames, le
// compteur de lignes que le SDK attend et les quatre interruptions d'affichage (DI0..DI3).
// Registres d'apres YAGCD (§ VI) : DI0..3 a 0x30..0x3C, mot haut a +0 (bit 15 : interruption
// levee, bit 12 : autorisee), mot bas a +2.
#include <iostream>
#include "runtime/log.h"

#include "runtime/cpu_context.h"
#include "runtime/env.h"
#include "runtime/hw/hw.h"

namespace nj::runtime::hw {

uint32_t g_vi_top_base = 0;   // TFBL (0x1C), registre entier (mot haut 0x1C, mot bas 0x1E)
uint32_t g_vi_btm_field_base = 0;   // BFBL (0x24 / 0x26)

// Adresse de la trame du haut (videos Sofdec, main.cpp). Le registre n'est pas l'adresse : FBB sur
// les bits 0-23, XOF 24-27, POFF bit 28 = FBB en pas de 32 octets (adresse << 5), que le SDK met
// quand la trame est au-dela de 16 Mo (YAGCD, VI). 12/10 : les disques commerciaux ecrivent le
// registre en deux mots de 16 bits ; seul le haut etait garde, et l'adresse n'etait pas decodee :
// video verte et striee, puis jeu bloque sur un ecran noir.
uint32_t vi_xfb_top_address() {
  const uint32_t fbb = g_vi_top_base & 0x00FFFFFFu;
  return (g_vi_top_base & (1u << 28)) ? (fbb << 5) : fbb;
}

namespace {
constexpr uint32_t kBase = 0xCC002000u, kVIIrq = 0x100;   // cause PI du VI
constexpr uint32_t kIrqFlag = 0x80000000u;
uint32_t display_cfg = 0;     // DCR (0x02)
uint32_t disp_int[4] = {};    // DI0..DI3, 32 bits chacun

void recompute_irq() {
  bool any = false;
  for (uint32_t d : disp_int) any |= (d & kIrqFlag) != 0;
  if (any) raise_irq(kVIIrq);
  else lower_irq(kVIIrq);
}

// Registre DI touche par cet acces : indice 0..3, et vrai pour le mot haut.
bool which_di(uint32_t off, int &idx, bool &high) {
  if (off < 0x30 || off > 0x3E || (off & 1)) return false;
  idx = (int)((off - 0x30) >> 2);
  high = (off & 2) == 0;
  return true;
}

uint32_t vi_read(uint32_t addr) {
  const uint32_t off = addr - kBase;
  int idx;
  bool high;
  if (which_di(off, idx, high)) return high ? disp_int[idx] >> 16 : disp_int[idx] & 0xFFFF;
  switch (off) {
    case 0x02: return display_cfg | 1;   // toujours « active »
    case 0x1C: return g_vi_top_base;
    case 0x24: return g_vi_btm_field_base;
    case 0x2C: return (uint32_t)((guest_time() / 64) % 313) + 1;   // ligne courante, 1..313
    case 0x2E: return 1;                                            // colonne courante
  }
  return 0;
}

void vi_write(uint32_t addr, uint32_t v) {
  const uint32_t off = addr - kBase;
  int idx;
  bool high;
  if (which_di(off, idx, high)) {
    uint32_t &d = disp_int[idx];
    if (high) {
      // bit 15 a 0 : le jeu acquitte ; le reste du mot haut est la configuration
      const uint32_t flag = (v & 0x8000) ? (d & kIrqFlag) : 0;
      d = flag | ((v & 0x7FFF) << 16) | (d & 0xFFFF);
      recompute_irq();
    } else {
      d = (d & 0xFFFF0000u) | (v & 0xFFFF);
    }
    return;
  }
  switch (off) {
    case 0x02: display_cfg = v; break;
    case 0x1C: g_vi_top_base = v; break;
    case 0x24: g_vi_btm_field_base = v; break;
  }
}
}  // namespace

// Retour de trame : DI0 et DI1 se levent (le SDK compte les trames sur l'un, l'autre sert au
// changement de trame), et la cause VI du PI avec eux.
void vi_raise() {
  static const bool trace = NJ_ENV("NJ_VITRACE") != nullptr;
  if (trace) {
    static int n = 0;
    if (n++ < 40) NJ_LOG << "[VI] retour de trame " << n << "\n";
  }
  disp_int[0] |= kIrqFlag;
  disp_int[1] |= kIrqFlag;
  raise_irq(kVIIrq);
}

// Acces 16 bits : les adresses des trames sont faites de deux mots (haut a +0, bas a +2), ecrits
// separement par le SDK ; le reste passe par les fonctions 32 bits (DI : deja par moities).
namespace {
uint32_t *frame_reg(uint32_t off, bool &high) {
  high = (off & 2) == 0;
  if (off == 0x1C || off == 0x1E) return &g_vi_top_base;
  if (off == 0x24 || off == 0x26) return &g_vi_btm_field_base;
  return nullptr;
}
uint16_t vi_read16(uint32_t addr) {
  bool high;
  if (uint32_t *r = frame_reg(addr - kBase, high)) return (uint16_t)(high ? *r >> 16 : *r);
  return (uint16_t)vi_read(addr);
}
void vi_write16(uint32_t addr, uint16_t v) {
  bool high;
  if (uint32_t *r = frame_reg(addr - kBase, high)) {
    *r = high ? (*r & 0x0000FFFFu) | ((uint32_t)v << 16) : (*r & 0xFFFF0000u) | v;
    return;
  }
  vi_write(addr, v);
}
}  // namespace

void attach_vi(IoBus &bus) { bus.map(kBase, kBase + 0xFF, vi_read, vi_write, vi_read16, vi_write16); }

}  // namespace nj::runtime::hw
