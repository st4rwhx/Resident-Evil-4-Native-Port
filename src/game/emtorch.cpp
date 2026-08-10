// game/emtorch.cpp: torch enemy (cEmTorch): candles, braziers and lamps that follow a parent's
// parts, burn a flame effect and break or fall when shot.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "emtorch.h"
#include "emhit.h"
#include "etc_model.h"
#include "esp.h"
#include "snd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" {
int MotionMove(cModel* m, int a);
void EtcSetAddAmb(cModel* m, int a);                                                         // EtcModel.cpp
void LifeDownSet(cEm* em, int dmg, int a);                                                  // em_sub.cpp
void EffectEspDelete(int a, int b, cModel* m, int c);                                        // est.cpp
void EffectEspgenDelete(int a, int b, cModel* m);
void EffectEfmDelete(int a, int b, cModel* m);
}

typedef void (*EmTorchFunc)(cEmTorch*);

static EmTorchFunc EmTorch_R0_move_tbl[4] = {
    emTorch_R0_Init,
    emTorch_R0_Move,
    0,
    0,
};

EmTorchFunc EmTorch_R1_move_tbl[4] = {
    emTorch_R1_Set,
    emTorch_R1_Parent,
    emTorch_R1_Break,
    emTorch_R1_Fall,
};

cEmTorch* SetTorch(void* bin, void* tpl, Vec* pos, Vec* rot, int type, int etcNo)
{
    cEmTorch* em;
    EmTorchWork* w;
    u16* flg;

    em = (cEmTorch*) EmMgr.create(0x47);
    if (em == 0) {
        return 0;
    }
    w = EMTORCH_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->rot = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetTorch() failed.");
        EmMgr.destroy(em);
        return 0;
    }
    em->type = type;
    if (type != 5) {
        EtcSetAddAmb(em, 0);
    } else {
        EtcSetAddAmb(em, 12);
    }
    w->eff = 0xFF;
    switch (em->type) {
    case 0:
    default:
        w->size.x = 300.0f;
        w->size.y = 300.0f;
        w->size.z = 450.0f;
        break;
    case 4:
        w->size.x = 250.0f;
        w->size.y = 500.0f;
        w->size.z = 250.0f;
        break;
    case 2:
        w->size.x = 300.0f;
        w->size.y = 300.0f;
        w->size.z = 300.0f;
        break;
    case 3:
        w->size.x = 300.0f;
        w->size.y = 600.0f;
        w->size.z = 300.0f;
        break;
    case 1:
        w->size.x = 250.0f;
        w->size.y = 700.0f;
        w->size.z = 250.0f;
        break;
    case 5:
        w->size.x = 400.0f;
        w->size.y = 600.0f;
        w->size.z = 400.0f;
        break;
    }
    em->atari.init(0, 2, 0, 0.0f, 0.0f, 0.0f, 700.0f, 400.0f, 500.0f, 500.0f);
    em->atari.setPriority(3);
    em->atari.throughOn();
    emTorchYarareInit(em);
    em->hpMax = em->hp;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 1000.0f, 1000.0f, 1000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 0x10);
    }
    em->lockParts = 0;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(1);
    em->setStatus(0xB);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    switch (em->type) {
    case 0:
    default:
        em->hp = 1000;
        break;
    case 1:
        em->hp = 1;
        break;
    case 2:
        em->hp = 1;
        break;
    case 3:
        em->hp = 1;
        break;
    case 4:
        em->hp = 1;
        break;
    case 5:
        em->hp = 1;
        break;
    }
    w->estNo = 50;
    w->flags = 0;
    w->etcNo = etcNo;
    flg = GetEtcFlgPtr(etcNo, pGS->room_id);
    if (flg && (*flg & 1)) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        em->xFC = 1;
        em->xFD = 2;
        em->xFE = 0;
        em->xFF = 0;
    } else {
        em->xFC = 1;
        em->xFD = 0;
        em->xFE = 0;
        em->xFF = 0;
    }
    return em;
}

void emTorchDmCk(cEmTorch* em)
{
    EmTorchWork* w = EMTORCH_WK(em);
    u8 wep;
    int dmg;

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
    switch (wep) {
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        em->dmType = 0;
        break;
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
        dmg = 999;
        break;
    case 7:
    case 8:
    case 0x21:
        if (em->plDist2 > 36000000.0f) {
            dmg = 999;
        } else {
            dmg = 9999;
        }
        break;
    case 5:
    case 6:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2A:
    case 0x2C:
    case 0x2D:
    default:
        dmg = 9999;
        break;
    }
    LifeDownSet(em, dmg, 0);
    if (em->type == 5) {
        EstSet(0, -1, &em->pos, &em->rot, w->eff, 1, 0, 0, 0, 0);
    }
    if (em->hp <= 0) {
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
            emTorchSetBreak(em, 0);
            break;
        case 7:
        case 8:
        case 0x21:
            if (em->dmRad > 36000000.0f) {
                emTorchSetBreak(em, 0);
            } else {
                emTorchSetBreak(em, 1);
            }
            break;
        case 5:
        case 6:
        case 0xD:
        case 0xE:
        case 0xF:
        case 0x12:
        case 0x13:
        case 0x29:
        case 0x2A:
        case 0x2C:
        case 0x2D:
        default:
            emTorchSetBreak(em, 2);
            break;
        }
    } else {
        if (em->type == 0) {
            SndCall(1, 0x3F, &em->pos, 0, 0, em);
        }
        if (w->eff != 0xFF) {
            EstSet((int) em, -1, 0, 0, w->eff, 1, 0, 0, (u32) em, 0);
        }
    }
}

void emTorchSetBreak(cEmTorch* em, u32 kind)
{
    EmTorchWork* w = EMTORCH_WK(em);

    em->hp = 0;
    if (w->eff != 0xFF && em->type != 5) {
        EffectEspDelete(1, w->estNo, em, 0);
        EffectEspgenDelete(1, w->estNo, em);
        EffectEfmDelete(1, w->estNo, em);
        switch (kind) {
        default:
            EstSet((int) em, -1, 0, 0, w->eff, 2, 0, 0, (u32) em, 0);
            break;
        case 0:
            EstSet((int) em, -1, 0, 0, w->eff, 2, 0, 0, (u32) em, 0);
            break;
        case 1:
            EstSet((int) em, -1, 0, 0, w->eff, 2, 0, 0, (u32) em, 0);
            break;
        case 2:
            EstSet((int) em, -1, 0, 0, w->eff, 3, 0, 0, (u32) em, 0);
            break;
        }
    }
    switch (em->type) {
    case 0:
        em->be_flag &= ~2;
        SndCall(1, 0x40, &em->pos, 0, 0, em);
        em->xFC = 1;
        em->xFD = 2;
        em->xFE = 0;
        em->xFF = 0;
        break;
    case 2:
    case 3:
        SndCall(6, 0x12, &em->pos, 0, 0, em);
        break;
    case 1:
    case 4:
        em->be_flag &= ~2;
        SndCall(6, 0x2A, &em->pos, 0, 0, em);
        em->xFC = 1;
        em->xFD = 2;
        em->xFE = 0;
        em->xFF = 0;
        break;
    case 5:
        em->xFC = 1;
        em->xFD = 3;
        em->xFE = 0;
        em->xFF = 0;
        break;
    }
}

void cEmTorch::move()
{
    emTorchDmCk(this);
    be_flag &= ~0x4000;
    EmTorch_R0_move_tbl[xFC](this);
}

void emTorch_R0_Init(cEmTorch* em)
{
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
}

void emTorch_R0_Move(cEmTorch* em)
{
    EmTorch_R1_move_tbl[em->xFD](em);
}

void emTorch_R1_Set(cEmTorch* em)
{
    EmTorchWork* w = EMTORCH_WK(em);

    if (em->xFE == 0) {
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        em->partsMatCalc();
        em->partsWorldCalc();
        w->timer = 30;
        em->xFE++;
    }
    em->be_flag |= 0x4000;
}

void emTorch_R1_Parent(cEmTorch* em)
{
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    EmTorchWork* w = EMTORCH_WK(em);
    cModel* parent = w->pParent;

    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    if (parent && parent->pParts) {
        PSMTXConcat(parent->getPartsPtr(w->partsNo)->mat, em->mat, m);
        if (!(w->flags & 1)) {
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
#line 617 "D:/Bio4/Prog/emtorch.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 619 "D:/Bio4/Prog/emtorch.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 621 "D:/Bio4/Prog/emtorch.cpp"
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

void emTorch_R1_Break(cEmTorch* em)
{
    EmTorchWork* w = EMTORCH_WK(em);
    u16* flg;

    if (em->xFE == 0) {
        flg = GetEtcFlgPtr(w->etcNo, pG->room_id);
        if (flg) {
            *flg |= 1;
        }
        em->hp = 0;
        em->be_flag &= ~2;
        w->x60 = 150;
        em->xFE++;
    }
    em->be_flag |= 0x4000;
}

void emTorch_R1_Fall(cEmTorch* em)
{
    EmTorchWork* w = EMTORCH_WK(em);
    u16* flg;
    f32 floor;

    switch (em->xFE) {
    case 0:
        flg = GetEtcFlgPtr(w->etcNo, pG->room_id);
        if (flg) {
            *flg |= 1;
        }
        em->hp = 0;
        em->be_flag &= ~2;
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        SndCall(6, 0x2D, &em->pos, 0, 0, em);
        em->xFE++;
    case 1:
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        w->spd.y -= 20.0f;
        floor = EatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < floor) {
            em->pos.y = floor;
            EffectEspDelete(1, w->estNo, em, 0);
            EffectEspgenDelete(1, w->estNo, em);
            EffectEfmDelete(1, w->estNo, em);
            EstSet(0, -1, &em->pos, 0, w->eff, 2, 0, 0, 0, 0);
            SndCall(6, 0x58, &em->pos, 0, 0, em);
            DmgMgr.set(5, 0x4B, &em->pos, 2500.0f, 1500.0f);
            em->be_flag &= ~2;
            em->xFE++;
        } else {
            RotMatrix(em->mat, &em->rot);
            TransMatrix(em->mat, &em->pos);
            ScaleMatrix(em->mat, &em->scale);
            if (em->pMotion) {
                em->motFlags2 |= 0x40000000;
                MotionMove(em, 0);
            } else {
                em->partsMatCalc();
            }
            em->partsWorldCalc();
            em->be_flag |= 0x4000;
        }
        break;
    case 2:
        em->be_flag |= 0x4000;
        break;
    }
}

void emTorchYarareInit(cEmTorch* em)
{
    EmTorchWork* w = EMTORCH_WK(em);

    switch (em->type) {
    case 0:
    case 4:
    default:
        YarareInitCube((cEmHit*) em, 0.0f, 0.0f, 0.0f, w->size.x * 0.5f + 50.0f, w->size.y, w->size.z * 0.5f + 50.0f, 0, 1);
        break;
    case 1:
        YarareInitCube((cEmHit*) em, 0.0f, -w->size.y, 0.0f, w->size.x * 0.5f + 50.0f, w->size.y, w->size.z * 0.5f + 50.0f, 0, 1);
        break;
    case 2:
    case 3:
        break;
    case 5:
        YarareInitCube((cEmHit*) em, 0.0f, -w->size.y - 100.0f, 0.0f, w->size.x * 0.5f + 50.0f, w->size.y, w->size.z * 0.5f + 50.0f, 0, 1);
        break;
    }
}

void cEmTorch::setBreak()
{
    if (hp > 0) {
        emTorchSetBreak(this, 0);
    }
}

void cEmTorch::setDelete()
{
    xFC = 1;
    xFD = 2;
    xFE = 0;
    xFF = 0;
}

void cEmTorch::setEff(u8 eff)
{
    EmTorchWork* w = EMTORCH_WK(this);

    w->eff = eff;
    if (hp > 0) {
        EstSet((int) this, -1, 0, 0, w->eff, 0, 1, w->estNo, (u32) this, 0);
    }
}

void cEmTorch::setParent(cModel* parent, int partsNo, int flag)
{
    EmTorchWork* w = EMTORCH_WK(this);

    w->pParent = parent;
    w->partsNo = partsNo;
    if (flag) {
        w->flags |= 1;
    } else {
        w->flags &= ~1;
    }
    xFC = 1;
    xFD = 1;
    xFE = 0;
    xFF = 0;
}
