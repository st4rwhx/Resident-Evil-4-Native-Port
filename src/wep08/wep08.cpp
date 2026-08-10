// wep08 module: the Striker shotgun (cObjStriker, object id 0x2F; routines wep/pl_shotgun.cpp).

#include "wep_mod.h"
#include "light.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

void PlShotgunMove(cPlayer* pl);   // wep/pl_shotgun.cpp

class cObjStriker : public cObjWep {
public:
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);

    void setCartridge();
};

void ObjStriker_init(cObj* obj);

void Wep08_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(0x2F);

    if (obj == 0) {
        pLog->err(0, 0, "Wep08_init() cObjWep CREATE FAILED");
        return;
    }
    pl->pWep->pObj = obj;
    obj->init(pl);
    obj->setMotion(pl);
    EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x3C, 1);
    PlWepMot[0] = WEP_ARC_PTR(0x1A);
    PlWepMot[1] = WEP_ARC_PTR(0x20);
    PlWepMot[2] = WEP_ARC_PTR(0x22);
}

void ObjStriker_init(cObj* obj)
{
    new (obj) cObjStriker();
}

void cObjStriker::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0x5), WEP_ARC_PTR(0x6)) == 0) {
        pLog->err(0, 0, "cObjStriker::init() failed.");
        return;
    }
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        lightInfo.init2(1, 1, &p0, &p1, 1);
    }
    wep.parent = parent;
    U16Set(wep.x24, 0x2D);
    PSet(wep.pMotNormal, WEP_ARC_PTR(0x31));
    resetMotion();
    wep.x18 = 0x2E;
    wep.x19 = 0x2E;
    wep.x1A = 0x2E;
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

void cObjStriker::moveFire()
{
    if (wep.step == 0) {
        MotionSetCore(this, &this->mot, WEP_ARC_PTR(0x30), 0, 0, 0, 0);
        SndCall(2, 0, &pos, 0, 0, 0);
        SndCall(2, 4, &pos, 0, 0, 0);
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0, 1);
        pG->flags_500C |= 0x00800000;
        EstSet((int) this, -1, 0, 0, 0x3C, 0, 0, 0xA, 0, 0);
        wep.step = 1;
    } else if (MotionCheckCrossFrame(&mot, 21.0f)) {
        setCartridge();
    }
}

void cObjStriker::moveReload()
{
    if (wep.step == 0) {
        void* m;
        u16 se;

        switch (pG->x4FBA) {
        default:
            m = WEP_ARC_PTR(0x2B);
            break;
        case 1:
            m = WEP_ARC_PTR(0x2D);
            break;
        case 2:
            m = WEP_ARC_PTR(0x2F);
            break;
        }
        motionSet(m, 0, 0, 1, 0);
        switch (pG->x4FBA) {
        default:
            se = 2;
            break;
        case 1:
            se = 0x20;
            break;
        case 2:
            se = 0x21;
            break;
        }
        wep.seHandle = SndCall(2, se, &pParts->worldPos, 0, 0, 0);
        wep.step = 1;
    } else if (MotionCheckCrossFrame(&mot, 35.0f)) {
        ItemMgr.reload();
    }
}

void cObjStriker::setCartridge()
{
    cModel* parts = pPL->getPartsPtr(0xA);
    Vec pos;
    Vec rot;
    Vec spd;
    cObj* obj;

    pos.x = -163.31f;
    pos.y = -6.87f;
    pos.z = 83.13f;
    PSMTXMultVec(parts->mat, &pos, &pos);
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    spd.x = 3.0f;
    spd.y = 30.0f;
    spd.z = 20.0f;
    spd.x += fRand1_1() * 3.0f;
    spd.y += fRand1_1() * 5.0f;
    spd.z += fRand1_1() * 5.0f;
    PSMTXMultVecSR(parts->mat, &spd, &spd);
    obj = SetObj10(WEP_ARC_PTR(0x8), WEP_ARC_PTR(0x9), &pos, &rot, &spd, 10.0f, 50.0f, 0x28, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

void cObjStriker::setMotion(cPlayer* pl)
{
    PSet(pl->pMotTbl[0x00], WEP_ARC_PTR(0x0A));
    PSet(pl->pMotTbl[0x01], 0);
    PSet(pl->pMotTbl[0x02], WEP_ARC_PTR(0x10));
    PSet(pl->pMotTbl[0x03], WEP_ARC_PTR(0x11));
    PSet(pl->pMotTbl[0x06], WEP_ARC_PTR(0x14));
    PSet(pl->pMotTbl[0x07], WEP_ARC_PTR(0x15));
    PSet(pl->pMotTbl[0x08], WEP_ARC_PTR(0x12));
    PSet(pl->pMotTbl[0x09], WEP_ARC_PTR(0x13));
    PSet(pl->pMotTbl[0x0B], WEP_ARC_PTR(0x16));
    PSet(pl->pMotTbl[0x0C], WEP_ARC_PTR(0x17));
    PSet(pl->pMotTbl[0x0D], WEP_ARC_PTR(0x0C));
    PSet(pl->pMotTbl[0x0E], WEP_ARC_PTR(0x0D));
    PSet(pl->pMotTbl[0x0F], WEP_ARC_PTR(0x0E));
    PSet(pl->pMotTbl[0x10], WEP_ARC_PTR(0x0F));
    PSet(pl->pMotTbl[0x5B], WEP_ARC_PTR(0x33));
    PSet(pl->pMotTbl[0x57], WEP_ARC_PTR(0x34));
    PSet(pl->pMotTbl[0x39], WEP_ARC_PTR(0x26));
    PSet(pl->pMotTbl[0x3A], WEP_ARC_PTR(0x27));
    PSet(pl->pMotTbl[0x3D], PL_ARC_PTR(pG->pPlArc, 0x5D));
    PSet(pl->pMotTbl[0x41], WEP_ARC_PTR(0x28));
    PSet(pl->pMotTbl[0x42], WEP_ARC_PTR(0x29));
    PSet(pl->pMotTbl[0x3F], WEP_ARC_PTR(0x24));
    PSet(pl->pMotTbl[0x40], WEP_ARC_PTR(0x25));
    pl->pBody->initWepHand((u32) WEP_ARC_PTR(0x7));
    pl->setRightHand(1);
    pl->setLeftHand((u32) PL_ARC_PTR(pG->pPlArc, 0x19));
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep08_init;
    WeaponMoveFunc = PlShotgunMove;
    ObjInitFunc[0x2F] = ObjStriker_init;
    OSReport("Wep08 STRIKER prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x2F] = 0;
    OSReport("Wep08 STRIKER epilog Ok\n");
}

extern "C" void _unresolved()
{
}
