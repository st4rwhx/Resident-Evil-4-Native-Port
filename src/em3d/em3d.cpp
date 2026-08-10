// em3d module (D:/Bio4/Prog/em3d.cpp): the support helicopter. It patrols a fixed position table
// (em3d_R1_Patrol), flies to the position the room selects (em3d_R1_TargetMove, setTarget) and
// hovers there shooting its chain guns at the room targets or the enemies it finds
// (em3d_R1_Atk, em3dChainGunMove, em3dGetTargetEm) before it fires a rocket; the rooms drive it
// through the extra virtuals of cEm3d.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "ctrl.h"
#include "em3d.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "est.h"
#include "motion.h"
#include "mes.h"
#include "sce.h"
#include "dbmodule.h"
#include "pad.h"
#include "pl_wep.h"
#include "snd.h"
#include "player.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

typedef void (*Em3dFunc)(cEm3d*);

static void em3d_R0_Init(cEm3d* em);
static void em3d_R0_Move(cEm3d* em);
static void em3d_R1_Patrol(cEm3d* em);
static void em3d_R1_TargetMove(cEm3d* em);
static void em3d_R1_Atk(cEm3d* em);
static void em3d_R1_WarpMove(cEm3d* em);

#define ARC(no) PL_ARC_PTR(em->subArc, no)

// Collision flag bits cleared through the info's address (`addi rX, em, 0x2b4; lhz 0x1a(rX)`).
static inline void AtariOff(cAtariInfo* at, u16 mask) { at->flags &= mask; }

// Routine bytes written through an int inline (player.cpp PlRoutineSet).
static inline void EmRoutineSet(cEm* em, int r0, int r1, int r2, int r3)
{
    em->xFC = r0;
    em->xFD = r1;
    em->xFE = r2;
    em->xFF = r3;
}

// math_sub.h's VECNormalize with the log pointer read as a plain struct member (em27.cpp).
#define VECNormalizeP(src, dst)                                                         \
    if (0.0f == (src)->x && 0.0f == (src)->y && 0.0f == (src)->z) {                    \
        pLog.p->err(0, 0, "VECNormalize:[%s/%d]", __FILE__, __LINE__);                  \
        (dst)->x = (dst)->y = (dst)->z = 0.0f;                                          \
    } else                                                                              \
        PSVECNormalize(src, dst)

// Radio message `no` at the bottom of the screen, held for 90 frames.
static inline void em3dMesSet(Em3dWork* w, int no)
{
    SceMesSet(no, 0xB2, 1, 100, 336 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
    w->mesTimer = 90;
}

// Pilot voice `no`: stop the running one first.
static inline void em3dVoice(cEm3d* em, Em3dWork* w, u16 no)
{
    SndStop(w->sndId, 0);
    w->sndId = SndCall(6, no, &em->pos, 0, 0, em);
}

// Hover: apply and damp the speed, vibrate (a macro for the same reason as EM3D_TURN_TO: the damp
// constant is loaded right before PSVECScale, not held across PSVECAdd).
#define EM3D_HOVER_MOVE(em, w, damp)                                                    \
    PSVECAdd(&(em)->pos, &(w)->spd, &(em)->pos);                                        \
    PSVECScale(&(w)->spd, &(w)->spd, damp);                                             \
    em3dVibMove(em)

// Turn towards `target` by `rate` of the remaining angle, at most `lim` per frame. A macro: an
// inline's constant parameters would stay live across the calls (PI in a callee-saved FPR), the
// original reloads every constant from the pool.
#define EM3D_TURN_TO(em, target, rate, lim, nlim)                                        \
    {                                                                                   \
        f32 a = Muku(&(em)->pos, target, (em)->rot.y, PI) * (rate);                     \
                                                                                        \
        if (a > (lim)) {                                                                \
            a = (lim);                                                                  \
        }                                                                               \
        if (a < (nlim)) {                                                               \
            a = (nlim);                                                                 \
        }                                                                               \
        (em)->rot.y += a;                                                               \
        (em)->rot.y = LIMIT_ANGLE((em)->rot.y);                                         \
    }

// World matrix from the coordinates, then the parts.
static inline void em3dMatCalc(cEm3d* em)
{
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
}

extern "C" void _prolog()
{
    OSReport("em3d prolog Ok\n");
    EmInitFunc = Em3dInit;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void cEm3d::setNoSuspend(int on)
{
    Em3dWork* w = EM3D_WK(this);
    u32 i;

    if (on) {
        be_flag |= 0x800;
    } else {
        be_flag &= ~0x800;
    }
    for (i = 0; i < 4; i++) {
        if (w->pMissile[i]) {
            w->pMissile[i]->setNoSuspend(on);
        }
    }
}

void Em3dInit(cEm* em)
{
    new (em) cEm3d();
}

void em3dDmCk(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    int near;
    u8 wep;

    if (em->dmHit == 0) {
        return;
    }
    em->dmHit = 0;
    if (w->mesTimer) {
        return;
    }
    near = 0;
    if ((em->x328.x - pPL->pos.x) * (em->x328.x - pPL->pos.x) + (em->x328.y - pPL->pos.y) * (em->x328.y - pPL->pos.y)
            + (em->x328.z - pPL->pos.z) * (em->x328.z - pPL->pos.z)
        < 9000000.0f) {
        near = 1;
    }
    wep = em->dmWep;
    if (wep == 0xD) {
        near = 1;
    }
    if (wep == 0x12) {
        near = 1;
    }
    if (wep == 0x13) {
        near = 1;
    }
    if (near) {
        em3dVoice(em, w, 0x6D);
        em3dMesSet(w, 3);
    } else {
        em3dVoice(em, w, 0x6C);
        em3dMesSet(w, 2);
    }
}

Em3dFunc Em3d_R0_move_tbl[4] = {
    em3d_R0_Init,
    em3d_R0_Move,
    NULL,
    NULL,
};

static Em3dFunc Em3d_R1_move_tbl[4] = {
    em3d_R1_Patrol,
    em3d_R1_TargetMove,
    em3d_R1_Atk,
    em3d_R1_WarpMove,
};

// Parts the four rockets hang on.
static u8 em3d_missile_parts[4] = { 0xC, 0xD, 0xE, 0xF };

// Flight positions and the targets shot at from them (setTarget's no).
Vec Em3d_pos_tbl[8] = {
    { 64477.0f, 16694.0f, 39279.0f },
    { 71742.0f, 18189.0f, 6156.0f },
    { 76500.0f, 21158.0f, 11073.0f },
    { 78400.0f, 22195.0f, 18985.0f },
    { 30104.0f, 18091.0f, -7604.0f },
    { 38118.0f, 17783.0f, -12089.0f },
    { 39018.0f, 17783.0f, -11119.0f },
    { 38763.0f, 22559.0f, -319.0f },
};

Vec Em3d_target_tbl[8] = {
    { 60169.0f, 12923.0f, 27734.0f },
    { 69984.0f, 11609.0f, 25335.0f },
    { 61087.0f, 12269.0f, 14072.0f },
    { 76253.0f, 15319.0f, 6588.0f },
    { 41774.0f, 10000.0f, -12444.0f },
    { 35073.0f, 10920.0f, 730.0f },
    { 27033.0f, 6732.0f, -13125.0f },
    { 25807.0f, 13198.0f, -13498.0f },
};
// The module's .data is padded to 8 bytes after the last table.
asm(".section .data\n\t.balign 8\n\t.text");

void cEm3d::move()
{
    Em3dWork* w = EM3D_WK(this);

    if (xFC) {
        em3dDmCk(this);
    }
    w->flags &= ~0x47;
    if (w->mesTimer) {
        w->mesTimer--;
    }
    Em3d_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    em3dTargetEmUpdate(this);
    em3dHeliPitchMove(this);
    em3dRoterMove(this);
    partsWorldCalc();
    hp = 0;
    em3dChainGunMove(this);
    hp = 1;
    EmAtCheck(this);
    atari.move();
    SatMgr.check(this, 0);
    {
        Vec target = Em3d_target_tbl[w->targetNo];

        if (w->flags & 8) {
            em3dVoice(this, w, 0x6C);
            em3dMesSet(w, 2);
        }
    }
    w->flags &= ~0x8;
}

static void em3d_R0_Init(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    cAtariInfo* at;
    u32 i;
    int zero;

    if (em->modelInit(ARC(5), ARC(6)) == 0) {
        pLog->err(0, 0, "em3d() ModelInit failed.");
        em->xFC = 0xFF;
        return;
    }
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 10000.0f, 10000.0f, 10000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    at = &em->atari;
    at->init(1, 0x2000, 10, 0.0f, 0.0f, 0.0f, 800.0f, 700.0f, 700.0f, 3000.0f);
    zero = 0;
    AtariOff(at, 0xFCFF);
    em->setStatus(1);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    em->setStatus(0xB);
    YarareInit(em, 0.0f, 750.0f, -3000.0f, 1500.0f, 6000.0f, 1, 0x45);
    em->lockParts = 2;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    EspDataLoad((u32) ARC(4), 0x32, 0);
    w->flags = zero;
    w->gunTimer = zero;
    w->vibAng.x = fRand1_1() * PI;
    w->vibAng.y = fRand1_1() * PI;
    w->vibAng.z = fRand1_1() * PI;
    w->vibSpd.x = fRand0_1() * 0.17453292f + 0.17453292f;
    w->vibSpd.y = fRand0_1() * 0.05235988f + 0.13962634f;
    w->vibSpd.z = fRand0_1() * 0.05235988f + 0.2443461f;
    w->range = 12000.0f;
    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    w->targetNo = 0;
    w->pTargetEm = 0;
    w->x2BC = 150;
    for (i = 0; i < 4; i++) {
        Vec pos;
        Vec rot;

        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 0.0f;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = 0.0f;
        w->pMissile[i] = SetHeliMissile(ARC(7), ARC(8), &pos, &rot, 0);
        if (w->pMissile[i]) {
            ((cObjMissile*) w->pMissile[i])->setParent(em, em3d_missile_parts[i], 0);
        }
    }
    w->patrolPos = Em3d_pos_tbl[0];
    EstSet((int) em, -1, 0, 0, 0x32, 0, 1, 0, (u32) em, 0);
    EstSet((int) em, -1, 0, 0, 0x32, 3, 1, 0, (u32) em, 0);
    EmRoutineSet(em, 1, 0, 0, 0);
    SndCall(6, 0, &em->pos, 0, 0, em);
    em3d_R0_Move(em);
}

static void em3d_R0_Move(cEm3d* em)
{
    Em3d_R1_move_tbl[em->xFD](em);
}

static void em3d_R1_Patrol(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    Vec target;

    w->flags |= 0x40;
    target = w->patrolPos;
    switch (em->xFE) {
    case 0:
        if (em->xFF) {
            w->timer = 30;
        } else {
            w->timer = 0;
        }
        em->xFF = 0;
        w->flags &= ~0x10;
        w->count = Rnd() % 90 + 90;
        w->pTargetEm = 0;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->flags |= 2;
        }
        EM3D_HOVER_MOVE(em, w, 0.95f);
        EM3D_TURN_TO(em, &target, 0.05f, 0.034906585f, -0.034906585f);
        if (w->count) {
            w->count--;
        } else {
            em->xFE++;
        }
        break;
    case 2:
        w->timer = Rnd() % 30 + 120;
        em->xFE++;
    case 3: {
        Vec v;

        w->flags |= 2;
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 30.0f;
        if (em->pos.y < 25000.0f) {
            v.y = 15.0f;
        }
        PSMTXMultVecSR(em->mat, &v, &v);
        PSVECAdd(&w->spd, &v, &w->spd);
        em->rot.y += 0.012271847f;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        EM3D_HOVER_MOVE(em, w, 0.95f);
        if (w->flags & 0x20) {
            if ((w->patrolPos.x - pPL->pos.x) * (w->patrolPos.x - pPL->pos.x)
                    + (w->patrolPos.y - pPL->pos.y) * (w->patrolPos.y - pPL->pos.y)
                    + (w->patrolPos.z - pPL->pos.z) * (w->patrolPos.z - pPL->pos.z)
                > 25000000.0f) {
                w->flags |= 1;
            }
        }
        if (w->timer) {
            w->timer--;
        } else {
            em->xFE = 0;
        }
        break;
    }
    }
    em3dMatCalc(em);
    if (w->targetSet) {
        // Plain byte stores: one QI zero serves targetSet and the routine bytes (the int inline's SI zero
        // would be a second `li`).
        w->targetSet = 0;
        em->xFC = 1;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
    }
}

static void em3d_R1_TargetMove(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    Vec v;
    Vec target = Em3d_target_tbl[w->targetNo];
    Vec pos = Em3d_pos_tbl[w->targetNo];

    switch (em->xFE) {
    case 0:
        em3dVoice(em, w, 0x69);
        em->xFE++;
    case 1:
        EM3D_TURN_TO(em, &pos, 0.1f, 0.034906585f, -0.034906585f);
        EM3D_HOVER_MOVE(em, w, 0.95f);
        if (fabsf(Muku(&em->pos, &pos, em->rot.y, PI)) < 0.034906585f) {
            em->xFE++;
        }
        break;
    case 2:
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        em->xFE++;
    case 3:
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 30.0f;
        if (em->pos.y > pos.y + 500.0f) {
            v.y = -15.0f;
        }
        if (em->pos.y < pos.y - 500.0f) {
            v.y = 15.0f;
        }
        PSMTXMultVecSR(em->mat, &v, &v);
        PSVECAdd(&w->spd, &v, &w->spd);
        EM3D_TURN_TO(em, &pos, 0.05f, 0.034906585f, -0.034906585f);
        EM3D_HOVER_MOVE(em, w, 0.95f);
        if ((em->pos.x - pos.x) * (em->pos.x - pos.x) + (em->pos.z - pos.z) * (em->pos.z - pos.z) < 9000000.0f) {
            w->pTargetEm = 0;
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    em3dMatCalc(em);
    if (pG->debug_mode == 7) {
        Draw_line3d(&em->pos, &pos, -1, 0);
        Draw_sphere(&target, 8000.0f, -1, 1, 1);
    }
}

static void em3d_R1_Atk(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    Vec target = Em3d_target_tbl[w->targetNo];
    Vec pos = Em3d_pos_tbl[w->targetNo];

    switch (em->xFE) {
    case 0:
        w->count = 0;
        w->timer = 240;
        w->mesDone = 0;
        em->xFE++;
    case 1:
        if (em->pos.y > pos.y + 500.0f) {
            w->spd.y = -15.0f;
        }
        if (em->pos.y < pos.y - 500.0f) {
            w->spd.y = 15.0f;
        }
        EM3D_HOVER_MOVE(em, w, 0.93f);
        EM3D_TURN_TO(em, &target, 0.05f, 0.034906585f, -0.034906585f);
        if (w->pTargetEm == 0) {
            em3dGetTargetEm(em);
            if (w->pTargetEm) {
                w->count++;
            }
        }
        if (w->timer == 0) {
            em3dRocketFire(em);
            em->xFE++;
            break;
        }
        w->timer--;
        if (w->mesTimer == 0
            && (pPL->pos.x - target.x) * (pPL->pos.x - target.x) + (pPL->pos.y - target.y) * (pPL->pos.y - target.y)
                    + (pPL->pos.z - target.z) * (pPL->pos.z - target.z)
                < 64000000.0f) {
            w->sndId = SndCall(6, 0x6E, &em->pos, 0, 0, em);
            em3dMesSet(w, 4);
            if (w->mesDone == 0) {
                w->mesDone = 1;
                if (w->timer > 30 && w->timer < 240) {
                    w->timer = 240;
                }
            }
        }
        if (w->timer < 30) {
            w->flags |= 4;
        }
        break;
    case 2:
        w->timer = 45;
        em->xFE++;
    case 3:
        if (em->pos.y > pos.y + 500.0f) {
            w->spd.y = -30.0f;
        }
        if (em->pos.y < pos.y - 500.0f) {
            w->spd.y = 30.0f;
        }
        EM3D_HOVER_MOVE(em, w, 0.95f);
        EM3D_TURN_TO(em, &target, 0.1f, 0.20943952f, -0.20943952f);
        if (w->timer) {
            w->timer--;
            break;
        }
        w->targetNo++;
        if (w->targetNo > 7) {
            w->targetNo = 0;
        }
        if ((s16) pG->pl_life > 0 && w->mesTimer == 0) {
            if (w->count > 2) {
                w->sndId = SndCall(6, 0x72, &em->pos, 0, 0, em);
                em3dMesSet(w, 6);
            } else {
                w->sndId = SndCall(6, 0x71, &em->pos, 0, 0, em);
                em3dMesSet(w, 5);
            }
        }
        EmRoutineSet(em, 1, 0, 0, 1);
        break;
    }
    em3dMatCalc(em);
    if (fabsf(Muku(&em->pos, &target, em->rot.y, PI)) < 0.5235988f) {
        w->flags |= 1;
    }
}

static void em3d_R1_WarpMove(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    Mtx m;
    Vec target = Em3d_target_tbl[w->targetNo];
    Vec pos = Em3d_pos_tbl[w->targetNo];

    switch (em->xFE) {
    case 0:
        PSMTXRotRad(m, 'y', em->rot.y);
        w->spd.x = 1000.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        PSMTXMultVecSR(m, &w->spd, &w->spd);
        w->timer = 45;
        em->xFE++;
    case 1:
        EM3D_TURN_TO(em, &pos, 0.1f, 0.034906585f, -0.034906585f);
        Muku(&em->pos, &pos, em->rot.y, PI);
        EM3D_HOVER_MOVE(em, w, 0.95f);
        if (w->timer) {
            w->timer--;
        } else {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    em3dMatCalc(em);
    if (pG->debug_mode == 7) {
        Draw_line3d(&em->pos, &pos, -1, 0);
        Draw_sphere(&target, 8000.0f, -1, 1, 1);
    }
}

void em3dRoterMove(cEm3d* em)
{
    cModel* p;

    p = em->getPartsPtr(0xA);
    p->rot.y += 0.61086524f;
    p->rot.y = LIMIT_ANGLE(p->rot.y);
    p = em->getPartsPtr(0xB);
    p->rot.x -= 0.61086524f;
    p->rot.y = LIMIT_ANGLE(p->rot.y);
}

// Aim one gun mount at `aim`: the mount (parts `gun`) pitches, the barrel (parts `gun` + 1) yaws,
// each towards the helicopter's own angle plus the offset to the target, limited to +/- lim. Macros
// (like EM3D_TURN_TO): the constants are loaded where they are used and shared by cse between the
// two identical mounts, an inline's parameters would be live across the whole function. The
// temporaries (p, len, angX, angY, m) are the FUNCTION's variables: one multi-set `angY` pseudo
// conflicts with every shared clamp constant and is allocated after `limY` (f29 below f30);
// macro-local variables give a short block-local angY that takes f30 first.
#define EM3D_GUN_AIM(em, aim, d, mount, gun, rotX, rotY, limX, nlimX, limY, nlimY)     \
    {                                                                                   \
        p = (em)->getPartsPtr(mount);                                                   \
        PSVECSubtract(aim, &p->worldPos, &(d));                                         \
        len = SQRTF((d).x * (d).x + (d).z * (d).z);                                     \
        angX = -atan2f((d).y, len);                                                     \
        angY = atan2f((d).x, (d).z);                                                    \
        p = (em)->getPartsPtr(gun);                                                     \
        m = Muku2(rotX, angX, limX);                                                    \
        p->rot.x += Muku2(p->rot.x, m, 0.024543693f);                                   \
        if (p->rot.x > (limX)) {                                                        \
            p->rot.x = (limX);                                                          \
        }                                                                               \
        if (p->rot.x < (nlimX)) {                                                       \
            p->rot.x = (nlimX);                                                         \
        }                                                                               \
        p->rot.x = LIMIT_ANGLE(p->rot.x);                                               \
        p = (em)->getPartsPtr((gun) + 1);                                               \
        m = Muku2(rotY, angY, limY);                                                    \
        p->rot.y += Muku2(p->rot.y, m, 0.024543693f);                                   \
        if (p->rot.y > (limY)) {                                                        \
            p->rot.y = (limY);                                                          \
        }                                                                               \
        if (p->rot.y < (nlimY)) {                                                       \
            p->rot.y = (nlimY);                                                         \
        }                                                                               \
        p->rot.y = LIMIT_ANGLE(p->rot.y);                                               \
    }

// One chain gun shot from parts `mount`: a random line ahead, the weapon hit check, then the wall
// hit effect (and its sound for the first gun). Uses the function's `p` (a multi-set pseudo):
// local-alloc then cannot tie it to the `&p->mat` addi, and it keeps its own callee-saved r31.
#define EM3D_GUN_SHOT(em, mount, spread, len, se, zero)                                 \
    {                                                                                   \
        p = (em)->getPartsPtr(mount);                                                   \
                                                                                        \
        a.x = 0.0f;                                                                     \
        a.y = 0.0f;                                                                     \
        a.z = 0.0f;                                                                     \
        b.x = fRand1_1() * (spread);                                                    \
        b.y = fRand1_1() * (spread);                                                    \
        b.z = (len);                                                                    \
        PSMTXMultVec(p->mat, &a, &a);                                                   \
        PSMTXMultVec(p->mat, &b, &b);                                                   \
        PlWepHitCheck2(0, &a, &b, 0xA, 3, 6000.0f);                                     \
        if (EatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0)) {                                \
            f32 l = SQRTF(nrm.x * nrm.x + nrm.z * nrm.z);                               \
                                                                                        \
            rot.x = -atan2f(nrm.y, l);                                                  \
            rot.y = atan2f(nrm.x, nrm.z);                                               \
            rot.z = 0.0f;                                                               \
            PSVECScale(&nrm, &s, 30.0f);                                                \
            PSVECAdd(&hit, &s, &hit);                                                   \
            EstSet(0, -1, &hit, &rot, 0x32, 6, 0, 0, zero, (void*) (zero));             \
            if (se) {                                                                   \
                SndCall(6, 0xA, &hit, 0, 0, em);                                        \
            }                                                                           \
        }                                                                               \
    }

void em3dChainGunMove(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    Vec target;
    Vec aim;
    Vec d;
    Vec a;
    Vec b;
    Vec hit;
    Vec s;
    Vec nrm;
    Vec rot;
    f32 rotX;
    f32 rotY;
    cModel* p;
    f32 len;
    f32 angX;
    f32 angY;
    f32 m;
    int noAim;
    int t;

    if (w->flags & 0x40) {
        target = w->patrolPos;
        aim = target;
    } else {
        target = Em3d_target_tbl[w->targetNo];
        if ((w->flags & 1) && w->pTargetEm) {
            aim = w->pTargetEm->pos;
            aim.y += 1000.0f;
        }
    }
    rotX = em->getPartsPtr(0)->rot.x;
    rotY = em->rot.y;
    EM3D_GUN_AIM(em, &aim, d, 4, 2, rotX, rotY, 0.5235988f, -0.5235988f, 0.34906584f, -0.34906584f);
    EM3D_GUN_AIM(em, &aim, d, 7, 5, rotX, rotY, 0.5235988f, -0.5235988f, 0.34906584f, -0.34906584f);
    p = em->getPartsPtr(4);
    p->rot.z += 0.34906584f;
    p->rot.z = LIMIT_ANGLE(p->rot.z);
    p = em->getPartsPtr(7);
    p->rot.z += 0.34906584f;
    p->rot.z = LIMIT_ANGLE(p->rot.z);
    EM3D_GUN_AIM(em, &target, d, 9, 8, rotX, rotY, 0.7853982f, -0.7853982f, 0.7853982f, -0.7853982f);
    noAim = !(w->flags & 1);
    if (noAim) {
        return;
    }
    t = w->gunTimer;
    if (t) {
        w->gunTimer--;
        return;
    }
    w->gunTimer = 2;
    EstSet((int) em, -1, 0, 0, 0x32, 1, 1, 0, (u32) em, (void*) t);
    EstSet((int) em, -1, 0, 0, 0x32, 2, 1, 0, (u32) em, (void*) t);
    SndCall(6, 1, &em->pos, 0, 0, em);
    EM3D_GUN_SHOT(em, 4, 5000.0f, 100000.0f, 1, t);
    EM3D_GUN_SHOT(em, 7, 2000.0f, 300000.0f, 0, t);
}

void em3dHeliPitchMove(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    cModel* p;
    f32 v;
    f32 ang;

    p = em->getPartsPtr(0);
    v = SQRTF(w->spd.x * w->spd.x + w->spd.z * w->spd.z) * 0.01f;
    if (v > 1.0f) {
        v = 1.0f;
    }
    ang = v * 0.34906584f + 0.2617994f;
    p->rot.x = p->rot.x * 0.9f + ang * 0.1f;
}

void em3dVibMove(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);

    w->vibAng.x += w->vibSpd.x;
    em->pos.x += SINF(w->vibAng.x) * 20.0f;
    w->vibAng.y += w->vibSpd.y;
    em->pos.y += SINF(w->vibAng.y) * 10.0f;
    w->vibAng.z += w->vibSpd.z;
    em->pos.z += SINF(w->vibAng.z) * 20.0f;
}

int em3dGetTargetEm(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    Vec target = Em3d_target_tbl[w->targetNo];
    Vec a;
    Vec b;
    Vec dir;
    Vec d;
    u32 i;

    PSVECSubtract(&target, &em->pos, &dir);
#line 1218 "D:/Bio4/Prog/em3d.cpp"
    VECNormalizeP(&dir, &dir);
    a = em->pos;
    w->pTargetEm = 0;
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        int dead = !(e->be_flag & 1);

        if (dead) {
            continue;
        }
        if (!(e->be_flag & 0x20)) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x40) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        if ((target.x - e->pos.x) * (target.x - e->pos.x) + (target.y - e->pos.y) * (target.y - e->pos.y)
                + (target.z - e->pos.z) * (target.z - e->pos.z)
            > w->range * w->range) {
            continue;
        }
        PSVECSubtract(&e->pos, &em->pos, &d);
#line 1240 "D:/Bio4/Prog/em3d.cpp"
        VECNormalizeP(&d, &d);
        if (acosf(PSVECDotProduct(&dir, &d)) > 0.5235988f) {
            continue;
        }
        b = e->pos;
        b.y += 1000.0f;
        if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x404000) == 0) {
            w->pTargetEm = e;
            return 1;
        }
    }
    return 0;
}

void em3dTargetEmUpdate(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    cEm* e = w->pTargetEm;

    if (e) {
        if (e->hp > 0) {
            Vec a = em->pos;
            Vec b = e->pos;

            b.y += 1000.0f;
            if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0x404000) == 0) {
                return;
            }
        }
        w->pTargetEm = 0;
    }
}

void em3dRocketFire(cEm3d* em)
{
    Em3dWork* w = EM3D_WK(em);
    Vec target = Em3d_target_tbl[w->targetNo];
    int no;

    // A loop left at its first iteration: the index stays a register (`lwzx`/`stwx` with the zero)
    // and the missile load is not hoisted above the target copy.
    for (no = 0; no < 4; no++) {
        if (w->pMissile[no] == 0) {
            Vec pos;
            Vec rot;
            cObjMissile* m;

            pos.x = 0.0f;
            pos.y = 0.0f;
            pos.z = 0.0f;
            rot.x = 0.0f;
            rot.y = 0.0f;
            rot.z = 0.0f;
            m = (cObjMissile*) SetHeliMissile(ARC(7), ARC(8), &pos, &rot, 0);
            if (m) {
                m->setParent(em, em3d_missile_parts[no], 0);
                m->setFire(&target);
            }
        } else {
            ((cObjMissile*) w->pMissile[no])->setFire(&target);
            w->pMissile[no] = 0;
        }
        break;
    }
}

int cEm3d::ckSelectEnable()
{
    int ret = 0;

    if (EM3D_WK(this)->flags & 2) {
        ret = 1;
    }
    return ret;
}

void cEm3d::setTarget(u32 no, f32 range)
{
    Em3dWork* w = EM3D_WK(this);

    w->targetSet = 1;
    w->flags |= 0x10;
    w->targetNo = no;
    if (no > 7) {
        w->targetNo = 7;
    }
    w->range = range;
}

void cEm3d::setTargetPos(u32 no, Vec* pos, f32 rotY, f32 range)
{
    Em3dWork* w = EM3D_WK(this);

    w->targetSet = 1;
    w->targetNo = no;
    if (no > 7) {
        w->targetNo = 7;
    }
    w->flags |= 0x10;
    w->range = range;
    rot.y = rotY;
    setPos(pos);
    // Plain byte stores: the QI one of targetSet is reused for xFC (an int inline's SI one is a second `li`).
    xFC = 1;
    xFD = 3;
    xFE = 0;
    xFF = 0;
}

int cEm3d::ckMissileFire()
{
    int ret = 0;

    if (EM3D_WK(this)->flags & 4) {
        ret = 1;
    }
    return ret;
}

void cEm3d::setEmLocked()
{
    EM3D_WK(this)->flags |= 8;
}

void cEm3d::setPatrolPos(Vec* pos)
{
    if (pos) {
        EM3D_WK(this)->patrolPos = *pos;
    }
}

void cEm3d::setFreeFire()
{
    EM3D_WK(this)->flags |= 0x20;
}
