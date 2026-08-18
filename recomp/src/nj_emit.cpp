// Emetteur de njrecomp, v1 (docs/RECOMPILATEUR.md) : semantique d'apres le manuel IBM PowerPC
// 750CL ; contrat avec le runtime actuel (appels, retours, verifications d'interruption, sc,
// rfi, mtmsr, correctifs REL) tel que releve sur le code genere. Les ecarts a la console que
// l'ancien moteur avait (SPRG, DSISR, dcbz, stfiwx) sont gardes en v1 pour que la batterie ne
// compare qu'un changement a la fois ; ils sont listes dans docs/RECOMPILATEUR.md.
#include "nj_emit.h"

#include "nj_image.h"

#include <cstdio>
#include "runtime/obf.h"
#include "runtime/diag.h"
using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

namespace nj {

namespace {

std::string hx(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%08X"_g, v);
    return b;
}
std::string num(int64_t v) { return std::to_string(v); }

}  // namespace

bool g_local_regs = false;
bool g_block_count = true;
bool g_cond_before_poll = false;
bool g_budget = false;
bool g_psq_const = false;

// psq_l / psq_st (psq D8, 30/09) : valeurs connues de chaque GQR (releve PC + decompile : main.cpp
// programme GQR2..5, la video CRI (uty_ppc.c) un second jeu, trans.cpp fait varier GQR6). Le code
// genere compare ctx.gqr[I] a ces valeurs et developpe la conversion de la valeur trouvee
// (nj_ops.h, psq_l_k / psq_s_k) ; sinon, ou pour GQR6, l'appel generique d'avant. Exact par
// construction, meme si un GQR change.
static std::string psq_call(bool st, int D, const std::string& ea, int W, int Iq) {
    static const char* kKnown[8] = {"0x00000000u"_g, "0x00000000u"_g, "0x00040004u"_g, "0x00050005u, 0x00070004u"_g,
                                    "0x00060006u, 0x00040004u"_g, "0x00070007u, 0x3F073F04u"_g, nullptr,
                                    "0x00000000u, 0x00070007u"_g};
    if (g_psq_const && kKnown[Iq])
        return std::string("    nj::"_g) + (st ? "psq_s_k<"_g : "psq_l_k<"_g) + kKnown[Iq] + ">(ctx, "_g + num(Iq) + ", "_g +
               num(D) + ", "_g + ea + ", "_g + num(W) + ");\n"_g;
    return std::string("    ctx."_g) + (st ? "psq_store("_g : "psq_load("_g) + num(D) + ", "_g + ea + ", "_g + num(W) + ", "_g +
           num(Iq) + ");\n"_g;
}
uint32_t g_used_regs = 0;

namespace {

std::string R(int n) {
    if (g_local_regs) {
        g_used_regs |= 1u << n;
        return "r"_g + num(n);
    }
    return "ctx.gpr["_g + num(n) + "]"_g;
}
std::string F(int n) { return "ctx.fpr["_g + num(n) + "]"_g; }
std::string P1(int n) { return "ctx.ps1["_g + num(n) + "]"_g; }

// Champs d'instruction (bit 0 = poids fort dans le manuel ; ici decalages depuis le poids faible).
struct I {
    uint32_t op;
    int prim() const { return int(op >> 26); }
    int rD() const { return int((op >> 21) & 31); }
    int rS() const { return rD(); }
    int rA() const { return int((op >> 16) & 31); }
    int rB() const { return int((op >> 11) & 31); }
    int rC() const { return int((op >> 6) & 31); }
    int16_t simm() const { return int16_t(op & 0xFFFF); }
    uint32_t uimm() const { return op & 0xFFFF; }
    bool rc() const { return op & 1; }
    bool oe() const { return (op >> 10) & 1; }
    int xo10() const { return int((op >> 1) & 0x3FF); }
    int xo9() const { return int((op >> 1) & 0x1FF); }
    int xo5() const { return int((op >> 1) & 0x1F); }
    int crfD() const { return int((op >> 23) & 7); }
    int crfS() const { return int((op >> 18) & 7); }
    int mb() const { return int((op >> 6) & 31); }
    int me() const { return int((op >> 1) & 31); }
    int sh() const { return rB(); }
};

uint32_t mask32(int mb, int me) {
    const uint32_t a = 0xFFFFFFFFu >> mb;
    const uint32_t b = 0xFFFFFFFFu << (31 - me);
    return mb <= me ? (a & b) : (a | b);
}

// Adresse effective d'un acces a deplacement : (rA|0) + d
std::string ea_d(int rA, int32_t d) {
    if (rA == 0) return "(uint32_t)"_g + num(d);
    return "("_g + R(rA) + " + "_g + num(d) + ")"_g;
}
std::string ea_x(int rA, int rB) {
    if (rA == 0) return R(rB);
    return "("_g + R(rA) + " + "_g + R(rB) + ")"_g;
}

std::string crbit_expr(int n) {
    static const char* k[] = {"lt"_g, "gt"_g, "eq"_g, "so"_g};
    return "ctx.cr["_g + num(n >> 2) + "]."_g + k[n & 3];
}

void rec0(std::string& o, bool rc, int reg) {
    if (rc) o += "    nj::cr0(ctx, "_g + R(reg) + ");\n"_g;
}

// Retour de boucle : sans compteur par bloc (cfg.block_count), on compte ici la longueur de la
// boucle (t..addr), pour garder une verification des interruptions toutes les ~512 instructions.
// reeval : instruction rejouee apres drain_callbacks (condition du branchement
// relue dans ctx, voir le cas 16).
void poll(std::string& o, uint32_t addr, uint32_t t, const std::string& reeval = ""_g) {
    // Budget 32 bits (cfg.budget) : sub + bmi par retour de boucle ; ctx.pc, retired et
    // poll_now passent dans nj_budget_slow (ios.cpp). Meme echeances, meme heure.
    if (g_budget && !g_block_count) {
        o += "    if ((ctx.budget -= "_g + num((int64_t)((addr - t) / 4 + 1)) + ") < 0) {\n"_g;
        o += "        NJ_WB; if (nj::runtime::nj_budget_slow(ctx, "_g + hx(addr) + ")) return;\n        NJ_RL;"_g +
             reeval + "\n    }\n"_g;
        return;
    }
    if (!g_block_count)
        o += "    ctx.retired += "_g + num((int64_t)((addr - t) / 4 + 1)) + ";\n"_g;
    o += "    ctx.pc = "_g + hx(addr) + ";\n"_g;
    o += "    if (ctx.retired >= ctx.check_deadline || "
         "nj::runtime::hw::poll_now.load(std::memory_order_relaxed)) {\n"_g;
    o += "        NJ_WB; if (drain_callbacks(ctx)) return;\n        NJ_RL;"_g + reeval + "\n    }\n"_g;
}

// Transfert vers `target` hors de la fonction ; `ret` = adresse de retour (0 pour un saut).
void leave_to(const EmitEnv& env, std::string& o, uint32_t target, uint32_t ret, const char* ind) {
    const std::string nat = env.native_name ? env.native_name(target) : ""_g;
    const std::string name = env.entry_name ? env.entry_name(target) : ""_g;
    std::string s = ind;
    if (ret) s += "ctx.lr = "_g + hx(ret) + "; "_g;
    s += "ctx.pc = "_g + hx(target) + ";"_g;
    if (!nat.empty())
        s += " NJ_WB; "_g + nat + "(ctx); ctx.pc = ctx.lr; NJ_RL;"_g;
    else if (!name.empty())
        s += " NJ_WB; "_g + name + "(ctx); NJ_RL;"_g;
    else {
        // pas une entree de fonction : le repartiteur s'en charge
        o += s + " NJ_WB; return;\n"_g;
        return;
    }
    if (ret)
        s += " if (ctx.pc != "_g + hx(ret) + ") return;"_g;
    else
        s += " return;"_g;
    o += s + "\n"_g;
}

// Condition d'un bc (BO, BI) ; decremente CTR si demande.
std::string bc_cond(std::string& o, int BO, int BI) {
    std::string c;
    if (!(BO & 4)) {
        o += "    --ctx.ctr;\n"_g;
        c = (BO & 2) ? "ctx.ctr == 0"_g : "ctx.ctr != 0"_g;
    }
    if (!(BO & 16)) {
        const std::string b = crbit_expr(BI);
        const std::string t = (BO & 8) ? b : "!"_g + b;
        c = c.empty() ? t : "("_g + c + ") && "_g + t;
    }
    return c;   // vide = toujours
}

// Registres speciaux : comportement de l'ancien moteur garde en v1 (voir en-tete).
bool emit_mfspr(std::string& o, int d, int spr) {
    const std::string D = R(d);
    switch (spr) {
    case 1: o += "    "_g + D + " = ctx.xer;\n"_g; return true;
    case 8: o += "    "_g + D + " = ctx.lr;\n"_g; return true;
    case 9: o += "    "_g + D + " = ctx.ctr;\n"_g; return true;
    case 22: o += "    "_g + D + " = ctx.get_decrementer();\n"_g; return true;
    case 26: o += "    "_g + D + " = ctx.srr0;\n"_g; return true;
    case 27: o += "    "_g + D + " = ctx.srr1;\n"_g; return true;
    case 268: o += "    "_g + D + " = (uint32_t)(ctx.timebase_now() & 0xFFFFFFFF);\n"_g; return true;
    case 269: o += "    "_g + D + " = (uint32_t)(ctx.timebase_now() >> 32);\n"_g; return true;
    case 287: o += "    "_g + D + " = 0x00083214; // PVR Gekko\n"_g; return true;
    default:
        if (spr >= 912 && spr <= 919) {
            o += "    "_g + D + " = ctx.gqr["_g + num(spr - 912) + "];\n"_g;
            return true;
        }
        o += "    "_g + D + " = 0; // spr "_g + num(spr) + " : 0 (comme l'ancien moteur)\n"_g;
        return true;
    }
}
void emit_mtspr(std::string& o, int s, int spr) {
    const std::string S = R(s);
    switch (spr) {
    case 1: o += "    ctx.xer = "_g + S + ";\n"_g; return;
    case 8: o += "    ctx.lr = "_g + S + ";\n"_g; return;
    case 9: o += "    ctx.ctr = "_g + S + ";\n"_g; return;
    case 22: o += "    ctx.set_decrementer("_g + S + ");\n"_g; return;
    case 26: o += "    ctx.srr0 = "_g + S + ";\n"_g; return;
    case 27: o += "    ctx.srr1 = "_g + S + ";\n"_g; return;
    default:
        if (spr >= 912 && spr <= 919) {
            o += "    ctx.gqr["_g + num(spr - 912) + "] = "_g + S + ";\n"_g;
            return;
        }
        o += "    // mtspr "_g + num(spr) + " : ignore (comme l'ancien moteur)\n"_g;
    }
}


}  // namespace

bool is_call(uint32_t op) {
    const int p = int(op >> 26);
    if (p == 18 || p == 16) return op & 1;
    if (p == 19) {
        const int x = int((op >> 1) & 0x3FF);
        return (x == 16 || x == 528) && (op & 1);
    }
    return false;
}

bool is_local_branch(uint32_t op) {
    const int p = int(op >> 26);
    return (p == 18 || p == 16) && !(op & 1);
}

uint32_t branch_target(const EmitEnv& env, uint32_t addr, uint32_t op) {
    if (env.branch_override) {
        if (const uint32_t t = env.branch_override(addr)) return t;
    }
    const int p = int(op >> 26);
    const bool aa = (op >> 1) & 1;
    int32_t d;
    if (p == 18) {
        d = int32_t(op & 0x03FFFFFC);
        if (d & 0x02000000) d -= 0x04000000;
    } else {
        d = int16_t(op & 0xFFFC);
    }
    return aa ? uint32_t(d) : addr + uint32_t(d);
}

EmitResult emit_instr(const EmitEnv& env, uint32_t addr, uint32_t op, std::string& o) {
    EmitResult r;
    const I i{op};
    const uint32_t next = addr + 4;
    const int D = i.rD(), A = i.rA(), B = i.rB(), C = i.rC();
    auto local = [&](uint32_t t) { return t >= env.fn_start && t < env.fn_end; };

    auto unknown = [&]() {
        o += "    NJ_WB; nj::runtime::interp_single(ctx, "_g + hx(op) + ", "_g + hx(addr) + "); NJ_RL;\n"_g;
        o += "    if (ctx.pc != "_g + hx(next) + ") return;\n"_g;
        r.unknown = true;
        r.ends_block = true;
        return r;
    };

    switch (i.prim()) {
    // ------------------------------------------------------------ entiers, immediats
    case 7:   // mulli
        o += "    "_g + R(D) + " = (uint32_t)((int32_t)"_g + R(A) + " * "_g + num(i.simm()) + ");\n"_g;
        return r;
    case 8:   // subfic
        o += "    "_g + R(D) + " = nj::add_ca(ctx, ~"_g + R(A) + ", (uint32_t)"_g + num(i.simm()) + ", 1);\n"_g;
        return r;
    case 10:  // cmpli
        o += "    nj::cr_unsigned(ctx, "_g + num(i.crfD()) + ", "_g + R(A) + ", "_g + num(i.uimm()) + "u);\n"_g;
        return r;
    case 11:  // cmpi
        o += "    nj::cr_signed(ctx, "_g + num(i.crfD()) + ", (int32_t)"_g + R(A) + ", "_g + num(i.simm()) + ");\n"_g;
        return r;
    case 12:  // addic
    case 13:  // addic.
        o += "    "_g + R(D) + " = nj::add_ca(ctx, "_g + R(A) + ", (uint32_t)"_g + num(i.simm()) + ", 0);\n"_g;
        rec0(o, i.prim() == 13, D);
        return r;
    case 14:  // addi / li
        if (A == 0)
            o += "    "_g + R(D) + " = (uint32_t)"_g + num(i.simm()) + ";\n"_g;
        else
            o += "    "_g + R(D) + " = "_g + R(A) + " + (uint32_t)"_g + num(i.simm()) + ";\n"_g;
        return r;
    case 15: {  // addis / lis
        const uint32_t hi = uint32_t(i.uimm()) << 16;
        if (A == 0)
            o += "    "_g + R(D) + " = "_g + hx(hi) + ";\n"_g;
        else
            o += "    "_g + R(D) + " = "_g + R(A) + " + "_g + hx(hi) + "u;\n"_g;
        uint32_t mod;
        bool bss;
        if (env.fixup && env.fixup(addr, mod, bss))
            o += "    "_g + R(D) + " += (uint32_t)nj::runtime::g_rel_delta["_g + num(mod) + "]["_g +
                 (bss ? "1"_g : "0"_g) + "];\n"_g;
        return r;
    }
    case 24: o += "    "_g + R(A) + " = "_g + R(D) + " | "_g + hx(i.uimm()) + "u;\n"_g; return r;   // ori
    case 25: {  // oris
        o += "    "_g + R(A) + " = "_g + R(D) + " | "_g + hx(i.uimm() << 16) + "u;\n"_g;
        uint32_t mod;
        bool bss;
        if (env.fixup && env.fixup(addr, mod, bss))
            o += "    "_g + R(A) + " += (uint32_t)nj::runtime::g_rel_delta["_g + num(mod) + "]["_g +
                 (bss ? "1"_g : "0"_g) + "];\n"_g;
        return r;
    }
    case 26: o += "    "_g + R(A) + " = "_g + R(D) + " ^ "_g + hx(i.uimm()) + "u;\n"_g; return r;   // xori
    case 27: o += "    "_g + R(A) + " = "_g + R(D) + " ^ "_g + hx(i.uimm() << 16) + "u;\n"_g; return r;  // xoris
    case 28:  // andi.
        o += "    "_g + R(A) + " = "_g + R(D) + " & "_g + hx(i.uimm()) + "u;\n"_g;
        rec0(o, true, A);
        return r;
    case 29:  // andis.
        o += "    "_g + R(A) + " = "_g + R(D) + " & "_g + hx(i.uimm() << 16) + "u;\n"_g;
        rec0(o, true, A);
        return r;
    case 20: {  // rlwimi
        const uint32_t m = mask32(i.mb(), i.me());
        o += "    "_g + R(A) + " = (nj::rotl("_g + R(D) + ", "_g + num(i.sh()) + ") & "_g + hx(m) + "u) | ("_g +
             R(A) + " & "_g + hx(~m) + "u);\n"_g;
        rec0(o, i.rc(), A);
        return r;
    }
    case 21: {  // rlwinm
        const uint32_t m = mask32(i.mb(), i.me());
        o += "    "_g + R(A) + " = nj::rotl("_g + R(D) + ", "_g + num(i.sh()) + ") & "_g + hx(m) + "u;\n"_g;
        rec0(o, i.rc(), A);
        return r;
    }
    case 23: {  // rlwnm
        const uint32_t m = mask32(i.mb(), i.me());
        o += "    "_g + R(A) + " = nj::rotl("_g + R(D) + ", "_g + R(B) + " & 31) & "_g + hx(m) + "u;\n"_g;
        rec0(o, i.rc(), A);
        return r;
    }

    // ------------------------------------------------------------ branchements
    case 18: {  // b, bl, ba, bla
        const uint32_t t = branch_target(env, addr, op);
        r.ends_block = true;
        if (op & 1) {  // appel
            leave_to(env, o, t, next, "    "_g);
            return r;
        }
        r.falls_through = false;
        if (local(t)) {
            if (t <= addr) poll(o, addr, t);
            o += "    goto L_"_g + hx(t).substr(2) + ";\n"_g;
        } else {
            leave_to(env, o, t, 0, "    "_g);
        }
        return r;
    }
    case 16: {  // bc, bcl
        const uint32_t t = branch_target(env, addr, op);
        r.ends_block = true;
        const int BO = D, BI = A;
        // Retour de boucle sur un bit de CR seul (sans ctr) : condition evaluee AVANT la
        // verification periodique, dans une variable, et relue dans ctx seulement si
        // drain_callbacks a tourne. Sinon l'appel (rare) de la verification, place entre
        // la comparaison et le branchement, forcait a relire ctx.cr (ldrb) a chaque tour (30/09).
        // Meme resultat : sans appel, ctx.cr n'a pas change entre les deux.
        if (g_cond_before_poll && !(op & 1) && local(t) && t <= addr && (BO & 4) && !(BO & 16)) {
            std::string dummy;
            const std::string c = bc_cond(dummy, BO, BI);
            o += "    { bool nj_t = "_g + c + ";\n"_g;
            poll(o, addr, t, " nj_t = "_g + c + ";"_g);
            o += "    if (nj_t) goto L_"_g + hx(t).substr(2) + "; }\n"_g;
            return r;
        }
        if (!(op & 1) && local(t) && t <= addr) poll(o, addr, t);
        const std::string c = bc_cond(o, BO, BI);
        std::string body;
        if (op & 1) {
            leave_to(env, body, t, next, "        "_g);
        } else if (local(t)) {
            body = "        goto L_"_g + hx(t).substr(2) + ";\n"_g;
        } else {
            leave_to(env, body, t, 0, "        "_g);
        }
        if (c.empty()) {
            o += body;
            if (!(op & 1)) r.falls_through = false;
        } else {
            o += "    if ("_g + c + ") {\n"_g + body + "    }\n"_g;
        }
        return r;
    }
    case 17:  // sc
        o += "    ctx.pc = "_g + hx(next) + ";\n"_g;
        o += "    NJ_WB; if (nj::runtime::service_syscall(ctx)) return;\n    NJ_RL;\n"_g;
        r.ends_block = true;
        return r;
    case 19: {
        switch (i.xo10()) {
        case 16: {  // bclr, bclrl
            r.ends_block = true;
            const std::string c = bc_cond(o, D, A);
            std::string body;
            if (op & 1)
                body = "        { const uint32_t t = ctx.lr; ctx.lr = "_g + hx(next) +
                       "; ctx.pc = t; NJ_WB; return; }\n"_g;
            else
                body = "        ctx.pc = ctx.lr; NJ_WB; return;\n"_g;
            if (c.empty()) {
                o += body;
                if (!(op & 1)) r.falls_through = false;
            } else {
                o += "    if ("_g + c + ") {\n"_g + body + "    }\n"_g;
            }
            return r;
        }
        case 528: {  // bcctr, bcctrl
            r.ends_block = true;
            const std::string c = bc_cond(o, D | 4, A);   // bcctr ne decremente jamais CTR
            std::string body;
            if (op & 1)
                body = "        ctx.lr = "_g + hx(next) + "; if (ctx.ctr) { ctx.pc = ctx.ctr; NJ_WB; return; } "
                       "else nj::runtime::log_null_call("_g + hx(addr) + ", ctx.lr);\n"_g;
            else
                body = "        ctx.pc = ctx.ctr; NJ_WB; return;\n"_g;
            if (c.empty()) {
                o += body;
                if (!(op & 1)) r.falls_through = false;
            } else {
                o += "    if ("_g + c + ") {\n"_g + body + "    }\n"_g;
            }
            return r;
        }
        case 50:  // rfi
            o += "    NJ_WB;\n    {\n        const uint32_t ee_was = ctx.msr & 0x8000;\n"_g;
            o += "        ctx.msr = ctx.srr1;\n        ctx.pc = ctx.srr0;\n"_g;
            o += "        nj::runtime::hle_dispatch_leave(ctx);\n"_g;
            o += "        if (!ee_was && (ctx.msr & 0x8000)) {\n"_g;
            o += "            if (drain_callbacks(ctx)) return;\n        }\n    }\n"_g;
            o += "    return;\n"_g;
            r.ends_block = true;
            r.falls_through = false;
            return r;
        case 150: o += "    // isync\n"_g; return r;
        case 0:   // mcrf
            o += "    ctx.cr["_g + num(i.crfD()) + "] = ctx.cr["_g + num(i.crfS()) + "];\n"_g;
            return r;
        case 257: case 129: case 289: case 225: case 33: case 449: case 417: case 193: {
            const std::string a = crbit_expr(A), b = crbit_expr(B);
            std::string e;
            switch (i.xo10()) {
            case 257: e = a + " && "_g + b; break;            // crand
            case 129: e = a + " && !"_g + b; break;           // crandc
            case 289: e = a + " == "_g + b; break;            // creqv
            case 225: e = "!("_g + a + " && "_g + b + ")"_g; break;  // crnand
            case 33: e = "!("_g + a + " || "_g + b + ")"_g; break;   // crnor
            case 449: e = a + " || "_g + b; break;            // cror
            case 417: e = a + " || !"_g + b; break;           // crorc
            default: e = a + " != "_g + b; break;             // crxor
            }
            o += "    { const bool v = "_g + e + "; nj::set_crbit(ctx, "_g + num(D) + ", v); }\n"_g;
            return r;
        }
        default: return unknown();
        }
    }

    // ------------------------------------------------------------ memoire, deplacement
    case 32: o += "    "_g + R(D) + " = ctx.mmu.read32("_g + ea_d(A, i.simm()) + ");\n"_g; return r;   // lwz
    case 34: o += "    "_g + R(D) + " = ctx.mmu.read8("_g + ea_d(A, i.simm()) + ");\n"_g; return r;    // lbz
    case 40: o += "    "_g + R(D) + " = ctx.mmu.read16("_g + ea_d(A, i.simm()) + ");\n"_g; return r;   // lhz
    case 42: o += "    "_g + R(D) + " = (uint32_t)(int32_t)(int16_t)ctx.mmu.read16("_g + ea_d(A, i.simm()) + ");\n"_g; return r;  // lha
    case 33: case 35: case 41: case 43: {  // lwzu lbzu lhzu lhau
        const char* rd = i.prim() == 33 ? "ctx.mmu.read32(ea)"_g : i.prim() == 35 ? "ctx.mmu.read8(ea)"_g
                       : i.prim() == 41 ? "ctx.mmu.read16(ea)"_g : "(uint32_t)(int32_t)(int16_t)ctx.mmu.read16(ea)"_g;
        o += "    { const uint32_t ea = "_g + R(A) + " + "_g + num(i.simm()) + "; "_g + R(D) + " = "_g + rd +
             "; "_g + R(A) + " = ea; }\n"_g;
        return r;
    }
    case 36: o += "    ctx.mmu.write32("_g + ea_d(A, i.simm()) + ", "_g + R(D) + ");\n"_g; return r;   // stw
    case 38: o += "    ctx.mmu.write8("_g + ea_d(A, i.simm()) + ", (uint8_t)"_g + R(D) + ");\n"_g; return r;  // stb
    case 44: o += "    ctx.mmu.write16("_g + ea_d(A, i.simm()) + ", (uint16_t)"_g + R(D) + ");\n"_g; return r;  // sth
    case 37: case 39: case 45: {  // stwu stbu sthu
        const char* wr = i.prim() == 37 ? "ctx.mmu.write32(ea, "_g : i.prim() == 39 ? "ctx.mmu.write8(ea, (uint8_t)"_g
                       : "ctx.mmu.write16(ea, (uint16_t)"_g;
        o += "    { const uint32_t ea = "_g + R(A) + " + "_g + num(i.simm()) + "; "_g + wr + R(D) + "); "_g + R(A) +
             " = ea; }\n"_g;
        return r;
    }
    case 46: {  // lmw (deroule : registres D..31, adresse prise avant les ecritures)
        // Un seul controle de bornes pour toute la plage (fast_ptr), puis des lectures directes ;
        // sinon le chemin complet mot par mot. Avant : un controle par registre (18 pour r14..r31).
        const std::string n = num(4 * (32 - D));
        o += "    { const uint32_t ea = "_g + ea_d(A, i.simm()) + ";\n"_g;
        o += "      if (const uint8_t* p = ctx.mmu.fast_ptr(ea, "_g + n + ")) {"_g;
        for (int k = D; k < 32; ++k)
            o += " "_g + R(k) + " = nj::runtime::MMU::be32(p + "_g + num(4 * (k - D)) + ");"_g;
        o += " } else {"_g;
        for (int k = D; k < 32; ++k)
            o += " "_g + R(k) + " = ctx.mmu.read32(ea + "_g + num(4 * (k - D)) + ");"_g;
        o += " } }\n"_g;
        return r;
    }
    case 47: {  // stmw (un seul controle de bornes, voir lmw ; adresse espionnee : chemin complet)
        const std::string n = num(4 * (32 - D));
        o += "    { const uint32_t ea = "_g + ea_d(A, i.simm()) + ";\n"_g;
        o += "      if (uint8_t* p = NJ_WATCHING() ? nullptr : ctx.mmu.fast_ptr(ea, "_g + n + ")) {"_g;
        for (int k = D; k < 32; ++k)
            o += " nj::runtime::MMU::put32(p + "_g + num(4 * (k - D)) + ", "_g + R(k) + ");"_g;
        o += " } else {"_g;
        for (int k = D; k < 32; ++k)
            o += " ctx.mmu.write32(ea + "_g + num(4 * (k - D)) + ", "_g + R(k) + ");"_g;
        o += " } }\n"_g;
        return r;
    }
    case 48:  // lfs
        o += "    "_g + F(D) + " = "_g + P1(D) + " = ctx.mmu.read_f32("_g + ea_d(A, i.simm()) + ");\n"_g;
        return r;
    case 49:  // lfsu
        o += "    { const uint32_t ea = "_g + R(A) + " + "_g + num(i.simm()) + "; "_g + F(D) + " = "_g + P1(D) +
             " = ctx.mmu.read_f32(ea); "_g + R(A) + " = ea; }\n"_g;
        return r;
    case 50: o += "    "_g + F(D) + " = ctx.mmu.read_f64("_g + ea_d(A, i.simm()) + ");\n"_g; return r;  // lfd
    case 51:  // lfdu
        o += "    { const uint32_t ea = "_g + R(A) + " + "_g + num(i.simm()) + "; "_g + F(D) +
             " = ctx.mmu.read_f64(ea); "_g + R(A) + " = ea; }\n"_g;
        return r;
    case 52: o += "    ctx.mmu.write_f32("_g + ea_d(A, i.simm()) + ", (float)"_g + F(D) + ");\n"_g; return r;  // stfs
    case 53:  // stfsu
        o += "    { const uint32_t ea = "_g + R(A) + " + "_g + num(i.simm()) + "; ctx.mmu.write_f32(ea, (float)"_g +
             F(D) + "); "_g + R(A) + " = ea; }\n"_g;
        return r;
    case 54: o += "    ctx.mmu.write_f64("_g + ea_d(A, i.simm()) + ", "_g + F(D) + ");\n"_g; return r;  // stfd
    case 55:  // stfdu
        o += "    { const uint32_t ea = "_g + R(A) + " + "_g + num(i.simm()) + "; ctx.mmu.write_f64(ea, "_g + F(D) +
             "); "_g + R(A) + " = ea; }\n"_g;
        return r;
    case 56: case 57: case 60: case 61: {  // psq_l psq_lu psq_st psq_stu
        const int W = (op >> 15) & 1, Iq = (op >> 12) & 7;
        int32_t d = int32_t(op & 0xFFF);
        if (d & 0x800) d -= 0x1000;
        const bool upd = i.prim() == 57 || i.prim() == 61;
        const bool st = i.prim() >= 60;
        const std::string ea = upd ? "("_g + R(A) + " + "_g + num(d) + ")"_g : ea_d(A, d);
        o += psq_call(st, D, ea, W, Iq);
        if (upd) o += "    "_g + R(A) + " = "_g + R(A) + " + "_g + num(d) + ";\n"_g;
        return r;
    }

    // ------------------------------------------------------------ paired singles (4)
    case 4: {
        const int x10 = i.xo10(), x6 = int((op >> 1) & 0x3F), x5 = i.xo5();
        switch (x10) {
        case 0: o += "    ctx.ps_cmpu0("_g + num(i.crfD()) + ", "_g + num(A) + ", "_g + num(B) + ");\n"_g; return r;
        case 32: o += "    ctx.ps_cmpo0("_g + num(i.crfD()) + ", "_g + num(A) + ", "_g + num(B) + ");\n"_g; return r;
        case 64: o += "    ctx.ps_cmpu1("_g + num(i.crfD()) + ", "_g + num(A) + ", "_g + num(B) + ");\n"_g; return r;
        case 96: o += "    ctx.ps_cmpo1("_g + num(i.crfD()) + ", "_g + num(A) + ", "_g + num(B) + ");\n"_g; return r;
        case 40: o += "    ctx.ps_neg("_g + num(D) + ", "_g + num(B) + ");\n"_g; return r;
        case 72: o += "    ctx.ps_mr("_g + num(D) + ", "_g + num(B) + ");\n"_g; return r;
        case 136: o += "    ctx.ps_nabs("_g + num(D) + ", "_g + num(B) + ");\n"_g; return r;
        case 264: o += "    ctx.ps_abs("_g + num(D) + ", "_g + num(B) + ");\n"_g; return r;
        case 528: o += "    ctx.ps_merge00("_g + num(D) + ", "_g + num(A) + ", "_g + num(B) + ");\n"_g; return r;
        case 560: o += "    ctx.ps_merge01("_g + num(D) + ", "_g + num(A) + ", "_g + num(B) + ");\n"_g; return r;
        case 592: o += "    ctx.ps_merge10("_g + num(D) + ", "_g + num(A) + ", "_g + num(B) + ");\n"_g; return r;
        case 624: o += "    ctx.ps_merge11("_g + num(D) + ", "_g + num(A) + ", "_g + num(B) + ");\n"_g; return r;
        case 1014: o += "    // dcbz_l : sans effet (comme l'ancien moteur)\n"_g; return r;
        default: break;
        }
        if (x6 == 6 || x6 == 7 || x6 == 38 || x6 == 39) {  // psq_lx psq_stx psq_lux psq_stux
            const int W = (op >> 10) & 1, Iq = (op >> 7) & 7;
            const bool upd = x6 >= 38, st = (x6 & 1);
            const std::string ea = upd ? "("_g + R(A) + " + "_g + R(B) + ")"_g : ea_x(A, B);
            o += psq_call(st, D, ea, W, Iq);
            if (upd) o += "    "_g + R(A) + " = "_g + R(A) + " + "_g + R(B) + ";\n"_g;
            return r;
        }
        const std::string d = num(D), a = num(A), b = num(B), c = num(C);
        switch (x5) {
        case 10: o += "    ctx.ps_sum0("_g + d + ", "_g + a + ", "_g + c + ", "_g + b + ");\n"_g; return r;
        case 11: o += "    ctx.ps_sum1("_g + d + ", "_g + a + ", "_g + c + ", "_g + b + ");\n"_g; return r;
        case 12: o += "    ctx.ps_muls0("_g + d + ", "_g + a + ", "_g + c + ");\n"_g; return r;
        case 13: o += "    ctx.ps_muls1("_g + d + ", "_g + a + ", "_g + c + ");\n"_g; return r;
        case 14: o += "    ctx.ps_madds0("_g + d + ", "_g + a + ", "_g + c + ", "_g + b + ");\n"_g; return r;
        case 15: o += "    ctx.ps_madds1("_g + d + ", "_g + a + ", "_g + c + ", "_g + b + ");\n"_g; return r;
        case 18: o += "    ctx.ps_div("_g + d + ", "_g + a + ", "_g + b + ");\n"_g; return r;
        case 20: o += "    ctx.ps_sub("_g + d + ", "_g + a + ", "_g + b + ");\n"_g; return r;
        case 21: o += "    ctx.ps_add("_g + d + ", "_g + a + ", "_g + b + ");\n"_g; return r;
        case 23: o += "    ctx.ps_sel("_g + d + ", "_g + a + ", "_g + c + ", "_g + b + ");\n"_g; return r;
        case 24:  // ps_res
            o += "    "_g + F(D) + " = (double)(float)(1.0 / "_g + F(B) + "); "_g + P1(D) + " = (double)(float)(1.0 / "_g +
                 P1(B) + ");\n"_g;
            return r;
        case 25: o += "    ctx.ps_mul("_g + d + ", "_g + a + ", "_g + c + ");\n"_g; return r;
        case 26:  // ps_rsqrte
            o += "    "_g + F(D) + " = (double)(float)(1.0 / std::sqrt("_g + F(B) + ")); "_g + P1(D) +
                 " = (double)(float)(1.0 / std::sqrt("_g + P1(B) + "));\n"_g;
            return r;
        case 28: o += "    ctx.ps_msub("_g + d + ", "_g + a + ", "_g + c + ", "_g + b + ");\n"_g; return r;
        case 29: o += "    ctx.ps_madd("_g + d + ", "_g + a + ", "_g + c + ", "_g + b + ");\n"_g; return r;
        case 30: o += "    ctx.ps_nmsub("_g + d + ", "_g + a + ", "_g + c + ", "_g + b + ");\n"_g; return r;
        case 31: o += "    ctx.ps_nmadd("_g + d + ", "_g + a + ", "_g + c + ", "_g + b + ");\n"_g; return r;
        default: return unknown();
        }
    }

    // ------------------------------------------------------------ flottants simples (59)
    case 59: {
        const std::string d = F(D), a = F(A), b = F(B), c = F(C);
        std::string e;
        switch (i.xo5()) {
        case 18: e = "(float)("_g + a + " / "_g + b + ")"_g; break;                           // fdivs
        case 20: e = "(float)("_g + a + " - "_g + b + ")"_g; break;                           // fsubs
        case 21: e = "(float)("_g + a + " + "_g + b + ")"_g; break;                           // fadds
        case 22: e = "(float)std::sqrt("_g + b + ")"_g; break;                              // fsqrts
        case 24: e = "(float)(1.0f / "_g + b + ")"_g; break;                                // fres
        case 25: e = "(float)("_g + a + " * "_g + c + ")"_g; break;                           // fmuls
        case 28: e = "(float)std::fma("_g + a + ", "_g + c + ", -"_g + b + ")"_g; break;         // fmsubs
        case 29: e = "(float)std::fma("_g + a + ", "_g + c + ", "_g + b + ")"_g; break;          // fmadds
        case 30: e = "(float)(-std::fma("_g + a + ", "_g + c + ", -"_g + b + "))"_g; break;      // fnmsubs
        case 31: e = "(float)(-std::fma("_g + a + ", "_g + c + ", "_g + b + "))"_g; break;       // fnmadds
        default: return unknown();
        }
        o += "    "_g + d + " = "_g + e + ";\n    "_g + P1(D) + " = "_g + d + ";\n"_g;
        return r;
    }

    // ------------------------------------------------------------ flottants doubles (63)
    case 63: {
        const std::string d = F(D), a = F(A), b = F(B), c = F(C);
        switch (i.xo5()) {
        case 18: o += "    "_g + d + " = "_g + a + " / "_g + b + ";\n"_g; return r;                  // fdiv
        case 20: o += "    "_g + d + " = "_g + a + " - "_g + b + ";\n"_g; return r;                  // fsub
        case 21: o += "    "_g + d + " = "_g + a + " + "_g + b + ";\n"_g; return r;                  // fadd
        case 22: o += "    "_g + d + " = std::sqrt("_g + b + ");\n"_g; return r;                   // fsqrt
        case 23: o += "    "_g + d + " = ("_g + a + " >= 0.0) ? "_g + c + " : "_g + b + ";\n"_g; return r;  // fsel
        case 25: o += "    "_g + d + " = "_g + a + " * "_g + c + ";\n"_g; return r;                  // fmul
        case 26: o += "    "_g + d + " = 1.0 / std::sqrt("_g + b + ");\n"_g; return r;             // frsqrte
        // multiplication-addition en double : produit puis somme arrondis separement, comme l'ancien
        // moteur (le Gekko fusionne ; ecart d'un bit de poids faible, garde en v1)
        case 28: o += "    "_g + d + " = "_g + a + " * "_g + c + " - "_g + b + ";\n"_g; return r;        // fmsub
        case 29: o += "    "_g + d + " = "_g + a + " * "_g + c + " + "_g + b + ";\n"_g; return r;        // fmadd
        case 30: o += "    "_g + d + " = -("_g + a + " * "_g + c + " - "_g + b + ");\n"_g; return r;     // fnmsub
        case 31: o += "    "_g + d + " = -("_g + a + " * "_g + c + " + "_g + b + ");\n"_g; return r;     // fnmadd
        default: break;
        }
        switch (i.xo10()) {
        case 0:   // fcmpu
        case 32:  // fcmpo
            o += "    nj::fcmp(ctx, "_g + num(i.crfD()) + ", "_g + a + ", "_g + b + ");\n"_g;
            return r;
        case 12:  // frsp
            o += "    "_g + d + " = (double)(float)"_g + b + "; "_g + P1(D) + " = "_g + d + ";\n"_g;
            return r;
        case 14:  // fctiw
        case 15: {  // fctiwz
            const std::string v = i.xo10() == 14 ? "std::nearbyint("_g + b + ")"_g : b;
            o += "    { const double v = "_g + v + ";\n"_g;
            o += "      const int32_t k = !(v > -2147483649.0) ? (-2147483647 - 1) : (v >= 2147483648.0 ? "
                 "2147483647 : (int32_t)v);\n"_g;
            o += "      "_g + d + " = nj::as_double(0xFFF8000000000000ull | (uint32_t)k); }\n"_g;
            return r;
        }
        case 40: o += "    "_g + d + " = -"_g + b + ";\n"_g; return r;                         // fneg
        case 72: o += "    "_g + d + " = "_g + b + ";\n"_g; return r;                          // fmr
        case 136: o += "    "_g + d + " = -std::fabs("_g + b + ");\n"_g; return r;             // fnabs
        case 264: o += "    "_g + d + " = std::fabs("_g + b + ");\n"_g; return r;              // fabs
        // FPSCR : ignore, comme l'ancien moteur (nos flottants ne suivent ni mode d'arrondi ni drapeaux)
        case 583: o += "    "_g + d + " = 0.0; // mffs\n"_g; return r;
        case 711: case 38: case 70: case 134: o += "    // ecriture du FPSCR ignoree\n"_g; return r;
        default: return unknown();
        }
    }

    // ------------------------------------------------------------ groupe 31
    case 31: {
        const int x = i.xo10();
        const int x9 = i.xo9();
        // arithmetique a debordement possible (OE) : XO sur 9 bits
        switch (x9) {
        case 266: o += "    "_g + R(D) + " = "_g + R(A) + " + "_g + R(B) + ";\n"_g; break;                       // add
        case 40: o += "    "_g + R(D) + " = "_g + R(B) + " - "_g + R(A) + ";\n"_g; break;                        // subf
        case 104: o += "    "_g + R(D) + " = (uint32_t)(-(int32_t)"_g + R(A) + ");\n"_g; break;               // neg
        case 235: o += "    "_g + R(D) + " = (uint32_t)((int32_t)"_g + R(A) + " * (int32_t)"_g + R(B) + ");\n"_g; break;  // mullw
        case 75: o += "    "_g + R(D) + " = (uint32_t)(((int64_t)(int32_t)"_g + R(A) + " * (int64_t)(int32_t)"_g + R(B) +
                      ") >> 32);\n"_g; break;                                                               // mulhw
        case 11: o += "    "_g + R(D) + " = (uint32_t)(((uint64_t)"_g + R(A) + " * (uint64_t)"_g + R(B) + ") >> 32);\n"_g;
            break;                                                                                        // mulhwu
        case 491:  // divw : par zero (et 0x80000000 / -1), rD inchange comme l'ancien moteur
            o += "    { const int32_t a = (int32_t)"_g + R(A) + ", b = (int32_t)"_g + R(B) +
                 "; if (b != 0 && !(a == (int32_t)0x80000000 && b == -1)) "_g + R(D) + " = (uint32_t)(a / b); }\n"_g;
            break;
        case 459:  // divwu : par zero, rD inchange comme l'ancien moteur
            o += "    if ("_g + R(B) + " != 0) "_g + R(D) + " = "_g + R(A) + " / "_g + R(B) + ";\n"_g;
            break;
        case 10: o += "    "_g + R(D) + " = nj::add_ca(ctx, "_g + R(A) + ", "_g + R(B) + ", 0);\n"_g; break;         // addc
        case 138: o += "    "_g + R(D) + " = nj::add_ca(ctx, "_g + R(A) + ", "_g + R(B) + ", nj::get_ca(ctx));\n"_g; break;  // adde
        case 202: o += "    "_g + R(D) + " = nj::add_ca(ctx, "_g + R(A) + ", 0u, nj::get_ca(ctx));\n"_g; break;     // addze
        case 234: o += "    "_g + R(D) + " = nj::add_ca(ctx, "_g + R(A) + ", 0xFFFFFFFFu, nj::get_ca(ctx));\n"_g; break;  // addme
        case 8: o += "    "_g + R(D) + " = nj::add_ca(ctx, ~"_g + R(A) + ", "_g + R(B) + ", 1);\n"_g; break;         // subfc
        case 136: o += "    "_g + R(D) + " = nj::add_ca(ctx, ~"_g + R(A) + ", "_g + R(B) + ", nj::get_ca(ctx));\n"_g; break;  // subfe
        case 200: o += "    "_g + R(D) + " = nj::add_ca(ctx, ~"_g + R(A) + ", 0u, nj::get_ca(ctx));\n"_g; break;    // subfze
        case 232: o += "    "_g + R(D) + " = nj::add_ca(ctx, ~"_g + R(A) + ", 0xFFFFFFFFu, nj::get_ca(ctx));\n"_g; break;  // subfme
        default: goto not_arith;
        }
        if (i.oe()) o += "    // OE : debordement non calcule (absent de RE4)\n"_g;
        rec0(o, i.rc(), D);
        return r;
    not_arith:
        switch (x) {
        case 0: o += "    nj::cr_signed(ctx, "_g + num(i.crfD()) + ", (int32_t)"_g + R(A) + ", (int32_t)"_g + R(B) + ");\n"_g; return r;  // cmp
        case 32: o += "    nj::cr_unsigned(ctx, "_g + num(i.crfD()) + ", "_g + R(A) + ", "_g + R(B) + ");\n"_g; return r;  // cmpl
        case 28: o += "    "_g + R(A) + " = "_g + R(D) + " & "_g + R(B) + ";\n"_g; rec0(o, i.rc(), A); return r;   // and
        case 60: o += "    "_g + R(A) + " = "_g + R(D) + " & ~"_g + R(B) + ";\n"_g; rec0(o, i.rc(), A); return r;  // andc
        case 444: o += "    "_g + R(A) + " = "_g + R(D) + " | "_g + R(B) + ";\n"_g; rec0(o, i.rc(), A); return r;  // or / mr
        case 412: o += "    "_g + R(A) + " = "_g + R(D) + " | ~"_g + R(B) + ";\n"_g; rec0(o, i.rc(), A); return r; // orc
        case 316: o += "    "_g + R(A) + " = "_g + R(D) + " ^ "_g + R(B) + ";\n"_g; rec0(o, i.rc(), A); return r;  // xor
        case 476: o += "    "_g + R(A) + " = ~("_g + R(D) + " & "_g + R(B) + ");\n"_g; rec0(o, i.rc(), A); return r;  // nand
        case 124: o += "    "_g + R(A) + " = ~("_g + R(D) + " | "_g + R(B) + ");\n"_g; rec0(o, i.rc(), A); return r;  // nor
        case 284: o += "    "_g + R(A) + " = ~("_g + R(D) + " ^ "_g + R(B) + ");\n"_g; rec0(o, i.rc(), A); return r;  // eqv
        case 24:  // slw
            o += "    { const uint32_t n = "_g + R(B) + " & 0x3F; "_g + R(A) + " = n > 31 ? 0u : "_g + R(D) + " << n; }\n"_g;
            rec0(o, i.rc(), A);
            return r;
        case 536:  // srw
            o += "    { const uint32_t n = "_g + R(B) + " & 0x3F; "_g + R(A) + " = n > 31 ? 0u : "_g + R(D) + " >> n; }\n"_g;
            rec0(o, i.rc(), A);
            return r;
        case 792:  // sraw
            o += "    { const int32_t s = (int32_t)"_g + R(D) + "; const uint32_t n = "_g + R(B) + " & 0x3F;\n"_g;
            o += "      if (n > 31) { "_g + R(A) + " = s < 0 ? 0xFFFFFFFFu : 0u; nj::set_ca(ctx, s < 0); }\n"_g;
            o += "      else { "_g + R(A) + " = (uint32_t)(s >> n); nj::set_ca(ctx, s < 0 && n && ((uint32_t)s & ((1u << n) - 1))); } }\n"_g;
            rec0(o, i.rc(), A);
            return r;
        case 824: {  // srawi
            const int n = i.sh();
            o += "    { const int32_t s = (int32_t)"_g + R(D) + "; "_g + R(A) + " = (uint32_t)(s >> "_g + num(n) + ");"_g;
            if (n)
                o += " nj::set_ca(ctx, s < 0 && ((uint32_t)s & "_g + hx((1u << n) - 1) + "u)); }\n"_g;
            else
                o += " nj::set_ca(ctx, false); }\n"_g;
            rec0(o, i.rc(), A);
            return r;
        }
        case 26: o += "    "_g + R(A) + " = "_g + R(D) + " ? (uint32_t)__builtin_clz("_g + R(D) + ") : 32u;\n"_g; rec0(o, i.rc(), A); return r;  // cntlzw
        case 922: o += "    "_g + R(A) + " = (uint32_t)(int32_t)(int16_t)"_g + R(D) + ";\n"_g; rec0(o, i.rc(), A); return r;  // extsh
        case 954: o += "    "_g + R(A) + " = (uint32_t)(int32_t)(int8_t)"_g + R(D) + ";\n"_g; rec0(o, i.rc(), A); return r;   // extsb
        // chargements et rangements indexes
        case 23: o += "    "_g + R(D) + " = ctx.mmu.read32("_g + ea_x(A, B) + ");\n"_g; return r;   // lwzx
        case 87: o += "    "_g + R(D) + " = ctx.mmu.read8("_g + ea_x(A, B) + ");\n"_g; return r;    // lbzx
        case 279: o += "    "_g + R(D) + " = ctx.mmu.read16("_g + ea_x(A, B) + ");\n"_g; return r;  // lhzx
        case 343: o += "    "_g + R(D) + " = (uint32_t)(int32_t)(int16_t)ctx.mmu.read16("_g + ea_x(A, B) + ");\n"_g; return r;  // lhax
        case 534: o += "    "_g + R(D) + " = __builtin_bswap32(ctx.mmu.read32("_g + ea_x(A, B) + "));\n"_g; return r;  // lwbrx
        case 790: o += "    "_g + R(D) + " = __builtin_bswap16(ctx.mmu.read16("_g + ea_x(A, B) + "));\n"_g; return r;  // lhbrx
        case 20: o += "    "_g + R(D) + " = ctx.mmu.read32("_g + ea_x(A, B) + "); // lwarx\n"_g; return r;
        case 55: case 119: case 311: case 375: {  // lwzux lbzux lhzux lhaux
            const char* rd = x == 55 ? "ctx.mmu.read32(ea)"_g : x == 119 ? "ctx.mmu.read8(ea)"_g
                           : x == 311 ? "ctx.mmu.read16(ea)"_g : "(uint32_t)(int32_t)(int16_t)ctx.mmu.read16(ea)"_g;
            o += "    { const uint32_t ea = "_g + R(A) + " + "_g + R(B) + "; "_g + R(D) + " = "_g + rd + "; "_g + R(A) + " = ea; }\n"_g;
            return r;
        }
        case 151: o += "    ctx.mmu.write32("_g + ea_x(A, B) + ", "_g + R(D) + ");\n"_g; return r;              // stwx
        case 215: o += "    ctx.mmu.write8("_g + ea_x(A, B) + ", (uint8_t)"_g + R(D) + ");\n"_g; return r;    // stbx
        case 407: o += "    ctx.mmu.write16("_g + ea_x(A, B) + ", (uint16_t)"_g + R(D) + ");\n"_g; return r;  // sthx
        case 662: o += "    ctx.mmu.write32("_g + ea_x(A, B) + ", __builtin_bswap32("_g + R(D) + "));\n"_g; return r;  // stwbrx
        case 918: o += "    ctx.mmu.write16("_g + ea_x(A, B) + ", __builtin_bswap16((uint16_t)"_g + R(D) + "));\n"_g; return r;  // sthbrx
        case 150:  // stwcx.
            o += "    ctx.mmu.write32("_g + ea_x(A, B) + ", "_g + R(D) + ");\n"_g;
            o += "    ctx.cr[0].lt = false; ctx.cr[0].gt = false; ctx.cr[0].eq = true; ctx.cr[0].so = (ctx.xer & nj::XER_SO) != 0;\n"_g;
            return r;
        case 183: case 247: case 439: {  // stwux stbux sthux
            const char* wr = x == 183 ? "ctx.mmu.write32(ea, "_g : x == 247 ? "ctx.mmu.write8(ea, (uint8_t)"_g
                           : "ctx.mmu.write16(ea, (uint16_t)"_g;
            o += "    { const uint32_t ea = "_g + R(A) + " + "_g + R(B) + "; "_g + wr + R(D) + "); "_g + R(A) + " = ea; }\n"_g;
            return r;
        }
        case 535: o += "    "_g + F(D) + " = "_g + P1(D) + " = ctx.mmu.read_f32("_g + ea_x(A, B) + ");\n"_g; return r;  // lfsx
        case 567:  // lfsux
            o += "    { const uint32_t ea = "_g + R(A) + " + "_g + R(B) + "; "_g + F(D) + " = "_g + P1(D) +
                 " = ctx.mmu.read_f32(ea); "_g + R(A) + " = ea; }\n"_g;
            return r;
        case 599: o += "    "_g + F(D) + " = ctx.mmu.read_f64("_g + ea_x(A, B) + ");\n"_g; return r;  // lfdx
        case 631:  // lfdux
            o += "    { const uint32_t ea = "_g + R(A) + " + "_g + R(B) + "; "_g + F(D) + " = ctx.mmu.read_f64(ea); "_g + R(A) +
                 " = ea; }\n"_g;
            return r;
        case 663: o += "    ctx.mmu.write_f32("_g + ea_x(A, B) + ", (float)"_g + F(D) + ");\n"_g; return r;  // stfsx
        case 695:  // stfsux
            o += "    { const uint32_t ea = "_g + R(A) + " + "_g + R(B) + "; ctx.mmu.write_f32(ea, (float)"_g + F(D) + "); "_g +
                 R(A) + " = ea; }\n"_g;
            return r;
        case 727: o += "    ctx.mmu.write_f64("_g + ea_x(A, B) + ", "_g + F(D) + ");\n"_g; return r;  // stfdx
        case 759:  // stfdux
            o += "    { const uint32_t ea = "_g + R(A) + " + "_g + R(B) + "; ctx.mmu.write_f64(ea, "_g + F(D) + "); "_g + R(A) +
                 " = ea; }\n"_g;
            return r;
        case 983:  // stfiwx : les 32 bits de poids faible du registre (l'ancien moteur convertissait
                   // la valeur en entier, comportement indefini : corrige, docs/RECOMPILATEUR.md)
            o += "    ctx.mmu.write32("_g + ea_x(A, B) + ", (uint32_t)nj::as_u64("_g + F(D) + "));\n"_g;
            return r;
        case 597: {  // lswi
            const int nb = B ? B : 32;
            o += "    NJ_WB; { uint32_t ea = "_g + (A ? R(A) : std::string("0u"_g)) + "; int reg = "_g + num(D) + ";\n"_g;
            o += "      for (int k = 0; k < "_g + num(nb) + "; ++k, ++ea) { if ((k & 3) == 0) { if (k) reg = (reg + 1) & 31; "
                 "ctx.gpr[reg] = 0; } ctx.gpr[reg] |= (uint32_t)ctx.mmu.read8(ea) << (24 - 8 * (k & 3)); } } NJ_RL;\n"_g;
            return r;
        }
        case 725: {  // stswi
            const int nb = B ? B : 32;
            o += "    NJ_WB; { uint32_t ea = "_g + (A ? R(A) : std::string("0u"_g)) + "; int reg = "_g + num(D) + ";\n"_g;
            o += "      for (int k = 0; k < "_g + num(nb) + "; ++k, ++ea) { if ((k & 3) == 0 && k) reg = (reg + 1) & 31; "
                 "ctx.mmu.write8(ea, (uint8_t)(ctx.gpr[reg] >> (24 - 8 * (k & 3)))); } }\n"_g;
            return r;
        }
        // registres de condition et speciaux
        case 19: o += "    "_g + R(D) + " = nj::cr_pack(ctx);\n"_g; return r;   // mfcr
        case 144: {  // mtcrf
            const int fxm = int((op >> 12) & 0xFF);
            for (int k = 0; k < 8; ++k)
                if (fxm & (0x80 >> k)) o += "    nj::cr_unpack_field(ctx, "_g + num(k) + ", "_g + R(D) + ");\n"_g;
            return r;
        }
        case 512:  // mcrxr
            o += "    ctx.cr["_g + num(i.crfD()) + "].lt = (ctx.xer >> 31) & 1; ctx.cr["_g + num(i.crfD()) +
                 "].gt = (ctx.xer >> 30) & 1; ctx.cr["_g + num(i.crfD()) + "].eq = (ctx.xer >> 29) & 1; ctx.cr["_g +
                 num(i.crfD()) + "].so = (ctx.xer >> 28) & 1; ctx.xer &= 0x0FFFFFFFu;\n"_g;
            return r;
        case 83: o += "    "_g + R(D) + " = ctx.msr;\n"_g; return r;   // mfmsr
        case 146:  // mtmsr
            o += "    {\n        const uint32_t ee_was = ctx.msr & 0x8000;\n        ctx.msr = "_g + R(D) + ";\n"_g;
            o += "        if (!ee_was && (ctx.msr & 0x8000)) {\n            ctx.pc = "_g + hx(next) + ";\n"_g;
            o += "            NJ_WB; if (drain_callbacks(ctx)) return;\n            NJ_RL;\n        }\n    }\n"_g;
            r.ends_block = true;
            return r;
        case 339: emit_mfspr(o, D, ((op >> 16) & 0x1F) | (((op >> 11) & 0x1F) << 5)); return r;
        case 467: emit_mtspr(o, D, ((op >> 16) & 0x1F) | (((op >> 11) & 0x1F) << 5)); return r;
        case 371: {  // mftb
            const int tbr = int(((op >> 16) & 0x1F) | (((op >> 11) & 0x1F) << 5));
            if (tbr == 269)
                o += "    "_g + R(D) + " = (uint32_t)(ctx.timebase_now() >> 32);\n"_g;
            else
                o += "    "_g + R(D) + " = (uint32_t)(ctx.timebase_now() & 0xFFFFFFFF);\n"_g;
            return r;
        }
        case 595: case 659: o += "    // mfsr : rD inchange (comme l'ancien moteur)\n"_g; return r;
        case 210: case 242: o += "    // mtsr : sans effet\n"_g; return r;
        // antememoires et synchronisation : sans effet (dcbz comme l'ancien moteur, ecart note)
        case 86: case 54: case 278: case 246: case 470: case 982: case 1014: case 598: case 854:
        case 306: case 566:
            o += "    // antememoire / synchronisation\n"_g;
            return r;
        case 4:  // tw
            o += "    // tw : piege ignore\n"_g;
            return r;
        default: return unknown();
        }
    }
    case 3:  // twi
        o += "    // twi : piege ignore\n"_g;
        return r;
    default:
        return unknown();
    }
}

}  // namespace nj
