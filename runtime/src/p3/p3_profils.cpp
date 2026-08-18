// P3 : profils de version embarques (tools/p3/gen_profil.py). Chaque profil est le TOML que le
// traducteur lit (fonctions sans les noms de Capcom, sites gardes, modules REL...), chiffre dans le
// binaire et dechiffre en memoire a la demande. Les fichiers profil_<nom>.inc sont fabriques au
// build (NJ_P3_PROFILS), jamais commites.
#include <string>

#include "runtime/game_version.h"

#if __has_include("profil_debug.inc")
#include "profil_debug.inc"
#define NJ_PROFIL_DEBUG 1
#endif
#if __has_include("profil_usa.inc")
#include "profil_usa.inc"
#define NJ_PROFIL_USA 1
#endif
#if __has_include("profil_eur.inc")
#include "profil_eur.inc"
#include "runtime/obf.h"
#include "runtime/diag.h"
using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire
#define NJ_PROFIL_EUR 1
#endif

namespace nj::runtime::p3 {
namespace {

std::string decrypt(const unsigned char* d, unsigned n, unsigned long long seed) {
    std::string out(n, '\0');
    unsigned long long x = seed;
    const volatile unsigned char* src = d;   // pas de dechiffrement a la compilation
    for (unsigned i = 0; i < n; ++i) {
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        const unsigned long long k = x * 0x2545F4914F6CDD1DULL;
        out[i] = (char)(src[i] ^ (unsigned char)(k >> 56));
    }
    return out;
}

}  // namespace

// Profil de la version `v` (texte TOML) ; vide si ce build ne l'embarque pas.
std::string profile_text(GameVersion v) {
    switch (v) {
#ifdef NJ_PROFIL_DEBUG
    case GameVersion::Debug: return decrypt(kData_debug, kSize_debug, kSeed_debug);
#endif
#ifdef NJ_PROFIL_USA
    case GameVersion::Usa: return decrypt(kData_usa, kSize_usa, kSeed_usa);
#endif
#ifdef NJ_PROFIL_EUR
    case GameVersion::Europe: return decrypt(kData_eur, kSize_eur, kSeed_eur);
#endif
    default: return {};
    }
}

}  // namespace nj::runtime::p3
