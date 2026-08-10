// Sscrn/ss_debug: sub screen debug menu (pad 3) and the attache case debug editor
// (D:/Bio4/Prog/ss_debug.cpp).
#include "types.h"
#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "item.h"
#include "cockpit.h"
#include "puzzle.h"
#include "joy.h"
#include "eprintf.h"
#include "main_mem.h"
#include "model.h"
#include "sscrn.h"
#include "ss_main.h"

// Attache case editor (ss_pzzl's SsPzzlMain owns one).
class ssDbgPzzl {
public:
    void* pSave;    // 0x00  ItemMgr save image taken at init (restored by item set 0x17)
    u8 caseSize;    // 0x04  wk->x2AE at init
    s8 cursor;      // 0x05  menu row
    s8 itemSet;     // 0x06  cItemMgr::setUp number, 0x17 = the saved inventory
    s8 bullet;      // 0x07  0 normal, 1 infinite, 2 infinite + no reload

    void init(SUB_SCREEN* wk);
    void quit();
    void move(SUB_SCREEN* wk);
};

// ss_pzzl.cpp debug offsets edited by the "Case Rot" row
extern int pzzlDbgNo;
extern f32 pzzlDbgPos;

static int dbg_cursor = 0;
static u8 dbg_blink = 0;
static int dbg_x = 42;
static int dbg_y = 5;

void SscrnDebugMenu(SUB_SCREEN* wk)
{
    const char* menu[4] = {"DEBUG DISP :", "MEMORY DISP:", "DEBUG PAGE :", "REVEIL     :"};
    JOY* joy = &Joy[2];
    int cx;
    int i;
    int y;

    if (joy->trg & 0x1000) {
        if (wk->x34C & 1) {
            wk->x34C &= ~1;
            return;
        }
        wk->x34C |= 1;
        dbg_cursor = 0;
    }
    if (wk->x34C & 1) {
        if (joy->trg & 0x200) {
            wk->x34C &= ~1;
            return;
        }
        if (joy->trg & 0x80008) {
            dbg_cursor--;
        }
        if (joy->trg & 0x40004) {
            dbg_cursor++;
        }
        dbg_cursor = dbg_cursor < 0 ? 0 : (dbg_cursor > 3 ? 3 : dbg_cursor);
        switch (dbg_cursor) {
        case 0:
            if (joy->trg & 0x10001) {
                wk->x34C |= 0x10;
            }
            if (joy->trg & 0x20002) {
                wk->x34C &= ~0x10;
            }
            break;
        case 1:
            if (joy->trg & 0x10001) {
                wk->x34C |= 0x20;
            }
            if (joy->trg & 0x20002) {
                wk->x34C &= ~0x20;
            }
            break;
        case 2:
            if (joy->rep & 0x10001) {
                pG->debug_mode--;
            }
            if (joy->rep & 0x20002) {
                pG->debug_mode++;
            }
            pG->debug_mode = pG->debug_mode < 0 ? 0x18 : (pG->debug_mode > 0x18 ? 0 : pG->debug_mode);
            break;
        case 3:
            if (joy->trg & 0x10001) {
                wk->x34C |= 0x40;
            }
            if (joy->trg & 0x20002) {
                wk->x34C &= ~0x40;
            }
            break;
        }
        eprintf(0x50, 0x8C, 5, 0, "----- SUBSCRN DEBUG -----");
        cx = 10;
        for (i = 0; i < 4; i++) {
            y = 0x9A + i * 0xE;
            eprintf(cx * 8, y, i == dbg_cursor ? 4 : 0, 0, "%s", menu[i]);
            switch (i) {
            case 0:
                if (wk->x34C & 0x10) {
                    eprintf((cx + 13) * 8, y, 0, 0, "ON-/---");
                } else {
                    eprintf((cx + 13) * 8, y, 0, 0, "---/OFF");
                }
                break;
            case 1:
                if (wk->x34C & 0x20) {
                    eprintf((cx + 13) * 8, y, 0, 0, "ON-/---");
                } else {
                    eprintf((cx + 13) * 8, y, 0, 0, "---/OFF");
                }
                break;
            case 2:
                eprintf((cx + 13) * 8, y, 0, 0, "%02d", pG->debug_mode);
                break;
            case 3:
                if (wk->x34C & 0x40) {
                    eprintf((cx + 13) * 8, y, 0, 0, "ON-/---");
                } else {
                    eprintf((cx + 13) * 8, y, 0, 0, "---/OFF");
                }
                break;
            }
        }
        eprintf(0x180, 0x62, 0, 0, "PRT");
        ssPartsMgr.dispWorkNum(0x1A0, 0x62, 0, 0);
        eprintf(0x180, 0x70, 0, 0, "MI");
        ssModInfoMgr.dispWorkNum(0x1A0, 0x70, 0, 0);
    }
    if (wk->x34C & 0x20) {
        pG->flags_68 |= 0x40000000;
    } else {
        pG->flags_68 &= ~0x40000000;
    }
}

void ssDbgPzzl::init(SUB_SCREEN* wk)
{
    cursor = 0;
    itemSet = 0x17;
#line 179 "D:/Bio4/Prog/ss_debug.cpp"
    pSave = MEM_ALLOC(ItemMgr.saveDataSize(), 1, 13);
    if (pSave) {
        ItemMgr.save(pSave);
        caseSize = wk->x2AE;
    }
}

void ssDbgPzzl::quit()
{
    if (pSave) {
        Mem_free(pSave);
    }
}

void ssDbgPzzl::move(SUB_SCREEN* wk)
{
    JOY* joy = &Joy[0];
    int changed = 0;
    int i;
    int x;
    int y;

    if ((s32) pG->flags_6C < 0) {
        bullet = 2;
    } else if (pG->flags_68 & 0x00400000) {
        bullet = 1;
    } else {
        bullet = 0;
    }
    dbg_blink++;
    if (joy->trg & 0x80008) {
        cursor--;
    }
    if (joy->trg & 0x40004) {
        cursor++;
    }
    if (joy->trg & 0xC000C) {
        dbg_blink = 0x18;
    }
    cursor = cursor < 0 ? 0 : (cursor > 5 ? 5 : cursor);
    switch (cursor) {
    case 0: {
        s8 old = itemSet;
        if (joy->trg & 0x10001) {
            itemSet--;
        }
        if (joy->trg & 0x20002) {
            itemSet++;
        }
        itemSet = itemSet < 0 ? 0x17 : (itemSet > 0x17 ? 0 : itemSet);
        if (old != itemSet) {
            ItemMgr.dumpType(1);
            ItemMgr.dumpType(2);
            ItemMgr.dumpType(3);
            ItemMgr.dumpType(4);
            ItemMgr.dumpType(6);
            ItemMgr.dumpType(9);
            ItemMgr.dumpType(0xE);
            if (itemSet == 0x17) {
                if (pSave) {
                    ItemMgr.load(pSave);
                    wk->x2AF = caseSize;
                } else {
                    itemSet = 0;
                    wk->x2AF = ItemMgr.setUp(itemSet);
                }
            } else {
                wk->x2AF = ItemMgr.setUp(itemSet);
            }
            changed = 1;
        }
        break;
    }
    case 1:
        if (joy->trg & 0x10001) {
            bullet--;
        }
        if (joy->trg & 0x20002) {
            bullet++;
        }
        bullet = bullet < 0 ? 2 : (bullet > 2 ? 0 : bullet);
        BitOff(pG->flags_68, 0x00400000);
        BitOff(pG->flags_6C, 0x80000000);
        switch (bullet) {
        case 2:
            BitOn(pG->flags_6C, 0x80000000);
            break;
        case 1:
            BitOn(pG->flags_68, 0x00400000);
            break;
        }
        break;
    case 2: {
        s8 old = wk->x2AF;
        if (joy->trg & 0x10001) {
            wk->x2AF--;
        }
        if (joy->trg & 0x20002) {
            wk->x2AF++;
        }
        wk->x2AF = (s8) wk->x2AF < 0 ? 3 : ((s8) wk->x2AF > 3 ? 0 : wk->x2AF);
        if (old != (s8) wk->x2AF) {
            ItemMgr.dump(0x7C);
            ItemMgr.dump(0x7D);
            ItemMgr.dump(0x7E);
            ItemMgr.dump(0x7F);
            switch ((s8) wk->x2AF) {
            case 0:
                ItemMgr.get(0x7C, 0);
                break;
            case 1:
                ItemMgr.get(0x7D, 0);
                break;
            case 2:
                ItemMgr.get(0x7E, 0);
                break;
            case 3:
                ItemMgr.get(0x7F, 0);
                break;
            }
            changed = 1;
        }
        break;
    }
    case 3: {
        int step = 1000;
        if (joy->on & 0x100) {
            step = 10000;
        }
        if (joy->rep2 & 0x10001) {
            pG->x4F98 -= step;
        }
        if (joy->rep2 & 0x20002) {
            pG->x4F98 += step;
        }
        {
            GlobalWork* g = pG;
            int p = g->x4F98;
            if (p >= 0) {
                if (p > 100000000) {
                    p = 100000000;
                }
            } else {
                p = 0;
            }
            g->x4F98 = p;
        }
        break;
    }
    case 4: {
        cMap* m = MapMgr.getWork(3);
        f32 step = 0.01f;
        f32 step2 = 10.0f;
        int step3 = 1;
        if (joy->on & 0x100) {
            step *= 10.0f;
            step3 = 10;
            step2 *= 10.0f;
        }
        if (joy->rep2 & 0x20) {
            m->rot.x -= step;
        }
        if (joy->rep2 & 0x40) {
            m->rot.x += step;
        }
        m->rot.x = m->rot.x < -1.5707964f ? 1.5707964f : (m->rot.x > 1.5707964f ? -1.5707964f : m->rot.x);
        if (joy->rep2 & 0x00800000) {
            pzzlDbgNo += step3;
        }
        if (joy->rep2 & 0x00400000) {
            pzzlDbgNo -= step3;
        }
        if (joy->rep2 & 0x00100000) {
            pzzlDbgPos -= step2;
        }
        if (joy->rep2 & 0x00200000) {
            pzzlDbgPos += step2;
        }
        break;
    }
    case 5:
        if (joy->rep2 & 0x10001) {
            g_boss_bar_flag--;
        }
        if (joy->rep2 & 0x20002) {
            g_boss_bar_flag++;
        }
        g_boss_bar_flag = (u32) g_boss_bar_flag > 3 ? 0 : g_boss_bar_flag;
        break;
    }
    if (changed) {
        wk->x2AE = wk->x2AF;
        wk->x2B0->quit();
        delete wk->x2B0;
        wk->x2B0 = new pzlPlayer;
        if (wk->x2B0->init((s8) wk->x2AE) == 0) {
            delete wk->x2B0;
        }
        pieceModelInit(wk);
        wk->x2B0->save();
    }
    {
        const char* items[6] = {"Item Set :", "Bullet   :", "Case Size:", "Peseta   :", "Case Rot :", "Boss Bar :"};
        x = dbg_x;
        y = dbg_y;
        eprintf(x * 8, y * 14, 5, 0, "----- CASE MAKE -----");
        y++;
        if (dbg_blink & 0x18) {
            eprintf((x - 1) * 8, (y + cursor) * 14, 0x16, 0, ">");
        }
        for (i = 0; i < 6; i++) {
            int col = i == cursor ? 4 : 0;
            eprintf(x * 8, (y + i) * 14, col, 0, "%s", items[i]);
            switch (i) {
            case 0: {
                const char* tbl[24] = {
                    "GAME INIT", "TRIAL VER", "STAGE 1-1", "STAGE 1-2", "STAGE 2-1", "STAGE 2-2",
                    "STAGE 3-1", "STAGE 3-2", "SHOOTING1", "SHOOTING2", "INIT+CURE", "STAGE 1DX",
                    "STAGE 2DX", "HANDGUN  ", "XXXXGUN",   "MINE RPG",  "NETA 1",    "NETA 2",
                    "NETA 3",    "NETA 4",    "NETA 5",    "KLAUSER",   "WESKER",    "---------",
                };
                eprintf((x + 11) * 8, (y + i) * 14, 0, 0, "%s", tbl[itemSet]);
                break;
            }
            case 1: {
                const char* tbl[3] = {"-----", "INF", "INF+RELOAD"};
                eprintf((x + 11) * 8, (y + i) * 14, 0, 0, "%s", tbl[bullet]);
                break;
            }
            case 2: {
                const char* tbl[4] = {"SMALL", "MEDIUM", "LARGE", "HUGE"};
                eprintf((x + 11) * 8, (y + i) * 14, 0, 0, "%s", tbl[(s8) wk->x2AF]);
                break;
            }
            case 3: {
                int c = i == cursor ? 0x16 : 0;
                eprintf((x + 11) * 8, (y + i) * 14, c, 0, "%d", pG->x4F98);
                break;
            }
            case 4: {
                cMap* m = MapMgr.getWork(3);
                eprintf((x + 11) * 8, (y + i) * 14, col, 0, "%.3f", m->rot.x);
                break;
            }
            case 5: {
                const char* tbl[4] = {"UP", "DOWN", "LEFT", "RIGHT"};
                eprintf((x + 11) * 8, (y + i) * 14, 0, 0, "%s", tbl[g_boss_bar_flag]);
                break;
            }
            }
        }
    }
}

template <class T>
int cManager<T>::dispWorkNum(int x, int y, int col, int sub)
{
    u32 n;
    u32 i;

    if ((u32) pArray < 0x80000000 || (u32) pArray > 0x82FFFFFF) {
        return 0;
    }
    n = 0;
    for (i = 0; i < nArray; i++) {
        T* p = (T*) ((u8*) pArray + size * i);
        if (p->be_flag & 0x601) {
            n++;
        }
    }
    eprintf(x, y, 0, col, "%3d/%3d/%4d", n - sub, maxAlive - sub, nArray - sub);
    return n;
}
