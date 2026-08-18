// Services du moteur rendus au jeu ou au debogage : heure, aiguillage des registres materiels,
// OSReport (journal du jeu), fin d'attente de trame, et les traces de developpement (adresse
// espionnee, ecritures non mappees, appels traces).
#include <cstdio>
#include "runtime/log.h"
#include "runtime/obf.h"
using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire
#include "runtime/game_version.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>
#include <string>

#include "runtime/config.h"
#include "runtime/cpu_context.h"
#include "runtime/env.h"
#include "runtime/host_clock.h"
#include "runtime/hw/hw.h"
#include "runtime/hw/io_bus.h"

namespace nj::runtime {

extern GuestCpu *g_guest;

// Microsecondes, origine de steady_clock : position du balayage VI, compteur d'echantillons AI.
uint64_t guest_time() { return (uint64_t)nj::host_now_us(); }

// --- registres materiels --------------------------------------------------------------------
// Le jeu adresse les registres par 0xCC00xxxx (ou leur vue physique 0x0C00xxxx) ; le bus les
// connait sous 0xCC00xxxx. 0xCD00xxxx (miroir Wii) reste tel quel.
namespace {
uint32_t io_address(uint32_t a) {
  const uint32_t top = a >> 24;
  return (top == 0xCD || top == 0x0D) ? a : 0xCC000000u | (a & 0x00FFFFFFu);
}
}  // namespace
uint32_t hwreg_read32(uint32_t addr) { return IoBus::get().read32(io_address(addr)); }
uint16_t hwreg_read16(uint32_t addr) { return IoBus::get().read16(io_address(addr)); }
void hwreg_write32(uint32_t addr, uint32_t val) { IoBus::get().write32(io_address(addr), val); }
void hwreg_write16(uint32_t addr, uint16_t val) { IoBus::get().write16(io_address(addr), val); }

// --- traces de developpement ------------------------------------------------------------------
bool g_trace_calls = [] {
  const char *e = NJ_ENV("NJ_TRACE_CALLS");
  return e && e[0] == '1';
}();
uint32_t g_watch_addr = [] {
  const char *e = NJ_ENV("NJ_WATCH");
  return e ? (uint32_t)std::strtoul(e, nullptr, 16) : 0u;
}();

void on_watch(uint32_t addr, uint32_t value, int width) {
  NJ_LOG << "[Watch] " << width << " bits 0x" << std::hex << addr << " <- 0x" << value;
  if (GuestCpu *c = g_guest) {
    NJ_LOG << " pc 0x" << c->pc << " lr 0x" << c->lr;
    static const bool regs = NJ_ENV("NJ_WATCH_REGS") != nullptr;
    if (regs)
      for (int r : {3, 4, 24, 25, 26, 27, 28, 29, 30, 31})
        NJ_LOG << " r" << std::dec << r << " 0x" << std::hex << c->gpr[r];
  }
  NJ_LOG << std::dec << '\n';
}

// Sans BAT, une ecriture sous 0x80000000 est une DSI sur la console ; ici l'adresse est masquee
// et l'ecriture tomberait en silence dans les variables de l'OS. Les 32 premieres sont nommees
// (pc, lr) pour trouver le coupable en une partie.
void unmapped_write_hit(uint32_t addr, uint32_t value, int width) {
  static uint32_t seen = 0;
  if (seen >= 32) return;
  ++seen;
  NJ_LOG << "[MMU] ecriture " << std::dec << width << " bits hors memoire 0x" << std::hex << addr << " <- 0x"
            << value;
  if (GuestCpu *c = g_guest) NJ_LOG << " pc 0x" << c->pc << " lr 0x" << c->lr;
  if (seen == 32) NJ_LOG << " (suivantes non signalees)";
  NJ_LOG << std::dec << std::endl;   // vidage : precede souvent un plantage
}

void log_null_call(uint32_t site, uint32_t lr) {
  static int seen = 0;
  if (seen++ < 20)
    NJ_LOG << "[Appel] pointeur nul ignore a 0x" << std::hex << site << " (lr 0x" << lr << ")" << std::dec << '\n';
}

}  // namespace nj::runtime

using nj::runtime::GuestCpu;

namespace {
std::string guest_cstr(GuestCpu &ctx, uint32_t a) {
  std::string s;
  for (uint8_t ch; (ch = ctx.mmu.read8(a)) != 0; ++a) s.push_back((char)ch);
  return s;
}

// printf du jeu : arguments entiers dans r4..r10, flottants dans f1..f8 (convention du SDK).
std::string format_report(GuestCpu &ctx) {
  const std::string fmt = guest_cstr(ctx, ctx.gpr[3]);
  std::string out;
  int next_gpr = 4, next_fpr = 1;
  for (size_t i = 0; i < fmt.size(); ++i) {
    if (fmt[i] != '%') {
      out.push_back(fmt[i]);
      continue;
    }
    if (i + 1 < fmt.size() && fmt[i + 1] == '%') {
      out.push_back('%');
      ++i;
      continue;
    }
    size_t j = i + 1;
    while (j < fmt.size() && std::strchr("0123456789.-+ #hlz", fmt[j])) ++j;
    if (j >= fmt.size()) {
      out += fmt.substr(i);
      break;
    }
    const std::string spec = fmt.substr(i, j - i + 1);
    const char conv = fmt[j];
    char buf[96];
    if (std::strchr("dic", conv)) {
      std::snprintf(buf, sizeof buf, spec.c_str(), (int)(int32_t)ctx.gpr[next_gpr++]);
      out += buf;
    } else if (std::strchr("uxXp", conv)) {
      std::snprintf(buf, sizeof buf, spec.c_str(), (unsigned)ctx.gpr[next_gpr++]);
      out += buf;
    } else if (conv == 's') {
      const uint32_t p = ctx.gpr[next_gpr++];
      out += p ? guest_cstr(ctx, p) : std::string("(null)");
    } else if (conv == 'f') {
      if (next_fpr <= 8) std::snprintf(buf, sizeof buf, spec.c_str(), ctx.fpr[next_fpr++]);
      else std::snprintf(buf, sizeof buf, "%s", "[flottant sur la pile]"_g);
      out += buf;
    } else {
      out += spec;
    }
    i = j;
  }
  return out;
}

// NJ_BOOTDIRECT=1 : sauter les logos, par le chemin que le jeu prend lui-meme apres un reset a
// chaud (titleWait : pRK->x17 != 0 -> mode 5). Rien d'autre : le bit de flags_54 qui saute aussi
// le menu de debug garde dans titleExit le choix de la salle et le placement de Leon (sans lui,
// pas de salle du tout). L'instant : apres l'effacement de la zone de reset (message
// « RESET_KEEP_WORK memory clear »), avant que titleWait ne lise x17.
void boot_direct(GuestCpu &ctx, const std::string &line) {
  static const bool on = NJ_ENV("NJ_BOOTDIRECT") != nullptr;
  static bool done;
  if (!on || done || line.find("RESET_KEEP_WORK") == std::string::npos) return;
  const uint32_t prk = ctx.mmu.read32(nj::runtime::ga(0x80315AD4));   // pRK (symboles du decompile)
  if (prk < 0x80000000u) {
    NJ_LOG << "[BootDirect] pRK pas encore pose\n";
    return;
  }
  ctx.mmu.write8(prk + 0x17, 1);
  done = true;
  NJ_LOG << "[BootDirect] logos sautes\n";
}
}  // namespace

extern "C" void OSReport(GuestCpu &ctx) {
  const std::string line = format_report(ctx);
  NJ_LOG << "[OSReport] [LR:0x" << std::hex << ctx.lr << std::dec << "] " << line;
  boot_direct(ctx, line);
}

// VIWaitForRetrace servi en natif : la trame suivante est donnee tout de suite (le moteur cadence
// lui-meme l'affichage).
extern "C" void VIWaitForRetrace(GuestCpu &) { nj::runtime::hw::vi_raise(); }
