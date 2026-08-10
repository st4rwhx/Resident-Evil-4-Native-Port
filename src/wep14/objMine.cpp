// Mine thrower weapon object (wep14 module, first object; real file name unknown): fires a cEmMine
// (SetMine) along the aim line, ejects a cartridge (SetObj10), reloads by tune level.

#include "wep_mod.h"
#include "atari_init.h"
#include "item.h"
#include "motion.h"
#include "esp.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"
#include "cam_ctrl.h"
#include "em_sub.h"
#include "emmine.h"
#include "math_sub.h"

class cObjMine : public cObjWep {
public:
    virtual ~cObjMine() {}
    virtual void moveReady();
    virtual void moveFire();
    virtual void moveDown();
    virtual void moveReload();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);
    virtual void interrupt();

    void setBullet();
    void setCartridge();
};

void wep14changeRightHand(cPlayer* pl, void* hand);   // wep14/wep14.cpp
void partsSet(cObjMine* obj);

#define PLA_ARC_PTR(no) PL_ARC_PTR(pG->pPlArc, no)

void ObjMine_init(cObj* obj)
{
    new (obj) cObjMine();
}

void cObjMine::init(cModel* parent)
{
    cAtariInfo* at;

    U16Set(wep.x24, 0x36);
    if (modelInit(WEP_ARC_PTR(0x8), WEP_ARC_PTR(0x7)) == 0) {
        pLog->err(0, 0, "cObjMine::init() failed.");
        return;
    }
    at = &sub2B4.atari;
    at->init(1, 0, 0, 0.0f, 100.0f, 0.0f, 0.0f, 100.0f, 100.0f, 100.0f);
    AtariFlagsAnd(at, 0xFCFF);
    pParts->pParent = parent->getPartsPtr(9);
    {
        static const Vec p0 = { 0.0f, 0.0f, 0.0f };
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };

        lightInfo.init2(1, 1, &p0, &p1, 1);
    }
    PSet(wep.parent, parent);
    PSet(wep.pMotNormal, WEP_ARC_PTR(0x21));
    resetMotion();
    setAbility(5.73f, 2.86f, 0.2864f, 0.2864f);
}

// The mine model (parts 1) hangs on the player's right hand (parts 0xA) at the parts origin.
void partsSet(cObjMine* obj)
{
    cModel* p;

    obj->getPartsPtr(1)->pParent = pPL->getPartsPtr(0xA);
    p = obj->getPartsPtr(1);
    p->pos.x = 0.0f;
    p->pos.y = 0.0f;
    p->pos.z = 0.0f;
    p->rot.x = 0.0f;
    p->rot.y = 0.0f;
    p->rot.z = 0.0f;
}

void cObjMine::moveReady()
{
    switch (wep.step) {
    case 0:
        MotionSetCore(this, &mot, WEP_ARC_PTR(0x22), 0, 0, 0, 0);
        getPartsPtr(1)->pParent = pParts;
        wep.step = 1;
    case 1:
        if (MotionGetState(this)) {
            wep.step = 2;
        }
        break;
    case 2:
        partsSet(this);
        MotionSetCore(this, &mot, WEP_ARC_PTR(0x20), 0, 0, 0, 0);
        wep.step = 3;
        break;
    }
}

void cObjMine::moveFire()
{
    if (wep.step == 0) {
        partsSet(this);
        setBullet();
        MotionSetCore(this, &mot, WEP_ARC_PTR(0x1E), 0, 0, 0, 0);
        SndCall(2, 0, &pos, 0, 0, 0);
        BitOn(pG->flags_500C, 0x00800000);
        if (pG->wep_type == 0) {
            EstSet((int) this, -1, 0, 0, 0x48, 0, 0, 0xA, 0, 0);
        }
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0, 1);
        wep.step = 1;
    }
    if (MotionCheckCrossFrame(&mot, 24.0f)) {
        if (pG->wep_type == 0) {
            setCartridge();
        }
    }
    if (MotionGetState(this)) {
        wep.mode = 0;
        wep.step = 0;
    }
}

// Launch the mine: from the hand (normal) or along the camera trajectory towards the aim target
// (scope type), pushed out of any effect collision it starts inside.
void cObjMine::setBullet()
{
    static const Vec mineSpd[2] = { { -1500.0f, 0.0f, 0.0f }, { -600.0f, 0.0f, 0.0f } };
    static Vec minePos;
    static Vec mineAt;
    static Vec mineDir;
    Vec spd;
    Vec hit;
    Vec nrm;
    Vec* pos;
    int normal;

    normal = !(pG->wep_type & 1);
    if (normal) {
        cModel* parts = pPL->getPartsPtr(0xA);

        PSMTXMultVecSR(parts->mat, &mineSpd[pG->wep_x4FB2], &spd);
        pos = &parts->worldPos;
    } else {
        static const Vec mineOfs = { -100.0f, -100.0f, -300.0f };

        CamCtrl.getTrajectory(&minePos, &mineAt);
        GetWepTargetPos(&minePos, &mineAt, 0, 0, 0, 0);
        hit = mineOfs;
        PSMTXMultVecSR(pPL->mat, &hit, &hit);
        PSVECAdd(&minePos, &hit, &minePos);
        pos = &minePos;
        PSVECSubtract(&mineAt, &minePos, &mineDir);
#line 212 "D:/Bio4/Prog/objMine.cpp"
        VECNormalize(&mineDir, &mineDir);
        PSVECScale(&mineDir, &spd, -mineSpd[pG->wep_x4FB2].x);
    }
    if (EatMgr.hitCheck(&pPL->getPartsPtr(0)->worldPos, pos, &hit, &nrm, 0, 0)) {
        PSVECScale(&nrm, &nrm, 500.0f);
        PSVECAdd(&hit, &nrm, pos);
        setPos(pos);
    }
    SetMine(PLA_ARC_PTR(0x72), PLA_ARC_PTR(0x73), pos, &spd, pG->wep_lv == 3);
}

void cObjMine::moveDown()
{
    switch (wep.step) {
    case 0:
        MotionSetCore(this, &mot, WEP_ARC_PTR(0x23), 0, 0, 0, 0);
        getPartsPtr(1)->pParent = pParts;
        wep.step = 1;
        break;
    case 1:
        if (MotionGetState(this)) {
            wep.mode = 0;
            wep.step = 0;
        }
        break;
    }
}

void cObjMine::moveReload()
{
    if (wep.step == 0) {
        void* m;
        int se;

        partsSet(this);
        switch (pG->x4FBA) {
        default:
            m = WEP_ARC_PTR(0x1F);
            break;
        case 1:
            m = WEP_ARC_PTR(0x25);
            break;
        }
        motionSet(m, 0, 0, 1, 0);
        EstSet((int) this, -1, 0, 0, 0x48, 1, 0, 0xA, 0, 0);
        switch (pG->x4FBA) {
        default:
            se = 2;
            break;
        case 1:
            se = 0x20;
            break;
        }
        wep.seHandle = SndCall(2, se, &pParts->worldPos, 0, 0, 0);
        wep.step = 1;
    } else {
        // reload frame (the mine change) by reload tune level
        static const f32 reloadFrame[2] = { 74.0f, 58.0f };

        if (MotionCheckCrossFrame(&mot, reloadFrame[pG->x4FBA])) {
            ItemMgr.reload();
        }
        if (MotionGetState(this)) {
            pParts->pParent = pPL->getPartsPtr(9);
            wep.mode = 0;
            wep.step = 0;
        }
    }
}

void cObjMine::setCartridge()
{
    cModel* parts = getPartsPtr(0);
    Vec pos;
    Vec rot;
    Vec spd;
    cObj* obj;

    pos.x = -348.0f;
    pos.y = -63.0f;
    pos.z = 38.0f;
    PSMTXMultVec(parts->mat, &pos, &pos);
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = PI / 2.0f;
    spd.x = 3.0f;
    spd.y = 60.0f;
    spd.z = 40.0f;
    spd.x += fRand1_1() * 15.0f;
    spd.y += fRand1_1() * 15.0f;
    spd.z += fRand1_1() * 15.0f;
    PSMTXMultVecSR(parts->mat, &spd, &spd);
    obj = SetObj10(PLA_ARC_PTR(0x7A), PLA_ARC_PTR(0x7B), &pos, &rot, &spd, 10.0f, 50.0f, 0x1E, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 1;
    }
}

void cObjMine::interrupt()
{
    cObjWep::interrupt();
    getPartsPtr(1)->pParent = pParts;
    resetMotion();
    wep.mode = 0;
    wep.step = 0;
    wep14changeRightHand(pPL, WEP_ARC_PTR(0x9));
    pPL->setLeftHand(4);
}

void cObjMine::setMotion(cPlayer* pl)
{
    PSet(pl->pMotTbl[0x00], WEP_ARC_PTR(0x0B));
    PSet(pl->pMotTbl[0x01], WEP_ARC_PTR(0x26));
    PSet(pl->pMotTbl[0x02], WEP_ARC_PTR(0x0E));
    PSet(pl->pMotTbl[0x03], WEP_ARC_PTR(0x27));
    PSet(pl->pMotTbl[0x06], WEP_ARC_PTR(0x10));
    PSet(pl->pMotTbl[0x07], WEP_ARC_PTR(0x28));
    PSet(pl->pMotTbl[0x08], WEP_ARC_PTR(0x0F));
    PSet(pl->pMotTbl[0x09], WEP_ARC_PTR(0x29));
    PSet(pl->pMotTbl[0x0B], WEP_ARC_PTR(0x11));
    PSet(pl->pMotTbl[0x0C], WEP_ARC_PTR(0x2A));
    PSet(pl->pMotTbl[0x0D], WEP_ARC_PTR(0x0C));
    PSet(pl->pMotTbl[0x0E], WEP_ARC_PTR(0x2B));
    PSet(pl->pMotTbl[0x0F], WEP_ARC_PTR(0x0D));
    PSet(pl->pMotTbl[0x10], WEP_ARC_PTR(0x2C));
    PSet(pl->pMotTbl[0x39], WEP_ARC_PTR(0x31));
    PSet(pl->pMotTbl[0x3A], WEP_ARC_PTR(0x32));
    PSet(pl->pMotTbl[0x3D], PLA_ARC_PTR(0x5D));
    PSet(pl->pMotTbl[0x41], WEP_ARC_PTR(0x33));
    PSet(pl->pMotTbl[0x42], WEP_ARC_PTR(0x34));
    PSet(pl->pMotTbl[0x3F], WEP_ARC_PTR(0x2F));
    PSet(pl->pMotTbl[0x40], WEP_ARC_PTR(0x30));
    PSet(pl->pMotTbl[0x5B], WEP_ARC_PTR(0x1C));
    PSet(pl->pMotTbl[0x5C], WEP_ARC_PTR(0x2D));
    PSet(pl->pMotTbl[0x57], WEP_ARC_PTR(0x1D));
    PSet(pl->pMotTbl[0x58], WEP_ARC_PTR(0x2E));
}
