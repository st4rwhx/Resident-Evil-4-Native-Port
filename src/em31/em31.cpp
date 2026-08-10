// em31 module (D:/Bio4/Prog/em31.cpp): the giant. cModel::type 0 is the body (Wait / Walk / Dash /
// Turn / Jump / Stamp / Kick / Catch / HeadAtk / BackAtk, the bridge fight em31_R1_BridgeVs with the
// pillar throws and the em31JumpCk / em31BridgeJumpCk bridge jumps, the Dm_* damage reactions with the
// player's climb on its back), type 1 the parasite tentacle on its back (em31_R1_T_*: it follows the
// body through em31SearchBody / em31TenMatCalc, carries the weak point object and the four eyes of
// em31Eyelid*, and takes the weapon damage the body forwards from em31DmCk).

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "em31.h"
#include "em10.h"
#include "emhit.h"
#include "embarrel.h"
#include "obj00.h"
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
#include "act_btn.h"
#include "game.h"
#include "snd.h"
#include "pad.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "pendulum.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "quake.h"

asm(".comm common_em31,52,4");

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");

// Falling pillar object (game/objPillar.cpp; the class is local to that unit).
class cObjPillar : public cObj {
public:
    virtual void move();

    int ckSet();
    void setBreak(Vec* pos, void* mot, int a);
    void setThrow(void* mot0, void* mot1, void* motEscape, void* plMot, int a);
    void setFall(void* mot0, void* mot1);
};
cObj* SetPillar(void* bin, void* tpl, Vec* pos, Vec* rot);

static void em31_R0_Init(cEm31* em);
static void em31_R0_Move(cEm31* em);
static void em31_R1_br_Dummy(cEm31* em);
static void em31_R1_Appear(cEm31* em);
static void em31_R1_Wait(cEm31* em);
static void em31_R1_Walk(cEm31* em);
static void em31_R1_Dash(cEm31* em);
static void em31_R1_Turn(cEm31* em);
static void em31_R1_BridgeVs(cEm31* em);
static void em31_R1_Jump(cEm31* em);
static void em31_R1_BerserkStart(cEm31* em);
static void em31_R1_BerserkEnd(cEm31* em);
static void em31_R1_Stamp(cEm31* em);
static void em31ActEscape(cEm31* em);
static void plemEscape(cPlayer* pl);
static void em31_R1_Kick(cEm31* em);
static void em31_R1_br_Catch(cEm31* em);
static void em31_R1_Catch(cEm31* em);
static void em31_R1_CatchHit(cEm31* em);
static void plem31_CatchHit(cPlayer* pl);
static void em31_R1_br_StepCatch(cEm31* em);
static void em31_R1_StepCatch(cEm31* em);
static void em31_R1_StepCatchHit(cEm31* em);
static void plem31_StepCatchHit(cPlayer* pl);
static void em31_R1_HeadAtk(cEm31* em);
static void em31_R1_BackAtk(cEm31* em);
static void em31_R1_T_Appear(cEm31* em);
static void em31_R1_T_Wait(cEm31* em);
static void em31_R1_T_Stamp(cEm31* em);
static void em31_R1_T_Atk(cEm31* em);
static void em31_R1_T_DashAtk(cEm31* em);
static void em31_R1_T_BerserkStart(cEm31* em);
static void em31_R1_T_BerserkEnd(cEm31* em);
static void em31_R1_T_Jump(cEm31* em);
static void em31_R1_T_Catch(cEm31* em);
static void em31_R1_T_CatchHit(cEm31* em);
static void em31_R1_T_StepCatch(cEm31* em);
static void em31_R1_T_StepCatchHit(cEm31* em);
static void em31_R1_T_PillarThrow(cEm31* em);
static void em31_R1_T_Dm_Normal(cEm31* em);
static void em31_R1_T_Down(cEm31* em);
static void em31_R1_T_Dm_Crane(cEm31* em);
static void em31_R1_T_Dm_Climb(cEm31* em);
static void em31_R1_T_Die(cEm31* em);
static void em31_R0_Damage(cEm31* em);
static void em31_R1_Dm_Normal(cEm31* em);
static void em31_R1_Dm_Down(cEm31* em);
static void em31_R1_Dm_Crane(cEm31* em);
static void em31_R1_Dm_Climb(cEm31* em);
static void plem31_Climb(cPlayer* pl);
static void em31SetActClimb(cEm31* em);
static void em31_R0_Die(cEm31* em);
static void em31_R1_Die_Normal(cEm31* em);
static void plem31_dm_Stamp(cPlayer* pl);

Em31Func Em31_R0_move_tbl[4] = {
    em31_R0_Init,
    em31_R0_Move,
    em31_R0_Damage,
    em31_R0_Die,
};

// Routine 1 table: {branch check, routine} per xFD (0x00..0x10 body, 0x11..0x22 tentacle).
static Em31Func Em31_R1_move_tbl[70] = {
    em31_R1_br_Dummy, em31_R1_Appear,             // 0x00
    em31_R1_br_Dummy, em31_R1_Wait,               // 0x01
    em31_R1_br_Dummy, em31_R1_Walk,               // 0x02
    em31_R1_br_Dummy, em31_R1_Dash,               // 0x03
    em31_R1_br_Dummy, em31_R1_Turn,               // 0x04
    em31_R1_br_Dummy, em31_R1_BerserkStart,       // 0x05
    em31_R1_br_Dummy, em31_R1_BerserkEnd,         // 0x06
    em31_R1_br_Dummy, em31_R1_BridgeVs,           // 0x07
    em31_R1_br_Dummy, em31_R1_Jump,               // 0x08
    em31_R1_br_Dummy, em31_R1_Stamp,              // 0x09
    em31_R1_br_Dummy, em31_R1_Kick,               // 0x0A
    em31_R1_br_Catch, em31_R1_Catch,              // 0x0B
    em31_R1_br_Dummy, em31_R1_CatchHit,           // 0x0C
    em31_R1_br_StepCatch, em31_R1_StepCatch,      // 0x0D
    em31_R1_br_Dummy, em31_R1_StepCatchHit,       // 0x0E
    em31_R1_br_Dummy, em31_R1_HeadAtk,            // 0x0F
    em31_R1_br_Dummy, em31_R1_BackAtk,            // 0x10
    em31_R1_br_Dummy, em31_R1_T_Appear,           // 0x11
    em31_R1_br_Dummy, em31_R1_T_Wait,             // 0x12
    em31_R1_br_Dummy, em31_R1_T_BerserkStart,     // 0x13
    em31_R1_br_Dummy, em31_R1_T_BerserkEnd,       // 0x14
    em31_R1_br_Dummy, em31_R1_T_Jump,             // 0x15
    em31_R1_br_Dummy, em31_R1_T_Stamp,            // 0x16
    em31_R1_br_Dummy, em31_R1_T_Atk,              // 0x17
    em31_R1_br_Dummy, em31_R1_T_DashAtk,          // 0x18
    em31_R1_br_Dummy, em31_R1_T_Catch,            // 0x19
    em31_R1_br_Dummy, em31_R1_T_CatchHit,         // 0x1A
    em31_R1_br_Dummy, em31_R1_T_StepCatch,        // 0x1B
    em31_R1_br_Dummy, em31_R1_T_StepCatchHit,     // 0x1C
    em31_R1_br_Dummy, em31_R1_T_PillarThrow,      // 0x1D
    em31_R1_br_Dummy, em31_R1_T_Dm_Normal,        // 0x1E
    em31_R1_br_Dummy, em31_R1_T_Down,             // 0x1F
    em31_R1_br_Dummy, em31_R1_T_Dm_Crane,         // 0x20
    em31_R1_br_Dummy, em31_R1_T_Dm_Climb,         // 0x21
    em31_R1_br_Dummy, em31_R1_T_Die,              // 0x22
};

static Em31Func Em31_R2_move_tbl[4] = {
    em31_R1_Dm_Normal,
    em31_R1_Dm_Down,
    em31_R1_Dm_Crane,
    em31_R1_Dm_Climb,
};

static Em31Func Em31_R3_move_tbl[1] = {
    em31_R1_Die_Normal,
};

// Attacks (em31AtkCk): [0]/[1] stamp, [2] dash, [3] back hand, [4] jump landing, [5]/[6] tentacle,
// [7] tail.
static EmAtkInfo em31_atk_tbl[8] = {
    { 1200.0f, 8, 1100, 0, 0xA, 0 },
    { 1200.0f, 8, 1100, 0, 0xA, 0 },
    { 1500.0f, 8, 2000, 0, 0xA, 0 },
    { 1500.0f, 8, 800, 0, 0xA, 0 },
    { 1000.0f, 8, 1100, 0, 0xA, 0 },
    { 1000.0f, 8, 800, 0, 0xA, 0 },
    { 1000.0f, 8, 800, 0, 0xA, 0 },
    { 500.0f, 8, 400, 0, 0xA, 0 },
};

// Parts index remap of the flipped motions (cModel::motFlip) of the body / the tentacle.
static u16 em31_flip0[120] = {
    0x00, 0x01, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x2E, 0x2F, 0x30, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x23, 0x24,
    0x25, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
};

static u16 em31_flip1[120] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x1D, 0x1E,
    0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
};

// The object the player holds on to while climbing the back (plem31_Climb); a one-member struct so
// every store through it reloads the pointer.
static struct {
    cObj* p;
} em31CatchObj = { 0 };

// Cloth chains: the (unused) single link of Em31ClothSet, the hanging chains of Em31ClothSet2 and
// Em31ClothSet3.
static u8 em31ClothP[1] = { 0x22 };
static u8 em31ClothUp[1] = { 0xFF };
static u8 em31ClothDp[1] = { 0xFF };
static f32 em31ClothMax[1] = { 0.3f };
static u8 em31ClothP2[15] = { 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37 };
static u8 em31ClothUp2[15] = { 0xFF, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0xFF, 0x31, 0xFF, 0x33, 0x34, 0x35, 0x36 };
static u8 em31ClothDp2[15] = { 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0xFF, 0x32, 0xFF, 0x34, 0x35, 0x36, 0x37, 0xFF };
static f32 em31ClothMax2[15] = {
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
};
static u8 em31ClothP3[18] = {
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
};
static u8 em31ClothUp3[18] = {
    0xFF, 0x38, 0x39, 0x3A, 0xFF, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0xFF, 0x43, 0xFF, 0x45, 0x46, 0x47, 0x48,
};
static u8 em31ClothDp3[18] = {
    0x39, 0x3A, 0x3B, 0xFF, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0xFF, 0x44, 0xFF, 0x46, 0x47, 0x48, 0x49, 0xFF,
};
static f32 em31ClothMax3[18] = {
    0.3f, 0.6f, 0.9f, 1.0f, 0.3f, 0.6f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 1.0f, 0.3f, 0.6f, 0.9f, 1.0f, 1.0f,
};

#define ARC(no) PL_ARC_PTR(em->subArc, no)
#define PL_ARC(no) PL_ARC_PTR(pl->subArc, no)

// The enemy a player damage callback belongs to (pl_sub SetPlDamage's first argument).
#define PL_EM(pl) ((cEm31*) (pl)->dmgType)

#define VIB_TBL ((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc))

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
static inline int em31DeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

// Work `no` of the object manager without the range check (the pillar scans loop over nArray).
static inline cObj* em31ObjWork(u32 no)
{
    return (cObj*) ((u8*) ObjMgr.pArray + ObjMgr.size * no);
}

// Enemy manager work `no` the same way (em31SearchBody).
static inline cEm31* em31EmWork(u32 no)
{
    return (cEm31*) ((u8*) EmMgr.pArray + EmMgr.size * no);
}

extern "C" void _prolog()
{
    OSReport("em31 prolog Ok\n");
    EmInitFunc = Em31Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em31Init(cEm* em)
{
    new (em) cEm31();
}

// Starts the bridge fight (the plain byte stores share the zero of the pillar count).
static inline void em31BridgeVsSet(cEm31* em, Em31Work* w)
{
    w->pillarCnt = 0;
    em->xFC = 1;
    em->xFD = 7;
    em->xFE = 0;
    em->xFF = 0;
}

// Sets the down reaction after a hit on the tentacle's weak point: `type` picks the down motion (a
// macro: the literal is shared with the routine byte of the same value inside each arm).
#define EM31_SET_DOWN(timer, type)                                                                  \
    w->downTimer = timer;                                                                           \
    if (w->flags & 0x1000) {                                                                        \
        EmRoutineSet(em, 2, 1, 0, type);                                                            \
    }                                                                                               \
    w->downType = type;                                                                             \
    w->flags |= 0x4000

void em31DmCk(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    EmHitInfo* part;
    int wep;
    int dmg;

    if (em->hp <= 0) {
        return;
    }
    if (w->pTen) {
        if (w->pTen->hp <= 0) {
            em->hp = 0;
            return;
        }
        if (!(w->flags & 0x90)) {
            if (w->pTen->ckWeakDamage()) {
                int total = w->pTen->getTotalDamage();

                w->flags &= ~0x40;
                w->weakTimer = 450;
                w->berserkTimer = 0;
                if (total >= (s16) (em->hpMax / 20)) {
                    EmRoutineSet(em, 2, 0, 0, 0);
                    return;
                }
                EM31_SET_DOWN(210, 0);
                return;
            }
        }
    }
    if (em->hp > 0 && em31DeadCk(em) == 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 4:
        case 5:
        case 7:
            if (w->dmGuard == 0) {
                w->dmGuard = 120;
            }
            break;
        }
    }
    if (em->dmHit == 0) {
        return;
    }
    wep = em->dmWep;
    em->dmHit = 0;
    em->dmType = 1;
    if (wep == 0x10) {
        em->dmType = 0x11;
    }
    part = em->dmPart;
    dmg = em31SetDmVal(em);
    if (w->pTen) {
        LifeDownSet2(w->pTen, dmg, 0, 0);
        if (em31EyelidDmcK(em, 0)) {
            LifeDownSet2(w->pTen, dmg, 0, 0);
        }
        em->hp = w->pTen->hp;
    }
    em31BloodSet(em);
    if (em->hp <= 0) {
        return;
    }
    if (w->flags & 0x80) {
        return;
    }
    if (w->flags & 0x10) {
        return;
    }
    switch (part->partsNo) {
    case 0x1B:
        if (em31EyelidDmcK(em, 1)) {
            EM31_SET_DOWN(210, 0);
        }
        break;
    case 0x1D:
        if (em31EyelidDmcK(em, 1)) {
            EM31_SET_DOWN(210, 1);
        }
        break;
    case 0x1F:
        if (em31EyelidDmcK(em, 1)) {
            EM31_SET_DOWN(210, 2);
        }
        break;
    case 0x21:
        if (em31EyelidDmcK(em, 1)) {
            EM31_SET_DOWN(210, 3);
        }
        break;
    default:
        int no = em->dmWep;
        int lim2B = 0x2B;

        switch (no) {
        // The `> 0x17` half of the tree is written out: five `goto` ranges up to INT_MAX (distinct labels, so
        // group_case_nodes keeps five nodes: right list weight 10 with 0x14..0x16 explicit) keep the 0x17 root;
        // their compares (0x1A/0x1E/0x20/0x23) collide with nothing, jump1 threads the bodies away, and the
        // if-chain gives the `[18-28] -> [2b-2c]{[29-2a],[2d]}` compares that balance_case_nodes cannot
        // produce (a 4-list never has its first node as root).
        case 0x18 ... 0x1A:
            goto high1;
        case 0x1B ... 0x1D:
            goto high2;
        case 0x1E ... 0x20:
            goto high3;
        case 0x21 ... 0x23:
            goto high4;
        case 0x24 ... 0x7FFFFFFF:
            goto high5;
        high1:
        high2:
        high3:
        high4:
        high5:
            if (no <= 0x28) {
                break;
            }
            if (no > 0x2C) {
                if (no == 0x2D) {
                    goto down;
                }
                break;
            }
            if (no >= lim2B) {  // a variable: fold rewrites a literal `>= 0x2B` to `> 0x2A`
                break;
            }
        down:
        case 0xD:
        case 0x12:
        case 0x13:
            EM31_SET_DOWN(90, 0);
            break;
        case 0x17:
            w->hitTimer = 90;
            break;
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
        case 0x8:
        case 0x9:
        case 0xA:
        case 0xB:
        case 0xC:
        case 0xE:
        case 0xF:
        case 0x10:
        case 0x11:
        case 0x14:
        case 0x15:
        case 0x16:
        default:
            break;
        }
        break;
    }
}

void em31DmCkT(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    EmHitInfo* part;
    int wep;
    int dmg;

    if (em->hp <= 0) {
        return;
    }
    if (em->dmHit == 0) {
        return;
    }
    wep = em->dmWep;
    em->dmHit = 0;
    em->dmType = 1;
    if (wep == 0x10) {
        em->dmType = 0x11;
    }
    part = em->dmPart;
    dmg = em31SetDmVal(em);
    LifeDownSet2(em, dmg, 0, 0);
    if (part->partsNo == 0xC) {
        w->flags |= 0x2000;
        w->totalDamage += dmg;
        w->flags |= 0x100;
    }
    em31TBloodSet(em);
}

void cEm31::move()
{
    Em31Work* w = EM31_WK(this);

    w->flags &= ~0x100;
    if (xFC) {
        switch (type) {
        case 0:
        default:
            em31DmCk(this);
            break;
        case 1:
            em31DmCkT(this);
            break;
        }
    }
    w->flags &= ~0x869F;
    if (w->atkWait) {
        w->atkWait--;
    }
    if (w->atkRtnWait) {
        w->atkRtnWait--;
    }
    if (!(w->flags & 0x40) && w->weakTimer) {
        w->weakTimer--;
    }
    if (em31DeadCk(pPL)) {
        w->atkWait = 60;
    }
    if (w->hitTimer) {
        w->hitTimer--;
    }
    em31RouteCk(this);
    Em31_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em31EyelidMove(this);
    switch (type) {
    case 0:
    default:
        partsWorldCalc();
        break;
    case 1:
        em31TenMatCalc(this);
        break;
    }
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    if (type == 0) {
        Em31ClothMove2(this, &w->cloth2);
        Em31ClothMove3(this, &w->cloth3);
        if (hp > 0) {
            EstSet((int) this, -1, 0, 0, 0x29, 0x1D, 0, 0, (u32) this, 0);
        }
    }
    em31TailAtkCk(this);
    em31FootSe(this);
    em31BreathSeStopCk(this);
    em31WeakMove(this);
}

static void em31_R0_Init(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    cParts* p;
    int zero;
    int one;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(7)) == 0) {
            pLog->err(0, 0, "em31() Body ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        w->pInfo = ModInfoMgr.create(ARC(5), ARC(7));
        if (w->pInfo) {
            em->addModel(w->pInfo);
        }
        em->motFlip = em31_flip0;
        for (p = (cParts*) em->pParts; p; p = p->pNext) {
            p->motParts.flags |= 0x440;
        }
        break;
    case 1:
        if (em->modelInit(ARC(8), ARC(7)) == 0) {
            pLog->err(0, 0, "em31() Tentacle ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        em->motFlip = em31_flip1;
        break;
    }
    em->setStatus(3);
    if (em->type == 0) {
        Em31ClothSet2(em, &w->cloth2);
        Em31ClothSet3(em, &w->cloth3);
    }
#line 941 "D:/Bio4/Prog/em31.cpp"
    em->p2A4 = MEM_ALLOC(0x98, 1, 0xD);
    em->lightInfo.init2(0, 1, &((Vec) { 0.0f, 0.0f, 0.0f }), &((Vec) { 10000.0f, 10000.0f, 10000.0f }), 2);
    atariInitF(&em->atari, 0.0f, 0.0f, 0.0f, 2000.0f, 700.0f, 700.0f, 3000.0f, 1, 0x2000, 10);   // COMPILER-DIFF: #1
    em->litArea.on(1);
    em->atari.setPriority(1);
    if (em->type == 1) {
        em->atari.throughOn();
    }
    switch (em->type) {
    case 0:
    default:
        YarareInit(em, 0.0f, 0.0f, 0.0f, 250.0f, 100.0f, 2, 1);
        YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 250.0f, 0.0f, 0x1B, 1);
        YarareAdd(em, &w->hit[1], 0.0f, 0.0f, 0.0f, 250.0f, 0.0f, 0x1D, 1);
        YarareAdd(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 250.0f, 0.0f, 0x1F, 1);
        YarareAdd(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 250.0f, 0.0f, 0x21, 1);
        YarareAdd(em, &w->hit[4], 0.0f, -200.0f, 0.0f, 250.0f, 200.0f, 0x34, 1);
        YarareAdd(em, &w->hit[5], 0.0f, -200.0f, 0.0f, 250.0f, 200.0f, 0x35, 1);
        YarareAdd(em, &w->hit[6], 0.0f, -200.0f, 0.0f, 250.0f, 200.0f, 0x36, 1);
        YarareAdd(em, &w->hit[7], 0.0f, -200.0f, 0.0f, 250.0f, 200.0f, 0x37, 1);
        YarareAdd(em, &w->hit[8], 0.0f, -200.0f, 0.0f, 250.0f, 200.0f, 0x38, 1);
        YarareAdd(em, &w->hit[9], -200.0f, 0.0f, 0.0f, 150.0f, 200.0f, 3, 3);
        YarareAdd(em, &w->hit[10], -1000.0f, 0.0f, 0.0f, 200.0f, 1000.0f, 4, 3);
        YarareAdd(em, &w->hit[11], -1000.0f, 0.0f, 0.0f, 200.0f, 1000.0f, 5, 3);
        YarareAdd(em, &w->hit[12], -1000.0f, 0.0f, 0.0f, 150.0f, 1000.0f, 6, 3);
        YarareAdd(em, &w->hit[13], -3000.0f, 0.0f, 0.0f, 150.0f, 3000.0f, 7, 3);
        YarareAdd(em, &w->hit[14], 0.0f, 0.0f, 0.0f, 150.0f, 200.0f, 9, 3);
        YarareAdd(em, &w->hit[15], 0.0f, 0.0f, 0.0f, 200.0f, 1000.0f, 0xA, 3);
        YarareAdd(em, &w->hit[16], 0.0f, 0.0f, 0.0f, 200.0f, 1000.0f, 0xB, 3);
        YarareAdd(em, &w->hit[17], 0.0f, 0.0f, 0.0f, 150.0f, 1000.0f, 0xC, 3);
        YarareAdd(em, &w->hit[18], 0.0f, 0.0f, 0.0f, 150.0f, 3000.0f, 0xD, 3);
        YarareAdd(em, &w->hit[19], -200.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0xF, 3);
        YarareAdd(em, &w->hit[20], -1000.0f, 0.0f, 0.0f, 200.0f, 1000.0f, 0x10, 3);
        YarareAdd(em, &w->hit[21], -1000.0f, 0.0f, 0.0f, 200.0f, 1000.0f, 0x11, 3);
        YarareAdd(em, &w->hit[22], -1000.0f, 0.0f, 0.0f, 150.0f, 1000.0f, 0x12, 3);
        YarareAdd(em, &w->hit[23], -3000.0f, 0.0f, 0.0f, 150.0f, 3000.0f, 0x13, 3);
        YarareAdd(em, &w->hit[24], 0.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x15, 3);
        YarareAdd(em, &w->hit[25], 0.0f, 0.0f, 0.0f, 200.0f, 1000.0f, 0x16, 3);
        YarareAdd(em, &w->hit[26], 0.0f, 0.0f, 0.0f, 200.0f, 1000.0f, 0x17, 3);
        YarareAdd(em, &w->hit[27], 0.0f, 0.0f, 0.0f, 150.0f, 1000.0f, 0x18, 3);
        YarareAdd(em, &w->hit[28], 0.0f, 0.0f, 0.0f, 150.0f, 3000.0f, 0x19, 3);
        break;
    case 1:
        YarareInit(em, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0xC, 0);
        YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 200.0f, 0.0f, 0xD, 1);
        YarareAdd(em, &w->hit[1], 0.0f, 0.0f, 0.0f, 150.0f, 500.0f, 2, 1);
        YarareAdd(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 150.0f, 300.0f, 3, 1);
        YarareAdd(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 150.0f, 300.0f, 4, 1);
        YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 150.0f, 300.0f, 5, 1);
        YarareAdd(em, &w->hit[5], 0.0f, 0.0f, 0.0f, 150.0f, 300.0f, 6, 1);
        YarareAdd(em, &w->hit[6], 0.0f, 0.0f, 0.0f, 150.0f, 300.0f, 7, 1);
        YarareAdd(em, &w->hit[7], 0.0f, 0.0f, 0.0f, 150.0f, 300.0f, 8, 1);
        YarareAdd(em, &w->hit[8], 0.0f, 0.0f, 0.0f, 150.0f, 300.0f, 9, 1);
        YarareAdd(em, &w->hit[9], 0.0f, 0.0f, 0.0f, 170.0f, 300.0f, 0xA, 1);
        YarareAdd(em, &w->hit[10], 0.0f, 0.0f, 0.0f, 200.0f, 300.0f, 0xB, 1);
        YarareAdd(em, &w->hit[11], -200.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x11, 3);
        YarareAdd(em, &w->hit[12], -200.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x12, 3);
        YarareAdd(em, &w->hit[13], -200.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x13, 3);
        YarareAdd(em, &w->hit[14], -200.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x14, 3);
        YarareAdd(em, &w->hit[15], -200.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x15, 3);
        YarareAdd(em, &w->hit[16], 0.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x20, 3);
        YarareAdd(em, &w->hit[17], 0.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x21, 3);
        YarareAdd(em, &w->hit[18], 0.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x22, 3);
        YarareAdd(em, &w->hit[19], 0.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x23, 3);
        YarareAdd(em, &w->hit[20], 0.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x24, 3);
        YarareAdd(em, &w->hit[21], 0.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x2F, 5);
        YarareAdd(em, &w->hit[22], 0.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x30, 5);
        YarareAdd(em, &w->hit[23], 0.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x31, 5);
        YarareAdd(em, &w->hit[24], 0.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x32, 5);
        YarareAdd(em, &w->hit[25], 0.0f, 0.0f, 0.0f, 150.0f, 200.0f, 0x33, 5);
        break;
    }
    one = 1;
    em->lockParts = one;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(9), 0x29, 0);
    zero = 0;
    w->espKind = EspPullCoreKind();
    w->x69C = 0.0f;
    w->weakTimer = 450;
    w->flags = zero;
    w->atkWait = zero;
    w->berserkTimer = zero;
    w->pPillar = (cObjPillar*) zero;
    w->x8F8 = zero;
    w->pBody = (cEm31*) zero;
    w->pTen = (cEm31*) zero;
    switch (em->type) {
    case 0:
    default:
        em31EyelidInit(em);
        em31WeakInit(em);
        em->xFC = one;
        em->xFD = zero;
        em->xFE = zero;
        em->xFF = zero;
        MotionSetCore(em, MOTION(em), ARC(0x40), 0, 0, 1, 0);
        MotionMoveF(em, 0);
        break;
    case 1:
        em31WeakInit(em);
        em->xFC = one;
        em->xFD = 0x11;
        em->xFE = zero;
        em->xFF = zero;
        MotionSetCore(em, MOTION(em), ARC(0x5C), 0, 0, 1, 0);
        MotionMoveF(em, 0);
        break;
    }
    em31_R0_Move(em);
}

static void em31_R0_Move(cEm31* em)
{
    Em31_R1_move_tbl[em->xFD * 2](em);
    Em31_R1_move_tbl[em->xFD * 2 + 1](em);
}

static void em31_R1_br_Dummy(cEm31* em)
{
}

static void em31_R1_Appear(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    w->flags |= 0x200;
    switch (em->xFE) {
    case 0:
        em->xFE++;
    case 1:
        MotionSetCore(em, MOTION(em), ARC(0x40), 0, 0, 1, 0);
        MotionMoveF(em, 0);
        if (!(em->flags_3C8 & 1)) {
            break;
        }
        em->xFE++;
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x40), 0, 0, 1, 0);
        em->setStatus(5);
        EstSet((int) em, -1, 0, 0, 0x29, 0x26, 1, w->espKind, (u32) em, 0);
        em->xFE++;
    case 3:
        if (em->frame > 399.7f && em->frame < 400.3f) {
            em31SetTail(em);
        }
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
}

// Attack wait after a tentacle hit, by difficulty.
static inline void em31SetAtkWait(Em31Work* w)
{
    w->atkWait = 60;
    if (pG->x4F88 <= 3) {
        w->atkWait = 90;
    }
    if (pG->x4F88 <= 1) {
        w->atkWait = 120;
    }
    if (pG->x4F88 > 6) {
        w->atkWait = 30;
    }
    if (pG->x4F88 > 9) {
        w->atkWait = 0;
    }
}

static void em31_R1_Wait(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    int ret;

    switch (em->xFE) {
    case 0:
        if (w->flags & 0x40) {
            MotionSetCore(em, MOTION(em), ARC(0x37), 0, 30, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xA), 0, 30, 5, 0);
        }
        em->atari.throughOff();
        w->timer = Rnd() % 60 + 90;
        if (pGS->x4F88 <= 3) {
            w->timer = Rnd() % 60 + 120;
        }
        if (pG->x4F88 <= 1) {
            w->timer = Rnd() % 60 + 150;
        }
        if (pG->x4F88 > 6) {
            w->timer = Rnd() % 60 + 45;
        }
        if (pG->x4F88 == 10) {
            w->timer = 0;
        }
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em31AtkRtnCk(em)) {
            break;
        }
        if (w->pTen && w->pTen->ckAtkHit()) {
            w->atkWait = 60;
            if (pG->x4F88 <= 3) {
                w->atkWait = 90;
            }
            if (pG->x4F88 <= 1) {
                w->atkWait = 120;
            }
            w->timer = Rnd() % 60 + 90;
        }
        if (w->timer) {
            w->timer--;
            break;
        }
        if (em->plDist2 > 64000000.0f) {
            f32 dy = fabsf(em->pos.y - pPL->pos.y);

            if (w->weakTimer == 0 && w->targetAngAbs < 0.5235988f && dy < 100.0f && !(w->flags & 0x40) &&
                pG->x4F88 > 1) {
                EmRoutineSet(em, 1, 5, 0, 0);
                break;
            }
        }
        if (w->routeAngAbs > 0.7853982f) {
            EmRoutineSet(em, 1, 4, 0, 0);
        } else {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    if (em31JumpCk(em) == 0) {
        ret = em31BridgeJumpCk(em);
        if (ret == 0 && em31BridgeVsCk(em, 0)) {
            em31BridgeVsSet(em, w);
        }
    }
}

static void em31_R1_Walk(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    Vec v;
    Vec ep;
    int ret;

    switch (em->xFE) {
    case 0:
        if (w->flags & 0x40) {
            MotionSetCore(em, MOTION(em), ARC(0x38), (int) ARC(0x39), 10, 5, 3);
            w->timer = 5;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xB), (int) ARC(0xC), 10, 5, 3);
            w->timer = 2;
        }
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.049087387f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            if (w->timer == 0) {
                EmRoutineSet(em, 1, 1, 0, 0);
                break;
            }
            w->timer--;
            if (em->plDist2 > 64000000.0f) {
                f32 dy = fabsf(em->pos.y - pPL->pos.y);

                if (w->weakTimer == 0 && w->targetAngAbs < 0.5235988f && dy < 100.0f && !(w->flags & 0x40) &&
                    pG->x4F88 > 1) {
                    EmRoutineSet(em, 1, 5, 0, 0);
                    break;
                }
            }
        }
        if (em31AtkRtnCk(em)) {
            break;
        }
        if (em->motEvent & 1) {
            cModel* p = em->getPartsPtr(7);

            ep = em->pos;
            v = p->worldPos;
            v.y += 500.0f;
            em31AtkCk(em, &v, &ep, 0);
            EstSet(0, -1, &p->worldPos, 0, 0x29, 0x11, 0, 0, 0, 0);
        }
        if (em->motEvent & 2) {
            cModel* p = em->getPartsPtr(0xD);

            ep = em->pos;
            v = p->worldPos;
            v.y += 500.0f;
            em31AtkCk(em, &v, &ep, 0);
            EstSet(0, -1, &p->worldPos, 0, 0x29, 0x11, 0, 0, 0, 0);
        }
        if (w->targetAngAbs > 0.7853982f) {
            EmRoutineSet(em, 1, 4, 0, 0);
        } else {
            if (em->plDist2 < 12250000.0f) {
                EmRoutineSet(em, 1, 1, 0, 0);
                break;
            }
            if (w->pTen == 0) {
                break;
            }
            if (w->pTen->ckAtkHit() == 0) {
                break;
            }
            em31SetAtkWait(w);
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
    if (em31JumpCk(em) == 0) {
        ret = em31BridgeJumpCk(em);
        if (ret == 0 && em31BridgeVsCk(em, 0)) {
            em31BridgeVsSet(em, w);
        }
    }
}

static void em31_R1_Dash(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    Vec v;
    Vec ep;

    w->flags |= 0x8000;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x3C), (int) ARC(0x3D), 10, 5, 3);
        if (w->pTen) {
            w->pTen->setDashAtk();
        }
        w->timer = 5;
        if (pG->x4F88 <= 3) {
            w->timer = 3;
        }
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.049087387f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            if (w->timer == 0) {
                EmRoutineSet(em, 1, 6, 0, 0);
                break;
            }
            w->timer--;
            if (w->targetAngAbs > 2.3561945f) {
                EmRoutineSet(em, 1, 6, 0, 0);
                break;
            }
        }
        if (w->targetAngAbs > 2.3561945f) {
            if ((u8) (Rnd() % 10) > 4) {
                EmRoutineSet(em, 1, 6, 0, 0);
            } else {
                if (w->pTen) {
                    w->pTen->setWait();
                }
                EmRoutineSet(em, 1, 4, 0, 0);
            }
            break;
        }
        if (w->pTen && w->pTen->ckAtkHit() && w->timer > 0) {
            w->timer = 1;
        }
        if (em->motEvent & 1) {
            cModel* p = em->getPartsPtr(7);

            ep = em->pos;
            v = p->worldPos;
            v.y += 500.0f;
            em31AtkCk(em, &v, &ep, 0);
            EstSet(0, -1, &p->worldPos, 0, 0x29, 0x11, 0, 0, 0, 0);
        }
        if (em->motEvent & 2) {
            cModel* p = em->getPartsPtr(0xD);

            ep = em->pos;
            v = p->worldPos;
            v.y += 500.0f;
            em31AtkCk(em, &v, &ep, 0);
            EstSet(0, -1, &p->worldPos, 0, 0x29, 0x11, 0, 0, 0, 0);
        }
        break;
    }
    if (em31BridgeVsCk(em, 0)) {
        em31BridgeVsSet(em, w);
    }
}

static void em31_R1_Turn(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    f32 dy;

    switch (em->xFE) {
    case 0:
        if (w->targetAngAbs > 2.3561945f || em->xFF) {
            if (w->targetAng < 0.0f) {
                MotionSetCore(em, MOTION(em), ARC(0x13), (int) ARC(0x14), 10, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x13), (int) ARC(0x14), 10, 0x41, 0);
            }
        } else if (w->targetAngAbs > 1.0471976f) {
            if (w->targetAng < 0.0f) {
                MotionSetCore(em, MOTION(em), ARC(0x11), (int) ARC(0x12), 10, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x11), (int) ARC(0x12), 10, 0x41, 0);
            }
        } else {
            if (w->targetAng < 0.0f) {
                MotionSetCore(em, MOTION(em), ARC(0x15), (int) ARC(0x16), 10, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x15), (int) ARC(0x16), 10, 0x41, 0);
            }
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em31BridgeVsCk(em, 0)) {
                em31BridgeVsSet(em, w);
            } else if (w->targetAngAbs > 0.7853982f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->flags & 0x40) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if (em->plDist2 > 64000000.0f &&
                       (dy = fabsf(em->pos.y - pPL->pos.y), w->weakTimer == 0 && w->targetAngAbs < 0.5235988f &&
                                                             dy < 100.0f && pG->x4F88 > 1)) {
                EmRoutineSet(em, 1, 5, 0, 0);
            } else {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    }
}

// Jump target of the bridge fight: the near end of the bridge on the giant's side.
// Jump to the far side of the bridge (a macro: with an inline the parameter copies of `em` and `w`
// make the bridgePos stores alias the routine bytes, which pins the store order).
#define EM31_BRIDGE_JUMP_SET()                                                                     \
    w->bridgePos = em->pos;                                                                         \
    if (side) {                                                                                     \
        w->bridgePos.x = -35500.0f;                                                                 \
        w->bridgePos.y = 15861.0f;                                                                  \
    } else {                                                                                        \
        w->bridgePos.x = -52600.0f;                                                                 \
        w->bridgePos.y = 17361.0f;                                                                  \
    }                                                                                               \
    EmRoutineSet(em, 1, 8, 0, 0)

static void em31_R1_BridgeVs(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    int side;
    f32 ang;
    f32 muku;
    f32 mukuAbs;
    f32 m;
    f32 a;
    int ret;

    if (em->pos.x > -44000.0f) {
        side = 0;
    } else {
        side = 1;
    }
    if (side) {
        ang = 1.5707964f;
    } else {
        ang = -1.5707964f;
    }
    muku = Muku(&em->pos, &pPL->pos, em->rot.y, PI);
    mukuAbs = fabsf(muku);
    if (em->xFE == 0) {
        m = Muku2(em->rot.y, ang, PI);
        a = fabsf(m);
        if (a < 0.2617994f) {
            em->xFE = 2;
        }
    }
    w->berserkTimer = 0;
    w->weakTimer = 450;
    w->flags &= ~0x40;
    switch (em->xFE) {
    case 0:
        m = Muku2(em->rot.y, ang, PI);
        a = fabsf(m);
        if (a > 2.3561945f) {
            if (m < 0.0f) {
                MotionSetCore(em, MOTION(em), ARC(0x13), (int) ARC(0x14), 10, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x13), (int) ARC(0x14), 10, 0x41, 0);
            }
        } else if (a > 1.0471976f) {
            if (m < 0.0f) {
                MotionSetCore(em, MOTION(em), ARC(0x11), (int) ARC(0x12), 10, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x11), (int) ARC(0x12), 10, 0x41, 0);
            }
        } else {
            if (m < 0.0f) {
                MotionSetCore(em, MOTION(em), ARC(0x15), (int) ARC(0x16), 10, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x15), (int) ARC(0x16), 10, 0x41, 0);
            }
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        if (muku > 0.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x3E), (int) ARC(0x3F), 10, 0x45, 3);
            w->motVar = 1;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x3E), (int) ARC(0x3F), 10, 5, 3);
            w->motVar = 0;
        }
        em->xFE++;
    case 3:
        em->rot.y += Muku2(em->rot.y, ang, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if (mukuAbs < 0.08726646f) {
            em->xFE = 4;
            break;
        }
        if (em31BridgeVsCk(em, 1)) {
            if (em31PLCraneCk(em)) {
                EM31_BRIDGE_JUMP_SET();
                break;
            }
            if (pPL->pos.x > -31000.0f || pPL->pos.z < 55000.0f) {
                EM31_BRIDGE_JUMP_SET();
                break;
            }
            if (w->motVar) {
                if (muku < 0.0f) {
                    em->xFE = 2;
                    break;
                }
            } else {
                if (muku > 0.0f) {
                    em->xFE = 2;
                    break;
                }
            }
            if (w->pTen == 0) {
                break;
            }
            if (w->pTen->ckAtkEnable() == 0) {
                break;
            }
            ret = em31PillarCk2(em);
            if (ret == 0) {
                break;
            }
            w->pTen->setAtk(0);
            w->pTen->xFF = ret;
            em->xFE = 8;
        } else {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 30, 5, 0);
        em->xFE++;
    case 5:
        em->rot.y += Muku2(em->rot.y, ang, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if (em31BridgeVsCk(em, 1) == 0) {
            EmRoutineSet(em, 1, 1, 0, 0);
            break;
        }
        if (w->atkWait == 0 && w->pTen && w->pTen->ckAtkEnable()) {
            em->xFE++;
            break;
        }
        if (mukuAbs > 0.17453292f) {
            em->xFE = 2;
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0x1C), 0, 30, 1, 0);
        if (w->pTen) {
            w->pTen->setPillarThrow();
        }
        w->pillarCnt++;
        em->xFE++;
    case 7:
        em->rot.y += Muku2(em->rot.y, ang, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            if ((u8) (Rnd() % 10) > 6 || w->pillarCnt > 2) {
                EM31_BRIDGE_JUMP_SET();
            } else {
                em->xFE = 2;
            }
        }
        break;
    case 8:
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 30, 5, 0);
        w->timer = 90;
        em->xFE++;
    case 9:
        em->rot.y += Muku2(em->rot.y, ang, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->xFE = 4;
        }
        break;
    }
}

// Landing hit of the jump: the parts' matrix origin shifted by -3000 on x, raised by 250 (a macro
// for the same reason as EM31_KICK_ATK_CK: the Vec locals are addressed directly).
#define EM31_JUMP_ATK_CK(parts)                                                                    \
    p = em->getPartsPtr(parts);                                                                     \
    v.x = -3000.0f;                                                                                 \
    v.y = 0.0f;                                                                                     \
    v.z = 0.0f;                                                                                     \
    PSMTXMultVec(p->mat, &v, &v);                                                                   \
    v.y += 250.0f;                                                                                  \
    em31AtkCk(em, &v, &ep, 0)

static void em31_R1_Jump(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    Vec v;
    Vec ep;
    cModel* p;

    w->flags |= 0x80;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x17), (int) ARC(0x18), 10, 0, 0);
        if (w->pTen) {
            w->pTen->setJump();
        }
        PSVECSubtract(&w->bridgePos, &em->pos, &w->jumpSpd);
        EstSet((int) em, -1, 0, 0, 0x29, 0x18, 0, 0, (u32) em, 0);
        w->atkHit = 0;
        em->xFE++;
    case 1:
        if (em->motEvent & 2) {
            PSVECScale(&w->jumpSpd, &v, 0.1f);
            PSVECAdd(&em->pos, &v, &em->pos);
            PSVECSubtract(&w->jumpSpd, &v, &w->jumpSpd);
            em->atari.throughOn();
        } else {
            em->atari.throughOff();
        }
        if (em->motEvent & 1) {
            v = em->pos;
            ep = em->pos;
            v.y += 250.0f;
            em31AtkCk(em, &v, &ep, 4);
            EM31_JUMP_ATK_CK(6);
            EM31_JUMP_ATK_CK(0xC);
            EM31_JUMP_ATK_CK(0x12);
            EM31_JUMP_ATK_CK(0x18);
        }
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 0.7853982f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    }
}

static void em31_R1_BerserkStart(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    w->flags |= 0x8000;
    w->berserkTimer = 300;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x33), (int) ARC(0x34), 30, 1, 0);
        if (w->pTen) {
            w->pTen->setBerserkStart();
        }
        w->flags |= 0x40;
        w->weakTimer = 450;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->routeAngAbs > 0.7853982f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        }
        break;
    }
}

static void em31_R1_BerserkEnd(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    w->weakTimer = 450;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x35), 0, 30, 1, 0);
        w->flags &= ~0x40;
        if (w->pTen) {
            w->pTen->setBerserkEnd();
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->routeAngAbs > 0.7853982f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
        break;
    }
}

// End of a stamp / kick: the attack wait when the player was hit, the walk otherwise.
static inline void em31StampEnd(cEm31* em, Em31Work* w)
{
    if (w->atkHit == 0) {
        GameAddPoint(0xB);
    }
    if (w->atkHit && (w->flags & 0x40)) {
        EmRoutineSet(em, 1, 6, 0, 0);
    } else if (w->atkHit) {
        em31SetAtkWait(w);
        EmRoutineSet(em, 1, 1, 0, 0);
    } else {
        EmRoutineSet(em, 1, 2, 0, 0);
    }
}

// Stamp hit: the foot's matrix origin shifted 3000 to the side, the dust effect and the attack.
static inline void em31StampAtkCk(cEm31* em, int parts, f32 x, Vec* v, Vec* ep)
{
    cModel* p = em->getPartsPtr(parts);

    v->x = x;
    v->y = 0.0f;
    v->z = 0.0f;
    *ep = em->pos;
    PSMTXMultVec(p->mat, v, v);
    EstSet(0, -1, v, 0, 0x29, 0x10, 0, 0, 0, 0);
    v->y += 250.0f;
    em31AtkCk(em, v, ep, 1);
}

static void em31_R1_Stamp(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    Vec v;
    Vec ep;
    Vec lp;
    Mtx inv;
    void* m0;
    void* m1;
    int flag;
    cModel* p;

    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &lp);
    switch (em->xFE) {
    case 0:
        if (lp.x > -300.0f && lp.x < 300.0f) {
            em->xFF = Rnd() % 3;
        } else {
            em->xFF = 2;
        }
        switch (em->xFF) {
        case 0:
        default:
            m0 = ARC(0x25);
            m1 = ARC(0x26);
            flag = 1;
            break;
        case 1:
            m0 = ARC(0x25);
            m1 = ARC(0x26);
            flag = 0x41;
            break;
        case 2:
            m0 = ARC(0x27);
            m1 = ARC(0x28);
            flag = 1;
            break;
        }
        if (w->pTen) {
            w->pTen->setStamp(em->xFF);
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 10, flag, 0);
        w->atkHit = 0;
        w->escaped = 0;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em31StampEnd(em, w);
            break;
        }
        if (em->motEvent & 1) {
            if (em->motFlags & 0x40) {
                p = em->getPartsPtr(0xC);
                v.x = 3000.0f;
                v.y = 0.0f;
                v.z = 0.0f;
            } else {
                p = em->getPartsPtr(6);
                v.x = -3000.0f;
                v.y = 0.0f;
                v.z = 0.0f;
            }
            ep = em->pos;
            PSMTXMultVec(p->mat, &v, &v);
            EstSet(0, -1, &v, 0, 0x29, 0x10, 0, 0, 0, 0);
            v.y += 250.0f;
            em31AtkCk(em, &v, &ep, 1);
        }
        if (em->motEvent & 2) {
            if (em->motFlags & 0x40) {
                p = em->getPartsPtr(6);
                v.x = -3000.0f;
                v.y = 0.0f;
                v.z = 0.0f;
            } else {
                p = em->getPartsPtr(0xC);
                v.x = 3000.0f;
                v.y = 0.0f;
                v.z = 0.0f;
            }
            ep = em->pos;
            PSMTXMultVec(p->mat, &v, &v);
            EstSet(0, -1, &v, 0, 0x29, 0x10, 0, 0, 0, 0);
            v.y += 250.0f;
            em31AtkCk(em, &v, &ep, 1);
        }
        if ((em->motEvent & 4) && w->atkHit == 0 && w->escaped == 0) {
            if (lp.x > -2500.0f && lp.x < 2500.0f && lp.y > -500.0f && lp.y < 500.0f && lp.z > 0.0f &&
                lp.z < 5000.0f) {
                ActBtn.set(0x25, 0xB, (int) em31ActEscape, (int) em, 1, 3, 0, 0);
            }
        }
        break;
    }
}

static void em31ActEscape(cEm31* em)
{
    EM31_WK(em)->escaped = 1;
    SetPlDamage((int) em, plemEscape);
}

static void plemEscape(cPlayer* pl)
{
    pl->subArc = PL_EM(pl)->subArc;
    pl->dmType = 2;
    switch (pl->xFE) {
    case 0:
        if (pl->xFF) {
            MotionSetCore(pl, MOTION(pl), PL_ARC(0x6D), (int) PL_ARC(0x6E), 3, 0x41, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC(0x6D), (int) PL_ARC(0x6E), 3, 1, 0);
        }
        SndCall(1, 0x48, &pl->pos, 0, 0, pl);
        SndCall(1, 0x11, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        pl->x3E0 = 50;
        pl->x3E4 = 15;
        pl->xFE++;
    case 1:
        em31EscapeCamMove(PL_EM(pl));
        if (pl->x3E4) {
            pl->rot.y += Muku(&pl->pos, &PL_EM(pl)->pos, pl->rot.y, 0.19634955f);
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        }
        MotionMoveF(pl, 0);
        if (pl->frame > 11.7f && pl->frame < 12.3f) {
            EstSet(0, -1, &pl->pos, 0, 3, 0x13, 0, 0, 0, 0);
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if (pl->x3E0) {
            pl->x3E0--;
        } else {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

void em31EscapeCamMove(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    GlobalWork* g = pG;
    Vec a;
    Vec b;
    Vec c;

    w->cam.param.fovy = g->Cam.param.fovy;
    a.x = -376.0f;
    a.y = 575.0f;
    a.z = -1831.0f;
    b.x = -244.0f;
    b.y = 809.0f;
    b.z = 52.6f;
    PSMTXMultVec(pPLS->mat, &a, &a);
    PSMTXMultVec(pPL->mat, &b, &b);
    PosToPos(&g->Cam.param.at, &b, &w->cam.param.at, 1.0f);
    PosToPos(&g->Cam.param.pos, &a, &w->cam.param.pos, 1.0f);
    if (EatMgr.hitCheck(&w->cam.param.at, &w->cam.param.pos, &c, 0, 0x8000, 0)) {
        Vec d;
        f32 len;

        PSVECSubtract(&c, &w->cam.param.at, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 2400 "D:/Bio4/Prog/em31.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&w->cam.param.at, &d, &w->cam.param.pos);
    }
    w->cam.up.x = 0.0f;
    w->cam.up.y = 1.0f;
    w->cam.up.z = 0.0f;
    w->cam.dist = SQRTF((w->cam.param.pos.x - w->cam.param.at.x) * (w->cam.param.pos.x - w->cam.param.at.x) +
                        (w->cam.param.pos.y - w->cam.param.at.y) * (w->cam.param.pos.y - w->cam.param.at.y) +
                        (w->cam.param.pos.z - w->cam.param.at.z) * (w->cam.param.pos.z - w->cam.param.at.z));
    CameraSetOrientationUp(&w->cam);
    CamCtrl.x250 = (s32) &w->cam;
}

// Kick hit: the foot's matrix origin shifted by `x`, raised by 250.
// Kick hit points along the foot parts' local x axis; a macro: the Vec locals must be addressed
// directly (a Vec* parameter makes the compiler keep the address in a register).
#define EM31_KICK_ATK_CK(xx)                                                                       \
    v.x = xx;                                                                                       \
    v.y = 0.0f;                                                                                     \
    v.z = 0.0f;                                                                                     \
    PSMTXMultVec(p->mat, &v, &v);                                                                   \
    v.y += 250.0f;                                                                                  \
    em31AtkCk(em, &v, &ep, 3)

static void em31_R1_Kick(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    Vec v;
    Vec ep;
    void* m0;
    void* m1;
    int flag;
    cModel* p;

    switch (em->xFE) {
    case 0:
        em->xFF = Rnd() & 1;
        switch (em->xFF) {
        case 0:
        default:
            m0 = ARC(0x23);
            m1 = ARC(0x24);
            flag = 1;
            break;
        case 1:
            m0 = ARC(0x23);
            m1 = ARC(0x24);
            flag = 0x41;
            break;
        }
        if (w->pTen) {
            w->pTen->setWait();
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 10, flag, 0);
        if (w->pTen) {
            w->pTen->setVoice(0x19, 90);
        }
        w->atkHit = 0;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em31StampEnd(em, w);
            break;
        }
        if (em->motEvent & 2) {
            ep = em->pos;
            if (em->motFlags & 0x40) {
                p = em->getPartsPtr(6);
                EM31_KICK_ATK_CK(-3000.0f);
                EM31_KICK_ATK_CK(-2500.0f);
                EM31_KICK_ATK_CK(-2000.0f);
            } else {
                p = em->getPartsPtr(0xC);
                EM31_KICK_ATK_CK(3000.0f);
                EM31_KICK_ATK_CK(2500.0f);
                EM31_KICK_ATK_CK(2000.0f);
            }
        }
        break;
    }
}

// Catch check of the branch routines: the tentacle's grabbing parts within range of the player
// (a macro: an inline parameter for the range would be loaded as a pseudo before the tests).
#define EM31_CATCH_BR_CK(parts, range, rtn)                                                         \
    if (w->pTen == 0) {                                                                             \
        return;                                                                                     \
    }                                                                                               \
    if (!(w->pTen->motEvent & 2)) {                                                                 \
        return;                                                                                     \
    }                                                                                               \
    if (em31DeadCk(pPL)) {                                                                          \
        return;                                                                                     \
    }                                                                                               \
    if ((s16) pG->pl_life <= 0) {                                                                   \
        return;                                                                                     \
    }                                                                                               \
    p = w->pTen->getPartsPtr(parts);                                                                \
    if (!((pPL->pos.x - p->worldPos.x) * (pPL->pos.x - p->worldPos.x) +                             \
              (pPL->pos.z - p->worldPos.z) * (pPL->pos.z - p->worldPos.z) >                         \
          range)) {                                                                                 \
        pPL->dmType = 2;                                                                            \
        em->dmType = 2;                                                                             \
        w->pTen->dmType = 2;                                                                        \
        EmRoutineSet(em, 1, rtn, 0, 0);                                                             \
        VibSetData(VIB_TBL, 0xB, 1);                                                                \
    }

static void em31_R1_br_Catch(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    cModel* p;

    EM31_CATCH_BR_CK(0xD, 2250000.0f, 0xC);
}

static void em31_R1_Catch(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x19), 0, 10, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x29, 0x20, 0, 0, (u32) em, 0);
        if (w->pTen) {
            w->pTen->setCatch();
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            GameAddPoint(0xB);
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
}

// End of a catch: collision back on, the berserk over, the attack wait by difficulty.
static inline void em31CatchEnd(cEm31* em, Em31Work* w)
{
    em->atari.throughOff();
    w->berserkTimer = 0;
    w->weakTimer = 450;
    w->atkWait = 60;
    w->flags &= ~0x40;
    if (pGS->x4F88 <= 3) {
        w->atkWait = 90;
    }
    if (pG->x4F88 <= 1) {
        w->atkWait = 120;
    }
    if (pG->x4F88 > 6) {
        w->atkWait = 30;
    }
    if (pG->x4F88 > 9) {
        w->atkWait = 0;
    }
    EmRoutineSet(em, 1, 1, 0, 0);
}

static void em31_R1_CatchHit(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    em->dmType = 2;
    w->flags |= 0x80;
    switch (em->xFE) {
    case 0:
        em->atari.throughOn();
        em31CatchPosSet(em, 0);
        MotionSetCore(em, MOTION(em), ARC(0x1A), (int) ARC(0x1B), 0, 1, 0);
        SetPlDamage((int) em, plem31_CatchHit);
        if (w->pTen) {
            w->pTen->setCatchHit();
        }
        w->timer = 30;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            em->atari.throughOff();
        }
        if (MotionMoveF(em, 0)) {
            em31CatchEnd(em, w);
        }
        break;
    }
}

// The caught player: carried in the giant's hand (case 0/1), squeezed (2/3).
// The player's catch-hit routine shared by the normal and the step catch (a macro: the `pl`
// parameter copy of an inline would pin the pos store order).
#define PLEM31_CATCH_HIT_SUB(mot, est)                                                             \
    BitOn(pG->flags_5010, 0x8000);                                                                  \
    pl->dmg.set(0, 10);                                                                             \
    pl->subArc = PL_EM(pl)->subArc;                                                                 \
    switch (pl->xFE) {                                                                              \
    case 0:                                                                                         \
        pl->pos.x = 0.0f;                                                                           \
        pl->pos.z = 4500.0f;                                                                        \
        pl->pos.y = 0.0f;                                                                           \
        PSMTXMultVec(PL_EM(pl)->mat, &pl->pos, &pl->pos);                                           \
        pl->rot.y = PL_EM(pl)->rot.y + PI;                                                          \
        pl->rot.y = LIMIT_ANGLE(pl->rot.y);                                                         \
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, mot), 0, 0, 1, 0);                     \
        PlSetFace(1);                                                                               \
        EstSet((int) pl, -1, 0, 0, 0x29, est, 0, 0, (u32) pl, 0);                                   \
        SndCall(1, 0xC, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);                                   \
        pl->atari.throughOn();                                                                      \
        pl->xFE++;                                                                                  \
    case 1:                                                                                         \
        if (MotionMoveF(pl, 0) && (s16) pG->pl_life > 0) {                                          \
            pl->xFE++;                                                                              \
            break;                                                                                  \
        }                                                                                           \
        if (pl->frame > 85.7f && pl->frame < 86.3f) {                                               \
            SndCall(8, 0x2E, &pl->pos, PL_EM(pl)->id, 0, pl);                                       \
            VibSetData(VIB_TBL, 0xB, 1);                                                            \
            LifeDownSet(pPL, 900, 0);                                                               \
            if ((s16) pG->pl_life > 0) {                                                            \
                PlSetDamageSe(0);                                                                   \
            } else {                                                                                \
                PlSetDamageSe(0xD);                                                                 \
            }                                                                                       \
        }                                                                                           \
        if (pl->frame > 101.7f && pl->frame < 102.3f) {                                             \
            SndCall(5, 4, &pl->pos, 0, 0, pl);                                                      \
        }                                                                                           \
        break;                                                                                      \
    case 2:                                                                                         \
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x73), 0, 5, 1, 0);                                    \
        EstSet((int) pl, -1, 0, 0, 0x29, 0x38, 0, 0, (u32) pl, 0);                                  \
        pl->xFE++;                                                                                  \
    case 3:                                                                                         \
        if (MotionMoveF(pl, 0)) {                                                                   \
            EndPlDamage();                                                                          \
            pl->dmg.set(0, 30);                                                                     \
        } else if (pl->frame > 28.7f && pl->frame < 29.3f) {                                        \
            SndCall(1, 4, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);                                 \
        }                                                                                           \
        break;                                                                                      \
    }                                                                                               \
    pl->x3A8 = pl->pos;                                                                             \
    pl->subArc = pl->subArc2;                                                                      

static void plem31_CatchHit(cPlayer* pl)
{
    PLEM31_CATCH_HIT_SUB(0x74, 0x36);
}

static void em31_R1_br_StepCatch(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    cModel* p;

    EM31_CATCH_BR_CK(0xB, 4000000.0f, 0xE);
}

static void em31_R1_StepCatch(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x19), 0, 10, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x29, 0x20, 0, 0, (u32) em, 0);
        if (w->pTen) {
            w->pTen->setStepCatch();
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            GameAddPoint(0xB);
            w->weakTimer = 450;
            w->berserkTimer = 0;
            w->flags &= ~0x40;
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
}

static void em31_R1_StepCatchHit(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    em->dmType = 2;
    w->flags |= 0x80;
    switch (em->xFE) {
    case 0:
        em->atari.throughOn();
        em31CatchPosSet(em, 0);
        MotionSetCore(em, MOTION(em), ARC(0x1A), (int) ARC(0x1B), 0, 1, 0);
        SetPlDamage((int) em, plem31_StepCatchHit);
        if (w->pTen) {
            w->pTen->setStepCatchHit();
        }
        w->timer = 30;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            em->atari.throughOff();
        }
        if (MotionMoveF(em, 0)) {
            em31CatchEnd(em, w);
        }
        break;
    }
}

static void plem31_StepCatchHit(cPlayer* pl)
{
    PLEM31_CATCH_HIT_SUB(0x72, 0x37);
}

// End of a tentacle attack routine: the wait when the player was hit, the walk / turn otherwise.
static inline void em31AtkEnd(cEm31* em, Em31Work* w)
{
    if (w->pTen->ckAtkHit()) {
        em31SetAtkWait(w);
    }
    if (MotionMoveF(em, 0)) {
        if (w->targetAngAbs > 0.7853982f) {
            EmRoutineSet(em, 1, 4, 0, 0);
        } else if (w->atkWait) {
            EmRoutineSet(em, 1, 1, 0, 0);
        } else {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
    }
}

static void em31_R1_HeadAtk(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    Vec pp;

    GetPlPos(&pp, 0, 18.0f);
    switch (em->xFE) {
    case 0: {
        f32 ang = Muku(&em->pos, &pp, em->rot.y, PI);
        f32 a = fabsf(ang);
        u32 no;

        no = 0;
        if (a > 0.34906584f) {
            no = 1;
        }
        if (a > 1.5707964f) {
            no = 3;
            if (ang < 0.0f) {
                no = 2;
            }
        }
        if (em->xFF) {
            no = em->xFF;
        }
        switch (no) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x1F), (int) ARC(0x20), 30, 1, 0);
            if (w->pTen) {
                w->pTen->setAtk(0);
            }
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0x1D), (int) ARC(0x1E), 30, 1, 0);
            if (w->pTen) {
                w->pTen->setAtk(1);
            }
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(0x21), (int) ARC(0x22), 30, 1, 0);
            if (w->pTen) {
                w->pTen->setAtk(2);
            }
            break;
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x21), (int) ARC(0x22), 30, 0x41, 0);
            if (w->pTen) {
                w->pTen->setAtk(3);
            }
            break;
        case 4:
            MotionSetCore(em, MOTION(em), ARC(0x1D), (int) ARC(0x1E), 30, 0x41, 0);
            if (w->pTen) {
                w->pTen->setAtk(4);
            }
            break;
        }
        em->xFE++;
    }
    case 1:
        em31AtkEnd(em, w);
        break;
    }
}

static void em31_R1_BackAtk(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    Vec pp;

    GetPlPos(&pp, 0, 18.0f);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0xE), 30, 1, 0);
        if (w->pTen) {
            w->pTen->setAtk(0);
        }
        em->xFE++;
    case 1:
        em31AtkEnd(em, w);
        break;
    }
}

static void em31_R1_T_Appear(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    em31SearchBody(em);
    switch (em->xFE) {
    case 0:
        em->xFE++;
    case 1:
        MotionSetCore(em, MOTION(em), ARC(0x5C), 0, 0, 1, 0);
        MotionMoveF(em, 0);
        if (!(em->flags_3C8 & 1)) {
            break;
        }
        em->xFE++;
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x5C), 0, 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x29, 0x24, 1, w->espKind, (u32) em, 0);
        EstSet(0, -1, 0, 0, 0x29, 0x25, 1, w->espKind, (u32) em, 0);
        em->setStatus(5);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
        }
        break;
    }
    em31WeakMode(em, 0);
}

static void em31_R1_T_Wait(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    void* mot;
    int a;
    int v;

    em31SearchBody(em);
    w->atkEnable = 1;
    switch (em->xFE) {
    case 0:
        mot = ARC(0x44);
        w->motVar = 0;
        if (w->pBody) {
            if (w->pBody->ckBerserk()) {
                mot = ARC(0x45);
                w->motVar = 1;
            }
            if (w->pBody->ckEyeBreak()) {
                mot = ARC(0x6A);
                w->motVar = 0;
            }
        }
        if (em->xFF) {
            a = 0;
        } else {
            a = 10;
        }
        MotionSetCore(em, MOTION(em), mot, 0, a, 5, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        v = 0;
        if (w->pBody && (w->pBody->ckBerserk() || w->pBody->ckEyeBreak())) {
            v = 1;
        }
        if (w->motVar != v) {
            em->xFE = 0;
        }
        break;
    }
    em31WeakMode(em, 0);
    if (w->pBody && (w->pBody->ckBerserk() || w->pBody->ckEyeBreak())) {
        em31WeakMode(em, 1);
    }
    em31BreathSe(em);
}

static void em31_R1_T_Stamp(cEm31* em)
{
    void* mot;
    int flag;
    int a;

    em31SearchBody(em);
    switch (em->xFE) {
    case 0:
        switch (em->xFF) {
        case 0:
        default:
            mot = ARC(0x67);
            flag = 1;
            a = 10;
            break;
        case 1:
            mot = ARC(0x67);
            flag = 0x41;
            a = 10;
            break;
        case 2:
            mot = ARC(0x66);
            flag = 1;
            a = 0;
            break;
        }
        em->setVoice(0x19, 2);
        MotionSetCore(em, MOTION(em), mot, 0, a, flag, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em->xFF == 2) {
                EmRoutineSet(em, 1, 0x12, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x12, 0, 0);
            }
        }
        break;
    }
    em31WeakMode(em, 0);
    em31BreathSe(em);
}

// The tentacle attack: every parts of the whip against the player, from the root's world position.
static inline void em31TenAtkCk(cEm31* em, int no)
{
    cModel* p0;

    em31TenMatCalc(em);
    p0 = em->getPartsPtr(0);
    em31AtkCk(em, &em->getPartsPtr(2)->worldPos, &p0->worldPos, no);
    em31AtkCk(em, &em->getPartsPtr(3)->worldPos, &p0->worldPos, no);
    em31AtkCk(em, &em->getPartsPtr(4)->worldPos, &p0->worldPos, no);
    em31AtkCk(em, &em->getPartsPtr(5)->worldPos, &p0->worldPos, no);
    em31AtkCk(em, &em->getPartsPtr(6)->worldPos, &p0->worldPos, no);
    em31AtkCk(em, &em->getPartsPtr(7)->worldPos, &p0->worldPos, no);
    em31AtkCk(em, &em->getPartsPtr(8)->worldPos, &p0->worldPos, no);
    em31AtkCk(em, &em->getPartsPtr(9)->worldPos, &p0->worldPos, no);
    em31AtkCk(em, &em->getPartsPtr(0xA)->worldPos, &p0->worldPos, no);
    em31AtkCk(em, &em->getPartsPtr(0xB)->worldPos, &p0->worldPos, no);
    em31AtkCk(em, &em->getPartsPtr(0x11)->worldPos, &p0->worldPos, no);
    em31AtkCk(em, &em->getPartsPtr(0x20)->worldPos, &p0->worldPos, no);
    em31AtkCk(em, &em->getPartsPtr(0x34)->worldPos, &p0->worldPos, no);
    em31AtkCk(em, &em->getPartsPtr(0x1C)->worldPos, &p0->worldPos, no);
    em31AtkCk(em, &em->getPartsPtr(0x2B)->worldPos, &p0->worldPos, no);
    em31AtkCk(em, &em->getPartsPtr(0x3A)->worldPos, &p0->worldPos, no);
}

static void em31_R1_T_Atk(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    void* m0;
    void* m1;
    int flag;
    int no;

    em31SearchBody(em);
    switch (em->xFE) {
    case 0:
        switch (em->xFF) {
        case 0:
        default:
            m0 = ARC(0x47);
            m1 = ARC(0x48);
            flag = 1;
            break;
        case 1:
            m0 = ARC(0x49);
            m1 = ARC(0x4A);
            flag = 1;
            break;
        case 2:
            m0 = ARC(0x4B);
            m1 = ARC(0x4C);
            flag = 1;
            break;
        case 3:
            m0 = ARC(0x4B);
            m1 = ARC(0x4C);
            flag = 0x41;
            break;
        case 4:
            m0 = ARC(0x49);
            m1 = ARC(0x4A);
            flag = 0x41;
            break;
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 3, flag, 0);
        EstSet((int) em, -1, 0, 0, 0x29, 0x1A, 0, 0, (u32) em, 0);
        em->setVoice(0x19, 2);
        w->atkHit = 0;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(0xB);
            }
            if (em->xFF) {
                EmRoutineSet(em, 1, 0x12, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0x12, 0, 1);
            }
        } else if (em->motEvent & 1) {
            no = 5;
            if (em->xFF) {
                no = 6;
            }
            em31TenAtkCk(em, no);
        }
        break;
    }
    em31WeakMode(em, 0);
}

static void em31_R1_T_DashAtk(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    em31SearchBody(em);
    w->atkEnable = 1;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x64), (int) ARC(0x65), 3, 5, 0);
        EstSet((int) em, -1, 0, 0, 0x29, 0x1A, 0, 0, (u32) em, 0);
        w->atkHit = 0;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(0xB);
            }
            EstSet((int) em, -1, 0, 0, 0x29, 0x1A, 0, 0, (u32) em, 0);
        }
        if (em->motEvent & 1) {
            em31TenAtkCk(em, 5);
        }
        if (em->frame > 21.7f && em->frame < 22.3f) {
            em->setVoice(0x19, 2);
        }
        break;
    }
    em31WeakMode(em, 1);
}

static void em31_R1_T_BerserkStart(cEm31* em)
{
    em31SearchBody(em);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x62), 0, 3, 1, 0);
        em->setVoice(0x19, 2);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
        }
        break;
    }
    em31WeakMode(em, 1);
}

static void em31_R1_T_BerserkEnd(cEm31* em)
{
    em31SearchBody(em);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x63), 0, 3, 1, 0);
        em->setVoice(0x37, 2);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
        }
        break;
    }
    em31WeakMode(em, 0);
}

static void em31_R1_T_Jump(cEm31* em)
{
    em31SearchBody(em);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x4D), 0, 3, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
        } else if (em->frame > 18.7f && em->frame < 19.3f) {
            em->setVoice(0x19, 2);
        }
        break;
    }
    em31WeakMode(em, 0);
}

static void em31_R1_T_Catch(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    Mtx inv;
    Vec lp;
    int near;

    em31SearchBody(em);
    switch (em->xFE) {
    case 0:
        near = 0;
        if (w->pBody) {
            PSMTXInverse(em->mat, inv);
            PSMTXMultVec(inv, &pPL->pos, &lp);
            if (lp.z < 3000.0f) {
                near = 1;
            }
        }
        if (near) {
            MotionSetCore(em, MOTION(em), ARC(0x68), (int) ARC(0x69), 0, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x58), (int) ARC(0x59), 0, 1, 0);
        }
        em->setVoice(0x19, 2);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
        }
        break;
    }
    em31WeakMode(em, 0);
}

static void em31_R1_T_CatchHit(cEm31* em)
{
    em31SearchBody(em);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x5A), (int) ARC(0x5B), 0, 1, 0);
        EstSet(0, -1, 0, 0, 0x29, 0x21, 0, 0, 0, 0);
        SndCall(8, 0x28, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x12, 0, 1);
        }
        break;
    }
    em31WeakMode(em, 0);
}

static void em31_R1_T_StepCatch(cEm31* em)
{
    em31SearchBody(em);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x4E), (int) ARC(0x4F), 0, 1, 0);
        em->setVoice(0x19, 2);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
        }
        break;
    }
    em31WeakMode(em, 0);
}

static void em31_R1_T_StepCatchHit(cEm31* em)
{
    em31SearchBody(em);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x50), (int) ARC(0x51), 0, 1, 0);
        EstSet(0, -1, 0, 0, 0x29, 0x21, 0, 0, 0, 0);
        SndCall(8, 0x28, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x12, 0, 1);
        }
        break;
    }
    em31WeakMode(em, 0);
}

static void em31_R1_T_PillarThrow(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    Vec v;

    em31SearchBody(em);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x60), (int) ARC(0x61), 3, 1, 0);
        em->setVoice(0x19, 2);
        w->atkHit = 0;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
            break;
        }
        if ((em->motEvent & 2) && w->pBody) {
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 2500.0f;
            PSMTXMultVec(w->pBody->mat, &v, &v);
            w->pPillar = (cObjPillar*) SetPillar(ARC(0x7A), ARC(0x7B), &v, &w->pBody->rot);
            if (w->pPillar) {
                w->pPillar->setThrow(ARC(0x7C), ARC(0x7D), ARC(0x7E), ARC(0x75), 0);
            }
        }
        if ((em->motEvent & 1) && w->pPillar) {
            w->pPillar = 0;
        }
        if (em->motEvent & 4) {
            em->setVoice(0x19, 2);
        }
        break;
    }
    em31WeakMode(em, 0);
}

// Drops the pillar the tentacle holds (damage / down / crane hit).
static inline void em31PillarDrop(cEm31* em, Em31Work* w)
{
    if (w->pPillar) {
        w->pPillar->setFall(ARC(0x7D), ARC(0x7F));
        w->pPillar = 0;
    }
}

static void em31_R1_T_Dm_Normal(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    w->flags |= 0x10;
    em31SearchBody(em);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x46), 0, 3, 1, 0);
        em31PillarDrop(em, w);
        w->totalDamage = 0;
        em->setVoice(0x1C, 2);
        em31WeakMode(em, 0);
        em->flags_3C8 &= ~1;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
        }
        break;
    }
}

static void em31_R1_T_Down(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    w->flags |= 0x10;
    em31SearchBody(em);
    w->atkEnable = 1;
    switch (em->xFE) {
    case 0:
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(0x53), (int) ARC(0x54), 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x53), (int) ARC(0x54), 3, 1, 0);
        }
        w->flags |= 0x1000;
        em31PillarDrop(em, w);
        w->totalDamage = 0;
        em->setVoice(0x1C, 2);
        em31WeakMode(em, 0);
        em->flags_3C8 &= ~1;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->flags &= ~0x2000;
            w->downMot = 0;
            em->xFE++;
        }
        break;
    case 2:
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(0x55), 0, w->downMot, 0x45, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x55), 0, w->downMot, 5, 0);
        }
        em31WeakMode(em, 1);
        w->timer = 0;
        em->xFE++;
    case 3:
        if (w->timer % 3 == 0) {
            EstSet((int) em, -1, 0, 0, 0x29, 0x17, 0, 0, (u32) em, 0);
        }
        w->timer++;
        MotionMoveF(em, 0);
        if (em->flags_3C8 & 1) {
            em->xFE++;
        } else {
            em31BreathSe(em);
        }
        break;
    case 4:
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(0x56), 0, 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x56), 0, 3, 1, 0);
        }
        if (w->flags & 0x2000) {
            EstSet((int) em, -1, 0, 0, 0x29, 0x16, 0, 0, (u32) em, 0);
        }
        em31WeakMode(em, 0);
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
        }
        break;
    case 6:
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(0x52), 0, 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x52), 0, 3, 1, 0);
        }
        em31WeakMode(em, 1);
        em->setVoice(0x1C, 2);
        em->xFE++;
    case 7:
        if (w->timer % 3 == 0) {
            EstSet((int) em, -1, 0, 0, 0x29, 0x17, 0, 0, (u32) em, 0);
        }
        w->timer++;
        if (MotionMoveF(em, 0)) {
            w->downMot = 0;
            em->xFE = 2;
        } else if (em->flags_3C8 & 1) {
            em->xFE = 4;
        }
        break;
    }
}

static void em31_R1_T_Dm_Crane(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    w->flags |= 0x10;
    em31SearchBody(em);
    w->atkEnable = 1;
    switch (em->xFE) {
    case 0:
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(0x57), 0, 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x57), 0, 3, 1, 0);
        }
        em31PillarDrop(em, w);
        em->setVoice(0x1C, 2);
        em31WeakMode(em, 1);
        em->flags_3C8 &= ~1;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFC = 1;
            em->xFD = 0x1F;
            em->xFE = 2;
        }
        break;
    }
}

static void em31_R1_T_Dm_Climb(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    w->flags |= 0x10;
    em31SearchBody(em);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x55), 0, 0, 1, 0);
        w->totalDamage = 0;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
            break;
        }
        if (em->frame > 40.7f && em->frame < 41.3f) {
            em->xFE++;
            break;
        }
        em31BreathSe(em);
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x81), 0, 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x29, 0x30, 0, 0, (u32) em, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        } else {
            if (em->frame > 77.7f && em->frame < 78.3f) {
                em->setVoice(0x1F, 150);
            }
            em31BreathSe(em);
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x56), 0, 0, 1, 0);
        em31WeakMode(em, 0);
        EstSet((int) em, -1, 0, 0, 0x29, 0x35, 0, 0, (u32) em, 0);
        em->setVoice(0x37, 2);
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            LifeDownSet(em, (s16) (em->hpMax / 20), 0);
            EmRoutineSet(em, 1, 0x12, 0, 0);
        }
        break;
    }
}

static void em31_R1_T_Die(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    w->atkEnable = 0;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x5D), 0, 0, 1, 0);
        em->hp = 0;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x5E), 0, 0, 1, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->clearStatus(5);
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x5F), 0, 0, 1, 0);
        em->xFE++;
    case 5:
        MotionMoveF(em, 0);
        break;
    }
}

static void em31_R0_Damage(cEm31* em)
{
    EM31_WK(em)->flags |= 8;
    Em31_R2_move_tbl[em->xFD](em);
}

// Routine after a damage reaction: back to the bridge fight when the player crossed, the walk otherwise.
static inline void em31DmEnd(cEm31* em, Em31Work* w)
{
    if (em31BridgeVsCk(em, 0)) {
        em31BridgeVsSet(em, w);
    } else {
        EmRoutineSet(em, 1, 2, 0, 0xA);
    }
}

static void em31_R1_Dm_Normal(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x29), (int) ARC(0x2A), 3, 1, 0);
        if (w->pTen) {
            w->pTen->setDmNormal();
        }
        w->flags &= ~0x40;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em31DmEnd(em, w);
        }
        break;
    }
}

// The player may climb the fallen giant's back: the action button while it lies near.
static inline void em31ClimbBtnCk(cEm31* em)
{
    f32 dy = fabsf(em->pos.y - pPLS->pos.y);

    if (em->plDist2 < 56250000.0f && dy < 100.0f) {
        ActBtn.set(0x19, 0xB, (int) em31SetActClimb, (int) em, 0, 1, 0, 0);
    }
}

static void em31_R1_Dm_Down(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    int flag;
    int ret;

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        flag = 0x201;
        if (w->flags & 0x1000) {
            flag = 0x100;
        }
        w->flags |= 0x1000;
        em->xFF = w->downType;
        switch (em->xFF) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x2B), (int) ARC(0x2C), 3, (u16) flag, 0);
            if (w->pTen) {
                w->pTen->setDown(0);
            }
            break;
        case 1:
            flag |= 0x40;
            MotionSetCore(em, MOTION(em), ARC(0x2B), (int) ARC(0x2C), 3, (u16) flag, 0);
            if (w->pTen) {
                w->pTen->setDown(1);
            }
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(0x2D), 0, 3, (u16) flag, 0);
            if (w->pTen) {
                w->pTen->setDown(0);
            }
            break;
        case 3:
            flag |= 0x40;
            MotionSetCore(em, MOTION(em), ARC(0x2D), 0, 3, (u16) flag, 0);
            if (w->pTen) {
                w->pTen->setDown(1);
            }
            break;
        }
        w->flags &= ~0x40;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        switch (em->xFF) {
        case 0:
        case 2:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x2F), 0, 3, 5, 0);
            break;
        case 1:
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x2F), 0, 3, 0x45, 0);
            break;
        }
        em->xFE++;
    case 3:
        MotionMoveF(em, 0);
        if (w->pTen && w->pTen->ckWeakDamage()) {
            if (w->pTen->getTotalDamage() < (s16) (em->hpMax / 20)) {
                em->xFE = 6;
                break;
            }
            w->downTimer = 0;
        }
        if (w->downTimer == 0) {
            em->xFE++;
            break;
        }
        w->downTimer--;
        em31ClimbBtnCk(em);
        break;
    case 4:
        switch (em->xFF) {
        case 0:
        case 2:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x30), (int) ARC(0x31), 3, 1, 0);
            break;
        case 1:
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x30), (int) ARC(0x31), 3, 0x41, 0);
            break;
        }
        if (w->pTen) {
            w->pTen->flags_3C8 |= 1;
        }
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            em31DmEnd(em, w);
        }
        break;
    case 6:
        if (em->motFlags & 0x40) {
            MotionSetCore(em, MOTION(em), ARC(0x36), 0, 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x36), 0, 3, 1, 0);
        }
        w->pTen->setDownDamage();
        em->xFE++;
    case 7:
        ret = MotionMoveF(em, 0);
        if (ret) {
            em->xFE = 2;
            break;
        }
        if (w->pTen && w->pTen->ckWeakDamage() && w->pTen->getTotalDamage() >= (s16) (em->hpMax / 20)) {
            w->downTimer = ret;
        }
        if (w->downTimer == 0) {
            em->xFE = 4;
            break;
        }
        w->downTimer--;
        em31ClimbBtnCk(em);
        break;
    }
}

static void em31_R1_Dm_Crane(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(0x32), 0, 3, 0x41, 0);
            if (w->pTen) {
                w->pTen->setDamageCrane(1);
            }
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x32), 0, 3, 1, 0);
            if (w->pTen) {
                w->pTen->setDamageCrane(0);
            }
        }
        w->flags &= ~0x40;
        em->atari.throughOff();
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->downTimer = 210;
            em->xFC = 2;
            em->xFD = 1;
            em->xFE = 2;
        }
        break;
    }
}

static void em31_R1_Dm_Climb(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    em->dmType = 2;
    w->flags |= 0x80;
    switch (em->xFE) {
    case 0:
        em->atari.throughOn();
        em31CatchPosSet(em, 1);
        MotionSetCore(em, MOTION(em), ARC(0x2F), 0, 0, 1, 0);
        SetPlDamage((int) em, plem31_Climb);
        if (w->pTen) {
            w->pTen->setClimb();
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0) || (em->frame > 40.7f && em->frame < 41.3f)) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x80), 0, 0, 1, 0);
        em->atari.throughOff();
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x30), 0, 0, 1, 0);
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            em31DmEnd(em, w);
        }
        break;
    }
}

// The player on the giant's back: climbs up (case 0/1), stabs the parasite (2/3), jumps off (4/5).
static void plem31_Climb(cPlayer* pl)
{
    cModel* p = pl->getPartsPtr(4);

    pl->dmg.set(0, 10);
    pl->subArc = PL_EM(pl)->subArc;
    switch (pl->xFE) {
    case 0:
        pl->pos.x = 0.0f;
        pl->pos.y = 0.0f;
        pl->pos.z = -7000.0f;
        PSMTXMultVec(PL_EM(pl)->mat, &pl->pos, &pl->pos);
        pl->rot.y = PL_EM(pl)->rot.y;
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x82), 0, 0, 0x201, 0);
        EstSet((int) pl, -1, 0, 0, 0x29, 0x34, 0, 0, (u32) pl, 0);
        pl->atari.throughOn();
        pl->xFE++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            pl->xFE++;
            break;
        }
        if (pl->frame > 15.7f && pl->frame < 16.3f) {
            SndCall(8, 0x39, &p->worldPos, PL_EM(pl)->id, 0, pl);
        }
        break;
    case 2:
        pl->pos.x = 0.0f;
        pl->pos.y = 0.0f;
        pl->pos.z = 0.0f;
        PSMTXMultVec(PL_EM(pl)->mat, &pl->pos, &pl->pos);
        pl->rot.y = PL_EM(pl)->rot.y;
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x83), 0, 0, 0x201, 0);
        em31CatchObj.p = ObjMgr.create(0xB);
        if (em31CatchObj.p) {
            em31CatchObj.p->modelInit(PL_ARC(0x86), PL_ARC(0x85));
            em31CatchObj.p->atari.flags &= 0xFCFF;
            em31CatchObj.p->pParts->pParent = pPLS->getPartsPtr(0xA);
            em31CatchObj.p->lightInfo.init2(1, 1, &((Vec) { 0.0f, 0.0f, 0.0f }), &((Vec) { 500.0f, 0.0f, 0.0f }), 1);
            em31CatchObj.p->wep.parent = pPL;
            em31CatchObj.p->be_flag &= ~2;
            EstSet((int) pl, -1, 0, 0, 0x29, 0x31, 0, 0, (u32) pl, 0);
        }
        pl->xFE++;
    case 3:
        if (MotionMoveF(pl, 0)) {
            pl->xFE++;
            break;
        }
        if (pl->frame > 24.7f && pl->frame < 25.3f) {
            pl->pWep->setTrans(0, 0);
            if (em31CatchObj.p) {
                em31CatchObj.p->be_flag |= 2;
            }
        }
        if (pl->frame > 13.7f && pl->frame < 14.3f) {
            SndCall(8, 0x3A, &p->worldPos, PL_EM(pl)->id, 0, pl);
        }
        if (pl->frame > 68.7f && pl->frame < 69.3f) {
            SndCall(8, 0x3B, &p->worldPos, PL_EM(pl)->id, 0, pl);
        }
        break;
    case 4:
        pl->pWep->setTrans(1, 0);
        if (em31CatchObj.p) {
            ObjMgr.destroy(em31CatchObj.p);
            em31CatchObj.p = 0;
        }
        pl->pos.x = 0.0f;
        pl->pos.y = 0.0f;
        pl->pos.z = 4500.0f;
        PSMTXMultVec(PL_EM(pl)->mat, &pl->pos, &pl->pos);
        pl->rot.y = PL_EM(pl)->rot.y;
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x84), 0, 0, 0x201, 0);
        EstSet((int) pl, -1, 0, 0, 0x29, 0x33, 0, 0, (u32) pl, 0);
        pl->xFE++;
    case 5:
        if (MotionMoveF(pl, 0)) {
            pl->atari.throughOff();
            EndPlDamage();
            pl->dmg.set(0, 30);
            break;
        }
        if (pl->frame > 10.7f && pl->frame < 11.3f) {
            SndCall(8, 0x3C, &p->worldPos, PL_EM(pl)->id, 0, pl);
        }
        if (pl->frame > 33.7f && pl->frame < 34.3f) {
            SndCall(8, 0x3D, &p->worldPos, PL_EM(pl)->id, 0, pl);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void em31SetActClimb(cEm31* em)
{
    EmRoutineSet(em, 2, 3, 0, 0);
    em->dmg.set(0, 30);
    pPL->dmg.set(0, 30);
}

static void em31_R0_Die(cEm31* em)
{
    EM31_WK(em)->flags |= 8;
    Em31_R3_move_tbl[em->xFD](em);
}

// Fades the model colours of the dying giant (and its tentacle) towards 0x30.
// Death fade of the model colours (macros: the four loops share em31_R1_Die_Normal's `info`).
#define EM31_DIE_FADE(p)                                                                           \
    for (info = p; info; info = info->pNext) {                                                      \
        info->color[0]--;                                                                           \
        if (info->color[0] < 0x30) {                                                                \
            info->color[0] = 0x30;                                                                  \
        }                                                                                           \
        info->color[1] = info->color[0];                                                            \
        info->color[2] = info->color[0];                                                            \
    }
#define EM31_DIE_FADE_END(p)                                                                       \
    for (info = p; info; info = info->pNext) {                                                      \
        info->color[0] = 0x30;                                                                      \
        info->color[1] = 0x30;                                                                      \
        info->color[2] = 0x30;                                                                      \
    }

// Releases the four tail objects (a macro: both expansions of em31_R1_Die_Normal share its `i`).
#define EM31_TAIL_CLEAR()                                                                          \
    for (i = 0; i < 4; i++) {                                                                       \
        if (w->pTail[i]) {                                                                          \
            w->pTail[i]->clearLostWait();                                                           \
            w->pTail[i] = 0;                                                                        \
        }                                                                                           \
    }

static void em31_R1_Die_Normal(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    u32 i;
    cModelInfo* info;

    if (w->pPillar) {
        ObjMgr.destroy(w->pPillar);
        w->pPillar = 0;
    }
    switch (em->xFE) {
    case 0:
        em->atari.clrFlag100();
        em->pos.x = -52850.0f;
        em->pos.y = 17500.0f;
        em->pos.z = 67000.0f;
        em->rot.y = 0.0f;
        em->hp = 0;
        MotionSetCore(em, MOTION(em), ARC(0x41), 0, 0, 1, 0);
        if (w->pTen) {
            w->pTen->xFC = 1;
            w->pTen->xFD = 0x22;
            w->pTen->xFE = 0;
            w->pTen->xFF = 0;
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        em->atari.clrFlag100();
        em->pos.x = -53000.0f;
        em->pos.y = 17500.0f;
        em->pos.z = 68000.0f;
        em->rot.y = 0.0f;
        MotionSetCore(em, MOTION(em), ARC(0x42), 0, 0, 1, 0);
        if (w->pTen) {
            w->pTen->xFC = 1;
            w->pTen->xFD = 0x22;
            w->pTen->xFE = 2;
            w->pTen->xFF = 0;
        }
        EM31_TAIL_CLEAR();
        if (w->flags & 0x800) {
            EstSet((int) em, -1, 0, 0, 0x29, 0x2A, 1, w->espKind, (u32) em, 0);
            EstSet((int) em, -1, 0, 0, 0x29, 0x2D, 1, w->espKind, (u32) em, 0);
            if (w->pTen) {
                EstSet((int) w->pTen, -1, 0, 0, 0x29, 0x32, 1, w->espKind, (u32) em, 0);
            }
        } else {
            EstSet((int) em, -1, 0, 0, 0x29, 0x2E, 1, w->espKind, (u32) em, 0);
            if (w->pTen) {
                EstSet((int) w->pTen, -1, 0, 0, 0x29, 0x2F, 1, w->espKind, (u32) em, 0);
            }
        }
        w->timer = 90;
        em->xFE++;
    case 3:
        if (w->flags & 0x800) {
            if (w->timer) {
                w->timer--;
            } else {
                EM31_DIE_FADE(em->pInfo);
                if (w->pTen) {
                    EM31_DIE_FADE(w->pTen->pInfo);
                }
            }
        }
        if (MotionMoveF(em, 0)) {
            EffectEspDelete(1, w->espKind, (u32) em, 0);
            EffectEspgenDelete(1, w->espKind, (int) em);
            EffectEfmDelete(1, w->espKind, (int) em);
            em->clearStatus(5);
            em->xFE++;
        }
        break;
    case 4:
        em->atari.clrFlag100();
        em->pos.x = -53000.0f;
        em->pos.y = 17500.0f;
        em->pos.z = 68000.0f;
        em->rot.y = 0.0f;
        em->hp = 0;
        MotionSetCore(em, MOTION(em), ARC(0x43), 0, 0, 1, 0);
        if (w->pTen) {
            w->pTen->xFC = 1;
            w->pTen->xFD = 0x22;
            w->pTen->xFE = 4;
            w->pTen->xFF = 0;
        }
        EM31_TAIL_CLEAR();
        EffectEspDelete(1, w->espKind, (u32) em, 0);
        EffectEspgenDelete(1, w->espKind, (int) em);
        EffectEfmDelete(1, w->espKind, (int) em);
        if (w->flags & 0x800) {
            EM31_DIE_FADE_END(em->pInfo);
            if (w->pTen) {
                EM31_DIE_FADE_END(w->pTen->pInfo);
            }
        }
        em->setStatus(8);
        EmSetDropItem(em);
        em->xFE++;
    case 5:
        MotionMoveF(em, 0);
        break;
    }
}

void em31RouteCk(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    Vec v;

    if (em->hp <= 0) {
        return;
    }
    v.x = 0.0f;
    v.y = 250.0f;
    v.z = -500.0f;
    PSMTXMultVec(em->mat, &v, &v);
    if (RouteCkPosToPos(&v, &pPL->pos, &w->routePos)) {
        w->flags |= 1;
    }
    w->routeAng = Muku(&em->pos, &w->routePos, em->rot.y, PI);
    w->routeAngAbs = fabsf(w->routeAng);
    w->targetPos = w->routePos;
    w->targetAng = w->routeAng;
    w->targetAngAbs = w->routeAngAbs;
    if (em->xFC == 0) {
        w->routeAng = 0.0f;
        w->routeAngAbs = 0.0f;
        em->plDist2 = 100000000.0f;
    }
}

// The single-link cloth of the first build: never called, the original link dropped the body and kept
// the constant pool (STRIP_UNUSED).
static void Em31ClothSet(cEm31* em, PlCloth* c)
{
    c->num = 1;
    c->pParts = em31ClothP;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = em31ClothUp;
    c->pDown = em31ClothDp;
    c->pWindS = 0;
    c->pWindR = 0;
    c->pAt = 0;
    c->x20 = 0;
    c->pMax = em31ClothMax;
    c->pRate = 0;
    c->nAt = 0;
    c->x3C = 30.0f;
    c->x40 = 0.5f;
    c->pModel = em;
    c->x44 = 0;
    c->x48 = 0.0f;
    c->x4C = 1.0f;
    c->x50 = 0.0f;
    c->flags = 0x40;
    c->x54 = 0;
    PenClothSet(em, (PenCloth*) c, 1000.0f);
}

void Em31ClothSet2(cEm31* em, PlCloth* c)
{
    c->num = 15;
    c->pParts = em31ClothP2;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = em31ClothUp2;
    c->pDown = em31ClothDp2;
    c->pWindS = 0;
    c->pWindR = 0;
    c->pAt = 0;
    c->x20 = 0;
    c->pMax = em31ClothMax2;
    c->pRate = 0;
    c->nAt = 0;
    c->x3C = 30.0f;
    c->x40 = 0.5f;
    c->pModel = em;
    c->x44 = 0;
    c->x48 = 0.0f;
    c->x4C = 1.0f;
    c->x50 = 0.0f;
    c->flags = 0x40;
    c->x54 = 0;
    PenClothSet(em, (PenCloth*) c, 100.0f);
}

void Em31ClothMove2(cEm31* em, PlCloth* c)
{
    Em31Work* w = EM31_WK(em);
    u32 i;

    if (w->flags & 0x200) {
        return;
    }
    PenClothMove3(em, (PenCloth*) c);
    for (i = 0x38; i <= 0x49; i++) {
        cParts* p = (cParts*) em->getPartsPtr(i);

        PSMTXConcat(p->pParent->mat, p->worldMat, p->mat);
        p->worldPos.x = p->mat[0][3];
        p->worldPos.y = p->mat[1][3];
        p->worldPos.z = p->mat[2][3];
    }
}

void Em31ClothSet3(cEm31* em, PlCloth* c)
{
    c->num = 18;
    c->pParts = em31ClothP3;
    c->pLeft = 0;
    c->pRight = 0;
    c->pUpLeft = 0;
    c->x14 = 0;
    c->pUp = em31ClothUp3;
    c->pDown = em31ClothDp3;
    c->pWindS = 0;
    c->pWindR = 0;
    c->pAt = 0;
    c->x20 = 0;
    c->pMax = em31ClothMax3;
    c->pRate = 0;
    c->nAt = 0;
    c->x3C = 30.0f;
    c->x40 = 0.5f;
    c->pModel = em;
    c->x44 = 0;
    c->x48 = 0.0f;
    c->x4C = 1.0f;
    c->x50 = 0.0f;
    c->flags = 0x40;
    c->x54 = 0;
    PenClothSet(em, (PenCloth*) c, 100.0f);
}

void Em31ClothMove3(cEm31* em, PlCloth* c)
{
    if (!(EM31_WK(em)->flags & 0x200)) {
        PenClothMove3(em, (PenCloth*) c);
    }
}

// The player under the giant's foot: the head comes off outside Japan, an effect otherwise.
static inline void em31PlCrush(cEm31* em)
{
    if ((s16) pG->pl_life <= 0) {
        em31PlHeadLost();
    } else {
        EstSet((int) pPL, -1, 0, 0, 0x29, 0x1B, 0, 0, (u32) pPL, 0);
        EstSet((int) em, -1, 0, 0, 0x29, 0x1C, 0, 0, (u32) em, 0);
    }
}

// Knock the player away from the giant.
static inline void em31PlBlow(cEm31* em)
{
    pPLS->rot.y = GetXZAngle(&pPL->pos, &em->pos);
    if ((s16) pGS->pl_life <= 0) {
        EstSet((int) pPL, -1, 0, 0, 0x29, 0x3A, 0, 0, (u32) pPL, 0);
    } else {
        EstSet((int) pPL, -1, 0, 0, 0x29, 0x3B, 0, 0, (u32) pPL, 0);
    }
    PlSetDamage(8, 0, 0);
}

int em31AtkCk(cEm31* em, Vec* pos, Vec* oldPos, int no)
{
    Em31Work* w = EM31_WK(em);
    EmAtkInfo* info;
    int hit;

    em31PillarAtkCk(em, pos);
    if (w->atkHit) {
        return 0;
    }
    info = &em31_atk_tbl[no];
    if ((s16) pG->pl_life > 1) {
        info->x0A |= 4;
    } else {
        info->x0A &= ~4;
    }
    hit = EmAtkHitCk(info, pos, oldPos, 1);
    if (hit != 0) {
        if (hit & 1) {
            w->atkHit = 1;
            w->flags |= 0x400;
            switch ((u32) no) {
            case 0:
                SndCall(8, 0x2A, &pPL->pos, em->id, 0, pPL);
                FSet(pPL->pos.x, pos->x);
                pPL->pos.z = pos->z;
                SetPlDamage((int) em, plem31_dm_Stamp);
                break;
            case 1:
                SndCall(8, 0x2A, &pPL->pos, em->id, 0, pPL);
                FSet(pPL->pos.x, pos->x);
                pPL->pos.z = pos->z;
                SetPlDamage((int) em, plem31_dm_Stamp);
                pPL->xFF = 1;
                break;
            case 4:
                SndCall(8, 0x2A, &pPL->pos, em->id, 0, pPL);
                SetPlDamage((int) em, plem31_dm_Stamp);
                pPL->xFF = 2;
                break;
            case 5:
                em31PlCrush(em);
                SndCall(8, 0xF, &pPL->pos, em->id, 0, pPL);
                break;
            case 6:
                em31PlCrush(em);
                SndCall(8, 0xF, &pPL->pos, em->id, 0, pPL);
                em31PlBlow(em);
                break;
            case 3:
                EmPlBloodSet2(em, pos, 1, 0x29, 0x3D);
                SndCall(8, 0xF, &pPL->pos, em->id, 0, pPL);
                em31PlBlow(em);
                break;
            case 7:
                SndCall(8, 0xF, &pPL->pos, em->id, 0, pPL);
                break;
            }
            QuakeExec(0, 0, 5, 22.0f, 2);
            VibSetData(VIB_TBL, 0xB, 1);
            return 1;
        }
    }
    return 0;
}

static void plem31_dm_Stamp(cPlayer* pl)
{
    pl->subArc = PL_EM(pl)->subArc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x6B), 0, 3, 1, 0);
        PlSetFace(1);
        pl->atari.clrFlag200();
        if ((s16) pGS->pl_life > 0) {
            PlSetDamageSe(0);
        } else {
            PlSetDamageSe(0xD);
        }
        switch (pl->xFF) {
        case 0:
        default:
            EstSet((int) pl, -1, 0, 0, 0x29, 0x1E, 0, 0, (u32) pl, 0);
            break;
        case 1:
            EstSet((int) pl, -1, 0, 0, 0x29, 0x1F, 0, 0, (u32) pl, 0);
            break;
        case 2:
            break;
        }
        pl->xFE++;
    case 1:
        em31StampCamMove(PL_EM(pl));
        if (MotionMoveF(pl, 0) || (pl->frame > 49.7f && pl->frame < 50.3f)) {
            if ((s16) pG->pl_life > 0) {
                pl->atari.throughOff();
                EmRoutineSet(pPLS, 1, 0, 0xA, 0);
            }
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

void em31StampCamMove(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    GlobalWork* g = pG;
    Camera* cam = &w->cam;
    Vec a;
    cModel* p;

    w->cam.param.fovy = g->Cam.param.fovy;
    a.x = 0.0f;
    a.y = 3000.0f;
    a.z = -3000.0f;
    PSMTXMultVec(pPLS->mat, &a, &a);
    PosToPos(&g->Cam.param.pos, &a, &w->cam.param.pos, 0.1f);
    p = pPL->getPartsPtr(0);
    PosToPos(&g->Cam.param.at, &p->worldPos, &w->cam.param.at, 0.3f);
    w->cam.up.x = 0.0f;
    w->cam.up.y = 1.0f;
    w->cam.up.z = 0.0f;
    w->cam.dist = SQRTF((w->cam.param.pos.x - w->cam.param.at.x) * (w->cam.param.pos.x - w->cam.param.at.x) +
                        (w->cam.param.pos.y - w->cam.param.at.y) * (w->cam.param.pos.y - w->cam.param.at.y) +
                        (w->cam.param.pos.z - w->cam.param.at.z) * (w->cam.param.pos.z - w->cam.param.at.z));
    CameraSetOrientationUp(cam);
    CamCtrl.x250 = (s32) cam;
}

void em31SearchBody(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    u32 i;

    if (w->pBody) {
        return;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm31* p = em31EmWork(i);

        if ((p->be_flag & 0x201) == 1 && p->id == 0x31 && p != em && p->type == 0) {
            w->pBody = p;
            EM31_WK(p)->pTen = em;
            return;
        }
    }
}

void em31TenMatCalc(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    cModel* p;

    if (em->type != 1) {
        return;
    }
    if (w->pBody == 0) {
        em->partsWorldCalc();
        return;
    }
    p = w->pBody->getPartsPtr(1);
    em->pos.x = 0.0f;
    em->pos.y = 0.0f;
    em->pos.z = 0.0f;
    em->rot.x = 0.0f;
    em->rot.y = 0.0f;
    em->rot.z = 0.0f;
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    PSMTXConcat(p->mat, em->mat, em->mat);
    em->partsWorldCalc();
    em31TentacleConnect(em);
}

void cEm31::setDamageCrane(int flip)
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x20, 0, flip);
}

void cEm31::setCranePos(int no)
{
    Em31Work* w = EM31_WK(this);
    Vec p;
    f32 ry;

    if (type != 0) {
        return;
    }
    switch (no) {
    case 0:
    default:
        p.x = -52979.0f;
        p.y = 17361.0f;
        p.z = 59571.0f;
        ry = PI;
        break;
    case 1:
        p.x = -35250.0f;
        p.y = 15811.0f;
        p.z = 81442.0f;
        ry = 0.0f;
        break;
    }
    rot.y = ry;
    setPos(&p);
    oldPos = p;
    w->atkWait = 60;
    if (pGS->x4F88 <= 3) {
        w->atkWait = 90;
    }
    if (pG->x4F88 <= 1) {
        w->atkWait = 120;
    }
    EmRoutineSet(this, 1, 1, 0, 0);
    if (w->pTen) {
        w->pTen->setWait();
    }
}

int cEm31::ckAtkEnable()
{
    Em31Work* w = EM31_WK(this);

    if (type != 1) {
        return 0;
    }
    if (w->atkEnable == 0) {
        return 0;
    }
    return 1;
}

void cEm31::setAtk(int no)
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x17, 0, no);
}

void cEm31::setDashAtk()
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x18, 0, 0);
}

void cEm31::setWait()
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x12, 0, 0);
}

void cEm31::setClimb()
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x21, 0, 0);
}

void cEm31::setPillarThrow()
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x1D, 0, 0);
}

void cEm31::setBerserkStart()
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x13, 0, 0);
}

void cEm31::setBerserkEnd()
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x14, 0, 0);
}

void cEm31::setJump()
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x15, 0, 0);
}

void cEm31::setStamp(u8 no)
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x16, 0, no);
}

void cEm31::setCatch()
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x19, 0, 0);
}

void cEm31::setCatchHit()
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x1A, 0, 0);
}

void cEm31::setStepCatch()
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x1B, 0, 0);
}

void cEm31::setStepCatchHit()
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x1C, 0, 0);
}

void cEm31::setDmNormal()
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x1E, 0, 0);
}

void cEm31::setDown(int flip)
{
    if (type != 1) {
        return;
    }
    EmRoutineSet(this, 1, 0x1F, 0, flip);
}

void cEm31::setDownDamage()
{
    if (type != 1) {
        return;
    }
    xFC = 1;
    xFD = 0x1F;
    xFE = 6;
}

int cEm31::ckWeakDamage()
{
    if (type != 1) {
        return 0;
    }
    if (EM31_WK(this)->flags & 0x100) {
        return 1;
    }
    return 0;
}

int cEm31::ckDownEnable()
{
    if (type != 0) {
        return 0;
    }
    if (EM31_WK(this)->flags & 0x4000) {
        return 1;
    }
    return 0;
}

void cEm31::setDownBody()
{
    if (type != 0) {
        return;
    }
    EmRoutineSet(this, 2, 1, 0, 0);
}

void cEm31::setDownCancel()
{
    Em31Work* w = EM31_WK(this);

    if (type != 0) {
        return;
    }
    xFD = 1;
    xFC = 2;
    xFE = 2;
    if (w->pTen) {
        w->pTen->setDown(xFF);
        w->pTen->xFE = 2;
    }
}

int cEm31::getTotalDamage()
{
    return EM31_WK(this)->totalDamage;
}

void em31TailAtkCk(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    cObj16** t;
    cObj16* o;
    u8 hit;
    u32 i;

    if (em->type != 0) {
        return;
    }
    if (em->hp <= 0) {
        return;
    }
    t = w->pTail;
    for (i = 0; i < 4; i++) {
        if (w->pTail[i]) {
            if (w->tailSeTimer) {
                w->tailSeTimer--;
            } else {
                w->tailSeTimer = 0x1D;
                SndCall(8, 9, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
            }
            break;
        }
    }
    em31SmallTentacleMove(em);
    hit = w->atkHit;
    w->atkHit = 0;
    // Indexed over `t`: loop.c's giv (r31, highest priority) is initialised from t and t dies at the copy, so
    // both share r31 and em drops to r30; the limit is `t + 12` (not em + 0xA70 as in the first loop).
    for (i = 0; i < 4; i++) {
        o = t[i];
        if (o) {
            em31AtkCk(em, &o->getPartsPtr(7)->worldPos, &em->pos, 7);
        }
    }
    w->atkHit = hit;
}

void em31EyelidInit(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    cModel* p;

    if (em->type != 0) {
        return;
    }
    w->eye[0].state = 1;
    w->eye[0].closed = 0;
    w->eye[0].dir = 3.1241393f;
    w->eye[0].parts = 0x1B;
    w->eye[0].timer = Rnd() % 90 + 90;
    p = em->getPartsPtr(0x1B);
    w->eye[0].parts2 = 0x1A;
    w->eye[0].posY = p->pos.y;
    w->eye[0].posY2 = p->pos.y - 50.0f;
    w->eye[0].hp = 2;
    w->eye[1].state = 1;
    w->eye[1].closed = 0;
    w->eye[1].dir = -3.1241393f;
    w->eye[1].parts = 0x1D;
    w->eye[1].timer = Rnd() % 90 + 90;
    p = em->getPartsPtr(0x1C);
    w->eye[1].parts2 = 0x1C;
    w->eye[1].posY = p->pos.y;
    w->eye[1].posY2 = p->pos.y - 50.0f;
    w->eye[1].hp = 2;
    w->eye[2].state = 1;
    w->eye[2].closed = 0;
    w->eye[2].dir = 3.1241393f;
    w->eye[2].parts = 0x1F;
    w->eye[2].timer = Rnd() % 90 + 90;
    p = em->getPartsPtr(0x1E);
    w->eye[2].parts2 = 0x1E;
    w->eye[2].posY = p->pos.y;
    w->eye[2].posY2 = p->pos.y - 50.0f;
    w->eye[2].hp = 1;
    w->eye[3].state = 1;
    w->eye[3].closed = 0;
    w->eye[3].dir = -3.1241393f;
    w->eye[3].parts = 0x21;
    w->eye[3].timer = Rnd() % 90 + 90;
    p = em->getPartsPtr(0x20);
    w->eye[3].parts2 = 0x20;
    w->eye[3].posY = p->pos.y;
    w->eye[3].posY2 = p->pos.y - 50.0f;
    w->eye[3].hp = 1;
}

void em31EyelidMove(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    Vec pp;
    Vec d;
    cParts* p;
    u32 i;
    f32 ang;
    f32 len;

    if (em->type != 0) {
        return;
    }
    p = (cParts*) pPL->getPartsPtr(4);
    pp = p->worldPos;
    pp.y -= 150.0f;
    for (i = 0; i < 4; i++) {
        Em31Eyelid* e = &w->eye[i];

        switch (e->state) {
        case 0:
            SndCall(8, 0x2D, &em->getPartsPtr(e->parts)->worldPos, em->id, 0, em);
            e->timer = Rnd() % 90 + 90;
            if (pGS->x4F88 > 6) {
                e->timer = Rnd() % 60 + 60;
            }
            if (pGS->x4F88 <= 3) {
                e->timer = Rnd() % 90 + 150;
            }
            if (pGS->x4F88 <= 1) {
                e->timer = Rnd() % 90 + 250;
            }
            e->state++;
        case 1: {
            f32 s;

            p = (cParts*) em->getPartsPtr(e->parts);
            p->rot.z *= 0.7f;
            p = (cParts*) em->getPartsPtr(e->parts2);
            p->pos.y = p->pos.y * 0.9f + e->posY * 0.1f;
            s = p->scale.x * 0.7f + 0.3f;
            p->scale.x = s;
            p->scale.z = s;
            p->scale.y = s;
            e->closed = 0;
            if ((w->flags & 0xC8) || w->hitTimer) {
                e->timer = 0;
            }
            if (e->timer == 0) {
                e->timer = Rnd() % 90 + 90;
                if (pGS->x4F88 <= 3) {
                    e->timer = Rnd() % 60 + 60;
                }
                e->state++;
            } else {
                e->timer--;
            }
            break;
        }
        case 2:
            SndCall(8, 0x2D, &em->getPartsPtr(e->parts)->worldPos, em->id, 0, em);
            e->state++;
        case 3: {
            f32 s;

            p = (cParts*) em->getPartsPtr(e->parts);
            p->rot.z = p->rot.z * 0.7f + e->dir * 0.3f;
            p = (cParts*) em->getPartsPtr(e->parts2);
            p->pos.y = p->pos.y * 0.5f + e->posY2 * 0.5f;
            s = p->scale.x * 0.7f + 0.3f * 0.8f;
            p->scale.x = s;
            p->scale.z = s;
            p->scale.y = s;
            e->closed = 1;
            if ((w->flags & 0xC8) || w->hitTimer) {
                if (e->timer) {
                    e->timer = Rnd() % 90 + 90;
                }
            }
            if (e->timer) {
                e->timer--;
            } else {
                e->state = 0;
            }
            break;
        }
        case 4:
            e->state++;
        case 5:
            p = (cParts*) em->getPartsPtr(e->parts);
            p->rot.z *= 0.7f;
            p = (cParts*) em->getPartsPtr(e->parts2);
            p->scale.x *= 0.7f;
            if (p->scale.x < 0.1f) {
                p->scale.x = 0.1f;
            }
            p->scale.y = p->scale.x;
            p->scale.z = p->scale.x;
            e->closed = 0;
            break;
        }
        p = (cParts*) em->getPartsPtr(e->parts2);
        d.x = 0.0f;
        d.y = 1.0f;
        d.z = 0.0f;
        PSMTXMultVecSR(p->pParent->mat, &d, &d);
        ang = atan2f(d.x, d.z);
        p->addRot.y = Muku(&p->worldPos, &pp, ang, 0.7853982f);
        PSVECSubtract(&pp, &p->worldPos, &d);
        len = SQRTF(d.x * d.x + d.z * d.z);
        ang = atan2f(d.y, len);
        ang = -ang;
        ang = Muku2(0.0f, ang, 0.7853982f);
        if (e->dir > 0.0f) {
            p->addRot.z = ang;
        } else {
            p->addRot.z = -ang;
        }
        p->addRot.x = 0.0f;
        p->motParts.flags |= 0x40000000;
    }
}

int em31EyelidDmcK(cEm31* em, int dmg)
{
    Em31Work* w = EM31_WK(em);
    EmHitInfo* part;
    int i;

    if (em->type != 0) {
        return 0;
    }
    part = em->dmPart;
    for (i = 0; i < 4; i++) {
        Em31Eyelid* e = &w->eye[i];

        if (e->parts2 == part->partsNo - 1) {
            if (e->closed) {
                return 0;
            }
            if (e->hp <= 0) {
                return 0;
            }
            if (dmg) {
                e->hp--;
                if (e->hp <= 0) {
                    e->hp = 0;
                    part->flags &= ~1;
                    e->state = 4;
                } else {
                    e->state = 2;
                    e->timer = 900;
                }
            }
            return 1;
        }
    }
    return 0;
}

void em31TentacleConnect(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    cModel* p;
    cModel* q;

    if (em->type != 1) {
        return;
    }
    if (w->pBody == 0) {
        return;
    }
    p = em->getPartsPtr(1);
    q = w->pBody->getPartsPtr(0x52);
    PSMTXCopy(p->mat, q->mat);
    q->worldPos = p->worldPos;
}

void cEm31::setHitCrane(Vec* target)
{
    int back;

    if (type != 0) {
        return;
    }
    back = 0;
    if (target) {
        if (Muku(&pos, target, rot.y, PI) < 0.0f) {
            back = 1;
        }
    }
    EmRoutineSet(this, 2, 2, 0, back);
}

void em31SmallTentacleMove(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    u16 step;
    u32 i;

    if (w->flags & 0x20) {
        if (em->plDist2 < 49000000.0f) {
            return;
        }
        w->flags &= ~0x20;
        step = (*(u16*) ARC(0x78) & 0x3FFF) / 4;
        for (i = 0; i < 4; i++) {
            if (w->pTail[i]) {
                MotSetObj16(w->pTail[i], ARC(0x78), 4, step * i);
            }
        }
    } else {
        if (em->plDist2 > 25000000.0f) {
            return;
        }
        w->flags |= 0x20;
        step = (*(u16*) ARC(0x79) & 0x3FFF) / 4;
        for (i = 0; i < 4; i++) {
            if (w->pTail[i]) {
                MotSetObj16(w->pTail[i], ARC(0x79), 4, step * i);
            }
        }
    }
}

int cEm31::ckBerserk()
{
    if (type != 0) {
        return 0;
    }
    if (EM31_WK(this)->flags & 0x40) {
        return 1;
    }
    return 0;
}

int em31SetDmVal(cEm31* em)
{
    EmHitInfo* part = em->dmPart;
    int near;
    int dmg;

    near = 0;
    if (part->rad < 36000000.0f) {
        near = 1;
    }
    {
        u32 no = em->dmWep;

        dmg = 100;
        if (no <= 0x2D) {
            dmg = GetWepDmVal(em, no, near);
        }
    }
    switch (em->type) {
    case 0:
    default:
        dmg /= 5;
        break;
    case 1:
        if (part->partsNo != 0xC) {
            dmg /= 5;
        }
        break;
    }
    return dmg;
}

void em31WeakMode(cEm31* em, int on)
{
    Em31Work* w = EM31_WK(em);

    if (on) {
        em->hitInfo.flags |= 1;
        w->hit[0].flags &= ~1;
    } else {
        em->hitInfo.flags &= ~1;
        w->hit[0].flags |= 1;
    }
}

void cEm31::setDie()
{
    if (type != 0) {
        return;
    }
    EM31_WK(this)->flags |= 0x800;
    EmRoutineSet(this, 3, 0, 0, 0);
}

void cEm31::setDieNormal()
{
    if (type != 0) {
        return;
    }
    EmRoutineSet(this, 3, 0, 2, 0);
}

void cEm31::setDieCancel()
{
    if (type != 0) {
        return;
    }
    EmRoutineSet(this, 3, 0, 4, 0);
}

int em31PillarCk(cEm31* em)
{
    Mtx inv;
    Vec lp;
    u32 i;

    PSMTXInverse(em->mat, inv);
    for (i = 0; i < ObjMgr.nArray; i++) {
        cObj* o = em31ObjWork(i);

        if ((o->be_flag & 0x201) == 1 && o->id == 0x1F && ((cObjPillar*) o)->ckSet()) {
            PSMTXMultVec(inv, &o->pos, &lp);
            if (lp.x > -3500.0f && lp.x < 3500.0f && lp.y > -500.0f && lp.y < 500.0f && lp.z > 0.0f &&
                lp.z < 4000.0f) {
                // Plain byte stores: the QImode `4` (lifetime 1) is hoisted by loop.c's second pass, after
                // the 4000.0 pool high, which is what gives it r23 and the high r22.
                if (lp.x < 0.0f) {
                    em->xFC = 1;
                    em->xFD = 0xF;
                    em->xFE = 0;
                    em->xFF = 1;
                } else {
                    em->xFC = 1;
                    em->xFD = 0xF;
                    em->xFE = 0;
                    em->xFF = 4;
                }
                return 1;
            }
        }
    }
    return 0;
}

int em31PillarCk2(cEm31* em)
{
    Mtx inv;
    Vec lp;
    u32 i;

    PSMTXInverse(em->mat, inv);
    for (i = 0; i < ObjMgr.nArray; i++) {
        cObj* o = em31ObjWork(i);

        if ((o->be_flag & 0x201) == 1 && o->id == 0x1F && ((cObjPillar*) o)->ckSet()) {
            PSMTXMultVec(inv, &o->pos, &lp);
            if (lp.x > 0.0f && lp.x < 2500.0f && lp.y > -500.0f && lp.y < 500.0f && lp.z > -1500.0f &&
                lp.z < 3500.0f) {
                return 1;
            }
            if (lp.x > -2500.0f && lp.x < 0.0f && lp.y > -500.0f && lp.y < 500.0f && lp.z > -1500.0f &&
                lp.z < 3500.0f) {
                return 2;
            }
        }
    }
    return 0;
}

void em31PillarAtkCk(cEm31* em, Vec* pos)
{
    Em31Work* w = EM31_WK(em);
    u32 i;

    for (i = 0; i < ObjMgr.nArray; i++) {
        cObj* o = em31ObjWork(i);

        if ((o->be_flag & 0x201) == 1 && o->id == 0x1F && ((cObjPillar*) o)->ckSet()) {
            if ((pos->x - o->pos.x) * (pos->x - o->pos.x) + (pos->z - o->pos.z) * (pos->z - o->pos.z) <
                1000000.0f) {
                if (w->pBody) {
                    ((cObjPillar*) o)->setBreak(&w->pBody->pos, ARC(0x6D), (int) ARC(0x6E));
                    SndCall(8, 0x2B, pos, em->id, 0, 0);
                }
            }
        }
    }
}

int em31JumpCk(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    Mtx m;
    Vec lp;
    f32 dy;
    u32 i;

    if (!(w->flags & 1)) {
        return 0;
    }
    if (em->plDist2 < 225000000.0f) {
        return 0;
    }
    dy = fabsf(em->pos.y - pPL->pos.y);
    if (dy > 100.0f) {
        return 0;
    }
    if (pPL->pos.x > -52000.0f && pPL->pos.x < -36000.0f && pPL->pos.z > 55000.0f && pPL->pos.z < 61000.0f) {
        return 0;
    }
    if (pPL->pos.x > -52000.0f && pPL->pos.x < -36000.0f && pPL->pos.z > 79000.0f) {
        return 0;
    }
    lp.x = -43811.0f;
    lp.y = 15861.0f;
    lp.z = 47353.0f;
    if ((pPL->pos.x - lp.x) * (pPL->pos.x - lp.x) + (pPL->pos.z - lp.z) * (pPL->pos.z - lp.z) < 16000000.0f) {
        return 0;
    }
    PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &pPL->pos));
    TransMatrix(m, &em->pos);
    PSMTXInverse(m, m);
    for (i = 0; i < ObjMgr.nArray; i++) {
        cObj* o = em31ObjWork(i);

        if ((o->be_flag & 0x201) == 1 && o->id == 0x1F && ((cObjPillar*) o)->ckSet()) {
            PSMTXMultVec(m, &o->pos, &lp);
            if (lp.x > -3500.0f && lp.x < 3500.0f && lp.z > 0.0f && lp.z * lp.z < em->plDist2) {
                return 0;
            }
        }
    }
    w->bridgePos = pPL->pos;
    w->flags &= ~0x40;
    EmRoutineSet(em, 1, 8, 0, 0);
    return 1;
}

// Blood / hit effect by weapon: the heavy hits (5, 6, 9, A, D, F, 12, 13, 15, 18, 1C, 28, 29, 2C, 2D)
// give `big`, the blades (B, C, 1B, 1D, 27) `blade`, the hand guns (7, 8, 21) `near ? big : small`,
// the special ones (10, 1A) `fire`, the rest `small`; 14, 16, 17, 19, 1F, 20, 2A give nothing.
#define EM31_BLOOD_SWITCH(small, blade, big, fire, nearBig, nearSmall)                            \
    switch (em->dmWep) {                                                                            \
    case 0x0:                                                                                       \
    case 0x1:                                                                                       \
    case 0x2:                                                                                       \
    case 0x3:                                                                                       \
    case 0x4:                                                                                       \
    case 0xE:                                                                                       \
    case 0x11:                                                                                      \
    case 0x26:                                                                                      \
    case 0x2B:                                                                                      \
    default:                                                                                        \
        small;                                                                                      \
        break;                                                                                      \
    case 0xB:                                                                                       \
    case 0xC:                                                                                       \
    case 0x1B:                                                                                      \
    case 0x1D:                                                                                      \
    case 0x27:                                                                                      \
        blade;                                                                                      \
        break;                                                                                      \
    case 0x7:                                                                                       \
    case 0x8:                                                                                       \
    case 0x21:                                                                                      \
        if (near) {                                                                                 \
            nearBig;                                                                                \
        } else {                                                                                    \
            nearSmall;                                                                              \
        }                                                                                           \
        break;                                                                                      \
    case 0x5:                                                                                       \
    case 0x6:                                                                                       \
    case 0x9:                                                                                       \
    case 0xA:                                                                                       \
    case 0xD:                                                                                       \
    case 0xF:                                                                                       \
    case 0x12:                                                                                      \
    case 0x13:                                                                                      \
    case 0x15:                                                                                      \
    case 0x18:                                                                                      \
    case 0x1C:                                                                                      \
    case 0x28:                                                                                      \
    case 0x29:                                                                                      \
    case 0x2C:                                                                                      \
    case 0x2D:                                                                                      \
        big;                                                                                        \
        break;                                                                                      \
    case 0x10:                                                                                      \
    case 0x1A:                                                                                      \
        fire;                                                                                       \
        break;                                                                                      \
    case 0x14:                                                                                      \
    case 0x16:                                                                                      \
    case 0x17:                                                                                      \
    case 0x19:                                                                                      \
    case 0x1F:                                                                                      \
    case 0x20:                                                                                      \
    case 0x2A:                                                                                      \
        break;                                                                                      \
    }

void em31BloodSet(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    EmHitInfo* part = em->dmPart;
    Vec pos;
    Vec dir;
    Vec dir2;
    int near;
    int kind;

    near = 0;
    if (part->rad < 36000000.0f) {
        near = 1;
    }
    kind = 0;
    if (part == &w->hit[4] || part == &w->hit[5] || part == &w->hit[6] || part == &w->hit[7] || part == &w->hit[8]) {
        kind = 1;
    }
    if (em31EyelidDmcK(em, 0)) {
        kind = 2;
        if (part == &w->hit[0]) {
            EstSet((int) em, -1, 0, 0, 0x29, 0x12, 0, 0, (u32) em, 0);
        }
        if (part == &w->hit[1]) {
            EstSet((int) em, -1, 0, 0, 0x29, 0x13, 0, 0, (u32) em, 0);
        }
        if (part == &w->hit[2]) {
            EstSet((int) em, -1, 0, 0, 0x29, 0x14, 0, 0, (u32) em, 0);
        }
        if (part == &w->hit[3]) {
            EstSet((int) em, -1, 0, 0, 0x29, 0x15, 0, 0, (u32) em, 0);
        }
    }
    switch ((u32) kind) {
    case 0:
    default:
        SndCall(8, 3, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
        EM31_BLOOD_SWITCH(EmDmBloodSet2(em, 0x29, 4, 0, 0, 0), EmDmBloodSet2(em, 0x29, 6, 0, 0, 0),
                          EmDmBloodSet2(em, 0x29, 5, 0, 0, 0), EmDmBloodSet2(em, 0x29, 7, 0, 0, 0),
                          EmDmBloodSet2(em, 0x29, 5, 0, 0, 0), EmDmBloodSet2(em, 0x29, 4, 0, 0, 0));
        break;
    case 1:
        SndCall(8, 6, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
        EM31_BLOOD_SWITCH(EmDmBloodSet2(em, 0x29, 0, 0, 0, 0),
                          EmDmBloodSet2(em, 0x29, 2, 0, 0, 0);
                          if ((Rnd() & 3) == 0) {
                              if (EmGetDmPos(em, &pos, &dir)) {
                                  EstSet(0, -1, &pos, 0, 0x29, 0xD, 0, 0, 0, 0);
                              }
                          },
                          EmDmBloodSet2(em, 0x29, 1, 0, 0, 0), EmDmBloodSet2(em, 0x29, 3, 0, 0, 0),
                          EmDmBloodSet2(em, 0x29, 1, 0, 0, 0), EmDmBloodSet2(em, 0x29, 0, 0, 0, 0));
        break;
    case 2:
        SndCall(8, 6, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
        EM31_BLOOD_SWITCH(EmDmBloodSet2(em, 0x29, 8, 0, 0, 0),
                          EmDmBloodSet2(em, 0x29, 0xA, 0, 0, 0);
                          if ((Rnd() & 3) == 0) {
                              if (EmGetDmPos(em, &pos, &dir2)) {
                                  EstSet(0, -1, &pos, 0, 0x29, 0xB, 0, 0, 0, 0);
                              }
                          },
                          EmDmBloodSet2(em, 0x29, 9, 0, 0, 0), EmDmBloodSet2(em, 0x29, 0xC, 0, 0, 0),
                          EmDmBloodSet2(em, 0x29, 9, 0, 0, 0), EmDmBloodSet2(em, 0x29, 8, 0, 0, 0));
        break;
    }
}

void em31TBloodSet(cEm31* em)
{
    Vec pos;
    Vec dir;
    int near;
    int kind;

    near = 0;
    if (em->dmPart->rad < 36000000.0f) {
        near = 1;
    }
    kind = 0;
    if (em->dmPart == &em->hitInfo) {
        kind = 1;
    }
    switch (kind) {
    case 0:
    default:
        SndCall(8, 3, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
        EM31_BLOOD_SWITCH(EmDmBloodSet2(em, 0x29, 4, 0, 0, 0), EmDmBloodSet2(em, 0x29, 6, 0, 0, 0),
                          EmDmBloodSet2(em, 0x29, 5, 0, 0, 0), EmDmBloodSet2(em, 0x29, 7, 0, 0, 0),
                          EmDmBloodSet2(em, 0x29, 5, 0, 0, 0), EmDmBloodSet2(em, 0x29, 4, 0, 0, 0));
        break;
    case 1:
        SndCall(8, 6, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
        EM31_BLOOD_SWITCH(EmDmBloodSet2(em, 0x29, 8, 0, 0, 0),
                          EmDmBloodSet2(em, 0x29, 0xA, 0, 0, 0);
                          if ((Rnd() & 3) == 0) {
                              if (EmGetDmPos(em, &pos, &dir)) {
                                  EstSet(0, -1, &pos, 0, 0x29, 0xB, 0, 0, 0, 0);
                              }
                          },
                          EmDmBloodSet2(em, 0x29, 9, 0, 0, 0), EmDmBloodSet2(em, 0x29, 0xC, 0, 0, 0),
                          EmDmBloodSet2(em, 0x29, 9, 0, 0, 0), EmDmBloodSet2(em, 0x29, 8, 0, 0, 0));
        break;
    }
}

void em31SetTail(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    cModel* p;
    Vec pos;
    Vec rot;
    u16 step;

    p = em->getPartsPtr(0x4A);
    p->rot.x = -1.2217305f;
    p->rot.y = 0.34906584f;
    p->rot.z = 0.0f;
    p = em->getPartsPtr(0x4B);
    p->rot.x = -1.2217305f;
    p->rot.y = -0.34906584f;
    p->rot.z = 0.0f;
    p = em->getPartsPtr(0x4C);
    p->rot.x = -2.0943952f;
    p->rot.y = -0.34906584f;
    p->rot.z = 0.0f;
    p = em->getPartsPtr(0x4D);
    p->rot.x = -2.0943952f;
    p->rot.y = 0.34906584f;
    p->rot.z = 0.0f;
    step = (*(u16*) ARC(0x78) & 0x3FFF) / 4;
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    if (w->pTail[0] == 0) {
        w->pTail[0] = (cObj16*) SetObj16(ARC(0x76), ARC(0x77), em, em, 0x4A, 0x10, &pos, &rot);
        if (w->pTail[0]) {
            MotSetObj16(w->pTail[0], ARC(0x78), 4, 0);
        }
    }
    if (w->pTail[1] == 0) {
        w->pTail[1] = (cObj16*) SetObj16(ARC(0x76), ARC(0x77), em, em, 0x4B, 0x10, &pos, &rot);
        if (w->pTail[1]) {
            MotSetObj16(w->pTail[1], ARC(0x78), 4, step);
        }
    }
    if (w->pTail[2] == 0) {
        w->pTail[2] = (cObj16*) SetObj16(ARC(0x76), ARC(0x77), em, em, 0x4C, 0x10, &pos, &rot);
        if (w->pTail[2]) {
            MotSetObj16(w->pTail[2], ARC(0x78), 4, step * 2);
        }
    }
    if (w->pTail[3] == 0) {
        w->pTail[3] = (cObj16*) SetObj16(ARC(0x76), ARC(0x77), em, em, 0x4D, 0x10, &pos, &rot);
        if (w->pTail[3]) {
            MotSetObj16(w->pTail[3], ARC(0x78), 4, step * 3);
        }
    }
}

// Resets the parts scale the appearance blew up.
static inline void em31ScaleReset(cEm31* em)
{
    cParts* p;

    for (p = (cParts*) em->pParts; p; p = p->pNext) {
        p->scale.x = 1.0f;
        p->scale.y = 1.0f;
        p->scale.z = 1.0f;
    }
}

void cEm31::setAppearCancel()
{
    Em31Work* w = EM31_WK(this);

    switch (type) {
    case 0:
    default:
        em31SetTail(this);
        em31ScaleReset(this);
        MotionSetCore(this, MOTION(this), PL_ARC_PTR(subArc, 0xA), 0, 0, 5, 0);
        MotionMoveF(this, 0);
        setStatus(5);
        EffectEspDelete(1, w->espKind, (u32) this, 0);
        EffectEspgenDelete(1, w->espKind, (int) this);
        EffectEfmDelete(1, w->espKind, (int) this);
        EmRoutineSet(this, 1, 2, 0, 0);
        break;
    case 1:
        em31ScaleReset(this);
        MotionSetCore(this, MOTION(this), PL_ARC_PTR(subArc, 0x44), 0, 0, 5, 0);
        MotionMoveF(this, 0);
        setStatus(5);
        EffectEspDelete(1, w->espKind, (u32) this, 0);
        EffectEspgenDelete(1, w->espKind, (int) this);
        EffectEfmDelete(1, w->espKind, (int) this);
        EmRoutineSet(this, 1, 0x12, 0, 0);
        break;
    }
}

// One foot step: the dust effect at the parts and the step / dash SE.
// Foot-step effect and sound of one foot parts (a macro: `v` is em31FootSe's Vec so the copy is
// addressed through the SndCall argument register).
#define EM31_FOOT_SE_SUB()                                                                         \
    EstSet(0, -1, &p->worldPos, 0, 0x29, 0xE, 0, 0, 0, 0);                                         \
    v = p->worldPos;                                                                                \
    v.y += 500.0f;                                                                                  \
    if (w->flags & 0x8000) {                                                                        \
        SndCall(8, 0x27, &v, em->id, 0, em);                                                        \
        EstSet(0, -1, 0, 0, 0x29, 0x3C, 0, 0, 0, 0);                                                \
    } else {                                                                                        \
        SndCall(8, 0, &v, em->id, 0, em);                                                           \
    }

void em31FootSe(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    cModel* p;
    Vec v;
    u8 ev = em->motEvent & 0xF0;

    if (ev == 0) {
        return;
    }
    if (em->motEvent & 0x10) {
        if (em->motFlags & 0x40) {
            p = em->getPartsPtr(0xD);
        } else {
            p = em->getPartsPtr(7);
        }
        EM31_FOOT_SE_SUB();
    }
    if (em->motEvent & 0x20) {
        if (em->motFlags & 0x40) {
            p = em->getPartsPtr(7);
        } else {
            p = em->getPartsPtr(0xD);
        }
        EM31_FOOT_SE_SUB();
    }
    if (em->motEvent & 0x40) {
        if (em->motFlags & 0x40) {
            p = em->getPartsPtr(0x19);
        } else {
            p = em->getPartsPtr(0x13);
        }
        EM31_FOOT_SE_SUB();
    }
    if (em->motEvent & 0x80) {
        if (em->motFlags & 0x40) {
            p = em->getPartsPtr(0x13);
        } else {
            p = em->getPartsPtr(0x19);
        }
        EM31_FOOT_SE_SUB();
    }
}

int cEm31::ckAtkHit()
{
    if (type != 1) {
        return 0;
    }
    if ((EM31_WK(this)->flags & 0x400) == 0) {
        return 0;
    }
    return 1;
}

int cEm31::ckRocketEnable()
{
    Em31Work* w = EM31_WK(this);

    switch (type) {
    case 0: {
        cEm31* t = w->pTen;

        if (t == 0) {
            return 0;
        }
        if (t->hp > t->hpMax / 2) {
            return 0;
        }
        if (t->hp <= 0) {
            return 0;
        }
        return 1;
    }
    case 1:
        if (hp > hpMax / 2) {
            return 0;
        }
        if (hp <= 0) {
            return 0;
        }
        return 1;
    }
    return 0;
}

int cEm31::ckEyeBreak()
{
    Em31Work* w = EM31_WK(this);
    int i;

    if (type != 0) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        if (w->eye[i].hp > 0) {
            return 0;
        }
    }
    return 1;
}

int em31BridgeJumpCk(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    Mtx m;
    Vec lp;
    int i;
    int j;

    if (pG->pRoomEmi == 0) {
        return 0;
    }
    for (i = 0; i < ((EmiData*) pG->pRoomEmi)->n; i++) {
        EmiEntry* e = &((EmiData*) pG->pRoomEmi)->entry[i];

        if (e->type != 0x11) {
            continue;
        }
        if (e->sub != 0) {
            continue;
        }
        if (e->state != 0) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
            9000000.0f) {
            continue;
        }
        PSMTXRotRad(m, 'y', e->rotY);
        TransMatrix(m, &e->pos);
        PSMTXInverse(m, m);
        PSMTXMultVec(m, &pPL->pos, &lp);
        if (lp.z < 6000.0f) {
            continue;
        }
        for (j = 0; j < ((EmiData*) pG->pRoomEmi)->n; j++) {
            EmiEntry* f = &((EmiData*) pG->pRoomEmi)->entry[j];

            if (f->type != 0x11) {
                continue;
            }
            if (f->sub != 0) {
                continue;
            }
            if (f->state != 1) {
                continue;
            }
            if (e->pad_3 != f->pad_3) {
                continue;
            }
            if (!(fabsf(Muku(&em->pos, &f->pos, em->rot.y, PI)) > 0.2617994f)) {
                w->bridgePos = f->pos;
                EmRoutineSet(em, 1, 8, 0, 0);
                return 1;
            }
        }
    }
    return 0;
}

int em31BridgeVsCk(cEm31* em, int far)
{
    if (far) {
        if (pPL->pos.x < -49000.0f && em->pos.x > -39000.0f && em->pos.x < -34000.0f && em->pos.z > 55000.0f) {
            return 1;
        }
        if (pPL->pos.x > -39000.0f && em->pos.x < -49000.0f) {
            return 1;
        }
    } else {
        if (pPL->pos.x < -49000.0f && em->pos.x > -39000.0f && em->pos.x < -35000.0f && em->pos.z > 55000.0f) {
            return 1;
        }
        if (pPL->pos.x > -39000.0f && em->pos.x < -49000.0f) {
            return 1;
        }
    }
    return 0;
}

int em31AtkRtnCk(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    Mtx inv;
    Vec pp;
    Vec lp;

    GetPlPos(&pp, 0, 18.0f);
    pp.y = pPL->pos.y;
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pp, &lp);
    if (em31PillarCk(em)) {
        return 1;
    }
    if (w->atkWait != 0) {
        return 0;
    }
    if (pPL->pos.y < em->pos.y + 1000.0f && !(w->flags & 0x40) && lp.x > -2500.0f && lp.x < 2500.0f &&
        lp.z > 3000.0f && lp.z < 4000.0f) {
        if ((u8) (Rnd() % 10) > 4) {
            EmRoutineSet(em, 1, 9, 0, 0);
        } else {
            EmRoutineSet(em, 1, 0xA, 0, 0);
        }
        return 1;
    }
    if (w->pTen == 0) {
        return 0;
    }
    if (w->pTen->ckAtkEnable() == 0) {
        return 0;
    }
    if (lp.x > -1000.0f && lp.x < 1000.0f && lp.y > 1000.0f && lp.z < 6000.0f) {
        EmRoutineSet(em, 1, 0xD, 0, 0);
        return 1;
    }
    if (w->atkRtnWait != 0) {
        return 0;
    }
    if ((u8) (Rnd() % 10) > 4) {
        // Region end (zero code): keeps `li r3, 0` after the store so jump2 folds it into the shared tail.
        do {
            w->atkRtnWait = 15;
        } while (0);
        return 0;
    }
    if (lp.x > -500.0f && lp.x < 500.0f && lp.y > -100.0f && lp.y < 100.0f && lp.z > 3000.0f && lp.z < 5000.0f) {
        if ((u8) (Rnd() % 10) > 4) {
            EmRoutineSet(em, 1, 0xF, 0, 0);
        } else {
            EmRoutineSet(em, 1, 0xB, 0, 0);
        }
        return 1;
    }
    if (lp.x > -800.0f && lp.x < 800.0f && lp.y > -100.0f && lp.y < 100.0f && lp.z > 0.0f && lp.z < 2000.0f) {
        switch (Rnd() & 3) {
        case 0:
        case 1:
        default:
            EmRoutineSet(em, 1, 0xB, 0, 0);
            return 1;
        case 2:
            EmRoutineSet(em, 1, 0x10, 0, 0);
            return 1;
        case 3:
            w->bridgePos = pPL->pos;
            EmRoutineSet(em, 1, 8, 0, 0);
            return 1;
        }
    }
    if (!((em->pos.x - pp.x) * (em->pos.x - pp.x) + (em->pos.z - pp.z) * (em->pos.z - pp.z) < 25000000.0f)) {
        return 0;
    }
    EmRoutineSet(em, 1, 0xF, 0, 0);
    return 1;
}

// Facing of the catch: along the bridge (climb) or slightly off it.
#define EM31_CATCH_ROT_SET()                                                                       \
    if (-1.5707964f < em->rot.y && em->rot.y < 1.5707964f) {                                        \
        if (climb) {                                                                                \
            em->rot.y = 0.0f;                                                                       \
        } else {                                                                                    \
            em->rot.y = 0.5235988f;                                                                 \
        }                                                                                           \
    } else {                                                                                        \
        if (climb) {                                                                                \
            em->rot.y = PI;                                                                         \
        } else {                                                                                    \
            em->rot.y = 3.6651914f;                                                                 \
        }                                                                                           \
    }

void em31CatchPosSet(cEm31* em, int climb)
{
    if (em->pos.x < -44500.0f) {
        em->pos.x = -53000.0f;
        if (em->pos.z < 65000.0f) {
            em->pos.z = 65000.0f;
        }
        if (em->pos.z > 75000.0f) {
            em->pos.z = 75000.0f;
        }
        em->pos.y = 17361.0f;
        EM31_CATCH_ROT_SET();
    } else if (em->pos.x < -32000.0f && em->pos.z > 55188.0f) {
        em->pos.x = -35000.0f;
        if (em->pos.z < 65000.0f) {
            em->pos.z = 65000.0f;
        }
        if (em->pos.z > 75000.0f) {
            em->pos.z = 75000.0f;
        }
        em->pos.y = 15861.0f;
        EM31_CATCH_ROT_SET();
    } else {
        em->pos.x = -28822.0f;
        em->pos.y = 15861.0f;
        em->pos.z = 53249.0f;
        EM31_CATCH_ROT_SET();
    }
}

int em31PLCraneCk(cEm31* em)
{
    Vec tbl[2] = {
        { -51085.0f, 19361.0f, 52299.0f },
        { -37130.0f, 17861.0f, 87721.0f },
    };
    u32 i;

    for (i = 0; i < 2; i++) {
        Vec* p = &tbl[i];

        if ((pPL->pos.x - p->x) * (pPL->pos.x - p->x) + (pPL->pos.z - p->z) * (pPL->pos.z - p->z) > 9000000.0f) {
            continue;
        }
        if (fabsf(pPL->pos.y - tbl[i].y) > 500.0f) {
            continue;
        }
        return 1;
    }
    return 0;
}

// The player's head comes off: the head object flies away (regions other than Japan).
void em31PlHeadLost()
{
    Vec ofs;
    Vec spd;
    cModel* p;
    cObj* obj;

    if (pSys->region == 0) {
        PlSetDamageSe(0xD);
        EstSet((int) pPL, -1, 0, 0, 0x29, 0x29, 0, 0, (u32) pPL, 0);
        return;
    }
    SndCall(1, 0x3E, &pPL->pos, 0, 0, pPL);
    EstSet((int) pPL, -1, 0, 0, 0x29, 0x27, 0, 0, (u32) pPL, 0);
    pPL->setHead(0);
    p = pPL->getPartsPtr(3);
    if (pG->x4FB8 == 2) {
        ofs.x = 0.0f;
        ofs.y = 84.0f;
        ofs.z = 0.0f;
    } else {
        ofs.x = 0.0f;
        ofs.y = 68.0f;
        ofs.z = 28.0f;
    }
    spd.x = 0.0f;
    spd.y = 50.0f;
    spd.z = -25.0f;
    PSMTXMultVec(p->mat, &ofs, &ofs);
    PSMTXMultVecSR(pPL->mat, &spd, &spd);
    obj = SetObj01(PL_ARC_PTR(pG->pPlArc, 0xC), PL_ARC_PTR(pG->pPlArc, 7), &ofs, &pPL->rot, &spd, 10.0f, 150.0f, 1000, 0x11);
    if (obj) {
        obj->lightInfo.x50 = 1;
        Obj01SetEst(obj, 0, -1, 4, 0, -1, 0, -1, 0, -1);
    }
    EstSet((int) obj, -1, 0, 0, 0x29, 0x28, 0, 0, (u32) obj, 0);
}

void em31WeakInit(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    Vec pos;
    Vec rot;
    u32 i;

    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    switch (em->type) {
    case 0:
    default:
        for (i = 0; i < 4; i++) {
            Em31Eyelid* e = &w->eye[i];

            e->pObj = SetObj00(ARC(6), ARC(7), &pos, &rot);
            if (e->pObj) {
                e->pObj->scale.x = 1.1f;
                e->pObj->scale.y = 1.1f;
                e->pObj->scale.z = 1.1f;
                e->pObj->lightInfo.x50 = 0x80;
                OyaSetObj00(e->pObj, em, e->parts2);
                e->pObj->atari.throughOn();
            }
        }
        break;
    case 1:
        w->pWeak = SetObj00(ARC(6), ARC(7), &pos, &rot);
        if (w->pWeak) {
            w->pWeak->scale.x = 1.3f;
            w->pWeak->scale.y = 1.3f;
            w->pWeak->scale.z = 1.3f;
            w->pWeak->lightInfo.x50 = 0x80;
            OyaSetObj00(w->pWeak, em, 0xB);
            w->pWeak->atari.throughOn();
        }
        break;
    }
}

void em31WeakMove(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    int i;

    switch (em->type) {
    case 0:
    default:
        for (i = 0; i < 4; i++) {
            Em31Eyelid* e = &w->eye[i];

            if (e->pObj) {
                if ((pGS->flags_5010 & 0x04000000) && e->hp > 0 && em->hp > 0 && e->closed == 0) {
                    e->pObj->be_flag |= 2;
                } else {
                    e->pObj->be_flag &= ~2;
                }
            }
        }
        break;
    case 1:
        if (w->pWeak) {
            if ((pG->flags_5010 & 0x04000000) && em->hp > 0 && (em->hitInfo.flags & 1)) {
                w->pWeak->be_flag |= 2;
            } else {
                w->pWeak->be_flag &= ~2;
            }
        }
        break;
    }
}

void cEm31::setVoice(int no, int timer)
{
    Em31Work* w = EM31_WK(this);
    cModel* p;

    if (type != 1) {
        return;
    }
    SndStop(w->sndId, 0);
    p = getPartsPtr(0xB);
    w->sndId = SndCall(8, no, &p->worldPos, id, 0, this);
    SndStop(w->sndId2, 0);
    w->breathTimer = timer;
}

void em31BreathSe(cEm31* em)
{
    Em31Work* w = EM31_WK(em);

    if (em->type != 1) {
        return;
    }
    if (w->breathTimer) {
        w->breathTimer--;
    } else {
        w->breathTimer = 0x3B;
        w->sndId2 = SndCall(8, 0x21, &em->getPartsPtr(0xB)->worldPos, em->id, 0, em);
    }
}

void em31BreathSeStopCk(cEm31* em)
{
    Em31Work* w = EM31_WK(em);
    u32 no;

    if (em->type != 1) {
        return;
    }
    no = em->seNo;
    if (no == 0) {
        return;
    }
    switch (no - 1) {
    default:
        return;
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1C:
    case 0x1F:
    case 0x37:
        SndStop(w->sndId2, 0);
        w->breathTimer = 2;
        break;
    }
}
