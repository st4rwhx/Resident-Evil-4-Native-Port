// CP (processeur de commandes, 0xCC000000) : la FIFO GX en memoire que le GPU consomme (YAGCD
// § CP). Le jeu la remplit puis fait avancer le pointeur d'ecriture ; nous la vidons vers le
// tuyau GX (gx.cpp) a ce moment-la, et aussi quand il lit la distance lecture/ecriture : sinon il
// croirait le GPU en retard et ecraserait des sommets pas encore lus (meme lecon que la PR 7214 de
// Dolphin, « synchroniser le GPU sur les acces aux registres du CP »).
// Registres de FIFO : 32 bits, en deux moities de 16 bits (basse a +0, haute a +2).
#include <iostream>
#include "runtime/log.h"

#include "runtime/cpu_context.h"
#include "runtime/env.h"
#include "runtime/hw/hw.h"

namespace nj::runtime {
extern GuestCpu *g_guest;
void GX_WGPIPE_Write8(uint8_t val);
}  // namespace nj::runtime

namespace nj::runtime::hw {

extern uint32_t g_fifo_cfg_gen;   // hw_pi.cpp

namespace {
constexpr uint32_t kBase = 0xCC000000u;
constexpr uint32_t kPhys = 0x03FFFFFFu;
uint16_t status = 0, control = 0, clear_reg = 0;
// FIFO : debut, fin, seuils haut / bas, distance, ecriture, lecture
enum { kStart, kEnd, kHigh, kLow, kDist, kPut, kGet, kFifoRegs };
uint32_t fifo[kFifoRegs] = {};

// Moitie (basse : +0, haute : +2) d'un registre de FIFO. Un acces 32 bits arrive sur la moitie
// basse avec la valeur entiere.
void store_half(uint32_t &reg, uint32_t off, uint32_t v) {
  if (off & 2) reg = (reg & 0x0000FFFFu) | ((v & 0xFFFF) << 16);
  else if (v > 0xFFFF) reg = v;
  else reg = (reg & 0xFFFF0000u) | v;
}

// Le GPU lit de `get` jusqu'a `put` (avec retour au debut), si la lecture est autorisee.
void drain() {
  if (!g_guest || !(control & 1)) return;
  const uint32_t lo = fifo[kStart] & kPhys, hi = fifo[kEnd] & kPhys, put = fifo[kPut] & kPhys;
  if (!lo || !hi) return;
  uint32_t get = fifo[kGet] & kPhys;
  const uint32_t from = get;
  int n = 0;
  for (; get != put && n < 100000; ++n) {
    GX_WGPIPE_Write8(g_guest->mmu.read8(get));
    if (++get >= hi) get = lo;
  }
  fifo[kGet] = get;
  static const bool trace = NJ_ENV("NJ_CPTRACE") != nullptr;
  if (n && trace)
    NJ_LOG << "[CP] " << n << " octets, lecture 0x" << std::hex << from << " -> 0x" << get << std::dec << "\n";
}

uint32_t cp_read(uint32_t addr) {
  const uint32_t off = addr - kBase;
  static const bool sync = NJ_ENV("NJ_NOCPSYNC") == nullptr;
  if (sync && off >= 0x30 && off <= 0x3B) {
    drain();
    const uint32_t lo = fifo[kStart] & kPhys, hi = fifo[kEnd] & kPhys;
    const uint32_t put = fifo[kPut] & kPhys, get = fifo[kGet] & kPhys;
    if (hi > lo) fifo[kDist] = put >= get ? put - get : (hi - lo) - (get - put);
  }
  if (off == 0x00) return status;
  if (off == 0x02) return control;
  if (off >= 0x20 && off <= 0x3A) {
    const uint32_t reg = fifo[(off - 0x20) >> 2];
    return (off & 2) ? reg >> 16 : reg & 0xFFFF;
  }
  return 0;
}

void cp_write(uint32_t addr, uint32_t v) {
  const uint32_t off = addr - kBase;
  switch (off) {
    case 0x00: status = (uint16_t)v; return;
    case 0x02: control = (uint16_t)v; drain(); return;
    case 0x04: clear_reg = (uint16_t)v; return;
  }
  if (off < 0x20 || off > 0x3A || (off & 1)) return;
  const int r = (int)((off - 0x20) >> 2);
  store_half(fifo[r], off, v);
  if (r == kStart) {
    ++g_fifo_cfg_gen;
    GX_FifoCfgChanged();
  } else if (r == kPut && ((off & 2) || v > 0xFFFF)) {
    drain();   // pointeur d'ecriture complet : le GPU peut lire
  }
}
}  // namespace

uint32_t cp_fifo_base() { return fifo[kStart]; }

void attach_cp(IoBus &bus) { bus.map(kBase, kBase + 0xFF, cp_read, cp_write); }

}  // namespace nj::runtime::hw
