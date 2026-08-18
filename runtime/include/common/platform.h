// Couche de compatibilite plateforme : Windows / Linux / macOS / Android / iOS, x86-64 et arm64.
//
// Pourquoi ce fichier existe alors que le portage semblait enorme. Audit mesure le 2026-09-20
// (tools/arm64_audit.py) sur 110,7 Mo de sources :
//
//   * les **109,2 Mo de C++ recompile** (export/output_*.cpp) sont neutres en architecture :
//     zero asm inline, zero intrinseque x86, zero `long` dependant de l'ABI, zero #pragma pack,
//     zero cast de pointeur reinterprete, zero branche sur l'endianness. Le recompilateur emet
//     du C++ portable par construction, et c'est 99 % du code ;
//   * `common/endian.h` avait deja la branche `__builtin_bswap` : rien a faire, clang arm64
//     l'assemble en `rev` ;
//   * `virtual_disc.cpp` et `input/sources/phone_source.cpp` etaient deja entierement `#ifdef`es
//     pour POSIX ;
//   * il ne restait donc, cote Windows dur, **qu'un `Sleep(1)`** dans le profileur d'echantillonnage
//     de `core/main.cpp`, lequel est de toute facon un outil de dev (SuspendThread + en-tetes PE).
//
// Ce qui reste vraiment specifique a arm64 n'est pas du code mais du **comportement** :
//
//   * `char` est **non signe par defaut sur ARM**, signe sur x86. Teste en simulant le defaut ARM
//     avec `g++ -funsigned-char` sur les 30 unites generees ;
//   * `long` fait 32 bits sur Windows x64 et **64 bits** sur Linux/Android/iOS arm64. Les deux
//     seuls usages restants sont dans des chemins de debug pilotes par variable d'environnement,
//     corriges en types de largeur fixe ;
//   * la conversion flottant -> entier hors domaine **sature** sur arm64 la ou x86 rend
//     0x80000000. Pour un recompilateur PPC c'est un ecart observable : voir `cvt_f2i_ppc`.
#pragma once

#include <cstdint>

// ---------------------------------------------------------------- identification

#if defined(_WIN32)
#  define NJ_OS_WINDOWS 1
#elif defined(__ANDROID__)
#  define NJ_OS_ANDROID 1
#  define NJ_OS_POSIX 1
#  define NJ_MOBILE 1
#elif defined(__APPLE__)
#  include <TargetConditionals.h>
#  if TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
#    define NJ_OS_IOS 1
#    define NJ_MOBILE 1
#  else
#    define NJ_OS_MACOS 1
#  endif
#  define NJ_OS_POSIX 1
#else
#  define NJ_OS_LINUX 1
#  define NJ_OS_POSIX 1
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#  define NJ_ARCH_ARM64 1
#elif defined(__x86_64__) || defined(_M_X64)
#  define NJ_ARCH_X64 1
#endif

// Le rendu passe en OpenGL ES sur mobile. Les shaders TEV generes n'utilisent que
// `layout(location=)`, `in`/`out`, `uniform`, `mat4` et des tableaux - tout est legal en
// GLSL ES 300, donc seule la ligne de version et les qualificatifs de precision changent
// (voir gx/tev_shader_gen.cpp).
#if defined(NJ_MOBILE)
#  define NJ_GLES 1
#endif

// ---------------------------------------------------------------- sommeil

#if defined(NJ_OS_WINDOWS)
#  include <windows.h>
#else
#  include <ctime>
#endif

namespace nj {

inline void sleep_ms(unsigned ms) {
#if defined(NJ_OS_WINDOWS)
    ::Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    ::nanosleep(&ts, nullptr);
#endif
}

// ---------------------------------------------------------------- flottant -> entier

// PPC `fctiw`/`fctiwz` rendent 0x80000000 pour tout ce qui ne tient pas dans un s32, y compris
// NaN. Un `(int32_t)f` du C rend cela en x86 par accident - `cvttss2si` produit exactement la
// meme valeur sentinelle - mais **pas** sur arm64, ou `fcvtzs` **sature** a INT32_MIN / INT32_MAX
// et rend 0 pour NaN. Le code recompile ne fait pas ces conversions lui-meme (le recompilateur
// emet ses propres aides), mais tout code hote qui imite le PPC doit passer par ici plutot que
// par un cast, sans quoi il sera juste sur une machine et faux sur l'autre.
inline int32_t cvt_f2i_ppc(double f) {
    if (!(f >= -2147483648.0 && f <= 2147483647.0))   // faux aussi pour NaN
        return (int32_t)0x80000000;
    return (int32_t)f;
}

}  // namespace nj

// ---------------------------------------------------------------- systeme de fichiers hote
//
// Sur iOS et Android une application n'ecrit que dans son bac a sable, et ne lit un fichier
// choisi par l'utilisateur qu'a travers le selecteur du systeme. Deux consequences pour ce
// runtime, et elles sont petites parce que tout le reste passe deja par la configuration ou par
// une variable d'environnement : un seul chemin absolu etait ecrit en dur dans les sources
// (`C:/re4n/hangdump.bin`, le vidage de blocage), et le disque lui-meme vient de
// `Config::game_dir`, qu'il suffit de renseigner.
//
// La coque mobile appelle donc `set_writable_dir()` au demarrage avec le repertoire du bac a
// sable, puis remplit `Config::game_dir` avec l'ISO que l'utilisateur vient de designer.

#include <string>

namespace nj {

std::string &writable_dir_storage();

inline void set_writable_dir(const std::string &d) { writable_dir_storage() = d; }

// Rend `<bac a sable>/<name>`, ou simplement `name` sur desktop quand rien n'a ete defini -
// ainsi les outils de debug gardent leur comportement d'avant sur PC.
inline std::string writable_path(const char *name) {
    const std::string &d = writable_dir_storage();
    if (d.empty()) return std::string(name);
    const char last = d.back();
    return d + (last == '/' || last == '\\' ? "" : "/") + name;
}

}  // namespace nj

