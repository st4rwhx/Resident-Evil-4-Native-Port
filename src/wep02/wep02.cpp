// wep02 module: the Red9 (cObjMauser, object id 0x27) and Punisher (cObjRuger, 0x21) handguns.
// Creates the weapon object of the equipped type, loads the muzzle effects and the player's
// weapon motions, and registers the handgun player routines (wep/pl_handgun.cpp).

#include "wep_mod.h"
#include "light.h"
#include "esp.h"

void PlHandgunMove(cPlayer* pl);   // wep/pl_handgun.cpp
void ObjMauser_init(cObj* obj);    // objMauser.cpp
void ObjRuger_init(cObj* obj);     // objRuger.cpp

void Wep02_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj;
    int id;

    switch (pG->wep_no) {
    case 2:
    default:
        id = 0x21;
        break;
    case 3:
        id = 0x27;
        break;
    }
    obj = (cObjWep*) ObjMgr.createBack(id);
    if (obj == 0) {
        pLog->err(0, 0, "Wep02_init() cObjWep CREATE FAILED");
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
    WeaponInitFunc = Wep02_init;
    WeaponMoveFunc = PlHandgunMove;
    ObjInitFunc[0x21] = ObjRuger_init;
    ObjInitFunc[0x27] = ObjMauser_init;
    OSReport("Wep02 HANDGUN prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x21] = 0;
    ObjInitFunc[0x27] = 0;
}

extern "C" void _unresolved()
{
}
