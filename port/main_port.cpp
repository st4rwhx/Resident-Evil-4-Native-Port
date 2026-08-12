// -*- coding: utf-8 -*-
// Coque desktop du portage : ce que fera l'Activity Android ou le projet Xcode.
//
// Elle ne fait que deux choses, et c'est deliberement tout : designer le jeu de l'utilisateur,
// puis entrer dans le main du jeu. Aucune donnee n'est embarquee - l'application mobile
// demandera l'ISO au premier lancement et appellera exactement ce point-la.
#include <cstdio>
#include <cstdlib>

void port_set_game_dir(const char *dir);
extern "C" void port_install_crash_handler(void);
int re4_main();   // liaison C++ : defini dans src/game/main.cpp

extern "C" void port_rodata_writable(void);
#include "njlink.h"

int main(int argc, char **argv)
{
    port_install_crash_handler();
    port_rodata_writable();

    const char *dir = (argc > 1) ? argv[1] : std::getenv("RE4_GAME_DIR");
    if (!dir) {
        std::printf("usage: %s <dossier du jeu extrait (sys/ + files/)>\n", argv[0]);
        return 1;
    }
    std::printf("[PORT] jeu : %s\n", dir);
    njlink_init();   // lance par l'appli Nightjar : images partagees et manette (njlink.cpp)
    port_set_game_dir(dir);
    const int r = re4_main();
    std::printf("[PORT] re4_main a rendu %d\n", r);
    return r;
}
