#include "runtime/hw/hw.h"
#include "runtime/log.h"
#include "runtime/config.h"
#include "runtime/cpu_context.h"
#include "runtime/env.h"
#include "runtime/timers.h"
#include "runtime/saves.h"
#include "runtime/report.h"
#include "common/platform.h"

#include <cstdio>
#include <cstring>
#include <set>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

namespace nj::runtime {
extern MMU *g_guest_mem;
}

namespace nj::runtime::hw {

// EXI - External Interface, 0xCC006800..0xCC00683F.
//
// Three channels of an SPI-like bus. Five 32-bit registers per channel, 0x14
// stride:
//
//   +0x00 CSR     chip selects, clock, interrupt mask/status
//   +0x04 MAR     DMA start address (physical)
//   +0x08 LENGTH  DMA byte count
//   +0x0C CR      TSTART / DMA / RW / TLEN
//   +0x10 DATA    immediate-mode data, MSB-justified
//
// Transfers run synchronously: the store that sets CR's TSTART bit performs
// the whole transfer and clears the bit before it returns. That is exactly
// what the SDK wants -- EXISync() polls TSTART directly and pulls DATA
// itself rather than waiting on the interrupt, so its spin loop exits on its
// first read:
//
//   while (exi->state & EXI_STATE_BUSY)
//       if (!(__EXIRegs[chan * 5 + 3] & 0x01)) { ...read DATA...; break; }
//
// Channel 0 carries two devices: device 1, the "IPL" (mask ROM / RTC / SRAM /
// UART), and device 0, memory card slot A (below). Slot B (channel 1) stays
// empty.

static constexpr uint32_t EXI_BASE = 0xCC006800;

static constexpr uint32_t CSR_EXIINTMASK = 1u << 0;
static constexpr uint32_t CSR_EXIINT = 1u << 1;
static constexpr uint32_t CSR_TCINTMASK = 1u << 2;
static constexpr uint32_t CSR_TCINT = 1u << 3;
static constexpr uint32_t CSR_CS_SHIFT = 7; // three chip selects, bits 7..9
static constexpr uint32_t CSR_CS_MASK = 7u << CSR_CS_SHIFT;
static constexpr uint32_t CSR_EXTINTMASK = 1u << 10;
static constexpr uint32_t CSR_EXTINT = 1u << 11;
static constexpr uint32_t CSR_EXT = 1u << 12;

static constexpr uint32_t CSR_INT_BITS = CSR_EXIINT | CSR_TCINT | CSR_EXTINT;

static constexpr uint32_t CR_TSTART = 1u << 0;
static constexpr uint32_t CR_DMA = 1u << 1;
static constexpr uint32_t CR_RW_SHIFT = 2;   // 0 read, 1 write, 2 read-write
static constexpr uint32_t CR_TLEN_SHIFT = 4; // immediate length minus one

struct ExiChannel {
  uint32_t csr = 0;
  uint32_t mar = 0;
  uint32_t length = 0;
  uint32_t cr = 0;
  uint32_t data = 0;
};
static ExiChannel exi_ch[3];

// --- channel 0, device 1: mask ROM / RTC / SRAM / UART ---------------------
//
// Every transaction opens with a four-byte command word clocked out while the
// device is selected: bit 31 is the direction, the rest is an offset into the
// device's address space. The SDK only ever issues three of them, and it
// writes the literal constants below:
//
//   0x20000000  RTC counter, 4 bytes   (__OSGetSystemTime / OSGetTime)
//   0x20000100  SRAM, 64 bytes         (__OSInitSram / __OSWriteSram)
//   0x20010000  UART                   (the EXI debug console)
//
// Anything below 0x00100000 is the IPL font ROM, which we do not carry. Reads
// there return zero; the SDK notices the missing font signature and falls
// back to its built-in one.

static uint32_t ipl_cmd = 0;
static int ipl_cmd_bytes = 0; // 0..4, command word being shifted in
static uint32_t ipl_offset = 0;
static bool ipl_cmd_write = false;

static uint8_t sram[64];
static bool sram_ready = false;
static std::string uart_line;

// Checksum over SRAM bytes 0x0C..0x13, stored at 0x00/0x02 as sum and
// one's-complement sum. Same routine the IPL uses, so __OSInitSram accepts
// the block instead of rewriting it with defaults.
static void sram_fix_checksums() {
  uint16_t sum = 0, sum_inv = 0;
  for (int i = 0x0C; i < 0x14; i += 2) {
    uint16_t v = (uint16_t)((sram[i] << 8) | sram[i + 1]);
    sum += v;
    sum_inv += (uint16_t)~v;
  }
  sram[0] = (uint8_t)(sum >> 8);
  sram[1] = (uint8_t)(sum & 0xFF);
  sram[2] = (uint8_t)(sum_inv >> 8);
  sram[3] = (uint8_t)(sum_inv & 0xFF);
}

static void sram_init() {
  if (sram_ready)
    return;
  std::memset(sram, 0, sizeof(sram));
  // 0x04 ead0, 0x08 ead1, 0x0C counterBias - all zero.
  sram[0x10] = 0;    // displayOffsetH
  sram[0x11] = 0;    // ntd
  sram[0x12] = 0;    // language: 0 = English (OSGetLanguage)
  sram[0x13] = 0x04; // flags: bit 2 = stereo sound. Progressive (0x80) and
                     // EuRgb60 (0x40) deliberately off - VIInit picks its
                     // mode from the 0x800000CC OS global, and claiming
                     // progressive here would put it out of step with it.
  // OSSramEx : flashID des deux emplacements nul, donc leur somme de controle
  // (~somme) vaut 0xFF. DoMount la verifie pour une carte deja deverrouillee.
  sram[0x3A] = 0xFF;
  sram[0x3B] = 0xFF;
  sram_fix_checksums();
  sram_ready = true;
}

// GameCube RTC counts seconds since 2000-01-01 00:00:00.
static uint32_t rtc_now() {
  // Oracle CPU (NJ_DETERMINISTIC) : horloge temps reel figee (1er janvier 2025), pour que deux
  // parties voient la meme date.
  static const bool det = NJ_ENV("NJ_DETERMINISTIC") != nullptr;
  if (det) return 788918400u;
  static constexpr int64_t kGcEpoch = 946684800; // 2000-01-01, Unix seconds
  int64_t t = (int64_t)std::time(nullptr) - kGcEpoch;
  return t > 0 ? (uint32_t)t : 0u;
}

// One byte over the wire. `out` is what the CPU is driving, the return value
// is what the device answers on the same clock.
static uint8_t ipl_transfer_byte(uint8_t out) {
  if (ipl_cmd_bytes < 4) {
    ipl_cmd = (ipl_cmd << 8) | out;
    if (++ipl_cmd_bytes == 4) {
      ipl_cmd_write = (ipl_cmd & 0x80000000u) != 0;
      ipl_offset = 0;
    }
    return 0xFF; // the device holds the bus high while it takes the command
  }

  const uint32_t region = ipl_cmd & 0x7FFFFF00u;
  uint8_t in = 0;

  if (region == 0x20000000u) { // RTC counter
    const uint32_t t = rtc_now();
    if (ipl_offset < 4)
      in = (uint8_t)(t >> (24 - 8 * ipl_offset));
  } else if (region == 0x20000100u) { // SRAM
    sram_init();
    const uint32_t i = ipl_offset & 0x3F;
    if (ipl_cmd_write)
      sram[i] = out;
    else
      in = sram[i];
  } else if (region == 0x20010000u) { // UART, the EXI debug console
    if (ipl_cmd_write && out) {
      if (out == '\n' || uart_line.size() >= 240) {
        NJ_LOG << "[EXI UART] " << uart_line << "\n";
        uart_line.clear();
      }
      if (out != '\n' && out != '\r')
        uart_line.push_back((char)out);
    }
    in = 0x04; // UART status: transmit buffer free
  }

  ipl_offset++;
  return in;
}

static void ipl_reset_transaction() {
  ipl_cmd = 0;
  ipl_cmd_bytes = 0;
  ipl_offset = 0;
  ipl_cmd_write = false;
}

static void exi_update_interrupt() {
  bool assert_int = false;
  for (int i = 0; i < 3; ++i) {
    const uint32_t csr = exi_ch[i].csr;
    if ((csr & CSR_EXIINT) && (csr & CSR_EXIINTMASK))
      assert_int = true;
    if ((csr & CSR_TCINT) && (csr & CSR_TCINTMASK))
      assert_int = true;
    if ((csr & CSR_EXTINT) && (csr & CSR_EXTINTMASK))
      assert_int = true;
  }
  if (assert_int)
    raise_irq(0x10); // PI INTSR bit 4 = EXI
  else
    lower_irq(0x10);
}

// --- channel 0, device 0: memory card slot A ------------------------------
//
// Une carte Nintendo de 251 blocs (16 Mbit, secteurs de 8 Ko), gardee dans un
// fichier de 2 Mo : memcard_a.raw, dans le repertoire inscriptible (a cote de
// l'executable sur PC, le bac a sable sur Android). NJ_NOCARD=1 la debranche
// (developpement seulement).
//
// Le protocole est celui que la bibliotheque CARD du jeu emet, lisible dans le
// SDK embarque (src/lib/CARDBios.c, CARDMount.c) et identique a ce que Dolphin
// emule (EXI_DeviceMemoryCard). Une transaction = une selection du peripherique ;
// premier octet = la commande :
//
//   0x00  identification : deux octets ecrits, puis l'identifiant sur quatre.
//         0x00000010 : 16 Mbit (id & 0xFC), secteurs de 8 Ko (bits 11-13 = 0),
//         latence de lecture 4 octets (bits 8-10 = 0). IsCard() l'accepte.
//   0x85  identifiant du fabricant (deux octets) : 0xC221, pas Samsung (0xEC),
//         donc des pages de 128 octets.
//   0x83  etat : 0x40 deverrouillee (pas de sequence de deverrouillage),
//         0x01 prete, 0x80 occupee, 0x10 / 0x08 erreur d'effacement / d'ecriture.
//   0x89  efface les bits d'erreur.
//   0x81  active (1) ou coupe l'interruption de fin d'operation.
//   0x52  lecture : AD1 AD2 AD3 BA (adresse = AD1<<17 | AD2<<9 | AD3<<7 | BA),
//         4 octets de latence, puis les donnees (512 par DMA), l'adresse
//         tournant dans le secteur de 512.
//   0xF2  ecriture d'une page : meme adresse, puis 128 octets ; ecrits au
//         relachement de la selection.
//   0xF1  effacement d'un secteur (AD1 AD2) ; 0xF4 effacement de toute la carte.
//
// Apres 0xF1 / 0xF2 / 0xF4, la carte est occupee un moment puis leve
// l'interruption EXI du canal 0 si elle est active : __CARDExiHandler relit
// alors l'etat et rend la main a l'appelant (comme Dolphin, apres 5000 tics).
//
// Deverrouillee des le depart, la carte fait verifier par DoMount que
// l'OSSramEx porte une somme de controle coherente pour flashID (flashID nul,
// somme 0xFF) : sram_init la pose.
static constexpr uint32_t kCardId = 0x00000010u;
static constexpr uint16_t kCardVendor = 0xC221;
static constexpr uint32_t kCardBytes = 2u * 1024u * 1024u;   // 16 Mbit
static constexpr uint32_t kSectorBytes = 0x2000;              // 8 Ko

static constexpr uint8_t MC_BUSY = 0x80, MC_UNLOCKED = 0x40, MC_ERASE_ERR = 0x10,
                         MC_PROGRAM_ERR = 0x08, MC_READY = 0x01;

static bool card_enabled() {
  static const bool on = NJ_ENV("NJ_NOCARD") == nullptr;
  return on;
}

static std::vector<uint8_t> card_mem;   // image de la carte, toujours kCardBytes
static std::FILE *card_file = nullptr;
static uint8_t card_status = MC_UNLOCKED | MC_READY;
static bool card_int_enabled = false;
static int card_phase = 0;              // octets vus depuis le debut de la transaction
static uint8_t card_cmd = 0xFF;         // premier octet : la commande
static uint32_t card_addr = 0;
static uint8_t card_prog[128];
static uint64_t card_ops = 0;

// Charge (ou cree) le fichier de la carte au premier acces. Une carte neuve est
// effacee (0xFF partout) : le jeu propose de la formater, et le formatage passe
// par ses propres routines, celles du SDK.
static void card_open() {
  if (!card_mem.empty())
    return;
  card_mem.assign(kCardBytes, 0xFF);
  const std::string path = nj::writable_path("memcard_a.raw");
  // imports en attente (menu F10 > Saves) et copie de securite, avant toute lecture (13/10)
  const std::string note = nj::saves::on_card_open(path);
  if (!note.empty()) nj::report::add(note);
  card_file = std::fopen(path.c_str(), "r+b");
  if (card_file) {
    const size_t got = std::fread(card_mem.data(), 1, kCardBytes, card_file);
    if (got != kCardBytes) {   // fichier tronque : complete a 0xFF sur le disque aussi
      std::fseek(card_file, 0, SEEK_SET);
      std::fwrite(card_mem.data(), 1, kCardBytes, card_file);
      std::fflush(card_file);
    }
    NJ_LOG << "[Card] carte A : " << path << "\n";
  } else if ((card_file = std::fopen(path.c_str(), "w+b")) != nullptr) {
    std::fwrite(card_mem.data(), 1, kCardBytes, card_file);
    std::fflush(card_file);
    NJ_LOG << "[Card] carte A creee (vierge) : " << path << "\n";
  } else {
    NJ_LOG << "[Card] impossible d'ecrire " << path << " : la carte ne gardera rien\n";
  }
}

// Ecrit une plage modifiee de l'image dans le fichier, tout de suite : une
// sauvegarde ne doit pas dependre d'une fermeture propre du jeu.
static void card_persist(uint32_t off, uint32_t len) {
  if (!card_file || off >= kCardBytes)
    return;
  if (len > kCardBytes - off)
    len = kCardBytes - off;
  std::fseek(card_file, (long)off, SEEK_SET);
  std::fwrite(card_mem.data() + off, 1, len, card_file);
  std::fflush(card_file);
}

static void exi_update_interrupt();

// Fin d'une ecriture ou d'un effacement : prete, et l'interruption EXI du
// canal 0 si CARD l'a demandee (0x81 01).
static void card_done_later() {
  card_status = (uint8_t)((card_status | MC_BUSY) & ~MC_READY);
  Timers::get().after(5000, [](GuestCpu &, uint64_t) {
    card_status = (uint8_t)((card_status | MC_READY) & ~MC_BUSY);
    if (card_int_enabled) {
      exi_ch[0].csr |= CSR_EXIINT;
      exi_update_interrupt();
    }
  });
}

static void card_reset_transaction() {
  card_phase = 0;
  card_cmd = 0xFF;
}

// Fin de transaction (la selection retombe) : c'est la que la carte ecrit ou
// efface, comme la puce flash.
static void card_deselect() {
  if (card_phase == 0)
    return;
  // Journal des transactions (developpement) : les 400 premieres, puis une sur 200.
  {
    static uint64_t n = 0;
    ++n;
    if (n <= 400 || n % 200 == 0)
      NJ_LOG << "[Card] #" << n << " cmd=0x" << std::hex << (unsigned)card_cmd << " addr=0x" << card_addr
                << std::dec << " octets=" << card_phase << " etat=0x" << std::hex << (unsigned)card_status
                << std::dec << '\n';
  }
  switch (card_cmd) {
  case 0xF2:   // ecriture de page
    if (card_phase > 5) {
      const int n = card_phase - 5;
      uint32_t a = card_addr & (kCardBytes - 1);
      for (int i = 0; i < n; ++i) {
        card_mem[a] = card_prog[i & 127];
        a = (a & ~0x1FFu) | ((a + 1) & 0x1FFu);
      }
      card_persist(card_addr & (kCardBytes - 1) & ~0x1FFu, 0x200);
      ++card_ops;
      card_done_later();
    }
    break;
  case 0xF1:   // effacement d'un secteur
    if (card_phase > 2) {
      const uint32_t s = card_addr & (kCardBytes - 1) & ~(kSectorBytes - 1);
      std::memset(card_mem.data() + s, 0xFF, kSectorBytes);
      card_persist(s, kSectorBytes);
      ++card_ops;
      card_done_later();
    }
    break;
  case 0xF4:   // effacement de la carte
    if (card_phase > 2) {
      std::memset(card_mem.data(), 0xFF, kCardBytes);
      card_persist(0, kCardBytes);
      ++card_ops;
      card_done_later();
    }
    break;
  }
  card_reset_transaction();
}

static uint8_t card_transfer_byte(uint8_t out) {
  card_open();
  const int n = card_phase++;
  if (n == 0) {
    card_cmd = out;
    if (card_cmd == 0x89)   // efface les erreurs
      card_status = (uint8_t)((card_status & ~(MC_ERASE_ERR | MC_PROGRAM_ERR)) | MC_READY);
    return 0xFF;
  }
  switch (card_cmd) {
  case 0x00:   // identification : octets 2 a 5
    return (n >= 2 && n < 6) ? (uint8_t)(kCardId >> (8 * (5 - n))) : 0;
  case 0x85:   // fabricant : octet 2 poids fort, octet 3 poids faible
    return (uint8_t)((n & 1) ? kCardVendor : kCardVendor >> 8);
  case 0x83:   // etat
    return card_status;
  case 0x81:   // interruption de fin d'operation
    if (n == 1)
      card_int_enabled = (out & 1) != 0;
    return 0xFF;
  case 0x52:   // lecture
    switch (n) {
    case 1: card_addr = (uint32_t)(out & 0x7F) << 17; return 0xFF;
    case 2: card_addr |= (uint32_t)out << 9; return 0xFF;
    case 3: card_addr |= (uint32_t)(out & 3) << 7; return 0xFF;
    case 4: card_addr |= out & 0x7Fu; return 0xFF;
    }
    if (n < 9)   // latence : 4 octets
      return 0xFF;
    {
      const uint32_t a = card_addr & (kCardBytes - 1);
      const uint32_t i = (a & ~0x1FFu) | ((a + (uint32_t)(n - 9)) & 0x1FFu);
      return card_mem[i];
    }
  case 0xF2:   // ecriture de page
    switch (n) {
    case 1: card_addr = (uint32_t)(out & 0x7F) << 17; break;
    case 2: card_addr |= (uint32_t)out << 9; break;
    case 3: card_addr |= (uint32_t)(out & 3) << 7; break;
    case 4: card_addr |= out & 0x7Fu; break;
    default: card_prog[(n - 5) & 127] = out; break;
    }
    return 0xFF;
  case 0xF1:   // effacement d'un secteur
    if (n == 1)
      card_addr = (uint32_t)(out & 0x7F) << 17;
    else if (n == 2)
      card_addr |= (uint32_t)out << 9;
    return 0xFF;
  default:     // 0x87 reveil, 0x88 sommeil, 0xF4 : rien a rendre
    return 0xFF;
  }
}

static void exi_run_transfer(int chan) {
  ExiChannel &c = exi_ch[chan];
  const uint32_t cs = (c.csr & CSR_CS_MASK) >> CSR_CS_SHIFT;
  const bool is_ipl = (chan == 0 && cs == 0x2); // CS1 -> device 1
  const bool is_card = (chan == 0 && cs == 0x1 && card_enabled()); // CS0 -> device 0
  const uint32_t rw = (c.cr >> CR_RW_SHIFT) & 3;
  const bool cpu_writes = (rw != 0); // EXI_WRITE or EXI_READWRITE
  const bool cpu_reads = (rw != 1);

  if (c.cr & CR_DMA) {
    const uint32_t len = c.length;
    // MAR holds a physical address; address guest RAM through its cached
    // mapping like the DI block does.
    const uint32_t addr = 0x80000000u | (c.mar & 0x01FFFFFFu);
    if (nj::runtime::g_guest_mem && len > 0 && len <= 0x100000) {
      for (uint32_t i = 0; i < len; ++i) {
        const uint8_t out =
            cpu_writes ? nj::runtime::g_guest_mem->read8(addr + i) : 0xFF;
        const uint8_t in = is_ipl    ? ipl_transfer_byte(out)
                           : is_card ? card_transfer_byte(out)
                                     : 0;
        if (cpu_reads)
          nj::runtime::g_guest_mem->write8(addr + i, in);
      }
    }
    c.length = 0;
  } else {
    const uint32_t len = ((c.cr >> CR_TLEN_SHIFT) & 3) + 1;
    uint32_t result = 0;
    for (uint32_t i = 0; i < len; ++i) {
      const uint8_t out =
          cpu_writes ? (uint8_t)(c.data >> (24 - 8 * i)) : 0xFF;
      const uint8_t in = is_ipl    ? ipl_transfer_byte(out)
                         : is_card ? card_transfer_byte(out)
                                   : 0;
      result |= (uint32_t)in << (24 - 8 * i);
    }
    // Immediate reads come back MSB-justified; EXISync() shifts them out of
    // DATA from bit 31 down.
    c.data = result;
  }

  c.cr &= ~CR_TSTART;
  c.csr |= CSR_TCINT;
  exi_update_interrupt();
}

static void exi_write_csr(int chan, uint32_t val) {
  ExiChannel &c = exi_ch[chan];

  // Any change of chip select is a transaction boundary - EXISelect() raises
  // one of CS0..CS2, EXIDeselect() drops them all.
  if (chan == 0 && (val & CSR_CS_MASK) != (c.csr & CSR_CS_MASK)) {
    ipl_reset_transaction();
    card_deselect();   // la carte ecrit / efface quand la selection retombe
  }

  // The three interrupt status bits are write-one-to-clear; EXT is read-only
  // and stays low because nothing is plugged in.
  const uint32_t acked = val & CSR_INT_BITS;
  const uint32_t kept = c.csr & CSR_INT_BITS & ~acked;
  // EXT est en lecture seule : il signale la carte de l'emplacement A, ce que
  // __EXIProbe lit avant d'autoriser CARD a parler au slot.
  const uint32_t ext = (chan == 0 && card_enabled()) ? CSR_EXT : 0u;
  c.csr = (val & ~(CSR_INT_BITS | CSR_EXT)) | kept | ext;
  exi_update_interrupt();
}

// La cause EXI a servir. Le PI ne dit que « EXI » (bit 4) ; le repartiteur de l'OS lit ensuite
// les CSR pour choisir entre EXI (le peripherique), TC (transfert fini) et EXT (branchement).
// Le runtime envoyait toujours l'interruption 9 (EXI du canal 0) : la fin d'une lecture DMA de
// la carte memoire n'atteignait jamais TCIntrruptHandler, et __CARDExiHandler tournait en rond.
// Le gestionnaire servi ecrit ensuite le CSR pour acquitter sa cause, ce qui relance
// exi_update_interrupt : une autre cause encore levee reposera le bit du PI.
int exi_os_irq() {
  for (int chan = 0; chan < 3; ++chan) {
    const uint32_t csr = exi_ch[chan].csr;
    if ((csr & CSR_TCINT) && (csr & CSR_TCINTMASK))
      return 10 + 3 * chan;
    if ((csr & CSR_EXIINT) && (csr & CSR_EXIINTMASK))
      return 9 + 3 * chan;
    if (chan < 2 && (csr & CSR_EXTINT) && (csr & CSR_EXTINTMASK))
      return 11 + 3 * chan;
  }
  return 9;
}

void attach_exi(IoBus &dispatcher) {
  dispatcher.map(
      EXI_BASE, EXI_BASE + 0xFF,
      [](uint32_t addr) -> uint32_t {
        const uint32_t off = addr - EXI_BASE;
        if (off >= 3 * 0x14)
          return 0;
        const int rchan = (int)(off / 0x14);
        const ExiChannel &c = exi_ch[rchan];
        switch ((off % 0x14) / 4) {
        case 0:
          // EXT est en lecture seule et doit etre juste des la premiere
          // lecture : __EXIProbe lit CSR sans l'avoir ecrit, donc le poser
          // uniquement dans le chemin d'ecriture le laissait a zero et le
          // peripherique restait invisible.
          return c.csr | ((rchan == 0 && card_enabled()) ? CSR_EXT : 0u);
        case 1: return c.mar;
        case 2: return c.length;
        case 3: return c.cr;
        case 4: return c.data;
        }
        return 0;
      },
      [](uint32_t addr, uint32_t val) {
        const uint32_t off = addr - EXI_BASE;
        if (off >= 3 * 0x14)
          return;
        const int chan = (int)(off / 0x14);
        ExiChannel &c = exi_ch[chan];
        switch ((off % 0x14) / 4) {
        case 0: exi_write_csr(chan, val); break;
        case 1: c.mar = val; break;
        case 2: c.length = val; break;
        case 3:
          c.cr = val;
          if (val & CR_TSTART)
            exi_run_transfer(chan);
          break;
        case 4: c.data = val; break;
        }
      });
}

} // namespace nj::runtime::hw
