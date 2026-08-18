#include "debug_menu_skip_source.h"
#include "runtime/log.h"
#include "runtime/game_version.h"

#include "runtime/cpu_context.h"
#include "runtime/env.h"

#include <cstring>
#include <iostream>

namespace nj::runtime::gfx { extern int g_debug_mode; }  // settings_menu.cpp
namespace nj::runtime {
extern GuestCpu* g_guest;
}

namespace nj::runtime::input {

namespace {

constexpr nj::runtime::GameAddr kMessPtrBuff {0x80315A08};  // char* mess_ptr_buff (.sbss, game/eprintf.cpp)
constexpr int kMessPtrNum = 0x300;             // MESS_PTR_NUM
constexpr uint16_t kButtonA = 0x0100;

bool heap_ptr(uint32_t p) { return p >= 0x80000000u && p < 0x81800000u; }

// Une ligne du tampon eprintf contient-elle `needle` ? Entree : x (2), y (2), couleur, w, h,
// puis le texte termine par 0 (EprintfBuffering).
bool buffer_has(MMU& m, const char* needle) {
    const uint32_t tbl = m.read32(kMessPtrBuff);
    if (!heap_ptr(tbl))
        return false;
    const size_t n = std::strlen(needle);
    char line[96];
    for (int i = 0; i < kMessPtrNum; ++i) {
        const uint32_t e = m.read32(tbl + i * 4);
        if (e == 0)
            break;
        if (!heap_ptr(e))
            continue;
        size_t k = 0;
        for (; k + 1 < sizeof(line); ++k) {
            line[k] = (char)m.read8(e + 7 + (uint32_t)k);
            if (line[k] == 0)
                break;
        }
        line[k] = 0;
        if (k >= n && std::strstr(line, needle))
            return true;
    }
    return false;
}

} // namespace

bool DebugMenuSkipSource::enabled() { return NJ_ENV("NJ_DEBUG_HUD") == nullptr && !nj::runtime::gfx::g_debug_mode; }

void DebugMenuSkipSource::poll(PadState pads[4]) {
    if (!g_guest)
        return;
    const auto now = std::chrono::steady_clock::now();
    if (down_) {
        if (now < release_) {
            pads[0].buttons |= kButtonA;
            return;
        }
        down_ = false;
    }
    if (now < next_)
        return;
    // Lecture limitee : quelques fois par seconde suffisent pour un menu qui attend.
    next_ = now + std::chrono::milliseconds(250);
    if (!buffer_has(g_guest->mmu, "DEBUG GAME"))
        return;
    // Le jeu lit le front de A (Joy.trg) : un appui franc, puis on laisse le temps au jeu de
    // sortir du menu avant de reessayer.
    NJ_LOG << "[Release] menu de developpeur du titre : DEBUG GAME valide (nouvelle partie)\n";
    down_ = true;
    release_ = now + std::chrono::milliseconds(150);
    next_ = now + std::chrono::milliseconds(900);
    pads[0].buttons |= kButtonA;
}

} // namespace nj::runtime::input
