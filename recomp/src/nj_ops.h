// nj_ops.h : aides du code genere par njrecomp (copie dans le projet genere).
// Semantique d'apres le manuel IBM PowerPC 750CL. XER : SO bit 31, OV bit 30, CA bit 29.
#pragma once
#include "runtime/cpu_context.h"
#include <cmath>
#include <cstdint>
#include <cstring>

// Toujours developpe : dans les fonctions generees geantes, le compilateur renoncait a
// developper ces aides (appels a nj::cr_unsigned... au profil du Galaxy S5, 29/09).
#define NJ_AI __attribute__((always_inline)) inline

namespace nj {

using nj::runtime::GuestCpu;

// psq specialises par valeur de GQR (njrecomp psq_const, nj_emit.cpp psq_call). Pour une valeur G
// connue a la compilation : memes conversions et saturations que le chemin direct de
// GuestCpu::psq_load_slow / psq_store_slow (types 0 flottant, 4 u8, 5 u16, 6 s8, 7 s16 ; echelle
// = exposant signe de 6 bits), sur MEM1 ou le cache verrouille (fast_ptr). Rend false si
// l'adresse n'y est pas : l'appelant passe alors par l'appel generique.
template <uint32_t G>
NJ_AI bool psq_ld_c(GuestCpu& c, uint32_t d, uint32_t a, uint32_t W) {
    constexpr uint32_t t = (G >> 16) & 7;
    static_assert(t == 0 || t >= 4, "psq : type de chargement reserve");
    constexpr uint32_t es = t == 0 ? 4 : ((t == 4 || t == 6) ? 1 : 2);
    const uint8_t* p = c.mmu.fast_ptr(a, W ? es : 2 * es);
    if (!p) return false;
    constexpr int sc0 = int((G >> 24) & 0x3F);
    constexpr int sc = (sc0 & 0x20) ? sc0 - 0x40 : sc0;
    const float scale = GuestCpu::pow2_scale(-sc);
    auto elem = [&](const uint8_t* q) -> double {
        if constexpr (t == 0) {
            const uint32_t b = nj::runtime::MMU::be32(q);
            float f;
            std::memcpy(&f, &b, 4);
            return f;
        } else if constexpr (t == 4) {
            return q[0] * scale;
        } else if constexpr (t == 5) {
            return nj::runtime::MMU::be16(q) * scale;
        } else if constexpr (t == 6) {
            return (int8_t)q[0] * scale;
        } else {
            return (int16_t)nj::runtime::MMU::be16(q) * scale;
        }
    };
    c.fpr[d] = elem(p);
    c.ps1[d] = W ? 1.0 : elem(p + es);
    return true;
}
template <uint32_t G>
NJ_AI bool psq_st_c(GuestCpu& c, uint32_t s, uint32_t a, uint32_t W) {
    constexpr uint32_t t = G & 7;
    static_assert(t == 0 || t >= 4, "psq : type d'ecriture reserve");
    if (NJ_WATCHING()) return false;
    constexpr uint32_t es = t == 0 ? 4 : ((t == 4 || t == 6) ? 1 : 2);
    uint8_t* p = c.mmu.fast_ptr(a, W ? es : 2 * es);
    if (!p) return false;
    constexpr int sc0 = int((G >> 8) & 0x3F);
    constexpr int sc = (sc0 & 0x20) ? sc0 - 0x40 : sc0;
    const float scale = GuestCpu::pow2_scale(sc);
    auto satd = [](double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); };
    auto put = [&](uint8_t* q, double val) {
        if constexpr (t == 0) {
            const float f = (float)val;
            uint32_t b;
            std::memcpy(&b, &f, 4);
            nj::runtime::MMU::put32(q, b);
        } else if constexpr (t == 4) {
            q[0] = (uint8_t)satd(val * scale, 0.0, 255.0);
        } else if constexpr (t == 5) {
            nj::runtime::MMU::put16(q, (uint16_t)satd(val * scale, 0.0, 65535.0));
        } else if constexpr (t == 6) {
            q[0] = (uint8_t)(int8_t)satd(val * scale, -128.0, 127.0);
        } else {
            nj::runtime::MMU::put16(q, (uint16_t)(int16_t)satd(val * scale, -32768.0, 32767.0));
        }
    };
    put(p, c.fpr[s]);
    if (W == 0) put(p + es, c.ps1[s]);
    return true;
}
template <uint32_t... Gs>
NJ_AI void psq_l_k(GuestCpu& c, uint32_t I, uint32_t d, uint32_t a, uint32_t W) {
    const uint32_t g = c.gqr[I];
    if (((g == Gs && psq_ld_c<Gs>(c, d, a, W)) || ...)) return;
    c.psq_load(d, a, W, I);
}
template <uint32_t... Gs>
NJ_AI void psq_s_k(GuestCpu& c, uint32_t I, uint32_t s, uint32_t a, uint32_t W) {
    const uint32_t g = c.gqr[I];
    if (((g == Gs && psq_st_c<Gs>(c, s, a, W)) || ...)) return;
    c.psq_store(s, a, W, I);
}

constexpr uint32_t XER_SO = 0x80000000u, XER_OV = 0x40000000u, XER_CA = 0x20000000u;

// Le bit SO du champ recoit XER[SO] (manuel, et l'ancien moteur fait de meme).
// Le champ (4 booleens lt, gt, eq, so contigus) est ecrit d'un seul mot de 32 bits au lieu de
// quatre octets : les comparaisons pesaient ~9 % du fil du jeu au Galaxy S5 (29/09). Meme
// contenu octet par octet (petit-boutiste : lt a l'octet 0, so a l'octet 3).
static_assert(sizeof(nj::runtime::ConditionField) == 4, "ConditionField : 4 octets");
NJ_AI void cr_store(GuestCpu& c, int f, bool lt, bool gt, bool eq) {
    const uint32_t v = (uint32_t)lt | ((uint32_t)gt << 8) | ((uint32_t)eq << 16) |
                       ((c.xer >> 31) << 24);
    std::memcpy(&c.cr[f], &v, 4);
}
NJ_AI void cr_signed(GuestCpu& c, int f, int32_t a, int32_t b) {
    cr_store(c, f, a < b, a > b, a == b);
}
NJ_AI void cr_unsigned(GuestCpu& c, int f, uint32_t a, uint32_t b) {
    cr_store(c, f, a < b, a > b, a == b);
}
// Rc = 1 : CR0 d'apres le resultat signe.
NJ_AI void cr0(GuestCpu& c, uint32_t r) { cr_signed(c, 0, int32_t(r), 0); }

NJ_AI void set_ca(GuestCpu& c, bool ca) { c.xer = ca ? (c.xer | XER_CA) : (c.xer & ~XER_CA); }
NJ_AI uint32_t get_ca(const GuestCpu& c) { return (c.xer >> 29) & 1u; }
// a + b + ci avec retenue sortante
NJ_AI uint32_t add_ca(GuestCpu& c, uint32_t a, uint32_t b, uint32_t ci) {
    const uint64_t s = uint64_t(a) + uint64_t(b) + ci;
    set_ca(c, (s >> 32) != 0);
    return uint32_t(s);
}
// Debordement signe (OE = 1) : SO est collant.
NJ_AI void set_ov(GuestCpu& c, bool ov) {
    c.xer = ov ? (c.xer | XER_OV | XER_SO) : (c.xer & ~XER_OV);
}

// Bit n (0 = poids fort) du registre de condition, et son ecriture.
NJ_AI bool crbit(const GuestCpu& c, int n) {
    const auto& f = c.cr[n >> 2];
    switch (n & 3) {
    case 0: return f.lt;
    case 1: return f.gt;
    case 2: return f.eq;
    default: return f.so;
    }
}
NJ_AI void set_crbit(GuestCpu& c, int n, bool v) {
    auto& f = c.cr[n >> 2];
    switch (n & 3) {
    case 0: f.lt = v; break;
    case 1: f.gt = v; break;
    case 2: f.eq = v; break;
    default: f.so = v; break;
    }
}
// mfcr / mtcrf, sans branchement. Un champ = un mot de 4 booleens (lt octet 0 ... so octet 3,
// voir cr_store). Rassembler : (mot & 0x01010101) * 0x08040201 met lt au bit 27, gt au 26, eq au
// 25, so au 24, sans collision sur ces quatre bits ; >> 24 donne le quartet lt gt eq so. L'ancienne
// boucle (32 tests d'octets et branchements) faisait ~100 instructions ARM par mfcr, dans le
// prologue de nombreuses fonctions de RE4 (649 mfcr dans le code genere).
NJ_AI uint32_t cr_field_nibble(const GuestCpu& c, int i) {
    uint32_t w;
    std::memcpy(&w, &c.cr[i], 4);
    return (((w & 0x01010101u) * 0x08040201u) >> 24) & 0xFu;
}
NJ_AI uint32_t cr_pack(const GuestCpu& c) {
    uint32_t v = 0;
    for (int i = 0; i < 8; ++i)
        v |= cr_field_nibble(c, i) << (28 - 4 * i);
    return v;
}
NJ_AI void cr_unpack_field(GuestCpu& c, int i, uint32_t v) {
    const uint32_t n = (v >> (28 - 4 * i)) & 0xF;
    const uint32_t w = ((n >> 3) & 1u) | ((n << 6) & 0x100u) | ((n << 15) & 0x10000u) |
                       ((n << 24) & 0x1000000u);
    std::memcpy(&c.cr[i], &w, 4);
}

NJ_AI uint32_t rotl(uint32_t v, uint32_t n) { n &= 31; return n ? (v << n) | (v >> (32 - n)) : v; }

// Comparaison flottante : champ de CR, non ordonne -> SO.
NJ_AI void fcmp(GuestCpu& c, int f, double a, double b) {
    const bool un = std::isnan(a) || std::isnan(b);
    c.cr[f].lt = !un && a < b;
    c.cr[f].gt = !un && a > b;
    c.cr[f].eq = !un && a == b;
    c.cr[f].so = un;
}

NJ_AI double as_double(uint64_t u) { double d; std::memcpy(&d, &u, 8); return d; }
NJ_AI uint64_t as_u64(double d) { uint64_t u; std::memcpy(&u, &d, 8); return u; }

// Acces memoire du code genere avec la base de MEM1 dans une variable locale (nj_fm, declaree en
// tete de chaque fonction, config local_mem_base). Par ctx.mmu, la base (mem1.data()) etait
// relue en memoire apres chaque chemin lent, puisque cet appel peut tout modifier pour le
// compilateur ; MEM1 n'est jamais reallouee. Memes chemins lents que MMU. S5, output_10 compile
// (30/09) : -3,3 % d'instructions, -13 % de chargements.
struct FM {
    uint8_t* const b;   // ctx.mmu.mem1.data()
    nj::runtime::MMU& m;
    NJ_AI uint8_t* f(uint32_t a) const {
        const uint32_t off = (a & 0xBFFFFFFFu) - 0x80000010u;   // comme MMU::mem1_fast
        return off < nj::runtime::MMU::kMem1Size - 0x10u ? b + 0x10u + off : nullptr;
    }
    NJ_AI uint8_t* fast_ptr(uint32_t a, uint32_t n) const {
        if (uint8_t* p = f(a)) return f(a + n - 1) ? p : nullptr;
        return m.fast_ptr(a, n);
    }
    // Acces gardes (sites listes, fonctions gardees) : avant l'appel lent, deux raccourcis en ligne
    // qui font exactement ce que ferait le chemin lent (30/09, bc9) :
    //  - cache verrouille 0xE0000000 (palette de matrices des boucles de peau) : l2_cache ;
    //  - ecriture au tuyau GX 0xCC008xxx : ajout au tampon d'ecriture (g_nj_wgp), memes octets dans
    //    le meme ordre que GX_WGPIPE_Write*, tant qu'il y a la place et que le tuyau n'est pas
    //    redirige (sinon lim = debut du tampon et on passe par l'appel).
    NJ_AI uint8_t* l2(uint32_t a, uint32_t n) const {
        const uint32_t l = a - 0xE0000000u;
        return l <= 0x40000u - n ? m.l2_cache.data() + l : nullptr;
    }
    static NJ_AI uint8_t* wgp(uint32_t a, uint32_t n) {
        if ((a & 0x3FFFF000u) != 0x0C008000u) return nullptr;
        nj::runtime::NjWgp& w = nj::runtime::g_nj_wgp;
        if (w.p + n > w.lim) return nullptr;
        uint8_t* p = w.p;
        w.p = p + n;
        return p;
    }
    NJ_AI uint8_t read8(uint32_t a) const {
        if (uint8_t* p = f(a)) [[likely]] return *p;
        if (uint8_t* p = l2(a, 1)) return *p;
        return m.read8_slow(a);
    }
    NJ_AI uint16_t read16(uint32_t a) const {
        if (uint8_t* p = f(a)) [[likely]] return m.be16(p);
        if (uint8_t* p = l2(a, 2)) return m.be16(p);
        return m.read16_slow(a);
    }
    NJ_AI uint32_t read32(uint32_t a) const {
        if (uint8_t* p = f(a)) [[likely]] return m.be32(p);
        if (uint8_t* p = l2(a, 4)) return m.be32(p);
        return m.read32_slow(a);
    }
    NJ_AI float read_f32(uint32_t a) const { const uint32_t v = read32(a); float x; std::memcpy(&x, &v, 4); return x; }
    NJ_AI double read_f64(uint32_t a) const {
        if (uint8_t* p = f(a)) [[likely]] return as_double(m.be64(p));
        if (uint8_t* p = l2(a, 8)) return as_double(m.be64(p));
        return m.read_f64_slow(a);
    }
    NJ_AI void write8(uint32_t a, uint8_t v) const {
        if (!NJ_WATCHING()) [[likely]] {
            if (uint8_t* p = f(a)) [[likely]] { *p = v; return; }
            if (uint8_t* p = l2(a, 1)) { *p = v; return; }
            if (uint8_t* p = wgp(a, 1)) { *p = v; return; }
        }
        m.write8_slow(a, v);
    }
    NJ_AI void write16(uint32_t a, uint16_t v) const {
        if (!NJ_WATCHING()) [[likely]] {
            if (uint8_t* p = f(a)) [[likely]] { m.put16(p, v); return; }
            if (uint8_t* p = l2(a, 2)) { m.put16(p, v); return; }
            if (uint8_t* p = wgp(a, 2)) { m.put16(p, v); return; }
        }
        m.write16_slow(a, v);
    }
    NJ_AI void write32(uint32_t a, uint32_t v) const {
        if (!NJ_WATCHING()) [[likely]] {
            if (uint8_t* p = f(a)) [[likely]] { m.put32(p, v); return; }
            if (uint8_t* p = l2(a, 4)) { m.put32(p, v); return; }
            if (uint8_t* p = wgp(a, 4)) { m.put32(p, v); return; }
        }
        m.write32_slow(a, v);
    }
    NJ_AI void write_f32(uint32_t a, float v) const {
        uint32_t x;
        std::memcpy(&x, &v, 4);
        if (!NJ_WATCHING()) [[likely]] {
            if (uint8_t* p = f(a)) [[likely]] { m.put32(p, x); return; }
            if (uint8_t* p = l2(a, 4)) { m.put32(p, x); return; }
            if (uint8_t* p = wgp(a, 4)) { m.put32(p, x); return; }
        }
        m.write_f32_slow(a, v);
    }
    NJ_AI void write_f64(uint32_t a, double v) const {
        if (!NJ_WATCHING()) [[likely]]
            if (uint8_t* p = f(a)) [[likely]] { m.put64(p, as_u64(v)); return; }
        m.write_f64_slow(a, v);
    }

    // --- fastmem (njrecomp fastmem_guard) : acces sans controle ni appel, base + (a & 0x0FFFFFFF)
    // (fenetre de 256 Mo, GuestRam). x* : rapide. c* : build paranoiaque (NJ_FASTMEM_CHECK), qui
    // verifie que l'adresse est bien en MEM1, sinon signale le site (nj_fm_violation) et passe par
    // le chemin complet. Le code genere ecrit NJ_FM(r32, site) ea) : voir la macro plus bas.
    NJ_AI uint8_t* x(uint32_t a) const { return b + (a & 0x0FFFFFFFu); }
    static NJ_AI bool ok(uint32_t a) {
        const uint32_t h = a >> 28, o = a & 0x0FFFFFFFu;
        return (h == 8 || h == 0xC) && o >= 0x10u && o < nj::runtime::MMU::kMem1Size;
    }
    NJ_AI uint8_t xr8(uint32_t a) const { return *x(a); }
    NJ_AI uint16_t xr16(uint32_t a) const { return m.be16(x(a)); }
    NJ_AI uint32_t xr32(uint32_t a) const { return m.be32(x(a)); }
    NJ_AI float xrf32(uint32_t a) const { const uint32_t v = m.be32(x(a)); float f; std::memcpy(&f, &v, 4); return f; }
    NJ_AI double xrf64(uint32_t a) const { return as_double(m.be64(x(a))); }
    NJ_AI void xw8(uint32_t a, uint8_t v) const { *x(a) = v; }
    NJ_AI void xw16(uint32_t a, uint16_t v) const { m.put16(x(a), v); }
    NJ_AI void xw32(uint32_t a, uint32_t v) const { m.put32(x(a), v); }
    NJ_AI void xwf32(uint32_t a, float v) const { uint32_t u; std::memcpy(&u, &v, 4); m.put32(x(a), u); }
    NJ_AI void xwf64(uint32_t a, double v) const { m.put64(x(a), as_u64(v)); }
#define NJ_FM_CHECKED(op, T, ...)                                               \
    if (ok(a) && ok(a + sizeof(T) - 1)) [[likely]] return x##op(__VA_ARGS__); \
    nj::runtime::nj_fm_violation(s, a)
    NJ_AI uint8_t cr8(uint32_t s, uint32_t a) const { NJ_FM_CHECKED(r8, uint8_t, a); return read8(a); }
    NJ_AI uint16_t cr16(uint32_t s, uint32_t a) const { NJ_FM_CHECKED(r16, uint16_t, a); return read16(a); }
    NJ_AI uint32_t cr32(uint32_t s, uint32_t a) const { NJ_FM_CHECKED(r32, uint32_t, a); return read32(a); }
    NJ_AI float crf32(uint32_t s, uint32_t a) const { NJ_FM_CHECKED(rf32, float, a); return read_f32(a); }
    NJ_AI double crf64(uint32_t s, uint32_t a) const { NJ_FM_CHECKED(rf64, double, a); return read_f64(a); }
    NJ_AI void cw8(uint32_t s, uint32_t a, uint8_t v) const { NJ_FM_CHECKED(w8, uint8_t, a, v); write8(a, v); }
    NJ_AI void cw16(uint32_t s, uint32_t a, uint16_t v) const { NJ_FM_CHECKED(w16, uint16_t, a, v); write16(a, v); }
    NJ_AI void cw32(uint32_t s, uint32_t a, uint32_t v) const { NJ_FM_CHECKED(w32, uint32_t, a, v); write32(a, v); }
    NJ_AI void cwf32(uint32_t s, uint32_t a, float v) const { NJ_FM_CHECKED(wf32, float, a, v); write_f32(a, v); }
    NJ_AI void cwf64(uint32_t s, uint32_t a, double v) const { NJ_FM_CHECKED(wf64, double, a, v); write_f64(a, v); }
#undef NJ_FM_CHECKED
};

}  // namespace nj

// NJ_FM(r32, 0x800C8754u) ea) -> nj_fm.xr32(ea) ou, avec NJ_FASTMEM_CHECK, nj_fm.cr32(0x800C8754u, ea).
#ifdef NJ_FASTMEM_CHECK
#define NJ_FM(op, site) nj_fm.c##op(site,
#else
#define NJ_FM(op, site) nj_fm.x##op(
#endif
