// wep34 module: a hand "weapon" (cObjHand, no weapon model). Creates the object, gives the
// player the hand model and fills the player's motion table from the weapon archive.

#include "wep_mod.h"

cObjWep* equipWeapon(cPlayer* pl);

// Motion table stores go through wep_mod.h's PSet: the original reloads pG after every one.

static void Wep34_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (obj == 0) {
        pLog->err(0, 0, "Wep13_init() failed.");
    } else {
        pl->pWep->pObj = obj;
        obj->setMotion(pl);
    }
}

void Wep34_move(cPlayer* pl)
{
}

cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(0x3d);

    if (obj == 0) {
        pLog->err(0, 0, "Wep34_init() cObjWep CREATE FAILED");
        return 0;
    }
    obj->init(pl);
    pl->pBody->initWepHand((u32) PL_ARC_PTR(pG->pPlArc, 0x11));
    pl->setRightHand(1);
    pl->setLeftHand(0);
    return obj;
}

void ObjHand_init(cObj* obj)
{
    new (obj) cObjHand();
}

void cObjHand::setMotion(cPlayer* pl)
{
    PSet(pl->pMotTbl[0x00], WEP_ARC_PTR(0x04));
    PSet(pl->pMotTbl[0x02], WEP_ARC_PTR(0x05));
    PSet(pl->pMotTbl[0x03], WEP_ARC_PTR(0x0B));
    PSet(pl->pMotTbl[0x08], WEP_ARC_PTR(0x06));
    PSet(pl->pMotTbl[0x09], WEP_ARC_PTR(0x0C));
    PSet(pl->pMotTbl[0x06], WEP_ARC_PTR(0x07));
    PSet(pl->pMotTbl[0x07], WEP_ARC_PTR(0x0D));
    PSet(pl->pMotTbl[0x0B], WEP_ARC_PTR(0x08));
    PSet(pl->pMotTbl[0x0C], WEP_ARC_PTR(0x0E));
    PSet(pl->pMotTbl[0x0D], WEP_ARC_PTR(0x09));
    PSet(pl->pMotTbl[0x0E], WEP_ARC_PTR(0x0F));
    PSet(pl->pMotTbl[0x0F], WEP_ARC_PTR(0x0A));
    PSet(pl->pMotTbl[0x10], WEP_ARC_PTR(0x10));
    PSet(pl->pMotTbl[0x3D], PL_ARC_PTR(pG->pPlArc, 0x5D));
    PSet(pl->pMotTbl[0x3F], WEP_ARC_PTR(0x11));
    PSet(pl->pMotTbl[0x40], WEP_ARC_PTR(0x12));
    PSet(pl->pMotTbl[0x39], WEP_ARC_PTR(0x13));
    PSet(pl->pMotTbl[0x3A], WEP_ARC_PTR(0x14));
    PSet(pl->pMotTbl[0x41], WEP_ARC_PTR(0x15));
    PSet(pl->pMotTbl[0x42], WEP_ARC_PTR(0x16));
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep34_init;
    WeaponMoveFunc = Wep34_move;
    ObjInitFunc[0x3d] = ObjHand_init;
    OSReport("Wep34 HAND prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x3d] = 0;
}

extern "C" void _unresolved()
{
}
