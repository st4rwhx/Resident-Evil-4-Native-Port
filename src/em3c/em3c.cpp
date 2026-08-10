// em3c module (D:/Bio4/Prog/em3c.cpp): a humanoid enemy in four variants (cModel::type 0..3; types
// 1 / 3 use a second motion set, types 2 / 3 carry a parasite head object that bites on its own). It
// walks / runs at the player (em3cRouteCk), grabs (em3c_R1_AtkWait, the player escapes with the action
// button: plemEscape*), kicks (em3c_R1_MoveAtk) and bursts its head on big damage (em3cPartsBombHead:
// the head parts fall apart as five-point cloth pieces, em3cPartsBombControl).

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "em3c.h"
#include "em10.h"
#include "emhit.h"
#include "emdoor.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "obj16.h"
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
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "dbmodule.h"
#include "db_log.h"

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// The module's 0x34-byte COMMON block: uninitialised template statics of the original object,
// merged into .bss by the REL link.
asm(".comm common_em3c,52,4");

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
// em_set.h declares EmSetDieCnt without arguments; this module passes the enemy.
void EmSetDieCntE(cEm* em) asm("EmSetDieCnt");

typedef void (*Em3cFunc)(cEm3c*);

static void em3c_R0_Init(cEm3c* em);
static void em3c_R0_Move(cEm3c* em);
static void em3c_R1_StartWait(cEm3c* em);
static void em3c_R1_AtkWait(cEm3c* em);
static void plemSurprised(cPlayer* pl);
static void plemEscapeAction(cEm3c* em);
static void plemEscape(cPlayer* pl);
static void subemSurprised();
static void subemSit();
static void em3c_R1_Wait(cEm3c* em);
static void em3c_R1_Walk(cEm3c* em);
static void em3c_R1_Run(cEm3c* em);
static void em3c_R1_Turn180(cEm3c* em);
static void em3c_R1_MoveAtk(cEm3c* em);
static void em3c_R1_CoreAtk(cEm3c* em);
static void plemDmMStar(cPlayer* pl);
static void em3c_R0_Damage(cEm3c* em);
static void em3c_R1_Dm_Normal(cEm3c* em);
static void em3c_R1_Dm_Big(cEm3c* em);
static void em3c_R1_Dm_Head(cEm3c* em);
static void em3c_R0_Die(cEm3c* em);
static void em3c_R1_Die_Normal(cEm3c* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)
#define PL_ARC(no) PL_ARC_PTR(pl->subArc, no)
#define SUB_ARC(no) PL_ARC_PTR(sub->subArc, no)

// The enemy a player / partner damage callback belongs to (pl_sub SetPlDamage's first argument).
#define PL_EM(pl) ((cEm*) (pl)->dmgType)

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

// Dead flag test (cDmgInfo upper 16 bits): an inline returning 0/1 gives the `li 1; andis.; bne; li 0` chain.
static inline int em3cDeadCk(cEm* em)
{
    return (em->flags_324 & 0xFFFF0000) ? 1 : 0;
}

extern "C" void _prolog()
{
    OSReport("em3c prolog Ok\n");
    EmInitFunc = Em3cInit;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em3cInit(cEm* em)
{
    new (em) cEm3c();
}

void em3cDmCk(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    EmHitInfo* part;
    int near;
    int kind;
    int dmg;

    if (em->dmHit == 0) {
        return;
    }
    em->dmHit = 0;
    if (em->dmWep == 0x14 || em->dmWep == 0x16) {
        return;
    }
    part = em->dmPart;
    near = 0;
    if (part->rad < 36000000.0f) {
        near = 1;
    }
    em->dmType = 1;
    if (em->dmWep == 0x10) {
        em->dmType = 0x11;
    }
    w->flags |= 0x80;
    kind = part->partsNo == 3;
    if (part->partsNo == 5) {
        kind = 2;
    }
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
    case 0x28:
    case 0x2B:
    case 0x2C:
    default:
        switch ((u32) kind) {
        case 0:
        default:
            SndCall(8, 0xE, &em->pos, em->id, 0, em);
            EmDmBloodSet2(em, 0x31, 0, 0, 0, 0);
            break;
        case 1:
            SndCall(8, 0x38, &em->pos, em->id, 0, em);
            EmDmBloodSet2(em, 0x31, 0x14, 0, 0, 0);
            break;
        case 2:
            SndCall(8, 0xF, &em->pos, em->id, 0, em);
            EmDmBloodSet2(em, 0x31, 0, 0, 0, 0);
            break;
        }
        break;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
        switch ((u32) kind) {
        case 0:
        default:
            SndCall(8, 0xE, &em->pos, em->id, 0, em);
            EmDmBloodSet2(em, 0x31, 1, 0, 0, 0);
            break;
        case 1:
            SndCall(8, 0x38, &em->pos, em->id, 0, em);
            EmDmBloodSet2(em, 0x31, 0x14, 0, 0, 0);
            break;
        case 2:
            SndCall(8, 0xF, &em->pos, em->id, 0, em);
            EmDmBloodSet2(em, 0x31, 1, 0, 0, 0);
            break;
        }
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            switch ((u32) kind) {
            case 0:
            default:
                SndCall(8, 0xE, &em->pos, em->id, 0, em);
                EmDmBloodSet2(em, 0x31, 1, 0, 0, 0);
                break;
            case 1:
                SndCall(8, 0x38, &em->pos, em->id, 0, em);
                EmDmBloodSet2(em, 0x31, 0x14, 0, 0, 0);
                break;
            case 2:
                SndCall(8, 0xF, &em->pos, em->id, 0, em);
                EmDmBloodSet2(em, 0x31, 1, 0, 0, 0);
                break;
            }
        } else {
            switch ((u32) kind) {
            case 0:
            default:
                SndCall(8, 0xE, &em->pos, em->id, 0, em);
                EmDmBloodSet2(em, 0x31, 0, 0, 0, 0);
                break;
            case 1:
                SndCall(8, 0x38, &em->pos, em->id, 0, em);
                EmDmBloodSet2(em, 0x31, 0x14, 0, 0, 0);
                break;
            case 2:
                SndCall(8, 0xF, &em->pos, em->id, 0, em);
                EmDmBloodSet2(em, 0x31, 0, 0, 0, 0);
                break;
            }
        }
        break;
    case 0x17:
    case 0x2A:
        break;
    }
    if (w->flags & 0x400) {
        return;
    }
    dmg = em3cSetDmVal(em);
    LifeDownSet(em, dmg, 0);
    if (w->pParasite && em->dmWep == 0x17) {
        em->hp = 0;
    }
    if (w->pParasite && em->dmWep == 0x2A) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        if (w->pParasite) {
            SndCall(8, 0x3D, &em->pos, em->id, 0, em);
            switch (em->type) {
            case 0:
            case 1:
            default:
                EstSet((int) em, -1, 0, 0, 0x31, 0x15, 0, 0, (u32) em, 0);
                break;
            case 2:
            case 3:
                EstSet(0, -1, &w->pParasite->getPartsPtr(9)->worldPos, 0, 0x31, 0x26, 0, 0, 0, 0);
                break;
            }
        }
        EmSetDie(em);
        EmSetDieCntE(em);
        EmRoutineSet(em, 3, 0, 0, 0);
    } else {
        if (w->flags & 0x200) {
            return;
        }
        if (w->flags & 0x100) {
            return;
        }
        if (part->partsNo == 5) {
            if (w->pParasite) {
                SndCall(8, 0x37, &em->pos, em->id, 0, em);
            }
            w->hitCnt -= dmg;
            w->bigHitCnt--;
            if (w->hitCnt <= 0) {
                EmRoutineSet(em, 2, 2, 0, 0);
                return;
            }
            if (w->bigHitCnt <= 0) {
                if (w->flags & 0x200) {
                    return;
                }
                w->bigHitCnt = Rnd() % 3 + 1;
                EmRoutineSet(em, 2, 2, 0, 0);
            }
        } else {
            if (em->hp < em->hpMax / 2 && !(w->flags & 0x800)) {
                EmRoutineSet(em, 2, 1, 0, 0);
                return;
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
            case 0x1B:
            case 0x1D:
            case 0x26:
            case 0x27:
            case 0x28:
            case 0x2B:
                break;
            case 5:
            case 6:
            case 0xD:
            case 0xF:
            case 0x12:
            case 0x13:
            case 0x29:
            case 0x2A:
            case 0x2C:
            case 0x2D:
            default:
                if (Rnd() & 3) {
                    EmRoutineSet(em, 2, 1, 0, 0);
                } else {
                    EmRoutineSet(em, 2, 0, 0, 0);
                }
                break;
            case 7:
            case 8:
            case 0x21:
                if (near == 0) {
                    return;
                }
                if ((u8) (Rnd() % 10) > 4) {
                    EmRoutineSet(em, 2, 0, 0, 0);
                }
                break;
            }
        }
    }
}

Em3cFunc Em3c_R0_move_tbl[4] = {
    em3c_R0_Init,
    em3c_R0_Move,
    em3c_R0_Damage,
    em3c_R0_Die,
};

static Em3cFunc Em3c_R1_move_tbl[8] = {
    em3c_R1_StartWait,
    em3c_R1_AtkWait,
    em3c_R1_Wait,
    em3c_R1_Walk,
    em3c_R1_Run,
    em3c_R1_Turn180,
    em3c_R1_MoveAtk,
    em3c_R1_CoreAtk,
};

static Em3cFunc Em3c_R2_move_tbl[3] = {
    em3c_R1_Dm_Normal,
    em3c_R1_Dm_Big,
    em3c_R1_Dm_Head,
};

static Em3cFunc Em3c_R3_move_tbl[1] = {
    em3c_R1_Die_Normal,
};

// Attacks (em3cAtkCk): [0] grab, [1] kick, [2] the parasite bite.
static EmAtkInfo em3c_atk_tbl[3] = {
    { 400.0f, 8, 0x30C, 0, 0xA, 0 },
    { 400.0f, 8, 0x17C, 0, 0xA, 0 },
    { 1000.0f, 8, 0x30C, 0, 0xA, 0 },
};

// Head parts that fall apart (em3cPartsBombSet) and the frames each waits before it starts.
static u8 em3c_bomb_parts[25] = {
    0x01, 0x03, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x16, 0x17, 0x18, 0x1B, 0x1C, 0x1D, 0x1E,
};
static u16 em3c_bomb_time[25] = {
    3, 2, 6, 5, 4, 3, 2, 1, 6, 5, 4, 3, 2, 1, 4, 5, 6, 7, 4, 5, 6, 5, 5, 5, 4,
};
// Rest distances between the five points of each piece kind (em3c_R0_Init fills it).
static f32 em3c_bomb_dist[5][5][5] = { 0.0f };
// The five corner points of each piece kind in parts space.
Vec em3c_bomb_pt[5][5] = {
    { { 0.0f, 0.0f, 300.0f }, { 0.0f, 0.0f, -300.0f }, { 300.0f, 0.0f, 0.0f }, { -300.0f, 0.0f, 0.0f }, { 0.0f, 300.0f, 0.0f } },
    { { 0.0f, 0.0f, 300.0f }, { 0.0f, 0.0f, -300.0f }, { 300.0f, 0.0f, 0.0f }, { -300.0f, 0.0f, 0.0f }, { 0.0f, 600.0f, 0.0f } },
    { { 0.0f, 0.0f, 200.0f }, { 0.0f, 0.0f, -200.0f }, { 200.0f, 0.0f, 0.0f }, { -200.0f, 0.0f, 0.0f }, { 0.0f, -500.0f, 0.0f } },
    { { 0.0f, 0.0f, 200.0f }, { 0.0f, 0.0f, -200.0f }, { 200.0f, 0.0f, 0.0f }, { -200.0f, 0.0f, 0.0f }, { 0.0f, -600.0f, 0.0f } },
    { { 0.0f, 0.0f, 2000.0f }, { 0.0f, 0.0f, -500.0f }, { 200.0f, 0.0f, 0.0f }, { -200.0f, 0.0f, 0.0f }, { 0.0f, 300.0f, 0.0f } },
};
// The original link 8-aligns the end of .data (the ngcld BSS tag follows): the 4 pad bytes after the table.
asm(".section .data\n\t.balign 8\n\t.text");

void cEm3c::move()
{
    Em3cWork* w = EM3C_WK(this);
    f32 len;

    if (xFC) {
        em3cDmCk(this);
    }
    w->flags &= ~0x707;
    if (w->timer74) {
        w->timer74--;
    }
    if (w->atkWait) {
        w->atkWait--;
    }
    if (w->waitTimer) {
        w->waitTimer--;
    }
    if (em3cDeadCk(pPL)) {
        if (w->atkWait <= 4) {
            w->atkWait = 5;
        }
    }
    em3cRouteCk(this);
    Em3c_R0_move_tbl[xFC](this);
    partsWorldCalc();
    len = SQRTF((oldPos.x - pos.x) * (oldPos.x - pos.x) + (oldPos.z - pos.z) * (oldPos.z - pos.z));
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    if (SQRTF((pos.x - oldPos.x) * (pos.x - oldPos.x) + (pos.z - oldPos.z) * (pos.z - oldPos.z)) < len * 0.5f) {
        w->stuckCnt++;
    } else {
        w->stuckCnt = 0;
    }
    em3cPartsBombControl(this);
    if (w->bombTimer) {
        w->bombTimer--;
        if (w->bombTimer == 0) {
            cModel* p;

            p = getPartsPtr(3);
            p->scale.x = 0.0f;
            p->scale.y = 0.0f;
            p->scale.z = 0.0f;
            p = getPartsPtr(4);
            p->scale.x = 0.0f;
            p->scale.y = 0.0f;
            p->scale.z = 0.0f;
        }
    }
    em3cFootSe(this);
    if (w->pParasite) {
        if (w->voiceTimer) {
            w->voiceTimer--;
        } else if (hp > 0) {
            w->voiceTimer = 30;
            switch (type) {
            case 0:
            case 1:
            default:
                SndCall(8, 0x3A, &pos, id, 0, this);
                break;
            case 2:
            case 3:
                SndCall(8, 0x30, &pos, id, 0, this);
                break;
            }
        }
        if (w->pParasite && w->pParasite->pParts && (w->pParasite->be_flag & 0x201) == 1) {
            Mtx inv;
            Vec v;
            cModel* p;

            PSMTXInverse(getPartsPtr(2)->mat, inv);
            switch (type) {
            case 0:
            case 1:
            default:
                p = w->pParasite->getPartsPtr(0x15);
                break;
            case 2:
            case 3:
                p = w->pParasite->getPartsPtr(9);
                break;
            }
            PSMTXMultVec(inv, &p->worldPos, &v);
            w->hit[10].ofs = v;
            w->hit[10].flags |= 1;
        } else {
            w->hit[10].flags &= ~1;
        }
    } else {
        w->hit[10].flags &= ~1;
    }
}

static void em3c_R0_Init(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    int zero = 0;
    u32 i;
    u32 j;
    u32 k;
    u8 no;

    em3cModelInit(em);
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 1000.0f, 1000.0f, 0.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    atariInitF(&em->atari, 0.0f, -900.0f, 0.0f, 550.0f, 450.0f, 450.0f, 1800.0f, 1, 0x2000, 10);   // COMPILER-DIFF: #1
    YarareInit(em, 0.0f, 0.0f, 0.0f, 200.0f, 250.0f, 2, 1);
    YarareAdd(em, &w->hit[0], 0.0f, 0.0f, 0.0f, 150.0f, 100.0f, 5, 1);
    YarareAdd(em, &w->hit[1], -300.0f, 0.0f, 0.0f, 120.0f, 300.0f, 8, 3);
    YarareAdd(em, &w->hit[2], -300.0f, 0.0f, 0.0f, 100.0f, 300.0f, 9, 3);
    YarareAdd(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 120.0f, 300.0f, 0xE, 3);
    YarareAdd(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 100.0f, 300.0f, 0xF, 3);
    YarareAdd(em, &w->hit[5], 0.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x13, 1);
    YarareAdd(em, &w->hit[6], 0.0f, -400.0f, 0.0f, 130.0f, 400.0f, 0x14, 1);
    YarareAdd(em, &w->hit[7], 0.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x17, 1);
    YarareAdd(em, &w->hit[8], 0.0f, -400.0f, 0.0f, 130.0f, 400.0f, 0x18, 1);
    YarareAdd(em, &w->hit[9], 0.0f, 300.0f, 0.0f, 150.0f, 50.0f, 3, 0);
    YarareAdd(em, &w->hit[10], 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 3, 0);
    EspDataLoad((u32) ARC(4), 0x31, 0);
    w->flags = zero;
    w->voiceTimer = 60;
    w->waitTimer = 300;
    w->bombTimer = zero;
    w->timer74 = zero;
    w->atkWait = zero;
    w->startPos = em->pos;
    w->startRot = em->rot;
    w->hitCnt = (s16) (em->hpMax / 14) + Rnd() % 25 + 1;
    w->bigHitCnt = Rnd() % 3 + 1;
    if (em->type != 1 && em->type != 3) {
        w->female = zero;
    } else {
        w->female = 1;
    }
    switch (em->type) {
    default:
        w->type23 = 0;
        break;
    case 2:
    case 3:
        w->type23 = 1;
        break;
    }
    w->espKind = EspPullCoreKind();
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {
            for (k = 0; k < 5; k++) {
                if (j == k) {
                    em3c_bomb_dist[i][j][k] = 0.0f;
                } else {
                    em3c_bomb_dist[i][j][k] = GetDistance3(&em3c_bomb_pt[i][j], &em3c_bomb_pt[i][k]);
                }
            }
        }
    }
    no = em->x38D;
    switch (no) {
    case 0:
    default:
        em->setStatus(5);
        EmRoutineSet(em, 1, 2, 0, 0);
        break;
    case 1:
        em->setStatus(0xB);
        em->atari.throughOn();
        w->flags |= 0x400;
        EmRoutineSet(em, 1, 0, 0, 0);
        break;
    case 2:
        em->setStatus(0xB);
        em->atari.setPriority(2);
        em->atari.clrFlag100();
        em->be_flag |= 0x10000;
        w->flags |= 0x400;
        EmRoutineSet(em, 1, 1, 0, 0);
        break;
    }
    if (w->female) {
        MotionSetCore(em, MOTION(em), ARC(0x2A), 0, 0, 1, 0);
    } else {
        MotionSetCore(em, MOTION(em), ARC(0x10), 0, 0, 1, 0);
    }
    MotionMoveF(em, 0);
    em3c_R0_Move(em);
}

static void em3c_R0_Move(cEm3c* em)
{
    Em3c_R1_move_tbl[em->xFD](em);
}

static void em3c_R1_StartWait(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    w->flags |= 0x400;
    switch (em->xFE) {
    case 0:
        em->hp = 1000;
        em->xFE++;
    case 1:
        if (w->female) {
            MotionSetCore(em, MOTION(em), ARC(0x33), (int) ARC(0x34), 0, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x19), (int) ARC(0x1A), 0, 1, 0);
        }
        MotionMoveF(em, 0);
        if (!(em->flags_3C8 & 1)) {
            break;
        }
        em->clearStatus(0xB);
        em->xFE++;
    case 2:
        em->setStatus(5);
        em->hp = em->hpMax;
        w->flags |= 0x80;
        em->atari.throughOff();
        if (w->female) {
            MotionSetCore(em, MOTION(em), ARC(0x33), (int) ARC(0x34), 0, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x19), (int) ARC(0x1A), 0, 1, 0);
        }
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if (w->targetAngAbs > 2.0943952f) {
                EmRoutineSet(em, 1, 5, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        }
        break;
    }
}

static void em3c_R1_AtkWait(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    w->flags |= 0x400;
    switch (em->xFE) {
    case 0:
        if (w->female) {
            MotionSetCore(em, MOTION(em), ARC(0x47), 0, 0, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x45), 0, 0, 5, 0);
        }
        w->actMode = 0;
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        if (em->plDist2 > 9000000.0f) {
            break;
        }
        if ((s16) pG->pl_life <= 0) {
            break;
        }
        em->xFE++;
    case 2:
        em->hp = 0;
        EmSetDie(em);
        em->atari.throughOn();
        SetPlDamage((int) em, plemSurprised);
        if (pSUB) {
            f32 d = (em->pos.x - pSUB->pos.x) * (em->pos.x - pSUB->pos.x) + (em->pos.y - pSUB->pos.y) * (em->pos.y - pSUB->pos.y)
                    + (em->pos.z - pSUB->pos.z) * (em->pos.z - pSUB->pos.z);

            if (pSUB->hp > 0 && d < 25000000.0f) {
                SetSubDamage((int) em, (void*) subemSurprised);
            }
        }
        em3cAtkSuspend(em, 1);
        w->timer = 30;
        em->xFE++;
    case 3:
        MotionMoveF(em, 0);
        if (w->timer) {
            w->timer--;
            if (w->timer == 0) {
                w->actMode = (Rnd() & 1) + 1;
            }
            break;
        }
        em->xFE++;
        break;
    case 4:
        if (w->female) {
            MotionSetCore(em, MOTION(em), ARC(0x2F), (int) ARC(0x30), 0, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x31, 0x23, 0, 0, (u32) em, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x15), (int) ARC(0x16), 0, 1, 0);
            EstSet((int) em, -1, 0, 0, 0x31, 0x20, 0, 0, (u32) em, 0);
        }
        w->timer = 60;
        w->escaped = 0;
        em->xFE++;
    case 5:
        MotionMoveF(em, 0);
        em3cAtkCk2(em, 2);
        if (em->seFlags28B & 1) {
            w->actMode = 0;
        }
        if (w->atkHit) {
            w->escaped = 1;
            em->flags_3C8 |= 2;
        }
        if (em->seFlags28B & 4) {
            Vec v;

            if (w->female) {
                v.x = 100.0f;
                v.y = 0.0f;
                v.z = 1000.0f;
            } else {
                v.x = 100.0f;
                v.y = 0.0f;
                v.z = 1150.0f;
            }
            PSMTXMultVec(em->mat, &v, &v);
            EstSet(0, -1, &v, &em->rot, 0x31, 2, 0, 0, 0, 0);
        }
        if (w->timer) {
            w->timer--;
        } else {
            em3cAtkSuspend(em, 0);
            EmRoutineSet(em, 3, 0, 0, 1);
        }
        break;
    }
    if (w->escaped == 0) {
        switch (w->actMode) {
        case 1:
            ActBtn.set(0x25, 5, (int) plemEscapeAction, (int) em, 0x42, 3, 0, 0);
            break;
        case 2:
            ActBtn.set(0x25, 5, (int) plemEscapeAction, (int) em, 0x42, 4, 0, 0);
            break;
        }
    }
}

static void plemSurprised(cPlayer* pl)
{
    pl->subArc = PL_EM(pl)->subArc;
    switch (pl->xFE) {
    case 0: {
        Vec v;

        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1800.0f;
        PSMTXMultVec(PL_EM(pl)->mat, &v, &pl->pos);
        pl->rot.y = GetXZAngle(&pl->pos, &PL_EM(pl)->pos);
        pl->atari.throughOn();
        if (pGS->x4FB8 == 1) {
            MotionSetCore(pl, MOTION(pl), PL_ARC(0x64), 0, 3, 1, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC(0x63), 0, 3, 1, 0);
        }
        pl->x3E0 = 5;
        pl->xFE++;
    }
    case 1:
        if (pl->x3E0) {
            pl->x3E0--;
        } else {
            pl->atari.throughOff();
        }
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void plemEscapeAction(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    w->actMode = 0;
    w->escaped = 1;
    pPLS->dmType = 2;
    SetPlDamage((int) em, plemEscape);
    if (pSUB) {
        f32 d = (em->pos.x - pSUB->pos.x) * (em->pos.x - pSUB->pos.x) + (em->pos.y - pSUB->pos.y) * (em->pos.y - pSUB->pos.y)
                + (em->pos.z - pSUB->pos.z) * (em->pos.z - pSUB->pos.z);

        if (pSUB->hp > 0 && d < 16000000.0f) {
            SetSubDamage((int) em, (void*) subemSit);
        }
    }
    GameAddPoint(9);
}

static void plemEscape(cPlayer* pl)
{
    pl->subArc = PL_EM(pl)->subArc;
    pl->dmType = 2;
    switch (pl->xFE) {
    case 0:
        if (pG->x4FB8 == 1) {
            MotionSetCore(pl, MOTION(pl), PL_ARC(0x65), 0, 3, 1, 0);
            EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, 0);
            SndCall(1, 5, &pl->pos, pl->id, 0, pl);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC(0x60), 0, 3, 1, 0);
            EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, 0);
            SndCall(1, 0x43, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
            SndCall(1, 0x44, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        }
        pl->atari.throughOff();
        pl->xFE++;
    case 1:
        if (pG->x4FB8 == 1) {
            if (pl->frame > 24.7f && pl->frame < 25.3f) {
                SndCall(5, 5, &pl->pos, 0, 0, pl);
            }
            if (pl->frame > 60.7f && pl->frame < 61.3f) {
                SndCall(1, 6, &pl->pos, pl->id, 0, pl);
                SndCall(1, 0x12, &pl->pos, pl->id, 0, pl);
            }
        } else {
            if (pl->frame > 10.7f && pl->frame < 11.3f) {
                SndCall(1, 0x4F, &pl->pos, 0, 0, pl);
            }
            if (pl->frame > 21.7f && pl->frame < 22.3f) {
                SndCall(5, 0x14, &pl->pos, 0, 0, pl);
            }
            if ((pl->frame > 36.7f && pl->frame < 37.3f) || (pl->frame > 49.7f && pl->frame < 50.3f)) {
                SndCall(5, 2, &pl->pos, 0, 0, pl);
            }
            if ((pl->frame > 37.7f && pl->frame < 38.3f) || (pl->frame > 50.7f && pl->frame < 51.3f)) {
                SndCall(5, 3, &pl->pos, 0, 0, pl);
            }
        }
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void subemSurprised()
{
    cSubChar* sub = pSUB;

    sub->subArc = PL_EM(sub)->subArc;
    sub->dmType = 2;
    switch (sub->xFE) {
    case 0: {
        Vec v;

        v.x = 400.0f;
        v.y = 0.0f;
        v.z = 2100.0f;
        PSMTXMultVec(PL_EM(sub)->mat, &v, &sub->pos);
        sub->rot.y = GetXZAngle(&sub->pos, &PL_EM(sub)->pos);
        sub->rot.y += -0.17453292f;
        sub->rot.y = LIMIT_ANGLE(sub->rot.y);
        MotionSetCore(sub, MOTION(sub), SUB_ARC(0x64), 0, 3, 0x101, 0);
        sub->atari.throughOn();
        sub->subHideMode = 50;
        sub->xFE++;
    }
    case 1:
        MotionMoveF(sub, 0);
        if (!(PL_EM(sub)->flags_3C8 & 2) && sub->subHideMode) {
            sub->subHideMode--;
        } else {
            sub->xFE++;
        }
        break;
    case 2:
        sub->atari.throughOff();
        MotionSetCore(sub, MOTION(sub), SUB_ARC(0x65), 0, 3, 0x101, 0);
        EstSet((int) sub, -1, 0, 0, 3, 0x14, 0, 0, (u32) sub, 0);
        SndCall(8, 4, &sub->pos, sub->id, 0, sub);
        sub->xFE++;
    case 3:
        if (sub->frame > 24.7f && sub->frame < 25.3f) {
            SndCall(5, 5, &sub->pos, 0, 0, sub);
        }
        if (MotionMoveF(sub, 0)) {
            EndSubDamage();
        }
        break;
    }
    sub->subArc = sub->subArc2;
}

static void subemSit()
{
    cSubChar* sub = pSUB;

    sub->dmType = 2;
    switch (sub->xFE) {
    case 0:
        sub->atari.throughOff();
        MotionSetCore(sub, MOTION(sub), SUB_ARC(0x43), 0, 3, 1, 0);
        SndCall(8, 4, &sub->pos, sub->id, 0, sub);
        sub->xFE++;
    case 1:
        if (MotionMoveF(sub, 0)) {
            sub->xFE++;
        }
        break;
    case 2:
        MotionSetCore(sub, MOTION(sub), SUB_ARC(0x44), 0, 3, 1, 0);
        sub->xFE++;
    case 3:
        if (MotionMoveF(sub, 0) && sub->xFF == 0) {
            sub->xFE++;
        }
        break;
    case 4:
        MotionSetCore(sub, MOTION(sub), SUB_ARC(0x45), 0, 3, 1, 0);
        sub->xFE++;
    case 5:
        if (MotionMoveF(sub, 0)) {
            EndSubDamage();
        }
        break;
    }
}

static void em3c_R1_Wait(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    switch (em->xFE) {
    case 0:
        if (w->female) {
            MotionSetCore(em, MOTION(em), ARC(0x2A), 0, 10, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x10), 0, 10, 1, 0);
        }
        em->xFE++;
    case 1:
        MotionMoveF(em, 0);
        em3cFindCk(em);
        if ((w->flags & 0x80) && em3cStayCk(em) == 0 && w->atkWait == 0) {
            if (w->targetAngAbs > 2.0943952f) {
                EmRoutineSet(em, 1, 5, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        }
        break;
    }
}

static void em3c_R1_Walk(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    switch (em->xFE) {
    case 0:
        if (w->female) {
            u16 fr = (u32) ((f32) (((MotionData*) ARC(0x2B))->maxFrame & 0x3FFF) * (f32) em->xFF / 256.0f);
            MotionSetCore(em, MOTION(em), ARC(0x2B), (int) ARC(0x2C), 10, 5, fr);
        } else {
            u16 fr = (u32) ((f32) (((MotionData*) ARC(0x11))->maxFrame & 0x3FFF) * (f32) em->xFF / 256.0f);
            MotionSetCore(em, MOTION(em), ARC(0x11), (int) ARC(0x12), 10, 5, fr);
        }
        em->xFE++;
    case 1:
        if (em->x29D == 0) {
            em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, PI / 64.0f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0) && em3cStayCk(em)) {
            EmRoutineSet(em, 1, 2, 0, 0);
            break;
        }
        if (w->atkWait && em->plDist2 < 6250000.0f) {
            EmRoutineSet(em, 1, 2, 0, 0);
            break;
        }
        {
            Vec pl;
            f32 d;

            GetPlPos(&pl, 0, 18.0f);
            d = (em->pos.x - pl.x) * (em->pos.x - pl.x) + (em->pos.z - pl.z) * (em->pos.z - pl.z);
            if (fabsf(Muku(&em->pos, &pl, em->rot.y, PI)) < PI / 2.0f && d < 4000000.0f) {
                switch (em->type) {
                case 0:
                case 1:
                default:
                    EmRoutineSet(em, 1, 6, 0, 0);
                    break;
                case 2:
                case 3:
                    if (w->pParasite && (Rnd() & 1) && w->pParasite->ckAtkEnable()) {
                        EmRoutineSet(em, 1, 7, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 6, 0, 0);
                    }
                    break;
                }
            } else if (w->targetAngAbs > 2.0943952f) {
                EmRoutineSet(em, 1, 5, 0, 0);
            } else if (pG->x4F88 > 9) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->plDist > 7000.0f && w->waitTimer == 0 && pG->x4F88 > 1) {
                if ((u8) (Rnd() % 10) > 4) {
                    w->waitTimer = 150;
                } else {
                    EmRoutineSet(em, 1, 4, 0, 0);
                }
            }
        }
        break;
    }
    em3cDoorOpenCk(em);
}

static void em3c_R1_Run(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    switch (em->xFE) {
    case 0:
        if (w->female) {
            u16 fr = (u32) ((f32) (((MotionData*) ARC(0x2D))->maxFrame & 0x3FFF) * (f32) em->xFF / 256.0f);
            MotionSetCore(em, MOTION(em), ARC(0x2D), (int) ARC(0x2E), 5, 5, fr);
        } else {
            u16 fr = (u32) ((f32) (((MotionData*) ARC(0x13))->maxFrame & 0x3FFF) * (f32) em->xFF / 256.0f);
            MotionSetCore(em, MOTION(em), ARC(0x13), (int) ARC(0x14), 5, 5, fr);
        }
        em->xFE++;
    case 1:
        if (em->x29D == 0) {
            em->rot.y += Muku(&em->pos, &w->targetPos, em->rot.y, PI / 48.0f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (em->plDist2 < 9000000.0f && (w->flags & 1)) {
                EmRoutineSet(em, 1, 3, 0, 0);
                break;
            }
            if (em3cStayCk(em)) {
                EmRoutineSet(em, 1, 2, 0, 0);
                break;
            }
        }
        if (w->atkWait && em->plDist2 < 6250000.0f) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        {
            Vec pl;
            f32 d;

            GetPlPos(&pl, 0, 18.0f);
            d = (em->pos.x - pl.x) * (em->pos.x - pl.x) + (em->pos.z - pl.z) * (em->pos.z - pl.z);
            if (fabsf(Muku(&em->pos, &pl, em->rot.y, PI)) < PI / 2.0f && d < 4000000.0f) {
                switch (em->type) {
                case 0:
                case 1:
                default:
                    EmRoutineSet(em, 1, 6, 0, 0);
                    break;
                case 2:
                case 3:
                    if (w->pParasite && (Rnd() & 1) && w->pParasite->ckAtkEnable()) {
                        EmRoutineSet(em, 1, 7, 0, 0);
                    } else {
                        EmRoutineSet(em, 1, 6, 0, 0);
                    }
                    break;
                }
            } else if (w->targetAngAbs > 2.0943952f) {
                EmRoutineSet(em, 1, 5, 0, 0);
            }
        }
        break;
    }
    em3cDoorOpenCk(em);
}

static void em3c_R1_Turn180(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    switch (em->xFE) {
    case 0:
        if (w->female) {
            MotionSetCore(em, MOTION(em), ARC(0x35), (int) ARC(0x36), 5, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x1B), (int) ARC(0x1C), 5, 1, 0);
        }
        w->turnAng = em->rot.y + PI;
        em->xFE++;
    case 1:
        if (em->seFlags28B & 8) {
            f32 a = Muku(&em->pos, &w->targetPos, w->turnAng, PI / 32.0f);

            w->turnAng += a;
            w->turnAng = LIMIT_ANGLE(w->turnAng);
            em->rot.y += a;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (em3cStayCk(em)) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        }
        break;
    }
}

static void em3c_R1_MoveAtk(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    Vec pl;
    Vec v;

    switch (em->xFE) {
    case 0: {
        f32 ang;

        GetPlPos(&pl, 0, 18.0f);
        ang = fabsf(Muku(&em->pos, &pl, em->rot.y, PI));
        int far = 1;
        if (ang < PI / 12.0f) {
            far = 0;
        }
        if (pG->x4FB8 == 1) {
            far = 0;
        }
        if ((u8) (Rnd() % 10) > 4) {
            far = 0;
        }
        w->timer = 20;
        if (pGS->x4F88 <= 2) {
            w->timer = 10;
        }
        if (pGS->x4F88 > 7) {
            w->timer = 30;
        }
        if (w->female) {
            if (far) {
                MotionSetCore(em, MOTION(em), ARC(0x39), (int) ARC(0x3A), 5, 5, 0);
                w->timer = 0;
                w->atkNo = 1;
                EstSet((int) em, -1, 0, 0, 0x31, 0x25, 0, 0, (u32) em, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x37), (int) ARC(0x38), 5, 5, 0);
                w->atkNo = 0;
                EstSet((int) em, -1, 0, 0, 0x31, 0x24, 0, 0, (u32) em, 0);
            }
        } else {
            if (far) {
                MotionSetCore(em, MOTION(em), ARC(0x1F), (int) ARC(0x20), 5, 5, 0);
                w->timer = 0;
                w->atkNo = 1;
                EstSet((int) em, -1, 0, 0, 0x31, 0x22, 0, 0, (u32) em, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x1D), (int) ARC(0x1E), 5, 5, 0);
                w->atkNo = 0;
                EstSet((int) em, -1, 0, 0, 0x31, 0x21, 0, 0, (u32) em, 0);
            }
        }
        w->atkHit = 0;
        em->xFE++;
    }
    case 1:
        if (w->timer) {
            w->timer--;
            em->rot.y += Muku(&em->pos, &w->routePos, em->rot.y, PI / 32.0f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0)) {
            if (w->atkHit == 0) {
                GameAddPoint(0xB);
            }
            if (w->atkHit != 0 || em->plDist2 < 6250000.0f) {
                w->atkWait = 45;
                if (pG->x4F88 <= 3) {
                    w->atkWait = 60;
                }
                if (pG->x4F88 <= 1) {
                    w->atkWait = 75;
                }
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        } else {
            em3cAtkCk2(em, w->atkNo);
            if (em->seFlags28B & 4) {
                if (w->female) {
                    v.x = 100.0f;
                    v.y = 0.0f;
                    v.z = 1000.0f;
                } else {
                    v.x = 100.0f;
                    v.y = 0.0f;
                    v.z = 1150.0f;
                }
                PSMTXMultVec(em->mat, &v, &v);
                EstSet(0, -1, &v, &em->rot, 0x31, 2, 0, 0, 0, 0);
            }
        }
        break;
    }
}

static void em3c_R1_CoreAtk(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    switch (em->xFE) {
    case 0:
        switch (em->type) {
        case 0:
        case 1:
        default:
            em->xFF = 0;
            if (w->female) {
                MotionSetCore(em, MOTION(em), ARC(0x2A), 0, 10, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x10), 0, 10, 1, 0);
            }
            break;
        case 2:
        case 3:
            em->xFF = 1;
            if (w->female) {
                MotionSetCore(em, MOTION(em), ARC(0x43), 0, 10, 1, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0x29), 0, 10, 1, 0);
            }
            break;
        }
        if (w->pParasite) {
            switch (em->type) {
            case 0:
            case 1:
            default:
                w->pParasite->setAtk(0);
                break;
            case 2:
            case 3:
                w->pParasite->setCritical();
                break;
            }
        }
        w->timer = 120;
        w->atkHit = 0;
        em->xFE++;
    case 1: {
        int end = 0;
        u16 ret = MotionMoveF(em, 0);

        if (em->xFF == 0) {
            if (w->timer) {
                w->timer--;
            } else {
                end = 1;
            }
        } else if (ret) {
            end = 1;
        }
        if (w->pParasite && w->pParasite->ckAtkHit()) {
            w->atkHit = 1;
        }
        if (end) {
            if (w->atkHit) {
                w->atkWait = 45;
                if (pG->x4F88 <= 3) {
                    w->atkWait = 60;
                }
                if (pG->x4F88 <= 1) {
                    w->atkWait = 75;
                }
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        }
        break;
    }
    }
}

static void plemDmMStar(cPlayer* pl)
{
    pl->subArc = PL_EM(pl)->subArc;
    pl->dmg.set(0, 2);
    switch (pl->xFE) {
    case 0: {
        int flag = 1;

        if (pl->xFF) {
            flag = 0x41;
        }
        MotionSetCore(pl, MOTION(pl), PL_ARC(0x61), (int) PL_ARC(0x62), 3, flag, 0);
        PlSetFace(1);
        PlSetDamageSe(0);
        pl->xFE++;
    }
    case 1:
        if (MotionMoveF(pl, 0)) {
            EndPlDamage();
            pl->dmg.set(0, 0xF);
        }
        break;
    }
    pl->subArc = pl->subArc2;
}

static void em3c_R0_Damage(cEm3c* em)
{
    EM3C_WK(em)->flags |= 0x100;
    Em3c_R2_move_tbl[em->xFD](em);
}

static void em3c_R1_Dm_Normal(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    switch (em->xFE) {
    case 0: {
        f32 ang;
        void* m0;
        void* m1;

        ang = fabsf(Muku(&em->pos, &em->x328, em->rot.y, PI));
        int dir = 1;
        if (ang < PI / 2.0f) {
            dir = 0;
        }
        if (Rnd() & 1) {
            dir = 2;
        }
        if (w->female) {
            switch ((u32) dir) {
            case 0:
            default:
                m0 = ARC(0x3B);
                m1 = ARC(0x3C);
                break;
            case 1:
                m0 = ARC(0x3D);
                m1 = ARC(0x3E);
                break;
            case 2:
                m0 = ARC(0x3F);
                m1 = ARC(0x40);
                break;
            }
        } else {
            switch ((u32) dir) {
            case 0:
            default:
                m0 = ARC(0x21);
                m1 = ARC(0x22);
                break;
            case 1:
                m0 = ARC(0x23);
                m1 = ARC(0x24);
                break;
            case 2:
                m0 = ARC(0x25);
                m1 = ARC(0x26);
                break;
            }
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 5, 1, 0);
        em->xFE++;
    }
    case 1:
        if (MotionMoveF(em, 0)) {
            if (pG->x4F88 > 9) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->plDist > 7000.0f && w->waitTimer == 0 && pG->x4F88 > 1 && (u8) (Rnd() % 10) > 4) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        }
        break;
    }
}

static void em3c_R1_Dm_Big(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    switch (em->xFE) {
    case 0: {
        void* m0;
        void* m1;

        if (w->female) {
            m0 = ARC(0x3F);
            m1 = ARC(0x40);
        } else {
            m0 = ARC(0x25);
            m1 = ARC(0x26);
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 5, 1, 0);
        w->timer = 60;
        em->xFE++;
    }
    case 1:
        if (MotionMoveF(em, 0)) {
            if (pG->x4F88 > 9) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->plDist > 7000.0f && w->waitTimer == 0 && pG->x4F88 > 1 && (u8) (Rnd() % 10) > 4) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        } else if (em->hp < em->hpMax / 2 && !(w->flags & 0x800) && w->timer) {
            w->timer--;
            if (w->timer == 0) {
                em3cPartsBombHead(em);
            }
        }
        break;
    }
}

static void em3c_R1_Dm_Head(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    w->flags |= 0x200;
    switch (em->xFE) {
    case 0:
        if (w->female) {
            MotionSetCore(em, MOTION(em), ARC(0x41), (int) ARC(0x42), 5, 1, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x27), (int) ARC(0x28), 5, 1, 0);
        }
        SndCall(8, 0x11, &em->pos, em->id, 0, em);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if (pG->x4F88 > 9) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else if (w->plDist > 7000.0f && w->waitTimer == 0 && pG->x4F88 > 1 && (u8) (Rnd() % 10) > 4) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
        } else if ((em->seFlags28B & 1) && w->hitCnt <= 0) {
            em3cPartsBombHead(em);
        }
        break;
    }
}

static void em3c_R0_Die(cEm3c* em)
{
    Em3c_R3_move_tbl[em->xFD](em);
}

static void em3c_R1_Die_Normal(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    // COMPILER-DIFF: #13 (int shape): the EstSet stack zero of the then-arm is a function-scope
    // constant with one use in another block, so sched1 sees the store as a leaf (issued after
    // `mr r3`) and update_equiv_regs moves the `li` next to it, where it takes r0 like the
    // original's rematerialised reload.
    int zero = 0;

    switch (em->xFE) {
        // Unreachable loop: its NOTE_INSN_LOOP_END survives in front of the `case 0:` label, so
        // cse does not follow `beq case0` (the label must be preceded by a BARRIER) and the arm
        // does not learn xFE == 0 — the EstSet stack zero is a fresh `li r0, 0`, not the switch register.
        do { } while (0);
    case 0:
        w->timer = 60;
        if (em->xFF) {
            EstSet((int) em, -1, 0, 0, 0x31, 5, 0, 0, (u32) em, (void*) zero);
        } else {
            EstSet((int) em, -1, 0, 0, 0x31, 0, 0, 0, (u32) em, 0);
            EstSet((int) em, -1, 0, 0, 0x31, 3, 0, 0, (u32) em, 0);
        }
        SndCall(8, 4, &em->pos, em->id, 0, em);
        if (w->pInfo3AC) {
            w->pInfo3AC->be_flag &= ~8;
        }
        em->atari.throughOn();
        if (w->pParasite) {
            w->pParasite->clearLostWait();
            EffectEspDelete(0, w->espKind, (u32) w->pParasite, 0);
            EffectEspgenDelete(0, w->espKind, (int) w->pParasite);
            EffectEfmDelete(0, w->espKind, (int) w->pParasite);
            w->pParasite = 0;
        }
        em3cPartsBombSet(em, 0);
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
            if (w->timer == 0) {
                em->clearStatus(5);
                em->setStatus(8);
                EmSetDropItem(em);
                if (em->xFF == 0) {
                    SndCall(8, 0x3E, &em->pos, em->id, 0, em);
                }
            }
        } else {
            em->clearStatus(5);
            em->alpha -= 0.02857f;
            em->x12F = 1;
            if (em->alpha < 0.0f) {
                em->alpha = 0.0f;
                em->be_flag &= ~2;
                em->xFE++;
            }
        }
        break;
    case 2:
        em->be_flag |= 0x4000;
        break;
    }
}

int em3cAtkCk(cEm3c* em, Vec* pos, int no)
{
    Em3cWork* w = EM3C_WK(em);
    EmAtkInfo* atk = &em3c_atk_tbl[no];
    cModel* p = GetPartsAddr(em->pParts, 0x1A);
    int hit = EmAtkHitCk(atk, pos, &p->x88, no == 2);

    if (hit) {
        w->atkHit = 1;
        if (hit & 1) {
            if (no) {
                EmPlBloodSet2(em, &p->worldPos, 1, 0x31, 6);
            } else {
                EmPlBloodSet2(em, &p->worldPos, 1, 0x31, 0xA);
            }
            if (no == 1 && (s16) pG->pl_life > 0) {
                EstSet((int) pPL, -1, 0, 0, 0x31, 9, 0, 0, (u32) pPL, 0);
                SetPlDamage((int) em, plemDmMStar);
                if (fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, PI)) < PI / 2.0f) {
                    FSet(pPL->rot.y, pPL->rot.y + Muku(&pPL->pos, &em->pos, pPL->rot.y, PI));
                    pPL->xFF = 0;
                } else {
                    FSet(pPL->rot.y, pPL->rot.y + Muku(&em->pos, &pPL->pos, pPL->rot.y, PI));
                    pPL->xFF = 1;
                }
            }
        }
        if (hit & 2) {
            if (no) {
                EmSubBloodSet(em, &p->worldPos, 1, 0x31, 6);
            } else {
                EmSubBloodSet(em, &p->worldPos, 1, 0x31, 0xA);
            }
        }
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        SndCall(8, 9, &em->pos, em->id, 0, em);
        return 1;
    }
    return 0;
}

int em3cAtkCk2(cEm3c* em, int no)
{
    Em3cWork* w = EM3C_WK(em);
    cModel* p;
    Vec v;

    if (w->atkHit) {
        return 0;
    }
    if (!(em->seFlags28B & 1)) {
        return 0;
    }
    p = GetPartsAddr(em->pParts, 0x1A);
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(p->mat, &v, &v);
    if (em3cAtkCk(em, &v, no)) {
        return 1;
    }
    p = GetPartsAddr(em->pParts, 0x1A);
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 500.0f;
    PSMTXMultVec(p->mat, &v, &v);
    if (em3cAtkCk(em, &v, no)) {
        return 1;
    }
    p = GetPartsAddr(em->pParts, 0x1A);
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1000.0f;
    PSMTXMultVec(p->mat, &v, &v);
    if (em3cAtkCk(em, &v, no)) {
        return 1;
    }
    if (w->female == 0) {
        p = GetPartsAddr(em->pParts, 0x1A);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1500.0f;
        PSMTXMultVec(p->mat, &v, &v);
        if (em3cAtkCk(em, &v, no)) {
            return 1;
        }
    }
    return 0;
}

void em3cRouteCk(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    Vec ofs;
    Vec top;
    Vec hit;
    Vec nrm;
    f32 spd;

    if (em->hp <= 0) {
        return;
    }
    if ((pG->flags_51E4 & 3) != (em->emsetNo & 3)) {
        return;
    }
    spd = SQRTF(em->plDist2);
    if (spd > 6000.0f) {
        spd = 6000.0f;
    }
    spd *= 1.0f / 6000.0f;
    switch (em->emsetNo % 5) {
    case 0:
    default:
        ofs.x = 0.0f;
        ofs.y = 500.0f;
        ofs.z = 0.0f;
        break;
    case 1:
        ofs.x = spd * 1500.0f;
        ofs.y = 500.0f;
        ofs.z = 0.0f;
        break;
    case 2:
        ofs.x = spd * -1500.0f;
        ofs.y = 500.0f;
        ofs.z = 0.0f;
        break;
    case 3:
        ofs.x = spd * 2000.0f;
        ofs.y = 500.0f;
        ofs.z = 0.0f;
        break;
    case 4:
        ofs.x = spd * -2000.0f;
        ofs.y = 500.0f;
        ofs.z = 0.0f;
        break;
    }
    PSMTXMultVec(pPL->mat, &ofs, &ofs);
    top = pPL->pos;
    top.y += 500.0f;
    if (SatMgr.hitCheck(&top, &ofs, &hit, 0, 0, 0)) {
        PSVECSubtract(&top, &hit, &nrm);
#line 2600 "D:/Bio4/Prog/em3c.cpp"
        VECNormalize(&nrm, &nrm);
        PSVECScale(&nrm, &nrm, 350.0f);
        PSVECAdd(&hit, &nrm, &ofs);
        if (pG->flags_60 & 0x4000) {
            Draw_line3d(&top, &hit, 0xFFFFFFFF, 0);
            Draw_line3d(&top, &ofs, 0xFF00FF00, 0);
        }
    }
    top = ofs;
    if (RouteCkToPos(em, &top, &w->routePos, 0, 0)) {
        w->flags |= 1;
    }
    w->routeAng = Muku(&em->pos, &w->routePos, em->rot.y, PI);
    w->routeAngAbs = fabsf(w->routeAng);
    if (em->xFC == 0) {
        w->routeAng = 0.0f;
        w->routeAngAbs = 0.0f;
        em->plDist2 = 100000000.0f;
    }
    w->plDist = RouteCkPosToPosDis(&em->pos, &pPL->pos);
    w->targetPos = w->routePos;
    w->targetAng = w->routeAng;
    w->targetAngAbs = w->routeAngAbs;
    w->targetDist = em->plDist2;
    w->pTarget = pPLS;
    w->flags &= ~4;
}

void em3cModelInit(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    Mtx m;
    void* bin;
    cModelInfo* info;

    switch (em->type) {
    case 0:
    default:
        em->modelInit(ARC(5), ARC(6));
        break;
    case 1:
        em->modelInit(ARC(5), ARC(6));
        break;
    case 2:
        em->modelInit(ARC(9), ARC(0xA));
        break;
    case 3:
        em->modelInit(ARC(9), ARC(0xA));
        break;
    }
    w->pInfo3AC = 0;
    switch (em->type) {
    case 0:
    default:
        bin = ARC(7);
        break;
    case 1:
        bin = ARC(7);
        break;
    case 2:
        bin = ARC(0xB);
        break;
    case 3:
        bin = ARC(0xB);
        break;
    }
    info = ModInfoMgr.create(bin, ARC(8));
    w->pInfo3AC = info;
    if (info) {
        em->addModel(info);
    }
    PSMTXRotRad(m, 'y', em->rot.y);
    TransMatrix(m, &em->pos);
    w->pInfo3A8 = 0;
    switch (em->type) {
    case 0:
    case 2:
    default:
        w->pInfo3A8 = ModInfoMgr.create(ARC(0xC), ARC(0xD));
        if (w->pInfo3A8) {
            em->addModel(w->pInfo3A8);
        }
        break;
    case 1:
    case 3:
        w->pInfo3A8 = ModInfoMgr.create(ARC(0xE), ARC(0xF));
        if (w->pInfo3A8) {
            em->addModel(w->pInfo3A8);
        }
        break;
    }
    em->scale.x = 1.2f;
    em->scale.y = 1.2f;
    em->scale.z = 1.2f;
}

void em3cPartsBombSet(cEm3c* em, int add)
{
    u32 i;
    u32 j;
    // pointer locals: the table bases stay first in the `add`/`lhzx` (a symbol operand is
    // swapped behind the index at expand time)
    u16* tm = em3c_bomb_time;
    Vec (*pt)[5] = em3c_bomb_pt;

    for (i = 0; i < 25; i++) {
        int no = em3c_bomb_parts[i];
        cParts* p = (cParts*) em->getPartsPtr(no);
        Em3cPartsBomb* b;
        Vec* tbl;
        int kind;

        if (p->motParts.flags & 0x01000000) {
            continue;
        }
        p->motParts.flags |= 0x25000002;
        b = EM3C_BOMB(p);
        switch (no) {
        default:
            kind = 0;
            break;
        case 1:
            kind = 1;
            break;
        case 0x10:
            kind = 4;
            break;
        case 0x12:
        case 0x16:
            kind = 2;
            break;
        case 0x11:
        case 0x13:
        case 0x14:
        case 0x17:
        case 0x18:
            kind = 3;
            break;
        }
        tbl = pt[kind];
        for (j = 0; j < 5; j++) {
            PSMTXMultVec(p->mat, &tbl[j], &b->pt[j]);
            b->spd[j].x = fRand1_1() * 50.0f;
            b->spd[j].y = fRand1_1() * 30.0f;
            b->spd[j].z = fRand1_1() * 50.0f;
        }
        {
            u16* tp = tm + i;
            int t = *tp;
            b->timer = add + t;
        }
    }
}

void em3cPartsBombHead(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    cParts* p;
    Em3cPartsBomb* b;
    Vec v;
    u32 j;

    w->hitCnt = 0;
    w->hit[0].flags &= ~1;
    if (w->flags & 0x10) {
        return;
    }
    p = (cParts*) em->getPartsPtr(3);
    if (p->motParts.flags & 0x01000000) {
        return;
    }
    p->motParts.flags |= 0x25000002;
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 10.0f;
    PSMTXMultVecSR(em->mat, &v, &v);
    b = EM3C_BOMB(p);
    for (j = 0; j < 5; j++) {
        PSMTXMultVec(p->mat, &em3c_bomb_pt[0][j], &b->pt[j]);
        b->spd[j].x = fRand1_1() * 10.0f + v.x;
        b->spd[j].y = fRand1_1() * 10.0f + v.y;
        b->spd[j].z = fRand1_1() * 10.0f + v.z;
    }
    b->timer = 0;
    w->bombTimer = 45;
    w->flags |= 0x10;
    EstSet((int) em, -1, 0, 0, 0x31, 4, 0, 0, (u32) em, 0);
    SndCall(8, 7, &em->pos, em->id, 0, em);
    em3cSetParasite(em);
}

void em3cPartsBombControl(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    Vec old[5];
    Vec cen;
    Vec a;
    Vec b;
    Vec c;
    cParts* p;
    u32 i;
    // declared right after `i`: gcse creates the PRE'd `bomb+C` / `p+C` / `fp+D` address pseudos in
    // hash-bucket order (hash = K + REGNO + C), and their spill slots follow that order -- `bomb`
    // must be pseudo p+2 (or p+3) for the target's 0xb0/0xb4/0xbc slots
    Em3cPartsBomb* bomb;
    u32 n;
    u32 j;
    u32 k;

    if (!(em->be_flag & 2)) {
        return;
    }
    for (i = 0; i < 25; i++) {
        int no = em3c_bomb_parts[i];
        p = (cParts*) em->getPartsPtr(no);
        int kind;
        f32 sum;

        if (!(p->motParts.flags & 0x01000000)) {
            continue;
        }
        switch (no) {
        default:
            kind = 0;
            break;
        case 1:
            kind = 1;
            break;
        case 0x10:
            kind = 4;
            break;
        case 0x12:
        case 0x16:
            kind = 2;
            break;
        case 0x11:
        case 0x13:
        case 0x14:
        case 0x17:
        case 0x18:
            kind = 3;
            break;
        }
        bomb = EM3C_BOMB(p);
        if (bomb->timer) {
            bomb->timer--;
            continue;
        }
        for (k = 0; k < 5; k++) {
            bomb->spd[k].y -= 10.0f;
            old[k] = bomb->pt[k];
            PSVECAdd(&bomb->pt[k], &bomb->spd[k], &bomb->pt[k]);
            bomb->hitBits = 0;
        }
        for (n = 0; n < 5; n++) {
            for (j = 0; j < 5; j++) {
                for (k = 0; k < 5; k++) {
                    if (j != k) {
                        f32 s;
                        f32 r;

                        // the length is the routine-scope `sum` (the same pseudo accumulates the speeds
                        // below): a multi-block pseudo that crosses calls, so it takes the callee-saved
                        // f31 (`fmr f31,f1`) and the 0.5/1.0 constants fall to f29/f30 like the target
                        PSVECSubtract(&bomb->pt[k], &bomb->pt[j], &cen);
                        sum = PSVECMag(&cen);
                        s = (em3c_bomb_dist[kind][j][k] - sum) * 0.5f;
                        r = 1.0f / sum;
                        PSVECScale(&cen, &cen, r * s);
                        PSVECAdd(&bomb->pt[k], &cen, &bomb->pt[k]);
                        PSVECSubtract(&bomb->pt[j], &cen, &bomb->pt[j]);
                        if (bomb->pt[j].y < em->pos.y) {
                            bomb->pt[j].y = em->pos.y;
                            bomb->hitBits |= 1 << j;
                        }
                        if (bomb->pt[k].y < em->pos.y) {
                            bomb->pt[k].y = em->pos.y;
                            bomb->hitBits |= 1 << k;
                        }
                    }
                }
                // Dead exit (k == 5 here) that only cse2 can fold: cse1 stops its extended block at the
                // k loop's LOOP_END note, cse2 (after loop.c) walks through it and knows `k > 4` from the
                // latch's exit test, deletes the jump, and flow drops the compare. So at gcse time the j
                // body still has an edge to the n latch that skips `j++`, and the block LCM leaves `j+1`
                // at its latch (j stays a biv with the j*20/j*12/&pt[j] givs) instead of hoisting it in
                // front of the k loop. No trace in the code. COMPILER-DIFF: 3
                if (k <= 4) {
                    goto next_n;
                }
            }
        next_n:;
        }
        sum = 0.0f;
        for (j = 0; j < 5; j++) {
            if ((bomb->hitBits >> j) & 1) {
                bomb->spd[j].x *= 0.8f;
                bomb->spd[j].y *= fRand0_1() * 0.2f + -0.6f;
                bomb->spd[j].z *= 0.8f;
                if (no == 3 && (w->flags & 0x50) == 0x10) {
                    SndCall(8, 8, &em->pos, em->id, 0, em);
                    w->flags |= 0x40;
                }
            } else {
                PSVECSubtract(&bomb->pt[j], &old[j], &bomb->spd[j]);
                PSVECScale(&bomb->spd[j], &bomb->spd[j], 0.999f);
            }
            sum += bomb->spd[j].x * bomb->spd[j].x + bomb->spd[j].y * bomb->spd[j].y + bomb->spd[j].z * bomb->spd[j].z;
        }
        if (sum < 1.0f) {
            p->motParts.flags &= ~0x01000000;
        }
        PSVECSubtract(&bomb->pt[0], &bomb->pt[1], &c);
        PSVECSubtract(&bomb->pt[2], &bomb->pt[3], &a);
        PSVECCrossProduct(&c, &a, &b);
        PSVECCrossProduct(&b, &c, &a);
#line 2959 "D:/Bio4/Prog/em3c.cpp"
        VECNormalize(&a, &a);
        VECNormalize(&b, &b);
        VECNormalize(&c, &c);
        p->mat[0][0] = a.x;
        p->mat[1][0] = a.y;
        p->mat[2][0] = a.z;
        p->mat[0][1] = b.x;
        p->mat[1][1] = b.y;
        p->mat[2][1] = b.z;
        p->mat[0][2] = c.x;
        p->mat[1][2] = c.y;
        p->mat[2][2] = c.z;
        PSVECAdd(&bomb->pt[0], &bomb->pt[1], &cen);
        PSVECScale(&cen, &cen, 0.5f);
        TransMatrix(p->mat, &cen);
        ScaleMatrix(p->mat, &p->scale);
        p->worldPos = cen;
        // pGS: the pG load stays behind the `p->worldPos = cen` word stores (global.h)
        if (pGS->debug_mode == 8) {
            // the routine's j/k again (and j for the speed loop above, p for the parts walk below):
            // one pseudo per name is what puts j in r24, k in r29 and p in r26 like the target --
            // separate counters rank differently in global alloc and permute the callee-saved set
            for (j = 0; j < 5; j++) {
                for (k = 0; k < 5; k++) {
                    if (j != k) {
                        Draw_line3d(&bomb->pt[j], &bomb->pt[k], 0xFFFFFFFF, 0);
                    }
                }
            }
        }
    }
    // the walk reuses `p`: the extra refs rank p above bomb in global alloc (r26/r25)
    p = (cParts*) em->pParts;
    while (p) {
        p = p->pNext;
        if (p == 0) {
            break;
        }
        if (!(p->motParts.flags & 0x01000000) && (((cParts*) p->pParent)->motParts.flags & 0x01000000)) {
            PSMTXConcat(p->pParent->mat, p->worldMat, p->mat);
            p->worldPos.x = p->mat[0][3];
            p->worldPos.y = p->mat[1][3];
            p->worldPos.z = p->mat[2][3];
        }
    }
}

int em3cSetDmVal(cEm3c* em)
{
    EmHitInfo* part = em->dmPart;
    int near = 0;
    int dmg;

    if (part->rad < 36000000.0f) {
        near = 1;
    }
    dmg = 20;
    if (em->dmWep <= 0x2D) {
        dmg = GetWepDmVal(em, em->dmWep, near);
    }
    if (part->partsNo == 3) {
        dmg *= 3;
    }
    return dmg;
}

void em3cSetParasite(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    Vec pos;
    Vec rot;
    cObj* obj;
    int frame;
    int female;
    void* bin;
    void* tpl;
    void* m0;
    void* m1;
    void* m2;
    void* m3;
    void* m4;
    void* m5;
    void* m6;
    void* m7;
    void* m8;
    void* m9;
    void* m10;

    w->flags |= 0x800;
    frame = (((MotionData*) ARC(0x4A))->maxFrame & 0x3FFF) / 4;
    switch (em->type) {
    case 0:
    case 1:
    default:
        female = 1;
        bin = ARC(0x4B);
        tpl = ARC(0x4C);
        m0 = ARC(0x4D);
        m1 = ARC(0x4E);
        m2 = ARC(0x52);
        m3 = ARC(0x54);
        m4 = ARC(0x55);
        m5 = ARC(0x55);
        m6 = ARC(0x55);
        m7 = ARC(0x4F);
        m8 = ARC(0x53);
        m9 = ARC(0x50);
        m10 = ARC(0x51);
        break;
    case 2:
    case 3:
        female = 0;
        bin = ARC(0x56);
        tpl = ARC(0x57);
        m0 = ARC(0x58);
        m1 = ARC(0x58);
        m2 = ARC(0x59);
        m3 = ARC(0x5A);
        m4 = ARC(0x5E);
        m5 = ARC(0x5C);
        m6 = ARC(0x5F);
        m7 = ARC(0x5B);
        m8 = ARC(0x5B);
        m9 = ARC(0x5D);
        m10 = ARC(0x5D);
        break;
    }
    pos.x = 0.0f;
    pos.y = 200.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    if (female) {
        w->pParasite = (cObj16*) SetObj16(bin, tpl, em, em, 2, 0xB, &pos, &rot);
    } else {
        w->pParasite = (cObj16*) SetObj16(bin, tpl, em, em, 2, 0xD, &pos, &rot);
    }
    if (w->pParasite) {
        w->pParasite->setMotData(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10);
        w->pParasite->setMotData(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10);
        w->pParasite->setPlDmgMot(ARC(0x61), (int) ARC(0x62));
    }
    if (female) {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = -0.6632251f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        obj = SetObj16(ARC(0x48), ARC(0x49), em, w->pParasite, 0x16, 0xC, &pos, &rot);
        if (obj) {
            MotSetObj16(obj, ARC(0x4A), 4, 0);
            w->pPara[0] = (cObj16*) obj;
        }
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.61086524f;
        obj = SetObj16(ARC(0x48), ARC(0x49), em, w->pParasite, 0x17, 0xC, &pos, &rot);
        if (obj) {
            MotSetObj16(obj, ARC(0x4A), 4, frame);
            w->pPara[1] = (cObj16*) obj;
        }
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = -0.5235988f;
        obj = SetObj16(ARC(0x48), ARC(0x49), em, w->pParasite, 0x18, 0xC, &pos, &rot);
        if (obj) {
            MotSetObj16(obj, ARC(0x4A), 4, frame * 2);
            w->pPara[2] = (cObj16*) obj;
        }
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        obj = SetObj16(ARC(0x48), ARC(0x49), em, w->pParasite, 0x19, 0xC, &pos, &rot);
        if (obj) {
            MotSetObj16(obj, ARC(0x4A), 4, frame * 3);
            w->pPara[3] = (cObj16*) obj;
        }
    }
    SndCall(8, 0x36, &em->getPartsPtr(3)->worldPos, em->id, 0, em);
    if (female) {
        EstSet((int) em, -1, 0, 0, 0x31, 0x11, 1, 0, (u32) em, 0);
    } else {
        EstSet((int) em, -1, 0, 0, 0x31, 0xD, 1, 0, (u32) em, 0);
    }
    if (w->pParasite) {
        EstSet((int) w->pParasite, -1, 0, 0, 0x31, 0x12, 0, w->espKind, (u32) w->pParasite, 0);
    }
    w->hit[9].flags |= 1;
}

void em3cFootSe(cEm3c* em)
{
    if (em->seNo) {
        if (em->seNo == 1 || em->seNo == 2) {
            em->seNo = 0;
            SndCall(8, 0, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
        }
    }
}

int em3cStayCk(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);
    u32 cnt = 0;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((e->be_flag & 0x201) == 1 && e->id == 0x3C && e->hp > 0 && e != em && e->checkStatus(5)
            && EM3C_WK(e)->plDist < w->plDist) {
            cnt++;
        }
    }
    if (pG->x4F88 <= 2) {
        if (cnt == 0) {
            return 0;
        }
    } else {
        if (cnt <= 1) {
            return 0;
        }
    }
    if (em->plDist2 > 25000000.0f && !(w->flags & 1)) {
        return 0;
    }
    return 1;
}

int em3cFindCk(cEm3c* em)
{
    Em3cWork* w = EM3C_WK(em);

    if (w->flags & 0x80) {
        return 0;
    }
    if (!(w->flags & 1)) {
        return 0;
    }
    if (w->routeAngAbs < 1.0471976f && em->plDist2 < 100000000.0f) {
        w->flags |= 0x80;
        return 1;
    }
    if (pG->flags_5010 & 0x20000000) {
        f32 r;

        switch (pG->bell_stat) {
        case 0:
            r = 25000.0f;
            break;
        case 1:
            r = 25000.0f;
            break;
        default:
            r = 25000.0f;
            break;
        }
        // the override after the switch makes the arm stores dead (flow deletes them, the
        // compares stay) and puts the pool load into the join block; `r` keeps 4 sets so
        // `r * r` is not folded
        r = 25000.0f;
        if ((em->pos.x - pG->bell_pos.x) * (em->pos.x - pG->bell_pos.x) + (em->pos.y - pG->bell_pos.y) * (em->pos.y - pG->bell_pos.y)
                + (em->pos.z - pG->bell_pos.z) * (em->pos.z - pG->bell_pos.z)
            < r * r) {
            if ((w->flags & 1) && w->plDist < r) {
                w->flags |= 0x80;
                return 1;
            }
        }
    }
    if ((pG->flags_500C & 0x00800000) && w->plDist < 25000.0f) {
        w->flags |= 0x80;
        return 1;
    }
    if (em3cDeadCk(em)) {
        w->flags |= 0x80;
        return 1;
    }
    return 0;
}

void em3cDoorOpenCk(cEm3c* em)
{
    Vec v;
    u32 i;
    f32 ang;

    if (EM3C_WK(em)->stuckCnt % 10 != 5) {
        return;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEmDoor* e = (cEmDoor*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        EmDoorWork* dw;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x41) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y)
                + (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z)
            > 6250000.0f) {
            continue;
        }
        dw = EMDOOR_WK(e);
        ang = fabsf(Muku2(dw->rotY, em->rot.y, PI));
        if (ang > PI / 4.0f && ang < 2.3561945f) {
            continue;
        }
        PSMTXMultVec(dw->inv, &em->pos, &v);
        if (ang < PI / 2.0f) {
            if (v.z > 0.0f || v.z < -800.0f) {
                continue;
            }
        } else {
            if (v.z < 0.0f || v.z > 800.0f) {
                continue;
            }
        }
        if (v.x > dw->width || v.x < -dw->width) {
            continue;
        }
        if (v.y > 500.0f || v.y < -500.0f) {
            continue;
        }
        switch (e->ckOpen()) {
        case 0:
        default:
            e->setOpen(&em->pos, 0, 0, 0);
            break;
        case 1:
        case 2:
        case 3:
            break;
        }
    }
}

void em3cAtkSuspend(cEm3c* em, int on)
{
    u32 i;

    if (on) {
        pG->flags_5010 |= 0x10000000;
        pPLS->setNoSuspend(1);
        em->setNoSuspend(1);
        if (pSUB) {
            pSUB->setNoSuspend(1);
        }
        pG->flags_5014 |= 0x02000000;
    } else {
        pG->flags_5010 &= ~0x10000000;
        pPLS->setNoSuspend(0);
        em->setNoSuspend(0);
        if (pSUB) {
            pSUB->setNoSuspend(0);
        }
        pG->flags_5014 &= ~0x02000000;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((e->be_flag & 0x201) == 1 && e->id == 0x3C && e != em && e->xFC == 1 && e->xFD <= 1) {
            if (on) {
                e->setNoSuspend(1);
            } else {
                e->setNoSuspend(0);
            }
        }
    }
}
