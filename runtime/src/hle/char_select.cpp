// Choix du personnage (menu F10 « Character » et Garde-robe) : Leon, Ashley, Ada, HUNK, Krauser, Wesker, Luis.
//
// Le jeu sait deja tout faire a partir d'un octet, pG->x4FB8 (0 Leon, 1 Ashley, 2 Ada, 3 HUNK,
// 4 Krauser, 5 Wesker, 6 Luis ; le menu de debogage du titre le regle, title.cpp) :
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
// Krauser (4) a besoin de plus : ses donnees (0x188000 octets) debordent sur la zone des armes, ses
// armes vont donc dans Game.pBuf (0x70000 octets) que gameInit ne cree QUE si x4FB8 vaut deja 4.
// On pose donc l'octet aussi en tete de gameInit (0x80142F7C, nouvelle partie ou chargement) : le
// jeu cree lui-meme son tampon (un chargement remet ensuite x4FB8 a celui de la sauvegarde, la salle
// suivante refait le changement). Sans tampon (choix fait en pleine partie), Krauser attend la
// prochaine partie. En histoire, readEmData prendrait les ennemis des Mercenaires (EmFileTbl_Klauser,
// EmFileTbl + 0x600 dans les trois versions) : on y recopie la table de l'histoire tant que Krauser
// joue l'histoire, et on rend l'originale avant les Mercenaires.
//
// L'inventaire de Leon est mis de cote au depart de Leon et rendu a son retour (objets, arme en
// main, vie, costume). Ada en mission et les Mercenaires (flags_54 bits 31 / 30) ne sont pas
// touches : le jeu y choisit lui-meme.
//
// Champs de Global : decalages du debug traduits par pg() (USA / Europe : +0xC a partir de 0x4F90).
// Fonctions et variables : ga() ; une adresse non traduite (0) coupe le changement.

#include "runtime/config.h"
#include "runtime/log.h"
#include "runtime/game_version.h"
#include "runtime/cpu_context.h"
#include "runtime/gfx_settings.h"
#include "krauser_mod.h"

#include <cstdint>
#include <cstdio>
#include <vector>

using nj::runtime::GuestCpu;
using nj::runtime::pg;
using GuestFn = void (*)(GuestCpu&);

extern "C" {
void nj_dispatch_one(GuestCpu& cpu);   // main_output.cpp (recompilateur)
}

namespace {

constexpr nj::runtime::GameAddr kPG {0x80314BC8};        // Global* pG
constexpr nj::runtime::GameAddr kItemMgr {0x80271A88};   // cItemMgr ItemMgr
constexpr nj::runtime::GameAddr kGamePBuf {0x802DFF78, 0x18};   // Game.pBuf : armes de Krauser
constexpr nj::runtime::GameAddr kEmFileTbl {0x8026AFE0};        // ReadFile EmFileTbl[64]
constexpr nj::runtime::GameAddr kFnPlayerInit {0x80057280};
constexpr nj::runtime::GameAddr kFnItemClear {0x8001A614};
constexpr nj::runtime::GameAddr kFnSetChar {0x8001AA94};
constexpr uint32_t kReturn = 0xFFFFFFF0;
constexpr uint32_t kEmTblSize = 0x200;      // 64 x ReadFile (8 octets)
constexpr uint32_t kEmTblKlauser = 0x600;   // EmFileTbl_Klauser - EmFileTbl

// Global (include/global.h), decalages du debug
constexpr uint32_t oFlags54 = 0x54;
constexpr uint32_t oWepOld = 0x4F7C;        // arme a recharger (read.cpp), avant 0x4F90
uint32_t oLife() { return pg(0x4FA4); }
uint32_t oLifeMax() { return pg(0x4FA6); }
uint32_t oWep() { return pg(0x4FB0); }      // wep_no, wep_type, x4FB2, lv, lv_mag, lv_ex (6 octets)
uint32_t oChar() { return pg(0x4FB8); }
uint32_t oCostume() { return pg(0x4FB9); }
uint32_t oX4FBA() { return pg(0x4FBA); }
uint32_t oFlags4FBE() { return pg(0x4FBE); }
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

// Table des ennemis de Krauser telle que sur le disque (rendue avant les Mercenaires).
std::vector<uint8_t> g_kl_tbl;
bool g_kl_patched = false;
// Nouvelle partie lancee en Krauser : l'inventaire de depart est celui de Leon (ItemMgr.gameInit),
// la premiere salle pose la panoplie de Krauser. Une partie chargee garde celui de la sauvegarde.
bool g_need_kit = false;

bool heap_ptr(uint32_t p) { return p >= 0x80000000u && p < 0x81800000u; }

bool story(uint32_t f54) { return !(f54 & 0x80000000u) && !(f54 & 0x40000000u); }

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
        g_leon.wep[i] = m.read8(g + oWep() + i);
    g_leon.life = m.read16(g + oLife());
    g_leon.life_max = m.read16(g + oLifeMax());
    g_leon.costume = m.read8(g + oCostume());
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
        m.write8(g + oWep() + i, g_leon.wep[i]);
    m.write8(g + oWepOld, g_leon.wep[0]);
    m.write16(g + oLife(), g_leon.life);
    m.write16(g + oLifeMax(), g_leon.life_max);
    m.write8(g + oCostume(), g_leon.costume);
    g_leon.valid = false;
    return true;
}

// Arme de depart de chaque personnage (numero, type). La version Europe de PlayerInit ne la pose
// plus : on l'ecrit nous-memes apres l'appel, dans les trois versions.
// 0 Leon, 1 Ashley, 2 Ada, 3 HUNK, 4 Krauser, 5 Wesker, 6 Luis
void start_weapon(GuestCpu& cpu, uint32_t g, int who) {
    static const uint8_t no[7] = {2, 2, 1, 0xB, 0x1C, 2, 3};
    static const uint8_t type[7] = {0, 0, 0, 0, 0, 1, 0};
    if (who < 0 || who >= 7) return;
    auto& m = cpu.mmu;
    m.write8(g + oWep(), no[who]);
    m.write8(g + oWep() + 1, type[who]);
    for (int i = 3; i < 6; ++i)
        m.write8(g + oWep() + i, 0);
    m.write8(g + oX4FBA(), 0);
}

// Ennemis de l'histoire pour Krauser en histoire, table d'origine ailleurs.
void em_table(GuestCpu& cpu, bool krauser_story) {
    auto& m = cpu.mmu;
    const uint32_t base = kEmFileTbl;
    if (!heap_ptr(base))
        return;
    const uint32_t kl = base + kEmTblKlauser;
    if (krauser_story && !g_kl_patched) {
        g_kl_tbl.resize(kEmTblSize);
        for (uint32_t i = 0; i < kEmTblSize; ++i) {
            g_kl_tbl[i] = m.read8(kl + i);
            m.write8(kl + i, m.read8(base + i));
        }
        g_kl_patched = true;
    } else if (!krauser_story && g_kl_patched) {
        for (uint32_t i = 0; i < kEmTblSize; ++i)
            m.write8(kl + i, g_kl_tbl[i]);
        g_kl_patched = false;
    }
}

const char* kNames[] = {"Leon", "Ashley", "Ada", "HUNK", "Krauser", "Wesker", "Luis"};

bool addresses_ok() {
    return heap_ptr(kPG) && heap_ptr(kItemMgr) && heap_ptr(kFnPlayerInit) && heap_ptr(kFnItemClear) &&
           heap_ptr(kFnSetChar) && heap_ptr(kGamePBuf);
}

} // namespace

void nj_char_gameInit(GuestCpu& cpu, GuestFn orig) {
    auto& m = cpu.mmu;
    const uint32_t g = m.read32(kPG);
    const int want = nj::runtime::gfx::g_character;
    if (heap_ptr(g))
        NJ_PRINTF("[Personnage] gameInit : choix %d, flags_54 %08X, personnage %d\n", want,
                  (unsigned)m.read32(g + oFlags54), m.read8(g + oChar()));
    if (want == 4 && addresses_ok() && heap_ptr(g) && story(m.read32(g + oFlags54)) && m.read8(g + oChar()) <= 6) {
        // gameInit cree Game.pBuf et lance PlayerInit avec Krauser ; le reste (inventaire, donnees
        // du joueur) se fait au chargement de la salle, comme pour les autres personnages.
        m.write8(g + oChar(), 4);
        m.write8(g + oCostume(), 0);
        g_need_kit = !(m.read32(g + oFlags54) & 0x100u);
        nj::krauser::game_init(cpu);
        NJ_PRINTF("[Personnage] Krauser des le debut de la partie (tampon des armes)\n");
        std::fflush(stdout);
    }
    cpu.pc = nj::runtime::ga(0x80142F7C);
    orig(cpu);
}

void nj_char_gameRoomInit(GuestCpu& cpu, GuestFn orig) {
    auto& m = cpu.mmu;
    const uint32_t g = m.read32(kPG);
    const int want = nj::runtime::gfx::g_character;
    if (addresses_ok() && heap_ptr(g)) {
        const uint32_t f54 = m.read32(g + oFlags54);
        int cur = m.read8(g + oChar());
        // Krauser sans son tampon d'armes (choisi en pleine partie) : il attend la prochaine partie.
        const bool can = want != 4 || heap_ptr(m.read32(kGamePBuf)) || cur == 4;
        // Histoire seulement (ni Ada en mission, ni Mercenaires), et pas la paire Leon + Ashley (6).
        // Pendant le passage ou l'histoire fait jouer Ashley (PlSelect(1), r206), on ne remet le
        // personnage choisi que si le joueur vient de le changer dans F10.
        static int last_want = -2;
        const bool just_changed = want != last_want;
        last_want = want;
        const bool kit = g_need_kit && want == 4 && cur == 4;
        g_need_kit = false;
        if (want >= 0 && want <= 6 && can && story(f54) && cur <= 6 && (cur != want || kit) &&
            (cur != 1 || just_changed)) {
            // Registres du jeu a l'entree de gameRoomInit : gardes, les appels ci-dessous les usent.
            uint32_t gpr[32];
            for (int i = 0; i < 32; ++i)
                gpr[i] = cpu.gpr[i];
            if (cur == 0 && !kit)
                save_leon(cpu, g);
            m.write8(g + oChar(), (uint8_t)want);
            m.write8(g + oCostume(), 0);
            call(cpu, kFnPlayerInit);   // vie, flags_4FBE bit 0
            start_weapon(cpu, g, want);
            if (!(want == 0 && restore_leon(cpu, g))) {
                cpu.gpr[3] = kItemMgr;
                call(cpu, kFnItemClear);
                if (want != 1) {                      // Ashley n'a rien
                    cpu.gpr[3] = kItemMgr;
                    cpu.gpr[4] = (uint32_t)(want == 6 ? 0 : want);
                    call(cpu, kFnSetChar);
                }
                m.write8(g + oWepOld, m.read8(g + oWep()));
            }
            m.write16(g + oFlags4FBE(), m.read16(g + oFlags4FBE()) | 1);   // ReadPlayerData recharge
            for (int i = 0; i < 32; ++i)
                cpu.gpr[i] = gpr[i];
            NJ_PRINTF("[Personnage] %s -> %s (a ce chargement de salle)\n", kNames[cur], kNames[want]);
            std::fflush(stdout);
            cur = want;
        } else if (want == 4 && !can && cur != 4) {
            static bool told = false;
            if (!told) {
                NJ_PRINTF("[Personnage] Krauser : a la prochaine partie (nouvelle ou chargee)\n");
                std::fflush(stdout);
                told = true;
            }
        }
        em_table(cpu, cur == 4 && story(f54));
        nj::krauser::room_init(cpu, cur == 4 && story(f54));
    }
    cpu.pc = nj::runtime::ga(0x8014339C);
    orig(cpu);
}
