#pragma once
// Journal du moteur. Dans un build de diffusion (NJ_RELEASE), ni l'appel ni le texte ne restent
// dans le binaire : la branche est ecartee a la compilation (if constexpr), ses chaines avec.
// La forme « if constexpr (...) {} else » garde le sens d'un if/else qui l'entoure.
#include <cstdio>
#include <iostream>

namespace nj {
#if defined(NJ_RELEASE) && !defined(NJ_DEVTOOLS)
inline constexpr bool kLogs = false;
#else
inline constexpr bool kLogs = true;
#endif
}  // namespace nj

#define NJ_LOG if constexpr (!::nj::kLogs) {} else std::cout
#define NJ_ERR if constexpr (!::nj::kLogs) {} else std::cerr
#define NJ_PRINTF(...) do { if constexpr (::nj::kLogs) std::printf(__VA_ARGS__); } while (0)
#define NJ_EPRINTF(...) do { if constexpr (::nj::kLogs) std::fprintf(stderr, __VA_ARGS__); } while (0)
