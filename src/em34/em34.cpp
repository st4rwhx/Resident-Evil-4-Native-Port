// em34 module (D:/Bio4/Prog/em34.cpp): the em34 / em37 / em33 enemies in one module, selected by
// cModel::type (1 = em37, 2..3 = em33, else em34). A large enemy that turns towards its target
// (em34RouteCk: the player or the partner), walks up to it and bites (em34AtkCk).

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em34.h"
#include "em10.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "em_cloth.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "motion.h"
#include "route_ck.h"
#include "foot_shadow.h"
#include "quake.h"
#include "pad.h"
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

typedef void (*Em34Func)(cEm34*);

static void em34_R0_Init(cEm34* em);
static void em34_R0_Move(cEm34* em);
static void em34_R1_Wait(cEm34* em);
static void em34_R1_Walk(cEm34* em);
static void em34_R1_Atk(cEm34* em);
static void em34_R0_Damage(cEm34* em);
static void em34_R1_Dm_Normal(cEm34* em);
static void em34_R0_Die(cEm34* em);
static void em34_R1_Die_Normal(cEm34* em);

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
static inline int em34DeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

extern "C" void _prolog()
{
    OSReport("em34 prolog Ok\n");
    EmInitFunc = Em34Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em34Init(cEm* em)
{
    new (em) cEm34();
}

void em34DmCk(cEm34* em)
{
    int dmg;

    if (em->dmHit == 0) {
        return;
    }
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
    default:
        dmg = 50;
        break;
    }
    LifeDownSet2(em, dmg, 0, 1);
    EmDmBloodSet(em);
    if (em->hp <= 0) {
        EmSetDie(em);
        EmRoutineSet(em, 3, 0, 0, 0);
    }
}

Em34Func Em34_R0_move_tbl[4] = {
    em34_R0_Init,
    em34_R0_Move,
    em34_R0_Damage,
    em34_R0_Die,
};

static Em34Func Em34_R1_move_tbl[3] = {
    em34_R1_Wait,
    em34_R1_Walk,
    em34_R1_Atk,
};

static Em34Func Em34_R2_move_tbl[1] = {
    em34_R1_Dm_Normal,
};

static Em34Func Em34_R3_move_tbl[1] = {
    em34_R1_Die_Normal,
};

// Bite attack (em34AtkCk): range, type, damage, ...
static EmAtkInfo em34_atk_tbl[1] = {
    { 300.0f, 8, 9999, 0, 0xA, 0 },
};
static int em34_atk_pad = 0;

void cEm34::move()
{
    Em34Work* w = EM34_WK(this);

    if (xFC) {
        em34DmCk(this);
    }
    w->flags &= ~0x1F;
    em34RouteCk(this);
    Em34_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em34NeckMove(this);
    partsWorldCalc();
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    switch (type) {
    case 0:
    default:
        Em34ClothMove2(this, &w->cloth2);
        Em34ClothMove1(this, &w->cloth1);
        break;
    case 1:
        Em37HairMove(this, &w->cloth1);
        Em37CoatMove(this, &w->cloth2);
        break;
    case 2:
    case 3:
        Em33ClothMove(this, &w->cloth1);
        Em33ClothMove2(this, &w->cloth2);
        break;
    }
}

static void em34_R0_Init(cEm34* em)
{
    Em34Work* w = EM34_WK(em);
    cModelInfo* info;
    int one;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(8)) == 0) {
            pLog->err(0, 0, "em34() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        info = ModInfoMgr.create(ARC(5), ARC(8));
        if (info) {
            em->addModel(info);
            w->pInfo0 = info;
        }
        info = ModInfoMgr.create(ARC(6), ARC(8));
        if (info) {
            em->addModel(info);
            w->pInfo1 = info;
        }
        info = ModInfoMgr.create(ARC(7), ARC(8));
        if (info) {
            em->addModel(info);
            w->pInfo2 = info;
        }
        break;
    case 1:
        if (em->modelInit(ARC(9), ARC(0xB)) == 0) {
            pLog->err(0, 0, "em37() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        info = ModInfoMgr.create(ARC(0xA), ARC(0xB));
        if (info) {
            em->addModel(info);
        }
        break;
    case 2:
        if (em->modelInit(ARC(0xC), ARC(0xE)) == 0) {
            pLog->err(0, 0, "em33() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        info = ModInfoMgr.create(ARC(0xD), ARC(0xE));
        if (info) {
            em->addModel(info);
        }
        em->be_flag |= 0x01000000;
        break;
    case 3:
        if (em->modelInit(ARC(0xC), ARC(0xF)) == 0) {
            pLog->err(0, 0, "em33() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        info = ModInfoMgr.create(ARC(0xD), ARC(0xF));
        if (info) {
            em->addModel(info);
        }
        em->be_flag |= 0x01000000;
        break;
    }
    em->pFootShadowTbl = &Em10_fs_tbl;
    switch (em->type) {
    case 0:
    default:
        Em34ClothSet2(em, &w->cloth2);
        Em34ClothSet1(em, &w->cloth1);
        break;
    case 1:
        Em37HairSet(em, &w->cloth1);
        Em37CoatSet(em, &w->cloth2);
        break;
    case 2:
    case 3:
        Em33ClothSet(em, &w->cloth1, 0);
        Em33ClothSet2(em, &w->cloth2, 0);
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
        atariInitF(&em->atari, 0.0f, 0.0f, 0.0f, 800.0f, 700.0f, 700.0f, 3000.0f, 1, 0x2000, 10);   // COMPILER-DIFF: #1
        break;
    case 1:
        atariInitF(&em->atari, 0.0f, 0.0f, 0.0f, 300.0f, 250.0f, 250.0f, 1000.0f, 1, 0x2000, 10);   // COMPILER-DIFF: #1
        break;
    case 2:
    case 3:
        atariInitF(&em->atari, 0.0f, 0.0f, 0.0f, 800.0f, 700.0f, 700.0f, 3000.0f, 1, 0x2000, 10);   // COMPILER-DIFF: #1
        break;
    }
    em->litArea.on(1);
    YarareInit(em, 0.0f, 0.0f, 0.0f, 400.0f, 200.0f, 1, 1);
    one = 1;
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 5, 1);
    YarareAdd(em, &w->hit[1], 0.0f, -100.0f, 0.0f, 200.0f, 200.0f, 0x14, 1);
    YarareAdd(em, &w->hit[2], 0.0f, -100.0f, 0.0f, 200.0f, 200.0f, 0x18, 1);
    em->lockParts = 2;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(0x10), 0x2B, 0);
    w->neckAng = 0.0f;
    w->flags = 0;
    EmRoutineSet(em, one, 0, 0, 0);
    switch (em->type) {
    case 0:
    default:
        MotionSetCore(em, MOTION(em), ARC(0x11), 0, 0, 5, 0);
        break;
    case 1:
        MotionSetCore(em, MOTION(em), ARC(0x14), 0, 0, 5, 0);
        break;
    case 2:
    case 3:
        MotionSetCore(em, MOTION(em), ARC(0x17), 0, 0, 5, 0);
        break;
    }
    MotionMoveF(em, 0);
    em34_R0_Move(em);
}

static void em34_R0_Move(cEm34* em)
{
    Em34_R1_move_tbl[em->xFD](em);
}

static void em34_R1_Wait(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        switch (em->type) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x11), 0, 30, 5, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0x14), 0, 30, 5, 0);
            break;
        case 2:
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x17), 0, 30, 5, 0);
            break;
        }
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em34DeadCk(em)) {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
}

static void em34_R1_Walk(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        switch (em->type) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x12), 0, 10, 5, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0x13), 0, 10, 5, 0);
            break;
        case 2:
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x18), 0, 10, 5, 0);
            break;
        }
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, PI / 64.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if (em->type == 1 && em->hp < 500) {
            if (em->plDist2 < 1000000.0f) {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        } else {
            if (em->plDist2 < 4000000.0f) {
                EmRoutineSet(em, 1, 0, 0, 0);
            }
        }
        break;
    }
}

static void em34_R1_Atk(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        switch (em->type) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x11), 0, 10, 5, 0);
            break;
        case 2:
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x17), 0, 10, 5, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0x15), (int) ARC(0x16), 10, 1, 0);
            break;
        }
        w->atkHit = 0;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, PI / 32.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0, 0, 0);
        } else if (em->seFlags28B & 1) {
            em34AtkCk(em, 0, 0xA);
        }
        break;
    }
}

static void em34_R0_Damage(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

    w->flags |= 8;
    Em34_R2_move_tbl[em->xFD](em);
}

static void em34_R1_Dm_Normal(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        switch (em->type) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x11), 0, 3, 1, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0x14), 0, 3, 1, 0);
            break;
        case 2:
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x17), 0, 0, 1, 0);
            break;
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 1, 0, 10);
        }
        break;
    }
}

static void em34_R0_Die(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

    w->flags |= 8;
    Em34_R3_move_tbl[em->xFD](em);
}

static void em34_R1_Die_Normal(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

    switch (em->xFE) {
    case 0:
        switch (em->type) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x11), 0, 3, 1, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0x14), 0, 3, 1, 0);
            break;
        case 2:
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x17), 0, 0, 1, 0);
            break;
        }
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

void em34RouteCk(cEm34* em)
{
    Em34Work* w = EM34_WK(em);

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

void em34NeckMove(cEm34* em)
{
    Em34Work* w = EM34_WK(em);
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

int em34AtkCk(cEm34* em, int no, int parts)
{
    Em34Work* w = EM34_WK(em);

    if (w->atkHit) {
        return 0;
    }
    {
        EmAtkInfo* atk = &em34_atk_tbl[no];
        cModel* p = em->getPartsPtr(parts);
        int hit = EmAtkHitCk(atk, &p->worldPos, &p->oldWorldPos, 0);

        if (hit) {
            if (hit & 1) {
                EmPlBloodSet(em, &p->worldPos, 1, 0xFF, 0xFF);
                w->atkHit = 1;
            }
            if (hit & 2) {
                EmSubBloodSet(em, &p->worldPos, 1, 0xFF, 0xFF);
                w->atkHit = 1;
            }
            QuakeExec(0, 0, 5, 22.0f, 2);
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            return 1;
        }
    }
    return 0;
}
