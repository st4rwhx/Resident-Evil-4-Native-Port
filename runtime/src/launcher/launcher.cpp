#include "runtime/mods.h"
#include "launcher/launcher.h"
#include "runtime/log.h"

#include "disc.h"
#include "discord_auth.h"
#include "runtime/report.h"
#include "runtime/config.h"
#include "host.h"
#include "i18n.h"
#include "launcher_fx.h"
#include "ui_fonts.h"

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"

#include "common/gl.h"

#include <SDL.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>
#include "runtime/obf.h"

using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

namespace nj::launcher {

bool enabled() {
#ifdef NJ_RELEASE
    return true;  // build de diffusion : aucun interrupteur pour s'en passer
#elif defined(__ANDROID__) || defined(__APPLE__)   // iPhone compris (le seul Apple vise ici)
    // Sur mobile, l'ecran d'accueil est le seul moyen de donner le disque (pas de ligne de
    // commande) : actif aussi dans les APK d'essai, NJ_LAUNCHER=0 pour s'en passer.
    const char *e = std::getenv("NJ_LAUNCHER");
    return !(e && e[0] == '0');
#else
    const char *e = std::getenv("NJ_LAUNCHER");
    return e && e[0] == '1';
#endif
}

namespace {

// ---- version -------------------------------------------------------------------------------

enum class Version { Checking, UpToDate, Outdated, Unverifiable };

struct VersionState {
    std::atomic<Version> result{Version::Checking};
    std::mutex m;
    std::string latest;
};

// "v1.2.10" -> {1,2,10}
std::vector<int> parse_version(const std::string &s) {
    std::vector<int> v;
    int cur = -1;
    for (char c : s) {
        if (c >= '0' && c <= '9') cur = (cur < 0 ? 0 : cur) * 10 + (c - '0');
        else if (cur >= 0) { v.push_back(cur); cur = -1; }
        if (c == '-' || c == '+') break;  // suffixe de pre-version ignore
    }
    if (cur >= 0) v.push_back(cur);
    return v;
}

bool newer(const std::string &remote, const std::string &local) {
    std::vector<int> a = parse_version(remote), b = parse_version(local);
    a.resize(4, 0);
    b.resize(4, 0);
    return a > b;
}

std::string json_string(const std::string &body, const char *key) {
    const std::string k = std::string("\"") + key + "\"";
    size_t p = body.find(k);
    if (p == std::string::npos) return {};
    p = body.find('"', body.find(':', p + k.size()) + 1);
    const size_t e = p == std::string::npos ? p : body.find('"', p + 1);
    return e == std::string::npos ? std::string() : body.substr(p + 1, e - p - 1);
}

// Deux sources pour ne dependre ni de la limite de l'API GitHub (60 requetes par heure et par
// adresse, vite atteinte derriere le NAT d'un operateur mobile) ni d'un seul serveur :
//   1. version.txt a la racine du depot, servi par raw.githubusercontent.com ;
//   2. l'API des Releases, tag_name de la derniere.
// Sans reponse valable des deux, la version est Â« invÃ©rifiable Â» : on passe quand meme (13/10, jeu
// hors ligne avec le laissez-passer Discord de la version, discord_auth.cpp).
void check_version(VersionState *st) {
    std::string latest;
    int status = 0;
    std::string body;
#ifndef NJ_RELEASE
    // Tests seulement (absent des builds de diffusion) : simule la derniere version publiee,
    // "none" simulant un reseau muet.
    if (const char *fake = std::getenv("NJ_FAKE_LATEST")) {
        SDL_Delay(600);
        {
            std::lock_guard<std::mutex> l(st->m);
            st->latest = std::strcmp(fake, "none") ? fake : "";
        }
        st->result = !std::strcmp(fake, "none") ? Version::Unverifiable
                     : newer(fake, app_version()) ? Version::Outdated : Version::UpToDate;
        return;
    }
#endif
    if (host::https_get(std::string("https://raw.githubusercontent.com/") + kRepo +
                            (discord::is_nightly() ? "/main/version-nightly.txt" : "/main/version.txt"),
                        status, body) &&
        status == 200) {
        const size_t e = body.find_first_of("\r\n");
        latest = body.substr(0, e);
    }
    if (parse_version(latest).empty() && !discord::is_nightly()) {   // pas de Release GitHub pour la nightly
        latest.clear();
        if (host::https_get(std::string("https://api.github.com/repos/") + kRepo + "/releases/latest",
                            status, body) &&
            status == 200)
            latest = json_string(body, "tag_name");
    }
    {
        std::lock_guard<std::mutex> l(st->m);
        st->latest = latest;
    }
    if (parse_version(latest).empty()) st->result = Version::Unverifiable;
    else st->result = newer(latest, app_version()) ? Version::Outdated : Version::UpToDate;
}

// ---- disque --------------------------------------------------------------------------------

struct DiscState {
    std::atomic<int> busy{0};  // 1 pendant la verification
    std::atomic<bool> done{false};
    std::mutex m;
    std::string image, message;
    DiscCheck result = DiscCheck::Unreadable;
    DiscInfo info;             // disque 1 verifie : identifiant et version
};

void finish(DiscState *st, DiscCheck r, const std::string &image) {
    std::lock_guard<std::mutex> l(st->m);
    st->result = r;
    st->image = image;
    st->message = describe(r);
    st->busy = 0;
    st->done = true;
}

// Rien a signaler : l'ecran redemande le disque (essai automatique d'un disque retenu qui a ete
// deplace ou ne convient plus : pas d'erreur en rouge avant que le joueur ait choisi quoi que ce soit).
void stay_silent(DiscState *st) {
    std::lock_guard<std::mutex> l(st->m);
    st->busy = 0;
    st->done = false;
}

void verify_disc(DiscState *st, std::string image, std::string game_dir, bool automatic = false) {
    DiscInfo info;
    const DiscCheck r = check_and_prepare(image, game_dir, &info);
    if (automatic && r != DiscCheck::Ok) {
        stay_silent(st);
        return;
    }
    {
        std::lock_guard<std::mutex> l(st->m);
        st->info = info;
    }
    finish(st, r, image);
}

// Disque 2 : celui donne, ou (image vide) celui qu'on trouve a cote du disque 1.
// Disque 2. Automatique : le disque retenu, sinon un disque 2 a cote du disque 1 ; rien qui
// convienne -> on le demande, sans message. Choisi par le joueur : le verdict est affiche.
void verify_disc2(DiscState *st, std::string image, std::string first, DiscInfo info, bool automatic = false) {
    if (automatic) {
        if (image.empty() || check_second(image, info) != DiscCheck::Ok)
            image = find_second_beside(first, info);
        if (image.empty()) {
            stay_silent(st);
            return;
        }
    }
    finish(st, check_second(image, info), image);
}

// ---- dessin --------------------------------------------------------------------------------

const ImU32 kRed = IM_COL32(178, 18, 27, 255);
const ImU32 kRedHi = IM_COL32(214, 40, 48, 255);
const ImU32 kText = IM_COL32(236, 232, 226, 255);
const ImU32 kMuted = IM_COL32(160, 154, 146, 255);
const ImU32 kDiscord = IM_COL32(88, 101, 242, 255);
// Un APK ne peut pas contenir sa propre empreinte : la page VERIFY.md du depot public (tenue a jour par
// l'auteur a chaque sortie) liste le SHA-256 et le scan VirusTotal de chaque version.
constexpr const char *kVirusTotalUrl = "https://github.com/st4rwhx/Resident-Evil-4-Native-Port/blob/main/VERIFY.md";

ImFont *g_body = nullptr, *g_title = nullptr, *g_small = nullptr;

void load_fonts(float px) {
    ImGuiIO &io = ImGui::GetIO();
    nj::runtime::i18n::init(nj::runtime::Config::get().ui_language.c_str());   // langue de l'appareil ou choix du menu
    if (!nj::runtime::ui::language_font_available()) nj::runtime::i18n::init("en");   // pas de police CJK ici : anglais
    const char *candidates[][2] = {
#if defined(_WIN32)
        {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/segoeuib.ttf"},
#elif defined(__ANDROID__)
        {"/system/fonts/Roboto-Regular.ttf", "/system/fonts/Roboto-Bold.ttf"},
#elif defined(__APPLE__)
        // polices du systeme iOS (celles qui existent sur l'appareil sont prises, les autres sautees)
        {"/System/Library/Fonts/Core/SFUI.ttf", "/System/Library/Fonts/Core/SFUI.ttf"},
        {"/System/Library/Fonts/Core/HelveticaNeue.ttc", "/System/Library/Fonts/Core/HelveticaNeue.ttc"},
        {"/System/Library/Fonts/Core/Avenir.ttc", "/System/Library/Fonts/Core/Avenir.ttc"},
        {"/System/Library/Fonts/Core/Helvetica.ttc", "/System/Library/Fonts/Core/Helvetica.ttc"},
#endif
        {nullptr, nullptr},
    };
    for (auto &c : candidates) {
        if (!c[0]) break;
        if (!std::filesystem::exists(c[0])) continue;
        g_body = nj::runtime::ui::add_font(io, c[0], px);
        g_small = nj::runtime::ui::add_font(io, c[0], px * 0.78f);
        g_title = nj::runtime::ui::add_font(io, std::filesystem::exists(c[1]) ? c[1] : c[0], px * 1.9f);
        if (g_body && g_small && g_title) return;
    }
    ImFontConfig cfg;
    cfg.SizePixels = px;
    g_body = g_small = g_title = io.Fonts->AddFontDefault(&cfg);
}

void centered_text(ImFont *font, ImU32 col, const char *text, float wrap) {
    ImGui::PushFont(font);
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    const float avail = ImGui::GetContentRegionAvail().x;
    const float w = std::min(wrap, avail);
    // Ligne par ligne pour centrer un texte qui passe a la ligne
    const char *s = text;
    const char *end = text + std::strlen(text);
    while (s < end) {
        // echelle reelle de la police a l'ecran (FontGlobalScale : iPhone haute densite) : mesurer a 1.0 faisait
        // couper les lignes au tiers de la largeur voulue.
        // (ImGui 1.92+ : CalcWordWrapPosition prend la taille affichee, FontGlobalScale compris)
        const char *brk = font->CalcWordWrapPosition(ImGui::GetFontSize(), s, end, w);
        if (brk == s) brk = s + 1;
        const ImVec2 sz = ImGui::CalcTextSize(s, brk);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - sz.x) * 0.5f);
        ImGui::TextUnformatted(s, brk);
        s = brk;
        while (s < end && (*s == ' ' || *s == '\n')) ++s;
    }
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

bool big_button(const char *label, ImU32 col, ImU32 hover, float width) {
    (void)hover;   // la surbrillance rouge est geree par fx::button
    return fx::button(label, col, width, ImGui::GetFontSize() * 2.3f);
}

// Un mini-disque GameCube (8 cm) qui tourne : un reflet balaie la surface, le moyeu reste fixe.
void spinning_disc(ImVec2 c, float r, float t) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddCircleFilled(c, r, IM_COL32(58, 60, 66, 255), 96);
    dl->AddCircleFilled(c, r * 0.97f, IM_COL32(196, 200, 208, 255), 96);
    // bandes concentriques de la surface gravee
    for (int i = 0; i < 6; ++i)
        dl->AddCircle(c, r * (0.45f + i * 0.09f), IM_COL32(170, 175, 186, 255), 96, 1.0f);
    // reflet irise qui tourne
    const ImU32 sheen[3] = {IM_COL32(255, 255, 255, 120), IM_COL32(150, 190, 255, 70), IM_COL32(255, 170, 210, 60)};
    for (int k = 0; k < 2; ++k) {
        const float a0 = t * 2.4f + k * 3.14159f;
        for (int s = 0; s < 3; ++s) {
            dl->PathLineTo(c);
            dl->PathArcTo(c, r * 0.95f, a0 + s * 0.22f, a0 + s * 0.22f + 0.28f, 16);
            dl->PathFillConvex(sheen[s]);
        }
    }
    dl->AddCircleFilled(c, r * 0.36f, IM_COL32(40, 42, 48, 255), 64);  // zone du label
    dl->AddCircle(c, r * 0.36f, IM_COL32(120, 124, 132, 255), 64, 2.0f);
    dl->AddCircleFilled(c, r * 0.13f, IM_COL32(12, 12, 14, 255), 48);  // trou central
    // repere qui tourne, pour que la rotation se voie meme sans reflet
    const float a = t * 2.4f;
    dl->AddLine(ImVec2(c.x + std::cos(a) * r * 0.16f, c.y + std::sin(a) * r * 0.16f),
                ImVec2(c.x + std::cos(a) * r * 0.33f, c.y + std::sin(a) * r * 0.33f), kRed, 3.0f);
}

void open_url(const char *url) { SDL_OpenURL(url); }

}  // namespace

bool run(SDL_Window *window, std::string &game_dir, std::string &disc_image, std::string &disc2_image) {
    // Les doigts doivent pouvoir cliquer dans le lanceur ; le jeu les coupe de la souris ensuite.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    ImGui::StyleColorsDark();
    auto &style = ImGui::GetStyle();
    style.FrameRounding = 10.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.043f, 0.043f, 0.051f, 1.0f);

    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(window, &w, &h);
    // Ecran haute densite (iPhone : 1 point = 3 pixels) : l'interface est dessinee en POINTS, la police est
    // chargee en pixels puis ramenee a l'echelle (FontGlobalScale). Sans cela le texte etait 3 fois trop grand
    // et le bas du menu sortait de l'ecran. Android : fenetre = pixels, dpi = 1, rien ne change.
    int ww = 0, wh = 0;
    SDL_GetWindowSize(window, &ww, &wh);
    const float dpi = (ww > 0) ? (float)w / (float)ww : 1.0f;
    const float base = (ww > 0 && wh > 0) ? (float)std::min(ww, wh) : (float)std::min(w, h);
    load_fonts(std::max(18.0f, base * 0.042f) * dpi);
    io.FontGlobalScale = 1.0f / dpi;

    ImGui_ImplSDL2_InitForOpenGL(window, SDL_GL_GetCurrentContext());
#ifdef NJ_GLES
    ImGui_ImplOpenGL3_Init("#version 300 es"_g);
#else
    ImGui_ImplOpenGL3_Init("#version 150"_g);
#endif

    // Statiques : les fils detaches peuvent finir apres la fermeture du lanceur.
    static VersionState version;
    version.result = Version::Checking;
    std::thread(check_version, &version).detach();
    static DiscState disc, disc2;
    const std::string gdir = (std::filesystem::path(host::data_dir()) / "game").string();

    enum class Screen { Version, Welcome, Disc } screen = Screen::Version;
    bool quit = false, finished = false, picking = false, tried_saved = false, tried_saved2 = false;
    bool without2 = false;   // disque de developpement seul : jouer sans le disque 2 (pas d'ile)
    int picking_disc = 1;
    Uint32 disc_ok_at = 0;
    const Uint32 t0 = SDL_GetTicks();

    NJ_LOG << "[Accueil] demarre, fenetre " << w << "x" << h << " (pixels)" << std::endl;
    unsigned long frames_logged = 0;
    while (!quit && !finished) {
        if (frames_logged++ % 120 == 0) {
            int ww = 0, wh = 0;
            SDL_GetWindowSize(window, &ww, &wh);
            NJ_LOG << "[Accueil] image " << frames_logged - 1 << ", fenetre " << ww << "x" << wh << ", ecran "
                   << int(screen) << ", version " << int(version.result.load()) << ", glerr 0x" << std::hex
                   << glGetError() << std::dec << std::endl;
        }
        discord::frame_tick();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
#if defined(__APPLE__)
            if (e.type == SDL_WINDOWEVENT) {
                static int win_logged = 0;
                if (win_logged++ < 40)
                    NJ_LOG << "[Accueil] evenement fenetre " << int(e.window.event) << " (" << e.window.data1 << ", "
                           << e.window.data2 << "), drapeaux 0x" << std::hex << SDL_GetWindowFlags(window) << std::dec
                           << std::endl;
            }
#endif
            ImGui_ImplSDL2_ProcessEvent(&e);
            quit = quit || e.type == SDL_QUIT;
            if (e.type == SDL_DROPFILE && e.drop.file) {   // mod depose sur la fenetre (runtime/mods.h)
                nj::mods::install_async(e.drop.file);
                SDL_free(e.drop.file);
            }
        }
        const float t = (SDL_GetTicks() - t0) / 1000.0f;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("launcher", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

        const float W = io.DisplaySize.x, H = io.DisplaySize.y;
        fx::background(ImGui::GetWindowDrawList(), io.DisplaySize, t);   // brume, braises, vignette
        fx::title_glow(ImGui::GetWindowDrawList(), ImVec2(W * 0.5f, H * 0.12f), H * 0.30f, t);
        const float col = std::min(W * 0.86f, ImGui::GetFontSize() * 26.0f);
        const float btn = std::min(col, ImGui::GetFontSize() * 15.0f);

        // bandeau du titre
        ImGui::SetCursorPosY(H * 0.07f);
        centered_text(g_title, kRed, N_("NIGHTJAR"), col);
        centered_text(g_small, kMuted, N_("by Ayano"), col);
        // Mention obligatoire (docs/SECURITE_ET_JURIDIQUE.md, 4.5) : sur chaque ecran du lanceur.
        centered_text(g_small, kMuted, N_("Unofficial fan project, not affiliated with Capcom. Requires your own game disc."), col);
        {   // installation d'un mod depose sur la fenetre
            std::string iname, imsg;
            float frac = 0.0f;
            bool iok = false;
            char line[256];
            if (nj::mods::install_status(iname, frac, imsg, iok)) {
                std::snprintf(line, sizeof line, "Installing %s... %d%%", iname.c_str(), (int)(frac * 100.0f));
                centered_text(g_small, kMuted, line, col);
            } else if (!imsg.empty()) {
                centered_text(g_small, iok ? kMuted : kRed, imsg.c_str(), col);
            }
        }
        ImGui::Dummy(ImVec2(0, H * 0.02f));
        const float header_end = ImGui::GetCursorPosY();
        auto place = [&](float y) { ImGui::SetCursorPosY(std::max(y, header_end)); };

        if (screen == Screen::Version) {
            const Version v = version.result.load();
            if (v == Version::Checking) {
                place(H * 0.42f);
                const char *dots[] = {"", ".", "..", "..."};
                std::string msg = std::string("Checking for updates") + dots[int(t * 3) % 4];
                centered_text(g_body, kMuted, msg.c_str(), col);
            } else if (v == Version::UpToDate || v == Version::Unverifiable) {
                screen = Screen::Disc;
            } else if (v == Version::Outdated) {
                std::string latest;
                {
                    std::lock_guard<std::mutex> l(version.m);
                    latest = version.latest;
                }
                place(H * 0.27f);
                centered_text(g_body, kText, N_("An update is available."), col);
                ImGui::Dummy(ImVec2(0, H * 0.015f));
                centered_text(g_body, kText,
                              N_("You can update to the latest version, or continue playing."), col);
                ImGui::Dummy(ImVec2(0, H * 0.03f));
                if (big_button(N_("Download the latest version"), kRed, kRedHi, btn)) open_url(kLatestUrl);
                ImGui::Dummy(ImVec2(0, H * 0.015f));
                if (big_button(N_("Continue to game"), kRed, kRedHi, btn)) screen = Screen::Disc;
                ImGui::Dummy(ImVec2(0, H * 0.015f));
                if (big_button(N_("Join the Discord server"), kDiscord, kDiscord, btn)) open_url(kDiscordUrl);
                ImGui::Dummy(ImVec2(0, H * 0.02f));
                centered_text(g_small, kMuted,
                              N_("To contact support or for any help, join the Discord server: discord.gg/AXAzExECSv"),
                              col);
            } else {  // Unverifiable
                place(H * 0.28f);
                centered_text(g_body, kText, N_("Couldn't check for updates."), col);
                ImGui::Dummy(ImVec2(0, H * 0.015f));
                centered_text(g_small, kMuted,
                              N_("Nightjar needs an internet connection to make sure you are running the latest version. Check your connection and try again."),
                              col);
                ImGui::Dummy(ImVec2(0, H * 0.05f));
                if (big_button(N_("Retry"), kRed, kRedHi, btn)) {
                    version.result = Version::Checking;
                    std::thread(check_version, &version).detach();
                }
                ImGui::Dummy(ImVec2(0, H * 0.02f));
                if (big_button(N_("Join the Discord server"), kDiscord, kDiscord, btn)) open_url(kDiscordUrl);
            }
        } else if (screen == Screen::Welcome) {
            screen = Screen::Disc;
            // Connexion Discord optionnelle pour support / communaute.
            static discord::State account;
            using A = discord::Access;
            if (account.access == A::Idle) {
                account.access = A::Checking;
                std::thread(discord::check_saved, &account).detach();
            }
            const A a = account.access.load();
            std::string user, detail;
            {
                std::lock_guard<std::mutex> l(account.m);
                user = account.user;
                detail = account.detail;
            }
            auto recheck = [&] {
                account.access = A::Checking;
                std::thread(discord::check_saved, &account).detach();
            };
            auto other_account = [&] {
                discord::forget();
                account.access = A::NeedLogin;
            };
            const std::string who = user.empty() ? std::string("Your account") : user;
            place(H * 0.27f);
            if (a == A::Granted) {
                screen = Screen::Disc;
            } else if (a == A::Checking) {
                const char *dots[] = {"", ".", "..", "..."};
                centered_text(g_body, kMuted, (std::string("Checking your Discord account") + dots[int(t * 3) % 4]).c_str(),
                              col);
            } else if (a == A::Waiting) {
#if defined(NJ_MOBILE)
                // appli Discord d'abord (ecran Â« Authorize Â»), navigateur au choix du joueur
                centered_text(g_body, kText, N_("Tap Authorize in Discord, then come back here."), col);
                ImGui::Dummy(ImVec2(0, H * 0.015f));
                centered_text(g_small, kMuted, N_("Discord didn't open the Authorize screen? Use your browser instead."), col);
                ImGui::Dummy(ImVec2(0, H * 0.04f));
                if (big_button(N_("Use the browser instead"), kDiscord, kDiscord, btn)) account.use_browser = true;
                ImGui::Dummy(ImVec2(0, H * 0.02f));
                if (big_button(N_("Cancel"), kDiscord, kDiscord, btn)) account.cancel = true;
#else
                centered_text(g_body, kText, N_("Finish signing in with Discord in your browser."), col);
                ImGui::Dummy(ImVec2(0, H * 0.05f));
                if (big_button(N_("Cancel"), kDiscord, kDiscord, btn)) account.cancel = true;
#endif
            } else if (a == A::NotMember) {
                centered_text(g_body, kText, (who + " is not on the Nightjar Discord server yet.").c_str(), col);
                ImGui::Dummy(ImVec2(0, H * 0.015f));
                centered_text(g_small, kMuted, N_("Join the server, then check again."), col);
                ImGui::Dummy(ImVec2(0, H * 0.04f));
                if (big_button(N_("Join the Discord server"), kDiscord, kDiscord, btn)) open_url(kDiscordUrl);
                ImGui::Dummy(ImVec2(0, H * 0.02f));
                if (big_button(N_("Check again"), kRed, kRedHi, btn)) recheck();
                ImGui::Dummy(ImVec2(0, H * 0.02f));
                if (big_button(N_("Use another account"), kDiscord, kDiscord, btn)) other_account();
            } else if (a == A::NoRole) {
                centered_text(g_body, kText, (who + " needs the Member role on the Nightjar server.").c_str(), col);
                ImGui::Dummy(ImVec2(0, H * 0.015f));
                centered_text(g_small, kMuted, N_("Verify yourself on the server to get it, then check again."), col);
                ImGui::Dummy(ImVec2(0, H * 0.04f));
                if (big_button(N_("Open the Discord server"), kDiscord, kDiscord, btn)) open_url(kDiscordUrl);
                ImGui::Dummy(ImVec2(0, H * 0.02f));
                if (big_button(N_("Check again"), kRed, kRedHi, btn)) recheck();
                ImGui::Dummy(ImVec2(0, H * 0.02f));
                if (big_button(N_("Use another account"), kDiscord, kDiscord, btn)) other_account();
            } else if (a == A::Offline) {
                if (detail == "lease") {
                    centered_text(g_body, kText, N_("Your offline access has expired."), col);
                    ImGui::Dummy(ImVec2(0, H * 0.015f));
                    centered_text(g_small, kMuted, N_("Connect to the internet once to renew it. Your saves are safe."), col);
                } else {
                    centered_text(g_body, kText, N_("Couldn't reach Discord."), col);
                    ImGui::Dummy(ImVec2(0, H * 0.015f));
                    centered_text(g_small, kMuted, N_("Check your connection and try again."), col);
                }
                ImGui::Dummy(ImVec2(0, H * 0.05f));
                if (big_button(N_("Retry"), kRed, kRedHi, btn)) recheck();
            } else {   // NeedLogin, Failed
                static bool s_open_faq = false;

                // Titre de phase place juste apres l'en-tete
                ImGui::SetCursorPosY(header_end);
                centered_text(g_body, kRedHi, N_("CLOSED BETA: HARDWARE OPTIMIZATION"), col);
                ImGui::Dummy(ImVec2(0, H * 0.01f));

                // Cadre structure d'information positionne sous le titre (hauteur ajustee sans scrollbar)
                const float box_w = std::min(W * 0.90f, ImGui::GetFontSize() * 32.0f);
                const float box_h = H * 0.44f;
                const float box_top = ImGui::GetCursorPosY();

                ImGui::SetCursorPosX((W - box_w) * 0.5f);
                ImGui::SetCursorPosY(box_top);

                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(H * 0.015f, H * 0.010f));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(20, 22, 26, 220));
                ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(65, 70, 82, 200));

                ImGui::BeginChild("beta_info", ImVec2(box_w, box_h), true, ImGuiWindowFlags_None);
                ImGui::PushFont(g_small);

                ImGui::TextColored(ImColor(235, 75, 75), "%s", TXT("[?] WHY DISCORD TESTERS?"));
                ImGui::TextWrapped("%s", TXT("Android has thousands of devices (Snapdragon, Mali, Exynos). Testing with the community allows profiling GPU bottlenecks across chips."));
                ImGui::TextColored(ImColor(140, 195, 255), "%s", TXT("In-game: use [Settings -> Report] to send crash dumps directly to Discord!"));
                ImGui::Dummy(ImVec2(0, 3));

                ImGui::TextColored(ImColor(235, 75, 75), "%s", TXT("[!] PRIVACY: NO SERVERS OF OURS, NO DATABASES"));
                ImGui::TextWrapped("%s", TXT("- We run no server and no database: there is nowhere on our side to store your data."));
                ImGui::TextWrapped("%s", TXT("- Official Discord Login: direct authentication. Nightjar never sees or stores your password."));
                ImGui::TextWrapped("%s", TXT("- Hashes and VirusTotal scans of each release: Verify button below. Network: discord.com (sign-in) and github.com (public version file), nothing else."));
                ImGui::Dummy(ImVec2(0, 3));

                ImGui::TextColored(ImColor(235, 75, 75), "%s", TXT("[>] OFFLINE PLAY"));
                ImGui::TextWrapped("%s", TXT("Today: sign in once per version. The game then starts without internet for up to 60 days, and it renews itself whenever you are online."));
                ImGui::TextWrapped("%s", TXT("Resident Evil 4 is a solo game. The final release is planned to work fully offline."));
                ImGui::TextColored(ImColor(160, 160, 160), "%s", TXT("If you prefer not to authenticate during beta tests, simply wait for public release!"));
                ImGui::PopFont();
                ImGui::EndChild();
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar(2);

                if (a == A::Failed && !detail.empty()) {
                    ImGui::Dummy(ImVec2(0, H * 0.006f));
                    centered_text(g_small, kRed, (std::string(TXT("Sign-in failed: ")) + detail).c_str(), col);
                }

                // Bouton principal de connexion positionne sous le cadre
                const float main_btn_w = std::min(box_w, ImGui::GetFontSize() * 20.0f);
                ImGui::SetCursorPosY(box_top + box_h + H * 0.02f);
                if (big_button(N_("Sign in to Participate in Tests"), kDiscord, kDiscord, main_btn_w))
                    std::thread(discord::login, &account).detach();
                if (const int gd = discord::guest_offer_days(); gd > 0) {   // essai invite (access.json Â« guest_days Â»)
                    ImGui::Dummy(ImVec2(0, H * 0.008f));
                    char gl[96];
                    std::snprintf(gl, sizeof gl, TXT("Try %d days as a guest"), gd);
                    if (big_button(gl, kRed, kRedHi, main_btn_w)) std::thread(discord::start_guest, &account).detach();
                }
                ImGui::Dummy(ImVec2(0, H * 0.008f));
                if (big_button(N_("Join the Discord server"), kDiscord, kDiscord, main_btn_w)) {
                    open_url(kDiscordUrl);
                }

                // Ligne de boutons secondaires
                ImGui::Dummy(ImVec2(0, H * 0.01f));
                const float gap = W * 0.015f;
                const float sub_btn_w = (main_btn_w - gap) * 0.5f;
                const float sub_h = ImGui::GetFontSize() * 1.85f;

                ImGui::SetCursorPosX((W - main_btn_w) * 0.5f);
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(IM_COL32(40, 43, 50, 255)));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(IM_COL32(65, 70, 82, 255)));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(IM_COL32(80, 86, 102, 255)));

                if (ImGui::Button(TXT("Verify (hashes)"), ImVec2(sub_btn_w, sub_h)))
                    open_url(kVirusTotalUrl);

                ImGui::SameLine(0, gap);

                const char *faq_id = "privacy_faq_modal";
                if (ImGui::Button(TXT("Privacy FAQ"), ImVec2(sub_btn_w, sub_h)))
                    s_open_faq = true;

                ImGui::PopStyleColor(3);

                // Modal Popup FAQ de Confidentialite
                if (s_open_faq) {
                    ImGui::OpenPopup(faq_id);
                    s_open_faq = false;
                }
                ImVec2 center = ImVec2(W * 0.5f, H * 0.5f);
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(std::min(W * 0.92f, ImGui::GetFontSize() * 32.0f), std::min(H * 0.85f, ImGui::GetFontSize() * 26.0f)));
                bool modal_open = true;
                if (ImGui::BeginPopupModal(faq_id, &modal_open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
                    ImGui::PushFont(g_small);
                    ImGui::TextColored(ImColor(235, 75, 75), "%s", TXT("1. What is this authentication?"));
                    ImGui::TextWrapped("%s", TXT("Nightjar is in closed beta hardware testing. Official Discord login ensures testing stays within our community group without managing any private accounts or passwords."));
                    ImGui::Dummy(ImVec2(0, 4));

                    ImGui::TextColored(ImColor(235, 75, 75), "%s", TXT("2. Does Nightjar collect or send personal data?"));
                    ImGui::TextWrapped("%s", TXT("NO. We run no server and no tracking database. The token only verifies server membership and stays on your device. In-game crash reports are only sent when YOU press [Settings -> Report]."));
                    ImGui::Dummy(ImVec2(0, 4));

                    ImGui::TextColored(ImColor(235, 75, 75), "%s", TXT("3. Is there any malware or spyware?"));
                    ImGui::TextWrapped("%s", TXT("No known malware: the hash and VirusTotal scan of each release are listed behind the Verify button. Nightjar is a native solo fan port. Dump your own original game disc to play."));
                    ImGui::Dummy(ImVec2(0, 4));

                    ImGui::TextColored(ImColor(235, 75, 75), "%s", TXT("4. Will the full release require Discord?"));
                    ImGui::TextWrapped("%s", TXT("NO. RE4 is a solo game. The plan for the final release is a one-time setup on first launch, then fully offline play."));
                    ImGui::PopFont();
                    ImGui::Dummy(ImVec2(0, 8));
                    if (ImGui::Button(TXT("Close"), ImVec2(-1, ImGui::GetFontSize() * 2.0f))) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
            }
        } else {  // Disc
            {
                ImGui::SetCursorPosY(H * 0.94f);
                centered_text(g_small, kMuted, N_("Join our community for help & support: discord.gg/AXAzExECSv"), col);
            }
            const float r = std::min(W, H) * 0.14f;
            const float disc_top = std::max(H * 0.2f, header_end);
            ImGui::SetCursorPosY(disc_top);
            ImGui::Dummy(ImVec2(0, 0));
            const ImVec2 wp = ImGui::GetWindowPos();
            spinning_disc(ImVec2(wp.x + W * 0.5f, wp.y + disc_top + r), r, t);
            ImGui::SetCursorPosY(disc_top + r * 2.0f + H * 0.04f);

            if (!tried_saved) {
                tried_saved = true;
                const std::string saved = host::saved_disc(1);
                if (!saved.empty()) {
                    disc.busy = 1;
                    std::thread(verify_disc, &disc, saved, gdir, true).detach();
                }
            }
            if (picking) {
                std::string p;
                const int s = host::poll_pick(p);
                if (s != 0) picking = false;
                if (s == 1 && picking_disc == 1) {
                    disc.busy = 1;
                    disc.done = false;
                    disc2.done = false;
                    tried_saved2 = false;
                    without2 = false;
                    std::thread(verify_disc, &disc, p, gdir, false).detach();
                } else if (s == 1) {
                    std::lock_guard<std::mutex> l(disc.m);
                    disc2.busy = 1;
                    disc2.done = false;
                    std::thread(verify_disc2, &disc2, p, disc.image, disc.info, false).detach();
                }
            }

            std::string message, image;
            DiscCheck result = DiscCheck::Unreadable;
            DiscInfo info;
            if (disc.done) {
                std::lock_guard<std::mutex> l(disc.m);
                message = disc.message;
                image = disc.image;
                result = disc.result;
                info = disc.info;
            }
            const bool one_ok = disc.done && result == DiscCheck::Ok;
            // Disque 2 : retenu d'un lancement precedent, sinon cherche a cote du disque 1.
            if (one_ok && !tried_saved2 && !disc2.busy) {
                tried_saved2 = true;
                std::string saved2 = host::saved_disc(2);
                disc2.busy = 1;
                disc2.done = false;
                std::thread(verify_disc2, &disc2, saved2, image, info, true).detach();
            }
            std::string message2, image2;
            DiscCheck result2 = DiscCheck::Unreadable;
            if (disc2.done) {
                std::lock_guard<std::mutex> l(disc2.m);
                message2 = disc2.message;
                image2 = disc2.image;
                result2 = disc2.result;
            }
            const bool two_ok = one_ok && disc2.done && result2 == DiscCheck::Ok;
            // Disque de developpement : le disque 2 (l'ile) est facultatif ; sans lui, village et chateau seulement.
            const bool dev_alone = one_ok && info.version == nj::runtime::GameVersion::Debug && without2 && !disc2.busy;
            const std::string region = std::string("Resident Evil 4 (") + region_name(info) + ")";

            if (two_ok || dev_alone) {
                centered_text(g_body, kText, (region + (two_ok ? " - Disc 1 and Disc 2 ready." : " - Disc 1 only (no island).")).c_str(), col);
                ImGui::Dummy(ImVec2(0, H * 0.01f));
                centered_text(g_small, kMuted, N_("Starting..."), col);
                if (!disc_ok_at) {
                    disc_ok_at = SDL_GetTicks();
                    nj::report::add("Disc: " + region + (two_ok ? ", disc 1 and 2 OK" : ", disc 1 only (development disc, no island)"));
                    host::save_disc(image, 1);
                    if (two_ok) host::save_disc(image2, 2);
                }
                if (SDL_GetTicks() - disc_ok_at > 1200) {
                    game_dir = gdir;
                    disc_image = image;
                    disc2_image = two_ok ? image2 : std::string();
                    finished = true;
                }
            } else if (one_ok) {
                centered_text(g_body, kText, (region + " - Disc 1 OK. Now select Disc 2.").c_str(), col);
                ImGui::Dummy(ImVec2(0, H * 0.01f));
                if (disc2.busy) centered_text(g_small, kMuted, N_("Looking for Disc 2..."), col);
                else if (disc2.done) centered_text(g_small, kRedHi, message2.c_str(), col);
                else centered_text(g_small, kMuted, N_("Select the Disc 2 image (.iso, .gcm or .rvz)."), col);
                ImGui::Dummy(ImVec2(0, H * 0.04f));
                if (!disc2.busy && !picking && big_button(N_("Select Disc 2"), kRed, kRedHi, btn)) {
                    picking = true;
                    picking_disc = 2;
                    host::begin_pick(2);
                }
                ImGui::Dummy(ImVec2(0, H * 0.01f));
                if (info.version == nj::runtime::GameVersion::Debug && !disc2.busy && !picking) {
                    ImGui::Dummy(ImVec2(0, H * 0.01f));
                    centered_text(g_small, kMuted, N_("Development disc: Disc 2 holds the island. Without it you can play the village and the castle."), col);
                    if (big_button(N_("Play without Disc 2"), kDiscord, kDiscord, btn))
                        without2 = true;
                }
                ImGui::Dummy(ImVec2(0, H * 0.01f));
                if (!disc2.busy && !picking && big_button(N_("Change Disc 1"), kDiscord, kDiscord, btn)) {
                    picking = true;
                    picking_disc = 1;
                    host::begin_pick(1);
                }
            } else {
                centered_text(g_body, kText, N_("Please select your legal copy of Resident Evil 4 (GameCube) - Disc 1"), col);
                ImGui::Dummy(ImVec2(0, H * 0.01f));
                if (disc.busy) centered_text(g_small, kMuted, N_("Checking the disc..."), col);
                else if (disc.done) centered_text(g_small, kRedHi, message.c_str(), col);
                else centered_text(g_small, kMuted, N_("Select your Disc 1 image (.iso, .gcm or .rvz)."), col);
                ImGui::Dummy(ImVec2(0, H * 0.04f));
                if (!disc.busy && !picking && big_button(N_("Select Disc 1"), kRed, kRedHi, btn)) {
                    picking = true;
                    picking_disc = 1;
                    host::begin_pick(1);
                }
            }
        }

        // plus de numero de version a l'ecran (13/10)

        fx::fade_in(ImGui::GetWindowDrawList(), io.DisplaySize, t);   // fondu depuis le noir a l'ouverture
        ImGui::End();
        ImGui::Render();
        // Sonde des erreurs GL, 6 premieres seulement (journal) : a quelle etape le pilote refuse une commande.
        static int probe_logged = 0;
        auto probe = [&](const char *stage) {
            const GLenum e = glGetError();
            if (e && probe_logged < 6) {
                ++probe_logged;
                NJ_LOG << "[Accueil] glerr 0x" << std::hex << e << std::dec << " avant/pendant : " << stage << std::endl;
            }
        };
        probe("debut de l'image (NewFrame, evenements)");
        int dw = 0, dh = 0;
        SDL_GL_GetDrawableSize(window, &dw, &dh);
        nj_fbo(GL_FRAMEBUFFER, 0);   // sur iOS : le framebuffer de la fenetre SDL
        probe("liaison du framebuffer de la fenetre");
        glViewport(0, 0, dw, dh);
        const bool magenta = false;
        glClearColor(0.043f, 0.043f, 0.051f, 1.0f);
        nj_wipe(GL_COLOR_BUFFER_BIT);
        probe("viewport et effacement");
        if (!magenta) ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        probe("RenderDrawData (ImGui)");
        if (probe_logged < 6 && frames_logged % 120 == 1) {
            GLint fb = -1, st = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb);
            st = (GLint)glCheckFramebufferStatus(GL_FRAMEBUFFER);
            NJ_LOG << "[Accueil] framebuffer lie " << fb << ", etat 0x" << std::hex << st << std::dec << ", fenetre " << dw
                   << "x" << dh << std::endl;
        }
        nj_before_swap();
        SDL_GL_SwapWindow(window);
        probe("SDL_GL_SwapWindow");
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    return finished;
}

}  // namespace nj::launcher
