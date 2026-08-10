// wep09 module entry: the bolt-action rifle (class in objSniper.cpp, routines wep/pl_rifle.cpp).

#include "wep_mod.h"
#include "light.h"
#include "esp.h"

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

void PlRifleMove(cPlayer* pl);   // wep/pl_rifle.cpp
cObjWep* equipWeapon(cPlayer* pl);
void ObjSniper_init(cObj* obj);  // wep09/objSniper.cpp

void Wep09_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep02_init() wep model init failed.");
    } else {
        pl->pWep->pObj = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x8), 0x3D, 1);
        PlWepMot[0] = WEP_ARC_PTR(0x15);
        PlWepMot[1] = WEP_ARC_PTR(0x15);
        PlWepMot[2] = WEP_ARC_PTR(0x15);
    }
}

cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(0x28);

    if (obj == 0) {
        pLog->err(0, 0, "Wep09_init() cObjWep CREATE FAILED");
        pl->pWep->pObj = obj;
        return 0;
    }
    obj->init(pl);
    return obj;
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep09_init;
    WeaponMoveFunc = PlRifleMove;
    ObjInitFunc[0x28] = ObjSniper_init;
    OSReport("Wep09 SNIPER prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x28] = 0;
}

extern "C" void _unresolved()
{
}
