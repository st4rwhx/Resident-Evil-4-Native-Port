// wep10 module entry: the semi-auto rifle (class in objHkSniper.cpp, routines wep/pl_rifle.cpp).

#include "wep_mod.h"
#include "light.h"
#include "esp.h"

void PlRifleMove(cPlayer* pl);   // wep/pl_rifle.cpp

void ObjHkSniper_init(cObj* obj);   // wep10/objHkSniper.cpp

void Wep10_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(0x30);

    if (obj == 0) {
        pLog->err(0, 0, "Wep10_init() cObjWep CREATE FAILED");
        pl->pWep->pObj = obj;
        return;
    }
    pl->pWep->pObj = obj;
    obj->init(pl);
    obj->setMotion(pl);
    EspDataLoad((u32) WEP_ARC_PTR(0x8), 0x44, 1);
    PlWepMot[0] = WEP_ARC_PTR(0xE);
    PlWepMot[1] = WEP_ARC_PTR(0xE);
    PlWepMot[2] = WEP_ARC_PTR(0xE);
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep10_init;
    WeaponMoveFunc = PlRifleMove;
    ObjInitFunc[0x30] = ObjHkSniper_init;
    OSReport("Wep10 HK-SNIPER prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x30] = 0;
}

extern "C" void _unresolved()
{
}
