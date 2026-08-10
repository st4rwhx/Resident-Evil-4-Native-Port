// game/item: the inventory (attache case) manager cItemMgr and the item/weapon tables (D:/Bio4/Prog/item.cpp).
// 68/78 byte-identical (DOL sweep 6, 2026-09-10). Shapes fixed there: save/load loop over `s[i]`
// with the array base in a pointer local declared first (the target's `addi r28,sd,4` base + i*12
// giv + a `mr` copy for the second loop), arm's `goto ok/ng` return layout, reload_main's two clamp
// variables (n, m), weaponParts' `if (p == 0) return 0` + `if (cnt++ == no)` (loop.c moves the
// found block behind the early-return barrier), reload's `goto done` + `int z = ATTR(p) == 0`
// store-flag, combine's single-set `inv`. Open: use (COMPILER-DIFF #6, 11 call tails), get /
// bulletNum (#6 single-insn tails), partsCombine, load (case 1/9 duplicated bodies need the switch
// value in r11), trigger/init/set_stage2/debugNumDisp/combine (register ties).
#include "types.h"
#include "atari.h"
#include "map_obj.h"
#include "global.h"
#include "main.h"
#include "main_mem.h"
#include "db_log.h"
#include "item.h"
#include "puzzle.h"
#include "mercenaries.h"
#include "eprintf.h"

extern "C" {
void qsort(void* base, u32 n, u32 size, int (*cmp)(const void*, const void*));
}

extern f32 WeaponLevelTbl[0x2E][7];     // em_dm_val
extern f32 PlShotFrameTbl[][5];         // pl_class
extern f32 PlReloadSpeedTbl[][3];       // pl_class

// One weapon (22 bytes): item id, x8 attribute, weapon number/type, bullet item id, magazine size per
// exclusive tune level (1..7).
struct WepInfo {
    u16 id;         // 0x00
    u8 attr;        // 0x02  ItemWork::x8 >> 13 this row applies to
    u8 no;          // 0x03  weapon number (pG->wep_no)
    u8 type;        // 0x04  weapon type (pG->wep_type)
    u8 x5;
    u16 bulletId;   // 0x06
    u16 charge[7];  // 0x08
};

// Max tune level per type (fire, magazine, speed, exclusive) of a weapon.
struct WepLevelInfo {
    u16 id;
    u8 lv[4];
};

// Two items that combine into a third.
struct CombInfo {
    u16 a;
    u16 b;
    u16 result;
};

// Item put into the case by the set_*() presets.
struct ItemSet {
    u16 id;
    u16 num;
};

// ItemWork::x6 tune level nibbles
#define LV_EX(p) ((u8) (p)->x6 & 0xF)
#define LV_FIRE_SET(p, v) ((p)->x6 = ((p)->x6 & 0x0FFF) | ((v) << 12))
#define LV_MAG_SET(p, v) ((p)->x6 = ((p)->x6 & 0xF0FF) | ((v) << 8))
#define LV_SPEED_SET(p, v) ((p)->x6 = ((p)->x6 & 0xFF0F) | ((v) << 4))
#define LV_EX_SET(p, v) ((p)->x6 = ((p)->x6 & 0xFFF0) | (v))
// ItemWork::x8: 3-bit attribute and 13-bit bullet count
#define ATTR(p) ((p)->x8 >> 13)
#define BULLET(p) ((p)->x8 & 0x1FFF)

static inline void setBullet(ItemWork* p, u16 n)
{
    p->x8 = (p->x8 & 0xE000) | (n & 0x1FFF);
}

static inline void U16Set(u16& d, u16 v) { d = v; }
static inline void U32Set(u32& d, u32 v) { d = v; }

// slot in use and of inventory type `type`
// (int parameter + cast: cItemMgr::num(int, u8) zero-extends its u8 argument before the loop in the
// original build, which ours only does when the compare is written against a cast int)
static inline int itemUse(ItemWork* p, int type)
{
    if (p->flags & 1) {
        return p->type == (u8) type;
    }
    return 0;
}

static inline int itemEmpty(ItemWork* p)
{
    int empty = !(p->flags & 1);

    if (empty) {
        return 1;
    }
    return 0;
}

#define ITEM_TYPE(id) (itemInfo((id), &info), info.type)
#define ITEM_UNIT(id) (itemInfo((id), &info), info.x3)
#define ITEM_MAX(id) (itemInfo((id), &info), info.x4)

// display order of the item ids (gld_order)
u16 g_item_order[] = {
    0x7F, 0x7E, 0x7D, 0xFE, 0xA9, 0x04, 0x18, 0x00, 0x07, 0x20, 0x46, 0x0E, 0x02, 0x01, 0x38, 0x23,
    0x25, 0x03, 0x21, 0x27, 0x29, 0x2A, 0x37, 0x2C, 0x94, 0x2D, 0x2E, 0x2F, 0x30, 0x34, 0x36, 0x35,
    0x3F, 0x42, 0x43, 0x44, 0x45, 0xAA, 0x08, 0x09, 0x0A, 0x95, 0x97, 0x06, 0x19, 0x1C, 0x12, 0x13,
    0x14, 0x16, 0xA8, 0x15, 0x05, 0x57, 0x58, 0x89, 0x59, 0x8A, 0x5A, 0x5B, 0x5C, 0x5D, 0x5F, 0x60,
    0x61, 0x5E, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0xB8, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1,
    0xC2,
};
int g_item_order_num = sizeof(g_item_order) / sizeof(g_item_order[0]);

const WepInfo wep_info[] = {
    {0x23, 0, 0x02, 0, 1, 0x04, {10, 13, 16, 19, 22, 25, 0}},
    {0x24, 0, 0x02, 1, 1, 0x04, {10, 13, 16, 19, 22, 25, 0}},
    {0x25, 0, 0x03, 0, 1, 0x04, {8, 10, 12, 15, 18, 22, 0}},
    {0x26, 0, 0x03, 2, 1, 0x04, {8, 10, 12, 15, 18, 22, 0}},
    {0x03, 0, 0x11, 0, 0, 0x04, {15, 18, 21, 24, 27, 30, 100}},
    {0x21, 0, 0x01, 0, 1, 0x04, {10, 13, 16, 20, 24, 28, 0}},
    {0x22, 0, 0x01, 1, 1, 0x04, {10, 13, 16, 20, 24, 28, 0}},
    {0x40, 0, 0x01, 0, 1, 0x04, {10, 13, 16, 20, 24, 28, 0}},
    {0x27, 0, 0x04, 0, 1, 0x04, {15, 18, 21, 25, 30, 35, 0}},
    {0x28, 0, 0x04, 1, 1, 0x04, {15, 18, 21, 25, 30, 35, 0}},
    {0x29, 0, 0x05, 0, 0, 0x00, {6, 8, 10, 12, 0, 0, 0}},
    {0x2C, 0, 0x07, 0, 0, 0x18, {6, 8, 10, 12, 15, 18, 0}},
    {0x2D, 0, 0x08, 0, 0, 0x18, {12, 14, 16, 20, 24, 28, 100}},
    {0x2E, 0, 0x09, 0, 1, 0x07, {5, 7, 9, 12, 15, 18, 0}},
    {0x6B, 0, 0x09, 1, 1, 0x07, {5, 7, 9, 12, 15, 18, 0}},
    {0x99, 0, 0x09, 2, 1, 0x07, {5, 7, 9, 12, 15, 18, 0}},
    {0x2F, 0, 0x0A, 0, 1, 0x07, {10, 12, 14, 17, 20, 24, 0}},
    {0x6C, 0, 0x0A, 1, 1, 0x07, {10, 12, 14, 17, 20, 24, 0}},
    {0x51, 0, 0x0A, 2, 1, 0x07, {10, 12, 14, 17, 20, 24, 0}},
    {0x30, 0, 0x0B, 0, 2, 0x20, {30, 50, 100, 150, 200, 250, 0}},
    {0x31, 0, 0x0B, 1, 2, 0x20, {30, 50, 100, 150, 200, 250, 0}},
    {0x32, 0, 0x0B, 2, 2, 0x20, {30, 50, 100, 150, 200, 250, 0}},
    {0x33, 0, 0x0B, 3, 2, 0x20, {30, 50, 100, 150, 200, 250, 0}},
    {0x36, 0, 0x0E, 0, 1, 0x46, {5, 7, 10, 0, 0, 0, 0}},
    {0xAB, 0, 0x0E, 1, 1, 0x46, {5, 7, 10, 0, 0, 0, 0}},
    {0x2A, 0, 0x06, 0, 1, 0x00, {7, 10, 14, 0, 0, 0, 0}},
    {0x2B, 0, 0x06, 1, 1, 0x00, {7, 10, 14, 0, 0, 0, 0}},
    {0x34, 0, 0x0C, 0, 0, 0x6A, {20, 25, 30, 35, 40, 50, 0xFFFF}},
    {0x35, 0, 0x0D, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x17, 0, 0x0D, 1, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x6D, 0, 0x0D, 2, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x37, 0, 0x0F, 0, 0, 0x1A, {3, 4, 5, 6, 8, 10, 0xFFFF}},
    {0x38, 0, 0x10, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x01, 0, 0x13, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x02, 0, 0x16, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x0E, 0, 0x17, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x800, 0, 0x00, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x08, 0, 0x19, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x09, 0, 0x1F, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x0A, 0, 0x20, 0, 0, 0xFF, {1, 1, 1, 0, 0, 0, 0}},
    {0x94, 0, 0x21, 0, 0, 0x18, {7, 9, 11, 13, 15, 17, 0}},
    {0x52, 0, 0x1C, 0, 0, 0x72, {0, 0, 0, 0, 0, 0, 0}},
    {0x3E, 0, 0x0B, 0, 0, 0x20, {30, 50, 100, 150, 200, 250, 0}},
};

const WepLevelInfo wep_level_info[] = {
    {0x23, {6, 3, 3, 6}}, {0x24, {6, 3, 3, 6}}, {0x25, {6, 3, 3, 6}}, {0x26, {6, 3, 3, 6}},
    {0x03, {6, 1, 3, 6}}, {0x21, {6, 3, 3, 6}}, {0x22, {6, 3, 3, 6}}, {0x40, {6, 3, 3, 6}},
    {0x27, {6, 3, 3, 6}}, {0x28, {6, 3, 3, 6}}, {0x29, {6, 1, 3, 4}}, {0x2C, {6, 1, 3, 6}},
    {0x2D, {6, 1, 3, 6}}, {0x94, {6, 1, 3, 6}}, {0x2E, {6, 1, 3, 6}}, {0x6B, {6, 1, 3, 6}},
    {0x99, {6, 1, 3, 6}}, {0x2F, {6, 1, 3, 6}}, {0x6C, {6, 1, 3, 6}}, {0x51, {6, 1, 3, 6}},
    {0x30, {6, 1, 3, 6}}, {0x31, {6, 1, 3, 6}}, {0x32, {6, 1, 3, 6}}, {0x33, {6, 1, 3, 6}},
    {0x3E, {6, 1, 3, 6}}, {0x36, {3, 1, 2, 3}}, {0xAB, {3, 1, 2, 3}}, {0x34, {6, 1, 3, 6}},
    {0x2A, {3, 1, 3, 3}}, {0x2B, {3, 1, 3, 3}}, {0x37, {6, 1, 3, 6}},
};

const CombInfo combination_info[] = {
    {0x21, 0x3F, 0x22}, {0x23, 0x3F, 0x24}, {0x25, 0x42, 0x26}, {0x27, 0x3F, 0x28},
    {0x30, 0x43, 0x32}, {0x31, 0x43, 0x33}, {0x2E, 0x44, 0x6B}, {0x99, 0x44, 0x6B},
    {0x2E, 0xC5, 0x99}, {0x6B, 0xC5, 0x99}, {0x2F, 0x45, 0x6C}, {0x51, 0x45, 0x6C},
    {0x2F, 0xC5, 0x51}, {0x6C, 0xC5, 0x51}, {0x36, 0xAA, 0xAB}, {0x06, 0x06, 0x12},
    {0x12, 0x06, 0x13}, {0x06, 0x19, 0x14}, {0x06, 0x1C, 0x16}, {0x19, 0x1C, 0xA8},
    {0x14, 0x1C, 0x15}, {0x16, 0x19, 0x15}, {0xA8, 0x06, 0x15}, {0x5E, 0x5F, 0x62},
    {0x5E, 0x60, 0x63}, {0x5E, 0x61, 0x64}, {0x63, 0x5F, 0x65}, {0x64, 0x5F, 0x66},
    {0x62, 0x60, 0x65}, {0x64, 0x60, 0x67}, {0x62, 0x61, 0x66}, {0x63, 0x61, 0x67},
    {0x67, 0x5F, 0x68}, {0x66, 0x60, 0x68}, {0x65, 0x61, 0x68}, {0x9A, 0x9B, 0x9D},
    {0x9A, 0x9C, 0x9E}, {0x9D, 0x9C, 0x9F}, {0x9E, 0x9B, 0x9F}, {0xB8, 0xB9, 0xBC},
    {0xB8, 0xBA, 0xBD}, {0xB8, 0xBB, 0xBE}, {0xBD, 0xB9, 0xBF}, {0xBE, 0xB9, 0xC0},
    {0xBC, 0xBA, 0xBF}, {0xBE, 0xBA, 0xC1}, {0xBC, 0xBB, 0xC0}, {0xBD, 0xBB, 0xC1},
    {0xC1, 0xB9, 0xC2}, {0xC0, 0xBA, 0xC2}, {0xBF, 0xBB, 0xC2}, {0xC6, 0xC7, 0xCA},
    {0xC6, 0xC8, 0xCB}, {0xC6, 0xC9, 0xCC}, {0xCB, 0xC7, 0xCD}, {0xCC, 0xC7, 0xCE},
    {0xCA, 0xC8, 0xCD}, {0xCC, 0xC8, 0xCF}, {0xCA, 0xC9, 0xCE}, {0xCB, 0xC9, 0xCF},
    {0xCF, 0xC7, 0xD0}, {0xCE, 0xC8, 0xD0}, {0xCD, 0xC9, 0xD0}, {0xD1, 0xD2, 0xD5},
    {0xD1, 0xD3, 0xD6}, {0xD1, 0xD4, 0xD7}, {0xD6, 0xD2, 0xD8}, {0xD7, 0xD2, 0xD9},
    {0xD5, 0xD3, 0xD8}, {0xD7, 0xD3, 0xDA}, {0xD5, 0xD4, 0xD9}, {0xD6, 0xD4, 0xDA},
    {0xDA, 0xD2, 0xDB}, {0xD9, 0xD3, 0xDB}, {0xD8, 0xD4, 0xDB}, {0xA4, 0xA5, 0xA6},
    {0x3A, 0x69, 0x7A},
};

cItemMgr ItemMgr;

int healing(u16 n)
{
    if (ItemMgr.x12 == 0) {
        if ((s16) pG->pl_life < (s16) pG->pl_life_max) {
            U16Set(pG->pl_life, n + pG->pl_life);
            if ((s16) pG->pl_life > (s16) pG->pl_life_max) {
                pG->pl_life = pG->pl_life_max;
            }
            return 1;
        }
    } else if (ItemMgr.x12 == 1) {
        if ((s16) pG->sub_life < (s16) pG->sub_life_max) {
            U16Set(pG->sub_life, n + pG->sub_life);
            if ((s16) pG->sub_life > (s16) pG->sub_life_max) {
                pG->sub_life = pG->sub_life_max;
            }
            return 1;
        }
    }
    return 0;
}

int lifeLevel(int levels, s16 max, int base)
{
    return (int) ((f32) (levels * (max - base)) / (f32) base + 0.5f);
}

f32 getPowerRatio(u16 id, s8 level)
{
    ItemInfo info;
    f32 ret;

    if (ITEM_TYPE(id) != 1) {
        return 0.0f;
    }
    if (id == 0x36) {
        switch (level) {
        case 1:
            ret = 2.0f;
            break;
        case 2:
            ret = 4.0f;
            break;
        case 3:
            ret = 6.0f;
            break;
        default:
            ret = 0.0f;
            break;
        }
        return ret;
    }
    return WeaponLevelTbl[WeaponId2WeaponNo(id)][level - 1] / WeaponLevelTbl[2][0];
}

f32 getSpeedRatio(u16 id, s8 level)
{
    ItemInfo info;

    if (ITEM_TYPE(id) != 1) {
        return 0.0f;
    }
    return PlShotFrameTbl[WeaponId2WeaponNo(id)][level - 1] / 30.0f;
}

f32 getReloadRatio(u16 id, s8 level)
{
    ItemInfo info;

    if (ITEM_TYPE(id) != 1) {
        return 0.0f;
    }
    return PlReloadSpeedTbl[WeaponId2WeaponNo(id)][level - 1] / 30.0f;
}

f32 getBulletRatio(u16 id, s8 level)
{
    ItemInfo info;

    if (ITEM_TYPE(id) != 1) {
        return 0.0f;
    }
    return (f32) WeaponId2ChargeNum(id, level);
}

void cItemMgr::clear()
{
    ItemWork* p = pItems;
    int i;

    for (i = 0; i < nItems; i++, p++) {
        p->flags = 0;
    }
    flagclear();
    checkId = 0xFFFF;
}

int cItemMgr::set_game(int no)
{
    dump(0x7C);
    dump(0x7D);
    dump(0x7E);
    dump(0x7F);
    get(0x7C, 0);
    get(0x23, 1);
    {
        ItemWork* p = pLast;
        p->x = 2;
        p->y = 1;
        p->orient = 0;
        p->board = 1;
        arm(p);
    }
    get(0x04, 20);
    {
        ItemWork* p = pLast;
        p->x = 7;
        p->y = 0;
        p->orient = 0;
        p->board = 1;
    }
    get(0x05, 1);
    {
        ItemWork* p = pLast;
        p->x = 6;
        p->y = 3;
        p->orient = 0;
        p->board = 1;
    }
    if (no != 0) {
        get(0x30, 0);
        {
            ItemWork* p = pLast;
            p->x = 12;
            p->y = 1;
            p->orient = 0;
            p->board = 1;
        }
        get(0x20, 0);
        {
            ItemWork* p = pLast;
            p->num = 100;
            p->x = 17;
            p->y = 0;
            p->orient = 0;
            p->board = 1;
        }
        get(0x20, 0);
        {
            ItemWork* p = pLast;
            p->num = 100;
            p->x = 17;
            p->y = 2;
            p->orient = 0;
            p->board = 1;
        }
        get(0x20, 0);
        {
            ItemWork* p = pLast;
            p->num = 100;
            p->x = 17;
            p->y = 4;
            p->orient = 0;
            p->board = 1;
        }
        get(0x20, 0);
        {
            ItemWork* p = pLast;
            p->num = 100;
            p->x = 13;
            p->y = 4;
            p->orient = 0;
            p->board = 1;
        }
        get(0x20, 0);
        {
            ItemWork* p = pLast;
            p->num = 100;
            p->x = 9;
            p->y = 4;
            p->orient = 0;
            p->board = 1;
        }
    }
    return 0;
}

#define PUT_TABLE(tbl, flag)                                              \
    for (i = 0; i < (int) (sizeof(tbl) / sizeof(tbl[0])); i++) {          \
        PutInCase(tbl[i].id, tbl[i].num, flag);                           \
    }

#define LV_SET(p, f, m, sp, e)                                                                             \
    LV_EX_SET(p, e);                                                                                      \
    LV_FIRE_SET(p, f);                                                                                    \
    LV_MAG_SET(p, m);                                                                                     \
    LV_SPEED_SET(p, sp)
#define CHARGE(p) setBullet(p, WeaponId2ChargeNum((p)->id, LV_EX(p) + 1))

int cItemMgr::set_ada(int no)
{
    if (no == 2) {
        ItemWork* p;
        u16 on;
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x21, 1}, {0x30, 1}, {0x2F, 1}, {0x45, 1}, {0x04, 30}, {0x20, 50}, {0x07, 5}, {0x01, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 0);
        p = search(0x21);
        on = 1;
        LV_SET(p, 5, 2, 1, 3);
        CHARGE(p);
        p = search(0x30);
        LV_SET(p, 5, 0, 1, 2);
        CHARGE(p);
        p = search(0x2F);
        LV_SET(p, 5, 0, 1, 1);
        CHARGE(p);
        search(0x45)->x8 = searchAt(p);
        search(0x45)->x6 = on;
        arm(search(0x21));
    }
    return 0;
}

int cItemMgr::set_char(int no)
{
    ItemWork* p;   // one function-scope pointer for every case (case 3 gets r31)

    switch (no) {
    case 0: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x27, 1}, {0x94, 1}, {0x04, 30}, {0x18, 10}, {0x05, 1}};
        PUT_TABLE(tbl, 0);
        p = search(0x27);
        LV_SET(p, 4, 1, 1, 2);
        CHARGE(p);
        p = search(0x94);
        LV_SET(p, 4, 0, 2, 3);
        CHARGE(p);
        arm(search(0x27));
        break;
    }
    case 2: {
        u16 on;
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x21, 1}, {0x30, 1}, {0x2F, 1}, {0x45, 1}, {0x04, 30}, {0x20, 100},
                         {0x07, 5}, {0x02, 1}, {0x02, 1}, {0x02, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 0);
        p = search(0x21);
        on = 1;
        LV_SET(p, 6, 2, 1, 3);
        CHARGE(p);
        p = search(0x30);
        LV_SET(p, 4, 0, 1, 2);
        CHARGE(p);
        p = search(0x2F);
        LV_SET(p, 5, 1, 1, 1);
        CHARGE(p);
        search(0x45)->x8 = searchAt(p);
        search(0x45)->x6 = on;
        arm(search(0x21));
        break;
    }
    case 3: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x3E, 1}, {0x20, 50}, {0x01, 1}, {0x01, 1}, {0x01, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 0);
        p = search(0x3E);
        LV_SET(p, 4, 0, 1, 2);
        CHARGE(p);
        arm(search(0x3E));
        break;
    }
    case 4: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x52, 1}, {0x72, 20}, {0x72, 10}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 0);
        arm(search(0x52));
        break;
    }
    case 5: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x23, 1}, {0x2A, 1}, {0x2F, 1}, {0x3F, 1}, {0x01, 1}, {0x01, 1}, {0x01, 1},
                         {0x01, 1}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1}, {0x02, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 0);
        p = search(0x23);
        LV_SET(p, 6, 2, 2, 5);
        CHARGE(p);
        search(0x3F)->x8 = searchAt(p);
        search(0x3F)->x6 = 1;
        p = search(0x2A);
        LV_SET(p, 1, 0, 1, 1);
        CHARGE(p);
        p = search(0x2F);
        LV_SET(p, 5, 1, 1, 5);
        setBullet(p, WeaponId2ChargeNum(0x2F, LV_EX(p) + 1));
        arm(search(0x23));
        break;
    }
    }
    return 0;
}

int cItemMgr::set_stage1(int no)
{
    int ret = 0;

    switch (no) {
    case 0: {
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x23, 1}, {0x2C, 1}, {0x2E, 1}, {0x30, 1}, {0x04, 50}, {0x18, 10}, {0x07, 10},
                         {0x20, 100}, {0x01, 1}, {0x02, 1}, {0x0E, 1}, {0x15, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 1);
        search(0x01)->num = 5;
        search(0x02)->num = 5;
        search(0x0E)->num = 5;
        arm(ItemMgr.search(0x23));
        pG->x4F98 = 0;
        break;
    }
    case 1: {
        ItemWork* p;
        u16 on;
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x25, 1}, {0x2C, 1}, {0x2E, 1}, {0x30, 1}, {0x44, 1}, {0x04, 50}, {0x18, 10}, {0x07, 10},
                         {0x20, 100}, {0x01, 1}, {0x02, 1}, {0x0E, 1}, {0x15, 1}, {0x15, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 1);
        p = search(0x2C);
        on = 1;
        LV_FIRE_SET(p, 2);
        LV_EX_SET(p, 1);
        setBullet(p, WeaponId2ChargeNum(0x2C, 2));
        p = search(0x2E);
        LV_EX_SET(p, 2);
        setBullet(p, WeaponId2ChargeNum(0x2E, 3));
        search(0x44)->x8 = searchAt(p);
        search(0x44)->x6 = on;
        search(0x30);
        search(0x25);
        search(0x01)->num = 5;
        search(0x02)->num = 5;
        search(0x0E)->num = 5;
        arm(ItemMgr.search(0x25));
        pG->x4F98 = 10000;
        break;
    }
    }
    return ret;
}

int cItemMgr::set_stage2(int no)
{
    int ret = 0;
    ItemWork* p;   // function-scope (case 3 takes r31)

    switch (no) {
    case 0: {
        u16 on;
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x25, 1}, {0x30, 1}, {0x2C, 1}, {0x2E, 1}, {0x04, 50}, {0x20, 100}, {0x18, 10}, {0x07, 10},
                         {0x01, 1}, {0x02, 1}, {0x0E, 1}, {0x06, 1}, {0x14, 1}, {0x43, 1}, {0x44, 1}};
        PUT_TABLE(tbl, 1);
        p = search(0x25);
        on = 1;
        LV_SET(p, 1, 1, 1, 1);
        setBullet(p, WeaponId2ChargeNum(0x25, 2));
        p = search(0x30);
        LV_SET(p, 2, 0, 2, 2);
        setBullet(p, WeaponId2ChargeNum(0x30, 3));
        search(0x43)->x8 = searchAt(p);
        search(0x43)->x6 = on;
        p = search(0x2C);
        LV_SET(p, 2, 0, 1, 2);
        setBullet(p, WeaponId2ChargeNum(0x2C, 3));
        p = search(0x2E);
        LV_SET(p, 2, 0, 1, 2);
        setBullet(p, WeaponId2ChargeNum(0x2E, 3));
        search(0x44)->x8 = searchAt(p);
        search(0x44)->x6 = on;
        arm(ItemMgr.search(0x25));
        pG->x4F98 = 40000;
        break;
    }
    case 1: {
        int i;

        ret = 2;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7E, 0);
        ItemSet tbl[] = {{0x27, 1}, {0x94, 1}, {0x2F, 1}, {0x30, 1}, {0x29, 1}, {0x45, 1}, {0x04, 50}, {0x18, 10},
                         {0x07, 10}, {0x20, 100}, {0x00, 10}, {0x01, 1}, {0x02, 1}, {0x0E, 1}, {0x05, 1}, {0x06, 1},
                         {0x06, 1}, {0x15, 1}, {0x15, 1}, {0x15, 1}, {0x15, 1}, {0x15, 1}, {0x15, 1}};
        PUT_TABLE(tbl, 2);
        p = search(0x30);
        LV_SET(p, 4, 0, 2, 3);
        setBullet(p, WeaponId2ChargeNum(0x30, 4));
        p = search(0x27);
        LV_SET(p, 3, 1, 1, 3);
        p = search(0x2F);
        LV_SET(p, 3, 0, 1, 3);
        p = search(0x94);
        LV_SET(p, 3, 0, 1, 3);
        setBullet(p, WeaponId2ChargeNum(0x94, 4));
        p = search(0x29);
        LV_SET(p, 2, 0, 1, 2);
        search(0x01)->num = 2;
        search(0x02)->num = 2;
        search(0x0E)->num = 2;
        arm(ItemMgr.search(0x27));
        pG->x4F98 = 20000;
        break;
    }
    case 2: {
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x2C, 1}, {0x30, 1}, {0x01, 1}, {0x20, 100}, {0x18, 10}, {0x18, 10}};
        PUT_TABLE(tbl, 1);
        p = search(0x30);
        LV_EX_SET(p, 1);
        p = search(0x2C);
        LV_SET(p, 0, 0, 0, 4);
        CHARGE(p);
        arm(ItemMgr.search(0x2C));
        break;
    }
    case 3: {
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x23, 1}, {0x2E, 1}, {0x01, 1}, {0x04, 50}, {0x07, 10}};
        PUT_TABLE(tbl, 1);
        p = search(0x2E);
        LV_SET(p, 0, 0, 0, 4);
        CHARGE(p);
        arm(ItemMgr.search(0x23));
        break;
    }
    }
    return ret;
}

int cItemMgr::set_stage3(int no)
{
    int ret = 0;

    switch (no) {
    case 0: {
        ItemWork* p;
        u16 on;
        int i;

        ret = 2;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7E, 0);
        ItemSet tbl[] = {{0x27, 1}, {0x30, 1}, {0x2D, 1}, {0x29, 1}, {0x2F, 1}, {0x04, 50}, {0x20, 100},
                         {0x18, 10}, {0x00, 10}, {0x07, 10}, {0x01, 1}, {0x43, 1}, {0x45, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 2);
        p = search(0x27);
        on = 1;
        LV_SET(p, 3, 1, 2, 3);
        setBullet(p, WeaponId2ChargeNum(0x27, LV_EX(p) + 1));
        p = search(0x30);
        LV_SET(p, 5, 2, 2, 5);
        setBullet(p, WeaponId2ChargeNum(0x30, LV_EX(p) + 1));
        search(0x43)->x8 = searchAt(p);
        search(0x43)->x6 = on;
        p = search(0x2D);
        LV_SET(p, 2, 0, 1, 2);
        setBullet(p, WeaponId2ChargeNum(0x2D, LV_EX(p) + 1));
        p = search(0x29);
        LV_SET(p, 4, 0, 2, 2);
        setBullet(p, WeaponId2ChargeNum(0x29, LV_EX(p) + 1));
        p = search(0x2F);
        LV_SET(p, 4, 0, 2, 4);
        setBullet(p, WeaponId2ChargeNum(0x2F, LV_EX(p) + 1));
        search(0x45)->x8 = searchAt(p);
        search(0x45)->x6 = on;
        arm(ItemMgr.search(0x27));
        pG->x4F98 = 40000;
        break;
    }
    case 1: {
        ItemWork* p;
        u16 on;
        int i;

        ret = 2;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7E, 0);
        ItemSet tbl[] = {{0x27, 1}, {0x30, 1}, {0x2D, 1}, {0x29, 1}, {0x2F, 1}, {0x04, 50}, {0x20, 100}, {0x18, 10},
                         {0x00, 10}, {0x07, 10}, {0x01, 1}, {0x43, 1}, {0x45, 1}, {0xC5, 1}, {0x05, 1}};
        PUT_TABLE(tbl, 2);
        p = search(0x27);
        on = 1;
        LV_SET(p, 3, 1, 2, 3);
        setBullet(p, WeaponId2ChargeNum(0x27, LV_EX(p) + 1));
        p = search(0x30);
        LV_SET(p, 5, 2, 2, 5);
        setBullet(p, WeaponId2ChargeNum(0x30, LV_EX(p) + 1));
        search(0x43)->x8 = searchAt(p);
        search(0x43)->x6 = on;
        p = search(0x2D);
        LV_SET(p, 2, 0, 1, 2);
        setBullet(p, WeaponId2ChargeNum(0x2D, LV_EX(p) + 1));
        p = search(0x29);
        LV_SET(p, 4, 0, 2, 2);
        setBullet(p, WeaponId2ChargeNum(0x29, LV_EX(p) + 1));
        p = search(0x2F);
        LV_SET(p, 4, 0, 2, 4);
        setBullet(p, WeaponId2ChargeNum(0x2F, LV_EX(p) + 1));
        search(0xC5)->x8 = searchAt(p);
        search(0xC5)->x6 = on;
        arm(ItemMgr.search(0x27));
        pG->x4F98 = 40000;
        break;
    }
    }
    return ret;
}

int cItemMgr::set_range(int no)
{
    int ret = 0;

    switch (no) {
    case 0: {
        ItemWork* p;
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x2C, 1}, {0x30, 1}, {0x01, 1}, {0x20, 100}, {0x18, 10}, {0x18, 10}};
        PUT_TABLE(tbl, 1);
        p = search(0x2C);
        LV_SPEED_SET(p, 1);
        LV_EX_SET(p, 3);
        setBullet(p, WeaponId2ChargeNum(0x2C, LV_EX(p) + 1));
        p = search(0x30);
        LV_EX_SET(p, 1);
        setBullet(p, WeaponId2ChargeNum(0x30, LV_EX(p) + 1));
        arm(ItemMgr.search(0x2C));
        break;
    }
    case 1: {
        ItemWork* p;
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x23, 1}, {0x2E, 1}, {0x01, 1}, {0x04, 50}, {0x07, 10}};
        PUT_TABLE(tbl, 1);
        p = search(0x23);
        LV_MAG_SET(p, 2);
        LV_SPEED_SET(p, 1);
        LV_EX_SET(p, 2);
        setBullet(p, WeaponId2ChargeNum(0x23, LV_EX(p) + 1));
        p = search(0x2E);
        LV_SPEED_SET(p, 1);
        LV_EX_SET(p, 3);
        setBullet(p, WeaponId2ChargeNum(0x2E, LV_EX(p) + 1));
        arm(ItemMgr.search(0x23));
        break;
    }
    }
    return ret;
}

int cItemMgr::set_debug(int no)
{
    int ret = 0;

    switch (no) {
    case 0: {
        int i;

        ret = set_game(0);
        ItemSet tbl[] = {{0x05, 1}, {0x05, 1}, {0x08, 1}, {0x09, 1}, {0x0A, 1}, {0x95, 1}, {0x97, 1}, {0x06, 1},
                         {0x19, 1}, {0x1C, 1}, {0x14, 1}, {0x16, 1}, {0x15, 1}, {0x12, 1}, {0x13, 1}};
        PUT_TABLE(tbl, ret);
        arm(ItemMgr.search(0x23));
        break;
    }
    case 1: {
        int i;

        ret = 3;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7F, 0);
        ItemSet tbl[] = {{0x25, 1}, {0x30, 1}, {0x2E, 1}, {0x2C, 1}, {0x36, 1}, {0x35, 1}, {0x44, 1}, {0xAA, 1},
                         {0x42, 1}, {0x43, 1}, {0x01, 1}, {0x02, 1}, {0x0E, 1}, {0x04, 50}, {0x46, 5}, {0x05, 1}};
        PUT_TABLE(tbl, 3);
        search(0x01)->num = 5;
        search(0x02)->num = 5;
        search(0x0E)->num = 5;
        arm(ItemMgr.search(0x25));
        break;
    }
    case 2: {
        int i;

        ret = 2;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7E, 0);
        ItemSet tbl[] = {{0x2D, 1}, {0x2F, 1}, {0x30, 1}, {0x27, 1}, {0x29, 1}, {0x23, 1}, {0x05, 1}, {0x05, 1},
                         {0x01, 1}, {0x02, 1}, {0x0E, 1}, {0x18, 10}, {0x07, 10}, {0x20, 100}, {0x04, 50}, {0x00, 10}};
        PUT_TABLE(tbl, 2);
        search(0x01)->num = 5;
        search(0x02)->num = 5;
        search(0x0E)->num = 5;
        arm(ItemMgr.search(0x23));
        break;
    }
    case 3: {
        int i;

        ret = 1;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7D, 0);
        ItemSet tbl[] = {{0x23, 1}, {0x03, 1}, {0x05, 1}, {0x2A, 1}, {0x21, 1}, {0x25, 1}, {0x27, 1},
                         {0x29, 1}, {0x37, 1}, {0x3F, 1}, {0x04, 50}, {0x00, 10}, {0x1A, 10}};
        PUT_TABLE(tbl, 1);
        arm(ItemMgr.search(0x23));
        break;
    }
    case 4: {
        int i;

        ret = 3;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7F, 0);
        ItemSet tbl[] = {{0x30, 1}, {0x34, 1}, {0x2C, 1}, {0x2D, 1}, {0x94, 1},
                         {0x2E, 1}, {0x2F, 1}, {0x20, 100}, {0x18, 10}, {0x07, 10}};
        PUT_TABLE(tbl, 3);
        arm(ItemMgr.search(0x23));
        break;
    }
    case 5: {
        ItemWork* p;
        u16 on;
        int i;

        ret = 3;
        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7F, 0);
        ItemSet tbl[] = {{0x36, 1}, {0x17, 1}, {0x35, 1}, {0x35, 1}, {0x6D, 1}, {0xAA, 1}, {0x46, 5}, {0x46, 5},
                         {0x46, 5}, {0x46, 5}, {0x46, 5}, {0x46, 5}, {0x01, 1}, {0x01, 1}, {0x01, 1}, {0x02, 1},
                         {0x02, 1}, {0x02, 1}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1},
                         {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}};
        PUT_TABLE(tbl, 3);
        p = search(0x36);
        on = 1;
        LV_SET(p, 2, 0, 1, 2);
        setBullet(p, WeaponId2ChargeNum(0x36, LV_EX(p) + 1));
        search(0xAA)->x8 = searchAt(p);
        search(0xAA)->x6 = on;
        arm(ItemMgr.search(0x36));
        break;
    }
    case 6: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x04, 50}, {0x18, 15}, {0x20, 100}, {0x07, 10}, {0x00, 10}, {0x04, 50}, {0x18, 15}, {0x20, 100},
                         {0x07, 10}, {0x00, 10}, {0x04, 50}, {0x18, 15}, {0x20, 100}, {0x07, 10}, {0x00, 10}, {0x04, 50},
                         {0x18, 15}, {0x20, 100}, {0x07, 10}, {0x00, 10}, {0x04, 50}, {0x18, 15}, {0x20, 100}, {0x07, 10},
                         {0x00, 10}, {0x04, 50}, {0x18, 15}, {0x20, 100}, {0x07, 10}, {0x00, 10}};
        PUT_TABLE(tbl, 0);
        arm(0);
        break;
    }
    case 7: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x05, 1}, {0x15, 1}, {0xA8, 1}, {0x16, 1}, {0x14, 1}, {0x13, 1}, {0x12, 1}, {0x1C, 1},
                         {0x19, 1}, {0x06, 1}, {0x05, 1}, {0x15, 1}, {0xA8, 1}, {0x16, 1}, {0x14, 1}, {0x13, 1},
                         {0x12, 1}, {0x1C, 1}, {0x19, 1}, {0x06, 1}, {0x05, 1}, {0x15, 1}, {0xA8, 1}, {0x16, 1},
                         {0x14, 1}, {0x13, 1}, {0x12, 1}, {0x1C, 1}, {0x19, 1}, {0x06, 1}};
        PUT_TABLE(tbl, 0);
        arm(0);
        break;
    }
    case 8: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x01, 1}, {0x01, 1}, {0x01, 1}, {0x01, 1}, {0x01, 1}, {0x01, 1}, {0x01, 1}, {0x01, 1},
                         {0x01, 1}, {0x01, 1}, {0x02, 1}, {0x02, 1}, {0x02, 1}, {0x02, 1}, {0x02, 1}, {0x02, 1},
                         {0x02, 1}, {0x02, 1}, {0x02, 1}, {0x02, 1}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1},
                         {0x0E, 1}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1}, {0x0E, 1}};
        PUT_TABLE(tbl, 0);
        arm(0);
        break;
    }
    case 9: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1},
                         {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1},
                         {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x08, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1},
                         {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1},
                         {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1}, {0x09, 1},
                         {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1},
                         {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1},
                         {0x0A, 1}, {0x0A, 1}, {0x0A, 1}, {0x0A, 1}};
        PUT_TABLE(tbl, 0);
        arm(0);
        break;
    }
    case 10: {
        int i;

        dump(0x7C);
        dump(0x7D);
        dump(0x7E);
        dump(0x7F);
        get(0x7C, 0);
        ItemSet tbl[] = {{0x97, 1}, {0x97, 1}, {0x97, 1}, {0x95, 1}, {0x95, 1}, {0x95, 1},
                         {0x95, 1}, {0x95, 1}, {0x95, 1}, {0x95, 1}, {0x95, 1}};
        PUT_TABLE(tbl, 0);
        arm(0);
        break;
    }
    }
    return ret;
}

int cItemMgr::setUp(int no)
{
    int ret = 0;

    switch (no) {
    case 0:
        ret = set_game(0);
        break;
    case 1:
        ret = set_game(1);
        break;
    case 2:
        ret = set_stage1(0);
        break;
    case 3:
        ret = set_stage1(1);
        break;
    case 4:
        ret = set_stage2(0);
        break;
    case 5:
        ret = set_stage2(1);
        break;
    case 6:
        ret = set_stage3(0);
        break;
    case 7:
        ret = set_stage3(1);
        break;
    case 8:
        ret = set_range(0);
        break;
    case 9:
        ret = set_range(1);
        break;
    case 10:
        ret = set_debug(0);
        break;
    case 11:
        ret = set_debug(1);
        break;
    case 12:
        ret = set_debug(2);
        break;
    case 13:
        ret = set_debug(3);
        break;
    case 14:
        ret = set_debug(4);
        break;
    case 15:
        ret = set_debug(5);
        break;
    case 16:
        ret = set_debug(6);
        break;
    case 17:
        ret = set_debug(7);
        break;
    case 18:
        ret = set_debug(8);
        break;
    case 19:
        ret = set_debug(9);
        break;
    case 20:
        ret = set_debug(10);
        break;
    case 21:
        ret = set_char(0);
        break;
    case 22:
        ret = set_char(2);
        break;
    }
    return ret;
}

static inline int flagNeg(u32& f)
{
    return (s32) f < 0;
}

static inline u32 chkFlag(u32& f, u32 b)
{
    return f & b;
}

void cItemMgr::gameInit()
{
    clear();
    roomInit();
    if (!flagNeg(pG->flags_54) && !chkFlag(pG->flags_54, 0x40000000)) {
        if (pG->x4FB8 == 1) {
            type = 0;
        }
        set_game(0);
        pG->x832C = 0;
        pG->x4F98 = 0;
        get(0xAC, 1);
        get(0xAD, 1);
        if (chkFlag(pG->flags_6C, 0x00800000) || chkFlag(pG->flags_6C, 0x00040000)) {
            get(0xAE, 1);
            get(0xAF, 1);
            get(0xB0, 1);
            get(0xB1, 1);
            get(0xB2, 1);
            get(0xB3, 1);
            get(0xB4, 1);
            get(0xB5, 1);
            get(0xB6, 1);
            get(0xB7, 1);
        }
        if (pG->flags_6C & 0x00040000) {
            get(0x48, 1);
            get(0x49, 1);
            get(0x4A, 1);
            get(0x4B, 1);
            get(0x4C, 1);
            get(0x4D, 1);
            get(0x4E, 1);
            get(0x4F, 1);
            get(0x50, 1);
            get(0xF4, 1);
            if (pG->flags_6C & 0x00020000) {
                get(0xF5, 1);
                get(0xF6, 1);
                get(0xF7, 1);
                get(0xF8, 1);
                get(0xF9, 1);
                get(0xFA, 1);
                get(0xFB, 1);
                get(0xFC, 1);
                get(0xFD, 1);
            }
        }
        if (pG->x4FB8 == 1) {
            type = 1;
        }
    } else {
        if ((s32) pG->flags_54 < 0) {
            set_ada(2);
        } else if (pG->flags_54 & 0x40000000) {
            set_char(pG->x4FB8);
        }
    }
}

void cItemMgr::roomInit()
{
    flagclear();
    if (pG->x4FB8 == 1) {
        type = 1;
    } else {
        type = 0;
    }
}

static inline void S32SetI(s32& d, s32 v) { d = v; }

int cItemMgr::init()
{
    ItemWork* p;
    int i;
    u32 sz;

    nItems = 0x180;
#line 2508 "D:/Bio4/Prog/item.cpp"
    pItems = (ItemWork*) MEM_ALLOC(0x180 * sizeof(ItemWork), 1, 13);
    pOrder = (ItemOrder*) MEM_ALLOC(nItems * sizeof(ItemOrder), 1, 13);
    if (pItems == 0) {
        return 0;
    }
    p = pItems;
    for (i = 0; i < nItems; i++, p++) {
        p->flags = 0;
        // COMPILER-DIFF: #13 (the flag-table size as a reload-materialised constant, weight 0 like the
        // string addi): set inside the loop body, the single set does not dominate the call block, so
        // gcse cprop leaves `sz` a pseudo (sched1 ranks the r3 copy at weight 0 next to the stw) and
        // update_equiv_regs substitutes the constant afterwards.
        sz = 8 * sizeof(u32);
    }
    nFlags = 8;
#line 2522 "D:/Bio4/Prog/item.cpp"
    pFlags = (u32*) MEM_ALLOC(sz, 1, 13);
    if (pFlags == 0) {
        Mem_free(pItems);
        Mem_free(pOrder);
        return 0;
    }
    flagclear();
    checkId = 0xFFFF;
    return 1;
}

void itemInfo(u16 id, ItemInfo* info)
{
    switch (id) {
    case 0x03:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x36:
    case 0x37:
    case 0x3E:
    case 0x40:
    case 0x51:
    case 0x52:
    case 0x6B:
    case 0x6C:
    case 0x94:
    case 0x99:
    case 0xAB:
        info->type = 1;
        info->x3 = 0;
        info->x4 = 0;
        break;
    case 0x01:
    case 0x02:
    case 0x0E:
    case 0x17:
    case 0x35:
    case 0x38:
    case 0x6D:
        info->type = 3;
        info->x3 = 1;
        info->x4 = 1;
        break;
    case 0x04:
        info->type = 2;
        info->x3 = 10;
        info->x4 = 50;
        break;
    case 0x00:
        info->type = 2;
        info->x3 = 5;
        info->x4 = 10;
        break;
    case 0x18:
        info->type = 2;
        info->x3 = 10;
        info->x4 = 15;
        break;
    case 0x07:
        info->type = 2;
        info->x3 = 5;
        info->x4 = 10;
        break;
    case 0x6A:
        info->type = 2;
        info->x3 = 50;
        info->x4 = 100;
        break;
    case 0x20:
        info->type = 2;
        info->x3 = 50;
        info->x4 = 100;
        break;
    case 0x1A:
        info->type = 2;
        info->x3 = 10;
        info->x4 = 10;
        break;
    case 0x46:
        info->type = 2;
        info->x3 = 1;
        info->x4 = 5;
        break;
    case 0xA0:
        info->type = 2;
        info->x3 = 5;
        info->x4 = 10;
        break;
    case 0x72:
        info->type = 2;
        info->x3 = 5;
        info->x4 = 20;
        break;
    case 0x3F:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0xAA:
    case 0xC5:
        info->type = 9;
        info->x3 = 0;
        info->x4 = 0;
        break;
    case 0x05:
    case 0x08:
    case 0x09:
    case 0x0A:
        info->type = 6;
        info->x3 = 1;
        info->x4 = 1;
        break;
    case 0x06:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x19:
    case 0x1C:
    case 0x95:
    case 0x97:
    case 0xA8:
        info->type = 6;
        info->x3 = 1;
        info->x4 = 1;
        break;
    case 0x56:
    case 0x57:
    case 0x5F:
    case 0x60:
    case 0x61:
    case 0x77:
    case 0xA1:
    case 0xB9:
    case 0xBA:
    case 0xBB:
    case 0xC7:
    case 0xC8:
    case 0xC9:
    case 0xD2:
    case 0xD3:
    case 0xD4:
        info->type = 12;
        info->x3 = 1;
        info->x4 = 999;
        break;
    case 0x1B:
    case 0x58:
    case 0x59:
    case 0x5A:
    case 0x5B:
    case 0x5C:
    case 0x5D:
    case 0x5E:
    case 0x62:
    case 0x63:
    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67:
    case 0x68:
    case 0x70:
    case 0x89:
    case 0x8A:
    case 0x8F:
    case 0x90:
    case 0x91:
    case 0x93:
    case 0x96:
    case 0x98:
    case 0x9A:
    case 0x9B:
    case 0x9C:
    case 0x9D:
    case 0x9E:
    case 0x9F:
    case 0xA2:
    case 0xB8:
    case 0xBC:
    case 0xBD:
    case 0xBE:
    case 0xBF:
    case 0xC0:
    case 0xC1:
    case 0xC2:
    case 0xC6:
    case 0xCA:
    case 0xCB:
    case 0xCC:
    case 0xCD:
    case 0xCE:
    case 0xCF:
    case 0xD0:
    case 0xD1:
    case 0xD5:
    case 0xD6:
    case 0xD7:
    case 0xD8:
    case 0xD9:
    case 0xDA:
    case 0xDB:
        info->type = 5;
        info->x3 = 1;
        info->x4 = 999;
        break;
    case 0x78:
        info->type = 8;
        info->x3 = 30;
        info->x4 = 999;
        break;
    case 0x79:
        info->type = 8;
        info->x3 = 30;
        info->x4 = 999;
    case 0x71:
        info->type = 8;
        info->x3 = 5;
        info->x4 = 999;
        break;
    case 0x73:
        info->type = 8;
        info->x3 = 30;
        info->x4 = 999;
        break;
    case 0x75:
        info->type = 8;
        info->x3 = 30;
        info->x4 = 999;
        break;
    case 0x48:
    case 0x49:
    case 0x4A:
    case 0x4B:
    case 0x4C:
    case 0x4D:
    case 0x4E:
    case 0x4F:
    case 0x50:
    case 0xAC:
    case 0xAD:
    case 0xAE:
    case 0xAF:
    case 0xB0:
    case 0xB1:
    case 0xB2:
    case 0xB3:
    case 0xB4:
    case 0xB5:
    case 0xB6:
    case 0xB7:
    case 0xF4:
    case 0xF5:
    case 0xF6:
    case 0xF7:
    case 0xF8:
    case 0xF9:
    case 0xFA:
    case 0xFB:
    case 0xFC:
    case 0xFD:
        info->type = 10;
        info->x3 = 1;
        info->x4 = 1;
        break;
    case 0x54:
    case 0x55:
    case 0x7C:
    case 0x7D:
    case 0x7E:
    case 0x7F:
    case 0xA9:
    case 0xFE:
        info->type = 11;
        info->x3 = 1;
        info->x4 = 1;
        break;
    case 0xDC:
    case 0xDD:
    case 0xDE:
    case 0xDF:
    case 0xE0:
    case 0xE1:
    case 0xE2:
    case 0xE3:
    case 0xE4:
    case 0xE5:
    case 0xE6:
    case 0xE7:
    case 0xE8:
    case 0xE9:
    case 0xEA:
    case 0xEB:
    case 0xEC:
    case 0xED:
    case 0xEE:
    case 0xEF:
    case 0xF0:
    case 0xF1:
    case 0xF2:
    case 0xF3:
        info->type = 13;
        info->x3 = 1;
        info->x4 = 999;
        break;
    case 0x0C:
        info->type = 14;
        info->x3 = 1;
        info->x4 = 1;
        break;
    default:
        info->type = 7;
        info->x3 = 1;
        info->x4 = 1;
        break;
    }
}

void cItemMgr::construct(ItemWork* p, u16 id)
{
    ItemInfo info;

    p->flags = 1;
    p->id = id;
    p->num = 0;
    p->board = 0;
    p->x = 0;
    p->y = 0;
    p->orient = 0;
    p->type = type;
    if (ITEM_TYPE(id) == 1) {
        switch (id) {
        case 0x40:
            p->id = 0x21;
            if (pGS->flags_51C0 & 0x8000) {
                LV_FIRE_SET(p, 1);
            } else {
                LV_FIRE_SET(p, 0);
            }
            LV_MAG_SET(p, 0);
            LV_SPEED_SET(p, 0);
            LV_EX_SET(p, 0);
            break;
        case 0x34:
            LV_SET(p, 6, 0, 2, 5);
            break;
        default:
            LV_SET(p, 0, 0, 0, 0);
            asm volatile("");
            break;
        }
        p->x8 = BULLET(p);
        setBullet(p, WeaponId2ChargeNum(id, 1));
    }
    if (ITEM_TYPE(id) == 9) {
        p->x6 = 0;
        p->x8 = 0xFFFF;
    }
    if (ITEM_TYPE(id) == 10) {
        p->x6b[0] = countFiles();
    }
}

ItemWork* cItemMgr::at(int no)
{
    if (no < nItems) {
        return &pItems[no];
    }
    return 0;
}

int cItemMgr::searchAt(ItemWork* p)
{
    int i;

    for (i = 0; i < nItems; i++) {
        if (p == &pItems[i]) {
            return i;
        }
    }
    return -1;
}

u8 gld_order(u8 idx)
{
    int n = g_item_order_num;
    int i;

    if (ItemMgr.at(idx) == 0) {
        return 0xFF;
    }
    for (i = 0; i < n; i++) {
        if (g_item_order[i] == ItemMgr.at(idx)->id) {
            return i;
        }
    }
    return 0xFF;
}

int gld_cmp(const void* a, const void* b)
{
    return gld_order(*(u8*) a) - gld_order(*(u8*) b);
}

int cItemMgr::makeItemList(u8* list, int all, s8* pNum, s8* pNum2)
{
    ItemInfo info;
    ItemWork* p = pItems;
    int cnt = 0;
    int i;

    *pNum2 = 0;
    *pNum = 0;
    for (i = 0; i < nItems; i++, p++) {
        if (all == 0) {
            if (itemUse(p, type)) {
                switch (ITEM_TYPE(p->id)) {
                case 5:
                case 12:
                    (*pNum2)++;
                    break;
                case 0:
                case 7:
                    list[*pNum] = i;
                    (*pNum)++;
                    break;
                case 1:
                case 2:
                case 3:
                case 4:
                case 6:
                case 8:
                case 9:
                    break;
                }
            }
        } else {
            if (itemUse(p, type) == 0) {
                list[i] = 0xFF;
            } else {
                list[i] = i;
            }
            cnt++;
        }
    }
    if (all == 0) {
        int j = 0;

        cnt = *pNum + *pNum2;
        p = pItems;
        for (i = 0; i < nItems; i++, p++) {
            if (itemUse(p, type)) {
                if (ITEM_TYPE(p->id) == 5 || info.type == 12) {
                    list[*pNum + j] = i;
                    j++;
                }
            }
        }
        qsort(list + *pNum, *pNum2, 1, gld_cmp);
    }
    return cnt;
}

ItemWork* cItemMgr::search(u16 id)
{
    ItemWork* p = pItems;
    int i;

    for (i = 0; i < nItems; i++, p++) {
        if (itemUse(p, type) && p->id == id) {
            return p;
        }
    }
    return 0;
}

ItemWork* cItemMgr::minimumSearch(u16 id)
{
    ItemWork* p = pItems;
    ItemWork* best = 0;
    int min = 10000000;
    int i;

    for (i = 0; i < nItems; i++, p++) {
        if (itemUse(p, type) && id == p->id && p->num < min) {
            min = p->num;
            best = p;
        }
    }
    return best;
}

int order_cmp(const void* a, const void* b)
{
    return ((ItemOrder*) b)->num - ((ItemOrder*) a)->num;
}

void cItemMgr::ordering(u16 id)
{
    ItemWork* p = pItems;
    int n = 0;
    int i;

    for (i = 0; i < nItems; i++, p++) {
        if (itemUse(p, type) && id == p->id) {
            pOrder[n].item = p;
            pOrder[n].num = p->num;
            n++;
        }
    }
    nOrder = n;
    qsort(pOrder, n, sizeof(ItemOrder), order_cmp);
}

int addMoney(int n)
{
    U32Set(pG->x4F98, pG->x4F98 + n);
    if ((s32) pG->x4F98 > 99999998) {
        pG->x4F98 = 99999999;
    }
    return 1;
}

// Inline wrappers: the caller's `&inf` is substituted into the hard-register argument set (recomputed
// `addi r4,r1,16` per call, not PRE'd into a callee-saved register).
static inline void itemInfoIW(int id, ItemInfo* inf)
{
    itemInfoI(id, inf);
}

static inline void itemInfoW(u16 id, ItemInfo* inf)
{
    itemInfo(id, inf);
}

int cItemMgr::get(int id, int num)
{
    ItemInfo info;
    ItemInfo inf;
    ItemInfo* pInfo = &info;
    ItemWork* p;
    int max;
    int i;

    switch (id) {
    case 0x78:
        if (num != 0) {
            return addMoney(num * 10);
        }
        return addMoney(300);
    case 0x79:
        if (num != 0) {
            return addMoney(num * 100);
        }
        return addMoney(3000);
    case 0x71:
        if (num != 0) {
            return addMoney(num * 1000);
        }
        return addMoney(5000);
    case 0x73:
        x28 = num;
        MercSysSetAddTime(num);
        pG->cdown_add_sec = x28;
        return 1;
    case 0x75:
        if (num == 0) {
            num = 30;
        }
        x2C = num * 30;
        MercSysSetBonusTime(num * 30);
        return 1;
    }
    itemInfoIW(id, &inf);
    switch (inf.type) {
    case 5:
    case 12:
    case 13:
        p = pItems;
        for (i = 0; i < nItems; i++, p++) {
            if (itemUse(p, type) && id == p->id) {
                int total;

                if (num == 0) {
                    itemInfoIW(id, &inf);
                    num = inf.x3;
                }
                total = p->num + num;
                itemInfoW(p->id, &inf);
                if (total <= inf.x4) {
                    {
                        int t = num + p->num;

                        p->num = t;
                    }
                    return 1;
                }
                return 0;
            }
        }
        break;
    }
    itemInfoI(id, pInfo);
    if (pInfo->type == 1 || pInfo->type == 9) {
        num = 1;
        max = 1;
    } else {
        if (num == 0) {
            num = pInfo->x3;
        }
        max = pInfo->x4;
    }
    if (num > max) {
        num = (u16) max;
        pLog->err(0, 0, "cItemMgr::get(): Volume of ITEM(0x%02x) is OOL.", id);
    }
    pLast = 0;
    p = pItems;
    for (i = 0; i < nItems; i++, p++) {
        if (itemEmpty(p)) {
            pLast = p;
            constructI(p, id);
            p->num = num;
            break;
        }
    }
    if (pLast == 0) {
        pLog->err(0, 0, "cItemMgr::get() Can't create item.");
        return 0;
    }
    return 1;
}

static inline int useSubChar(cItemMgr* m)
{
    if (m->x12 == 0) {
        return pG->x4FB8 == 1;
    }
    return 1;
}

int cItemMgr::use(ItemWork* p)
{
    ItemInfo info;

    if (p == 0) {
        return 0;
    }
    if (ITEM_TYPE(p->id) == 1) {
    ng:
        return 0;
    }
    if (p->num == 0) {
        goto ng;
    }
    switch (p->id) {
    case 0x01:
    case 0x02:
    case 0x0E:
    case 0x17:
    case 0x35:
        p->num--;
        if ((s32) pGS->flags_6C < 0) {
            if (p->num != 0) {
                return 1;
            }
            p->num = 5;
        }
        goto chk;
    case 0x15:
    case 0x16: {
        int hp = 0;
        int ok = 0;
        int heal = 0;

        if (!useSubChar(this)) {
            int level = lifeLevel(20, pG->pl_life_max, 1200);

            if (level <= 19) {
                level++;
                U16Set(pG->pl_life_max, 1200);
                pG->pl_life_max += (int) ((f32) (level * 60) + 0.5f);
                ok = 1;
            }
        } else if (pG->x4FB8 == 1) {
            int level = lifeLevel(5, pG->pl_life_max, 600);

            if (level <= 4) {
                level++;
                U16Set(pG->pl_life_max, 600);
                pG->pl_life_max += (int) ((f32) (level * 120) + 0.5f);
                ok = 1;
            }
        } else {
            int level = lifeLevel(5, pG->sub_life_max, 600);

            if (level <= 4) {
                level++;
                U16Set(pG->sub_life_max, 600);
                pG->sub_life_max += (int) ((f32) (level * 120) + 0.5f);
                ok = 1;
            }
        }
        switch (p->id) {
        case 0x16:
            heal = 600;
            break;
        case 0x15:
            heal = 2400;
            break;
        }
        if (heal != 0) {
            if (healing(heal) != 0) {
                hp = 1;
            }
        }
        if (ok == 0 && hp == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    }
    case 0x08:
        if (healing(400) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x09:
        if (healing(800) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x0A:
        if (healing(2400) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x05:
        if (healing(2400) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x95:
        if (healing(900) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x97:
        if (healing(2400) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x06:
        if (healing(600) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x12:
        if (healing(1300) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x13:
        if (healing(2400) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    case 0x14:
        if (healing(2400) == 0) {
            asm volatile(""); // COMPILER-DIFF: 6 (layout: keeps the healing arms apart at jump2)
            goto ng;
        }
        break;
    default:
        {
            int no = p->id;

            if (!(pFlags[no >> 5] & (0x80000000 >> (no & 0x1F)))) {
                return 0;
            }
        }
        flagclear();
        checkId = p->id;
        if (p->id == 0x84 || p->id == 0x92) {
            goto chk;
        }
        break;
    }
    p->num--;
chk:
    if (p->num == 0) {
        p->flags = 0;
    }
    return 1;
}

void cItemMgr::erase(ItemWork* p)
{
    ItemInfo info;

    p->flags = 0;
    if (ITEM_TYPE(p->id) == 1) {
        int idx = ItemMgr.searchAt(p);
        ItemWork* q = pItems;
        int i;

        for (i = 0; i < nItems; i++, q++) {
            if (itemUse(q, type)) {
                if (ITEM_TYPE(q->id) == 9 && q->x6 == 1 && idx == q->x8) {
                    q->x6 = 0;
                    q->x8 = 0xFFFF;
                }
            }
        }
        if (p == pArm) {
            ItemMgr.arm(0);
        }
    } else {
        cItemMgr* m = &ItemMgr;

        if (ITEM_TYPE(m->armId) == 1) {
            m->arm(m->pArm);
        }
    }
}

int cItemMgr::dump(int id)
{
    return dump(searchI(id));
}

int cItemMgr::dump(ItemWork* p)
{
    if (p == 0) {
        return 0;
    }
    if (p->num != 0) {
        p->num--;
    }
    if (p->num == 0) {
        erase(p);
    }
    return 1;
}

int cItemMgr::dumpAll(ItemWork* p)
{
    if (p == 0) {
        return 0;
    }
    p->num = 0;
    erase(p);
    return 1;
}

int cItemMgr::dumpType(int t)
{
    ItemInfo info;
    ItemWork* p = pItems;
    int i;

    for (i = 0; i < nItems; i++, p++) {
        if (itemUse(p, type)) {
            if (t == ITEM_TYPE(p->id)) {
                dumpAll(p);
            }
        }
    }
    return 1;
}

int cItemMgr::num(int id, u8 t)
{
    ItemWork* p = pItems;
    u16 n = 0;
    int i;

    for (i = 0; i < nItems; i++, p++) {
        if (itemUse(p, t) && p->id == id) {
            n += p->num;
        }
    }
    return n;
}

int cItemMgr::num(int id)
{
    ItemWork* p = pItems;
    u16 n = 0;
    int i;

    for (i = 0; i < nItems; i++, p++) {
        if (itemUse(p, type) && p->id == id) {
            n += p->num;
        }
    }
    return n;
}

int cItemMgr::num(ItemWork* p)
{
    if (p == 0) {
        return 0;
    }
    return p->num;
}

int itemCombineCheck(u16 id)
{
    int i;

    for (i = 0; i < (int) (sizeof(combination_info) / sizeof(combination_info[0])); i++) {
        if (id == combination_info[i].a || id == combination_info[i].b) {
            return 1;
        }
    }
    return 0;
}

int itemCombine(u16 a, u16 b, u16* result)
{
    int i;

    for (i = 0; i < (int) (sizeof(combination_info) / sizeof(combination_info[0])); i++) {
        if (a == combination_info[i].a && b == combination_info[i].b) {
            *result = combination_info[i].result;
            return 1;
        }
    }
    for (i = 0; i < (int) (sizeof(combination_info) / sizeof(combination_info[0])); i++) {
        if (b == combination_info[i].a && a == combination_info[i].b) {
            *result = combination_info[i].result;
            return 1;
        }
    }
    return 0;
}

int cItemMgr::combine(ItemWork* a, ItemWork* b, int flag)
{
    ItemInfo info;
    u16 newId;
    int ret = 0;
    u16 ida;
    u16 idb;

    if (a == 0 || b == 0) {
        return 0;
    }
    if (a == b && b->num <= 1) {
        return 0;
    }
    if (flag != 0) {
        ida = weaponId(a);
        idb = weaponId(b);
    } else {
        ida = a->id;
        idb = b->id;
    }
    if (ITEM_TYPE(a->id) == 1) {
        int lv = LV_EX(a) + 1;

        if (ITEM_TYPE(b->id) == 2) {
            int attr;
            u8 attr8;

            if (flag == 0) {
                ret = 0;
                goto end;
            }
            attr = ATTR(a);
            attr8 = attr;
            if (idb == WeaponId2BulletId(ida, attr8)) {
                ret = reload_main(a, b, WeaponId2ChargeNum(ida, lv));
            } else {
                int inv = attr == 0;
                if (idb == WeaponId2BulletId(ida, inv)) {
                    int n = b->num;

                    b->id = WeaponId2BulletId(ida, attr8);
                    b->num = BULLET(a);
                    if (b->num == 0) {
                        erase(b);
                    }
                    a->x8 = (inv << 13) | (n & 0x1FFF);
                    {
                        register ItemWork* arm asm("r0"); // COMPILER-DIFF: 17 (local-alloc fake-lifetime parity: the pArm load reuses r0 right after the x8 store's value dies)

                        arm = pArm;
                        if (a == arm) {
                            pG->wep_x4FB2 = ATTR(a);
                        }
                    }
                    ret = 1;
                }
            }
        } else if (ITEM_TYPE(b->id) == 9) {
            ret = partsCombine(a, b);
        }
    } else if (ITEM_TYPE(b->id) == 1) {
        int lv = LV_EX(b) + 1;

        if (ITEM_TYPE(a->id) == 2) {
            int attr;
            u8 attr8;

            if (flag == 0) {
                ret = 0;
                goto end;
            }
            attr = ATTR(b);
            attr8 = attr;
            if (ida == WeaponId2BulletId(idb, attr8)) {
                ret = reload_main(b, a, WeaponId2ChargeNum(idb, lv));
            } else {
                int inv = attr == 0;
                if (ida == WeaponId2BulletId(idb, inv)) {
                    int n = a->num;

                    a->id = WeaponId2BulletId(idb, attr8);
                    a->num = BULLET(b);
                    if (a->num == 0) {
                        erase(a);
                    }
                    b->x8 = (inv << 13) | (n & 0x1FFF);
                    {
                        register ItemWork* arm asm("r0"); // COMPILER-DIFF: 17 (local-alloc fake-lifetime parity: the pArm load reuses r0 right after the x8 store's value dies)

                        arm = pArm;
                        if (b == arm) {
                            pG->wep_x4FB2 = ATTR(b);
                        }
                    }
                    ret = 1;
                }
            }
        } else if (ITEM_TYPE(a->id) == 9) {
            ret = partsCombine(b, a);
        }
    } else if (ITEM_TYPE(a->id) == 2 || ITEM_TYPE(a->id) == 3) {
        if (ida == idb) {
            int room;

            itemInfo(a->id, &info);
            room = info.x4 - a->num;
            if (room == 0) {
                ret = 0;
            } else {
                if (b->num < room) {
                    a->num = a->num + b->num;
                    b->num = 0;
                } else {
                    a->num = a->num + room;
                    b->num = b->num - room;
                }
                ret = 1;
            }
        }
    } else {
        ret = itemCombine(ida, idb, &newId);
        if (ret != 0) {
            a->num--;
            b->num--;
            if (a->num != 0) {
                get(newId, 1);
            } else {
                itemInfo(newId, &info);
                a->num = 1;
                a->id = newId;
            }
        }
    }
    if (a->num == 0) {
        erase(a);
    }
    if (b->num == 0) {
        erase(b);
    }
end:
    return ret;
}

int cItemMgr::partsCombine(ItemWork* wep, ItemWork* part)
{
    u16 newId;
    ItemWork* list[2];
    ItemWork* p;
    ItemWork** lp;
    int ret;
    int idx;
    int n;
    int i;

    ret = itemCombine(wep->id, part->id, &newId);
    if (ret != 0) {
        ItemInfo info;

        if (part->x6 != 0) {
        part->x6 = 0;
        if (pArm != 0 && pArm == at(part->x8)) {
            armId = weaponId(pArm);
        }
        part->x8 = 0xFFFF;
    }
    p = pItems;
    idx = searchAt(wep);
    list[0] = 0;
    list[1] = 0;
    n = 0;
    i = 0;
    if (i < nItems) {
        lp = list;
        do {
            if (itemUse(p, type)) {
                if (ITEM_TYPE(p->id) == 9 && p->x6 == 1 && idx == p->x8) {
                    *lp++ = p;
                    n++;
                }
            }
            p++;
        } while (++i < nItems);
    }
    for (int j = 0; j < n; j++) {
        if (list[j]->id == part->id) {
            list[j]->x6 = 0;
            list[j]->x8 = 0xFFFF;
            list[j] = 0;
        }
    }
    if (n == 1) {
        if (list[0] != 0) {
            list[0]->x6 = 0;
            list[0]->x8 = 0xFFFF;
            list[0] = 0;
        }
    }
        part->x8 = searchAt(wep);
        part->x6 = 1;
        if (pArm != 0 && pArm == wep) {
            armId = weaponId(wep);
        }
    }
    return ret;
}

int cItemMgr::available(u16 id)
{
    pFlags[id >> 5] |= 0x80000000 >> (id & 0x1F);
}

void cItemMgr::flagclear()
{
    int i;

    for (i = 0; i < nFlags; i++) {
        pFlags[i] = 0;
    }
}

int cItemMgr::check(u16 id)
{
    if (id == checkId) {
        checkId = 0xFFFF;
        return 1;
    }
    return 0;
}

u16 bareHand()
{
    return 0x800;
}

int cItemMgr::arm(ItemWork* p)
{
    ItemInfo info;

    if (p == 0) {
        pArm = p;
        armId = bareHand();
        goto ok;
    }
    if (p->num == 0) {
        goto ng;
    }
    if (ITEM_TYPE(p->id) == 1 || ITEM_TYPE(p->id) == 3 || ITEM_TYPE(p->id) == 6) {
        pArm = p;
        armId = weaponId(p);
        pArm->x8 = BULLET(pArm);
        if (ITEM_TYPE(p->id) == 1) {
            pG->wep_lv = p->x6 >> 12;
            pG->wep_lv_mag = (p->x6 >> 8) & 0xF;
            pG->x4FBA = (p->x6 >> 4) & 0xF;
            pG->wep_lv_ex = LV_EX(p);
        }
        goto ok;
    }
ng:
    return 0;
ok:
    return 1;
}

int cItemMgr::reloadable()
{
    return reloadable(pArm, 0);
}

int cItemMgr::reloadable(ItemWork* p, int flag)
{
    int ret = 0;
    u16 id;
    u16 have;
    u16 max;

    if (p == 0) {
        return 0;
    }
    id = weaponId(p);
    if (id == 0x52) {
        return 0;
    }
    WeaponId2ChargeNum(id, LV_EX(p) + 1);
    have = BULLET(p);
    max = WeaponId2ChargeNumI(id, LV_EX(p) + 1);
    if (have < max) {
        if (search(WeaponId2BulletId(id, ATTR(p))) != 0) {
            ret = 1;
        } else if (flag != 0 && (p->id == 0x36 || p->id == 0xAB)) {
            if (search(WeaponId2BulletId(p->id, ATTR(p) == 0)) != 0) {
                ret = 1;
            }
        }
        if ((s32) pG->flags_6C < 0) {
            ret = 1;
        }
    }
    return ret;
}

int cItemMgr::reload()
{
    return reload(pArm, 0);
}

int cItemMgr::reload(ItemWork* p, int flag)
{
    ItemInfo info;
    int ret = 0;
    u16 id;
    u16 bid;

    if (p == 0) {
        return 0;
    }
    id = weaponId(p);
    if (ITEM_TYPE(id) != 1) {
        goto done;
    }
    if ((s32) pG->flags_6C < 0) {
        setBullet(p, WeaponId2ChargeNum(id, LV_EX(p) + 1));
        return 0;
    }
    bid = WeaponId2BulletId(id, ATTR(p));
    if (flag != 0 && (p->id == 0x36 || p->id == 0xAB)) {
        if (ItemMgr.bulletNum() == 0 && ItemMgr.num(bid) == 0) {
            {
                int z = ATTR(p) == 0;
                p->x8 = BULLET(p) | (z << 13);
            }
            bid = WeaponId2BulletId(p->id, ATTR(p));
            if (p == pArm) {
                pG->wep_x4FB2 = ATTR(p);
            }
        }
    }
    ordering(bid);
    while (reloadable(p, flag) != 0 && num(bid) != 0) {
        combine(p, minimumSearch(bid), 1);
        ret = 1;
    }
done:
    return ret;
}

int reload_main(ItemWork* wep, ItemWork* ammo, int max)
{
    int have = BULLET(wep);
    int room = max - have;
    int n;
    int m;

    if (room == 0) {
        return 0;
    }
    if (ammo->num >= max) {
        n = max;
    } else {
        n = ammo->num;
    }
    if (n > room) {
        m = room;
    } else {
        m = n;
    }
    wep->x8 = (wep->x8 & 0xE000) | ((have + m) & 0x1FFF);
    ammo->num -= m;
    return 1;
}

int cItemMgr::trigger()
{
    ItemInfo info;

    switch (ITEM_TYPE(armId)) {
    case 1:
        return trigger(pArm);
    case 3:
    case 6: {
        ItemWork* p = pArm;
        register int id asm("r9"); // COMPILER-DIFF: #2 (the original masks the u16 member before the call)
        id = armId;
        asm("" : "+r"(id));

        if (p->flags == 0 || id != p->id) {
            p = minimumSearch((u16) id);
        }
        return trigger(p);
    }
    }
    return 0;
}

int cItemMgr::trigger(ItemWork* p)
{
    ItemInfo info;

    if (pG->flags_68 & 0x00400000) {
        return 1;
    }
    if (p == 0) {
        return 0;
    }
    if (p->id == 0x6D) {
        return 1;
    }
    WeaponId2ChargeNum(p->id, LV_EX(p) + 1);
    switch (ITEM_TYPE(p->id)) {
    case 1: {
        int n;

        if (p->id == 0x52) {
            return dump(0x72);
        }
        n = BULLET(p);
        if (n == 0) {
            return 0;
        }
        setBullet(p, n - 1);
        return 1;
    }
    case 3:
    case 6:
        if ((s32) pG->flags_6C < 0) {
            return 1;
        }
        if (p->id == 0x35) {
            ItemMgr.arm(0);
        }
        return dump(p);
    }
    return 0;
}

u16 cItemMgr::weaponId(ItemWork* p)
{
    ItemInfo info;
    u16 id;
    ItemWork* q = pItems;
    int idx = searchAt(p);
    int i;

    id = p->id;
    if (ITEM_TYPE(id) != 1) {
        return p->id;
    }
    for (i = 0; i < nItems; i++, q++) {
        if (itemUse(q, type)) {
            if (ITEM_TYPE(q->id) == 9 && q->x6 == 1 && idx == q->x8) {
                itemCombine(id, q->id, &id);
            }
        }
    }
    return id;
}

ItemWork* cItemMgr::weaponParts(ItemWork* p, int no)
{
    ItemInfo info;
    ItemWork* q = pItems;
    int idx = searchAt(p);
    int cnt = 0;
    int i;

    if (p == 0) {
        return 0;
    }
    for (i = 0; i < nItems; i++, q++) {
        if (itemUse(q, type)) {
            if (ITEM_TYPE(q->id) == 9 && q->x6 == 1 && idx == q->x8) {
                if (cnt++ == no) {
                    return q;
                }
            }
        }
    }
    return 0;
}

int cItemMgr::bulletNumTotal(int bulletId)
{
    static int tbl_num = sizeof(wep_info) / sizeof(wep_info[0]);
    u16 total = numS(bulletId);
    int i;

    for (i = 0; i < tbl_num; i++) {
        if (bulletId == wep_info[i].bulletId) {
            total += bulletNum(wep_info[i].id);
        }
    }
    return total;
}

u16 cItemMgr::bulletNum()
{
    return bulletNumCurrentS();
}

u32 cItemMgr::bulletNumCurrent()
{
    ItemInfo info;

    switch (ITEM_TYPE(armId)) {
    case 1:
        return bulletNum(pArm);
    case 3:
    case 6:
        return bulletNum(armId);
    }
    return 0;
}

int cItemMgr::bulletNum(u16 id)
{
    ItemWork* p = pItems;
    u16 total = 0;
    int i;

    if (id == 0x6D) {
        return 1;
    }
    for (i = 0; i < nItems; i++, p++) {
        if (itemUse(p, type) && p->id == id) {
            total += bulletNum(p);
        }
    }
    return total;
}

int cItemMgr::bulletNum(ItemWork* p)
{
    ItemInfo info;
    int n;

    if ((s32) pG->flags_6C >= 0 && (pG->flags_68 & 0x00400000)) {
        return 100;
    }
    if (p == 0) {
        return 0;
    }
    switch (ITEM_TYPE(p->id)) {
    case 1:
        if (p->id == 0x52) {
            return num(0x72);
        }
        n = BULLET(p);
        break;
    case 3:
        return p->num;
    case 6:
        switch (p->id) {
        case 8:
        case 9:
        case 10:
            return p->num;
        default:
            return 0;
        }
    default:
        n = 0;
        break;
    }
    return n;
}

int cItemMgr::saveDataSize()
{
    return sizeof(ItemSaveData);
}

void cItemMgr::save(void* dst)
{
    ItemInfo info;
    ItemSaveData* sd = (ItemSaveData*) dst;
    ItemSaveWork* s = sd->item;
    ItemWork* p = pItems;
    int i;

    sd->armIdx = 0xFFFF;
    for (i = 0; i < 0x180; i++) {
        memclr_asm(&s[i], sizeof(ItemSaveWork));
        s[i].id = 0xFFFF;
    }
    for (i = 0; i < nItems; i++, p++) {
        memclr_asm(&s[i], sizeof(ItemSaveWork));
        if (itemEmpty(p)) {
            register int m1 asm("r0"); // COMPILER-DIFF: #13 (the 0xFFFF re-materialised at the store)
            m1 = -1;
            s[i].id = m1;
        } else {
            s[i].id = p->id;
            if (p->type == 1) {
                s[i].id |= 0x8000;
            }
            switch (ITEM_TYPE(p->id)) {
            case 1:
            case 9:
                s[i].x2 = p->x6;
                s[i].x4 = p->x8;
                break;
            case 10:
                s[i].x2 = p->num;
                s[i].x4 = p->x6b[0];
                break;
            default:
                s[i].x2 = p->num;
                break;
            }
            s[i].x = p->x;
            s[i].y = p->y;
            s[i].orient = p->orient;
            s[i].board = p->board;
        }
        if (p == pArm) {
            sd->armIdx = i;
        }
    }
    sd->armId = armId;
}

void cItemMgr::load(void* src)
{
    ItemInfo info;
    ItemSaveData* sd = (ItemSaveData*) src;
    ItemSaveWork* s = sd->item;
    ItemWork* p = pItems;
    int i;

    pArm = 0;
    for (i = 0; i < nItems; i++, p++) {
        if (s[i].id != 0xFFFF) {
            int wep = 0;

            if (s[i].id & 0x8000) {
                wep = 1;
            }
            construct(p, s[i].id & 0x7FFF);
            if (wep) {
                p->type = 1;
            } else {
                p->type = 0;
            }
            switch (ITEM_TYPE(p->id)) {
            case 1:
                p->x6 = s[i].x2;
                p->x8 = s[i].x4;
                p->num = 1;
                break;
            case 9:
                p->x6 = s[i].x2;
                p->x8 = s[i].x4;
                p->num = 1;
                break;
            case 10:
                p->num = s[i].x2;
                p->x6b[0] = s[i].x4;
                break;
            default:
                p->num = s[i].x2;
                break;
            }
            p->x = s[i].x;
            p->y = s[i].y;
            p->orient = s[i].orient;
            p->board = s[i].board;
        } else {
            p->flags = 0;
        }
        if (sd->armIdx != 0xFFFF && i == sd->armIdx) {
            pArm = p;
        }
    }
    armId = sd->armId;
}

int cItemMgr::offboardDump(ItemWork* keep)
{
    ItemInfo info;
    ItemWork* p = pItems;
    int i;

    for (i = 0; i < nItems; i++, p++) {
        if (itemUse(p, type)) {
            switch (ITEM_TYPE(p->id)) {
            case 1:
            case 2:
            case 3:
            case 6:
            case 9:
                if (p->board == 0) {
                    if (keep != p) {
                        erase(p);
                    }
                    if (pArm == p) {
                        pArm = 0;
                        armId = bareHand();
                    }
                }
                break;
            }
        }
    }
    return 1;
}

void cItemMgr::takeOver()
{
    ItemInfo info;
    int i;

    if (type != 0) {
        pLog->err(0, 0, "cItemMgr::takeOver() Incorrect character type.");
        return;
    }
    for (i = 0; i < ItemMgr.nItems; i++) {
        ItemWork* p = ItemMgr.at(i);

        if (itemUse(p, 1)) {
            switch (ITEM_TYPE(p->id)) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 6: {
                f32 fx = (f32) p->x * 0.5f;
                f32 fy = 5.0f - (f32) p->y * 0.5f;
                s8 o = p->orient;

                p->x = (s8) (fy * 2.0f);
                p->y = (s8) (fx * 2.0f);
                if (o < 0) {
                } else if (o <= 3) {
                    p->orient++;
                    if (p->orient > 3) {
                        p->orient -= 4;
                    }
                } else if (o <= 7) {
                    p->orient++;
                    if (p->orient > 7) {
                        p->orient -= 4;
                    }
                }
                p->board = 0;
                break;
            }
            case 12: {
                ItemWork* q = ItemMgr.search(p->id);

                if (q != 0) {
                    u16 max;

                    q->num += p->num;
                    max = ITEM_MAX(q->id);
                    if (q->num > max) {
                        q->num = ITEM_MAX(q->id);
                    }
                    p->flags = 0;
                }
                break;
            }
            }
            p->type = 0;
        }
    }
}

int cItemMgr::countFiles()
{
    ItemInfo info;
    int n = 0;
    int i;

    for (i = 0; i < ItemMgr.nItems; i++) {
        ItemWork* p = ItemMgr.at(i);

        if (!itemUse(p, type)) {
            if (ITEM_TYPE(p->id) == 10) {
                n++;
            }
        }
    }
    return n;
}

u16 WeaponNo2WeaponId(u8 no, u8 type)
{
    static int tbl_num = sizeof(wep_info) / sizeof(wep_info[0]);
    int i;

    for (i = 0; i < tbl_num; i++) {
        if (no == wep_info[i].no && type == wep_info[i].type) {
            return wep_info[i].id;
        }
    }
    pLog->err(0, 0, "WeaponNo2WeaponId(): (%d, %d) not found.", no, type);
    return 0xFFFF;
}

u8 WeaponId2WeaponNo(u16 id)
{
    static int tbl_num = sizeof(wep_info) / sizeof(wep_info[0]);
    int i;

    for (i = 0; i < tbl_num; i++) {
        if (id == wep_info[i].id) {
            return wep_info[i].no;
        }
    }
    pLog->err(0, 0, "WeaponId2WeaponNo(): id 0x%02x not found.", id);
    return 0xFF;
}

u8 WeaponId2WeaponType(u16 id)
{
    static int tbl_num = sizeof(wep_info) / sizeof(wep_info[0]);
    int i;

    for (i = 0; i < tbl_num; i++) {
        if (id == wep_info[i].id) {
            return wep_info[i].type;
        }
    }
    pLog->err(0, 0, "WeaponId2WeaponType(): id 0x%02x not found.", id);
    return 0xFF;
}

u16 WeaponId2BulletId(u16 id, int attr)
{
    static int tbl_num = sizeof(wep_info) / sizeof(wep_info[0]);
    int i;

    for (i = 0; i < tbl_num; i++) {
        if (id == wep_info[i].id && attr == wep_info[i].attr) {
            return wep_info[i].bulletId;
        }
    }
    pLog->err(0, 0, "WeaponId2BulletId(): id 0x%02x not found.", id);
    return 0xFFFF;
}

u8 WeaponId2ChargeNum(u16 id, int level)
{
    static int tbl_num = sizeof(wep_info) / sizeof(wep_info[0]);
    int i;

    for (i = 0; i < tbl_num; i++) {
        if (id == wep_info[i].id) {
            return wep_info[i].charge[level - 1];
        }
    }
    pLog->err(0, 0, "WeaponId2ChargeNum(): id 0x%02x not found.", id);
    return 0xFF;
}

int WeaponId2MaxLevel(u16 id, int type)
{
    int i;

    for (i = 0; i < (int) (sizeof(wep_level_info) / sizeof(wep_level_info[0])); i++) {
        if (id == wep_level_info[i].id) {
            return wep_level_info[i].lv[type];
        }
    }
    return 1;
}

void cItemMgr::debugNumDisp(int a)
{
    static int sX = 48;
    static int sY = 5;
    ItemInfo info;
    int row = 0;
    int color = 0;
    int id = 0;
    int col = 0;

    for (; id <= 0xFE; id++) {
        int n;

        if (ITEM_TYPE(id) == 2) {
            n = bulletNumTotal((u16) id);
        } else {
            n = num((u16) id);
        }
        if (n != 0) {
            switch (ITEM_TYPE(id)) {
            case 0:
                color = 1;
                break;
            case 1:
                color = 2;
                break;
            case 2:
                color = 3;
                break;
            case 3:
                color = 4;
                break;
            case 5:
                color = 6;
                break;
            case 6:
                color = 7;
                break;
            case 7:
                color = 18;
                break;
            case 4:
            case 8:
                color = 0;
                break;
            case 9:
                color = 20;
                break;
            case 10:
                color = 22;
                break;
            case 11:
                color = 23;
                break;
            case 12:
                color = 24;
                break;
            case 13:
                color = 19;
                break;
            case 14:
                color = 5;
                break;
            }
            eprintf((sX - col) * 8, (sY + row++) * 14, color, a, "Item%02x : %03d", id, n);
        }
        if (row > 21) {
            row = 0;
            col += 14;
        }
    }
}

void cItemMgr::debugWeapon(int id)
{
    ItemWork* p = searchI(id);

    if (p != 0) {
        pArm = p;
    } else {
        get(id, 0);
        pArm = pLast;
    }
    armId = id;
}

// the split object pads .sdata to 8 bytes (lbl_80313F4C)
asm(".section .sdata; .balign 8");
