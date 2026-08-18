#include "input/input_manager.h"

#include <chrono>
#include <cstdlib>

#include "runtime/env.h"

namespace nj::runtime::input {

InputHub &InputHub::get() {
  static InputHub m;
  return m;
}

void InputHub::plug_source(std::unique_ptr<PadSource> source) {
  if (source) sources_.push_back(std::move(source));
}

void InputHub::update() {
  PadState draft[4];
  for (auto &s : sources_) s->poll(draft);
  // NJ_FORCEAIM=<s> : gachette R tenue sans condition (apres s secondes, 1 = tout de suite),
  // pour separer un defaut de la visee de la lecture des entrees.
  static const char *fa = NJ_ENV("NJ_FORCEAIM");
  if (fa) {
    static const int after = std::atoi(fa);
    static const auto t0 = std::chrono::steady_clock::now();
    if (after <= 1 || std::chrono::steady_clock::now() - t0 >= std::chrono::seconds(after)) {
      draft[0].buttons |= 0x0020;
      draft[0].trigger_r = 255;
      draft[0].err = 0;
    }
  }
  std::lock_guard<std::mutex> g(lock_);
  for (int i = 0; i < 4; ++i) published_[i] = draft[i];
}

PadState InputHub::pad(int port) {
  std::lock_guard<std::mutex> g(lock_);
  return (port >= 0 && port < 4) ? published_[port] : PadState{};
}

}  // namespace nj::runtime::input
