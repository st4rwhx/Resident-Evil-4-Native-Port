#pragma once
// Minuteries de la machine invitee, sur la base de temps (tics du Gekko) : fin de lecture du
// disque, courrier du DSP, DMA audio, VBlank... Une poignee seulement a la fois : un petit tableau
// parcouru en entier coute moins qu'un tas, et l'echeance la plus proche reste en cache (lue a
// chaque point de controle du code recompile).
#include <cstdint>
#include <functional>

namespace nj::runtime {

struct GuestCpu;

class Timers {
 public:
  using Id = uint64_t;
  // late : retard du declenchement sur l'echeance, en tics.
  using Fn = std::function<void(GuestCpu &, uint64_t late)>;

  static Timers &get();

  Id after(uint64_t ticks, Fn fn);    // une fois, dans `ticks` tics
  Id every(uint64_t ticks, Fn fn);    // periodique ; la suivante est armee avant l'appel
  bool cancel(Id id);
  // L'horloge avance a `now` : declenche ce qui est echu, dans l'ordre des echeances.
  void run_due(GuestCpu &ctx, uint64_t now);
  void clear();

  uint64_t now() const { return clock_; }
  uint64_t next_due() const { return soonest_; }   // ~0 : rien d'arme

 private:
  struct Slot {
    Id id = 0;          // 0 : libre
    uint64_t due = 0;
    uint64_t period = 0;
    Fn fn;
  };
  static constexpr int kSlots = 32;
  Slot slots_[kSlots];
  uint64_t clock_ = 0;
  uint64_t soonest_ = ~0ull;
  Id last_id_ = 0;
  bool busy_ = false;

  Id arm(uint64_t due, uint64_t period, Fn fn);
  void refresh_soonest();
};

}  // namespace nj::runtime
