#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "obj.h"
#include "esp.h"
#include "est.h"
#include "emhit.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "rnd.h"
#include "pl_wep.h"
#include "player.h"

// Player sub weapons: the thrown hand grenade (cObjGrenade), incendiary grenade (cObjGreFire),
// flash grenade (cObjGreLight) and the egg (cObjEgg) share cSubWep's flight, bounce and water
// handling; each supplies its explosion.

class cSubWep : public cObj {
public:
    cSubWep();
    virtual ~cSubWep() {}
    virtual void beginEvent();
    virtual void move();
    virtual void explode() = 0;
    virtual void waterExplode() = 0;

    void moveNormal();
    void moveWater();
    void scrAdjust();
    void dmgSet(int kind);
    void addSpeed();
    void bounce(Vec* nrm);
    int getEffectType();
    int init(Vec* rot, f32 power);
};

class cObjGrenade : public cSubWep {
public:
    cObjGrenade();
    virtual ~cObjGrenade() {}
    virtual void explode();
    virtual void waterExplode();
};

class cObjGreFire : public cSubWep {
public:
    cObjGreFire();
    virtual ~cObjGreFire() {}
    virtual void explode();
    virtual void waterExplode();
};

class cObjGreLight : public cSubWep {
public:
    cObjGreLight();
    virtual ~cObjGreLight() {}
    virtual void explode();
    virtual void waterExplode();
};

class cObjEgg : public cSubWep {
public:
    cObjEgg();
    virtual ~cObjEgg() {}
    virtual void explode();
    virtual void waterExplode();
};

extern "C" {
void setThrowSpeed(Vec* spd, f32 power);
}

void cSubWep::move()
{
    static void (cSubWep::*funcTbl[2])() = { &cSubWep::moveNormal, &cSubWep::moveWater };

    (this->*funcTbl[xFC])();
}

void cSubWep::moveNormal()
{

    if (subWep.life >= 0) {
        if (subWep.life > 0) {
            subWep.life--;
        } else {
            if (type == 0) {
                scrAdjust();
                explode();
            }
            ObjMgr.destroy(this);
            return;
        }
    }
    addSpeed();
    {
        cModel* parts = getPartsPtr(0);
        if (parts) {
            PSVECAdd(&parts->rot, &subWep.rotSpd, &parts->rot);
            parts->rot.x = LIMIT_ANGLE(parts->rot.x);
            parts->rot.y = LIMIT_ANGLE(parts->rot.y);
            parts->rot.z = LIMIT_ANGLE(parts->rot.z);
            RotMatrix(parts->worldMat, &parts->rot);
            TransMatrix(parts->worldMat, &parts->pos);
            ScaleMatrix(parts->worldMat, &parts->scale);
        }
    }
    matUpdate();
}

void cSubWep::moveWater()
{

    subWep.life--;
    if (subWep.life > 0) {
        return;
    }
    if (!(subWep.effNo == 0xD2 && subWep.effPrm == 1)) {
        EstSet(0, -1, &pos, 0, subWep.effNo, subWep.effPrm, 0, 0, 0, 0);
        if (subWep.effNo == 0 && subWep.effPrm == 0x15) {
            Vec a;
            Vec b;
            Vec nrm;

            a.x = pos.x;
            a.y = pos.y - 300.0f;
            a.z = pos.z;
            b.x = pos.x;
            b.y = pos.y + 300.0f;
            b.z = pos.z;
            if (EatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0) == 0 || nrm.y > 0.9f) {
                EstSet(0, -1, &pos, 0, 0, 0x28, 0, 0, 0, 0);
            }
        }
    }
    AddWaterPower(&pos, 1.0f);
    waterExplode();
    ObjMgr.destroy(this);
}

void cSubWep::scrAdjust()
{
    Vec p;
    Vec a;
    Vec hit;
    Vec nrm;
    Vec n;
    const f32 ofs = 400.0f;

    p.x = 0.0f;
    p.y = 300.0f;
    p.z = 0.0f;
    PSMTXMultVec(mat, &p, &p);
    a.x = p.x + ofs;
    a.y = p.y;
    a.z = p.z;
    if (EatMgr.hitCheck(&p, &a, &hit, &n, 0, 0)) {
        nrm.x = n.x;
        nrm.y = 0.0f;
        nrm.z = n.z;
        if (!(nrm.x == 0.0f && nrm.z == 0.0f)) {
#line 170 "D:/Bio4/Prog/objSubWep.cpp"
            VECNormalize(&nrm, &nrm);
            PSVECScale(&nrm, &nrm, ofs);
            PSVECAdd(&hit, &nrm, &pos);
        }
    }
    a.x = p.x - ofs;
    a.y = p.y;
    a.z = p.z;
    if (EatMgr.hitCheck(&p, &a, &hit, &n, 0, 0)) {
        nrm.x = n.x;
        nrm.y = 0.0f;
        nrm.z = n.z;
        if (!(nrm.x == 0.0f && nrm.z == 0.0f)) {
#line 180 "D:/Bio4/Prog/objSubWep.cpp"
            VECNormalize(&nrm, &nrm);
            PSVECScale(&nrm, &nrm, ofs);
            PSVECAdd(&hit, &nrm, &pos);
        }
    }
    a.x = p.x;
    a.y = p.y;
    a.z = p.z + ofs;
    if (EatMgr.hitCheck(&p, &a, &hit, &n, 0, 0)) {
        nrm.x = n.x;
        nrm.y = 0.0f;
        nrm.z = n.z;
        if (!(nrm.x == 0.0f && nrm.z == 0.0f)) {
#line 190 "D:/Bio4/Prog/objSubWep.cpp"
            VECNormalize(&nrm, &nrm);
            PSVECScale(&nrm, &nrm, ofs);
            PSVECAdd(&hit, &nrm, &pos);
        }
    }
    a.x = p.x;
    a.y = p.y;
    a.z = p.z - ofs;
    if (EatMgr.hitCheck(&p, &a, &hit, &n, 0, 0)) {
        nrm.x = n.x;
        nrm.y = 0.0f;
        nrm.z = n.z;
        if (!(nrm.x == 0.0f && nrm.z == 0.0f)) {
#line 200 "D:/Bio4/Prog/objSubWep.cpp"
            VECNormalize(&nrm, &nrm);
            PSVECScale(&nrm, &nrm, ofs);
            PSVECAdd(&hit, &nrm, &pos);
        }
    }
}

void cSubWep::dmgSet(int kind)
{
    switch (kind) {
    case 8:
        DmgMgr.set(2, 2, &pos, 3000.0f, 3000.0f);
        DmgMgr.set(8, 2, &pos, 15000.0f, 3000.0f);
        break;
    case 1:
        DmgMgr.set(1, 75, &pos, 2500.0f, 1500.0f);
        break;
    }
}

void cSubWep::addSpeed()
{
    Vec old;
    Vec hit;
    Vec nrm;
    f32 wh;
    AtEffInfo* info;

    VehicleAdjust(&pos);
    old = pos;
    subWep.spd.y -= subWep.grav;
    PSVECAdd(&pos, &subWep.spd, &pos);
    EatMgr.hitCheck(&old, &pos, &hit, 0, 0, 0x4000);
    if (GetWaterHeight(&pos, &wh) && pos.y <= wh && hit.y < wh) {
        pos.y = wh + 20.0f;
        info = EatMgr.getEffInfo(2);
        if (info == 0) {
            pLog->err(0, 0, "GRENADE CANT FOUND WATER INFORMATION");
            pLog->err(0, 0, "  PLEASE SET EatMgr.registEffInfo()");
            return;
        }
        subWep.attr = info->flags;
        switch (type) {
        case 0:
        default:
            subWep.effNo = info->eff13[0];
            subWep.effPrm = info->eff13[1];
            break;
        case 1:
            subWep.effNo = info->eff16[0];
            subWep.effPrm = info->eff16[1];
            break;
        case 2:
            subWep.effNo = info->eff17[0];
            subWep.effPrm = info->eff17[1];
            break;
        case 3:
            subWep.effNo = info->eff17[0];
            subWep.effPrm = info->eff17[1];
            break;
        case 4:
            subWep.effNo = info->eff17[0];
            subWep.effPrm = info->eff17[1];
            break;
        case 5:
            subWep.effNo = info->eff17[0];
            subWep.effPrm = info->eff17[1];
            break;
        }
        if (type > 2) {
            if (info->eff0[0] != 0xD2) {
                EstSet(0, -1, &pos, 0, info->eff0[0], (u8) info->eff0[1], 0, 0, 0, 0);
            } else if (info->eff0[1] != 1) {
                EstSet(0, -1, &pos, 0, 0, 0x3A, 0, 0, 0, 0);
            }
            SndCall(5, 0x24, &pos, 0, 0, 0);
            AddWaterPower(&pos, 0.5f);
            ObjMgr.destroy(this);
        } else if (type == 0) {
            if (info->eff0[0] != 0xD2) {
                EstSet(0, -1, &pos, 0, info->eff0[0], (u8) info->eff0[1], 0, 0, 0, 0);
            } else if (info->eff0[1] != 1) {
                EstSet(0, -1, &pos, 0, 0, 0x3A, 0, 0, 0, 0);
            }
            SndCall(5, 0x24, &pos, 0, 0, 0);
            AddWaterPower(&pos, 0.5f);
            xFC = 1;
            be_flag &= ~2;
        } else {
            subWep.life = 1;
            moveWater();
        }
        return;
    }
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &oldPos, &pos, subWep.rad * 0.5f, 0x2001, 0x4000);
    if (nrm.x == 0.0f && nrm.y == 0.0f && nrm.z == 0.0f) {
        return;
    }
    info = EatMgr.getEffInfo(getEffectType());
    if (info) {
        subWep.attr = info->flags | 0x80000000;
        switch (type) {
        case 0:
        default:
            subWep.effNo = info->eff13[0];
            subWep.effPrm = info->eff13[1];
            break;
        case 1:
            subWep.effNo = info->eff16[0];
            subWep.effPrm = info->eff16[1];
            break;
        case 2:
            subWep.effNo = info->eff17[0];
            subWep.effPrm = info->eff17[1];
            break;
        }
    } else {
        subWep.attr = 0;
    }
    if (subWep.attr & 1) {
        if (type > 2) {
            if (info->eff0[0] != 0xD2) {
                EstSet(0, -1, &pos, 0, info->eff0[0], (u8) info->eff0[1], 0, 0, 0, 0);
            } else if (info->eff0[1] != 1) {
                EstSet(0, -1, &pos, 0, 0, 0x3A, 0, 0, 0, 0);
            }
            SndCall(5, 0x24, &pos, 0, 0, 0);
            AddWaterPower(&pos, 0.5f);
            ObjMgr.destroy(this);
        } else if (type == 0) {
            if (info->eff0[0] != 0xD2) {
                EstSet(0, -1, &pos, 0, info->eff0[0], (u8) info->eff0[1], 0, 0, 0, 0);
            } else if (info->eff0[1] != 1) {
                EstSet(0, -1, &pos, 0, 0, 0x3A, 0, 0, 0, 0);
            }
            SndCall(5, 0x24, &pos, 0, 0, 0);
            xFC = 1;
            be_flag &= ~2;
        } else {
            subWep.life = 1;
            moveWater();
        }
    } else {
        bounce(&nrm);
    }
}

void cSubWep::bounce(Vec* nrm)
{
    Vec ref;
    f32 len;
    const f32 lim = 0.7f;
    const f32 minSpd = 50.0f;
    const f32 rate = 0.5f;
    const f32 rotRate = -0.8f;

    len = RootSumSquare3(&subWep.spd);
    C_VECReflect(&subWep.spd, nrm, &ref);
    PSVECScale(&ref, &subWep.spd, len * rate);
    if (nrm->y > 0.0f && nrm->y < lim) {
        if (subWep.spd.y < minSpd) {
            subWep.spd.y = minSpd;
        }
    }
    PSVECScale(&subWep.rotSpd, &subWep.rotSpd, rotRate);
    if (nrm->y > lim) {
        if (fabsf(subWep.spd.y) > 10.0f) {
            if (subWep.flags & 1) {
                scrAdjust();
                explode();
                ObjMgr.destroy(this);
            } else if (subWep.seCnt0 <= 3) {
                SndCall(5, 6, &pos, 0, 0, 0);
                subWep.seCnt0++;
            }
        }
    } else if ((subWep.flags & 2) || (type == 1 && nrm->y > lim)) {
        subWep.flags |= 0x10;
        explode();
        ObjMgr.destroy(this);
    } else if (subWep.seCnt1 <= 3) {
        SndCall(1, 0x21, &pos, 0, 0, 0);
        subWep.seCnt1++;
    }
}

int cSubWep::getEffectType()
{
    Vec d;
    u32 attr;

#line 484 "D:/Bio4/Prog/objSubWep.cpp"
    VECNormalize(&subWep.spd, &d);
    PSVECScale(&d, &d, 3000.0f);
    PSVECAdd(&d, &pos, &d);
    attr = EatMgr.hitCheck(&pos, &d, 0, 0, 0, 0);
    if (attr & 0x1000000) {
        return EatGetEffectType(attr);
    }
    return 0;
}

cSubWep::cSubWep()
{
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 1000.0f, 1000.0f, 0.0f };

    sub2B4.atari.throughOn();
    lightInfo.init2(0, 1, &p0, &p1, 4);
    subWep.seCnt0 = 0;
    subWep.seCnt1 = 0;
    subWep.flags = 0;
    subWep.grav = 20.0f;
    subWep.rad = 50.0f;
    subWep.life = 10;
    subWep.x7C = 3;
    subWep.rotSpd.x = fRand0_1() * 0.19634955f + 0.09817477f;
    subWep.rotSpd.y = 0.0f;
    subWep.rotSpd.z = fRand0_1() * 0.09817477f + 0.09817477f;
    if (Rnd() & 1) {
        subWep.rotSpd.x = -subWep.rotSpd.x;
    }
    if (Rnd() & 1) {
        subWep.rotSpd.z = -subWep.rotSpd.z;
    }
}

int cSubWep::init(Vec* rot, f32 power)
{
    Vec p;
    Vec d;
    cModel* parts;
    cModel* parts2;
    void* bin;
    void* tpl;

    switch (type) {
    case 0:
    default:
        bin = PL_ARC_PTR(pG->pPlArc, 0x6A);
        tpl = PL_ARC_PTR(pG->pPlArc, 0x6B);
        break;
    case 1:
        bin = PL_ARC_PTR(pG->pPlArc, 0x6A);
        tpl = PL_ARC_PTR(pG->pPlArc, 0x6D);
        break;
    case 2:
        bin = PL_ARC_PTR(pG->pPlArc, 0x6A);
        tpl = PL_ARC_PTR(pG->pPlArc, 0x6F);
        break;
    case 3:
        bin = PL_ARC_PTR(pG->pPlArc, 0x7D);
        tpl = PL_ARC_PTR(pG->pPlArc, 0x7E);
        break;
    case 4:
        bin = PL_ARC_PTR(pG->pPlArc, 0x7D);
        tpl = PL_ARC_PTR(pG->pPlArc, 0x7F);
        break;
    case 5:
        bin = PL_ARC_PTR(pG->pPlArc, 0x7D);
        tpl = PL_ARC_PTR(pG->pPlArc, 0x80);
        break;
    }
    if (modelInit(bin, tpl) == 0) {
        ObjMgr.destroy(this);
        return 0;
    }
    if (type >= 3 && type <= 5) {
        scale.x = 0.5f;
        scale.y = 0.5f;
        scale.z = 0.5f;
    }
    parts = pPL->getPartsPtr(0);
    parts2 = pPL->getPartsPtr(10);
    if (EatMgr.hitCheck(&parts->worldPos, &parts2->worldPos, &p, &d, 0, 0) & 0x1000000) {
        PSVECScale(&d, &d, 500.0f);
        PSVECAdd(&p, &d, &p);
    }
    setPos(&p);
    this->rot = *rot;
    setThrowSpeed(&subWep.spd, power);
    switch (type) {
    case 0:
        subWep.life = 45;
        break;
    case 1:
        subWep.life = 300;
        break;
    case 2:
        subWep.life = 300;
        break;
    case 3:
        subWep.life = 300;
        break;
    case 4:
        subWep.life = 300;
        break;
    case 5:
        subWep.life = 300;
        break;
    }
    return 1;
}

void setThrowSpeed(Vec* spd, f32 power)
{
    static const Vec speedGre = { 0.0f, 30.000002f, 283.5f };
    static const Vec speedEgg = { 0.0f, 5.0f, 500.0f };
    static Vec h_ang = { -0.2617994f, 0.0f, 0.0f };
    Vec v;
    Vec ang;
    Vec d;
    cModel* parts;

    switch (pG->wep_no) {
    default:
        v = speedGre;
        break;
    case 0x19:
    case 0x1F:
    case 0x20:
        v = speedEgg;
        break;
    }
    if (power > 0.1f) {
        PSVECScale(&v, &v, power + 1.0f);
    } else if (power < -0.2f) {
        PSVECScale(&v, &v, power * 0.4f + 1.0f);
    } else {
        RotVector(&v, &h_ang, &v);
    }
    v.x = fRand1_1() * 15.0f;
    ang.z = 0.0f;
    ang.y = 0.0f;
    ang.x = power * -0.7853982f;
    RotVector(&v, &ang, &v);
    PSMTXMultVecSR(pPL->mat, &v, spd);
    parts = pPL->getPartsPtr(0);
    PSVECSubtract(&parts->worldPos, &parts->x88, &d);
    PSVECAdd(spd, &d, spd);
}

void cSubWep::beginEvent()
{
    ObjMgr.destroy(this);
}

cObjGrenade::cObjGrenade()
{
}

void cObjGrenade::explode()
{
    f32 wh;
    int no;
    int prm;
    const f32 up = 1000.0f;
    const f32 down = 3000.0f;

    if (GetWaterHeight(&pos, &wh) && pos.y <= wh) {
        EspSetWaterBomb(&pos);
        AddWaterPower(&pos, 1.0f);
        SndCall(1, 0x17, &pos, 0, 0, 0);
    } else {
        if (subWep.effNo == 0xD2 && subWep.effPrm == 1) {
            return;
        }
        if (subWep.attr < 0 && subWep.effNo != 0xD2) {
            no = (u8) subWep.effNo;
            prm = subWep.effPrm;
        } else {
            Vec a;
            Vec b;
            Vec nrm;
            u32 attr;

            a.x = pos.x;
            a.y = pos.y + up;
            a.z = pos.z;
            b.x = pos.x;
            b.y = pos.y - down;
            b.z = pos.z;
            attr = EatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0);
            if (nrm.y > 0.9f && !(attr & 0x40)) {
                EstSet(0, -1, &pos, 0, 0, 0x1A, 0, 0, 0, 0);
            }
            no = 0;
            prm = 0xD;
        }
        EstSet(0, -1, &pos, 0, no, prm, 0, 0, 0, 0);
        SndCall(1, 0x14, &pos, 0, 0, 0);
    }
    BitOn(pG->flags_500C, 0x800000);
    PlWepHitCheck2(0, &pos, &pos, 0x13, 0, 6000.0f);
    BitOn(pG->flags_5010, 0x20000000);
    memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &pos, sizeof(Vec));
    pG->bell_stat = 1;
}

void cObjGrenade::waterExplode()
{
    BitOn(pG->flags_500C, 0x800000);
    PlWepHitCheck2(0, &pos, &pos, 0x13, 0, 6000.0f);
    SndCall(1, 0x17, &pos, 0, 0, 0);
    BitOn(pG->flags_5010, 0x20000000);
    memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &pos, sizeof(Vec));
    pG->bell_stat = 1;
}

cObjGreFire::cObjGreFire()
{
    subWep.flags |= 1;
}

void cObjGreFire::explode()
{
    f32 wh;
    int no;
    int prm;
    const f32 up = 1000.0f;
    const f32 down = 3000.0f;

    if (GetWaterHeight(&pos, &wh) && pos.y <= wh) {
        EspSetWaterBomb(&pos);
        AddWaterPower(&pos, 1.0f);
        SndCall(1, 0x17, &pos, 0, 0, 0);
    } else {
        if (subWep.effNo == 0xD2 && subWep.effPrm == 1) {
            return;
        }
        if (subWep.attr < 0 && subWep.effNo != 0xD2) {
            no = (u8) subWep.effNo;
            prm = subWep.effPrm;
        } else {
            Vec a;
            Vec b;
            Vec nrm;
            u32 attr;

            a.x = pos.x;
            a.y = pos.y + up;
            a.z = pos.z;
            b.x = pos.x;
            b.y = pos.y - down;
            b.z = pos.z;
            attr = EatMgr.hitCheck(&a, &b, 0, &nrm, 0, 0);
            if (nrm.y > 0.9f && !(attr & 0x40)) {
                if (pos.y - SatMgr.getFloor(&pos, 600.0f, 100000.0f, 0, 0) < 200.0f) {
                    EstSet(0, -1, &pos, 0, 0, 0x26, 0, 0, 0, 0);
                }
            }
            no = 0;
            prm = 0xB;
        }
        EstSet(0, -1, &pos, 0, no, prm, 0, 0, 0, 0);
        SndCall(1, 0x22, &pos, 0, 0, 0);
        dmgSet(1);
    }
    BitOn(pG->flags_500C, 0x800000);
    BitOn(pG->flags_5010, 0x20000000);
    memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &pos, sizeof(Vec));
    pG->bell_stat = 1;
}

void cObjGreFire::waterExplode()
{
    SndCall(1, 0x23, &pos, 0, 0, 0);
}

cObjGreLight::cObjGreLight()
{
    subWep.flags |= 1;
}

void cObjGreLight::explode()
{
    f32 wh;
    int no;
    int prm;

    if (GetWaterHeight(&pos, &wh) && pos.y <= wh) {
        EspSetWaterBomb(&pos);
        AddWaterPower(&pos, 1.0f);
        SndCall(1, 0x17, &pos, 0, 0, 0);
    } else {
        if (subWep.effNo == 0xD2 && subWep.effPrm == 1) {
            return;
        }
        if (subWep.attr < 0 && subWep.effNo != 0xD2) {
            no = (u8) subWep.effNo;
            prm = subWep.effPrm;
        } else {
            EstSet(0, -1, 0, 0, 0, 0x3F, 0, 0, 0, 0);
            no = 0;
            prm = 0xC;
        }
        EstSet(0, -1, &pos, 0, no, prm, 0, 0, 0, 0);
        SndCall(1, 0x13, &pos, 0, 0, 0);
    }
    BitOn(pG->flags_500C, 0x800000);
    PlWepHitCheck2(0, &pos, &pos, 0x17, 0, 15000.0f);
    BitOn(pG->flags_5010, 0x20000000);
    memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &pos, sizeof(Vec));
    pG->bell_stat = 1;
}

void cObjGreLight::waterExplode()
{
    SndCall(1, 0x24, &pos, 0, 0, 0);
}

cObjEgg::cObjEgg()
{
    subWep.flags |= 3;
}

void cObjEgg::explode()
{
    f32 wh;

    if (GetWaterHeight(&pos, &wh) && pos.y <= wh) {
        AddWaterPower(&pos, 1.0f);
    } else {
        if (subWep.flags & 0x10) {
            EstSet(0, -1, &pos, 0, 0, 0x43, 0, 0, 0, 0);
        } else {
            EstSet(0, -1, &pos, 0, 0, 0x42, 0, 0, 0, 0);
        }
        SndCall(1, 6, &pos, 0, 0, 0);
    }
    BitOn(pG->flags_500C, 0x800000);
    PlWepHitCheck2(0, &pos, &pos, 0x19, 0, 2000.0f);
    BitOn(pG->flags_5010, 0x20000000);
    memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &pos, sizeof(Vec));
    pG->bell_stat = 1;
}

void cObjEgg::waterExplode()
{
}
