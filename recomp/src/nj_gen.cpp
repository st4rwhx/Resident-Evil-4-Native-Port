#include "nj_gen.h"
#include "nj_lib.h"

#include "nj_config.h"
#include "nj_emit.h"
#include "nj_image.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include "runtime/obf.h"
#include "runtime/diag.h"
using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

namespace fs = ::std::filesystem;

namespace nj {

namespace {

struct Func {
    uint32_t start = 0, end = 0;
    std::string name;       // identifiant C++ (dans g::)
    std::string native;     // fonction native du runtime qui la remplace ("" sinon)
    std::string wrap;       // fonction native qui l'enveloppe ("" sinon)
};

std::string hx(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%08X"_g, v);
    return b;
}
std::string hx8(uint32_t v) { return hx(v).substr(2); }

std::string sanitize(const std::string& s) {
    std::string o;
    for (char c : s) o += (std::isalnum((unsigned char)c) || c == '_') ? c : '_';
    if (o.empty() || std::isdigit((unsigned char)o[0])) o = "f_"_g + o;
    static const std::unordered_set<std::string> kw = {
        "alignas"_g, "alignof"_g, "and"_g, "asm"_g, "auto"_g, "bool"_g, "break"_g, "case"_g, "catch"_g, "char"_g, "class"_g,
        "const"_g, "continue"_g, "default"_g, "delete"_g, "do"_g, "double"_g, "else"_g, "enum"_g, "explicit"_g, "export"_g,
        "extern"_g, "false"_g, "float"_g, "for"_g, "friend"_g, "goto"_g, "if"_g, "inline"_g, "int"_g, "long"_g, "mutable"_g,
        "namespace"_g, "new"_g, "noexcept"_g, "not"_g, "nullptr"_g, "operator"_g, "or"_g, "private"_g, "protected"_g,
        "public"_g, "register"_g, "return"_g, "short"_g, "signed"_g, "sizeof"_g, "static"_g, "struct"_g, "switch"_g,
        "template"_g, "this"_g, "throw"_g, "true"_g, "try"_g, "typedef"_g, "typename"_g, "union"_g, "unsigned"_g,
        "using"_g, "virtual"_g, "void"_g, "volatile"_g, "while"_g, "xor"_g, "main"_g};
    if (kw.count(o)) o += "_"_g;
    return o;
}

// Une instruction qui transfere ou peut transferer le controle termine un bloc.
bool ends_block(uint32_t op) {
    const int p = int(op >> 26);
    if (p == 16 || p == 18 || p == 17) return true;
    if (p == 19) {
        const int x = int((op >> 1) & 0x3FF);
        return x == 16 || x == 528 || x == 50;
    }
    if (p == 31 && int((op >> 1) & 0x3FF) == 146) return true;   // mtmsr
    return false;
}

bool read_symbols_from(std::istream& f, std::vector<Func>& out) {
    std::string line;
    std::getline(f, line);   // en-tete Name,Location,Size
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t a = line.rfind(','), b = a == std::string::npos ? a : line.rfind(',', a - 1);
        if (a == std::string::npos || b == std::string::npos) continue;
        Func fn;
        fn.name = line.substr(0, b);
        if (fn.name.size() >= 2 && fn.name.front() == '"' && fn.name.back() == '"')
            fn.name = fn.name.substr(1, fn.name.size() - 2);
        try {
            fn.start = uint32_t(std::stoul(line.substr(b + 1, a - b - 1), nullptr, 16));
            fn.end = fn.start + uint32_t(std::stoul(line.substr(a + 1)));
        } catch (...) {
            continue;
        }
        out.push_back(fn);
    }
    return true;
}

bool read_symbols(const Config& cfg, std::vector<Func>& out) {
    if (!cfg.symbols_text.empty()) {
        std::istringstream f(cfg.symbols_text);
        return read_symbols_from(f, out);
    }
    std::ifstream f(cfg.symbols_csv);
    if (!f) {
        std::cerr << "[gen] symboles illisibles : "_g << cfg.symbols_csv << "\n"_g;
        return false;
    }
    return read_symbols_from(f, out);
}

void write_if_changed(const std::string& path, const std::string& text) {
    std::ifstream in(path, std::ios::binary);
    if (in) {
        std::stringstream ss;
        ss << in.rdbuf();
        if (ss.str() == text) return;   // inchange : ne pas forcer une recompilation
    }
    std::ofstream(path, std::ios::binary) << text;
}

}  // namespace

std::string file_head() {
    return std::string("// Genere par njrecomp (Nightjar). Ne pas modifier.\n#include \"functions.h\"\n"
                       "#include \"nj_ops.h\"\n#include <atomic>\n#include <cmath>\n\n"
                       "using namespace nj::runtime;\n\nnamespace g {\n\n"_g);
}

// Etat partage par l'analyse et l'emission (njrecomp, et la bibliotheque de P3 : nj_lib.h).
struct Resume { uint32_t pc; size_t fn; };
struct GenState {
    const Config& cfg;
    const Image& img;
    GenStats& st;
    std::vector<Func> funcs;
    std::unordered_map<uint32_t, size_t> entry_of;
    std::unordered_set<uint32_t> code_ptrs;
    std::set<uint32_t> mid_targets;
    EmitEnv env;
    std::unordered_set<uint32_t> fm_guard;
    bool fastmem = false;
    size_t fm_fast = 0, fm_guarded = 0, fm_fn_guarded = 0;
    std::vector<std::string> fm_guarded_names;
    std::ofstream* frag = nullptr;
};

// Fonctions, points de reprise des autres fonctions, environnement de l'emetteur, sites fastmem.
bool analyze(GenState& g) {
    const Config& cfg = g.cfg;
    const Image& img = g.img;
    GenStats& st = g.st;
    auto& funcs = g.funcs;
    auto& entry_of = g.entry_of;
    auto& code_ptrs = g.code_ptrs;
    auto& mid_targets = g.mid_targets;
    auto& env = g.env;
    auto& fm_guard = g.fm_guard;
    auto& fastmem = g.fastmem;
    auto& fm_fast = g.fm_fast;
    auto& fm_guarded = g.fm_guarded;
    auto& fm_fn_guarded = g.fm_fn_guarded;
    auto& fm_guarded_names = g.fm_guarded_names;
    (void)cfg; (void)img; (void)st; (void)funcs; (void)entry_of; (void)code_ptrs; (void)mid_targets; (void)env;
    (void)fm_guard; (void)fastmem; (void)fm_fast; (void)fm_guarded; (void)fm_fn_guarded; (void)fm_guarded_names;
    GenState* gp = &g;
    // ------------------------------------------------------------ fonctions
    std::vector<Func> syms;
    if (!read_symbols(cfg, syms)) return false;
    for (auto& f : syms)
        if (f.end > f.start && img.is_code(f.start)) funcs.push_back(f);
    std::sort(funcs.begin(), funcs.end(), [](const Func& a, const Func& b) { return a.start < b.start; });
    // doublons exacts d'adresse : on garde le premier
    funcs.erase(std::unique(funcs.begin(), funcs.end(),
                            [](const Func& a, const Func& b) { return a.start == b.start; }),
                funcs.end());
    for (size_t k = 0; k + 1 < funcs.size(); ++k)
        if (funcs[k].end > funcs[k + 1].start) {
            funcs[k].end = funcs[k + 1].start;
            ++st.overlaps;
        }
    // trous du code non couverts par un symbole : fonctions anonymes (mots nuls de bourrage exclus)
    {
        std::vector<Func> gaps;
        size_t fi = 0;
        for (const auto& s : img.segments) {
            if (!s.code) continue;
            uint32_t a = s.addr;
            const uint32_t e = s.addr + s.size;
            while (a < e) {
                while (fi < funcs.size() && funcs[fi].end <= a) ++fi;
                if (fi < funcs.size() && funcs[fi].start <= a) {
                    a = funcs[fi].end;
                    continue;
                }
                const uint32_t stop = (fi < funcs.size() && funcs[fi].start < e) ? funcs[fi].start : e;
                while (a < stop && img.word(a) == 0) a += 4;
                if (a < stop) {
                    Func g;
                    g.start = a;
                    g.end = stop;
                    g.name = "fn_"_g + hx8(a);
                    gaps.push_back(g);
                }
                a = stop;
            }
        }
        st.gap_functions = gaps.size();
        funcs.insert(funcs.end(), gaps.begin(), gaps.end());
        std::sort(funcs.begin(), funcs.end(), [](const Func& a, const Func& b) { return a.start < b.start; });
    }
    // noms uniques, liaisons natives et enveloppes
    {
        std::unordered_map<std::string, int> seen;
        const std::unordered_set<std::string> natives(cfg.hle_natives.begin(), cfg.hle_natives.end());
        for (auto& f : funcs) {
            if (natives.count(f.name)) {
                f.native = f.name;
                ++st.natives_bound;
            }
            std::string n = sanitize(f.name);
            if (seen[n]++) n += "_"_g + hx8(f.start);
            f.name = n;
            auto w = cfg.hle_wraps.find(f.start);
            if (w != cfg.hle_wraps.end()) {
                f.wrap = w->second;
                ++st.wraps;
            }
        }
    }
    st.functions = funcs.size();
    for (size_t k = 0; k < funcs.size(); ++k) entry_of[funcs[k].start] = k;

    // ------------------------------------------------------------ pointeurs de code dans les donnees
    for (const auto& s : img.segments) {
        if (s.code || s.bytes.empty()) continue;
        for (uint32_t off = 0; off + 4 <= s.bytes.size(); off += 4) {
            const uint32_t w = (uint32_t(s.bytes[off]) << 24) | (uint32_t(s.bytes[off + 1]) << 16) |
                               (uint32_t(s.bytes[off + 2]) << 8) | s.bytes[off + 3];
            if (!(w & 3) && img.is_code(w)) code_ptrs.insert(w);
        }
    }
    st.code_pointers = code_ptrs.size();

    // ------------------------------------------------------------ environnement de l'emetteur
    env.img = &img;
    env.entry_name = [gp](uint32_t a) -> std::string {
        auto it = gp->entry_of.find(a);
        if (it == gp->entry_of.end()) return ""_g;
        if (gp->cfg.slot_calls) return "::nj_slot["_g + std::to_string(it->second) + "]"_g;
        return "g::"_g + gp->funcs[it->second].name;
    };
    env.native_name = [gp](uint32_t a) -> std::string {
        auto it = gp->entry_of.find(a);
        if (it == gp->entry_of.end()) return ""_g;
        return gp->funcs[it->second].native.empty() ? ""_g : "::"_g + gp->funcs[it->second].native;
    };
    env.fixup = [gp](uint32_t a, uint32_t& mod, bool& bss) {
        auto it = gp->img.fixups.find(a);
        if (it == gp->img.fixups.end()) return false;
        mod = it->second.module;
        bss = it->second.bss;
        return true;
    };
    env.branch_override = [gp](uint32_t a) -> uint32_t {
        auto it = gp->img.branch_targets.find(a);
        return it == gp->img.branch_targets.end() ? 0u : it->second;
    };

    // Cibles de branchement hors de la fonction courante qui tombent au milieu d'une autre
    // (CodeWarrior : bl __save_fpr + 4 * (32 - n)) : points de reprise, sinon le repartiteur ne
    // trouve que l'interpreteur.
    for (const Func& f : funcs) {
        if (!f.native.empty()) continue;
        env.fn_start = f.start;
        env.fn_end = f.end;
        for (uint32_t a = f.start; a < f.end; a += 4) {
            const uint32_t op = img.word(a);
            if (!is_call(op) && !is_local_branch(op)) continue;
            const uint32_t t = branch_target(env, a, op);
            if (t && (t < f.start || t >= f.end) && !entry_of.count(t)) mid_targets.insert(t);
        }
    }

    // Fastmem : sites gardes (premiere colonne hexadecimale de chaque ligne, # = commentaire).
    fastmem = !cfg.fastmem_guard.empty() || !cfg.fastmem_guard_text.empty();
    if (fastmem) {
        std::ifstream gf;
        std::istringstream gt(cfg.fastmem_guard_text);
        if (cfg.fastmem_guard_text.empty()) {
            gf.open(cfg.fastmem_guard);
            if (!gf) { std::cerr << "[njrecomp] fastmem_guard introuvable : "_g << cfg.fastmem_guard << "\n"_g; return false; }
        }
        std::istream& in = cfg.fastmem_guard_text.empty() ? static_cast<std::istream&>(gf) : gt;
        std::string line;
        while (std::getline(in, line))
            if (!line.empty() && line[0] != '#') fm_guard.insert(uint32_t(std::stoul(line, nullptr, 16)));
    }
    return true;
}

// functions.h : declarations des fonctions du jeu, natives et enveloppes.
std::string functions_header(const GenState& g) {
    const auto& funcs = g.funcs;
    const Config& cfg = g.cfg;
    std::string h = "// Genere par njrecomp (Nightjar). Ne pas modifier.\n#pragma once\n"
                    "#include \"runtime/cpu_context.h\"\n#include \"runtime/rel_aot.h\"\n"
                    "#include \"runtime/hw/hw.h\"\n\n"_g;
    std::set<std::string> natives, wraps;
    for (const auto& f : funcs) {
        if (!f.native.empty()) natives.insert(f.native);
        if (!f.wrap.empty()) wraps.insert(f.wrap);
    }
    h += "extern \"C\" {\n"_g;
    for (const auto& n : natives) h += "void "_g + n + "(nj::runtime::GuestCpu& ctx);\n"_g;
    h += "}\n"_g;
    for (const auto& w : wraps)
        h += "void "_g + w + "(nj::runtime::GuestCpu& ctx, void (*orig)(nj::runtime::GuestCpu&));\n"_g;
    h += "\nnamespace g {\n"_g;
    for (const auto& f : funcs) {
        if (!f.native.empty()) continue;
        h += "void "_g + f.name + "(nj::runtime::GuestCpu& ctx);\n"_g;
        if (!f.wrap.empty()) h += "void "_g + f.name + "__orig(nj::runtime::GuestCpu& ctx);\n"_g;
    }
    h += "}  // namespace g\n"_g;
    if (cfg.slot_calls)
        h += "extern \"C\" void (*nj_slot[])(nj::runtime::GuestCpu&);\n"_g;
    return h;
}

// C++ d'une fonction du jeu (non native) ; ajoute ses points de reprise a `resumes`.
std::string emit_function(GenState& g, size_t fi, std::vector<Resume>& resumes) {
    const Config& cfg = g.cfg;
    const Image& img = g.img;
    GenStats& st = g.st;
    auto& funcs = g.funcs;
    auto& entry_of = g.entry_of;
    auto& code_ptrs = g.code_ptrs;
    auto& mid_targets = g.mid_targets;
    auto& env = g.env;
    auto& fm_guard = g.fm_guard;
    auto& fastmem = g.fastmem;
    auto& fm_fast = g.fm_fast;
    auto& fm_guarded = g.fm_guarded;
    auto& fm_fn_guarded = g.fm_fn_guarded;
    auto& fm_guarded_names = g.fm_guarded_names;
    (void)cfg; (void)img; (void)st; (void)funcs; (void)entry_of; (void)code_ptrs; (void)mid_targets; (void)env;
    (void)fm_guard; (void)fastmem; (void)fm_fast; (void)fm_guarded; (void)fm_fn_guarded; (void)fm_guarded_names;
        const Func& f = funcs[fi];
        env.fn_start = f.start;
        env.fn_end = f.end;
        // points de reprise et cibles locales
        std::set<uint32_t> resume = {f.start}, labels, targets;
        // Raison de chaque point de reprise (la premiere trouvee), pour GenStats::rp_by.
        std::map<uint32_t, int> why = {{f.start, 0}};
        auto add_resume = [&](uint32_t a, int k) { resume.insert(a); why.emplace(a, k); };
        for (uint32_t a = f.start; a < f.end; a += 4) {
            const uint32_t op = img.word(a);
            const int p = int(op >> 26);
            if (is_call(op)) add_resume(a + 4, 1);
            if (p == 17) add_resume(a + 4, 2);                                           // sc
            if (p == 31 && int((op >> 1) & 0x3FF) == 146) add_resume(a + 4, 2);          // mtmsr
            if (is_local_branch(op)) {
                const uint32_t t = branch_target(env, a, op);
                if (t >= f.start && t < f.end) {
                    labels.insert(t);
                    targets.insert(t);
                    if (t <= a) add_resume(a, 3);                                      // verification
                }
            }
            if (a != f.start && code_ptrs.count(a)) add_resume(a, 4);
            if (a != f.start && mid_targets.count(a)) add_resume(a, 5);
        }
        for (const auto& [a, k] : why)
            if (a < f.end) ++st.rp_by[k];
        // Reprise froide des retours de boucle (cold_loop_resume) : le switch vise une copie du
        // branchement placee en fin de fonction (R_<a>), et non l'etiquette posee devant le
        // branchement chaud. Sans cette jonction, le compilateur branche sur les drapeaux de la
        // comparaison au lieu de relire ctx.cr (ldrb) a chaque tour de boucle (30/09).
        std::set<uint32_t> cold;
        if (cfg.cold_loop_resume)
            for (const auto& [a, k] : why)
                if (k == 3 && a != f.start && a < f.end && !code_ptrs.count(a) && !mid_targets.count(a))
                    cold.insert(a);
        for (uint32_t a : resume)
            if (a < f.end && (!cold.count(a) || targets.count(a))) labels.insert(a);
        for (uint32_t a : cold)
            if (a + 4 < f.end) labels.insert(a + 4);
        env.labels = &labels;
        std::set<uint32_t> block_start = labels;
        block_start.insert(f.start);
        for (uint32_t a = f.start; a < f.end; a += 4)
            if (ends_block(img.word(a))) block_start.insert(a + 4);

        std::string body;
        const std::string fname = f.wrap.empty() ? f.name : f.name + "__orig"_g;
        body += "void "_g + fname + "(GuestCpu& ctx) {\n"_g;
        // Registres locaux seulement dans les fonctions feuilles (sans appel ni sc) : ailleurs, la
        // reecriture / relecture de tous les registres autour de chaque appel coutait plus que le
        // gain (S5, 29/09 : 75 -> 17 millions d'instructions par seconde avec des locaux partout ;
        // ARMv7 n'a que ~13 registres, les locaux debordent sur la pile de toute facon).
        {
            bool leaf = true;
            for (uint32_t a = f.start; a < f.end && leaf; a += 4) {
                const uint32_t op = img.word(a);
                if (is_call(op) || int(op >> 26) == 17) leaf = false;
            }
            g_local_regs = cfg.local_regs && leaf;
            g_block_count = cfg.block_count;
            g_cond_before_poll = cfg.cond_before_poll;
            g_budget = cfg.budget;
            g_psq_const = cfg.psq_const;
        }
        // Fastmem, par site (30/09, remplace la garde de fonction entiere par lis) : marquage des
        // registres. Un registre charge par lis/addis d'une adresse speciale (0xCC00-0xCC01 MMIO et
        // tuyau GX, 0xCD00-0xCD01, 0xC800-0xC8FF EFB, 0xE000-0xE001 cache verrouille, 0x0C00-0x0C01,
        // 0x0D00-0x0D01), ou derive d'un registre marque par addi/addis/ori/oris/xori/add/subf/or/xor,
        // est marque pour TOUTE la fonction (union, prudent). Un acces dont la base (rA, ou rA/rB en
        // forme indexee) est marquee reste garde ; les autres sont rapides. Ainsi `addis r9, r9,
        // 0xE000` ne garde que les acces par r9 dans les boucles de peau, et plus toute la fonction.
        // Reste la regle de nom pour les familles froides du SDK qui parlent au materiel (sans GX, dont
        // les ecritures au tuyau passent par un lis marque).
        bool fn_guard = false;
        uint32_t fm_taint = 0;
        if (fastmem) {
            auto special = [](uint32_t imm) {
                return (imm >= 0xCC00 && imm <= 0xCC01) || (imm >= 0xCD00 && imm <= 0xCD01) ||
                       (imm >= 0xC800 && imm <= 0xC8FF) || (imm >= 0xE000 && imm <= 0xE001) ||
                       (imm >= 0x0C00 && imm <= 0x0C01) || (imm >= 0x0D00 && imm <= 0x0D01);
            };
            for (int pass = 0; pass < 32; ++pass) {
                const uint32_t before_t = fm_taint;
                auto t = [&](uint32_t r) { return r != 0 && (fm_taint >> r) & 1u; };
                for (uint32_t a = f.start; a < f.end; a += 4) {
                    const uint32_t op = img.word(a);
                    const uint32_t pr = op >> 26, rd = (op >> 21) & 31, ra = (op >> 16) & 31, rb = (op >> 11) & 31;
                    uint32_t dst = 32;
                    bool mark = false;
                    if (pr == 15) { dst = rd; mark = special(op & 0xFFFF) || t(ra); }          // addis / lis
                    else if (pr == 14) { dst = rd; mark = t(ra); }                             // addi
                    else if (pr == 24 || pr == 25 || pr == 26 || pr == 27) { dst = ra; mark = t(rd); }   // ori oris xori xoris
                    else if (pr == 31) {
                        const uint32_t xo = (op >> 1) & 0x3FF;
                        if (xo == 266 || xo == 10 || xo == 138 || xo == 40 || xo == 8) { dst = rd; mark = t(ra) || t(rb); }  // add addc adde subf subfc
                        else if (xo == 444 || xo == 316) { dst = ra; mark = t(rd) || t(rb); }                               // or xor
                    }
                    if (dst < 32 && mark) fm_taint |= 1u << dst;
                }
                if (fm_taint == before_t) break;
            }
            std::string n = f.name;
            while (n.rfind("__"_g, 0) == 0) n = n.substr(2);
            if (n.rfind("re4_"_g, 0) == 0) n = n.substr(4);
            static const char* kSdk[] = {"OS"_g, "VI"_g, "DSP"_g, "AI"_g, "AR"_g, "EXI"_g, "SI"_g, "CARD"_g, "DVD"_g,
                                         "PAD"_g, "LC"_g, "PPC"_g, "DB"_g};
            for (const char* p : kSdk) {
                const size_t k = std::strlen(p);
                if (n.size() > k && n.compare(0, k, p) == 0 && (std::isupper((unsigned char)n[k]) || n[k] == '_'))
                    fn_guard = true;
            }
            if (cfg.guard_fns.count(f.start)) fn_guard = true;
            if (fn_guard) {
                ++fm_fn_guarded;
                fm_guarded_names.push_back(f.name);
            }
        }
        g_used_regs = 0;
        body += "NJ_DECL\n"_g;   // registres locaux (remplace apres l'emission)
        body += "    if (ctx.pc != "_g + hx(f.start) + ") {\n        switch (ctx.pc) {\n"_g;
        for (uint32_t a : resume)
            if (a != f.start && a < f.end)
                body += "        case "_g + hx(a) + ": goto "_g + (cold.count(a) ? "R_"_g : "L_"_g) + hx8(a) + ";\n"_g;
        body += "        default: nj::runtime::interp_advance(ctx); return;\n        }\n    }\n"_g;
        bool falls = true;
        for (uint32_t a = f.start; a < f.end; a += 4) {
            if (labels.count(a)) body += "L_"_g + hx8(a) + ":;\n"_g;
            if (block_start.count(a)) {
                auto nx = block_start.upper_bound(a);
                const uint32_t stop = (nx == block_start.end() || *nx > f.end) ? f.end : *nx;
                if (cfg.block_count)
                    body += "    ctx.retired += "_g + std::to_string((stop - a) / 4) + ";\n"_g;
            }
            const uint32_t op = img.word(a);
            const size_t before = body.size();
            const EmitResult r = emit_instr(env, a, op, body);
            if (r.unknown) ++st.unknown;
            falls = r.falls_through;
            // Fastmem : acces de ce site en direct (NJ_FM), sauf site ou fonction gardes. lmw/stmw
            // (fast_ptr, deja un seul controle) et psq restent tels quels.
            if (fastmem) {
                std::string t = body.substr(before);
                if (t.find("ctx.mmu."_g) != std::string::npos && t.find("fast_ptr("_g) == std::string::npos) {
                    // Base de l'acces : rA (formes D), rA et rB (formes indexees, primaire 31).
                    const uint32_t pr = op >> 26, ra = (op >> 16) & 31, rb = (op >> 11) & 31;
                    const bool base_marked = (ra != 0 && ((fm_taint >> ra) & 1u)) ||
                                             (pr == 31 && rb != 0 && ((fm_taint >> rb) & 1u));
                    if (fn_guard || fm_guard.count(a) || base_marked) {
                        ++fm_guarded;
                    } else {
                        static const std::pair<const char*, const char*> kOps[] = {
                            {"read8("_g, "r8"_g}, {"read16("_g, "r16"_g}, {"read32("_g, "r32"_g}, {"read_f32("_g, "rf32"_g},
                            {"read_f64("_g, "rf64"_g}, {"write8("_g, "w8"_g}, {"write16("_g, "w16"_g},
                            {"write32("_g, "w32"_g}, {"write_f32("_g, "wf32"_g}, {"write_f64("_g, "wf64"_g}};
                        for (const auto& [from, op] : kOps) {
                            const std::string a1 = std::string("ctx.mmu."_g) + from;
                            const std::string b1 = std::string("NJ_FM("_g) + op + ", "_g + hx(a) + "u) "_g;
                            for (size_t q = t.find(a1); q != std::string::npos; q = t.find(a1, q + b1.size()))
                                t.replace(q, a1.size(), b1);
                        }
                        body = body.substr(0, before) + t;
                        ++fm_fast;
                    }
                } else if (t.find("fast_ptr("_g) != std::string::npos) {
                    // lmw/stmw : la branche de secours (hors MEM1 et L2, jamais prise en pratique)
                    // garde l'acces compact de MMU plutot que nj::FM, dont les chemins gardes
                    // developpent le cache verrouille et le tuyau GX : a chaque prologue/epilogue, ca
                    // faisait +25 % de code (output_10, 30/09). NJ_MMU_OOL. redevient ctx.mmu. apres
                    // le passage ctx.mmu. -> nj_fm.
                    const std::string a1 = "else {"_g;
                    size_t q = t.find(a1);
                    if (q != std::string::npos) {
                        std::string tail = t.substr(q);
                        for (size_t k = tail.find("ctx.mmu."_g); k != std::string::npos; k = tail.find("ctx.mmu."_g, k + 12))
                            tail.replace(k, 8, "NJ_MMU_OOL."_g);
                        t = t.substr(0, q) + tail;
                        body = body.substr(0, before) + t;
                    }
                }
            }
            // Sites d'acces lents (slow_sites, preparation du fastmem) : adresse de l'instruction
            // posee autour de chaque acces memoire, lue par les chemins lents (cpu_context.h).
            if (cfg.slow_sites) {
                const std::string t = body.substr(before);
                if (t.find("ctx.mmu."_g) != std::string::npos || t.find("ctx.psq_"_g) != std::string::npos)
                    body = body.substr(0, before) + "    nj::runtime::g_nj_site = "_g + hx(a) + "u;\n"_g + t +
                           "    nj::runtime::g_nj_site = 0;\n"_g;
            }
            if (g.frag && g.frag->is_open())
                *g.frag << "@@ "_g << hx8(a) << " "_g << hx8(op) << "\n"_g << body.substr(before) << "\n"_g;
        }
        if (falls) body += "    ctx.pc = "_g + hx(f.end) + "; NJ_WB; return;   // suite dans la fonction suivante\n"_g;
        for (uint32_t a : cold) {
            body += "R_"_g + hx8(a) + ":;   // reprise froide du retour de boucle "_g + hx(a) + "\n"_g;
            const EmitResult r = emit_instr(env, a, img.word(a), body);
            if (r.falls_through)
                body += a + 4 < f.end ? "    goto L_"_g + hx8(a + 4) + ";\n"_g
                                      : "    ctx.pc = "_g + hx(f.end) + "; NJ_WB; return;\n"_g;
        }
        body += "}\n\n"_g;
        // Registres locaux : declaration + chargement en tete (avant l'aiguillage des points de
        // reprise, pour que toute entree les trouve charges), reecriture / relecture aux marqueurs.
        {
            std::string decl, wb, rl;
            for (int n = 0; n < 32; ++n) {
                if (!(g_used_regs & (1u << n))) continue;
                const std::string k = std::to_string(n);
                decl += "    uint32_t r"_g + k + " = ctx.gpr["_g + k + "];\n"_g;
                wb += "ctx.gpr["_g + k + "] = r"_g + k + "; "_g;
                rl += "r"_g + k + " = ctx.gpr["_g + k + "]; "_g;
            }
            auto replace_all = [](std::string& t, const std::string& a, const std::string& b) {
                for (size_t p = t.find(a); p != std::string::npos; p = t.find(a, p + b.size()))
                    t.replace(p, a.size(), b);
            };
            // Base de MEM1 en variable locale (nj::FM, nj_ops.h) ; avant l'insertion de decl, qui
            // lit ctx.gpr mais pas ctx.mmu.
            if (cfg.local_mem_base &&
                (body.find("ctx.mmu."_g) != std::string::npos || body.find("NJ_FM("_g) != std::string::npos)) {
                replace_all(body, "ctx.mmu."_g, "nj_fm."_g);
                replace_all(body, "NJ_MMU_OOL."_g, "ctx.mmu."_g);
                decl = "    const nj::FM nj_fm{ctx.mmu.mem1.data(), ctx.mmu};\n"_g + decl;
            }
            replace_all(body, "NJ_DECL\n"_g, decl);
            replace_all(body, "NJ_WB; "_g, wb);
            replace_all(body, "NJ_WB;"_g, wb);
            replace_all(body, "NJ_RL; "_g, rl);
            replace_all(body, "NJ_RL;"_g, rl);
        }
        if (!f.wrap.empty())
            body += "void "_g + f.name + "(GuestCpu& ctx) {\n    if (ctx.pc != "_g + hx(f.start) + ") { "_g + fname +
                    "(ctx); return; }\n    ::"_g + f.wrap + "(ctx, &"_g + fname + ");\n}\n\n"_g;
        for (uint32_t a : resume)
            if (a < f.end) resumes.push_back({a, fi});
        st.labels += labels.size();
        st.instructions += (f.end - f.start) / 4;
    return body;
}

// Projet complet (outil njrecomp) ; le moteur de diffusion n'embarque que le traducteur.
#ifndef NJ_RELEASE
bool generate(const Config& cfg, const Image& img, GenStats& st) {
    GenState g{cfg, img, st};
    if (!analyze(g)) return false;
    auto& funcs = g.funcs;
    auto& entry_of = g.entry_of;
    auto& code_ptrs = g.code_ptrs;
    auto& env = g.env;
    auto& fastmem = g.fastmem;
    auto& fm_guard = g.fm_guard;
    auto& fm_fast = g.fm_fast;
    auto& fm_guarded = g.fm_guarded;
    auto& fm_fn_guarded = g.fm_fn_guarded;
    auto& fm_guarded_names = g.fm_guarded_names;
    (void)entry_of; (void)code_ptrs; (void)env;
    // ------------------------------------------------------------ emission
    fs::create_directories(cfg.output_dir);
    std::vector<Resume> resumes;   // points de reprise (entrees comprises), pour la table
    std::vector<std::string> files;
    std::string cur;
    size_t cur_instr = 0;
    auto flush_file = [&]() {
        if (cur.empty()) return;
        const std::string name = "output_"_g + std::to_string(files.size()) + ".cpp"_g;
        write_if_changed(cfg.output_dir + "/"_g + name, file_head() + cur + "}  // namespace g\n"_g);
        files.push_back(name);
        cur.clear();
        cur_instr = 0;
    };

    std::ofstream frag;
    if (!cfg.fragments_path.empty()) frag.open(cfg.fragments_path, std::ios::binary);
    g.frag = &frag;
    for (size_t fi = 0; fi < funcs.size(); ++fi) {
        const Func& f = funcs[fi];
        if (!f.native.empty()) {
            resumes.push_back({f.start, fi});
            continue;
        }
        const std::string body = emit_function(g, fi, resumes);
        cur += body;
        cur_instr += (f.end - f.start) / 4;
        if (cur_instr >= cfg.instructions_per_file) flush_file();
    }
    flush_file();
    st.files = files.size();
    st.resume_points = resumes.size();
    if (fastmem) {
        std::ofstream gl(cfg.output_dir + "/fastmem_fonctions_gardees.txt"_g);
        for (const auto& n : fm_guarded_names) gl << n << "\n"_g;
    }
    if (fastmem)
        std::printf("[njrecomp] fastmem : %zu instructions rapides, %zu gardees, %zu fonctions gardees, %zu sites listes\n"_g,
                    fm_fast, fm_guarded, fm_fn_guarded, fm_guard.size());

    // ------------------------------------------------------------ functions.h
    write_if_changed(cfg.output_dir + "/functions.h"_g, functions_header(g));

    // ------------------------------------------------------------ main_output.cpp
    {
        std::sort(resumes.begin(), resumes.end(), [](const Resume& a, const Resume& b) { return a.pc < b.pc; });
        resumes.erase(std::unique(resumes.begin(), resumes.end(),
                                  [](const Resume& a, const Resume& b) { return a.pc == b.pc; }),
                      resumes.end());
        uint32_t dol_end = 0;
        for (const auto& s : img.segments)
            if (s.module == 0) dol_end = std::max(dol_end, s.addr + s.size);
        std::string m;
        m += "// Genere par njrecomp (Nightjar). Ne pas modifier.\n#include \"functions.h\"\n"
             "#include \"runtime/dispatch.h\"\n\nusing namespace nj::runtime;\n\n"_g;
        // Tables seulement : la repartition vit dans le moteur (runtime/src/core/dispatch.cpp, P3 jalon 3).
        m += "namespace {\nconst FuncBound kBounds[] = {\n"_g;
        for (const auto& f : funcs) {
            const std::string fn = f.native.empty() ? "g::"_g + f.name : "::"_g + f.native;
            m += "    {"_g + hx(f.start) + ", "_g + hx(f.end) + ", "_g + fn + ", "_g + std::to_string(&f - funcs.data()) + "},\n"_g;
        }
        m += "};\nconst Resume kResume[] = {\n"_g;
        for (const auto& r : resumes) {
            const Func& f = funcs[r.fn];
            const bool hle = !f.native.empty() && r.pc == f.start;
            const std::string fn = hle ? "::"_g + f.native : "g::"_g + f.name;
            m += std::string("    {"_g) + hx(r.pc) + ", "_g + (hle ? "true"_g : "false"_g) + ", "_g + fn + ", "_g +
                 std::to_string(r.fn) + "},\n"_g;
        }
        m += "};\n"_g;
        m += "}  // namespace\n\n"_g;
        if (cfg.slot_calls) {
            m += "// P3 : table des cases (docs/P3_TRADUCTEUR.md) ; nj_native[n] = code natif branche.\n"_g;
            m += "extern \"C\" void (*nj_slot[])(GuestCpu&) = {\n"_g;
            for (const auto& f : funcs) m += "    "_g + (f.native.empty() ? "g::"_g + f.name : "::"_g + f.native) + ",\n"_g;
            m += "};\nbool nj_native["_g + std::to_string(funcs.size()) + "];\n"_g;
            m += "extern \"C\" const unsigned nj_slot_count = "_g + std::to_string(funcs.size()) + ";\n"_g;
            m += "extern \"C\" const char* const nj_slot_names[] = {\n"_g;
            for (const auto& f : funcs) m += "    \""_g + f.name + "\",\n"_g;
            m += "};\n"_g;
        }
        m += "namespace {\nconst bool kDispatchRegistered = [] {\n    DispatchTables t;\n"
             "    t.bounds = kBounds;\n    t.bound_count = sizeof(kBounds) / sizeof(kBounds[0]);\n"
             "    t.resumes = kResume;\n    t.resume_count = sizeof(kResume) / sizeof(kResume[0]);\n"_g;
        if (cfg.slot_calls)
            m += "    t.slots = nj_slot;\n    t.native = nj_native;\n    t.slot_names = nj_slot_names;\n"
                 "    t.slot_count = "_g + std::to_string(funcs.size()) + ";\n"_g;
        m += "    t.rel_scan_lo = "_g + hx(dol_end) + ";\n    t.entry = "_g + hx(img.entry) +
             ";\n    dispatch_register(t);\n    return true;\n}();\n}  // namespace\n"_g;
        m += "namespace {\nconst nj::runtime::RelAotModule kRelAot[] = {\n"_g;
        for (const auto& mod : img.modules)
            m += "    {"_g + std::to_string(mod.id) + ", "_g + hx(mod.base) + ", "_g + hx(mod.image_size) + ", "_g +
                 hx(mod.bss) + ", "_g + hx(mod.bss_size) + "},\n"_g;
        m += "    {0, 0, 0, 0, 0}\n};\n"_g;
        m += "const bool kRelAotRegistered = (nj::runtime::rel_aot_register(kRelAot, "_g +
             std::to_string(img.modules.size()) + "), true);\n}  // namespace\n"_g;
        // Empreinte (FNV-1a 64) du main.dol traduit : le lanceur refuse un disque d'une autre version.
        {
            std::ifstream d(cfg.input_game_dir + "/sys/main.dol"_g, std::ios::binary);
            uint64_t h = 0xCBF29CE484222325ull;
            for (int c; (c = d.get()) != EOF;) h = (h ^ uint8_t(c)) * 0x100000001B3ull;
            char buf[40];
            std::snprintf(buf, sizeof buf, "0x%016llXull"_g, (unsigned long long)h);
            m += std::string("extern \"C\" const unsigned long long g_nj_dol_fnv = "_g) + buf + ";\n"_g;
        }
        // Relais des fonctions que le runtime appelle par leur nom (liaison C, comme il les declare).
        for (const auto& rc : cfg.runtime_calls) {
            bool found = false;
            for (const auto& f : funcs)
                if (f.name == rc && f.native.empty()) found = true;
            if (!found) {
                // absente de cette version (fonction modifiee, non appariee) : relais vide, le
                // runtime ne s'en sert pas pour cette version
                std::cerr << "[njrecomp] runtime_calls : "_g << rc << " absente de cette version, relais vide\n"_g;
                m += "extern \"C\" void "_g + rc + "(GuestCpu&) {}\n"_g;
                continue;
            }
            m += "extern \"C\" void "_g + rc + "(GuestCpu& ctx) { g::"_g + rc + "(ctx); }\n"_g;
        }
        write_if_changed(cfg.output_dir + "/main_output.cpp"_g, m);
    }

    // ------------------------------------------------------------ nj_ops.h, runtime, CMake
    {
        // nj_ops.h est a cote de l'executable njrecomp ou dans le depot : on le cherche
        const char* candidates[] = {"nj_ops.h"_g, "../recomp/src/nj_ops.h"_g, "../../recomp/src/nj_ops.h"_g};
        bool ok = false;
        for (const char* c : candidates)
            if (fs::exists(c)) {
                std::ifstream in(c, std::ios::binary);
                std::stringstream ss;
                ss << in.rdbuf();
                write_if_changed(cfg.output_dir + "/nj_ops.h"_g, ss.str());
                ok = true;
                break;
            }
        if (!ok && !cfg.runtime_source_dir.empty()) {
            const fs::path p = fs::path(cfg.runtime_source_dir).parent_path() / "recomp/src/nj_ops.h"_g;
            if (fs::exists(p)) {
                std::ifstream in(p, std::ios::binary);
                std::stringstream ss;
                ss << in.rdbuf();
                write_if_changed(cfg.output_dir + "/nj_ops.h"_g, ss.str());
                ok = true;
            }
        }
        if (!ok) {
            std::cerr << "[gen] nj_ops.h introuvable\n"_g;
            return false;
        }
    }
    if (!cfg.runtime_source_dir.empty()) {
        const fs::path src = cfg.runtime_source_dir, dst = fs::path(cfg.output_dir) / "runtime"_g;
        for (auto it = fs::recursive_directory_iterator(src); it != fs::recursive_directory_iterator(); ++it) {
            const fs::path rel{fs::relative(it->path(), src)};
            const std::string top = rel.begin()->string();
            if (top == "build"_g || top == ".git"_g) {
                if (it->is_directory()) { it.disable_recursion_pending(); }
                continue;
            }
            const fs::path d = dst / rel;
            if (it->is_directory()) {
                fs::create_directories(d);
            } else if (it->is_regular_file()) {
                if (!fs::exists(d) || fs::file_size(d) != fs::file_size(it->path()) ||
                    fs::last_write_time(d) < fs::last_write_time(it->path())) {
                    fs::create_directories(d.parent_path());
                    fs::copy_file(it->path(), d, fs::copy_options::overwrite_existing);
                    // copie datee de maintenant : la date d'origine peut etre plus ancienne qu'un
                    // objet deja compile, et make ne recompilerait pas le fichier change
                    fs::last_write_time(d, fs::file_time_type::clock::now());
                }
            }
        }
    }
    {
        std::string c;
        c += "# Genere par njrecomp (Nightjar).\ncmake_minimum_required(VERSION 3.20)\n"_g;
        c += "project("_g + cfg.project_name + " LANGUAGES C CXX)\n\n"_g;
        c += "set(CMAKE_POLICY_VERSION_MINIMUM 3.5)\nset(CMAKE_CXX_STANDARD 20)\nset(CMAKE_CXX_STANDARD_REQUIRED ON)\n"_g;
        c += "if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)\n"
             "    set(CMAKE_BUILD_TYPE Release CACHE STRING \"\" FORCE)\nendif()\n"_g;
        c += "if(CMAKE_CXX_COMPILER_ID MATCHES \"GNU|Clang\")\n"
             "    set(CMAKE_CXX_FLAGS_RELEASE \"-O2 -DNDEBUG\")\n    set(CMAKE_C_FLAGS_RELEASE \"-O2 -DNDEBUG\")\nendif()\n\n"_g;
        c += "if(NJ_RELEASE AND CMAKE_CXX_COMPILER_ID MATCHES \"GNU|Clang\")\n"
             "    get_filename_component(NJ_RACINE \"${CMAKE_SOURCE_DIR}\" DIRECTORY)\n"
             // GCC : la derniere regle qui convient l'emporte ; les dependances apres la racine
             "    foreach(nj_dir \"${CMAKE_BINARY_DIR}\" \"${CMAKE_SOURCE_DIR}\" \"${NJ_RACINE}\")\n"
             "        string(REPLACE \"/\" \"\\\\\" nj_w \"${nj_dir}\")\n"
             "        add_compile_options(\"-ffile-prefix-map=${nj_dir}=.\" \"-ffile-prefix-map=${nj_w}=.\")\n"
             "    endforeach()\n"
             "    foreach(nj_dep SDL2 GLAD TOMLPLUSPLUS)\n"
             "        if(FETCHCONTENT_SOURCE_DIR_${nj_dep})\n"
             "            string(REPLACE \"/\" \"\\\\\" nj_w \"${FETCHCONTENT_SOURCE_DIR_${nj_dep}}\")\n"
             "            add_compile_options(\"-ffile-prefix-map=${FETCHCONTENT_SOURCE_DIR_${nj_dep}}=${nj_dep}\" \"-ffile-prefix-map=${nj_w}=${nj_dep}\")\n"
             "        endif()\n    endforeach()\n"
             "    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)\n"
             // LTO : tout le moteur optimise et fondu a l'edition de liens (P2) ; NJ_LTO=OFF pour comparer
             "    option(NJ_LTO \"Optimisation a l'edition de liens (diffusion)\" ON)\n"
             "    if(NJ_LTO)\n"
             "        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)\n"   // CMake prend gcc-ar pour les archives
             "    endif()\nendif()\n"_g;
        c += "include(FetchContent)\n"_g;
        c += "FetchContent_Declare(tomlplusplus GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git GIT_TAG v3.4.0)\n"_g;
        c += "FetchContent_MakeAvailable(tomlplusplus)\n"_g;
        c += "set(SDL2_DISABLE_INSTALL ON CACHE BOOL \"\" FORCE)\n"_g;
        c += "FetchContent_Declare(SDL2 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-2.28.5)\n"_g;
        c += "FetchContent_MakeAvailable(SDL2)\n\nadd_subdirectory(runtime)\n\n"_g;
        c += "add_executable("_g + cfg.project_name + " runtime/src/core/main.cpp main_output.cpp"_g;
        for (const auto& f : files) c += " "_g + f;
        c += ")\n"_g;
        c += "target_include_directories("_g + cfg.project_name + " PRIVATE runtime/src runtime/include ${CMAKE_CURRENT_SOURCE_DIR})\n"_g;
        c += "target_link_libraries("_g + cfg.project_name + " PRIVATE njruntime)\n"_g;
        // L'executable complet contient le code traduit du jeu : jamais diffuse (P3). Le produit est
        // NightjarP3 (Nightjar.exe en diffusion).
        c += "if(WIN32 AND NJ_RELEASE)\n    target_link_options("_g + cfg.project_name + " PRIVATE -mwindows -static -s)\nendif()\n"_g;
        c += "if(WIN32 AND EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/runtime/res/re4native.rc\")\n"_g;
        c += "    target_sources("_g + cfg.project_name + " PRIVATE runtime/res/re4native.rc)\n"_g;
        c += "    target_include_directories("_g + cfg.project_name + " PRIVATE runtime/res)\nendif()\n"_g;
        // P3 (jalon 3) : le moteur seul, sans aucun code du jeu ; il traduit le disque au lancement.
        c += "if(NJ_P3_JIT)\n    add_executable(NightjarP3 runtime/src/core/main.cpp runtime/src/p3/p3_moteur_seul.cpp)\n"
             "    target_include_directories(NightjarP3 PRIVATE runtime/src runtime/include)\n"
             "    # pile du fil du jeu (std::thread : reserve de l'exe) : le code -O0 a de gros cadres (06/10)\n"
             "    if(WIN32)\n        target_link_options(NightjarP3 PRIVATE -Wl,--stack,268435456)\n    endif()\n"
             "    target_link_libraries(NightjarP3 PRIVATE njruntime)\n"
             // diffusion : -s comme avant (un strip apres coup laisse une table de chaines COFF) ; la carte
             // du lien, gardee sur la tour, donne les fonctions des adresses de plantage (12/10)
             "    if(WIN32 AND NJ_RELEASE)\n        target_link_options(NightjarP3 PRIVATE -mwindows -static -s -Wl,-Map=Nightjar.map)\n"
             "        set_target_properties(NightjarP3 PROPERTIES OUTPUT_NAME Nightjar)\n    endif()\n"
             "    if(WIN32 AND EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/runtime/res/re4native.rc\")\n"
             "        target_sources(NightjarP3 PRIVATE runtime/res/re4native.rc)\n"
             "        target_include_directories(NightjarP3 PRIVATE runtime/res)\n    endif()\nendif()\n"_g;
        write_if_changed(cfg.output_dir + "/CMakeLists.txt"_g, c);
    }
    return true;
}
#endif

// ------------------------------------------------------------ bibliotheque (nj_lib.h)
struct Translator::Impl {
    Config cfg;
    std::unique_ptr<Image> img;
    GenStats st;
    std::unique_ptr<GenState> g;
};

Translator::Translator() : impl_(std::make_unique<Impl>()) {}
Translator::~Translator() = default;

bool Translator::open(const Config& cfg, std::unique_ptr<Image> img) {
    impl_->cfg = cfg;
    impl_->img = std::move(img);
    impl_->g.reset(new GenState{impl_->cfg, *impl_->img, impl_->st});
    if (!analyze(*impl_->g)) return false;
    funcs_.clear();
    for (const Func& f : impl_->g->funcs) funcs_.push_back({f.start, f.end, !f.native.empty(), f.name, f.native});
    return true;
}

std::vector<Translator::Module> Translator::modules() const {
    std::vector<Module> m;
    for (const auto& x : impl_->img->modules) m.push_back({x.id, x.base, x.image_size, x.bss, x.bss_size});
    return m;
}
uint32_t Translator::entry() const { return impl_->img->entry; }
uint32_t Translator::dol_end() const {
    uint32_t e = 0;
    for (const auto& s : impl_->img->segments)
        if (s.module == 0) e = std::max(e, s.addr + s.size);
    return e;
}

std::string Translator::functions_header() const { return nj::functions_header(*impl_->g); }

std::string Translator::emit_batch(const std::vector<uint32_t>& fns, std::vector<Resume>& resumes) {
    std::vector<nj::Resume> r;
    std::string out = file_head();
    for (uint32_t fi : fns) {
        if (fi >= impl_->g->funcs.size() || !impl_->g->funcs[fi].native.empty()) continue;
        out += emit_function(*impl_->g, fi, r);
    }
    out += "}  // namespace g\n"_g;
    for (const auto& x : r) resumes.push_back({x.pc, (uint32_t)x.fn});
    return out;
}

}  // namespace nj
