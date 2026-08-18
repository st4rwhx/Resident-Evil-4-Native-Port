// DI (interface du lecteur, 0xCC006000) : les commandes que le SDK envoie au lecteur (YAGCD § DI)
// et leurs interruptions. Les lectures (0xA8) viennent de l'image du disque (DiscView).
//
// Registres : 0x00 etat (DISR), 0x04 capot (DICVR), 0x08/0x0C/0x10 commande, 0x14 adresse
// memoire, 0x18 longueur, 0x1C controle (bit 0 : lancer), 0x20 reponse immediate, 0x24 config.
// DISR : bit 0 BRK (demande d'arret), puis paires masque/drapeau : 1/2 erreur, 3/4 transfert
// fini, 5/6 arret fait.
#include <atomic>
#include <cstring>
#include <string>
#include "runtime/log.h"
#include <iostream>
#include <vector>

#include "runtime/config.h"
#include "runtime/cpu_context.h"
#include "runtime/env.h"
#include "runtime/hw/hw.h"
#include "runtime/perf_journal.h"
#include <chrono>
#include "runtime/timers.h"
#include "runtime/virtual_disc.h"

namespace nj::runtime {
extern GuestCpu *g_guest;
extern MMU *g_guest_mem;
}  // namespace nj::runtime

namespace nj::runtime::hw {

int di_irq_delay = 0;

namespace {
constexpr uint32_t kBase = 0xCC006000u, kDIIrq = 0x04;   // cause PI du lecteur
constexpr uint32_t kBreak = 0x01, kErrMask = 0x02, kErr = 0x04, kDoneMask = 0x08, kDone = 0x10;
constexpr uint32_t kBrkMask = 0x20, kBrkDone = 0x40;
constexpr uint32_t kCoverMask = 0x02, kCover = 0x04;

uint32_t sr = 0, cover = 0, cmd[3] = {}, mem_addr = 0, length = 0, ctrl = 0, immediate = 0;
bool audio_stream_on = false;
Timers::Id read_end = 0;   // fin programmee de la lecture en cours (un arret l'annule)

// Journal court des commandes du lecteur (19/10), pour le detecteur de gel : une fin de lecture
// perdue ou doublee laisse le jeu au repos (SelectThread inactif) jusqu'a une temporisation.
struct DiEvent {
  int64_t ms = 0;
  char kind = 0;      // R debut de lecture, D fin, B arret, X lecture lancee sur une autre en cours, C autre commande
  uint32_t a = 0, b = 0;
};
constexpr int kDiEvents = 24;
DiEvent di_events[kDiEvents];
std::atomic<uint32_t> di_event_n{0};
std::atomic<uint32_t> di_reads_started{0}, di_reads_done{0};
int64_t di_now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
void di_note(char kind, uint32_t a, uint32_t b) {
  const uint32_t i = di_event_n.fetch_add(1, std::memory_order_relaxed);
  di_events[i % kDiEvents] = DiEvent{di_now_ms(), kind, a, b};
}

void update_line() {
  const bool on = ((sr & kDone) && (sr & kDoneMask)) || ((sr & kErr) && (sr & kErrMask)) ||
                  ((sr & kBrkDone) && (sr & kBrkMask)) || ((cover & kCover) && (cover & kCoverMask));
  if (on) raise_irq(kDIIrq);
  else lower_irq(kDIIrq);
}

// Lecture 0xA8 : copie immediate des octets, fin annoncee apres une duree realiste.
//
// Pourquoi une duree : RE4 (snd_str2.cpp) appelle DVDReadAsyncPrio puis pose str->dvd_busy = 1.
// Une fin de lecture servie avant l'instruction suivante remettait dvd_busy a 0 trop tot, le flux
// audio se croyait en lecture pour toujours et la reinitialisation (Exit?, GAME RESET) l'attendait
// sans fin. 1 ms d'acces plus 12 Mo/s (quatre fois la console, pour des chargements courts).
void start_read() {
  auto &disc = DiscView::get();
  if (!disc.valid()) disc.init(Config::get().game_dir);
  const uint64_t offset = (uint64_t)cmd[1] << 2;
  const uint32_t n = length ? length : cmd[2];
  const uint32_t dst = 0x80000000u | (mem_addr & 0x01FFFFFFu);
  if (g_guest_mem && n > 0 && n < 0x4000000) {
    const auto t_read = std::chrono::steady_clock::now();   // journal de performance : part du disque
    std::vector<uint8_t> buf(n, 0);
    const size_t got = disc.read(offset, n, buf.data());
    if (got < n) {
      NJ_LOG << "[DI] lecture hors fichiers : 0x" << std::hex << offset << " +0x" << n << " (lus 0x" << got
                << ")" << std::dec << "\n";
      std::memset(buf.data() + got, 0, n - got);
    }
    for (uint32_t i = 0; i < n; ++i) g_guest_mem->write8(dst + i, buf[i]);
    nj::perf::g_disc_us += (uint64_t)((std::chrono::steady_clock::now() - t_read) / std::chrono::microseconds(1));
    nj::perf::g_disc_bytes += n;
    nj::perf::note_file(disc.file_at(offset));
  }
  ctrl |= 1;   // occupe jusqu'a la fin
  if (read_end) di_note('X', (uint32_t)offset, n);
  di_note('R', (uint32_t)offset, n);
  di_reads_started.fetch_add(1, std::memory_order_relaxed);
  read_end = Timers::get().after(40500 + (uint64_t)n * 27 / 8, [](GuestCpu &, uint64_t late) {
    di_note('D', (uint32_t)(late / 40), 0);   // retard du declenchement, en ~microsecondes
    di_reads_done.fetch_add(1, std::memory_order_relaxed);
    read_end = 0;
    ctrl &= ~1u;
    length = 0;
    sr |= kDone;
    update_line();
  });
}

void run_command() {
  const uint32_t op = cmd[0] >> 24, sub = (cmd[0] >> 16) & 0xFF;
  if (op != 0xA8) di_note('C', op, sub);
  ctrl &= ~1u;
  switch (op) {
    case 0xA8:
      start_read();
      break;
    case 0x12:   // identification du lecteur : date de revision a +4
      if (g_guest_mem && length > 0 && length <= 0x40) {
        const uint32_t dst = 0x80000000u | (mem_addr & 0x01FFFFFFu);
        for (uint32_t i = 0; i < length; ++i) g_guest_mem->write8(dst + i, 0);
        if (length >= 8) g_guest_mem->write32(dst + 4, 0x20020402);
      }
      length = 0;
      sr |= kDone;
      break;
    case 0xE1:   // flux audio du disque : marche / arret
    case 0xE0:
    case 0xAB:
      if (sub == 0x00 && (cmd[1] || cmd[2])) audio_stream_on = true;
      else if (sub == 0x01) audio_stream_on = false;
      immediate = 0;
      sr |= kErr;
      break;
    case 0xE2:   // etat du flux audio
      immediate = (sub == 0x00 && audio_stream_on) ? 1 : 0;
      sr |= kErr;
      break;
    default:
      if (op != 0xE3 && op != 0xE4)
        NJ_LOG << "[DI] commande 0x" << std::hex << op << " ignoree" << std::dec << "\n";
      immediate = 0;
      sr |= kErr;
      break;
  }
  update_line();
}

uint32_t di_read(uint32_t addr) {
  switch (addr - kBase) {
    case 0x00: return sr;
    case 0x04: return cover;
    case 0x08: return cmd[0];
    case 0x0C: return cmd[1];
    case 0x10: return cmd[2];
    case 0x14: return mem_addr;
    case 0x18: return length;
    case 0x1C: return ctrl;
    case 0x20: return immediate;
    case 0x24: return 0xFF;
  }
  return 0;
}

void di_write(uint32_t addr, uint32_t v) {
  switch (addr - kBase) {
    case 0x00: {
      // drapeaux : 1 acquitte ; masques : recopies. BRK n'acquitte rien, c'est une demande
      // d'arret (DVDCancel pendant une lecture) : la lecture s'arrete et « arret fait » se leve,
      // le rappel de la lecture recevra DVD_RESULT_CANCELED.
      const uint32_t masks = kErrMask | kDoneMask | kBrkMask;
      sr = (sr & ~(v & (kErr | kDone | kBrkDone)) & ~masks) | (v & masks);
      if (v & kBreak) {
        di_note('B', read_end ? 1 : 0, 0);
        if (read_end) Timers::get().cancel(read_end);
        read_end = 0;
        ctrl &= ~1u;
        sr = (sr & ~kBreak) | kBrkDone;
      }
      update_line();
      break;
    }
    case 0x04:
      cover = (cover & ~(v & kCover) & ~kCoverMask) | (v & kCoverMask);
      update_line();
      break;
    case 0x08: cmd[0] = v; break;
    case 0x0C: cmd[1] = v; break;
    case 0x10: cmd[2] = v; break;
    case 0x14: mem_addr = v; break;
    case 0x18: length = v; break;
    case 0x1C:
      ctrl = v;
      if (v & 1) run_command();
      break;
    case 0x20: immediate = v; break;
  }
}
}  // namespace

// Etat du lecteur pour le detecteur de gel (perf_journal.cpp) : lu depuis un autre fil, sans verrou
// (diagnostic : une valeur un peu en retard est acceptable).
std::string di_debug_dump() {
  char b[160];
  std::snprintf(b, sizeof b, " di sr=%x ctrl=%x fin=%d lect=%u/%u irq=%x/%x", sr, ctrl, read_end ? 1 : 0,
                di_reads_done.load(), di_reads_started.load(), hw::pi_cause.load(), hw::pi_enable.load());
  std::string s = b;
  const int64_t now = di_now_ms();
  const uint32_t n = di_event_n.load();
  for (uint32_t k = n > 10 ? n - 10 : 0; k < n; ++k) {
    const DiEvent &e = di_events[k % kDiEvents];
    if (!e.kind) continue;
    std::snprintf(b, sizeof b, " %c%lld:%x/%x", e.kind, (long long)(now - e.ms), e.a, e.b);
    s += b;
  }
  const Timers &t = Timers::get();
  const uint64_t due = t.next_due(), tn = t.now();
  std::snprintf(b, sizeof b, " tim=%lld", due == ~0ull ? -1ll : (long long)((int64_t)(due - tn) / 40500));
  return s + b;
}

void di_step() {
  if (di_irq_delay > 0 && --di_irq_delay == 0) update_line();
}

void attach_di(IoBus &bus) { bus.map(kBase, kBase + 0xFF, di_read, di_write); }

}  // namespace nj::runtime::hw
