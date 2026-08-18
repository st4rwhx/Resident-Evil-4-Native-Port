#pragma once
// Le processeur invite (Gekko) tel que le code recompile le voit : registres, memoire, base de
// temps, decrementeur, instructions appariees (paired singles) et quantification (psq).
// Ecrit d'apres le manuel IBM du 750CL et YAGCD ; reecrit le 02/10 (remplace l'en-tete herite).
//
// Chemin rapide des acces memoire (adresse en MEM1) developpe sur place dans le code recompile,
// chemin lent (registres materiels, tuyau GX, adresses non mappees) hors ligne. Profil du Galaxy
// S5 (29/09) : passer par deux appels (dont un trampoline long) coutait ~20 % du fil du jeu ;
// developper aussi le chemin lent faisait exploser la bibliotheque. -DNJ_MEM_NOINLINE : tout hors
// ligne (comparaison).
#ifdef NJ_MEM_NOINLINE
#define NJ_MEM_FAST __attribute__((noinline))
#else
#define NJ_MEM_FAST __attribute__((always_inline)) inline
#endif
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csetjmp>

// Tampon de saut de taille FIXE : 256 octets, comme le jmp_buf d'Android arm64 (long[32]). Le jeu iPhone est compile avec
// les en-tetes Android et le moteur avec ceux d'Apple (jmp_buf de 192 octets) : sans cela GuestCpu n'avait pas la meme
// taille des deux cotes et tous les champs apres celui-ci etaient decales de 64 octets (plantage au premier saut).
#if defined(__APPLE__) && !defined(NJ_APPLE_CROSS)
typedef long NjJmpBuf[32];
#define NJ_JB(x) (*reinterpret_cast<jmp_buf *>(&(x)))
#else
typedef jmp_buf NjJmpBuf;
#define NJ_JB(x) (x)
#endif
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include "runtime/env.h"
#include "runtime/host_clock.h"

// Lecture de l'EFB par le processeur (GXPeekZ), src/hle/gx/efb_peek.cpp.
uint32_t nj_efb_peek_read(uint32_t paddr);

namespace nj::runtime {

// Un champ du registre de condition : inferieur, superieur, egal, debordement.
struct CrField {
  bool lt = false;
  bool gt = false;
  bool eq = false;
  bool so = false;
};
using ConditionField = CrField;

// --- tuyau GX (gx.cpp) --------------------------------------------------------------------
void GX_WGPIPE_Write8(uint8_t val);
void GX_WGPIPE_Write16(uint16_t val);
void GX_WGPIPE_Write32(uint32_t val);
void gx_wgpipe_f32(float val);
void gx_wgpipe_f64(double val);
// Octets d'une liste d'affichage copies au moment de l'appel du jeu, partages avec l'analyseur
// (pas recopies) ; nul sans copie.
std::shared_ptr<const std::vector<uint8_t>> GX_DL_Fetch(uint32_t addr, uint32_t size);
// Tampon d'ecriture du tuyau : le code genere y ajoute directement ses ecritures a 0xCC008xxx
// tant que p + n <= lim (sinon GX_WGPIPE_Write*). Fil du jeu seulement.
struct NjWgp {
  uint8_t *p;
  uint8_t *lim;
};
extern NjWgp g_nj_wgp;
void GX_FifoCfgChanged();   // borne de FIFO reecrite (hw_pi, hw_cp)
void GX_EnableStaging(bool on);
void GX_StageFlush();
struct GuestCpu;
void GX_FifoHashMark(uint64_t vi, GuestCpu &ctx);   // oracle CPU (NJ_FIFOHASH)

// --- registres materiels (os_hle_stubs.cpp -> IoBus) --------------------------------------
uint16_t hwreg_read16(uint32_t addr);
uint32_t hwreg_read32(uint32_t addr);
void hwreg_write16(uint32_t addr, uint16_t val);
void hwreg_write32(uint32_t addr, uint32_t val);

// --- interruptions et appels du moteur (ios.cpp) ------------------------------------------
bool service_syscall(GuestCpu &ctx);
bool drain_callbacks(GuestCpu &ctx);
// Budget d'instructions epuise (njrecomp budget = true) : reporte le consomme, pose ctx.pc,
// regarde echeances et interruptions.
bool nj_budget_slow(GuestCpu &ctx, uint32_t pc);
void hle_dispatch_leave(GuestCpu &ctx);   // sortie d'un niveau d'interruption (rfi)
namespace hw {
extern std::atomic<bool> poll_now;   // regarder les interruptions sans attendre l'echeance
}

// --- debogage -----------------------------------------------------------------------------
void on_watch(uint32_t addr, uint32_t value, int width);
extern uint32_t g_watch_addr;
// Adresse espionnee (NJ_WATCH) : lue a chaque ecriture du chemin rapide. NJ_NO_WATCH
// (Android) retire le test.
#ifdef NJ_NO_WATCH
#define NJ_WATCHING() false
#else
#define NJ_WATCHING() (::nj::runtime::g_watch_addr != 0)
#endif
// Ecriture sous 0x80000000 (sans BAT : une DSI sur la console).
void unmapped_write_hit(uint32_t addr, uint32_t value, int width);
// Sites d'acces lents (NJ_SLOW_SITES, preparation du fastmem) : slow_sites.cpp.
extern uint32_t g_nj_site;
void nj_slow_site_hit(uint32_t addr, int kind);
#ifdef NJ_SLOW_SITES
#define NJ_SLOW_HIT(addr, kind) ::nj::runtime::nj_slow_site_hit((addr), (kind))
#else
#define NJ_SLOW_HIT(addr, kind) ((void)0)
#endif

// MEM1 au debut d'une fenetre reservee de 256 Mo (fastmem, njrecomp fastmem_guard) : le code
// genere lit base + (adresse & 0x0FFFFFFF) sans controle ; registres materiels et EFB tombent dans
// la partie non accessible de la fenetre (un mauvais classement plante au lieu de lire faux).
// Meme interface qu'un std::vector (data, size, []). guest_ram.cpp.
struct GuestRam {
  uint8_t *p = nullptr;
  size_t n = 0;
  void resize(size_t size, uint8_t fill);
  uint8_t *data() { return p; }
  const uint8_t *data() const { return p; }
  size_t size() const { return n; }
  uint8_t &operator[](size_t i) { return p[i]; }
  const uint8_t &operator[](size_t i) const { return p[i]; }
};
static constexpr size_t kFastmemWindow = 256u << 20;
// Build paranoiaque (NJ_FASTMEM_CHECK) : acces rapide hors MEM1, journalise par site.
void nj_fm_violation(uint32_t site, uint32_t addr);

// ==========================================================================================
// Memoire de l'invite : MEM1 (24 Mo), ARAM (16 Mo utiles, 64 Mo reserves), cache verrouille.
// ==========================================================================================
struct MMU {
  GuestRam mem1;
  std::vector<std::uint8_t> mem2;
  // Cache L2 du Gekko utilise comme memoire de travail apres LCEnable, a 0xE0000000 (0x40000
  // octets) : RE4 y garde ses matrices d'os (sans lui, chaque modele anime s'effondrait en un point).
  std::vector<uint8_t> l2_cache = std::vector<uint8_t>(0x40000 + 8, 0);

  // Moteur natif (coeur Nightjar) : la memoire du jeu existe deja (arene a 0x80000000 dans le coeur) ;
  // une MMU construite pendant que `external_mem1` est pose la designe au lieu d'allouer (pas d'ARAM).
  static inline uint8_t *external_mem1 = nullptr;
  MMU() {
    if (external_mem1) {
      mem1.p = external_mem1;
      mem1.n = (24u << 20) + 8;
      return;
    }
    mem1.resize((24u << 20) + 8, 0);
    mem2.resize((64u << 20) + 8, 0);
  }

  // --- ordre des octets ---
  __attribute__((always_inline)) static inline uint16_t be16(const uint8_t *p) { uint16_t v; std::memcpy(&v, p, 2); return __builtin_bswap16(v); }
  __attribute__((always_inline)) static inline uint32_t be32(const uint8_t *p) { uint32_t v; std::memcpy(&v, p, 4); return __builtin_bswap32(v); }
  __attribute__((always_inline)) static inline uint64_t be64(const uint8_t *p) { uint64_t v; std::memcpy(&v, p, 8); return __builtin_bswap64(v); }
  __attribute__((always_inline)) static inline void put16(uint8_t *p, uint16_t v) { v = __builtin_bswap16(v); std::memcpy(p, &v, 2); }
  __attribute__((always_inline)) static inline void put32(uint8_t *p, uint32_t v) { v = __builtin_bswap32(v); std::memcpy(p, &v, 4); }
  __attribute__((always_inline)) static inline void put64(uint8_t *p, uint64_t v) { v = __builtin_bswap64(v); std::memcpy(p, &v, 8); }

  // --- adresses -> octets ---
  // [addr, addr + size) entierement dans une meme memoire (MEM1, ARAM vue a 0x10000000, cache
  // verrouille), ou nul : une taille venue de la memoire invitee ne doit pas faire lire au-dela.
  inline uint8_t *get_span(uint32_t addr, uint64_t size) {
    const uint32_t phys = addr & 0x3FFFFFFFu;
    if ((uint64_t)phys + size <= mem1.size() - 8) return &mem1[phys];
    const uint32_t aram = phys - 0x10000000u;
    if (phys >= 0x10000000u && (uint64_t)aram + size <= mem2.size() - 8) return &mem2[aram];
    const uint32_t l2 = addr - 0xE0000000u;
    if (l2 < 0x40000u && (uint64_t)l2 + size <= 0x40000u) return &l2_cache[l2];
    return nullptr;
  }
  inline uint8_t *get_ptr(uint32_t addr) { return get_span(addr, 1); }

  // Chemin rapide : MEM1 par ses deux vues (0x80000000 en cache, 0xC0000000 sans). Les 16
  // premiers octets restent au chemin complet (le mot a 0x0C y est neutralise).
  static constexpr uint32_t kMem1Size = 24u << 20;
  __attribute__((always_inline)) inline uint8_t *mem1_fast(uint32_t addr) {
    const uint32_t off = (addr & 0xBFFFFFFFu) - 0x80000010u;
    return off < kMem1Size - 0x10u ? mem1.data() + 0x10u + off : nullptr;
  }
  // [addr, addr + size) en MEM1 ou dans le cache verrouille (boucles de peau de RE4), sinon nul.
  __attribute__((always_inline)) inline uint8_t *fast_ptr(uint32_t addr, uint32_t size) {
    if (uint8_t *p = mem1_fast(addr)) return mem1_fast(addr + size - 1) ? p : nullptr;
    const uint32_t l2 = addr - 0xE0000000u;
    if (l2 < 0x40000u && l2 + size <= 0x40000u) {
      NJ_SLOW_HIT(addr, 14);
      return &l2_cache[l2];
    }
    return nullptr;
  }

  // Registres materiels : 0x0C00xxxx (et leur miroir 0x0D00xxxx) ; tuyau GX : 0x0C008xxx.
  static bool is_io(uint32_t phys) {
    const uint32_t page = phys & 0xFFFF0000u;
    return page == 0x0C000000u || page == 0x0D000000u;
  }
  static bool is_gather_pipe(uint32_t phys) { return (phys & 0xFFFFF000u) == 0x0C008000u; }

  // --- chemin lent, commun a toutes les tailles ---
  // N octets gros-boutistes en memoire (0 hors memoire).
  template <int N>
  uint64_t load_ram(uint32_t addr) {
    const uint8_t *q = get_span(addr, N);
    if (!q) return 0;
    uint64_t v = 0;
    for (int i = 0; i < N; ++i) v = v << 8 | q[i];
    return v;
  }
  template <int N>
  void store_ram(uint32_t addr, uint64_t v) {
    uint8_t *q = get_span(addr, N);
    if (!q) return;
    for (int i = N - 1; i >= 0; --i, v >>= 8) q[i] = (uint8_t)v;
  }
  // Avant une ecriture lente : journal des ecritures non mappees et de l'adresse espionnee.
  void note_store(uint32_t addr, uint64_t v, int bits) {
    const uint32_t shown = (uint32_t)(bits == 64 ? v >> 32 : v);
    if (addr < 0x80000000u) [[unlikely]]
      unmapped_write_hit(addr, shown, bits);
    const uint32_t mask = bits == 64 ? 0x3FFFFFF8u : 0x3FFFFFFCu;
    if (g_watch_addr && (addr & mask) == (g_watch_addr & mask)) on_watch(addr, shown, bits);
  }

  // --- 8 bits ---
  NJ_MEM_FAST uint8_t read8(uint32_t addr) {
    if (uint8_t *p = mem1_fast(addr)) [[likely]] return *p;
    return read8_slow(addr);
  }
  __attribute__((noinline)) uint8_t read8_slow(uint32_t addr) {
    NJ_SLOW_HIT(addr, 1);
    const uint32_t phys = addr & 0x3FFFFFFFu;
    if (is_io(phys)) return (uint8_t)(hwreg_read32(phys & ~3u) >> (8 * (3 - (phys & 3))));
    return (uint8_t)load_ram<1>(addr);
  }
  NJ_MEM_FAST void write8(uint32_t addr, uint8_t value) {
    if (!NJ_WATCHING()) [[likely]]
      if (uint8_t *p = mem1_fast(addr)) [[likely]] {
        *p = value;
        return;
      }
    write8_slow(addr, value);
  }
  __attribute__((noinline)) void write8_slow(uint32_t addr, uint8_t value) {
    NJ_SLOW_HIT(addr, 2);
    note_store(addr, value, 8);
    const uint32_t phys = addr & 0x3FFFFFFFu;
    if (is_gather_pipe(phys)) return GX_WGPIPE_Write8(value);
    if (is_io(phys)) return;   // pas d'ecriture 8 bits dans les registres
    store_ram<1>(addr, value);
  }

  // --- 16 bits ---
  NJ_MEM_FAST uint16_t read16(uint32_t addr) {
    if (uint8_t *p = mem1_fast(addr)) [[likely]] return be16(p);
    return read16_slow(addr);
  }
  __attribute__((noinline)) uint16_t read16_slow(uint32_t addr) {
    NJ_SLOW_HIT(addr, 3);
    const uint32_t phys = addr & 0x3FFFFFFFu;
    if (is_io(phys)) return hwreg_read16(phys);
    return (uint16_t)load_ram<2>(addr);
  }
  NJ_MEM_FAST void write16(uint32_t addr, uint16_t value) {
    if (!NJ_WATCHING()) [[likely]]
      if (uint8_t *p = mem1_fast(addr)) [[likely]] {
        put16(p, value);
        return;
      }
    write16_slow(addr, value);
  }
  __attribute__((noinline)) void write16_slow(uint32_t addr, uint16_t value) {
    NJ_SLOW_HIT(addr, 4);
    note_store(addr, value, 16);
    const uint32_t phys = addr & 0x3FFFFFFFu;
    if (is_gather_pipe(phys)) return GX_WGPIPE_Write16(value);
    if (is_io(phys)) return hwreg_write16(phys, value);
    store_ram<2>(addr, value);
  }

  // --- 32 bits ---
  NJ_MEM_FAST uint32_t read32(uint32_t addr) {
    if (uint8_t *p = mem1_fast(addr)) [[likely]] return be32(p);
    return read32_slow(addr);
  }
  __attribute__((noinline)) uint32_t read32_slow(uint32_t addr) {
    NJ_SLOW_HIT(addr, 5);
    const uint32_t phys = addr & 0x3FFFFFFFu;
    if (phys == 0x0C) return 0;   // mot neutralise
    if (is_io(phys)) return hwreg_read32(phys);
    if ((phys >> 24) == 0x08) return ::nj_efb_peek_read(phys);   // lecture de l'EFB
    return (uint32_t)load_ram<4>(addr);
  }
  NJ_MEM_FAST void write32(uint32_t addr, uint32_t value) {
    if (!NJ_WATCHING()) [[likely]]
      if (uint8_t *p = mem1_fast(addr)) [[likely]] {
        put32(p, value);
        return;
      }
    write32_slow(addr, value);
  }
  __attribute__((noinline)) void write32_slow(uint32_t addr, uint32_t value) {
    NJ_SLOW_HIT(addr, 6);
    const uint32_t phys = addr & 0x3FFFFFFFu;
    if (addr < 0x80000000u) [[unlikely]]
      unmapped_write_hit(addr, value, 32);
    if (phys == 0x0C) return;
    if (g_watch_addr && (phys & ~3u) == (g_watch_addr & 0x3FFFFFFCu)) on_watch(addr, value, 32);
    if (is_gather_pipe(phys)) return GX_WGPIPE_Write32(value);
    if (is_io(phys)) return hwreg_write32(phys, value);
    store_ram<4>(addr, value);
  }

  // --- flottants et 64 bits (memoire et tuyau GX seulement) ---
  float read_f32(uint32_t addr) {
    const uint32_t bits = read32(addr);
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
  }
  NJ_MEM_FAST void write_f32(uint32_t addr, float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, 4);
    if (!NJ_WATCHING()) [[likely]]
      if (uint8_t *p = mem1_fast(addr)) [[likely]] {
        put32(p, bits);
        return;
      }
    write_f32_slow(addr, value);
  }
  __attribute__((noinline)) void write_f32_slow(uint32_t addr, float value) {
    NJ_SLOW_HIT(addr, 7);
    uint32_t bits;
    std::memcpy(&bits, &value, 4);
    note_store(addr, bits, 32);
    if (is_gather_pipe(addr & 0x3FFFFFFFu)) return gx_wgpipe_f32(value);
    store_ram<4>(addr, bits);
  }
  NJ_MEM_FAST uint64_t read64(uint32_t addr) {
    if (uint8_t *p = mem1_fast(addr)) [[likely]] return be64(p);
    return read64_slow(addr);
  }
  __attribute__((noinline)) uint64_t read64_slow(uint32_t addr) {
    NJ_SLOW_HIT(addr, 9);
    return load_ram<8>(addr);
  }
  NJ_MEM_FAST void write64(uint32_t addr, uint64_t value) {
    if (!NJ_WATCHING()) [[likely]]
      if (uint8_t *p = mem1_fast(addr)) [[likely]] {
        put64(p, value);
        return;
      }
    write64_slow(addr, value);
  }
  __attribute__((noinline)) void write64_slow(uint32_t addr, uint64_t value) {
    NJ_SLOW_HIT(addr, 10);
    note_store(addr, value, 64);
    store_ram<8>(addr, value);
  }
  NJ_MEM_FAST double read_f64(uint32_t addr) {
    const uint64_t bits = read64(addr);
    double d;
    std::memcpy(&d, &bits, 8);
    return d;
  }
  __attribute__((noinline)) double read_f64_slow(uint32_t addr) {   // appele par nj_ops.h
    NJ_SLOW_HIT(addr, 11);
    const uint64_t bits = load_ram<8>(addr);
    double d;
    std::memcpy(&d, &bits, 8);
    return d;
  }
  NJ_MEM_FAST void write_f64(uint32_t addr, double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, 8);
    if (!NJ_WATCHING()) [[likely]]
      if (uint8_t *p = mem1_fast(addr)) [[likely]] {
        put64(p, bits);
        return;
      }
    write_f64_slow(addr, value);
  }
  __attribute__((noinline)) void write_f64_slow(uint32_t addr, double value) {
    NJ_SLOW_HIT(addr, 8);
    uint64_t bits;
    std::memcpy(&bits, &value, 8);
    note_store(addr, bits, 64);
    if (is_gather_pipe(addr & 0x3FFFFFFFu)) return gx_wgpipe_f64(value);
    store_ram<8>(addr, bits);
  }
};

// ==========================================================================================
// Registres de l'architecture : ce qu'une interruption sauve et rend.
// ==========================================================================================
struct GekkoRegs {
  std::array<uint32_t, 32> gpr{};
  std::array<double, 32> fpr{};   // flottants, et moitie 0 des paires
  std::array<double, 32> ps1{};   // moitie 1 des paires (paired singles)
  std::array<CrField, 8> cr{};
  uint32_t pc = 0, lr = 0, ctr = 0, xer = 0;
  uint32_t msr = 0, fpscr = 0, srr0 = 0, srr1 = 0;
  std::array<uint32_t, 8> gqr{};    // formats de quantification
  std::array<uint32_t, 4> sprg{};
};

struct GuestCpu : GekkoRegs {
  // Registres sauves a l'entree d'un rappel ou d'une interruption, rendus au retour.
  std::vector<GekkoRegs> saved;
  void push_saved(uint32_t msr_value) {
    saved.push_back(static_cast<const GekkoRegs &>(*this));
    saved.back().msr = msr_value;
  }
  void pop_saved() {
    static_cast<GekkoRegs &>(*this) = saved.back();
    saved.pop_back();
  }
  bool inside_cb = false;
  int cb_nesting = 0;

  MMU mmu;

  uint32_t exception_pc = 0;
  uint64_t retired = 0;
  // Echeance (en instructions) du prochain regard aux interruptions par les retours de boucle.
  uint64_t check_deadline = 0;
  // Budget 32 bits (njrecomp budget = true) : `if ((ctx.budget -= n) < 0) nj_budget_slow(...)`
  // a chaque retour de boucle, au lieu d'un compteur et d'une comparaison 64 bits. Consomme =
  // budget_horizon - budget. Fil du jeu seulement.
  int32_t budget = 0;
  int32_t budget_horizon = 0;
  void budget_sync() {
    retired += (uint64_t)((int64_t)budget_horizon - budget);
    budget = budget_horizon = 0;
  }
  void budget_arm() {
    const uint64_t left = check_deadline > retired ? check_deadline - retired : 0;
    budget = budget_horizon = (int32_t)(left < (1u << 30) ? left : (1u << 30));
  }

  std::atomic<bool> alive{true};
  std::atomic<bool> vblank_due{false};
  NjJmpBuf exc_jump;
  uint32_t reservation_addr = 0xFFFFFFFFu;   // lwarx / stwcx.
  uint32_t saved_dispatch = 0;           // OSContext de l'interruption en cours (ios.cpp)

  // --- base de temps -----------------------------------------------------------------------
  // Elle suit le temps reel, comme sur la console (40,5 MHz). Compter les instructions faisait
  // courir l'horloge du jeu plusieurs fois trop vite (chronometre de partie de RE4 a 3,4 fois le
  // temps reel). NJ_INSTTB=1 : l'ancienne horloge en instructions.
  uint64_t timebase_hz = 40500000;
  std::chrono::steady_clock::time_point tb_start = std::chrono::steady_clock::now();
  // Depart en microsecondes, converti une fois (la conversion est une division 64 bits,
  // logicielle sur ARM 32 bits : ~15 % du fil du jeu au S5).
  mutable int64_t tb_start_us_cache_ = INT64_MIN;
  int64_t tb_start_us() const {
    if (tb_start_us_cache_ == INT64_MIN) [[unlikely]]
      tb_start_us_cache_ =
          std::chrono::duration_cast<std::chrono::microseconds>(tb_start.time_since_epoch()).count();
    return tb_start_us_cache_;
  }
  static bool instruction_clock() {
#if defined(NJ_RELEASE) && !defined(NJ_DEVTOOLS)
    return false;
#else
    static const bool on = NJ_ENV("NJ_INSTTB") != nullptr;
    return on;
#endif
  }
  uint64_t timebase_now() const {
    if (instruction_clock()) return retired;
    return us_to_tb((uint64_t)(nj::host_now_us() - tb_start_us()));
  }
  // Meme chose a la precision de l'heure publiee (host_coarse_us) : verifications du moteur
  // seulement, jamais une lecture du jeu.
  uint64_t read_timebase_coarse() const {
    if (instruction_clock()) return retired;
    return us_to_tb((uint64_t)(nj::host_coarse_us() - tb_start_us()));
  }
  // Microsecondes -> tics sans division 64 bits : 40,5 MHz = 81/2 tics par microseconde,
  // 60,75 MHz = 243/4. Exact au bit pres (us = q * 10^6 + r).
  uint64_t us_to_tb(uint64_t us) const {
    switch (timebase_hz) {
      case 40500000: return us * 81 / 2;
      case 60750000: return us * 243 / 4;
      default: return us / 1000000 * timebase_hz + us % 1000000 * timebase_hz / 1000000;
    }
  }

  // --- decrementeur -------------------------------------------------------------------------
  uint64_t dec_value = 0xFFFFFFFFu;
  uint64_t dec_base_tb = 0;
  bool dec_fired = false;
  void set_decrementer(uint32_t v) {
    dec_value = v;
    dec_base_tb = timebase_now();
    dec_fired = v != 0xFFFFFFFFu;
  }
  uint32_t get_decrementer() {
    const uint64_t gone = timebase_now() - dec_base_tb;
    return gone >= dec_value ? 0 : (uint32_t)(dec_value - gone);
  }
  bool dec_expired() { return dec_fired && timebase_now() - dec_base_tb >= dec_value; }
  bool dec_expired_coarse() { return dec_fired && read_timebase_coarse() - dec_base_tb >= dec_value; }

  // Compatibilite sources precompilees
  uint64_t read_timebase() const { return timebase_now(); }
  uint32_t read_dec() { return get_decrementer(); }
  void write_dec(uint32_t v) { set_decrementer(v); }

  // --- quantification (psq_l / psq_st) ------------------------------------------------------
  // GQR : bits 16-18 type au chargement, 24-29 echelle au chargement ; bits 0-2 type au
  // rangement, 8-13 echelle au rangement. Types : 0 flottant, 4 u8, 5 u16, 6 s8, 7 s16.
  // L'echelle est un exposant signe sur 6 bits : x 2^-e au chargement, x 2^e au rangement (une
  // echelle 0x20 = -32 donne 2^32 : les normales s8 de RE4 en dependent).
  static float pow2_scale(int e) {
    if (e < -126 || e > 127) [[unlikely]]
      return std::ldexp(1.0f, e);
    const uint32_t bits = (uint32_t)(e + 127) << 23;   // 2^e exact, sans table
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
  }
  static int gqr_exponent(uint32_t field6) { return (field6 & 0x20) ? (int)field6 - 0x40 : (int)field6; }
  static uint32_t quant_size(uint32_t type) { return type == 0 ? 4 : (type == 4 || type == 6) ? 1 : 2; }
  static double unpack_q(const uint8_t *q, uint32_t type, float scale) {
    switch (type) {
      case 0: {
        const uint32_t b = MMU::be32(q);
        float f;
        std::memcpy(&f, &b, 4);
        return f;
      }
      case 4: return q[0] * scale;
      case 5: return MMU::be16(q) * scale;
      case 6: return (int8_t)q[0] * scale;
      default: return (int16_t)MMU::be16(q) * scale;
    }
  }
  static double clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
  static void pack_q(uint8_t *q, uint32_t type, float scale, double v) {
    switch (type) {
      case 0: {
        const float f = (float)v;
        uint32_t b;
        std::memcpy(&b, &f, 4);
        MMU::put32(q, b);
        break;
      }
      case 4: q[0] = (uint8_t)clampd(v * scale, 0.0, 255.0); break;
      case 5: MMU::put16(q, (uint16_t)clampd(v * scale, 0.0, 65535.0)); break;
      case 6: q[0] = (uint8_t)(int8_t)clampd(v * scale, -128.0, 127.0); break;
      default: MMU::put16(q, (uint16_t)(int16_t)clampd(v * scale, -32768.0, 32767.0)); break;
    }
  }

  // Cas courant en ligne (flottants en MEM1 ou dans le cache verrouille), le reste hors ligne.
  // 20/10 : GQR6 change d'un lot de sommets a l'autre (trans.cpp), donc psq_const ne le connait pas ; les boucles d'habillage des personnages
  // (loop_1bone / loop_1bone8, 33 % des echantillons du fil du jeu dans les salles de nuit) passaient donc par psq_load_slow a chaque element.
  // Les types quantifies prennent maintenant le chemin en ligne ; l'echelle (2^-e) n'est recalculee que quand la valeur du GQR change.
  uint32_t psq_ld_g[8] = {~0u, ~0u, ~0u, ~0u, ~0u, ~0u, ~0u, ~0u};
  float psq_ld_scale[8] = {};
  uint32_t psq_st_g[8] = {~0u, ~0u, ~0u, ~0u, ~0u, ~0u, ~0u, ~0u};
  float psq_st_scale[8] = {};
  NJ_MEM_FAST void psq_load(uint32_t frD, uint32_t addr, uint32_t W, uint32_t I) {
    const uint32_t g = gqr[I], type = (g >> 16) & 7;
    if (type == 0) [[likely]] {
      if (const uint8_t *p = mmu.fast_ptr(addr, W ? 4u : 8u)) [[likely]] {
        fpr[frD] = unpack_q(p, 0, 1.0f);
        ps1[frD] = W ? 1.0 : unpack_q(p + 4, 0, 1.0f);
        return;
      }
    } else if (type >= 4) {
      if (psq_ld_g[I] != g) [[unlikely]] {
        psq_ld_g[I] = g;
        psq_ld_scale[I] = pow2_scale(-gqr_exponent((g >> 24) & 0x3F));
      }
      const uint32_t size = quant_size(type);
      if (const uint8_t *p = mmu.fast_ptr(addr, W ? size : 2 * size)) [[likely]] {
        const float scale = psq_ld_scale[I];
        fpr[frD] = unpack_q(p, type, scale);
        ps1[frD] = W ? 1.0 : unpack_q(p + size, type, scale);
        return;
      }
    }
    psq_load_slow(frD, addr, W, I);
  }
  __attribute__((noinline)) void psq_load_slow(uint32_t frD, uint32_t addr, uint32_t W, uint32_t I) {
    NJ_SLOW_HIT(addr, 12);
    const uint32_t g = gqr[I], type = (g >> 16) & 7, size = quant_size(type);
    const float scale = pow2_scale(-gqr_exponent((g >> 24) & 0x3F));
    if (type == 0 || type >= 4)
      if (const uint8_t *p = mmu.fast_ptr(addr, W ? size : 2 * size)) {
        fpr[frD] = unpack_q(p, type, scale);
        ps1[frD] = W ? 1.0 : unpack_q(p + size, type, scale);
        return;
      }
    // ailleurs (ou types 1-3, non definis) : element par element par le chemin complet
    auto element = [&](uint32_t a) -> double {
      switch (type) {
        case 0: return mmu.read_f32(a);
        case 4: return mmu.read8(a) * scale;
        case 5: return mmu.read16(a) * scale;
        case 6: return (int8_t)mmu.read8(a) * scale;
        case 7: return (int16_t)mmu.read16(a) * scale;
        default: return 0.0;
      }
    };
    fpr[frD] = element(addr);
    ps1[frD] = W ? 1.0 : element(addr + size);
  }
  NJ_MEM_FAST void psq_store(uint32_t frS, uint32_t addr, uint32_t W, uint32_t I) {
    const uint32_t g = gqr[I], type = g & 7;
    if (!NJ_WATCHING()) [[likely]] {
      if (type == 0) [[likely]] {
        if (uint8_t *p = mmu.fast_ptr(addr, W ? 4u : 8u)) [[likely]] {
          pack_q(p, 0, 1.0f, fpr[frS]);
          if (!W) pack_q(p + 4, 0, 1.0f, ps1[frS]);
          return;
        }
      } else if (type >= 4) {
        if (psq_st_g[I] != g) [[unlikely]] {
          psq_st_g[I] = g;
          psq_st_scale[I] = pow2_scale(gqr_exponent((g >> 8) & 0x3F));
        }
        const uint32_t size = quant_size(type);
        if (uint8_t *p = mmu.fast_ptr(addr, W ? size : 2 * size)) [[likely]] {
          const float scale = psq_st_scale[I];
          pack_q(p, type, scale, fpr[frS]);
          if (!W) pack_q(p + size, type, scale, ps1[frS]);
          return;
        }
      }
    }
    psq_store_slow(frS, addr, W, I);
  }
  __attribute__((noinline)) void psq_store_slow(uint32_t frS, uint32_t addr, uint32_t W, uint32_t I) {
    NJ_SLOW_HIT(addr, 13);
    const uint32_t g = gqr[I], type = g & 7, size = quant_size(type);
    const float scale = pow2_scale(gqr_exponent((g >> 8) & 0x3F));
    if (!NJ_WATCHING() && (type == 0 || type >= 4))
      if (uint8_t *p = mmu.fast_ptr(addr, W ? size : 2 * size)) {
        pack_q(p, type, scale, fpr[frS]);
        if (!W) pack_q(p + size, type, scale, ps1[frS]);
        return;
      }
    auto element = [&](uint32_t a, double v) {
      switch (type) {
        case 0: mmu.write_f32(a, (float)v); break;
        case 4: mmu.write8(a, (uint8_t)clampd(v * scale, 0.0, 255.0)); break;
        case 5: mmu.write16(a, (uint16_t)clampd(v * scale, 0.0, 65535.0)); break;
        case 6: mmu.write8(a, (uint8_t)(int8_t)clampd(v * scale, -128.0, 127.0)); break;
        case 7: mmu.write16(a, (uint16_t)(int16_t)clampd(v * scale, -32768.0, 32767.0)); break;
        default: break;
      }
    };
    element(addr, fpr[frS]);
    if (!W) element(addr + size, ps1[frS]);
  }

  // --- instructions appariees ----------------------------------------------------------------
  // Chaque registre flottant porte deux valeurs : fpr (moitie 0) et ps1 (moitie 1). set_pair
  // recoit les deux resultats deja calcules (D peut etre aussi une source).
  void set_pair(uint32_t D, double lo, double hi) {
    fpr[D] = lo;
    ps1[D] = hi;
  }
  void ps_add(uint32_t D, uint32_t A, uint32_t B) { set_pair(D, fpr[A] + fpr[B], ps1[A] + ps1[B]); }
  void ps_sub(uint32_t D, uint32_t A, uint32_t B) { set_pair(D, fpr[A] - fpr[B], ps1[A] - ps1[B]); }
  void ps_mul(uint32_t D, uint32_t A, uint32_t C) { set_pair(D, fpr[A] * fpr[C], ps1[A] * ps1[C]); }
  void ps_div(uint32_t D, uint32_t A, uint32_t B) { set_pair(D, fpr[A] / fpr[B], ps1[A] / ps1[B]); }
  // a * c + b, fusionne comme sur le Gekko
  void ps_madd(uint32_t D, uint32_t A, uint32_t C, uint32_t B) {
    set_pair(D, std::fma(fpr[A], fpr[C], fpr[B]), std::fma(ps1[A], ps1[C], ps1[B]));
  }
  void ps_msub(uint32_t D, uint32_t A, uint32_t C, uint32_t B) {
    set_pair(D, std::fma(fpr[A], fpr[C], -fpr[B]), std::fma(ps1[A], ps1[C], -ps1[B]));
  }
  void ps_nmadd(uint32_t D, uint32_t A, uint32_t C, uint32_t B) {
    set_pair(D, -std::fma(fpr[A], fpr[C], fpr[B]), -std::fma(ps1[A], ps1[C], ps1[B]));
  }
  void ps_nmsub(uint32_t D, uint32_t A, uint32_t C, uint32_t B) {
    set_pair(D, -std::fma(fpr[A], fpr[C], -fpr[B]), -std::fma(ps1[A], ps1[C], -ps1[B]));
  }
  // sommes croisees, arrondies en simple precision
  void ps_sum0(uint32_t D, uint32_t A, uint32_t C, uint32_t B) { set_pair(D, (float)(fpr[A] + ps1[B]), (float)ps1[C]); }
  void ps_sum1(uint32_t D, uint32_t A, uint32_t C, uint32_t B) { set_pair(D, (float)fpr[C], (float)(fpr[A] + ps1[B])); }
  // par une moitie de C (scalaire)
  void ps_muls0(uint32_t D, uint32_t A, uint32_t C) { set_pair(D, fpr[A] * fpr[C], ps1[A] * fpr[C]); }
  void ps_muls1(uint32_t D, uint32_t A, uint32_t C) { set_pair(D, fpr[A] * ps1[C], ps1[A] * ps1[C]); }
  void ps_madds0(uint32_t D, uint32_t A, uint32_t C, uint32_t B) {
    set_pair(D, std::fma(fpr[A], fpr[C], fpr[B]), std::fma(ps1[A], fpr[C], ps1[B]));
  }
  void ps_madds1(uint32_t D, uint32_t A, uint32_t C, uint32_t B) {
    set_pair(D, std::fma(fpr[A], ps1[C], fpr[B]), std::fma(ps1[A], ps1[C], ps1[B]));
  }
  void ps_sel(uint32_t D, uint32_t A, uint32_t C, uint32_t B) {
    set_pair(D, fpr[A] >= 0.0 ? fpr[C] : fpr[B], ps1[A] >= 0.0 ? ps1[C] : ps1[B]);
  }
  void ps_mr(uint32_t D, uint32_t B) { set_pair(D, fpr[B], ps1[B]); }
  void ps_neg(uint32_t D, uint32_t B) { set_pair(D, -fpr[B], -ps1[B]); }
  void ps_abs(uint32_t D, uint32_t B) { set_pair(D, std::fabs(fpr[B]), std::fabs(ps1[B])); }
  void ps_nabs(uint32_t D, uint32_t B) { set_pair(D, -std::fabs(fpr[B]), -std::fabs(ps1[B])); }
  // fusions : moitie choisie de A, moitie choisie de B
  void ps_merge00(uint32_t D, uint32_t A, uint32_t B) { set_pair(D, fpr[A], fpr[B]); }
  void ps_merge01(uint32_t D, uint32_t A, uint32_t B) { set_pair(D, fpr[A], ps1[B]); }
  void ps_merge10(uint32_t D, uint32_t A, uint32_t B) { set_pair(D, ps1[A], fpr[B]); }
  void ps_merge11(uint32_t D, uint32_t A, uint32_t B) { set_pair(D, ps1[A], ps1[B]); }
  // comparaisons d'une moitie : non ordonne -> SO
  void compare_into(uint32_t field, double a, double b) {
    CrField &f = cr[field];
    const bool unordered = std::isnan(a) || std::isnan(b);
    f.lt = !unordered && a < b;
    f.gt = !unordered && a > b;
    f.eq = !unordered && a == b;
    f.so = unordered;
  }
  void ps_cmpu0(uint32_t crD, uint32_t A, uint32_t B) { compare_into(crD, fpr[A], fpr[B]); }
  void ps_cmpo0(uint32_t crD, uint32_t A, uint32_t B) { compare_into(crD, fpr[A], fpr[B]); }
  void ps_cmpu1(uint32_t crD, uint32_t A, uint32_t B) { compare_into(crD, ps1[A], ps1[B]); }
  void ps_cmpo1(uint32_t crD, uint32_t A, uint32_t B) { compare_into(crD, ps1[A], ps1[B]); }
};

// --- processeur de secours (fallback_cpu.cpp) -----------------------------------------------
void interp_single(GuestCpu &ctx, uint32_t opcode, uint32_t pc);
bool interp_one_insn(GuestCpu &ctx);
void interp_advance(GuestCpu &ctx);
void interp_report(std::ostream &os);
void add_recompiled_range(uint32_t lo, uint32_t hi);
bool in_recompiled_code(uint32_t at);

// --- trace des appels (developpement) -------------------------------------------------------
extern bool g_trace_calls;
void log_null_call(uint32_t site, uint32_t lr);

}  // namespace nj::runtime

namespace nj::runtime {
using CPUContext = GuestCpu;
}

namespace nwii::runtime {
using CPUContext = nj::runtime::GuestCpu;
using GuestCpu = nj::runtime::GuestCpu;
using MMU = nj::runtime::MMU;
using ConditionField = nj::runtime::ConditionField;
using NjWgp = nj::runtime::NjWgp;
inline NjWgp &g_nj_wgp = nj::runtime::g_nj_wgp;
inline void nj_fm_violation(uint32_t s, uint32_t a) { nj::runtime::nj_fm_violation(s, a); }
inline void interpret_step(nj::runtime::GuestCpu &c) { nj::runtime::interp_advance(c); }
inline void micro_interpret(nj::runtime::GuestCpu &c, uint32_t op, uint32_t pc) { nj::runtime::interp_single(c, op, pc); }
inline bool nj_budget_slow(nj::runtime::GuestCpu &c, uint32_t pc) { return nj::runtime::nj_budget_slow(c, pc); }
inline void hle_dispatch_leave(nj::runtime::GuestCpu &c) { nj::runtime::hle_dispatch_leave(c); }
inline bool handle_syscall(nj::runtime::GuestCpu &c) { return nj::runtime::service_syscall(c); }
inline void note_null_call(uint32_t s, uint32_t lr) { nj::runtime::log_null_call(s, lr); }
namespace hw {
inline std::atomic<bool> &poll_now = nj::runtime::hw::poll_now;
}
}  // namespace nwii::runtime

inline bool process_pending_callbacks(nj::runtime::GuestCpu &c) {
  return nj::runtime::drain_callbacks(c);
}

#include "runtime/rel_aot.h"

// Alias pour g_rel_delta : declare dans nj::runtime par rel_aot.h, utilise par
// les sources precompilees comme nwii::runtime::g_rel_delta.
namespace nwii::runtime {
inline int32_t (&g_rel_delta)[nj::runtime::kRelMaxId][2] = nj::runtime::g_rel_delta;
}
