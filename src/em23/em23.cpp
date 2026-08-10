// em23 module (D:/Bio4/Prog/em23.cpp): the crow. Sits on the ground (or on the corpse the room
// hands it) until the player comes near, takes off, circles at flyHeight above the player, turns,
// lands again on the corpse and pecks at it; a hit knocks it out of the air (Dm_Air) or kills it.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em23.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "motion.h"
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

typedef void (*Em23Func)(cEm23*);

static void em23_R0_Init(cEm23* em);
static void em23_R0_Move(cEm23* em);
static void em23_R1_R20ALanding(cEm23* em);
static void em23_R1_Wait(cEm23* em);
static void em23_R1_Takeoff(cEm23* em);
static void em23_R1_TakeoffDash(cEm23* em);
static void em23_R1_Turn(cEm23* em);
static void em23_R1_Landing(cEm23* em);
static void em23_R1_ToCorpse(cEm23* em);
static void em23_R1_Eat(cEm23* em);
static void em23_R0_Damage(cEm23* em);
static void em23_R1_Dm_Air(cEm23* em);
static void em23_R0_Die(cEm23* em);
static void em23_R1_Die_Normal(cEm23* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)

// Collision flag bits changed through the info's address (`addi rX, em, 0x2b4; lhz 0x1a(rX)`).
static inline void AtariOff(cAtariInfo* at, u16 mask) { at->flags &= mask; }
static inline void AtariOn(cAtariInfo* at, u16 bits) { at->flags |= bits; }

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
    OSReport("em23 prolog Ok\n");
    EmInitFunc = Em23Init;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em23Init(cEm* em)
{
    new (em) cEm23();
}

void em23DmCk(cEm23* em)
{
    Em23Work* w = EM23_WK(em);
    int near;
    int dmg;

    if (em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, 0)) {
        case 1:
        case 4:
        case 5:
        case 7:
            em->hp = 0;
            EmSetDie(em);
            EmReserveDropItem(em);
            w->flags |= 4;
            EmRoutineSet(em, 2, 0, 0, 0);
            return;
        }
    }
    if (em->dmHit) {
        em->dmHit = 0;
        near = 0;
        if (em->dmPart->rad < 36000000.0f) {
            near = 1;
        }
        switch (em->dmWep) {
        case 0:
        case 2:
        case 0xB:
        case 0x10:
            dmg = Rnd() % 200 + 900;
            break;
        case 7:
        case 8:
        case 0x21:
            if (near) {
                dmg = 9999;
            } else {
                dmg = Rnd() % 200 + 900;
            }
            break;
        case 0x1B:
            dmg = 9999;
            break;
        case 1:
        case 3:
        case 4:
        case 5:
        case 6:
        case 9:
        case 0xA:
        case 0xC:
        case 0xD:
        case 0xF:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1D:
        case 0x26:
        case 0x27:
        case 0x2C:
        case 0x2D:
        default:
            dmg = 9999;
            break;
        case 0xE:
            dmg = 0;
            break;
        }
        LifeDownSet(em, dmg, 0);
        EmDmBloodSet2(em, 0x1B, 0, 0, 0, 0);
        EmDmBloodSet2(em, 0x1B, 1, 0, 0, 0);
        SndCall(8, 0xF, &em->pos, em->id, 0, em);
        EmRoutineSet(em, 2, 0, 0, 0);
    }
}

Em23Func Em23_R0_move_tbl[4] = {
    em23_R0_Init,
    em23_R0_Move,
    em23_R0_Damage,
    em23_R0_Die,
};

static Em23Func Em23_R1_move_tbl[8] = {
    em23_R1_R20ALanding,
    em23_R1_Wait,
    em23_R1_Takeoff,
    em23_R1_TakeoffDash,
    em23_R1_Turn,
    em23_R1_Landing,
    em23_R1_ToCorpse,
    em23_R1_Eat,
};

static Em23Func Em23_R2_move_tbl[1] = {
    em23_R1_Dm_Air,
};

static Em23Func Em23_R3_move_tbl[1] = {
    em23_R1_Die_Normal,
};

// Parts index remap of the flipped motions (cModel::motFlip).
static u16 em23_flip_tbl[80] = {
    0, 1, 2, 3, 4, 5, 0xA, 0xB, 0xC, 0xD, 6, 7, 8, 9, 0xE, 0xF, 0x11, 0x10, 0x15, 0x16, 0x17, 0x12, 0x13, 0x14,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
    0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
    0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D,
    0x4E, 0x4F,
};

void cEm23::move()
{
    Em23Work* w = EM23_WK(this);
    Vec old;

    em23DmCk(this);
    w->flags &= ~1;
    if (w->moveTimer) {
        w->moveTimer--;
    }
    if (w->stateTimer) {
        w->stateTimer--;
    }
    Em23_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    partsWorldCalc();
    old = pos;
    EmAtCheck(this);
    atari.move();
    SatMgr.checkAir(this, 0x100000);
    PartsWorldPosCalc(this);
    partsFixAdjust();
    PartsWorldPosCalc(this);
    if (old.x != pos.x || old.z != pos.z) {
        w->moveTimer = 10;
        w->flags |= 2;
    } else {
        w->flags &= ~2;
    }
    if ((seFlags28B & 0x80) && pos.y - pPL->pos.y < 3000.0f) {
        SndCall(8, 0xB, &pos, id, 0, this);
    }
}

static void em23_R0_Init(cEm23* em)
{
    Em23Work* w = EM23_WK(em);
    f32 scale;
    int zero;

    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 1000.0f, 1000.0f, 0.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->atari.init(1, 0x2000, 10, 0.0f, -100.0f, 0.0f, 350.0f, 150.0f, 150.0f, 200.0f);
    em->setStatus(1);
    em->setStatus(0xB);
    EspDataLoad((u32) ARC(4), 0x1B, 0);
    em->motFlip = em23_flip_tbl;
    YarareInit(em, 0.0f, -100.0f, 0.0f, 250.0f, 200.0f, 1, 1);
    if (em->modelInit(ARC(5), ARC(8)) == 0) {
        pLog->err(0, 0, "em23() ModelInit failed.");
        em->xFC = 0xFF;
        return;
    }
    em->be_flag &= ~0x10;
    scale = fRand1_1() * 0.1f + 1.0f;
    em->scale.x = scale;
    em->scale.y = scale;
    em->scale.z = scale;
    w->pWingInfo = 0;
    w->wing = 0xFF;
    em23SetWing(em, 1);
    w->flags = zero;
    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    w->moveTimer = zero;
    w->x20 = 0.0f;
    w->pCorpse = 0;
    w->x3C = 100000000.0f;
    w->x24 = 0.0f;
    w->stateTimer = Rnd() % 300 + 150;
    w->flyHeight = (f32) (em->emsetNo % 5) * 1000.0f + 8000.0f;
    if (pGS->room_id == 0x30A) {
        w->flyHeight = (f32) (em->emsetNo % 5) * 2000.0f + 20000.0f;
    }
    w->pCtrl11 = GetCtrlCtrl11();
    w->pCtrl12 = GetCtrlCtrl12();
    switch (em->x38D) {
    case 0:
    default:
        EmRoutineSet(em, 1, 1, 0, 0);
        break;
    case 1:
        EmRoutineSet(em, 1, 0, 0, 0);
        break;
    }
    MotionSetCore(em, MOTION(em), ARC(9), 0, 0, 5, 0);
    MotionMoveF(em, 0);
    em23_R0_Move(em);
}

static void em23_R0_Move(cEm23* em)
{
    Em23_R1_move_tbl[em->xFD](em);
}

// Idle motion: one of the four ground animations, two of them with a blend motion.
static inline void em23WaitMotion(cEm23* em)
{
    void* m0;
    void* m1;

    switch (Rnd() & 3) {
    case 0:
    default:
        m0 = ARC(9);
        m1 = 0;
        break;
    case 1:
        m0 = ARC(0xA);
        m1 = 0;
        break;
    case 2:
        m0 = ARC(0xB);
        m1 = ARC(0x1C);
        break;
    case 3:
        m0 = ARC(0xC);
        m1 = ARC(0x1D);
        break;
    }
    MotionSetCore(em, MOTION(em), m0, (int) m1, 5, 5, 0);
}

// Takeoff motion: the blend motion depends on the enemy list slot.
static inline void em23TakeoffMotion(cEm23* em)
{
    void* m1;

    switch (em->emsetNo % 5) {
    case 0:
    default:
        m1 = ARC(0x24);
        break;
    case 1:
        m1 = ARC(0x25);
        break;
    case 2:
        m1 = ARC(0x26);
        break;
    case 3:
        m1 = ARC(0x27);
        break;
    case 4:
        m1 = ARC(0x28);
        break;
    }
    MotionSetCore(em, MOTION(em), ARC(0xE), (int) m1, 5, 1, 0);
}

// The player is close enough to react to (farther with the noise flag set).
#define EM23_PL_NEAR(em, near, far) \
    ((em)->plDist2 < (near) || ((pG->flags_500C & 0x00800000) && (em)->plDist2 < (far)))

// Flight speed update shared by the air routines: accelerate forward, climb or dive towards the
// height flyHeight above the player, then move.
// (the clamp constants are pseudos created after the sums: the em3a hover-clamp idiom, otherwise
// the pool load waits behind the store and the compare reuses the sum's register)
static inline void em23FlyMove(cEm23* em, Em23Work* w)
{
    {
        f32 z = w->spd.z + 10.0f;
        f32 max = 150.0f;

        w->spd.z = z;
        if (z > max) {
            w->spd.z = max;
        }
    }
    if (em->pos.y < pPL->pos.y + w->flyHeight) {
        f32 y = w->spd.y + 10.0f;
        f32 max = 50.0f;

        w->spd.y = y;
        if (y > max) {
            w->spd.y = max;
        }
    } else {
        f32 y = w->spd.y - 10.0f;
        f32 min = 0.0f;

        w->spd.y = y;
        if (y < min) {
            w->spd.y = min;
        }
    }
    em23AddSpeedAir(em, em->rot.y);
}

// Both motion switches are written out with ONE pair of routine-scope pointers (not the
// em23WaitMotion/em23TakeoffMotion inlines): the shared `m1` pseudo is what makes the takeoff join's
// subArc copy take r10 instead of r11 (global-alloc `regs_someone_prefers`).
static void em23_R1_R20ALanding(cEm23* em)
{
    Em23Work* w = EM23_WK(em);
    void* m0;
    void* m1;

    switch (em->xFE) {
    case 0:
        switch (Rnd() & 3) {
        case 0:
        default:
            m0 = ARC(9);
            m1 = 0;
            break;
        case 1:
            m0 = ARC(0xA);
            m1 = 0;
            break;
        case 2:
            m0 = ARC(0xB);
            m1 = ARC(0x1C);
            break;
        case 3:
            m0 = ARC(0xC);
            m1 = ARC(0x1D);
            break;
        }
        MotionSetCore(em, MOTION(em), m0, (int) m1, 5, 5, 0);
        em->hp = 1;
        em23SetWing(em, 1);
        em->xFE++;
    case 1:
        em->partsFixMemory(0x14);
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        if (EM23_PL_NEAR(em, 25000000.0f, 100000000.0f)) {
            em->xFE++;
        }
        break;
    case 2:
        switch (em->emsetNo % 5) {
        case 0:
        default:
            m1 = ARC(0x24);
            break;
        case 1:
            m1 = ARC(0x25);
            break;
        case 2:
            m1 = ARC(0x26);
            break;
        case 3:
            m1 = ARC(0x27);
            break;
        case 4:
            m1 = ARC(0x28);
            break;
        }
        MotionSetCore(em, MOTION(em), ARC(0xE), (int) m1, 5, 1, 0);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        w->stateTimer = Rnd() % 300 + 150;
        w->targetAng = em->rot.y + fRand1_1() * (PI / 2.0f);
        w->targetAng = LIMIT_ANGLE(w->targetAng);
        w->timer = 7;
        em->xFE++;
    case 3:
        if (em->seFlags28B & 1) {
            em23SetWing(em, 0);
            Ctrl11SetSe(w->pCtrl11, em, 10, 10, 0xB);
        }
        if (MotionMoveF(em, 0)) {
            em->xFE++;
            w->spd.x = 0.0f;
            w->spd.y = 20.0f;
            w->spd.z = 30.0f;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x23), 5, 5, 0);
        AtariOff(&em->atari, 0xFCFF);
        w->timer = 20;
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            if (w->timer) {
                w->timer--;
            } else {
                AtariOn(&em->atari, 0x300);
                EmRoutineSet(em, 1, 4, 0, 0);
            }
        }
        break;
    }
    if (em->xFE > 3) {
        em23FlyMove(em, w);
    }
}

static void em23_R1_Wait(cEm23* em)
{
    switch (em->xFE) {
    case 0:
        em23WaitMotion(em);
        em23SetWing(em, 1);
        em->xFE++;
    case 1:
        em->partsFixMemory(0x14);
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        if (EM23_PL_NEAR(em, 25000000.0f, 100000000.0f)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
}

static void em23_R1_Takeoff(cEm23* em)
{
    Em23Work* w = EM23_WK(em);

    switch (em->xFE) {
    case 0:
        em23TakeoffMotion(em);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        w->stateTimer = Rnd() % 300 + 150;
        w->targetAng = em->rot.y + fRand1_1() * (PI / 2.0f);
        w->targetAng = LIMIT_ANGLE(w->targetAng);
        w->timer = 7;
        em->xFE++;
    case 1:
        em->rot.y += Muku2(em->rot.y, w->targetAng, PI / 16.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (em->seFlags28B & 1) {
            em23SetWing(em, 0);
            Ctrl11SetSe(w->pCtrl11, em, 10, 10, 0xB);
        }
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 4, 0, 0);
            w->spd.x = 0.0f;
            w->spd.y = 20.0f;
            w->spd.z = 30.0f;
        }
        break;
    }
}

static void em23_R1_TakeoffDash(cEm23* em)
{
    Em23Work* w = EM23_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xF), 0, 5, 1, 0);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        w->stateTimer = Rnd() % 300 + 150;
        w->timer = 7;
        w->targetAng = em->rot.y + fRand1_1() * (PI / 2.0f);
        w->targetAng = LIMIT_ANGLE(w->targetAng);
        em->xFE++;
    case 1:
        em->rot.y += Muku2(em->rot.y, w->targetAng, PI / 16.0f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (em->seFlags28B & 1) {
            em23SetWing(em, 0);
            Ctrl11SetSe(w->pCtrl11, em, 10, 10, 0xB);
        }
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 4, 0, 0);
            w->spd.x = 0.0f;
            w->spd.y = 20.0f;
            w->spd.z = 30.0f;
        }
        break;
    }
}

static void em23_R1_Turn(cEm23* em)
{
    Em23Work* w = EM23_WK(em);

    switch (em->xFE) {
    case 0:
        switch (em->xFF) {
        case 0:
        default:
            MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x23), 5, 5, 0);
            break;
        case 1:
            MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x22), 5, 5, 0);
            break;
        case 2:
            MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x21), 5, 5, 0);
            break;
        case 3:
            MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x20), 5, 5, 0);
            break;
        case 4:
            MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x1F), 5, 5, 0);
            break;
        case 5:
            MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x1E), 5, 5, 0);
            break;
        case 6:
            if (Rnd() & 1) {
                MotionSetCore(em, MOTION(em), ARC(0x17), 0, 5, 5, 0);
            } else {
                MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x1E), 5, 5, 0);
            }
            break;
        }
        w->timer = 5;
        w->turnTimer = Rnd() % 120 + 120;
        w->turnDir = Rnd() & 1;
        em->xFE++;
    case 1: {
        f32 step;
        f32 y;

        if (w->turnTimer) {
            w->turnTimer--;
            if (w->turnTimer == 0) {
                w->turnDir ^= 1;
                w->turnTimer = Rnd() % 120 + 120;
            }
        }
        if (w->turnDir) {
            step = PI / 256.0f;
        } else {
            step = 0.017453292f;
        }
        if (w->moveTimer) {
            step = PI / 32.0f;
        }
        if (pG->room_id == 0x30A && em->pos.y < pPL->pos.y + w->flyHeight) {
            if (em->emsetNo & 1) {
                step = PI / 128.0f;
            } else {
                step = -PI / 128.0f;
            }
            y = em->rot.y + step;
        } else {
            y = em->rot.y + Muku(&em->pos, &pPL->pos, em->rot.y, step);
        }
        em->rot.y = y;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        if (MotionMoveF(em, 0)) {
            if (w->timer) {
                w->timer--;
            } else {
                em->xFF++;
                if (em->xFF > 5) {
                    em->xFF = 5;
                }
                em->xFE = 0;
            }
        }
        break;
    }
    }
    em23FlyMove(em, w);
}

static void em23_R1_Landing(cEm23* em)
{
    Em23Work* w = EM23_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0xD), (int) ARC(0x23), 5, 5, 0);
        em->xFE++;
    case 1:
        if (w->pCorpse) {
            em->rot.y += Muku(&em->pos, &w->pCorpse->pos, em->rot.y, PI / 64.0f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        MotionMoveF(em, 0);
        if (w->pCorpse
            && (em->pos.x - w->pCorpse->pos.x) * (em->pos.x - w->pCorpse->pos.x)
                       + (em->pos.z - w->pCorpse->pos.z) * (em->pos.z - w->pCorpse->pos.z)
                   < 9000000.0f) {
            em->xFE++;
        } else {
            w->spd.z += 10.0f;
            if (w->spd.z > 150.0f) {
                w->spd.z = 150.0f;
            }
            if (em->pos.y < pPL->pos.y + 7000.0f) {
                w->spd.y += 10.0f;
                if (w->spd.y > 50.0f) {
                    w->spd.y = 50.0f;
                }
            } else {
                w->spd.y -= 10.0f;
                if (w->spd.y < 0.0f) {
                    w->spd.y = 0.0f;
                }
            }
            em23AddSpeedAir(em, em->rot.y);
        }
        break;
    case 2:
        em->xFE++;
    case 3:
        if (w->pCorpse) {
            em->rot.y += Muku(&em->pos, &w->pCorpse->pos, em->rot.y, PI / 64.0f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        MotionMoveF(em, 0);
        w->spd.z -= 5.0f;
        if (w->spd.z < 100.0f) {
            w->spd.z = 100.0f;
        }
        w->spd.y -= 5.0f;
        if (w->spd.y < -50.0f) {
            w->spd.y = -50.0f;
        }
        if (em23AddSpeedAir(em, em->rot.y)) {
            if (fabsf(em->pos.y - w->pCorpse->pos.y) > 200.0f) {
                EmRoutineSet(em, 1, 4, 0, 0);
            } else {
                em->xFE++;
            }
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x10), (int) ARC(0x29), 5, 5, 0);
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            if (w->pCorpse) {
                if (fabsf(em->pos.y - w->pCorpse->pos.y) < 200.0f) {
                    EmRoutineSet(em, 1, 6, 0, 0);
                } else {
                    EmRoutineSet(em, 1, 2, 0, 0);
                }
            } else {
                EmRoutineSet(em, 1, 1, 0, 0);
            }
        }
        break;
    }
    if (w->pCorpse) {
        if ((pG->flags_51E4 & 0xF) == (em->emsetNo & 0xF)) {
            Vec v;
            f32 fl;

            v = w->pCorpse->pos;
            fl = SatMgr.getFloor(&em->pos, 0.0f, 5000.0f, 0, 0);
            if (fl == -100000.0f) {
                fl = v.y;
            }
            v.y = fl + 500.0f;
            if (SatMgr.hitCheck(&em->pos, &v, 0, 0, 0, 0)) {
                goto takeoff;
            }
        }
        if (w->pCorpse && w->pCorpse->plDist2 < 16000000.0f) {
        takeoff:
            EmRoutineSet(em, 1, 4, 0, 0);
            return;
        }
    }
    em23SetWing(em, 0);
}

static void em23_R1_ToCorpse(cEm23* em)
{
    Em23Work* w = EM23_WK(em);

    if (w->pCorpse && em->xFE == 0) {
        f32 ang = Muku(&em->pos, &w->pCorpse->pos, em->rot.y, PI);
        f32 a = fabsf(ang);

        if (a > 2.0943952f) {
            em->xFE = 2;
        }
        if (a > 1.0471976f) {
            if (ang < 0.0f) {
                em->xFE = 4;
            } else {
                em->xFE = 6;
            }
        }
    }
    switch (em->xFE) {
    case 0:
        if (Rnd() & 1) {
            MotionSetCore(em, MOTION(em), ARC(0x1A), (int) ARC(0x2B), 5, 5, 0);
        } else {
            MotionSetCore(em, MOTION(em), ARC(0x1B), (int) ARC(0x2C), 5, 5, 0);
        }
        em->xFE++;
    case 1:
        if (w->pCorpse) {
            em->rot.y += Muku(&em->pos, &w->pCorpse->pos, em->rot.y, PI / 64.0f);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        if (MotionMoveF(em, 0) && w->pCorpse
            && (em->pos.x - w->pCorpse->pos.x) * (em->pos.x - w->pCorpse->pos.x)
                       + (em->pos.z - w->pCorpse->pos.z) * (em->pos.z - w->pCorpse->pos.z)
                   < 360000.0f) {
            EmRoutineSet(em, 1, 7, 0, 0);
            return;
        }
        if (EM23_PL_NEAR(em, 6250000.0f, 100000000.0f)) {
            if (Rnd() % 3) {
                EmRoutineSet(em, 1, 2, 0, 0);
            } else {
                EmRoutineSet(em, 1, 3, 0, 0);
            }
            return;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x13), 0, 5, 1, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    case 4:
        MotionSetCore(em, MOTION(em), ARC(0x12), 0, 5, 1, 0);
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    case 6:
        MotionSetCore(em, MOTION(em), ARC(0x12), 0, 5, 0x41, 0);
        em->xFE++;
    case 7:
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    }
    em23SetWing(em, 1);
}

static void em23_R1_Eat(cEm23* em)
{
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x18), 0, 5, 5, 0);
        em->xFE++;
    case 1:
        if (MotionMoveF(em, 0) && (Rnd() & 1)) {
            em->xFE++;
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x18), 0, 5, 5, 0);
        em->xFE++;
    case 3:
        if (MotionMoveF(em, 0)) {
            if (Rnd() & 1) {
                em->xFE = 0;
            } else {
                em->xFE++;
            }
        }
        break;
    case 4:
        em23WaitMotion(em);
        em->xFE++;
    case 5:
        if (MotionMoveF(em, 0)) {
            em->xFE = 0;
        }
        break;
    }
    if (EM23_PL_NEAR(em, 6250000.0f, 100000000.0f)) {
        EmRoutineSet(em, 1, 2, 0, 0);
    } else {
        em23SetWing(em, 1);
    }
}

static void em23_R0_Damage(cEm23* em)
{
    Em23_R2_move_tbl[em->xFD](em);
}

static void em23_R1_Dm_Air(cEm23* em)
{
    Em23Work* w = EM23_WK(em);

    switch (em->xFE) {
    case 0:
        em->dmType = 0x80;
        MotionSetCore(em, MOTION(em), ARC(0x16), (int) ARC(0x2A), 5, 5, 0);
        if (Rnd() & 1) {
            w->dmRotSpd = PI / 20.0f;
        } else {
            w->dmRotSpd = -PI / 20.0f;
        }
        w->dmAng = em->rot.y;
        w->targetAng = em->pos.y;
        em->xFE++;
    case 1:
        em->rot.y += w->dmRotSpd;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        w->spd.y -= 10.0f;
        w->spd.z *= 0.95f;
        if (em23AddSpeedAir(em, w->dmAng) && em->pos.y >= w->targetAng - 10.0f) {
            SndCall(8, 9, &em->pos, em->id, 0, em);
            if (em->hp > 0) {
                em->xFE++;
            } else {
                EmRoutineSet(em, 3, 0, 0, 0);
            }
        } else {
            w->targetAng = em->pos.y;
            MotionMoveF(em, 0);
        }
        break;
    case 2:
        MotionSetCore(em, MOTION(em), ARC(0x15), 0, 5, 5, 0);
        w->spd.y = 0.0f;
        em->dmType = 0;
        em->xFE++;
    case 3:
        w->dmRotSpd *= 0.9f;
        em->rot.y += w->dmRotSpd;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        w->spd.z *= 0.85f;
        em23AddSpeedAir(em, w->dmAng);
        if (MotionMoveF(em, 0)) {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    em23SetWing(em, 0);
}

static void em23_R0_Die(cEm23* em)
{
    Em23_R3_move_tbl[em->xFD](em);
}

static void em23_R1_Die_Normal(cEm23* em)
{
    Em23Work* w = EM23_WK(em);

    switch (em->xFE) {
    case 0:
        MotionSetCore(em, MOTION(em), ARC(0x14), 0, 5, 1, 0);
        AtariOff(&em->atari, 0xFCFF);
        SndCall(8, 8, &em->pos, em->id, 0, em);
        em->setStatus(8);
        EmSetDropItem(em);
        EmSetDie(em);
        EmReserveDropItem(em);
        em->xFE++;
    case 1:
        if (w->flags & 4) {
            cModelInfo* info;

            for (info = em->pInfo; info; info = info->pNext) {
                if (info->color[0] > 0x20) {
                    info->color[0] -= 0x20;
                }
                info->color[2] = info->color[1] = info->color[0];
            }
        }
        if (MotionMoveF(em, 0)) {
            em->xFE++;
        }
        break;
    }
    em23SetWing(em, 0);
}

int em23AddSpeedAir(cEm23* em, f32 ang)
{
    Em23Work* w = EM23_WK(em);
    Mtx m;
    Vec v;
    Vec a;
    Vec b;
    Vec hit;

    PSMTXRotRad(m, 'y', ang);
    PSMTXMultVecSR(m, &w->spd, &v);
    PSVECAdd(&em->pos, &v, &em->pos);
    if (v.y >= 0.0f) {
        return 0;
    }
    a = em->pos;
    b = em->pos;
    a.y = em->oldPos.y + 500.0f;
    if (SatMgr.hitCheck(&a, &b, &hit, 0, 0, 0) && hit.y > em->pos.y) {
        em->pos.y = hit.y;
        w->spd.y *= 0.5f;
        return 1;
    }
    if (em->pos.y < -100000.0f) {
        em->pos.y = -100000.0f;
        return 1;
    }
    return 0;
}

void em23SetWing(cEm23* em, int on)
{
    Em23Work* w = EM23_WK(em);
    cModelInfo* info;
    void* bin;
    void* tpl;

    if (w->wing == on) {
        return;
    }
    w->wing = on;
    if (w->pWingInfo) {
        em->deleteModelData(w->pWingInfo->pData);
        w->pWingInfo = 0;
    }
    // bin and tpl both assigned in each arm (two-set pseudos, tails cross-jumped into the join with
    // the tpl offset load first); a shared `ARC(8)` after the join gets a gcse PRE copy of subArc
    if (on) {
        bin = ARC(7);
        tpl = ARC(8);
    } else {
        bin = ARC(6);
        tpl = ARC(8);
    }
    info = ModInfoMgr.create(bin, tpl);
    w->pWingInfo = info;
    if (info) {
        em->addModel(info);
    }
}
