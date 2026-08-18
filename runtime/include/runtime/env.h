#pragma once
#include <cstdlib>

// Cached environment switches.
//
// std::getenv takes the CRT environment lock and scans every entry. The
// runtime's diagnostic switches (NJ_*) used to be read on hot paths --
// drain_callbacks runs on every loop back-edge of recompiled code --
// and on Windows that alone pinned the CPU thread: gdb stack samples of
// Resident Evil 4 landed in ucrtbase!strchr under drain_callbacks
// 7 times out of 10, holding the guest to ~3 MIPS.
//
// NJ_ENV("NAME") reads the variable once per call site (every expansion is
// its own lambda with its own static) and returns the cached pointer after
// that. The runtime never modifies its own environment, so this is exact.
//
// Build de diffusion (NJ_RELEASE) : aucun interrupteur. Chaque NJ_ENV vaut nullptr, donc le
// comportement par defaut ; rien de ce qui trace, vide la memoire, ecrit des fichiers ou change
// l'emulation ne peut etre reveille de l'exterieur (variables posees par un autre programme, ou
// sur Android par l'intention qui lance l'application).
// Exception : build de dev prive (NJ_DEVTOOLS, jamais publie, verifie par verifier_diffusion) : les interrupteurs restent
// lisibles ; sur Android, la coque les pose depuis dev_env.txt (android_glue.cpp), faute de variables d'environnement.
#if defined(NJ_RELEASE) && !defined(NJ_DEVTOOLS)
#define NJ_ENV(name) (static_cast<const char *>(nullptr))
#else
#define NJ_ENV(name)                                                         \
  ([]() -> const char * {                                                      \
    static const char *const nj_env_value_ = std::getenv(name);              \
    return nj_env_value_;                                                    \
  }())
#endif
