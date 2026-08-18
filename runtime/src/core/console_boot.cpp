// Demarrage de la console GameCube (console_boot.h).
#include "runtime/console_boot.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "runtime/config.h"
#include "runtime/cpu_context.h"
#include "runtime/game_version.h"
#include "runtime/log.h"
#include "runtime/virtual_disc.h"

namespace nj::runtime {
namespace {

constexpr uint32_t kMem1Size = 24u << 20;      // 24 Mo
constexpr uint32_t kMem1End = 0x81800000u;
constexpr uint32_t kArenaHiDefault = 0x81700000u;
constexpr uint32_t kBi2Addr = 0x81200000u;
constexpr uint32_t align_up32(uint32_t v) { return (v + 31) & ~31u; }

// Mots de la memoire basse (YAGCD « Low Memory »).
enum LowMem : uint32_t {
  kMagic = 0x80000020, kPhysMem = 0x80000024, kConsoleMem = 0x80000028, kConsoleType = 0x8000002C,
  kArenaLo = 0x80000030, kArenaHi = 0x80000034, kFstAddr = 0x80000038, kFstMax = 0x8000003C,
  kVideoPal = 0x800000CC, kSimMem = 0x800000F0, kBi2Ptr = 0x800000F4, kBusClock = 0x800000F8,
  kCpuClock = 0x800000FC,
};

// Premiere adresse libre apres le programme, relevee au-dessus de la pile de demarrage si le
// code d'entree la place plus haut (lis r1,hi puis ori/addi r1,r1,lo dans ses 128 premieres
// instructions).
uint32_t arena_start(MMU &m, const boot::Program &exe) {
  uint32_t lo = 0x80000000u;
  for (const auto &s : exe.segments)
    lo = std::max(lo, s.addr + s.size);
  lo = align_up32(lo);
  uint32_t hi16 = 0, stack = 0;
  bool seen_lis = false;
  for (uint32_t i = 0; i < 0x200 && stack == 0; i += 4) {
    const uint32_t op = m.read32(exe.entry + i);
    switch (op >> 16) {
    case 0x3C20:   // lis r1, hi
      hi16 = (op & 0xFFFF) << 16;
      seen_lis = true;
      break;
    case 0x6021:   // ori r1, r1, lo
      if (seen_lis) stack = hi16 | (op & 0xFFFF);
      break;
    case 0x3821:   // addi r1, r1, lo
      if (seen_lis) stack = hi16 + (int16_t)(op & 0xFFFF);
      break;
    }
  }
  if (stack >= lo && stack < kMem1End) {
    NJ_LOG << "[Loader] pile de demarrage 0x" << std::hex << stack << " au-dessus du programme 0x" << lo
             << std::dec << " : arene relevee" << std::endl;
    lo = align_up32(stack + 0x100);
  }
  return lo;
}

// Region video : PAL pour la version europeenne (reconnue par l'empreinte du DOL), ou d'apres la
// quatrieme lettre de l'identifiant du jeu.
bool video_pal() {
  if (game_version() == GameVersion::Europe)
    return true;
  const std::string &id = Config::get().game_id;
  const char r = id.size() >= 4 ? id[3] : 'E';
  return std::string("PDFISHUXYZ").find(r) != std::string::npos;
}

}  // namespace

void boot_console(GuestCpu &ctx, const boot::Program &exe, const std::string &game_dir) {
  MMU &m = ctx.mmu;

  // Programme : zones bss remises a zero, puis segments copies ; son code est recompile, tout
  // comme la fenetre des modules REL traduits (rel_aot.h).
  for (const auto &s : exe.segments)
    if (s.zero)
      for (uint32_t i = 0; i < s.size; ++i) m.write8(s.addr + i, 0);
  for (const auto &s : exe.segments) {
    if (s.zero)
      continue;
    for (uint32_t i = 0; i < s.size; ++i) m.write8(s.addr + i, s.bytes[i]);
    if (s.code && s.size)
      add_recompiled_range(s.addr, s.addr + s.size);
  }
  add_recompiled_range(0xB0000000u, 0xC0000000u);

  const uint32_t arena_lo = arena_start(m, exe);
  uint32_t arena_hi = kArenaHiDefault;

  // Disque : en-tete (identifiant...) en tete de memoire, FST en haut de la MEM1, BI2.
  auto &disc = DiscView::get();
  if (disc.init(game_dir)) {
    const auto &boot = disc.boot_data();
    for (uint32_t i = 0; i < 0x20 && i < boot.size(); ++i) m.write8(0x80000000u + i, boot[i]);
    const auto &fst = disc.fst_data();
    const uint32_t fst_size = (uint32_t)fst.size();
    const uint32_t fst_addr = (kMem1End - fst_size) & ~63u;
    for (uint32_t i = 0; i < fst_size; ++i) m.write8(fst_addr + i, fst[i]);
    m.write32(kFstAddr, fst_addr);
    m.write32(kFstMax, fst_size);
    arena_hi = fst_addr;
    std::vector<uint8_t> bi2(0x2000);
    if (disc.read(0x440, (uint32_t)bi2.size(), bi2.data()) == bi2.size()) {
      for (uint32_t i = 0; i < bi2.size(); ++i) m.write8(kBi2Addr + i, bi2[i]);
      m.write32(kBi2Ptr, kBi2Addr);
    }
  } else {
    m.write32(kFstAddr, arena_hi);
  }

  m.write32(kMagic, 0x0D15EA5Eu);
  m.write32(kPhysMem, kMem1Size);
  m.write32(kConsoleMem, kMem1Size);
  m.write32(kConsoleType, 3);
  m.write32(kArenaLo, arena_lo);
  m.write32(kArenaHi, arena_hi);
  m.write32(kSimMem, kMem1Size);
  NJ_LOG << "[Loader] arene 0x" << std::hex << arena_lo << " - 0x" << arena_hi << std::dec << " ("
           << (arena_hi - arena_lo) / 1024 << " Kio)" << std::endl;

  // Region video, sauf si le programme lui-meme couvre cette adresse.
  bool covered = false;
  for (const auto &s : exe.segments)
    covered |= !s.zero && kVideoPal >= s.addr && kVideoPal < s.addr + s.size;
  if (!covered)
    m.write32(kVideoPal, video_pal() ? 1u : 0u);

  // Horloges de la GameCube (bus 162 MHz, processeur 486 MHz, base de temps bus / 4) et pile.
  m.write32(kBusClock, 162000000u);
  m.write32(kCpuClock, 486000000u);
  ctx.timebase_hz = 40500000;
  ctx.gpr[1] = 0x816FFFF0u;
  ctx.tb_start = std::chrono::steady_clock::now();
}

}  // namespace nj::runtime
