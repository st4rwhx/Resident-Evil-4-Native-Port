// Thompson weapon object (wep12 module, first object; real file name unknown): a machine gun
// without weapon types (fixed model / ability), cartridge ejection and level-dependent reload.

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

void ObjTompson_init(cObj* obj)
{
    new (obj) cObjTompson();
}

void cObjTompson::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0x6), WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjTompson::init() failed.");
        return;
    }
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        lightInfo.init2(1, 1, &p0, &p1, 1);
    }
    wep.parent = parent;
    U16Set(wep.x24, 0x34);
    wep.pMotNormal = WEP_ARC_PTR(0x29);
    resetMotion();
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

void cObjTompson::moveFire()
{
    if (wep.step == 0) {
        void* mot;

        if (ItemMgr.bulletNum()) {
            mot = WEP_ARC_PTR(0x27);
        } else {
            mot = WEP_ARC_PTR(0x2A);
        }
        MotionSetCore(this, &this->mot, mot, 0, 0, 0, 0);
        EstSet((int) this, -1, 0, 0, 0x46, 0, 0, 0, (u32) this, 0);
        SndCall(2, 0, &pParts->worldPos, 0, 0, 0);
        SndCall(2, 0x15, &pos, 0, 0, 0);
        pG->flags_500C |= 0x00800000;
        setCartridge();
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xA, 1);
        wep.step = 1;
    }
}

void cObjTompson::moveReload()
{
    static const f32 reloadEnd[3] = { 40.0f, 34.0f, 28.0f };

    if (wep.step == 0) {
        void* mot;
        u16 se;

        switch (pG->x4FBA) {
        default:
            mot = WEP_ARC_PTR(0x28);
            break;
        case 1:
            mot = WEP_ARC_PTR(0x2B);
            break;
        case 2:
            mot = WEP_ARC_PTR(0x2C);
            break;
        }
        motionSet(mot, 0, 0, 1, 0);
        switch (pG->x4FBA) {
        default:
            se = 2;
            break;
        case 1:
            se = 0x20;
            break;
        case 2:
            se = 0x21;
            break;
        }
        wep.seHandle = SndCall(2, se, &pParts->worldPos, 0, 0, 0);
        wep.step = 1;
    } else if (MotionCheckCrossFrame(&mot, reloadEnd[pG->x4FBA])) {
        ItemMgr.reload();
    }
}

void cObjTompson::setCartridge()
{
    cModel* parts = pPL->getPartsPtr(0xA);
    Vec pos;
    Vec rot;
    Vec spd;
    cObj* obj;

    pos.x = -216.0f;
    pos.y = -24.0f;
    pos.z = 105.9f;
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
    obj = SetObj10(WEP_ARC_PTR(0xA), WEP_ARC_PTR(0xB), &pos, &rot, &spd, 10.0f, 50.0f, 0x28, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

void cObjTompson::setMotion(cPlayer* pl)
{
    PSet(pl->pMotTbl[0x00], WEP_ARC_PTR(0x0D));
    PSet(pl->pMotTbl[0x01], 0);
    PSet(pl->pMotTbl[0x02], WEP_ARC_PTR(0x12));
    PSet(pl->pMotTbl[0x03], WEP_ARC_PTR(0x13));
    PSet(pl->pMotTbl[0x06], WEP_ARC_PTR(0x16));
    PSet(pl->pMotTbl[0x07], WEP_ARC_PTR(0x17));
    PSet(pl->pMotTbl[0x08], WEP_ARC_PTR(0x14));
    PSet(pl->pMotTbl[0x09], WEP_ARC_PTR(0x15));
    PSet(pl->pMotTbl[0x0B], WEP_ARC_PTR(0x18));
    PSet(pl->pMotTbl[0x0C], WEP_ARC_PTR(0x19));
    PSet(pl->pMotTbl[0x0D], WEP_ARC_PTR(0x0E));
    PSet(pl->pMotTbl[0x0E], WEP_ARC_PTR(0x0F));
    PSet(pl->pMotTbl[0x0F], WEP_ARC_PTR(0x10));
    PSet(pl->pMotTbl[0x10], WEP_ARC_PTR(0x11));
    PSet(pl->pMotTbl[0x5D], 0);
    PSet(pl->pMotTbl[0x5E], 0);
    PSet(pl->pMotTbl[0x5B], WEP_ARC_PTR(0x25));
    PSet(pl->pMotTbl[0x57], WEP_ARC_PTR(0x26));
    PSet(pl->pMotTbl[0x52], WEP_ARC_PTR(0x29));
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
