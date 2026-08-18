// Sources d'entree selon la configuration (input_mode), puis les sources toujours presentes.
#include "bindings.h"
#include "input/input_manager.h"
#include "sources/autostart_source.h"
#include "sources/debug_menu_skip_source.h"
#include "sources/gamepad_source.h"
#include "sources/mouse_keyboard_source.h"
#include "sources/remote_source.h"
#include "sources/script_source.h"
#include "input/touch_source.h"
#include "runtime/config.h"
#include "runtime/env.h"
#include "runtime/log.h"

#include <memory>

namespace nj::runtime::input {

// Charge les assignations une fois, avant que la moindre source ne tourne (InputHub::update
// est appele de deux endroits : un chargement paresseux pouvait vider la table pendant sa lecture).
// Modes : 2-4 manette d'abord, 5 et 7 clavier/souris seuls (7 : le pointeur joue le doigt), 1 (Wiimote
// Bluetooth) non pris en charge -> clavier/souris, 6 et autres : clavier/souris puis manette.
void install_input_sources() {
  const char *ini = NJ_ENV("NJ_INPUT_INI");
  bindings_load(ini ? ini : "input.ini");

  auto &im = InputHub::get();
  const int mode = Config::get().input_mode;
  const bool pad_first = mode >= 2 && mode <= 4;
  const bool keyboard_only = mode == 1 || mode == 5 || mode == 7;
  if (mode == 1)
    NJ_LOG << "[Input] mode 1 (Wiimote Bluetooth) non pris en charge : clavier et souris" << std::endl;
  if (pad_first)
    im.plug_source(std::make_unique<JoypadInput>());
  im.plug_source(std::make_unique<KbmSource>());
  if (!pad_first && !keyboard_only)
    im.plug_source(std::make_unique<JoypadInput>());

  // Commandes tactiles : toujours la, muettes tant qu'aucun doigt n'a touche l'ecran
  // (NJ_TOUCH_MOUSE=1 : la souris joue un doigt sur PC).
  im.plug_source(std::make_unique<TouchSource>());
  // Sources d'essai, ajoutees par-dessus les autres : demarrage automatique, menu de developpeur
  // du titre passe tout seul (mode debug coupe), script de touches, manette du pont de debug.
  if (AutoStartSource::enabled())
    im.plug_source(std::make_unique<AutoStartSource>());
  if (DebugMenuSkipSource::enabled())
    im.plug_source(std::make_unique<DebugMenuSkipSource>());
  if (ScriptSource::enabled())
    im.plug_source(std::make_unique<ScriptSource>());
#ifndef NJ_RELEASE
  if (RemoteSource::enabled())
    im.plug_source(std::make_unique<RemoteSource>());
#endif
}

}  // namespace nj::runtime::input
