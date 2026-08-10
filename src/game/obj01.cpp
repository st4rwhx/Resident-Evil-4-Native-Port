#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "obj.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "rnd.h"
#include "pl_wep.h"

// Grenade (hand / incendiary / flash): thrown under gravity, bounces off the scenario, explodes
// or drowns when its fuse runs out; can be held by a model until `holdTimer` expires.
class cObj01 : public cObj {
public:
    virtual void move();
    virtual ~cObj01() {}

    void move00();
    void move01();
    void dmgSet(int kind);
};

extern "C" {
int MotionMove(cModel* m, int a);
int obj01AddSpeed(cObj01* obj);
}
int MotionSetCore(cModel* m, void* work, void* mot, int a, int b, int c, int d);

void cObj01::move()
{
    static void (cObj01::*funcTbl[2])() = { &cObj01::move00, &cObj01::move01 };

    (this->*funcTbl[xFC])();
}

void cObj01::move00()
{
    Obj01Work* w = &o1;
    int life = w->life;
    f32 wh;

    if (life) {
        if (w->type != 2) {
            w->life = life - 1;
        }
    } else {
        if (w->estNo0 != -1 && w->estPrm0 != -1) {
            switch (w->type) {
            case 1:
                BitOn(pG->flags_500C, 0x800000);
                if (GetWaterHeight(&pos, &wh) && pos.y <= wh) {
                    EstSet(0, -1, &pos, 0, w->estNo3, (u8) w->estPrm3, 0, 0, 0, 0);
                    AddWaterPower(&pos, 1.0f);
                    SndCall(1, 0x17, &pos, 0, 0, 0);
                } else {
                    EstSet(0, -1, &pos, 0, w->estNo0, (u8) w->estPrm0, 0, 0, 0, 0);
                    SndCall(1, 0x14, &pos, 0, 0, 0);
                }
                PlWepHitCheck2(0, &pos, &pos, 0x13, 0, 6000.0f);
                BitOn(pG->flags_5010, 0x20000000);
                memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &pos, sizeof(Vec));
                pG->bell_stat = 1;
                ObjMgr.destroy(this);
                return;
            case 2:
                BitOn(pG->flags_500C, 0x800000);
                EstSet(0, -1, &pos, 0, w->estNo0, (u8) w->estPrm0, 0, 0, 0, 0);
                EstSet(0, -1, &pos, 0, w->estNo1, (u8) w->estPrm1, 0, 0, 0, 0);
                SndCall(1, 0x15, &pos, 0, 0, 0);
                SndCall(1, 0x16, &pos, 0, 0, 0);
                if (w->type == 2) {
                    dmgSet(4);
                } else {
                    dmgSet(5);
                }
                xFC = 1;
                return;
            case 3:
                BitOn(pG->flags_500C, 0x800000);
                EstSet(0, -1, &pos, 0, w->estNo0, (u8) w->estPrm0, 0, 0, 0, 0);
                EstSet((int) this, -1, 0, 0, w->estNo1, (u8) w->estPrm1, 0, 0, (u32) this, 0);
                SndCall(6, 0, &pos, 0, 0, 0);
                if (w->type == 2) {
                    dmgSet(4);
                } else {
                    dmgSet(5);
                }
                xFC = 1;
                return;
            case 0:
            default:
                break;
            }
        } else {
            alpha -= 0.2f;
            if (alpha <= 0.0f) {
                ObjMgr.destroy(this);
            }
            return;
        }
        ObjMgr.destroy(this);
        return;
    }
    if (w->flags & 1) {
        MotionSetCore(this, &pMotion, w->pMot, 0, 0, w->motPrm, 0);
        w->flags = (w->flags & ~1) | 2;
    }
    if (w->flags & 2) {
        MotionMove(this, 0);
    }
    if (w->hold) {
        if (w->holdTimer) {
            w->holdTimer--;
            if (w->holdTimer == 0) {
                cModel* parts;
                Vec hit;
                Vec dir;

                parts = GetPartsAddr(w->hold->pParts, 0);
                if (SatMgr.hitCheck(&parts->worldPos, &pos, &hit, 0, 0, 0)) {
                    PSVECSubtract(&parts->worldPos, &hit, &dir);
#line 172 "D:/Bio4/Prog/obj01.cpp"
                    VECNormalize(&dir, &dir);
                    PSVECScale(&dir, &dir, w->rad);
                    PSVECAdd(&hit, &dir, &dir);
                    pos = dir;
                    TransMatrix(mat, &pos);
                }
                oldPos = pos;
                w->hold = 0;
            }
        }
        if (w->hold == 0) {
            if (obj01AddSpeed(this)) {
                ObjMgr.destroy(this);
                return;
            }
        }
    } else if (obj01AddSpeed(this)) {
        ObjMgr.destroy(this);
        return;
    }
    if (w->flags & 8) {
        if (w->hold == 0) {
            cModel* parts = GetPartsAddr(pParts, 0);
            if (parts) {
                PSVECAdd(&parts->rot, &w->rotSpd, &parts->rot);
                parts->rot.x = LIMIT_ANGLE(parts->rot.x);
                parts->rot.y = LIMIT_ANGLE(parts->rot.y);
                parts->rot.z = LIMIT_ANGLE(parts->rot.z);
                RotMatrix(parts->worldMat, &parts->rot);
                TransMatrix(parts->worldMat, &parts->pos);
                ScaleMatrix(parts->worldMat, &parts->scale);
            }
        }
    }
    if (w->hold) {
        if ((w->hold->be_flag & 0x201) != 1) {
            w->hold = 0;
        }
    }
    if (w->hold) {
        cModel* parts = w->hold->getPartsPtr(w->holdParts);
        RotMatrix(mat, &w->holdRot);
        TransMatrix(mat, &w->holdOfs);
        ScaleMatrix(mat, &scale);
        PSMTXMultVec(parts->mat, &w->holdOfs, &pos);
        PSMTXConcat(parts->mat, mat, mat);
        TransMatrix(mat, &pos);
        alpha = w->hold->alpha;
        x158 = w->hold->x158;
    } else {
        RotMatrix(worldMat, &rot);
        TransMatrix(worldMat, &pos);
        ScaleMatrix(worldMat, &scale);
        PSMTXCopy(worldMat, mat);
        alpha = 1.0f;
        x158 = 1.0f;
    }
    partsMatCalc();
    partsWorldCalc();
}

void cObj01::move01()
{
    xFE++;
    if (xFE > 30) {
        xFE = 0;
        SndCall(1, 0x16, &pos, 0, 0, 0);
        xFF++;
        if (xFF > 5) {
            ObjMgr.destroy(this);
        }
    }
}

void cObj01::dmgSet(int kind)
{
    switch ((u32) kind) {
    case 8:
        DmgMgr.set(2, 2, &pos, 3000.0f, 3000.0f);
        DmgMgr.set(8, 2, &pos, 15000.0f, 3000.0f);
        break;
    case 4:
        DmgMgr.set(4, 90, &pos, 1500.0f, 3000.0f);
        break;
    case 5:
        DmgMgr.set(5, 90, &pos, 1500.0f, 3000.0f);
        break;
    }
}

int obj01AddSpeed(cObj01* obj)
{
    Obj01Work* w = &obj->o1;
    f32 wh;
    Vec ref;
    Vec nrm;
    f32 len;

    w->spd.y -= w->grav;
    PSVECAdd(&obj->pos, &w->spd, &obj->pos);
    if (!(w->flags & 4)) {
        return 0;
    }
    if (GetWaterHeight(&obj->pos, &wh) && obj->pos.y <= wh) {
        obj->pos.y = wh;
        if (!(w->flags7C & 8)) {
            EstSet(0, -1, &obj->pos, 0, w->estNo2, (u8) w->estPrm2, 0, 0, 0, 0);
            w->flags7C |= 8;
            AddWaterPower(&obj->pos, 0.5f);
            if (obj->type != 1) {
                SndCall(6, 0x64, &obj->pos, 0, 0, 0);
            } else {
                SndCall(2, 4, &obj->pos, 0, 0, 0);
            }
        }
        w->life = 0;
        switch (w->type) {
        case 2:
        case 3:
            break;
        default:
            return 0;
        }
        return 1;
    }
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &obj->oldPos, &obj->pos, w->rad * 0.5f, 0x2001, 0);
    if (nrm.x == 0.0f && nrm.y == 0.0f && nrm.z == 0.0f) {
        return 0;
    }
    len = RootSumSquare3(&w->spd);
    C_VECReflect(&w->spd, &nrm, &ref);
    PSVECScale(&ref, &w->spd, len * 0.2f);
    PSVECScale(&w->rotSpd, &w->rotSpd, -0.8f);
    if (nrm.y > 0.9f) {
        switch (w->type) {
        case 0:
            break;
        case 1:
            if (w->spd.y > 50.0f) {
                if (w->seDone == 0) {
                    w->seDone = 3;
                    SndCall(5, 6, &obj->pos, 0, 0, 0);
                }
            }
            break;
        case 2:
            obj->dmgSet(4);
            EstSet(0, -1, &obj->pos, 0, w->estNo0, (u8) w->estPrm0, 0, 0, 0, 0);
            EstSet(0, -1, &obj->pos, 0, w->estNo1, (u8) w->estPrm1, 0, 0, 0, 0);
            if (w->type == 3) {
                SndCall(6, 0, &obj->pos, 0, 0, 0);
            } else {
                SndCall(1, 0x15, &obj->pos, 0, 0, 0);
                SndCall(1, 0x16, &obj->pos, 0, 0, 0);
            }
            obj->xFC = 1;
            obj->be_flag &= ~2;
            return 0;
        case 3:
            obj->dmgSet(4);
            EstSet(0, -1, &obj->pos, 0, w->estNo0, (u8) w->estPrm0, 0, 0, 0, 0);
            EstSet((int) obj, -1, 0, 0, w->estNo1, (u8) w->estPrm1, 0, 0, (u32) obj, 0);
            if (w->type == 3) {
                SndCall(6, 0, &obj->pos, 0, 0, 0);
            } else {
                SndCall(1, 0x15, &obj->pos, 0, 0, 0);
                SndCall(1, 0x16, &obj->pos, 0, 0, 0);
            }
            obj->xFC = 1;
            obj->be_flag &= ~2;
            return 0;
        case 4:
            if (w->seDone == 0) {
                w->seDone = 1;
                SndCall(5, 5, &obj->pos, 0, 0, 0);
            }
            break;
        }
    }
    return 0;
}

cObj* SetObj01(void* bin, void* tpl, Vec* pos, Vec* rot, Vec* spd, f32 grav, f32 rad, int life, int flags)
{
    cObj* obj;
    Obj01Work* w;

    obj = ObjMgr.create(1);
    if (obj == 0) {
        return 0;
    }
    if (obj->modelInit(bin, tpl) == 0) {
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 1000.0f, 1000.0f, 0.0f };

    obj->sub2B4.atari.throughOn();
    obj->lightInfo.init2(0, 1, &p0, &p1, 4);
    w = &obj->o1;
    obj->pos = *pos;
    obj->oldPos = *pos;
    obj->rot = *rot;
    w->spd = *spd;
    w->grav = grav;
    w->rad = rad;
    w->life = life;
    w->hold = 0;
    w->estNo0 = -1;
    w->estPrm0 = -1;
    w->estNo1 = -1;
    w->estPrm1 = -1;
    w->type = 0;
    w->holdTimer = 0;
    w->seDone = 0;
    if (flags & 1) {
        w->flags |= 4;
    }
    if (flags & 2) {
        w->flags |= 8;
        w->rotSpd.x = fRand0_1() * 0.19634955f + 0.09817477f;
        w->rotSpd.y = 0.0f;
        w->rotSpd.z = fRand0_1() * 0.09817477f + 0.09817477f;
        if (Rnd() & 1) {
            w->rotSpd.x = -w->rotSpd.x;
        }
        if (Rnd() & 1) {
            w->rotSpd.z = -w->rotSpd.z;
        }
    }
    if (flags & 4) {
        w->flags |= 8;
        w->rotSpd.x = -(fRand0_1() * 0.049087387f + 0.19634955f);
        w->rotSpd.y = 0.0f;
        w->rotSpd.z = 0.0f;
    }
    if (flags & 0x10) {
        w->flags |= 8;
        w->rotSpd.x = -0.2617994f;
        w->rotSpd.y = 0.0f;
        w->rotSpd.z = 0.0f;
    }
    return obj;
}

void Obj01SetEst(cObj* obj, int no0, int prm0, u32 type, int no1, int prm1, int no2, int prm2, int no3, int prm3)
{
    Obj01Work* w;

    if (obj == 0) {
        return;
    }
    w = &obj->o1;
    w->estNo0 = no0;
    w->estPrm0 = prm0;
    w->estNo1 = no1;
    w->estPrm1 = prm1;
    w->estPrm2 = prm2;
    w->estNo2 = no2;
    w->estNo3 = no3;
    w->estPrm3 = prm3;
    w->type = type;
}
