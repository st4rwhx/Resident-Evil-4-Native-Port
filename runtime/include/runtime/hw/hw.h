#pragma once
#include <string>
// Peripheriques de la GameCube vus par le reste du runtime : etat des interruptions du PI,
// signaux echanges entre blocs (VI, DSP, AI, DI, EXI, PE, CP) et inscription sur le bus.
// Chaque bloc vit dans src/hw/hw_<bloc>.cpp.
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "runtime/hw/io_bus.h"

namespace nj::runtime {
struct MMU;
uint64_t guest_time();   // base de temps de l'invite, en tics
}  // namespace nj::runtime

namespace nj::runtime::hw {

// PI : causes d'interruption levees (INTSR) et autorisees (INTMR).
extern std::atomic<uint32_t> pi_cause;
extern std::atomic<uint32_t> pi_enable;
// Le fil du jeu doit regarder les interruptions tout de suite, sans attendre son echeance.
extern std::atomic<bool> poll_now;

inline void raise_irq(uint32_t cause) {
  pi_cause.fetch_or(cause, std::memory_order_relaxed);
  poll_now.store(true, std::memory_order_relaxed);
}
inline void lower_irq(uint32_t cause) { pi_cause.fetch_and(~cause, std::memory_order_relaxed); }

// Etat du PE (jetons, fin de dessin) et delai d'interruption du lecteur.
extern std::atomic<uint32_t> pe_status;
extern int di_irq_delay;

// Signaux entre blocs
void vi_raise();
void ai_step();
void dsp_raise();
void di_step();
std::string di_debug_dump();   // etat du lecteur pour le detecteur de gel (perf_journal.cpp)
void pe_token(uint32_t token, bool irq);
void pe_finish();
bool pe_tracing();
// Interruption OS a servir pour le DSP / l'EXI (numero du vecteur du SDK).
int dsp_os_irq();
int exi_os_irq();   // 9 + 3 x canal (+1 transfert fini, +2 peripherique branche) ; 9 si aucune
// Son mixe pour l'hote (stereo 16 bits), en images.
size_t audio_pull(int16_t *out, size_t frames);
// Images de son en attente, et retrait des plus anciennes au-dela de keep (rattrapage du retard).
size_t audio_buffered();
void audio_trim(size_t keep_frames);
// File FIFO du CPU (PI) et base de la FIFO du CP.
void cpu_fifo_get(uint32_t &base, uint32_t &end, uint32_t &wptr);
void cpu_fifo_set_wptr(uint32_t wptr);
uint32_t cp_fifo_base();
// Diagnostic : liste des fils de l'invite (pose par main.cpp).
extern void (*dump_threads_hook)(nj::runtime::MMU &);

// Inscription des blocs sur le bus
void attach_cp(IoBus &bus);
void attach_pi(IoBus &bus);
void attach_pe(IoBus &bus);
void attach_vi(IoBus &bus);
void attach_dsp(IoBus &bus);
void attach_ai(IoBus &bus);
void attach_di(IoBus &bus);
void attach_si(IoBus &bus);
void attach_exi(IoBus &bus);
void attach_hardware(IoBus &bus);

}  // namespace nj::runtime::hw
