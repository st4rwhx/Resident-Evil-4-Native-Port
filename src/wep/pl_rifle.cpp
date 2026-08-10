// Rifle player routines ("D:/Bio4/Prog/pl_rifle.cpp"; wep09/wep10/wep40/wep47 modules, first routine
// object): routine 2 of the player while a rifle is equipped: ready, set (scope camera), fire, down,
// reload, next target. Modelled on game/pl_knife.cpp.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "pl_wep.h"
#include "global.h"
#include "main.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "motion.h"
#include "snd.h"
#include "pad.h"
#include "math_sub.h"

// motion.h declares the one-argument MotionMove; the routines pass a second argument (pl_knife.cpp).
int MotionMoveI(cModel* m, int flag) asm("MotionMove");

#define WEP_ARC_PTR(no) PL_ARC_PTR((PlArc*) pG->pWepArc, no)

// Store through a scalar reference: the following global load stays below it.
static inline void ISet(int& d, int v) { d = v; }

// Routine bytes through int parameters (player.cpp PlRoutineSet).
static inline void PlRoutineSet(cPlayer* pl, int r0, int r1, int r2, int r3)
{
    pl->xFC = r0;
    pl->xFD = r1;
    pl->xFE = r2;
    pl->xFF = r3;
}

// Scope camera on: the thermal light set for the infrared scope (weapon type 2 / weapon 0x1D).
static inline void scopeOn(cPlayer* pl)
{
    BitOn(pl->flags_420, 0x10);
    if (pG->wep_type == 2 || pG->wep_no == 0x1D) {
        pG->flags_5010 |= 0x04000000;
        pl->flags_420 |= 0x200;
        LightMgr.setThermo();
    }
}

static void wep09_r2_ready(cPlayer* pl);
static void wep09_r3_ready00(cPlayer* pl);
static void wep09_r3_ready10(cPlayer* pl);
static void wep09_r2_set(cPlayer* pl);
static void wep09_r3_set00(cPlayer* pl);
static void wep09_r3_set10(cPlayer* pl);
static void wep09_r3_set20(cPlayer* pl);
static void wep09_r2_fire(cPlayer* pl);
static void wep09_r3_fire00(cPlayer* pl);
static void wep09_r3_fire10(cPlayer* pl);
static void wep09_r3_fire20(cPlayer* pl);
static void wep09_r3_fire30(cPlayer* pl);
static void wepDown(cPlayer* pl);
static void wep09_r2_reload(cPlayer* pl);
static void wep09_r2_next(cPlayer* pl);

void PlRifleMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep09_r2_ready,
        wep09_r2_set,
        wep09_r2_fire,
        wepDown,
        wep09_r2_reload,
        wep09_r2_next,
    };

    func_tbl[pl->xFE](pl);
    pl->pWep->lockMove();
}

static void wep09_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep09_r3_ready00,
        wep09_r3_ready10,
    };

    func_tbl[pl->xFF](pl);
    if (joyKamae() == 0 && pl->xFF != 3) {
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
    } else {
        if (pl->pLockEm) {
            CamCtrlShoulderSetAim(&pl->pLockEm->pos);
        } else {
            Vec aim = {0.0f, 1000.0f, 10000.0f};
            Vec hit;

            PSMTXMultVec(pl->mat, &aim, &aim);
            SatMgr.hitCheck(&pl->getPartsPtr(0)->worldPos, &aim, &hit, 0, 0, 0);
            CamCtrlShoulderSetAim(&hit);
        }
        if (pl->keyReload() && pl->pWep->pObj->reloadable()) {
            pl->pWep->x26 |= 1;
            PlRoutineSet(pl, 0, 6, 4, 0);
            pl->pWep->pObj->setDisp(1, 1);
            pl->x3E0 = 1;
        }
    }
}

static void wep09_r3_ready00(cPlayer* pl)
{
    void* mot;
    cPlWep* w = pl->pWep;

    w->x2C = 0.0f;
    w->pitch = 0.0f;
    pl->pWep->x30 = CamCtrl.getCameraDirection();
    pl->pNeck->init(0, 0, 0);
    pl->pWep->lockInit();
    mot = WEP_ARC_PTR(0x14);
    mot3.set(pl, mot, mot, mot, 0, 3, 0, 4, 0);
    mot3.move(m3r[0]);
    pl->motionMove();
    pl->xFF = 1;
    pl->x3E4 = 0;
    pl->x3F0 = 0;
}

static void wep09_r3_ready10(cPlayer* pl)
{
    if (pl->frame < 4.0f) {
        f32 d = pl->pWep->x30 / (4.0f - pl->frame);

        pl->rot.y += d;
        pl->pWep->x30 -= d;
    }
    if (pl->motionMove()) {
        PlRoutineSet(pl, 0, 6, 1, 0);
        pl->x3F0 = 10;
    }
    pl->pWaist->set(0.0f, 0.4f);
}

static void wep09_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep09_r3_set00,
        wep09_r3_set10,
        wep09_r3_set20,
    };

    func_tbl[pl->xFF](pl);
    pl->setLaserSight(0, 0);
    if (pl->x3E4 == 0 && MotionCheckCrossFrame(&pl->mot, 2.0f)) {
        SndCall(2, 9, &pl->pParts->worldPos, 0, 0, 0);
        pl->x3E4 = 1;
    }
    if (pl->x3F0 != 0) {
        pl->x3F0--;
    }
    if (joyKamae() == 0) {
        Vec at;

        if (pl->flags_420 & 0x40) {
            pl->xFC = 0;
            pl->xFE = 0;
            pl->xFD = 0x11;
            pl->xFF = 0;
        } else {
            int md = 3;

            PlRoutineSet(pl, 0, 6, md, 0);
        }
        pl->x3E0 = 0;
        CamCtrl.getTrajectory(&pl->evTarget, &at);
        PSVECSubtract(&at, &pl->evTarget, &pl->evTarget);
    } else if (joyFireOn() && pl->x3F0 == 0 && pl->pWep->pObj->bulletNum()) {
        PlRoutineSet(pl, 0, 6, 2, 0);
    } else if (joyFireTrg() && pl->pWep->pObj->bulletNum() == 0) {
        if (pl->pWep->pObj->reloadable()) {
            pl->pWep->x26 |= 1;
            PlRoutineSet(pl, 0, 6, 4, 0);
            pl->pWep->pObj->setDisp(1, 1);
            pl->x3E0 = 0;
        } else {
            SndCall(2, 3, &pl->getPartsPtr(4)->worldPos, 0, 0, 0);
            goto reload;
        }
    } else {
    reload:
        if (pl->keyReload() && pl->pWep->pObj->reloadable()) {
            pl->pWep->pObj->setDisp(1, 1);
            pl->pWep->x26 |= 1;
            PlRoutineSet(pl, 0, 6, 4, 0);
            pl->x3E0 = 1;
        }
    }
}

static void wep09_r3_set00(cPlayer* pl)
{
    CamCtrl.startScope(0, 0);
    CameraMove();
    scopeOn(pl);
    pl->motionSet(WEP_ARC_PTR(0x15), 5, 0, 1, 0);
    pl->motionMove();
    pl->xFF = 1;
}

static void wep09_r3_set10(cPlayer* pl)
{
    pl->motionMove();
}

static void wep09_r3_set20(cPlayer* pl)
{
    if (pl->motionMove()) {
        pl->xFF = 0;
    }
}

static void wep09_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep09_r3_fire00,
        wep09_r3_fire10,
        wep09_r3_fire20,
        wep09_r3_fire30,
        0,
    };

    func_tbl[pl->xFF](pl);
}

static void wep09_r3_fire00(cPlayer* pl)
{
    Vec from;
    Vec to;
    Vec dir;
    void* mot;
    cObjWep* obj;

    pl->pWep->pObj->trigger();
    mot = WEP_ARC_PTR(0x15);
    mot3.set(pl, mot, mot, mot, 0, 0, 0, 4, 0);
    pl->motionMove();
    CamCtrl.getTrajectory(&from, &to);
    if (pG->wep_no == 0xA) {
        PSVECSubtract(&to, &from, &dir);
#line 406 "D:/Bio4/Prog/pl_rifle.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, 200000.0f);
        PSVECAdd(&from, &dir, &to);
    }
    PlWepHitCheck2(pl, &from, &to, pG->wep_no, 0, 6000.0f);
    if (pG->wep_no == 9) {
        SndCall(2, 0, &pl->getPartsPtr(4)->worldPos, 0, 0, 0);
    }
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0, 1);
    ISet(pl->x3F0, 0);
    if (pG->wep_no == 0xA) {
        obj = pl->pWep->pObj;
        obj->wep.mode = 2;
        obj->wep.step = 0;
    }
    pl->pWep->pObj->setDisp(1, 0);
    CamCtrl.getTrajectory(&pl->evTarget, &dir);
    PSVECSubtract(&dir, &pl->evTarget, &pl->evTarget);
    pl->xFF = 1;
}

static void wep09_r3_fire10(cPlayer* pl)
{
    cPlWep* w;

    pl->motionMove();
    ISet(pl->x3F0, pl->x3F0 + 1);   // reference store: the pG load stays below it
    w = pl->pWep;
    if (pl->x3F0 > (u8) PlShotFrameTbl[pG->wep_no][pG->wep_lv_mag]) {
        if (pG->wep_no != 9 || w->pObj->bulletNum() == 0) {
            PlRoutineSet(pl, 0, 6, 1, 0);
            pl->x3F0 = 10;
        } else {
            pl->xFF = 2;
        }
    } else if (pG->wep_no != 9 && pl->x3F0 > 10 && joyKamae() == 0) {
        PlRoutineSet(pl, 0, 6, 3, 0);
        pl->x3E0 = 0;
    }
}

static void wep09_r3_fire20(cPlayer* pl)
{
    cObjWep* obj;

    m3r[1] = 0.0f;
    m3r[0] = 0.0f;
    CamCtrl.saveScopeParam();
    pl->endCamera();
    pl->pWep->pObj->setDisp(1, 1);
    MotionSetCore(pl, &pl->mot, WEP_ARC_PTR(0x1B), 0, 3, 5, 0);
    pl->motionMove();
    obj = pl->pWep->pObj;
    obj->wep.mode = 2;
    obj->wep.step = 0;
    pl->xFF = 3;
}

static void wep09_r3_fire30(cPlayer* pl)
{
    if (joyKamae() == 0 && pl->frame >= 25.0f) {
        if (pl->flags_420 & 0x40) {
            pl->xFC = 0;
            pl->xFE = 0;
            pl->xFD = 0x11;
            pl->xFF = 0;
        } else {
            // store order brute-forced (x3E0 first, xFD last, the 3 through an int local)
            int md = 3;

            pl->x3E0 = 1;
            pl->xFC = 0;
            pl->xFE = md;
            pl->xFF = 0;
            pl->xFD = 6;
        }
    } else if (pl->motionMove()) {
        if (joyKamae()) {
            CamCtrl.startScope(0, 0);
            CamCtrl.loadScopeParam();
            CameraMove();
            scopeOn(pl);
            PlRoutineSet(pl, 0, 6, 1, 0);
        } else if (pl->flags_420 & 0x40) {
            pl->xFC = 0;
            pl->xFE = 0;
            pl->xFD = 0x11;
            pl->xFF = 0;
        } else {
            // store order brute-forced (x3E0 first, xFD last, the 3 through an int local)
            int md = 3;

            pl->x3E0 = 1;
            pl->xFC = 0;
            pl->xFE = md;
            pl->xFF = 0;
            pl->xFD = 6;
        }
    }
}

static void wepDown(cPlayer* pl)
{
    f32 e;

    pl->endCamera();
    e = VecElevation(&pl->evTarget);
    m3r[0] = e;
    m3r[1] = e;
    m3r[2] = 0.0f;
    pl->pWep->pObj->setDisp(1, 1);
    if (dmMotCk()) {
        if (pG->wep_no == 0xA) {
            void* mot = WEP_ARC_PTR(0x16);

            mot3.set(pl, mot, mot, mot, 0, 5, 0, 4, 0);
        } else if (pl->x3E0 == 0) {
            PlArc* arc = (PlArc*) pG->pWepArc;

            mot3.set(pl, PL_ARC_PTR(arc, 0x16), PL_ARC_PTR(arc, 0x19), PL_ARC_PTR(arc, 0x1A), 0, 0, 0, 4, 0);
        } else {
            void* mot = WEP_ARC_PTR(0x1C);
            cObjWep* obj;

            mot3.set(pl, mot, mot, mot, 0, 0, 0, 4, 0);
            obj = pl->pWep->pObj;
            obj->motionSet(WEP_ARC_PTR(0x24), 0, 0, 1, 0);
            obj->wep.mode = 0;
            obj->wep.step = 0;
        }
        mot3.move(m3r[0]);
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

static void wep09_r2_reload(cPlayer* pl)
{
    switch (pl->xFF) {
    case 0: {
        void* mot;
        cObjWep* obj;

        m3r[1] = 0.0f;
        m3r[0] = 0.0f;
        pl->endCamera();
        switch (pG->x4FBA) {
        default:
            mot = WEP_ARC_PTR(0x17);
            break;
        case 1:
            mot = WEP_ARC_PTR(0x1D);
            break;
        case 2:
            mot = WEP_ARC_PTR(0x1E);
            break;
        }
        MotionSetCore(pl, &pl->mot, mot, 0, 3, 5, 0);
        pl->motionMove();
        pl->pWep->knifeStance = 1;
        pl->xFF = 1;
        obj = pl->pWep->pObj;
        obj->wep.mode = 4;
        obj->wep.step = 0;
        break;
    }
    case 1:
        if (joyKamae() == 0 && pl->mot.frame >= PlReloadEndTbl[pG->wep_no][pG->x4FBA]) {
            if (pl->flags_420 & 0x40) {
                pl->xFC = 0;
                pl->xFE = 0;
                pl->xFD = 0x11;
                pl->xFF = 0;
            } else {
                int md = 3;

                PlRoutineSet(pl, 0, 6, md, 0);
                pl->x3E0 = 1;
            }
        }
        if (pl->frame >= (f32) (pl->frameMax - 5)) {
            CamCtrl.startScope(0, 0);
            CameraMove();
            scopeOn(pl);
            SndCall(2, 9, &pl->pParts->worldPos, 0, 0, 0);
            PlRoutineSet(pl, 0, 6, 1, 2);
            pl->x3F0 = 10;
        }
        MotionMoveI(pl, 0);
        break;
    }
}

static void wep09_r2_next(cPlayer* pl)
{
    cModel* em = pl->pLockEm;

    switch (pl->xFF) {
    case 0:
        pl->x3E0 = 0;
        pl->xFF = 1;
    case 1:
        if (em) {
            if (GetDistance3(&pl->pos, &em->pos) > 200.0f) {
                pl->rot.y += Muku(&pl->pos, &em->pos, pl->rot.y, 0.31415927f);
                pl->rot.y = LIMIT_ANGLE(pl->rot.y);
            }
        }
        if ((int) pl->x3E0++ > 9) {
            PlRoutineSet(pl, 0, 6, 1, 0);
        }
        break;
    }
    if (Key.trg & 0x20) {
        if (pl->pWep->lockNext()) {
            PlRoutineSet(pl, 0, 6, 5, 0);
        } else {
            PlRoutineSet(pl, 0, 6, 1, 0);
        }
    } else {
        if (joyKamae() == 0) {
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
        CameraMove();
        pl->motionMove();
    }
}

// The module's .data section is 8-aligned in the original.
asm(".section .data\n\t.balign 8\n\t.text");
