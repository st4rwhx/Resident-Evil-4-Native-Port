// game/pl_knife.cpp: player routine 2 (knife): ready stance, set (idle/turn), fire (slash + hit check), down.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "global.h"
#include "main.h"
#include "cam_ctrl.h"
#include "item.h"
#include "snd.h"
#include "esp.h"
#include "math_sub.h"

extern "C" {
int MotionMove(cModel* m, int flag);               // game/motion.cpp
int MotionCheckCrossFrame(void* work, f32 frame);  // game/motion.cpp
}

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

void knife_r3_ready00(cPlayer* pl);
void knife_r3_ready10(cPlayer* pl);
void knife_r3_set00(cPlayer* pl);
void knife_r3_set10(cPlayer* pl);
void knife_r3_set20(cPlayer* pl);
void knife_r3_set30(cPlayer* pl);
static void knife_r3_set40(cPlayer* pl);
void hitCheck(cPlayer* pl, int no, u32 flag);
void knife_r3_fire00(cPlayer* pl);
void knife_r3_fire10(cPlayer* pl);
void knife_r3_down00(cPlayer* pl);
void knife_r3_down10(cPlayer* pl);

// face model info: the face blend weights (0x5C/0x70/0x84) are reset to `v`
#define FACE_SET(pl, v)                                 \
    do {                                                \
        cModelInfo* face = (pl)->pBody->pFace;          \
        if (VALID_PTR(face)) {                          \
            face->x84 = v;                              \
            face->x70 = v;                              \
            face->x5C = v;                              \
        }                                               \
    } while (0)

void PlKnifeMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        knife_r2_ready,
        knife_r2_set,
        knife_r2_fire,
        knife_r2_down,
    };

    func_tbl[pl->xFE](pl);
    pl->pWep->lockMove();
    pl->checkXbutton();
}

void knife_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        knife_r3_ready00,
        knife_r3_ready10,
    };

    pl->x3E0 = 0;
    if (pl->xFF == 100) {
        pl->xFF = 0;
        pl->x3E0 = 1;
    }
    if (Key.on & 1) {
        if (pl->pWep->knifeStance != 0) {
            pl->pWep->knifeStance = 0;
        }
    } else if (Key.on & 2) {
        if (pl->pWep->knifeStance != 2) {
            pl->pWep->knifeStance = 2;
        }
    } else {
        if (pl->pWep->knifeStance != 1) {
            pl->pWep->knifeStance = 1;
        }
    }
    func_tbl[pl->xFF](pl);
    if (joyLKamae() == 0 && pl->xFF != 3) {
        setWepTrans(pl, 1);
        FACE_SET(pl, 0.0f);
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
    }
}

void knife_r3_ready00(cPlayer* pl)
{
    f32 pitch;
    void* mot0;
    void* mot1;

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
    pl->pNeck->init(0, 0, 0);
    if ((G_WEP_ID & 0xFFFF0000) == 0x0D020000) {
        mot0 = pl->pMotTbl[0x59];
        mot1 = pl->pMotTbl[0x5A];
    } else if (ItemMgr.bulletNum() && (Key.on & 0x10) && pG->wep_no == 0xD) {
        mot0 = pl->pMotTbl[0x55];
        mot1 = pl->pMotTbl[0x56];
    } else {
        mot0 = PL_ARC_PTR(pG->pPlArc, 0x23);
        mot1 = PL_ARC_PTR(pG->pPlArc, 0x24);
    }
    mot3.set(pl, mot0, mot0, mot0, (int) mot1, 3, 0, 4, 0);
    mot3.move(m3r[0]);
    pl->motionMove();
    lockCtr = 0;
    pl->xFF = 1;
}

void knife_r3_ready10(cPlayer* pl)
{
    if (MotionCheckCrossFrame(&pl->pMotion, 4.0f)) {
        setWepTrans(pl, 0);
        FACE_SET(pl, 1.0f);
    }
    if ((G_WEP_ID & 0xFFFF0000) == 0x0D020000) {
        if (MotionCheckCrossFrame(&pl->pMotion, 10.0f)) {
            ((cObjLauncher*) pl->pWep->pObj)->gripBack();
            pl->xFE = 1;
            pl->xFF = 4;
        }
    } else {
        if (pl->frame >= 10.0f) {
            pl->xFE = 1;
            pl->xFF = 4;
        }
    }
    mot3.move(m3r[0]);
    pl->motionMove();
}

void knife_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        knife_r3_set00,
        knife_r3_set10,
        knife_r3_set20,
        knife_r3_set30,
        knife_r3_set40,
    };

    func_tbl[pl->xFF](pl);
    if (pl->xFF != 4) {
        PlWepLockCtrl(pl);
    }
    if (joyLKamae() == 0) {
        if (pl->flags_420 & 0x40) {
            pl->xFC = 0;
            pl->xFE = 0;
            pl->xFD = 0x11;
            pl->xFF = 0;
        } else if (pG->wep_no == 0xD && joyKamae()) {
            pl->xFC = 0;
            pl->xFF = 2;
            pl->xFE = 0;
            pl->xFD = 6;
        } else {
            pl->xFC = 0;
            pl->xFD = 0xB;
            pl->xFE = 3;
            pl->xFF = 0;
        }
    } else if (joyFireTrg() || joyFireOn()) {
        pl->xFE = 2;
        pl->xFF = 0;
    }
}

void knife_r3_set00(cPlayer* pl)
{
    PlArc* arc = pG->pPlArc;

    mot3.set(pl, PL_ARC_PTR(arc, 0x81), PL_ARC_PTR(arc, 0x83), PL_ARC_PTR(arc, 0x85), 0, 3, 0, 4, 0);
    mot3.move(m3r[0]);
    pl->motionMove();
    pl->xFF = 1;
}

void knife_r3_set10(cPlayer* pl)
{
    pl->motionMove();
}

void knife_r3_set20(cPlayer* pl)
{
    if ((Key.on & 4) == 0) {
        pl->xFF = 0;
    }
    MotionMove(pl, 0);
    if (pl->frame > 9.7f && pl->frame < 10.3f) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->worldPos, 0, 0, 0);
    }
    if (pl->frame > 22.7f && pl->frame < 23.3f) {
        SndCall(5, 1, &pl->getPartsPtr(0x18)->worldPos, 0, 0, 0);
    }
}

void knife_r3_set30(cPlayer* pl)
{
    if ((Key.on & 8) == 0) {
        pl->xFF = 0;
    }
    MotionMove(pl, 0);
    if (pl->frame > 9.7f && pl->frame < 10.3f) {
        SndCall(5, 0, &pl->getPartsPtr(0x14)->worldPos, 0, 0, 0);
    }
    if (pl->frame > 22.7f && pl->frame < 23.3f) {
        SndCall(5, 1, &pl->getPartsPtr(0x18)->worldPos, 0, 0, 0);
    }
}

static void knife_r3_set40(cPlayer* pl)
{
    if (MotionMove(pl, 0) || (Key.on & 0x10F)) {
        pl->xFF = 0;
    }
}

void knife_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        knife_r3_fire00,
        knife_r3_fire10,
    };

    func_tbl[pl->xFF](pl);
    PlWepLockCtrl(pl);
}

void hitCheck(cPlayer* pl, int no, u32 flag)
{
    static Vec ohpos;
    Vec p0;
    Vec p1;
    f32 len;
    cModel* parts;

    switch (pG->x4FB8) {
    case 0:
    default:
        len = 750.0f;
        break;
    case 4:
        len = 1200.0f;
        break;
    }
    parts = pl->getPartsPtr(2);
    p0.x = 0.0f;
    p0.y = 0.0f;
    p0.z = 0.0f;
    PSMTXMultVec(parts->mat, &p0, &p0);
    parts = pl->getPartsPtr(9);
    p1.x = -len;
    p1.y = 0.0f;
    p1.z = 0.0f;
    PSMTXMultVec(parts->mat, &p1, &p1);
    if (!(flag & 8)) {
        Vec d;

        PSVECSubtract(&p1, &ohpos, &d);
        PSVECScale(&d, &d, 0.2f);
        PSVECAdd(&ohpos, &d, &ohpos);
        PlWepHitCheck2(pl, &p0, &ohpos, 0x10, flag | 1, 6000.0f);
        PSVECAdd(&ohpos, &d, &ohpos);
        PlWepHitCheck2(pl, &p0, &ohpos, 0x10, flag | 1, 6000.0f);
        PSVECAdd(&ohpos, &d, &ohpos);
        PlWepHitCheck2(pl, &p0, &ohpos, 0x10, flag | 1, 6000.0f);
        PSVECAdd(&ohpos, &d, &ohpos);
        PlWepHitCheck2(pl, &p0, &ohpos, 0x10, flag | 1, 6000.0f);
    }
    ohpos = p1;
    flag &= ~8;
    PlWepHitCheck2(pl, &p0, &p1, 0x10, flag | 1, 6000.0f);
    p0.y += 200.0f;
    p1.y += 400.0f;
    PlWepHitCheck2(pl, &p0, &p1, 0x10, flag | 1, 6000.0f);
    p0.y -= 300.0f;
    p1.y -= 600.0f;
    PlWepHitCheck2(pl, &p0, &p1, 0x10, flag | 1, 6000.0f);
}

void knife_r3_fire00(cPlayer* pl)
{
    PlArc* arc = pG->pPlArc;

    mot3.set(pl, PL_ARC_PTR(arc, 0x82), PL_ARC_PTR(arc, 0x84), PL_ARC_PTR(arc, 0x86), 0, 3, 0, 4, 0);
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    MotionMove(pl, 0);
    pl->pWaist->set(pl->x400, 0.4f);
    EstSet((int) pl, -1, 0, 0, 0, 0x2B, 0, 0xA, 0, 0);
    pl->pBody->waistMove();
    pl->partsWorldCalc();
    pl->xFF = 1;
}

void knife_r3_fire10(cPlayer* pl)
{
    f32 st = 6.0f;   // unused: only order the constant pool (6.0, 10.0 before 3.0)
    f32 ed = 10.0f;
    f32 wh;

    if (MotionCheckCrossFrame(&pl->pMotion, 3.0f)) {
        SndCall(1, 3, &pl->getPartsPtr(4)->worldPos, 0, 0, 0);
    }
    pl->motionMove();
    if (pl->frame >= 6.0f && pl->frame <= 10.0f) {
        u32 flag = 0;

        if (!(pl->frame >= 7.0f && pl->frame <= 8.0f)) {
            flag = 4;
        }
        if (pl->frame > 5.7f && pl->frame < 6.3f) {
            flag |= 8;
        }
        hitCheck(pl, (int) (pl->frame - 6.0f), flag);
    }
    if (MotionCheckCrossFrame(&pl->pMotion, 6.0f)) {
        Vec* pos = &pl->getPartsPtr(10)->worldPos;

        if (GetWaterHeight(pos, &wh) && pos->y < wh + 100.0f) {
            if ((G_ROOM_ID32 & 0xFFFF0000) == 0x010A0000 || (G_ROOM_ID32 & 0xFFFF0000) == 0x011A0000) {
                EstSet((int) pl, -1, 0, 0, 1, 0x25, 0, 0, (u32) pl, 0);
            } else {
                EstSet((int) pl, -1, 0, 0, 3, 0, 0, 0, (u32) pl, 0);
            }
            SndCall(1, 0x51, &pl->getPartsPtr(10)->worldPos, 0, 0, 0);
        }
    }
    if (pl->frame >= (f32) (pl->frameMax - 2)) {
        pl->xFE = 1;
        pl->xFF = 4;
    }
}

void knife_r2_down(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        knife_r3_down00,
        knife_r3_down10,
    };

    func_tbl[pl->xFF](pl);
    FSet(pl->rot.y, pl->rot.y - pl->pWaist->set(0.0f, 0.4f));
    BitOn(pG->flags_500C, 0x2000000);
    pl->checkCtrl();
}

void knife_r3_down00(cPlayer* pl)
{
    void* mot0;
    void* mot1;

    if (joyKamae()) {
        cObjWep* obj = pl->pWep->pObj;

        int on = 1;

        obj->wep.mode = on;
        obj->wep.step = 0;
        mot0 = pl->pMotTbl[0x57];
        mot1 = pl->pMotTbl[0x58];
        pl->x3E0 = on;
    } else {
        if (dmMotCk()) {
            mot0 = pl->pMotTbl[0x5B];
            mot1 = pl->pMotTbl[0x5C];
        } else {
            mot0 = PL_ARC_PTR(pG->pPlArc, 0x87);
            mot1 = 0;
        }
        pl->x3E0 = 0;
    }
    if (mot0 == 0) {
        FACE_SET(pl, 0.0f);
        setWepTrans(pl, 1);
        pl->xFC = 0;
        pl->xFD = 0;
        pl->xFE = 0;
        pl->xFF = 0;
    } else {
        pl->motionSet(mot0, 5, 0, ((G_WEP_ID & 0xFFFF0000) == 0x0E000000) ? 0x100 : 0, (int) mot1);
        pl->motionMove();
        pl->xFF = 1;
    }
}

// back to routine 0 (both exits of down10 share this tail)
#define KNIFE_RESET(pl)   \
    do {                  \
        (pl)->xFC = 0;    \
        (pl)->xFD = 0;    \
        (pl)->xFE = 0;    \
        (pl)->xFF = 0;    \
    } while (0)

void knife_r3_down10(cPlayer* pl)
{
    if (MotionCheckCrossFrame(&pl->pMotion, 4.0f)) {
        FACE_SET(pl, 0.0f);
        setWepTrans(pl, 1);
    }
    if (MotionCheckCrossFrame(&pl->pMotion, 15.0f)) {
        if ((G_WEP_ID & 0xFFFF0000) == 0x0D020000) {
            ((cObjLauncher*) pl->pWep->pObj)->grip(0);
        }
    }
    if (pl->motionMove()) {
        if (pl->x3E0 == 0) {
            KNIFE_RESET(pl);
        } else {
            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = 1;
            pl->xFF = 0;
            pl->pWep->pitch = 0.0f;
            m3r[1] = 0.0f;
            m3r[0] = 0.0f;
        }
    } else if ((Key.on & 0x10F) || (pl->x3E0 != 0 && joyKamae() == 0) || (pl->x3E0 == 0 && joyKamae() != 0)) {
        FACE_SET(pl, 0.0f);
        setWepTrans(pl, 1);
        if ((G_WEP_ID & 0xFFFF0000) == 0x0D020000) {
            ((cObjLauncher*) pl->pWep->pObj)->grip(0);
        }
        KNIFE_RESET(pl);
    }
}

void setWepTrans(cPlayer* pl, int on)
{
    switch (pG->wep_no) {
    default:
        pl->pWep->pObj->setDisp(1, on);
        break;
    case 0x13:
    case 0x16:
    case 0x17:
    case 0x19:
    case 0x1F:
    case 0x20:
        pl->pWep->pObj2->setDisp(1, on);
        break;
    case 0xD:
        break;
    }
}
