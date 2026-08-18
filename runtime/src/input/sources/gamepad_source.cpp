// Manettes de l'hote -> manettes GameCube, ports 0 a 3.
//
// Deux dispositions :
//  - GameCube (F10 « Controller layout » 0) : A, B, X, Y, Start et la croix a leur place ; LB = L
//    numerique, RB = R numerique, gachette droite enfoncee a moitie = Z ; gachettes analogiques ;
//  - UHD Type I/II/III (1 a 3) : un bit par bouton physique, la table Key_type_tbl du jeu (reecrite
//    par hle/pad.cpp) decide de ce que fait chacun. LB, RB et Back prennent les bits 0x0080, 0x2000
//    et 0x4000, libres sur la manette GameCube ; le menu titre lit la manette brute et y retrouve
//    A, B et Start.
#include "gamepad_source.h"
#include "runtime/log.h"

#include <SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "runtime/config.h"
#include "runtime/env.h"
#include "runtime/gfx_settings.h"
#include "input/re4_context.h"
#include <algorithm>

namespace nj::runtime::input {

namespace {
constexpr int kDead = 8000;   // zone morte des sticks (sur 32767)

struct Bit {
  SDL_GameControllerButton button;
  uint16_t bit;
};
// Communs aux dispositions UHD et GameCube classique : faces, Start, croix.
constexpr Bit kFaces[] = {
    {SDL_CONTROLLER_BUTTON_A, 0x0100},          {SDL_CONTROLLER_BUTTON_B, 0x0200},
    {SDL_CONTROLLER_BUTTON_X, 0x0400},          {SDL_CONTROLLER_BUTTON_Y, 0x0800},
    {SDL_CONTROLLER_BUTTON_START, 0x1000},      {SDL_CONTROLLER_BUTTON_DPAD_UP, 0x0008},
    {SDL_CONTROLLER_BUTTON_DPAD_DOWN, 0x0004},  {SDL_CONTROLLER_BUTTON_DPAD_LEFT, 0x0001},
    {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 0x0002},
};
// Disposition GameCube faithful (PS / Xbox) : Croix=A, Carre=B, Rond=X, Triangle=Y.
constexpr Bit kFaithfulFaces[] = {
    {SDL_CONTROLLER_BUTTON_A, 0x0100},          // Croix (PS) / A (Xbox)      -> A
    {SDL_CONTROLLER_BUTTON_X, 0x0200},          // Carre (PS) / X (Xbox)      -> B
    {SDL_CONTROLLER_BUTTON_B, 0x0400},          // Rond (PS) / B (Xbox)       -> X
    {SDL_CONTROLLER_BUTTON_Y, 0x0800},          // Triangle (PS) / Y (Xbox)   -> Y
    {SDL_CONTROLLER_BUTTON_START, 0x1000},      // OPTIONS (PS) / Menu (Xbox) -> Start
    {SDL_CONTROLLER_BUTTON_DPAD_UP, 0x0008},
    {SDL_CONTROLLER_BUTTON_DPAD_DOWN, 0x0004},
    {SDL_CONTROLLER_BUTTON_DPAD_LEFT, 0x0001},
    {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 0x0002},
};
// UHD : boutons de plus, sur des bits libres.
constexpr Bit kUhdExtra[] = {
    {SDL_CONTROLLER_BUTTON_BACK, 0x4000},
    {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 0x0080},
    {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 0x2000},
};

#include "../../../third_party/gamecontrollerdb/gamecontrollerdb.inc"

// Manettes ouvertes, dans l'ordre d'arrivee : port 0 = la premiere (14/10). Avant, seuls les
// peripheriques d'index 0 a 3 etaient lus : un telephone en declare d'autres (capteurs, telecommande)
// qui prenaient ces places, et une manette Bluetooth arrivait au-dela.
SDL_GameController *g_open[4] = {};

void refresh_controllers() {
  static bool db = false;
  if (!db) {
    db = true;
    // base SDL_GameControllerDB : manettes Bluetooth / DirectInput inconnues de SDL
    const int n = SDL_GameControllerAddMappingsFromRW(SDL_RWFromConstMem(kGameControllerDb, sizeof kGameControllerDb - 1), 1);
    NJ_PRINTF("[Manette] base : %d correspondances\n", n);
  }
  // debranchees : place liberee, les suivantes remontent
  int k = 0;
  for (int i = 0; i < 4; ++i) {
    if (g_open[i] && !SDL_GameControllerGetAttached(g_open[i])) {
      NJ_PRINTF("[Manette] debranchee : %s\n", SDL_GameControllerName(g_open[i]) ? SDL_GameControllerName(g_open[i]) : "?");
      SDL_GameControllerClose(g_open[i]);
      g_open[i] = nullptr;
    }
    if (g_open[i]) g_open[k++] = g_open[i];
  }
  for (int i = k; i < 4; ++i) g_open[i] = nullptr;
  // nouvelles : tous les peripheriques, pas seulement les 4 premiers
  static int last_count = -1;
  const int count = SDL_NumJoysticks();
  if (count == last_count && k == 4) return;
  last_count = count;
  for (int j = 0; j < count && k < 4; ++j) {
    if (!SDL_IsGameController(j)) continue;
    const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(j);
    bool have = false;
    for (int i = 0; i < k; ++i)
      have |= SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_open[i])) == id;
    if (have) continue;
    if (SDL_GameController *gc = SDL_GameControllerOpen(j)) {
      g_open[k] = gc;
      const char *name = SDL_GameControllerName(gc);
      // une manette oubliee branchee s'ajoute au clavier : le dire (son L empeche de viser)
      NJ_PRINTF("[Manette] port %d : %s (NJ_NOGAMEPAD=1 pour l'ignorer)\n", k, name ? name : "?");
      std::fflush(stdout);
      ++k;
    }
  }
}

// ---- vibrations (14/10) ----
// Le jeu allume / eteint le moteur image par image (pad.cpp : Vib_level, PADControlMotor) : la part
// d'images « marche » donne l'intensite. On la lisse et on la renvoie toutes les 3 images, pour
// 100 ms, a la manette (port 0) et, sur telephone, au vibreur (SDL_Haptic).
SDL_Haptic *g_phone = nullptr;

// Vibreur du telephone, ouvert a la premiere demande (Android : service Vibrator, permission VIBRATE).
SDL_Haptic *phone() {
#ifdef NJ_MOBILE
  static bool tried = false;
  if (!tried) {
    tried = true;
    if (SDL_NumHaptics() > 0 && (g_phone = SDL_HapticOpen(0)) && SDL_HapticRumbleInit(g_phone) != 0) {
      SDL_HapticClose(g_phone);
      g_phone = nullptr;
    }
  }
#endif
  return g_phone;
}

void haptics_frame(SDL_GameController *gc) {
  static float duty = 0.0f;
  static int frame = 0;
  static bool on = false;
  const int lvl = gfx::g_haptics;
  const int st = lvl ? re4_vib_state() : 0;
  duty = st == 1 ? duty * 0.75f + 0.25f : st == 2 ? 0.0f : duty * 0.75f;
  const float amp = lvl == 0 ? 0.0f : std::min(1.0f, duty * (lvl == 2 ? 1.6f : 0.8f));
  if (++frame % 3 && !(on && amp < 0.05f)) return;
  if (amp < 0.05f) {
    if (on) {
      if (gc) SDL_GameControllerRumble(gc, 0, 0, 0);
      if (g_phone) SDL_HapticRumbleStop(g_phone);
      on = false;
    }
    return;
  }
  on = true;
  const Uint16 a = (Uint16)(amp * 65535.0f);
  if (gc) SDL_GameControllerRumble(gc, a, a, 120);
  if (SDL_Haptic *h = phone()) SDL_HapticRumblePlay(h, amp, 120);
}

int8_t stick(int raw, bool invert) {
  if (std::abs(raw) <= kDead) return 0;
  const float v = raw / 32767.0f * 127.0f;
  return (int8_t)(invert ? -v : v);
}
}  // namespace

void JoypadInput::poll(PadState pads[4]) {
  // NJ_NOGAMEPAD=1 : manettes de l'hote ignorees (essais automatiques)
  static const bool off = NJ_ENV("NJ_NOGAMEPAD") != nullptr;
  if (off) return;
  const int mode = Config::get().input_mode;
  const bool right_stick_points = mode == 3 || mode == 4;   // stick droit reserve au pointeur
  const int layout = gfx::g_controller_layout;

  refresh_controllers();
  haptics_frame(g_open[0]);
  for (int port = 0; port < 4; ++port) {
    SDL_GameController *gc = g_open[port];
    if (!gc) continue;
    auto held = [gc](SDL_GameControllerButton b) { return SDL_GameControllerGetButton(gc, b) != 0; };
    auto axis = [gc](SDL_GameControllerAxis a) { return (int)SDL_GameControllerGetAxis(gc, a); };

    PadState &p = pads[port];
    p.err = 0;
    uint16_t bits = 0;
    const Bit *faces = (layout == 4) ? kFaithfulFaces : kFaces;
    const size_t faces_count = (layout == 4) ? (sizeof(kFaithfulFaces) / sizeof(Bit)) : (sizeof(kFaces) / sizeof(Bit));
    for (size_t i = 0; i < faces_count; ++i)
      if (held(faces[i].button)) bits |= faces[i].bit;
    const int lx = axis(SDL_CONTROLLER_AXIS_LEFTX), ly = axis(SDL_CONTROLLER_AXIS_LEFTY);
    const int rx = axis(SDL_CONTROLLER_AXIS_RIGHTX), ry = axis(SDL_CONTROLLER_AXIS_RIGHTY);
    const int lt = axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT), rt = axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    if (lx < -kDead || lx > kDead) p.stick_x = stick(lx, false);
    if (ly < -kDead || ly > kDead) p.stick_y = stick(ly, true);
    // Visee GameCube au stick (14/10) : plage ouverte par clamp_wide, courbe selon le reglage
    // (Fast lineaire : precis au centre ; Very fast : plus vif a mi-course).
    if (gfx::g_aim_mode == 1 && gfx::g_stick_aim > 0 && re4_aiming()) {
      const float e = gfx::g_stick_aim == 2 ? 0.7f : 1.0f;
      auto curve = [e](int raw, bool invert) -> int8_t {
        const float n = std::min(1.0f, (std::abs(raw) - kDead) / (32767.0f - kDead));
        if (n <= 0.0f) return 0;
        const float v = std::pow(n, e) * 127.0f * (raw < 0 ? -1.0f : 1.0f);
        return (int8_t)(invert ? -v : v);
      };
      if (lx < -kDead || lx > kDead) p.stick_x = curve(lx, false);
      if (ly < -kDead || ly > kDead) p.stick_y = curve(ly, true);
    }
    if (!right_stick_points) {
      if (rx < -kDead || rx > kDead) p.substick_x = stick(rx, false);
      if (ry < -kDead || ry > kDead) p.substick_y = stick(ry, true);
    }

    if (layout == 4) {
      // Disposition GameCube faithful (PS / Xbox)
      // R1 / RB -> GameCube Z (0x0010, Carte)
      if (held(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) bits |= 0x0010;
      // Gachette L2 / LT -> GameCube L (0x0040, Couteau / Rotation gauche)
      if (lt > 3000) bits |= 0x0040;
      p.trigger_l = (uint8_t)(std::clamp(lt, 0, 32767) / 32767.0f * 255.0f);
      // Gachette R2 / RT -> GameCube R (0x0020, Viser / Rotation droite)
      if (rt > 3000) bits |= 0x0020;
      p.trigger_r = (uint8_t)(std::clamp(rt, 0, 32767) / 32767.0f * 255.0f);
      p.buttons |= bits;
      const bool moved = std::abs(lx) > kDead || std::abs(ly) > kDead || std::abs(rx) > kDead || std::abs(ry) > kDead;
      if (port == 0 && (bits || p.trigger_l || p.trigger_r || moved)) gfx::g_last_device = 1;
      continue;
    }

    if (layout > 0) {
      for (const Bit &b : kUhdExtra)
        if (held(b.button)) bits |= b.bit;
      // gachettes : PadRead pose les bits L/R des qu'elles bougent ; un seuil evite qu'un leger
      // jeu vise ou sorte le couteau
      auto trigger = [](int v) -> uint8_t { return v > 3000 ? (uint8_t)(v / 32767.0f * 255.0f) : 0; };
      p.trigger_l = trigger(lt);
      p.trigger_r = trigger(rt);
      p.buttons |= bits;
      const bool moved = std::abs(lx) > kDead || std::abs(ly) > kDead || std::abs(rx) > kDead || std::abs(ry) > kDead;
      if (port == 0 && (bits || p.trigger_l || p.trigger_r || moved)) gfx::g_last_device = 1;
      continue;
    }

    // disposition GameCube
    if (held(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) {
      bits |= 0x0040;
      static bool said = false;
      if (!said) {
        said = true;
        NJ_PRINTF("[Manette] L enfonce : viser est impossible tant qu'il l'est\n");
        std::fflush(stdout);
      }
    }
    if (held(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) bits |= 0x0020;
    if (rt > 16384) bits |= 0x0010;   // Z
    p.buttons |= bits;
    p.trigger_l = (uint8_t)(lt / 32767.0f * 255.0f);
    p.trigger_r = (uint8_t)(rt / 32767.0f * 255.0f);
  }
}

}  // namespace nj::runtime::input

namespace nj::runtime::gfx {
// Toucher d'un bouton : 15 ms (legeres) ou 30 ms (fortes), vibreur du telephone.
void haptic_tap() {
#ifdef NJ_MOBILE
  if (SDL_Haptic *h = g_haptics ? nj::runtime::input::phone() : nullptr)
    SDL_HapticRumblePlay(h, g_haptics == 2 ? 0.8f : 0.4f, g_haptics == 2 ? 30 : 15);
#endif
}
}  // namespace nj::runtime::gfx
