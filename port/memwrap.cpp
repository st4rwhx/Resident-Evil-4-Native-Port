// -*- coding: utf-8 -*-
// Mesure (13/11/2026, lenteur de r100) : 11 200 defauts de page par seconde sur le fil du jeu, 72 % dans memset.
// Le lien remplace memset / calloc / malloc de NOTRE code par ces fonctions (-Wl,--wrap=...) ; les gros appels
// (>= 64 Ko) sont comptes par appelant et imprimes toutes les 10 s : [MEMWRAP] appelant (adresse dans libnjcore.so),
// nombre d'appels, octets. Rien n'est change au comportement.
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <dlfcn.h>

extern "C" {
void *__real_memset(void *d, int c, size_t n);
void *__real_calloc(size_t a, size_t b);
void *__real_malloc(size_t n);
}

namespace {
const size_t kBig = 64 * 1024;
struct Slot {
    std::atomic<uintptr_t> caller{0};
    std::atomic<unsigned> calls{0};
    std::atomic<unsigned long long> bytes{0};
    char kind = 0;
};
Slot g_slots[64];
std::atomic<long> g_last{0};
uintptr_t g_base = 0;

void note(char kind, uintptr_t caller, size_t n)
{
    for (int i = 0; i < 64; ++i) {
        uintptr_t cur = g_slots[i].caller.load(std::memory_order_relaxed);
        if (cur == 0) {
            uintptr_t z = 0;
            if (!g_slots[i].caller.compare_exchange_strong(z, caller))
                cur = z;
            else {
                g_slots[i].kind = kind;
                cur = caller;
            }
        }
        if (cur == caller) {
            g_slots[i].calls.fetch_add(1, std::memory_order_relaxed);
            g_slots[i].bytes.fetch_add(n, std::memory_order_relaxed);
            break;
        }
    }
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const long now = ts.tv_sec;
    long last = g_last.load(std::memory_order_relaxed);
    if (now - last >= 10 && g_last.compare_exchange_strong(last, now)) {
        if (!g_base) {
            Dl_info di;
            if (dladdr((void *) &note, &di))
                g_base = (uintptr_t) di.dli_fbase;
        }
        for (int i = 0; i < 64; ++i) {
            const uintptr_t c = g_slots[i].caller.load();
            if (!c)
                break;
            const unsigned k = g_slots[i].calls.exchange(0);
            const unsigned long long b = g_slots[i].bytes.exchange(0);
            if (k)
                std::printf("[MEMWRAP] %c appelant +0x%lx : %u appels, %llu Ko en 10 s\n", g_slots[i].kind,
                            (unsigned long) (c - g_base), k, b / 1024);
        }
    }
}
}  // namespace

extern "C" {
void *__wrap_memset(void *d, int c, size_t n)
{
    if (n >= kBig)
        note('S', (uintptr_t) __builtin_return_address(0), n);
    return __real_memset(d, c, n);
}
void *__wrap_calloc(size_t a, size_t b)
{
    if (a * b >= kBig)
        note('C', (uintptr_t) __builtin_return_address(0), a * b);
    return __real_calloc(a, b);
}
void *__wrap_malloc(size_t n)
{
    if (n >= kBig)
        note('M', (uintptr_t) __builtin_return_address(0), n);
    return __real_malloc(n);
}
}
