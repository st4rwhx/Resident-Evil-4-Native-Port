// pl0f module (D:/Bio4/Prog/pl0f.cpp): the lake boat of the Del Lago fight. A cEm placed by the room
// script; two point masses (bow / stern) on the water carry the hull (pl0fBoatControl), the player
// rides and steers it (PlBoatMove / plboat_R2_*, setTiller), the boss drags it by the anchor rope
// (pl0fBoatChaseBoss) and the player throws the harpoons (plboat_R2_SpearSet / SpearThrow).

#include "atari.h"
#include "light.h"
#include "pl0f.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_body.h"
#include "pl_wep.h"
#include "pl_cloth.h"
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
#include "em_sub.h"
#include "act_btn.h"

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);              // game/em.cpp
extern void (*BoatMoveFunc)(cPlayer* pl);        // game/player.cpp (pl_R1_Boat calls it)
extern "C" void Em_R0_Scenario(cEm* em);         // game/em_sub.cpp
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");   // MotionMove called with a second argument (pl_npc.cpp)

// The module's 0x30-byte COMMON block: uninitialised template statics of the original object,
// appended to .bss by snmakerel.
asm(".comm common_pl0f,48,4");

#line 1 "D:/Bio4/Prog/pl0f.cpp"

typedef void (*Pl0fFunc)(cPl0f*);
typedef void (*PlBoatFunc)(cPlayer*);

static void pl0f_R0_Init(cPl0f* em);
static void pl0f_R0_Move(cPl0f* em);
static void pl0f_R1_Wait(cPl0f* em);
static void pl0f_R1_RideMove(cPl0f* em);
static void pl0f_R1_RideStart(cPl0f* em);
static void pl0f_R1_BossMove(cPl0f* em);
static void pl0f_R1_Guard(cPl0f* em);
static void pl0f_R1_Drop(cPl0f* em);
static void pl0f_R1_WaterRide(cPl0f* em);
static void pl0f_R1_BossGuard(cPl0f* em);
static void pl0f_R1_R10dIn(cPl0f* em);
static void pl0f_R1_R10dOut(cPl0f* em);
static void pl0f_R1_R10eIn(cPl0f* em);
static void pl0f_R1_R10eOut(cPl0f* em);
static void pl0f_R1_R10eIn2(cPl0f* em);
static void pl0f_R1_R10eOut2(cPl0f* em);
static void pl0fActRide(cPl0f* em);
static void pl0fActRideR10d(cPl0f* em);
static void pl0fActRideR10e(cPl0f* em);
static void pl0fActRideR10e2(cPl0f* em);
static void pl0fActGetOff(cPl0f* em);
static void PlBoatMove(cPlayer* pl);
static void plboat_R2_Ride(cPlayer* pl);
static void plboat_R2_Getoff(cPlayer* pl);
static void plboat_R2_Move(cPlayer* pl);
static void plboat_R2_SpearSet(cPlayer* pl);
static void plboat_R2_SpearThrow(cPlayer* pl);
static void plboat_R2_SpearSet2(cPlayer* pl);
static void plboat_R2_SpearThrow2(cPlayer* pl);
static void plboat_R2_BossDie(cPlayer* pl);
static void plboat_R2_Guard(cPlayer* pl);
static void plboat_R2_FallWater(cPlayer* pl);
static void plboat_R2_Swim(cPlayer* pl);
static void plboat_R2_WaterRide(cPlayer* pl);
static void plboat_R2_Die(cPlayer* pl);
static void plboat_R2_R10dIn(cPlayer* pl);
static void plboat_R2_R10dOut(cPlayer* pl);
static void plboat_R2_R10eIn(cPlayer* pl);
static void plboat_R2_R10eOut(cPlayer* pl);
static void plboat_R2_R10eIn2(cPlayer* pl);
static void plboat_R2_R10eOut2(cPlayer* pl);
static void subBoatRide();
static void subBoatGetoff();
static void subBoatR10dIn();
static void subBoatR10eIn();
static void subBoatR10eIn2();

#define ARC(no) PL_ARC_PTR(em->subArc, no)
#define SUBARC(no) PL_ARC_PTR(sub->subArc, no)
#define PLARC(no) PL_ARC_PTR(pl->subArc, no)
#define VIB_TBL ((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc))
#define PL_BOAT(pl) ((cPl0f*) (pl)->pBoat)
#define SUB_BOAT(sub) ((cPl0f*) (sub)->dmgType)
#define ROPE(w) ((cObj*) (w)->pRope)

// The boss (em2f) work as far as the boat reads it.
struct Em2fWorkView {
    u8 pad[0x5C8];
    u8 espKind;   // 0x5C8 (0x9A8)
};

// Store through a reference: a scalar (non-struct) MEM, so a following global load stays below it.
static inline void PSet(void*& d, void* v) { d = v; }
static inline f32 FRef(f32& v) { return v; }
static inline void U32Set(u32& d, u32 v) { d = v; }
static inline void IntSet(int& d, int v) { d = v; }
static inline void U8Set(u8& d, int v) { d = v; }
static inline void EmSet(cEm*& d, cEm* v) { d = v; }

struct SubCharPtr {
    cSubChar* p;
};
#define pSUBS (((SubCharPtr*) &pSUB)->p)

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

static Pl0fFunc Pl0f_R0_move_tbl[5] = {
    pl0f_R0_Init,
    pl0f_R0_Move,
    0,
    0,
    (Pl0fFunc) Em_R0_Scenario,
};

static Pl0fFunc Pl0f_R1_move_tbl[14] = {
    pl0f_R1_Wait,
    pl0f_R1_RideMove,
    pl0f_R1_RideStart,
    pl0f_R1_Guard,
    pl0f_R1_Drop,
    pl0f_R1_WaterRide,
    pl0f_R1_BossMove,
    pl0f_R1_BossGuard,
    pl0f_R1_R10dIn,
    pl0f_R1_R10dOut,
    pl0f_R1_R10eIn,
    pl0f_R1_R10eOut,
    pl0f_R1_R10eIn2,
    pl0f_R1_R10eOut2,
};

static Camera pl0f_camera = { 0 };
static f32 pl0f_spd_damp = 0.96f;

extern "C" void _prolog()
{
    OSReport("Pl0f prolog Ok\n");
    EmInitFunc = Pl0fInit;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Pl0fInit(cEm* em)
{
    new (em) cPl0f();
}

void cPl0f::move()
{
    Pl0fWork* w = PL0F_WK(this);

    w->flags &= ~0xC;
    Pl0f_R0_move_tbl[xFC](this);
    if (pG->room_id != 0x10D && pG->room_id != 0x10E) {
        if (w->cnt64) {
            w->cnt64--;
        } else {
            w->cnt64 = 0x1D;
            EstSet((int) this, -1, 0, 0, 0xF, 7, 0, 0x35, (u32) this, 0);
        }
    }
    if (w->pRope) {
        if (w->pAnchor) {
            Vec v;

            v.x = 0.0f;
            v.y = 1000.0f;
            v.z = 0.0f;
            PSMTXMultVec(w->pAnchor->mat, &v, &v);
            PenClothFixSet(ROPE(w), &w->cloth, 0x1C, &v);
        } else {
            PenClothFixClear(ROPE(w), &w->cloth, 0x1C);
        }
        if (!(pG->flags_5010 & 0x00080000) && !(w->flags & 8) && w->pBoss) {
            ROPE(w)->be_flag |= 2;
            if (w->pBoss && (w->pBoss->flags_3C8 & 0x100)) {
                ROPE(w)->be_flag &= ~2;
            }
        } else {
            ROPE(w)->be_flag &= ~2;
        }
    }
    if (w->pBoss && (w->pBoss->flags_3C8 & 0x40)) {
        Vec v;
        Mtx m;
        u32 i;

        w->swayAmp.x = PI / 16;
        w->swayAmp.y = 0.0f;
        w->swayAmp.z = PI / 16;
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 100.0f;
        PSMTXRotRad(m, 'y', GetXZAngle(&w->pBoss->pos, &pos));
        PSMTXMultVecSR(m, &v, &v);
        SndCall(8, 0x17, &pos, 0xF, 0, 0);
        for (i = 0; i < 2; i++) {
            w->node[i].spd = v;
        }
    }
}

void cPl0f::setTiller()
{
    Pl0fWork* w = PL0F_WK(this);

    if (Key.on & 1) {
        w->tiller |= 1;
    }
    if (Key.on & 2) {
        w->tiller |= 2;
    }
    if (Key.on & 8) {
        w->tiller |= 4;
    }
    if (Key.on & 4) {
        w->tiller |= 8;
    }
}

void cPl0f::setTillerFront()
{
    Pl0fWork* w = PL0F_WK(this);

    w->tiller |= 1;
}

void cPl0f::setPos(Vec* p, f32 ang)
{
    Pl0fWork* w = PL0F_WK(this);
    u32 i;

    for (i = 0; i < 2; i++) {
        Pl0fNode* n = &w->node[i];

        n->spd.x = 0.0f;
        n->spd.y = 0.0f;
        n->spd.z = 0.0f;
    }
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

static void pl0f_R0_Init(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    u32 i;
    u32 j;

    em->modelInit(ARC(0x5), ARC(0x6));
    em->be_flag &= ~0x10;
    em->x12F = 0;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 4);
    }
    U8Set(em->lockParts, 0);   // SImode zero: not merged with x12F's QImode zero across the init2 call
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(3);
    em->atari.flags &= 0xFCFF;
    em->atari.setPriority(1);
    em->setStatus(1);
    EspDataLoad((u32) ARC(0x4), 0xF, 0);
    w->node[0].pos.x = 0.0f;
    w->node[0].pos.y = 0.0f;
    w->node[0].pos.z = 2500.0f;
    w->node[1].pos.x = 0.0f;
    w->node[1].pos.y = 0.0f;
    w->node[1].pos.z = -1500.0f;
    for (i = 0; i < 2; i++) {
        Pl0fNode* n = &w->node[i];

        n->dist[i] = 0.0f;
        n->maxLen = 25000.0f;
        n->spd.x = 0.0f;
        n->spd.y = 0.0f;
        n->spd.z = 0.0f;
        for (j = i + 1; j < 2; j++) {
            Pl0fNode* m = &w->node[j];
            f32 len;

            len = SQRTF((n->pos.x - m->pos.x) * (n->pos.x - m->pos.x) + (n->pos.y - m->pos.y) * (n->pos.y - m->pos.y) + (n->pos.z - m->pos.z) * (n->pos.z - m->pos.z));
            n->dist[j] = len;
            m->dist[i] = len;
        }
    }
    w->cnt64 = 0x1D;
    w->flags = 0;
    w->roll = 0.0f;
    w->pitch = 0.0f;
    w->rollPhase = 0.0f;
    w->pitchPhase = 0.0f;
    w->x50 = 0.0f;
    w->bossMode = 0;
    w->tiller = 0;
    w->x6D = 0;
    w->anchorEff = 0;
    w->swayAmp.x = 0.0f;
    w->swayAmp.y = 0.0f;
    w->swayAmp.z = 0.0f;
    w->swayPhase.x = 0.0f;
    w->swayPhase.y = 0.0f;
    w->seNo = 0;
    w->swayPhase.z = 0.0f;   // last 0.0 store (dying register): issued first
    if ((int) em->flags_3C8 < 0) {
        LightMgr.createBack(0, 3, 0, 0)->setParent(em);
    }
    w->espKind = EspPullCoreKind();
    if (pG->room_id != 0x10D && pG->room_id != 0x10E) {
        EstSet((int) em, -1, 0, 0, 0xF, 0xF, 0x800, 0, (u32) em, 0);
    }
    pl0fSetAnchor(em);
    pl0fLongRopeSet(em);
    switch (em->type) {
    case 0:
    default:
        switch (em->x38D) {
        case 0:
        default:
            PlRoutineSet(em, 1, 0, 0, 0);
            break;
        case 1:
            PlRoutineSet(em, 1, 2, 0, 0);
            break;
        }
        break;
    case 1:
        PlRoutineSet(em, 1, 8, 0, 0);
        break;
    case 2:
        PlRoutineSet(em, 1, 0xA, 0, 0);
        break;
    case 3:
        em->type = 2;
        PlRoutineSet(em, 1, 0, 0, 0);
        break;
    case 4:
        PlRoutineSet(em, 1, 0xC, 0, 0);
        break;
    case 5:
        em->type = 4;
        PlRoutineSet(em, 1, 0, 0, 0);
        break;
    }
    pl0f_R0_Move(em);
}

static void pl0f_R0_Move(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    if (w->spdXZ > 30.0f) {
        cModel* p = em->getPartsPtr(2);

        p->rot.z += 1.0471976f;
        p->rot.z = LIMIT_ANGLE(p->rot.z);
    }
    Pl0f_R1_move_tbl[em->xFD](em);
    if (w->pBoss) {
        w->hist[w->histIdx] = w->pBoss->pos;
        w->histIdx++;
        if (w->histIdx > 9) {
            w->histIdx = 0;
        }
    }
}

// Engine stop SE: the long one after a minute of running.
static inline void pl0fEngineStop(Pl0fWork* w, Vec* pos)
{
    w->flags &= ~2;
    if (w->engineCnt > 60) {
        SndCall(8, 0xB, pos, 0xF, 0, 0);
    } else {
        SndCall(8, 0x10, pos, 0xF, 0, 0);
    }
}

static void pl0f_R1_Wait(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    if (w->flags & 2) {
        w->flags &= ~2;
        if (w->engineCnt > 60) {
            SndCall(8, 0xB, &em->pos, 0xF, 0, 0);
        } else {
            SndCall(8, 0x10, &em->pos, 0xF, 0, 0);
        }
        w->engineCnt = 0;
    }
    w->bossMode = 0;
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
    pl0fRideActEvtCk(em);
}

// The boss is close in front: the player guards (routine 0/F/7), the boat 1/7.
#define BOSS_NEAR(boss, em) \
    ((boss) && ((boss)->flags_3C8 & 0x10) && ((boss)->pos.x - (em)->pos.x) * ((boss)->pos.x - (em)->pos.x) + ((boss)->pos.z - (em)->pos.z) * ((boss)->pos.z - (em)->pos.z) < 9.0e8f)

static void pl0f_R1_RideMove(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    w->bossMode = 0;
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    ((cPlayer*) em)->checkCtrl();
    em->partsMatCalc();
    em->partsWorldCalc();
    if (BOSS_NEAR(w->pBoss, em)) {
        PlRoutineSet(pPL, 0, 0xF, 7, 0);
        PlRoutineSet(em, 1, 7, 0, 0);
    } else if (pl0fCrashCk(em)) {
        PlRoutineSet(pPL, 0, 0xF, 7, 0);
        PlRoutineSet(em, 1, 3, 0, 0);
    }
}

static void pl0f_R1_RideStart(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    cPlayer* pl = pPL;

    pl->pBody->initWepHand((u32) ARC(0x8));
    pl->setRightHand(1);
    pl->pWep->setTrans(0, 0);
    EmSet(pl->pBoat, em);
    BoatMoveFunc = PlBoatMove;
    PlRoutineSet(pPL, 0, 0xF, 2, 0);
    w->flags |= 1;
    w->seNo = SndCall(8, 0x11, &em->pos, 0xF, 0, 0);
    if (pSUBS) {
        SetSubDamage((int) em, (void*) subBoatRide);
        pSUB->xFE = 2;
    }
    PlRoutineSet(em, 1, 1, 0, 0);
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

static void pl0f_R1_BossMove(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    w->bossMode = 1;
    pl0fBoatChaseBoss(em);
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
    if (w->pBoss && (s16) w->pBoss->hp <= 0) {
        em->xFC = 1;   // plain byte stores: the QImode 1 is the `bossMode = 1` pseudo (r30) kept across the calls
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
        {
            int zero = 0;   // one SImode zero for both stores, separate from the routine's QImode zero

            w->pBoss = (cEm*) zero;
            w->bossMode = zero;
        }
        return;
    }
    switch (em->xFE) {
    case 0:
        w->timer = 30;
        em->xFE++;
    case 1: {
        if (BOSS_NEAR(w->pBoss, em)) {
            PlRoutineSet(pPL, 0, 0xF, 7, 0);
            PlRoutineSet(em, 1, 7, 0, 0);
        } else if (w->timer) {
            w->timer--;
        } else if (w->pBoss && pl0fCrashCk(em)) {
            if (w->spdXZ > 200.0f || (w->pBoss->flags_3C8 & 4)) {
                PlRoutineSet(pPL, 0, 0xF, 5, 0);
                {
                    cPlayer* pl = pPL;   // second pPL load (the byte stores above alias it), kept across setLost

                    if (pl->pSpear) {
                        pl->pSpear->setLost();
                        pl->pSpear = 0;
                    }
                }
                PlRoutineSet(em, 1, 4, 0, 0);
                w->bossMode = 1;
            } else {
                PlRoutineSet(pPL, 0, 0xF, 7, 0);
                PlRoutineSet(em, 1, 7, 0, 0);
            }
        }
        break;
    }
    }
}

static void pl0f_R1_Guard(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x1D), 0, 0, 5, 0);
        SndCall(8, 0x17, &em->pos, 0xF, 0, 0);
        VibSetData(VIB_TBL, 0xB, 1);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            PlRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
    if (BOSS_NEAR(w->pBoss, em)) {
        PlRoutineSet(pPL, 0, 0xF, 7, 0);
        PlRoutineSet(em, 1, 7, 0, 0);
    } else if (pl0fCrashCk(em)) {
        PlRoutineSet(pPL, 0, 0xF, 7, 0);
        PlRoutineSet(em, 1, 3, 0, 0);
    }
}

static void pl0f_R1_Drop(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    w->flags |= 0xC;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x1F), 0, 0, 5, 0);
        EstSet((int) em, -1, 0, 0, 0xF, 0xB, 0, 0x35, (u32) em, 0);
        LifeDownSet2(em, 100, 0, 1);
        em->getPartsPtr(1)->rot.y = 0.0f;
        w->timer = 60;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            PlRoutineSet(em, 1, 0, 0, 0);
        } else if (w->timer) {
            f32 spd;
            u32 i;

            w->timer--;
            spd = 300.0f;
            if ((s16) em->hp <= 699) {
                spd = 350.0f;
            }
            if ((s16) em->hp <= 399) {
                spd = 400.0f;
            }
            if ((s16) em->hp <= 0) {
                spd = 450.0f;
            }
            for (i = 0; i < 2; i++) {
                Vec d;
                f32 s = spd;

                d = w->node[i].spd;
                if (d.x != 0.0f && d.y != 0.0f && d.z != 0.0f) {
                    d.x = 0.0f;
                    d.y = 0.0f;
                    d.z = -1.0f;
                    PSMTXMultVecSR(em->mat, &d, &d);
                }
#line 806
                VECNormalize(&d, &d);
                PSVECScale(&d, &d, s);
                w->node[i].spd = d;
                // Dead test (flow deletes the store, jump2 the branch): its insns keep the loop
                // above loop.c's pass-2 threshold, so the VECNormalize string `lis` stays inside
                // the loop like the target's.
                if (w->timer == 0) {
                    s = 0.0f;
                }
            }
        }
        break;
    }
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

static void pl0f_R1_WaterRide(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    w->flags |= 4;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x22), 0, 0, 5, 0);
        EstSet((int) em, -1, 0, 0, 0xF, 0xC, 0, 0x35, (u32) em, 0);
        SndCall(8, 0x17, &em->pos, 0xF, 0, 0);
        w->timer = 60;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            PlRoutineSet(em, 1, 6, 0, 0);
        }
        break;
    }
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

static void pl0f_R1_BossGuard(cPl0f* em)
{
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x1D), 0, 0, 5, 0);
        SndCall(8, 0x17, &em->pos, 0xF, 0, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            PlRoutineSet(em, 1, 6, 0, 0);
        }
        break;
    }
    pl0fBoatChaseBoss(em);
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// Entrance of rooms 10D / 10E: the boat drives itself in and the player gets off at the landing.
// Macros, not inlines: an inline's constant arguments (the partner routine address, the four f32 room
// coordinates) are expanded at the call's head, i.e. before the switch / the timer2 test.
#define PL0F_ROOM_IN(plRoutine, subFunc, t1, t2) \
{ \
    cPlayer* pl = pPL; \
 \
    switch (em->xFE) { \
    case 0: \
        EmSet(pl->pBoat, em); \
        BoatMoveFunc = PlBoatMove; \
        PlRoutineSet(pPL, 0, 0xF, plRoutine, 0); \
        if (pSUB) { \
            SetSubDamage((int) em, (void*) subFunc); \
        } \
        w->timer = t1; \
        w->timer2 = t2; \
        w->cnt = 0; \
        em->xFE++; \
    case 1: \
        w->cnt++; \
        if (w->cnt & 1) { \
            EstSet((int) em, -1, 0, 0, 1, 0xA, 0, 0x35, (u32) em, 0); \
        } \
        if (w->timer) { \
            w->timer--; \
            w->tiller |= 1; \
        } \
        break; \
    } \
    pl0fBoatSpdControl(em); \
    pl0fBoatControl(em); \
    em->partsMatCalc(); \
    em->partsWorldCalc(); \
}

#define PL0F_ROOM_IN_END(ang, px, py, pz) \
{ \
    if (w->timer2) { \
        w->timer2--; \
    } else { \
        cPlayer* pl = pPL; \
 \
        pl->x400 = ang; \
        pl->evTarget.x = px; \
        pl->evTarget.y = py; \
        pl->evTarget.z = pz; \
        PlRoutineSet(pl, 0, 0xF, 1, 0); \
        PlRoutineSet(em, 1, 0, 0, 0); \
        PL0F_WK(em)->flags &= ~1; \
        SndStop(PL0F_WK(em)->seNo, 0); \
        if (pSUB) { \
            SetSubDamage((int) em, (void*) subBoatGetoff); \
        } \
    } \
}

static void pl0f_R1_R10dIn(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    PL0F_ROOM_IN(0xD, subBoatR10dIn, 45, 75);
    PL0F_ROOM_IN_END(1.47f, -500.0f, -2280.0f, -16870.0f);
}

static void pl0f_R1_R10dOut(cPl0f* em)
{
    PL0F_WK(em)->bossMode = 0;
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

static void pl0f_R1_R10eIn(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    PL0F_ROOM_IN(0xF, subBoatR10eIn, 65, 95);
    PL0F_ROOM_IN_END(1.568879f, 38250.0f, -15000.0f, 52360.0f);
}

static void pl0f_R1_R10eOut(cPl0f* em)
{
    PL0F_WK(em)->bossMode = 0;
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

static void pl0f_R1_R10eIn2(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    PL0F_ROOM_IN(0x11, subBoatR10eIn2, 65, 95);
    PL0F_ROOM_IN_END(1.57f, -46610.0f, -15000.0f, 39280.0f);
}

static void pl0f_R1_R10eOut2(cPl0f* em)
{
    PL0F_WK(em)->bossMode = 0;
    pl0fBoatSpdControl(em);
    pl0fBoatControl(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

// Pulls a node back inside maxLen of its fixPos (x / z only). A macro on the routine's `d`, `n` and
// `len`: the PSVEC* `&d` arguments are one gcse-PRE'd pseudo per block (`mr rX,r23` copies hoisted
// out of the loops) while the member reads stay frame-direct (an inline taking `Vec*` substitutes
// `&d` into every use; called with a pointer local it keeps `4(rP)`/`8(rP)` reads); the squared
// length goes through the routine's `len` (a global pseudo: f12 by global-alloc, not tied to the
// dying `fmuls` result).
#define PL0F_NODE_LIMIT(n, line)                                                                   \
    PSVECSubtract(&(n)->wpos, &(n)->fixPos, &d);                                                   \
    len = d.x * d.x + d.z * d.z;                                                                   \
    if (len > (n)->maxLen * (n)->maxLen) {                                                         \
        if (0.0f == d.x && 0.0f == d.y && 0.0f == d.z) {                                           \
            pLog.p->err(0, 0, "VECNormalize:[%s/%d]", "D:/Bio4/Prog/pl0f.cpp", line);              \
            d.x = d.y = d.z = 0.0f;                                                                \
        } else {                                                                                   \
            PSVECNormalize(&d, &d);                                                                \
        }                                                                                          \
        d.y = 0.0f;                                                                                \
        PSVECScale(&d, &d, (n)->maxLen);                                                           \
        PSVECAdd(&(n)->fixPos, &d, &(n)->wpos);                                                    \
    }

void pl0fBoatControl(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    Mtx m;
    Vec d;
    u32 i;
    u32 k;
    u32 pass;
    f32 len;
    Pl0fNode* n;

    // `i` and `n` are the SAME variables in all three node loops: with `i` set in the first loop,
    // gcse does not PRE the second loop's `i + 1` across the k loop (a fresh `u32 j` gets `r = j + 1`
    // hoisted into the k preheader, so `j` is no biv and `&node[j]` a `mulli`); with `n` first
    // mentioned in the first loop it is not `replaceable` in the second, so at the `if (n->fixed)`
    // jump loop.c marks it `cant_derive` (the biv is `maybe_multiple` behind the k loop's back
    // edge) and `&n->wpos` / `&n->fixPos` / `n->dist` stay displacements off the `mr r31,r25` copy
    // instead of becoming their own stepping pointers.
    PSMTXRotRad(m, 'y', em->rot.y);
    TransMatrix(m, &em->pos);
    for (i = 0; i < 2; i++) {
        n = &w->node[i];
        PSMTXMultVec(m, &n->pos, &n->wpos);
        n->wposOld = n->wpos;
        PSVECAdd(&n->wpos, &n->spd, &n->wpos);
        if (n->fixed) {
            PL0F_NODE_LIMIT(n, 1205);
        }
    }
    for (pass = 0; pass < 4; pass++) {
        for (i = 0; i < 2; i++) {
            n = &w->node[i];
            if (n->fixed) {
                PL0F_NODE_LIMIT(n, 1226);
            }
            for (k = 0; k < 2; k++) {
                if (i != k) {
                    Pl0fNode* o = &w->node[k];
                    f32 rate;

                    PSVECSubtract(&o->wpos, &n->wpos, &d);
                    len = PSVECMag(&d);   // the routine's `len`: f12 (`fmr f12,f1`), not tied to f1
                    rate = (n->dist[k] - len) * 0.5f;   // 0.5 enters the pool before 1.0
                    PSVECScale(&d, &d, (1.0f / len) * rate);   // 1/len is the left operand of the fmuls
                    PSVECAdd(&o->wpos, &d, &o->wpos);
                    PSVECSubtract(&n->wpos, &d, &n->wpos);
                }
            }
        }
    }
    for (i = 0; i < 2; i++) {
        n = &w->node[i];
        n->fixed = 0;
        PSVECSubtract(&n->wpos, &n->wposOld, &n->spd);
        PSVECScale(&n->spd, &n->spd, pl0f_spd_damp);
    }
    pl0fScrAdjust(em);
    PSVECSubtract(&w->node[0].wpos, &w->node[1].wpos, &d);
    em->rot.x = 0.0f;
    em->rot.y = atan2f(d.x, d.z);
    RotMatrix(em->mat, &em->rot);
    PSVECScale(&w->node[0].pos, &d, -1.0f);
    TransMatrix(em->mat, &w->node[0].wpos);
    PSMTXMultVec(em->mat, &d, &d);
    TransMatrix(em->mat, &d);
    em->pos = d;
    pl0fGetBoatDir(em);
    pl0fWaterEff(em);
    pl0fBoatRoll(em);
    if (w->spdXZ > 100.0f) {
        AddWaterPower(&w->node[0].wpos, fRand1_1() * 0.3f);
        AddWaterPower(&w->node[1].wpos, fRand1_1() * 0.3f);
    }
    {
        cPlayer* pl = pPL;
        cModel* p = em->getPartsPtr(1);

        if ((pPL->stat & 0xFFFFFF00) == 0x000F0200) {
            p->rot.y = pl->blendRate500 * (1.0f / 255.0f) * -0.5235988f;
        } else {
            p->rot.y *= 0.9f;
        }
    }
}

void pl0fGetBoatDir(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
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

void pl0fWaterEff(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    static u8 cnt = 0;
    static u8 turn = 0;
    static u8 hideCnt = 0;
    static u8 cnt3 = 0;
    f32 a;

    if (pG->room_id == 0x10D || pG->room_id == 0x10E) {
        return;
    }
    if (w->dirAngAbs > 2.443461f && w->spdXZ > 30.0f && !(pG->flags_51E4 & 3)) {
        EstSet((int) em, -1, 0, 0, 0xF, 0x1D, 0, 0x35, (u32) em, 0);
    }
    if (w->spdXZ < 150.0f) {
        return;
    }
    if (w->flags & 4) {
        return;
    }
    a = fabsf(w->dirAng);
    if (w->dirAngAbs > PI / 16 && a < 2.0943952f) {
        if (turn == 0) {
            turn = 1;
            if (w->dirAng < 0.0f) {
                EstSet((int) em, -1, 0, 0, 0xF, 3, 0, 0x35, (u32) em, 0);
            } else {
                EstSet((int) em, -1, 0, 0, 0xF, 4, 0, 0x35, (u32) em, 0);
            }
            SndCall(8, 0xC, &em->pos, 0xF, 0, 0);
        }
    } else {
        turn = 0;
    }
    cnt++;
    if (cnt & 1) {
        EstSet((int) em, -1, 0, 0, 0xF, 0, 0, 0x35, (u32) em, 0);
    }
    if (a < 2.0943952f) {
        EstSet((int) em, -1, 0, 0, 0xF, 1, 0, 0x35, (u32) em, 0);
        EstSet((int) em, -1, 0, 0, 0xF, 2, 0, 0x35, (u32) em, 0);
    }
    cnt3++;
    if (cnt3 % 20 == 0) {
        EstSet((int) em, -1, 0, 0, 0xF, 9, 0, 0x35, (u32) em, 0);
        SndCall(8, 0x12, &em->pos, 0xF, 0, 0);
    }
    if (pG->flags_5010 & 0x00800000) {
        if (hideCnt <= 4) {
            hideCnt++;
            return;
        }
        EstSet((int) em, -1, 0, 0, 0xF, 5, 0, 0x35, (u32) em, 0);
    } else {
        if (hideCnt != 0) {
            EstSet((int) em, -1, 0, 0, 0xF, 8, 0, 0x35, (u32) em, 0);
        }
        hideCnt = 0;
    }
}

void pl0fBoatRoll(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
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

void pl0fBoatAddSpd(cPl0f* em, u32 no, Vec* spd)
{
    Pl0fWork* w = PL0F_WK(em);

    if (no < 2) {
        PSVECAdd(&w->node[no].spd, spd, &w->node[no].spd);
    }
}

void pl0fBoatSpdControl(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    Mtx m;
    Vec spd;
    Vec p;

    p = em->pos;
    p.y += 500.0f;
    if (pG->flags_5010 & 0x00800000) {
        if (w->flags & 2) {
            pl0fEngineStop(w, &p);
        }
        w->engineCnt = 0;
        return;
    }
    if (w->tiller & 0xF) {
        if (!(w->flags & 2)) {
            w->flags |= 2;
            SndCall(8, 0xA, &p, 0xF, 0, 0);
            w->engineCnt = 0;
        }
        w->engineCnt++;
    } else {
        if (w->flags & 2) {
            pl0fEngineStop(w, &p);
        }
        w->engineCnt = 0;
    }
    spd.x = 0.0f;
    spd.y = 0.0f;
    spd.z = 0.0f;
    if (w->tiller & 1) {
        switch (em->type) {   // range node: `cmpwi 5; bgt; cmpwi 1; bge` (an if-range folds to subi/cmplwi); default first
        default:
            spd.x = 0.0f;
            spd.y = 0.0f;
            spd.z = 50.0f;
            break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            spd.x = 0.0f;
            spd.y = 0.0f;
            spd.z = 20.0f;
            break;
        }
    }
    if (w->tiller & 2) {
        spd.x = 0.0f;
        spd.y = 0.0f;
        spd.z = -15.0f;
    }
    if (w->tiller & 0xC) {
        if (w->tiller & 1) {
            if (w->tiller & 4) {
                w->rotSpd = -PI / 20;
            }
            if (w->tiller & 8) {
                w->rotSpd = PI / 20;
            }
        } else {
            if (w->pBoss) {
                if (w->tiller & 4) {
                    w->rotSpd = -PI / 16;
                }
                if (w->tiller & 8) {
                    w->rotSpd = PI / 16;
                }
                spd.x = 0.0f;
                spd.y = 0.0f;
                spd.z = 50.0f;
            } else {
                if (w->tiller & 4) {
                    w->rotSpd = -PI / 8;
                }
                if (w->tiller & 8) {
                    w->rotSpd = PI / 8;
                }
                spd.x = 0.0f;
                spd.y = 0.0f;
                spd.z = 25.0f;
            }
            if (w->tiller & 2) {
                spd.z *= -0.5f;
            }
        }
    } else {
        w->rotSpd = 0.0f;
    }
    PSMTXRotRad(m, 'y', em->rot.y + w->rotSpd);
    PSMTXMultVecSR(m, &spd, &spd);
    pl0fBoatAddSpd(em, 1, &spd);
    w->tiller = 0;
}

void pl0fBoatChaseBoss(cPl0f* em)
{
    cEm* boss = PL0F_WK(em)->pBoss;   // no work pointer: pBoss folds into em+0x3F0
    Pl0fNode* n;
    Vec v;
    Vec b;

    if (boss == 0) {
        return;
    }
    n = &PL0F_WK(em)->node[0];   // assigned after the early return: the addi is issued with the first block's stores
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = -2000.0f;
    PSMTXMultVec(boss->mat, &v, &v);
    v.y = n->wpos.y;
    n->fixed = 1;
    n->fixPos = v;
    if (boss->flags_3C8 & 8) {
        n->maxLen = 500000.0f;
    } else {
        f32 len;

        b.x = 0.0f;
        b.y = 0.0f;
        b.z = -2000.0f;
        PSMTXMultVec(boss->mat, &b, &b);
        len = SQRTF((n->wpos.x - b.x) * (n->wpos.x - b.x) + (n->wpos.z - b.z) * (n->wpos.z - b.z));
        if (len < n->maxLen && len > 25000.0f) {
            n->maxLen = len;
        } else {
            n->maxLen = n->maxLen * 0.97f + 750.0f;
        }
    }
}

// Camera distance from the position / target pair, then the orientation.
#define CAM_SET(cam)                                                                                              \
    {                                                                                                             \
        Vec* cp = &(cam).param.pos;                                                                               \
        Vec* ca = &(cam).param.at;                                                                                \
                                                                                                                  \
        (cam).dist = SQRTF((cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z)); \
    }                                                                                                             \
    CameraSetOrientationUp(&(cam))

static Vec pl0f_ride_cam_ofs = { -1000.0f, 1500.0f, -5000.0f };

void pl0fRideCamMove(cPl0f* em, f32 rate)
{
    Camera* gcam = &pG->Cam;
    Mtx m;
    Vec pos;
    Vec at;

    PSMTXRotRad(m, 'y', em->rot.y);
    TransMatrix(m, &em->pos);
    PSMTXMultVec(m, &pl0f_ride_cam_ofs, &pos);
    at = pPL->getPartsPtr(0)->worldPos;
    pl0f_camera.param.fovy = 40.0f;
    PosToPos(&gcam->param.at, &at, &pl0f_camera.param.at, rate);
    PosToPos(&gcam->param.pos, &pos, &pl0f_camera.param.pos, rate);
    pl0f_camera.up.x = 0.0f;
    pl0f_camera.up.y = 1.0f;
    pl0f_camera.up.z = 0.0f;
    CAM_SET(pl0f_camera);
    CamCtrl.x250 = (s32) &pl0f_camera;
}

static Vec pl0f_getoff_cam_ofs = { -1000.0f, 1500.0f, -5000.0f };

void pl0fGetoffCamMove(cPl0f* em)
{
    Camera* gcam = &pG->Cam;
    Mtx m;
    Vec pos;
    Vec at;

    PSMTXRotRad(m, 'y', em->rot.y);
    TransMatrix(m, &em->pos);
    PSMTXMultVec(m, &pl0f_getoff_cam_ofs, &pos);
    at = pPL->getPartsPtr(0)->worldPos;
    pl0f_camera.param.fovy = 40.0f;
    PosToPos(&gcam->param.at, &at, &pl0f_camera.param.at, 1.0f);
    PosToPos(&gcam->param.pos, &pos, &pl0f_camera.param.pos, 1.0f);
    pl0f_camera.up.x = 0.0f;
    pl0f_camera.up.y = 1.0f;
    pl0f_camera.up.z = 0.0f;
    CAM_SET(pl0f_camera);
    CamCtrl.x250 = (s32) &pl0f_camera;
}

static Vec pl0f_boss_cam_ofs = { -1200.0f, 1400.0f, 0.0f };
static f32 pl0f_boss_cam_y = -500.0f;
static f32 pl0f_boss_cam_up = 1400.0f;
static f32 pl0f_boss_cam_dist = 5000.0f;
static f32 pl0f_boss_cam_y2 = -1000.0f;
static f32 pl0f_boss_cam_up2 = 1600.0f;
static f32 pl0f_boss_cam_dist2 = 1800.0f;
// Struct view of a static f32: the load is MEM_IN_STRUCT_P and stays below the preceding `cat = bpos`
// struct-copy stores (a plain scalar load never aliases a varying struct store and floats above them).
struct Pl0fF32V { f32 f; };
#define PL0F_F32S(x) (((Pl0fF32V*) &(x))->f)
static Vec pl0f_boss_cam_at_ofs = { -400.0f, 0.0f, 0.0f };
static Vec pl0f_boss_cam_pos0 = { -1000.0f, 1500.0f, -5000.0f };
static Vec pl0f_boss_cam_at0 = { 0.0f, 1000.0f, 5000.0f };
static Vec pl0f_boss_cam_pos1 = { -500.0f, 1900.0f, -1500.0f };
static Vec pl0f_boss_cam_at1 = { 0.0f, 1500.0f, 5000.0f };

void pl0fBossCamMove(cPl0f* em, int hide)
{
    Pl0fWork* w = PL0F_WK(em);
    Camera* gcam = &pG->Cam;
    Mtx m;
    Vec bpos;
    Vec cpos;
    Vec cat;
    Vec d;
    Vec ang;
    Vec d2;
    f32 len;

    cEm* boss = w->pBoss;   // read before the flags test (the original loads pBoss above the `andi.`)

    if (pPL->flags_420 & 4) {
        return;
    }
    if (boss && w->bossMode) {
        if (hide) {
            bpos = boss->pos;
            bpos.y = em->pos.y + 1300.0f;
            cpos = pl0f_boss_cam_at_ofs;
            PSMTXMultVec(em->mat, &cpos, &cpos);
            PSVECSubtract(&bpos, &cpos, &d);
            len = SQRTF(d.x * d.x + d.z * d.z);
            ang.x = -atan2f(d.y, len);
            ang.y = atan2f(d.x, d.z);
            ang.z = 0.0f;
            ang.y = Muku2(em->rot.y, ang.y, PI / 4) + em->rot.y;
            RotMatrix(m, &ang);
            TransMatrix(m, &cpos);
            d.z = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z);
            d.x = 0.0f;
            d.y = 0.0f;
            PSMTXMultVec(m, &d, &bpos);
            cpos.y += pl0f_boss_cam_up2;
            PSVECSubtract(&cpos, &bpos, &d);
#line 1838
            VECNormalize(&d, &d);
            PSVECScale(&d, &d, pl0f_boss_cam_dist2);
            PSVECAdd(&cpos, &d, &d);
            if (d.y < em->pos.y + pl0f_boss_cam_up2) {
                d.y = em->pos.y + pl0f_boss_cam_up2;
            }
            cpos = d;
            cat = bpos;
            if (cat.y < em->pos.y + PL0F_F32S(pl0f_boss_cam_y2)) {
                cat.y = em->pos.y + PL0F_F32S(pl0f_boss_cam_y2);
            }
            PSVECSubtract(&cat, &cpos, &d);
            len = SQRTF(d.x * d.x + d.z * d.z);
            ang.x = -atan2f(d.y, len);
            ang.y = atan2f(d.x, d.z);
            ang.z = 0.0f;
            if (ang.x > 0.2617994f) {
                ang.x = 0.2617994f;
            }
            if (ang.x < -0.2617994f) {
                ang.x = -0.2617994f;
            }
            RotMatrix(m, &ang);
            TransMatrix(m, &cpos);
            len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z);
            d.x = 0.0f;
            d.y = 0.0f;
            d.z = len;
            PSMTXMultVec(m, &d, &cat);
            pl0f_camera.param.fovy = pl0f_camera.param.fovy * 0.9f + 3.0f;
        } else {
            PSMTXRotRad(m, 'y', em->rot.y);
            TransMatrix(m, &em->pos);
            PSMTXMultVec(m, &pl0f_boss_cam_ofs, &cpos);
            bpos = w->hist[w->histIdx];
            bpos.y = em->pos.y + pl0f_boss_cam_y;
            PSVECSubtract(&cpos, &bpos, &d);
#line 1876
            VECNormalize(&d, &d);
            PSVECScale(&d, &d, pl0f_boss_cam_dist);
            PSVECAdd(&cpos, &d, &d);
            if (d.y < em->pos.y + pl0f_boss_cam_up) {
                d.y = em->pos.y + pl0f_boss_cam_up;
            }
            cpos = d;
            PSVECAdd(&em->pos, &bpos, &d);
            PSVECScale(&d, &d, 0.5f);
            cat = d;
            pl0f_camera.param.fovy = pl0f_camera.param.fovy * 0.9f + 4.0f;
            PSVECSubtract(&cat, &cpos, &d);
            len = SQRTF(d.x * d.x + d.z * d.z);
            ang.x = -atan2f(d.y, len);
            ang.y = atan2f(d.x, d.z);
            ang.z = 0.0f;
            if (ang.x > 0.2617994f) {
                ang.x = 0.2617994f;
            }
            if (ang.x < -0.2617994f) {
                ang.x = -0.2617994f;
            }
            RotMatrix(m, &ang);
            TransMatrix(m, &cpos);
            len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z);
            d.x = 0.0f;
            d.y = 0.0f;
            d.z = len;
            PSMTXMultVec(m, &d, &cat);
        }
        PosToPos(&gcam->param.at, &cat, &pl0f_camera.param.at, 0.1f);
        PosToPos(&gcam->param.pos, &cpos, &pl0f_camera.param.pos, 0.5f);
    } else {
        PSMTXRotRad(m, 'y', em->rot.y);
        TransMatrix(m, &em->pos);
        if (hide) {
            PSMTXMultVec(m, &pl0f_boss_cam_pos1, &cpos);
            PSMTXMultVec(m, &pl0f_boss_cam_at1, &cat);
        } else {
            PSMTXMultVec(m, &pl0f_boss_cam_pos0, &cpos);
            PSMTXMultVec(m, &pl0f_boss_cam_at0, &cat);
        }
        pl0f_camera.param.fovy = pl0f_camera.param.fovy * 0.9f + 4.0f;
        PosToPos(&gcam->param.at, &cat, &pl0f_camera.param.at, 0.3f);
        PosToPos(&gcam->param.pos, &cpos, &pl0f_camera.param.pos, 0.5f);
        PSVECSubtract(&pl0f_camera.param.at, &pl0f_camera.param.pos, &d2);
#line 1929
        VECNormalize(&d2, &d2);
        PSVECScale(&d2, &d2, 1500.0f);
        PSVECAdd(&pl0f_camera.param.pos, &d2, &d2);
        EatMgr.adjust(0, &d2, &pl0f_camera.param.pos, 500.0f, 0x2001, 0);
    }
    pl0f_camera.up.x = 0.0f;
    pl0f_camera.up.y = 1.0f;
    pl0f_camera.up.z = 0.0f;
    CAM_SET(pl0f_camera);
    CamCtrl.x250 = (s32) &pl0f_camera;
}

static Vec pl0f_hide_cam_at = { -500.0f, 1850.0f, -1800.0f };
static Vec pl0f_hide_cam_pos = { 0.0f, -200.0f, 10000.0f };

void pl0fHideModeCamSet(cPlayer* pl)
{
    Camera* gcam = &pG->Cam;
    Mtx m;
    Vec at;
    Vec pos;

    PSMTXRotRad(m, 'y', pl->pBoat->rot.y);
    TransMatrix(m, &pl->pos);
    PSMTXMultVec(m, &pl0f_hide_cam_at, &at);
    PSMTXMultVec(m, &pl0f_hide_cam_pos, &pos);
    pl0f_camera.param.fovy = pl0f_camera.param.fovy * 0.9f + 4.0f;
    PosToPos(&gcam->param.at, &pos, &pl0f_camera.param.at, 1.0f);
    PosToPos(&gcam->param.pos, &at, &pl0f_camera.param.pos, 1.0f);
    pl0f_camera.up.x = 0.0f;
    pl0f_camera.up.y = 1.0f;
    pl0f_camera.up.z = 0.0f;
    CAM_SET(pl0f_camera);
}

void pl0fHideModeCamMove(cPlayer* pl)
{
    Camera* gcam = &pG->Cam;
    Mtx m;
    Vec at;
    Vec pos;

    PSMTXRotRad(m, 'y', pl->pBoat->rot.y);
    TransMatrix(m, &pl->pos);
    PSMTXMultVec(m, &pl0f_hide_cam_at, &at);
    PSMTXMultVec(m, &pl0f_hide_cam_pos, &pos);
    pl0f_camera.param.fovy = pl0f_camera.param.fovy * 0.9f + 4.0f;
    PosToPos(&gcam->param.at, &pos, &pl0f_camera.param.at, 0.1f);
    PosToPos(&gcam->param.pos, &at, &pl0f_camera.param.pos, 0.1f);
    pl0f_camera.up.x = 0.0f;
    pl0f_camera.up.y = 1.0f;
    pl0f_camera.up.z = 0.0f;
    CAM_SET(pl0f_camera);
    CamCtrl.x250 = (s32) &pl0f_camera;
}

static Vec pl0f_die_cam_at = { -300.0f, 1700.0f, -2500.0f };
static Vec pl0f_die_cam_pos = { 0.0f, 1600.0f, 10000.0f };

void pl0fBossDieCamSet(cPlayer* pl)
{
    Camera* gcam = &pG->Cam;
    Mtx m;
    Vec at;
    Vec pos;

    PSMTXRotRad(m, 'y', pl->pBoat->rot.y);
    TransMatrix(m, &pl->pos);
    PSMTXMultVec(m, &pl0f_die_cam_at, &at);
    PSMTXMultVec(m, &pl0f_die_cam_pos, &pos);
    pl0f_camera.param.fovy = 40.0f;
    PosToPos(&gcam->param.at, &pos, &pl0f_camera.param.at, 1.0f);
    PosToPos(&gcam->param.pos, &at, &pl0f_camera.param.pos, 1.0f);
    pl0f_camera.up.x = 0.0f;
    pl0f_camera.up.y = 1.0f;
    pl0f_camera.up.z = 0.0f;
    CAM_SET(pl0f_camera);
}

void pl0fBossDieCamMove(cPlayer* pl)
{
    Camera* gcam = &pG->Cam;
    Mtx m;
    Vec at;
    Vec pos;

    PSMTXRotRad(m, 'y', pl->pBoat->rot.y);
    TransMatrix(m, &pl->pos);
    PSMTXMultVec(m, &pl0f_die_cam_at, &at);
    PSMTXMultVec(m, &pl0f_die_cam_pos, &pos);
    pl0f_camera.param.fovy = 40.0f;
    PosToPos(&gcam->param.at, &pos, &pl0f_camera.param.at, 1.0f);
    PosToPos(&gcam->param.pos, &at, &pl0f_camera.param.pos, 1.0f);
    pl0f_camera.up.x = 0.0f;
    pl0f_camera.up.y = 1.0f;
    pl0f_camera.up.z = 0.0f;
    CAM_SET(pl0f_camera);
    CamCtrl.x250 = (s32) &pl0f_camera;
}

void pl0fRideActEvtCk(cPl0f* em)
{
    if (pG->flags_5010 & 0x00200000) {
        return;
    }
    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, PI)) > PI / 4) {
        return;
    }
    if (fabsf(em->pos.y - pPL->pos.y) > 10000.0f) {
        return;
    }
    if (em->plDist2 > 9000000.0f) {
        return;
    }
    switch (em->type) {
    case 0:
    default:
        ActBtn.set(0x23, 5, (int) pl0fActRide, (int) em, 0, 1, 0, 0);
        break;
    case 1:
        ActBtn.set(0x23, 5, (int) pl0fActRideR10d, (int) em, 0, 1, 0, 0);
        break;
    case 2:
    case 3:
        ActBtn.set(0x23, 5, (int) pl0fActRideR10e, (int) em, 0, 1, 0, 0);
        break;
    case 4:
    case 5:
        ActBtn.set(0x23, 5, (int) pl0fActRideR10e2, (int) em, 0, 1, 0, 0);
        break;
    }
}

static Vec pl0f_getoff_ck[3] = {
    { -48000.0f, -1300.0f, 22350.0f },
    { 124900.0f, -1300.0f, 148110.0f },
    { 0.0f, 0.0f, 0.0f },
};
static Vec pl0f_getoff_land[3] = {
    { -48810.0f, -1300.0f, 24780.0f },
    { 127560.0f, -1300.0f, 149100.0f },
    { 0.0f, 0.0f, 0.0f },
};
static f32 pl0f_getoff_ang[3] = { 1.466677f, 3.089821f, 0.0f };

void pl0fGetoffActEvtCk(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    u32 i;

    if (!(pG->flags_5010 & 0x00200000)) {
        return;
    }
    if ((pG->room_id32 & 0xFFFF0000) != 0x010B0000 && (pG->room_id32 & 0xFFFF0000) != 0x011B0000) {
        return;
    }
    for (i = 0; i < 2; i++) {
        if ((pPL->pos.x - pl0f_getoff_ck[i].x) * (pPL->pos.x - pl0f_getoff_ck[i].x) + (pPL->pos.z - pl0f_getoff_ck[i].z) * (pPL->pos.z - pl0f_getoff_ck[i].z) < 4.9e7f) {
            w->getoffAng = pl0f_getoff_ang[i];
            w->getoffPos = pl0f_getoff_land[i];
            ActBtn.set(0x24, 5, (int) pl0fActGetOff, (int) em, 0, 1, 0, 0);
        }
    }
}

static void pl0fActRide(cPl0f* em)
{
    EmSet(pPL->pBoat, em);
    BoatMoveFunc = PlBoatMove;
    PlRoutineSet(pPL, 0, 0xF, 0, 0);
    PlRoutineSet(em, 1, 1, 0, 0);
    if (pSUB) {
        SetSubDamage((int) em, (void*) subBoatRide);
    }
}

static void pl0fActRideR10d(cPl0f* em)
{
    EmSet(pPL->pBoat, em);
    BoatMoveFunc = PlBoatMove;
    PlRoutineSet(pPL, 0, 0xF, 0xE, 0);
    PlRoutineSet(em, 1, 9, 0, 0);
    if (pSUB) {
        SetSubDamage((int) em, (void*) subBoatRide);
    }
}

static void pl0fActRideR10e(cPl0f* em)
{
    EmSet(pPL->pBoat, em);
    BoatMoveFunc = PlBoatMove;
    PlRoutineSet(pPL, 0, 0xF, 0x10, 0);
    PlRoutineSet(em, 1, 0xB, 0, 0);
    if (pSUB) {
        SetSubDamage((int) em, (void*) subBoatRide);
    }
}

static void pl0fActRideR10e2(cPl0f* em)
{
    EmSet(pPL->pBoat, em);
    BoatMoveFunc = PlBoatMove;
    PlRoutineSet(pPL, 0, 0xF, 0x12, 0);
    PlRoutineSet(em, 1, 0xD, 0, 0);
    if (pSUB) {
        SetSubDamage((int) em, (void*) subBoatRide);
    }
}

static void pl0fActGetOff(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    cPlayer* pl = pPL;

    pl->x400 = w->getoffAng;
    pl->evTarget = w->getoffPos;
    PlRoutineSet(pl, 0, 0xF, 1, 0);
    PlRoutineSet(em, 1, 0, 0, 0);
    w->flags &= ~1;
    SndStop(w->seNo, 0);
    if (pSUB) {
        SetSubDamage((int) em, (void*) subBoatGetoff);
    }
}

// The boat hits the boss (em2f) or a floating island (obj1c).
int pl0fCrashCk(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    Vec hit;
    u32 i;
    u32 n;

    for (n = 0; n < EmMgr.nArray; n++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * n);

        if ((e->be_flag & 0x201) == 1 && e->id == 0x2F && (s16) e->hp > 0) {
            for (i = 0; i < 2; i++) {
                if (EmYarareContactCk(e, &w->node[i].wpos, &hit, 800.0f)) {
                    int away = 0;

                    if (pG->flags_5010 & 0x00800000) {
                        away = 1;
                    }
                    if (e->flags_3C8 & 4) {
                        away = 1;
                    }
                    pl0fCrashAdjustSet(em, &hit, away);
                    return 1;
                }
            }
        }
    }
    for (n = 0; n < ObjMgr.nArray; n++) {
        cObj* o = (cObj*) ((u8*) ObjMgr.pArray + ObjMgr.size * n);

        if ((o->be_flag & 0x201) == 1 && o->id == 0x1C) {
            f32 r = o->scale.x * 1800.0f;

            for (i = 0; i < 2; i++) {
                Pl0fNode* nd = &w->node[i];

                if ((nd->wpos.x - o->pos.x) * (nd->wpos.x - o->pos.x) + (nd->wpos.z - o->pos.z) * (nd->wpos.z - o->pos.z) < r * r) {
                    int away;

                    ((cObj1c*) o)->setCrash();
                    away = 0;   // after the call: the flag stays in the argument register
                    if (pG->flags_5010 & 0x00800000) {
                        away = 1;
                    }
                    pl0fCrashAdjustSet(em, &o->pos, away);
                    return 1;
                }
            }
        }
    }
    return 0;
}

void pl0fCrashAdjustSet(cPl0f* em, Vec* p, int away)
{
    Pl0fWork* w = PL0F_WK(em);
    Mtx m;
    Vec d;
    u32 i;

    if (away) {
        d.x = 0.0f;
        d.y = 0.0f;
        d.z = -1.0f;
        PSMTXRotRad(m, 'y', em->rot.y);
        PSMTXMultVecSR(m, &d, &d);
        PSVECScale(&d, &d, 300.0f);
    } else {
        PSVECSubtract(&em->pos, p, &d);
        d.y = 0.0f;
        if (d.x == 0.0f && d.z == 0.0f) {
            d.x = 0.0f;
            d.y = 0.0f;
            d.z = -1.0f;
            PSMTXMultVecSR(em->mat, &d, &d);
        }
#line 2446
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, 300.0f);
    }
    for (i = 0; i < 2; i++) {
        w->node[i].spd = d;
    }
}

// Pushes both nodes out of the scenario walls; the movement of the first hit node is applied to both.
// pLog read as a plain struct member (no inline operator-> block notes): the high(pLog) then sits right
// next to its load and loop.c's lifetime for the movable is 1, below the hoisting threshold of a
// 130-insn loop with a call (ScrAdjust); the header macro's operator-> gives lifetime 3 (hoisted).
#define PL0F_VECNORMALIZE(src, dst)                                                     \
    if (0.0f == (src)->x && 0.0f == (src)->y && 0.0f == (src)->z) {                    \
        pLog.p->err(0, 0, "VECNormalize:[%s/%d]", __FILE__, __LINE__);                  \
        (dst)->x = (dst)->y = (dst)->z = 0.0f;                                          \
    } else                                                                              \
        PSVECNormalize(src, dst)

void pl0fScrAdjust(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    Vec nrm;
    Vec p;
    Vec d;
    u32 i;
    u32 j;

    if (em->type == 1) {
        return;
    }
    if (em->type == 2) {
        return;
    }
    if (em->type == 3) {
        return;
    }
    if (em->type == 4) {
        return;
    }
    if (em->type == 5) {
        return;
    }
    for (i = 0; i < 2; i++) {
        Pl0fNode* n = &w->node[i];

        nrm.x = 0.0f;
        nrm.y = 0.0f;
        nrm.z = 0.0f;
        p = n->wpos;
        SatMgr.adjust(&nrm, &n->wposOld, &p, 300.0f, 0x2081, 0);
        if (!(nrm.x == 0.0f && nrm.y == 0.0f && nrm.z == 0.0f)) {
            f32 len;

            PSVECSubtract(&p, &n->wpos, &d);
            d.y = 0.0f;
            if (!(d.x == 0.0f && d.z == 0.0f)) {
                len = SQRTF(d.x * d.x + d.z * d.z) * 1.2f;
#line 2499
                PL0F_VECNORMALIZE(&d, &d);
                PSVECScale(&d, &d, len);
                for (j = 0; j < 2; j++) {
                    Vec* wp = &w->node[j].wpos;
                    Vec* sp = &w->node[j].spd;

                    PSVECAdd(wp, &d, wp);
                    *sp = d;
                }
                return;
            }
        }
        // Dead second set of `n` (deleted by flow, the compare by jump2): with two sets `n` is not a
        // giv, so loop.c keeps the target's per-iteration `add n, w, ofs` (ofs = the reduced giv
        // i * sizeof(Pl0fNode) + 0x168, initialised after the hoisted highs) and the member addresses
        // stay displacements from `n` instead of becoming stepping pointers. The frame operands add
        // no register refs, so the callee-saved order (&nrm, ofs, i, &p, w) is unchanged.
        if (d.x == d.y) {
            n = 0;
        }
    }
}

static PlBoatFunc plboat_R2_move_tbl[19] = {
    plboat_R2_Ride,
    plboat_R2_Getoff,
    plboat_R2_Move,
    plboat_R2_SpearSet,
    plboat_R2_SpearThrow,
    plboat_R2_FallWater,
    plboat_R2_Swim,
    plboat_R2_Guard,
    plboat_R2_WaterRide,
    plboat_R2_Die,
    plboat_R2_SpearSet2,
    plboat_R2_SpearThrow2,
    plboat_R2_BossDie,
    plboat_R2_R10dIn,
    plboat_R2_R10dOut,
    plboat_R2_R10eIn,
    plboat_R2_R10eOut,
    plboat_R2_R10eIn2,
    plboat_R2_R10eOut2,
};

// The player's boat routine (pl_R1_Boat -> BoatMoveFunc): the boat's motion archive replaces the
// player's for the duration of the routine.
static void PlBoatMove(cPlayer* pl)
{
    if (pl->pBoat == 0) {
        pLog->err(0, 0, "PlBoatMove(): m_pBoat == NULL!");
        return;
    }
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
    pl0fSetAnchorEm2f(pl);
    pl->subArc = pl->subArc2;
}

// The engine start SE of the boat, the player's foot on the tiller.
// Engine start SE: a macro on the routine's own `Vec v` (the original shares the boarding vector's frame
// slot; an inline's temp Vec takes a second slot).
#define PLBOAT_ENGINE_START() \
{ \
    Pl0fWork* w = PL0F_WK(boat); \
 \
    w->flags |= 1; \
    v = boat->pos; \
    v.y += 500.0f; \
    w->seNo = SndCall(8, 0x11, &v, 0xF, 0, 0); \
}

static void plboat_R2_Ride(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    Vec v;

    switch (pl->xFF) {
    case 0:
        v.x = 1500.0f;
        v.y = 500.0f;
        v.z = 100.0f;
        PSMTXMultVec(boat->mat, &v, &v);
        pl->pos.x = v.x;
        pl->pos.z = v.z;
        pl->x3FC = 1;
        pl->x400 = v.y - pl->pos.y;
        pl->x3E0 = 20;
        pl->rot.y = boat->rot.y - PI / 2;
        pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        MotionSetCore(pl, &pl->mot, PLARC(0x29), 0, 0, 5, 0);
        pl->sightRate = 0.0f;
        pl->blendRate500 = 0.0f;
        pl->pBody->initWepHand((u32) PLARC(0x8));
        pl->setRightHand(1);
        pl->pWep->setTrans(0, 0);
        pl->xFF++;
    case 1:
        if (pl->x3E0) {
            pl->x3E0--;
        } else {
            f32 dy = pl->x400 * 0.1f;

            pl->pos.y += dy;
            pl->x400 -= dy;
        }
        if (pl->frame > 25.7f && pl->frame < 26.3f) {
            SndCall(8, 4, &pl->pos, boat->id, 0, 0);
        }
        if (pl->frame > 27.7f && pl->frame < 28.3f) {
            SndCall(8, 5, &pl->pos, boat->id, 0, 0);
        }
        if (MotionMoveF(pl, 0)) {
            PLBOAT_ENGINE_START();
            PlRoutineSet(pPLS, 0, 0xF, 2, 0);
        }
        break;
    }
    if (pl->x3FC) {
        pl->x3FC--;
        pl0fRideCamMove(boat, 1.0f);
    } else {
        pl0fRideCamMove(boat, 0.05f);
    }
}

static void plboat_R2_Getoff(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    switch (pl->xFF) {
    case 0:
        MotionSetCore(pl, &pl->mot, PLARC(0x2A), 0, 0, 5, 0);
        pl->pos = pl->evTarget;
        pl->rot.y = pl->x400;
        boat->setPos(&pl->pos, pl->x400);
        FSet(pl->blendRate500, 0.0f);   // the pSUB load stays below the store
        if (pSUB) {
            subOnBoat(pSUB, boat);
            pSUB->partsMatCalc();
            pSUB->partsWorldCalc();
        }
        pl->x3E0 = 20;
        pl->x400 = 100.0f;
        pl->xFF++;
    case 1:
        pl0fGetoffCamMove(boat);
        if (pl->x3E0) {
            pl->x3E0--;
        } else {
            f32 dy = pl->x400 * 0.1f;

            pl->pos.y += dy;
            pl->x400 -= dy;
        }
        if (pl->frame > 30.7f && pl->frame < 31.3f) {
            SndCall(5, 2, &pl->pos, pl->id, 0, 0);
        }
        if (pl->frame > 45.7f && pl->frame < 46.3f) {
            SndCall(5, 3, &pl->pos, pl->id, 0, 0);
        }
        if (MotionMoveF(pl, 0)) {
            cPlayer* p;

            EndPlDamage();
            p = pPL;
            p->setRightHand(0);
            p->pWep->setTrans(1, 0);
        }
        break;
    }
}

static void plboat_R2_Move(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    cEm* boss;

    if (boat) {
        // The work pointer local keeps the then-arm two insns at jump1 time, so the `boss = 0` hoist
        // happens in jump2 (after sched2) and the `li` stays between the compare and the branch.
        Pl0fWork* w = PL0F_WK(boat);
        boss = w->pBoss;
    } else {
        boss = 0;
    }
    switch (pl->xFF) {
    case 0:
        pl->blendRate500 = 0.0f;
        pl->x4FD = 0xA;
        pl->x4FC = 0;
        if (pl->pSpear) {
            pl->pSpear->setLost();
            pl->pSpear = 0;
        }
        pl->xFF++;
    case 1:
        plboatBlendMotSet(pl, PLARC(0x9), PLARC(0xB), PLARC(0xA), 0, 0, 0);
        if (Key.on & 0xC) {
            if (Key.on & 8) {
                pl->blendRate500 += 31.875f;
                if (pl->blendRate500 > 255.0f) {
                    pl->blendRate500 = 255.0f;
                }
            }
            if (Key.on & 4) {
                pl->blendRate500 -= 31.875f;
                if (pl->blendRate500 < -255.0f) {
                    pl->blendRate500 = -255.0f;
                }
            }
        } else {
            pl->blendRate500 *= 0.9f;
        }
        pl->sightRate *= 0.9f;
        plOnBoat(pl);
        MotionMoveF(pl, 0);
        if (Key.on & 0x10) {
            PlRoutineSet(pPL, 0, 0xF, 3, 0);
        }
        break;
    }
    if (pl->pBoat) {
        PL_BOAT(pl)->setTiller();
        if (pl->pBoat) {
            pl0fBossCamMove(PL_BOAT(pl), 0);
        }
    }
    if (boss) {
        if (boss->flags_3C8 & 0x20) {
            PlRoutineSet(pPL, 0, 0xF, 0xA, 0);
        }
    } else {
        pl0fGetoffActEvtCk(boat);
    }
}

// Harpoon aim: the stick (or the buttons) lean the player (blendRate500) and tilt the sight
// (sightRate); at the sight limits the boat turns.
// Harpoon aim: a macro, not a static inline. integrate.c drops RTX_UNCHANGING_P from the inlined pool loads
// (the 255 / 0.39 clamp constants), so they would depend on the preceding `stfs blendRate500/sightRate` and
// sink below it; the original loads them before the add and compares before the store.
#define PLBOAT_AIM_CONTROL() \
{ \
    f32 d; \
 \
    if ((u8) (Key.sy + 15) > 30) { \
        if ((int) pSys->flags < 0) { \
            d = (f32) -Key.sy / 72.0f * 31.875f; \
        } else { \
            d = (f32) Key.sy / 72.0f * 31.875f; \
        } \
        pl->blendRate500 += d; \
        if (pl->blendRate500 > 255.0f) { \
            pl->blendRate500 = 255.0f; \
        } \
        if (pl->blendRate500 < -255.0f) { \
            pl->blendRate500 = -255.0f; \
        } \
    } else if (Key.on & 3) { \
        if (Key.on & 1) { \
            if ((int) pSys->flags < 0) { \
                d = -31.875f; \
            } else { \
                d = 31.875f; \
            } \
        } else { \
            if ((int) pSys->flags < 0) { \
                d = 31.875f; \
            } else { \
                d = -31.875f; \
            } \
        } \
        pl->blendRate500 += d; \
        if (pl->blendRate500 > 255.0f) { \
            pl->blendRate500 = 255.0f; \
        } \
        if (pl->blendRate500 < -255.0f) { \
            pl->blendRate500 = -255.0f; \
        } \
    } \
    if ((u8) (Key.sx + 15) > 30) { \
        d = (f32) Key.sx / -72.0f * 0.049087385f; \
        pl->sightRate += d; \
        if (pl->sightRate > 0.3926991f) { \
            pl->sightRate = 0.3926991f; \
            boat->rot.y += d; \
        } \
        if (pl->sightRate < -0.3926991f) { \
            pl->sightRate = -0.3926991f; \
            boat->rot.y += d; \
        } \
    } else if (Key.on & 0xC) { \
        if (Key.on & 4) { \
            d = -0.049087385f; \
        } else { \
            d = 0.049087385f; \
        } \
        pl->sightRate += d; \
        if (pl->sightRate > 0.3926991f) { \
            pl->sightRate = 0.3926991f; \
            boat->rot.y += d; \
        } \
        if (pl->sightRate < -0.3926991f) { \
            pl->sightRate = -0.3926991f; \
            boat->rot.y += d; \
        } \
    } \
}

static void plboat_R2_SpearSet(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    cEm* boss;

    if (boat) {
        // The work pointer local keeps the then-arm two insns at jump1 time, so the `boss = 0` hoist
        // happens in jump2 (after sched2) and the `li` stays between the compare and the branch.
        Pl0fWork* w = PL0F_WK(boat);
        boss = w->pBoss;
    } else {
        boss = 0;
    }
    switch (pl->xFF) {
    case 0:
        MotionSetCore(pl, &pl->mot, PLARC(0xC), 0, 0xA, 1, 0);
        pl->x4FD = 0xA;
        pl->x4FC = 0;
        pl->blendRate500 = 0.0f;
        pl->x3EC = 0xF;
        pl->xFF++;
    case 1:
        plOnBoat(pl);
        if (pl->frame > 6.7f && pl->frame < 7.3f) {
            plboatSetSpear(pl);
        }
        if (MotionMoveF(pl, 0)) {
            pl->xFF++;
        } else if (!(Key.on & 0x10) || (boss && (boss->flags_3C8 & 0x20))) {
            if (pl->pSpear) {
                pl->pSpear->setLost();
                pl->pSpear = 0;
            }
            FSet(pl->blendRate500, 0.0f);   // the pPL load stays below the store
            PlRoutineSet(pPL, 0, 0xF, 2, 0);
        }
        break;
    case 2:
        pl->x4FD = 0xA;
        pl->x4FC = 0;
        plboatSetSpear(pl);
        pl->xFF++;
    case 3:
        plboatBlendMotSet(pl, PLARC(0xE), PLARC(0xF), PLARC(0xD), 0, 0, 0);
        PLBOAT_AIM_CONTROL();
        plOnBoat(pl);
        MotionMoveF(pl, 0);
        plboatSightCurMove(pl);
        if ((boss && (boss->flags_3C8 & 0x20)) || !(Key.on & 0x10)) {
            pl->xFF++;
        } else if (Key.trg & 0x80) {
            PlRoutineSet(pPL, 0, 0xF, 4, 0);
        }
        break;
    case 4:
        MotionSetCore(pl, &pl->mot, PLARC(0x13), 0, 0xA, 1, 0);
        pl->x3EC = 99999;
        pl->xFF++;
    case 5:
        plOnBoat(pl);
        if (pl->frame > 13.7f && pl->frame < 14.3f) {
            if (pl->pSpear) {
                pl->pSpear->setLost();
                pl->pSpear = 0;
            }
        }
        if (MotionMoveF(pl, 0)) {
            FSet(pl->blendRate500, 0.0f);
            PlRoutineSet(pPL, 0, 0xF, 2, 0);
        }
        break;
    }
    if (pl->pBoat) {
        if (pl->x3EC) {
            pl->x3EC--;
            pl0fBossCamMove(PL_BOAT(pl), 0);
        } else {
            pG->flags_5010 |= 0x00800000;
            pl0fBossCamMove(PL_BOAT(pl), 1);
        }
    }
}

static void plboat_R2_SpearThrow(cPlayer* pl)
{
    switch (pl->xFF) {
    case 0:
        pl->x4FD = 0xA;
        pl->x4FC = 0;
        U32Set(pl->x3E0, 0);
        pl->xFF++;
    case 1:
        plboatBlendMotSet(pl, PLARC(0x11), PLARC(0x12), PLARC(0x10), 0, 0, 0);
        plOnBoat(pl);
        if (MotionMoveF(pl, 0)) {
            PlRoutineSet(pPL, 0, 0xF, 3, 2);
        } else {
            if (pl->frame > 11.7f && pl->frame < 12.3f) {
                plboatSpearThrow(pl);
            }
            if (pl->frame > 45.7f && pl->frame < 46.3f) {
                plboatSetSpear(pl);
            }
            pl->x3E0++;
            if ((int) pl->x3E0 > 20 && !(Key.on & 0x10)) {
                if ((int) pl->x3E0 >= 31 && (int) pl->x3E0 <= 49) {
                    FSet(pl->blendRate500, 0.0f);
                    PlRoutineSet(pPL, 0, 0xF, 2, 0);
                } else {
                    PlRoutineSet(pPL, 0, 0xF, 3, 4);
                }
            }
        }
        break;
    }
    pG->flags_5010 |= 0x00800000;
    if (pl->pBoat) {
        pl0fBossCamMove(PL_BOAT(pl), 1);
    }
}

static void plboat_R2_SpearSet2(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    cEm* boss;

    if (boat) {
        // The work pointer local keeps the then-arm two insns at jump1 time, so the `boss = 0` hoist
        // happens in jump2 (after sched2) and the `li` stays between the compare and the branch.
        Pl0fWork* w = PL0F_WK(boat);
        boss = w->pBoss;
    } else {
        boss = 0;
    }
    switch (pl->xFF) {
    case 0:
        pl0fHidePosSet(pl);
        MotionSetCore(pl, &pl->mot, PLARC(0x27), 0, 0xA, 1, 0);
        pl->blendRate500 = 0.0f;
        pl->x4FC = 0;
        pl->x4FD = 0xA;
        pl->x3EC = 0xF;
        pl0fHideModeCamSet(pl);
        pl->xFF++;
    case 1:
        plOnBoat(pl);
        if (pl->frame > 89.7f && pl->frame < 90.3f) {
            plboatSetSpear(pl);
        }
        if (MotionMoveF(pl, 0)) {
            pl->xFF++;
        }
        break;
    case 2:
        pl->x4FD = 0xA;
        pl->x4FC = 0;
        plboatSetSpear(pl);
        pl->xFF++;
    case 3:
        ActBtn.set(0x17, 5, 0, 0, 0, 1, 0, 0);
        plboatBlendMotSet(pl, PLARC(0xE), PLARC(0xF), PLARC(0xD), 0, 0, 0);
        PLBOAT_AIM_CONTROL();
        plOnBoat(pl);
        MotionMoveF(pl, 0);
        plboatSightCurMove(pl);
        if (boss && !(boss->flags_3C8 & 0x20)) {
            pl->xFF++;
        } else if (Key.trg & 0x80) {
            PlRoutineSet(pPL, 0, 0xF, 0xB, 0);
        }
        break;
    case 4:
        MotionSetCore(pl, &pl->mot, PLARC(0x13), 0, 0xA, 1, 0);
        pl->x3EC = 99999;
        pl->xFF++;
    case 5:
        plOnBoat(pl);
        if (pl->frame > 13.7f && pl->frame < 14.3f) {
            if (pl->pSpear) {
                pl->pSpear->setLost();
                pl->pSpear = 0;
            }
        }
        if (MotionMoveF(pl, 0)) {
            PlRoutineSet(pPL, 0, 0xF, 2, 0);
        }
        break;
    }
    if (pl->pBoat) {
        if (pl->x3EC) {
            pl->x3EC--;
            pl0fBossCamMove(PL_BOAT(pl), 0);
        } else {
            pG->flags_5010 |= 0x00800000;
            pl0fHideModeCamMove(pl);
        }
    }
}

static void plboat_R2_SpearThrow2(cPlayer* pl)
{
    ActBtn.set(0x17, 5, 0, 0, 0, 1, 0, 0);
    switch (pl->xFF) {
    case 0:
        pl->x4FD = 0xA;
        pl->x4FC = 0;
        U32Set(pl->x3E0, 0);
        pl->xFF++;
    case 1:
        plboatBlendMotSet(pl, PLARC(0x11), PLARC(0x12), PLARC(0x10), 0, 0, 0);
        plOnBoat(pl);
        if (MotionMoveF(pl, 0)) {
            PlRoutineSet(pPL, 0, 0xF, 0xA, 2);
        } else {
            if (pl->frame > 11.7f && pl->frame < 12.3f) {
                plboatSpearThrow(pl);
            }
            if (pl->frame > 45.7f && pl->frame < 46.3f) {
                plboatSetSpear(pl);
            }
        }
        break;
    }
    pG->flags_5010 |= 0x00800000;
    pl0fHideModeCamMove(pl);
}

static void plboat_R2_BossDie(cPlayer* pl)
{
    switch (pl->xFF) {
    case 0:
        pl0fBossDiePosSet(pl);
        plboatSetSpear(pl);
        MotionSetCore(pl, &pl->mot, PLARC(0xE), 0, 0, 1, 0);
        pl0fBossDieCamSet(pl);
        pl->x3E0 = 300;
        pl->xFF++;
    case 1:
        plOnBoat(pl);
        MotionMoveF(pl, 0);
        if (pl->x3E0) {
            pl->x3E0--;
        } else {
            pl->xFF++;
        }
        break;
    case 2:
        MotionSetCore(pl, &pl->mot, PLARC(0x13), 0, 0xA, 1, 0);
        pl->x3EC = 99999;
        pl->xFF++;
    case 3:
        plOnBoat(pl);
        if (pl->frame > 13.7f && pl->frame < 14.3f) {
            if (pl->pSpear) {
                pl->pSpear->setLost();
                pl->pSpear = 0;
            }
        }
        if (MotionMoveF(pl, 0)) {
            PlRoutineSet(pPL, 0, 0xF, 2, 0);
        }
        break;
    }
    pl0fBossDieCamMove(pl);
}

static void plboat_R2_Guard(cPlayer* pl)
{
    switch (pl->xFF) {
    case 0:
        MotionSetCore(pl, &pl->mot, PLARC(0x1E), 0, 0xA, 1, 0);
        pl->x4FD = 0xA;
        pl->x4FC = 0;
        pl->blendRate500 = 0.0f;
        if (pl->pSpear) {
            pl->pSpear->setLost();
            pl->pSpear = 0;
        }
        pl->xFF++;
    case 1:
        plOnBoat(pl);
        if (MotionMoveF(pl, 0)) {
            PlRoutineSet(pPL, 0, 0xF, 2, 0);
        }
        break;
    }
    if (pl->pBoat) {
        pl0fBossCamMove(PL_BOAT(pl), 0);
    }
}

static void plboat_R2_FallWater(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    pG->flags_5010 |= 0x00400000;
    switch (pl->xFF) {
    case 0:
        MotionSetCore(pl, &pl->mot, PLARC(0x20), 0, 3, 1, 0);
        pl->rot.x = 0.0f;
        pl->rot.z = 0.0f;
        pl->blendRate500 = 0.0f;
        LifeDownSet2(pl, 500, 0, 1);
        VibSetData(VIB_TBL, 0xB, 1);
        if (pl->pSpear) {
            pl->pSpear->setLost();
            pl->pSpear = 0;
        }
        pl->x3E0 = 65;
        pl->x3E4 = 23;
        pl->x400 = pl->rot.y;
        pl->x3EC = 0;
        pPLS->endCamera();   // struct view: the pPL load stays below the four stores
        pl00SetDropCam(pl);
        EstSet((int) pl, -1, 0, 0, 0xF, 0x15, 0, 0x35, (u32) boat, 0);
        pl->x3E8 = 26;
        pl->setRightHand(0);
        SndCall(8, 0x16, &pl->pos, 0xF, 0, 0);
        if ((s16) pG->pl_life <= 0) {
            PlSetDamageSe(0xD);
        } else {
            PlSetDamageSe(0);
        }
        pl->xFF++;
    case 1:
        if (pl->x3E0) {
            pl->x3E0--;
        } else {
            pl->rot.y += Muku(&pl->pos, &pl->pBoat->pos, pl->rot.y, 0.09817477f);
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        }
        if (pl->x3E4) {
            Vec v;

            pl->x3E4--;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 0.0f;
            PSMTXMultVec(pl->pBoat->mat, &v, &pl->pos);
            if (pl->x3E4 == 0) {
                pl->pos.y += 150.0f;
            }
        }
        if (pl->x3E8) {
            pl->x3E8--;
            if (pl->x3E8 == 0) {
                EstSet(0, -1, &pl->pos, 0, 0xF, 0x14, 0, 0x35, (u32) pl, 0);
                SndCall(8, 2, &pl->pos, 0xF, 0, 0);
                SndStrVolSet(0, 5, 70, 1);
            }
        }
        if (pl->frame > 122.7f && pl->frame < 123.3f) {
            SndCall(8, 3, &pl->pos, 0xF, 0, 0);
            SndStrVolReset(0, 5, 1);
        }
        if (MotionMoveF(pl, 0)) {
            if ((s16) pG->pl_life > 0) {
                PlRoutineSet(pPL, 0, 0xF, 6, 0);
            }
        }
        break;
    }
    pl00DropCamMove(pl);
}

static void plboat_R2_Swim(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    int first = 0;
    int one;
    Vec v;
    f32 h;

    if (boat) {
        Pl0fWork* w = PL0F_WK(boat);

        if (w->x6D == 0) {
            w->x6D = 1;
            first = 1;
        }
    }
    // COMPILER-DIFF: #13 -- single-use constant set in another block: update_equiv_regs moves the
    // `li` next to the x3E4 store (shortest qty -> r9 before the pG pointer) and the store's source
    // crosses the calls, so it gets the TRUE store->call link and is issued first (source order).
    one = 1;
    pG->flags_5010 |= 0x00400000;
    switch (pl->xFF) {
    case 0:
        pl->rot.x = 0.0f;
        pl->rot.z = 0.0f;
        pl0fSwimPosSet(pl);
        EffectEspDelete(0, 0x34, (u32) pl, 0);
        EffectEspgenDelete(0, 0x34, (int) pl);
        EffectEfmDelete(0, 0x34, (int) pl);
        pG->flags_5010 &= ~0x00100000;
        pl->x3E4 = one;
        pl->x3E0 = 0;
        pl->x3F0 = 0;
        pl->x3E8 = 4;
        if ((Rnd() & 1) || first) {
            pl00SetSwimCam(pl);
            pl->x3EC = 0;
        } else {
            pl00SetChaseCam(pl);
            pl->x3EC = 90;
            EstSet(0, -1, 0, 0, 0xF, 0xE, 0, 0x34, (u32) pl, (void*) first);
            pG->flags_5010 |= 0x00100000;
        }
        MotionSetCore(pl, &pl->mot, PLARC(0x14), (int) PLARC(0x15), 5, 5, 0);
        pl->evTarget.x = 0.0f;
        pl->evTarget.y = 0.0f;
        pl->evTarget.z = 50.0f;
        pl->rot.y += Muku(&pl->pos, &pl->pBoat->pos, pl->rot.y, PI);
        pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        pl->xFF++;
    case 1: {
        int n = (int) pl->x3E0 / 20;
        int lim;

        if (n > 7) {
            n = 7;
        }
        if (n != pl->x3E4) {
            void* m;
            f32 rate;
            u32 cnt;
            u32 f;

            pl->x3E4 = n;
            switch (n) {
            case 0:
            default:
                m = PLARC(0x15);
                pl->evTarget.z = 50.0f;
                break;
            case 1:
                m = PLARC(0x16);
                pl->evTarget.z = 62.5f;
                break;
            case 2:
                m = PLARC(0x17);
                pl->evTarget.z = 75.0f;
                break;
            case 3:
                m = PLARC(0x18);
                pl->evTarget.z = 87.5f;
                break;
            case 4:
                m = PLARC(0x19);
                pl->evTarget.z = 100.0f;
                break;
            case 5:
                m = PLARC(0x1A);
                pl->evTarget.z = 120.5f;
                break;
            case 6:
                m = PLARC(0x1B);
                pl->evTarget.z = 140.0f;
                break;
            case 7:
                m = PLARC(0x1C);
                pl->evTarget.z = 170.0f;
                break;
            }
            rate = pl->frame / (f32) pl->frameMax;
            cnt = *(u16*) m;
            f = (u32) ((f32) cnt * rate) + 1;
            if (f >= cnt) {
                f = 0;
            }
            MotionSetCore(pl, &pl->mot, PLARC(0x14), (int) m, pl->x29D, 5, (u16) f);
        }
        if (pl->motEvent & 0x40) {
            EstSet((int) pl, -1, 0, 0, 0xF, 0x11, 0, 0x35, (u32) boat, 0);
            SndCall(8, 0x1A, &pl->pos, 0xF, 0, 0);
        }
        if (pl->motEvent & 0x80) {
            EstSet((int) pl, -1, 0, 0, 0xF, 0x12, 0, 0x35, (u32) boat, 0);
            SndCall(8, 0x19, &pl->pos, 0xF, 0, 0);
        }
        if (pl->x3E8) {
            pl->x3E8--;
        } else {
            pl->x3E8 = 3;
            EstSet((int) pl, -1, 0, 0, 0xF, 0x10, 0, 0x35, (u32) boat, 0);
        }
        lim = 8;
        if (pG->x4F88 <= 3) {
            lim = 12;
        }
        if (pG->x4F88 > 6) {
            lim = 4;
        }
        pl->x3F0++;
        if (pl->x3F0 > lim) {
            pl->x3F0 = lim;
            if (pl->x3E0) {
                pl->x3E0--;
            }
        }
        if (Key.trg & 0x80000) {
            if ((s16) pG->pl_life <= 1) {
                pl->x3E0 += pl->x3F0 - pl->x3F0 / 2;
            } else {
                pl->x3E0 += pl->x3F0;
            }
            pl->x3F0 = 0;
            if ((int) pl->x3E0 > 159) {
                pl->x3E0 = 159;
            }
        }
        PSMTXMultVecSR(pl->mat, &pl->evTarget, &v);
        PSVECAdd(&pl->pos, &v, &pl->pos);
        MotionMoveF(pl, 0);
        break;
    }
    }
    if (GetWaterHeight(&pl->pos, &h)) {
        pl->pos.y = h - 100.0f;
    }
    if (pl->pBoat->plDist2 < 3240000.0f) {
        PlRoutineSet(pPL, 0, 0xF, 8, 0);
        pl->pBoat->xFC = 1;
        pl->pBoat->xFD = 5;
        pl->pBoat->xFE = 0;
        pl->pBoat->xFF = 0;
    }
    if (pl->x3EC) {
        pl->x3EC--;
        if (pl->x3EC == 0) {
            pl00SetSwimCam(pl);
            EffectEspDelete(0, 0x34, (u32) pl, 0);
            EffectEspgenDelete(0, 0x34, (int) pl);
            EffectEfmDelete(0, 0x34, (int) pl);
            pG->flags_5010 &= ~0x00100000;
        }
    }
    if (pl->x3EC) {
        pl00ChaseCamMove(pl);
    } else {
        pl00SwimCamMove(pl);
    }
    ActBtn.set(0x11, 5, 0, 0, 0, 2, 0, 0);
}

static Vec plboat_ride_pos;
static f32 plboat_ride_ang;

static void plboat_R2_WaterRide(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    Mtx m;
    Vec v;

    switch (pl->xFF) {
    case 0:
        MotionSetCore(pl, &pl->mot, PLARC(0x21), 0, 0xF, 5, 0);
        pl->sightRate = 0.0f;
        pl->blendRate500 = 0.0f;
        PSMTXRotRad(m, 'y', boat->rot.y);
        TransMatrix(m, &boat->pos);
        v.x = 1604.49f;
        v.y = 0.0f;
        v.z = -50.0f;
        PSMTXMultVec(m, &v, &v);
        PSVECSubtract(&v, &pl->pos, &pl->evTarget);
        pl->rot.y = pl->pBoat->rot.y - PI / 2;
        pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        EstSet((int) pl, -1, 0, 0, 0xF, 0x16, 0, 0x35, (u32) boat, 0);
        plboat_ride_pos = boat->pos;
        plboat_ride_ang = boat->rot.y;
        pl->xFF++;
    case 1:
        PSVECScale(&pl->evTarget, &v, 0.3f);
        PSVECAdd(&pl->pos, &v, &pl->pos);
        PSVECSubtract(&pl->evTarget, &v, &pl->evTarget);
        PSVECSubtract(&boat->pos, &plboat_ride_pos, &v);
        plboat_ride_pos = boat->pos;
        PSVECAdd(&pl->pos, &v, &pl->pos);
        pl->rot.y += Muku2(plboat_ride_ang, boat->rot.y, PI);
        plboat_ride_ang = boat->rot.y;
        if (MotionMoveF(pl, 0)) {
            pl->pBody->initWepHand((u32) PLARC(0x8));
            pl->setRightHand(1);
            pl->pWep->setTrans(0, 0);
            PlRoutineSet(pPL, 0, 0xF, 2, 0);
        }
        break;
    }
    pl00SwimCamMove(pl);
}

static void plboat_R2_Die(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    cEm* boss;

    if (boat) {
        // The work pointer local keeps the then-arm two insns at jump1 time, so the `boss = 0` hoist
        // happens in jump2 (after sched2) and the `li` stays between the compare and the branch.
        Pl0fWork* w = PL0F_WK(boat);
        boss = w->pBoss;
    } else {
        boss = 0;
    }
    switch (pl->xFF) {
    case 0: {
        int eaten = 0;

        MotionSetCore(pl, &pl->mot, PLARC(0x28), 0, 0, 5, 0);
        if (boss && (boss->flags_3C8 & 0x80) && (Rnd() & 1)) {
            eaten = 1;
        }
        if (eaten) {
            pl00SetDieCam(pl);
            pl->x3E0 = 1;
            pl->x3E4 = 60;
            if (boss) {
                Em2fWorkView* bw = (Em2fWorkView*) &boss->x3E0;

                EffectEspDelete(0, bw->espKind, (u32) boss, 0);
                EffectEspgenDelete(0, bw->espKind, (int) boss);
                EffectEfmDelete(0, bw->espKind, (int) boss);
                EstSet((int) boss, -1, 0, 0, 0x27, 0xA, 0, 0, (u32) boss, 0);
            }
        } else {
            pl->x3E0 = eaten;
            pl->x3E4 = 1;
        }
        VibSetData(VIB_TBL, 0xD, 1);
        pl->xFF++;
    }
    case 1:
        if (boss) {
            pl->pos.x = 0.0f;
            pl->pos.y = -300.0f;
            pl->pos.z = 700.0f;
            pl->rot.x = PI / 2;
            pl->rot.y = 0.0f;
            pl->rot.z = 0.0f;
            RotMatrix(pl->mat, &pl->rot);
            TransMatrix(pl->mat, &pl->pos);
            ScaleMatrix(pl->mat, &pl->scale);
            PSMTXConcat(boss->getPartsPtr(8)->mat, pl->mat, pl->mat);
            pl->motFlags2 |= 0x40000000;
        }
        MotionMoveF(pl, 0);
        if (pl->x3E4) {
            pl->x3E4--;
            if (pl->x3E4 == 0) {
                pG->pl_life = 0;
            }
        }
        break;
    }
    if (pl->x3E0) {
        pl00DieCamMove(pl);
    } else {
        pl00SwimCamMove(pl);
    }
}

// R10d / R10e entrance: the player sits and steers (the boat drives itself, pl0f_R1_R10xIn). A macro,
// not a static inline: integrate.c copies the inlined body's MEMs without RTX_UNCHANGING_P, so an inlined
// pool load (`lfs 0.0`, `lfs 1.0`) gets a true dependence on every preceding store through `pl` and sinks
// below them; the original issues both loads above the stores (stw x3FC, stb x4FC, stfs, ... / lis, subi,
// lfs, mr, stw).
#define PLBOAT_ROOM_IN() \
{ \
    switch (pl->xFF) { \
    case 0: \
        pl->pBody->initWepHand((u32) PLARC(0x8)); \
        pl->setRightHand(1); \
        pl->pWep->setTrans(0, 0); \
        pl->x3FC = 1; \
        pl->x4FD = 0; \
        pl->x4FC = 0; \
        pl->blendRate500 = 0.0f; \
        pl->xFF++; \
    case 1: \
        plboatBlendMotSet(pl, PLARC(0x9), PLARC(0xB), PLARC(0xA), 0, 0, 0); \
        plOnBoat(pl); \
        MotionMoveF(pl, 0); \
        break; \
    } \
    if (pl->x3FC) { \
        pl->x3FC--; \
        pl0fRideCamMove(boat, 1.0f); \
    } else { \
        pl0fRideCamMove(boat, 0.3f); \
    } \
}

static void plboat_R2_R10dIn(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    PLBOAT_ROOM_IN();
}

// R10d / R10e exit: the player boards the boat at the landing and it leaves the room. A macro, not an
// inline with f32 parameters: the room position literals must enter the constant pool at their use in
// case 2 (an inline's constant arguments are expanded first and head the pool).
#define PLBOAT_ROOM_OUT(px, py, pz, ang) \
{ \
    Vec v; \
 \
    switch (pl->xFF) { \
    case 0: \
        v.x = 1500.0f; \
        v.y = 500.0f; \
        v.z = 100.0f; \
        PSMTXMultVec(boat->mat, &v, &v); \
        pl->pos.x = v.x; \
        pl->pos.z = v.z; \
        pl->x3FC = 1; \
        pl->x400 = v.y - pl->pos.y; \
        pl->x3E0 = 20; \
        pl->rot.y = boat->rot.y - PI / 2; \
        pl->rot.y = LIMIT_ANGLE(pl->rot.y); \
        MotionSetCore(pl, &pl->mot, PLARC(0x29), 0, 0, 5, 0); \
        pl->sightRate = 0.0f; \
        pl->blendRate500 = 0.0f; \
        pl->pBody->initWepHand((u32) PLARC(0x8)); \
        pl->setRightHand(1); \
        pl->pWep->setTrans(0, 0); \
        pl->xFF++; \
    case 1: \
        if (pl->x3FC) { \
            pl->x3FC--; \
            pl0fRideCamMove(boat, 1.0f); \
        } else { \
            pl0fRideCamMove(boat, 0.05f); \
        } \
        if (pl->x3E0) { \
            pl->x3E0--; \
        } else { \
            f32 dy = pl->x400 * 0.1f; \
 \
            pl->pos.y += dy; \
            pl->x400 -= dy; \
        } \
        if (pl->frame > 25.7f && pl->frame < 26.3f) { \
            SndCall(8, 4, &pl->pos, boat->id, 0, 0); \
        } \
        if (pl->frame > 27.7f && pl->frame < 28.3f) { \
            SndCall(8, 5, &pl->pos, boat->id, 0, 0); \
        } \
        if (MotionMoveF(pl, 0)) { \
            PLBOAT_ENGINE_START(); \
            pl->xFF++; \
        } \
        break; \
    case 2: \
        pl->x4FD = 0; \
        pl->x4FC = 0; \
        pl->blendRate500 = 0.0f; \
        pl->pos.x = px; \
        pl->pos.y = py; \
        pl->pos.z = pz; \
        pl->rot.y = ang; \
        boat->setPos(&pl->pos, ang); \
        EffectEspDelete(0, 0x35, (u32) boat, 0); \
        EffectEspgenDelete(0, 0x35, (int) boat); \
        EffectEfmDelete(0, 0x35, (int) boat); \
        pl->x3E0 = 0; \
        pl->xFF++; \
    case 3: \
        pl->x3E0++; \
        if (pl->x3E0 & 1) { \
            EstSet((int) boat, -1, 0, 0, 1, 0xA, 0, 0x35, (u32) boat, 0); \
        } \
        if ((int) pl->x3E0 % 20 == 0) { \
            Vec p; \
 \
            p = pl->pos; \
            p.y += 500.0f; \
            SndCall(8, 0x12, &p, 0xF, 0, 0); \
        } \
        boat->setTillerFront(); \
        pl0fRideCamMove(boat, 1.0f); \
        plboatBlendMotSet(pl, PLARC(0x9), PLARC(0xB), PLARC(0xA), 0, 0, 0); \
        plOnBoat(pl); \
        MotionMoveF(pl, 0); \
        break; \
    } \
}

static void plboat_R2_R10dOut(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    PLBOAT_ROOM_OUT(30.0f, -2490.0f, -14520.0f, -0.05043f);
}

static void plboat_R2_R10eIn(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    PLBOAT_ROOM_IN();
}

static void plboat_R2_R10eOut(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    PLBOAT_ROOM_OUT(36030.0f, -15000.0f, 54920.0f, -1.570221f);
}

static void plboat_R2_R10eIn2(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    PLBOAT_ROOM_IN();
}

static void plboat_R2_R10eOut2(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);

    PLBOAT_ROOM_OUT(-42720.0f, -15000.0f, 42580.0f, 1.57f);
}

static Vec pl00_swim_cam_pos = { -3500.0f, 2000.0f, -2000.0f };
static Vec pl00_swim_cam_at = { 5000.0f, -500.0f, 500.0f };
static Vec pl00_chase_cam_ofs = { 0.0f, -2000.0f, -40000.0f };
static Camera pl00_drop_camera = { 0 };

void pl00SetSwimCam(cPlayer* pl)
{
    Mtx m;

    PSMTXRotRad(m, 'y', pl->pBoat->rot.y);
    TransMatrix(m, &pl->pBoat->pos);
    PSMTXMultVec(m, &pl00_swim_cam_pos, &pl0f_camera.param.pos);
    PSMTXMultVec(m, &pl00_swim_cam_at, &pl0f_camera.param.at);
    pl0f_camera.param.fovy = 40.0f;
    pl0f_camera.up.x = 0.0f;
    pl0f_camera.up.y = 1.0f;
    pl0f_camera.up.z = 0.0f;
    CAM_SET(pl0f_camera);
}

void pl00SwimCamMove(cPlayer* pl)
{
    Camera* gcam = &pG->Cam;
    Vec pos;
    Vec at;

    BitOn(pG->flags_5010, 0x00080000);
    if ((s16) pG->pl_life <= 0) {
        cEm* boss;

        if (pl->pBoat) {
            Pl0fWork* w = PL0F_WK(pl->pBoat);   // two insns at jump1: the `boss = 0` arm is not hoisted
            boss = w->pBoss;
        } else {
            boss = 0;
        }
        if (boss) {
            at = boss->getPartsPtr(7)->worldPos;
        } else {
            at = gcam->param.at;
        }
        pos = gcam->param.pos;
        PosToPos(&gcam->param.at, &at, &pl0f_camera.param.at, 1.0f);
        PosToPos(&gcam->param.pos, &pos, &pl0f_camera.param.pos, 1.0f);
    }
    pl0f_camera.param.fovy = pl0f_camera.param.fovy * 0.9f + 4.0f;
    pl0f_camera.up.x = 0.0f;
    pl0f_camera.up.y = 1.0f;
    pl0f_camera.up.z = 0.0f;
    CAM_SET(pl0f_camera);
    CamCtrl.x250 = (s32) &pl0f_camera;
}

void pl00SetChaseCam(cPlayer* pl)
{
    Mtx m;

    pl0f_camera.param.at = pl->pos;
    PSMTXRotRad(m, 'y', pl->rot.y);
    TransMatrix(m, &pl->pos);
    PSMTXMultVec(m, &pl00_chase_cam_ofs, &pl0f_camera.param.pos);
    pl0f_camera.param.fovy = 40.0f;
    pl0f_camera.up.x = 0.0f;
    pl0f_camera.up.y = 1.0f;
    pl0f_camera.up.z = 0.0f;
    CAM_SET(pl0f_camera);
}

void pl00ChaseCamMove(cPlayer* pl)
{
    Vec d;

    pG->flags_5010 |= 0x00080000;
    pl0f_camera.param.at = pl->pos;
    PSVECSubtract(&pl0f_camera.param.at, &pl0f_camera.param.pos, &d);
    d.y = 0.0f;
#line 4642
    VECNormalize(&d, &d);
    PSVECScale(&d, &d, 260.0f);
    PSVECAdd(&pl0f_camera.param.pos, &d, &pl0f_camera.param.pos);
    pl0f_camera.param.fovy = pl0f_camera.param.fovy * 0.9f + 4.0f;
    pl0f_camera.up.x = 0.0f;
    pl0f_camera.up.y = 1.0f;
    pl0f_camera.up.z = 0.0f;
    CAM_SET(pl0f_camera);
    CamCtrl.x250 = (s32) &pl0f_camera;
}

void pl00SetDieCam(cPlayer* pl)
{
    Mtx m;
    Vec v;

    pl0f_camera.param.pos = pPL->pos;
    pl0f_camera.param.at = pPL->pos;
    pl0f_camera.param.pos.y = 23000.0f;
    pl0f_camera.param.fovy = 40.0f;
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1.0f;
    PSMTXRotRad(m, 'y', pl->rot.y);
    PSMTXMultVecSR(m, &v, &pl0f_camera.up);
    CAM_SET(pl0f_camera);
}

void pl00DieCamMove(cPlayer* pl)
{
    Mtx m;
    Vec v;
    cModel* p;

    BitOn(pG->flags_5010, 0x00080000);   // reference store: the pPL load stays below it
    p = pPL->getPartsPtr(0);
    pl0f_camera.param.pos = p->worldPos;
    pl0f_camera.param.at = p->worldPos;
    pl0f_camera.param.pos.y = 14000.0f;
    pl0f_camera.param.fovy = 40.0f;
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1.0f;
    PSMTXRotRad(m, 'y', pl->rot.y);
    PSMTXMultVecSR(m, &v, &pl0f_camera.up);
    CAM_SET(pl0f_camera);
    CamCtrl.x250 = (s32) &pl0f_camera;
}

void pl00SetDropCam(cPlayer* pl)
{
    Vec v;   // frame offset 0: recomputed per call; `m` behind it gets the callee-saved address pseudo
    Mtx m;

    PSMTXRotRad(m, 'y', pl->rot.y);
    TransMatrix(m, &pl->pos);
    if (Rnd() & 1) {
        v.x = 1500.0f;
        v.y = 0.0f;
        v.z = -9000.0f;
    } else {
        v.x = 8000.0f;
        v.y = 0.0f;
        v.z = -4000.0f;
    }
    PSMTXMultVec(m, &v, &pl0f_camera.param.pos);
    pl0f_camera.param.at = pl->pos;
    pl0f_camera.param.fovy = 40.0f;
    pl0f_camera.up.x = 0.0f;
    pl0f_camera.up.y = 1.0f;
    pl0f_camera.up.z = 0.0f;
    CAM_SET(pl0f_camera);
}

void pl00DropCamMove(cPlayer* pl)
{
    Camera* gcam = &pG->Cam;
    Vec at;
    Vec pos;
    f32 h;

    at = pl0f_camera.param.pos;
    at.y = pl->pos.y + 1000.0f;
    pos = pl->pos;
    pl00_drop_camera.param.fovy = 40.0f;
    PosToPos(&gcam->param.at, &pos, &pl00_drop_camera.param.at, 1.0f);
    PosToPos(&gcam->param.pos, &at, &pl00_drop_camera.param.pos, 1.0f);
    pl00_drop_camera.up.x = 0.0f;
    pl00_drop_camera.up.y = 1.0f;
    pl00_drop_camera.up.z = 0.0f;
    CAM_SET(pl00_drop_camera);
    CamCtrl.x250 = (s32) &pl00_drop_camera;
    pG->flags_5010 &= ~0x00100000;
    if (GetWaterHeight(&at, &h)) {
        switch (pl->x3EC) {
        case 0:
            if (h > at.y) {
                EstSet(0, -1, 0, 0, 0xF, 0xA, 0, 0x34, (u32) pl, 0);
                pG->flags_5010 |= 0x00100000;
                pl->x3F0 = 10;
                pl->x3EC++;
            }
            break;
        case 1:
            if (pl->x3F0) {
                pl->x3F0--;
            } else {
                pG->flags_5010 |= 0x00100000;
                if (h <= at.y - 300.0f) {
                    pl->x3EC++;
                }
            }
            break;
        case 2:
            pG->flags_5010 |= 0x00100000;
            if (h <= at.y) {
                EffectEspDelete(0, 0x34, (u32) pl, 0);
                EffectEspgenDelete(0, 0x34, (int) pl);
                EffectEfmDelete(0, 0x34, (int) pl);
                pG->flags_5010 &= ~0x00100000;
                pl->x3EC++;
            }
            break;
        }
    }
}

void plboatSetSpear(cPlayer* pl)
{
    Vec pos;
    Vec rot;

    if (pl->pSpear == 0) {
        SndCall(8, 0, &pl->pos, 0xF, 0, 0);
        pos.x = -70.0f;
        pos.y = -30.0f;
        pos.z = -500.0f;
        rot.x = 0.0f;
        rot.y = PI;
        rot.z = 0.0f;
        pl->pSpear = (cObjSpear*) SetSpear(PLARC(0x7), PLARC(0x6), &pos, &rot);
        if (pl->pSpear) {
            pl->pSpear->setParent(pl, 0xA, 0);
        }
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

// Seats the player on the boat: position from the boat's matrix, the matrix and rotation copied,
// the waist follows the sight.
void plOnBoat(cPlayer* pl)
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
    pl->pWaist->set(pl->sightRate, 0.4f);
}

// Finds the living Del Lago (em id 0x2F, x38D == 1) and hooks the boat to it.
int testSearchEm2f(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    u32 n;

    w->pBoss = 0;
    for (n = 0; n < EmMgr.nArray; n++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * n);

        if ((e->be_flag & 0x201) == 1 && e->id == 0x2F && (s16) e->hp > 0 && e->x38D == 1) {
            Vec v;
            f32 len;
            int i;

            Pl0fNode* n = &w->node[0];   // node pointer kept callee-saved across the calls; `w` itself dies before them

            w->pBoss = e;
            em->xFC = 1;   // plain byte stores: the 6 stays in the loop, the zero is hoisted (an int inline hoists both)
            em->xFD = 6;
            em->xFE = 0;
            em->xFF = 0;
            {
                // A second work pointer: cse turns it into a copy of `w` that loop.c hoists (`mr r12, r6`
                // before the loop) and the pSelf store goes through the copy.
                Pl0fWork* w2 = PL0F_WK(em);

                w2->pSelf = em;
            }
            for (i = 0; i < 10; i++) {
                w->hist[i] = e->pos;
            }
            w->histIdx = 0;
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = -2000.0f;
            PSMTXMultVec(w->pBoss->mat, &v, &v);
            len = SQRTF((n->wpos.x - v.x) * (n->wpos.x - v.x) + (n->wpos.z - v.z) * (n->wpos.z - v.z));
            if (len > 25000.0f) {
                n->maxLen = len;
            } else {
                n->maxLen = 25000.0f;
            }
            return 1;
        }
    }
    return 0;
}

// Screen position of the harpoon sight from the lean / tilt rates.
void plboatSightCurGet(cPlayer* pl, Vec* out)
{
    out->x = 256.0f - pl->sightRate * 488.92398f;
    out->y = 180.0f - pl->blendRate500 * 0.390625f;
    out->z = 0.0f;
}

void plboatSightCurMove(cPlayer* pl)
{
    Vec p;

    plboatSightCurGet(pl, &p);
    EstSet(0, -1, &p, 0, 0xF, 6, 0, 0x35, (u32) pl, 0);
}

void plboatSpearThrow(cPlayer* pl)
{
    cObjSpear* spear = pl->pSpear;
    Camera* gcam = &pG->Cam;
    Vec cur;
    Vec dir;
    Vec target;
    Vec hand;
    cModel* p;

    if (spear == 0) {
        return;
    }
    plboatSightCurGet(pl, &cur);
    CamPos2ScrnVec(&dir, cur.x, cur.y);
#line 5166
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, 25000.0f);
    PSVECAdd(&gcam->param.pos, &dir, &target);
    hand.x = -70.0f;
    hand.y = -30.0f;
    hand.z = -500.0f;
    p = pl->getPartsPtr(0xA);
    PSMTXMultVec(p->mat, &hand, &hand);
    PSVECSubtract(&target, &hand, &dir);
#line 5177
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, 2000.0f);
    dir.y += 50.0f;
    SndCall(8, 1, &p->worldPos, 0xF, 0, 0);
    spear->setThrow(&dir);
    pl->pSpear = 0;
}

static u8 pl0f_rope_parts[30] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
};
static u8 pl0f_rope_up[30] = {
    0xFF, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
};
static u8 pl0f_rope_down[30] = {
    2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 0xFF,
};

// The anchor rope: a 30-link chain object hung on the boat's parts 5.
void pl0fLongRopeSet(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    Vec pos;
    Vec rot;

    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    w->pRope = SetChain(ARC(0x23), ARC(0x24), &pos, &rot);
    if (w->pRope) {
        w->cloth.num = 30;
        w->cloth.pParts = pl0f_rope_parts;
        w->cloth.x58 = em;
        w->cloth.x48 = 0.0f;
        w->cloth.x50 = 0.0f;
        w->cloth.pUp = pl0f_rope_up;
        w->cloth.pDown = pl0f_rope_down;
        w->cloth.x3C = 5.0f;
        w->cloth.x40 = 0.9f;
        w->cloth.x44 = 100;
        w->cloth.x4C = 0.1f;
        w->cloth.flags = 0x108;
        w->cloth.x08 = 0;
        w->cloth.x0C = 0;
        w->cloth.x10 = 0;
        w->cloth.x14 = 0;
        w->cloth.pMax = 0;
        w->cloth.x2C = 0;
        w->cloth.x30 = 0;
        w->cloth.x34 = 0;
        w->cloth.x20 = 0;
        w->cloth.x24 = 0;
        w->cloth.x38 = 0;
        w->cloth.x54 = 0;   // the last zero store (dying zero register) is issued first
        w->pRope->setChain(&w->cloth);
        pos.x = 0.0f;
        pos.y = 600.0f;
        pos.z = 2550.0f;
        w->pRope->setParent(em, 0, &pos, 0);
    }
}

// Where the player surfaces after the drop: a random spot around the boat.
void pl0fSwimPosSet(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    Vec v;
    f32 ang;
    f32 dist;

    v.x = 37460.0f;
    v.y = boat->pos.y;
    v.z = 63360.0f;
    if (Rnd() & 1) {
        ang = -2.0f;
    } else {
        ang = 1.0f;
    }
    ang = fRand1_1() * 0.049087385f + ang;
    boat->setPos(&v, ang);
    dist = 15000.0f;
    if ((s16) boat->hp <= 899) {
        if ((s16) pG->pl_life <= 799) {
            if (Rnd() & 1) {
                dist = 17000.0f;
            } else {
                dist = 19000.0f;
            }
        }
        if ((s16) pG->pl_life <= 399) {
            if (Rnd() & 1) {
                dist = 24000.0f;
            } else {
                dist = 26000.0f;
            }
        }
        if ((s16) pG->pl_life <= 1) {
            if (Rnd() & 1) {
                dist = 26000.0f;
            } else {
                dist = 28000.0f;
            }
        }
    }
    v.x = dist;
    v.y = pl->pos.y;
    v.z = 0.0f;
    PSMTXMultVec(boat->mat, &v, &pl->pos);
    pl->oldPos = pl->pos;
    pl->rot.y = GetXZAngle(&pl->pos, &boat->pos);
    RotMatrix(pl->mat, &pl->rot);
    TransMatrix(pl->mat, &pl->pos);
    RotMatrix(pl->worldMat, &pl->rot);
    TransMatrix(pl->worldMat, &pl->pos);
    ScaleMatrix(pl->worldMat, &pl->scale);
    PSMTXCopy(pl->worldMat, pl->mat);
    EffectEspDelete(0, 0x35, (u32) boat, 0);
    EffectEspgenDelete(0, 0x35, (int) boat);
    EffectEfmDelete(0, 0x35, (int) boat);
}

void pl0fHidePosSet(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    Vec v;
    f32 ang;

    v.x = 40000.0f;
    v.y = boat->pos.y;
    v.z = 40000.0f;
    if (Rnd() & 1) {
        ang = -2.45f;
    } else {
        ang = 0.59f;
    }
    ang = fRand1_1() * 0.049087385f + ang;   // the inner call into the local first: the vtable lookup follows it
    boat->setPos(&v, ang);
    pl->pos = boat->pos;
    pl->rot.y = boat->rot.y;
    EffectEspDelete(0, 0x35, (u32) boat, 0);
    EffectEspgenDelete(0, 0x35, (int) boat);
    EffectEfmDelete(0, 0x35, (int) boat);
}

void pl0fBossDiePosSet(cPlayer* pl)
{
    cPl0f* boat = PL_BOAT(pl);
    Vec v;

    v.x = 24250.0f;
    v.y = boat->pos.y;
    v.z = 92250.0f;
    pl->rot.y = 2.8464928f;
    pl->rot.y = LIMIT_ANGLE(pl->rot.y);
    boat->setPos(&v, pl->rot.y);   // the just-stored member is forwarded: f1 passes straight through
    pl->pos = boat->pos;
    EffectEspDelete(0, 0x35, (u32) boat, 0);
    EffectEspgenDelete(0, 0x35, (int) boat);
    EffectEfmDelete(0, 0x35, (int) boat);
}

// The anchor object hung on the boat (room 10B only).
void pl0fSetAnchor(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);
    Vec pos;
    Vec rot;

    if (w->pAnchor == 0 && pG->room_id == 0x10B) {
        pos.x = 0.0f;
        pos.y = 700.0f;
        pos.z = 2300.0f;
        rot.x = 2.3f;
        rot.y = 3.14f;
        rot.z = 0.0f;
        w->pAnchor = SetObj00(ARC(0x25), ARC(0x26), &pos, &rot);
        OyaSetObj00(w->pAnchor, em, 0);
    }
}

// Moves the anchor onto the boss (parts 0x1A) once it is hooked.
void pl0fSetAnchorEm2f(cPl0f* em)
{
    Pl0fWork* w = PL0F_WK(em);

    if (w->pBoss && w->pAnchor) {
        w->pAnchor->pos.x = 0.0f;
        w->pAnchor->pos.y = 1000.0f;
        w->pAnchor->pos.z = 0.0f;
        w->pAnchor->rot.x = 0.0f;
        w->pAnchor->rot.y = 0.0f;
        w->pAnchor->rot.z = 0.0f;
        OyaSetObj00(w->pAnchor, w->pBoss, 0x1A);
    }
}

// Rope effects while the boss pulls: the rope strain effect left (0x1F) or right (0x1E) of the boat.
void pl0fSetAnchorEm2f(cPlayer* pl)
{
    Pl0fWork* w;
    cEm* boss;
    f32 ang;

    if (pl->pBoat == 0) {
        return;
    }
    w = PL0F_WK(pl->pBoat);
    boss = w->pBoss;
    if (boss == 0) {
        EffectEspDelete(0, 0x36, (u32) pl, 0);
        EffectEspgenDelete(0, 0x36, (int) pl);
        EffectEfmDelete(0, 0x36, (int) pl);
        w->anchorEff = 0;
        return;
    }
    // Two separate ifs: each body is the fall-through of its own test, so cse stores the known-zero `andi.`
    // result (r28) and jump2 merges the two identical bodies; an `||` body starts at a label and gets `li r0,0`.
    if (!(boss->flags_3C8 & 0x20)) {
        EffectEspDelete(0, 0x36, (u32) pl, 0);
        EffectEspgenDelete(0, 0x36, (int) pl);
        EffectEfmDelete(0, 0x36, (int) pl);
        w->anchorEff = 0;
        return;
    }
    if (!(boss->flags_3C8 & 4)) {
        EffectEspDelete(0, 0x36, (u32) pl, 0);
        EffectEspgenDelete(0, 0x36, (int) pl);
        EffectEfmDelete(0, 0x36, (int) pl);
        w->anchorEff = 0;
        return;
    }
    ang = Muku(&pl->pos, &boss->pos, pl->rot.y, PI);
    if (fabsf(ang) < PI / 8) {
        EffectEspDelete(0, 0x36, (u32) pl, 0);
        EffectEspgenDelete(0, 0x36, (int) pl);
        EffectEfmDelete(0, 0x36, (int) pl);
        w->anchorEff = 0;
        return;
    }
    if (ang < 0.0f) {
        if (w->anchorEff != 1) {
            EffectEspDelete(0, 0x36, (u32) pl, 0);
            EffectEspgenDelete(0, 0x36, (int) pl);
            EffectEfmDelete(0, 0x36, (int) pl);
            w->anchorEff = 1;
            EstSet(0, -1, 0, 0, 0xF, 0x1F, 0, 0x36, (u32) pl, 0);
        }
    } else {
        if (w->anchorEff != 2) {
            EffectEspDelete(0, 0x36, (u32) pl, 0);
            EffectEspgenDelete(0, 0x36, (int) pl);
            EffectEfmDelete(0, 0x36, (int) pl);
            w->anchorEff = 2;
            EstSet(0, -1, 0, 0, 0xF, 0x1E, 0, 0x36, (u32) pl, 0);
        }
    }
}

// The partner (Ashley is not in the boat; the routines exist for the R10d / R10e entrances). A macro: an
// inline's f32 arguments are expanded at the head of the block (both pool `lis` hoisted above the
// subHideMode test) and its inlined pool loads lose RTX_UNCHANGING_P (integrate.c), so they would depend on
// the preceding stores through `sub`.
#define SUB_BOAT_SIT(lo, hi) \
{ \
    if (sub->subHideMode) { \
        if (w->spdXZ < lo) { \
            sub->subHideMode = 0; \
            MotionSetCore(sub, &sub->mot, SUBARC(0x2C), 0, 5, 5, 0); \
        } \
    } else if (w->spdXZ > hi) { \
        sub->subHideMode = 1; \
        sub->subSelf->sub409 = 5; \
        sub->sub408 = 0; \
        sub->subBlendRate = 0.0f; \
    } \
    if (sub->subHideMode) { \
        sub->subBlendRate = sub->subBlendRate * 0.9f + pPL->blendRate500 * 0.1f; \
        subBlendMotSet(sub, SUBARC(0x2E), SUBARC(0x2F), SUBARC(0x30), 0, 0, 0); \
    } \
    subOnBoat(sub, boat); \
    MotionMoveF(sub, 0); \
}

static void subBoatRide()
{
    cSubChar* sub = pSUB;
    cPl0f* boat = SUB_BOAT(sub);
    Pl0fWork* w = PL0F_WK(boat);
    Vec v;

    sub->subArc = boat->subArc;
    sub->dmType = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->xFE) {
    case 0:
        MotionSetCore(sub, &sub->mot, SUBARC(0x2B), 0, 5, 5, 0);
        v.x = 984.32f;
        v.y = 500.0f;
        v.z = 731.88f;
        PSMTXMultVec(boat->mat, &v, &v);
        sub->pos.x = v.x;
        sub->pos.z = v.z;
        sub->sub52C = v.y - sub->pos.y;
        sub->subHideMode = 20;
        sub->rot.y = boat->rot.y - PI / 2;
        sub->rot.y = LIMIT_ANGLE(sub->rot.y);
        sub->atari.flags &= 0xFCFF;
        sub->xFE++;
    case 1:
        if (sub->subHideMode) {
            sub->subHideMode--;
        } else {
            f32 dy = sub->sub52C * 0.1f;

            sub->pos.y += dy;
            sub->sub52C -= dy;
        }
        if (MotionMoveF(sub, 0)) {
            sub->xFE++;
        } else {
            if (sub->frame > 21.7f && sub->frame < 22.3f) {
                SndCall(8, 6, &sub->pos, boat->id, 0, 0);
            }
            if (sub->frame > 22.7f && sub->frame < 23.3f) {
                SndCall(8, 6, &sub->pos, boat->id, 0, 0);
            }
        }
        break;
    case 2:
        MotionSetCore(sub, &sub->mot, SUBARC(0x2C), 0, 5, 5, 0);
        sub->atari.flags &= 0xFCFF;
        sub->subHideMode = 0;
        sub->xFE++;
    case 3:
        SUB_BOAT_SIT(50.0f, 30.0f);
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

static void subBoatGetoff()
{
    cSubChar* sub = pSUB;
    cPl0f* boat = SUB_BOAT(sub);

    sub->subArc = boat->subArc;
    sub->dmType = 0x1E;
    sub->motFlags2 &= ~0x40000000;
    switch (sub->xFE) {
    case 0:
        MotionSetCore(sub, &sub->mot, SUBARC(0x2D), 0, 5, 5, 0);
        sub->subHideMode = 20;
        sub->sub52C = 100.0f;
        sub->xFE++;
    case 1:
        if (sub->subHideMode) {
            sub->subHideMode--;
        } else {
            f32 dy = sub->sub52C * 0.1f;

            sub->pos.y += dy;
            sub->sub52C -= dy;
        }
        if (MotionMoveF(sub, 0)) {
            EndSubDamage();
        }
        break;
    }
    sub->motFlags2 &= ~0x40000000;
    sub->subArc = sub->subArc2;
}

#define SUB_BOAT_ROOM_IN() \
{ \
    cSubChar* sub = pSUB; \
    cPl0f* boat = SUB_BOAT(sub); \
    Pl0fWork* w = PL0F_WK(boat); \
 \
    sub->subArc = boat->subArc; \
    sub->dmType = 0x1E; \
    sub->motFlags2 &= ~0x40000000; \
    switch (sub->xFE) { \
    case 0: \
        MotionSetCore(sub, &sub->mot, SUBARC(0x2C), 0, 0, 5, 0); \
        sub->atari.flags &= 0xFCFF; \
        sub->subHideMode = 0; \
        sub->xFE++; \
    case 1: \
        SUB_BOAT_SIT(50.0f, 30.0f); \
        break; \
    } \
    sub->motFlags2 &= ~0x40000000; \
    sub->subArc = sub->subArc2; \
}

static void subBoatR10dIn()
{
    SUB_BOAT_ROOM_IN();
}

static void subBoatR10eIn()
{
    SUB_BOAT_ROOM_IN();
}

static void subBoatR10eIn2()
{
    SUB_BOAT_ROOM_IN();
}

void subOnBoat(cSubChar* sub, cPl0f* boat)
{
    Mtx m;
    Vec v;

    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1217.69f;
    PSMTXMultVec(boat->mat, &v, &sub->pos);
    PSMTXCopy(boat->mat, sub->mat);
    PSMTXRotRad(m, 'y', PI);
    PSMTXConcat(sub->mat, m, sub->mat);
    TransMatrix(sub->mat, &sub->pos);
    sub->rot = boat->rot;
    sub->rot.y += PI;
    sub->rot.y = LIMIT_ANGLE(sub->rot.y);
    sub->motFlags2 |= 0x40000000;
}

void cPl0f::setBossStart(Vec* p, f32 ang)
{
    if (testSearchEm2f(this)) {
        cPlayer* pl;

        pl0fSetAnchorEm2f(this);
        setPos(p, ang);
        FSet(pPL->rot.y, ang);   // scalar-reference store: pPL is reloaded for the pos copy
        pPL->pos = pos;
        EffectEspDelete(0, 0x35, (u32) this, 0);
        EffectEspgenDelete(0, 0x35, (int) this);
        EffectEfmDelete(0, 0x35, (int) this);
        PlRoutineSet(pPL, 0, 0xF, 2, 0);
        PlRoutineSet(this, 1, 6, 0, 0);
        pl = pPL;
        pl->pBody->initWepHand((u32) PL_ARC_PTR(subArc, 8));
        pl->setRightHand(1);
        pl->pWep->setTrans(0, 0);
    }
}

void cPl0f::stopEngine()
{
    SndStop(PL0F_WK(this)->seNo, 0);
}
