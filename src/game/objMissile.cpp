#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "map_obj.h"
#include "widget.h"
#include "obj.h"
#include "esp.h"
#include "emhit.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "pl_wep.h"

// Helicopter missile: follows a parts of the helicopter (R0_Parent), waits (R0_FireWait), flies
// toward its target and explodes on the scenario / an enemy (R0_Fire, objMissileBomb).
class cObjMissile : public cObj {
public:
    virtual void move();
    virtual ~cObjMissile() {}

    void setParent(cModel* parent, int partsNo, int noNormalize);
    void setFire(Vec* target);
};

extern "C" {
int MotionMove(cModel* m, int a);
void objMissile_R0_Set(cObjMissile* obj);
void objMissile_R0_Parent(cObjMissile* obj);
void objMissile_R0_FireWait(cObjMissile* obj);
void objMissile_R0_Fire(cObjMissile* obj);
void objMissile_R0_Lost(cObjMissile* obj);
void objMissileBomb(cObjMissile* obj, Vec* pos);
}

void (*ObjMissile_R0_move_tbl[5])(cObjMissile*) = {
    objMissile_R0_Set, objMissile_R0_Parent, objMissile_R0_FireWait, objMissile_R0_Fire, objMissile_R0_Lost,
};

cObj* SetHeliMissile(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type)
{
    cObj* obj;
    MissileWork* w;

    obj = ObjMgr.create(0x38);
    if (obj == 0) {
        return 0;
    }
    w = &obj->missile;
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetLadder() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 5000.0f, 5000.0f, 5000.0f };

    obj->lightInfo.init2(0, 1, &p0, &p1, 2);
    AtariInit(&obj->sub2B4.atari, 0.0f, 1000.0f, -700.0f, 350.0f, 700.0f, 700.0f, 1000.0f, 0, 2, 0);
    obj->sub2B4.atari.throughOn();
    if (pos) {
        obj->pos = *pos;
    } else {
        obj->pos.x = 0.0f;
        obj->pos.y = 0.0f;
        obj->pos.z = 0.0f;
    }
    obj->oldPos = obj->pos;
    if (rot) {
        obj->rot = *rot;
    } else {
        obj->rot.x = 0.0f;
        obj->rot.y = 0.0f;
        obj->rot.z = 0.0f;
    }
    obj->type = type;
    w->hit = 0;
    if (obj->type == 1) {
        w->hit = SetEmHit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc), (void*) (pG->pArc->ofs_24 + (u32) pG->pArc), &obj->pos, &obj->rot, 1);
        if (w->hit) {
            w->hit->hp = 0;
            YarareInitCube(w->hit, 0.0f, -300.0f, -300.0f, 300.0f, 600.0f, 600.0f, 1, 5);
            w->hit->setParent(obj, 0, 0);
        }
    }
    obj->xFC = 0;
    obj->xFD = 0;
    obj->xFE = 0;
    obj->xFF = 0;
    return obj;
}

void cObjMissile::move()
{
    MissileWork* w = &missile;

    if (w->parent) {
        if ((w->parent->be_flag & 0x201) != 1 || ((cEm*) w->parent)->hp <= 0) {
            if (w->hit) {
                EmMgr.destroy(w->hit);
                w->hit = 0;
            }
            ObjMgr.destroy(this);
            return;
        }
    }
    ObjMissile_R0_move_tbl[xFC](this);
}

void objMissile_R0_Set(cObjMissile* obj)
{
    obj->matUpdate();
}

void objMissile_R0_Parent(cObjMissile* obj)
{
    MissileWork* w = &obj->missile;
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    cModel* parent = w->parent;

    RotMatrix(obj->mat, &obj->rot);
    TransMatrix(obj->mat, &obj->pos);
    ScaleMatrix(obj->mat, &obj->scale);
    if (parent && parent->pParts) {
        PSMTXConcat(parent->getPartsPtr(w->partsNo)->mat, obj->mat, m);
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
#line 249 "D:/Bio4/Prog/objMissile.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 251 "D:/Bio4/Prog/objMissile.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 253 "D:/Bio4/Prog/objMissile.cpp"
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
        PSMTXCopy(m, obj->mat);
    }
    if (obj->pMotion) {
        obj->x21C |= 0x40000000;
        MotionMove(obj, 0);
    } else {
        obj->partsMatCalc();
    }
    obj->partsWorldCalc();
}

void objMissile_R0_FireWait(cObjMissile* obj)
{
    MissileWork* w = &obj->missile;
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    cModel* parent = w->parent;

    switch (obj->xFE) {
    case 0:
        w->timer = 15;
        switch (obj->type) {
        case 0:
        default:
            EstSet((int) obj, -1, 0, 0, 0x32, 4, 0, 0, (u32) obj, 0);
            break;
        case 1:
            break;
        }
        obj->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            obj->xFC = 3;
            obj->xFD = 0;
            obj->xFE = 0;
            obj->xFF = 0;
        }
        break;
    }
    RotMatrix(obj->mat, &obj->rot);
    TransMatrix(obj->mat, &obj->pos);
    ScaleMatrix(obj->mat, &obj->scale);
    if (parent && parent->pParts) {
        PSMTXConcat(parent->getPartsPtr(w->partsNo)->mat, obj->mat, m);
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
#line 341 "D:/Bio4/Prog/objMissile.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 343 "D:/Bio4/Prog/objMissile.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 345 "D:/Bio4/Prog/objMissile.cpp"
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
        PSMTXCopy(m, obj->mat);
    }
    if (obj->pMotion) {
        obj->x21C |= 0x40000000;
        MotionMove(obj, 0);
    } else {
        obj->partsMatCalc();
    }
    obj->partsWorldCalc();
}

void objMissile_R0_Fire(cObjMissile* obj)
{
    MissileWork* w = &obj->missile;

    if (obj->xFE == 0) {
        Vec d;

        obj->pos.x = obj->mat[0][3];
        obj->pos.y = obj->mat[1][3];
        obj->pos.z = obj->mat[2][3];
        obj->oldPos = obj->pos;
        Matrix2AxisAngle(obj->mat, &obj->rot);
        if (w->hit) {
            w->hit->hp = 1;
        }
        if (w->hasTarget) {
            f32 len;

            PSVECSubtract(&w->target, &obj->pos, &d);
            len = SQRTF(d.x * d.x + d.z * d.z);
            obj->rot.x = -atan2f(d.y, len);
            obj->rot.y = atan2f(d.x, d.z);
            obj->rot.z = 0.0f;
            RotMatrix(obj->mat, &obj->rot);
            TransMatrix(obj->mat, &obj->pos);
        }
        w->timer = 90;
        w->hitWait = 3;
        switch (obj->type) {
        case 0:
        default:
            EstSet((int) obj, -1, 0, 0, 0x32, 5, 0, 0, (u32) obj, 0);
            SndCall(6, 2, &obj->pos, 0, 0, obj);
            w->spd.x = 0.0f;
            w->spd.y = 0.0f;
            w->spd.z = 300.0f;
            break;
        case 1:
            w->spd.x = 0.0f;
            w->spd.y = 0.0f;
            w->spd.z = 150.0f;
            break;
        }
        PSMTXMultVecSR(obj->mat, &w->spd, &w->spd);
        w->parent = 0;
        obj->xFE++;
    }
    Vec hit;
    Vec nrm;

    PSVECAdd(&obj->pos, &w->spd, &obj->pos);
    PSVECScale(&w->spd, &w->spd, 1.1f);
    if (w->hitWait) {
        w->hitWait--;
    } else {
        if (EatMgr.hitCheck(&obj->oldPos, &obj->pos, &hit, 0, 0, 0)) {
            PSVECSubtract(&obj->oldPos, &obj->pos, &nrm);
            if (nrm.x == 0.0f && nrm.y == 0.0f && nrm.z == 0.0f) {
                objMissileBomb(obj, &hit);
                return;
            }
#line 450 "D:/Bio4/Prog/objMissile.cpp"
            VECNormalize(&nrm, &nrm);
            PSVECScale(&nrm, &nrm, 300.0f);
            PSVECAdd(&hit, &nrm, &hit);
            objMissileBomb(obj, &hit);
            return;
        }
    }
    if (obj->type == 1) {
        Vec nrm2;

        if (EmAtkLineHitCk(&obj->oldPos, &obj->pos, &hit, &nrm2, 0)) {
            objMissileBomb(obj, &hit);
            return;
        }
        if (w->hit) {
            if (w->hit->ckDmgWeapon()) {
                objMissileBomb(obj, &obj->pos);
                return;
            }
        }
    }
    RotMatrix(obj->mat, &obj->rot);
    TransMatrix(obj->mat, &obj->pos);
    ScaleMatrix(obj->mat, &obj->scale);
    if (obj->pMotion) {
        obj->x21C |= 0x40000000;
        MotionMove(obj, 0);
    } else {
        obj->partsMatCalc();
    }
    obj->partsWorldCalc();
    if (w->timer) {
        w->timer--;
    } else {
        obj->xFC = 4;
        obj->xFD = 0;
        obj->xFE = 0;
        obj->xFF = 0;
    }
}

void objMissile_R0_Lost(cObjMissile* obj)
{
    MissileWork* w = &obj->missile;

    obj->be_flag &= ~2;
    if (w->hit) {
        EmMgr.destroy(w->hit);
        w->hit = 0;
    }
    ObjMgr.destroy(obj);
}

void cObjMissile::setParent(cModel* parent, int partsNo, int noNormalize)
{
    MissileWork* w = &missile;

    w->parent = parent;
    w->partsNo = partsNo;
    w->noNormalize = noNormalize;
    xFC = 1;
    xFD = 0;
    xFE = 0;
    xFF = 0;
}

void cObjMissile::setFire(Vec* target)
{
    MissileWork* w = &missile;

    w->hasTarget = 0;
    if (target) {
        w->target = *target;
        w->hasTarget = 1;
    }
    xFC = 2;
    xFD = 0;
    xFE = 0;
    xFF = 0;
}

void objMissileBomb(cObjMissile* obj, Vec* pos)
{
    MissileWork* w = &obj->missile;

    switch (obj->type) {
    case 0:
    default:
        EstSet(0, -1, pos, 0, 0x32, 7, 0, 0, 0, 0);
        SndCall(6, 3, &obj->pos, 0, 0, obj);
        PlWepHitCheck2(0, &obj->oldPos, &obj->oldPos, 0x12, 3, 8000.0f);
        break;
    case 1:
        EstSet(0, -1, pos, 0, 2, 5, 0, 0, 0, 0);
        PlWepHitCheck2(0, &obj->oldPos, &obj->oldPos, 0x13, 3, 2000.0f);
        break;
    }
    if (w->hit) {
        EmMgr.destroy(w->hit);
        w->hit = 0;
    }
    if (G_ROOM_ID == 0x320) {
        pG->flags_174 |= 0x80000000;
    }
    obj->xFC = 4;
    obj->xFD = 0;
    obj->xFE = 0;
    obj->xFF = 0;
}
