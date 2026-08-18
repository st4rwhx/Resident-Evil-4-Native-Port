// PI (interface processeur, 0xCC003000) : causes et masque des interruptions, FIFO du CPU vers
// le GPU, et le registre de revision de la console (YAGCD § PI).
#include <iostream>
#include "runtime/log.h"

#include "runtime/config.h"
#include "runtime/cpu_context.h"
#include "runtime/hw/hw.h"

namespace nj::runtime::hw {

std::atomic<uint32_t> pi_cause{0};
std::atomic<uint32_t> pi_enable{0};
std::atomic<bool> poll_now{false};

// Change a chaque ecriture d'une borne de FIFO (ici ou dans hw_cp.cpp) : le tuyau GX (gx.cpp)
// ne recalcule son aiguillage qu'a ce moment-la.
uint32_t g_fifo_cfg_gen = 1;

namespace {
constexpr uint32_t kBase = 0xCC003000u;
uint32_t fifo_lo = 0, fifo_hi = 0, fifo_put = 0;

uint32_t pi_read(uint32_t addr) {
  switch (addr - kBase) {
    case 0x00: return pi_cause.load();
    case 0x04: return pi_enable.load();
    case 0x0C: return fifo_lo;
    case 0x10: return fifo_hi;
    case 0x14: return fifo_put;
    case 0x2C:   // revision : 1 sur GameCube
      return Config::get().platform == Platform::GameCube ? 0x01u : 0x23u;
  }
  return 0;
}

void pi_write(uint32_t addr, uint32_t v) {
  switch (addr - kBase) {
    case 0x00: lower_irq(v); break;   // ecrire une cause l'acquitte
    case 0x04:
      if (pi_enable.load() != v)
        NJ_LOG << "[PI] masque 0x" << std::hex << pi_enable.load() << " -> 0x" << v << std::dec << std::endl;
      pi_enable = v;
      break;
    case 0x0C: fifo_lo = v; ++g_fifo_cfg_gen; GX_FifoCfgChanged(); break;
    case 0x10: fifo_hi = v; ++g_fifo_cfg_gen; GX_FifoCfgChanged(); break;
    case 0x14: fifo_put = v; break;
  }
}
}  // namespace

void cpu_fifo_get(uint32_t &base, uint32_t &end, uint32_t &wptr) {
  base = fifo_lo;
  end = fifo_hi;
  wptr = fifo_put;
}
void cpu_fifo_set_wptr(uint32_t wptr) { fifo_put = wptr; }

void attach_pi(IoBus &bus) { bus.map(kBase, kBase + 0xFF, pi_read, pi_write); }

}  // namespace nj::runtime::hw
