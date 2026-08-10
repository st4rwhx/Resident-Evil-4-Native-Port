// em35 module (D:/Bio4/Prog/em35.cpp): the boss of the beam hall. cModel::type 0 is the whole enemy that
// walks the floor (em35_R1_Walk / Atk / BearHug / Hook / Catch ...), type 1 the upper body that climbs the
// room's beam graph after the divide (em35_R1_U_*, em35Beam*Ck) and type 2 the divided legs.

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "em35.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "obj00.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "quake.h"
#include "motion.h"
#include "route_ck.h"
#include "act_btn.h"
#include "item.h"
#include "game.h"
#include "snd.h"
#include "pad.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pendulum.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "eprintf.h"
#include "db_log.h"

asm(".comm common_em35,52,4");

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// em_set.h declares EmSetDieCnt without arguments; this module passes the enemy.
void EmSetDieCntE(cEm* em) asm("EmSetDieCnt");
// game/em_dm_val.cpp (declared in em10.h, not included here).
int GetWepDmVal(cEm* em, u32 a, int b);

typedef void (*Em35Func)(cEm35*);

static void em35_R0_Init(cEm35* em);
static void em35_R0_Move(cEm35* em);
static void em35_R1_br_dummy(cEm35* em);
static void em35_R1_Divide(cEm35* em);
static void em35_R1_U_Divide(cEm35* em);
static void em35_R1_Wait(cEm35* em);
static void em35_R1_Walk(cEm35* em);
static void em35_R1_BigStep(cEm35* em);
static void em35_R1_Turn(cEm35* em);
static void em35_R1_Atk(cEm35* em);
static void em35_R1_br_AtkDouble(cEm35* em);
static void em35_R1_AtkDouble(cEm35* em);
static void em35_R1_BearHug(cEm35* em);
static void plem35_BearHug(cPlayer* pl);
static void em35AtkEscapeAction(cEm35* em);
static void plem35Sit(cPlayer* pl);
static void em35_R1_Atk2F(cEm35* em);
static void plem35DmFall2F(cPlayer* pl);
static void em35_R1_LongAtk(cEm35* em);
static void em35_R1_Hook(cEm35* em);
static void plem35DmHook(cPlayer* pl);
static void em35_R1_br_Critical(cEm35* em);
static void em35_R1_Critical(cEm35* em);
static void em35_R1_CriticalHit(cEm35* em);
static void plem35_CriticalHit(cPlayer* pl);
static void em35DashEscapeAction(cEm35* em);
static void plem35DashEscape(cPlayer* pl);
static void plem35DmStamp(cPlayer* pl);
static void em35_R1_br_Catch(cEm35* em);
static void em35_R1_Catch(cEm35* em);
static void em35_R1_CatchHit(cEm35* em);
static void plem35_CatchHit(cPlayer* pl);
static void em35_R1_U_Wait(cEm35* em);
static void em35_R1_U_Jump(cEm35* em);
static void em35_R1_U_JumpUp(cEm35* em);
static void em35_R1_U_JumpDown(cEm35* em);
static void em35_R1_U_DoubleJump(cEm35* em);
static void em35_R1_U_BackJump(cEm35* em);
static void em35_R1_U_Turn180(cEm35* em);
static void em35_R1_U_Step(cEm35* em);
static void em35_R1_U_BigStep(cEm35* em);
static void em35_R1_U_OverStep(cEm35* em);
static void em35_R1_U_StepUp(cEm35* em);
static void em35_R1_U_StepDown(cEm35* em);
static void em35_R1_U_HandAtk(cEm35* em);
static void em35_R1_U_Atk(cEm35* em);
static void em35_R1_U_Upper(cEm35* em);
static void em35_R1_U_AtkSpear(cEm35* em);
static void em35_R1_U_Crawl(cEm35* em);
static void em35_R1_U_CrawlTurn(cEm35* em);
static void em35_R1_U_JumpToBeam(cEm35* em);
static void em35_R0_Damage(cEm35* em);
static void em35_R1_Dm_Small(cEm35* em);
static void em35_R1_Dm_Spinal(cEm35* em);
static void em35_R1_Dm_Big(cEm35* em);
static void em35_R1_Dm_Frame(cEm35* em);
static void em35_R1_Dm_U_Fall(cEm35* em);
static void em35_R1_Dm_U_Crawl(cEm35* em);
static void em35_R0_Die(cEm35* em);
static void em35_R1_Die_Normal(cEm35* em);
static void em35_R1_Die_Pose(cEm35* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)
#define PL_ARC(no) PL_ARC_PTR(pl->subArc, no)

// The enemy a player damage callback belongs to (pl_sub SetPlDamage's first argument).
#define PL_EM(pl) ((cEm35*) (pl)->dmgType)
// The same through the global player pointer (the catch callbacks read it that way).
#define PL_EM_G ((cEm35*) pPL->dmgType)

#define VIB_TBL ((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc))

// Struct-member view of the player pointer: a load through it is not hoisted above the preceding
// stores through the work pointer (cam_ctrl.cpp PlayerPtr).
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

static inline void U32Set(u32& d, u32 v) { d = v; }

// Flag update through a volatile view: keeps the following global load (pPL) below the sth (wep_mod.h).
static inline void AtariFlagsOr(cAtariInfo* at, u16 mask) { *(volatile u16*) &at->flags |= mask; }

// Dead flag test (cDmgInfo upper 16 bits): an inline returning 0/1 gives the `li 1; andis.; bne; li 0` chain.
static inline int em35DeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

extern "C" void _prolog()
{
    OSReport("em35 prolog Ok\n");
    EmInitFunc = Em35Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em35Init(cEm* em)
{
    new (em) cEm35();
}

void em35DmCk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    Camera* cam = &pG->Cam;
    int near;
    int dmg;
    cModel* p;
    f32 d;

    if (em->hp > 0 && em35DeadCk(em) == 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 4:
        case 5:
        case 7:
            if (w->dieTimer == 0) {
                w->dieTimer = 120;
                if (w->flags & 8) {
                    return;
                }
                em->xFC = 2;
                em->xFD = 3;
                em->xFE = 0;
                em->xFF = 0;
                w->weakDmg = 0;
                w->dmgCnt = 0;
                return;
            }
            break;
        }
    }
    if (em->dmHit == 0) {
        return;
    }
    em->dmHit = 0;
    em->dmType = 1;
    if (em->dmWep == 0x10) {
        em->dmType = 0x11;
    }
    near = 0;
    if (em->dmPart->rad < 36000000.0f) {
        near = 1;
    }
    dmg = em35SetDmVal(em);
    LifeDownSet2(em, dmg, 0, 0);
    p = em->getPartsPtr(0);
    d = (cam->param.pos.x - p->worldPos.x) * (cam->param.pos.x - p->worldPos.x) +
        (cam->param.pos.y - p->worldPos.y) * (cam->param.pos.y - p->worldPos.y) +
        (cam->param.pos.z - p->worldPos.z) * (cam->param.pos.z - p->worldPos.z);
    switch (em->dmWep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xE:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x26:
    case 0x2B:
        EmDmBloodSet2(em, 0x2C, 0, 0, 0, 0);
        SndCall(8, 4, &em->pos, em->id, 0, em);
        break;
    case 0x10:
        EmDmBloodSet2(em, 0x2C, 0x17, 0, 0, 0);
        SndCall(8, 4, &em->pos, em->id, 0, em);
        break;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27: {
        Vec pos;
        Vec dir;

        EmDmBloodSet2(em, 0x2C, 0xA, 0, 0, 0);
        SndCall(8, 4, &em->pos, em->id, 0, em);
        if ((Rnd() & 3) == 0) {
            if (EmGetDmPos(em, &pos, &dir)) {
                EstSet(0, -1, &pos, 0, 0x2C, 0xB, 0, 0, 0, 0);
            }
        }
        break;
    }
    case 7:
    case 8:
    case 0x21:
        if (near) {
            if (d < 16000000.0f) {
                EmDmBloodSet2(em, 0x2C, 2, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x2C, 1, 0, 0, 0);
            }
            SndCall(8, 6, &em->pos, em->id, 0, em);
        } else {
            EmDmBloodSet2(em, 0x2C, 0, 0, 0, 0);
            SndCall(8, 4, &em->pos, em->id, 0, em);
        }
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        if (d < 16000000.0f) {
            EmDmBloodSet2(em, 0x2C, 2, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x2C, 1, 0, 0, 0);
        }
        SndCall(8, 6, &em->pos, em->id, 0, em);
        break;
    case 0x17:
    case 0x2A:
        if (w->flags & 8) {
            return;
        }
        em->xFF = 0;
        em->xFC = 2;
        em->xFE = 0;
        em->xFD = 2;
        w->weakDmg = 0;
        w->dmgCnt = 0;
        return;
    }
    if (em->hp <= 0) {
        EmSetDie(em);
        EmRoutineSet(em, 3, 0, 0, 0);
        return;
    }
    w->dmgCnt += dmg;
    if (em35WeakDmCk(em)) {
        w->weakDmg += dmg;
        if (w->weakDmg > 400) {
            w->weakDmg = 0;
            w->dmgCnt = 0;
            em->xFC = 2;
            em->xFD = 1;
            em->xFE = 0;
            em->xFF = 0;
            return;
        }
    }
    switch (em->dmWep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0xE:
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
        if (w->flags & 8) {
            return;
        }
        if (w->dmgCnt > 400) {
            if (w->flags & 0x80) {
                return;
            }
            w->dmgCnt = 0;
            em->xFC = 2;
            em->xFD = 0;
            em->xFE = 0;
            em->xFF = 0;
        }
        return;
    case 7:
    case 8:
    case 0x21:
        if (w->flags & 8) {
            return;
        }
        if (near) {
            if ((u8) (Rnd() % 3) == 0) {
                if (!(w->flags & 0x80)) {
                    if (Rnd() & 7) {
                        EmRoutineSet(em, 2, 0, 0, 0);
                    } else {
                        EmRoutineSet(em, 2, 2, 0, 0);
                    }
                    w->dmgCnt = 0;
                    return;
                }
            }
        }
        if (w->dmgCnt > 400) {
            w->dmgCnt = 0;
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        return;
    case 5:
    case 6:
    case 0xF:
    case 0x2C:
        if (Rnd() & 3) {
            EmRoutineSet(em, 2, 0, 0, 0);
        } else {
            EmRoutineSet(em, 2, 2, 0, 0);
        }
        w->weakDmg = 0;
        w->dmgCnt = 0;
        return;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
    default:
        em->xFC = 2;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
        break;
    }
    w->weakDmg = 0;
    w->dmgCnt = 0;
}

void em35DmCkUpper(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    Camera* cam = &pG->Cam;
    int near;
    int dmg;
    cModel* p;
    f32 d;

    if (em->dmHit == 0) {
        return;
    }
    em->dmHit = 0;
    em->dmType = 1;
    if (em->dmWep == 0x10) {
        em->dmType = 0x11;
    }
    near = 0;
    if (em->dmPart->rad < 16000000.0f) {
        near = 1;
    }
    dmg = em35SetDmVal(em);
    LifeDownSet2(em, dmg, 0, 0);
    w->dmgCnt += dmg;
    p = em->getPartsPtr(0);
    d = (cam->param.pos.x - p->worldPos.x) * (cam->param.pos.x - p->worldPos.x) +
        (cam->param.pos.y - p->worldPos.y) * (cam->param.pos.y - p->worldPos.y) +
        (cam->param.pos.z - p->worldPos.z) * (cam->param.pos.z - p->worldPos.z);
    switch (em->dmWep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0xE:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        EmDmBloodSet2(em, 0x2C, 0, 0, 0, 0);
        SndCall(8, 4, &em->pos, em->id, 0, em);
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            if (d < 16000000.0f) {
                EmDmBloodSet2(em, 0x2C, 2, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x2C, 1, 0, 0, 0);
            }
            SndCall(8, 4, &em->pos, em->id, 0, em);
        } else {
            EmDmBloodSet2(em, 0x2C, 0, 0, 0, 0);
            SndCall(8, 4, &em->pos, em->id, 0, em);
        }
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
    default:
        if (d < 16000000.0f) {
            EmDmBloodSet2(em, 0x2C, 2, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x2C, 1, 0, 0, 0);
        }
        SndCall(8, 4, &em->pos, em->id, 0, em);
        break;
    case 0x17:
    case 0x2A:
        break;
    }
    if (em->hp <= 0) {
        EmSetDie(em);
        EmSetDieCntE(em);
        EmRoutineSet(em, 2, 4, 0, 0);
        return;
    }
    if (w->flags & 0x20) {
        switch (em->dmWep) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 0xB:
        case 0xC:
        case 0xE:
        case 0xF:
        case 0x10:
        case 0x11:
        case 0x14:
        case 0x15:
        case 0x1B:
        case 0x1D:
        case 0x26:
        case 0x27:
        case 0x2B:
        case 0x2C:
            return;
        case 7:
        case 8:
        case 0x21:
            if (near == 0) {
                return;
            }
            if ((u8) (Rnd() % 3) != 0) {
                return;
            }
            EmRoutineSet(em, 2, 5, 0, 0);
            return;
        case 9:
        case 0xA:
        case 0x28:
            if ((u8) (Rnd() % 3) != 0) {
                return;
            }
            EmRoutineSet(em, 2, 5, 0, 0);
            return;
        case 0xD:
        case 0x12:
        case 0x13:
        case 0x29:
        case 0x2D:
        default:
            EmRoutineSet(em, 2, 5, 0, 0);
            return;
        }
    }
    switch (em->dmWep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0xE:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        if (w->dmgCnt > 400) {
            w->dmgCnt = 0;
            EmRoutineSet(em, 2, 4, 0, 0);
        }
        return;
    case 7:
    case 8:
    case 0x21:
        if (w->dmgCnt > 400) {
            w->dmgCnt = 0;
            EmRoutineSet(em, 2, 4, 0, 0);
            return;
        }
        if (near == 0) {
            return;
        }
        if ((Rnd() & 3) == 0) {
            EmRoutineSet(em, 2, 4, 0, 0);
        }
        return;
    case 9:
    case 0xA:
    case 0x28:
        if (w->dmgCnt > 400) {
            w->dmgCnt = 0;
            EmRoutineSet(em, 2, 4, 0, 0);
            return;
        }
        if ((Rnd() & 3) == 0) {
            EmRoutineSet(em, 2, 4, 0, 0);
        }
        return;
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
        EmRoutineSet(em, 2, 4, 0, 0);
        return;
    }
}

Em35Func Em35_R0_move_tbl[4] = {
    em35_R0_Init,
    em35_R0_Move,
    em35_R0_Damage,
    em35_R0_Die,
};

// Pairs of {branch check, routine} per routine-1 state (em35_R0_Move calls both).
static Em35Func Em35_R1_move_tbl[70] = {
    em35_R1_br_dummy, em35_R1_Wait,
    em35_R1_br_dummy, em35_R1_Walk,
    em35_R1_br_dummy, em35_R1_BigStep,
    em35_R1_br_dummy, em35_R1_Turn,
    em35_R1_br_dummy, em35_R1_Atk,
    em35_R1_br_dummy, em35_R1_LongAtk,
    em35_R1_br_AtkDouble, em35_R1_AtkDouble,
    em35_R1_br_dummy, em35_R1_BearHug,
    em35_R1_br_dummy, em35_R1_Hook,
    em35_R1_br_Critical, em35_R1_Critical,
    em35_R1_br_dummy, em35_R1_CriticalHit,
    em35_R1_br_dummy, em35_R1_Atk2F,
    em35_R1_br_Catch, em35_R1_Catch,
    em35_R1_br_dummy, em35_R1_CatchHit,
    em35_R1_br_dummy, em35_R1_Divide,
    em35_R1_br_dummy, em35_R1_U_Wait,
    em35_R1_br_dummy, em35_R1_U_Jump,
    em35_R1_br_dummy, em35_R1_U_JumpUp,
    em35_R1_br_dummy, em35_R1_U_JumpDown,
    em35_R1_br_dummy, em35_R1_U_DoubleJump,
    em35_R1_br_dummy, em35_R1_U_BackJump,
    em35_R1_br_dummy, em35_R1_U_Turn180,
    em35_R1_br_dummy, em35_R1_U_Step,
    em35_R1_br_dummy, em35_R1_U_BigStep,
    em35_R1_br_dummy, em35_R1_U_OverStep,
    em35_R1_br_dummy, em35_R1_U_StepUp,
    em35_R1_br_dummy, em35_R1_U_StepDown,
    em35_R1_br_dummy, em35_R1_U_HandAtk,
    em35_R1_br_dummy, em35_R1_U_Atk,
    em35_R1_br_dummy, em35_R1_U_Upper,
    em35_R1_br_dummy, em35_R1_U_AtkSpear,
    em35_R1_br_dummy, em35_R1_U_Divide,
    em35_R1_br_dummy, em35_R1_U_Crawl,
    em35_R1_br_dummy, em35_R1_U_CrawlTurn,
    em35_R1_br_dummy, em35_R1_U_JumpToBeam,
};

static Em35Func Em35_R2_move_tbl[6] = {
    em35_R1_Dm_Small,
    em35_R1_Dm_Spinal,
    em35_R1_Dm_Big,
    em35_R1_Dm_Frame,
    em35_R1_Dm_U_Fall,
    em35_R1_Dm_U_Crawl,
};

static Em35Func Em35_R3_move_tbl[2] = {
    em35_R1_Die_Normal,
    em35_R1_Die_Pose,
};

// The beam graph of the room: 17 beams with their links and end points.
static Em35Beam em35_beam_tbl[17] = {
    { 0, 0xFF, 0x0D, { 0xFF, 0xFF }, 0xFF, 0x01, { 0xFF, 0xFF, 0xFF, 0x09 }, { 39071.0f, -4000.0f, -53163.0f }, { 32050.0f, -4000.0f, -53163.0f } },
    { 0, 0xFF, 0x0E, { 0xFF, 0x04 }, 0x00, 0x02, { 0xFF, 0x0B, 0x09, 0xFF }, { 39071.0f, -4000.0f, -58159.0f }, { 32050.0f, -4000.0f, -58159.0f } },
    { 0, 0xFF, 0x0F, { 0x07, 0x05 }, 0x01, 0x03, { 0x0B, 0x0C, 0xFF, 0x0A }, { 39071.0f, -4000.0f, -63158.0f }, { 32050.0f, -4000.0f, -63158.0f } },
    { 0, 0xFF, 0xFF, { 0x08, 0x06 }, 0x02, 0xFF, { 0x0C, 0xFF, 0x0A, 0xFF }, { 39071.0f, -4000.0f, -68158.0f }, { 32050.0f, -4000.0f, -68158.0f } },
    { 0, 0xFF, 0xFF, { 0x01, 0xFF }, 0xFF, 0x05, { 0x09, 0xFF, 0xFF, 0xFF }, { 32050.0f, -4000.0f, -58159.0f }, { 29528.0f, -4000.0f, -58159.0f } },
    { 0, 0xFF, 0xFF, { 0xFF, 0x02 }, 0x04, 0x06, { 0xFF, 0x0A, 0xFF, 0xFF }, { 32050.0f, -4000.0f, -63158.0f }, { 29528.0f, -4000.0f, -63158.0f } },
    { 0, 0xFF, 0xFF, { 0x03, 0xFF }, 0x05, 0xFF, { 0x0A, 0xFF, 0xFF, 0xFF }, { 32050.0f, -4000.0f, -68158.0f }, { 29528.0f, -4000.0f, -68158.0f } },
    { 0, 0xFF, 0xFF, { 0xFF, 0x02 }, 0xFF, 0x08, { 0xFF, 0xFF, 0x0B, 0x0C }, { 41589.0f, -4000.0f, -63158.0f }, { 39071.0f, -4000.0f, -63158.0f } },
    { 0, 0xFF, 0xFF, { 0xFF, 0x03 }, 0x07, 0xFF, { 0xFF, 0xFF, 0x0C, 0xFF }, { 41489.0f, -4000.0f, -68158.0f }, { 39071.0f, -4000.0f, -68158.0f } },
    { 1, 0xFF, 0x10, { 0xFF, 0xFF }, 0xFF, 0xFF, { 0x00, 0x01, 0xFF, 0xFF }, { 32050.0f, -4000.0f, -53163.0f }, { 32050.0f, -4000.0f, -58159.0f } },
    { 1, 0xFF, 0xFF, { 0xFF, 0xFF }, 0xFF, 0xFF, { 0x02, 0x03, 0x05, 0x06 }, { 32050.0f, -4000.0f, -63158.0f }, { 32050.0f, -4000.0f, -68158.0f } },
    { 1, 0xFF, 0xFF, { 0xFF, 0xFF }, 0xFF, 0xFF, { 0xFF, 0xFF, 0x01, 0x02 }, { 39071.0f, -4000.0f, -58159.0f }, { 39071.0f, -4000.0f, -63158.0f } },
    { 1, 0xFF, 0xFF, { 0xFF, 0xFF }, 0xFF, 0xFF, { 0xFF, 0xFF, 0x02, 0x03 }, { 39071.0f, -4000.0f, -63158.0f }, { 39071.0f, -4000.0f, -68158.0f } },
    { 0, 0x00, 0xFF, { 0xFF, 0xFF }, 0xFF, 0x0E, { 0xFF, 0xFF, 0xFF, 0x10 }, { 39071.0f, -8000.0f, -53163.0f }, { 32050.0f, -8000.0f, -53163.0f } },
    { 0, 0x01, 0xFF, { 0xFF, 0xFF }, 0x0D, 0x0F, { 0xFF, 0xFF, 0x10, 0xFF }, { 39071.0f, -8000.0f, -58159.0f }, { 32050.0f, -8000.0f, -58159.0f } },
    { 0, 0x02, 0xFF, { 0xFF, 0xFF }, 0x0E, 0xFF, { 0xFF, 0xFF, 0xFF, 0xFF }, { 39071.0f, -8000.0f, -63158.0f }, { 32050.0f, -8000.0f, -63158.0f } },
    { 1, 0x09, 0xFF, { 0xFF, 0xFF }, 0xFF, 0xFF, { 0x0D, 0x0E, 0xFF, 0xFF }, { 32050.0f, -8000.0f, -53163.0f }, { 32050.0f, -8000.0f, -58159.0f } },
};

// Positions the upper body crawls to (em35_R1_U_Crawl / Dm_U_Fall / Dm_U_Crawl).
static Vec em35_crawl_pos[5] = {
    { 35560.0f, -8000.0f, -53163.0f },
    { 36720.0f, -8000.0f, -58159.0f },
    { 36720.0f, -8000.0f, -63158.0f },
    { 30790.0f, -4000.0f, -68158.0f },
    { 40330.0f, -4000.0f, -68158.0f },
};

// Parts index remap of the flipped motions (cModel::motFlip) of the whole body / the upper body.
static u16 em35_flip0[120] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1A, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x1B, 0x20, 0x21, 0x22, 0x23,
    0x1C, 0x1D, 0x1E, 0x1F, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
    0x32, 0x33, 0x30, 0x31, 0x34, 0x37, 0x38, 0x35, 0x36, 0x3B, 0x3C, 0x39, 0x3A, 0x3F, 0x40, 0x3D,
    0x3E, 0x43, 0x44, 0x41, 0x42, 0x47, 0x48, 0x45, 0x46, 0x4C, 0x4D, 0x4E, 0x49, 0x4A, 0x4B, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
};

static u16 em35_flip1[120] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0A, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x21, 0x22,
    0x23, 0x1E, 0x15, 0x20, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
    0x32, 0x33, 0x30, 0x31, 0x34, 0x37, 0x38, 0x35, 0x36, 0x40, 0x41, 0x3D, 0x3E, 0x3F, 0x39, 0x3A,
    0x3B, 0x3C, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
};

// Attacks (em35AtkCk): [0] punch, [1] punch (left), [2] stamp, [3] double punch, [4] hook, [5]/[6] second
// floor punch, [7] critical, [8]/[9] upper body hand, [0xA]/[0xB] upper, [0xC] spear.
static EmAtkInfo em35_atk_tbl[13] = {
    { 400.0f, 8, 800, 0, 0xA, 0 },
    { 400.0f, 8, 800, 0, 0xA, 0 },
    { 400.0f, 8, 800, 0, 0xA, 0 },
    { 400.0f, 8, 0, 0, 0xA, 0 },
    { 500.0f, 8, 0, 0, 0xA, 0 },
    { 300.0f, 8, 800, 0, 0xA, 0 },
    { 300.0f, 8, 800, 0, 0xA, 0 },
    { 500.0f, 8, 9999, 0, 0xA, 0 },
    { 500.0f, 8, 800, 0, 0xA, 0 },
    { 500.0f, 8, 800, 0, 0xA, 0 },
    { 500.0f, 8, 800, 0, 0xA, 0 },
    { 500.0f, 8, 800, 0, 0xA, 0 },
    { 700.0f, 8, 800, 0, 0xA, 0 },
};

// Cloth chains (em35ClothSet*): the type 1 tail and the hanging skin of both types.
static u8 em35ClothP[6] = { 0x12, 0x13, 0x14, 0x15, 0x16, 0x17 };
static u8 em35ClothUp[6] = { 0xFF, 0x12, 0x13, 0x14, 0x15, 0x16 };
static u8 em35ClothDp[6] = { 0x13, 0x14, 0x15, 0x16, 0x17, 0xFF };
static f32 em35ClothMax[6] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
static u8 em35ClothP2[2] = { 0x19, 0x1A };
static u8 em35ClothUp2[2] = { 0xFF, 0x19 };
static u8 em35ClothDp2[2] = { 0x1A, 0xFF };
static f32 em35ClothMax2[2] = { 0.3f, 0.4f };
static f32 em35ClothRate2[2] = { 0.8f, 0.8f };
// em35ClothAt2/3 are the only globals among the cloth tables (REL ADDR16 field 0 = global symbol).
PlClothAt em35ClothAt2[5] = {
    { 0, 4, 4, 1.0f, 120.0f, { 0.0f, -100.0f, -30.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 2, 3, 0.3f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 5, 5, 1.0f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 0xB, 0xB, 1.0f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 2, 3, 0.7f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
};
static u8 em35ClothP3[2] = { 0x0B, 0x0C };
static u8 em35ClothUp3[2] = { 0xFF, 0x0B };
static u8 em35ClothDp3[2] = { 0x0C, 0xFF };
static f32 em35ClothMax3[2] = { 0.3f, 0.4f };
static f32 em35ClothRate3[2] = { 0.8f, 0.8f };
PlClothAt em35ClothAt3[5] = {
    { 0, 9, 9, 1.0f, 120.0f, { 0.0f, -100.0f, -30.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 7, 8, 0.3f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 0xF, 0xF, 1.0f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 0x15, 0x15, 1.0f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 7, 8, 0.7f, 130.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
};

void cEm35::move()
{
    Em35Work* w = EM35_WK(this);

    if (xFC) {
        switch (type) {
        case 0:
        default:
            em35DmCk(this);
            break;
        case 1:
            em35DmCkUpper(this);
            break;
        }
    }
    w->flags &= ~0xFF;
    if (w->atkWait) {
        w->atkWait--;
    }
    if (w->lockWait) {
        w->lockWait--;
    }
    if (w->dieTimer) {
        w->dieTimer--;
    }
    em35RouteCk(this);
    if (type == 1) {
        int no = em35GetBeamNo(&pos, 0);

        eprintf2(8, 12, 300, 400, 0, 0, "beam = %d", no);
        w->beamNo = no;
        if (no != 0xFF) {
            w->beamType = em35_beam_tbl[no].type;
        }
    }
    Em35_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em35NeckMove(this);
    em35ScaleMove(this);
    partsWorldCalc();
    em35ClothMove(this);
    em35ClothMove2(this);
    em35ClothMove3(this);
    be_flag &= ~0x00200000;
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    if (hp > 0) {
        if (type == 0) {
            if (w->seTimer) {
                w->seTimer--;
            } else {
                SndCall(8, 8, &pos, id, 0, this);
                w->seTimer = 60;
            }
        }
        if (hp > 0 && type == 1) {
            if (w->effTimer) {
                w->effTimer--;
            } else {
                cModel* p = getPartsPtr(0x15);

                w->effTimer = 14;
                EstSet(0, -1, &p->worldPos, 0, 0x2C, 5, 0, 0, 0, 0);
            }
        }
    }
    em35WeakMove(this);
}

static void em35_R0_Init(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    int zero;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(5)) == 0) {
            pLog->err(0, 0, "em35() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        w->pInfo = ModInfoMgr.create(ARC(6), ARC(5));
        if (w->pInfo) {
            em->addModel(w->pInfo);
        }
        break;
    case 1:
        if (em->modelInit(ARC(7), ARC(5)) == 0) {
            pLog->err(0, 0, "em35() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    case 2:
        if (em->modelInit(ARC(8), ARC(5)) == 0) {
            pLog->err(0, 0, "em35() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    }
    em35ClothSet(em);
    em35ClothSet2(em);
    em35ClothSet3(em);
    if (em->type == 1) {
        em->motFlip = em35_flip1;
    } else {
        em->motFlip = em35_flip0;
    }
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 5000.0f, 5000.0f, 5000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    atariInitF(&em->atari, 0.0f, 0.0f, 0.0f, 800.0f, 700.0f, 700.0f, 3000.0f, 1, 0x2000, 10);   // COMPILER-DIFF: #1
    em->litArea.on(1);
    switch (em->type) {
    case 0:
    default:
        YarareInit(em, 0.0f, 0.0f, 0.0f, 300.0f, 150.0f, 1, 1);
        YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 0xA, 1);
        YarareAdd(em, &w->hit[1], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 3, 1);
        YarareAdd(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 4, 1);
        YarareAdd(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 5, 1);
        YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 6, 1);
        YarareAdd(em, &w->hit[5], 0.0f, 0.0f, 0.0f, 300.0f, 300.0f, 7, 1);
        YarareAdd(em, &w->hit[6], -300.0f, 0.0f, 0.0f, 150.0f, 400.0f, 0x12, 3);
        YarareAdd(em, &w->hit[7], -300.0f, 0.0f, 0.0f, 170.0f, 300.0f, 0x13, 3);
        YarareAdd(em, &w->hit[8], 20.0f, -400.0f, 0.0f, 200.0f, 400.0f, 0x18, 1);
        YarareAdd(em, &w->hit[9], 0.0f, 0.0f, 0.0f, 170.0f, 300.0f, 0x19, 3);
        YarareAdd(em, &w->hit[10], -20.0f, -300.0f, 0.0f, 220.0f, 300.0f, 0x1D, 1);
        YarareAdd(em, &w->hit[11], -20.0f, -400.0f, 0.0f, 200.0f, 400.0f, 0x1E, 1);
        YarareAdd(em, &w->hit[12], 20.0f, -300.0f, 0.0f, 220.0f, 300.0f, 0x21, 1);
        YarareAdd(em, &w->hit[13], 20.0f, -400.0f, 0.0f, 200.0f, 400.0f, 0x22, 1);
        YarareAdd(em, &w->hit[14], 20.0f, 0.0f, 0.0f, 200.0f, 50.0f, 0x4B, 1);
        YarareAdd(em, &w->hit[15], 20.0f, 0.0f, 0.0f, 200.0f, 50.0f, 0x4E, 1);
        YarareAdd(em, &w->hit[16], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x2B, 1);
        YarareAdd(em, &w->hit[17], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x2C, 1);
        YarareAdd(em, &w->hit[18], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x2D, 1);
        YarareAdd(em, &w->hit[19], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x2E, 1);
        YarareAdd(em, &w->hit[20], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x2F, 1);
        YarareAdd(em, &w->hit[21], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x25, 1);
        YarareAdd(em, &w->hit[22], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x26, 1);
        YarareAdd(em, &w->hit[23], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x27, 1);
        YarareAdd(em, &w->hit[24], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x28, 1);
        YarareAdd(em, &w->hit[25], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x29, 1);
        break;
    case 1:
        YarareInit(em, 0.0f, 0.0f, 0.0f, 300.0f, 150.0f, 1, 1);
        YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 200.0f, 100.0f, 5, 1);
        YarareAdd(em, &w->hit[1], 0.0f, 0.0f, 0.0f, 200.0f, 150.0f, 0x12, 1);
        YarareAdd(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 200.0f, 150.0f, 0x13, 1);
        YarareAdd(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 200.0f, 150.0f, 0x14, 1);
        YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 200.0f, 150.0f, 0x15, 1);
        YarareAdd(em, &w->hit[5], 0.0f, 0.0f, 0.0f, 300.0f, 300.0f, 2, 1);
        YarareAdd(em, &w->hit[6], -300.0f, 0.0f, 0.0f, 150.0f, 400.0f, 8, 3);
        YarareAdd(em, &w->hit[7], -300.0f, 0.0f, 0.0f, 170.0f, 300.0f, 9, 3);
        YarareAdd(em, &w->hit[8], 20.0f, -400.0f, 0.0f, 200.0f, 400.0f, 0xE, 1);
        YarareAdd(em, &w->hit[9], 0.0f, 0.0f, 0.0f, 170.0f, 300.0f, 0xF, 3);
        YarareAdd(em, &w->hit[10], 20.0f, 0.0f, 0.0f, 200.0f, 50.0f, 0x20, 1);
        YarareAdd(em, &w->hit[11], 20.0f, 0.0f, 0.0f, 200.0f, 50.0f, 0x23, 1);
        YarareAdd(em, &w->hit[12], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x2B, 1);
        YarareAdd(em, &w->hit[13], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x2C, 1);
        YarareAdd(em, &w->hit[14], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x2D, 1);
        YarareAdd(em, &w->hit[15], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x2E, 1);
        YarareAdd(em, &w->hit[16], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x2F, 1);
        YarareAdd(em, &w->hit[17], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x25, 1);
        YarareAdd(em, &w->hit[18], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x26, 1);
        YarareAdd(em, &w->hit[19], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x27, 1);
        YarareAdd(em, &w->hit[20], 0.0f, 0.0f, 0.0f, 200.0f, 200.0f, 0x28, 1);
        YarareAdd(em, &w->hit[21], 0.0f, 0.0f, 0.0f, 200.0f, 400.0f, 0x29, 1);
        break;
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(9), 0x2C, 0);
    w->espKind = EspPullCoreKind();
    w->flags = zero;
    w->neckAng = 0.0f;
    w->scaleAng = 0.0f;
    w->atkWait = zero;
    w->weakDmg = zero;
    w->dmgCnt = zero;
    w->sndId = zero;
    w->lockWait = 300;
    w->seTimer = 60;
    em35WeakInit(em);
    em->setStatus(5);
    em->setStatus(9);
    switch (em->x38D) {
    case 0:
    default:
        switch (em->type) {
        case 0:
        default:
            EstSet((int) em, -1, 0, 0, 0x2C, 3, 1, w->espKind, (u32) em, (void*) zero);
            EstSet((int) em, -1, 0, 0, 0x2C, 8, 1, w->espKind, (u32) em, (void*) zero);
            EmRoutineSet(em, 1, 1, zero, zero);
            MotionSetCore(em, MOTION(em), ARC(0xA), 0, 0, 1, 0);
            MotionMoveF(em, 0);
            break;
        case 1:
            EstSet((int) em, -1, 0, 0, 0x2C, 4, 1, w->espKind, (u32) em, (void*) zero);
            EstSet((int) em, -1, 0, 0, 0x2C, 9, 1, w->espKind, (u32) em, (void*) zero);
            em->atari.throughOn();
            EmRoutineSet(em, 1, 0xF, zero, zero);
            MotionSetCore(em, MOTION(em), ARC(0x48), 0, 0, 1, 0);
            MotionMoveF(em, 0);
            break;
        }
        break;
    case 1:
        switch (em->type) {
        case 0:
        default:
            EmRoutineSet(em, 1, 0xE, zero, zero);
            em->clearStatus(9);
            MotionSetCore(em, MOTION(em), ARC(0x46), 0, 0, 1, 0);
            MotionMoveF(em, 0);
            break;
        case 1:
            EstSet((int) em, -1, 0, 0, 0x2C, 4, 1, w->espKind, (u32) em, (void*) zero);
            EstSet((int) em, -1, 0, 0, 0x2C, 9, 1, w->espKind, (u32) em, (void*) zero);
            em->atari.throughOn();
            EmRoutineSet(em, 1, 0x1F, zero, zero);
            MotionSetCore(em, MOTION(em), ARC(0x88), 0, 0, 1, 0);
            MotionMoveF(em, 0);
            break;
        }
        break;
    }
    em35_R0_Move(em);
}

static void em35_R0_Move(cEm35* em)
{
    Em35_R1_move_tbl[em->xFD * 2](em);
    Em35_R1_move_tbl[em->xFD * 2 + 1](em);
}

static void em35_R1_br_dummy(cEm35* em)
{
}

static void em35_R1_Divide(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0:
        if ((pG->room_id32 & 0xFFFF0000) == 0x011F0000) {
            Vec v;

            em->rot.y = PI;
            em->pos.x = 36500.0f;
            em->pos.y = -8000.0f;
            em->pos.z = -57970.0f;
            RotMatrix(em->mat, &em->rot);
            TransMatrix(em->mat, &em->pos);
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = -243.39f;
            PSMTXMultVec(em->mat, &v, &em->pos);
        }
        MotionSetCore(em, MOTION(em), ARC(0x46), (int) ARC(0x47), 0, 1, 0);
        em->hp = 0;
        em->clearStatus(5);
        em->atari.throughOn();
        w->timer = 0;
        EstSet((int) em, -1, 0, 0, 0x2C, 0x18, 1, 0, (u32) em, 0);
        em->xFE++;
    case 1:
        if (em->motEvent & 1) {
            if (w->timer) {
                w->timer--;
            } else {
                w->timer = 3;
                SndCall(8, 0xC, &em->pos, em->id, 0, em);
            }
        }
        if (MotionMoveF(em, 0)) {
            EffectEspDelete(1, w->espKind, (u32) em, 0);
            EffectEspgenDelete(1, w->espKind, (int) em);
            EffectEfmDelete(1, w->espKind, (int) em);
            em->xFE++;
        }
        break;
    }
}

static void em35_R1_U_Divide(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0:
        if ((pG->room_id32 & 0xFFFF0000) == 0x011F0000) {
            Vec v;

            em->rot.y = PI;
            em->pos.x = 36500.0f;
            em->pos.y = -8000.0f;
            em->pos.z = -57970.0f;
            RotMatrix(em->mat, &em->rot);
            TransMatrix(em->mat, &em->pos);
            v.x = 267.97f;
            v.y = 0.0f;
            v.z = -96.05f;
            PSMTXMultVec(em->mat, &v, &em->pos);
        }
        MotionSetCore(em, MOTION(em), ARC(0x88), 0, 0, 1, 0);
        em->xFE++;
    case 1:
        w->flags |= 0x40;
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        w->timer = 90;
        em->xFE++;
    case 3:
        w->flags |= 0x40;
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
        } else {
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x89), 0, 0, 1, 0);
        em->xFE++;
    case 5:
        w->flags |= 0x40;
        if (MotionMoveF(em, 0)) {
            w->flags &= ~0x40;
            em->be_flag |= 0x00200000;
            EstSet((int) em, -1, 0, 0, 0x2C, 4, 1, w->espKind, (u32) em, 0);
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

static void em35_R1_Wait(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0: {
        int flip = 5;

        if (em->motFlags & 0x40) {
            flip = 0x45;
        }
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 30, flip, 0);
        em->xFE++;
    }
    case 1:
        MotionMoveF(em, 0);
        if (em35DeadCk(em)) {
            if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if (em35BigStepCk(em) == 0) {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else {
            if (em->plDist2 > 25000000.0f) {
                w->atkWait = 0;
            }
            if (w->atkWait == 0) {
                if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                    EmRoutineSet(em, 1, 4, 0, 0);
                } else if (w->targetAngAbs > 1.0471976f) {
                    EmRoutineSet(em, 1, 3, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 1, 0, 0);
                }
            }
        }
        break;
    }
}

static void em35_R1_Walk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0: {
        int flip = 5;

        if (em->motFlags & 0x40) {
            flip = 0x45;
        }
        if (em->plDist2 > 49000000.0f) {
            switch (em->xFF) {
            case 0:
            default:
                MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0xE), 10, flip, 0);
                break;
            case 1:
                MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0xE), 10, flip, 0);
                break;
            case 2:
                MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0xE), 10, flip, 0x4C);
                break;
            case 3:
                MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0xE), 10, flip, 0);
                break;
            }
        } else {
            switch (em->xFF) {
            case 0:
            default:
                MotionSetCore(em, MOTION(em), ARC(0xB), (int) ARC(0xC), 10, flip, 0);
                break;
            case 1:
                MotionSetCore(em, MOTION(em), ARC(0xB), (int) ARC(0xC), 10, flip, 0);
                break;
            case 2:
                MotionSetCore(em, MOTION(em), ARC(0xB), (int) ARC(0xC), 10, flip, 0x5D);
                break;
            case 3:
                MotionSetCore(em, MOTION(em), ARC(0xB), (int) ARC(0xC), 10, flip, 0);
                break;
            }
        }
        w->walkType = (u8) (Rnd() % 3);
        em->xFE++;
    }
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.049087387f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if (em->pos.y + 2000.0f < pPL->pos.y) {
            if (em->plDist2 < 16000000.0f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else {
                goto big_step;
            }
        } else if (w->routeAngAbs < 0.5235988f && em->plDist2 > 4000000.0f && em->plDist2 < 12250000.0f &&
                   w->lockWait == 0) {
            if ((s16) pG->pl_life <= 900 && (Rnd() & 1)) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                switch ((u8) (Rnd() % 3)) {
                case 0:
                    EmRoutineSet(em, 1, 8, 0, 0);
                    break;
                case 1:
                    EmRoutineSet(em, 1, 5, 0, 0);
                    break;
                case 2:
                    EmRoutineSet(em, 1, 6, 0, 0);
                    break;
                }
            }
        } else if (w->walkType) {
            if (em->plDist2 < 2250000.0f && w->routeAngAbs < 0.7853982f) {
                EmRoutineSet(em, 1, 0xC, 0, 1);
            } else if (em35bPlRunCk(em) && em->plDist2 < 12250000.0f && w->routeAngAbs < 0.7853982f) {
                EmRoutineSet(em, 1, 0xC, 0, 0);
            } else {
                goto turn_ck;
            }
        } else {
            if (w->routeAngAbs < 1.0471976f &&
                (em->plDist2 < 6250000.0f || (em35bPlRunCk(em) && em->plDist2 < 12250000.0f))) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
            turn_ck:
                if (w->routeAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                    EmRoutineSet(em, 1, 4, 0, 0);
                } else {
                big_step:
                    em35BigStepCk(em);
                }
            }
        }
        break;
    }
}

static void em35_R1_BigStep(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0: {
        int flip = 5;

        if (em->motFlags & 0x40) {
            flip = 0x45;
        }
        if (w->targetDist > 81000000.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x42), (int) ARC(0x43), 10, flip, 0);
            em->xFF = 1;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x44), (int) ARC(0x45), 10, flip, 0);
            em->xFF = 0;
        }
        w->walkType = (u8) (Rnd() % 3);
        em->xFE++;
    }
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else {
            if (em->motEvent & 4) {
                if (w->routeAngAbs < 0.5235988f && em->plDist2 > 4000000.0f && em->plDist2 < 12250000.0f &&
                    w->lockWait == 0) {
                    if ((s16) pG->pl_life <= 900 && (Rnd() & 1)) {
                        EmRoutineSet(em, 1, 9, 0, 0);
                    } else {
                        switch ((u8) (Rnd() % 3)) {
                        case 0:
                            EmRoutineSet(em, 1, 8, 0, 0);
                            break;
                        case 1:
                            EmRoutineSet(em, 1, 5, 0, 0);
                            break;
                        case 2:
                            EmRoutineSet(em, 1, 6, 0, 0);
                            break;
                        }
                    }
                } else if (w->walkType) {
                    if (em->plDist2 < 2250000.0f && w->routeAngAbs < 0.7853982f) {
                        EmRoutineSet(em, 1, 0xC, 0, 1);
                    } else if (em35bPlRunCk(em) && em->plDist2 < 12250000.0f && w->routeAngAbs < 0.7853982f) {
                        EmRoutineSet(em, 1, 0xC, 0, 0);
                    } else {
                        goto turn_ck;
                    }
                } else {
                    if (w->routeAngAbs < 1.0471976f &&
                        (em->plDist2 < 6250000.0f || (em35bPlRunCk(em) && em->plDist2 < 12250000.0f))) {
                        EmRoutineSet(em, 1, 4, 0, 0);
                    } else {
                    turn_ck:
                        if (w->routeAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                            EmRoutineSet(em, 1, 4, 0, 0);
                        } else {
                            goto se;
                        }
                    }
                }
            } else {
            se:
                if (em->motEvent & 1) {
                    EstSet((int) em, -1, 0, 0, 0x2C, 0x1C, 0, 0, (u32) em, 0);
                }
                if (em->motEvent & 2) {
                    if (em->xFF) {
                        EstSet((int) em, -1, 0, 0, 0x2C, 0x1D, 0, 0, (u32) em, 0);
                    } else {
                        EstSet((int) em, -1, 0, 0, 0x2C, 0x1E, 0, 0, (u32) em, 0);
                    }
                }
            }
        }
        break;
    }
}

static void em35_R1_Turn(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0: {
        int flip = 1;

        if (em->motFlags & 0x40) {
            flip = 0x41;
        }
        if (w->targetAngAbs > 2.268928f) {
            MotionSetCore(em, MOTION(em), ARC(0xF), (int) ARC(0x10), 10, flip, 0);
            em->xFF = 1;
        } else if (w->targetAng < 0.0f) {
            if (flip & 0x40) {
                MotionSetCore(em, MOTION(em), ARC(0x13), (int) ARC(0x14), 10, flip, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x11), (int) ARC(0x12), 10, flip, 0);
            }
            em->xFF = 2;
        } else {
            if (flip & 0x40) {
                MotionSetCore(em, MOTION(em), ARC(0x11), (int) ARC(0x12), 10, flip, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x13), (int) ARC(0x14), 10, flip, 0);
            }
            em->xFF = 3;
        }
        em->xFE++;
    }
    case 1:
        if (em->motEvent & 8) {
            w->flags |= 0x10;
        }
        if (MotionMoveF(em, 0)) {
            if (em->plDist2 < 2250000.0f && w->routeAngAbs < 0.7853982f) {
                EmRoutineSet(em, 1, 0xC, 0, 1);
            } else if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else if (em35BigStepCk(em) == 0) {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

static void em35_R1_Atk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0:
        if (w->routeAng < 0.0f) {
            em->xFF = 0;
            if (w->routeAngAbs < 1.5707964f) {
                MotionSetCore(em, MOTION(em), ARC(0x19), (int) ARC(0x1A), 10, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x1B), (int) ARC(0x1C), 10, 1, 0);
                EstSet((int) em, -1, 0, 0, 0x2C, 0x13, 0, 0, (u32) em, 0);
            }
        } else {
            em->xFF = 1;
            if (w->routeAngAbs < 1.5707964f) {
                MotionSetCore(em, MOTION(em), ARC(0x19), (int) ARC(0x1A), 10, 0x41, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x1B), (int) ARC(0x1C), 10, 0x41, 0);
                EstSet((int) em, -1, 0, 0, 0x2C, 0x13, 0, 0, (u32) em, 0);
            }
        }
        w->timer = 30;
        w->timer2 = 8;
        w->atkTimer = 60;
        w->atkHit = 0;
        w->atkHit2 = 0;
        em->xFE++;
    case 1:
        if (w->atkTimer) {
            w->atkTimer--;
            w->flags |= 0x80;
        }
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(0xB);
            }
            if (w->atkHit) {
                w->atkWait = 90;
                EmRoutineSet(em, 1, 0, 0, 0);
            } else if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else {
            if ((em->motEvent & 4) && w->routeAngAbs < 1.5707964f && em->plDist2 < 25000000.0f) {
                ActBtn.set(0x13, 0xB, (int) em35AtkEscapeAction, (int) em, 1, 3, 0, 0);
            }
            if (em->motEvent & 1) {
                if (em->xFF == 0) {
                    em35AtkCk(em, 0, 0x11);
                    em35AtkCk(em, 0, 0x12);
                    em35AtkCk(em, 0, 0x13);
                    em35AtkCk(em, 0, 0x14);
                    em35AtkCk(em, 0, 0x2C);
                    em35AtkCk(em, 0, 0x2D);
                    em35AtkCk(em, 0, 0x2E);
                    em35AtkCk(em, 0, 0x2F);
                } else {
                    em35AtkCk(em, 1, 0x17);
                    em35AtkCk(em, 1, 0x18);
                    em35AtkCk(em, 1, 0x19);
                    em35AtkCk(em, 1, 0x1A);
                    em35AtkCk(em, 1, 0x26);
                    em35AtkCk(em, 1, 0x27);
                    em35AtkCk(em, 1, 0x28);
                    em35AtkCk(em, 1, 0x29);
                }
            }
        }
        break;
    }
}

static void em35_R1_br_AtkDouble(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->motEvent & 1) {
        em35AtkCk(em, 3, 0x11);
        em35AtkCk(em, 3, 0x12);
        em35AtkCk(em, 3, 0x13);
        em35AtkCk(em, 3, 0x14);
        em35AtkCk(em, 3, 0x2C);
        em35AtkCk(em, 3, 0x2D);
        em35AtkCk(em, 3, 0x2E);
        em35AtkCk(em, 3, 0x2F);
        if (w->atkHit) {
            EmRoutineSet(em, 1, 7, 0, 0);
            return;
        }
    }
    if (em->motEvent & 2) {
        em35AtkCk(em, 3, 0x17);
        em35AtkCk(em, 3, 0x18);
        em35AtkCk(em, 3, 0x19);
        em35AtkCk(em, 3, 0x1A);
        em35AtkCk(em, 3, 0x26);
        em35AtkCk(em, 3, 0x27);
        em35AtkCk(em, 3, 0x28);
        em35AtkCk(em, 3, 0x29);
        if (w->atkHit) {
            EmRoutineSet(em, 1, 7, 0, 0);
        }
    }
}

static void em35_R1_AtkDouble(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x17), (int) ARC(0x18), 10, 1, 0);
        w->lockWait = 450;
        w->atkHit = 0;
        w->atkHit2 = 0;
        w->walkType = 0;
        w->atkTimer = 60;
        em->xFE++;
    case 1:
        if (w->atkTimer) {
            w->atkTimer--;
            w->flags |= 0x80;
        }
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(0xB);
            }
            if (w->atkHit) {
                w->atkWait = 90;
                EmRoutineSet(em, 1, 0, 0, 0);
            } else if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else {
            if (w->atkHit2) {
                w->walkType = 1;
            }
            if ((em->motEvent & 4) && w->routeAngAbs < 1.5707964f && em->plDist2 < 25000000.0f) {
                ActBtn.set(0x13, 0xB, (int) em35AtkEscapeAction, (int) em, 1, 3, 0, 0);
            }
            if ((em->motEvent & 0x10) && w->atkHit == 0 && w->routeAngAbs < 1.5707964f &&
                em->plDist2 < 25000000.0f) {
                if (w->walkType) {
                    ActBtn.set(0x13, 0xB, (int) em35AtkEscapeAction, (int) em, 2, 4, 0, 0);
                } else {
                    ActBtn.set(0x13, 0xB, (int) em35AtkEscapeAction, (int) em, 1, 4, 0, 0);
                }
            }
        }
        break;
    }
}

static void em35_R1_BearHug(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0:
        em35CatchPosSet(em);
        MotionSetCore(em, MOTION(em), ARC(0x36), (int) ARC(0x37), 5, 1, 0);
        SetPlDamage((int) em, plem35_BearHug);
        PlSetDamageSe(0);
        PlGachaInit();
        em->dmg.set(0, 0);
        EstSet((int) em, -1, 0, 0, 0x2C, 0x1F, 0, 0, (u32) em, 0);
        EstSet((int) pPL, -1, 0, 0, 0x2C, 0x22, 0, 0, (u32) pPL, 0);
        w->timer = 145;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->motEvent & 2) {
            PlGachaMove();
            LifeDownSet2(pPL, 10, 0, 1);
            if ((em->motEvent & 4) && (u32) PlGachaGet() > 50) {
                em->xFE = 2;
            } else if (em->motEvent & 1) {
                pG->pl_life = 0;
                EstSet((int) em, -1, 0, 0, 0x2C, 0x20, 0, 0, (u32) em, 0);
                EstSet((int) pPL, -1, 0, 0, 0x2C, 0x23, 0, 0, (u32) pPL, 0);
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x38), (int) ARC(0x39), 0, 1, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
    em->x3A8 = em->pos;
}

// Routine test on the cModel status word (em10.cpp EM_RTN).
#define EM_RTN(em, fc, fd) (((em)->stat & 0xFFFF0000) == (u32) (((fc) << 24) | ((fd) << 16)))

static void plem35_BearHug(cPlayer* pl)
{
    BitOn(pG->flags_5010, 0x8000);
    pl->subArc = PL_EM_G->subArc;
    pl->dmType = 10;
    switch (pl->xFE) {
    case 0: {
        Vec v;
        f32 y;

        pl->atari.flags &= ~0x300;
        v.x = -101.81f;
        v.y = 0.0f;
        v.z = 3473.71f;
        pl->rot.x = 0.0f;
        y = PL_EM(pl)->rot.y;
        pl->rot.z = 0.0f;
        pl->rot.y = y + PI;
        LIMIT_ANGLE(pl->rot.y);
        PSMTXMultVec(PL_EM(pl)->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x93), 0, 5, 1, 0);
        PlSetFace(1);
        pl->atari.set(10, 480.00003f, 400.0f);
        pl->dmg.set(0, 0);
        pl->xFE++;
    }
    case 1:
        MotionMoveF(pl, 0);
        pl->xFE = PL_EM(pl)->xFE;
        if (!EM_RTN(PL_EM_G, 1, 7)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        } else {
            if (pl->frame > 72.7f && pl->frame < 73.3f) {
                U32Set(pl->x3E0, SndCall(8, 0x4B, &pl->pos, PL_EM(pl)->id, 0, pl));
                VibSetData(VIB_TBL, 0xB, 1);
            }
            if (pl->frame > 157.7f && pl->frame < 158.3f) {
                U32Set(pl->x3E0, SndCall(8, 0x4D, &pl->pos, PL_EM(pl)->id, 0, pl));
                VibSetData(VIB_TBL, 0xB, 1);
            }
        }
        break;
    case 2: {
        Vec v;
        f32 y;

        v.x = 30.01f;
        v.y = 0.0f;
        v.z = 244.65f;
        pl->rot.x = 0.0f;
        y = PL_EM(pl)->rot.y;
        pl->rot.z = 0.0f;
        pl->rot.y = y + PI;
        LIMIT_ANGLE(pl->rot.y);
        PSMTXMultVec(PL_EM(pl)->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x94), 0, 0, 1, 0);
        SndStop(pl->x3E0, 0);
        SndCall(1, 0x3D, &pPL->pos, pPL->id, 0, pPL);
        pl->xFE++;
    }
    case 3:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        } else if (!EM_RTN(PL_EM_G, 1, 7)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em35AtkEscapeAction(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->atkHit2 = 1;
    SetPlDamage((int) em, plem35Sit);
    GameAddPoint(9);
}

static void plem35Sit(cPlayer* pl)
{
    pl->subArc = PL_EM(pl)->subArc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x9A), 0, 3, 1, 0);
        pl->dmg.set(0, 30);
        GameAddPoint(0xB);
        pl->xFE++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void em35_R1_Atk2F(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0: {
        f32 ang;
        f32 abs;

        w->atkHit = 0;
        ang = GetXZAngle(&em->pos, &pPL->pos);
        abs = fabsf(ang);
        if (ang < 0.0f) {
            w->jumpAng = -1.5707964f;
        } else {
            w->jumpAng = 1.5707964f;
        }
        if (abs < 0.7853982f) {
            w->jumpAng = 0.0f;
        }
        if (abs > 2.3561945f) {
            w->jumpAng = PI;
        }
        if (Muku2(w->jumpAng, ang, PI) < 0.0f) {
            em->xFF = 1;
            MotionSetCore(em, MOTION(em), ARC(0x26), (int) ARC(0x27), 10, 0x41, 0);
        } else {
            em->xFF = 0;
            MotionSetCore(em, MOTION(em), ARC(0x26), (int) ARC(0x27), 10, 1, 0);
        }
        em->xFE++;
    }
    case 1:
        em->rot.y += Muku2(em->rot.y, w->jumpAng, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        if (w->routeAngAbs < 0.7853982f && (Rnd() & 1)) {
            if (em->xFF) {
                MotionSetCore(em, MOTION(em), ARC(0x15), (int) ARC(0x16), 10, 0x41, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x15), (int) ARC(0x16), 10, 1, 0);
            }
        } else {
            if (em->xFF) {
                MotionSetCore(em, MOTION(em), ARC(0x28), (int) ARC(0x29), 10, 0x41, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x28), (int) ARC(0x29), 10, 1, 0);
            }
        }
        SndCall(8, 0x66, &em->pos, em->id, 0, em);
        w->timer = 10;
        w->atkHit = 0;
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(0xB);
            }
            em->xFE++;
        } else if (em->motEvent & 1) {
            em35AtkCk(em, 5, 0x2C);
            em35AtkCk(em, 5, 0x2D);
            em35AtkCk(em, 5, 0x2E);
            em35AtkCk(em, 5, 0x2F);
            em35AtkCk(em, 6, 0x26);
            em35AtkCk(em, 6, 0x27);
            em35AtkCk(em, 6, 0x28);
            em35AtkCk(em, 6, 0x29);
        }
        break;
    case 4:
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(0x2A), (int) ARC(0x2B), 10, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x2A), (int) ARC(0x2B), 10, 1, 0);
        }
        w->timer = 10;
        w->atkHit = 0;
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            if (w->atkHit) {
                w->atkWait = 90;
                EmRoutineSet(em, 1, 0, 0, 0);
            } else if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

static void plem35DmFall2F(cPlayer* pl)
{
    pl->subArc = PL_EM(pl)->subArc;
    pl->dmType = 10;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x92), 0, 3, 1, 0);
        pl->atari.throughOn();
        if ((s16) pGS->pl_life > 0) {
            PlSetDamageSe(0);
        } else {
            PlSetDamageSe(0xD);
        }
        EstSet((int) pl, -1, 0, 0, 0x2C, 0x16, 0, 0, (u32) pl, 0);
        pl->xFE++;
    case 1:
        if (MotionMoveF(pl, 0) && (s16) pG->pl_life > 0) {
            pl->atari.throughOff();
            EmRoutineSet(pPLS, 1, 0, 0xA, 0);
        } else if (pl->frame > 39.7f && pl->frame < 40.3f) {
            SndCall(5, 5, &pPL->pos, pPL->id, 0, pPL);
            SndCall(1, 0x12, &pPL->pos, pPL->id, 0, pPL);
            SndCall(1, 7, &pPL->pos, pPL->id, 0, pPL);
            VibSetData(VIB_TBL, 0xB, 1);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void em35_R1_LongAtk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x1D), (int) ARC(0x1E), 10, 1, 0);
        w->timer = 15;
        w->atkHit = 0;
        w->lockWait = 450;
        w->atkTimer = 60;
        em->xFE++;
    case 1:
        if (w->atkTimer) {
            w->atkTimer--;
            w->flags |= 0x80;
        }
        if (w->timer) {
            w->timer--;
            em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.049087387f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if ((em->motEvent & 4) && w->routeAngAbs < 1.5707964f && em->plDist2 < 64000000.0f) {
            ActBtn.set(0x25, 0xB, (int) em35DashEscapeAction, (int) em, 1, 3, 0, 0);
        }
        if (em->motEvent & 0x20) {
            EstSet((int) em, -1, 0, 0, 0x2C, 0x12, 0, 0, (u32) em, 0);
        }
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(0xB);
            }
            if (w->atkHit) {
                w->atkWait = 90;
                EmRoutineSet(em, 1, 0, 0, 0);
            } else if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        } else if (em->motEvent & 1) {
            em35AtkCk(em, 2, 0x11);
            em35AtkCk(em, 2, 0x12);
            em35AtkCk(em, 2, 0x13);
            em35AtkCk(em, 2, 0x14);
            em35AtkCk(em, 2, 0x2C);
            em35AtkCk(em, 2, 0x2D);
            em35AtkCk(em, 2, 0x2E);
            em35AtkCk(em, 2, 0x2F);
            em35AtkCk(em, 2, 0x17);
            em35AtkCk(em, 2, 0x18);
            em35AtkCk(em, 2, 0x19);
            em35AtkCk(em, 2, 0x1A);
            em35AtkCk(em, 2, 0x26);
            em35AtkCk(em, 2, 0x27);
            em35AtkCk(em, 2, 0x28);
            em35AtkCk(em, 2, 0x29);
        }
        break;
    }
}

static void em35_R1_Hook(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x2C), (int) ARC(0x2D), 10, 1, 0);
        w->timer = 15;
        w->timer2 = 30;
        w->atkHit = 0;
        w->lockWait = 450;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
            em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.049087387f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->atkHit) {
                if (em->plDist2 < 2250000.0f && w->routeAngAbs < 0.7853982f && (Rnd() & 1)) {
                    EmRoutineSet(em, 1, 0xC, 0, 0);
                } else if (em->plDist2 < 12250000.0f) {
                    EmRoutineSet(em, 1, 4, 0, 0);
                } else {
                    goto far;
                }
            } else {
            far:
                if (w->targetAngAbs > 1.0471976f) {
                    if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                        EmRoutineSet(em, 1, 4, 0, 0);
                    } else if (w->targetAngAbs > 1.0471976f) {
                        EmRoutineSet(em, 1, 3, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 3, 0, 0);
                    }
                } else {
                    EmRoutineSet(em, 1, 1, 0, 0);
                }
            }
        } else if (em->motEvent & 1) {
            em35AtkCk(em, 4, 0x11);
            em35AtkCk(em, 4, 0x12);
            em35AtkCk(em, 4, 0x13);
            em35AtkCk(em, 4, 0x14);
            em35AtkCk(em, 4, 0x2C);
            em35AtkCk(em, 4, 0x2D);
            em35AtkCk(em, 4, 0x2E);
            em35AtkCk(em, 4, 0x2F);
            em35AtkCk(em, 4, 0x17);
            em35AtkCk(em, 4, 0x18);
            em35AtkCk(em, 4, 0x19);
            em35AtkCk(em, 4, 0x1A);
            em35AtkCk(em, 4, 0x26);
            em35AtkCk(em, 4, 0x27);
            em35AtkCk(em, 4, 0x28);
            em35AtkCk(em, 4, 0x29);
        }
        break;
    }
}

static void plem35DmHook(cPlayer* pl)
{
    pl->subArc = PL_EM(pl)->subArc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x91), 0, 3, 1, 0);
        pl->dmType = 30;
        PlSetDamageSe(0);
        PlSetFace(1);
        pl->xFE++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void em35_R1_br_Critical(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->hp > 0 && (em->motEvent & 1)) {
        em35AtkCk(em, 7, 0x2C);
        em35AtkCk(em, 7, 0x2D);
        em35AtkCk(em, 7, 0x2E);
        em35AtkCk(em, 7, 0x2F);
        if (w->atkHit) {
            VibSetData(VIB_TBL, 0xB, 1);
            em->dmg.set(0, 2);
            EmRoutineSet(em, 1, 0xA, 0, 0);
        }
    }
}

// Turn towards the player with a slight lead (em35_R1_Critical). A macro, not an inline: the PI
// pool load is issued above the preceding `timer--` store (an inlined body's pool loads lose
// RTX_UNCHANGING_P and sink below it).
// `ang` is the routine's variable (one pseudo for both expansions): a global pseudo takes the copy
// preference f1 (ascending scan) where a block-local one takes f2 (allocation order), which decides
// whether the `fmr f2,f1` argument copy sits before or after the `lis` of Muku2's limit.
#define EM35_CRITICAL_TURN(em, ang)                                                                 \
    {                                                                                               \
        ang = LIMIT_ANGLE((em)->rot.y + Muku(&(em)->pos, &pPLS->pos, (em)->rot.y, PI) + 0.05235988f); \
        (em)->rot.y += Muku2((em)->rot.y, ang, 0.09817477f);                                        \
        (em)->rot.y = LIMIT_ANGLE((em)->rot.y);                                                     \
    }

static void em35_R1_Critical(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    f32 ang;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x22), (int) ARC(0x23), 10, 1, 0);
        w->timer = 40;
        w->atkHit = 0;
        w->lockWait = 450;
        em->xFE++;
    case 1:
        w->flags |= 0x80;
        if (w->timer) {
            w->timer--;
            EM35_CRITICAL_TURN(em, ang);
        }
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x24), (int) ARC(0x25), 10, 1, 0);
        w->timer = 10;
        w->atkHit = 0;
        w->lockWait = 450;
        w->atkTimer = 60;
        em->xFE++;
    case 3:
        if (w->atkTimer) {
            w->atkTimer--;
            w->flags |= 0x80;
        }
        if (w->timer) {
            w->timer--;
            EM35_CRITICAL_TURN(em, ang);
        }
        if (em->motEvent & 4) {
            ActBtn.set(0x25, 0xB, (int) em35DashEscapeAction, (int) em, 1, 3, 0, 0);
        }
        if (MotionMoveF(em, 0)) {
            GameAddPoint(0xB);
            if (w->atkHit) {
                w->atkWait = 90;
                EmRoutineSet(em, 1, 0, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

static void em35_R1_CriticalHit(cEm35* em)
{
    em->dmg.set(0, 10);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x40), (int) ARC(0x41), 0, 1, 0);
        SetPlDamage((int) em, plem35_CriticalHit);
        PlSetDamageSe(0xD);
        em->xFE++;
    case 1:
        em->dmg.set(0, 2);
        MotionMoveF(em, 0);
        break;
    }
    em->x3A8 = em->pos;
}

static void plem35_CriticalHit(cPlayer* pl)
{
    f32 y;

    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 10);
    pl->subArc = PL_EM(pl)->subArc;
    switch (pl->xFE) {
    case 0: {
        Vec v;

        pl->atari.flags &= ~0x300;
        v.x = 755.75f;
        v.y = 0.0f;
        v.z = 2621.64f;
        pl->rot.x = 0.0f;
        y = PL_EM(pl)->rot.y;
        pl->rot.z = 0.0f;
        pl->rot.y = y + PI;
        LIMIT_ANGLE(pl->rot.y);
        PSMTXMultVec(PL_EM(pl)->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x90), 0, 0, 1, 0);
        PlSetFace(1);
        pl->x3E0 = 100;
        EstSet((int) pl, -1, 0, 0, 0x2C, 0x10, 0, 0, (u32) pl, 0);
        pl->xFE++;
    }
    case 1:
        if (pl->x3E0) {
            pl->x3E0--;
            if (pl->x3E0 == 0) {
                pG->pl_life = 0;
            }
        }
        MotionMoveF(pl, 0);
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em35DashEscapeAction(cEm35* em)
{
    SetPlDamage((int) em, plem35DashEscape);
    GameAddPoint(9);
}

static void plem35DashEscape(cPlayer* pl)
{
    pl->subArc = PL_EM(pl)->subArc;
    pl->dmType = 10;
    switch (pl->xFE) {
    case 0:
        if (Muku(&pl->pos, &PL_EM(pl)->pos, pl->rot.y, PI) < 0.0f) {
            MotionSetCore(pl, MOTION(pl), PL_ARC(0x98), (int) PL_ARC(0x99), 3, 1, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC(0x98), (int) PL_ARC(0x99), 3, 0x41, 0);
        }
        GameAddPoint(0xB);
        SndCall(1, 0x48, &pl->pos, 0, 0, pl);
        SndCall(1, 0x11, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        pl->x3E0 = 50;
        pl->x3E4 = 15;
        pl->xFE++;
    case 1:
        em35EscapeCamMove(PL_EM(pl));
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

void em35EscapeCamMove(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
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
#line 3756 "D:/Bio4/Prog/em35.cpp"
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

static void plem35DmStamp(cPlayer* pl)
{
    pl->subArc = PL_EM(pl)->subArc;
    pl->dmType = 10;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x97), 0, 3, 1, 0);
        PlSetFace(1);
        pl->atari.flags &= ~0x200;
        if ((s16) pGS->pl_life > 0) {
            PlSetDamageSe(0);
        } else {
            PlSetDamageSe(0xD);
        }
        EstSet((int) pl, -1, 0, 0, 0x2C, 0x11, 0, 0, (u32) pl, 0);
        pl->xFE++;
    case 1:
        em35StampCamMove(PL_EM(pl));
        if (MotionMoveF(pl, 0)) {
            if ((s16) pG->pl_life > 0) {
                EmRoutineSet(pPL, 1, 0, 0xA, 0);
            }
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

void em35StampCamMove(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
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

static void em35_R1_br_Catch(cEm35* em)
{
    if (em->hp > 0 && (em->motEvent & 2) && em35CatchCk(em)) {
        VibSetData(VIB_TBL, 7, 1);
        if (em->motFlags & 0x40) {
            em->stat = 0x010D0001;
        } else {
            em->stat = 0x010D0000;
        }
    }
}

static void em35_R1_Catch(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0:
        if (w->routeAng < 0.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x1F), (int) ARC(0x20), 10, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x1F), (int) ARC(0x20), 10, 0x41, 0);
        }
        w->atkTimer = 45;
        em->xFE++;
    case 1:
        if (w->atkTimer) {
            w->atkTimer--;
            w->flags |= 0x80;
        }
        if (MotionMoveF(em, 0)) {
            GameAddPoint(0xB);
            if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

static void em35_R1_CatchHit(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0:
        em->atari.clrFlag100();
        em35CatchPosSet(em);
        MotionSetCore(em, MOTION(em), ARC(0x34), (int) ARC(0x35), 5, 1, 0);
        SetPlDamage((int) em, plem35_CatchHit);
        PlSetDamageSe(0);
        w->timer = 15;
        w->timer2 = 40;
        PlGachaInit();
        em->dmg.set(0, 0);
        em->xFE++;
    case 1:
        PlGachaMove();
        if (em->motEvent & 1) {
            LifeDownSet2(pPL, 250, 0, 1);
            SndCall(5, 5, &pPL->pos, pPL->id, 0, pPL);
            SndCall(1, 0x12, &pPL->pos, pPL->id, 0, pPL);
            SndCall(1, 7, &pPL->pos, pPL->id, 0, pPL);
            VibSetData(VIB_TBL, 0xB, 1);
        }
        if (MotionMoveF(em, 0)) {
            if ((u32) PlGachaGet() <= 49 || (s16) pG->pl_life <= 1) {
                em->xFE = 4;
            } else {
                em->xFE = 2;
            }
        } else if (em->frame > 99.7f && em->frame < 100.3f) {
            if ((u32) PlGachaGet() <= 49 || (s16) pG->pl_life <= 1) {
                em->xFE = 4;
            } else {
                em->xFE = 2;
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x3E), (int) ARC(0x3F), 0, 1, 0);
        em->atari.setFlag100();
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x3C), (int) ARC(0x3D), 0, 1, 0);
        AtariFlagsOr(&em->atari, 0x100);
        LifeDownSet2(pPL, 500, 0, 0);
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            w->atkWait = 90;
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
    em->x3A8 = em->pos;
}

static void plem35_CatchHit(cPlayer* pl)
{
    BitOn(pG->flags_5010, 0x8000);
    pl->subArc = PL_EM_G->subArc;
    pl->dmType = 2;
    switch (pl->xFE) {
    case 0: {
        cAtariInfo* at;
        Vec v;
        f32 y;

        pl->atari.flags &= ~0x300;
        at = &pl->atari;
        at->clrFlag100();
        v.x = -61.37f;
        v.y = 0.0f;
        v.z = 1774.91f;
        pl->rot.x = 0.0f;
        y = PL_EM(pl)->rot.y;
        pl->rot.z = 0.0f;
        pl->rot.y = y + PI;
        LIMIT_ANGLE(pl->rot.y);
        PSMTXMultVec(PL_EM(pl)->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x8C), 0, 5, 1, 0);
        PlSetFace(1);
        at->set(10, 480.00003f, 400.0f);
        EstSet((int) pl, -1, 0, 0, 0x2C, 0xE, 0, 0, (u32) pl, 0);
        pl->xFE++;
    }
    case 1:
        MotionMoveF(pl, 0);
        pl->xFE = PL_EM(pl)->xFE;
        if (!EM_RTN(PL_EM_G, 1, 0xD)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    case 2: {
        Vec v;
        f32 y;

        v.x = -406.3f;
        v.y = 0.0f;
        v.z = 1014.0f;
        pl->rot.x = 0.0f;
        y = PL_EM(pl)->rot.y;
        pl->rot.z = 0.0f;
        pl->rot.y = y + PI;
        LIMIT_ANGLE(pl->rot.y);
        PSMTXMultVec(PL_EM(pl)->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x8F), 0, 0, 1, 0);
        SndCall(1, 0x3D, &pPL->pos, pPL->id, 0, pPL);
        pl->xFE++;
    }
    case 3:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        } else if (!EM_RTN(PL_EM_G, 1, 0xD)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        }
        break;
    case 4: {
        Vec v;
        f32 y;

        v.x = -406.3f;
        v.y = 0.0f;
        v.z = 1014.0f;
        pl->rot.x = 0.0f;
        y = PL_EM(pl)->rot.y;
        pl->rot.z = 0.0f;
        pl->rot.y = y + PI;
        LIMIT_ANGLE(pl->rot.y);
        PSMTXMultVec(PL_EM(pl)->mat, &v, &pl->pos);
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x8E), 0, 0, 1, 0);
        EstSet((int) pl, -1, 0, 0, 0x2C, 0xF, 0, 0, (u32) pl, 0);
        pl->xFE++;
    }
    case 5:
        pl->dmg.set(0, 2);
        if (MotionMoveF(pl, 0)) {
            if ((s16) pG->pl_life > 0) {
                pl->xFE++;
            }
        } else {
            if (pl->frame > 38.7f && pl->frame < 39.3f) {
                SndCall(8, 0x13, &pPL->pos, PL_EM(pl)->id, 0, pPL);
                SndCall(8, 0x58, &pl->pos, PL_EM(pl)->id, 0, pl);
                VibSetData(VIB_TBL, 0xB, 1);
            }
            if (pl->frame > 77.7f && pl->frame < 78.3f) {
                SndCall(8, 0x14, &pPL->pos, PL_EM(pl)->id, 0, pPL);
                SndCall(8, 0x59, &pl->pos, PL_EM(pl)->id, 0, pl);
                VibSetData(VIB_TBL, 0xB, 1);
                if ((s16) pG->pl_life <= 0) {
                    PlSetDamageSe(0xD);
                }
            }
        }
        break;
    case 6:
        pl->atari.flags |= 0x300;
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x8A), 0, 0, 1, 0);
        SndCall(1, 0x29, &pl->getPartsPtr(0)->worldPos, 0, 0, pPL);
        SndCall(1, 4, &pl->pos, 0, 0, pl);
        pl->xFE++;
    case 7:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 30);
        } else {
            if (pl->frame > 21.7f && pl->frame < 22.3f) {
                SndCall(5, 2, &pl->pos, 0, 0, pl);
            }
            if (pl->frame > 19.7f && pl->frame < 20.3f) {
                SndCall(5, 3, &pl->pos, 0, 0, pl);
            }
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em35_R1_U_Wait(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(0x48), 0, 10, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x49), 0, 10, 5, 0);
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if ((Rnd() & 3) == 0 && em->plDist2 > 25000000.0f && em35BeamDownCk(em, w->beamNo)) {
                EmRoutineSet(em, 1, 0x1A, 0, 0);
            } else if ((Rnd() & 3) == 0 && em->plDist2 > 25000000.0f && em35BeamUpCk(em, w->beamNo)) {
                EmRoutineSet(em, 1, 0x19, 0, 0);
            } else {
                em35NextRtnSetUpper(em);
            }
        }
        break;
    }
}

static void em35_R1_U_Jump(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0: {
        f32 dy = fabsf(em->pos.y - pPL->pos.y);

        if (w->routeAngAbs < 0.5235988f && em->plDist2 < 49000000.0f && dy < 500.0f && (Rnd() & 1) &&
            pG->x4F88 > 1) {
            if (w->routeAng < 0.0f) {
                MotionSetCore(em, MOTION(em), ARC(0x70), (int) ARC(0x71), 3, 0x41, 0);
                em->xFF = 1;
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x70), (int) ARC(0x71), 3, 1, 0);
                em->xFF = 0;
            }
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x4E), (int) ARC(0x4F), 3, 1, 0);
            em->xFF = 0;
        }
        w->atkHit = 0;
        w->atkHit2 = 0;
        em->xFE++;
    }
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->atkHit) {
                em35NextRtnSetUpper2(em);
            } else {
                em35NextRtnSetUpper(em);
            }
        } else {
            if (em->motEvent & 1) {
                if (em->xFF == 0) {
                    em35AtkCk(em, 9, 0xE);
                    em35AtkCk(em, 9, 0xF);
                    em35AtkCk(em, 9, 0x10);
                    em35AtkCk(em, 9, 0x22);
                } else {
                    em35AtkCk(em, 8, 8);
                    em35AtkCk(em, 8, 9);
                    em35AtkCk(em, 8, 0xA);
                    em35AtkCk(em, 8, 0x1F);
                }
            }
            if ((em->motEvent & 4) && w->routeAngAbs < 1.5707964f && em->plDist2 < 25000000.0f) {
                ActBtn.set(0x13, 0xB, (int) em35AtkEscapeAction, (int) em, 1, 3, 0, 0);
            }
        }
        break;
    }
}

static void em35_R1_U_JumpUp(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x7C), (int) ARC(0x7D), 3, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

static void em35_R1_U_JumpDown(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x7E), (int) ARC(0x7F), 3, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

static void em35_R1_U_DoubleJump(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x80), (int) ARC(0x81), 3, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

static void em35_R1_U_BackJump(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x50), (int) ARC(0x51), 3, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            f32 dy = fabsf(em->pos.y - pPL->pos.y);

            if (em->plDist2 < 30250000.0f && dy < 500.0f) {
                if (em->plDist2 < 2250000.0f) {
                    EmRoutineSet(em, 1, 0x1B, 0, 0);
                } else if (em->plDist2 < 9000000.0f && w->routeAngAbs < 0.2617994f) {
                    EmRoutineSet(em, 1, 0x1D, 0, 0);
                } else if (em->plDist2 < 9000000.0f && (Rnd() & 1)) {
                    EmRoutineSet(em, 1, 0x1C, 0, 0);
                } else if (w->routeAngAbs < 0.7330383f) {
                    EmRoutineSet(em, 1, 0x1E, 0, 0);
                } else {
                    goto beam;
                }
            } else {
            beam:
                if (em->xFF && em35BeamBackCk(em, w->beamNo)) {
                    em->xFE = 0;
                } else if ((Rnd() & 1) && em35BeamDownCk(em, w->beamNo)) {
                    EmRoutineSet(em, 1, 0x1A, 0, 0);
                } else if ((Rnd() & 1) && em35BeamUpCk(em, w->beamNo)) {
                    EmRoutineSet(em, 1, 0x19, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 0xF, 0, 0);
                }
            }
        }
        break;
    }
}

static void em35_R1_U_Turn180(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x5A), (int) ARC(0x5B), 10, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

static void em35_R1_U_Step(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(0x52), (int) ARC(0x53), 10, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x52), (int) ARC(0x53), 10, 1, 0);
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

static void em35_R1_U_BigStep(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(0x54), (int) ARC(0x55), 10, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x54), (int) ARC(0x55), 10, 1, 0);
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

static void em35_R1_U_OverStep(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0: {
        f32 d;

        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(0x60), (int) ARC(0x61), 10, 0x41, 0);
            d = -(SQRTF(em35GetBeamDis(&em->pos, w->beamNo, 1, em->rot.y)) - 1500.0f);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x60), (int) ARC(0x61), 10, 1, 0);
            d = SQRTF(em35GetBeamDis(&em->pos, w->beamNo, 0, em->rot.y)) - 1500.0f;
        }
        w->jumpSpd.x = d;
        w->jumpSpd.y = 0.0f;
        w->jumpSpd.z = 0.0f;
        PSMTXMultVecSR(em->mat, &w->jumpSpd, &w->jumpSpd);
        em->xFE++;
    }
    case 1: {
        Vec v;
        Vec* js = &w->jumpSpd;

        PSVECScale(js, &v, 0.1f);
        PSVECAdd(&em->pos, &v, &em->pos);
        PSVECSubtract(js, &v, js);
        if (MotionMoveF(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
    }
}

static void em35_R1_U_StepUp(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x56), (int) ARC(0x57), 10, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

static void em35_R1_U_StepDown(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        if (w->routeAngAbs < 1.5707964f) {
            if (em->plDist2 < 9000000.0f && w->routeAngAbs < 1.0471976f && pG->x4F88 > 1) {
                MotionSetCore(em, MOTION(em), ARC(0x72), (int) ARC(0x73), 10, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x58), (int) ARC(0x59), 10, 1, 0);
            }
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x62), (int) ARC(0x63), 10, 1, 0);
        }
        w->atkHit = 0;
        w->atkHit2 = 0;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->atkHit) {
                em35NextRtnSetUpper2(em);
            } else {
                em35NextRtnSetUpper(em);
            }
        } else {
            if (em->motEvent & 1) {
                em35AtkCk(em, 8, 8);
                em35AtkCk(em, 8, 9);
                em35AtkCk(em, 8, 0xA);
                em35AtkCk(em, 8, 0x1F);
                em35AtkCk(em, 8, 0x2B);
                em35AtkCk(em, 8, 0x2C);
                em35AtkCk(em, 8, 0x2D);
                em35AtkCk(em, 8, 0x2E);
                em35AtkCk(em, 8, 0x2F);
            }
            if ((em->motEvent & 4) && em->plDist2 < 9000000.0f) {
                ActBtn.set(0x13, 0xB, (int) em35AtkEscapeAction, (int) em, 1, 3, 0, 0);
            }
        }
        break;
    }
}

static void em35_R1_U_HandAtk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0: {
        int flip;
        void* m;
        void* s;

        if (w->routeAng < 0.0f) {
            em->xFF = 1;
            flip = 0x41;
        } else {
            em->xFF = 0;
            flip = 1;
        }
        if (w->routeAngAbs < 1.5707964f) {
            m = ARC(0x66);
            s = ARC(0x67);
        } else {
            m = ARC(0x64);
            s = ARC(0x65);
        }
        MotionSetCore(em, MOTION(em), m, (int) s, 3, flip, 0);
        w->atkHit = 0;
        em->xFE++;
    }
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(0xB);
            }
            if (w->atkHit) {
                em35NextRtnSetUpper2(em);
            } else {
                em35NextRtnSetUpper(em);
            }
        } else if (em->motEvent & 1) {
            if (em->xFF == 0) {
                em35AtkCk(em, 8, 8);
                em35AtkCk(em, 8, 9);
                em35AtkCk(em, 8, 0xA);
                em35AtkCk(em, 8, 0x1F);
            } else {
                em35AtkCk(em, 9, 0xE);
                em35AtkCk(em, 9, 0xF);
                em35AtkCk(em, 9, 0x10);
                em35AtkCk(em, 9, 0x22);
            }
        }
        break;
    }
}

static void em35_R1_U_Atk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0: {
        void* m = ARC(0x68);
        void* s = ARC(0x69);

        if (w->routeAngAbs > 1.5707964f) {
            m = ARC(0x86);
            s = ARC(0x87);
        }
        if (w->routeAng < 0.0f) {
            em->xFF = 1;
        } else {
            em->xFF = 0;
        }
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), m, (int) s, 3, 0x41, 0);
        } else {
            MotionSetCore(em, MOTION(em), m, (int) s, 3, 1, 0);
        }
        EstSet((int) em, -1, 0, 0, 0x2C, 6, 0, 0, (u32) em, 0);
        w->atkHit = 0;
        em->xFE++;
    }
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(0xB);
            }
            if (w->atkHit) {
                em35NextRtnSetUpper2(em);
            } else {
                em35NextRtnSetUpper(em);
            }
        } else if (em->motEvent & 1) {
            if (em->xFF == 0) {
                em35AtkCk(em, 8, 8);
                em35AtkCk(em, 8, 9);
                em35AtkCk(em, 8, 0xA);
                em35AtkCk(em, 8, 0x1F);
                em35AtkCk(em, 8, 0x2B);
                em35AtkCk(em, 8, 0x2C);
                em35AtkCk(em, 8, 0x2D);
                em35AtkCk(em, 8, 0x2E);
                em35AtkCk(em, 8, 0x2F);
            } else {
                em35AtkCk(em, 9, 0xE);
                em35AtkCk(em, 9, 0xF);
                em35AtkCk(em, 9, 0x10);
                em35AtkCk(em, 9, 0x22);
                em35AtkCk(em, 9, 0x25);
                em35AtkCk(em, 9, 0x26);
                em35AtkCk(em, 9, 0x27);
                em35AtkCk(em, 9, 0x28);
                em35AtkCk(em, 9, 0x29);
            }
        }
        break;
    }
}

static void em35_R1_U_Upper(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0: {
        void* m = ARC(0x6E);
        void* s = ARC(0x6F);

        if (w->routeAng < 0.0f) {
            MotionSetCore(em, MOTION(em), m, (int) s, 3, 1, 0);
            em->xFF = 0;
        } else {
            MotionSetCore(em, MOTION(em), m, (int) s, 3, 0x41, 0);
            em->xFF = 1;
        }
        EstSet((int) em, -1, 0, 0, 0x2C, 6, 0, 0, (u32) em, 0);
        w->atkHit = 0;
        em->xFE++;
    }
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(0xB);
            }
            if (w->atkHit) {
                em35NextRtnSetUpper2(em);
            } else {
                em35NextRtnSetUpper(em);
            }
        } else if (em->motEvent & 1) {
            if (em->xFF == 0) {
                em35AtkCk(em, 0xA, 8);
                em35AtkCk(em, 0xA, 9);
                em35AtkCk(em, 0xA, 0xA);
                em35AtkCk(em, 0xA, 0x1F);
                em35AtkCk(em, 0xA, 0x2B);
                em35AtkCk(em, 0xA, 0x2C);
                em35AtkCk(em, 0xA, 0x2D);
                em35AtkCk(em, 0xA, 0x2E);
                em35AtkCk(em, 0xA, 0x2F);
            } else {
                em35AtkCk(em, 0xB, 0xE);
                em35AtkCk(em, 0xB, 0xF);
                em35AtkCk(em, 0xB, 0x10);
                em35AtkCk(em, 0xB, 0x22);
                em35AtkCk(em, 0xB, 0x25);
                em35AtkCk(em, 0xB, 0x26);
                em35AtkCk(em, 0xB, 0x27);
                em35AtkCk(em, 0xB, 0x28);
                em35AtkCk(em, 0xB, 0x29);
            }
        }
        break;
    }
}

static void em35_R1_U_AtkSpear(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0: {
        Vec v;
        f32 ang;

        // blendB before atkHit: the switch register (known 0) then dies at the `stb`, which is the store
        // sched1 issues right after blendA; blendB keeps weight 0 and sinks before v.y.
        w->blendA = 10;
        w->blendB = 0;
        w->atkHit = 0;
        w->blendRate = 0.0f;
        v.x = -87.72f;
        v.y = 0.0f;
        v.z = 678.9f;
        PSMTXMultVec(em->mat, &v, &v);
        ang = Muku(&v, &pPL->pos, em->rot.y, PI);
        if (ang > 0.0f) {
            ang *= 1.3f;
        }
        if (ang > 0.7853982f) {
            ang = 0.7853982f;
        }
        if (ang < -0.7853982f) {
            ang = -0.7853982f;
        }
        ang *= -324.6761f;
        if (ang > 255.0f) {
            ang = 255.0f;
        }
        if (ang < -255.0f) {
            ang = -255.0f;
        }
        w->blendRate = ang;
        em->xFE++;
    }
    case 1:
        em35BlendMotSet(em, ARC(0x6A), ARC(0x6D), ARC(0x6C), ARC(0x6B), 0, 0, 1);
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(0xB);
            }
            if (w->atkHit) {
                em35NextRtnSetUpper2(em);
            } else {
                em35NextRtnSetUpper(em);
            }
        } else if (em->motEvent & 1) {
            em35AtkCk(em, 0xC, 8);
            em35AtkCk(em, 0xC, 9);
            em35AtkCk(em, 0xC, 0xA);
            em35AtkCk(em, 0xC, 0x1F);
            em35AtkCk(em, 0xC, 0x2B);
            em35AtkCk(em, 0xC, 0x2C);
            em35AtkCk(em, 0xC, 0x2D);
            em35AtkCk(em, 0xC, 0x2E);
            em35AtkCk(em, 0xC, 0x2F);
        }
        break;
    }
}

// Nearest of the first three crawl positions (XZ plane).
static inline int em35NearCrawlPos(cEm35* em)
{
    f32 best = 999999730000.0f;
    int no = 0;
    int i;

    for (i = 0; i < 3; i++) {
        f32 d = (em->pos.x - em35_crawl_pos[i].x) * (em->pos.x - em35_crawl_pos[i].x) +
                (em->pos.z - em35_crawl_pos[i].z) * (em->pos.z - em35_crawl_pos[i].z);

        if (d < best) {
            best = d;
            no = i;
        }
    }
    return no;
}

static void em35_R1_U_Crawl(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x4A), (int) ARC(0x4B), 10, 5, em->xFF);
        w->timer2 = 0;
        w->timer = 15;
        w->atkTimer = (u8) (Rnd() % 30);
        em->xFE++;
    case 1: {
        Vec pos;
        Vec out;
        int no;

        if (w->atkTimer) {
            w->atkTimer--;
        } else {
            cModel* p = em->getPartsPtr(0);

            w->atkTimer = (u8) (Rnd() % 30) + 60;
            SndCall(8, 0x40, &p->worldPos, em->id, 0, em);
        }
        if (em->pos.y < -6000.0f) {
            f32 best = 999999730000.0f;
            int i;

            no = 0;
            for (i = 0; i < 3; i++) {
                f32 d = (em->pos.x - em35_crawl_pos[i].x) * (em->pos.x - em35_crawl_pos[i].x) +
                        (em->pos.z - em35_crawl_pos[i].z) * (em->pos.z - em35_crawl_pos[i].z);

                if (d < best) {
                    best = d;
                    no = i;
                }
            }
        } else {
            no = 4;
            if (em->pos.x < 35000.0f) {
                no = 3;
            }
        }
        pos = em35_crawl_pos[no];
        RouteCkToPos(em, &pos, &out, 0, 0);
        fabsf(Muku(&em->pos, &out, em->rot.y, PI));
        em->rot.y += Muku(&em->pos, &out, em->rot.y, 0.2617994f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        if ((em->pos.x - pos.x) * (em->pos.x - pos.x) + (em->pos.z - pos.z) * (em->pos.z - pos.z) < 1000000.0f) {
            em->xFC = 1;
            em->xFD = 0x22;
            em->xFE = 0;
            em->xFF = no;
        }
        break;
    }
    }
    w->timer2++;
    if (w->timer2 % 3 == 0) {
        EstSet((int) em, -1, 0, 0, 0x2C, 0xC, 0, 0, (u32) em, 0);
    }
    if (w->timer2 % 6 == 0) {
        EstSet((int) em, -1, 0, 0, 0x2C, 0xD, 0, 0, (u32) em, 0);
    }
}

static void em35_R1_U_CrawlTurn(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x84), (int) ARC(0x85), 10, 1, em->xFF);
        w->timer2 = 0;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFC = 1;
            em->xFD = 0x20;
            em->xFE = 0;
            em->xFF = 0x22;
        }
        break;
    }
    w->timer2++;
    if (w->timer2 % 3 == 0) {
        EstSet((int) em, -1, 0, 0, 0x2C, 0xC, 0, 0, (u32) em, 0);
    }
    if (w->timer2 % 6 == 0) {
        EstSet((int) em, -1, 0, 0, 0x2C, 0xD, 0, 0, (u32) em, 0);
    }
}

static void em35_R1_U_JumpToBeam(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    Mtx m;
    Vec spd;
    Vec rot;
    Vec v;

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        if (em->pos.y < -6000.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x82), (int) ARC(0x83), 5, 0, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x4C), (int) ARC(0x4D), 5, 0, 0);
        }
        w->jumpAng = 0.0f;
        if (em->rot.y > 1.5707964f || em->rot.y < -1.5707964f) {
            w->jumpAng = PI;
        }
        PSMTXRotRad(m, 'y', w->jumpAng);
        TransMatrix(m, &em35_crawl_pos[em->xFF]);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -73.81f;
        PSMTXMultVec(m, &v, &v);
        PSVECSubtract(&v, &em->pos, &w->jumpSpd);
        w->dmgCnt = 0;
        em->xFE++;
    case 1:
        if (!(em->motEvent & 4)) {
            w->flags |= 0x20;
        }
        em->rot.y += Muku2(em->rot.y, w->jumpAng, 0.19634955f);
        PSVECScale(&w->jumpSpd, &v, 0.05f);
        PSVECAdd(&em->pos, &v, &em->pos);
        PSVECSubtract(&w->jumpSpd, &v, &w->jumpSpd);
        MotionGetSpeed(em, MOTION(em), 0, &spd, &rot);
        PSMTXRotRad(m, 'y', w->jumpAng);
        PSMTXMultVecSR(m, &spd, &spd);
        MotionAddSpeed(em, MOTION(em), &spd, &rot);
        if (MotionMoveF(em, 0)) {
            em->rot.y = w->jumpAng;
            PSVECAdd(&em->pos, &w->jumpSpd, &em->pos);
            em35NextRtnSetUpper(em);
        }
        break;
    }
}

static void em35_R0_Damage(cEm35* em)
{
    EM35_WK(em)->flags |= 8;
    Em35_R2_move_tbl[em->xFD](em);
}

static void em35_R1_Dm_Small(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0: {
        Mtx inv;
        Vec a;
        Vec b;
        cModel* p0 = em->getPartsPtr(0x1E);
        cModel* p1 = em->getPartsPtr(0x22);

        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &p0->worldPos, &a);
        PSMTXMultVec(inv, &p1->worldPos, &b);
        if (a.z < b.z) {
            em->xFF = 1;
            MotionSetCore(em, MOTION(em), ARC(0x32), 0, 3, 0x41, 0);
        } else {
            em->xFF = 0;
            MotionSetCore(em, MOTION(em), ARC(0x32), 0, 3, 1, 0);
        }
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 0x29, &em->pos, em->id, 0, em);
        w->sndId = SndCall(8, 0x32, &em->pos, em->id, 0, em);
        em->atari.setFlag100();
        em->xFE++;
    }
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

static void em35_R1_Dm_Spinal(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x30), (int) ARC(0x31), 3, 1, 0);
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 0x2F, &em->pos, em->id, 0, em);
        w->sndId = SndCall(8, 0x34, &em->pos, em->id, 0, em);
        em->atari.setFlag100();
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
                break;
            }
            if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
                break;
            }
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        if (em->motEvent & 1) {
            SndStop(w->sndId, 0);
            w->sndId = SndCall(8, 0x3A, &em->pos, em->id, 0, em);
            w->sndId = SndCall(8, 0x37, &em->pos, em->id, 0, em);
        }
        break;
    }
}

static void em35_R1_Dm_Big(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0: {
        Mtx inv;
        Vec a;
        Vec b;
        cModel* p0 = em->getPartsPtr(0x1E);
        cModel* p1 = em->getPartsPtr(0x22);

        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &p0->worldPos, &a);
        PSMTXMultVec(inv, &p1->worldPos, &b);
        if (a.z < b.z) {
            em->xFF = 1;
            MotionSetCore(em, MOTION(em), ARC(0x2E), (int) ARC(0x2F), 3, 0x41, 0);
        } else {
            em->xFF = 0;
            MotionSetCore(em, MOTION(em), ARC(0x2E), (int) ARC(0x2F), 3, 1, 0);
        }
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 0x2C, &em->pos, em->id, 0, em);
        w->sndId = SndCall(8, 0x33, &em->pos, em->id, 0, em);
        em->atari.setFlag100();
        em->xFE++;
    }
    case 1:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
}

static void em35_R1_Dm_Frame(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0: {
        Mtx inv;
        Vec a;
        Vec b;
        cModel* p0 = em->getPartsPtr(0x1E);
        cModel* p1 = em->getPartsPtr(0x22);

        PSMTXInverse(em->mat, inv);
        PSMTXMultVec(inv, &p0->worldPos, &a);
        PSMTXMultVec(inv, &p1->worldPos, &b);
        if (a.z < b.z) {
            em->xFF = 1;
            MotionSetCore(em, MOTION(em), ARC(0x2E), (int) ARC(0x2F), 3, 0x41, 0);
        } else {
            em->xFF = 0;
            MotionSetCore(em, MOTION(em), ARC(0x2E), (int) ARC(0x2F), 3, 1, 0);
        }
        LifeDownSet2(em, 100, 0, 0);
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 0x2C, &em->pos, em->id, 0, em);
        w->sndId = SndCall(8, 0x33, &em->pos, em->id, 0, em);
        em->atari.setFlag100();
        w->timer = 50;
        em->xFE++;
    }
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 0);
                break;
            }
            if (w->targetAngAbs > 2.0943952f && em->plDist2 < 6250000.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
                break;
            }
            if (w->targetAngAbs > 1.0471976f) {
                EmRoutineSet(em, 1, 3, 0, 0);
                break;
            }
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        if (w->timer) {
            w->timer--;
            LifeDownSet2(em, 10, 0, 0);
            if (w->timer == 0 && em->hp <= 0) {
                EmRoutineSet(em, 3, 0, 0, 0);
            }
        }
        break;
    }
}

// Turn towards the nearest crawl position after a fall (em35_R1_Dm_U_Fall / Dm_U_Crawl).
static inline void em35CrawlStart(cEm35* em)
{
    Vec out;
    int no = em35NearCrawlPos(em);

    RouteCkToPos(em, &em35_crawl_pos[no], &out, 0, 0);
    if (fabsf(Muku(&em->pos, &out, em->rot.y, PI)) > 2.0943952f) {
        EmRoutineSet(em, 1, 0x21, 0, 0);
        MotionMoveF(em, 0);
    } else {
        EmRoutineSet(em, 1, 0x20, 0, 0);
    }
}

static void em35_R1_Dm_U_Fall(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    Vec out;
    f32 ang;

    switch (em->xFE) {
    case 0: {
        Vec a;
        Vec b;

        ang = fabsf(Muku(&em->pos, &em->x328, em->rot.y, PI));

        a.x = 0.0f;
        a.y = 500.0f;
        a.z = 0.0f;
        b.x = 0.0f;
        b.y = 500.0f;
        b.z = -2000.0f;
        PSMTXMultVec(em->mat, &a, &a);
        PSMTXMultVec(em->mat, &b, &b);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x100800) || ang > 1.5707964f) {
            MotionSetCore(em, MOTION(em), ARC(0x77), (int) ARC(0x78), 3, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x74), (int) ARC(0x75), 3, 1, 0);
        }
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 0x3B, &em->pos, em->id, 0, em);
        em->xFE++;
    }
    case 1:
        // The original keeps both copies of this tail (COMPILER-DIFF #6 shape: ours cross-jumps them).
        if (MotionMoveF(em, 0)) {
            MotionSetCore(em, MOTION(em), ARC(0x76), 0, 3, 1, 0);
            MotionMoveF(em, 0);
            em->xFE++;
        } else if (em->motEvent & 1) {
            f32 fl = SatMgr.getFloor(&em->pos, em->oldPos.y - em->pos.y + 2000.0f, 100000.0f, 0, 0);

            if (em->pos.y < fl) {
                em->pos.y = fl;
                MotionSetCore(em, MOTION(em), ARC(0x76), 0, 3, 1, 0);
                MotionMoveF(em, 0);
                em->xFE++;
            }
        }
        break;
    case 2:
        w->sndId = SndCall(8, 0x3E, &em->pos, em->id, 0, em);
        em->xFE++;
    case 3:
        w->flags |= 0x20;
        if (MotionMoveF(em, 0)) {
            if (em->hp > 0) {
                int no = em35NearCrawlPos(em);

                RouteCkToPos(em, &em35_crawl_pos[no], &out, 0, 0);
                ang = fabsf(Muku(&em->pos, &out, em->rot.y, PI));
                if (ang > 2.0943952f) {
                    EmRoutineSet(em, 1, 0x21, 0, 0);
                    MotionMoveF(em, 0);
                } else {
                    EmRoutineSet(em, 1, 0x20, 0, 0);
                }
            } else {
                em->clearStatus(5);
                em->setStatus(8);
                em->atari.flags &= ~0x300;
                em->xFE++;
            }
        }
        break;
    }
}

static void em35_R1_Dm_U_Crawl(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    w->flags |= 0x20;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x79), (int) ARC(0x7A), 3, 1, 0);
        SndStop(w->sndId, 0);
        w->sndId = SndCall(8, 0x29, &em->pos, em->id, 0, em);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em->hp > 0) {
                EmRoutineSet(em, 1, 0x20, 0, 0xA);
            } else {
                em->clearStatus(5);
                em->setStatus(8);
                em->atari.flags &= ~0x300;
                em->xFE++;
            }
        } else if (em->motEvent & 4) {
            em35CrawlStart(em);
        }
        break;
    }
}

static void em35_R0_Die(cEm35* em)
{
    EM35_WK(em)->flags |= 8;
    Em35_R3_move_tbl[em->xFD](em);
}

static void em35_R1_Die_Normal(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA), 0, 3, 1, 0);
        em->clearStatus(5);
        em->setStatus(8);
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->atari.flags &= ~0x300;
            em->xFE++;
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

static void em35_R1_Die_Pose(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    switch (em->xFE) {
    case 0:
        em->atari.throughOn();
        em->pos.x = 36817.0f;
        em->pos.y = -7963.56f;
        em->pos.z = -59757.32f;
        em->rot.x = 0.0f;
        em->rot.y = 0.0f;
        em->rot.z = 0.0f;
        MotionSetCore(em, MOTION(em), ARC(0x7B), 0, 0, 1, 0);
        EffectEspDelete(1, w->espKind, (u32) em, 0);
        EffectEspgenDelete(1, w->espKind, (int) em);
        EffectEfmDelete(1, w->espKind, (int) em);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        em->xFE++;
        break;
    }
}

void em35RouteCk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->hp <= 0) {
        return;
    }
    if (em->type == 1) {
        w->routePos = pPL->pos;
        w->flags |= 1;
        w->routeAng = Muku(&em->pos, &w->routePos, em->rot.y, PI);
        w->routeAngAbs = fabsf(w->routeAng);
        w->targetPos = w->routePos;
        w->targetAng = w->routeAng;
        w->targetAngAbs = w->routeAngAbs;
        w->targetDist = em->plDist2;
        w->pTarget = pPLS;
        w->flags &= ~4;
        return;
    }
    if (pPL->pos.y > em->pos.y + 1000.0f) {
        Vec v;

        w->routePos = pPL->pos;
        w->flags |= 1;
        w->routeAng = Muku(&em->pos, &w->routePos, em->rot.y, PI);
        w->routeAngAbs = fabsf(w->routeAng);
        w->targetPos = w->routePos;
        w->targetAng = w->routeAng;
        w->targetAngAbs = w->routeAngAbs;
        w->targetDist = em->plDist2;
        w->pTarget = pPLS;
        if (pPLS->pos.x < 33000.0f && pPLS->pos.z > -61300.0f) {
            if (em->pos.z < -60000.0f) {
                v.x = 35686.0f;
                v.y = -8100.0f;
                v.z = -58425.0f;
                RouteCkToPos(em, &v, &w->targetPos, 0, 0);
                w->targetAng = Muku(&em->pos, &w->targetPos, em->rot.y, PI);
                w->targetAngAbs = fabsf(w->targetAng);
            }
        } else if (pPLS->pos.x > 38000.0f && pPLS->pos.z > -63000.0f) {
            if (em->pos.x > 36500.0f) {
                v.x = 37419.0f;
                v.y = -8100.0f;
                v.z = -61700.0f;
                RouteCkToPos(em, &v, &w->targetPos, 0, 0);
                w->targetAng = Muku(&em->pos, &w->targetPos, em->rot.y, PI);
                w->targetAngAbs = fabsf(w->targetAng);
            }
        } else {
            if (em->pos.z > -61500.0f) {
                v.x = 36391.0f;
                v.y = -8100.0f;
                v.z = -63645.0f;
                RouteCkToPos(em, &v, &w->targetPos, 0, 0);
                w->targetAng = Muku(&em->pos, &w->targetPos, em->rot.y, PI);
                w->targetAngAbs = fabsf(w->targetAng);
            }
        }
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
}

void em35NeckMove(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    Vec v;
    cParts* p;

    em->getPartsPtr(4);
    p = (cParts*) pPL->getPartsPtr(4);
    v.x = 0.0f;
    v.y = 250.0f;
    v.z = 0.0f;
    PSMTXMultVec(p->mat, &v, &v);
    if (w->flags & 0x10) {
        w->neckAng = w->neckAng * 0.95f + Muku(&em->pos, &pPL->pos, em->rot.y, 1.0471976f) * 0.05f;
    } else {
        w->neckAng = w->neckAng * 0.95f;
    }
    if (em->type == 1) {
        p = (cParts*) em->getPartsPtr(3);
        p->motParts.flags |= 0x40000000;
        p->addRot.x = 0.0f;
        p->addRot.y = w->neckAng;
        p->addRot.z = 0.0f;
    } else {
        f32 ang = w->neckAng * 0.25f;

        p = (cParts*) em->getPartsPtr(2);
        p->motParts.flags |= 0x40000000;
        p->addRot.x = 0.0f;
        p->addRot.y = ang;
        p->addRot.z = 0.0f;
        p = (cParts*) em->getPartsPtr(3);
        p->motParts.flags |= 0x40000000;
        p->addRot.x = 0.0f;
        p->addRot.y = ang;
        p->addRot.z = 0.0f;
        p = (cParts*) em->getPartsPtr(4);
        p->motParts.flags |= 0x40000000;
        p->addRot.x = 0.0f;
        p->addRot.y = ang;
        p->addRot.z = 0.0f;
        p = (cParts*) em->getPartsPtr(5);
        p->motParts.flags |= 0x40000000;
        p->addRot.x = 0.0f;
        p->addRot.y = ang;
        p->addRot.z = 0.0f;
    }
}

// Turn the player towards the enemy and knock him down (em35AtkCk).
static inline void em35PlKnock(cEm35* em)
{
    FSet(pPL->rot.y, pPL->rot.y + Muku(&pPL->pos, &em->pos, pPL->rot.y, PI));
    pPL->rot.y = LIMIT_ANGLE(pPL->rot.y);
    PlSetDamage(8, 0, 0);
}

int em35AtkCk(cEm35* em, u32 no, int parts)
{
    Em35Work* w = EM35_WK(em);
    EmAtkInfo* info;
    cModel* p;
    int hit;

    if (w->atkHit) {
        return 0;
    }
    info = &em35_atk_tbl[no];
    p = em->getPartsPtr(parts);
    hit = EmAtkHitCk(info, &p->worldPos, &p->oldWorldPos, 0);
    if (hit != 0) {
        if (hit & 1) {
            w->atkHit = 1;
            switch (no) {
            default:
                EmPlBloodSet(em, &p->worldPos, 1, 0xFF, 0xFF);
                break;
            case 0:
            case 1:
                if (no == 0) {
                    pPL->x328 = em->pos;
                    EmPlBloodSet2(em, &p->worldPos, 1, 0x2C, 0x14);
                } else {
                    pPL->x328 = em->pos;
                    EmPlBloodSet2(em, &p->worldPos, 1, 0x2C, 0x15);
                }
                em35PlKnock(em);
                SndCall(8, 0x11, &em->pos, em->id, 0, em);
                break;
            case 2:
                SetPlDamage((int) em, plem35DmStamp);
                SndCall(8, 0x11, &em->pos, em->id, 0, em);
                break;
            case 3:
                SndCall(8, 0x47, &em->pos, em->id, 0, em);
                break;
            case 4:
                pPL->rot.y = GetXZAngle(&pPL->pos, &em->pos);
                SetPlDamage((int) em, plem35DmHook);
                SndCall(8, 0x47, &em->pos, em->id, 0, em);
                break;
            case 5:
            case 6:
                if (no == 5) {
                    pPL->x328 = em->pos;
                    EmPlBloodSet2(em, &p->worldPos, 1, 0x2C, 0x14);
                } else {
                    pPL->x328 = em->pos;
                    EmPlBloodSet2(em, &p->worldPos, 1, 0x2C, 0x15);
                }
                if (pPL->pos.y > em->pos.y + 2000.0f) {
                    if (em35PlFallCk(em) == 0) {
                        pPL->rot.y = GetXZAngle(&p->worldPos, &p->oldWorldPos);
                        PlSetDamage(8, 0, 0);
                    }
                }
                SndCall(8, 0x11, &em->pos, em->id, 0, em);
                break;
            case 7:
                EmPlBloodSet(em, &p->worldPos, 1, 0xFF, 0xFF);
                pG->pl_life = 1;
                em->dmType = 0x80;
                pPL->dmType = 0x80;
                SndCall(8, 0x1E, &em->pos, em->id, 0, em);
                break;
            case 8:
            case 9:
                if (no == 8) {
                    pPL->x328 = em->pos;
                    EmPlBloodSet2(em, &p->worldPos, 1, 0x2C, 0x14);
                } else {
                    pPL->x328 = em->pos;
                    EmPlBloodSet2(em, &p->worldPos, 1, 0x2C, 0x15);
                }
                em35PlKnock(em);
                SndCall(8, 0x11, &em->pos, em->id, 0, em);
                break;
            case 0xA:
            case 0xB:
                if (no == 0xA) {
                    pPL->x328 = em->pos;
                    EmPlBloodSet2(em, &p->worldPos, 1, 0x2C, 0x1A);
                } else {
                    pPL->x328 = em->pos;
                    EmPlBloodSet2(em, &p->worldPos, 1, 0x2C, 0x1B);
                }
                em35PlKnock(em);
                SndCall(8, 0x11, &em->pos, em->id, 0, em);
                break;
            case 0xC:
                pPL->x328 = em->pos;
                EmPlBloodSet2(em, &p->worldPos, 1, 0x2C, 0x19);
                em35PlKnock(em);
                SndCall(8, 0x11, &em->pos, em->id, 0, em);
                break;
            }
        }
        if (hit & 2) {
            EmSubBloodSet(em, &p->worldPos, 1, 0xFF, 0xFF);
            w->atkHit = 1;
        }
        QuakeExec(0, 0, 5, 22.0f, 2);
        VibSetData(VIB_TBL, 7, 1);
        return 1;
    }
    return 0;
}

int em35PlFallCk(cEm35* em)
{
    Vec a;
    Vec b;
    Vec hit;
    Vec nrm;
    Vec d;

    if (pPL->pos.y < em->pos.y + 2000.0f) {
        return 0;
    }
    a = pPL->pos;
    a.y += 500.0f;
    b = em->pos;
    b.y = a.y;
    if (SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0) & 0x00100000) {
        PSVECScale(&nrm, &d, 300.0f);
        PSVECAdd(&hit, &d, &d);
        FSet(pPL->pos.x, d.x);
        pPL->pos.z = d.z;
        pPL->rot.y = atan2f(-nrm.x, -nrm.z);
        SetPlDamage((int) em, plem35DmFall2F);
        return 1;
    }
    return 0;
}

int em35WeakDmCk(cEm35* em)
{
    s16 n = em->dmPart->partsNo;

    if (n == 3) {
        return 1;
    }
    if (n == 4) {
        return 1;
    }
    if (n == 5) {
        return 1;
    }
    return n == 6;
}

// Squared distance of the parts to `pos` below the catch range (em35CatchCk).
#define EM35_CATCH_PARTS_CK(parts)                                                                         \
    p = em->getPartsPtr(parts);                                                                             \
    if ((p->worldPos.x - pos.x) * (p->worldPos.x - pos.x) + (p->worldPos.y - pos.y) * (p->worldPos.y - pos.y) + \
            (p->worldPos.z - pos.z) * (p->worldPos.z - pos.z) <                                            \
        160000.0f) {                                                                                       \
        hit = 1;                                                                                           \
    }

int em35CatchCk(cEm35* em)
{
    Vec pos;
    cModel* p;
    int hit;

    if (em35DeadCk(pPL)) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (!(em->motEvent & 2)) {
        return 0;
    }
    if (pG->flags_5010 & 0x8000) {
        return 0;
    }
    pos = pPL->pos;
    pos.y += 1600.0f;
    hit = 0;
    if (em->motFlags & 0x40) {
        EM35_CATCH_PARTS_CK(0x4D);
        EM35_CATCH_PARTS_CK(0x1A);
        EM35_CATCH_PARTS_CK(0x19);
        EM35_CATCH_PARTS_CK(0x18);
    } else {
        EM35_CATCH_PARTS_CK(0x4A);
        EM35_CATCH_PARTS_CK(0x14);
        EM35_CATCH_PARTS_CK(0x13);
        EM35_CATCH_PARTS_CK(0x12);
    }
    if (hit == 0) {
        return 0;
    }
    pPL->dmg.set(0, 2);
    em->dmg.set(0, 2);
    VibSetData(VIB_TBL, 7, 1);
    return 1;
}

void em35CatchPosSet(cEm35* em)
{
    Vec tbl[3] = {
        { 36500.0f, -7950.0f, -63186.0f },
        { 36500.0f, -7950.0f, -58162.0f },
        { 36500.0f, -7950.0f, -53152.0f },
    };
    u8 kind[3] = { 0, 1, 2 };

    if ((pG->room_id32 & 0xFFFF0000) == 0x011F0000) {
        Vec pos;
        f32 best = 10000000000000000.0f;
        int no = 0;
        int i;

        for (i = 0; i < 3; i++) {
            Vec* p = &tbl[i];
            f32 d = (em->pos.x - p->x) * (em->pos.x - p->x) + (em->pos.y - p->y) * (em->pos.y - p->y) +
                    (em->pos.z - p->z) * (em->pos.z - p->z);

            if (!(d > best)) {
                best = d;
                no = i;
            }
        }
        pos = tbl[no];
        switch (kind[no]) {
        case 0:
        default:
            em->rot.y = PI;
            pos.z += 4819.2f;
            break;
        case 1:
            if (fabsf(em->rot.y) < 1.5707964f) {
                em->rot.y = 0.0f;
                pos.z -= 4819.2f;
            } else {
                em->rot.y = PI;
                pos.z += 4819.2f;
            }
            break;
        case 2:
            em->rot.y = 0.0f;
            pos.z -= 4819.2f;
            break;
        }
        em->setPos(&pos);
    }
}

int em35bPlRunCk(cEm35* em)
{
    Mtx inv;
    Vec v;

    if (pPL->xFC != 0) {
        return 0;
    }
    if (pPL->xFD != 3) {
        return 0;
    }
    if (pG->x4F88 <= 3) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) > 1.5707964f) {
        return 0;
    }
    PSMTXInverse(pPL->mat, inv);
    PSMTXMultVec(inv, &em->pos, &v);
    if (v.x > 1000.0f) {
        return 0;
    }
    if (v.x < -1000.0f) {
        return 0;
    }
    return 1;
}

void em35ClothSet(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->type == 1) {
        w->cloth1.num = 6;
        w->cloth1.pParts = em35ClothP;
        w->cloth1.pLeft = 0;
        w->cloth1.pRight = 0;
        w->cloth1.pUpLeft = 0;
        w->cloth1.x14 = 0;
        w->cloth1.pUp = em35ClothUp;
        w->cloth1.pDown = em35ClothDp;
        w->cloth1.pMax = em35ClothMax;
        w->cloth1.pWindS = 0;
        w->cloth1.pWindR = 0;
        w->cloth1.x20 = 0;
        w->cloth1.pRate = 0;
        w->cloth1.pAt = 0;
        w->cloth1.nAt = 0;
        w->cloth1.x3C = 15.0f;
        w->cloth1.x40 = 0.9f;
        w->cloth1.x44 = 3;
        w->cloth1.x48 = 0.0f;
        w->cloth1.x4C = 0.05f;
        w->cloth1.x50 = 0.0f;
        w->cloth1.flags = 0;
        w->cloth1.x54 = 0;
        PenClothSet(em, (PenCloth*) &w->cloth1, 100.0f);
    }
}

void em35ClothMove(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->type == 1 && !(w->flags & 0x40)) {
        u32 i;

        PenClothMove2(em, (PenCloth*) &w->cloth1);
        for (i = 0x35; i <= 0x41; i++) {
            cParts* p = (cParts*) em->getPartsPtr(i);

            PSMTXConcat(p->pParent->mat, p->worldMat, p->mat);
            p->worldPos.x = p->mat[0][3];
            p->worldPos.y = p->mat[1][3];
            p->worldPos.z = p->mat[2][3];
        }
    }
}

void em35ClothSet2(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->type == 1) {
        w->cloth2.num = 2;
        w->cloth2.pParts = em35ClothP2;
        w->cloth2.pLeft = 0;
        w->cloth2.pRight = 0;
        w->cloth2.pUpLeft = 0;
        w->cloth2.x14 = 0;
        w->cloth2.pUp = em35ClothUp2;
        w->cloth2.pDown = em35ClothDp2;
        w->cloth2.pWindS = 0;
        w->cloth2.pWindR = 0;
        w->cloth2.x20 = 0;
        w->cloth2.pMax = em35ClothMax2;
        w->cloth2.pAt = em35ClothAt2;
        w->cloth2.pRate = em35ClothRate2;
        w->cloth2.nAt = 5;
        w->cloth2.x3C = 15.0f;
        w->cloth2.x40 = 0.8f;
        w->cloth2.x44 = 4;
        w->cloth2.pModel = em;
        w->cloth2.x48 = 0.0f;
        w->cloth2.x4C = 1.0f;
        w->cloth2.x50 = 0.0f;
        w->cloth2.flags = 0x100;
        w->cloth2.x54 = 0;
        PenClothSet(em, (PenCloth*) &w->cloth2, 100.0f);
    }
}

void em35ClothMove2(cEm35* em)
{
    if (em->type == 1) {
        PenClothMove3(em, (PenCloth*) &EM35_WK(em)->cloth2);
    }
}

void em35ClothSet3(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->type == 0) {
        w->cloth2.num = 2;
        w->cloth2.pParts = em35ClothP3;
        w->cloth2.pLeft = 0;
        w->cloth2.pRight = 0;
        w->cloth2.pUpLeft = 0;
        w->cloth2.x14 = 0;
        w->cloth2.pUp = em35ClothUp3;
        w->cloth2.pDown = em35ClothDp3;
        w->cloth2.pWindS = 0;
        w->cloth2.pWindR = 0;
        w->cloth2.x20 = 0;
        w->cloth2.pMax = em35ClothMax3;
        w->cloth2.pRate = em35ClothRate3;
        w->cloth2.nAt = 5;
        w->cloth2.pAt = em35ClothAt3;
        w->cloth2.x3C = 15.0f;
        w->cloth2.x40 = 0.8f;
        w->cloth2.x44 = 4;
        w->cloth2.pModel = em;
        w->cloth2.x48 = 0.0f;
        w->cloth2.x4C = 1.0f;
        w->cloth2.x50 = 0.0f;
        w->cloth2.flags = 0x100;
        w->cloth2.x54 = 0;
        PenClothSet(em, (PenCloth*) &w->cloth2, 100.0f);
    }
}

void em35ClothMove3(cEm35* em)
{
    if (em->type == 0) {
        PenClothMove3(em, (PenCloth*) &EM35_WK(em)->cloth2);
    }
}

void em35NextRtnSetUpper(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    f32 dy = em->pos.y - pPL->pos.y;

    dy = fabsf(dy);
    if (pPL->pos.y > em->pos.y + 1000.0f) {
        if (em35BeamFrontUpCk(em, w->beamNo) && (Rnd() & 1)) {
            EmRoutineSet(em, 1, 0x11, 0, 0);
            return;
        }
        if (em->plDist2 < 100000000.0f && em35BeamUpCk(em, w->beamNo)) {
            EmRoutineSet(em, 1, 0x19, 0, 0);
            return;
        }
    }
    if (pPL->pos.y < em->pos.y - 1000.0f) {
        if (em35BeamFrontDownCk(em, w->beamNo) && (Rnd() & 1)) {
            EmRoutineSet(em, 1, 0x12, 0, 0);
            return;
        }
        if (em->plDist2 < 25000000.0f && em35BeamDownCk(em, w->beamNo)) {
            EmRoutineSet(em, 1, 0x1A, 0, 0);
            return;
        }
    }
    if (em35LockCk(em) && (Rnd() & 1) && w->beamNo != 0xFF && (Rnd() & 1)) {
        if (em35BeamUpCk(em, w->beamNo)) {
            EmRoutineSet(em, 1, 0x19, 0, 0);
            return;
        }
        if (em35BeamDownCk(em, w->beamNo)) {
            EmRoutineSet(em, 1, 0x1A, 0, 0);
            return;
        }
    }
    if (em->plDist2 < 30250000.0f && dy < 500.0f) {
        if (em->plDist2 < 2250000.0f) {
            EmRoutineSet(em, 1, 0x1B, 0, 0);
            return;
        }
        if (em->plDist2 < 9000000.0f && w->routeAngAbs < 0.2617994f) {
            EmRoutineSet(em, 1, 0x1D, 0, 0);
            return;
        }
        if (em->plDist2 < 9000000.0f && w->routeAngAbs < 1.0471976f && (Rnd() & 1)) {
            EmRoutineSet(em, 1, 0x1C, 0, 0);
            return;
        }
        if (em->plDist2 < 9000000.0f && w->routeAngAbs > 2.0943952f && (Rnd() & 1)) {
            EmRoutineSet(em, 1, 0x1C, 0, 0);
            return;
        }
        if (w->routeAngAbs < 0.7330383f) {
            EmRoutineSet(em, 1, 0x1E, 0, 0);
            return;
        }
    }
    if (w->routeAngAbs > 1.5707964f) {
        EmRoutineSet(em, 1, 0x15, 0, 0);
        return;
    }
    if (w->routeAngAbs > 0.7853982f) {
        if (w->routeAng < 0.0f) {
            if (em35BeamSideStepCk(em, 1)) {
                return;
            }
        } else {
            if (em35BeamSideStepCk(em, 0)) {
                return;
            }
        }
    }
    if (em35BeamFrontUpCk(em, w->beamNo) && (Rnd() & 1)) {
        EmRoutineSet(em, 1, 0x11, 0, 0);
        return;
    }
    if (em35BeamFrontDownCk(em, w->beamNo) && (Rnd() & 1)) {
        EmRoutineSet(em, 1, 0x12, 0, 0);
        return;
    }
    if ((Rnd() & 1) && pG->x4F88 > 1 && em35BeamFrontDobuleCk(em, w->beamNo)) {
        EmRoutineSet(em, 1, 0x13, 0, 0);
        return;
    }
    if (em35BeamFrontCk(em, w->beamNo)) {
        EmRoutineSet(em, 1, 0x10, 0, 0);
        return;
    }
    if (em35BeamSideStepCk(em, 1)) {
        return;
    }
    if (em35BeamSideStepCk(em, 0)) {
        return;
    }
    if (w->routeAng < 0.0f) {
        EmRoutineSet(em, 1, 0x16, 0, 1);
    } else {
        EmRoutineSet(em, 1, 0x16, 0, 0);
    }
}

void em35NextRtnSetUpper2(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em35BeamBackCk(em, w->beamNo)) {
        EmRoutineSet(em, 1, 0x14, 0, 1);
        return;
    }
    if (em35BeamUpCk(em, w->beamNo)) {
        EmRoutineSet(em, 1, 0x19, 0, 0);
        return;
    }
    if (em35BeamDownCk(em, w->beamNo)) {
        EmRoutineSet(em, 1, 0x1A, 0, 0);
        return;
    }
    if (em35BeamFrontCk(em, w->beamNo)) {
        EmRoutineSet(em, 1, 0x10, 0, 0);
        return;
    }
    if (w->routeAng > 0.0f) {
        if (em35BeamSideStepCk(em, 1)) {
            return;
        }
    } else {
        if (em35BeamSideStepCk(em, 0)) {
            return;
        }
    }
    EmRoutineSet(em, 1, 0xF, 0, 0);
}

int em35LockCk(cEm35* em)
{
    Mtx inv;
    Vec v;

    if (pPL->xFC != 0) {
        return 0;
    }
    if (pPL->xFD != 6) {
        return 0;
    }
    if (em->plDist2 > 144000000.0f) {
        return 0;
    }
    if (pG->wep_no == 0x10) {
        return 0;
    }
    if (ItemMgr.bulletNumCurrent() == 0) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) > 0.7853982f) {
        return 0;
    }
    PSMTXInverse(pPL->getPartsPtr(0xA)->mat, inv);
    PSMTXMultVec(inv, &em->getPartsPtr(0)->worldPos, &v);
    if (v.x > 0.0f) {
        return 0;
    }
    if (v.z > 500.0f) {
        return 0;
    }
    if (v.z < -500.0f) {
        return 0;
    }
    if (v.y > 500.0f) {
        return 0;
    }
    if (v.y < -500.0f) {
        return 0;
    }
    return 1;
}

int em35GetBeamNo(Vec* pos, int type)
{
    Mtx m;
    Vec v;
    f32 best = 100000000.0f;
    int no = 0xFF;
    u8 i;

    for (i = 0; i < 17; i++) {
        Em35Beam* b = &em35_beam_tbl[i];

        if (b->type == type && !(fabsf(b->a.y - pos->y) > 500.0f)) {
            f32 ang = GetXZAngle(&b->a, &b->b);
            f32 len = SQRTF((b->a.x - b->b.x) * (b->a.x - b->b.x) + (b->a.z - b->b.z) * (b->a.z - b->b.z));

            PSMTXRotRad(m, 'y', ang);
            TransMatrix(m, &b->a);
            PSMTXInverse(m, m);
            PSMTXMultVec(m, pos, &v);
            if (!(v.z < 0.0f) && !(v.z > len)) {
                f32 ax = fabsf(v.x);

                if (!(ax > best)) {
                    best = ax;
                    no = i;
                }
            }
        }
    }
    return no;
}

f32 em35GetBeamDis(Vec* pos, int no, int side, f32 ang)
{
    Em35Beam* b;
    int far;
    f32 d;

    if (no == 0xFF) {
        return 0.0f;
    }
    b = &em35_beam_tbl[no];
    far = 0;
    switch (b->type) {
    case 0:
    default:
        if (ang > 1.5707964f) {
            far = 1;
        }
        if (ang < -1.5707964f) {
            far = 1;
        }
        break;
    case 1:
        if (ang > 0.0f) {
            far = 1;
        }
        break;
    }
    if (side) {
        far ^= 1;
    }
    if (far == 0) {
        d = (pos->x - b->a.x) * (pos->x - b->a.x) + (pos->z - b->a.z) * (pos->z - b->a.z);
    } else {
        d = (pos->x - b->b.x) * (pos->x - b->b.x) + (pos->z - b->b.z) * (pos->z - b->b.z);
    }
    return d;
}

// Two wall probes ahead of the enemy: the way onto the next beam is clear.
#define EM35_BEAM_WAY_CK(em, Y, Z)                                  \
    {                                                               \
        Vec a;                                                      \
        Vec b;                                                      \
                                                                    \
        a.x = 300.0f;                                               \
        a.y = Y;                                                    \
        a.z = 0.0f;                                                 \
        b.x = 300.0f;                                               \
        b.y = Y;                                                    \
        b.z = Z;                                                    \
        PSMTXMultVec((em)->mat, &a, &a);                            \
        PSMTXMultVec((em)->mat, &b, &b);                            \
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x100800)) {           \
            return 0;                                               \
        }                                                           \
        a.x = -300.0f;                                              \
        a.y = Y;                                                    \
        a.z = 0.0f;                                                 \
        b.x = -300.0f;                                              \
        b.y = Y;                                                    \
        b.z = Z;                                                    \
        PSMTXMultVec((em)->mat, &a, &a);                            \
        PSMTXMultVec((em)->mat, &b, &b);                            \
        return SatMgr.hitCheck(&a, &b, 0, 0, 0, 0x100800) == 0;     \
    }

int em35BeamFrontCk(cEm35* em, int no)
{
    Em35Beam* b;
    int next;

    if (no == 0xFF) {
        return 0;
    }
    b = &em35_beam_tbl[no];
    switch (em35BeamDirCk(no, em->rot.y)) {
    case 0:
        next = b->front;
        if (next == 0xFF) {
            return 0;
        }
        break;
    case 1:
        next = b->back;
        if (next == 0xFF) {
            return 0;
        }
        break;
    default:
        return 0;
    }
    EM35_BEAM_WAY_CK(em, 1000.0f, 5500.0f);
}

int em35BeamBackCk(cEm35* em, int no)
{
    Em35Beam* b;
    int next;

    if (no == 0xFF) {
        return 0;
    }
    b = &em35_beam_tbl[no];
    switch (em35BeamDirCk(no, em->rot.y)) {
    case 0:
        next = b->back;
        if (next == 0xFF) {
            return 0;
        }
        break;
    case 1:
        next = b->front;
        if (next == 0xFF) {
            return 0;
        }
        break;
    default:
        return 0;
    }
    EM35_BEAM_WAY_CK(em, 1000.0f, -5500.0f);
}

int em35BeamFrontDobuleCk(cEm35* em, int no)
{
    Em35Beam* b;
    int next;

    if (no == 0xFF) {
        return 0;
    }
    if (em->plDist2 < 100000000.0f) {
        return 0;
    }
    if (em35BeamFrontCk(em, no) == 0) {
        return 0;
    }
    b = &em35_beam_tbl[no];
    switch (em35BeamDirCk(no, em->rot.y)) {
    case 0:
        next = b->front;
        break;
    case 1:
        next = b->back;
        break;
    default:
        return 0;
    }
    if (em35BeamFrontCk(em, next) == 0) {
        return 0;
    }
    return 1;
}

int em35BeamUpCk(cEm35* em, int no)
{
    if (no == 0xFF) {
        return 0;
    }
    if (em->pos.y > -6000.0f) {
        return 0;
    }
    if (em35_beam_tbl[no].up == 0xFF) {
        return 0;
    }
    return 1;
}

int em35BeamDownCk(cEm35* em, int no)
{
    if (no == 0xFF) {
        return 0;
    }
    if (em->pos.y < -6000.0f) {
        return 0;
    }
    if (em35_beam_tbl[no].down == 0xFF) {
        return 0;
    }
    return 1;
}

int em35BeamFrontUpCk(cEm35* em, int no)
{
    Em35Beam* b;
    int next;

    if (no == 0xFF) {
        return 0;
    }
    if (em->pos.y > -6000.0f) {
        return 0;
    }
    if (em35BeamUpCk(em, no) == 0) {
        return 0;
    }
    b = &em35_beam_tbl[no];
    b = &em35_beam_tbl[b->up];
    switch (em35BeamDirCk(no, em->rot.y)) {
    case 0:
        next = b->front;
        if (next == 0xFF) {
            return 0;
        }
        break;
    case 1:
        next = b->back;
        if (next == 0xFF) {
            return 0;
        }
        break;
    default:
        return 0;
    }
    EM35_BEAM_WAY_CK(em, 5000.0f, 5500.0f);
}

int em35BeamFrontDownCk(cEm35* em, int no)
{
    Em35Beam* b;
    int next;

    if (no == 0xFF) {
        return 0;
    }
    if (em->pos.y < -6000.0f) {
        return 0;
    }
    if (em35BeamDownCk(em, no) == 0) {
        return 0;
    }
    b = &em35_beam_tbl[no];
    b = &em35_beam_tbl[b->down];
    switch (em35BeamDirCk(no, em->rot.y)) {
    case 0:
        next = b->back;
        if (next == 0xFF) {
            return 0;
        }
        break;
    case 1:
        next = b->front;
        if (next == 0xFF) {
            return 0;
        }
        break;
    default:
        return 0;
    }
    EM35_BEAM_WAY_CK(em, -3000.0f, 5500.0f);
}

int em35BeamSideStepCk(cEm35* em, int side)
{
    Em35Work* w = EM35_WK(em);
    f32 d = em35GetBeamDis(&em->pos, w->beamNo, side, em->rot.y);
    int ret;

    if (d > 12250000.0f) {
        EmRoutineSet(em, 1, 0x17, 0, side);
        ret = 1;
    } else if (d > 2250000.0f) {
        EmRoutineSet(em, 1, 0x16, 0, side);
        ret = 1;
    } else if (em35BeamSideCk(w->beamNo, side, em->rot.y) == 0) {
        ret = 0;
    } else {
        EmRoutineSet(em, 1, 0x18, 0, side);
        ret = 1;
    }
    return ret;
}

int em35BeamSideCk(int no, int side, f32 ang)
{
    Em35Beam* b;
    int dir;

    if (no == 0xFF) {
        return 0;
    }
    b = &em35_beam_tbl[no];
    dir = em35BeamDirCk(no, ang);
    if (side) {
        switch ((u32) dir) {
        case 0:
            dir = 1;
            break;
        case 1:
            dir = 0;
            break;
        case 2:
            dir = 3;
            break;
        case 3:
            dir = 2;
            break;
        }
    }
    switch ((u32) dir) {
    case 0:
        if (b->side[0] != 0xFF) {
            return 1;
        }
        break;
    case 1:
        if (b->side[1] != 0xFF) {
            return 1;
        }
        break;
    case 2:
        if (b->front != 0xFF) {
            return 1;
        }
        break;
    case 3:
        if (b->back != 0xFF) {
            return 1;
        }
        break;
    }
    return 0;
}

int em35BeamDirCk(int no, f32 ang)
{
    int dir;

    if (no == 0xFF) {
        return 0;
    }
    dir = 0;
    switch (em35_beam_tbl[no].type) {
    case 0:
    default:
        if (ang > 1.5707964f) {
            dir = 1;
        }
        if (ang < -1.5707964f) {
            dir = 1;
        }
        return dir;
    case 1:
        dir = 2;
        if (ang > 0.0f) {
            dir = 3;
        }
        return dir;
    }
}

int em35SetDmVal(cEm35* em)
{
    int near;
    int dmg;

    near = 0;
    if (em->dmPart->rad < 16000000.0f) {
        near = 1;
    }
    {
        u32 no = em->dmWep;

        dmg = 20;
        if (no <= 0x2D) {
            dmg = GetWepDmVal(em, no, near);
        }
    }
    if (em->type == 0 && em35WeakDmCk(em)) {
        dmg *= 2;
    }
    return dmg;
}

void em35BlendMotSet(cEm35* em, void* m0, void* m1, void* m2, void* m3, int a, int b, int kind)
{
    Em35Work* w = EM35_WK(em);
    f32 rate = fabsf(w->blendRate);
    MotionWorkSub* bm;
    void* m;
    int seq;

    MotionSetCore(em, MOTION(em), m0, (int) m3, (u8) w->blendA, (u16) kind, (u16) w->blendB);
    if (w->blendRate < 0.0f) {
        m = m1;
        seq = a;
    } else {
        m = m2;
        seq = b;
    }
    bm = &w->blendMot;
    MotionSetCore(em, bm, m, seq, (u8) w->blendA, (u16) kind, (u16) w->blendB);
    em->blendMot = bm;
    bm->blendRate = rate * (1.0f / 256.0f);
    if (w->blendA) {
        w->blendA--;
    }
    w->blendB++;
    if (w->blendB >= em->frameMax) {
        w->blendB = 0;
    }
}

void em35ScaleMove(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    cModel* p = em->getPartsPtr(0x34);
    f32 s = SINF(w->scaleAng) * 0.3f + 1.0f;

    p->scale.x = s;
    p->scale.y = s;
    p->scale.z = s;
    ScaleMatrix(p->mat, &p->scale);
    if (em->hp > 0) {
        w->scaleAng += 0.15707964f;
        w->scaleAng = LIMIT_ANGLE(w->scaleAng);
    }
}

void cEm35::setDiePose()
{
    atari.throughOn();
    pos.x = 36817.0f;
    pos.y = -7963.56f;
    pos.z = -59757.32f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    MotionSetCore(this, MOTION(this), PL_ARC_PTR(subArc, 0x7B), 0, 0, 1, 0);
    MotionMoveF(this, 0);
    EmRoutineSet(this, 3, 1, 0, 0);
}

void cEm35::setUpperStart()
{
    Em35Work* w = EM35_WK(this);

    pos.x = 36500.0f;
    pos.y = -8000.0f;
    pos.z = -58220.0f;
    rot.x = 0.0f;
    rot.y = PI;
    rot.z = 0.0f;
    MotionSetCore(this, MOTION(this), PL_ARC_PTR(subArc, 0x48), 0, 0, 5, 0);
    MotionMoveF(this, 0);
    w->flags &= ~0x40;
    be_flag |= 0x00200000;
    EmRoutineSet(this, 1, 0xF, 0, 0);
}

int em35BigStepCk(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    Vec a;
    Vec b;

    if (w->targetAngAbs > 0.5235988f) {
        return 0;
    }
    if (w->targetDist < 56250000.0f) {
        return 0;
    }
    if (em->hp > (s16) (em->hpMax / 10) * 8) {
        return 0;
    }
    if (pG->x4F88 > 1) {
        return 0;
    }
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = 10000.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return 0;
    }
    EmRoutineSet(em, 1, 2, 0, 0);
    return 1;
}

void em35WeakInit(cEm35* em)
{
    Em35Work* w = EM35_WK(em);

    if (em->type == 0) {
        Vec pos;
        Vec rot;
        u32 i;

        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        for (i = 0; i < 4; i++) {
            w->pWeak[i] = SetObj00(ARC(0x9B), ARC(0x9C), &pos, &rot);
            if (w->pWeak[i]) {
                w->pWeak[i]->scale.x = 0.7f;
                w->pWeak[i]->scale.y = 0.7f;
                w->pWeak[i]->scale.z = 0.7f;
                w->pWeak[i]->lightInfo.x50 = 0x80;
                OyaSetObj00(w->pWeak[i], em, i + 2);
                w->pWeak[i]->atari.throughOn();
            }
        }
    }
}

void em35WeakMove(cEm35* em)
{
    Em35Work* w = EM35_WK(em);
    u32 i;

    if (em->type != 0) {
        return;
    }
    for (i = 0; i < 4; i++) {
        if (w->pWeak[i]) {
            if ((pGS->flags_5010 & 0x04000000) && em->hp > 0) {
                w->pWeak[i]->be_flag |= 2;
            } else {
                w->pWeak[i]->be_flag &= ~2;
            }
        }
    }
}
