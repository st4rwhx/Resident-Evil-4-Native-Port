// Handgun player routines (the pl_handgun object of the handgun weapon modules wep01/02/04/05/06/
// 15/38/43/44, byte-identical in all nine; real file name unknown): routine 2 of the player while a
// handgun is equipped: ready (draw + aim), set (idle / turn), fire, down (wepDown) and reload.
// Modelled on game/pl_knife.cpp / wep/pl_rocket.cpp.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "pl_wep.h"
#include "global.h"
#include "main.h"
#include "cam_ctrl.h"
#include "camera.h"
#include "motion.h"
#include "pad.h"
#include "snd.h"
#include "rnd.h"
#include "math_sub.h"

extern "C" {
f64 atan2(f64 y, f64 x);
// motion.h declares the one-argument MotionMove; the routines pass a second argument (pl_knife.cpp).
int MotionMoveI(cModel* m, int flag) asm("MotionMove");
}

#define WEP_ARC_PTR(no) PL_ARC_PTR((PlArc*) pG->pWepArc, no)
#define WEP_OBJ(pl) ((pl)->pWep->pObj)

// The weapon object's own cAtariInfo (the object's collision with enemies while it is held).
#define WEP_ATARI(pl) (&WEP_OBJ(pl)->sub2B4.atari)

static void wep02_r2_ready(cPlayer* pl);
static void wep02_r3_ready00(cPlayer* pl);
static void wep02_r3_ready10(cPlayer* pl);
static void wep02_r3_ready20(cPlayer* pl);
static void wep02_r3_ready30(cPlayer* pl);
static void wep02_r2_set(cPlayer* pl);
static void wep02_r3_set00(cPlayer* pl);
static void wep02_r3_set10(cPlayer* pl);
static void wep02_r3_set40(cPlayer* pl);
static void wep02_r2_fire(cPlayer* pl);
static void wep02_r3_fire00(cPlayer* pl);
static void wep02_r3_fire10(cPlayer* pl);
void wepDown(cPlayer* pl);
static void wep02_r2_reload(cPlayer* pl);

// ready30 (the turn towards the lock target): positions the player is pulled to / turned to.
static Vec pos = {0.0f, 0.0f, 0.0f};
static Vec tgt = {0.0f, 0.0f, 0.0f};

#ifndef PL_HANDGUN_NO_MOVE
void PlHandgunMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep02_r2_ready,
        wep02_r2_set,
        wep02_r2_fire,
        0,
        wep02_r2_reload,
    };

    func_tbl[pl->xFE](pl);
    pl->pWep->lockMove();
}
#else
// pl0d (Wesker, src/pl0d/wep02.cpp): the same object without PlHandgunMove; its routine table stays in .data.
static void (*wep02_func_tbl[])(cPlayer*) = {
    wep02_r2_ready,
    wep02_r2_set,
    wep02_r2_fire,
    0,
    wep02_r2_reload,
};
#endif

static void wep02_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep02_r3_ready00,
        wep02_r3_ready10,
        wep02_r3_ready20,
        wep02_r3_ready30,
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
            WEP_ATARI(pl)->clrFlag200();
        }
    } else if (pl->keyReload() && WEP_OBJ(pl)->reloadable()) {
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

static void wep02_r3_ready00(cPlayer* pl)
{
    void* mot0;
    void* mot1;
    cObjWep* obj;

    pl->x3E4 = 0;
    pl->pWep->x2C = 0.0f;
    pl->pWep->lockInit();
    PlSetLockPitch(pl);
    pl->x400 = 0.0f;
    pl->pNeck->init(0, 0, 0);
    obj = WEP_OBJ(pl);
    obj->wep.mode = 1;
    obj->wep.step = 0;
    if (pG->wep_no == 2) {
        WEP_ATARI(pl)->setFlag200();
    }
    FSet(pl->pWep->x30, CamCtrl.getCameraDirection());
    mot0 = WEP_ARC_PTR(0x22);
    mot1 = WEP_ARC_PTR(0x23);
    mot3.set(pl, mot0, mot0, mot0, (int) mot1, 3, 0, 4, 0);
    mot3.move(m3r[0]);
    pl->xFF = 1;
}

static void wep02_r3_ready10(cPlayer* pl)
{
    if (pl->frame > 1.7f && pl->frame < 2.3f) {
        if (pl->x3E8 == 1) {
            SndCall(1, 0x29, &pl->getPartsPtr(0)->worldPos, 0, 0, 0);
        } else {
            SndCall(1, 0x28, &pl->getPartsPtr(0)->worldPos, 0, 0, 0);
        }
    }
    if (pl->frame < 4.0f) {
        f32 d = pl->pWep->x30 / (4.0f - pl->frame);

        pl->rot.y += d;
        pl->pWep->x30 -= d;
    }
    pl->motionMove();
    if (pl->frame >= 4.0f) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 1;
        pl->xFF = 4;
    }
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    pl->pWaist->set(0.0f, 0.4f);
}

static void wep02_r3_ready20(cPlayer* pl)
{
    MotionMoveI(pl, 0);
    if (pl->frame >= 4.0f) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 1;
        pl->xFF = 4;
    }
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    pl->pWaist->set(0.0f, 0.4f);
}

// ready30: turn / step towards the aim target while the motion plays. The pitch control is the
// shared PlWepAutoTrack tail (game/pl_wep.cpp). Forms that matter: `t` is assigned AFTER the Muku
// call and lives across GetDistance3 (a pseudo that already crosses a call is hoisted by sched1 above
// the earlier call and reload_cse turns its `addi` into the copy `mr r29, r4` of Muku's argument);
// m3r is walked through the pointer `r` (assigned after atan2) except for the first m3r[0] store,
// which is a direct reference (fresh `lis`); the clamp bounds are variables (both loaded before the
// first compare).
static void wep02_r3_ready30(cPlayer* pl)
{
    f32 dist;
    f32 x;
    f64 a;
    f32* r;
    Vec* t;

    if (MotionMoveI(pl, 0)) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->worldPos, 0, 0, 0);
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 1;
        pl->xFF = 0;
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

static void wep02_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep02_r3_set00,
        wep02_r3_set10,
        0,
        0,
        wep02_r3_set40,
    };
    int fire;

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
        return;
    }
    fire = joyFireTrg();
    if (fire) {
        fire = WEP_OBJ(pl)->bulletNum();
        if (fire) {
            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = 2;
            pl->xFF = 0;
            return;
        }
        if (WEP_OBJ(pl)->reloadable()) {
            pl->pWep->x26 |= 1;
            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = 4;
            pl->xFF = 0;
            pl->x3E0 = fire;
            return;
        }
        SndCall(2, 0x17, &pl->getPartsPtr(4)->worldPos, 0, 0, 0);
    } else if (joyFireOn() && WEP_OBJ(pl)->bulletNum()) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 2;
        pl->xFF = 0;
        return;
    }
    if (pl->keyReload() && WEP_OBJ(pl)->reloadable()) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 4;
        pl->xFF = 0;
        pl->x3E0 = 1;
    }
}

static void wep02_r3_set00(cPlayer* pl)
{
    PlArc* arc = (PlArc*) pG->pWepArc;

    mot3.set(pl, PL_ARC_PTR(arc, 0x26), PL_ARC_PTR(arc, 0x27), PL_ARC_PTR(arc, 0x28), 0, 3, 0, 4, 0);
    mot3.move(m3r[0]);
    pl->motionMove();
    pl->xFF = 1;
}

static void wep02_r3_set10(cPlayer* pl)
{
    pl->motionMove();
}

static void wep02_r3_set40(cPlayer* pl)
{
    if (pl->motionMove() || (Key.on & 0x10F)) {
        pl->xFF = 0;
    }
}

static void wep02_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep02_r3_fire00,
        wep02_r3_fire10,
    };

    func_tbl[pl->xFF](pl);
    PlWepLockCtrl(pl);
}

static void wep02_r3_fire00(cPlayer* pl)
{
    PlArc* arc;
    cModel* parts;
    cObjWep* obj;
    Vec p0;
    Vec p1;
    f32 pitch;

    WEP_OBJ(pl)->trigger();
    arc = (PlArc*) pG->pWepArc;
    mot3.set(pl, PL_ARC_PTR(arc, 0x29), PL_ARC_PTR(arc, 0x2A), PL_ARC_PTR(arc, 0x2B), 0, 0, 0, 4, 0);
    mot3.move(m3r[0]);
    MotionMoveI(pl, 0);
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    pl->pWaist->set(pl->x400, 0.4f);
    WEP_ATARI(pl)->clrFlag200();
    pl->pBody->waistMove();
    pl->partsWorldCalc();
    parts = pl->getPartsPtr(10);
    p0.x = 234.5f;
    p0.y = -24.0f;
    p0.z = 38.33f;
    PSMTXMultVec(parts->mat, &p0, &p0);
    p1.x = -50000.0f;
    p1.y = fRand1_1() * 200.0f;
    p1.z = fRand1_1() * 200.0f;
    PSMTXMultVecSR(parts->mat, &p1, &p1);
    PSVECAdd(&p0, &p1, &p1);
    PlWepHitCheck2(pl, &p0, &p1, pG->wep_no, 0, 6000.0f);
    pl->x3F4 = 1;
    pl->x3F0 = 1;
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
}

static void wep02_r3_fire10(cPlayer* pl)
{
    pl->motionMove();
    if (MotionCheckCrossFrame(&pl->mot, (f32) ((int) (u8) PlShotFrameTbl[pG->wep_no][pG->wep_lv_mag] - 2))) {
        if (pG->wep_no == 2) {
            WEP_ATARI(pl)->setFlag200();
        }
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 1;
        pl->xFF = 4;
    }
}

void wepDown(cPlayer* pl)
{
    cObjWep* obj;

    if (dmMotCk()) {
        int hokan;
        int frame;

        if (pl->xFF == 0) {
            hokan = 3;
            frame = 0;
        } else {
            hokan = 5;
            frame = 3;
        }
        MotionSetCore(pl, &pl->mot, WEP_ARC_PTR(0x24), (int) WEP_ARC_PTR(0x25), hokan, 5, frame);
        pl->xFC = 0;
        pl->xFD = 0;
        pl->xFE = 2;
        pl->xFF = 0;
    } else {
        pl->xFF = 1;
        pl->x4FD = 0xF;
        pl->xFC = 0;
        pl->xFD = 0;
        pl->xFE = 0;
        pl->x4FC = 0;
    }
    pl->motionMove();
    obj = WEP_OBJ(pl);
    obj->wep.mode = 3;
    obj->wep.step = 0;
    WEP_ATARI(pl)->clrFlag200();
    FSet(pl->rot.y, pl->rot.y - pl->pWaist->set(0.0f, 0.4f));
}

static void wep02_r2_reload(cPlayer* pl)
{
    u8 step = pl->xFF;
    cObjWep* obj;
    void* mot;

    switch (step) {
    case 0:
        switch (pG->x4FBA) {
        default:
            mot = WEP_ARC_PTR(0x2D);
            break;
        case 1:
            mot = WEP_ARC_PTR(0x2E);
            break;
        case 2:
            mot = WEP_ARC_PTR(0x2F);
            break;
        }
        MotionSetCore(pl, &pl->mot, mot, 0, 3, 1, 0);
        pl->motionMove();
        pl->xFF = 1;
        obj = WEP_OBJ(pl);
        obj->wep.mode = 4;
        obj->wep.step = 0;
        break;
    case 1:
        if (m3r[0] < -0.1f || m3r[0] > 0.1f) {
            if (pl->mot.frame >= PlReloadEndTbl[pG->wep_no][pG->x4FBA]) {
                if (joyKamae()) {
                    pl->xFF = 2;
                } else if (pl->flags_420 & 0x40) {
                    pl->xFC = 0;
                    pl->xFE = 0;
                    pl->xFD = 0x11;
                    pl->xFF = 0;
                } else {
                    wepDown(pl);
                    pl->xFF = step;
                }
            }
        } else {
            if (joyKamae() == 0 && pl->mot.frame >= PlReloadEndTbl[pG->wep_no][pG->x4FBA]) {
                if (pl->flags_420 & 0x40) {
                    pl->xFC = 0;
                    pl->xFE = 0;
                    pl->xFD = 0x11;
                    pl->xFF = 0;
                } else {
                    wepDown(pl);
                    pl->xFF = step;
                }
            } else if (pl->frame >= (f32) (pl->frameMax - 1)) {
                pl->xFC = 0;
                pl->xFD = 6;
                pl->xFE = 1;
                pl->xFF = 0;
            }
        }
        pl->motionMove();
        break;
    case 2: {
        PlArc* arc = (PlArc*) pG->pWepArc;

        mot3.set(pl, PL_ARC_PTR(arc, 0x26), PL_ARC_PTR(arc, 0x27), PL_ARC_PTR(arc, 0x28), 0, 9, 0, 4, 0);
        pl->x3E0 = 0;
        pl->xFF = 3;
    }
    case 3:
        pl->x3E0++;
        if ((int) pl->x3E0 > 8) {
            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = 1;
            pl->xFF = 0;
        }
        mot3.move(m3r[0]);
        pl->motionMove();
        break;
    }
}

// The object's .data is 8-aligned in the original link (wep01: .data starts 4 bytes after the end of
// .rodata); the size is already a multiple of 8, so this only raises the section alignment.
asm(".section .data\n\t.balign 8\n\t.text");
