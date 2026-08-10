// wep28 module: Krauser's bow (cObjBow) and its arrow (cObjAllow); routines in wep/pl_bow.cpp.

#include "wep_mod.h"
#include "light.h"
#include "esp.h"
#include "item.h"
#include "motion.h"
#include "snd.h"
#include "emmine.h"

void PlBowMove(cPlayer* pl);   // wep/pl_bow.cpp

void ObjKlauBow_init(cObj* obj);
void ObjKlauAllow_init(cObj* obj);

void Wep28_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(0x11);

    if (obj == 0) {
        pLog->err(0, 0, "Wep28_init() cObjWep CREATE FAILED");
        return;
    }
    pl->pWep->pObj = obj;
    obj->init(pl);
    obj->setMotion(pl);
    obj = (cObjWep*) ObjMgr.createBack(0x10);
    if (obj == 0) {
        pLog->err(0, 0, "Wep28_init() cObjWep CREATE FAILED");
        return;
    }
    pl->pWep->pObj2 = obj;
    obj->init(pl);
    obj->setDisp(1, 0);
    PSet(pl->pWep->pObj->bow.allow, obj);
    EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x50, 1);
}

void cObjBow::moveReady()
{
    if (wep.step == 0) {
        MotionSetCore(this, &this->mot, WEP_ARC_PTR(0x2E), 0, 0, 0, 0);
        if (bow.allow) {
            bow.allow->motionSet(PL_ARC_PTR(pG->pPlArc, 0x73), 0, 0, 1, 0);
        }
        wep.step = 1;
    }
    if (MotionCheckCrossFrame(&mot, 10.0f)) {
        SndCall(2, 0, &pParts->worldPos, 0, 0, 0);
    }
    if (MotionGetState(this)) {
        wep.mode = 0;
        wep.step = 0;
    }
}

void cObjBow::moveFire()
{
    if (wep.step == 0) {
        void* m;

        if (ItemMgr.bulletNum()) {
            m = WEP_ARC_PTR(0x2F);
        } else {
            m = WEP_ARC_PTR(0x2C);
        }
        MotionSetCore(this, &this->mot, m, 0, 0, 0, 0);
        setDispAllow(0);
        setAllow();
        SndCall(2, 1, &pParts->worldPos, 0, 0, 0);
        wep.step = 1;
    }
    if (MotionGetState(this)) {
        wep.mode = 0;
        wep.step = 0;
    }
}

void cObjBow::moveDown()
{
    if (wep.step == 0) {
        setDispAllow(0);
        resetMotion();
        wep.step = 1;
    }
    if (MotionGetState(this)) {
        wep.mode = 0;
        wep.step = 0;
    }
}

// Shared by both init()s: one error string and one pair of light-set constants in .rodata (a
// static inline's strings / statics are emitted when it is parsed, i.e. here, before init's pool).
static inline void wepInitErr()
{
    pLog->err(0, 0, "cObjWep::init() failed.");
}

static inline void wepLightInit(cObjWep* o)
{
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 500.0f, 0.0f, 0.0f };

    o->lightInfo.init2(1, 1, &p0, &p1, 1);
}

void cObjBow::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0x5), WEP_ARC_PTR(0x6)) == 0) {
        wepInitErr();
        return;
    }
    sub2B4.atari.init(1, 0, 0, 0.0f, 100.0f, 0.0f, 0.0f, 100.0f, 100.0f, 100.0f);
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0x10);
    wepLightInit(this);
    PSet(wep.parent, parent);
    PSet(wep.pMotNormal, WEP_ARC_PTR(0x2D));
    PSet(wep.pMotEmpty, WEP_ARC_PTR(0x2D));
    resetMotion();
    setDispAllow(0);
    bow.allow = 0;
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

void cObjBow::setDispAllow(int on)
{
    cModel* parts = getPartsPtr(4);
    f32 s;

    if (on == 1) {
        s = 1.0f;
    } else {
        s = 0.0f;
    }
    parts->scale.x = s;
    parts->scale.y = s;
    parts->scale.z = s;
}

void cObjBow::setMotion(cPlayer* pl)
{
    PSet(pl->pMotTbl[0x00], WEP_ARC_PTR(0x0A));
    PSet(pl->pMotTbl[0x02], WEP_ARC_PTR(0x0B));
    PSet(pl->pMotTbl[0x03], WEP_ARC_PTR(0x11));
    PSet(pl->pMotTbl[0x06], WEP_ARC_PTR(0x0D));
    PSet(pl->pMotTbl[0x07], WEP_ARC_PTR(0x13));
    PSet(pl->pMotTbl[0x08], WEP_ARC_PTR(0x0C));
    PSet(pl->pMotTbl[0x09], WEP_ARC_PTR(0x12));
    PSet(pl->pMotTbl[0x0B], WEP_ARC_PTR(0x0E));
    PSet(pl->pMotTbl[0x0C], WEP_ARC_PTR(0x14));
    PSet(pl->pMotTbl[0x0D], WEP_ARC_PTR(0x0F));
    PSet(pl->pMotTbl[0x0E], WEP_ARC_PTR(0x15));
    PSet(pl->pMotTbl[0x0F], WEP_ARC_PTR(0x10));
    PSet(pl->pMotTbl[0x10], WEP_ARC_PTR(0x16));
    PSet(pl->pMotTbl[0x39], WEP_ARC_PTR(0x19));
    PSet(pl->pMotTbl[0x3A], WEP_ARC_PTR(0x1A));
    PSet(pl->pMotTbl[0x3D], PL_ARC_PTR(pG->pPlArc, 0x5D));
    PSet(pl->pMotTbl[0x41], WEP_ARC_PTR(0x1B));
    PSet(pl->pMotTbl[0x42], WEP_ARC_PTR(0x1C));
    PSet(pl->pMotTbl[0x3F], WEP_ARC_PTR(0x17));
    PSet(pl->pMotTbl[0x40], WEP_ARC_PTR(0x18));
    pl->pBody->initWepHand((u32) WEP_ARC_PTR(0x7));
    pl->setRightHand(0);
    pl->setLeftHand(7);
}

void cObjBow::setAllow()
{
    static const Vec dir = { -1500.0f, 0.0f, 0.0f };
    cModel* parts = pPL->getPartsPtr(0xA);
    Vec spd;
    Vec hit;
    Vec nrm;

    PSMTXMultVecSR(parts->mat, &dir, &spd);
    if (EatMgr.hitCheck(&pPL->getPartsPtr(0)->worldPos, &parts->worldPos, &hit, &nrm, 0, 0)) {
        PSVECScale(&nrm, &nrm, 2000.0f);
        PSVECAdd(&hit, &nrm, &parts->worldPos);
        setPos(&parts->worldPos);
    }
    SetMine(PL_ARC_PTR(pG->pPlArc, 0x70), PL_ARC_PTR(pG->pPlArc, 0x71), &parts->worldPos, &spd, 2);
}

int cObjBow::keyKamae()
{
    if (pG->flags_5018 & 0x00800000) {
        return (Key.on >> 4) & 1;
    }
    if ((Key.on & 0x10) && ItemMgr.bulletNum()) {
        return 1;
    }
    return 0;
}

void cObjBow::interrupt()
{
    cObjWep::interrupt();
    setDispAllow(0);
    if (bow.allow) {
        bow.allow->setDisp(1, 0);
    }
    resetMotion();
    wep.mode = 0;
    wep.step = 0;
}

void ObjKlauBow_init(cObj* obj)
{
    new (obj) cObjBow();
}

void cObjAllow::moveFire()
{
}

void cObjAllow::init(cModel* parent)
{
    if (modelInit(PL_ARC_PTR(pG->pPlArc, 0x70), PL_ARC_PTR(pG->pPlArc, 0x71)) == 0) {
        wepInitErr();
        return;
    }
    sub2B4.atari.init(1, 0, 0, 0.0f, 100.0f, 0.0f, 0.0f, 100.0f, 100.0f, 100.0f);
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    wepLightInit(this);
    pos.x = -850.0f;
    pos.y = 5.0f;
    pos.z = 24.0f;
    rot.x = 0.0f;
    rot.y = -(PI / 2.0f);
    rot.z = 0.0f;
    parentSet(parent, 0xA, &pos, &rot);
}

void cObjAllow::setMotion(cPlayer* pl)
{
}

void ObjKlauAllow_init(cObj* obj)
{
    new (obj) cObjAllow();
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep28_init;
    WeaponMoveFunc = PlBowMove;
    ObjInitFunc[0x11] = ObjKlauBow_init;
    ObjInitFunc[0x10] = ObjKlauAllow_init;
    OSReport("Wep28 KLAUSER-BOW prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x11] = 0;
    ObjInitFunc[0x10] = 0;
}

extern "C" void _unresolved()
{
}
