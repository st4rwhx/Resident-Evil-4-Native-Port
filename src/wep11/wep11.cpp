// wep11 module: the TMP machine gun. Weapon class wep/objMachinegun.cpp, routines wep/pl_machine.cpp.

#include "wep_mod.h"
#include "light.h"
#include "esp.h"

void PlMachineMove(cPlayer* pl);   // wep/pl_machine.cpp
void ObjMachinegun_init(cObj* obj);   // wep/objMachinegun.cpp

void Wep11_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(0x2D);

    if (obj == 0) {
        pLog->err(0, 0, "Wep11_init() cObjWep CREATE FAILED");
    } else {
        pl->pWep->pObj = obj;
        obj->init(pl);
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x45, 1);
        PlWepMot[0] = WEP_ARC_PTR(0x1B);
        PlWepMot[1] = WEP_ARC_PTR(0x1F);
        PlWepMot[2] = WEP_ARC_PTR(0x21);
    }
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep11_init;
    WeaponMoveFunc = PlMachineMove;
    ObjInitFunc[0x2D] = ObjMachinegun_init;
    OSReport("Wep11 MACHINEGUN prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x2D] = 0;
}

extern "C" void _unresolved()
{
}
