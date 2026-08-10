// em2e module (D:/Bio4/Prog/em2e.cpp): a small crawling enemy that wanders (wait / walk / turn),
// or in wall mode (work flag bit1) walks along the surface normal found by the scenario check
// (em2eSetWallMatrix). Dies to a nearby player when pG->flags_5010 bit31 is set or to any damage.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "em2e.h"
#include "emhit.h"
#include "em_set.h"
#include "em_sub.h"
#include "at_mod.h"
#include "atari_init.h"
#include "esp.h"
#include "snd.h"
#include "player.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

typedef void (*Em2eFunc)(cEm2e*);

static void em2e_R0_Init(cEm2e* em);
static void em2e_R0_Move(cEm2e* em);
static void em2e_R1_Wait(cEm2e* em);
static void em2e_R1_Walk(cEm2e* em);
static void em2e_R1_Turn(cEm2e* em);
static void em2e_R1_W_Wait(cEm2e* em);
static void em2e_R1_W_Walk(cEm2e* em);
static void em2e_R1_W_Turn(cEm2e* em);
static void em2e_R0_Damage(cEm2e* em);
static void em2e_R0_Die(cEm2e* em);
static void em2e_R1_Die_Normal(cEm2e* em);

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

extern "C" void _prolog()
{
    EmInitFunc = Em2eInit;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}

void Em2eInit(cEm* em)
{
    new (em) cEm2e();
}

void em2eDmCk(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);

    if (em->hp > 0 && (int) pG->flags_5010 < 0 && !(w->flags & 2)) {
        if ((pPL->pos.x - em->pos.x) * (pPL->pos.x - em->pos.x) + (pPL->pos.y - em->pos.y) * (pPL->pos.y - em->pos.y)
            + (pPL->pos.z - em->pos.z) * (pPL->pos.z - em->pos.z) < 160000.0f) {
            em->hp = 0;
            EstSet((int) em, -1, 0, 0, 0x26, 0, 0, 0, (u32) em, 0);
            SndCall(8, 4, &em->pos, em->id, 0, em);
            EmSetDie(em);
            EmRoutineSet(em, 3, 0, 0, 0);
        }
    }
    if (em->dmHit) {
        em->dmHit = 0;
        em->hp = 0;
        if (w->flags & 2) {
            Vec rot;

            rot.x = 0.0f;
            rot.y = atan2f(w->nrm.x, w->nrm.z);
            rot.z = 0.0f;
            EstSet(0, -1, &em->pos, &rot, 0x26, 1, 0, 0, 0, 0);
        } else {
            EstSet((int) em, -1, 0, 0, 0x26, 0, 0, 0, (u32) em, 0);
        }
        SndCall(8, 4, &em->pos, em->id, 0, em);
        EmSetDie(em);
        EmRoutineSet(em, 3, 0, 0, 0);
    }
}

Em2eFunc Em2e_R0_move_tbl[5] = {
    em2e_R0_Init,
    em2e_R0_Move,
    em2e_R0_Damage,
    em2e_R0_Die,
    (Em2eFunc) Em_R0_Scenario,
};

static Em2eFunc Em2e_R1_move_tbl[6] = {
    em2e_R1_Wait,
    em2e_R1_Walk,
    em2e_R1_Turn,
    em2e_R1_W_Wait,
    em2e_R1_W_Walk,
    em2e_R1_W_Turn,
};

static Em2eFunc Em2e_R3_move_tbl[1] = {
    em2e_R1_Die_Normal,
};

void cEm2e::move()
{
    Em2eWork* w = EM2E_WK(this);

    em2eDmCk(this);
    Em2e_R0_move_tbl[xFC](this);
    if (xFC == 0xFF) {
        EmMgr.destroy(this);
        return;
    }
    if (hp > 0) {
        EmAtCheck(this);
        atari.move();
        if (!(w->flags & 2)) {
            SatMgr.check(this, 0);
        }
    }
}

static void em2e_R0_Init(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);
    cAtariInfo* at;
    int zero;

    switch (em->type) {
    case 0:
    default:
        if (em->modelInit(ARC(4), ARC(5)) == 0) {
            pLog->err(0, 0, "em2e 00() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    case 1:
        if (em->modelInit(ARC(4), ARC(6)) == 0) {
            pLog->err(0, 0, "em2e 01() ModelInit failed.");
            em->xFC = 0xFF;
            return;
        }
        break;
    }
    em->setStatus(1);
    zero = 0;
    at = &em->atari;
    em->be_flag &= ~0x01000000;
    em->setStatus(0xB);
    EspDataLoad((u32) ARC(7), 0x26, 0);
    em->hp = 1;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 500.0f, 500.0f, 500.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    atariInitF(at, 0.0f, 0.0f, 0.0f, 150.0f, 150.0f, 150.0f, 300.0f, 1, 0x2000, 10);   // COMPILER-DIFF: #1
    AtariOff(at, 0xFDFF);
    em->be_flag &= ~0x10;
    YarareInit(em, 0.0f, 0.0f, 0.0f, 100.0f, 50.0f, 1, 1);
    w->flags = zero;
    w->footAng = 0.0f;
    w->nrm.y = 1.0f;
    w->nrm.x = 0.0f;
    w->nrm.z = 0.0f;
    switch (em->x38D) {
    case 0:
    default:
        EmRoutineSet(em, 1, zero, zero, zero);
        break;
    case 1: {
        Mtx m;
        Vec a;
        Vec b;
        Vec hit;
        Vec nrm;

        PSMTXRotRad(m, 'y', em->rot.y);
        TransMatrix(m, &em->pos);
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 0.0f;
        b.x = 0.0f;
        b.y = 0.0f;
        b.z = 2000.0f;
        PSMTXMultVec(m, &a, &a);
        PSMTXMultVec(m, &b, &b);
        if (SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0)) {
            em->pos = hit;
            w->nrm = nrm;
        }
        AtariOff(at, 0xFEFF);
        EmRoutineSet(em, 1, 3, zero, zero);
        break;
    }
    }
    em2e_R0_Move(em);
}

static void em2e_R0_Move(cEm2e* em)
{
    Em2e_R1_move_tbl[em->xFD](em);
}

static void em2e_R1_Wait(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);

    switch (em->xFE) {
    case 0:
        w->timer = Rnd() % 90 + 60;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else if (Rnd() & 1) {
            EmRoutineSet(em, 1, 0, 0, 0);
        } else {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

static void em2e_R1_Walk(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);

    switch (em->xFE) {
    case 0:
        w->timer = Rnd() % 60 + 30;
        em->xFE++;
    case 1: {
        Vec spd;

        spd.x = 0.0f;
        spd.y = 0.0f;
        spd.z = 10.0f;
        PSMTXMultVecSR(em->mat, &spd, &spd);
        PSVECAdd(&em->pos, &spd, &em->pos);
        em2eFootMove(em);
        if (w->timer) {
            w->timer--;
        } else if (Rnd() & 3) {
            EmRoutineSet(em, 1, 0, 0, 0);
        } else {
            EmRoutineSet(em, 1, 2, 0, 0);
        }
        break;
    }
    }
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

static void em2e_R1_Turn(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);

    switch (em->xFE) {
    case 0:
        w->timer = Rnd() % 45 + 15;
        w->turnDir = Rnd() & 1;
        em->xFE++;
    case 1:
        if (w->turnDir) {
            em->rot.y += PI / 64.0f;
        } else {
            em->rot.y -= PI / 64.0f;
        }
        em2eFootMove(em);
        if (w->timer) {
            w->timer--;
        } else if (Rnd() & 3) {
            EmRoutineSet(em, 1, 0, 0, 0);
        } else {
            EmRoutineSet(em, 1, 1, 0, 0);
        }
        break;
    }
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

static void em2e_R1_W_Wait(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);

    w->flags |= 2;
    switch (em->xFE) {
    case 0:
        w->timer = Rnd() % 90 + 60;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            EmRoutineSet(em, 1, 5, 0, 0);
        }
        break;
    }
    em2eSetWallMatrix(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

static void em2e_R1_W_Walk(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);

    w->flags |= 2;
    switch (em->xFE) {
    case 0:
        w->timer = 15;
        em->xFE++;
    case 1: {
        Vec spd;

        spd.x = 0.0f;
        spd.y = 0.0f;
        spd.z = 10.0f;
        PSMTXMultVecSR(em->mat, &spd, &spd);
        PSVECAdd(&em->pos, &spd, &em->pos);
        em2eFootMove(em);
        if (w->timer) {
            w->timer--;
        } else if (Rnd() & 3) {
            EmRoutineSet(em, 1, 3, 0, 0);
        } else {
            EmRoutineSet(em, 1, 5, 0, 0);
        }
        break;
    }
    }
    em2eSetWallMatrix(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

static void em2e_R1_W_Turn(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);

    w->flags |= 2;
    switch (em->xFE) {
    case 0:
        w->timer = 15;
        w->turnDir = em->emsetNo & 1;
        em->xFE++;
    case 1: {
        Mtx m;

        PSMTXRotRad(m, 'y', w->turnDir ? PI / 64.0f : -PI / 64.0f);
        PSMTXConcat(em->mat, m, em->mat);
        TransMatrix(em->mat, &em->pos);
        em2eFootMove(em);
        if (w->timer) {
            w->timer--;
        } else {
            EmRoutineSet(em, 1, 4, 0, 0);
        }
        break;
    }
    }
    em2eSetWallMatrix(em);
    em->partsMatCalc();
    em->partsWorldCalc();
}

static void em2e_R0_Damage(cEm2e* em)
{
    Em2e_R3_move_tbl[em->xFD](em);
}

static void em2e_R0_Die(cEm2e* em)
{
    Em2e_R3_move_tbl[em->xFD](em);
}

static void em2e_R1_Die_Normal(cEm2e* em)
{
    if (em->xFE == 0) {
        AtariOff(&em->atari, 0xFCFF);
        em->be_flag &= ~2;
        em->xFE++;
    }
}

void em2eFootMove(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);
    cModel* p2 = em->getPartsPtr(2);
    cModel* p3 = em->getPartsPtr(3);

    p2->rot.y = w->footAng;
    p3->rot.y = -w->footAng;
    if (w->flags & 1) {
        w->footAng += PI / 64.0f;
        if (w->footAng >= PI / 16.0f) {
            w->footAng = PI / 16.0f;
            w->flags &= ~1;
        }
    } else {
        w->footAng -= PI / 64.0f;
        if (w->footAng <= -PI / 16.0f) {
            w->footAng = -PI / 16.0f;
            w->flags |= 1;
        }
    }
}

void em2eSetWallMatrix(cEm2e* em)
{
    Em2eWork* w = EM2E_WK(em);
    Vec nrm;
    Vec hit;
    Vec a;
    Vec b;
    Vec up;
    Vec axis;
    Mtx m;
    f32 ang;

    PSVECScale(&w->nrm, &a, 500.0f);
    PSVECScale(&w->nrm, &b, -2000.0f);
    PSVECAdd(&em->pos, &a, &a);
    PSVECAdd(&em->pos, &b, &b);
    if (SatMgr.hitCheck(&a, &b, &hit, &nrm, 0, 0)) {
        em->pos = hit;
        w->nrm = nrm;
    }
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    PSMTXMultVecSR(em->mat, &up, &up);
#line 694 "D:/Bio4/Prog/em2e.cpp"
    VECNormalize(&up, &up);
    PSVECCrossProduct(&up, &w->nrm, &axis);
    ang = acosf(PSVECDotProduct(&up, &w->nrm));
    if (ang > 0.001f) {
        PSMTXRotAxisRad(m, &axis, ang);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
}
