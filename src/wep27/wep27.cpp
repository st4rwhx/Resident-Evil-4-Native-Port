// wep27 module: the Klauser machine gun (own copy of the cObjMachinegun class, routines wep/pl_machine.cpp).

#include "wep_mod.h"
#include "light.h"
#include "esp.h"
#include "item.h"
#include "motion.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

void PlMachineMove(cPlayer* pl);   // wep/pl_machine.cpp
// The module's own copy of the class (wep_mod.h declares the wep11 one); its ObjInitFunc entry is
// static here (the REL field holds S+A).
void ObjKlauMGun_init(cObj* obj);

void Wep27_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(0x2D);

    if (obj == 0) {
        pLog->err(0, 0, "Wep27_init() cObjWep CREATE FAILED");
    } else {
        pl->pWep->pObj = obj;
        obj->init(pl);
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x45, 1);
    }
}

void cObjMachinegun::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0x6), WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjMachinegun::init() modelInit() failed.");
        ObjMgr.destroy(this);
        return;
    }
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        lightInfo.init2(1, 1, &p0, &p1, 1);
    }
    PSet(wep.parent, parent);
    switch (pG->wep_type) {
    case 0:
    default:
        PSet(wep.pMotNormal, WEP_ARC_PTR(0x2A));
        PSet(wep.pMotEmpty, WEP_ARC_PTR(0x2F));
        wep.x24 = 0x30;
        setAbility(7.0f, 2.1f, 0.2864f * 0.7f, 0.2864f * 0.7f);
        break;
    case 1:
        PSet(wep.pMotNormal, WEP_ARC_PTR(0x2A));
        PSet(wep.pMotEmpty, WEP_ARC_PTR(0x2F));
        wep.x24 = 0x31;
        setAbility(5.73f * 0.7f, 2.86f * 0.7f, 0.2864f * 0.7f, 0.2864f * 0.7f);
        break;
    case 2:
        PSet(wep.pMotNormal, WEP_ARC_PTR(0x2B));
        PSet(wep.pMotEmpty, WEP_ARC_PTR(0x2F));
        wep.x24 = 0x32;
        setAbility(5.73f * 0.2f, 2.86f * 0.2f, 0.2864f * 0.5f, 0.2864f * 0.5f);
        break;
    case 3:
        PSet(wep.pMotNormal, WEP_ARC_PTR(0x2B));
        PSet(wep.pMotEmpty, WEP_ARC_PTR(0x2F));
        wep.x24 = 0x33;
        setAbility(5.73f * 0.2f, 2.86f * 0.2f, 0.2864f * 0.5f, 0.2864f * 0.5f);
        break;
    }
    resetMotion();
}

void cObjMachinegun::moveFire()
{
    int type = 0;

    if (wep.step == 0) {
        void* mot;

        switch (pG->wep_type) {
        case 2:
        default:
            if (ItemMgr.bulletNum()) {
                mot = WEP_ARC_PTR(0x27);
            } else {
                mot = WEP_ARC_PTR(0x2D);
            }
            break;
        case 1:
        case 3:
            if (ItemMgr.bulletNum()) {
                mot = WEP_ARC_PTR(0x2C);
            } else {
                mot = WEP_ARC_PTR(0x2E);
            }
            break;
        }
        MotionSetCore(this, &this->mot, mot, 0, 0, 0, 0);
        switch (pG->wep_type) {
        case 2:
        default:
            SndCall(2, 0, &pos, 0, 0, 0);
            pG->flags_500C |= 0x00800000;
            break;
        case 1:
        case 3:
            SndCall(2, 0x18, &pos, 0, 0, 0);
            SndCall(2, 0x15, &pos, 0, 0, 0);
            break;
        }
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xA, 1);
        setCartridge();
        switch (pG->wep_type) {
        case 0:
        case 2:
            type = 0;
            break;
        case 1:
        case 3:
            type = 1;
            break;
        }
        EstSet((int) this, -1, 0, 0, 0x45, type, 0, 0xA, 0, 0);
        wep.step = 1;
    }
    if (MotionGetState(this)) {
        wep.mode = 0;
        wep.step = 0;
    }
}

void cObjMachinegun::setCartridge()
{
    cModel* parts = pPL->getPartsPtr(0xA);
    Vec pos;
    Vec rot;
    Vec spd;
    cObj* obj;

    pos.x = -109.0f;
    pos.y = -22.0f;
    pos.z = 90.0f;
    PSMTXMultVec(parts->mat, &pos, &pos);
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    spd.x = 3.0f;
    spd.y = 60.0f;
    spd.z = 60.0f;
    spd.x += fRand1_1() * 15.0f;
    spd.y += fRand1_1() * 15.0f;
    spd.z += fRand1_1() * 15.0f;
    PSMTXMultVecSR(parts->mat, &spd, &spd);
    obj = SetObj10(WEP_ARC_PTR(0xA), WEP_ARC_PTR(0xB), &pos, &rot, &spd, 10.0f, 50.0f, 0x1E, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

void cObjMachinegun::moveReload()
{
    static const int reloadEnd[4] = { 40, 40, 52, 52 };

    if (wep.step == 0) {
        void* mot;

        if (ItemMgr.bulletNum()) {
            switch (pG->x4FBA) {
            default:
                mot = WEP_ARC_PTR(0x29);
                break;
            case 1:
                mot = WEP_ARC_PTR(0x32);
                break;
            case 2:
                mot = WEP_ARC_PTR(0x33);
                break;
            }
        } else {
            switch (pG->x4FBA) {
            default:
                mot = WEP_ARC_PTR(0x28);
                break;
            case 1:
                mot = WEP_ARC_PTR(0x30);
                break;
            case 2:
                mot = WEP_ARC_PTR(0x31);
                break;
            }
        }
        motionSet(mot, 0, 0, 1, 0);
        wep.seHandle = SndCall(2, 2, &pParts->worldPos, 0, 0, 0);
        wep.step = 1;
    }
    if (MotionCheckCrossFrame(&mot, (f32) reloadEnd[pG->wep_type])) {
        SndCall(2, 4, &pParts->worldPos, 0, 0, 0);
        ItemMgr.reload();
    }
}

void cObjMachinegun::setMotion(cPlayer* pl)
{
    PSet(pl->pMotTbl[0x00], WEP_ARC_PTR(0x0D));
    PSet(pl->pMotTbl[0x02], WEP_ARC_PTR(0x0E));
    PSet(pl->pMotTbl[0x03], WEP_ARC_PTR(0x14));
    PSet(pl->pMotTbl[0x06], WEP_ARC_PTR(0x10));
    PSet(pl->pMotTbl[0x07], WEP_ARC_PTR(0x16));
    PSet(pl->pMotTbl[0x08], WEP_ARC_PTR(0x0F));
    PSet(pl->pMotTbl[0x09], WEP_ARC_PTR(0x15));
    PSet(pl->pMotTbl[0x0B], WEP_ARC_PTR(0x11));
    PSet(pl->pMotTbl[0x0C], WEP_ARC_PTR(0x17));
    PSet(pl->pMotTbl[0x0D], WEP_ARC_PTR(0x12));
    PSet(pl->pMotTbl[0x0E], WEP_ARC_PTR(0x18));
    PSet(pl->pMotTbl[0x0F], WEP_ARC_PTR(0x13));
    PSet(pl->pMotTbl[0x10], WEP_ARC_PTR(0x19));
    PSet(pl->pMotTbl[0x39], WEP_ARC_PTR(0x36));
    PSet(pl->pMotTbl[0x3A], WEP_ARC_PTR(0x37));
    PSet(pl->pMotTbl[0x3D], PL_ARC_PTR(pG->pPlArc, 0x5D));
    PSet(pl->pMotTbl[0x41], WEP_ARC_PTR(0x38));
    PSet(pl->pMotTbl[0x42], WEP_ARC_PTR(0x39));
    PSet(pl->pMotTbl[0x3F], WEP_ARC_PTR(0x34));
    PSet(pl->pMotTbl[0x40], WEP_ARC_PTR(0x35));
    pl->pBody->initWepHand((u32) WEP_ARC_PTR(0xC));
    pl->setRightHand(1);
    pl->setLeftHand(2);
}

void ObjKlauMGun_init(cObj* obj)
{
    new (obj) cObjMachinegun();
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep27_init;
    WeaponMoveFunc = PlMachineMove;
    ObjInitFunc[0x2D] = ObjKlauMGun_init;
    OSReport("Wep27 KLAUSER-MACHINEGUN prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x2D] = 0;
}

extern "C" void _unresolved()
{
}
