// Grenade player routines (the pl_grenade object of the hand grenade modules wep19/30/41/42/45,
// byte-identical in all five; real file name unknown): routine 2 of the player while a throwable is
// equipped: ready (draw + aim, stance by the up/down keys), set (idle / turn), fire (throw), down
// and next target (routine 5). Modelled on game/pl_knife.cpp / wep/pl_handgun.cpp.

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
#include "item.h"
#include "math_sub.h"

extern "C" {
f64 atan2(f64 y, f64 x);
// motion.h declares the one-argument MotionMove; the routines pass a second argument (pl_knife.cpp).
int MotionMoveI(cModel* m, int flag) asm("MotionMove");
}

#define WEP_ARC_PTR(no) PL_ARC_PTR((PlArc*) pG->pWepArc, no)

// game/objSubWep.cpp: the thrown grenade / egg objects (init only, the module never touches the rest)
class cSubWep : public cObj {
public:
    int init(Vec* rot, f32 power);
};

static void wep19_r2_ready(cPlayer* pl);
static void wep19_r3_ready00(cPlayer* pl);
static void wep19_r3_ready10(cPlayer* pl);
static void wep19_r3_ready20(cPlayer* pl);
static void wep19_r3_ready30(cPlayer* pl);
static void wep19_r2_set(cPlayer* pl);
static void wep19_r3_set00(cPlayer* pl);
static void wep19_r3_set10(cPlayer* pl);
static void wep19_r3_set20(cPlayer* pl);
static void wep19_r3_set30(cPlayer* pl);
static void wep19_r3_set40(cPlayer* pl);
static void wep19_r2_fire(cPlayer* pl);
static void wep19_r3_fire00(cPlayer* pl);
static void wep19_r3_fire10(cPlayer* pl);
static void wepDown(cPlayer* pl);
static void wep19_r2_next(cPlayer* pl);
void readyWeapon(cPlayer* pl);
void itemThrow(cPlayer* pl);

// ready30 (the turn towards the lock target): positions the player is pulled to / turned to.
static Vec pos;
static Vec tgt;

void PlGrenadeMove(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep19_r2_ready,
        wep19_r2_set,
        wep19_r2_fire,
        wepDown,
        0,
        wep19_r2_next,
    };

    pl->pWep->lockMove();
    if (pl->xFE == 4) {
        pLog->err(0, 0, "ERROR:grenade cant reload action!!!!!");
        pl->xFE = 0;
    }
    func_tbl[pl->xFE](pl);
}

static void wep19_r2_ready(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep19_r3_ready00,
        wep19_r3_ready10,
        wep19_r3_ready20,
        wep19_r3_ready30,
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

static void wep19_r3_ready00(cPlayer* pl)
{
    f32 pitch;
    void* mot;

    pl->x3E4 = 0;
    pl->pWep->x2C = 0.0f;
    pl->pWep->x30 = CamCtrl.getCameraDirection();
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
    pl->pWep->lockInit();
    mot = WEP_ARC_PTR(0xF);
    mot3.set(pl, mot, mot, mot, 0, 3, 0, 4, 0);
    mot3.move(m3r[0]);
    m3r[0] = 0.0f;
    m3r[1] = 0.0f;
    m3r[2] = 0.0f;
    SndCall(1, 0x28, &pl->pParts->worldPos, 0, 0, 0);
    lockCtr = 0;
    pl->xFF = 1;
}

static void wep19_r3_ready10(cPlayer* pl)
{
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

static void wep19_r3_ready20(cPlayer* pl)
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

// ready30: turn / step towards the aim target while the motion plays (the pl_handgun form: `t` is
// assigned after the Muku call and lives across GetDistance3, m3r is walked through `r`, the
// first m3r[0] store is a direct reference, the clamp bounds are variables).
static void wep19_r3_ready30(cPlayer* pl)
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

static void wep19_r2_set(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep19_r3_set00,
        wep19_r3_set10,
        wep19_r3_set20,
        wep19_r3_set30,
        wep19_r3_set40,
    };

    func_tbl[pl->xFF](pl);
    PlWepLockCtrl(pl);
    if (lockCtr != 0) {
        lockCtr--;
    }
    if (joyKamae() == 0) {
        if (pl->flags_420 & 0x40) {
            pl->xFC = 0;
            pl->xFE = 0;
            pl->xFD = 0x11;
            pl->xFF = 0;
        } else {
            int md = 3;

            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = md;
            pl->xFF = 0;
        }
    } else if ((joyFireTrg() || joyFireOn()) && ItemMgr.bulletNum()) {
        pl->xFC = 0;
        pl->xFD = 6;
        pl->xFE = 2;
        pl->xFF = 0;
    }
}

static void wep19_r3_set00(cPlayer* pl)
{
    PlArc* arc = (PlArc*) pG->pWepArc;

    mot3.set(pl, PL_ARC_PTR(arc, 0x11), PL_ARC_PTR(arc, 0x14), PL_ARC_PTR(arc, 0x17), 0, 3, 0, 4, 0);
    mot3.move(m3r[0]);
    pl->motionMove();
    pl->xFF = 1;
}

static void wep19_r3_set10(cPlayer* pl)
{
    pl->motionMove();
}

// Two never-called routines the original linker dropped: only their constant pools (200, 500, 300,
// 0 | 30.000002, 283.5, 20, 0.1, 1, -0.2, 0.4, 0, -PI/4) and the static Vec survive (STRIP_UNUSED).
static void wep19_r3_set50(cPlayer* pl)
{
    Vec p;

    p.x = 200.0f;
    p.y = 500.0f;
    p.z = 300.0f;
    if (pl->frame != 0.0f) {
        PSMTXMultVec(pl->mat, &p, &p);
        pl->setPos(&p);
    }
}

static void wep19_r3_set60(cPlayer* pl)
{
    static Vec rot = {-0.2617994f, 0.0f, 0.0f};
    f32 d;

    d = pl->frame * 30.000002f + 283.5f;
    d = d * 20.0f + 0.1f;
    rot.y = (1.0f - d) + -0.2f;
    rot.z = d * 0.4f;
    if (rot.z != 0.0f) {
        rot.x = -PI / 4.0f;
    }
    pl->setAng(&rot);
}

static void wep19_r3_set20(cPlayer* pl)
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

static void wep19_r3_set30(cPlayer* pl)
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

static void wep19_r3_set40(cPlayer* pl)
{
    if (MotionMoveI(pl, 0)) {
        pl->xFF = 0;
    }
}

static void wep19_r2_fire(cPlayer* pl)
{
    static void (*func_tbl[])(cPlayer*) = {
        wep19_r3_fire00,
        wep19_r3_fire10,
    };

    func_tbl[pl->xFF](pl);
    if (joyKamae()) {
        PlWepLockCtrl(pl);
    }
}

static void wep19_r3_fire00(cPlayer* pl)
{
    if (ItemMgr.bulletNum() > 1) {
        PlArc* arc = (PlArc*) pG->pWepArc;

        mot3.set(pl, PL_ARC_PTR(arc, 0x12), PL_ARC_PTR(arc, 0x15), PL_ARC_PTR(arc, 0x18), 0, 3, 0, 4, 0);
        pl->x3F0 = 0;
    } else {
        PlArc* arc = (PlArc*) pG->pWepArc;

        mot3.set(pl, PL_ARC_PTR(arc, 0x13), PL_ARC_PTR(arc, 0x16), PL_ARC_PTR(arc, 0x19), 0, 3, 0, 4, 0);
        pl->x3F0 = 1;
    }
    mot3.move(m3r[0]);
    MotionMoveI(pl, 0);
    SndCall(1, 1, &pl->pParts->worldPos, 0, 0, 0);
    m3r[0] = m3r[0] * m3r[2] + m3r[1] * (1.0f - m3r[2]);
    mot3.move(m3r[0]);
    pl->pWaist->set(pl->x400, 0.4f);
    pl->xFF = 1;
}

static void wep19_r3_fire10(cPlayer* pl)
{
    const f32 throwFrame = 5.0f;

    pl->motionMove();
    if (MotionCheckCrossFrame(&pl->mot, throwFrame)) {
        itemThrow(pl);
        if (ItemMgr.bulletNum()) {
            pl->pWep->pObj2->setDisp(1, 0);
        } else {
            pl->pWep->pObj2->setDisp(0, 0);
        }
    }
    if (pl->x3F0 == 0) {
        if (pl->frame > 24.7f && pl->frame < 25.3f) {
            readyWeapon(pl);
            SndCall(1, 0, &pl->pParts->worldPos, 0, 0, 0);
        }
        if (pl->frame >= 30.0f) {
            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = 1;
            pl->xFF = 4;
        }
    } else {
        if (pl->frame >= 15.0f) {
            pl->setRightHand(1);
            if (pl->flags_420 & 0x40) {
                pl->xFC = 0;
                pl->xFE = 0;
                pl->xFD = 0x11;
                pl->xFF = 0;
            } else {
                pl->xFC = 0;
                pl->xFD = 0;
                pl->xFE = 2;
                pl->xFF = 0;
            }
        }
    }
}

static void wepDown(cPlayer* pl)
{
    if (dmMotCk()) {
        MotionSetCore(pl, &pl->mot, WEP_ARC_PTR(0x10), 0, 3, 5, 0);
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
    FSet(pl->rot.y, pl->rot.y - pl->pWaist->set(0.0f, 0.4f));
}

static void wep19_r2_next(cPlayer* pl)
{
    cModel* em = pl->pLockEm;
    int n;

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
        n = pl->x3E0++;
        if (n > 9) {
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

            pl->xFC = 0;
            pl->xFD = 6;
            pl->xFE = md;
            pl->xFF = 0;
        }
    }
}

// Shows the grenade in the hand again after a throw (the next one, while any is left).
void readyWeapon(cPlayer* pl)
{
    pl->pWep->pObj2->setDisp(1, 1);
    if (!(pG->flags_68 & 0x00400000) && ItemMgr.bulletNum() == 1) {
        pl->pWep->pObj->setDisp(0, 0);
    }
}

// Throws the equipped item: creates the sub weapon object of the weapon number and launches it
// along the aim pitch (m3r[0]).
void itemThrow(cPlayer* pl)
{
    cObj* obj;
    int id;

    switch (pG->wep_no) {
    case 0x13:
    case 0x1E:
    case 0x29:
    default:
        id = 0x1A;
        break;
    case 0x16:
        id = 0x29;
        break;
    case 0x17:
    case 0x2A:
        id = 0x2A;
        break;
    case 0x19:
    case 0x1F:
    case 0x20:
        id = 0x3A;
        break;
    }
    obj = ObjMgr.createBack(id);
    if (obj == 0) {
        pLog->err(0, 0, "itemThrow() grenade work alloc failed.");
        return;
    }
    switch (pG->wep_no) {
    case 0x13:
        obj->type = 0;
        break;
    case 0x16:
        obj->type = 1;
        break;
    case 0x17:
        obj->type = 2;
        break;
    case 0x19:
        obj->type = 3;
        break;
    case 0x1F:
        obj->type = 4;
        break;
    case 0x20:
        obj->type = 5;
        break;
    }
    if (((cSubWep*) obj)->init(&pl->rot, m3r[0]) == 0) {
        pLog->err(0, 0, "itemThrow() init failed.");
    }
    ItemMgr.trigger();
}
