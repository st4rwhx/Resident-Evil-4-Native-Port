// wep01 module: the FN57 (Blacktail) handgun (class object objFn57.cpp, object id 0x32).
// Creates the weapon object, loads the muzzle effects and the player's weapon motions, and registers
// the handgun player routines (wep/pl_handgun.cpp).

#include "wep_mod.h"
#include "light.h"
#include "esp.h"

void PlHandgunMove(cPlayer* pl);   // wep/pl_handgun.cpp
void ObjFn57_init(cObj* obj);      // objFn57.cpp

void Wep01_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj;

    obj = (cObjWep*) ObjMgr.createBack(0x32);
    if (obj == 0) {
        pLog->err(0, 0, "Wep01_init() cObjFn57 CREATE FAILED");
    } else {
        pl->pWep->pObj = obj;
        obj->init(pl);
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x35, 1);
        PlWepMot[0] = WEP_ARC_PTR(0x26);
        PlWepMot[1] = WEP_ARC_PTR(0x27);
        PlWepMot[2] = WEP_ARC_PTR(0x28);
    }
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep01_init;
    WeaponMoveFunc = PlHandgunMove;
    ObjInitFunc[0x32] = ObjFn57_init;
    OSReport("Wep01 FN57 prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x32] = 0;
}

extern "C" void _unresolved()
{
}
