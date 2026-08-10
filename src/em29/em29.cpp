// em29 module (D:/Bio4/Prog/em29.cpp): the bats. They hang from the ceiling or sit on the ground
// until the player comes close, fly around (em29_R1_Walk / Turn, em29SetSPeed blends the speed and
// keeps the bat between the floor and the ceiling), dash at the player (em29_R1_AtkDash) or rush him
// in a swarm (em29_R1_AtkRush, plem29_BatRush), and are pushed apart from each other and the player by
// em29ObaHitCk. Their sounds go through the room's ctrl11 / ctrl12 controls.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em29.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "route_ck.h"
#include "pad.h"
#include "player.h"
#include "pl_sub.h"
#include "snd.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");

typedef void (*Em29Func)(cEm29*);

static void em29_R0_Init(cEm29* em);
static void em29_R0_Move(cEm29* em);
static void em29_R1_WaitLand(cEm29* em);
static void em29_R1_WaitCeiling(cEm29* em);
static void em29_R1_Walk(cEm29* em);
static void em29_R1_Turn(cEm29* em);
static void em29_R1_AtkDash(cEm29* em);
static void em29_R1_AtkRush(cEm29* em);
static void em29_R0_Damage(cEm29* em);
static void em29_R1_Dm_Air(cEm29* em);
static void em29_R1_Dm_Ceiling(cEm29* em);
static void em29_R1_Dm_Land(cEm29* em);
static void em29_R1_Dm_Recovery(cEm29* em);
static void em29_R0_Die(cEm29* em);
static void em29_R1_Die_Normal(cEm29* em);
static void em29_R1_Die_Reset(cEm29* em);
static void em29_R1_Die_FadeOut(cEm29* em);
static void plem29_BatRush(cPlayer* pl);

#define ARC(no) PL_ARC_PTR(em->subArc, no)
#define PL_ARC(no) PL_ARC_PTR(pl->subArc, no)

// The enemy a player damage callback belongs to (pl_sub SetPlDamage's first argument).
#define PL_EM(pl) ((cEm*) (pl)->dmgType)

// Struct-member view of the player pointer: a load through it is not hoisted above the preceding
// stores through the work pointer (cam_ctrl.cpp PlayerPtr).
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

// math_sub.h's VECNormalize with the log pointer read as a plain struct member: the `lis pLog@ha`
// is not hoisted out of the scan loop (em27.cpp).
#define VECNormalizeP(src, dst)                                                         \
    if (0.0f == (src)->x && 0.0f == (src)->y && 0.0f == (src)->z) {                    \
        pLog.p->err(0, 0, "VECNormalize:[%s/%d]", __FILE__, __LINE__);                  \
        (dst)->x = (dst)->y = (dst)->z = 0.0f;                                          \
    } else                                                                              \
        PSVECNormalize(src, dst)

// Work `no` of the enemy manager with the range check kept (em.h's EmMgrWork lets jump threading
// fold it away inside the scan loops; the manager pointer local defeats it, db_light objWorkChkP).
static inline cEm* em29EmWork(u32 no)
{
    cEmMgr* m = &EmMgr;

    if (no >= m->nArray) {
        return 0;
    }
    return (cEm*) ((u8*) m->pArray + m->size * no);
}

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

// Damage / death routine per the wait state the bat was in (0: flying, 1: on the ceiling, 2: on the ground).
static inline void em29DmRoutineSet(cEm29* em, u32 kind)
{
    switch (kind) {
    case 0:
    default:
        EmRoutineSet(em, 2, 0, 0, 0);
        break;
    case 1:
        EmRoutineSet(em, 2, 1, 0, 0);   // cse stores the kind register for the 1
        break;
    case 2:
        EmRoutineSet(em, 2, 2, 0, 0);
        break;
    }
}

// The same routine set with the arms laid out 1, 2, default: the copy em29DmCk's dead `dmWep == 0x21`
// then-arm uses. Only the layout differs (the tree is the same, followed by `b default`); jump2 deletes
// this copy whole, but with the default body first it merges the arms in an order that leaves the
// then-copy's tree behind (see em29DmCk).
static inline void em29DmRoutineSetLate(cEm29* em, u32 kind)
{
    switch (kind) {
    case 1:
        EmRoutineSet(em, 2, 1, 0, 0);
        break;
    case 2:
        EmRoutineSet(em, 2, 2, 0, 0);
        break;
    case 0:
    default:
        EmRoutineSet(em, 2, 0, 0, 0);
        break;
    }
}

// The tail's routine set: `z` is the `zero` pseudo (target `li r29,0` at the dmg join) so the arms are
// register-distinct from the early set's (whose zero is the `em->hp = 0` register) and jump2 does not
// cross-jump the two sets into one.
static inline void em29DmRoutineSetZ(cEm29* em, u32 kind, int z)
{
    switch (kind) {
    case 0:
    default:
        EmRoutineSet(em, 2, z, z, z);
        break;
    case 1:
        EmRoutineSet(em, 2, 1, z, z);
        break;
    case 2:
        EmRoutineSet(em, 2, 2, z, z);
        break;
    }
}

extern "C" void _prolog()
{
    OSReport("em29 prolog Ok\n");
    EmInitFunc = Em29Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em29Init(cEm* em)
{
    new (em) cEm29();
}

void em29DmCk(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    u32 kind;
    int hit;
    int wep;
    int b1;
    int b3;
    int dmg;
    int zero;

    kind = 0;
    if (w->flags & 0x40) {
        kind = 1;
    }
    if (w->flags & 0x20) {
        kind = 2;
    }
    if ((em->be_flag & 2) && em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 2:
        case 6:
        case 8:
            em->hp = 0;
            Ctrl12CntAdd(w->pCtrl12, 2, 1);
            Ctrl11SetSe(w->pCtrl11, em, 1, 0x1C, 0xA);
            w->flags |= 0x80;
            em29DmRoutineSet(em, kind);
            return;
        }
    }
    hit = em->dmHit;
    if (hit == 0) {
        return;
    }
    wep = em->dmWep;
    // b3, b1, store: `hit` dies at b1 (weight 0), so sched1 ranks b1 above b3 and, by source order, above
    // the dmHit store: `rlwinm b1; stb dmHit; rlwinm b3` like the target (the other five orders differ).
    b3 = (hit >> 3) & 1;
    b1 = (hit >> 1) & 1;
    em->dmHit = 0;
    switch (wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x2B:
        dmg = (Rnd() & 1) + 999;
        break;
    case 7:
    case 8:
    case 0x21:
        dmg = 9999;
        if (em->plDist2 > 16000000.0f) {
            dmg = (Rnd() & 1) + 999;
        }
        break;
    // default-grouped nodes shape the tree (tools/research/casetree.py): 0xF gives the left half the weight
    // that keeps [0x10,0x11] the root, [0x2C,0x2D] makes 0x21 the right root; their compares fold
    // into `b default`. The default arm is written before case 0xE (bodies laid out A, X, D, Y).
    case 5:
    case 6:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        dmg = 9999;
        break;
    case 0xE:
        dmg = 0;
        break;
    }
    zero = 0;
    if (b1) {
        dmg <<= 2;
    }
    if (b3) {
        dmg = 9999;
    }
    LifeDownSet2(em, dmg, 0, 0);
    if (em->hp > 0) {
        EmDmBloodSet2(em, 0x21, 1, 0, 0, 0);
    }
    SndCall(8, 0xE, &em->pos, em->id, 0, em);
    if (em->hp > 0) {
        goto alive;
    }
    // The do-while doubles zero's ref weight so global alloc places it (r29) before `kind` (r28)
    // and `b3` (r26) -- with plain refs kind ranks above zero and the two swap registers.
    do {
        EstSet(0, -1, &em->pos, &em->rot, 0x21, 0, 0, 0, (u32) zero, (void*) zero);
        em->be_flag &= ~2;
        Ctrl12CntAdd(w->pCtrl12, 2, 1);
        em29LastCk(em);
        em29DmRoutineSetZ(em, kind, zero);
    } while (0);
    goto tail;
    // Dead loop in front of the `alive` label: its LOOP_END note stops cse from carrying `zero == 0`
    // into the hp > 0 arm, whose routine sets keep fresh `li r0,0`/`li r9,0` like the target.
    do {
    } while (0);
alive:
    // Dead test with identical arms: jump2 cross-jumps the then-copy into the else copy and the
    // surviving `lbz dmWep; cmpwi 0x21` is the target's dead compare. Two layout conditions make the
    // then-copy vanish whole: (1) the else copy's last arm must end in `b END` at jump2 entry -- the
    // `return` jumps over the `tail:` block whose store is dead (deleted in flow1, so `b END; tail: END:`
    // reaches jump2 with the jump intact) -- otherwise the then kind-0 body's last `stb` is matched
    // first against the code falling into END (1-insn fall-through candidate) and the whole-body
    // match is never tried; (2) the then-copy's arms are laid out 1, 2, default (em29DmRoutineSetLate),
    // so its kind-0 remnant `b` is not inverted around the kind-2 arm's remnant and the then-tree is
    // cross-jumped into the else tree. The tail's `dmg = 0` is also the dead store keeping the jump.
    if (em->dmWep == 0x21) {
        em29DmRoutineSetLate(em, kind);
    } else {
        em29DmRoutineSet(em, kind);
    }
    return;
tail:
    dmg = 0;
}

Em29Func Em29_R0_move_tbl[4] = {
    em29_R0_Init,
    em29_R0_Move,
    em29_R0_Damage,
    em29_R0_Die,
};

static Em29Func Em29_R1_move_tbl[6] = {
    em29_R1_WaitLand,
    em29_R1_WaitCeiling,
    em29_R1_Walk,
    em29_R1_Turn,
    em29_R1_AtkDash,
    em29_R1_AtkRush,
};

static Em29Func Em29_R2_move_tbl[4] = {
    em29_R1_Dm_Air,
    em29_R1_Dm_Ceiling,
    em29_R1_Dm_Land,
    em29_R1_Dm_Recovery,
};

static Em29Func Em29_R3_move_tbl[3] = {
    em29_R1_Die_Normal,
    em29_R1_Die_Reset,
    em29_R1_Die_FadeOut,
};

// Parts index remap of the flipped motions (cModel::motFlip).
static u16 em29_flip_tbl[22] = {
    0, 1, 2, 6, 7, 8, 3, 4, 5, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 0,
};

// Bite attack (em29AtkCk): range, type, damage, ...
static EmAtkInfo em29_atk_tbl[1] = {
    { 350.0f, 8, 10, 1, 10, 0 },
};

void cEm29::move()
{
    Em29Work* w = EM29_WK(this);

    if (xFC) {
        em29DmCk(this);
    }
    if (w->escTimer) {
        w->escTimer--;
    }
    if (w->atkTimer) {
        w->atkTimer--;
    }
    w->flags &= ~0x6F;
    em29RouteCk(this);
    Em29_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    partsWorldCalc();
    em29ObaHitCk(this);
    atari.move();
    SatMgr.checkAir(this, 0);
    if (hp > 0 && (be_flag & 2)) {
        Ctrl12Set(w->pCtrl12, 3, 2);
    }
    if (em29FriendCk(this) == 0) {
        EmSetDie(this);
    }
}

static void em29_R0_Init(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    int one;
    int zero;

    one = 1;
    em->x12F = one;
    if (em->modelInit(ARC(4), ARC(5)) == 0) {
        pLog->err(0, 0, "em29() ModelInit failed.");
        em->xFC = 0xFF;
        return;
    }
    em->be_flag &= ~0x10;
    em->motFlip = em29_flip_tbl;
    em->hp = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 300.0f, 300.0f, 300.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->scale.x = 1.5f;
    em->scale.y = 1.5f;
    em->scale.z = 1.5f;
    AtariInit(&em->atari, 0.0f, 0.0f, 0.0f, 250.0f, 100.0f, 100.0f, 100.0f, 1, 0x2800, 10);   // COMPILER-DIFF: #1
    em->atari.flags &= ~0x200;
    em->setStatus(0xB);
    YarareInit(em, 0.0f, -30.0f, 0.0f, 100.0f, 60.0f, 5, 1);
    EspDataLoad((u32) ARC(6), 0x21, 0);
    w->flags = zero;
    w->atkTimer = 180;
    w->escTimer = zero;
    w->x90 = zero;
    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    w->tgtSpd.x = 0.0f;
    w->tgtSpd.y = 0.0f;
    w->tgtSpd.z = 0.0f;
    w->initPos = em->pos;
    w->initRot = em->rot;
    w->pCtrl11 = GetCtrlCtrl11();
    w->pCtrl12 = GetCtrlCtrl12();
    switch (em->x38D) {
    case 0:
    default:
        EmRoutineSet(em, one, zero, zero, zero);
        break;
    case 1:
        EmRoutineSet(em, 1, 1, zero, zero);
        break;
    }
    em->rot.y = fRand1_1() * PI;
    MotionSetCore(em, MOTION(em), ARC(7), 0, 0, 1, 0);
    MotionMoveF(em, 0);
    em29_R0_Move(em);
}

static void em29_R0_Move(cEm29* em)
{
    Em29_R1_move_tbl[em->xFD](em);
}

static void em29_R1_WaitLand(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    u8 se;

    w->flags |= 0x20;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(7), 0, 0, 5, Rnd() % 20);
        w->timer = Rnd() % 30;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->plDist2 < 25000000.0f) {
            if (w->timer) {
                w->timer--;
            } else {
                em->xFE++;
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 5, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 4:
        em->pos.y += 400.0f;
        MotionSetCore(em, MOTION(em), ARC(0xC), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        w->atkTimer = 180;
        EmRoutineSet(em, 1, 2, 0, 0);
        w->spd.x = 0.0f;
        w->spd.y = 80.0f;
        w->spd.z = 100.0f;
        break;
    }
    se = Rnd() % 6 + 21;
    Ctrl11SetSe(w->pCtrl11, em, Rnd() % 30 + 60, se, 9);
}

static void em29_R1_WaitCeiling(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    u8 se;

    w->flags |= 0x40;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(8), 0, 0, 5, Rnd() % 20);
        w->timer = Rnd() % 30;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->plDist2 < 25000000.0f) {
            if (w->timer) {
                w->timer--;
            } else {
                em->xFE++;
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(9), 0, 0, 5, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            w->atkTimer = 180;
            EmRoutineSet(em, 1, 2, 0, 0);
            w->spd.x = 0.0f;
            w->spd.y = -50.0f;
            w->spd.z = 100.0f;
        }
        break;
    }
    se = Rnd() % 6 + 21;
    Ctrl11SetSe(w->pCtrl11, em, Rnd() % 30 + 60, se, 9);
}

static void em29_R1_Walk(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    int hit;

    switch (em->xFE) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(0xB), 0, 0, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xC), 0, 0, 5, 0);
        }
        w->tgtSpd.x = 0.0f;
        w->tgtSpd.y = fRand1_1() * 100.0f;
        if (em->pos.y < SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) + 500.0f) {
            w->tgtSpd.y = fRand0_1() * 50.0f + 50.0f;
        }
        w->tgtSpd.z = fRand1_1() * 30.0f + 120.0f;
        if (!(Rnd() & 1)) {
            w->escTimer = 15;
        }
        w->timer = (Rnd() & 0xA) + 10;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, PI / 32.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em29SetSPeed(em, 0.1f);
        MotionMoveF(em, 0);
        hit = Ctrl12CntCk(w->pCtrl12, 2, 10);
        if (hit) {
            em->hp = 0;
            EmRoutineSet(em, 3, 2, 0, 0);
        } else if (w->timer == 0) {
            EmRoutineSet(em, 1, 2, 0, 0);
        } else {
            w->timer--;
            if (w->targetAngAbs > PI / 2.0f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        }
        break;
    }
    em29CallSe(em, 0);
}

static void em29_R1_Turn(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    void* mot;

    switch (em->xFE) {
    case 0:
        if (w->targetAngAbs > 2.0943952f) {
            mot = ARC(0xF);
        } else if (w->targetAng < 0.0f) {
            mot = ARC(0xE);
        } else {
            mot = ARC(0xD);
        }
        MotionSetCore(em, MOTION(em), mot, 0, 0, 1, 0);
        w->tgtSpd.x = 0.0f;
        w->tgtSpd.y = fRand1_1() * 100.0f;
        if (em->pos.y < SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) + 500.0f) {
            w->tgtSpd.y = fRand0_1() * 50.0f + 50.0f;
        }
        w->tgtSpd.z = fRand1_1() * 40.0f + 80.0f;
        em->xFE++;
    case 1:
        em29SetSPeed(em, 0.5f);
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        if (Ctrl12CntCk(w->pCtrl12, 2, 10)) {
            em->hp = 0;
            EmRoutineSet(em, 3, 2, 0, 0);
        }
        break;
    }
    em29CallSe(em, 0);
}

static void em29_R1_AtkDash(cEm29* em)
{
    Em29Work* w = EM29_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x12), (int) ARC(0x19), 5, 1, 0);
        w->escTimer = 0;
        w->atkHit = 0;
        w->tgtSpd.x = 0.0f;
        w->tgtSpd.y = 0.0f;
        w->tgtSpd.z = 0.0f;
        w->timer = 5;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
            em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, PI / 32.0f);
        }
        em29SetSPeed(em, 0.3f);
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        } else if ((em->seFlags28B & 1) && w->atkHit == 0 && em29AtkCk(em, 0)) {
            Ctrl12Set(w->pCtrl12, 2, 60);
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x13), 0, 0, 1, 0);
        w->escTimer = 0;
        w->atkHit = 0;
        w->tgtSpd.x = 0.0f;
        w->tgtSpd.y = fRand1_1() * 100.0f;
        w->tgtSpd.z = 130.0f;
        em->xFE++;
    case 3:
        em29SetSPeed(em, 0.5f);
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    em29CallSe(em, 0);
}

static void em29_R1_AtkRush(cEm29* em)
{
    Em29Work* w = EM29_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xC), 0, 2, 5, 0);
        w->tgtSpd.x = 0.0f;
        w->tgtSpd.z = fRand1_1() * 25.0f + 150.0f;
        w->tgtSpd.y = fRand1_1() * 100.0f;
        if (em->pos.y > 1800.0f) {
            w->tgtSpd.y = fRand0_1() * -100.0f;
        }
        if (em->pos.y < 800.0f) {
            w->tgtSpd.y = fRand0_1() * 100.0f;
        }
        w->timer = Rnd() % 10 + 10;
        em->xFE++;
    case 1: {
        f32 dy;

        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, PI / 10.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em29SetSPeed(em, 0.1f);
        MotionMoveF(em, 0);
        dy = fabsf(em->pos.y - (pPL->pos.y + 1300.0f));
        if (w->targetAngAbs < PI / 8.0f && w->targetDist < 360000.0f && dy < 400.0f && w->escTimer == 0) {
            em->xFE++;
        } else if (w->timer) {
            w->timer--;
        } else {
            em->xFE = 0;
        }
        break;
    }
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x14), 0, 2, 5, 0);
        w->tgtSpd.x = 0.0f;
        w->tgtSpd.z = 0.0f;
        w->tgtSpd.y = fRand1_1() * 3.0f;
        if (em->pos.y < 1000.0f) {
            w->tgtSpd.y = fRand0_1() * 10.0f;
        }
        w->escTimer = 0;
        w->timer = Rnd() % 20 + 20;
        if ((s16) pGS->pl_life > 0) {
            LifeDownSet2(pPL, 20, 0, 0);
            if (!(Rnd() & 1)) {
                cModel* p = GetPartsAddr(em->pParts, 2);

                EmPlBloodSet(em, &p->worldPos, 1, 0xFF, 0xFF);
                VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xA, 1);
            }
            if ((s16) pG->pl_life <= 0) {
                PlSetDamage(0, 0, 0);
            }
        }
        em->xFE++;
    case 3:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, PI / 16.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em29SetSPeed(em, 0.5f);
        MotionMoveF(em, 0);
        if (w->timer == 0) {
            em->xFE = 0;
            w->escTimer = 15;
        } else {
            w->timer--;
            if (w->targetDist > 640000.0f) {
                em->xFE = 0;
                w->escTimer = 15;
            }
        }
        break;
    }
    if (Ctrl12Ck(w->pCtrl12, 2) == 0 || (s16) pG->pl_life <= 0) {
        w->escTimer = 30;
        w->atkTimer = 180;
        EmRoutineSet(em, 1, 2, 0, 0);
    } else {
        em29CallSe(em, 1);
    }
}

static void em29_R0_Damage(cEm29* em)
{
    Em29Work* w = EM29_WK(em);

    w->flags |= 8;
    Em29_R2_move_tbl[em->xFD](em);
}

static void em29_R1_Dm_Air(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    Vec a;
    Vec hit;
    int flag;

    switch (em->xFE) {
    case 0:
        em->xFF = Rnd() & 1;
        flag = 1;
        if (em->xFF) {
            flag = 0x41;
        }
        MotionSetCore(em, MOTION(em), ARC(0x15), 0, 3, flag, 0);
        w->grav = fRand0_1() * 10.0f + 3.0f;
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        em->xFE++;
    case 1:
        w->spd.y -= w->grav;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        a.x = em->pos.x;
        a.y = em->oldPos.y;
        a.z = em->pos.z;
        if (SatMgr.hitCheck(&a, &em->pos, &hit, 0, 0, 0)) {
            em->pos.y = hit.y;
            MotionMoveF(em, 0);
            em->xFE = 4;
        } else if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        flag = 1;
        if (em->xFF) {
            flag = 0x41;
        }
        MotionSetCore(em, MOTION(em), ARC(0x16), 0, 3, flag, 0);
        w->dmRot = fRand0_1() * 0.31415927f + 0.31415927f;
        if (Rnd() & 1) {
            w->dmRot = -w->dmRot;
        }
        em->xFE++;
    case 3:
        em->rot.y += w->dmRot;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        w->spd.y -= w->grav;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        a.x = em->pos.x;
        a.y = em->oldPos.y;
        a.z = em->pos.z;
        if (SatMgr.hitCheck(&a, &em->pos, &hit, 0, 0, 0)) {
            em->pos.y = hit.y;
            MotionMoveF(em, 0);
            em->xFE = 4;
        } else {
            MotionMoveF(em, 0);
        }
        break;
    case 4:
        flag = 1;
        if (em->xFF) {
            flag = 0x41;
        }
        MotionSetCore(em, MOTION(em), ARC(0x17), 0, 3, flag, 0);
        if (em->be_flag & 2) {
            SndCall(8, 0xF, &em->pos, em->id, 0, em);
        }
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 0);
            } else {
                EmRoutineSet(em, 2, 3, 0, 0);
            }
        }
        break;
    }
}

static void em29_R1_Dm_Ceiling(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    Vec a;
    Vec hit;
    int flag;

    switch (em->xFE) {
    case 0:
        em->xFF = Rnd() & 1;
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        flag = 1;
        if (em->xFF) {
            flag = 0x41;
        }
        MotionSetCore(em, MOTION(em), ARC(0x16), 0, 3, flag, 0);
        w->grav = fRand0_1() * 10.0f + 3.0f;
        w->dmRot = fRand0_1() * 0.31415927f + 0.31415927f;
        if (Rnd() & 1) {
            w->dmRot = -w->dmRot;
        }
        em->xFE++;
    case 1:
        em->rot.y += w->dmRot;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        w->spd.y -= w->grav;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        a.x = em->pos.x;
        a.y = em->oldPos.y;
        a.z = em->pos.z;
        if (SatMgr.hitCheck(&a, &em->pos, &hit, 0, 0, 0)) {
            em->pos.y = hit.y;
            MotionMoveF(em, 0);
            em->xFE++;
        } else {
            MotionMoveF(em, 0);
        }
        break;
    case 2:
        flag = 1;
        if (em->xFF) {
            flag = 0x41;
        }
        MotionSetCore(em, MOTION(em), ARC(0x17), 0, 3, flag, 0);
        if (em->be_flag & 2) {
            SndCall(8, 0xF, &em->pos, em->id, 0, em);
        }
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 0);
            } else {
                EmRoutineSet(em, 2, 3, 0, 0);
            }
        }
        break;
    }
}

static void em29_R1_Dm_Land(cEm29* em)
{
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x11), 0, 3, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 0);
            } else {
                EmRoutineSet(em, 2, 3, 0, 0);
            }
        }
        break;
    }
}

static void em29_R1_Dm_Recovery(cEm29* em)
{
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x10), 0, 3, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
}

static void em29_R0_Die(cEm29* em)
{
    Em29Work* w = EM29_WK(em);

    w->flags |= 8;
    Em29_R3_move_tbl[em->xFD](em);
}

static void em29_R1_Die_Normal(cEm29* em)
{
    Em29Work* w = EM29_WK(em);

    switch (em->xFE) {
    case 0:
        em->atari.flags &= ~0x100;
        w->timer = Rnd() % 30 + 30;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            em->alpha -= 0.02f;
            if (em->alpha < 0.0f) {
                em->alpha = 0.0f;
                em->be_flag &= ~2;
                if (em29FriendCk(em) && !(w->flags & 0x80)) {
                    EmRoutineSet(em, 3, 1, 0, 0);
                } else {
                    em->xFE++;
                }
            }
        }
        break;
    }
}

static void em29_R1_Die_Reset(cEm29* em)
{
    Em29Work* w = EM29_WK(em);
    cModel* p;

    switch (em->xFE) {
    case 0:
        em->pos = w->initPos;
        em->rot = w->initRot;
        em->oldPos = em->pos;
        em->motFlags2 &= ~0x40000000;
        MotionSetCore(em, MOTION(em), ARC(0xC), 0, 0, 1, 0);
        MotionMoveF(em, 0);
        PartsWorldPosCalc(em);
        for (p = em->pParts; p; p = p->pParts) {
            p->oldWorldPos = p->worldPos;
            p->x88 = p->oldWorldPos;
        }
        em->hp = 1000;
        em->atari.flags |= 0x100;
        em->alpha = 0.0f;
        em->be_flag |= 2;
        em->xFE++;
    case 1:
        em->alpha += 0.1f;
        if (em->alpha > 1.0f) {
            em->alpha = 1.0f;
            w->spd.x = 0.0f;
            w->spd.y = 0.0f;
            w->spd.z = 0.0f;
            w->tgtSpd.x = 0.0f;
            w->tgtSpd.y = 0.0f;
            w->tgtSpd.z = 0.0f;
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
}

static void em29_R1_Die_FadeOut(cEm29* em)
{
    Em29Work* w = EM29_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xC), 0, 0, 5, 0);
        w->tgtSpd.z = fRand1_1() * 25.0f + 75.0f;
        w->tgtSpd.x = 0.0f;
        w->tgtSpd.y = fRand1_1() * 100.0f;
        if (em->pos.y < 1600.0f) {
            w->tgtSpd.y = fRand0_1() * 50.0f + 50.0f;
        }
        w->escTimer = 100;
        w->timer = (Rnd() & 0xA) + 10;
        em->hp = 0;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, PI / 32.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em29SetSPeed(em, 0.1f);
        MotionMoveF(em, 0);
        em->alpha -= 0.1f;
        if (em->alpha < 0.0f) {
            em->alpha = 0.0f;
            em->be_flag &= ~2;
            em->xFE++;
        }
        break;
    }
}

void em29RouteCk(cEm29* em)
{
    Em29Work* w = EM29_WK(em);

    if (em->hp <= 0) {
        return;
    }
    if (w->flags & 0x20) {
        return;
    }
    if (w->flags & 0x40) {
        return;
    }
    if ((pG->flags_51E4 & 3) != (em->emsetNo & 3)) {
        return;
    }
    if (RouteCkToPos(em, &pPL->pos, &w->routePos, 0, 0)) {
        w->flags |= 1;
    }
    w->routeAng = Muku(&em->pos, &w->routePos, em->rot.y, PI);
    w->routeAngAbs = fabsf(w->routeAng);
    if (em->xFC == 0) {
        w->routeAng = 0.0f;
        w->routeAngAbs = 0.0f;
        em->plDist2 = 100000000.0f;
    }
    w->targetPos = w->routePos;
    w->targetAng = w->routeAng;
    w->targetAngAbs = w->routeAngAbs;
    w->targetDist = em->plDist2;
    w->pTarget = pPLS;
    if ((em->pos.x - w->initPos.x) * (em->pos.x - w->initPos.x) + (em->pos.z - w->initPos.z) * (em->pos.z - w->initPos.z)
        > em->x3CC * em->x3CC) {
        RouteCkToPos(em, &w->initPos, &w->targetPos, 0, 0);
        w->targetAng = Muku(&em->pos, &w->targetPos, em->rot.y, PI);
        w->targetAngAbs = fabsf(w->targetAng);
        w->targetDist = (em->pos.x - w->initPos.x) * (em->pos.x - w->initPos.x)
                        + (em->pos.z - w->initPos.z) * (em->pos.z - w->initPos.z);
    }
    if (!(w->flags & 0x10)) {
        if (em->plDist2 < 360000.0f) {
            w->escTimer = 45;
        }
    }
    w->flags &= ~0x10;
    if (w->escTimer) {
        RouteCkEscEm(em, w->pTarget, &w->targetPos);
    }
}

void em29SetSPeed(cEm29* em, f32 rate)
{
    Em29Work* w = EM29_WK(em);
    Mtx m;
    Vec v;
    f32 fl;

    w->spd.x = w->spd.x * (1.0f - rate) + w->tgtSpd.x * rate;
    w->spd.y = w->spd.y * (1.0f - rate) + w->tgtSpd.y * rate;
    w->spd.z = w->spd.z * (1.0f - rate) + w->tgtSpd.z * rate;
    PSMTXRotRad(m, 'y', em->rot.y);
    PSMTXMultVecSR(m, &w->spd, &v);
    PSVECAdd(&em->pos, &v, &em->pos);
    fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
    if (em->pos.y > fl + 2500.0f) {
        em->pos.y = fl + 2500.0f;
    }
    if (em->pos.y < fl + 100.0f) {
        em->pos.y = fl + 100.0f;
    }
}

void em29ObaHitCk(cEm29* em)
{
    Vec d;
    f32 len;
    f32 r;
    u32 i;

    if (em->hp <= 0) {
        return;
    }
    if (!(em->be_flag & 2)) {
        return;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = em29EmWork(i);

        if (!(e->be_flag & 1)) {
            continue;
        }
        if (!(e->be_flag & 0x20)) {
            continue;
        }
        if (e->id != 0x29) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e->pParts == 0) {
            continue;
        }
        PSVECSubtract(&em->pos, &e->pos, &d);
        len = d.x * d.x + d.y * d.y + d.z * d.z;
        if (len > 40000.0f) {
            continue;
        }
        if (len <= 0.0f) {
            continue;
        }
        len = SQRTF(len) * 0.9f + 20.0f;
#line 1674 "D:/Bio4/Prog/em29.cpp"
        VECNormalizeP(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&e->pos, &d, &em->pos);
        if (em->pos.y < 0.0f) {
            em->pos.y = 0.0f;
        }
    }
    if (em->pos.y > pPL->pos.y + 1600.0f) {
        return;
    }
    if (em->pos.y < pPL->pos.y - 200.0f) {
        return;
    }
    PSVECSubtract(&em->pos, &pPL->pos, &d);
    d.y = 0.0f;
    len = d.x * d.x + d.z * d.z;
    r = pPL->atari.rectZ + 100.0f;
    if (len > r * r) {
        return;
    }
    if (len <= 0.0f) {
        return;
    }
#line 1691 "D:/Bio4/Prog/em29.cpp"
    VECNormalizeP(&d, &d);
    PSVECScale(&d, &d, r);
    FSet(em->pos.x, pPL->pos.x + d.x);
    FSet(em->pos.z, pPL->pos.z + d.z);
}

int em29LastCk(cEm29* em)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = em29EmWork(i);

        if (!(e->be_flag & 1)) {
            continue;
        }
        if (!(e->be_flag & 0x20)) {
            continue;
        }
        if (e->id != 0x29) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (!(e->be_flag & 2)) {
            continue;
        }
        if (e->pParts == 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        return 0;
    }
    return 1;
}

int em29FriendCk(cEm29* em)
{
    u32 i;

    if (Ctrl12CntCk(EM29_WK(em)->pCtrl12, 2, 10)) {
        return 0;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = em29EmWork(i);

        if (!(e->be_flag & 1)) {
            continue;
        }
        if (!(e->be_flag & 0x20)) {
            continue;
        }
        if (e->id != 0x29) {
            continue;
        }
        if (e->pParts == 0) {
            continue;
        }
        if (e->hp > 0) {
            return 1;
        }
    }
    return 0;
}

void em29CallSe(cEm29* em, int type)
{
    Em29Work* w = EM29_WK(em);
    u8 se;

    if (type) {
        se = Rnd() % 2 + 4;
        Ctrl11SetSe(w->pCtrl11, em, Rnd() % 3 + 4, se, 8);
        se = Rnd() % 5 + 16;
        Ctrl11SetSe(w->pCtrl11, em, Rnd() % 10 + 20, se, 9);
    } else {
        se = (Rnd() & 7) + 6;
        Ctrl11SetSe(w->pCtrl11, em, (Rnd() & 3) | 4, se, 8);
        se = Rnd() % 6 + 21;
        Ctrl11SetSe(w->pCtrl11, em, Rnd() % 10 + 20, se, 9);
    }
}

int em29AtkCk(cEm29* em, int no)
{
    Em29Work* w = EM29_WK(em);
    EmAtkInfo* atk = &em29_atk_tbl[no];
    cModel* p = GetPartsAddr(em->pParts, 2);
    int hit = EmAtkHitCk(atk, &p->worldPos, &p->x88, 0);

    if (hit) {
        if (hit & 1) {
            EmPlBloodSet(em, &p->worldPos, 1, 0xFF, 0xFF);
            w->atkHit = 1;
            pPL->subArc = em->subArc;
            SetPlDamage((int) em, plem29_BatRush);
        }
        if (hit & 2) {
            EmSubBloodSet(em, &p->worldPos, 1, 0xFF, 0xFF);
            w->atkHit = 1;
        }
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        return 1;
    }
    return 0;
}

static void plem29_BatRush(cPlayer* pl)
{
    pl->dmg.set(0, 10);
    pl->subArc = PL_EM(pPL)->subArc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x18), 0, 3, 5, 0);
        PlSetDamageSe(0);
        pl->xFE++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}
