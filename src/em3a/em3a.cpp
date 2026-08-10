// em3a module (D:/Bio4/Prog/em3a.cpp): the helicopter. Types 0/1 patrol the room's EMI route
// points (em3a_R1_Patrol), find the player (em3aFindPLCk), chase and shoot him with the gun
// (em3aGunHitCk) or a missile (em3aRocketFire); type 2 is the hovering variant that hides,
// appears and circles the player (the B_ routines) until its nearCnt runs out.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em3a.h"
#include "em10.h"
#include "emhit.h"
#include "embarrel.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "route_ck.h"
#include "quake.h"
#include "pad.h"
#include "pl_wep.h"
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
// em_set.h declares the empty form; this unit passes the dying enemy (the original prototype
// took it; em_set.cpp ignores its arguments).
void EmSetDieCntE(cEm* em) asm("EmSetDieCnt");

typedef void (*Em3aFunc)(cEm3a*);

static void em3a_R0_Init(cEm3a* em);
static void em3a_R0_Move(cEm3a* em);
static void em3a_R1_Patrol(cEm3a* em);
static void em3a_R1_Atk(cEm3a* em);
static void em3a_R1_Chase(cEm3a* em);
static void em3a_R1_Fix(cEm3a* em);
static void em3a_R1_FixAtk(cEm3a* em);
static void em3a_R1_Die(cEm3a* em);
static void em3a_R1_B_HideWait(cEm3a* em);
static void em3a_R1_B_Hide(cEm3a* em);
static void em3a_R1_B_Appear(cEm3a* em);
static void em3a_R1_B_Wait(cEm3a* em);
static void em3a_R1_B_Move(cEm3a* em);
static void em3a_R1_B_Die(cEm3a* em);
static void em3a_R1_B_AppearDie(cEm3a* em);
static void em3a_R1_B_Bomb(cEm3a* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)

// Collision flag bits set / cleared through the info's address (`addi rX, em, 0x2b4; lhz 0x1a(rX)`).
static inline void AtariOff(cAtariInfo* at, u16 mask) { at->flags &= mask; }
static inline void AtariOn(cAtariInfo* at, u16 bits) { at->flags |= bits; }

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

// Struct-member view of the player pointer: a load through it is not hoisted above the preceding
// stores of a stack copy (cam_ctrl.cpp PlayerPtr).
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

// Dead flag test (cDmgInfo upper 16 bits): an inline returning 0/1 gives the `li 1; andis.; bne; li 0` chain.
static inline int em3aDeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

// Stores through a scalar reference: the following pG load is reloaded (em3a_R1_Atk's timer
// table, em3aPatrolInit's route pointer).
static inline void IntSet(int& x, int v) { x = v; }
static inline void EmiSet(EmiEntry*& p, EmiEntry* v) { p = v; }
// Reference read of pG: the load depends on the preceding `w->pRoute = 0` store (a MEM with neither
// the struct nor the scalar flag), which ranks the store above the `lis pG@ha` in em3aPatrolInit.
static inline GlobalWork* GRef(GlobalWork*& p) { return p; }

// Hover: keep the height between fl + 1800 and fl + 2000, apply and damp the speed, vibrate.
static inline void em3aHoverMove(cEm3a* em, Em3aWork* w, f32 fl)
{
    if (em->pos.y < fl + 1800.0f) {
        f32 y = w->spd.y + 10.0f;
        f32 max = 20.0f;

        w->spd.y = y;
        if (y > max) {
            w->spd.y = max;
        }
    }
    if (em->pos.y > fl + 2000.0f) {
        f32 y = w->spd.y - 10.0f;
        f32 min = -20.0f;

        w->spd.y = y;
        if (y < min) {
            w->spd.y = min;
        }
    }
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    PSVECScale(&w->spd, &w->spd, 0.9f);
    em3aVibMove(em);
}

// The same without the height control (R1_FixAtk).
static inline void em3aFixMove(cEm3a* em, Em3aWork* w)
{
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    PSVECScale(&w->spd, &w->spd, 0.9f);
    em3aVibMove(em);
}

// Turn towards the player by at most `lim`.
static inline void em3aTurnToPL(cEm3a* em, f32 lim)
{
    em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, lim);
    em->rot.y = LIMIT_ANGLE(em->rot.y);
}

// Attack found: drop the search effects, start the alert effect and sound.
static inline void em3aFoundSet(cEm3a* em, Em3aWork* w, int est, int parts)
{
    EffectEspDelete(1, w->espKind, (u32) em, 0);
    EffectEspgenDelete(1, w->espKind, (int) em);
    EffectEfmDelete(1, w->espKind, (int) em);
    EstSet((int) em, -1, 0, 0, 2, est, 1, w->espKind, (u32) em, 0);
    SndCall(8, 0xA, &em->getPartsPtr(parts)->worldPos, em->id, 0, em);
}

extern "C" void _prolog()
{
    OSReport("em3a prolog Ok\n");
    EmInitFunc = Em3aInit;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void cEm3a::setNoSuspend(int on)
{
    if (on) {
        be_flag |= 0x800;
    } else {
        be_flag &= ~0x800;
    }
}

void Em3aInit(cEm* em)
{
    new (em) cEm3a();
}

void em3aDmCk(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    EmHitInfo* hit;
    int one;

    if (em->dmHit == 0) {
        return;
    }
    em->dmHit = 0;
    one = 1;
    em->dmType = one;
    if (em->dmWep == 0x10) {
        em->dmType = 0x11;
    }
    hit = em->dmg.part;
    if (em->type == 2) {
        EmDmBloodSet2(em, 2, 1, 0, 0, 0);
        if (w->flags & 8) {
            switch (em->dmWep) {
            case 0x13:
            case 0x29:
            case 0x2D:
                if (hit->rad > 1000000.0f) {
                    return;
                }
                break;
            }
            em->hp = 0;
            EmRoutineSet(em, 1, 0xC, 0, 0);
            return;
        }
        if (hit->partsNo == 0xB) {
            em->hp = 0;
            EmRoutineSet(em, one, 0xB, 0, 0);
            return;
        }
    }
    LifeDownSet(em, em3aSetDmVal(em), 0);
    EmDmBloodSet2(em, 2, 1, 0, 0, 0);
    if (em->type != 2) {
        if (!(w->flags & 4)) {
            w->flags |= 4;
            EstSet((int) em, -1, 0, 0, 2, 2, 1, w->espKind, (u32) em, 0);
        }
    }
    if (em->hp <= 0) {
        if (em->type == 2) {
            EmRoutineSet(em, 1, 0xD, 0, 0);
        } else {
            EmRoutineSet(em, 1, 5, 0, 0);
        }
    }
}

Em3aFunc Em3a_R0_move_tbl[4] = {
    em3a_R0_Init,
    em3a_R0_Move,
    NULL,
    NULL,
};

static Em3aFunc Em3a_R1_move_tbl[14] = {
    em3a_R1_Patrol,
    em3a_R1_Atk,
    em3a_R1_Chase,
    em3a_R1_Fix,
    em3a_R1_FixAtk,
    em3a_R1_Die,
    em3a_R1_B_HideWait,
    em3a_R1_B_Hide,
    em3a_R1_B_Appear,
    em3a_R1_B_Wait,
    em3a_R1_B_Move,
    em3a_R1_B_Die,
    em3a_R1_B_AppearDie,
    em3a_R1_B_Bomb,
};

// Parts index remap of the type 2 model's flipped motions (cModel::motFlip).
static u16 em3a_flip_tbl[120] = {
    0x00, 0x03, 0x04, 0x01, 0x02, 0x07, 0x08, 0x05, 0x06, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
};

// Gun hit damage handed to EmAtkSetDamagePL (em3aGunHitCk): range, type, damage, ...
static EmAtkInfo em3a_atk_info = { 100.0f, 8, 600, 0, 0xA, 0 };

void cEm3a::move()
{
    Em3aWork* w = EM3A_WK(this);

    if (xFC) {
        em3aDmCk(this);
    }
    if (w->atkWait) {
        w->atkWait--;
    }
    if (em3aDeadCk(pPL)) {
        w->atkWait = 90;
    }
    Em3a_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em3aFanMove(this);
    partsWorldCalc();
    em3aGunMove(this);
    if (hp > 0) {
        EmAtCheck(this);
        atari.move();
        switch (type) {
        case 0:
        case 1:
        default: {
            f32 fl = SatMgr.getFloor(&pos, 600.0f, 100000.0f, 0, 0) + 500.0f;

            if (pos.y < fl) {
                pos.y = fl;
            }
            SatMgr.checkAir(this, 0x383810);
            break;
        }
        case 2:
            SatMgr.check(this, 0);
            break;
        }
    }
    em3aEngineSe(this);
}

static void em3a_R0_Init(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    switch (em->type) {
    case 0:
    case 1:
    default:
        if (em->modelInit(ARC(5), ARC(6)) == 0) {
            pLog->err(0, 0, "em3a() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    case 2:
        if (em->modelInit(ARC(0xB), ARC(0xC)) == 0) {
            pLog->err(0, 0, "em3a() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    }
    if (em->type == 2) {
        em->motFlip = em3a_flip_tbl;
    }
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 10000.0f, 10000.0f, 10000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    atariInitF(&em->atari, 0.0f, 0.0f, 0.0f, 400.0f, 350.0f, 350.0f, 300.0f, 1, 0x2000, 10);   // COMPILER-DIFF: #1
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    switch (em->type) {
    case 0:
    case 1:
    default:
        YarareInit(em, 0.0f, 0.0f, -250.0f, 200.0f, 800.0f, 1, 5);
        YarareAddCube(em, &w->hit[0], 0.0f, -50.0f, 0.0f, 250.0f, 100.0f, 250.0f, 5, 1);
        YarareAddCube(em, &w->hit[1], 0.0f, -50.0f, 0.0f, 250.0f, 100.0f, 250.0f, 7, 1);
        YarareAddCube(em, &w->hit[2], 0.0f, -170.0f, 180.0f, 50.0f, 140.0f, 250.0f, 0xA, 1);
        YarareAddCube(em, &w->hit[3], 0.0f, -50.0f, 150.0f, 100.0f, 70.0f, 250.0f, 4, 1);
        break;
    case 2:
        YarareInit(em, 0.0f, 50.0f, 0.0f, 250.0f, 0.0f, 1, 1);
        YarareAddCube(em, &w->hit[0], 0.0f, -50.0f, 0.0f, 150.0f, 200.0f, 150.0f, 0xB, 1);
        break;
    }
    if (em->type == 1) {
        Vec pos;
        Vec rot;
        cModel* p;

        pos.x = 0.0f;
        pos.y = -101.0f;
        pos.z = -143.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pMissile = SetHeliMissile(ARC(9), ARC(0xA), &pos, &rot, 1);
        if (w->pMissile) {
            ((cObjMissile*) w->pMissile)->setParent(em, 9, 0);
        }
        p = em->getPartsPtr(0xA);
        p->scale.x = 0.0f;
        p->scale.y = 0.0f;
        p->scale.z = 0.0f;
    }
    em->lockParts = 2;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(4), 2, 0);
    w->espKind = EspPullCoreKind();
    w->flags = 0;
    w->vibAng.x = fRand1_1() * PI;
    w->vibAng.y = fRand1_1() * PI;
    w->vibAng.z = fRand1_1() * PI;
    w->vibSpd.x = fRand0_1() * 0.06981317f + 0.08726646f;
    w->vibSpd.y = fRand0_1() * 0.02617994f + 0.06981317f;
    w->vibSpd.z = fRand0_1() * 0.02617994f + 0.12217305f;
    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    w->atkWait = 0;
    w->seTimer = 29;
    em3aPatrolInit(em);
    switch (em->type) {
    case 0:
    case 1:
    default:
        em->getPartsPtr(4)->rot.z = 0.61086524f;
        em->getPartsPtr(6)->rot.z = -0.61086524f;
        EstSet((int) em, -1, 0, 0, 2, 0, 1, w->espKind, (u32) em, 0);
        break;
    case 2:
        EstSet((int) em, -1, 0, 0, 2, 0xA, 1, w->espKind, (u32) em, 0);
        break;
    }
    em->setStatus(5);
    switch (em->type) {
    case 0:
    case 1:
    default:
        switch (em->x38D) {
        case 0:
        default:
            EmRoutineSet(em, 1, 0, 0, 0);
            break;
        case 1:
            EmRoutineSet(em, 1, 3, 0, 0);
            break;
        case 2:
            EmRoutineSet(em, 1, 3, 0, 1);
            break;
        case 3:
            EmRoutineSet(em, 1, 3, 0, 2);
            break;
        case 4:
            EmRoutineSet(em, 1, 3, 0, 3);
            break;
        case 5:
            EmRoutineSet(em, 1, 3, 0, 4);
            break;
        }
        break;
    case 2:
        switch (em->x38D) {
        case 5:
        default:
            EmRoutineSet(em, 1, 6, 0, 0);
            MotionSetCore(em, MOTION(em), ARC(0x12), 0, 0, 1, 0);
            break;
        case 6:
            EmRoutineSet(em, 1, 9, 0, 0);
            MotionSetCore(em, MOTION(em), ARC(0xD), 0, 0, 1, 0);
            break;
        case 7:
            // Plain byte stores: the QImode zero keeps MotionSetCore's `li r9, 0` (em30_R0_Init).
            em->xFC = 1;
            em->xFD = 6;
            em->xFE = 0;
            em->xFF = 1;
            MotionSetCore(em, MOTION(em), ARC(0x12), 0, 0, 1, 0);
            break;
        }
        MotionMoveF(em, 0);
        break;
    }
    em3a_R0_Move(em);
}

static void em3a_R0_Move(cEm3a* em)
{
    Em3a_R1_move_tbl[em->xFD](em);
}

static void em3a_R1_Patrol(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    Vec v;
    Vec a;
    Vec b;
    f32 fl;

    fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
    switch (em->xFE) {
    case 0:
        w->timer = Rnd() % 90 + 90;
        em->xFE++;
    case 1:
        em3aHoverMove(em, w, fl);
        em->rot.y += 0.008726646f;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (w->timer) {
            w->timer--;
        } else if (w->pRoute) {
            em->xFE++;
        }
        break;
    case 2:
        em->xFE++;
    case 3: {
        EmiEntry* r = w->pRoute;

        if (r) {
            a = em->pos;
            a.y = fl + 500.0f;
            b = r->pos;
            b.y += 500.0f;
            RouteCkPosToPos(&a, &b, &w->routePos);
            w->routeAng = Muku(&em->pos, &w->routePos, em->rot.y, PI);
            w->routeAngAbs = fabsf(w->routeAng);
            em->rot.y += Muku(&em->pos, &w->routePos, em->rot.y, 0.017453292f);
            if (w->routeAngAbs < 0.08726646f) {
                v.x = 0.0f;
                v.y = 0.0f;
                v.z = 10.0f;
                PSMTXMultVecSR(em->mat, &v, &v);
                PSVECAdd(&w->spd, &v, &w->spd);
            }
        }
        em3aHoverMove(em, w, fl);
        if (em3aPatrolUpdate(em)) {
            em->xFE = 0;
        }
        break;
    }
    }
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    if (em3aFindPLCk(em)) {
        w->flags |= 1;
        em3aFoundSet(em, w, 9, 0);
    }
    if (w->flags & 1) {
        EmRoutineSet(em, 1, 1, 0, 0);
    }
}

// Gun / rocket attack wait by difficulty rank (R1_Atk / R1_FixAtk step 2).
static inline void em3aSetAtkTimer(Em3aWork* w)
{
    IntSet(w->timer, 46);
    if (pG->x4F88 <= 1) {
        IntSet(w->timer, 76);
    }
    if (pG->x4F88 <= 3) {
        IntSet(w->timer, 61);
    }
    if (pG->x4F88 > 6) {
        IntSet(w->timer, 31);
    }
    if (pG->x4F88 > 9) {
        IntSet(w->timer, 16);
    }
}

static void em3a_R1_Atk(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    f32 fl;
    f32 ang;

    if (em->xFE == 0 && (w->flags & 2)) {
        em->xFE = 2;
    }
    fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(7), 0, 0, 1, 0);
        em->xFE++;
    case 1:
        em3aHoverMove(em, w, fl);
        if (w->flags & 1) {
            em3aTurnToPL(em, 0.034906585f);
        }
        if (MotionMoveF(em, 0)) {
            w->flags |= 2;
            em->xFE++;
        }
        break;
    case 2:
        em3aSetAtkTimer(w);
        em->xFE++;
    case 3:
        em3aHoverMove(em, w, fl);
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.034906585f);
        FSet(em->rot.y, LIMIT_ANGLE(em->rot.y));
        ang = fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI));
        if (em3aLookPLCk(em) == 0 || em->plDist2 > 100000000.0f) {
            EmRoutineSet(em, 1, 2, 0, 0);
            break;
        }
        if (fabsf(em->pos.y - pPL->pos.y) > 5000.0f) {
            break;
        }
        if (ang > 0.5235988f) {
            w->timer = 76;
            break;
        }
        if (w->atkWait) {
            break;
        }
        if ((s16) pG->pl_life <= 0) {
            break;
        }
        if (w->timer) {
            w->timer--;
            if (w->timer % 15 == 0) {
                SndCall(8, 3, &em->pos, em->id, 0, em);
                EstSet((int) em, -1, 0, 0, 2, 0x11, 0, 0, (u32) em, 0);
            }
        } else if (em3aBossCk(em) == 0) {
            if (em->type == 1) {
                em->xFE = 6;
            } else {
                em->xFE++;
            }
        }
        break;
    case 4:
        w->timer = 90;
        em->xFE++;
    case 5:
        em3aHoverMove(em, w, fl);
        em3aTurnToPL(em, 0.017453292f);
        if (em3aBossCk(em)) {
            w->timer = 0;
        }
        if (w->timer) {
            w->timer--;
            if (w->timer % 3 == 0) {
                if (em3aGunHitCk(em)) {
                    if (w->timer > 30) {
                        w->timer = 30;
                    }
                }
            }
        } else {
            w->atkWait = 30;
            em->xFE = 2;
        }
        break;
    case 6:
        w->timer = 30;
        em->xFE++;
    case 7:
        em3aHoverMove(em, w, fl);
        em3aTurnToPL(em, 0.017453292f);
        if (w->timer) {
            w->timer--;
        } else {
            em3aRocketFire(em);
            em->xFE++;
        }
        break;
    case 8:
        MotionSetCore(em, MOTION(em), ARC(8), 0, 0, 1, 0);
        w->flags &= ~2;
        em->xFE++;
    case 9:
        em3aHoverMove(em, w, fl);
        if (w->flags & 1) {
            em3aTurnToPL(em, 0.017453292f);
        }
        if (MotionMoveF(em, 0)) {
            w->atkWait = 60;
            em->xFE = 0;
        }
        break;
    }
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
}

static void em3a_R1_Chase(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    Vec a;
    Vec b;
    Vec v;
    f32 fl;

    fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
    switch (em->xFE) {
    case 0:
        em->xFE++;
    case 1:
        a = em->pos;
        a.y = fl + 500.0f;
        b = pPLS->pos;
        b.y += 500.0f;
        RouteCkPosToPos(&a, &b, &w->routePos);
        w->routeAng = Muku(&em->pos, &w->routePos, em->rot.y, PI);
        w->routeAngAbs = fabsf(w->routeAng);
        em->rot.y += Muku(&em->pos, &w->routePos, em->rot.y, 0.034906585f);
        if (w->routeAngAbs < 0.08726646f) {
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 10.0f;
            PSMTXMultVecSR(em->mat, &v, &v);
            PSVECAdd(&w->spd, &v, &w->spd);
        }
        em3aHoverMove(em, w, fl);
        if (em3aLookPLCk(em) && em->plDist2 < 100000000.0f) {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
}

static void em3a_R1_Fix(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    Vec v;
    int t;

    SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
    // Dead test (t is reassigned before its use): flow1 deletes the arms and jump2 the branch, but
    // the `w->flags` load behind the call gives `w` its sched priority (addi issued among the arg
    // moves) and the branch splits the block for gcse/sched1.
    if (w->flags & 2) {
        t = 1;
    } else {
        t = 0;
    }
    switch (em->xFE) {
    case 0:
        switch (em->xFF) {
        case 0:
        default:
            w->spd.x = 0.0f;
            w->spd.y = 250.0f;
            w->spd.z = 0.0f;
            break;
        case 1:
            w->spd.x = 0.0f;
            w->spd.y = -250.0f;
            w->spd.z = 0.0f;
            break;
        case 2:
            w->spd.x = -250.0f;
            w->spd.y = 0.0f;
            w->spd.z = 0.0f;
            break;
        case 3:
            w->spd.x = 250.0f;
            w->spd.y = 0.0f;
            w->spd.z = 0.0f;
            break;
        case 4:
            w->spd.x = 0.0f;
            w->spd.y = 0.0f;
            w->spd.z = 350.0f;
            break;
        }
        t = 20;
        w->timer = t;
        em->xFE++;
    case 1:
        PSMTXMultVecSR(em->mat, &w->spd, &v);
        PSVECAdd(&em->pos, &v, &em->pos);
        PSVECScale(&w->spd, &w->spd, 0.9f);
        em3aVibMove(em);
        if (w->timer) {
            w->timer--;
        } else {
            w->flags |= 1;
            em3aFoundSet(em, w, 9, 0);
            PSMTXMultVecSR(em->mat, &w->spd, &w->spd);
            EmRoutineSet(em, 1, 4, 0, 0);
        }
        break;
    }
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
}

static void em3a_R1_FixAtk(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    f32 ang;

    if (em->xFE == 0 && (w->flags & 2)) {
        em->xFE = 2;
    }
    SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(7), 0, 0, 1, 0);
        em->xFE++;
    case 1:
        em3aFixMove(em, w);
        if (w->flags & 1) {
            em3aTurnToPL(em, 0.034906585f);
        }
        if (MotionMoveF(em, 0)) {
            w->flags |= 2;
            em->xFE++;
        }
        break;
    case 2:
        em3aSetAtkTimer(w);
        em->xFE++;
    case 3:
        em3aFixMove(em, w);
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.034906585f);
        FSet(em->rot.y, LIMIT_ANGLE(em->rot.y));
        ang = fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI));
        if (fabsf(em->pos.y - pPL->pos.y) > 5000.0f) {
            break;
        }
        if (w->atkWait) {
            break;
        }
        if (em3aLookPLCk(em) == 0) {
            EmRoutineSet(em, 1, 2, 0, 0);
            break;
        }
        if (em->plDist2 > 225000000.0f || ang > 0.5235988f) {
            w->timer = 90;
            break;
        }
        if (w->timer) {
            w->timer--;
            if (w->timer % 15 == 0) {
                SndCall(8, 3, &em->pos, em->id, 0, em);
                EstSet((int) em, -1, 0, 0, 2, 0x11, 0, 0, (u32) em, 0);
            }
        } else if (em3aBossCk(em) == 0) {
            if (em->type == 1) {
                em->xFE = 6;
            } else {
                em->xFE++;
            }
        }
        break;
    case 4:
        w->timer = 90;
        em->xFE++;
    case 5:
        em3aFixMove(em, w);
        em3aTurnToPL(em, 0.034906585f);
        if (em3aBossCk(em)) {
            w->timer = 0;
        }
        if (em3aLookPLCk(em) == 0) {
            EmRoutineSet(em, 1, 2, 0, 0);
            break;
        }
        if (w->timer) {
            w->timer--;
            if (w->timer % 3 == 0) {
                if (em3aGunHitCk(em)) {
                    if (w->timer > 30) {
                        w->timer = 30;
                    }
                }
            }
        } else {
            w->atkWait = 60;
            em->xFE = 2;
        }
        break;
    case 6:
        w->timer = 30;
        em->xFE++;
    case 7:
        em3aFixMove(em, w);
        em3aTurnToPL(em, 0.034906585f);
        if (w->timer) {
            w->timer--;
        } else {
            em3aRocketFire(em);
            w->atkWait = 60;
            em->xFE = 2;
        }
        break;
    case 8:
        MotionSetCore(em, MOTION(em), ARC(8), 0, 0, 1, 0);
        w->flags &= ~2;
        em->xFE++;
    case 9:
        em3aFixMove(em, w);
        if (w->flags & 1) {
            em3aTurnToPL(em, 0.034906585f);
        }
        if (MotionMoveF(em, 0)) {
            w->atkWait = 60;
            em->xFE = 0;
        }
        break;
    }
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
}

static void em3a_R1_Die(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    if (em->xFE == 0) {
        em->hp = 0;
        EmSetDie(em);
        EmSetDieCntE(em);
        EffectEspDelete(1, w->espKind, (u32) em, 0);
        EffectEspgenDelete(1, w->espKind, (int) em);
        EffectEfmDelete(1, w->espKind, (int) em);
        EstSet((int) em, -1, 0, 0, 2, 6, 0, 0, (u32) em, 0);
        SndStop(w->sndId, 0);
        SndCall(8, 2, &em->pos, em->id, 0, em);
        if (em->type == 2) {
            PlWepHitCheck2(0, &em->pos, &em->pos, 0x13, 3, 4000.0f);
        }
        em->setStatus(5);
        em->be_flag &= ~2;
        AtariOff(&em->atari, 0xFCFF);
        em->xFE++;
    }
}

static void em3a_R1_B_HideWait(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    w->flags |= 8;
    switch (em->xFE) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        MotionSetCore(em, MOTION(em), ARC(0x12), 0, 3, 5, 0);
        w->flags &= ~1;
        w->timer = 30;
        w->turnDir = 0;
        em->xFE++;
    case 1:
        if (em->xFF) {
            MotionMoveF(em, 0);
            break;
        }
        if (w->turnDir) {
            // LIMIT_ANGLE written in both arms: the tails are cross-jumped after reload, while
            // a shared statement makes `ry` a global pseudo and swaps the f0/f13 temps.
            if (w->turnDir & 1) {
                em->rot.y += 0.02617994f;
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            } else {
                em->rot.y -= 0.02617994f;
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            }
        }
        if (w->timer) {
            w->timer--;
        } else if (w->turnDir) {
            w->turnDir = 0;
            w->timer = Rnd() % 60 + 60;
        } else {
            w->turnDir = (Rnd() & 1) + 1;
            if (Rnd() % 10 > 4) {
                w->timer = 30;
            } else {
                w->timer = 60;
            }
        }
        MotionMoveF(em, 0);
        break;
    }
    if (em->xFF) {
        if (em->flags_3C8 & 1) {
            w->flags |= 1;
        }
    } else if (em3aFindPLCk(em)) {
        if (fabsf(em->pos.y - pPL->pos.y) < 250.0f) {
            w->flags |= 1;
        }
    }
    if (w->flags & 1) {
        em3aFoundSet(em, w, 0xB, 0xA);
        EmRoutineSet(em, 1, 8, 0, 0);
    }
}

static void em3a_R1_B_Hide(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    switch (em->xFE) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        MotionSetCore(em, MOTION(em), ARC(0x11), 0, 3, 1, 0);
        SndCall(8, 5, &em->getPartsPtr(0xA)->worldPos, em->id, 0, em);
        EffectEspDelete(1, w->espKind, (u32) em, 0);
        EffectEspgenDelete(1, w->espKind, (int) em);
        EffectEfmDelete(1, w->espKind, (int) em);
        EstSet((int) em, -1, 0, 0, 2, 0xA, 1, w->espKind, (u32) em, 0);
        EstSet((int) em, -1, 0, 0, 2, 0xC, 0, 0, (u32) em, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 6, 0, 0);
        }
        break;
    }
}

static void em3a_R1_B_Appear(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    switch (em->xFE) {
    case 0:
        AtariOn(&em->atari, 0x300);
        MotionSetCore(em, MOTION(em), ARC(0x13), 0, 3, 1, 0);
        SndCall(8, 5, &em->getPartsPtr(0xA)->worldPos, em->id, 0, em);
        EstSet((int) em, -1, 0, 0, 2, 0xD, 0, 0, (u32) em, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->lostCnt = 0;
            EmRoutineSet(em, 1, 0xA, 0, 0);
        }
        break;
    }
}

static void em3a_R1_B_Wait(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    switch (em->xFE) {
    case 0:
        AtariOn(&em->atari, 0x300);
        MotionSetCore(em, MOTION(em), ARC(0xD), 0, 3, 5, 0);
        w->flags &= ~1;
        w->turnDir = 0;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        if (Rnd() % 10 > 4) {
            MotionSetCore(em, MOTION(em), ARC(0x10), 0, 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x10), 0, 3, 1, 0);
        }
        w->timer = Rnd() % 3 + 2;
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    }
    if (em3aFindPLCk(em)) {
        if (fabsf(em->pos.y - pPL->pos.y) < 250.0f) {
            w->flags |= 1;
        }
    }
    if (w->flags & 1) {
        em3aFoundSet(em, w, 0xB, 0xA);
        EmRoutineSet(em, 1, 0xA, 0, 0);
    }
}

static void em3a_R1_B_Move(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    int lim;

    RouteCkToPos(em, &pPL->pos, &w->routePos, 0, 0);
    w->routeAng = Muku(&em->pos, &w->routePos, em->rot.y, PI);
    w->routeAngAbs = fabsf(w->routeAng);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE), (int) ARC(0xF), 5, 5, 0);
        w->timer = Rnd() % 3 + 2;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->routePos, em->rot.y, 0.05235988f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            if (w->timer == 0) {
                em->xFE = 4;
                break;
            }
            w->timer--;
        }
        if (w->lostCnt > 89) {
            EmRoutineSet(em, 1, 7, 0, 0);
            break;
        }
        if (w->routeAngAbs > 0.5235988f) {
            em->xFE = 2;
        }
        break;
    case 2:
        if (w->routeAng < 0.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x10), 0, 5, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x10), 0, 5, 1, 0);
        }
        w->timer = Rnd() % 3 + 2;
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if (w->routeAngAbs < 0.5235988f) {
                em->xFE = 0;
            } else {
                em->xFE = 2;
            }
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0xD), 0, 0xA, 5, 0);
        w->timer = Rnd() % 60 + 60;
        em->xFE++;
    case 5:
        MotionMoveF(em, 0);
        if (w->timer == 0) {
            em->xFE = 0;
            break;
        }
        w->timer--;
        if (w->routeAngAbs > 1.0471976f) {
            em->xFE = 2;
            break;
        }
        if (w->lostCnt > 89) {
            EmRoutineSet(em, 1, 7, 0, 0);
        }
        break;
    }
    if ((em->pos.x - pPL->pos.x) * (em->pos.x - pPL->pos.x) + (em->pos.y - pPL->pos.y) * (em->pos.y - pPL->pos.y)
            + (em->pos.z - pPL->pos.z) * (em->pos.z - pPL->pos.z) < 9000000.0f
        && em3aBossNearCk(em) == 0) {
        u8 rank;

        w->nearCnt++;
        if (w->nearCnt % 15 == 5) {
            SndCall(8, 3, &em->pos, em->id, 0, em);
            EstSet((int) em, -1, 0, 0, 2, 0x12, 0, 0, (u32) em, 0);
        }
        rank = pG->x4F88;
        lim = 76;
        if (rank <= 3) {
            lim = 91;
        }
        if (rank <= 1) {
            lim = 121;
        }
        if (rank > 6) {
            lim = 61;
        }
        if (rank > 9) {
            lim = 46;
        }
        if (w->nearCnt > lim) {
            EmRoutineSet(em, 1, 0xD, 0, 0);
            return;
        }
    } else {
        w->nearCnt = 0;
    }
    if (em->plDist2 > 25000000.0f) {
        w->lostCnt++;
    } else {
        w->lostCnt = 0;
    }
    if (fabsf(em->pos.y - pPL->pos.y) > 1000.0f) {
        w->lostCnt = 90;
    }
}

static void em3a_R1_B_Die(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x14), 0, 3, 1, 0);
        EstSet((int) em, -1, 0, 0, 2, 0xF, 0, 0, (u32) em, 0);
        SndCall(8, 7, &em->getPartsPtr(0xA)->worldPos, em->id, 0, em);
        em->hp = 0;
        w->timer = 50;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            EmRoutineSet(em, 1, 0xD, 0, 0);
        }
        break;
    }
}

static void em3a_R1_B_AppearDie(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    switch (em->xFE) {
    case 0: {
        Vec* wp;

        MotionSetCore(em, MOTION(em), ARC(0x15), 0, 3, 1, 0);
        EstSet((int) em, -1, 0, 0, 2, 0xE, 0, 0, (u32) em, 0);
        wp = &em->getPartsPtr(0xA)->worldPos;
        SndCall(8, 5, wp, em->id, 0, em);
        SndCall(8, 7, wp, em->id, 0, em);
        em->hp = 0;
        w->timer = 50;
        em->xFE++;
    }
    case 1:
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            EmRoutineSet(em, 1, 0xD, 0, 0);
        }
        break;
    }
}

static void em3a_R1_B_Bomb(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    if (em->xFE == 0) {
        Vec p;

        em->hp = 0;
        EmSetDie(em);
        EmSetDieCntE(em);
        EffectEspDelete(1, w->espKind, (u32) em, 0);
        EffectEspgenDelete(1, w->espKind, (int) em);
        EffectEfmDelete(1, w->espKind, (int) em);
        EstSet((int) em, -1, 0, 0, 2, 0x10, 0, 0, (u32) em, 0);
        SndStop(w->sndId, 0);
        SndCall(8, 2, &em->pos, em->id, 0, em);
        em->clearStatus(5);
        em->setStatus(8);
        EmSetDropItem(em);
        p = em->pos;
        p.y += 250.0f;
        PlWepHitCheck2(0, &p, &p, 0x13, 3, 3000.0f);
        em->be_flag &= ~2;
        AtariOff(&em->atari, 0xFCFF);
        em->xFE++;
    }
}

void em3aVibMove(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    w->vibAng.x += w->vibSpd.x;
    em->pos.x += SINF(w->vibAng.x) * 6.0f;
    w->vibAng.y += w->vibSpd.y;
    em->pos.y += SINF(w->vibAng.y) * 3.0f;
    w->vibAng.z += w->vibSpd.z;
    em->pos.z += SINF(w->vibAng.z) * 6.0f;
}

void em3aGunMove(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    if (em->type != 2 && (w->flags & 2)) {
        Vec t;
        Vec d;
        cModel* p;
        f32 len;
        f32 ang;

        t = pPL->pos;
        t.y += 1300.0f;
        em->getPartsPtr(0);
        p = em->getPartsPtr(9);
        PSVECSubtract(&t, &p->worldPos, &d);
        len = SQRTF(d.x * d.x + d.z * d.z);
        ang = -atan2f(d.y, len);
        if (ang < -0.2617994f) {
            ang = -0.2617994f;
        }
        if (ang > 0.7853982f) {
            ang = 0.7853982f;
        }
        p->rot.x += Muku2(p->rot.x, ang + 1.5707964f, 0.024543693f);
        p->rot.x = LIMIT_ANGLE(p->rot.x);
    }
}

int em3aSetDmVal(cEm3a* em)
{
    int flag;
    int val;

    flag = 0;
    if (em->dmg.part->rad < 36000000.0f) {
        flag = 1;
    }
    val = 100;
    if (em->dmWep <= 0x2D) {
        val = GetWepDmVal(em, em->dmWep, flag);
    }
    return val * 5;
}

void em3aPatrolInit(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    u32 i;

    w->pRoute = 0;
    if (GRef(pG)->pRoomEmi == 0) {
        return;
    }
    for (i = 0; i < ((EmiData*) pG->pRoomEmi)->n; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pRoomEmi + o);

        if (e->type == 0x13 && e->state != 0 && e->state == em->x3D0 && e->pad_3 == 0) {
            EmiSet(w->pRoute, e);
            return;
        }
    }
}

int em3aPatrolUpdate(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    EmiData* emi;
    u32 i;

    emi = (EmiData*) pG->pRoomEmi;
    if (emi == 0) {
        return 0;
    }
    if (w->pRoute == 0) {
        return 0;
    }
    if ((w->pRoute->pos.x - em->pos.x) * (w->pRoute->pos.x - em->pos.x)
            + (w->pRoute->pos.z - em->pos.z) * (w->pRoute->pos.z - em->pos.z)
        > 250000.0f) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type == 0x13 && e->state != 0 && e->state == em->x3D0 && e->pad_3 == w->pRoute->pad_3 + 1) {
            w->pRoute = e;
            return 1;
        }
    }
    for (i = 0; i < ((EmiData*) pG->pRoomEmi)->n; i++) {
        EmiEntry* e = &((EmiData*) pG->pRoomEmi)->entry[i];

        if (e->type == 0x13 && e->state != 0 && e->state == em->x3D0 && e->pad_3 == 0) {
            w->pRoute = e;
            return 1;
        }
    }
    return 0;
}

void em3aFanMove(cEm3a* em)
{
    if (em->type != 2 && em->hp > 0) {
        cModel* p;

        p = em->getPartsPtr(5);
        p->rot.y += 0.5235988f;
        p->rot.y = LIMIT_ANGLE(p->rot.y);
        p = em->getPartsPtr(7);
        p->rot.y += 0.5235988f;
        p->rot.y = LIMIT_ANGLE(p->rot.y);
    }
}

int em3aFindPLCk(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);
    f32 range;
    f32 ang;

    if (w->flags & 1) {
        return 1;
    }
    if (em->plDist2 < 6250000.0f) {
        return 1;
    }
    if (em->type == 2) {
        range = 5000.0f;
    } else {
        range = 15000.0f;
    }
    ang = fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI));
    if (ang < 0.5235988f && em->plDist2 < range * range) {
        cModel* p;
        Vec a;
        Vec b;

        if (em->type == 2) {
            p = em->getPartsPtr(0xA);
        } else {
            p = em->getPartsPtr(0);
        }
        a = p->worldPos;
        b = pPLS->pos;
        b.y += 1000.0f;
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0) {
            return 1;
        }
    }
    if (em->type == 2) {
        return 0;
    }
    if (em3aDeadCk(em)) {
        return 1;
    }
    if ((pG->flags_500C & 0x00800000) && em->plDist2 < 225000000.0f) {
        return 1;
    }
    if ((pG->flags_5010 & 0x20000000) && pG->bell_stat == 2) {
        if ((em->pos.x - pG->bell_pos.x) * (em->pos.x - pG->bell_pos.x)
                + (em->pos.y - pG->bell_pos.y) * (em->pos.y - pG->bell_pos.y)
                + (em->pos.z - pG->bell_pos.z) * (em->pos.z - pG->bell_pos.z)
            < 100000000.0f) {
            return 1;
        }
    }
    return 0;
}

int em3aLookPLCk(cEm3a* em)
{
    Vec a;
    Vec b;

    a = em->pos;
    b = pPLS->pos;
    a.y += 200.0f;
    b.y += 1000.0f;
    return EatMgr.hitCheck(&a, &b, 0, 0, 0, 0) == 0;
}

int em3aGunHitCk(cEm3a* em)
{
    Vec a;
    Vec b;
    EmAtkInfo atk;
    Vec hit;
    Vec nrm;
    Vec s;
    Vec rot;
    Vec d;
    u32 attr;
    cModel* p;
    cEm* hitEm;
    s16 hp;
    f32 len;
    int ret;

    EstSet((int) em, -1, 0, 0, 2, 3, 0, 0, (u32) em, 0);
    SndCall(8, 1, &em->pos, em->id, 0, em);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = fRand1_1() * 5000.0f;
    b.y = fRand1_1() * 5000.0f;
    b.z = 50000.0f;
    p = em->getPartsPtr(0xB);
    PSMTXMultVec(p->mat, &a, &a);
    PSMTXMultVec(p->mat, &b, &b);
    hp = em->hp;
    em->hp = 0;
    PlWepHitCheck2(0, &a, &b, 0xC, 3, 6000.0f);
    hitEm = EmAtkLineHitCk(&a, &b, &hit, &nrm, &attr);
    em->hp = hp;
    if (hitEm == 0) {
        len = SQRTF(nrm.x * nrm.x + nrm.z * nrm.z);
        rot.x = -atan2f(nrm.y, len);
        rot.y = atan2f(nrm.x, nrm.z);
        rot.z = 0.0f;
        PSVECScale(&nrm, &s, 30.0f);
        PSVECAdd(&hit, &s, &hit);
        EstSet(0, -1, &hit, &rot, 2, 4, 0, 0, (u32) hitEm, hitEm);
        PSVECSubtract(&hit, &a, &d);
        EspSetGatling(a, d);
        SndCall(6, 0xA, &hit, 0, 0, 0);
        ret = 0;
    } else {
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        SndCall(6, 0x15, &pPL->pos, 0, 0, pPL);
        QuakeExec(0, 0, 5, 22.0f, 2);
        EmPlBloodSet2(em, &em->pos, 1, 2, 7);
        atk = em3a_atk_info;
        EmAtkSetDamagePL(hitEm, &atk, &a, &b);
        ret = 1;
    }
    return ret;
}

void em3aRocketFire(cEm3a* em)
{
    Vec pos;
    Vec rot;
    cObjMissile* m;

    pos.x = 0.0f;
    pos.y = -101.0f;
    pos.z = -143.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    m = (cObjMissile*) SetHeliMissile(ARC(9), ARC(0xA), &pos, &rot, 1);
    if (m) {
        m->setParent(em, 9, 0);
        m->setFire(0);
    }
}

int em3aBossCk(cEm3a* em)
{
    Mtx inv;
    Vec lp;
    u32 i;

    PSMTXInverse(em->mat, inv);
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((e->be_flag & 0x201) == 1 && e->id == 0x39 && e->hp > 0 && (e->be_flag & 2)) {
            PSMTXMultVec(inv, &e->pos, &lp);
            if (lp.x > -4000.0f && lp.x < 4000.0f && lp.z > 0.0f && lp.z < 30000.0f) {
                return 1;
            }
            if (fabsf(Muku(&em->pos, &e->pos, em->rot.y, PI)) < 0.7853982f && lp.z > 0.0f && lp.z < 15000.0f) {
                return 1;
            }
        }
    }
    return 0;
}

int em3aBossNearCk(cEm3a* em)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((e->be_flag & 0x201) == 1 && e->id == 0x39 && e->hp > 0
            && (em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y)
                    + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z)
                < 16000000.0f) {
            return 1;
        }
    }
    return 0;
}

void em3aEngineSe(cEm3a* em)
{
    Em3aWork* w = EM3A_WK(em);

    if (em->type != 2 && em->hp > 0) {
        if (w->seTimer) {
            w->seTimer--;
        } else {
            w->seTimer = 29;
            w->sndId = SndCall(8, 0, &em->pos, em->id, 0, em);
        }
    }
}

void cEm3a::setAtkWait(int frames)
{
    EM3A_WK(this)->atkWait = frames;
}
