// wep43 module: Wesker's handgun (the Punisher class object wep/objRuger.cpp, object id 0x21).
// Creates the weapon object, loads the muzzle effects and the player's weapon motions, and registers
// the handgun player routines (wep/pl_handgun.cpp).

#include "wep_mod.h"
#include "light.h"
#include "esp.h"

void PlHandgunMove(cPlayer* pl);   // wep/pl_handgun.cpp
void ObjRuger_init(cObj* obj);     // wep/objRuger.cpp

void Wep43_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj;

    obj = (cObjWep*) ObjMgr.createBack(0x21);
    if (obj == 0) {
        pLog->err(0, 0, "Wep43_init() cObjWep CREATE FAILED");
    } else {
        pl->pWep->pObj = obj;
        obj->init(pl);
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x36, 1);
        PlWepMot[0] = WEP_ARC_PTR(0x26);
        PlWepMot[1] = WEP_ARC_PTR(0x27);
        PlWepMot[2] = WEP_ARC_PTR(0x28);
    }
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep43_init;
    WeaponMoveFunc = PlHandgunMove;
    ObjInitFunc[0x21] = ObjRuger_init;
    OSReport("Wep43 WESKER - HANDGUN prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x21] = 0;
}

extern "C" void _unresolved()
{
}
