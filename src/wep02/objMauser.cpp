// Red9 (Mauser) weapon object (wep02 module, first object; real file name unknown): model by weapon
// type, ready / fire / down motions, cartridge ejection, the two-step reload (magazine then pin).

#include "wep_mod.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"

class cObjMauser : public cObjWep {
public:
    virtual ~cObjMauser() {}
    virtual void moveReady();
    virtual void moveFire();
    virtual void moveDown();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);

    void setCartridge();
    void setPin();
};

// reload motion frames of the magazine change / the pin (moveReload sets them by tune level)
static f32 reloadFrame;
static f32 pinFrame;

// wep.x18..x1A of the object (an extern-linkage const: emitted here, before init's string)
extern const u8 mauser_tbl[3];
const u8 mauser_tbl[3] = { 0xE, 0xC, 0xA };

void ObjMauser_init(cObj* obj)
{
    new (obj) cObjMauser();
}

void cObjMauser::init(cModel* parent)
{
    void* bin;

    if (pG->wep_type != 2) {
        bin = WEP_ARC_PTR(0x6);
        wep.x24 = 0x25;
        setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
    } else {
        bin = WEP_ARC_PTR(0x7);
        wep.x24 = 0x26;
        setAbility(1.146f, 0.57199997f, 0.1432f, 0.1432f);
    }
    if (modelInit(bin, WEP_ARC_PTR(0x5)) == 0) {
        pLog->err(0, 0, "cObjMauser::init() failed.");
        return;
    }
    sub2B4.atari.init(1, 0, 0, 0.0f, 100.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    AtariFlagsAnd(&sub2B4.atari, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(0xA);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        lightInfo.init2(1, 1, &p0, &p1, 1);
    }
    wep.parent = parent;
    wep.x18 = mauser_tbl[0];
    wep.x19 = mauser_tbl[1];
    wep.x1A = mauser_tbl[2];
    PSet(wep.pMotNormal, WEP_ARC_PTR(0x34));
    PSet(wep.pMotEmpty, WEP_ARC_PTR(0x38));
    resetMotion();
}

void cObjMauser::moveReady()
{
    if (wep.step == 0) {
        MotionSetCore(this, &mot, WEP_ARC_PTR(0x3B), 0, 0, 0, 0);
        wep.step = 1;
    } else if (MotionGetState(this)) {
        wep.mode = 0;
        wep.step = 0;
    }
}

void cObjMauser::moveFire()
{
    if (wep.step == 0) {
        void* m;

        if (ItemMgr.bulletNum()) {
            m = WEP_ARC_PTR(0x32);
        } else {
            m = WEP_ARC_PTR(0x37);
        }
        MotionSetCore(this, &mot, m, 0, 0, 0, 0);
        SndCall(2, 0, &pos, 0, 0, 0);
        SndCall(2, 2, &pos, 0, 0, 0);
        pG->flags_500C |= 0x00800000;
        EstSet((int) this, -1, 0, 0, 0x36, 0, 0, 0xA, 0, 0);
        setCartridge();
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0, 1);
        wep.step = 1;
    }
    if (MotionGetState(this)) {
        wep.mode = 0;
        wep.step = 0;
    }
}

void cObjMauser::moveDown()
{
    if (wep.step == 0) {
        MotionSetCore(this, &mot, WEP_ARC_PTR(0x3C), 0, 0, 0, 0);
        wep.step = 1;
    } else if (MotionGetState(this)) {
        wep.mode = 0;
        wep.step = 0;
    }
}

void cObjMauser::setCartridge()
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

// Reload motion by ammunition state and reload tune level (x4FBA); the magazine / pin frames of
// that motion go to reloadFrame / pinFrame.
#define MAUSER_RELOAD_MOTION(m)                    \
    if (ItemMgr.bulletNum()) {                     \
        switch (pG->x4FBA) {                       \
        default:                                   \
            m = WEP_ARC_PTR(0x36);                 \
            reloadFrame = 44.0f;                   \
            pinFrame = 55.0f;                      \
            break;                                 \
        case 1:                                    \
            m = WEP_ARC_PTR(0x41);                 \
            reloadFrame = 34.0f;                   \
            pinFrame = 51.0f;                      \
            break;                                 \
        case 2:                                    \
            m = WEP_ARC_PTR(0x42);                 \
            reloadFrame = 24.0f;                   \
            pinFrame = 39.0f;                      \
            break;                                 \
        }                                          \
    } else {                                       \
        switch (pG->x4FBA) {                       \
        default:                                   \
            m = WEP_ARC_PTR(0x33);                 \
            reloadFrame = 44.0f;                   \
            pinFrame = 55.0f;                      \
            break;                                 \
        case 1:                                    \
            m = WEP_ARC_PTR(0x3F);                 \
            reloadFrame = 34.0f;                   \
            pinFrame = 51.0f;                      \
            break;                                 \
        case 2:                                    \
            m = WEP_ARC_PTR(0x40);                 \
            reloadFrame = 24.0f;                   \
            pinFrame = 39.0f;                      \
            break;                                 \
        }                                          \
    }

void cObjMauser::moveReload()
{
    if (wep.step == 0) {
        void* m;
        u16 se;

        if (pG->wep_type != 2) {
            MAUSER_RELOAD_MOTION(m);
        } else {
            MAUSER_RELOAD_MOTION(m);
        }
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
        motionSet(m, 0, 0, 1, 0);
        wep.seHandle = SndCall(2, se, &getPartsPtr(0)->worldPos, 0, 0, 0);
        wep.step = 1;
    }
    if (MotionCheckCrossFrame(&mot, reloadFrame)) {
        ItemMgr.reload();
    }
    if (MotionCheckCrossFrame(&mot, pinFrame)) {
        setPin();
    }
}

void cObjMauser::setPin()
{
    const f32 rad = 50.0f;
    const f32 grav = 8.0f;
    Vec pos;
    Vec rot;
    Vec spd;
    cObj* obj;

    pos.x = -270.0f;
    pos.y = 0.0f;
    pos.z = 100.0f;
    PSMTXMultVec(pParts->mat, &pos, &pos);
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    spd.x = 1.0f;
    spd.y = 40.0f;
    spd.z = 40.0f;
    spd.x += fRand1_1() * 5.0f;
    spd.y += fRand1_1() * 5.0f;
    spd.z += fRand1_1() * 5.0f;
    PSMTXMultVecSR(pParts->mat, &spd, &spd);
    obj = SetObj10(WEP_ARC_PTR(0x43), WEP_ARC_PTR(0x44), &pos, &rot, &spd, grav, rad, 0x1E, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

void cObjMauser::setMotion(cPlayer* pl)
{
    PSet(pl->pMotTbl[0x00], WEP_ARC_PTR(0x0B));
    PSet(pl->pMotTbl[0x01], WEP_ARC_PTR(0x0C));
    PSet(pl->pMotTbl[0x02], WEP_ARC_PTR(0x11));
    PSet(pl->pMotTbl[0x03], WEP_ARC_PTR(0x12));
    PSet(pl->pMotTbl[0x06], WEP_ARC_PTR(0x15));
    PSet(pl->pMotTbl[0x07], WEP_ARC_PTR(0x16));
    PSet(pl->pMotTbl[0x08], WEP_ARC_PTR(0x13));
    PSet(pl->pMotTbl[0x09], WEP_ARC_PTR(0x14));
    PSet(pl->pMotTbl[0x0B], WEP_ARC_PTR(0x17));
    PSet(pl->pMotTbl[0x0C], WEP_ARC_PTR(0x18));
    PSet(pl->pMotTbl[0x0D], WEP_ARC_PTR(0x0D));
    PSet(pl->pMotTbl[0x0E], WEP_ARC_PTR(0x0E));
    PSet(pl->pMotTbl[0x0F], WEP_ARC_PTR(0x0F));
    PSet(pl->pMotTbl[0x10], WEP_ARC_PTR(0x10));
    PSet(pl->pMotTbl[0x5B], WEP_ARC_PTR(0x30));
    PSet(pl->pMotTbl[0x57], WEP_ARC_PTR(0x31));
    PSet(pl->pMotTbl[0x39], WEP_ARC_PTR(0x19));
    PSet(pl->pMotTbl[0x3A], WEP_ARC_PTR(0x1A));
    PSet(pl->pMotTbl[0x3D], PL_ARC_PTR(pG->pPlArc, 0x5D));
    PSet(pl->pMotTbl[0x41], WEP_ARC_PTR(0x1B));
    PSet(pl->pMotTbl[0x42], WEP_ARC_PTR(0x1C));
    PSet(pl->pMotTbl[0x3F], WEP_ARC_PTR(0x1D));
    PSet(pl->pMotTbl[0x40], WEP_ARC_PTR(0x1E));
    PSet(pl->pMotTbl[0x5D], WEP_ARC_PTR(0x1F));
    PSet(pl->pMotTbl[0x5E], WEP_ARC_PTR(0x20));
    pl->pBody->initWepHand((u32) WEP_ARC_PTR(0xA));
    pl->setRightHand(1);
    pl->setLeftHand(4);
}
