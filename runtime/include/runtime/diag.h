// Messages de diagnostic sans texte dans le build de diffusion (12/10).
//
// NJ_DIAG("format", args...) : en developpement, fprintf(stderr, ...) comme avant. En diffusion
// (NJ_RELEASE), le format n'entre pas dans le binaire : seule son empreinte (FNV-1a 32, calculee a
// la compilation) et les valeurs vont au journal chiffre (perf_journal, ligne « D »).
// tools/p3/journal_perf.py retrouve le texte en calculant la meme empreinte sur les sources.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <type_traits>

namespace nj::diag {

consteval uint32_t code(const char *s) {
    uint32_t h = 2166136261u;
    for (; *s; ++s) h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}

// perf_journal.cpp : « D <t> <code> <valeurs> »
void record(uint32_t code, const std::string &values);

template <class T>
void append(std::string &out, const T &v) {
    char b[48];
    using U = std::decay_t<T>;
    if constexpr (std::is_floating_point_v<U>) std::snprintf(b, sizeof b, " %.3g", (double)v);
    else if constexpr (std::is_integral_v<U> && std::is_signed_v<U>) std::snprintf(b, sizeof b, " %lld", (long long)v);
    else if constexpr (std::is_integral_v<U>) std::snprintf(b, sizeof b, " %llu", (unsigned long long)v);
    else if constexpr (std::is_convertible_v<U, const char *>) {
        // valeurs de la machine (version du jeu, module) : 40 caracteres au plus, sans espace
        std::string s = v ? std::string(v).substr(0, 40) : std::string("-");
        for (char &c : s)
            if (c == ' ' || c == '\n' || c == '\r') c = '_';
        if (s.empty() || s == "\n") s = "-";
        out += ' ';
        out += s;
        return;
    } else if constexpr (std::is_same_v<U, std::string>) {
        append(out, v.c_str());
        return;
    } else {
        std::snprintf(b, sizeof b, " ?");
    }
    out += b;
}

template <class... A>
void emit(uint32_t c, const A &...a) {
    std::string v;
    (append(v, a), ...);
    record(c, v);
}

}  // namespace nj::diag

#if defined(NJ_RELEASE)
#define NJ_DIAG(fmt, ...) ::nj::diag::emit(::nj::diag::code(fmt) __VA_OPT__(, ) __VA_ARGS__)
#else
#define NJ_DIAG(...) std::fprintf(stderr, __VA_ARGS__)
#endif
