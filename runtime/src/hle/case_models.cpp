// Modeles de la mallette de l'UHD (F10 « Attache case models ») : place pour un ss_pzzl.dat plus
// grand que celui de la GameCube.
//
// L'ecran de la mallette charge Sscrn.rel, ss_cmmn.dat et ss_pzzl.dat a la suite dans une zone de
// 3 Mo (sscrn.cpp, SS_ARAM_SIZE), puis cree son tas (n° 12) a pzzlOfs + 0xE4000 : ss_pzzl.dat
// ne doit pas depasser 0xE4000 octets. Les modeles de l'UHD, plus detailles, le font grossir
// (tools/re4/uhd_case_models.py) ; le disque virtuel sert le fichier en entier et note le
// depassement (g_case_pzzl_extra) ; ici, a la creation du tas 12, on en recule le debut d'autant.
//
// Seulement dans ce cas-la (sscrn.cpp:466) : le marchand (type 0x10) place son tas apres TOUTES
// les donnees chargees (aramSize + 0x50000), donc deja apres le ss_pzzl plus grand, et le type
// 0x20 le pose sur ss_pzzl lui-meme. Decaler aussi ceux-la retirait la taille deux fois au tas du
// marchand (998 Ko sur GameCube, 717 Ko ici) : plantage a l'ouverture du marchand (30/09).
// Enveloppe de MemCreateHeap (0x801542E0, [hle_wraps]).

#include "runtime/cpu_context.h"
#include "runtime/log.h"
#include "runtime/game_version.h"
#include "runtime/gfx_settings.h"

#include <cstdint>
#include <cstdio>

using nj::runtime::GuestCpu;
using GuestFn = void (*)(GuestCpu&);

namespace {
constexpr nj::runtime::GameAddr kSubScreenType {0x80277F10, (0x2C)};  // SubScreenWk.type (include/sscrn.h)
}

void nj_case_MemCreateHeap(GuestCpu& ctx, GuestFn orig) {
    const uint32_t extra = nj::runtime::gfx::g_case_pzzl_extra;
    if (ctx.gpr[3] == 12 && extra && !(ctx.mmu.read32(kSubScreenType) & 0x30)) {
        const uint32_t start = (ctx.gpr[4] + extra + 31) & ~31u;
        if (start < ctx.gpr[5]) {
            static int n = 0;
            if (n++ < 4)
                NJ_PRINTF("[Mallette UHD] tas 12 : %08X -> %08X (+%u octets)\n", ctx.gpr[4], start, extra);
            ctx.gpr[4] = start;
        }
    }
    ctx.pc = nj::runtime::ga(0x801542E0);
    orig(ctx);
}
