// Repartition des fonctions du jeu traduit (P3, jalon 3). Reprise telle quelle de ce que njrecomp
// ecrivait dans main_output.cpp ; les tables viennent maintenant de dispatch_register().
#include "runtime/dispatch.h"
#include "runtime/p3_restart.h"
#include "runtime/cpu_context.h"
#include "runtime/crash_report.h"
#include "runtime/env.h"
#include "runtime/game_call.h"
#include "runtime/rel_aot.h"
#include "runtime/hw/hw.h"
#include "runtime/perf_journal.h"
#include "runtime/obf.h"
using namespace nj::obf_litteraux;   // "texte"_g : chiffre dans le binaire

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

namespace nj::runtime::input { extern std::atomic<bool> g_autostart_restart; }   // autostart_source.cpp
extern "C" void nj_p3_jit_start();   // p3_jit.cpp (NJ_P3_JIT) ou p3_jit_stub.cpp
extern "C" bool nj_p3_host_open();   // p3_host.cpp (NJ_P3_JIT) ou p3_jit_stub.cpp

namespace nj::runtime {
bool interpret_single(GuestCpu& ctx);   // hle/fallback_cpu.cpp

void (*g_p3_want)(uint32_t fn) = nullptr;
bool (*g_p3_need)(uint32_t fn) = nullptr;
P3Progress g_p3_progress;
std::atomic<bool> g_p3_restart_wanted{false};   // runtime/p3_restart.h

namespace {

DispatchTables g_t;

// Point de reprise exact (table triee), avec un petit cache.
struct ResumeSlot { uint32_t pc; const Resume* r; };
ResumeSlot g_resume_cache[1u << 14];

const Resume* find_resume(uint32_t pc) {
    ResumeSlot& s = g_resume_cache[(pc >> 2) & ((1u << 14) - 1)];
    if (s.r && s.pc == pc) return s.r;
    const Resume* b = g_t.resumes;
    const Resume* e = b + g_t.resume_count;
    const Resume* it = std::lower_bound(b, e, pc, [](const Resume& r, uint32_t v) { return r.pc < v; });
    if (it == e || it->pc != pc) return nullptr;
    s = {pc, it};
    return it;
}

// Fonction qui contient pc (reprise a une adresse qui n'est pas un point de reprise connu).
const FuncBound* find_bound_search(uint32_t pc) {
    const FuncBound* b = g_t.bounds;
    const FuncBound* e = b + g_t.bound_count;
    const FuncBound* it = std::upper_bound(b, e, pc, [](uint32_t v, const FuncBound& f) { return v < f.start; });
    if (it != b && pc < (it - 1)->end) return it - 1;
    return nullptr;
}
struct BoundSlot { uint32_t pc; const FuncBound* f; };
BoundSlot g_bound_cache[1u << 14];
const FuncBound* find_bound_entry(uint32_t pc) {
    BoundSlot& s = g_bound_cache[(pc >> 2) & ((1u << 14) - 1)];
    if (s.f && s.pc == pc) return s.f;
    const FuncBound* f = find_bound_search(pc);
    if (f) s = {pc, f};
    return f;
}

// Code d'une fonction : la case quand le code traduit passe par la table (P3 : branchee a chaud,
// absente des tables tant que la fonction n'est pas compilee), sinon le pointeur des tables.
inline GameFn code_of(uint32_t fn, GameFn f) { return g_t.slots ? g_t.slots[fn] : f; }

// Fonction native, ou rendue native sur-le-champ si elle ne peut pas etre interpretee (module REL).
inline bool ensure_native(const FuncBound* fb) {
    if (g_t.native[fb->fn]) return true;
    if (fb->start < 0xB0000000u || !g_p3_need) return false;
    // 19/10 : compilation sur demande dans le fil du jeu (gel de plusieurs secondes) ; toujours au journal,
    // meme sans NJ_P3_JOURNAL : le build de diffusion n'en disait rien.
    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = g_p3_need(fb->fn);
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    static int s_fail_logged = 0;
    if (ok ? sec > 0.02 : s_fail_logged++ < 20) {
        char b[80];
        std::snprintf(b, sizeof b, "P3 sur demande fn=%u %.2f s%s"_g, fb->fn, sec, ok ? "" : " echec");
        nj::perf::event(b);
    }
    return ok;
}

// L'interpreteur ne sait pas executer l'instruction : le jeu s'arrete (rapport de plantage).
void stop_in_interpreter(GuestCpu& ctx) {
    char at[64];
    std::snprintf(at, sizeof at, "pc %08X lr %08X", ctx.pc, ctx.lr);
    nj::crash::note("arret dans l'interpreteur"_g, at);
    ctx.alive = false;
}

inline void normalize_pc(GuestCpu& ctx) {
    if ((ctx.pc & 0xF0000000u) == 0xC0000000u) ctx.pc = (ctx.pc & 0x0FFFFFFFu) | 0x80000000u;
    else if ((ctx.pc & 0xF0000000u) == 0xD0000000u) ctx.pc = (ctx.pc & 0x0FFFFFFFu) | 0x90000000u;
    else if (ctx.pc < 0x80000000u) ctx.pc |= 0x80000000u;
    if (ctx.pc >= g_t.rel_scan_lo && ctx.pc < 0x81800000u) ctx.pc = rel_to_aot(ctx);
}

inline void run_at(GuestCpu& ctx) {
    const uint32_t target = ctx.pc;
    ++ctx.retired;
    // Fonction non branchee (P3) : le repartiteur l'interprete pas a pas.
    if (g_t.native) {
        if (const FuncBound* fb = find_bound_entry(target); fb && !ensure_native(fb)) {
            if (g_p3_want && target == fb->start) g_p3_want(fb->fn);
            if (!interpret_single(ctx)) stop_in_interpreter(ctx);
            return;
        }
    }
    if (const Resume* r = g_t.resume_count ? find_resume(target) : nullptr) {
        code_of(r->fn, r->func)(ctx);
        if (r->hle) ctx.pc = ctx.lr;
    } else if (const FuncBound* fb = find_bound_entry(target)) {
        code_of(fb->fn, fb->func)(ctx);
        if (fb->hle && target == fb->start) ctx.pc = ctx.lr;
    } else {
        interp_advance(ctx);   // code hors de toute fonction (copie a l'execution)
    }
}

}  // namespace

// P3, passerelle d'une case non branchee : la fonction appelee s'execute dans l'interpreteur
// jusqu'a son retour (pc == lr a l'entree, appel ou saut terminal) ; ses propres appels vers une
// fonction branchee repartent en natif. Une interruption qui change de contexte remonte comme
// dans le code genere (drain_callbacks).
extern "C" void nj_interp(GuestCpu& ctx) {
    const uint32_t ret = ctx.lr;
    if (const FuncBound* fb = find_bound_entry(ctx.pc)) {
        if (fb->start >= 0xB0000000u && ensure_native(fb)) {   // module REL : jamais interprete
            g_t.slots[fb->fn](ctx);
            return;
        }
        if (g_p3_want) g_p3_want(fb->fn);
    }
    while (ctx.alive) {
        if (ctx.retired >= ctx.check_deadline || hw::poll_now.load(std::memory_order_relaxed)) {
            if (drain_callbacks(ctx)) return;
        }
        normalize_pc(ctx);
        // retour au repartiteur : fin de la fonction, retour d'un rappel du moteur, aucun fil pret
        if (ctx.pc == ret || ctx.pc == 0xFFFFFFFCu || ctx.pc == 0) return;
        if (g_t.resume_count) {
            const Resume* r = find_resume(ctx.pc);
            if (r && g_t.native[r->fn]) {
                g_t.slots[r->fn](ctx);
                if (r->hle) ctx.pc = ctx.lr;
                continue;
            }
        } else if (const FuncBound* fb = find_bound_entry(ctx.pc); fb && ensure_native(fb)) {
            const uint32_t at = ctx.pc;
            g_t.slots[fb->fn](ctx);
            if (fb->hle && at == fb->start) ctx.pc = ctx.lr;
            continue;
        }
        ++ctx.retired;
        if (!interpret_single(ctx)) { stop_in_interpreter(ctx); return; }
    }
}

namespace {

// P3, simulation (etape B) : NJ_P3_CHAUDES=<liste> (fonctions_chaudes.py --liste) ne branche que
// les fonctions du DOL de la liste ; les autres passent par nj_interp. Modules REL : tous branches.
void p3_init() {
    for (std::size_t i = 0; i < g_t.bound_count; ++i) g_t.native[g_t.bounds[i].fn] = true;
    nj_p3_jit_start();
    const char* path = NJ_ENV("NJ_P3_CHAUDES");
    if (!path) return;
    std::FILE* f = std::fopen(path, "r");
    if (!f) return;
    std::vector<uint32_t> hot;
    char line[256];
    while (std::fgets(line, sizeof line, f))
        if (line[0] != '#') hot.push_back((uint32_t)std::strtoul(line, nullptr, 16));
    std::fclose(f);
    std::sort(hot.begin(), hot.end());
    std::size_t off = 0;
    for (std::size_t i = 0; i < g_t.bound_count; ++i) {
        const FuncBound& b = g_t.bounds[i];
        if (b.start >= 0xB0000000u) continue;
        if (std::binary_search(hot.begin(), hot.end(), b.start)) continue;
        g_t.native[b.fn] = false;
        g_t.slots[b.fn] = nj_interp;
        ++off;
    }
    std::cout << std::dec << "[P3] liste chaude : " << hot.size() << " fonctions ; " << off
              << " fonctions du DOL interpretees" << std::endl;
}

}  // namespace

void dispatch_register(const DispatchTables& t) {
    g_t = t;
    for (auto& s : g_resume_cache) s = {0, nullptr};
    for (auto& s : g_bound_cache) s = {0, nullptr};
}

const DispatchTables& dispatch_tables() { return g_t; }

}  // namespace nj::runtime

using nj::runtime::GuestCpu;

#if !defined(NJ_PRECOMPILED) && !defined(NJ_PRECOMPILED_MULTI)
// Execute la fonction a ctx.pc jusqu'a son retour au repartiteur (appels depuis le runtime).
extern "C" void nj_dispatch_one(GuestCpu& ctx) {
    nj::runtime::normalize_pc(ctx);
    nj::runtime::run_at(ctx);
}

extern "C" void run_game(GuestCpu& ctx) {
    using namespace nj::runtime;
    // Moteur seul (P3) : aucun code traduit n'a enregistre de tables, le moteur traduit le disque.
    if (!g_t.bounds) {
        if (!nj_p3_host_open()) {
            std::cerr << "[P3] aucun code du jeu : rien a executer"_g << std::endl;
            return;
        }
        input::g_autostart_restart = true;   // le jeu demarre maintenant (essais automatiques)
    } else if (g_t.slots) {
        p3_init();
    }
    if (ctx.pc == 0) ctx.pc = g_t.entry;
    while (ctx.alive) {
#if defined(_WIN32)
        if (setjmp(NJ_JB(ctx.exc_jump)) != 0) continue;
#else
        if (_setjmp(NJ_JB(ctx.exc_jump)) != 0) continue;
#endif
        while (ctx.alive) {
            if (ctx.pc == 0) {
                if ((ctx.msr & 0x8000) == 0) ctx.msr |= 0x8000;
                drain_callbacks(ctx);
                if (ctx.pc == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            if (ctx.pc == 0xFFFFFFFCu || ctx.retired >= ctx.check_deadline ||
                hw::poll_now.load(std::memory_order_relaxed))
                drain_callbacks(ctx);
            if (ctx.pc == 0xFFFFFFFCu) {
                if (!ctx.saved.empty()) ctx.pop_saved();
                if (--ctx.cb_nesting <= 0) {
                    ctx.inside_cb = false;
                    ctx.cb_nesting = 0;
                }
                continue;
            }
            normalize_pc(ctx);
            run_at(ctx);
        }
    }
}
#endif

// Appel synchrone d'une fonction du jeu depuis le moteur (20/10, eventail d'armes) : meme boucle que run_game
// (rappels, interruptions, changements de fil, son propre point de reprise des exceptions) jusqu'au retour a
// kCallMark. Tous les registres sont rendus ; seule la memoire du jeu change. Fil du jeu seulement, a un point
// de passage sur (PADRead de la boucle principale) : la fonction peut lire le disque et laisser tourner les
// autres fils, comme quand le jeu l'appelle lui-meme.
namespace {
constexpr uint32_t kCallMark = 0xFFFFFFF8u;
bool g_call_busy = false;
}  // namespace

namespace nj::runtime {

bool call_game_busy() { return g_call_busy; }

uint32_t call_game(GuestCpu& ctx, uint32_t fn, std::initializer_list<uint32_t> args) {
    if (g_call_busy || args.size() > 8) return 0;
    g_call_busy = true;
    const GekkoRegs keep = ctx;
    NjJmpBuf keep_jump;
    std::memcpy(&keep_jump, &ctx.exc_jump, sizeof(NjJmpBuf));
    uint32_t r = 3;
    for (uint32_t a : args) ctx.gpr[r++] = a;
    ctx.pc = fn;
    ctx.lr = kCallMark;
    while (ctx.alive && ctx.pc != kCallMark) {
#if defined(_WIN32)
        if (setjmp(NJ_JB(ctx.exc_jump)) != 0) continue;
#else
        if (_setjmp(NJ_JB(ctx.exc_jump)) != 0) continue;
#endif
        while (ctx.alive && ctx.pc != kCallMark) {
            if (ctx.pc == 0) {
                if ((ctx.msr & 0x8000) == 0) ctx.msr |= 0x8000;
                drain_callbacks(ctx);
                if (ctx.pc == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            if (ctx.pc == 0xFFFFFFFCu || ctx.retired >= ctx.check_deadline ||
                hw::poll_now.load(std::memory_order_relaxed))
                drain_callbacks(ctx);
            if (ctx.pc == 0xFFFFFFFCu) {
                if (!ctx.saved.empty()) ctx.pop_saved();
                if (--ctx.cb_nesting <= 0) {
                    ctx.inside_cb = false;
                    ctx.cb_nesting = 0;
                }
                continue;
            }
            normalize_pc(ctx);
            run_at(ctx);
        }
    }
    const uint32_t ret = ctx.gpr[3];
    static_cast<GekkoRegs&>(ctx) = keep;
    std::memcpy(&ctx.exc_jump, &keep_jump, sizeof(NjJmpBuf));
    g_call_busy = false;
    return ret;
}

}  // namespace nj::runtime
