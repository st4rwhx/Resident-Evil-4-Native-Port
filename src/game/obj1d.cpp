#include "atari.h"
#include "obj.h"
#include "global.h"
#include "math_sub.h"

// Chain link: a model hung between two parts of a parent (the interpolated orientation and
// position of the two parts), with an optional pendulum cloth. Fades out when the parent is lost.
class cObjChain : public cObj {
public:
    virtual void move();
    virtual ~cObjChain() {}

    void setParent(cModel* parent, int parts, Vec* ofs, int flag);
    void setParent2(cModel* parent, int parts1, Vec* ofs1, int parts2, Vec* ofs2, int flag);
    void setChain(PenCloth* cloth);
    void chainMove();
};

extern "C" {
int MotionMove(cModel* m, int a);
void obj1d_R1_Set(cObjChain* obj);
void obj1d_R1_LostWait(cObjChain* obj);
void obj1d_R1_Lost(cObjChain* obj);
void obj1d_R1_Parent(cObjChain* obj);
}

static void (*Obj1d_R1_move_tbl[4])(cObjChain*) = { obj1d_R1_Set, obj1d_R1_LostWait, obj1d_R1_Lost,
                                                     obj1d_R1_Parent };

cObj* SetChain(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    ChainWork* w;

    obj = ObjMgr.createBack(0x1D);
    if (obj == 0) {
        return 0;
    }
    w = &obj->chain;
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetChain() modelInit() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 10000.0f, 10000.0f, 100000.0f };

    obj->sub2B4.atari.throughOn();
    obj->lightInfo.init2(0, 1, &p0, &p1, 2);
    obj->pos = *pos;
    obj->oldPos = *pos;
    obj->rot = *rot;
    w->parent = 0;
    w->parts1 = 0;
    w->parts2 = 0;
    w->cloth = 0;
    obj->xFC = 1;
    obj->xFD = 0;
    obj->xFE = 0;
    obj->xFF = 0;
    Obj1d_R1_move_tbl[obj->xFD]((cObjChain*) obj);
    return obj;
}

void cObjChain::move()
{
    ChainWork* w = &chain;

    if (w->parent) {
        if ((w->parent->be_flag & 0x201) != 1) {
            ObjMgr.destroy(this);
            return;
        }
    }
    if (pMotion) {
        MotionMove(this, 0);
    }
    Obj1d_R1_move_tbl[xFD](this);
    if ((be_flag & 0x201) == 1) {
        chainMove();
    }
}

void obj1d_R1_Set(cObjChain* obj)
{
    if (obj->pMotion) {
        MotionMove(obj, 0);
    } else {
        RotMatrix(obj->mat, &obj->rot);
        TransMatrix(obj->mat, &obj->pos);
        ScaleMatrix(obj->mat, &obj->scale);
        obj->partsMatCalc();
    }
    obj->partsWorldCalc();
}

void obj1d_R1_LostWait(cObjChain* obj)
{
    ChainWork* w = &obj->chain;

    switch (obj->xFE) {
    case 0:
        w->timer = 90;
        obj->xFE++;
    case 1:
        if (w->timer == 0) {
            obj->alpha -= 0.1f;
            if (obj->alpha <= 0.0f) {
                obj->alpha = 0.0f;
                obj->xFC = 1;
                obj->xFD = 2;
                obj->xFE = 0;
                obj->xFF = 0;
                break;
            }
        } else {
            w->timer--;
        }
        {
            Vec scr;
            Vec p;

            p = obj->pos;
            GetScreenPos(&p, &scr);
            if (scr.z > 1.0f) {
                obj->xFC = 1;
                obj->xFD = 2;
                obj->xFE = 0;
                obj->xFF = 0;
            }
        }
        break;
    }
    RotMatrix(obj->mat, &obj->rot);
    TransMatrix(obj->mat, &obj->pos);
    ScaleMatrix(obj->mat, &obj->scale);
    obj->partsMatCalc();
    obj->partsWorldCalc();
}

void obj1d_R1_Lost(cObjChain* obj)
{
    if (obj->xFE == 0) {
        obj->xFE++;
        obj->be_flag &= ~2;
        obj->be_flag &= ~0x20;
        ObjMgr.destroy(obj);
    }
}

void obj1d_R1_Parent(cObjChain* obj)
{
    ChainWork* w = &obj->chain;
    Mtx ma;
    Mtx mb;
    Vec v0;
    Vec v1;
    Vec v2;
    Quaternion qa;
    Quaternion qb;
    Quaternion q;
    Vec pa;
    Vec pb;
    Vec p;
    cModel* parent = w->parent;
    cModel* partsA;
    cModel* partsB;

    RotMatrix(obj->mat, &obj->rot);
    TransMatrix(obj->mat, &obj->pos);
    ScaleMatrix(obj->mat, &obj->scale);
    if (parent && parent->pParts) {
        partsA = parent->getPartsPtr(w->parts1);
        PSMTXConcat(partsA->mat, obj->mat, ma);
        partsB = parent->getPartsPtr(w->parts2);
        PSMTXConcat(partsB->mat, obj->mat, mb);
        if (!(w->flags & 2)) {
            v0.x = ma[0][0];
            v0.y = ma[1][0];
            v0.z = ma[2][0];
            v1.x = ma[0][1];
            v1.y = ma[1][1];
            v1.z = ma[2][1];
            v2.x = ma[0][2];
            v2.y = ma[1][2];
            v2.z = ma[2][2];
            if (v0.x == 0.0f && v0.y == 0.0f && v0.z == 0.0f) {
                v0.x = 1.0f;
            }
#line 290 "D:/Bio4/Prog/obj1d.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 292 "D:/Bio4/Prog/obj1d.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 294 "D:/Bio4/Prog/obj1d.cpp"
            VECNormalize(&v2, &v2);
            ma[0][0] = v0.x;
            ma[1][0] = v0.y;
            ma[2][0] = v0.z;
            ma[0][1] = v1.x;
            ma[1][1] = v1.y;
            ma[2][1] = v1.z;
            ma[0][2] = v2.x;
            ma[1][2] = v2.y;
            ma[2][2] = v2.z;
            v0.x = mb[0][0];
            v0.y = mb[1][0];
            v0.z = mb[2][0];
            v1.x = mb[0][1];
            v1.y = mb[1][1];
            v1.z = mb[2][1];
            v2.x = mb[0][2];
            v2.y = mb[1][2];
            v2.z = mb[2][2];
            if (v0.x == 0.0f && v0.y == 0.0f && v0.z == 0.0f) {
                v0.x = 1.0f;
            }
#line 315 "D:/Bio4/Prog/obj1d.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 317 "D:/Bio4/Prog/obj1d.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 319 "D:/Bio4/Prog/obj1d.cpp"
            VECNormalize(&v2, &v2);
            mb[0][0] = v0.x;
            mb[1][0] = v0.y;
            mb[2][0] = v0.z;
            mb[0][1] = v1.x;
            mb[1][1] = v1.y;
            mb[2][1] = v1.z;
            mb[0][2] = v2.x;
            mb[1][2] = v2.y;
            mb[2][2] = v2.z;
        }
        C_QUATMtx(&qa, ma);
        C_QUATMtx(&qb, mb);
        C_QUATSlerp(&qa, &qb, &q, 0.5f);
        PSMTXQuat(obj->mat, &q);
        PSMTXMultVec(partsA->mat, &w->ofs1, &pa);
        PSMTXMultVec(partsB->mat, &w->ofs2, &pb);
        PosToPos(&pa, &pb, &p, 0.5f);
        TransMatrix(obj->mat, &p);
    }
    if (obj->pMotion) {
        obj->x21C |= 0x40000000;
        MotionMove(obj, 0);
    } else {
        obj->partsMatCalc();
    }
    obj->partsWorldCalc();
}

void cObjChain::setParent(cModel* parent, int parts, Vec* ofs, int flag)
{
    ChainWork* w = &chain;

    w->parent = parent;
    w->parts1 = parts;
    w->parts2 = parts;
    w->ofs1 = *ofs;
    w->ofs2 = *ofs;
    if (flag) {
        w->flags |= 2;
    } else {
        w->flags &= ~2;
    }
    xFC = 1;
    xFD = 3;
    xFE = 0;
    xFF = 0;
}

void cObjChain::setParent2(cModel* parent, int parts1, Vec* ofs1, int parts2, Vec* ofs2, int flag)
{
    ChainWork* w = &chain;

    w->parent = parent;
    w->parts1 = parts1;
    w->parts2 = parts2;
    w->ofs1 = *ofs1;
    w->ofs2 = *ofs2;
    if (flag) {
        w->flags |= 2;
    } else {
        w->flags &= ~2;
    }
    xFC = 1;
    xFD = 3;
    xFE = 0;
    xFF = 0;
}

void cObjChain::setChain(PenCloth* cloth)
{
    chain.cloth = cloth;
    if (cloth) {
        PenClothSet(this, cloth, 100.0f);
    }
}

void cObjChain::chainMove()
{
    if (chain.cloth) {
        PenClothMove(this, chain.cloth);
        be_flag &= ~0x00E00000;
    }
}
