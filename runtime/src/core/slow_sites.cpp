// Releve des sites d'acces memoire lents, pour le fastmem (docs/OPTIMISATION_S5.md).
// Le code genere avec slow_sites = true (njrecomp) pose dans g_nj_site l'adresse PPC de chaque
// instruction d'acces memoire ; compile avec NJ_SLOW_SITES, chaque chemin lent de MMU compte
// (site, octet haut de l'adresse, genre d'acces). Site 0 : acces du runtime lui-meme.
// Le fichier (NJ_SLOW_SITES_OUT, defaut slow_sites.txt) : « site region genre compte », en
// hexadecimal sauf le compte. A chaque psq lent (genres 12 et 13), les 8 GQR du moment sont aussi
// comptes (lignes « gqr ») : les GQR sont-ils des constantes apres le demarrage ?
#include "runtime/cpu_context.h"
#include "runtime/env.h"
#include <cstdio>
#include <array>
#include <cstdlib>
#include <map>
#include <mutex>
#include <unordered_map>

namespace nj {
namespace runtime {

uint32_t g_nj_site = 0;
extern GuestCpu *g_guest;

#ifdef NJ_SLOW_SITES
namespace {
std::mutex s_m;
std::unordered_map<uint64_t, uint64_t> *s_counts = nullptr;
uint64_t s_total = 0;
std::map<std::array<uint32_t, 8>, uint64_t> *s_gqr = nullptr;

void dump_locked() {
  if (!s_counts) return;
  const char *out = NJ_ENV("NJ_SLOW_SITES_OUT");
  FILE *f = std::fopen(out ? out : "slow_sites.txt", "w");
  if (!f) return;
  std::fprintf(f, "# site region genre compte (genre : 1 r8 2 w8 3 r16 4 w16 5 r32 6 w32 7 wf32 8 wf64 "
                  "9 r64 10 w64 11 rf64 12 psq_l 13 psq_st 14 L2 par fast_ptr)\n");
  for (const auto &kv : *s_counts)
    std::fprintf(f, "%08x %02x %d %llu\n", unsigned(kv.first >> 32), unsigned((kv.first >> 8) & 0xFF),
                 int(kv.first & 0xFF), (unsigned long long)kv.second);
  if (s_gqr)
    for (const auto &kv : *s_gqr)
      std::fprintf(f, "gqr %08x %08x %08x %08x %08x %08x %08x %08x %llu\n", kv.first[0], kv.first[1],
                   kv.first[2], kv.first[3], kv.first[4], kv.first[5], kv.first[6], kv.first[7],
                   (unsigned long long)kv.second);
  std::fclose(f);
}
void dump_at_exit() {
  std::lock_guard<std::mutex> lk(s_m);
  dump_locked();
}
}  // namespace

void nj_slow_site_hit(uint32_t addr, int kind) {
  const uint64_t key = (uint64_t(g_nj_site) << 32) | ((addr >> 24) << 8) | uint32_t(kind & 0xFF);
  std::lock_guard<std::mutex> lk(s_m);
  if (!s_counts) {
    s_counts = new std::unordered_map<uint64_t, uint64_t>();
    s_gqr = new std::map<std::array<uint32_t, 8>, uint64_t>();
    std::atexit(dump_at_exit);
  }
  bool new_gqr = false;
  if ((kind == 12 || kind == 13) && g_guest) {
    std::array<uint32_t, 8> g;
    for (int i = 0; i < 8; ++i) g[i] = g_guest->gqr[i];
    new_gqr = (*s_gqr)[g]++ == 0;
  }
  // Reecrit a chaque nouveau site (quelques centaines en tout) et toutes les 2^20 occurrences :
  // le jeu est souvent arrete sans sortie propre.
  if ((*s_counts)[key]++ == 0 || new_gqr || (++s_total & 0xFFFFF) == 0) dump_locked();
}
#else
void nj_slow_site_hit(uint32_t, int) {}
#endif

}  // namespace runtime
}  // namespace nj
