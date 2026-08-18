#include "runtime/config.h"
#include "runtime/log.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>

#include "runtime/gfx_settings.h"
#include "input/touch_source.h"

namespace nj::runtime::gfx { extern int g_touch_simple, g_touch_controls, g_touch_anim, g_touch_size, g_touch_lock, g_debug_mode, g_compat_mask; }  // settings_menu.cpp
namespace nj::runtime {

Config &Config::get() {
  static Config c;
  return c;
}

namespace {
// Entier ou texte (« native », « uncapped ») selon ce que le joueur a ecrit.
template <class Fn>
void int_or_word(const toml::node_view<toml::node> &n, Fn &&set) {
  if (auto i = n.value<int64_t>()) set((int)*i, std::string());
  else if (auto s = n.value<std::string>()) set(std::atoi(s->c_str()), *s);
}
void flag(const toml::node_view<toml::node> &n, int &out) {
  if (auto b = n.value<bool>()) out = *b ? 1 : 0;
}
}  // namespace

// Reglages par defaut propres a chaque appareil (19/10) : un config.toml ecrit par une version plus ancienne garde des
// valeurs pensees pour l'autre plateforme (resolution native, HD, 16x sur un telephone ; boutons tactiles sur un PC).
// Quand kSettingsVersion monte, ces cles sont retirees du fichier une fois : le defaut de l'appareil revient, et la
// version est ecrite pour ne pas recommencer. Le reste (disque, personnage, mods) n'est jamais touche.
static std::string trim_left(const std::string &s) {
  const size_t i = s.find_first_not_of(" \t");
  return i == std::string::npos ? std::string() : s.substr(i);
}

static void migrate_settings(const std::string &path) {
  namespace g = gfx;
  std::vector<std::string> lines;
  {
    std::ifstream in(path);
    for (std::string l; std::getline(in, l);) lines.push_back(l);
  }
  int version = 0;
  bool in_meta = false;
  for (const std::string &l : lines) {
    const std::string t = trim_left(l);
    if (t.rfind("[", 0) == 0) in_meta = t.rfind("[meta]", 0) == 0;
    else if (in_meta && t.rfind("settings_version", 0) == 0 && t.find('=') != std::string::npos)
      version = std::atoi(t.c_str() + t.find('=') + 1);
  }
  if (version >= g::kSettingsVersion) return;
#ifdef NJ_MOBILE
  static const char *const kDrop[] = {"internal_resolution", "anisotropy", "framerate", "output_resampling", "vsync",
                                      "fullscreen", "window_width", "window_height", "hd_textures", "hd_cutscenes",
                                      "hd_interface", "hd_environment", "hd_items", "hd_weapons", "hd_case", "hd_hud",
                                      "uhd_case_models"};
#else
  static const char *const kDrop[] = {"touch_style", "touch_opacity", "touch_action", "touch_simple", "touch_controls",
                                      "touch_anim", "touch_size", "touch_lock", "haptics", "stick_aim", "button_style"};
#endif
  std::vector<std::string> out;
  std::string section;
  bool dropped = false;
  for (const std::string &l : lines) {
    const std::string t = trim_left(l);
    if (t.rfind("[", 0) == 0) section = t.substr(0, t.find(']') + 1);
    if (t.rfind("[meta]", 0) == 0) continue;   // reecrit plus bas
    if (section == "[meta]" && t.rfind("settings_version", 0) == 0) continue;
    bool drop = false;
    // version 3 (18/11, mobile) : seuls le style des boutons (Or) et leur animation (toujours) reviennent au defaut ;
    // la liste de la version 2 ne s'applique qu'aux fichiers plus anciens (sinon elle remettait aussi la resolution).
#ifdef NJ_MOBILE
    static const char *const kDrop3[] = {"touch_style", "touch_anim"};
#else
    static const char *const kDrop3[] = {"touch_style"};
#endif
    if (section == "[graphics]" && version == 2)
      for (const char *k : kDrop3) {
        const size_t n = std::strlen(k), eq = t.find('=');
        if (t.rfind(k, 0) == 0 && eq != std::string::npos && eq >= n &&
            t.substr(n, eq - n).find_first_not_of(" \t") == std::string::npos) {
          drop = true;
          break;
        }
      }
    if (section == "[graphics]" && version < 2)
      for (const char *k : kDrop) {
        const size_t n = std::strlen(k), eq = t.find('=');
        if (t.rfind(k, 0) == 0 && eq != std::string::npos && eq >= n &&
            t.substr(n, eq - n).find_first_not_of(" \t") == std::string::npos) {
          drop = true;
          break;
        }
      }
    if (drop) dropped = true;
    else out.push_back(l);
  }
  out.push_back("");
  out.push_back("[meta]");
  out.push_back("settings_version = " + std::to_string(g::kSettingsVersion));
  std::ofstream f(path, std::ios::trunc);
  for (const std::string &l : out) f << l << "\n";
  g::g_settings_reset_notice = dropped;
  NJ_LOG << "[Config] reglages par defaut de l'appareil remis (version " << version << " -> " << g::kSettingsVersion
         << ")" << std::endl;
}

void Config::load(const std::string &path) {
  if (!std::filesystem::exists(path)) {
    NJ_LOG << "[Config] pas de " << path << " : reglages par defaut" << std::endl;
    return;
  }
  migrate_settings(path);
  toml::table t;
  try {
    t = toml::parse_file(path);
  } catch (const toml::parse_error &e) {
    NJ_ERR << "[Config] " << path << " illisible : " << e << std::endl;
    return;
  }
  namespace g = gfx;
  auto gr = t["graphics"];

  if (auto v = gr["vsync"].value<bool>()) vsync_on = *v;
  if (auto v = gr["window_width"].value<int64_t>()) window_width = (int)*v;
  if (auto v = gr["window_height"].value<int64_t>()) window_height = (int)*v;
  if (auto v = gr["fullscreen"].value<bool>()) fullscreen = *v;
  if (auto v = t["disc"]["second"].value<std::string>()) disc2_image = *v;
  if (auto v = t["ui"]["language"].value<std::string>()) ui_language = *v;

  // cadence : 30, 60, 120... ou « uncapped »
  // 28/10 (demande de l'utilisateur) : les cadences sont de retour, 30 par defaut (la logique du jeu reste a 30 ; au-dela,
  // images intermediaires de la scene 3D, frame_interp.cpp).
  int_or_word(gr["framerate"], [](int n, const std::string &w) {
    if (w == "uncapped" || w == "unlimited") g::g_fps_target = 0;
    else g::g_fps_target = n >= 30 ? std::min(n, 240) : 30;
  });
  // resolution interne : « native » ou 1 a kMaxScale
  int_or_word(gr["internal_resolution"], [](int n, const std::string &w) {
    g::g_scale_setting = w == "native" ? 0 : std::clamp(n, 0, g::kMaxScale);
  });
  if (auto v = gr["output_resampling"].value<std::string>()) g::g_resample = *v == "area" ? 1 : 0;
  if (auto v = gr["anisotropy"].value<int64_t>()) {
    int a = 1;
    for (int p : {2, 4, 8, 16})
      if (*v >= p) a = p;
    g::g_aniso = a;
  }
  if (auto v = gr["aspect"].value<std::string>()) g::g_aspect = *v == "4:3" ? 1 : 0;
  if (auto v = gr["screen_fit"].value<int64_t>()) g::g_screen_fit = (int)std::clamp<int64_t>(*v, 0, 3);
  if (auto v = gr["screen_ratio"].value<int64_t>()) g::g_screen_ratio = (int)std::clamp<int64_t>(*v, 0, g::kScreenRatioCount - 1);
  flag(gr["hd_textures"], g::g_hd_textures);
  flag(gr["hd_cutscenes"], g::g_hd_cutscenes);
  flag(gr["hd_interface"], g::g_hd_interface);
  flag(gr["hd_environment"], g::g_hd_environment);
  flag(gr["hd_items"], g::g_hd_items);
  flag(gr["hd_weapons"], g::g_hd_weapons);
  flag(gr["hd_case"], g::g_hd_case);
  flag(gr["hd_hud"], g::g_hd_hud);
  flag(gr["uhd_case_models"], g::g_uhd_case_models);
  flag(gr["early_drawdone"], g::g_early_drawdone);
  flag(gr["disable_fog"], g::g_no_fog);
  if (auto v = gr["controller_layout"].value<int64_t>()) g::g_controller_layout = (int)std::clamp<int64_t>(*v, 0, 4);
  if (auto v = gr["touch_style"].value<int64_t>()) g::g_touch_style = (int)std::clamp<int64_t>(*v, 0, 2);
  if (auto v = gr["touch_opacity"].value<int64_t>()) g::g_touch_opacity = (int)std::clamp<int64_t>(*v, 20, 100);
  if (auto v = gr["touch_action"].value<int64_t>()) g::g_touch_action = (int)std::clamp<int64_t>(*v, 0, 1);
  if (auto v = gr["touch_simple"].value<int64_t>()) g::g_touch_simple = (int)std::clamp<int64_t>(*v, 0, 1);
  if (auto v = gr["touch_lock"].value<int64_t>()) g::g_touch_lock = (int)std::clamp<int64_t>(*v, 0, 1);
  if (auto v = gr["touch_controls"].value<int64_t>()) g::g_touch_controls = (int)std::clamp<int64_t>(*v, 0, 2);
  if (auto v = gr["debug_mode"].value<int64_t>()) g::g_debug_mode = (int)std::clamp<int64_t>(*v, 0, 1);
  if (auto v = gr["touch_anim"].value<int64_t>()) g::g_touch_anim = (int)std::clamp<int64_t>(*v, 0, 2);
  if (auto v = gr["title_menu"].value<int64_t>()) g::g_title_menu = (int)std::clamp<int64_t>(*v, 0, 1);
  if (auto v = gr["title_scene"].value<int64_t>()) g::g_title_scene = (int)std::clamp<int64_t>(*v, 0, 1);
  if (auto v = gr["leon_outfit"].value<int64_t>()) g::g_leon_outfit = (int)std::clamp<int64_t>(*v, -1, 3);
  if (auto v = gr["compat_mask"].value<int64_t>()) g::g_compat_mask = (int)std::clamp<int64_t>(*v, 0, 15);
  if (auto v = gr["touch_size"].value<int64_t>()) g::g_touch_size = (int)std::clamp<int64_t>(*v, 70, 130);
  if (auto v = gr["haptics"].value<int64_t>()) g::g_haptics = (int)std::clamp<int64_t>(*v, 0, 2);
  if (auto v = gr["stick_aim"].value<int64_t>()) g::g_stick_aim = (int)std::clamp<int64_t>(*v, 0, 2);
  if (auto v = gr["button_style"].value<int64_t>()) g::g_button_style = *v == 2 ? 1 : (int)std::clamp<int64_t>(*v, 0, 4);   // PlayStation retire (13/10)

  auto in = t["input"];
  if (auto v = in["mode"].value<int64_t>()) input_mode = (int)*v;
  if (auto v = in["mouse_turn"].value<bool>()) mouse_turn = *v;
  if (auto v = in["mouse_sensitivity"].value<double>(); v && *v > 0) mouse_sensitivity = (float)*v;

  auto wa = t["wii_aiming"];
  if (auto v = wa["sensitivity"].value<double>(); v && *v > 0) wii_sensitivity = (float)*v;
  if (auto v = wa["edge_turn"].value<bool>()) wii_edge_turn = *v;
  if (auto v = wa["turn_sensitivity"].value<int64_t>()) wii_turn_sensitivity = (int)*v;
  if (auto v = wa["edge_zone"].value<int64_t>()) wii_edge_zone = (int)*v;
  if (auto v = wa["edge_zone_speed"].value<int64_t>()) wii_edge_zone_speed = (int)*v;
  if (auto v = wa["friction"].value<int64_t>()) wii_friction = (int)*v;
  if (auto v = wa["hit_marker"].value<bool>()) wii_hit_marker = *v;
  if (auto v = wa["keep_point"].value<bool>()) wii_keep_point = *v;
  if (auto v = wa["enabled"].value<bool>()) g::g_aim_mode = *v ? 0 : 1;
  if (auto v = wa["mode"].value<std::string>()) g::g_aim_mode = *v == "wii" ? 0 : *v == "pc" ? 2 : 1;
  if (auto v = wa["detach_camera"].value<bool>()) g::g_aim_detach = *v ? 1 : 0;
  // disposition personnalisee des boutons tactiles : « nom = "dx,dy,echelle" » (input/touch_source.cpp)
  if (auto tl = t["touch_layout"].as_table())
    for (auto &&[k, v] : *tl)
      if (auto sv = v.value<std::string>()) {
        float dx = 0, dy = 0, sc = 1;
        if (std::sscanf(sv->c_str(), "%f,%f,%f", &dx, &dy, &sc) == 3)
          input::layout_set_by_name(std::string(k.str()), dx, dy, sc);
      }
  if (auto v = t["player"]["character"].value<int64_t>(); v && *v >= -1 && *v <= 6) g::g_character = (int)*v;

#ifndef NJ_RELEASE
  // developpement seulement : identifiant de jeu force
  if (auto v = t["game"]["id"].value<std::string>()) game_id = *v;
#endif
}

}  // namespace nj::runtime
