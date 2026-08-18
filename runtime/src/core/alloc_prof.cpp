// NJ_SHAPE=1 : operator new/delete comptes et chronometres (rdtsc), separes entre le fil
// hote (celui qui analyse et rend) et les autres. Lu par shape_report (gx.cpp). Sert a savoir
// combien les allocations par dessin coutent vraiment : le profil du fil hote melange, dans
// ntdll, les allocations et les attentes (fils de decodage, sommeil de la boucle).
//
// Hors NJ_SHAPE, un test de drapeau par allocation. Windows seulement (le profil vient de la).
#include <cstdint>
#include "runtime/log.h"
#include <cstdlib>
#include <new>
#include <utility>

#include "runtime/env.h"

namespace nj {
struct AllocStats {
    uint64_t host_n = 0, host_ticks = 0, other_n = 0, other_ticks = 0;
};
AllocStats g_alloc_stats;       // fil hote : non atomique ; les autres : approximatif, suffisant
unsigned long g_host_tid = 0;   // pose par gx_pump_fifo (gx.cpp)
bool g_allocprof_on = false;    // pose au premier gx_pump_fifo si NJ_SHAPE
}

#if defined(_WIN32) && defined(__x86_64__)
#include <windows.h>
#include <x86intrin.h>

namespace {
// Une allocation sur 64 du fil hote : sa pile (5 niveaux au-dessus d'operator new). Table a
// adressage ouvert par pile complete ; alloc_sites_report l'imprime (adresses brutes, a nommer
// avec la table de symboles de l'exe : tools/re4/allocsites.py).
constexpr int kDepth = 5, kSlots = 4096;
struct Site { void* pc[kDepth]; uint64_t n; };
Site g_sites[kSlots];
uint64_t g_sites_lost = 0;

void sample_site() {
    void* fr[kDepth + 2] = {};
    const USHORT got = RtlCaptureStackBackTrace(2, kDepth, fr, nullptr);   // saute timed_alloc, new
    if (got == 0) return;
    uint64_t h = 1469598103934665603ull;
    for (int i = 0; i < kDepth; ++i) h = (h ^ (uint64_t)(uintptr_t)fr[i]) * 1099511628211ull;
    for (int k = 0; k < 16; ++k) {
        Site& e = g_sites[(h + k) & (kSlots - 1)];
        if (e.n == 0) {
            for (int i = 0; i < kDepth; ++i) e.pc[i] = fr[i];
            e.n = 1;
            return;
        }
        bool same = true;
        for (int i = 0; i < kDepth && same; ++i) same = e.pc[i] == fr[i];
        if (same) { ++e.n; return; }
    }
    ++g_sites_lost;
}

inline void* timed_alloc(std::size_t n) {
    if (!nj::g_allocprof_on) [[likely]]
        return std::malloc(n ? n : 1);
    const uint64_t t0 = __rdtsc();
    void* p = std::malloc(n ? n : 1);
    const uint64_t dt = __rdtsc() - t0;
    if (GetCurrentThreadId() == nj::g_host_tid) {
        ++nj::g_alloc_stats.host_n;
        nj::g_alloc_stats.host_ticks += dt;
        if ((nj::g_alloc_stats.host_n & 63) == 0)
            sample_site();
    } else {
        ++nj::g_alloc_stats.other_n;
        nj::g_alloc_stats.other_ticks += dt;
    }
    return p;
}
inline void timed_free(void* p) {
    if (!p) return;
    if (!nj::g_allocprof_on) [[likely]] {
        std::free(p);
        return;
    }
    const uint64_t t0 = __rdtsc();
    std::free(p);
    const uint64_t dt = __rdtsc() - t0;
    if (GetCurrentThreadId() == nj::g_host_tid)
        nj::g_alloc_stats.host_ticks += dt;
    else
        nj::g_alloc_stats.other_ticks += dt;
}
} // namespace

void* operator new(std::size_t n) {
    void* p = timed_alloc(n);
    if (!p) throw std::bad_alloc();
    return p;
}
void* operator new[](std::size_t n) {
    void* p = timed_alloc(n);
    if (!p) throw std::bad_alloc();
    return p;
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return timed_alloc(n); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return timed_alloc(n); }
void operator delete(void* p) noexcept { timed_free(p); }
void operator delete[](void* p) noexcept { timed_free(p); }
void operator delete(void* p, std::size_t) noexcept { timed_free(p); }
void operator delete[](void* p, std::size_t) noexcept { timed_free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { timed_free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { timed_free(p); }
#endif

#include <cstdio>
namespace nj {
// Les 25 piles les plus frequentes, adresses relatives a la base de l'exe.
void alloc_sites_report() {
#if defined(_WIN32) && defined(__x86_64__)
    const uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    int order[kSlots];
    int n = 0;
    for (int i = 0; i < kSlots; ++i)
        if (g_sites[i].n) order[n++] = i;
    for (int k = 0; k < n && k < 25; ++k) {
        int best = k;
        for (int j = k + 1; j < n; ++j)
            if (g_sites[order[j]].n > g_sites[order[best]].n) best = j;
        std::swap(order[k], order[best]);
        const Site& e = g_sites[order[k]];
        NJ_PRINTF("[ALLOCSITE] n=%llu", (unsigned long long)e.n);
        for (int i = 0; i < kDepth; ++i) {
            const uintptr_t a = (uintptr_t)e.pc[i];
            if (a >= base && a < base + 0x40000000)
                NJ_PRINTF(" +%llx", (unsigned long long)(a - base));
            else
                NJ_PRINTF(" ext:%llx", (unsigned long long)a);
        }
        NJ_PRINTF("\n");
    }
    NJ_PRINTF("[ALLOCSITE] piles perdues (table pleine) : %llu\n", (unsigned long long)g_sites_lost);
#endif
}
}
