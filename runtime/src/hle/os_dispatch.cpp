// Exceptions et interruptions du Gekko, servies au jeu entre deux blocs de code recompile.
//
// Le code genere rend la main ici (drain_callbacks) aux retours de boucle, quand son
// budget d'instructions est epuise ou quand un autre fil leve hw::poll_now. On y fait, dans cet
// ordre : le tuyau GX, la mesure des attentes et le sommeil des boucles d'attente, le retour d'un
// gestionnaire (lr = 0xFFFFFFFC : contexte de l'OS recharge), le battement du journal, le disque,
// les minuteries (VI comprise), le decrementeur, puis les interruptions externes du PI.
//
// Un gestionnaire est lance comme sur la console : le contexte courant de l'OS (OSContext, pointe
// par 0x800000D4) recoit les registres, SRR0/SRR1 gardent pc et msr, MSR[EE] tombe, r3 = numero
// d'exception ou d'interruption de l'OS, r4 = contexte ; lr = 0xFFFFFFFC marque la fin. Le saut
// passe par exc_jump (deroule la pile hote jusqu'au repartiteur).
#include <atomic>
#include "runtime/log.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include "runtime/config.h"
#include "runtime/cpu_context.h"
#include "runtime/perf_journal.h"
#include "runtime/env.h"
#include "runtime/game_version.h"
#include "runtime/host_clock.h"
#include "runtime/hw/hw.h"
#include "runtime/timers.h"

void GX_DeterministicParse();                                // gx.cpp, NJ_DETERMINISTIC
extern std::atomic<uint64_t> g_fl_idle_us, g_fl_vspin_us;   // gx.cpp, NJ_FRAMELOG
bool framelog_enabled();

uint64_t g_vi_fired = 0, g_vi_skip[4] = {0, 0, 0, 0};   // NJ_FRAMELOG (gx.cpp)
uint64_t g_vi_late = 0;                                // VI reportees puis servies
namespace nj::runtime {
uint64_t g_ppc_calls = 0;   // passages ici par fenetre [PERF] (main.cpp)
}

namespace nj::runtime {

namespace {

// --- globales de l'OS du SDK (adresses fixes de la GameCube) --------------------------------
constexpr uint32_t kCurrentContext = 0x800000D4;   // OSContext* courant
constexpr uint32_t kActiveThreads = 0x800000DC;    // file des fils actifs
constexpr uint32_t kCurrentThread = 0x800000E4;    // OSThread* courant
constexpr uint32_t kExceptionTable = 0x80003000;   // gestionnaires d'exception (OS)
constexpr uint32_t kInterruptTable = 0x80003040;   // gestionnaires d'interruption (OS)
constexpr uint32_t kReturnMark = 0xFFFFFFFCu;      // lr d'un gestionnaire lance ici
constexpr uint32_t kMsrEE = 0x8000;
constexpr int kExcDecrementer = 8;
constexpr int kExcExternal = 4;

// OSContext : disposition du SDK.
namespace ctxo {
constexpr uint32_t gpr = 0x000, cr = 0x080, lr = 0x084, ctr = 0x088, xer = 0x08C, fpr = 0x090;
constexpr uint32_t fpscr = 0x194, srr0 = 0x198, srr1 = 0x19C, state = 0x1A2, gqr = 0x1A4, ps1 = 0x1C8;
constexpr uint16_t kFpSaved = 0x0001, kExc = 0x0002;
}  // namespace ctxo
// OSThread : etat, priorite, file d'attente, maillon des actifs.
namespace tho {
constexpr uint32_t state = 0x2C8, prio = 0x2D0, queue = 0x2DC, next_active = 0x2FC;
}

// Interruptions du PI (bit de cause) -> numero d'interruption de l'OS, par priorite. 0 : l'EXI et
// le DSP disent eux-memes laquelle de leurs sources a sonne.
struct PiSource {
  uint32_t bit;
  int os;   // -1 : demander au bloc (exi_os_irq / dsp_os_irq)
  int (*ask)();
};
const PiSource kPiSources[] = {
    {0x4000, 27, nullptr},   // PI : IPC (Wii, garde pour l'ordre)
    {0x0100, 24, nullptr},   // VI
    {0x0004, 21, nullptr},   // DI
    {0x0008, 20, nullptr},   // SI
    {0x0010, -1, hw::exi_os_irq},
    {0x0040, -1, hw::dsp_os_irq},
    {0x0020, 8, nullptr},    // AI (numero de l'OS : AI)
    {0x0400, 19, nullptr},   // PE fin
    {0x0200, 18, nullptr},   // PE jeton
    {0x0800, 17, nullptr},   // CP
};

uint32_t pack_cr(const GuestCpu &cpu) {
  uint32_t v = 0;
  for (int i = 0; i < 8; ++i) {
    const uint32_t nib = (cpu.cr[i].lt ? 8u : 0) | (cpu.cr[i].gt ? 4u : 0) | (cpu.cr[i].eq ? 2u : 0) |
                         (cpu.cr[i].so ? 1u : 0);
    v |= nib << (28 - 4 * i);
  }
  return v;
}

void unpack_cr(GuestCpu &cpu, uint32_t v) {
  for (int i = 0; i < 8; ++i) {
    const uint32_t nib = (v >> (28 - 4 * i)) & 0xF;
    cpu.cr[i].lt = nib & 8;
    cpu.cr[i].gt = nib & 4;
    cpu.cr[i].eq = nib & 2;
    cpu.cr[i].so = nib & 1;
  }
}

void store_fpu(GuestCpu &cpu, uint32_t c) {
  for (int i = 0; i < 32; ++i) {
    uint64_t d;
    std::memcpy(&d, &cpu.fpr[i], 8);
    cpu.mmu.write64(c + ctxo::fpr + 8 * i, d);
    std::memcpy(&d, &cpu.ps1[i], 8);
    cpu.mmu.write64(c + ctxo::ps1 + 8 * i, d);
  }
  cpu.mmu.write32(c + ctxo::fpscr, cpu.fpscr);
}

void load_fpu(GuestCpu &cpu, uint32_t c) {
  for (int i = 0; i < 32; ++i) {
    uint64_t d = cpu.mmu.read64(c + ctxo::fpr + 8 * i);
    std::memcpy(&cpu.fpr[i], &d, 8);
    d = cpu.mmu.read64(c + ctxo::ps1 + 8 * i);
    std::memcpy(&cpu.ps1[i], &d, 8);
  }
  cpu.fpscr = cpu.mmu.read32(c + ctxo::fpscr);
}

// --- flottants : l'OS ne les change de fil que sur l'exception « FPU indisponible » (MSR[FP]),
// jamais levee ici. Un changement de fil fait par l'ordonnanceur du jeu gardait donc les flottants
// du fil precedent (le decompresseur Yz2 tient son etat dans f3-f5 : chargement de r100 sans fin).
// On les echange a chaque rfi. fpu_owner : le contexte dont les valeurs sont dans les registres.
uint32_t fpu_owner = 0;

bool is_active_thread(GuestCpu &cpu, uint32_t c) {
  uint32_t th = cpu.mmu.read32(kActiveThreads);
  for (int guard = 0; th && guard < 64; ++guard) {
    if (th == c) return true;   // un OSThread commence par son OSContext
    th = cpu.mmu.read32(th + tho::next_active);
  }
  return false;
}

void switch_fpu(GuestCpu &cpu) {
  const uint32_t cur = cpu.mmu.read32(kCurrentContext);
  if (!cur || cur == fpu_owner) return;
  if (fpu_owner && is_active_thread(cpu, fpu_owner)) {
    store_fpu(cpu, fpu_owner);
    cpu.mmu.write16(fpu_owner + ctxo::state, cpu.mmu.read16(fpu_owner + ctxo::state) | ctxo::kFpSaved);
  }
  if (cpu.mmu.read16(cur + ctxo::state) & ctxo::kFpSaved) load_fpu(cpu, cur);
  fpu_owner = cur;
}

void save_context(GuestCpu &cpu, uint32_t c) {
  for (int i = 0; i < 32; ++i) cpu.mmu.write32(c + ctxo::gpr + 4 * i, cpu.gpr[i]);
  // exception en cours, flottants sauves (l'OS et switch_fpu peuvent les reprendre d'ici)
  cpu.mmu.write16(c + ctxo::state, cpu.mmu.read16(c + ctxo::state) | ctxo::kFpSaved | ctxo::kExc);
  cpu.mmu.write32(c + ctxo::cr, pack_cr(cpu));
  cpu.mmu.write32(c + ctxo::lr, cpu.lr);
  cpu.mmu.write32(c + ctxo::ctr, cpu.ctr);
  cpu.mmu.write32(c + ctxo::xer, cpu.xer);
  store_fpu(cpu, c);
  cpu.mmu.write32(c + ctxo::srr0, cpu.pc);
  cpu.mmu.write32(c + ctxo::srr1, cpu.msr);
  for (int i = 0; i < 8; ++i) cpu.mmu.write32(c + ctxo::gqr + 4 * i, cpu.gqr[i]);
  fpu_owner = 0;   // le gestionnaire qui va tourner ne possede pas cet etat
}

bool load_context(GuestCpu &cpu) {
  const uint32_t c = cpu.mmu.read32(kCurrentContext);
  if (!c) return false;
  fpu_owner = c;
  for (int i = 0; i < 32; ++i) cpu.gpr[i] = cpu.mmu.read32(c + ctxo::gpr + 4 * i);
  unpack_cr(cpu, cpu.mmu.read32(c + ctxo::cr));
  cpu.lr = cpu.mmu.read32(c + ctxo::lr);
  cpu.ctr = cpu.mmu.read32(c + ctxo::ctr);
  cpu.xer = cpu.mmu.read32(c + ctxo::xer);
  load_fpu(cpu, c);
  for (int i = 0; i < 8; ++i) cpu.gqr[i] = cpu.mmu.read32(c + ctxo::gqr + 4 * i);
  cpu.pc = cpu.mmu.read32(c + ctxo::srr0);
  cpu.msr = cpu.mmu.read32(c + ctxo::srr1);
  cpu.mmu.write16(c + ctxo::state, cpu.mmu.read16(c + ctxo::state) & ~ctxo::kExc);
  return true;
}

// Gestionnaires imbriques : un gestionnaire peut rouvrir les interruptions (le rappel d'image de
// l'AX appelle OSEnableInterrupts dans __AIDHandler) et un second arrive avant la fin du premier.
// Chaque contexte sauve est garde sur une pile pour que chaque retour reprenne le sien.
std::vector<uint32_t> outer_saved;

void enter_handler(GuestCpu &cpu, uint32_t saved) {
  if (cpu.saved_dispatch) outer_saved.push_back(cpu.saved_dispatch);
  cpu.saved_dispatch = saved;
}

// Saut dans un gestionnaire de l'OS, comme le processeur a une exception : retour note dans
// SRR0 / SRR1, interruptions coupees, r3 = numero, r4 = contexte, retour vers la marque du runtime.
// Ne revient pas (vers la boucle de repartition).
[[noreturn]] void jump_to_handler(GuestCpu &cpu, uint32_t handler, int number, uint32_t context, uint32_t srr1) {
  cpu.srr0 = cpu.pc;
  cpu.srr1 = srr1;
  cpu.msr &= ~kMsrEE;
  cpu.gpr[3] = (uint32_t)number;
  cpu.gpr[4] = context;
  cpu.lr = kReturnMark;
  cpu.pc = handler;
  longjmp(NJ_JB(cpu.exc_jump), 1);
}

// Lance un gestionnaire de l'OS sur le contexte courant ; ne revient pas.
[[noreturn]] void raise_exception(GuestCpu &cpu, uint32_t handler, int number, uint32_t context) {
  save_context(cpu, context);
  enter_handler(cpu, context);
  jump_to_handler(cpu, handler, number, context, cpu.msr);
}

bool valid_handler(uint32_t h) { return h != 0 && h != 0xFFFFFFFFu; }

// --- boucles d'attente ----------------------------------------------------------------------
// Le jeu attend en tournant : re4_main sur le compteur de VBlank entre deux images, SelectThread
// quand aucun fil n'est pret. Sur la console le processeur n'a rien d'autre a faire ; ici ce tour
// prenait ~0,5 coeur aux fils de lecture et de rendu. Dans ces boucles le fil dort jusqu'a
// l'echeance la plus proche (minuteries, decrementeur), par tranches de 0,5 ms pour voir ce que
// les autres fils levent (poll_now). NJ_NOIDLESLEEP=1 : coupe.
struct WaitLoops {
  uint32_t sel0, sel1, vs10, vs11, vs20, vs21, vc0, vc1;
  int kind(uint32_t pc) const {   // 1 : SelectThread inactif ; 2 : attente de VBlank
    if (pc >= sel0 && pc <= sel1) return 1;
    if ((pc >= vs10 && pc <= vs11) || (pc >= vs20 && pc <= vs21) || (pc >= vc0 && pc < vc1)) return 2;
    return 0;
  }
};

const WaitLoops &wait_loops() {   // lues une fois : la version du jeu est fixee au demarrage
  static const WaitLoops w{ga(0x80078B38), ga(0x80078B58), ga(0x801532D8), ga(0x801532EC),
                           ga(0x801532F4), ga(0x80153304), ga(0x8015394C), ga(0x80153960)};
  return w;
}

void sleep_in_wait_loop(GuestCpu &cpu) {
#if defined(NJ_RELEASE) && !defined(NJ_DEVTOOLS)
  static const bool off = false;
#else
  static const bool off = NJ_ENV("NJ_NOIDLESLEEP") || NJ_ENV("NJ_INSTTB");
#endif
  if (off || cpu.inside_cb || !wait_loops().kind(cpu.pc)) return;
  if (hw::poll_now.load(std::memory_order_relaxed) || (hw::pi_cause.load() & hw::pi_enable.load())) return;
  uint64_t due = Timers::get().next_due();
  if (cpu.dec_fired) due = std::min(due, cpu.dec_base_tb + cpu.dec_value);
  const uint64_t margin = cpu.timebase_hz / 2000;   // l'heure publiee retarde, le reveil deborde
  for (int slice = 0; slice < 40; ++slice) {
    const uint64_t now = cpu.read_timebase_coarse();
    if (due != ~0ull && now + margin >= due) break;
    uint64_t left_us = 500;
    if (due != ~0ull && due > now + margin) {
      const uint64_t rem = (due - margin - now) * 1000000 / cpu.timebase_hz;
      if (rem < left_us) left_us = rem;
    }
    precise_sleep_us((int)left_us);
    if (hw::poll_now.load(std::memory_order_relaxed) || (hw::pi_cause.load() & hw::pi_enable.load())) break;
  }
}

void account_wait(GuestCpu &cpu) {   // NJ_FRAMELOG : temps en attente, par sorte de boucle
  static int last_kind = 0;
  static int64_t last_t = host_coarse_us();
  const int kind = wait_loops().kind(cpu.pc);
  const int64_t now = host_coarse_us();
  if (kind && kind == last_kind)
    (kind == 1 ? g_fl_idle_us : g_fl_vspin_us).fetch_add((uint64_t)(now - last_t), std::memory_order_relaxed);
  last_kind = kind;
  last_t = now;
}

// Decrementeur expire, vu a l'heure publiee : arme a l'heure exacte (set_decrementer), que l'heure
// publiee suit avec ~200 us de retard. Sans la garde la soustraction debordait et le decrementeur
// « expirait » a chaque passage (jeu noye sous les interruptions, 29/09).
bool dec_expired(GuestCpu &cpu) {
  if (!cpu.dec_fired) return false;
  const uint64_t now = cpu.read_timebase_coarse();
  return now >= cpu.dec_base_tb && now - cpu.dec_base_tb >= cpu.dec_value;
}

// --- VI : 60/1,001 Hz sur l'horloge reelle, seule source de retrace ---------------------------
bool vi_owed = false;

bool vi_can_fire(GuestCpu &c) {
  return (hw::pi_enable & 0x100) && !c.inside_cb && !(hw::pi_cause & 0x100) &&
         (c.saved_dispatch == 0 || c.mmu.read32(kCurrentThread) == 0);
}

void fire_vi(GuestCpu &c) {
  hw::vi_raise();
  ++g_vi_fired;
  GX_FifoHashMark(g_vi_fired, c);
}

void start_vi_timer(GuestCpu &cpu) {
  uint64_t period = cpu.timebase_hz * 1001 / 60000;
  if (!period) period = 500000;
  Timers::get().every(period, [](GuestCpu &c, uint64_t) {
    if (vi_can_fire(c)) {
      fire_vi(c);
      vi_owed = false;
      return;
    }
    // Sur la console une VI qui tombe pendant un gestionnaire reste en attente et est servie
    // ensuite ; la sauter coutait une periode entiere (le jeu compte deux VI par image : 27/09).
    // Reportee sauf si masquee ou deja en attente. NJ_VIDROP=1 : on la saute.
    static const bool drop = NJ_ENV("NJ_VIDROP") != nullptr;
    if (!drop && (hw::pi_enable & 0x100) && !(hw::pi_cause & 0x100)) vi_owed = true;
    if (!(hw::pi_enable & 0x100)) ++g_vi_skip[0];
    else if (c.inside_cb) ++g_vi_skip[1];
    else if (hw::pi_cause & 0x100) ++g_vi_skip[2];
    else ++g_vi_skip[3];
  });
}

// --- journal ----------------------------------------------------------------------------------
void list_threads(GuestCpu &cpu, const char *tag) {
  const uint32_t cur = cpu.mmu.read32(kCurrentThread);
  uint32_t th = cpu.mmu.read32(kActiveThreads);
  for (int guard = 0; th && guard < 24; ++guard) {
    NJ_LOG << "  [" << tag << "] 0x" << std::hex << th << (th == cur ? "*" : " ") << " state=" << std::dec
              << cpu.mmu.read16(th + tho::state) << " prio=" << cpu.mmu.read32(th + tho::prio) << " waitq=0x"
              << std::hex << cpu.mmu.read32(th + tho::queue) << " srr0=0x" << cpu.mmu.read32(th + ctxo::srr0);
    uint32_t sp = cpu.mmu.read32(th + 4);   // pile d'appels : quelques retours
    for (int f = 0; f < 6 && sp && sp < 0x94000000u; ++f) {
      if (const uint32_t ret = cpu.mmu.read32(sp + 4)) NJ_LOG << " <-0x" << ret;
      sp = cpu.mmu.read32(sp);
    }
    NJ_LOG << std::dec << "\n";
    th = cpu.mmu.read32(th + tho::next_active);
  }
}

void heartbeat(GuestCpu &cpu) {
  NJ_LOG << "[Heartbeat] PC: 0x" << std::hex << cpu.pc << " LR: 0x" << cpu.lr << " intmr=0x"
            << hw::pi_enable.load() << " intsr=0x" << hw::pi_cause.load() << " msr=0x" << cpu.msr
            << " dec_h=0x" << cpu.mmu.read32(kExceptionTable + 4 * kExcDecrementer) << std::dec << "\n";
  list_threads(cpu, "Thread");
}

// Le decrementeur tombe 64 fois de suite au meme pc : une alarme de l'OS dont l'echeance est
// passee sans avoir ete servie retient le jeu. Les alarmes proches de r13 dont l'echeance est
// future sont ramenees a maintenant (releve du 09/2026, chargement bloque).
bool unstick_alarms(GuestCpu &cpu) {
  static uint32_t same = 0, last_pc = 0;
  if (cpu.pc != last_pc) {
    same = 0;
    last_pc = cpu.pc;
    return false;
  }
  if (++same < 64) return false;
  same = 0;
  nj::perf::g_unstick.fetch_add(1, std::memory_order_relaxed);
  const uint32_t base = cpu.gpr[13];
  if (!base) return true;
  const uint64_t now = cpu.timebase_now();
  const uint32_t now_hi = uint32_t(now >> 32), now_lo = uint32_t(now);
  int brought = 0;   // 19/10 : alarmes futures ramenees a maintenant (journal : un gel de 2,9 s ~ 64 echeances du decrementeur au meme pc)
  for (int i = 0; i < 512; ++i) {
    const uint32_t a = base - 4u * i;
    if (a < 0x80000000u || a > 0x81FFFFFFu) continue;
    const uint32_t handler = cpu.mmu.read32(a + 0x18);
    if (!(handler >= 0x80000000u && handler < 0x82000000u)) continue;
    const uint32_t hi = cpu.mmu.read32(a + 0x08), lo = cpu.mmu.read32(a + 0x0C);
    if ((!hi && !lo) || hi < now_hi || (hi == now_hi && lo <= now_lo)) continue;
    cpu.mmu.write32(a + 0x08, now_hi);
    cpu.mmu.write32(a + 0x0C, now_lo);
    ++brought;
    static uint32_t n = 0;
    if (n++ % 32 == 0)
      NJ_LOG << "[HLE DEC] alarme ramenee a maintenant 0x" << std::hex << a << " gestionnaire 0x" << handler
                << std::dec << '\n';
  }
  {
    char b[96];
    std::snprintf(b, sizeof b, "alarmes ramenees pc=%08x lr=%08x n=%d", cpu.pc, cpu.lr, brought);
    nj::perf::event(b);
  }
  return true;
}

// Retour d'un gestionnaire lance ici : le contexte que l'ordonnanceur a choisi reprend.
bool return_from_handler(GuestCpu &cpu) {
  if (cpu.saved_dispatch) {
    const uint32_t thread = cpu.mmu.read32(kCurrentThread);
    // Un gestionnaire imbrique a sauve un contexte d'exception, pas celui d'un fil : on le
    // reprend tel quel (SelectThread ne quitte jamais un tel contexte non plus).
    const bool nested = !outer_saved.empty();
    cpu.mmu.write32(kCurrentContext, !nested && thread && thread != cpu.saved_dispatch ? thread
                                                                                           : cpu.saved_dispatch);
    hle_dispatch_leave(cpu);
  }
  if (!load_context(cpu)) return false;
  // Le decrementeur compte pendant les gestionnaires, comme sur la console. Le remettre a
  // maintenant au retour (code herite) repoussait toute echeance plus longue que la periode
  // VI : l'alarme de 60 Hz de la version europeenne (16,667 ms < 16,683 ms) ne tombait jamais.
  static uint32_t restores = 0;
  if (cpu.pc == 0 || (++restores % 64) == 1)
    NJ_LOG << "[HLE PI] Context restore #" << std::dec << restores << " pc=0x" << std::hex << cpu.pc << " r1=0x"
              << cpu.gpr[1] << " msr=0x" << cpu.msr << std::dec << "\n";
  return true;
}

// Interruption externe la plus prioritaire, ou false.
bool pick_interrupt(uint32_t active, int &os_number, uint32_t &bit) {
  for (const PiSource &s : kPiSources)
    if (active & s.bit) {
      os_number = s.ask ? s.ask() : s.os;
      bit = s.bit;
      return true;
    }
  return false;
}

}  // namespace

// rfi du code genere : flottants du contexte choisi, puis gestionnaire exterieur s'il y en a.
void hle_dispatch_leave(GuestCpu &cpu) {
  switch_fpu(cpu);
  if (outer_saved.empty()) {
    cpu.saved_dispatch = 0;
    return;
  }
  cpu.saved_dispatch = outer_saved.back();
  outer_saved.pop_back();
}

// sc : RE4 (GameCube) ne s'en sert que pour la synchronisation.
bool service_syscall(GuestCpu &cpu) {
  const uint32_t pc = cpu.pc;
  if (drain_callbacks(cpu)) return true;
  cpu.pc = pc;
  return false;
}

bool nj_budget_slow(GuestCpu &cpu, uint32_t pc) {
  cpu.budget_sync();
  cpu.pc = pc;
  if (cpu.retired >= cpu.check_deadline || hw::poll_now.load(std::memory_order_relaxed))
    return drain_callbacks(cpu);   // rearme lui-meme
  cpu.budget_arm();
  return false;
}

bool drain_callbacks(GuestCpu &cpu) {
  ++g_ppc_calls;
  // Prochain passage dans 2 048 instructions (~20 us au Galaxy S5), ou des que poll_now est leve.
  // Le budget est consomme puis rearme avant tout code du jeu lance d'ici (gestionnaires), qui
  // decompte le meme budget. NJ_POLL=<instructions> pour comparer.
  static const uint64_t poll = [] {
    const char *e = NJ_ENV("NJ_POLL");
    const uint64_t v = e ? std::strtoull(e, nullptr, 0) : 0;
    return v ? v : 2048;
  }();
  cpu.budget_sync();
  cpu.check_deadline = cpu.retired + poll;
  cpu.budget_arm();
  GX_StageFlush();
  ::GX_DeterministicParse();

  // Toujours compte (15/10) : sous NJ_FRAMELOG seulement, l'attente restait a 0 en diffusion
  // (NJ_ENV mort) et la colonne « jeu » du journal valait la duree entiere de l'image.
  account_wait(cpu);
  sleep_in_wait_loop(cpu);
  hw::poll_now.store(false, std::memory_order_relaxed);

  if (cpu.pc == kReturnMark && !cpu.inside_cb && return_from_handler(cpu)) return true;

  // battement : une echeance plutot qu'un modulo 64 bits (__udivmoddi4 sur ARM 32 bits)
  static uint64_t next_beat = 20000000;
  if (cpu.retired >= next_beat) {
    next_beat = cpu.retired + 20000000;
    heartbeat(cpu);
  }
  if (cpu.pc == 0) {   // aucun fil pret
    static uint64_t idle = 0;
    if (++idle == 200000) {
      NJ_LOG << "[IDLE] aucun fil pret. intmr=0x" << std::hex << hw::pi_enable << " intsr=0x" << hw::pi_cause
                << std::dec << " dec=" << cpu.dec_fired << "\n";
      list_threads(cpu, "IdleThread");
    }
  }

  hw::di_step();

  if (cpu.vblank_due && !cpu.inside_cb && (cpu.msr & kMsrEE)) {   // NJ_HOSTVI
    cpu.vblank_due = false;
    hw::vi_raise();
  }
  if (cpu.inside_cb && cpu.pc == kReturnMark) cpu.inside_cb = false;
  if (cpu.inside_cb) return false;

  static bool vi_started = false;
  if (!vi_started) {
    vi_started = true;
    start_vi_timer(cpu);
  }
  Timers::get().run_due(cpu, cpu.read_timebase_coarse());
  if (vi_owed && vi_can_fire(cpu)) {   // VI reportee : servie des que possible
    vi_owed = false;
    fire_vi(cpu);
    ++g_vi_late;
  }

  // decrementeur
  if (dec_expired(cpu) && (cpu.msr & kMsrEE) && !cpu.inside_cb) {
    cpu.dec_fired = false;
    // Toujours servir l'interruption : la jeter (code herite) laissait le decrementeur desarme,
    // que seul le gestionnaire de l'OS rearme ; dans une boucle d'attente (meme pc) les alarmes
    // s'arretaient pour de bon (Europe : gel apres ~50 s, 05/10).
    unstick_alarms(cpu);
    const uint32_t handler = cpu.mmu.read32(kExceptionTable + 4 * kExcDecrementer);
    const uint32_t current = cpu.mmu.read32(kCurrentContext);
    if (valid_handler(handler) && current) raise_exception(cpu, handler, kExcDecrementer, current);
  }

  // interruptions externes (PI)
  const uint32_t active = hw::pi_cause & hw::pi_enable;
  int os_number = -1;
  uint32_t bit = 0;
  if (active && (cpu.msr & kMsrEE) && pick_interrupt(active, os_number, bit) && os_number >= 0) {
    const uint32_t handler = cpu.mmu.read32(kInterruptTable + 4 * os_number);
    if (!valid_handler(handler)) {
      static uint32_t last_pc = 0;
      static int warned;
      if (cpu.pc != last_pc) {
        warned = 0;
        last_pc = cpu.pc;
      }
      if (warned++ < 3)
        NJ_LOG << "[HLE PI] interruption " << os_number << " sans gestionnaire (OS en reinitialisation)\n";
      return false;
    }
    const uint32_t current = cpu.mmu.read32(kCurrentContext);
    if (current) {
      hw::pi_cause &= ~bit;
      raise_exception(cpu, handler, os_number, current);
    }
    // Pas encore de contexte de l'OS (demarrage) : gestionnaire en mode rappel, pile hote.
    cpu.msr &= ~kMsrEE;
    cpu.push_saved(cpu.msr | kMsrEE);
    cpu.inside_cb = true;
    ++cpu.cb_nesting;
    jump_to_handler(cpu, handler, os_number, 0, cpu.msr | kMsrEE);
  }
  return false;
}

}  // namespace nj::runtime
