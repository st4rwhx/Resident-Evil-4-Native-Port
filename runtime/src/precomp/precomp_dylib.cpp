// iOS : le code precompile du jeu n'est PAS lie dans l'application. Il est compile ailleurs en
// libnjgame_usa.dylib / libnjgame_eur.dylib, glisse dans Frameworks/ avant la signature, et charge ici
// d'apres le disque du joueur (meme choix de version que precomp_dispatch.cpp).
#include <dlfcn.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "runtime/cpu_context.h"
#include "runtime/game_version.h"
#include "runtime/log.h"
#include "runtime/rel_aot.h"

// Pas de filtre sur le hash du DOL : game_version() fait la selection (comme precomp_dispatch.cpp).
extern "C" const unsigned long long g_nj_dol_fnv = 0;

namespace {

using RunFn = void (*)(nj::runtime::GuestCpu &);
struct Game {
    void *lib = nullptr;
    RunFn run = nullptr;
    RunFn dispatch = nullptr;
    const nj::runtime::RelAotModule *modules = nullptr;
};

std::string frameworks_dir() {
    Dl_info info{};
    if (dladdr(reinterpret_cast<void *>(&frameworks_dir), &info) && info.dli_fname) {
        std::string p = info.dli_fname;   // .../Nightjar.app/Nightjar
        const auto slash = p.rfind('/');
        if (slash != std::string::npos) return p.substr(0, slash) + "/Frameworks/";
    }
    return "";
}

Game load_game(const char *region) {
    Game g;
    const std::string name = std::string("libnjgame_") + region + ".dylib";
    const std::string dir = frameworks_dir();
    if (!dir.empty()) g.lib = dlopen((dir + name).c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!g.lib) g.lib = dlopen(("@rpath/" + name).c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!g.lib) {
        const char *e = dlerror();
        NJ_ERR << "[Jeu] " << name << " introuvable : " << (e ? e : "?") << std::endl;
        std::abort();
    }
    g.run = reinterpret_cast<RunFn>(dlsym(g.lib, (std::string("run_game_") + region).c_str()));
    g.dispatch = reinterpret_cast<RunFn>(dlsym(g.lib, (std::string("nj_dispatch_one_") + region).c_str()));
    g.modules = reinterpret_cast<const nj::runtime::RelAotModule *>(
        dlsym(g.lib, (std::string("g_rel_aot_modules_") + region).c_str()));
    if (!g.run || !g.dispatch || !g.modules) {
        NJ_ERR << "[Jeu] " << name << " : symboles manquants" << std::endl;
        std::abort();
    }
    return g;
}

uint32_t count_rel_modules(const nj::runtime::RelAotModule *m) {
    uint32_t n = 0;
    while (m[n].base != 0) ++n;
    return n;
}

Game &selected() {
    static Game game = [] {
        const bool eur = nj::runtime::game_version() == nj::runtime::GameVersion::Europe;
        Game g = load_game(eur ? "eur" : "usa");
        nj::runtime::rel_aot_register(g.modules, count_rel_modules(g.modules));
        return g;
    }();
    return game;
}

}  // namespace

extern "C" void run_game(nj::runtime::GuestCpu &ctx) { selected().run(ctx); }

extern "C" void nj_dispatch_one(nj::runtime::GuestCpu &ctx) {
    static const RunFn fn = selected().dispatch;
    fn(ctx);
}
