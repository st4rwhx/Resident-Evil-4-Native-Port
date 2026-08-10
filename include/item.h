#ifndef ITEM_H
#define ITEM_H

#include "types.h"

// One inventory slot (cItemMgr::pItems[], 0xE bytes).
struct ItemWork {
    u16 id;        // 0x00  item id
    u16 num;       // 0x02  count / bullets
    u8 flags;      // 0x04  bit0 in use
    u8 type;       // 0x05  inventory type (cItemMgr::type selects the visible set)
    union {
        u16 x6;    // 0x06  weapon tune levels, one nibble each: fire << 12 | mag << 8 | speed << 4 | ex (merchant)
                   //       weapon parts (type 9): 1 = attached; files (type 0xA): x6b[0] = countFiles()
        u8 x6b[2];
    };
    u16 x8;        // 0x08  top 3 bits: weapon slot attribute (sscrn: pG->wep_x4FB2), low 13: bullets loaded
                   //       weapon parts (type 9): slot index of the weapon it is attached to (0xFFFF = none)
    s8 x;          // 0x0A  case position (cells * 2) and orientation (puzzle pzlPlayer::save)
    s8 y;          // 0x0B
    s8 orient;     // 0x0C
    u8 board;      // 0x0D  1 = in the case, 0 = on the spare board
};

// cItemMgr::ordering() output (cItemMgr::pOrder[], 8 bytes): the in-use slots holding one item id.
struct ItemOrder {
    ItemWork* item;  // 0x00
    u16 num;         // 0x04  copy of item->num
    u8 pad_6[2];
};

// itemInfo() result (game/item.cpp).
struct ItemInfo {
    u8 x0;
    u8 x1;
    u8 type;       // 0x02  1 weapon, 2 ammo, 3 = weapon with a magazine (sscrn: empty check), 5/0xC treasure, 9 weapon part, 0xA file ...
    u8 x3;         // 0x03  default count when get(id, 0)
    u16 x4;        // 0x04  max count per slot
};

// One saved slot (cItemMgr::save/load, 12 bytes; 0x180 of them after the 4-byte header).
struct ItemSaveWork {
    u16 id;        // 0x00  item id, bit 15 = ItemWork::type 1; 0xFFFF = empty
    u16 x2;        // 0x02  num (weapons/parts: x6)
    u16 x4;        // 0x04  weapons/parts: x8; files: x6b[0]
    u8 pad_6[2];
    s8 x;          // 0x08
    s8 y;          // 0x09
    s8 orient;     // 0x0A
    u8 board;      // 0x0B
};

struct ItemSaveData {
    u16 armId;               // 0x00
    u16 armIdx;              // 0x02  slot index of the equipped weapon, 0xFFFF = none
    ItemSaveWork item[0x180];// 0x04
};                           // 0x1204 = cItemMgr::saveDataSize()

// Inventory manager (game/item.cpp, 0x30 bytes).
class cItemMgr {
public:
    u32* pFlags;                // 0x00  one bit per item id (available()/use(): items usable this frame)
    s32 nFlags;                 // 0x04  words in pFlags (8)
    u16 checkId;                // 0x08  item id use() handed to check(), 0xFFFF = none
    u8 pad_A[2];
    ItemWork* pArm;             // 0x0C  equipped weapon slot (NULL = bare hands)
    u16 armId;                  // 0x10  equipped weapon item id
    s8 x12;                     // 0x12  0 player, 1 sub character heals (sce_at clears it before use())
    u8 type;                    // 0x13  inventory type (num(id) / search count only this type)
    ItemWork* pItems;           // 0x14
    ItemWork* pLast;            // 0x18  slot the last get() filled (puzzle PutInCase copies the piece position into it)
    s32 nItems;                 // 0x1C
    ItemOrder* pOrder;          // 0x20  ordering() result (merchant: sorted slots of one item id)
    s32 nOrder;                 // 0x24  entries in pOrder
    u32 x28;                    // 0x28  (sce_at: number shown with item 0x73; get(0x73, n): mercenaries add time)
    u32 x2C;                    // 0x2C  (sce_at: number shown with item 0x75; get(0x75, n): mercenaries bonus time)

    void clear();
    int set_game(int no);
    int set_ada(int no);
    int set_char(int no);
    int set_stage1(int no);
    int set_stage2(int no);
    int set_stage3(int no);
    int set_range(int no);
    int set_debug(int no);
    int setUp(int no);
    void gameInit();
    void roomInit();
    int init();
    void construct(ItemWork* out, u16 id);  // fill a slot template for item `id` (puzzle PutInCase)
    // get() passes its int id without the `clrlwi 16` the u16 parameter would get: the original
    // build did not narrow it. Same function, int view of the parameter.
    void constructI(ItemWork* out, int id) asm("construct__8cItemMgrP8ItemWorkUs");
    ItemWork* at(int no);       // 0x8001DB5C: slot `no` of pItems, NULL when no >= nItems
    int searchAt(ItemWork* p);  // 0x8001DB80: slot index of `p`, -1 if not in pItems
    int makeItemList(u8* list, int all, s8* pNum, s8* pNum2);
    ItemWork* search(u16 id);   // 0x8001DED0: the in-use slot of this->type holding `id`, NULL if none
    // dump(int)/debugWeapon pass their int id without the `clrlwi 16` (original build). Same function.
    ItemWork* searchI(int id) asm("search__8cItemMgrUs");
    ItemWork* minimumSearch(u16 id);
    void ordering(u16 id);      // 0x8001DFD0: collect the in-use slots holding `id` into pOrder (qsort by order_cmp)
    int get(int id, int num);
    int use(ItemWork* p);       // 0x8001E3BC
    void erase(ItemWork* p);    // remove slot `p` (puzzle removeExtraPiece)
    int dump(int id);           // 0x8001E970: drop item `id`
    int dump(ItemWork* p);
    int dumpAll(ItemWork* p);
    int dumpType(int type);
    int num(int id, u8 type);   // 0x8001EAE4: count of item `id` of the given type (pl_sub: num(0xFE, 0))
    int num(int id);            // 0x8001EB54: count of item `id` of this->type
    int num(ItemWork* p);
    int combine(ItemWork* a, ItemWork* b, int flag);  // merge b into a (puzzle cmbPiece)
    int partsCombine(ItemWork* wep, ItemWork* part);
    int available(u16 id);      // 0x8001F2C4
    void flagclear();           // 0x8001F2E8
    int check(u16 id);
    int arm(ItemWork* p);       // 0x8001F350: equip `p` (NULL: bare hands)
    // equipped weapon (this->xC), objWep: reloadable(x, 0) / reload(x, 0) / trigger(x)
    int reloadable();           // 0x8001F470
    int reloadable(ItemWork* p, int flag);
    int reload();               // 0x8001F5B4
    int reload(ItemWork* p, int flag);
    int trigger();              // 0x8001F7E8
    int trigger(ItemWork* p);
    u16 weaponId(ItemWork* p);
    ItemWork* weaponParts(ItemWork* p, int no);
    int bulletNumTotal(int bulletId);
    u16 bulletNum();            // 0x8001FC20: bulletNumCurrent() of the equipped weapon
    u32 bulletNumCurrent();     // 0x8001FC40
    // bulletNum() returns it without the `clrlwi 16` (original build). Same function, u16 view.
    u16 bulletNumCurrentS() asm("bulletNumCurrent__8cItemMgr");
    // COMPILER-DIFF: 4 (bulletNumTotal assigns num()'s int result to a u16 total without the
    // `clrlwi 16` the int -> u16 conversion gives us). Same function, u16 view.
    u16 numS(int id) asm("num__8cItemMgri");
    int bulletNum(u16 id);
    int bulletNum(ItemWork* p);
    int saveDataSize();
    void save(void* dst);
    void load(void* src);
    int offboardDump(ItemWork* keep);
    void takeOver();
    int countFiles();
    void debugNumDisp(int a);
    void debugWeapon(int id);
};

extern cItemMgr ItemMgr;

// weapon number/type -> item id (0xFFFF when unknown)
u16 WeaponNo2WeaponId(u8 no, u8 type);
// life meter level of a max life `max` (cockpit: lifeLevel(20, pl_life_max, 1200))
int lifeLevel(int levels, s16 max, int base);

extern "C" {
// item id -> weapon number / type (0xFF when unknown), item attributes
u8 WeaponId2WeaponNo(u16 id);
u8 WeaponId2WeaponType(u16 id);
void itemInfo(u16 id, ItemInfo* info);
// int view of itemInfo for cItemMgr::get (no `clrlwi 16` of the int id in the original build)
void itemInfoI(int id, ItemInfo* info) asm("itemInfo");
// weapon item id -> its bullet item id (attr: ItemWork::x8 >> 13), charge count, max tune level per type
u16 WeaponId2BulletId(u16 id, int attr);
u8 WeaponId2ChargeNum(u16 id, int level);
// int view: reloadable() masks the result to u16 (`clrlwi 16`) in the original build
int WeaponId2ChargeNumI(u16 id, int level) asm("WeaponId2ChargeNum");
int WeaponId2MaxLevel(u16 id, int type);
// weapon tune ratios at tune level `level` (examine: power x10 / speed, reload x100 percent)
f32 getPowerRatio(u16 id, s8 level);
f32 getSpeedRatio(u16 id, s8 level);
f32 getReloadRatio(u16 id, s8 level);
f32 getBulletRatio(u16 id, s8 level);
// heal the player (ItemMgr.x12 0) or the sub character (1) by `n`; 0 when already at max
int healing(u16 n);
int addMoney(int n);
u16 bareHand();
int itemCombineCheck(u16 id);
// COMPILER-DIFF: 4 (int view: the original passes a u16 local without the zero-extension, ss_pzzl itemCommandType)
int itemCombineCheckI(int id) asm("itemCombineCheck");
int itemCombine(u16 a, u16 b, u16* result);
int reload_main(ItemWork* wep, ItemWork* ammo, int max);
u8 gld_order(u8 idx);
int gld_cmp(const void* a, const void* b);
int order_cmp(const void* a, const void* b);
}

extern u16 g_item_order[];
extern int g_item_order_num;

#endif
