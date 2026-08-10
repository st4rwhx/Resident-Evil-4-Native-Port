// wep13 module: the rocket launcher. The weapon object is the DOL's cObjLauncher (id 0x23); the
// module supplies the player routines (wep/pl_rocket.cpp) and this entry object.

#include "wep_mod.h"
#include "light.h"
#include "esp.h"

void PlRocketMove(cPlayer* pl);   // wep/pl_rocket.cpp

void Wep13_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj;

    pl->flags_420 &= ~0x400;
    obj = (cObjWep*) ObjMgr.createBack(0x23);
    if (obj == 0) {
        pLog->err(0, 0, "Wep13_init() cObjWep CREATE FAILED");
    } else {
        obj->init(pl);
        pl->pWep->pObj = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x6), 0x47, 1);
        PlWepMot[0] = WEP_ARC_PTR(0xF);
        PlWepMot[1] = WEP_ARC_PTR(0x12);
        PlWepMot[2] = WEP_ARC_PTR(0x14);
    }
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep13_init;
    WeaponMoveFunc = PlRocketMove;
    OSReport("Wep13 ROCKET-RUNCHER prolog Ok\n");
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}
