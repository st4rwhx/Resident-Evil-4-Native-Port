// VP70 weapon object (wep17 module, first object; real file name unknown): a Mauser-style handgun
// with ready / fire motions, cartridge ejection and the reload by tune level.

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

class cObjVp70 : public cObjWep {
public:
    virtual ~cObjVp70() {}
    virtual void moveReady();
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);

    void setCartridge();
};

// wep.x18..x1A of the object (an extern-linkage const: emitted here, before init's string)
extern const u8 vp70_tbl[3];
const u8 vp70_tbl[3] = { 0xE, 0xC, 0xA };

#define VP70_ARC_PTR(no) PL_ARC_PTR(pG->pPlArc, no)

void ObjVp70_init(cObj* obj)
{
    new (obj) cObjVp70();
}

void cObjVp70::init(cModel* parent)
{
    if (modelInit(WEP_ARC_PTR(0x6), WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjVp70::init() failed.");
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
    wep.x24 = 3;
    PSet(wep.pMotNormal, WEP_ARC_PTR(0x36));
    PSet(wep.pMotEmpty, WEP_ARC_PTR(0x38));
    resetMotion();
    wep.x18 = vp70_tbl[0];
    wep.x19 = vp70_tbl[1];
    wep.x1A = vp70_tbl[2];
    setAbility(1.146f, 0.57199997f, 0.1432f, 0.1432f);
}

void cObjVp70::moveReady()
{
    if (wep.step == 0) {
        MotionSetCore(this, &mot, WEP_ARC_PTR(0x39), 0, 0, 0, 0);
        wep.step = 1;
    } else if (MotionGetState(this)) {
        wep.mode = 0;
        wep.step = 0;
    }
}

void cObjVp70::moveFire()
{
    if (wep.step == 0) {
        void* m;

        if (ItemMgr.bulletNum()) {
            m = WEP_ARC_PTR(0x35);
        } else {
            m = WEP_ARC_PTR(0x37);
        }
        MotionSetCore(this, &mot, m, 0, 0, 0, 0);
        SndCall(2, 0, &pos, 0, 0, 0);
        pG->flags_500C |= 0x00800000;
        EstSet((int) this, -1, 0, 0, 0x4B, 0, 0, 0xA, 0, 0);
        setCartridge();
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0, 1);
        wep.step = 1;
    }
}

void cObjVp70::moveReload()
{
    if (wep.step == 0) {
        void* m;
        u16 se;

        if (ItemMgr.bulletNum()) {
            switch (pG->x4FBA) {
            default:
                m = WEP_ARC_PTR(0x26);
                break;
            case 1:
                m = WEP_ARC_PTR(0x2B);
                break;
            case 2:
                m = WEP_ARC_PTR(0x2C);
                break;
            }
        } else {
            switch (pG->x4FBA) {
            default:
                m = WEP_ARC_PTR(0x25);
                break;
            case 1:
                m = WEP_ARC_PTR(0x28);
                break;
            case 2:
                m = WEP_ARC_PTR(0x29);
                break;
            }
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
        wep.seHandle = SndCall(2, se, &getPartsPtr(0)->worldPos, 0, 0, 0);
        wep.step = 1;
    }
    {
        // reload frame (the magazine change) by reload tune level
        static const f32 reloadFrame[3] = { 33.0f, 27.0f, 19.0f };

        if (MotionCheckCrossFrame(&mot, reloadFrame[pG->x4FBA])) {
            ItemMgr.reload();
        }
    }
}

void cObjVp70::setCartridge()
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
    obj = SetObj10(WEP_ARC_PTR(0x7), WEP_ARC_PTR(0x8), &pos, &rot, &spd, 10.0f, 50.0f, 0x1E, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

void cObjVp70::setMotion(cPlayer* pl)
{
    PSet(pl->pMotTbl[0x00], WEP_ARC_PTR(0x0A));
    PSet(pl->pMotTbl[0x01], (void*) 0);
    PSet(pl->pMotTbl[0x02], WEP_ARC_PTR(0x0D));
    PSet(pl->pMotTbl[0x03], WEP_ARC_PTR(0x1D));
    PSet(pl->pMotTbl[0x06], WEP_ARC_PTR(0x0F));
    PSet(pl->pMotTbl[0x07], WEP_ARC_PTR(0x1F));
    PSet(pl->pMotTbl[0x08], WEP_ARC_PTR(0x0E));
    PSet(pl->pMotTbl[0x09], WEP_ARC_PTR(0x1E));
    PSet(pl->pMotTbl[0x0B], WEP_ARC_PTR(0x10));
    PSet(pl->pMotTbl[0x0C], WEP_ARC_PTR(0x20));
    PSet(pl->pMotTbl[0x0D], WEP_ARC_PTR(0x0B));
    PSet(pl->pMotTbl[0x0E], WEP_ARC_PTR(0x1B));
    PSet(pl->pMotTbl[0x0F], WEP_ARC_PTR(0x0C));
    PSet(pl->pMotTbl[0x10], WEP_ARC_PTR(0x1C));
    PSet(pl->pMotTbl[0x5B], WEP_ARC_PTR(0x41));
    PSet(pl->pMotTbl[0x57], WEP_ARC_PTR(0x42));
    PSet(pl->pMotTbl[0x39], WEP_ARC_PTR(0x45));
    PSet(pl->pMotTbl[0x3A], WEP_ARC_PTR(0x46));
    PSet(pl->pMotTbl[0x3D], PL_ARC_PTR(pG->pPlArc, 0x5D));
    PSet(pl->pMotTbl[0x41], WEP_ARC_PTR(0x47));
    PSet(pl->pMotTbl[0x42], WEP_ARC_PTR(0x48));
    PSet(pl->pMotTbl[0x3F], WEP_ARC_PTR(0x43));
    PSet(pl->pMotTbl[0x40], WEP_ARC_PTR(0x44));
    pl->pBody->initWepHand((u32) WEP_ARC_PTR(0x9));
    pl->setRightHand(1);
    pl->setLeftHand(4);
}
