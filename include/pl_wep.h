#ifndef PL_WEP_H
#define PL_WEP_H

#include "types.h"
#include "vec.h"
#include "model.h"
#include "obj.h"
#include "main.h"

class cPlayer;

// Player weapon object (game/objWep.cpp): a cObj whose work area holds ObjWepWork (`wep`, obj.h).
// The vtable order is objWep's `cObjWep virtual table`; the in-class bodies are the ones the
// original emits after the destructor (objWep owns the vtable, so every in-class inline is
// emitted there: add none that the target lacks).
class cObjWep : public cObj {
public:
    cObjWep();
    virtual ~cObjWep() {}
    virtual void move();
    virtual void moveAll() {}
    virtual void moveStay() {}
    virtual void moveReady() {}
    virtual void moveFire() {}
    virtual void moveDown() {}
    virtual void moveReload() {}
    virtual void moveDrop() {}
    virtual void init(cModel* parent) { setAbility(5.73f, 2.86f, 0.2864f, 0.2864f); }
    virtual void setMotion(cPlayer* pl) {}    // pl_sub PlReloadBullet: the launcher fills the player's motion table
    virtual void interrupt();
    virtual void endReload(int noReload);
    void setAbility(f32 pitch, f32 yaw, f32 pitchStep, f32 yawStep) {
        wep.lockRandPitch = pitch * 0.017453292f;
        wep.lockRandYaw = yaw * 0.017453292f;
        wep.lockRandPitchStep = pitchStep * 0.017453292f;
        wep.lockRandYawStep = yawStep * 0.017453292f;
    }
    virtual int keyKamae() { return (Key.on >> 4) & 1; }   // pl_sub joyKamae
    virtual void fire() {}
    virtual void beginReload() {}

    void setDisp(int type, int on);
    void parentSet(cModel* parent, int partsNo, Vec* pos, Vec* rot);
    void parentRelease();
    void resetMotion();
    void trigger();
    int bulletNum();
    int reloadable();
    void drawLaserSight(int draw, int noCalc);
    void getMarkerPos(Vec* pos, Vec* at);
    void satCheck();
};

// Rocket (game/objRocket.cpp): hangs on the launcher, flies with its motion and explodes on the
// scenario / water / player weapon target line (`rocket`, obj.h).
class cObjRocket : public cObj {
public:
    virtual ~cObjRocket() {}
    virtual void beginEvent();
    virtual void move();

    void init();
    void fire();

    static const Vec lightPos;   // light set origin / range shared with the launcher (objRocket.cpp)
    static const Vec lightSize;
};

// Rocket launcher (game/objRocket.cpp): carries a cObjRocket (`launcher`, obj.h) it launches.
class cObjLauncher : public cObjWep {
public:
    cObjLauncher();
    virtual ~cObjLauncher();
    virtual void setNoSuspend(int on) {
        if (on) {
            be_flag |= 0x800;
        } else {
            be_flag &= ~0x800;
        }
        if (launcher.rocket) {
            launcher.rocket->setNoSuspend(on);
        }
    }
    virtual void moveFire();
    virtual void moveDrop();
    virtual void init(cModel* parent);
    virtual void setMotion(cPlayer* pl);
    virtual void interrupt();
    virtual int keyKamae();

    void loadRocket();
    int ckBoss();
    void launch();
    void drop(int se);
    void grip(int a);
    void gripBack();
};

// Player weapon control (game/pl_wep.cpp), 0x44 bytes at cEm::pWep.
class cPlWep {
public:
    u8 pad_0[0x20];
    u8 x20;              // 0x20  (ctor: 0)
    u8 x21;              // 0x21  (wep07 pl_shotgun reload: 0)
    u8 pad_22;
    u8 x23;              // 0x23  (wep07 ready00: 0)
    u8 knifeStance;      // 0x24  knife ready stance: 0 low, 1 middle, 2 high
    u8 pad_25;
    u8 x26;              // 0x26  bit0: reload requested by the routine (wep11 pl_machine)
    u8 pad_27;
    f32 pitch;           // 0x28  aim pitch
    f32 x2C;             // 0x2C
    f32 x30;             // 0x30  camera direction at the ready start (wep13 pl_rocket: the player turns to it over ready10's first frames)
    cObjWep* pObj;       // 0x34  weapon object (cObjLauncher for the rocket launcher)
    cObjWep* pObj2;      // 0x38  second weapon object (rifles / launchers display part)
    u8 pad_3C[4];
    u8 x40;              // 0x40  lock frames left (lockInit/lockNext: 10; lockMove clears it on a stick move)
    u8 pad_41[3];

    cPlWep();
    f32 getAngle();
    f32 getPitch();
    void move();
    int getMarkerPos(Vec* out);
    cModel* lockInit();
    void lockMove();
    cModel* lockNext();
    void setTrans(int on, int type);   // pObj/pObj2 display by weapon (pl_sub PlSetHand)
};

// knife/weapon collision (pl, top, bottom, type, flags, length)
u32 PlWepHitCheck2(cModel* pl, Vec* p0, Vec* p1, int type, u32 flag, f32 len);
void PlWepLockCtrl(cModel* pl);

extern "C" {
u32 PlWepHitCheck3(Vec* pos, int type, u32 prio, f32 len);
void PlWepAutoTrack(cModel* pl, int mode, f32 rate);
void PlWepLockRandInit();
void PlWepLockRand(cModel* pl, int flag, f32* pitch, f32* yaw);
void PlSetLockPitch(cModel* pl);
int GetWepSizeGroup(int no);
int PlCornerCheck();
cModel* SearchLockEm(Vec* pos, cModel* skip);
cModel* SearchTargetEm(Vec* pos, cModel* skip, f32 range);
}

extern u8 lockCtr;
extern void (*WeaponInitFunc)(cModel*);

#endif
