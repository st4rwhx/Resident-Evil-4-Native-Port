// Crochets de src/game/main_mem.cpp que la cible PC definit dans pc_glue.cpp (fil du jeu repere,
// sonde de tas par variable d'environnement). Sur ARM les fils du jeu sont serialises par un
// jeton unique : tous partagent le tas du jeu, et la sonde reste eteinte.
// port_host_scope : le rendu Nightjar (port/njgfx, libnjruntime) tourne sur le fil du jeu mais ses
// allocations C++ ne doivent pas aller dans le tas du jeu (operator new de main_mem.cpp) : elles y
// decalaient tout (917 Ko + des dizaines de blocs au titre) et r100.das ecrasait le tas 4 (nj14).
// port_render_thread (14/11) : fil de rendu de njgfx (NJ_RTHREAD=1). Il tourne EN MEME TEMPS que le jeu : le compteur
// global ci-dessus ne peut pas le couvrir (le jeu aurait alloue hors de son tas pendant chaque rendu, defaut nj14).
// Il est donc reconnu par son identite ; operator delete choisit deja le tas d'apres l'adresse.
#include <pthread.h>
extern "C" int port_host_scope = 0;
extern "C" unsigned long port_render_thread = 0;
extern "C" int port_on_game_thread(void)
{
    return port_host_scope == 0 && (port_render_thread == 0 || (unsigned long) pthread_self() != port_render_thread);
}
extern "C" int port_heap_trace(void) { return 0; }

#include <stdlib.h>
extern "C" unsigned port_filter_mask(void)   // RE4_FILTERMASK (src/game/filter.cpp)
{
    static const char *e = getenv("RE4_FILTERMASK");
    static const unsigned m = e ? (unsigned) strtoul(e, nullptr, 16) : 0u;
    return m;
}

extern "C" int port_debug_text(void)   // eprintf du jeu (src/game/eprintf.cpp)
{
    static const int on = getenv("RE4_EPRINTF") != nullptr;
    return on;
}

// Pour le code du jeu, qui ne peut pas inclure <stdio.h> (voir osreport_pc.cpp).
#include <android/log.h>
#include <stdarg.h>
extern "C" void port_note(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, "RE4", fmt, ap);
    va_end(ap);
}

// Le jeu reecrit en place l'extension de certains noms de FileTbl (SET_DRS_NAME : ".das" -> ".drs")
// - sur GameCube ces chaines vivent en memoire inscriptible. Sur ARM ce sont des litteraux en
// lecture seule, et l'ecriture tuait le jeu des le chargement du joueur. On rend donc inscriptible
// le segment de donnees en lecture seule de notre propre binaire (copie privee : le fichier n'est
// pas touche).
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
extern "C" void port_rodata_writable(void)
{
    Dl_info di;
    if (!dladdr((void *) &port_rodata_writable, &di) || !di.dli_fbase)
        return;
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f)
        return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        unsigned long lo = 0, hi = 0, off = 0;
        char perms[8] = {0};
        if (sscanf(line, "%lx-%lx %7s %lx", &lo, &hi, perms, &off) != 4)
            continue;
        if (lo < (unsigned long) di.dli_fbase || lo >= (unsigned long) di.dli_fbase + 0x1000000)
            continue;
        if (strcmp(perms, "r--p") == 0)
            mprotect((void *) lo, hi - lo, PROT_READ | PROT_WRITE);
    }
    fclose(f);
}

#include <stdlib.h>
// RE4_START_ROOM=0x100 : salle de depart imposee (banc d'essai uniquement).
extern "C" unsigned port_start_room(void)
{
    static const char *e = getenv("RE4_START_ROOM");
    return e ? (unsigned) strtoul(e, nullptr, 0) : 0u;
}
// RE4_NEWGAME_ROOM=0x101 : la NOUVELLE PARTIE commence dans cette salle (scenes de reference du rejeu, 13/11).
// RE4_START_ROOM n'agit que dans le menu de salles du titre ; la nouvelle partie imposait 0x120.
extern "C" unsigned port_newgame_room(void)
{
    static const char *e = getenv("RE4_NEWGAME_ROOM");
    return e ? (unsigned) strtoul(e, nullptr, 0) : 0u;
}
// RE4_NEWGAME_POINT=n : n-ieme point d'entree valide de debug/roomInfo.dat pour cette salle (0 par defaut).
extern "C" unsigned port_newgame_point(void)
{
    static const char *e = getenv("RE4_NEWGAME_POINT");
    return e ? (unsigned) strtoul(e, nullptr, 0) : 0u;
}
