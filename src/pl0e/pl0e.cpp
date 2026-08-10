// pl0e module (D:/Bio4/Prog/pl0e.cpp): the jet ski of the chase. A cEm the room script puts on a rail
// path (setRail / set2ndRail) and the player rides (setRide): pl0ePathMove follows the path with the
// stick steering the lateral offset, the player and partner routines (PlBoatMove / plboat_R2_*,
// subBoat*) ride on it, pl0eCamMove drives the camera, pl0eWaveMove the wave object under it.

#include "atari.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "pl0e.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_body.h"
#include "pl_wep.h"
#include "global.h"
#include "main.h"
#include "joy.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "esp.h"
#include "est.h"
#include "obj00.h"
#include "snd.h"
#include "pad.h"
#include "game.h"
#include "sscrn.h"
#include "dbmodule.h"
#include "rnd.h"
#include "math_sub.h"
#include "db_log.h"

// The module's 0x34-byte COMMON block (st_room.h): uninitialised template statics of the original
// object, appended to .bss by snmakerel.
asm(".comm common_pl0e,52,4");

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);              // game/em.cpp
extern void (*BoatMoveFunc)(cPlayer* pl);        // game/player.cpp (pl_R1_Boat calls it)
extern "C" void Em_R0_Scenario(cEm* em);                    // game/em_sub.cpp
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// COMPILER-DIFF: #1 (argument-move order): pl0ePathMove issues `lwz r4, pRailObj` before `lfs f1, dist`;
// the model-before-dist redeclaration is ABI-identical (GPR / FPR argument registers are numbered independently).
int PathGetPosEmM(void* path, cModel* model, f32 dist, u16* seg, Vec* out) asm("PathGetPosEm");   // MotionMove called with a second argument (pl_npc.cpp)

#line 1 "D:/Bio4/Prog/pl0e.cpp"

typedef void (*Pl0eFunc)(cPl0e*);
typedef void (*PlBoatFunc)(cPlayer*);

static void pl0e_R0_Init(cPl0e* em);
static void pl0e_R0_Move(cPl0e* em);
static void pl0e_R1_Wait(cPl0e* em);
static void pl0e_R1_Ride(cPl0e* em);
static void pl0e_R1_RailMove(cPl0e* em);
static void pl0e_R1_Jump(cPl0e* em);
static void pl0e_R1_Crash(cPl0e* em);
static void pl0e_R1_Sink(cPl0e* em);
static void pl0e_R1_JumpMiss(cPl0e* em);
static void PlBoatMove(cPlayer* pl);
static void plboat_R2_Ride(cPlayer* pl);
static void plboat_R2_Move(cPlayer* pl);
static void plboat_R2_Jump(cPlayer* pl);
static void plboat_R2_Landing(cPlayer* pl);
static void plboat_R2_Crash(cPlayer* pl);
static void plboat_R2_Sink(cPlayer* pl);
static void plboat_R2_JumpMiss(cPlayer* pl);
static void subBoatRide();
static void subBoatRun();
static void subBoatJump();
static void subBoatLanding();
static void subBoatCrash();
static void subBoatSink();
static void subBoatJumpMiss();

#define ARC(no) PL_ARC_PTR(em->subArc, no)
#define SUBARC(no) PL_ARC_PTR(sub->subArc, no)
#define PLARC(no) PL_ARC_PTR(pl->subArc, no)
#define VIB_TBL ((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc))
#define PL_BOAT(pl) ((cPl0e*) (pl)->pBoat)
#define SUB_BOAT(sub) ((cPl0e*) (sub)->dmgType)

// Store through a reference: a scalar (non-struct) MEM, so a following global load stays below it.
static inline void PSet(void*& d, void* v) { d = v; }
// Read through a reference: a MEM with neither the struct nor the scalar flag stays below preceding member stores.
static inline f32 FRef(f32& v) { return v; }
static inline void U16And(u16& d, u16 m) { d &= m; }
static inline void ISet(int& d, int v) { d = v; }
static inline void U16Set(u16& d, u16 v) { d = v; }
static inline void U32Set(u32& d, u32 v) { d = v; }

struct SubCharPtr {
    cSubChar* p;
};
#define pSUBS (((SubCharPtr*) &pSUB)->p)
static inline GlobalWork* GRef(GlobalWork*& p) { return p; }

// Struct-member view of pPL: the load stays below a preceding store through a work pointer.
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void PlRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

// Speed towards a limit by 25 per frame: from above it falls, from below it rises, never crossing it.
#define SPD_ADJUST(spd, lim)                    \
    if ((spd) > (lim)) {                        \
        (spd) -= 25.0f;                         \
        if ((spd) < (lim)) (spd) = (lim);       \
    } else {                                    \
        (spd) += 25.0f;                         \
        if ((spd) > (lim)) (spd) = (lim);       \
    }

f32 pl0e_spd_max = 800.0f;
static f32 pl0e_spd_boost = 1440.0f;
static f32 pl0e_spd_slow = 600.0f;

static Pl0eFunc Pl0e_R0_move_tbl[5] = {
    pl0e_R0_Init,
    pl0e_R0_Move,
    0,
    0,
    (Pl0eFunc) Em_R0_Scenario,
};

static Pl0eFunc Pl0e_R1_move_tbl[7] = {
    pl0e_R1_Wait,
    pl0e_R1_Ride,
    pl0e_R1_RailMove,
    pl0e_R1_Jump,
    pl0e_R1_Crash,
    pl0e_R1_Sink,
    pl0e_R1_JumpMiss,
};

extern "C" void _prolog()
{
    OSReport("Pl0e prolog Ok\n");
    EmInitFunc = Pl0eInit;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Pl0eInit(cEm* em)
{
    new (em) cPl0e();
}

void cPl0e::move()
{
    Pl0eWork* w = PL0E_WK(this);

    w->flags &= ~1;
    w->rotY = rot.y;
    Pl0e_R0_move_tbl[xFC](this);
    if (pG->room_id != 0x10D && pG->room_id != 0x10E) {
        if (w->cnt68 == 0) {
            w->cnt68 = 0x1D;
        } else {
            w->cnt68--;
        }
    }
    pl0eWaveMove(this);
}

void cPl0e::setPos(Vec* p, f32 ang)
{
    pos = *p;
    oldPos = pos;
    rot.y = ang;
    rot.x = 0.0f;
    rot.z = 0.0f;
    RotMatrix(mat, &rot);
    TransMatrix(mat, &pos);
    partsMatCalc();
    partsWorldCalc();
    EffectEspDelete(0, 0x35, (u32) this, 0);
    EffectEspgenDelete(0, 0x35, (int) this);
    EffectEfmDelete(0, 0x35, (int) this);
}

static void pl0e_R0_Init(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);
    int zero;

    em->modelInit(ARC(0x5), ARC(0x6));
    em->be_flag &= ~0x10;
    em->x12F = 0;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 4);
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(3);
    em->atari.flags &= 0xFCFF;
    em->atari.setPriority(1);
    em->setStatus(1);
    EspDataLoad((u32) ARC(0x4), 0xE, 0);
    w->flags = zero;
    w->cnt68 = 0x1D;
    w->sink = 96000.0f;
    // pRailObj / spdX are written LAST: they are the last uses of the shared zero (r28) and 0.0 (f31),
    // so sched1 issues them first (dying source) and the target's block order comes out (weight model).
    w->roll = 0.0f;
    w->pitch = 0.0f;
    w->rollPhase = 0.0f;
    w->pitchPhase = 0.0f;
    w->x54 = 0.0f;
    w->x78 = zero;
    w->x71 = zero;
    w->x72 = zero;
    w->swayAmp.x = 0.0f;
    w->swayAmp.y = 0.0f;
    w->swayAmp.z = 0.0f;
    w->swayPhase.x = 0.0f;
    w->swayPhase.y = 0.0f;
    w->swayPhase.z = 0.0f;
    w->xD8 = 0.0f;
    w->camRate = 0.0f;
    w->jumpCnt = zero;
    w->seNo = zero;
    w->pitch104 = zero;
    w->pPath = 0;
    w->ofs.x = 0.0f;
    w->ofs.y = 0.0f;
    w->ofs.z = 0.0f;
    w->pRailObj = 0;
    w->spdX = 0.0f;
    w->floorY0 = em->pos.y;
    w->floorY1 = em->pos.y;
    w->pWave = SetObj00((void*) (pGS->pArc->ofs_20 + (u32) pGS->pArc), (void*) (pGS->pArc->ofs_24 + (u32) pGS->pArc), 0, 0);
    w->espKind = EspPullCoreKind();
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
    em->xFC = 1;
    pl0e_R0_Move(em);
}

static void pl0e_R0_Move(cPl0e* em)
{
    Pl0e_R1_move_tbl[em->xFD](em);
}

static void pl0e_R1_Wait(cPl0e* em)
{
    em->pos.y = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
    pl0eBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
    pl0eRideActEvtCk(em);
}

static void pl0e_R1_Ride(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);

    switch (em->xFE) {
    case 0:
        em->pos.x = 0.0f;
        em->pos.y = 0.0f;
        em->pos.z = 0.0f;
        em->rot.x = 0.0f;
        em->rot.y = 0.0f;
        em->rot.z = 0.0f;
        MotionSetCore(em, &em->mot, ARC(0xF), 0, 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 0xE, 0xA, 1, w->espKind, (u32) em, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->pos.y = -26663.0f;
            em->rot.y = 2.2f;
            w->seNo = SndCall(8, 0xA, &em->pos, em->id, 0, em);
            EffectEspDelete(1, w->espKind, (u32) em, 0);
            EffectEspgenDelete(1, w->espKind, (int) em);
            EffectEfmDelete(1, w->espKind, (int) em);
            PlRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    em->partsWorldCalc();
}

static void pl0e_R1_RailMove(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);

    switch (em->xFE) {
    case 0:
        w->hokan = 0;
        w->frame = 0;
        w->frameOld = 0;
        w->blendRate = 0.0f;
        w->spdY = 0.0f;
        em->xFE++;
    case 1:
        pl0ePathMove(em, 0);
        pl0eSlopeControl(em);
        if (pl0eCrashCk(em)) {
            pG->pl_life = 0;
            PlRoutineSet(em, 1, 4, 0, 0);
        } else if (pl0eSinkCk(em)) {
            pG->pl_life = 0;
            PlRoutineSet(em, 1, 5, 0, 0);
        } else if (pl0eJumpMissCk(em)) {
            pG->pl_life = 0;
            PlRoutineSet(em, 1, 6, 0, 0);
        } else if (pl0eJumpCk(em)) {
            w->spdY = 200.0f;
            PlRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
    if (Key.on & 0xC) {
        if (Key.on & 0x8) {
            w->blendRate += 31.875f;
            if (w->blendRate > 255.0f) {
                w->blendRate = 255.0f;
            }
        }
        if (Key.on & 0x4) {
            w->blendRate -= 31.875f;
            if (w->blendRate < -255.0f) {
                w->blendRate = -255.0f;
            }
        }
    } else {
        w->blendRate *= 0.9f;
    }
    w->frameOld = w->frame;
    pl0eBlendMotSet(em, ARC(0x8), ARC(0xA), ARC(0x9), 0, 0, 0);
    MotionMoveF(em, 0);
    em->partsWorldCalc();
}

static void pl0e_R1_Jump(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);

    switch (em->xFE) {
    case 0:
        w->jumpCnt++;
        if ((Joy[0].on & 0x60) == 0x60) {
            if (w->flags & 8) {
                MotionSetCore(em, &em->mot, ARC(0x14), 0, 0xA, 1, 0);
                PlRoutineSet(pPL, 0, 0xF, 2, 0);
                U32Set(pPL->x3E0, 2);
                if (pSUB) {
                    SetSubDamage((int) em, (void*) subBoatJump);
                    pSUB->xFF = 2;
                }
            } else {
                MotionSetCore(em, &em->mot, ARC(0xE), 0, 0xA, 1, 0);
                PlRoutineSet(pPL, 0, 0xF, 2, 0);
                U32Set(pPL->x3E0, 1);
                if (pSUB) {
                    SetSubDamage((int) em, (void*) subBoatJump);
                    pSUB->xFF = 1;
                }
            }
        } else {
            MotionSetCore(em, &em->mot, ARC(0xB), 0, 0xA, 1, 0);
            PlRoutineSet(pPL, 0, 0xF, 2, 0);
            U32Set(pPL->x3E0, 0);
            if (pSUB) {
                SetSubDamage((int) em, (void*) subBoatJump);
            }
        }
        w->flags |= 8;
        SndCall(8, 8, &em->pos, em->id, 0, em);
        VibSetData(VIB_TBL, 7, 1);
        w->timer = 5;
        w->fall = 0;
        em->xFE++;
    case 1:
        w->flags |= 1;
        pl0ePathMove(em, 1);
        pl0eSlopeControl(em);
        if (pl0eJumpCk(em)) {
            w->spdY = 200.0f;
        }
        if (w->timer) {
            w->timer--;
            if (w->timer == 0) {
                SndStop(w->seNo, 0);
            }
        }
        if (w->spdY < -15.0f) {
            w->fall = 1;
        }
        if (w->fall && w->spdY > -15.0f) {
            em->xFE++;
        } else {
            MotionMoveF(em, 0);
        }
        break;
    case 2:
        w->hokan = 0;
        w->blendRate = 0.0f;
        w->frame = 0;
        w->frameOld = 0;
        PlRoutineSet(pPLS, 0, 0xF, 3, 0);
        if (pSUB) {
            SetSubDamage((int) em, (void*) subBoatLanding);
        }
        if (em->be_flag & 2) {
            EstSet((int) em, -1, 0, 0, 0xE, 4, 0, 0, (u32) em, 0);
        }
        SndCall(8, 9, &em->pos, em->id, 0, em);
        VibSetData(VIB_TBL, 7, 1);
        w->seNo = SndCall(8, 0xA, &em->pos, em->id, 0, em);
        em->xFE++;
    case 3:
        pl0ePathMove(em, 0);
        pl0eSlopeControl(em);
        w->frameOld = w->frame;
        pl0eBlendMotSet(em, ARC(0xC), ARC(0x11), ARC(0x10), 0, 0, 0);
        if (MotionMoveF(em, 0)) {
            PlRoutineSet(em, 1, 2, 0, 0);
        } else if (pl0eCrashCk(em)) {
            pG->pl_life = 0;
            PlRoutineSet(em, 1, 4, 0, 0);
        } else if (pl0eSinkCk(em)) {
            pG->pl_life = 0;
            PlRoutineSet(em, 1, 5, 0, 0);
        } else if (pl0eJumpMissCk(em)) {
            pG->pl_life = 0;
            PlRoutineSet(em, 1, 6, 0, 0);
        } else if (pl0eJumpCk(em)) {
            w->spdY = 200.0f;
            PlRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
    em->partsWorldCalc();
}

static void pl0e_R1_Crash(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0xD), 0, 3, 1, 0);
        if (w->flags & 2) {
            EstSet((int) em, -1, 0, 0, 0xE, 0xB, 0, 0, (u32) em, 0);
        } else {
            EstSet((int) em, -1, 0, 0, 0xE, 5, 0, 0, (u32) em, 0);
        }
        PlRoutineSet(pPL, 0, 0xF, 4, 0);
        if (pSUB) {
            SetSubDamage((int) em, (void*) subBoatCrash);
        }
        SndStop(w->seNo, 0);
        SndStrReq(1, 0x38, 0x80000003, 0, 0, 0.0f);
        VibSetData(VIB_TBL, 0xD, 1);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        break;
    }
    em->partsWorldCalc();
}

static void pl0e_R1_Sink(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);

    switch (em->xFE) {
    case 0:
        em->pos.x = 0.0f;
        em->pos.y = 0.0f;
        em->pos.z = 0.0f;
        em->rot.x = 0.0f;
        em->rot.y = 0.0f;
        em->rot.z = 0.0f;
        MotionSetCore(em, &em->mot, ARC(0x12), 0, 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 0xE, 0xC, 0, 0, (u32) em, 0);
        w->flags |= 4;
        pGS->pl_life = 0;
        DiedemoExec(0x1E, 0);
        w->xD4 = 0x14;
        PlRoutineSet(pPLS, 0, 0xF, 5, 0);
        if (pSUB) {
            SetSubDamage((int) em, (void*) subBoatSink);
        }
        SndStop(w->seNo, 0);
        SndStrReq(1, 0x39, 0x80000003, 0, 0, 0.0f);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->frame > 17.7f && em->frame < 18.3f) {
            VibSetData(VIB_TBL, 0xD, 1);
        }
        break;
    }
    em->partsWorldCalc();
}

static void pl0e_R1_JumpMiss(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);

    switch (em->xFE) {
    case 0:
        em->pos.x = 0.0f;
        em->pos.y = 0.0f;
        em->pos.z = 0.0f;
        em->rot.x = 0.0f;
        em->rot.y = 0.0f;
        em->rot.z = 0.0f;
        MotionSetCore(em, &em->mot, ARC(0x13), 0, 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 0xE, 0xD, 0, 0, (u32) em, 0);
        pG->pl_life = 0;
        DiedemoExec(0x1E, 0);
        w->xD4 = 0x14;
        PlRoutineSet(pPLS, 0, 0xF, 6, 0);
        if (pSUB) {
            SetSubDamage((int) em, (void*) subBoatJumpMiss);
        }
        SndStop(w->seNo, 0);
        SndStrReq(1, 0x72, 0x80000003, 0, 0, 0.0f);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->frame > 32.7f && em->frame < 33.3f) {
            VibSetData(VIB_TBL, 0xD, 1);
        }
        break;
    }
    em->partsWorldCalc();
}

void pl0eBoatControl(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);
    Mtx m;
    Vec v;

    PSMTXRotRad(em->mat, 'y', em->rot.y);
    PSMTXMultVec(em->mat, &w->ofsF0, &v);
    PSVECAdd(&em->pos, &v, &em->pos);
    TransMatrix(m, &em->pos);
    RotMatrix(em->worldMat, &em->rot);
    TransMatrix(em->worldMat, &em->pos);
    ScaleMatrix(em->worldMat, &em->scale);
    PSMTXCopy(em->worldMat, em->mat);
    pl0eScrAdjust(em);
    pl0eGetBoatDir(em);
    pl0eBoatRoll(em);
}

void pl0eGetBoatDir(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);
    Vec d;

    PSVECSubtract(&em->pos, &em->oldPos, &d);
    w->spdXZ = SQRTF(d.x * d.x + d.z * d.z);
    if (w->spdXZ > 100.0f) {
        w->dirAng = atan2f(d.x, d.z);
        w->dirAng = Muku2(em->rot.y, w->dirAng, PI);
    } else {
        w->dirAng *= 0.9f;
    }
    w->dirAngAbs = fabsf(w->dirAng);
}

void pl0eBoatRoll(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);
    Mtx m;
    Vec sway;
    f32 t;
    f32 a;

    if (w->dirAngAbs < PI / 2) {
        t = w->spdXZ * 0.01f;
        if (t > 1.0f) {
            t = 1.0f;
        }
        a = w->dirAng * 0.3f * t;
        w->roll = w->roll * 0.9f + a * 0.1f;
    } else {
        w->roll *= 0.9f;
    }
    w->rollPhase += fRand0_1() * 0.3926991f + 0.09817477f;
    sinf(w->rollPhase);
    PSMTXRotRad(m, 'z', w->roll);
    PSMTXConcat(em->mat, m, em->mat);
    if (w->dirAngAbs < PI / 2) {
        a = w->spdXZ * 0.005f;
        if (a > 1.0f) {
            a = 1.0f;
        }
        a *= -0.19634955f;
        w->pitch = w->pitch * 0.9f + a * 0.1f;
    } else {
        w->pitch *= 0.9f;
    }
    a = w->pitch;
    w->pitchPhase += fRand0_1() * 0.3926991f + 0.09817477f;
    PSMTXRotRad(m, 'x', sinf(w->pitchPhase) * 0.012271847f + a);
    PSMTXConcat(em->mat, m, em->mat);
    sway.x = w->swayAmp.x * sinf(w->swayPhase.x);
    sway.y = 0.0f;
    sway.z = w->swayAmp.z * sinf(w->swayPhase.z);
    PSVECScale(&w->swayAmp, &w->swayAmp, 0.96f);
    w->swayPhase.x += 0.31415927f;
    w->swayPhase.z += 0.34906587f;
    RotMatrix(m, &sway);
    PSMTXConcat(em->mat, m, em->mat);
}

static Camera pl0e_camera = { 0 };
static Vec pl0e_cam_ofs = { 0.0f, 0.0f, 5000.0f };
static f32 pl0e_cam_up = 1500.0f;
static f32 pl0e_cam_dist = 5000.0f;
static Vec pl0e_cam_pos0 = { -1500.0f, 0.0f, -3000.0f };
static Vec pl0e_cam_pos1 = { -1500.0f, 0.0f, -5000.0f };

void pl0eCamMove(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);
    Camera* gcam = &pG->Cam;
    Mtx m;
    Vec at;
    Vec target;
    Vec dir;

    if (pPL->flags_420 & 4) {
        return;
    }
    PSMTXRotRad(m, 'y', em->rot.y);
    TransMatrix(m, &em->pos);
    PSMTXMultVec(m, &pl0e_cam_ofs, &target);
    pl0ePathGetTarget(em, &target);
    at = em->pos;
    PSVECSubtract(&at, &target, &dir);
#line 1028
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, pl0e_cam_dist);
    PSVECAdd(&em->pos, &dir, &at);
    if (w->spd > pl0e_spd_max * 0.8f + pl0e_spd_boost * 0.2f) {
        w->camRate = w->camRate * 0.9f + 0.1f;
    } else {
        w->camRate = w->camRate * 0.9f;
    }
    PosToPos(&pl0e_cam_pos0, &pl0e_cam_pos1, &dir, w->camRate);
    PSMTXMultVec(em->mat, &dir, &dir);
    at.x = at.x * 0.5f + dir.x * 0.5f;
    at.z = at.z * 0.5f + dir.z * 0.5f;
    at.y = at.y + pl0e_cam_up;
    PosToPos(&gcam->param.at, &target, &pl0e_camera.param.at, 1.0f);
    PosToPos(&gcam->param.pos, &at, &pl0e_camera.param.pos, 1.0f);
    pl0e_camera.param.fovy = pl0e_camera.param.fovy * 0.9f + 4.0f;
    PSMTXRotRad(m, 'z', -Muku2(w->rotY, em->rot.y, PI) * 3.0f);
    dir.y = 1.0f;
    dir.x = 0.0f;
    dir.z = 0.0f;
    PSMTXMultVecSR(m, &dir, &pl0e_camera.up);
    {
        Vec* cp = &pl0e_camera.param.pos;
        Vec* ca = &pl0e_camera.param.at;

        pl0e_camera.dist = SQRTF((cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z));
    }
    CameraSetOrientationUp(&pl0e_camera);
    CamCtrl.x250 = (s32) &pl0e_camera;
}

void pl0eRideActEvtCk(cPl0e* em)
{
    u8 unused[6];   // the original frame has 8 unused bytes (a BLKmode local nothing references)

    if (!(pG->flags_5010 & 0x00200000)) {
        if (!(fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, PI)) > PI / 4)) {
            fabsf(em->pos.y - pPL->pos.y);
        }
    }
}

void cPl0e::setRide()
{
    Pl0eWork* w = PL0E_WK(this);
    cPlayer* pl = pPL;

    if (w->pRailObj) {
        xFC = 1;
        xFD = 1;
        xFE = 0;
        xFF = 0;
        // Reference store: the pPL reload of PlRoutineSet then depends on it (cost 2) and is not
        // ready when the BoatMoveFunc store is, so sched1 issues that store first and the
        // PlBoatMove address dies before the reload is born (both r9; the zero takes r10).
        PSet((void*&) pl->pBoat, this);
        BoatMoveFunc = PlBoatMove;
        PlRoutineSet(pPL, 0, 0xF, 0, 0);
        if (pSUB) {
            SetSubDamage((int) this, (void*) subBoatRide);
        }
    }
}

void pl0eScrAdjust(cPl0e* em)
{
    Vec nrm;
    Vec p;
    Vec d;

    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    p = em->pos;
    if (pG->debug_mode == 7) {
        Draw_sphere(&em->pos, 300.0f, 0xFFFFFFFF, 1, 1);
    }
    SatMgr.adjust(&nrm, &em->oldPos, &p, 600.0f, 0x2081, 0);
    if (!(nrm.x == 0.0f && nrm.y == 0.0f && nrm.z == 0.0f)) {
        PSVECSubtract(&p, &em->pos, &d);
        d.y = 0.0f;
        if (!(d.x == 0.0f && d.y == 0.0f && d.z == 0.0f)) {
            PSVECAdd(&em->pos, &d, &em->pos);
        }
    }
}

static PlBoatFunc plboat_R2_move_tbl[7] = {
    plboat_R2_Ride,
    plboat_R2_Move,
    plboat_R2_Jump,
    plboat_R2_Landing,
    plboat_R2_Crash,
    plboat_R2_Sink,
    plboat_R2_JumpMiss,
};

// The player's boat routine (pl_R1_Boat -> BoatMoveFunc): the boat's motion archive replaces the
// player's for the duration of the routine.
static void PlBoatMove(cPlayer* pl)
{
    if (pl->pBoat == 0) {
        pLog->err(0, 0, "PlBoatMove(): m_pBoat == NULL!");
        return;
    }
    SubScreenWait(0xF);
    pG->flags_5010 |= 0x00200000;
    PlSetNeck(2);
    pl->atari.flags &= 0xFCFF;
    pl->dmType = 0x1E;
    pl->subArc = pl->pBoat->subArc;
    pl->motFlags2 &= ~0x40000000;
    pl->neckMot.flags2 &= ~0x40000000;
    plboat_R2_move_tbl[pl->xFE](pl);
    pl->motFlags2 &= ~0x40000000;
    pl->neckMot.flags2 &= ~0x40000000;
    pl->subArc = pl->subArc2;
}

static void plboat_R2_Ride(cPlayer* pl)
{
    switch (pl->xFF) {
    case 0:
        pl->pos.x = 0.0f;
        pl->pos.y = 0.0f;
        pl->pos.z = 0.0f;
        pl->rot.x = 0.0f;
        pl->rot.y = 0.0f;
        pl->rot.z = 0.0f;
        MotionSetCore(pl, &pl->mot, PLARC(0x1D), 0, 0, 0x201, 0);
        pl->blendRate500 = 0.0f;
        {
            cAtariInfo* at = &pl->atari;
            at->flags &= 0xFCFF;
        }
        pl->be_flag |= 0x00200000;
        pl->pBody->initWepHand((u32) PLARC(0x7));
        pl->setRightHand(1);
        pl->pBody->initWepHand((u32) PL_ARC_PTR(pG->pPlArc, 0x15));
        pl->setLeftHand(1);
        pl->pWep->setTrans(0, 0);
        pl->xFF++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            PlRoutineSet(pPL, 0, 0xF, 1, 0);
        }
        break;
    }
}

static void plboat_R2_Move(cPlayer* pl)
{
    Pl0eWork* w = PL0E_WK(PL_BOAT(pl));

    switch (pl->xFF) {
    case 0:
        pl->blendRate500 = 0.0f;
        pl->x4FD = 0;
        pl->x4FC = 0;
        pl->xFF++;
    case 1:
        plboatBlendMotSet(pl, PLARC(0x16), PLARC(0x18), PLARC(0x17), 0, 0, 0);
        pl->blendRate500 = w->blendRate;
        pl->x4FC = (u8) w->frameOld;
        plOnJet(pl);
        MotionMoveF(pl, 0);
        break;
    }
    if (pl->pBoat) {
        pl0eCamMove(PL_BOAT(pl));
    }
}

static void plboat_R2_Jump(cPlayer* pl)
{
    switch (pl->xFF) {
    case 0:
        switch ((int) pl->x3E0) {
        case 0:
        default:
            MotionSetCore(pl, &pl->mot, PLARC(0x19), 0, 0xA, 1, 0);
            break;
        case 1:
            MotionSetCore(pl, &pl->mot, PLARC(0x1C), 0, 0xA, 1, 0);
            break;
        case 2:
            MotionSetCore(pl, &pl->mot, PLARC(0x23), 0, 0xA, 1, 0);
            break;
        }
        pl->xFF++;
    case 1:
        plOnJet(pl);
        MotionMoveF(pl, 0);
        break;
    }
    if (pl->pBoat) {
        pl0eCamMove(PL_BOAT(pl));
    }
}

static void plboat_R2_Landing(cPlayer* pl)
{
    Pl0eWork* w = PL0E_WK(PL_BOAT(pl));

    switch (pl->xFF) {
    case 0:
        pl->x4FD = 0xA;
        pl->x4FC = 0;
        pl->blendRate500 = 0.0f;
        pl->xFF++;
    case 1:
        plboatBlendMotSet(pl, PLARC(0x1A), PLARC(0x1F), PLARC(0x1E), 0, 0, 0);
        pl->blendRate500 = w->blendRate;
        pl->x4FC = (u8) w->frameOld;
        plOnJet(pl);
        if (MotionMoveF(pl, 0)) {
            PlRoutineSet(pPL, 0, 0xF, 1, 0);
        }
        break;
    }
    if (pl->pBoat) {
        pl0eCamMove(PL_BOAT(pl));
    }
}

static void plboat_R2_Crash(cPlayer* pl)
{
    Pl0eWork* w = PL0E_WK(PL_BOAT(pl));

    switch (pl->xFF) {
    case 0:
        if (w->ofs.x > 0.0f) {
            MotionSetCore(pl, &pl->mot, PLARC(0x1B), 0, 3, 1, 0);
        } else {
            MotionSetCore(pl, &pl->mot, PLARC(0x20), 0, 3, 1, 0);
        }
        plOnJet(pl);
        pG->pl_life = 0;
        pl->xFF++;
    case 1:
        MotionMoveF(pl, 0);
        break;
    }
}

static void plboat_R2_Sink(cPlayer* pl)
{
    switch (pl->xFF) {
    case 0:
        pl->pos.x = 0.0f;
        pl->pos.y = 0.0f;
        pl->pos.z = 0.0f;
        pl->rot.x = 0.0f;
        pl->rot.y = 0.0f;
        pl->rot.z = 0.0f;
        MotionSetCore(pl, &pl->mot, PLARC(0x21), 0, 0, 0x201, 0);
        pl->blendRate500 = 0.0f;
        {
            cAtariInfo* at = &pl->atari;
            at->flags &= 0xFCFF;
        }
        pGS->pl_life = 0;
        pl->xFF++;
    case 1:
        MotionMoveF(pl, 0);
        break;
    }
}

static void plboat_R2_JumpMiss(cPlayer* pl)
{
    switch (pl->xFF) {
    case 0:
        pl->pos.x = 0.0f;
        pl->pos.y = 0.0f;
        pl->pos.z = 0.0f;
        pl->rot.x = 0.0f;
        pl->rot.y = 0.0f;
        pl->rot.z = 0.0f;
        MotionSetCore(pl, &pl->mot, PLARC(0x22), 0, 0, 0x201, 0);
        pl->blendRate500 = 0.0f;
        {
            cAtariInfo* at = &pl->atari;
            at->flags &= 0xFCFF;
        }
        pGS->pl_life = 0;
        pl->xFF++;
    case 1:
        MotionMoveF(pl, 0);
        break;
    }
}

// Lean blend of the rider: m0 straight, m1 left / m2 right by the sign of the blend rate.
void plboatBlendMotSet(cPlayer* pl, void* m0, void* m1, void* m2, int a, int b, int c)
{
    f32 rate = fabsf(pl->blendRate500);
    MotionWorkSub* bm;
    void* m;
    int f;

    MotionSetCore(pl, &pl->mot, m0, a, pl->x4FD, 4, pl->x4FC);
    if (pl->blendRate500 < 0.0f) {
        m = m1;
        f = b;
    } else {
        m = m2;
        f = c;
    }
    bm = &pl->neckMot;
    MotionSetCore(pl, bm, m, f, pl->x4FD, 4, pl->x4FC);
    pl->blendMot = bm;
    bm->blendRate = rate * (1.0f / 256.0f);
    if (pl->x4FD) {
        pl->x4FD--;
    }
    pl->x4FC++;
    if (pl->x4FC >= pl->frameMax) {
        pl->x4FC = 0;
    }
}

void subBlendMotSet(cSubChar* sub, void* m0, void* m1, void* m2, int a, int b, int c)
{
    f32 rate = fabsf(sub->subBlendRate);
    MotionWorkSub* bm;
    void* m;
    int f;

    MotionSetCore(sub, &sub->mot, m0, a, sub->sub409, 4, sub->sub408);
    if (sub->subBlendRate < 0.0f) {
        m = m1;
        f = b;
    } else {
        m = m2;
        f = c;
    }
    bm = &sub->subBackMot;
    MotionSetCore(sub, bm, m, f, sub->sub409, 4, sub->sub408);
    sub->blendMot = bm;
    bm->blendRate = rate * (1.0f / 256.0f);
    if (sub->sub409) {
        sub->sub409--;
    }
    sub->sub408++;
    if (sub->sub408 >= sub->frameMax) {
        sub->sub408 = 0;
    }
}

// Seats the player on the ski: position from the ski's matrix, the matrix and rotation copied.
void plOnJet(cPlayer* pl)
{
    Vec v;

    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(pl->pBoat->mat, &v, &pl->pos);
    PSMTXCopy(pl->pBoat->mat, pl->mat);
    pl->rot = pl->pBoat->rot;
    pl->motFlags2 |= 0x40000000;
    pl->neckMot.flags2 |= 0x40000000;
}

static void subBoatRide()
{
    cSubChar* sub = pSUB;
    cPl0e* boat = SUB_BOAT(sub);

    sub->subArc = boat->subArc;
    sub->dmType = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->xFE) {
    case 0:
        sub->pos.x = 0.0f;
        sub->pos.y = 0.0f;
        sub->pos.z = 0.0f;
        sub->rot.x = 0.0f;
        sub->rot.y = 0.0f;
        sub->rot.z = 0.0f;
        MotionSetCore(sub, &sub->mot, SUBARC(0x2C), 0, 0, 1, 0);
        sub->be_flag |= 0x00200000;
        {
            cAtariInfo* at = &sub->atari;
            at->flags &= 0xFCFF;
        }
        sub->xFE++;
    case 1:
        if (MotionMoveF(sub, 0)) {
            SetSubDamage((int) boat, (void*) subBoatRun);
        } else {
            if (sub->frame > 21.7f && sub->frame < 22.3f) {
                SndCall(5, 0x16, &sub->pos, 0, 0, 0);
            }
            if (sub->frame > 22.7f && sub->frame < 23.3f) {
                SndCall(5, 0x17, &sub->pos, 0, 0, 0);
            }
        }
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

static void subBoatRun()
{
    cSubChar* sub = pSUB;
    cPl0e* boat = SUB_BOAT(sub);
    Pl0eWork* w = PL0E_WK(boat);

    sub->subArc = boat->subArc;
    sub->dmType = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->xFE) {
    case 0:
        sub->atari.flags &= 0xFCFF;
        sub->subBlendRate = 0.0f;
        sub->sub409 = 0;
        sub->sub408 = 0;
        sub->xFE++;
    case 1:
        sub->subBlendRate = w->blendRate;
        sub->sub408 = (u8) w->frameOld;
        subOnJet(sub, boat);
        subBlendMotSet(sub, SUBARC(0x25), SUBARC(0x27), SUBARC(0x26), 0, 0, 0);
        MotionMoveF(sub, 0);
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

static void subBoatJump()
{
    cSubChar* sub = pSUB;
    cPl0e* boat = SUB_BOAT(sub);

    sub->subArc = boat->subArc;
    sub->dmType = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->xFE) {
    case 0:
        switch (sub->xFF) {
        case 0:
        default:
            MotionSetCore(sub, &sub->mot, SUBARC(0x28), 0, 0xA, 1, 0);
            break;
        case 1:
            MotionSetCore(sub, &sub->mot, SUBARC(0x2B), 0, 0xA, 1, 0);
            break;
        case 2:
            MotionSetCore(sub, &sub->mot, SUBARC(0x31), 0, 0xA, 1, 0);
            break;
        }
        sub->atari.flags &= 0xFCFF;
        sub->xFE++;
    case 1:
        subOnJet(sub, boat);
        MotionMoveF(sub, 0);
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

static void subBoatLanding()
{
    cSubChar* sub = pSUB;
    cPl0e* boat = SUB_BOAT(sub);
    Pl0eWork* w = PL0E_WK(boat);

    sub->subArc = boat->subArc;
    sub->dmType = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->xFE) {
    case 0:
        sub->atari.flags &= 0xFCFF;
        sub->sub409 = 0xA;
        sub->sub408 = 0;
        sub->subBlendRate = 0.0f;
        sub->xFE++;
    case 1:
        sub->subBlendRate = w->blendRate;
        sub->sub408 = (u8) w->frameOld;
        subOnJet(sub, boat);
        subBlendMotSet(sub, SUBARC(0x29), SUBARC(0x2E), SUBARC(0x2D), 0, 0, 0);
        if (MotionMoveF(sub, 0)) {
            SetSubDamage((int) boat, (void*) subBoatRun);
        }
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

static void subBoatCrash()
{
    cSubChar* sub = pSUB;
    cPl0e* boat = SUB_BOAT(sub);

    sub->subArc = boat->subArc;
    sub->dmType = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->xFE) {
    case 0:
        MotionSetCore(sub, &sub->mot, SUBARC(0x2A), 0, 3, 1, 0);
        sub->atari.flags &= 0xFCFF;
        sub->xFE++;
    case 1:
        subOnJet(sub, boat);
        MotionMoveF(sub, 0);
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

static void subBoatSink()
{
    cSubChar* sub = pSUB;
    cPl0e* boat = SUB_BOAT(sub);

    sub->subArc = boat->subArc;
    sub->dmType = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->xFE) {
    case 0:
        sub->pos.x = 0.0f;
        sub->pos.y = 0.0f;
        sub->pos.z = 0.0f;
        sub->rot.x = 0.0f;
        sub->rot.y = 0.0f;
        sub->rot.z = 0.0f;
        MotionSetCore(sub, &sub->mot, SUBARC(0x2F), 0, 0, 1, 0);
        sub->xFE++;
    case 1:
        MotionMoveF(sub, 0);
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

static void subBoatJumpMiss()
{
    cSubChar* sub = pSUB;
    cPl0e* boat = SUB_BOAT(sub);

    sub->subArc = boat->subArc;
    sub->dmType = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->xFE) {
    case 0:
        sub->pos.x = 0.0f;
        sub->pos.y = 0.0f;
        sub->pos.z = 0.0f;
        sub->rot.x = 0.0f;
        sub->rot.y = 0.0f;
        sub->rot.z = 0.0f;
        MotionSetCore(sub, &sub->mot, SUBARC(0x30), 0, 0, 1, 0);
        sub->xFE++;
    case 1:
        MotionMoveF(sub, 0);
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

void subOnJet(cSubChar* sub, cPl0e* boat)
{
    Vec v;

    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(boat->mat, &v, &sub->pos);
    PSMTXCopy(boat->mat, sub->mat);
    TransMatrix(sub->mat, &sub->pos);
    sub->rot = boat->rot;
    sub->motFlags2 |= 0x40000000;
}

void cPl0e::setRail(void* path)
{
    Pl0eWork* w = PL0E_WK(this);

    w->pRailObj = SetObj00((void*) (pG->pArc->ofs_20 + (u32) pG->pArc), (void*) (pG->pArc->ofs_24 + (u32) pG->pArc), 0, 0);
    if (w->pRailObj) {
        w->seg = 0;
        w->dist = 0.0f;
        w->pPath = path;
        w->spd = FRef(pl0e_spd_max);
        w->length = PathGetLength(path);
        w->pathPos = pos;
        w->pathPosOld = pos;
    }
}

void pl0ePathMove(cPl0e* em, int jump)
{
    Pl0eWork* w = PL0E_WK(em);
    Mtx m;
    Vec p;
    Vec a;
    Vec b;
    Vec hit;
    Mtx inv;
    Vec d;
    f32 ang;

    if (w->pRailObj == 0) {
        return;
    }
    w->pathPosOld = w->pathPos;
    PathGetPosEmM(w->pPath, w->pRailObj, w->dist, &w->seg, &w->pathPos);
    PSMTXRotRad(m, 'y', em->rot.y);
    TransMatrix(m, &w->pathPos);
    if ((Key.on & 0xC) && jump == 0) {
        if (Key.on & 0x8) {
            w->spdX += 80.0f;
            if (w->spdX > 400.0f) {
                w->spdX = 400.0f;
            }
        } else {
            w->spdX -= 50.0f;
            if (w->spdX < -400.0f) {
                w->spdX = -400.0f;
            }
        }
    } else {
        w->spdX *= 0.87f;
    }
    w->ofs.x += w->spdX;
    PSMTXMultVec(m, &w->ofs, &p);
    em->pos.x = p.x;
    em->pos.z = p.z;
    if ((w->pathPos.x - p.x) * (w->pathPos.x - p.x) + (w->pathPos.z - p.z) * (w->pathPos.z - p.z) > 10000.0f) {
        PSVECSubtract(&em->pos, &w->pathPos, &d);
        d.y = 0.0f;
#line 2209
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, 500.0f);
        a = w->pathPos;
        b = em->pos;
        a.y = em->pos.y;
        PSVECAdd(&b, &d, &b);
        if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0x80800)) {
            PSVECSubtract(&a, &b, &d);
#line 2221
            VECNormalize(&d, &d);
            PSVECScale(&d, &d, 500.0f);
            em->pos.x = hit.x + d.x;
            em->pos.z = hit.z + d.z;
            PSMTXInverse(m, inv);
            PSMTXMultVec(inv, &em->pos, &w->ofs);
            w->ofs.y = 0.0f;
            w->ofs.z = 0.0f;
            w->spdX = 0.0f;
        }
    }
    w->dist += w->spd;
    if (jump != 0) {
        if (w->jumpCnt <= 1) {
            SPD_ADJUST(w->spd, FRef(pl0e_spd_max));
        }
    } else {
        if (Key.on & 3) {
            if (Key.on & 1) {
                w->spd += 25.0f;
                if (w->spd > FRef(pl0e_spd_boost)) {
                    w->spd = FRef(pl0e_spd_boost);
                }
                w->pitch104 += 4;
                if (w->pitch104 > 500) {
                    w->pitch104 = 500;
                }
            } else {
                SPD_ADJUST(w->spd, FRef(pl0e_spd_slow));
                if (w->pitch104 > 4) {
                    w->pitch104 -= 4;
                } else {
                    w->pitch104 = 0;
                }
            }
        } else {
            SPD_ADJUST(w->spd, FRef(pl0e_spd_max));
            if (w->pitch104 > 4) {
                w->pitch104 -= 4;
            } else {
                w->pitch104 = 0;
            }
        }
    }
    if (w->dist >= w->length) {
        w->dist = w->length;
    }
    p.x = 0.0f;
    p.y = 0.0f;
    p.z = 15000.0f;
    PSMTXMultVec(m, &p, &p);
    pl0ePathGetTarget(em, &p);
    ang = GetXZAngle(&w->pathPos, &p);
    em->rot.y += Muku2(em->rot.y, ang, 0.09817477f);
    em->rot.y = LIMIT_ANGLE(em->rot.y);
    if (!(w->flags & 1) && (em->be_flag & 2)) {
        if (w->spd > 100.0f) {
            EstSet((int) em, -1, 0, 0, 0xE, 0, 0, 0, (u32) em, 0);
        }
        if (w->spd > pl0e_spd_max + 100.0f) {
            EstSet((int) em, -1, 0, 0, 0xE, 3, 0, 0, (u32) em, 0);
        }
        if (Key.trg & 8) {
            EstSet((int) em, -1, 0, 0, 0xE, 1, 0, 0, (u32) em, 0);
            SndCall(8, 0xC, &em->pos, em->id, 0, em);
        }
        if (Key.trg & 4) {
            EstSet((int) em, -1, 0, 0, 0xE, 2, 0, 0, (u32) em, 0);
            SndCall(8, 0xC, &em->pos, em->id, 0, em);
        }
    }
    if (w->flags & 2) {
        w->sink -= pl0e_spd_boost - w->spd;
        if (w->sink <= 0.0f) {
            w->sink = 0.0f;
        }
    }
    if (!(w->flags & 1) && (s16) pG->pl_life > 0) {
        static u8 cnt = 0;

        cnt++;
        if (cnt % 20 == 0) {
            SndCall(8, 0x12, &em->pos, em->id, 0, em);
        }
    }
    SndSetDopPitch(w->seNo, (s16) w->pitch104);
}

void cPl0e::set2ndRail()
{
    Pl0eWork* w = PL0E_WK(this);

    pPL->pBoat = this;
    w->flags |= 2;
    SndStop(w->seNo, 0);
    w->seNo = SndCall(8, 0xA, &pos, id, 0, this);
    w->sink = 96000.0f;
    w->dist += 50000.0f;
    w->ofs.x = 0.0f;
    w->ofs.y = 0.0f;
    w->ofs.z = 0.0f;
    w->spdX = 0.0f;
    xFC = 1;
    xFD = 2;
    xFE = 0;
    xFF = 0;
    BoatMoveFunc = PlBoatMove;
    PlRoutineSet(pPL, 0, 0xF, 1, 0);
    if (pSUB) {
        SetSubDamage((int) this, (void*) subBoatRun);
    }
    if (w->pWave) {
        EstSet((int) w->pWave, -1, 0, 0, 0xE, 9, 1, 0, (u32) w->pWave, 0);
    }
}

void cPl0e::stopEngine()
{
    SndStop(PL0E_WK(this)->seNo, 0);
}

// Path position 15000 ahead of the ski (x / z only), when it is still on the path.
void pl0ePathGetTarget(cPl0e* em, Vec* out)
{
    Pl0eWork* w = PL0E_WK(em);
    Vec p;
    u16 seg[1];
    f32 d;

    if (w->pRailObj) {
        d = w->dist + 15000.0f;
        if (!(d >= w->length)) {
            seg[0] = 0;
            PathGetPosEm(w->pPath, d, w->pRailObj, seg, &p);
            out->x = p.x;
            out->z = p.z;
        }
    }
}

// Gravity plus the water surface under the four corners: returns 0 while the ski sits on the water.
int pl0eSlopeControl(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);
    Mtx m;
    Vec v;
    f32 fl[4];
    f32 y;

    PSMTXRotRad(m, 'y', em->rot.y);
    TransMatrix(m, &em->pos);
    w->spdY -= 15.0f;
    em->pos.y += w->spdY;
    v.x = 200.0f;
    v.y = 0.0f;
    v.z = 1500.0f;
    PSMTXMultVec(m, &v, &v);
    fl[0] = SatMgr.getFloor(&v, 10000.0f, 100000.0f, 0, 0);
    v.x = -200.0f;
    v.y = 0.0f;
    v.z = 1500.0f;
    PSMTXMultVec(m, &v, &v);
    fl[1] = SatMgr.getFloor(&v, 10000.0f, 100000.0f, 0, 0);
    v.x = 200.0f;
    v.y = 0.0f;
    v.z = -1500.0f;
    PSMTXMultVec(m, &v, &v);
    fl[2] = SatMgr.getFloor(&v, 10000.0f, 100000.0f, 0, 0);
    v.x = -200.0f;
    v.y = 0.0f;
    v.z = -1500.0f;
    PSMTXMultVec(m, &v, &v);
    fl[3] = SatMgr.getFloor(&v, 10000.0f, 100000.0f, 0, 0);
    if (fl[0] < fl[1]) {
        fl[0] = fl[1];
    }
    if (fl[2] < fl[3]) {
        fl[2] = fl[3];
    }
    y = (fl[0] + fl[2]) * 0.5f;
    if (em->pos.y < y) {
        w->spdY = 0.0f;
        em->pos.y = y;
        return 0;
    }
    return 1;
}

void pl0eBlendMotSet(cPl0e* em, void* m0, void* m1, void* m2, int a, int b, int c)
{
    Pl0eWork* w = PL0E_WK(em);
    f32 rate = fabsf(w->blendRate);
    MotionWorkSub* bm;
    void* m;
    int f;

    MotionSetCore(em, &em->mot, m0, a, (u8) w->hokan, 4, (u16) w->frame);
    if (w->blendRate < 0.0f) {
        m = m1;
        f = b;
    } else {
        m = m2;
        f = c;
    }
    bm = &w->blendMot;
    MotionSetCore(em, bm, m, f, (u8) w->hokan, 4, (u16) w->frame);
    em->blendMot = bm;
    bm->blendRate = rate * (1.0f / 256.0f);
    if (w->hokan) {
        w->hokan--;
    }
    w->frame++;
    if (w->frame >= em->frameMax) {
        w->frame = 0;
    }
}

// Jump ramp under the ski (scenario attribute bit 19).
int pl0eJumpCk(cPl0e* em)
{
    Vec a;
    Vec b;

    if ((s16) pG->pl_life > 0) {
        a = em->pos;
        b = em->pos;
        a.y += 1000.0f;
        b.y -= 10000.0f;
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x80000) {
            return 1;
        }
    }
    return 0;
}

// Wall 3500 ahead of the ski (attribute bit 11) at the right, left and centre.
int pl0eCrashCk(cPl0e* em)
{
    Vec a;
    Vec b;

    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    a.x = 400.0f;
    a.y = 1000.0f;
    a.z = 0.0f;
    b.x = 400.0f;
    b.y = 1000.0f;
    b.z = 3500.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x800) {
        return 1;
    }
    a.x = -400.0f;
    a.y = 1000.0f;
    a.z = 0.0f;
    b.x = -400.0f;
    b.y = 1000.0f;
    b.z = 3500.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x800) {
        return 1;
    }
    a.x = 0.0f;
    a.y = 1000.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 1000.0f;
    b.z = 3500.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x800) {
        return 1;
    }
    return 0;
}

int pl0eSinkCk(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);

    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    return w->sink <= 0.0f;
}

// Fall-off area under the ski (attribute bit 13).
int pl0eJumpMissCk(cPl0e* em)
{
    Vec a;
    Vec b;

    if ((s16) pG->pl_life > 0) {
        a = em->pos;
        b = em->pos;
        a.y += 1000.0f;
        b.y -= 10000.0f;
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x2000) {
            return 1;
        }
    }
    return 0;
}

// The wave object follows the ski, sinking with it (4000 .. 8000 below by the sink value).
void pl0eWaveMove(cPl0e* em)
{
    Pl0eWork* w = PL0E_WK(em);
    Vec v;
    f32 y;

    if (w->pWave) {
        y = w->sink / 96000.0f * 4000.0f + 4000.0f;
        if (y < 4000.0f) {
            y = 4000.0f;
        }
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -y;
        PSMTXMultVec(em->mat, &v, &v);
        w->pWave->pos = v;
        w->pWave->rot = em->rot;
    }
}
