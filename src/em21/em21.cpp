// em21 module (D:/Bio4/Prog/em21.cpp): the village dog. It sleeps, wanders and barks until the
// player comes close, then runs away (em21_R1_Escape, taking the other dogs along); the room 100 dog
// waits in a bear trap (em21_R1_R100TrapWait) until the player frees it (plemTrapCancel and its
// camera) and escapes; later it fights El Gigante at its side (em21_R1_VsElgigante).

#include "atari.h"
#include "dmg.h"
#include "em21.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "embarrel.h"
#include "at_mod.h"
#include "atari_init.h"
#include "act_btn.h"
#include "motion.h"
#include "route_ck.h"
#include "cam_ctrl.h"
#include "pad.h"
#include "player.h"
#include "pl_sub.h"
#include "snd.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

// The module's 0x30-byte COMMON block (st_room.h): uninitialised template statics of the original
// object, merged into .bss by the REL link.
asm(".comm common_em21,48,4");

extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");

typedef void (*Em21Func)(cEm21*);

static void em21_R0_Init(cEm21* em);
static void em21_R0_Move(cEm21* em);
static void em21_R1_Wait(cEm21* em);
static void em21_R1_Wander(cEm21* em);
static void em21_R1_Turn(cEm21* em);
static void em21_R1_Escape(cEm21* em);
static void em21_R1_Bark(cEm21* em);
static void em21_R1_R100TrapWait(cEm21* em);
static void em21_R1_R100TrapCancel(cEm21* em);
static void em21_R1_R100Escape(cEm21* em);
static void em21_R1_VsElgigante(cEm21* em);
static void em21TrapCancelAction(cEm21* em);
static void plemTrapCancel(cPlayer* pl);

#define ARC(no) PL_ARC_PTR(em->subArc, no)
#define PL_ARC(no) PL_ARC_PTR(pl->subArc, no)

// The enemy a player damage callback belongs to (pl_sub SetPlDamage's first argument).
#define PL_EM(pl) ((cEm*) (pl)->dmgType)

// Struct-member view of the player pointer: a load through it is not hoisted above the preceding
// stores through the work pointer (cam_ctrl.cpp PlayerPtr).
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

// Work `no` of the enemy manager with the range check kept (em.h's EmMgrWork lets jump threading
// fold it away inside the scan loops; the manager pointer local defeats it, db_light objWorkChkP).
static inline cEm* em21EmWork(u32 no)
{
    cEmMgr* m = &EmMgr;

    if (no >= m->nArray) {
        return 0;
    }
    return (cEm*) ((u8*) m->pArray + m->size * no);
}

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

// Dead flag test (cDmgInfo upper 16 bits): an inline returning 0/1 gives the `li 1; andis.; bne; li 0` chain.
static inline int em21DeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

extern "C" void _prolog()
{
    EmInitFunc = Em21Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em21Init(cEm* em)
{
    new (em) cEm21();
}

void em21DmCk(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    Vec hitPos;
    u8 mode;

    if (em21DeadCk(em) == 0) {
        switch (DmgMgr.hitCheck(&em->pos, &hitPos)) {
        case 1:
        case 4:
        case 5:
        case 7:
            mode = em->x38D;
            {
                register int c asm("r9"); // COMPILER-DIFF: #13
                c = 0x1E;
                em->dmType = c;
            }
            if (mode != 2) {
                if ((em->stat & 0xFFFF0000) == 0x01050000 && w->pTrap) {
                    w->pTrap->xFC = 1;
                    w->pTrap->xFD = 4;
                    w->pTrap->xFE = 0;
                    w->pTrap->xFF = 0;
                    em->xFC = 1;
                    em->dmType = 0x3C;
                    em->xFD = 7;
                    em->xFE = 0;
                    em->xFF = 1;
                } else {
                    EmRoutineSet(em, 1, 3, 0, 0);
                }
            }
            return;
        }
    }
    if (em->dmHit == 0) {
        return;
    }
    em->dmHit = 0;
    if (em->x38D == 2) {
        return;
    }
    if ((em->stat & 0xFFFF0000) == 0x01050000 && w->pTrap) {
        w->pTrap->xFC = 1;
        w->pTrap->xFD = 4;
        w->pTrap->xFE = 0;
        w->pTrap->xFF = 0;
        em->xFC = 1;
        em->dmType = 0x3C;
        em->xFD = 7;
        em->xFE = 0;
        em->xFF = 1;
    } else {
        em->dmType = 0x3C;
        EmRoutineSet(em, 1, 3, 0, 0);
    }
}

Em21Func Em21_R0_move_tbl[5] = {
    em21_R0_Init,
    em21_R0_Move,
    em21_R0_Move,
    em21_R0_Move,
    (Em21Func) Em_R0_Scenario,
};

static Em21Func Em21_R1_move_tbl[9] = {
    em21_R1_Wait,
    em21_R1_Wander,
    em21_R1_Turn,
    em21_R1_Escape,
    em21_R1_Bark,
    em21_R1_R100TrapWait,
    em21_R1_R100TrapCancel,
    em21_R1_R100Escape,
    em21_R1_VsElgigante,
};

// Parts index remap of the flipped motions (cModel::motFlip).
static u16 em21_flip_tbl[50] = {
    0, 1, 2, 3, 4, 5, 6, 8, 7, 0xD, 0xE, 0xF, 0x10, 9, 0xA, 0xB, 0xC, 0x11, 0x16, 0x17, 0x18, 0x19, 0x12, 0x13, 0x14,
    0x15, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
    0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31,
};

// The camera plemTrapCancel installs (the trap release cut): explicitly zero-initialised so it
// stays in .data.
static Camera em21_trap_cam = { 0 };
// COMPILER-DIFF: candidate #12 (cse related-value): `cam = &em21_trap_cam` after the `&em21_trap_cam.param.pos/at`
// pointers are known is a fresh `lis/addi` pair in the original; our cse rewrites it as `at - 0xB0`.
// An asm-labelled alias declaration gives cse a distinct SYMBOL_REF and keeps the fresh pair.
extern Camera em21_trap_cam_v asm("em21_trap_cam");
// .data is padded to 8 bytes before the linker's BSS tag word.
asm(".section .data\n\t.balign 8\n\t.text");

void cEm21::move()
{
    Em21Work* w = EM21_WK(this);
    f32 d;

    em21DmCk(this);
    w->flags &= ~2;
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
    Em21RouteCk(this);
    Em21_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em21NeckMove(this);
    partsWorldCalc();
    d = SQRTF((pos.x - oldPos.x) * (pos.x - oldPos.x) + (pos.z - oldPos.z) * (pos.z - oldPos.z));
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    if (SQRTF((pos.x - oldPos.x) * (pos.x - oldPos.x) + (pos.z - oldPos.z) * (pos.z - oldPos.z)) < d * 0.5f) {
        w->stuckTimer = 3;
    }
}

static void em21_R0_Init(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    cAtariInfo* at;
    int zero;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(5)) == 0) {
            pLog->err(0, 0, "em21() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    case 1:
        if (em->modelInit(ARC(4), ARC(6)) == 0) {
            pLog->err(0, 0, "em21() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    }
    em->setStatus(1);
    zero = 0;
    em->setStatus(0xB);
    at = &em->atari;
    em->hp = 1000;
    em->motFlip = em21_flip_tbl;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 1000.0f, 1000.0f, 0.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    at->init(3, 0x2000, 10, 0.0f, -500.0f, 0.0f, 450.0f, 400.0f, 400.0f, 1000.0f);
    em21YarareInit(em);
    w->tilt = 0.0f;
    w->plDist = 100000000.0f;
    w->stuckTimer = zero;
    w->neckX = 0.0f;
    w->neckY = 0.0f;
    w->pTrap = (cEm*) zero;
    w->sndId = zero;
    switch (em->x38D) {
    default:
        EmRoutineSet(em, 1, 0, 0, 0);
        break;
    case 1:
        at->setPriority(1);
        EmRoutineSet(em, 1, 5, 0, 0);
        break;
    case 2:
        EmRoutineSet(em, 1, 8, 0, 0);
        break;
    }
    MotionSetCore(em, MOTION(em), ARC(0xB), 0, 0, 5, 0);
    MotionMoveF(em, 0);
    em21_R0_Move(em);
}

static void em21_R0_Move(cEm21* em)
{
    Em21_R1_move_tbl[em->xFD](em);
}

static void em21_R1_Wait(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xB), 0, 30, 5, 0);
        w->timer = Rnd() % 3;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->timer) {
                w->timer--;
            } else {
                em->xFE++;
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xC), (int) ARC(0x17), 30, 1, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if (Rnd() & 1) {
                em21SetWanderPos(em);
                EmRoutineSet(em, 1, 1, 0, 0);
            } else {
                em->xFE = 0;
            }
        }
        break;
    }
    if (em21WakeCk(em)) {
        w->escTimer = 2;
        EmRoutineSet(em, 1, 3, 0, 0);
    }
}

static void em21_R1_Wander(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(7), (int) ARC(0xD), 30, 5, 0);
        w->timer = Rnd() % 5;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, PI / 128.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em21DirMatrix(em, 0.0f);
        MotionMoveF(em, 0);
        if ((em->pos.x - w->wanderPos.x) * (em->pos.x - w->wanderPos.x) + (em->pos.z - w->wanderPos.z) * (em->pos.z - w->wanderPos.z)
            < 640000.0f) {
            EmRoutineSet(em, 1, 0, 2, 0);
        }
        break;
    }
    if (em21WakeCk(em)) {
        w->escTimer = 2;
        EmRoutineSet(em, 1, 3, 0, 0);
    }
}

static void em21_R1_Turn(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    w->flags |= 2;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(7), (int) ARC(0xD), 30, 5, 0);
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, PI / 64.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em21DirMatrix(em, 0.0f);
        MotionMoveF(em, 0);
        break;
    }
    if (em->plDist2 < 16000000.0f) {
        EmRoutineSet(em, 1, 3, 0, 0);
        w->escTimer = 2;
    } else if (w->routeAngAbs < PI / 8.0f) {
        EmRoutineSet(em, 1, 4, 0, 0);
    }
}

static void em21_R1_Escape(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    f32 t;

    if (em->plDist2 < 25000000.0f) {
        w->flags |= 2;
    }
    if (em->xFE == 0) {
        RouteCkEscEm(em, pPL, &w->targetPos);
        t = fabsf(Muku(&em->pos, &w->targetPos, em->rot.y, PI));
        if (t > 2.0943952f) {
            em->xFE = 2;
        }
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 0xB, &em->pos, em->id, 0, em);
        if (!(pGS->flags_5010 & 0x20000000)) {
            BitOn(pG->flags_5010, 0x20000000);
            memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &em->pos, sizeof(Vec));
            pG->bell_stat = 0;
        }
    }
    w->escTimer = 2;
    switch (em->xFE) {
    case 0:
        switch (Rnd() & 3) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(9), (int) ARC(0x12), 3, 5, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(9), (int) ARC(0x13), 3, 5, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(9), (int) ARC(0x14), 3, 5, 0);
            break;
        case 3:
            MotionSetCore(em, MOTION(em), ARC(9), (int) ARC(0x15), 3, 5, 0);
            break;
        }
        em21EscapeWithYou(em);
        w->escAng = GetXZAngle(&pPL->pos, &em->pos);
        w->escAng += fRand1_1() * (PI / 2.0f);
        w->escAng = LIMIT_ANGLE(w->escAng);
        w->timer = Rnd() % 3 + 3;
        w->timer2 = Rnd() % 30 + 30;
        em->xFF = Rnd() & 1;
        em->xFE++;
    case 1:
        if (w->timer2) {
            w->timer2--;
        } else {
            w->timer2 = Rnd() % 15 + 15;
            w->escAng += fRand1_1() * (PI / 4.0f);
            w->escAng = LIMIT_ANGLE(w->escAng);
        }
        t = Muku2(em->rot.y, w->escAng, PI / 40.0f);
        em->rot.y += t;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em21DirMatrix(em, t);
        if (w->stuckTimer > 3) {
            if (em->xFF) {
                w->escAng += PI / 5.0f;
            } else {
                w->escAng -= PI / 5.0f;
            }
            w->escAng = LIMIT_ANGLE(w->escAng);
        }
        if (MotionMoveF(em, 0)) {
            if (w->timer) {
                w->timer--;
            } else {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xA), (int) ARC(0x16), 3, 1, 0);
        em21EscapeWithYou(em);
        em->xFE++;
    case 3:
        w->timer = Rnd() % 30 + 30;
        if (em->seFlags28B & 2) {
            if (w->stuckTimer) {
                t = 0.058904864f;
            } else {
                t = PI / 40.0f;
            }
            em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, t);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
            em21DirMatrix(em, Muku(&em->pos, &w->targetPos, em->rot.y, PI));
        } else {
            em21DirMatrix(em, 0.0f);
        }
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    }
}

static void em21_R1_Bark(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    f32 ang;

    w->flags |= 2;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xB), 0, 30, 5, 0);
        em->xFE++;
    case 1:
        em21DirMatrix(em, 0.0f);
        if (MotionMoveF(em, 0)) {
            int zero = 0;

            em->xFE = zero;
            if (w->routeAngAbs > PI / 4.0f) {
                EmRoutineSet(em, 1, 2, 0, 0);
                break;
            }
        }
        ang = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, PI));
        if (ang < PI / 4.0f && em->plDist2 < 100000000.0f) {
            em->xFE++;
        }
        break;
    case 2:
        switch (Rnd() & 3) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(8), (int) ARC(0xE), 30, 5, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(8), (int) ARC(0xF), 30, 5, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(8), (int) ARC(0x10), 30, 5, 0);
            break;
        case 3:
            MotionSetCore(em, MOTION(em), ARC(8), (int) ARC(0x11), 30, 5, 0);
            break;
        }
        em->xFE++;
    case 3:
        em21DirMatrix(em, 0.0f);
        if (MotionMoveF(em, 0)) {
            int zero = 0;

            em->xFE = zero;
            if (w->routeAngAbs > PI / 4.0f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                ang = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, PI));
                if (ang > 1.0471976f || em->plDist2 > 144000000.0f) {
                    em->xFE = zero;
                }
            }
        }
        break;
    }
    if (em->plDist2 < 25000000.0f) {
        w->escTimer = 2;
        EmRoutineSet(em, 1, 3, 0, 0);
    }
}

static void em21_R1_R100TrapWait(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x18), 0, 0, 5, 0);
        w->pTrap = 0;
        w->timer = Rnd() % 30 + 75;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->timer = Rnd() % 30 + 75;
            SndStop(w->sndId, 0);
            w->sndId = SndCall(8, 0xC, &em->pos, em->id, 0, em);
        }
        MotionMoveF(em, 0);
        em21TrapSearch(em);
        if (w->pTrap && (w->pTrap->stat & 0xFFFF0000) == 0x01040000) {
            em->dmType = 0x3C;
            EmRoutineSet(em, 1, 7, 0, 1);
            return;
        }
        break;
    }
    if (!(em->plDist2 > 9000000.0f)) {
        ActBtn.set(0x15, 5, (int) em21TrapCancelAction, (int) em, 0, 1, 0, 0);
    }
}

static void em21_R1_R100TrapCancel(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x19), (int) ARC(0x1C), 3, 5, 0);
        if (w->pTrap) {
            w->pTrap->flags_3C8 |= 1;
        }
        SndStrReq(1, 0xE, 0x80000003, 0, 0, 0.0f);
        pG->flags_51BC |= 0x80000;
        em->hp = 0;
        em->atari.flags &= ~0x200;
        em->xFE++;
    case 1:
        em->dmType = 2;
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 7, 0, 0);
        }
        break;
    }
}

static void em21_R1_R100Escape(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    Vec target = { -24950.0f, -400.0f, -26740.0f };

    switch (em->xFE) {
    case 0:
        EmSetDie(em);
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(9), (int) ARC(0x12), 3, 5, 0);
            SndStop(w->sndId, 0);
            w->sndId = SndCall(8, 0xB, &em->pos, em->id, 0, em);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x1A), (int) ARC(0x1D), 3, 5, 0);
        }
        em->atari.flags |= 0x100;
        MotionMoveF(em, 0);
        em->xFE++;
    case 1: {
        f32 lim;

        if (w->stuckTimer) {
            lim = 0.058904864f;
        } else {
            lim = PI / 40.0f;
        }
        em->rot.y += Muku(&em->pos, &target, em->rot.y, lim);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em21DirMatrix(em, Muku(&em->pos, &target, em->rot.y, PI));
        MotionMoveF(em, 0);
        if ((em->pos.x - target.x) * (em->pos.x - target.x) + (em->pos.z - target.z) * (em->pos.z - target.z) < 36000000.0f) {
            em->xFE++;
        }
        break;
    }
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x1E), 0, 3, 1, 0);
        em->atari.flags &= ~0x100;
        MotionMoveF(em, 0);
        em->xFE++;
    case 3:
        if (em->motFrame > 2.7f && em->motFrame < 3.3f) {
            SndCall(8, 7, &em->pos, em->id, 0, em);
        }
        em21DirMatrix(em, 0.0f);
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x1A), (int) ARC(0x1D), 3, 5, 0);
        MotionMoveF(em, 0);
        em->xFE++;
    case 5:
        em21DirMatrix(em, 0.0f);
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 6:
        MotionMoveF(em, 0);
        em->atari.flags &= ~0x300;
        em->alpha -= 0.1f;
        if (em->alpha <= 0.0f) {
            em->be_flag &= ~2;
            em->alpha = 0.0f;
            em->xFE++;
        }
        break;
    }
}

static void em21_R1_VsElgigante(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    cEm* g = w->pGigante;
    f32 ang;
    f32 d;

    w->flags |= 8;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x24), 0, 0, 5, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em21SearchElgigante(em)) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x20), 0, 5, 5, 0);
        w->timer = Rnd() % 60 + 60;
        em->xFE++;
    case 3:
        w->flags |= 2;
        if (w->timer) {
            w->timer--;
        } else {
            w->timer = Rnd() % 60 + 60;
            w->sndId = SndCall(8, 5, &em->pos, em->id, 0, em);
        }
        if (MotionMoveF(em, 0) && (Rnd() & 3) == 0) {
            em->xFE++;
            break;
        }
        if (g->hp <= 0) {
            em->xFE = 0x10;
            break;
        }
        if ((em->pos.x - g->pos.x) * (em->pos.x - g->pos.x) + (em->pos.z - g->pos.z) * (em->pos.z - g->pos.z) < 25000000.0f
            && (g->flags_3C8 & 4)) {
            em->xFE = 6;
            break;
        }
        if (em21DeadCk(em)) {
            em->xFE = 6;
        } else {
            ang = fabsf(Muku(&em->pos, &g->pos, em->rot.y, PI));
            if (ang > 0.87266463f) {
                em->xFE = 0xE;
            }
        }
        break;
    case 4:
        switch (Rnd() & 3) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(8), (int) ARC(0xE), 30, 5, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(8), (int) ARC(0xF), 30, 5, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(8), (int) ARC(0x10), 30, 5, 0);
            break;
        case 3:
            MotionSetCore(em, MOTION(em), ARC(8), (int) ARC(0x11), 30, 5, 0);
            break;
        }
        em->xFE++;
    case 5:
        w->flags |= 2;
        if (MotionMoveF(em, 0)) {
            em->xFE = 2;
            break;
        }
        if ((em->pos.x - g->pos.x) * (em->pos.x - g->pos.x) + (em->pos.z - g->pos.z) * (em->pos.z - g->pos.z) < 25000000.0f
            && (g->flags_3C8 & 4)) {
            em->xFE = 6;
            break;
        }
        if (em21DeadCk(em)) {
            em->xFE = 6;
        } else {
            ang = fabsf(Muku(&em->pos, &g->pos, em->rot.y, PI));
            if (ang > 0.87266463f) {
                em->xFE = 0xE;
            }
        }
        break;
    case 6:
        if (Rnd() & 2) {
            MotionSetCore(em, MOTION(em), ARC(0x1F), (int) ARC(0x22), 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x1F), (int) ARC(0x22), 3, 1, 0);
        }
        em->xFE++;
    case 7:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
            if (em21GetBarkPos(em) == 0) {
                em->xFE = 2;
                break;
            }
            if (fabsf(Muku(&em->pos, &w->barkPos, em->rot.y, PI)) < 2.0943952f) {
                em->xFE = 8;
                break;
            }
            em->xFE = 0xA;
        }
        break;
    case 8:
        MotionSetCore(em, MOTION(em), ARC(0x1A), (int) ARC(0x1D), 3, 5, 0);
        em->xFE++;
    case 9:
        w->flags |= 2;
        // the Muku limit shares the `ang` variable: its f2 argument copy gives the pseudo an f2
        // preference, so `fabs f2, f1` in every arm
        if (w->stuckTimer) {
            ang = 0.058904864f;
        } else {
            ang = PI / 40.0f;
        }
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, ang);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em21DirMatrix(em, Muku(&em->pos, &w->targetPos, em->rot.y, PI));
        MotionMoveF(em, 0);
        {
            // temp computed BEFORE d: combine then substitutes t (the later LOG_LINK is tried
            // first) and the fmadds keeps d's own register for the dz*dz term
            f32 t = (em->pos.x - w->barkPos.x) * (em->pos.x - w->barkPos.x);
            d = (em->pos.z - w->barkPos.z) * (em->pos.z - w->barkPos.z);
            d += t;
        }
        if (d < 4000000.0f) {
            ang = fabsf(Muku(&em->pos, &g->pos, em->rot.y, PI));
            if (ang < 1.7453293f) {
                em->xFE = 2;
                break;
            }
            em->xFE = 0xC;
            break;
        }
        if (em21DeadCk(em)) {
            em->xFE = 6;
        }
        break;
    case 0xA:
        MotionSetCore(em, MOTION(em), ARC(0xA), (int) ARC(0x16), 3, 1, 0);
        em->xFE++;
    case 0xB:
        if (MotionMoveF(em, 0)) {
            em->xFE = 8;
            break;
        }
        if (em21DeadCk(em)) {
            em->xFE = 6;
        }
        break;
    case 0xC:
        MotionSetCore(em, MOTION(em), ARC(0x21), (int) ARC(0x23), 3, 1, 0);
        em->xFE++;
    case 0xD:
        if (MotionMoveF(em, 0)) {
            em->xFE = 2;
            break;
        }
        if (em21DeadCk(em)) {
            em->xFE = 6;
        }
        break;
    case 0xE:
        MotionSetCore(em, MOTION(em), ARC(0x26), (int) ARC(0x27), 3, 5, 0);
        em->xFE++;
    case 0xF:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, PI / 64.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em21DirMatrix(em, 0.0f);
        MotionMoveF(em, 0);
        ang = fabsf(Muku(&em->pos, &g->pos, em->rot.y, PI));
        if (ang < 0.34906585f) {
            em->xFE = 2;
            break;
        }
        if ((em->pos.x - g->pos.x) * (em->pos.x - g->pos.x) + (em->pos.z - g->pos.z) * (em->pos.z - g->pos.z) < 9000000.0f
            && (g->flags_3C8 & 4)) {
            em->xFE = 6;
            break;
        }
        if (em21DeadCk(em)) {
            em->xFE = 6;
        }
        break;
    case 0x10:
        MotionSetCore(em, MOTION(em), ARC(0x25), 0, 10, 5, 0);
        em->xFE++;
    case 0x11:
        w->flags |= 2;
        MotionMoveF(em, 0);
        break;
    }
}

int em21SearchElgigante(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if (!e->isAlive()) {
            continue;
        }
        if (e->id != 0x2B) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (e->checkStatus(5) == 0) {
            continue;
        }
        w->pGigante = e;
        return 1;
    }
    return 0;
}

static void em21TrapCancelAction(cEm21* em)
{
    pPL->dmType = 2;
    em->dmType = 2;
    SetPlDamage((int) em, plemTrapCancel);
    EmRoutineSet(em, 1, 6, 0, 0);
}

static void plemTrapCancel(cPlayer* pl)
{
    cEm* em = PL_EM(pl);
    Vec v;

    pl->subArc = PL_EM(pPL)->subArc;
    switch (pl->xFE) {
    case 0:
        v.x = 431.86002f;
        v.y = 0.0f;
        v.z = -989.88f;
        pl->rot.y = em->rot.y;
        PSMTXMultVec(em->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x1B), 0, 0, 1, 0);
        pl->xFE++;
    case 1:
        pl->dmType = 2;
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    }
    pl->subArc = pl->subArc2;
    plem21TrapCamMove(pl);
}

void plem21TrapCamMove(cModel* m)
{
    Camera* c = &pG->Cam;
    Camera* cam;
    Vec v;
    Vec a;

    v.x = 2500.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(m->mat, &v, &v);
    v.y += 2000.0f;
    a.x = 0.0f;
    a.y = 1000.0f;
    a.z = 1000.0f;
    PSMTXMultVec(m->mat, &a, &a);
    PosToPos(&c->param.at, &a, &em21_trap_cam.param.at, 1.0f);
    PosToPos(&c->param.pos, &v, &em21_trap_cam.param.pos, 1.0f);
    {
        Vec* pos = &em21_trap_cam.param.pos;
        Vec* at = &em21_trap_cam.param.at;
        f32 dx = pos->x - at->x;
        f32 dy = pos->y - at->y;
        f32 dz = pos->z - at->z;

        cam = &em21_trap_cam_v;   // COMPILER-DIFF: candidate #12
        cam->up.x = 0.0f;
        cam->up.y = 1.0f;
        cam->up.z = 0.0f;
        cam->dist = SQRTF(dx * dx + dy * dy + dz * dz);
    }
    cam->param.fovy = 55.0f;
    CameraSetOrientationUp(cam);
    CamCtrl.x250 = (s32) cam;
}

void Em21RouteCk(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    if (em->hp <= 0) {
        return;
    }
    if ((pG->flags_51E4 & 7) != (em->emsetNo & 7)) {
        return;
    }
    w->flags &= ~1;
    if (RouteCkToPos(em, &pPLS->pos, &w->routePos, 0, 0)) {
        w->flags |= 1;
    }
    w->routeAng = Muku(&em->pos, &w->routePos, em->rot.y, PI);
    w->routeAngAbs = fabsf(w->routeAng);
    if (w->flags & 8) {
        RouteCkToPos(em, &w->barkPos, &w->targetPos, 0, 0);
        w->targetAng = Muku(&em->pos, &w->targetPos, em->rot.y, PI);
        w->targetAngAbs = fabsf(w->targetAng);
        w->targetDist2 = (em->pos.x - w->barkPos.x) * (em->pos.x - w->barkPos.x)
                         + (em->pos.z - w->barkPos.z) * (em->pos.z - w->barkPos.z);
        w->pTarget = 0;
        return;
    }
    if (w->flags & 4) {
        RouteCkToPos(em, &w->wanderPos, &w->targetPos, 0, 0);
        w->targetAng = Muku(&em->pos, &w->targetPos, em->rot.y, PI);
        w->targetAngAbs = fabsf(w->targetAng);
        w->targetDist2 = (em->pos.x - w->wanderPos.x) * (em->pos.x - w->wanderPos.x)
                         + (em->pos.z - w->wanderPos.z) * (em->pos.z - w->wanderPos.z);
        w->pTarget = 0;
    } else {
        w->targetPos = w->routePos;
        w->targetAng = w->routeAng;
        w->targetAngAbs = w->routeAngAbs;
        w->targetDist2 = em->plDist2;
        w->pTarget = pPLS;
    }
    w->flags &= ~4;
    w->plDist = RouteCkPosToPosDis(&em->pos, &pPLS->pos);
    if (w->escTimer) {
        RouteCkEscEm(em, pPL, &w->targetPos);
    }
}

void em21DirMatrix(cEm21* em, f32 dir)
{
    Em21Work* w = EM21_WK(em);
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

void em21NeckMove(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    Mtx m;
    Vec d;
    cModel* tp;
    cParts* p;
    f32 f;
    f32 len;

    if (w->pGigante && w->pGigante->hp > 0) {
        tp = w->pGigante->getPartsPtr(0);
    } else {
        tp = pPL->getPartsPtr(4);
    }
    p = (cParts*) em->getPartsPtr(4);
    if (w->flags & 2) {
        // do{}while(0) (COMPILER-DIFF-tagged tie lever): the refs inside count twice, so tp
        // outranks em in global-alloc (tp r29, em r28); the notes sit at block edges (no barrier)
        do {
            f = Muku(&em->pos, &p->worldPos, em->rot.y, 1.0471976f);
            w->neckY += Muku2(w->neckY, f, 0.098174773f);
            PSVECSubtract(&tp->worldPos, &p->worldPos, &d);
            len = SQRTF(d.x * d.x + d.z * d.z);
        } while (0);
        f = -atan2f(d.y, len);
        if (f > 0.7853982f) {
            f = 0.7853982f;
        }
        if (f < -0.7853982f) {
            f = -0.7853982f;
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
    p->addRot.y = f;
    p->motParts.flags |= 0x40000000;
    p->addRot.x = 0.0f;
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

int em21WakeCk(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    if (em->plDist2 < 2250000.0f) {
        return 1;
    }
    if (em->plDist2 < 16000000.0f && w->routeAngAbs < PI / 4.0f) {
        return 1;
    }
    if ((int) pG->flags_5010 < 0 && em->plDist2 < 16000000.0f) {
        return 1;
    }
    if ((pG->flags_5010 & 0x40000000) || (pG->flags_500C & 0x00800000)) {
        if (w->plDist < 15000.0f) {
            return 1;
        }
    }
    if (pG->flags_5010 & 0x20000000) {
        f32 r;

        // three identical arms + the override after the switch: the arm sets are dead (the
        // compare skeleton stays) and the block-local `r` is loaded at the use (em3cFindCk idiom)
        switch (pG->bell_stat) {
        case 0:
            r = 15000.0f;
            break;
        case 1:
            r = 15000.0f;
            break;
        default:
            r = 15000.0f;
            break;
        }
        r = 15000.0f;
        if ((em->pos.x - pG->bell_pos.x) * (em->pos.x - pG->bell_pos.x) + (em->pos.y - pG->bell_pos.y) * (em->pos.y - pG->bell_pos.y)
                + (em->pos.z - pG->bell_pos.z) * (em->pos.z - pG->bell_pos.z)
            < r * r) {
            return 1;
        }
    }
    return 0;
}

void em21YarareInit(cEm21* em)
{
    Em21Work* w = EM21_WK(em);

    YarareInit(em, 0.0f, -100.0f, -150.0f, 250.0f, 650.0f, 2, 5);
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 150.0f, 100.0f, 6, 5);
    YarareAdd(em, &w->hit[1], 0.0f, -400.0f, 0.0f, 100.0f, 400.0f, 0xB, 1);
    YarareAdd(em, &w->hit[2], 0.0f, -400.0f, 0.0f, 100.0f, 400.0f, 0xF, 1);
    YarareAdd(em, &w->hit[3], 0.0f, -400.0f, 0.0f, 150.0f, 200.0f, 0x14, 1);
    YarareAdd(em, &w->hit[4], 0.0f, -400.0f, 0.0f, 150.0f, 200.0f, 0x18, 1);
}

void em21EscapeWithYou(cEm21* em)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = em21EmWork(i);

        if (!(e->be_flag & 1)) {
            continue;
        }
        if (!(e->be_flag & 0x20)) {
            continue;
        }
        if (e->id != 0x21) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (e->xFC != 1) {
            continue;
        }
        if (e->xFD == 4 || e->xFD == 0 || e->xFD == 2) {
            if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) > 9000000.0f) {
                continue;
            }
            EmRoutineSet(e, 1, 3, 0, 0);
        }
    }
}

void em21SetWanderPos(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    Mtx m;
    Vec a;
    Vec b;
    Vec hit;
    f32 ang;

    ang = LIMIT_ANGLE(em->rot.y + fRand1_1() * (PI / 2.0f));
    a = em->pos;
    a.y += 500.0f;
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = fRand0_1() * 3000.0f + 3000.0f;
    PSMTXRotRad(m, 'y', ang);
    TransMatrix(m, &em->pos);
    PSMTXMultVec(m, &b, &b);
    if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0)) {
        b = hit;
    }
    w->wanderPos = b;
    w->flags |= 4;
}

int em21TrapSearch(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    u32 i;

    if (w->pTrap) {
        return 0;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = em21EmWork(i);

        if (!(e->be_flag & 1)) {
            continue;
        }
        if (!(e->be_flag & 0x20)) {
            continue;
        }
        if (e->id != 0x2A) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (e->type != 0) {
            continue;
        }
        if (e->x38D == 1) {
            Vec v;

            w->pTrap = e;
            e->rot.y = em->rot.y;
            v.x = 256.48f;
            v.y = 0.0f;
            v.z = -304.75f;
            PSMTXMultVec(em->mat, &v, &e->pos);
            return 1;
        }
    }
    return 0;
}

int em21GetBarkPos(cEm21* em)
{
    Em21Work* w = EM21_WK(em);
    EmiData* emi = (EmiData*) pG->pRoomEmi;
    EmiEntry* e;
    f32 best = 0.0f;
    int idx;
    int i;

    if (emi == 0) {
        return 0;
    }
    idx = -1;
    for (i = 0; i < emi->n; i++) {
        e = &emi->entry[i];
        if (e->type != 0xA) {
            continue;
        }
        if (idx == -1) {
            idx = i;
            best = (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z);
        } else {
            f32 d = (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z);

            if (!(d < best)) {
                best = d;
                idx = i;
            }
        }
    }
    if (idx == -1) {
        return 0;
    }
    e = &((EmiData*) pG->pRoomEmi)->entry[idx];
    w->barkPos = e->pos;
    return 1;
}
