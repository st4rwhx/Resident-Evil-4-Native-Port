// Machine gun player routines (wep11/wep12/wep27/wep29/wep39 modules, first routine object; real
// file name unknown): routine 2 of the player while a machine gun is equipped: ready, set (idle /
// turn), fire (burst with recoil), down, reload. Modelled on game/pl_knife.cpp.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "pl_wep.h"
#include "global.h"
#include "main.h"
#include "joy.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "snd.h"
#include "rnd.h"
#include "math_sub.h"

// motion.h declares the one-argument MotionMove; the routines pass a second argument (pl_knife.cpp).
int MotionMoveI(cModel* m, int flag) asm("MotionMove");

#define WEP_ARC_PTR(no) PL_ARC_PTR((PlArc*) pG->pWepArc, no)

// Routine bytes through int parameters (player.cpp PlRoutineSet).
static inline void PlRoutineSet(cPlayer* pl, int r0, int r1, int r2, int r3)
{
    pl->xFC = r0;
    pl->xFD = r1;
    pl->xFE = r2;
    pl->xFF = r3;
}

u8 lockCtr = 0;

static void foo22(cPlayer* pl);
static void wep11_r2_ready(cPlayer* pl);
static void wep11_r3_ready00(cPlayer* pl);
static void wep11_r3_ready10(cPlayer* pl);
static void wep11_r3_ready20(cPlayer* pl);
static void wep11_r2_set(cPlayer* pl);
static void wep11_r3_set00(cPlayer* pl);
static void wep11_r3_set10(cPlayer* pl);
static void wep11_r3_set40(cPlayer* pl);
static void wep11_r2_fire(cPlayer* pl);
static void wep11_r3_fire00(cPlayer* pl);
static void wep11_r3_fire10(cPlayer* pl);
static void wep11_r2_down(cPlayer* pl);
static void wep11_r2_reload(cPlayer* pl);

void PlMachineMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep11_r2_ready,
        wep11_r2_set,
        wep11_r2_fire,
        wep11_r2_down,
        wep11_r2_reload,
    };

    func_tbl[pl->xFE](pl);
    pl->pWep->lockMove();
}

static void foo22(cPlayer* pl)
{
}

static void wep11_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep11_r3_ready00,
        wep11_r3_ready10,
        wep11_r3_ready20,
        foo22,
        foo22,
        foo22,
    };

    func_tbl[pl->xFF](pl);
    if (joyKamae() == 0) {
        if (pl->flags_420 & 0x40) {
            pl->xFC = 0;
            pl->xFE = 0;
            pl->xFD = 0x11;
            pl->xFF = 0;
        } else {
            pl->xFC = 0;
            pl->xFD = 0;
            pl->xFE = 0;
            pl->xFF = 0;
        }
    } else if (pl->keyReload() && pl->pWep->pObj->reloadable()) {
        pl->pWep->x26 |= 1;
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 4;
        pl->xFF = 0;
        pl->x3E0 = 1;
    } else {
        Vec aim = {0.0f, 1000.0f, 10000.0f};
        Vec hit;

        PSMTXMultVec(pl->mat, &aim, &aim);
        SatMgr.hitCheck(&pl->getPartsPtr(0)->worldPos, &aim, &hit, 0, 0, 0);
        CamCtrlShoulderSetAim(&hit);
    }
}

static void wep11_r3_ready00(cPlayer* pl)
{
    f32 pitch;
    void* mot;

    pl->x3E4 = 0;
    pl->pWep->x2C = 0.0f;
    pl->x400 = 0.0f;
    pl->pWep->x30 = CamCtrl.getCameraDirection();
    pitch = CamCtrl.getCameraPitch();
    if (pitch > 0.0f) {
        pitch += pitch;
    }
    pl->pWep->pitch = pitch;
    pitch *= 2.0f / PI;
    m3r[1] = pitch;
    m3r[0] = pitch;
    m3r[2] = 0.0f;
    pl->pNeck->init(0, 0, 0);

    if (pG->wep_no == 0xB) {
        SndCall(2, 9, &pl->getPartsPtr(0xA)->worldPos, 0, 0, 0);
    }
    pl->pWep->lockInit();
    mot = WEP_ARC_PTR(0x1A);
    mot3.set(pl, mot, mot, mot, 0, 3, 0, 4, 0);
    mot3.move(m3r[0]);
    lockCtr = 0;
    pl->xFF = 1;
}

static void wep11_r3_ready10(cPlayer* pl)
{
    const f32 endFrame = 5.0f;   // pool order: 5.0 before the 3.0 / 4.0 / 2.0 of the statements above

    if (pl->frame <= 3.0f) {
        f32 d = pl->pWep->x30 / (4.0f - pl->frame);

        pl->rot.y += d;
        pl->pWep->x30 -= d;
    }
    if (MotionCheckCrossFrame(&pl->mot, 2.0f)) {
        int se = 0x28;

        if (pl->x3E8 == 1) {
            se = 0x29;
        }
        SndCall(1, (u16) se, &pl->getPartsPtr(0)->worldPos, 0, 0, 0);
    }
    if (pl->frame >= endFrame) {
        PlRoutineSet(pl, 0, 6, 1, 4);
        pl->x3E0 = 0;
    }

    MotionMoveI(pl, 0);
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    pl->pWaist->set(0.0f, 0.4f);
}

static void wep11_r3_ready20(cPlayer* pl)
{
    if (MotionMoveI(pl, 0)) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->worldPos, 0, 0, 0);
        PlRoutineSet(pl, 0, 6, 1, 0);
        pl->x3E0 = 0;
    }
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    pl->pWaist->set(0.0f, 0.4f);
}

static void wep11_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep11_r3_set00,
        wep11_r3_set10,
        0,
        0,
        wep11_r3_set40,
    };

    func_tbl[pl->xFF](pl);
    PlWepLockCtrl(pl);
    pl->setLaserSight(1, 0);
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
    } else if (joyFireTrg()) {
        if (pl->pWep->pObj->bulletNum()) {
            PlRoutineSet(pl, 0, 6, 2, 0);
            pl->x3F8 = 0;
            PlWepLockRandInit();
        } else if (pl->pWep->pObj->reloadable()) {

            pl->pWep->x26 |= 1;
            PlRoutineSet(pl, 0, 6, 4, 0);
        } else {
            SndCall(2, 0x17, &pl->getPartsPtr(4)->worldPos, 0, 0, 0);
            goto reload;
        }
    } else if (joyFireOn() && pl->pWep->pObj->bulletNum()) {
        PlRoutineSet(pl, 0, 6, 2, 0);
        pl->x3F8 = 0;
        PlWepLockRandInit();
    } else {
    reload:
        if ((Joy[0].trg & 0x200) && pl->pWep->pObj->reloadable()) {
            pl->pWep->x26 |= 1;
            PlRoutineSet(pl, 0, 6, 4, 0);
            pl->x3E0 = 1;
        }
    }
}

static void wep11_r3_set00(cPlayer* pl)
{
    PlArc* arc;
    u8 hokan = 0;

    if (pl->x3E0 == 0) {
        hokan = 3;
    }
    arc = (PlArc*) pG->pWepArc;
    mot3.set(pl, PL_ARC_PTR(arc, 0x1B), PL_ARC_PTR(arc, 0x1F), PL_ARC_PTR(arc, 0x21), 0, hokan, 0, 4, 0);
    mot3.move(m3r[0]);
    pl->motionMove();
    pl->xFF = 1;
}

static void wep11_r3_set10(cPlayer* pl)
{
    pl->motionMove();
}

static void wep11_r3_set40(cPlayer* pl)
{
    if (pl->motionMove() || (Key.on & 0x10F)) {
        pl->xFF = 0;
    }
}

static void wep11_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep11_r3_fire00,
        wep11_r3_fire10,
        0,
    };

    PlWepLockCtrl(pl);
    func_tbl[pl->xFF](pl);
}

// Never called (its two static Vecs are the 0x18 unreferenced .bss bytes before fire00's p0/p1).
static inline void wep11_hitCheck(cPlayer* pl)
{
    static Vec p0;
    static Vec p1;

    PlWepHitCheck2(pl, &p0, &p1, pG->wep_no, 0, 6000.0f);
}

static void wep11_r3_fire00(cPlayer* pl)
{
    static Vec p0;
    static Vec p1;
    PlArc* arc;
    cModel* parts;
    f32 pitch;
    cObjWep* obj;

    pl->pWep->pObj->trigger();
    arc = (PlArc*) pG->pWepArc;
    mot3.set(pl, PL_ARC_PTR(arc, 0x1C), PL_ARC_PTR(arc, 0x20), PL_ARC_PTR(arc, 0x22), 0, 0, 0, 4, 0);
    mot3.move(m3r[0]);
    MotionMoveI(pl, 0);
    pl->pBody->waistMove();
    pl->partsWorldCalc();
    parts = pl->getPartsPtr(0xA);
    p0.x = -265.5f;
    p0.y = -24.0f;
    p0.z = 38.33f;
    PSMTXMultVec(parts->mat, &p0, &p0);
    p1.x = -50000.0f;
    p1.y = fRand1_1() * 200.0f;
    p1.z = fRand1_1() * 200.0f;
    PSMTXMultVecSR(parts->mat, &p1, &p1);
    PSVECAdd(&p0, &p1, &p1);
    PlWepHitCheck2(pl, &p0, &p1, pG->wep_no, 0, 6000.0f);
    obj = pl->pWep->pObj;
    obj->wep.mode = 2;
    obj->wep.step = 0;
    pl->x3F4 = 1;
    pl->x3F0 = 1;
    pitch = m3r[0];
    PlWepLockRand(pl, 2, &pitch, &pl->x400);
    m3r[1] = pitch;
    if (m3r[2] == 0.0f) {
        m3r[0] = pitch;
    }
    pl->pWep->pObj->drawLaserSight(1, 0);
    pl->xFF = 1;
}

static void wep11_r3_fire10(cPlayer* pl)
{
    pl->motionMove();
    pl->pWep->pObj->drawLaserSight(1, 1);
    if (pl->frame >= 3.0f) {
        if (joyKamae()) {
            if (joyFireOn()) {
                if (pl->pWep->pObj->bulletNum()) {
                    pl->xFF = 0;
                } else {
                    SndCall(2, 0x17, &pl->getPartsPtr(4)->worldPos, 0, 0, 0);
                    PlRoutineSet(pl, 0, 6, 1, 0);
                }
                pl->x3E0 = 1;
            } else {
                pl->x3E0 = 0;
                PlRoutineSet(pl, 0, 6, 1, 0);
            }

        } else if (pl->flags_420 & 0x40) {
            pl->xFC = 0;
            pl->xFE = 0;
            pl->xFD = 0x11;
            pl->xFF = 0;
        } else {
            int md = 3;

            PlRoutineSet(pl, 0, 6, md, 0);
        }
    }
}


static void wep11_r2_down(cPlayer* pl)
{
    if (dmMotCk()) {
        MotionSetCore(pl, &pl->mot, WEP_ARC_PTR(0x1D), 0, 3, 5, 0);
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

static void wep11_r2_reload(cPlayer* pl)
{
    switch (pl->xFF) {
    case 0: {
        void* mot;
        cObjWep* obj;

        switch (pG->x4FBA) {
        default:
            mot = WEP_ARC_PTR(0x1E);
            break;
        case 1:
            mot = WEP_ARC_PTR(0x23);
            break;
        case 2:
            mot = WEP_ARC_PTR(0x24);
            break;
        }
        MotionSetCore(pl, &pl->mot, mot, 0, 3, 5, 0);
        MotionMoveI(pl, 0);
        pl->pWep->knifeStance = 1;
        pl->xFF = 1;
        obj = pl->pWep->pObj;
        obj->wep.mode = 4;
        obj->wep.step = 0;
        break;
    }
    case 1:
        if (MotionCheckCrossFrame(&pl->mot, PlReloadEndTbl[pG->wep_no][pG->x4FBA]) && joyKamae() == 0) {
            if (pl->flags_420 & 0x40) {
                pl->xFC = 0;
                pl->xFE = 0;
                pl->xFD = 0x11;
                pl->xFF = 0;
            } else {
                pl->xFE = 3;
            }
        } else if (MotionMoveI(pl, 0)) {
            PlRoutineSet(pl, 0, 6, 1, 0);
            pl->x3E0 = 0;
        }
        break;
    case 3:
        if (MotionMoveI(pl, 0)) {
            pl->xFC = 0;
            pl->xFD = 0;
            pl->xFE = 0;
            pl->xFF = 0;
        }
        break;

    }
}

// The module's .data section is 8-aligned (the original linker's placement; the tables start at 4).
asm(".section .data\n\t.balign 8\n\t.text");
