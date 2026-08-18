// Nightjar 0.1.8 : dispatch multi-region.
// Un seul binaire embarque le code precompile USA et Europe. Les sources de chaque
// region sont compilees avec des symboles renommes (-Dg=g_usa / g_eur, etc.) ;
// ce fichier fournit les vrais run_game() et nj_dispatch_one() que le runtime
// appelle, et les redirige vers la bonne region d'apres le disque du joueur.
#include "runtime/cpu_context.h"
#include "runtime/game_version.h"
#include "runtime/rel_aot.h"

// Multi-region : accepte toute version reconnue (comme P3). disc.cpp verifie g_nj_dol_fnv ;
// 0 = pas de filtre sur le hash du DOL, game_version() fait la selection.
extern "C" const unsigned long long g_nj_dol_fnv = 0;

// --- symboles exportes par chaque jeu de sources precompilees -------------------
extern "C" void run_game_usa(nj::runtime::GuestCpu &ctx);
extern "C" void run_game_eur(nj::runtime::GuestCpu &ctx);
extern "C" void nj_dispatch_one_usa(nj::runtime::GuestCpu &ctx);
extern "C" void nj_dispatch_one_eur(nj::runtime::GuestCpu &ctx);
extern "C" const nj::runtime::RelAotModule g_rel_aot_modules_usa[];
extern "C" const nj::runtime::RelAotModule g_rel_aot_modules_eur[];

// --- compteur de modules REL (termine par base == 0) ----------------------------
static uint32_t count_rel_modules(const nj::runtime::RelAotModule *m) {
    uint32_t n = 0;
    while (m[n].base != 0) ++n;
    return n;
}

// --- run_game : appele une seule fois, boucle jusqu'a la fin --------------------
extern "C" void run_game(nj::runtime::GuestCpu &ctx) {
    using nj::runtime::GameVersion;
    const auto v = nj::runtime::game_version();
    if (v == GameVersion::Europe) {
        nj::runtime::rel_aot_register(g_rel_aot_modules_eur, count_rel_modules(g_rel_aot_modules_eur));
        run_game_eur(ctx);
    } else {
        nj::runtime::rel_aot_register(g_rel_aot_modules_usa, count_rel_modules(g_rel_aot_modules_usa));
        run_game_usa(ctx);
    }
}

// --- nj_dispatch_one : chemin chaud, branchement toujours identique -----------
extern "C" void nj_dispatch_one(nj::runtime::GuestCpu &ctx) {
    if (nj::runtime::game_version() == nj::runtime::GameVersion::Europe)
        nj_dispatch_one_eur(ctx);
    else
        nj_dispatch_one_usa(ctx);
}
