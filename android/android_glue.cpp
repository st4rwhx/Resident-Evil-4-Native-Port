// Ce que la coque Android doit faire avant que main() ne tourne.
//
// Le runtime ecrit son journal sur std::cout / std::cerr, qui ne vont nulle part dans une
// application Android : on les redirige vers logcat (etiquette NJ), ligne par ligne, par un
// tube et un fil de lecture. Et plusieurs fichiers sont ouverts par un chemin relatif
// (input.ini, config.toml, les vidages de debug) : on se place dans le repertoire de
// l'application, que RE4Activity publie dans NJ_FILES_DIR avant de charger libmain.so.
#include "common/platform.h"

#include <android/log.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace {

int g_pipe[2];

void *pump_to_logcat(void *) {
    char buf[1024];
    size_t used = 0;
    for (;;) {
        ssize_t n = read(g_pipe[0], buf + used, sizeof(buf) - 1 - used);
        if (n <= 0) break;
        used += (size_t) n;
        buf[used] = 0;
        char *line = buf;
        char *nl;
        while ((nl = strchr(line, '\n')) != nullptr) {
            *nl = 0;
            __android_log_write(ANDROID_LOG_INFO, "NJ", line);
            line = nl + 1;
        }
        used = strlen(line);
        memmove(buf, line, used);
        if (used == sizeof(buf) - 1) {  // ligne trop longue : la couper plutot que bloquer
            __android_log_write(ANDROID_LOG_INFO, "NJ", buf);
            used = 0;
        }
    }
    return nullptr;
}

__attribute__((constructor)) void android_glue_init() {
#if defined(NJ_RELEASE) && !defined(NJ_DEVTOOLS)
    // Build de diffusion : rien dans logcat, que toute application avec l'ADB lirait.
    (void) pump_to_logcat;
    if (FILE *nul = freopen("/dev/null", "w", stdout)) (void) nul;
    if (FILE *nul = freopen("/dev/null", "w", stderr)) (void) nul;
#else
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
#endif
#if !defined(NJ_RELEASE) || defined(NJ_DEVTOOLS)
    if (pipe(g_pipe) == 0) {
        dup2(g_pipe[1], 1);
        dup2(g_pipe[1], 2);
        pthread_t t;
        pthread_create(&t, nullptr, pump_to_logcat, nullptr);
        pthread_detach(t);
    }
#endif
    if (const char *dir = getenv("NJ_FILES_DIR")) {
#ifdef NJ_RELEASE
        if (chdir(dir) != 0) {}
#else
        if (chdir(dir) != 0)
            __android_log_print(ANDROID_LOG_WARN, "NJ", "chdir(%s) a echoue", dir);
#endif
        nj::set_writable_dir(dir);
#if defined(NJ_DEVTOOLS)
        // dev_env.txt (build de dev prive) : une ligne NOM=valeur par interrupteur NJ_*, pose avant tout NJ_ENV
        // (chaque NJ_ENV lit sa variable une seule fois). Ex. : NJ_BOOTDIRECT=1 pour sauter les logos.
        if (FILE *f = fopen("dev_env.txt", "r")) {
            char l[512];
            while (fgets(l, sizeof l, f)) {
                l[strcspn(l, "\r\n")] = 0;
                char *eq = strchr(l, '=');
                if (l[0] == '#' || !eq) continue;
                *eq = 0;
                setenv(l, eq + 1, 1);
                __android_log_print(ANDROID_LOG_INFO, "NJ", "dev_env : %s=%s", l, eq + 1);
            }
            fclose(f);
        }
#endif
    }
}

}  // namespace
