// PE (moteur de pixels, 0xCC001000) : les deux signaux que le GPU renvoie au jeu, le jeton
// (GXSetDrawSync) et la fin de dessin (GXSetDrawDone), avec leurs interruptions (YAGCD § PE).
// Registre de controle 0x0A : bit 0 / bit 1 autorisent les interruptions jeton / fin ; ecrire
// bit 2 / bit 3 les acquitte. Jeton courant : 0x0E.
#include <atomic>
#include "runtime/log.h"
#include <chrono>
#include <cstdlib>
#include <iostream>

#include "runtime/cpu_context.h"
#include "runtime/env.h"
#include "runtime/hw/hw.h"

namespace nj::runtime {
extern GuestCpu *g_guest;
}

namespace nj::runtime::hw {

// Pose par main.cpp ; nul dans les outils qui lient le runtime sans lui.
void (*dump_threads_hook)(nj::runtime::MMU &) = nullptr;
std::atomic<uint32_t> pe_status{0};
std::atomic<uint32_t> g_pe_token{0};

namespace {
constexpr uint32_t kBase = 0xCC001000u, kTokenIrq = 0x200, kFinishIrq = 0x400;
std::atomic<uint32_t> enables{0};
std::atomic<bool> token_up{false}, finish_up{false};

uint64_t wall_us() {
  return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch()).count() % 100000000ull;
}

// Les lignes du PI suivent « leve ET autorise ».
void sync_lines() {
  pe_status = (token_up ? 0x04u : 0u) | (finish_up ? 0x08u : 0u);
  if (token_up && (enables & 1)) raise_irq(kTokenIrq);
  else lower_irq(kTokenIrq);
  if (finish_up && (enables & 2)) raise_irq(kFinishIrq);
  else lower_irq(kFinishIrq);
}

uint32_t pe_read(uint32_t addr) {
  switch (addr - kBase) {
    case 0x0A: return enables.load();
    case 0x0E: return g_pe_token.load() & 0xFFFF;
  }
  return 0;
}

void pe_write(uint32_t addr, uint32_t v) {
  if (addr - kBase != 0x0A) return;
  if (pe_tracing()) {
    static int n = 0;
    // acquittements 20 a 22 : qui dort sur GXDrawDone (piles des fils)
    if (n >= 20 && n <= 22 && g_guest && dump_threads_hook) dump_threads_hook(g_guest->mmu);
    if (n++ < 200)
      NJ_LOG << "[PE] " << wall_us() << " us controle 0x" << std::hex << v << std::dec
                << (v & 0x08 ? " acquitte-fin" : "") << (v & 0x04 ? " acquitte-jeton" : "") << "\n";
  }
  if (v & 0x04) token_up = false;
  if (v & 0x08) finish_up = false;
  enables = v & 0x03;
  sync_lines();
}
}  // namespace

// NJ_PETRACE=<s> : horodatage des signaux du PE et de leurs acquittements, a partir de s
// secondes (latence de GXDrawDone).
bool pe_tracing() {
  static const char *env = NJ_ENV("NJ_PETRACE");
  if (!env) return false;
  static const auto t0 = std::chrono::steady_clock::now();
  static const double from = std::atof(env);
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() >= from;
}

void pe_token(uint32_t token, bool irq) {
  g_pe_token.store(token);
  if (irq) {
    token_up = true;
    sync_lines();
  }
}

void pe_finish() {
  if (pe_tracing()) {
    static int n = 0;
    if (n++ < 200)
      NJ_LOG << "[PE] " << wall_us() << " us fin de dessin (controle 0x" << std::hex << enables.load()
                << std::dec << ")\n";
  }
  finish_up = true;
  sync_lines();
}

void attach_pe(IoBus &bus) { bus.map(kBase, kBase + 0xFF, pe_read, pe_write); }

}  // namespace nj::runtime::hw
