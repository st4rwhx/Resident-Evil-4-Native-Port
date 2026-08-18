#include "runtime/rel_aot.h"
#include "runtime/log.h"

#include "runtime/cpu_context.h"
#include "runtime/env.h"

#include <cstdio>

namespace nj::runtime {

int32_t g_rel_delta[kRelMaxId][2] = {};

namespace {

// OSModuleInfo (src/lib/OSLink.c) : id +0, next +4, numSections +0xC, sectionInfoOffset +0x10
// (adresse de la table de sections une fois lie), bssSection +0x33 (u8). La table des modules
// lies commence a 0x800030C8 (__OSModuleList.head).
constexpr uint32_t kModuleListHead = 0x800030C8;

struct Active {
    uint32_t lo = 0, hi = 0, aot = 0, info = 0, sections = 0, id = 0;
};
Active g_active[32];
int g_active_n = 0;

const RelAotModule* g_modules = nullptr;
uint32_t g_module_count = 0;

const RelAotModule* find_aot(uint32_t id) {
    for (uint32_t i = 0; i < g_module_count; ++i)
        if (g_modules[i].id == id)
            return &g_modules[i];
    return nullptr;
}

bool in_mem1(uint32_t a) { return a >= 0x80000000u && a < 0x81800000u; }

// Reconstruit la table des modules lies a partir de la liste du jeu.
void rescan(MMU& m) {
    g_active_n = 0;
    uint32_t info = m.read32(kModuleListHead);
    for (int guard = 0; in_mem1(info) && guard < 64; ++guard) {
        const uint32_t id = m.read32(info);
        const RelAotModule* a = find_aot(id);
        if (a && id < kRelMaxId && g_active_n < 32) {
            const uint32_t sections = m.read32(info + 0x10);
            const uint32_t bss_sec = m.read8(info + 0x33);
            const uint32_t bss = bss_sec ? (m.read32(sections + bss_sec * 8) & ~3u) : 0;
            g_rel_delta[id][0] = (int32_t)(info - a->base);
            g_rel_delta[id][1] = bss ? (int32_t)(bss - a->bss) : 0;
            Active& e = g_active[g_active_n++];
            e.lo = info;
            e.hi = info + a->image_size;
            e.aot = a->base;
            e.info = info;
            e.sections = sections;
            e.id = id;
        }
        info = m.read32(info + 4);
    }
}

} // namespace

void rel_aot_register(const RelAotModule* modules, uint32_t count) {
    g_modules = modules;
    g_module_count = count;
}

uint32_t rel_to_aot(GuestCpu& ctx) {
    const uint32_t pc = ctx.pc;
    MMU& m = ctx.mmu;
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < g_active_n; ++i) {
            const Active& e = g_active[i];
            if (pc < e.lo || pc >= e.hi)
                continue;
            // Le module est-il toujours la ? Un autre peut avoir ete charge au meme endroit.
            if (m.read32(e.info) != e.id || m.read32(e.info + 0x10) != e.sections)
                break;
            return pc - e.lo + e.aot;
        }
        rescan(m);
    }
    static int warned;
    if (warned < 16 && NJ_ENV("NJ_RELDBG")) {
        ++warned;
        NJ_PRINTF("[REL] code hors modules recompiles a 0x%08X (lr=0x%08X)\n", pc, ctx.lr);
    }
    return pc;
}

} // namespace nj::runtime
