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
#include <stdio.h>
extern "C" void port_note(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, "RE4", fmt, ap);
    va_end(ap);
    va_start(ap, fmt);   // aussi dans le journal du coeur (core.log), lu par nos bancs
    vprintf(fmt, ap);
    va_end(ap);
    putchar(10);
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
// Scene de menu (menu titre de Nightjar, etape 3, 18/11) : NJ_TITLE_SCENE=<salle> (pose par l'appli) -> au titre, sans
// commande en attente, le jeu entre dans cette salle (chemin DEBUG GAME), la pad est ignoree, une camera lente remplace la
// sienne (game.cpp) et l'accueil de l'appli reste affiche ; une commande -> remise a zero, puis le titre l'execute.
// Etat : 0 hors scene, 1 en route (choix fait au titre), 2 active (salle chargee ou en chargement), 3 sortie demandee.
static volatile int g_scene = 0;
extern "C" unsigned port_scene_room(void)
{
    static const char *e = getenv("NJ_TITLE_SCENE");
    return e ? (unsigned) strtoul(e, nullptr, 0) : 0u;
}
extern "C" int port_menu_scene_state(void) { return g_scene; }
extern "C" void port_menu_scene_set(int v) { g_scene = v; }
extern "C" int port_menu_scene_active(void) { return g_scene == 2; }
extern "C" unsigned port_start_room(void)
{
    if (g_scene == 1) {   // la scene de menu passe par le meme chemin que la salle de test
        g_scene = 2;
        return port_scene_room();
    }
    static const char *e = getenv("RE4_START_ROOM");
    return e ? (unsigned) strtoul(e, nullptr, 0) : 0u;
}
// Sortie du titre (A2, 15/11, src/game/title.cpp titleExit) : choix direct au lieu du menu de salles de debug invisible.
// RE4_ROOM_MENU=1 : menu de debug d'origine. Choix : port_title_choice_set() (demarrage Nightjar, a venir), sinon
// RE4_TITLE_CHOICE=new|load|test, sinon salle de test si RE4_START_ROOM est pose, chargement si RE4_AUTOLOAD=2,
// sinon nouvelle partie (salle 0x120, cinematique d'ouverture, comme le jeu commercial).
static int g_title_choice = -1;
extern "C" void port_title_choice_set(int choice) { g_title_choice = choice; }

// Menu titre de Nightjar (18/11) : NJ_TITLE_APP=1 (pose par l'appli au lancement du coeur) -> le jeu n'affiche plus son
// menu titre et attend une commande de l'appli (NJ_MSG_TITLE, port/njlink.cpp) : 1 continuer (sauvegarde la plus
// recente), 2 nouvelle partie, 3 ecran de chargement du jeu, 4 charger la sauvegarde `slot`, 5 options du jeu.
static volatile int g_title_cmd = 0, g_title_slot = -1;
static volatile int g_title_waiting = 0;
static int g_card_direct = -2;   // chargement direct : -2 aucun, -1 la plus recente, n = sauvegarde n (bh4_data0n)
extern "C" int port_title_app(void)
{
    static const char *e = getenv("NJ_TITLE_APP");
    return e && e[0] == '1';
}
extern "C" void port_title_cmd_set(int cmd, int slot) { g_title_slot = slot; g_title_cmd = cmd; }
extern "C" int port_title_cmd_take(int *slot)
{
    const int c = g_title_cmd;
    if (c) {
        *slot = g_title_slot;
        g_title_cmd = 0;
    }
    return c;
}
extern "C" void port_title_waiting_set(int v) { g_title_waiting = v; }
extern "C" int port_title_cmd_pending(void) { return g_title_cmd != 0; }
extern "C" int port_title_waiting(void) { return g_title_waiting; }
extern "C" int port_card_direct(void) { return g_card_direct; }
extern "C" void port_card_direct_set(int n) { g_card_direct = n; }
extern "C" __attribute__((weak)) void port_save_thumb(int) {}   // sans le rendu Nightjar (port/njgfx) : pas d'image
extern "C" int port_room_menu(void)
{
    static const char *e = getenv("RE4_ROOM_MENU");
    return e && e[0] == '1';
}
extern "C" int port_title_choice(void)
{
    if (g_title_choice >= 0) return g_title_choice;
    if (const char *e = getenv("RE4_TITLE_CHOICE")) {
        if (e[0] == 'l') return 1;
        if (e[0] == 't') return 0;
        return 2;
    }
    if (port_start_room()) return 0;
    if (const char *a = getenv("RE4_AUTOLOAD"))
        if (a[0] == '2') return 1;
    return 2;
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

// RE4_CHK_PL=1 : verification par image de la chaine des zones de touche de Leon (15/11, plantage em10 : un
// p->next valait 0x437a0000, un flottant). Appelee depuis main.cpp (port_tm_frame).
extern "C" int port_chk_pl(void)
{
    static const int on = getenv("RE4_CHK_PL") != nullptr;
    return on;
}

// Adresse de retour lisible par addr2line : decalage dans notre binaire (libnjcore.so).
extern "C" void port_ret_note(const char *tag, void *ret)
{
    Dl_info di;
    if (dladdr(ret, &di) && di.dli_fbase)
        port_note("%s +0x%lx", tag, (unsigned long) ((char *) ret - (char *) di.dli_fbase));
}
