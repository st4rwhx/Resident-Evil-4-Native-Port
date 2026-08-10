// em39 module (D:/Bio4/Prog/em39.cpp): the knife-fight / second-battle boss enemy. Types 0/1 are
// the knife fight (Talk / Success / Failure / the knife routines), type 2 the second battle with the
// machine gun, grenades, the bow and the mutated arm (the T_ routines).

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "map_obj.h"
#include "widget.h"
// emwep.h declares the DOL's plemBackjump (game/emwep.cpp); this unit has a local routine of the
// same name, so the header's declaration is renamed out of the way.
#define plemBackjump plemBackjump_emwep
#include "em39.h"
#include "em10.h"
#undef plemBackjump
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "em_cloth.h"
#include "emdoor.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "route_ck.h"
#include "foot_shadow.h"
#include "act_btn.h"
#include "sscrn.h"
#include "snd.h"
#include "pad.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "pl_npc.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "dbmodule.h"
#include "quake.h"
#include "item.h"
#include "sce_at.h"
#include "game.h"

// The module's 0x34-byte COMMON block (st_room.h): uninitialised template statics of the original
// object, merged into .bss by the REL link.
asm(".comm common_em39,52,4");

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp
extern FootShadowTbl Em39_fs_tbl;     // game/foot_shadow_tbl.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// COMPILER-DIFF #4: the original passes an int SE number to SndCall's u16 parameter without the
// truncation ours emits (`mr` instead of `clrlwi 16`): int-view declaration (em39SetVoice, em39FootEff,
// em39PLVoiceCk).
u32 SndCallI(u16 blk, int no, Vec* pos, int id, int vol, cUnit* obj) asm("SndCall__FUsUsP3VeciiP5cUnit");
// em_set.h declares the empty form; this unit passes the dying enemy (the original prototype
// took it; em_set.cpp ignores its arguments).
void EmSetDieCntE(cEm* em) asm("EmSetDieCnt");
// model.h's member; the enemies call it with the old ModelData (em10.cpp).
extern "C" void cModel_swapModelInfo(cModel* m, ModelData* old, cModelInfo* info) asm("swapModelInfo__6cModelP9ModelDataP10cModelInfo");

static inline void U8Set(u8& d, u8 v) { d = v; }

static void em39_R0_Init(cEm39* em);
static void em39_R0_Move(cEm39* em);
static void em39_R0_Damage(cEm39* em);
static void em39_R0_Die(cEm39* em);
static void em39_R1_br_Dummy(cEm39* em);
static void em39_R1_Talk1st(cEm39* em);
static void em39_R1_Talk2nd(cEm39* em);
static void em39_R1_Success(cEm39* em);
static void plem39_Success(cPlayer* pl);
static void em39_R1_Failure(cEm39* em);
static void plem39_Failure(cPlayer* pl);
static void em39_R1_Wait(cEm39* em);
static void em39_R1_Sit(cEm39* em);
static void em39_R1_SitDown(cEm39* em);
static void em39_R1_WallWait(cEm39* em);
static void em39_R1_Walk(cEm39* em);
static void em39_R1_Run(cEm39* em);
static void em39_R1_Goto(cEm39* em);
static void em39_R1_Turn180(cEm39* em);
static void em39_R1_Threat(cEm39* em);
static void em39_R1_Escape(cEm39* em);
static void em39_R1_Backjump(cEm39* em);
static void em39_R1_Step(cEm39* em);
static void em39_R1_Slant(cEm39* em);
static void em39_R1_Slant2(cEm39* em);
static void em39_R1_SuperDash(cEm39* em);
static void em39_R1_JumpDown(cEm39* em);
static void em39_R1_JumpUp(cEm39* em);
static void em39_R1_JumpUp2(cEm39* em);
static void em39_R1_JumpUp3(cEm39* em);
static void em39_R1_FanceJump(cEm39* em);
static void em39_R1_AtkKnife(cEm39* em);
static void em39_R1_AtkDoor(cEm39* em);
static void em39_R1_br_KnifeCatch(cEm39* em);
static void em39_R1_KnifeCatch(cEm39* em);
static void em39_R1_KnifeHit(cEm39* em);
static void plem39_KnifeHit(cPlayer* pl);
static void em39_R1_Knife4Atk(cEm39* em);
static void plem39_Knife4Atk(cPlayer* pl);
static void em39_R1_Atk_MG(cEm39* em);
static void em39_R1_Reload(cEm39* em);
static void em39_R1_AppearMG(cEm39* em);
static void em39_R1_AppearMG2(cEm39* em);
static void em39_R1_AppearGR(cEm39* em);
static void em39_R1_AppearGR2(cEm39* em);
static void em39_R1_ThrowGR(cEm39* em);
static void em39_R1_AppearBow(cEm39* em);
static void em39_R1_Flash(cEm39* em);
static void em39_R1_Hide(cEm39* em);
static void em39_R1_br_T_Atk(cEm39* em);
static void em39_R1_T_Atk(cEm39* em);
static void em39_R1_T_BackKnuckle(cEm39* em);
static void em39_R1_br_T_LongAtk(cEm39* em);
static void em39_R1_T_LongAtk(cEm39* em);
static void em39_R1_T_JumpAtk(cEm39* em);
static void plem39_Stamp(cPlayer* pl);
static void em39SitAction(cEm39* em);
static void plem39Sit(cPlayer* pl);
static void em39BackjumpAction(cEm39* em);
static void plemBackjump(cPlayer* pl);
static void em39_R1_br_T_Kick(cEm39* em);
static void em39_R1_T_Kick(cEm39* em);
static void em39_R1_br_T_LowKick(cEm39* em);
static void em39_R1_T_LowKick(cEm39* em);
static void em39_R1_T_LowKickHit(cEm39* em);
static void plem39_LowKickHit(cPlayer* pl);
static void em39_R1_T_CliffAtk(cEm39* em);
static void plem39_CliffAtk(cPlayer* pl);
static void em39_R1_Dm_Normal(cEm39* em);
static void em39_R1_Dm_Head(cEm39* em);
static void em39_R1_Dm_Blow(cEm39* em);
static void em39_R1_Dm_T_Head(cEm39* em);
static void em39_R1_Dm_T_Down(cEm39* em);
static void em39_R1_Dm_T_DownHead(cEm39* em);
static void em39_R1_Die_Normal(cEm39* em);
static void em39_R1_Die_Flash(cEm39* em);
static void em39ActOn(cEm39* em);
static void plemDmSide(cPlayer* pl);

#define ARC(no) PL_ARC_PTR(em->subArc, no)

// Collision flag bits set / cleared through the info's address (`addi rX, em, 0x2b4; lhz 0x1a(rX)`).
static inline void AtariOn(cAtariInfo* at, u16 b) { at->flags |= b; }
static inline void AtariOff(cAtariInfo* at, u16 mask) { at->flags &= mask; }
// u16 reference RMW: keeps the following `lwz pPL` below the `sth` (plem39_CliffAtk).
static inline void AtariOffR(u16& f, u16 mask) { f &= mask; }

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

// Dead flag test (cDmgInfo upper 16 bits): an inline returning 0/1 gives the `li 1; andis.; bne; li 0` chain.
static inline int em39DeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

// Struct-member views of the player / partner pointers: a load through them is not hoisted above
// the preceding stores (cam_ctrl.cpp PlayerPtr).
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)
struct SubCharPtr {
    cSubChar* p;
};
#define pSUBS (((SubCharPtr*) &pSUB)->p)

// Routine test on the cModel status word (xFC / xFD as the upper half of `stat`).
#define EM_RTN(em, fc, fd) (((em)->stat & 0xFFFF0000) == (u32) (((fc) << 24) | ((fd) << 16)))

extern "C" void _prolog()
{
    OSReport("em39 prolog Ok\n");
    EmInitFunc = Em39Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em39Init(cEm* em)
{
    new (em) cEm39();
}

cEm39::~cEm39()
{
    Em39Work* w = EM39_WK(this);

    if (w->pWep && w->pWep->isAlive()) {
        EmMgr.destroy(w->pWep);
    }
    if (w->pWep2 && w->pWep2->isAlive()) {
        EmMgr.destroy(w->pWep2);
    }
    if (w->pWep3 && w->pWep3->isAlive()) {
        EmMgr.destroy(w->pWep3);
    }
    if (w->x58C && w->x58C->isAlive()) {
        EmMgr.destroy(w->x58C);
    }
    if (w->pObj12 && w->pObj12->isAlive()) {
        ObjMgr.destroy(w->pObj12);
    }
}

void cEm39::setNoSuspend(int on)
{
    Em39Work* w = EM39_WK(this);

    if (on) {
        be_flag |= 0x800;
    } else {
        be_flag &= ~0x800;
    }
    if (w->pWep) {
        w->pWep->setNoSuspend(on);
    }
    if (w->pWep2) {
        w->pWep2->setNoSuspend(on);
    }
    if (w->pWep3) {
        w->pWep3->setNoSuspend(on);
    }
    if (w->x58C) {
        w->x58C->setNoSuspend(on);
    }
    if (w->pObj12) {
        w->pObj12->setNoSuspend(on);
    }
}

void em39DmCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    EmHitInfo* hit;
    int dmg;

    if (em->hp > 0 && !em39DeadCk(em)) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 4:
        case 5:
        case 7:
            if (w->x8A0 == 0) {
                if (w->flags & 0x100) {
                    return;
                }
                w->x8A0 = 120;
                if (em->type != 2) {
                    LifeDownSet2(em, 200, 0, 1);
                    w->dmgTotal += 200;
                    w->x698 += 200;
                    w->x69C += 200;
                } else {
                    LifeDownSet2(em, 100, 0, 0);
                    w->dmgTotal += 100;
                    w->x698 += 100;
                    w->x69C += 100;
                }
                if (em->hp <= 0) {
                    EmSetDie(em);
                    EmSetDieCntE(em);
                    if (pG->room_id == 0x31C) {
                        return;
                    }
                    EmRoutineSet(em, 3, 1, 0, 0);
                    return;
                }
                if (w->flags & 8) {
                    return;
                }
                if (em->type == 2) {
                    if (w->flags & 0x1000) {
                        w->x698 = 0;
                        EmRoutineSet(em, 2, 1, 0, 0);
                    } else {
                        w->x698 = 0;
                        EmRoutineSet(em, 2, 4, 0, 0);
                    }
                } else {
                    EmRoutineSet(em, 2, 0, 0, 0);
                }
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
    hit = em->dmPart;
    if (em39GuardCk(em)) {
        EmDmBloodSet2(em, 0x2F, 0x2D, 0, 0, 0);
        SndCall(8, 0x4C, &pPL->pos, em->id, 0, pPL);
        return;
    }
    dmg = em39SetDmVal(em);
    if (em->type != 2) {
        LifeDownSet2(em, dmg, 0, 1);
    } else {
        LifeDownSet2(em, dmg, 0, 0);
    }
    w->dmgTotal += dmg;
    em39BloodSet(em);
    SndCall(8, 0xD, &em->pos, em->id, 0, em);
    if (em->hp <= 0) {
        EmSetDie(em);
        EmSetDieCntE(em);
        if (pG->room_id == 0x31C) {
            return;
        }
        EmRoutineSet(em, 3, 1, 0, 0);
        return;
    }
    if (w->flags & 0x100) {
        return;
    }
    w->x698 += dmg;
    w->x69C += dmg;
    if (em->type == 2) {
        if (hit->partsNo == 5) {
            w->x698 = 0;
            if (w->flags & 0x10000) {
                EmRoutineSet(em, 2, 5, 0, 0);
            } else {
                EmRoutineSet(em, 2, 3, 0, 0);
            }
            return;
        }
        if (w->flags & 0x1000) {
            return;
        }
        if (w->x698 <= 200) {
            return;
        }
        w->x698 = 0;
        EmRoutineSet(em, 2, 4, 0, 0);
        return;
    }
    if (w->flags & 8) {
        return;
    }
    switch (em->dmWep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0x11:
    case 0x14:
    case 0x15:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        if (hit->partsNo == 5) {
            EmRoutineSet(em, 2, 1, 0, 0);
        } else if (w->x698 > 200) {
            w->x698 = 0;
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    case 9:
    case 0xA:
    case 0x28:
        if (hit->partsNo == 5) {
            EmRoutineSet(em, 2, 1, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    case 0x10:
    case 0x1A:
        if (hit->partsNo == 5) {
            EmRoutineSet(em, 2, 1, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (hit->partsNo == 5) {
            EmRoutineSet(em, 2, 1, 0, 0);
        } else if (w->x698 > 200) {
            w->x698 = 0;
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    case 5:
    case 6:
    case 0xF:
    case 0x13:
    case 0x29:
    case 0x2C:
    case 0x2D:
        if (hit->partsNo == 5) {
            EmRoutineSet(em, 2, 1, 0, 0);
        } else {
            EmRoutineSet(em, 2, 0, 0, 0);
        }
        break;
    case 0xE:
        break;
    case 0xD:
    case 0x12:
    default:
        EmRoutineSet(em, 2, 2, 0, 0);
        break;
    case 0x17:
    case 0x2A:
        EmRoutineSet(em, 2, 1, 0, 0);
        break;
    }
}

Em39Func Em39_R0_move_tbl[4] = {
    em39_R0_Init,
    em39_R0_Move,
    em39_R0_Damage,
    em39_R0_Die,
};

// Routine 1 handlers: the branch check and the move of each sub-routine (cModel::xFD), as one
// flat table (`tbl[xFD * 2]` / `tbl[xFD * 2 + 1]`: `slwi 3; ori 4`).
static Em39Func Em39_R1_move_tbl[94] = {
    em39_R1_br_Dummy, em39_R1_Talk1st,  // 0x00
    em39_R1_br_Dummy, em39_R1_Talk2nd,  // 0x01
    em39_R1_br_Dummy, em39_R1_Success,  // 0x02
    em39_R1_br_Dummy, em39_R1_Failure,  // 0x03
    em39_R1_br_Dummy, em39_R1_Wait,  // 0x04
    em39_R1_br_Dummy, em39_R1_Sit,  // 0x05
    em39_R1_br_Dummy, em39_R1_SitDown,  // 0x06
    em39_R1_br_Dummy, em39_R1_WallWait,  // 0x07
    em39_R1_br_Dummy, em39_R1_Walk,  // 0x08
    em39_R1_br_Dummy, em39_R1_Run,  // 0x09
    em39_R1_br_Dummy, em39_R1_Goto,  // 0x0A
    em39_R1_br_Dummy, em39_R1_Turn180,  // 0x0B
    em39_R1_br_Dummy, em39_R1_Threat,  // 0x0C
    em39_R1_br_Dummy, em39_R1_Escape,  // 0x0D
    em39_R1_br_Dummy, em39_R1_Backjump,  // 0x0E
    em39_R1_br_Dummy, em39_R1_Step,  // 0x0F
    em39_R1_br_Dummy, em39_R1_Slant,  // 0x10
    em39_R1_br_Dummy, em39_R1_Slant2,  // 0x11
    em39_R1_br_Dummy, em39_R1_SuperDash,  // 0x12
    em39_R1_br_Dummy, em39_R1_JumpDown,  // 0x13
    em39_R1_br_Dummy, em39_R1_JumpUp,  // 0x14
    em39_R1_br_Dummy, em39_R1_JumpUp2,  // 0x15
    em39_R1_br_Dummy, em39_R1_JumpUp3,  // 0x16
    em39_R1_br_Dummy, em39_R1_FanceJump,  // 0x17
    em39_R1_br_Dummy, em39_R1_AtkKnife,  // 0x18
    em39_R1_br_Dummy, em39_R1_AtkDoor,  // 0x19
    em39_R1_br_KnifeCatch, em39_R1_KnifeCatch,  // 0x1A
    em39_R1_br_Dummy, em39_R1_KnifeHit,  // 0x1B
    em39_R1_br_Dummy, em39_R1_Knife4Atk,  // 0x1C
    em39_R1_br_Dummy, em39_R1_Atk_MG,  // 0x1D
    em39_R1_br_Dummy, em39_R1_Reload,  // 0x1E
    em39_R1_br_Dummy, em39_R1_AppearMG,  // 0x1F
    em39_R1_br_Dummy, em39_R1_AppearMG2,  // 0x20
    em39_R1_br_Dummy, em39_R1_AppearGR,  // 0x21
    em39_R1_br_Dummy, em39_R1_AppearGR2,  // 0x22
    em39_R1_br_Dummy, em39_R1_ThrowGR,  // 0x23
    em39_R1_br_Dummy, em39_R1_AppearBow,  // 0x24
    em39_R1_br_Dummy, em39_R1_Flash,  // 0x25
    em39_R1_br_Dummy, em39_R1_Hide,  // 0x26
    em39_R1_br_T_Atk, em39_R1_T_Atk,  // 0x27
    em39_R1_br_Dummy, em39_R1_T_BackKnuckle,  // 0x28
    em39_R1_br_T_LongAtk, em39_R1_T_LongAtk,  // 0x29
    em39_R1_br_Dummy, em39_R1_T_JumpAtk,  // 0x2A
    em39_R1_br_T_Kick, em39_R1_T_Kick,  // 0x2B
    em39_R1_br_T_LowKick, em39_R1_T_LowKick,  // 0x2C
    em39_R1_br_Dummy, em39_R1_T_LowKickHit,  // 0x2D
    em39_R1_br_Dummy, em39_R1_T_CliffAtk,  // 0x2E
};

static Em39Func Em39_R1_dmg_tbl[6] = {
    em39_R1_Dm_Normal,
    em39_R1_Dm_Head,
    em39_R1_Dm_Blow,
    em39_R1_Dm_T_Head,
    em39_R1_Dm_T_Down,
    em39_R1_Dm_T_DownHead,
};

static Em39Func Em39_R1_die_tbl[2] = {
    em39_R1_Die_Normal,
    em39_R1_Die_Flash,
};

// Parts index remap of the flipped motions (cModel::motFlip).
static u16 em39_flip_tbl[120] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0A, 0x11, 0x16, 0x17, 0x18, 0x19, 0x12, 0x13, 0x14, 0x15, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x21, 0x20, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,
    0x3F, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
};

// Machine gun damage handed to EmAtkSetDamagePL (em39GunHitCk).
static EmAtkInfo em39_gun_atk_info = { 300.0f, 8, 800, 0, 0xA, 0 };

// Melee attack table (em39AtkCk / em39AtkCk2 index it by attack number).
static EmAtkInfo em39_atk_tbl[10] = {
    { 300.0f, 8, 1500, 0, 0xA, 0 },
    { 200.0f, 8, 800, 0, 0xA, 0 },
    { 6000.0f, 8, 1000, 0, 0xA, 0 },
    { 200.0f, 8, 1000, 0, 0xA, 0 },
    { 200.0f, 8, 1000, 0, 0xA, 0 },
    { 200.0f, 8, 1500, 0, 0xA, 0 },
    { 200.0f, 8, 1500, 0, 0xA, 0 },
    { 200.0f, 8, 500, 0, 0xA, 0 },
    { 200.0f, 8, 500, 0, 0xA, 0 },
    { 200.0f, 8, 500, 0, 0xA, 0 },
};

void cEm39::move()
{
    Em39Work* w = EM39_WK(this);
    f32 dist;

    if (xFC) {
        em39DmCk(this);
    }
    em39PLNearTowerCk(this);
    w->flags &= 0xFE6AEA20;
    if (w->x680) {
        w->x680--;
    }
    if (w->x684) {
        w->x684--;
    }
    if (w->x688) {
        w->x688--;
    }
    if (w->x694) {
        w->x694--;
    }
    if (w->x898) {
        w->x898--;
    }
    if (w->x8A0) {
        w->x8A0--;
    }
    if (w->x6A0) {
        w->x6A0--;
    }
    if ((w->flags & 0x800) && w->x89C) {
        w->x89C--;
    }
    clearStatus(3);
    em39RouteCk(this);
    Em39_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    if (xFC) {
        em39ArmControl(this);
    }
    em39NeckMove(this);
    em39WaistMove(this);
    partsWorldCalc();
    dist = SQRTF((oldPos.x - pos.x) * (oldPos.x - pos.x) + (oldPos.z - pos.z) * (oldPos.z - pos.z));
    if (seFlags28B & 0x40) {
        atari.flags |= 0x10;
    } else {
        atari.flags &= ~0x10;
    }
    EmAtCheck(this);
    atari.move();
    if (!(seFlags28B & 0x40)) {
        SatMgr.check(this, 0);
    }
    if (SQRTF((pos.x - oldPos.x) * (pos.x - oldPos.x) + (pos.z - oldPos.z) * (pos.z - oldPos.z)) < dist * 0.5f) {
        w->stuckCnt++;
    } else {
        w->stuckCnt = 0;
    }
    if (w->flags & 0x400) {
        alpha = 0.0f;
    } else {
        alpha += 0.1f;
        if (alpha > 1.0f) {
            alpha = 1.0f;
        }
    }
    if (seFlags28B & 0x40) {
        if (shdCol <= 0xF6) {
            shdCol += 8;
        } else {
            shdCol = 0xFF;
        }
    } else {
        if (shdCol > 8) {
            shdCol -= 8;
        } else {
            shdCol = 0;
        }
    }
    em39MarkerMove(this);
    em39VoiceMove(this);
    em39SpeechMove(this);
    if (be_flag & 2) {
        if (pG->flags_500C & 0x00800000) {
            w->x8A4 = 0;
        } else {
            w->x8A4++;
        }
    } else {
        w->x8A4 = 0;
    }
    em39FootEff(this);
    em39PLVoiceCk(this);
    if (pG->room_id != 0x31C) {
        EM_LIST(emsetNo)->hp = hp;
    }
    {
        int hide = 0;
        Camera* cam = &pG->Cam;
        Vec* nrm = pFloorNrm;
        if (nrm == 0 || nrm->y < 0.8f) {
            hide = 1;
        }
        if (cam->param.pos.y < pos.y) {
            hide = 1;
        }
        if ((w->flags & 0x01000000) || hide) {
            if (shdCol <= 0xF6) {
                shdCol += 8;
            } else {
                shdCol = 0xFF;
            }
        } else {
            if (shdCol > 8) {
                shdCol -= 8;
            } else {
                shdCol = hide;
            }
        }
    }
}

static void em39_R0_Init(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    cModelInfo* info;
    Vec pos;
    Vec rot;
    int one;
    u8 zero;   // only byte fields take it: a promoted QI variable stores directly (`stb r30`)
    // COMPILER-DIFF: #13 -- the original never allocates its REG_EQUIV constants: the shared literal
    // zero is reload's callee-saved r20, the 0x1D/300/10/-1/450 init constants its spill registers
    // r0/r8/r11/r9/r10 and the subArc load of the routine MotionSetCore its r11, and the post-call
    // init block is issued in pure source order (no store carries a register death). Pinned here.
    register int z0 asm("r20");
    register int r0c asm("r0");
    register int r8c asm("r8");
    register int r9c asm("r9");
    register int r10c asm("r10");
    register int r11c asm("r11");
    register PlArc* arc11 asm("r11");

    switch (em->type) {
    case 1:
    default:
        if (em->modelInit(ARC(0x15), ARC(0x17)) == 0) {
            pLog->err(0, 0, "em39() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        info = ModInfoMgr.create(ARC(5), ARC(0x10));
        if (info) {
            em->addModel(info);
        }
        info = ModInfoMgr.create(ARC(6), ARC(0x18));
        if (info) {
            em->addModel(info);
        }
        info = ModInfoMgr.create(ARC(7), ARC(0x12));
        if (info) {
            em->addModel(info);
        }
        {
            Vec pos2;
            Vec rot2;

            pos2.x = 0.0f;
            pos2.y = 146.0f;
            pos2.z = 22.6f;
            rot2.x = 0.0f;
            rot2.y = 0.0f;
            rot2.z = 0.0f;
            w->pObj12 = SetObj12(ARC(0x16), ARC(0x19), &pos2, &rot2);
            if (w->pObj12) {
                ((cObj12*) w->pObj12)->setParent(em, 4, 1);
            }
        }
        w->x66C = ModInfoMgr.create(ARC(9), ARC(0x10));
        if (w->x66C) {
            em->addModel(w->x66C);
        }
        w->handType = 0xFF;
        em39HandSet(em, 1);
        break;
    case 2:
        if (em->modelInit(ARC(0x1A), ARC(0x1D)) == 0) {
            pLog->err(0, 0, "em39() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        info = ModInfoMgr.create(ARC(5), ARC(0x10));
        if (info) {
            em->addModel(info);
        }
        info = ModInfoMgr.create(ARC(6), ARC(0x1E));
        if (info) {
            em->addModel(info);
        }
        info = ModInfoMgr.create(ARC(0x1B), ARC(0x1F));
        if (info) {
            em->addModel(info);
        }
        info = ModInfoMgr.create(ARC(0x1C), ARC(0x20));
        if (info) {
            em->addModel(info);
        }
        w->x66C = ModInfoMgr.create(ARC(9), ARC(0x10));
        if (w->x66C) {
            em->addModel(w->x66C);
        }
        w->handType = 0xFF;
        em39HandSet(em, 1);
        break;
    }
    em->be_flag |= 0x01000000;
    w->pWep = 0;
    if (em->type != 2) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pWep = SetWeapon(ARC(0x26), ARC(0x25), &pos, &rot, 0);
        if (w->pWep) {
            w->pWep->setParent(em, 0xA, 0);
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
        }
    }
    w->pWep2 = 0;
    if (em->type != 2) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pWep2 = SetWeapon(ARC(0x2C), ARC(0x2B), &pos, &rot, 0);
        if (w->pWep2) {
            w->pWep2->setParent(em, 0xA, 0);
            w->pWep2->setTransMode(0);
        }
    }
    w->pWep3 = 0;
    if (em->type != 2) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pWep3 = SetWeapon(ARC(0x32), ARC(0x31), &pos, &rot, 0);
        if (w->pWep3) {
            w->pWep3->setParent(em, 0x10, 0);
            w->pWep3->setTransMode(0);
        }
    }
    z0 = 0;
    w->x58C = (cEmWep*) z0;
    em->pFootShadowTbl = &Em39_fs_tbl;
    em->motFlip = em39_flip_tbl;
#line 1268 "D:/Bio4/Prog/em39.cpp"
    em->p2A4 = MEM_ALLOC(0x98, 1, 0xD);
    // GNU constructor expressions: emitted at the statement like strings, and shared through the
    // constant hash (plem39_CliffAtk reuses this zero vector).
    em->lightInfo.init2(0, 1, &((Vec) { 0.0f, 0.0f, 0.0f }), &((Vec) { 10000.0f, 10000.0f, 10000.0f }), 2);
    atariInitF(&em->atari, 0.0f, 1000.0f, 0.0f, 500.0f, 400.0f, 400.0f, 1000.0f, 1, 0x2000, 10);   // COMPILER-DIFF: #1
    em->litArea.on(1);
    YarareInit(em, 0.0f, 0.0f, 0.0f, 130.0f, 100.0f, 5, 1);
    YarareAdd(em, &w->hit[0], 0.0f, -30.0f, 0.0f, 200.0f, 300.0f, 2, 1);
    YarareAdd(em, &w->hit[1], -20.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x14, 1);
    one = 1;
    YarareAdd(em, &w->hit[2], 20.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x18, 1);
    YarareAdd(em, &w->hit[3], -300.0f, 0.0f, 0.0f, 100.0f, 400.0f, 9, 3);
    YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 100.0f, 400.0f, 0xF, 3);
    YarareAdd(em, &w->hit[5], -20.0f, -300.0f, 0.0f, 170.0f, 300.0f, 0x13, 1);
    YarareAdd(em, &w->hit[6], 20.0f, -300.0f, 0.0f, 170.0f, 300.0f, 0x17, 1);
    YarareAdd(em, &w->hit[7], -300.0f, 0.0f, 0.0f, 120.0f, 300.0f, 8, 3);
    YarareAdd(em, &w->hit[8], 0.0f, 0.0f, 0.0f, 120.0f, 300.0f, 0xE, 3);
    if (em->type == 2) {
        YarareAdd(em, &w->hit[9], 0.0f, 0.0f, 0.0f, 50.0f, 60.0f, 0x62, 3);
        YarareAdd(em, &w->hit[10], 0.0f, 0.0f, 0.0f, 50.0f, 60.0f, 0x63, 3);
        YarareAddCube(em, &w->hit[11], 200.0f, -50.0f, 0.0f, 450.0f, 70.0f, 100.0f, 0x64, 3);
        YarareAdd(em, &w->hit[12], 0.0f, 0.0f, 0.0f, 50.0f, 60.0f, 0x65, 3);
        YarareAddCube(em, &w->hit[13], -120.0f, -30.0f, -50.0f, 250.0f, 60.0f, 100.0f, 0x66, 3);
        YarareAddCube(em, &w->hit[14], -120.0f, -30.0f, 0.0f, 250.0f, 60.0f, 50.0f, 0x7B, 3);
        YarareAddCube(em, &w->hit[15], -120.0f, -30.0f, 0.0f, 300.0f, 60.0f, 50.0f, 0x7C, 3);
        YarareAddCube(em, &w->hit[16], -120.0f, -30.0f, 0.0f, 300.0f, 60.0f, 50.0f, 0x7D, 3);
        YarareAddCube(em, &w->hit[17], -120.0f, -30.0f, 0.0f, 400.0f, 60.0f, 50.0f, 0x7E, 3);
        YarareAddCube(em, &w->hit[18], -120.0f, -30.0f, 0.0f, 250.0f, 60.0f, 50.0f, 0x7F, 3);
    }
    em->lockParts = 2;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(4), 0x2F, 0);
    r0c = 0x1D;
    r8c = 300;
    r11c = 10;
    r9c = -1;
    r10c = 450;
    w->x59C = 0.0f;
    w->pDoor = 0;
    w->flags = z0;
    w->x598 = 0.0f;
    w->x680 = z0;
    w->x8B5 = z0;
    w->pGotoPoint = 0;
    w->x8BB = z0;
    w->x898 = z0;
    w->dmgTotal = z0;
    w->x8A0 = z0;
    w->x69C = z0;
    zero = 0;
    asm("" : "+r"(zero) : "r"(z0), "f"(0.0f)); // COMPILER-DIFF: #13
    w->x8C4 = zero;
    w->x8A8 = r0c;
    w->x684 = r8c;
    w->x678 = r11c;
    w->x8C0 = r9c;
    w->x89C = r10c;
    w->x688 = r10c;
    w->espKind = EspPullCoreKind();
    w->espKind2 = EspPullCoreKind();
    if (em->type != 2) {
        int rtn;

        em->setStatus(5);
        rtn = em->x38D;
        // The routine bytes are stored in every arm (jump2 cross-jumps the identical tails): in their own
        // blocks the QI stores precede the subArc load, and the dying `xFF` store leads the `xFE` one.
        switch (rtn) {
        case 0:
        default:
            rtn = 0x26;
            em->xFC = one;
            em->xFD = rtn;
            em->xFE = zero;
            em->xFF = zero;
            break;
        case 1:
            rtn = 4;
            em->xFC = one;
            em->xFD = rtn;
            em->xFE = zero;
            em->xFF = zero;
            break;
        case 4:
            em->xFC = one;
            em->xFD = rtn;
            em->xFE = zero;
            em->xFF = zero;
            break;
        }
        arc11 = em->subArc;
        MotionSetCore(em, MOTION(em), PL_ARC_PTR(arc11, 0x73), (int) PL_ARC_PTR(arc11, 0x74), 0, 1, 0);
    } else {
        int no;

        em->setStatus(5);
        no = em->x38D;
        switch (no) {
        case 0:
        default:
            em->xFC = one;
            em->xFD = 4;
            em->xFE = zero;
            em->xFF = zero;
            break;
        case 1:
            em->xFC = one;
            em->xFD = 4;
            em->xFE = zero;
            em->xFF = zero;
            break;
        case 2:
            em->xFC = one;
            em->xFD = no;
            em->xFE = zero;
            em->xFF = zero;
            break;
        case 3:
            em->xFC = one;
            em->xFD = no;
            em->xFE = zero;
            em->xFF = zero;
            break;
        }
        MotionSetCore(em, MOTION(em), ARC(0xF5), 0, 0, 1, 0);
    }
    MotionMoveF(em, 0);
    em39_R0_Move(em);
}

void em39HandSet(cEm39* em, int type)
{
    Em39Work* w = EM39_WK(em);
    cModelInfo* info;
    void* bin;

    if (w->handType == type) {
        return;
    }
    w->handType = type;
    switch ((u32) type) {
    case 0:
    default:
        bin = ARC(0xA);
        break;
    case 1:
        bin = ARC(0xB);
        break;
    case 2:
        bin = ARC(0xC);
        break;
    case 3:
        bin = ARC(0xD);
        break;
    }
    info = ModInfoMgr.create(bin, ARC(0x14));
    if (info) {
        if (w->pHandInfo) {
            cModel_swapModelInfo(em, w->pHandInfo->pData, info);
        } else {
            em->addModel(info);
        }
        w->pHandInfo = info;
        info->be_flag |= 8;
    }
    if (em->type == 2) {
        return;
    }
    switch ((u32) type) {
    case 0:
    default:
        bin = ARC(0xE);
        break;
    case 1:
        bin = ARC(0xE);
        break;
    case 2:
        bin = ARC(0xF);
        break;
    case 3:
        bin = ARC(0xE);
        break;
    }
    info = ModInfoMgr.create(bin, ARC(0x14));
    if (info) {
        if (w->pHandInfo2) {
            cModel_swapModelInfo(em, w->pHandInfo2->pData, info);
        } else {
            em->addModel(info);
        }
        w->pHandInfo2 = info;
        info->be_flag |= 8;
    }
}

void em39DieModelSet(cEm39* em)
{
    cModelInfo* info = ModInfoMgr.create(ARC(0x21), ARC(0x22));

    if (info) {
        cModel_swapModelInfo(em, em->pInfo->pData, info);
    }
}

static void em39_R0_Move(cEm39* em)
{
    Em39_R1_move_tbl[em->xFD * 2](em);
    Em39_R1_move_tbl[em->xFD * 2 + 1](em);
}

static void em39_R1_br_Dummy(cEm39* em)
{
}

static void em39_R1_Talk1st(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    em->dmType = 2;
    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        em->pos.x = 29231.0f;
        em->pos.y = 8658.0f;
        em->pos.z = -10844.0f;
        em->rot.y = 1.8654078f;
        MotionSetCore(em, MOTION(em), ARC(0xAB), 0, 0, 5, 0);
        em->be_flag |= 2;
        AtariOff(&em->atari, 0xFCFF);
        em39WepSet(em, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        break;
    }
}

static void em39_R1_Talk2nd(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    em->dmType = 2;
    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        em->pos.x = -719.0f;
        em->pos.y = 2000.0f;
        em->pos.z = -4022.0f;
        em->rot.y = -1.9547688f;
        MotionSetCore(em, MOTION(em), ARC(0xAB), 0, 0, 5, 0);
        em->be_flag |= 2;
        AtariOff(&em->atari, 0xFCFF);
        em39WepSet(em, 0);
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        break;
    }
}

static void em39_R1_Success(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    em->dmType = 2;
    switch (em->xFE) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em->pos.x = 220.09f;
        em->pos.y = 12000.0f;
        em->pos.z = -9402.42f;
        em->rot.y = -0.9032079f;
        MotionSetCore(em, MOTION(em), ARC(0xF1), (int) ARC(0xF2), 0, 1, 0);
        SetPlDamage((int) em, plem39_Success);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x43, 0, w->espKind, (u32) em, 0);
        w->x8BB = 6;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            w->flags |= 0x00800000;
            AtariOn(&em->atari, 0x300);
            EmRoutineSet(em, 1, 4, 0, 0);
        } else if (em->seFlags28B & 1) {
            AtariOn(&em->atari, 0x300);
            w->x8BB = 0;
        }
        break;
    }
    em39HandSet(em, 1);
}

static void plem39_Success(cPlayer* pl)
{
    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 0xA);
    pl->subArc = ((cEm*) pl->dmgType)->subArc;
    switch (pl->xFE) {
    case 0:
        AtariOff(&pl->atari, 0xFCFF);
        pl->pos.x = -1751.14f;
        pl->pos.y = 12000.0f;
        pl->pos.z = -7649.44f;
        pl->rot.y = 3.1415927f;
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x124), 0, 0, 1, 0);
        pl->pWep->setTrans(0, 0);
        pl->xFE++;
    case 1:
        if (MotionMoveF(pl, 0)) {
            pl->pWep->setTrans(1, 0);
            AtariOn(&pl->atari, 0x300);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em39_R1_Failure(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    em->dmType = 2;
    switch (em->xFE) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em->pos.x = 94.95f;
        em->pos.y = 12000.0f;
        em->pos.z = -9417.92f;
        em->rot.y = -0.8901179f;
        MotionSetCore(em, MOTION(em), ARC(0xF3), (int) ARC(0xF4), 0, 1, 0);
        SetPlDamage((int) em, plem39_Failure);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x44, 0, w->espKind, (u32) em, 0);
        w->x8BB = 6;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (em39GetCliffPos(em)) {
                em->stat = 0x012E0000;
            } else {
                AtariOn(&em->atari, 0x300);
                EmRoutineSet(em, 1, 4, 0, 0);
            }
        }
        break;
    }
    em39HandSet(em, 1);
}

static void plem39_Failure(cPlayer* pl)
{
    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 0xA);
    pl->subArc = ((cEm*) pl->dmgType)->subArc;
    switch (pl->xFE) {
    case 0:
        AtariOff(&pl->atari, 0xFCFF);
        pl->pos.x = -1736.93f;
        pl->pos.y = 12000.0f;
        pl->pos.z = -7639.26f;
        pl->rot.y = 2.8536134f;
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x125), 0, 0, 1, 0);
        EstSet((int) pl, -1, 0, 0, 0x2F, 0x45, 0, 0, (u32) pl, 0);
        PlSetFace(1);
        pl->pWep->setTrans(0, 0);
        pl->xFE++;
    case 1:
        MotionMoveF(pl, 0);
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em39_R1_Wait(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->flags |= 0x30;
    if (em->type != 2 && em->xFE == 0) {
        em->xFE = 2;
    }
    switch (em->xFE) {
    case 0:
        em->be_flag |= 2;
        AtariOn(&em->atari, 0x300);
        MotionSetCore(em, MOTION(em), ARC(0xF5), (int) ARC(0xF6), 3, 1, 0xA);
        w->x8BB = 8;
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
        }
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        if (em->type != 2) {
            MotionSetCore(em, MOTION(em), ARC(0x73), (int) ARC(0x74), 0x1E, 5, 0);
        } else {
            w->x8BB = 0;
            MotionSetCore(em, MOTION(em), ARC(0xD6), 0, 0x1E, 5, 0);
        }
        em->be_flag |= 2;
        AtariOn(&em->atari, 0x300);
        w->x8BB = 8;
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
        }
        em->xFE++;
    case 3:
        MotionMoveF(em, 0);
        if ((s16) pG->pl_life > 0 && em->hp > 0 && em->x38D != 1) {
            if (em39DeadCk(em)) {
                if (em39JumpUpCk3(em)) {
                    return;
                }
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                if (em->plDist2 > 25000000.0f) {
                    w->x680 = 0;
                }
                if (w->x680) {
                    break;
                }
                if (em39JumpUpCk3(em)) {
                    return;
                }
                if (w->targetAngAbs > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                } else if (pG->x4F88 <= 9) {
                    EmRoutineSet(em, 1, 8, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 9, 0, 0);
                }
            }
        }
        break;
    }
    if (em39LockCk(em)) {
        w->lockCnt++;
        if (w->lockCnt > 10) {
            if (em->type == 2) {
                if (em39SlantCk2(em)) {
                    return;
                }
                w->lockCnt = 0;
            } else {
                if (w->x694 == 0 || em39HeadLockCk(em)) {
                    if (em39SlantCk(em)) {
                        return;
                    }
                    if (em->plDist2 < 25000000.0f) {
                        EmRoutineSet(em, 1, 0xF, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 0xD, 0, 0);
                    }
                    return;
                }
            }
        }
    } else {
        w->lockCnt = 0;
    }
    if (w->flags & 0x20000) {
        w->x8C4 = 5;
        EmRoutineSet(em, 1, 0x26, 0, 0);
    } else {
        em39GotoCk(em);
    }
}

static void em39_R1_Sit(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        w->x10 = 0;
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(0x3B), 0, 0, 5, 0);
        } else {
            int far;

            em39SitChg(em);
            far = 0;
            if (w->pGotoPoint && (em->pos.x - w->pGotoPoint->pos.x) * (em->pos.x - w->pGotoPoint->pos.x) + (em->pos.z - w->pGotoPoint->pos.z) * (em->pos.z - w->pGotoPoint->pos.z) > 90000.0f) {
                far = 1;
            }
            if (far) {
                MotionSetCore(em, MOTION(em), ARC(0x62), (int) ARC(0x63), 3, 5, 0);
                w->x10 = 1;
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x3B), 0, 0x1E, 5, 0);
            }
        }
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        AtariOff(&em->atari, 0xFCFF);
        if (w->pGotoPoint) {
            if ((em->pos.x - w->pGotoPoint->pos.x) * (em->pos.x - w->pGotoPoint->pos.x) + (em->pos.z - w->pGotoPoint->pos.z) * (em->pos.z - w->pGotoPoint->pos.z) < 22500.0f) {
                if (w->x10) {
                    MotionSetCore(em, MOTION(em), ARC(0x3B), 0, 0x1E, 5, 0);
                    w->x10 = 0;
                }
                em->pos = w->pGotoPoint->pos;
                em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.09817477f);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            } else {
                em->rot.y += Muku(&em->pos, &w->pGotoPoint->pos, em->rot.y, 0.7853982f);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            }
        }
        if ((s16) pG->pl_life > 0) {
            if (em39ExitCk(em) || (w->flags & 0x2000) || em->plDist2 < 25000000.0f) {
                if (em39AreaMoveCk(em) == 0) {
                    w->x898 = 200;
                    EmRoutineSet(em, 1, 0x26, 0, 0);
                }
            } else {
                if (em39DeadCk(pPL)) {
                    w->x680 = 30;
                }
                if (w->x680 == 0) {
                    u8 r;

                    AtariOn(&em->atari, 0x300);
                    r = Rnd() % 3;
                    switch (r) {
                    case 0:
                    default:
                        EmRoutineSet(em, 1, 0x24, 0, 0);
                        break;
                    case 1:
                        EmRoutineSet(em, 1, 0x1F, 0, 0);
                        break;
                    case 2:
                        EmRoutineSet(em, 1, 0x21, 0, 0);
                        break;
                    }
                }
            }
        }
        break;
    }
    if (w->flags & 0x20000) {
        w->x8C4 = 5;
        EmRoutineSet(em, 1, 0x26, 0, 0);
    }
}

static void em39_R1_SitDown(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        if (w->x8B4 != 3) {
            MotionSetCore(em, MOTION(em), ARC(0xD1), (int) ARC(0xD2), 3, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x8E), (int) ARC(0x8F), 3, 1, 0);
        }
        AtariOff(&em->atari, 0xFCFF);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if ((s16) w->dmgTotal > 400) {
                if (em39AreaMoveCk(em) == 0) {
                    w->x898 = 200;
                    EmRoutineSet(em, 1, 0x26, 0, 0);
                }
            } else {
                w->x680 = (u8) (Rnd() % 45) + 90;
                EmRoutineSet(em, 1, 5, 0, 0);
            }
        }
        break;
    }
}

static void em39_R1_WallWait(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    f32 dy;

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(0x96), 0, 0, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x96), 0, 0xA, 5, 0);
        }
        em->be_flag |= 2;
        AtariOn(&em->atari, 0x300);
        w->x4 = 60;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if ((s16) pG->pl_life > 0) {
            if (w->pGotoPoint) {
                f32 ang;

                PosToPos(&em->pos, &w->pGotoPoint->pos, &em->pos, 0.1f);
                ang = LIMIT_ANGLE(w->pGotoPoint->rotY + PI);
                em->rot.y += Muku2(em->rot.y, ang, 0.39269908f);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            }
            if (w->x4) {
                w->x4--;
            } else {
                dy = em->pos.y - pPL->pos.y;
                dy = fabsf(dy);
                if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) < 2.3561945f || em->plDist2 < 25000000.0f) {
                    w->x684 = 300;
                    w->x680 = 0;
                    if (w->x6A0 == 0) {
                        EmRoutineSet(em, 1, 9, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 8, 0, 0);
                    }
                } else if (w->x680 == 0) {
                    u8 r = Rnd() % 10;

                    if (r > 4 || dy > 2000.0f) {
                        EmRoutineSet(em, 1, 0x20, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 0x22, 0, 0);
                    }
                }
            }
        }
        break;
    }
    if (w->flags & 0x20000) {
        w->x8C4 = 5;
        EmRoutineSet(em, 1, 0x26, 0, 0);
    }
}

// Walk / Run / Goto share the "stop when the player is dead or the enemy died" branch and the
// lock-on escape (em39LockCk) and the jump / door checks.
static void em39_R1_Walk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        if (em->type == 2) {
            w->x8BB = 8;
            MotionSetCore(em, MOTION(em), ARC(0xD4), (int) ARC(0xD5), 0xA, 5, 0);
        } else if (w->x8B4 == 1) {
            MotionSetCore(em, MOTION(em), ARC(0x9F), (int) ARC(0xA0), 0xA, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x3C), (int) ARC(0x3D), 0xA, 5, 0);
        }
        if (Muku(&pPL->pos, &em->pos, pPL->rot.y, PI) > 0.0f) {
            w->x8B5 = 0;
        } else {
            w->x8B5 = 1;
        }
        w->x4 = 0;
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
        }
        if (w->x58C) {
            w->x58C->setFall(0, 0, 20.0f);
            w->x58C = 0;
        }
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        {
            f32 ang = Muku(&pPL->pos, &em->pos, pPL->rot.y, PI);

            if (fabsf(ang) > 0.2617994f) {
                if (ang > 0.0f) {
                    w->x8B5 = 0;
                } else {
                    w->x8B5 = 1;
                }
            }
        }
        if ((s16) pG->pl_life <= 0 || em->hp <= 0) {
            EmRoutineSet(em, 1, 4, 0, 0);
            break;
        }
        if (em39AtkRtnCk(em)) {
            break;
        }
        if (w->targetAngAbs > 2.3561945f) {
            EmRoutineSet(em, 1, 0xB, 0, 0);
        } else if (em->plDist2 > 20250000.0f && w->x6A0 == 0) {
            if (em39SlantCk(em)) {
                return;
            }
            EmRoutineSet(em, 1, 9, 0, 0);
        }
        break;
    }
    if (em39LockCk(em)) {
        w->lockCnt++;
        if (w->lockCnt > 10) {
            if (em->type == 2) {
                if (em39SlantCk2(em)) {
                    return;
                }
                w->lockCnt = 0;
            } else {
                if (w->x694 == 0 || em39HeadLockCk(em)) {
                    if (em39SlantCk(em)) {
                        return;
                    }
                    if (em->plDist2 < 25000000.0f) {
                        EmRoutineSet(em, 1, 0xF, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 0xD, 0, 0);
                    }
                    return;
                }
            }
        }
    } else {
        w->lockCnt = 0;
    }
    if (em39JumpDownCk(em, 0)) {
        return;
    }
    if (em39JumpUpCk(em)) {
        return;
    }
    if (em39FanceJumpCk(em)) {
        return;
    }
    if (em39DoorOpenCk(em)) {
        return;
    }
    if (em39GotoCk(em)) {
        return;
    }
    if (w->flags & 0x20000) {
        w->x8C4 = 5;
        EmRoutineSet(em, 1, 0x26, 0, 0);
    }
}

static void em39_R1_Run(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        em->be_flag |= 2;
        AtariOn(&em->atari, 0x300);
        if (em->type == 2) {
            w->x8BB = 8;
            MotionSetCore(em, MOTION(em), ARC(0xD7), (int) ARC(0xD8), 5, 5, 0);
        } else {
            u8 r = Rnd() % 10;

            if (r > 4) {
                MotionSetCore(em, MOTION(em), ARC(0x3E), (int) ARC(0x3F), 5, 5, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x40), (int) ARC(0x41), 5, 5, 0);
            }
        }
        if (Muku(&pPL->pos, &em->pos, pPL->rot.y, PI) > 0.0f) {
            w->x8B5 = 0;
        } else {
            w->x8B5 = 1;
        }
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
        }
        if (w->x58C) {
            w->x58C->setFall(0, 0, 20.0f);
            w->x58C = 0;
        }
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.2617994f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        {
            f32 ang = Muku(&pPL->pos, &em->pos, pPL->rot.y, PI);

            if (fabsf(ang) > 0.2617994f) {
                if (ang > 0.0f) {
                    w->x8B5 = 0;
                } else {
                    w->x8B5 = 1;
                }
            }
        }
        if ((s16) pG->pl_life <= 0) {
            EmRoutineSet(em, 1, 4, 0, 0);
        } else if (em->hp <= 0) {
            EmRoutineSet(em, 1, 4, 0, 0);
        } else if (em39AtkRtnCk(em)) {
            break;
        } else if (w->targetAngAbs > 2.3561945f) {
            EmRoutineSet(em, 1, 0xB, 0, 0);
        } else if (em->plDist2 < 9000000.0f && em->type == 2 && pG->x4F88 <= 1) {
            EmRoutineSet(em, 1, 8, 0, 0);
        } else if (em->plDist2 < 6250000.0f && em->type == 2 && pG->x4F88 <= 3) {
            EmRoutineSet(em, 1, 8, 0, 0);
        }
        break;
    }
    if (em39LockCk(em) && em->type != 2) {
        w->lockCnt++;
        if (w->lockCnt > 15) {
            if (w->x694 == 0 || em39HeadLockCk(em)) {
                if (em39JumpUpCk3(em)) {
                    return;
                }
                if (em39SlantCk(em)) {
                    return;
                }
                if (em->plDist2 < 25000000.0f) {
                    EmRoutineSet(em, 1, 0xF, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 0xD, 0, 0);
                }
                return;
            }
        }
    } else {
        w->lockCnt = 0;
    }
    if (em39JumpDownCk(em, 0)) {
        return;
    }
    if (em39JumpUpCk(em)) {
        return;
    }
    if (em39FanceJumpCk(em)) {
        return;
    }
    if (em39DoorOpenCk(em)) {
        return;
    }
    if (em39GotoCk(em)) {
        return;
    }
    if (w->flags & 0x20000) {
        w->x8C4 = 5;
        EmRoutineSet(em, 1, 0x26, 0, 0);
    }
}

static void em39_R1_Goto(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        em->be_flag |= 2;
        AtariOn(&em->atari, 0x300);
        w->x8BB = 8;
        MotionSetCore(em, MOTION(em), ARC(0x40), (int) ARC(0x41), 0xA, 5, 0);
        if (Muku(&pPL->pos, &em->pos, pPL->rot.y, PI) > 0.0f) {
            w->x8B5 = 0;
        } else {
            w->x8B5 = 1;
        }
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
        }
        if (w->x58C) {
            w->x58C->setFall(0, 0, 20.0f);
            w->x58C = 0;
        }
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionMoveF(em, 0);
        {
            f32 ang = Muku(&pPL->pos, &em->pos, pPL->rot.y, PI);

            if (fabsf(ang) > 0.2617994f) {
                if (ang > 0.0f) {
                    w->x8B5 = 0;
                } else {
                    w->x8B5 = 1;
                }
            }
        }
        if ((s16) pG->pl_life <= 0) {
            EmRoutineSet(em, 1, 4, 0, 0);
        } else if (w->targetAngAbs > 2.3561945f) {
            EmRoutineSet(em, 1, 0xB, 0, 0);
        } else if ((em->pos.x - w->gotoPos.x) * (em->pos.x - w->gotoPos.x) + (em->pos.y - w->gotoPos.y) * (em->pos.y - w->gotoPos.y) + (em->pos.z - w->gotoPos.z) * (em->pos.z - w->gotoPos.z) < 1000000.0f) {
            {
                u8 zero = 0;
                w->gotoOn = zero;
                EmRoutineSet(em, 1, 4, zero, zero);
            }
        }
        break;
    }
    if (em39JumpDownCk(em, 0)) {
        return;
    }
    if (em39JumpUpCk(em)) {
        return;
    }
    if (em39FanceJumpCk(em)) {
        return;
    }
    if (em39DoorOpenCk(em)) {
        return;
    }
    if (w->flags & 0x20000) {
        w->x8C4 = 5;
        EmRoutineSet(em, 1, 0x26, 0, 0);
    }
}

static void em39_R1_Turn180(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x48), (int) ARC(0x49), 5, 1, 0);
        w->x18 = em->rot.y + PI;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 8) {
            f32 d = Muku(&em->pos, &w->targetPos, w->x18, 0.09817477f);

            w->x18 += d;
            w->x18 = LIMIT_ANGLE(w->x18);
            em->rot.y += d;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 8, 0, 0);
        } else if (w->x680 == 0 && (em->seFlags28B & 4)) {
            if (em39AtkRtnCk(em)) {
                break;
            }
            if (em39SlantCk(em)) {
                return;
            }
            if (w->x6A0 == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
    if (em39JumpDownCk(em, 0)) {
        return;
    }
    if (em39JumpUpCk(em)) {
        return;
    }
    if (em39FanceJumpCk(em)) {
        return;
    }
    em39GotoCk(em);
}

static void em39_R1_Threat(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int end;

    w->flags |= 0x10;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x6C), 0, 5, 1, 0);
        w->x8BB = 0;
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        end = MotionMoveF(em, 0);
        if (end) {
            if (em->xFF && (u8) (Rnd() % 10) > 3) {
                if ((u8) (Rnd() % 10) > 4 && em->plDist2 > 25000000.0f) {
                    EmRoutineSet(em, 1, 0x1D, 0, 1);
                } else {
                    EmRoutineSet(em, 1, 0x23, 0, 1);
                }
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        } else if (em39LockCk(em) && em->type != 2 && em->xFF <= 3) {
            if ((u8) (Rnd() % 10) > 6 && em39JumpUpCk3(em)) {
                break;
            }
            if (em->plDist2 < 25000000.0f) {
                EmRoutineSet(em, 1, 0xF, 0, 0);
            } else {
                EmRoutineSet(em, 1, 0xD, 0, 0);
            }
        }
        break;
    }
    em39GotoCk(em);
}

// Wall probe from the chest towards (x, 0, z) in model space (Escape / Step).
#define EM39_WALL_CK(em, a, b, bx, bz)                                                             \
    (a).x = 0.0f;                                                                                  \
    (a).y = 1500.0f;                                                                               \
    (a).z = 0.0f;                                                                                  \
    (b).x = bx;                                                                                    \
    (b).y = 1500.0f;                                                                               \
    (b).z = bz;                                                                                    \
    PSMTXMultVec((em)->mat, &(a), &(a));                                                           \
    PSMTXMultVec((em)->mat, &(b), &(b));

static void em39_R1_Escape(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec a;
    Vec b;
    int end;

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0: {
        int wall = 0;
        u8 dir;

        EM39_WALL_CK(em, a, b, 2000.0f, 0.0f);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            wall = 1;
        }
        EM39_WALL_CK(em, a, b, -2000.0f, 0.0f);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            wall |= 2;
        }
        EM39_WALL_CK(em, a, b, 0.0f, -2000.0f);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            wall |= 4;
        }
        dir = Rnd() & 3;
        switch ((u32) wall) {
        case 0:
            break;
        case 1:
            dir = 0;
            break;
        case 2:
            dir = 1;
            break;
        case 3:
            dir = 2;
            break;
        default:
            dir = 3;
            break;
        }
        if (dir == 3 && em->plDist2 < 16000000.0f) {
            dir = Rnd() % 3;
        }
        switch ((u32) dir) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x44), (int) ARC(0x45), 3, 1, 5);
            EstSet((int) em, -1, 0, 0, 0x2F, 0xD, 0, 0, (u32) em, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0x46), (int) ARC(0x47), 3, 1, 5);
            EstSet((int) em, -1, 0, 0, 0x2F, 0xE, 0, 0, (u32) em, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(0x42), (int) ARC(0x43), 3, 1, 3);
            EstSet((int) em, -1, 0, 0, 0x2F, 0x14, 0, 0, (u32) em, 0);
            break;
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0x4C), (int) ARC(0x4D), 3, 1, 8);
            EstSet((int) em, -1, 0, 0, 0x2F, 0xF, 0, 0, (u32) em, 0);
            break;
        }
        em->dmType = 0x1E;
        w->x4 = 30;
        em->xFF++;
        w->x694 = (u8) (Rnd() % 150) + 150;
        em->xFE++;
    }
    case 1:
        if (w->x4) {
            w->x4--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.39269908f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        end = MotionMoveF(em, 0);
        if (end) {
            if (w->targetAngAbs > 2.3561945f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else {
                EmRoutineSet(em, 1, 4, 0, 0);
            }
            break;
        }
        if (em->seFlags28B & 4) {
            if (em39LockCk(em) && em->type != 2 && em->xFF <= 3) {
                if ((u8) (Rnd() % 10) > 6 && em39JumpUpCk3(em)) {
                    break;
                }
                em->xFE = end;
                break;
            }
        } else {
            w->flags |= 0x100;
        }
        if (em->seFlags28B & 1) {
            if ((u8) (Rnd() % 10) > 4) {
                if (w->targetAngAbs > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                } else if (w->x680) {
                    EmRoutineSet(em, 1, 4, 0, 0);
                } else {
                    if ((u8) (Rnd() % 10) > 4 && em39AtkRtnCk(em)) {
                        break;
                    }
                    if (em39SlantCk(em)) {
                        break;
                    }
                    if (w->x6A0 == 0) {
                        EmRoutineSet(em, 1, 9, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 8, 0, 0);
                    }
                }
            }
        }
        break;
    }
}

static void em39_R1_Backjump(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int jump;

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        if (em->xFF == 3 || em->xFF == 4) {
            MotionSetCore(em, MOTION(em), ARC(0x75), (int) ARC(0x76), 3, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2F, 0x29, 0, 0, (u32) em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x42), (int) ARC(0x43), 3, 1, 3);
            EstSet((int) em, -1, 0, 0, 0x2F, 0x14, 0, 0, (u32) em, 0);
        }
        w->x8BB = 0;
        em->dmType = 0x1E;
        w->x4 = 10;
        if (em->type == 2) {
            w->x6A0 = 60;
            if (pG->x4F88 <= 1) {
                w->x6A0 = 150;
            }
            if (pG->x4F88 <= 3) {
                w->x6A0 = 120;
            }
            if (pG->x4F88 > 6) {
                w->x6A0 = 30;
            }
            if (pG->x4F88 > 9) {
                w->x6A0 = 0;
            }
        }
        em->xFE++;
    case 1:
        if (w->x4) {
            w->x4--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.19634955f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (em->xFF == 3) {
                EmRoutineSet(em, 1, 0x1E, 0, 0);
            } else if (w->targetAngAbs > 2.3561945f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else {
                EmRoutineSet(em, 1, 4, 0, 0);
            }
            break;
        }
        if (em->seFlags28B & 4) {
            jump = em39JumpUpCk3(em);
            if (jump) {
                break;
            }
            if (em->xFF == 1) {
                if (em->type == 2) {
                    if (em39AtkRtnCk(em)) {
                        break;
                    }
                    if (em->plDist2 > 36000000.0f) {
                        EmRoutineSet(em, 1, 4, 0, 0);
                        break;
                    }
                }
                EmRoutineSet(em, 1, 0xE, 0, 4);
                break;
            }
            if (em->xFF == 2) {
                EmRoutineSet(em, 1, 0xE, 0, 3);
                break;
            }
            if (em39LockCk(em) && em->type != 2 && em->xFF == 0) {
                Rnd();
                if (em->plDist2 < 25000000.0f) {
                    EmRoutineSet(em, 1, 0xF, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 0xD, 0, 0);
                }
                break;
            }
        } else {
            w->flags |= 0x100;
        }
        if (em->seFlags28B & 1) {
            if (em->xFF == 4 && em->type == 2) {
                if (em39AtkRtnCk(em)) {
                    break;
                }
                EmRoutineSet(em, 1, 4, 0, 0);
                break;
            }
        }
        if (em->seFlags28B & 1) {
            if ((u8) (Rnd() % 10) > 4 && em->xFF == 0) {
                if (w->targetAngAbs > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                } else if (w->x680) {
                    EmRoutineSet(em, 1, 4, 0, 0);
                } else {
                    if ((u8) (Rnd() % 10) > 4 && (w->flags & 1)) {
                        f32 dy = fabsf(em->pos.y - pPL->pos.y);

                        if (em->plDist2 < 100000000.0f && em->plDist2 > 36000000.0f && w->routeAngAbs < 0.5235988f && dy < 2000.0f) {
                            EmRoutineSet(em, 1, 0x1D, 0, 0);
                            break;
                        }
                    }
                    if (em39SlantCk(em)) {
                        break;
                    }
                    if (w->x6A0 == 0) {
                        EmRoutineSet(em, 1, 9, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 8, 0, 0);
                    }
                }
            }
        }
        break;
    }
}

static void em39_R1_Step(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec a;
    Vec b;

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0: {
        int dir = Rnd() & 1;

        EM39_WALL_CK(em, a, b, 2000.0f, 0.0f);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            dir = 0;
        }
        EM39_WALL_CK(em, a, b, -2000.0f, 0.0f);
        if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
            dir = 1;
        }
        if (em->plDist2 > 6250000.0f && em->plDist2 < 12250000.0f) {
            dir = 2;
        }
        if (em->xFF) {
            dir = 2;
        }
        switch ((u32) dir) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x6F), (int) ARC(0x70), 3, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2F, 0xB, 0, 0, (u32) em, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0x6D), (int) ARC(0x6E), 3, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2F, 0xA, 0, 0, (u32) em, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(0x71), (int) ARC(0x72), 3, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2F, 0xC, 0, 0, (u32) em, 0);
            break;
        }
        em->dmType = 0x14;
        w->x4 = 20;
        em->xFF++;
        em->xFE++;
    }
    case 1:
        if (w->x4) {
            w->x4--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.39269908f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.3561945f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
                break;
            }
            if (em39SlantCk(em)) {
                break;
            }
            if (w->x6A0 == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
            break;
        }
        if (!(em->seFlags28B & 4)) {
            w->flags |= 0x100;
        }
        if (em->seFlags28B & 1) {
            if (em39AtkRtnCk(em)) {
                break;
            }
            if ((u8) (Rnd() % 10) > 4) {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                if (w->targetAngAbs > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                    break;
                }
                if (em39SlantCk(em)) {
                    break;
                }
                if (w->x6A0 == 0) {
                    EmRoutineSet(em, 1, 9, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 8, 0, 0);
                }
            }
        }
        break;
    }
}

static void em39_R1_Slant(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(0x64), (int) ARC(0x65), 3, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2F, 0xB, 0, 0, (u32) em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x66), (int) ARC(0x67), 3, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2F, 0xA, 0, 0, (u32) em, 0);
        }
        em->dmType = 5;
        w->x4 = 10;
        em->xFE++;
    case 1:
        w->x684 = 30;
        if (w->x4) {
            w->x4--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.39269908f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.3561945f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else if (w->x6A0 == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
            break;
        }
        if ((em->seFlags28B & 4) && w->targetAngAbs < 0.7853982f) {
            if (em39SlantCk(em)) {
                break;
            }
            if (em->plDist2 < 12250000.0f) {
                EmRoutineSet(em, 1, 0xF, 0, 1);
            } else if (w->x6A0 == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
            break;
        }
        if (em->seFlags28B & 1) {
            if ((u8) (Rnd() % 10) > 4) {
                if (w->targetAngAbs > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                } else {
                    em39AtkRtnCk(em);
                }
            }
        }
        break;
    }
}

static void em39_R1_Slant2(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        if ((u8) (Rnd() % 10) > 4) {
            MotionSetCore(em, MOTION(em), ARC(0xFB), (int) ARC(0xFC), 3, 1, 0);
            if (pG->x4FB8 == 2) {
                EstSet((int) em, -1, 0, 0, 0x2F, 0x49, 0, 0, (u32) em, 0);
            } else {
                EstSet((int) em, -1, 0, 0, 0x2F, 0x40, 0, 0, (u32) em, 0);
            }
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xFD), (int) ARC(0xFE), 3, 1, 0);
            if (pG->x4FB8 == 2) {
                EstSet((int) em, -1, 0, 0, 0x2F, 0x4A, 0, 0, (u32) em, 0);
            } else {
                EstSet((int) em, -1, 0, 0, 0x2F, 0x41, 0, 0, (u32) em, 0);
            }
        }
        em->dmType = 5;
        w->x4 = 10;
        w->x8BB = 8;
        em->xFE++;
    case 1:
        w->x684 = 30;
        if (w->x4) {
            w->x4--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.39269908f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.3561945f) {
                EmRoutineSet(em, 1, 0xB, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
            break;
        }
        if (em->seFlags28B & 4) {
            if (pG->x4F88 > 3) {
                if (em->plDist2 > 12250000.0f && (u8) (Rnd() % 10) > 4) {
                    EmRoutineSet(em, 1, 0x11, 0, 0);
                    break;
                }
                if (em->plDist2 > 12250000.0f && (u8) (Rnd() % 10) > 4 && w->x6A0 == 0) {
                    EmRoutineSet(em, 1, 9, 0, 0);
                    break;
                }
            }
            EmRoutineSet(em, 1, 8, 0, 0);
        }
        break;
    }
}

static void em39_R1_SuperDash(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec spd;
    f32 d;

    w->flags |= 0x30;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xD9), (int) ARC(0xDA), 3, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2F, 0xC, 0, 0, (u32) em, 0);
        w->x10 = 0;
        d = GetDistance3(&em->pos, &pPLS->pos) - 7100.0f;
        w->x18 = d * 0.25f;
        w->x688 = 600;
        if (pGS->x4F88 <= 3) {
            w->x688 = 900;
        }
        if (pG->x4F88 > 6) {
            w->x688 = 450;
        }
        if (pG->x4F88 > 9) {
            w->x688 = 360;
        }
        w->x8BB = 8;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 8) {
            em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.39269908f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->seFlags28B & 1) {
            if (w->x10 == 0) {
                w->x10 = 1;
                d = GetDistance3(&em->pos, &pPLS->pos) - 7100.0f;
        w->x18 = d * 0.25f;
            }
            spd.x = 0.0f;
            spd.y = 0.0f;
            spd.z = w->x18;
            PSMTXMultVecSR(em->mat, &spd, &spd);
            PSVECAdd(&em->pos, &spd, &em->pos);
        }
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 4, 2, 0);
        } else if (em->seFlags28B & 4) {
            if (em39AtkRtnCk(em)) {
                break;
            }
            EmRoutineSet(em, 1, 4, 2, 0);
        }
        break;
    }
}

static void em39_R1_JumpDown(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int end;
    Vec v;
    f32 fl;

    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        if (em->xFF == 0) {
            MotionSetCore(em, MOTION(em), ARC(0x50), (int) ARC(0x51), 3, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2F, 0x11, 0, 0, (u32) em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x52), (int) ARC(0x53), 3, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2F, 0x12, 0, 0, (u32) em, 0);
        }
        em->be_flag |= 2;
        AtariOff(&em->atari, 0xFCFF);
        w->flags &= ~0x8000;
        if (w->x58C) {
            w->x58C->setFall(0, 0, 20.0f);
            w->x58C = 0;
        }
        em->xFE++;
    case 1:
        w->flags |= 0x01000000;
        em->rot.y += Muku2(em->rot.y, w->jumpAng, 0.39269908f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        em->dmType = 2;
        end = MotionMoveF(em, 0);
        if (em->seFlags28B & 0x10) {
            v = em->pos;
            v.y = em->oldPos.y;
            fl = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
            if (em->pos.y < fl) {
                em->pos.y = fl;
                AtariOn(&em->atari, 0x300);
                if (w->x8B4 == 3 || w->x8B4 == 4) {
                    em39WepSet(em, 0);
                }
                SndCall(8, 0xE, &em->pos, em->id, 0, em);
                MotionSetCore(em, MOTION(em), ARC(0x54), (int) ARC(0x55), 3, 1, 0);
                EstSet((int) em, -1, 0, 0, 0x2F, 0x13, 0, 0, (u32) em, 0);
                MotionMoveF(em, 0);
                em->xFE = 2;
                break;
            }
        }
        if (end) {
            em->xFE++;
        }
        break;
    case 2:
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if (em->pos.y < -4000.0f) {
                w->x898 = 200;
                EmRoutineSet(em, 1, 0x26, 0, 0);
                return;
            }
            if (em39GotoCk(em)) {
                return;
            }
            if (w->x6A0 == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
    if (w->flags & 0x20000) {
        w->x8C4 = 5;
        EmRoutineSet(em, 1, 0x26, 0, 0);
    }
}

static void em39_R1_JumpUp(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Mtx m;
    Vec v;

    w->flags |= 0x100;
    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        PSMTXRotRad(m, 'y', w->jumpAng);
        TransMatrix(m, &em->pos);
        v.x = 0.0f;
        v.y = 3200.0f;
        v.z = 2530.0f;
        PSMTXMultVec(m, &v, &v);
        PSVECSubtract(&w->jumpPos, &v, &w->x1C);
        MotionSetCore(em, MOTION(em), ARC(0x4E), (int) ARC(0x4F), 3, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x10, 0, 0, (u32) em, 0);
        AtariOff(&em->atari, 0xFCFF);
        em->xFE++;
    case 1:
        em->rot.y += Muku2(em->rot.y, w->jumpAng, 0.39269908f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (em->seFlags28B & 1) {
            PSVECScale(&w->x1C, &v, 0.1f);
            PSVECAdd(&em->pos, &v, &em->pos);
            PSVECSubtract(&w->x1C, &v, &w->x1C);
            w->flags |= 0x01000000;
        }
        if (MotionMoveF(em, 0)) {
            AtariOn(&em->atari, 0x300);
            if (em39GotoCk(em)) {
                break;
            }
            if (w->x6A0 == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
}

static void em39_R1_JumpUp2(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Mtx m;
    Vec v;

    w->flags |= 0x100;
    em->setStatus(3);
    switch (em->xFE) {
    case 0: {
        // COMPILER-DIFF: #2 -- the target's `fmuls f12,f0,f12` ties the product to the constant's
        // register (its sum was not a tieable operand); value-carrying pins reproduce the tie and
        // the FPR names around it.
        register f32 posy asm("fr11");
        register f32 dy asm("fr0");
        register f32 k asm("fr12");
        register f32 t asm("fr12");
        register f32 k19 asm("fr13");
        register f32 py asm("fr13");

        MotionSetCore(em, MOTION(em), ARC(0x6A), (int) ARC(0x6B), 0xA, 1, 0);
        posy = em->pos.y;
        dy = w->jumpPos.y - posy + 1000.0f;
        k = 0.0052631581f;
        t = dy * k;
        w->x28.x = 0.0f;
        w->x28.z = 0.0f;
        k19 = 19.0f;
        py = t * k19;
        w->x28.y = py;
        w->x18 = t;
        PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &w->jumpPos));
        TransMatrix(m, &em->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1500.0f;
        PSMTXMultVec(m, &v, &v);
        PSVECSubtract(&w->jumpPos, &v, &w->x1C);
        w->x1C.y = 0.0f;
        em->xFE++;
    }
    case 1:
        em->rot.y += Muku2(em->rot.y, w->jumpAng, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (em->seFlags28B & 0x40) {
            if (em->seFlags28B & 0x10) {
                PSVECAdd(&em->pos, &w->x28, &em->pos);
                w->x28.y -= 35.714287f;
            } else {
                PSVECAdd(&em->pos, &w->x28, &em->pos);
                w->x28.y -= w->x18;
            }
            PSVECScale(&w->x1C, &v, 0.1f);
            PSVECAdd(&em->pos, &v, &em->pos);
            PSVECSubtract(&w->x1C, &v, &w->x1C);
            w->flags |= 0x01000000;
        }
        if (MotionMoveF(em, 0)) {
            AtariOn(&em->atari, 0x300);
            if (em39GotoCk(em)) {
                break;
            }
            if (w->x6A0 == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
}

static void em39_R1_JumpUp3(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Mtx m;
    Vec v;

    w->flags |= 0x100;
    em->setStatus(3);
    switch (em->xFE) {
    case 0: {
        if (fabsf(Muku(&em->pos, &w->jumpPos, em->rot.y, PI)) < 1.5707964f) {
            MotionSetCore(em, MOTION(em), ARC(0x6A), (int) ARC(0x6B), 0xA, 1, 0);
            // COMPILER-DIFF: #2 -- both arms: the product is tied to the constant's register
            // (`fmuls f12,f13,f12` / `fmuls f12,f0,f12`) and the 0.0 reuses the 1000.0 register;
            // value-carrying pins per arm (the JumpUp2 recipe). Store order x, y, z, x18: the zero's
            // first use sinks last, the dying stores keep source order.
            register f32 posy asm("fr0");
            register f32 dy asm("fr13");
            register f32 k asm("fr12");
            register f32 t asm("fr12");
            register f32 k19 asm("fr0");
            register f32 py asm("fr0");
            register f32 zf asm("fr11");

            posy = em->pos.y;
            dy = w->jumpPos.y - posy + 1000.0f;
            k = 0.0052631581f;
            t = dy * k;
            zf = 0.0f;
            k19 = 19.0f;
            py = t * k19;
            w->x28.x = zf;
            w->x28.y = py;
            w->x28.z = zf;
            w->x18 = t;
            em->xFF = 0;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x68), (int) ARC(0x69), 0xA, 1, 0);
            register f32 posy asm("fr13"); // COMPILER-DIFF: #2 (see the other arm)
            register f32 dy asm("fr0");
            register f32 k asm("fr12");
            register f32 t asm("fr12");
            register f32 k19 asm("fr13");
            register f32 py asm("fr13");
            register f32 zf asm("fr11");

            posy = em->pos.y;
            dy = w->jumpPos.y - posy + 1000.0f;
            k = 0.0058479533f;
            t = dy * k;
            zf = 0.0f;
            k19 = 18.0f;
            py = t * k19;
            {
                // COMPILER-DIFF: 13 -- the `li r0,1` precedes the four stores: with the values kept alive
                // past the stores (no store dies) the block is in LUID order, and the `1` is set first.
                int one = 1;
                w->x28.y = py;
                w->x28.z = zf;
                w->x18 = t;
                w->x28.x = zf;
                asm("" : "=m"(w->x4) : "f"(py), "f"(zf), "f"(t)); // COMPILER-DIFF: 13
                em->xFF = one;
            }
        }
        PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &w->jumpPos));
        TransMatrix(m, &em->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1500.0f;
        PSMTXMultVec(m, &v, &v);
        PSVECSubtract(&w->jumpPos, &v, &w->x1C);
        w->x1C.y = 0.0f;
        em->xFE++;
    }
    case 1:
        if (em->xFF) {
            f32 a;

            a = GetXZAngle(&w->jumpPos, &em->pos);
            {
                // COMPILER-DIFF: #1 -- the limit constant reaches the call as a dying pseudo (the original's
                // REG_EQUIV constant): its `lis` then outranks the GetXZAngle result copy in sched1 (path
                // lis; lfs; fmr f3 = 12 vs 11) and precedes the `fmr f2,f1`. The codeless asm is the second
                // reference that keeps combine from folding the pseudo into the argument load.
                f32 k = 0.39269908f;
                asm("" : "=m"(w->x4) : "f"(k));
                em->rot.y += Muku2(em->rot.y, a, k);
            }
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        } else {
            em->rot.y += Muku(&em->pos, &w->jumpPos, em->rot.y, 0.39269908f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->seFlags28B & 0x40) {
            if (em->seFlags28B & 0x10) {
                PSVECAdd(&em->pos, &w->x28, &em->pos);
                w->x28.y -= 35.714287f;
            } else {
                PSVECAdd(&em->pos, &w->x28, &em->pos);
                w->x28.y -= w->x18;
            }
            PSVECScale(&w->x1C, &v, 0.1f);
            PSVECAdd(&em->pos, &v, &em->pos);
            PSVECSubtract(&w->x1C, &v, &w->x1C);
            w->flags |= 0x01000000;
        }
        if (MotionMoveF(em, 0)) {
            AtariOn(&em->atari, 0x300);
            w->flags |= 0x8000;
            if (w->flags & 0x00400000) {
                w->flags |= 0x00040000;
            }
            w->flags |= 0x00400000;
            if (em39GotoCk(em)) {
                break;
            }
            if ((u8) (Rnd() % 10) > 4 && em->plDist2 > 25000000.0f) {
                EmRoutineSet(em, 1, 0x1D, 0, 1);
            } else {
                EmRoutineSet(em, 1, 0x23, 0, 1);
            }
        }
        break;
    }
}

static void em39_R1_FanceJump(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->flags |= 0x100;
    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x4A), (int) ARC(0x4B), 3, 1, 0);
        em->xFE++;
    case 1:
        em->rot.y += Muku2(em->rot.y, w->jumpAng, 0.39269908f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            if (em39GotoCk(em)) {
                break;
            }
            EmRoutineSet(em, 1, 8, 0, 0);
        }
        break;
    }
}

static void em39_R1_AtkKnife(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->flags |= 0x80;
    w->flags &= ~0x20;
    if (em->xFE == 0 && w->x8B4 != 1) {
        em->xFE = 2;
    }
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA1), (int) ARC(0xA2), 3, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x17, 0, 0, (u32) em, 0);
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
        }
        w->x4 = 0;
        w->x8B6 = 0;
        w->x8 = 22;
        w->x8B7 = 0;
        em->xFE++;
    case 1:
        if (w->x4) {
            w->x4--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.2617994f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->seFlags28B & 1) {
            em39AtkCk(em, 2, 0xA);
        }
        if (em->seFlags28B & 2) {
            em39SetVoice(em, 0x19);
        }
        if (MotionMoveF(em, 0)) {
            if (w->x8B6 == 0) {
                GameAddPoint(0xB);
            }
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                w->x680 = 30;
                if (em->plDist2 < 9000000.0f) {
                    EmRoutineSet(em, 1, 0xE, 0, 1);
                } else {
                    EmRoutineSet(em, 1, 4, 0, 0);
                }
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xA3), (int) ARC(0xA4), 3, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x18, 0, 0, (u32) em, 0);
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
        }
        em39SetVoice(em, 0x19);
        w->x8B6 = 0;
        w->x8B7 = 0;
        w->x4 = 10;
        w->x8 = 22;
        em->xFE++;
    case 3:
        em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (em->seFlags28B & 0x10) {
            em39WepSet(em, 1);
            if (w->pWep) {
                MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
            }
        }
        if (MotionMoveF(em, 0)) {
            if (w->x8B6 == 0) {
                GameAddPoint(0xB);
            }
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                w->x680 = 30;
                if (em->plDist2 < 9000000.0f) {
                    EmRoutineSet(em, 1, 0xE, 0, 1);
                } else {
                    EmRoutineSet(em, 1, 4, 0, 0);
                }
            }
        } else if (em->seFlags28B & 1) {
            em39AtkCk(em, 2, 0xA);
        }
        break;
    }
    em39HandSet(em, 1);
}

// The knife swing at a door (cEmDoor): break or open it at the hit frame.
static inline void em39DoorHit(cEm39* em, Em39Work* w)
{
    if ((em->seFlags28B & 1) && w->pDoor) {
        if (w->pDoor->isAlive()) {
            if (w->pDoor->type == 0) {
                w->pDoor->setBreak(&em->pos);
            } else {
                w->pDoor->setOpen(&em->pos, 0, 0, 0);
            }
        }
        w->pDoor = 0;
    }
}

static void em39_R1_AtkDoor(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->flags |= 0x80;
    w->flags &= ~0x20;
    if (em->xFE == 0 && w->x8B4 != 1) {
        em->xFE = 2;
    }
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xA1), (int) ARC(0xA2), 3, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x17, 0, 0, (u32) em, 0);
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
        }
        w->x4 = 0;
        w->x8B6 = 0;
        w->x8 = 22;
        w->x8B7 = 0;
        em->xFE++;
    case 1:
        em39DoorHit(em, w);
        if (em->seFlags28B & 2) {
            em39SetVoice(em, 0x19);
        }
        if (MotionMoveF(em, 0)) {
            if (w->x6A0 == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xA3), (int) ARC(0xA4), 3, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x18, 0, 0, (u32) em, 0);
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
        }
        em39SetVoice(em, 0x19);
        w->x8B6 = 0;
        w->x8B7 = 0;
        w->x4 = 10;
        w->x8 = 22;
        em->xFE++;
    case 3:
        if (em->seFlags28B & 0x10) {
            em39WepSet(em, 1);
            if (w->pWep) {
                MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
            }
        }
        em39DoorHit(em, w);
        if (em->seFlags28B & 2) {
            em39SetVoice(em, 0x19);
        }
        if (MotionMoveF(em, 0)) {
            if (w->x6A0 == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        }
        break;
    }
    em39HandSet(em, 1);
}

static void em39_R1_br_KnifeCatch(cEm39* em)
{
    if (em->hp > 0 && (em->seFlags28B & 2) && em39CatchCk(em)) {
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        em->stat = 0x011B0000;
    }
}

static void em39_R1_KnifeCatch(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    f32 pang = 0.0f;

    w->flags |= 0x80;
    w->flags &= ~0x20;
    switch (em->xFE) {
    case 0: {
        f32 ang = Muku(&em->pos, &pPL->pos, em->rot.y, PI);

        pang = fabsf(pang);
        if (pang < 0.7853982f) {
            MotionSetCore(em, MOTION(em), ARC(0xA9), (int) ARC(0xAA), 3, 1, 0);
            w->x18 = em->rot.y;
        } else if (ang < 0.0f) {
            MotionSetCore(em, MOTION(em), ARC(0xA5), (int) ARC(0xA6), 3, 1, 0);
            w->x18 = em->rot.y + -1.5707964f;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xA7), (int) ARC(0xA8), 3, 1, 0);
            w->x18 = em->rot.y + 1.5707964f;
        }
        w->x18 = LIMIT_ANGLE(w->x18);
        asm("" : "=m"(w->x4) : "f"(pang)); // COMPILER-DIFF: candidate #12 (cse2 canonical register)
        w->x4 = 10;
        w->x8B6 = 0;
        w->x8B7 = 0;
        em39WepSet(em, 1);
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x29), 0, 0, 5, 0);
        }
        AtariOn(&em->atari, 0x300);
        em->be_flag |= 2;
        em39SetVoice(em, 0x19);
        em->xFE++;
    }
    case 1:
        if (w->x4) {
            f32 d;

            w->x4--;
            d = Muku(&em->pos, &pPLS->pos, w->x18, 0.09817477f);
            w->x18 += d;
            w->x18 = LIMIT_ANGLE(w->x18);
            em->rot.y += d;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            GameAddPoint(0xB);
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                w->x680 = 30;
                if (em39JumpUpCk3(em)) {
                    break;
                }
                if (em->plDist2 < 9000000.0f) {
                    EmRoutineSet(em, 1, 0xE, 0, 1);
                } else {
                    EmRoutineSet(em, 1, 4, 0, 0);
                }
            }
        }
        break;
    }
    em39HandSet(em, 1);
}

static void em39_R1_KnifeHit(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int end;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xAC), (int) ARC(0xAD), 3, 1, 0);
        if (w->pWep) {
            EstSet((int) w->pWep, -1, 0, 0, 0x2F, 0x19, 0, 0, (u32) w->pWep, 0);
        }
        EmCatchPLSet(em, PI, 2, (int) plem39_KnifeHit, -25.34f, 0.0f, -239.75f);
        PlGachaInit();
        w->x8B7 = 0;
        w->x10 = Rnd() & 1;
        if ((u8) (Rnd() % 10) > 4) {
            em39SetVoice(em, 0x23);
        } else {
            em39SetVoice(em, 0x24);
        }
        w->x4 = 10;
        if (pG->x4F88 <= 1) {
            w->x4 = 5;
        }
        if (pG->x4F88 <= 3) {
            w->x4 = 7;
        }
        if (pG->x4F88 > 6) {
            w->x4 = 13;
        }
        if (pG->x4F88 > 9) {
            w->x4 = 15;
        }
        em->xFE++;
    case 1:
        if ((u32) PlGachaGet() < (u32) w->x4 || !(em->seFlags28B & 1)) {
            PlGachaMove();
        } else if (w->x8B7 == 0) {
            if (w->x10) {
                ActBtn.set(0x25, 0xB, (int) em39ActOn, (int) em, 2, 3, 0, 0);
            } else {
                ActBtn.set(0x25, 0xB, (int) em39ActOn, (int) em, 2, 4, 0, 0);
            }
        }
        if (EmCatchMotionMove(em, 0.3f, 0.2f)) {
            em->xFE++;
        } else if (w->x8B7) {
            em->xFE = 4;
        }
        break;
    case 2:
        LifeDownSet(pPL, 0x47E, 0);
        if ((s16) pG->pl_life <= 0) {
            pG->pl_life = 0;
            MotionSetCore(em, MOTION(em), ARC(0xAE), (int) ARC(0xAF), 3, 1, 0);
            if (w->pWep) {
                EstSet((int) w->pWep, -1, 0, 0, 0x2F, 0x1A, 0, 0, (u32) w->pWep, 0);
            }
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xC2), (int) ARC(0xC3), 3, 1, 0);
        }
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if (w->pWep) {
                MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
            }
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                EmRoutineSet(em, 1, 0xE, 0, 1);
            }
        } else if ((s16) pG->pl_life <= 0) {
            if (em->frame > 22.7f && em->frame < 23.3f) {
                PlSetDamageSe(0xD);
            }
        } else {
            if (em->frame > 10.7f && em->frame < 11.3f) {
                PlSetDamageSe(0);
            }
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0xB0), (int) ARC(0xB1), 3, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x1C, 0, 0, (u32) em, 0);
        EmCatchPLSet(em, PI, 2, (int) plem39_KnifeHit, -82.6f, 0.0f, -230.94f);
        pPL->xFE = 4;
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
        }
        w->x4 = 10;
        em->xFE++;
    case 5:
        if (w->x4) {
            w->x4--;
            end = EmCatchMotionMove(em, 0.3f, 0.2f);
        } else {
            end = MotionMoveF(em, 0);
        }
        if (end) {
            if (em39JumpUpCk3(em)) {
                break;
            }
            EmRoutineSet(em, 1, 4, 0, 0);
        }
        break;
    }
    em39HandSet(em, 1);
}

static void plem39_KnifeHit(cPlayer* pl)
{
    int end;

    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 0xA);
    pl->subArc = ((cEm*) pl->dmgType)->subArc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x113), 0, 5, 1, 0);
        PlSetFace(1);
        pl->atari.set(0xA, 480.00003f, 400.0f);
        pl->pWep->setTrans(0, 0);
        PlSetFace(1);
        pl->xFE++;
    case 1:
        EmCatchMotionMove(pl, 0.3f, 0.2f);
        if (!EM_RTN((cEm*) pPL->dmgType, 1, 0x1B)) {
            goto END;
        }
        pl->xFE = ((cEm*) pl->dmgType)->xFE;
        break;
    case 2:
        if ((s16) pG->pl_life <= 0) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x114), 0, 5, 1, 0);
            EstSet((int) pl, -1, 0, 0, 0x2F, 0x1B, 0, 0, (u32) pl, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x11A), 0, 5, 1, 0);
            EstSet((int) pl, -1, 0, 0, 0x2F, 0x2A, 0, 0, (u32) pl, 0);
        }
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xB, 1);
        pl->xFE++;
    case 3:
        if (MotionMoveF(pl, 0) && (s16) pG->pl_life > 0) {
        END:
            pl->pWep->setTrans(1, 0);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        }
        break;
    case 4:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x115), 0, 5, 1, 0);
        pl->x3E0 = 10;
        pl->xFE++;
    case 5:
        if (pl->x3E0) {
            pl->x3E0--;
            end = EmCatchMotionMove(pl, 0.3f, 0.2f);
        } else {
            end = MotionMoveF(pl, 0);
        }
        if (end) {
            pl->pWep->setTrans(1, 0);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

// Knife4Atk: the four-step knife combo with the player caught (cases 0/2/4/6 start a swing, the odd
// cases wait for the action button).
#define EM39_K4_WAIT(em, w)                                                                         \
    if (EmCatchMotionMove(em, 0.3f, 0.2f)) {                                                       \
        (em)->xFE = 8;                                                                             \
        break;                                                                                     \
    }                                                                                              \
    if ((w)->x4 == 0) {                                                                            \
        switch ((u32) (w)->x10) {                                                                  \
        case 0:                                                                                    \
        default:                                                                                   \
            ActBtn.set(0x25, 0xB, (int) em39ActOn, (int) (em), 2, 3, 0, 0);                        \
            break;                                                                                 \
        case 1:                                                                                    \
            ActBtn.set(0x25, 0xB, (int) em39ActOn, (int) (em), 2, 4, 0, 0);                        \
            break;                                                                                 \
        }                                                                                          \
    }                                                                                              \
    if ((w)->x4) {                                                                                 \
        (w)->x4--;                                                                                 \
        break;                                                                                     \
    }                                                                                              \
    if ((w)->x8B7 == 0) {                                                                          \
        break;                                                                                     \
    }

#define EM39_K4_EFF_DELETE(em, w)                                                                   \
    EffectEspDelete(0, (w)->espKind, (u32) (em), 0);                                               \
    EffectEspgenDelete(0, (w)->espKind, (int) (em));                                               \
    EffectEfmDelete(0, (w)->espKind, (int) (em));

static void em39_R1_Knife4Atk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int end;

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xBA), (int) ARC(0xBB), 5, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem39_Knife4Atk, -288.71f, 0.0f, 1982.72f);
        w->x8B7 = 0;
        w->x10 = Rnd() & 1;
        em39WepSet(em, 1);
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
        }
        w->x4 = 12;
        if (pG->x4F88 <= 1) {
            w->x4 = 5;
        }
        if (pG->x4F88 <= 3) {
            w->x4 = 8;
        }
        if (pG->x4F88 > 6) {
            w->x4 = 13;
        }
        if (pG->x4F88 > 9) {
            w->x4 = 15;
        }
        EstSet(0, -1, 0, 0, 0x2F, 0x1D, 0, w->espKind, (u32) em, 0);
        em->xFE++;
    case 1:
        EM39_K4_WAIT(em, w);
        em->xFE++;
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xBC), (int) ARC(0xBD), 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem39_Knife4Atk, -42.88f, 0.0f, 1328.94f);
        pPL->xFE = 2;
        w->x8B7 = 0;
        w->x10 = Rnd() & 1;
        em39WepSet(em, 1);
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
        }
        w->x4 = 8;
        if (pG->x4F88 <= 1) {
            w->x4 = 3;
        }
        if (pG->x4F88 <= 3) {
            w->x4 = 6;
        }
        if (pG->x4F88 > 6) {
            w->x4 = 9;
        }
        if (pG->x4F88 > 9) {
            w->x4 = 11;
        }
        EM39_K4_EFF_DELETE(em, w);
        EstSet(0, -1, 0, 0, 0x2F, 0x1E, 0, w->espKind, (u32) em, 0);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x1F, 0, 0, (u32) em, 0);
        em39SetVoice(em, 0x19);
        em->xFE++;
    case 3:
        EM39_K4_WAIT(em, w);
        if ((u8) (Rnd() % 10) > 4) {
            em->xFE = 0xA;
        } else {
            em->xFE++;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0xBE), (int) ARC(0xBF), 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem39_Knife4Atk, 132.05f, 0.0f, 1047.06f);
        pPL->xFE = 4;
        w->x8B7 = 0;
        w->x10 = Rnd() & 1;
        em39WepSet(em, 1);
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x28), 0, 0, 1, 0);
        }
        w->x4 = 10;
        if (pG->x4F88 <= 1) {
            w->x4 = 5;
        }
        if (pG->x4F88 <= 3) {
            w->x4 = 8;
        }
        if (pG->x4F88 > 6) {
            w->x4 = 11;
        }
        if (pG->x4F88 > 9) {
            w->x4 = 13;
        }
        EM39_K4_EFF_DELETE(em, w);
        EstSet(0, -1, 0, 0, 0x2F, 0x20, 0, w->espKind, (u32) em, 0);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x21, 0, 0, (u32) em, 0);
        em39SetVoice(em, 0x19);
        em->xFE++;
    case 5:
        EM39_K4_WAIT(em, w);
        if ((u8) (Rnd() % 10) > 4) {
            em->xFE = 0xA;
        } else if (pG->x4F88 <= 1) {
            em->xFE = 0xA;
        } else {
            em->xFE++;
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0xC0), (int) ARC(0xC1), 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem39_Knife4Atk, -3.97f, 0.0f, 1927.6f);
        pPL->xFE = 6;
        w->x8B7 = 0;
        w->x10 = Rnd() & 1;
        em39WepSet(em, 1);
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x2A), 0, 0, 1, 0);
        }
        w->x4 = 11;
        if (pG->x4F88 <= 1) {
            w->x4 = 6;
        }
        if (pG->x4F88 <= 3) {
            w->x4 = 9;
        }
        if (pG->x4F88 > 6) {
            w->x4 = 12;
        }
        if (pG->x4F88 > 9) {
            w->x4 = 14;
        }
        EM39_K4_EFF_DELETE(em, w);
        EstSet(0, -1, 0, 0, 0x2F, 0x22, 0, w->espKind, (u32) em, 0);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x23, 0, 0, (u32) em, 0);
        em39SetVoice(em, 0x19);
        em->xFE++;
    case 7:
        EM39_K4_WAIT(em, w);
        em->xFE = 0xA;
        break;
    case 8:
        LifeDownSet(pPL, 0x47E, 0);
        if ((s16) pG->pl_life <= 0) {
            EmCatchPLSet(em, 0.0f, 2, (int) plem39_Knife4Atk, 220.72f, 0.0f, 1134.69f);
            pPL->xFE = 8;
            MotionSetCore(em, MOTION(em), ARC(0xB4), (int) ARC(0xB5), 0, 1, 0);
        } else {
            EmCatchPLSet(em, 0.0f, 2, (int) plem39_Knife4Atk, -451.47f, 0.0f, 1038.97f);
            pPL->xFE = 8;
            MotionSetCore(em, MOTION(em), ARC(0xB8), (int) ARC(0xB9), 0, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2F, 0x27, 0, 0, (u32) em, 0);
        }
        EM39_K4_EFF_DELETE(em, w);
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
        }
        w->x4 = 10;
        em->xFE++;
    case 9:
        if (w->x4) {
            w->x4--;
            end = EmCatchMotionMove(em, 0.3f, 0.2f);
        } else {
            end = MotionMoveF(em, 0);
        }
        if (end) {
            if ((s16) pG->pl_life > 0) {
                EmRoutineSet(em, 1, 0xE, 0, 1);
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                EmRoutineSet(em, 1, 4, 0, 0);
            }
        } else if ((s16) pG->pl_life <= 0) {
            if (em->frame > 11.7f && em->frame < 12.3f) {
                PlSetDamageSe(0xD);
            }
        } else {
            if (em->frame > 2.7f && em->frame < 3.3f) {
                PlSetDamageSe(0);
            }
        }
        break;
    case 0xA:
        MotionSetCore(em, MOTION(em), ARC(0xB6), (int) ARC(0xB7), 3, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem39_Knife4Atk, -3.97f, 0.0f, 1927.6f);
        pPL->xFE = 0xA;
        if (w->pWep) {
            MotionSetCore(w->pWep, MOTION(w->pWep), ARC(0x27), 0, 0, 5, 0);
        }
        EM39_K4_EFF_DELETE(em, w);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x26, 0, 0, (u32) em, 0);
        w->x4 = 10;
        em->xFE++;
    case 0xB:
        if (w->x4) {
            w->x4--;
            end = EmCatchMotionMove(em, 0.3f, 0.2f);
        } else {
            end = MotionMoveF(em, 0);
        }
        if (end) {
            em->xFE++;
        }
        break;
    case 0xC:
        MotionSetCore(em, MOTION(em), ARC(0x77), (int) ARC(0x78), 3, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x2B, 0, 0, (u32) em, 0);
        w->x4 = 10;
        em->xFE++;
    case 0xD:
        if (w->x4) {
            w->x4--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.19634955f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (em39JumpUpCk3(em)) {
                break;
            }
            if (em39AtkRtnCk(em)) {
                break;
            }
            EmRoutineSet(em, 1, 8, 0, 0);
        }
        break;
    }
    em39HandSet(em, 1);
}

// Player side of Knife4Atk: follow the catch motion until the enemy leaves routine 0x1C.
#define PLEM39_K4_WAIT(pl)                                                                          \
    EmCatchMotionMove(pl, 0.3f, 0.2f);                                                             \
    if (!EM_RTN((cEm*) pPL->dmgType, 1, 0x1C)) {                                                   \
        EndPlDamage();                                                                             \
        (pl)->dmg.set(0, 0x1E);                                                                    \
    }                                                                                              \
    break;

static void plem39_Knife4Atk(cPlayer* pl)
{
    int end;

    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 0xA);
    pl->subArc = ((cEm*) pl->dmgType)->subArc;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x11B), 0, 5, 1, 0);
        pl->xFE++;
    case 1:
        PLEM39_K4_WAIT(pl);
    case 2:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x11C), 0, 0, 1, 0);
        pl->xFE++;
    case 3:
        PLEM39_K4_WAIT(pl);
    case 4:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x11D), 0, 0, 1, 0);
        pl->xFE++;
    case 5:
        PLEM39_K4_WAIT(pl);
    case 6:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x11E), 0, 0, 1, 0);
        pl->xFE++;
    case 7:
        PLEM39_K4_WAIT(pl);
    case 8:
        if ((s16) pG->pl_life <= 0) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x117), 0, 0, 1, 0);
            EstSet((int) pl, -1, 0, 0, 0x2F, 0x24, 0, 0, (u32) pl, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x119), 0, 0, 1, 0);
            EstSet((int) pl, -1, 0, 0, 0x2F, 0x28, 0, 0, (u32) pl, 0);
        }
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xB, 1);
        pl->x3E0 = 10;
        pl->xFE++;
    case 9:
        if (pl->x3E0) {
            pl->x3E0--;
            end = EmCatchMotionMove(pl, 0.3f, 0.2f);
        } else {
            end = MotionMoveF(pl, 0);
        }
        if (end && (s16) pG->pl_life > 0) {
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        }
        break;
    case 0xA:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x118), 0, 0, 1, 0);
        EstSet((int) pl, -1, 0, 0, 0x2F, 0x25, 0, 0, (u32) pl, 0);
        pl->x3E0 = 10;
        pl->xFE++;
    case 0xB:
        if (pl->x3E0) {
            pl->x3E0--;
            end = EmCatchMotionMove(pl, 0.3f, 0.2f);
        } else {
            end = MotionMoveF(pl, 0);
        }
        if (end) {
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

// Machine gun pitch towards `target` from the muzzle `a` in 1/1024 turns, clamped to a byte.
#define EM39_GUN_PITCH(ang, target, a, b)                                                           \
    PSVECSubtract(&(target), &(a), &(b));                                                          \
    {                                                                                              \
        f32 d = SQRTF((b).x * (b).x + (b).z * (b).z);                                              \
        ang = -atan2f((b).y, d) * 325.94931f;                                                      \
    }                                                                                              \
    if (ang > 255.0f) {                                                                            \
        ang = 255.0f;                                                                              \
    }                                                                                              \
    if (ang < -255.0f) {                                                                           \
        ang = -255.0f;                                                                             \
    }

static void em39_R1_Atk_MG(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec target;
    Vec a;
    Vec b;
    Mtx m;
    f32 ang;
    f32 t;

    w->flags |= 0xC0;
    w->flags &= ~0x20;
    // pPLS: the pPL load waits for the flags store (struct view), which frees the sched1 slots
    // that put `&b` and its PRE copy before the first call.
    PSMTXRotRad(m, 'y', LIMIT_ANGLE(GetXZAngle(&em->pos, &pPLS->pos) + 0.08726646f));
    TransMatrix(m, &pPL->pos);
    b.x = 0.0f;
    b.y = 1500.0f;
    b.z = 0.0f;
    PSMTXMultVec(m, &b, &target);
    a = em->pos;
    a.y += 1600.0f;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x92), (int) ARC(0x93), 5, 1, 0);
        w->x4 = (((MotionData*) ARC(0x92))->maxFrame & 0x3FFF) - 20;
        em39WepSet(em, 3);
        w->x8B6 = 0;
        w->x8B7 = 0;
        w->x684 = 300;
        em->xFE++;
    case 1:
        t = LIMIT_ANGLE(GetXZAngle(&em->pos, &target) + -0.2617994f);
        em->rot.y += Muku2(em->rot.y, t, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        EM39_GUN_PITCH(ang, target, a, b);
        w->gunPitch = ang;
        w->x6D0 = 10;
        w->x6D4 = 0;
        MotionSetCore(em, MOTION(em), ARC(0x7C), 0, 0xA, 5, 0);
        w->x10 = 50;
        w->xC = 5;
        em->xFE++;
    case 3:
        t = LIMIT_ANGLE(GetXZAngle(&em->pos, &target) + -0.2617994f);
        em->rot.y += Muku2(em->rot.y, t, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        EM39_GUN_PITCH(ang, target, a, b);
        w->gunPitch = w->gunPitch * 0.9f + ang * 0.1f;
        em39BlendMotSet(em, ARC(0x7C), ARC(0x7D), ARC(0x7E), 0, 0, 0, 5);
        MotionMoveF(em, 0);
        if (w->xC) {
            w->xC--;
            break;
        }
        if ((u8) (Rnd() % 10) > 4) {
            em39SetVoice(em, 0x32);
        } else {
            em39SetVoice(em, 0x33);
        }
        em->xFE++;
        break;
    case 4:
        w->x6D0 = 0;
        w->x8 = 3;
        w->x6D4 = 0;
        if (w->pWep2) {
            MotionSetCore(w->pWep2, MOTION(w->pWep2), ARC(0x2F), 0, 0, 1, 0);
        }
        em->xFE++;
    case 5:
        em->rot.y += Muku(&em->pos, &target, em->rot.y, 0.012271847f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        EM39_GUN_PITCH(ang, target, a, b);
        w->gunPitch = w->gunPitch * 0.9f + ang * 0.1f;
        em39BlendMotSet(em, ARC(0x7F), ARC(0x80), ARC(0x81), 0, 0, 0, 5);
        if (MotionMoveF(em, 0)) {
            if (w->x8B6 == 0) {
                GameAddPoint(0xB);
            }
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (em->xFF) {
                EmRoutineSet(em, 1, 0x1E, 0, 1);
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                EmRoutineSet(em, 1, 0xE, 0, 2);
            }
        } else {
            if (w->x10) {
                if (w->x8 == 3) {
                    em39WaistMove(em);
                    em->partsWorldCalc();
                    em39GunHitCk(em);
                    em39SetCartridge(em);
                }
                if (w->x8) {
                    w->x8--;
                    if (w->x8 == 0) {
                        w->x10--;
                        em->xFE = 4;
                        break;
                    }
                }
            }
            if (em39DeadCk(pPL) && (u32) w->x10 > 5) {
                w->x10 = 5;
            }
            if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) > 1.5707964f) {
                w->x10 = 0;
            }
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0x8E), (int) ARC(0x8F), 0xA, 1, 0);
        em->xFE++;
    case 7:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            if (em->xFF) {
                EmRoutineSet(em, 1, 0x1E, 0, 1);
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                EmRoutineSet(em, 1, 0xE, 0, 2);
            }
        }
        break;
    }
    em39HandSet(em, 2);
}

static void em39_R1_Reload(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x90), (int) ARC(0x91), 5, 1, 0);
        em39WepSet(em, 3);
        if (w->pWep2) {
            MotionSetCore(w->pWep2, MOTION(w->pWep2), ARC(0x30), 0, 0, 1, 0);
        }
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (em->xFF && (u8) (Rnd() % 10) > 3) {
                if ((u8) (Rnd() % 10) > 4 && em->plDist2 > 25000000.0f) {
                    EmRoutineSet(em, 1, 0x1D, 0, 1);
                } else {
                    EmRoutineSet(em, 1, 0x23, 0, 1);
                }
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                if (w->targetAngAbs > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 8, 0, 0);
                }
            }
        }
        break;
    }
    em39HandSet(em, 2);
}

static void em39_R1_AppearMG(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec target;
    Vec a;
    Vec b;
    Mtx m;
    f32 ang;
    f32 t;

    w->flags |= 0xC0;
    w->flags &= ~0x20;
    GetPlPos(&target, 0, 10.0f);
    target.y += 1300.0f;
    PSVECSubtract(&target, &em->pos, &b);
    PSMTXRotRad(m, 'y', 0.018325957f);
    TransMatrix(m, &em->pos);
    PSMTXMultVec(m, &b, &target);
    a = em->pos;
    a.y += 1400.0f;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x8C), (int) ARC(0x8D), 0, 1, 0);
        AtariOn(&em->atari, 0x300);
        em->be_flag |= 2;
        w->x4 = 10;
        em39WepSet(em, 3);
        w->x8B7 = 0;
        w->x698 = 200;
        em->xFE++;
    case 1:
        t = LIMIT_ANGLE(GetXZAngle(&em->pos, &target) + -0.2617994f);
        em->rot.y += Muku2(em->rot.y, t, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        EM39_GUN_PITCH(ang, target, a, b);
        w->gunPitch = ang;
        w->x6D0 = 10;
        w->x6D4 = 0;
        MotionSetCore(em, MOTION(em), ARC(0x7C), 0, 0xA, 5, 0);
        w->x10 = 50;
        w->xC = 30;
        em->xFE++;
    case 3:
        t = LIMIT_ANGLE(GetXZAngle(&em->pos, &target) + -0.2617994f);
        em->rot.y += Muku2(em->rot.y, t, 0.19634955f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        EM39_GUN_PITCH(ang, target, a, b);
        w->gunPitch = w->gunPitch * 0.9f + ang * 0.1f;
        em39BlendMotSet(em, ARC(0x7C), ARC(0x7D), ARC(0x7E), 0, 0, 0, 5);
        MotionMoveF(em, 0);
        if (w->xC) {
            w->xC--;
            break;
        }
        if ((u8) (Rnd() % 10) > 4) {
            em39SetVoice(em, 0x32);
        } else {
            em39SetVoice(em, 0x33);
        }
        em->xFE++;
        break;
    case 4:
        w->x6D0 = 0;
        w->x8 = 3;
        w->x6D4 = 0;
        if (w->pWep2) {
            MotionSetCore(w->pWep2, MOTION(w->pWep2), ARC(0x2F), 0, 0, 1, 0);
        }
        em->xFE++;
    case 5:
        em->rot.y += Muku(&em->pos, &target, em->rot.y, 0.0061359233f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        EM39_GUN_PITCH(ang, target, a, b);
        w->gunPitch = w->gunPitch * 0.9f + ang * 0.1f;
        em39BlendMotSet(em, ARC(0x7F), ARC(0x80), ARC(0x81), 0, 0, 0, 5);
        if (MotionMoveF(em, 0)) {
            if (w->x8B6 == 0) {
                GameAddPoint(0xB);
            }
            em->xFE++;
        } else {
            if (w->x10) {
                if (w->x8 == 3) {
                    w->x10--;
                    em39WaistMove(em);
                    em->partsWorldCalc();
                    em39GunHitCk(em);
                    em39SetCartridge(em);
                }
                if (w->x8) {
                    w->x8--;
                    if (w->x8 == 0) {
                        em->xFE = 4;
                        break;
                    }
                }
            }
            if (em39DeadCk(pPL) && (u32) w->x10 > 5) {
                w->x10 = 5;
            }
            if (em39ExitCk(em) && (u32) w->x10 > 5) {
                w->x10 = 5;
            }
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0x8E), (int) ARC(0x8F), 0xA, 1, 0);
        if (w->x8B6) {
            em39SetSpeech(em, 0x5A, 0x20);
        }
        em->xFE++;
    case 7:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            w->x680 = (u8) (Rnd() % 45) + 90;
            EmRoutineSet(em, 1, 5, 0, 0);
        }
        break;
    }
    em39HandSet(em, 2);
}

// The same pitch with the negated atan2 kept in a function-scope variable (AppearMG2).
#define EM39_GUN_PITCH2(ang, t, target, a, b)                                                       \
    PSVECSubtract(&(target), &(a), &(b));                                                          \
    {                                                                                              \
        f32 d = SQRTF((b).x * (b).x + (b).z * (b).z);                                              \
        ang = -atan2f((b).y, d);                                                                   \
    }                                                                                              \
    t = ang * 325.94931f;                                                                          \
    if (t > 255.0f) {                                                                              \
        t = 255.0f;                                                                                \
    }                                                                                              \
    if (t < -255.0f) {                                                                             \
        t = -255.0f;                                                                               \
    }

static void em39_R1_AppearMG2(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec target;
    Vec a;
    Vec b;
    Mtx m;
    f32 ang;
    f32 t;

    w->flags |= 0xC0;
    w->flags &= ~0x20;
    GetPlPos(&target, 0, 10.0f);
    target.y += 1500.0f;
    PSVECSubtract(&target, &em->pos, &b);
    PSMTXRotRad(m, 'y', 0.018325957f);
    TransMatrix(m, &em->pos);
    PSMTXMultVec(m, &b, &target);
    a = em->pos;
    a.y += 1600.0f;
    switch (em->xFE) {
    case 0:
        if (Muku(&em->pos, &pPL->pos, em->rot.y, PI) < 0.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x83), (int) ARC(0x84), 3, 1, 0);
            w->x4 = (((MotionData*) ARC(0x83))->maxFrame & 0x3FFF) - 20;
            em->xFF = 1;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x88), (int) ARC(0x89), 3, 1, 0);
            w->x4 = (((MotionData*) ARC(0x88))->maxFrame & 0x3FFF) - 20;
            em->xFF = 0;
        }
        AtariOn(&em->atari, 0x300);
        em->be_flag |= 2;
        w->x4 = 10;
        em39WepSet(em, 3);
        w->x8B7 = 0;
        w->x698 = 200;
        w->x18 = em->rot.y + PI;
        em->xFE++;
    case 1: {
        ang = Muku(&em->pos, &target, w->x18, 0.09817477f);
        w->x18 += ang;
        w->x18 = LIMIT_ANGLE(w->x18);
        em->rot.y += ang;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    }
    case 2:
        EM39_GUN_PITCH2(ang, t, target, a, b);
        w->gunPitch = t;
        w->x6D0 = 10;
        w->x6D4 = 0;
        MotionSetCore(em, MOTION(em), ARC(0x7C), 0, 0xA, 5, 0);
        w->x10 = 50;
        w->xC = 5;
        em->xFE++;
    case 3:
        em->rot.y += Muku(&em->pos, &target, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        EM39_GUN_PITCH2(ang, t, target, a, b);
        w->gunPitch = w->gunPitch * 0.9f + t * 0.1f;
        em39BlendMotSet(em, ARC(0x7C), ARC(0x7D), ARC(0x7E), 0, 0, 0, 5);
        MotionMoveF(em, 0);
        if (w->xC) {
            w->xC--;
            break;
        }
        if ((u8) (Rnd() % 10) > 4) {
            em39SetVoice(em, 0x32);
        } else {
            em39SetVoice(em, 0x33);
        }
        em->xFE++;
        break;
    case 4:
        w->x6D0 = 0;
        w->x8 = 3;
        w->x6D4 = 0;
        if (w->pWep2) {
            MotionSetCore(w->pWep2, MOTION(w->pWep2), ARC(0x2F), 0, 0, 1, 0);
        }
        em->xFE++;
    case 5:
        EM39_GUN_PITCH2(ang, t, target, a, b);
        w->gunPitch = w->gunPitch * 0.9f + t * 0.1f;
        em39BlendMotSet(em, ARC(0x7F), ARC(0x80), ARC(0x81), 0, 0, 0, 5);
        if (MotionMoveF(em, 0)) {
            if (w->x8B6 == 0) {
                GameAddPoint(0xB);
            }
            if (em->plDist2 < 9000000.0f || (u8) (Rnd() % 10) > 6) {
                EmRoutineSet(em, 1, 8, 0, 0);
            } else {
                em->xFE++;
            }
        } else {
            if (w->x10) {
                if (w->x8 == 3) {
                    w->x10--;
                    em39WaistMove(em);
                    em->partsWorldCalc();
                    em39GunHitCk(em);
                    em39SetCartridge(em);
                }
                if (w->x8) {
                    w->x8--;
                    if (w->x8 == 0) {
                        em->xFE = 4;
                        break;
                    }
                }
            }
            if (em39DeadCk(pPL) && (u32) w->x10 > 5) {
                w->x10 = 5;
            }
        }
        break;
    case 6:
        if (em->xFF) {
            MotionSetCore(em, MOTION(em), ARC(0x85), (int) ARC(0x86), 3, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x8A), (int) ARC(0x8B), 3, 1, 0);
        }
        em->xFE++;
    case 7:
        w->flags &= ~0x40;
        if (MotionMoveF(em, 0)) {
            w->x680 = (u8) (Rnd() % 30) + 60;
            EmRoutineSet(em, 1, 7, 0, 0);
        }
        break;
    }
    em39HandSet(em, 2);
}

#define PL_ARC(no) PL_ARC_PTR(pG->pPlArc, no)

// Throw the grenade in hand towards the player (AppearGR / AppearGR2 hit frame).
#define EM39_GRENADE_THROW(em, w, spd)                                                              \
    (w)->pGrenade->setGrenadeThrow(&(spd), 60, ARC(0x10B), ARC(0x10C), ARC(0x10D), ARC(0x111));   \
    (w)->pGrenade = 0;                                                                             \
    em39WepSet(em, 0);

static void em39_R1_AppearGR(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec target;
    Vec a;
    Vec b;

    w->flags |= 0x50;
    w->flags &= ~0x20;
    if (w->x8B7) {
        target = pPL->pos;
        target.y += 1300.0f;
    } else {
        Mtx m;

        GetPlPos(&target, 0, 10.0f);
        target.y += 1300.0f;
        PSVECSubtract(&target, &em->pos, &b);
        PSMTXRotRad(m, 'y', 0.008726646f);
        TransMatrix(m, &em->pos);
        PSMTXMultVec(m, &b, &target);
    }
    a = em->pos;
    a.y += 1600.0f;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x94), (int) ARC(0x95), 3, 1, 0);
        AtariOn(&em->atari, 0x300);
        em->be_flag |= 2;
        w->x8B7 = 0;
        w->x698 = 200;
        if (w->x8A4 > 450) {
            u8 r = Rnd() % 3;

            switch (r) {
            case 0:
            default:
                em39SetVoice(em, 0x26);
                break;
            case 1:
                em39SetVoice(em, 0x27);
                break;
            case 2:
                em39SetVoice(em, 0x28);
                break;
            }
            w->x8A4 = 0;
        }
        w->x4 = 30;
        em->xFE++;
    case 1:
        if (w->x4) {
            w->x4--;
            em->rot.y += Muku(&em->pos, &target, em->rot.y, 0.19634955f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->seFlags28B & 2) {
            if (w->pGrenade == 0) {
                Vec pos;
                Vec rot;

                pos.x = 0.0f;
                pos.y = 0.0f;
                pos.z = 0.0f;
                rot.x = 0.0f;
                rot.y = 0.0f;
                rot.z = 0.0f;
                w->pGrenade = SetWeapon(PL_ARC(0x6A), PL_ARC(0x6B), &pos, &rot, 0);
            }
            if (w->pGrenade) {
                w->pGrenade->setParent(em, 0xA, 0);
            }
            em39WepSet(em, 2);
        }
        if ((em->seFlags28B & 1) && w->pGrenade) {
            Vec spd;
            f32 d = SQRTF((em->pos.x - pPL->pos.x) * (em->pos.x - pPL->pos.x) + (em->pos.z - pPL->pos.z) * (em->pos.z - pPL->pos.z)) - 2000.0f;

            if (d < 5000.0f) {
                d = 5000.0f;
            }
            d *= 0.022222223f;
            spd.x = 0.0f;
            spd.y = 250.0f;
            spd.z = d;
            PSMTXMultVecSR(em->mat, &spd, &spd);
            EM39_GRENADE_THROW(em, w, spd);
        }
        if (MotionMoveF(em, 0)) {
            w->x680 = (u8) (Rnd() % 45) + 90;
            EmRoutineSet(em, 1, 5, 0, 0);
        }
        break;
    }
    em39HandSet(em, 0);
}

static void em39_R1_AppearGR2(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec target;
    Vec pos;
    Vec rot;
    Mtx m;
    Vec spd;

    w->flags |= 0x80;
    w->flags &= ~0x20;
    target = pPLS->getPartsPtr(4)->worldPos;
    switch (em->xFE) {
    case 0:
        if (Muku(&em->pos, &pPL->pos, em->rot.y, PI) < 0.0f) {
            MotionSetCore(em, MOTION(em), ARC(0x97), (int) ARC(0x98), 3, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x99), (int) ARC(0x9A), 3, 1, 0);
        }
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pGrenade = SetWeapon(PL_ARC(0x6A), PL_ARC(0x6B), &pos, &rot, 0);
        if (w->pGrenade) {
            w->pGrenade->setParent(em, 0xA, 0);
        }
        em39WepSet(em, 2);
        AtariOn(&em->atari, 0x300);
        em->be_flag |= 2;
        w->x8B7 = 0;
        w->x698 = 200;
        w->x18 = em->rot.y + PI;
        em->xFE++;
    case 1:
        if ((em->seFlags28B & 1) && w->pGrenade) {
            f32 d = (SQRTF((em->pos.x - pPL->pos.x) * (em->pos.x - pPL->pos.x) + (em->pos.z - pPL->pos.z) * (em->pos.z - pPL->pos.z)) - 1000.0f) * 0.025f;

            if (d < 200.0f) {
                d = 200.0f;
            }
            spd.x = 0.0f;
            spd.y = 180.0f;
            spd.z = d;
            PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &pPL->pos));
            PSMTXMultVecSR(m, &spd, &spd);
            EM39_GRENADE_THROW(em, w, spd);
        }
        if (MotionMoveF(em, 0)) {
            w->x680 = (u8) (Rnd() % 30) + 60;
            EmRoutineSet(em, 1, 7, 0, 0);
        }
        break;
    }
    em39HandSet(em, 0);
}

static void em39_R1_ThrowGR(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec hand;

    Vec p1 = { 28961.0f, 5250.0f, -2620.0f };
    Vec p2 = { 31552.0f, 5250.0f, -6063.0f };
    Vec p3 = { 31546.0f, 5250.0f, -11491.0f };
    w->flags |= 0x80;
    w->flags &= ~0x20;
    hand = pPLS->getPartsPtr(4)->worldPos;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x9B), (int) ARC(0x9C), 3, 1, 0);
        w->x684 = 600;
        w->x4 = 15;
        w->x8B7 = 0;
        w->x8B6 = 0;
        w->x10 = 0;
        if (pPLS->pos.x < 30580.0f && em->xFF) { // struct view: the load then depends on the word stores too and the block issues in source order
            w->x10 = 2;
            if (em->pos.z > -5360.0f) {
                w->x10 = 1;
            }
            if (em->pos.z < -8792.0f) {
                w->x10 = 3;
            }
        }
        em->xFE++;
    case 1:
        if (w->x4) {
            w->x4--;
            switch ((u32) w->x10) {
            case 0:
            default:
                em->rot.y += Muku(&em->pos, &pPL->pos, em->rot.y, 0.19634955f);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
            case 1:
                em->rot.y += Muku(&em->pos, &p1, em->rot.y, 0.19634955f);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
                break;
            case 2:
                em->rot.y += Muku(&em->pos, &p2, em->rot.y, 0.19634955f);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
                break;
            case 3:
                em->rot.y += Muku(&em->pos, &p3, em->rot.y, 0.19634955f);
                em->rot.y = LIMIT_ANGLE(em->rot.y);
                break;
            }
        }
        if (em->seFlags28B & 2) {
            if (w->pGrenade == 0) {
                Vec pos;
                Vec rot;

                pos.x = 0.0f;
                pos.y = 0.0f;
                pos.z = 0.0f;
                rot.x = 0.0f;
                rot.y = 0.0f;
                rot.z = 0.0f;
                w->pGrenade = SetWeapon(PL_ARC(0x6A), PL_ARC(0x6B), &pos, &rot, 0);
            }
            if (w->pGrenade) {
                w->pGrenade->setParent(em, 0xA, 0);
            }
            em39WepSet(em, 2);
        }
        if ((em->seFlags28B & 1) && w->pGrenade) {
            Vec spd;
            Mtx m;
            Vec tpos;
            f32 d;

            switch ((u32) w->x10) {
            case 0:
            default:
                tpos = pPL->pos;
                break;
            case 1:
                tpos = p1;
                break;
            case 2:
                tpos = p2;
                break;
            case 3:
                tpos = p3;
                break;
            }
            d = (SQRTF((em->pos.x - tpos.x) * (em->pos.x - tpos.x) + (em->pos.z - tpos.z) * (em->pos.z - tpos.z)) - 1000.0f) * 0.025f;
            if (d < 200.0f) {
                d = 200.0f;
            }
            spd.x = 0.0f;
            spd.y = 120.0f;
            spd.z = d;
            if (em->pos.y > tpos.y + 2000.0f) {
                spd.y = 0.0f;
            }
            asm volatile("" : : "f"(d)); // COMPILER-DIFF: #13 (dying stores issued in source order)
            PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &tpos));
            PSMTXMultVecSR(m, &spd, &spd);
            w->pGrenade->setGrenadeThrow(&spd, 45, ARC(0x10B), ARC(0x10C), ARC(0x10D), ARC(0x111));
            w->pGrenade = 0;
            em39WepSet(em, 0);
        }
        if (MotionMoveF(em, 0) || (em->seFlags28B & 4)) {
            if (em->xFF) {
                em->xFF--;
                if (em->xFF == 0) {
                    EmRoutineSet(em, 1, 0xC, 0, 1);
                } else {
                    em->xFC = 1;
                    em->xFD = 0x23;
                    em->xFE = 0;
                }
            } else {
                if (em39JumpUpCk3(em)) {
                    break;
                }
                w->x680 = 30;
                EmRoutineSet(em, 1, 0xE, 0, 1);
            }
        }
        break;
    }
    em39HandSet(em, 0);
}

static void em39_R1_AppearBow(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec target;
    Vec a;
    Vec b;
    f32 ang;

    w->flags |= 0x50;
    w->flags &= ~0x20;
    if (w->x8B7) {
        target = pPL->pos;
        target.y += 1300.0f;
    } else {
        Mtx m;

        GetPlPos(&target, 0, 10.0f);
        target.y += 1300.0f;
        PSVECSubtract(&target, &em->pos, &b);
        PSMTXRotRad(m, 'y', 0.008726646f);
        TransMatrix(m, &em->pos);
        PSMTXMultVec(m, &b, &target);
    }
    a = em->pos;
    a.y += 1600.0f;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xC4), (int) ARC(0xC5), 0xA, 1, 0);
        w->x4 = (((MotionData*) ARC(0xC4))->maxFrame & 0x3FFF) - 15;
        em39WepSet(em, 4);
        em39BowSet(em, 0);
        if (w->pWep3) {
            MotionSetCore(w->pWep3, MOTION(w->pWep3), ARC(0x33), 0, 0, 0, 0);
        }
        em39ArrowSet(em);
        w->x10 = (u8) (Rnd() % 3) + 3;
        w->xC = 30;
        w->x8B7 = 0;
        em39HandSet(em, 3);
        em->be_flag |= 2;
        w->x698 = 200;
        if (w->x8A4 > 450) {
            u8 r = Rnd() % 3;

            switch (r) {
            case 0:
            default:
                em39SetVoice(em, 0x26);
                break;
            case 1:
                em39SetVoice(em, 0x27);
                break;
            case 2:
                em39SetVoice(em, 0x28);
                break;
            }
            w->x8A4 = 0;
        }
        em->xFE++;
    case 1:
        em->rot.y += Muku(&em->pos, &target, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        } else if (em->seFlags28B & 0x10) {
            em39BowSet(em, 1);
        }
        break;
    case 2:
        EM39_GUN_PITCH(ang, target, a, b);
        w->gunPitch = ang;
        w->x6D0 = 10;
        w->x6D4 = 0;
        w->x8B6 = 0;
        w->x8B7 = 0;
        em->xFE++;
    case 3:
        em->rot.y += Muku(&em->pos, &target, em->rot.y, 0.09817477f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        PSVECSubtract(&target, &a, &b);
        {
            f32 d = SQRTF(b.x * b.x + b.z * b.z);

            ang = -atan2f(b.y, d) * 257.32843f;
        }
        if (ang > 255.0f) {
            ang = 255.0f;
        }
        if (ang < -255.0f) {
            ang = -255.0f;
        }
        w->gunPitch = w->gunPitch * 0.8f + ang * 0.2f;
        em39BlendMotSet(em, ARC(0xC6), ARC(0xC7), ARC(0xC8), 0, 0, 0, 5);
        MotionMoveF(em, 0);
        if (em39ExitCk(em)) {
            em->xFE = 6;
        }
        if (w->xC) {
            w->xC--;
            break;
        }
        em->xFE++;
        break;
    case 4:
        w->x6D0 = 0;
        w->x6D4 = 0;
        if ((u32) w->x10 > 1) {
            w->bowMot0 = ARC(0xC9);
            w->bowMot3 = ARC(0xCA);
            w->bowMot1 = ARC(0xCB);
            w->bowMot2 = ARC(0xCC);
            if (w->pWep3) {
                MotionSetCore(w->pWep3, MOTION(w->pWep3), ARC(0x34), 0, 0, 0, 0);
            }
        } else {
            w->bowMot0 = ARC(0xCE);
            w->bowMot3 = 0;
            w->bowMot1 = ARC(0xCF);
            w->bowMot2 = ARC(0xD0);
            if (w->pWep3) {
                MotionSetCore(w->pWep3, MOTION(w->pWep3), ARC(0x35), 0, 0, 0, 0);
            }
        }
        SndCall(8, 0x12, &em->pos, em->id, 0, em);
        em39ArrowFire(em, &target, (u8) (Rnd() % 3));
        em39HandSet(em, 0);
        w->x4 = 5;
        em->xFE++;
    case 5:
        PSVECSubtract(&target, &a, &b);
        {
            f32 d = SQRTF(b.x * b.x + b.z * b.z);

            ang = -atan2f(b.y, d) * 261.92355f;
        }
        if (ang > 255.0f) {
            ang = 255.0f;
        }
        if (ang < -255.0f) {
            ang = -255.0f;
        }
        w->gunPitch = w->gunPitch * 0.8f + ang * 0.2f;
        em39BlendMotSet(em, w->bowMot0, w->bowMot1, w->bowMot2, (int) w->bowMot3, 0, 0, 1);
        if ((s16) pG->pl_life <= 0) {
            w->x10 = 0;
        }
        if (pPL->xFC == 1) {
            w->x10 = 0;
        }
        if (em39ExitCk(em)) {
            w->x10 = 0;
        }
        if (MotionMoveF(em, 0)) {
            if (w->x10) {
                w->x10--;
                if (w->x10) {
                    w->xC = 50;
                    em->xFE = 2;
                    break;
                }
            }
            em->xFE++;
        } else {
            if (em->seFlags28B & 0x10) {
                em39BowSet(em, 1);
            }
            if (em->seFlags28B & 2) {
                em39ArrowSet(em);
                em39HandSet(em, 3);
            }
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0xD1), (int) ARC(0xD2), 0xA, 1, 0);
        w->x4 = 30;
        em->xFE++;
    case 7:
        if (MotionMoveF(em, 0)) {
            w->x680 = (u8) (Rnd() % 45) + 90;
            EmRoutineSet(em, 1, 5, 0, 0);
        }
        break;
    }
}

static void em39_R1_Flash(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec pos;
    Vec rot;
    Vec spd;
    int end;

    w->flags |= 0x130;
    switch (em->xFE) {
    case 0: {
        MotionSetCore(em, MOTION(em), ARC(0x9B), (int) ARC(0x9C), 6, 1, 0);
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pFlash = SetWeapon(PL_ARC(0x6A), PL_ARC(0x6F), &pos, &rot, 0);
        if (w->pFlash) {
            w->pFlash->setParent(em, 0xA, 0);
        }
        em39WepSet(em, 2);
        w->x4 = 0;
        em->xFE++;
    }
    case 1:
        end = MotionMoveF(em, 0);
        if (end) {
            w->x898 = 200;
            EmRoutineSet(em, 1, 0x26, 0, 0);
            break;
        }
        if ((em->seFlags28B & 1) && w->pFlash) {
            spd.x = 0.0f;
            spd.y = 100.0f;
            spd.z = 150.0f;
            PSMTXMultVecSR(em->mat, &spd, &spd);
            w->x4 = 28;
            w->pFlash->setFlashThrow(&spd, 28);
            w->pFlash = 0;
        }
        if (w->x4) {
            w->x4--;
            if (w->x4 == 0) {
                w->x898 = 200;
                EmRoutineSet(em, 1, 0x26, 0, 0);
            }
        }
        break;
    }
    em39HandSet(em, 0);
}

static void em39_R1_Hide(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int lim;

    em->dmType = 2;
    w->flags |= 0x430;
    if (w->flags & 0x20000) {
        w->x8C4 = 5;
    }
    switch (em->xFE) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em39WepSet(em, 0);
        if (w->x58C) {
            w->x58C->setLost();
            w->x58C = 0;
        }
        em->be_flag |= 0x10000000;
        em->setStatus(1);
        w->x89C = 450;
        w->x69C = 0;
        w->gotoOn = 0;
        if (w->x8C4 == 1) {
            w->x8C4 = 2;
        }
        if (w->x8C4 == 4) {
            w->x69C = 0;
            w->x8C4 = 5;
        }
        w->pGotoPoint = 0;
        em->be_flag &= ~2;
        em->xFE++;
    case 1:
        MotionSetCore(em, MOTION(em), ARC(0x73), 0, 0, 0, 0);
        MotionMoveF(em, 0);
        if (w->x8C4 == 5) {
            w->flags |= 0x100000;
        }
        switch (w->x8C4) {
        case 0:
        case 3:
        default:
            lim = 1000;
            break;
        case 1:
            lim = 500;
            break;
        case 2:
            lim = 500;
            break;
        case 4:
            lim = 500;
            break;
        case 5:
            lim = 0;
            break;
        }
        if (w->x69C < lim && w->x898 == 0 && em39AppearCk(em)) {
            em->be_flag &= ~0x10000000;
            em->clearStatus(1);
        }
        break;
    }
}

static void em39_R1_br_T_Atk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    if (em->hp > 0 && (em->seFlags28B & 1) && w->x8B6 == 0) {
        em39LeftArmAtkCk(em, 4);
        if (w->x8B6) {
            SndCall(8, 0x3D, &pPL->pos, em->id, 0, pPL);
            if (em39GetCliffPos(em)) {
                if ((s16) pG->pl_life <= 0) {
                    pG->pl_life = 1;
                }
                em->stat = 0x012E0000;
            } else {
                EmPlBloodSet2(em, &em->pos, 1, 0x2F, 0x2C);
                pPL->rot.y = GetXZAngle(&pPL->pos, &em->pos);
                PlSetDamage(8, 0, 0);
            }
        }
    }
}

// The mutated-arm swings share the wait / cancel handling.
#define EM39_T_ATK_TAIL(em, w, action)                                                              \
    if (em->seFlags28B & 2) {                                                                      \
        w->x8B7 = 1;                                                                               \
    }                                                                                              \
    if (w->x8) {                                                                                   \
        w->x8--;                                                                                   \
    } else if (w->x8B6 == 0 && w->x8B7 == 0) {                                                     \
        switch ((u32) w->x10) {                                                                    \
        case 0:                                                                                    \
        default:                                                                                   \
            ActBtn.set(0x25, 0xB, (int) action, (int) em, 1, 3, 0, 0);                             \
            break;                                                                                 \
        case 1:                                                                                    \
            ActBtn.set(0x25, 0xB, (int) action, (int) em, 1, 4, 0, 0);                             \
            break;                                                                                 \
        }                                                                                          \
    }

#define EM39_T_ATK_INIT(em, w, est)                                                                 \
    EstSet((int) em, -1, 0, 0, 0x2F, est, 0, w->espKind, (u32) em, 0);                             \
    w->x8BB = 4;                                                                                   \
    w->x8 = 6;                                                                                     \
    if (pG->x4F88 <= 1) {                                                                          \
        w->x8 = 0;                                                                                 \
    }                                                                                              \
    if (pG->x4F88 <= 3) {                                                                          \
        w->x8 = 3;                                                                                 \
    }                                                                                              \
    if (pG->x4F88 > 6) {                                                                           \
        w->x8 = 7;                                                                                 \
    }                                                                                              \
    if (pG->x4F88 > 9) {                                                                           \
        w->x8 = 9;                                                                                 \
    }                                                                                              \
    w->x10 = Rnd() & 1;                                                                            \
    if (pGS->x4F88 <= 3) {                                                                         \
        w->x10 = 0;                                                                                \
    }                                                                                              \
    w->x8B7 = 0;                                                                                   \
    w->x8B6 = 0;

#define EM39_T_ATK_END(em, w, end)                                                                  \
    if (end) {                                                                                     \
        int zero = 0;                                                                              \
                                                                                                   \
        w->x8BB = zero;                                                                            \
        if ((s16) pG->pl_life <= 0) {                                                              \
            EmRoutineSet(em, 1, 4, zero, zero);                                                    \
        } else {                                                                                   \
            w->x680 = 30;                                                                          \
            EmRoutineSet(em, 1, 0xE, zero, 1);                                                     \
        }                                                                                          \
    } else if (em->seFlags28B & 4) {                                                               \
        if (w->x8B6 == 0) {                                                                        \
            GameAddPoint(0xB);                                                                     \
        }                                                                                          \
        EmRoutineSet(em, 1, 0xE, end, 1);                                                          \
    }

static void em39_R1_T_Atk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int end;

    w->flags |= 0x80;
    w->flags &= ~0x20;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xDB), (int) ARC(0xDC), 3, 1, 0);
        EM39_T_ATK_INIT(em, w, 0x30);
        em->xFE++;
    case 1:
        if (em->seFlags28B & 8) {
            em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.19634955f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        end = MotionMoveF(em, 0);
        EM39_T_ATK_END(em, w, end);
        break;
    }
    EM39_T_ATK_TAIL(em, w, em39BackjumpAction);
}

static void em39_R1_T_BackKnuckle(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int end;

    w->flags |= 0x80;
    w->flags &= ~0x20;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xDD), (int) ARC(0xDE), 3, 1, 0);
        EM39_T_ATK_INIT(em, w, 0x31);
        em->xFE++;
    case 1:
        if (em->seFlags28B & 8) {
            em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.19634955f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->seFlags28B & 1) {
            em39LeftArmAtkCk(em, 5);
        }
        end = MotionMoveF(em, 0);
        EM39_T_ATK_END(em, w, end);
        break;
    }
    EM39_T_ATK_TAIL(em, w, em39SitAction);
}

static void em39_R1_br_T_LongAtk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    if (em->hp > 0 && (em->seFlags28B & 1) && w->x8B6 == 0) {
        em39LeftArmAtkCk(em, 6);
        if (w->x8B6) {
            SndCall(8, 0x3D, &pPL->pos, em->id, 0, pPL);
            if (em39GetCliffPos(em)) {
                if ((s16) pG->pl_life <= 0) {
                    pG->pl_life = 1;
                }
                em->stat = 0x012E0000;
            } else {
                EmPlBloodSet2(em, &em->pos, 1, 0x2F, 0x2C);
                pPL->rot.y = GetXZAngle(&pPL->pos, &em->pos);
                PlSetDamage(8, 0, 0);
            }
        }
    }
}

// The long swings offer the dodge button only while the enemy faces the player.
#define EM39_T_LONG_TAIL(em, w, action)                                                             \
    if (w->x8) {                                                                                   \
        w->x8--;                                                                                   \
    } else {                                                                                       \
        f32 a = fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI));                                   \
                                                                                                   \
        if (w->x8B6 == 0 && w->x8B7 == 0 && a < 1.5707964f) {                                      \
            switch ((u32) w->x10) {                                                                \
            case 0:                                                                                \
            default:                                                                               \
                ActBtn.set(0x25, 0xB, (int) action, (int) em, 1, 3, 0, 0);                         \
                break;                                                                             \
            case 1:                                                                                \
                ActBtn.set(0x25, 0xB, (int) action, (int) em, 1, 4, 0, 0);                         \
                break;                                                                             \
            }                                                                                      \
        }                                                                                          \
    }

static void em39_R1_T_LongAtk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int end;

    w->flags |= 0x80;
    w->flags &= ~0x20;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xDF), (int) ARC(0xE0), 3, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x32, 0, w->espKind, (u32) em, 0);
        w->x8BB = 4;
        w->x4 = 30;
        w->x684 = 450;
        w->x8 = 15;
        w->x8B6 = 0;
        w->x8B7 = 0;
        if (pGS->x4F88 <= 1) {
            w->x8 = 5;
        }
        if (pG->x4F88 <= 3) {
            w->x8 = 10;
        }
        if (pG->x4F88 > 6) {
            w->x8 = 16;
        }
        if (pG->x4F88 > 9) {
            w->x8 = 18;
        }
        w->x10 = Rnd() & 1;
        if (pGS->x4F88 <= 3) {
            w->x10 = 0;
        }
        em->xFE++;
    case 1:
        if (w->x4) {
            w->x4--;
            w->flags |= 0x100;
        }
        if (em->seFlags28B & 8) {
            em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.19634955f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        end = MotionMoveF(em, 0);
        if (end) {
            int zero = 0;

            w->x8BB = zero;
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, zero, zero);
            } else {
                w->x680 = 30;
                EmRoutineSet(em, 1, 0xE, zero, 1);
            }
        } else if (em->seFlags28B & 4) {
            if (w->x8B6 == 0) {
                GameAddPoint(0xB);
            }
            EmRoutineSet(em, 1, 0xE, end, 1);
        } else if (em->seFlags28B & 2) {
            w->x8B7 = 1;
        }
        break;
    }
    EM39_T_LONG_TAIL(em, w, em39SitAction);
}

static void em39_R1_T_JumpAtk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->flags |= 0x80;
    w->flags &= ~0x20;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE5), (int) ARC(0xE6), 3, 1, 0);
        if (pG->x4FB8 == 2) {
            EstSet((int) em, -1, 0, 0, 0x2F, 0x4B, 0, w->espKind, (u32) em, 0);
        } else {
            EstSet((int) em, -1, 0, 0, 0x2F, 0x3B, 0, w->espKind, (u32) em, 0);
        }
        EstSet((int) em, -1, 0, 0, 0x2F, 0x3C, 0, w->espKind, (u32) em, 0);
        w->x684 = 450;
        w->x4 = 15;
        w->x8B6 = 0;
        w->x8B7 = 0;
        if (pGS->x4F88 <= 1) {
            w->x4 = 5;
        }
        if (pG->x4F88 <= 3) {
            w->x4 = 10;
        }
        if (pG->x4F88 > 6) {
            w->x4 = 16;
        }
        if (pG->x4F88 > 9) {
            w->x4 = 18;
        }
        w->x10 = Rnd() & 1;
        if (pGS->x4F88 <= 3) {
            w->x10 = 0;
        }
        w->xC = 1;
        w->x8BB = 4;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 8) {
            em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.19634955f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->seFlags28B & 1) {
            em39LeftArmAtkCk(em, 7);
        }
        if (em->seFlags28B & 2) {
            w->xC = 0;
        }
        if (w->xC) {
            w->flags |= 0x100;
        }
        if (MotionMoveF(em, 0)) {
            int zero = 0;

            w->x8BB = zero;
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, zero, zero);
            } else {
                w->x680 = 30;
                EmRoutineSet(em, 1, 0xE, zero, 1);
            }
        } else if (em->seFlags28B & 4) {
            if (w->x8B6 == 0) {
                GameAddPoint(0xB);
                if (w->x8B6 == 0) {
                    int rtn = em39AtkRtnCk(em);

                    if (rtn) {
                        break;
                    }
                    if ((u8) (Rnd() % 10) > 4 && em->plDist2 < 9000000.0f) {
                        EmRoutineSet(em, 1, 8, rtn, 1);
                        break;
                    }
                }
            }
            EmRoutineSet(em, 1, 0xE, 0, 1);
        }
        break;
    }
    if (em->seFlags28B & 2) {
        w->x8B7 = 1;
    }
    EM39_T_LONG_TAIL(em, w, em39BackjumpAction);
}

// Frame window test on the player's motion frame (sound cues of the damage motions).
#define PL_FRAME_IN(pl, lo, hi) ((pl)->frame > (lo) && (pl)->frame < (hi))

static void plem39_Stamp(cPlayer* pl)
{
    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 0xA);
    pl->subArc = ((cEm*) pl->dmgType)->subArc;
    switch (pl->xFE) {
    case 0:
        if ((s16) pG->pl_life <= 0) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x127), 0, 5, 1, 0);
            EstSet((int) pl, -1, 0, 0, 0x2F, 0x3E, 0, 0, (u32) pl, 0);
            PlSetDamageSe(0xD);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x126), 0, 5, 1, 0);
            EstSet((int) pl, -1, 0, 0, 0x2F, 0x3D, 0, 0, (u32) pl, 0);
            PlSetDamageSe(0);
        }
        PlSetFace(1);
        pl->xFE++;
    case 1:
        if (MotionMoveF(pl, 0) && (s16) pG->pl_life > 0) {
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
            break;
        }
        if (PL_FRAME_IN(pl, 4.7f, 5.3f)) {
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if ((s16) pG->pl_life > 0) {
            if (PL_FRAME_IN(pl, 87.7f, 88.3f) || PL_FRAME_IN(pl, 116.7f, 117.3f)) {
                SndCall(5, 0, &pl->pos, 0, 0, pl);
            }
            if (PL_FRAME_IN(pl, 106.7f, 107.3f) || PL_FRAME_IN(pl, 130.7f, 131.3f)) {
                SndCall(5, 1, &pl->pos, 0, 0, pl);
            }
            if (PL_FRAME_IN(pl, 59.7f, 60.3f)) {
                SndCall(1, 4, &pl->pos, 0, 0, pl);
                SndCall(1, 0x29, &pl->getPartsPtr(0)->worldPos, 0, 0, pPL);
            }
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void em39SitAction(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    SetPlDamage((int) em, plem39Sit);
    w->x8B7 = 1;
    GameAddPoint(9);
}

static void plem39Sit(cPlayer* pl)
{
    pl->subArc = ((cEm*) pl->dmgType)->subArc;
    pl->dmg.set(0, 0x1E);
    switch (pl->xFE) {
    case 0:
        if (pG->x4FB8 == 2) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x12A), 0, 5, 1, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x110), 0, 5, 1, 0);
        }
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

static void em39BackjumpAction(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    f32 ang = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, PI));

    SetPlDamage((int) em, plemBackjump);
    if (ang > 1.5707964f) {
        pPL->xFE = 2;
    }
    w->x8B7 = 1;
    GameAddPoint(9);
}

// The dodge: back jump (cases 0/1) or side roll (cases 2/3), cancelled by any button after 35 frames.
#define PLEM39_BACKJUMP_INIT(pl, motA, motB)                                                        \
    if (pG->x4FB8 == 2) {                                                                          \
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, motA), 0, 3, 1, 5);                  \
    } else {                                                                                       \
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, motB), 0, 3, 1, 5);                  \
    }                                                                                              \
    EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, 0);                                       \
    SndCall(1, 0x43, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);                                     \
    SndCall(1, 0x44, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);                                     \
    GameAddPoint(0xB);                                                                             \
    pl->x3E0 = 35;                                                                                 \
    pl->x3E4 = 0;                                                                                  \
    pl->xFE++;

#define PLEM39_BACKJUMP_KEY(pl)                                                                     \
    if (pl->x3E0) {                                                                                \
        pl->x3E0--;                                                                                \
    } else if (Key.on & 0x1F) {                                                                    \
        pl->x3E4 = 1;                                                                              \
    }

static void plemBackjump(cPlayer* pl)
{
    pl->subArc = ((cEm*) pl->dmgType)->subArc;
    pl->dmType = 0x1E;
    switch (pl->xFE) {
    case 0:
        PLEM39_BACKJUMP_INIT(pl, 0x12B, 0x10D);
    case 1:
        PLEM39_BACKJUMP_KEY(pl);
        if (PL_FRAME_IN(pl, 10.7f, 11.3f)) {
            SndCall(1, 0x4F, &pl->pos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 21.7f, 22.3f)) {
            SndCall(5, 0x14, &pl->pos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 36.7f, 37.3f) || PL_FRAME_IN(pl, 49.7f, 50.3f)) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 37.7f, 38.3f) || PL_FRAME_IN(pl, 50.7f, 51.3f)) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMoveF(pl, 0) || pl->x3E4) {
            EndPlDamage();
        }
        break;
    case 2:
        PLEM39_BACKJUMP_INIT(pl, 0x12C, 0x111);
    case 3:
        PLEM39_BACKJUMP_KEY(pl);
        if (PL_FRAME_IN(pl, 11.7f, 12.3f)) {
            EstSet(0, -1, &pl->pos, 0, 3, 0x13, 0, 0, 0, 0);
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 21.7f, 22.3f)) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 34.7f, 35.3f)) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMoveF(pl, 0) || pl->x3E4) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void em39_R1_br_T_Kick(cEm39* em)
{
    if (em->hp > 0 && (em->seFlags28B & 2) && em39KickHitCk(em)) {
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        SndCall(8, 0x38, &pPL->pos, em->id, 0, pPL);
        if (em39GetCliffPos(em)) {
            LifeDownSet2(pPL, 500, 0, 1);
            em->stat = 0x012E0000;
        } else {
            LifeDownSet(pPL, 500, 0);
            pPL->rot.y = GetXZAngle(&pPL->pos, &em->pos);
            PlSetDamage(8, 0, 0);
        }
    }
}

// Kick hit probe: a point of the leg (parts 0x13) matrix, checked against the player 200 up.
#define EM39_KICK_CK(em, mat, a, b, ay)                                                             \
    (a).x = 0.0f;                                                                                  \
    (a).y = ay;                                                                                    \
    (a).z = 0.0f;                                                                                  \
    PSMTXMultVec(mat, &(a), &(a));                                                                 \
    (a).y += 200.0f;                                                                               \
    (b).y = (a).y;                                                                                 \
    em39AtkCk2(em, 8, &(a), &(b));

static void em39_R1_T_Kick(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec a;
    Vec b;

    w->flags |= 0x80;
    w->flags &= ~0x20;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE1), (int) ARC(0xE2), 3, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x2F, 0, w->espKind, (u32) em, 0);
        w->x8BB = 0;
        w->x8B6 = 0;
        w->x8B7 = 0;
        w->x4 = 10;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 8) {
            em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.19634955f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (w->x4) {
            w->x4--;
            em->rot.y += Muku(&em->pos, &pPLS->pos, em->rot.y, 0.19634955f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (em->seFlags28B & 1) {
            cModel* p;

            b = em->pos;
            p = em->getPartsPtr(0x13);
            EM39_KICK_CK(em, p->mat, a, b, 0.0f);
            EM39_KICK_CK(em, p->mat, a, b, -200.0f);
            EM39_KICK_CK(em, p->mat, a, b, -400.0f);
            EM39_KICK_CK(em, p->mat, a, b, -600.0f);
        }
        if (em->seFlags28B & 0x10) {
            w->x8B6 = 0;
        }
        if (MotionMoveF(em, 0)) {
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                w->x680 = 30;
                EmRoutineSet(em, 1, 0xE, 0, 1);
            }
        } else if (em->seFlags28B & 4) {
            if (w->x8B6 == 0) {
                GameAddPoint(0xB);
            }
            if (em->xFF == 0 && em->plDist2 < 9000000.0f && (u8) (Rnd() % 10) > 4 && w->routeAngAbs < 1.0471976f) {
                int hit = w->x8B6;

                if (hit != 0) {
                    goto rtn_e;
                }
                if (em->plDist2 > 4000000.0f) {
                    if ((u8) (Rnd() % 10) > 4) {
                        EmRoutineSet(em, 1, 0x2B, hit, 1);
                    } else {
                        EmRoutineSet(em, 1, 0x2C, hit, 1);
                    }
                    break;
                }
            }
            if (w->x8B6 == 0) {
                int rtn = em39AtkRtnCk(em);

                if (rtn) {
                    break;
                }
                if ((u8) (Rnd() % 10) > 4 && em->plDist2 < 9000000.0f) {
                    EmRoutineSet(em, 1, 8, rtn, 1);
                    break;
                }
            }
        rtn_e:
            EmRoutineSet(em, 1, 0xE, 0, 1);
        }
        break;
    }
}

static void em39_R1_br_T_LowKick(cEm39* em)
{
    if (em->hp > 0 && (em->seFlags28B & 2) && em39KickHitCk(em)) {
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        LifeDownSet2(pPL, 500, 0, 1);
        SndCall(8, 0x38, &pPL->pos, em->id, 0, pPL);
        em->stat = 0x012D0000;
    }
}

static void em39_R1_T_LowKick(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st = em->xFE;

    w->flags |= 0x80;
    w->flags &= ~0x20;
    switch (st) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE3), (int) ARC(0xE4), 3, 1, 0);
        if (pG->x4FB8 == 2) {
            EstSet((int) em, -1, 0, 0, 0x2F, 0x48, 0, w->espKind, (u32) em, (void*) st);
        } else {
            EstSet((int) em, -1, 0, 0, 0x2F, 0x35, 0, w->espKind, (u32) em, (void*) st);
        }
        w->x8BB = 0;
        w->x8B6 = 0;
        w->x8B7 = 0;
        w->x68C = 0x19;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 8) {
            em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, 0.19634955f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if ((s16) pG->pl_life <= 0) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                w->x680 = 30;
                EmRoutineSet(em, 1, 0xE, 0, 1);
            }
        } else if (em->seFlags28B & 4) {
            int f;

            if (w->x8B6 == 0) {
                GameAddPoint(0xB);
            }
            f = em->xFF;
            if (f == 0 && em->plDist2 < 9000000.0f && (u8) (Rnd() % 10) > 4 && w->routeAngAbs < 1.0471976f &&
                em->plDist2 > 4000000.0f) {
                if ((u8) (Rnd() % 10) > 4) {
                    EmRoutineSet(em, 1, 0x2B, f, 1);
                } else {
                    EmRoutineSet(em, 1, 0x2C, f, 1);
                }
                break;
            }
            if (w->x8B6 == 0) {
                int rtn = em39AtkRtnCk(em);

                if (rtn) {
                    break;
                }
                if ((u8) (Rnd() % 10) > 4 && em->plDist2 < 9000000.0f) {
                    EmRoutineSet(em, 1, 8, rtn, 1);
                    break;
                }
            }
            EmRoutineSet(em, 1, 0xE, 0, 1);
        }
        break;
    }
}

static void em39_R1_T_LowKickHit(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st = em->xFE;
    int end;

    switch (st) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xE9), (int) ARC(0xEA), 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem39_LowKickHit, -129.7f, 0.0f, 1405.22f);
        w->x8B7 = st;
        if ((u8) (Rnd() % 10) > 4) {
            em39SetVoice(em, 0x23);
        } else {
            em39SetVoice(em, 0x24);
        }
        w->x8BB = 0xC;
        w->x4 = 10;
        if (pG->x4F88 <= 1) {
            w->x4 = 5;
        }
        if (pG->x4F88 <= 3) {
            w->x4 = 7;
        }
        if (pG->x4F88 > 6) {
            w->x4 = 12;
        }
        if (pG->x4F88 > 9) {
            w->x4 = 15;
        }
        if ((s16) pG->pl_life <= 1) {
            w->x4 = 18;
        }
        w->x14 = 0;
        w->x10 = Rnd() & 1;
        if (pGS->x4F88 <= 9) {
            w->x10 = 0;
        }
        EM39_K4_EFF_DELETE(em, w);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x36, 0, w->espKind, (u32) em, 0);
        if (pG->x4FB8 == 2) {
            EstSet((int) pPL, -1, 0, 0, 0x2F, 0x47, 0, w->espKind, (u32) em, 0);
        } else {
            EstSet((int) pPL, -1, 0, 0, 0x2F, 0x33, 0, w->espKind, (u32) em, 0);
        }
        em->xFE++;
    case 1:
        em->dmType = 2;
        EmCatchMotionMove(em, 1.0f, 1.0f);
        if (em->seFlags28B & 4) {
            w->x14 = 1;
        }
        if (em->seFlags28B & 1) {
            pG->pl_life = 0;
        }
        if (w->x8B7) {
            em->xFE = 2;
            break;
        }
        if (w->x4) {
            w->x4--;
            break;
        }
        if (w->x14 == 0) {
            if (w->x10) {
                ActBtn.set(0x25, 0xB, (int) em39ActOn, (int) em, 2, 3, 0, 0);
            } else {
                ActBtn.set(0x25, 0xB, (int) em39ActOn, (int) em, 2, 4, 0, 0);
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xEB), (int) ARC(0xEC), 0, 1, 0);
        EmCatchPLSet(em, 0.0f, 2, (int) plem39_LowKickHit, -84.43f, 0.0f, 1068.35f);
        pPL->xFE = st;
        EM39_K4_EFF_DELETE(em, w);
        w->x8BB = 0xE;
        if (pG->x4FB8 == 2) {
            EstSet((int) em, -1, 0, 0, 0x2F, 0x4C, 0, 0, (u32) em, 0);
        } else {
            EstSet((int) em, -1, 0, 0, 0x2F, 0x37, 0, 0, (u32) em, 0);
        }
        EstSet((int) pPL, -1, 0, 0, 0x2F, 0x34, 0, 0, (u32) pPL, 0);
        w->x4 = 10;
        em->xFE++;
    case 3:
        if (w->x4) {
            w->x4--;
            end = EmCatchMotionMove(em, 1.0f, 1.0f);
        } else {
            end = MotionMoveF(em, 0);
        }
        if (end || (em->seFlags28B & 4)) {
            w->x680 = 30;
            EmRoutineSet(em, 1, 0xE, 0, 1);
        }
        break;
    }
    em39HandSet(em, 1);
}

static void plem39_LowKickHit(cPlayer* pl)
{
    cModel* p = pl->getPartsPtr(4);
    int end;

    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 0xA);
    pl->subArc = ((cEm*) pl->dmgType)->subArc;
    switch (pl->xFE) {
    case 0:
        if (pG->x4FB8 == 2) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x12F), 0, 0, 1, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x11F), 0, 0, 1, 0);
        }
        PlSetDamageSe(0);
        PlSetFace(1);
        pl->atari.set(0xA, 480.00003f, 400.0f);
        pl->pWep->setTrans(0, 0);
        pl->xFE++;
    case 1:
        EmCatchMotionMove(pl, 1.0f, 1.0f);
        if (((cEm*) pPL->dmgType)->xFC != 1 && ((cEm*) pPL->dmgType)->xFD != 0x1B) {
            pl->pWep->setTrans(1, 0);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
            break;
        }
        if (PL_FRAME_IN(pl, 18.7f, 19.3f)) {
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 43.7f, 44.3f)) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xD, 1);
        }
        if (PL_FRAME_IN(pl, 78.7f, 79.3f)) {
            if (pG->x4FB8 == 2) {
                SndCall(8, 0x56, &p->worldPos, ((cEm*) pl->dmgType)->id, 0, pPL);
            } else {
                SndCall(8, 0x54, &p->worldPos, ((cEm*) pl->dmgType)->id, 0, pPL);
            }
        }
        if (PL_FRAME_IN(pl, 130.7f, 131.3f)) {
            PlSetDamageSe(0xD);
        }
        if (PL_FRAME_IN(pl, 151.7f, 152.3f)) {
            SndCall(1, 0x4F, &pl->pos, 0, 0, pl);
        }
        pl->xFE = ((cEm*) pl->dmgType)->xFE;
        break;
    case 2:
        if (pG->x4FB8 == 2) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x130), 0, 0, 1, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x120), 0, 0, 1, 0);
        }
        pl->x3E0 = 10;
        SndCall(1, 0x4F, &pl->pos, 0, 0, pl);
        pl->xFE++;
    case 3:
        if (pl->x3E0) {
            pl->x3E0--;
            end = EmCatchMotionMove(pl, 1.0f, 1.0f);
        } else {
            end = MotionMoveF(pl, 0);
        }
        if (end) {
            pl->pWep->setTrans(1, 0);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        } else {
            if (PL_FRAME_IN(pl, 12.7f, 13.3f) || PL_FRAME_IN(pl, 40.7f, 41.3f)) {
                SndCall(1, 0x4F, &pl->pos, 0, 0, pl);
            }
            if (PL_FRAME_IN(pl, 37.7f, 38.3f)) {
                SndCall(1, 0x43, &p->worldPos, 0, 0, pl);
            }
            if (PL_FRAME_IN(pl, 45.7f, 46.3f)) {
                SndCall(5, 0xD, &pl->pos, 0, 0, pl);
            }
            if (PL_FRAME_IN(pl, 47.7f, 48.3f)) {
                SndCall(5, 0xE, &pl->pos, 0, 0, pl);
            }
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

// Cliff attack: the enemy stands on the ledge (jumpPos / jumpAng) and drags the player over it.
// The object pointer is a one-member struct so that every store through the object reloads it.
static struct {
    cObj* p;
} em39CliffObj = { 0 };
// Debug laser-marker line colours (em39MarkerMove).
static u32 em39MarkerCol0 = 0x20400000;
static u32 em39MarkerCol1 = 0x20800000;

#define EM39_CLIFF_POS(em, w, mat, a, az)                                                            \
    PSMTXRotRad(mat, 'y', (w)->jumpAng);                                                            \
    TransMatrix(mat, &(w)->jumpPos);                                                                \
    (a).x = 0.0f;                                                                                  \
    (a).y = 0.0f;                                                                                  \
    (a).z = az;                                                                                    \
    PSMTXMultVec(mat, &(a), &(em)->pos);                                                           \
    (em)->rot.y = (w)->jumpAng;

static void em39_R1_T_CliffAtk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st = em->xFE;
    Mtx mat;
    Vec a;

    em->dmType = 2;
    switch (st) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        EM39_CLIFF_POS(em, w, mat, a, -1145.15f);
        MotionSetCore(em, MOTION(em), ARC(0xED), (int) ARC(0xEE), 0, 1, 0);
        SetPlDamage((int) em, plem39_CliffAtk);
        w->x8BB = st;
        w->x10 = 10;
        if (pGS->x4F88 <= 1) {
            w->x10 = 5;
        }
        if (pG->x4F88 <= 3) {
            w->x10 = 8;
        }
        if (pG->x4F88 > 6) {
            w->x10 = 12;
        }
        if (pG->x4F88 > 9) {
            w->x10 = 15;
        }
        if ((s16) pG->pl_life <= 1) {
            w->x10 = 20;
        }
        EM39_K4_EFF_DELETE(em, w);
        w->x4 = st;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (w->x4) {
            w->x4--;
        } else {
            w->x4 = 4;
            EstSet(0, -1, 0, 0, 0x2F, 0x38, 0, 0, 0, 0);
        }
        if (em->seFlags28B & 1) {
            pG->pl_life = 0;
        }
        if (em->seFlags28B & 4) {
            ActBtn.set(0x19, 5, 0, 0, 2, 2, 0, 0);
            if (Key.trg & 0x80000) {
                if (w->x10 == 0) {
                    em->xFE++;
                    break;
                }
                w->x10--;
            }
        }
        if (em->seFlags28B & 0x10) {
            em39SetVoice(em, 0x52);
        }
        if (em->frame > 48.7f && em->frame < 49.3f) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        }
        if (em->frame > 139.7f && em->frame < 140.3f) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xB, 1);
        }
        break;
    case 2:
        EM39_CLIFF_POS(em, w, mat, a, -382.78f);
        MotionSetCore(em, MOTION(em), ARC(0xEF), (int) ARC(0xF0), 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x39, 0, 0, (u32) em, 0);
        SetPlDamage((int) em, plem39_CliffAtk);
        pPL->xFE = st;
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0) || (em->seFlags28B & 1)) {
            w->flags |= 0x800000;
            AtariOn(&em->atari, 0x300);
            w->x680 = 30;
            EmRoutineSet(em, 1, 0xE, 0, 1);
        } else if (em->seFlags28B & 4) {
            AtariOn(&em->atari, 0x300);
        }
        break;
    }
    em39HandSet(em, 1);
}

static void plem39_CliffAtk(cPlayer* pl)
{
    cModel* p = pl->getPartsPtr(4);

    pG->flags_5010 |= 0x8000;
    pl->dmg.set(0, 0xA);
    pl->subArc = ((cEm*) pl->dmgType)->subArc;
    switch (pl->xFE) {
    case 0:
        AtariOff(&pl->atari, 0xFCFF);
        pl->pos.x = -15.66f;
        pl->pos.y = 0.0f;
        pl->pos.z = 934.56f;
        PSMTXMultVec(((cEm*) pl->dmgType)->mat, &pl->pos, &pl->pos);
        pl->rot.y = ((cEm*) pl->dmgType)->rot.y + PI;
        pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x121), 0, 0, 1, 0);
        PlSetFace(1);
        pl->pWep->setTrans(0, 0);
        pl->xFE++;
    case 1:
        if (PL_FRAME_IN(pl, 7.7f, 8.3f)) {
            SndCall(1, 0x34, &p->worldPos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 14.7f, 15.3f)) {
            SndCall(1, 0x34, &p->worldPos, 0, 0, pl);
            SndCall(1, 7, &p->worldPos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 142.7f, 143.3f)) {
            SndCall(1, 0x4A, &p->worldPos, 0, 0, pl);
        }
        MotionMoveF(pl, 0);
        break;
    case 2:
        pl->pos.x = 54.86f;
        pl->pos.y = 0.0f;
        pl->pos.z = 704.51f;
        PSMTXMultVec(((cEm*) pl->dmgType)->mat, &pl->pos, &pl->pos);
        pl->rot.y = ((cEm*) pl->dmgType)->rot.y + PI;
        pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x122), 0, 0, 1, 0);
        em39CliffObj.p = ObjMgr.create(0xB);
        if (em39CliffObj.p) {
            em39CliffObj.p->modelInit(PL_ARC_PTR(pl->subArc, 0x129), PL_ARC_PTR(pl->subArc, 0x128));
            AtariOffR(em39CliffObj.p->atari.flags, 0xFCFF);
            em39CliffObj.p->pParts->pParent = pPL->getPartsPtr(0xA);
            em39CliffObj.p->lightInfo.init2(1, 1, &((Vec) { 0.0f, 0.0f, 0.0f }), &((Vec) { 500.0f, 0.0f, 0.0f }), 1);
            em39CliffObj.p->wep.parent = pPL;
            em39CliffObj.p->setNoSuspend(1);
        }
        pl->pWep->setTrans(0, 0);
        pl->xFE++;
    case 3:
        if (PL_FRAME_IN(pl, 18.7f, 19.3f)) {
            SndCall(1, 0x10, &p->worldPos, 0, 0, pl);
        }
        if (MotionMoveF(pl, 0)) {
            pl->xFE++;
        }
        break;
    case 4:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x123), 0, 0, 1, 0);
        pl->pWep->setTrans(1, 0);
        if (em39CliffObj.p) {
            ObjMgr.destroy(em39CliffObj.p);
            em39CliffObj.p = 0;
        }
        SndCall(1, 7, &p->worldPos, 0, 0, pl);
        pl->xFE++;
    case 5:
        if (PL_FRAME_IN(pl, 8.7f, 9.3f)) {
            SndCall(1, 0x4F, &p->worldPos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 24.7f, 25.3f)) {
            SndCall(5, 0xD, &pl->pos, 0, 0, pl);
        }
        if (PL_FRAME_IN(pl, 30.7f, 31.3f)) {
            SndCall(5, 0xE, &pl->pos, 0, 0, pl);
        }
        if (MotionMoveF(pl, 0)) {
            AtariOn(&pl->atari, 0x300);
            EndPlDamage();
            pl->dmg.set(0, 0x1E);
        }
        break;
    }
    pl->x3A8 = pl->pos;
    pl->subArc = pl->subArc2;
}

static void em39_R0_Damage(cEm39* em)
{
    EM39_WK(em)->flags |= 8;
    Em39_R1_dmg_tbl[em->xFD](em);
}

// Damage entry: drop the hanging object and the grenade in hand, delete the effects, voice + speech.
#define EM39_DM_DROP(em, w, voice, speech)                                                          \
    if ((w)->pObj12 && (w)->x678 == 0) {                                                           \
        Mtx m;                                                                                     \
        Vec v;                                                                                     \
                                                                                                   \
        PSMTXRotRad(m, 'y', GetXZAngle(&pPL->pos, &(em)->pos));                                    \
        v.x = 0.0f;                                                                                \
        v.y = 40.0f;                                                                               \
        v.z = -50.0f;                                                                              \
        PSMTXMultVecSR(m, &v, &v);                                                                 \
        ((cObj12*) (w)->pObj12)->setFall(&v, 2);                                                   \
        (w)->pObj12 = 0;                                                                           \
    }                                                                                              \
    if ((w)->pGrenade) {                                                                           \
        Vec spd;                                                                                   \
                                                                                                   \
        spd.x = 0.0f;                                                                              \
        spd.y = 10.0f;                                                                             \
        spd.z = 50.0f;                                                                             \
        PSMTXMultVecSR((em)->mat, &spd, &spd);                                                     \
        EM39_GRENADE_THROW(em, w, spd);                                                            \
    }                                                                                              \
    EffectEspDelete(0, (w)->espKind2, (u32) (em), 0);                                              \
    EffectEspgenDelete(0, (w)->espKind2, (int) (em));                                              \
    EffectEfmDelete(0, (w)->espKind2, (int) (em));                                                 \
    EM39_K4_EFF_DELETE(em, w);                                                                     \
    em39SetVoice(em, voice);                                                                       \
    switch ((u8) (Rnd() % 3)) {                                                                    \
    case 0:                                                                                        \
    default:                                                                                       \
        em39SetSpeech(em, speech, 0x2E);                                                           \
        break;                                                                                     \
    case 1:                                                                                        \
        em39SetSpeech(em, speech, 0x2F);                                                           \
        break;                                                                                     \
    case 2:                                                                                        \
        em39SetSpeech(em, speech, 0x30);                                                           \
        break;                                                                                     \
    }                                                                                              \
    (w)->x8A4 = 0;                                                                                 \
    AtariOn(&(em)->atari, 0x300);

// Damage recovery decision on the return-to-idle frame (seFlags28B bit 2).
#define EM39_DM_RECOVER(em, w)                                                                      \
    {                                                                                              \
        int lim;                                                                                   \
                                                                                                   \
        if ((w)->pGotoPoint && (w)->pGotoPoint->sub == 0) {                                        \
            EmRoutineSet(em, 1, 6, 0, 0);                                                          \
            break;                                                                                 \
        }                                                                                          \
        if (em39GotoCk(em)) {                                                                      \
            break;                                                                                 \
        }                                                                                          \
        switch ((w)->x8C4) {                                                                       \
        case 0:                                                                                    \
        case 3:                                                                                    \
        default:                                                                                   \
            lim = 1000;                                                                            \
            break;                                                                                 \
        case 1:                                                                                    \
            lim = 500;                                                                             \
            break;                                                                                 \
        case 2:                                                                                    \
            lim = 500;                                                                             \
            break;                                                                                 \
        case 4:                                                                                    \
            lim = 500;                                                                             \
            break;                                                                                 \
        case 5:                                                                                    \
            lim = 0;                                                                               \
            break;                                                                                 \
        }                                                                                          \
        if ((w)->x69C >= lim) {                                                                    \
            EmRoutineSet(em, 1, 0x25, 0, 0);                                                       \
            break;                                                                                 \
        }                                                                                          \
        if (em39LockCk(em) || (em)->plDist2 < 4000000.0f) {                                       \
            if ((u8) (Rnd() % 10) > 6 && em39JumpUpCk3(em)) {                                      \
                break;                                                                             \
            }                                                                                      \
            if ((em)->plDist2 < 25000000.0f) {                                                     \
                EmRoutineSet(em, 1, 0xF, 0, 0);                                                    \
            } else {                                                                               \
                EmRoutineSet(em, 1, 0xD, 0, 0);                                                    \
            }                                                                                      \
            break;                                                                                 \
        }                                                                                          \
    }

static void em39_R1_Dm_Normal(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->flags |= 0x10;
    w->x698 = 0;
    switch (em->xFE) {
    case 0:
        if (fabsf(Muku(&em->pos, &em->x328, em->rot.y, PI)) < 1.5707964f) {
            if ((u8) (Rnd() % 10) > 5) {
                MotionSetCore(em, MOTION(em), ARC(0x56), (int) ARC(0x57), 3, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x58), (int) ARC(0x59), 3, 1, 0);
            }
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x5C), (int) ARC(0x5D), 3, 1, 0);
        }
        if (w->x678) {
            w->x678--;
        }
        EM39_DM_DROP(em, w, 6, 0x1E);
        w->x4 = 10;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 8, 0, 0xA);
        }
        if (em->seFlags28B & 4) {
            EM39_DM_RECOVER(em, w);
        } else {
            w->flags |= 0x100;
        }
        if (em->seFlags28B & 1) {
            if ((u8) (Rnd() % 10) > 4) {
                int rtn;

                if (w->pGotoPoint && w->pGotoPoint->sub == 0) {
                    EmRoutineSet(em, 1, 6, 0, 0);
                    break;
                }
                rtn = em39GotoCk(em);
                if (rtn) {
                    break;
                }
                if (w->targetAngAbs > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, rtn, rtn);
                } else if (em->plDist2 < 9000000.0f) {
                    EmRoutineSet(em, 1, 0xE, rtn, 1);
                } else {
                    EmRoutineSet(em, 1, 8, rtn, rtn);
                }
            }
        }
        break;
    }
}

static void em39_R1_Dm_Head(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    w->flags |= 0x10;
    w->x698 = 0;
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x60), (int) ARC(0x61), 3, 1, 0);
        if (w->x678) {
            w->x678--;
        }
        EM39_DM_DROP(em, w, 9, 0x3C);
        w->x4 = 10;
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 8, 0, 0xA);
        }
        if (em->seFlags28B & 4) {
            EM39_DM_RECOVER(em, w);
        } else {
            w->flags |= 0x100;
        }
        if (em->seFlags28B & 1) {
            if ((u8) (Rnd() % 10) > 4) {
                int rtn;

                if (w->pGotoPoint && w->pGotoPoint->sub == 0) {
                    EmRoutineSet(em, 1, 6, 0, 0);
                    break;
                }
                rtn = em39GotoCk(em);
                if (rtn) {
                    break;
                }
                if (w->targetAngAbs > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, rtn, rtn);
                } else if (em->plDist2 < 9000000.0f) {
                    EmRoutineSet(em, 1, 0xE, rtn, 1);
                } else {
                    EmRoutineSet(em, 1, 8, rtn, rtn);
                }
            }
        }
        break;
    }
}

static void em39_R1_Dm_Blow(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int rtn;

    w->flags |= 0x10;
    w->x698 = 0;
    switch (em->xFE) {
    case 0:
        if (fabsf(Muku(&em->pos, &em->x328, em->rot.y, PI)) < 1.5707964f) {
            MotionSetCore(em, MOTION(em), ARC(0x5A), (int) ARC(0x5B), 3, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2F, 0x15, 0, 0, (u32) em, 0);
            w->flags |= 0x200;
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x5E), (int) ARC(0x5F), 3, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2F, 0x16, 0, 0, (u32) em, 0);
            w->flags &= ~0x200;
        }
        if (w->x678) {
            w->x678 = 0;
        }
        EM39_DM_DROP(em, w, 9, 0x5A);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0) || (em->hp > 0 && (em->seFlags28B & 4))) {
            em->xFE++;
        }
        break;
    case 2:
        if (w->flags & 0x200) {
            MotionSetCore(em, MOTION(em), ARC(0x77), (int) ARC(0x78), 3, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x2F, 0x2B, 0, 0, (u32) em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x79), (int) ARC(0x7A), 3, 1, 0);
        }
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if (w->pGotoPoint && w->pGotoPoint->sub == 0) {
                EmRoutineSet(em, 1, 6, 0, 0);
                break;
            }
            rtn = em39GotoCk(em);
            if (rtn) {
                break;
            }
            EmRoutineSet(em, 1, 8, rtn, 0xA);
        }
        if (em->seFlags28B & 4) {
            EM39_DM_RECOVER(em, w);
        } else {
            w->flags |= 0x100;
        }
        if (em->seFlags28B & 1) {
            if ((u8) (Rnd() % 10) > 4) {
                if (w->targetAngAbs > 2.3561945f) {
                    EmRoutineSet(em, 1, 0xB, 0, 0);
                } else if (em->plDist2 < 9000000.0f) {
                    EmRoutineSet(em, 1, 0xE, 0, 1);
                } else if (w->x6A0 == 0) {
                    EmRoutineSet(em, 1, 9, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 8, 0, 0);
                }
            }
        }
        break;
    }
}

// Tower-form damage: the recovery choice by difficulty / lock-on, or the next attack.
#define EM39_DM_T_RECOVER(em, w, end)                                                               \
    if (end) {                                                                                     \
        int rtn = em39AtkRtnCk(em);                                                                \
                                                                                                   \
        if (rtn) {                                                                                 \
            break;                                                                                 \
        }                                                                                          \
        if ((w)->x6A0 == 0) {                                                                      \
            EmRoutineSet(em, 1, 9, rtn, rtn);                                                      \
        } else {                                                                                   \
            EmRoutineSet(em, 1, 8, rtn, rtn);                                                      \
        }                                                                                          \
    } else if ((em)->seFlags28B & 4) {                                                             \
        if (pG->x4F88 <= 3) {                                                                      \
            EmRoutineSet(em, 1, 0xE, end, 1);                                                      \
        } else if (em39LockCk(em)) {                                                               \
            if (pG->x4F88 <= 9) {                                                                  \
                EmRoutineSet(em, 1, 0xE, end, 1);                                                  \
            } else {                                                                               \
                EmRoutineSet(em, 1, 0xF, end, end);                                                \
            }                                                                                      \
        } else {                                                                                   \
            int rtn = em39AtkRtnCk(em);                                                            \
                                                                                                   \
            if (rtn) {                                                                             \
                break;                                                                             \
            }                                                                                      \
            if ((w)->x6A0 == 0) {                                                                  \
                EmRoutineSet(em, 1, 9, rtn, rtn);                                                  \
            } else {                                                                               \
                EmRoutineSet(em, 1, 8, rtn, rtn);                                                  \
            }                                                                                      \
        }                                                                                          \
    } else if ((em)->seFlags28B & 1) {                                                             \
        (w)->x8BB = 8;                                                                             \
    }

static void em39_R1_Dm_T_Head(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st;
    int end;

    w->flags |= 0x110;
    w->x698 = 0;
    st = em->xFE;
    switch (st) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x60), (int) ARC(0x61), 3, 1, 0);
        EM39_K4_EFF_DELETE(em, w);
        em39SetVoice(em, 9);
        w->x8A4 = st;
        w->x4 = 10;
        em->xFE++;
    case 1:
        end = MotionMoveF(em, 0);
        EM39_DM_T_RECOVER(em, w, end);
        break;
    }
}

static void em39_R1_Dm_T_Down(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st;
    int end;

    w->flags |= 0x10;
    w->x698 = 0;
    st = em->xFE;
    switch (st) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF7), (int) ARC(0xF8), 3, 1, 0);
        w->x8BB = st;
        EM39_K4_EFF_DELETE(em, w);
        em39SetVoice(em, 6);
        w->x8A4 = st;
        w->x4 = 10;
        em->xFE++;
    case 1:
        end = MotionMoveF(em, 0);
        if (end == 0) {
            w->flags |= 0x1000;
            if (!(em->seFlags28B & 0x10)) {
                w->flags |= 0x10000;
            }
        }
        EM39_DM_T_RECOVER(em, w, end);
        break;
    }
}

static void em39_R1_Dm_T_DownHead(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st;
    int end;

    w->flags |= 0x110;
    w->x698 = 0;
    st = em->xFE;
    switch (st) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF9), (int) ARC(0xFA), 3, 1, 0);
        EM39_K4_EFF_DELETE(em, w);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x42, 0, 0, (u32) em, (void*) st);
        em39SetVoice(em, 9);
        w->x4 = 10;
        w->x8BB = st;
        w->x8A4 = st;
        em->xFE++;
    case 1:
        end = MotionMoveF(em, 0);
        if (end) {
            if (w->x6A0 == 0) {
                EmRoutineSet(em, 1, 9, 0, 0);
            } else {
                EmRoutineSet(em, 1, 8, 0, 0);
            }
        } else if (em->seFlags28B & 4) {
            if (pG->x4F88 <= 3) {
                EmRoutineSet(em, 1, 0xE, end, 1);
            } else if (em39LockCk(em)) {
                if (pG->x4F88 <= 9) {
                    EmRoutineSet(em, 1, 0xE, end, 1);
                } else {
                    EmRoutineSet(em, 1, 0xF, end, end);
                }
            } else {
                int rtn = em39AtkRtnCk(em);

                if (rtn) {
                    break;
                }
                if (w->x6A0 == 0) {
                    EmRoutineSet(em, 1, 9, rtn, rtn);
                } else {
                    EmRoutineSet(em, 1, 8, rtn, rtn);
                }
            }
        } else if (em->seFlags28B & 1) {
            w->x8BB = 8;
        }
        break;
    }
}

static void em39_R0_Die(cEm39* em)
{
    EM39_WK(em)->flags |= 8;
    Em39_R1_die_tbl[em->xFD](em);
}

static void em39_R1_Die_Normal(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st = em->xFE;

    switch (st) {
    case 0:
        AtariOff(&em->atari, 0xFCFF);
        em->pos.x = 6640.0f;
        em->pos.y = 12050.0f;
        em->pos.z = -14796.0f;
        em->rot.y = 1.57f;
        MotionSetCore(em, MOTION(em), ARC(0xE7), 0, 0, 0x201, 0);
        EM39_K4_EFF_DELETE(em, w);
        EstSet((int) em, -1, 0, 0, 0x2F, 0x3F, 0, w->espKind, (u32) em, (void*) st);
        w->x8BB = 0x10;
        em39DieModelSet(em);
        w->sndId = SndStrReq(1, 0xE7, 0x80000003, 0, 0, 0.0f);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2: {
        MotionData* mot = (MotionData*) ARC(0xE7);

        MotionSetCore(em, MOTION(em), mot, 0, 0, 0x100, (u16) ((mot->maxFrame & 0x3FFF) - 1));
        em->clearStatus(5);
        em->setStatus(8);
        EmSetDropItem(em);
        EM39_K4_EFF_DELETE(em, w);
        EstSet(0, -1, 0, 0, 0x2F, 0x46, 0, 0, 0, 0);
        w->x8BB = 0x12;
        SndStrReq(w->sndId, 8, 0, 0);
        em->xFE++;
    }
    case 3:
        MotionMoveF(em, 0);
        break;
    }
}

static void em39_R1_Die_Flash(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int st = em->xFE;
    Vec pos;
    Vec rot;
    Vec spd;
    int end;

    w->flags |= 0x130;
    switch (st) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x9B), (int) ARC(0x9C), 6, 1, 0);
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pFlash = SetWeapon(PL_ARC(0x6A), PL_ARC(0x6F), &pos, &rot, 0);
        if (w->pFlash) {
            w->pFlash->setParent(em, 0xA, 0);
        }
        em39WepSet(em, 2);
        w->x4 = st;
        em->xFE++;
    case 1:
        end = MotionMoveF(em, 0);
        if (end == 0) {
            if ((em->seFlags28B & 1) && w->pFlash) {
                spd.x = 0.0f;
                spd.y = 100.0f;
                spd.z = 150.0f;
                PSMTXMultVecSR(em->mat, &spd, &spd);
                w->x4 = 28;
                w->pFlash->setFlashThrow(&spd, 28);
                w->pFlash = (cEmWep*) end;
            }
            if (w->x4) {
                w->x4--;
                if (w->x4) {
                    break;
                }
                em->clearStatus(5);
                em->setStatus(8);
                EmSetDropItem(em);
                AtariOff(&em->atari, 0xFCFF);
                em->be_flag |= 0x10000000;
                em->setStatus(1);
                em->be_flag &= ~2;
                em->xFE++;
                break;
            } else {
                break;
            }
        }
        AtariOff(&em->atari, 0xFCFF);
        em->be_flag |= 0x10000000;
        em->setStatus(1);
        em->be_flag &= ~2;
        em->xFE++;
        break;
    }
    em39HandSet(em, 0);
}

// ---- HELPERS ----
void em39RouteCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec plPos;
    Vec a;
    Vec b;
    int up;

    if (em->hp <= 0) {
        return;
    }
    if (em->type == 2) {
        plPos = pPL->pos;
    } else {
        if (SQRTF(em->plDist2) < 5000.0f) {
            f32 dy = fabsf(em->pos.y - pPL->pos.y);

            if (dy < 0.0f) {
                up = 0;
            }
            a = pPL->pos;
            plPos = a;
        } else {
            f32 dy = fabsf(em->pos.y - pPL->pos.y);

            if (dy < 0.0f) {
                up = 0;
            }
            a = pPL->pos;
            plPos = a;
        }
    }
    // COMPILER-DIFF: candidate #17 (global-alloc priority order) -- the target allocates the four
    // callee-saved pseudos as &em->pos r27 > &a r26 > pPL high r25 > PI high r24; ours ranks &a (4 refs/74)
    // above &em->pos (4/106) and the PI high (3/178) above the pPL high (5/1096). Codeless asms whose
    // "m" operands go through the PRE'd pseudos add refs without code: three `pPL` mentions in three
    // blocks (8 refs: 3*8/1192 > 3/182) and two `&em->pos` member reads after the second Muku (6 refs:
    // 2*6/108 > 2*4/76). A second mention in the same block would be a fresh `lis` (cse keeps a `high`).
    up = 0;
    if (pPL->pos.y > em->pos.y + 1000.0f) {
        up = 1;
        asm("" : "=m"(w->x8B6) : "m"(pPL)); // COMPILER-DIFF: candidate #17
    }
    RouteCkToPos(em, &plPos, &w->routePos, up, 0);
    w->routeAng = Muku(&em->pos, &w->routePos, em->rot.y, PI);
    w->routeAngAbs = fabsf(w->routeAng);
    if (em->xFC == 0) {
        w->routeAng = 0.0f;
        w->routeAngAbs = 0.0f;
        em->plDist2 = 100000000.0f;
        asm("" : "=m"(w->x8B6) : "m"(pPL)); // COMPILER-DIFF: candidate #17
    }
    a.x = em->pos.x;
    a.y = em->pos.y + 1300.0f;
    a.z = em->pos.z;
    b.x = pPL->pos.x;
    b.y = pPL->pos.y + 1300.0f;
    b.z = pPL->pos.z;
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x4000) == 0) {
        w->flags |= 1;
    }
    w->targetPos = w->routePos;
    w->targetAng = w->routeAng;
    w->targetAngAbs = w->routeAngAbs;
    w->targetDist = em->plDist2;
    w->pTarget = pPLS;
    w->flags &= ~4;
    if (w->gotoOn) {
        up = 0;
        if (w->gotoPos.y > em->pos.y + 1000.0f) {
            up = 1;
            asm("" : "=m"(w->x8B6) : "m"(pPL)); // COMPILER-DIFF: candidate #17
        }
        RouteCkToPos(em, &w->gotoPos, &w->targetPos, up, 0);
        w->targetAng = Muku(&em->pos, &w->targetPos, em->rot.y, PI);
        w->targetAngAbs = fabsf(w->targetAng);
        w->targetDist = (em->pos.x - w->gotoPos.x) * (em->pos.x - w->gotoPos.x) + (em->pos.z - w->gotoPos.z) * (em->pos.z - w->gotoPos.z);
        {
            Vec* pep = &em->pos; // COMPILER-DIFF: candidate #17 -- two refs for the PRE'd &em->pos
            asm("" : "=m"(w->x8B7) : "m"(pep->y), "m"(pep->z));
        }
    }
    if (pG->flags_60 & 0x4000) {
        Vec c = em->pos;

        c.y += 250.0f;
        Draw_line3d(&c, &w->targetPos, 0xFFFFFF40, 0);
    }
}

extern "C" void Draw_line3d_222(Vec* p0, Vec* p1, u32 color, int blend);
cObj* SetObj10(void* bin, void* tpl, Vec* pos, Vec* rot, Vec* spd, f32 grav, f32 rad, int life, int flags);
void Obj10SetEst(cObj* obj, int no0, int prm0, u32 type, int no1, int prm1, int no2, int prm2, int no3, int prm3);

// Neck: turn the head (parts 3 addRot) towards the player while flags bit 4 is set, else relax.
void em39NeckMove(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    cModel* p = em->getPartsPtr(4);
    cModel* plp = pPL->getPartsPtr(4);
    Vec a;
    Vec d;

    a.x = 0.0f;
    a.y = 50.0f;
    a.z = 0.0f;
    PSMTXMultVec(plp->mat, &a, &a);
    if (w->flags & 0x10) {
        f32 ang = Muku(&em->pos, &a, em->rot.y, 1.0471976f);
        f32 len;
        f32 pitch;

        w->x59C = w->x59C * 0.9f + ang * 0.1f;
        PSVECSubtract(&a, &p->worldPos, &d);
        len = SQRTF(d.x * d.x + d.z * d.z);
        pitch = -atan2f(d.y, len);
        if (pitch > 0.7853982f) {
            pitch = 0.7853982f;
        }
        if (pitch < -0.7853982f) {
            pitch = -0.7853982f;
        }
        w->x598 = w->x598 * 0.9f + pitch * 0.1f;
    } else {
        w->x598 *= 0.9f;
        w->x59C *= 0.9f;
    }
    p = em->getPartsPtr(3);
    ((cParts*) p)->motParts.flags |= 0x40000000;
    ((cParts*) p)->addRot.x = w->x598;
    ((cParts*) p)->addRot.y = w->x59C;
    ((cParts*) p)->addRot.z = 0.0f;
}

void em39WaistMove(cEm39* em)
{
    Vec a;
}

// The original keeps the three constant-pool words (0.9, 0.020000001, 1.0) of em39WaistMove's
// dead-stripped body; ours drops unreferenced pool entries (mark_constant_pool).
// COMPILER-DIFF: candidate #10 (unreferenced constant-pool entries kept).
asm(".section .rodata\n\t.long 0x3f666666, 0x3ca3d70b, 0x3f800000\n\t.text");

// Laser marker: from the machine gun muzzle (x8B4 == 3) or the bow (x8B4 == 4) to the target.
void em39MarkerMove(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    cModel* p;
    Vec from;
    Vec to;

    if (!(w->flags & 0x40)) {
        return;
    }
    switch (w->x8B4) {
    case 3:
        p = em->getPartsPtr(0xA);
        from.x = 228.0f;
        from.y = -24.0f;
        from.z = 29.0f;
        to.x = -50000.0f;
        to.y = 0.0f;
        to.z = 0.0f;
        break;
    case 4:
        if (w->pWep3 == 0) {
            return;
        }
        p = w->pWep3->getPartsPtr(0);
        from.x = 0.0f;
        from.y = 0.0f;
        from.z = 0.0f;
        to.x = 50000.0f;
        to.y = 0.0f;
        to.z = 0.0f;
        break;
    case 0:
    case 1:
    case 2:
    default:
        return;
    }
    PSMTXMultVec(p->mat, &from, &from);
    PSMTXMultVecSR(p->mat, &to, &to);
    PSVECAdd(&from, &to, &to);
    GetWepTargetPos(&from, &to, 1, 0, 0, 0);
    EstSet(0, -1, &to, 0, 0, 0x50, 0, 0, 0, 0);
    if (pG->x4 == 1) {
        Draw_line3d_222(&to, &from, em39MarkerCol0, 1);
    } else {
        Draw_line3d_222(&to, &from, em39MarkerCol1, 1);
    }
}

// Machine gun shot: a line from the muzzle with a random spread; hits the player or leaves a spark.
int em39GunHitCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec from;
    Vec to;
    EmAtkInfo atk;
    Vec hit;
    Vec nrm;
    Vec a;
    Vec rot;
    Vec b;
    cEm* target;
    cModel* p;
    s16 hp;
    f32 len;

    if (w->pWep2) {
        EstSet((int) w->pWep2, -1, 0, 0, 0x2F, 3, 0, 0, (u32) w->pWep2, 0);
    }
    from.x = 228.0f;
    from.y = -24.0f;
    from.z = 29.0f;
    to.x = -50000.0f;
    to.y = fRand1_1() * 1000.0f;
    to.z = fRand1_1() * 1000.0f;
    atk = em39_gun_atk_info;
    p = em->getPartsPtr(0xA);
    PSMTXMultVec(p->mat, &from, &from);
    PSMTXMultVecSR(p->mat, &to, &to);
    PSVECAdd(&from, &to, &to);
    if (pG->debug_mode == 7) {
        Draw_line3d(&from, &to, 0xFFFFFFFF, 0);
    }
    hp = em->hp;
    em->hp = 0;
    PlWepHitCheck2(0, &from, &to, 0x1B, 3, 6000.0f);
    em->hp = hp;
    SndCall(8, 0xC, &em->pos, em->id, 0, em);
    target = EmAtkLineHitCk(&from, &to, &hit, &nrm, 0);
    if (target) {
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        SndCall(6, 0x15, &pPL->pos, 0, 0, pPL);
        QuakeExec(0, 0, 5, 22.0f, 2);
        EmPlBloodSet2(em, &from, 1, 0x2F, 0x2C);
        EmAtkSetDamagePL(target, &atk, &from, &to);
        w->x8B6 = 1;
        return 1;
    }
    len = SQRTF(nrm.x * nrm.x + nrm.z * nrm.z);
    rot.x = -atan2f(nrm.y, len);
    rot.y = atan2f(nrm.x, nrm.z);
    rot.z = 0.0f;
    PSVECScale(&nrm, &a, 30.0f);
    PSVECAdd(&hit, &a, &hit);
    EstSet(0, -1, &hit, &rot, 0x2F, 4, 0, 0, (u32) target, (void*) target);
    PSVECSubtract(&hit, &from, &b);
    EspSetGatling(from, b);
    SndCall(6, 0xA, &hit, 0, 0, 0);
    return 0;
}

// The action-button callback and the player damage routine are static: they are emitted here, where
// the original had them (after em39GunHitCk).
static void em39ActOn(cEm39* em)
{
    EM39_WK(em)->x8B7 = 1;
    GameAddPoint(9);
}

static void plemDmSide(cPlayer* pl)
{
    pl->subArc = ((cEm*) pl->dmgType)->subArc;
    switch (pl->xFE) {
    case 0: {
        int flag = 1;

        if (pl->xFF) {
            flag = 0x41;
        }
        if (pG->x4FB8 == 2) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x12D), (int) PL_ARC_PTR(pl->subArc, 0x12E), 3, flag, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pl->subArc, 0x10E), (int) PL_ARC_PTR(pl->subArc, 0x10F), 3, flag, 0);
        }
        PlSetFace(1);
        pl->st.x325 = 0xA;
        PlSetDamageSe(0);
        pl->xFE++;
    }
    case 1:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

// Ejected cartridge (obj10) from the machine gun.
void em39SetCartridge(cEm39* em)
{
    cModel* p = em->getPartsPtr(0xA);
    cObj* obj;
    Vec pos;
    Vec rot;
    Vec spd;

    pos.x = -90.74f;
    pos.y = -17.97f;
    pos.z = 100.98f;
    PSMTXMultVec(p->mat, &pos, &pos);
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    spd.x = 3.0f;
    spd.y = 60.0f;
    spd.z = 60.0f;
    spd.x += fRand1_1() * 15.0f;
    spd.y += fRand1_1() * 15.0f;
    spd.z += fRand1_1() * 15.0f;
    PSMTXMultVecSR(p->mat, &spd, &spd);
    obj = SetObj10(ARC(0x2D), ARC(0x2E), &pos, &rot, &spd, 10.0f, 50.0f, 30, 3);
    if (obj) {
        Obj10SetEst(obj, 0, 0, 0, 0, 0, 0, 0x13, 0, 0);
        obj->type = 0x63;
    }
}

// The player aims at the enemy: laser on it, or the body origin inside the gun's aiming box.
int em39LockCk(cEm39* em)
{
    Mtx inv;
    Vec p;

    if (pG->x4F88 <= 1) {
        return 0;
    }
    if (pPL->xFC != 0) {
        return 0;
    }
    if (pPL->xFD != 6) {
        return 0;
    }
    if (pG->wep_no == 0x10) {
        return 0;
    }
    if (ItemMgr.bulletNumCurrent() == 0) {
        return 0;
    }
    if (pPL->pWep->pObj->wep.target && pPL->pWep->pObj->wep.target == em) {
        return 1;
    }
    if (em->plDist2 > 144000000.0f) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) > 0.7853982f) {
        return 0;
    }
    PSMTXInverse(pPL->getPartsPtr(0xA)->mat, inv);
    PSMTXMultVec(inv, &em->getPartsPtr(0)->worldPos, &p);
    if (p.x > 0.0f) {
        return 0;
    }
    if (p.z > 800.0f) {
        return 0;
    }
    if (p.z < -800.0f) {
        return 0;
    }
    if (p.y > 800.0f) {
        return 0;
    }
    if (p.y < -800.0f) {
        return 0;
    }
    return 1;
}

int em39HeadLockCk(cEm39* em)
{
    Mtx inv;
    Vec p;
    cModel* head;

    if (pG->x4F88 <= 1) {
        return 0;
    }
    if (pG->wep_no == 0x10) {
        return 0;
    }
    if (ItemMgr.bulletNumCurrent() == 0) {
        return 0;
    }
    PSMTXInverse(pPL->getPartsPtr(0xA)->mat, inv);
    head = em->getPartsPtr(4);
    p.x = 0.0f;
    p.y = 150.0f;
    p.z = 0.0f;
    PSMTXMultVec(head->mat, &p, &p);
    PSMTXMultVec(inv, &p, &p);
    if (p.x > 0.0f) {
        return 0;
    }
    if (p.z > 150.0f) {
        return 0;
    }
    if (p.z < -150.0f) {
        return 0;
    }
    if (p.y > 150.0f) {
        return 0;
    }
    if (p.y < -150.0f) {
        return 0;
    }
    return 1;
}

int em39AtkCk(cEm39* em, int no, int parts)
{
    cModel* p = em->getPartsPtr(parts);

    return em39AtkCk2(em, no, &p->worldPos, &p->oldWorldPos);
}

// Side damage on the player: turn him towards the enemy and pick the left / right motion.
#define EM39_ATK_SIDE(em, ang)                                                                      \
    SetPlDamage((int) (em), plemDmSide);                                                           \
    ang = Muku(&pPL->pos, &(em)->pos, pPL->rot.y, PI);                                             \
    ang = fabsf(ang);                                                                              \
    if (ang < 1.5707964f) {                                                                        \
        FSet(pPL->rot.y, pPL->rot.y + Muku(&pPL->pos, &(em)->pos, pPL->rot.y, PI));                \
        pPL->xFF = 0;                                                                              \
    } else {                                                                                       \
        FSet(pPL->rot.y, pPL->rot.y + Muku(&(em)->pos, &pPL->pos, pPL->rot.y, PI));                \
        pPL->xFF = 1;                                                                              \
    }

#define EM39_ATK_KNOCK(em)                                                                          \
    pPL->rot.y = GetXZAngle(&pPL->pos, &(em)->pos);                                                \
    PlSetDamage(8, 0, 0);

int em39AtkCk2(cEm39* em, int no, Vec* a, Vec* b)
{
    Em39Work* w = EM39_WK(em);
    int hit = w->x8B6;
    u32 res;
    f32 ang;

    if (hit) {
        return 0;
    }
    res = EmAtkHitCk(&em39_atk_tbl[no - 1], a, b, 0); // the target addresses .data+0x298 = the entry before the table
    if (res) {
        if (res & 1) {
            switch ((u32) no) {
            case 2:
                EmPlBloodSet2(em, a, 1, 0x2F, 0x2C);
                SndCall(8, 0x3D, &pPL->pos, em->id, 0, pPL);
                break;
            case 5:
                EmPlBloodSet2(em, a, 1, 0x2F, 0x3A);
                SndCall(8, 0x3D, &pPL->pos, em->id, 0, pPL);
                if ((s16) pG->pl_life <= 0) {
                    EM39_ATK_KNOCK(em);
                } else {
                    EM39_ATK_SIDE(em, ang);
                }
                break;
            case 7:
                EmPlBloodSet2(em, a, 1, 0x2F, 0x3A);
                SndCall(8, 0x3D, &pPL->pos, em->id, 0, pPL);
                if (pG->x4FB8 == 2) {
                    EM39_ATK_KNOCK(em);
                } else {
                    SetPlDamage((int) em, plem39_Stamp);
                }
                break;
            case 8:
                EmPlBloodSet2(em, a, 1, 0x2F, 0x3A);
                SndCall(8, 0x38, &pPL->pos, em->id, 0, pPL);
                if ((s16) pG->pl_life <= 0) {
                    EM39_ATK_KNOCK(em);
                } else {
                    EM39_ATK_SIDE(em, ang);
                }
                break;
            case 9:
                EmPlBloodSet2(em, a, 1, 0x2F, 0x3A);
                SndCall(8, 0x38, &pPL->pos, em->id, 0, pPL);
                EM39_ATK_KNOCK(em);
                break;
            case 0xA:
                EmPlBloodSet2(em, a, 1, 0x2F, 0x3A);
                SndCall(8, 0x38, &pPL->pos, em->id, 0, pPL);
                break;
            }
            w->x8B6 = 1;
        }
        if (res & 2) {
            EmSubBloodSet(em, a, 1, 0xFF, 0xFF);
            w->x8B6 = 1;
        }
        QuakeExec(0, 0, 5, 22.0f, 2);
        if (em->type != 2) {
            ang = Muku(&pPL->pos, &em->pos, pPL->rot.y, PI);
            ang = fabsf(ang);
            if (!(ang < 2.3561945f)) {
                if ((u8) (Rnd() % 10) > 4) {
                    em39SetSpeech(em, 0x3C, 0x21);
                } else {
                    em39SetSpeech(em, 0x3C, 0x22);
                }
            }
        }
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        return 1;
    }
    return 0;
}

// Flags bit 17 when the player stands near one of the two tower bases.
void em39PLNearTowerCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec a = { -8462.0f, -3150.0f, -7937.0f };
    Vec b = { 7111.0f, -3150.0f, -8987.0f };

    {
        f32 d;
        d = (pPLS->pos.x - a.x) * (pPL->pos.x - a.x) + (pPL->pos.y - a.y) * (pPL->pos.y - a.y) + (pPL->pos.z - a.z) * (pPL->pos.z - a.z);
        if (d < 4000000.0f) {
            w->flags |= 0x20000;
        }
        d = (pPL->pos.x - b.x) * (pPL->pos.x - b.x) + (pPL->pos.y - b.y) * (pPL->pos.y - b.y) + (pPL->pos.z - b.z) * (pPL->pos.z - b.z);
        if (d < 4000000.0f) {
            w->flags |= 0x20000;
        }
    }
}

// Jump down: look for a ledge in one of the four directions (front / back / right / left), or the
// fixed drop point of the second area.
#define EM39_JUMPDOWN_PROBE(em, w, a, b, hit, ax, az, bx, bz, res, sgn, rtnFE, rtnFF)                \
    a.x = ax;                                                                                      \
    a.y = 500.0f;                                                                                  \
    a.z = az;                                                                                      \
    b.x = bx;                                                                                      \
    b.y = 500.0f;                                                                                  \
    b.z = bz;                                                                                      \
    PSMTXMultVec((em)->mat, &a, &a);                                                               \
    PSMTXMultVec((em)->mat, &b, &b);                                                               \
    res = SatMgr.hitCheck(&a, &b, 0, &hit, 0, 0) & 0x142810;                                       \
    if (res) {                                                                                     \
        (w)->jumpAng = atan2f(sgn hit.x, sgn hit.z);                                               \
        EmRoutineSet(em, 1, 0x13, rtnFE, rtnFF);                                                   \
        return 1;                                                                                  \
    }

int em39JumpDownCk(cEm39* em, int force)
{
    Em39Work* w = EM39_WK(em);
    Vec drop = { 8941.0f, 2050.0f, -9080.0f };
    Vec a;
    Vec b;
    Vec hit;
    u32 res0;
    u32 res1;
    u32 res2;

    // Every `return 0` is written before the last probe's: jump2 keeps the LAST copy as the cross-jump survivor.
    if (em->type == 2) {
        return 0;
    }
    if ((pG->flags_51E4 & 3) == (em->emsetNo & 3)) {
        if (SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) < em->pos.y - 350.0f) {
            a = em->pos;
            a.z += 100.0f;
            a.x += 100.0f;
            if (SatMgr.getFloor(&a, 600.0f, 100000.0f, 0, 0) < em->pos.y - 350.0f) {
                w->jumpAng = em->rot.y;
                EmRoutineSet(em, 1, 0x13, 0, 0);
                return 1;
            }
        }
    }
    if ((em->pos.x - drop.x) * (em->pos.x - drop.x) + (em->pos.z - drop.z) * (em->pos.z - drop.z) < 4000000.0f) {
        return 0;
    }
    if (force == 0) {
        f32 ang;

        if ((u32) w->stuckCnt % 10 != 5) {
            return 0;
        }
        ang = GetXZAngle(&em->pos, &w->targetPos);
        if (fabsf(Muku2(em->rot.y, ang, PI)) > 0.2617994f) {
            return 0;
        }
    }
    EM39_JUMPDOWN_PROBE(em, w, a, b, hit, 0.0f, 0.0f, 0.0f, 1000.0f, res0, -, 0, 0);
    EM39_JUMPDOWN_PROBE(em, w, a, b, hit, 0.0f, 0.0f, 0.0f, -1000.0f, res1, +, 0, 1);
    EM39_JUMPDOWN_PROBE(em, w, a, b, hit, 0.0f, 0.0f, 1000.0f, 0.0f, res2, -, 0, 0);
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = -1000.0f;
    b.y = 500.0f;
    b.z = 0.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (SatMgr.hitCheck(&a, &b, 0, &hit, 0, 0) & 0x142810) {
        w->jumpAng = atan2f(-hit.x, -hit.z);
        EmRoutineSet(em, 1, 0x13, 0, 0);
        return 1;
    }
    return 0;
}

// Jump up: a ladder object (type 0x13) or a scenario ladder near the enemy.
int em39JumpUpCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Mtx m;
    Vec a;
    Vec pos;
    s8 level;
    f32 ang;
    u32 i;

    if (em39JumpUpCk2(em)) {
        return 1;
    }
    if (w->targetPos.y - em->pos.y < 1000.0f) {
        return 0;
    }
    for (i = 0; i < ObjMgr.nArray; i++) {
        cObj* o = (cObj*) ((u8*) ObjMgr.pArray + ObjMgr.size * i);
        int alive = o->be_flag & 0x201;

        if (alive != 1) {
            continue;
        }
        if (o->id != 0x13) {
            continue;
        }
        if ((em->oldPos.x - o->pos.x) * (em->oldPos.x - o->pos.x) + (em->oldPos.y - o->pos.y) * (em->oldPos.y - o->pos.y) + (em->oldPos.z - o->pos.z) * (em->oldPos.z - o->pos.z) > 4000000.0f) {
            continue;
        }
        PSMTXRotRad(m, 'y', o->rot.y);
        TransMatrix(m, &o->pos);
        a.x = 0.0f;
        a.y = 3200.0f;
        a.z = -2000.0f;
        PSMTXMultVec(m, &a, &a);
        if (!(fabsf(Muku(&em->pos, &a, em->rot.y, PI)) > 1.0471976f)) {
            w->jumpAng = GetXZAngle(&em->pos, &a);
            w->jumpPos = a;
            EmRoutineSet(em, alive, 0x14, 0, 0);
            return 1;
        }
    }
    if (SceAtSearchLadder(em, &pos, &ang, (u8*) &level) == 0) {
        return 0;
    }
    if ((em->pos.x - pos.x) * (em->pos.x - pos.x) + (em->pos.y - pos.y) * (em->pos.y - pos.y) + (em->pos.z - pos.z) * (em->pos.z - pos.z) > 1000000.0f) {
        return 0;
    }
    if (fabsf(Muku2(em->rot.y, ang, PI)) > 1.5707964f) {
        return 0;
    }
    PSMTXRotRad(m, 'y', ang);
    TransMatrix(m, &pos);
    a.x = 0.0f;
    a.y = (f32) level * 1000.0f;
    a.z = 2000.0f;
    PSMTXMultVec(m, &a, &a);
    w->jumpAng = GetXZAngle(&em->pos, &a);
    w->jumpPos = a;
    EmRoutineSet(em, 1, 0x15, 0, 0);
    return 1;
}

#define EM39_EMI ((EmiData*) pG->pRoomEmi)

// Squared distance from the enemy to an EMI point.
#define EM39_EMI_DIST2(em, e)                                                                       \
    (((em)->pos.x - (e)->pos.x) * ((em)->pos.x - (e)->pos.x) + ((em)->pos.y - (e)->pos.y) * ((em)->pos.y - (e)->pos.y) + \
     ((em)->pos.z - (e)->pos.z) * ((em)->pos.z - (e)->pos.z))

// Jump up along a pair of EMI type 0x11 points (state 0 at the foot, 1 at the top, same group byte).
int em39JumpUpCk2(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    u32 i;
    u32 j;

    if (pG->pRoomEmi == 0) {
        return 0;
    }
    for (i = 0; i < EM39_EMI->n; i++) {
        EmiEntry* e = &EM39_EMI->entry[i];

        if (e->type != 0x11) {
            continue;
        }
        if (e->sub != 0) {
            continue;
        }
        if (e->state != 0) {
            continue;
        }
        if (EM39_EMI_DIST2(em, e) > 1000000.0f) {
            continue;
        }
        for (j = 0; j < EM39_EMI->n; j++) {
            EmiEntry* f = &EM39_EMI->entry[j];
            int sub;
            int state;
            f32 ang;

            if (f->type != 0x11) {
                continue;
            }
            sub = f->sub;
            if (sub != 0) {
                continue;
            }
            state = f->state;
            if (state != 1) {
                continue;
            }
            if (e->pad_3 != f->pad_3) {
                continue;
            }
            if (fabsf(Muku(&em->pos, &f->pos, em->rot.y, PI)) > 0.2617994f) {
                continue;
            }
            ang = GetXZAngle(&em->pos, &w->targetPos);
            if (fabsf(Muku2(ang, GetXZAngle(&em->pos, &f->pos), PI)) > 0.2617994f) {
                continue;
            }
            w->jumpAng = GetXZAngle(&em->pos, &f->pos);
            w->jumpPos = f->pos;
            EmRoutineSet(em, state, 0x15, sub, sub);
            return 1;
        }
    }
    return 0;
}

// Drop down along a pair of EMI type 0x11 points (sub 1).
int em39JumpUpCk3(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    EmiData* emi = EM39_EMI;
    u32 i;
    u32 j;

    if (emi == 0) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        // EM39_EMI (not emi) at the body top: the pG->pRoomEmi reload is hoisted out of the outer loop
        // (no conditional jump passed yet, so the trapping load is movable) and cse2 folds it to a copy
        // of emi; the inner loop's duplicated entry test then reads ->n through that copy while its
        // latch reloads pG->pRoomEmi (after LOOP_VTOP, hoisted into the inner preheader).
        EmiEntry* e = &EM39_EMI->entry[i];

        if (e->type != 0x11) {
            continue;
        }
        if (e->sub != 1) {
            continue;
        }
        if (e->state != 0) {
            continue;
        }
        if (EM39_EMI_DIST2(em, e) > 1000000.0f) {
            continue;
        }
        for (j = 0; j < EM39_EMI->n; j++) {
            EmiEntry* f = &EM39_EMI->entry[j];
            int state;

            if (f->type != 0x11) {
                continue;
            }
            if (f->sub != 1) {
                continue;
            }
            state = f->state;
            if (state != 1) {
                continue;
            }
            if (e->pad_3 != f->pad_3) {
                continue;
            }
            w->jumpAng = GetXZAngle(&em->pos, &f->pos);
            w->jumpPos = f->pos;
            EmRoutineSet(em, state, 0x16, 0, 0);
            return 1;
        }
    }
    return 0;
}

// Blood effect by the weapon that hit (dmWep), the big ones only from far away.
void em39BloodSet(cEm39* em)
{
    int far = 0;

    if (em->dmPart->rad < 36000000.0f) {
        far = 1;
    }
    switch (em->dmWep) {
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        EmDmBloodSet2(em, 0x2F, 1, 0, 0, 0);
        break;
    case 7:
    case 8:
    case 0x21:
        if (far) {
            EmDmBloodSet2(em, 0x2F, 2, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x2F, 0, 0, 0, 0);
        }
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xE:
    case 0x10:
    case 0x11:
    case 0x26:
    case 0x2B:
        EmDmBloodSet2(em, 0x2F, 0, 0, 0, 0);
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x15:
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
        EmDmBloodSet2(em, 0x2F, 2, 0, 0, 0);
        break;
    case 0:
    case 0x14:
    default:
        break; // explicit: the two nodes shape the tree (tools/research/casetree.py search)
    }
}

// Two-motion blend: m0 as the main motion, m1 / m2 by the sign of gunPitch as the blended one.
void em39BlendMotSet(cEm39* em, void* m0, void* m1, void* m2, int a, int b, int c, u16 d)
{
    Em39Work* w = EM39_WK(em);
    MotionWorkSub* bm;
    void* m;
    int seq;
    int ai, dd;
    asm("" : "=r"(ai) : "0"((int) a)); // COMPILER-DIFF: #2 (u16 argument masked at the calls)
    asm("" : "=r"(dd) : "0"((int) d)); // COMPILER-DIFF: #2
    f32 rate = fabsf(w->gunPitch);

    MotionSetCore(em, MOTION(em), m0, ai, (u8) w->x6D0, (u16) dd, (u16) w->x6D4);
    if (w->gunPitch < 0.0f) {
        m = m1;
        seq = b;
    } else {
        m = m2;
        seq = c;
    }
    bm = &w->blendMot;
    MotionSetCore(em, bm, m, seq, (u8) w->x6D0, (u16) dd, (u16) w->x6D4);
    em->blendMot = bm;
    bm->blendRate = rate * 0.00390625f;
    if (w->x6D0) {
        w->x6D0--;
    }
    w->x6D4++;
    if ((u32) w->x6D4 >= em->frameMax) {
        w->x6D4 = 0;
    }
}

// Appear at an EMI type 0xE point: sub 0 = drop in, 1 = door (facing the player), 2 = fixed spot.
#define EM39_APPEAR_POS(em, w, e)                                                                   \
    (w)->pGotoPoint = e;                                                                           \
    (em)->dmType = 2;                                                                              \
    AtariOff(&(em)->atari, 0xFCFF);                                                                \
    (em)->setPos(&(e)->pos);                                                                       \
    (em)->pos = (e)->pos;                                                                          \
    (em)->oldPos = (em)->pos;

int em39AppearCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec plPos;
    Vec pos;
    int retry;
    u32 i;

    if (pG->pRoomEmi == 0) {
        return 0;
    }
    if (pPL->pos.y < -1000.0f) {
        return 0;
    }
    if (pPL->pos.x > 20000.0f) {
        return 0;
    }
    plPos = pPL->pos;
    plPos.y += 1500.0f;
    retry = 1;
    GetPlPos(&pos, 0, 20.0f);
    for (i = 0; i < EM39_EMI->n; i++) {
        EmiEntry* e = &EM39_EMI->entry[i];

        if (e->type != 0xE) {
            continue;
        }
        if (w->x8C0 == i) {
            continue;
        }
        if (w->x8C0 != -1 && retry) {
            u32 ofs = w->x8C0 * 0x40 + 8;

            if ((*(u32*) ((u32) EM39_EMI + ofs) & 0x00FF00FF) == (*(u32*) e & 0x00FF00FF)) {
                continue;
            }
        }
        switch (e->sub) {
        case 0: {
            int st = e->state;
            if (st != 0) {
                continue;
            }
            if (em39AreaCk(em, 0, 1, e->pad_3) == 0) {
                continue;
            }
            w->x698 = st;
            EM39_APPEAR_POS(em, w, e);
            em->rot.y = GetXZAngle(&em->pos, &pPLS->pos);
            w->x680 = st;
            em->xFC = 1;
            em->xFD = 5;
            em->xFE = st;
            em->xFF = 1;
            w->x8C0 = i;
            w->dmgTotal = st;
            w->flags |= 0x800;
            w->flags &= ~0x2000;
            return 1;
        }
        case 1: {
            u32 j;
            int found;

            if (e->state != 0) {
                continue;
            }
            if (em39AreaCk(em, 1, 1, e->pad_3) == 0) {
                continue;
            }
            if (fabsf(Muku(&pPL->pos, &e->pos, pPL->rot.y, PI)) > 0.7853982f) {
                continue;
            }
            found = 0;
            for (j = i + 1; j < EM39_EMI->n; j++) {
                u8* f = (u8*) &EM39_EMI->entry[j];

                if (f[0] == 0xE && f[1] == e->sub && f[2] == e->state && f[3] == e->pad_3) {
                    found = 1;
                    break;
                }
            }
            if (found && (u8) (Rnd() % 10) > 3) {
                retry = 0;
                w->x8C0 = i;
                continue;
            }
            w->x698 = 0;
            EM39_APPEAR_POS(em, w, e);
            em->rot.y = e->rotY + PI;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
            w->x680 = 0;
            em->xFC = 1;
            em->xFD = 7;
            em->xFE = 0;
            em->xFF = 1;
            w->dmgTotal = 0;
            w->x8C0 = i;
            w->flags |= 0x800;
            w->flags &= ~0x2000;
            return 1;
        }
        case 2: {
            int st = e->state;
            if (st != 0) {
                continue;
            }
            if (em39AreaCk(em, 2, 1, e->pad_3) == 0) {
                continue;
            }
            w->x698 = st;
            w->pGotoPoint = (EmiEntry*) st;
            em->dmType = 2;
            AtariOff(&em->atari, 0xFCFF);
            em->setPos(&e->pos);
            em->pos = e->pos;
            em->oldPos = em->pos;
            em->rot.y = e->rotY;
            w->x680 = st;
            EmRoutineSet(em, 1, 9, st, st);
            w->dmgTotal = st;
            w->x8C0 = i;
            w->flags |= 0x800;
            w->flags &= ~0x2000;
            return 1;
        }
        default:
            continue;
        }
    }
    return 0;
}

// The player stands on the exit point (EMI type 0xE, state 2) of the enemy's entry group.
int em39ExitCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    EmiData* emi = EM39_EMI;
    u32 i;

    if (emi == 0) {
        return 0;
    }
    if (w->pGotoPoint == 0) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type != 0xE) {
            continue;
        }
        if (e->sub != w->pGotoPoint->sub) {
            continue;
        }
        if (e->state != 2) {
            continue;
        }
        if (e->pad_3 != w->pGotoPoint->pad_3) {
            continue;
        }
        if (!((pPL->pos.x - e->pos.x) * (pPL->pos.x - e->pos.x) + (pPL->pos.z - e->pos.z) * (pPL->pos.z - e->pos.z) > 4000000.0f)) {
            w->flags |= 0x2000;
            return 1;
        }
    }
    return 0;
}

// Move to the group's EMI type 0xE state 3 point, and retire the group's other points.
int em39AreaMoveCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    u32 i;
    // `t`/`ofs` shared by both loops: with two sets each they are non-replaceable user-variable givs whose benefit
    // (3/5 - copy_cost 4 - add_cost 2) is negative, so loop.c leaves the outer `i*64+8` unreduced (`slwi; addi 8` per
    // iteration like the target); the inner loop's copies get a final value and are reduced as before.
    u32 ofs;
    u32 t;

    if (pG->pRoomEmi == 0) {
        return 0;
    }
    if (w->pGotoPoint == 0) {
        return 0;
    }
    for (i = 0; i < EM39_EMI->n; i++) {
        EmiEntry* e;

        t = i * 0x40;
        ofs = t + 8;
        e = (EmiEntry*) ((u32) EM39_EMI + ofs);

        if (e->type != 0xE) {
            continue;
        }
        if (e->sub != w->pGotoPoint->sub) {
            continue;
        }
        if (e->state != 3) {
            continue;
        }
        if (e->pad_3 != w->pGotoPoint->pad_3) {
            continue;
        }
        w->gotoOn = 1;
        w->gotoPos = e->pos;
        em->xFC = 1;
        em->xFD = 0xA;
        em->xFE = 0;
        em->xFF = 0;
        for (i = 0; i < EM39_EMI->n; i++) {
            EmiEntry* f;

            t = i * 0x40;
            ofs = t + 8;
            f = (EmiEntry*) ((u32) EM39_EMI + ofs);

            if (f->type != 0xE) {
                continue;
            }
            if (f->sub != w->pGotoPoint->sub) {
                continue;
            }
            if (f->pad_3 != w->pGotoPoint->pad_3) {
                continue;
            }
            f->type = 0;
        }
        w->pGotoPoint = 0;
        return 1;
    }
    return 0;
}

// Change the sitting point to another free EMI type 0xE point of the same group.
int em39SitChg(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    u32 i;

    if (pG->pRoomEmi == 0) {
        return 0;
    }
    for (i = 0; i < EM39_EMI->n; i++) {
        EmiEntry* e = &EM39_EMI->entry[i];

        if (e->type != 0xE) {
            continue;
        }
        if (e == w->pGotoPoint) {
            continue;
        }
        if (e->sub != 0) {
            continue;
        }
        if (e->state != 0) {
            continue;
        }
        if (e->state != w->pGotoPoint->sub) {
            continue;
        }
        if (e->pad_3 != w->pGotoPoint->pad_3) {
            continue;
        }
        if ((u8) (Rnd() % 10) > 4) {
            w->pGotoPoint = e;
            return 1;
        }
    }
    return 0;
}

// The player is within 2000 of an EMI type 0xE point with the given sub / state / group.
int em39AreaCk(cEm39* em, int sub, int state, int group)
{
    EmiData* emi = EM39_EMI;
    u32 i;

    if (emi == 0) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type != 0xE) {
            continue;
        }
        if (e->sub != sub) {
            continue;
        }
        if (e->state != state) {
            continue;
        }
        if (e->pad_3 != group) {
            continue;
        }
        if (!((pPL->pos.x - e->pos.x) * (pPL->pos.x - e->pos.x) + (pPL->pos.y - e->pos.y) * (pPL->pos.y - e->pos.y) + (pPL->pos.z - e->pos.z) * (pPL->pos.z - e->pos.z) > 4000000.0f)) {
            return 1;
        }
    }
    return 0;
}

// Weapon in hand: 0 none, 1 knife, 2 (grenade), 3 machine gun, 4 bow.
void em39WepSet(cEm39* em, int no)
{
    Em39Work* w = EM39_WK(em);

    w->x8B4 = no;
    if (w->pWep2) {
        w->pWep2->setTransMode(0);
    }
    if (w->pWep) {
        w->pWep->setTransMode(0);
    }
    if (w->pWep3) {
        w->pWep3->setTransMode(0);
    }
    if (w->x66C) {
        w->x66C->be_flag |= 8;
    }
    switch ((u32) no) {
    case 1:
        if (w->pWep) {
            w->pWep->setTransMode(1);
        }
        if (w->x66C) {
            w->x66C->be_flag &= ~8;
        }
        break;
    case 3:
        if (w->pWep2) {
            w->pWep2->setTransMode(1);
        }
        break;
    case 4:
        if (w->pWep3) {
            w->pWep3->setTransMode(1);
        }
        break;
    case 0:
    case 2:
    default:
        break;
    }
}

// Catch range: the player in the enemy's local box, nothing between them and beside them.
#define EM39_CATCH_BODY(em, w)                                                                      \
    Mtx inv;                                                                                       \
    Vec p;                                                                                         \
    Vec a;                                                                                         \
    Vec b;                                                                                         \
    Mtx m;                                                                                         \
    int dead = 1;                                                                                  \
    int noFlag;                                                                                    \
                                                                                                   \
    if ((pPL->flags_324 & 0xFFFF0000) == 0) {                                                      \
        dead = 0;                                                                                  \
    }                                                                                              \
    if (dead) {                                                                                    \
        return 0;                                                                                  \
    }                                                                                              \
    if ((s16) pG->pl_life <= 0) {                                                                  \
        return 0;                                                                                  \
    }                                                                                              \
    if ((em)->hp <= 0) {                                                                           \
        return 0;                                                                                  \
    }                                                                                              \
    if (!((em)->seFlags28B & 2)) {                                                                 \
        return 0;                                                                                  \
    }                                                                                              \
    noFlag = !((w)->flags & 1);                                                                    \
    if (noFlag) {                                                                                  \
        return 0;                                                                                  \
    }                                                                                              \
    if (pG->flags_5010 & 0x8000) {                                                                 \
        return 0;                                                                                  \
    }                                                                                              \
    PSMTXInverse((em)->mat, inv);                                                                  \
    PSMTXMultVec(inv, &pPL->pos, &p);                                                              \
    if (p.x < -800.0f) {                                                                           \
        return 0;                                                                                  \
    }                                                                                              \
    if (p.x > 800.0f) {                                                                            \
        return 0;                                                                                  \
    }                                                                                              \
    if (p.y < -500.0f) {                                                                           \
        return 0;                                                                                  \
    }                                                                                              \
    if (p.y > 500.0f) {                                                                            \
        return 0;                                                                                  \
    }                                                                                              \
    if (p.z < 0.0f) {                                                                              \
        return 0;                                                                                  \
    }                                                                                              \
    if (p.z > 1700.0f) {                                                                           \
        return 0;                                                                                  \
    }                                                                                              \
    a = (em)->pos;                                                                                 \
    b = pPL->pos;                                                                                  \
    a.y += 500.0f;                                                                                 \
    b.y += 500.0f;                                                                                 \
    if (SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {                                                     \
        return 0;                                                                                  \
    }                                                                                              \
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {                                                     \
        return 0;                                                                                  \
    }                                                                                              \
    PSMTXRotRad(m, 'y', GetXZAngle(&(em)->pos, &pPL->pos));                                        \
    TransMatrix(m, &(em)->pos);                                                                    \
    a.x = 300.0f;                                                                                  \
    a.y = 500.0f;                                                                                  \
    a.z = 0.0f;                                                                                    \
    b.x = 300.0f;                                                                                  \
    b.y = 500.0f;                                                                                  \
    b.z = 500.0f;                                                                                  \
    PSMTXMultVec(m, &a, &a);                                                                       \
    PSMTXMultVec(m, &b, &b);                                                                       \
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {                                                     \
        return 0;                                                                                  \
    }                                                                                              \
    a.x = 300.0f;                                                                                  \
    a.y = 500.0f;                                                                                  \
    a.z = 0.0f;                                                                                    \
    b.x = 300.0f;                                                                                  \
    b.y = 500.0f;                                                                                  \
    b.z = 500.0f;                                                                                  \
    PSMTXMultVec(m, &a, &a);                                                                       \
    PSMTXMultVec(m, &b, &b);                                                                       \
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {                                                     \
        return 0;                                                                                  \
    }                                                                                              \
    pPL->dmg.set(0, 2);                                                                            \
    (em)->dmg.set(0, 2);                                                                           \
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);                           \
    return 1;

int em39CatchCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    EM39_CATCH_BODY(em, w);
}

int em39KickHitCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    EM39_CATCH_BODY(em, w);
}

// Arrow on the bow (weapon 0x37): only while no thrown knife is out.
void em39ArrowSet(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec pos;
    Vec rot;

    if (w->pWep3 == 0) {
        return;
    }
    if (w->x58C) {
        return;
    }
    w->pWep3->getPartsPtr(4);
    pos.x = -850.0f;
    pos.y = 5.0f;
    pos.z = 24.0f;
    rot.x = 0.0f;
    rot.y = -1.5707964f;
    rot.z = 0.0f;
    w->x58C = SetWeapon(ARC(0x37), ARC(0x38), &pos, &rot, 0);
    if (w->x58C) {
        w->x58C->setParent(em, 0xA, 0);
        em39BowSet(em, 0);
        SndCall(8, 0x10, &em->pos, em->id, 0, em);
    }
}

// Arrow shot from the bow at `target` (mode 1 / 2: aimed 1000 to the right / left of it).
#define EM39_ARROW_AIM(pos, tgt, dir, target)                                                       \
    tgt = *(target);                                                                               \
    PSVECSubtract(&(pos), &(tgt), &(dir));                                                         \
    dir.y = 0.0f;                                                                                  \
    if (dir.x == 0.0f && dir.z == 0.0f) {                                                          \
        dir.z = 1.0f;                                                                              \
    }

void em39ArrowFire(cEm39* em, Vec* target, int mode)
{
    Em39Work* w = EM39_WK(em);
    Mtx m;
    Vec pos;
    Vec tgt;
    Vec dir;

    if (w->pWep3 == 0) {
        return;
    }
    if (w->x58C == 0) {
        return;
    }
    em39BowSet(em, 0);
    w->x58C->setTransMode(1);
    {
        cModel* p = w->pWep3->getPartsPtr(4);

        pos.x = 500.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        PSMTXMultVec(p->mat, &pos, &pos);
    }
    TransMatrix(w->x58C->mat, &pos);
    switch ((u32) mode) {
    case 0:
    default:
        EM39_ARROW_AIM(pos, tgt, dir, target);
#line 11941 "D:/Bio4/Prog/em39.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, 1500.0f);
        PSVECAdd(&tgt, &dir, &tgt);
        break;
    case 1:
        EM39_ARROW_AIM(pos, tgt, dir, target);
#line 11951 "D:/Bio4/Prog/em39.cpp"
        VECNormalize(&dir, &dir);
        PSMTXRotRad(m, 'y', 1.5707964f);
        PSMTXMultVecSR(m, &dir, &dir);
        PSVECScale(&dir, &dir, 1000.0f);
        PSVECAdd(&tgt, &dir, &tgt);
        break;
    case 2:
        EM39_ARROW_AIM(pos, tgt, dir, target);
#line 11963 "D:/Bio4/Prog/em39.cpp"
        VECNormalize(&dir, &dir);
        PSMTXRotRad(m, 'y', -1.5707964f);
        PSMTXMultVecSR(m, &dir, &dir);
        PSVECScale(&dir, &dir, 1000.0f);
        PSVECAdd(&tgt, &dir, &tgt);
        break;
    }
    PSVECSubtract(&tgt, &pos, &dir);
    if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f) {
        dir.z = 1.0f;
    }
#line 11973 "D:/Bio4/Prog/em39.cpp"
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, 1500.0f);
    dir.x += fRand1_1() * 10.0f;
    dir.y += fRand1_1() * 10.0f;
    dir.z += fRand1_1() * 10.0f;
    w->x58C->be_flag |= 0x4000;
    w->x58C->setShotArrow(&dir, &em39_atk_tbl[0]);
    w->x58C->setSeHitWall(8, 0x13, em->id);
    w->x58C = 0;
}

// Bow string parts (4) drawn / hidden, arrow transparency and the aiming effect.
void em39BowSet(cEm39* em, int on)
{
    Em39Work* w = EM39_WK(em);
    cModel* p;

    if (w->pWep3 == 0) {
        return;
    }
    p = w->pWep3->getPartsPtr(4);
    if (on) {
        p->scale.x = 1.0f;
        p->scale.y = 1.0f;
        p->scale.z = 1.0f;
        if (w->x58C) {
            w->x58C->setTransMode(0);
        }
        EstSet((int) w->pWep3, -1, 0, 0, 0x2F, 9, 0, w->espKind2, (u32) em, 0);
    } else {
        p->scale.x = 0.0f;
        p->scale.y = 0.0f;
        p->scale.z = 0.0f;
        if (w->x58C) {
            w->x58C->setTransMode(1);
        }
        EffectEspDelete(0, w->espKind2, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind2, (int) em);
        EffectEfmDelete(0, w->espKind2, (int) em);
    }
}

// A door (em 0x41) the knife swing can open: in front, within its frame, openable.
int em39DoorOpenCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec p;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEmDoor* d = (cEmDoor*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        EmDoorWork* dw;
        f32 ang;
        u32 st;

        if ((d->be_flag & 0x201) != 1) {
            continue;
        }
        if (d->id != 0x41) {
            continue;
        }
        if (d->hp <= 0) {
            continue;
        }
        if ((em->pos.x - d->pos.x) * (em->pos.x - d->pos.x) + (em->pos.y - d->pos.y) * (em->pos.y - d->pos.y) + (em->pos.z - d->pos.z) * (em->pos.z - d->pos.z) > 6250000.0f) {
            continue;
        }
        dw = EMDOOR_WK(d);
        ang = fabsf(Muku2(dw->rotY, em->rot.y, PI));
        if (ang > 0.7853982f && ang < 2.3561945f) {
            continue;
        }
        PSMTXMultVec(dw->inv, &em->pos, &p);
        if (ang < 1.5707964f) {
            if (p.z > 0.0f) {
                continue;
            }
            if (p.z < -800.0f) {
                continue;
            }
        } else {
            if (p.z < 0.0f) {
                continue;
            }
            if (p.z > 800.0f) {
                continue;
            }
        }
        if (p.x > dw->width) {
            continue;
        }
        if (p.x < -dw->width) {
            continue;
        }
        if (p.y > 500.0f) {
            continue;
        }
        if (p.y < -500.0f) {
            continue;
        }
        st = d->ckOpen();
        if (st) {
            if ((u32) st <= 3) {
                continue;
            }
        }
        if (em->type != 2 && d->type == 0) {
            w->pDoor = d;
            em->xFC = 1;
            em->xFD = 0x19;
            em->xFE = 0;
            em->xFF = 0;
            return 1;
        }
        d->setOpen(&em->pos, 0, 0, 0);
        return 0;
    }
    return 0;
}

// Tower form left arm: its own motion work (armMot) by the arm state x8BB / x8BC.
#define EM39_ARM_MOT(w)  ((MotionWork*) &(w)->armMot)

void em39ArmControl(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    int se = 0;
    void* mot;

    if (em->type != 2) {
        return;
    }
    w->armMot.speedRate = 1.0f;
    w->armMot.flags2 |= 0x10000000;
    switch (w->x8BB) {
    case 0:
        switch (w->x8BC) {
        case 0:
        default:
            mot = ARC(0xFF);
            break;
        case 1:
            mot = ARC(0x103);
            se = 1;
            break;
        case 2:
            mot = ARC(0x105);
            se = 1;
            break;
        }
        MotionSetCore(em, EM39_ARM_MOT(w), mot, 0, 0, 4, 0);
        w->x8BC = 0;
        w->x8BB++;
        goto STEP;
    case 2:
        MotionSetCore(em, EM39_ARM_MOT(w), ARC(0xFF), 0, 0, 4, 0);
        w->x8BB++;
        goto MOVE;
    case 4:
        switch (w->x8BC) {
        case 0:
        default:
            mot = ARC(0x102);
            se = 1;
            break;
        case 1:
            mot = ARC(0x100);
            break;
        case 2:
            mot = ARC(0x107);
            se = 1;
            break;
        }
        MotionSetCore(em, EM39_ARM_MOT(w), mot, 0, 0, 4, 0);
        w->x8BC = 1;
        w->x8BB++;
        goto STEP;
    case 6:
        MotionSetCore(em, EM39_ARM_MOT(w), ARC(0x100), 0, 0, 4, 0);
        w->x8BB++;
        goto MOVE;
    case 8:
        switch (w->x8BC) {
        case 0:
        default:
            mot = ARC(0x104);
            se = 1;
            break;
        case 1:
            mot = ARC(0x106);
            se = 1;
            break;
        case 2:
            mot = ARC(0x101);
            break;
        }
        MotionSetCore(em, EM39_ARM_MOT(w), mot, 0, 0, 4, 0);
        w->x8BC = 2;
        w->x8BB++;
    case 1:
    case 5:
    case 9:
    STEP:
        MotionMoveCore(em, EM39_ARM_MOT(w), 0);
        if (MotionSequenceCtrl(EM39_ARM_MOT(w))) {
            w->x8BB++;
        }
        break;
    case 0xA:
        MotionSetCore(em, EM39_ARM_MOT(w), ARC(0x101), 0, 0, 4, 0);
        w->x8BB++;
        goto MOVE;
    case 0xC:
        w->armMot.speedRate = 1.0f;
        w->armMot.flags2 |= 0x10000000;
        MotionSetCore(em, EM39_ARM_MOT(w), ARC(0x108), 0, 0, 1, 0);
        w->x8BC = 1;
        se = 1;
        w->x8BB++;
        goto MOVE;
    case 0xE:
        w->armMot.speedRate = 1.0f;
        w->armMot.flags2 |= 0x10000000;
        MotionSetCore(em, EM39_ARM_MOT(w), ARC(0x109), 0, 0, 1, 0);
        w->x8BC = 1;
        se = 1;
        w->x8BB++;
        goto MOVE;
    case 0x10:
        w->armMot.speedRate = 1.0f;
        w->armMot.flags2 |= 0x10000000;
        MotionSetCore(em, EM39_ARM_MOT(w), ARC(0x10A), 0, 0, 1, 0);
        w->x8BC = se;
        w->x8BB++;
    case 3:
    case 7:
    case 0xB:
    case 0xD:
    case 0xF:
    case 0x11:
    MOVE:
        MotionMoveCore(em, EM39_ARM_MOT(w), 0);
        MotionSequenceCtrl(EM39_ARM_MOT(w));
        break;
    case 0x12: {
        int mf = ((MotionData*) ARC(0x10A))->maxFrame;

        w->armMot.speedRate = 1.0f;
        w->armMot.flags2 |= 0x10000000;
        MotionSetCore(em, EM39_ARM_MOT(w), ARC(0x10A), 0, 0, 0, (u16) ((mf & 0x3FFF) - 1));
        w->x8BC = se;
        w->x8BB++;
    }
    case 0x13:
        MotionMoveCore(em, EM39_ARM_MOT(w), 0);
        MotionSequenceCtrl(EM39_ARM_MOT(w));
        break;
    }
    if (se) {
        SndCall(8, 0x53, &em->getPartsPtr(0xE)->worldPos, em->id, 0, em);
    }
}

// Fence jump: a fence (attribute 0x20) right in front; the landing side is the free one.
int em39FanceJumpCk2(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec a;
    Vec b;
    Vec hit;
    Vec nrm;
    Mtx m;
    Vec c;
    Vec d;
    Vec e;
    int side;
    f32 ang;

    if ((u32) w->stuckCnt % 10 != 6) {
        return 0;
    }
    a.x = 0.0f;
    a.y = 500.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 500.0f;
    b.z = 800.0f;
    PSMTXMultVec(em->mat, &a, &a);
    PSMTXMultVec(em->mat, &b, &b);
    if (!(SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0) & 0x20)) {
        return 0;
    }
    ang = atan2f(-nrm.x, -nrm.z);
    w->jumpAng = ang;
    side = 0;
    w->jumpPos = em->pos;
    PSMTXRotRad(m, 'y', ang);
    TransMatrix(m, &em->pos);
    d.x = 300.0f;
    d.y = 1200.0f;
    d.z = 0.0f;
    e.x = 300.0f;
    e.y = 1200.0f;
    e.z = 800.0f;
    PSMTXMultVec(m, &d, &d);
    PSMTXMultVec(m, &e, &e);
    if (SatMgr.hitCheck(&d, &e, 0, 0, 0, 0x20)) {
        side = 1;
    }
    d.x = -300.0f;
    d.y = 1200.0f;
    d.z = 0.0f;
    e.x = -300.0f;
    e.y = 1200.0f;
    e.z = 800.0f;
    PSMTXMultVec(m, &d, &d);
    PSMTXMultVec(m, &e, &e);
    if (SatMgr.hitCheck(&d, &e, 0, 0, 0, 0x20)) {
        side |= 2;
    }
    if (side == 3) {
        return 0;
    }
    if (side & 1) {
        c.x = -300.0f;
        c.y = 0.0f;
        c.z = 0.0f;
        PSMTXMultVec(m, &c, &w->jumpPos);
    }
    if (side & 2) {
        c.x = 300.0f;
        c.y = 0.0f;
        c.z = 0.0f;
        PSMTXMultVec(m, &c, &w->jumpPos);
    }
    return 1;
}

int em39FanceJumpCk(cEm39* em)
{
    if (em39FanceJumpCk2(em)) {
        EmRoutineSet(em, 1, 0x17, 0, 0);
        return 1;
    }
    return 0;
}

// Damage value of the hit (100 for the non-weapon ids), doubled at the head parts.
int em39SetDmVal(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    EmHitInfo* h = em->dmPart;
    int far = 0;
    int val;

    if (h->rad < 36000000.0f) {
        far = 1;
    }
    val = 100;
    if (em->dmWep <= 0x2D) {
        val = GetWepDmVal(em, em->dmWep, far);
    }
    if (h->partsNo == 5) {
        val += val;
        w->x698 += 200;
    }
    return val;
}

// Attack return: what the enemy does after an attack ended.
int em39AtkRtnCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec a;
    Vec b;
    Vec c;
    int dead = 1;
    f32 dy;
    f32 ang;
    int hit;
    int x684;
    int noFlag;

    if ((pPL->flags_324 & 0xFFFF0000) == 0) {
        dead = 0;
    }
    if (dead) {
        return 0;
    }
    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    if (em->hp <= 0) {
        return 0;
    }
    noFlag = !(w->flags & 1);
    if (noFlag) {
        return 0;
    }
    if (w->x680) {
        return 0;
    }
    dy = em->pos.y - pPL->pos.y;
    dy = fabsf(dy);
    ang = fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, PI));
    if (em->type == 2) {
        if (pG->x4F88 > 1 && w->x688 == 0 && (w->flags & 1) && em->plDist2 > 36000000.0f && em->plDist2 < 100000000.0f &&
            w->routeAngAbs < 0.5235988f && ang < 0.5235988f) {
            a = pPL->pos;
            b = em->pos;
            a.y += 500.0f;
            b.y += 500.0f;
            if (!SatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
                EmRoutineSet(em, 1, 0x12, 0, 0);
                return 1;
            }
        }
        x684 = w->x684;
        if (x684 == 0 && em->plDist2 < 20250000.0f && em->plDist2 > 12250000.0f && w->routeAngAbs < 1.5707964f) {
            if ((u8) (Rnd() % 10) > 4) {
                EmRoutineSet(em, 1, 0x29, x684, x684);
            } else {
                EmRoutineSet(em, 1, 0x2A, x684, x684);
            }
            return 1;
        }
        if (em->plDist2 < 4840000.0f && w->routeAngAbs < 1.5707964f && dy < 1500.0f) {
            u32 r = (u8) (Rnd() % 100);

            if (em->plDist2 > 4000000.0f && r > 30) {
                if ((u8) (Rnd() % 10) > 4) {
                    EmRoutineSet(em, 1, 0x2B, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 0x2C, 0, 0);
                }
            } else {
                if ((u8) (Rnd() % 10) > 4) {
                    EmRoutineSet(em, 1, 0x28, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 0x27, 0, 0);
                }
            }
            return 1;
        }
        return 0;
    }
    if (em->plDist2 < 2890000.0f && w->routeAngAbs < 1.5707964f && (w->flags & 1)) {
        a = em->pos;
        c = pPLS->pos;
        a.y += 500.0f;
        c.y += 500.0f;
        hit = SatMgr.hitCheck(&a, &c, 0, 0, 0, 0);
        if (hit == 0) {
            if (dy < 250.0f) {
                if (ang < 1.5707964f) {
                    if ((u8) (Rnd() % 10) > 6) {
                        EmRoutineSet(em, 1, 0x1C, hit, hit);
                        return 1;
                    }
                } else {
                    EmRoutineSet(em, 1, 0x1A, hit, hit);
                    return 1;
                }
            }
            if (dy < 1700.0f) {
                EmRoutineSet(em, 1, 0x18, 0, 0);
                return 1;
            }
        }
    }
    x684 = w->x684;
    if (x684 == 0 && (w->flags & 1) && em->plDist2 > 36000000.0f && em->plDist2 < 100000000.0f && w->routeAngAbs < 0.5235988f &&
        dy < 2000.0f) {
        if ((u8) (Rnd() % 10) > 2 || dy > 1000.0f) {
            EmRoutineSet(em, 1, 0x1D, x684, x684);
        } else {
            EmRoutineSet(em, 1, 0x23, x684, x684);
        }
        return 1;
    }
    return 0;
}

int em39GotoCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    if (w->gotoOn == 0) {
        return 0;
    }
    EmRoutineSet(em, 1, 0xA, 0, 0);
    return 1;
}

void cEm39::set2ndBattle()
{
    register Em39Work* w asm("r29"); // COMPILER-DIFF: #13 -- w kept live past its last store (see the asm below)
    Vec p = { 31259.0f, 5250.0f, -14068.0f };
    u32 zero;

    w = EM39_WK(this);
    AtariOff(&atari, 0xFCFF);
    rot.y = 1.4660766f;
    setPos(&p);
    zero = 0;
    w->x680 = 30;
    w->x8C4 = 3;
    w->x69C = zero;
    w->pGotoPoint = (EmiEntry*) zero;
    w->dmgTotal = zero;
    w->x698 = zero;
    EmRoutineSet(this, 1, 4, 0, 0);
    asm volatile("" : : "r"(zero), "r"(w)); // COMPILER-DIFF: #13 -- neither the zero nor w dies at its last store in the original (pure source order)
}

void cEm39::set1stDoorClear()
{
    Em39Work* w = EM39_WK(this);

    w->x69C = 0;
    w->x8C4 = 1;
}

void cEm39::set2ndDoorClear()
{
    Em39Work* w = EM39_WK(this);

    w->x69C = 0;
    w->x8C4 = 4;
}

// Motion-key voice request (seNo - 1) for the voice numbers the enemy owns.
void em39VoiceMove(cEm39* em)
{
    u32 v = em->seNo;

    if (v == 0) {
        return;
    }
    v--;
    switch (v) {
    case 6:
    case 9:
    case 0x19:
    case 0x1E:
    case 0x1F:
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x3E:
    case 0x3F:
        em->seNo = 0;
        em39SetVoice(em, (u8) v);
        break;
    }
}

void em39SetVoice(cEm39* em, int no)
{
    Em39Work* w = EM39_WK(em);
    cModel* p = em->getPartsPtr(4);

    SndStop(w->voiceId, 0);
    w->voiceId = SndCallI(8, no, &p->worldPos, em->id, 0, em);
    w->speechTime = 0;
}

void em39SetSpeech(cEm39* em, int no, int time)
{
    Em39Work* w = EM39_WK(em);

    w->speechTime = no;
    w->speechNo = time;
}

void em39SpeechMove(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    if (w->speechTime) {
        w->speechTime--;
        if (w->speechTime == 0) {
            em39SetVoice(em, w->speechNo);
        }
    }
}

// Slant (side-step) towards the player from mid range; the side alternates.
int em39SlantCk(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    Vec a;
    Vec b;
    int hit;

    if (em->type == 2) {
        return 0;
    }
    if (em->plDist2 > 144000000.0f) {
        return 0;
    }
    if (em->plDist2 < 12250000.0f) {
        return 0;
    }
    if (w->targetAngAbs > 0.5235988f) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) > 0.5235988f) {
        return 0;
    }
    if (fabsf(em->pos.y - pPL->pos.y) > 500.0f) {
        return 0;
    }
    a = em->pos;
    b = pPL->pos;
    a.y += 500.0f;
    b.y += 500.0f;
    hit = SatMgr.hitCheck(&a, &b, 0, 0, 0, 0);
    if (hit) {
        return 0;
    }
    w->slantSide ^= 1;
    em->xFC = 1;
    em->xFD = 0x10;
    em->xFE = hit;
    em->xFF = w->slantSide;
    return 1;
}

// Tower form: jump aside when the player aims from far enough (by difficulty / remaining hp).
int em39SlantCk2(cEm39* em)
{
    Em39Work* w = EM39_WK(em);

    if (em->type != 2) {
        return 0;
    }
    if (em->plDist2 < 6250000.0f) {
        return 0;
    }
    if (w->targetAngAbs > 0.5235988f) {
        return 0;
    }
    if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, PI)) > 0.5235988f) {
        return 0;
    }
    if (pG->x4F88 <= 1) {
        return 0;
    }
    if (pG->x4F88 <= 3 && (u8) (Rnd() % 10) > 4) {
        return 0;
    }
    if (pG->x4F88 <= 9 && em->hp > em->hpMax / 2 && (u8) (Rnd() % 10) > 6) {
        return 0;
    }
    EmRoutineSet(em, 1, 0x11, 0, 0);
    return 1;
}

// Guard check (tower form): the hit landed on the shield parts.
int em39GuardCk(cEm39* em)
{
    EmHitInfo* h;

    if (em->type != 2) {
        return 0;
    }
    h = em->dmPart;
    if (h->partsNo == 0xE) {
        return 1;
    }
    if (h->partsNo == 0xF) {
        return 1;
    }
    if (h->partsNo == 0x62) {
        return 1;
    }
    if (h->partsNo == 0x63) {
        return 1;
    }
    if (h->partsNo == 0x64) {
        return 1;
    }
    if (h->partsNo == 0x65) {
        return 1;
    }
    if (h->partsNo == 0x66) {
        return 1;
    }
    if (h->partsNo == 0x7B) {
        return 1;
    }
    if (h->partsNo == 0x7C) {
        return 1;
    }
    if (h->partsNo == 0x7D) {
        return 1;
    }
    if (h->partsNo == 0x7E) {
        return 1;
    }
    return h->partsNo == 0x7F;
}

// Motion-key foot sounds (seNo - 1) with the footstep dust in the tower room.
void em39FootEff(cEm39* em)
{
    u32 v = em->seNo;
    int no;

    if (v == 0) {
        return;
    }
    no = (u8) (v - 1);
    switch (no) {
    case 2:
        if ((em->stat & 0xFFFF0000) == 0x01090000) {
            EstSet(0, -1, &em->getPartsPtr(0x15)->worldPos, &em->rot, 0x2F, 0x2E, 0, 0, 0, 0);
        }
        if (pG->room_id != 0x31C) {
            no = 0x61;
        }
        break;
    case 3:
        if ((em->stat & 0xFFFF0000) == 0x01090000) {
            EstSet(0, -1, &em->getPartsPtr(0x19)->worldPos, &em->rot, 0x2F, 0x2E, 0, 0, 0, 0);
        }
        if (pG->room_id != 0x31C) {
            no = 0x62;
        }
        break;
    case 0:
        if (pG->room_id != 0x31C) {
            no = 0x5F;
        }
        break;
    case 1:
        if (pG->room_id != 0x31C) {
            no = 0x60;
        }
        break;
    case 0xE:
        if (pG->room_id != 0x31C) {
            no = 0x63;
        }
        break;
    case 0x39:
        if (pG->room_id != 0x31C) {
            no = 0x64;
        }
        break;
    default:
        return;
    }
    em->seNo = 0;
    SndCallI(8, no, &em->getPartsPtr(0)->worldPos, em->id, 0, pPL);
}

// Motion-key player voice (seNo - 1 = 0x43 / 0x44 / 0x54), Ashley's numbers in her chapter.
void em39PLVoiceCk(cEm39* em)
{
    u32 v = em->seNo;
    int no;

    if (v == 0) {
        return;
    }
    no = (u8) (v - 1);
    switch (no) {
    case 0x43:
        if (pG->x4FB8 == 2) {
            no = 0x55;
        }
        break;
    case 0x44:
        if (pG->x4FB8 == 2) {
            no = 0x57;
        }
        break;
    case 0x54:
        if (pG->x4FB8 == 2) {
            no = 0x56;
        }
        break;
    default:
        return;
    }
    em->seNo = 0;
    SndCallI(8, no, &pPL->getPartsPtr(4)->worldPos, em->id, 0, pPL);
}

int cEm39::ckHide()
{
    if (EM39_WK(this)->flags & 0x400) {
        return 1;
    }
    return 0;
}

// Tower form left arm attack: the arm parts origins and points along the forearm (parts 0x63).
void em39LeftArmAtkCk(cEm39* em, int no)
{
    Vec a;
    Vec b = em->pos;
    cModel* p;

    p = em->getPartsPtr(0x10);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    p = em->getPartsPtr(0x5F);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    p = em->getPartsPtr(0x60);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    p = em->getPartsPtr(0x61);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    p = em->getPartsPtr(0x62);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    p = em->getPartsPtr(0x63);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 100.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 200.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 300.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 400.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 500.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 600.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 700.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
    a.x = 800.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    PSMTXMultVec(p->mat, &a, &a);
    em39AtkCk2(em, no, &a, &b);
}

// The closest EMI type 0xD point to the player within 2000: the cliff-attack spot.
int em39GetCliffPos(cEm39* em)
{
    Em39Work* w = EM39_WK(em);
    EmiData* emi = EM39_EMI;
    EmiEntry* best;
    f32 bestDist;
    u32 i;

    if (emi == 0) {
        return 0;
    }
    bestDist = 4000000.0f;
    best = 0;
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];
        f32 d;

        if (e->type != 0xD) {
            continue;
        }
        d = (pPL->pos.x - e->pos.x) * (pPL->pos.x - e->pos.x) + (pPL->pos.z - e->pos.z) * (pPL->pos.z - e->pos.z);
        if (d > bestDist) {
            continue;
        }
        bestDist = d;
        best = e;
    }
    if (best == 0) {
        return 0;
    }
    w->jumpPos = best->pos;
    w->jumpAng = best->rotY;
    return 1;
}

void cEm39::setDie()
{
    EmRoutineSet(this, 3, 0, 0, 0);
}

void cEm39::setDieCancel()
{
    AtariOff(&atari, 0xFCFF);
    pos.x = 9210.38f;
    pos.y = 12050.0f;
    pos.z = -14974.03f;
    rot.y = 1.57f;
    EmRoutineSet(this, 3, 0, 2, 0);
}

int cEm39::ckTalk1st()
{
    Em39Work* w = EM39_WK(this);

    if (w->flags & 0x80000) {
        return 0;
    }
    if (w->flags & 0x40000) {
        return 1;
    }
    return 0;
}

void cEm39::setTalk1st()
{
    Em39Work* w = EM39_WK(this);

    SndStop(w->voiceId, 0);
    dmType = 2;
    w->flags |= 0x80000;
    EmRoutineSet(this, 1, 0, 0, 0);
}

void cEm39::setTalk1stCancel()
{
    AtariOn(&atari, 0x300);
    if ((u8) (Rnd() % 10) > 4 && plDist2 > 25000000.0f) {
        EmRoutineSet(this, 1, 0x1D, 0, 1);
    } else {
        EmRoutineSet(this, 1, 0x23, 0, 1);
    }
}

int cEm39::ckTalk2nd()
{
    Em39Work* w = EM39_WK(this);

    if (w->flags & 0x200000) {
        return 0;
    }
    if (w->flags & 0x100000) {
        return 1;
    }
    return 0;
}

void cEm39::setTalk2nd()
{
    Em39Work* w = EM39_WK(this);

    SndStop(w->voiceId, 0);
    dmType = 2;
    w->flags |= 0x200000;
    EmRoutineSet(this, 1, 1, 0, 0);
}

void cEm39::setTalk2ndCancel()
{
    EM39_WK(this)->x8C4 = 5;
    EmRoutineSet(this, 1, 0x26, 0, 0);
}

int cEm39::ckBombCutEnable()
{
    if (EM39_WK(this)->flags & 0x800000) {
        return 1;
    }
    return 0;
}

// The split object's .data is 8-aligned (4 pad bytes before the ngcld BSS tag).
asm(".section .data\n\t.balign 8\n\t.text");
