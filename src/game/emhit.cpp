// game/emhit.cpp: hit-only enemy (cEmHit): a damage receiver for objects, a parts follower
// (setParent) and the beetle that flies away when shot.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "emhit.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern cEm* pPL;   // game/em.cpp

extern "C" {
int MotionMove(cModel* m, int a);
void EtcSetAddAmb(cModel* m, int a);                                                         // EtcModel.cpp
}
void MotionSetCore(cModel* m, void* mot, void* data, int a, int b, int c, int d);

typedef void (*EmHitFunc)(cEmHit*);

static EmHitFunc EmHit_R0_move_tbl[4] = {
    emHit_R0_Init,
    emHit_R0_Move,
    0,
    0,
};

EmHitFunc EmHit_R1_move_tbl[4] = {
    emHit_R1_Set,
    emHit_R1_Parent,
    emHit_R1_Break,
    emHit_R1_Beetle,
};

cEmHit* SetEmHit(void* bin, void* tpl, Vec* pos, Vec* rot, int type)
{
    cEmHit* em;
    EmHitWork* w;

    em = (cEmHit*) EmMgr.create(0x4D);
    if (em == 0) {
        return 0;
    }
    w = EMHIT_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->rot = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetEmHit() failed.");
        EmMgr.destroy(em);
        return 0;
    }
    em->type = type;
    EtcSetAddAmb(em, 5);
    cModel* parent = 0;
    w->x248 = 0xFF;
    w->size.x = 200.0f;
    w->size.y = 200.0f;
    w->size.z = 200.0f;
    em->atari.init(0, 2, 0, 0.0f, 0.0f, 0.0f, 700.0f, 400.0f, 500.0f, 500.0f);
    em->atari.setPriority(3);
    em->atari.throughOn();
    emHitYarareInit(em);
    em->hpMax = em->hp = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 1000.0f, 1000.0f, 0.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 0x10);
    }
    em->lockParts = 0;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(1);
    em->setStatus(0xB);
    w->flags = 0;
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    w->status = 0;
    w->pParent = parent;
    w->partsNo = 0;
    w->noNormalize = 0;
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
    return em;
}

void emHitDmCk(cEmHit* em)
{
    EmHitWork* w = EMHIT_WK(em);
    u8 wep;

    w->status = 0;
    if (em->dmHit == 0) {
        em->dmWep = 0;
        return;
    }
    wep = em->dmWep;
    em->dmHit = 0;
    if (wep == 0x14) {
        return;
    }
    if (wep == 0x16) {
        return;
    }
    if (wep == 0x17) {
        return;
    }
    if (wep == 0x2A) {
        return;
    }
    if (wep == 0xE) {
        return;
    }
    em->dmType = 1;
    if (wep == 0x10) {
        em->dmType = 0x11;
    }
    switch (em->type) {
    case 0:
    default:
        em->hp = 0;
        w->status = 1;
        em->xFC = 1;
        em->xFD = 2;
        em->xFE = 0;
        em->xFF = 0;
        break;
    case 1:
        w->status = 1;
        break;
    case 2:
        em->hp = 0;
        break;
    }
}

void cEmHit::move()
{
    EmHitWork* w = EMHIT_WK(this);

    w->status = 0;
    emHitDmCk(this);
    be_flag &= ~0x4000;
    EmHit_R0_move_tbl[xFC](this);
}

void emHit_R0_Init(cEmHit* em)
{
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
}

void emHit_R0_Move(cEmHit* em)
{
    EmHit_R1_move_tbl[em->xFD](em);
}

void emHit_R1_Set(cEmHit* em)
{
    if (em->xFE == 0) {
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        em->partsMatCalc();
        em->partsWorldCalc();
        em->xFE++;
    }
    em->be_flag |= 0x4000;
}

void emHit_R1_Parent(cEmHit* em)
{
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    EmHitWork* w = EMHIT_WK(em);
    cModel* parent = w->pParent;

    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    if (parent && parent->pParts) {
        PSMTXConcat(parent->getPartsPtr(w->partsNo)->mat, em->mat, m);
        if (w->noNormalize == 0) {
            v0.x = m[0][0];
            v0.y = m[1][0];
            v0.z = m[2][0];
            v1.x = m[0][1];
            v1.y = m[1][1];
            v1.z = m[2][1];
            v2.x = m[0][2];
            v2.y = m[1][2];
            v2.z = m[2][2];
            if (v0.x == 0.0f && v0.y == 0.0f && v0.z == 0.0f) {
                v0.x = 1.0f;
            }
#line 340 "D:/Bio4/Prog/emhit.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 342 "D:/Bio4/Prog/emhit.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 344 "D:/Bio4/Prog/emhit.cpp"
            VECNormalize(&v2, &v2);
            m[0][0] = v0.x;
            m[1][0] = v0.y;
            m[2][0] = v0.z;
            m[0][1] = v1.x;
            m[1][1] = v1.y;
            m[2][1] = v1.z;
            m[0][2] = v2.x;
            m[1][2] = v2.y;
            m[2][2] = v2.z;
        }
        PSMTXCopy(m, em->mat);
    }
    if (em->pMotion) {
        em->motFlags2 |= 0x40000000;
        MotionMove(em, 0);
    } else {
        em->partsMatCalc();
    }
    em->partsWorldCalc();
}

void emHit_R1_Break(cEmHit* em)
{
    EmHitWork* w = EMHIT_WK(em);

    switch (em->xFE) {
    case 0:
        em->hp = 0;
        em->be_flag &= ~2;
        w->status = 1;
        em->xFE++;
    case 1:
        em->be_flag |= 0x4000;
        break;
    }
}

void emHit_R1_Beetle(cEmHit* em)
{
    EmHitWork* w = EMHIT_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->pMotion, w->mot0, 0, 0, 5, 0);
        em->xFE++;
    case 1:
        MotionMove(em, 0);
        if (em->hp <= 0) {
            em->xFE++;
        } else if (fabsf(Muku(&pPL->pos, &em->pos, em->rot.y, 3.1415927f)) < 0.5235988f) {
            if (em->plDist2 < 2250000.0f) {
                em->hp = 0;
                em->xFE++;
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->pMotion, w->mot1, 0, 0, 1, 0x1F);
        em->xFE++;
    case 3:
        if (MotionMove(em, 0)) {
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, &em->pMotion, w->mot2, 0, 3, 5, 0);
        w->spd.x = 0.0f;
        w->spd.y = 10.0f;
        w->spd.z = 10.0f;
        PSMTXMultVecSR(em->mat, &w->spd, &w->spd);
        w->timer = 300;
        em->xFE++;
    case 5:
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y = w->spd.y * 0.9f + 4.0f;
        w->spd.z = w->spd.z * 0.9f + 4.0f;
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->alpha -= 0.1f;
            if (em->alpha <= 0.0f) {
                em->be_flag &= ~2;
                em->alpha = 0.0f;
                em->xFE++;
            }
        }
        break;
    }
    em->partsWorldCalc();
}

void emHitYarareInit(cEmHit* em)
{
    EmHitWork* w = EMHIT_WK(em);

    YarareInitCube(em, 0.0f, 0.0f, 0.0f, w->size.x * 0.5f + 50.0f, w->size.y, w->size.z * 0.5f + 50.0f, 0, 1);
}

int cEmHit::ckStatus()
{
    return EMHIT_WK(this)->status;
}

int cEmHit::ckDmgWeapon()
{
    if (EMHIT_WK(this)->status == 0) {
        return 0;
    }
    return dmWep;
}

void cEmHit::setParent(cModel* parent, int partsNo, int noNormalize)
{
    EmHitWork* w = EMHIT_WK(this);

    w->pParent = parent;
    w->partsNo = partsNo;
    w->noNormalize = noNormalize;
    xFC = 1;
    xFD = 1;
    xFE = 0;
    xFF = 0;
    ((cEm*) parent)->atari.flags &= ~0x200;
}

void cEmHit::setBeetle(void* mot0, void* mot1, void* mot2)
{
    EmHitWork* w = EMHIT_WK(this);

    w->mot0 = mot0;
    w->mot1 = mot1;
    w->mot2 = mot2;
    if (mot0 && mot1 && mot2) {
        YarareInitCube(this, 0.0f, 0.0f, 0.0f, 50.0f, 100.0f, 100.0f, 1, 1);
        hp = 1;
        xFC = 1;
        xFD = 3;
        xFE = 0;
        xFF = 0;
    }
}
