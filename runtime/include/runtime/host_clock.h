#pragma once
// Horloge hote en microsecondes, sur la meme origine que std::chrono::steady_clock.
//
// La base de temps invitee (GuestCpu::timebase_now) est relue a chaque passage de
// drain_callbacks, soit ~800 000 fois par seconde. Sous MinGW, steady_clock::now()
// passe par clock_gettime de libwinpthread : 9 % du fil CPU au profil du village (27/09).
// Sur Windows x86-64 on lit le compteur du processeur (rdtsc, invariant sur tout processeur
// recent), converti par une pente mesuree au demarrage contre steady_clock. Ailleurs, ou avec
// NJ_SLOWCLOCK=1, steady_clock comme avant.
#include <atomic>
#include <chrono>
#include <ctime>
#include <cstdint>

#if defined(_WIN32) && defined(__x86_64__)
#include <x86intrin.h>
#define NJ_TSC_CLOCK 1
inline uint64_t nj_hw_counter() { return __rdtsc(); }
#elif defined(__ANDROID__) && defined(__arm__)
// Galaxy S5 (Krait, noyau 3.4 en 32 bits) : pas de vDSO, chaque steady_clock::now() est un appel
// systeme (~1 us). Profil du 29/09 : 22 % du fil du jeu. Le compteur du minuteur generique ARM
// (CNTVCT, 19,2 MHz) se lit en une instruction si le noyau l'ouvre a l'espace utilisateur ; sinon
// l'essai au demarrage (host_clock.cpp) le detecte et on reste sur steady_clock.
#define NJ_TSC_CLOCK 1
inline uint64_t nj_hw_counter() {
    uint32_t lo, hi;
    asm volatile("mrrc p15, 1, %0, %1, c14" : "=r"(lo), "=r"(hi));
    return ((uint64_t)hi << 32) | lo;
}
#endif

namespace nj {

struct TscClock {
    bool on = false;
    uint64_t tsc0 = 0;      // compteur a l'etalonnage
    int64_t us0 = 0;        // steady_clock (us depuis son origine) au meme instant
    double us_per_tick = 0; // pente mesuree
};
extern TscClock g_tsc_clock;   // host_clock.cpp, etalonne avant main()
// Horloge publiee par un fil (appareils sans compteur lisible : Galaxy S5, dont le noyau refuse
// CNTVCT a l'espace utilisateur). Le fil relit steady_clock toutes les ~100 us et publie la valeur
// en microsecondes ; host_coarse_us() la prend sans appel systeme ni division 64 bits.
extern std::atomic<int64_t> g_coarse_us;
extern std::atomic<bool> g_coarse_on;
// Etalonnage explicite (Android, depuis main) ; rend un etat lisible pour le journal.
const char *host_clock_calibrate();
// Sommeil court et precis (minuteur haute resolution sous Windows, ou le sommeil ordinaire
// arrondit a la milliseconde) : boucles d'attente du jeu (ios.cpp).
void precise_sleep_us(int us);

inline int64_t steady_us() {
#if (defined(__ANDROID__) || defined(__linux__)) && !defined(NJ_APPLE_CROSS)
    // (NJ_APPLE_CROSS : jeu iPhone compile avec les en-tetes Android, ou CLOCK_MONOTONIC vaut 1 : sur Apple cet
    // identifiant est invalide, l'heure restait figee et les boucles d'attente du jeu ne finissaient jamais)
    // Meme horloge que steady_clock (CLOCK_MONOTONIC, libc++), sans la division 64 bits
    // nanosecondes -> microsecondes (__udivmoddi4 sur ARM 32 bits) : 32 bits sur tv_nsec.
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + (int64_t)((uint32_t)ts.tv_nsec / 1000u);
#else
    return std::chrono::steady_clock::now().time_since_epoch() /
               std::chrono::microseconds(1);
#endif
}

inline int64_t host_now_us() {
#ifdef NJ_TSC_CLOCK
    const TscClock& c = g_tsc_clock;
    if (c.on) [[likely]]
        return c.us0 + (int64_t)((double)(nj_hw_counter() - c.tsc0) * c.us_per_tick);
#endif
    return steady_us();
}

// Heure publiee (precision ~100 us) pour les verifications internes du moteur, faites des
// centaines de milliers de fois par seconde (echeances d'evenements, decrementeur). Jamais pour ce
// que le jeu lit lui-meme (mftb, OSGetTime, registres AI / VI) : l'etalonnage audio du SDK
// (__AI_SRC_INIT) mesure un ecart de ~30 us et boucle sans fin sur une heure a 100 us (29/09).
inline int64_t host_coarse_us() {
    if (g_coarse_on.load(std::memory_order_relaxed)) [[likely]]
        return g_coarse_us.load(std::memory_order_relaxed);
    return host_now_us();
}

} // namespace nj
