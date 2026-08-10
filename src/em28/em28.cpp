// em28 module (D:/Bio4/Prog/em28.cpp): the crow. Waits, walks and takes off (dash / jump routines) when
// the player comes near or the bell rings (em28EscapeCk), drops a random item from its wait routine,
// and dies on the ground (R1_Die_Normal) or falling out of the air (R1_Die_Air).

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em28.h"
#include "emhit.h"
#include "emwep.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "motion.h"
#include "sce_at.h"
#include "snd.h"
#include "player.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

// motion.h declares the one-argument form; the enemies pass a second argument.
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");

typedef void (*Em28Func)(cEm28*);

static void em28_R0_Init(cEm28* em);
static void em28_R0_Move(cEm28* em);
static void em28_R1_Wait(cEm28* em);
static void em28_R1_Walk(cEm28* em);
static void em28_R1_Dash(cEm28* em);
static void em28_R1_Jump(cEm28* em);
static void em28_R0_Damage(cEm28* em);
static void em28_R1_Dm_Small(cEm28* em);
static void em28_R0_Die(cEm28* em);
static void em28_R1_Die_Normal(cEm28* em);
static void em28_R1_Die_Air(cEm28* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

// Struct-member view of the player pointer (cam_ctrl.cpp PlayerPtr).
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)

extern "C" void _prolog()
{
    OSReport("em28 prolog Ok\n");
    EmInitFunc = Em28Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em28Init(cEm* em)
{
    new (em) cEm28();
}

void em28DmCk(cEm28* em)
{
    Em28Work* w = EM28_WK(em);
    u8 wep;

    if (em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 4:
        case 5:
        case 7:
            em->hp = 0;
            EmSetDie(em);
            w->flags |= 0x40;
            if (w->flags & 0x10) {
                EmRoutineSet(em, 3, 1, 0, 0);
            } else {
                EmRoutineSet(em, 3, 0, 0, 0);
            }
            return;
        }
    }
    if (em->dmHit) {
        wep = em->dmWep;
        em->dmHit = 0;
        switch (wep) {
        case 0x16:
        case 0x17:
        case 0x2A:
            if (!(w->flags & 0x10)) {
                if (Rnd() & 1) {
                    EmRoutineSet(em, 1, 3, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 2, 0, 0);
                }
            }
            return;
        }
        em->hp = 0;
        SndCall(8, 0, &em->pos, em->id, 0, em);
        switch (em->dmWep) {
        case 0:
        case 0xb:
        case 0xc:
        case 0x10:
        case 0x11:
        case 0x14:
        case 0x15:
        case 0x1b:
        case 0x1d:
        case 0x26:
        case 0x27:
        case 0x2b:
            EmDmBloodSet2(em, 0x20, 0, 0, 0, 0);
            EmDmBloodSet2(em, 0x20, 2, 0, 0, 0);
            break;
        case 5:
        case 6:
        case 9:
        case 0xa:
        case 0xd:
        case 0x12:
        case 0x13:
        case 0x28:
        case 0x29:
            EmDmBloodSet2(em, 0x20, 0, 0, 0, 0);
            EmDmBloodSet2(em, 0x20, 2, 0, 0, 0);
            break;
        case 0x2c:
        default:
            EmDmBloodSet2(em, 0x20, 0, 0, 0, 0);
            EmDmBloodSet2(em, 0x20, 2, 0, 0, 0);
            break;
        case 7:
        case 8:
        case 0x21:
            if (em->plDist2 < 16000000.0f) {
                EmDmBloodSet2(em, 0x20, 0, 0, 0, 0);
                EmDmBloodSet2(em, 0x20, 2, 0, 0, 0);
            } else {
                EmDmBloodSet2(em, 0x20, 0, 0, 0, 0);
                EmDmBloodSet2(em, 0x20, 2, 0, 0, 0);
            }
            break;
        case 0xE:
            EmDmBloodSet2(em, 0x20, 0, 0, 0, 0);
            EmDmBloodSet2(em, 0x20, 2, 0, 0, 0);
            em->hp = 1;
            if (!(w->flags & 0x10)) {
                // Allocation lever (loop notes, no code): the doubled routine refs rank the HI `1`
                // (hp = 1 / xFC) above the HI zero in global-alloc (one r30, zero r29).
                do { EmRoutineSet(em, 1, (Rnd() & 1) ? 3 : 2, 0, 0); } while (0);
            }
            return;
        }
        if (em->hp <= 0) {
            EmSetDie(em);
            if (w->flags & 0x10) {
                EmRoutineSet(em, 3, 1, 0, 0);
            } else {
                EmRoutineSet(em, 3, 0, 0, 0);
            }
        } else if (!(w->flags & 0x10)) {
            if (Rnd() & 1) {
                EmRoutineSet(em, 1, 3, 0, 0);
            } else {
                EmRoutineSet(em, 1, 2, 0, 0);
            }
        }
    }
}

Em28Func Em28_R0_move_tbl[4] = {
    em28_R0_Init,
    em28_R0_Move,
    em28_R0_Damage,
    em28_R0_Die,
};

static Em28Func Em28_R1_move_tbl[4] = {
    em28_R1_Wait,
    em28_R1_Walk,
    em28_R1_Dash,
    em28_R1_Jump,
};

static Em28Func Em28_R2_move_tbl[1] = {
    em28_R1_Dm_Small,
};

static Em28Func Em28_R3_move_tbl[2] = {
    em28_R1_Die_Normal,
    em28_R1_Die_Air,
};

// Parts remap of the mirrored motions (cModel::motFlip).
static u16 em28_flip_tbl[36] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x08, 0x09, 0x0A, 0x05, 0x06, 0x07, 0x0B, 0x0F, 0x10, 0x11, 0x0C,
    0x0D, 0x0E, 0x12, 0x13, 0x14, 0x15, 0x16, 0x0F, 0x10, 0x11, 0x12, 0x17, 0x18, 0x19, 0x1A, 0x1B,
    0x1C, 0x1D, 0x1E, 0x00,
};
asm(".section .data\n\t.balign 8\n\t.text");

void cEm28::move()
{
    Em28Work* w = EM28_WK(this);
    f32 spd;

    if (xFC != 0) {
        em28DmCk(this);
    }
    w->flags &= ~0x1F;
    if (w->escapeWait) {
        w->escapeWait--;
    }
    Em28_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    partsWorldCalc();
    spd = SQRTF((oldPos.x - pos.x) * (oldPos.x - pos.x) + (oldPos.z - pos.z) * (oldPos.z - pos.z));
    EmAtCheck(this);
    atari.move();
    if (w->flags & 0x10) {
        SatMgr.checkAir(this, 0);
    } else {
        SatMgr.check(this, 0);
    }
    if (SQRTF((pos.x - oldPos.x) * (pos.x - oldPos.x) + (pos.z - oldPos.z) * (pos.z - oldPos.z)) < spd * 0.5f) {
        w->stuckCnt++;
    } else {
        w->stuckCnt = 0;
    }
}

static void em28_R0_Init(cEm28* em)
{
    Em28Work* w = EM28_WK(em);
    int zero;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(5)) == 0) {
            pLog->err(0, 0, "em28() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    case 1:
        if (em->modelInit(ARC(4), ARC(6)) == 0) {
            pLog->err(0, 0, "em28() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    }
    em->be_flag &= ~0x10;
    em->setStatus(3);
    zero = 0;
    em->setStatus(1);
    em->motFlip = em28_flip_tbl;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 500.0f, 500.0f, 0.0f };

        em->lightInfo.init2(0, 3, &ofs, &size, 2);
    }
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    atariInitF(&em->atari, 0.0f, 0.0f, 0.0f, 300.0f, 200.0f, 200.0f, 500.0f, 3, 0x2000, 10);   // COMPILER-DIFF: #1
    em->setStatus(0xB);
    YarareInit(em, 0.0f, 0.0f, -130.0f, 200.0f, 100.0f, 3, 5);
    EspDataLoad((u32) ARC(0xB), 0x20, 0);
    w->flags = zero;
    w->escapeWait = zero;
    w->pCtrl11 = GetCtrlCtrl11();
    w->pCtrl12 = GetCtrlCtrl12();
    w->x17C = zero;
    em->setStatus(5);
    EmRoutineSet(em, 1, zero, zero, zero);
    MotionSetCore(em, MOTION(em), ARC(0xC), 0, 0, 5, 0);
    MotionMoveF(em, 0);
    em28_R0_Move(em);
}

static void em28_R0_Move(cEm28* em)
{
    Em28_R1_move_tbl[em->xFD](em);
}

// Take off when the floor under the crow drops away (its perch broke).
static inline void em28FloorCk(cEm28* em)
{
    if ((pG->flags_51E4 & 3) == (em->emsetNo & 3)) {
        if (SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) < em->pos.y - 250.0f) {
            EmRoutineSet(em, 1, 3, 0, 0);
        }
    }
}

// Remember where the bell (this crow) was disturbed.
static inline void em28BellSet(cEm28* em)
{
    if (!(pG->flags_5010 & 0x20000000)) {
        BitOn(pG->flags_5010, 0x20000000);
        memcpy((u8*) pG + 0x4F3C, &em->pos, sizeof(Vec));
        pG->bell_stat = 0;
    }
}

static void em28_R1_Wait(cEm28* em)
{
    Em28Work* w = EM28_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xC), 0, 0x1E, 5, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            if ((int) em->flags_3C8 >= 0) {
                if (Rnd() & 1) {
                    if (Rnd() & 3) {
                        em->xFE++;
                    } else if ((Rnd() & 3) == 0 && !(w->flags & 0x20)) {
                        em->xFE = 6;
                    } else {
                        em->xFE = 4;
                    }
                }
            }
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0xD), 0, 0xA, 1, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if (Rnd() & 1) {
                em->xFE = 0;
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x18), 0, 0xA, 1, 0);
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            if (Rnd() & 1) {
                em->xFE = 0;
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0x19), 0, 0xA, 1, 0);
        w->timer = 15;
        em->xFE++;
    case 7:
        if (w->timer) {
            w->timer--;
            if (w->timer == 0) {
                Vec pos;
                cEmWep* wep;
                // One `no` for the three arms: a 3-set pseudo with 9 refs over ~15 insns outranks
                // em in global-alloc and takes r31 (a block-local `no` is local-alloc'd and can
                // never get r31, the frame-pointer register), which pushes em to r30 and lets
                // &pos reuse r31.
                int no;

                pos = em->pos;
                if (Rnd() & 7) {
                    if ((u8) (Rnd() % 3)) {
                        wep = SetWeapon(ARC(7), ARC(8), &pos, &em->rot, 1);
                        if (wep) {
                            no = SceAtCreateItemAt(&pos, 8, 0, -1, -1, 0, -1);
                            SceAtSetItemModel(no, wep);
                            wep->setAtNo(no);
                        }
                    } else {
                        wep = SetWeapon(ARC(7), ARC(9), &pos, &em->rot, 1);
                        if (wep) {
                            no = SceAtCreateItemAt(&pos, 9, 0, -1, -1, 0, -1);
                            SceAtSetItemModel(no, wep);
                            wep->setAtNo(no);
                        }
                    }
                } else {
                    wep = SetWeapon(ARC(7), ARC(0xA), &pos, &em->rot, 1);
                    if (wep) {
                        no = SceAtCreateItemAt(&pos, 0xA, 0, -1, -1, 0, -1);
                        SceAtSetItemModel(no, wep);
                        wep->setAtNo(no);
                    }
                }
                w->flags |= 0x20;
                if (wep) {
                    wep->setYarare(0, 100.0f, 10.0f);
                    wep->setEffDamage(0x20, 1);
                    wep->setSeDamage(1, 6, em->id);
                }
            }
        }
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
    em28FloorCk(em);
    em28EscapeCk(em);
}

static void em28_R1_Walk(cEm28* em)
{
    Em28Work* w = EM28_WK(em);

    switch (em->xFE) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(0xE), 0, 0xA, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0xF), 0, 0xA, 5, 0);
        }
        w->targetAng = fRand1_1() * PI;
        w->timer = (u8) (Rnd() % 5) + 3;
        em->xFE++;
    case 1:
        em->rot.y += Muku2(em->rot.y, w->targetAng, PI / 128.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (w->stuckCnt > 30) {
            w->timer = 0;
        }
        if (MotionMoveF(em, 0)) {
            if (w->timer) {
                w->timer--;
            } else {
                EmRoutineSet(em, 1, 0, 0, 0);
            }
        }
        break;
    }
    em28FloorCk(em);
    em28EscapeCk(em);
}

static void em28_R1_Dash(cEm28* em)
{
    Em28Work* w = EM28_WK(em);
    int st = em->xFE;

    switch (st) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x10), (int) ARC(0x1B), 3, 5, 0);
        w->targetAng = GetXZAngle(&pPL->pos, &em->pos);
        w->targetAng += fRand1_1() * (PI / 2.0f);
        w->targetAng = LIMIT_ANGLE(w->targetAng);
        w->timer = (u8) (Rnd() % 3) + 3;
        w->turnTimer = (u8) (Rnd() % 30) + 30;
        EstSet(0, -1, &em->getPartsPtr(0)->worldPos, 0, 0x20, 0, 0, 0, 0, 0);
        SndCall(8, 2, &em->pos, em->id, 0, em);
        em28BellSet(em);
        em->xFF = Rnd() & 1;
        em->xFE++;
    case 1:
        if (w->turnTimer) {
            w->turnTimer--;
        } else {
            w->turnTimer = (u8) (Rnd() % 15) + 15;
            w->targetAng += fRand1_1() * (PI / 4.0f);
            w->targetAng = LIMIT_ANGLE(w->targetAng);
        }
        em->rot.y += Muku2(em->rot.y, w->targetAng, PI / 16.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (w->stuckCnt > 3) {
            if (em->xFF) {
                w->targetAng += PI / 5.0f;
            } else {
                w->targetAng -= PI / 5.0f;
            }
            w->targetAng = LIMIT_ANGLE(w->targetAng);
        }
        if (MotionMoveF(em, 0)) {
            if (w->timer) {
                w->timer--;
            } else {
                EmRoutineSet(em, 1, 0, 0, 0);
            }
        }
        break;
    }
    em28FloorCk(em);
    em28EscapeCk(em);
}

static void em28_R1_Jump(cEm28* em)
{
    Em28Work* w = EM28_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x12), (int) ARC(0x1C), 3, 1, 0);
        w->targetAng = GetXZAngle(&pPL->pos, &em->pos);
        w->targetAng += fRand1_1() * (PI / 2.0f);
        w->targetAng = LIMIT_ANGLE(w->targetAng);
        w->timer = (Rnd() & 3) + 4;
        em->xFF = Rnd() & 1;
        SndCall(8, 2, &em->pos, em->id, 0, em);
        em->xFE++;
    case 1:
        em->rot.y += Muku2(em->rot.y, w->targetAng, PI / 16.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (w->stuckCnt > 3) {
            if (em->xFF) {
                w->targetAng += PI / 16.0f;
            } else {
                w->targetAng -= PI / 16.0f;
            }
            w->targetAng = LIMIT_ANGLE(w->targetAng);
        }
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x13), (int) ARC(0x1D), 3, 4, 0);
        w->spd.x = 0.0f;
        w->spd.y = fRand0_1() * 100.0f + 150.0f;
        w->spd.z = fRand0_1() * 50.0f + 150.0f;
        EstSet(0, -1, &em->getPartsPtr(0)->worldPos, 0, 0x20, 0, 0, 0, 0, 0);
        em28BellSet(em);
        em->xFE++;
    case 3: {
        Vec v;

        w->flags |= 0x10;
        em->rot.y += Muku2(em->rot.y, w->targetAng, PI / 16.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (w->stuckCnt > 3) {
            w->targetAng += PI / 32.0f;
            w->targetAng = LIMIT_ANGLE(w->targetAng);
        }
        w->spd.y -= 15.0f;
        PSMTXMultVecSR(em->mat, &w->spd, &v);
        PSVECAdd(&em->pos, &v, &em->pos);
        if (w->spd.y < 0.0f) {
            f32 fl;

            v = em->pos;
            v.y = em->oldPos.y;
            fl = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
            if (em->pos.y < fl) {
                em->pos.y = fl;
                w->spd.y = 0.0f;
                em->xFE++;
                MotionMoveF(em, 0);
                break;
            }
        }
        MotionMoveF(em, 0);
        break;
    }
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x14), (int) ARC(0x1E), 3, 1, 0);
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    em28EscapeCk(em);
}

static void em28_R0_Damage(cEm28* em)
{
    Em28Work* w = EM28_WK(em);

    w->flags |= 8;
    Em28_R2_move_tbl[em->xFD](em);
}

static void em28_R1_Dm_Small(cEm28* em)
{
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x11), 0, 0, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 0, 0, 0);
        }
        break;
    }
}

static void em28_R0_Die(cEm28* em)
{
    Em28Work* w = EM28_WK(em);

    w->flags |= 8;
    Em28_R3_move_tbl[em->xFD](em);
}

// Fade every part while dying (work flag bit6).
static inline void em28DieFade(cEm28* em, Em28Work* w)
{
    if (w->flags & 0x40) {
        cModelInfo* info;

        for (info = em->pInfo; info; info = info->pNext) {
            if (info->color[0] > 0x20) {
                info->color[0] -= 0x20;
            }
            info->color[2] = info->color[1] = info->color[0];
        }
    }
}

static void em28_R1_Die_Normal(cEm28* em)
{
    Em28Work* w = EM28_WK(em);

    switch (em->xFE) {
    case 0:
        em->clearStatus(5);
        switch ((u8) (Rnd() % 3)) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0x11), 0, 3, 1, 0);
            break;
        case 1:
            em->rot.y += Muku(&em->pos, &em->x328, em->rot.y, PI);
            MotionSetCore(em, MOTION(em), ARC(0x17), 0, 3, 1, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(0x1A), 0, 3, 1, 0);
            break;
        }
        w->timer = 14;
        em->atari.flags &= ~0x200;
        em->xFE++;
    case 1:
        em28DieFade(em, w);
        if (w->timer) {
            w->timer--;
            if (w->timer == 0) {
                SndCall(8, 1, &em->getPartsPtr(0)->worldPos, em->id, 0, em);
            }
        }
        if (MotionMoveF(em, 0)) {
            em->setStatus(8);
            EmSetDropItem(em);
            em->xFE++;
        }
        break;
    }
}

static void em28_R1_Die_Air(cEm28* em)
{
    Em28Work* w = EM28_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x15), 0, 3, 1, 0);
        em->xFE++;
    case 1: {
        Vec v;

        em28DieFade(em, w);
        w->flags |= 0x10;
        w->spd.y -= 15.0f;
        PSMTXMultVecSR(em->mat, &w->spd, &v);
        PSVECAdd(&em->pos, &v, &em->pos);
        if (w->spd.y < 0.0f) {
            f32 fl;

            v = em->pos;
            v.y = em->oldPos.y;
            fl = SatMgr.getFloor(&v, 600.0f, 100000.0f, 0, 0);
            if (em->pos.y < fl) {
                em->pos.y = fl;
                w->spd.y = 0.0f;
                em->xFE++;
                MotionMoveF(em, 0);
                break;
            }
        }
        MotionMoveF(em, 0);
        break;
    }
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x16), 0, 3, 1, 0);
        em->clearStatus(5);
        em->atari.flags &= ~0x200;
        SndCall(8, 1, &em->pos, em->id, 0, em);
        em->xFE++;
    case 3:
        em28DieFade(em, w);
        if (MotionMoveF(em, 0)) {
            em->setStatus(8);
            EmSetDropItem(em);
            em->xFE++;
        }
        break;
    }
}

// Escape when the crow stands too close to the player, the bell rings or the player shoots.
int em28EscapeCk(cEm28* em)
{
    Em28Work* w = EM28_WK(em);
    f32 d;

    if (w->flags & 0x10) {
        return 0;
    }
    if (w->escapeWait != 0) {
        return 0;
    }
    int esc = 0;
    if (pG->flags_5010 & 0x20000000) {
        f32 r;

        // three identical arms + the override after the switch: the arm sets are dead (the
        // compare skeleton stays) and the block-local `r` is loaded at the use (em3cFindCk idiom)
        switch (pG->bell_stat) {
        case 0:
            r = 15000.0f;
            break;
        case 1:
            r = 15000.0f;
            break;
        default:
            r = 15000.0f;
            break;
        }
        r = 15000.0f;
        if ((em->pos.x - pG->bell_pos.x) * (em->pos.x - pG->bell_pos.x) + (em->pos.y - pG->bell_pos.y) * (em->pos.y - pG->bell_pos.y)
            + (em->pos.z - pG->bell_pos.z) * (em->pos.z - pG->bell_pos.z) < r * r) {
            esc = 1;
        }
    }
    d = em->plDist2;
    if (pG->flags_500C & 0x00800000) {
        if (d < 100000000.0f) {
            esc = 1;
        }
    }
    if (d < 4000000.0f) {
        esc = 1;
    }
    if (!esc) {
        return 0;
    }
    if (Rnd() & 3) {
        EmRoutineSet(em, 1, 2, 0, 0);
    } else {
        EmRoutineSet(em, 1, 3, 0, 0);
    }
    w->escapeWait = (u8) (Rnd() % 15) + 15;
    return 1;
}
