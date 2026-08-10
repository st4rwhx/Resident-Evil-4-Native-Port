// game/debug: debug overlays, process timing bars, debug/config.txt (D:/Bio4/Prog/debug.cpp).
#include "types.h"
#include "global.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "gx.h"
#include "main.h"
#include "main_mem.h"
#include "joy.h"
#include "libgpu.h"
#include "eprintf.h"
#include "datactrl.h"
#include "db_log.h"
#include "db_cam.h"
#include "dvd.h"
#include "snd.h"
#include "debug.h"

void bio4_GXSetCopyClear(GXColor color, u32 z);   // game/gx_sub.cpp
void DbMenuExitAfterCheck();                        // game/db_menu.cpp

extern "C" {
u32 OSGetTick();
void OSReport(const char* fmt, ...);
unsigned int strlen(const char* s);
unsigned int strcspn(const char* s, const char* reject);
int strncmp(const char* a, const char* b, unsigned int n);
long strtol(const char* s, char** end, int base);
void* memset(void* dst, int c, unsigned int n);
}
extern u8 PlMode;       // game/player.cpp
extern u8 PlFormMode;   // game/player.cpp

#define HALT(cond)                                                           \
    if (cond) {                                                              \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);                        \
        *(volatile u32*) 0x11111111 = 0;                                     \
    }

// 0x20-byte tile primitive (TILE padded to the array stride used by the debug bars)
struct DbgTile {
    u32 tag;          // 0x00
    u32 code;         // 0x04
    GpuColor c0;      // 0x08
    s16 x0, y0;       // 0x0C
    s16 w, h;         // 0x10
    s16 z0;           // 0x14
    u8 pad_16[0xA];
};

static u32 proc_tick[32];
const char* proc_name[32];   // not declared in debug.h: .bss order proc_tick, proc_name
u32 zero_tick;
int proc_tick_idx;
int proc_tick_idx_bak;
int g_proc_cnt;

#define OS_BUS_CLOCK (((OSClock*) 0x80000000)->busClock)
struct OSClock {
    u8 pad_0[0xF8];
    u32 busClock;   // 0xF8
};

#line 66 "D:/Bio4/Prog/debug.cpp"

void DebugControl()
{
    if ((pG->flags_68 & 0x40000000) && pG->debug_mode) {
        MemCheckUsedHeap();
        processBarDisp();
        PrimitiveBuffDisp();
        DC.dispDebug();
    }
    if ((pG->flags_68 & 0x8000) && pG->debug_mode) {
        debugPadInfoDisp();
    }
    HALT(Joy[2].on == 0x1600);
    DbMenuExitAfterCheck();
}

void debugPadInfoDisp()
{
    u32 i;
    u32 on = Joy[0].on;
    u32 trg = Joy[0].trg;

    // `i * 6 + 10` at all three sites: the two single-use givs of the if/else arms are reduced
    // separately (two `li 10` / `addi 6`), the third one (short lifetime in a long loop) is not
    // (`mulli; addi` in the loop), and the arms cross-jump their `crclr; bl`.
    for (i = 0; i < 32; i++) {
        if (on & (0x80000000 >> i)) {
            eprintf2(6, 12, i * 6 + 10, 2, 4, 1, "I");
        } else {
            eprintf2(6, 12, i * 6 + 10, 2, 0, 1, "_");
        }
        if (trg & (0x80000000 >> i)) {
            eprintf2(6, 12, i * 6 + 10, 2, 6, 1, "o");
        }
    }
}

// A macro, not an inline: the fourth bar's `TICKX(..) - x0` is stored as s16 directly (the front-end
// shortens the subtraction to HImode, `subf` on the raw lhz); an inline's s16 return value is
// sign-extended first (`extsh`) whatever the caller does with it.
#define TICKX(tick) ((s16) ((f32) (tick) / total * 400.0f))

// Progressive (60Hz) screen: the 400-line bars are squashed to 300 lines below y = 56.
#define PROG_Y(y) ((s16) ((f32) (s16) (y) / 1.3333334f + 56.0f))
#define PROG_H(h) ((s16) ((f32) (s16) (h) / 1.3333334f))
// Reference read of pSys: the load stays below the preceding tile stores.
static inline SystemWork* SysRef(SystemWork*& p) { return p; }
// Ticks -> 1/100 frame units (bus clock / 4 = tick rate, 60 frames per second)
#define TICK_100F(t) ((f32) (t) * 60.0f / (f32) (clk->busClock >> 2) * 100.0f)
#define TICK_1000F(t) ((f32) (t) * 60.0f / (f32) (clk->busClock >> 2) * 1000.0f)

void processBarDisp()
{
    static DbgTile tile[6];
    static u8 cnt;
    static u8 xchr[5] = "-/l/";  // u8: passed to %c without extsb
    int vcnt = GetSystemVcnt();
    u32 frameTick = OS_BUS_CLOCK / 240 * vcnt;
    f32 total;
    OSClock* clk;
    DbgTile* t = tile;
    s16 x0;
    s16 x1;
    s16 x2;
    // The bar width, declared here and assigned in tile 1: the pseudo of the shared `5` is created at
    // the declaration (below x2, above tile 1's `4`) while its `li` keeps tile 1's statement position,
    // so on the global-alloc priority tie with the `4` (7 refs, len 498 both) the `5` is allocated
    // first (r20) and the `4` second (r19). `s16 w = 5;` here instead moves the `li` up: 34 words.
    s16 w;
    u32 i;

    total = (f32) frameTick;
    x0 = TICKX(proc_tick[4]);
    t->z0 = 0;
    // COMPILER-DIFF: cse class head of the zero. Tile 1's `z0 = 0` stays a single-use pseudo
    // (`li r0,0` next to its sth) and tiles 2-6 share this one (`li r17,0`): the original had a
    // zero-valued variable assigned here and read after the if/else; which one is unknown, `zz`
    // stands in for it (its read below folds back to `li 0`, no code).
    s16 zz = 0;
    // clk's `lis 0x8000` (a block-0 filler at sched1) follows the zero's `li` in the target: LUID order.
    clk = (OSClock*) 0x80000000;
    t->code = 4;
    t->x0 = 6;
    t->y0 = 30;
    w = 5;
    t->w = w;
    t->c0.r = 0x80;
    t->c0.g = 0x80;
    t->c0.b = 0x20;
    t->c0.cd = 0xFF;
    t->h = x0;
    if (SysRef(pSys)->flags & 0x40000000) {
        t->y0 = PROG_Y(30);
        t->h = PROG_H(t->h);
    }
    AddPrim(&MainOt[1], (u32*) t);
    t++;

    x1 = TICKX(proc_tick[1]);
    t->code = 4;
    t->x0 = 6;
    t->y0 = x0 + 30;
    t->z0 = 0;
    t->w = 5;
    t->c0.r = 0x80;
    t->c0.g = 0x20;
    t->c0.b = 0x20;
    t->c0.cd = 0xFF;
    t->h = x1 - x0;
    if (SysRef(pSys)->flags & 0x40000000) {
        t->y0 = PROG_Y(t->y0);
        t->h = PROG_H(t->h);
    }
    AddPrim(&MainOt[1], (u32*) t);
    t++;

    x2 = TICKX(proc_tick[2]);
    t->y0 = x0 + 30;
    t->code = 4;
    t->x0 = 12;
    t->z0 = 0;
    t->w = 5;
    t->c0.r = 0x20;
    t->c0.g = 0x20;
    t->c0.b = 0x80;
    t->c0.cd = 0xFF;
    t->h = x2 - x0;
    if (SysRef(pSys)->flags & 0x40000000) {
        t->y0 = PROG_Y(t->y0);
        t->h = PROG_H(t->h);
    }
    AddPrim(&MainOt[1], (u32*) t);
    t++;

    // The fourth bar's base is x0 reused as the max of x1/x2, in the if/else spelling: jump1 hoists
    // the else arm (`mr x0,x2; cmpw x1,x2; ble; mr x0,x1`) with the compare still on x2, and x0
    // (10 refs, len ~133) is allocated right after x2 (r29) in pass 0 among the call-crossing
    // registers already in use: r28 (the join block's 0x8000 high), which is what the target has
    // (`mr r28,r29 .. subf r0,r28,r0`). A separate `x3` variable (4 refs, len 29, no call) took the
    // first free caller-saved register r8 instead; `x0 = x2; if (x1 > x2) x0 = x1;` lets cse
    // rewrite the compare onto x0 (121 words).
    if (x1 > x2) {
        x0 = x1;
    } else {
        x0 = x2;
    }
    t->y0 = x0 + 30;
    t->c0.g = 0x80;
    t->code = 4;
    t->x0 = 6;
    t->z0 = 0;
    t->w = 5;
    t->c0.r = 0x20;
    t->c0.b = 0x20;
    t->c0.cd = 0xFF;
    t->h = TICKX(proc_tick[3]) - x0;
    if (SysRef(pSys)->flags & 0x40000000) {
        t->y0 = PROG_Y(t->y0);
        t->h = PROG_H(t->h);
    }
    AddPrim(&MainOt[1], (u32*) t);
    t++;

    t->x0 = 6;
    t->code = 4;
    t->y0 = 30;
    t->z0 = 0;
    t->w = 5;
    t->h = 400;
    t->c0.r = 8;
    t->c0.g = 8;
    t->c0.b = 0x20;
    t->c0.cd = 0xFF;
    if (SysRef(pSys)->flags & 0x40000000) {
        t->y0 = 78;
        t->h = 300;
    }
    AddPrim(&MainOt[1], (u32*) t);
    t++;

    t->code = 4;
    t->x0 = 12;
    t->y0 = 30;
    t->z0 = 0;
    t->w = 5;
    t->h = 400;
    t->c0.r = 8;   // r, g, b, cd: the target's `stb g, b, cd, r` is this LUID order (the 8 dies at g)
    t->c0.g = 8;
    t->c0.b = 0x20;
    t->c0.cd = 0xFF;
    if (SysRef(pSys)->flags & 0x40000000) {
        t->y0 = 78;
        t->h = 300;
    }
    AddPrim(&MainOt[1], (u32*) t);

    if (proc_tick[2] > proc_tick[1]) {
        eprintf2(10, 16, 12, 400, 0, 1, "%4.0f", TICK_100F(proc_tick[3]));
        eprintf2(10, 16, 12, 416, 0, 1, "%4.0f", TICK_100F(proc_tick[1]));
    } else {
        eprintf2(10, 16, 12, 400, 0, 1, "%4.0f", TICK_100F(proc_tick[2]));
        eprintf2(10, 16, 12, 416, 0, 1, "%4.0f", TICK_100F(proc_tick[3]));
    }
    // The two reads after the if/else go through the OS_BUS_CLOCK constant, not `clk`: the join
    // block re-materialises the 0x8000 high into its own call-crossing register (`lis r28`) while
    // the arms and the loop keep the block-0 `clk` (r14).
    eprintf2(10, 16, zz, 16, 0, 13, "%4.0f", (f32) proc_tick[3] * 60.0f / (f32) (OS_BUS_CLOCK >> 2) * 100.0f);
    g_proc_cnt = (u32) ((f32) proc_tick[3] * 60.0f / (f32) (OS_BUS_CLOCK >> 2) * 100.0f);
    eprintf2(10, 16, 42, 28, 0, 2, "1000/F");
    for (i = 5; i < proc_tick_idx_bak + 5; i++) {
        // COMPILER-DIFF: loop.c insn_count stand-in. The "%5.0f %s" format high is the loop's last
        // movable (threshold 71 - 3 * 8 moved = 47, savings 1, life 1); our body has exactly 47 real
        // insns at loop pass 1, so it is hoisted there, before the giv inits (`addi r26; li r28; li
        // r27`). The target hoists it in pass 2 (after the giv inits): its pass-1 body had >= 48
        // insns. This dead test (load, compare, branch, store: +4; the store is dead by liveness and
        // goes in flow, the test in jump2) emits no code; the real extra statement is unknown.
        if (proc_tick_idx == 0) {
            x0 = 0;
        }
        eprintf2(10, 16, 32, 50 + (i - 5) * 16, 0, 2, "%5.0f %s", TICK_1000F(proc_tick[i] - proc_tick[i - 1]), proc_name[i]);
    }
    cnt = (cnt + 1) & 3;
    eprintf2(14, 14, 10, 18, 0, 0, "%c", xchr[cnt]);
}

// Both arms store proc_tick first: the two independent `stwx` have equal priority and sched1 issues
// the one with more dying registers first -- the LAST store in RTL order owns the index register's
// death, so the target's `stwx name` before `stwx tick` means the tick store came first in source.
void ProcessTickGet(int no, const char* name)
{
    if ((u32) no <= 4) {
        proc_tick[no] = OSGetTick() - zero_tick;
        proc_name[no] = name;
    } else {
        proc_tick[proc_tick_idx + 5] = OSGetTick() - zero_tick;
        proc_name[proc_tick_idx + 5] = name;
        proc_tick_idx++;
    }
}

void ProcessTickInit()
{
    zero_tick = OSGetTick();
    proc_tick_idx_bak = proc_tick_idx;
    proc_tick_idx = 0;
}

// The primitive buffer statistics live in the big GX block at pG + 0x184 in the original
struct PrimBuffView {
    u8 pad_0[0x4F10 - 0x184];
    s32 base;   // pG->prim_base
    f32 rate;   // pG->prim_rate
};

void PrimitiveBuffDisp()
{
    static DbgTile tile[6];
    DbgTile* t;
    int max = pG->prim_max;
    PrimBuffView* pb;
    f32 rate;

    if (max == 0) {
        return;
    }
    pb = (PrimBuffView*) &pG->gxStage;
    rate = 1.0f - (f32) (int) (pG->prim_cnt + max * (pG->vtx_buf_no + 1) - pb->base) / (f32) max;
    t = tile;
    if (pb->rate < rate) {
        pb->rate = rate;
    }
    if (rate > 0.9f && !(pG->flags_64 & 0x00200000)) {
        pLog->warn(0, 0, "PrimitiveBuff :  work remain under 1/10");
    }

    {
        s16 width = 200;

        t->code = 4;
        t->x0 = (s16) (pb->rate * (f32) width + (f32) width);
        t->y0 = 24;
        t->w = 4;
        t->h = 4;
        t->c0.r = 0x80;
        t->c0.g = 0x14;
        t->c0.b = 0x14;
        t->c0.cd = 0xFF;
        t->z0 = 0;
        int z = 0;
        if (SysRef(pSys)->flags & 0x40000000) {
            t->y0 = 74;
            t->h = 3;
        }
        AddPrim(&MainOt[1], (u32*) t);
        t++;

        t->w = (s16) (rate * (f32) width);
        t->c0.b = 0x80;
        t->code = 4;
        t->x0 = width;
        t->y0 = 24;
        t->z0 = z;
        t->h = 4;
        t->c0.r = 0x14;
        t->c0.g = 0x14;
        t->c0.cd = 0xFF;
        if (SysRef(pSys)->flags & 0x40000000) {
            t->y0 = 74;
            t->h = 3;
        }
        AddPrim(&MainOt[1], (u32*) t);
        t++;

        t->y0 = 24;
        t->z0 = z;
        t->code = 4;
        t->x0 = width;
        t->c0.r = 0x14;
        t->c0.g = 0x14;
        t->w = width;
        t->h = 4;
        t->c0.b = 0x14;
        t->c0.cd = 0xFF;
        if (SysRef(pSys)->flags & 0x40000000) {
            t->y0 = 74;
            t->h = 3;
        }
        AddPrim(&MainOt[1], (u32*) t);
    }
}

static inline void KeyTypeSet(int v) { CamDbg.key_type = v; }   // the SCR store: its `li 1` precedes the CamDbg address (life 3), so loop.c hoists the shared 1 (see docs/research/ "DOL debug/db_cam closer")
#define CFG_ON(p) (strncmp(p, "ON", 2) == 0)
#define CFG_OFF3(p) (strncmp(p, "OFF", 3) == 0)

#line 817 "D:/Bio4/Prog/debug.cpp"
void ConfigSet()
{
    int size = 0;
    char* buf;
    char* end;
    char* p;
    int req;
    int ret;   // unused: the original's tests are plain `if (symbol_check(..))`; the line keeps __LINE__

    BitOn(pG->flags_68, 0x40000000);
    BitOn(pG->flags_68, 0x8000);
    BitOff(pG->flags_6C, 0x08000000);
    PlMode = 0;
    PlFormMode = 0;
    pG->x4F9F = 0;
    buf = (char*) MEM_CALLOC(0x1000, 1, 13);
    req = DvdReadN("debug/config.txt", buf, 0, 0, 0, 0x11, __FILE__, __LINE__);
    if (Dvd.ReadCheck(req, &size, 0, 0) < 0) {
        Mem_free(buf);
        return;
    }
    p = buf;
    end = buf + size;
    while (p < end) {
        p = space_skip(p);
        if (*p++ != '[') {
            continue;
        }
        if (symbol_check(&p, "USER")) {

            int i = 0;
            while (*p != '\r') {
                pUser_name[i++] = *p;
                p++;
            }
        } else if (symbol_check(&p, "BRIGHTNESS")) {
            if (pRK->brightness == 0) {
                pRK->brightness = num_get(&p);
            }
        } else if (symbol_check(&p, "STAGE")) {
            pG->stage_no = num_get(&p);
        } else if (symbol_check(&p, "ROOM")) {
            pG->room_no = num_get(&p);
        } else if (symbol_check(&p, "JUMP_POINT")) {
            pG->x4F9F = num_get(&p);
        } else if (symbol_check(&p, "PRINT_PAGE")) {
            pG->debug_mode = num_get(&p);
            pG->debug_disp = -1;
        } else if (symbol_check(&p, "PLAYER")) {
            pG->x4FB8 = num_get(&p);
        } else if (symbol_check(&p, "BGM")) {
            if (CFG_OFF3(p)) {
                BitOn(pG->flags_68, 0x00100000);
            } else {
                BitOff(pG->flags_68, 0x00100000);
            }
        } else if (symbol_check(&p, "SE")) {
            if (CFG_OFF3(p)) {
                BitOn(pG->flags_68, 0x00080000);
            } else {
                BitOff(pG->flags_68, 0x00080000);
            }
        } else if (symbol_check(&p, "SCENARIO")) {
            if (CFG_OFF3(p)) {
                BitOn(pG->flags_68, 0x04000000);
            } else {
                BitOff(pG->flags_68, 0x04000000);
            }
        } else if (symbol_check(&p, "NO_ENEMY")) {
            if (CFG_OFF3(p)) {
                BitOff(pG->flags_68, 0x00200000);
            } else {
                BitOn(pG->flags_68, 0x00200000);
            }
        } else if (symbol_check(&p, "ENEMY_SET")) {
            if (CFG_OFF3(p)) {
                BitOn(pG->flags_68, 0x00200000);
            } else {
                BitOff(pG->flags_68, 0x00200000);
            }
        } else if (symbol_check(&p, "ETC_SET")) {
            if (CFG_OFF3(p)) {
                BitOn(pG->flags_6C, 0x800);
            } else {
                BitOff(pG->flags_6C, 0x800);
            }
        } else if (symbol_check(&p, "PROCESS_BAR")) {
            if (CFG_OFF3(p)) {
                BitOff(pG->flags_68, 0x40000000);
                BitOff(pG->flags_68, 0x8000);
            } else {
                BitOn(pG->flags_68, 0x40000000);
                BitOn(pG->flags_68, 0x8000);
            }
        } else if (symbol_check(&p, "VIBRATION")) {
            if (symbol_check(&p, "OFF")) {
                BitOff(pSys->flags, 0x08000000);
            } else {
                BitOn(pSys->flags, 0x08000000);
            }
        } else if (symbol_check(&p, "BG_BLACK")) {
            if (symbol_check(&p, "ON")) {
                GXColor c;
                c.r = c.g = c.b = c.a = 0;
                bio4_GXSetCopyClear(c, 0xFFFFFF);
            }
        } else if (symbol_check(&p, "DBG_CAM_KEY")) {
            if (symbol_check(&p, "DFLT")) {
                CamDbg.key_type = 0;
            } else if (symbol_check(&p, "SCR")) {
                KeyTypeSet(1);
            }
        } else if (symbol_check(&p, "DBG_ESP_DISP")) {
            if (symbol_check(&p, "ON")) {
                BitOn(pG->flags_6C, 0x8000);
            }
        } else if (symbol_check(&p, "GAME_MODE")) {
            if ((u32) ((u8) *p - '0') <= 9) {
                pG->x8354 = num_get(&p);
                if (pG->x8354 > 6) {
                    pG->x8354 = 6;
                }
            } else {
                if (symbol_check(&p, "VERY_EASY")) {
                    pG->x8354 = 1;
                }
                if (symbol_check(&p, "EASY")) {
                    pG->x8354 = 3;
                }
                if (symbol_check(&p, "NORMAL")) {
                    pG->x8354 = 5;
                }
                if (symbol_check(&p, "HARD")) {
                    pG->x8354 = 6;
                }
            }
        } else if (symbol_check(&p, "SOUND_MODE")) {
            int mode = 1;
            if ((u32) ((u8) *p - '0') <= 9) {
                mode = num_get(&p);
                mode = mode < 0 ? 0 : (mode > 2 ? 2 : mode);

            }
            if (symbol_check(&p, "MONO")) {
                mode = 0;
            }
            if (symbol_check(&p, "STEREO")) {
                mode = 1;
            }
            if (symbol_check(&p, "DPL2")) {
                mode = 2;
            }
            if (mode != pSys->sound_mode) {
                SndSetOutputMode(mode, 0);
            }
        } else if (symbol_check(&p, "WARNING_LOG")) {
            if (CFG_ON(p)) {
                BitOff(pG->flags_6C, 0x04000000);
            } else {
                BitOn(pG->flags_6C, 0x04000000);
            }
        } else if (symbol_check(&p, "PLAYER_MODE")) {
            PlMode = num_get(&p);
        } else if (symbol_check(&p, "SN_PC_READ")) {
            if (CFG_ON(p)) {
                BitOn(pG->flags_54, 0x00020000);
            } else {
                BitOff(pG->flags_54, 0x00020000);
            }
        } else if (symbol_check(&p, "SN_PC_READ_TOOL")) {
            if (CFG_ON(p)) {
                BitOn(pG->flags_54, 0x00010000);
            } else {
                BitOff(pG->flags_54, 0x00010000);
            }
        } else if (symbol_check(&p, "NO_DEATH")) {
            if (CFG_ON(p)) {
                BitOn(pG->flags_68, 0x00800000);
            } else {
                BitOff(pG->flags_68, 0x00800000);
            }
        } else if (symbol_check(&p, "OBJ_SERVER")) {
            if (CFG_ON(p)) {
                BitOn(pG->flags_6C, 0x01000000);
            } else {
                BitOff(pG->flags_6C, 0x01000000);
            }
        } else if (symbol_check(&p, "LANGUAGE")) {
            if (symbol_check(&p, "JPN")) {
                pSys->language = 0;
            }
            if (symbol_check(&p, "USA")) {
                pSys->language = 1;
            }
            if (symbol_check(&p, "ENG")) {
                pSys->language = 2;
            }
            if (symbol_check(&p, "GER")) {
                pSys->language = 3;
            }
            if (symbol_check(&p, "FRA")) {
                pSys->language = 4;
            }
            if (symbol_check(&p, "ESP")) {
                pSys->language = 5;
            }
            if (symbol_check(&p, "ITA")) {
                pSys->language = 6;
            }
            if (symbol_check(&p, "KOR")) {
                pSys->language = 7;
            }
        } else if (symbol_check(&p, "PUBLICITY_VER")) {
            if (CFG_ON(p)) {
                BitOn(pG->flags_54, 8);
            } else {
                BitOff(pG->flags_54, 8);
            }
        } else if (symbol_check(&p, "AIM_REVERSE")) {
            if (CFG_ON(p)) {
                BitOn(pSys->flags, 0x80000000);
            } else {
                BitOff(pSys->flags, 0x80000000);
            }
        } else if (symbol_check(&p, "WIDE_MODE")) {
            if (CFG_ON(p)) {
                BitOn(pSys->flags, 0x40000000);
            } else {
                BitOff(pSys->flags, 0x40000000);
            }
        } else if (symbol_check(&p, "AUTO_LOCK_ON")) {
            if (CFG_ON(p)) {
                BitOn(pSys->flags, 0x20000000);
            } else {
                BitOff(pSys->flags, 0x20000000);
            }
        } else if (symbol_check(&p, "SINGLE_DISK")) {
            if (CFG_ON(p)) {
                BitOn(pG->flags_68, 0x02000000);
            } else {
                BitOff(pG->flags_68, 0x02000000);
            }
        } else if (symbol_check(&p, "BUGCHECK_MODE")) {
            if (CFG_ON(p)) {
                BitOn(pG->flags_68, 0x01000000);
            } else {
                BitOff(pG->flags_68, 0x01000000);
            }
        } else if (symbol_check(&p, "LIGHT_CHECK")) {
            if (CFG_ON(p)) {
                BitOn(pG->flags_68, 0x100);
            } else {
                BitOff(pG->flags_68, 0x100);
            }
        } else if (symbol_check(&p, "TITLE_CHECK")) {
            if (CFG_ON(p)) {
                BitOn(pG->flags_64, 0x00080000);
            } else {
                BitOff(pG->flags_64, 0x00080000);
            }
        }
    }
    Mem_free(buf);
    PlMode = 3;
}

int symbol_check(char** p, const char* sym)
{
    int len = strlen(sym);

    if (len != strcspn(*p, "] \t\n\r")) {
        return 0;  // its own `li r3,0` before the branch
    }
    if (strncmp(*p, sym, len) == 0) {
        *p += len;
        if (**p == ']') {
            (*p)++;
        }
        *p = space_skip(*p);
        return 1;
    }
    return 0;
}

char* space_skip(char* p)
{
    do {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            p++;
        }
    } while (comment_check(&p));
    return p;
}

int comment_check(char** pp)
{
    char* p = *pp;

    if (strncmp(p, "[[", 2) == 0) {
        p += 2;
        while (strncmp(p, "]]", 2) != 0) {
            p++;
        }
        p += 2;
    } else if (strncmp(p, "/*", 2) == 0) {
        p += 2;
        while (strncmp(p, "*/", 2) != 0) {
            p++;
        }
        p += 2;
    } else if (strncmp(p, "//", 2) == 0) {
        while (*p != '\n') {
            p++;
        }
        p++;
    } else {
        return 0;
    }
    *pp = p;
    return -1;
}

int num_get(char** p)
{
    *p = space_skip(*p);
    if (strncmp(*p, "0x", 2) == 0) {
        return strtol(*p, p, 16);
    }
    return strtol(*p, p, 10);
}
