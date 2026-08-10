// em18 module (D:/Bio4/Prog/em18.cpp): the merchant. Waits, offers the trade action button when
// the player stands in front of him (em18ActEvtSetTrade), plays the trade motion and opens the
// shop sub screen; dies to a damage volume of kind 1/7 or to any weapon but the special ones.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "map_obj.h"
#include "widget.h"
#include "em18.h"
#include "em10.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "em_cloth.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "motion.h"
#include "foot_shadow.h"
#include "act_btn.h"
#include "sscrn.h"
#include "snd.h"
#include "pad.h"
#include "player.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

// The module's 0x34-byte COMMON block (st_room.h): uninitialised template statics of the original
// object, merged into .bss by the REL link.
asm(".comm common_em18,52,4");

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp
extern FootShadowTbl Em10_fs_tbl;     // game/foot_shadow_tbl.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// model.h's member; the enemies call it with the old ModelData (em10.cpp).
extern "C" void cModel_swapModelInfo(cModel* m, ModelData* old, cModelInfo* info) asm("swapModelInfo__6cModelP9ModelDataP10cModelInfo");

typedef void (*Em18Func)(cEm18*);

static void em18_R0_Init(cEm18* em);
static void em18_R0_Move(cEm18* em);
static void em18_R1_Wait(cEm18* em);
static void em18_R1_Trade(cEm18* em);
static void em18TradeAction(cEm18* em);
static void em18_R0_Damage(cEm18* em);
static void em18_R1_Dm_Normal(cEm18* em);
static void em18_R0_Die(cEm18* em);
static void em18_R1_Die_Normal(cEm18* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

extern "C" void _prolog()
{
    OSReport("em18 prolog Ok\n");
    EmInitFunc = Em18Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em18Init(cEm* em)
{
    new (em) cEm18();
}

void em18DmCk(cEm18* em)
{
    u8 wep;

    if (em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 7:
            em->hp = 0;
            EmSetDie(em);
            EmRoutineSet(em, 3, 0, 0, 0);
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
    wep = em->dmWep;
    if (wep == 0x14) {
        return;
    }
    if (wep == 0x16) {
        return;
    }
    if (wep == 0x17) {
        return;
    }
    if (wep == 0xE) {
        return;
    }
    em->hp = 0;
    em18BloodSet(em);
    SndCall(8, 4, &em->pos, em->id, 0, 0);
    EmSetDie(em);
    EmRoutineSet(em, 3, 0, 0, 0);
}

void em18BloodSet(cEm18* em)
{
    EmHitInfo* part = em->dmPart;

    switch (em->dmWep) {
    case 7:
    case 8:
        if (part->rad < 36000000.0f) {
            EmDmBloodSet2(em, 0x15, 1, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x15, 0, 0, 0, 0);
        }
        break;
    case 1:
    case 2:
    case 3:
    case 4:
        EmDmBloodSet2(em, 0x15, 0, 0, 0, 0);
        break;
    case 9:
    case 0xA:
        EmDmBloodSet2(em, 0x15, 0, 0, 0, 0);
        break;
    case 0xB:
    case 0xC:
    case 0x10:
        EmDmBloodSet2(em, 0x15, 0, 0, 0, 0);
        break;
    case 5:
    case 6:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x15:
        EmDmBloodSet2(em, 0x15, 1, 0, 0, 0);
        break;
    case 0:
    case 0x14:
    default:
        break;
    }
}

Em18Func Em18_R0_move_tbl[4] = {
    em18_R0_Init,
    em18_R0_Move,
    em18_R0_Damage,
    em18_R0_Die,
};

static Em18Func Em18_R1_move_tbl[2] = {
    em18_R1_Wait,
    em18_R1_Trade,
};

static Em18Func Em18_R2_move_tbl[1] = {
    em18_R1_Dm_Normal,
};

static Em18Func Em18_R3_move_tbl[1] = {
    em18_R1_Die_Normal,
};

// Parts index remap of the mirrored motions (cModel::motFlip).
static u16 em18_flip_tbl[80] = {
    0, 1, 2, 3, 4, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 5, 6, 7, 8, 9,
    0xA, 0x11, 0x16, 0x17, 0x18, 0x19, 0x12, 0x13, 0x14, 0x15, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
};

void cEm18::move()
{
    Em18Work* w = EM18_WK(this);

    if (xFC) {
        em18DmCk(this);
    }
    w->flags &= ~0x1F;
    Em18_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em18NeckMove(this);
    partsWorldCalc();
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    Em18ClothMove(this, &w->cloth);
}

static void em18_R0_Init(cEm18* em)
{
    Em18Work* w = EM18_WK(em);
    cModelInfo* info;
    void* tpl;
    void* tplE;
    int one;

    if (em->modelInit(ARC(5), ARC(6)) == 0) {
        pLog->err(0, 0, "em18() ModelInit failed.");
        em->xFC = 0xFF;
        return;
    }
    tpl = ARC(8);
    tplE = ARC(0xF);
    w->pInfoE = ModInfoMgr.create(ARC(0xE), tplE);
    if (w->pInfoE) {
        em->addModel(w->pInfoE);
        w->pInfoE->be_flag |= 0x20;
    }
    info = ModInfoMgr.create(ARC(7), tpl);
    if (info) {
        em->addModel(info);
    }
    w->pHandL = 0;
    w->pHandR = 0;
    em18HandSet(em);
    w->pCloth = 0;
    w->pGoods = 0;
    em18ClothPartsSet(em, 0);
    em18GoodsPartsSet(em, 0);
    em->pFootShadowTbl = &Em10_fs_tbl;
    em->motFlip = em18_flip_tbl;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 10000.0f, 10000.0f, 10000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->atari.init(1, 0x2000, 10, 0.0f, 0.0f, 0.0f, 800.0f, 700.0f, 700.0f, 1800.0f);
    em->atari.flags |= 8;
    em->litArea.on(1);
    one = 1;
    em->setStatus(one);
    em->setStatus(0xB);
    YarareInit(em, 0.0f, 0.0f, 0.0f, 150.0f, 120.0f, 5, 1);
    YarareAdd(em, &w->hit[0], 0.0f, -30.0f, 0.0f, 200.0f, 300.0f, 2, 1);
    YarareAdd(em, &w->hit[1], -20.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x14, 1);
    YarareAdd(em, &w->hit[2], 20.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x18, 1);
    YarareAdd(em, &w->hit[3], -300.0f, 0.0f, 0.0f, 100.0f, 400.0f, 9, 3);
    YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 100.0f, 400.0f, 0xF, 3);
    YarareAdd(em, &w->hit[5], -20.0f, -300.0f, 0.0f, 170.0f, 300.0f, 0x13, 1);
    YarareAdd(em, &w->hit[6], 20.0f, -300.0f, 0.0f, 170.0f, 300.0f, 0x17, 1);
    YarareAdd(em, &w->hit[7], -300.0f, 0.0f, 0.0f, 120.0f, 300.0f, 8, 3);
    YarareAdd(em, &w->hit[8], 0.0f, 0.0f, 0.0f, 120.0f, 300.0f, 0xE, 3);
    em->lockParts = 0;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(4), 0x15, 0);
    Em18ClothSet(em, &w->cloth, 0);
    w->flags = 0;
    w->neckAng = 0.0f;
    EmRoutineSet(em, one, 0, 0, 0);
    MotionSetCore(em, MOTION(em), ARC(0x14), 0, 0, 1, 0);
    MotionMoveF(em, 0);
    em->clearStatus(5);
    em18_R0_Move(em);
}

static void em18_R0_Move(cEm18* em)
{
    Em18_R1_move_tbl[em->xFD](em);
}

static void em18_R1_Wait(cEm18* em)
{
    Em18Work* w = EM18_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x14), 0, 30, 5, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        break;
    }
    em18ActEvtSetTrade(em);
}

static void em18_R1_Trade(cEm18* em)
{
    Em18Work* w = EM18_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x15), 0, 10, 1, 0);
        w->sndId = SndCall(8, 9, &em->pos, em->id, 0, 0);
        KeyStop(0xEFCF0000);
        em->xFE++;
    case 1:
        em->dmType = 2;
        if (em->motFrame > 35.7f && em->motFrame < 36.3f) {
            em18ClothPartsSet(em, 1);
            SndCall(8, 0xA, &em->pos, em->id, 0, 0);
        }
        if (em->motFrame > 40.7f && em->motFrame < 41.3f) {
            em18GoodsPartsSet(em, 1);
        }
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, PI / 16.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        if (SubScreenOpen(0x10, 0)) {
            em->xFE++;
        }
        break;
    case 3:
        MotionSetCore(em, MOTION(em), ARC(0x17), 0, 10, 1, 0);
        SndCall(8, 0xA, &em->pos, em->id, 0, 0);
        w->sndId = SndCall(8, 7, &em->pos, em->id, 0, 0);
        pGS->flags_170 &= 0x7FFFFFFF;
        em->xFE++;
    case 4:
        if (em->motFrame > 33.7f && em->motFrame < 34.3f) {
            em18ClothPartsSet(em, 0);
        }
        if (em->motFrame > 26.7f && em->motFrame < 27.3f) {
            em18GoodsPartsSet(em, 0);
        }
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
}

void em18ActEvtSetTrade(cEm18* em)
{
    Mtx inv;
    Vec lp;
    // COMPILER-DIFF: #5. The original has one `fabsf(lp.y) > 700` check after the type if/else and its
    // interblock scheduler copied it into the first arm (`fabs f13, f0` with the 700 still in f12); ours
    // forms no region there, so the check is written in both arms through one shared `ay`: a pseudo set
    // in two blocks is global-allocated, so local-alloc cannot tie the fabs result to its dying input.
    f32 ay;

    if (em->hp <= 0) {
        return;
    }
    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, PI)) > 0.7853982f) {
        return;
    }
    PSMTXInverse(pPL->mat, inv);
    PSMTXMultVec(inv, &em->pos, &lp);
    if (em->type != 1) {
        if (lp.z > 1200.0f || lp.z < 0.0f) {
            return;
        }
        if (lp.x > 700.0f || lp.x < -700.0f) {
            return;
        }
        ay = fabsf(lp.y);
        if (ay > 700.0f) {
            return;
        }
    } else {
        if (em->plDist2 > 2250000.0f) {
            if (lp.z > 2500.0f || lp.z < 1000.0f) {
                return;
            }
            if (lp.x > 700.0f || lp.x < -700.0f) {
                return;
            }
        }
        ay = fabsf(lp.y);
        if (ay > 700.0f) {
            return;
        }
    }
    ActBtn.set(0, 2, (int) em18TradeAction, (int) em, 0, 1, 0, 0);
}

static void em18TradeAction(cEm18* em)
{
    Em18Work* w = EM18_WK(em);

    if (em->type != 1) {
        if (w->flags & 0x20) {
            SubScreenOpen(0x10, 0);
        } else {
            BitOn(w->flags, 0x20);
            EmRoutineSet(em, 1, 1, 0, 0);
            pPL->dmg.set(0, 30);
        }
    } else {
        SubScreenOpen(0x10, 0);
    }
}

static void em18_R0_Damage(cEm18* em)
{
    Em18Work* w = EM18_WK(em);

    w->flags |= 8;
    Em18_R2_move_tbl[em->xFD](em);
}

static void em18_R1_Dm_Normal(cEm18* em)
{
    Em18Work* w = EM18_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x14), 0, 3, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0, 0, 10);
        }
        break;
    }
}

static void em18_R0_Die(cEm18* em)
{
    Em18Work* w = EM18_WK(em);

    w->flags |= 8;
    Em18_R3_move_tbl[em->xFD](em);
}

static void em18_R1_Die_Normal(cEm18* em)
{
    Em18Work* w = EM18_WK(em);

    switch (em->xFE) {
    case 2:
    default:
        break;
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x18), 0, 3, 1, 0);
        SndStop(w->sndId, 0);
        SndCall(8, 8, &em->pos, em->id, 0, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->clearStatus(5);
            em->atari.flags &= ~0x300;
            em->xFE++;
        } else {
            if (em->motFrame > 34.7f && em->motFrame < 35.3f) {
                SndCall(8, 5, &em->pos, em->id, 0, 0);
            }
            if (em->motFrame > 65.7f && em->motFrame < 66.3f) {
                SndCall(8, 6, &em->pos, em->id, 0, 0);
            }
        }
        break;
    }
}

void em18NeckMove(cEm18* em)
{
    Em18Work* w = EM18_WK(em);
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

void em18ClothPartsSet(cEm18* em, int on)
{
    Em18Work* w = EM18_WK(em);
    void* tpl = ARC(0xF);
    void* bin;
    cModelInfo* info;

    switch (on) {
    case 0:
    default:
        bin = ARC(0xC);
        break;
    case 1:
        bin = ARC(0xB);
        break;
    }
    info = ModInfoMgr.create(bin, tpl);
    if (info) {
        if (w->pCloth) {
            cModel_swapModelInfo(em, w->pCloth->pData, info);
        } else {
            em->addModel(info);
        }
        info->be_flag |= 0x20;
        w->pCloth = info;
    }
}

void em18GoodsPartsSet(cEm18* em, int on)
{
    Em18Work* w = EM18_WK(em);

    if (w->pGoods == 0) {
        cModelInfo* info = ModInfoMgr.create(ARC(0xD), ARC(6));

        if (info) {
            em->addModel(info);
        }
        w->pGoods = info;
    }
    if (on == 0) {
        w->pGoods->be_flag &= ~8;
    } else {
        w->pGoods->be_flag |= 8;
    }
}

void em18HandSet(cEm18* em)
{
    Em18Work* w = EM18_WK(em);
    void* binL = ARC(0x12);
    void* binR = ARC(0x13);
    cModelInfo* info;

    info = ModInfoMgr.create(binL, ARC(6));
    if (info) {
        if (w->pHandL) {
            cModel_swapModelInfo(em, w->pHandL->pData, info);
        } else {
            em->addModel(info);
        }
        w->pHandL = info;
    }
    info = ModInfoMgr.create(binR, ARC(6));
    if (info) {
        if (w->pHandR) {
            cModel_swapModelInfo(em, w->pHandR->pData, info);
        } else {
            em->addModel(info);
        }
        w->pHandR = info;
    }
}
