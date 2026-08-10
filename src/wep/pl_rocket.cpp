// Rocket launcher player routines (wep13 module, first object; real file name unknown): routine 2
// of the player while the launcher is equipped: ready (grip + aim), set (idle / turn), fire, down,
// next target (routine 5) and throw away (routine 6). Modelled on game/pl_knife.cpp.

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
#include "math_sub.h"

// motion.h declares the one-argument MotionMove; the routines pass a second argument (pl_knife.cpp).
int MotionMoveI(cModel* m, int flag) asm("MotionMove");

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)
#define WEP_ARC_PTR(no) PL_ARC_PTR((PlArc*) pG->pWepArc, no)
#define LAUNCHER(pl) ((cObjLauncher*) (pl)->pWep->pObj)

// Routine bytes through int parameters (player.cpp PlRoutineSet): SImode constants that the
// preceding QImode byte stores (`wep.step = 0` in r2_throw) do not share.
static inline void PlRoutineSet(cPlayer* pl, int r0, int r1, int r2, int r3)
{
    pl->xFC = r0;
    pl->xFD = r1;
    pl->xFE = r2;
    pl->xFF = r3;
}


// face model info: the face blend weights (0x5C/0x70/0x84) are reset to `v`
// (a plain block: a do/while(0) body's loop notes lengthen the live ranges around it and flip
// the callee-saved order of down30's pl / joyLKamae result)
#define FACE_SET(pl, v)                                 \
    {                                                   \
        cModelInfo* face = (pl)->pBody->pFace;          \
        if (VALID_PTR(face)) {                          \
            face->x84 = v;                              \
            face->x70 = v;                              \
            face->x5C = v;                              \
        }                                               \
    }

u8 lockCtr = 0;

static void wep13_r2_ready(cPlayer* pl);
static void wep13_r3_ready00(cPlayer* pl);
static void wep13_r3_ready10(cPlayer* pl);
static void wep13_r3_ready20(cPlayer* pl);
static void wep13_r3_ready30(cPlayer* pl);
static void wep13_r2_set(cPlayer* pl);
static void wep13_r3_set00(cPlayer* pl);
static void wep13_r3_set10(cPlayer* pl);
static void wep13_r3_set20(cPlayer* pl);
static void wep13_r3_set30(cPlayer* pl);
static void wep13_r3_set40(cPlayer* pl);
static void wep13_r2_fire(cPlayer* pl);
static void wep13_r3_fire00(cPlayer* pl);
static void wep13_r3_fire10(cPlayer* pl);
static void wep13_r2_down(cPlayer* pl);
static void wep13_r3_down00(cPlayer* pl);
static void wep13_r3_down10(cPlayer* pl);
static void wep13_r3_down20(cPlayer* pl);
static void wep13_r3_down30(cPlayer* pl);
static void wep13_r2_throw(cPlayer* pl);
static void wep13_r2_next(cPlayer* pl);

void PlRocketMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep13_r2_ready,
        wep13_r2_set,
        wep13_r2_fire,
        wep13_r2_down,
        0,
        wep13_r2_next,
        wep13_r2_throw,
    };

    func_tbl[pl->xFE](pl);
    pl->pWep->lockMove();
}

static void wep13_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep13_r3_ready00,
        wep13_r3_ready10,
        wep13_r3_ready20,
        wep13_r3_ready30,
    };

    func_tbl[pl->xFF](pl);
    if (joyKamae() == 0 && pl->xFF != 3) {
        LAUNCHER(pl)->grip(0);
        pl->pWep->pObj->resetMotion();
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
    }
    if (pl->pLockEm) {
        CamCtrlShoulderSetAim(&pl->pLockEm->pos);
    } else {
        Vec aim = {0.0f, 1000.0f, 10000.0f};
        Vec hit;

        PSMTXMultVec(pl->mat, &aim, &aim);
        SatMgr.hitCheck(&pl->getPartsPtr(0)->worldPos, &aim, &hit, 0, 0, 0);
        CamCtrlShoulderSetAim(&hit);
    }
}

static void wep13_r3_ready00(cPlayer* pl)
{
    f32 pitch;
    void* mot;

    pl->x3E4 = 0;
    pl->pWep->x2C = 0.0f;
    pitch = CamCtrl.getCameraPitch();
    if (pitch > 0.0f) {
        pitch += pitch;
    }
    pl->pWep->pitch = pitch;
    m3r[2] = 0.0f;
    pitch *= 2.0f / PI;
    m3r[1] = pitch;
    m3r[0] = pitch;
    pl->x400 = 0.0f;
    pl->pWep->x30 = CamCtrl.getCameraDirection();
    pl->pNeck->init(0, 0, 0);
    pl->pWep->lockInit();
    mot = WEP_ARC_PTR(0x18);
    mot3.set(pl, mot, mot, mot, 0, 3, 0, 4, 0);
    mot3.move(m3r[0]);
    m3r[0] = 0.0f;
    m3r[1] = 0.0f;
    m3r[2] = 0.0f;
    lockCtr = 0;
    if (pl->flags_420 & 0x400) {
        pl->pWep->pObj->setDisp(0, 1);
        pl->flags_420 &= ~0x400;
        pl->pWep->pObj->setMotion(pl);
    }
    if (pG->wep_type != 2) {
        LAUNCHER(pl)->gripBack();
        pl->pWep->pObj->motionSet(WEP_ARC_PTR(0x20), 0, 0, 1, 0);
    }
    pl->xFF = 1;
}

static void wep13_r3_ready10(cPlayer* pl)
{
    if (pl->frame < 4.0f) {
        f32 d = pl->pWep->x30 / (4.0f - pl->frame);

        pl->rot.y += d;
        pl->pWep->x30 -= d;
    }
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    pl->pWaist->set(0.0f, 0.4f);
    pl->motionMove();
    if (MotionCheckCrossFrame(&pl->mot, 11.0f)) {
        LAUNCHER(pl)->grip(1);
    }
    if (MotionCheckCrossFrame(&pl->mot, 32.0f)) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 1;
        pl->xFF = 0;
    }

}

static void wep13_r3_ready20(cPlayer* pl)
{
    pl->motionSet(pl->pMotTbl[0x57], 5, 0, 0, (int) pl->pMotTbl[0x58]);
    pl->motionMove();
    pl->xFF = 3;
}

static void wep13_r3_ready30(cPlayer* pl)
{
    if (MotionCheckCrossFrame(&pl->mot, 5.0f)) {
        FACE_SET(pl, 0.0f);
    }
    if (MotionCheckCrossFrame(&pl->mot, 14.0f)) {
        LAUNCHER(pl)->grip(1);
    }
    if (MotionCheckCrossFrame(&pl->mot, 39.0f)) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 1;
        pl->xFF = 0;
    }
    pl->motionMove();
}

// r2_set: the launcher line copy of `to` reads the frame directly (`lwz 0x18(r1)..0x20(r1)`) while
// `from` (frame offset 0) goes through an address register; a plain `obj->launcher.to = to` after
// `getTrajectory(&from, &to)` makes cse reuse the call's `&to` pseudo for the copy and gcse PRE
// hoists it into a callee-saved register. The copy through an inline taking the address by pointer
// keeps the frame-relative loads.
static inline void VecCopy(Vec* d, const Vec* s)
{
    *d = *s;
}

static void wep13_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep13_r3_set00,
        wep13_r3_set10,
        wep13_r3_set20,
        wep13_r3_set30,
        wep13_r3_set40,
    };

    func_tbl[pl->xFF](pl);
    pl->setLaserSight(0, 0);
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
    } else if (joyFireOn() && pl->pWep->pObj->bulletNum()) {
        Vec from;
        Vec to;
        cObjLauncher* obj;

        CameraMove();
        CamCtrl.getTrajectory(&from, &to);
        obj = LAUNCHER(pl);
        obj->launcher.from = from;
        VecCopy(&obj->launcher.to, &to);
        pl->endCamera();
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 2;
        pl->xFF = 0;
    }
}

static void wep13_r3_set00(cPlayer* pl)
{
    PlArc* arc = (PlArc*) pG->pWepArc;

    mot3.set(pl, PL_ARC_PTR(arc, 0xF), PL_ARC_PTR(arc, 0x12), PL_ARC_PTR(arc, 0x14), 0, 3, 0, 4, 0);
    mot3.move(m3r[0]);
    pl->motionMove();
    pl->pWep->pObj->setDisp(1, 0);
    SndCall(2, 9, &pl->pParts->worldPos, 0, 0, 0);
    if (!(pl->flags_420 & 0x10)) {
        CamCtrl.startScope(0, 0);
        pl->flags_420 |= 0x10;
    }
    pl->xFF = 1;
}

static void wep13_r3_set10(cPlayer* pl)
{
    MotionMoveI(pl, 0);
}

static void wep13_r3_set20(cPlayer* pl)
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

static void wep13_r3_set30(cPlayer* pl)
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

static void wep13_r3_set40(cPlayer* pl)
{
    if (MotionMoveI(pl, 0) || (Key.on & 0x10F)) {
        pl->xFF = 0;
    }
}

static void wep13_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep13_r3_fire00,
        wep13_r3_fire10,
    };

    func_tbl[pl->xFF](pl);
}

static void wep13_r3_fire00(cPlayer* pl)
{
    PlArc* arc;
    cObjWep* obj;
    f32 pitch;

    pl->pWep->pObj->trigger();
    m3r[1] = 0.0f;
    m3r[0] = 0.0f;
    arc = (PlArc*) pG->pWepArc;
    mot3.set(pl, PL_ARC_PTR(arc, 0x11), PL_ARC_PTR(arc, 0x13), PL_ARC_PTR(arc, 0x15), 0, 0, 0, 4, 0);
    mot3.move(m3r[0]);
    MotionMoveI(pl, 0);
    pl->pWep->pObj->setDisp(1, 1);
    obj = pl->pWep->pObj;
    obj->wep.mode = 2;
    obj->wep.step = 0;
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0, 1);
    pitch = m3r[0];
    PlWepLockRand(pl, 2, &pitch, &pl->x400);
    m3r[1] = pitch;
    if (m3r[2] == 0.0f) {
        m3r[0] = pitch;
    }
    m3r[1] = 0.0f;
    m3r[0] = 0.0f;
    pl->xFF = 1;
}

static void wep13_r3_fire10(cPlayer* pl)
{
    pl->motionMove();
    if (pG->wep_type != 2) {
        if (MotionCheckCrossFrame(&pl->mot, 39.0f)) {
            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = 6;
            pl->xFF = 0;
        }
        return;
    }
    if (joyKamae() == 0 && pl->frame >= 45.0f) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 3;
        pl->xFF = 0;
    }
    if (MotionGetState(pl)) {
        if (joyKamae()) {
            CamCtrl.startScope(0, 0);
            CameraMove();
            pl->flags_420 |= 0x10;
            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = 1;
            pl->xFF = 0;
        } else {
            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = 3;
            pl->xFF = 0;
        }
    }
}

static void wep13_r2_down(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep13_r3_down00,
        wep13_r3_down10,
        wep13_r3_down20,
        wep13_r3_down30,
        0,
    };

    func_tbl[pl->xFF](pl);
    FSet(pl->rot.y, pl->rot.y - pl->pWaist->set(0.0f, 0.4f));
    BitOn(pG->flags_500C, 0x2000000);
    pl->checkCtrl();
}

static void wep13_r3_down00(cPlayer* pl)
{
    CamCtrl.endScope();
    CameraMove();
    pl->flags_420 &= ~0x10;
    pl->pWep->pObj->setDisp(1, 1);
    if (joyLKamae()) {
        // `li r9,3` before the stack-argument `stw r0,8(r1)`: the two tie in sched2 (equal
        // priority and dependents), so sched1's issue order decides. A constant in a local
        // makes the r9 argument a copy of a dying pseudo (weight 0) that sched1 issues before
        // the `mr r4,pl` copy (+1) and before the stack store, which waits for its
        // anti-dependence on the pMotTbl loads; reload ties the pseudo to r9.
        u8 hokan = 3;
        void* mot0 = pl->pMotTbl[0x55];
        void* mot1 = pl->pMotTbl[0x56];

        mot3.set(pl, mot0, mot0, mot0, (int) mot1, hokan, 0, 4, 0);
        mot3.move(m3r[0]);
        // The dead loop's NOTE_INSN_LOOP_END ends cse's extended block, so the QImode store
        // below gets its own `li r0,3` instead of a subreg of `hokan` (which would keep the
        // constant in a callee-saved register across the calls).
        do { } while (0);
        pl->xFF = 3;
    } else {

        MotionSetCore(pl, &pl->mot, WEP_ARC_PTR(0x19), 0, 7, 5, 0);
        pl->xFF = 1;
    }
    pl->motionMove();
}

static void wep13_r3_down10(cPlayer* pl)
{
    const f32 gripFrame = 13.0f;
    const f32 seFrame = 18.0f;
    int end = pl->motionMove();

    if (dmMotCk() == 0 && pl->frame >= 15.0f) {
        pl->xFF = 1;
        pl->x4FD = 0xF;
        pl->xFC = 0;
        pl->xFD = 0;
        pl->xFE = 0;
        pl->x4FC = 0;
    } else if (end) {
        SndCall(5, 2, &pl->getPartsPtr(0x14)->worldPos, 0, 0, 0);
        pl->xFC = 0;
        pl->xFD = 0;
        pl->xFE = 0;
        pl->xFF = 0;
    }
    if (MotionCheckCrossFrame(&pl->mot, gripFrame)) {
        LAUNCHER(pl)->grip(0);
    }
    if (MotionCheckCrossFrame(&pl->mot, seFrame)) {
        SndCall(2, 2, &pl->pParts->worldPos, 0, 0, 0);
    }
    if (joyLKamae()) {
        LAUNCHER(pl)->grip(0);
        pl->xFC = 0;
        pl->xFD = 0xB;
        pl->xFE = 0;
        pl->xFF = 0;
    } else if (joyKamae()) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 0;
        pl->xFF = 0;
    } else if (Key.on & 0x10F) {
        LAUNCHER(pl)->grip(0);
        pl->xFC = 0;
        pl->xFD = 0;
        pl->xFE = 0;
        pl->xFF = 0;
    }
}

static void wep13_r3_down20(cPlayer* pl)
{
    MotionSetCore(pl, &pl->mot, pl->pMotTbl[0], 0, 3, 5, 0);
    pl->xFF = 1;
}

static void wep13_r3_down30(cPlayer* pl)
{
    if (MotionCheckCrossFrame(&pl->mot, 9.0f)) {
        if (pG->wep_type == 2) {
            LAUNCHER(pl)->gripBack();
        } else {
            LAUNCHER(pl)->grip(0);
        }
    }
    if (MotionCheckCrossFrame(&pl->mot, 13.0f)) {
        FACE_SET(pl, 1.0f);
    }
    if (joyLKamae() == 0) {
        LAUNCHER(pl)->grip(0);
        pl->xFC = 0;
        pl->xFD = 0;
        pl->xFE = 0;
        pl->xFF = 0;
    }
    if (pl->motionMove()) {
        SndCall(5, 2, &pl->getPartsPtr(0x14)->worldPos, 0, 0, 0);
        pl->xFC = 0;
        pl->xFD = 0xB;
        pl->xFE = 1;
        pl->xFF = 0;
    }
}

static void wep13_r2_throw(cPlayer* pl)
{
    switch (pl->xFF) {
    case 0:
        pl->motionSet(WEP_ARC_PTR(0x16), 7, 0, 1, 0);
        SndCall(2, 2, &pl->pParts->worldPos, 0, 0, 0);
        pl->xFF = 1;
    case 1:
        if (MotionCheckCrossFrame(&pl->mot, 18.0f)) {
            cObjWep* obj;

            pl->flags_420 |= 0x400;
            obj = pl->pWep->pObj;
            obj->wep.mode = 5;
            obj->wep.step = 0;
            pl->pWep->pObj->setMotion(pl);
            PlRoutineSet(pl, 0, 0, 2, 0);
        }
        pl->motionMove();
        break;
    }
}

static void wep13_r2_next(cPlayer* pl)
{
    cModel* em = pl->pLockEm;

    switch (pl->xFF) {
    case 0:
        pl->x3E0 = 0;
        pl->xFF = 1;
        pl->x3E4 = 0;
    case 1:
        if (GetDistance3(&pl->pos, &em->pos) > 200.0f) {
            pl->rot.y += Muku(&pl->pos, &em->pos, pl->rot.y, PI / 10.0f);
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        }
        pl->x400 = 0.0f;
        pl->pWaist->set(0.0f, 0.4f);
        pl->pBody->waistMove();
        pl->motionMove();
        if ((int) pl->x3E0++ > 9) {
            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = 1;
            pl->xFF = 0;
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

// The module's .data section is 8-aligned (the original linker's placement; the tables start at 4).
asm(".section .data\n\t.balign 8\n\t.text");
