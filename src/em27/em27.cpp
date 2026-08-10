// em27 module (D:/Bio4/Prog/em27.cpp): the lake fish. Swims towards a target point (the player while
// chaseTimer runs, else its home or a random point), with wait / walk / dash / bank / turn / jump
// routines, three damage reactions and a die routine that floats the body up to the surface.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em27.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "motion.h"
#include "sce_at.h"
#include "snd.h"
#include "player.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");

typedef void (*Em27Func)(cEm27*);

static void em27_R0_Init(cEm27* em);
static void em27_R0_Move(cEm27* em);
static void em27_R1_Wait(cEm27* em);
static void em27_R1_Walk(cEm27* em);
static void em27_R1_Dash(cEm27* em);
static void em27_R1_Bank(cEm27* em);
static void em27_R1_Turn180(cEm27* em);
static void em27_R1_Jump(cEm27* em);
static void em27_R0_Damage(cEm27* em);
static void em27_R1_Dm_Normal(cEm27* em);
static void em27_R1_Dm_Big(cEm27* em);
static void em27_R1_Dm_Air(cEm27* em);
static void em27_R0_Die(cEm27* em);
static void em27_R1_Die_Normal(cEm27* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)

// Collision flag bits cleared through the info's address (`addi rX, em, 0x2b4; lhz 0x1a(rX)`).
static inline void AtariOff(cAtariInfo* at, u16 mask) { at->flags &= mask; }

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

// math_sub.h's VECNormalize with the log pointer read as a plain struct member: the inline
// `cLogPtr::operator->` puts two block notes between `high(pLog)` and the load, which raises the
// loop.c lifetime of the high pseudo from 1 to 3 and gets it hoisted out of the em27ObaHitCk loop
// (the original keeps `lis pLog@ha` inside the loop).
#define VECNormalizeP(src, dst)                                                         \
    if (0.0f == (src)->x && 0.0f == (src)->y && 0.0f == (src)->z) {                    \
        pLog.p->err(0, 0, "VECNormalize:[%s/%d]", __FILE__, __LINE__);                  \
        (dst)->x = (dst)->y = (dst)->z = 0.0f;                                          \
    } else                                                                              \
        PSVECNormalize(src, dst)

// Work `no` of the enemy manager with the range check read through a manager copy (em_set.cpp).
static inline cEm* em27MgrWork(u32 no)
{
    cEmMgr* m = &EmMgr;
    if (no >= m->nArray) {
        return 0;
    }
    return (cEm*) ((u8*) m->pArray + m->size * no);
}

extern "C" void _prolog()
{
    OSReport("em27 prolog Ok\n");
    EmInitFunc = Em27Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em27Init(cEm* em)
{
    new (em) cEm27();
}

void em27DmCk(cEm27* em)
{
    Em27Work* w = EM27_WK(em);
    int wep;
    int dmg;
    // COMPILER-DIFF: #13 (int shape): the EstSet stack zero is a function-scope constant with one
    // use in another block, so sched1 sees the store as a leaf and update_equiv_regs moves the
    // `li` next to it (r0, like the original's rematerialised reload).
    int zero;

    if (em->dmHit == 0) {
        return;
    }
    wep = em->dmWep;
    em->dmHit = 0;
    zero = 0;
    if (wep == 0x14 || wep == 0x16 || wep == 0x17 || wep == 0x2A) {
        return;
    }
    switch (wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 7:
    case 8:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x1A:
    case 0x1B:
    case 0x21:
        dmg = (Rnd() & 1) + 999;
        break;
    case 0x19:
    case 0x1F:
    case 0x20:
        return;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x2C:
    case 0x2D:
    default:
        dmg = 9999;
        break;
    }
    LifeDownSet2(em, dmg, 0, 0);
    if (em->hp > 0) {
        if (em->pos.y > w->waterHeight + 300.0f) {
            EmDmBloodSet2(em, 0x1F, 6, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x1F, 7, 0, 0, 0);
            EstSet((int) em, -1, 0, 0, 0x1F, 8, 0, 0, (u32) em, (void*) zero);
        }
    }
    em->alpha = 1.0f;
    if (em->hp <= 0) {
        EmSetDie(em);
        if (em->pos.y > w->waterHeight) {
            EmRoutineSet(em, 2, 2, 0, 0);
        } else if (Rnd() & 1) {
            EmRoutineSet(em, 2, 0, 0, 0);
        } else {
            EmRoutineSet(em, 2, 1, 0, 0);
        }
    } else {
        // Two identical arms behind a dmWep == 0x21 test: jump2 cross-jumps the whole first arm
        // into the second and only the dead `lbz dmWep; cmpwi 0x21` survives (jump2 runs after
        // flow2). The first arm must be written else-first (`!(a > b)`) so that after its
        // sub-arms are merged it reads `ble E2; b T2`, identical to the second arm's head.
        if (em->dmWep == 0x21) {
            if (!(em->pos.y > w->waterHeight)) {
                EmRoutineSet(em, 2, 0, 0, 0);
            } else {
                EmRoutineSet(em, 2, 2, 0, 0);
            }
        } else {
            if (em->pos.y > w->waterHeight) {
                EmRoutineSet(em, 2, 2, 0, 0);
            } else {
                EmRoutineSet(em, 2, 0, 0, 0);
            }
        }
    }
}

Em27Func Em27_R0_move_tbl[4] = {
    em27_R0_Init,
    em27_R0_Move,
    em27_R0_Damage,
    em27_R0_Die,
};

static Em27Func Em27_R1_move_tbl[6] = {
    em27_R1_Wait,
    em27_R1_Walk,
    em27_R1_Dash,
    em27_R1_Bank,
    em27_R1_Turn180,
    em27_R1_Jump,
};

static Em27Func Em27_R2_move_tbl[3] = {
    em27_R1_Dm_Normal,
    em27_R1_Dm_Big,
    em27_R1_Dm_Air,
};

static Em27Func Em27_R3_move_tbl[1] = {
    em27_R1_Die_Normal,
};

// Parts index remap of the flipped motions (cModel::motFlip).
static u16 em27_flip_tbl[24] = {
    0, 1, 2, 3, 4, 5, 6, 8, 7, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0, 0, 0,
};

void cEm27::move()
{
    Em27Work* w = EM27_WK(this);
    Vec v;
    f32 wh;

    if (pG->room_id == 0x10B && (pG->flags_5010 & 0x00200000) && hp > 0) {
        be_flag &= ~2;
        be_flag &= ~0x20;
        return;
    }
    if (xFC != 0) {
        em27DmCk(this);
    }
    if (w->dashTimer) {
        w->dashTimer--;
    }
    if (w->chaseTimer) {
        w->chaseTimer--;
    }
    w->flags &= ~0xD8;
    if (!(w->flags & 0x100)) {
        if (GetWaterHeight(&pos, &wh)) {
            w->waterHeight = wh - 300.0f;
        }
    }
    if (plDist2 < 250000.0f) {
        w->chaseTimer = Rnd() % 60 + 60;
    }
    if (pG->flags_500C & 0x00800000) {
        w->chaseTimer = Rnd() % 60 + 60;
    }
    if ((u8) pG->flags_51E4 == emsetNo % 0x100) {
        if (Rnd() & 1) {
            w->chaseTimer = Rnd() % 60 + 60;
        }
    }
    if (w->chaseTimer) {
        PSVECSubtract(&pos, &pPL->pos, &v);
#line 379 "D:/Bio4/Prog/em27.cpp"
        VECNormalize(&v, &v);
        PSVECScale(&v, &v, 10000.0f);
        PSVECAdd(&pPL->pos, &v, &w->target);
    } else if (w->retarget) {
        w->retarget = 0;
    } else {
        w->retarget = Rnd() % 120 + 120;
        if (Rnd() & 1) {
            w->target = w->home;
        } else {
            v.x = fRand1_1() * 10000.0f;
            v.y = 0.0f;
            v.z = fRand1_1() * 10000.0f;
            PSMTXMultVec(mat, &v, &w->target);
        }
    }
    Em27_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em27ScaleReset(this);
    partsWorldCalc();
    EmAtCheck(this);
    em27ObaHitCk(this);
    atari.move();
    SatMgr.checkAir(this, 0);
    em27WaterEffSet(this);
}

static void em27_R0_Init(cEm27* em)
{
    Em27Work* w = EM27_WK(em);
    cAtariInfo* at;
    Vec* pos;
    f32 scale;
    f32 wh;
    int zero;

    em->x12F = 0;
    if (em->modelInit(ARC(4), ARC(5)) == 0) {
        pLog->err(0, 0, "em27() ModelInit failed.");
        em->xFC = 0xFF;
        return;
    }
    em->be_flag &= ~0x10;
    em->motFlip = em27_flip_tbl;
    em->hp = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 300.0f, 300.0f, 300.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    scale = fRand0_1() * 0.5f + 1.0f;
    if (em->type == 1) {
        scale = 3.0f;
    }
    at = &em->atari;
    pos = &em->pos;
    em->scale.x = scale;
    em->scale.y = scale;
    em->scale.z = scale;
    at->init(1, 0x2800, 10, 0.0f, 0.0f, 0.0f, 250.0f, 100.0f, 100.0f, 100.0f);
    em->atari.flags &= 0xFDFF;
    em->setStatus(0xB);
    YarareInit(em, 0.0f, 0.0f, -100.0f, 100.0f, 250.0f, 5, 5);
    EspDataLoad((u32) ARC(6), 0x1F, 0);
    w->flags = zero;
    w->dashTimer = Rnd() % 150 + 210;
    w->chaseTimer = zero;
    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    w->spdTarget.x = 0.0f;
    w->spdTarget.y = 0.0f;
    w->spdTarget.z = 0.0f;
    w->retarget = zero;
    w->home = *pos;
    w->waterHeight = 400.0f;
    if (GetWaterHeight(pos, &wh)) {
        w->waterHeight = wh;
    }
    if (em->pos.y > w->waterHeight) {
        em->pos.y = w->waterHeight;
    }
    w->initPos = *pos;
    w->initRot = em->rot;
    w->pCtrl11 = GetCtrlCtrl11();
    w->pCtrl12 = GetCtrlCtrl12();
    em->setStatus(1);
    AtariOff(at, 0xFDFF);
    EmRoutineSet(em, 1, zero, zero, zero);
    em->rot.y = fRand1_1() * PI;
    MotionSetCore(em, MOTION(em), ARC(7), 0, 0, 1, 0);
    MotionMoveF(em, 0);
    em27_R0_Move(em);
}

static void em27_R0_Move(cEm27* em)
{
    Em27_R1_move_tbl[em->xFD](em);
}

static void em27_R1_Wait(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(7), 0, 0, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xE), 0, 0, 5, 0);
        }
        w->timer = (Rnd() & 0x3C) + 60;
        w->spdTarget.x = 0.0f;
        w->spdTarget.y = 0.0f;
        w->spdTarget.z = 0.0f;
        w->spdTarget.z = fRand1_1() * 10.0f + 10.0f;
        em->xFE++;
    case 1:
        em27SetSPeed(em, 0.1f);
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else if ((Rnd() & 7) == 0 && em27JumpCk(em)) {
            EmRoutineSet(em, 1, 5, 0, 0);
        } else if (w->dashTimer == 0) {
            EmRoutineSet(em, 1, 2, 0, 0);
        } else if (Rnd() & 3) {
            EmRoutineSet(em, 1, 1, 0, 0);
        } else {
            EmRoutineSet(em, 1, 4, 0, 0);
        }
        break;
    }
}

static void em27_R1_Walk(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    switch (em->xFE) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(8), 0, 5, 5, 0);
            w->spdTarget.z = fRand1_1() * 25.0f + 30.0f;
        } else {
            MotionSetCore(em, MOTION(em), ARC(9), 0, 5, 5, 0);
            w->spdTarget.z = fRand1_1() * 25.0f + 60.0f;
        }
        w->spdTarget.x = 0.0f;
        w->spdTarget.y = fRand1_1() * 10.0f;
        w->timer = (Rnd() & 0x3C) + 60;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->target, em->rot.y, PI / 32.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em27SetSPeed(em, 0.1f);
        if (MotionMoveF(em, 0) && (Rnd() & 3) == 0) {
            if (w->x24 > 9000000.0f && (Rnd() & 3) == 0 && em27JumpCk(em)) {
                EmRoutineSet(em, 1, 5, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        } else if (w->dashTimer == 0) {
            EmRoutineSet(em, 1, 2, 0, 0);
        } else if (w->timer) {
            w->timer--;
        } else {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
}

static void em27_R1_Dash(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 5, 5, 0);
        w->timer = Rnd() % 6 + 7;
        w->dashTimer = Rnd() % 60 + 210;
        w->spdTarget.x = 0.0f;
        w->spdTarget.y = fRand1_1() * 15.0f;
        w->spdTarget.z = 130.0f;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->target, em->rot.y, PI / 16.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em27SetSPeed(em, 0.5f);
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            EmRoutineSet(em, 1, 1, 0, 0);
            w->dashTimer = Rnd() % 150 + 210;
        }
        break;
    }
}

static void em27_R1_Bank(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    switch (em->xFE) {
    case 0: {
        Vec v;

        MotionSetCore(em, MOTION(em), ARC(0xD), 0, 5, 5, 0);
        w->spdTarget.z = fRand1_1() * 25.0f + 100.0f;
        w->spdTarget.x = 0.0f;
        w->spdTarget.y = fRand1_1() * 10.0f;
        v = em->pos;
        v.y = w->waterHeight + 300.0f;
        EstSet(0, -1, &v, &em->rot, 0x1F, 0xF, 0, 0, 0, 0);
        SndCall(8, 0, &em->pos, em->id, 0, em);
        em->xFE++;
    }
    case 1:
        em->rot.y += Muku(&em->pos, &w->target, em->rot.y, PI / 32.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em27SetSPeed(em, 0.1f);
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
}

static void em27_R1_Turn180(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    switch (em->xFE) {
    case 0: {
        void* m;
        int flag;

        if (Rnd() & 1) {
            m = ARC(0xB);
        } else {
            m = ARC(0xC);
        }
        if (Rnd() & 1) {
            flag = 1;
        } else {
            flag = 0x41;
        }
        MotionSetCore(em, MOTION(em), m, 0, 5, flag, 0);
        em->xFE++;
    }
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->dashTimer == 0) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

static void em27_R1_Jump(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    switch (em->xFE) {
    case 0: {
        void* m0;
        void* m1;
        int flag;

        switch (Rnd() & 1) {
        case 0:
        default:
            m0 = ARC(0xF);
            m1 = ARC(0x1D);
            break;
        case 1:
            m0 = ARC(0x10);
            m1 = ARC(0x1E);
            break;
        }
        if (Rnd() & 1) {
            flag = 0;
        } else {
            flag = 0x40;
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 5, flag, 0);
        em->xFE++;
    }
    case 1:
        if (em27MotionMoveScale(em)) {
            if (w->dashTimer == 0) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

static void em27_R0_Damage(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    w->flags |= 8;
    Em27_R2_move_tbl[em->xFD](em);
}

static void em27_R1_Dm_Normal(cEm27* em)
{
    switch (em->xFE) {
    case 0: {
        int flag;

        em->rot.y += Muku(&em->pos, &em->x328, em->rot.y, PI);
        if (Rnd() & 1) {
            flag = 0;
            em->xFF = flag;
        } else {
            em->xFF = 1;
            flag = 0x40;
        }
        MotionSetCore(em, MOTION(em), ARC(0x11), (int) ARC(0x1F), 3, flag, 0);
        em->xFE++;
    }
    case 1:
        if (em27MotionMoveScale(em)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        if ((em->seFlags28B & 4) && em->hp <= 0) {
            em->xFC = 3;
            em->xFD = 0;
            em->xFE = 0;
        }
        break;
    }
}

static void em27_R1_Dm_Big(cEm27* em)
{
    switch (em->xFE) {
    case 0: {
        int flag;

        em->rot.y += Muku(&em->pos, &em->x328, em->rot.y, PI);
        em->xFF = Rnd() & 1;
        if (em->xFF) {
            flag = 0x40;
        } else {
            flag = 0;
        }
        MotionSetCore(em, MOTION(em), ARC(0x12), 0, 3, flag, 0);
        em->xFE++;
    }
    case 1:
        if (em27MotionMoveScale(em)) {
            em->xFC = 3;
            em->xFD = 0;
            em->xFE = 0;
        }
        break;
    }
}

static void em27_R1_Dm_Air(cEm27* em)
{
    Em27Work* w = EM27_WK(em);
    Mtx m;
    Vec v;
    int flag;

    switch (em->xFE) {
    case 0:
        em->rot.y += Muku(&em->pos, &em->x328, em->rot.y, PI);
        em->xFF = Rnd() & 1;
        if (em->xFF) {
            flag = 0x41;
        } else {
            flag = 1;
        }
        MotionSetCore(em, MOTION(em), ARC(0x13), 0, 3, flag, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        if (em->xFF) {
            flag = 0x41;
        } else {
            flag = 1;
        }
        MotionSetCore(em, MOTION(em), ARC(0x16), 0, 3, flag, 0);
        w->spd.x = 0.0f;
        w->spd.y = -100.0f;
        w->spd.z = 0.0f;
        em->xFE++;
    case 3:
        w->spd.y -= 5.0f;
        PSMTXRotRad(m, 'y', em->rot.y);
        PSMTXMultVecSR(m, &w->spd, &v);
        PSVECAdd(&em->pos, &v, &em->pos);
        MotionMoveF(em, 0);
        if (em->pos.y < w->waterHeight) {
            em->xFE++;
        }
        break;
    case 4:
        em->xFE++;
    case 5:
        w->spd.y += 20.0f;
        PSMTXRotRad(m, 'y', em->rot.y);
        PSMTXMultVecSR(m, &w->spd, &v);
        PSVECAdd(&em->pos, &v, &em->pos);
        if (em->pos.y < w->waterHeight - 300.0f) {
            em->pos.y = w->waterHeight - 300.0f;
            w->spd.y = 0.0f;
        }
        MotionMoveF(em, 0);
        if (w->spd.y > 0.0f) {
            if (em->hp <= 0) {
                em->xFC = 3;
                em->xFD = 0;
                em->xFE = 0;
            } else {
                em->xFE++;
            }
        }
        break;
    case 6:
        if (em->xFF) {
            flag = 0x41;
        } else {
            flag = 1;
        }
        MotionSetCore(em, MOTION(em), ARC(0x14), 0, 3, flag, 0);
        em->xFE++;
    case 7:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
}

static void em27_R0_Die(cEm27* em)
{
    Em27Work* w = EM27_WK(em);

    w->flags |= 8;
    Em27_R3_move_tbl[em->xFD](em);
}

static void em27_R1_Die_Normal(cEm27* em)
{
    Em27Work* w = EM27_WK(em);
    int flag;
    int no;

    w->flags |= 0x80;
    switch (em->xFE) {
    case 0:
        if (em->xFF) {
            flag = 0x41;
        } else {
            flag = 1;
        }
        w->dieVariant = Rnd() & 1;
        if (w->dieVariant) {
            MotionSetCore(em, MOTION(em), ARC(0x15), 0, 10, flag, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x1C), 0, 15, flag, 0);
        }
        Ctrl12CntAdd(w->pCtrl12, 1, 1);
        em->atari.flags &= 0xFCFF;
        w->timer = 120;
        w->spd.y = fRand1_1() * PI;
        w->upDown = 0;
        switch (em->type) {
        case 0:
        default:
            no = SceAtCreateItemAt(&em->pos, 0x95, 0, -1, -1, 0, -1);
            break;
        case 1:
            no = SceAtCreateItemAt(&em->pos, 0x97, 0, -1, -1, 0, -1);
            break;
        }
        SceAtSetItemModel(no, em);
        em->xFE++;
    case 1:
        w->spd.y += PI / 32.0f;
        w->spd.y = LIMIT_ANGLE(w->spd.y);
        if (w->upDown) {
            em->pos.y += sinf(w->spd.y) * 3.0f;
        } else {
            em->pos.y += 5.0f;
            if (em->pos.y > w->waterHeight + 150.0f) {
                em->pos.y = w->waterHeight + 150.0f;
                w->upDown = 1;
            }
        }
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->xFE++;
        }
        break;
    case 2:
        w->timer = Rnd() % 60 + 60;
        w->upDown = 0;
        w->spd.y = fRand1_1() * PI;
        em->xFE++;
    case 3:
        w->spd.y += PI / 32.0f;
        w->spd.y = LIMIT_ANGLE(w->spd.y);
        em->pos.y = SINF(w->spd.y) * 3.0f + em->pos.y;
        if (em->pos.y > w->waterHeight + 150.0f) {
            em->pos.y = em->pos.y * 0.9f + (w->waterHeight + 150.0f) * 0.1f;
        }
        if (w->upDown) {
            w->upDown--;
        } else {
            cModel* p = em->getPartsPtr(4);
            Vec v;

            flag = 1;
            v.x = p->worldPos.x;
            v.y = w->waterHeight + 300.0f;
            v.z = p->worldPos.z;
            EstSet(0, -1, &v, &em->rot, 0x1F, 5, 0, 0, 0, 0);
            w->upDown = Rnd() % 60 + 5;
            if (em->xFF) {
                flag = 0x41;
            }
            if (w->dieVariant) {
                MotionSetCore(em, MOTION(em), ARC(0x1B), 0, 10, flag, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x1C), 0, 10, flag, 0);
            }
        }
        MotionMoveF(em, 0);
        break;
    }
}

void em27SetSPeed(cEm27* em, f32 rate)
{
    Em27Work* w = EM27_WK(em);
    Mtx m;
    Vec v;

    w->spd.x = w->spd.x * (1.0f - rate) + w->spdTarget.x * rate;
    w->spd.y = w->spd.y * (1.0f - rate) + w->spdTarget.y * rate;
    w->spd.z = w->spd.z * (1.0f - rate) + w->spdTarget.z * rate;
    PSMTXRotRad(m, 'y', em->rot.y);
    PSMTXMultVecSR(m, &w->spd, &v);
    PSVECAdd(&em->pos, &v, &em->pos);
    if (em->pos.y < w->waterHeight - 300.0f) {
        em->pos.y = w->waterHeight - 300.0f;
    }
    if (em->pos.y > w->waterHeight) {
        em->pos.y = w->waterHeight;
    }
}

void em27ScaleReset(cEm27* em)
{
    Em27Work* w = EM27_WK(em);
    cModel* p;

    if (w->flags & 0x10) {
        return;
    }
    p = em->getPartsPtr(1);
    p->scale.x = p->scale.x * 0.9f + 0.1f;
    p->scale.y = p->scale.y * 0.9f + 0.1f;
    p->scale.z = p->scale.z * 0.9f + 0.1f;
    p = em->getPartsPtr(3);
    p->scale.x = p->scale.x * 0.9f + 0.1f;
    p->scale.y = p->scale.y * 0.9f + 0.1f;
    p->scale.z = p->scale.z * 0.9f + 0.1f;
}

void em27ObaHitCk(cEm27* em)
{
    Em27Work* w = EM27_WK(em);
    Vec d;
    f32 dist;
    f32 r;
    u32 i;

    if (w->flags & 0x40) {
        return;
    }
    if (!(em->be_flag & 2)) {
        return;
    }
    if (em->hp <= 0) {
        return;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = em27MgrWork(i);

        {
            int dead = !(e->be_flag & 1);

            if (dead) {
                continue;
            }
        }
        if (!(e->be_flag & 0x20)) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e->pParts == 0) {
            continue;
        }
        PSVECSubtract(&em->pos, &e->pos, &d);
        dist = d.x * d.x + d.y * d.y + d.z * d.z;
        if (dist > 40000.0f) {
            continue;
        }
        if (dist <= 0.0f) {
            continue;
        }
        dist = SQRTF(dist) * 0.9f + 20.0f;
#line 1315 "D:/Bio4/Prog/em27.cpp"
        VECNormalizeP(&d, &d);
        PSVECScale(&d, &d, dist);
        PSVECAdd(&e->pos, &d, &em->pos);
    }
    PSVECSubtract(&em->pos, &pPL->pos, &d);
    d.y = 0.0f;
    dist = d.x * d.x + d.z * d.z;
    r = pPL->atari.rectZ + 100.0f;
    if (dist > r * r) {
        return;
    }
    if (dist <= 0.0f) {
        return;
    }
#line 1327 "D:/Bio4/Prog/em27.cpp"
    VECNormalize(&d, &d);
    PSVECScale(&d, &d, r);
    FSet(em->pos.x, pPL->pos.x + d.x);
    FSet(em->pos.z, pPL->pos.z + d.z);
    PartsWorldPosCalc(em);
}

int em27MotionMoveScale(cEm27* em)
{
    Vec spd;
    Vec rot;
    Vec inv;
    int ret;
    cModel* p;

    inv.x = 1.0f / em->scale.x;
    inv.y = 1.0f / em->scale.y;
    inv.z = 1.0f / em->scale.z;
    MotionGetSpeed(em, MOTION(em), 0, &spd, &rot);
    spd.x *= inv.x;
    spd.y *= inv.y;
    spd.z *= inv.z;
    MotionAddSpeed(em, MOTION(em), &spd, &rot);
    ret = MotionMoveF(em, 0);
    p = em->getPartsPtr(0);
    p->pos.y *= inv.y;
    p->worldMat[1][3] *= inv.y;
    return ret;
}

void em27WaterEffSet(cEm27* em)
{
    Em27Work* w = EM27_WK(em);
    Vec v;
    f32 h;
    cModel* p;

    if (w->flags & 0x80) {
        return;
    }
    if (!(em->be_flag & 2)) {
        return;
    }
    h = w->waterHeight + 300.0f;
    v = em->pos;
    v.y = h;
    p = em->getPartsPtr(0);
    if (p->worldPos.y > h && p->oldWorldPos.y < h) {
        AddWaterPower(&em->pos, -0.5f);
        EstSet(0, -1, &v, &em->rot, 0x1F, 0, 0, 0, 0, 0);
        EstSet((int) em, -1, 0, 0, 0x1F, 4, 0, 0, (u32) em, 0);
        SndCall(8, (Rnd() & 3) | 4, &em->pos, em->id, 0, em);
    }
    if (p->worldPos.y < h && p->oldWorldPos.y > h) {
        AddWaterPower(&em->pos, 0.5f);
        EstSet(0, -1, &v, &em->rot, 0x1F, 1, 0, 0, 0, 0);
        SndCall(8, (Rnd() & 1) + 7, &em->pos, em->id, 0, em);
    }
}

int em27JumpCk(cEm27* em)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if (e->isAlive() && e->id == 0xF) {
            if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z)
                < 4000000.0f) {
                return 0;
            }
        }
    }
    return 1;
}

void cEm27::setWaterHeight(f32 h)
{
    Em27Work* w = EM27_WK(this);

    w->waterHeight = h;
    w->flags |= 0x100;
}
