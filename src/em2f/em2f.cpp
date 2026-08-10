// em2f module (D:/Bio4/Prog/em2f.cpp): the lake monster. It swims along the room's EMI route points
// (em2fSetNextRoute / em2fChangeRoute), dives and surfaces (em2fWaterEffSet), rams the boat from
// below (em2f_R1_RisingDragon), swallows the player (em2f_R1_Packman), hides until the harpoons hit
// (em2f_R1_HideMode), drags the player under when it dies (em2f_R1_Critical with its camera) and
// breaks the floating islands it runs into (em2fIslandCrashCk); its tentacles are cObj16 objects.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "map_obj.h"
#include "widget.h"
#include "em2f.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "embarrel.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "route_ck.h"
#include "cam_ctrl.h"
#include "quake.h"
#include "game.h"
#include "dbmodule.h"
#include "pad.h"
#include "player.h"
#include "pl_npc.h"
#include "snd.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

// The module's 0x34-byte COMMON block (st_room.h): uninitialised template statics of the original
// object, merged into .bss by the REL link.
asm(".comm common_em2f,52,4");

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// em_set.cpp's EmSetDieCnt takes no argument; the module passes the enemy.
void EmSetDieCntE(cEm* em) asm("EmSetDieCnt");

// game/obj16.cpp: the tentacle objects (obj16.h pulls in em10.h).
extern "C" {
cObj* SetObj16(void* bin, void* tpl, cModel* target, cModel* body, int partsNo, u8 type, Vec* pos, Vec* rot);
void MotSetObj16(cObj* obj, void* mot, int a, int b);
}

// Enemy head object (game/obj16.cpp; em10.h declares the same class, which this module cannot include).
class cObj16 : public cObj {
public:
    void clearLostWait();
};

// Floating island (game/obj1c.cpp): only what the crash check calls.
class cObj1c : public cObj {
public:
    void setCrashBig(Vec* from);
    int ckCrash();
};

typedef void (*Em2fFunc)(cEm2f*);

static void em2f_R0_Init(cEm2f* em);
static void em2f_R0_Move(cEm2f* em);
static void em2f_R1_Wait(cEm2f* em);
static void em2f_R1_Walk(cEm2f* em);
static void em2f_R1_SwimWait(cEm2f* em);
static void em2f_R1_Swim(cEm2f* em);
static void em2f_R1_SwimTurn90(cEm2f* em);
static void em2f_R1_SwimTurn180(cEm2f* em);
static void em2f_R1_SwimTurn180Atk(cEm2f* em);
static void em2f_R1_RisingDragon(cEm2f* em);
static void em2f_R1_Packman(cEm2f* em);
static void em2f_R1_HideMode(cEm2f* em);
static void em2f_R1_Critical(cEm2f* em);
static void em2f_R0_Damage(cEm2f* em);
static void em2f_R1_Dm_Normal(cEm2f* em);
static void em2f_R0_Die(cEm2f* em);
static void em2f_R1_Die_Normal(cEm2f* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)

// Struct-member view of the player pointer (cam_ctrl.cpp PlayerPtr).
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)
struct SubCharPtr {
    cSubChar* p;
};
#define pSUBS (((SubCharPtr*) &pSUB)->p)

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

static inline void IntSet(int& d, int v) { d = v; }

// Effect `no` of the monster's effect set on itself.
static inline void em2fEstSet(cEm2f* em, Em2fWork* w, int no)
{
    EstSet((int) em, -1, 0, 0, 0x27, no, 0, w->espKind, (u32) em, 0);
}

// The effects the monster owns are removed before it is moved somewhere else.
static inline void em2fEffectDelete(cEm2f* em, Em2fWork* w)
{
    EffectEspDelete(0, w->espKind, (u32) em, 0);
    EffectEspgenDelete(0, w->espKind, (int) em);
    EffectEfmDelete(0, w->espKind, (int) em);
}

extern "C" void _prolog()
{
    OSReport("em2f prolog Ok\n");
    EmInitFunc = Em2fInit;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em2fInit(cEm* em)
{
    new (em) cEm2f();
}

void em2fDmCk(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int dmg;

    w->flags &= ~0x40;
    if (em->dmHit == 0) {
        return;
    }
    w->flags |= 0x40;
    em->dmHit = 0;
    switch (em->dmWep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x2B:
        dmg = 1;
        break;
    case 0x15:
        dmg = 100;
        break;
    case 7:
    case 8:
    case 0x21:
        dmg = 1;
        if (em->plDist2 > 36000000.0f) {
            dmg = 3;
        }
        break;
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2A:
    case 0x2C:
    default:
        dmg = 1000;
        break;
    }
    LifeDownSet2(em, dmg, 0, 0);
    if (em->hp <= 0) {
        EmSetDie(em);
        EmSetDieCntE(em);
        em->clearStatus(5);
    }
}

Em2fFunc Em2f_R0_move_tbl[4] = {
    em2f_R0_Init,
    em2f_R0_Move,
    em2f_R0_Damage,
    em2f_R0_Die,
};

static Em2fFunc Em2f_R1_move_tbl[11] = {
    em2f_R1_Wait,
    em2f_R1_Walk,
    em2f_R1_SwimWait,
    em2f_R1_Swim,
    em2f_R1_SwimTurn90,
    em2f_R1_SwimTurn180,
    em2f_R1_SwimTurn180Atk,
    em2f_R1_RisingDragon,
    em2f_R1_Packman,
    em2f_R1_HideMode,
    em2f_R1_Critical,
};

static Em2fFunc Em2f_R2_move_tbl[1] = {
    em2f_R1_Dm_Normal,
};

static Em2fFunc Em2f_R3_move_tbl[1] = {
    em2f_R1_Die_Normal,
};

// Route points used when the room has no EMI data.
static Vec em2f_route_tbl[4] = {
    { -25230.0f, -2000.0f, 98080.0f },
    { 11390.0f, -2000.0f, 113600.0f },
    { 49290.0f, -2000.0f, 18650.0f },
    { 95900.0f, -2000.0f, 18650.0f },
};

// Parts index remap of the flipped motions (cModel::motFlip).
static u16 em2f_flip_tbl[80] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xD, 0xE, 0xF, 0xA, 0xB, 0xC, 0x10, 0x14, 0x15, 0x16, 0x11, 0x12, 0x13, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
    0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,
    0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
};

// Where the monster surfaces for the death scene.
static Vec em2f_die_pos = { -46800.0f, -734.0f, 22157.0f };

// The camera em2fCriCamMove installs (the drowning cut): explicitly zero-initialised so it stays
// in .data.
static Camera em2f_cri_cam = { 0 };

void cEm2f::move()
{
    Em2fWork* w = EM2F_WK(this);

    if (xFC) {
        em2fDmCk(this);
    }
    if (w->waitTimer) {
        w->waitTimer--;
    }
    w->flags &= ~0x58F;
    em2fRouteCk(this);
    Em2f_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    partsWorldCalc();
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    em2fIslandCrashCk(this);
    if (seFlags28B & 0x10) {
        if (w->seTimer1) {
            w->seTimer1--;
        } else {
            w->seTimer1 = 90;
            w->sndId1 = SndCall(8, 0xB, &pos, id, 0, this);
        }
        if (w->seTimer2) {
            w->seTimer2--;
        } else {
            w->seTimer2 = 57;
            w->sndId2 = SndCall(8, 0xC, &pos, id, 0, this);
        }
        if (w->flags & 0x400) {
            if (w->seTimer3) {
                w->seTimer3--;
            } else {
                w->seTimer3 = 45;
                SndCall(8, 0x10, &pos, id, 0, this);
            }
        }
    } else {
        SndStop(w->sndId1, 0);
        SndStop(w->sndId2, 0);
    }
    em2fTentacleMove(this);
}

static void em2f_R0_Init(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int zero;
    u32 i;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(5)) == 0) {
            pLog->err(0, 0, "em2b() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    case 1:
        if (em->modelInit(ARC(6), ARC(7)) == 0) {
            pLog->err(0, 0, "em2b() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    }
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 50000.0f, 50000.0f, 50000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    AtariInit(&em->atari, 0.0f, 0.0f, 0.0f, 700.0f, 3500.0f, 3500.0f, 2000.0f, 1, 2, 0);   // COMPILER-DIFF: #1
    YarareInit(em, 0.0f, -200.0f, 0.0f, 1200.0f, 1000.0f, 9, 5);
    YarareAdd(em, &w->hit[0], 0.0f, -200.0f, 0.0f, 1200.0f, 1400.0f, 2, 5);
    zero = 0;
    YarareAdd(em, &w->hit[1], 0.0f, -100.0f, 0.0f, 1400.0f, 1400.0f, 4, 5);
    YarareAdd(em, &w->hit[2], 0.0f, -200.0f, 0.0f, 1200.0f, 700.0f, 6, 5);
    YarareAdd(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 700.0f, 1400.0f, 0x19, 5);
    YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 900.0f, 1000.0f, 0x1B, 5);
    YarareAdd(em, &w->hit[5], 0.0f, 0.0f, 0.0f, 700.0f, 600.0f, 0x1C, 5);
    YarareAdd(em, &w->hit[6], 0.0f, 0.0f, -600.0f, 500.0f, 600.0f, 0x1D, 5);
    em->hp = 1000;
    em->motFlip = em2f_flip_tbl;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(8), 0x27, 0);
    w->flags = zero;
    w->x580 = em->pos.y;
    w->x584 = 0.0f;
    w->x5C9 = zero;
    w->x5CB = zero;
    w->effTimer1 = 30;
    w->effTimer2 = 3;
    w->effTimer3 = 3;
    w->atkCnt = Rnd() % 3 + 1;
    // the zero's dying store (pBoat, written last) is issued first, the rest in source order
    w->seTimer1 = zero;
    w->seTimer2 = zero;
    w->seTimer3 = zero;
    w->sndId1 = zero;
    w->sndId2 = zero;
    w->pBoat = (cEm*) zero;
    w->rndFlag = Rnd() & 1;
    for (i = 0; i < 6; i++) {
        w->pTentacle[i] = 0;
    }
    w->pCtrl12 = GetCtrlCtrl12();
    w->espKind = EspPullCoreKind();
    w->waterY = em->pos.y;
    w->routeIdx = -1;
    w->nextRouteType = em2fSetNextRoute(em);
    w->routeType = w->nextRouteType;
    switch (em->x38D) {
    case 0:
    default:
        em->setStatus(5);
        // plain byte stores (em30 rule): the int inline's SI zero would take r9 and reload_cse
        // would delete MotionSetCore's `li r9, 0`
        em->xFC = 1;
        em->xFD = 0;
        em->xFE = 0;
        em->xFF = 0;
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 0x401, 0);
        MotionMoveF(em, 0);
        break;
    case 1:
        em->atari.flags &= ~0x100;
        em->setStatus(5);
        em->scale.x = 2.0f;
        em->scale.y = 2.0f;
        em->scale.z = 2.0f;
        EmRoutineSet(em, 1, 2, 0, 0);
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 0x401, 0);
        MotionMoveF(em, 0);
        break;
    case 2:
        em->atari.flags &= ~0x100;
        em->scale.x = 2.0f;
        em->scale.y = 2.0f;
        em->scale.z = 2.0f;
        em->setStatus(3);
        em->atari.flags &= ~0x300;
        em->atari.flags |= 8;
        EmRoutineSet(em, 1, 0xA, 0, 0);
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 0x401, 0);
        MotionMoveF(em, 0);
        break;
    }
    em2f_R0_Move(em);
}

static void em2f_R0_Move(cEm2f* em)
{
    em->flags_3C8 &= ~0x1FC;
    Em2f_R1_move_tbl[em->xFD](em);
    if (em->seFlags28B & 1) {
        em->flags_3C8 |= 4;
    }
    if (em->seFlags28B & 0x80) {
        em->flags_3C8 |= 8;
    }
    if (em->seFlags28B & 8) {
        em->flags_3C8 |= 0x10;
    }
}

static void em2f_R1_Wait(cEm2f* em)
{
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 10, 0x405, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        break;
    }
}

static void em2f_R1_Walk(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(9), 0, 10, 0x405, 0);
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, PI / 32.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if (em->plDist2 < 1000000.0f) {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
}

static void em2f_R1_SwimWait(cEm2f* em)
{
    // single use in another block: update_equiv_regs moves the li next to the stb (short qty, r0)
    int one = 1;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 1, 0);
        MotionMoveF(em, 0);
        em->be_flag &= ~2;
        em->xFE++;
    case 1:
        if (pG->flags_5010 & 0x00200000) {
            em->be_flag |= 2;
            EmRoutineSet(em, one, 3, 0, 0);
        }
        break;
    }
}

static void em2f_R1_Swim(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int flip;
    f32 d;

    flip = 0x405;
    if (w->flags & 0x20) {
        flip = 0x445;
    }
    switch (em->xFE) {
    case 0:
        em->atari.flags &= ~0x100;
        MotionSetCore(em, MOTION(em), ARC(0xA), (int) ARC(0x1D), 30, flip, 0);
        w->waitTimer = Rnd() % 90 + 90;
        w->x424 = 0;
        w->x420 = 10;
        w->x41C = 0.0f;
        if (w->flags & 0x10) {
            w->dive = w->waterY - 5000.0f - em->pos.y;
        } else {
            w->dive = w->waterY - em->pos.y;
        }
        w->timer = 0;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->nextPos, em->rot.y, PI / 200.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if (em->seFlags28B & 0x40) {
            w->flags |= 0x20;
        } else {
            w->flags &= ~0x20;
        }
        {
            f32 t = w->dive * 0.05f;
            em->pos.y += t;
            w->dive -= t;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xE), (int) ARC(0x1F), 30, flip, 0);
        em2fEstSet(em, w, 0xC);
        w->timer = 80;
        em->xFE++;
        goto swim;  // the target's case 2 runs the swim step too (tails cross-jumped into case 4's)
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x1E), 30, flip, 0);
        em2fEstSet(em, w, 4);
        w->timer = 80;
        em->xFE++;
    case 3:
    case 5:
    swim:
        em->rot.y += Muku(&em->pos, &w->nextPos, em->rot.y, PI / 200.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    }
    em2fWaterEffSet(em);
    if (em2fRisingDragonCk(em)) {
        return;
    }
    if (w->timer) {
        w->timer--;
    }
    {
        cModel* p = em->getPartsPtr(8);

        d = (p->worldPos.x - w->nextPos.x) * (p->worldPos.x - w->nextPos.x)
            + (p->worldPos.z - w->nextPos.z) * (p->worldPos.z - w->nextPos.z);
        if (d < 100000000.0f) {
            w->routeType = w->nextRouteType;
            w->nextRouteType = em2fSetNextRoute(em);
            d = (em->pos.x - w->nextPos.x) * (em->pos.x - w->nextPos.x) + (em->pos.z - w->nextPos.z) * (em->pos.z - w->nextPos.z);
            if (w->routeType == 2) {
                if (w->timer) {
                    return;
                }
                if (w->atkCnt) {
                    EmRoutineSet(em, 1, 6, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 9, 0, 0);
                }
                return;
            }
        }
        if (w->timer) {
            return;
        }
        {
            f32 ang = fabsf(Muku(&p->worldPos, &w->nextPos, em->rot.y, PI));

            if (ang > 1.0471976f && d < 1600000000.0f) {
                em2fChangeRoute(em);
                return;
            }
            if (ang > 2.3561945f) {
                EmRoutineSet(em, 1, 5, 0, 0);
                return;
            }
            if (ang > 1.0471976f) {
                EmRoutineSet(em, 1, 4, 0, 0);
                return;
            }
        }
        if (w->flags & 0x10) {
            if (w->routeType == 0) {
                w->flags &= ~0x10;
                em->xFE = 4;
            }
        } else {
            if (w->routeType == 1) {
                w->flags |= 0x10;
                em->xFE = 2;
            }
        }
    }
}

static void em2f_R1_SwimTurn90(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int flip;

    flip = 0x405;
    if (w->flags & 0x20) {
        flip = 0x445;
    }
    switch (em->xFE) {
    case 0:
        if (Muku(&em->pos, &w->nextPos, em->rot.y, PI) < 0.0f) {
            if (w->flags & 0x20) {
                MotionSetCore(em, MOTION(em), ARC(0x10), (int) ARC(0x21), 30, flip, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0xF), (int) ARC(0x20), 30, flip, 0);
            }
            if (!(w->flags & 0x10)) {
                em2fEstSet(em, w, 1);
            }
        } else {
            if (w->flags & 0x20) {
                MotionSetCore(em, MOTION(em), ARC(0xF), (int) ARC(0x20), 30, flip, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x10), (int) ARC(0x21), 30, flip, 0);
            }
            if (!(w->flags & 0x10)) {
                em2fEstSet(em, w, 0xD);
            }
        }
        em->xFE++;
    case 1:
        if (em->seFlags28B & 2) {
            em->rot.y += Muku(&em->pos, &w->nextPos, em->rot.y, PI / 200.0f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
    em2fWaterEffSet(em);
    em2fRisingDragonCk(em);
}

static void em2f_R1_SwimTurn180(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int flip;

    flip = 0x405;
    if (w->flags & 0x20) {
        flip = 0x445;
    }
    switch (em->xFE) {
    case 0:
        if (w->flags & 0x10) {
            MotionSetCore(em, MOTION(em), ARC(0x1A), (int) ARC(0x2B), 30, flip, 0);
            w->flags &= ~0x10;
            em2fEstSet(em, w, 0x12);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x19), (int) ARC(0x2A), 30, flip, 0);
            w->flags |= 0x10;
            em2fEstSet(em, w, 0x11);
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
    em2fWaterEffSet(em);
    em2fRisingDragonCk(em);
}

static void em2f_R1_SwimTurn180Atk(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int flip;

    flip = 0x405;
    if (w->flags & 0x20) {
        flip = 0x445;
    }
    switch (em->xFE) {
    case 0:
        if ((pG->flags_5010 & 0x00200000) && w->atkCnt) {
            w->atkCnt--;
        }
        if (w->flags & 0x10) {
            MotionSetCore(em, MOTION(em), ARC(0x18), (int) ARC(0x29), 30, flip, 0);
            em2fEstSet(em, w, 0x10);
        } else if (Rnd() & 3) {
            MotionSetCore(em, MOTION(em), ARC(0x16), (int) ARC(0x27), 30, flip, 0);
            em2fEstSet(em, w, 0xE);
        } else if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(0x13), (int) ARC(0x24), 30, flip, 0);
            em2fEstSet(em, w, 9);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x14), (int) ARC(0x25), 30, flip, 0);
            em2fEstSet(em, w, 8);
        }
        w->flags &= ~0x10;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->flags &= ~0x10;
            EmRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
    em2fWaterEffSet(em);
    em2fRisingDragonCk(em);
}

static void em2f_R1_RisingDragon(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int r;
    // `hide` is set again after the switch: its last mention lies beyond the case-1 ebb, so cse2
    // keeps it (not the timer register, whose last use is the decrement) as the canonical zero of
    // the routine stores. `one` (single use in another block) is moved next to its stb by
    // update_equiv_regs and takes r0.
    u32 hide;
    int one = 1;

    em->flags_3C8 |= 0x80;
    switch (em->xFE) {
    case 0:
        em2fSetPosBetweenBoat(em);
        w->flags &= ~0x20;
        MotionSetCore(em, MOTION(em), ARC(0xA), (int) ARC(0x1D), 0, 5, 0);
        w->timer = 149;
        w->timer2 = 15;
        w->risingOk = 0;
        w->flags |= 0x10;
        em->xFE++;
    case 1:
        em->flags_3C8 |= 0x108;
        MotionMoveF(em, 0);
        if (w->timer == 0) {
            hide = pG->flags_5010 & 0x00400000;
            if (hide == 0) {
                EmRoutineSet(em, one, 3, 0, 0);
                break;
            }
            em->xFE++;
            break;
        }
        do { w->timer--; } while (0);  // LOOP_END barrier: the timer2 load stays below the store
        if (w->timer2) {
            w->timer2--;
            if (w->timer2 == 0) {
                SndCall(8, 0xE, &em->pos, em->id, 0, em);
            }
        }
        break;
    case 2:
        em2fSetPosRisingD(em);
        if (w->flags & 0x200) {
            SndRoomStrStart(1, 3, 1);
            w->flags &= ~0x200;
        }
        MotionSetCore(em, MOTION(em), ARC(0xA), (int) ARC(0x1D), 0, 5, 75);
        w->flags |= 0x10;
        w->timer = 170;
        em->xFE++;
    case 3:
        em->flags_3C8 |= 0x108;
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x15), (int) ARC(0x26), 10, 0x401, 0);
        w->flags &= ~0x10;
        em2fEstSet(em, w, 7);
        w->timer = 0;
        w->timer2 = 90;
        em->xFE++;
    case 5:
        if (w->timer2) {
            w->timer2--;
            em->flags_3C8 |= 0x100;
        }
        r = MotionMoveF(em, 0);
        if (r) {
            em2fChangeRoute(em);
            EmRoutineSet(em, 1, 3, 0, 0);
        } else {
            if (em->seFlags28B & 1) {
                if (pG->flags_5010 & 0x00400000) {
                    EmRoutineSet(pPL, r, 0xF, 9, r);
                    SndCall(8, 0x1E, &em->pos, em->id, 0, em);
                    SndCall(8, 0x1F, &pPL->pos, em->id, 0, pPL);
                }
                w->timer = 30;
                em->flags_3C8 |= 0x40;
            }
            if (w->timer) {
                w->timer--;
                AddWaterPower(&em->pos, 3.0f);
            }
            if (em->motFrame > 129.7f && em->motFrame < 130.3f) {
                em->flags_3C8 |= 0x40;
            }
        }
        break;
    }
    hide = pG->flags_5010 & 0x00400000;
    if (hide == 0) {
        w->risingOk = 1;
    }
    if (w->risingOk && em2fRisingDragonCk(em)) {
        return;
    }
    em2fWaterEffSet(em);
}

static void em2f_R1_Packman(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int r;
    u32 hide;  // see em2f_R1_RisingDragon
    int one = 1;

    switch (em->xFE) {
    case 0:
        em2fSetPosBetweenBoat(em);
        w->flags &= ~0x20;
        MotionSetCore(em, MOTION(em), ARC(0xA), (int) ARC(0x1D), 0, 5, 0);
        w->timer = 149;
        w->risingOk = 0;
        w->timer2 = 15;
        w->flags |= 0x10;
        em->xFE++;
    case 1:
        em->flags_3C8 |= 0x108;
        MotionMoveF(em, 0);
        if (w->timer == 0) {
            hide = pG->flags_5010 & 0x00400000;
            if (hide == 0) {
                EmRoutineSet(em, one, 3, 0, 0);
                break;
            }
            em->xFE++;
            break;
        }
        do { w->timer--; } while (0);  // LOOP_END barrier: the timer2 load stays below the store
        if (w->timer2) {
            w->timer2--;
            if (w->timer2 == 0) {
                SndCall(8, 0xE, &em->pos, em->id, 0, em);
            }
        }
        break;
    case 2:
        em2fSetPosPackman(em);
        if (w->flags & 0x200) {
            SndRoomStrStart(1, 3, 1);
            w->flags &= ~0x200;
        }
        MotionSetCore(em, MOTION(em), ARC(0xA), (int) ARC(0x1D), 0, 5, 0);
        w->flags |= 0x10;
        w->timer = 180;
        em->xFE++;
    case 3:
        em->flags_3C8 |= 0x108;
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x17), (int) ARC(0x28), 10, 0x401, 0);
        w->flags &= ~0x10;
        em2fEstSet(em, w, 0xF);
        w->timer = 0;
        w->timer2 = 90;
        em->xFE++;
    case 5:
        if (w->timer2) {
            w->timer2--;
            em->flags_3C8 |= 0x100;
        }
        if (em->seFlags28B & 4) {
            SndCall(8, 0x19, &em->pos, em->id, 0, em);
        }
        r = MotionMoveF(em, 0);
        if (r) {
            if ((s16) pG->pl_life > 0) {
                em2fChangeRoute(em);
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                em->xFE++;
            }
        } else {
            if (em->seFlags28B & 1) {
                if (pG->flags_5010 & 0x00400000) {
                    EmRoutineSet(pPL, r, 0xF, 9, r);
                    pPL->be_flag &= ~2;
                    SndCall(8, 0x1E, &em->pos, em->id, 0, em);
                }
                w->timer = 30;
            }
            if (w->timer) {
                w->timer--;
                AddWaterPower(&em->pos, 3.0f);
            }
            if (em->motFrame > 59.7f && em->motFrame < 60.3f) {
                em->flags_3C8 |= 0x40;
            }
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0xE), (int) ARC(0x1F), 30, 1, 0);
        em2fEstSet(em, w, 0xC);
        em->xFE++;
    case 7:
        if (MotionMoveF(em, 0)) {
            em2fChangeRoute(em);
            EmRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
    hide = pG->flags_5010 & 0x00400000;
    if (hide == 0) {
        w->risingOk = 1;
    }
    if (w->risingOk && em2fRisingDragonCk(em)) {
        return;
    }
    em2fWaterEffSet(em);
}

static void em2f_R1_HideMode(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int flip;

    flip = 0x405;
    if (w->flags & 0x20) {
        flip = 0x445;
    }
    if (w->flags & 0x40) {
        w->hideRush = 1;
    }
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x1C), (int) ARC(0x2D), 30, flip, 0);
        w->flags &= ~0x20;
        w->atkCnt = Rnd() % 3 + 1;
        w->timer = 120;
        SndRoomStrStop(5);
        SndEventStrStop(5);
        w->flags |= 0x200;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            em->flags_3C8 |= 0x28;
        }
        if (MotionMoveF(em, 0)) {
            em->be_flag &= ~2;
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xA), (int) ARC(0x1D), 0, 5, 0);
        w->flags |= 0x10;
        w->timer = Rnd() % 90 + 90;
        em->xFE++;
    case 3:
        em->flags_3C8 |= 0x128;
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->xFE++;
        }
        break;
    case 4:
        em2fSetPosHideMode(em);
        w->hideRush = 0;
        MotionSetCore(em, MOTION(em), ARC(0x32), (int) ARC(0x34), 30, flip, 0);
        em->be_flag |= 2;
        SndRoomStrStart(1, 3, 1);
        w->flags &= ~0x200;
        em2fEstSet(em, w, 0x15);
        em->xFE++;
    case 5:
        em->flags_3C8 |= 0x12C;
        w->flags |= 0x480;
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(9), (int) ARC(0x33), 30, 5, 0);
        SndCall(8, 0x1A, &em->pos, em->id, 0, em);
        SndCall(8, 0x1C, &em->pos, em->id, 0, em);
        w->flags &= ~0x10;
        em->xFE++;
    case 7:
        em->flags_3C8 |= 0x12C;
        w->flags |= 0x580;
        MotionMoveF(em, 0);
        if (em->plDist2 < 900000000.0f) {
            if (w->hideRush) {
                em->xFE = 0xA;
            } else {
                em->xFE = 8;
            }
        }
        break;
    case 8:
        MotionSetCore(em, MOTION(em), ARC(0x17), (int) ARC(0x28), 10, 0x401, 0);
        w->flags &= ~0x10;
        em2fEstSet(em, w, 0xF);
        w->timer2 = 90;
        em->xFE++;
    case 9:
        em->flags_3C8 |= 0x12C;
        if (w->timer2) {
            w->timer2--;
            em->flags_3C8 |= 0x100;
        }
        w->flags |= 0x480;
        if (em->seFlags28B & 4) {
            SndCall(8, 0x1B, &em->pos, em->id, 0, em);
        }
        if (MotionMoveF(em, 0)) {
            em2fChangeRoute(em);
            EmRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    case 0xA:
        MotionSetCore(em, MOTION(em), ARC(0xE), (int) ARC(0x1F), 30, flip, 0);
        em2fEstSet(em, w, 0x16);
        w->timer2 = 90;
        em->xFE++;
    case 0xB:
        em->flags_3C8 |= 0x28;
        if (w->timer2) {
            w->timer2--;
            em->flags_3C8 |= 0x100;
        }
        if (MotionMoveF(em, 0)) {
            em2fChangeRoute(em);
            EmRoutineSet(em, 1, 3, 0, 0);
        }
        break;
    }
    if (em2fRisingDragonCk(em)) {
        return;
    }
    em2fWaterEffSet(em);
}

static void em2f_R1_Critical(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    Mtx m;
    Vec v;

    switch (em->xFE) {
    case 0:
        em->rot.y = -1.57f;
        PSMTXRotRad(m, 'y', -1.57f);
        v.x = -58.0f;
        v.y = -48095.0f;
        v.z = -35742.0f;
        PSMTXMultVecSR(m, &v, &v);
        PSVECAdd(&em2f_die_pos, &v, &em->pos);
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 0x405, 0);
        MotionMoveF(em, 0);
        em->be_flag &= ~2;
        w->timer = 5;
        em->xFE++;
    case 1:
        if ((em2f_die_pos.x - pPL->pos.x) * (em2f_die_pos.x - pPL->pos.x) + (em2f_die_pos.z - pPL->pos.z) * (em2f_die_pos.z - pPL->pos.z)
            > 6250000.0f) {
            break;
        }
        if (pPL->pos.y < -800.0f) {
            break;
        }
        if (!(pG->flags_500C & 0x00800000)) {
            break;
        }
        if (w->timer) {
            QuakeExec(0, 0, 30, 3.0f, 2);
            w->timer--;
        } else {
            em->xFE++;
        }
        break;
    case 2:
        em->rot.y = -1.57f;
        PSMTXRotRad(m, 'y', -1.57f);
        v.x = -58.0f;
        v.y = -24500.0f;
        v.z = -20000.0f;
        PSMTXMultVecSR(m, &v, &v);
        PSVECAdd(&em2f_die_pos, &v, &em->pos);
        em->be_flag |= 2;
        KeyStop(0xEFCF0000);
        w->camPos.x = -51287.0f;
        w->camPos.y = 3580.0f;
        w->camPos.z = 23010.0f;
        w->camAt.x = -46241.0f;
        w->camAt.y = 28.0f;
        w->camAt.z = 22609.0f;
        w->timer = 3;
        w->timer2 = 15;
        MotionSetCore(em, MOTION(em), ARC(0x1B), (int) ARC(0x2C), 0, 1, 0);
        em2fEstSet(em, w, 0x13);
        em->xFE++;
    case 3:
        if (w->timer) {
            w->timer--;
        } else {
            if (w->timer2) {
                w->timer2--;
                if (w->timer2 == 0) {
                    w->camPos.x = -57285.0f;
                    w->camPos.y = 38.0f;
                    w->camPos.z = 22167.0f;
                    w->camAt.x = -45583.0f;
                    w->camAt.y = 221.0f;
                    w->camAt.z = 22613.0f;
                }
            }
            em2fCriCamMove(em);
        }
        if (em->seFlags28B & 4) {
            pPL->be_flag &= ~2;
        }
        if (em->seFlags28B & 1) {
            DiedemoExec(2, 0);
            pG->pl_life = 0;
        }
        if (MotionMoveF(em, 0)) {
            em->hp = 0;
            em->be_flag &= ~2;
            em->xFE++;
        }
        break;
    }
}

void em2fCriCamMove(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    Camera* c = &pG->Cam;
    Camera* cam;
    Vec v;

    PosToPos(&c->param.pos, &w->camPos, &em2f_cri_cam.param.pos, 0.1f);
    PosToPos(&c->param.at, &w->camAt, &em2f_cri_cam.param.at, 0.1f);
    v.x = fRand1_1() * 50.0f;
    v.y = fRand1_1() * 50.0f;
    v.z = fRand1_1() * 50.0f;
    PSVECAdd(&em2f_cri_cam.param.pos, &v, &em2f_cri_cam.param.pos);
    PSVECAdd(&em2f_cri_cam.param.at, &v, &em2f_cri_cam.param.at);
    {
        // cam BEFORE the pos/at pointers: its lo_sum finds no register for em2f_cri_cam+N (the call
        // arguments were hard regs) and stays a fresh lis/addi; the pointers then reuse the call
        // arguments' lo_sum table entries instead of being re-based on cam.
        Vec* pos;
        Vec* at;
        f32 dx, dy, dz;
        cam = &em2f_cri_cam;
        pos = &em2f_cri_cam.param.pos;
        at = &em2f_cri_cam.param.at;
        dx = pos->x - at->x;
        dy = pos->y - at->y;
        dz = pos->z - at->z;
        cam->param.fovy = c->param.fovy;
        cam->up.x = 0.0f;
        cam->up.y = 1.0f;
        cam->up.z = 0.0f;
        cam->dist = SQRTF(dx * dx + dy * dy + dz * dz);
    }
    CameraSetOrientationUp(cam);
    CamCtrl.x250 = (s32) cam;
}

static void em2f_R0_Damage(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);

    w->flags |= 8;
    Em2f_R2_move_tbl[em->xFD](em);
}

static void em2f_R1_Dm_Normal(cEm2f* em)
{
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(9), 0, 3, 0x401, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
}

static void em2f_R0_Die(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);

    w->flags |= 8;
    Em2f_R3_move_tbl[em->xFD](em);
}

static void em2f_R1_Die_Normal(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    cPlayer* pl;

    switch (em->xFE) {
    case 0:
        em2fSetPosDie(em);
        MotionSetCore(em, MOTION(em), ARC(0x2E), 0, 0, 0x401, 0);
        pl = pPL;
        pl->xFD = 0xF;
        pl->xFE = 0xC;
        pl->xFC = 0;
        pl->xFF = 0;
        if (w->pBoat) {
            w->pBoat->xFC = 1;
            w->pBoat->xFD = 1;
            w->pBoat->xFE = 0;
            w->pBoat->xFF = 0;
        }
        w->timer = 112;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->clearStatus(5);
            em->xFE++;
        } else if (w->timer) {
            w->timer--;
            if (w->timer == 0) {
                em->clearStatus(5);
            }
        }
        break;
    case 2:
        w->timer = 30;
        em->xFE++;
    case 3:
        if (w->timer) {
            w->timer--;
        } else {
            em->alpha -= 0.02f;
            if (em->alpha < 0.0f) {
                em->alpha = 0.0f;
                em->be_flag &= ~2;
            }
        }
        break;
    }
}

void em2fRouteCk(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);

    if (em->hp <= 0) {
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
    w->flags &= ~4;
    if (w->flags & 2) {
        if (!(w->flags & 1) || em->plDist2 > em->x374) {
            w->targetPos = w->subRoutePos;
            w->targetAng = w->subAng;
            w->targetAngAbs = w->subAngAbs;
            w->targetDist = em->x374;
            w->pTarget = pSUBS;
            w->flags |= 4;
        }
    }
}

// Never called (the original REL link dropped the body and kept the pool: 0, 1/256, 0.9, 0.05, 1.1, 1).
static void em2fScaleSet(cEm2f* em)
{
    f32 r = 0.0f;

    if (em->hp) {
        r = (f32) Rnd() * (1.0f / 256.0f);
    }
    if (r > 0.9f) {
        r = 0.9f;
    }
    em->scale.x = r * 0.05f + 1.1f;
    em->scale.y = 1.0f;
}

// Water splash on the parts `no`.
static inline void em2fWaterPower(cEm2f* em, int no)
{
    cModel* p = em->getPartsPtr(no);

    AddWaterPower(&p->worldPos, fRand0_1() * 0.3f + 0.3f);
}

void em2fWaterEffSet(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);

    if (w->waterY < em->pos.y + 2000.0f) {
        em2fWaterPower(em, 0);
        em2fWaterPower(em, 4);
        em2fWaterPower(em, 7);
        em2fWaterPower(em, 0x18);
        em2fWaterPower(em, 0x1B);
        if (w->effTimer2) {
            w->effTimer2--;
            if (w->effTimer2 == 0) {
                w->effTimer2 = 9;
                em2fEstSet(em, w, 3);
            }
        }
        if (w->effTimer3) {
            w->effTimer3--;
            if (w->effTimer3 == 0) {
                w->effTimer3 = 3;
                if (w->flags & 0x100) {
                    em2fEstSet(em, w, 0x14);
                } else {
                    em2fEstSet(em, w, 6);
                }
            }
        }
    } else {
        if (w->effTimer1) {
            w->effTimer1--;
            if (w->effTimer1 == 0) {
                w->effTimer1 = 2;
                em2fEstSet(em, w, 2);
            }
        }
    }
    if (pG->debug_mode == 7) {
        Draw_line3d(&w->nextPos, &em->pos, 0xFF808080, 0);
        Draw_sphere(&w->nextPos, 2000.0f, 0xFFFFFFFF, 1, 1);
    }
}

int em2fSetNextRoute(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    EmiData* emi = (EmiData*) pG->pRoomEmi;
    u32 i;
    int idx;

    if (emi == 0) {
        i = Rnd() & 3;  // the loop counter: its r3 copy preference puts i in r3
        w->nextPos = em2f_route_tbl[i];
        return Rnd() % 3;
    }
    idx = -1;
    for (i = 0; i < (u32) emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type != 2) {
            continue;
        }
        if (i > (u32) w->routeIdx) {
            w->routeIdx = i;
            w->nextPos = e->pos;
            return e->sub;
        }
        if ((u32) idx > i) {
            idx = i;
        }
    }
    // the returning then-arm is moved to the end by jump.c, so the idx arm is the fall-through
    if (idx == -1) {
        i = Rnd() & 3;
        w->nextPos = em2f_route_tbl[i];
        return Rnd() % 3;
    }
    {
        EmiEntry* e;

        IntSet(w->routeIdx, idx);  // reference store: the pG reload waits for it (idx frees r10)
        e = &((EmiData*) pG->pRoomEmi)->entry[idx];
        w->nextPos = e->pos;
        return e->sub;
    }
}

// Puts the monster right under the boat, facing the player.
void em2fSetPosBetweenBoat(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    Mtx m;
    Vec v;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if (!e->isAlive()) {
            continue;
        }
        if (e->id != 0xF) {
            continue;
        }
        em->rot.x = 0.0f;
        em->rot.y = pPLS->rot.y;
        em->rot.z = 0.0f;
        em->be_flag |= 2;
        em2fEffectDelete(em, w);
        PSMTXRotRad(m, 'y', em->rot.y);
        PosToPos(&e->pos, &pPL->pos, &v, 0.5f);
        v.y = w->waterY;
        TransMatrix(m, &v);
        v.x = -5000.0f;
        v.y = -5000.0f;
        v.z = -40000.0f;
        PSMTXMultVec(m, &v, &em->pos);
        break;
    }
}

// Puts the monster beside the boat for the ramming attack.
void em2fSetPosRisingD(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    Mtx m;
    Vec v;
    Vec a;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if (!e->isAlive()) {
            continue;
        }
        if (e->id != 0xF) {
            continue;
        }
        em2fEffectDelete(em, w);
        PSMTXRotRad(m, 'y', e->rot.y);
        TransMatrix(m, &e->pos);
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 500.0f;
        PSMTXMultVec(m, &a, &a);
        em->rot.y = e->rot.y - (PI / 2.0f) + (PI / 32.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        PSMTXRotRad(m, 'y', em->rot.y);
        v = a;
        v.y = w->waterY;
        TransMatrix(m, &v);
        v.x = 0.0f;
        v.y = -5000.0f;
        v.z = -133000.0f;
        PSMTXMultVec(m, &v, &em->pos);
        break;
    }
}

// Puts the monster beside the boat for the swallowing attack.
void em2fSetPosPackman(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    Mtx m;
    Vec v;
    Vec a;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if (!e->isAlive()) {
            continue;
        }
        if (e->id != 0xF) {
            continue;
        }
        em2fEffectDelete(em, w);
        PSMTXRotRad(m, 'y', e->rot.y);
        TransMatrix(m, &e->pos);
        a.x = 2000.0f;
        a.y = 0.0f;
        a.z = 0.0f;
        PSMTXMultVec(m, &a, &a);
        em->rot.y = e->rot.y - (PI / 2.0f) + (PI / 16.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        PSMTXRotRad(m, 'y', em->rot.y);
        v = a;
        v.y = w->waterY;
        TransMatrix(m, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -133000.0f;
        PSMTXMultVec(m, &v, &em->pos);
        break;
    }
}

// Puts the monster far away from the player, facing him, for the hiding phase.
void em2fSetPosHideMode(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    Mtx m;
    Vec v;
    f32 ang;

    if (Rnd() & 1) {
        ang = 2.45f;
    } else {
        ang = -0.79f;
    }
    PSMTXRotRad(m, 'y', fRand1_1() * (PI / 64.0f) + ang);
    TransMatrix(m, &pPL->pos);
    v.x = 0.0f;
    v.z = 100000.0f;
    v.y = 0.0f;
    PSMTXMultVec(m, &v, &em->pos);
    em->pos.y = w->waterY - 37894.84f - 4882.0f;
    em->rot.y = GetXZAngle(&em->pos, &pPLS->pos);
    em->rot.y += PI;
    em->rot.y = LIMIT_ANGLE(em->rot.y);
    PSMTXRotRad(m, 'y', em->rot.y);
    TransMatrix(m, &em->pos);
    v.x = -10000.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(m, &v, &em->pos);
    em2fEffectDelete(em, w);
}

void em2fSetPosDie(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);

    em->pos.x = 40120.0f;
    em->pos.y = w->waterY;
    em->pos.z = 38050.0f;
    em->rot.x = 0.0f;
    em->rot.y = -0.2951f;
    em->rot.z = 0.0f;
}

int em2fRisingDragonCk(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    int one;

    one = 1;
    if (pG->flags_5010 & 0x00400000) {
        if (w->rndFlag) {
            if (Rnd() & 3) {
                w->rndFlag = 0;
            }
            EmRoutineSet(em, one, 7, 0, 0);
        } else {
            if (Rnd() & 3) {
                w->rndFlag = one;
            }
            EmRoutineSet(em, one, 8, 0, 0);
        }
        return 1;
    }
    return 0;
}

void em2fChangeRoute(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    EmiData* emi = (EmiData*) pG->pRoomEmi;
    int best;
    int bestPrev;
    f32 bestAng;
    int i;
    EmiEntry* e;     // function scope: shared by the search loop and the tail (e r30, prev r3)
    EmiEntry* prev;

    if (emi == 0) {
        i = Rnd() & 3;
        w->nextPos = em2f_route_tbl[i];
        return;
    }
    best = -1;
    bestPrev = -1;
    bestAng = PI;
    for (i = 0; i < ((EmiData*) pG->pRoomEmi)->n; i++) {
        int j;
        f32 ang;

        e = &((EmiData*) pG->pRoomEmi)->entry[i];

        if (e->type != 2) {
            continue;
        }
        ang = fabsf(Muku(&em->pos, &e->pos, em->rot.y, PI));
        if (ang > bestAng) {
            continue;
        }
        if (i == 0) {
            j = ((EmiData*) pG->pRoomEmi)->n - 1;
        } else {
            j = i - 1;
        }
        prev = 0;
        for (; j > 0; j--) {
            prev = &((EmiData*) pG->pRoomEmi)->entry[j];
            if (prev->type == 2) {
                break;
            }
        }
        if (j < 0) {
            continue;
        }
        if (prev == 0) {
            continue;
        }
        if (fabsf(Muku2(GetXZAngle(&prev->pos, &e->pos), em->rot.y, PI)) > PI / 4.0f) {
            continue;
        }
        bestAng = ang;
        best = i;
        bestPrev = j;
    }
    if (best == -1) {
        i = Rnd() & 3;
        w->nextPos = em2f_route_tbl[i];
        return;
    }
    IntSet(w->routeIdx, best);  // reference store: the pG reload stays below it
    e = &((EmiData*) pG->pRoomEmi)->entry[best];
    prev = &((EmiData*) pG->pRoomEmi)->entry[bestPrev];
    w->nextPos = e->pos;
    w->routeType = prev->sub;
    w->nextRouteType = e->sub;
}

// Breaks the floating islands the head or the tail runs into.
void em2fIslandCrashCk(cEm2f* em)
{
    u32 i;

    for (i = 0; i < ObjMgr.nArray; i++) {
        cObj1c* o = (cObj1c*) ((u8*) ObjMgr.pArray + ObjMgr.size * i);
        cModel* p;
        f32 r;
        f32 d;

        if (!o->isAlive()) {
            continue;
        }
        if (o->id != 0x1C) {
            continue;
        }
        if (o->ckCrash()) {
            continue;
        }
        r = o->scale.x * 1800.0f;
        d = (em->pos.x - o->pos.x) * (em->pos.x - o->pos.x) + (em->pos.z - o->pos.z) * (em->pos.z - o->pos.z);
        if (d > 1600000000.0f) {
            continue;
        }
        p = em->getPartsPtr(8);
        r = r * r;
        d = (p->worldPos.x - o->pos.x) * (p->worldPos.x - o->pos.x) + (p->worldPos.y - o->pos.y) * (p->worldPos.y - o->pos.y) +
            (p->worldPos.z - o->pos.z) * (p->worldPos.z - o->pos.z);
        if (d < r) {
            o->setCrashBig(&p->worldPos);
            continue;
        }
        p = em->getPartsPtr(3);
        d = (p->worldPos.x - o->pos.x) * (p->worldPos.x - o->pos.x) + (p->worldPos.y - o->pos.y) * (p->worldPos.y - o->pos.y) +
            (p->worldPos.z - o->pos.z) * (p->worldPos.z - o->pos.z);
        if (d < r) {
            o->setCrashBig(&p->worldPos);
            continue;
        }
        p = em->getPartsPtr(0x19);
        d = (p->worldPos.x - o->pos.x) * (p->worldPos.x - o->pos.x) + (p->worldPos.y - o->pos.y) * (p->worldPos.y - o->pos.y) +
            (p->worldPos.z - o->pos.z) * (p->worldPos.z - o->pos.z);
        if (d < r) {
            o->setCrashBig(&p->worldPos);
        }
    }
}

void em2fTentacleMove(cEm2f* em)
{
    Em2fWork* w = EM2F_WK(em);
    u16 step = (*(u16*) ARC(0x31) & 0x3FFF) / 6;
    u32 i;

    if ((em->seFlags28B & 0x20) && (w->flags & 0x80)) {
        int frame = 0;

        for (i = 0; i < 6; i++, frame += step) {
            int parts;
            Vec pos;
            Vec rot;

            if (w->pTentacle[i]) {
                continue;
            }
            switch (i) {
            case 0:
            default:
                parts = 0x1D;
                break;
            case 1:
                parts = 0x1E;
                break;
            case 2:
                parts = 0x1F;
                break;
            case 3:
                parts = 0x20;
                break;
            case 4:
                parts = 0x21;
                break;
            case 5:
                parts = 0x22;
                break;
            }
            pos.x = 0.0f;
            pos.y = 0.0f;
            pos.z = 0.0f;
            rot.x = PI / 2.0f;
            rot.y = 0.0f;
            rot.z = fRand1_1() * PI;
            w->pTentacle[i] = (cObj16*) SetObj16(ARC(0x2F), ARC(0x30), em, em, parts, 0xA, &pos, &rot);
            if (w->pTentacle[i]) {
                MotSetObj16(w->pTentacle[i], ARC(0x31), 4, frame);
            }
        }
        if (w->seTimer4) {
            w->seTimer4--;
        } else {
            w->seTimer4 = 45;
            SndCall(8, 0x12, &em->pos, em->id, 0, em);
        }
    } else {
        for (i = 0; i < 6; i++) {
            if (w->pTentacle[i]) {
                w->pTentacle[i]->clearLostWait();
                w->pTentacle[i] = 0;
            }
        }
        w->seTimer4 = 0;
    }
}
