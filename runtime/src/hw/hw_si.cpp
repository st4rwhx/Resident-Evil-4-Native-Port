#include "runtime/hw/hw.h"
#include "input/input_manager.h"
#include "runtime/config.h"
#include "runtime/cpu_context.h"

#include <cstring>
#include <iostream>

namespace nj::runtime::hw {

// SI - Serial Interface, 0xCC006400..0xCC0064FF. The four controller ports.
//
//   0x6400 + chan*0x0C   OUTBUF / INBUFH / INBUFL   per-channel poll buffers
//   0x6430               POLL      poll enable, rate (X) and line (Y)
//   0x6434               COMCSR    TSTART, channel, lengths, interrupts
//   0x6438               SR        per-channel status, one byte each,
//                                  channel 0 in the high byte
//   0x643C               EXILK     EXI lock
//   0x6480..0x64FF       128-byte command/response buffer
//
// Bit positions follow YAGCD converted to LSB-first numbering, cross-checked
// against the SDK: SIInit() writes 0x80000000 to COMCSR to acknowledge TCINT
// (bit 31) and spins on `__SIRegs[13] & 1` for TSTART (bit 0).
//
// Pad input does not come through here -- PADRead is served at the HLE seam
// straight from InputHub -- so this block exists for the boot path:
// SIInit() and SISetSamplingRate() program these registers and wait on
// TSTART, and SIGetType() runs a real transfer to identify each port. What it
// needs is registers that latch, transfers that complete, and a believable
// answer to the device-ID command. Port 1 answers as a standard GameCube
// controller so anything that gates on controller presence agrees with the
// input the HLE seam delivers; ports 2-4 report no response.

static constexpr uint32_t SI_BASE = 0xCC006400;
static constexpr uint32_t SI_POLL = 0xCC006430;
static constexpr uint32_t SI_COMCSR = 0xCC006434;
static constexpr uint32_t SI_SR = 0xCC006438;
static constexpr uint32_t SI_EXILK = 0xCC00643C;
static constexpr uint32_t SI_IOBUF = 0xCC006480;

static constexpr uint32_t CSR_TSTART = 1u << 0;
static constexpr uint32_t CSR_CHANNEL_SHIFT = 1; // bits 1..2
static constexpr uint32_t CSR_INLNGTH_SHIFT = 8;  // bits 8..14
static constexpr uint32_t CSR_OUTLNGTH_SHIFT = 16; // bits 16..22
static constexpr uint32_t CSR_RDSTINTMSK = 1u << 27;
static constexpr uint32_t CSR_RDSTINT = 1u << 28;
static constexpr uint32_t CSR_COMERR = 1u << 29;
static constexpr uint32_t CSR_TCINTMSK = 1u << 30;
static constexpr uint32_t CSR_TCINT = 1u << 31;

// Per-channel status byte, within its byte of SR.
static constexpr uint32_t ST_NOREP = 1u << 3;
static constexpr uint32_t ST_WRST = 1u << 4;

// SI device type words, as SIGetType() reports them.
static constexpr uint32_t SI_TYPE_GC_CONTROLLER = 0x09000000;

static uint32_t si_chan_buf[4][3]; // OUTBUF, INBUFH, INBUFL
static uint32_t si_poll = 0;
static uint32_t si_comcsr = 0;
static uint32_t si_sr = 0;
static uint32_t si_exilk = 0;
static uint8_t si_iobuf[128];

static bool chan_connected(int chan) { return chan == 0; }

// Channel 0 occupies SR bits 24..31, channel 3 bits 0..7.
static inline uint32_t sr_shift(int chan) { return (uint32_t)(24 - 8 * chan); }

static void si_update_interrupt() {
  const bool assert_int =
      ((si_comcsr & CSR_TCINT) && (si_comcsr & CSR_TCINTMSK)) ||
      ((si_comcsr & CSR_RDSTINT) && (si_comcsr & CSR_RDSTINTMSK));
  if (assert_int)
    raise_irq(0x08); // PI INTSR bit 3 = SI
  else
    lower_irq(0x08);
}

// Fill si_iobuf with the reply to the command already sitting in it.
// Returns false when the port should report "no response".
static bool si_device_reply(int chan, uint32_t in_len) {
  if (!chan_connected(chan))
    return false;

  const uint8_t cmd = si_iobuf[0];
  std::memset(si_iobuf, 0, sizeof(si_iobuf));

  switch (cmd) {
  case 0x00:   // info: device type and status, 3 bytes
  case 0xFF: { // reset, same reply
    si_iobuf[0] = (uint8_t)(SI_TYPE_GC_CONTROLLER >> 24);
    si_iobuf[1] = (uint8_t)(SI_TYPE_GC_CONTROLLER >> 16);
    si_iobuf[2] = (uint8_t)(SI_TYPE_GC_CONTROLLER >> 8);
    return true;
  }
  case 0x41:   // get origin, 10 bytes
  case 0x42: { // calibrate, same layout
    si_iobuf[0] = 0x00; // buttons
    si_iobuf[1] = 0x00;
    si_iobuf[2] = 128; // main stick X, centred
    si_iobuf[3] = 128; // main stick Y
    si_iobuf[4] = 128; // C stick X
    si_iobuf[5] = 128; // C stick Y
    si_iobuf[6] = 0;   // L analog
    si_iobuf[7] = 0;   // R analog
    si_iobuf[8] = 0;
    si_iobuf[9] = 0;
    return true;
  }
  case 0x40: { // poll, 8 bytes : l'etat reel de la manette.
    // Cette reponse etait **neutre**, au motif que la vraie entree arrivait
    // par PADRead. C'est faux : le SDK lit aussi les tampons de scrutation
    // SI, et il y trouvait des zeros. Le jeu recevait donc alternativement
    // l'etat correct et une manette relachee.
    //
    // Symptome, et il a coute une soiree : Leon levait puis baissait son arme
    // a quelques hertz. Mesure decisive - le bit de visee force sans aucune
    // condition, sans souris ni liaison, oscillait quand meme : 2 images sur
    // 12 arme levee. Et Key.on du jeu retombait a zero une lecture sur deux.
    // Le defaut n'etait donc dans aucune source d'entree, mais dans la
    // livraison.
    const auto st = nj::runtime::input::InputHub::get().pad(0);
    // Les bits de bouton de la reponse SI suivent la meme disposition que
    // PADStatus::button sur les 13 bits utiles ; au-dela ce sont des
    // drapeaux d'etat (origine, erreur) qu'on laisse a zero.
    const uint16_t btn = (uint16_t)(st.buttons & 0x1FFF);
    si_iobuf[0] = (uint8_t)(btn >> 8);
    si_iobuf[1] = (uint8_t)(btn & 0xFF);
    // Les sticks SI sont non signes, centres sur 128 ; les notres sont
    // signes autour de zero.
    si_iobuf[2] = (uint8_t)(128 + st.stick_x);
    si_iobuf[3] = (uint8_t)(128 + st.stick_y);
    si_iobuf[4] = (uint8_t)(128 + st.substick_x);
    si_iobuf[5] = (uint8_t)(128 + st.substick_y);
    si_iobuf[6] = st.trigger_l;
    si_iobuf[7] = st.trigger_r;
    return true;
  }
  default:
    // Unknown command: answer with zeroes rather than a bus error, so the
    // caller sees a well-formed (if empty) reply.
    (void)in_len;
    return true;
  }
}

static void si_run_transfer() {
  const int chan = (int)((si_comcsr >> CSR_CHANNEL_SHIFT) & 3);
  uint32_t in_len = (si_comcsr >> CSR_INLNGTH_SHIFT) & 0x7F;
  if (in_len == 0)
    in_len = 128; // the SDK encodes a full 128-byte read as zero

  const uint32_t shift = sr_shift(chan);
  si_sr &= ~(0xFFu << shift);

  if (si_device_reply(chan, in_len)) {
    si_sr |= (uint32_t)ST_WRST << shift;
    si_comcsr &= ~CSR_COMERR;
  } else {
    std::memset(si_iobuf, 0, sizeof(si_iobuf));
    si_sr |= (uint32_t)ST_NOREP << shift;
    si_comcsr |= CSR_COMERR;
  }

  // Synchronous completion, same reasoning as EXI: SIInit()'s
  // `while (__SIRegs[13] & 1)` must see TSTART already clear.
  si_comcsr &= ~CSR_TSTART;
  si_comcsr |= CSR_TCINT;
  si_update_interrupt();
}

void attach_si(IoBus &dispatcher) {
  dispatcher.map(
      SI_BASE, SI_BASE + 0xFF,
      [](uint32_t addr) -> uint32_t {
        if (addr >= SI_IOBUF && addr < SI_IOBUF + sizeof(si_iobuf)) {
          const uint32_t i = addr - SI_IOBUF;
          return ((uint32_t)si_iobuf[i] << 24) |
                 ((uint32_t)si_iobuf[i + 1] << 16) |
                 ((uint32_t)si_iobuf[i + 2] << 8) | (uint32_t)si_iobuf[i + 3];
        }
        switch (addr) {
        case SI_POLL: return si_poll;
        case SI_COMCSR: return si_comcsr;
        case SI_SR: return si_sr;
        case SI_EXILK: return si_exilk;
        default: break;
        }
        const uint32_t off = addr - SI_BASE;
        if (off < 4 * 0x0C)
          return si_chan_buf[off / 0x0C][(off % 0x0C) / 4];
        return 0;
      },
      [](uint32_t addr, uint32_t val) {
        if (addr >= SI_IOBUF && addr < SI_IOBUF + sizeof(si_iobuf)) {
          const uint32_t i = addr - SI_IOBUF;
          si_iobuf[i] = (uint8_t)(val >> 24);
          si_iobuf[i + 1] = (uint8_t)(val >> 16);
          si_iobuf[i + 2] = (uint8_t)(val >> 8);
          si_iobuf[i + 3] = (uint8_t)val;
          return;
        }
        switch (addr) {
        case SI_POLL:
          si_poll = val;
          return;
        case SI_COMCSR: {
          // TCINT and RDSTINT are write-one-to-clear; COMERR is read-only.
          const uint32_t acked = val & (CSR_TCINT | CSR_RDSTINT);
          const uint32_t kept =
              si_comcsr & (CSR_TCINT | CSR_RDSTINT) & ~acked;
          si_comcsr =
              (val & ~(CSR_TCINT | CSR_RDSTINT | CSR_COMERR)) | kept |
              (si_comcsr & CSR_COMERR);
          if (val & CSR_TSTART)
            si_run_transfer();
          else
            si_update_interrupt();
          return;
        }
        case SI_SR:
          // The SDK clears a channel's stale status by reading SR, masking
          // out that channel's byte and writing the whole word back, so a
          // plain latch is the right model.
          si_sr = val;
          return;
        case SI_EXILK:
          si_exilk = val;
          return;
        default:
          break;
        }
        const uint32_t off = addr - SI_BASE;
        if (off < 4 * 0x0C)
          si_chan_buf[off / 0x0C][(off % 0x0C) / 4] = val;
      });
}

} // namespace nj::runtime::hw
