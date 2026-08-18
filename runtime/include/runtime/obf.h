// Chaines chiffrees a la compilation (P2, docs/SECURITE_ET_JURIDIQUE.md) : "texte"_g.
//
// Le binaire ne contient que les octets chiffres (argument de modele, en donnees en lecture seule).
// Le texte est dechiffre une fois, a la premiere utilisation, dans une chaine statique propre a ce
// litteral ; l'operateur rend un const char * stable, donc le code qui l'utilise ne change pas de
// type. La cle depend de l'heure de compilation : chaque build chiffre autrement (renouvellement).
// Sert au texte des nuanceurs GLSL (le savoir-faire du rendu), pas aux journaux (retires en diffusion).
#pragma once
#include <cstddef>
#include <string>

namespace nj::obf {

consteval unsigned seed() {
    const char t[] = __TIME__ __DATE__;
    unsigned h = 2166136261u;
    for (char c : t) h = (h ^ (unsigned char)c) * 16777619u;
    return h;
}

constexpr unsigned char key(std::size_t i) {
    unsigned x = seed() + 0x9E3779B9u * (unsigned)(i + 1);
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    return (unsigned char)x;
}

template <std::size_t N>
struct Chiffre {
    unsigned char d[N]{};
    consteval Chiffre(const char (&s)[N]) {
        for (std::size_t i = 0; i < N; ++i) d[i] = (unsigned char)s[i] ^ key(i);
    }
};

}  // namespace nj::obf

namespace nj::obf_litteraux {

template <nj::obf::Chiffre S>
const char *operator""_g() {
    static const std::string s = [] {
        constexpr std::size_t n = sizeof(S.d);
        // Lecture par un pointeur volatile : sans lui, le compilateur dechiffre a la compilation
        // (donnees et cle connues) et range le texte en clair dans le binaire (constate le 06/10).
        const volatile unsigned char *src = S.d;
#if defined(__GNUC__)
        // 12/10 : le volatile ne suffit pas a GCC pour un texte court (<= 15 octets, tampon interne de
        // std::string) : il transformait l'initialisation en donnee statique, texte en clair. Une
        // barriere asm opaque interdit tout calcul d'avance.
        __asm__ volatile("" : "+r"(src)::"memory");
#endif
        std::string r(n - 1, '\0');
        for (std::size_t i = 0; i + 1 < n; ++i) r[i] = (char)(src[i] ^ nj::obf::key(i));
        return r;
    }();
    return s.c_str();
}

}  // namespace nj::obf_litteraux
