#include <malloc.h>
// -*- coding: utf-8 -*-
// Coque desktop du portage : ce que fera l'Activity Android ou le projet Xcode.
//
// Elle ne fait que deux choses, et c'est deliberement tout : designer le jeu de l'utilisateur,
// puis entrer dans le main du jeu. Aucune donnee n'est embarquee - l'application mobile
// demandera l'ISO au premier lancement et appellera exactement ce point-la.
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <stddef.h>
#if defined(__LP64__)
extern "C" void* port_pile_fenetre(int slot, size_t* taille);   // port/os.cpp
#endif

void port_set_game_dir(const char *dir);
extern "C" void port_install_crash_handler(void);
int re4_main();   // liaison C++ : defini dans src/game/main.cpp

extern "C" void port_rodata_writable(void);
#include "njlink.h"

int main(int argc, char **argv)
{
    port_install_crash_handler();
    port_rodata_writable();
    // Journal ligne par ligne : l'appli arrete le coeur par un arret force, et un tampon plein laissait perdre les
    // dernieres secondes du journal (on croyait le jeu fige alors qu'il tournait).
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    // Allocateur d'Android 9 (jemalloc) : rendre les pages liberees au systeme apres un delai plutot que tout de suite.
    // Mesure du 13/11 (simpleperf, cinematique r100) : 11 200 defauts de page par seconde sur le fil du jeu, ~30 % de
    // son temps dans le noyau, parce que le rendu alloue et libere des Mo de sommets a chaque image.
#ifndef NJ_APPLE_CROSS   // iOS : pas de mallopt (allocateur d'Android seulement)
    std::printf("[PORT] mallopt(M_DECAY_TIME, 1) = %d\n", mallopt(-100 /* M_DECAY_TIME, Android 9+ */, 1));
#endif

    const char *dir = (argc > 1) ? argv[1] : std::getenv("RE4_GAME_DIR");
    if (!dir) {
        std::printf("usage: %s <dossier du jeu extrait (sys/ + files/)>\n", argv[0]);
        return 1;
    }
    std::printf("[PORT] jeu : %s\n", dir);
    njlink_init();   // lance par l'appli Nightjar : images partagees et manette (njlink.cpp)
    port_set_game_dir(dir);
#if PORT_P64
    // N7 : le jeu tourne sur un fil dont la pile est DANS la fenetre de 4 Go (port/os.cpp port_pile_fenetre) : ses adresses
    // de variables locales tiennent alors en 32 bits, comme sur la console.
    int r = 0;
    {
        pthread_attr_t at;
        pthread_attr_init(&at);
        size_t t = 0;
        void* pile = port_pile_fenetre(-1, &t);
        pthread_attr_setstack(&at, pile, t);
        pthread_t fil;
        static int rendu;
        pthread_create(&fil, &at, [](void*) -> void* { rendu = re4_main(); return nullptr; }, nullptr);
        pthread_attr_destroy(&at);
        pthread_join(fil, nullptr);
        r = rendu;
    }
#else
    const int r = re4_main();
#endif
    std::printf("[PORT] re4_main a rendu %d\n", r);
    return r;
}
