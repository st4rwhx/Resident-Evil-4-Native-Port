// em36 module (D:/Bio4/Prog/em36.cpp): the regenerating enemy. cModel::type 0/1 is the walking one,
// type 2/3 the spined one (em36_R1_D_*), the limbs are lost and grow back (em36LostParts /
// em36_R1_Regene*), the weak points are cObj00 objects (em36WeakInit).

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "em36.h"
#include "emhit.h"
#include "em_set.h"
#include "emdoor.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "obj00.h"
#include "obj16.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "quake.h"
#include "motion.h"
#include "route_ck.h"
#include "act_btn.h"
#include "item.h"
#include "game.h"
#include "snd.h"
#include "pad.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "eprintf.h"
#include "db_log.h"
#include "main_mem.h"

asm(".comm common_em36,52,4");

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// em_set.h declares EmSetDieCnt without arguments; this module passes the enemy.
void EmSetDieCntE(cEm* em) asm("EmSetDieCnt");
void EmReserveDropItem(cEm* em);
typedef void (*Em36Func)(cEm36*);

static void em36_R0_Init(cEm36* em);
static void em36_R0_Move(cEm36* em);
static void em36_R1_br_Dummy(cEm36* em);
static void em36_R1_R307Bed(cEm36* em);
static void em36_R1_R307Appear(cEm36* em);
static void em36_R1_R309Appear(cEm36* em);
static void em36_R1_R308Appear(cEm36* em);
static void em36_R1_R310Appear(cEm36* em);
static void em36_R1_Wait(cEm36* em);
static void em36_R1_Walk(cEm36* em);
static void em36_R1_Dash(cEm36* em);
static void em36_R1_Turn(cEm36* em);
static void em36_R1_JumpDown(cEm36* em);
static void em36_R1_FanceOver(cEm36* em);
static void em36_R1_Threat(cEm36* em);
static void em36_R1_Crash(cEm36* em);
static void em36_R1_Atk(cEm36* em);
static void em36_R1_SpineAtk(cEm36* em);
static void plem36_Stamp(cPlayer* pl);
static void subem36_Stamp();
static void em36_R1_br_Catch(cEm36* em);
static void em36_R1_Catch(cEm36* em);
static void em36_R1_CatchHit(cEm36* em);
static void plem36_CatchHit(cPlayer* pl);
static void em36_R1_br_LongCatch(cEm36* em);
static void em36_R1_LongCatch(cEm36* em);
static void em36_R1_LongCatchHit(cEm36* em);
static void plem36_LongCatchHit(cPlayer* pl);
static void em36_R1_SpineCatchHit(cEm36* em);
static void plem36_SpineCatchHit(cPlayer* pl);
static void em36_R1_br_LostCatch(cEm36* em);
static void em36_R1_LostCatch(cEm36* em);
static void em36_R1_RegeneArm(cEm36* em);
static void em36_R1_RegeneArm2(cEm36* em);
static void em36_R1_RegeneFoot(cEm36* em);
static void em36_R1_D_Wait(cEm36* em);
static void em36_R1_D_Walk(cEm36* em);
static void em36_R1_D_Turn(cEm36* em);
static void em36_R1_D_SpineAtk(cEm36* em);
static void em36_R1_br_D_Catch(cEm36* em);
static void em36_R1_D_Catch(cEm36* em);
static void em36_R1_D_CatchHit(cEm36* em);
static void plem36_D_CatchHit(cPlayer* pl);
static void em36_R1_Wakeup(cEm36* em);
static void em36_R0_Damage(cEm36* em);
static void em36_R1_Dm_Normal(cEm36* em);
static void em36_R1_Dm_Big(cEm36* em);
static void em36_R1_Dm_Weak(cEm36* em);
static void em36_R1_Dm_Down(cEm36* em);
static void em36_R1_Dm_DownWeak(cEm36* em);
static void em36_R1_Dm_DownJump(cEm36* em);
static void em36_R0_Die(cEm36* em);
static void em36_R1_Die_Normal(cEm36* em);
static void em36_R1_Die_Down(cEm36* em);

Em36Func Em36_R0_move_tbl[4] = {
    em36_R0_Init,
    em36_R0_Move,
    em36_R0_Damage,
    em36_R0_Die,
};

// Pairs of {branch check, routine} per routine-1 state (em36_R0_Move calls both).
static Em36Func Em36_R1_move_tbl[62] = {
    em36_R1_br_Dummy, em36_R1_Wait,
    em36_R1_br_Dummy, em36_R1_Walk,
    em36_R1_br_Dummy, em36_R1_Dash,
    em36_R1_br_Dummy, em36_R1_Turn,
    em36_R1_br_Dummy, em36_R1_JumpDown,
    em36_R1_br_Dummy, em36_R1_FanceOver,
    em36_R1_br_Dummy, em36_R1_Threat,
    em36_R1_br_Dummy, em36_R1_Crash,
    em36_R1_br_Dummy, em36_R1_Atk,
    em36_R1_br_Dummy, em36_R1_SpineAtk,
    em36_R1_br_Catch, em36_R1_Catch,
    em36_R1_br_Dummy, em36_R1_CatchHit,
    em36_R1_br_LongCatch, em36_R1_LongCatch,
    em36_R1_br_Dummy, em36_R1_LongCatchHit,
    em36_R1_br_Dummy, em36_R1_SpineCatchHit,
    em36_R1_br_LostCatch, em36_R1_LostCatch,
    em36_R1_br_Dummy, em36_R1_D_Wait,
    em36_R1_br_Dummy, em36_R1_D_Walk,
    em36_R1_br_Dummy, em36_R1_D_Turn,
    em36_R1_br_Dummy, em36_R1_D_SpineAtk,
    em36_R1_br_D_Catch, em36_R1_D_Catch,
    em36_R1_br_Dummy, em36_R1_D_CatchHit,
    em36_R1_br_Dummy, em36_R1_Wakeup,
    em36_R1_br_Dummy, em36_R1_RegeneArm,
    em36_R1_br_Dummy, em36_R1_RegeneArm2,
    em36_R1_br_Dummy, em36_R1_RegeneFoot,
    em36_R1_br_Dummy, em36_R1_R307Bed,
    em36_R1_br_Dummy, em36_R1_R307Appear,
    em36_R1_br_Dummy, em36_R1_R309Appear,
    em36_R1_br_Dummy, em36_R1_R308Appear,
    em36_R1_br_Dummy, em36_R1_R310Appear,
};

static Em36Func Em36_R2_move_tbl[6] = {
    em36_R1_Dm_Normal,
    em36_R1_Dm_Big,
    em36_R1_Dm_Weak,
    em36_R1_Dm_Down,
    em36_R1_Dm_DownWeak,
    em36_R1_Dm_DownJump,
};

static Em36Func Em36_R3_move_tbl[2] = {
    em36_R1_Die_Normal,
    em36_R1_Die_Down,
};

// The attack ranges (em36AtkCk2: 0 the stamp, 1 the spine).
static EmAtkInfo em36_atk_tbl[2] = {
    { 400.0f, 8, 0x030C, 0x0000, 0x000A, 0x0000 },
    { 1500.0f, 8, 0x030C, 0x0000, 0x000A, 0x0000 },
};

// The mirrored parts numbers (cModel::motFlip) of the flipped motions.
static u16 em36_flip[120] = {
    0x0, 0x1, 0x2, 0x3, 0x4, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0x11, 0x16, 0x17,
    0x18, 0x19, 0x12, 0x13, 0x14, 0x15, 0x1B, 0x1A, 0x1C, 0x1D, 0x1E, 0x1F, 0x23, 0x24, 0x25, 0x20, 0x21, 0x22, 0x26, 0x27,
    0x28, 0x29, 0x2A, 0x2C, 0x2B, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
    0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63,
    0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
};

// The weak points (cObj00) hang on these parts, at these offsets (em36WeakInit).
static u8 em36_weak_parts[5] = { 3, 2, 0x2D, 0, 2 };
static Vec em36_weak_pos[5] = {
    { -129.0f, -59.0f, 146.0f },
    { 121.0f, -30.0f, 234.0f },
    { -130.0f, 43.0f, 214.0f },
    { 138.0f, -65.0f, 190.0f },
    { 97.0f, -65.0f, -182.0f },
};
static Vec em36_weak_rot[5] = {
    { -0.47123888f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f },
    { 0.17453292f, 0.0f, 0.0f },
    { 0.17453292f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f },
};

#define ARC(no) PL_ARC_PTR(em->subArc, no)
#define PL_ARC(no) PL_ARC_PTR(pl->subArc, no)

// The enemy a player damage callback belongs to (pl_sub SetPlDamage's first argument).
#define PL_EM(pl) ((cEm36*) (pl)->dmgType)
#define PL_EM_G ((cEm36*) pPL->dmgType)

#define VIB_TBL ((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc))

// Struct-member view of the player pointer (cam_ctrl.cpp PlayerPtr).
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)
#define pSUBS (((PlayerPtr*) &pSUB)->p)

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

static inline void U32Set(u32& d, u32 v) { d = v; }
static inline void IntSet(int& d, int v) { d = v; }

// Flag update through a volatile view: keeps the following global load below the sth (wep_mod.h).
static inline void AtariFlagsOr(cAtariInfo* at, u16 mask) { *(volatile u16*) &at->flags |= mask; }
static inline void AtariFlagsAndV(cAtariInfo* at, u16 mask) { *(volatile u16*) &at->flags &= mask; }

static inline int em36DeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

// The appearance effects (type 0/1: two, type 2/3: one).
static inline void em36AppearEsp(cEm36* em, Em36Work* w)
{
    switch (em->type) {
    case 0:
    case 1:
    default: {
        int zero = 0;

        EstSet((int) em, -1, 0, 0, 0x2D, 8, 1, w->espKind[1], (u32) em, (void*) zero);
        EstSet((int) em, -1, 0, 0, 0x2D, 9, 1, w->espKind[2], (u32) em, (void*) zero);
        break;
    }
    case 2:
    case 3:
        EstSet((int) em, -1, 0, 0, 0x2D, 9, 1, w->espKind[2], (u32) em, 0);
        break;
    }
}

extern "C" void _prolog()
{
    OSReport("em36 prolog Ok\n");
    EmInitFunc = Em36Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em36Init(cEm* em)
{
    new (em) cEm36();
}

// A lost limb whose hit box took the damage (em36DmCk): the routine is set straight from the loop (one
// stepping limb pointer, the hit path jumps into the RS arm); an int inline returns through a flag.
#define EM36_LIMB_HIT_CK(no)                                                                       \
    if (pG->flags_5010 & 0x04000000) {                                                             \
        int i;                                                                                     \
                                                                                                   \
        for (i = 0; i < 5; i++) {                                                                  \
            Em36Limb* l = &w->limb[i];                                                             \
                                                                                                   \
            if (l->pObj != 0 && part == &w->hit[l->hit]) {                                         \
                EmRoutineSet(em, 2, no, 0, 0);                                                     \
                return;                                                                            \
            }                                                                                      \
        }                                                                                          \
    }

void em36DmCk(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    int near;
    int dmg;
    EmHitInfo* part;

    if (em36CrashCk(em)) {
        return;
    }
    if (em->hp > 0 && em36DeadCk(em) == 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 4:
        case 5:
        case 7:
            if (w->dieTimer == 0) {
                w->dieTimer = 120;
                w->flags |= 0x200;
                LifeDownSet2(em, 1000, 0, 0);
                if (em->hp <= 0) {
                    EmSetDie(em);
                    EmReserveDropItem(em);
                    EmSetDieCntE(em);
                    if (w->flags & 0x100) {
                        EmRoutineSet(em, 2, 5, 0, 0);
                    } else if (w->flags & 0x20) {
                        EmRoutineSet(em, 3, 1, 0, 0);
                    } else {
                        EmRoutineSet(em, 3, 0, 0, 0);
                    }
                    return;
                }
                if (w->flags & 0x40) {
                    return;
                }
                if (w->flags & 0x100) {
                    EmRoutineSet(em, 2, 5, 0, 0);
                } else if (w->flags & 0x20) {
                    EmRoutineSet(em, 2, 3, 0, 0);
                } else {
                    EmRoutineSet(em, 2, 1, 0, 0);
                }
                return;
            }
            break;
        }
    }
    if (em->dmHit == 0) {
        return;
    }
    em->dmHit = 0;
    em->dmType = 1;
    if (em->dmWep == 0x10) {
        em->dmType = 0x11;
    }
    w->flags |= 0x200;
    near = 0;
    part = em->dmPart;
    if (part->rad < 36000000.0f) {
        near = 1;
    }
    dmg = em36SetDmVal(em);
    LifeDownSet2(em, dmg, 0, 0);
    SndCall(8, 0x1C, &em->pos, em->id, 0, em);
    if (em->dmWep == 0xD || em->dmWep == 0x12) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        EmSetDie(em);
        EmReserveDropItem(em);
        EmSetDieCntE(em);
        if (w->flags & 0x100) {
            EmRoutineSet(em, 2, 5, 0, 0);
        } else if (w->flags & 0x20) {
            EmRoutineSet(em, 3, 1, 0, 0);
        } else {
            EmRoutineSet(em, 3, 0, 0, 0);
        }
        return;
    }
    if (w->flags & 0x40) {
        em36BloodSet(em);
        return;
    }
    if (w->flags & 0x100) {
        switch (em->dmWep) {
        case 7:
        case 8:
        case 0x21:
            if (near == 0) {
                break;
            }
        case 5:
        case 6:
        case 9:
        case 0xA:
        case 0xF:
        case 0x28:
        case 0x2C:
            if (em36LostParts(em) == 0) {
                em36BloodSet(em);
            }
            break;
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 0xB:
        case 0xC:
        case 0x10:
        case 0x11:
        case 0x14:
        case 0x15:
        case 0x1B:
        case 0x1D:
        case 0x26:
        case 0x27:
        case 0x29:
        case 0x2B:
        case 0x2D:
            em36BloodSet(em);
            break;
        case 0xD:
        case 0x12:
        case 0x13:
            em36BloodSet(em);
            break;
        case 0xE:
            em36BloodSet(em);
        }
        EmRoutineSet(em, 2, 5, 0, 0);
        return;
    }
    if (w->flags & 0x20) {
        switch (em->dmWep) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 0xB:
        case 0xC:
        case 0x10:
        case 0x11:
        case 0x14:
        case 0x15:
        case 0x1B:
        case 0x1D:
        case 0x26:
        case 0x27:
        case 0x2B:
            if ((u8) (Rnd() % 10) != 5) {
                goto blood;
            }
            goto lost;
        case 7:
        case 8:
        case 0x21:
            if (near == 0) {
                break;
            }
        case 5:
        case 6:
        case 0xF:
        case 0x2C:
        lost:
            if (em36LostParts(em) == 0) {
            blood:
                em36BloodSet(em);
            }
            break;
        case 0xD:
        case 0x12:
        case 0x13:
        case 0x29:
        case 0x2D:
            em36BloodSet(em);
            break;
        case 0xE:
            em36BloodSet(em);
            break;
        case 9:
        case 0xA:
        case 0x28:
            EM36_LIMB_HIT_CK(4);
            if (em36LostParts(em) == 0) {
                em36BloodSet(em);
            }
            break;
        }
        EmRoutineSet(em, 2, 3, 0, 0);
        return;
    }
    switch (em->dmWep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        if ((u8) (Rnd() % 10) == 5) {
            if (em36LostParts(em)) {
                EmRoutineSet(em, 2, 0, 0, 0);
                return;
            }
        }
        em36BloodSet(em);
        return;
    case 7:
    case 8:
    case 0x21:
        if (near == 0) {
            return;
        }
        goto lost2;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x17:
    case 0x29:
    case 0x2A:
    case 0x2D:
        em36BloodSet(em);
        EmRoutineSet(em, 2, 1, 0, 0);
        return;
    case 9:
    case 0xA:
    case 0x28:
        EM36_LIMB_HIT_CK(2);
    case 5:
    case 6:
    case 0xF:
    case 0x1C:
    case 0x2C:
    lost2:
        if (em36LostParts(em) == 0) {
            em36BloodSet(em);
        }
        EmRoutineSet(em, 2, 0, 0, 0);
        return;
    case 0xE:
        em36BloodSet(em);
        return;
    }
}

void cEm36::move()
{
    Em36Work* w = EM36_WK(this);
    f32 d;
    u16 atFlags;
    int i;

    if (xFC != 0) {
        em36DmCk(this);
    }
    w->flags &= 0xFFD40220;
    if (w->wait) {
        w->wait--;
    }
    if (w->dieTimer) {
        w->dieTimer--;
    }
    for (i = 0; i < 7; i++) {
        if (w->atkTimer[i]) {
            w->atkTimer[i]--;
        }
    }
    w->frameCnt++;
    clearStatus(3);
    if (hp > 0) {
        hp++;
        if (hp > hpMax) {
            hp = hpMax;
        }
    }
    em36RouteCk(this);
    if (w->flags & 1) {
        if (w->findWait) {
            w->findWait--;
        }
        if (w->seWait) {
            w->seWait--;
        }
    }
    Em36_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    if (seFlags28B & 0x80) {
        w->flags |= 0xA0;
        setStatus(3);
    }
    em36NeckMove(this);
    em36SlopeMove(this);
    partsWorldCalc();
    em36ScaleCompress(this);
    d = SQRTF((oldPos.x - pos.x) * (oldPos.x - pos.x) + (oldPos.z - pos.z) * (oldPos.z - pos.z));
    if (seFlags28B & 0x20) {
        atari.flags |= 0x10;
    } else {
        atari.flags &= ~0x10;
    }
    atFlags = atari.flags;
    if (seFlags28B & 0x20) {
        atari.flags &= ~0x100;
    }
    EmAtCheck(this);
    atari.move();
    if (w->flags & 0x400) {
        SatMgr.checkAir(this, 0x1C2810);
    } else {
        SatMgr.check(this, 0);
    }
    atari.flags = atFlags;
    if (SQRTF((pos.x - oldPos.x) * (pos.x - oldPos.x) + (pos.z - oldPos.z) * (pos.z - oldPos.z)) < d * 0.5f) {
        w->stuckCnt++;
    } else {
        w->stuckCnt = 0;
    }
    em36YarareCk(this);
    em36WeakMove(this);
    em36RegeneCk2(this);
    em36RegeneTenMove(this);
    em36BreathSeStopCk(this);
    em36SpineScaleMove(this);
}

static void em36_R0_Init(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    int i;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(0x13)) == 0) {
            pLog->err(0, 0, "em36() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    case 1:
        if (em->modelInit(ARC(4), ARC(0x14)) == 0) {
            pLog->err(0, 0, "em36() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    case 2:
    case 3:
        if (em->modelInit(ARC(0x15), ARC(0x24)) == 0) {
            pLog->err(0, 0, "em36() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    }
    em36PartsSet(em, 0, 0);
    em36PartsSet(em, 1, 0);
    em36PartsSet(em, 2, 0);
    em36PartsSet(em, 3, 0);
    em36PartsSet(em, 4, 0);
    em36PartsSet(em, 5, 0);
    em36PartsSet(em, 6, 0);
    em->motFlip = em36_flip;
    EspDataLoad((u32) ARC(0x25), 0x2D, 0);
#line 966 "D:/Bio4/Prog/em36.cpp"
    em->p2A4 = MEM_ALLOC(0x98, 1, 0xD);
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->atari.init(1, 0x2000, 10, 0.0f, 0.0f, 0.0f, 400.0f, 300.0f, 300.0f, 1000.0f);
    em->litArea.on(1);
    YarareInit(em, 0.0f, 30.0f, 40.0f, 190.0f, 50.0f, 2, 1);
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 100.0f, 0.0f, 1, 0);
    YarareAdd(em, &w->hit[1], 0.0f, 0.0f, 0.0f, 100.0f, 0.0f, 1, 0);
    YarareAdd(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 100.0f, 0.0f, 1, 0);
    YarareAdd(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 100.0f, 0.0f, 1, 0);
    YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 100.0f, 0.0f, 1, 0);
    YarareAdd(em, &w->hit[5], 35.0f, -150.0f, 70.0f, 180.0f, 50.0f, 3, 1);
    YarareAdd(em, &w->hit[6], -35.0f, -150.0f, 70.0f, 180.0f, 50.0f, 3, 1);
    YarareAdd(em, &w->hit[7], 50.0f, 0.0f, 0.0f, 180.0f, 0.0f, 3, 1);
    YarareAdd(em, &w->hit[8], -50.0f, 0.0f, 0.0f, 180.0f, 0.0f, 3, 1);
    YarareAdd(em, &w->hit[9], 70.0f, 125.0f, -60.0f, 130.0f, 0.0f, 3, 1);
    YarareAdd(em, &w->hit[10], -70.0f, 125.0f, -60.0f, 130.0f, 0.0f, 3, 1);
    YarareAdd(em, &w->hit[11], 50.0f, -100.0f, 0.0f, 180.0f, 0.0f, 0x12, 1);
    YarareAdd(em, &w->hit[12], -50.0f, -100.0f, 0.0f, 180.0f, 0.0f, 0x12, 1);
    YarareAdd(em, &w->hit[13], 0.0f, 0.0f, 0.0f, 95.0f, 100.0f, 4, 1);
    YarareAdd(em, &w->hit[14], 0.0f, 10.0f, 30.0f, 110.0f, 80.0f, 5, 1);
    YarareAdd(em, &w->hit[15], 0.0f, 50.0f, -20.0f, 110.0f, 35.0f, 5, 1);
    YarareAdd(em, &w->hit[16], -10.0f, -300.0f, 20.0f, 130.0f, 200.0f, 0x13, 1);
    YarareAdd(em, &w->hit[17], 0.0f, -500.0f, 0.0f, 85.0f, 550.0f, 0x14, 1);
    YarareAdd(em, &w->hit[18], 10.0f, -300.0f, 20.0f, 130.0f, 200.0f, 0x17, 1);
    YarareAdd(em, &w->hit[19], 0.0f, -500.0f, 0.0f, 85.0f, 550.0f, 0x18, 1);
    YarareAdd(em, &w->hit[20], -80.0f, 0.0f, 0.0f, 95.0f, 80.0f, 7, 3);
    YarareAdd(em, &w->hit[21], -110.0f, 5.0f, 0.0f, 80.0f, 130.0f, 8, 3);
    YarareAdd(em, &w->hit[22], -230.0f, 5.0f, 5.0f, 78.0f, 200.0f, 9, 3);
    YarareAdd(em, &w->hit[23], -200.0f, 10.0f, 10.0f, 60.0f, 150.0f, 0xA, 3);
    YarareAdd(em, &w->hit[24], -50.0f, -10.0f, 15.0f, 80.0f, 0.0f, 0xB, 3);
    YarareAdd(em, &w->hit[25], 0.0f, 0.0f, 0.0f, 95.0f, 80.0f, 0xD, 3);
    YarareAdd(em, &w->hit[26], -20.0f, 5.0f, 0.0f, 80.0f, 130.0f, 0xE, 3);
    YarareAdd(em, &w->hit[27], 30.0f, 5.0f, 5.0f, 78.0f, 200.0f, 0xF, 3);
    YarareAdd(em, &w->hit[28], 50.0f, 10.0f, 10.0f, 60.0f, 150.0f, 0x10, 3);
    YarareAdd(em, &w->hit[29], 50.0f, -10.0f, 15.0f, 80.0f, 0.0f, 0x11, 3);
    em36WeakInit(em);
    em->lockParts = 2;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    w->x780 = 0.0f;
    // Store order read off the target: espKind[3] before [2] (0x44 takes r9, 0x43 r0), flags before wait
    // (the dying zero store is issued first), spine cleared through a stepping pointer (ascending loop).
    w->espKind[0] = 0x41;
    w->espKind[1] = 0x42;
    w->espKind[3] = 0x44;
    w->espKind[2] = 0x43;
    w->flags = 0;
    w->wait = 0;
    w->findWait = (u8) (Rnd() % 150) + 150;
    w->seWait = (int) Rnd() % 600 + 450;
    w->scale = 1.0f;
    {
        f32* sp = w->spine;

        for (i = 0; i < 14; i++) {
            *sp++ = 0.0f;
        }
    }
    for (i = 0; i < 7; i++) {
        w->ten[i].obj[0] = 0;
        w->ten[i].obj[1] = 0;
        w->ten[i].obj[2] = 0;
    }
    switch (em->x38D) {
    case 0:
    default:
        // Plain byte stores (QImode zero): MotionSetCore's 0 argument gets its own `li r9, 0`.
        em36AppearEsp(em, w);
        em->setStatus(5);
        em->xFC = 1;
        em->xFD = 0;
        em->xFE = 0;
        em->xFF = 0;
        MotionSetCore(em, MOTION(em), ARC(0x26), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        break;
    case 1:
        em->xFC = 1;
        em->xFD = 0x1A;
        em->xFE = 0;
        em->xFF = 0;
        MotionSetCore(em, MOTION(em), ARC(0x26), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        break;
    case 2:
        em36AppearEsp(em, w);
        EmRoutineSet(em, 1, 0x1C, 0, 0);
        em->setStatus(5);
        MotionSetCore(em, MOTION(em), ARC(0x26), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        break;
    case 3:
        EmRoutineSet(em, 1, 0x1D, 0, 0);
        em->clearStatus(5);
        MotionSetCore(em, MOTION(em), ARC(0x97), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        break;
    case 4:
        EmRoutineSet(em, 1, 0x1E, 0, 0);
        em->clearStatus(5);
        MotionSetCore(em, MOTION(em), ARC(0x9B), 0, 0, 0x100, 0);
        MotionMoveF(em, 0);
        break;
    }
    em36_R0_Move(em);
    OSReport("em36 free size = 0x%x\n", 0x7E4);
}

static void em36_R0_Move(cEm36* em)
{
    Em36_R1_move_tbl[em->xFD * 2](em);
    Em36_R1_move_tbl[em->xFD * 2 + 1](em);
}

static void em36_R1_br_Dummy(cEm36* em)
{
}

static void em36_R1_R307Bed(cEm36* em)
{
    em->dmType = 2;
    switch (em->xFE) {
    case 0: {
        cAtariInfo* at;

        MotionSetCore(em, MOTION(em), ARC(0x88), 0, 0, 5, 0);
        em->pos.x = -8906.54f;
        em->pos.y = 1005.07f;
        em->pos.z = -4882.06f;
        em->rot.y = 1.8358682f;
        at = &em->atari;
        at->throughOn();
        em->xFE++;
    }
    case 1:
        MotionMoveF(em, 0);
        break;
    }
}

static void em36_R1_R307Appear(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    em->dmType = 2;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x26), 0, 0, 5, 0);
        em36AppearEsp(em, w);
        em->pos.x = -6050.54f;
        em->pos.y = 0.0f;
        em->pos.z = -3660.0f;
        em->rot.y = 1.573289f;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            cAtariInfo* at;

            em->flags_3C8 &= ~1;
            w->flags |= 0x200;
            em->setStatus(5);
            EmRoutineSet(em, 1, 1, 0, 0);
            at = &em->atari;
            at->throughOff();
        }
        break;
    }
}

static void em36_R1_R309Appear(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    em->dmType = 2;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x26), 0, 0, 5, 0);
        em->setStatus(5);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            em->flags_3C8 &= ~1;
            w->seWait = 300;
            w->flags |= 0x200;
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
}

static void em36_R1_R308Appear(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    em->dmType = 2;
    switch (em->xFE) {
    case 0:
        em->pos.x = -1767.99f;
        em->pos.y = 0.0f;
        em->pos.z = -4187.12f;
        em->rot.y = 0.0f;
        MotionSetCore(em, MOTION(em), ARC(0x97), (int) ARC(0x98), 0, 5, 0);
        em->hp = 0;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            em->flags_3C8 &= ~1;
            em->hp = em->hpMax;
            em->setStatus(5);
            w->flags |= 0x200;
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x99), (int) ARC(0x9A), 0, 1, 0);
        em36AppearEsp(em, w);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if (w->flags2 & 0xC) {
                EmRoutineSet(em, 1, 0x10, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x16, 0, 0);
            }
        }
        break;
    }
}

static void em36_R1_R310Appear(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    switch (em->xFE) {
    case 0: {
        cAtariInfo* at;

        em->pos.x = -7148.93f;
        em->pos.y = 899.44f;
        em->pos.z = -29680.91f;
        em->rot.y = PI;
        em->hp = 0;
        at = &em->atari;
        at->throughOn();
        em->xFE++;
    }
    case 1:
        MotionSetCore(em, MOTION(em), ARC(0x9B), (int) ARC(0x9C), 0, 0x100, 0);
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            cAtariInfo* at;

            em->flags_3C8 &= ~1;
            at = &em->atari;
            at->throughOff();
            em->hp = em->hpMax;
            em->setStatus(5);
            w->flags |= 0x200;
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x9B), (int) ARC(0x9C), 0, 1, 0);
        em36AppearEsp(em, w);
        w->timer = 98;
        em->xFE++;
    case 3:
        if (em->seFlags28B & 4) {
            w->flags |= 0x40;
        }
        if (w->timer) {
            w->timer--;
        } else {
            w->flags |= 0x2000;
        }
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.3561945f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
            if (em->plDist2 > 9000000.0f) {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

static void em36_R1_Wait(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x26), 0, 0x1E, 5, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if ((s16) pG->pl_life > 0) {
            em36FindCk(em);
            if ((w->flags & 0x200) && w->wait == 0) {
                if (w->targetAngAbs > 2.3561945f) {
                    EmRoutineSet(em, 1, 3, 0, 0);
                }
                if (em->plDist2 > 9000000.0f) {
                    EmRoutineSet(em, 1, 1, 0, 0);
                }
            }
        }
        break;
    }
    if (em36RegeneCk(em) == 0) {
        em36BreathSe(em);
    }
}

// The tail every walking routine shares: routine 0 without the player, else the door / jump / fence checks.
static inline void em36WalkTail(cEm36* em)
{
    if (em36RegeneCk(em) == 0) {
        if ((s16) pG->pl_life <= 0) {
            EmRoutineSet(em, 1, 0, 0, 0);
        } else {
            em36DoorOpenCk(em);
            if (em36JumpDownCk(em) == 0 && em36FanceOverCk(em) == 0) {
                em36BreathSe(em);
            }
        }
    }
}

static void em36_R1_Walk(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        switch (em->type) {
        case 0:
        case 1:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x27), (int) ARC(0x28), 10, 5, 0);
            break;
        case 2:
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x2D), (int) ARC(0x2E), 10, 5, 0);
            break;
        }
        w->timer = (u8) (Rnd() % 6) + 6;
        w->timer2 = 0;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0) && (w->flags & 1)) {
            if (w->timer == 0) {
                w->wait = 30;
                EmRoutineSet(em, 1, 6, 0, 0);
                break;
            }
            w->timer--;
        }
        if (em36AtkRtnCk(em)) {
            break;
        }
        if (w->targetAngAbs > 2.3561945f) {
            EmRoutineSet(em, 1, 3, 0, 0);
        }
        if ((u32) em->type <= 1 && !(w->flags & 4) && w->seWait == 0 && (w->flags & 1) && em->plDist2 > 25000000.0f &&
            em->plDist2 < 100000000.0f) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    if (em36RegeneCk(em) == 0) {
        if ((s16) pG->pl_life <= 0) {
            EmRoutineSet(em, 1, 0, 0, 0);
        } else {
            em36DoorOpenCk(em);
            if (em36JumpDownCk(em) == 0 && em36FanceOverCk(em) == 0) {
                em36BreathSe(em);
                switch (em->type) {
                case 2:
                case 3:
                    if (w->timer2) {
                        w->timer2--;
                    } else {
                        w->timer2 = 59;
                        SndCall(8, 0x2E, &em->pos, em->id, 0, em);
                    }
                    break;
                case 0:
                case 1:
                default:
                    break;
                }
            }
        }
    }
}

static void em36_R1_Dash(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        switch (em->type) {
        case 0:
        case 1:
        default:
            w->seWait = (int) Rnd() % 450 + 450;
            MotionSetCore(em, MOTION(em), ARC(0x29), (int) ARC(0x2A), 10, 5, 0);
            break;
        case 2:
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x2F), (int) ARC(0x30), 10, 5, 0);
            break;
        }
        w->timer = (u8) (Rnd() % 3) + 4;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0) && (w->flags & 1)) {
            if (w->timer == 0) {
                w->wait = 30;
                EmRoutineSet(em, 1, 6, 0, 0);
                break;
            }
            w->timer--;
        }
        if (em36AtkRtnCk(em)) {
            break;
        }
        if (w->targetAngAbs > 2.3561945f) {
            EmRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
    em36WalkTail(em);
}

static void em36_R1_Turn(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x2B), (int) ARC(0x2C), 3, 1, 0);
        w->turnAng = em->rot.y + PI;
        w->turnAng = LIMIT_ANGLE(w->turnAng);
        em->xFE++;
    case 1: {
        f32 d;

        d = Muku(&em->pos, &w->targetPos, w->turnAng, 0.09817477f);
        w->turnAng += d;
        w->turnAng = LIMIT_ANGLE(w->turnAng);
        em->rot.y += d;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            if ((u32) em->type <= 1 && !(w->flags & 4) && w->seWait == 0 && (w->flags & 1) &&
                em->plDist2 > 25000000.0f && em->plDist2 < 100000000.0f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else {
            Vec v;

            if (w->flags2 & 0x10) {
                v = pPL->pos;
            } else {
                GetPlPos(&v, 0, 18.0f);
            }
            fabsf(em->pos.y - pPL->pos.y);
            em36AtkRtnCk(em);
        }
        break;
    }
    }
    em36BreathSe(em);
}

static void em36_R1_JumpDown(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    u8 step = em->xFE;

    switch (step) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x90), (int) ARC(0x91), 3, 1, 0);
        w->x014 = 0;
        em->xFE++;
    case 1: {
        int end;

        em->rot.y += Muku2(em->rot.y, w->jumpAng, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        w->flags |= 0x440;
        em->setStatus(3);
        end = MotionMoveF(em, 0);
        if (!(em->seFlags28B & 0x20)) {
            Vec v;
            f32 fl;

            v = em->pos;
            v.y = em->oldPos.y;
            fl = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
            if (em->pos.y < fl) {
                em->pos.y = fl;
                MotionSetCore(em, MOTION(em), ARC(0x92), 0, 3, 1, 0);
                MotionMoveF(em, 0);
                em->xFE = 2;
            } else if (end) {
                MotionSetCore(em, MOTION(em), ARC(0x92), 0, 3, 1, 0);
                em->xFE = 2;
            }
        }
        break;
    }
    case 2:
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 0, 0, 0);
            } else {
                if (w->targetAngAbs > 2.3561945f) {
                    EmRoutineSet(em, 1, 3, 0, 0);
                }
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
    em36BreathSe(em);
}

static void em36_R1_FanceOver(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x40;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x33), (int) ARC(0x34), 3, 1, 0);
        PSVECSubtract(&w->fanceVec, &em->pos, &w->fanceVec);
        w->fanceVec.y = 0.0f;
        em->xFE++;
    case 1: {
        Mtx m;
        Vec* d = &w->fanceVec;
        Vec v;

        PSMTXRotRad(m, 'y', em->rot.y);
        PSMTXMultVecSR(m, d, d);
        PSVECScale(d, &v, 0.2f);
        PSVECAdd(&em->pos, &v, &em->pos);
        PSVECSubtract(d, &v, d);
        if (MotionMoveF(em, 0)) {
            if (w->flags2 & 0xC) {
                EmRoutineSet(em, 1, 0x10, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x16, 0, 0);
            }
        }
        break;
    }
    }
    em36BreathSe(em);
}

static void em36_R1_Threat(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x31), (int) ARC(0x32), 10, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 0, 0, 0);
            } else {
                if (w->targetAngAbs > 2.3561945f) {
                    EmRoutineSet(em, 1, 3, 0, 0);
                }
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else if (em->seFlags28B & 1) {
            w->flags |= 0x2000;
        }
        break;
    }
}

static void em36_R1_Crash(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0: {
        int flip;

        if ((u8) (Rnd() % 10) > 4) {
            flip = 0x41;
        } else {
            flip = 1;
        }
        if (em->xFF == 0) {
            MotionSetCore(em, MOTION(em), ARC(0x95), (int) ARC(0x96), 5, flip, 0);
            w->timer = 20;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x59), (int) ARC(0x5A), 5, flip, 0);
        }
        em36VoiceSet(em, 0x38, 2);
        em->xFE++;
    }
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
}

// The attack target: the partner when it is the target, the player otherwise, at its feet when the
// flag says so.
#define em36AtkTarget(em, w, v) \
    if (((w)->flags & 4) && pSUB) { \
        if ((w)->flags2 & 0x10) { \
            v = pSUB->pos; \
        } else { \
            GetPlPos(&v, pSUB, 18.0f); \
        } \
    } else { \
        if ((w)->flags2 & 0x10) { \
            v = pPL->pos; \
        } else { \
            GetPlPos(&v, 0, 18.0f); \
        } \
    }

static void em36_R1_Atk(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0: {
        Vec v;
        int flip = 0x41;

        if (!(w->flags2 & 1)) {
            flip = 1;
        }
        if ((w->flags & 4) && pSUB) {
            if (w->flags2 & 0x10) {
                v = pSUB->pos;
            } else {
                GetPlPos(&v, pSUB, 18.0f);
            }
        } else {
            if (w->flags2 & 0x10) {
                v = pPL->pos;
            } else {
                GetPlPos(&v, 0, 18.0f);
            }
        }
        if (fabsf(Muku(&em->pos, &v, em->rot.y, PI)) < 1.5707964f) {
            MotionSetCore(em, MOTION(em), ARC(0x44), (int) ARC(0x45), 5, flip, 0);
            w->timer = 20;
            if (pG->x4F88 <= 2) {
                w->timer = 5;
            }
            if (pG->x4F88 > 7) {
                w->timer = 30;
            }
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x46), (int) ARC(0x47), 5, flip, 0);
            w->timer = 0;
        }
        w->timer2 = 23;
        w->atkHit = 0;
        em->xFE++;
    }
    case 1: {
        Vec v;

        v = pPL->pos;
        if (w->timer2) {
            w->timer2--;
            em36AtkTarget(em, w, v);
        }
        if (w->timer) {
            w->timer--;
            em->rot.y += Muku(&em->pos, &v, em->rot.y, 0.13659098f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(0xB);
            }
            if (w->atkHit) {
                w->wait = 0;
                EmRoutineSet(em, 1, 6, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
            break;
        }
        if (em->seFlags28B & 1) {
            cModel* p;
            Vec a;

            if (!(em->motFlags & 0x40)) {
                em36AtkCk(em, 0, 7);
                em36AtkCk(em, 0, 8);
                em36AtkCk(em, 0, 9);
                em36AtkCk(em, 0, 0xA);
                p = em->getPartsPtr(8);
                a = p->worldPos;
                a.y = em->pos.y + 500.0f;
                em36AtkCk2(em, 0, &a, &p->oldWorldPos);
            } else {
                em36AtkCk(em, 0, 0xD);
                em36AtkCk(em, 0, 0xE);
                em36AtkCk(em, 0, 0xF);
                em36AtkCk(em, 0, 0x10);
                p = em->getPartsPtr(0xE);
                a = p->worldPos;
                a.y = em->pos.y + 500.0f;
                em36AtkCk2(em, 0, &a, &p->oldWorldPos);
            }
        }
        if (!(em->motFlags & 0x40)) {
            w->flags |= 0x1A000;
        } else {
            w->flags |= 0x2A000;
        }
        break;
    }
    }
}

static void em36_R1_SpineAtk(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    u8 step = em->xFE;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA3), (int) ARC(0xA4), 5, 1, 0);
        w->atkHit = step;
        w->timer2 = 23;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(0xB);
            }
            if (w->atkHit) {
                w->wait = 0;
                EmRoutineSet(em, 1, 6, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else if (em->seFlags28B & 1) {
            w->flags |= 0xA000;
            em36AtkCk(em, 1, 0);
        }
        break;
    }
}

#define SUB_ARC(no) PL_ARC_PTR(sub->subArc, no)

static void plem36_Stamp(cPlayer* pl)
{
    BitOn(pG->flags_5010, 0x8000);
    pl->dmg.set(0, 10);
    pl->subArc = PL_EM(pl)->subArc;
    switch (pl->xFE) {
    case 0:
        if ((s16) pG->pl_life <= 0) {
            MotionSetCore(pl, MOTION(pl), PL_ARC(0x80), 0, 5, 1, 0);
            EstSet((int) pl, -1, 0, 0, 0x2D, 0x33, 0, 0, (u32) pl, 0);
            PlSetDamageSe(0xD);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC(0x7F), 0, 5, 1, 0);
            EstSet((int) pl, -1, 0, 0, 0x2D, 0x34, 0, 0, (u32) pl, 0);
            PlSetDamageSe(0);
        }
        PlSetFace(1);
        pl->xFE++;
    case 1:
        if (MotionMoveF(pl, 0) && (s16) pG->pl_life > 0) {
            EndPlDamage();
            pl->dmg.set(0, 30);
            break;
        }
        if (pl->frame > 4.7f && pl->frame < 5.3f) {
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if ((s16) pG->pl_life > 0) {
            if ((pl->frame > 87.7f && pl->frame < 88.3f) || (pl->frame > 116.7f && pl->frame < 117.3f)) {
                SndCall(5, 0, &pl->pos, 0, 0, pl);
            }
            if ((pl->frame > 106.7f && pl->frame < 107.3f) || (pl->frame > 130.7f && pl->frame < 131.3f)) {
                SndCall(5, 1, &pl->pos, 0, 0, pl);
            }
            if (pl->frame > 59.7f && pl->frame < 60.3f) {
                SndCall(1, 4, &pl->pos, 0, 0, pl);
                SndCall(1, 0x29, &pl->getPartsPtr(0)->worldPos, 0, 0, pPL);
            }
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void subem36_Stamp()
{
    cSubChar* sub = pSUB;
    u8 step;

    sub->subArc = PL_EM(sub)->subArc;
    sub->dmType = 2;
    pG->flags_5014 |= 0x20000000;
    step = sub->xFE;
    switch (step) {
    case 0:
        LifeDownSet(sub, 780, 0);
        if ((s16) pG->sub_life <= 0) {
            MotionSetCore(sub, MOTION(sub), SUB_ARC(0x8C), 0, 5, 1, 0);
            em36VoiceSet(sub, 0xD, 2);
            EstSet((int) sub, -1, 0, 0, 0x2D, 0x42, 0, 0, (u32) sub, 0);
        } else {
            MotionSetCore(sub, MOTION(sub), SUB_ARC(0x8B), 0, 5, 1, 0);
            em36VoiceSet(sub, 9, 2);
            EstSet((int) sub, -1, 0, 0, 0x2D, 0x41, 0, 0, (u32) sub, 0);
        }
        SubCharSetFace(1);
        sub->xFE++;
    case 1:
        if (MotionMoveF(sub, 0) && (s16) pG->sub_life > 0) {
            EndSubDamage();
        }
        if (sub->frame > 4.7f && sub->frame < 5.3f) {
            SndCall(5, 5, &sub->pos, 0, 0, sub);
        }
        break;
    }
    sub->subArc = sub->subArc2;
}

static void em36_R1_br_Catch(cEm36* em)
{
    if (em->hp > 0 && (em->seFlags28B & 2) && em36CatchCk(em)) {
        VibSetData(VIB_TBL, 7, 1);
        em->stat = 0x010B0000;
    }
}

static void em36_R1_Catch(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0: {
        Vec v;

        if (w->flags2 & 0x10) {
            v = pPL->pos;
        } else {
            GetPlPos(&v, 0, 18.0f);
        }
        if (fabsf(Muku(&em->pos, &v, em->rot.y, PI)) < 1.5707964f) {
            MotionSetCore(em, MOTION(em), ARC(0x35), (int) ARC(0x36), 5, 1, 0);
            w->turnAng = em->rot.y;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x37), (int) ARC(0x38), 5, 1, 0);
            w->turnAng = em->rot.y + PI;
            w->turnAng = LIMIT_ANGLE(w->turnAng);
        }
        w->timer = 20;
        if (pG->x4F88 <= 2) {
            w->timer = 5;
        }
        if (pG->x4F88 > 7) {
            w->timer = 30;
        }
        w->timer2 = 18;
        w->atkHit = 0;
        em->xFE++;
    }
    case 1: {
        Vec v;

        v = pPLS->pos;
        if (w->timer2) {
            w->timer2--;
            if (w->flags2 & 0x10) {
                v = pPL->pos;
            } else {
                GetPlPos(&v, 0, (f32) w->timer2);
            }
        }
        if (em->seFlags28B & 8) {
            f32 d;

            d = Muku(&em->pos, &v, w->turnAng, 0.12566371f);
            w->turnAng += d;
            w->turnAng = LIMIT_ANGLE(w->turnAng);
            em->rot.y += d;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            GameAddPoint(0xB);
            if (w->targetAngAbs > 2.3561945f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
    }
}


#define EM_RTN(em, fc, fd) (((em)->stat & 0xFFFF0000) == (u32) (((fc) << 24) | ((fd) << 16)))

// The effects of the appearance are removed when the player is caught.
// (a macro: through an inline the kind array's address becomes a pseudo, the loads must reload
// w->espKind[no] after each call)
#define em36CatchEffectDelete(em, w, no) \
    EffectEspDelete(1, (w)->espKind[no], (u32) (em), 0); \
    EffectEspgenDelete(1, (w)->espKind[no], (int) (em)); \
    EffectEfmDelete(1, (w)->espKind[no], (int) (em))

static void em36_R1_CatchHit(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    u8 step = em->xFE;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x3B), 0, 5, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem36_CatchHit, -328.61f, 0.0f, 648.33f);
        PlGachaInit();
        em36CatchEffectDelete(em, w, 1);
        em36CatchEffectDelete(em, w, 2);
        if (!(w->flags2 & 0x10)) {
            EstSet((int) em, -1, 0, 0, 0x2D, 0xB, 1, w->espKind[1], (u32) em, 0);
        } else {
            EstSet((int) em, -1, 0, 0, 0x2D, 0x2F, 0, 0, (u32) em, 0);
        }
        SndCall(8, 0xE, &em->pos, em->id, 0, em);
        SndStop(w->sndId, 0);
        w->voiceTimer = 2;
        w->x014 = 0;
        w->x018 = 0;
        w->timer = 25;
        w->timer2 = 10;
        em->xFE++;
    case 1: {
        int end;

        if (w->timer) {
            w->timer--;
        } else {
            PlGachaMove();
            LifeDownSet2(pPL, 20, 0, 1);
        }
        if (w->timer2) {
            w->timer2--;
            end = EmCatchMotionMove(em, 0.3f, 0.2f);
        } else {
            end = MotionMoveF(em, 0);
        }
        if (end) {
            if ((s16) pG->pl_life > 1) {
                em->xFE = 4;
            } else {
                em->xFE = 2;
            }
        } else if ((u32) PlGachaGet() > 30 && (s16) pG->pl_life > 1) {
            em->xFE = 4;
        } else if (em->frame > 22.7f && em->frame < 23.3f) {
            em36VoiceSet(em, 0xB, 2);
        }
        break;
    }
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x3E), (int) ARC(0x3F), 5, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem36_CatchHit, -285.13f, 0.0f, 307.3f);
        pPL->xFE = step;
        em36CatchEffectDelete(em, w, 1);
        if (!(w->flags2 & 0x10)) {
            switch (em->type) {
            case 0:
            case 1:
            default:
                EstSet((int) em, -1, 0, 0, 0x2D, 8, 1, w->espKind[1], (u32) em, 0);
                break;
            case 2:
            case 3:
                break;
            }
        }
        EstSet((int) em, -1, 0, 0, 0x2D, 9, 1, w->espKind[2], (u32) em, 0);
        EstSet((int) em, -1, 0, 0, 0x2D, 0x2D, 0, 0, (u32) em, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->atari.flags &= ~8;
            EmRoutineSet(em, 1, 6, 0, 0);
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x3C), (int) ARC(0x3D), 5, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem36_CatchHit, -285.13f, 0.0f, 307.3f);
        pPL->xFE = step;
        em36CatchEffectDelete(em, w, 1);
        if (!(w->flags2 & 0x10)) {
            switch (em->type) {
            case 0:
            case 1:
            default:
                EstSet((int) em, -1, 0, 0, 0x2D, 8, 1, w->espKind[1], (u32) em, 0);
                break;
            case 2:
            case 3:
                break;
            }
        }
        EstSet((int) em, -1, 0, 0, 0x2D, 9, 1, w->espKind[2], (u32) em, 0);
        EstSet((int) em, -1, 0, 0, 0x2D, 0xC, 0, 0, (u32) em, 0);
        PlGachaInit();
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            em->atari.flags &= ~8;
            EmRoutineSet(em, 1, 1, 0, 0);
        } else if (em->seFlags28B & 1) {
            SndStop(w->sndId, 0);
        }
        break;
    }
}

static void plem36_CatchHit(cPlayer* pl)
{
    u8 step;

    BitOn(pG->flags_5010, 0x8000);
    pl->dmg.set(0, 10);
    pl->subArc = PL_EM(pl)->subArc;
    step = pl->xFE;
    switch (step) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x78), 0, 5, 1, 0);
        PlSetFace(1);
        pl->atari.set(10, 480.00003f, 400.0f);
        pl->x3E0 = 10;
        pl->x3E4 = 0;
        pl->xFE++;
    case 1:
        if (pl->x3E0) {
            pl->x3E0--;
            EmCatchMotionMove(pl, 0.3f, 0.2f);
        } else {
            MotionMoveF(pl, 0);
        }
        if (pl->frame > 14.7f && pl->frame < 15.3f) {
            VibSetData(VIB_TBL, 0xF, 1);
        }
        if (PL_EM_G->xFC != 1 && PL_EM_G->xFD != 0xB) {
            SndStop(pl->x3E4, 0);
            VibSetClearType(1);
            EndPlDamage();
            pl->dmg.set(0, 30);
        } else {
            if (pl->frame > 24.7f && pl->frame < 25.3f) {
                pl->x3E4 = SndCall(8, 0x37, &pPL->getPartsPtr(4)->worldPos, PL_EM(pl)->id, 0, pl);
            }
            pl->xFE = PL_EM_G->xFE;
        }
        break;
    case 2:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x7A), 0, 5, 1, 0);
        pG->pl_life = 0;
        PlSetDamageSe(0xD);
        EstSet((int) pl, -1, 0, 0, 0x2D, 0x31, 0, 0, (u32) pl, 0);
        pl->xFE++;
    case 3:
        MotionMoveF(pl, 0);
        if (pl->frame > 91.7f && pl->frame < 92.3f) {
            SndCall(5, 4, &pl->pos, 0, 0, pl);
        }
        if (pl->frame > 125.7f && pl->frame < 126.3f) {
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        break;
    case 4:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x79), 0, 5, 1, 0);
        VibSetClearType(1);
        EstSet((int) pl, -1, 0, 0, 0x2D, 0x30, 0, 0, (u32) pl, 0);
        SndStop(pl->x3E4, 0);
        SndCall(8, 0x3B, &pPL->getPartsPtr(4)->worldPos, PL_EM(pl)->id, 0, pl);
        pl->xFE++;
    case 5:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

// The wait until the next search after a catch: shorter on the higher difficulty ranks.
static inline void em36SetFindWait(Em36Work* w)
{
    w->findWait = (u8) (Rnd() % 150) + 150;
    if (pGS->x4F88 <= 3) {
        w->findWait = (u8) (Rnd() % 150) + 300;
    }
    if (pGS->x4F88 > 6) {
        w->findWait = (u8) (Rnd() % 150) + 90;
    }
}

static void em36_R1_br_LongCatch(cEm36* em)
{
    if (em->hp > 0 && (em->seFlags28B & 2) && em36LongCatchCk(em)) {
        VibSetData(VIB_TBL, 7, 1);
        em->stat = 0x010D0000;
    }
}

static void em36_R1_LongCatch(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    u8 step = em->xFE;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x39), (int) ARC(0x3A), 5, 1, 0);
        w->turnAng = em->rot.y;
        EstSet((int) em, -1, 0, 0, 0x2D, 7, 0, 0, (u32) em, (void*) step);
        w->timer = 20;
        if (pG->x4F88 <= 2) {
            w->timer = 5;
        }
        if (pG->x4F88 > 7) {
            w->timer = 30;
        }
        w->atkHit = step;
        w->timer2 = 18;
        em->xFE++;
    case 1: {
        Vec v;

        v = pPLS->pos;
        if (w->timer2) {
            w->timer2--;
            if (w->flags2 & 0x10) {
                v = pPL->pos;
            } else {
                GetPlPos(&v, 0, (f32) w->timer2);
            }
        }
        if (em->seFlags28B & 8) {
            f32 d;

            d = Muku(&em->pos, &v, w->turnAng, 0.12566371f);
            w->turnAng += d;
            w->turnAng = LIMIT_ANGLE(w->turnAng);
            em->rot.y += d;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            GameAddPoint(0xB);
            em36SetFindWait(w);
            if (w->targetAngAbs > 2.3561945f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
    }
}

static void em36_R1_LongCatchHit(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    u8 step = em->xFE;

    em->dmType = 2;
    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x42), (int) ARC(0x43), 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem36_LongCatchHit, 186.17f, 0.0f, 3266.86f);
        SndCall(8, 0x24, &pPL->getPartsPtr(3)->worldPos, em->id, 0, pPL);
        PlGachaInit();
        EstSet((int) em, -1, 0, 0, 0x2D, 0x24, 0, 0, (u32) em, (void*) step);
        w->timer = 25;
        em->xFE++;
    case 1:
        if (EmCatchMotionMove(em, 1.0f, 1.0f)) {
            em36SetFindWait(w);
            switch (em->type) {
            case 0:
            case 1:
            default:
                em->stat = 0x010B0000;
                break;
            case 2:
            case 3:
                em->stat = 0x010E0000;
                break;
            }
        }
        if (em->seFlags28B & 1) {
            SndCall(8, 0x23, &pPL->getPartsPtr(3)->worldPos, em->id, 0, pPL);
        }
        if (em36BetweenHitCk(em)) {
            EmRoutineSet(em, 1, 7, 0, 0);
        }
        break;
    }
}

static void plem36_LongCatchHit(cPlayer* pl)
{
    BitOn(pG->flags_5010, 0x8000);
    pl->dmg.set(0, 10);
    pl->subArc = PL_EM(pl)->subArc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x7E), 0, 0, 1, 0);
        PlSetFace(1);
        pl->atari.set(10, 480.00003f, 400.0f);
        pl->xFE++;
    case 1:
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        if (!EM_RTN(PL_EM_G, 1, 0xD)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        } else {
            pl->xFE = PL_EM_G->xFE;
        }
        break;
    case 2:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x7A), 0, 5, 1, 0);
        pl->xFE++;
    case 3:
        MotionMoveF(pl, 0);
        break;
    case 4:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x79), 0, 5, 1, 0);
        pl->xFE++;
    case 5:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em36_R1_SpineCatchHit(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        LifeDownSet2(pPL, 1100, 0, 0);
        if ((s16) pG->pl_life <= 0) {
            MotionSetCore(em, MOTION(em), ARC(0x9E), 0, 0, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x9D), 0, 0, 1, 0);
        }
        EmCatchPLSet(em, 0.0f, 2, (int) plem36_SpineCatchHit, -50.16f, 0.0f, 887.11f);
        SndCall(8, 0x35, &em->pos, em->id, 0, em);
        VibSetData(VIB_TBL, 0xB, 1);
        w->timer = 10;
        w->timer2 = 45;
        em->xFE++;
    case 1: {
        int end;

        if (w->timer) {
            w->timer--;
            end = EmCatchMotionMove(em, 1.0f, 1.0f);
        } else {
            end = MotionMoveF(em, 0);
        }
        if (w->timer2) {
            w->timer2--;
            w->flags |= 0x6000;
        }
        if ((s16) pG->pl_life <= 0) {
            w->flags |= 0x6000;
        }
        if (end && (s16) pG->pl_life > 0) {
            em->atari.flags &= ~8;
            EmRoutineSet(em, 1, 6, 0, 0);
        }
        break;
    }
    }
}

static void plem36_SpineCatchHit(cPlayer* pl)
{
    u8 step;

    BitOn(pG->flags_5010, 0x8000);
    pl->dmg.set(0, 10);
    pl->subArc = PL_EM(pl)->subArc;
    step = pl->xFE;
    switch (step) {
    case 0:
        if ((s16) pG->pl_life <= 0) {
            MotionSetCore(pl, MOTION(pl), PL_ARC(0xA0), 0, 0, 1, 0);
            EstSet((int) pl, -1, 0, 0, 0x2D, 0x45, 0, 0, (u32) pl, (void*) step);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC(0x9F), 0, 0, 1, 0);
            EstSet((int) pl, -1, 0, 0, 0x2D, 0x44, 0, 0, (u32) pl, (void*) step);
        }
        PlSetFace(1);
        pl->atari.set(10, 480.00003f, 400.0f);
        pl->x3E0 = 10;
        pl->xFE++;
    case 1: {
        int end;

        if (pl->x3E0) {
            pl->x3E0--;
            end = EmCatchMotionMove(pl, 1.0f, 1.0f);
        } else {
            end = MotionMoveF(pl, 0);
        }
        if (end && (s16) pG->pl_life > 0) {
            EndPlDamage();
            pl->dmg.set(0, 30);
            break;
        }
        if (pl->frame > 6.7f && pl->frame < 7.3f) {
            if ((s16) pG->pl_life <= 0) {
                PlSetDamageSe(0xD);
            } else {
                PlSetDamageSe(0);
            }
        }
        if ((s16) pG->pl_life > 0 && pl->frame > 61.7f && pl->frame < 62.3f) {
            SndCall(8, 0x3E, &pl->getPartsPtr(4)->worldPos, 0x36, 0, pl);
        }
        break;
    }
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em36_R1_br_LostCatch(cEm36* em)
{
    if (em->hp > 0 && (em->seFlags28B & 2) && em36BiteCk(em)) {
        VibSetData(VIB_TBL, 7, 1);
        em->stat = 0x01150000;
    }
}

static void em36_R1_LostCatch(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    u8 step = em->xFE;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x40), (int) ARC(0x41), 5, 1, 0);
        w->turnAng = em->rot.y;
        w->timer = 20;
        if (pGS->x4F88 <= 2) {
            w->timer = 5;
        }
        if (pGS->x4F88 > 7) {
            w->timer = 30;
        }
        w->atkHit = step;
        w->timer2 = 18;
        em->xFE++;
    case 1: {
        Vec v;

        v = pPLS->pos;
        if (w->timer2) {
            w->timer2--;
            if (w->flags2 & 0x10) {
                v = pPL->pos;
            } else {
                GetPlPos(&v, 0, (f32) w->timer2);
            }
        }
        if (em->seFlags28B & 8) {
            f32 d;

            d = Muku(&em->pos, &v, w->turnAng, 0.12566371f);
            w->turnAng += d;
            w->turnAng = LIMIT_ANGLE(w->turnAng);
            em->rot.y += d;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.3561945f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
    }
}

// The lost arm grows back: the motion key events reattach the parts.
static inline void em36RegeneArmParts(cEm36* em, Em36Work* w)
{
    if (em->seFlags28B & 1) {
        if (em->motFlags & 0x40) {
            w->flags2 &= ~2;
            em36PartsSet(em, 3, 0);
            em36RegeneTenClear(em, 3);
        } else {
            w->flags2 &= ~1;
            em36PartsSet(em, 2, 0);
            em36RegeneTenClear(em, 2);
        }
    }
    if (em->seFlags28B & 2) {
        if (em->motFlags & 0x40) {
            w->flags2 &= ~1;
            em36PartsSet(em, 2, 0);
            em36RegeneTenClear(em, 2);
        } else {
            w->flags2 &= ~2;
            em36PartsSet(em, 3, 0);
            em36RegeneTenClear(em, 3);
        }
    }
}

static void em36_R1_RegeneArm(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    u8 step = em->xFE;

    w->flags |= 0x50;
    switch (step) {
    case 0:
        if ((w->flags2 & 3) == 3) {
            MotionSetCore(em, MOTION(em), ARC(0x54), (int) ARC(0x55), 5, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2D, 0x11, 0, 0, (u32) em, (void*) step);
        } else if (w->flags2 & 2) {
            MotionSetCore(em, MOTION(em), ARC(0x52), (int) ARC(0x53), 5, 0x41, 0);
            EstSet((int) em, -1, 0, 0, 0x2D, 0xE, 0, 0, (u32) em, (void*) step);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x52), (int) ARC(0x53), 5, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2D, 0xD, 0, 0, (u32) em, 0);
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if ((u32) em->type <= 1 && !(w->flags & 4) && w->seWait == 0 && (w->flags & 1) &&
                em->plDist2 > 25000000.0f && em->plDist2 < 100000000.0f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
            break;
        }
        em36RegeneArmParts(em, w);
        break;
    }
}

static void em36_R1_RegeneArm2(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    u8 step = em->xFE;

    w->flags |= 0xD0;
    switch (step) {
    case 0:
        if ((w->flags2 & 3) == 3) {
            MotionSetCore(em, MOTION(em), ARC(0x6E), (int) ARC(0x6F), 5, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2D, 0x3F, 0, 0, (u32) em, (void*) step);
        } else if (w->flags2 & 2) {
            MotionSetCore(em, MOTION(em), ARC(0x6C), (int) ARC(0x6D), 5, 0x41, 0);
            EstSet((int) em, -1, 0, 0, 0x2D, 0x3D, 0, 0, (u32) em, (void*) step);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x6C), (int) ARC(0x6D), 5, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2D, 0x3E, 0, 0, (u32) em, 0);
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->flags2 & 0xC) {
                EmRoutineSet(em, 1, 0x19, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x16, 0, 0);
            }
            break;
        }
        em36RegeneArmParts(em, w);
        break;
    }
}

static void em36_R1_RegeneFoot(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    u8 step = em->xFE;

    w->flags |= 0xD0;
    switch (step) {
    case 0:
        if ((w->flags2 & 0xC) == 0xC) {
            MotionSetCore(em, MOTION(em), ARC(0x72), (int) ARC(0x73), 5, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2D, 0x40, 0, 0, (u32) em, (void*) step);
        } else if (w->flags2 & 8) {
            MotionSetCore(em, MOTION(em), ARC(0x70), (int) ARC(0x71), 5, 0x41, 0);
            EstSet((int) em, -1, 0, 0, 0x2D, 0x10, 0, 0, (u32) em, (void*) step);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x70), (int) ARC(0x71), 5, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2D, 0xF, 0, 0, (u32) em, 0);
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if ((w->flags2 & 3) && w->atkTimer[2] == 0 && w->atkTimer[3] == 0) {
                EmRoutineSet(em, 1, 0x18, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x16, 0, 0);
            }
            break;
        }
        if (em->seFlags28B & 1) {
            if (em->motFlags & 0x40) {
                w->flags2 &= ~8;
                em36PartsSet(em, 5, 0);
                em36RegeneTenClear(em, 5);
            } else {
                w->flags2 &= ~4;
                em36PartsSet(em, 4, 0);
                em36RegeneTenClear(em, 4);
            }
        }
        if (em->seFlags28B & 2) {
            if (em->motFlags & 0x40) {
                w->flags2 &= ~4;
                em36PartsSet(em, 4, 0);
                em36RegeneTenClear(em, 4);
            } else {
                w->flags2 &= ~8;
                em36PartsSet(em, 5, 0);
                em36RegeneTenClear(em, 5);
            }
        }
        break;
    }
}

static void em36_R1_D_Wait(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0xB0;
    switch (em->xFE) {
    case 0:
        if (em->type == 2 || em->type == 3 || (u8) (Rnd() % 10) > 4) {
            MotionSetCore(em, MOTION(em), ARC(0x76), (int) ARC(0x77), 10, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x62), 0, 10, 5, 0);
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if ((em->type == 2 || em->type == 3) && em->plDist2 > 2250000.0f && (s16) pG->pl_life > 0) {
                EmRoutineSet(em, 1, 0x11, 0, 0);
            } else {
                em->xFE = 0;
            }
        } else {
            if (w->atkTimer[4] == 0 && w->atkTimer[5] == 0) {
                EmRoutineSet(em, 1, 0x19, 0, 0);
            } else if (!(w->flags2 & 0xC)) {
                EmRoutineSet(em, 1, 0x16, 0, 0);
            } else if ((s16) pG->pl_life > 0) {
                if (w->wait == 0) {
                    Vec v;
                    f32 d;

                    if (w->flags2 & 0x10) {
                        v = pPL->pos;
                    } else {
                        GetPlPos(&v, 0, 10.0f);
                    }
                    d = (em->pos.x - v.x) * (em->pos.x - v.x) + (em->pos.z - v.z) * (em->pos.z - v.z);
                    switch (em->type) {
                    case 0:
                    case 1:
                    default:
                        if ((w->flags & 1) && d < 36000000.0f && w->routeAngAbs < 0.5235988f) {
                            w->wait = 90;
                            EmRoutineSet(em, 1, 0x14, 0, 0);
                            return;
                        }
                        break;
                    case 2:
                    case 3:
                        if ((w->flags & 1) && d < 2250000.0f) {
                            w->wait = 90;
                            EmRoutineSet(em, 1, 0x13, 0, 0);
                            return;
                        }
                        break;
                    }
                } else if (w->routeAngAbs > 0.5235988f) {
                    EmRoutineSet(em, 1, 0x12, 0, 0);
                }
            }
        }
        break;
    }
    em36BreathSe(em);
}

static void em36_R1_D_Walk(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0xB0;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA1), (int) ARC(0xA2), 10, 5, 0);
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            if (w->routeAngAbs > 0.5235988f) {
                EmRoutineSet(em, 1, 0x12, 0, 0);
            } else if ((s16) pG->pl_life <= 0 || em->plDist2 < 1000000.0f) {
                EmRoutineSet(em, 1, 0x10, 0, 0);
            } else if (w->atkTimer[4] == 0 && w->atkTimer[5] == 0) {
                EmRoutineSet(em, 1, 0x19, 0, 0);
            } else if (!(w->flags2 & 0xC)) {
                EmRoutineSet(em, 1, 0x16, 0, 0);
            }
        } else if (w->wait == 0) {
            Vec v;
            f32 d;

            if ((w->flags2 & 0x10) || pG->x4F88 <= 2) {
                v = pPL->pos;
            } else {
                GetPlPos(&v, 0, 10.0f);
            }
            d = (em->pos.x - v.x) * (em->pos.x - v.x) + (em->pos.z - v.z) * (em->pos.z - v.z);
            switch (em->type) {
            case 0:
            case 1:
            default:
                if ((w->flags & 1) && d < 36000000.0f && w->routeAngAbs < 0.5235988f) {
                    w->wait = 90;
                    EmRoutineSet(em, 1, 0x14, 0, 0);
                    return;
                }
                break;
            case 2:
            case 3:
                if ((w->flags & 1) && d < 2250000.0f) {
                    w->wait = 90;
                    EmRoutineSet(em, 1, 0x13, 0, 0);
                    return;
                }
                break;
            }
        }
        break;
    }
    em36BreathSe(em);
}

static void em36_R1_D_Turn(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0xB0;
    switch (em->xFE) {
    case 0:
        if (w->routeAng < 0.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x74), (int) ARC(0x75), 10, 1, 0);
            w->turnAng = em->rot.y + -1.5707964f;
            w->turnAng = LIMIT_ANGLE(w->turnAng);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x74), (int) ARC(0x75), 10, 0x41, 0);
            w->turnAng = em->rot.y + 1.5707964f;
            w->turnAng = LIMIT_ANGLE(w->turnAng);
        }
        em->xFE++;
    case 1:
        if (em->seFlags28B & 8) {
            f32 d;

            d = Muku(&em->pos, &w->targetPos, w->turnAng, 0.09817477f);
            w->turnAng += d;
            w->turnAng = LIMIT_ANGLE(w->turnAng);
            em->rot.y += d;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->atkTimer[4] == 0 && w->atkTimer[5] == 0) {
                EmRoutineSet(em, 1, 0x19, 0, 0);
            } else if (w->routeAngAbs > 0.5235988f) {
                EmRoutineSet(em, 1, 0x12, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x10, 0, 0);
            }
        } else if (w->wait == 0) {
            Vec v;
            f32 d;

            if ((w->flags2 & 0x10) || pG->x4F88 <= 2) {
                v = pPL->pos;
            } else {
                GetPlPos(&v, 0, 10.0f);
            }
            d = (em->pos.x - v.x) * (em->pos.x - v.x) + (em->pos.z - v.z) * (em->pos.z - v.z);
            if ((w->flags & 1) && d < 16000000.0f && w->routeAngAbs < 0.5235988f) {
                switch (em->type) {
                case 0:
                case 1:
                default:
                    EmRoutineSet(em, 1, 0x14, 0, 0);
                    break;
                case 2:
                case 3:
                    EmRoutineSet(em, 1, 0x13, 0, 0);
                    break;
                }
                return;
            }
        }
        break;
    }
    em36BreathSe(em);
}

static void em36_R1_D_SpineAtk(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    u8 step = em->xFE;

    w->flags |= 0xB0;
    switch (step) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA5), (int) ARC(0xA6), 5, 1, 0);
        w->atkHit = step;
        w->timer2 = 23;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->wait = 30;
            EmRoutineSet(em, 1, 0x10, 0, 0);
        } else if (em->seFlags28B & 1) {
            w->flags |= 0xA000;
            em36AtkCk(em, 1, 0);
        }
        break;
    }
}

static void em36_R1_br_D_Catch(cEm36* em)
{
    if (em->hp > 0 && (em->seFlags28B & 2) && em36BiteCk(em)) {
        VibSetData(VIB_TBL, 7, 1);
        em->stat = 0x01150000;
    }
}

static void em36_R1_D_Catch(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    u8 step = em->xFE;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x64), (int) ARC(0x65), 5, 1, 0);
        w->timer = 20;
        if (pG->x4F88 <= 2) {
            w->timer = 5;
        }
        if (pG->x4F88 > 7) {
            w->timer = 30;
        }
        w->atkHit = step;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
            em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.19634955f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            w->wait = 30;
            EmRoutineSet(em, 1, 0x10, 0, 0);
        }
        break;
    }
    if (em->seFlags28B & 0x40) {
        w->flags |= 0x100;
    }
}

static void em36_R1_D_CatchHit(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    u8 step = em->xFE;

    w->flags |= 0x10;
    switch (step) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x66), 0, 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem36_D_CatchHit, -101.71f, 0.0f, 555.67f);
        em36VoiceSet(em, 0xB, 2);
        EstSet((int) em, -1, 0, 0, 0x2D, 0x2B, 1, w->espKind[0], (u32) em, (void*) step);
        PlGachaInit();
        em->xFE++;
    case 1:
        PlGachaMove();
        LifeDownSet2(pPL, 20, 0, 1);
        if (EmCatchMotionMove(em, 1.0f, 1.0f)) {
            if ((s16) pG->pl_life > 1) {
                em->xFE = 4;
            } else {
                em->xFE = 2;
            }
        } else if ((u32) PlGachaGet() > 30 && (s16) pG->pl_life > 1) {
            em->xFE = 4;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x69), 0, 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem36_D_CatchHit, -19.96f, 0.0f, 405.95f);
        pPL->xFE = step;
        pG->pl_life = 0;
        PlSetDamageSe(0xD);
        em36CatchEffectDelete(em, w, 0);
        EstSet((int) em, -1, 0, 0, 0x2D, 0x2E, 0, 0, (u32) em, 0);
        w->timer = 10;
        em->xFE++;
    case 3:
        if (w->timer) {
            w->timer--;
            EmCatchMotionMove(em, 1.0f, 1.0f);
        } else {
            MotionMoveF(em, 0);
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x67), (int) ARC(0x68), 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem36_D_CatchHit, 89.73f, 0.0f, 328.37f);
        pPL->xFE = step;
        PlGachaInit();
        SndStop(w->sndId, 0);
        em36CatchEffectDelete(em, w, 0);
        EstSet((int) em, -1, 0, 0, 0x2D, 0x2C, 0, 0, (u32) em, 0);
        w->timer = 10;
        em->xFE++;
    case 5: {
        int end;

        if (w->timer) {
            w->timer--;
            end = EmCatchMotionMove(em, 1.0f, 1.0f);
        } else {
            end = MotionMoveF(em, 0);
        }
        if (end) {
            em->atari.flags &= ~8;
            w->wait = 90;
            EmRoutineSet(em, 1, 0x10, 0, 0);
        }
        break;
    }
    }
}

static void plem36_D_CatchHit(cPlayer* pl)
{
    BitOn(pG->flags_5010, 0x8000);
    pl->dmg.set(0, 10);
    pl->subArc = PL_EM(pl)->subArc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x7B), 0, 0, 1, 0);
        PlSetFace(1);
        pl->atari.set(10, 480.00003f, 400.0f);
        VibSetData(VIB_TBL, 0xF, 1);
        pl->x3E4 = SndCall(8, 0x37, &pPL->getPartsPtr(4)->worldPos, PL_EM(pl)->id, 0, pl);
        pl->xFE++;
    case 1:
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        if (!EM_RTN(PL_EM_G, 1, 0xB)) {
            SndStop(pl->x3E4, 0);
            VibSetClearType(1);
            EndPlDamage();
            pl->dmg.set(0, 30);
        } else {
            pl->xFE = PL_EM_G->xFE;
        }
        break;
    case 2:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x7D), 0, 0, 1, 0);
        pl->x3E0 = 10;
        EstSet((int) pl, -1, 0, 0, 0x2D, 0x43, 0, 0, (u32) pl, 0);
        pl->xFE++;
    case 3:
        if (pl->x3E0) {
            pl->x3E0--;
            EmCatchMotionMove(pl, 1.0f, 1.0f);
        } else {
            MotionMoveF(pl, 0);
        }
        if (pl->frame > 15.7f && pl->frame < 16.3f) {
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        break;
    case 4:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x7C), 0, 0, 1, 0);
        EstSet((int) pl, -1, 0, 0, 0x2D, 0x32, 0, 0, (u32) pl, 0);
        VibSetClearType(1);
        SndStop(pl->x3E4, 0);
        SndCall(8, 0x3B, &pPL->getPartsPtr(4)->worldPos, PL_EM(pl)->id, 0, pl);
        pl->x3E0 = 10;
        pl->xFE++;
    case 5: {
        int end;

        if (pl->x3E0) {
            pl->x3E0--;
            end = EmCatchMotionMove(pl, 1.0f, 1.0f);
        } else {
            end = MotionMoveF(pl, 0);
        }
        if (end) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    }
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em36_R1_Wakeup(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x40;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x6A), (int) ARC(0x6B), 5, 1, 0);
        em->xFE++;
    case 1:
        if (em->seFlags28B & 0x80) {
            w->flags |= 0x20;
        } else {
            w->flags &= ~0x20;
        }
        if (MotionMoveF(em, 0)) {
            u32 f = w->flags;

            w->flags &= ~0x20;
            if ((u32) em->type <= 1 && !(f & 4) && w->seWait == 0 && (f & 1) && em->plDist2 > 25000000.0f &&
                em->plDist2 < 100000000.0f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

static void em36_R0_Damage(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 8;
    Em36_R2_move_tbl[em->xFD](em);
}

static void em36_R1_Dm_Normal(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0: {
        void* m0;
        void* m1;
        int flip;
        f32 ang;

        em->xFF = em36GetDmPosType(em);
        ang = fabsf(Muku(&em->pos, &em->x328, em->rot.y, PI));
        m1 = 0;
        if (ang < 1.5707964f) {
            switch (em->xFF) {
            case 0:
            default:
                m0 = ARC(0x5B);
                flip = 1;
                break;
            case 1:
                m0 = ARC(0x58);
                flip = 1;
                break;
            case 2:
                m0 = ARC(0x4A);
                flip = 1;
                break;
            case 3:
                m0 = ARC(0x4A);
                flip = 0x41;
                break;
            case 4:
                m0 = ARC(0x4C);
                m1 = ARC(0x4D);
                flip = 1;
                break;
            case 5:
                m0 = ARC(0x4C);
                m1 = ARC(0x4D);
                flip = 0x41;
                break;
            }
        } else {
            switch (em->xFF) {
            case 0:
            default:
                m0 = ARC(0x5C);
                m1 = ARC(0x5D);
                flip = 1;
                break;
            case 1:
                m0 = ARC(0x59);
                m1 = ARC(0x5A);
                flip = 1;
                break;
            case 2:
                m0 = ARC(0x4B);
                flip = 1;
                break;
            case 3:
                m0 = ARC(0x4B);
                flip = 0x41;
                break;
            case 4:
                m0 = ARC(0x4E);
                m1 = ARC(0x4F);
                flip = 1;
                break;
            case 5:
                m0 = ARC(0x4E);
                m1 = ARC(0x4F);
                flip = 0x41;
                break;
            }
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 3, flip, 0);
        em->xFE++;
    }
    case 1:
        switch (em->xFF) {
        case 4:
        case 5:
            w->flags |= 0x40;
            break;
        }
        if (MotionMoveF(em, 0)) {
            switch (em->xFF) {
            default:
                if ((u32) em->type <= 1 && !(w->flags & 4) && w->seWait == 0 && (w->flags & 1) &&
                    em->plDist2 > 25000000.0f && em->plDist2 < 100000000.0f) {
                    EmRoutineSet(em, 1, 2, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 1, 0, 0xA);
                }
                break;
            case 4:
            case 5:
                w->flags |= 0x20;
                EmRoutineSet(em, 1, 0x10, 0, 0xA);
                break;
            }
        }
        break;
    }
}

// The random twitch of the big damage motions.
static inline void em36DmTwitch(Em36Work* w)
{
    w->flags |= 0x80000;
    if (w->timer) {
        w->timer--;
        switch ((u8) (Rnd() % 6)) {
        default:
            w->flags &= ~0x2000;
            break;
        case 0:
            w->flags |= 0x2000;
            break;
        case 1:
            w->flags |= 0x6000;
            break;
        case 2:
            w->flags |= 0xA000;
            break;
        case 3:
            w->flags |= 0x22000;
            break;
        case 4:
            w->flags |= 0x12000;
            break;
        }
    }
}

// The same twitch while dying: the timer is the motion's end and is not counted down here.
static inline void em36DieTwitch(Em36Work* w)
{
    w->flags |= 0x80000;
    if (w->timer) {
        switch ((u8) (Rnd() % 6)) {
        default:
            w->flags &= ~0x2000;
            break;
        case 0:
            w->flags |= 0x2000;
            break;
        case 1:
            w->flags |= 0x6000;
            break;
        case 2:
            w->flags |= 0xA000;
            break;
        case 3:
            w->flags |= 0x22000;
            break;
        case 4:
            w->flags |= 0x12000;
            break;
        }
    }
}

// The spined enemy's extra damage voice.
static inline void em36DmVoice(cEm36* em)
{
    em36VoiceSet(em, 0x38, 2);
    switch (em->type) {
    case 2:
    case 3:
        SndCall(8, 0x36, &em->pos, em->id, 0, em);
        break;
    case 0:
    case 1:
    default:
        break;
    }
}

static void em36_R1_Dm_Big(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x10;
    w->flags |= 0x200000;
    switch (em->xFE) {
    case 0: {
        Vec* p;

        if ((u8) (Rnd() % 10) > 4) {
            if ((u8) (Rnd() % 10) > 4) {
                MotionSetCore(em, MOTION(em), ARC(0x48), 0, 3, 0x41, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x48), 0, 3, 1, 0);
            }
        } else {
            if ((u8) (Rnd() % 10) > 4) {
                MotionSetCore(em, MOTION(em), ARC(0x49), 0, 3, 0x41, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x49), 0, 3, 1, 0);
            }
        }
        em36VoiceSet(em, 0x38, 2);
        p = &em->pos;
        SndCall(8, 0x2A, p, em->id, 0, em);
        switch (em->type) {
        case 2:
        case 3:
            SndCall(8, 0x36, p, em->id, 0, em);
            break;
        case 0:
        case 1:
        default:
            break;
        }
        w->timer = 60;
        em->xFE++;
    }
    case 1:
        em36DmTwitch(w);
        if (MotionMoveF(em, 0)) {
            if ((u32) em->type <= 1 && !(w->flags & 4) && w->seWait == 0 && (w->flags & 1) &&
                em->plDist2 > 25000000.0f && em->plDist2 < 100000000.0f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0xA);
            }
        }
        break;
    }
}

static void em36_R1_Dm_Weak(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        if ((u8) (Rnd() % 10) > 4) {
            MotionSetCore(em, MOTION(em), ARC(0x93), (int) ARC(0x94), 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x93), (int) ARC(0x94), 3, 1, 0);
        }
        em36DmVoice(em);
        w->timer = 60;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if ((u32) em->type <= 1 && !(w->flags & 4) && w->seWait == 0 && (w->flags & 1) &&
                em->plDist2 > 25000000.0f && em->plDist2 < 100000000.0f) {
                EmRoutineSet(em, 1, 2, 0, 0);
                break;
            }
            EmRoutineSet(em, 1, 1, 0, 0xA);
        }
        em36DmTwitch(w);
        break;
    }
}

static void em36_R1_Dm_Down(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        if ((u8) (Rnd() % 10) > 4) {
            MotionSetCore(em, MOTION(em), ARC(0x5E), 0, 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x5E), 0, 3, 1, 0);
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (!(w->flags2 & 0xC)) {
                EmRoutineSet(em, 1, 0x16, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x10, 0, 0);
            }
        }
        break;
    }
}

static void em36_R1_Dm_DownWeak(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        if ((u8) (Rnd() % 10) > 4) {
            MotionSetCore(em, MOTION(em), ARC(0x60), (int) ARC(0x61), 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x60), (int) ARC(0x61), 3, 1, 0);
        }
        em36DmVoice(em);
        w->timer = 60;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (!(w->flags2 & 0xC)) {
                EmRoutineSet(em, 1, 0x16, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x10, 0, 0);
            }
            break;
        }
        em36DmTwitch(w);
        break;
    }
}

static void em36_R1_Dm_DownJump(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x50), (int) ARC(0x51), 3, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 1, 0, 0);
            } else if ((u8) (Rnd() % 10) > 2) {
                EmRoutineSet(em, 1, 0x10, 0, 0);
            } else {
                switch (em->type) {
                case 0:
                case 1:
                default:
                    EmRoutineSet(em, 1, 0x14, 0, 0);
                    break;
                case 2:
                case 3:
                    EmRoutineSet(em, 1, 0x13, 0, 0);
                    break;
                }
            }
        }
        break;
    }
}

static void em36_R0_Die(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    w->flags |= 8;
    Em36_R3_move_tbl[em->xFD](em);
}

// The death: fall, then sink into the floor while fading out (em36_R1_Die_Normal / Die_Down).
static inline void em36DieCore(cEm36* em, int mot0, int mot1)
{
    Em36Work* w = EM36_WK(em);
    u8 step = em->xFE;

    switch (step) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(mot0), (int) ARC(mot1), 30, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2D, 0x3C, 0, 0, (u32) em, (void*) step);
        em36DmVoice(em);
        w->timer = 1;
        w->timer2 = 0;
        em->xFE++;
    case 1: {
        int end;

        end = MotionMoveF(em, 0);
        if (end) {
            em->clearStatus(5);
            em->setStatus(8);
            EmSetDropItem(em);
            em->atari.flags &= ~0x300;
            em->xFE++;
            break;
        }
        if (em->seFlags28B & 1) {
            em36PartsSet(em, 6, 1);
            if (w->pParts[0]) {
                w->pParts[0]->be_flag &= ~8;
            }
            if (w->pParts[1]) {
                w->pParts[1]->be_flag &= ~8;
            }
            if (w->pParts[2]) {
                w->pParts[2]->be_flag &= ~8;
            }
            if (w->pParts[3]) {
                w->pParts[3]->be_flag &= ~8;
            }
            em36CatchEffectDelete(em, w, 1);
            em36CatchEffectDelete(em, w, 2);
            em36CatchEffectDelete(em, w, 3);
            SndCall(8, 0x25, &em->pos, em->id, 0, em);
            em36RegeneTenClear(em, 1);
            em36RegeneTenClear(em, 0);
            em36RegeneTenClear(em, 2);
            em36RegeneTenClear(em, 3);
            em36RegeneTenClear(em, 4);
            em36RegeneTenClear(em, 5);
            w->timer = end;
            w->seTimer = 0x97;
        }
        em36DieTwitch(w);
        break;
    }
    case 2:
        w->timer2 = 150;
        w->timer = 0;
        EstSet((int) em, -1, 0, 0, 0x2D, 0x47, 0, 0, (u32) em, 0);
        SndCall(8, 0x3F, &em->pos, em->id, 0, em);
        {
            f32 one = 1.0f;

            w->flags |= 0x100000;
            w->scale = one;
        }
        em->xFE++;
    case 3:
        if (w->timer) {
            w->timer--;
        } else {
            f32 sc = w->scale - 0.003f;
            f32 min = 0.1f;

            w->scale = sc;
            if (sc < min) {
                w->scale = min;
            }
            em->pos.y -= 6.0f;
        }
        TransMatrix(em->mat, &em->pos);
        if (w->timer2) {
            w->timer2--;
        } else {
            f32 a = em->alpha - 0.1f;
            f32 zero = 0.0f;

            em->alpha = a;
            if (a <= zero) {
                em->alpha = zero;
                em->be_flag &= ~2;
                em->be_flag |= 0x4000;
                em->xFE++;
            }
        }
        break;
    }
    if (w->seTimer) {
        w->seTimer--;
        if (w->seTimer % 30 == 0) {
            SndCall(8, 0x27, &em->pos, em->id, 0, em);
        }
    }
}

static void em36_R1_Die_Normal(cEm36* em)
{
    em36DieCore(em, 0x56, 0x57);
}

static void em36_R1_Die_Down(cEm36* em)
{
    em36DieCore(em, 0x89, 0x8A);
}

void em36RouteCk(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    Vec a;
    Vec b;

    if (em->hp <= 0) {
        return;
    }
    RouteCkToPos(em, &pPL->pos, &w->routePos, 0, 0);
    w->routeAng = Muku(&em->pos, &w->routePos, em->rot.y, PI);
    w->routeAngAbs = fabsf(w->routeAng);
    if (em->xFC == 0) {
        w->routeAng = 0.0f;
        w->routeAngAbs = 0.0f;
        em->plDist2 = 100000000.0f;
    }
    a.x = em->pos.x;
    a.y = em->pos.y + 1300.0f;
    a.z = em->pos.z;
    b.x = pPL->pos.x;
    b.y = pPL->pos.y + 1300.0f;
    b.z = pPL->pos.z;
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
        w->flags |= 1;
    }
    a.x = em->pos.x;
    a.y = em->pos.y + 1300.0f;
    a.z = em->pos.z;
    b.x = pPL->pos.x;
    b.y = pPL->pos.y + 1300.0f;
    b.z = pPL->pos.z;
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x4000) == 0) {
        w->flags |= 0x1000;
    }
    a.x = em->pos.x;
    a.y = em->pos.y + 500.0f;
    a.z = em->pos.z;
    b.x = pPL->pos.x;
    b.y = pPL->pos.y + 500.0f;
    b.z = pPL->pos.z;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
        w->flags |= 0x800;
    }
    w->plRouteDis = RouteCkPosToPosDis(&em->pos, &pPL->pos);
    if (pSUBS) {
        RouteCkToEm(em, pSUB, &w->subRoutePos, 0);
        w->subRouteDis = RouteCkPosToPosDis(&em->pos, &pSUB->pos);
        w->subDist2 = (em->pos.x - pSUBS->pos.x) * (em->pos.x - pSUBS->pos.x) +
                      (em->pos.z - pSUBS->pos.z) * (em->pos.z - pSUBS->pos.z);
        w->subAng = Muku(&em->pos, &w->subRoutePos, em->rot.y, PI);
        w->subAngAbs = fabsf(w->subAng);
        if (em->xFC == 0) {
            w->subAng = 0.0f;
            w->subAngAbs = 0.0f;
            w->subDist2 = 100000000.0f;
        }
        a.x = em->pos.x;
        a.y = em->pos.y + 1500.0f;
        a.z = em->pos.z;
        b.x = pSUB->pos.x;
        b.y = pSUB->pos.y + 1500.0f;
        b.z = pSUB->pos.z;
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
            w->flags |= 2;
        }
    } else {
        w->flags &= ~4;
        w->subRoutePos = w->routePos;
        w->subRouteDis = 100000000.0f;
        w->subDist2 = 1.0e16f;
        w->subAng = 0.0f;
        w->subAngAbs = 0.0f;
    }
    w->targetPos = w->routePos;
    w->targetAng = w->routeAng;
    w->targetAngAbs = w->routeAngAbs;
    w->targetDist = em->plDist2;
    w->pTarget = pPLS;
    w->flags &= ~4;
    if (pSUBS && !(pG->flags_500C & 0x800) && w->plRouteDis > w->subRouteDis + 1000.0f) {
        w->targetPos = w->subRoutePos;
        w->targetAng = w->subAng;
        w->targetAngAbs = w->subAngAbs;
        w->targetDist = w->subDist2;
        w->flags |= 4;
    }
}

void em36NeckMove(cEm36* em)
{
    Vec v;
}

int em36AtkCk(cEm36* em, u32 no, int parts)
{
    cModel* p = em->getPartsPtr(parts);

    return em36AtkCk2(em, no, &p->worldPos, &p->oldWorldPos);
}

int em36AtkCk2(cEm36* em, int no, Vec* pos, Vec* oldPos)
{
    Em36Work* w = EM36_WK(em);
    int hit;

    if (w->atkHit) {
        return 0;
    }
    hit = EmAtkHitCk(&em36_atk_tbl[no], pos, oldPos, 0);
    if (hit) {
        if (hit & 1) {
            w->atkHit = 1;
            switch (no) {
            case 0:
                SndCall(8, 0x1B, &pPL->pos, em->id, 0, pPL);
                SetPlDamage((int) em, plem36_Stamp);
                VibSetData(VIB_TBL, 0xB, 1);
                break;
            case 1:
                SndCall(8, 0x35, &pPL->pos, em->id, 0, pPL);
                EmSubBloodSet(em, pos, 1, 0x2D, 0x46);
                VibSetData(VIB_TBL, 7, 1);
                break;
            }
            QuakeExec(0, 0, 5, 22.0f, 2);
        }
        if (hit & 2) {
            switch (no) {
            case 0:
                SndCall(8, 0x1B, &em->pos, em->id, 0, em);
                SetSubDamage((int) em, (void*) subem36_Stamp);
                break;
            case 1:
                SndCall(8, 0x35, &pSUB->pos, em->id, 0, pSUB);
                EmSubBloodSet(em, pos, 1, 0x2D, 0x46);
                break;
            }
            w->atkHit = 1;
        }
        return 1;
    }
    return 0;
}

int em36GetDmPosType(cEm36* em)
{
    EmHitInfo* p = em->dmPart;

    if (p == 0) {
        return 0;
    }
    switch (p->partsNo) {
    case 2:
        return 0;
    case 3:
        return 0;
    case 4:
        return 1;
    case 5:
        return 1;
    case 7:
        return 2;
    case 8:
        return 2;
    case 9:
        return 2;
    case 0xA:
        return 2;
    case 0xB:
        return 2;
    case 0xD:
        return 3;
    case 0xE:
        return 3;
    case 0xF:
        return 3;
    case 0x10:
        return 3;
    case 0x11:
        return 3;
    case 0x12:
        return 0;
    case 0x13:
        return 4;
    case 0x14:
        return 4;
    case 0x17:
        return 5;
    case 0x18:
        break;
    default:
        return 0;
    }
    return 5;
}

int em36LostParts(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    switch ((u32) em36GetDmPosType(em)) {
    case 0:
    default: {
        cModel* p;

        if (w->flags2 & 0x20) {
            return 0;
        }
        w->flags2 |= 0x20;
        em36PartsSet(em, 0, 1);
        EstSet((int) em, -1, 0, 0, 0x2D, 1, 0, 0, (u32) em, 0);
        EstSet((int) em, -1, 0, 0, 0x2D, 0x3B, 0, 0, (u32) em, 0);
        SndCall(8, 0x18, &em->pos, em->id, 0, em);
        w->lostTimer = 0x5B;
        w->atkTimer[0] = 450;
        p = em->getPartsPtr(0x2D);
        p->scale.x = 1.0f;
        p->scale.y = 1.0f;
        p->scale.z = 1.0f;
        return 1;
    }
    case 1:
        if (w->flags2 & 0x10) {
            return 0;
        }
        w->flags2 |= 0x10;
        em36PartsSet(em, 1, 1);
        em36CatchEffectDelete(em, w, 1);
        EstSet((int) em, -1, 0, 0, 0x2D, 0x12, 0, 0, (u32) em, 0);
        SndCall(8, 0x18, &em->pos, em->id, 0, em);
        w->lostTimer = 0x5B;
        w->atkTimer[1] = 450;
        return 1;
    case 2:
        if (!(w->flags2 & 1)) {
            w->flags2 |= 1;
            em36PartsSet(em, 2, 1);
            EstSet((int) em, -1, 0, 0, 0x2D, 0x1B, 0, 0, (u32) em, 0);
            EstSet((int) em, -1, 0, 0, 0x2D, 0x35, 0, 0, (u32) em, 0);
            SndCall(8, 0x18, &em->pos, em->id, 0, em);
            w->lostTimer = 0x5B;
            w->atkTimer[2] = 450;
            return 1;
        }
        return 0;
    case 3:
        if (w->flags2 & 2) {
            return 0;
        }
        w->flags2 |= 2;
        em36PartsSet(em, 3, 1);
        EstSet((int) em, -1, 0, 0, 0x2D, 0x1C, 0, 0, (u32) em, 0);
        EstSet((int) em, -1, 0, 0, 0x2D, 0x36, 0, 0, (u32) em, 0);
        SndCall(8, 0x18, &em->pos, em->id, 0, em);
        w->lostTimer = 0x5B;
        w->atkTimer[3] = 450;
        return 1;
    case 4:
        if (w->flags2 & 4) {
            return 0;
        }
        w->flags2 |= 4;
        em36PartsSet(em, 4, 1);
        EstSet((int) em, -1, 0, 0, 0x2D, 0x1D, 0, 0, (u32) em, 0);
        EstSet((int) em, -1, 0, 0, 0x2D, 0x37, 0, 0, (u32) em, 0);
        SndCall(8, 0x18, &em->pos, em->id, 0, em);
        w->lostTimer = 0x5B;
        w->atkTimer[4] = 450;
        return 1;
    case 5:
        if (w->flags2 & 8) {
            break;
        }
        w->flags2 |= 8;
        em36PartsSet(em, 5, 1);
        EstSet((int) em, -1, 0, 0, 0x2D, 0x1E, 0, 0, (u32) em, 0);
        EstSet((int) em, -1, 0, 0, 0x2D, 0x38, 0, 0, (u32) em, 0);
        SndCall(8, 0x18, &em->pos, em->id, 0, em);
        w->lostTimer = 0x5B;
        w->atkTimer[5] = 450;
        return 1;
    }
    return 0;
}

int em36RegeneCk(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    if (w->atkTimer[2] != 0 || w->atkTimer[3] != 0) {
        return 0;
    }
    if (w->flags2 & 3) {
        EmRoutineSet(em, 1, 0x17, 0, 0);
        return 1;
    }
    return 0;
}

void em36RegeneCk2(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    if (em->hp <= 0) {
        return;
    }
    if (w->flags2 & 0x10) {
        if (w->atkTimer[1] == 9) {
            EstSet((int) em, -1, 0, 0, 0x2D, 0x39, 0, 0, (u32) em, 0);
        }
        if (w->atkTimer[1] == 0) {
            w->flags2 &= ~0x10;
            em36PartsSet(em, 1, 0);
            em36RegeneTenClear(em, 1);
            SndCall(8, 0x17, &em->pos, em->id, 0, em);
            switch (em->type) {
            case 0:
            case 1:
            default:
                EstSet((int) em, -1, 0, 0, 0x2D, 8, 1, w->espKind[1], (u32) em, 0);
                break;
            case 2:
            case 3:
                break;
            }
        }
    }
    if (w->flags2 & 0x20) {
        if (w->atkTimer[0] <= 45) {
            cModel* p = em->getPartsPtr(0x2D);

            p->scale.x = p->scale.x * 0.92f + 0.004f;
            p->scale.y = p->scale.y * 0.92f + 0.004f;
        }
        if (w->atkTimer[0] == 9) {
            EstSet((int) em, -1, 0, 0, 0x2D, 0x3A, 0, 0, (u32) em, 0);
        }
        if (w->atkTimer[0] == 0) {
            w->flags2 &= ~0x20;
            em36PartsSet(em, 0, 0);
            em36RegeneTenClear(em, 0);
            SndCall(8, 0x17, &em->pos, em->id, 0, em);
        }
    }
    if (w->lostTimer != 0) {
        if ((s16) (w->lostTimer % 30) == 1) {
            SndCall(8, 0x27, &em->pos, em->id, 0, em);
        }
        w->lostTimer--;
    }
}

// The hit boxes of the lost limbs are switched off.
void em36YarareCk(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    if (w->flags2 & 0x10) {
        w->hit[13].flags &= ~1;
        w->hit[14].flags &= ~1;
        w->hit[15].flags &= ~1;
    } else {
        w->hit[13].flags |= 1;
        w->hit[14].flags |= 1;
        w->hit[15].flags |= 1;
    }
    if (w->flags2 & 0x20) {
        em->hitInfo.flags &= ~1;
        w->hit[5].flags &= ~1;
        w->hit[6].flags &= ~1;
    } else {
        em->hitInfo.flags |= 1;
        w->hit[5].flags |= 1;
        w->hit[6].flags |= 1;
    }
    if (w->flags2 & 1) {
        w->hit[21].flags &= ~1;
        w->hit[22].flags &= ~1;
        w->hit[23].flags &= ~1;
        w->hit[24].flags &= ~1;
    } else {
        w->hit[21].flags |= 1;
        w->hit[22].flags |= 1;
        w->hit[23].flags |= 1;
        w->hit[24].flags |= 1;
    }
    if (w->flags2 & 2) {
        w->hit[26].flags &= ~1;
        w->hit[27].flags &= ~1;
        w->hit[28].flags &= ~1;
        w->hit[29].flags &= ~1;
    } else {
        w->hit[26].flags |= 1;
        w->hit[27].flags |= 1;
        w->hit[28].flags |= 1;
        w->hit[29].flags |= 1;
    }
    if (w->flags2 & 4) {
        w->hit[17].flags &= ~1;
    } else {
        w->hit[17].flags |= 1;
    }
    if (w->flags2 & 8) {
        w->hit[19].flags &= ~1;
    } else {
        w->hit[19].flags |= 1;
    }
}

// The catch check body (em36CatchCk / em36BiteCk differ only in the reach; a macro keeps the reach
// constant at its compare instead of an inline parameter pseudo loaded at the entry).
#define EM36_CATCH_AREA_CK(zmax) \
    Em36Work* w = EM36_WK(em); \
    Mtx inv; \
    Vec lp; \
    Vec a; \
    Vec b; \
    Mtx m; \
    if (em36DeadCk(pPL)) { \
        return 0; \
    } \
    if ((s16) pG->pl_life <= 0) { \
        return 0; \
    } \
    if (em->hp <= 0) { \
        return 0; \
    } \
    if (!(em->seFlags28B & 2)) { \
        return 0; \
    } \
    if (!(w->flags & 1)) { \
        return 0; \
    } \
    if (pG->flags_5010 & 0x8000) { \
        return 0; \
    } \
    if (!(w->flags & 0x800)) { \
        return 0; \
    } \
    PSMTXInverse(em->mat, inv); \
    PSMTXMultVec(inv, &pPL->pos, &lp); \
    if (lp.y < -500.0f || lp.y > 500.0f) { \
        return 0; \
    } \
    if (lp.z < 0.0f || lp.z > zmax) { \
        return 0; \
    } \
    if (!(lp.x > -400.0f && lp.x < 400.0f)) { \
        return 0; \
    } \
    a = em->pos; \
    b = pPL->pos; \
    a.y += 500.0f; \
    b.y += 500.0f; \
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) { \
        return 0; \
    } \
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) { \
        return 0; \
    } \
    PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &pPL->pos)); \
    TransMatrix(m, &em->pos); \
    a.x = 300.0f; \
    a.y = 500.0f; \
    a.z = 0.0f; \
    b.x = 300.0f; \
    b.y = 500.0f; \
    b.z = 500.0f; \
    PSMTXMultVec(m, &a, &a); \
    PSMTXMultVec(m, &b, &b); \
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) { \
        return 0; \
    } \
    a.x = 300.0f; \
    a.y = 500.0f; \
    a.z = 0.0f; \
    b.x = 300.0f; \
    b.y = 500.0f; \
    b.z = 500.0f; \
    PSMTXMultVec(m, &a, &a); \
    PSMTXMultVec(m, &b, &b); \
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) { \
        return 0; \
    } \
    pPL->dmg.set(0, 2); \
    em->dmg.set(0, 2); \
    VibSetData(VIB_TBL, 7, 1); \
    return 1;


int em36CatchCk(cEm36* em)
{
    EM36_CATCH_AREA_CK(1700.0f);
}

int em36BiteCk(cEm36* em)
{
    EM36_CATCH_AREA_CK(1000.0f);
}

int em36LongCatchCk(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    Vec a;
    Vec b;
    Mtx m;
    cModel* p;
    f32 len;

    if (em36DeadCk(pPL)) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (em->hp <= 0) {
        return 0;
    }
    if (!(em->seFlags28B & 2)) {
        return 0;
    }
    if (!(w->flags & 1)) {
        return 0;
    }
    if (pG->flags_5010 & 0x8000) {
        return 0;
    }
    if (!(w->flags & 0x800)) {
        return 0;
    }
    p = em->getPartsPtr(10);
    if ((p->worldPos.x - pPL->pos.x) * (p->worldPos.x - pPL->pos.x) +
            (p->worldPos.z - pPL->pos.z) * (p->worldPos.z - pPL->pos.z) > 250000.0f) {
        return 0;
    }
    if (fabsf(em->pos.y - pPL->pos.y) > 500.0f) {
        return 0;
    }
    a = em->pos;
    b = pPL->pos;
    a.y += 500.0f;
    b.y += 500.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    len = SQRTF((em->pos.x - pPL->pos.x) * (em->pos.x - pPL->pos.x) + (em->pos.z - pPL->pos.z) * (em->pos.z - pPL->pos.z));
    PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &pPL->pos));
    TransMatrix(m, &em->pos);
    a.x = 300.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 300.0f;
    b.y = 500.0f;
    b.z = len;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    a.x = 300.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 300.0f;
    b.y = 500.0f;
    b.z = len;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    pPL->dmg.set(0, 2);
    em->dmg.set(0, 2);
    VibSetData(VIB_TBL, 7, 1);
    return 1;
}

int em36BetweenHitCk(cEm36* em)
{
    Vec a;
    Vec b;
    Vec* pa = &a;
    Vec* pb = &b;

    a = em->pos;
    b = pPL->pos;
    a.y += 500.0f;
    b.y += 500.0f;
    if (SatMgr.hitCheck(pa, pb, 0, 0, 0, 0)) {
        return 1;
    }
    if (EatMgr.hitCheck(pa, pb, 0, 0, 0, 0)) {
        return 1;
    }
    return 0;
}

void em36WeakInit(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    EmListData* list = EM_LIST(em->emsetNo);
    int sum = 0;
    int a;
    int b;
    u32 i;

    a = Rnd() & 3;
    b = Rnd() & 3;
    if ((u8) (Rnd() % 10) > 4 || pG->x4F88 > 7) {
        b = a;
    }
    if (pG->x4F88 <= 2) {
        u32 r;

        // Region end after the Rnd chain (zero code) + else-arm 3: jump.c hoists `a = 3` right before the
        // branch and sched1 leaves it after the `cmplwi` instead of pairing it with the `lis` after the call.
        do {
            r = (u8) (Rnd() % 10);
        } while (0);
        if (r <= 4) {
            a = 2;
        } else {
            a = 3;
        }
    }
    {
        u32 f = em->flags_3C8;

        if ((f & 0x04000000) && !(em->flags_3C8 & 0xF8000000)) {
            em->flags_3C8 |= 0x08000000;
        }
    }
    for (i = 0; i < 5; i++) {
        Em36Limb* l = &w->limb[i];
        u32 bit = 0x80000000 >> i;

        if (em->flags_3C8 & 0x04000000) {
            if ((em->flags_3C8 & bit) == 0) {
                continue;
            }
        } else {
            em->flags_3C8 &= ~bit;
            switch (em->type) {
            case 0:
            case 1:
                if (i == a || i == b) {
                    continue;
                }
                if (pG->x4F88 <= 9 && i == 4) {
                    continue;
                }
                break;
            }
        }
        l->pObj = SetObj00(ARC(0x81), ARC(0x82), &em36_weak_pos[i], &em36_weak_rot[i]);
        if (l->pObj) {
            l->pObj->lightInfo.x50 = 0x80;
            OyaSetObj00(l->pObj, em, em36_weak_parts[i]);
            MotSetObj00(l->pObj, ARC(0x83), 4, 0);
            l->pObj->atari.throughOn();
        }
        l->hp = 1000;
        switch (i) {
        case 0:
        default:
            l->hit = 0;
            break;
        case 1:
            l->hit = i;
            break;
        case 2:
            l->hit = i;
            break;
        case 3:
            l->hit = i;
            break;
        case 4:
            l->hit = i;
            break;
        }
        w->hit[l->hit].flags |= 1;
        sum += 1000;
        em->flags_3C8 |= bit;
    }
    em->flags_3C8 |= 0x04000000;
    list->flags4 = em->flags_3C8;
    em->hp = sum;
    em->hpMax = sum;
}

void em36WeakMove(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    u32 i;

    for (i = 0; i < 5; i++) {
        Em36Limb* l = &w->limb[i];

        if (l->pObj) {
            Mtx inv;
            Vec v;

            PSMTXInverse(em->getPartsPtr(0)->mat, inv);
            PSMTXMultVec(inv, &l->pObj->getPartsPtr(0)->worldPos, &v);
            w->hit[l->hit].ofs = v;
            if ((pGS->flags_5010 & 0x04000000) && l->hp > 0 && em->hp > 0) {
                l->pObj->be_flag |= 2;
                w->hit[l->hit].flags |= 1;
            } else {
                l->pObj->be_flag &= ~2;
                w->hit[l->hit].flags &= ~1;
            }
        }
    }
}

void em36SlopeMove(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    Vec a;
    Vec b;
    Vec v;
    Mtx m;
    f32 ang;
    f32 tilt; // one f32 for the half-length, the SQRTF result and the tilt: the `fmr f2,tilt`
              // atan2f argument copy gives the pseudo an f2 copy preference in global alloc,
              // which is where all three values sit in the target (a block-local `h` is
              // local-alloc'd to f13 and `tilt` alone falls to f12)

    if (w->flags & 0x80) {
        f32 fa;
        f32 fb;

        tilt = em->atari.rectX * em->scale.z - 100.0f;
        b.z = -tilt;
        a.z = tilt;
        a.x = 0.0f;
        a.y = 1000.0f;
        b.x = 0.0f;
        b.y = 1000.0f;
        PSMTXMultVec(em->mat, &a, &a);
        PSMTXMultVec(em->mat, &b, &b);
        fa = SatMgr.getFloor(&a, 600.0f, 100000.0f, 0, 0);
        fb = SatMgr.getFloor(&b, 600.0f, 100000.0f, 0, 0);
        if (fa == -100000.0f) {
            fa = em->pos.y;
        }
        if (fb == -100000.0f) {
            fb = em->pos.y;
        }
        fa -= fb;
        if (fa > 1500.0f) {
            fa = 1500.0f;
        }
        if (fa < -1500.0f) {
            fa = -1500.0f;
        }
        tilt = SQRTF((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
        ang = -atan2f(fa, tilt);
        if (w->slopeTimer) {
            w->slopeTimer--;
        }
    } else {
        ang = 0.0f;
        w->slopeTimer = 30;
    }
    w->slopeRot.x = w->slopeRot.x * 0.95f + ang * 0.05f;
    RotMatrix(m, &w->slopeRot);
    tilt = w->slopeRot.x;
    if (tilt > 0.0f) {
        tilt -= 0.1f;
        if (tilt < 0.0f) {
            tilt = 0.0f;
        }
    } else {
        tilt += 0.1f;
        if (tilt > 0.0f) {
            tilt = 0.0f;
        }
    }
    if (w->slopeTimer == 0 || w->slopeTimer == 30) {
        tilt = 0.0f;
    }
    // `tilt *= 150` as its own statement and slopeLift*0.9 written first: the fmadds fuses the
    // 0.1 product (`fmuls f2,f2,f13; fmuls f0,f2,f0; fmadds f13,f13,f12,f0`)
    tilt *= 150.0f;
    w->slopeLift = w->slopeLift * 0.9f + tilt * 0.1f;
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = w->slopeLift;
    PSMTXMultVecSR(em->mat, &v, &v);
    PSVECAdd(&em->pos, &v, &em->pos);
    PSMTXConcat(em->mat, m, em->mat);
    TransMatrix(em->mat, &em->pos);
}

int em36SetDmVal(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    EmHitInfo* part = em->dmPart;
    EmListData* list = EM_LIST(em->emsetNo);
    int near;
    int dmg;

    near = 0;
    if (part->rad < 16000000.0f) {
        near = 1;
    }
    dmg = 100;
    if ((u32) em->dmWep <= 0x2D) {
        dmg = GetWepDmVal(em, em->dmWep, near);
    }
    switch (em->dmWep) {
    case 9:
    case 0xA:
    case 0x28:
        if (pG->flags_5010 & 0x04000000) {
            int sum = 0;
            int i;

            for (i = 0; i < 5; i++) {
                Em36Limb* l = &w->limb[i];

                if (l->pObj) {
                    u32 bit = 0x80000000 >> i;

                    if (part == &w->hit[l->hit]) {
                        l->hp = 0;
                        dmg = 1000;
                        em->flags_3C8 &= ~bit;
                        list->flags4 = em->flags_3C8;
                    }
                    sum += l->hp;
                }
            }
            if (sum <= 0) {
                dmg = em->hp;
            }
        }
        break;
    }
    return dmg;
}

void cEm36::setR307Appear()
{
    EmRoutineSet(this, 1, 0x1B, 0, 0);
}

// The hit mark effect of the damage part, in the parts' local frame.
void em36SetHitMark(cEm36* em, int big)
{
    Em36Work* w = EM36_WK(em);
    EmHitInfo* part = em->dmPart;
    Vec lp;
    Vec rot;
    Mtx inv;
    Vec wp;
    Vec lp2;
    Vec d;
    MtxPtr m;
    EspSeqData* seq;
    // The original zero-extends the u8 once (`clrlwi r30`) before both calls; a hard-register QImode variable
    // keeps the extension (combine drops it for a pseudo whose sets are the constants 3/4).
    register u8 kind asm("r30"); // COMPILER-DIFF: #2
    u32 i;
    f32 len;

    m = em->getPartsPtr(part->partsNo - 1)->mat;
    PSMTXInverse(m, inv);
    PSMTXMultVec(inv, &part->pos, &lp);
    PSMTXMultVec(m, &part->ofs, &wp);
    PSMTXCopy(m, inv);
    PSMTXInverse(m, inv);
    PSMTXMultVec(inv, &part->pos, &lp2);
    if (part->flags & 2) {
        wp = lp2;
        wp.y = 0.0f;
        wp.z = 0.0f;
        PSVECSubtract(&lp2, &wp, &d);
        len = SQRTF(d.x * d.x + d.z * d.z);
        rot.x = -atan2f(d.y, len) * 57.295776f;
        rot.y = 0.0f;
        rot.z = 0.0f;
    } else {
        wp = lp2;
        wp.x = 0.0f;
        wp.z = 0.0f;
        PSVECSubtract(&lp2, &wp, &d);
        rot.x = 0.0f;
        rot.y = atan2f(d.x, d.z) * 57.295776f;
        rot.z = 0.0f;
    }
    kind = 4;
    if (big) {
        kind = 3;
    }
    seq = EspGetEstAddr(0x2D, kind, 1);
    if (seq) {
        for (i = 0; i < seq->num; i++) {
            EspGenWork* r = &seq->rec[i];

            r->pos = lp;
            r->x7 = part->partsNo - 1;
            if (r->x1 != 0x16) {
                r->x58 = rot;
            }
        }
    }
    EstSet((int) em, -1, 0, 0, 0x2D, kind, 1, w->espKind[3], (u32) em, 0);
}

// The doors in front are opened.
void em36DoorOpenCk(cEm36* em)
{
    Vec lp;
    u32 i;
    u32 r;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEmDoor* e = (cEmDoor*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        EmDoorWork* dw;
        f32 ang;

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
                (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) > 6250000.0f) {
            continue;
        }
        dw = EMDOOR_WK(e);
        ang = fabsf(Muku2(dw->rotY, em->rot.y, PI));
        if (ang > 0.7853982f && ang < 2.3561945f) {
            continue;
        }
        PSMTXMultVec(dw->inv, &em->pos, &lp);
        if (ang < 1.5707964f) {
            if (lp.z > 0.0f || lp.z < -800.0f) {
                continue;
            }
        } else {
            if (lp.z < 0.0f || lp.z > 800.0f) {
                continue;
            }
        }
        if (lp.x > dw->width || lp.x < -dw->width) {
            continue;
        }
        if (lp.y > 500.0f || lp.y < -500.0f) {
            continue;
        }
        r = e->ckOpen();
        if (r) {
            if (r <= 3) {
                continue;
            }
        }
        e->setOpen(&em->pos, 0, 0, 0);
    }
}

int em36AtkRtnCk(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    Vec v;
    f32 dy;
    f32 d;

    if ((w->flags & 4) && pSUB) {
        if ((w->flags2 & 0x10) || pG->x4F88 <= 2) {
            v = pSUB->pos;
        } else {
            GetPlPos(&v, pSUB, 18.0f);
        }
        d = (em->pos.x - v.x) * (em->pos.x - v.x) + (em->pos.z - v.z) * (em->pos.z - v.z);
        dy = em->pos.y - pSUB->pos.y;
        dy = fabsf(dy);
        if ((w->flags & 2) && d < 2250000.0f && dy < 500.0f) {
            if ((w->flags2 & 3) != 3) {
                EmRoutineSet(em, 1, 8, 0, 0);
                return 1;
            }
            EmRoutineSet(em, 1, 0x17, 0, 0);
            return 1;
        }
        return 0;
    }
    if ((w->flags2 & 0x10) || pG->x4F88 <= 2) {
        v = pPL->pos;
    } else {
        GetPlPos(&v, 0, 18.0f);
    }
    d = (em->pos.x - v.x) * (em->pos.x - v.x) + (em->pos.z - v.z) * (em->pos.z - v.z);
    dy = em->pos.y - pPL->pos.y;
    dy = fabsf(dy);
    if (w->findWait == 0 && (w->flags & 1) && !(w->flags2 & 3) && d > 2250000.0f && d < 12250000.0f && dy < 250.0f &&
        (w->flags & 0x800) && w->routeAngAbs < 0.7853982f) {
        switch (em->type) {
        case 0:
        case 1:
        default:
            if (pG->x4F88 > 9) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
                return 1;
            }
            if ((u8) (Rnd() % 10) > 7 && pG->x4F88 > 1) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
                return 1;
            }
            break;
        case 2:
        case 3:
            if (pG->x4F88 > 9) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
                return 1;
            }
            if ((u8) (Rnd() % 10) > 3 && pG->x4F88 > 1) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
                return 1;
            }
            break;
        }
        em36SetFindWait(w);
    }
    if (!(d < 2250000.0f)) {
        return 0;
    }
    if (!(dy < 500.0f)) {
        return 0;
    }
    if ((w->flags & 1) && !(w->flags2 & 3) && (u8) (Rnd() % 10) > 4 && (w->flags & 0x804) == 0x800) {
        switch (em->type) {
        case 0:
        case 1:
        default:
            EmRoutineSet(em, 1, 0xA, 0, 0);
            return 1;
        case 2:
        case 3:
            EmRoutineSet(em, 1, 9, 0, 0);
            return 1;
        }
    }
    if ((w->flags & 0x1000) && (w->flags2 & 3) != 3) {
        EmRoutineSet(em, 1, 8, 0, 0);
        return 1;
    }
    switch (em->type) {
    case 0:
    case 1:
    default:
        if ((w->flags & 1) && (w->flags2 & 3)) {
            EmRoutineSet(em, 1, 0xF, 0, 0);
            return 1;
        }
        break;
    case 2:
    case 3:
        EmRoutineSet(em, 1, 9, 0, 0);
        return 1;
    }
    return 0;
}

void em36PartsSet(cEm36* em, int no, int on)
{
    Em36Work* w = EM36_WK(em);
    void* bin;
    cModelInfo* info;

    switch (em->type) {
    case 0:
    case 1:
    default:
        switch ((u32) no) {
        case 0:
            if (on == 0) {
                bin = ARC(5);
            } else {
                bin = ARC(6);
            }
            break;
        case 1:
            if (on == 0) {
                bin = ARC(7);
            } else {
                bin = ARC(8);
            }
            break;
        case 2:
            if (on == 0) {
                bin = ARC(9);
            } else {
                bin = ARC(0xA);
            }
            break;
        case 3:
            if (on == 0) {
                bin = ARC(0xB);
            } else {
                bin = ARC(0xC);
            }
            break;
        case 4:
            if (on == 0) {
                bin = ARC(0xD);
            } else {
                bin = ARC(0xE);
            }
            break;
        case 5:
            if (on == 0) {
                bin = ARC(0xF);
            } else {
                bin = ARC(0x10);
            }
            break;
        case 6:
            if (on == 0) {
                bin = ARC(0x11);
            } else {
                bin = ARC(0x12);
            }
            break;
        default:
            return;
        }
        break;
    case 2:
    case 3:
        switch ((u32) no) {
        case 0:
            if (on == 0) {
                bin = ARC(0x16);
            } else {
                bin = ARC(0x17);
            }
            break;
        case 1:
            if (on == 0) {
                bin = ARC(0x18);
            } else {
                bin = ARC(0x19);
            }
            break;
        case 2:
            if (on == 0) {
                bin = ARC(0x1A);
            } else {
                bin = ARC(0x1B);
            }
            break;
        case 3:
            if (on == 0) {
                bin = ARC(0x1C);
            } else {
                bin = ARC(0x1D);
            }
            break;
        case 4:
            if (on == 0) {
                bin = ARC(0x1E);
            } else {
                bin = ARC(0x1F);
            }
            break;
        case 5:
            if (on == 0) {
                bin = ARC(0x20);
            } else {
                bin = ARC(0x21);
            }
            break;
        case 6:
            if (on == 0) {
                bin = ARC(0x22);
            } else {
                bin = ARC(0x23);
            }
            break;
        default:
            return;
        }
        break;
    }
    // The create call is INSIDE each type arm: jump2 cross-jumps the three identical tails
    // (`lis/addi ModInfoMgr; add r5; bl create; mr r31,r3`) into one copy that falls into the join,
    // so the result copy and the join's `cmpwi r31,0` are in different blocks at combine time (no
    // `mr.` fusion), `info` is a multi-set pseudo (takes r31, em falls to r30), and the merged tail
    // is ordered by sched2 (`lis; addi; add`). One call after a tpl-offset switch gives `mr. r30,r3`.
    switch (em->type) {
    case 0:
    default:
        info = ModInfoMgr.create(bin, ARC(0x13));
        break;
    case 1:
        info = ModInfoMgr.create(bin, ARC(0x14));
        break;
    case 2:
    case 3:
        info = ModInfoMgr.create(bin, ARC(0x24));
        break;
    }
    if (info) {
        if (w->pParts[no]) {
            em->swapModelInfo(w->pParts[no]->pData, info);
        } else {
            em->addModel(info);
        }
        w->pParts[no] = info;
        info->be_flag |= 8;
    }
}

// The tentacle positions / rotations (three per limb) and their parts. Two-dimensional: the
// [i][k] address is a direct pass-1 giv of the k loop (`base + i*36 + k*12`), so its increment is
// created before the obj-index giv and the latch order is `k, rot, pos, ofs, obj`; a flat
// `[i * 3 + k]` index is a giv-of-a-giv that only the second loop pass reduces (obj first).
static Vec em36_ten_pos[7][3] = {
    { { 0.0f, 0.0f, 0.0f }, { -120.0f, 150.0f, 0.0f }, { 120.0f, 150.0f, 0.0f } },
    { { 0.0f, 0.0f, 50.0f }, { -50.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 20.0f }, { -20.0f, 0.0f, 0.0f }, { 20.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 20.0f }, { -20.0f, 0.0f, 0.0f }, { 20.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
};
static Vec em36_ten_rot[7][3] = {
    { { 0.5235988f, 0.0f, 0.0f }, { 2.3561945f, 0.5235988f, 0.0f }, { 2.3561945f, -0.5235988f, 0.0f } },
    { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { { 0.0f, 0.0f, 1.5707964f }, { 0.0f, 0.0f, 1.5707964f }, { 0.0f, 0.0f, 1.5707964f } },
    { { 0.0f, 0.0f, -1.5707964f }, { 0.0f, 0.0f, -1.5707964f }, { 0.0f, 0.0f, -1.5707964f } },
    { { 0.0f, 0.0f, PI }, { 0.0f, 0.0f, PI }, { 0.0f, 0.0f, PI } },
    { { 0.0f, 0.0f, PI }, { 0.0f, 0.0f, PI }, { 0.0f, 0.0f, PI } },
    { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
};
static u8 em36_ten_parts[6] = { 1, 4, 7, 0xD, 0x13, 0x17 };

// The effects of the lost limbs while they grow back, and the tentacles growing out of them
// when the regrowth timer reaches 300.
void em36RegeneTenMove(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    u16 step;
    int any;
    u32 i;

    if (em->hp <= 0) {
        return;
    }
    any = 0;
    step = (*(u16*) ARC(0x8F) & 0x3FFF) / 4;
    for (i = 0; i < 7; i++) {
        u32 k;

        switch (i) {
        case 0:
            if (!(w->flags2 & 0x20)) {
                continue;
            }
            if (w->atkTimer[0] <= 300) {
                EstSet((int) em, -1, 0, 0, 0x2D, 0x21, 0, 0, (u32) em, 0);
                any = 1;
                if ((u8) (w->frameCnt % 3) == 0) {
                    EstSet((int) em, -1, 0, 0, 0x2D, 0x23, 0, 0, (u32) em, 0);
                }
            } else {
                if ((u8) (w->frameCnt % 3) == 0) {
                    EstSet((int) em, -1, 0, 0, 0x2D, 0x2A, 0, 0, (u32) em, 0);
                }
            }
            if (w->atkTimer[0] != 300) {
                continue;
            }
            SndCall(8, 0x13, &em->pos, em->id, 0, em);
            w->tenSeTimer = 29;
            EstSet((int) em, -1, 0, 0, 0x2D, 0x22, 0, 0, (u32) em, 0);
            continue;
        case 1:
            if (!(w->flags2 & 0x10)) {
                continue;
            }
            if (w->atkTimer[1] <= 300) {
                any = 1;
                if ((u8) (w->frameCnt % 3) == 0) {
                    EstSet((int) em, -1, 0, 0, 0x2D, 0x20, 0, 0, (u32) em, 0);
                }
            } else {
                if ((u8) (w->frameCnt % 3) == 0) {
                    EstSet((int) em, -1, 0, 0, 0x2D, 0x29, 0, 0, (u32) em, 0);
                }
            }
            if (w->atkTimer[1] != 300) {
                continue;
            }
            SndCall(8, 0x13, &em->pos, em->id, 0, em);
            w->tenSeTimer = 29;
            EstSet((int) em, -1, 0, 0, 0x2D, 0x1F, 0, 0, (u32) em, 0);
            break;
        case 2:
            if (!(w->flags2 & 1)) {
                continue;
            }
            if (w->atkTimer[2] <= 300) {
                any = 1;
                if ((u8) (w->frameCnt % 3) == 0) {
                    EstSet((int) em, -1, 0, 0, 0x2D, 0x14, 0, 0, (u32) em, 0);
                }
            } else {
                if ((u8) (w->frameCnt % 3) == 0) {
                    EstSet((int) em, -1, 0, 0, 0x2D, 0x25, 0, 0, (u32) em, 0);
                }
            }
            if (w->atkTimer[2] != 300) {
                continue;
            }
            SndCall(8, 0x13, &em->pos, em->id, 0, em);
            w->tenSeTimer = 29;
            EstSet((int) em, -1, 0, 0, 0x2D, 0x13, 0, 0, (u32) em, 0);
            break;
        case 3:
            if (!(w->flags2 & 2)) {
                continue;
            }
            if (w->atkTimer[3] <= 300) {
                any = 1;
                if ((u8) (w->frameCnt % 3) == 0) {
                    EstSet((int) em, -1, 0, 0, 0x2D, 0x16, 0, 0, (u32) em, 0);
                }
            } else {
                if ((u8) (w->frameCnt % 3) == 0) {
                    EstSet((int) em, -1, 0, 0, 0x2D, 0x26, 0, 0, (u32) em, 0);
                }
            }
            if (w->atkTimer[3] != 300) {
                continue;
            }
            SndCall(8, 0x13, &em->pos, em->id, 0, em);
            w->tenSeTimer = 29;
            EstSet((int) em, -1, 0, 0, 0x2D, 0x15, 0, 0, (u32) em, 0);
            break;
        case 4:
            if (!(w->flags2 & 4)) {
                continue;
            }
            if (w->atkTimer[4] <= 300) {
                any = 1;
                if ((u8) (w->frameCnt % 3) == 0) {
                    EstSet((int) em, -1, 0, 0, 0x2D, 0x18, 0, 0, (u32) em, 0);
                }
            } else {
                if ((u8) (w->frameCnt % 3) == 0) {
                    EstSet((int) em, -1, 0, 0, 0x2D, 0x27, 0, 0, (u32) em, 0);
                }
            }
            if (w->atkTimer[4] != 300) {
                continue;
            }
            SndCall(8, 0x13, &em->pos, em->id, 0, em);
            w->tenSeTimer = 29;
            EstSet((int) em, -1, 0, 0, 0x2D, 0x17, 0, 0, (u32) em, 0);
            break;
        case 5:
            if (!(w->flags2 & 8)) {
                continue;
            }
            if (w->atkTimer[5] <= 300) {
                any = 1;
                if ((u8) (w->frameCnt % 3) == 0) {
                    EstSet((int) em, -1, 0, 0, 0x2D, 0x1A, 0, 0, (u32) em, 0);
                }
            } else {
                if ((u8) (w->frameCnt % 3) == 0) {
                    EstSet((int) em, -1, 0, 0, 0x2D, 0x28, 0, 0, (u32) em, 0);
                }
            }
            if (w->atkTimer[5] != 300) {
                continue;
            }
            SndCall(8, 0x13, &em->pos, em->id, 0, em);
            w->tenSeTimer = 29;
            EstSet((int) em, -1, 0, 0, 0x2D, 0x19, 0, 0, (u32) em, 0);
            break;
        default:
            continue;
        }
        for (k = 0; k < 3; k++) {
            if (w->ten[i].obj[k] == 0) {
                Vec pos;
                Vec rot;
                cObj* obj;

                pos = em36_ten_pos[i][k];
                rot = em36_ten_rot[i][k];
                obj = SetObj16(ARC(0x8D), ARC(0x8E), em, em, em36_ten_parts[i], 0x11, &pos, &rot);
                if (obj) {
                    // `step * k` (a giv, its `li 0` init emitted after the hoisted invariants and
                    // the increment `add ofs,ofs,step` at the latch) -- an `ofs += step` counter
                    // puts `li ofs,0` before the k-loop preheader's `lis/addi` and allocates it r20.
                    MotSetObj16(obj, ARC(0x8F), 4, step * k);
                    w->ten[i].obj[k] = (cObj16*) obj;
                }
            }
        }
    }
    if (any) {
        if (w->tenSeTimer) {
            w->tenSeTimer--;
        } else {
            w->tenSeTimer = 29;
            SndCall(8, 0x14, &em->pos, em->id, 0, em);
        }
    }
}

void em36RegeneTenClear(cEm36* em, int no)
{
    Em36Work* w = EM36_WK(em);
    u32 k;

    for (k = 0; k < 3; k++) {
        if (w->ten[no].obj[k]) {
            w->ten[no].obj[k]->clearLostWait();
            w->ten[no].obj[k] = 0;
        }
    }
}

void em36BloodSet(cEm36* em)
{
    int near = 0;

    if (em->dmPart->rad < 36000000.0f) {
        near = 1;
    }
    // `default:` at the top of the body, jumping to the normal-blood arm: the default label then
    // sits right after the compare tree, so jump.c inverts the last node's `bgt default; b big`
    // into `ble big` falling into `default: b normal` (the tree's tail `ble B; b D`); a default
    // label on the arm itself gives `bgt default; b big`.
    switch (em->dmWep) {
    default:
        goto normal;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27: {
        Vec pos;
        Vec dir;

        EmDmBloodSet2(em, 0x2D, 2, 0, 0, 0);
        em36SetHitMark(em, 0);
        if ((Rnd() & 3) == 0) {
            if (EmGetDmPos(em, &pos, &dir)) {
                EstSet(0, -1, &pos, 0, 0x2D, 6, 0, 0, 0, 0);
            }
        }
        break;
    }
    case 7:
    case 8:
    case 0x21:
        if (near) {
            goto big;
        }
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xE:
    case 0x11:
    case 0x26:
    case 0x2B:
    normal:
        EmDmBloodSet2(em, 0x2D, 0, 0, 0, 0);
        em36SetHitMark(em, 0);
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x15:
    case 0x18:
    case 0x1C:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
    big:
        EmDmBloodSet2(em, 0x2D, 0xA, 0, 0, 0);
        em36SetHitMark(em, 1);
        break;
    case 0x10:
    case 0x1A:
        EmDmBloodSet2(em, 0x2D, 2, 0, 0, 0);
        break;
    case 0x14:
    case 0x16:
    case 0x17:
    case 0x19:
    case 0x1F:
    case 0x20:
    case 0x2A:
        break;
    }
}

int em36FindCk(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    int find = 0;

    if (w->flags & 0x200) {
        return 0;
    }
    if (w->flags & 1) {
        if (em->plDist2 < 225000000.0f && w->routeAngAbs < 1.0471976f) {
            find = 1;
        }
        if ((int) pG->flags_5010 < 0 && em->plDist2 < 25000000.0f) {
            find = 1;
        }
        if (em->plDist2 < 12250000.0f) {
            find = 1;
        }
    }
    if (pG->flags_5010 & 0x20000000) {
        f32 r;

        switch (pG->bell_stat) {
        case 0:
            r = 25000.0f;
            break;
        case 1:
            r = 25000.0f;
            break;
        default:
            r = 25000.0f;
            break;
        }
        r = 25000.0f;
        if ((em->pos.x - pG->bell_pos.x) * (em->pos.x - pG->bell_pos.x) + (em->pos.y - pG->bell_pos.y) * (em->pos.y - pG->bell_pos.y) +
                (em->pos.z - pG->bell_pos.z) * (em->pos.z - pG->bell_pos.z) < r * r) {
            if ((w->flags & 1) && w->plRouteDis < r) {
                find = 1;
            }
        }
    }
    if ((pG->flags_500C & 0x00800000) && w->plRouteDis < 25000.0f) {
        find = 1;
    }
    if (em36DeadCk(em)) {
        find = 1;
    }
    if (find) {
        w->flags |= 0x200;
        return 1;
    }
    return 0;
}

int cEm36::ckFindPL()
{
    Em36Work* w = EM36_WK(this);

    if (hp <= 0) {
        return 0;
    }
    if (checkStatus(5) && (w->flags & 0x200)) {
        return 1;
    }
    return 0;
}

void cEm36::setFindPL()
{
    Em36Work* w = EM36_WK(this);

    w->flags |= 0x200;
}

int em36JumpDownCk(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    Vec nrm;
    Vec a;
    Vec b;
    Vec* pa = &a;
    Vec* pb = &b;
    f32 ang;

    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = 500.0f;
    PSMTXMultVec(em->mat, pa, pa);
    PSMTXMultVec(em->mat, pb, pb);
    if (SatMgr.hitCheck(pa, pb, 0, &nrm, 0, 0) & 0x102010) {
        ang = atan2f(-nrm.x, -nrm.z);
        if (!(fabsf(Muku2(em->rot.y, ang, PI)) > 0.5235988f)) {
            w->jumpAng = ang;
            EmRoutineSet(em, 1, 4, 0, 0);
            return 1;
        }
    }
    return 0;
}

int em36FanceOverCk(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    Vec a;
    Vec b;
    Vec hit;
    Vec nrm;
    Mtx m;
    Vec e;
    Vec c;
    Vec d;
    int mask;
    u32 rem;
    f32 ang;

    if (pG->flags_5014 & 0x08000000) {
        return 0;
    }
    // Written-out modulo: the second load is cse'd, the subtraction's operand is the dying load temp (r9) and
    // `rem` is set once (lowest global priority -> r24 below SatMgr@ha); `rem % 10` ties the load to rem.
    rem = w->stuckCnt - w->stuckCnt / 10 * 10;
    if (rem != 5) {
        return 0;
    }
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = 800.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (!(SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0) & 0x20)) {
        return 0;
    }
    ang = atan2f(-nrm.x, -nrm.z);
    mask = 0;
    em->rot.y = ang;
    PSMTXRotRad(m, 'y', ang);
    TransMatrix(m, &em->pos);
    c.x = 300.0f;
    c.y = 1200.0f;
    c.z = 0.0f;
    d.x = 300.0f;
    d.y = 1200.0f;
    d.z = 800.0f;
    PSMTXMultVec(m, &c, &c);
    PSMTXMultVec(m, &d, &d);
    if (SatMgr.hitCheck(&c, &d, 0, 0, 0, 0x20)) {
        mask = 1;
    }
    c.x = -300.0f;
    c.y = 1200.0f;
    c.z = 0.0f;
    d.x = -300.0f;
    d.y = 1200.0f;
    d.z = 800.0f;
    PSMTXMultVec(m, &c, &c);
    PSMTXMultVec(m, &d, &d);
    if (SatMgr.hitCheck(&c, &d, 0, 0, 0, 0x20)) {
        mask |= 2;
    }
    if (mask == 3) {
        return 0;
    }
    w->fanceVec = em->pos;
    if (mask & 1) {
        e.x = -300.0f;
        e.y = 0.0f;
        e.z = 0.0f;
        PSMTXMultVec(m, &e, &w->fanceVec);
    }
    if (mask & 2) {
        e.x = 300.0f;
        e.y = 0.0f;
        e.z = 0.0f;
        PSMTXMultVec(m, &e, &w->fanceVec);
    }
    EmRoutineSet(em, 1, rem, 0, 0);
    return 1;
}

void em36BreathSe(cEm36* em)
{
    Em36Work* w = EM36_WK(em);

    if (w->voiceTimer) {
        w->voiceTimer--;
        return;
    }
    w->voiceTimer = (u8) (Rnd() % 30) + 59;
    w->sndId = SndCall(8, 0x1F, &em->pos, em->id, 0, em);
}

void em36BreathSeStopCk(cEm36* em)
{
    u32 no = em->seNo;

    if (no) {
        no--;
        switch (no) {
        case 6:
        case 7:
        case 8:
        case 9:
        case 0xA:
        case 0xB:
        case 0xC:
        case 0xD:
        case 0xE:
        case 0x38:
            em36VoiceSet(em, no, 2);
            em->seNo = 0;
            break;
        }
    }
}

void em36VoiceSet(cEm* em, int no, int timer)
{
    Em36Work* w = EM36_WK(em);

    cModel* p;

    SndStop(w->sndId, 0);
    p = em->getPartsPtr(3);
    w->sndId = SndCall(8, no, &p->worldPos, em->id, 0, em);
    w->voiceTimer = timer;
}

// The spine parts and their scale per regeneration stage (em36SpineScaleMove).
static u8 em36_spine_parts[14] = { 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B };
static Vec em36_spine_scale0[14] = {
    { 3.0f, 3.0f, 3.0f }, { 2.0f, 2.0f, 2.0f }, { 1.5f, 1.5f, 1.5f }, { 4.0f, 4.0f, 4.0f }, { 4.0f, 4.0f, 4.0f },
    { 3.0f, 3.0f, 3.0f }, { 4.0f, 4.0f, 4.0f }, { 4.0f, 4.0f, 4.0f }, { 3.0f, 3.0f, 3.0f }, { 2.0f, 1.5f, 2.0f },
    { 2.0f, 1.5f, 2.0f }, { 2.0f, 1.5f, 2.0f }, { 2.0f, 1.5f, 2.0f }, { 4.0f, 4.0f, 4.0f },
};
static Vec em36_spine_scale1[14] = {
    { 6.0f, 6.0f, 6.0f }, { 2.0f, 2.0f, 2.0f }, { 1.5f, 1.5f, 1.5f }, { 4.0f, 4.0f, 4.0f }, { 4.0f, 4.0f, 4.0f },
    { 3.0f, 3.0f, 3.0f }, { 4.0f, 4.0f, 4.0f }, { 4.0f, 4.0f, 4.0f }, { 3.0f, 3.0f, 3.0f }, { 2.0f, 1.5f, 2.0f },
    { 2.0f, 1.5f, 2.0f }, { 2.0f, 1.5f, 2.0f }, { 2.0f, 1.5f, 2.0f }, { 6.0f, 6.0f, 6.0f },
};
static Vec em36_spine_scale2[14] = {
    { 8.0f, 8.0f, 8.0f }, { 4.0f, 4.0f, 4.0f }, { 3.0f, 3.0f, 3.0f }, { 8.0f, 8.0f, 8.0f }, { 8.0f, 8.0f, 8.0f },
    { 6.0f, 6.0f, 6.0f }, { 8.0f, 8.0f, 8.0f }, { 8.0f, 8.0f, 8.0f }, { 6.0f, 6.0f, 6.0f }, { 4.0f, 3.0f, 4.0f },
    { 4.0f, 3.0f, 4.0f }, { 4.0f, 3.0f, 4.0f }, { 4.0f, 3.0f, 4.0f }, { 8.0f, 8.0f, 8.0f },
};

void em36SpineScaleMove(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    Vec one;
    u32 i;
    int snd;

    switch (em->type) {
    case 0:
    case 1:
    default:
        return;
    case 2:
    case 3:
        break;
    }
    one.x = 1.0f;
    one.y = 1.0f;
    one.z = 1.0f;
    snd = 0;
    for (i = 0; i < 14; i++) {
        cParts* p = (cParts*) em->getPartsPtr(em36_spine_parts[i]);
        int on = 0;

        if (w->flags & 0x2000) {
            on = 1;
        }
        if ((w->flags & 0x10000) && (u32) (em36_spine_parts[i] - 0x31) > 2) {
            on = 0;
        }
        if ((w->flags & 0x20000) && (u32) (em36_spine_parts[i] - 0x34) > 2) {
            on = 0;
        }
        if (on) {
            w->spine[i] = w->spine[i] * 0.8f + 0.2f;
            snd = 1;
        } else {
            w->spine[i] = w->spine[i] * 0.8f;
        }
        if (w->flags & 0x4000) {
            PosToPos(&one, &em36_spine_scale1[i], &p->scale, w->spine[i]);
        } else if (w->flags & 0x8000) {
            PosToPos(&one, &em36_spine_scale2[i], &p->scale, w->spine[i]);
        } else {
            PosToPos(&one, &em36_spine_scale0[i], &p->scale, w->spine[i]);
        }
    }
    if (w->flags & 0x80000) {
        w->flags &= ~0x40000;
        return;
    }
    if (snd) {
        if (!(w->flags & 0x40000)) {
            w->flags |= 0x40000;
            SndCall(8, 0x32, &em->pos, em->id, 0, em);
        }
    } else {
        if (w->flags & 0x40000) {
            w->flags &= ~0x40000;
            SndCall(8, 0x33, &em->pos, em->id, 0, em);
        }
    }
}

int em36CrashCk(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    Vec hit;

    if (em->dmHit != 0) {
        return 0;
    }
    if (em->hp <= 0) {
        return 0;
    }
    if (w->flags & 0x20) {
        return 0;
    }
    if (DmgMgr.hitCheck(&em->pos, &hit) != 3) {
        return 0;
    }
    if (w->flags & 0x100) {
        EmRoutineSet(em, 2, 5, 0, 0);
        return 1;
    }
    if (fabsf(Muku(&em->pos, &hit, em->rot.y, PI)) < 1.5707964f) {
        EmRoutineSet(em, 1, 7, 0, 0);
    } else {
        EmRoutineSet(em, 1, 7, 0, 1);
    }
    w->fanceVec = hit;
    return 1;
}

void em36ScaleCompress(cEm36* em)
{
    Em36Work* w = EM36_WK(em);
    Mtx m;
    Vec s;
    cParts* p;

    if (!(w->flags & 0x100000)) {
        return;
    }
    PSMTXIdentity(m);
    s.x = 1.0f;
    s.y = w->scale;
    s.z = 1.0f;
    ScaleMatrix(m, &s);
    for (p = em->pPartsHead; p; p = p->pNext) {
        PSMTXConcat(m, p->mat, p->mat);
        p->mat[0][3] = p->worldPos.x;
        p->mat[1][3] = p->worldPos.y;
        p->mat[2][3] = p->worldPos.z;
    }
}
