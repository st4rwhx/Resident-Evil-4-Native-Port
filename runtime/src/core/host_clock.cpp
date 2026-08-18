#include "runtime/host_clock.h"
#include "runtime/log.h"
#include "runtime/env.h"

#include <cstdio>
#include <cstdlib>
#include <thread>
#if defined(__ANDROID__) && defined(__arm__)
#include <csetjmp>
#include <csignal>
#endif

namespace nj {

TscClock g_tsc_clock;
std::atomic<int64_t> g_coarse_us{0};
std::atomic<bool> g_coarse_on{false};

// Fil d'horloge (voir host_clock.h). NJ_COARSECLK_US=<periode> (defaut 100) ; 0 : coupe.
static const char *start_coarse_clock() {
    const char *e = NJ_ENV("NJ_COARSECLK_US");
    const long period = e ? std::atol(e) : 100;
    if (period <= 0) return "steady_clock (horloge publiee coupee)";
    g_coarse_us.store(steady_us());
    std::thread([period] {
        for (;;) {
            const int64_t t = steady_us();
            if (t > g_coarse_us.load(std::memory_order_relaxed))
                g_coarse_us.store(t, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::microseconds(period));
        }
    }).detach();
    g_coarse_on.store(true);
    return "horloge publiee par un fil";
}

namespace {

#if defined(__ANDROID__) && defined(__arm__)
// Lecture d'essai de CNTVCT : un noyau qui ne l'ouvre pas a l'espace utilisateur leve SIGILL.
sigjmp_buf s_probe_jmp;
void on_probe_sigill(int) { siglongjmp(s_probe_jmp, 1); }
bool counter_readable() {
    struct sigaction sa {}, old {};
    sa.sa_handler = on_probe_sigill;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGILL, &sa, &old);
    bool ok = false;
    if (sigsetjmp(s_probe_jmp, 1) == 0) {
        const uint64_t a = nj_hw_counter();
        const uint64_t b = nj_hw_counter();
        ok = a != 0 && b >= a;
    }
    sigaction(SIGILL, &old, nullptr);
    return ok;
}
#else
bool counter_readable() { return true; }
#endif

// Etalonnage avant main() : 30 ms entre deux lectures couplees (compteur, steady_clock).
// Une erreur de pente de 1e-4 decale la base de temps de 0,1 ms par seconde : sans effet sur
// les alarmes et le decrementeur, qui mesurent des ecarts courts.
} // namespace

// Etalonnage explicite (Android : appele depuis main, une fois le journal branche ; l'essai de
// lecture du compteur y est journalise). Sans effet si l'horloge est deja etalonnee.
const char *host_clock_calibrate() {
    if (g_tsc_clock.on) return "deja actif";
#ifdef NJ_TSC_CLOCK
    if (NJ_ENV("NJ_SLOWCLOCK")) return "coupe (NJ_SLOWCLOCK)";
    if (!counter_readable()) return start_coarse_clock();   // compteur illisible (SIGILL)
    const uint64_t t0 = nj_hw_counter();
    const int64_t s0 = steady_us();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const uint64_t t1 = nj_hw_counter();
    const int64_t s1 = steady_us();
    if (t1 <= t0 || s1 <= s0) return "compteur immobile";
    g_tsc_clock.tsc0 = t1;
    g_tsc_clock.us0 = s1;
    g_tsc_clock.us_per_tick = (double)(s1 - s0) / (double)(t1 - t0);
    g_tsc_clock.on = true;
    return "compteur materiel actif";
#else
    return "pas de compteur sur cette plateforme";
#endif
}

namespace {

struct Calibrate {
    Calibrate() {
#if defined(NJ_TSC_CLOCK) && !defined(__ANDROID__)
        if (NJ_ENV("NJ_SLOWCLOCK"))   // A/B ; coupe dans le build de diffusion
            return;
        if (!counter_readable())
            return;
        const uint64_t t0 = nj_hw_counter();
        const int64_t s0 = steady_us();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        const uint64_t t1 = nj_hw_counter();
        const int64_t s1 = steady_us();
        if (t1 <= t0 || s1 <= s0)
            return;
        g_tsc_clock.tsc0 = t1;
        g_tsc_clock.us0 = s1;
        g_tsc_clock.us_per_tick = (double)(s1 - s0) / (double)(t1 - t0);
        g_tsc_clock.on = true;
        NJ_PRINTF("[HostClock] compteur materiel actif (%.4f us par pas)\n",
                    g_tsc_clock.us_per_tick);
#endif
    }
} s_calibrate;

} // namespace
} // namespace nj

#ifdef _WIN32
#include <windows.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#endif

namespace nj {
void precise_sleep_us(int us) {
    if (us <= 0) return;
#ifdef _WIN32
    // Un minuteur par fil ; sans le drapeau haute resolution (Windows < 10 1803), minuteur ordinaire.
    static thread_local HANDLE t = [] {
        HANDLE h = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                          TIMER_ALL_ACCESS);
        return h ? h : CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    }();
    LARGE_INTEGER due;
    due.QuadPart = -(LONGLONG)us * 10;   // relatif, en 100 ns
    if (t && SetWaitableTimer(t, &due, 0, nullptr, nullptr, FALSE)) {
        WaitForSingleObject(t, INFINITE);
        return;
    }
#endif
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}
} // namespace nj
