// wep12 module: the Thompson. Weapon class wep12/objTompson.cpp, routines wep/pl_machine.cpp.

#include "wep_mod.h"
#include "light.h"
#include "esp.h"

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

void PlMachineMove(cPlayer* pl);   // wep/pl_machine.cpp
cObjWep* equipWeapon(cPlayer* pl);

void Wep12_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep11_init() wep model init failed.");
    } else {
        pl->pWep->pObj = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x46, 1);
        PlWepMot[0] = WEP_ARC_PTR(0x1B);
        PlWepMot[1] = WEP_ARC_PTR(0x1F);
        PlWepMot[2] = WEP_ARC_PTR(0x21);
    }
}

cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(0x25);

    if (obj == 0) {
        pLog->err(0, 0, "Wep12_init() cObjWep CREATE FAILED");
        return 0;
    }
    obj->init(pl);
    return obj;
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep12_init;
    WeaponMoveFunc = PlMachineMove;
    ObjInitFunc[0x25] = ObjTompson_init;
    OSReport("Wep12 tompson prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x25] = 0;
}

extern "C" void _unresolved()
{
}
