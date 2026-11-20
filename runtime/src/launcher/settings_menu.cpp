// Menu des parametres video (F10) : resolution de la fenetre, plein ecran, resolution interne,
// synchro verticale. Les choix s'appliquent tout de suite et s'enregistrent dans config.toml.
#include "runtime/native/native_core.h"   // pause du jeu natif quand le menu s'ouvre
#include "runtime/mods.h"
#include "runtime/saves.h"
#include "runtime/report.h"
#include "host.h"
#include "i18n.h"
#include "input/touch_overlay.h"
#include "input/touch_source.h"
#include "ui_fonts.h"
#include "previews.h"
#include "rmenu.h"
#include "ui_skin.h"
#include <ctime>
#include <sys/stat.h>
#include <filesystem>
#include "runtime/gfx_settings.h"
#include "runtime/log.h"
#include "runtime/config.h"
#include "runtime/dispatch.h"
#include "runtime/env.h"
#include "runtime/gpu_select.h"

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"

#include "common/gl.h"

#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "runtime/obf.h"

using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

namespace nj::runtime::gfx {

int g_scale = 1;
unsigned g_game_images = 0;
#ifdef NJ_MOBILE
// Telephone : 640x480 par defaut. « Natif » donnait 4x (2560x1920) sur un ecran 1080p, bien
// au-dela de ce que tient un GPU mobile ; le joueur peut monter dans le menu.
int g_scale_setting = 1;
#else
int g_scale_setting = 0;
#endif
bool g_menu_open = false;
int g_resample = 1;   // surface par defaut : sans effet si l image n est pas reduite
int g_aniso = 1;
int g_no_fog = 0;
int g_aspect = 0;
int g_screen_fit = 0, g_screen_ratio = 0;
float screen_ratio_value(int index) {
    static const float r[kScreenRatioCount] = {0.0f, 4.0f / 3.0f, 3.0f / 2.0f, 16.0f / 10.0f, 16.0f / 9.0f, 18.0f / 9.0f,
                                               19.5f / 9.0f, 20.0f / 9.0f, 21.0f / 9.0f};
    return index > 0 && index < kScreenRatioCount ? r[index] : 0.0f;
}
int g_hd_textures = 0;
int g_hd_cutscenes = 0;
int g_hd_interface = 0;
int g_hd_environment = 0;
int g_hd_items = 0;
int g_hd_weapons = 0;
int g_hd_case = 0;
int g_hd_hud = 0;
int g_uhd_case_models = 0;
int g_early_drawdone = 1;
// Invites des boutons : tactiles sur mobile (Android et iOS, 29/10 : le joueur n'a pas de manette GameCube en main),
// GameCube ailleurs ; le choix du menu F10 / config.toml l'emporte.
#if defined(__ANDROID__) || defined(NJ_MOBILE)
int g_button_style = 4;
#else
int g_button_style = 0;
#endif
int g_controller_layout = 3;
int g_touch_style = 2;   // boutons Or (Nightjar) par defaut (18/11, choix de l'utilisateur ; 1 = pierre, 0 = classique)
int g_touch_opacity = 80;
int g_touch_action = 0;
// Commandes tactiles simples (16/10) : trois boutons a droite (action, viser, couteau) ; la course s'enclenche
// toute seule quand le stick est pousse a fond (B maintenu), et tirer le stick a fond vers le bas fait le demi-tour
// rapide de RE4 (B + bas). 0 = complet (course et demi-tour sur des boutons).
int g_touch_simple = 1;
// Verrous course / viser / couteau (19/10) : 0 un appui court verrouille, 1 double appui court.
int g_touch_lock = 0;
bool g_settings_reset_notice = false;
int g_font_script = 0;      // ecriture des polices construites (i18n::Script) : un autre jeu de glyphes demande un redemarrage
bool g_lang_restart = false;
// Commandes tactiles (16/10, retour des testeurs) : 0 automatique (cachees quand une manette vient de servir, elles
// reviennent au toucher), 1 toujours affichees, 2 cachees (seul l'engrenage des reglages reste).
int g_touch_controls = 0;
// Compatibilite de rendu (bits) : 1 sans invalidation des tampons, 2 sans transformation des sommets sur le GPU, 4 sans
// geometrie residente, 8 sans caches de decodage. 0 = tout actif (defaut). Cle compat_mask de config.toml.
int g_compat_mask = 0;
int g_title_menu = 1;
int g_leon_outfit = -1;   // garde-robe (18/11) : -1 automatique (le jeu decide), 0 veste, 1 chemise, 2 gilet, 3 RPD
int g_title_scene = 1;   // fond vivant : la salle r100 en direct derriere l'accueil (18/11)    // menu titre de Nightjar par defaut (18/11)
int g_touch_anim = 1;    // icones animees : 1 toujours (defaut depuis le 29/10), 2 seulement quand on s'en sert, 0 jamais
int g_touch_size = 100;  // taille des boutons, en %
// Mode debug (16/10, demande des testeurs) : au prochain lancement, le disque debug garde son debug/config.txt tel quel
// (affichage de debug, barres de temps) et le menu de developpeur du titre reste (stage, salle, ennemis...). Aucun effet
// sur les disques commerciaux : ils n'ont pas ce code.
int g_debug_mode = 0;
int g_haptics = 1;
// Options du jeu (SystemSave, hle/pad.cpp) : courant et demande
int g_game_cur_brightness = -1, g_game_cur_reverse = -1, g_game_cur_vibration = -1, g_game_cur_knife = -1;
int g_game_cur_snd = -1, g_game_cur_retry_ok = -1, g_game_set_snd = -1, g_game_set_menu = 0;
int g_game_set_brightness = -1, g_game_set_reverse = -1, g_game_set_vibration = -1, g_game_set_knife = -1, g_game_set_reset = 0;
int g_stick_aim = 1;
volatile int g_last_device = 0;
uint32_t g_case_pzzl_extra = 0;
// Visee GameCube au stick par defaut (16/10, demande de l'utilisateur) : jouable sans ouvrir le menu F10, sur manette
// comme au tactile. La visee au pointeur Wii et celle de la souris PC restent au menu (« Aim with »).
int g_aim_mode = 1;
int g_aim_detach = 0;
int g_character = -1;
// 30 partout par defaut (06/10) : le 60 par interpolation ne tient pas encore sur PC (26 i/s en jeu,
// artefacts d'interface). Le joueur peut toujours choisir 60 dans le menu ; a remettre quand la couche
// graphique native le tiendra (docs/SOIXANTE_IPS.md).
int g_fps_target = 30;
bool g_show_fps = true;

bool present_due(bool new_game_frame) {
    using C = std::chrono::steady_clock;
    static C::time_point next = C::now();
    const int f = g_fps_target;
    if (g_menu_open)
        return true;
    if (f <= 30) {
        // Presenter seulement une image neuve du jeu. Echanger les tampons a chaque tour de la
        // boucle (synchro verticale : 16,6 ms bloquees) retardait chaque GXDrawDone du jeu
        // jusqu'au tour suivant, puisque c'est ce tour qui analyse le flux et leve le signal PE.
        // Le chateau en fait une quinzaine par image (Esp0e_HideCheck, une par lueur de bougie) :
        // 15 x 16,6 ms, 4 i/s (mesure du 27/09, NJ_PETRACE). Filet de 100 ms pour que la
        // fenetre vive pendant un chargement. NJ_PRESENTOLD=1 : ancien comportement (essais A/B).
        static const bool old = NJ_ENV("NJ_PRESENTOLD") != nullptr;
        if (old)
            return true;
        static C::time_point last = C::now();
        const auto now = C::now();
        if (new_game_frame || now - last >= std::chrono::milliseconds(100)) {
            last = now;
            return true;
        }
        return false;
    }
    const auto now = C::now();
    if (now < next)
        return false;
    const auto period = std::chrono::nanoseconds(1000000000LL / f);
    next += period;
    if (next < now)
        next = now + period;   // en retard : on repart de maintenant, pas de rafale
    return true;
}

namespace {

// Texte traduit dont le format recoit une valeur (message d'erreur d'un module, en anglais) : « Import: %s. »
std::string tr_fmt(const char *en_format, const std::string &value) {
    char b[600];
    std::snprintf(b, sizeof b, TXT(en_format), value.c_str());
    return b;
}

std::string g_config_path = "config.toml";
ImGuiContext *g_ctx = nullptr;
bool g_vsync = true;

struct Mode {
    int w, h;
};

// Le jeu montre 448 lignes (512 x 448 dans l'EFB) ; en 16:9 il n'en garde que les trois quarts.
// Le plus petit S dont ces lignes couvrent la hauteur de la fenetre, pour ne jamais agrandir.
// « Natif » : la plus petite echelle dont l'EFB (640x480 x s) couvre la fenetre, en largeur et
// en hauteur. L'ancienne formule (ceil(h / 336)) suréchantillonnait : 3x en 720p, 4x (2560x1920)
// en 1080p, soit des copies d'EFB et une memoire GPU bien au-dela de ce qu'affiche la fenetre
// (12/10 : fermeture du jeu au passage 720p -> 1080p, PC a court de memoire).
int native_scale(int draw_w, int draw_h) {
    const int sw = (draw_w + kEfbW - 1) / kEfbW, sh = (draw_h + kEfbH - 1) / kEfbH;
    return std::max(1, std::min(kMaxScale, std::max(sw, sh)));
}

std::vector<Mode> window_modes(SDL_Window *window) {
    std::vector<Mode> out;
    const int d = std::max(0, SDL_GetWindowDisplayIndex(window));
    SDL_DisplayMode desk{};
    SDL_GetDesktopDisplayMode(d, &desk);
    for (int i = 0, n = SDL_GetNumDisplayModes(d); i < n; ++i) {
        SDL_DisplayMode m{};
        if (SDL_GetDisplayMode(d, i, &m) != 0 || m.w < 640 || m.h < 480) continue;
        if (desk.w > 0 && (m.w > desk.w || m.h > desk.h)) continue;
        if (std::none_of(out.begin(), out.end(), [&](const Mode &o) { return o.w == m.w && o.h == m.h; }))
            out.push_back({m.w, m.h});
    }
    // Les tailles 4:3 classiques, au cas ou le pilote n'en liste pas.
    for (Mode m : {Mode{640, 480}, Mode{1280, 960}, Mode{1600, 1200}})
        if ((desk.w <= 0 || (m.w <= desk.w && m.h <= desk.h)) &&
            std::none_of(out.begin(), out.end(), [&](const Mode &o) { return o.w == m.w && o.h == m.h; }))
            out.push_back(m);
    std::sort(out.begin(), out.end(), [](const Mode &a, const Mode &b) {
        return a.w != b.w ? a.w < b.w : a.h < b.h;
    });
    return out;
}

// Remplace (ou ajoute) les cles de [graphics] sans toucher au reste du fichier ni aux commentaires.
void save_config(SDL_Window *window) {
    std::vector<std::string> lines;
    {
        std::ifstream in(g_config_path);
        for (std::string l; std::getline(in, l);) lines.push_back(l);
    }
    const bool fs = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP;
    int ww = 0, wh = 0;
    SDL_GetWindowSize(window, &ww, &wh);
    Config &cfg = Config::get();
    if (!fs) {
        cfg.window_width = ww;
        cfg.window_height = wh;
    }
    cfg.fullscreen = fs;
    cfg.vsync_on = g_vsync;
    const std::string ir = g_scale_setting == 0 ? "\"native\"" : std::to_string(g_scale_setting);
    const std::vector<std::pair<std::string, std::string>> keys = {
        {"window_width", std::to_string(cfg.window_width)},
        {"window_height", std::to_string(cfg.window_height)},
        {"fullscreen", fs ? "true" : "false"},
        {"vsync", g_vsync ? "true" : "false"},
        {"internal_resolution", ir},
        {"framerate", g_fps_target == 0 ? std::string("\"uncapped\"") : std::to_string(g_fps_target)},
        {"output_resampling", g_resample == 1 ? "\"area\"" : "\"bilinear\""},
        {"anisotropy", std::to_string(g_aniso)},
        {"disable_fog", g_no_fog ? "true" : "false"},
        {"aspect", g_aspect == 1 ? "\"4:3\"" : "\"16:9\""},
        {"screen_fit", std::to_string(g_screen_fit)},
        {"screen_ratio", std::to_string(g_screen_ratio)},
        {"hd_textures", g_hd_textures ? "true" : "false"},
        {"hd_cutscenes", g_hd_cutscenes ? "true" : "false"},
        {"hd_interface", g_hd_interface ? "true" : "false"},
        {"hd_environment", g_hd_environment ? "true" : "false"},
        {"hd_items", g_hd_items ? "true" : "false"},
        {"hd_weapons", g_hd_weapons ? "true" : "false"},
        {"hd_case", g_hd_case ? "true" : "false"},
        {"hd_hud", g_hd_hud ? "true" : "false"},
        {"uhd_case_models", g_uhd_case_models ? "true" : "false"},
        {"button_style", std::to_string(g_button_style)},
        {"controller_layout", std::to_string(g_controller_layout)},
        {"touch_style", std::to_string(g_touch_style)},
        {"touch_opacity", std::to_string(g_touch_opacity)},
        {"touch_action", std::to_string(g_touch_action)},
        {"touch_simple", std::to_string(g_touch_simple)},
        {"touch_lock", std::to_string(g_touch_lock)},
        {"touch_controls", std::to_string(g_touch_controls)},
        {"debug_mode", std::to_string(g_debug_mode)},
        {"touch_anim", std::to_string(g_touch_anim)},
        {"title_menu", std::to_string(g_title_menu)},
        {"title_scene", std::to_string(g_title_scene)},
        {"leon_outfit", std::to_string(g_leon_outfit)},
        {"compat_mask", std::to_string(g_compat_mask)},
        {"touch_size", std::to_string(g_touch_size)},
        {"haptics", std::to_string(g_haptics)},
        {"stick_aim", std::to_string(g_stick_aim)},
    };
    auto trim = [](std::string s) {
        s.erase(0, s.find_first_not_of(" \t"));
        return s;
    };
    // Ecrit des cles dans une section (creee au besoin), sans toucher au reste du fichier.
    auto write_section = [&](const std::string &name,
                             const std::vector<std::pair<std::string, std::string>> &keys) {
    int sec = -1, sec_end = (int) lines.size();
    for (int i = 0; i < (int) lines.size(); ++i) {
        const std::string t = trim(lines[i]);
        if (t.rfind("[", 0) == 0) {
            if (sec >= 0) { sec_end = i; break; }
            if (t.rfind(name, 0) == 0) sec = i;
        }
    }
    if (sec < 0) {
        lines.push_back("");
        lines.push_back(name);
        sec = (int) lines.size() - 1;
        sec_end = (int) lines.size();
    }
    for (const auto &kv : keys) {
        bool done = false;
        for (int i = sec + 1; i < sec_end; ++i) {
            const std::string t = trim(lines[i]);
            if (t.rfind(kv.first, 0) == 0) {
                const size_t eq = t.find('=');
                if (eq != std::string::npos && trim(t.substr(kv.first.size(), eq - kv.first.size())).empty()) {
                    lines[i] = kv.first + " = " + kv.second;
                    done = true;
                    break;
                }
            }
        }
        if (!done) {
            lines.insert(lines.begin() + sec_end, kv.first + " = " + kv.second);
            ++sec_end;
        }
    }
    };
    write_section("[graphics]", keys);
    write_section("[wii_aiming]", {{"enabled", g_aim_mode == 0 ? "true" : "false"},
                                   {"mode", g_aim_mode == 0 ? "\"wii\"" : g_aim_mode == 1 ? "\"gamecube\"" : "\"pc\""},
                                   {"detach_camera", g_aim_detach ? "true" : "false"}});
    write_section("[player]", {{"character", std::to_string(g_character)}});
    write_section("[meta]", {{"settings_version", std::to_string(kSettingsVersion)}});
    write_section("[touch_layout]", nj::runtime::input::layout_export());
    write_section("[ui]", {{"language", "\"" + Config::get().ui_language + "\""}});
    std::ofstream out(g_config_path, std::ios::trunc);
    for (const auto &l : lines) out << l << "\n";
    NJ_PRINTF("[Parametres] enregistres dans %s\n", g_config_path.c_str());
}

ImFont *g_serif = nullptr;   // serif des menus style RE4 (nullptr : police par defaut)

void init_imgui(SDL_Window *window) {
    IMGUI_CHECKVERSION();
    nj::runtime::i18n::init(Config::get().ui_language.c_str());
    if (!nj::runtime::ui::language_font_available()) nj::runtime::i18n::init("en");   // pas de police CJK ici : anglais
    g_font_script = (int)nj::runtime::i18n::script();
    ImGuiContext *prev = ImGui::GetCurrentContext();
    g_ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(g_ctx);
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGuiStyle &st = ImGui::GetStyle();
    st.WindowRounding = 8.0f;
    st.FrameRounding = 6.0f;
    st.TabRounding = 6.0f;
    st.GrabRounding = 6.0f;
    st.WindowPadding = ImVec2(14.0f, 12.0f);
    st.ItemSpacing = ImVec2(8.0f, 7.0f);
    st.WindowBorderSize = 1.0f;
    // charte Nightjar (lanceur) : noir, rouge, texte creme
    {
        ImVec4 *c = st.Colors;
        const ImVec4 red(0.70f, 0.07f, 0.11f, 1.0f), red_hi(0.84f, 0.16f, 0.19f, 1.0f),
            red_dim(0.42f, 0.05f, 0.08f, 1.0f), text(0.925f, 0.91f, 0.886f, 1.0f);
        c[ImGuiCol_Text] = text;
        c[ImGuiCol_TextDisabled] = ImVec4(0.63f, 0.60f, 0.57f, 1.0f);
        c[ImGuiCol_WindowBg] = ImVec4(0.045f, 0.045f, 0.05f, 1.0f);
        c[ImGuiCol_PopupBg] = ImVec4(0.06f, 0.06f, 0.07f, 0.98f);
        c[ImGuiCol_Border] = ImVec4(0.42f, 0.05f, 0.08f, 0.6f);
        c[ImGuiCol_TitleBg] = ImVec4(0.03f, 0.03f, 0.035f, 1.0f);
        c[ImGuiCol_TitleBgActive] = red_dim;
        c[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.12f, 0.12f, 1.0f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.17f, 0.17f, 1.0f);
        c[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.19f, 0.19f, 1.0f);
        c[ImGuiCol_Button] = red_dim;
        c[ImGuiCol_ButtonHovered] = red;
        c[ImGuiCol_ButtonActive] = red_hi;
        c[ImGuiCol_Header] = red_dim;
        c[ImGuiCol_HeaderHovered] = red;
        c[ImGuiCol_HeaderActive] = red_hi;
        c[ImGuiCol_Tab] = ImVec4(0.12f, 0.11f, 0.11f, 1.0f);
        c[ImGuiCol_TabHovered] = red;
        c[ImGuiCol_TabActive] = red_dim;
        c[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.11f, 0.11f, 1.0f);
        c[ImGuiCol_TabUnfocusedActive] = red_dim;
        c[ImGuiCol_CheckMark] = red_hi;
        c[ImGuiCol_SliderGrab] = red;
        c[ImGuiCol_SliderGrabActive] = red_hi;
        c[ImGuiCol_PlotHistogram] = red;
        c[ImGuiCol_Separator] = ImVec4(0.42f, 0.05f, 0.08f, 0.5f);
        c[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.22f, 0.22f, 1.0f);
    }
    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(window, &w, &h);
#ifdef NJ_MOBILE
    // au doigt : texte plus grand, zones de toucher plus larges (13/10)
    const float px = std::max(22.0f, h * 0.034f);
    st.ScaleAllSizes(1.5f);
    st.TouchExtraPadding = ImVec2(6.0f, 6.0f);
    st.ScrollbarSize = std::max(st.ScrollbarSize, px * 1.1f);
    st.GrabMinSize = std::max(st.GrabMinSize, px * 1.2f);
#else
    const float px = std::max(16.0f, h * 0.026f);
#endif
#if defined(_WIN32)
    nj::runtime::ui::add_font(io, "C:/Windows/Fonts/segoeui.ttf", px);
#elif defined(__ANDROID__)
    nj::runtime::ui::add_font(io, "/system/fonts/Roboto-Regular.ttf", px);
#endif
    {   // serif du menu style RE4 (le jeu a des titres a empattements) : latin et cyrillique seulement
        const auto sc = nj::runtime::i18n::script();
        if (sc == nj::runtime::i18n::Script::kLatin || sc == nj::runtime::i18n::Script::kCyrillic) {
#if defined(_WIN32)
            g_serif = nj::runtime::ui::add_font(io, "C:/Windows/Fonts/georgia.ttf", px);
#elif defined(__ANDROID__)
            g_serif = nj::runtime::ui::add_font(io, "/system/fonts/NotoSerif-Regular.ttf", px);
#endif
        }
    }
    if (io.Fonts->Fonts.empty()) {
        ImFontConfig c;
        c.SizePixels = px;
        io.Fonts->AddFontDefault(&c);
    }
    ImGui_ImplSDL2_InitForOpenGL(window, SDL_GL_GetCurrentContext());
#ifdef NJ_GLES
    ImGui_ImplOpenGL3_Init("#version 300 es"_g);
#else
    ImGui_ImplOpenGL3_Init("#version 150"_g);
#endif
    if (prev) ImGui::SetCurrentContext(prev);
}

SDL_Window *g_menu_window = nullptr;   // fenetre du menu, pour enregistrer a la fermeture (14/10)

void set_open(bool open) {
    // Toute fermeture enregistre les reglages (14/10) : plus de changements perdus sans « Save ».
    if (g_menu_open && !open && g_menu_window) save_config(g_menu_window);
    if (g_menu_open && !open) g_settings_reset_notice = false;
    if (g_menu_open != open && nj::native::loaded()) nj::native::pause(open);   // jeu natif : en pause tant que le menu est ouvert
    g_menu_open = open;
#ifdef NJ_MOBILE
    // Au doigt : ImGui ne lit que la souris, SDL lui traduit les touchers le temps du menu (le
    // jeu, lui, lit les doigts directement et coupe cette traduction).
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, open ? "1" : "0");
    SDL_SetRelativeMouseMode(SDL_FALSE);   // positions absolues pour les touchers traduits (13/10)
#else
    // Le jeu tient la souris en mode relatif ; le menu a besoin d'un vrai curseur.
    SDL_SetRelativeMouseMode(open ? SDL_FALSE : SDL_TRUE);
    SDL_ShowCursor(open ? SDL_ENABLE : SDL_DISABLE);
#endif
}

}  // namespace

int wanted_scale(int draw_w, int draw_h) {
    return g_scale_setting > 0 ? std::min(kMaxScale, g_scale_setting) : native_scale(draw_w, draw_h);
}

void set_config_path(const char *path) {
    if (path && *path) g_config_path = path;
    nj::runtime::input::layout_set_saved_callback([] { if (g_menu_window) save_config(g_menu_window); });   // « valider » de l'edition
    g_vsync = Config::get().vsync_on;
}

void menu_toggle() { set_open(!g_menu_open); }

void menu_event(const void *ev) {
    const SDL_Event &e = *static_cast<const SDL_Event *>(ev);
    if (e.type == SDL_KEYDOWN && !e.key.repeat && (e.key.keysym.scancode == SDL_SCANCODE_F10 || e.key.keysym.sym == SDLK_F10)) {
        set_open(!g_menu_open);
        return;
    }
    if ((g_menu_open || g_title_home) && g_ctx) {   // le menu ou l'ecran d'accueil (18/11)
        ImGuiContext *prev = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(g_ctx);
        ImGui_ImplSDL2_ProcessEvent(&e);
        if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) set_open(false);
        ImGui::SetCurrentContext(prev);
    }
}

// ---- onglets Saves et Report (13/10) ----------------------------------------------------------
namespace {
namespace host = nj::launcher::host;

std::vector<nj::saves::File> s_card;
std::string s_card_status;
bool s_card_read = false;
std::vector<nj::saves::File> s_offer;   // fichiers proposes a l'import (apercu)
std::string s_offer_from, s_save_msg;
bool s_save_ok = true, s_convert = true, s_importing = false, s_exporting = false;

void refresh_card() {
    s_card_read = true;
    nj::saves::list_card(s_card, s_card_status);
}

std::string u8s(const std::filesystem::path &p) {
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

std::string region_name(const std::string &g) {
    if (g.size() < 4) return g;
    switch (g[3]) {
    case 'E': return "USA";
    case 'P': return "Europe";
    case 'J': return "Japan";
    default: return g;
    }
}

std::string save_label(const nj::saves::File &f) {
    if (f.name == "bh4_system") return "System file";
    if (f.name.rfind("bh4_data", 0) == 0 && f.name.size() >= 10) {
        char b[16];
        std::snprintf(b, sizeof b, "FILE %02d", std::atoi(f.name.c_str() + 8) + 1);
        return b;
    }
    return f.name;
}

std::string save_when(uint32_t t2000) {
    const std::time_t t = std::time_t(t2000) + 946684800;   // secondes depuis le 1/1/2000, heure locale de la console
    char b[32];
    std::strftime(b, sizeof b, "%Y-%m-%d %H:%M", std::gmtime(&t));
    return b;
}

void offer_file(const std::string &path) {
    std::string err;
    s_offer.clear();
    s_offer_from = u8s(std::filesystem::u8path(path).filename());
    if (!nj::saves::read_import(path, s_offer, err)) {
        s_save_msg = tr_fmt(N_("Import: %s."), err);
        s_save_ok = false;
        s_offer.clear();
    } else {
        s_save_msg.clear();
    }
}

}  // namespace

void saves_offer(const char *path) {
    offer_file(path);
    s_card_read = false;
}

// ---- menu style RE4 : aides (rmenu.cpp dessine, ici on branche les reglages) ---------------------------------------------
namespace {
nj::rmenu::Skin g_skin;
bool g_skin_built = false;
#ifndef NJ_MOBILE
std::vector<Mode> g_re4_modes;
#endif

// Habillage du jeu (ui_skin) -> textures GL, une fois, des que le fil de fond a fini ou que le cache est lu.
void build_skin_textures() {
    if (g_skin_built || !nj::ui_skin::ready()) return;
    g_skin_built = true;
    auto up = [](int piece, nj::rmenu::Skin::Tex &t) {
        const nj::icons::Image *im = nj::ui_skin::piece(piece);
        if (!im) return;
        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, im->w, im->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, im->rgba.data());
        t.id = (ImTextureID)(intptr_t)id;
        t.w = im->w;
        t.h = im->h;
    };
    namespace us = nj::ui_skin;
    up(us::Bg, g_skin.bg);
    up(us::Frame, g_skin.frame);
    up(us::Row, g_skin.row);
    up(us::Head, g_skin.head);
    up(us::Paper, g_skin.paper);
    up(us::Rack, g_skin.rack);
    up(us::GlyphStick, g_skin.g_stick);
    up(us::GlyphLR, g_skin.g_lr);
    up(us::GlyphOrb, g_skin.g_orb);
    up(us::Scroll, g_skin.scroll);
    up(us::GlyphA, g_skin.g_a);
    up(us::GlyphR, g_skin.g_r);
    up(us::GlyphB, g_skin.g_b);
    up(us::GlyphY, g_skin.g_y);
    up(us::GlyphStart, g_skin.g_start);
    // Police du jeu (atlas de 32 px) : desactivee, trop floue a l'ecran ; le menu garde la serif de l'interface (g_serif).
    constexpr bool kUseGameFont = false;
    if (kUseGameFont) {   // atlas agrandi chez le joueur (rmenu.cpp, GlyphFont::build) puis envoye a la carte graphique
        static nj::rmenu::GlyphFont gf;
        if (const nj::icons::Image *fi = nj::ui_skin::piece(us::FontAtlas))
            if (gf.build(fi->rgba.data(), fi->w, fi->h)) {
                GLuint id = 0;
                glGenTextures(1, &id);
                glBindTexture(GL_TEXTURE_2D, id);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gf.tw, gf.th, 0, GL_RGBA, GL_UNSIGNED_BYTE, gf.pixels.data());
                gf.tex = (ImTextureID)(intptr_t)id;
                gf.pixels.clear();
                gf.pixels.shrink_to_fit();
                g_skin.gfont = &gf;
            }
    }
}

// L'arme de depart du jeu (icone rendue chez le joueur, cache d'icones d'armes) pour l'apercu de visee ; vide sans cache.
const nj::rmenu::pv::Img &gun_img() {
    static nj::rmenu::pv::Img im;
    static bool tried = false;
    if (!tried) {
        tried = true;
        std::vector<unsigned char> px;
        int sz = 0;
        if (nj::runtime::input::touch::weapon_icon_rgba(0x23, px, sz) && sz > 0) {
            GLuint id = 0;
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sz, sz, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            im.id = (ImTextureID)(intptr_t)id;
            im.w = im.h = sz;
        }
    }
    return im;
}

// Atlas tactile (celui que dessine le jeu) pour la maquette des reglages tactiles : decode et envoye au premier besoin.
const nj::rmenu::pv::Atlas *touch_atlas() {
    static nj::rmenu::pv::Atlas at;
    static bool tried = false;
    if (!tried) {
        tried = true;
        std::vector<unsigned char> px;
        int w = 0, h = 0;
        if (nj::runtime::input::touch::overlay_atlas_rgba(px, w, h)) {
            GLuint id = 0;
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            at.id = (ImTextureID)(intptr_t)id;
            at.w = w;
            at.h = h;
            at.style_first = nj::runtime::input::touch::overlay_style_cell(0);
        }
    }
    return &at;
}

// Captures de l'ecran pour l'apercu des reglages : 0 = image du jeu a l'ouverture du menu, 1 = image en direct. GL_RGB : alpha a 1.
GLuint g_shot_id[2] = {0, 0};
void capture_shot(int which, int w, int h) {
    if (w <= 0 || h <= 0) return;
    nj_fbo(GL_FRAMEBUFFER, 0);
    const bool fresh = !g_shot_id[which];
    if (fresh) glGenTextures(1, &g_shot_id[which]);
    glBindTexture(GL_TEXTURE_2D, g_shot_id[which]);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, w, h, 0);
    if (fresh) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    nj::rmenu::Skin::Tex &t = which ? g_skin.shot_live : g_skin.shot_before;
    t.id = (ImTextureID)(intptr_t)g_shot_id[which];
    t.w = w;
    t.h = h;
}
int g_open_frames = 0;

void re4_apply_language(int idx) {
    namespace i18n = nj::runtime::i18n;
    Config &cfg = Config::get();
    cfg.ui_language = idx <= 0 ? "auto" : i18n::code_at(idx - 1);
    i18n::init(cfg.ui_language.c_str());
    if (!nj::runtime::ui::language_font_available()) i18n::init("en");   // pas de police pour cette langue ici
    g_lang_restart = (int)i18n::script() != g_font_script;
}
bool re4_nonzero(int v) { return v != 0; }
bool re4_not30(int v) { return v != 30; }
const char *re4_fps_text() { return "30 FPS"; }
const char *re4_pads_text() {
    static std::string pads;
    pads.clear();
    for (int j = 0, n = SDL_NumJoysticks(); j < n; ++j)
        if (SDL_IsGameController(j)) {
            const char *nm = SDL_GameControllerNameForIndex(j);
            pads += std::string(pads.empty() ? "" : ", ") + (nm ? nm : "?");
        }
    return pads.empty() ? "-" : pads.c_str();
}
#ifndef NJ_MOBILE
void re4_apply_fullscreen(int v) {
    if (g_menu_window) SDL_SetWindowFullscreen(g_menu_window, v ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}
bool re4_windowed() { return g_menu_window && !(SDL_GetWindowFlags(g_menu_window) & SDL_WINDOW_FULLSCREEN_DESKTOP); }
void re4_apply_resolution(int idx) {
    if (!g_menu_window || idx < 0 || idx >= (int)g_re4_modes.size()) return;
    SDL_SetWindowSize(g_menu_window, g_re4_modes[idx].w, g_re4_modes[idx].h);
    SDL_SetWindowPosition(g_menu_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}
#endif
#ifdef NJ_MOBILE
void re4_edit_layout() {
    nj::runtime::input::layout_edit_begin();
    set_open(false);
}
const char *re4_edit_text() { return "Edit"; }
#endif
}  // namespace

// ---- pages du menu style RE4 : Mods, Textures, Saves, Report en lignes de donnees (memes actions que les onglets classiques) ------------
namespace {
namespace rm = nj::rmenu;

rm::Option page_info(const std::string &label, const std::string &text = std::string(), const char *hint = nullptr) {
    rm::Option o;
    o.kind = rm::Kind::Info;
    o.label_str = label;
    if (!text.empty()) o.text_fn = [text] { return text; };
    o.hint = hint;
    return o;
}
rm::Option page_action(const std::string &label, std::function<void()> fn, const char *hint = nullptr) {
    rm::Option o;
    o.kind = rm::Kind::Action;
    o.label_str = label;
    o.text_fn = [] { return std::string(">>"); };
    o.run_fn = std::move(fn);
    o.hint = hint;
    return o;
}
// Texte conserve le temps d'une image (les options ne gardent que des const char *).
const char *keep(const std::string &s) {
    static std::string ring[48];
    static int k = 0;
    ring[k = (k + 1) % 48] = s;
    return ring[k].c_str();
}

void re4_page_mods(rm::Category &c) {
    std::string iname, imsg;
    float frac = 0.0f;
    bool iok = false;
    const bool busy = nj::mods::install_status(iname, frac, imsg, iok);
    if (busy) {
        char b[120];
        std::snprintf(b, sizeof b, TXT("Installing %s..."), iname.c_str());
        const int pct = (int)(frac * 100.0f);
        c.options.push_back(page_info(b, std::to_string(pct) + "%"));
    } else if (!imsg.empty()) {
        c.options.push_back(page_info(imsg));
    }
    const std::vector<nj::mods::Mod> mods = nj::mods::list();
    static std::string confirm_remove;
    static std::vector<int> on;
    on.assign(mods.size(), 0);
    bool files_mod = false;
    if (mods.empty()) c.options.push_back(page_info(TXT("No mods installed.")));
    for (size_t i = 0; i < mods.size(); ++i) {
        const nj::mods::Mod &m = mods[i];
        files_mod |= m.enabled && m.has_files;
        on[i] = m.enabled ? 1 : 0;
        rm::Option o;
        o.kind = rm::Kind::Toggle;
        o.label_str = m.name + (m.version.empty() ? "" : "  v" + m.version);
        o.value = &on[i];
        const std::string id = m.id;
        o.change_fn = [id](int v) { nj::mods::set_enabled(id, v != 0); };
        char mb[32];
        std::snprintf(mb, sizeof mb, TXT("%.0f MB"), (double)m.bytes / 1048576.0);
        o.hint = keep((m.author.empty() ? "" : "by " + m.author + " - ") + m.description + (m.description.empty() ? "" : "  ") + mb);
        if (i > 0) o.links.push_back({"Up", [id] { nj::mods::move(id, -1); }});
        if (i + 1 < mods.size()) o.links.push_back({"Down", [id] { nj::mods::move(id, +1); }});
        if (!m.local) {
            if (confirm_remove == m.id) o.links.push_back({"Sure? Remove", [id] { nj::mods::remove(id); confirm_remove.clear(); }});
            else o.links.push_back({"Remove", [id] { confirm_remove = id; }});
        }
        c.options.push_back(std::move(o));
    }
    const char *note = mods.size() > 1 ? TXT("Higher in the list wins when two mods change the same thing.")
                       : files_mod    ? TXT("Mods that change game files apply at the next start.")
                                      : nullptr;
    if (!busy) {
        for (const std::string &a : nj::mods::archives_in_folder()) {
            const std::string nm = a.substr(a.find_last_of("/\\") + 1);
            char ib[200];
            std::snprintf(ib, sizeof ib, TXT("Install %s"), nm.c_str());
            c.options.push_back(page_action(ib, [a] { nj::mods::install_async(a); }));
        }
#ifdef _WIN32
        c.options.push_back(page_action(TXT("Install mod..."), [] {
            const std::string p = nj::mods::pick_mod_file();
            if (!p.empty()) nj::mods::install_async(p);
        }, TXT("Or drag a .njmod file onto the Nightjar window.")));
        c.options.push_back(page_action(TXT("Open mods folder"), [] {
            std::error_code ec;
            std::filesystem::create_directories(std::filesystem::u8path(nj::mods::mods_dir()), ec);
            SDL_OpenURL(("file:///" + nj::mods::mods_dir()).c_str());
        }));
#else
        static bool picking = false;
        static std::string picked;
        if (picking) {
            std::string path;
            const int r = host::import_poll(path);
            if (r == 1) {
                picked = path;
                nj::mods::install_async(path);
            }
            if (r != 0) picking = false;
        }
        if (!picked.empty() && !nj::mods::install_status(iname, frac, imsg, iok)) {
            std::error_code ec;
            std::filesystem::remove(std::filesystem::u8path(picked), ec);
            picked.clear();
        }
        if (!picking)
            c.options.push_back(page_action(TXT("Install mod..."), [] { picking = true; host::import_begin(); },
                                            keep(std::string(TXT("Choose a .njmod file (for example in Downloads).")))));
#endif
    }
    if (note && !c.options.empty()) c.options.back().hint = note;
}

void re4_page_textures(rm::Category &c) {
    // Un mod de textures vient d'etre installe : textures HD allumees si tout etait sur l'original.
    if (nj::mods::take_new_textures() && !g_hd_textures && !g_hd_weapons && !g_hd_hud && !g_hd_case && !g_hd_items && !g_hd_interface &&
        !g_hd_environment && !g_hd_cutscenes)
        g_hd_textures = g_hd_weapons = g_hd_hud = g_hd_case = g_hd_items = g_hd_interface = g_hd_environment = g_hd_cutscenes = 1;
    int *const all[] = {&g_hd_textures, &g_hd_weapons, &g_hd_hud, &g_hd_case, &g_hd_items, &g_hd_interface, &g_hd_environment, &g_hd_cutscenes};
    c.options.push_back(page_action(TXT("All HD"), [all] { for (int *v : all) *v = 1; }));
    c.options.push_back(page_action(TXT("All original"), [all] { for (int *v : all) *v = 0; }));
    static const char *const hd_names[] = {N_("Original (GameCube)"), N_("HD (from mods)")};
    static const int hd_values[] = {0, 1};
    static const char *const md_names[] = {N_("Original (GameCube)"), N_("RE4 UHD")};
    auto row = [&](const char *label, int &v, const char *const *names) {
        rm::Option o;
        o.label = label;
        o.kind = rm::Kind::Choice;
        o.value = &v;
        o.names = names;
        o.values = hd_values;
        o.n = 2;
        c.options.push_back(o);
    };
    row("Characters", g_hd_textures, hd_names);
    row("Weapons", g_hd_weapons, hd_names);
    row("HUD", g_hd_hud, hd_names);
    row("Attache case", g_hd_case, hd_names);
    row("Items", g_hd_items, hd_names);
    row("Interface", g_hd_interface, hd_names);
    row("Environment", g_hd_environment, hd_names);
    row("Cutscenes", g_hd_cutscenes, hd_names);
    row("Attache case models", g_uhd_case_models, md_names);
    c.options.back().hint = "Case models: applied at the next game start";
}

void re4_page_saves(rm::Category &c) {
    if (!s_card_read) refresh_card();
    const std::string disc = Config::get().game_id.substr(0, 4);
    if (s_importing) {
        std::string path;
        const int r = host::import_poll(path);
        if (r == 1) offer_file(path);
        if (r != 0) s_importing = false;
    }
    if (s_exporting) {
        const int r = host::export_poll();
        if (r != 0) {
            s_exporting = false;
            s_save_msg = r > 0 ? TXT("Memory card saved.") : TXT("Export cancelled.");
            s_save_ok = r > 0;
        }
    }
    int others = 0;
    bool any = false;
    for (const nj::saves::File &f : s_card) {
        if (f.game.rfind("G4B", 0) != 0) { ++others; continue; }
        any = true;
        c.options.push_back(page_info(save_label(f), region_name(f.game) + "  " + save_when(f.modtime)));
    }
    if (!any) c.options.push_back(page_info(s_card_status.empty() ? TXT("No Resident Evil 4 save yet.") : s_card_status));
    if (others) { char b[80]; std::snprintf(b, sizeof b, TXT("%d other file(s) on the card."), others); c.options.push_back(page_info(b)); }
    if (const int n = nj::saves::pending_count()) { char b[120]; std::snprintf(b, sizeof b, TXT("%d file(s) will be imported the next time you start Nightjar."), n); c.options.push_back(page_info(b)); }
    if (!s_offer.empty()) {
        { char fb[240]; std::snprintf(fb, sizeof fb, TXT("Found in %s:"), s_offer_from.c_str()); c.options.push_back(page_info(fb)); }
        bool other_region = false;
        for (const nj::saves::File &f : s_offer) {
            c.options.push_back(page_info(save_label(f), region_name(f.game) + "  " + save_when(f.modtime)));
            other_region |= f.game.size() >= 4 && disc.size() >= 4 && f.game[3] != disc[3];
        }
        static int conv = 1;
        conv = s_convert ? 1 : 0;
        if (other_region) {
            rm::Option o;
            char cb[200];
            std::snprintf(cb, sizeof cb, TXT("Convert to my disc's region (%s), experimental"), region_name(disc).c_str());
            o.kind = rm::Kind::Toggle;
            o.label_str = cb;
            o.value = &conv;
            o.change_fn = [](int v) { s_convert = v != 0; };
            c.options.push_back(std::move(o));
        }
        c.options.push_back(page_action(TXT("Import"), [disc] {
            std::vector<nj::saves::File> files;
            for (nj::saves::File f : s_offer) {
                const bool other = f.game.size() >= 4 && disc.size() >= 4 && f.game[3] != disc[3];
                if (other && s_convert) {
                    if (f.name == "bh4_system") continue;
                    nj::saves::set_region(f, disc);
                }
                files.push_back(std::move(f));
            }
            std::string err;
            if (files.empty()) { s_save_msg = TXT("Nothing to import."); s_save_ok = false; }
            else if (nj::saves::queue_import(files, err)) {
                s_save_msg = TXT("Ready: restart Nightjar to load the imported save(s).");
                s_save_ok = true;
                nj::report::add("Saves: " + std::to_string(files.size()) + " file(s) queued for import");
            } else { s_save_msg = tr_fmt(N_("Import: %s."), err); s_save_ok = false; }
            s_offer.clear();
        }, TXT("A file with the same name is replaced. Your memory card is backed up first.")));
        c.options.push_back(page_action(TXT("Cancel"), [] { s_offer.clear(); }));
    }
    if (!s_importing && !s_exporting) {
        c.options.push_back(page_action(TXT("Import save..."), [] { s_importing = true; host::import_begin(); },
#ifdef NJ_MOBILE
                                        TXT("Export your saves before uninstalling Nightjar: uninstalling deletes them.")));
#else
                                        TXT("You can also drag a .gci or .raw save onto the Nightjar window.")));
#endif
        c.options.push_back(page_action(TXT("Export saves"), [] {
#ifdef NJ_MOBILE
            char name[64];
            const std::time_t t = std::time(nullptr);
            std::strftime(name, sizeof name, "Nightjar-saves-%Y%m%d.raw", std::localtime(&t));
            s_exporting = true;
            host::export_begin(nj::saves::card_path(), name);
#else
            std::string folder, err;
            if (nj::saves::export_all(folder, err)) {
                host::open_folder(folder);
                s_save_msg = TXT("Exported (.gci files for Dolphin + the whole memory card).");
                s_save_ok = true;
            } else { s_save_msg = tr_fmt(N_("Export: %s."), err); s_save_ok = false; }
#endif
        }, TXT("Your memory card is backed up at each start (last 5 kept).")));
        c.options.push_back(page_action(TXT("Refresh"), [] { refresh_card(); }));
#ifndef NJ_MOBILE
        static std::vector<std::string> dolphin;
        static bool dolphin_open = false;
        c.options.push_back(page_action(TXT("Import from Dolphin"), [] { dolphin = nj::saves::dolphin_candidates(); dolphin_open = true; }));
        c.options.push_back(page_action(TXT("Open backups folder"), [] { host::open_folder(nj::saves::backups_dir()); }));
        if (dolphin_open) {
            if (dolphin.empty()) c.options.push_back(page_info(TXT("No Dolphin save found. Use \"Import save...\" and pick the .gci or .raw file.")));
            for (const std::string &p : dolphin) {
                const std::filesystem::path fp = std::filesystem::u8path(p);
                c.options.push_back(page_action(u8s(fp.parent_path().filename()) + " / " + u8s(fp.filename()), [p] { offer_file(p); dolphin_open = false; }));
            }
        }
#endif
    }
    if (!s_save_msg.empty()) c.options.push_back(page_info(s_save_msg));
}

void re4_page_report(rm::Category &c) {
    static nj::report::Text rep;
    static auto last = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    static std::string msg;
    const auto now = std::chrono::steady_clock::now();
    if (now - last > std::chrono::seconds(1)) {
        last = now;
        std::vector<std::string> st;
        char b[96];
        std::snprintf(b, sizeof b, "render %dx%d", efb_w(), efb_h());
        st.push_back(b);
        std::snprintf(b, sizeof b, "aniso %dx", g_aniso);
        st.push_back(b);
        std::snprintf(b, sizeof b, "fps %s%s", g_fps_target ? std::to_string(g_fps_target).c_str() : "uncapped", g_fps_target == 30 ? "" : " (test)");
        st.push_back(b);
        st.push_back(std::string("vsync ") + (g_vsync ? "on" : "off"));
        st.push_back(std::string("aspect ") + (g_aspect ? "4:3" : "16:9"));
        st.push_back("disc " + Config::get().game_id);
        std::string mods;
        for (const nj::mods::Mod &m : nj::mods::list())
            if (m.enabled) mods += (mods.empty() ? "" : ", ") + m.name;
        st.push_back("mods " + (mods.empty() ? std::string("none") : mods));
        rep = nj::report::build(st);
    }
    if (!rep.crash.empty()) { char b[300]; std::snprintf(b, sizeof b, TXT("Last session crashed: %s"), rep.crash.c_str()); c.options.push_back(page_info(b)); }
    c.options.push_back(page_action(TXT("Copy short report"), [&] { SDL_SetClipboardText(rep.brief.c_str()); msg = TXT("Copied: paste it in #re4-native-help."); },
                                    TXT("Having a problem? Send this report in #re4-native-help.")));
    c.options.push_back(page_action(TXT("Copy full report"), [&] { SDL_SetClipboardText(rep.full.c_str()); msg = TXT("Copied (long reports become a .txt file in Discord)."); },
                                    TXT("No personal data: no Discord name, no password. The attached journal is encrypted.")));
#ifdef NJ_MOBILE
    c.options.push_back(page_action(TXT("Share"), [&] { msg = host::share(rep.full, nj::report::attachments()) ? "" : "Can't open the share menu."; }));
#else
    c.options.push_back(page_action(TXT("Open folder"), [&] { host::open_folder(nj::report::folder()); msg = TXT("Attach plantage.txt and the newest file in journal/ to your message."); }));
#endif
    c.options.push_back(page_action(TXT("Invite a friend"), [&] {
        const std::string inv = "Nightjar: Resident Evil 4 on PC and Android (you need your own game disc). Join the Discord: https://discord.gg/AXAzExECSv";
#ifdef NJ_MOBILE
        msg = host::share(inv, std::vector<std::string>{}) ? "" : "Can't open the share menu.";
#else
        SDL_SetClipboardText(inv.c_str());
        msg = TXT("Invitation copied: paste it to your friend.");
#endif
    }));
    if (!msg.empty()) c.options.push_back(page_info(msg));
    std::istringstream is(rep.brief);
    std::string line;
    int n = 0;
    while (std::getline(is, line) && n++ < 8) c.options.push_back(page_info(line));
}
}  // namespace

void menu_frame(SDL_Window *window) {
    if (!g_menu_open) {
        g_open_frames = 0;
        return;
    }
    ++g_open_frames;
    if (!g_ctx) init_imgui(window);
    g_menu_window = window;
    ImGuiContext *prev = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(g_ctx);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    const ImGuiIO &io = ImGui::GetIO();
    // Centre a la premiere ouverture, puis deplacable a la souris (barre de titre) : garde sa place
    // d'une ouverture a l'autre pendant la partie.
    // 19/10 : un vrai menu plutot qu'une petite fenetre. Telephone : tout l'ecran ; PC : 70 % x 88 % de la fenetre, centre. Le contenu
    // defile ; les libelles et les listes prennent la largeur disponible (label_w, combo_w plus bas).
#ifdef NJ_MOBILE
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    const ImGuiWindowFlags win_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoSavedSettings;
#else
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::max(io.DisplaySize.x * 0.7f, std::min(io.DisplaySize.x, ImGui::GetFontSize() * 40.0f)),
                                    io.DisplaySize.y * 0.88f),
                             ImGuiCond_Always);
    const ImGuiWindowFlags win_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoSavedSettings;
#endif
    const bool re4 = true;   // le menu style RE4 (rmenu.cpp) est le seul menu
    if (re4) {
        nj::ui_skin::poll();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(io.DisplaySize);
    }
    ImGui::SetNextWindowBgAlpha(0.97f);
    char wtitle[96];   // ### : l'identite de la fenetre ne change pas avec la langue
    std::snprintf(wtitle, sizeof wtitle, "NIGHTJAR  -  %s###nightjar_settings", TXT("Settings"));
    if (re4) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    }
    ImGui::Begin(wtitle, nullptr, re4 ? (win_flags | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse) : win_flags);
    if (re4) ImGui::PopStyleVar(2);

    int dw = 0, dh = 0, ww = 0, wh = 0;
    SDL_GL_GetDrawableSize(window, &dw, &dh);
    SDL_GetWindowSize(window, &ww, &wh);
    const bool fs = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP;
    const float avail_w = ImGui::GetContentRegionAvail().x;


    if (re4) {
        namespace rm = nj::rmenu;
        namespace i18n = nj::runtime::i18n;
        build_skin_textures();
        static rm::State rst;
        g_skin.font = g_serif;
        if (g_open_frames == 1) { rst.page = -1; rst.row = 0; rst.scroll = 0.0f; rst.velocity = 0.0f; }
        g_skin.touch = false;
#ifdef NJ_MOBILE
        g_skin.touch = SDL_NumJoysticks() == 0;
#endif
        if (g_open_frames == 1) {   // image du jeu a l'ouverture : le « avant » de l'apercu
            capture_shot(0, dw, dh);
            capture_shot(1, dw, dh);
        } else if (rst.preview_active && g_open_frames % 5 == 0) {
            capture_shot(1, dw, dh);
        }
        // miroirs entiers des booleens (rmenu pilote des int)
        static int m_fps = 0, m_vsync = 0, m_fs = 0, m_lang = 0, m_res = 0;
        m_fps = g_show_fps ? 1 : 0;
        m_vsync = g_vsync ? 1 : 0;
        m_fs = fs ? 1 : 0;
        static std::vector<const char *> lang_names;   // langue de l'interface
        lang_names.assign(1, "Automatic (device language)");
        {
            int cur = 0;
            for (int i = 0; i < i18n::count(); ++i) {
                lang_names.push_back(i18n::native_name_at(i));
                if (Config::get().ui_language == i18n::code_at(i)) cur = i + 1;
            }
            m_lang = cur;
        }
        static std::vector<std::string> res_labels;
        static std::vector<const char *> res_names;
        res_labels.clear();
        res_names.clear();
        std::vector<rm::Category> cats;
        auto add = [](rm::Category &c, rm::Option o) { c.options.push_back(o); };
        auto choice = [&](rm::Category &c, const char *label, int &v, const char *const *names, const int *values, int n, const char *hint) {
            rm::Option o;
            o.label = label; o.kind = rm::Kind::Choice; o.value = &v; o.names = names; o.values = values; o.n = n; o.hint = hint;
            add(c, o);
        };
        auto slider = [&](rm::Category &c, const char *label, int &v, int lo, int hi, int step, const char *hint) {
            rm::Option o;
            o.label = label; o.kind = rm::Kind::Slider; o.value = &v; o.lo = lo; o.hi = hi; o.step = step; o.hint = hint;
            add(c, o);
        };
        (void)slider;
        // ---------------- Display ----------------
        {
            rm::Category c;
            c.label = "Display";
            c.glyph = rm::kGlyphScreen;
            {
                rm::Option o;
                o.label = "Language"; o.kind = rm::Kind::Choice; o.value = &m_lang; o.names = lang_names.data(); o.n = (int)lang_names.size();
                o.on_change = re4_apply_language;
                o.hint = "The language of the menus. Changing between languages with the same alphabet is immediate.";
                add(c, o);
            }
            {
                rm::Option o;
                o.label = "Show FPS & Refresh Rate"; o.kind = rm::Kind::Toggle; o.value = &m_fps;
                o.hint = "Shows the frame rate in a corner of the screen.";
                add(c, o);
            }
#ifndef NJ_MOBILE
            {
                static const char *const dm_names[] = {N_("Windowed"), N_("Fullscreen")};
                choice(c, "Display mode", m_fs, dm_names, nullptr, 2, "Window or full screen.");
                c.options.back().on_change = re4_apply_fullscreen;
                g_re4_modes = window_modes(window);
                int cur = 0;
                for (size_t i = 0; i < g_re4_modes.size(); ++i) {
                    char b[40];
                    std::snprintf(b, sizeof b, "%d x %d", g_re4_modes[i].w, g_re4_modes[i].h);
                    res_labels.push_back(b);
                    if (g_re4_modes[i].w == ww && g_re4_modes[i].h == wh) cur = (int)i;
                }
                for (auto &l : res_labels) res_names.push_back(l.c_str());
                m_res = cur;
                rm::Option o;
                o.label = "Resolution"; o.kind = rm::Kind::Choice; o.value = &m_res; o.names = res_names.data(); o.n = (int)res_names.size();
                o.enabled = re4_windowed; o.on_change = re4_apply_resolution;
                o.hint = "Size of the window (in full screen, the desktop's).";
                if (o.n > 0) add(c, o);
            }
#endif
            {
                static std::vector<std::string> ir_labels;
                static std::vector<const char *> ir_names;
                ir_labels.clear();
                ir_names.clear();
                for (int sIdx = 0; sIdx <= kMaxScale; ++sIdx) {
                    char b[96];
                    if (sIdx == 0) std::snprintf(b, sizeof b, "Native (%dx, %dx%d)", native_scale(dw, dh), kEfbW * native_scale(dw, dh), kEfbH * native_scale(dw, dh));
                    else std::snprintf(b, sizeof b, "%dx (%dx%d)%s", sIdx, kEfbW * sIdx, kEfbH * sIdx, sIdx == 1 ? " GameCube" : "");
                    ir_labels.push_back(b);
                }
                for (auto &l : ir_labels) ir_names.push_back(l.c_str());
                choice(c, "Internal resolution", g_scale_setting, ir_names.data(), nullptr, (int)ir_names.size(),
                       "How sharp the 3D picture is drawn. Higher is sharper and slower; Native matches your screen.");
            }
            {
                static const char *const fps_names[] = {N_("30 (as the game)"), N_("60 (in-between pictures)"), N_("120 (test)"), N_("Uncapped (test)")};
                static const int fps_values[] = {30, 60, 120, 0};
                choice(c, "Framerate", g_fps_target, fps_names, fps_values, 4,
                       "The game's logic always runs at 30 per second. Above 30, the 3D scene gets in-between pictures; it only holds on a device that already keeps 30.");
                c.options.back().warn = "Experimental: if the game slows down, go back to 30.";
                c.options.back().warn_when = re4_not30;
            }
            {
                rm::Option o;
                o.label = "V-Sync"; o.kind = rm::Kind::Toggle; o.value = &m_vsync;
                o.hint = "Waits for the screen's refresh. Removes tearing; may add a little delay.";
                add(c, o);
            }
            cats.push_back(std::move(c));
        }
        {   // menu titre de Nightjar (18/11) : visibles dans Affichage ; effet au prochain lancement du jeu
            for (auto &cc : cats) {
                if (std::strcmp(cc.label, "Display") != 0) continue;
                static const char *const tm_names[] = {N_("Original (GameCube)"), N_("Nightjar")};
                static const int tm_values[] = {0, 1};
                rm::Option o;
                o.label = "Title menu"; o.kind = rm::Kind::Choice; o.value = &g_title_menu; o.names = tm_names; o.values = tm_values; o.n = 2;
                o.hint = "Nightjar: Continue in one tap, saves with a picture of where you were. Original: the game's own title menu. Takes effect on the next launch.";
                cc.options.push_back(o);
                static const char *const ts_names[] = {N_("Live scene"), N_("Picture")};
                static const int ts_values[] = {1, 0};
                rm::Option b;
                b.label = "Title background"; b.kind = rm::Kind::Choice; b.value = &g_title_scene; b.names = ts_names; b.values = ts_values; b.n = 2;
                b.hint = "Live scene: the game itself plays behind the title menu (the village path in the fog). Picture: the title image. Takes effect on the next launch.";
                b.enabled = [] { return g_title_menu != 0; };
                cc.options.push_back(b);
            }
        }
        // ---------------- Graphics ----------------
        {
            rm::Category c;
            c.label = "Graphics";
            c.glyph = rm::kGlyphEye;
            static const char *const rs_names[] = {N_("Bilinear"), N_("Area")};
            static const int rs_values[] = {0, 1};
            choice(c, "Downscaling", g_resample, rs_names, rs_values, 2, "Area keeps thin lines steady when the picture is shrunk.");
            static const char *const an_names[] = {N_("Off"), N_("2x"), N_("4x"), N_("8x"), N_("16x")};
            static const int an_values[] = {1, 2, 4, 8, 16};
            choice(c, "Anisotropic filtering", g_aniso, an_names, an_values, 5, "Keeps floors and walls sharp at a slant. Costs a little speed on weak GPUs.");
            static const char *const fog_names[] = {N_("Off"), N_("On")};
            static const int fog_values[] = {0, 1};
            choice(c, "Disable fog", g_no_fog, fog_names, fog_values, 2, "Fog is part of the game's look; turning it off is faster on very old phones.");
            c.options.back().warn = "Not recommended: the game no longer looks like the original. Only for very old phones.";
            c.options.back().warn_when = re4_nonzero;
            static const char *const as_names[] = {N_("16:9 (Wii Edition)"), N_("4:3 (GameCube)")};
            static const int as_values[] = {0, 1};
            choice(c, "Aspect ratio", g_aspect, as_names, as_values, 2, "16:9 shows the widescreen picture; 4:3 is the original.");
            static const char *const sf_names[] = {N_("Automatic"), N_("Fit (black bars)"), N_("Fill (crop)"), N_("Stretch")};
            static const int sf_values[] = {0, 1, 2, 3};
            choice(c, "Screen fit", g_screen_fit, sf_names, sf_values, 4,
                   "Automatic: keeps the picture's proportions and stretches it slightly on wider screens. Fit: black bars. Fill: crops the edges. Stretch: fills the screen.");
            static const char *const sr_names[] = {N_("Game default"), N_("4:3"), N_("3:2"), N_("16:10"), N_("16:9"), N_("18:9"), N_("19.5:9"), N_("20:9"), N_("21:9")};
            static const int sr_values[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
            choice(c, "Picture ratio", g_screen_ratio, sr_names, sr_values, 9, "Shows the picture at this ratio, fitted inside your screen. Use it to match an ultrawide or unusual screen.");
            static const char *const cm_names[] = {N_("Normal"), N_("Compatibility (slower)")};
            static const int cm_values[] = {0, 13};
            choice(c, "Rendering mode", g_compat_mask, cm_names, cm_values, 2, "If some surfaces look broken, try Compatibility, then restart the game. It can be slower.");
            cats.push_back(std::move(c));
        }
        // ---------------- Controls ----------------
        {
            rm::Category c;
            c.label = "Controls";
            c.glyph = rm::kGlyphPad;
            static const char *const bt_names[] = {N_("GameCube"), N_("Xbox"), N_("Keyboard / mouse"), N_("Mobile (touch)")};
            static const int bt_values[] = {0, 1, 3, 4};
            choice(c, "Button icons", g_button_style, bt_names, bt_values, 4, "Which buttons the game shows in its messages (\"Press A\"...).");
#ifdef NJ_MOBILE
            static const char *const ts_names[] = {N_("Stone (RE4 HUD)"), N_("Gold (Nightjar)"), N_("Classic")};
            static const int ts_values[] = {1, 2, 0};
            choice(c, "Button look", g_touch_style, ts_names, ts_values, 3, "Look of the on-screen buttons.");
            slider(c, "Opacity", g_touch_opacity, 20, 100, 5, "Lower = you see more of the game behind the buttons.");
            static const char *const ta_names[] = {N_("Always shown"), N_("Only when needed")};
            static const int ta_values[] = {0, 1};
            choice(c, "Action button", g_touch_action, ta_names, ta_values, 2, "Only when needed: the hand button appears when there is something to do (door, ladder, kick...).");
            static const char *const tsm_names[] = {N_("Simple (3 buttons)"), N_("Full")};
            static const int tsm_values[] = {1, 0};
            choice(c, "Layout", g_touch_simple, tsm_names, tsm_values, 2, "Simple: action, aim and knife. Push the stick all the way to run; pull it all the way back to turn around. Full: run and turn-around buttons.");
            static const char *const tl_names[] = {N_("One tap"), N_("Double tap")};
            static const int tl_values[] = {0, 1};
            choice(c, "Lock style", g_touch_lock, tl_names, tl_values, 2, "Run, aim and knife stay on once locked; touch them again to release. One tap: a quick tap locks. Double tap: two quick taps in a row.");
            static const char *const tc_names[] = {N_("Automatic"), N_("Always shown"), N_("Hidden")};
            static const int tc_values[] = {0, 1, 2};
            choice(c, "Touch controls", g_touch_controls, tc_names, tc_values, 3, "Automatic: hidden while a controller is being used, back as soon as you touch the screen. Hidden: only the settings gear stays.");
            static const char *const tan_names[] = {N_("Only when used"), N_("Always"), N_("Off")};
            static const int tan_values[] = {2, 1, 0};
            choice(c, "Button animation", g_touch_anim, tan_names, tan_values, 3, "Only when used: an icon moves while you hold it, or while it is toggled on (run, aim, knife) or has something to do (action).");
            slider(c, "Button size", g_touch_size, 70, 130, 5, "Smaller buttons leave more of the screen (and the health gauge) visible.");
            {
                rm::Option o;
                o.label = "Edit touch layout"; o.kind = rm::Kind::Action; o.run = re4_edit_layout; o.text = re4_edit_text;
                o.hint = "Move and resize each button: drag it, use + and - to resize, the dots button turns the grid on or off, the cross resets, the check saves.";
                add(c, o);
            }
#endif
            static const char *const hp_names[] = {N_("Off"), N_("Light"), N_("Strong")};
            static const int hp_values[] = {0, 1, 2};
#ifdef NJ_MOBILE
            choice(c, "Vibration", g_haptics, hp_names, hp_values, 3, "Damage, shots and explosions shake the controller and the phone. Touch buttons give a short tap.");
#else
            choice(c, "Vibration", g_haptics, hp_names, hp_values, 3, "Damage, shots and explosions shake the controller, like on GameCube.");
#endif
            {
                rm::Option o;
                o.label = "Gamepad"; o.kind = rm::Kind::Info; o.text = re4_pads_text;
                o.hint = "Connect a controller (USB or Bluetooth) at any time.";
                add(c, o);
            }
            static const char *const cl_names[] = {
                N_("GameCube (original)"),
                N_("Type I (RE4 UHD)"),
                N_("Type II (RE4 UHD)"),
                N_("Type III (RE4 UHD)"),
                N_("GameCube faithful (PS/Xbox)")
            };
            static const int cl_values[] = {0, 1, 2, 3, 4};
            choice(c, "Button mapping", g_controller_layout, cl_names, cl_values, 5, "GameCube: buttons as on the original pad. Type I/II/III: the layouts of the PC version. GameCube faithful: DualShock/Xbox with original GameCube logic.");
            static const char *const aim_names[] = {N_("Wii Edition (pointer)"), N_("GameCube (stick)"), N_("PC (RE4 UHD mouse)")};
            static const int aim_values[] = {0, 1, 2};
            choice(c, "Aim with", g_aim_mode, aim_names, aim_values, 3, "Wii: the crosshair moves freely on screen. GameCube: the stick moves the laser, like the original. PC: mouse aiming.");
            if (g_aim_mode == 1) {
                static const char *const sa_names[] = {N_("Original"), N_("Fast"), N_("Very fast")};
                static const int sa_values[] = {0, 1, 2};
                choice(c, "Stick aim speed", g_stick_aim, sa_names, sa_values, 3, "Fast: precise near the center, up to twice as fast when the stick is pushed all the way.");
            }
            if (g_aim_mode == 2) {
                static const char *const cam_names[] = {N_("Locked to aim (Modern)"), N_("Detached (Classic)")};
                static const int cam_values[] = {0, 1};
                choice(c, "Aim camera", g_aim_detach, cam_names, cam_values, 2, "Modern: the camera follows the crosshair. Classic: the camera stays still.");
            }
            static const char *const dbg_names[] = {N_("Off"), N_("On (next launch)")};
            static const int dbg_values[] = {0, 1};
            choice(c, "Debug mode", g_debug_mode, dbg_names, dbg_values, 2, "Debug disc only: keeps the developer menu (stage, room, enemies) and the debug display. Takes effect on the next launch. Retail discs have no debug menu.");
            cats.push_back(std::move(c));
        }
        // ---------------- options du jeu (celles de son menu OPTIONS) ----------------
        {
            auto pending = [](int set, int cur) { return set >= 0 ? set : (cur >= 0 ? cur : 0); };
            static int gi_rev = 0, gi_knife = 0, gi_bright = 0;
            gi_rev = pending(g_game_set_reverse, g_game_cur_reverse);
            gi_knife = pending(g_game_set_knife, g_game_cur_knife);
            gi_bright = pending(g_game_set_brightness, g_game_cur_brightness) - 64;
            static auto game_ready = [] { return g_game_cur_brightness >= 0; };
            for (auto &cc : cats) {
                if (std::strcmp(cc.label, "Controls") != 0) continue;
                rm::Option o;
                o.label = "Inverted aim"; o.kind = rm::Kind::Toggle; o.value = &gi_rev; o.enabled = game_ready;
                o.on_change = [](int v) { g_game_set_reverse = v; };
                o.hint = "Inverts the vertical aiming axis, like the game's own options.";
                cc.options.push_back(o);
                rm::Option k;
                k.label = "Quick knife"; k.kind = rm::Kind::Toggle; k.value = &gi_knife; k.enabled = game_ready;
                k.on_change = [](int v) { g_game_set_knife = v; };
                k.hint = "The knife button draws the knife at once, like the game's own options.";
                cc.options.push_back(k);
                break;
            }
            {   // AUDIO SETUP du jeu : le jeu change lui-meme de mode (SndSetOutputMode)
                rm::Category c;
                c.label = "Audio setup";
                c.glyph = rm::kGlyphScreen;
                static int gi_snd = 1;
                gi_snd = g_game_set_snd >= 0 ? g_game_set_snd : (g_game_cur_snd >= 0 && g_game_cur_snd <= 2 ? g_game_cur_snd : 1);
                static const char *const sn_names[] = {N_("Stereo"), N_("Monaural"), N_("Dolby Surround Pro Logic II")};
                static const int sn_values[] = {1, 0, 2};
                rm::Option o;
                o.label = "Sound"; o.kind = rm::Kind::Choice; o.value = &gi_snd; o.names = sn_names; o.values = sn_values; o.n = 3; o.enabled = game_ready;
                o.on_change = [](int v) { g_game_set_snd = v; };
                o.hint = "The game's audio output mode. Dolby Surround Pro Logic II is the game's surround mode.";
                c.options.push_back(o);
                cats.push_back(std::move(c));
            }
            {
                rm::Category c;
                c.label = "Brightness adjust";
                c.glyph = rm::kGlyphEye;
                rm::Option o;
                o.label = "Brightness"; o.kind = rm::Kind::Slider; o.value = &gi_bright; o.lo = -30; o.hi = 50; o.step = 1; o.enabled = game_ready;
                o.on_change = [](int v) { g_game_set_brightness = 64 + v; };
                o.hint = "The game's brightness, from -30 to +50 around its default (0). Applies at once.";
                o.preview = 1;
                c.options.push_back(o);
                cats.push_back(std::move(c));
            }
            {
                rm::Category c;
                c.label = "Retry or load";
                c.glyph = rm::kGlyphDisk;
                static double reset_armed = 0.0;
                rm::Option o;
                o.label = "Game reset"; o.kind = rm::Kind::Action; o.enabled = game_ready;
                o.text_fn = [] { return std::string(ImGui::GetTime() < reset_armed ? TXT("Tap again to confirm") : TXT("Run")); };
                o.run_fn = [] {
                    if (ImGui::GetTime() < reset_armed) { g_game_set_reset = 1; reset_armed = 0.0; }
                    else reset_armed = ImGui::GetTime() + 4.0;
                };
                o.hint = "Returns to the title screen, as the game's own GAME RESET. Progress since the last save is lost.";
                c.options.push_back(o);
                // Retry et Load : on ouvre le menu OPTIONS du jeu sur l'entree voulue ; le jeu demande lui-meme sa confirmation
                rm::Option rt;
                rt.label = "Retry from a continue point"; rt.kind = rm::Kind::Action;
                rt.enabled = [] { return g_game_cur_retry_ok == 1; };
                rt.text = [] { return "Open"; };
                rt.run_fn = [] { g_game_set_menu = 1; };
                rt.hint = "Opens the game's own menu on this entry: the game asks you to confirm. Close this menu to see it.";
                c.options.insert(c.options.begin(), rt);
                rm::Option ld;
                ld.label = "Load game"; ld.kind = rm::Kind::Action; ld.enabled = game_ready;
                ld.text = [] { return "Open"; };
                ld.run_fn = [] { g_game_set_menu = 2; };
                ld.hint = "Opens the game's own memory card screen to load a save. Close this menu to see it.";
                c.options.insert(c.options.begin() + 1, ld);
                cats.push_back(std::move(c));
            }
        }
        for (auto &cc : cats)
            for (auto &oo : cc.options)
                if (std::strcmp(cc.label, "Graphics") == 0 || std::strcmp(oo.label, "Internal resolution") == 0) oo.preview = 1;
        {   // apercus dessines (previews.cpp) : un par reglage dont l'effet se voit
            namespace pv = nj::rmenu::pv;
            auto ctx = [&](ImDrawList *dl, ImVec2 a, ImVec2 b, float dpx) {
                pv::Ctx k;
                k.dl = dl; k.a = a; k.b = b; k.dp = dpx; k.font = g_serif;
                k.tr = [](const char *t) { return TXT(t); };
                k.cream = g_skin.cream_hi; k.dim = g_skin.dim_text; k.red = g_skin.red; k.line = g_skin.line; k.panel = g_skin.panel_col;
                k.shot = g_skin.shot_before.id;
                k.atlas = touch_atlas();
                static pv::IconSet is;
                is.stick.id = g_skin.g_stick.id; is.stick.w = g_skin.g_stick.w; is.stick.h = g_skin.g_stick.h;
                is.gun = gun_img();
                k.icons = &is;
                k.t = ImGui::GetTime();
                return k;
            };
            auto attach = [&](const char *cat, const char *opt, float hgt, std::function<void(const pv::Ctx &)> fn) {
                for (auto &cc : cats)
                    if (std::strcmp(cc.label, cat) == 0)
                        for (auto &oo : cc.options)
                            if (std::strcmp(oo.label, opt) == 0) {
                                oo.preview = 0;
                                oo.preview_h = hgt;
                                oo.preview_fn = [ctx, fn](ImDrawList *dl, ImVec2 a, ImVec2 b, float dpx) { fn(ctx(dl, a, b, dpx)); };
                            }
            };
            for (auto &cc : cats)
                if (std::strcmp(cc.label, "Controls") == 0)
                    for (auto &oo : cc.options)
                        if (std::strcmp(oo.label, "Button mapping") == 0)
                            oo.table_fn = [] {   // touches -> actions (Key_type_tbl du jeu, hle/pad.cpp)
                                using R = std::vector<std::pair<std::string, std::string>>;
                                if (g_controller_layout == 0)
                                    return R{{"A", "Action / Fire"}, {"B", "Run"}, {"X", "Call Ashley"}, {"Y", "Inventory"}, {"L", "Knife"}, {"R", "Aim"}, {"Z", "Map"}, {"Start", "Pause"}};
                                if (g_controller_layout == 1)
                                    return R{{"RT", "Aim"}, {"A", "Action / Fire"}, {"X", "Run"}, {"LT", "Knife"}, {"LB", "Lock on"}, {"RB", "Call Ashley"}, {"Y", "Map"}, {"Start", "Inventory"}, {"Back", "Options"}, {"B", "Cancel"}};
                                if (g_controller_layout == 4)
                                    return R{{"Cross / A", "Action / Fire"}, {"Square / X", "Run / 180° turn"}, {"Circle / B", "Call Ashley"}, {"Triangle / Y", "Inventory"}, {"L2 / LT", "Knife"}, {"R2 / RT", "Aim"}, {"R1 / RB", "Map (Z)"}, {"Options / Start", "Pause / Skip"}};
                                return R{{"LT", "Aim"}, {"RT", "Fire"}, {"A", "Action"}, {"X", "Run / Turn"}, {"LB", "Knife"}, {"RB", "Call Ashley"}, {"Y", "Map"}, {"Start", "Inventory"}, {"Back", "Options"}, {"B", "Cancel"}};
                            };
            attach("Controls", "Button icons", 0.62f, [](const pv::Ctx &k) {
                auto im = [](const nj::rmenu::Skin::Tex &t) { pv::Img i; i.id = t.id; i.w = t.w; i.h = t.h; return i; };
                pv::IconSet s;
                s.stick = im(g_skin.g_stick); s.a = im(g_skin.g_a); s.b = im(g_skin.g_b); s.x = im(g_skin.g_orb); s.y = im(g_skin.g_y);
                s.l = im(g_skin.g_lr); s.r = im(g_skin.g_r); s.start = im(g_skin.g_start);
                pv::button_icons(k, s, g_button_style);
            });
            for (const char *o : {"Button look", "Opacity", "Action button", "Layout", "Button size", "Lock style", "Touch controls", "Button animation"})
                attach("Controls", o, 0.5625f, [](const pv::Ctx &k) { pv::touch_buttons(k, g_touch_style, g_touch_opacity, g_touch_simple, g_touch_size, g_touch_action == 0); });
            attach("Controls", "Aim with", 0.62f, [](const pv::Ctx &k) { pv::aim_demo(k, g_aim_mode, g_stick_aim, 0, 0); });
            attach("Controls", "Stick aim speed", 0.62f, [](const pv::Ctx &k) { pv::aim_demo(k, 1, g_stick_aim, 1, 0); });
            attach("Controls", "Aim camera", 0.62f, [](const pv::Ctx &k) { pv::aim_demo(k, 2, 0, 2, g_aim_detach); });
            attach("Controls", "Vibration", 0.5f, [](const pv::Ctx &k) { pv::vibration(k, g_haptics); });
            const float sw = (float)dw, sh = (float)dh;
            for (const char *o : {"Aspect ratio", "Screen fit", "Picture ratio"})
                attach("Graphics", o, 0.7f, [sw, sh](const pv::Ctx &k) { pv::screen_fit(k, sw, sh, g_aspect, g_screen_fit, g_screen_ratio); });
            attach("Display", "Show FPS & Refresh Rate", 0.625f, [](const pv::Ctx &k) { pv::fps_corner(k, g_show_fps ? 1 : 0); });
        }
        // ---------------- categories libres (contenu ImGui dans le cadre RE4) ----------------
        {
            rm::Category c;
            c.label = "Mods";
            c.glyph = rm::kGlyphBox;
            re4_page_mods(c);
            cats.push_back(std::move(c));
        }
        if (!nj::mods::index()->textures.empty()) {
            rm::Category c;
            c.label = "Textures";
            c.glyph = rm::kGlyphBox;
            re4_page_textures(c);
            cats.push_back(std::move(c));
        }
        {
            rm::Category c;
            c.label = "Saves";
            c.glyph = rm::kGlyphDisk;
            re4_page_saves(c);
            cats.push_back(std::move(c));
        }
        {
            rm::Category c;
            c.label = "Report";
            c.glyph = rm::kGlyphFlag;
            re4_page_report(c);
            cats.push_back(std::move(c));
        }

        {   // l'ordre du menu OPTIONS du jeu (RETRY or LOAD, CONTROLLER SETUP, BRIGHTNESS ADJUST, AUDIO SETUP), puis ce qui est propre a Nightjar
            static const char *const order[] = {"Retry or load", "Controls", "Brightness adjust", "Audio setup", "Display", "Graphics", "Mods", "Textures", "Saves", "Report"};
            std::vector<rm::Category> sorted;
            for (const char *name : order)
                for (auto &cc : cats)
                    if (std::strcmp(cc.label, name) == 0) { sorted.push_back(std::move(cc)); break; }
            for (auto &cc : cats)   // categories imprevues : a la fin
                if (cc.label) {
                    bool known = false;
                    for (const char *name : order) known = known || std::strcmp(cc.label, name) == 0;
                    if (!known) sorted.push_back(std::move(cc));
                }
            cats = std::move(sorted);
            for (auto &cc : cats) {
                if (std::strcmp(cc.label, "Controls") != 0) continue;
                cc.label = "Controller setup";
                // dans l'ordre du jeu d'abord (TYPE, visee inversee, vibration, couteau rapide), puis les reglages de Nightjar
                static const char *const first[] = {"Button mapping", "Inverted aim", "Vibration", "Quick knife"};
                std::vector<rm::Option> head, rest;
                for (const char *name : first)
                    for (auto &oo : cc.options)
                        if (std::strcmp(oo.label, name) == 0 && oo.label_str.empty()) { head.push_back(oo); break; }
                for (auto &oo : cc.options) {
                    bool is_head = false;
                    for (const char *name : first) is_head = is_head || std::strcmp(oo.label, name) == 0;
                    if (!is_head) rest.push_back(oo);
                }
                cc.options = head;
                cc.options.insert(cc.options.end(), rest.begin(), rest.end());
            }
            // (18/11) « Reglage de la manette » avait 20 lignes : on la decoupe. Elle garde ce qui vient du jeu (type, visee
            // inversee, vibration, couteau rapide) + icones et manette branchee ; le tactile et la visee ont leur rubrique
            // (colonne Nightjar) ; le mode debug va dans Rapport.
            static const char *const kTouch[] = {"Button look", "Opacity", "Action button", "Layout", "Lock style", "Touch controls", "Button animation", "Button size", "Edit touch layout"};
            static const char *const kAim[] = {"Aim with", "Stick aim speed", "Aim camera"};
            (void) N_("Aim and Wii controls");   // cles de traduction des rubriques et de l'indication de defilement (rmenu)
            (void) N_("The game");
            (void) N_("%d more settings");
            auto in = [](const char *l, const char *const *tab, size_t n) { for (size_t i = 0; i < n; ++i) if (std::strcmp(l, tab[i]) == 0) return true; return false; };
            rm::Category touch, aim;
            touch.label = "Touch controls"; touch.glyph = rm::kGlyphPad; touch.column = 1;
            aim.label = "Aim and Wii controls"; aim.glyph = rm::kGlyphEye; aim.column = 1;
            rm::Option debug_opt;
            bool has_debug = false;
            for (auto &cc : cats) {
                if (std::strcmp(cc.label, "Controller setup") != 0) continue;
                std::vector<rm::Option> keep;
                for (auto &oo : cc.options) {
                    if (!oo.label_str.empty()) keep.push_back(oo);
                    else if (in(oo.label, kTouch, sizeof kTouch / sizeof *kTouch)) touch.options.push_back(oo);
                    else if (in(oo.label, kAim, sizeof kAim / sizeof *kAim)) aim.options.push_back(oo);
                    else if (std::strcmp(oo.label, "Debug mode") == 0) { debug_opt = oo; has_debug = true; }
                    else keep.push_back(oo);
                }
                cc.options = keep;
            }
            for (auto &cc : cats)
                if (has_debug && std::strcmp(cc.label, "Report") == 0 && !cc.custom) cc.options.push_back(debug_opt);
            // colonnes : les quatre rubriques du menu OPTIONS du jeu a gauche, celles de Nightjar a droite
            static const char *const kGame[] = {"Retry or load", "Controller setup", "Brightness adjust", "Audio setup"};
            for (auto &cc : cats) cc.column = in(cc.label, kGame, sizeof kGame / sizeof *kGame) ? 0 : 1;
            {   // GARDE-ROBE ET PERSONNAGES (18/11) : rubrique du jeu, colonne de gauche
                // Choix direct du personnage jouable (Leon, Ashley, Ada, HUNK, Krauser, Wesker, Luis) et tenue de Leon.
                rm::Category wc;
                wc.label = "Wardrobe"; wc.glyph = rm::kGlyphBox; wc.column = 0;
                static const char *const char_names[] = {
                    N_("Leon (Story default)"), N_("Leon S. Kennedy"), N_("Ashley Graham"), N_("Ada Wong"),
                    N_("HUNK"), N_("Jack Krauser (Playable mod)"), N_("Albert Wesker"), N_("Luis Sera")
                };
                static const int char_values[] = {-1, 0, 1, 2, 3, 4, 5, 6};
                rm::Option opt_char;
                opt_char.label = "Character"; opt_char.kind = rm::Kind::Choice; opt_char.value = &g_character;
                opt_char.names = char_names; opt_char.values = char_values; opt_char.n = 8;
                opt_char.hint = "Choose player character: Leon, Ashley, Ada, HUNK, Krauser (with boss moves & mutant arm), Wesker, or Luis. Applies at next room transition or new game.";
                opt_char.change_fn = [](int v) { g_character = v; };
                wc.options.push_back(opt_char);

                static const char *const lo_names[] = {N_("Automatic"), N_("Jacket"), N_("Shirt only"), N_("Vest"), N_("R.P.D.")};
                static const int lo_values[] = {-1, 0, 1, 2, 3};
                rm::Option w;
                w.label = "Leon's outfit"; w.kind = rm::Kind::Choice; w.value = &g_leon_outfit; w.names = lo_names; w.values = lo_values; w.n = 5;
                w.hint = "Automatic: the game decides (jacket, then the shirt when the story takes it off, the vest when you carry it). Any other choice stays for the whole game, cutscenes included. Changes at once.";
                w.change_fn = [](int v) { nj::native::setting(1, v); };
                wc.options.push_back(w);
                cats.push_back(std::move(wc));
            }
            // tactile et visee juste apres Graphismes
            size_t at = cats.size();
            for (size_t i = 0; i < cats.size(); ++i)
                if (std::strcmp(cats[i].label, "Graphics") == 0) at = i + 1;
            std::vector<rm::Category> extra;
            if (!touch.options.empty()) extra.push_back(std::move(touch));
            if (!aim.options.empty()) extra.push_back(std::move(aim));
            cats.insert(cats.begin() + (std::ptrdiff_t) std::min(at, cats.size()), std::make_move_iterator(extra.begin()), std::make_move_iterator(extra.end()));
        }
        bool quit = false;
        const float dp = std::max(1.0f, io.DisplaySize.y / 450.0f);
        rm::draw(cats, rst, g_skin, [](const char *k) { return TXT(k); }, ImVec2(0.0f, 0.0f), io.DisplaySize, dp, quit);
        g_show_fps = m_fps != 0;
        if (g_vsync != (m_vsync != 0)) {
            g_vsync = m_vsync != 0;
            SDL_GL_SetSwapInterval(g_vsync ? 1 : 0);
        }
        if (quit) set_open(false);
    }

    ImGui::End();
#ifdef NJ_MOBILE
    // Au doigt : toucher hors du menu (engrenage compris) le referme, sauf pour quitter une liste
    // deroulante ouverte (14/10).
    if (ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) &&
        !ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
        set_open(false);
#endif
    ImGui::Render();
    nj_fbo(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, dw, dh);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::SetCurrentContext(prev);
}

// ---- ecran d'accueil : menu titre de Nightjar (18/11) ------------------------------------------------------------------
// Le jeu natif attend au titre (port/arm_hooks.cpp, NJS_TITLE_WAIT) ; on dessine CONTINUER (sauvegarde la plus recente),
// NOUVELLE PARTIE, CHARGER (liste des sauvegardes), OPTIONS, et on lui envoie la commande (NJ_MSG_TITLE).
bool g_title_home = false;
namespace {
struct SaveCard {
    int slot = 0;
    bool dated = false;
    int year = 0, mon = 0, mday = 0, hour = 0, min = 0;
    unsigned play = 0;   // secondes
    int diff = 0;
    long long when = 0;  // pour trier : date du fichier
    int room = -1;   // salle (fiche .info du coeur), -1 inconnue
};
std::vector<SaveCard> s_home_saves;
std::vector<std::pair<int, GLuint>> s_home_thumbs;   // emplacement -> texture de l'image de la sauvegarde (bh4_dataNN.thumb)

// PPM P6 ecrit par le coeur (port/njgfx port_save_thumb) -> texture GL ; 0 si absent.
GLuint home_load_thumb(const std::string &path) {
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) return 0;
    int w = 0, h = 0, mx = 0;
    char magic[3] = {};
    GLuint tex = 0;
    if (std::fscanf(f, "%2s %d %d %d", magic, &w, &h, &mx) == 4 && magic[0] == 'P' && magic[1] == '6' && w > 0 && h > 0 && w <= 1024 && h <= 1024) {
        std::fgetc(f);
        std::vector<unsigned char> px((size_t)w * h * 3);
        if (std::fread(px.data(), 1, px.size(), f) == px.size()) {
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, px.data());
        }
    }
    std::fclose(f);
    return tex;
}
GLuint home_thumb_of(int slot) {
    for (const auto &t : s_home_thumbs) if (t.first == slot) return t.second;
    return 0;
}

// bh4_dataNN.bin du coeur (port/card.cpp) : en-tete 'CARD' + longueur, puis les donnees de la carte ; SaveInfo a +0x2000
// (src/game/card.cpp : magie 0x116 a +4, OSCalendarTime a +8, difficulte +0x3D, temps de jeu en secondes +0x48).
void home_read_saves() {
    s_home_saves.clear();
    for (auto &t : s_home_thumbs) glDeleteTextures(1, &t.second);
    s_home_thumbs.clear();
    const std::string dir = nj::native::saves_dir();
    if (dir.empty()) return;
    for (int i = 0; i < 20; ++i) {
        char p[512];
        std::snprintf(p, sizeof p, "%s/bh4_data%02d.bin", dir.c_str(), i);
        FILE *f = std::fopen(p, "rb");
        if (!f) continue;
        std::vector<unsigned char> b;
        unsigned char buf[65536];
        size_t r;
        while ((r = std::fread(buf, 1, sizeof buf, f)) > 0) b.insert(b.end(), buf, buf + r);
        std::fclose(f);
        auto u32 = [&](size_t o) { uint32_t v = 0; if (o + 4 <= b.size()) std::memcpy(&v, &b[o], 4); return v; };
        if (b.size() < 8 || u32(0) != 0x43415244u) continue;
        const size_t len = u32(4);
        if (len > b.size()) continue;
        const size_t d = b.size() - len, info = d + 0x2000;
        if (info + 0x60 > b.size() || u32(info + 4) != 0x116u) continue;
        SaveCard c;
        c.slot = i;
        const int sec = (int)u32(info + 8), mn = (int)u32(info + 12), hr = (int)u32(info + 16), md = (int)u32(info + 20), mo = (int)u32(info + 24), yr = (int)u32(info + 28);
        (void)sec;
        c.dated = yr >= 2000 && yr < 2200 && mo >= 0 && mo < 12 && md >= 1 && md <= 31;
        c.year = yr; c.mon = mo; c.mday = md; c.hour = hr; c.min = mn;
        c.play = u32(info + 0x48);
        c.diff = b[info + 0x3D];
        {   // fiche ecrite par le coeur a la sauvegarde (port_save_info) : salle
            std::string ip = p;
            ip.replace(ip.size() - 4, 4, ".info");
            if (FILE *fi = std::fopen(ip.c_str(), "r")) {
                unsigned r = 0;
                if (std::fscanf(fi, "room=%u", &r) == 1) c.room = (int)r;
                std::fclose(fi);
            }
        }
        struct stat st{};
        if (stat(p, &st) == 0) c.when = (long long)st.st_mtime;
        s_home_saves.push_back(c);
        std::string tp = p;
        tp.replace(tp.size() - 4, 4, ".thumb");
        if (GLuint t = home_load_thumb(tp)) s_home_thumbs.push_back({i, t});
    }
    std::sort(s_home_saves.begin(), s_home_saves.end(), [](const SaveCard &a, const SaveCard &b) { return a.when > b.when; });
}

// Lieu d'une sauvegarde d'apres sa salle (fiche .info) : etape du jeu (1 village, 2 chateau, 3 ile) + code de la salle.
std::string home_place(int room) {
    if (room < 0) return "";
    const int st = (room >> 8) & 0xF;
    const char *n = st == 1 ? N_("The village") : st == 2 ? N_("The castle") : st == 3 ? N_("The island") : nullptr;
    char t[64];
    std::snprintf(t, sizeof t, "%s  (r%03x)", n ? TXT(n) : "", room);
    return t;
}
// Logo du jeu (title.dat, t31 : TPL 480x117 IA8 a 0x0689c0), lu sur le disque du joueur, jamais embarque ; teinte du
// jeu (255, 215, 165). Repli : premiere TPL 480x117 du fichier.
GLuint s_logo_tex = 0;
bool s_logo_tried = false;
GLuint home_logo() {
    if (s_logo_tried) return s_logo_tex;
    s_logo_tried = true;
    std::string dir = nj::native::saves_dir();
    const size_t k = dir.rfind('/');
    if (k == std::string::npos) return 0;
    FILE *f = std::fopen((dir.substr(0, k) + "/disc1/files/ss/Eng/title.dat").c_str(), "rb");
    if (!f) return 0;
    std::vector<unsigned char> b;
    unsigned char buf[65536];
    size_t r;
    while ((r = std::fread(buf, 1, sizeof buf, f)) > 0) b.insert(b.end(), buf, buf + r);
    std::fclose(f);
    auto be32 = [&](size_t o) { return o + 4 <= b.size() ? (uint32_t)b[o] << 24 | (uint32_t)b[o + 1] << 16 | (uint32_t)b[o + 2] << 8 | b[o + 3] : 0u; };
    auto is_logo = [&](size_t o) {
        if (be32(o) != 0x0020AF30u) return false;
        const size_t ih = o + be32(o + 12);
        return (be32(ih) >> 16) == 117 && (be32(ih) & 0xFFFF) == 480 && be32(ih + 4) == 3;
    };
    size_t o = 0x0689c0;
    if (!is_logo(o)) {
        o = 0;
        for (size_t q = 0; q + 64 < b.size(); q += 32) if (is_logo(q)) { o = q; break; }
        if (!o) return 0;
    }
    const size_t ih = o + be32(o + 12), data = o + be32(ih + 8);
    const int w = 480, h = 117;
    if (data + (size_t)((w + 3) / 4) * ((h + 3) / 4) * 32 > b.size()) return 0;
    std::vector<unsigned char> px((size_t)w * h * 4, 0);
    size_t p = data;
    for (int by = 0; by < h; by += 4)
        for (int bx = 0; bx < w; bx += 4)
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x, p += 2) {   // IA8 : alpha puis intensite
                    const int X = bx + x, Y = by + y;
                    if (X >= w || Y >= h) continue;
                    unsigned char *d = &px[((size_t)Y * w + X) * 4];
                    const unsigned a = b[p], i = b[p + 1];
                    d[0] = (unsigned char)(i * 255 / 255); d[1] = (unsigned char)(i * 215 / 255); d[2] = (unsigned char)(i * 165 / 255); d[3] = (unsigned char)a;
                }
    glGenTextures(1, &s_logo_tex);
    glBindTexture(GL_TEXTURE_2D, s_logo_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    return s_logo_tex;
}
// Logo + bandeau a un endroit : renvoie la boite du logo (pour le bandeau, dessine apres le rendu ImGui).
struct LogoPlace { float cx = 0, bottom = 0, w = 0; bool on = false; };
LogoPlace home_draw_logo(ImDrawList *dl, float x, float y, float w, float alpha) {
    LogoPlace lp;
    const GLuint t = home_logo();
    const float h = w * 117.0f / 480.0f;
    if (t) dl->AddImage((ImTextureID)(intptr_t)t, ImVec2(x, y), ImVec2(x + w, y + h), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, (int)(alpha * 255.0f)));
    lp.cx = x + w * 0.5f; lp.bottom = y + h; lp.w = w; lp.on = t != 0;
    return lp;
}
#if defined(_WIN32)
static inline struct tm *nj_localtime_r(const time_t *timep, struct tm *result) {
    localtime_s(result, timep);
    return result;
}
#define localtime_r nj_localtime_r
#endif

std::string home_save_line(const SaveCard &c, bool with_file) {
    char t[200];
    char date[48] = "";
    {   // aujourd'hui / hier + heure, sinon la date
        const time_t now = time(nullptr);
        tm lt{};
        localtime_r(&now, &lt);
        tm y = lt;
        y.tm_mday -= 1;
        mktime(&y);
        if (c.dated && c.year == lt.tm_year + 1900 && c.mon == lt.tm_mon && c.mday == lt.tm_mday)
            std::snprintf(date, sizeof date, "%s %02d:%02d", TXT("Today"), c.hour, c.min);
        else if (c.dated && c.year == y.tm_year + 1900 && c.mon == y.tm_mon && c.mday == y.tm_mday)
            std::snprintf(date, sizeof date, "%s %02d:%02d", TXT("Yesterday"), c.hour, c.min);
    }
    if (date[0]) {}
    else if (c.dated) std::snprintf(date, sizeof date, "%02d/%02d/%04d %02d:%02d", c.mday, c.mon + 1, c.year, c.hour, c.min);
    else {
        const time_t w = (time_t)c.when;
        tm lt{};
        localtime_r(&w, &lt);
        std::strftime(date, sizeof date, "%d/%m/%Y %H:%M", &lt);
    }
    const unsigned h = c.play / 3600, m = (c.play / 60) % 60;
    const std::string place = home_place(c.room);
    const char *diff = c.diff == 6 ? TXT("Professional") : c.diff == 3 ? TXT("Easy") : TXT("Normal");
    if (!place.empty()) std::snprintf(t, sizeof t, "%s  -  %s  -  %uh%02u  -  %s", place.c_str(), diff, h, m, date);
    else if (with_file) std::snprintf(t, sizeof t, "%s %02d   %s   %uh%02u", TXT("File"), c.slot + 1, date, h, m);
    else std::snprintf(t, sizeof t, "%s %02d  -  %s  -  %uh%02u", TXT("File"), c.slot + 1, date, h, m);
    return t;
}
}  // namespace

void title_home_frame(SDL_Window *window) {
    bool want = false;
    NjState st{};
    if (g_title_menu && !g_menu_open && nj::native::loaded() && nj::native::game_state(st) && (st.bits & (NJS_TITLE_WAIT | NJS_SCENE_LOADING))) want = true;
    static int page = 0, cursor = 0;
    // ecran titre (PRESS START) a chaque arrivee au titre ; noir tant que le decor se prepare, fondu quand il est pret
    static bool attract = true;
    static double live_at = -1.0, loading_at = -1.0;
    const bool scene_loading = (st.bits & NJS_SCENE_LOADING) != 0, scene_live = (st.bits & NJS_SCENE_LIVE) != 0;
    // garde-robe (page 3) : la tenue sous le curseur s'essaie dans le decor apres un court arret (manette, clavier) ;
    // A ou un toucher la fait porter (enregistree) ; Retour remet la tenue portee
    static int try_on = -2, hover_idx = -1;
    static double hover_since = 0.0;
    struct WardrobeEntry {
        const char *name;
        const char *sub;
        int outfit;    // -1..3 pour Leon, -100 si personnage
        int character; // -1 Leon auto, 0 Leon, 1 Ashley, 2 Ada, 3 Hunk, 4 Krauser, 5 Wesker, 6 Luis
    };
    static const WardrobeEntry kWardrobeEntries[] = {
        {N_("Leon (Automatic)"), N_("The game decides, as in the original"), -1, -1},
        {N_("Leon (Jacket)"), N_("Leon keeps his iconic jacket all game long"), 0, 0},
        {N_("Leon (Shirt only)"), N_("Tactical shirt, no jacket from the start"), 1, 0},
        {N_("Leon (Tactical Vest)"), N_("Bulletproof SWAT vest look"), 2, 0},
        {N_("Leon (R.P.D. Uniform)"), N_("Raccoon City Police Department classic uniform"), 3, 0},
        {N_("Ashley Graham"), N_("Playable story partner in full campaign"), -100, 1},
        {N_("Ada Wong"), N_("Assignment & Mercenaries moves, hookshot and arsenal"), -100, 2},
        {N_("HUNK"), N_("The Grim Reaper, custom melee, neck breaker & TMP loadout"), -100, 3},
        {N_("Jack Krauser"), N_("Playable mod: full boss moves, super dash, combat knife & mutant arm (R)"), -100, 4},
        {N_("Albert Wesker"), N_("S.T.A.R.S. captain, thrust punch, chicane & heavy firearms"), -100, 5},
        {N_("Luis Sera"), N_("Cabin defender, Red9 marksman & partner play"), -100, 6},
    };
    static const int kNumWardrobeEntries = (int)(sizeof(kWardrobeEntries) / sizeof(kWardrobeEntries[0]));
    auto wardrobe_close = [&]() {
        if (g_character <= 0 && try_on != g_leon_outfit) nj::native::setting(1, g_leon_outfit);
        nj::native::setting(2, 0);
        try_on = -2; hover_idx = -1;
    };
    const bool cleared = (st.bits & NJS_GAME_CLEARED) != 0, easy_ok = (st.bits & NJS_EASY_OK) != 0;
    if (want != g_title_home) {
        g_title_home = want;
#ifdef NJ_MOBILE
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, (want || g_menu_open) ? "1" : "0");   // menu Options ouvert : il en a besoin
        SDL_SetRelativeMouseMode(SDL_FALSE);
#endif
        if (want) { home_read_saves(); page = 0; cursor = 0; attract = g_title_scene != 0; live_at = -1.0; }
    }
    if (!want) return;
    if (!g_ctx) init_imgui(window);
    ImGuiContext *prev = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(g_ctx);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    const ImGuiIO &io = ImGui::GetIO();
    nj::ui_skin::poll();
    build_skin_textures();
    g_skin.font = g_serif;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("###nightjar_home", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(2);
    const float dp = std::max(1.0f, io.DisplaySize.y / 450.0f);
    namespace rm = nj::rmenu;
    std::vector<rm::HomeItem> items;
    enum { kContinue, kNew, kLoad, kOptions, kQuit, kSave, kBack, kLevel, kWardrobe, kOutfitPick };
    std::vector<int> act, slot_of;
    auto add = [&](const char *label, const std::string &sub, bool big, int a, int s = -1) {
        rm::HomeItem it;
        it.label = TXT(label);
        it.sub = sub;
        it.big = big;
        items.push_back(it);
        act.push_back(a);
        slot_of.push_back(s);
    };
    const char *heading = nullptr;
    if (page == 0) {
        if (!s_home_saves.empty()) {
            add(N_("Continue"), home_save_line(s_home_saves[0], false), true, kContinue);
            items.back().img = (ImTextureID)(intptr_t)home_thumb_of(s_home_saves[0].slot);
        }
        add(N_("New game"), "", false, kNew);
        if (!s_home_saves.empty()) add(N_("Load game"), "", false, kLoad);
        add(N_("Wardrobe"), "", false, kWardrobe);
        add(N_("Options"), "", false, kOptions);
#ifndef NJ_MOBILE
        add(N_("Quit"), "", false, kQuit);
#endif
    } else {
        heading = TXT("Load game");
        for (const auto &c : s_home_saves) {
            rm::HomeItem it;
            it.label = home_save_line(c, true);
            it.img = (ImTextureID)(intptr_t)home_thumb_of(c.slot);
            items.push_back(it);
            act.push_back(kSave);
            slot_of.push_back(c.slot);
        }
        add(N_("Back"), "", false, kBack);
    }
    if (page == 2) {   // choix de la difficulte : seulement apres la fin du jeu, comme le jeu (titleLevelInit)
        items.clear(); act.clear(); slot_of.clear();
        heading = TXT("Difficulty");
        add(N_("Professional"), "", false, kLevel, 6);
        add(N_("Normal"), "", false, kLevel, 5);
        if (easy_ok) add(N_("Easy"), "", false, kLevel, 3);
        add(N_("Back"), "", false, kBack);
    }
    if (page == 3) {   // garde-robe et personnages : tenues et heros jouables
        items.clear(); act.clear(); slot_of.clear();
        heading = TXT("Wardrobe & Characters");
        for (int i = 0; i < kNumWardrobeEntries; ++i) {
            std::string sub = TXT(kWardrobeEntries[i].sub);
            bool active = false;
            if (kWardrobeEntries[i].character == -1 && g_character == -1 && g_leon_outfit == -1) {
                active = true;
            } else if (kWardrobeEntries[i].character == 0 && (g_character <= 0) && g_leon_outfit == kWardrobeEntries[i].outfit) {
                active = true;
            } else if (kWardrobeEntries[i].character > 0 && g_character == kWardrobeEntries[i].character) {
                active = true;
            }
            if (active) sub = std::string(TXT("Equipped")) + "  -  " + sub;
            add(kWardrobeEntries[i].name, sub, false, kOutfitPick, i);
        }
        add(N_("Back"), "", false, kBack);
    }
    // garde-robe : la liste dans la moitie gauche, Leon a droite (camera de cabine decalee, game.cpp portMenuSceneFrame)
    ImDrawList *hdl = ImGui::GetWindowDrawList();
    const double now_t = ImGui::GetTime();
    if (scene_live && live_at < 0.0) live_at = now_t;
    if (scene_loading && loading_at < 0.0) loading_at = now_t;
    if (!scene_loading) loading_at = -1.0;
    {   // noir : cache le titre d'origine et la preparation du decor, puis fondu d'ouverture (0,9 s)
        float a = 0.0f;
        if (scene_loading) a = std::clamp((float)((now_t - loading_at) / 0.5), 0.0f, 1.0f);   // fondu au noir 0,5 s
        else if (g_title_scene && live_at >= 0.0) a = std::clamp(1.0f - (float)((now_t - live_at) / 0.9), 0.0f, 1.0f);
        else if (g_title_scene && !scene_live && (st.bits & NJS_TITLE_WAIT) == 0) a = 1.0f;
        if (a > 0.0f) hdl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, (int)(a * 255.0f)));
    }
    const float lx = io.DisplaySize.x * 0.08f;
    int chosen = -1;
    LogoPlace logo;
    static double logo_at = -1.0;   // apparition du bandeau
    if (scene_loading) {
        const char *ld = TXT("Loading");
        const int dots = (int)(now_t * 2.0) % 4;
        std::string t = std::string(ld) + std::string((size_t)dots, '.');
        hdl->AddText(g_serif, 14.0f * dp, ImVec2(io.DisplaySize.x - 140.0f * dp, io.DisplaySize.y - 34.0f * dp), IM_COL32(200, 190, 170, 160), t.c_str());
    } else if (attract) {   // ecran titre : le decor vivant + le titre + APPUYEZ SUR START
        hdl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(io.DisplaySize.x * 0.62f, io.DisplaySize.y), IM_COL32(8, 2, 2, 200), IM_COL32(8, 2, 2, 0), IM_COL32(8, 2, 2, 0), IM_COL32(8, 2, 2, 200));
        const float ty = io.DisplaySize.y * 0.24f;
        logo = home_draw_logo(hdl, lx, ty, io.DisplaySize.x * 0.40f, 1.0f);
        const float pulse = 0.55f + 0.45f * std::sin((float)now_t * 3.0f);
#ifdef NJ_MOBILE
        const char *ps = TXT("Touch the screen");
#else
        const char *ps = TXT("Press Start");
#endif
        hdl->AddText(g_serif, 20.0f * dp, ImVec2(lx, logo.bottom + logo.w * 0.16f), IM_COL32(235, 225, 200, (int)(pulse * 255.0f)), ps);
        hdl->AddText(g_serif, 12.0f * dp, ImVec2(lx, io.DisplaySize.y - 30.0f * dp), IM_COL32(200, 190, 170, 140), "Nightjar");
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadStart, false) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false) || io.MouseReleased[0]) {
            attract = false;
            nj::native::setting(3, -1);   // la voix « Resident Evil 4 » du jeu, comme a PRESS START
        }
    } else {
        logo = home_draw_logo(hdl, lx, 14.0f * dp, io.DisplaySize.x * 0.22f, 1.0f);
        std::string hint = std::string("A  ") + TXT("Select") + "      B  " + TXT("Back");
        hdl->AddText(g_serif, 16.0f * dp, ImVec2(io.DisplaySize.x - 250.0f * dp, io.DisplaySize.y - 34.0f * dp), IM_COL32(235, 225, 205, 225), hint.c_str());
        chosen = rm::draw_home(items, cursor, g_skin, ImVec2(0.0f, 0.0f), io.DisplaySize, dp, heading, true);
    }
    {   // son du curseur du jeu (SndCall(0, 10)) quand la selection change
        static int last_cursor = -1, last_page = -1;
        if (!attract && !scene_loading && page == last_page && cursor != last_cursor && last_cursor >= 0 && chosen < 0)
            nj::native::setting(3, 10);
        last_cursor = cursor; last_page = page;
    }
    if (page == 3) {   // essayage au survol (curseur arrete ~0,35 s sur une tenue de Leon)
        const double now = ImGui::GetTime();
        if (cursor != hover_idx) { hover_idx = cursor; hover_since = now; }
        if (chosen < 0 && cursor >= 0 && cursor < kNumWardrobeEntries && now - hover_since > 0.35) {
            const int o = kWardrobeEntries[cursor].outfit;
            if (o != -100 && try_on != o) {
                try_on = o;
                nj::native::setting(1, try_on);
            }
        }
    }
    if (!attract && !scene_loading && page != 0 && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))) {
        nj::native::setting(3, 5);   // son de retour du jeu
        if (page == 3) wardrobe_close();
        page = 0; cursor = 0;
    }
    if (chosen >= 0) {
        nj::native::setting(3, act[chosen] == kBack ? 5 : 4);   // sons du menu du jeu
        switch (act[chosen]) {
        case kContinue: nj::native::title_command(4, s_home_saves[0].slot); break;   // la plus recente au sens du fichier
        case kNew:   // jeu jamais fini : directement en normal, sans choix (comme le jeu) ; fini : choix de la difficulte
            if (cleared) { page = 2; cursor = 1; }
            else nj::native::title_command(2, -1);
            break;
        case kLevel: nj::native::title_command(2, slot_of[chosen]); break;
        case kLoad: page = 1; cursor = 0; break;
        case kOptions: set_open(true); break;
        case kQuit: { SDL_Event q{}; q.type = SDL_QUIT; SDL_PushEvent(&q); } break;
        case kSave: nj::native::title_command(4, slot_of[chosen]); break;
        case kBack: if (page == 3) wardrobe_close(); page = 0; cursor = 0; break;
        case kWardrobe:
            page = 3;
            cursor = 0;
            for (int i = 0; i < kNumWardrobeEntries; ++i) {
                if (kWardrobeEntries[i].character == g_character) {
                    if (g_character <= 0) {
                        if (kWardrobeEntries[i].outfit == g_leon_outfit) { cursor = i; break; }
                    } else {
                        cursor = i; break;
                    }
                }
            }
            try_on = g_leon_outfit; hover_idx = cursor; hover_since = ImGui::GetTime();
            nj::native::setting(2, 1);   // camera de cabine dans la scene du titre
            break;
        case kOutfitPick: {   // equiper ce personnage ou cette tenue : enregistree, appliquee
            const int idx = slot_of[chosen];
            if (idx >= 0 && idx < kNumWardrobeEntries) {
                const auto &e = kWardrobeEntries[idx];
                g_character = e.character;
                if (e.outfit != -100) {
                    g_leon_outfit = e.outfit;
                    if (try_on != g_leon_outfit) nj::native::setting(1, g_leon_outfit);
                    try_on = g_leon_outfit;
                }
                save_config(window);
            }
            break;
        }
        }
    }
    ImGui::End();
    ImGui::Render();
    int dw = 0, dh = 0;
    SDL_GL_GetDrawableSize(window, &dw, &dh);
    nj_fbo(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, dw, dh);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (logo.on) {   // bandeau « native / PORT BY AYANO » sous le logo, avec ses effets (touch_overlay.cpp)
        if (logo_at < 0.0) logo_at = now_t;
        const float sx = io.DisplaySize.x > 0 ? (float)dw / io.DisplaySize.x : 1.0f;
        const float lh = logo.w * 0.075f;
        nj::runtime::input::touch::title_banner_draw_at(logo.cx * sx, (logo.bottom + lh * 0.9f) * sx, lh * sx, (float)(now_t - logo_at), 1.0f, dw, dh);
    } else {
        logo_at = -1.0;
    }
    ImGui::SetCurrentContext(prev);
}

void fps_overlay_frame(SDL_Window *window) {
    if (!g_show_fps) return;
    if (g_menu_open || g_title_home) return;   // meme contexte ImGui : sa trame avalerait les touchers du menu / de l'accueil
    const auto &pr = nj::runtime::g_p3_progress;
    if (pr.phase.load() != 0) return;

    if (!g_ctx) init_imgui(window);
    ImGuiContext *prev = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(g_ctx);

    static float s_fps = 30.0f;
    static int s_refresh_rate = 60;
    static auto s_last_time = std::chrono::steady_clock::now();
    static bool s_rate_queried = false;

    const auto now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(now - s_last_time).count();
    s_last_time = now;
    // Coeur natif : images neuves du jeu par seconde, comptees sur des fenetres d'une demi-seconde. Sinon (ancien chemin) :
    // cadence des presentations.
    static unsigned s_img0 = 0;
    static double s_win = 0;
    static bool s_native_seen = false;
    if (g_game_images != 0) s_native_seen = true;
    if (s_native_seen) {
        s_win += (dt > 0 && dt < 1.0) ? dt : 0;
        if (s_win >= 0.5) {
            s_fps = (float)((g_game_images - s_img0) / s_win);
            s_img0 = g_game_images;
            s_win = 0;
        }
    } else if (dt > 0.001 && dt < 1.0) {
        const float instant_fps = (float)(1.0 / dt);
        s_fps = s_fps * 0.92f + instant_fps * 0.08f;
    }

    if (!s_rate_queried && window) {
        SDL_DisplayMode mode;
        const int idx = SDL_GetWindowDisplayIndex(window);
        if (SDL_GetCurrentDisplayMode(idx >= 0 ? idx : 0, &mode) == 0 && mode.refresh_rate > 0) {
            s_refresh_rate = mode.refresh_rate;
            s_rate_queried = true;
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    const ImGuiIO &io = ImGui::GetIO();
    char text[64];
    std::snprintf(text, sizeof text, "%.1f FPS / %d Hz", s_fps, s_refresh_rate);

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - ImGui::GetFontSize() * 11.5f, ImGui::GetFontSize() * 0.6f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.40f);
    const ImGuiWindowFlags win_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                       ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("##fps_overlay", nullptr, win_flags)) {
        ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.35f, 1.0f), "%s", text);
    }
    ImGui::End();

    ImGui::Render();
    int dw = 0, dh = 0;
    SDL_GL_GetDrawableSize(window, &dw, &dh);
    nj_fbo(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, dw, dh);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::SetCurrentContext(prev);
}

void prep_frame(SDL_Window *window) {
    const auto &pr = nj::runtime::g_p3_progress;
    const int phase = pr.phase.load();
    if (phase == 0) return;
    if (!g_ctx) init_imgui(window);
    ImGuiContext *prev = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(g_ctx);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    const ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.5f, 0.0f), ImGuiCond_Always);
    ImGui::Begin("##prep", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_AlwaysAutoResize);
#ifdef __ANDROID__
    ImGui::TextUnformatted(TXT("Preparing the game for this device"));
#else
    ImGui::TextUnformatted(TXT("Preparing the game for this computer"));
#endif
    ImGui::Spacing();
    const uint32_t done = pr.done.load(), total = pr.total.load();
    const float f = total ? (float)done / (float)total : 0.0f;
    char label[64];
    if (phase == 1) {
        ImGui::TextDisabled("%s", TXT("Reading your disc..."));
    } else {
        static Uint32 t_install = 0;
        if (phase == 4 && !t_install) t_install = SDL_GetTicks();
        if (phase == 4 && done > 0 && done < total) {
            // temps restant : au rythme moyen depuis le debut de l'installation
            const double el = (SDL_GetTicks() - t_install) / 1000.0, left = el / done * (total - done);
            std::snprintf(label, sizeof label, TXT("%u%%  -  about %d min left"), (unsigned)(f * 100.0f + 0.5f),
                          std::max(1, (int)((left + 59.0) / 60.0)));
        } else if (phase == 4) {
            std::snprintf(label, sizeof label, "%u%%", (unsigned)(f * 100.0f + 0.5f));
        } else {
            std::snprintf(label, sizeof label, "%u / %u", done, total);
        }
        ImGui::TextDisabled("%s", phase == 2   ? TXT("Loading the prepared game...")
                            : phase == 4 ? TXT("Optimizing the game for your hardware...")
                                         : TXT("Translating the game code..."));
        ImGui::ProgressBar(f, ImVec2(-1, 0), label);
    }
    ImGui::Spacing();
    ImGui::TextDisabled("%s", TXT("Only the first launch takes this long."));
    ImGui::End();
    ImGui::Render();
    int dw = 0, dh = 0;
    SDL_GL_GetDrawableSize(window, &dw, &dh);
    nj_fbo(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, dw, dh);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::SetCurrentContext(prev);
}

}  // namespace nj::runtime::gfx
