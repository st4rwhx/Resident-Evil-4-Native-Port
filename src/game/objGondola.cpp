#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "map_obj.h"
#include "widget.h"
#include "obj.h"
#include "em.h"
#include "global.h"
#include "math_sub.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "pad.h"
#include "quake.h"
#include "player.h"
#include "pl_npc.h"

// Cable car (gondola): carries the player, the partner and up to five enemies along its motion,
// with five collision quads following the car; the break routine hands the camera over.
class cObjGondola : public cObj {
public:
    virtual void move();
    virtual ~cObjGondola() {}

    void setMoveMotion(void* mot, int frame);
    int ckRide();
    void setRideEm(cEm* em);
    void setGetOffEm(cEm* em);
    void setDamage();
    void setBreak();
    void setRidePL();
    void setGetOffPL();
    void setSubMotion(MotionWork* work, void* mot, void* breakMot);
    void setVib();
};

// motion.h declares MotionMove with one argument; the object units call it with two. Only the
// two blend fields of MotionWork are touched here.
struct GondolaMotWork {
    u8 pad_0[0x44];
    u32 flags2;           // 0x44
    u8 pad_48[0xC8 - 0x48];
    f32 blendRate;        // 0xC8
};

// Struct-member view of pSUB (the pLog trick): the load stays below the preceding work store
// (setRidePL: `w->rideSUB = 0; if (pSUB)`).
struct SubCharPtr {
    cSubChar* p;
};
#define pSUBS (((SubCharPtr*) &pSUB)->p)

extern "C" {
int MotionMove(cModel* m, int a);
void DiedemoExec(int no, int a);
void objGondola_R0_Set(cObjGondola* obj);
void objGondola_R0_Move(cObjGondola* obj);
void objGondola_R0_Down(cObjGondola* obj);
void objGondola_R0_Up(cObjGondola* obj);
void objGondola_R0_Break(cObjGondola* obj);
void objGondolaSatClear(cObjGondola* obj);
void objGondolaSatSet(cObjGondola* obj);
void objGondolaRideEmAdjust(cObjGondola* obj, Vec* d);
}
void MotionSetCore(cModel* m, void* work, void* mot, int a, int b, int c, int d);

static void (*ObjGondola_R0_move_tbl[5])(cObjGondola*) = {
    objGondola_R0_Set, objGondola_R0_Move, objGondola_R0_Down, objGondola_R0_Up, objGondola_R0_Break,
};

cObj* SetGondola(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    GondolaWork* w;
    int i;
    cEm** p;

    obj = ObjMgr.create(0x35);
    if (obj == 0) {
        return 0;
    }
    w = &obj->gondola;
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetLadder() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 5000.0f, 5000.0f, 5000.0f };

    obj->lightInfo.init2(0, 1, &p0, &p1, 0x10);
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
    for (i = 0; i < 5; i++) {
        w->sat[i] = 0;
        w->sat2[i] = 0;
    }
    p = w->rideEm;
    for (i = 0; i < 5; i++) {
        *p++ = 0;
    }
    w->ridePL = 0;
    w->rideSUB = 0;
    w->subWork = 0;
    w->subMot = 0;
    w->breakMot = 0;
    obj->xFC = 0;
    obj->xFD = 0;
    obj->xFE = 0;
    obj->xFF = 0;
    objGondolaSatSet((cObjGondola*) obj);
    return obj;
}

void cObjGondola::move()
{
    GondolaWork* w = &gondola;

    objGondolaSatClear(this);
    if (w->cnt) {
        w->cnt--;
    }
    ObjGondola_R0_move_tbl[xFC](this);
}

void objGondola_R0_Set(cObjGondola* obj)
{
    obj->matUpdate();
    obj->partsWorldCalc();
}

void objGondola_R0_Move(cObjGondola* obj)
{
    Vec b;
    Vec a;
    Vec d;
    cModel* parts;

    obj->gondola.ridePL = 0;
    parts = obj->getPartsPtr(1);
    a.x = 0.0f;
    a.y = -4828.03f;
    a.z = 0.0f;
    PSMTXMultVec(parts->mat, &a, &a);
    MotionMove(obj, 0);
    obj->partsWorldCalc();
    parts = obj->getPartsPtr(1);
    b.x = 0.0f;
    b.y = -4828.03f;
    b.z = 0.0f;
    PSMTXMultVec(parts->mat, &b, &b);
    PSVECSubtract(&b, &a, &d);
    objGondolaRideEmAdjust(obj, &d);
}

void objGondola_R0_Down(cObjGondola* obj)
{
    GondolaWork* w = &obj->gondola;
    Vec b;
    Vec a;
    Vec d;
    cModel* parts;

    w->ridePL = 1;
    parts = obj->getPartsPtr(1);
    a.x = 0.0f;
    a.y = -4828.03f;
    a.z = 0.0f;
    PSMTXMultVec(parts->mat, &a, &a);
    MotionMove(obj, 0);
    obj->partsWorldCalc();
    parts = obj->getPartsPtr(1);
    b.x = 0.0f;
    b.y = -4828.03f;
    b.z = 0.0f;
    PSMTXMultVec(parts->mat, &b, &b);
    PSVECSubtract(&b, &a, &d);
    PSVECAdd(&pPL->pos, &d, &b);
    pPL->setPos(&b);
    memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->quake_ofs), &d, sizeof(Vec));
    if (pSUB && w->rideSUB) {
        PSVECAdd(&pSUB->pos, &d, &b);
        pSUB->setPos(&b);
    }
    objGondolaSatSet(obj);
}

void objGondola_R0_Up(cObjGondola* obj)
{
    GondolaWork* w = &obj->gondola;
    Vec b;
    Vec a;
    Vec d;
    cModel* parts;

    w->ridePL = 1;
    parts = obj->getPartsPtr(1);
    a.x = 0.0f;
    a.y = -4828.03f;
    a.z = 0.0f;
    PSMTXMultVec(parts->mat, &a, &a);
    MotionMove(obj, 0);
    obj->partsWorldCalc();
    parts = obj->getPartsPtr(1);
    b.x = 0.0f;
    b.y = -4828.03f;
    b.z = 0.0f;
    PSMTXMultVec(parts->mat, &b, &b);
    PSVECSubtract(&b, &a, &d);
    PSVECAdd(&pPL->pos, &d, &b);
    pPL->setPos(&b);
    memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->quake_ofs), &d, sizeof(Vec));
    if (pSUB && w->rideSUB) {
        PSVECAdd(&pSUB->pos, &d, &b);
        pSUB->setPos(&b);
    }
    objGondolaSatSet(obj);
    if (obj->motFrame >= 4105.0f && obj->motFrame <= 4105.0f) {
        b.x = 21722.0f;
        b.y = 10274.0f;
        b.z = -35327.0f;
        FSet(pPL->rot.y, -0.49f);
        pPL->setPos(&b);
        if (pSUB && w->rideSUB) {
            b.x = 22577.0f;
            b.y = 10274.0f;
            b.z = -35864.0f;
            FSet(pSUB->rot.y, -0.49f);
            pSUB->setPos(&b);
        }
        pG->flags_500C &= ~0x20;
        w->ridePL = 0;
        obj->xFC = 1;
        obj->xFD = 0;
        obj->xFE = 0;
        obj->xFF = 0;
    }
}

void objGondola_R0_Break(cObjGondola* obj)
{
    GondolaWork* w = &obj->gondola;
    Vec b;
    Vec a;
    Vec v;
    cModel* parts;
    f32 len;
    Vec* cp;
    Vec* ca;
    static Camera ObjGondolaCam;

    switch (obj->xFE) {
    case 0:
        w->x14.x = 0.0f;
        w->x14.y = 0.0f;
        w->x14.z = 0.0f;
        if (w->ridePL) {
            pG->pl_life = 0;
            DiedemoExec(0x3C, 0);
        }
        w->timer = 42;
        obj->xFE++;
    case 1:
        parts = obj->getPartsPtr(0);
        v.x = -2000.0f;
        v.y = 0.0f;
        v.z = -1115.0f;
        PSMTXMultVec(parts->mat, &v, &v);
        ObjGondolaCam.param.pos = v;
        parts = obj->getPartsPtr(1);
        v.x = 0.0f;
        v.y = -2757.0f;
        v.z = 0.0f;
        PSMTXMultVec(parts->mat, &v, &v);
        ObjGondolaCam.param.at = v;
        cp = &ObjGondolaCam.param.pos;
        ca = &ObjGondolaCam.param.at;
        ObjGondolaCam.up.x = 0.0f;
        ObjGondolaCam.up.z = 0.0f;
        ObjGondolaCam.param.fovy = 50.0f;
        ObjGondolaCam.up.y = 1.0f;
        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        ObjGondolaCam.dist = SQRTF(len);
        CameraSetOrientationUp(&ObjGondolaCam);
        CamCtrl.x250 = (s32) &ObjGondolaCam;
        if (w->timer) {
            w->timer--;
            if (w->timer == 0) {
                if (w->subWork && w->breakMot) {
                    ((GondolaMotWork*) w->subWork)->flags2 |= 0x10000000;
                    MotionSetCore(obj, w->subWork, w->breakMot, 0, 0, 0, 0);
                    ((GondolaMotWork*) w->subWork)->flags2 &= ~0x10000000;
                    obj->motBlend = w->subWork;
                    ((GondolaMotWork*) obj->motBlend)->blendRate = 1.0f;
                    ((GondolaMotWork*) obj->motBlend)->flags2 |= 0x80000000;
                }
            }
        }
        break;
    }
    parts = obj->getPartsPtr(1);
    a.x = 0.0f;
    a.y = -4828.03f;
    a.z = 0.0f;
    PSMTXMultVec(parts->mat, &a, &a);
    MotionMove(obj, 0);
    obj->partsWorldCalc();
    parts = obj->getPartsPtr(1);
    b.x = 0.0f;
    b.y = -4828.03f;
    b.z = 0.0f;
    PSMTXMultVec(parts->mat, &b, &b);
    PSVECSubtract(&b, &a, &v);
    if (w->ridePL) {
        PSVECAdd(&pPL->pos, &v, &b);
        pPL->setPos(&b);
        memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->quake_ofs), &v, sizeof(Vec));
        if (pSUB && w->rideSUB) {
            PSVECAdd(&pSUB->pos, &v, &b);
            pSUB->setPos(&b);
        }
        objGondolaSatSet(obj);
    }
}

void objGondolaSatClear(cObjGondola* obj)
{
    GondolaWork* w = &obj->gondola;
    int i;

    for (i = 0; i < 5; i++) {
        if (w->sat[i]) {
            w->sat[i]->flags &= ~4;
        }
        if (w->sat2[i]) {
            w->sat2[i]->flags &= ~4;
        }
    }
}

void objGondolaSatSet(cObjGondola* obj)
{
    GondolaWork* w = &obj->gondola;
    Vec pos;
    Vec rot;
    Vec v;
    Vec poly[4];
    cModel* parts;
    u32 i;
    f32 hw;
    f32 hd;
    f32 cx;
    f32 cz;
    f32 h;
    f32 r;

    parts = obj->getPartsPtr(0);
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1.0f;
    PSMTXMultVecSR(parts->mat, &v, &v);
    rot.x = 0.0f;
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    pos = parts->worldPos;
    pos.y -= 4828.03f;
    for (i = 0; i < 1; i++) {
        switch (i) {
        case 0:
        default:
            h = 0.0f;
            hw = 1000.0f;
            r = 3000.0f;
            cx = 0.0f;
            hd = 1500.0f;
            cz = 0.0f;
            break;
        case 1:
            hw = 1000.0f;
            r = 1100.0f;
            hd = 100.0f;
            cx = 0.0f;
            h = -100.0f;
            cz = 1500.0f;
            break;
        case 2:
            hw = 1000.0f;
            cz = -1500.0f;
            r = 1100.0f;
            hd = 100.0f;
            cx = 0.0f;
            h = -100.0f;
            break;
        case 3:
            hw = 100.0f;
            r = 1100.0f;
            hd = 1500.0f;
            cx = 1000.0f;
            h = -100.0f;
            cz = 0.0f;
            break;
        case 4:
            hw = 100.0f;
            cx = -1000.0f;
            r = 1100.0f;
            hd = 1500.0f;
            h = -100.0f;
            cz = 0.0f;
            break;
        }
        poly[0].x = -hw + cx;
        poly[0].y = h;
        poly[0].z = -hd + cz;
        poly[1].x = -hw + cx;
        poly[1].y = h;
        poly[1].z = hd + cz;
        poly[2].x = hw + cx;
        poly[2].y = h;
        poly[2].z = hd + cz;
        poly[3].x = hw + cx;
        poly[3].y = h;
        poly[3].z = -hd + cz;
        if (w->sat[i]) {
            w->sat[i]->flags |= 4;
            w->sat[i]->setCoord(&pos, &rot);
        } else {
            w->sat[i] = SatMgr.create(&pos, &rot, poly, 0, 0x100, r);
        }
    }
}

// Never called (dead-stripped by the original linker, STRIP_UNUSED): only their constant pools
// survive in .rodata (3000^2, pi, pi/3, 4250, 4343, 1850, 1950, 4828.03, 5000^2 / 4828.03, 5000^2).
static int objGondolaRideAreaCk(cObjGondola* obj, cEm* em)
{
    Vec d;
    f32 ang;

    PSVECSubtract(&em->pos, &obj->pos, &d);
    if (d.x * d.x + d.z * d.z > 9000000.0f) {
        return 0;
    }
    ang = atan2f(d.x, d.z) - obj->rot.y;
    if (ang > 3.1415927f) {
        return 0;
    }
    if (ang < 1.0471976f) {
        return 0;
    }
    if (d.x > 4250.0f) {
        return 0;
    }
    if (d.x < 4343.0f) {
        return 0;
    }
    if (d.z > 1850.0f) {
        return 0;
    }
    if (d.z < 1950.0f) {
        return 0;
    }
    if (d.y > 4828.03f) {
        return 0;
    }
    if (d.x * d.x + d.y * d.y + d.z * d.z > 25000000.0f) {
        return 0;
    }
    return 1;
}

static int objGondolaRideDistCk(cObjGondola* obj, cEm* em)
{
    Vec d;

    PSVECSubtract(&em->pos, &obj->pos, &d);
    d.y -= 4828.03f;
    if (d.x * d.x + d.y * d.y + d.z * d.z > 25000000.0f) {
        return 0;
    }
    return 1;
}

void cObjGondola::setMoveMotion(void* mot, int frame)
{
    MotionSetCore(this, &pMotion, mot, 0, 0, 0x8005, frame);
    xFC = 1;
    xFD = 0;
    xFE = 0;
    xFF = 0;
}

int cObjGondola::ckRide()
{
    if (gondola.ridePL) {
        return 1;
    }
    return 0;
}

void cObjGondola::setRideEm(cEm* em)
{
    GondolaWork* w = &gondola;
    cModel* parts = getPartsPtr(0);
    Vec v;
    u32 i;

    for (i = 0; i < 5; i++) {
        if (w->rideEm[i] == 0) {
            w->rideEm[i] = em;
            v.x = 0.0f;
            v.y = -4828.03f;
            v.z = (f32) i * 300.0f + -1000.0f;
            if (i & 1) {
                v.x = -300.0f;
            }
            em->atari.throughOn();
            PSMTXMultVec(parts->mat, &v, &v);
            em->setPos(&v);
            break;
        }
    }
}

void cObjGondola::setGetOffEm(cEm* em)
{
    GondolaWork* w = &gondola;
    int i;

    for (i = 0; i < 5; i++) {
        if (w->rideEm[i] == em) {
            w->rideEm[i] = 0;
        }
    }
}

void cObjGondola::setDamage()
{
    QuakeExec(0, 0, 5, 22.0f, 2);
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
}

void cObjGondola::setBreak()
{
    QuakeExec(0, 0, 10, 30.0f, 2);
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
    xFC = 4;
    xFD = 0;
    xFE = 0;
    xFF = 0;
}

void objGondolaRideEmAdjust(cObjGondola* obj, Vec* d)
{
    GondolaWork* w = &obj->gondola;
    cModel* parts = obj->getPartsPtr(1);
    Vec c;
    u32 i;

    c.x = 0.0f;
    c.y = -4828.03f;
    c.z = 0.0f;
    PSMTXMultVec(parts->mat, &c, &c);
    for (i = 0; i < 5; i++) {
        if (w->rideEm[i]) {
            PSVECAdd(&w->rideEm[i]->pos, d, &w->rideEm[i]->pos);
            w->rideEm[i]->setPos(&w->rideEm[i]->pos);
            if ((c.x - w->rideEm[i]->pos.x) * (c.x - w->rideEm[i]->pos.x) +
                (c.y - w->rideEm[i]->pos.y) * (c.y - w->rideEm[i]->pos.y) +
                (c.z - w->rideEm[i]->pos.z) * (c.z - w->rideEm[i]->pos.z) > 16000000.0f) {
                w->rideEm[i] = 0;
            }
        }
    }
}

void cObjGondola::setRidePL()
{
    GondolaWork* w = &gondola;
    Vec v;

    MotionMove(this, 0);
    partsWorldCalc();
    v = getPartsPtr(0)->worldPos;
    v.y -= 4828.03f;
    FSet(pPL->rot.y, 2.84f);
    pPL->setPos(&v);
    w->rideSUB = 0;
    if (pSUBS) {
        Vec v2;

        v2.x = -500.0f;
        v2.y = 0.0f;
        v2.z = -800.0f;
        PSMTXMultVec(pPL->mat, &v2, &v2);
        FSet(pSUB->rot.y, 2.84f);
        pSUB->setPos(&v2);
        w->rideSUB = 1;
    }
    pG->flags_500C |= 0x20;
    xFC = 2;
    xFD = 0;
    xFE = 0;
    xFF = 0;
}

void cObjGondola::setGetOffPL()
{
    pG->flags_500C &= ~0x20;
    xFC = 1;
    xFD = 0;
    xFE = 0;
    xFF = 0;
}

void cObjGondola::setSubMotion(MotionWork* work, void* mot, void* breakMot)
{
    GondolaWork* w = &gondola;

    w->subWork = work;
    w->subMot = mot;
    w->breakMot = breakMot;
}

void cObjGondola::setVib()
{
    GondolaWork* w = &gondola;

    if (w->subWork && w->subMot) {
        ((GondolaMotWork*) w->subWork)->flags2 |= 0x10000000;
        MotionSetCore(this, w->subWork, w->subMot, 0, 0, 0, 0);
        ((GondolaMotWork*) w->subWork)->flags2 &= ~0x10000000;
        motBlend = w->subWork;
        ((GondolaMotWork*) motBlend)->blendRate = 1.0f;
        ((GondolaMotWork*) motBlend)->flags2 |= 0x80000000;
        QuakeExec(0, 0, 10, 30.0f, 2);
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
    }
}
