// -*- coding: utf-8 -*-
// Point d'entree de la cible PC : aurora ouvre la fenetre et le GPU, puis on entre dans le
// main du jeu.
//
// aurora prend le `main` du programme (`aurora/main.h` fait `#define main aurora_main`) ; le
// notre devient donc `aurora_main`, appele par sa bibliotheque. Ensuite c'est RE4 qui mene :
// sa boucle principale est infinie et ne rend jamais la main, alors la fenetre est pompee
// depuis `VIWaitForRetrace` (voir port/vi_pc.cpp) - le seul point ou le jeu s'arrete une fois
// par image.
//
// Aucune donnee n'est embarquee : l'application demande son jeu a l'utilisateur. Ici c'est un
// argument ou RE4_GAME_DIR ; l'Activity Android appellera exactement le meme point.
#include <aurora/aurora.h>
#include <aurora/main.h>

// OSBaseAddress vient d'aurora, pas du decompile : ses en-tetes passent apres les notres dans
// le chemin d'inclusion, et <dolphin/os.h> resoudrait vers la version du jeu, qui ne la
// declare pas. On la reprend donc telle quelle (lib/dolphin/os/OSMemory.cpp, liaison C).
extern "C" {
#include <stdint.h>
extern uintptr_t OSBaseAddress;
}

#include <cstdio>
#include <cstdlib>
#include <cstring>

void port_set_game_dir(const char *dir);
extern "C" void port_vi_first_frame(void);
int re4_main();   // liaison C++ : defini dans src/game/main.cpp

static void log_callback(AuroraLogLevel level, const char *module, const char *message,
                         unsigned int len)
{
    (void)len;
    const char *tag = "?";
    std::FILE *out = stdout;
    switch (level) {
    case LOG_DEBUG:   tag = "DEBUG"; break;
    case LOG_INFO:    tag = "INFO"; break;
    case LOG_WARNING: tag = "WARN"; break;
    case LOG_ERROR:   tag = "ERREUR"; out = stderr; break;
    case LOG_FATAL:   tag = "FATAL";  out = stderr; break;
    }
    std::fprintf(out, "[aurora %s: %s] %s\n", tag, module, message);
    std::fflush(out);
}

int main(int argc, char *argv[])
{
    // Le dossier du jeu : le premier argument qui n'est pas une option d'aurora.
    const char *dir = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') { dir = argv[i]; break; }
    }
    if (!dir)
        dir = std::getenv("RE4_GAME_DIR");
    if (!dir) {
        std::printf("usage: %s <dossier du jeu extrait (sys/ + files/)>\n", argv[0]);
        return 1;
    }

    AuroraConfig config;
    std::memset(&config, 0, sizeof(config));
    config.appName = "Resident Evil 4";
    config.logCallback = &log_callback;
    config.logLevel = LOG_INFO;
    config.windowWidth = 640;
    config.windowHeight = 480;
    config.vsync = true;
    // 24 Mo de MEM1, comme le GameCube. Pas de MEM2 : notre couche ARAM (port/aram.cpp) gere
    // deja les 16 Mo d'ARAM et les transferts ARQ que RE4 attend.
    config.mem1Size = 24 * 1024 * 1024;
    config.mem2Size = 0;

    AuroraInfo info = aurora_initialize(argc, argv, &config);
    std::printf("[PORT] aurora prete : fenetre %ux%u\n", info.windowSize.width,
                info.windowSize.height);

    port_vi_first_frame();

    std::printf("[PORT] jeu : %s\n", dir);
    port_set_game_dir(dir);
    const int r = re4_main();

    // On n'arrive normalement jamais ici : la boucle de RE4 ne se termine pas, c'est la
    // fermeture de la fenetre (traitee dans VIWaitForRetrace) qui arrete le programme.
    std::printf("[PORT] re4_main a rendu %d\n", r);
    aurora_shutdown();
    return r;
}

// La base de MEM1 telle qu'aurora l'a allouee. PORT_MEM1() y ramene les quatre adresses que
// RE4 ecrit en clair (port/port_prelude.h).
extern "C" unsigned long port_mem1_base(void)
{
    return (unsigned long) OSBaseAddress;
}
