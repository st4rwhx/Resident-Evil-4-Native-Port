// Changer d'arme sans ouvrir la mallette (eventail des favoris, 20/10). Comportement observe a la fermeture de la
// mallette (docs/PLAN_TACTILE_ET_SOURIS_2026-10-20.md) : l'inventaire arme la piece, puis, si l'arme voulue differe de
// celle en main, le jeu relache l'arme, charge la nouvelle et l'initialise. Regles de refus de re4_tweaks (code public) :
// pas pendant un tir ni une recharge, et seulement quand le jeu permettrait d'ouvrir la mallette (subScrCheck).
// Execute dans PADRead, appele par la boucle principale du jeu entre deux images (verifie par le cadre appelant).
#include "runtime/quick_equip.h"
#include "runtime/cpu_context.h"
#include "runtime/dev_tour.h"
#include "runtime/game_call.h"
#include "runtime/game_version.h"
#include "runtime/perf_journal.h"

#include <atomic>
#include <cstdio>

namespace nj::runtime {
namespace {

std::atomic<int> g_req{-1};   // identifiant d'objet demande, -1 = rien

constexpr GameAddr kPG{0x80314BC8}, kPPL{0x803159F4}, kItemMgr{0x80271A88};
constexpr GameAddr kSearch{0x8001DED0}, kArm{0x8001F350}, kWepNo{0x800205BC}, kWepType{0x8002063C};
constexpr GameAddr kSndBlkStop{0x8005D13C}, kSubScrCheck{0x80173CC0};
constexpr GameAddr kWepRelease{0x80178CA0}, kWepLoad{0x80178F54}, kWepInit{0x80178F8C};
constexpr GameAddr kMain{0x80152FC8}, kMainEnd{0x8015333C};

bool heap(uint32_t p) { return p >= 0x80000000u && p < 0x81800000u; }

// Rend 0 si l'arme est en main, sinon la raison du refus (journal).
const char* equip(GuestCpu& ctx, uint16_t id) {
    MMU& m = ctx.mmu;
    // PADRead appele par PadRead, lui-meme appele par main (boucle d'image) : adresse de retour de PadRead dans son cadre
    const uint32_t up = m.read32(ctx.gpr[1]);
    const uint32_t ret_main = heap(up) ? m.read32(up + 4) : 0;
    const uint32_t main0 = kMain, main1 = kMainEnd;
    if (!main0 || !main1 || ret_main < main0 || ret_main >= main1) return "hors boucle principale";
    for (uint32_t f : {(uint32_t)kSearch, (uint32_t)kArm, (uint32_t)kWepNo, (uint32_t)kWepType, (uint32_t)kSndBlkStop,
                       (uint32_t)kSubScrCheck, (uint32_t)kWepRelease, (uint32_t)kWepLoad, (uint32_t)kWepInit})
        if (!f) return "adresse inconnue dans cette version";
    const uint32_t g = m.read32(kPG), pl = m.read32(kPPL), im = kItemMgr;
    if (!heap(g) || !heap(pl)) return "pas en jeu";
    if (m.read8(g + pg(0x4FB8)) == 1) return "Ashley jouable";
    const uint8_t r2 = m.read8(pl + 0xFE);
    if (r2 == 4 || r2 == 2) return "tir ou recharge";
    if (!call_game(ctx, kSubScrCheck, {pl})) return "le jeu refuse la mallette";
    const uint32_t item = call_game(ctx, kSearch, {im, id});
    if (!heap(item)) return "objet absent";
    if (!call_game(ctx, kArm, {im, item})) return "le jeu refuse l'arme";
    const uint32_t arm_id = m.read16(im + 0x10), p_arm = m.read32(im + 0x0C);
    const uint32_t no = call_game(ctx, kWepNo, {arm_id}) & 0xFF, type = call_game(ctx, kWepType, {arm_id}) & 0xFF;
    const uint8_t lv = heap(p_arm) ? (uint8_t)(m.read16(p_arm + 8) >> 13) : 0;
    if (m.read8(g + pg(0x4FB0)) == no && m.read8(g + pg(0x4FB1)) == type && m.read8(g + pg(0x4FB2)) == lv)
        return nullptr;   // deja en main
    call_game(ctx, kSndBlkStop, {2});
    call_game(ctx, kWepRelease, {pl});
    call_game(ctx, kWepLoad, {pl, no, type});
    m.write8(g + pg(0x4FB2), lv);
    call_game(ctx, kWepInit, {pl});
    return nullptr;
}

}  // namespace

void quick_equip_request(uint16_t item_id) { g_req.store(item_id); }

void quick_equip_run(GuestCpu& ctx) {
    int id = g_req.exchange(-1);
    if (const int k = nj::dev::knob("equip", 0); id < 0 && k > 0) {   // essai : knobs.txt equip=<id> (une fois par valeur)
        static int last = 0;
        if (k != last) id = last = k;
    }
    if (id < 0) return;
    const char* why = equip(ctx, (uint16_t)id);
    char b[96];   // journal de perf : present aussi dans le build de diffusion (essais MuMu)
    std::snprintf(b, sizeof b, "equipement rapide objet %d : %s", id, why ? why : "en main");
    nj::perf::event(b);
}

}  // namespace nj::runtime
