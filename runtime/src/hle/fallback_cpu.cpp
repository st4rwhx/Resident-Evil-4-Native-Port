// Processeur de secours : execute, instruction par instruction, le code invite que la recompilation
// ne couvre pas. Ecrit le 02/10 depuis le manuel IBM « PowerPC Microprocessor Family : The
// Programming Environments » et le manuel du 750CL (Gekko) ; remplace l'interpreteur herite.
//
// Quand sert-il ? Presque jamais : tout le DOL et les 110 modules REL sont recompiles (0 entree
// mesuree en partie, NJ_INTERPLOG). Restent deux cas :
//   - un saut vers du code hors de toute fonction connue (code recopie par le jeu a l'execution) :
//     interp_advance() roule jusqu'a revenir dans le code recompile ;
//   - un mot de donnees au milieu d'une fonction (opcode 0 ou 1) : interp_single().
// Couverture : entiers, logique, decalages et rotations, comparaisons, branchements, registre de
// condition, chargements et rangements (entiers, flottants simples et doubles), registres
// speciaux, arithmetique flottante de base. Les instructions appariees (ps_*, psq_*) et le reste
// du flottant ne sont pas attendus ici : une instruction inconnue est signalee une fois et arrete
// le fil invite proprement plutot que d'etre sautee en silence.
// 06/10 (P3) : il devient l'interpreteur des fonctions pas encore compilees ; appariees, psq et tout
// le flottant ajoutes, semantique alignee sur l'emetteur (banc tools/re4/banc_interp.py).
#include "runtime/cpu_context.h"
#include "runtime/log.h"
#include "runtime/env.h"
#include "runtime/obf.h"
using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace nj::runtime {

// ----------------------------------------------------------------------------- zones recompilees
namespace {
struct Range { uint32_t lo, hi; };
Range g_ranges[16];
int g_nranges = 0;
}  // namespace

void add_recompiled_range(uint32_t lo, uint32_t hi) {
  if (g_nranges < 16) g_ranges[g_nranges++] = {lo, hi};
}

bool in_recompiled_code(uint32_t at) {
  for (int i = 0; i < g_nranges; ++i)
    if (at >= g_ranges[i].lo && at < g_ranges[i].hi) return true;
  return false;
}

uint64_t g_interp_insts = 0;

// ----------------------------------------------------------------------------- decodage
namespace {
inline uint32_t bits(uint32_t w, int from, int to) {   // numerotation IBM : bit 0 = poids fort
  return (w >> (31 - to)) & ((1u << (to - from + 1)) - 1);
}
inline int32_t simm(uint32_t w) { return (int16_t)(w & 0xFFFF); }
inline uint32_t uimm(uint32_t w) { return w & 0xFFFF; }
inline uint32_t rot(uint32_t v, uint32_t n) { n &= 31; return n ? (v << n) | (v >> (32 - n)) : v; }
// Masque MB..ME (numerotation IBM, avec repli quand MB > ME).
inline uint32_t span(uint32_t mb, uint32_t me) {
  const uint32_t a = 0xFFFFFFFFu >> mb, b = me == 31 ? 0xFFFFFFFFu : ~(0xFFFFFFFFu >> (me + 1));
  return mb <= me ? (a & b) : (a | b);
}

constexpr uint32_t XER_SO = 0x80000000u, XER_OV = 0x40000000u, XER_CA = 0x20000000u;

inline bool carry(const GuestCpu& c) { return (c.xer & XER_CA) != 0; }
inline void set_carry(GuestCpu& c, bool v) { c.xer = v ? (c.xer | XER_CA) : (c.xer & ~XER_CA); }
inline void set_ov(GuestCpu& c, bool v) {
  if (v) c.xer |= XER_OV | XER_SO;
  else c.xer &= ~XER_OV;
}
inline void record(GuestCpu& c, int field, int32_t v) {
  ConditionField& f = c.cr[field];
  f.lt = v < 0;
  f.gt = v > 0;
  f.eq = v == 0;
  f.so = (c.xer & XER_SO) != 0;
}
inline void compare(GuestCpu& c, int field, bool lt, bool gt) {
  ConditionField& f = c.cr[field];
  f.lt = lt;
  f.gt = gt;
  f.eq = !lt && !gt;
  f.so = (c.xer & XER_SO) != 0;
}
// Bit n (0..31, IBM) du registre de condition.
inline bool crbit(const GuestCpu& c, uint32_t n) {
  const ConditionField& f = c.cr[n >> 2];
  switch (n & 3) { case 0: return f.lt; case 1: return f.gt; case 2: return f.eq; default: return f.so; }
}
inline void set_crbit(GuestCpu& c, uint32_t n, bool v) {
  ConditionField& f = c.cr[n >> 2];
  switch (n & 3) { case 0: f.lt = v; break; case 1: f.gt = v; break; case 2: f.eq = v; break; default: f.so = v; }
}
uint32_t cr_word(const GuestCpu& c) {
  uint32_t w = 0;
  for (int i = 0; i < 8; ++i) {
    const ConditionField& f = c.cr[i];
    w |= ((f.lt << 3) | (f.gt << 2) | (f.eq << 1) | (uint32_t)f.so) << (28 - 4 * i);
  }
  return w;
}
void set_cr_field_bits(GuestCpu& c, int i, uint32_t nib) {
  ConditionField& f = c.cr[i];
  f.lt = nib & 8; f.gt = nib & 4; f.eq = nib & 2; f.so = nib & 1;
}

// Branchement conditionnel : champ BO et bit BI (bc, bclr, bcctr).
bool branch_taken(GuestCpu& c, uint32_t bo, uint32_t bi, bool uses_ctr) {
  bool ctr_ok = true;
  if (uses_ctr && !(bo & 4)) {
    --c.ctr;
    ctr_ok = ((c.ctr != 0) != ((bo & 2) != 0));
  }
  const bool cond_ok = (bo & 16) || (crbit(c, bi) == ((bo & 8) != 0));
  return ctr_ok && cond_ok;
}

uint32_t spr_num(uint32_t w) { return (bits(w, 11, 15)) | (bits(w, 16, 20) << 5); }

bool read_spr(GuestCpu& c, uint32_t n, uint32_t& v) {
  switch (n) {
    case 1: v = c.xer; return true;
    case 8: v = c.lr; return true;
    case 9: v = c.ctr; return true;
    case 26: v = c.srr0; return true;
    case 27: v = c.srr1; return true;
    case 268: v = (uint32_t)c.timebase_now(); return true;
    case 269: v = (uint32_t)(c.timebase_now() >> 32); return true;
    default:
      if (n >= 272 && n <= 275) { v = c.sprg[n - 272]; return true; }
      if (n >= 912 && n <= 919) { v = c.gqr[n - 912]; return true; }
      v = 0;   // registres de configuration (HID, WPAR, caches...) : sans effet ici
      return true;
  }
}

void write_spr(GuestCpu& c, uint32_t n, uint32_t v) {
  switch (n) {
    case 1: c.xer = v; break;
    case 8: c.lr = v; break;
    case 9: c.ctr = v; break;
    case 26: c.srr0 = v; break;
    case 27: c.srr1 = v; break;
    default:
      if (n >= 272 && n <= 275) c.sprg[n - 272] = v;
      else if (n >= 912 && n <= 919) c.gqr[n - 912] = v;
      break;
  }
}

inline float as_f32(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
inline uint32_t f32_bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
inline double as_f64(uint64_t u) { double d; std::memcpy(&d, &u, 8); return d; }
inline uint64_t f64_bits(double d) { uint64_t u; std::memcpy(&u, &d, 8); return u; }

void unsupported(GuestCpu& c, uint32_t w) {
  static int said = 0;
  if (said++ < 8)
    NJ_LOG << "[CPU] instruction non prise en charge 0x" << std::hex << w << " a 0x" << c.pc
              << " (lr 0x" << c.lr << ")" << std::dec << std::endl;
  c.alive = false;
}

// Forme X de l'opcode 31 (entiers, chargements indexes, registres speciaux).
bool exec31(GuestCpu& c, uint32_t w) {
  const uint32_t D = bits(w, 6, 10), A = bits(w, 11, 15), B = bits(w, 16, 20);
  const bool rc = w & 1, oe = bits(w, 21, 21);
  const uint32_t xo10 = bits(w, 21, 30), xo9 = bits(w, 22, 30);
  uint32_t* r = c.gpr.data();
  const uint32_t ea = (A ? r[A] : 0) + r[B];
  auto done = [&](uint32_t reg) { if (rc) record(c, 0, (int32_t)r[reg]); return true; };

  switch (xo9) {   // arithmetique (le bit OE est le bit 21)
    case 266: { const uint64_t s = (uint64_t)r[A] + r[B]; r[D] = (uint32_t)s;
      if (oe) set_ov(c, (~(r[A] ^ r[B]) & (r[A] ^ r[D])) >> 31); return done(D); }
    case 10: { const uint64_t s = (uint64_t)r[A] + r[B]; set_carry(c, s >> 32); r[D] = (uint32_t)s; return done(D); }
    case 138: { const uint64_t s = (uint64_t)r[A] + r[B] + carry(c); set_carry(c, s >> 32); r[D] = (uint32_t)s; return done(D); }
    case 40: { const uint32_t a = r[A], b = r[B]; r[D] = b - a;
      if (oe) set_ov(c, ((a ^ b) & (b ^ r[D])) >> 31); return done(D); }
    case 8: { const uint32_t a = r[A], b = r[B]; set_carry(c, b >= a); r[D] = b - a; return done(D); }
    case 136: { const uint64_t s = (uint64_t)(~r[A]) + r[B] + carry(c); set_carry(c, s >> 32); r[D] = (uint32_t)s; return done(D); }
    case 104: r[D] = 0u - r[A]; return done(D);
    case 202: { const uint64_t s = (uint64_t)r[A] + carry(c); set_carry(c, s >> 32); r[D] = (uint32_t)s; return done(D); }
    case 200: { const uint64_t s = (uint64_t)(~r[A]) + carry(c); set_carry(c, s >> 32); r[D] = (uint32_t)s; return done(D); }
    case 234: { const uint64_t s = (uint64_t)r[A] + carry(c) + 0xFFFFFFFFull; set_carry(c, s >> 32); r[D] = (uint32_t)s; return done(D); }
    case 232: { const uint64_t s = (uint64_t)(~r[A]) + carry(c) + 0xFFFFFFFFull; set_carry(c, s >> 32); r[D] = (uint32_t)s; return done(D); }
    case 235: r[D] = (uint32_t)((int32_t)r[A] * (int32_t)r[B]); return done(D);
    case 75: r[D] = (uint32_t)(((int64_t)(int32_t)r[A] * (int32_t)r[B]) >> 32); return done(D);
    case 11: r[D] = (uint32_t)(((uint64_t)r[A] * r[B]) >> 32); return done(D);
    // divisions par zero (et INT_MIN / -1) : rD inchange, comme l'emetteur (nj_emit.cpp)
    case 491: { const int32_t a = (int32_t)r[A], b = (int32_t)r[B];
      if (b != 0 && !(a == INT32_MIN && b == -1)) r[D] = (uint32_t)(a / b); return done(D); }
    case 459: if (r[B]) r[D] = r[A] / r[B]; return done(D);
  }
  switch (xo10) {
    case 0: case 32: {   // cmp, cmpl
      const int f = D >> 2;
      if (xo10 == 0) compare(c, f, (int32_t)r[A] < (int32_t)r[B], (int32_t)r[A] > (int32_t)r[B]);
      else compare(c, f, r[A] < r[B], r[A] > r[B]);
      return true;
    }
    case 28: r[A] = r[D] & r[B]; return done(A);
    case 60: r[A] = r[D] & ~r[B]; return done(A);
    case 444: r[A] = r[D] | r[B]; return done(A);
    case 412: r[A] = r[D] | ~r[B]; return done(A);
    case 316: r[A] = r[D] ^ r[B]; return done(A);
    case 124: r[A] = ~(r[D] | r[B]); return done(A);
    case 476: r[A] = ~(r[D] & r[B]); return done(A);
    case 284: r[A] = ~(r[D] ^ r[B]); return done(A);
    case 954: r[A] = (uint32_t)(int32_t)(int8_t)r[D]; return done(A);
    case 922: r[A] = (uint32_t)(int32_t)(int16_t)r[D]; return done(A);
    case 26: r[A] = r[D] ? (uint32_t)__builtin_clz(r[D]) : 32; return done(A);
    case 24: { const uint32_t n = r[B] & 63; r[A] = n > 31 ? 0 : r[D] << n; return done(A); }
    case 536: { const uint32_t n = r[B] & 63; r[A] = n > 31 ? 0 : r[D] >> n; return done(A); }
    case 792: {   // sraw
      const uint32_t n = r[B] & 63; const int32_t s = (int32_t)r[D];
      if (n > 31) { r[A] = (uint32_t)(s >> 31); set_carry(c, s < 0); }
      else { set_carry(c, s < 0 && n && ((uint32_t)s & ((1u << n) - 1))); r[A] = (uint32_t)(s >> n); }
      return done(A);
    }
    case 824: {   // srawi
      const uint32_t n = B; const int32_t s = (int32_t)r[D];
      set_carry(c, s < 0 && n && ((uint32_t)s & ((1u << n) - 1))); r[A] = (uint32_t)(s >> n); return done(A);
    }
    // chargements et rangements indexes
    case 23: r[D] = c.mmu.read32(ea); return true;
    case 55: r[D] = c.mmu.read32(ea); r[A] = ea; return true;
    case 87: r[D] = c.mmu.read8(ea); return true;
    case 119: r[D] = c.mmu.read8(ea); r[A] = ea; return true;
    case 279: r[D] = c.mmu.read16(ea); return true;
    case 311: r[D] = c.mmu.read16(ea); r[A] = ea; return true;
    case 343: r[D] = (uint32_t)(int32_t)(int16_t)c.mmu.read16(ea); return true;
    case 375: r[D] = (uint32_t)(int32_t)(int16_t)c.mmu.read16(ea); r[A] = ea; return true;
    case 534: r[D] = __builtin_bswap32(c.mmu.read32(ea)); return true;
    case 790: r[D] = __builtin_bswap16(c.mmu.read16(ea)); return true;
    case 151: c.mmu.write32(ea, r[D]); return true;
    case 183: c.mmu.write32(ea, r[D]); r[A] = ea; return true;
    case 215: c.mmu.write8(ea, (uint8_t)r[D]); return true;
    case 247: c.mmu.write8(ea, (uint8_t)r[D]); r[A] = ea; return true;
    case 407: c.mmu.write16(ea, (uint16_t)r[D]); return true;
    case 439: c.mmu.write16(ea, (uint16_t)r[D]); r[A] = ea; return true;
    case 662: c.mmu.write32(ea, __builtin_bswap32(r[D])); return true;
    case 918: c.mmu.write16(ea, __builtin_bswap16((uint16_t)r[D])); return true;
    case 535: c.fpr[D] = as_f32(c.mmu.read32(ea)); c.ps1[D] = c.fpr[D]; return true;    // lfsx
    case 599: c.fpr[D] = as_f64(c.mmu.read64(ea)); return true;                         // lfdx
    case 631: c.fpr[D] = as_f64(c.mmu.read64(ea)); r[A] = ea; return true;              // lfdux
    case 567: c.fpr[D] = as_f32(c.mmu.read32(ea)); c.ps1[D] = c.fpr[D]; r[A] = ea; return true;   // lfsux
    case 695: c.mmu.write32(ea, f32_bits((float)c.fpr[D])); r[A] = ea; return true;      // stfsux
    case 759: c.mmu.write64(ea, f64_bits(c.fpr[D])); r[A] = ea; return true;            // stfdux
    case 663: c.mmu.write32(ea, f32_bits((float)c.fpr[D])); return true;                // stfsx
    case 727: c.mmu.write64(ea, f64_bits(c.fpr[D])); return true;                       // stfdx
    case 983: c.mmu.write32(ea, (uint32_t)f64_bits(c.fpr[D])); return true;             // stfiwx
    // registres speciaux et condition
    case 339: { uint32_t v; read_spr(c, spr_num(w), v); r[D] = v; return true; }        // mfspr
    case 467: write_spr(c, spr_num(w), r[D]); return true;                              // mtspr
    case 371: r[D] = (uint32_t)(spr_num(w) == 269 ? c.timebase_now() >> 32 : c.timebase_now()); return true;
    case 19: r[D] = cr_word(c); return true;                                            // mfcr
    case 144: {                                                                         // mtcrf
      const uint32_t crm = bits(w, 12, 19);
      for (int i = 0; i < 8; ++i)
        if (crm & (0x80u >> i)) set_cr_field_bits(c, i, (r[D] >> (28 - 4 * i)) & 0xF);
      return true;
    }
    case 512: {                                                                         // mcrxr
      set_cr_field_bits(c, D >> 2, c.xer >> 28);
      c.xer &= 0x0FFFFFFFu;
      return true;
    }
    case 83: r[D] = c.msr; return true;                                                 // mfmsr
    case 146: c.msr = r[D]; return true;                                                // mtmsr
    case 595: case 659: return true;                                                    // mfsr(in) : rD inchange
    case 210: case 242: return true;                                                    // mtsr(in)
    // caches et synchronisation : sans effet sur cette machine
    case 86: case 54: case 278: case 246: case 470: case 982: case 598: case 854: case 1014: case 306: case 566:
      if (xo10 == 1014) for (uint32_t i = 0; i < 32; i += 4) c.mmu.write32((ea & ~31u) + i, 0);   // dcbz
      return true;
  }
  return false;
}

// Opcode 63 : flottant double, et opcode 59 : flottant simple. Reproduit l'emetteur (nj_emit.cpp,
// cas 59 et 63) a l'identique : une fonction passe de l'interpreteur au code natif en cours de
// partie (P3), les deux doivent donner les memes bits (banc tools/re4/banc_interp.py). Rc ignore
// (CR1 inchange), FPSCR ignore, comme l'emetteur.
bool exec_float(GuestCpu& c, uint32_t w, bool single) {
  const uint32_t D = bits(w, 6, 10), A = bits(w, 11, 15), B = bits(w, 16, 20), C = bits(w, 21, 25);
  const uint32_t xo5 = bits(w, 26, 30), xo10 = bits(w, 21, 30);
  double* f = c.fpr.data();
  const double a = f[A], b = f[B], cc = f[C];
  if (single) {
    float e;
    switch (xo5) {
      case 18: e = (float)(a / b); break;                     // fdivs
      case 20: e = (float)(a - b); break;                     // fsubs
      case 21: e = (float)(a + b); break;                     // fadds
      case 22: e = (float)std::sqrt(b); break;                // fsqrts
      case 24: e = (float)(1.0f / b); break;                  // fres
      case 25: e = (float)(a * cc); break;                    // fmuls
      case 28: e = (float)std::fma(a, cc, -b); break;         // fmsubs
      case 29: e = (float)std::fma(a, cc, b); break;          // fmadds
      case 30: e = (float)(-std::fma(a, cc, -b)); break;      // fnmsubs
      case 31: e = (float)(-std::fma(a, cc, b)); break;       // fnmadds
      default: return false;
    }
    f[D] = e;
    c.ps1[D] = f[D];
    return true;
  }
  switch (xo5) {
    case 18: f[D] = a / b; return true;                       // fdiv
    case 20: f[D] = a - b; return true;                       // fsub
    case 21: f[D] = a + b; return true;                       // fadd
    case 22: f[D] = std::sqrt(b); return true;                // fsqrt
    case 23: f[D] = (a >= 0.0) ? cc : b; return true;         // fsel
    case 25: f[D] = a * cc; return true;                      // fmul
    case 26: f[D] = 1.0 / std::sqrt(b); return true;          // frsqrte
    // multiplication-addition en double : produit puis somme arrondis separement (emetteur)
    case 28: f[D] = a * cc - b; return true;                  // fmsub
    case 29: f[D] = a * cc + b; return true;                  // fmadd
    case 30: f[D] = -(a * cc - b); return true;               // fnmsub
    case 31: f[D] = -(a * cc + b); return true;               // fnmadd
  }
  switch (xo10) {
    case 0: case 32: {                                        // fcmpu, fcmpo
      ConditionField& cf = c.cr[D >> 2];
      const bool un = std::isnan(a) || std::isnan(b);
      cf.lt = !un && a < b; cf.gt = !un && a > b; cf.eq = !un && a == b; cf.so = un;
      return true;
    }
    case 12: f[D] = (double)(float)b; c.ps1[D] = f[D]; return true;   // frsp
    case 14: case 15: {                                       // fctiw, fctiwz
      const double v = xo10 == 14 ? std::nearbyint(b) : b;
      const int32_t k = !(v > -2147483649.0) ? INT32_MIN : (v >= 2147483648.0 ? INT32_MAX : (int32_t)v);
      f[D] = as_f64(0xFFF8000000000000ull | (uint32_t)k);
      return true;
    }
    case 40: f[D] = -b; return true;                          // fneg
    case 72: f[D] = b; return true;                           // fmr
    case 136: f[D] = -std::fabs(b); return true;              // fnabs
    case 264: f[D] = std::fabs(b); return true;               // fabs
    case 583: f[D] = 0.0; return true;                        // mffs (FPSCR ignore)
    case 711: case 38: case 70: case 134: return true;        // mtfsf, mtfsb1, mtfsb0, mtfsfi
  }
  return false;
}

// Opcode 4 (instructions appariees) : memes methodes de GuestCpu que le code genere.
bool exec_paired(GuestCpu& c, uint32_t w) {
  const uint32_t D = bits(w, 6, 10), A = bits(w, 11, 15), B = bits(w, 16, 20), C = bits(w, 21, 25);
  const uint32_t x10 = bits(w, 21, 30), x6 = bits(w, 25, 30), x5 = bits(w, 26, 30);
  uint32_t* r = c.gpr.data();
  switch (x10) {
    case 0: c.ps_cmpu0(D >> 2, A, B); return true;
    case 32: c.ps_cmpo0(D >> 2, A, B); return true;
    case 64: c.ps_cmpu1(D >> 2, A, B); return true;
    case 96: c.ps_cmpo1(D >> 2, A, B); return true;
    case 40: c.ps_neg(D, B); return true;
    case 72: c.ps_mr(D, B); return true;
    case 136: c.ps_nabs(D, B); return true;
    case 264: c.ps_abs(D, B); return true;
    case 528: c.ps_merge00(D, A, B); return true;
    case 560: c.ps_merge01(D, A, B); return true;
    case 592: c.ps_merge10(D, A, B); return true;
    case 624: c.ps_merge11(D, A, B); return true;
    case 1014: return true;                                   // dcbz_l : sans effet (emetteur)
  }
  if (x6 == 6 || x6 == 7 || x6 == 38 || x6 == 39) {           // psq_lx psq_stx psq_lux psq_stux
    const uint32_t W = bits(w, 21, 21), Iq = bits(w, 22, 24);
    const bool upd = x6 >= 38, st = x6 & 1;
    const uint32_t ea = upd ? r[A] + r[B] : (A ? r[A] : 0) + r[B];
    if (st) c.psq_store(D, ea, W, Iq); else c.psq_load(D, ea, W, Iq);
    if (upd) r[A] = r[A] + r[B];
    return true;
  }
  switch (x5) {
    case 10: c.ps_sum0(D, A, C, B); return true;
    case 11: c.ps_sum1(D, A, C, B); return true;
    case 12: c.ps_muls0(D, A, C); return true;
    case 13: c.ps_muls1(D, A, C); return true;
    case 14: c.ps_madds0(D, A, C, B); return true;
    case 15: c.ps_madds1(D, A, C, B); return true;
    case 18: c.ps_div(D, A, B); return true;
    case 20: c.ps_sub(D, A, B); return true;
    case 21: c.ps_add(D, A, B); return true;
    case 23: c.ps_sel(D, A, C, B); return true;
    case 24: c.fpr[D] = (double)(float)(1.0 / c.fpr[B]); c.ps1[D] = (double)(float)(1.0 / c.ps1[B]); return true;   // ps_res
    case 25: c.ps_mul(D, A, C); return true;
    case 26: c.fpr[D] = (double)(float)(1.0 / std::sqrt(c.fpr[B]));                                                  // ps_rsqrte
      c.ps1[D] = (double)(float)(1.0 / std::sqrt(c.ps1[B])); return true;
    case 28: c.ps_msub(D, A, C, B); return true;
    case 29: c.ps_madd(D, A, C, B); return true;
    case 30: c.ps_nmsub(D, A, C, B); return true;
    case 31: c.ps_nmadd(D, A, C, B); return true;
  }
  return false;
}

// psq_l psq_lu psq_st psq_stu (56, 57, 60, 61).
bool exec_psq(GuestCpu& c, uint32_t w) {
  const uint32_t p = w >> 26, D = bits(w, 6, 10), A = bits(w, 11, 15);
  const uint32_t W = bits(w, 16, 16), Iq = bits(w, 17, 19);
  int32_t d = (int32_t)(w & 0xFFF);
  if (d & 0x800) d -= 0x1000;
  uint32_t* r = c.gpr.data();
  const bool upd = p == 57 || p == 61, st = p >= 60;
  const uint32_t ea = upd ? r[A] + (uint32_t)d : (A ? r[A] : 0) + (uint32_t)d;
  if (st) c.psq_store(D, ea, W, Iq); else c.psq_load(D, ea, W, Iq);
  if (upd) r[A] = r[A] + (uint32_t)d;
  return true;
}
}  // namespace

// Une instruction a ctx.pc ; pc avance (ou saute). Faux si l'instruction est inconnue.
bool interp_one_insn(GuestCpu& c) {
  const uint32_t pc = c.pc, w = c.mmu.read32(pc);
  uint32_t* r = c.gpr.data();
  const uint32_t D = bits(w, 6, 10), A = bits(w, 11, 15);
  const uint32_t base = A ? r[A] : 0;
  c.pc = pc + 4;
  switch (w >> 26) {
    case 14: r[D] = base + simm(w); return true;                                   // addi
    case 15: r[D] = base + ((uint32_t)uimm(w) << 16); return true;                 // addis
    case 12: case 13: {                                                            // addic(.)
      const uint64_t s = (uint64_t)r[A] + (uint32_t)simm(w);
      set_carry(c, s >> 32); r[D] = (uint32_t)s;
      if ((w >> 26) == 13) record(c, 0, (int32_t)r[D]);
      return true;
    }
    case 8: { const uint32_t a = r[A], i = (uint32_t)simm(w); set_carry(c, i >= a); r[D] = i - a; return true; }   // subfic
    case 7: r[D] = (uint32_t)((int32_t)r[A] * simm(w)); return true;               // mulli
    case 11: compare(c, D >> 2, (int32_t)r[A] < simm(w), (int32_t)r[A] > simm(w)); return true;      // cmpi
    case 10: compare(c, D >> 2, r[A] < uimm(w), r[A] > uimm(w)); return true;      // cmpli
    case 24: r[A] = r[D] | uimm(w); return true;                                   // ori
    case 25: r[A] = r[D] | (uimm(w) << 16); return true;                           // oris
    case 26: r[A] = r[D] ^ uimm(w); return true;                                   // xori
    case 27: r[A] = r[D] ^ (uimm(w) << 16); return true;                           // xoris
    case 28: r[A] = r[D] & uimm(w); record(c, 0, (int32_t)r[A]); return true;      // andi.
    case 29: r[A] = r[D] & (uimm(w) << 16); record(c, 0, (int32_t)r[A]); return true;   // andis.
    case 21: case 23: case 20: {                                                   // rlwinm, rlwnm, rlwimi
      const uint32_t sh = (w >> 26) == 23 ? (r[bits(w, 16, 20)] & 31) : bits(w, 16, 20);
      const uint32_t m = span(bits(w, 21, 25), bits(w, 26, 30));
      const uint32_t v = rot(r[D], sh);
      r[A] = (w >> 26) == 20 ? ((v & m) | (r[A] & ~m)) : (v & m);
      if (w & 1) record(c, 0, (int32_t)r[A]);
      return true;
    }
    // chargements et rangements
    case 32: r[D] = c.mmu.read32(base + simm(w)); return true;
    case 33: { const uint32_t ea = r[A] + simm(w); r[D] = c.mmu.read32(ea); r[A] = ea; return true; }
    case 34: r[D] = c.mmu.read8(base + simm(w)); return true;
    case 35: { const uint32_t ea = r[A] + simm(w); r[D] = c.mmu.read8(ea); r[A] = ea; return true; }
    case 40: r[D] = c.mmu.read16(base + simm(w)); return true;
    case 41: { const uint32_t ea = r[A] + simm(w); r[D] = c.mmu.read16(ea); r[A] = ea; return true; }
    case 42: r[D] = (uint32_t)(int32_t)(int16_t)c.mmu.read16(base + simm(w)); return true;
    case 43: { const uint32_t ea = r[A] + simm(w); r[D] = (uint32_t)(int32_t)(int16_t)c.mmu.read16(ea); r[A] = ea; return true; }
    case 36: c.mmu.write32(base + simm(w), r[D]); return true;
    case 37: { const uint32_t ea = r[A] + simm(w); c.mmu.write32(ea, r[D]); r[A] = ea; return true; }
    case 38: c.mmu.write8(base + simm(w), (uint8_t)r[D]); return true;
    case 39: { const uint32_t ea = r[A] + simm(w); c.mmu.write8(ea, (uint8_t)r[D]); r[A] = ea; return true; }
    case 44: c.mmu.write16(base + simm(w), (uint16_t)r[D]); return true;
    case 45: { const uint32_t ea = r[A] + simm(w); c.mmu.write16(ea, (uint16_t)r[D]); r[A] = ea; return true; }
    case 46: { uint32_t ea = base + simm(w); for (uint32_t i = D; i < 32; ++i, ea += 4) r[i] = c.mmu.read32(ea); return true; }   // lmw
    case 47: { uint32_t ea = base + simm(w); for (uint32_t i = D; i < 32; ++i, ea += 4) c.mmu.write32(ea, r[i]); return true; }  // stmw
    case 48: c.fpr[D] = as_f32(c.mmu.read32(base + simm(w))); c.ps1[D] = c.fpr[D]; return true;    // lfs
    case 49: { const uint32_t ea = r[A] + simm(w); c.fpr[D] = as_f32(c.mmu.read32(ea)); c.ps1[D] = c.fpr[D]; r[A] = ea; return true; }
    case 50: c.fpr[D] = as_f64(c.mmu.read64(base + simm(w))); return true;                         // lfd
    case 51: { const uint32_t ea = r[A] + simm(w); c.fpr[D] = as_f64(c.mmu.read64(ea)); r[A] = ea; return true; }
    case 52: c.mmu.write32(base + simm(w), f32_bits((float)c.fpr[D])); return true;               // stfs
    case 53: { const uint32_t ea = r[A] + simm(w); c.mmu.write32(ea, f32_bits((float)c.fpr[D])); r[A] = ea; return true; }
    case 54: c.mmu.write64(base + simm(w), f64_bits(c.fpr[D])); return true;                      // stfd
    case 55: { const uint32_t ea = r[A] + simm(w); c.mmu.write64(ea, f64_bits(c.fpr[D])); r[A] = ea; return true; }
    // branchements
    case 18: {                                                                     // b, bl, ba, bla
      int32_t li = (int32_t)(w & 0x03FFFFFC);
      if (li & 0x02000000) li -= 0x04000000;
      if (w & 1) c.lr = pc + 4;
      c.pc = (w & 2) ? (uint32_t)li : pc + (uint32_t)li;
      return true;
    }
    case 16: {                                                                     // bc
      const bool take = branch_taken(c, D, A, true);
      if (w & 1) c.lr = pc + 4;
      if (take) c.pc = (w & 2) ? (uint32_t)(int16_t)(w & 0xFFFC) : pc + (uint32_t)(int16_t)(w & 0xFFFC);
      return true;
    }
    case 19: {
      const uint32_t xo = bits(w, 21, 30), B = bits(w, 16, 20);
      switch (xo) {
        case 16: { const uint32_t target = c.lr & ~3u; const bool take = branch_taken(c, D, A, true);   // bclr
          if (w & 1) c.lr = pc + 4; if (take) c.pc = target; return true; }
        case 528: { const bool take = branch_taken(c, D, A, false);                                       // bcctr
          if (w & 1) c.lr = pc + 4; if (take) c.pc = c.ctr & ~3u; return true; }
        case 257: set_crbit(c, D, crbit(c, A) && crbit(c, B)); return true;
        case 449: set_crbit(c, D, crbit(c, A) || crbit(c, B)); return true;
        case 193: set_crbit(c, D, crbit(c, A) != crbit(c, B)); return true;
        case 225: set_crbit(c, D, !(crbit(c, A) && crbit(c, B))); return true;
        case 33: set_crbit(c, D, !(crbit(c, A) || crbit(c, B))); return true;
        case 289: set_crbit(c, D, crbit(c, A) == crbit(c, B)); return true;
        case 129: set_crbit(c, D, crbit(c, A) && !crbit(c, B)); return true;
        case 417: set_crbit(c, D, crbit(c, A) || !crbit(c, B)); return true;
        case 0: c.cr[D >> 2] = c.cr[A >> 2]; return true;                          // mcrf
        case 150: return true;                                                     // isync
        case 50: c.msr = c.srr1; c.pc = c.srr0 & ~3u; return true;                 // rfi
      }
      break;
    }
    case 17: return true;                                                          // sc : rien a appeler ici
    case 3: return true;                                                           // twi : jamais declenche
    case 31: if (exec31(c, w)) return true; break;
    case 59: if (exec_float(c, w, true)) return true; break;
    case 4: if (exec_paired(c, w)) return true; break;
    case 56: case 57: case 60: case 61: return exec_psq(c, w);
    case 63: if (exec_float(c, w, false)) return true; break;
  }
  c.pc = pc;
  unsupported(c, w);
  return false;
}

// Mot de donnees au milieu d'une fonction recompilee (opcode 0 ou 1) : sans effet, on avance.
void interp_single(GuestCpu& c, uint32_t opcode, uint32_t pc) {
  if (opcode <= 1) {
    c.pc = pc + 4;
    return;
  }
  c.pc = pc;
  interp_one_insn(c);
}

// Code hors de toute fonction recompilee : on l'execute jusqu'a retomber dans du code recompile.
void interp_advance(GuestCpu& c) {
  static int notes = 0;
  if (notes++ < 4)
    NJ_LOG << "[CPU] execution hors code recompile a 0x" << std::hex << c.pc << " (lr 0x" << c.lr
              << ")" << std::dec << std::endl;
  for (uint32_t n = 0; n < 50000000u && c.alive; ++n) {
    const uint32_t before = c.pc;
    const uint32_t ee = c.msr & 0x8000;
    ++c.retired;
    ++g_interp_insts;
    if (!interp_one_insn(c)) return;
    // Interruptions : servies quand elles viennent d'etre autorisees, et de temps en temps.
    if ((!ee && (c.msr & 0x8000)) || (n & 0xFFF) == 0xFFF)
      if (drain_callbacks(c) && in_recompiled_code(c.pc)) return;
    if (c.pc != before + 4 && in_recompiled_code(c.pc)) return;
  }
  if (c.alive)
    NJ_LOG << "[CPU] execution hors code recompile trop longue, arret a 0x" << std::hex << c.pc
              << std::dec << std::endl;
}

// Une seule instruction (P3 : fonctions non branchees, nj_interp) ; false si elle est inconnue.
bool interpret_single(GuestCpu& c) {
  ++g_interp_insts;
  return interp_one_insn(c);
}

void interp_report(std::ostream& os) {
  if (g_interp_insts) os << "[CPU] instructions hors code recompile : "_g << g_interp_insts << "\n";
}

}  // namespace nj::runtime
