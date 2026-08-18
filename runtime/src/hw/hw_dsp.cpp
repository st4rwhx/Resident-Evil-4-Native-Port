// DSP, ARAM et DMA audio (0xCC005000), d'apres YAGCD (§ DSP, § ARAM, § AI DMA).
//
// - Boites aux lettres : le CPU ecrit un courrier de 32 bits en deux moities (0x00 haute, 0x02
//   basse) ; le DSP repond par 0x04/0x06, bit 15 de la moitie haute = « courrier present ».
//   Le microcode audio (AX) n'est pas execute : ax.cpp mixe les voix et nous repondons ici ce que
//   le microcode repondrait, avec un petit delai.
// - Registre de controle 0x0A (CSR) : bit 0 reset, bit 11 arret ; trois sources d'interruption,
//   chacune en paire « autorisee / levee » : AI DMA (bits 2/3), ARAM DMA (4/5), DSP (6/7) ;
//   ecrire 1 sur un bit « levee » l'acquitte.
// - DMA ARAM : MEM1 <-> ARAM (0x20 adresse MEM1, 0x24 adresse ARAM, 0x28 compte, bit 31 = sens
//   ARAM -> MEM1).
// - DMA audio : bloc de sortie du mixeur (0x30/0x32 adresse, 0x36 longueur en blocs de 32 octets,
//   bit 15 = marche), lu toutes les 5 ms et envoye a l'hote.
#include <algorithm>
#include <chrono>
#include "runtime/log.h"
#include <cstdio>
#include <cstring>
#include <deque>
#include <iostream>
#include <mutex>
#include <vector>

#include "runtime/config.h"
#include "runtime/cpu_context.h"
#include "runtime/env.h"
#include "runtime/hw/hw.h"
#include "runtime/perf_journal.h"
#include "runtime/timers.h"

namespace nj::runtime {
extern MMU *g_guest_mem;
extern GuestCpu *g_guest;
}  // namespace nj::runtime

namespace nj::runtime::hle {
void ax_hle_command_list(uint32_t list_addr, uint32_t size_words);   // ax.cpp
}

namespace nj::runtime::hw {

namespace {
constexpr uint32_t kBase = 0xCC005000u, kDSPIrq = 0x40;   // cause PI du DSP
constexpr uint16_t kReset = 0x0001, kHalt = 0x0800, kMailIrqOn = 0x0100;
constexpr uint16_t kAiDone = 0x0008, kAramDone = 0x0020, kDspDone = 0x0080;
constexpr uint16_t kRaised = kAiDone | kAramDone | kDspDone;

uint16_t csr = kHalt;
uint16_t cpu_box_hi = 0;               // vue du courrier CPU -> DSP (toujours « lu »)
uint16_t dsp_box_hi = 0, dsp_box_lo = 0;
uint16_t aram_size = 0, aram_mode = 0, aram_refresh = 0;
uint32_t aram_mem = 0, aram_addr = 0, aram_count = 0;
uint32_t out_addr = 0;
uint16_t out_ctl = 0;
Timers::Id out_timer = 0;

std::mutex ring_lock;
std::vector<int16_t> ring;   // gauche, droite, ...

void sync_line() {
  if ((csr >> 1) & csr & kRaised) raise_irq(kDSPIrq);
  else lower_irq(kDSPIrq);
}

// Reponses du DSP : une seule visible a la fois, la suivante apparait quand le CPU a lu la
// moitie basse de la precedente (deux reponses arrivees ensemble ne doivent pas s'ecraser :
// c'etait le gel des voix de RE4 entre le menu et la partie).
struct Reply {
  uint32_t mail;
  bool irq;
};
std::deque<Reply> replies;

void show_next_reply() {
  if ((dsp_box_hi & 0x8000) || replies.empty()) return;
  const Reply r = replies.front();
  replies.pop_front();
  dsp_box_hi = (uint16_t)((r.mail >> 16) | 0x8000);
  dsp_box_lo = (uint16_t)r.mail;
  if (r.irq) {
    csr |= kDspDone;
    sync_line();
  }
}

void reply_later(uint32_t mail, bool irq) {
  Timers::get().after(g_guest->timebase_hz / 1000, [mail, irq](GuestCpu &, uint64_t) {
    replies.push_back({mail, irq});
    show_next_reply();
  });
}

// Ce que le microcode AX repond au CPU (protocole des courriers 0xBABE / 0xCDD1).
void ax_mail(uint32_t mail) {
  static int state = 0;           // 0 attente, 1 adresse de liste attendue, 2 liste mixee
  static uint16_t list_words = 0;
  auto answer = [](uint16_t code) { reply_later(0xDCD10000u | code, true); };
  if (state == 1) {   // l'adresse de la liste de commandes : on mixe l'image, puis DSP_YIELD
    hle::ax_hle_command_list(mail, list_words);
    answer(0x0002);
    state = 2;
    return;
  }
  if ((mail >> 16) == 0xBABE) {
    list_words = (uint16_t)mail;
    state = 1;
    return;
  }
  if (state == 2 && mail == 0xCDD10003u) {   // continuer : rien a repondre
    state = 0;
    return;
  }
  if (mail == 0xCDD10002u) {
    // reset : le DSP retourne a sa ROM, qui s'annonce par 0x8071FEED sans interruption
    // (__DSP_boot_task l'attend par lecture directe avant d'envoyer la tache suivante)
    reply_later(0x8071FEEDu, false);
    state = 0;
    return;
  }
  if ((mail >> 16) == 0xCDD1) {
    // reprise (0000 apres une liste, ou 0001) : DSP_RESUME ; les autres : DSP_INIT
    const bool resume = (state == 2 && mail == 0xCDD10000u) || mail == 0xCDD10001u;
    state = 0;
    answer(resume ? 0x0001 : 0x0000);
    return;
  }
  answer(0x0000);
}

void aram_dma() {
  const bool to_mem1 = (aram_count & 0x80000000u) != 0;
  const uint32_t n = aram_count & 0x7FFFFFFFu;
  const uint32_t m = aram_mem & 0x01FFFFFFu, a = aram_addr & 0x00FFFFFFu;
  if (g_guest_mem && n) {
    const uint32_t m1 = (uint32_t)g_guest_mem->mem1.size(), m2 = (uint32_t)g_guest_mem->mem2.size();
    if (m < m1 && a < m2) {
      uint32_t len = n;
      if (len > m1 - m) len = m1 - m;
      if (len > m2 - a) len = m2 - a;
      if (to_mem1) std::memcpy(g_guest_mem->mem1.data() + m, g_guest_mem->mem2.data() + a, len);
      else std::memcpy(g_guest_mem->mem2.data() + a, g_guest_mem->mem1.data() + m, len);
    }
  }
  Timers::get().after((uint64_t)n / 2 + 64, [](GuestCpu &, uint64_t) {
    aram_count = 0;
    csr |= kAramDone;
    sync_line();
  });
}

// Toutes les 5 ms tant que le DMA audio tourne : le bloc mixe part vers l'hote.
// AX accelere pendant une attente du jeu (20/10/2026). A l'entree d'une salle, RE4 attend dans une boucle (SndRoomBgmLoad : « tant que la musique
// de la salle precedente n'a pas fini son fondu, appeler SndWatcher ») que le pilote sonore ait avance de 400 images AX (2 s a 200 images/s) ; sur la
// console, le chargement du disque est plus long que cela et on ne s'en apercoit pas ; ici le chargement est instantane, donc le jeu ne dessine plus
// pendant ~2 a 4 s (les « gels » des journaux : fil du jeu a 96-99 % d'un coeur, pc dans Snd*, aucune alarme rattrapee). Quand le jeu n'a rien dessine depuis
// 150 ms ET execute plus de 100 millions d'instructions par seconde (une boucle, pas une lecture de disque), les images AX sont servies 8 fois plus
// vite : le fondu finit en ~250 ms. Le son produit pendant ce temps est jete par l'anneau de sortie (plein) ; retour a 200/s des que le jeu dessine.
bool g_ax_boost = false;
void audio_dma_tick(GuestCpu &, uint64_t);
void rearm_audio_timer(bool boost) {
  if (out_timer) Timers::get().cancel(out_timer);
  out_timer = Timers::get().every(g_guest->timebase_hz / (boost ? 1600 : 200), audio_dma_tick);
  g_ax_boost = boost;
}

void audio_dma_tick(GuestCpu &cpu, uint64_t) {
  nj::perf::g_ax_ticks.fetch_add(1, std::memory_order_relaxed);
  {
    static uint64_t last_ret = 0;
    static int64_t last_us = 0;
    const int64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (nj::perf::ms_since_frame() >= 150) {
      const int64_t dt = now_us - last_us;
      const bool busy = last_us && dt > 0 && dt < 100000 && (double)(cpu.retired - last_ret) * 1e6 / (double)dt > 100e6;
      if (busy && !g_ax_boost) rearm_audio_timer(true);
    } else if (g_ax_boost) {
      rearm_audio_timer(false);
    }
    last_ret = cpu.retired;
    last_us = now_us;
  }
  if (!(out_ctl & 0x8000)) return;
  const uint32_t bytes = (uint32_t)(out_ctl & 0x7FFF) * 32;
  if (g_guest_mem && bytes) {
    static FILE *dump = [] {   // NJ_AUDIO_DUMP=<fichier> : s16 stereo 32 kHz brut
      const char *p = NJ_ENV("NJ_AUDIO_DUMP");
      return p ? std::fopen(p, "wb") : nullptr;
    }();
    std::lock_guard<std::mutex> g(ring_lock);
    if (ring.size() < 64000) {
      const uint32_t va = out_addr | 0x80000000u;
      for (uint32_t at = va; at + 4 <= va + bytes; at += 4) {
        // le mixeur ecrit droite puis gauche
        const int16_t r = (int16_t)g_guest_mem->read16(at), l = (int16_t)g_guest_mem->read16(at + 2);
        ring.push_back(l);
        ring.push_back(r);
        if (dump) {
          const int16_t f[2] = {l, r};
          std::fwrite(f, sizeof f, 1, dump);
        }
      }
    }
  }
  csr |= kAiDone;
  sync_line();
}

uint32_t dsp_read(uint32_t addr) {
  switch (addr - kBase) {
    case 0x00: case 0x02: return cpu_box_hi;
    case 0x04: return dsp_box_hi;
    case 0x06: {   // lire la moitie basse vide la boite et fait paraitre la reponse suivante
      const uint16_t lo = dsp_box_lo;
      dsp_box_hi &= ~0x8000;
      csr &= ~kDspDone;
      sync_line();
      show_next_reply();
      return lo;
    }
    case 0x08: case 0x0A: return csr;
    case 0x12: return aram_size;
    case 0x16: return aram_mode | 1;   // ARAM toujours prete
    case 0x1A: return aram_refresh;
    case 0x20: return aram_mem;
    case 0x24: return aram_addr;
    case 0x28: return aram_count;
    case 0x30: return out_addr >> 16;
    case 0x32: return out_addr & 0xFFFF;
    case 0x36: return out_ctl;
  }
  return 0;
}

void dsp_write(uint32_t addr, uint32_t v) {
  static const bool trace = NJ_ENV("NJ_DSPTRACE") != nullptr;
  switch (addr - kBase) {
    case 0x00: case 0x02: {
      static uint16_t hi = 0;
      if (addr - kBase == 0x00) hi = (uint16_t)v;
      cpu_box_hi = (uint16_t)(v & 0x7FFF);
      // le courrier est complet quand la moitie basse est ecrite
      if (addr - kBase == 0x02 && (csr & kMailIrqOn)) ax_mail(((uint32_t)hi << 16) | (v & 0xFFFF));
      if (trace)
        NJ_LOG << "[DSP] courrier CPU +" << std::hex << (addr - kBase) << " = 0x" << v << std::dec << "\n";
      break;
    }
    case 0x04: case 0x06:
      if (v & 0x8000) dsp_box_hi &= ~0x8000;
      break;
    case 0x08: case 0x0A: {
      const uint16_t w = (uint16_t)v;
      const bool released = (csr & kHalt) && !(w & kHalt);
      csr = (uint16_t)((csr & kRaised & ~w) | (w & ~(kRaised | kReset)));
      if ((w & kReset) || released) {   // un reset perd les reponses non lues
        replies.clear();
        dsp_box_hi = 0xDCD1;
        dsp_box_lo = 0;
      }
      sync_line();
      break;
    }
    case 0x12: aram_size = (uint16_t)v; break;
    case 0x16: aram_mode = (uint16_t)v; break;
    case 0x1A: aram_refresh = (uint16_t)v; break;
    case 0x20: aram_mem = v; break;
    case 0x24: aram_addr = v; break;
    case 0x28: aram_count = v; aram_dma(); break;
    case 0x30: out_addr = (out_addr & 0xFFFF) | ((v & 0xFFFF) << 16); break;
    case 0x32: out_addr = (out_addr & 0xFFFF0000u) | (v & 0xFFFF); break;
    case 0x36: {
      const bool was = out_ctl & 0x8000;
      out_ctl = (uint16_t)v;
      const bool on = out_ctl & 0x8000;
      if (on && !was) {
        if (out_timer) Timers::get().cancel(out_timer);
        g_ax_boost = false;
        out_timer = Timers::get().every(g_guest->timebase_hz / 200, audio_dma_tick);
      } else if (!on && was && out_timer) {
        Timers::get().cancel(out_timer);
        out_timer = 0;
      }
      break;
    }
  }
}

// Registres DMA ARAM en moities de 16 bits (le SDK les ecrit ainsi) : haute a +0, basse a +2.
uint16_t dsp_read16(uint32_t addr) {
  switch (addr - kBase) {
    case 0x20: return (uint16_t)(aram_mem >> 16);
    case 0x22: return (uint16_t)aram_mem;
    case 0x24: return (uint16_t)(aram_addr >> 16);
    case 0x26: return (uint16_t)aram_addr;
    case 0x28: return (uint16_t)(aram_count >> 16);
    case 0x2A: return (uint16_t)aram_count;
  }
  return (uint16_t)dsp_read(addr);
}

void dsp_write16(uint32_t addr, uint16_t v) {
  auto hi = [v](uint32_t &r) { r = (r & 0x0000FFFFu) | ((uint32_t)v << 16); };
  auto lo = [v](uint32_t &r) { r = (r & 0xFFFF0000u) | v; };
  switch (addr - kBase) {
    case 0x20: hi(aram_mem); return;
    case 0x22: lo(aram_mem); return;
    case 0x24: hi(aram_addr); return;
    case 0x26: lo(aram_addr); return;
    case 0x28: hi(aram_count); return;
    case 0x2A: lo(aram_count); aram_dma(); return;
  }
  dsp_write(addr, v);
}
}  // namespace

size_t audio_pull(int16_t *out, size_t frames) {
  std::lock_guard<std::mutex> g(ring_lock);
  const size_t n = std::min(frames, ring.size() / 2);
  std::memcpy(out, ring.data(), n * 2 * sizeof(int16_t));
  ring.erase(ring.begin(), ring.begin() + (ptrdiff_t)(n * 2));
  if (n < frames) std::memset(out + n * 2, 0, (frames - n) * 2 * sizeof(int16_t));
  return n;
}

size_t audio_buffered() {
  std::lock_guard<std::mutex> g(ring_lock);
  return ring.size() / 2;
}

void audio_trim(size_t keep_frames) {
  std::lock_guard<std::mutex> g(ring_lock);
  if (ring.size() / 2 > keep_frames)
    ring.erase(ring.begin(), ring.end() - (ptrdiff_t)(keep_frames * 2));
}

// Vecteur OS de l'interruption la plus prioritaire : DSP 7, ARAM 6, AI 5.
int dsp_os_irq() {
  const uint16_t up = (csr >> 1) & csr & kRaised;
  if (up & kDspDone) return 7;
  if (up & kAramDone) return 6;
  if (up & kAiDone) return 5;
  return 7;
}

void dsp_raise() {
  if (csr & kMailIrqOn) {
    csr |= kDspDone;
    raise_irq(kDSPIrq);
  }
}

void attach_dsp(IoBus &bus) { bus.map(kBase, kBase + 0xFF, dsp_read, dsp_write, dsp_read16, dsp_write16); }

}  // namespace nj::runtime::hw
