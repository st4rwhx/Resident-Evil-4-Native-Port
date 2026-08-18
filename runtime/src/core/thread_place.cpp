#include "runtime/thread_place.h"

#include "runtime/env.h"
#include "runtime/obf.h"
#include "runtime/perf_journal.h"
using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <string>
#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/syscall.h>
#include <unistd.h>
#endif
#if defined(__linux__)
#include <dlfcn.h>
#include <sched.h>
#include <sys/resource.h>
#endif

namespace nj::runtime::place {
namespace {

// Fils suivis dans le journal : jeu, lecture du flux, hote (rendu).
struct Fil {
    char tag = 0;   // 'j', 'l', 'h'
    int tid = 0;
#if defined(_WIN32)
    HANDLE h = nullptr;
#endif
    uint64_t last_ns = 0;
    uint64_t last_wait = 0, last_vol = 0, last_maj = 0;   // phase A : schedstat, commutations volontaires, defauts majeurs
};
std::mutex g_mu;
Fil g_fils[3];
int g_nfils = 0;
std::atomic<int> g_tid_jeu{0}, g_tid_lecture{0};

bool placement_on() {
    static const bool on = [] {
        const char *e = NJ_ENV("NJ_PLACE");
        return !(e && std::atoi(e) == 0);
    }();
    return on;
}

#if defined(__linux__)
int current_tid() { return (int)syscall(SYS_gettid); }

long read_long(const char *path) {
    FILE *f = std::fopen(path, "r"_g);
    if (!f) return -1;
    long v = -1;
    if (std::fscanf(f, "%ld", &v) != 1) v = -1;
    std::fclose(f);
    return v;
}

// Gros coeurs = capacite >= 60 % de la plus forte (cpu_capacity, sinon frequence maximale).
// Machine homogene ou topologie illisible : ok = false, on ne place rien.
struct Topo {
    cpu_set_t big, little;
    int nbig = 0, nlittle = 0, first_big = -1;
    bool ok = false;
};
const Topo &topo() {
    static const Topo t = [] {
        Topo r;
        CPU_ZERO(&r.big);
        CPU_ZERO(&r.little);
        long n = sysconf(_SC_NPROCESSORS_CONF);
        if (n <= 1) return r;
        if (n > 32) n = 32;
        long cap[32] = {}, best = 0;
        char path[96];
        for (long i = 0; i < n; ++i) {
            std::snprintf(path, sizeof path, "/sys/devices/system/cpu/cpu%ld/cpu_capacity"_g, i);
            cap[i] = read_long(path);
            if (cap[i] <= 0) {
                std::snprintf(path, sizeof path, "/sys/devices/system/cpu/cpu%ld/cpufreq/cpuinfo_max_freq"_g, i);
                cap[i] = read_long(path);
            }
            if (cap[i] > best) best = cap[i];
        }
        if (best <= 0) return r;
        for (long i = 0; i < n; ++i) {
            if (cap[i] <= 0) continue;
            if (cap[i] * 10 >= best * 6) {
                CPU_SET(i, &r.big);
                ++r.nbig;
                if (r.first_big < 0) r.first_big = (int)i;
            } else {
                CPU_SET(i, &r.little);
                ++r.nlittle;
            }
        }
        r.ok = r.nbig > 0 && r.nlittle > 0;
        return r;
    }();
    return t;
}

// Fil courant restreint a `want` (dans la limite de ce que le systeme lui permet deja).
// 27/10 : on part de l'affinite du PROCESSUS (fil principal, jamais restreint), pas de celle du fil createur.
// Avant, le fil hote se limitait a {0,3}, puis le fil du jeu (cree apres, il heritait de {0,3}) croisait son {1,2} avec {0,3} :
// intersection vide, aucune restriction ; le fil de lecture ({2,3}) finissait sur le seul coeur 3 et ses fils de decodage
// heritaient de {3}. Resultat mesure sur le S5 : coeurs 1 et 2 inutilises (204 % d'inactivite), fil du jeu 57 % du temps pret sans coeur.
void restrict_to(const cpu_set_t &want) {
    cpu_set_t cur;
    CPU_ZERO(&cur);
    if (sched_getaffinity(getpid(), sizeof cur, &cur) != 0) return;
    cpu_set_t both;
    CPU_AND(&both, &cur, &want);
    if (CPU_COUNT(&both) > 0) sched_setaffinity(0, sizeof both, &both);
}

uint64_t thread_cpu_ns(const Fil &f, int *last_cpu) {
    char path[64];
    std::snprintf(path, sizeof path, "/proc/self/task/%d/stat"_g, f.tid);
    FILE *fp = std::fopen(path, "r"_g);
    if (!fp) return 0;
    char buf[1024];
    const size_t n = std::fread(buf, 1, sizeof buf - 1, fp);
    std::fclose(fp);
    buf[n] = 0;
    const char *p = std::strrchr(buf, ')');   // le nom du fil peut contenir des espaces
    if (!p) return 0;
    // apres ')' : champ 3 (etat) a l'indice 0 ; utime = 14, stime = 15, processeur = 39
    unsigned long long ut = 0, st = 0;
    int cpu = -1, idx = 0;
    for (const char *q = p + 1; *q && idx <= 36;) {
        while (*q == ' ') ++q;
        if (!*q) break;
        if (idx == 11) ut = std::strtoull(q, nullptr, 10);
        if (idx == 12) st = std::strtoull(q, nullptr, 10);
        if (idx == 36) cpu = std::atoi(q);
        while (*q && *q != ' ') ++q;
        ++idx;
    }
    if (last_cpu) *last_cpu = cpu;
    static const long hz = sysconf(_SC_CLK_TCK) > 0 ? sysconf(_SC_CLK_TCK) : 100;
    return (ut + st) * (1000000000ull / (uint64_t)hz);
}

// Phase A : ou le fil NE tourne PAS. schedstat = (temps sur le coeur, temps pret mais sans coeur, tranches) ;
// « voluntary_ctxt_switches » = fois ou il s'est endormi (verrou, sommeil, attente) ; defauts majeurs = lectures disque/swap.
void sched_extra(int tid, uint64_t &wait_ns, uint64_t &vol, uint64_t &maj) {
    char path[64];
    wait_ns = vol = maj = 0;
    std::snprintf(path, sizeof path, "/proc/self/task/%d/schedstat"_g, tid);
    if (FILE *fp = std::fopen(path, "r"_g)) {
        unsigned long long run = 0, wt = 0, ts = 0;
        if (std::fscanf(fp, "%llu %llu %llu", &run, &wt, &ts) >= 2) wait_ns = wt;
        std::fclose(fp);
    }
    std::snprintf(path, sizeof path, "/proc/self/task/%d/status"_g, tid);
    if (FILE *fp = std::fopen(path, "r"_g)) {
        char line[160];
        while (std::fgets(line, sizeof line, fp)) {
            unsigned long long v = 0;
            if (std::sscanf(line, "voluntary_ctxt_switches: %llu"_g, &v) == 1) vol = v;
        }
        std::fclose(fp);
    }
    std::snprintf(path, sizeof path, "/proc/self/task/%d/stat"_g, tid);
    if (FILE *fp = std::fopen(path, "r"_g)) {
        char buf[1024];
        const size_t n = std::fread(buf, 1, sizeof buf - 1, fp);
        std::fclose(fp);
        buf[n] = 0;
        if (const char *p = std::strrchr(buf, ')')) {
            int idx = 0;
            for (const char *q = p + 1; *q && idx <= 9;) {   // apres ')' : champ 3 a l'indice 0 ; majflt = champ 12 = indice 9
                while (*q == ' ') ++q;
                if (!*q) break;
                if (idx == 9) maj = std::strtoull(q, nullptr, 10);
                while (*q && *q != ' ') ++q;
                ++idx;
            }
        }
    }
}
#elif defined(_WIN32)
int current_tid() { return (int)GetCurrentThreadId(); }
uint64_t thread_cpu_ns(const Fil &f, int *last_cpu) {
    if (last_cpu) *last_cpu = -1;
    FILETIME c, e, k, u;
    if (!f.h || !GetThreadTimes(f.h, &c, &e, &k, &u)) return 0;
    const uint64_t kk = ((uint64_t)k.dwHighDateTime << 32) | k.dwLowDateTime;
    const uint64_t uu = ((uint64_t)u.dwHighDateTime << 32) | u.dwLowDateTime;
    return (kk + uu) * 100;
}
#else
int current_tid() { return 0; }
uint64_t thread_cpu_ns(const Fil &, int *last_cpu) {
    if (last_cpu) *last_cpu = -1;
    return 0;
}
#endif

// ---- ADPF (Android 13+) et marge thermique (Android 11+), charges a l'execution -------------
#if defined(__ANDROID__)
using HintMgrFn = void *(*)();
using HintCreateFn = void *(*)(void *, const int32_t *, size_t, int64_t);
using HintReportFn = int (*)(void *, int64_t);
using ThermMgrFn = void *(*)();
using ThermHeadroomFn = float (*)(void *, int);

void *libandroid() {
    static void *h = dlopen("libandroid.so"_g, RTLD_NOW);
    return h;
}

void *g_session = nullptr;
HintReportFn g_report = nullptr;
bool g_session_tried = false;

void try_session() {
    if (g_session_tried) return;
    const int j = g_tid_jeu.load(), l = g_tid_lecture.load();
    if (!j || !l) return;
    g_session_tried = true;
    void *lib = libandroid();
    if (!lib) return;
    auto get = (HintMgrFn)dlsym(lib, "APerformanceHint_getManager"_g);
    auto create = (HintCreateFn)dlsym(lib, "APerformanceHint_createSession"_g);
    g_report = (HintReportFn)dlsym(lib, "APerformanceHint_reportActualWorkDuration"_g);
    if (!get || !create || !g_report) return;
    void *mgr = get();
    if (!mgr) return;
    const int32_t tids[2] = {j, l};
    g_session = create(mgr, tids, 2, 33333333);   // une image a 30 i/s
    nj::perf::event(g_session ? "adpf on"_g : "adpf refuse"_g);
}

float thermal_headroom() {
    static ThermHeadroomFn fn = nullptr;
    static void *mgr = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        if (void *lib = libandroid()) {
            auto acq = (ThermMgrFn)dlsym(lib, "AThermal_acquireManager"_g);
            fn = (ThermHeadroomFn)dlsym(lib, "AThermal_getThermalHeadroom"_g);
            if (acq) mgr = acq();
        }
    }
    if (!fn || !mgr) return NAN;
    return fn(mgr, 0);
}
#endif

}  // namespace

unsigned big_cores() {
#if defined(__linux__)
    const Topo &t = topo();
    if (t.ok) return (unsigned)t.nbig;
#endif
    return std::max(1u, std::thread::hardware_concurrency());
}

void thread_started(const char *name) {
    if (!name) return;
    char tag = 0;
    const bool jeu = !std::strcmp(name, "jeu"_g), lecture = !std::strcmp(name, "lecture"_g);
    if (jeu) tag = 'j';
    else if (lecture) tag = 'l';
    else if (!std::strcmp(name, "hote"_g)) tag = 'h';
    const int tid = current_tid();
    if (jeu) g_tid_jeu = tid;
    if (lecture) g_tid_lecture = tid;

#if defined(__linux__)
    if (jeu || lecture || tag == 'h') {
        setpriority(PRIO_PROCESS, 0, -10);
    }
    const Topo &t = topo();
    // perf::event prend le verrou du journal : jamais depuis second_fields / frame (verrou tenu)
    static const bool announced = [&] {
        char ev[96];
        std::snprintf(ev, sizeof ev, "place big=%d little=%d on=%d"_g, t.nbig, t.nlittle,
                      (int)(t.ok && placement_on()));
        nj::perf::event(ev);
        return true;
    }();
    (void)announced;
    if (placement_on()) {
        if (t.ok) {
            if (jeu || lecture) restrict_to(t.big);
            else if (!std::strcmp(name, "nettoyage"_g) || !std::strcmp(name, "texhd"_g)) restrict_to(t.little);
        } else {
            // Machine homogene (ex: Snapdragon 805 4 coeurs Krait 450 sur Galaxy S5+)
            long n = sysconf(_SC_NPROCESSORS_CONF);
            if (n >= 4) {
                cpu_set_t aff;
                CPU_ZERO(&aff);
                if (jeu) {
                    CPU_SET(1, &aff);
                    CPU_SET(2, &aff);
                    restrict_to(aff);
                } else if (lecture) {
                    CPU_SET(2, &aff);
                    CPU_SET(3, &aff);
                    restrict_to(aff);
                } else if (tag == 'h') {
                    CPU_SET(0, &aff);
                    CPU_SET(3, &aff);
                    restrict_to(aff);
                }
            }
        }
    }
#endif

    if (!tag) return;
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_nfils >= 3) return;
    Fil &f = g_fils[g_nfils];
    f.tag = tag;
    f.tid = tid;
#if defined(_WIN32)
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &f.h, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
#endif
    f.last_ns = thread_cpu_ns(f, nullptr);
    ++g_nfils;
}

void frame_work(float game_ms) {
#if defined(__ANDROID__)
    if (!placement_on()) return;
    try_session();
    if (g_session && game_ms > 0.0f) g_report(g_session, (int64_t)(game_ms * 1.0e6f));
#else
    (void)game_ms;
#endif
}

std::string second_fields() {
    static auto last = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    const double wall_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(now - last).count();
    last = now;
    static bool premiere = true;   // 1re ligne : fenetre sans mesure (last vient d'etre pris)
    const bool fenetre_ok = !premiere && wall_ns > 0.5e9;
    premiere = false;
    std::string out;
    char b[48];
    {
        std::lock_guard<std::mutex> lk(g_mu);
        // c<x> = temps processeur du fil en % d'un coeur ; k<x> = dernier coeur ou il a tourne
        for (int i = 0; i < g_nfils; ++i) {
            Fil &f = g_fils[i];
            int cpu = -1;
            const uint64_t ns = thread_cpu_ns(f, &cpu);
            if (fenetre_ok && ns >= f.last_ns) {
                std::snprintf(b, sizeof b, " c%c=%.0f"_g, f.tag, (double)(ns - f.last_ns) / wall_ns * 100.0);
                out += b;
            }
            f.last_ns = ns;
#if defined(__linux__)
            if (fenetre_ok) {   // w = % du temps pret sans coeur ; v = endormissements/s ; f = defauts majeurs/s
                uint64_t wt = 0, vol = 0, maj = 0;
                sched_extra(f.tid, wt, vol, maj);
                if (wt >= f.last_wait && vol >= f.last_vol && maj >= f.last_maj) {
                    std::snprintf(b, sizeof b, " w%c=%.0f v%c=%.0f f%c=%.0f"_g, f.tag, (double)(wt - f.last_wait) / wall_ns * 100.0,
                                  f.tag, (double)(vol - f.last_vol) * 1.0e9 / wall_ns, f.tag, (double)(maj - f.last_maj) * 1.0e9 / wall_ns);
                    out += b;
                }
                f.last_wait = wt; f.last_vol = vol; f.last_maj = maj;
            }
#endif
            if (cpu >= 0) {
                std::snprintf(b, sizeof b, " k%c=%d"_g, f.tag, cpu);
                out += b;
            }
        }
    }
#if defined(__linux__)
    const Topo &t = topo();
    if (t.first_big >= 0) {   // frequence courante des gros coeurs (MHz)
        char path[96];
        std::snprintf(path, sizeof path, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq"_g, t.first_big);
        const long khz = read_long(path);
        if (khz > 0) {
            std::snprintf(b, sizeof b, " fb=%ld"_g, khz / 1000);
            out += b;
        }
    }
#endif
#if defined(__ANDROID__)
    // marge thermique : 1.0 = bridage severe ; demandee toutes les 5 s (le systeme limite la cadence)
    static int tick = 0;
    static float headroom = NAN;
    if (tick++ % 5 == 0) {
        const float h = thermal_headroom();
        if (!std::isnan(h)) headroom = h;
    }
    if (!std::isnan(headroom)) {
        std::snprintf(b, sizeof b, " th=%.2f"_g, headroom);
        out += b;
    }
#endif
    return out;
}

}  // namespace nj::runtime::place
