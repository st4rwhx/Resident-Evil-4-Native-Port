// Appelants des divisions 64 bits logicielles (ARMv7 32 bits : __aeabi_uldivmod / __aeabi_ldivmod,
// puis __udivmoddi4). Le profileur SIGPROF ne les retrouve pas : dans __udivmoddi4 le registre de
// retour est deja reutilise. Construit seulement avec -DNJ_DIVPROF=ON (android/CMakeLists.txt,
// edition de liens --wrap) : chaque appel passe par une enveloppe qui compte son adresse de retour.
// Tous les NJ_DIVPROF_N appels (defaut 500 000), les 16 premiers appelants partent au journal
// (NJ DIVPROF), en decalage dans libmain.so : llvm-addr2line -f -e libmain.so <decalage>.
#if defined(__arm__) && !defined(__aarch64__)
#include <android/log.h>
#include <dlfcn.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>

namespace {
constexpr int kSlots = 4096;
std::atomic<uint32_t> g_key[kSlots];
std::atomic<uint32_t> g_cnt[kSlots];
std::atomic<uint32_t> g_total{0};
std::atomic<bool> g_dumping{false};

uint32_t period() {
    static const uint32_t n = [] {
        const char* s = std::getenv("NJ_DIVPROF_N");
        return s ? uint32_t(std::strtoul(s, nullptr, 10)) : 500000u;
    }();
    return n ? n : 500000u;
}

void dump() {
    struct E { uint32_t ra, n; };
    E top[16] = {};
    uint32_t sum = 0;
    for (int i = 0; i < kSlots; ++i) {
        const uint32_t n = g_cnt[i].exchange(0, std::memory_order_relaxed);
        const uint32_t ra = g_key[i].load(std::memory_order_relaxed);
        if (!n) continue;
        sum += n;
        if (n > top[15].n) {
            top[15] = {ra, n};
            std::sort(top, top + 16, [](const E& a, const E& b) { return a.n > b.n; });
        }
    }
    __android_log_print(ANDROID_LOG_INFO, "NJ", "[DIVPROF] %u divisions 64 bits", sum);
    for (const E& e : top) {
        if (!e.n) break;
        Dl_info di{};
        uintptr_t off = e.ra;
        const char* lib = "?";
        if (dladdr(reinterpret_cast<void*>(e.ra), &di) && di.dli_fbase) {
            off = e.ra - reinterpret_cast<uintptr_t>(di.dli_fbase);
            lib = di.dli_fname ? di.dli_fname : "?";
        }
        __android_log_print(ANDROID_LOG_INFO, "NJ", "[DIVPROF] %6.2f%% %08x %s %s", 100.0 * e.n / (sum ? sum : 1),
                            unsigned(off), di.dli_sname ? di.dli_sname : "-", lib);
    }
}
}  // namespace

extern "C" __attribute__((used)) void nj_divprof_hit(uint32_t ra) {
    uint32_t h = (ra * 2654435761u) >> 20;   // 12 bits
    for (int probe = 0; probe < 64; ++probe, h = (h + 1) & (kSlots - 1)) {
        uint32_t k = g_key[h].load(std::memory_order_relaxed);
        if (k == 0 && g_key[h].compare_exchange_strong(k, ra, std::memory_order_relaxed)) k = ra;
        if (k == ra) { g_cnt[h].fetch_add(1, std::memory_order_relaxed); break; }
    }
    if (g_total.fetch_add(1, std::memory_order_relaxed) % period() == period() - 1) {
        bool no = false;
        if (g_dumping.compare_exchange_strong(no, true)) { dump(); g_dumping.store(false); }
    }
}

// Enveloppes : garder r0-r3 (dividende, diviseur) et lr, noter lr, puis la vraie fonction.
#define NJ_DIVPROF_WRAP(name)                                   \
    asm(".text\n.arm\n.align 2\n.global __wrap_" #name "\n"      \
        ".type __wrap_" #name ", %function\n"                     \
        "__wrap_" #name ":\n"                                     \
        "  push {r0, r1, r2, r3, r4, r5, r12, lr}\n"              \
        "  mov r0, lr\n"                                          \
        "  bl nj_divprof_hit\n"                                 \
        "  pop {r0, r1, r2, r3, r4, r5, r12, lr}\n"               \
        "  b __real_" #name "\n"                                  \
        ".size __wrap_" #name ", .-__wrap_" #name "\n");
NJ_DIVPROF_WRAP(__aeabi_uldivmod)
NJ_DIVPROF_WRAP(__aeabi_ldivmod)
#endif
