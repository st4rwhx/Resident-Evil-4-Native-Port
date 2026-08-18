// Profileur par echantillonnage des fils (Android). Voir runtime/njprof.h.
#include "runtime/njprof.h"
#include "runtime/log.h"
#include "runtime/thread_place.h"

#if defined(__ANDROID__)
#include "runtime/env.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <sys/syscall.h>
#include <thread>
#include <ucontext.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace nj::runtime::njprof {
namespace {

constexpr int kMaxThreads = 16;
constexpr uint32_t kRing = 1u << 16;   // echantillons par fil entre deux lectures

struct Slot {
  std::atomic<int> tid{0};
  char name[16] = {0};
  uintptr_t ring[kRing];
  uintptr_t ring_lr[kRing];   // adresse de retour : l'appelant (fonctions feuilles, bibliotheques)
  std::atomic<uint32_t> head{0};
};
Slot g_slots[kMaxThreads];
std::atomic<int> g_nslots{0};
std::atomic<bool> g_started{false};

bool enabled() {
  static const bool on = NJ_ENV("NJ_APROF") != nullptr;
  return on;
}

// Gestionnaire de signal : rien d'autre qu'une ecriture dans un tableau deja alloue.
void on_sigprof(int, siginfo_t *, void *uc_void) {
  const int tid = (int)syscall(SYS_gettid);
  const int n = g_nslots.load(std::memory_order_acquire);
  for (int i = 0; i < n; ++i) {
    Slot &s = g_slots[i];
    if (s.tid.load(std::memory_order_relaxed) != tid) continue;
    auto *uc = (ucontext_t *)uc_void;
#if defined(__arm__)
    const uintptr_t pc = (uintptr_t)uc->uc_mcontext.arm_pc;
    const uintptr_t lr = (uintptr_t)uc->uc_mcontext.arm_lr;
#elif defined(__aarch64__)
    const uintptr_t pc = (uintptr_t)uc->uc_mcontext.pc;
    const uintptr_t lr = (uintptr_t)uc->uc_mcontext.regs[30];
#else
    const uintptr_t pc = 0, lr = 0;
#endif
    const uint32_t h = s.head.load(std::memory_order_relaxed);
    s.ring[h & (kRing - 1)] = pc;
    s.ring_lr[h & (kRing - 1)] = lr;
    s.head.store(h + 1, std::memory_order_release);
    return;
  }
}

void sampler_loop() {
  Dl_info self{};
  dladdr((void *)&sampler_loop, &self);
  const uintptr_t main_base = (uintptr_t)self.dli_fbase;
  const pid_t pid = getpid();
  std::vector<uint32_t> tail(kMaxThreads, 0);
  std::vector<std::unordered_map<uintptr_t, uint32_t>> hist(kMaxThreads);
  std::vector<std::unordered_map<uintptr_t, uint32_t>> hist_lr(kMaxThreads);
  // NJ_APROF_FOCUS=<debut>,<fin> (decalages hexadecimaux dans libmain.so) : pour les
  // echantillons pris dans cette plage (une fonction), qui l'appelait (adresse de retour).
  std::vector<std::unordered_map<uintptr_t, uint32_t>> hist_focus(kMaxThreads);
  uintptr_t focus_lo = 0, focus_hi = 0;
  if (const char *f = NJ_ENV("NJ_APROF_FOCUS")) {
    focus_lo = (uintptr_t)std::strtoul(f, nullptr, 16);
    if (const char *c = std::strchr(f, ',')) focus_hi = (uintptr_t)std::strtoul(c + 1, nullptr, 16);
  }
  std::vector<uint64_t> total(kMaxThreads, 0);
  auto last = std::chrono::steady_clock::now();
  for (;;) {
    std::this_thread::sleep_for(std::chrono::microseconds(1000));
    const int n = g_nslots.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i) {
      const int tid = g_slots[i].tid.load(std::memory_order_relaxed);
      if (tid) syscall(SYS_tgkill, pid, tid, SIGPROF);
    }
    // Relever les anneaux (hors du gestionnaire : ici on peut allouer).
    for (int i = 0; i < n; ++i) {
      Slot &s = g_slots[i];
      const uint32_t h = s.head.load(std::memory_order_acquire);
      for (uint32_t k = tail[i]; k != h; ++k) {
        const uintptr_t spc = s.ring[k & (kRing - 1)];
        ++hist[i][spc];
        ++hist_lr[i][s.ring_lr[k & (kRing - 1)]];
        if (focus_hi && spc - main_base >= focus_lo && spc - main_base < focus_hi)
          ++hist_focus[i][s.ring_lr[k & (kRing - 1)]];
        ++total[i];
      }
      tail[i] = h;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - last < std::chrono::seconds(10)) continue;
    last = now;
    for (int i = 0; i < n; ++i) {
      if (!total[i]) continue;
      // Regrouper : dans libmain.so par adresse (a 4 octets pres), ailleurs par bibliotheque.
      std::unordered_map<std::string, uint32_t> per_lib;
      std::vector<std::pair<uint32_t, uintptr_t>> ours;
      std::unordered_map<uintptr_t, uint32_t> ours_map;
      for (const auto &kv : hist[i]) {
        Dl_info di{};
        if (dladdr((void *)kv.first, &di) && (uintptr_t)di.dli_fbase == main_base) {
          ours_map[(kv.first - main_base) & ~(uintptr_t)3] += kv.second;
        } else {
          const char *lib = (di.dli_fname && *di.dli_fname) ? di.dli_fname : "?";
          const char *slash = std::strrchr(lib, '/');
          // Symbole exporte le plus proche : distingue l'attente (futex, poll) du travail
          // (memcpy, malloc...) dans libc, et les entrees du pilote GL.
          std::string key = slash ? slash + 1 : lib;
          if (di.dli_sname) { key += ":"; key += di.dli_sname; }
          per_lib[key] += kv.second;
        }
      }
      uint64_t in_main = 0;
      for (const auto &kv : ours_map) {
        ours.push_back({kv.second, kv.first});
        in_main += kv.second;
      }
      std::sort(ours.begin(), ours.end(), [](auto &a, auto &b) { return a.first > b.first; });
      NJ_LOG << "[APROF] fil=" << g_slots[i].name << " echantillons=" << total[i]
                << " libmain=" << in_main << std::endl;
      std::vector<std::pair<uint32_t, std::string>> libs;
      for (const auto &kv : per_lib) libs.push_back({kv.second, kv.first});
      std::sort(libs.begin(), libs.end(), [](auto &a, auto &b) { return a.first > b.first; });
      for (size_t k = 0; k < libs.size() && k < 16; ++k)
        NJ_LOG << "[APROF] fil=" << g_slots[i].name << " lib " << libs[k].first << " "
                  << libs[k].second << std::endl;
      // Adresses les plus frequentes (plusieurs par ligne pour limiter le journal).
      std::string line;
      int cnt = 0;
      for (size_t k = 0; k < ours.size() && k < 400; ++k) {
        char b[32];
        std::snprintf(b, sizeof b, " %x:%u", (unsigned)ours[k].second, ours[k].first);
        line += b;
        if (++cnt == 20) {
          NJ_LOG << "[APROF] fil=" << g_slots[i].name << " pc" << line << std::endl;
          line.clear();
          cnt = 0;
        }
      }
      if (!line.empty())
        NJ_LOG << "[APROF] fil=" << g_slots[i].name << " pc" << line << std::endl;
      // Appelants (adresse de retour dans libmain.so), tous echantillons confondus.
      {
        std::unordered_map<uintptr_t, uint32_t> lr_map;
        for (const auto &kv : hist_lr[i]) {
          Dl_info di{};
          if (dladdr((void *)kv.first, &di) && (uintptr_t)di.dli_fbase == main_base)
            lr_map[(kv.first - main_base) & ~(uintptr_t)1] += kv.second;
        }
        std::vector<std::pair<uint32_t, uintptr_t>> lrs;
        for (const auto &kv : lr_map) lrs.push_back({kv.second, kv.first});
        std::sort(lrs.begin(), lrs.end(), [](auto &a, auto &b) { return a.first > b.first; });
        std::string l2;
        int c2 = 0;
        for (size_t k = 0; k < lrs.size() && k < 200; ++k) {
          char b[32];
          std::snprintf(b, sizeof b, " %x:%u", (unsigned)lrs[k].second, lrs[k].first);
          l2 += b;
          if (++c2 == 20) {
            NJ_LOG << "[APROF] fil=" << g_slots[i].name << " lr" << l2 << std::endl;
            l2.clear();
            c2 = 0;
          }
        }
        if (!l2.empty()) NJ_LOG << "[APROF] fil=" << g_slots[i].name << " lr" << l2 << std::endl;
      }
      if (!hist_focus[i].empty()) {
        std::string l3;
        for (const auto &kv : hist_focus[i]) {
          char b[32];
          std::snprintf(b, sizeof b, " %x:%u", (unsigned)((kv.first - main_base) & ~(uintptr_t)1),
                        kv.second);
          l3 += b;
        }
        NJ_LOG << "[APROF] fil=" << g_slots[i].name << " focus" << l3 << std::endl;
        hist_focus[i].clear();
      }
      hist[i].clear();
      hist_lr[i].clear();
      total[i] = 0;
    }
  }
}

}  // namespace

void register_thread(const char *name) {
  place::thread_started(name);   // placement et mesures du journal, toujours
  if (!enabled()) return;
  const int i = g_nslots.load();
  if (i >= kMaxThreads) return;
  std::strncpy(g_slots[i].name, name, sizeof g_slots[i].name - 1);
  g_slots[i].tid.store((int)syscall(SYS_gettid));
  g_nslots.store(i + 1, std::memory_order_release);
}

void start() {
  if (!enabled() || g_started.exchange(true)) return;
  struct sigaction sa {};
  sa.sa_sigaction = on_sigprof;
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGPROF, &sa, nullptr);
  std::thread(sampler_loop).detach();
  NJ_LOG << "[APROF] echantillonneur demarre" << std::endl;
}

}  // namespace nj::runtime::njprof

#elif defined(_WIN32)

// Windows : NJ_THREADCPU=1 imprime toutes les 10 s le temps processeur de chaque fil nomme
// (ligne [THREADCPU], en % d'un coeur), pour voir qui occupe les 4 coeurs (01/10).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "runtime/env.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace nj::runtime::njprof {
namespace {
struct Slot {
  std::string name;
  HANDLE h;
  uint64_t last;
  std::map<uint64_t, uint64_t> hist;  // NJ_THREADPROF : adresse -> echantillons
  // (adresse, premier appelant dans l'exe) -> echantillons : qui, chez nous, mene a une
  // bibliotheque (verrou de libwinpthread, attente de ntdll...).
  std::map<std::pair<uint64_t, uint64_t>, uint64_t> calls;
};
std::mutex g_mu;
std::vector<Slot> g_slots;
const char *prof_dir() {
  static const char *d = NJ_ENV("NJ_THREADPROF");
  return d;
}
bool on() {
  static const bool o = NJ_ENV("NJ_THREADCPU") != nullptr || prof_dir() != nullptr;
  return o;
}

// NJ_THREADPROF=<dossier> : toutes les ~1 ms, adresse d'execution de chaque fil nomme ; toutes les
// 10 s, <dossier>/prof_<fil><n>.txt = « module decalage echantillons » par ligne (decalage dans
// l'exe -> nm ; ailleurs : pilote, noyau...). Profil en jeu reel, avec OBS (01/10).
void dump_prof() {
  std::lock_guard<std::mutex> lk(g_mu);
  int n = 0;
  for (Slot &sl : g_slots) {
    char path[512];
    std::snprintf(path, sizeof path, "%s/prof_%s%d.txt", prof_dir(), sl.name.c_str(), n++);
    FILE *f = std::fopen(path, "w");
    if (!f)
      continue;
    for (const auto &kv : sl.hist) {
      HMODULE m = nullptr;
      char mod[MAX_PATH] = "?";
      if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                 GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             (LPCSTR)kv.first, &m) && m) {
        GetModuleFileNameA(m, mod, sizeof mod);
        const char *b = std::strrchr(mod, '\\');
        std::fprintf(f, "%s %llx %llu\n", b ? b + 1 : mod, (unsigned long long)(kv.first - (uint64_t)m),
                     (unsigned long long)kv.second);
      } else {
        std::fprintf(f, "? %llx %llu\n", (unsigned long long)kv.first, (unsigned long long)kv.second);
      }
    }
    std::fclose(f);
    std::snprintf(path, sizeof path, "%s/pile_%s%d.txt", prof_dir(), sl.name.c_str(), n - 1);
    if (FILE *g = std::fopen(path, "w")) {
      const uint64_t base = (uint64_t)GetModuleHandleA(nullptr);
      for (const auto &kv : sl.calls) {
        HMODULE m = nullptr;
        char mod[MAX_PATH] = "?";
        uint64_t off = kv.first.first;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)kv.first.first, &m) && m) {
          GetModuleFileNameA(m, mod, sizeof mod);
          off -= (uint64_t)m;
        }
        const char *b = std::strrchr(mod, '\\');
        std::fprintf(g, "%s %llx %llx %llu\n", b ? b + 1 : mod, (unsigned long long)off,
                     (unsigned long long)(kv.first.second ? kv.first.second - base : 0),
                     (unsigned long long)kv.second);
      }
      std::fclose(g);
    }
  }
}
// Premier cadre de l'exe en remontant la pile (tables de deroulement x64). Fil suspendu : aucune
// allocation ici.
uint64_t first_exe_frame(CONTEXT c) {
  static const uint64_t lo = (uint64_t)GetModuleHandleA(nullptr);
  static const uint64_t hi = [] {
    const auto *dos = (const IMAGE_DOS_HEADER *)lo;
    const auto *nt = (const IMAGE_NT_HEADERS *)(lo + dos->e_lfanew);
    return lo + nt->OptionalHeader.SizeOfImage;
  }();
  for (int depth = 0; depth < 24; ++depth) {
    if (c.Rip >= lo && c.Rip < hi)
      return c.Rip;
    DWORD64 image = 0;
    PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(c.Rip, &image, nullptr);
    if (!fn) {
      // fonction feuille : l'adresse de retour est au sommet de la pile
      if (!c.Rsp)
        return 0;
      c.Rip = *(const DWORD64 *)c.Rsp;
      c.Rsp += 8;
    } else {
      void *handler_data = nullptr;
      DWORD64 frame = 0;
      RtlVirtualUnwind(UNW_FLAG_NHANDLER, image, c.Rip, fn, &c, &handler_data, &frame, nullptr);
    }
    if (!c.Rip)
      return 0;
  }
  return 0;
}
void sample_loop() {
  // Pas d'echantillon pendant le demarrage : un fil suspendu pendant le chargement d'une DLL (pilote
  // GL) tient le verrou du chargeur, que RtlLookupFunctionEntry attend -> blocage (essai du 01/10,
  // jeu fige apres l'init GL). NJ_THREADPROF_DELAY=<s>, 30 par defaut.
  const char *dl = NJ_ENV("NJ_THREADPROF_DELAY");
  Sleep((DWORD)((dl ? std::atoi(dl) : 30) * 1000));
  for (;;) {
    Sleep(1);
    std::lock_guard<std::mutex> lk(g_mu);
    for (Slot &sl : g_slots) {
      if (SuspendThread(sl.h) == (DWORD)-1)
        continue;
      // pas d'allocation tant que le fil est suspendu (il peut tenir le verrou du tas)
      CONTEXT c{};
      c.ContextFlags = CONTEXT_FULL;
      const bool got = GetThreadContext(sl.h, &c) != 0;
      const uint64_t caller = got ? first_exe_frame(c) : 0;
      ResumeThread(sl.h);
      if (got) {
        ++sl.hist[c.Rip];
        ++sl.calls[{c.Rip, caller}];
      }
    }
  }
}
uint64_t cpu100ns(HANDLE h) {
  FILETIME c, e, k, u;
  if (!GetThreadTimes(h, &c, &e, &k, &u))
    return 0;
  return (((uint64_t)k.dwHighDateTime << 32) | k.dwLowDateTime) +
         (((uint64_t)u.dwHighDateTime << 32) | u.dwLowDateTime);
}
void report_loop() {
  std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    const auto t1 = std::chrono::steady_clock::now();
    const double wall = std::chrono::duration<double>(t1 - t0).count();
    t0 = t1;
    std::string line = "[THREADCPU]";
    {
      // tout le processus : la difference avec les fils nommes = SDL (son), pilote GL...
      static uint64_t last_proc = 0;
      FILETIME c, e, k, u;
      if (GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u)) {
        const uint64_t now = (((uint64_t)k.dwHighDateTime << 32) | k.dwLowDateTime) +
                             (((uint64_t)u.dwHighDateTime << 32) | u.dwLowDateTime);
        char buf[64];
        std::snprintf(buf, sizeof buf, " processus=%.0f%%", last_proc ? (now - last_proc) / 1e7 / wall * 100.0 : 0.0);
        line += buf;
        last_proc = now;
      }
    }
    std::lock_guard<std::mutex> lk(g_mu);
    for (Slot &sl : g_slots) {
      const uint64_t now = cpu100ns(sl.h);
      char buf[96];
      std::snprintf(buf, sizeof buf, " %s=%.0f%%", sl.name.c_str(), (now - sl.last) / 1e7 / wall * 100.0);
      line += buf;
      sl.last = now;
    }
    NJ_PRINTF("%s\n", line.c_str());
    std::fflush(stdout);
    if (prof_dir()) {
      g_mu.unlock();
      dump_prof();
      g_mu.lock();
    }
  }
}
}  // namespace

void register_thread(const char *name) {
  place::thread_started(name);   // mesures du journal, toujours
  if (!on())
    return;
  HANDLE h = nullptr;
  DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &h, 0, FALSE,
                  DUPLICATE_SAME_ACCESS);
  std::lock_guard<std::mutex> lk(g_mu);
  g_slots.push_back({name, h, cpu100ns(h)});
  static bool started = false;
  if (!started) {
    started = true;
    std::thread(report_loop).detach();
    if (prof_dir())
      std::thread(sample_loop).detach();
  }
}
void start() {}
}  // namespace nj::runtime::njprof

#else

namespace nj::runtime::njprof {
void register_thread(const char *name) { place::thread_started(name); }
void start() {}
}  // namespace nj::runtime::njprof

#endif
