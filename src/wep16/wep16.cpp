// wep16 module: Leon's knife (cObjKnife; routines wep/pl_knife.cpp = the DOL's game/pl_knife.cpp).

#include "wep_mod.h"
#include "light.h"
#include "esp.h"
#include "motion.h"

void PlKnifeMove(cPlayer* pl);   // wep/pl_knife.cpp

void ObjKnife_init(cObj* obj);

// pPL read as a struct member: the load stays below the collision-flag store before it.
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

void Wep16_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjKnife* obj = (cObjKnife*) ObjMgr.createBack(0x24);

    if (obj == 0) {
        pLog->err(0, 0, "Wep16_init() cObjWep CREATE FAILED");
        return;
    }
    obj->init();
    pl->pWep->pObj = obj;
    obj->setMotion(pl);
    EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x4A, 1);
    PlWepMot[0] = WEP_ARC_PTR(0x2A);
    PlWepMot[1] = WEP_ARC_PTR(0x2C);
    PlWepMot[2] = WEP_ARC_PTR(0x2E);
}

void cObjKnife::init()
{
    if (modelInit(WEP_ARC_PTR(0x6), WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjKnife::init() failed.");
        return;
    }
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = pPLS->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        lightInfo.init2(1, 1, &p0, &p1, 1);
    }
    wep.parent = pPL;
    resetMotion();
    MotionSetCore(this, &this->mot, PL_ARC_PTR(pG->pPlArc, 0x1B), 0, 0, 0, 0);
}

void ObjKnife_init(cObj* obj)
{
    new (obj) cObjKnife();
}

void cObjKnife::setMotion(cPlayer* pl)
{
    PSet(pl->pMotTbl[0x00], WEP_ARC_PTR(0x08));
    PSet(pl->pMotTbl[0x01], 0);
    PSet(pl->pMotTbl[0x02], WEP_ARC_PTR(0x09));
    PSet(pl->pMotTbl[0x03], WEP_ARC_PTR(0x1B));
    PSet(pl->pMotTbl[0x04], WEP_ARC_PTR(0x09));
    PSet(pl->pMotTbl[0x05], WEP_ARC_PTR(0x09));
    PSet(pl->pMotTbl[0x06], WEP_ARC_PTR(0x0C));
    PSet(pl->pMotTbl[0x07], WEP_ARC_PTR(0x1D));
    PSet(pl->pMotTbl[0x08], WEP_ARC_PTR(0x0A));
    PSet(pl->pMotTbl[0x09], WEP_ARC_PTR(0x1C));
    PSet(pl->pMotTbl[0x0A], WEP_ARC_PTR(0x0A));
    PSet(pl->pMotTbl[0x0B], WEP_ARC_PTR(0x0D));
    PSet(pl->pMotTbl[0x0C], WEP_ARC_PTR(0x1E));
    PSet(pl->pMotTbl[0x0D], WEP_ARC_PTR(0x0E));
    PSet(pl->pMotTbl[0x0E], WEP_ARC_PTR(0x1F));
    PSet(pl->pMotTbl[0x0F], WEP_ARC_PTR(0x0F));
    PSet(pl->pMotTbl[0x10], WEP_ARC_PTR(0x20));
    PSet(pl->pMotTbl[0x12], WEP_ARC_PTR(0x0C));
    PSet(pl->pMotTbl[0x13], WEP_ARC_PTR(0x0C));
    PSet(pl->pMotTbl[0x3D], PL_ARC_PTR(pG->pPlArc, 0x5D));
    pl->pBody->initWepHand((u32) WEP_ARC_PTR(0x7));
    pl->setRightHand(1);
    pl->setLeftHand(0);
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep16_init;
    WeaponMoveFunc = PlKnifeMove;
    ObjInitFunc[0x24] = ObjKnife_init;
    OSReport("Wep16 KNIFE prolog Ok\n");
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}
