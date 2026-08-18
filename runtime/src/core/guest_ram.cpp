// MEM1 dans une fenetre reservee de 256 Mo (voir GuestRam, cpu_context.h). Seuls les `size`
// premiers octets sont accessibles et mis a zero ; le reste de la fenetre ne l'est pas, pour que
// tout acces rapide classe a tort sur un registre materiel (0xCC.. -> +0x0C000000) ou l'EFB
// (0xC8.. -> +0x08000000) s'arrete net au lieu de lire faux.
#include "runtime/cpu_context.h"
#include "runtime/log.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>
#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace nj {
namespace runtime {

extern GuestCpu *g_guest;

#ifndef _WIN32
// Filet du build rapide : un acces classe « rapide » a tort (registre materiel, EFB) tombe dans la
// partie non accessible de la fenetre. On le dit ([FMFAULT] : adresse invitee, dernier pc PPC
// connu), puis on rend la main au gestionnaire precedent (ART, debuggerd) : l'instruction refaute
// et le plantage suit son cours normal, jamais de corruption silencieuse.
static uint8_t *s_fm_window = nullptr;
static struct sigaction s_old_segv;
static void fm_segv(int sig, siginfo_t *si, void *uc) {
  const uint8_t *a = static_cast<const uint8_t *>(si->si_addr);
  if (s_fm_window && a >= s_fm_window && a < s_fm_window + kFastmemWindow) {
    const unsigned off = unsigned(a - s_fm_window);
    char buf[200];
    const int n = std::snprintf(buf, sizeof buf,
                                "[FMFAULT] acces rapide hors MEM1 : adresse invitee 0x?%07x, dernier pc PPC %08x\n",
                                off, g_guest ? unsigned(g_guest->pc) : 0u);
    if (n > 0) (void)!write(2, buf, size_t(n));
    std::fputs(buf, stdout);
    std::fflush(stdout);
  }
  sigaction(SIGSEGV, &s_old_segv, nullptr);   // puis refaute sous l'ancien gestionnaire
  (void)sig;
  (void)uc;
}
static void fm_install_segv(uint8_t *w) {
  s_fm_window = w;
  struct sigaction sa;
  std::memset(&sa, 0, sizeof sa);
  sa.sa_sigaction = fm_segv;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGSEGV, &sa, &s_old_segv);
}
#endif

void GuestRam::resize(size_t size, uint8_t fill) {
  if (p) return;   // une seule fois (constructeur de MMU)
  if (size > kFastmemWindow) std::abort();
#ifdef _WIN32
  void *w = VirtualAlloc(nullptr, kFastmemWindow, MEM_RESERVE, PAGE_NOACCESS);
  if (w && VirtualAlloc(w, size, MEM_COMMIT, PAGE_READWRITE))
    p = static_cast<uint8_t *>(w);
#else
  void *w = mmap(nullptr, kFastmemWindow, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (w != MAP_FAILED && mprotect(w, (size + 4095) & ~size_t(4095), PROT_READ | PROT_WRITE) == 0)
    p = static_cast<uint8_t *>(w);
#endif
  if (!p) {
    NJ_EPRINTF("[GuestRam] reservation de %zu Mo impossible\n", kFastmemWindow >> 20);
    std::abort();
  }
  n = size;
  if (fill) std::memset(p, fill, size);   // pages neuves deja a zero
#ifndef _WIN32
  fm_install_segv(p);
#endif
}

void nj_fm_violation(uint32_t site, uint32_t addr) {
  static std::mutex m;
  static std::unordered_map<uint32_t, uint64_t> seen;
  std::lock_guard<std::mutex> lk(m);
  const uint64_t c = ++seen[site];
  if (c == 1 || c == 1000000)
    NJ_PRINTF("[FMVIOL] site=%08x adresse=%08x (%llu) sites=%zu\n", site, addr, (unsigned long long)c,
                seen.size());
}

}  // namespace runtime
}  // namespace nj
