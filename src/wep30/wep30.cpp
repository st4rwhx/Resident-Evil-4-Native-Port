// wep30 module: Ada's hand grenade (the wep19 cObjHandGre build for one grenade kind; object id
// 0x3C). The module object carries the class, the entry points and the grenade routine registration
// (wep/pl_grenade.cpp).

#include "wep_mod.h"
#include "light.h"
#include "item.h"
#include "esp.h"
#include "pad.h"

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

void PlGrenadeMove(cPlayer* pl);   // wep/pl_grenade.cpp
cObjWep* equipWeapon(cPlayer* pl);

class cObjHandGre : public cObjWep {
public:
    virtual ~cObjHandGre() {}
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);
    virtual int keyKamae();
};

void Wep30_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep30_init() wep model init failed.");
    } else {
        pl->pWep->pObj = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x4D, 1);
    }
}

// The two grenade objects: the hand one (parts 0x11) is pObj, the belt one (parts 0xA) pObj2.
cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj;
    Vec pos;
    Vec rot;

    pl->pWep->pObj = 0;
    pl->pWep->pObj2 = 0;
    obj = (cObjWep*) ObjMgr.createBack(0x3C);
    if (obj == 0) {
        goto fail;
    }
    obj->init(pl);
    pl->pWep->pObj = obj;
    pos.x = -170.0f;
    pos.y = -60.0f;
    pos.z = -40.0f;
    rot.x = -0.13439035f;
    rot.y = 2.4958208f;
    rot.z = -0.2268928f;
    obj->parentSet(pl, 0x11, &pos, &rot);
    if (pG->wep_no == 0x19 || pG->wep_no == 0x1F || pG->wep_no == 0x20) {
        obj->pParts->scale.x = 0.5f;
        obj->pParts->scale.y = 0.5f;
        obj->pParts->scale.z = 0.5f;
    }
    if (ItemMgr.bulletNum() <= 1) {
        obj->setDisp(0, 0);
    }
    obj = (cObjWep*) ObjMgr.createBack(0x3C);
    if (obj == 0) {
    fail:
        pLog->err(0, 0, "Wep30_init() cObjWep CREATE FAILED");
        return 0;
    }
    obj->init(pl);
    pos.x = -70.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = PI / 2.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    obj->parentSet(pl, 0xA, &pos, &rot);
    if (pG->wep_no == 0x19 || pG->wep_no == 0x1F || pG->wep_no == 0x20) {
        obj->pParts->scale.x = 0.5f;
        obj->pParts->scale.y = 0.5f;
        obj->pParts->scale.z = 0.5f;
    }
    pl->pWep->pObj2 = obj;
    if (ItemMgr.bulletNum() == 0) {
        obj->setDisp(0, 0);
    }
    return pl->pWep->pObj;
}

void ObjHandGre_init(cObj* obj)
{
    new (obj) cObjHandGre();
}

void cObjHandGre::init(cModel* parent)
{
    if (modelInit(PL_ARC_PTR(pG->pPlArc, 0x6A), PL_ARC_PTR(pG->pPlArc, 0x6B)) == 0) {
        pLog->err(0, 0, "cObjHandGre::init() failed.");
    }
}

void cObjHandGre::setMotion(cPlayer* pl)
{
    u16 num;
    void* hand;

    PSet(pl->pMotTbl[0x00], WEP_ARC_PTR(0x08));
    PSet(pl->pMotTbl[0x02], WEP_ARC_PTR(0x09));
    PSet(pl->pMotTbl[0x03], WEP_ARC_PTR(0x1A));
    PSet(pl->pMotTbl[0x06], WEP_ARC_PTR(0x0B));
    PSet(pl->pMotTbl[0x07], WEP_ARC_PTR(0x1C));
    PSet(pl->pMotTbl[0x08], WEP_ARC_PTR(0x0A));
    PSet(pl->pMotTbl[0x09], WEP_ARC_PTR(0x1B));
    PSet(pl->pMotTbl[0x0B], WEP_ARC_PTR(0x0C));
    PSet(pl->pMotTbl[0x0C], WEP_ARC_PTR(0x1D));
    PSet(pl->pMotTbl[0x0D], WEP_ARC_PTR(0x0D));
    PSet(pl->pMotTbl[0x0E], WEP_ARC_PTR(0x1E));
    PSet(pl->pMotTbl[0x0F], WEP_ARC_PTR(0x0E));
    PSet(pl->pMotTbl[0x10], WEP_ARC_PTR(0x1F));
    PSet(pl->pMotTbl[0x3D], PL_ARC_PTR(pG->pPlArc, 0x5D));
    PSet(pl->pMotTbl[0x3F], WEP_ARC_PTR(0x20));
    PSet(pl->pMotTbl[0x40], WEP_ARC_PTR(0x21));
    PSet(pl->pMotTbl[0x39], WEP_ARC_PTR(0x22));
    PSet(pl->pMotTbl[0x3A], WEP_ARC_PTR(0x23));
    PSet(pl->pMotTbl[0x41], WEP_ARC_PTR(0x24));
    PSet(pl->pMotTbl[0x42], WEP_ARC_PTR(0x25));
    num = ItemMgr.bulletNum();
    if (num > 1) {
        setDisp(0, 1);
    } else {
        setDisp(0, 0);
    }
    if (num) {
        pl->pWep->pObj2->setDisp(0, 1);
    } else {
        pl->pWep->pObj2->setDisp(0, 0);
    }
    if (bulletNum()) {
        hand = WEP_ARC_PTR(0x7);
    } else {
        hand = PL_ARC_PTR(pG->pPlArc, 0x11);
    }
    pl->pBody->initWepHand((u32) hand);
    pl->setRightHand(1);
    pl->setLeftHand(0);
}

int cObjHandGre::keyKamae()
{
    if ((Key.on & 0x10) && ItemMgr.bulletNum()) {
        return 1;
    }
    return 0;
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep30_init;
    WeaponMoveFunc = PlGrenadeMove;
    ObjInitFunc[0x3C] = ObjHandGre_init;
    OSReport("Wep30 ADA|GRENADE prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x3C] = 0;
}

extern "C" void _unresolved()
{
}
