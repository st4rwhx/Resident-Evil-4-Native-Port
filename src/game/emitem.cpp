// game/emitem.cpp: item enemy (cEmItem): a pick-up model that follows a parent's parts, swings
// like a medal and breaks or drops when hit by a weapon or a damage volume.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "emitem.h"
#include "emhit.h"
#include "etc_model.h"
#include "esp.h"
#include "snd.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" {
int MotionMove(cModel* m, int a);
void EtcSetAddAmb(cModel* m, int a);                                                         // EtcModel.cpp
}

typedef void (*EmItemFunc)(cEmItem*);

static EmItemFunc EmItem_R0_move_tbl[4] = {
    emItem_R0_Init,
    emItem_R0_Move,
    0,
    0,
};

static EmItemFunc EmItem_R1_move_tbl[5] = {
    emItem_R1_Set,
    emItem_R1_MedalSet,
    emItem_R1_Parent,
    emItem_R1_Drop,
    emItem_R1_Break,
};

cEmItem* SetEmItem(void* bin, void* tpl, Vec* pos, Vec* rot, int type, int etcNo)
{
    cEmItem* em;
    EmItemWork* w;

    em = (cEmItem*) EmMgr.create(0x4C);
    if (em == 0) {
        return 0;
    }
    w = EMITEM_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->rot = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetEmItem() failed.");
        EmMgr.destroy(em);
        return 0;
    }
    em->type = type;
    em->be_flag |= 0x4000;
    EtcSetAddAmb(em, 4);
    w->eff = 0xFF;
    switch (em->type) {
    case 0:
    default:
        w->size.x = 200.0f;
        w->size.y = 300.0f;
        w->size.z = 200.0f;
        break;
    case 1:
        w->size.x = 100.0f;
        w->size.y = 200.0f;
        w->size.z = 10.0f;
        break;
    }
    em->atari.init(0, 2, 0, 0.0f, 0.0f, 0.0f, 700.0f, 400.0f, 500.0f, 500.0f);
    em->atari.setPriority(3);
    em->atari.flags &= ~0x300;
    emItemYarareInit(em);
    em->hpMax = em->hp = 1000;
    static const Vec ofs = { 0.0f, 0.0f, 0.0f };
    static const Vec size = { 1000.0f, 1000.0f, 0.0f };
    if (em->type != 1) {
        em->lightInfo.init2(0, 1, &ofs, &size, 0x20);
    } else {
        em->lightInfo.init2(0, 1, &ofs, &size, 0x20);
    }
    int rotType = 0;
    em->lockParts = 0;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(1);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    w->flags = 0;
    w->rotType = rotType;
    w->status = 0;
    w->rotAng.x = fRand1_1() * 3.1415927f;
    w->rotAng.y = fRand1_1() * 3.1415927f;
    w->rotAng.z = fRand1_1() * 3.1415927f;
    w->rotSpd.x = fRand0_1() * 0.017453292f + 0.05235988f;
    w->rotSpd.y = fRand0_1() * 0.034906585f + 0.08726646f;
    w->rotSpd.z = fRand0_1() * 0.017453292f + 0.05235988f;
    if (Rnd() & 1) {
        w->rotSpd.x = -w->rotSpd.x;
    }
    if (Rnd() & 1) {
        w->rotSpd.y = -w->rotSpd.y;
    }
    if (Rnd() & 1) {
        w->rotSpd.z = -w->rotSpd.z;
    }
    w->rotAmp.x = 0.17453292f;
    w->rotAmp.y = 0.5235988f;
    w->rotAmp.z = 0.17453292f;
    if (em->type == 1) {
        u16* p;

        w->etcNo = etcNo;
        p = GetEtcFlgPtr(etcNo, pG->room_id);
        if (p && (*p & 1)) {
            em->hp = 0;
        }
    }
    if (em->hp <= 0) {
        em->xFC = 1;
        em->xFD = 4;
        em->xFE = 0;
        em->xFF = 0;
    } else if (em->type != 1) {
        em->xFC = 1;
        em->xFD = 0;
        em->xFE = 0;
        em->xFF = 0;
    } else {
        em->xFC = 1;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
    }
    return em;
}

void emItemDmCk(cEmItem* em)
{
    EmItemWork* w = EMITEM_WK(em);
    u8 wep;
    Vec hit;
    Vec dir;

    if (em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, &hit)) {
        case 1:
        case 4:
        case 5:
        case 7:
            switch (em->type) {
            case 0:
            default:
                em->xFC = 1;
                em->xFD = 3;
                em->hp = 0;
                em->xFE = 0;
                em->xFF = 0;
                break;
            case 1:
                em->xFC = 1;
                em->xFD = 4;
                em->hp = 0;
                em->xFE = 0;
                em->xFF = 0;
                EstSet((int) em, -1, 0, 0, w->eff, 0, 0, 0, (u32) em, 0);
                break;
            }
            return;
        }
    }
    if (em->dmHit == 0) {
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
    w->status = 3;
    switch (em->type) {
    case 0:
    default:
        em->hp = 0;
        if (EmGetDmPos(em, &hit, &dir) == 0) {
            dir.x = 0.0f;
            dir.y = 0.0f;
            dir.z = 0.0f;
        }
        EstSet(0, -1, &em->getPartsPtr(0)->worldPos, &dir, 0, 0x57, 0, 0, 0, 0);
        em->xFC = 1;
        em->xFD = 3;
        em->xFE = 0;
        em->xFF = 0;
        break;
    case 1:
        em->xFC = 1;
        em->xFD = 4;
        em->hp = 0;
        em->xFE = 0;
        em->xFF = 0;
        EstSet((int) em, -1, 0, 0, w->eff, 0, 0, 0, (u32) em, 0);
        break;
    }
    if (em->type == 1) {
        SndCall(6, 0x2E, &em->pos, 0, 0, em);
    }
}

void cEmItem::move()
{
    emItemDmCk(this);
    be_flag &= ~0x4000;
    EmItem_R0_move_tbl[xFC](this);
}

void emItem_R0_Init(cEmItem* em)
{
    if (em->type != 1) {
        em->xFC = 1;
        em->xFD = 0;
        em->xFE = 0;
        em->xFF = 0;
    } else {
        em->xFC = 1;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
    }
}

void emItem_R0_Move(cEmItem* em)
{
    EmItem_R1_move_tbl[em->xFD](em);
}

void emItem_R1_Set(cEmItem* em)
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

void emItem_R1_MedalSet(cEmItem* em)
{
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
    emItemRotMove(em);
}

void emItem_R1_Parent(cEmItem* em)
{
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    EmItemWork* w = EMITEM_WK(em);
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
#line 475 "D:/Bio4/Prog/emitem.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 477 "D:/Bio4/Prog/emitem.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 479 "D:/Bio4/Prog/emitem.cpp"
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
    emItemRotMove(em);
}

void emItem_R1_Drop(cEmItem* em)
{
    EmItemWork* w = EMITEM_WK(em);
    f32 floor;

    switch (em->xFE) {
    case 0:
        em->pos.x = em->mat[0][3];
        em->pos.y = em->mat[1][3];
        em->pos.z = em->mat[2][3];
        Matrix2AxisAngle(em->mat, &em->rot);
        w->spd.x = 0.0f;
        w->spd.y = -10.0f;
        w->spd.z = 0.0f;
        em->xFE++;
    case 1:
        w->spd.y -= 10.0f;
        floor = EatMgr.getFloor(&em->pos, 0.0f, 100000.0f, 0, 0);
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (em->pos.y < floor) {
            em->pos.y = floor;
            w->status = 1;
            em->xFE++;
        }
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        em->partsMatCalc();
        em->partsWorldCalc();
        break;
    case 2:
        em->be_flag |= 0x4000;
        break;
    }
}

void emItem_R1_Break(cEmItem* em)
{
    EmItemWork* w = EMITEM_WK(em);
    u16* flg;

    switch (em->xFE) {
    case 0:
        w->status = 2;
        em->hp = 0;
        em->be_flag &= ~2;
        flg = GetEtcFlgPtr(w->etcNo, pGS->room_id);
        if (flg) {
            *flg |= 1;
        }
        em->xFE++;
    case 1:
        em->be_flag |= 0x4000;
        break;
    }
}

void emItemYarareInit(cEmItem* em)
{
    EmItemWork* w = EMITEM_WK(em);

    switch (em->type) {
    case 0:
    default:
        YarareInitCube((cEmHit*) em, 0.0f, -(w->size.y * 0.5f), 0.0f, w->size.x * 0.5f + 50.0f, w->size.y, w->size.z * 0.5f + 50.0f, 0, 1);
        break;
    case 1:
        YarareInitCube((cEmHit*) em, 0.0f, -1000.0f, 0.0f, w->size.x * 0.5f + 50.0f, w->size.y, w->size.z * 0.5f + 50.0f, 1, 1);
        break;
    }
}

void cEmItem::setEff(u8 eff)
{
    EMITEM_WK(this)->eff = eff;
}

int cEmItem::ckStatus()
{
    return EMITEM_WK(this)->status;
}

void cEmItem::setParent(cModel* parent, int partsNo, int noNormalize)
{
    EmItemWork* w = EMITEM_WK(this);

    w->pParent = parent;
    w->partsNo = partsNo;
    w->noNormalize = noNormalize;
    xFC = 1;
    xFD = 2;
    xFE = 0;
    xFF = 0;
    ((cEm*) parent)->atari.flags &= ~0x200;
}

void cEmItem::setRotType(u8 type)
{
    EMITEM_WK(this)->rotType = type;
}

void emItemRotMove(cEmItem* em)
{
    EmItemWork* w = EMITEM_WK(em);
    Mtx tmp;
    cModel* p;

    switch (w->rotType) {
    case 1:
        p = em->getPartsPtr(0);
        PSMTXRotRad(tmp, 'x', SINF(w->rotAng.x) * w->rotAmp.x);
        PSMTXConcat(tmp, p->mat, p->mat);
        TransMatrix(p->mat, &p->worldPos);
        PSMTXRotRad(tmp, 'z', SINF(w->rotAng.z) * w->rotAmp.z);
        PSMTXConcat(tmp, p->mat, p->mat);
        TransMatrix(p->mat, &p->worldPos);
        PSMTXRotRad(tmp, 'y', SINF(w->rotAng.y) * w->rotAmp.y);
        PSMTXConcat(p->mat, tmp, p->mat);
        TransMatrix(p->mat, &p->worldPos);
        w->rotAng.x += w->rotSpd.x;
        w->rotAng.x = LIMIT_ANGLE(w->rotAng.x);
        w->rotAng.y += w->rotSpd.y;
        w->rotAng.y = LIMIT_ANGLE(w->rotAng.y);
        w->rotAng.z += w->rotSpd.z;
        w->rotAng.z = LIMIT_ANGLE(w->rotAng.z);
        break;
    case 2:
        p = em->getPartsPtr(0);
        RotMatrix(p->mat, &em->rot);
        TransMatrix(p->mat, &p->worldPos);
        break;
    }
}
