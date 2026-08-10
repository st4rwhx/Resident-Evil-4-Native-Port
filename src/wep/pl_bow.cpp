// Bow player routines (wep28 module, first object; real file name unknown): routine 2 of the
// player while Krauser's bow is equipped: ready (draw the arrow), set (idle / turn), fire (shoot),
// down. Modelled on game/pl_knife.cpp.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "pl_wep.h"
#include "wep_mod.h"
#include "global.h"
#include "main.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "snd.h"
#include "math_sub.h"

// motion.h declares the one-argument MotionMove; the routines pass a second argument (pl_knife.cpp).
int MotionMoveI(cModel* m, int flag) asm("MotionMove");

#define WEP_ARC_PTR(no) PL_ARC_PTR((PlArc*) pG->pWepArc, no)
#define BOW(pl) ((cObjBow*) (pl)->pWep->pObj)

// Routine bytes through int parameters (player.cpp PlRoutineSet).
static inline void PlRoutineSet(cPlayer* pl, int r0, int r1, int r2, int r3)
{
    pl->xFC = r0;
    pl->xFD = r1;
    pl->xFE = r2;
    pl->xFF = r3;
}

static void wep28_r2_ready(cPlayer* pl);
static void wep28_r3_ready00(cPlayer* pl);
static void wep28_r3_ready10(cPlayer* pl);
static void wep28_r3_ready20(cPlayer* pl);
static void wep28_r2_set(cPlayer* pl);
static void wep28_r3_set00(cPlayer* pl);
static void wep28_r3_set10(cPlayer* pl);
static void wep28_r3_set20(cPlayer* pl);
static void wep28_r3_set30(cPlayer* pl);
static void wep28_r3_set40(cPlayer* pl);
static void wep28_r2_fire(cPlayer* pl);
static void wep28_r3_fire00(cPlayer* pl);
static void wep28_r3_fire10(cPlayer* pl);
static void wepDown(cPlayer* pl);

void PlBowMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep28_r2_ready,
        wep28_r2_set,
        wep28_r2_fire,
        wepDown,
        0,
    };

    func_tbl[pl->xFE](pl);
}

static void wep28_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep28_r3_ready00,
        wep28_r3_ready10,
        wep28_r3_ready20,
    };

    func_tbl[pl->xFF](pl);
    if (joyKamae() == 0) {
        if (pl->flags_420 & 0x40) {
            pl->xFC = 0;
            pl->xFE = 0;
            pl->xFD = 0x11;
            pl->xFF = 0;
        } else {
            // 0xD and 6 share one register (`li r9,0xd; stb ff; li r9,6; stb fd`): one int local
            // assigned twice, not two constants.
            int no = 0xD;

            pl->xFF = no;
            pl->xFC = 0;
            pl->xFE = 3;
            no = 6;
            pl->xFD = no;
        }
    } else {
        Vec aim = {0.0f, 1000.0f, 10000.0f};
        Vec hit;

        PSMTXMultVec(pl->mat, &aim, &aim);
        SatMgr.hitCheck(&pl->getPartsPtr(0)->worldPos, &aim, &hit, 0, 0, 0);
        CamCtrlShoulderSetAim(&hit);
    }
}

static void wep28_r3_ready00(cPlayer* pl)
{
    f32 pitch;
    void* mot;
    cObjWep* obj;
    int hokan;

    pl->x3E4 = 0;
    pl->pWep->x2C = 0.0f;
    pitch = CamCtrl.getCameraPitch();
    if (pitch > 0.0f) {
        pitch += pitch;
    }
    pl->pWep->pitch = pitch;
    pitch *= 2.0f / PI;
    m3r[1] = pitch;
    m3r[0] = pitch;
    m3r[2] = 0.0f;
    pl->x400 = 0.0f;
    pl->pNeck->init(0, 0, 0);
    pl->pWep->x30 = CamCtrl.getCameraDirection();
    obj = pl->pWep->pObj;
    obj->wep.mode = 1;
    obj->wep.step = 0;
    hokan = 4;
    if (!(pl->flags_420 & 0x40)) {
        hokan = 5;
    }
    mot = WEP_ARC_PTR(0x1F);
    mot3.set(pl, mot, mot, mot, 0, 3, 0, hokan, 0);
    mot3.move(m3r[0]);
    pl->xFF = 1;
}

static void wep28_r3_ready10(cPlayer* pl)
{
    if (pl->frame < 4.0f) {
        f32 d = pl->pWep->x30 / (4.0f - pl->frame);

        pl->rot.y += d;
        pl->pWep->x30 -= d;
    }
    if (MotionCheckCrossFrame(&pl->mot, 4.0f)) {
        pl->pWep->pObj2->setDisp(1, 1);
        pl->setRightHand(1);
    } else if (MotionCheckCrossFrame(&pl->mot, 11.0f)) {
        pl->pWep->pObj2->setDisp(1, 0);
        BOW(pl)->setDispAllow(1);
    }
    if (pl->motionMove()) {
        PlRoutineSet(pl, 0, 6, 1, 0);
    }
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    pl->pWaist->set(0.0f, 0.4f);
}

static void wep28_r3_ready20(cPlayer* pl)
{
    if (MotionMoveI(pl, 0)) {
        PlRoutineSet(pl, 0, 6, 1, 0);
    }
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    pl->pWaist->set(0.0f, 0.4f);
}

static void wep28_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep28_r3_set00,
        wep28_r3_set10,
        wep28_r3_set20,
        wep28_r3_set30,
        wep28_r3_set40,
    };

    func_tbl[pl->xFF](pl);
    pl->setLaserSight(1, 0);
    PlWepLockCtrl(pl);
    if (joyKamae() == 0) {
        if (pl->flags_420 & 0x40) {
            pl->xFC = 0;
            pl->xFE = 0;
            pl->xFD = 0x11;
            pl->xFF = 0;
        } else {
            wepDown(pl);
        }
    } else if (joyFireTrg()) {
        if (pl->pWep->pObj->bulletNum()) {
            PlRoutineSet(pl, 0, 6, 2, 0);
        }
    } else if (joyFireOn() && pl->pWep->pObj->bulletNum()) {
        PlRoutineSet(pl, 0, 6, 2, 0);
    }
}

static void wep28_r3_set00(cPlayer* pl)
{
    PlArc* arc = (PlArc*) pG->pWepArc;

    mot3.set(pl, PL_ARC_PTR(arc, 0x21), PL_ARC_PTR(arc, 0x24), PL_ARC_PTR(arc, 0x27), 0, 3, 0, 4, 0);
    mot3.move(m3r[0]);
    pl->motionMove();
    pl->xFF = 1;
}

static void wep28_r3_set10(cPlayer* pl)
{
    MotionMoveI(pl, 0);
}

static void wep28_r3_set20(cPlayer* pl)
{
    if ((Key.on & 4) == 0) {
        pl->xFF = 0;
    }
    MotionMoveI(pl, 0);
    if (pl->frame > 9.7f && pl->frame < 10.3f) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->worldPos, 0, 0, 0);
    }
    if (pl->frame > 22.7f && pl->frame < 23.3f) {
        SndCall(5, 1, &pl->getPartsPtr(0x18)->worldPos, 0, 0, 0);
    }
}

static void wep28_r3_set30(cPlayer* pl)
{
    if ((Key.on & 8) == 0) {
        pl->xFF = 0;
    }
    MotionMoveI(pl, 0);
    if (pl->frame > 9.7f && pl->frame < 10.3f) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->worldPos, 0, 0, 0);
    }
    if (pl->frame > 22.7f && pl->frame < 23.3f) {
        SndCall(5, 1, &pl->getPartsPtr(0x18)->worldPos, 0, 0, 0);
    }
}

static void wep28_r3_set40(cPlayer* pl)
{
    if (pl->motionMove() || (Key.on & 0x10F)) {
        pl->xFF = 0;
    }
}

static void wep28_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep28_r3_fire00,
        wep28_r3_fire10,
        0,
    };

    func_tbl[pl->xFF](pl);
}

static void wep28_r3_fire00(cPlayer* pl)
{
    PlArc* arc;
    f32 pitch;
    cObjWep* obj;

    pl->pWep->pObj->trigger();
    arc = (PlArc*) pG->pWepArc;
    mot3.set(pl, PL_ARC_PTR(arc, 0x22), PL_ARC_PTR(arc, 0x25), PL_ARC_PTR(arc, 0x28), 0, 0, 0, 4, 0);
    mot3.move(m3r[0]);
    MotionMoveI(pl, 0);
    pl->x3F4 = 1;
    pl->x3F0 = 1;
    obj = pl->pWep->pObj;
    obj->wep.mode = 2;
    obj->wep.step = 0;
    pl->setRightHand(0);
    pitch = m3r[0];
    PlWepLockRand(pl, 2, &pitch, &pl->x400);
    m3r[1] = pitch;
    if (m3r[2] == 0.0f) {
        m3r[0] = pitch;
    }
    pl->xFF = 1;
}

static void wep28_r3_fire10(cPlayer* pl)
{
    int endFrame = 5;

    PlWepLockCtrl(pl);
    if (MotionCheckCrossFrame(&pl->mot, 16.0f)) {
        pl->pWep->pObj2->setDisp(1, 1);
        pl->setRightHand(1);
    } else if (MotionCheckCrossFrame(&pl->mot, 35.0f)) {
        pl->pWep->pObj2->setDisp(1, 0);
        BOW(pl)->setDispAllow(1);
    }
    if (pl->motionMove()) {
        PlRoutineSet(pl, 0, 6, 1, 0);
    } else if (pl->frame >= (f32) endFrame && joyKamae() == 0) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 3;
        pl->xFF = 0xD;
    }
}

static void wepDown(cPlayer* pl)
{
    cObjWep* obj;

    pl->setRightHand(0);
    pl->pWep->pObj2->setDisp(1, 0);
    obj = pl->pWep->pObj;
    obj->wep.mode = 3;
    obj->wep.step = 0;
    if (dmMotCk()) {
        pl->motionSet(WEP_ARC_PTR(0x20), 3, pl->xFF, 1, 0);
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
}
