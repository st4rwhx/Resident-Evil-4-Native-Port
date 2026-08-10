// em3b module (D:/Bio4/Prog/em3b.cpp): the truck (type 0) that runs into the barricade and the mine
// carts (type 1 running down the track, type 2 stopped). The driver is another enemy (work pDriver);
// the vehicles run over the player, the partner and any Ganado in front (em3bRunDownCk*).

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em3b.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "snd.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_npc.h"
#include "pl_wep.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");

typedef void (*Em3bFunc)(cEm3b*);

static void em3b_R0_Init(cEm3b* em);
static void em3b_R0_Move(cEm3b* em);
static void em3b_R1_Truck_Wait(cEm3b* em);
static void em3b_R1_Truck_Run(cEm3b* em);
static void em3b_R1_Truck_RunInto(cEm3b* em);
static void em3b_R1_Cart_Wait(cEm3b* em);
static void em3b_R1_Cart_Run(cEm3b* em);
static void em3b_R1_Cart_Damage(cEm3b* em);
static void em3b_R1_StopCart_Damage(cEm3b* em);
static void em3b_R1_Cart_Lost(cEm3b* em);
static void subem3bRunDown();

#define ARC(no) PL_ARC_PTR(em->subArc, no)

// Scalar-reference store: the following pPL/pSUB loads stay below it (they are reloaded after it).
static inline void U16Set(u16& d, int v)
{
    d = v;
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
static inline int em3bDeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

extern "C" void _prolog()
{
    OSReport("em3b prolog Ok\n");
    EmInitFunc = Em3bInit;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em3bInit(cEm* em)
{
    new (em) cEm3b();
}

void em3bDmCkTruck(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    int wep;
    int dmg;
    Vec* pos;
    cModel* p;

    if (em->dmHit == 0) {
        return;
    }
    wep = em->dmWep;
    em->dmHit = 0;
    if (wep == 0x14 || wep == 0x16 || wep == 0x17 || wep == 0x2A || wep == 0xE) {
        return;
    }
    p = em->getPartsPtr(0);
    em->dmType = 1;
    if (em->dmWep == 0x10) {
        em->dmType = 0x11;
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
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x2B:
        dmg = 200;
        break;
    case 7:
    case 8:
    case 0x21:
        dmg = 500;
        if (em->plDist2 > 16000000.0f) {
            dmg = 200;
        }
        break;
    case 5:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x29:
    case 0x2C:
    default:   // the default-grouped values are real tree nodes (root 0x10-0x11, casetree.py)
        dmg = 1000;
        break;
    case 0xE:
        dmg = 0;
        break;
    }
    LifeDownSet2(em, dmg, 0, 1);
    // the parts pointer crosses LifeDownSet2 (callee-saved copy right after getPartsPtr) and the
    // worldPos address is formed here; sched1 hoists the addi above the call
    pos = &p->worldPos;
    EmDmBloodSet2(em, 1, 0x21, 0, 0, 0);
    SndCall(6, 4, pos, 0, 0, em);
    if (em->hp <= 1 && w->dmgWait == 0) {
        w->dmgWait = 150;
        EstSet((int) em, -1, 0, 0, 1, 0x23, 0, 0, (u32) em, 0);
        SndCall(6, 6, pos, 0, 0, em);
        w->sndId = SndCall(6, 0xC, pos, 0, 0, em);
    }
}

void em3bDmCkCart(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);

    if ((em->be_flag & 2) && !em3bDeadCk(em) && em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 4:
        case 5:
        case 7:
            // the death body is written here AND in case 0x16 (jump2 cross-jumps the copies into the
            // later one; each copy stores the dmgWait register cse knows to be 0)
            if (w->dmgWait == 0) {
                w->dmgWait = 150;
                EmRoutineSet(em, 1, 5, 0, 0);
                EstSet((int) em, -1, 0, 0, 0xCA, 1, 0, w->espKind, (u32) em, 0);
                w->dmgWait = 150;
                return;
            }
        }
    }
    if (em->dmHit) {
        em->dmHit = 0;
        em->dmType = 1;
        // dmWep read directly at both uses: the byte store between them forces the reload the
        // target has before the switch (a u8 local keeps one load)
        if (em->dmWep == 0x10) {
            em->dmType = 0x11;
        }
        switch (em->dmWep) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 0xA:
        case 0xB:
        case 0xC:
        case 0xD:
        case 0xF:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x1B:
        case 0x1D:
        case 0x21:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2B:
        case 0x2C:
        case 0x2D:
            EmDmBloodSet2(em, 0xCA, 0, 0, 0, 0);
            goto stop_ck;
        case 0x17:
        case 0x2A:
            break;
        case 0:
        case 0xE:
        case 0x10:
        case 0x14:
        case 0x15:
        case 0x18:
        default:   // the default-grouped values are real tree nodes (root 0x16, casetree.py)
            EmDmBloodSet2(em, 0xCA, 4, 0, 0, 0);
            break;
        case 0x16:
        stop_ck:   // laid out after the default arm: the blood arm reaches it through the goto
            if (w->dmgWait == 0) {
                w->dmgWait = 150;
                EmRoutineSet(em, 1, 5, 0, 0);
                EstSet((int) em, -1, 0, 0, 0xCA, 1, 0, w->espKind, (u32) em, 0);
                w->dmgWait = 150;
            }
            break;
        }
    }
}

void em3bDmCkStopCart(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);

    if ((em->be_flag & 2) && !em3bDeadCk(em) && em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 4:
        case 5:
        case 7:
            // the death body is written here AND in case 0x16 (jump2 cross-jumps the copies into the
            // later one; each copy stores the dmgWait register cse knows to be 0)
            if (w->dmgWait == 0) {
                em->hp = 0;
                EmRoutineSet(em, 1, 7, 0, 0);
                return;
            }
        }
    }
    if (em->dmHit) {
        em->dmHit = 0;
        em->dmType = 1;
        // dmWep read directly at both uses: the byte store between them forces the reload the
        // target has before the switch (a u8 local keeps one load)
        if (em->dmWep == 0x10) {
            em->dmType = 0x11;
        }
        switch (em->dmWep) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 0xA:
        case 0xB:
        case 0xC:
        case 0xD:
        case 0xF:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x1B:
        case 0x1D:
        case 0x21:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2B:
        case 0x2C:
        case 0x2D:
            EmDmBloodSet2(em, 0xCA, 0, 0, 0, 0);
            goto stop_ck;
        case 0x17:
        case 0x2A:
            break;
        case 0:
        case 0xE:
        case 0x10:
        case 0x14:
        case 0x15:
        case 0x18:
        default:   // the default-grouped values are real tree nodes (root 0x16, casetree.py)
            EmDmBloodSet2(em, 0xCA, 4, 0, 0, 0);
            break;
        case 0x16:
        stop_ck:   // laid out after the default arm: the blood arm reaches it through the goto
            if (w->dmgWait == 0) {
                em->hp = 0;
                EmRoutineSet(em, 1, 7, 0, 0);
            }
            break;
        }
    }
}

Em3bFunc Em3b_R0_move_tbl[5] = {
    em3b_R0_Init,
    em3b_R0_Move,
    0,
    0,
    (Em3bFunc) Em_R0_Scenario,
};

static Em3bFunc Em3b_R1_move_tbl[9] = {
    em3b_R1_Truck_Wait,
    em3b_R1_Truck_Run,
    em3b_R1_Truck_RunInto,
    em3b_R1_Cart_Wait,
    em3b_R1_Cart_Run,
    em3b_R1_Cart_Damage,
    em3b_R1_Cart_Lost,
    em3b_R1_StopCart_Damage,
    0,
};

void cEm3b::move()
{
    Em3bWork* w = EM3B_WK(this);

    if (xFC != 0) {
        switch (type) {
        case 0:
        default:
            em3bDmCkTruck(this);
            break;
        case 1:
            em3bDmCkCart(this);
            break;
        case 2:
            em3bDmCkStopCart(this);
            break;
        }
    }
    w->flags &= ~0x1F;
    if (w->dmgWait) {
        w->dmgWait--;
    }
    Em3b_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em3bSlopeMove(this);
    partsWorldCalc();
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
}

static void em3b_R0_Init(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    cAtariInfo* at;
    int zero;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(5)) == 0) {
            pLog->err(0, 0, "em3b() Turck:ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    case 1:
    case 2:
        if (em->modelInit(ARC(0xB), ARC(0xC)) == 0) {
            pLog->err(0, 0, "em3b() Cart:ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    }
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 10000.0f, 10000.0f, 10000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    switch (em->type) {
    case 0:
    default:
        AtariInit(&em->atari, 0.0f, 0.0f, 0.0f, 900.0f, 3400.0f, 3400.0f, 1500.0f, 1, 2, 0);   // COMPILER-DIFF: #1
        break;
    case 1:
    case 2:
        AtariInit(&em->atari, 0.0f, 500.0f, 0.0f, 600.0f, 1500.0f, 1500.0f, 1500.0f, 0, 2, 0);   // COMPILER-DIFF: #1
        break;
    }
    at = &em->atari;
    em->setStatus(0xB);
    switch (em->type) {
    case 0:
    default:
        at->setPriority(3);
        at->flags &= ~0x100;
        YarareInitCube(em, 0.0f, -500.0f, 0.0f, 500.0f, 1500.0f, 3305.0f, 1, 1);
        YarareAddCube(em, &w->hit, 0.0f, 1000.0f, -1500.0f, 850.0f, 1500.0f, 1850.0f, 1, 1);
        break;
    case 1:
    case 2:
        at->setPriority(3);
        at->flags &= ~0x100;
        YarareInitCube(em, 0.0f, 0.0f, 0.0f, 700.0f, 1000.0f, 1300.0f, 1, 1);
        break;
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(5);
    EspDataLoad((u32) ARC(6), 0x30, 0);
    w->espKind = EspPullCoreKind();
    w->flags = zero;
    w->sndId2 = zero;
    w->sndId = zero;
    w->dmgWait = zero;
    switch (em->type) {
    case 0:
    default:
        EmRoutineSet(em, 1, 0, 0, 0);
        break;
    case 1:
    case 2:
        EmRoutineSet(em, 1, 3, zero, zero);
        break;
    }
    em3b_R0_Move(em);
}

static void em3b_R0_Move(cEm3b* em)
{
    Em3b_R1_move_tbl[em->xFD](em);
}

// Park the vehicle at the origin (the room moves it with its matrix).
static inline void em3bPosReset(cEm3b* em)
{
    em->pos.x = 0.0f;
    em->pos.y = 0.0f;
    em->pos.z = 0.0f;
    em->rot.x = 0.0f;
    em->rot.y = 0.0f;
    em->rot.z = 0.0f;
}

static void em3b_R1_Truck_Wait(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    int st = em->xFE;

    switch (st) {
    case 0:
        em3bPosReset(em);
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 1, 0);
        MotionMoveF(em, 0);
        w->timer = 10;
        em->xFE++;
        break;
    case 1:
        em3bPosReset(em);
        MotionSetCore(em, MOTION(em), ARC(7), 0, 0, 1, 0);
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else if (em->flags_3C8 & 1) {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
}

static void em3b_R1_Truck_Run(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    cModel* p = em->getPartsPtr(0);
    f32 f;

    switch (em->xFE) {
    case 0:
        em3bPosReset(em);
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 3, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        em3bPosReset(em);
        MotionSetCore(em, MOTION(em), ARC(7), 0, 3, 1, 0);
        w->timer = 450;
        w->seTimer = 30;
        EstSet((int) em, -1, 0, 0, 1, 0, 1, 0, (u32) em, 0);
        em->xFE++;
    case 3: {
        int end = MotionMoveF(em, 0);

        if (end) {
            em->xFE++;
            break;
        }
        em3bRunDownCkTruck(em);
        f = em->frame;
        if (f > 249.7f && f < 250.3f) {
            // the first stop stores the (zero) MotionMoveF result kept in a callee-saved register; the
            // second test's label has two uses (pDriver == 0 and the `&&` false path), so cse does not
            // carry the known zero into it and its literal zero is a fresh `li` — two copies survive
            if (w->pDriver && w->pDriver->hp <= 0) {
                EmRoutineSet(em, 1, 2, end, end);
                break;
            }
            if (em->hp <= 1) {
                EmRoutineSet(em, 1, 2, 0, 0);
                break;
            }
        }
        f = em->frame;
        if (f > 319.7f && f < 320.3f) {
            if ((w->pDriver && w->pDriver->hp <= 0) || em->hp <= 1) {
                EmRoutineSet(em, 1, 2, 0, 1);
                break;
            }
        }
        f = em->frame;
        if (f > 469.7f && f < 470.3f) {
            SndCall(6, 9, &p->worldPos, 0, 0, em);
        }
        if (w->timer) {
            w->timer--;
            if (w->seTimer) {
                w->seTimer--;
            } else {
                w->seTimer = (u8) (Rnd() % 60) + 30;
                SndCall(6, 7, &p->worldPos, 0, 0, em);
            }
        }
        f = em->frame;
        if (f > 464.7f && f < 465.3f) {
            EstSet((int) em, -1, 0, 0, 1, 0x24, 0, 0, (u32) em, 0);
            em->flags_3C8 |= 2;
            em->hp = 0;
            em->clearStatus(5);
            SndStop(w->sndId, 0);
        }
        break;
    }
    case 4:
        break;
    }
}

static void em3b_R1_Truck_RunInto(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    cModel* p = em->getPartsPtr(0);
    int st = em->xFE;
    f32 f;

    switch (st) {
    case 0: {
        int dir = em->xFF;

        if (dir) {
            MotionSetCore(em, MOTION(em), ARC(9), 0, 3, 1, 0);
            EstSet((int) em, -1, 0, 0, 1, 0x26, 0, 0, (u32) em, 0);
            w->timer = 60;
        } else {
            MotionSetCore(em, MOTION(em), ARC(8), 0, 3, 1, 0);
            EstSet((int) em, -1, 0, 0, 1, 0x25, 0, 0, (u32) em, 0);
            w->timer = 120;
        }
        em->xFE++;
    }
    case 1:
        if (MotionMoveF(em, 0)) {
            em->clearStatus(5);
            em->xFE++;
            break;
        }
        if (w->timer) {
            w->timer--;
            em3bRunDownCkTruck(em);
            if (w->timer == 0) {
                em->atari.pos.x = -150.0f;
                em->atari.pos.y = 0.0f;
                em->atari.pos.z = -200.0f;
                em->atari.rectX = 1500.0f;
            }
        }
        if (em->xFF) {
            f = em->frame;
            if (f > 19.7f && f < 20.3f) {
                SndCall(6, 0xA, &p->worldPos, 0, 0, em);
            }
            f = em->frame;
            if (f > 42.7f && f < 43.3f) {
                SndCall(6, 8, &p->worldPos, 0, 0, em);
                em->flags_3C8 |= 2;
                em->hp = 0;
                SndStop(w->sndId, 0);
            }
        } else {
            f = em->frame;
            if (f > 12.7f && f < 13.3f) {
                SndCall(6, 0xB, &p->worldPos, 0, 0, em);
            }
            f = em->frame;
            if (f > 110.7f && f < 111.3f) {
                SndCall(6, 8, &p->worldPos, 0, 0, em);
                em->flags_3C8 |= 2;
                em->hp = 0;
                SndStop(w->sndId, 0);
            }
        }
        break;
    case 2:
        break;
    }
}

static void em3b_R1_Cart_Wait(cEm3b* em)
{
    MotionSetCore(em, MOTION(em), ARC(0xD), 0, 0, 0, 0);
    MotionMoveF(em, 0);
}

static void em3b_R1_Cart_Run(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    int t;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xD), 0, 3, 5, 0);
        w->timer = 80;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        t = w->timer;
        if (t) {
            w->timer--;
        } else {
            Vec v;
            int zero;

            em->hp = t;
            v = em->pos;
            v.y += 1000.0f;
            zero = 0;
            PlWepHitCheck2(0, &v, &v, 0x13, 2, 6000.0f);
            pG->flags_500C |= 0x00800000;
            EffectEspDelete(0, w->espKind, (u32) em, 0);
            EffectEspgenDelete(0, w->espKind, (int) em);
            EffectEfmDelete(0, w->espKind, (int) em);
            EstSet((int) em, -1, 0, 0, 0xCA, 2, 0, 0, (u32) em, 0);
            w->dmgWait = 150;
            SndStop(w->sndId2, 0);
            SndCall(6, 0xA, &em->pos, 0, 0, em);
            EmRoutineSet(em, 1, 6, zero, zero);
        }
        em3bRunDownCkCart(em);
        break;
    }
}

static void em3b_R1_Cart_Damage(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE), 0, 3, 1, 0);
        w->sndId2 = SndCall(6, 9, &em->pos, 0, 0, em);
        EmSetDie(em);
        em->clearStatus(5);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 4, 0, 0);
        }
        break;
    }
}

static void em3b_R1_StopCart_Damage(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    int t;

    switch (em->xFE) {
    case 0:
        EstSet((int) em, -1, 0, 0, 0xCA, 3, 0, w->espKind, (u32) em, 0);
        w->dmgWait = 150;
        MotionSetCore(em, MOTION(em), ARC(0xE), 0, 3, 1, 0);
        w->timer = 1;
        EmSetDie(em);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        t = w->timer;
        if (t) {
            w->timer--;
        } else {
            Vec v;

            v = em->pos;
            v.y += 1000.0f;
            PlWepHitCheck2(0, &v, &v, 0x13, 2, 6000.0f);
            v.y += 3000.0f;
            PlWepHitCheck2(0, &v, &v, 0x13, 2, 6000.0f);
            pG->flags_500C |= 0x00800000;
            SndStop(w->sndId2, 0);
            SndCall(6, 0xA, &em->pos, 0, 0, em);
            EmRoutineSet(em, 1, 6, t, t);
        }
        break;
    }
}

static void em3b_R1_Cart_Lost(cEm3b* em)
{
    int st = em->xFE;

    if (st == 0) {
        em->be_flag &= ~2;
        em->atari.flags &= ~0x300;
        EmSetDie(em);
        em->hp = st;
        em->clearStatus(5);
        em->xFE++;
    }
}

// Run over the player, the partner and the Ganados in front of the truck.
// x/z squared distance: the temp computed BEFORE d fuses dx into d's register and keeps dz*dz standalone
// (em21 VsElgigante rule).
static inline f32 em3bDistXZ(cModel* p, Vec* q)
{
    f32 t;
    f32 d;

    t = (p->worldPos.x - q->x) * (p->worldPos.x - q->x);
    d = (p->worldPos.z - q->z) * (p->worldPos.z - q->z);
    d += t;
    return d;
}

void em3bRunDownCkTruck(cEm3b* em)
{
    Em3bWork* w = EM3B_WK(em);
    int parts[3] = { 0, 1, 4 };
    cModel* p;
    u32 i;

    if ((s16) pG->pl_life > 0) {
        for (i = 0; i < 3; i++) {
            int zero = 0;

            p = em->getPartsPtr(parts[i]);
            if (em3bDistXZ(p, &pPL->pos) < 6250000.0f) {
                U16Set(pG->pl_life, zero);
                pPL->rot.y += Muku(&pPL->pos, &p->worldPos, pPL->rot.y, PI);
                pPL->rot.y = LIMIT_ANGLE(em->rot.y);
                PlSetDamage(8, 0, 0);
                SndCall(1, 0x4B, &pPL->pos, 0, 0, 0);
                break;
            }
        }
    }
    if (pSUB && (s16) pG->sub_life > 0) {
        for (i = 0; i < 3; i++) {
            int zero = 0;

            p = em->getPartsPtr(parts[i]);
            if (em3bDistXZ(p, &pSUB->pos) < 6250000.0f) {
                U16Set(pG->sub_life, zero);
                // reference store: pSUB and rot.y are re-read for LIMIT_ANGLE (a plain store is forwarded)
                FSet(pSUB->rot.y, pSUB->rot.y + Muku(&pSUB->pos, &p->worldPos, pSUB->rot.y, PI));
                pSUB->rot.y = LIMIT_ANGLE(pSUB->rot.y);
                SetSubDamage((int) em, (void*) subem3bRunDown);
                SndCall(1, 0x4B, &pSUB->pos, 0, 0, 0);
                break;
            }
        }
    }
    p = em->getPartsPtr(0);
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        int zero = 0;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if (e == w->pDriver) {
            continue;
        }
        if (em3bDistXZ(p, &e->pos) < 20250000.0f) {
            e->hp = zero;
            EmRoutineSet(e, 3, 4, zero, zero);
            SndCall(1, 0x4B, &em->pos, 0, 0, 0);
        }
    }
}

// Run over the player and the Ganados in front of the cart.
void em3bRunDownCkCart(cEm3b* em)
{
    cModel* p;
    u32 i;

    if ((s16) pG->pl_life > 0 && !em3bDeadCk(pPL)) {
        for (i = 0; i < 2; i++) {
            p = em->getPartsPtr(1);
            if (em3bDistXZ(p, &pPL->pos) < 2250000.0f) {
                LifeDownSet(pPL, 500, 0);
                pPL->rot.y = em->rot.y + PI;
                pPL->rot.y = LIMIT_ANGLE(em->rot.y);
                PlSetDamage(8, 0, 0);
                break;
            }
        }
    }
    p = em->getPartsPtr(1);
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        int zero = 0;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if ((p->worldPos.x - e->pos.x) * (p->worldPos.x - e->pos.x) + (p->worldPos.y - e->pos.y) * (p->worldPos.y - e->pos.y)
            + (p->worldPos.z - e->pos.z) * (p->worldPos.z - e->pos.z) < 2890000.0f) {
            e->hp = zero;
            EmRoutineSet(e, 3, 4, zero, zero);   // the ff store is the zero's last use: issued first
        }
    }
}

// Partner damage routine (SetSubDamage callback): the run-over motion from the truck's archive.
static void subem3bRunDown()
{
    cSubChar* sub = pSUB;
    int st = sub->xFE;
    PlArc* arc = ((cEm*) sub->dmgType)->subArc;

    sub->subArc = arc;
    switch (st) {
    case 0:
        MotionSetCore(sub, MOTION(sub), PL_ARC_PTR(arc, 0xF), 0, 3, 1, 0);
        pG->sub_life = st;
        sub->xFE++;
    case 1:
        MotionMoveF(sub, 0);
        break;
    }
    sub->subArc = sub->subArc2;
}

// Tilt the running cart to the slope of the track.
void em3bSlopeMove(cEm3b* em)
{
    Mtx m;
    Vec a;
    Vec b;
    f32 fa;
    f32 fb;
    f32 ang;

    if (em->type != 1) {
        return;
    }
    if (em->motFlags2 & 0x40000000) {
        return;
    }
    fa = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
    // two statements into the function-scope fb: the difference and the fabs share fb's register (f1)
    fb = fa - em->pos.y;
    fb = fabsf(fb);
    if (fb > 500.0f) {
        return;
    }
    em->pos.y = fa;
    TransMatrix(em->mat, &em->pos);
    a.x = 0.0f;
    a.y = 1000.0f;
    a.z = 500.0f;
    b.x = 0.0f;
    b.y = 1000.0f;
    b.z = -500.0f;
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
    ang = -atan2f(fa, SQRTF((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z)));
    if (ang > PI / 6.0f) {
        ang = PI / 6.0f;
    }
    if (ang < -PI / 6.0f) {
        ang = -PI / 6.0f;
    }
    PSMTXRotRad(m, 'x', ang);
    PSMTXConcat(em->mat, m, em->mat);
    TransMatrix(em->mat, &em->pos);
}

int cEm3b::ckFire()
{
    if (EM3B_WK(this)->dmgWait != 0) {
        return 1;
    }
    return 0;
}
