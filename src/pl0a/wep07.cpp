// pl0a module, first object: Krauser's build of the shotgun player routines (wep07: ready / set / fire,
// no PlShotgunMove / set20 / set30 / set40 / reload — the routine table keeps their empty slots).
// Real file name unknown (the weapon modules' shared routine object).

#include "atari.h"
#include "light.h"
#include "pl_mod.h"
#include "cam_ctrl.h"
#include "motion.h"

extern "C" {
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");   // motion.h declares the one-argument form
}

static void wep07_r2_ready(cPlayer* pl);
static void wep07_r3_ready00(cPlayer* pl);
static void wep07_r3_ready10(cPlayer* pl);
static void wep07_r3_ready20(cPlayer* pl);
static void wep07_r2_set(cPlayer* pl);
static void wep07_r3_set00(cPlayer* pl);
static void wep07_r3_set10(cPlayer* pl);
static void wep07_r2_fire(cPlayer* pl);
static void wep07_r3_fire00(cPlayer* pl);
static void wep07_r3_fire10(cPlayer* pl);
void wepDown(cPlayer* pl);

// Routine 2 table of the full shotgun build (PlShotgunMove indexes it); unreferenced here.
static void (*wep07_func_tbl[5])(cPlayer*) = {
    wep07_r2_ready,
    wep07_r2_set,
    wep07_r2_fire,
    0,
    0,
};

static void wep07_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep07_r3_ready00,
        wep07_r3_ready10,
        wep07_r3_ready20,
    };

    func_tbl[pl->xFF](pl);
    if (joyKamae() == 0) {
        pl->xFC = 0;
        pl->xFD = 0;
        pl->xFE = 0;
        pl->xFF = 0;
    } else {
        Vec aim = {0.0f, 1000.0f, 10000.0f};
        Vec hit;

        PSMTXMultVec(pl->mat, &aim, &aim);
        SatMgr.hitCheck(&pl->getPartsPtr(0)->worldPos, &aim, &hit, 0, 0, 0);
        CamCtrlShoulderSetAim(&hit);
    }
}

static void wep07_r3_ready00(cPlayer* pl)
{
    f32 pitch;
    void* mot;
    int hokan;

    pl->x3E4 = 0;
    pl->pWep->x23 = 0;
    pl->pWep->x2C = 0.0f;
    pitch = CamCtrl.getCameraPitch();
    if (pitch > 0.0f) {
        pitch += pitch;
    }
    pl->pWep->pitch = pitch;
    pitch *= 2.0f / PI;
    m3r[1] = pitch;
    m3r[2] = 0.0f;
    m3r[0] = pitch;
    pl->x400 = 0.0f;
    pl->pNeck->init(0, 0, 0);
    pl->pWep->x30 = CamCtrl.getCameraDirection();
    hokan = 4;
    if (!(pl->flags_420 & 0x40)) {
        hokan = 5;
    }
    mot = PL_ARC(0x8B);
    mot3.set(pl, mot, mot, mot, 0, 3, 0, hokan, 0);
    mot3.move(m3r[0]);
    pl->xFF = 1;
}

static void wep07_r3_ready10(cPlayer* pl)
{
    if (pl->frame >= 5.0f) {
        PlWepLockCtrl(pl);
    }
    if (pl->motionMove()) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 1;
        pl->xFF = 0;
    }
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    pl->pWaist->set(0.0f, 0.4f);
}

static void wep07_r3_ready20(cPlayer* pl)
{
    if (MotionMoveF(pl, 0)) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 1;
        pl->xFF = 0;
    }
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    pl->pWaist->set(0.0f, 0.4f);
}

static void wep07_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep07_r3_set00,
        wep07_r3_set10,
    };

    func_tbl[pl->xFF](pl);
    PlWepLockCtrl(pl);
    if (joyKamae() == 0) {
        wepDown(pl);
    } else if (joyFireOn()) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 2;
        pl->xFF = 0;
    }
}

static void wep07_r3_set00(cPlayer* pl)
{
    PlArc* arc = pG->pPlArc;

    mot3.set(pl, PL_ARC_PTR(arc, 0x8D), PL_ARC_PTR(arc, 0x8F), PL_ARC_PTR(arc, 0x4A), 0, 3, 0, 4, 0);
    mot3.move(m3r[0]);
    pl->motionMove();
    pl->xFF = 1;
}

static void wep07_r3_set10(cPlayer* pl)
{
    pl->motionMove();
}

static void wep07_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep07_r3_fire00,
        wep07_r3_fire10,
    };

    func_tbl[pl->xFF](pl);
}

static void wep07_r3_fire00(cPlayer* pl)
{
    PlArc* arc = pG->pPlArc;

    mot3.set(pl, PL_ARC_PTR(arc, 0x8E), PL_ARC_PTR(arc, 0x90), PL_ARC_PTR(arc, 0x4B), 0, 0, 0, 4, 0);
    mot3.move(m3r[0]);
    MotionMoveF(pl, 0);
    pl->pBody->waistMove();
    pl->partsWorldCalc();
    pl->xFF = 1;
}

static void wep07_r3_fire10(cPlayer* pl)
{
    if (pl->motionMove()) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 1;
        pl->xFF = 0;
    }
}

void wepDown(cPlayer* pl)
{
    pl->motionMove();
    if (dmMotCk()) {
        MotionSetCore(pl, &pl->pMotion, PL_ARC(0x8C), 0, 3, 5, 0);
        pl->xFC = 0;
        pl->xFD = 0;
        pl->xFE = 2;
        pl->xFF = 0;
    } else {
        pl->xFC = 0;
        pl->xFD = 0;
        pl->xFE = 0;
        pl->xFF = 1;
        pl->x4FD = 0xF;
        pl->x4FC = 0;
    }
}
