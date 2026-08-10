// em22 module (D:/Bio4/Prog/em22.cpp): the dog. It hunts the player along the route (Em22RouteCk,
// em22_R1_Run / Turn / Threat / SideStep), jumps at him (em22_R1_JumpAtk, plem22_JumpAtkHit), escapes
// when hurt (em22_R1_Escape) and grows parasites out of its back (em22SetParasite, em22_R1_ParaAtk).

#include "atari.h"
#include "ctrl.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "dmg.h"
#include "em22.h"
#include "em10.h"
#include "emdoor.h"
#include "obj16.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "item.h"
#include "pl_wep.h"
#include "dbmodule.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "route_ck.h"
#include "game.h"
#include "snd.h"
#include "pad.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

// The module's 0x34-byte COMMON block: uninitialised template statics of the original object,
// merged into .bss by the REL link.
asm(".comm common_em22,52,4");

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// em_set.h declares EmSetDieCnt without arguments; this module passes the enemy.
void EmSetDieCntE(cEm* em) asm("EmSetDieCnt");

typedef void (*Em22Func)(cEm22*);

static void em22_R0_Init(cEm22* em);
static void em22_R0_Move(cEm22* em);
static void em22_R1_br_dummy(cEm22* em);
static void em22_R1_Goto(cEm22* em);
static void em22_R1_R11B_A(cEm22* em);
static void em22_R1_R11B_B(cEm22* em);
static void em22_R1_R11B_C(cEm22* em);
static void em22_R1_InCage(cEm22* em);
static void em22_R1_JumpWait(cEm22* em);
static void em22_R1_Wait(cEm22* em);
static void em22_R1_RunAbout(cEm22* em);
static void em22_R1_Run(cEm22* em);
static void em22_R1_Turn(cEm22* em);
static void em22_R1_Escape(cEm22* em);
static void em22_R1_Threat(cEm22* em);
static void em22_R1_SideStep(cEm22* em);
static void em22_R1_br_JumpAtk(cEm22* em);
static void em22_R1_JumpAtk(cEm22* em);
static void em22_R1_JumpAtkHit(cEm22* em);
static void plem22_JumpAtkHit(cPlayer* pl);
static void em22_R1_br_ParaAtk(cEm22* em);
static void em22_R1_ParaAtk(cEm22* em);
static void em22_R1_ParaAtkHit(cEm22* em);
static void plem22_ParaAtkHit(cPlayer* pl);
static void em22_R1_Wakeup(cEm22* em);
static void em22_R1_Parasite(cEm22* em);
static void em22_R1_Jump(cEm22* em);
static void em22_R0_Damage(cEm22* em);
static void em22_R1_Dm_Small(cEm22* em);
static void em22_R1_Dm_Blow(cEm22* em);
static void em22_R0_Die(cEm22* em);
static void em22_R1_Die_Lost(cEm22* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)
#define PL_ARC(no) PL_ARC_PTR(pl->subArc, no)

// The enemy a player damage callback belongs to (pl_sub SetPlDamage's first argument).
#define PL_EM(pl) ((cEm22*) (pl)->dmgType)
// The same through the global player pointer (the catch callbacks read it that way).
#define PL_EM_G ((cEm22*) pPL->dmgType)

// Struct-member views of the player / partner / pG pointers: a load through them is not hoisted above
// the preceding stores through the work pointer (cam_ctrl.cpp PlayerPtr).
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)
struct SubCharPtr {
    cSubChar* p;
};
#define pSUBS (((SubCharPtr*) &pSUB)->p)

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

// Dead flag test (cDmgInfo upper 16 bits): an inline returning 0/1 gives the `li 1; andis.; bne; li 0` chain.
static inline int em22DeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

extern "C" void _prolog()
{
    EmInitFunc = Em22Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em22Init(cEm* em)
{
    new (em) cEm22();
}

void em22DmCk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    int near;

    if (em->hp > 0 && em22DeadCk(em) == 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 4:
        case 5:
        case 7:
            EmSetDie(em);
            EmReserveDropItem(em);
            em->hp = 0;
            w->flags |= 0x400;
            EmRoutineSet(em, 2, 1, 0, 0);
            return;
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
    near = 0;
    if (em->dmPart->rad < 36000000.0f) {
        near = 1;
    }
    LifeDownSet2(em, em22SetDmVal(em), 0, 0);
    em22BloodSet(em);
    SndCall(8, 0xF, &em->pos, em->id, 0, em);
    if (em->hp <= 0) {
        EmSetDie(em);
        EmReserveDropItem(em);
        em->xFC = 2;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
        return;
    }
    if (w->flags & 8) {
        em->xFC = 2;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
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
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        if (w->flags & 0x22) {
            return;
        }
        if (Rnd() & 3) {
            EmRoutineSet(em, 2, 0, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2C:
    case 0x2D:
    default:
        EmRoutineSet(em, 2, 1, 0, 0);
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            if (Rnd() & 3) {
                EmRoutineSet(em, 2, 1, 0, 0);
            } else {
                EmRoutineSet(em, 2, 0, 0, 0);
            }
        } else {
            if (w->flags & 0x22) {
                return;
            }
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    }
}

Em22Func Em22_R0_move_tbl[5] = {
    em22_R0_Init,
    em22_R0_Move,
    em22_R0_Damage,
    em22_R0_Die,
    (Em22Func) Em_R0_Scenario,
};

// Pairs of {branch check, routine} per routine-1 state (em22_R0_Move calls both).
static Em22Func Em22_R1_move_tbl[40] = {
    em22_R1_br_dummy, em22_R1_Goto,
    em22_R1_br_dummy, em22_R1_R11B_A,
    em22_R1_br_dummy, em22_R1_R11B_B,
    em22_R1_br_dummy, em22_R1_R11B_C,
    em22_R1_br_dummy, em22_R1_InCage,
    em22_R1_br_dummy, em22_R1_JumpWait,
    em22_R1_br_dummy, em22_R1_Wait,
    em22_R1_br_dummy, em22_R1_RunAbout,
    em22_R1_br_dummy, em22_R1_Run,
    em22_R1_br_dummy, em22_R1_Turn,
    em22_R1_br_dummy, em22_R1_Escape,
    em22_R1_br_dummy, em22_R1_Threat,
    em22_R1_br_dummy, em22_R1_SideStep,
    em22_R1_br_JumpAtk, em22_R1_JumpAtk,
    em22_R1_br_dummy, em22_R1_JumpAtkHit,
    em22_R1_br_ParaAtk, em22_R1_ParaAtk,
    em22_R1_br_dummy, em22_R1_ParaAtkHit,
    em22_R1_br_dummy, em22_R1_Wakeup,
    em22_R1_br_dummy, em22_R1_Parasite,
    em22_R1_br_dummy, em22_R1_Jump,
};

static Em22Func Em22_R2_move_tbl[2] = {
    em22_R1_Dm_Small,
    em22_R1_Dm_Blow,
};

static Em22Func Em22_R3_move_tbl[1] = {
    em22_R1_Die_Lost,
};

// Parts index remap of the flipped motions (cModel::motFlip): the legs swapped.
static u16 em22_flip[50] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x08, 0x07, 0x0D, 0x0E, 0x0F, 0x10, 0x09, 0x0A, 0x0B,
    0x0C, 0x11, 0x16, 0x17, 0x18, 0x19, 0x12, 0x13, 0x14, 0x15, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31,
};
// The parasite's remap (em22SetParasite): the identity.
static u16 em22_para_flip[80] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
};
// The original link 8-aligns the end of .data (the ngcld BSS tag follows): the 4 pad bytes after the table.
asm(".section .data\n\t.balign 8\n\t.text");

void cEm22::move()
{
    Em22Work* w = EM22_WK(this);
    f32 len;
    u16 atFlags;

    em22DmCk(this);
    w->flags &= ~0x2AE;
    if (w->plDeadWait) {
        w->plDeadWait--;
    }
    if (em22DeadCk(pPL)) {
        w->plDeadWait = 30;
    }
    if (w->stuckTimer) {
        w->stuckTimer--;
    }
    if (w->escTimer) {
        w->escTimer--;
    }
    if ((s16) pG->pl_life <= 0) {
        w->escTimer = 1;
    }
    motFlags2 &= ~0x40000000;
    Em22RouteCk(this);
    Em22_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    if (hp > 0) {
        em22OpenBack(this);
    }
    em22NeckMove(this);
    partsWorldCalc();
    em22ScaleCompress(this);
    len = SQRTF((pos.x - oldPos.x) * (pos.x - oldPos.x) + (pos.z - oldPos.z) * (pos.z - oldPos.z));
    atFlags = atari.flags;
    if (w->flags & 0x80) {
        atari.flags |= 0x10;
    } else {
        atari.flags &= ~0x10;
    }
    EmAtCheck(this);
    atari.move();
    if (w->flags & 0x80) {
        SatMgr.checkAir(this, 0x180800);
    } else {
        SatMgr.check(this, 0);
    }
    atari.flags = atFlags;
    if (SQRTF((pos.x - oldPos.x) * (pos.x - oldPos.x) + (pos.z - oldPos.z) * (pos.z - oldPos.z)) < len * 0.5f) {
        w->stuckTimer = 3;
        w->stuckCnt++;
    } else {
        w->stuckCnt = 0;
    }
    em22FootSeControl(this);
    if (hp > 0 && w->pPara[0]) {
        if (w->voiceTimer) {
            w->voiceTimer--;
        } else {
            w->voiceTimer = 30;
            SndCall(8, 0x28, &pos, id, 0, this);
        }
    }
    if ((w->flags & 0x10) && hp > 0) {
        if (be_flag & 0x800) {
            EstSet((int) this, -1, 0, 0, 0x1A, 0x10, 1, 0, (u32) this, 0);
        } else {
            EstSet((int) this, -1, 0, 0, 0x1A, 0x10, 0, 0, (u32) this, 0);
        }
    }
    em22FootEff(this);
    em22AtkParaClearCk(this);
}

static void em22_R0_Init(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    void* mot;
    int zero;
    u32 i;

    if (em->modelInit(ARC(4), ARC(5)) == 0) {
        pLog->err(0, 0, "em22() ModelInit failed.");
        em->xFC = 0xFF;
        return;
    }
    zero = 0;
    mot = MOTION(em);
    EspDataLoad((u32) ARC(6), 0x1A, 0);
    em->motFlip = em22_flip;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->atari.init(3, 0x2000, 10, 0.0f, 0.0f, 0.0f, 450.0f, 400.0f, 400.0f, 500.0f);
    em22YarareInit(em);
    w->pCtrl11 = GetCtrlCtrl11();
    w->pCtrl12 = GetCtrlCtrl12();
    w->tilt = 0.0f;
    w->stuckTimer = zero;
    w->neckX = 0.0f;
    w->neckY = 0.0f;
    w->sndId[0] = zero;
    w->sndId[1] = zero;
    w->sndId[2] = zero;
    w->slaverTimer = zero;
    w->x2D0 = zero;
    w->plDist = 100000000.0f;
    w->scale = 1.0f;
    w->paraWait = (int) Rnd() % 900 + 300;
    w->voiceTimer = zero;
    for (i = 0; i < 5; i++) {
        w->pPara[i] = 0;
    }
    for (i = 0; i < 3; i++) {
        w->pParaAtk[i] = 0;
    }
    w->espKind = EspPullCoreKind();
    if (em->be_flag & 0x800) {
        EstSet((int) em, -1, 0, 0, 0x1A, 2, 1, w->espKind, (u32) em, 0);
    } else {
        EstSet((int) em, -1, 0, 0, 0x1A, 2, 0, w->espKind, (u32) em, 0);
    }
    em->setStatus(5);
    switch (em->x38D) {
    case 0:
    default:
        EmRoutineSet(em, 1, 6, 0, 0);
        break;
    case 1:
        EmRoutineSet(em, 1, 1, 0, 0);
        break;
    case 3:
        EmRoutineSet(em, 1, 3, 0, 0);
        break;
    case 2:
        EmRoutineSet(em, 1, em->x38D, 0, 0);
        break;
    case 4:
        EmRoutineSet(em, 1, em->x38D, 0, 0);
        break;
    case 5:
        EmRoutineSet(em, 1, em->x38D, 0, 0);
        break;
    }
    if (em->flags_3C8 & 0x40000000) {
        em22SetParasite(em);
    }
    MotionSetCore(em, mot, ARC(7), 0, 0, 5, 0);
    MotionMoveF(em, 0);
    em22_R0_Move(em);
}

static void em22_R0_Move(cEm22* em)
{
    Em22_R1_move_tbl[em->xFD * 2](em);
    Em22_R1_move_tbl[em->xFD * 2 + 1](em);
}

static void em22_R1_br_dummy(cEm22* em)
{
}

// The run motion of the enemy (five variants by list number) with its blend sequence.
static inline void em22SetRunMotion(cEm22* em)
{
    switch ((u8) (em->emsetNo % 5)) {
    case 0:
    default:
        MotionSetCore(em, MOTION(em), ARC(9), (int) ARC(0x21), 10, 5, 0);
        break;
    case 1:
        MotionSetCore(em, MOTION(em), ARC(9), (int) ARC(0x22), 10, 5, 0);
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(9), (int) ARC(0x23), 10, 5, 0);
        break;
    case 3:
        MotionSetCore(em, MOTION(em), ARC(9), (int) ARC(0x24), 10, 5, 0);
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(9), (int) ARC(0x25), 10, 5, 0);
        break;
    }
}

// Turn towards the route point (slower while the enemy is stuck) and face the run direction. A macro:
// as an inline taking `w` the LIMIT_ANGLE result gets copied (`fmr f0, f1`) before the store.
#define em22RunTurn(em, w)                                                                        \
    if ((w)->stuckTimer) {                                                                        \
        lim = 0.058904864f;                                                                       \
    } else {                                                                                      \
        lim = 0.078539819f;                                                                       \
    }                                                                                             \
    (em)->rot.y += Muku(&(em)->pos, &(w)->routePos, (em)->rot.y, lim);                            \
    (em)->rot.y = LIMIT_ANGLE((em)->rot.y);                                                       \
    em22DirMatrix(em, Muku(&(em)->pos, &(w)->routePos, (em)->rot.y, PI))

static void em22_R1_Goto(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    f32 lim;

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        em22SetRunMotion(em);
        em->xFE++;
    case 1:
        em22RunTurn(em, w);
        MotionMoveF(em, 0);
        if ((em->pos.x - w->gotoPos.x) * (em->pos.x - w->gotoPos.x) + (em->pos.y - w->gotoPos.y) * (em->pos.y - w->gotoPos.y) +
                (em->pos.z - w->gotoPos.z) * (em->pos.z - w->gotoPos.z) <
            1000000.0f) {
            em->xFC = 1;
            em->xFD = 6;
            em->xFE = 0;
            em->xFF = 0;
            w->gotoOn = 0;
        }
        break;
    }
    if (em22JumpCk(em) == 0) {
        em22DoorOpenCk(em);
        em22SlaverSet(em, 1);
    }
}

static void em22_R1_R11B_A(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x44), (int) ARC(0x46), 0, 5, 0);
        w->timer = 58;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x27), 10, 5, 0);
        w->timer = 252;
        em->xFE++;
    case 3:
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            EmRoutineSet(em, 1, 0xB, 0, 0);
        }
        break;
    }
    em22SlaverSet(em, 0);
}

static void em22_R1_R11B_B(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        em->atari.flags &= ~0x300;
        MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x27), 10, 5, 0);
        w->timer = 150;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xB), 0, 10, 1, 0);
        em->xFE++;
    case 3: {
        Vec v;
        f32 fl;

        em22DirMatrix(em, 0.0f);
        MotionMoveF(em, 0);
        v = em->pos;
        v.y = em->oldPos.y;
        fl = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y > fl) {
            break;
        }
        em->pos.y = fl;
        em->xFE++;
    }
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0xC), 0, 3, 5, 0);
        em->atari.flags |= 0x300;
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0xE), (int) ARC(0x2C), 3, 1, 0);
        if (em->be_flag & 0x800) {
            EstSet((int) em, -1, 0, 0, 0x1A, 6, 1, 0, (u32) em, 0);
        } else {
            EstSet((int) em, -1, 0, 0, 0x1A, 6, 0, 0, (u32) em, 0);
        }
        em->xFE++;
    case 7:
        em22DirMatrix(em, 0.0f);
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        } else if (em->motEvent & 1) {
            em22SetParasite(em);
        }
        break;
    case 8:
        MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x27), 10, 5, 0);
        w->timer = 28;
        em->xFE++;
    case 9:
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            EmRoutineSet(em, 1, 0xB, 0, 0);
        }
        break;
    }
    em22SlaverSet(em, 0);
}

static void em22_R1_R11B_C(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        em->atari.flags &= ~0x300;
        MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x27), 10, 0x45, 0xF);
        w->timer = 310;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->timer == 0) {
            em->hp = 0;
            em->clearStatus(5);
            em->alpha = 0.0f;
            em->be_flag &= ~2;
            em->be_flag |= 0x4000;
            em->xFE++;
        } else {
            w->timer--;
            em22SlaverSet(em, 0);
        }
        break;
    }
}

static void em22_R1_InCage(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x27), 0, 4, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            if (em22GotoCk(em) == 0) {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
    em22SlaverSet(em, 0);
}

static void em22_R1_JumpWait(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(7), 0, 30, 5, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em22GotoCk(em)) {
            return;
        }
        if ((w->flags & 1) && em->plDist2 < em->x3CC * em->x3CC) {
            EmRoutineSet(em, 1, 7, 0, 0);
            return;
        }
        if (em->flags_3C8 & 1) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x59), (int) ARC(0x5A), 3, 1, 0);
        SndCall(8, 8, &em->pos, em->id, 0, em);
        w->timer = 20;
        em->xFE++;
    case 3:
        w->flags |= 8;
        if (w->timer) {
            w->timer--;
            em->dmType = 2;
            em->atari.throughOn();
        } else {
            em->atari.throughOff();
        }
        em22DirMatrix(em, 0.0f);
        if (MotionMoveF(em, 0)) {
            if (em22GotoCk(em)) {
                return;
            }
            EmRoutineSet(em, 1, 7, 0, 0);
        } else {
            em22DoorOpenCk(em);
        }
        break;
    }
    em22SlaverSet(em, 0);
}

static void em22_R1_Wait(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(7), 0, 30, 5, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->routeAngAbs > 0.7853982f) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x44), (int) ARC(0x46), 10, 5, 0);
        em->xFE++;
    case 3:
        em->rot.y += Muku(&em->pos, &w->routePos, em->rot.y, 0.049087387f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em22DirMatrix(em, 0.0f);
        MotionMoveF(em, 0);
        if (w->routeAngAbs < 0.2617994f) {
            em->xFE = 0;
        }
        break;
    }
    if (em22GotoCk(em)) {
        return;
    }
    if (em->flags_3C8 & 1) {
        em->flags_3C8 &= ~1;
        EmRoutineSet(em, 1, 7, 0, 0);
        return;
    }
    if ((w->flags & 1) && em->plDist2 < em->x3CC * em->x3CC) {
        EmRoutineSet(em, 1, 7, 0, 0);
        return;
    }
    {
        // the zero of the last routine set is a block-local pseudo set before the test (its
        // `li` lands at the top of the test block, above the pG load)
        int zero = 0;
        if ((pG->flags_500C & 0x00800000) && em->plDist2 < 625000000.0f) {
            EmRoutineSet(em, 1, 7, zero, zero);
            return;
        }
    }
    em22SlaverSet(em, 0);
}

static void em22_R1_RunAbout(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    f32 lim;

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        em->xFF = 1;
        em22SetRunMotion(em);
        w->timer = (u8) (Rnd() % 3) + 3;
        em->xFE++;
    case 1:
        em22RunTurn(em, w);
        MotionMoveF(em, 0);
        if ((w->flags & 1) && em->plDist2 < 49000000.0f && w->routeAngAbs < 0.5235988f) {
            EmRoutineSet(em, 1, 0xB, 0, 0);
        } else if (w->stuckCnt > 5) {
            EmRoutineSet(em, 1, 9, 0, 0);
        } else if (em22PlRunCk(em) && w->plDist > 5000.0f) {
            EmRoutineSet(em, 1, 8, 0, 0);
        }
        break;
    }
    if (em22GotoCk(em)) {
        return;
    }
    if (em->plDist2 < 6250000.0f) {
        EmRoutineSet(em, 1, 0xA, 0, 0);
    } else if (em22SetParasiteCk(em)) {
        EmRoutineSet(em, 1, 0x12, 0, 0);
    } else if (em22JumpCk(em) == 0) {
        if (w->targetAngAbs > 2.0943952f) {
            EmRoutineSet(em, 1, 9, 0, 0);
        } else {
            em22DoorOpenCk(em);
            em22SlaverSet(em, 1);
        }
    }
}

static void em22_R1_Run(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    f32 lim;
    Vec a;
    Vec b;
    int hit;

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        em22SetRunMotion(em);
        em->xFE++;
    case 1:
        em22RunTurn(em, w);
        MotionMoveF(em, 0);
        if (em->plDist2 < 20250000.0f && w->routeAngAbs < 0.5235988f && em22PlRunCk(em) == 0) {
            if (w->plDeadWait == 0) {
                a = em->pos;
                b = pPLS->pos;
                a.y += 500.0f;
                b.y += 500.0f;
                if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0 && em22ScreenInCk(em) && (u8) (Rnd() % 10) > 4 && (w->flags & 1)) {
                    EmRoutineSet(em, 1, 0xD, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 7, 0, 0);
                }
            } else {
                EmRoutineSet(em, 1, 7, 0, 0);
            }
        } else if (em->plDist2 < 4000000.0f && w->routeAngAbs < 0.5235988f) {
            if (w->plDeadWait == 0) {
                a = em->pos;
                b = pPLS->pos;
                a.y += 500.0f;
                b.y += 500.0f;
                hit = SatMgr.hitCheck(&a, &b, 0, 0, 0, 0);
                if (hit == 0 && em22ScreenInCk(em) && (u8) (Rnd() % 10) > 4 && (w->flags & 1)) {
                    EmRoutineSet(em, 1, 0xD, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 7, 0, 0);
                }
            } else {
                EmRoutineSet(em, 1, 7, 0, 0);
            }
        }
        break;
    }
    if (em22GotoCk(em)) {
        return;
    }
    if (em22SetParasiteCk(em)) {
        em22SetParasite(em);
    }
    if (em22JumpCk(em)) {
        return;
    }
    if (w->targetAngAbs > 2.0943952f) {
        EmRoutineSet(em, 1, 9, 0, 0);
    } else {
        em22DoorOpenCk(em);
        em22SlaverSet(em, 1);
    }
}

static void em22_R1_Turn(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    f32 ang;
    f32 d;

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        ang = LIMIT_ANGLE(GetXZAngle(&em->pos, &w->routePos) - em->rot.y);
        d = fabsf(ang);
        if (d < 0.7853982f) {
            MotionSetCore(em, MOTION(em), ARC(0x3E), (int) ARC(0x41), 3, 1, 0);
            w->delta = 0.0f;
        } else if (ang < 0.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x3F), (int) ARC(0x42), 3, 0x41, 0);
            w->delta = -1.5707964f;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x3F), (int) ARC(0x42), 3, 1, 0);
            w->delta = 1.5707964f;
        }
        w->delta = ang - w->delta;
        em->xFE++;
    case 1:
        d = w->delta * 0.1f;
        em->rot.y += d;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        w->delta -= d;
        em22DirMatrix(em, 0.0f);
        if (MotionMoveF(em, 0)) {
            if (em22GotoCk(em) == 0) {
                if (em->plDist2 < 6250000.0f) {
                    EmRoutineSet(em, 1, 0xA, 0, 0);
                    return;
                }
                if (em22PlRunCk(em)) {
                    if (w->plDist > 5000.0f) {
                        EmRoutineSet(em, 1, 8, 0, 0);
                    }
                } else {
                    if (w->plDist > 7000.0f) {
                        EmRoutineSet(em, 1, 7, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 0xB, 0, 0);
                    }
                }
            }
        }
        break;
    }
    em22SlaverSet(em, 0);
}

// The bell / rung point (emwep.cpp): the byte-pointer copy keeps the pG reload before the next store.
#define SET_BELL_POS(pos) memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), pos, sizeof(Vec))

static void em22_R1_Escape(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    f32 lim;

    if (em->plDist2 < 25000000.0f) {
        w->flags |= 4;
    }
    if (em->xFE == 0) {
        RouteCkEscEm(em, pPL, &w->routePos);
        lim = fabsf(Muku(&em->pos, &w->routePos, em->rot.y, PI));
        if (lim > 2.3561945f) {
            em->xFE = 2;
        }
        if (!(pG->flags_5010 & 0x20000000)) {
            BitOn(pG->flags_5010, 0x20000000);
            SET_BELL_POS(&em->pos);
            pG->bell_stat = 0;
        }
    }
    w->escTimer = 2;
    switch (em->xFE) {
    case 0:
        em22SetRunMotion(em);
        w->timer = (u8) (Rnd() % 30) + 30;
        em->xFE++;
    case 1:
        em22RunTurn(em, w);
        MotionMoveF(em, 0);
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x40), (int) ARC(0x43), 3, 1, 0);
        em->xFE++;
    case 3:
        w->timer = (u8) (Rnd() % 30) + 30;
        em22DirMatrix(em, 0.0f);
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    }
    if (em22GotoCk(em)) {
        return;
    }
    if (w->timer) {
        w->timer--;
    }
    if (em->plDist2 > 49000000.0f && w->timer == 0) {
        EmRoutineSet(em, 1, 7, 0, 1);
        return;
    }
    if (em22JumpCk(em)) {
        return;
    }
    if (w->targetAngAbs > 2.0943952f) {
        EmRoutineSet(em, 1, 9, 0, 0);
    } else {
        em22DoorOpenCk(em);
        em22SlaverSet(em, 1);
    }
}

static void em22_R1_Threat(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    Vec a;
    Vec b;
    Vec c;

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x27), 10, 5, 0);
        w->timer = (u8) (Rnd() % 15) + 30;
        w->lockCnt = 0;
        em->xFE++;
    case 1:
        em22DirMatrix(em, 0.0f);
        MotionMoveF(em, 0);
        if (fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, PI)) < 1.0471976f) {
            if (w->timer) {
                w->timer--;
            } else {
                if (w->plDeadWait == 0) {
                    EmRoutineSet(em, 1, 8, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 7, 0, 0);
                }
                break;
            }
        }
        if (w->routeAngAbs > 1.5707964f) {
            EmRoutineSet(em, 1, 9, 0, 0);
        } else if (em22PlRunCk(em)) {
            if (w->plDist > 5000.0f) {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        } else if (em->plDist2 < 16000000.0f && w->routeAngAbs < 0.5235988f && w->plDeadWait == 0) {
            a = em->pos;
            b = pPLS->pos;
            a.y += 500.0f;
            b.y += 500.0f;
            if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
                EmRoutineSet(em, 1, 0xF, 0, 0);
            }
        } else if (em22PlRunCk2(em) && em->plDist2 < 49000000.0f && w->plDeadWait == 0) {
            a = em->pos;
            c = pPLS->pos;
            a.y += 500.0f;
            c.y += 500.0f;
            if (SatMgr.hitCheck(&a, &c, 0, 0, 0, 0) == 0) {
                EmRoutineSet(em, 1, 0xF, 0, 0);
            }
        }
        break;
    }
    if (em22GotoCk(em)) {
        return;
    }
    if (em22SetParasiteCk(em)) {
        EmRoutineSet(em, 1, 0x12, 0, 0);
        return;
    }
    if (em22LockCk(em)) {
        w->lockCnt++;
        if (w->lockCnt > 15) {
            EmRoutineSet(em, 1, 0xC, 0, 0);
            return;
        }
    } else {
        w->lockCnt = 0;
    }
    em22SlaverSet(em, 0);
}

static void em22_R1_SideStep(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    em->dmType = 2;
    w->flags |= 4;
    switch (em->xFE) {
    case 0: {
        Vec a;
        Vec b;
        int side = Rnd() & 1;

        if (side) {
            a.x = 0.0f;
            a.y = 500.0f;
            a.z = 0.0f;
            b.x = 1000.0f;
            b.y = 500.0f;
            b.z = 0.0f;
            PSMTXMultVec(em->mat, &a, &a);
            PSMTXMultVec(em->mat, &b, &b);
            if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
                side = 0;
            }
        } else {
            a.x = 0.0f;
            a.y = 500.0f;
            a.z = 0.0f;
            b.x = -1000.0f;
            b.y = 500.0f;
            b.z = 0.0f;
            PSMTXMultVec(em->mat, &a, &a);
            PSMTXMultVec(em->mat, &b, &b);
            if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
                side = 1;
            }
        }
        if (side) {
            MotionSetCore(em, MOTION(em), ARC(0xF), (int) ARC(0x2D), 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xF), (int) ARC(0x2D), 3, 1, 0);
        }
        em->xFE++;
    }
    case 1:
        em22DirMatrix(em, 0.0f);
        if (MotionMoveF(em, 0)) {
            if (em22GotoCk(em)) {
                return;
            }
            if (fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, PI)) < 1.0471976f && w->plDeadWait == 0) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 7, 0, 1);
            }
        }
        break;
    }
    em22SlaverSet(em, 1);
}

#define VIB_TBL ((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc))


static void em22_R1_br_JumpAtk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    Mtx inv;
    Vec v;
    Vec a;
    Vec b;
    Mtx m;

    if (!(em->motEvent & 1)) {
        return;
    }
    if (em22DeadCk(pPL)) {
        return;
    }
    if (!(w->flags & 1)) {
        return;
    }
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &v);
    if (v.x > 500.0f || v.x < -500.0f || v.z > 1200.0f || v.z < 0.0f || v.y > 500.0f || v.y < -500.0f) {
        return;
    }
    a = em->pos;
    b = pPL->pos;
    a.y += 1500.0f;
    b.y += 1500.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return;
    }
    a = em->pos;
    b = pPL->pos;
    a.y += 500.0f;
    b.y += 500.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return;
    }
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return;
    }
    PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &pPL->pos));
    TransMatrix(m, &em->pos);
    a.x = 300.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 300.0f;
    b.y = 500.0f;
    b.z = 500.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return;
    }
    a.x = 300.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 300.0f;
    b.y = 500.0f;
    b.z = 500.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return;
    }
    EmRoutineSet(em, 1, 0xE, 0, 0);
    pPL->dmType = 30;
    em->dmType = 30;
    VibSetData(VIB_TBL, 7, 1);
}

static void em22_R1_JumpAtk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x16), (int) ARC(0x33), 3, 1, 0);
        SndCall(8, 8, &em->pos, em->id, 0, em);
        w->timer = 10;
        em->xFE++;
    case 1:
        w->flags |= 8;
        if (w->timer) {
            w->timer--;
            em->rot.y += Muku(&em->pos, &w->routePos, em->rot.y, 0.049087387f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        em22DirMatrix(em, 0.0f);
        if (MotionMoveF(em, 0)) {
            GameAddPoint(0xB);
            if (em22GotoCk(em)) {
                return;
            }
            EmRoutineSet(em, 1, 0xA, 0, 0);
        }
        break;
    }
    em22DoorOpenCk(em);
    em22SlaverSet(em, 1);
}

static void em22_R1_JumpAtkHit(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    em->dmType = 2;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x17), 0, 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem22_JumpAtkHit, -41.59f, 0.0f, 638.16f);
        PlSetDamageSe(0);
        w->timer = 10;
        w->tilt = 0.0f;
        SndCall(8, 0x15, &em->pos, em->id, 0, em);
        PlGachaInit();
        w->camType = em22GetCamType(em);
        em->xFE++;
    case 1:
        em22CamMove(em, w->camType);
        if (EmCatchMotionMove(em, 1.0f, 1.0f)) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x18), 0, 3, 1, 0);
        w->sndId[0] = SndCall(8, 0x12, &em->pos, em->id, 0, em);
        w->sndId[2] = SndCall(8, 0x21, &pPLS->pos, em->id, 0, em);
        EstSet((int) em, -1, 0, 0, 0x1A, 0xD, 0, 0, (u32) em, 0);
        w->timer = 0;
        em->xFE++;
    case 3:
        em22CamMove(em, w->camType);
        PlGachaMove();
        LifeDownSet2(pPL, 10, 0, 1);
        if (MotionMoveF(em, 0)) {
            if ((u32) PlGachaGet() < 15) {
                LifeDownSet2(pPL, 200, 0, 0);
            }
            if ((s16) pG->pl_life <= 1) {
                pG->pl_life = 0;
                em->xFE = 6;
            } else {
                em->xFE = 4;
            }
        } else if (w->timer) {
            w->timer--;
        } else {
            w->timer = 5;
            EstSet((int) em, -1, 0, 0, 0x1A, 7, 0, 0, (u32) em, 0);
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x19), (int) ARC(0x34), 3, 1, 0);
        w->timer = 45;
        em->xFE++;
    case 5:
        if (w->timer) {
            w->timer--;
            em22CamMove(em, w->camType);
        }
        if (em->motEvent & 1) {
            SndStop(w->sndId[0], 0);
            SndStop(w->sndId[1], 0);
            SndCall(8, 0x11, &em->pos, em->id, 0, em);
            SndStop(w->sndId[2], 0);
            SndCall(8, 0x22, &pPL->pos, em->id, 0, em);
        }
        if ((em->motEvent & 2) && ChkWaterEffectEnable(&em->pos)) {
            EstSet(0, -1, &em->pos, 0, 0x1A, 4, 0, 0, 0, 0);
        }
        if (MotionMoveF(em, 0)) {
            em->atari.setPriority(0);
            EmRoutineSet(em, 1, 0x11, 0, 0);
        } else if (w->timer) {
            w->timer--;
        } else {
            w->timer = 5;
            EstSet((int) em, -1, 0, 0, 0x1A, 0xA, 0, 0, (u32) em, 0);
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0x1A), 0, 3, 1, 0);
        SndStop(w->sndId[2], 0);
        SndCall(1, 0xD, &pPL->pos, 0, 0, pPL);
        EstSet((int) em, -1, 0, 0, 0x1A, 0xB, 0, 0, (u32) em, 0);
        DiedemoExec(30, 0);
        em->xFE++;
    case 7:
        em22CamMove(em, w->camType);
        MotionMoveF(em, 0);
        break;
    }
    em->x3A8 = em->pos;
}

static void plem22_JumpAtkHit(cPlayer* pl)
{
    f32 y;

    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 10);
    pl->subArc = PL_EM_G->subArc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x35), 0, 0, 1, 0);
        PlSetFace(1);
        pl->atari.set(10, 480.00003f, 400.0f);
        pl->pWep->setTrans(0, 0);
        VibSetData(VIB_TBL, 0xF, 1);
        pl->xFE++;
    case 1:
        if (pl->frame > 6.7f && pl->frame < 7.3f) {
            SndCall(5, 0xC, &pl->pos, 0, 0, pl);
        }
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        pl->xFE = PL_EM_G->xFE;
        break;
    case 2: {
        Vec v;

        v.x = -35.86f;
        v.y = 0.0f;
        v.z = 235.52f;
        pl->rot.x = 0.0f;
        y = PL_EM(pl)->rot.y;
        pl->rot.z = 0.0f;
        pl->rot.y = y + PI;
        LIMIT_ANGLE(pl->rot.y);
        PSMTXMultVec(PL_EM(pl)->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x36), 0, 3, 1, 0);
        pl->xFE++;
    }
    case 3:
        MotionMoveF(pl, 0);
        pl->xFE = PL_EM_G->xFE;
        break;
    case 4:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x37), 0, 3, 1, 0);
        pl->x3E0 = 45;
        VibSetClearType(1);
        pl->xFE++;
    case 5:
        if (pl->x3E0) {
            pl->x3E0--;
            if (pl->x3E0 == 0) {
                pl->pWep->setTrans(1, 0);
            }
        }
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    case 6:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x38), 0, 3, 1, 0);
        pl->xFE++;
    case 7:
        MotionMoveF(pl, 0);
        break;
    }
    pl->x3A8 = pl->pos;
    pl->x378 = pl->x37C;
}

static void em22_R1_br_ParaAtk(cEm22* em)
{
    Mtx inv;
    Vec v;

    if (!(em->motEvent & 1)) {
        return;
    }
    if (em22DeadCk(pPL)) {
        return;
    }
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &v);
    if (v.x > 500.0f || v.x < -500.0f || v.z > 4000.0f || v.z < 0.0f || v.y > 500.0f || v.y < -500.0f) {
        return;
    }
    EmRoutineSet(em, 1, 0x10, 0, 0);
    pPL->dmType = 30;
    em->dmType = 30;
    VibSetData(VIB_TBL, 7, 1);
}

static void em22_R1_ParaAtk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 0x204;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x50), (int) ARC(0x52), 3, 1, 0);
        SndCall(8, 0x2E, &em->pos, em->id, 0, em);
        SndCall(8, 0x2C, &em->pos, em->id, 0, em);
        w->timer2 = 30;
        w->timer = 15;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.19634955f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (w->timer2) {
            w->timer2--;
            if (w->timer2 == 0) {
                SndCall(8, 0x2D, &em->pos, em->id, 0, em);
            }
        }
        em22DirMatrix(em, 0.0f);
        if (MotionMoveF(em, 0)) {
            GameAddPoint(0xB);
            EmRoutineSet(em, 1, 0xA, 0, 0);
        } else {
            if (em->motEvent & 0x80) {
                em22SetParasiteAtk(em);
                em22SetParasite(em);
            }
            if (em->motEvent & 0x40) {
                em22ParaSetMotAtk(em);
            }
        }
        break;
    }
}

static void em22_R1_ParaAtkHit(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    em->dmType = 2;
    w->flags |= 0x204;
    switch (em->xFE) {
    case 0:
        em22ParaAtkHitPosSet(em);
        MotionSetCore(em, MOTION(em), ARC(0x4F), 0, 3, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem22_JumpAtkHit, -41.59f, 0.0f, 638.16f);
        SetPlDamage((int) em, plem22_ParaAtkHit);
        em22ParaSetMotAtkHit(em);
        SndCall(8, 0x2F, &em->pos, em->id, 0, em);
        PlSetDamageSe(0);
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.098174773f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            em->atari.setPriority(0);
            w->plDeadWait = 30;
            em22ParaSetMotWait(em);
            EmRoutineSet(em, 1, 7, 0, 0);
        }
        break;
    }
}

static void plem22_ParaAtkHit(cPlayer* pl)
{
    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 10);
    pl->subArc = PL_EM_G->subArc;
    switch (pl->xFE) {
    case 0:
        pl->rot.y += Muku(&pl->pos, &PL_EM(pl)->pos, pl->rot.y, PI);
        pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x4D), 0, 3, 1, 0);
        PlSetFace(1);
        EstSet((int) pl, -1, 0, 0, 0x1A, 0xC, 0, 0, (u32) pl, 0);
        VibSetData(VIB_TBL, 0xE, 1);
        pl->x3E0 = 45;
        pl->xFE++;
    case 1:
        if (pl->x3E0) {
            pl->x3E0--;
            LifeDownSet2(pPLS, 10, 0, 0);
            if (pl->x3E0 == 0 && (s16) pG->pl_life <= 0) {
                PlSetDamageSe(0xD);
                PlSetDamage(6, 0, 0);
                break;
            }
        }
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    }
    pl->x378 = pl->x37C;
}

static void em22_R1_Wakeup(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    int flip;

    switch (em->xFE) {
    case 0:
        flip = 1;
        if (em->motFlags & 0x40) {
            flip = 0x41;
        }
        if (w->targetAngAbs < 1.5707964f) {
            MotionSetCore(em, MOTION(em), ARC(0x14), (int) ARC(0x31), 3, flip, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x15), (int) ARC(0x32), 3, flip, 0);
        }
        w->timer = 15;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
            w->flags |= 2;
        }
        em22DirMatrix(em, 0.0f);
        if (MotionMoveF(em, 0)) {
            if (em22GotoCk(em) == 0) {
                EmRoutineSet(em, 1, 7, 0, 0);
            }
        }
        break;
    }
}

static void em22_R1_Parasite(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    w->flags |= 0x20;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE), (int) ARC(0x2C), 3, 1, 0);
        if (em->be_flag & 0x800) {
            EstSet((int) em, -1, 0, 0, 0x1A, 6, 1, 0, (u32) em, 0);
        } else {
            EstSet((int) em, -1, 0, 0, 0x1A, 6, 0, 0, (u32) em, 0);
        }
        em->xFE++;
    case 1:
        em22DirMatrix(em, 0.0f);
        if (MotionMoveF(em, 0)) {
            if (em22GotoCk(em) == 0) {
                EmRoutineSet(em, 1, 7, 0, 0);
            }
        } else if (em->motEvent & 1) {
            em22SetParasite(em);
        }
        break;
    }
}

static void em22_R1_Jump(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    Mtx m;
    Vec v;
    f32 fl;

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x3C), (int) ARC(0x3D), 3, 1, 0);
        PSMTXRotRad(m, 'y', em->rot.y);
        TransMatrix(m, &em->pos);
        v.x = 0.0f;
        v.y = 500.0f;
        v.z = 4356.0f;
        PSMTXMultVec(m, &v, &v);
        fl = SatMgr.getFloor(&v, 1000.0f, 100000.0f, 0, 0);
        if (fl == -100000.0f) {
            fl = em->pos.y;
        }
        w->delta = fl - em->pos.y;
        em->xFE++;
    case 1:
        fl = w->delta * 0.1f;
        em->pos.y += fl;
        w->delta -= fl;
        w->flags |= 0x88;
        em22DirMatrix(em, 0.0f);
        if (MotionMoveF(em, 0)) {
            if (em22GotoCk(em) == 0) {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
    em22SlaverSet(em, 1);
}

static void em22_R0_Damage(cEm22* em)
{
    Em22_R2_move_tbl[em->xFD](em);
}

static void em22_R1_Dm_Small(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    void* mot;
    int flip;

    switch (em->xFE) {
    case 0:
        switch (Rnd() & 3) {
        case 0:
        default:
            mot = ARC(0x10);
            flip = 1;
            break;
        case 1:
            mot = ARC(0x10);
            flip = 0x41;
            break;
        case 2:
            mot = ARC(0x45);
            flip = 1;
            break;
        case 3:
            mot = ARC(0x45);
            flip = 0x41;
            break;
        }
        MotionSetCore(em, MOTION(em), mot, 0, 3, flip, 0);
        SndCall(8, 0xC, &em->pos, em->id, 0, em);
        w->lockCnt = 0;
        w->timer = 8;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em22LockCk(em)) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else if (Rnd() & 1) {
                EmRoutineSet(em, 1, 0xA, 0, 0);
            } else if (w->plDeadWait == 0) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                EmRoutineSet(em, 1, 7, 0, 0);
            }
        } else if (w->timer) {
            w->timer--;
            if (w->timer == 0 && (Rnd() & 3) == 0) {
                em->dmType = 5;
                EmRoutineSet(em, 1, 0xC, 0, 0);
            }
        }
        break;
    }
}

static void em22_R1_Dm_Blow(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    f32 ang;
    f32 angAbs;
    f32 fl;
    void* mot;
    void* blend;
    int flip;
    cModelInfo* info;

    w->flags |= 2;
    switch (em->xFE) {
    case 0:
        ang = Muku(&em->pos, &em->x328, em->rot.y, PI);
        angAbs = fabsf(ang);
        em->xFF = ang < 0.0f ? 3 : 2;
        if (angAbs < 0.7853982f) {
            em->xFF = 0;
        }
        if (angAbs > 2.3561945f) {
            em->xFF = 1;
        }
        switch (em->xFF) {
        case 0:
        default:
            mot = ARC(0x12);
            blend = ARC(0x2F);
            flip = 1;
            break;
        case 1:
            mot = ARC(0x11);
            blend = ARC(0x2E);
            flip = 1;
            break;
        case 2:
            mot = ARC(0x13);
            blend = ARC(0x30);
            flip = 1;
            break;
        case 3:
            mot = ARC(0x13);
            blend = ARC(0x30);
            flip = 0x41;
            break;
        }
        MotionSetCore(em, MOTION(em), mot, (int) blend, 3, flip, 0);
        switch (em->xFF) {
        case 0:
        default:
            w->blowSpd.x = 0.0f;
            w->blowSpd.y = -100.0f;
            w->blowSpd.z = -200.0f;
            break;
        case 1:
            w->blowSpd.x = 0.0f;
            w->blowSpd.y = -100.0f;
            w->blowSpd.z = 200.0f;
            break;
        case 2:
            w->blowSpd.x = -200.0f;
            w->blowSpd.y = -100.0f;
            w->blowSpd.z = 0.0f;
            break;
        case 3:
            w->blowSpd.x = 200.0f;
            w->blowSpd.y = -100.0f;
            w->blowSpd.z = 0.0f;
            break;
        }
        PSMTXMultVecSR(em->mat, &w->blowSpd, &w->blowSpd);
        if (em->hp <= 0) {
            SndStop(w->x2C8, 0);
            SndCall(8, 0x1E, &em->pos, em->id, 0, em);
        } else {
            SndCall(8, 0xC, &em->pos, em->id, 0, em);
        }
        w->timer = 1;
        w->camType = 0;
        w->timer2 = 0;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                em->atari.flags &= ~0x300;
                EmRoutineSet(em, 3, 0, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x11, 0, 0);
            }
        } else {
            if (em->motEvent & 2) {
                if (ChkWaterEffectEnable(&em->pos)) {
                    EstSet(0, -1, &em->pos, 0, 0x1A, 4, 0, 0, 0, 0);
                }
                fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
                if (em->pos.y < fl) {
                    em->pos.y = fl;
                    w->timer = 0;
                } else {
                    em->xFE = 2;
                }
            }
            if (w->timer) {
                w->flags |= 0x88;
            } else {
                w->flags &= ~0x88;
            }
        }
        break;
    case 2:
        switch (em->xFF) {
        case 0:
        default:
            mot = ARC(0x53);
            flip = 1;
            break;
        case 1:
            mot = ARC(0x55);
            flip = 1;
            break;
        case 2:
            mot = ARC(0x57);
            flip = 1;
            break;
        case 3:
            mot = ARC(0x57);
            flip = 0x41;
            break;
        }
        MotionSetCore(em, MOTION(em), mot, 0, 3, flip, 0);
        em->xFE++;
    case 3:
        w->flags |= 0x88;
        w->timer2++;
        PSVECAdd(&em->pos, &w->blowSpd, &em->pos);
        w->blowSpd.y -= 20.0f;
        MotionMoveF(em, 0);
        fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < fl) {
            em->pos.y = fl;
            w->blowSpd.x = 0.0f;
            w->blowSpd.y = 0.0f;
            w->blowSpd.z = 0.0f;
            w->timer = 0;
            LifeDownSet(em, w->timer2 * 50, 0);
            em->xFE++;
        }
        break;
    case 4:
        switch (em->xFF) {
        case 0:
        default:
            mot = ARC(0x54);
            flip = 1;
            break;
        case 1:
            mot = ARC(0x56);
            flip = 1;
            break;
        case 2:
            mot = ARC(0x58);
            flip = 1;
            break;
        case 3:
            mot = ARC(0x58);
            flip = 0x41;
            break;
        }
        MotionSetCore(em, MOTION(em), mot, 0, 3, flip, 0);
        SndCall(8, 0x10, &em->pos, em->id, 0, em);
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                em->atari.flags &= ~0x300;
                EmRoutineSet(em, 3, 0, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x11, 0, 0);
            }
        }
        break;
    }
    if ((w->flags & 0x400) && em->pInfo) {
        for (info = em->pInfo; info; info = info->pNext) {
            if (info->color[0] > 0x20) {
                info->color[0] -= 0x20;
            }
            info->color[1] = info->color[0];
            info->color[2] = info->color[0];
        }
    }
}

static void em22_R0_Die(cEm22* em)
{
    Em22_R3_move_tbl[em->xFD](em);
}

static void em22_R1_Die_Lost(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    u32 i;

    switch (em->xFE) {
    case 0:
        em->atari.flags &= ~0x300;
        em->atari.flags |= 0x10;
        EmSetDie(em);
        EmReserveDropItem(em);
        EmSetDieCntE(em);
        em->clearStatus(5);
        em->setStatus(8);
        EmSetDropItem(em);
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        EstSet((int) em, -1, 0, 0, 0x1A, 5, 0, 0, (u32) em, 0);
        for (i = 0; i < 5; i++) {
            if (w->pPara[i]) {
                w->pPara[i]->clearLostWait();
                w->pPara[i] = 0;
            }
        }
        w->timer = 30;
        w->timer2 = 150;
        w->scale = 1.0f;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
            if (w->timer == 0) {
                SndCall(8, 0x26, &em->pos, em->id, 0, em);
            }
        } else {
            w->scale -= 0.01f;
            if (w->scale < 0.1f) {
                w->scale = 0.1f;
            }
            em->pos.y -= 3.0f;
        }
        TransMatrix(em->mat, &em->pos);
        if (w->timer2) {
            w->timer2--;
        } else {
            em->alpha -= 0.1f;
            if (em->alpha <= 0.0f) {
                em->alpha = 0.0f;
                em->be_flag &= ~2;
                em->be_flag |= 0x4000;
                em->xFE++;
            }
        }
        break;
    }
}

void Em22RouteCk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    Vec a;
    Vec b;
    Vec dbg;
    f32 dx;
    f32 dz;

    if (em->hp <= 0) {
        return;
    }
    if ((pG->flags_51E4 & 3) != (em->emsetNo & 3)) {
        return;
    }
    w->flags &= ~1;
    RouteCkToPos(em, &pPLS->pos, &w->plRoutePos, 0, 0);
    w->routeAng = Muku(&em->pos, &w->plRoutePos, em->rot.y, PI);
    w->routeAngAbs = fabsf(w->routeAng);
    a.x = em->pos.x;
    a.y = em->pos.y + 500.0f;
    a.z = em->pos.z;
    b.x = pPL->pos.x;
    b.y = pPL->pos.y + 500.0f;
    b.z = pPL->pos.z;
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
        w->flags |= 1;
    }
    w->routePos = w->plRoutePos;
    w->targetAng = w->routeAng;
    w->targetAngAbs = w->routeAngAbs;
    w->targetDist2 = em->plDist2;
    w->pTarget = pPLS;
    w->plDist = RouteCkPosToPosDis(&em->pos, &pPLS->pos);
    if (w->gotoOn) {
        RouteCkToPos(em, &w->gotoPos, &w->routePos, 0, 0);
        w->targetAng = Muku(&em->pos, &w->routePos, em->rot.y, PI);
        w->targetAngAbs = fabsf(w->targetAng);
        dz = em->pos.z - w->gotoPos.z;
        dx = em->pos.x - w->gotoPos.x;
        w->targetDist2 = dx * dx + dz * dz;
        if (pGS->flags_60 & 0x4000) {
            dbg = em->pos;
            dbg.y += 250.0f;
            Draw_line3d(&dbg, &w->routePos, 0xFFFFFF40, 0);
            Draw_line3d(&dbg, &w->gotoPos, 0xFF0000FF, 0);
        }
    } else if (w->escTimer) {
        RouteCkEscEm(em, pPL, &w->routePos);
        w->targetAng = Muku(&em->pos, &w->routePos, em->rot.y, PI);
        w->targetAngAbs = fabsf(w->targetAng);
        if (pGS->flags_60 & 0x4000) {
            dbg = em->pos;
            dbg.y += 250.0f;
            Draw_line3d(&dbg, &w->routePos, 0xFFFFFF40, 0);
        }
    } else {
        if (pGS->flags_60 & 0x4000) {
            dbg = em->pos;
            dbg.y += 250.0f;
            Draw_line3d(&dbg, &w->routePos, 0xFFFFFF40, 0);
        }
    }
}

void em22DirMatrix(cEm22* em, f32 dir)
{
    Em22Work* w = EM22_WK(em);
    f32 t;

    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->motFlags2 |= 0x40000000;
    t = 0.0f;
    if (dir > 0.0f) {
        t = -0.5235988f;
    }
    if (dir < 0.0f) {
        t = 0.5235988f;
    }
    dir = fabsf(dir);
    if (dir < 0.049087387f) {
        t = 0.0f;
    }
    w->tilt = w->tilt * 0.8f + t * 0.2f;
    // The magnitude reuses `t` (a multi-set global pseudo): the fabs result is not tied to the
    // dying blend result and lands in t's f11.
    t = fabsf(w->tilt);
    if (t > 0.001f) {
        Mtx m;
        Vec ax;

        ax.x = 0.0f;
        ax.y = 0.0f;
        ax.z = 1.0f;
        PSMTXMultVecSR(em->mat, &ax, &ax);
        PSMTXRotAxisRad(m, &ax, w->tilt);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
}

void em22NeckMove(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    Mtx m;
    Vec d;
    cModel* plP;
    cParts* p;
    f32 f;
    f32 len;

    plP = pPL->getPartsPtr(4);
    p = (cParts*) em->getPartsPtr(4);
    if (w->flags & 4) {
        f = Muku(&em->pos, &pPL->pos, em->rot.y, 1.0471976f);
        w->neckY += Muku2(w->neckY, f, 0.098174773f);
        PSVECSubtract(&plP->worldPos, &p->worldPos, &d);
        len = SQRTF(d.x * d.x + d.z * d.z);
        f = -atan2f(d.y, len);
        if (f > 1.2217305f) {
            f = 1.2217305f;
        }
        if (f < -1.2217305f) {
            f = -1.2217305f;
        }
        w->neckX += Muku2(w->neckX, f, 0.098174773f);
    } else {
        w->neckX *= 0.9f;
        w->neckY *= 0.9f;
    }
    f = w->neckY * 0.33333334f;
    p = (cParts*) em->getPartsPtr(3);
    p->addRot.z = 0.0f;
    p->addRot.x = 0.0f;
    p->addRot.y = f;
    p->motParts.flags |= 0x40000000;
    p = (cParts*) em->getPartsPtr(4);
    p->addRot.z = 0.0f;
    p->addRot.x = 0.0f;
    p->addRot.y = f;
    p->motParts.flags |= 0x40000000;
    p = (cParts*) em->getPartsPtr(5);
    p->addRot.x = 0.0f;
    p->addRot.y = f;
    p->motParts.flags |= 0x40000000;
    p->addRot.z = 0.0f;
    f = fabsf(w->neckX);
    if (!(f < 0.01f)) {
        f = w->neckX * 0.33333334f;
        p = (cParts*) em->getPartsPtr(3);
        PSMTXRotRad(m, 'x', f);
        PSMTXConcat(p->worldMat, m, p->worldMat);
        p = (cParts*) em->getPartsPtr(4);
        PSMTXRotRad(m, 'x', f);
        PSMTXConcat(p->worldMat, m, p->worldMat);
        p = (cParts*) em->getPartsPtr(5);
        PSMTXRotRad(m, 'x', f);
        PSMTXConcat(p->worldMat, m, p->worldMat);
    }
}

void em22YarareInit(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    YarareInit(em, 0.0f, -100.0f, -150.0f, 250.0f, 650.0f, 2, 5);
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 150.0f, 100.0f, 6, 5);
    YarareAdd(em, &w->hit[1], 0.0f, -400.0f, 0.0f, 100.0f, 400.0f, 0xB, 1);
    YarareAdd(em, &w->hit[2], 0.0f, -400.0f, 0.0f, 100.0f, 400.0f, 0xF, 1);
    YarareAdd(em, &w->hit[3], 0.0f, -400.0f, 0.0f, 150.0f, 200.0f, 0x14, 1);
    YarareAdd(em, &w->hit[4], 0.0f, -400.0f, 0.0f, 150.0f, 200.0f, 0x18, 1);
}

void em22BloodSet(cEm22* em)
{
    int near;

    near = 0;
    if (em->dmPart->rad < 36000000.0f) {
        near = 1;
    }
    switch (em->dmWep) {
    case 7:
    case 8:
    case 0x21:
        if (near) {
            EmDmBloodSet2(em, 0x1A, 1, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x1A, 0, 0, 0, 0);
        }
        break;
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
    case 0x2B:
        EmDmBloodSet2(em, 0x1A, 0, 0, 0, 0);
        break;
    case 0x27:
        EmDmBloodSet2(em, 0x1A, 0, 0, 0, 0);
        break;
    case 0:
    case 0x14:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x2A:
    default:
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x15:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
        EmDmBloodSet2(em, 0x1A, 1, 0, 0, 0);
        break;
    }
}

int em22SetDmVal(cEm22* em)
{
    int near;
    int dm;

    near = 0;
    if (em->dmPart->rad < 36000000.0f) {
        near = 1;
    }
    dm = 100;
    if (em->dmWep <= 0x2D) {
        dm = GetWepDmVal(em, em->dmWep, near);
    }
    return dm;
}

int em22GetCamType(cEm22* em)
{
    Vec a;
    Vec b;

    a.x = -15.0f;
    a.y = 225.0f;
    a.z = -1604.0f;
    b.x = 115.0f;
    b.y = 349.0f;
    b.z = 136.0f;
    PSMTXMultVec(pPL->mat, &a, &a);
    PSMTXMultVec(pPL->mat, &b, &b);
    if (EatMgr.hitCheck(&b, &a, 0, 0, 0, 0)) {
        return 1;
    }
    return 0;
}

void em22CamMove(cEm22* em, int type)
{
    Em22Work* w = EM22_WK(em);
    Camera* gcam = &pG->Cam;
    Vec a;
    Vec b;
    Vec hit;
    Vec d;

    switch (type) {
    case 0:
    default:
        a.x = -373.0f;
        a.y = 101.0f;
        a.z = -1260.0f;
        b.x = 196.0f;
        b.y = 358.0f;
        b.z = -45.0f;
        break;
    case 1:
        a.x = 1221.0f;
        a.y = 1748.0f;
        a.z = 1085.0f;
        b.x = -137.0f;
        b.y = 211.0f;
        b.z = -208.0f;
        break;
    }
    PSMTXMultVec(pPL->mat, &a, &a);
    PSMTXMultVec(pPL->mat, &b, &b);
    PosToPos(&gcam->param.pos, &a, &w->cam.param.pos, 0.2f);
    PosToPos(&gcam->param.at, &b, &w->cam.param.at, 0.2f);
    if ((w->cam.param.pos.x - w->cam.param.at.x) * (w->cam.param.pos.x - w->cam.param.at.x) +
            (w->cam.param.pos.y - w->cam.param.at.y) * (w->cam.param.pos.y - w->cam.param.at.y) +
            (w->cam.param.pos.z - w->cam.param.at.z) * (w->cam.param.pos.z - w->cam.param.at.z) >
        100.0f) {
        PSVECSubtract(&w->cam.param.pos, &w->cam.param.at, &d);
#line 3428 "D:/Bio4/Prog/em22.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, 250.0f);
        PSVECAdd(&w->cam.param.pos, &d, &w->cam.param.pos);
        if (EatMgr.hitCheck(&w->cam.param.at, &w->cam.param.pos, &hit, 0, 0x8000, 0)) {
            w->cam.param.pos = hit;
        }
        PSVECSubtract(&w->cam.param.pos, &d, &w->cam.param.pos);
    }
    w->cam.up.x = 0.0f;
    w->cam.up.y = 1.0f;
    w->cam.up.z = 0.0f;
    w->cam.dist = SQRTF((w->cam.param.pos.x - w->cam.param.at.x) * (w->cam.param.pos.x - w->cam.param.at.x) +
                        (w->cam.param.pos.y - w->cam.param.at.y) * (w->cam.param.pos.y - w->cam.param.at.y) +
                        (w->cam.param.pos.z - w->cam.param.at.z) * (w->cam.param.pos.z - w->cam.param.at.z));
    w->cam.param.fovy = 50.0f;
    CameraSetOrientationUp(&w->cam);
    CamCtrl.x250 = (s32) &w->cam;
}

int em22SetParasiteCk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    if (w->flags & 0x10) {
        return 0;
    }
    if (w->paraWait) {
        w->paraWait--;
        return 0;
    }
    return 1;
}

void em22SetParasite(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    u16 step;
    u32 i;
    Vec pos;
    Vec rot;
    Vec sc;

    if (w->flags & 0x10) {
        return;
    }
    step = (*(u16*) ARC(0x3B) & 0x3FFF) / 5;
    for (i = 0; i < 5; i++) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = fRand1_1() * PI;
        rot.z = 0.0f;
        w->pPara[i] = (cObj16*) SetObj16(ARC(0x39), ARC(0x3A), em, em, i + 0x23, 5, &pos, &rot);
        if (w->pPara[i]) {
            MotSetObj16(w->pPara[i], ARC(0x3B), 4, step * i);
            if (i == 0 || i == 4) {
                sc.x = 1.3f;
                sc.y = 1.3f;
                sc.z = 1.3f;
            } else {
                sc.x = 1.5f;
                sc.y = 1.5f;
                sc.z = 1.5f;
            }
            w->pPara[i]->setScale(&sc);
        }
    }
    w->flags |= 0x10;
}

void em22SetParasiteAtk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    u16 step;
    u32 i;
    Vec pos;
    Vec rot;
    Vec sc;

    step = (*(u16*) ARC(0x49) & 0x3FFF) / 3;
    for (i = 0; i < 3; i++) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pParaAtk[i] = (cObj16*) SetObj16(ARC(0x47), ARC(0x48), em, em, i + 0x23, 6, &pos, &rot);
        if (w->pParaAtk[i]) {
            MotSetObj16(w->pParaAtk[i], ARC(0x49), 4, step * i);
            sc.x = 1.0f;
            sc.y = 1.0f;
            sc.z = 1.0f;
            w->pParaAtk[i]->setScale(&sc);
            w->pParaAtk[i]->motFlip = em22_para_flip;
        }
    }
}

void em22ParaSetMotWait(cEm22* em)
{
    u32 i;
    cObj16** pp = EM22_WK(em)->pParaAtk;

    for (i = 0; i < 3; i++) {
        if (pp[i]) {
            if (i != 1) {
                MotSetObj16(pp[i], ARC(0x49), 5, 0);
            } else {
                // Allocation lever (loop notes, no code): the 7th weighted em ref ranks em
                // above i in global-alloc (em r30, i r29).
                do { MotSetObj16(pp[1], ARC(0x49), 0x45, 0); } while (0);
            }
        }
    }
}

void em22ParaSetMotAtk(cEm22* em)
{
    u32 i;
    cObj16** pp = EM22_WK(em)->pParaAtk;

    for (i = 0; i < 3; i++) {
        if (pp[i]) {
            if (i != 1) {
                MotSetObj16(pp[i], ARC(0x4C), 1, 0);
            } else {
                // Allocation lever (loop notes, no code): the 7th weighted em ref ranks em
                // above i in global-alloc (em r30, i r29).
                do { MotSetObj16(pp[1], ARC(0x4C), 0x41, 0); } while (0);
            }
        }
    }
}

void em22ParaSetMotAtkHit(cEm22* em)
{
    u32 i;
    cObj16** pp = EM22_WK(em)->pParaAtk;

    for (i = 0; i < 3; i++) {
        if (pp[i]) {
            // default first: the arms are laid out default, 1, 2 (the nested if/else form put
            // the i == 2 body before the i == 1 body).
            switch (i) {
            default:
                MotSetObj16(pp[i], ARC(0x4A), 1, 0);
                break;
            case 1:
                MotSetObj16(pp[1], ARC(0x4B), 0x41, 0);
                break;
            case 2:
                MotSetObj16(pp[2], ARC(0x4B), 1, 5);
                break;
            }
        }
    }
}

void em22AtkParaClearCk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    u32 i;

    if (w->flags & 0x200) {
        return;
    }
    for (i = 0; i < 3; i++) {
        if (w->pParaAtk[i]) {
            w->pParaAtk[i]->clearLostWait();
            w->pParaAtk[i] = 0;
        }
    }
}

void em22OpenBack(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    cModel* p;

    if (w->flags & 0x10) {
        p = em->getPartsPtr(0x1F);
        p->rot.z = p->rot.z * 0.9f + 0.08726647f;
        p = em->getPartsPtr(0x20);
        p->rot.z = p->rot.z * 0.9f + -0.08726647f;
        p = em->getPartsPtr(0x21);
        p->rot.z = p->rot.z * 0.9f + 0.08726647f;
        p = em->getPartsPtr(0x22);
        p->rot.z = p->rot.z * 0.9f + -0.08726647f;
    }
}

int em22LockCk(cEm22* em)
{
    Mtx inv;
    Vec v;

    if (pPL->xFC != 0) {
        return 0;
    }
    if (pPL->xFD != 6) {
        return 0;
    }
    if (em->plDist2 > 64000000.0f) {
        return 0;
    }
    if (pG->wep_no == 0x10) {
        return 0;
    }
    if (ItemMgr.bulletNumCurrent() == 0) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) > 1.0471976f) {
        return 0;
    }
    if (pPL->pWep->pObj->wep.target && pPL->pWep->pObj->wep.target == em) {
        return 1;
    }
    PSMTXInverse(pPL->getPartsPtr(10)->mat, inv);
    PSMTXMultVec(inv, &em->getPartsPtr(0)->worldPos, &v);
    if (v.x > 0.0f) {
        return 0;
    }
    if (v.z > 500.0f) {
        return 0;
    }
    if (v.z < -500.0f) {
        return 0;
    }
    if (v.y > 500.0f) {
        return 0;
    }
    if (v.y < -500.0f) {
        return 0;
    }
    return 1;
}

void em22ScaleCompress(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    Mtx m;
    Vec s;
    cParts* p;

    if (em->xFC != 3) {
        return;
    }
    if (em->xFD == 0) {
        PSMTXIdentity(m);
        s.x = 1.0f;
        s.y = w->scale;
        s.z = 1.0f;
        ScaleMatrix(m, &s);
        for (p = (cParts*) em->pParts; p; p = p->pNext) {
            PSMTXConcat(m, p->mat, p->mat);
            p->mat[0][3] = p->worldPos.x;
            p->mat[1][3] = p->worldPos.y;
            p->mat[2][3] = p->worldPos.z;
        }
    }
}

void em22FootSeControl(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    u8 no;

    no = em->seNo;
    if (no == 0) {
        return;
    }
    no--;
    switch (no) {
    case 0:
    case 1:
        Ctrl11SetSe(w->pCtrl11, em, 3, no, 0xC);
        em->seNo = 0;
        break;
    case 2:
        Ctrl11SetSe(w->pCtrl11, em, 3, no, 0xD);
        em->seNo = 0;
        break;
    case 4:
    case 5:
    case 8:
    case 9:
        // Allocation lever (loop notes, no code): at depth 2 the two `w` refs count 3x (w 9 refs /
        // 30 insns beats em 11 / 47), so w takes r31 and em r30.
        do { do { w->x2C8 = Ctrl11SetSe(w->pCtrl11, em, 10, no, 0xE); } while (0); } while (0);
        em->seNo = 0;
        break;
    }
}

int em22JumpCk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    Vec nrm;
    Mtx m;
    Vec a;
    Vec b;
    cModel* p;

    if (w->stuckCnt <= 1) {
        return 0;
    }
    if (w->targetAngAbs > 0.5235988f) {
        return 0;
    }
    p = em->getPartsPtr(2);
    PSMTXCopy(em->mat, m);
    TransMatrix(m, &p->worldPos);
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = 600.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0) & 0x80020) {
        em->rot.y = atan2f(-nrm.x, -nrm.z);
        EmRoutineSet(em, 1, 0x13, 0, 0);
        return 1;
    }
    return 0;
}

int em22PlRunCk(cEm22* em)
{
    if (pPL->xFC != 0) {
        return 0;
    }
    return pPL->xFD == 3;
}

int em22PlRunCk2(cEm22* em)
{
    Mtx inv;
    Vec v;

    if (pPL->xFC != 0) {
        return 0;
    }
    if (pPL->xFD != 3) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) > 0.7853982f) {
        return 0;
    }
    PSMTXInverse(pPL->mat, inv);
    PSMTXMultVec(inv, &em->pos, &v);
    if (v.x > 1500.0f) {
        return 0;
    }
    if (v.x < -1500.0f) {
        return 0;
    }
    return 1;
}

// Splash at a foot (parts `no`) when the motion key says it touched the ground.
static inline void em22FootSplash(cEm22* em, int no)
{
    cModel* p = em->getPartsPtr(no);

    if (em->be_flag & 0x800) {
        EstSet(0, -1, &p->worldPos, 0, 0x1A, 3, 1, 0, (u32) em, 0);
    } else {
        EstSet(0, -1, &p->worldPos, 0, 0x1A, 3, 0, 0, 0, 0);
    }
}

void em22FootEff(cEm22* em)
{
    if (!(em->motEvent & 0x3C)) {
        return;
    }
    if (ChkWaterEffectEnable(&em->pos) == 0) {
        return;
    }
    if (em->motEvent & 4) {
        em22FootSplash(em, 0xC);
    }
    if (em->motEvent & 8) {
        em22FootSplash(em, 0x10);
    }
    if (em->motEvent & 0x10) {
        em22FootSplash(em, 0x15);
    }
    if (em->motEvent & 0x20) {
        em22FootSplash(em, 0x19);
    }
}

void em22ParaAtkHitPosSet(cEm22* em)
{
    Vec d;
    Vec a;
    Vec b;

    if (em->plDist2 > 6250000.0f) {
        return;
    }
    PSVECSubtract(&em->pos, &pPL->pos, &d);
    d.y = 0.0f;
#line 4013 "D:/Bio4/Prog/em22.cpp"
    VECNormalize(&d, &d);
    PSVECScale(&d, &d, 2500.0f);
    a = pPL->pos;
    a.y = em->pos.y + 500.0f;
    b = a;
    PSVECAdd(&b, &d, &b);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
        b.y = SatMgr.getFloor(&b, 600.0f, 100000.0f, 0, 0);
        em->pos = b;
    }
}

void em22SlaverSet(cEm22* em, int run)
{
    Em22Work* w = EM22_WK(em);

    if (w->slaverTimer) {
        w->slaverTimer--;
        return;
    }
    switch (run) {
    case 0:
    default:
        w->slaverTimer = 8;
        if (em->be_flag & 0x800) {
            EstSet((int) em, -1, 0, 0, 0x1A, 8, 1, 0, (u32) em, 0);
        } else {
            EstSet((int) em, -1, 0, 0, 0x1A, 8, 0, 0, (u32) em, 0);
        }
        break;
    case 1:
        w->slaverTimer = 14;
        if (em->be_flag & 0x800) {
            EstSet((int) em, -1, 0, 0, 0x1A, 9, 1, 0, (u32) em, 0);
        } else {
            EstSet((int) em, -1, 0, 0, 0x1A, 9, 0, 0, (u32) em, 0);
        }
        break;
    }
}

int em22GotoCk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);

    if (w->gotoOn == 0) {
        return 0;
    }
    EmRoutineSet(em, 1, 0, 0, 0);
    return 1;
}

void cEm22::setGoto(Vec* pos, int on)
{
    Em22Work* w = EM22_WK(this);
    f32 fl;

    w->gotoOn = on;
    w->gotoPos = *pos;
    fl = SatMgr.getFloor(&w->gotoPos, 600.0f, 100000.0f, 0, 0);
    if (fl != -100000.0f) {
        w->gotoPos.y = fl + 50.0f;
    }
}

int em22ScreenInCk(cEm22* em)
{
    Vec s;
    Vec p;

    p = em->pos;
    if (GetScreenPos(&p, &s) && s.x > 0.0f && s.x < 512.0f && s.y > 0.0f && s.y < 448.0f) {
        return 1;
    }
    p = em->getPartsPtr(4)->worldPos;
    if (GetScreenPos(&p, &s) && s.x > 0.0f && s.x < 512.0f && s.y > 0.0f && s.y < 448.0f) {
        return 1;
    }
    return 0;
}

// Work `no` of the enemy manager without the range check (the callers loop over nArray).
static inline cEmDoor* em22EmWork(u32 no)
{
    return (cEmDoor*) ((u8*) EmMgr.pArray + EmMgr.size * no);
}

void em22DoorOpenCk(cEm22* em)
{
    Em22Work* w = EM22_WK(em);
    u32 i;
    Vec v;
    f32 ang;

    if (w->stuckCnt % 10 != 5) {
        return;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEmDoor* door = em22EmWork(i);
        EmDoorWork* dw;

        if ((door->be_flag & 0x201) != 1) {
            continue;
        }
        if (door->id != 0x41) {
            continue;
        }
        if (door->hp <= 0) {
            continue;
        }
        if ((em->pos.x - door->pos.x) * (em->pos.x - door->pos.x) + (em->pos.y - door->pos.y) * (em->pos.y - door->pos.y) +
                (em->pos.z - door->pos.z) * (em->pos.z - door->pos.z) >
            6250000.0f) {
            continue;
        }
        dw = EMDOOR_WK(door);
        ang = fabsf(Muku2(dw->rotY, em->rot.y, PI));
        if (ang > 0.7853982f && ang < 2.3561945f) {
            continue;
        }
        PSMTXMultVec(dw->inv, &em->pos, &v);
        if (ang < 1.5707964f) {
            if (v.z > 0.0f) {
                continue;
            }
            if (v.z < -800.0f) {
                continue;
            }
        } else {
            if (v.z < 0.0f) {
                continue;
            }
            if (v.z > 800.0f) {
                continue;
            }
        }
        if (v.x > dw->width) {
            continue;
        }
        if (v.x < -dw->width) {
            continue;
        }
        if (v.y > 500.0f) {
            continue;
        }
        if (v.y < -500.0f) {
            continue;
        }
        {
            u32 st = door->ckOpen();

            if (st) {
                if (st <= 3) {
                    continue;
                }
            }
        }
        door->setOpen(&em->pos, 0, 0, 0);
    }
}
