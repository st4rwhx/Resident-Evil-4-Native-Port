// wep05 module entry: the civilian handgun (class in objCivilian.cpp, routines wep/pl_handgun.cpp).

#include "wep_mod.h"
#include "light.h"
#include "esp.h"

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

void PlHandgunMove(cPlayer* pl);   // wep/pl_handgun.cpp
void ObjCivilian_init(cObj* obj);  // wep05/objCivilian.cpp

void Wep05_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(0x2E);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep05_init() wep model init failed.");
        return;
    }
    pl->pWep->pObj = obj;
    obj->init(pl);
    obj->setMotion(pl);
    EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x39, 1);
    PlWepMot[0] = WEP_ARC_PTR(0x26);
    PlWepMot[1] = WEP_ARC_PTR(0x27);
    PlWepMot[2] = WEP_ARC_PTR(0x28);
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep05_init;
    WeaponMoveFunc = PlHandgunMove;
    ObjInitFunc[0x2E] = ObjCivilian_init;
    OSReport("Wep05 CIVILIAN prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x2E] = 0;
}

extern "C" void _unresolved()
{
}
