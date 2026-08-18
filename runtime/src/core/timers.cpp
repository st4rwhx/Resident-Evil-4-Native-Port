#include "runtime/timers.h"
#include "runtime/log.h"

#include <iostream>
#include <utility>

namespace nj::runtime {

Timers &Timers::get() {
  static Timers t;
  return t;
}

void Timers::refresh_soonest() {
  uint64_t s = ~0ull;
  for (const Slot &e : slots_)
    if (e.id && e.due < s) s = e.due;
  soonest_ = s;
}

Timers::Id Timers::arm(uint64_t due, uint64_t period, Fn fn) {
  for (Slot &e : slots_) {
    if (e.id) continue;
    e.id = ++last_id_;
    e.due = due;
    e.period = period;
    e.fn = std::move(fn);
    if (due < soonest_) soonest_ = due;
    return e.id;
  }
  NJ_ERR << "[Timers] plus de place (" << kSlots << ")" << std::endl;
  return 0;
}

Timers::Id Timers::after(uint64_t ticks, Fn fn) { return arm(clock_ + ticks, 0, std::move(fn)); }

Timers::Id Timers::every(uint64_t ticks, Fn fn) {
  const uint64_t p = ticks ? ticks : 1;
  return arm(clock_ + p, p, std::move(fn));
}

bool Timers::cancel(Id id) {
  if (!id) return false;
  for (Slot &e : slots_) {
    if (e.id != id) continue;
    e.id = 0;
    e.fn = nullptr;
    refresh_soonest();
    return true;
  }
  return false;
}

void Timers::run_due(GuestCpu &ctx, uint64_t now) {
  if (now < clock_) return;   // l'horloge ne recule pas
  clock_ = now;
  if (busy_ || soonest_ > clock_) return;   // pas de reentree depuis un rappel
  busy_ = true;
  for (;;) {
    // la plus ancienne echeance passee ; a echeance egale, la premiere armee
    Slot *next = nullptr;
    for (Slot &e : slots_)
      if (e.id && e.due <= clock_ && (!next || e.due < next->due || (e.due == next->due && e.id < next->id)))
        next = &e;
    if (!next) break;
    const uint64_t late = clock_ - next->due;
    if (next->period) {
      // periodique : reste arme, prochaine echeance d'abord (un rappel peut l'annuler)
      Fn fn = next->fn;
      uint64_t due = next->due + next->period;
      if (due <= clock_) due = clock_ + next->period;
      next->due = due;
      refresh_soonest();
      fn(ctx, late);
    } else {
      Fn fn = std::move(next->fn);
      next->id = 0;
      next->fn = nullptr;
      refresh_soonest();
      fn(ctx, late);
    }
  }
  busy_ = false;
}

void Timers::clear() {
  for (Slot &e : slots_) {
    e.id = 0;
    e.fn = nullptr;
  }
  clock_ = 0;
  soonest_ = ~0ull;
  last_id_ = 0;
  busy_ = false;
}

}  // namespace nj::runtime
