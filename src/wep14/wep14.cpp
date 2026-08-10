// wep14 module: the mine thrower (weapon number 0x14, cObjMine object id 0x36; wep14/objMine.cpp).
// The module has its own player routine (the handgun routine of wep/pl_handgun.cpp with the
// mine thrower's aim types: type 0 fires from the hand, the scope type (wep_type bit0) aims
// through the camera trajectory and swaps the right hand model between the ready/reload motions).

#include "wep_mod.h"
#include "light.h"
#include "esp.h"
#include "main.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "motion.h"
#include "pad.h"
#include "snd.h"
#include "math_sub.h"

extern "C" {
f64 atan2(f64 y, f64 x);
// motion.h declares the one-argument MotionMove; the routines pass a second argument (pl_knife.cpp).
int MotionMoveI(cModel* m, int flag) asm("MotionMove");
}

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)
#define WEP_OBJ(pl) ((pl)->pWep->pObj)
// The weapon object's own cAtariInfo (the object's collision with enemies while it is held).
#define WEP_ATARI(pl) (&WEP_OBJ(pl)->sub2B4.atari)

// Routine bytes through int parameters (player.cpp PlRoutineSet).
static inline void PlRoutineSet(cPlayer* pl, int r0, int r1, int r2, int r3)
{
    pl->xFC = r0;
    pl->xFD = r1;
    pl->xFE = r2;
    pl->xFF = r3;
}

// Scalar-reference stores (the following pG load stays below them).
static inline void U32Set(u32& d, u32 v) { d = v; }
static inline void IntSet(int& d, int v) { d = v; }

void ObjMine_init(cObj* obj);   // wep14/objMine.cpp

void Wep14_move(cPlayer* pl);
cObjWep* equipWeapon(cPlayer* pl);
void wep14changeRightHand(cPlayer* pl, void* hand);
static void wep14_r2_ready(cPlayer* pl);
static void wep14_r3_ready00(cPlayer* pl);
static void wep14_r3_ready10(cPlayer* pl);
static void wep14_r3_ready20(cPlayer* pl);
static void wep14_r3_ready30(cPlayer* pl);
static void wep14_r2_set(cPlayer* pl);
static void wep14_r3_set00(cPlayer* pl);
static void wep14_r3_set10(cPlayer* pl);
static void wep14_r3_set20(cPlayer* pl);
static void wep14_r3_set30(cPlayer* pl);
static void wep14_r3_set40(cPlayer* pl);
static void wep14_r2_fire(cPlayer* pl);
static void wep14_r3_fire00(cPlayer* pl);
static void wep14_r3_fire10(cPlayer* pl);
static void wep14_r2_down(cPlayer* pl);
static void wep14_r2_reload(cPlayer* pl);
static void wep14_r2_next(cPlayer* pl);

u8 lockCtr = 0;

// ready30 (the turn towards the lock target): positions the player is pulled to / turned to.
static Vec pos;
static Vec tgt;

void Wep14_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep14_init() wep model init failed.");
    } else {
        pl->pWep->pObj = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x6), 0x48, 1);
        wep14changeRightHand(pl, WEP_ARC_PTR(0x9));
        pl->setLeftHand(4);
        PlWepMot[0] = WEP_ARC_PTR(0x13);
        PlWepMot[1] = WEP_ARC_PTR(0x17);
        PlWepMot[2] = WEP_ARC_PTR(0x19);
    }
}

void Wep14_move(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep14_r2_ready,
        wep14_r2_set,
        wep14_r2_fire,
        wep14_r2_down,
        wep14_r2_reload,
        wep14_r2_next,
    };

    func_tbl[pl->xFE](pl);
    pl->pWep->lockMove();
}

static void wep14_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep14_r3_ready00,
        wep14_r3_ready10,
        wep14_r3_ready20,
        wep14_r3_ready30,
    };

    func_tbl[pl->xFF](pl);
    if (joyKamae() == 0 && pl->xFF != 3) {
        if (pl->flags_420 & 0x40) {
            pl->xFC = 0;
            pl->xFE = 0;
            pl->xFD = 0x11;
            pl->xFF = 0;
        } else {
            cObjWep* obj;

            pl->xFC = 0;
            pl->xFD = 0;
            pl->xFE = 0;
            pl->xFF = 0;
            obj = WEP_OBJ(pl);
            obj->wep.mode = 3;
            obj->wep.step = 0;
        }
        wep14changeRightHand(pl, WEP_ARC_PTR(0x9));
    }
    if (pl->keyReload() && WEP_OBJ(pl)->reloadable()) {
        wep14changeRightHand(pl, WEP_ARC_PTR(0x9));
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 4;
        pl->xFF = 0;
        pl->x3E0 = 1;
    } else if (pl->pLockEm) {
        CamCtrlShoulderSetAim(&pl->pLockEm->pos);
    } else {
        Vec aim = {0.0f, 1000.0f, 10000.0f};
        Vec hit;

        PSMTXMultVec(pl->mat, &aim, &aim);
        SatMgr.hitCheck(&pl->getPartsPtr(0)->worldPos, &aim, &hit, 0, 0, 0);
        CamCtrlShoulderSetAim(&hit);
    }
}

static void wep14_r3_ready00(cPlayer* pl)
{
    const f32 zero = 0.0f;
    cObjWep* obj;
    f32 pitch;
    void* m;
    int normal;
    int hokan;

    pl->x3E4 = 0;
    pl->pWep->x2C = zero;
    pitch = CamCtrl.getCameraPitch();
    if (pitch > zero) {
        pitch += pitch;
    }
    pl->pWep->pitch = pitch;
    m3r[2] = zero;
    pitch *= 2.0f / PI;
    m3r[1] = pitch;
    m3r[0] = pitch;
    pl->x400 = zero;
    FSet(pl->pWep->x30, CamCtrl.getCameraDirection());
    wep14changeRightHand(pl, WEP_ARC_PTR(0xA));
    pl->pNeck->init(0, 0, 0);
    pl->pWep->lockInit();
    hokan = 4;
    normal = !(pG->wep_type & 1);
    if (normal) {
        hokan = 0x104;
    }
    m = WEP_ARC_PTR(0x12);
    mot3.set(pl, m, m, m, 0, 0, 0, hokan, 0);
    mot3.move(m3r[0]);
    pl->motionMove();
    m3r[0] = zero;
    lockCtr = 0;
    m3r[1] = zero;
    m3r[2] = zero;
    obj = WEP_OBJ(pl);
    obj->wep.step = 0;
    obj->wep.mode = 1;
    pl->xFF = 1;
}

static void wep14_r3_ready10(cPlayer* pl)
{
    if (pl->frame < 4.0f) {
        f32 d = pl->pWep->x30 / (4.0f - pl->frame);

        pl->rot.y += d;
        pl->pWep->x30 -= d;
    }
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    pl->pWaist->set(0.0f, 0.4f);
    if (pl->motionMove()) {
        SndCall(2, 9, &pl->getPartsPtr(0xA)->worldPos, 0, 0, 0);
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 1;
        pl->xFF = 0;
    }
}

static void wep14_r3_ready20(cPlayer* pl)
{
    if (pl->motionMove()) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->worldPos, 0, 0, 0);
        PlRoutineSet(pl, 0, 6, 1, 0);
    }
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    pl->pWaist->set(0.0f, 0.4f);
}

// ready30: turn / step towards the aim target while the motion plays (wep/pl_handgun.cpp).
static void wep14_r3_ready30(cPlayer* pl)
{
    f32 dist;
    f32 x;
    f64 a;
    f32* r;
    Vec* t;

    if (pl->motionMove()) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->worldPos, 0, 0, 0);
        PlRoutineSet(pl, 0, 6, 1, 0);
    }
    pl->rot.y += Muku(&pl->pos, &tgt, pl->rot.y, PI / 8.0f);
    pl->pos.x = pl->pos.x * 0.6f + pos.x * 0.4f;
    pl->pos.z = pl->pos.z * 0.6f + pos.z * 0.4f;
    t = &tgt;
    dist = GetDistance3(&pos, t);
    a = atan2(t->y - pos.y, dist);
    r = m3r;
    x = a / (PI / 4.0f) - r[0];
    if (x > 0.05f) {
        x = 0.05f;
    }
    if (x < -0.05f) {
        x = -0.05f;
    }
    r[1] += x;
    if (r[2] == 0.0f) {
        m3r[0] = r[1];
    }
    {
        f32 lo = -1.0f;
        f32 hi = 1.0f;

        if (r[1] < lo) {
            r[1] = lo;
        } else if (r[1] > hi) {
            r[1] = hi;
        }
    }
    if (r[2] == 0.0f) {
        r[0] = r[1];
    }
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    pl->pWaist->set(0.0f, 0.4f);
}

static void wep14_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep14_r3_set00,
        wep14_r3_set10,
        wep14_r3_set20,
        wep14_r3_set30,
        wep14_r3_set40,
    };
    int fire;
    int normal;

    func_tbl[pl->xFF](pl);
    normal = !(pG->wep_type & 1);
    if (normal) {
        pl->setLaserSight(1, 0);
    } else {
        pl->setLaserSight(0, 0);
    }
    if (joyKamae() == 0) {
        if (pl->flags_420 & 0x40) {
            pl->xFC = 0;
            pl->xFE = 0;
            pl->xFD = 0x11;
            pl->xFF = 0;
        } else {
            int md = 3;

            PlRoutineSet(pl, 0, 6, md, 0);
        }
    } else if ((fire = joyFireTrg())) {
        fire = WEP_OBJ(pl)->bulletNum();
        if (fire) {
            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = 2;
            pl->xFF = 0;
        } else if (WEP_OBJ(pl)->reloadable()) {
            pl->pWep->x26 |= 1;
            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = 4;
            pl->xFF = 0;
            pl->x3E0 = fire;
        } else {
            SndCall(2, 3, &pl->getPartsPtr(4)->worldPos, 0, 0, 0);
            goto reload;
        }
    } else if (joyFireOn() && WEP_OBJ(pl)->bulletNum()) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 2;
        pl->xFF = 0;
    } else {
    reload:
        if (pl->keyReload() && WEP_OBJ(pl)->reloadable()) {
            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = 4;
            pl->xFF = 0;
            pl->x3E0 = 1;
        }
    }
}

static void wep14_r3_set00(cPlayer* pl)
{
    PlArc* arc;

    if (pG->wep_type & 1) {
        CamCtrl.startScope(0, 0);
        CameraMove();
        pl->flags_420 |= 0x10;
    }
    arc = (PlArc*) pG->pWepArc;
    mot3.set(pl, PL_ARC_PTR(arc, 0x13), PL_ARC_PTR(arc, 0x17), PL_ARC_PTR(arc, 0x19), 0, 3, 0, 4, 0);
    mot3.move(m3r[0]);
    pl->motionMove();
    pl->xFF = 1;
}

static void wep14_r3_set10(cPlayer* pl)
{
    PlWepLockCtrl(pl);
    pl->motionMove();
}

static void wep14_r3_set20(cPlayer* pl)
{
    if ((Key.on & 4) == 0) {
        pl->xFF = 0;
    }
    MotionMoveI(pl, 0);
}

static void wep14_r3_set30(cPlayer* pl)
{
    if ((Key.on & 8) == 0) {
        pl->xFF = 0;
    }
    pl->motionMove();
}

static void wep14_r3_set40(cPlayer* pl)
{
    if (MotionMoveI(pl, 0) || (Key.on & 0x10F)) {
        pl->xFF = 0;
    }
}

static void wep14_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep14_r3_fire00,
        wep14_r3_fire10,
    };

    func_tbl[pl->xFF](pl);
}

static void wep14_r3_fire00(cPlayer* pl)
{
    PlArc* arc;
    cObjWep* obj;
    f32 pitch;

    WEP_OBJ(pl)->trigger();
    arc = (PlArc*) pG->pWepArc;
    mot3.set(pl, PL_ARC_PTR(arc, 0x14), PL_ARC_PTR(arc, 0x18), PL_ARC_PTR(arc, 0x1A), 0, 0, 0, 4, 0);
    mot3.move(m3r[0]);
    pl->motionMove();
    obj = WEP_OBJ(pl);
    obj->wep.mode = 2;
    obj->wep.step = 0;
    pitch = m3r[0];
    PlWepLockRand(pl, 2, &pitch, &pl->x400);
    m3r[1] = pitch;
    if (m3r[2] == 0.0f) {
        m3r[0] = pitch;
    }
    pl->xFF = 1;
    pl->flags_420 &= ~0x20;
}

// fire10: the hand model is swapped for the throw frames (setRightHand 5 / 4).
static void wep14_r3_fire10(cPlayer* pl)
{
    if (MotionCheckCrossFrame(&pl->mot, 23.0f)) {
        SndCall(2, 4, &pl->getPartsPtr(4)->worldPos, 0, 0, 0);
        pl->flags_420 |= 0x20;
        pl->setLeftHand(5);
    }
    if (MotionCheckCrossFrame(&pl->mot, 30.0f)) {
        pl->flags_420 &= ~0x20;
        pl->setLeftHand(4);
    }
    if (pl->motionMove()) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 1;
        pl->xFF = 0;
    }
}

static void wep14_r2_down(cPlayer* pl)
{
    cObjWep* obj;

    if (pG->wep_type & 1) {
        CamCtrl.endScope();
        CameraMove();
        pl->flags_420 &= ~0x10;
    }
    if (dmMotCk()) {
        pl->motionSet(WEP_ARC_PTR(0x15), 7, 0, 1, 0);
        PlRoutineSet(pl, 0, 0, 2, 0);
    } else {
        pl->xFF = 1;
        pl->x4FD = 0xF;
        pl->xFC = 0;
        pl->xFD = 0;
        pl->xFE = 0;
        pl->x4FC = 0;
    }
    pl->motionMove();
    {
        int md = 3;

        obj = WEP_OBJ(pl);
        obj->wep.mode = md;
        obj->wep.step = 0;
    }
    AtariFlagsAndV(WEP_ATARI(pl), 0xFDFF);
    wep14changeRightHand(pl, WEP_ARC_PTR(0x9));
    FSet(pl->rot.y, pl->rot.y - pl->pWaist->set(0.0f, 0.4f));
}

static void wep14_r2_reload(cPlayer* pl)
{
    u8 step = pl->xFF;
    cObjWep* obj;
    void* mot;

    switch (step) {
    case 0:
        if (pG->wep_type & 1) {
            pl->endCamera();
        }
        switch (pG->x4FBA) {
        default:
            mot = WEP_ARC_PTR(0x16);
            break;
        case 1:
            mot = WEP_ARC_PTR(0x1B);
            break;
        }
        MotionSetCore(pl, &pl->mot, mot, 0, 3, 5, 0);
        wep14changeRightHand(pl, WEP_ARC_PTR(0xA));
        pl->xFF = 1;
        obj = WEP_OBJ(pl);
        obj->wep.mode = 4;
        obj->wep.step = 0;
    case 1:
        if (pl->motionMove()) {
            if (pG->wep_type & 1) {
                CamCtrl.startScope(0, 0);
                CameraMove();
                pl->flags_420 |= 0x10;
            }
            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = 1;
            pl->xFF = 0;
        }
        break;
    }
}

// next: turn to the lock target (pLockEm) with the turn motion.
static void wep14_r2_next(cPlayer* pl)
{
    u8 step = pl->xFF;
    cModel* em = pl->pLockEm;

    switch (step) {
    case 0:
        U32Set(pl->x3E0, 0);
        IntSet(pl->x3E4, 0);
        MotionSetCore(pl, &pl->mot, WEP_ARC_PTR(0x12), 0, 0xA, 1, 0);
        pl->xFF = 1;
    case 1:
        if (GetDistance3(&pl->pos, &em->pos) > 200.0f) {
            pl->rot.y += Muku(&pl->pos, &em->pos, pl->rot.y, 0.31415927f);
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        }
        pl->x400 = 0.0f;
        pl->pWaist->set(0.0f, 0.4f);
        pl->pBody->waistMove();
        pl->motionMove();
        if ((int) pl->x3E0++ > 9) {
            PlRoutineSet(pl, 0, 6, 1, 0);
        }
        break;
    }
    if (Key.trg & 0x20) {
        if (pl->pWep->lockNext()) {
            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = 5;
            pl->xFF = 0;
        } else {
            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = 1;
            pl->xFF = 0;
        }
    } else if (joyKamae() == 0) {
        if (pl->flags_420 & 0x40) {
            pl->xFC = 0;
            pl->xFE = 0;
            pl->xFD = 0x11;
            pl->xFF = 0;
        } else {
            int md = 1;

            PlRoutineSet(pl, 0, 6, md, 0);
        }
    }
}

cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj;

    obj = (cObjWep*) ObjMgr.createBack(0x36);
    if (obj == 0) {
        pLog->err(0, 0, "Wep14_init() cObjWep CREATE FAILED");
        return 0;
    }
    obj->init(pl);
    return obj;
}

// Right hand model swap: the mine thrower's hand data by motion (ready / reload).
void wep14changeRightHand(cPlayer* pl, void* hand)
{
    pl->setRightHand(0);
    pl->pBody->initWepHand((u32) hand);
    pl->setRightHand(1);
}

extern "C" void _prolog()
{
    WeaponInitFunc = Wep14_init;
    WeaponMoveFunc = Wep14_move;
    ObjInitFunc[0x36] = ObjMine_init;
    OSReport("Wep14 MINE-THROWER prolog Ok\n");
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

// The module's .data section is 8-aligned (the original linker's placement; the tables start at 4).
asm(".section .data\n\t.balign 8\n\t.text");
