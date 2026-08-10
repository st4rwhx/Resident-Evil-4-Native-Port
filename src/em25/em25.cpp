// em25 module (D:/Bio4/Prog/em25.cpp): the parasite. It is born out of a host enemy's head
// (cEm25::setParent / setBirth, the P_ routines keep it on the parent's parts through em25OnParent,
// em25SetParasite attaches three tentacle objects), attacks the player from there (P_Atk, the
// poison spit of em25SetPoison) or leaves the host (Dm_P_GoOut) and crawls after the player on the
// floor (Wait / Walk / Run / Turn90 / JumpAtk / Bite with the plem25_Bite catch).

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "em25.h"
#include "em10.h"
#include "emhit.h"
#include "obj01.h"
#include "obj16.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "route_ck.h"
#include "game.h"
#include "snd.h"
#include "pad.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "quake.h"

asm(".comm common_em25,52,4");

extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// em_set.h declares EmSetDieCnt without arguments; this module passes the enemy.
void EmSetDieCntE(cEm* em) asm("EmSetDieCnt");

static void em25_R0_Init(cEm25* em);
static void em25_R0_Move(cEm25* em);
static void em25_R1_br_Dummy(cEm25* em);
static void em25_R1_Hide(cEm25* em);
static void em25_R1_Birth(cEm25* em);
static void em25_R1_Wait(cEm25* em);
static void em25_R1_Walk(cEm25* em);
static void em25_R1_Run(cEm25* em);
static void em25_R1_Turn90(cEm25* em);
static void em25_R1_br_JumpAtk(cEm25* em);
static void em25_R1_JumpAtk(cEm25* em);
static void em25_R1_Bite(cEm25* em);
static void plem25_Bite(cPlayer* pl);
static void em25_R1_P_Appear(cEm25* em);
static void em25_R1_P_Wait(cEm25* em);
static void em25_R1_P_Atk(cEm25* em);
static void em25_R1_P_Poison(cEm25* em);
static void em25_R0_Damage(cEm25* em);
static void em25_R1_Dm_P_Normal(cEm25* em);
static void em25_R1_Dm_P_GoOut(cEm25* em);
static void em25_R1_Dm_Small(cEm25* em);
static void em25_R1_Dm_Big(cEm25* em);
static void em25_R1_Dm_Frame(cEm25* em);
static void em25_R0_Die(cEm25* em);
static void em25_R1_Die_P_Normal(cEm25* em);
static void em25_R1_Die_Normal(cEm25* em);
static void em25_R1_Die_Big(cEm25* em);

Em25Func Em25_R0_move_tbl[5] = {
    em25_R0_Init,
    em25_R0_Move,
    em25_R0_Damage,
    em25_R0_Die,
    (Em25Func) Em_R0_Scenario,
};

// Routine 1 table: {branch check, routine} per xFD.
static Em25Func Em25_R1_move_tbl[24] = {
    em25_R1_br_Dummy, em25_R1_Hide,          // 0x00
    em25_R1_br_Dummy, em25_R1_Birth,         // 0x01
    em25_R1_br_Dummy, em25_R1_Wait,          // 0x02
    em25_R1_br_Dummy, em25_R1_Walk,          // 0x03
    em25_R1_br_Dummy, em25_R1_Run,           // 0x04
    em25_R1_br_Dummy, em25_R1_Turn90,        // 0x05
    em25_R1_br_JumpAtk, em25_R1_JumpAtk,     // 0x06
    em25_R1_br_Dummy, em25_R1_Bite,          // 0x07
    em25_R1_br_Dummy, em25_R1_P_Appear,      // 0x08
    em25_R1_br_Dummy, em25_R1_P_Wait,        // 0x09
    em25_R1_br_Dummy, em25_R1_P_Atk,         // 0x0A
    em25_R1_br_Dummy, em25_R1_P_Poison,      // 0x0B
};

static Em25Func Em25_R2_move_tbl[5] = {
    em25_R1_Dm_P_Normal,
    em25_R1_Dm_P_GoOut,
    em25_R1_Dm_Small,
    em25_R1_Dm_Big,
    em25_R1_Dm_Frame,
};

static Em25Func Em25_R3_move_tbl[3] = {
    em25_R1_Die_P_Normal,
    em25_R1_Die_Normal,
    em25_R1_Die_Big,
};

// Parts index remap of the flipped motions (cModel::motFlip).
static u16 em25_flip_tbl[80] = {
    0,  1,  2,  3,  6,  7,  4,  5,  10, 11, 8,  9,  13, 12, 16, 17, 14, 15, 19, 18,
    22, 23, 20, 21, 25, 24, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
};

// Attack parameters per em25AtkCk kind: 0 bite (floor), 1 bite from the host.
static EmAtkInfo em25_atk_tbl[2] = {
    { 300.0f, 8, 500, 4, 10, 0 },
    { 500.0f, 8, 800, 0, 10, 0 },
};

// Poison projectile (SetObj08) attack parameters.
static EmAtkInfo em25_poison_atk[1] = {
    { 500.0f, 8, 800, 0, 10, 0 },
};
static int em25_atk_pad = 0;

#define ARC(no) PL_ARC_PTR(em->subArc, no)
#define PL_ARC(no) PL_ARC_PTR(pl->subArc, no)

// Struct-member views of the player pointer / pG: a load through them is not hoisted above the
// preceding stores through the work pointer (cam_ctrl.cpp PlayerPtr).
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

// Dead flag test (cDmgInfo upper 16 bits): an inline returning 0/1 gives the `li 1; andis.; bne; li 0` chain.
static inline int em25DeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

// Collision flag bits set / cleared through the info's address (`addi rX, em, 0x2b4; lhz 0x1a(rX)`).
static inline void AtariOn(cAtariInfo* at, u16 b) { at->flags |= b; }

// u8 store through a reference with a promoted parameter: the constant is an SImode pseudo the
// following routine bytes share (em2d.cpp).
static inline void U8Set(u8& d, u8 v) { d = v; }
static inline void AtariOff(cAtariInfo* at, u16 mask) { at->flags &= mask; }

extern "C" void _prolog()
{
    EmInitFunc = Em25Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em25Init(cEm* em)
{
    new (em) cEm25();
}

void em25DmCk(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    int wep;
    int zero;

    if ((em->be_flag & 2) && em25DeadCk(em) == 0 && w->pParent == 0 && em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 4:
        case 5:
        case 7:
            if (w->dmGuard == 0) {
                w->dmGuard = 120;
                EmRoutineSet(em, 2, 4, 0, 0);
                return;
            }
            break;
        }
    }
    if (em->dmHit == 0) {
        return;
    }
    wep = em->dmWep;
    zero = 0;
    em->dmHit = zero;
    em->dmType = 1;
    if (wep == 0x10) {
        em->dmType = 0x11;
    }
    // COMPILER-DIFF: candidate #12 (AROUND form; r104 execEvent00 family) -- the original's cse forgets
    // `zero == 0` past the skipped `if` block, so the `hitCnt = 0` below gets its own `li`; ours
    // carries the equivalence through and would store `zero`.
    asm("" : "+r"(zero));
    em25BloodSet(em);
    w->hitCnt++;
    if (w->hitCnt > 3) {
        w->hitCnt = 0;
    }
    LifeDownSet(em, em25SetDmVal(em), 0);
    SndCall(8, 0xB, &em->pos, em->id, 0, em);
    if (em->hp <= 0) {
        switch (w->mode) {
        case 0:
        default:
            em->be_flag &= ~0x10000;
            EmSetDieCntE(em);
            EmRoutineSet(em, 3, 2, zero, zero);
            break;
        case 1:
            EmRoutineSet(em, 2, zero, zero, zero);
            em->hp = 1;
            w->dead = 1;
            break;
        }
    } else {
        if (w->flags & 8) {
            return;
        }
        switch (w->mode) {
        case 0:
        default:
            if (!(Rnd() & 1)) {
                if (Rnd() & 3) {
                    EmRoutineSet(em, 2, 2, 0, 0);
                } else {
                    EmRoutineSet(em, 2, 3, 0, 0);
                }
            }
            break;
        case 1:
            EmRoutineSet(em, 2, 0, 0, 0);
            break;
        }
    }
}

void cEm25::move()
{
    Em25Work* w = EM25_WK(this);

    if (xFC && w->pParent && !w->pParent->isAlive()) {
        w->pParent = 0;
        EmRoutineSet(this, 1, 0, 0, 0);
    }
    be_flag &= ~0x4000;
    motFlags2 &= ~0x40000000;
    em25DmCk(this);
    w->flags &= ~0x2F;
    if (!(pGS->flags_68 & 0x20000) && w->lifeTimer) {
        w->lifeTimer--;
    }
    if (w->waitTimer) {
        w->waitTimer--;
    }
    if (em25DeadCk(pPL)) {
        w->waitTimer = 120;
    }
    if (w->dmGuard) {
        w->dmGuard--;
    }
    em25RouteCk(this);
    Em25_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    partsWorldCalc();
    em25ScaleCompress(this);
    if (w->mode == 0 && hp > 0) {
        EmAtCheck(this);
        atari.move();
        SatMgr.check(this, 0);
    }
    if (hp > 0 || w->pParent) {
        if (w->seTimer) {
            w->seTimer--;
        } else {
            w->seTimer = 30;
            if (w->pParent) {
                SndCall(8, 0x1A, &w->pParent->pos, id, 0, this);
            } else {
                SndCall(8, 8, &pos, id, 0, this);
            }
        }
        if (w->pParent) {
            if (w->estTimer) {
                w->estTimer--;
            } else {
                w->estTimer = 2;
                EstSet((int) this, -1, 0, 0, 0x1D, 0xB, 0, 0, (u32) this, 0);
            }
        }
    }
    if (pG->flags_5010 & 0x04000000) {
        lightInfo.x50 = 0x80;
    } else {
        lightInfo.x50 = 2;
    }
}

static void em25_R0_Init(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    f32 fzero;
    int zero;
    u32 i;

    if (em->modelInit(ARC(4), ARC(5)) == 0) {
        pLog->err(0, 0, "em25() ModelInit failed.");
        em->xFC = 0xFF;
        return;
    }
    em->setStatus(0xB);
    zero = 0;
    em->motFlip = em25_flip_tbl;
    EspDataLoad((u32) ARC(7), 0x1D, 0);
    {
        static const Vec ofs = {0.0f, 0.0f, 0.0f};
        static const Vec size = {2000.0f, 2000.0f, 2000.0f};
        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    fzero = 0.0f;
    em->lockParts = zero;
    em->lockOfs.x = fzero;
    em->lockOfs.y = fzero;
    em->lockOfs.z = fzero;
    atariInitF(&em->atari, fzero, 500.0f, fzero, 350.0f, 350.0f, 350.0f, 1000.0f, 1, 0x2000, 10);
    YarareInit(em, fzero, fzero, fzero, 300.0f, 200.0f, 2, 1);
    YarareAdd(em, &w->hit[0], fzero, fzero, fzero, 100.0f, 100.0f, 0x1D, 1);
    YarareAdd(em, &w->hit[1], fzero, fzero, fzero, 100.0f, 100.0f, 0x1E, 1);
    YarareAdd(em, &w->hit[2], fzero, fzero, fzero, 100.0f, 100.0f, 0x1F, 1);
    w->flags = zero;
    w->scaleY = 1.0f;
    w->pParent = (cEm*) zero;
    w->parentParts = zero;
    w->hitCnt = zero;
    w->dead = zero;
    w->x3A4 = zero;
    w->estTimer = zero;
    w->waitTimer = zero;
    w->atkEnable = zero;
    for (i = 0; i < 3; i++) {
        w->pPara[i] = 0;
    }
    w->espKind = EspPullCoreKind();
    switch (em->x38D) {
    case 0:
    default:
        em->xFC = 1;
        em->xFD = 0;
        em->xFE = 0;
        em->xFF = 0;
        w->mode = 0;
        break;
    case 1:
        em->setStatus(5);
        w->lifeTimer = 900;
        em->xFC = 1;
        em->xFD = 2;
        em->xFE = 0;
        em->xFF = 0;
        EstSet((int) em, -1, 0, 0, 0x1D, 2, 0, w->espKind, (u32) em, 0);
        em25SetParasite(em);
        break;
    }
    em25_R0_Move(em);
}

static void em25_R0_Move(cEm25* em)
{
    Em25_R1_move_tbl[em->xFD * 2](em);
    Em25_R1_move_tbl[em->xFD * 2 + 1](em);
}

static void em25_R1_br_Dummy(cEm25* em)
{
}

static void em25_R1_Hide(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    cModelInfo* info;
    cModel* p;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(8), 0, 0, 5, 0);
        MotionMoveF(em, 0);
        em->hp = 0;
        em->be_flag &= ~2;
        em->be_flag &= ~0x10000;
        w->pParent = 0;
        w->dead = 0;
        AtariOff(&em->atari, 0xFCFF);
        em->setStatus(1);
        em->be_flag &= ~0x10;
        w->scaleY = 1.0f;
        for (info = em->pInfo; info; info = info->pNext) {
            info->color[0] = 0xFF;
            info->color[1] = 0xFF;
            info->color[2] = 0xFF;
        }
        for (p = em->pParts; p; p = p->pParts) {
            p->scale.x = 1.0f;
            p->scale.y = 1.0f;
            p->scale.z = 1.0f;
        }
        w->scaleY = 1.0f;
        em->xFE++;
        break;
    case 1:
        em->be_flag |= 0x4000;
        break;
    }
}

static void em25_R1_Birth(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    cModel* p;

    switch (em->xFE) {
    case 0:
        em->scale.x = 1.0f;
        em->scale.y = 1.0f;
        em->scale.z = 1.0f;
        em->alpha = 1.0f;
        em->hp = em->hpMax;
        em->be_flag |= 2;
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        TransMatrix(em->mat, &em->scale);
        MotionSetCore(em, &em->mot, ARC(0x26), (int) ARC(0x2E), 0, 1, 0);
        for (p = em->pParts; p; p = p->pParts) {
            p->scale.x = 1.0f;
            p->scale.y = 1.0f;
            p->scale.z = 1.0f;
        }
        em->setStatus(5);
        AtariOn(&em->atari, 0x300);
        em->clearStatus(1);
        w->scaleY = 1.0f;
        w->lifeTimer = 900;
        EstSet((int) em, -1, 0, 0, 0x1D, 2, 0, w->espKind, (u32) em, 0);
        em25SetParasite(em);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
}

static void em25_R1_Wait(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x19), 0, 5, 5, 0);
        if (w->waitTimer <= 29) {
            w->waitTimer = (u8) (Rnd() % 30) + 30;
        }
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if ((s16) pG->pl_life > 0) {
            if (w->targetAngAbs > 1.22173047f) {
                EmRoutineSet(em, 1, 5, 0, 0);
            } else if (w->waitTimer == 0) {
                if (em->plDist2 < 4000000.0f && w->targetAngAbs < 0.523598790f) {
                    EmRoutineSet(em, 1, 6, 0, 0);
                } else if (em->plDist2 > 25000000.0f) {
                    EmRoutineSet(em, 1, 4, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 3, 0, 0);
                }
            }
        }
        break;
    }
    if (w->lifeTimer == 0) {
        em->hp = 0;
        em->be_flag |= 0x10000;
        EmRoutineSet(em, 3, 2, 0, 0);
    }
}

static void em25_R1_Walk(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x1A), (int) ARC(0x1B), 5, 5, 0);
        w->timer = (u8) (Rnd() % 5) + 5;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.0981747732f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            if (w->timer == 0) {
                EmRoutineSet(em, 1, 2, 0, 0);
                break;
            }
            w->timer--;
        }
        if (em->plDist2 < 4000000.0f && w->targetAngAbs < 0.523598790f) {
            EmRoutineSet(em, 1, 2, 0, 0);
        } else if (w->targetAngAbs > 1.22173047f) {
            EmRoutineSet(em, 1, 5, 0, 0);
        }
        break;
    }
    if (w->lifeTimer == 0) {
        em->hp = 0;
        em->be_flag |= 0x10000;
        EmRoutineSet(em, 3, 2, 0, 0);
    }
}

static void em25_R1_Run(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x1C), (int) ARC(0x1D), 5, 5, 0);
        w->timer = (u8) (Rnd() % 3) + 2;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.0981747732f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            if (w->timer == 0) {
                EmRoutineSet(em, 1, 2, 0, 0);
                break;
            }
            w->timer--;
        }
        if (em->plDist2 < 4000000.0f && w->targetAngAbs < 0.523598790f) {
            EmRoutineSet(em, 1, 2, 0, 0);
        } else if (w->targetAngAbs > 1.22173047f) {
            EmRoutineSet(em, 1, 5, 0, 0);
        }
        break;
    }
    if (w->lifeTimer == 0) {
        em->hp = 0;
        em->be_flag |= 0x10000;
        EmRoutineSet(em, 3, 2, 0, 0);
    }
}

static void em25_R1_Turn90(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->xFE) {
    case 0:
        if (Muku(&em->pos, &pPL->pos, em->rot.y, 3.14159274f) < 0.0f) {
            MotionSetCore(em, &em->mot, ARC(0x1E), (int) ARC(0x1F), 5, 1, 0);
        } else {
            MotionSetCore(em, &em->mot, ARC(0x1E), (int) ARC(0x1F), 5, 0x41, 0);
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, 3.14159274f)) > 1.22173047f) {
                em->xFE = 0;
            } else if (em->plDist2 > 9000000.0f) {
                if (em->plDist2 > 25000000.0f) {
                    EmRoutineSet(em, 1, 4, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 3, 0, 0);
                }
            } else {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    }
    if (w->lifeTimer == 0) {
        em->hp = 0;
        em->be_flag |= 0x10000;
        EmRoutineSet(em, 3, 2, 0, 0);
    }
}

static void em25_R1_br_JumpAtk(cEm25* em)
{
    if ((em->seFlags28B & 1) && em25CatchCk(em)) {
        EmRoutineSet(em, 1, 7, 0, 0);
    }
}

static void em25_R1_JumpAtk(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x26), (int) ARC(0x2E), 5, 1, 0);
        w->timer = 10;
        w->atkHit = 0;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.0981747732f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            GameAddPoint(0xB);
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
}

static void em25_R1_Bite(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    cAtariInfo* at;
    int fe;
    int end;

    fe = em->xFE;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x27), 0, 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x1D, 3, 0, 0, (u32) em, (void*) fe);
        EmCatchPLSet(em, 0.0f, 2, (int) plem25_Bite, -150.0f, 0.0f, 628.0f);
        PlGachaInit();
        SndCall(8, 0x12, &em->pos, em->id, 0, em);
        w->timer = 50;
        w->catchTimer = 10;
        em->xFF = Rnd() & 3;
        w->sndId = 0;
        em->xFE++;
    case 1:
        em->dmg.set(0, 10);
        PlGachaMove();
        if (w->catchTimer) {
            w->catchTimer--;
            end = EmCatchMotionMove(em, 0.3f, 0.2f);
        } else {
            end = MotionMoveF(em, 0);
        }
        if (end) {
            at = &em->atari;
            at->setPriority(0);
            AtariOn(at, 0x300);
            w->waitTimer = 60;
            em->dmType = 10;
            EmRoutineSet(em, 1, 2, 0, 0);
            break;
        }
        if (em->frame > 19.7000008f && em->frame < 20.2999992f) {
            w->sndId = SndCall(8, 0x13, &em->pos, em->id, 0, em);
        }
        if (w->timer) {
            w->timer--;
            LifeDownSet2(pPLS, 5, 0, 1);
            if (w->timer == 0) {
                if ((u32) PlGachaGet() < 15) {
                    LifeDownSet2(pPL, 500, 0, 1);
                }
                if ((s16) pG->pl_life <= 1) {
                    pG->pl_life = 0;
                    em->xFE++;
                } else {
                    SndStop(w->sndId, 0);
                    SndCall(8, 0x14, &em->pos, em->id, 0, em);
                }
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->mot, ARC(0x2B), 0, 0, 1, 0);
        SndStop(w->sndId, 0);
        PlSetDamageSe(0xD);
        EmCatchPLSet(em, 0.0f, 2, (int) plem25_Bite, 24.3600006f, 0.0f, 307.75f);
        pPL->xFE = fe;
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            at = &em->atari;
            at->setPriority(0);
            AtariOn(at, 0x300);
            w->waitTimer = 60;
            em->dmType = 10;
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    em->x3A8 = em->pos;
}

static void plem25_Bite(cPlayer* pl)
{
    int end;

    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 10);
    pl->subArc = ((cEm*) pPL->dmgType)->subArc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, &pl->mot, PL_ARC(0x2F), 0, 0, 1, 0);
        PlSetFace(1);
        pl->atari.set(10, 480.000031f, 400.0f);
        pl->x3E0 = 10;
        pl->xFE++;
    case 1:
        if (pl->x3E0) {
            pl->x3E0--;
            end = EmCatchMotionMove(pl, 0.3f, 0.2f);
        } else {
            end = MotionMoveF(pl, 0);
        }
        if (end) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    case 2:
        MotionSetCore(pl, &pl->mot, PL_ARC(0x30), 0, 0, 1, 0);
        pl->atari.set(10, 480.000031f, 400.0f);
        pl->xFE++;
    case 3:
        MotionMoveF(pl, 0);
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em25_R1_P_Appear(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    int fe;

    fe = em->xFE;
    switch (fe) {
    case 0:
        em->scale.x = 0.0f;
        em->scale.y = 0.0f;
        em->scale.z = 0.0f;
        MotionSetCore(em, &em->mot, ARC(0xC), (int) ARC(0xD), 0, 1, 0);
        w->seTimer = 0;
        em->setStatus(5);
        EstSet((int) em, -1, 0, 0, 0x1D, 2, 0, w->espKind, (u32) em, (void*) fe);
        w->atkEnable = 0;
        w->scaleY = 1.0f;
        em->alpha = 0.0f;
        em->be_flag |= 2;
        em->xFE++;
    case 1:
        em->scale.x = em->scale.x * 0.899999976f + 0.100000001f;
        em->scale.y = em->scale.y * 0.899999976f + 0.100000001f;
        em->scale.z = em->scale.z * 0.899999976f + 0.100000001f;
        em->alpha += 0.100000001f;
        if (em->alpha > 1.0f) {
            em->alpha = 1.0f;
        }
        em25OnParent(em);
        if (MotionMoveF(em, 0)) {
            em->scale.x = 1.0f;
            em->scale.y = 1.0f;
            em->scale.z = 1.0f;
            em->alpha = 1.0f;
            em->hp = 0;
            U8Set(w->atkEnable, 1);
            EmRoutineSet(em, 1, 9, 0, 0);
        }
        break;
    }
}

static void em25_R1_P_Wait(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    Vec v;
    cModel* p;
    f32 ang;
    f32 d;

    w->atkEnable = 1;
    ang = 0.0f;
    if (w->pParent) {
        p = w->pParent->getPartsPtr(w->parentParts);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1.0f;
        PSMTXMultVecSR(p->mat, &v, &v);
        if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f) {
            v.z = 1.0f;
        }
#line 1275 "D:/Bio4/Prog/em25.cpp"
        VECNormalize(&v, &v);
        ang = asinf(v.y);
    }
    if (ang > 0.523598790f || ang < -0.523598790f) {
        w->flags |= 0x20;
    }
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(8), 0, 30, 5, 0);
        w->timer = 10;
        em->xFE++;
    case 1:
        em25OnParent(em);
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
            break;
        }
        if (!(em->xFE == 2 || em->xFE == 3) && ang > 1.04719758f) {
            em->xFE = 2;
            break;
        }
        if (!(em->xFE == 4 || em->xFE == 5) && ang < -1.04719758f) {
            em->xFE = 4;
            break;
        }
        p = em->getPartsPtr(0);
        d = (pPL->pos.x - p->worldPos.x) * (pPL->pos.x - p->worldPos.x) +
            (pPL->pos.y - p->worldPos.y) * (pPL->pos.y - p->worldPos.y) +
            (pPL->pos.z - p->worldPos.z) * (pPL->pos.z - p->worldPos.z);
        if (d < 25000000.0f) {
            em->xFE = 2;
        }
        break;
    case 2:
        MotionSetCore(em, &em->mot, ARC(9), 0, 30, 5, 0);
        em->xFE++;
    case 3:
        em25OnParent(em);
        MotionMoveF(em, 0);
        p = em->getPartsPtr(0);
        d = (pPL->pos.x - p->worldPos.x) * (pPL->pos.x - p->worldPos.x) +
            (pPL->pos.y - p->worldPos.y) * (pPL->pos.y - p->worldPos.y) +
            (pPL->pos.z - p->worldPos.z) * (pPL->pos.z - p->worldPos.z);
        if ((d > 64000000.0f && ang < 0.785398185f) || ang < -0.523598790f) {
            em->xFE = 0;
        }
        break;
    case 4:
        MotionSetCore(em, &em->mot, ARC(0xA), 0, 30, 5, 0);
        em->xFE++;
    case 5:
        em25OnParent(em);
        MotionMoveF(em, 0);
        if (ang > -0.785398185f) {
            em->xFE = 0;
        }
        break;
    }
}

static void em25_R1_P_Atk(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    int fe;

    fe = em->xFE;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x13), (int) ARC(0x14), 5, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x1D, 0xC, 0, 0, (u32) em, (void*) fe);
        w->atkHit = 0;
        em->xFE++;
    case 1:
        em25OnParent(em);
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 9, 0, 0);
        } else if (em->seFlags28B & 1) {
            em25AtkCk(em, 1, 2);
        }
        break;
    }
}

static void em25_R1_P_Poison(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x15), (int) ARC(0x16), 5, 1, 0);
        w->atkHit = 0;
        em->xFE++;
    case 1:
        em25OnParent(em);
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 9, 0, 0);
        } else if (em->frame > 17.7000008f && em->frame < 18.2999992f) {
            em25SetPoison(em);
        }
        break;
    }
}

static void em25_R0_Damage(cEm25* em)
{
    EM25_WK(em)->flags |= 8;
    Em25_R2_move_tbl[em->xFD](em);
}

static void em25_R1_Dm_P_Normal(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->xFE) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, &em->mot, ARC(0xE), 0, 5, 5, 0);
        } else {
            MotionSetCore(em, &em->mot, ARC(0xF), 0, 5, 5, 0);
        }
        SndCall(8, 5, &em->pos, em->id, 0, em);
        w->timer = 30;
        em->xFE++;
    case 1:
        em25OnParent(em);
        if (w->timer) {
            w->timer--;
            w->atkEnable = 0;
        } else {
            w->atkEnable = 1;
        }
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 9, 0, 0);
        }
        break;
    }
}

static void em25_R1_Dm_P_GoOut(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    Vec a;
    Vec b;
    Vec hit;
    int fe;

    fe = em->xFE;
    switch (fe) {
    case 0:
        em->setStatus(5);
        if (w->pParent) {
            em->rot.y = w->pParent->rot.y;
        }
        em->pos.x = em->mat[0][3];
        em->pos.y = w->pParent->pos.y;
        em->pos.z = em->mat[2][3];
        if (w->pParent) {
            a = w->pParent->pos;
            b = em->pos;
            a.y += 100.0f;
            b.y += 100.0f;
            if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0) || ObjHitCheck(&hit, 0, &a, &b, 1)) {
                em->pos = w->pParent->pos;
            }
        }
        em->oldPos = em->pos;
        w->mode = 0;
        w->pParent = 0;
        if (em->xFF) {
            MotionSetCore(em, &em->mot, ARC(0x24), (int) ARC(0x25), 5, 1, 0);
        } else {
            MotionSetCore(em, &em->mot, ARC(0x26), (int) ARC(0x2E), 5, 1, 0);
        }
        w->lifeTimer = 900;
        em25SetParasite(em);
        em->hp = em->hpMax;
        SndCall(8, 5, &em->pos, em->id, 0, em);
        w->scaleY = 1.0f;
        em->dmType = 2;
        MotionMoveF(em, 0);
        em->partsWorldCalc();
        em->xFE++;
        break;
    case 1:
        AtariOn(&em->atari, 0x300);
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, fe, 2, 0, 0);
        }
        break;
    }
}

static void em25_R1_Dm_Small(cEm25* em)
{
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x21), 0, 5, 1, 0);
        SndCall(8, 5, &em->pos, em->id, 0, em);
        SndCall(8, 0, &em->pos, em->id, 0, em);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
}

static void em25_R1_Dm_Big(cEm25* em)
{
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x29), (int) ARC(0x2A), 5, 1, 0);
        SndCall(8, 5, &em->pos, em->id, 0, em);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 1, 0, 0);
            } else {
                em->xFE++;
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->mot, ARC(0x24), (int) ARC(0x25), 5, 1, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
}

static void em25_R1_Dm_Frame(cEm25* em)
{
    cModelInfo* info;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x29), 0, 5, 1, 0);
        LifeDownSet(em, 1000, 0);
        SndCall(8, 5, &em->pos, em->id, 0, em);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmSetDieCntE(em);
                em->be_flag &= ~0x10000;
                EmRoutineSet(em, 3, 1, 0, 0);
            } else {
                em->xFE++;
            }
        } else if (em->hp <= 0) {
            for (info = em->pInfo; info; info = info->pNext) {
                if (info->color[0] > 0x20) {
                    info->color[0] -= 0x20;
                }
                info->color[2] = info->color[1] = info->color[0];
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->mot, ARC(0x24), (int) ARC(0x25), 5, 1, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
}

static void em25_R0_Die(cEm25* em)
{
    Em25_R3_move_tbl[em->xFD](em);
}

static void em25_R1_Die_P_Normal(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x10), 0, 5, 1, 0);
        w->dead = 1;
        w->timer = 15;
        em->hp = 0;
        em->clearStatus(5);
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        em->xFE++;
    case 1:
        em25OnParent(em);
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->scale.x *= 0.899999976f;
            em->scale.y *= 0.899999976f;
            em->scale.z *= 0.899999976f;
        }
        if (em->scale.x < 0.00999999978f) {
            em->scale.x = 0.0f;
            em->scale.y = 0.0f;
            em->scale.z = 0.0f;
            em->be_flag &= ~2;
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
}

static void em25_R1_Die_Normal(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    int fe;

    fe = em->xFE;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x20), 0, 5, 1, 0);
        em25ClearParasite(em);
        EstSet((int) em, -1, 0, 0, 0x1D, 4, 0, 0, (u32) em, (void*) fe);
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        em->clearStatus(5);
        SndCall(8, 0xE, &em->pos, em->id, 0, em);
        em->hp = 0;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            AtariOff(&em->atari, 0xFCFF);
            em->xFE++;
            em->setStatus(8);
            EmSetDropItem(em);
            SndCall(8, 0xD, &em->pos, em->id, 0, em);
        }
        break;
    case 2:
        em->pos.y -= 3.0f;
        MotionMoveF(em, 0);
        w->scaleY -= 0.00999999978f;
        if (w->scaleY < 0.100000001f) {
            w->scaleY = 0.100000001f;
            em->be_flag &= ~2;
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
}

static void em25_R1_Die_Big(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    int fe;

    fe = em->xFE;
    switch (fe) {
    case 0:
        MotionSetCore(em, &em->mot, ARC(0x22), (int) ARC(0x23), 5, 1, 0);
        em->clearStatus(5);
        em25ClearParasite(em);
        EstSet((int) em, -1, 0, 0, 0x1D, 4, 0, 0, (u32) em, (void*) fe);
        SndCall(8, 0xD, &em->pos, em->id, 0, em);
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        em->hp = 0;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            AtariOff(&em->atari, 0xFCFF);
            em->xFE++;
            em->setStatus(8);
            EmSetDropItem(em);
        }
        break;
    case 2:
        em->pos.y -= 3.0f;
        MotionMoveF(em, 0);
        w->scaleY -= 0.00999999978f;
        if (w->scaleY < 0.100000001f) {
            w->scaleY = 0.100000001f;
            em->be_flag &= ~2;
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
}

// Keeps the parasite on its parent's parts: turns the head towards the player (or the partner when
// she is much closer) and concatenates the local matrix onto the parent parts matrix.
void em25OnParent(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    cEm* parent;
    cModel* p;
    cModel* t;
    Vec tgt;
    f32 d;
    f32 ang;

    parent = w->pParent;
    if (parent) {
        p = parent->getPartsPtr(w->parentParts);
        if (w->flags & 0x20) {
            em->rot.y *= 0.899999976f;
        } else {
            t = pPL->getPartsPtr(3);
            tgt = t->worldPos;
            if (pSUB) {
                d = SQRTF((parent->pos.x - pPL->pos.x) * (parent->pos.x - pPL->pos.x) +
                          (parent->pos.y - pPL->pos.y) * (parent->pos.y - pPL->pos.y) +
                          (parent->pos.z - pPL->pos.z) * (parent->pos.z - pPL->pos.z));
                if (d > SQRTF((parent->pos.x - pSUB->pos.x) * (parent->pos.x - pSUB->pos.x) +
                              (parent->pos.y - pSUB->pos.y) * (parent->pos.y - pSUB->pos.y) +
                              (parent->pos.z - pSUB->pos.z) * (parent->pos.z - pSUB->pos.z)) +
                            3000.0f) {
                    t = pSUB->getPartsPtr(3);
                    tgt = t->worldPos;
                }
            }
            ang = Muku(&parent->pos, &tgt, parent->rot.y, 1.57079637f);
            ang = Muku2(em->rot.y, ang, 0.0981747732f);
            em->rot.y += ang;
        }
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        PSMTXConcat(p->mat, em->mat, em->mat);
        em->motFlags2 |= 0x40000000;
    }
}

int cEm25::ckParent()
{
    return EM25_WK(this)->pParent == 0;
}

void cEm25::setParent(cEm* parent, int parts, Vec* ppos, Vec* prot)
{
    Em25Work* w = EM25_WK(this);

    w->pParent = parent;
    w->parentParts = parts;
    if (ppos) {
        pos = *ppos;
    } else {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
    }
    if (prot) {
        rot = *prot;
    } else {
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
    }
    AtariOff(&atari, 0xFCFF);
    U8Set(w->dead, 0);
    w->mode = 1;
    EmRoutineSet(this, 1, 8, 0, 0);
}

void cEm25::setGoOut(int flag)
{
    if (flag) {
        EmRoutineSet(this, 2, 1, 0, 1);
    } else {
        EmRoutineSet(this, 2, 1, 0, 0);
    }
}

void cEm25::setWait()
{
    EmRoutineSet(this, 1, 9, 0, 0);
}

void cEm25::setAtk()
{
    xFC = 1;
    xFD = 0xA;
    EM25_WK(this)->atkHit = 0;
    xFE = 0;
    xFF = 0;
}

int cEm25::ckAtkHit()
{
    if (EM25_WK(this)->atkHit) {
        return 1;
    }
    return 0;
}

void cEm25::setPoison()
{
    EmRoutineSet(this, 1, 0xB, 0, 0);
}

int cEm25::ckAtkEnd()
{
    return motState;
}

void cEm25::setDie()
{
    EmRoutineSet(this, 3, 0, 0, 0);
}

int cEm25::ckDie()
{
    if (EM25_WK(this)->dead) {
        return 1;
    }
    return 0;
}

void cEm25::setHide()
{
    Em25Work* w = EM25_WK(this);

    hp = 0;
    be_flag &= ~2;
    w->pParent = 0;
    w->dead = 0;
    EmRoutineSet(this, 1, 0, 0, 0);
}

int cEm25::ckHide()
{
    return (stat & 0xFFFF0000) == 0x01000000;
}

void cEm25::setBirth(Vec* ppos, f32 ang)
{
    setPos(ppos);
    oldPos = *ppos;
    rot.y = ang;
    EmRoutineSet(this, 1, 1, 0, 0);
}

int em25AtkCk(cEm25* em, int no, int parts)
{
    Em25Work* w = EM25_WK(em);
    EmAtkInfo* atk;
    cModel* p;
    int hit;

    if (w->atkHit) {
        return 0;
    }
    atk = &em25_atk_tbl[no];
    p = em->getPartsPtr(parts);
    hit = EmAtkHitCk(atk, &p->worldPos, &p->oldWorldPos, 0);
    if (hit) {
        if (hit & 1) {
            w->atkHit = 1;
            if (no == 1) {
                EmPlBloodSet2(em, &p->worldPos, 1, 0x1D, 0x10);
                if ((s16) pG->pl_life <= 0) {
                    em25PlHeadLost();
                }
                SndCall(8, 0x21, &em->pos, em->id, 0, em);
            }
            QuakeExec(0, 0, 5, 22.0f, 2);
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        }
        if (hit & 2) {
            if (pSUB) {
                w->atkHit = 1;
                if (no == 1) {
                    EmSubBloodSet(em, &p->worldPos, 1, 0x1D, 0x10);
                    SndCall(8, 0x21, &em->pos, em->id, 0, em);
                }
            }
        }
        return 1;
    }
    return 0;
}

int em25CatchCk(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    Mtx inv;
    Vec pos;
    Vec a;
    Vec b;
    Mtx m;

    if (em25DeadCk(pPL)) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (em->hp <= 0) {
        return 0;
    }
    if (!(em->seFlags28B & 1)) {
        return 0;
    }
    if (!(w->flags & 1)) {
        return 0;
    }
    if (pG->flags_5010 & 0x8000) {
        return 0;
    }
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &pos);
    if (pos.x < -300.0f || pos.x > 300.0f) {
        return 0;
    }
    if (pos.y < -250.0f || pos.y > 250.0f) {
        return 0;
    }
    if (pos.z < 0.0f || pos.z > 1000.0f) {
        return 0;
    }
    a = em->pos;
    b = pPLS->pos;
    a.y += 500.0f;
    b.y += 500.0f;
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    a = em->pos;
    b = pPLS->pos;
    a.y += 500.0f;
    b.y += 500.0f;
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &pPLS->pos));
    TransMatrix(m, &em->pos);
    a.x = 300.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 300.0f;
    b.y = 500.0f;
    b.z = 500.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    a.x = 300.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 300.0f;
    b.y = 500.0f;
    b.z = 500.0f;
    PSMTXMultVec(m, &a, &a);
    PSMTXMultVec(m, &b, &b);
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    pPLS->dmg.set(0, 2);
    em->dmg.set(0, 2);
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
    return 1;
}

void em25ScaleCompress(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    Mtx m;
    Vec scale;
    cModel* p;

    PSMTXIdentity(m);
    scale.x = 1.0f;
    scale.y = w->scaleY;
    scale.z = 1.0f;
    ScaleMatrix(m, &scale);
    for (p = em->pParts; p; p = p->pParts) {
        PSMTXConcat(m, p->mat, p->mat);
        p->mat[0][3] = p->worldPos.x;
        p->mat[1][3] = p->worldPos.y;
        p->mat[2][3] = p->worldPos.z;
    }
}

void em25RouteCk(cEm25* em)
{
    Em25Work* w = EM25_WK(em);

    if (em->hp <= 0) {
        return;
    }
    if (w->mode != 0) {
        return;
    }
    if (em->xFC != 0) {
        if ((pG->flags_51E4 & 7) != (em->emsetNo & 7)) {
            return;
        }
    }
    if (RouteCkToPos(em, &pPL->pos, &w->routePos, 0, 0)) {
        w->flags |= 1;
    }
    w->routeAng = Muku(&em->pos, &w->routePos, em->rot.y, 3.14159274f);
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
    w->flags &= ~4;
}

int em25SetDmVal(cEm25* em)
{
    int near = 0;
    int dmg;

    if (em->dmPart->rad < 36000000.0f) {
        near = 1;
    }
    dmg = 100;
    if (em->dmWep <= 0x2D) {
        dmg = GetWepDmVal(em, em->dmWep, near);
    }
    dmg *= 2;
    if (em->dmWep == 0x17 || em->dmWep == 0x2A) {
        dmg = 9999;
    }
    return dmg;
}

void em25SetParasite(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    Vec pos;
    Vec rot;
    u16 step;

    if (w->pPara[0] == 0 && w->pPara[1] == 0 && w->pPara[2] == 0 && !(w->flags & 0x10)) {
        step = (*(u16*) ARC(0x33) & 0x3FFF) / 3;
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = -0.185004905f;
        rot.y = -0.0663225129f;
        rot.z = 2.80474400f;
        w->pPara[0] = (cObj16*) SetObj16(ARC(0x31), ARC(0x32), em, em, 0x21, 7, &pos, &rot);
        if (w->pPara[0]) {
            MotSetObj16(w->pPara[0], ARC(0x33), 4, 0);
        }
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 3.36673999f;
        w->pPara[1] = (cObj16*) SetObj16(ARC(0x31), ARC(0x32), em, em, 0x22, 7, &pos, &rot);
        if (w->pPara[1]) {
            MotSetObj16(w->pPara[1], ARC(0x33), 4, step);
        }
        rot.x = 0.668461084f;
        rot.y = 0.0f;
        rot.z = 3.14159274f;
        w->pPara[2] = (cObj16*) SetObj16(ARC(0x31), ARC(0x32), em, em, 0x23, 7, &pos, &rot);
        if (w->pPara[2]) {
            MotSetObj16(w->pPara[2], ARC(0x33), 4, step * 2);
        }
        w->flags |= 0x10;
    }
}

void em25ClearParasite(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    u32 i;

    if (w->flags & 0x10) {
        for (i = 0; i < 3; i++) {
            if (w->pPara[i]) {
                w->pPara[i]->clearLostWait();
                w->pPara[i] = 0;
            }
        }
        w->flags &= ~0x10;
    }
}

void em25BloodSet(cEm25* em)
{
    Vec pos;
    Vec dir;
    int near = 0;

    if (em->dmPart->rad < 36000000.0f) {
        near = 1;
    }
    switch (em->dmWep) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 0x11:
    case 0x26:
    case 0x2B:
        EmDmBloodSet2(em, 0x1D, 0, 0, 0, 0);
        break;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        EmDmBloodSet2(em, 0x1D, 0xD, 0, 0, 0);
        if ((Rnd() & 3) == 0) {
            if (EmGetDmPos(em, &pos, &dir)) {
                EstSet(0, -1, &pos, 0, 0x1D, 0xE, 0, 0, 0, 0);
            }
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            EmDmBloodSet2(em, 0x1D, 1, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x1D, 0, 0, 0, 0);
        }
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x15:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
        EmDmBloodSet2(em, 0x1D, 1, 0, 0, 0);
        break;
    case 0x10:
    case 0x1A:
        EmDmBloodSet2(em, 0x1D, 0xF, 0, 0, 0);
        break;
    case 0:
    case 0x14:
    case 0x2A:
    default:
        break;
    }
}

// The player's head comes off: the head object flies away (regions other than Japan).
void em25PlHeadLost()
{
    Vec ofs;
    Vec spd;
    cModel* p;
    cObj* obj;
    int zero;

    if (pSys->region == 0) {
        PlSetDamageSe(0xD);
        EstSet((int) pPL, -1, 0, 0, 0x1D, 0xA, 0, 0, (u32) pPL, 0);
        return;
    }
    zero = 0;
    SndCall(1, 0x3E, &pPL->pos, 0, 0, pPL);
    EstSet((int) pPL, -1, 0, 0, 0x10, 0x45, 0, 0, (u32) pPL, (void*) zero);
    pPL->setHead(0);
    p = pPL->getPartsPtr(3);
    ofs.x = 0.0f;
    ofs.y = 68.0f;
    ofs.z = 28.0f;
    spd.x = 0.0f;
    spd.y = 50.0f;
    spd.z = -25.0f;
    PSMTXMultVec(p->mat, &ofs, &ofs);
    PSMTXMultVecSR(pPL->mat, &spd, &spd);
    obj = SetObj01(PL_ARC_PTR(pG->pPlArc, 0xC), PL_ARC_PTR(pG->pPlArc, 7), &ofs, &pPL->rot, &spd, 10.0f, 150.0f, 1000, 0x11);
    if (obj) {
        obj->lightInfo.x50 = 1;
        Obj01SetEst(obj, 0, -1, 4, 0, -1, 0, -1, (int) zero, -1);
    }
    EstSet((int) obj, -1, 0, 0, 0x10, 0x46, 0, 0, (u32) obj, (void*) zero);
}

void em25SetPoison(cEm25* em)
{
    Em25Work* w = EM25_WK(em);
    Vec spd;
    Mtx m;
    Vec tgt;
    Vec rot;
    cEm* parent;
    cModel* p;
    cModel* t;
    cObj* obj;
    EmAtkInfo* atk;
    f32 d;
    f32 ang;

    parent = w->pParent;
    em->partsWorldCalc();
    t = pPL->getPartsPtr(3);
    tgt = t->worldPos;
    if (pSUB) {
        d = SQRTF((parent->pos.x - pPL->pos.x) * (parent->pos.x - pPL->pos.x) +
                  (parent->pos.y - pPL->pos.y) * (parent->pos.y - pPL->pos.y) +
                  (parent->pos.z - pPL->pos.z) * (parent->pos.z - pPL->pos.z));
        if (d > SQRTF((parent->pos.x - pSUB->pos.x) * (parent->pos.x - pSUB->pos.x) +
                      (parent->pos.y - pSUB->pos.y) * (parent->pos.y - pSUB->pos.y) +
                      (parent->pos.z - pSUB->pos.z) * (parent->pos.z - pSUB->pos.z)) +
                    3000.0f) {
            t = pSUB->getPartsPtr(3);
            tgt = t->worldPos;
        }
    }
    SndCall(8, 0x1E, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
    atk = em25_poison_atk;
    p = em->getPartsPtr(0);
    obj = SetObj08(em, 0, 0, &p->worldPos, &em->rot, 0x40000000, atk);
    if (parent) {
        ang = LIMIT_ANGLE(parent->rot.y + em->rot.y);
    } else {
        ang = GetXZAngle(&p->worldPos, &tgt);
    }
    PSMTXRotRad(m, 'y', ang);
    spd.x = 0.0f;
    spd.y = 40.0f;
    spd.z = 130.0f;
    PSMTXMultVecSR(m, &spd, &spd);
    SetObj08Spd(obj, &spd, 30, 10.0f, 100.0f);
    SetObj08Est(obj, 0, 0, 0x1D, 8, 0x1D, 7, 0x1D, 9, 1);
    SetObj08Se(obj, 8, 0x1F);
    rot.x = 0.0f;
    rot.y = ang;
    rot.z = 0.0f;
    EstSet(0, -1, &p->worldPos, &rot, 0x1D, 6, 0, 0, 0, 0);
}

int cEm25::ckAtkEnable()
{
    if (EM25_WK(this)->atkEnable == 0) {
        return 0;
    }
    return 1;
}

void cEm25::setDamage()
{
    EmRoutineSet(this, 2, 0, 0, 0);
}

int cEm25::ckLock()
{
    cEm* parent = EM25_WK(this)->pParent;
    f32 d;

    if (parent && pSUB) {
        d = SQRTF((parent->pos.x - pPL->pos.x) * (parent->pos.x - pPL->pos.x) +
                  (parent->pos.y - pPL->pos.y) * (parent->pos.y - pPL->pos.y) +
                  (parent->pos.z - pPL->pos.z) * (parent->pos.z - pPL->pos.z));
        return d > SQRTF((parent->pos.x - pSUB->pos.x) * (parent->pos.x - pSUB->pos.x) +
                         (parent->pos.y - pSUB->pos.y) * (parent->pos.y - pSUB->pos.y) +
                         (parent->pos.z - pSUB->pos.z) * (parent->pos.z - pSUB->pos.z)) +
                       3000.0f;
    }
    return 0;
}
