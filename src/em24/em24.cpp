// em24 module (D:/Bio4/Prog/em24.cpp): a small enemy that either waits in a box and jumps out at the
// player (routine 1/0, work flag bit2), or wanders freely (1/2) and coils up when the player comes
// near (1/1, 1/3). Dies to any damage volume of kind 1/4/5/7 and to most weapons; a random weapon
// item drops at death (em24_R0_Die).

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "em24.h"
#include "emhit.h"
#include "emwep.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "motion.h"
#include "quake.h"
#include "pad.h"
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

typedef void (*Em24Func)(cEm24*);

static void em24_R0_Init(cEm24* em);
static void em24_R0_Move(cEm24* em);
static void em24_R1_BoxWait(cEm24* em);
static void em24_R1_CoilWait(cEm24* em);
static void em24_R1_Free(cEm24* em);
static void em24_R1_Coil(cEm24* em);
static void em24_R0_Damage(cEm24* em);
static void em24_R0_Die(cEm24* em);

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

// Struct-member view of the player pointer: a load through it is not hoisted above the preceding
// stores through the work pointer (cam_ctrl.cpp PlayerPtr).
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

// Dead flag test (cDmgInfo upper 16 bits): an inline returning 0/1 gives the `li 1; andis.; bne; li 0` chain.
static inline int em24DeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

extern "C" void _prolog()
{
    OSReport("em24 prolog Ok\n");
    EmInitFunc = Em24Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em24Init(cEm* em)
{
    new (em) cEm24();
}

void em24DmCk(cEm24* em)
{
    Em24Work* w = EM24_WK(em);
    int wep;

    if (em->hp > 0 && !em24DeadCk(em)) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 4:
        case 5:
        case 7:
            goto die;
        }
    }
    if (em->dmHit) {
        wep = em->dmWep;
        em->dmHit = 0;
        if (wep == 0x14 || wep == 0x16 || wep == 0x17 || wep == 0x2A || wep == 0xE) {
            return;
        }
        SndCall(8, 8, &em->pos, em->id, 0, em);
        if (w->flags & 0x20) {
            EmDmBloodSet2(em, 0x1C, 2, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x1C, 0, 0, 0, 0);
        }
    die:
        em->hp = 0;
        em->dmType = 0x80;
        EmRoutineSet(em, 3, 0, 0, 0);
    }
}

Em24Func Em24_R0_move_tbl[4] = {
    em24_R0_Init,
    em24_R0_Move,
    em24_R0_Damage,
    em24_R0_Die,
};

static Em24Func Em24_R1_move_tbl[4] = {
    em24_R1_BoxWait,
    em24_R1_CoilWait,
    em24_R1_Free,
    em24_R1_Coil,
};

// Jump attack (em24AtkCk): range, type, damage, ...
static EmAtkInfo em24_atk_tbl[1] = {
    { 500.0f, 8, 100, 4, 0xA, 0 },
};

void cEm24::move()
{
    Em24Work* w = EM24_WK(this);
    f32 spd;

    em24DmCk(this);
    w->flags &= ~4;
    Em24_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em24SlopeMove(this);
    partsWorldCalc();
    w->flags &= ~0x20;
    if (w->flags & 0x10) {
        return;
    }
    spd = SQRTF((oldPos.x - pos.x) * (oldPos.x - pos.x) + (oldPos.z - pos.z) * (oldPos.z - pos.z));
    EmAtCheck(this);
    atari.move();
    if (hp > 0) {
        if (w->flags & 4) {
            SatMgr.checkAir(this, 0);
        } else {
            f32 wh;

            pos.y = SatMgr.getFloor(&pos, 600.0f, 100000.0f, 0, 0);
            if (GetWaterHeight(&pos, &wh)) {
                if (pos.y < wh) {
                    w->flags |= 0x20;
                    pos.y = wh;
                    if (hp > 0) {
                        if (w->splashTimer) {
                            w->splashTimer--;
                        } else {
                            w->splashTimer = 2;
                            EstSet((int) this, -1, 0, 0, 0x1C, 4, 0, 0, (u32) this, 0);
                        }
                    }
                }
            }
            SatMgr.checkAir(this, 0);
        }
    }
    if (SQRTF((pos.x - oldPos.x) * (pos.x - oldPos.x) + (pos.z - oldPos.z) * (pos.z - oldPos.z)) < spd * 0.5f) {
        w->stuckCnt++;
    } else {
        w->stuckCnt = 0;
    }
}

static void em24_R0_Init(cEm24* em)
{
    Em24Work* w = EM24_WK(em);
    cAtariInfo* at;
    f32 scale;
    int zero;
    int two;

    if (em->modelInit(ARC(5), ARC(6)) == 0) {
        pLog->err(0, 0, "em24() ModelInit failed.");
        em->xFC = 0xFF;
        return;
    }
    scale = (f32) (int) Rnd() * 0.15f * (1.0f / 256.0f) + 1.1f;
    em->scale.x = scale;
    em->scale.y = scale;
    em->scale.z = scale;
    zero = 0;
    two = 2;
    EspDataLoad((u32) ARC(4), 0x1C, 0);
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 1000.0f, 1000.0f, 1000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    at = &em->atari;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    atariInitF(at, 0.0f, -50.0f, 0.0f, 350.0f, 150.0f, 150.0f, 100.0f, 1, 0x2000, 10);   // COMPILER-DIFF: #1
    AtariOff(at, 0xFDFF);
    em->setStatus(1);
    em->be_flag &= ~0x10;
    em->setStatus(0xB);
    YarareInit(em, 0.0f, 0.0f, 0.0f, 50.0f, 50.0f, 1, 5);
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 50.0f, 50.0f, 3, 5);
    YarareAdd(em, &w->hit[1], 0.0f, 0.0f, 0.0f, 50.0f, 50.0f, 6, 5);
    YarareAdd(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 50.0f, 50.0f, 8, 5);
    YarareAdd(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 50.0f, 50.0f, 0xA, 5);
    YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 50.0f, 50.0f, 0xC, 5);
    w->flags = zero;
    w->stuckCnt = zero;
    w->splashTimer = two;
    w->slopeRot.x = 0.0f;
    w->slopeRot.y = 0.0f;
    w->slopeRot.z = 0.0f;
    switch (em->x38D) {
    default:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        EmRoutineSet(em, 1, two, zero, zero);
        break;
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        EmRoutineSet(em, 1, 0, 0, 0);
        break;
    }
    em24_R0_Move(em);
}

static void em24_R0_Move(cEm24* em)
{
    Em24_R1_move_tbl[em->xFD](em);
}

static void em24_R1_BoxWait(cEm24* em)
{
    Em24Work* w = EM24_WK(em);

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x17), 0, 0, 5, 0);
        AtariOff(&em->atari, 0xFCFF);
        w->timer = 45;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, PI);
        MotionMoveF(em, 0);
        if (!(em->flags_3C8 & 1)) {
            em->dmType = 2;
            break;
        }
        if (w->timer) {
            w->timer--;
        } else {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x11), (int) ARC(0x18), 3, 1, 0);
        w->spd.x = 0.0f;
        w->spd.y = -100.0f;
        w->spd.z = 200.0f;
        w->motEnd = 0;
        w->atkHit = 0;
        SndCall(8, 4, &em->pos, em->id, 0, em);
        EstSet((int) em, -1, 0, 0, 0x1C, 5, 0, 0, (u32) em, 0);
        em->xFE++;
    case 3: {
        int two = 2;

        em->dmType = two;
        if (em->seFlags28B & 1) {
            cModel* p = em->getPartsPtr(5);

            em24AtkCk(em, &p->worldPos, &p->oldWorldPos, 0);
        }
        if (!(em->seFlags28B & 0x80)) {
            cAtariInfo* at = &em->atari;
            Vec v;
            f32 fl;

            at->flags |= 0x100;
            PSMTXMultVecSR(em->mat, &w->spd, &v);
            PSVECAdd(&em->pos, &v, &em->pos);
            w->spd.y -= 20.0f;
            fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
            if (em->pos.y < fl) {
                em->pos.y = fl;
                w->spd.y = 0.0f;
                if (MotionMoveF(em, 0)) {
                    w->motEnd = 1;
                }
                if (w->motEnd) {
                    at->flags |= 0x100;
                    EmRoutineSet(em, 1, two, 0, 0);
                    break;
                }
            }
        }
        if (MotionMoveF(em, 0)) {
            w->motEnd = 1;
        }
        break;
    }
    }
}

static void em24_R1_CoilWait(cEm24* em)
{
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 0, 5, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->plDist2 < 9000000.0f) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x12), 0, 0, 1, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
}

static void em24_R1_Free(cEm24* em)
{
    Em24Work* w = EM24_WK(em);

    switch (em->xFE) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(7), 0, 3, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(8), 0, 3, 5, 0);
        }
        w->motEnd = Rnd() % 3;
        w->targetAng = GetXZAngle(&pPLS->pos, &em->pos);
        w->targetAng += fRand1_1() * (PI / 2.0f);
        w->targetAng = LIMIT_ANGLE(w->targetAng);
        w->timer = Rnd() % 3 + 3;
        w->turnTimer = Rnd() % 30 + 30;
        w->stuckCnt = 0;
        em->xFE++;
    case 1:
        if (w->turnTimer) {
            w->turnTimer--;
        } else {
            w->turnTimer = Rnd() % 15 + 15;
            w->targetAng += fRand1_1() * (PI / 4.0f);
            w->targetAng = LIMIT_ANGLE(w->targetAng);
        }
        em->rot.y += Muku2(em->rot.y, w->targetAng, PI / 128.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            if (w->stuckCnt > 1) {
                em->xFE++;
            }
        }
        break;
    case 2:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(9), 0, 3, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xA), 0, 3, 1, 0);
        }
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    }
}

static void em24_R1_Coil(cEm24* em)
{
    Em24Work* w = EM24_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE), 0, 3, 5, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 3, 5, 0);
        w->timer = Rnd() % 90 + 90;
        em->xFE++;
    case 3:
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, PI / 64.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x12), 0, 3, 5, 0);
        em->xFE++;
    case 5:
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, PI / 64.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
}

static void em24_R0_Damage(cEm24* em)
{
    EmRoutineSet(em, 1, 2, 0, 0);
}

static void em24_R0_Die(cEm24* em)
{
    Em24Work* w = EM24_WK(em);

    switch (em->xFD) {
    case 0:
        AtariOff(&em->atari, 0xFEFF);
        if (w->flags & 0x20) {
            em->xFF = 1;
        } else {
            em->xFF = 0;
        }
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(0x15), 0, 3, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x1C, 7, 0, 0, (u32) em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x13), 0, 3, 1, 0);
        }
        EmSetDie(em);
        em->xFD++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFD++;
        }
        break;
    case 2: {
        Vec pos;
        void* bin;
        void* tpl;
        int id;
        cEmWep* wep;

        w->timer = 60;
        if (em->xFF) {
            EstSet((int) em, -1, 0, 0, 0x1C, 3, 0, 0, (u32) em, 0);
        } else {
            EstSet((int) em, -1, 0, 0, 0x1C, 1, 0, 0, (u32) em, 0);
        }
        pos = em->pos;
        switch (Rnd() & 0xF) {
        default:
            id = 8;
            bin = ARC(0x19);
            tpl = ARC(0x1A);
            break;
        case 0xC:
        case 0xD:
        case 0xE:
            id = 9;
            bin = ARC(0x19);
            tpl = ARC(0x1B);
            break;
        case 0xF:
            id = 0xA;
            bin = ARC(0x19);
            tpl = ARC(0x1C);
            break;
        }
        wep = SetWeapon(bin, tpl, &pos, &em->rot, 1);
        if (wep) {
            int no = SceAtCreateItemAt(&pos, id, 0, -1, -1, 0, -1);

            SceAtSetItemModel(no, wep);
            wep->setAtNo(no);
            wep->setYarare(0, 100.0f, 10.0f);
            wep->setEffDamage(0x1C, 0x1F);
            wep->setSeDamage(8, 0xC, em->id);
        }
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        em->xFD++;
    }
    case 3:
        em->pos.y -= 2.0f;
        if (w->timer) {
            em->alpha -= 0.05f;
            if (em->alpha < 0.0f) {
                em->alpha = 0.0f;
                em->be_flag &= ~2;
                em->be_flag |= 0x4000;
                em->xFD++;
                w->flags |= 0x10;
                break;
            }
        }
        MotionMoveF(em, 0);
        break;
    case 4:
        break;
    }
}

int em24AtkCk(cEm24* em, Vec* a, Vec* b, int no)
{
    Em24Work* w = EM24_WK(em);

    if (w->atkHit) {
        return 0;
    }
    {
        int hit = EmAtkHitCk(&em24_atk_tbl[no], a, b, 0);

        if (hit) {
            w->atkHit = 1;
            if (hit & 1) {
                EmPlBloodSet2(em, a, 1, 0x1C, 6);
                QuakeExec(0, 0, 5, 22.0f, 2);
                VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
                return 1;
            }
        }
    }
    return 0;
}

void em24SlopeMove(cEm24* em)
{
    Em24Work* w = EM24_WK(em);
    Vec a;
    Vec b;
    Mtx m;
    f32 wh;
    f32 fa;
    f32 fb;
    f32 len;

    if (em->hp <= 0) {
        return;
    }
    a.x = 0.0f;
    a.y = 1000.0f;
    a.z = 200.0f;
    b.x = 0.0f;
    b.y = 1000.0f;
    b.z = -200.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    fa = SatMgr.getFloor(&a, 600.0f, 100000.0f, 0, 0);
    if (GetWaterHeight(&a, &wh)) {
        if (fa < wh) {
            fa = wh;
        }
    }
    fb = SatMgr.getFloor(&b, 600.0f, 100000.0f, 0, 0);
    if (GetWaterHeight(&b, &wh)) {
        if (fb < wh) {
            fb = wh;
        }
    }
    if (fa == -100000.0f) {
        fa = em->pos.y;
    }
    if (fb == -100000.0f) {
        fb = em->pos.y;
    }
    fa -= fb;
    if (fa > 400.0f) {
        fa = 400.0f;
    }
    if (fa < -400.0f) {
        fa = -400.0f;
    }
    len = SQRTF((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
    w->slopeRot.x = w->slopeRot.x * 0.95f + -atan2f(fa, len) * 0.05f;
    RotMatrix(m, &w->slopeRot);
    PSMTXConcat(em->mat, m, em->mat);
    TransMatrix(em->mat, &em->pos);
}
