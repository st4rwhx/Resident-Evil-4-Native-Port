#include "atari.h"
#include "event.h"
#include "obj.h"
#include "global.h"
#include "math_sub.h"
#include "pl_cloth.h"

// Event costume / cloth model: follows a parts of its parent with a slerp blend and runs the
// cloth simulation selected by `type` (player costumes, enemy cloth sets, the ribbon / rope).
class cObj18 : public cObj {
public:
    virtual void move();
    virtual ~cObj18() {}
};

extern "C" {
int MotionMove(cModel* m, int a);
void* memset(void* p, int c, unsigned int n);
void obj18SetOya(cObj18* obj);
void Em34ClothSet1(cModel* m, PlCloth* c);
void Em34ClothSet2(cModel* m, PlCloth* c);
void Em34ClothMove1(cModel* m, PlCloth* c);
void Em34ClothMove2(cModel* m, PlCloth* c);
void Em34ClothReset(cModel* m);
void Em37HairSet(cModel* m, PlCloth* c);
void Em37CoatSet(cModel* m, PlCloth* c);
void Em37HairMove(cModel* m, PlCloth* c);
void Em37CoatMove(cModel* m, PlCloth* c);
void Em37ClothReset(cModel* m);
void Em30ClothSet1(cModel* m, PlCloth* c);
void Em30ClothSet2(cModel* m, PlCloth* c);
void Em30ClothMove1(cModel* m, PlCloth* c);
void Em30ClothMove2(cModel* m, PlCloth* c);
void Em30ClothReset(cModel* m);
void Em33ClothSet(cModel* m, PlCloth* c, int a);
void Em33ClothSet2(cModel* m, PlCloth* c, int a);
void Em33ClothMove(cModel* m, PlCloth* c);
void Em33ClothMove2(cModel* m, PlCloth* c);
void Em33ClothReset(cModel* m);
cObj* Em2bShortRopeSet(cModel* m, PlCloth* c, void* bin, void* tpl);
}

// Never called: the original keeps the message of this unused inline in .rodata.
static inline void obj18FreeSizeErr(int size)
{
    pLog->err(0, 0, "SetObj18 freeSize failed : %d", size);
}

PlCloth Obj18Cloth1;
PlCloth Obj18Cloth2;
static PlCloth Obj18Cloth3;
PlCloth Obj18Cloth4;
static PlCloth Obj18Cloth5;
PlCloth Obj18Cloth6;
static PlCloth Evt_leonHair;
PlCloth Evt_leonJacket;
PlCloth Evt_leonHolster;
PlCloth Evt_girlHair;
static PlCloth Evt_girlSkirt;
PlCloth Evt_girlSweater;
PlCloth Evt_adaRibbon;
PlCloth Evt_adaDress;
static PlCloth Evt_adaHair;
static PlCloth Evt_luisHair;

cObj* SetObj18(void* bin, void* tpl, Vec* pos, Vec* rot, int type)
{
    cObj* obj;
    Obj18Work* w;
    int lightFlag;
    Vec sz;
    Vec ofs;
    void* cbin;
    void* ctpl;
    cModelInfo* info;
    ModelBound* b;

    obj = ObjMgr.createBack(0x18);
    if (obj == 0) {
        return 0;
    }
    w = &obj->o18;
    memset(w, 0, sizeof(Obj18Work));
    if (obj->modelInit(bin, tpl) == 0) {
        ObjMgr.destroy(obj);
        return 0;
    }
    obj->sub2B4.atari.throughOn();
    lightFlag = 4;
    if (type == 1) {
        lightFlag = 0x40;
        if (pG->x4FB8 == 0) {
            lightFlag = 1;
        }
    }
    if (type == 2) {
        lightFlag = 0x40;
        if (pG->x4FB8 == 1) {
            lightFlag = 1;
        }
    }
    if (type == 3) {
        lightFlag = 0x40;
        if (pG->x4FB8 == 2) {
            lightFlag = 1;
        }
    }
    if (type == 0x17) {
        lightFlag = 0x40;
        if (pG->x4FB8 == 2) {
            lightFlag = 1;
        }
    }
    if (type == 4) {
        lightFlag = 0x40;
    }
    if (type == 5) {
        lightFlag = 0x40;
    }
    if (type == 6) {
        lightFlag = 2;
    }
    if (type == 7) {
        lightFlag = 2;
    }
    if (type == 8) {
        lightFlag = 2;
    }
    if (type == 0x12) {
        lightFlag = 2;
    }
    if (type == 0xB) {
        lightFlag = 2;
    }
    if (type == 0x13) {
        lightFlag = 0x20;
    }
    if (type == 0x14) {
        lightFlag = 4;
    }
    if (type == 0x15) {
        lightFlag = 0x20;
    }
    if (type == 0x16) {
        lightFlag = 4;
    }
    if (type == 9) {
        lightFlag = 2;
    }
    if (type == 0x18) {
        lightFlag = 2;
    }
    if (type == 0xA) {
        lightFlag = 2;
    }
    if (type == 0xC) {
        lightFlag = 2;
    }
    if (type == 0xD) {
        lightFlag = 2;
    }
    if (type == 0) {
        lightFlag = 4;
    }
    if (type == 0xE) {
        lightFlag = 4;
    }
    if (type == 0xF) {
        lightFlag = 0x10;
    }
    if (type == 0x10) {
        lightFlag = 1;
    }
    if (type == 0x11) {
        lightFlag = 8;
    }
    w->type = type;
    info = obj->pInfo;
    b = &info->bound;
    sz.x = b->size.x;
    sz.y = b->size.y;
    sz.z = b->size.z;
    PSVECSubtract(&info->bound.center, &obj->pParts->pos, &ofs);
    obj->lightInfo.init2(2, 1, &ofs, &sz, lightFlag);
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
    w->rate = 1.0f;
    w->rateSpd = 0.0f;
    w->oya = 0;
    w->partsNo = 0;
    w->x74 = 0;
    switch (w->type) {
    case 1:
        if (pG->x4FB8 == 0) {
            PlClothSetLeon(obj, &Evt_leonHair, &Evt_leonJacket, &Evt_leonHolster);
        }
        break;
    case 2:
        PlClothSetGirl(obj, &Evt_girlHair, &Evt_girlSkirt, &Evt_girlSweater, 1);
        break;
    case 3:
        PlClothSetAda(obj, &Evt_adaRibbon, &Evt_adaDress, &Evt_adaHair, 1);
        if (pG->costume2 == 0) {
            if (EvtMgr.GetBin(&cbin, "em/pl02/pl020f.bin", 0)) {
                if (EvtMgr.GetBin(&ctpl, "em/pl02/pl020a.tpl", 0)) {
                    w->child = (cObj*) AdaRibbonSet(obj, &Evt_adaRibbon, cbin, ctpl);
                    if (w->child) {
                        w->child->setNoSuspend(1);
                        w->child->lightInfo.x50 = obj->lightInfo.x50;
                    }
                }
            }
        }
        break;
    case 4:
        PlClothSetLuis(obj, &Evt_luisHair);
        break;
    case 7:
        break;
    case 8:
        Em34ClothSet2(obj, &Obj18Cloth2);
        Em34ClothSet1(obj, &Obj18Cloth1);
        break;
    case 9:
        Em37HairSet(obj, &Obj18Cloth1);
        Em37CoatSet(obj, &Obj18Cloth2);
        break;
    case 0xA:
        Em30ClothSet1(obj, &Obj18Cloth1);
        Em30ClothSet2(obj, &Obj18Cloth2);
        break;
    case 0x13:
        Em33ClothSet(obj, &Obj18Cloth3, 0);
        Em33ClothSet2(obj, &Obj18Cloth4, 0);
        break;
    case 0x15:
        Em33ClothSet(obj, &Obj18Cloth3, 1);
        Em33ClothSet2(obj, &Obj18Cloth4, 1);
        break;
    case 0x14:
        Em33ClothSet(obj, &Obj18Cloth5, 0);
        Em33ClothSet2(obj, &Obj18Cloth6, 0);
        break;
    case 0x16:
        Em33ClothSet(obj, &Obj18Cloth5, 1);
        Em33ClothSet2(obj, &Obj18Cloth6, 1);
        break;
    case 0xB:
        if (EvtMgr.GetBin(&cbin, "obj/objmodel/obm0700.bin", 0) == 0) {
            pLog->err(0, 0, "Event::ExePacket_Mot : dat failed");
            return 0;
        }
        if (EvtMgr.GetBin(&ctpl, "obj/objmodel/obm0700.tpl", 0) == 0) {
            pLog->err(0, 0, "Event::ExePacket_Mot : dat failed");
            return 0;
        }
        w->child = Em2bShortRopeSet(obj, &Obj18Cloth1, cbin, ctpl);
        if (w->child) {
            w->child->setNoSuspend(1);
        }
        break;
    }
    return obj;
}

int DelObj18(cObj* obj)
{
    if (obj == 0) {
        pLog->err(0, 0, "Evt_SetElgiganteRope : pointer failed");
        return 0;
    }
    if (obj->o18.child) {
        ObjMgr.destroy(obj->o18.child);
    }
    return 1;
}

void cObj18::move()
{
    Obj18Work* w = &o18;

    if (w->debugFlag) {
        pLog->mes(0, 0, "cObj18:move DebugFlag");
    }
    if (pMotion) {
        MotionMove(this, 0);
        partsWorldCalc();
    } else {
        RotMatrix(worldMat, &rot);
        TransMatrix(worldMat, &pos);
        ScaleMatrix(worldMat, &scale);
        PSMTXCopy(worldMat, mat);
    }
    if (w->oya) {
        if ((w->oya->be_flag & 0x201) != 1) {
            w->oya = 0;
        }
    }
    obj18SetOya(this);
    if (x2B0 == 0) {
        partsMatCalc();
        partsWorldCalc();
    }
    if (pG->costume2 == 1) {
        if (w->type == 2) {
            w->flags &= ~0x40;
        }
    }
    if (!(w->flags & 0x40)) {
        switch (w->type) {
        case 1:
            if (pG->x4FB8 == 0) {
                PlClothMoveLeon(this, &Evt_leonHair, &Evt_leonJacket, &Evt_leonHolster);
            }
            break;
        case 2:
            PlClothMoveGirl(this, &Evt_girlHair, &Evt_girlSkirt, &Evt_girlSweater);
            break;
        case 3:
            PlClothMoveAda(this, &Evt_adaRibbon, &Evt_adaDress, &Evt_adaHair);
            break;
        case 4:
            PlClothMoveLuis(this, &Evt_luisHair);
            break;
        case 7:
            break;
        case 8:
            Em34ClothMove1(this, &Obj18Cloth1);
            Em34ClothMove2(this, &Obj18Cloth2);
            Em34ClothReset(this);
            break;
        case 9:
            Em37HairMove(this, &Obj18Cloth1);
            Em37CoatMove(this, &Obj18Cloth2);
            Em37ClothReset(this);
            break;
        case 0xA:
            Em30ClothMove1(this, &Obj18Cloth1);
            Em30ClothMove2(this, &Obj18Cloth2);
            Em30ClothReset(this);
            break;
        case 0x13:
            Em33ClothMove(this, &Obj18Cloth3);
            Em33ClothMove2(this, &Obj18Cloth4);
            Em33ClothReset(this);
            break;
        case 0x15:
            Em33ClothMove(this, &Obj18Cloth3);
            Em33ClothMove2(this, &Obj18Cloth4);
            Em33ClothReset(this);
            break;
        case 0x14:
            Em33ClothMove(this, &Obj18Cloth5);
            Em33ClothMove2(this, &Obj18Cloth6);
            Em33ClothReset(this);
            break;
        case 0x16:
            Em33ClothMove(this, &Obj18Cloth5);
            Em33ClothMove2(this, &Obj18Cloth6);
            Em33ClothReset(this);
            break;
        case 0xB:
            break;
        }
    }
}

void OyaSetObj18(cObj* obj, cModel* oya, int partsNo)
{
    Obj18Work* w;

    if (obj == 0) {
        return;
    }
    if (obj->x12E != 1) {
        return;
    }
    if (obj->id != 0x18) {
        return;
    }
    w = &obj->o18;
    w->oya = oya;
    w->partsNo = partsNo;
    w->flags &= ~8;
    w->flags &= ~3;
}

int obj18GetOya(cModel** out, cObj* obj)
{
    *out = 0;
    if (obj->o18.oya == 0) {
        return 0;
    }
    if (obj->o18.oya->pParts == 0) {
        return 0;
    }
    *out = obj->o18.oya;
    return 1;
}

void obj18SetOya(cObj18* obj)
{
    Obj18Work* w = &obj->o18;
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    Vec p;
    Quaternion q0;
    Quaternion q1;
    Quaternion q;

    if (w->oya == 0) {
        return;
    }
    if (w->oya->pParts == 0) {
        return;
    }
    PSMTXConcat(w->oya->getPartsPtr(w->partsNo)->mat, obj->mat, m);
    v0.x = m[0][0];
    v0.y = m[1][0];
    v0.z = m[2][0];
    v1.x = m[0][1];
    v1.y = m[1][1];
    v1.z = m[2][1];
    v2.x = m[0][2];
    v2.y = m[1][2];
    v2.z = m[2][2];
#line 976 "D:/Bio4/Prog/obj18.cpp"
    VECNormalize(&v0, &v0);
    VECNormalize(&v1, &v1);
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
    if (w->rate < 1.0f) {
        w->rate += w->rateSpd;
        if (w->rate >= 1.0f) {
            w->rate = 1.0f;
            w->flags &= ~8;
        }
    }
    if (w->flags & 8) {
        f32 rate = w->rate;
        f32 inv = 1.0f - rate;

        p.x = m[0][3] * rate + w->mat[0][3] * inv;
        p.y = m[1][3] * rate + w->mat[1][3] * inv;
        p.z = m[2][3] * rate + w->mat[2][3] * inv;
        C_QUATMtx(&q0, m);
        C_QUATMtx(&q1, w->mat);
        C_QUATSlerp(&q0, &q1, &q, w->rate);
        PSMTXQuat(obj->mat, &q);
        TransMatrix(obj->mat, &p);
        PSMTXCopy(obj->mat, w->mat);
    } else {
        PSMTXCopy(m, obj->mat);
    }
    if (w->oya) {
        if (w->oya->lightInfo.x50 & 2) {
            obj->lightInfo.x50 &= ~0x10;
            obj->lightInfo.x50 |= 2;
        }
    }
}

void Obj18CmfSet(cObj* obj, u32 cmf)
{
    if (obj == 0) {
        return;
    }
    if (obj->x12E != 1) {
        return;
    }
    if (obj->id != 0x18) {
        return;
    }
    obj->o18.cmf = cmf;
}

u32 Obj18CmfGet(cObj* obj)
{
    if (obj == 0) {
        return 0;
    }
    if (obj->x12E != 1 || obj->id != 0x18) {
        return 0;
    }
    return obj->o18.cmf;
}

void Obj18CmfOn(cObj* obj, u32 no)
{
    u32 cmf[1];
    u32* p;

    cmf[0] = Obj18CmfGet(obj);
    p = cmf;
    p[no >> 5] |= 0x80000000 >> (no & 31);
    Obj18CmfSet(obj, cmf[0]);
}
