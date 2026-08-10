// wep40 module: Ada's semi-auto rifle (own copy of the cObjHkSniper class, object id 0x30; routines
// wep/pl_rifle.cpp).

#include "wep_mod.h"
#include "light.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

void PlRifleMove(cPlayer* pl);   // wep/pl_rifle.cpp
cObjWep* equipWeapon(cPlayer* pl);
void ObjHkSniper_init(cObj* obj);

void Wep40_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep02_init() wep model init failed.");
    } else {
        pl->pWep->pObj = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x8), 0x44, 1);
        PlWepMot[0] = WEP_ARC_PTR(0xE);
        PlWepMot[1] = PL_ARC_PTR(pG->pPlArc, 0x5E);
        PlWepMot[2] = WEP_ARC_PTR(0xE);
    }
}

cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(0x30);

    if (obj == 0) {
        pLog->err(0, 0, "Wep09_init() cObjWep CREATE FAILED");
        pl->pWep->pObj = obj;
        return 0;
    }
    obj->init(pl);
    return obj;
}

// wep.x18..x1A of the object (an extern-linkage const: emitted here, before init's string)
extern const u8 hksniper_tbl[3];
const u8 hksniper_tbl[3] = { 0x14, 0xA, 0 };

void cObjHkSniper::init(cModel* parent)
{
    void* bin;

    U16Set(wep.x24, 0x2F);
    if (pG->wep_type == 0) {
        bin = WEP_ARC_PTR(0xA);
    } else {
        bin = WEP_ARC_PTR(0xB);
    }
    if (modelInit(bin, WEP_ARC_PTR(0x9)) == 0) {
        pLog->err(0, 0, "cObjSniper::init() failed.");
        ObjMgr.destroy(this);
        return;
    }
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    pParts->pos.x = 0.5f;
    pParts->pos.y = -5.0f;
    pParts->pos.z = -3.0f;
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        lightInfo.init2(1, 1, &p0, &p1, 1);
    }
    wep.parent = parent;
    wep.x18 = hksniper_tbl[0];
    wep.x19 = hksniper_tbl[1];
    wep.x1A = hksniper_tbl[2];
    PSet(wep.pMotNormal, WEP_ARC_PTR(0x22));
    resetMotion();
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

void cObjHkSniper::moveFire()
{
    if (wep.step == 0) {
        pMotion = 0;
        SndCall(2, 0, &pParts->worldPos, 0, 0, 0);
        SndCall(2, 4, &pParts->worldPos, 0, 0, 0);
        BitOn(pG->flags_500C, 0x00800000);
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0, 1);
        wep.step = 1;
    }
}

void cObjHkSniper::moveReload()
{
    if (wep.step == 0) {
        MotionSetCore(this, &this->mot, WEP_ARC_PTR(0x25), 0, 0, 0, 0);
        wep.seHandle = SndCall(2, 2, &pParts->worldPos, 0, 0, 0);
        wep.step = 1;
    }
    if (MotionCheckCrossFrame(&mot, 34.0f)) {
        ItemMgr.reload();
    }
}

void cObjHkSniper::setMotion(cPlayer* pl)
{
    PSet(pl->pMotTbl[0x00], WEP_ARC_PTR(0x0E));
    PSet(pl->pMotTbl[0x01], 0);
    PSet(pl->pMotTbl[0x02], WEP_ARC_PTR(0x11));
    PSet(pl->pMotTbl[0x03], WEP_ARC_PTR(0x29));
    PSet(pl->pMotTbl[0x06], WEP_ARC_PTR(0x13));
    PSet(pl->pMotTbl[0x07], WEP_ARC_PTR(0x2B));
    PSet(pl->pMotTbl[0x08], WEP_ARC_PTR(0x12));
    PSet(pl->pMotTbl[0x09], WEP_ARC_PTR(0x2A));
    PSet(pl->pMotTbl[0x0B], WEP_ARC_PTR(0x18));
    PSet(pl->pMotTbl[0x0C], WEP_ARC_PTR(0x2C));
    PSet(pl->pMotTbl[0x0D], WEP_ARC_PTR(0x0F));
    PSet(pl->pMotTbl[0x0E], WEP_ARC_PTR(0x27));
    PSet(pl->pMotTbl[0x0F], WEP_ARC_PTR(0x10));
    PSet(pl->pMotTbl[0x10], WEP_ARC_PTR(0x28));
    PSet(pl->pMotTbl[0x3D], PL_ARC_PTR(pG->pPlArc, 0x5D));
    PSet(pl->pMotTbl[0x3F], WEP_ARC_PTR(0x38));
    PSet(pl->pMotTbl[0x40], WEP_ARC_PTR(0x39));
    PSet(pl->pMotTbl[0x39], WEP_ARC_PTR(0x3A));
    PSet(pl->pMotTbl[0x3A], WEP_ARC_PTR(0x3B));
    PSet(pl->pMotTbl[0x41], WEP_ARC_PTR(0x3C));
    PSet(pl->pMotTbl[0x42], WEP_ARC_PTR(0x3D));
    pl->pBody->initWepHand((u32) WEP_ARC_PTR(0xD));
    pl->setRightHand(1);
    pl->setLeftHand(2);
}

void ObjHkSniper_init(cObj* obj)
{
    new (obj) cObjHkSniper();
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep40_init;
    WeaponMoveFunc = PlRifleMove;
    ObjInitFunc[0x30] = ObjHkSniper_init;
    OSReport("Wep40 ADA SEMIAUTO-RIFLE prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x30] = 0;
}

extern "C" void _unresolved()
{
}
