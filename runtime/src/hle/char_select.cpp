// Choix du personnage (menu F10 « Character ») : Leon, Ashley, Ada, HUNK, Krauser, Wesker.
//
// Le jeu sait deja tout faire a partir d'un octet, pG->x4FB8 (0 Leon, 1 Ashley, 2 Ada, 3 HUNK,
// 4 Krauser, 5 Wesker ; le menu de debogage du titre le regle, title.cpp) :
//   - ReadPlayerData (read.cpp) charge le modele, les animations, les sons et le module (REL) du
//     personnage quand pG->flags_4FBE bit 0 est leve ;
//   - EmFileTbl_Ada / _Wesker / _Klauser : les fichiers propres a chacun ;
//   - PlayerInit (player.cpp) : son arme de depart et sa vie ;
//   - cItemMgr::set_char (item.cpp) : sa panoplie des Mercenaires ;
//   - la mallette (Sscrn/ss_model.cpp) montre le bon personnage, anime.
//
// Changer en pleine salle est impossible proprement : ReadPlayerData attend le disque en rendant
// la main au planificateur des taches (TaskSleep). On applique donc le choix au chargement de la
// salle suivante : en tete de gameRoomInit (0x8014339C, [hle_wraps]), avant son ReadPlayerData.
//
// L'inventaire de Leon est mis de cote au depart de Leon et rendu a son retour (objets, arme en
// main, vie, costume). Ada en mission et les Mercenaires (flags_54 bits 31 / 30) ne sont pas
// touches : le jeu y choisit lui-meme.

#include "runtime/config.h"
#include "runtime/log.h"
#include "runtime/game_version.h"
#include "runtime/cpu_context.h"
#include "runtime/gfx_settings.h"

#include <cstdint>
#include <cstdio>
#include <vector>

using nj::runtime::GuestCpu;
using GuestFn = void (*)(GuestCpu&);

extern "C" {
void nj_dispatch_one(GuestCpu& cpu);   // main_output.cpp (recompilateur)
}

namespace {

constexpr nj::runtime::GameAddr kPG {0x80314BC8};        // Global* pG
constexpr nj::runtime::GameAddr kItemMgr {0x80271A88};   // cItemMgr ItemMgr
constexpr nj::runtime::GameAddr kFnPlayerInit {0x80057280};
constexpr nj::runtime::GameAddr kFnItemClear {0x8001A614};
constexpr nj::runtime::GameAddr kFnSetChar {0x8001AA94};
constexpr uint32_t kReturn = 0xFFFFFFF0;

// Global (include/global.h)
constexpr uint32_t oFlags54 = 0x54;
constexpr uint32_t oLife = 0x4FA4, oLifeMax = 0x4FA6;
constexpr uint32_t oWep = 0x4FB0;           // wep_no, wep_type, x4FB2, lv, lv_mag, lv_ex (6 octets)
constexpr uint32_t oChar = 0x4FB8, oCostume = 0x4FB9;
constexpr uint32_t oFlags4FBE = 0x4FBE;
constexpr uint32_t oWepOld = 0x4F7C;        // arme a recharger (read.cpp)
// cItemMgr (include/item.h)
constexpr uint32_t oArm = 0x0C, oArmId = 0x10, oItems = 0x14, oNItems = 0x1C;
constexpr uint32_t kItemSize = 0x0E;        // sizeof(ItemWork)

struct LeonSave {
    bool valid = false;
    std::vector<uint8_t> items;
    int32_t arm_slot = -1;
    uint16_t arm_id = 0;
    uint8_t wep[6] = {};
    uint16_t life = 0, life_max = 0;
    uint8_t costume = 0;
} g_leon;

bool heap_ptr(uint32_t p) { return p >= 0x80000000u && p < 0x81800000u; }

void call(GuestCpu& cpu, uint32_t addr) {
    const uint32_t lr = cpu.lr;
    cpu.lr = kReturn;
    cpu.pc = addr;
    const uint32_t msr = cpu.msr;
    cpu.msr &= ~0x8000u;
    nj_dispatch_one(cpu);   // fonction a addr, trouvee par le repartiteur (P3 : pas de nom)
    for (int guard = 0; cpu.pc != kReturn && cpu.alive && guard < 2000000; ++guard)
        nj_dispatch_one(cpu);
    cpu.msr = (cpu.msr & ~0x8000u) | (msr & 0x8000u);
    cpu.lr = lr;
}

void save_leon(GuestCpu& cpu, uint32_t g) {
    auto& m = cpu.mmu;
    const uint32_t items = m.read32(kItemMgr + oItems);
    const int32_t n = (int32_t)m.read32(kItemMgr + oNItems);
    if (!heap_ptr(items) || n <= 0 || n > 256)
        return;
    g_leon.items.resize((size_t)n * kItemSize);
    for (size_t i = 0; i < g_leon.items.size(); ++i)
        g_leon.items[i] = m.read8(items + (uint32_t)i);
    const uint32_t arm = m.read32(kItemMgr + oArm);
    g_leon.arm_slot = heap_ptr(arm) ? (int32_t)((arm - items) / kItemSize) : -1;
    g_leon.arm_id = m.read16(kItemMgr + oArmId);
    for (int i = 0; i < 6; ++i)
        g_leon.wep[i] = m.read8(g + oWep + i);
    g_leon.life = m.read16(g + oLife);
    g_leon.life_max = m.read16(g + oLifeMax);
    g_leon.costume = m.read8(g + oCostume);
    g_leon.valid = true;
}

bool restore_leon(GuestCpu& cpu, uint32_t g) {
    auto& m = cpu.mmu;
    const uint32_t items = m.read32(kItemMgr + oItems);
    const int32_t n = (int32_t)m.read32(kItemMgr + oNItems);
    if (!g_leon.valid || !heap_ptr(items) || (size_t)n * kItemSize != g_leon.items.size())
        return false;
    for (size_t i = 0; i < g_leon.items.size(); ++i)
        m.write8(items + (uint32_t)i, g_leon.items[i]);
    m.write32(kItemMgr + oArm, g_leon.arm_slot >= 0 ? items + (uint32_t)g_leon.arm_slot * kItemSize : 0);
    m.write16(kItemMgr + oArmId, g_leon.arm_id);
    for (int i = 0; i < 6; ++i)
        m.write8(g + oWep + i, g_leon.wep[i]);
    m.write8(g + oWepOld, g_leon.wep[0]);
    m.write16(g + oLife, g_leon.life);
    m.write16(g + oLifeMax, g_leon.life_max);
    m.write8(g + oCostume, g_leon.costume);
    g_leon.valid = false;
    return true;
}

const char* kNames[] = {"Leon", "Ashley", "Ada", "HUNK", "Krauser", "Wesker"};

} // namespace

void nj_char_gameRoomInit(GuestCpu& cpu, GuestFn orig) {
    auto& m = cpu.mmu;
    const uint32_t g = m.read32(kPG);
    const int want = nj::runtime::gfx::g_character;
    // Releve fait sur le build debug (champs de Global, routines) : ailleurs, rien.
    const bool debug_build = nj::runtime::game_version() == nj::runtime::GameVersion::Debug;
    if (debug_build && heap_ptr(g) && want >= 0 && want <= 5) {
        const uint32_t f54 = m.read32(g + oFlags54);
        const int cur = m.read8(g + oChar);
        // Histoire seulement (ni Ada en mission, ni Mercenaires), et pas la paire Leon + Ashley (6).
        // Pendant le passage ou l'histoire fait jouer Ashley (PlSelect(1), r206), on ne remet le
        // personnage choisi que si le joueur vient de le changer dans F10.
        static int last_want = -2;
        const bool just_changed = want != last_want;
        last_want = want;
        if (!(f54 & 0x80000000u) && !(f54 & 0x40000000u) && cur <= 5 && cur != want &&
            (cur != 1 || just_changed)) {
            // Registres du jeu a l'entree de gameRoomInit : gardes, les appels ci-dessous les usent.
            uint32_t gpr[32];
            for (int i = 0; i < 32; ++i)
                gpr[i] = cpu.gpr[i];
            if (cur == 0)
                save_leon(cpu, g);
            m.write8(g + oChar, (uint8_t)want);
            m.write8(g + oCostume, 0);
            call(cpu, kFnPlayerInit);   // arme de depart, vie, flags_4FBE bit 0
            if (!(want == 0 && restore_leon(cpu, g))) {
                cpu.gpr[3] = kItemMgr;
                call(cpu, kFnItemClear);
                if (want != 1) {                      // Ashley n'a rien
                    cpu.gpr[3] = kItemMgr;
                    cpu.gpr[4] = (uint32_t)want;
                    call(cpu, kFnSetChar);
                }
                m.write8(g + oWepOld, m.read8(g + oWep));
            }
            m.write16(g + oFlags4FBE, m.read16(g + oFlags4FBE) | 1);   // ReadPlayerData recharge
            for (int i = 0; i < 32; ++i)
                cpu.gpr[i] = gpr[i];
            NJ_PRINTF("[Personnage] %s -> %s (a ce chargement de salle)\n", kNames[cur], kNames[want]);
            std::fflush(stdout);
        }
    }
    cpu.pc = nj::runtime::ga(0x8014339C);
    orig(cpu);
}
