// game/pad: 12/12 functions byte-identical. PadRead: `dead` (5 refs / 526 doubled insns, priority 190)
// beats the loop-2 `&Pad_data` hoist (4 / 448, 178) in global-alloc (target r16/r17); pinned to r16, the
// constant kept first in block 0 by a codeless `asm volatile("")` barrier (see PadRead). The Key loop preheader order (`Key.on = 0` stores before
// the `li r0,64; mtctr` pair) is a reference store: the pSys load then depends on it (a struct-member
// store never conflicts with a fixed scalar), which lifts the stores above the count reload.
#include "types.h"
#include "global.h"
#include "main.h"
#include "main_mem.h"
#include "joy.h"
#include "pad.h"
#include "math_sub.h"
#include "rnd.h"
#include "eprintf.h"

static inline void U64Set(u64& d, u64 v) { d = v; }

#define KEY_BIT(n) ((u64) 1 << (n))

#define STICK_DEAD 10
#define STICK_ON 30.0f
#define DEG(d) ((d) * (PI / 180.0f))

u32 Key_type_tbl[2][64] = {
    {
        0x00080008, 0x00040004, 0x00020002, 0x00010001, 0x00000020, 0x00000040, 0x00000200, 0x00000100,
        0x00000000, 0x00000400, 0x00000100, 0x00000040, 0x00001000, 0x00001000, 0x00800000, 0x00400000,
        0x00000800, 0x00000400, 0x00000200, 0x00000100, 0x00000800, 0x00000010, 0x00000040, 0x00000020,
        0x00080008, 0x00040004, 0x00020002, 0x00010001, 0x00000010, 0x00001000, 0x00000200, 0x00000100,
        0x00000001, 0x00000002, 0x00000004, 0x00000008, 0x00200000, 0x00100000,
    },
    {
        0,
    },
};

static PADStatus Pad_data[4];
static int Vib_level = 0;
static u32 ResetBits;
u32 ConnectedBits;

// The do/while(0) matters: the loop notes stop the scheduler from mixing the flag stores
// with the preceding Key stores (KeyClear).
#define KeyStopFlagClear()                          \
    do {                                            \
        BitOff(pG->flags_500C, 0x4000);             \
        BitOff(pG->flags_500C, 0x40000000);         \
        BitOff(pG->flags_500C, 0x20000000);         \
    } while (0)

void PadInit()
{
    PADInit();
    PADReset(0xF0000000);
    PADSetAnalogMode(3);
    Vib_level = 0;
    Key.old = 0;
}

void PadRead()
{
    int i;
    int j;
    u32 bit;
    PADStatus* pad;
    JOY* joy;
    // `dead` (5 refs / doubled length) outranks the loop-2 `&Pad_data` hoist in global alloc and
    // takes r17 (target: r16, allocated after the hoist); pinning it keeps the hoist's r17. The
    // constant has no dependents in block 0 (priority 1 < the `lis Pad_data@ha` chain), so sched
    // would issue it second; the codeless barrier after it makes every later insn depend on it,
    // where the target issues `li r16,10` before `lis Pad_data@ha`.
    register int dead asm("r16");                // COMPILER-DIFF: #17

    dead = 10;
    asm volatile("");                            // COMPILER-DIFF: #13 (sched barrier, no code)
    PADRead(Pad_data);
    PADClamp(Pad_data);
    for (i = 0; i < 4; i++) {
        u32 chan = PAD_CHAN0_BIT >> i;
        switch (Pad_data[i].err) {
        case PAD_ERR_NONE:
            ConnectedBits |= chan;
            break;
        case PAD_ERR_NO_CONTROLLER:
            ResetBits |= chan;
            break;
        case PAD_ERR_NOT_READY:
        case PAD_ERR_TRANSFER:
            break;
        }
    }
    if (ResetBits) {
        if (PADReset(ResetBits)) {
            ResetBits = 0;
        }
    }

    for (i = 0; i < 4; i++) {
        joy = &Joy[i];
        pad = &Pad_data[i];
        joy->x8 = pad->err;
        if (joy->x8 != 0) {
            memclr_asm(joy, sizeof(JOY));
            joy->x8 = pad->err;
            continue;
        }
        joy->old = joy->on;
        joy->on = pad->button;
        joy->sx = pad->stickX;
        joy->sy = pad->stickY;
        joy->ssx = pad->substickX;
        joy->ssy = pad->substickY;
        joy->trigL = pad->triggerLeft;
        joy->trigR = pad->triggerRight;
        joy->anaA = pad->analogA;
        joy->anaB = pad->analogB;
        if (joy->trigL) {
            joy->on |= JOY_L;
        }
        if (joy->trigR) {
            joy->on |= JOY_R;
        }
        {
            f32 x = (f32) joy->sx;
            f32 y = (f32) joy->sy;
            f32 ang = -atan2f(x, y);
            if (x * x + y * y > STICK_ON * STICK_ON) {
                if (ang < 0.0f) {
                    if (ang > -DEG(70.0f)) {
                        joy->on |= JOY_SUP;
                    }
                    if (ang < -DEG(110.0f)) {
                        joy->on |= JOY_SDOWN;
                    }
                    if (ang < -DEG(30.0f) && ang > -DEG(150.0f)) {
                        joy->on |= JOY_SLEFT;
                    }
                } else {
                    if (ang < DEG(70.0f)) {
                        joy->on |= JOY_SUP;
                    }
                    if (ang > DEG(110.0f)) {
                        joy->on |= JOY_SDOWN;
                    }
                    if (ang > DEG(30.0f) && ang < DEG(150.0f)) {
                        joy->on |= JOY_SRIGHT;
                    }
                }
            }
        }
        if (joy->ssx < -10) {
            joy->on |= JOY_SSLEFT;
        } else if (dead < joy->ssx) {
            joy->on |= JOY_SSRIGHT;
        } else {
            joy->ssx = 0;
        }
        if (joy->ssy < -10) {
            joy->on |= JOY_SSDOWN;
        } else if (dead < joy->ssy) {
            joy->on |= JOY_SSUP;
        } else {
            joy->ssy = 0;
        }
        {
        u32 bit;
        joy->rep = 0;
        joy->rep2 = 0;
        // trg first and `old ^ on`: the target loads old before on (`xor r0,r9,r10`), computes
        // rel (`and r9,r0,r9`, tied to the dying old) first and stores rel then trg.
        joy->trg = (joy->old ^ joy->on) & joy->on;
        joy->rel = (joy->old ^ joy->on) & joy->old;
        for (bit = 1, j = 0; j < 32; j++, bit <<= 1) {
            if (joy->on & bit) {
                if (joy->trg & bit) {
                    joy->rep |= bit;
                    joy->rep2 |= bit;
                } else {
                    if (joy->rep_timer[j] <= 0) {
                        joy->rep |= bit;
                        joy->rep_timer[j] = 6;
                    }
                    if (joy->rep2_timer[j] <= 0) {
                        joy->rep2 |= bit;
                        joy->rep2_timer[j] = 3;
                    }
                }
                joy->rep_timer[j] -= GetSystemVcnt();
                joy->rep2_timer[j] -= GetSystemVcnt();
            } else {
                joy->rep_timer[j] = 24;
                joy->rep2_timer[j] = 18;
            }
        }
        }
    }

    {
        // Reference store through a `&Key` pointer: `stw 16(r11)` with r11 = &Key, and the following
        // `pSys` load depends on it, so the stores are issued before the `li r0,64; mtctr` pair.
        KeyWork* k = &Key;
        U64Set(k->on, 0);
    }
    for (i = 0, bit = 1; i < 64; bit <<= 1, i++) {
        if (Joy[0].on & Key_type_tbl[pSys->key_type][i]) {
            Key.on |= bit;
        }
    }
    if ((Key.on & KEY_BIT(0)) && (Key.on & KEY_BIT(1))) {
        Key.on &= ~KEY_BIT(0);
        Key.on &= ~KEY_BIT(1);
    }
    if ((Key.on & KEY_BIT(2)) && (Key.on & KEY_BIT(3))) {
        Key.on &= ~KEY_BIT(2);
        Key.on &= ~KEY_BIT(3);
    }
    if ((Key.on & KEY_BIT(24)) && (Key.on & KEY_BIT(25))) {
        Key.on &= ~KEY_BIT(24);
        Key.on &= ~KEY_BIT(25);
    }
    if ((Key.on & KEY_BIT(27)) && (Key.on & KEY_BIT(26))) {
        Key.on &= ~KEY_BIT(27);
        Key.on &= ~KEY_BIT(26);
    }
    if ((Key.on & KEY_BIT(35)) && (Key.on & KEY_BIT(34))) {
        Key.on &= ~KEY_BIT(35);
        Key.on &= ~KEY_BIT(34);
    }
    if ((Key.on & KEY_BIT(32)) && (Key.on & KEY_BIT(33))) {
        Key.on &= ~KEY_BIT(32);
        Key.on &= ~KEY_BIT(33);
    }
    if ((Key.on & KEY_BIT(14)) && (Key.on & KEY_BIT(15))) {
        Key.on &= ~KEY_BIT(14);
        Key.on &= ~KEY_BIT(15);
    }
    if ((Key.on & KEY_BIT(37)) && (Key.on & KEY_BIT(36))) {
        Key.on &= ~KEY_BIT(37);
        Key.on &= ~KEY_BIT(36);
    }

    Key.rep = 0;
    Key.rep2 = 0;
    Key.trg = (Key.old ^ Key.on) & Key.on;
    Key.rel = (Key.old ^ Key.on) & Key.old;
    Key.old = Key.on;
    for (bit = 1, j = 0; j < 64; j++, bit <<= 1) {
        if (Key.on & bit) {
            if (Key.trg & bit) {
                Key.rep |= bit;
                Key.rep2 |= bit;
            } else {
                if (Key.rep_timer[j] <= 0) {
                    Key.rep |= bit;
                    Key.rep_timer[j] = 6;
                }
                if (Key.rep2_timer[j] <= 0) {
                    Key.rep2 |= bit;
                    Key.rep2_timer[j] = 3;
                }
            }
            Key.rep_timer[j] -= GetSystemVcnt();
            Key.rep2_timer[j] -= GetSystemVcnt();
        } else {
            Key.rep_timer[j] = 24;
            Key.rep2_timer[j] = 18;
        }
    }

    if (!(pG->flags_170 & 0x80000000)) {
        Key.sx = Joy[0].sx;
        Key.sy = Joy[0].sy;
        Key.ssx = Joy[0].ssx;
        Key.ssy = Joy[0].ssy;
        Key.trigL = Joy[0].trigL;
        Key.trigR = Joy[0].trigR;
    } else {
        KeyStop(0);
    }
    Key.x6 = 0;
    Key.x7 = 0;
    Pad_test();
    if (pG->flags_170 & 0x10000000) {
        KeyStopFlagClear();
    }
    VibControl();
}

void KeyStop(u64 mask)
{
    BitOn(pG->flags_170, 0x80000000);
    KeyClear(mask);
}

void KeyClear(u64 mask)
{
    static u64 un_stop_mask = 0;

    if (mask) {
        un_stop_mask = mask;
    }
    Key.on &= un_stop_mask;
    Key.trg &= un_stop_mask;
    Key.rel &= un_stop_mask;
    Key.rep &= un_stop_mask;
    Key.rep2 &= un_stop_mask;
    Key.trigL = 0;
    Key.trigR = 0;
    KeyStopFlagClear();
}

void VibControl()
{
    u8 old = Joy[0].vib_state;
    int max = 0;
    VibWork* v;
    int lvl;
    int i;

    for (i = 0; i < 10; i++) {
        v = &Joy[0].vib[i];
        if (v->time == 0) {
            continue;
        }
        if (v->wait) {
            v->wait--;
            continue;
        }
        v->time--;
        v->level += v->add;
        lvl = v->level;
        if (v->type & 0x8000) {
            lvl = (u32) ((Rnd() << 8) + Rnd()) % (lvl + 1);
        }
        if (max < lvl) {
            max = lvl;
        }
    }
    Vib_level += max;
    if (Vib_level > 0x7F7F) {
        Vib_level -= 0x7F80;
        Joy[0].vib_state = 1;
    } else {
        Joy[0].vib_state = 0;
    }
    if ((pG->flags_170 & 0x8000) && old == 1) {
        Joy[0].vib_state = 2;
        PADControlMotor(0, 2);
        Vib_level = 0;
    } else if (Joy[0].vib_state != old) {
        PADControlMotor(0, Joy[0].vib_state);
    }
}

VibWork* PullVibWork()
{
    int i;
    VibWork* v = Joy[0].vib;
    if (!(pSys->flags & 0x08000000)) {
        return NULL;
    }
    for (i = 0; i < 10; i++, v++) {
        if (v->time == 0) {
            return v;
        }
    }
    return NULL;
}

void VibSet(u32 time, u32 level, u16 wait, u16 type)
{
    VibWork* v = PullVibWork();
    if (v) {
        if (time > 0xFF) {
            time = 0xFF;
        }
        v->type = type;
        v->time = time;
        v->wait = wait;
        v->level = level << 7;
        v->add = 0;
    }
}

void VibSetDataCore(VibData* d, u32 type)
{
    u32 i;
    VibWork* v;
    VibDataEntry* e;
    int lvl;
    int add;

    for (i = 0; i < d->num; i++) {
        v = PullVibWork();
        if (!v) {
            return;
        }
        e = d->e;
        e += i;
        lvl = e->lvl0 << 12;
        add = ((e->lvl1 - e->lvl0) << 12) / e->time;
        v->type = e->type | type;
        v->time = e->time;
        v->wait = e->wait;
        v->level = lvl;
        v->add = add;
    }
}

void VibSetData(VibDataTbl* t, u32 no, u32 type)
{
    u32* ofs = t->ofs;
    if (no < t->num && ofs[no]) {
        VibSetDataCore((VibData*) (ofs[no] + (u32) t), type);
    }
}

void VibSetClearType(u32 type)
{
    int i;
    VibWork* v = Joy[0].vib;
    type &= 0xF;
    for (i = 0; i < 10; i++, v++) {
        if (v->type & type) {
            v->time = 0;
        }
    }
}

int PadCheckStatus(JOY* joy)
{
    if (pG->flags_54 & 8) {
        return 0;
    }
    return joy->x8 == 0;
}

void Pad_test()
{
    u8* p = (u8*) &Joy[0];

    eprintf(32, 90, 0, 5, "ON   %08x %08x %08x %08x", BtoX(p[0x10]), BtoX(p[0x11]), BtoX(p[0x12]), BtoX(p[0x13]));
    eprintf(32, 105, 0, 5, "TRG  %08x %08x %08x %08x", BtoX(p[0x14]), BtoX(p[0x15]), BtoX(p[0x16]), BtoX(p[0x17]));
    eprintf(32, 120, 0, 5, "REL  %08x %08x %08x %08x", BtoX(p[0x18]), BtoX(p[0x19]), BtoX(p[0x1A]), BtoX(p[0x1B]));
    eprintf(32, 135, 0, 5, "REP  %08x %08x %08x %08x", BtoX(p[0x1C]), BtoX(p[0x1D]), BtoX(p[0x1E]), BtoX(p[0x1F]));
    eprintf(32, 150, 0, 5, "REP2 %08x %08x %08x %08x", BtoX(p[0x20]), BtoX(p[0x21]), BtoX(p[0x22]), BtoX(p[0x23]));
    eprintf(32, 180, 0, 5, "STICK_X       %d", Joy[0].sx);
    eprintf(32, 195, 0, 5, "STICK_Y       %d", Joy[0].sy);
    eprintf(32, 210, 0, 5, "SUB_STICK_X   %d", Joy[0].ssx);
    eprintf(32, 225, 0, 5, "SUB_STICK_Y   %d", Joy[0].ssy);
    eprintf(32, 240, 0, 5, "TRIGGER_LEFT  %d", Joy[0].trigL);
    eprintf(32, 255, 0, 5, "TRIGGER_RIGHT %d", Joy[0].trigR);
}

// The split object's .rodata is 8-aligned and 4 bytes longer (padding after the last string).
asm(".section .rodata; .balign 8");
