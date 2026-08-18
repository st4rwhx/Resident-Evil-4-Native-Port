// AI (interface audio, 0xCC006C00) : registre de controle, volume du flux du disque, compteur
// d'echantillons et seuil d'interruption (YAGCD § AI). Le son lui-meme sort par le DSP
// (hw_dsp.cpp, ax.cpp) ; RE4 ne se sert de l'AI que pour son compteur.
#include "runtime/hw/hw.h"

namespace nj::runtime::hw {

namespace {
constexpr uint32_t kBase = 0xCC006C00u, kAIIrq = 0x20;     // cause PI de l'AI
constexpr uint32_t kRun = 0x20, kIrqOn = 0x02, kIrqFlag = 0x04, kResetCount = 0x08;
uint32_t control = 0, volume = 0, count_base = 0, irq_at = 0;
uint64_t count_since = 0;   // instant ou le compteur valait count_base

// Echantillons ecoules depuis count_since (meme cadence que les mesures d'origine du port).
uint32_t live_count() {
  if (!(control & kRun)) return count_base;
  return count_base + (uint32_t)((guest_time() - count_since) * 48 / 1000);
}

// Seuil franchi : drapeau d'interruption, et la cause PI si le jeu l'a autorisee.
void check_threshold(uint32_t count) {
  if (!(control & kRun) || irq_at == 0 || count < irq_at) return;
  control |= kIrqFlag;
  if (control & kIrqOn) raise_irq(kAIIrq);
}

uint32_t ai_read(uint32_t addr) {
  const uint32_t count = live_count();
  check_threshold(count);
  switch (addr - kBase) {
    case 0x0: return control;
    case 0x4: return volume;
    case 0x8: return count;
    case 0xC: return irq_at;
  }
  return 0;
}

void ai_write(uint32_t addr, uint32_t v) {
  switch (addr - kBase) {
    case 0x0:
      if (v & kIrqFlag) {   // acquittement
        control &= ~kIrqFlag;
        lower_irq(kAIIrq);
      }
      if (v & kResetCount) {
        count_base = 0;
        count_since = guest_time();
      }
      if ((v & kRun) && !(control & kRun)) count_since = guest_time();
      control = (control & kIrqFlag) | (v & ~kIrqFlag);
      break;
    case 0x4: volume = v; break;
    case 0x8:
      count_base = v;
      count_since = guest_time();
      break;
    case 0xC: irq_at = v; break;
  }
}
}  // namespace

void ai_step() { check_threshold(live_count()); }

void attach_ai(IoBus &bus) { bus.map(kBase, kBase + 0xFF, ai_read, ai_write); }

}  // namespace nj::runtime::hw
