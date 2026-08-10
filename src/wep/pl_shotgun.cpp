// Shotgun player routines (wep07/wep08/wep33 modules, first routine object; real file name unknown):
// routine 2 of the player while a shotgun is equipped: ready, set (idle / turn), fire (pellet spread
// hit checks), down, reload (shell by shell). Modelled on game/pl_knife.cpp; pl0a/wep07.cpp is
// Krauser's reduced build of the same file.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "pl_wep.h"
#include "global.h"
#include "main.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "snd.h"
#include "pad.h"
#include "rnd.h"
#include "math_sub.h"
#include "esp.h"

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

static void wep07_r2_ready(cPlayer* pl);
static void wep07_r3_ready00(cPlayer* pl);
static void wep07_r3_ready10(cPlayer* pl);
static void wep07_r3_ready20(cPlayer* pl);
static void wep07_r2_set(cPlayer* pl);
static void wep07_r3_set00(cPlayer* pl);
static void wep07_r3_set10(cPlayer* pl);
static void wep07_r3_set20(cPlayer* pl);
static void wep07_r3_set30(cPlayer* pl);
static void wep07_r3_set40(cPlayer* pl);
static void wep07_r2_fire(cPlayer* pl);
static void wep07_r3_fire00(cPlayer* pl);
static void wep07_r3_fire10(cPlayer* pl);
void wepDown(cPlayer* pl);
static void wep07_r2_reload(cPlayer* pl);

void PlShotgunMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep07_r2_ready,
        wep07_r2_set,
        wep07_r2_fire,
        0,
        wep07_r2_reload,
    };

    func_tbl[pl->xFE](pl);
    pl->pWep->lockMove();
    if (pl->pWep->x23) {
        pl->pWep->x23--;
    }
}

static void wep07_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep07_r3_ready00,
        wep07_r3_ready10,
        wep07_r3_ready20,
    };

    func_tbl[pl->xFF](pl);
    if (joyKamae() == 0) {
        pl->motSpeedRate = 1.0f;
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
        pl->motSpeedRate = 1.0f;
        pl->pWep->x26 |= 1;
        PlRoutineSet(pl, 0, 6, 4, 0);
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
    m3r[0] = pitch;
    m3r[2] = 0.0f;
    pl->x400 = 0.0f;
    pl->pNeck->init(0, 0, 0);
    pl->pWep->x30 = CamCtrl.getCameraDirection();
    pl->pWep->lockInit();
    hokan = 4;
    if (!(pl->flags_420 & 0x40)) {
        hokan = 5;
    }
    mot = WEP_ARC_PTR(0x18);
    mot3.set(pl, mot, mot, mot, 0, 3, 0, hokan, 0);
    if (pG->wep_no == 8) {
        pl->motSpeedRate = 1.4f;
    }
    mot3.move(m3r[0]);
    pl->xFF = 1;
}

static void wep07_r3_ready10(cPlayer* pl)
{
    f32 endFrame;

    if (pl->frame >= 5.0f) {
        PlWepLockCtrl(pl);
    }
    if (pl->frame > 1.7f && pl->frame < 2.3f) {
        int se = 0x28;

        if (pl->x3E8 == 1) {
            se = 0x29;
        }
        SndCall(1, (u16) se, &pl->getPartsPtr(0)->worldPos, 0, 0, 0);
    }
    if (pl->frame < 4.0f) {
        f32 d = pl->pWep->x30 / (4.0f - pl->frame);

        pl->rot.y += d;
        pl->pWep->x30 -= d;
    }
    pl->motionMove();
    if (pG->wep_no == 8) {
        endFrame = 8.0f;
    } else {
        endFrame = 10.0f;
    }
    if (pl->frame >= endFrame) {
        pl->motSpeedRate = 1.0f;
        SndCall(2, 9, &pl->getPartsPtr(0xA)->worldPos, 0, 0, 0);
        PlRoutineSet(pl, 0, 6, 1, 0);
    }
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    pl->pWaist->set(0.0f, 0.4f);
}

static void wep07_r3_ready20(cPlayer* pl)
{
    if (MotionMoveI(pl, 0)) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->worldPos, 0, 0, 0);
        PlRoutineSet(pl, 0, 6, 1, 0);
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
        wep07_r3_set20,
        wep07_r3_set30,
        wep07_r3_set40,
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
            wepDown(pl);
        }
    } else if (joyFireTrg()) {
        if (pl->pWep->pObj->bulletNum()) {
            PlRoutineSet(pl, 0, 6, 2, 0);
        } else if (pl->pWep->pObj->reloadable()) {
            pl->pWep->x26 |= 1;
            PlRoutineSet(pl, 0, 6, 4, 0);
            pl->x3E0 = 0;
        } else {
            SndCall(2, 3, &pl->getPartsPtr(4)->worldPos, 0, 0, 0);
            goto reload;
        }
    } else if (joyFireOn() && pl->pWep->pObj->bulletNum()) {
        PlRoutineSet(pl, 0, 6, 2, 0);
    } else {
    reload:
        if (pl->keyReload() && pl->pWep->pObj->reloadable()) {
            pl->pWep->x26 |= 1;
            PlRoutineSet(pl, 0, 6, 4, 0);
            pl->x3E0 = 1;
        }
    }
}

static void wep07_r3_set00(cPlayer* pl)
{
    PlArc* arc = (PlArc*) pG->pWepArc;

    mot3.set(pl, PL_ARC_PTR(arc, 0x1A), PL_ARC_PTR(arc, 0x20), PL_ARC_PTR(arc, 0x22), 0, 3, 0, 4, 0);
    mot3.move(m3r[0]);
    pl->motionMove();
    pl->xFF = 1;
}

static void wep07_r3_set10(cPlayer* pl)
{
    MotionMoveI(pl, 0);
}

static void wep07_r3_set20(cPlayer* pl)
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

static void wep07_r3_set30(cPlayer* pl)
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

static void wep07_r3_set40(cPlayer* pl)
{
    if (pl->motionMove() || (Key.on & 0x10F)) {
        pl->xFF = 0;
    }
}

static void wep07_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep07_r3_fire00,
        wep07_r3_fire10,
        0,
    };

    func_tbl[pl->xFF](pl);
}

static void wep07_r3_fire00(cPlayer* pl)
{
    Vec p0;
    Vec p1;
    Vec d;
    f32 wh;
    f32 pitch;
    PlArc* arc;
    f32 rnd;
    int n = 0xD;
    u32 i;
    cObjWep* obj;

    pl->pWep->pObj->trigger();
    arc = (PlArc*) pG->pWepArc;
    mot3.set(pl, PL_ARC_PTR(arc, 0x1E), PL_ARC_PTR(arc, 0x21), PL_ARC_PTR(arc, 0x23), 0, 0, 0, 4, 0);
    mot3.move(m3r[0]);
    MotionMoveI(pl, 0);
    pl->pBody->waistMove();
    pl->partsWorldCalc();
    rnd = fRand1_1();
    if (pG->wep_no == 8) {
        n = 0x13;
    }
    for (i = 0; i < n; i++) {
        cModel* parts = pl->getPartsPtr(0xA);
        u32 flag;

        if (i == 0) {
            p0.x = -15.5f;
            p0.y = -24.0f;
            p0.z = 38.33f;
        } else {
            p0.x = 234.5f;
            p0.y = -24.0f;
            p0.z = 38.33f;
        }
        p1.x = -50000.0f;
        p1.y = 0.0f;
        p1.z = 0.0f;
        if (i != 0) {
            f32 r = (f32) ((i - 1) / 6) + 1.0f;
            f32 r2;

            switch ((i - 1) % 6) {
            case 0:
                d.x = 0.0f;
                d.y = 0.5f;
                d.z = 0.866f;
                break;
            case 1:
                d.x = 0.0f;
                d.y = 0.5f;
                d.z = -0.866f;
                break;
            case 2:
                d.x = 0.0f;
                d.y = -0.5f;
                d.z = 0.866f;
                break;
            case 3:
                d.x = 0.0f;
                d.y = -0.5f;
                d.z = -0.866f;
                break;
            case 4:
                d.x = 0.0f;
                d.y = 1.0f;
                d.z = 0.0f;
                break;
            case 5:
                d.x = 0.0f;
                d.y = -1.0f;
                d.z = 0.0f;
                break;
            }
            p0.y += d.y * (r * 150.0f);
            p0.z += d.z * (r * 150.0f);
            rnd += fRand0_1() + PI;
            d.y = sinf(rnd);
            d.z = cosf(rnd);
            r2 = (f32) ((i - 1) / 3) + 1.0f;
            p1.y += d.y * r2 * 1500.0f;
            p1.z += d.z * r2 * 1500.0f;
        } else {
            Vec* mk;

            p1.y += fRand1_1() * 100.0f;
            p1.z += fRand1_1() * 100.0f;
            mk = &pl->pWep->pObj->wep.marker;
            if (GetWaterHeight(mk, &wh) && mk->y <= wh) {
                SndCall(2, 0xB, mk, 0, 0, 0);
            }
        }
        PSMTXMultVec(parts->mat, &p0, &p0);
        PSMTXMultVecSR(parts->mat, &p1, &p1);
        PSVECAdd(&p0, &p1, &p1);
        flag = 0;
        if (i & 3) {
            flag = 1;
        }
        if (i != 0) {
            flag |= 4;
        }
        PlWepHitCheck2(pl, &p0, &p1, pG->wep_no, flag, 6000.0f);
    }
    pl->x3F4 = 1;
    pl->x3F0 = 1;
    obj = pl->pWep->pObj;
    obj->wep.mode = 2;
    obj->wep.step = 0;
    pitch = m3r[0];
    PlWepLockRand(pl, 2, &pitch, &pl->x400);
    m3r[1] = pitch;
    {
        // COMPILER-DIFF: #13: the original never allocates the REG_EQUIV 0.0 pseudo; reload
        // re-materialises `lis/lfs` for the compare in the first free FPR (f0, so m3r[2] takes f13)
        // and sched2 issues it before the m3r[2] load. Ours would local-alloc the shorter m3r[2]
        // load to f0 and keep the RTL order (m3r[2] first).
        register f32 zero asm("fr0"); // COMPILER-DIFF: #13
        zero = 0.0f;
        if (m3r[2] == zero) {
            m3r[0] = pitch;
        }
    }
    pl->xFF = 1;
}

static void wep07_r3_fire10(cPlayer* pl)
{
    int endFrame;

    // The dead `cmpwi 0x21` after the tree needs a third node below 8 grouped with the default
    // (`case 7:`): the 3-node tree emits `bgt test` around it, jump.c inverts that into `ble default`
    // and jump2 cross-jumps the separate 0x21 body, leaving its compare.
    switch (pG->wep_no) {
    case 8:
        endFrame = 0x20;
        break;
    case 0x21:
        endFrame = 0x28;
        break;
    case 7:
    default:
        endFrame = 0x28;
        break;
    }
    if (pl->frame >= 10.0f && joyKamae()) {
        PlWepLockCtrl(pl);
    }
    pl->motionMove();
    if (pl->frame >= (f32) endFrame) {
        if (joyKamae() && joyFireOn() && pl->pWep->pObj->bulletNum() == 0) {
            SndCall(2, 3, &pl->getPartsPtr(4)->worldPos, 0, 0, 0);
        }
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 1;
        pl->xFF = 4;
    }
    if (pl->frame >= 10.0f) {
        pl->pBody->waistMove();
        pl->partsWorldCalc();
    }
}

void wepDown(cPlayer* pl)
{
    if (dmMotCk()) {
        MotionSetCore(pl, &pl->mot, WEP_ARC_PTR(0x1F), 0, 3, 5, 0);
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

static void wep07_r2_reload(cPlayer* pl)
{
    switch (pl->xFF) {
    case 0: {
        void* mot;
        cObjWep* obj;

        switch (pG->x4FBA) {
        default:
            mot = WEP_ARC_PTR(0x2A);
            break;
        case 1:
            mot = WEP_ARC_PTR(0x2C);
            break;
        case 2:
            mot = WEP_ARC_PTR(0x2E);
            break;
        }
        MotionSetCore(pl, &pl->mot, mot, 0, 3, 5, 0);
        pl->motionMove();
        pl->pWep->x21 = 0;
        pl->pWep->knifeStance = 1;
        pl->xFF = 1;
        obj = pl->pWep->pObj;
        obj->wep.mode = 4;
        obj->wep.step = 0;
        break;
    }
    case 1:
        pl->motionMove();
        if (m3r[0] < -0.1f || m3r[0] > 0.1f) {
            if (pl->frame >= PlReloadEndTbl[pG->wep_no][pG->x4FBA]) {
                if (joyKamae()) {
                    pl->xFF = 2;
                } else if (pl->flags_420 & 0x40) {
                    pl->xFC = 0;
                    pl->xFE = 0;
                    pl->xFD = 0x11;
                    pl->xFF = 0;
                } else {
                    pl->x4FD = 9;
                    pl->x4FC = 0;
                    pl->xFC = 0;
                    pl->xFD = 0;
                    pl->xFE = 0;
                    pl->xFF = 2;
                }
            }
        } else {
            if (joyKamae() == 0 && pl->frame >= PlReloadEndTbl[pG->wep_no][pG->x4FBA]) {
                if (pl->flags_420 & 0x40) {
                    pl->xFC = 0;
                    pl->xFE = 0;
                    pl->xFD = 0x11;
                    pl->xFF = 0;
                } else {
                    pl->x4FD = 9;
                    pl->x4FC = 0;
                    pl->xFC = 0;
                    pl->xFD = 0;
                    pl->xFE = 0;
                    pl->xFF = 2;
                }
            }
            if (pl->frame >= (f32) (pl->frameMax - 1)) {
                PlRoutineSet(pl, 0, 6, 1, 0);
            }
        }
        break;
    case 2: {
        PlArc* arc = (PlArc*) pG->pWepArc;

        mot3.set(pl, PL_ARC_PTR(arc, 0x1A), PL_ARC_PTR(arc, 0x20), PL_ARC_PTR(arc, 0x22), 0, 9, 0, 4, 0);
        pl->x3E0 = 0;
        pl->xFF = 3;
    }
    case 3:
        if ((int) ++pl->x3E0 > 8) {
            PlRoutineSet(pl, 0, 6, 1, 0);
        }
        mot3.move(m3r[0]);
        pl->motionMove();
        break;
    }
}

// The module's .data section is 8-aligned in the original.
asm(".section .data\n\t.balign 8\n\t.text");
