#include "types.h"
#include "vec.h"
#include "atari.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "camera.h"
#include "model.h"
#include "player.h"
#include "t_util.h"

extern "C" void* memset(void* dst, int c, unsigned int n);

int lifeLevel(int levels, s16 max, int base);
void DrawGage(int x, int y, int h, int w, int val, int max, int color);
void CamStick2World(Camera* cam, JOY* joy, Vec* out);
extern "C" void Draw_pos(Vec* pos, int size);  // dbmodule.cpp, C linkage (the DOL symbol is `Draw_pos`)

extern u8 PlKaiou;
extern cModel* pSUB;

// Bug-check (cheat) menu tool.
class cToolBugcheck {
public:
    u32 stop_bak;  // 0x00  saved pG stop flags
    u16 x;         // 0x04  menu position
    u16 y;         // 0x06
    u16 mx;        // 0x08  position used for this frame's drawing
    u16 my;        // 0x0A
    s8 cursor;     // 0x0C
    u8 pad_D[3];

    void init();
    void main();
    void exit();
    static void menuPosMove();
    static void menuLife();
    void menu();
};

cToolBugcheck BC;

#define ROUND(x) ((int) ((x) + 0.5f))

void ToolBugcheck()
{
    BC.main();
}

void cToolBugcheck::init()
{
    BitSet(stop_bak, TOOL_FLAG(OFS_STOP_FLG));
    BitOn(TOOL_FLAG(OFS_STOP_FLG), ~0x4000);
    x = 80;
    y = 60;
    cursor = 0;
}

void cToolBugcheck::main()
{
    init();
    while (!(Joy[0].trg & JOY_B)) {
        if (Joy[0].on & 0x200000) {
            x += 8;
        }
        if (Joy[0].on & 0x100000) {
            x -= 8;
        }
        if (Joy[0].on & 0x400000) {
            y += 8;
        }
        if (Joy[0].on & 0x800000) {
            y -= 8;
        }
        mx = x;
        my = y;
        menu();
        TaskSleep(1);
    }
    exit();
}

void cToolBugcheck::exit()
{
    TOOL_FLAG(OFS_DEBUG_FLG) &= 0x7FFFFFFF;
    TOOL_FLAG(OFS_STOP_FLG) = stop_bak;
    TaskExit();
}

void cToolBugcheck::menuPosMove()
{
    f32 speed;
    f32 floor;
    cModel* pl;

    BitOff(TOOL_FLAG(OFS_STOP_FLG), 0x10000000);
    BitOff(TOOL_FLAG(OFS_STOP_FLG), 0x40000000);
    while (1) {
        if (Joy[0].on & JOY_X) {
            TOOL_FLAG(OFS_DEBUG_FLG + 8) |= 8;
        } else {
            TOOL_FLAG(OFS_DEBUG_FLG + 8) &= ~8;
        }
        speed = (Joy[0].on & JOY_A) ? 8.0f : 2.0f;
        Vec v = {0.0f, 0.0f, 0.0f};
        JOY joy = Joy[0];
        joy.sx = 0;
        CamStick2World(&pG->Cam, &joy, &v);
        PSVECScale(&v, &v, speed);
        v.y = 0.0f;
        if (Joy[0].on & 0x10000) {
            pPL->rot.y += 0.13962634f;
        }
        if (Joy[0].on & 0x20000) {
            pPL->rot.y -= 0.13962634f;
        }
        floor = SatMgr.getFloor(&pPL->pos, 600.0f, 100000.0f, 0, 0);
        if (pPL->pos.y > floor + 500.0f || (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 8)) {
            v.y = v.y + speed * (f32) (int) Joy[0].trigR - speed * (f32) (int) Joy[0].trigL;
        }
        PSVECAdd(&pPL->pos, &v, &pPL->pos);
        pl = pPL;
        pl->setPos(&pl->pos);
        {
            // COMPILER-DIFF: candidate (combine): the `&pl->rot` pseudo must reach sched1 uncombined
            // (target `addi r4,r30,160; mr r3,r30`). A non-volatile asm that reads it and launders
            // `pl` is a second use that blocks combine, is issued before the r4 move (so P is tied to
            // r4) and leaves sched2's dependent counts of the setPos arg moves equal; it is also the
            // extra loop insn at global-alloc time that orders the 14 hoisted highs r14-r25.
            Vec* pr = &pl->rot;
            asm("" : "+r"(pl) : "r"(pr));
            pl->setAng(pr);
        }
        {
            // COMPILER-DIFF: candidate (gcse hash bucket of the .LC label name): one more constant
            // pool label before "X:%.0f" gives the target's hoisted-high pseudo order.
            f32 lc0 = 1.0f;
        }
        eprintf(32, 56, 0, 0, "X:%.0f", pPL->pos.x);
        eprintf(32, 70, 0, 0, "Y:%.0f", pPL->pos.y);
        eprintf(32, 84, 0, 0, "Z:%.0f", pPL->pos.z);
        eprintf(32, 98, 0, 0, "R:%.2f", pPL->rot.y);
        eprintf(32, 126, (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 8) ? 0 : 0x14, 0, "[X]:SCR_NO_HIT");
        eprintf(32, 140, 0, 0, "[A]:SPPED_UP");
        eprintf(32, 154, 0, 0, "[L]:POS_UP");
        eprintf(32, 168, 0, 0, "[R]:POS_DOWN");
        eprintf(32, 182, 0, 0, "[Z]:CALL_ASHLEY");
        Draw_pos(&pPL->pos, 2000);
        if ((Joy[0].on & JOY_Z) && pSUB != NULL && (pSUB->be_flag & 0x201) == 1) {
            pSUB->setPos(&pPL->pos);
        }
        if (Joy[0].trg & JOY_B) {
            break;
        }
        TaskSleep(1);
    }
    TOOL_FLAG(OFS_DEBUG_FLG + 8) &= ~8;
    BitOn(TOOL_FLAG(OFS_STOP_FLG), 0x10000000);
    BitOn(TOOL_FLAG(OFS_STOP_FLG), 0x40000000);
}

void cToolBugcheck::menuLife()
{
    int cur = 0;
    int n;
    int lv;

    {
        // COMPILER-DIFF: candidate (gcse hash bucket of the .LC label name): the nine hoisted string
        // highs are PRE reaching regs numbered in expr-hash bucket order of "*.LCn" ((7933 + h) % 183
        // buckets, h = h*129 + c); global-alloc ties (equal priority) go by that number. Thirteen dead
        // labels put the strings at .LC42-.LC50: the decade step 49 -> 50 wraps ">" (bucket 48) below
        // ASHLEY..L-TRIG (106-111), so ">" is allocated first at the shared priority 46 (r24) and the
        // six follow in eprintf order (r23-r18); LIFE/PLAYER (.LC42/43) sit at 45 (see the loop end).
        f32 lc1 = 1.0f;
        f32 lc2 = 2.0f;
        f32 lc3 = 3.0f;
        f32 lc4 = 4.0f;
        f32 lc5 = 5.0f;
        f32 lc6 = 6.0f;
        f32 lc7 = 7.0f;
        f32 lc8 = 8.0f;
        f32 lc9 = 9.0f;
        f32 lc10 = 10.0f;
        f32 lc11 = 11.0f;
        f32 lc12 = 12.0f;
        f32 lc13 = 13.0f;
    }
    while (1) {
        if (Joy[0].trg & JOY_UP) {
            cur--;
        }
        if (Joy[0].trg & JOY_DOWN) {
            cur++;
        }
        if (cur >= 0) {
            n = cur;
            if (n > 2) {
                n = 2;
            }
        } else {
            n = 0;
        }
        cur = n;
        switch (n) {
        case 0:
            TOOL_HALF(OFS_PL_LIFE) += (s16) (Joy[0].sx * 0.4f);
            if (Joy[0].on & JOY_RIGHT) {
                TOOL_HALF(OFS_PL_LIFE) += 25;
            }
            if (Joy[0].on & JOY_LEFT) {
                TOOL_HALF(OFS_PL_LIFE) -= 25;
            }
            TOOL_HALF(OFS_PL_LIFE) = (s16) TOOL_HALF(OFS_PL_LIFE) < 0 ? 0 : ((s16) TOOL_HALF(OFS_PL_LIFE) > (s16) TOOL_HALF(OFS_PL_LIFE_MAX) ? TOOL_HALF(OFS_PL_LIFE_MAX) : TOOL_HALF(OFS_PL_LIFE));
            if (Joy[0].rep2 & (JOY_R | JOY_L)) {
                lv = lifeLevel(20, TOOL_HALF(OFS_PL_LIFE_MAX), 1200);
                if (Joy[0].rep2 & JOY_R) {
                    lv++;
                }
                if (Joy[0].rep2 & JOY_L) {
                    lv--;
                }
                if (lv >= 0) {
                    if (lv > 20) {
                        lv = 20;
                    }
                } else {
                    lv = 0;
                }
                TOOL_HALF(OFS_PL_LIFE_MAX) = 1200;
                TOOL_HALF(OFS_PL_LIFE_MAX) += ROUND((f32) (lv * 60));
                TOOL_HALF(OFS_PL_LIFE) = TOOL_HALF(OFS_PL_LIFE_MAX);
            }
            break;
        case 1:
            TOOL_HALF(OFS_SUB_LIFE) += (s16) (Joy[0].sx * 0.4f);
            if (Joy[0].on & JOY_RIGHT) {
                TOOL_HALF(OFS_SUB_LIFE) += 25;
            }
            if (Joy[0].on & JOY_LEFT) {
                TOOL_HALF(OFS_SUB_LIFE) -= 25;
            }
            TOOL_HALF(OFS_SUB_LIFE) = (s16) TOOL_HALF(OFS_SUB_LIFE) < 0 ? 0 : ((s16) TOOL_HALF(OFS_SUB_LIFE) > (s16) TOOL_HALF(OFS_SUB_LIFE_MAX) ? TOOL_HALF(OFS_SUB_LIFE_MAX) : TOOL_HALF(OFS_SUB_LIFE));
            if (Joy[0].rep2 & (JOY_R | JOY_L)) {
                lv = lifeLevel(5, TOOL_HALF(OFS_SUB_LIFE_MAX), 600);
                if (Joy[0].rep2 & JOY_R) {
                    lv++;
                }
                if (Joy[0].rep2 & JOY_L) {
                    lv--;
                }
                if (lv >= 0) {
                    if (lv > 5) {
                        lv = 5;
                    }
                } else {
                    lv = 0;
                }
                TOOL_HALF(OFS_SUB_LIFE_MAX) = 600;
                TOOL_HALF(OFS_SUB_LIFE_MAX) += ROUND((f32) (lv * 120));
                TOOL_HALF(OFS_SUB_LIFE) = TOOL_HALF(OFS_SUB_LIFE_MAX);
            }
            break;
        case 2:
            if (Joy[0].trg & JOY_A) {
                TOOL_HALF(OFS_PL_LIFE) = TOOL_HALF(OFS_PL_LIFE_MAX);
                TOOL_HALF(OFS_SUB_LIFE) = TOOL_HALF(OFS_SUB_LIFE_MAX);
            }
            break;
        }
        eprintf(48, 56, 4, 0, "LIFE");
        lv = lifeLevel(20, TOOL_HALF(OFS_PL_LIFE_MAX), 1200);
        eprintf(48, 70, 0, 0, "PLAYER:%4d/%4d[%2d]", (s16) TOOL_HALF(OFS_PL_LIFE), (s16) TOOL_HALF(OFS_PL_LIFE_MAX), lv);
        DrawGage(216, 70, 8, 100, (s16) TOOL_HALF(OFS_PL_LIFE), (s16) TOOL_HALF(OFS_PL_LIFE_MAX), -1);
        lv = lifeLevel(5, TOOL_HALF(OFS_SUB_LIFE_MAX), 600);
        eprintf(48, 84, 0, 0, "ASHLEY:%4d/%4d[%2d]", (s16) TOOL_HALF(OFS_SUB_LIFE), (s16) TOOL_HALF(OFS_SUB_LIFE_MAX), lv);
        DrawGage(216, 84, 8, 100, (s16) TOOL_HALF(OFS_SUB_LIFE), (s16) TOOL_HALF(OFS_SUB_LIFE_MAX), -1);
        eprintf(48, 98, 0, 0, "LIFE MAX");
        eprintf(64, 126, 6, 0, "STICK-R or JOY-R life up");
        eprintf(64, 140, 6, 0, "STICK-L or JOY_L life down");
        eprintf(64, 154, 6, 0, "R-TRIGGER life max up");
        eprintf(64, 168, 6, 0, "L-TRIGGER life max down");
        eprintf(40, (n + 5) * 14, 0, 0, ">");
        if (Joy[0].trg & JOY_B) {
            break;
        }
        TaskSleep(1);
        // COMPILER-DIFF: tie (global-alloc live length): two more insns in the loop at global-alloc
        // time (+4 REG_LIVE_LENGTH for every loop-invariant pseudo). The nine string highs are born
        // in eprintf order in the preheader (2 per insn): LIFE 656 and PLAYER 654 drop to priority
        // int(30000/len) = 45 (allocated last, in bucket order: LIFE r17, PLAYER r16) while
        // ASHLEY..L-TRIG (652-642) and ">" (640) share 46. The second anchor must not be the same
        // asm again (cse deletes the repeated store as redundant).
        asm("" : "=m"(PlKaiou));
        asm("" : "+m"(PlKaiou));
    }
}

void cToolBugcheck::menu()
{
    static TOOL_MENU menu[18] = {
        {1, "WEP_MUGEN", NULL},
        {1, "PL_SPEED", NULL},
        {1, "PL_NO_DEATH", NULL},
        {1, "EM_NO_DEATH", NULL},
        {1, "POS_MOVE", menuPosMove},
        {1, "LIFE", menuLife},
        {1, "OBJ_OBJ_AT_DISP", NULL},
        {1, "OBJ_SCR_AT_DISP", NULL},
        {1, "SCROLL_AT_DISP", NULL},
        {1, "EFFECT_AT_DISP", NULL},
        {1, "EVENT_AT_DISP", NULL},
        {1, "SHOP_ALL", NULL},
        {1, "BGM_STOP", NULL},
        {1, "SE_STOP", NULL},
        {1, "OBJ_NO_DISP", NULL},
        {1, "SCR_NO_DISP", NULL},
        {1, "EM_QUICK_DEAD", NULL},
        {1, "EM_LIFE_DISP", NULL},
    };
    static const char* wep_mugen_str[3] = {"OFF", "MUGEN", "MUGEN+RELOAD"};
    static const char* pl_speed_str[5] = {"OFF", "x2", "x3", "x4", "x5"};
    s16 px = mx;
    s16 py = my;
    int i;

    ToolMenuDisp_cur(px, py, 0, &cursor, menu, sizeof(menu), Joy);
    switch (cursor) {
    case 0:
        if (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 0x400000) {
            i = 1;
        } else if (TOOL_FLAG(OFS_DEBUG_FLG + 12) & 0x80000000) {
            i = 2;
        } else {
            i = 0;
        }
        if (Joy[0].rep & 0x20002) {
            i++;
        }
        if (Joy[0].rep & 0x10001) {
            i--;
        }
        if (Joy[0].rep & JOY_A) {
            i++;
        }
        i = i < 0 ? 2 : (i > 2 ? 0 : i);
        TOOL_FLAG(OFS_DEBUG_FLG + 8) &= ~0x400000;
        TOOL_FLAG(OFS_DEBUG_FLG + 12) &= 0x7FFFFFFF;
        switch (i) {
        case 0:
            break;
        case 1:
            TOOL_FLAG(OFS_DEBUG_FLG + 8) |= 0x400000;
            break;
        case 2:
            TOOL_FLAG(OFS_DEBUG_FLG + 12) |= 0x80000000;
            break;
        }
        break;
    case 1:
        if (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 0x10000) {
            i = PlKaiou + 1;
        } else {
            i = 0;
        }
        if (Joy[0].rep & 0x20002) {
            i++;
        }
        if (Joy[0].rep & 0x10001) {
            i--;
        }
        if (Joy[0].rep & JOY_A) {
            i++;
        }
        i = i < 0 ? 4 : (i > 4 ? 0 : i);
        if (i == 0) {
            TOOL_FLAG(OFS_DEBUG_FLG + 8) &= ~0x10000;
        } else {
            TOOL_FLAG(OFS_DEBUG_FLG + 8) |= 0x10000;
            PlKaiou = i - 1;
        }
        break;
    case 2:
        if (Joy[0].trg & 0x30103) {
            TOOL_FLAG(OFS_DEBUG_FLG + 8) ^= 0x800000;
        }
        break;
    case 3:
        if (Joy[0].trg & 0x30103) {
            TOOL_FLAG(OFS_DEBUG_FLG + 8) ^= 0x20000;
        }
        break;
    case 4:
    case 5:
        break;
    case 6:
        if (Joy[0].trg & 0x30103) {
            TOOL_FLAG(OFS_DEBUG_FLG + 8) ^= 0x10000000;
        }
        break;
    case 7:
        if (Joy[0].trg & 0x30103) {
            TOOL_FLAG(OFS_DEBUG_FLG + 8) ^= 0x20000000;
        }
        break;
    case 8:
        if (Joy[0].trg & 0x30103) {
            TOOL_FLAG(OFS_DEBUG_FLG) ^= 0x8000000;
        }
        break;
    case 9:
        if (Joy[0].trg & 0x30103) {
            TOOL_FLAG(OFS_DEBUG_FLG) ^= 0x4000000;
        }
        break;
    case 10:
        if (Joy[0].trg & 0x30103) {
            TOOL_FLAG(OFS_DEBUG_FLG) ^= 0x400000;
        }
        break;
    case 11:
        if (Joy[0].trg & 0x30103) {
            TOOL_FLAG(OFS_DEBUG_FLG + 12) ^= 0x10;
        }
        break;
    case 12:
        if (Joy[0].trg & 0x30103) {
            TOOL_FLAG(OFS_DEBUG_FLG + 8) ^= 0x100000;
        }
        break;
    case 13:
        if (Joy[0].trg & 0x30103) {
            TOOL_FLAG(OFS_DEBUG_FLG + 8) ^= 0x80000;
        }
        break;
    case 14:
        if (Joy[0].trg & 0x30103) {
            TOOL_FLAG(OFS_DISP_FLG) ^= 0x40000000;
            if (TOOL_FLAG(OFS_DISP_FLG) & 0x40000000) {
                TOOL_FLAG(OFS_DISP_FLG) |= 0x80000000;
                TOOL_FLAG(OFS_DISP_FLG) |= 0x10000000;
                TOOL_FLAG(OFS_STOP_FLG) |= 0x20000000;
                TOOL_FLAG(OFS_STOP_FLG) |= 0x4000000;
            } else {
                TOOL_FLAG(OFS_DISP_FLG) &= ~0x80000000;
                TOOL_FLAG(OFS_DISP_FLG) &= ~0x10000000;
                TOOL_FLAG(OFS_STOP_FLG) &= ~0x20000000;
                TOOL_FLAG(OFS_STOP_FLG) &= ~0x4000000;
            }
        }
        break;
    case 15:
        if (Joy[0].trg & 0x30103) {
            TOOL_FLAG(OFS_DISP_FLG) ^= 0x8000000;
        }
        break;
    case 16:
        if (Joy[0].trg & 0x30103) {
            TOOL_FLAG(OFS_DEBUG_FLG + 8) ^= 0x2000;
        }
        break;
    case 17:
        if (Joy[0].trg & 0x30103) {
            TOOL_FLAG(OFS_DEBUG_FLG + 8) ^= 0x1000;
        }
        break;
    }

    px += 128;
    if (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 0x400000) {
        i = 1;
    } else if (TOOL_FLAG(OFS_DEBUG_FLG + 12) & 0x80000000) {
        i = 2;
    } else {
        i = 0;
    }
    eprintf(px, py, 0, 0, "%s", i >= 0 && i <= 2 ? wep_mugen_str[i] : "...no string");
    py += 16;
    if (!(TOOL_FLAG(OFS_DEBUG_FLG + 8) & 0x10000)) {
        i = 0;
    } else {
        i = PlKaiou + 1;
    }
    eprintf(px, py, 0, 0, "%s", i >= 0 && i <= 4 ? pl_speed_str[i] : "...no string");
    py += 16;
    eprintf(px, py, 0, 0, "%s", (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 0x800000) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 0x20000) ? "ON" : "OFF");
    py += 48;
    eprintf(px, py, 0, 0, "%s", (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 0x10000000) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 0x20000000) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", (TOOL_FLAG(OFS_DEBUG_FLG) & 0x8000000) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", (TOOL_FLAG(OFS_DEBUG_FLG) & 0x4000000) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", (TOOL_FLAG(OFS_DEBUG_FLG) & 0x400000) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", (TOOL_FLAG(OFS_DEBUG_FLG + 12) & 0x10) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 0x100000) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 0x80000) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", (TOOL_FLAG(OFS_DISP_FLG) & 0x40000000) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", (TOOL_FLAG(OFS_DISP_FLG) & 0x8000000) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 0x2000) ? "ON" : "OFF");
    py += 16;
    eprintf(px, py, 0, 0, "%s", (TOOL_FLAG(OFS_DEBUG_FLG + 8) & 0x1000) ? "ON" : "OFF");
}
