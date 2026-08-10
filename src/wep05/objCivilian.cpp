// Civilian handgun weapon object (wep05 module, first object; real file name unknown): a handgun
// without weapon types, fire / reload motions by reload tune level.

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"

class cObjCivilian : public cObjWep {
public:
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);
};

// wep.x18..x1A of the object (an extern-linkage const: emitted here, before init's string)
extern const u8 civilian_tbl[3];
const u8 civilian_tbl[3] = { 0x14, 0x14, 0x14 };

void ObjCivilian_init(cObj* obj)
{
    new (obj) cObjCivilian();
}

void cObjCivilian::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0x6), WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjWep::init() failed.");
        return;
    }
    sub2B4.atari.init(1, 0, 0, 0.0f, 100.0f, 0.0f, 0.0f, 100.0f, 100.0f, 100.0f);
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        lightInfo.init2(1, 1, &p0, &p1, 1);
    }
    wep.parent = parent;
    U16Set(wep.x24, 0x29);
    PSet(wep.pMotNormal, WEP_ARC_PTR(0x34));
    resetMotion();
    wep.x18 = civilian_tbl[0];
    wep.x19 = civilian_tbl[1];
    wep.x1A = civilian_tbl[2];
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

void cObjCivilian::moveFire()
{
    if (wep.step == 0) {
        MotionSetCore(this, &this->mot, WEP_ARC_PTR(0x32), 0, 0, 0, 0);
        SndCall(2, 0, &pos, 0, 0, 0);
        SndCall(2, 2, &pos, 0, 0, 0);
        pG->flags_500C |= 0x00800000;
        EstSet((int) this, -1, 0, 0, 0x39, 0, 0, 0xA, 0, 0);
        wep.step = 1;
    }
    if (MotionGetState(this)) {
        wep.mode = 0;
        wep.step = 0;
    }
}

void cObjCivilian::moveReload()
{
    static const f32 reloadEnd[3] = { 75.0f, 48.0f, 28.0f };

    if (wep.step == 0) {
        void* m;
        u16 se;

        switch (pG->x4FBA) {
        default:
            m = WEP_ARC_PTR(0x33);
            break;
        case 1:
            m = WEP_ARC_PTR(0x35);
            break;
        case 2:
            m = WEP_ARC_PTR(0x36);
            break;
        }
        motionSet(m, 0, 0, 1, 0);
        switch (pG->x4FBA) {
        default:
            se = 0x16;
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

void cObjCivilian::setMotion(cPlayer* pl)
{
    PSet(pl->pMotTbl[0x00], WEP_ARC_PTR(0x0B));
    PSet(pl->pMotTbl[0x01], WEP_ARC_PTR(0x0C));
    PSet(pl->pMotTbl[0x02], WEP_ARC_PTR(0x11));
    PSet(pl->pMotTbl[0x03], WEP_ARC_PTR(0x12));
    PSet(pl->pMotTbl[0x06], WEP_ARC_PTR(0x15));
    PSet(pl->pMotTbl[0x07], WEP_ARC_PTR(0x16));
    PSet(pl->pMotTbl[0x08], WEP_ARC_PTR(0x13));
    PSet(pl->pMotTbl[0x09], WEP_ARC_PTR(0x14));
    PSet(pl->pMotTbl[0x0A], WEP_ARC_PTR(0x13));
    PSet(pl->pMotTbl[0x0B], WEP_ARC_PTR(0x17));
    PSet(pl->pMotTbl[0x0C], WEP_ARC_PTR(0x18));
    PSet(pl->pMotTbl[0x0D], WEP_ARC_PTR(0x0D));
    PSet(pl->pMotTbl[0x0E], WEP_ARC_PTR(0x0E));
    PSet(pl->pMotTbl[0x0F], WEP_ARC_PTR(0x0F));
    PSet(pl->pMotTbl[0x10], WEP_ARC_PTR(0x10));
    PSet(pl->pMotTbl[0x39], WEP_ARC_PTR(0x19));
    PSet(pl->pMotTbl[0x3A], WEP_ARC_PTR(0x1A));
    PSet(pl->pMotTbl[0x3D], PL_ARC_PTR(pG->pPlArc, 0x5D));
    PSet(pl->pMotTbl[0x41], WEP_ARC_PTR(0x1B));
    PSet(pl->pMotTbl[0x42], WEP_ARC_PTR(0x1C));
    PSet(pl->pMotTbl[0x3F], WEP_ARC_PTR(0x1D));
    PSet(pl->pMotTbl[0x40], WEP_ARC_PTR(0x1E));
    PSet(pl->pMotTbl[0x5D], WEP_ARC_PTR(0x1F));
    PSet(pl->pMotTbl[0x5E], WEP_ARC_PTR(0x20));
    PSet(pl->pMotTbl[0x57], WEP_ARC_PTR(0x31));
    PSet(pl->pMotTbl[0x5B], WEP_ARC_PTR(0x30));
    pl->pBody->initWepHand((u32) WEP_ARC_PTR(0x8));
    pl->setRightHand(1);
    pl->setLeftHand((u32) WEP_ARC_PTR(0x9));
}
