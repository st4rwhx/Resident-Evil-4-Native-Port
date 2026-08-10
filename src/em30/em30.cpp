// em30 module (D:/Bio4/Prog/em30.cpp): a large stationary enemy with two cloth chains and three
// head objects (obj16) carrying four parasites each; it turns towards the player or the partner
// (em30RouteCk chooses the target) and dies after one damage routine.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em30.h"
#include "em10.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "em_cloth.h"
#include "at_mod.h"
#include "atari_init.h"
#include "obj16.h"
#include "esp.h"
#include "motion.h"
#include "route_ck.h"
#include "foot_shadow.h"
#include "player.h"
#include "pl_npc.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp
extern FootShadowTbl Em10_fs_tbl;     // game/foot_shadow_tbl.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");

typedef void (*Em30Func)(cEm30*);

static void em30_R0_Init(cEm30* em);
static void em30_R0_Move(cEm30* em);
static void em30_R1_Wait(cEm30* em);
static void em30_R1_Walk(cEm30* em);
static void em30_R0_Damage(cEm30* em);
static void em30_R1_Dm_Normal(cEm30* em);
static void em30_R0_Die(cEm30* em);
static void em30_R1_Die_Normal(cEm30* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)

// Struct-member views of the character pointers: a load through them is not hoisted above the
// preceding stores through the work pointer (cam_ctrl.cpp PlayerPtr).
struct PlayerPtr {
    cPlayer* p;
};
struct SubCharPtr {
    cSubChar* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)
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
static inline int em30DeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

extern "C" void _prolog()
{
    OSReport("em30 prolog Ok\n");
    EmInitFunc = Em30Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em30Init(cEm* em)
{
    new (em) cEm30();
}

int em30DmCk(cEm30* em)
{
    int dmg;

    if (em->dmHit) {
        em->dmHit = 0;
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
            dmg = (Rnd() & 1) + 10;
            break;
        case 7:
        case 8:
        case 0x21:
            dmg = 10;
            if (em->plDist2 > 16000000.0f) {
                dmg = (Rnd() & 1) + 50;
            }
            break;
        case 5:
        case 6:
        case 0xD:
        case 0xE:
        case 0xF:
        case 0x12:
        case 0x13:
        case 0x2C:
        case 0x2D:
        default:
            dmg = 50;
            break;
        }
        LifeDownSet2(em, dmg, 0, 1);
        EmDmBloodSet(em);
        if (em->hp <= 0) {
            EmSetDie(em);
            EmRoutineSet(em, 3, 0, 0, 0);
        } else {
            // `lbz r3, dmWep; cmpwi r3, 0x21` with no branch (r3 = the return value): the branch
            // around an empty taken arm is deleted by jump2 after reload. Written as `== 0x21`, cse
            // folds the returned value to `li r3, 0x21` on the taken path (record_jump_equiv); the
            // xor form hides the equivalence from cse and combine folds the compare back.
            if ((em->dmWep ^ 0x21) == 0) {
                return em->dmWep;
            }
        }
    }
}

Em30Func Em30_R0_move_tbl[4] = {
    em30_R0_Init,
    em30_R0_Move,
    em30_R0_Damage,
    em30_R0_Die,
};

static Em30Func Em30_R1_move_tbl[2] = {
    em30_R1_Wait,
    em30_R1_Walk,
};

static Em30Func Em30_R2_move_tbl[1] = {
    em30_R1_Dm_Normal,
};

static Em30Func Em30_R3_move_tbl[1] = {
    em30_R1_Die_Normal,
};

void cEm30::move()
{
    Em30Work* w = EM30_WK(this);

    if (xFC) {
        em30DmCk(this);
    }
    w->flags &= ~0x1F;
    em30RouteCk(this);
    Em30_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em30NeckMove(this);
    partsWorldCalc();
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    Em30ClothMove1(this, &w->cloth1);
    Em30ClothMove2(this, &w->cloth2);
}

static void em30_R0_Init(cEm30* em)
{
    Em30Work* w = EM30_WK(em);
    cModelInfo* info;

    if (em->modelInit(ARC(4), ARC(5)) == 0) {
        pLog->err(0, 0, "em30() ModelInit failed.");
        em->xFC = 0xFF;
        return;
    }
    w->pInfo0 = ModInfoMgr.create(ARC(6), ARC(7));
    if (w->pInfo0) {
        em->addModel(w->pInfo0);
    }
    w->pInfo1 = ModInfoMgr.create(ARC(8), ARC(7));
    if (w->pInfo1) {
        em->addModel(w->pInfo1);
    }
    info = ModInfoMgr.create(ARC(9), ARC(5));
    if (info) {
        em->addModel(info);
    }
    if (em->type == 0) {
        info = ModInfoMgr.create(ARC(0xA), ARC(5));
    } else {
        info = ModInfoMgr.create(ARC(0xB), ARC(5));
    }
    if (info) {
        em->addModel(info);
    }
    info = ModInfoMgr.create(ARC(0xC), ARC(5));
    if (info) {
        em->addModel(info);
    }
    info = ModInfoMgr.create(ARC(0xD), ARC(5));
    if (info) {
        em->addModel(info);
    }
    em->pFootShadowTbl = &Em10_fs_tbl;
    Em30ClothSet1(em, &w->cloth1);
    Em30ClothSet2(em, &w->cloth2);
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 10000.0f, 10000.0f, 10000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->atari.init(1, 0x2000, 10, 0.0f, 0.0f, 0.0f, 800.0f, 700.0f, 700.0f, 3000.0f);
    em->litArea.on(1);
    YarareInit(em, 0.0f, 0.0f, 0.0f, 400.0f, 200.0f, 1, 1);
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 5, 1);
    YarareAdd(em, &w->hit[1], 0.0f, -100.0f, 0.0f, 200.0f, 200.0f, 0x14, 1);
    YarareAdd(em, &w->hit[2], 0.0f, -100.0f, 0.0f, 200.0f, 200.0f, 0x18, 1);
    em->lockParts = 2;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(0xE), 0x28, 0);
    w->neckAng = 0.0f;
    w->flags = 0;
    if ((int) em->flags_3C8 < 0) {
        if (w->pInfo0) {
            w->pInfo0->be_flag &= ~8;
        }
        if (w->pInfo1) {
            w->pInfo1->be_flag &= ~8;
        }
        em30SetParasite(em, 0);
        em30SetParasite(em, 1);
        em30SetParasite(em, 2);
    }
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
    MotionSetCore(em, MOTION(em), ARC(0xF), 0, 0, 1, 0);
    MotionMoveF(em, 0);
    em30_R0_Move(em);
}

static void em30_R0_Move(cEm30* em)
{
    Em30_R1_move_tbl[em->xFD](em);
}

static void em30_R1_Wait(cEm30* em)
{
    Em30Work* w = EM30_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 30, 5, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em30DeadCk(em)) {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
}

static void em30_R1_Walk(cEm30* em)
{
    Em30Work* w = EM30_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x10), 0, 10, 5, 0);
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, PI / 64.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if (em->plDist2 < 4000000.0f) {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
}

static void em30_R0_Damage(cEm30* em)
{
    Em30Work* w = EM30_WK(em);

    w->flags |= 8;
    Em30_R2_move_tbl[em->xFD](em);
}

static void em30_R1_Dm_Normal(cEm30* em)
{
    Em30Work* w = EM30_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 3, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 1, 0, 10);
        }
        break;
    }
}

static void em30_R0_Die(cEm30* em)
{
    Em30Work* w = EM30_WK(em);

    w->flags |= 8;
    Em30_R3_move_tbl[em->xFD](em);
}

static void em30_R1_Die_Normal(cEm30* em)
{
    Em30Work* w = EM30_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 3, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->clearStatus(0);
            em->clearStatus(5);
            em->clearStatus(6);
            em->clearStatus(7);
            em->atari.flags &= ~0x300;
            em->xFE++;
        }
        break;
    case 2:
        w->timer = 30;
        em->xFE++;
    case 3:
        if (w->timer) {
            w->timer--;
        } else {
            em->alpha -= 0.02f;
            if (em->alpha < 0.0f) {
                em->alpha = 0.0f;
                em->be_flag &= ~2;
            }
        }
        break;
    }
}

void em30RouteCk(cEm30* em)
{
    Em30Work* w = EM30_WK(em);

    if (em->hp <= 0) {
        return;
    }
    if (RouteCkToPos(em, &pPL->pos, &w->routePos, 0, 0)) {
        w->flags |= 1;
    }
    w->routeAng = Muku(&em->pos, &w->routePos, em->rot.y, PI);
    w->routeAngAbs = fabsf(w->routeAng);
    if (em->xFC == 0) {
        w->routeAng = 0.0f;
        w->routeAngAbs = 0.0f;
        em->plDist2 = 100000000.0f;
    }
    w->targetPos = w->routePos;
    w->targetAng = w->routeAng;
    w->targetAngAbs = w->routeAngAbs;
    w->targetDist = em->plDist2;
    w->pTarget = pPLS;
    w->flags &= ~4;
    if (w->flags & 2) {
        if (!(w->flags & 1) || em->plDist2 > em->x374) {
            w->targetPos = w->subRoutePos;
            w->targetAng = w->subAng;
            w->targetAngAbs = w->subAngAbs;
            w->targetDist = em->x374;
            w->pTarget = pSUBS;
            w->flags |= 4;
        }
    }
}

void em30NeckMove(cEm30* em)
{
    Em30Work* w = EM30_WK(em);
    cModel* p;
    Vec v;

    p = em->getPartsPtr(4);
    {
        cModel* h = pPL->getPartsPtr(4);

        v.x = 0.0f;
        v.y = 250.0f;
        v.z = 0.0f;
        PSMTXMultVec(h->mat, &v, &v);
    }
    if (w->flags & 0x10) {
        w->neckAng = w->neckAng * 0.9f + Muku(&em->pos, &pPL->pos, em->rot.y, 1.0471976f) * 0.1f;
    } else {
        w->neckAng = w->neckAng * 0.9f;
    }
    p = em->getPartsPtr(3);
    ((cParts*) p)->motParts.flags |= 0x40000000;
    ((cParts*) p)->addRot.x = 0.0f;
    ((cParts*) p)->addRot.y = w->neckAng;
    ((cParts*) p)->addRot.z = 0.0f;
}

void em30SetParasite(cEm30* em, int no)
{
    Em30Work* w = EM30_WK(em);
    Vec pos;
    Vec rot;
    cObj* obj;
    int frame = (((MotionData*) ARC(0x14))->maxFrame & 0x3FFF) / 4;
    void* bin = ARC(0x15);
    void* tpl = ARC(0x16);
    void* m0 = ARC(0x17);
    void* m1 = ARC(0x18);
    void* m2 = ARC(0x1C);
    void* m3 = ARC(0x1E);
    void* m4 = ARC(0x1F);
    void* m7 = ARC(0x19);
    void* m8 = ARC(0x1D);
    void* m9 = ARC(0x1A);
    void* m10 = ARC(0x1B);

    switch ((u32) no) {
    case 0:
    default:
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        break;
    case 1:
        pos.x = 50.0f;
        pos.y = 0.0f;
        pos.z = -50.0f;
        rot.x = 0.0f;
        rot.y = 2.0943952f;
        rot.z = 0.0f;
        break;
    case 2:
        pos.x = -50.0f;
        pos.y = 0.0f;
        pos.z = -50.0f;
        rot.x = 0.0f;
        rot.y = -2.0943952f;
        rot.z = 0.0f;
        break;
    }
    w->pHead[no] = SetObj16(bin, tpl, em, em, 3, 0xE, &pos, &rot);
    if (w->pHead[no]) {
        ((cObj16*) w->pHead[no])->setMotData(m0, m1, m2, m3, m4, m4, m4, m7, m8, m9, m10);
        ((cObj16*) w->pHead[no])->setMotData(m0, m1, m2, m3, m4, m4, m4, m7, m8, m9, m10);
        ((cObj16*) w->pHead[no])->setPlDmgMot(ARC(0x20), 0);
    }
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = -0.6632251f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    obj = SetObj16(ARC(0x12), ARC(0x13), em, w->pHead[no], 0x16, 0xF, &pos, &rot);
    if (obj) {
        MotSetObj16(obj, ARC(0x14), 4, 0);
        w->para[no].p[0] = obj;
    }
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.61086524f;
    obj = SetObj16(ARC(0x12), ARC(0x13), em, w->pHead[no], 0x17, 0xF, &pos, &rot);
    if (obj) {
        MotSetObj16(obj, ARC(0x14), 4, frame);
        w->para[no].p[1] = obj;
    }
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = -0.5235988f;
    obj = SetObj16(ARC(0x12), ARC(0x13), em, w->pHead[no], 0x18, 0xF, &pos, &rot);
    if (obj) {
        MotSetObj16(obj, ARC(0x14), 4, frame * 2);
        w->para[no].p[2] = obj;
    }
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    obj = SetObj16(ARC(0x12), ARC(0x13), em, w->pHead[no], 0x19, 0xF, &pos, &rot);
    if (obj) {
        MotSetObj16(obj, ARC(0x14), 4, frame * 3);
        w->para[no].p[3] = obj;
    }
}
