// wep04 module: the XD9 handgun (cObjXd9, object id 0x33). The module object carries the class,
// the entry points and the handgun routine registration (wep/pl_handgun.cpp).

#include "wep_mod.h"
#include "light.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

void PlHandgunMove(cPlayer* pl);   // wep/pl_handgun.cpp
cObjWep* equipWeapon(cPlayer* pl);

class cObjXd9 : public cObjWep {
public:
    virtual ~cObjXd9() {}
    virtual void moveFire();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);

    void setCartridge();
};

// wep.x18..x1B of the object (an extern-linkage const: emitted here, before Wep04_init's strings)
extern const u8 xd9_tbl[4];
const u8 xd9_tbl[4] = { 0xE, 0xC, 0x8, 0x8 };

void Wep04_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep04_init() wep model init failed.");
    } else {
        pl->pWep->pObj = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), 0x38, 1);
        PlWepMot[0] = WEP_ARC_PTR(0x26);
        PlWepMot[1] = WEP_ARC_PTR(0x27);
        PlWepMot[2] = WEP_ARC_PTR(0x28);
    }
}

cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(0x33);

    if (obj == 0) {
        pLog->err(0, 0, "Wep04_init() cObjWep CREATE FAILED");
        return 0;
    }
    obj->init(pl);
    return obj;
}

void ObjXd9_init(cObj* obj)
{
    new (obj) cObjXd9();
}

void cObjXd9::init(cModel* parent)
{
    void* bin;

    if (pG->wep_type != 1) {
        bin = WEP_ARC_PTR(0x6);
        wep.x24 = 0x27;
    } else {
        bin = WEP_ARC_PTR(0x7);
        wep.x24 = 0x28;
    }
    if (modelInit(bin, WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjWep::init() failed.");
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
    PSet(wep.pMotNormal, WEP_ARC_PTR(0x34));
    PSet(wep.pMotEmpty, WEP_ARC_PTR(0x39));
    resetMotion();
    wep.x18 = xd9_tbl[0];
    wep.x19 = xd9_tbl[1];
    wep.x1A = xd9_tbl[2];
    wep.x1B = xd9_tbl[3];
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

void cObjXd9::moveFire()
{
    if (wep.step == 0) {
        void* m;
        int type;
        int zero;

        if (ItemMgr.bulletNum()) {
            switch (pG->wep_type) {
            case 0:
            default:
                m = WEP_ARC_PTR(0x32);
                break;
            case 1:
                m = WEP_ARC_PTR(0x35);
                break;
            }
        } else {
            switch (pG->wep_type) {
            case 0:
            default:
                m = WEP_ARC_PTR(0x37);
                break;
            case 1:
                m = WEP_ARC_PTR(0x38);
                break;
            }
        }
        MotionSetCore(this, &mot, m, 0, 0, 0, 0);
        SndCall(2, 2, &pos, 0, 0, 0);
        SndCall(2, 4, &pos, 0, 0, 0);
        zero = 0;
        SndCall(2, zero, &pos, 0, 0, 0);
        BitOn(pG->flags_500C, 0x00800000);
        type = 0;
        if (pG->wep_type == 1) {
            type = 1;
        }
        EstSet((int) this, -1, 0, 0, 0x38, type, 0, 0xA, 0, 0);
        setCartridge();
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0, 1);
        wep.step = 1;
    } else if (MotionGetState(this)) {
        wep.mode = 0;
        wep.step = 0;
    }
}

void cObjXd9::moveReload()
{
    static const f32 reloadEnd[3] = { 32.0f, 27.0f, 17.0f };

    if (wep.step == 0) {
        void* m;
        u16 se;

        if (ItemMgr.bulletNum()) {
            switch (pG->x4FBA) {
            default:
                m = WEP_ARC_PTR(0x36);
                break;
            case 1:
                m = WEP_ARC_PTR(0x3C);
                break;
            case 2:
                m = WEP_ARC_PTR(0x3D);
                break;
            }
        } else {
            switch (pG->x4FBA) {
            default:
                m = WEP_ARC_PTR(0x33);
                break;
            case 1:
                m = WEP_ARC_PTR(0x3A);
                break;
            case 2:
                m = WEP_ARC_PTR(0x3B);
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
        wep.seHandle = SndCall(2, se, &pParts->worldPos, 0, 0, 0);
        wep.step = 1;
    } else if (MotionCheckCrossFrame(&mot, reloadEnd[pG->x4FBA])) {
        ItemMgr.reload();
    }
}

void cObjXd9::setCartridge()
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
    obj = SetObj10(WEP_ARC_PTR(0x8), WEP_ARC_PTR(0x9), &pos, &rot, &spd, 10.0f, 50.0f, 0x1E, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

void cObjXd9::setMotion(cPlayer* pl)
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
    PSet(pl->pMotTbl[0x5B], WEP_ARC_PTR(0x30));
    PSet(pl->pMotTbl[0x57], WEP_ARC_PTR(0x31));
    pl->pBody->initWepHand((u32) WEP_ARC_PTR(0xA));
    pl->setRightHand(1);
    pl->setLeftHand(4);
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep04_init;
    WeaponMoveFunc = PlHandgunMove;
    ObjInitFunc[0x33] = ObjXd9_init;
    OSReport("Wep04 XD9 prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x33] = 0;
    OSReport("Wep04 XD9 epilog Ok\n");
}

extern "C" void _unresolved()
{
}
