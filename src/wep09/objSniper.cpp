// Bolt-action rifle weapon object (wep09 module, first object; real file name unknown): scoped rifle
// with cartridge ejection after the shot and the level-dependent reload motion.

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

class cObjSniper : public cObjWep {
public:
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);

    void setCartridge();
};

// wep.x18..x1A of the object (an extern-linkage const: emitted here, before init's string)
extern const u8 sniper_tbl[3];
const u8 sniper_tbl[3] = { 0x14, 0, 0 };

void ObjSniper_init(cObj* obj)
{
    new (obj) cObjSniper();
}

void cObjSniper::init(cModel* parent)
{
    U16Set(wep.x24, 0x2E);
    if (modelInit(WEP_ARC_PTR(0xA), WEP_ARC_PTR(0x9)) == 0) {
        pLog->err(0, 0, "cObjSniper::init() failed.");
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
    PSet(wep.pMotNormal, WEP_ARC_PTR(0x23));
    resetMotion();
    wep.x18 = sniper_tbl[0];
    wep.x19 = sniper_tbl[1];
    wep.x1A = sniper_tbl[2];
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

void cObjSniper::moveFire()
{
    if (wep.step == 0) {
        MotionSetCore(this, &this->mot, WEP_ARC_PTR(0x21), 0, 0, 0, 0);
        motSpeedRate = 1.0f;
        SndCall(2, 4, &getPartsPtr(0)->worldPos, 0, 0, 0);
        pG->flags_500C |= 0x00800000;
        wep.step = 1;
    } else {
        if (MotionCheckCrossFrame(&mot, 14.0f)) {
            setCartridge();
        }
        if (MotionGetState(this)) {
            wep.mode = 0;
            wep.step = 0;
        }
    }
}

void cObjSniper::setCartridge()
{
    cModel* parts = getPartsPtr(1);
    Vec pos;
    Vec rot;
    Vec spd;
    cObj* obj;

    pos.x = -240.0f;
    pos.y = -20.0f;
    pos.z = 70.0f;
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
    obj = SetObj10(WEP_ARC_PTR(0xB), WEP_ARC_PTR(0xC), &pos, &rot, &spd, 10.0f, 50.0f, 0x28, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

void cObjSniper::moveReload()
{
    if (wep.step == 0) {
        void* m;
        u16 se;

        switch (pG->x4FBA) {
        default:
            m = WEP_ARC_PTR(0x22);
            break;
        case 1:
            m = WEP_ARC_PTR(0x25);
            break;
        case 2:
            m = WEP_ARC_PTR(0x26);
            break;
        }
        MotionSetCore(this, &this->mot, m, 0, 0, 0, 0);
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
        wep.seHandle = SndCall(2, se, &getPartsPtr(0)->worldPos, 0, 0, 0);
        wep.step = 1;
    }
    if (MotionCheckCrossFrame(&mot, 10.0f)) {
        EstSet((int) this, -1, 0, 0, 0x3D, 0, 0, 0xA, 0, 0);
        ItemMgr.reload();
    }
}

void cObjSniper::setMotion(cPlayer* pl)
{
    PSet(pl->pMotTbl[0x00], WEP_ARC_PTR(0x0E));
    PSet(pl->pMotTbl[0x02], WEP_ARC_PTR(0x11));
    PSet(pl->pMotTbl[0x03], WEP_ARC_PTR(0x29));
    PSet(pl->pMotTbl[0x06], WEP_ARC_PTR(0x13));
    PSet(pl->pMotTbl[0x07], WEP_ARC_PTR(0x2B));
    PSet(pl->pMotTbl[0x08], WEP_ARC_PTR(0x12));
    PSet(pl->pMotTbl[0x09], WEP_ARC_PTR(0x2A));
    PSet(pl->pMotTbl[0x0B], WEP_ARC_PTR(0x18));
    PSet(pl->pMotTbl[0x0C], WEP_ARC_PTR(0x2D));
    PSet(pl->pMotTbl[0x0D], WEP_ARC_PTR(0x0F));
    PSet(pl->pMotTbl[0x0E], WEP_ARC_PTR(0x27));
    PSet(pl->pMotTbl[0x0F], WEP_ARC_PTR(0x10));
    PSet(pl->pMotTbl[0x10], WEP_ARC_PTR(0x28));
    PSet(pl->pMotTbl[0x5B], WEP_ARC_PTR(0x1F));
    PSet(pl->pMotTbl[0x57], WEP_ARC_PTR(0x20));
    PSet(pl->pMotTbl[0x39], WEP_ARC_PTR(0x3A));
    PSet(pl->pMotTbl[0x3A], WEP_ARC_PTR(0x3B));
    PSet(pl->pMotTbl[0x3D], PL_ARC_PTR(pG->pPlArc, 0x5D));
    PSet(pl->pMotTbl[0x41], WEP_ARC_PTR(0x3C));
    PSet(pl->pMotTbl[0x42], WEP_ARC_PTR(0x3D));
    PSet(pl->pMotTbl[0x3F], WEP_ARC_PTR(0x38));
    PSet(pl->pMotTbl[0x40], WEP_ARC_PTR(0x39));
    pl->pBody->initWepHand((u32) WEP_ARC_PTR(0xD));
    pl->setRightHand(1);
    pl->setLeftHand(2);
}
