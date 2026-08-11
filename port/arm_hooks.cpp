// Crochets de src/game/main_mem.cpp que la cible PC definit dans pc_glue.cpp (fil du jeu repere,
// sonde de tas par variable d'environnement). Sur ARM les fils du jeu sont serialises par un
// jeton unique : tous partagent le tas du jeu, et la sonde reste eteinte.
extern "C" int port_on_game_thread(void) { return 1; }
extern "C" int port_heap_trace(void) { return 0; }

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
