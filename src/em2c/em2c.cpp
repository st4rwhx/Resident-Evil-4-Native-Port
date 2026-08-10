// em2c module (D:/Bio4/Prog/em2c.cpp): the insect boss (cModel::type 0) and its tail (type 1).
// It shares the em2d routine set (floor walking, wall / ceiling climbing, jump and tail attacks,
// the player-catch callbacks) and adds the hide / ambush states, the freeze reaction and the
// two-motion blends (em2cBlendMotSet).

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "em2c.h"
#include "emhit.h"
#include "emdoor.h"
#include "obj01.h"
#include "obj.h"
#include "pl_wep.h"
#include "pendulum.h"
#include "main.h"
#include "em_set.h"
#include "em_sub.h"
#include "em_cloth.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "route_ck.h"
#include "act_btn.h"
#include "game.h"
#include "snd.h"
#include "pad.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "dbmodule.h"
#include "db_log.h"

// The module's 0x34-byte COMMON block (st_room.h): uninitialised template statics of the original
// object, merged into .bss by the REL link.
asm(".comm common_em2c,52,4");
#include "camera.h"
#include "cam_ctrl.h"
#include "quake.h"
#include "item.h"

extern "C" void OSReport(const char* fmt, ...);
int GetWepDmVal(cEm* em, u32 a, int b);   // em10.h (not included: it pulls emwep.h's global plemBackjump)
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// em_set.h declares EmSetDieCnt without arguments; this module passes the enemy.
void EmSetDieCntE(cEm* em) asm("EmSetDieCnt");
// COMPILER-DIFF #4: the original passes the int work field to the u16 parameter without the
// truncation ours emits (`lwz` instead of `lhz`); int-view declarations of the two blend setters.
void em2cBlendMotSetI(cEm2c* em, void* m0, void* m1, void* m2, int a, int b, int c, int d) asm("em2cBlendMotSet__FP5cEm2cPvN21iiiUs");
void em2cBlendMotSet2I(cEm2c* em, void* m0, void* m1, int a, int b, int d) asm("em2cBlendMotSet2__FP5cEm2cPvT1iiUs");

static void em2c_R0_Init(cEm2c* em);
static void em2c_R0_Move(cEm2c* em);
static void em2c_R1_br_Dummy(cEm2c* em);
static void em2c_R1_Wait(cEm2c* em);
static void em2c_R1_Walk(cEm2c* em);
static void em2c_R1_Dash(cEm2c* em);
static void em2c_R1_Turn180(cEm2c* em);
static void em2c_R1_Ambush(cEm2c* em);
static void em2c_R1_SideStep(cEm2c* em);
static void em2c_R1_BackJump(cEm2c* em);
static void em2c_R1_Threat(cEm2c* em);
static void em2c_R1_AtkSign(cEm2c* em);
static void em2c_R1_Atk(cEm2c* em);
static void em2c_R1_JumpAtk(cEm2c* em);
static void em2c_R1_BackKnuckle(cEm2c* em);
static void em2c_R1_TailAtk(cEm2c* em);
static void plem2c_CriticalHit(cPlayer* pl);
static void em2c_R1_SwayBack(cEm2c* em);
static void em2c_R1_WakeupWait(cEm2c* em);
static void em2c_R1_Wakeup(cEm2c* em);
static void em2c_R1_DownJump(cEm2c* em);
static void em2c_R1_ToCeiling(cEm2c* em);
static void em2c_R1_ToHide(cEm2c* em);
static void em2c_R1_HideWait(cEm2c* em);
static void em2c_R1_HideAtk(cEm2c* em);
static void em2c_R1_HideFall(cEm2c* em);
static void em2c_R1_JumpDown(cEm2c* em);
static void em2c_R1_WallOver(cEm2c* em);
static void em2c_R1_F_Wait(cEm2c* em);
static void em2c_R1_F_Walk(cEm2c* em);
static void em2c_R1_F_Atk(cEm2c* em);
static void em2c_R1_F_Clear(cEm2c* em);
static void em2c_R1_W_Wait(cEm2c* em);
static void em2c_R1_W_Walk(cEm2c* em);
static void em2c_R1_W_Atk(cEm2c* em);
static void em2c_R1_W_Turn180(cEm2c* em);
static void em2c_R1_W_Fall(cEm2c* em);
static void em2c_R1_C_Wait(cEm2c* em);
static void em2c_R1_C_Fall(cEm2c* em);
static void em2c_R1_Reset_Wait(cEm2c* em);
static void em2c_R1_T_Wait(cEm2c* em);
static void em2c_R1_T_Hide(cEm2c* em);
static void em2c_R0_Damage(cEm2c* em);
static void em2c_R1_Dm_Normal(cEm2c* em);
static void em2c_R1_Dm_Down(cEm2c* em);
static void em2c_R1_Dm_Jump(cEm2c* em);
static void em2c_R1_Dm_Wall(cEm2c* em);
static void em2c_R1_Dm_Guard(cEm2c* em);
static void em2c_R1_Dm_Freeze(cEm2c* em);
static void em2c_R1_Dm_C_Freeze(cEm2c* em);
static void em2cKickAction(cEm2c* em);
static void em2c_R1_Dm_F_Normal(cEm2c* em);
static void em2c_R1_Dm_F_Blow(cEm2c* em);
static void em2c_R0_Die(cEm2c* em);
static void em2c_R1_Die_Lost(cEm2c* em);
static void em2c_R1_Die_Normal(cEm2c* em);
static void em2c_R1_Die_Freeze(cEm2c* em);
static void em2c_R1_Die_Down(cEm2c* em);
static void em2c_R1_Die_Wall(cEm2c* em);
static void plemDmSide(cPlayer* pl);
static void plemDmTail(cPlayer* pl);
static void em2cSitAction(cEm2c* em);
static void plem2cSit(cPlayer* pl);
static void em2cEscapeAction(cEm2c* em);
static void plem2cEscape(cPlayer* pl);
static void em2cBackjumpAction(cEm2c* em);
static void em2cBackjumpAction2(cEm2c* em);
static void plemBackjump2(cPlayer* pl);

// The two player callbacks that emwep.h also declares carry C linkage here (the module's own
// local copies keep the unmangled names in the REL symbol table).
extern "C" {
static void plemKick(cPlayer* pl);
static void plemBackjump(cPlayer* pl);
}

#define ARC(no) PL_ARC_PTR(em->subArc, no)
#define PL_ARC(no) PL_ARC_PTR(pl->subArc, no)

// The enemy a player damage callback belongs to (pl_sub SetPlDamage's first argument).
#define PL_EM(pl) ((cEm2c*) (pl)->dmgType)

// Struct-member view of the player pointer (cam_ctrl.cpp PlayerPtr).
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

// Routine test on the cModel status word (xFC / xFD as the upper half of `stat`).
#define EM_RTN(em, fc, fd) (((em)->stat & 0xFFFF0000) == (u32) (((fc) << 24) | ((fd) << 16)))

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

// Scalar reference stores: pG / the player pointer are reloaded after them (st_room.h).
static inline void IntSet(int& d, int v) { d = v; }
static inline void U8Set(u8& d, u8 v) { d = v; }

// Dead flag test (cDmgInfo upper 16 bits): an inline returning 0/1 gives the `li 1; andis.; bne; li 0` chain.
static inline int em2cDeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

// Collision flag bits set / cleared through the info's address (`addi rX, em, 0x2b4; lhz 0x1a(rX)`).
static inline void AtariOn(cAtariInfo* at, u16 b) { at->flags |= b; }
static inline void AtariOff(cAtariInfo* at, u16 mask) { at->flags &= mask; }

// Attack wait by difficulty: pG is reloaded after every store (reference stores).
static inline void em2cSetAtkWait(Em2cWork* w, int a, int b, int c, int d, int e)
{
    IntSet(w->atkWait, a);
    if (pG->x4F88 > 1) {
        IntSet(w->atkWait, b);
    }
    if (pG->x4F88 > 3) {
        IntSet(w->atkWait, c);
    }
    if (pG->x4F88 > 6) {
        IntSet(w->atkWait, d);
    }
    if (pG->x4F88 == 10) {
        IntSet(w->atkWait, e);
    }
}

// Per-frame movement with gravity: the position follows `spd`, which falls 20 per frame, and the
// enemy lands on the floor under its old position.
static inline void em2cGravityMove(cEm2c* em, Em2cWork* w)
{
    f32 fl;

    PSVECAdd(&em->pos, &w->spd, &em->pos);
    w->spd.y -= 20.0f;
    fl = SatMgr.getFloor(&em->oldPos, 600.0f, 100000.0f, 0, 0);
    if (em->pos.y < fl) {
        em->pos.y = fl;
        w->spd.y = 0.0f;
    }
}

// Turn towards `target` by at most `step` per frame.
static inline void em2cTurnTo(cEm2c* em, Vec* target, f32 step)
{
    em->rot.y += Muku(&em->pos, target, em->rot.y, step);
    em->rot.y = LIMIT_ANGLE(em->rot.y);
}

Em2cFunc Em2c_R0_move_tbl[4] = {
    em2c_R0_Init,
    em2c_R0_Move,
    em2c_R0_Damage,
    em2c_R0_Die,
};

// Routine 1 table: {branch check, routine} per xFD (a flat table: `[xFD * 2 + 1]`).
static Em2cFunc Em2c_R1_move_tbl[76] = {
    em2c_R1_br_Dummy, em2c_R1_Wait,         // 0x00
    em2c_R1_br_Dummy, em2c_R1_Walk,         // 0x01
    em2c_R1_br_Dummy, em2c_R1_Dash,         // 0x02
    em2c_R1_br_Dummy, em2c_R1_Turn180,      // 0x03
    em2c_R1_br_Dummy, em2c_R1_Ambush,       // 0x04
    em2c_R1_br_Dummy, em2c_R1_SideStep,     // 0x05
    em2c_R1_br_Dummy, em2c_R1_BackJump,     // 0x06
    em2c_R1_br_Dummy, em2c_R1_Threat,       // 0x07
    em2c_R1_br_Dummy, em2c_R1_AtkSign,      // 0x08
    em2c_R1_br_Dummy, em2c_R1_Atk,          // 0x09
    em2c_R1_br_Dummy, em2c_R1_JumpAtk,      // 0x0A
    em2c_R1_br_Dummy, em2c_R1_BackKnuckle,  // 0x0B
    em2c_R1_br_Dummy, em2c_R1_TailAtk,      // 0x0C
    em2c_R1_br_Dummy, em2c_R1_SwayBack,     // 0x0D
    em2c_R1_br_Dummy, em2c_R1_WakeupWait,   // 0x0E
    em2c_R1_br_Dummy, em2c_R1_Wakeup,       // 0x0F
    em2c_R1_br_Dummy, em2c_R1_DownJump,     // 0x10
    em2c_R1_br_Dummy, em2c_R1_ToCeiling,    // 0x11
    em2c_R1_br_Dummy, em2c_R1_ToHide,       // 0x12
    em2c_R1_br_Dummy, em2c_R1_HideWait,     // 0x13
    em2c_R1_br_Dummy, em2c_R1_HideAtk,      // 0x14
    em2c_R1_br_Dummy, em2c_R1_HideFall,     // 0x15
    em2c_R1_br_Dummy, em2c_R1_JumpDown,     // 0x16
    em2c_R1_br_Dummy, em2c_R1_F_Wait,       // 0x17
    em2c_R1_br_Dummy, em2c_R1_F_Walk,       // 0x18
    em2c_R1_br_Dummy, em2c_R1_F_Atk,        // 0x19
    em2c_R1_br_Dummy, em2c_R1_F_Clear,      // 0x1A
    em2c_R1_br_Dummy, em2c_R1_WallOver,     // 0x1B
    em2c_R1_br_Dummy, em2c_R1_W_Wait,       // 0x1C
    em2c_R1_br_Dummy, em2c_R1_W_Walk,       // 0x1D
    em2c_R1_br_Dummy, em2c_R1_W_Atk,        // 0x1E
    em2c_R1_br_Dummy, em2c_R1_W_Turn180,    // 0x1F
    em2c_R1_br_Dummy, em2c_R1_W_Fall,       // 0x20
    em2c_R1_br_Dummy, em2c_R1_C_Wait,       // 0x21
    em2c_R1_br_Dummy, em2c_R1_C_Fall,       // 0x22
    em2c_R1_br_Dummy, em2c_R1_Reset_Wait,   // 0x23
    em2c_R1_br_Dummy, em2c_R1_T_Wait,       // 0x24
    em2c_R1_br_Dummy, em2c_R1_T_Hide,       // 0x25
};

static Em2cFunc Em2c_R1_dm_tbl[9] = {
    em2c_R1_Dm_Normal,
    em2c_R1_Dm_Down,
    em2c_R1_Dm_Jump,
    em2c_R1_Dm_Wall,
    em2c_R1_Dm_Guard,
    em2c_R1_Dm_Freeze,
    em2c_R1_Dm_C_Freeze,
    em2c_R1_Dm_F_Normal,
    em2c_R1_Dm_F_Blow,
};

static Em2cFunc Em2c_R1_die_tbl[5] = {
    em2c_R1_Die_Lost,
    em2c_R1_Die_Normal,
    em2c_R1_Die_Freeze,
    em2c_R1_Die_Down,
    em2c_R1_Die_Wall,
};

// Motion parts flip table (cModel::motFlip): the mirrored parts index per parts.
static u16 em2c_xflip_tbl[120] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x0A, 0x0B, 0x0C, 0x0D, 0x06, 0x07, 0x08, 0x09, 0x0E, 0x13,
    0x14, 0x15, 0x16, 0x0F, 0x10, 0x11, 0x12, 0x18, 0x17, 0x1A, 0x19, 0x1C, 0x1B, 0x1D, 0x1E, 0x20,
    0x1F, 0x23, 0x24, 0x21, 0x22, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x25, 0x26, 0x27, 0x28, 0x29,
    0x2A, 0x34, 0x35, 0x36, 0x31, 0x32, 0x33, 0x38, 0x37, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4F, 0x50, 0x4D,
    0x4E, 0x52, 0x51, 0x54, 0x53, 0x55, 0x57, 0x56, 0x58, 0x5A, 0x59, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
};

// Attack parameters per attack number (em2cAtkCk).
static EmAtkInfo em2c_atk_info[8] = {
    { 500.0f, 8, 650, 0, 10, 0 },
    { 500.0f, 8, 650, 0, 10, 0 },
    { 500.0f, 8, 650, 0, 10, 0 },
    { 500.0f, 8, 650, 0, 10, 0 },
    { 500.0f, 8, 650, 0, 10, 0 },
    { 500.0f, 8, 650, 0, 10, 0 },
    { 500.0f, 8, 300, 0, 10, 0 },
    { 500.0f, 8, 650, 0, 10, 0 },
};

extern "C" void _prolog()
{
    OSReport("em2c prolog Ok\n");
    EmInitFunc = Em2cInit;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em2cInit(cEm* em)
{
    new (em) cEm2c();
}

// Damage reaction after a hit while alive (em2cDmCk): by the state flags, then by the weapon.
void em2cDmCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Camera* cam = &pG->Cam;
    EmHitInfo* part;
    cModel* p;
    int near;
    int guard;
    int dmg;
    f32 dmAng;
    f32 dist;
    f32 py;

    if (em->hp > 0) {
        if (!(w->flags & 0x100840) && !em2cDeadCk(em)) {
            int two = 2;      // the routine 2 of the first two arms in a callee-saved register

            if ((int) pG->sceat_x17C < 0) {
            em2cSetFreeze(em);
            if (w->flags & 0x200000) {
                EmRoutineSet(em, two, 6, 0, 0);
                return;
            }
            if ((w->flags & 0x20) && w->wallNrm.y < 0.5f) {
                EmRoutineSet(em, two, 3, 0, 0);
                return;
            }
            if (w->flags & 0x1010) {
                EmRoutineSet(em, 2, 2, 0, 0);
                return;
            }
            if (w->flags & 0x400) {
                EmRoutineSet(em, 2, 1, 0, 0);
                return;
            }
            EmRoutineSet(em, 2, 5, 0, 0);
            return;
            }
        }
        if (em->hp > 0 && !em2cDeadCk(em)) {
            switch (DmgMgr.hitCheck(&em->pos, 0)) {
            case 1:
            case 4:
            case 5:
            case 7:
                if (w->dmGuard == 0) {
                    w->guardCnt = 0;
                    w->dmGuard = 120;
                    LifeDownSet2(em, 500, 0, 0);
                    if (em->hp <= 0) {
                        EmSetDie(em);
                        EmReserveDropItem(em);
                        if (w->flags & 0x20) {
                            EmRoutineSet(em, 3, 4, 0, 0);
                        } else if (w->flags & 0x1010) {
                            EmRoutineSet(em, 2, 2, 0, 0);
                        } else if (w->flags & 0x800) {
                            EmRoutineSet(em, 3, 2, 0, 0);
                        } else if (w->flags & 0x400) {
                            EmRoutineSet(em, 3, 3, 0, 0);
                        } else {
                            EmRoutineSet(em, 3, 1, 0, 0);
                        }
                        return;
                    }
                    if ((w->flags & 0x20) && w->wallNrm.y < 0.5f) {
                        if (w->dmgTotal <= 999) {
                            return;
                        }
                        w->dmgTotal = 0;
                        EmRoutineSet(em, 2, 3, 0, 0);
                        return;
                    }
                    if (w->flags & 0x1010) {
                        EmRoutineSet(em, 2, 2, 0, 0);
                        return;
                    }
                    if (w->flags & 0x400) {
                        EmRoutineSet(em, 2, 1, 0, 0);
                        return;
                    }
                    if (w->flags & 8) {
                        return;
                    }
                    if (w->flags & 0x800) {
                        EmRoutineSet(em, 2, 7, 0, 0);
                    } else {
                        EmRoutineSet(em, 2, 0, 0, 0);
                    }
                    return;
                }
                break;
            }
        }
    }
    if (em->dmHit == 0) {
        return;
    }
    em->dmHit = 0;
    BitOn(pG->flags_5010, 0x20000000);
    pGS->bell_pos = em->pos;
    pGS->bell_stat = 0;
    em->dmType = 1;
    if (em->dmWep == 0x10) {
        em->dmType = 0x11;
    }
    w->x56C = 0;
    near = 0;
    part = em->dmPart;
    if (part->rad < 36000000.0f) {
        near = 1;
    }
    dmAng = fabsf(Muku(&em->pos, &em->x328, em->rot.y, 3.14159274f));
    switch (em->dmWep) {
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 0xA:
    case 0xF:
    case 0x15:
    case 0x21:
    case 0x28:
    case 0x2C:
        if (!(w->flags & 0x800) && w->routeAngAbs < 0.52359879f && (w->flags & 0x40000) && Rnd() % 100 > 49) {
            em->dmType = 0xA;
            py = part->pos.y;  // loaded once (both arms read it)
            // one if/else (two RS blocks, fresh zeros): the four-arm nest cross-jumps the stores first and
            // leaves the li blocks unmerged
            if (em->pos.y > 1700.0f ? py > 1.0f : py > 0.0f) {
                EmRoutineSet(em, 1, 0xD, 0, 0);
            } else {
                EmRoutineSet(em, 1, 5, 0, 0);
            }
            return;
        }
        break;
    }
    guard = 0;
    if (w->flags & 0x100000) {
        guard = 1;
    }
    dmg = em2cSetDmVal(em);
    LifeDownSet2(em, dmg, 0, guard);
    w->dmgTotal += dmg;
    if (w->guardCnt) {
        w->guardCnt -= dmg / 6;
        if (w->guardCnt < 0) {
            w->guardCnt = 0;
        }
    }
    p = em->getPartsPtr(0);
    dist = (cam->param.pos.x - p->worldPos.x) * (cam->param.pos.x - p->worldPos.x) +
           (cam->param.pos.y - p->worldPos.y) * (cam->param.pos.y - p->worldPos.y) +
           (cam->param.pos.z - p->worldPos.z) * (cam->param.pos.z - p->worldPos.z);
    switch (em->dmWep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xE:
    case 0x11:
    case 0x15:
    case 0x26:
    case 0x2B:
        if (w->flags & 0x800) {
            EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
        }
        break;
    case 0x10:
        if (w->flags & 0x800) {
            EmDmBloodSet2(em, 0x24, 0x24, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x24, 0x25, 0, 0, 0);
        }
        break;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        if (w->flags & 0x800) {
            EmDmBloodSet2(em, 0x24, 0x23, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            if (dist < 16000000.0f) {
                if (w->flags & 0x800) {
                    EmDmBloodSet2(em, 0x24, 0xF, 0, 0, 0);
                } else {
                    EmDmBloodSet2(em, 0x24, 4, 0, 0, 0);
                }
            } else {
                if (w->flags & 0x800) {
                    EmDmBloodSet2(em, 0x24, 0xF, 0, 0, 0);
                } else {
                    EmDmBloodSet2(em, 0x24, 4, 0, 0, 0);
                }
            }
        } else {
            if (w->flags & 0x800) {
                EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
            }
        }
        break;
    case 0x17:
    case 0x2A:
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        if (dist < 16000000.0f) {
            if (w->flags & 0x800) {
                EmDmBloodSet2(em, 0x24, 0xF, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x24, 4, 0, 0, 0);
            }
        } else {
            if (w->flags & 0x800) {
                EmDmBloodSet2(em, 0x24, 0xF, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x24, 4, 0, 0, 0);
            }
        }
        break;
    }
    if (w->flags & 0x800) {
        SndCall(8, 9, &em->pos, em->id, 0, em);
    } else {
        SndCall(8, 6, &em->pos, em->id, 0, em);
    }
    if ((w->flags & 0x400000) && em->hp <= 0) {
        em->hp = 1;
    }
    if (em->hp <= 0) {
        EmSetDie(em);
        EmReserveDropItem(em);
        if (w->flags & 0x20) {
            EmRoutineSet(em, 3, 4, 0, 0);
            return;
        }
        if (w->flags & 0x1010) {
            EmRoutineSet(em, 2, 2, 0, 0);
            return;
        }
        asm volatile("");  // COMPILER-DIFF: #16 candidate: jump2's "if (c) { x = a; goto l; } x = b" hoist of the arm's `li r0,2` above the `beq` (x = r0 = the switch load below) fires in ours, not in the original; the codeless asm makes the next block's first insn a non-SET
        switch (em->dmWep) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 0xB:
        case 0xC:
        case 0xE:
        case 0x10:
        case 0x11:
        case 0x16:
        case 0x17:
        case 0x1B:
        case 0x1D:
        case 0x26:
        case 0x27:
        case 0x2A:
        case 0x2B:
        default:
            if (w->flags & 0x800) {
                EmRoutineSet(em, 3, 2, 0, 0);
            } else if (w->flags & 0x400) {
                EmRoutineSet(em, 3, 3, 0, 0);
            } else {
                EmRoutineSet(em, 3, 1, 0, 0);
            }
            return;
        case 7:
        case 8:
        case 0x21:
            if (w->flags & 0x800) {
                EmRoutineSet(em, 3, 2, 0, 0);
            } else if (w->flags & 0x400) {
                EmRoutineSet(em, 3, 3, 0, 0);
            } else {
                EmRoutineSet(em, 3, 1, 0, 0);
            }
            return;
        case 5:
        case 6:
        case 9:
        case 0xA:
        case 0xD:
        case 0xF:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x18:
        case 0x28:
        case 0x29:
        case 0x2C:
        case 0x2D:
            if (w->flags & 0x800) {
                EmRoutineSet(em, 3, 2, 0, 0);
            } else if (w->flags & 0x400) {
                EmRoutineSet(em, 3, 3, 0, 0);
            } else {
                EmRoutineSet(em, 3, 1, 0, 0);
            }
            return;
        }
    }
    if (w->flags & 0x8000) {
        return;
    }
    if ((w->flags & 0x20) && w->wallNrm.y < 0.5f) {
        if (w->dmgTotal <= 999) {
            return;
        }
        w->dmgTotal = 0;
        EmRoutineSet(em, 2, 3, 0, 0);
        return;
    }
    if (w->flags & 0x1010) {
        EmRoutineSet(em, 2, 2, 0, 0);
        return;
    }
    if (w->flags & 0x400) {
        if ((w->flags & 0x800) && w->guardCnt == 0) {
            return;
        }
        switch (em->dmWep) {
        default:
            if (Rnd() & 3) {
                return;
            }
            EmRoutineSet(em, 2, 1, 0, 0);
            return;
        case 7:
        case 8:
        case 0x21:
            if (near == 0) {
                return;
            }
            EmRoutineSet(em, 2, 1, 0, 0);
            return;
        case 0xD:
        case 0xE:
        case 0xF:
        case 0x12:
        case 0x13:
        case 0x2D:
            EmRoutineSet(em, 2, 1, 0, 0);
            return;
        }
    }
    if (w->flags & 8) {
        return;
    }
    switch (em->dmWep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x2B:
        if (w->dmgTotal <= 999) {
            return;
        }
        w->dmgTotal = 0;
        if (w->flags & 0x800) {
            EmRoutineSet(em, 2, 7, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        return;
    case 7:
    case 8:
    case 0x21:
        if (near == 0) {
            return;
        }
        if (w->dmgTotal <= 999) {
            return;
        }
        w->dmgTotal = 0;
        if (w->flags & 0x800) {
            if (Rnd() & 3) {
                EmRoutineSet(em, 2, 7, 0, 0);
            } else {
                EmRoutineSet(em, 2, 8, 0, 0);
            }
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        return;
    case 5:
    case 6:
    case 0xF:
    case 0x2C:
        if (w->dmgTotal <= 999) {
            return;
        }
        w->dmgTotal = 0;
        if (w->flags & 0x800) {
            if (Rnd() & 3) {
                EmRoutineSet(em, 2, 7, 0, 0);
            } else {
                EmRoutineSet(em, 2, 8, 0, 0);
            }
        } else if (dmAng < 1.57079637f) {
            EmRoutineSet(em, 2, 4, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        return;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
    default:
        if (w->flags & 0x800) {
            if (Rnd() & 3) {
                EmRoutineSet(em, 2, 7, 0, 0);
            } else {
                EmRoutineSet(em, 2, 8, 0, 0);
            }
        } else if (dmAng < 1.57079637f) {
            EmRoutineSet(em, 2, 4, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        return;
    case 0x17:
    case 0x2A:
        if (w->flags & 0x800) {
            return;
        }
        if (dmAng < 1.57079637f) {
            EmRoutineSet(em, 2, 4, 0, 0);
        }
        return;
    case 0x14:
        if (w->flags & 0x800) {
            EmRoutineSet(em, 2, 8, 0, 0);
        } else {
            EmRoutineSet(em, 2, 4, 0, 0);
        }
        return;
    case 0xE:
        return;
    }
}

void em2cTailDmCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Camera* cam = &pG->Cam;
    cModel* p;
    int near;
    int dmg;
    f32 dist;

    if (em->dmHit == 0) {
        return;
    }
    em->dmHit = 0;
    BitOn(pG->flags_5010, 0x20000000);
    pGS->bell_pos = em->pos;
    pGS->bell_stat = 0;
    em->dmType = 1;
    if (em->dmWep == 0x10) {
        em->dmType = 0x11;
    }
    w->x56C = 0;
    near = 0;
    if (em->dmPart->rad < 36000000.0f) {
        near = 1;
    }
    fabsf(Muku(&em->pos, &em->x328, em->rot.y, 3.14159274f));
    dmg = em2cSetDmVal(em);
    LifeDownSet2(em, dmg, 0, 1);
    w->dmgTotal += dmg;
    p = em->getPartsPtr(0);
    dist = (cam->param.pos.x - p->worldPos.x) * (cam->param.pos.x - p->worldPos.x) +
           (cam->param.pos.y - p->worldPos.y) * (cam->param.pos.y - p->worldPos.y) +
           (cam->param.pos.z - p->worldPos.z) * (cam->param.pos.z - p->worldPos.z);
    switch (em->dmWep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xE:
    case 0x11:
    case 0x15:
    case 0x26:
    case 0x2B:
        if (w->flags & 0x800) {
            EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
        }
        break;
    case 0x10:
        if (w->flags & 0x800) {
            EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
        }
        break;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        if (w->flags & 0x800) {
            EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            if (dist < 16000000.0f) {
                if (w->flags & 0x800) {
                    EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
                } else {
                    EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
                }
            } else {
                if (w->flags & 0x800) {
                    EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
                } else {
                    EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
                }
            }
        } else {
            if (w->flags & 0x800) {
                EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
            }
        }
        break;
    case 0x17:
    case 0x2A:
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        if (dist < 16000000.0f) {
            if (w->flags & 0x800) {
                EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
            }
        } else {
            if (w->flags & 0x800) {
                EmDmBloodSet2(em, 0x24, 8, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x24, 3, 0, 0, 0);
            }
        }
        break;
    }
    SndCall(8, 6, &em->pos, em->id, 0, em);
}

void cEm2c::move()
{
    Em2cWork* w = EM2C_WK(this);
    cAtariInfo* at;
    f32 len;
    f32 moved;
    u16 atFlags;
    int n;

    motFlags2 &= ~0x40000000;
    if (xFC != 0) {
        switch (type) {
        case 0:
        default:
            em2cDmCk(this);
            break;
        case 1:
            em2cTailDmCk(this);
            break;
        }
    }
    clearStatus(3);
    w->flags &= ~0x007F91FE;
    if (w->atkWait) {
        w->atkWait--;
    }
    if (w->jumpWait) {
        w->jumpWait--;
    }
    if (w->dmGuard) {
        w->dmGuard--;
    }
    if (w->guardCnt) {
        w->guardCnt--;
    }
    if (w->atkWait == 0 && em2cDeadCk(pPL)) {
        w->atkWait = 10;
    }
    if (w->x570) {
        w->x570--;
    }
    if (w->x574) {
        w->x574--;
    }
    if (w->atkCnt > 450) {
        w->flags |= 0x2000;
    } else {
        w->flags &= ~0x2000;
    }
    em2cRouteCk(this);
    Em2c_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    w->flags &= ~0x400;
    if (seFlags28B & 0x80) {
        w->flags |= 0x400;
    }
    if (w->wallNrm.y < 0.699999988f) {  // member calls in the arms: `&atari` is PRE'd into both (`mr r30,r0` copy)
        atari.set(10, 600.0f, 250.0f);
    } else {
        atari.set(10, 600.0f, 500.0f);
    }
    at = &atari;
    em2cNeckMove(this);
    partsWorldCalc();
    em2cScaleCompress(this);
    len = SQRTF((oldPos.x - pos.x) * (oldPos.x - pos.x) + (oldPos.z - pos.z) * (oldPos.z - pos.z));
    atFlags = atari.flags;
    if (w->flags & 0x1000) {
        at->flags &= ~0x200;
    }
    EmAtCheck(this);
    at->move();
    if (w->flags & 0x60) {
        at->set(5, 180.0f, 500.0f);
        if (!(w->flags & 0x20)) {
            SatMgr.checkAir(this, 0x980800);
        }
    } else {
        at->set(5, 600.0f, 500.0f);
        SatMgr.check(this, 0);
    }
    atari.flags = atFlags;
    moved = SQRTF((pos.x - oldPos.x) * (pos.x - oldPos.x) + (pos.z - oldPos.z) * (pos.z - oldPos.z));
    if (moved < len * 0.5f) {
        w->stuckCnt++;
    } else {
        w->stuckCnt = 0;
    }
    em2cClothMove(this);
    em2cFootSeMove(this);
    if ((w->flags & 0x800) && hp > 0) {
        n = w->effTimer;
        if (n) {
            w->effTimer--;
        } else {
            w->effTimer = 8;
            EstSet((int) this, -1, 0, 0, 0x24, 5, 0, 0, (u32) this, (void*) n);
        }
    }
    em2cBreathSeStopCk(this);
}

void em2cInitRtnSet(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero = 0;

    w->flags = zero;
    w->atkWait = zero;
    w->jumpWait = zero;
    w->x558 = 1.0f;
    w->wallNrm.y = 1.0f;
    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    w->wallNrm.x = 0.0f;
    w->wallNrm.z = 0.0f;
    w->humTimer = Rnd() % 90 + 90;
    w->x6BC = 5;
    w->breathTimer = 0x1D;
    w->x584 = 0.0f;
    w->dmgTotal = zero;
    w->atkCnt = zero;
    w->x56C = zero;
    w->dmGuard = zero;
    w->x570 = zero;
    w->effTimer = zero;
    w->x574 = zero;
    w->pTail = (cEm*) zero;
    w->x534 = zero;
    w->homePos = em->pos;
    em->setStatus(5);
    switch (em->type) {
    case 0:
    default:
        switch (em->x38D) {
        case 0:
        default:
            EmRoutineSet(em, 1, 1, 0, 0);
            MotionSetCore(em, &em->mot, ARC(7), 0, 0, 5, 0);
            break;
        case 1:
            AtariOff(&em->atari, 0xFCFF);
            EmRoutineSet(em, 1, 0x21, 0, 0);
            MotionSetCore(em, &em->mot, ARC(0x49), 0, 0, 5, 0);
            break;
        case 2:
            AtariOff(&em->atari, 0xFCFF);
            EmRoutineSet(em, 1, 0x23, 0, 0);
            MotionSetCore(em, &em->mot, ARC(0x49), 0, 0, 5, 0);
            break;
        }
        break;
    case 1:
        AtariOff(&em->atari, 0xFCFF);
        switch (em->x38D) {
        case 0:
        default:
            EmRoutineSet(em, 1, 0x24, 0, 0);
            break;
        case 3:
            EmRoutineSet(em, 1, 0x25, 0, 0);
            break;
        }
        MotionSetCore(em, &em->mot, ARC(0x87), 0, 0, 5, 0);
        break;
    }
}

static void em2c_R0_Init(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero;

    em->x12F = 5;
    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(5), ARC(6)) == 0) {
            pLog->err(0, 0, "em2c() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    case 1:
        if (em->modelInit(ARC(0x86), ARC(6)) == 0) {
            pLog->err(0, 0, "em2c() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    }
    EspDataLoad((u32) ARC(4), 0x24, 0);
    w->pCtrl12 = GetCtrlCtrl12();
    em2cTexrenderInit(em);
    em->motFlip = em2c_xflip_tbl;
#line 1623 "D:/Bio4/Prog/em2c.cpp"
    em->p2A4 = MEM_ALLOC(0x98, 1, 13);
    {
        static const Vec ofs = {0.0f, 0.0f, 0.0f};
        static const Vec size = {10000.0f, 10000.0f, 10000.0f};
        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    atariInitF(&em->atari, 0.0f, 0.0f, 0.0f, 600.0f, 500.0f, 500.0f, 1000.0f, 1, 0x2000, 10);
    em->litArea.on(1);
    switch (em->type) {
    case 0:
    default:
        YarareInit(em, 0.0f, -50.0f, 0.0f, 190.0f, 100.0f, 6, 1);
        YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 240.0f, 50.0f, 2, 1);
        YarareAdd(em, &w->hit[1], 0.0f, 0.0f, 0.0f, 240.0f, 200.0f, 3, 1);
        YarareAdd(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 220.0f, 50.0f, 4, 1);
        YarareAdd(em, &w->hit[3], -400.0f, 0.0f, 0.0f, 140.0f, 400.0f, 8, 3);
        YarareAdd(em, &w->hit[4], -400.0f, 0.0f, 0.0f, 110.0f, 500.0f, 9, 3);
        YarareAdd(em, &w->hit[5], 0.0f, 0.0f, 0.0f, 140.0f, 400.0f, 0xC, 3);
        YarareAdd(em, &w->hit[6], 0.0f, 0.0f, 0.0f, 110.0f, 500.0f, 0xD, 3);
        YarareAdd(em, &w->hit[7], 0.0f, -500.0f, 0.0f, 150.0f, 500.0f, 0x10, 1);
        YarareAdd(em, &w->hit[8], 0.0f, -600.0f, 0.0f, 120.0f, 600.0f, 0x11, 1);
        YarareAdd(em, &w->hit[9], 0.0f, -500.0f, 0.0f, 150.0f, 500.0f, 0x14, 1);
        YarareAdd(em, &w->hit[10], 0.0f, -600.0f, 0.0f, 120.0f, 600.0f, 0x15, 1);
        YarareAdd(em, &w->hit[11], 0.0f, 0.0f, 0.0f, 190.0f, 50.0f, 0x1E, 1);
        YarareAdd(em, &w->hit[12], 0.0f, 0.0f, 0.0f, 190.0f, 50.0f, 0x1F, 1);
        break;
    case 1:
        YarareInit(em, 0.0f, 0.0f, 0.0f, 100.0f, 100.0f, 1, 1);
        break;
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em2cClothSet(em);
    w->espKind = EspPullCoreKind();
    w->espKind2 = EspPullCoreKind();
    switch (em->type) {
    case 0:
    default:
        EstSet((int) em, -1, 0, 0, 0x24, 1, 0, w->espKind, (u32) em, (void*) zero);
        EstSet((int) em, -1, 0, 0, 0x24, 6, 0, w->espKind, (u32) em, (void*) zero);
        break;
    case 1:
        break;
    }
    w->startPos = em->pos;
    w->startRot = em->rot;
    em2cInitRtnSet(em);
    MotionMoveF(em, 0);
    em2c_R0_Move(em);
    OSReport("em2c free size = 0x%x\n", 0x6C0);
}

static void em2c_R0_Move(cEm2c* em)
{
    Em2c_R1_move_tbl[em->xFD * 2](em);
    Em2c_R1_move_tbl[em->xFD * 2 + 1](em);
}

static void em2c_R1_br_Dummy(cEm2c* em)
{
}

// ---------------------------------------------------------------------------------------------
// Routine 1: floor

static void em2c_R1_Wait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int r;

    w->flags |= 0x180;
    w->flags |= 0x40000;
    switch (em->xFE) {
    case 0:
        if (em->motFlags & 0x40) {
            MotionSetCore(em, &em->mot, ARC(7), 0, 30, 0x45, 0);
        } else {
            MotionSetCore(em, &em->mot, ARC(7), 0, 30, 5, 0);
        }
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if ((s16) pG->pl_life <= 0) {
            break;
        }
        if (w->routeAngAbs > 1.57079637f && pG->x4F88 > 3) {
            w->atkWait = 0;
        }
        if (em->plDist2 > 25000000.0f && pG->x4F88 > 3) {
            w->atkWait = 0;
        }
        if (em2cDeadCk(em) && pG->x4F88 > 1) {
            w->atkWait = 0;
        }
        if (w->atkWait) {
            break;
        }
        r = em2cAmbushCk(em);
        if (r) {
            break;
        }
        if (w->targetAngAbs > 2.09439516f) {
            EmRoutineSet(em, 1, 3, r, r);
        } else if (w->flags & 0x800) {
            EmRoutineSet(em, 1, 0x18, r, r);
        } else if (em->plDist2 > 25000000.0f && Rnd() % 10 > 7 && pG->room_id == 0x221 &&
                   fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f)) > 1.57079637f) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
        } else if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f && w->x570 == 0) {
            EmRoutineSet(em, 1, 2, 0, 0);
        } else {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
    if (em2cLockCk(em)) {
        w->lockCnt++;
        if (w->lockCnt > 5) {
            if (Rnd() % 3 == 0) {
                if (em2cToCeilingCk(em)) {
                    return;
                }
            }
            EmRoutineSet(em, 1, 5, 0, 0);
            return;
        }
    } else {
        w->lockCnt = 0;
    }
    if (em2cFallCk(em)) {
        return;
    }
    em2cBreathSe(em);
}

static void em2c_R1_Walk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec pos;
    f32 ang;
    int seq;

    w->flags |= 0x100;
    w->flags |= 0x40000;
    switch (em->xFE) {
        do { } while (0);  // dead loop before the label: fresh `li 0` for the zero stores
    case 0:
        if (em->motFlags & 0x40) {
            w->blendSeq = 0xB;
        } else {
            w->blendSeq = 0x20;
        }
        w->blendM0 = ARC(8);
        w->blendM1 = ARC(0xE);
        w->blendM2 = ARC(0xD);
        w->blendA = (int) ARC(9);
        w->blendB = 0;
        w->blendD = 5;
        w->blendC = 0;
        w->blendCnt = 5;
        w->blendVal = 0.0f;
        em->xFE++;
    case 1:
        ang = Muku(&em->pos, &w->targetPos, em->rot.y, 3.14159274f);
        if (ang > 0.785398185f) {
            ang = 0.785398185f;
        }
        if (ang < -0.785398185f) {
            ang = -0.785398185f;
        }
        {
            f32 blend = ang * 324.676086f; // separate statement: pool order 324.676086, 0.8, 0.2
            w->blendVal = w->blendVal * 0.800000012f + blend * 0.200000003f;
        }
        em->rot.y += w->blendVal * 0.00392156886f * 0.0490873866f;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em2cBlendMotSetI(em, w->blendM0, w->blendM1, w->blendM2, w->blendA, 0, 0, w->blendD);
        MotionMoveF(em, 0);
        break;
    }
    em2cDoorOpenCk(em);
    if (em2cJumpDownCk(em)) {
        return;
    }
    if (em2cWallOverCk(em)) {
        return;
    }
    if (em2cLockCk(em)) {
        w->lockCnt++;
        if (w->lockCnt > 5) {
            switch (pG->wep_no) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 0x10:
            case 0x11:
            case 0x26:
            case 0x2B:
                if (Rnd() % 10 > 8) {
                    EmRoutineSet(em, 1, 7, 0, 0);
                    return;
                }
                break;
            }
            if (Rnd() % 3 == 0) {
                if (em2cToCeilingCk(em)) {
                    return;
                }
            }
            EmRoutineSet(em, 1, 5, 0, 0);
            return;
        }
    } else {
        w->lockCnt = 0;
    }
    if (w->atkWait) {
        if (em->plDist2 < 2250000.0f && w->routeAngAbs < 0.785398185f) {
            EmRoutineSet(em, 1, 6, 0, 0);
        } else {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        return;
    }
    if (w->flags & 1) {
        f32 plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f));
        f32 lim = 12250000.0f;
        if (plAng > 1.57079637f) {
            lim = 1690000.0f;
        }
        if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
            if (pPL->xFC != 0 && pPL->xFD != 6 && ItemMgr.bulletNumCurrent() == 0 && Rnd() % 10 > 4 &&
                !(w->flags & 0x800)) {
                EmRoutineSet(em, 1, 7, 0, 0);
                return;
            }
            if (plAng > 1.57079637f) {
                GetPlPos(&pos, 0, 20.0f);
                if ((em->pos.x - pos.x) * (em->pos.x - pos.x) + (em->pos.z - pos.z) * (em->pos.z - pos.z) <
                    6250000.0f) {
                    EmRoutineSet(em, 1, 9, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 0xA, 0, 0);
                }
                return;
            }
            if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                EmRoutineSet(em, 1, 9, 0, 0);
                return;
            }
            if (Rnd() % 10 > 7 && plAng < 1.22173047f) {
                if (Rnd() % 10 > 8) {
                    EmRoutineSet(em, 1, 7, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 8, 0, 0);
                }
                return;
            }
            switch (Rnd() % 3) {
            default:
                EmRoutineSet(em, 1, 0xB, 0, 0);
                return;
            case 1:
                if (plAng < 1.57079637f) {
                    EmRoutineSet(em, 1, 0xC, 0, 0);
                    return;
                }
                // fallthrough into case 2: its label has two uses, so the arm gets fresh `li 1`s
            case 2:
                EmRoutineSet(em, 1, 0xA, 0, 0);
                return;
            }
        }
    }
    if (em->plDist2 < 1440000.0f && fabsf(em->mat[1][3] - pPL->pos.y) < 500.0f && w->routeAngAbs > 1.04719758f &&
        w->routeAngAbs < 1.91986215f) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 1.0f;
        PSMTXMultVecSR(em->mat, &pos, &pos);
        em->rot.y = atan2f(pos.x, pos.z);
        EmRoutineSet(em, 1, 6, 0, 0);
        return;
    }
    if (em2cAmbushCk(em)) {
        return;
    }
    if (w->plDist > 10000.0f) {
        if (w->x570 == 0) {
            EmRoutineSet(em, 1, 2, 0, 0);
            return;
        }
    } else if (w->x570 == 0 && (w->flags & 1) && w->plDist > 5000.0f) {
        EmRoutineSet(em, 1, 2, 0, 0);
        return;
    }
    w->atkCnt++;
    if (em2cFallCk(em)) {
        return;
    }
    em2cBreathSe(em);
}

// Wait / Turn180 end: the next floor routine by the target angle, the hide state and chance.
static inline void em2cNextWalkSet(cEm2c* em, Em2cWork* w, int r)
{
    if (w->targetAngAbs > 2.09439516f) {
        EmRoutineSet(em, 1, 3, r, r);
    } else if (w->flags & 0x800) {
        EmRoutineSet(em, 1, 0x18, r, r);
    } else if (em->plDist2 > 25000000.0f && Rnd() % 10 > 7 && pG->room_id == 0x221 &&
               fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f)) > 1.57079637f) {
        EmRoutineSet(em, 1, 0x12, 0, 0);
    } else if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f && w->x570 == 0) {
        EmRoutineSet(em, 1, 2, 0, 0);
    } else {
        EmRoutineSet(em, 1, 1, 0, 0);
    }
}

static void em2c_R1_Dash(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec pos;
    int over;

    w->flags |= 0x180;
    switch (em->xFE) {
        do { } while (0);  // dead loop before the label: fresh `li 0` for the zero stores
    case 0:
        if (em->motFlags & 0x40) {
            w->blendSeq = 8;
        } else {
            w->blendSeq = 0;
        }
        w->blendM0 = ARC(0xA);
        w->blendM1 = ARC(0xC);
        w->blendA = (int) ARC(0xB);
        w->blendB = 0;
        w->blendD = 5;
        w->blendCnt = 5;
        w->blendVal = 0.0f;
        if (em->plDist2 > 25000000.0f) {
            w->blendVal = 255.0f;
        }
        w->timer = 15;
        w->timer8 = 0;
        w->x570 = 150;
        em->xFE++;
    case 1:
        em2cTurnTo(em, &w->routePos, 0.0981747732f);
        if (w->timer8 % 6 == 0) {
            EstSet((int) em, -1, 0, 0, 0x24, 0xE, 0, w->espKind2, (u32) em, 0);
        }
        if (em->plDist2 > 25000000.0f && pG->x4F88 > 1) {
            w->blendVal += 16.0f;
            if (w->blendVal > 255.0f) {
                w->blendVal = 255.0f;
            }
        } else {
            w->blendVal -= 16.0f;
            if (w->blendVal < 0.0f) {
                w->blendVal = 0.0f;
            }
        }
        em2cBlendMotSet2I(em, w->blendM0, w->blendM1, w->blendA, 0, w->blendD);
        if (MotionMoveF(em, 0)) {
            if (w->timer == 0) {
                EmRoutineSet(em, 1, 0, 0, 0);
                break;
            }
            w->timer--;
        }
        w->timer8++;
        break;
    }
    em2cDoorOpenCk(em);
    if (em2cJumpDownCk(em)) {
        return;
    }
    over = em2cWallOverCk(em);
    if (over) {
        return;
    }
    if (em2cLockCk(em)) {
        w->lockCnt++;
        if (w->lockCnt > 5) {
            if (Rnd() % 10 > 5) {
                EmRoutineSet(em, 1, 5, over, over);
                return;
            }
            w->lockCnt = over;
        }
    } else {
        w->lockCnt = 0;
    }
    if (w->flags & 1) {
        f32 plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f));
        f32 lim = 12250000.0f;
        if (plAng > 1.57079637f) {
            lim = 1690000.0f;
        }
        if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
            if (pPL->xFC != 0 && pPL->xFD != 6 && ItemMgr.bulletNumCurrent() == 0 && Rnd() % 10 > 4 &&
                !(w->flags & 0x800)) {
                EmRoutineSet(em, 1, 7, 0, 0);
                return;
            }
            if (plAng > 1.57079637f) {
                GetPlPos(&pos, 0, 20.0f);
                if ((em->pos.x - pos.x) * (em->pos.x - pos.x) + (em->pos.z - pos.z) * (em->pos.z - pos.z) <
                    6250000.0f) {
                    EmRoutineSet(em, 1, 9, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 0xA, 0, 0);
                }
                return;
            }
            if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                EmRoutineSet(em, 1, 9, 0, 0);
                return;
            }
            if (Rnd() % 10 > 7 && plAng < 1.22173047f) {
                EmRoutineSet(em, 1, 8, 0, 0);
                return;
            }
            switch (Rnd() % 3) {
            default:
                EmRoutineSet(em, 1, 0xB, 0, 0);
                return;
            case 1:
                if (plAng < 1.57079637f) {
                    EmRoutineSet(em, 1, 0xC, 0, 0);
                    return;
                }
                // fallthrough into case 2: its label has two uses, so the arm gets fresh `li 1`s
            case 2:
                EmRoutineSet(em, 1, 0xA, 0, 0);
                return;
            }
        }
    }
    if (em->plDist2 < 1440000.0f && fabsf(em->mat[1][3] - pPL->pos.y) < 500.0f && w->routeAngAbs > 1.04719758f &&
        w->routeAngAbs < 1.91986215f) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 1.0f;
        PSMTXMultVecSR(em->mat, &pos, &pos);
        em->rot.y = atan2f(pos.x, pos.z);
        EmRoutineSet(em, 1, 6, 0, 0);
        return;
    }
    w->atkCnt++;
    if (em2cFallCk(em)) {
        return;
    }
    em2cBreathSe(em);
}

static void em2c_R1_Turn180(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    f32 d;
    int r;

    w->flags |= 0x100;
    switch (em->xFE) {
    case 0:
        if (w->flags & 0x800) {
            if (em->motFlags & 0x40) {
                MotionSetCore(em, &em->mot, ARC(0x34), (int) ARC(0x35), 5, 0x41, 0);
            } else {
                MotionSetCore(em, &em->mot, ARC(0x34), (int) ARC(0x35), 5, 1, 0);
            }
        } else if (em->motFlags & 0x40) {
            MotionSetCore(em, &em->mot, ARC(0x13), (int) ARC(0x14), 5, 0x41, 0);
        } else {
            MotionSetCore(em, &em->mot, ARC(0x13), (int) ARC(0x14), 5, 1, 0);
        }
        w->turnAng = em->rot.y + 3.14159274f;
        w->timer = 60;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 0x10) {
            d = Muku(&em->pos, &w->targetPos, w->turnAng, 0.0981747732f);
            w->turnAng += d;
            w->turnAng = LIMIT_ANGLE(w->turnAng);
            em->rot.y += d;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            r = em2cAmbushCk(em);
            if (r) {
                break;
            }
            em2cNextWalkSet(em, w, r);
        }
        break;
    }
    if (em2cFallCk(em)) {
        return;
    }
    em2cBreathSe(em);
}

// Wait end without the hide options: turn, dash or walk.
static inline void em2cNextWalkSet2(cEm2c* em, Em2cWork* w)
{
    if (w->targetAngAbs > 2.09439516f) {
        EmRoutineSet(em, 1, 3, 0, 0);
    } else if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
        EmRoutineSet(em, 1, 2, 0, 0);
    } else {
        EmRoutineSet(em, 1, 1, 0, 0);
    }
}

// Side-step start: the step motion mirrored towards the free side, with the sound / effect.
static inline void em2cSideStepMotSet(cEm2c* em, Em2cWork* w, int side)
{
    if (side) {
        MotionSetCore(em, &em->mot, ARC(0xF), (int) ARC(0x10), 5, 0x41, 0);
    } else {
        MotionSetCore(em, &em->mot, ARC(0xF), (int) ARC(0x10), 5, 1, 0);
    }
    EstSet((int) em, -1, 0, 0, 0x24, 9, 0, w->espKind2, (u32) em, 0);
}

static void em2c_R1_Ambush(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    w->flags |= 0x180;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(7), 0, 30, 5, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->plDist2 < 100000000.0f) {
            em->xFE++;
        }
        break;
    case 2:
        if (Muku(&em->pos, &pPL->pos, em->rot.y, 3.14159274f) < 0.0f) {
            MotionSetCore(em, &em->mot, ARC(0xF), (int) ARC(0x10), 5, 0x41, 0);
        } else {
            MotionSetCore(em, &em->mot, ARC(0xF), (int) ARC(0x10), 5, 1, 0);
        }
        EstSet((int) em, -1, 0, 0, 0x24, 9, 0, w->espKind2, (u32) em, 0);
        SndCall(8, 0x40, &em->pos, em->id, 0, em);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em2cNextWalkSet2(em, w);
        }
        break;
    }
}

static void em2c_R1_SideStep(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec a;
    Vec b;
    f32 plAng;
    f32 lim;
    int side;

    w->flags |= 0x180;
    switch (em->xFE) {
    case 0:
        side = Rnd() & 1;
        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = 2000.0f;
        b.y = 500.0f;
        b.z = 0.0f;
        PSMTXMultVec(em->mat, &a, &a);
        PSMTXMultVec(em->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x383830)) {
            side = 1;
        }
        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = -2000.0f;
        b.y = 500.0f;
        b.z = 0.0f;
        PSMTXMultVec(em->mat, &a, &a);
        PSMTXMultVec(em->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x383830)) {
            side = 0;
        }
        em2cSideStepMotSet(em, w, side);
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        SndCall(8, 0x40, &em->pos, em->id, 0, em);
        em->xFE++;
    case 1:
        if ((w->flags & 1) && w->routeAngAbs < 0.52359879f) {
            em2cTurnTo(em, &w->routePos, 0.0981747732f);
        }
        if (MotionMoveF(em, 0)) {
            em2cNextWalkSet2(em, w);
            break;
        }
        if ((em->seFlags28B & 4) && (w->flags & 1)) {
            plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f));
            lim = 12250000.0f;
            if (plAng > 1.57079637f) {
                lim = 4000000.0f;
            }
            if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
                if (pPL->xFC != 0 && pPL->xFD != 6 && ItemMgr.bulletNumCurrent() == 0 && Rnd() % 10 > 4 &&
                    !(w->flags & 0x800)) {
                    EmRoutineSet(em, 1, 7, 0, 0);
                    break;
                }
                if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                    EmRoutineSet(em, 1, 9, 0, 0);
                    break;
                }
                if (Rnd() % 10 > 7 && plAng < 1.22173047f) {
                    if (Rnd() % 10 > 8) {
                        EmRoutineSet(em, 1, 7, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 8, 0, 0);
                    }
                    break;
                }
                switch (Rnd() % 3) {
                default:
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                    break;
                case 1:
                    EmRoutineSet(em, 1, 0xC, 0, 0);
                    break;
                case 2:
                    EmRoutineSet(em, 1, 0xA, 0, 0);
                    break;
                }
            } else if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    }
}

static void em2c_R1_BackJump(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int fe;
    int r;

    w->flags |= 0x80;
    if (em->xFF == 0) {
        w->flags |= 0x100;
    }
    fe = em->xFE;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x27), (int) ARC(0x28), 5, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x24, 0x2B, 0, 0, (u32) em, (void*) fe);
        w->timer = 10;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
            em2cTurnTo(em, &w->routePos, 0.196349546f);
        }
        if (MotionMoveF(em, 0)) {
            r = em2cAmbushCk(em);
            if (r) {
                break;
            }
            if (em->plDist2 > 25000000.0f && Rnd() % 10 > 5 && pG->room_id == 0x221 &&
                fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f)) > 1.57079637f) {
                EmRoutineSet(em, 1, 0x12, r, r);
            } else if (pPL->xFC != 0 && pPL->xFD != 6 && ItemMgr.bulletNumCurrent() == 0 && Rnd() % 10 > 4 &&
                       !(w->flags & 0x800)) {
                EmRoutineSet(em, 1, 7, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

static void em2c_R1_Threat(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    f32 plAng;
    f32 lim;
    int fe;

    w->flags |= 0x50000;
    fe = em->xFE;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x44), 0, 5, 1, 0);
        w->dmgTotal = fe;
        w->timer = 20;
        em->xFE++;
    case 1:
        em2cTurnTo(em, &pPL->pos, 0.157079637f);
        if (MotionMoveF(em, 0)) {
            if (w->flags & 1) {
                plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f));
                lim = 12250000.0f;
                if (plAng > 1.57079637f) {
                    lim = 4000000.0f;
                }
                if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
                    if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                        EmRoutineSet(em, 1, 9, 0, 0);
                        break;
                    }
                    switch (Rnd() % 3) {
                    default:
                        EmRoutineSet(em, 1, 0xB, 0, 0);
                        break;
                    case 1:
                        EmRoutineSet(em, 1, 0xC, 0, 0);
                        break;
                    case 2:
                        EmRoutineSet(em, 1, 0xA, 0, 0);
                        break;
                    }
                    break;
                }
            }
            if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else if (em->frame > 24.7000008f && em->frame < 25.2999992f) {
            SndCall(8, 0x45, &em->pos, em->id, 0, em);
        }
        break;
    }
}

static void em2c_R1_AtkSign(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int end;

    w->flags |= 0x50000;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x25), (int) ARC(0x26), 5, 1, 0);
        w->timer = 20;
        em->xFE++;
    case 1:
        em2cTurnTo(em, &pPL->pos, 0.157079637f);
        end = MotionMoveF(em, 0);
        if (end) {
            if (w->flags & 1) {
                f32 plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f));
                f32 lim = 12250000.0f;
                if (plAng > 1.57079637f) {
                    lim = 4000000.0f;
                }
                if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
                    if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                        EmRoutineSet(em, 1, 9, 0, 0);
                        break;
                    }
                    switch (Rnd() % 3) {
                    default:
                        EmRoutineSet(em, 1, 0xB, 0, 0);
                        break;
                    case 1:
                        EmRoutineSet(em, 1, 0xC, 0, 0);
                        break;
                    case 2:
                        EmRoutineSet(em, 1, 0xA, 0, 0);
                        break;
                    }
                    break;
                }
            }
            if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else if ((em->seFlags28B & 4) && (Rnd() & 1) && pG->x4F88 > 3) {
            if (w->flags & 1) {
                f32 plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f));
                f32 lim = 12250000.0f;
                if (plAng > 1.57079637f) {
                    lim = 4000000.0f;
                }
                if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
                    if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                        EmRoutineSet(em, 1, 9, end, end);
                        break;
                    }
                    switch (Rnd() % 3) {
                    default:
                        EmRoutineSet(em, 1, 0xB, 0, 0);
                        break;
                    case 1:
                        EmRoutineSet(em, 1, 0xC, 0, 0);
                        break;
                    case 2:
                        EmRoutineSet(em, 1, 0xA, 0, 0);
                        break;
                    }
                    break;
                }
            }
            if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    }
}

static void em2c_R1_Atk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero;
    int r;

    w->flags |= 0x80;
    w->flags |= 0x10000;
    switch (em->xFE) {
    case 0:
        if (w->routeAng < 0.0f) {
            MotionSetCore(em, &em->mot, ARC(0x15), (int) ARC(0x16), 10, 1, 0);
        } else {
            MotionSetCore(em, &em->mot, ARC(0x15), (int) ARC(0x16), 10, 0x41, 0);
        }
        r = Rnd() & 1;
        if (r) {
            w->dmgTotal = 1000;
        } else {
            w->dmgTotal = r;
        }
        zero = 0;
        EstSet((int) em, -1, 0, 0, 0x24, 2, 0, 0, (u32) em, (void*) zero);
        EstSet((int) em, -1, 0, 0, 0x24, 0x10, 0, 0, (u32) em, (void*) zero);
        w->atkCnt = zero;
        w->timer = 15;
        w->atkHit = zero;
        em->flags_3C8 &= ~4;
        em->xFE++;
    case 1:
        if (em->xFF == 0 && w->timer) {
            w->timer--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.157079637f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(0xB);
            }
            w->dmgTotal = 0;
            w->atkWait = 5;
            if (w->atkHit) {
                if (em->plDist2 < 4000000.0f) {
                    EmRoutineSet(em, 1, 6, 0, 0);
                    break;
                }
                if (pG->room_id == 0x221) {
                    EmRoutineSet(em, 1, 0x12, 0, 0);
                    break;
                }
            }
            EmRoutineSet(em, 1, 0, 0, 0);
        } else if (em->seFlags28B & 1) {
            em2cDoorOpenCk2(em);
            if (em->motFlags & 0x40) {
                em2cAtkCk(em, 0, 0xB);
                em2cAtkCk(em, 0, 0xC);
                em2cAtkCk(em, 0, 0xD);
            } else {
                em2cAtkCk(em, 0, 7);
                em2cAtkCk(em, 0, 8);
                em2cAtkCk(em, 0, 9);
            }
        }
        break;
    }
}

// Attack end: the next routine by whether it hit (side-step / hide close by, else wait).
static inline void em2cAtkEndSet(cEm2c* em, Em2cWork* w)
{
    if (w->atkHit == 0) {
        GameAddPoint(0xB);
    }
    w->dmgTotal = 0;
    w->atkWait = 5;
    if (w->atkHit) {
        if (em->plDist2 < 4000000.0f) {
            EmRoutineSet(em, 1, 6, 0, 0);
            return;
        }
        if (pG->room_id == 0x221) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
            return;
        }
    }
    EmRoutineSet(em, 1, 0, 0, 0);
}

// Attack hit frames: the claw parts of the swinging side.
static inline void em2cClawAtkCk(cEm2c* em, int no)
{
    if (em->motFlags & 0x40) {
        em2cAtkCk(em, no, 0xB);
        em2cAtkCk(em, no, 0xC);
        em2cAtkCk(em, no, 0xD);
    } else {
        em2cAtkCk(em, no, 7);
        em2cAtkCk(em, no, 8);
        em2cAtkCk(em, no, 9);
    }
}

static void em2c_R1_JumpAtk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero;
    int hit;
    int r;

    w->flags |= 0x80;
    w->flags |= 0x10000;
    switch (em->xFE) {
    case 0:
        if (w->routeAng < 0.0f) {
            MotionSetCore(em, &em->mot, ARC(0x17), (int) ARC(0x18), 10, 1, 0);
        } else {
            MotionSetCore(em, &em->mot, ARC(0x17), (int) ARC(0x18), 10, 0x41, 0);
        }
        r = Rnd() & 1;
        if (r) {
            w->dmgTotal = 1000;
        } else {
            w->dmgTotal = r;
        }
        zero = 0;
        EstSet((int) em, -1, 0, 0, 0x24, 2, 0, 0, (u32) em, (void*) zero);
        EstSet((int) em, -1, 0, 0, 0x24, 0x14, 0, w->espKind2, (u32) em, (void*) zero);
        w->timer = 3;
        w->timer8 = 25;
        w->walkMode = 5;
        w->atkCnt = zero;
        w->atkHit = zero;
        em->flags_3C8 &= ~4;
        em->xFE++;
    case 1:
        if (w->walkMode) {
            w->walkMode--;
        } else if (w->timer8) {
            hit = w->atkHit;
            if (hit == 0) {
                w->timer8--;
                if (fabsf(Muku(&pPLS->pos, &em->pos, pPLS->rot.y, 3.14159274f)) < 1.04719758f) {
                    ActBtn.set(0x25, 0xB, (int) em2cEscapeAction, (int) em, 1, 3, 0, hit);
                } else {
                    ActBtn.set(0x13, 0xB, (int) em2cSitAction, (int) em, 1, 3, 0, hit);
                }
            }
        }
        if (w->timer) {
            w->timer--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.157079637f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            em2cAtkEndSet(em, w);
        } else if (em->seFlags28B & 1) {
            em2cClawAtkCk(em, 1);
        }
        break;
    }
}

static void em2c_R1_BackKnuckle(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero;
    int fe;
    int hit;
    int r;

    w->flags |= 0x80;
    w->flags |= 0x10000;
    fe = em->xFE;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x19), (int) ARC(0x1A), 10, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x24, 0x14, 0, w->espKind2, (u32) em, (void*) fe);
        r = Rnd() & 1;
        if (r) {
            w->dmgTotal = 1000;
        } else {
            w->dmgTotal = r;
        }
        zero = 0;
        EstSet((int) em, -1, 0, 0, 0x24, 2, 0, 0, (u32) em, (void*) zero);
        w->timer = 15;
        w->timer8 = 25;
        w->walkMode = 5;
        w->atkCnt = zero;
        w->atkHit = zero;
        em->flags_3C8 &= ~4;
        em->xFE++;
    case 1:
        if (w->walkMode) {
            w->walkMode--;
        } else if (w->timer8) {
            hit = w->atkHit;
            if (hit == 0) {
                w->timer8--;
                ActBtn.set(0x13, 0xB, (int) em2cSitAction, (int) em, 1, 3, 0, hit);
            }
        }
        if (MotionMoveF(em, 0)) {
            em2cAtkEndSet(em, w);
        } else if (em->seFlags28B & 1) {
            em2cClawAtkCk(em, 2);
        }
        break;
    }
}

static void em2c_R1_TailAtk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero;
    int hit;

    w->flags |= 0x80;
    w->flags |= 0x30000;
    switch (em->xFE) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, &em->mot, ARC(0x1B), (int) ARC(0x1C), 10, 1, 0);
        } else {
            MotionSetCore(em, &em->mot, ARC(0x1B), (int) ARC(0x1C), 10, 0x41, 0);
        }
        zero = 0;
        w->atkCnt = zero;
        EstSet((int) em, -1, 0, 0, 0x24, 2, 0, 0, (u32) em, (void*) zero);
        EstSet((int) em, -1, 0, 0, 0x24, 0x12, 0, w->espKind2, (u32) em, (void*) zero);
        w->timer = 15;
        w->timer8 = 25;
        w->walkMode = 5;
        w->atkHit = zero;
        em->flags_3C8 &= ~4;
        w->mode = zero;
        em->xFE++;
    case 1:
        if (w->walkMode) {
            w->walkMode--;
        } else if (w->timer8) {
            hit = w->mode;
            if (hit == 0) {
                w->timer8--;
                ActBtn.set(0x25, 0xB, (int) em2cEscapeAction, (int) em, 1, 3, 0, hit);
            }
        }
        if (MotionMoveF(em, 0)) {
            em2cAtkEndSet(em, w);
            break;
        }
        if (pPL->xFC == 1 && (pPL->dmg.x1 & 0x80)) {
            pPL->dmg.x1 = 8;
        }
        if (em->seFlags28B & 1) {
            w->atkHit = 0;
            em->flags_3C8 &= ~4;
            em2cAtkCk(em, 5, 0x3C);
            em2cAtkCk(em, 5, 0x3D);
            em2cAtkCk(em, 5, 0x3E);
            em2cAtkCk(em, 5, 0x3F);
            em2cAtkCk(em, 5, 0x40);
            em2cAtkCk(em, 5, 0x41);
            em2cAtkCk(em, 5, 0x42);
            em2cAtkCk(em, 5, 0x43);
            em2cAtkCk(em, 5, 0x44);
            em2cAtkCk(em, 5, 0x45);
            em2cAtkCk(em, 5, 0x46);
            em2cAtkCk(em, 5, 0x47);
            em2cAtkCk(em, 5, 0x48);
            em2cAtkCk(em, 5, 0x49);
            em2cAtkCk(em, 5, 0x4A);
            em2cAtkCk(em, 5, 0x4B);
            em2cAtkCk(em, 5, 0x4C);
            if (w->atkHit) {
                w->mode = 1;
            }
        }
        break;
    }
}

static void plem2c_CriticalHit(cPlayer* pl)
{
    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 10);
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, &pl->mot, PL_ARC_PTR(pG->pPlArc, 0x4C), (int) PL_ARC_PTR(pG->pPlArc, 0x4D), 5, 1, 0);
        em2cPlHeadLost();
        pl->xFE++;
    case 1:
        MotionMoveF(pl, 0);
        break;
    }
}

static void em2c_R1_SwayBack(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int end;

    switch (em->xFE) {
    case 0:
        switch (Rnd() % 3) {
        case 0:
        default:
            MotionSetCore(em, &em->mot, ARC(0x7D), (int) ARC(0x7E), 4, 1, 0);
            break;
        case 1:
            MotionSetCore(em, &em->mot, ARC(0x7F), (int) ARC(0x80), 4, 1, 0);
            break;
        case 2:
            MotionSetCore(em, &em->mot, ARC(0x7F), (int) ARC(0x80), 4, 0x41, 0);
            break;
        }
        EstSet((int) em, -1, 0, 0, 0x24, 0xB, 0, w->espKind2, (u32) em, 0);
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        SndCall(8, 0x40, &em->pos, em->id, 0, em);
        em->dmg.x1 = 3;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 1) {
            w->flags |= 0x80000;
        } else {
            w->flags |= 0x40000;
        }
        end = MotionMoveF(em, 0);
        if (end) {
            em2cNextWalkSet2(em, w);
            break;
        }
        if ((em->seFlags28B & 4) && (Rnd() & 1)) {
            if (w->flags & 1) {
                f32 plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f));
                f32 lim = 12250000.0f;
                if (plAng > 1.57079637f) {
                    lim = 4000000.0f;
                }
                if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
                    if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                        EmRoutineSet(em, 1, 9, end, end);
                        break;
                    }
                    switch (Rnd() % 3) {
                    default:
                        EmRoutineSet(em, 1, 0xB, 0, 0);
                        break;
                    case 1:
                        EmRoutineSet(em, 1, 0xC, 0, 0);
                        break;
                    case 2:
                        EmRoutineSet(em, 1, 0xA, 0, 0);
                        break;
                    }
                    break;
                }
            }
            if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    }
}

static void em2c_R1_WakeupWait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int r;

    w->flags |= 0x400;
    switch (em->xFE) {
    case 0:
        w->x56C = Rnd() % 30 + 30;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if ((w->flags & 0x800) && w->guardCnt == 0) {
            w->x56C = w->guardCnt;
        }
        if (w->x56C) {
            w->x56C--;
            break;
        }
        r = em2cDownJumpCk(em);
        if (r) {
            break;
        }
        EmRoutineSet(em, 1, 0xF, r, r);
        break;
    }
}

static void em2c_R1_Wakeup(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int r;

    w->flags |= 0x8100;
    switch (em->xFE) {
    case 0:
        em->xFF = 0;
        if (w->guardCnt <= 0 || !(w->flags & 0x800)) {
            MotionSetCore(em, &em->mot, ARC(0x77), (int) ARC(0x78), 5, 1, 0);
            em->xFF = 1;
        } else {
            MotionSetCore(em, &em->mot, ARC(0x42), (int) ARC(0x43), 5, 1, 0);
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->dmgTotal = 0;
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if (w->flags & 0x800) {
                EmRoutineSet(em, 1, 0x18, 0, 0);
            } else {
                r = em2cLockCk(em);
                if (r) {
                    EmRoutineSet(em, 1, 5, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 1, r, r);
                }
            }
            break;
        }
        if (em->xFF && (w->flags & 0x800)) {
            w->flags |= 0x8000;
            if (em->seFlags28B & 1) {
                w->flags &= ~0x800;
                SndCall(8, 0x35, &em->pos, em->id, 0, em);
                SndCall(8, 0x36, &em->pos, em->id, 0, em);
                if (w->pTex) {
                    em->pInfo->setTexBlendTbl(w->texBlend);
                    em->pInfo->setBlendRatio(0);
                    em->pInfo->setBlendType(2);
                }
            }
        }
        if ((em->seFlags28B & 4) && w->guardCnt == 0) {
            if (em->seFlags28B & 0x80) {
                EmRoutineSet(em, 1, 0xF, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x1A, 0, 0);
            }
        }
        break;
    }
}

static void em2c_R1_DownJump(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Mtx inv;
    Vec a;
    Vec b;
    Vec hit;
    Vec plPos;
    Mtx m;

    w->flags |= 0x100;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x6F), (int) ARC(0x70), 5, 1, 0);
        w->timer = 10;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->flags |= 0x40;
            em->dmType = 2;
        }
        MotionMoveF(em, 0);
        a = em->oldPos;
        b = em->pos;
        a.y += 1000.0f;
        b.y += 1000.0f;
        if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0x383830) == 0) {
            break;
        }
        PSMTXRotRad(m, 'x', 3.14159274f);
        em->pos.y = hit.y;
        TransMatrix(em->mat, &em->pos);
        PSMTXConcat(em->mat, m, em->mat);
        em->xFE++;
    case 2:
        em2cSetdLandingEff(em);
        w->wallNrm.x = 0.0f;
        w->wallNrm.y = -1.0f;
        w->wallNrm.z = 0.0f;
        MotionSetCore(em, &em->mot, ARC(0x7B), 0, 0, 5, 0);
        em->xFE++;
    case 3:
        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &plPos);
        w->flags |= 0x120;
        em->setStatus(3);
        em2cSetWallMatrix2(em, 1.0f);
        if (MotionMoveF(em, 0)) {
            w->wallNrm.x = 0.0f;
            w->wallNrm.y = -1.0f;
            w->wallNrm.z = 0.0f;
            if (plPos.z < -500.0f) {
                EmRoutineSet(em, 1, 0x1F, 0, 0);
            }
            EmRoutineSet(em, 1, 0x1D, 0, 0);
        }
        break;
    }
}

static void em2c_R1_ToCeiling(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Mtx inv;
    Vec a;
    Vec b;
    Vec hit;
    Vec plPos;
    Mtx m;
    int fe;

    w->flags |= 0x100;
    fe = em->xFE;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x79), (int) ARC(0x7A), 5, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x24, 0x2C, 0, 0, (u32) em, (void*) fe);
        em->xFE++;
    case 1:
        if (em->seFlags28B & 1) {
            w->flags |= 0x40;
            em->dmType = 2;
        }
        MotionMoveF(em, 0);
        a = em->oldPos;
        b = em->pos;
        a.y += 1000.0f;
        b.y += 1000.0f;
        if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0x383830) == 0) {
            break;
        }
        PSMTXRotRad(m, 'z', 3.14159274f);
        em->pos.y = hit.y;
        TransMatrix(em->mat, &em->pos);
        PSMTXConcat(em->mat, m, em->mat);
        em->xFE++;
    case 2:
        em2cSetdLandingEff(em);
        w->wallNrm.x = 0.0f;
        w->wallNrm.y = -1.0f;
        w->wallNrm.z = 0.0f;
        MotionSetCore(em, &em->mot, ARC(0x64), 0, 0, 5, 0);
        em->xFE++;
    case 3:
        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &plPos);
        w->flags |= 0x120;
        em->setStatus(3);
        em2cSetWallMatrix2(em, 1.0f);
        if (MotionMoveF(em, 0)) {
            w->wallNrm.x = 0.0f;
            w->wallNrm.y = -1.0f;
            w->wallNrm.z = 0.0f;
            if (plPos.z < -500.0f) {
                EmRoutineSet(em, 1, 0x1F, 0, 0);
            }
            EmRoutineSet(em, 1, 0x1D, 0, 0);
        }
        break;
    }
}

static void em2c_R1_ToHide(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Mtx inv;
    Vec a;
    Vec b;
    Vec hit;
    Vec plPos;
    Mtx m;
    int fe;

    w->flags |= 0x8100;
    fe = em->xFE;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x79), (int) ARC(0x7A), 5, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x24, 0x2C, 0, 0, (u32) em, (void*) fe);
        em->xFE++;
    case 1:
        if (em->seFlags28B & 1) {
            w->flags |= 0x40;
            em->dmType = 2;
        }
        MotionMoveF(em, 0);
        a = em->oldPos;
        b = em->pos;
        a.y += 1000.0f;
        b.y += 1000.0f;
        if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0x383830) == 0) {
            break;
        }
        PSMTXRotRad(m, 'z', 3.14159274f);
        em->pos.y = hit.y;
        TransMatrix(em->mat, &em->pos);
        PSMTXConcat(em->mat, m, em->mat);
        em->xFE++;
    case 2:
        em2cSetdLandingEff(em);
        w->wallNrm.x = 0.0f;
        w->wallNrm.y = -1.0f;
        w->wallNrm.z = 0.0f;
        MotionSetCore(em, &em->mot, ARC(0x64), 0, 0, 5, 0);
        em->xFE++;
    case 3:
        em->alpha -= 0.100000001f;
        em->dmType = 2;
        if (em->alpha <= 0.0f) {
            em->alpha = 0.0f;
            em->be_flag &= ~2;
        }
        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &plPos);
        w->flags |= 0x120;
        em->setStatus(3);
        em2cSetWallMatrix2(em, 1.0f);
        if (MotionMoveF(em, 0)) {
            EffectEspDelete(0, w->espKind, (u32) em, 0);
            EffectEspgenDelete(0, w->espKind, (int) em);
            EffectEfmDelete(0, w->espKind, (int) em);
            EmRoutineSet(em, 1, 0x13, 0, 0);
        }
        break;
    }
}

static void em2c_R1_HideWait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u8 fe;
    int r;
    int t;

    w->flags |= 0x8000;
    w->flags |= 0x220000;
    em2cGetTail(em);
    fe = em->xFE;
    switch (fe) {
    case 0:
        em->xFE++;
        w->mode = 3;
        w->x6B8 = fe;
        AtariOff(&em->atari, 0xFCFF);
        em->alpha = 1.0f;
        em->be_flag |= 2;
        w->timer = Rnd() % 60 + 60;
        w->timer8 = fe;
    case 1:
        GetPlPos(&em->pos, 0, 10.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->rot.y = em->rot.y + (pPLS->rot.y + 3.14159274f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionSetCore(em, &em->mot, ARC(0x8A), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        r = em2cDoorCk(em);
        if (r) {
            w->timer = Rnd() % 6 * 30 + 150;
            w->timer8++;
        } else {
            w->timer8 = r;
        }
        t = w->timer;
        if (t == 0) {
            if (w->pTail && Rnd() % 10 > 4) {
                EmRoutineSet(em, 1, 0x14, t, t);
                break;
            }
        } else {
            w->timer = t - 1;
            asm volatile("");  // COMPILER-DIFF: #15 candidate: the original keeps the same-base `lwz timer8` below the `stw timer` (r0 for both); ours hoists it (a dead do-while keeps the order but gives the load r9)
            if (w->timer8 <= 90) {
                break;
            }
        }
        EmRoutineSet(em, 1, 0x15, 0, 0);
        break;
    }
}

static void em2c_R1_HideAtk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u8 fe;
    int t;

    w->flags |= 0x8000;
    w->flags |= 0x120000;
    em2cGetTail(em);
    fe = em->xFE;
    switch (fe) {
    case 0:
        em->xFE++;
        w->x6B8 = fe;
        w->mode = Rnd() % 3 + 1;
        AtariOff(&em->atari, 0xFCFF);
        w->pTail->flags_3C8 &= ~4;
        EstSet((int) em, -1, 0, 0, 0x24, 1, 0, w->espKind, (u32) em, (void*) fe);
        EstSet((int) em, -1, 0, 0, 0x24, 6, 0, w->espKind, (u32) em, (void*) fe);
        w->timer = 24;
        w->timer8 = 24;
        em->xFF = fe;
    case 1:
        GetPlPos(&em->pos, 0, 10.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->rot.y = em->rot.y + (pPLS->rot.y + 3.14159274f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionSetCore(em, &em->mot, ARC(0x8A), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        if (w->pTail->flags_3C8 & 4) {
            em->xFF = 1;
        }
        t = w->timer;
        if (t) {
            w->timer = t - 1;
        } else if (em->xFF) {
            EmRoutineSet(em, 1, 0x15, t, t);
            break;
        } else if (w->mode) {
            w->mode--;
            w->timer = 30;
            w->timer8 = 15;
            if (w->x6B8) {
                GetPlPos(&w->pTail->pos, 0, 0.0f);
            } else {
                GetPlPos(&w->pTail->pos, 0, 10.0f);
            }
            w->pTail->pos.y = pPLS->pos.y + 4000.0f;
            w->pTail->rot.y += pPLS->rot.y + 3.14159274f;
            w->pTail->rot.y = LIMIT_ANGLE(em->rot.y);
            w->pTail->flags_3C8 |= 1;
            w->pTail->flags_3C8 &= ~4;
            w->x6B8 = 0;
        } else {
            em->xFE++;
            break;
        }
        if (w->timer < w->timer8) {
            if (w->x6B8 == 0) {
                if (em->xFF == 0) {
                    if (w->mode) {
                        ActBtn.set(0x25, 0xB, (int) em2cBackjumpAction2, (int) em, 2, 3, 0, em->xFF);
                    } else {
                        ActBtn.set(0x13, 0xB, (int) em2cSitAction, (int) em, 2, 4, 0, w->mode);
                    }
                }
            } else {
                w->timer = 0;
            }
        }
        if (w->timer % 3) {
            EstSet(0, -1, &em->pos, 0, 0x24, 0xC, 0, 0, 0, 0);
        }
        break;
    case 2:
        if (w->x6B8 == 0) {
            GetPlPos(&em->pos, 0, 10.0f);
            em->pos.y = pPLS->pos.y + 4000.0f;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 3.14159274f) + 3.14159274f;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        MotionSetCore(em, &em->mot, ARC(0x8A), (int) ARC(0x8B), 0, 5, 0);
        EstSet((int) em, -1, 0, 0, 0x24, 0x21, 0, 0, (u32) em, 0);
        w->timer = 10;
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x15, 0, 0);
            break;
        }
        if (w->timer) {
            w->timer--;
            if (w->x6B8 == 0 && em->xFF == 0) {
                ActBtn.set(0x13, 0xB, (int) em2cSitAction, (int) em, 2, 4, 0, em->xFF);
            }
        }
        if (em->seFlags28B & 1) {
            em2cAtkCk(em, 4, 0xB);
            em2cAtkCk(em, 4, 0xC);
            em2cAtkCk(em, 4, 0xD);
        }
        break;
    }
}

static void em2c_R1_HideFall(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec v;
    f32 fl;
    int end;

    w->flags |= 0x8000;
    w->flags |= 0x500000;
    switch (em->xFE) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em2cGetFallPos(em);
        em->alpha = 0.0f;
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        w->timer = 30;
        em->xFE++;
    case 1:
        em->dmType = 2;
        MotionSetCore(em, &em->mot, ARC(0x89), 0, 0, 1, 0);
        MotionMoveF(em, 0);
        em->dmType = 2;
        if (w->timer) {
            w->timer--;
            break;
        }
        em->xFE++;
        break;
    case 2:
        AtariOff(&em->atari, 0xFCFF);
        EstSet((int) em, -1, 0, 0, 0x24, 1, 0, w->espKind, (u32) em, 0);
        EstSet((int) em, -1, 0, 0, 0x24, 6, 0, w->espKind, (u32) em, 0);
        MotionSetCore(em, &em->mot, ARC(0x89), 0, 5, 1, 0);
        em->xFE++;
    case 3:
        w->flags |= 0x1000;
        em->dmType = 2;
        em->alpha += 0.100000001f;
        if (em->alpha > 1.0f) {
            em->alpha = 1.0f;
        }
        em->dmType = 2;
        end = MotionMoveF(em, 0);
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 1.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            em->rot.y = atan2f(v.x, v.z);
            em->alpha = 1.0f;
            MotionSetCore(em, &em->mot, ARC(0x7B), 0, 5, 1, 0);
            MotionMoveF(em, 0);
            em->xFE = 6;
        } else if (end) {
            AtariOn(&em->atari, 0x300);
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, &em->mot, ARC(0x63), 0, 5, 5, 0);
        w->spd.x = 0.0f;
        w->spd.y = -200.0f;
        w->spd.z = 0.0f;
        em->xFE++;
    case 5:
        w->flags |= 0x1000;
        em->dmType = 2;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 1.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            em->rot.y = atan2f(v.x, v.z);
            w->wallNrm.x = 0.0f;
            w->wallNrm.y = 1.0f;
            w->wallNrm.z = 0.0f;
            MotionSetCore(em, &em->mot, ARC(0x7B), 0, 5, 1, 0);
            MotionMoveF(em, 0);
            em->xFE = 6;
        } else {
            MotionMoveF(em, 0);
        }
        break;
    case 6:
        AtariOn(&em->atari, 0x300);
        em2cSetdLandingEff(em);
        w->wallNrm.x = 0.0f;
        w->wallNrm.y = 1.0f;
        w->wallNrm.z = 0.0f;
        em->xFE++;
    case 7:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if ((w->flags & 1) && (Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

static void em2c_R1_JumpDown(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec spd;
    Vec rot;
    f32 fl;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x57), (int) ARC(0x59), 5, 0, 0);
        em->xFE++;
    case 1:
        em->rot.y += Muku2(em->rot.y, w->jumpAng, 0.196349546f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionGetSpeed(em, &em->mot, 0, &spd, &rot);
        if (em->seFlags28B & 1) {
            w->flags |= 0x40;
            em->dmType = 2;
            spd.x *= 0.5f;
            spd.z *= 0.5f;
        }
        MotionAddSpeed(em, &em->mot, &spd, &rot);
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->mot, ARC(0x57), (int) ARC(0x5A), 5, 4, 0);
        w->spd.x = 0.0f;
        w->spd.y = -450.0f;
        w->spd.z = 30.0f;
        em->xFE++;
    case 3:
        w->flags |= 0x40;
        em->dmType = 2;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            em->xFE++;
        }
        MotionMoveF(em, 0);
        break;
    case 4:
        MotionSetCore(em, &em->mot, ARC(0x57), (int) ARC(0x5B), 5, 1, 0);
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if (w->flags & 0x800) {
                EmRoutineSet(em, 1, 0x18, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

static void em2c_R1_WallOver(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec d;
    int t;

    w->flags |= 0x100;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x29), (int) ARC(0x2A), 10, 5, 0);
        em->xFE++;
    case 1:
        em->rot.y += Muku2(em->rot.y, w->jumpAng, 0.392699093f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->mot, ARC(0x29), (int) ARC(0x2A), 10, 5, 0);
        w->timer = 30;
        em->xFE++;
    case 3:
        w->flags |= 0x20;
        em->setStatus(3);
        em2cSetWallMatrix2(em, 0.400000006f);
        MotionMoveF(em, 0);
        t = w->timer;
        if (t) {
            w->timer = t - 1;
        } else if (w->wallNrm.y > 0.699999988f) {
            d.x = 0.0f;
            d.y = 0.0f;
            d.z = 1.0f;
            PSMTXMultVecSR(em->mat, &d, &d);
            em->rot.y = atan2f(d.x, d.z);
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, t, t);
            } else if (w->flags & 0x800) {
                EmRoutineSet(em, 1, 0x18, t, t);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Routine 1: frozen (F_*), wall (W_*)

static void em2c_R1_F_Wait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int wait;

    w->flags |= 0x180;
    switch (em->xFE) {
    case 0:
        if (em->motFlags & 0x40) {
            MotionSetCore(em, &em->mot, ARC(0x2B), 0, 30, 0x45, 0);
        } else {
            MotionSetCore(em, &em->mot, ARC(0x2B), 0, 30, 5, 0);
        }
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if ((s16) pG->pl_life <= 0) {
            break;
        }
        wait = w->atkWait;
        if (wait) {
            break;
        }
        if (w->targetAngAbs > 2.09439516f) {
            EmRoutineSet(em, 1, 3, wait, wait);
        } else {
            EmRoutineSet(em, 1, 0x18, wait, wait);
        }
        break;
    }
    if (em2cFallCk(em)) {
        return;
    }
    em2cBreathSe(em);
}

static void em2c_R1_F_Walk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    f32 ang;
    int seq;
    int wait;
    int r;

    w->flags |= 0x100;
    switch (em->xFE) {
        do { } while (0);  // dead loop before the label: the arm does not know xFE == 0 (fresh `li 0` for the zero stores)
    case 0:
        if (em->motFlags & 0x40) {
            w->blendSeq = 0xB;
        } else {
            w->blendSeq = 0x22;
        }
        w->blendM0 = ARC(0x2C);
        w->blendM1 = ARC(0x33);
        w->blendM2 = ARC(0x32);
        w->blendA = (int) ARC(0x2D);
        w->blendB = 0;
        w->blendD = 5;
        w->blendC = 0;
        w->blendCnt = 5;
        w->blendVal = 0.0f;
        em->xFE++;
    case 1:
        ang = Muku(&em->pos, &w->targetPos, em->rot.y, 3.14159274f);
        if (ang > 1.04719758f) {
            ang = 1.04719758f;
        }
        if (ang < -1.04719758f) {
            ang = -1.04719758f;
        }
        {
            f32 blend = ang * 243.50705f; // separate statement: pool order 243.50705, 0.8, 0.2
            w->blendVal = w->blendVal * 0.800000012f + blend * 0.200000003f;
        }
        em->rot.y += w->blendVal * 0.00392156886f * 0.0490873866f;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em2cBlendMotSetI(em, w->blendM0, w->blendM1, w->blendM2, w->blendA, 0, 0, w->blendD);
        MotionMoveF(em, 0);
        break;
    }
    em2cDoorOpenCk(em);
    if (em2cJumpDownCk(em)) {
        return;
    }
    r = em2cWallOverCk(em);
    if (r) {
        return;
    }
    wait = w->atkWait;
    if (wait) {
        EmRoutineSet(em, 1, 0, r, r);
        return;
    }
    if ((w->flags & 1) && em->plDist2 < 4000000.0f && w->routeAngAbs < 0.52359879f) {
        EmRoutineSet(em, 1, 0x19, wait, wait);
        return;
    }
    if (em2cFallCk(em)) {
        return;
    }
    {
        int g = w->guardCnt;  // its own variable: reusing `r` would make it cse's canonical zero for the 0x19 stores

        if (g == 0) {
            EmRoutineSet(em, 1, 0x1A, g, g);
            return;
        }
    }
    em2cBreathSe(em);
}

static void em2c_R1_F_Atk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero;
    int r;

    w->flags |= 0x80;
    w->flags |= 0x10000;
    switch (em->xFE) {
    case 0:
        w->mode = Rnd() & 1;
        if (w->mode) {
            MotionSetCore(em, &em->mot, ARC(0x38), (int) ARC(0x39), 10, 1, 0);
        } else {
            MotionSetCore(em, &em->mot, ARC(0x36), (int) ARC(0x37), 10, 1, 0);
        }
        r = Rnd() & 1;
        if (r) {
            w->dmgTotal = 1000;
        } else {
            w->dmgTotal = r;
        }
        zero = 0;
        EstSet((int) em, -1, 0, 0, 0x24, 2, 0, 0, (u32) em, (void*) zero);
        w->atkCnt = zero;
        w->timer = 15;
        w->atkHit = zero;
        em->flags_3C8 &= ~4;
        em->xFE++;
    case 1:
        if (em->xFF == 0 && w->timer) {
            w->timer--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.157079637f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            w->atkWait = 30;
            w->dmgTotal = 0;
            EmRoutineSet(em, 1, 0x17, 0, 0);
        } else if (em->seFlags28B & 1) {
            em2cDoorOpenCk2(em);
            if (w->mode) {
                em2cAtkCk(em, 6, 7);
                em2cAtkCk(em, 6, 8);
                em2cAtkCk(em, 6, 9);
            } else {
                em2cAtkCk(em, 6, 0xB);
                em2cAtkCk(em, 6, 0xC);
                em2cAtkCk(em, 6, 0xD);
            }
        }
        break;
    }
}

static void em2c_R1_F_Clear(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int fe;

    w->flags |= 0x8080;
    fe = em->xFE;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x30), (int) ARC(0x31), 5, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x24, 0x26, 0, 0, (u32) em, (void*) fe);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->dmgTotal = 0;
            w->atkWait = 30;
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if (em->plDist2 > 25000000.0f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else if (em->seFlags28B & 1) {
            w->flags &= ~0x800;
            if (w->pTex) {
                em->pInfo->setTexBlendTbl(w->texBlend);
                em->pInfo->setBlendRatio(0);
                em->pInfo->setBlendType(2);
            }
        }
        break;
    }
}

static void em2c_R1_W_Wait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Mtx inv;
    Vec plPos;

    w->flags |= 0x120;
    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x60), 0, 30, 5, 0);
        em->xFE++;
    case 1:
        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &plPos);
        em2cSetWallMatrix2(em, 0.400000006f);
        MotionMoveF(em, 0);
        if (w->atkWait == 0) {
            EmRoutineSet(em, 1, 0x1D, 0, 0);
        } else if (plPos.z < -8000.0f) {
            EmRoutineSet(em, 1, 0x1F, 0, 0);
        }
        break;
    }
    em2cBreathSe(em);
}

static void em2c_R1_W_Walk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Mtx m;
    Mtx inv;
    Vec plPos;
    Vec d;
    f32 ang;
    f32 ny;
    int t;
    int r;

    w->flags |= 0x120;
    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x29), (int) ARC(0x2A), 10, 5, 0);
        w->mode = Rnd() & 3;
        if (em->xFF) {
            w->turnAng = em2cGetPlDir(em, &w->wallTarget);
            w->timer = Rnd() % 60 + 60;
        } else {
            w->timer = 0;
            w->turnAng = w->plDir;
        }
        w->timer8 = (Rnd() & 3) + 8;
        em->xFE++;
    case 1:
        PSMTXInverse(em->mat, inv);
        plPos = pPL->pos;
        plPos.y += 1800.0f;
        PSMTXMultVec(inv, &plPos, &plPos);
        if (w->timer) {
            w->timer--;
            ang = Muku2(0.0f, w->turnAng, 0.0981747732f);
            PSMTXRotRad(m, 'y', ang);
            PSMTXConcat(em->mat, m, em->mat);
            w->turnAng -= ang;
        } else {
            ang = Muku2(0.0f, w->plDir, 0.0981747732f);
            PSMTXRotRad(m, 'y', ang);
            PSMTXConcat(em->mat, m, em->mat);
            w->plDir -= ang;
        }
        em2cSetWallMatrix2(em, 0.400000006f);
        if (MotionMoveF(em, 0)) {
            t = w->timer8;
            if (t) {
                w->timer8 = t - 1;
            } else if (w->wallNrm.y > 0.899999976f || w->wallNrm.y < -0.899999976f) {
                w->atkWait = Rnd() % 30 + 30;
                EmRoutineSet(em, 1, 0x1C, t, t);
                break;
            }
        }
        if (em2cNoWallCk(em) && w->wallNrm.y < 0.699999988f) {
            EmRoutineSet(em, 1, 0x20, 0, 0);
            break;
        }
        ny = w->wallNrm.y;
        if ((w->flags & 1) && plPos.x > -300.0f && plPos.x < 300.0f && plPos.y > 0.0f && plPos.y < 2200.0f &&
            plPos.z > 0.0f && plPos.z < 2000.0f) {
            if (ny > 0.899999976f) {
                d.x = 0.0f;
                d.y = 0.0f;
                d.z = 1.0f;
                PSMTXMultVecSR(em->mat, &d, &d);
                em->rot.y = atan2f(d.x, d.z);
                EmRoutineSet(em, 1, 9, 0, 0);
                break;
            }
            if (ny < -0.899999976f || (ny > -0.100000001f && ny < 0.100000001f)) {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
                break;
            }
        }
        if (ny < -0.899999976f && em->plDist2 < 250000.0f) {
            if (Rnd() % 10 > 2) {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x20, 0, 0);
            }
            break;
        }
        t = w->flags & 0x2000;
        if (t) {
            EmRoutineSet(em, 1, 0x20, 0, 0);
            break;
        }
        if (plPos.z < -8000.0f) {
            EmRoutineSet(em, 1, 0x1F, t, t);
        }
        r = em2cWallFallCk(em);
        if (r) {
            EmRoutineSet(em, 1, 0x20, t, t);
            break;
        }
        if (w->wallNrm.y > 0.899999976f && em->plDist2 < 1440000.0f && fabsf(em->mat[1][3] - pPL->pos.y) < 500.0f &&
            w->routeAngAbs > 1.04719758f && w->routeAngAbs < 1.91986215f) {
            d.x = 0.0f;
            d.y = 0.0f;
            d.z = 1.0f;
            PSMTXMultVecSR(em->mat, &d, &d);
            em->rot.y = atan2f(d.x, d.z);
            EmRoutineSet(em, 1, 6, r, r);
        }
        break;
    }
    w->atkCnt++;
    em2cBreathSe(em);
}

static void em2c_R1_W_Atk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec d;
    int zero;
    int fe;

    w->flags |= 0x20;
    em->setStatus(3);
    fe = em->xFE;
    switch (fe) {
    case 0:
        d.x = 0.0f;
        d.y = 1.0f;
        d.z = 0.0f;
        PSMTXMultVecSR(em->mat, &d, &d);
        if (d.y > -0.600000024f) {
            em->xFF = 1;
            MotionSetCore(em, &em->mot, ARC(0x81), (int) ARC(0x82), 10, 1, 0);
        } else {
            em->xFF = fe;
            MotionSetCore(em, &em->mot, ARC(0x49), (int) ARC(0x4A), 10, 1, 0);
        }
        zero = 0;
        w->timer = 10;
        w->atkCnt = zero;
        w->atkHit = zero;
        em->flags_3C8 &= ~4;
        em->xFE++;
    case 1:
        em2cSetWallMatrix2(em, 0.400000006f);
        if (MotionMoveF(em, 0)) {
            if (Rnd() % 10 > 4) {
                EmRoutineSet(em, 1, 0x20, 0, 0);
            } else if (w->atkHit) {
                em2cSetAtkWait(w, 60, 45, 40, 35, 30);
                EmRoutineSet(em, 1, 0x1C, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x1D, w->atkHit, w->atkHit);
            }
        } else if (em->seFlags28B & 1) {
            if (em->xFF) {
                em2cAtkCk(em, 3, 0xB);
                em2cAtkCk(em, 3, 0xC);
                em2cAtkCk(em, 3, 0xD);
            } else {
                em2cAtkCk(em, 3, 7);
                em2cAtkCk(em, 3, 8);
                em2cAtkCk(em, 3, 9);
            }
        }
        break;
    }
}

static void em2c_R1_W_Turn180(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Mtx m;
    Vec spd;
    Vec rot;

    w->flags |= 0x120;
    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x47), (int) ARC(0x48), 5, 0, 0);
        em->xFE++;
    case 1:
        MotionGetSpeed(em, &em->mot, 0, &spd, &rot);
        RotMatrix(m, &rot);
        PSMTXConcat(em->mat, m, em->mat);
        MotionAddSpeed(em, &em->mot, &spd, &rot);
        em2cSetWallMatrix2(em, 0.400000006f);
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x1C, 0, 0);
        }
        break;
    }
    em2cBreathSe(em);
}

static void em2c_R1_W_Fall(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec v;
    f32 fl;

    w->flags |= 0x140;
    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x61), (int) ARC(0x62), 5, 1, 0);
        if (w->wallNrm.y > 0.899999976f) {
            w->spd.x = 0.0f;
            w->spd.y = 80.0f;
            w->spd.z = 80.0f;
            PSMTXMultVecSR(em->mat, &w->spd, &w->spd);
        } else {
            PSVECScale(&w->wallNrm, &w->spd, 150.0f);
        }
        w->timer = 6;
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        em->xFE++;
    case 1:
        w->flags |= 0x1000;
        em->dmType = 2;
        if (w->timer) {
            w->timer--;
            em2cSetWallMatrix2(em, 0.400000006f);
            MotionMoveF(em, 0);
            if (w->timer == 0) {
                em->xFE++;
            }
            break;
        }
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 1.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            em->rot.y = atan2f(v.x, v.z);
            MotionSetCore(em, &em->mot, ARC(0x7B), 0, 5, 1, 0);
            MotionMoveF(em, 0);
            em->xFE = 6;
            break;
        }
        em2cSetFallMatrix(em);
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->mot, ARC(0x65), 0, 5, 5, 0);
        em->xFE++;
    case 3:
        w->flags |= 0x1000;
        em->dmType = 2;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 1.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            em->rot.y = atan2f(v.x, v.z);
            MotionSetCore(em, &em->mot, ARC(0x7B), 0, 5, 1, 0);
            MotionMoveF(em, 0);
            em->xFE = 6;
            break;
        }
        em2cSetFallMatrix(em);
        MotionMoveF(em, 0);
        break;
    case 4:
        MotionSetCore(em, &em->mot, ARC(0x63), 0, 5, 5, 0);
        em->xFE++;
    case 5:
        w->flags |= 0x1000;
        em->dmType = 2;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 1.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            em->rot.y = atan2f(v.x, v.z);
            MotionSetCore(em, &em->mot, ARC(0x7B), 0, 5, 1, 0);
            MotionMoveF(em, 0);
            em->xFE = 6;
            break;
        }
        em2cSetFallMatrix(em);
        MotionMoveF(em, 0);
        break;
    case 6:
        em2cSetdLandingEff(em);
        w->wallNrm.x = 0.0f;
        w->wallNrm.y = 1.0f;
        w->wallNrm.z = 0.0f;
        em->xFE++;
    case 7:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Routine 1: ceiling (C_*), reset, tail (T_*)

static void em2c_R1_C_Wait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u8 fe;
    int t;

    w->flags |= 0x8000;
    w->flags |= 0x120000;
    em2cGetTail(em);
    fe = em->xFE;
    switch (fe) {
    case 0:
        em->xFE++;
        w->x6B8 = fe;
        if (em->xFF) {
            *(volatile int*) &w->timer = Rnd() % 150 + 100;  // volatile store: the pG load stays below it (a reference store folds the address to em+0x3E4)
            if (pG->x4F88 <= 3) {
                *(volatile int*) &w->timer = Rnd() % 150 + 200;
            }
            if (pG->x4F88 <= 1) {
                w->timer = Rnd() % 150 + 300;
            }
        } else {
            w->timer = Rnd() % 60 + 60;
        }
        em->xFF = 1;
    case 1:
        GetPlPos(&em->pos, 0, 10.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->rot.y = em->rot.y + (pPLS->rot.y + 3.14159274f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionSetCore(em, &em->mot, ARC(0x8A), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 2) {
            break;
        }
        if (em2cDoorCk(em)) {
            w->timer = Rnd() % 60 + 60;
        }
        t = w->timer;
        if (t > 44) {
            if (pPL->xFC == 0 && (u32) (pPL->xFD - 1) <= 2 && t) {
                w->timer = t - 1;
            }
            break;
        }
        if (t) {
            w->timer = t - 1;
            break;
        }
        if (Rnd() % 10 > 3 && w->pTail) {
            w->pTail->flags_3C8 |= 1;
            em->xFE = 6;
        } else {
            em->xFE++;
        }
        break;
    case 2:
        GetPlPos(&em->pos, 0, 10.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->rot.y = em->rot.y + (pPLS->rot.y + 3.14159274f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionSetCore(em, &em->mot, ARC(0x8A), 0, 0, 5, 0);
        w->timer = 70;
        w->x6B8 = 0;
        em->xFE++;
    case 3:
        if (w->x6B8 == 0) {
            GetPlPos(&em->pos, 0, 10.0f);
            em->pos.y += 4000.0f;
            em->rot.y = em->rot.y + (pPLS->rot.y + 3.14159274f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        MotionSetCore(em, &em->mot, ARC(0x8A), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        if (w->timer % 3) {
            EstSet(0, -1, &em->pos, 0, 0x24, 0xC, 0, 0, 0, 0);
        }
        if (w->timer > 17) {
            if (em2cDoorCk(em)) {
                em->xFF = 0;
                em->xFE = 0;
                break;
            }
        } else if (w->x6B8 == 0) {
            ActBtn.set(0x13, 0xB, (int) em2cSitAction, (int) em, 1, 4, 0, w->x6B8);
        }
        if (w->timer) {
            w->timer--;
        } else {
            em->xFE++;
        }
        break;
    case 4:
        if (w->x6B8 == 0) {
            GetPlPos(&em->pos, 0, 10.0f);
            em->pos.y = pPLS->pos.y + 4000.0f;
            em->rot.y = em->rot.y + (pPLS->rot.y + 3.14159274f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        MotionSetCore(em, &em->mot, ARC(0x8A), (int) ARC(0x8B), 0, 5, 0);
        w->timer = 10;
        w->atkHit = 0;
        em->flags_3C8 &= ~4;
        pGS->flags_174 |= 0x40000000;  // struct view: the pG load stays below the flags_3C8 store
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
            break;
        }
        if (w->timer) {
            w->timer--;
            if (w->x6B8 == 0) {
                ActBtn.set(0x25, 0xB, (int) em2cBackjumpAction, (int) em, 1, 4, 0, w->x6B8);
            }
        }
        if (em->seFlags28B & 1) {
            em2cAtkCk(em, 4, 0xB);
            em2cAtkCk(em, 4, 0xC);
            em2cAtkCk(em, 4, 0xD);
        }
        break;
    case 6:
        em->xFE++;
        w->x6B8 = 0;
        w->timer = 120;
    case 7:
        GetPlPos(&em->pos, 0, 10.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->rot.y = em->rot.y + (pPLS->rot.y + 3.14159274f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionSetCore(em, &em->mot, ARC(0x8A), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        {
            int t7 = w->timer;  // its own variable: with case 1's `t` the three `timer = t - 1` copies are cross-jumped into this one

            if (t7) {
                w->timer = t7 - 1;
            } else {
                em->xFE = t7;
            }
        }
        break;
    }
    if (em->flags_3C8 & 1) {
        EmRoutineSet(em, 1, 0x22, 0, 0);
    } else {
        em2cBreathSe(em);
    }
}

static void em2c_R1_C_Fall(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int end;

    w->flags |= 0x8000;
    switch (em->xFE) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em->pos.x = -12725.0f;
        em->pos.y = 886.0f;
        em->pos.z = -80973.0f;
        em->rot.y = -0.589999974f;
        w->jumpAng = -0.589999974f;
        em->alpha = 1.0f;
        em->be_flag |= 2;
        MotionSetCore(em, &em->mot, ARC(0x94), (int) ARC(0x95), 5, 1, 0);
        em->xFE++;
    case 1:
        end = MotionMoveF(em, 0);
        if (end) {
            em->flags_3C8 |= 8;
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if ((w->flags & 1) && (Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
            break;
        }
        if (em->frame > 69.6999969f && em->frame < 70.3000031f) {
            EstSet((int) em, -1, 0, 0, 0x24, 7, 0, 0, end, (void*) end);
        }
        if (em->seFlags28B & 1) {
            em2cSetdLandingEff(em);
            AtariOn(&em->atari, 0x300);
        }
        break;
    }
}

static void em2c_R1_Reset_Wait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec v;
    f32 fl;
    int fe;
    int end;

    w->flags |= 0x8000;
    w->flags |= 0x100000;
    fe = em->xFE;
    switch (fe) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em->pos.y = 4330.0f;
        MotionSetCore(em, &em->mot, ARC(0x89), 0, 5, 1, 0);
        if (!(em->flags_3C8 & 1)) {
            MotionMoveF(em, 0);
            em->hp = fe;
            em->be_flag &= ~2;
            break;
        }
        em->be_flag |= 2;
        em->hp = em->hpMax;
        em->xFE++;
    case 1:
        w->flags |= 0x1000;
        em->dmType = 2;
        end = MotionMoveF(em, 0);
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 1.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            em->rot.y = atan2f(v.x, v.z);
            w->wallNrm.x = 0.0f;
            w->wallNrm.y = 1.0f;
            w->wallNrm.z = 0.0f;
            MotionSetCore(em, &em->mot, ARC(0x7B), 0, 5, 1, 0);
            MotionMoveF(em, 0);
            em->xFE = 4;
        } else if (end) {
            AtariOn(&em->atari, 0x300);
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->mot, ARC(0x63), 0, 5, 5, 0);
        w->spd.x = 0.0f;
        w->spd.y = -200.0f;
        w->spd.z = 0.0f;
        em->xFE++;
    case 3:
        w->flags |= 0x1000;
        em->dmType = 2;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 1.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            em->rot.y = atan2f(v.x, v.z);
            w->wallNrm.x = 0.0f;
            w->wallNrm.y = 1.0f;
            w->wallNrm.z = 0.0f;
            MotionSetCore(em, &em->mot, ARC(0x7B), 0, 5, 1, 0);
            MotionMoveF(em, 0);
            em->xFE = 4;
        } else {
            MotionMoveF(em, 0);
        }
        break;
    case 4:
        AtariOn(&em->atari, 0x300);
        em2cSetdLandingEff(em);
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if ((w->flags & 1) && (Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

static void em2c_R1_T_Wait(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u8 fe;
    int zero;
    u32 i;

    fe = em->xFE;
    switch (fe) {
    case 0:
        em->xFE++;
        w->x6B8 = fe;
    case 1:
        GetPlPos(&em->pos, 0, 10.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->rot.y = em->rot.y + (pPLS->rot.y + 3.14159274f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionSetCore(em, &em->mot, ARC(0x87), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 2) {
            EM_LIST(em->emsetNo)->x3 = 3;
            EmRoutineSet(em, 1, 0x25, 0, 0);
            break;
        }
        if (em->flags_3C8 & 1) {
            em->flags_3C8 &= ~1;
            em->xFE++;
        }
        break;
    case 2:
        GetPlPos(&em->pos, 0, 10.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->rot.y = em->rot.y + (pPLS->rot.y + 3.14159274f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionSetCore(em, &em->mot, ARC(0x87), 0, 0, 5, 0);
        if (Rnd() % 10 > 4 || em2cFloorTypeCk(em) == 0) {
            em->xFF = 0;
        } else {
            em->xFF = 1;
            SndCall(8, 0x49, &em->pos, em->id, 0, em);
        }
        w->timer = 70;
        w->x6B8 = 0;
        em->xFE++;
    case 3:
        if (em->xFF) {
            MotionSetCore(em, &em->mot, ARC(0x8C), 0, 0, 5, 0);
            if (w->x6B8 == 0) {
                GetPlPos(&em->pos, 0, 10.0f);
                em->pos.y = pPLS->pos.y - 500.0f;
                em->rot.y = em->rot.y + (pPLS->rot.y + 3.14159274f);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            }
            if (w->timer % 3) {
                EstSet(0, -1, &em->pos, 0, 0x24, 0x2D, 0, 0, 0, 0);
            }
        } else {
            MotionSetCore(em, &em->mot, ARC(0x87), 0, 0, 5, 0);
            GetPlPos(&em->pos, 0, 10.0f);
            em->pos.y += 4000.0f;
            em->rot.y = em->rot.y + (pPLS->rot.y + 3.14159274f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
            if (w->timer % 3) {
                EstSet(0, -1, &em->pos, 0, 0x24, 0xC, 0, 0, 0, 0);
            }
        }
        MotionMoveF(em, 0);
        if (w->timer > 17) {
            if ((em->flags_3C8 & 0x40000000) && em2cDoorCk(em)) {
                em->xFE = 0;
                break;
            }
        } else if (w->x6B8 == 0) {
            ActBtn.set(0x25, 0xB, (int) em2cBackjumpAction, (int) em, 1, 3, 0, w->x6B8);
        }
        if (em->xFF) {
            int r = em2cFloorTypeCk(em);
            if (r == 0) {
                em->xFE = r;
                break;
            }
        }
        if (w->timer) {
            w->timer--;
        } else {
            em->xFE++;
        }
        break;
    case 4:
        if (w->x6B8 == 0) {
            GetPlPos(&em->pos, 0, 10.0f);
            em->pos.y = pPLS->pos.y + 4000.0f;
            em->rot.y = em->rot.y + (pPLS->rot.y + 3.14159274f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->xFF) {
            em->pos.y = pPL->pos.y - 500.0f;
            MotionSetCore(em, &em->mot, ARC(0x8C), (int) ARC(0x8D), 0, 5, 0);
            zero = 0;
            EstSet((int) em, -1, 0, 0, 0x24, 0x1E, 0, 0, (u32) em, (void*) zero);
            EstSet((int) em, -1, 0, 0, 0x24, 0x20, 0, 0, (u32) em, (void*) zero);
            SndCall(8, 0x4A, &em->pos, em->id, 0, em);
            SndCall(8, 0x3A, &em->pos, em->id, 0, em);
        } else {
            em->pos.y = pPL->pos.y + 4000.0f;
            MotionSetCore(em, &em->mot, ARC(0x87), (int) ARC(0x88), 0, 5, 0);
            EstSet((int) em, -1, 0, 0, 0x24, 0x1B, 0, 0, (u32) em, 0);
            EstSet(0, -1, &em->pos, &em->rot, 0x24, 0x1F, 0, 0, 0, 0);
            SndCall(8, 0x3F, &em->pos, em->id, 0, em);
            SndCall(8, 0x3A, &em->pos, em->id, 0, em);
        }
        w->timer = 10;
        w->atkHit = 0;
        em->flags_3C8 &= ~4;
        em->flags_3C8 |= 0x40000000;
        pGS->flags_174 |= 0x40000000;
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
            break;
        }
        if (w->timer) {
            w->timer--;
            if (w->x6B8 == 0) {
                ActBtn.set(0x25, 0xB, (int) em2cBackjumpAction, (int) em, 1, 3, 0, w->x6B8);
            }
        }
        if (em->seFlags28B & 1) {
            for (i = 1; i <= 0x12; i++) {
                em2cAtkCk(em, 7, i);
            }
        }
        break;
    }
}

static void em2c_R1_T_Hide(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u8 fe;
    int zero;
    u32 i;

    fe = em->xFE;
    switch (fe) {
    case 0:
        em->xFE++;
        w->x6B8 = fe;
    case 1:
        GetPlPos(&em->pos, 0, 3.0f);
        em->pos.y = pPLS->pos.y + 4000.0f;
        em->rot.y = em->rot.y + (pPLS->rot.y + 3.14159274f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionSetCore(em, &em->mot, ARC(0x8E), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            em->flags_3C8 &= ~1;
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, &em->mot, ARC(0x8E), (int) ARC(0x8F), 0, 5, 0);
        zero = 0;
        EstSet((int) em, -1, 0, 0, 0x24, 0x1B, 0, 0, (u32) em, (void*) zero);
        EstSet(0, -1, &em->pos, &em->rot, 0x24, 0x1F, 0, 0, zero, (void*) zero);
        SndCall(8, 0x3F, &em->pos, em->id, 0, em);
        SndCall(8, 0x3A, &em->pos, em->id, 0, em);
        w->atkHit = zero;
        w->timer = 10;
        em->flags_3C8 &= ~4;
        em->flags_3C8 |= 0x40000000;
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
            break;
        }
        if (em->seFlags28B & 1) {
            for (i = 1; i <= 0x12; i++) {
                em2cAtkCk(em, 7, i);
            }
        }
        if (em->flags_3C8 & 1) {
            em->flags_3C8 &= ~1;
            em->xFE = 2;
        }
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Routine 2: damage

// Damage start sounds and effects: the breath stops, the hit voice, the hidden effects go.
static inline void em2cDmStartSet(cEm2c* em, Em2cWork* w)
{
    SndStop(w->breathSnd, 0);
    w->breathTimer = 2;
    SndCall(8, 0xF, &em->pos, em->id, 0, em);
    EffectEspDelete(0, w->espKind2, (u32) em, 0);
    EffectEspgenDelete(0, w->espKind2, (int) em);
    EffectEfmDelete(0, w->espKind2, (int) em);
}

// Damage end on the floor: the walk / turn / dash pick (with the ceiling climb chance).
static inline int em2cDmEndSet(cEm2c* em, Em2cWork* w, int zero)
{
    w->dmgTotal = zero;
    if (Rnd() % 3 == 0) {
        if (em2cToCeilingCk(em)) {
            return 1;
        }
    }
    if (w->targetAngAbs > 2.09439516f) {
        EmRoutineSet(em, 1, 3, zero, zero);
    } else if ((w->flags & 1) && (Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
        EmRoutineSet(em, 1, 2, zero, zero);
    } else {
        EmRoutineSet(em, 1, 1, 0, 0);
    }
    return 1;
}

static void em2c_R0_Damage(cEm2c* em)
{
    EM2C_WK(em)->flags |= 8;
    Em2c_R1_dm_tbl[em->xFD](em);
}

static void em2c_R1_Dm_Normal(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int end;

    w->flags |= 0x100;
    switch (em->xFE) {
    case 0:
        if (fabsf(Muku(&em->pos, &em->x328, em->rot.y, 3.14159274f)) < 1.57079637f) {
            switch (Rnd() % 3) {
            case 0:
            default:
                MotionSetCore(em, &em->mot, ARC(0x1D), (int) ARC(0x1E), 5, 1, 0);
                break;
            case 1:
                MotionSetCore(em, &em->mot, ARC(0x1F), (int) ARC(0x20), 5, 1, 0);
                break;
            case 2:
                MotionSetCore(em, &em->mot, ARC(0x21), (int) ARC(0x22), 5, 1, 0);
                break;
            }
        } else {
            MotionSetCore(em, &em->mot, ARC(0x23), (int) ARC(0x24), 5, 1, 0);
        }
        em2cDmStartSet(em, w);
        w->timer = 30;
        em->xFE++;
    case 1:
        end = MotionMoveF(em, 0);
        if (end) {
            end = 0;
            em2cDmEndSet(em, w, end);
            break;
        }
        if (em->seFlags28B & 4) {
            if (Rnd() & 1) {
                em2cDmEndSet(em, w, end);
                break;
            }
        }
        if (w->timer) {
            w->timer--;
            break;
        }
        if (em2cLockCk(em)) {
            int r = em2cToCeilingCk(em);  // block-local: `mr. r3,r3` (keeps this RS out of the cross-jumped RS tails)

            if (r == 0) {
                EmRoutineSet(em, 1, 5, r, r);
            }
        }
        break;
    }
}

static void em2c_R1_Dm_Down(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int r;

    w->flags |= 0x8000;
    switch (em->xFE) {
    case 0:
        if (w->flags & 0x4000) {
            MotionSetCore(em, &em->mot, ARC(0x92), 0, 5, 1, 0);
        } else {
            MotionSetCore(em, &em->mot, ARC(0x6B), (int) ARC(0x6C), 5, 1, 0);
        }
        em2cDmStartSet(em, w);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            r = em2cDownJumpCk(em);
            if (r == 0) {
                EmRoutineSet(em, 1, 0xF, r, r);
            }
        }
        break;
    }
}

// Damage fall step: the position follows `spd` with gravity; on the floor the landing motion.
// The fall step of the damage/die routines, a macro (not an inline: integrate.c drops the
// RTX_UNCHANGING_P flag of an inlined body's constant-pool loads, which then depend on the
// preceding byte store and sink below the int argument moves). The landing tail (`xFE = next`)
// and the fall arm live inside it, so the caller has no return-value diamond.
#define EM2C_DM_FALL(em, w, v_, DM_TYPE, DOWN, ATARI_ON, NEXT, FALL_MTX, END_INC)                \
    {                                                                                          \
        f32 fl_;                                                                               \
                                                                                               \
        PSVECAdd(&(em)->pos, &(w)->spd, &(em)->pos);                                           \
        (w)->spd.y -= 20.0f;                                                                   \
        if (DM_TYPE) {                                                                         \
            (em)->dmType = 2;                                                                  \
        }                                                                                      \
        fl_ = SatMgr.getFloor(&(em)->pos, 600.0f, 100000.0f, 0, 0);                             \
        PSVECAdd(&(em)->pos, &(w)->spd, &(em)->pos);                                           \
        if ((em)->pos.y < fl_) {                                                               \
            (em)->pos.y = fl_;                                                                 \
            v_.x = 0.0f; \
            v_.y = 0.0f; \
            v_.z = 1.0f; \
            PSMTXMultVecSR((em)->mat, &v_, &v_);                                               \
            (em)->rot.y = atan2f(v_.x, v_.z);                                                  \
            MotionSetCore(em, &(em)->mot, ARC(0x69), (int) ARC(0x6A), 5, 1, 0);               \
            MotionMoveF(em, 0);                                                                \
            if (DOWN) {                                                                        \
                em2cSetDownEff(em);                                                            \
            } else {                                                                           \
                em2cSetdLandingEff(em);                                                        \
            }                                                                                  \
            if (ATARI_ON) {                                                                    \
                AtariOn(&(em)->atari, 0x300);                                                  \
            }                                                                                  \
            (em)->xFE = NEXT;                                                                  \
        } else {                                                                               \
            if (FALL_MTX) {                                                                    \
                em2cSetFallMatrix(em);                                                         \
            }                                                                                  \
            if (END_INC) {                                                                     \
                if (MotionMoveF(em, 0)) {                                                      \
                    (em)->xFE++;                                                               \
                }                                                                              \
            } else {                                                                           \
                MotionMoveF(em, 0);                                                            \
            }                                                                                  \
        }                                                                                      \
    }

static void em2c_R1_Dm_Jump(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec v;
    Vec ofs;
    u8 fe;

    w->flags |= 0x40;
    em->setStatus(3);
    fe = em->xFE;
    switch (fe) {
    case 0:
        em->rot.y = GetXZAngle(&em->pos, &em->x328);
        ofs.x = 0.0f;
        ofs.y = -1028.31995f;
        ofs.z = 0.0f;
        PSMTXMultVec(em->mat, &ofs, &em->pos);
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = -100.0f;
        PSMTXMultVecSR(em->mat, &w->spd, &w->spd);
        MotionSetCore(em, &em->mot, ARC(0x67), 0, 0, 5, 0);
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        w->flags |= 0x4000;
        EffectEspDelete(0, w->espKind2, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind2, (int) em);
        EffectEfmDelete(0, w->espKind2, (int) em);
        em->xFE++;
    case 1:
        EM2C_DM_FALL(em, w, v, 1, 0, 0, 4, 1, 1);
        break;
    case 2:
        MotionSetCore(em, &em->mot, ARC(0x68), 0, 0, 5, 0);
        em->xFE++;
    case 3:
        EM2C_DM_FALL(em, w, v, 1, 1, 0, 4, 1, 0);
        break;
    case 4:
        em->xFE++;
    case 5:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            if (em->hp > 0) {
                EmRoutineSet(em, 1, 0xE, 0, 0);
            } else {
                EmRoutineSet(em, 3, 3, 0, 0);
            }
        }
        break;
    }
}

static void em2c_R1_Dm_Wall(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec ofs;
    Mtx m;
    u8 fe;

    w->flags |= 0x40;
    em->setStatus(3);
    fe = em->xFE;
    switch (fe) {
    case 0:
        PSVECScale(&w->wallNrm, &w->spd, 100.0f);
        PSVECScale(&w->wallNrm, &ofs, 700.0f);
        PSVECAdd(&em->pos, &ofs, &em->pos);
        TransMatrix(em->mat, &em->pos);
        PSMTXRotRad(m, 'x', 3.14159274f);
        PSMTXConcat(em->mat, m, em->mat);
        MotionSetCore(em, &em->mot, ARC(0x68), 0, 0, 5, 0);
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        w->flags |= 0x4000;
        EffectEspDelete(0, w->espKind2, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind2, (int) em);
        EffectEfmDelete(0, w->espKind2, (int) em);
        em->xFE++;
    case 1:
        EM2C_DM_FALL(em, w, ofs, 1, 1, 0, 2, 1, 0);
        break;
    case 2:
        em->xFE++;
    case 3:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            if (em->hp > 0) {
                EmRoutineSet(em, 1, 0xE, 0, 0);
            } else {
                EmRoutineSet(em, 3, 3, 0, 0);
            }
        }
        break;
    }
}

static void em2c_R1_Dm_Guard(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int lock;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x11), (int) ARC(0x12), 5, 1, 0);
        w->timer = 30;
        EffectEspDelete(0, w->espKind2, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind2, (int) em);
        EffectEfmDelete(0, w->espKind2, (int) em);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->dmgTotal = 0;
            lock = em2cLockCk(em);
            if (lock) {
                if (em2cToCeilingCk(em) == 0) {
                    EmRoutineSet(em, 1, 5, 0, 0);
                }
                break;
            }
            if (Rnd() % 3 == 0) {
                if (em2cToCeilingCk(em)) {
                    break;
                }
            }
            if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, lock, lock);
            } else if ((w->flags & 1) && (Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, lock, lock);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
            break;
        }
        if ((em->seFlags28B & 4) && (w->flags & 1)) {
            f32 plAng = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f));
            f32 lim = 12250000.0f;
            if (plAng > 1.57079637f) {
                lim = 4000000.0f;
            }
            if (em->plDist2 < lim && w->routeAngAbs < 0.52359879f) {
                if (pPL->xFC != 0 && pPL->xFD != 6 && ItemMgr.bulletNumCurrent() == 0 && Rnd() % 10 > 4 &&
                    !(w->flags & 0x800)) {
                    EmRoutineSet(em, 1, 7, 0, 0);
                    break;
                }
                if (em->plDist2 < 6250000.0f && plAng < 1.57079637f) {
                    EmRoutineSet(em, 1, 9, 0, 0);
                    break;
                }
                switch (Rnd() % 3) {
                default:
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                    break;
                case 1:
                    EmRoutineSet(em, 1, 0xC, 0, 0);
                    break;
                case 2:
                    EmRoutineSet(em, 1, 0xA, 0, 0);
                    break;
                }
                break;
            }
            if ((Rnd() & 1) && w->targetAngAbs < 0.52359879f) {
                EmRoutineSet(em, 1, 2, 0, 0);
                break;
            }
        }
        if (w->timer) {
            w->timer--;
            break;
        }
        if (em2cLockCk(em)) {
            int r = em2cToCeilingCk(em);  // block-local (`mr. r3,r3`): identical to the case-1 copy, cross-jumped

            if (r == 0) {
                EmRoutineSet(em, 1, 5, r, r);
            }
        }
        break;
    }
}

static void em2c_R1_Dm_Freeze(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int fe;
    int hit;

    w->flags &= ~8;
    fe = em->xFE;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x2E), (int) ARC(0x2F), 5, 1, 0);
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        SndCall(8, 0x33, &em->pos, em->id, 0, em);
        SndCall(8, 0x34, &em->pos, em->id, 0, em);
        EffectEspDelete(0, w->espKind2, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind2, (int) em);
        EffectEfmDelete(0, w->espKind2, (int) em);
        w->x6B8 = fe;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x18, 0, 0);
            break;
        }
        if (em->plDist2 < 2250000.0f) {
            hit = w->x6B8;
            if (hit == 0 && fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f)) < 0.785398185f) {
                ActBtn.set(7, 0xB, (int) em2cKickAction, (int) em, 1, 1, 0, hit);
            }
        }
        break;
    }
}

static void em2c_R1_Dm_C_Freeze(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec v;
    u8 fe;

    w->flags |= 0x40;
    em->setStatus(3);
    fe = em->xFE;
    switch (fe) {
    case 0:
        PSVECScale(&w->wallNrm, &w->spd, 100.0f);
        em->pos = pPLS->pos;
        em->pos.y += 6000.0f;
        em->rot.y = pPLS->rot.y + 3.14159274f;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionSetCore(em, &em->mot, ARC(0x68), 0, 0, 5, 0);
        em->alpha = 1.0f;
        em->be_flag |= 2;
        EstSet((int) em, -1, 0, 0, 0x24, 1, 0, w->espKind, (u32) em, (void*) fe);
        EstSet((int) em, -1, 0, 0, 0x24, 6, 0, w->espKind, (u32) em, (void*) fe);
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        SndCall(8, 0x33, &em->pos, em->id, 0, em);
        SndCall(8, 0x34, &em->pos, em->id, 0, em);
        w->flags |= 0x4000;
        EffectEspDelete(0, w->espKind2, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind2, (int) em);
        EffectEfmDelete(0, w->espKind2, (int) em);
        em->xFE++;
    case 1:
        EM2C_DM_FALL(em, w, v, 0, 1, 1, 2, 0, 0);
        break;
    case 2:
        em->xFE++;
    case 3:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            if (em->hp > 0) {
                EmRoutineSet(em, 1, 0xE, 0, 0);
            } else {
                EmRoutineSet(em, 3, 3, 0, 0);
            }
        }
        break;
    }
}

static void em2cKickAction(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    SetPlDamage((int) em, plemKick);
    pPL->dmg.set(0, 30);
    if (pSUB) {
        cDmgInfo* d = &pSUB->dmg;  // &pSUB->dmg is computed before the dead test

        if (!em2cDeadCk(pSUB)) {
            d->set(0, 30);
        }
    }
    w->x6B8 = 1;
    GameAddPoint(9);
}

static void plemKick(cPlayer* pl)
{
    Vec pos;

    pl->subArc = PL_EM(pl)->subArc;
    pl->dmg.set(0, 30);
    pG->flags_5014 |= 0x40000000;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, &pl->mot, PL_ARC(0x7C), 0, 6, 1, 0);
        pl->x3E4 = 10;
        pl->x3E8 = 33;
        GameAddPoint(9);
        pl->x3E0 = 17;
        pl->xFF = Rnd() & 3;
        pl->xFE++;
    case 1:
        if (pl->x3E4) {
            pl->x3E4--;
            pl->rot.y += Muku(&pl->pos, &PL_EM(pl)->pos, pl->rot.y, 0.196349546f);
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
            if (pl->x3E4 == 0) {
                SndCall(1, 0x11, &pl->pos, 0, 0, pl);
                SndCall(1, 0x10, &pl->pos, 0, 0, pl);
            }
        }
        if (pl->x3E0) {
            pl->x3E0--;
            if (pl->x3E0 == 0) {
                pos.x = 0.0f;
                pos.y = 1500.0f;
                pos.z = 300.0f;
                PSMTXMultVec(pPL->mat, &pos, &pos);
                if (PlWepHitCheck3(&pos, 0x14, 10, 1200.0f)) {
                    SndCall(1, 0xF, &pl->pos, 0, 0, pl);
                }
                pos.x = 0.0f;
                pos.y = 1000.0f;
                pos.z = 300.0f;
                PSMTXMultVec(pPL->mat, &pos, &pos);
                if (PlWepHitCheck3(&pos, 0x14, 10, 1200.0f)) {
                    SndCall(1, 0xF, &pl->pos, 0, 0, pl);
                }
            }
        }
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 15);
            break;
        }
        if (pl->x3E8) {
            pl->x3E8--;
            break;
        }
        if (joyKamae() || (Key.on & 0x10F)) {
            EndPlDamage();
            pl->dmg.set(0, 15);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void em2c_R1_Dm_F_Normal(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int flip;
    int hit;

    w->flags |= 0x100;
    switch (em->xFE) {
    case 0:
        flip = 0x41;
        if (em->motFlags & 0x40) {
            flip = 1;
        }
        if (fabsf(Muku(&em->pos, &em->x328, em->rot.y, 3.14159274f)) < 1.57079637f) {
            MotionSetCore(em, &em->mot, ARC(0x3A), (int) ARC(0x3B), 5, flip, 0);
        } else {
            MotionSetCore(em, &em->mot, ARC(0x3C), (int) ARC(0x3D), 5, flip, 0);
        }
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        w->timer = 30;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->dmgTotal = 0;
            if (w->guardCnt == 0) {
                EmRoutineSet(em, 1, 0x1A, 0, 0);
            } else if (w->targetAngAbs > 2.09439516f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x18, 0, 0);
            }
            break;
        }
        if (w->timer) {
            w->timer--;
        } else if (w->guardCnt) {
            w->flags &= ~8;
        }
        if (w->guardCnt == 0) {
            w->dmgTotal = 0;
            EmRoutineSet(em, 1, 0x1A, 0, 0);
            break;
        }
        if (em->plDist2 < 2250000.0f) {
            hit = w->x6B8;
            if (hit == 0 && fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f)) < 0.785398185f) {
                ActBtn.set(7, 0xB, (int) em2cKickAction, (int) em, 1, 1, 0, hit);
            }
        }
        break;
    }
}

static void em2c_R1_Dm_F_Blow(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int fe;

    fe = em->xFE;
    switch (fe) {
    case 0:
        if (fabsf(Muku(&em->pos, &em->x328, em->rot.y, 3.14159274f)) < 1.57079637f) {
            em->rot.y += Muku(&em->pos, &em->x328, em->rot.y, 3.14159274f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
            MotionSetCore(em, &em->mot, ARC(0x3E), (int) ARC(0x3F), 5, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x24, 0x28, 0, 0, (u32) em, (void*) fe);
        } else {
            em->rot.y += Muku(&em->x328, &em->pos, em->rot.y, 3.14159274f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
            MotionSetCore(em, &em->mot, ARC(0x40), (int) ARC(0x41), 5, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x24, 0x29, 0, 0, (u32) em, (void*) fe);
        }
        if (em->hp > 0) {
            SndStop(w->breathSnd, 0);
            w->breathTimer = 2;
            SndCall(8, 0xF, &em->pos, em->id, 0, em);
        }
        w->flags |= 0x4000;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0xE, 0, 0);
            }
        }
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Routine 3: die

// The enemy's effects (espKind) go at death.
static inline void em2cDieEffDelete(cEm2c* em, Em2cWork* w)
{
    EffectEspDelete(0, w->espKind, (u32) em, 0);
    EffectEspgenDelete(0, w->espKind, (int) em);
    EffectEfmDelete(0, w->espKind, (int) em);
}

static void em2c_R0_Die(cEm2c* em)
{
    EM2C_WK(em)->flags |= 8;
    Em2c_R1_die_tbl[em->xFD](em);
}

static void em2c_R1_Die_Lost(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int fe;

    fe = em->xFE;
    if (fe == 0) {
        AtariOff(&em->atari, 0xFCFF);
        EmSetDie(em);
        EmReserveDropItem(em);
        em->clearStatus(5);
        em->setStatus(8);
        EmSetDropItem(em);
        em2cDieEffDelete(em, w);
        if (em->be_flag & 2) {
            EstSet((int) em, -1, 0, 0, 0x24, 0x31, 0, 0, fe, (void*) fe);
        }
        w->timer = 30;
        w->timer8 = 150;
        w->x558 = 1.0f;
        em->xFE++;
    }
}

static void em2c_R1_Die_Normal(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int fe;

    fe = em->xFE;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x90), (int) ARC(0x91), 5, 1, 0);
        EmSetDieCntE(em);
        em2cDieEffDelete(em, w);
        EstSet((int) em, -1, 0, 0, 0x24, 0x33, 0, 0, (u32) em, (void*) fe);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 3, 0, 0, 0);
        }
        break;
    }
}

static void em2c_R1_Die_Freeze(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec rot;
    int t;

    switch (em->xFE) {
    case 0:
        EmSetDieCntE(em);
        w->timer = 11;
        w->timer8 = 8;
        em->xFE++;
    case 1:
        if (w->timer8) {
            t = w->timer8 - 1;
            w->timer8 = t;
            if (t == 0) {
                Camera* cam = &pG->Cam;
                em2cDieEffDelete(em, w);
                rot.x = 0.0f;
                rot.y = GetXZAngle(&em->pos, &cam->param.pos);
                rot.z = 0.0f;
                EstSet((int) em, -1, 0, 0, 0x24, 0xA, 0, 0, t, (void*) t);
                EstSet(0, -1, &em->pos, &rot, 0x24, 0x32, 0, 0, t, (void*) t);
                SndCall(8, 0x41, &em->pos, em->id, 0, em);
            }
        }
        if (w->timer) {
            w->timer--;
        } else {
            em->be_flag &= ~2;
            EmRoutineSet(em, 3, 0, 0, 0);
        }
        break;
    }
}

static void em2c_R1_Die_Down(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    switch (em->xFE) {
    case 0:
        if (w->flags & 0x4000) {
            MotionSetCore(em, &em->mot, ARC(0x93), 0, 5, 1, 0);
        } else {
            MotionSetCore(em, &em->mot, ARC(0x6D), (int) ARC(0x6E), 5, 1, 0);
        }
        EmSetDieCntE(em);
        em2cDieEffDelete(em, w);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 3, 0, 0, 0);
        }
        break;
    }
}

static void em2c_R1_Die_Wall(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec ofs;
    Mtx m;
    u8 fe;

    w->flags |= 0x40;
    em->setStatus(3);
    fe = em->xFE;
    switch (fe) {
    case 0:
        PSVECScale(&w->wallNrm, &w->spd, 100.0f);
        PSVECScale(&w->wallNrm, &ofs, 700.0f);
        PSVECAdd(&em->pos, &ofs, &em->pos);
        TransMatrix(em->mat, &em->pos);
        PSMTXRotRad(m, 'x', 3.14159274f);
        PSMTXConcat(em->mat, m, em->mat);
        MotionSetCore(em, &em->mot, ARC(0x68), 0, 0, 5, 0);
        w->flags |= 0x4000;
        em2cDieEffDelete(em, w);
        EmSetDieCntE(em);
        em->xFE++;
    case 1:
        EM2C_DM_FALL(em, w, ofs, 0, 0, 0, 2, 1, 0);
        break;
    case 2:
        em->xFE++;
    case 3:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 3, 3, 0, 0);
        }
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Helpers

void em2cRouteCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec plPos;
    Vec a;
    Vec b;
    int up;

    if (em->hp <= 0) {
        return;
    }
    up = 0;
    if (fabsf(pPL->pos.y - em->pos.y) > 1000.0f) {
        up = 1;
    }
    RouteCkToPos(em, &pPL->pos, &w->routePos, up, 0);
    w->routeAng = Muku(&em->pos, &w->routePos, em->rot.y, 3.14159274f);
    w->routeAngAbs = fabsf(w->routeAng);
    if (em->xFC == 0) {
        w->routeAng = 0.0f;
        w->routeAngAbs = 0.0f;
        em->plDist2 = 100000000.0f;
    }
    plPos = pPL->pos;
    plPos.y += 1800.0f;
    w->plDir = em2cGetPlDir(em, &plPos);
    w->plDirAbs = fabsf(w->plDir);
    w->flags &= ~1;
    if (em->xFC != 0) {
        a = em->getPartsPtr(0)->worldPos;
        b.x = pPLS->pos.x;  // struct view: the pPL load stays below the `a` copy's stores
        b.y = pPLS->pos.y + 1500.0f;
        b.z = pPLS->pos.z;
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
            w->flags |= 1;
        }
    }
    w->plDist = RouteCkPosToPosDis(&em->pos, &pPL->pos);
    w->homeDist = RouteCkPosToPosDis(&w->homePos, &pPLS->pos);
    w->targetPos = w->routePos;
    w->targetAng = w->routeAng;
    w->targetAngAbs = w->routeAngAbs;
    w->targetDist = em->plDist2;
    w->pTarget = pPLS;
    w->flags &= ~4;
}

int em2cSetDmVal(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int near = 0;
    int dmg;

    if (em->dmPart->rad < 36000000.0f) {
        near = 1;
    }
    dmg = 100;
    if (em->dmWep <= 0x2D) {
        dmg = GetWepDmVal(em, em->dmWep, near);
    }
    if (w->flags & 0x800) {
        dmg *= 3;
    }
    return dmg;
}

int em2cAtkCk(cEm2c* em, int no, int parts)
{
    Em2cWork* w = EM2C_WK(em);
    EmAtkInfo* atk;
    cModel* p;
    int hit;

    if (w->atkHit) {  // the early return's label keeps `mr r3, em` before getPartsPtr (em26 idiom)
        return 0;
    }
    atk = &em2c_atk_info[no];
    p = em->getPartsPtr(parts);
    hit = EmAtkHitCk(atk, &p->worldPos, &p->oldWorldPos, 0);
    if (hit) {
        if (hit & 1) {
            switch ((u32) no) {
            case 0:
                if (em->motFlags & 0x40) {
                    pPL->x328 = em->pos;
                    EmPlBloodSet2(em, &p->worldPos, 1, 0x24, 0x18);
                } else {
                    pPL->x328 = em->pos;
                    EmPlBloodSet2(em, &p->worldPos, 1, 0x24, 0x17);
                }
                if ((s16) pG->pl_life > 0) {
                    SetPlDamage((int) em, plemDmSide);
                    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f)) < 1.57079637f) {
                        pPLS->rot.y += Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f);
                        pPLS->xFF = 0;
                    } else {
                        pPLS->rot.y += Muku(&em->pos, &pPL->pos, pPL->rot.y, 3.14159274f);
                        pPLS->xFF = 1;
                    }
                    if (em->motFlags & 0x40) {
                        if (pPL->xFF) {
                            pPL->xFF = 0;
                        } else {
                            pPL->xFF = 1;
                        }
                    }
                } else {
                    em2cPlHeadLost();
                }
                break;
            case 3:
                if (em->motFlags & 0x40) {
                    pPL->x328 = em->pos;
                    EmPlBloodSet2(em, &p->worldPos, 1, 0x24, 0x18);
                } else {
                    pPL->x328 = em->pos;
                    EmPlBloodSet2(em, &p->worldPos, 1, 0x24, 0x17);
                }
                if ((s16) pG->pl_life <= 0) {
                    em2cPlHeadLost();
                }
                break;
            case 4:
                pPL->x328 = em->pos;
                EmPlBloodSet2(em, &p->worldPos, 1, 0x24, 0x22);
                if ((s16) pG->pl_life <= 0) {
                    em2cPlHeadLost();
                }
                break;
            case 1:
                pPL->x328 = em->pos;
                EmPlBloodSet2(em, &p->worldPos, 1, 0x24, 0x15);
                if ((s16) pG->pl_life <= 0) {
                    pG->pl_life = 0;
                    SetPlDamage((int) em, plem2c_CriticalHit);
                } else {
                    pPL->rot.y = GetXZAngle(&pPL->pos, &em->pos);
                    PlSetDamage(8, 0, 0);
                }
                break;
            case 2:
                EmPlBloodSet2(em, &p->worldPos, 1, 0x24, 0x19);
                pPL->rot.y = GetXZAngle(&pPL->pos, &em->pos);
                PlSetDamage(8, 0, 0);
                break;
            case 7:
                EmPlBloodSet2(em, &p->worldPos, 1, 0x24, 0x1A);
                if ((s16) pG->pl_life <= 0) {
                    pPL->rot.y = GetXZAngle(&pPL->pos, &em->pos);
                    PlSetDamage(8, 0, 0);
                } else {
                    SetPlDamage((int) em, plemDmTail);
                }
                break;
            case 5:
                EmPlBloodSet2(em, &p->worldPos, 1, 0x24, 0x1C);
                EstSet((int) pPL, -1, 0, 0, 0x24, 0x1D, 0, 0, (u32) pPL, 0);
                if ((s16) pG->pl_life <= 0) {
                    pPL->rot.y = GetXZAngle(&pPL->pos, &em->pos);
                    PlSetDamage(8, 0, 0);
                } else {
                    SetPlDamage((int) em, plemDmTail);
                }
                break;
            }
            w->atkHit = 1;
            em->flags_3C8 |= 4;
        }
        if (hit & 2) {
            EmSubBloodSet(em, &p->worldPos, 1, 0xFF, 0xFF);
            w->atkHit = 1;
        }
        QuakeExec(0, 0, 5, 22.0f, 2);
        SndCall(8, 0xD, &em->pos, em->id, 0, em);
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        return 1;
    }
    return 0;
}
void em2cScaleCompress(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Mtx m;
    Vec scale;
    cModel* p;

    if (em->xFC != 3) {
        return;
    }
    if (em->xFD != 0) {
        return;
    }
    PSMTXIdentity(m);
    scale.x = 1.0f;
    scale.y = w->x558;
    scale.z = 1.0f;
    ScaleMatrix(m, &scale);
    for (p = em->pParts; p; p = p->pParts) {
        PSMTXConcat(m, p->mat, p->mat);
        p->mat[0][3] = p->worldPos.x;
        p->mat[1][3] = p->worldPos.y;
        p->mat[2][3] = p->worldPos.z;
    }
}

int em2cLockCk(cEm2c* em)
{
    Mtx inv;
    Vec pos;

    if (pPL->xFC != 0) {
        return 0;
    }
    if (pPL->xFD != 6) {
        return 0;
    }
    if (em->plDist2 > 144000000.0f) {
        return 0;
    }
    if (pG->wep_no == 0x10) {
        return 0;
    }
    if (pG->x4F88 <= 1) {
        return 0;
    }
    if (ItemMgr.bulletNumCurrent() == 0) {
        return 0;
    }
    switch (pG->wep_no) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, 3.14159274f)) > 0.785398185f) {
        return 0;
    }
    if (pPL->pWep->pObj->wep.target && pPL->pWep->pObj->wep.target == em) {
        return 1;
    }
    PSMTXInverse(pPL->getPartsPtr(10)->mat, inv);
    PSMTXMultVec(inv, &em->getPartsPtr(0)->worldPos, &pos);
    if (pos.x > 0.0f) {
        return 0;
    }
    if (pos.z > 800.0f || pos.z < -800.0f) {
        return 0;
    }
    if (pos.y > 800.0f) {
        return 0;
    }
    if (pos.y < -800.0f) {
        return 0;
    }
    return 1;
}

int em2cSetWallMatrix2(cEm2c* em, f32 rate)
{
    Em2cWork* w = EM2C_WK(em);
    Mtx m;
    Vec nrm;
    Vec n0;
    Vec n1;
    Vec n2;
    Vec n3;
    Vec hit;
    Vec a;
    Vec b;
    Vec up;
    Vec sum;
    f32 ang;

    PSMTXCopy(em->mat, m);
    TransMatrix(m, &em->pos);
    a.x = 0.0f;
    a.y = 700.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = -500.0f;
    b.z = 1200.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    n0.x = 0.0f;
    n0.y = 0.0f;
    n0.z = 0.0f;
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0x383830)) {
        n0 = nrm;
    }
    a.x = 0.0f;
    a.y = 700.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = -500.0f;
    b.z = -1200.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    n1.x = 0.0f;
    n1.y = 0.0f;
    n1.z = 0.0f;
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0x383830)) {
        n1 = nrm;
    }
    b.x = -1200.0f;
    a.x = 0.0f;
    a.y = 700.0f;
    a.z = 0.0f;
    b.y = -500.0f;
    b.z = 0.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    n2.x = 0.0f;
    n2.y = 0.0f;
    n2.z = 0.0f;
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0x383830)) {
        n2 = nrm;
    }
    a.y = 700.0f;
    b.x = 1200.0f;
    b.y = -500.0f;
    a.x = 0.0f;
    a.z = 0.0f;
    b.z = 0.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    n3.x = 0.0f;
    n3.y = 0.0f;
    n3.z = 0.0f;
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0x383830)) {
        n3 = nrm;
    }
    PSVECAdd(&n0, &n1, &sum);
    PSVECAdd(&n2, &sum, &sum);
    PSVECAdd(&n3, &sum, &sum);
    if (sum.x == 0.0f && sum.y == 0.0f && sum.z == 0.0f) {
        sum.y = 1.0f;
    }
#line 7932 "D:/Bio4/Prog/em2c.cpp"
    VECNormalize(&sum, &w->wallNrm);
    em->motFlags2 |= 0x40000000;
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    PSMTXMultVecSR(em->mat, &up, &up);
#line 7944 "D:/Bio4/Prog/em2c.cpp"
    VECNormalize(&up, &up);
    PSVECCrossProduct(&up, &w->wallNrm, &sum);
    ang = acosf(PSVECDotProduct(&up, &w->wallNrm));
    if (ang > 0.00100000005f) {
        PSMTXRotAxisRad(m, &sum, ang * rate);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
    a.x = 0.0f;
    a.y = 600.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = -600.0f;
    b.z = 0.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0x383830)) {
        em->pos = hit;
        sum.y = sinf(acosf(PSVECDotProduct(&nrm, &w->wallNrm))) * 700.0f;
        sum.x = 0.0f;
        sum.z = 0.0f;
        PSMTXMultVecSR(em->mat, &sum, &sum);
        PSVECAdd(&em->pos, &sum, &em->pos);
        TransMatrix(em->mat, &em->pos);
        return 1;
    }
    return 0;
}

void em2cSetFallMatrix(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec up;
    Vec axis;
    Mtx m;
    f32 ang;

    w->wallNrm.x = 0.0f;
    w->wallNrm.y = 1.0f;
    w->wallNrm.z = 0.0f;
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    em->motFlags2 |= 0x40000000;
    PSMTXMultVecSR(em->mat, &up, &up);
#line 8022 "D:/Bio4/Prog/em2c.cpp"
    VECNormalize(&up, &up);
    PSVECCrossProduct(&up, &w->wallNrm, &axis);
    ang = acosf(PSVECDotProduct(&up, &w->wallNrm));
    if (ang > 0.00100000005f) {
        PSMTXRotAxisRad(m, &axis, ang * 0.200000003f);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
}

void em2cPlHeadLost()
{
    Vec ofs;
    Vec spd;
    Vec rot;
    cModel* p3;
    cObj* obj;
    int zero;

    if (pSys->region == 0) {
        PlSetDamageSe(0xD);
        EstSet((int) pPL, -1, 0, 0, 0x24, 0x2E, 0, 0, (u32) pPL, 0);
        return;
    }
    zero = 0;
    SndCall(1, 0x3E, &pPL->pos, 0, 0, pPL);
    EstSet((int) pPL, -1, 0, 0, 0x24, 0x2F, 0, 0, (u32) pPL, (void*) zero);
    pPL->setHead(0);
    p3 = pPL->getPartsPtr(3);
    ofs.x = 0.0f;
    ofs.y = 68.0f;
    ofs.z = 28.0f;
    spd.x = 0.0f;
    spd.y = 80.0f;
    spd.z = -50.0f;
    rot = pPL->rot;
    rot.y += 3.14159274f;
    rot.y = LIMIT_ANGLE(rot.y);
    PSMTXMultVec(p3->mat, &ofs, &ofs);
    PSMTXMultVecSR(pPL->mat, &spd, &spd);
    obj = SetObj01(PL_ARC_PTR(pG->pPlArc, 0xC), PL_ARC_PTR(pG->pPlArc, 7), &ofs, &rot, &spd, 15.0f, 150.0f, 1000, 0x11);
    if (obj) {
        obj->lightInfo.x50 = 1;
        Obj01SetEst(obj, 0, -1, 4, 0, -1, 0, -1, (int) zero, -1);
    }
    EstSet((int) obj, -1, 0, 0, 0x24, 0x30, 0, 0, (u32) obj, (void*) zero);
}

f32 em2cGetPlDir(cEm2c* em, Vec* pos)
{
    Mtx inv;
    Vec d;

    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, pos, &d);
    return atan2f(d.x, d.z);
}

int em2cWallFallCk(cEm2c* em)
{
    Vec a;
    Vec b;

    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 500.0f;
    b.x = 0.0f;
    b.y = -1000.0f;
    b.z = 500.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x383830)) {
        return 0;
    }
    a.x = 0.0f;
    a.y = -500.0f;
    a.z = 500.0f;
    b.x = 0.0f;
    b.y = -500.0f;
    b.z = -1000.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    return SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x383830) == 0;
}

// Door in the way while walking: shock it open (or break it) when the enemy is stuck.
void em2cDoorOpenCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec v;
    f32 ang;
    u32 i;

    if (w->x574) {
        return;
    }
    if (w->stuckCnt > 3) {
        return;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEmDoor* e = (cEmDoor*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        EmDoorWork* dw;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x41) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y) +
                (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
            6250000.0f) {
            continue;
        }
        dw = EMDOOR_WK(e);
        ang = fabsf(Muku2(dw->rotY, em->rot.y, 3.14159274f));
        if (ang > 0.785398185f && ang < 2.3561945f) {
            continue;
        }
        PSMTXMultVec(dw->inv, &em->pos, &v);
        if (ang < 1.57079637f) {
            if (v.z > 0.0f || v.z < -800.0f) {
                continue;
            }
        } else {
            if (v.z < 0.0f || v.z > 800.0f) {
                continue;
            }
        }
        if (v.x > dw->width || v.x < -dw->width) {
            continue;
        }
        if (v.y > 500.0f || v.y < -500.0f) {
            continue;
        }
        switch (e->ckOpen()) {
        case 0:
        default:
            if (e->hp > 300) {
                e->setShock(0, &em->pos, 0);
                e->hp -= 800;
                if (e->hp <= 0) {
                    e->hp = 1;
                }
                w->x574 = Rnd() % 15 + 15;
                w->x570 = 150;
                if (EM_RTN(em, 1, 2)) {
                    em->xFC = 1;
                    em->xFD = 1;
                    em->xFE = 0;
                    em->xFF = 0;
                }
            } else if (w->flags & 0x800) {
                em->xFC = 1;
                em->xFD = 0x19;
                em->xFE = 0;
                em->xFF = 1;
            } else {
                em->xFC = 1;
                em->xFD = 9;
                em->xFE = 0;
                em->xFF = 1;
            }
            break;
        case 1:
        case 2:
        case 3:
            break;
        }
    }
}

void em2cDoorOpenCk2(cEm2c* em)
{
    Vec v;
    f32 ang;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEmDoor* e = (cEmDoor*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        EmDoorWork* dw;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x41) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y) +
                (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
            6250000.0f) {
            continue;
        }
        dw = EMDOOR_WK(e);
        ang = fabsf(Muku2(dw->rotY, em->rot.y, 3.14159274f));
        if (ang > 0.785398185f && ang < 2.3561945f) {
            continue;
        }
        PSMTXMultVec(dw->inv, &em->pos, &v);
        if (ang < 1.57079637f) {
            if (v.z > 0.0f || v.z < -800.0f) {
                continue;
            }
        } else {
            if (v.z < 0.0f || v.z > 800.0f) {
                continue;
            }
        }
        if (v.x > dw->width || v.x < -dw->width) {
            continue;
        }
        if (v.y > 500.0f || v.y < -500.0f) {
            continue;
        }
        switch (e->ckOpen()) {
        case 0:
        default:
            e->setOpen(&em->pos, 0, 0, 1);
            break;
        case 1:
        case 2:
        case 3:
            break;
        }
    }
}

static u8 em2c_cloth_parts[20] = { 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42,
                                   0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C };
static u8 em2c_cloth_up[20] = { 0xFF, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41,
                                0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B };
static u8 em2c_cloth_down[20] = { 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43,
                                  0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0xFF };
static f32 em2c_cloth_max[20] = { 0.3f, 0.4f, 0.53f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f,
                                  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

void em2cClothSet(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    int zero = em->type;

    if (zero != 0) {
        return;
    }
    w->cloth.num = 20;
    w->cloth.pParts = em2c_cloth_parts;
    w->cloth.pLeft = (u8*) zero;
    w->cloth.pRight = (u8*) zero;
    w->cloth.pUpLeft = (u8*) zero;
    w->cloth.x14 = zero;
    w->cloth.pUp = em2c_cloth_up;
    w->cloth.pDown = em2c_cloth_down;
    w->cloth.pWindS = (f32*) zero;
    w->cloth.pWindR = (f32*) zero;
    w->cloth.x20 = zero;
    w->cloth.pRate = (f32*) zero;
    w->cloth.pMax = em2c_cloth_max;
    w->cloth.pAt = (PlClothAt*) zero;
    w->cloth.nAt = zero;
    w->cloth.x3C = 40.0f;
    w->cloth.x40 = 0.6f;
    w->cloth.x44 = 3;
    {
        const f32 z = 0.0f;  // pool order: 0.0 before 0.05

        w->cloth.x4C = 0.05f;
        w->cloth.pModel = (cModel*) zero;
        w->cloth.x48 = z;
        w->cloth.x50 = z;
    }
    w->cloth.flags = zero;
    w->cloth.x54 = zero;
    PenClothSet(em, (PenCloth*) &w->cloth, 100.0f);
}

void em2cClothMove(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    cModel* p;
    u32 i;

    if (em->type != 0) {
        return;
    }
    if (w->flags & 0x20000) {
        em->be_flag |= 0x200000;
        return;
    }
    PenClothMove2(em, (PenCloth*) &w->cloth);
    for (i = 0x4B; i <= 0x4C; i++) {
        p = em->getPartsPtr(i);
        PSMTXConcat(p->pParent->mat, p->worldMat, p->mat);
        p->worldPos.x = p->mat[0][3];
        p->worldPos.y = p->mat[1][3];
        p->worldPos.z = p->mat[2][3];
    }
    em->be_flag &= ~0xE00000;
}

void em2cFootSeMove(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    cModel* p;
    cModel* p0;
    int parts;
    u32 no;

    if (em->seNo == 0) {
        return;
    }
    if (em->type != 0) {
        return;
    }
    no = em->seNo - 1;
    parts = 0x12;
    switch (no) {
    case 0:
        break;
    case 1:
        parts = 0x16;
        break;
    case 4:
        em->seNo = 0;
        em2cSetdLandingEff(em);
        return;
    case 5:
        em->seNo = 0;
        em2cSetDownEff(em);
        return;
    case 0xE:
        em->seNo = 0;
        em2cSetJumpEff(em);
        return;
    default:
        return;
    }
    p = em->getPartsPtr(parts);
    p0 = em->getPartsPtr(0);
    if (w->wallNrm.y < 0.0f) {
        EstSet(0, -1, &p->worldPos, 0, 0x24, 0xD, 0, 0, 0, 0);
    }
}

void em2cSetdLandingEff(cEm2c* em)
{
    em->getPartsPtr(0);
    SndCall(8, 4, &em->pos, em->id, 0, em);
    EstSet((int) em, -1, 0, 0, 0x24, 0x2A, 0, 0, 0, 0);
}

void em2cSetDownEff(cEm2c* em)
{
    cModel* p0 = em->getPartsPtr(0);

    SndCall(8, 5, &p0->worldPos, em->id, 0, em);
}

void em2cSetJumpEff(cEm2c* em)
{
    cModel* p0 = em->getPartsPtr(0);

    SndCall(8, 0xE, &p0->worldPos, em->id, 0, em);
}

int em2cFallCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    if (SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) > em->pos.y - 250.0f) {
        return 0;
    }
    w->wallNrm.x = 0.0f;
    w->wallNrm.y = 1.0f;
    w->wallNrm.z = 0.0f;
    EmRoutineSet(em, 1, 0x20, 0, 0);
    return 1;
}

int em2cDownJumpCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec a;
    Vec b;

    if (!(w->flags & 0x4000)) {
        return 0;
    }
    if (Rnd() & 1) {
        return 0;
    }
    if (em2cNoWallCk(em)) {
        return 0;
    }
    return 0;
}

int em2cToCeilingCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Vec a;
    Vec b;
    int r;

    if (pG->room_id != 0x221) {
        return 0;
    }
    r = em2cNoWallCk(em);
    if (r) {
        return 0;
    }
    b = em->pos;
    a = b;
    a.y += 500.0f;
    b.y += 8000.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x383830) == 0) {
        return 0;
    }
    w->dmgTotal = r;
    EmRoutineSet(em, 1, 0x11, r, r);
    return 1;
}

int em2cJumpDownCk(cEm2c* em)
{
    Vec a;
    Vec b;
    Vec hit;

    return 0;
}

int em2cWallOverCk(cEm2c* em)
{
    Vec a;
    Vec b;
    Vec hit;

    return 0;
}

// Neck parts (3) turns towards the player while it is found (addRot.y, damped).
void em2cNeckMove(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    cParts* p;
    cModel* pp;
    Vec v;

    em->getPartsPtr(4);
    pp = pPL->getPartsPtr(4);
    v.x = 0.0f;
    v.y = 250.0f;
    v.z = 0.0f;
    PSMTXMultVec(pp->mat, &v, &v);
    if (w->flags & 0x80) {
        w->x584 = w->x584 * 0.899999976f + Muku(&em->pos, &pPL->pos, em->rot.y, 1.04719758f) * 0.100000001f;
    } else {
        w->x584 = w->x584 * 0.899999976f;
    }
    p = (cParts*) em->getPartsPtr(3);
    p->motParts.flags |= 0x40000000;
    p->addRot.x = 0.0f;
    p->addRot.y = w->x584;
    p->addRot.z = 0.0f;
}

// Player hit by a side attack: knocked to the side the attack came from.
static void plemDmSide(cPlayer* pl)
{
    int flip;

    pl->subArc = PL_EM(pl)->subArc;
    pl->dmg.set(0, 2);
    switch (pl->xFE) {
    case 0:
        flip = 1;
        if (pl->xFF) {
            flip = 0x41;
        }
        MotionSetCore(pl, &pl->mot, PL_ARC(0x83), (int) PL_ARC(0x84), 5, flip, 0);
        PlSetFace(1);
        PlSetDamageSe(0);
        pl->xFE++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 0xF);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Player hit by the tail: one of three knock-back motions.
static void plemDmTail(cPlayer* pl)
{
    void* m0;
    void* m1;
    int flip;

    pl->subArc = PL_EM(pl)->subArc;
    switch (pl->xFE) {
    case 0:
        flip = 1;
        if (Rnd() % 10 > 4) {
            flip = 0x41;
        }
        switch (Rnd() % 3) {
        case 0:
        default:
            m0 = PL_ARC_PTR(pG->pPlArc, 0x48);
            m1 = 0;
            break;
        case 1:
            m0 = PL_ARC_PTR(pG->pPlArc, 0x49);
            m1 = 0;
            break;
        case 2:
            m0 = PL_ARC(0x83);
            m1 = PL_ARC(0x84);
            break;
        }
        MotionSetCore(pl, &pl->mot, m0, (int) m1, 5, flip, 0);
        pl->dmg.set(0, 5);
        PlSetFace(1);
        PlSetDamageSe(0);
        pl->xFE++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void em2cSitAction(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    SetPlDamage((int) em, plem2cSit);
    w->x6B8 = 1;
    GameAddPoint(9);
}

static void plem2cSit(cPlayer* pl)
{
    pl->subArc = PL_EM(pl)->subArc;
    pl->dmg.set(0, 30);
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, &pl->mot, PL_ARC(0x73), 0, 5, 1, 0);
        GameAddPoint(0xB);
        pl->xFE++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void em2cEscapeAction(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    SetPlDamage((int) em, plem2cEscape);
    w->x6B8 = 1;
    GameAddPoint(9);
}

// Player escapes the ambush: a back jump when walls are on both sides, a side roll otherwise.
static void plem2cEscape(cPlayer* pl)
{
    Vec a;
    Vec b;
    int side;
    int wall;
    int fe;

    pl->subArc = PL_EM(pl)->subArc;
    fe = pl->xFE;
    pl->dmg.x1 = 0x1E;
    switch (fe) {
    case 0:
        side = 0;
        if (Muku(&PL_EM(pl)->pos, &pl->pos, pl->rot.y, 3.14159274f) < 0.0f) {
            side = 1;
        }
        side = Rnd() & 1;
        wall = 0;
        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = 2000.0f;
        b.y = 500.0f;
        b.z = 1000.0f;
        PSMTXMultVec(pl->mat, &a, &a);
        PSMTXMultVec(pl->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            wall = 1;
        }
        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = -2000.0f;
        b.y = 500.0f;
        b.z = 1000.0f;
        PSMTXMultVec(pl->mat, &a, &a);
        PSMTXMultVec(pl->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            wall |= 2;
        }
        if (wall == 3) {
            MotionSetCore(pl, &pl->mot, PL_ARC(0x85), 0, 5, 1, 5);
            EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, (void*) fe);
            SndCall(1, 0x43, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
            SndCall(1, 0x44, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
            pl->x3E8 = fe;
        } else {
            if (wall & 1) {
                side = 1;
            }
            if (wall & 2) {
                side = 0;
            }
            if (side) {
                MotionSetCore(pl, &pl->mot, PL_ARC(0x74), (int) PL_ARC(0x75), 5, 1, 0);
            } else {
                MotionSetCore(pl, &pl->mot, PL_ARC(0x74), (int) PL_ARC(0x75), 5, 0x41, 0);
            }
            SndCall(1, 0x48, &pl->pos, 0, 0, pl);
            SndCall(1, 0x11, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
            pl->x3E8 = 1;
        }
        GameAddPoint(0xB);
        pl->x3E0 = 50;
        pl->x3E4 = 15;
        pl->xFE++;
    case 1:
        if (pl->x3E8) {
            em2cEscapeCamMove(PL_EM(pl));
            if (pl->frame > 11.6999998f && pl->frame < 12.3000002f) {
                EstSet(0, -1, &pl->pos, 0, 3, 0x13, 0, 0, 0, 0);
                SndCall(5, 5, &pl->pos, 0, 0, pl);
            }
        } else {
            if (pl->frame > 10.6999998f && pl->frame < 11.3000002f) {
                SndCall(1, 0x4F, &pl->pos, 0, 0, pl);
            }
            if (pl->frame > 21.7000008f && pl->frame < 22.2999992f) {
                SndCall(5, 0x14, &pl->pos, 0, 0, pl);
            }
            if ((pl->frame > 36.7000008f && pl->frame < 37.2999992f) ||
                (pl->frame > 49.7000008f && pl->frame < 50.2999992f)) {
                SndCall(5, 2, &pl->pos, 0, 0, pl);
            }
            if ((pl->frame > 37.7000008f && pl->frame < 38.2999992f) ||
                (pl->frame > 50.7000008f && pl->frame < 51.2999992f)) {
                SndCall(5, 3, &pl->pos, 0, 0, pl);
            }
        }
        if (pl->x3E4) {
            pl->rot.y += Muku(&pl->pos, &PL_EM(pl)->pos, pl->rot.y, 0.392699093f);
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        }
        if (MotionMoveF(pl, 0)) {
            pl->x3E0 = 0;
        }
        if (pl->x3E0) {
            pl->x3E0--;
        } else {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Escape camera: a fixed view behind the player, pulled in front of the nearest wall.
void em2cEscapeCamMove(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    Camera* cam = &pG->Cam;
    Vec pos;
    Vec at;
    Vec hit;
    Vec d;
    f32 len;

    w->cam.param.fovy = cam->param.fovy;
    pos.x = -376.0f;
    pos.y = 575.0f;
    pos.z = -1831.0f;
    at.x = -244.0f;
    at.y = 809.0f;
    at.z = 52.5999985f;
    PSMTXMultVec(pPLS->mat, &pos, &pos);
    PSMTXMultVec(pPLS->mat, &at, &at);
    PosToPos(&cam->param.at, &at, &w->cam.param.at, 1.0f);
    PosToPos(&cam->param.pos, &pos, &w->cam.param.pos, 1.0f);
    if (EatMgr.hitCheck(&w->cam.param.at, &w->cam.param.pos, &hit, 0, 0x8000, 0)) {
        PSVECSubtract(&hit, &w->cam.param.at, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 9125 "D:/Bio4/Prog/em2c.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&w->cam.param.at, &d, &w->cam.param.pos);
    }
    w->cam.up.x = 0.0f;
    w->cam.up.y = 1.0f;
    w->cam.up.z = 0.0f;
    w->cam.dist = SQRTF((w->cam.param.pos.x - w->cam.param.at.x) * (w->cam.param.pos.x - w->cam.param.at.x) +
                        (w->cam.param.pos.y - w->cam.param.at.y) * (w->cam.param.pos.y - w->cam.param.at.y) +
                        (w->cam.param.pos.z - w->cam.param.at.z) * (w->cam.param.pos.z - w->cam.param.at.z));
    CameraSetOrientationUp(&w->cam);
    CamCtrl.x250 = (s32) &w->cam;
}

static void em2cBackjumpAction(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    SetPlDamage((int) em, plemBackjump);
    w->x6B8 = 1;
    GameAddPoint(9);
}

static void plemBackjump(cPlayer* pl)
{
    int fe;

    pl->subArc = PL_EM(pl)->subArc;
    fe = pl->xFE;
    pl->dmg.x1 = 0x3C;
    switch (fe) {
    case 0:
        MotionSetCore(pl, &pl->mot, PL_ARC(0x85), 0, 5, 1, 5);
        EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, (void*) fe);
        SndCall(1, 0x43, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        SndCall(1, 0x44, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        pl->x3E8 = fe;
        GameAddPoint(0xB);
        pl->x3E0 = 35;
        pl->x3E4 = fe;
        pl->xFE++;
    case 1:
        if (pl->x3E0) {
            pl->x3E0--;
        } else if (Key.on & 0x1F) {
            pl->x3E4 = 1;
        }
        if (pl->frame > 20.7000008f && pl->frame < 21.2999992f) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if (pl->frame > 33.7000008f && pl->frame < 34.2999992f) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMoveF(pl, 0) || pl->x3E4) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void em2cBackjumpAction2(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    SetPlDamage((int) em, plemBackjump2);
    w->x6B8 = 1;
    GameAddPoint(9);
}

static void plemBackjump2(cPlayer* pl)
{
    int fe;

    pl->subArc = PL_EM(pl)->subArc;
    fe = pl->xFE;
    switch (fe) {
    case 0:
        MotionSetCore(pl, &pl->mot, PL_ARC(0x85), 0, 5, 1, 5);
        SndCall(1, 0x43, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        SndCall(1, 0x44, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, (void*) fe);
        pl->x3E8 = fe;
        GameAddPoint(0xB);
        pl->dmg.x1 = 0x14;
        pl->x3E0 = 35;
        pl->x3E4 = fe;
        pl->xFE++;
    case 1:
        if (pl->x3E0) {
            pl->x3E0--;
        } else if (Key.on & 0x1F) {
            pl->x3E4 = 1;
        }
        if (pl->frame > 20.7000008f && pl->frame < 21.2999992f) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if (pl->frame > 33.7000008f && pl->frame < 34.2999992f) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMoveF(pl, 0) || pl->x3E4) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Two-motion blend: m0 on the main work, m1 or m2 (by the sign of blendVal) on the blend work,
// weighted by |blendVal| / 256.
void em2cBlendMotSet(cEm2c* em, void* m0, void* m1, void* m2, int a, int b, int c, u16 d)
{
    Em2cWork* w = EM2C_WK(em);
    MotionWork* bm;
    int dd;
    int aa;
    asm("" : "=r"(aa) : "0"(a));            // COMPILER-DIFF: #2 (order only: gives a's copy the same chain length as d's so `mr r0,r7` keeps its place)
    asm("" : "=r"(dd) : "0"((int) d));     // COMPILER-DIFF: #2: the original masks the u16 `d` at each MotionSetCore call (`clrlwi r8,r25,16`)
    f32 val = fabsf(w->blendVal);
    void* m;
    int arg;

    MotionSetCore(em, &em->mot, m0, aa, (u8) w->blendCnt, (u16) dd, (u16) w->blendSeq);
    if (w->blendVal > 0.0f) {
        m = m1;
        arg = b;
    } else {
        m = m2;
        arg = c;
    }
    bm = EM2C_BLEND_MOT(w);
    MotionSetCore(em, bm, m, arg, (u8) w->blendCnt, (u16) dd, (u16) w->blendSeq);
    em->motBlend = bm;
    bm->blendRate = val * 0.00390625f;
    if (w->blendCnt) {
        w->blendCnt--;
    }
    w->blendSeq++;
    if ((u32) w->blendSeq >= em->frameMax) {
        w->blendSeq = 0;
    }
}

void em2cBlendMotSet2(cEm2c* em, void* m0, void* m1, int a, int b, u16 d)
{
    Em2cWork* w = EM2C_WK(em);
    MotionWork* bm;
    int dd;
    dd = (int) d;
    f32 val = fabsf(w->blendVal);

    MotionSetCore(em, &em->mot, m0, a, (u8) w->blendCnt, (u16) dd, (u16) w->blendSeq);
    bm = EM2C_BLEND_MOT(w);
    MotionSetCore(em, bm, m1, b, (u8) w->blendCnt, (u16) dd, (u16) w->blendSeq);
    em->motBlend = bm;
    bm->blendRate = val * 0.00390625f;
    if (w->blendCnt) {
        w->blendCnt--;
    }
    w->blendSeq++;
    if ((u32) w->blendSeq >= em->frameMax) {
        w->blendSeq = 0;
    }
}

void em2cTexrenderInit(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u8* tbl = w->texBlend;
    TexRenderMng* tex;
    int zero;

    tex = Ctrl12GetTexRenderEm2c(w->pCtrl12);
    w->pTex = tex;
    if (tex == 0) {
        pLog->err(0, 0, "em2cTexrenderInit:: Manager alloc failed!!");
        return;
    }
    tbl[0] = 1;
    tbl[1] = 0;
    tbl[4] = 0xF7;
    tbl[5] = w->pTex->texId;
    w->pTex->repType = 1;
    w->pTex->sy = w->pTex->sx = 0x40;
    zero = 0;
    EstSet(0, -1, 0, 0, 0x24, 0, w->pTex->mask | 0x801, 0, zero, (void*) zero);
}

void em2cSetFreeze(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    void* tbl = w->texBlend;

    w->flags |= 0x800;
    w->guardCnt = 900;
    pGS->flags_174 |= 0x80000000;
    if (w->pTex) {
        em->pInfo->setTexBlendTbl(tbl);
        em->pInfo->setBlendRatio(0xFF);
        em->pInfo->setBlendType(2);
    }
}

int em2cNoWallCk(cEm2c* em)
{
    EmiData* emi = (EmiData*) pG->pRoomEmi;
    u32 i;

    if (emi == 0) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type == 0x10 &&
            !((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
              2250000.0f)) {
            return 1;
        }
    }
    return 0;
}

void em2cGetTail(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u32 i;

    if (w->pTail) {
        return;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x2C) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (e->type != 1) {
            continue;
        }
        w->pTail = e;
        return;
    }
}

int em2cDoorCk(cEm2c* em)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x41 && e->id != 0x4E) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
            4000000.0f) {
            continue;
        }
        return 1;
    }
    return 0;
}

// Floor areas the enemy must not fall into (position, facing and half width per area).
static Vec em2c_floor_pos[3] = {
    { -21390.0f, 886.0f, -8534.0f },
    { -18407.0f, 886.0f, -31718.0f },
    { -11984.0f, 886.0f, -74395.0f },
};
static f32 em2c_floor_rot[3] = { 1.17999995f, -1.47000003f, 2.45000005f };
static f32 em2c_floor_w[3] = { 5000.0f, 5000.0f, 10000.0f };

int em2cFloorTypeCk(cEm2c* em)
{
    Mtx m;
    Vec v;
    u32 i;

    for (i = 0; i < 3; i++) {
        PSMTXRotRad(m, 'y', em2c_floor_rot[i]);
        TransMatrix(m, &em2c_floor_pos[i]);
        PSMTXInverse(m, m);
        PSMTXMultVec(m, &em->pos, &v);
        if (v.x > -em2c_floor_w[i] && v.x < em2c_floor_w[i] && v.z > -1000.0f) {
            return 0;
        }
    }
    return 1;
}

// Ambush points (position and facing); the player must be 5..10 m in front of one.
static Vec em2c_ambush_pos[2] = {
    { -20544.0f, 886.0f, -31480.0f },
    { -19194.0f, 886.0f, -7630.0f },
};
static f32 em2c_ambush_rot[2] = { -3.03999996f, 2.75f };

int em2cAmbushCk(cEm2c* em)
{
    Mtx m;
    Vec v;
    u32 i;

    if (em->plDist2 < 25000000.0f) {
        return 0;
    }
    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.14159274f)) < 2.09439516f) {
        return 0;
    }
    for (i = 1; i < 2; i++) {
        PSMTXRotRad(m, 'y', em2c_ambush_rot[i]);
        TransMatrix(m, &em2c_ambush_pos[i]);
        PSMTXInverse(m, m);
        PSMTXMultVec(m, &pPL->pos, &v);
        if (v.z > 5000.0f && v.z < 10000.0f) {
            em->setPos(&em2c_ambush_pos[i]);
            em->oldPos = em->pos;
            em->rot.y = em2c_ambush_rot[i];
            EmRoutineSet(em, 1, 4, 0, 0);
            return 1;
        }
    }
    return 0;
}

// Ceiling fall position: the EMI type 0x12 point nearest the player (at least 1 m away), 4330 high.
void em2cGetFallPos(cEm2c* em)
{
    EmiData* emi;
    f32 best;
    f32 d;
    u32 i;

    em->pos = pPLS->pos;
    em->pos.y = 4330.0f;
    em->rot.y = pPLS->rot.y + 3.14159274f;
    em->rot.y = LIMIT_ANGLE(em->rot.y);
    emi = (EmiData*) pGS->pRoomEmi;
    if (emi == 0) {
        return;
    }
    best = 10000000000.0f;
    for (i = 0; i < ((EmiData*) pGS->pRoomEmi)->n; i++) {
        EmiEntry* e = &((EmiData*) pGS->pRoomEmi)->entry[i];

        if (e->type != 0x12) {
            continue;
        }
        d = (pPLS->pos.x - e->pos.x) * (pPLS->pos.x - e->pos.x) + (pPLS->pos.z - e->pos.z) * (pPLS->pos.z - e->pos.z);
        if (d < 1000000.0f) {
            continue;
        }
        if (d > best) {
            continue;
        }
        best = d;
        em->pos = e->pos;
        em->pos.y = 4330.0f;
    }
}

void em2cBreathSe(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);

    if (w->breathTimer) {
        w->breathTimer--;
        return;
    }
    w->breathTimer = 59;
    w->breathSnd = SndCall(8, 0x46, &em->pos, em->id, 0, em);
}

// Stops the breath sound on the motion sounds that voice the enemy.
void em2cBreathSeStopCk(cEm2c* em)
{
    Em2cWork* w = EM2C_WK(em);
    u32 no;

    if (em->seNo == 0) {
        return;
    }
    no = em->seNo - 1;
    switch (no) {
    case 4:
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x24:
    case 0x26:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x39:
    case 0x3A:
    case 0x3D:
    case 0x3E:
    case 0x3F:
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
        SndStop(w->breathSnd, 0);
        w->breathTimer = 2;
        break;
    }
}
