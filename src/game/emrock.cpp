// game/emrock.cpp: rolling rock enemy (cEmRock): boulders that hang on a parent, fall, get
// thrown, roll after the player (with the escape event) or drop on him.
//
// Byte-identical (DOL sweep 23b). emRockDropCamMove sets `up` before `len` (sched1's 32-entry
// pending-memory flush otherwise lands on the up.x store).
//
// Camera tails: `Camera* cam = &emRockCam;` is declared BEFORE the `cp`/`ca` pointers. cse rewrites
// `&emRockCam` from the OLDEST related constant (`emRockCam+K`) whose class still holds a register
// (use_related_value walks the ring from the base symbol): with `ca = &emRockCam.param.at` declared
// first, `cam` came out `ca - 176`; the original has `cp - 164` (PushCamMove: the `pos = p` block
// copy's address pseudo) or a fresh `lis/addi` (EscapeCamMove2/DropDieCamMove: the PosToPos/PSVECAdd
// argument registers were clobbered by the calls), i.e. `cam` was computed before `ca` existed.

#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "map_obj.h"
#include "widget.h"
#include "emrock.h"
#include "emhit.h"
#include "at_mod.h"
#include "esp.h"
#include "snd.h"
#include "quake.h"
#include "pad.h"
#include "main.h"
#include "act_btn.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "cockpit.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "route_ck.h"
#include "game.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" {
int MotionMove(cModel* m, int a);
void EffectEspgenDelete(int a, int b, cModel* m);
int EmAtkHitCk(void* info, Vec* a, Vec* b, int flag);   // em_sub.cpp
}
void MotionSetCore(cModel* m, void* w, void* data, int seq, int hokan, int flags, int frame);   // motion.cpp (C++ linkage)
// cGameSave::save is `save(void*)` by name but the original reads a second argument (-1 here);
// ABI-identical redeclaration (dvd.h ReadCheckInfo).
int GameSaveSave(cGameSave* g, void* data, int mode) asm("save__9cGameSavePv");

// setYarareCube(0, 400, 800, 400) with the float arguments' moves issued before the `li r4, 0`
// (atari_init.h: GCC emits the argument moves in declaration order).
void setYarareCubeF(cEmRock* em, f32 x, f32 y, f32 z, Vec* size) asm("setYarareCube__7cEmRockP3Vecfff");

// The rock the player damage callbacks belong to: the original re-reads pl->dmgType at every use.
#define PL_ROCK(pl) ((cEmRock*) (pl)->dmgType)

// Head of a key-frame motion data block (motion.h MotionData; motion.h's one-argument MotionMove
// prototype keeps it out of the em units).
struct RockMotData {
    u16 maxFrame;   // 0x00
};

// lockParts = 0 through an int parameter: the zero becomes an SImode pseudo shared with the
// later `= 0` stores (emmine SetMine).
static inline void LockPartsSet(cEm* em, int no)
{
    em->lockParts = no;
}

// Struct-member view of pPL (the pGS trick): the load stays below the preceding atari flag store.
struct PlayerPtr {
    cPlayer* p;
};
#define PLS (((PlayerPtr*) &pPL)->p)

typedef void (*EmRockFunc)(cEmRock*);

extern "C" void emRock_R0_Move(cEmRock* em);
extern "C" void emRock_R1_Lost(cEmRock* em);

EmRockFunc EmRock_R0_move_tbl[4] = {
    emRock_R0_Init,
    emRock_R0_Move,
    0,
    0,
};

static EmRockFunc EmRock_R1_move_tbl[9] = {
    emRock_R1_Set,
    emRock_R1_Lost,
    emRock_R1_Parent,
    emRock_R1_Fall,
    emRock_R1_Throw,
    emRock_R1_Throw2,
    emRock_R1_Roll,
    emRock_R1_Drop,
    emRock_R1_Drop2,
};

// Attack parameters of a falling / thrown rock without its own (setFall / setThrow); range = radius.
static EmAtkInfo emRockAtk = { 1500.0f, 8, 9999, 0, 10, 0 };

// Event camera of the escape / drop scenes (CamCtrl.x250 points at it while they run).
static Camera emRockCam = { 0 };

cEmRock* SetRock(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type)
{
    cEmRock* em;
    EmRockWork* w;

    em = (cEmRock*) EmMgr.createBack(0x4A);
    if (em == 0) {
        return 0;
    }
    w = EMROCK_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->rot = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetRock() ModelInit failed.");
        EmMgr.destroy(em);
        return 0;
    }
    em->type = type;
    switch (em->type) {
    case 0:
        break;
    case 1:
        em->scale.x = 4.2f;
        em->scale.y = 4.2f;
        em->scale.z = 4.2f;
        break;
    }
    if (em->type != 3) {
        atariInitF(&em->atari, 0.0f, -(em->scale.y * 1200.0f) * 0.5f, 0.0f, em->scale.x * 1200.0f * 0.5f,
                   em->scale.x * 1200.0f * 0.5f, em->scale.x * 1200.0f * 0.5f, em->scale.y * 1200.0f * 0.5f, 0, 0x2000, 10);
    } else {
        atariInitF(&em->atari, 0.0f, 2000.0f, 0.0f, 2700.0f, 2700.0f, 2700.0f, 2000.0f, 0, 0x2000, 10);
    }
    em->hp = 1000;
    em->hpMax = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 10000.0f, 10000.0f, 10000.0f };

        if (type != 1 && type != 3) {
            em->lightInfo.init2(0, 1, &ofs, &size, 0x10);
        } else {
            em->lightInfo.init2(0, 1, &ofs, &size, 8);
        }
    }
    // COMPILER-DIFF: #13 (dying-store shape): the original's zero pseudo (REG_EQUIV 0, never
    // allocated, reloaded per label region as `li r30, 0`) does not die at `seAlways[2] = 0`, so the
    // store block comes out in source order; ours allocates the pseudo and would hoist that dying
    // store to the block top. The volatile use after the block keeps our pseudo live past it; the
    // second block's literal zeros are the fresh post-label `li r30, 0` of the original.
    int zero;
    zero = 0;
    LockPartsSet(em, zero);
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(1);
    em->setStatus(0xB);
    em->be_flag &= ~0x01000000;
    em->atari.setPriority(3);
    em->atari.clrFlag100();
    em->be_flag &= ~0x10;
    w->alwaysWait = 4;
    w->sndId = zero;
    w->flags = zero;
    w->x24 = zero;
    w->pParent = (cEm*) zero;
    w->x30 = zero;
    w->pAtk = (EmAtkInfo*) zero;
    w->xA1 = zero;
    w->se8C = zero;
    w->seFall[0] = 0xFF;
    w->seFall[1] = 0xFF;
    w->seFall[2] = zero;
    w->seFall[3] = zero;
    w->se8D[0] = 0xFF;
    w->se8D[1] = 0xFF;
    w->se8D[2] = zero;
    w->se97[0] = 0xFF;
    w->se97[1] = 0xFF;
    w->se97[2] = zero;
    w->se90[0] = 0xFF;
    w->se90[1] = 0xFF;
    w->se90[2] = zero;
    w->seAlways[0] = 0xFF;
    w->seAlways[1] = 0xFF;
    w->seAlways[2] = zero;
    w->effFall[0] = 0xFF;
    w->effFall[1] = 0xFF;
    w->eff9E[0] = 0xFF;
    w->eff9E[1] = 0xFF;
    w->eff9C[0] = 0xFF;
    w->eff9C[1] = 0xFF;
    asm volatile("" : : "r"(zero)); // COMPILER-DIFF: #13
    if (em->type != 3) {
        w->radius = em->scale.x * 600.0f;
    } else {
        w->radius = 2000.0f;
    }
    w->started = 0;
    w->grav = 20.0f;
    w->rollWait = 0;
    em->pMotion = (void*) 0;
    w->plMot[2] = (void*) 0;
    w->plMot[3] = (void*) 0;
    w->plMot[4] = (void*) 0;
    w->plMot[5] = (void*) 0;
    w->plMot[6] = (void*) 0;
    w->plMot[7] = (void*) 0;
    w->plMot[8] = (void*) 0;
    w->plMot[9] = (void*) 0;
    w->plMot[10] = (void*) 0;
    w->plMot[11] = (void*) 0;
    w->mot1 = (void*) 0;
    w->mot0 = (void*) 0;
    w->mot2 = (void*) 0;
    w->mot3 = (void*) 0;
    w->pSat = (cSat*) 0;
    w->espKind = EspPullCoreKind();
    em->setStatus(5);
    em->flags_3C8 &= ~1;
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
    emRock_R0_Move(em);
    return em;
}

void cEmRock::beginEvent()
{
}

void emRockDmCk(cEmRock* em)
{
    if (em->dmHit == 0) {
        return;
    }
    em->dmHit = 0;
}

void cEmRock::move()
{
    EmRockWork* w = EMROCK_WK(this);

    emRockDmCk(this);
    EmRock_R0_move_tbl[xFC](this);
    if ((be_flag & 0x201) != 1) {
        return;
    }
    EmAtCheck(this);
    atari.move();
    if (w->pParent) {
        alpha = w->pParent->alpha;
        x158 = w->pParent->x158;
        if (w->pParent->be_flag & 2) {
            be_flag |= 2;
        } else {
            be_flag &= ~2;
        }
    }
    if (w->flags & 2) {
        be_flag &= ~2;
    }
    emRockSatSet(this);
}

void emRock_R0_Init(cEmRock* em)
{
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
}

void emRock_R0_Move(cEmRock* em)
{
    EmRock_R1_move_tbl[em->xFD](em);
}

void emRock_R1_Set(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);

    if (em->pMotion) {
        MotionMove(em, 0);
    } else {
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        em->partsMatCalc();
    }
    em->partsWorldCalc();
    switch (em->xFE) {
    case 0:
        em->xFE++;
        break;
    case 1:
        if (w->started == 0 && em->type == 1) {
            if (emRockRollStartCk(em)) {
                w->started = 1;
                em->flags_3C8 |= 1;
                em->xFC = 1;
                em->xFD = 6;
                em->xFE = 0;
                em->xFF = 0;
            }
        }
        break;
    }
}

void emRock_R1_Lost(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);

    switch (em->xFE) {
    case 0:
        em->hp = 0;
        em->be_flag &= ~2;
        em->clearStatus(5);
        EffectEspgenDelete(0, w->espKind, em);
        em->xFE++;
        w->timer = 30;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            EmMgr.destroy(em);
        }
        break;
    }
}

void emRock_R1_Parent(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    cEm* parent = w->pParent;
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;

    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    if (parent && parent->pParts) {
        PSMTXConcat(parent->getPartsPtr(w->partsNo)->mat, em->mat, m);
        if (!(w->flags & 1)) {
            v0.x = m[0][0];
            v0.y = m[1][0];
            v0.z = m[2][0];
            v1.x = m[0][1];
            v1.y = m[1][1];
            v1.z = m[2][1];
            v2.x = m[0][2];
            v2.y = m[1][2];
            v2.z = m[2][2];
            if (v0.x == 0.0f && v0.y == 0.0f && v0.z == 0.0f) {
                v0.x = 1.0f;
            }
#line 547 "D:/Bio4/Prog/emrock.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 549 "D:/Bio4/Prog/emrock.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 551 "D:/Bio4/Prog/emrock.cpp"
            VECNormalize(&v2, &v2);
            m[0][0] = v0.x;
            m[1][0] = v0.y;
            m[2][0] = v0.z;
            m[0][1] = v1.x;
            m[1][1] = v1.y;
            m[2][1] = v1.z;
            m[0][2] = v2.x;
            m[1][2] = v2.y;
            m[2][2] = v2.z;
        }
        PSMTXCopy(m, em->mat);
    }
    if (em->pMotion) {
        em->motFlags2 |= 0x40000000;
        MotionMove(em, 0);
    } else {
        em->partsMatCalc();
    }
    em->partsWorldCalc();
}

void emRock_R1_Fall(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    Vec d;
    Vec nrm;
    f32 len;
    f32 ang;

    switch (em->xFE) {
    case 0:
        w->timer2 = 60;
        em->xFE++;
    case 1:
        if (w->timer2) {
            w->timer2--;
            break;
        }
        em->pos.x = em->mat[0][3];
        em->pos.y = em->mat[1][3];
        em->pos.z = em->mat[2][3];
        Matrix2AxisAngle(em->mat, &em->rot);
        em->xFC = 1;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
        EstSet(0, -1, &em->pos, 0, 1, 8, 0, 0, 0, 0);
        break;
    }
    w->spd.y -= w->grav;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &em->oldPos, &em->pos, w->radius, 0x2001, 0);
    if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
        em->be_flag &= ~2;
        em->pos.x = em->mat[0][3];
        em->pos.y = em->mat[1][3];
        em->pos.z = em->mat[2][3];
        Matrix2AxisAngle(em->mat, &em->rot);
        em->xFC = 1;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
        EstSet(0, -1, &em->pos, 0, 1, 8, 0, 0, 0, 0);
        return;
    }
    if (w->pAtk) {
        emRockAtkCk(em, w->pAtk, 0, w->radius);
    }
    {
        Mtx m;
        Vec up;
        Vec axis;

        PSVECSubtract(&em->pos, &em->oldPos, &d);
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
        len = SQRTF((em->pos.x - em->oldPos.x) * (em->pos.x - em->oldPos.x) +
                    (em->pos.y - em->oldPos.y) * (em->pos.y - em->oldPos.y) +
                    (em->pos.z - em->oldPos.z) * (em->pos.z - em->oldPos.z));
        if (len > 500.0f) {
            len = 500.0f;
        }
        ang = len * 0.002f * 0.62831855f;
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        axis.x = 0.0f;
        axis.y = 0.0f;
        axis.z = 1.0f;
        PSMTXMultVecSR(m, &axis, &axis);
        if (axis.x == 0.0f) {
            axis.y = 0.0f;
        }
#line 654 "D:/Bio4/Prog/emrock.cpp"
        VECNormalize(&axis, &axis);
        len = acosf(PSVECDotProduct(&up, &axis));
        if (len > 0.01f && len < 3.1315927f) {
            PSVECCrossProduct(&up, &axis, &up);
            PSMTXRotAxisRad(m, &up, ang);
            PSMTXConcat(m, em->mat, em->mat);
        }
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    emRockAtkScrCk(em);
}

void emRock_R1_Throw(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    Vec d;
    Vec nrm;
    f32 len;
    f32 ang;

    switch (em->xFE) {
    case 0:
        w->timer = 0;
        w->timer2 = 60;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->timer = w->alwaysWait;
            if (w->seAlways[0] != 0xFF && w->seAlways[1] != 0xFF) {
                w->sndId = SndCall(w->seAlways[0], w->seAlways[1], &em->pos, w->seAlways[2], 0, em);
            }
        }
        if (w->timer2) {
            w->timer2--;
            break;
        }
        em->pos.x = em->mat[0][3];
        em->pos.y = em->mat[1][3];
        em->pos.z = em->mat[2][3];
        Matrix2AxisAngle(em->mat, &em->rot);
        em->xFC = 1;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
        EstSet(0, -1, &em->pos, 0, 1, 8, 0, 0, 0, 0);
        break;
    }
    w->spd.y -= w->grav;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &em->oldPos, &em->pos, w->radius, 0x2001, 0);
    if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
        f32 spd;

        spd = RootSumSquare3(&w->spd);
        C_VECReflect(&w->spd, &nrm, &d);
        PSVECScale(&d, &w->spd, spd * 0.99f);
        if (nrm.y < 0.5f) {
            em->be_flag &= ~2;
            em->pos.x = em->mat[0][3];
            em->pos.y = em->mat[1][3];
            em->pos.z = em->mat[2][3];
            Matrix2AxisAngle(em->mat, &em->rot);
            em->xFC = 1;
            em->xFD = 1;
            em->xFE = 0;
            em->xFF = 0;
            EstSet(0, -1, &em->pos, 0, 1, 8, 0, 0, 0, 0);
            return;
        }
        if (w->spd.y > 50.0f) {
            if (w->seFall[0] != 0xFF && w->seFall[1] != 0xFF) {
                SndCall(w->seFall[0], w->seFall[1], &em->pos, w->seFall[2], 0, em);
            }
            if (w->effFall[0] != 0xFF && w->effFall[1] != 0xFF) {
                Vec fp;

                fp = em->pos;
                fp.y = EatMgr.getFloor(&fp, 600.0f, 100000.0f, 0, 0);
                EstSet(0, -1, &fp, 0, w->effFall[0], w->effFall[1], 0, 0, 0, 0);
            }
            QuakeExec(0, 0, 5, 22.0f, 2);
        }
    }
    if (w->pAtk) {
        emRockAtkCk(em, w->pAtk, 1, w->radius);
    }
    {
        Mtx m;
        Vec up;
        Vec axis;

        PSVECSubtract(&em->pos, &em->oldPos, &d);
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
        len = SQRTF((em->pos.x - em->oldPos.x) * (em->pos.x - em->oldPos.x) +
                    (em->pos.y - em->oldPos.y) * (em->pos.y - em->oldPos.y) +
                    (em->pos.z - em->oldPos.z) * (em->pos.z - em->oldPos.z));
        if (len > 500.0f) {
            len = 500.0f;
        }
        ang = len * 0.002f * 0.62831855f;
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        axis.x = 0.0f;
        axis.y = 0.0f;
        axis.z = 1.0f;
        PSMTXMultVecSR(m, &axis, &axis);
        if (axis.x == 0.0f) {
            axis.y = 0.0f;
        }
#line 800 "D:/Bio4/Prog/emrock.cpp"
        VECNormalize(&axis, &axis);
        len = acosf(PSVECDotProduct(&up, &axis));
        if (len > 0.01f && len < 3.1315927f) {
            PSVECCrossProduct(&up, &axis, &up);
            PSMTXRotAxisRad(m, &up, ang);
            PSMTXConcat(m, em->mat, em->mat);
        }
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    emRockAtkScrCk(em);
}

void emRock_R1_Throw2(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    cAtariInfo* at;
    Vec d;
    Vec nrm;
    f32 len;
    f32 ang;

    switch (em->xFE) {
    case 0:
        w->timer = 0;
        w->timer2 = 180;
        at = &em->atari;
        at->flags &= ~0x300;
        SndCall(6, 0x49, &em->pos, 0, 0, em);
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->timer = w->alwaysWait;
            if (w->seAlways[0] != 0xFF && w->seAlways[1] != 0xFF) {
                w->sndId = SndCall(w->seAlways[0], w->seAlways[1], &em->pos, w->seAlways[2], 0, em);
            }
        }
        if (w->timer2) {
            w->timer2--;
            break;
        }
        em->pos.x = em->mat[0][3];
        em->pos.y = em->mat[1][3];
        em->pos.z = em->mat[2][3];
        Matrix2AxisAngle(em->mat, &em->rot);
        em->xFC = 1;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
        break;
    }
    w->spd.y -= w->grav;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &em->oldPos, &em->pos, w->radius, 0x2001, 0);
    if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
        Vec p;

        RootSumSquare3(&w->spd);
        C_VECReflect(&w->spd, &nrm, &d);
        if ((s16) pG->pl_life > 500) {
            w->pAtk->x0A |= 4;
        } else {
            w->pAtk->x0A &= ~4;
        }
        p = em->pos;
        p.y += 1000.0f;
        PlWepHitCheck2(0, &p, &p, 0x12, 3, 5000.0f);
        EffectEspgenDelete(0, w->espKind, em);
        if (nrm.y > 0.7f) {
            EstSet(0, -1, &em->pos, 0, 1, 1, 0, 0, 0, 0);
        } else {
            EstSet(0, -1, &em->pos, 0, 1, 2, 0, 0, 0, 0);
        }
        SndCall(6, 0x4A, &em->pos, 0, 0, em);
        em->be_flag &= ~2;
        em->xFC = 1;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
        return;
    }
    {
        Mtx m;
        Vec up;
        Vec axis;

        PSVECSubtract(&em->pos, &em->oldPos, &d);
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
        len = SQRTF((em->pos.x - em->oldPos.x) * (em->pos.x - em->oldPos.x) +
                    (em->pos.y - em->oldPos.y) * (em->pos.y - em->oldPos.y) +
                    (em->pos.z - em->oldPos.z) * (em->pos.z - em->oldPos.z));
        if (len > 500.0f) {
            len = 500.0f;
        }
        ang = len * 0.002f * 0.62831855f;
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        axis.x = 0.0f;
        axis.y = 0.0f;
        axis.z = 1.0f;
        PSMTXMultVecSR(m, &axis, &axis);
        if (axis.x == 0.0f) {
            axis.y = 0.0f;
        }
#line 936 "D:/Bio4/Prog/emrock.cpp"
        VECNormalize(&axis, &axis);
        len = acosf(PSVECDotProduct(&up, &axis));
        if (len > 0.01f && len < 3.1315927f) {
            PSVECCrossProduct(&up, &axis, &up);
            PSMTXRotAxisRad(m, &up, ang);
            PSMTXConcat(m, em->mat, em->mat);
        }
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    emRockAtkScrCk(em);
}

void emRock_R1_Roll(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    Vec d;
    f32 len;
    f32 ang;

    switch (em->xFE) {
    case 0:
        KeyStop(0xEFCF0000ULL);
        if (emRockSetRollRoute(em) == 0) {
            em->pos.x = em->mat[0][3];
            em->pos.y = em->mat[1][3];
            em->pos.z = em->mat[2][3];
            Matrix2AxisAngle(em->mat, &em->rot);
            em->xFC = 1;
            em->xFD = 0;
            em->xFE = 0;
            em->xFF = 0;
            return;
        }
        switch (pG->room_no) {
        case 4:
            SndStrReq(1, 0x3E, 0x80000003, 0, 0, 0.0f);
            break;
        case 6:
            SndStrReq(1, 0x3C, 0x80000003, 0, 0, 0.0f);
            break;
        case 0xA:
            SndStrReq(1, 0x3D, 0x80000003, 0, 0, 0.0f);
            break;
        }
        emRockPushCk(em, 0);
        em->atari.flags &= ~0x200;
        PLS->rot.y = em->rot.y;
        SetPlDamage((int) em, (void (*)(cPlayer*)) plemRockEscape);
        w->timer3 = 75;
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        if ((pG->room_id32 & 0xFFFF0000) == 0x01040000) {
            w->xAD = 1;
            w->rollWait = 0;
        } else {
            w->xAD = 0;
            w->rollWait = 25;
        }
        em->xFE++;
    case 1:
        if (w->timer3) {
            w->timer3--;
            if (w->timer3 == 0) {
                w->sndId2 = SndCall(6, 5, &em->pos, 0, 0, em);
            }
            return;
        }
        if (emRockSetRollSpd(em)) {
            SndStop(w->sndId2, 0);
            SndCall(6, 6, &em->pos, 0, 0, em);
            EstSet(0, -1, &em->pos, 0, 1, 0x1F, 0, 0, 0, 0);
            em->be_flag &= ~2;
            em->xFC = 1;
            em->xFD = 1;
            em->xFE = 0;
            em->xFF = 0;
            return;
        }
    default:
        w->spd.y -= 10.0f;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        if (w->rollWait) {
            w->rollWait--;
        } else {
            f32 floor;

            floor = EatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) + w->radius;
            if (em->pos.y < floor) {
                em->pos.y = floor;
                w->spd.y *= -0.5f;
                if (w->spd.y > 50.0f) {
                    Vec fp;

                    fp = em->pos;
                    fp.y -= w->radius;
                    EstSet(0, -1, &fp, 0, 0xC8, 0, 0, 0, 0, 0);
                    SndCall(6, 7, &em->pos, 0, 0, em);
                    if (w->xAD == 0) {
                        w->xAD = 1;
                        w->spd.x = 0.0f;
                        w->spd.z = 0.0f;
                    }
                }
            }
        }
        emRockRollHitCk(em);
        emRockRunDownCk(em);
        {
            Mtx m;
            Vec up;
            Vec axis;

            PSVECSubtract(&em->pos, &em->oldPos, &d);
            PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
            len = SQRTF((em->pos.x - em->oldPos.x) * (em->pos.x - em->oldPos.x) +
                        (em->pos.y - em->oldPos.y) * (em->pos.y - em->oldPos.y) +
                        (em->pos.z - em->oldPos.z) * (em->pos.z - em->oldPos.z));
            if (len > 500.0f) {
                len = 500.0f;
            }
            ang = len * 0.002f * 0.31415927f;
            up.x = 0.0f;
            up.y = 1.0f;
            up.z = 0.0f;
            axis.x = 0.0f;
            axis.y = 0.0f;
            axis.z = 1.0f;
            PSMTXMultVecSR(m, &axis, &axis);
            if (axis.x == 0.0f) {
                axis.y = 0.0f;
            }
#line 1093 "D:/Bio4/Prog/emrock.cpp"
            VECNormalize(&axis, &axis);
            len = acosf(PSVECDotProduct(&up, &axis));
            if (len > 0.01f && len < 3.1315927f) {
                PSVECCrossProduct(&up, &axis, &up);
                PSMTXRotAxisRad(m, &up, ang);
                PSMTXConcat(m, em->mat, em->mat);
            }
        }
        TransMatrix(em->mat, &em->pos);
        em->partsWorldCalc();
        emRockAtkScrCk(em);
        break;
    }
}

void emRock_R1_Drop(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);

    switch (em->xFE) {
    case 0:
        em->atari.flags &= ~0x200;
        MotionSetCore(em, &em->pMotion, w->mot0, 0, 0, 1, 0);
        em->xFE++;
    case 1:
        MotionMove(em, 0);
        if (!(em->flags_3C8 & 1)) {
            break;
        }
        em->xFE++;
    case 2:
        MotionSetCore(em, &em->pMotion, w->mot1, 0, 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 1, 4, 0, w->espKind, (u32) em, 0);
        SndCall(6, 8, &em->pos, 0, 0, em);
        w->timer = 37;
        em->xFE++;
    case 3:
        if (w->timer) {
            w->timer--;
            emRockDropHitCk(em);
            emRockDropHitCkSub(em);
            if (emRockDropHitCkEm2b(em)) {
                em->be_flag &= ~2;
                em->atari.flags &= ~0x200;
                em->hp = 0;
                em->xFC = 1;
                em->xFD = 1;
                em->xFE = 0;
                em->xFF = 0;
                EffectEspgenDelete(0, w->espKind, em);
                EstSet(0, -1, &em->getPartsPtr(0)->worldPos, 0, 1, 1, 0, 0, 0, 0);
                SndCall(6, 7, &em->pos, 0, 0, em);
                break;
            }
            if (w->timer == 0) {
                SndCall(6, 7, &em->pos, 0, 0, em);
                em->atari.flags |= 0x200;
            }
        }
        if (MotionMove(em, 0)) {
            em->xFE++;
        }
        break;
    case 4:
        break;
    }
    em->partsWorldCalc();
}

void emRock_R1_Drop2(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);

    switch (em->xFE) {
    case 0:
        em->xFE++;
        em->atari.flags &= ~0x200;
    case 1:
        MotionSetCore(em, &em->pMotion, w->mot1, 0, 0, 1, 0);
        MotionMove(em, 0);
        if (!(em->flags_3C8 & 1)) {
            break;
        }
        if ((s16) pG->pl_life <= 0) {
            break;
        }
        KeyStop(0xEFCF0000ULL);
        em->xFE++;
    case 2:
        w->timer = 25;
        emRockPushCk(em, 50);
        FSet(pPL->rot.y, -0.1f);
        FSet(pPL->pos.x, -4924.0f);
        FSet(pPL->pos.y, -11950.0f);
        FSet(pPL->pos.z, -14770.0f);
        pPL->setPos(&pPL->pos);
        pPL->st.x325 = 2;
        SetPlDamage((int) em, (void (*)(cPlayer*)) plemDropFind);
        em->xFE++;
    case 3:
        MotionSetCore(em, &em->pMotion, w->mot1, 0, 0, 1, 0);
        MotionMove(em, 0);
        if (w->timer) {
            w->timer--;
            break;
        }
        em->xFE++;
        break;
    case 4:
        MotionSetCore(em, &em->pMotion, w->mot1, 0, 0, 1, 0);
        EstSet(0, -1, 0, 0, 1, 5, 0, 0, 0, 0);
        SndCall(6, 4, &em->pos, 0, 0, em);
        w->timer = 31;
        w->rnd = Rnd() & 1;
        w->xAE = 0;
        em->xFE++;
    case 5:
        if (MotionMove(em, 0)) {
            SndCall(6, 5, &em->pos, 0, 0, em);
            em->be_flag &= ~2;
            em->xFC = 1;
            em->xFD = 1;
            em->xFE = 0;
            em->xFF = 0;
            break;
        }
        if (w->timer) {
            w->timer--;
            if (w->timer == 0) {
                if (w->xAE != 0) {
                    break;
                }
                if ((s16) pG->pl_life > 0) {
                    w->xAE = 1;
                    SetPlDamage((int) em, (void (*)(cPlayer*)) plemDropDie);
                    pPL->xFF = 1;
                    break;
                }
            }
            if (w->xAE == 0) {
                switch (w->rnd) {
                case 0:
                default:
                    ActBtn.set(0x25, 5, (int) plemDropEscAction, (int) em, 0x42, 3, 0, 0);
                    break;
                case 1:
                    ActBtn.set(0x25, 5, (int) plemDropEscAction, (int) em, 0x42, 4, 0, 0);
                    break;
                }
            }
        }
        break;
    }
    em->partsWorldCalc();
}

void plemDropEscAction(cEmRock* em)
{
    EMROCK_WK(em)->xAE = 1;
    pPL->st.x325 = 2;
    SetPlDamage((int) em, (void (*)(cPlayer*)) plemDropEscape);
}

// Player damage routine of the drop: notices the rock, then the escape / death routine takes over.
void plemDropFind(cPlayer* pl)
{
    EmRockWork* w = EMROCK_WK(PL_ROCK(pl));

    pl->x378 = PL_ROCK(pl)->x378;
    pl->st.x325 = 2;
    switch (pl->xFE) {
    case 0:
        pl->x3E0 = 25;
        pl->xFE++;
    case 1:
        if (pl->x3E0) {
            pl->x3E0--;
            MotionSetCore(pl, &pl->pMotion, w->mot5, 0, 3, 1, 0);
            emRockPushCamMove(PL_ROCK(pl));
        } else {
            emRockDropCamMove(PL_ROCK(pl));
        }
        if (MotionMove(pl, 0)) {
            pG->flags_170 &= ~0x80000000;
            EndPlDamage();
        }
        break;
    }
    pl->x378 = pl->x37C;
}

// Player damage routine: the player dives out of the way of the dropping rock.
void plemDropEscape(cPlayer* pl)
{
    EmRockWork* w = EMROCK_WK(PL_ROCK(pl));

    pl->x378 = PL_ROCK(pl)->x378;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, &pl->pMotion, w->mot4, 0, 3, 1, 0);
        EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, 0);
        SndCall(1, 0x43, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        SndCall(1, 0x44, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        pPL->st.x325 = 0x1E;
        pl->xFE++;
    case 1:
        if (pl->frame > 20.7f && pl->frame < 21.3f) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if (pl->frame > 33.7f && pl->frame < 34.3f) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMove(pl, 0)) {
            pG->flags_170 &= ~0x80000000;
            EndPlDamage();
        }
        break;
    }
    pl->x378 = pl->x37C;
}

// The rolling rock runs the player over: 1 when it hit him this frame.
int emRockRollHitCk(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    int dead;

    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    dead = 1;
    if (!(pPL->flags_324 & 0xFFFF0000)) {
        dead = 0;
    }
    if (dead) {
        return 0;
    }
    if ((em->pos.x - pPL->pos.x) * (em->pos.x - pPL->pos.x) + (em->pos.z - pPL->pos.z) * (em->pos.z - pPL->pos.z) >
        w->radius * w->radius) {
        return 0;
    }
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 0xB, 1);
    QuakeExec(0, 0, 5, 22.0f, 2);
    pG->pl_life = 0;
    PlSetDamage(8, 0, 0);
    return 1;
}

void cEmRock::setParent(cEm* parent, int partsNo_, int flag)
{
    EmRockWork* w = EMROCK_WK(this);

    w->partsNo = partsNo_;
    w->pParent = parent;
    if (flag) {
        w->flags |= 1;
    } else {
        w->flags &= ~1;
    }
    xFC = 1;
    xFD = 2;
    xFE = 0;
    xFF = 0;
    parent->atari.flags &= ~0x200;
}

// Drops the rock off its parent: it falls straight down (emRock_R1_Fall) with `atk` as its attack.
void cEmRock::setFall(EmAtkInfo* atk)
{
    EmRockWork* w = EMROCK_WK(this);
    Mtx m;

    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &rot);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    oldPos = pos;
    if (w->pParent) {
        w->x30 = (u32) w->pParent;
    }
    w->pParent = 0;
    hp = 1;
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emRockAtk;
        emRockAtk.range = w->radius;
    }
    xFC = 1;
    xFD = 3;
    xFE = 0;
    xFF = 0;
}

// Throws the rock with speed `spd` (a random forward throw in the parent's frame when NULL).
void cEmRock::setThrow(Vec* spd, EmAtkInfo* atk)
{
    EmRockWork* w = EMROCK_WK(this);
    Vec v;
    Mtx m;

    if (spd) {
        v = *spd;
    } else {
        v.x = fRand1_1() * 10.0f + 20.0f;
        v.y = fRand1_1() * 10.0f + 75.0f;
        v.z = fRand1_1() * 10.0f + 350.0f;
        if (w->pParent) {
            PSMTXMultVecSR(w->pParent->mat, &v, &v);
        } else {
            PSMTXMultVecSR(mat, &v, &v);
        }
    }
    w->spd.x = v.x;
    w->spd.y = v.y;
    w->spd.z = v.z;
    rot.x = 0.0f;
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &rot);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    oldPos = pos;
    if (w->pParent) {
        w->x30 = (u32) w->pParent;
    }
    w->pParent = 0;
    hp = 1;
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emRockAtk;
        emRockAtk.range = w->radius;
    }
    xFC = 1;
    xFD = 4;
    xFE = 0;
    xFF = 0;
}

// setThrow variant that breaks on the first scenario hit (emRock_R1_Throw2).
void cEmRock::setThrow2(Vec* spd, EmAtkInfo* atk)
{
    EmRockWork* w = EMROCK_WK(this);
    Vec v;
    Mtx m;

    if (spd) {
        v = *spd;
    } else {
        v.x = fRand1_1() * 10.0f + 20.0f;
        v.y = fRand1_1() * 10.0f + 75.0f;
        v.z = fRand1_1() * 10.0f + 350.0f;
        if (w->pParent) {
            PSMTXMultVecSR(w->pParent->mat, &v, &v);
        } else {
            PSMTXMultVecSR(mat, &v, &v);
        }
    }
    w->spd.x = v.x;
    w->spd.y = v.y;
    w->spd.z = v.z;
    rot.x = 0.0f;
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &rot);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    oldPos = pos;
    if (w->pParent) {
        w->x30 = (u32) w->pParent;
    }
    w->pParent = 0;
    hp = 1;
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emRockAtk;
        emRockAtk.range = w->radius;
    }
    xFC = 1;
    xFD = 5;
    xFE = 0;
    xFF = 0;
}

// Dead-stripped in the original (STRIP_UNUSED): only its constant pool (one 0.0f) survives between
// setThrow2's pool and setYarareCube's.
static void emRockSpdClear(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);

    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
}

void cEmRock::setSeFall(u8 blk, u8 no, u8 vol)
{
    EmRockWork* w = EMROCK_WK(this);

    w->seFall[0] = blk;
    w->seFall[1] = no;
    w->seFall[2] = vol;
    w->seFall[3] = 0;
}

void cEmRock::setEffFall(u8 id, u8 type)
{
    EmRockWork* w = EMROCK_WK(this);

    w->effFall[0] = id;
    w->effFall[1] = type;
}

void cEmRock::setEffAlways(int id, int type)
{
    EstSet((int) this, -1, 0, 0, id, type, 0, EMROCK_WK(this)->espKind, (u32) this, 0);
}

void cEmRock::setYarareCube(Vec* size, f32 x, f32 y, f32 z)
{
    if (size) {
        YarareInitCube(this, size->x, size->y, size->z, x, y, z, 0, 1);
    } else {
        YarareInitCube(this, 0.0f, -400.0f, 0.0f, x, y, z, 0, 1);
    }
    hp = 1;
}

void cEmRock::setTransMode(int on)
{
    EmRockWork* w = EMROCK_WK(this);

    if (on) {
        w->flags &= ~2;
    } else {
        w->flags |= 2;
    }
}

// Scenario event triggers (EMI type 3) the flying rock passes over: sets the pG->flags_174 event
// bits selected by the entry's sub type (room 119 fires a second bit while the trigger is fresh).
void emRockAtkScrCk(cEmRock* em)
{
    int i;

    if (pG->pRoomEmi == 0) {
        return;
    }
    for (i = 0; i < *(int*) pG->pRoomEmi; i++) {
        u32 o = i * 0x40 + 8;
        EmiEntry* e = (EmiEntry*) ((u8*) pG->pRoomEmi + o);

        if (((u8*) pG->pRoomEmi)[o] != 3) {
            continue;
        }
        if (e->state == 3) {
            continue;
        }
        if ((e->pos.x - em->pos.x) * (e->pos.x - em->pos.x) + (e->pos.z - em->pos.z) * (e->pos.z - em->pos.z) >
            9000000.0f) {
            continue;
        }
        if ((pG->room_id32 & 0xFFFF0000) == 0x01190000) {
            if (e->state == 0) {
                switch (e->sub) {
                case 0:
                    BitOn(pG->flags_174, 0x80000000);
                    BitOn(pG->flags_174, 0x10000000);
                    break;
                case 1:
                    BitOn(pG->flags_174, 0x40000000);
                    BitOn(pG->flags_174, 0x08000000);
                    break;
                case 2:
                    BitOn(pG->flags_174, 0x20000000);
                    BitOn(pG->flags_174, 0x04000000);
                    break;
                }
            } else {
                switch (e->sub) {
                case 0:
                    pG->flags_174 |= 0x80000000;
                    break;
                case 1:
                    pG->flags_174 |= 0x40000000;
                    break;
                case 2:
                    pG->flags_174 |= 0x20000000;
                    break;
                }
            }
        }
        e->state = 3;
    }
}

// First EMI route point (type 6): 1 when found (routeIdx / pRoute set).
int emRockSetRollRoute(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    u8* emi;
    int i;
    int idx;

    emi = (u8*) pG->pRoomEmi;
    if (emi == 0) {
        return 0;
    }
    idx = -1;
    for (i = 0; i < *(int*) pG->pRoomEmi; i++) {
        u32 o = i * 0x40 + 8;

        if (((u8*) pG->pRoomEmi)[o] == 6) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        return 0;
    }
    w->routeIdx = idx;
    {
        u32 o = idx * 0x40 + 8;

        w->pRoute = (EmiEntry*) ((u8*) pGS->pRoomEmi + o);
    }
    return 1;
}

// Steers the rolling speed towards the current route point, advancing to the next one within
// 500 units; 1 when the route ends (the rock stops).
int emRockSetRollSpd(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    u8* emi;
    EmiEntry* e;
    int idx;
    int i;
    f32 spd;
    f32 add;
    Vec dir;

    emi = (u8*) pG->pRoomEmi;
    if (emi == 0) {
        return 1;
    }
    e = w->pRoute;
    spd = (e->pos.x - em->pos.x) * (e->pos.x - em->pos.x) + (e->pos.z - em->pos.z) * (e->pos.z - em->pos.z);
    if (spd < 250000.0f) {
        idx = -1;
        for (i = w->routeIdx + 1; i < *(int*) pG->pRoomEmi; i++) {
            u32 o = i * 0x40 + 8;

            if (((u8*) pG->pRoomEmi)[o] == 6) {
                idx = i;
                break;
            }
        }
        if (idx == -1) {
            return 1;
        }
        w->routeIdx = idx;
        {
            u32 o = idx * 0x40 + 8;

            e = (EmiEntry*) ((u8*) pGS->pRoomEmi + o);
        }
        w->pRoute = e;
    }
    PSVECSubtract(&e->pos, &em->pos, &dir);
    dir.y = 0.0f;
#line 2170 "D:/Bio4/Prog/emrock.cpp"
    VECNormalize(&dir, &dir);
    spd = SQRTF(w->spd.x * w->spd.x + w->spd.z * w->spd.z);
    if (w->xAD) {
        add = 1.3f;
        if (pG->x4F88 <= 2) {
            add = 1.27f;
        }
    } else {
        add = 3.0f;
    }
    spd += add;
    if (spd < 50.0f) {
        spd = 50.0f;
    }
    if (spd > 500.0f) {
        spd = 500.0f;
    }
    PSVECScale(&dir, &dir, spd);
    w->spd.x = dir.x;
    w->spd.z = dir.z;
    return 0;
}

// The player stepped into a roll start trigger (EMI type 8, 3000 units).
int emRockRollStartCk(cEmRock* em)
{
    EmiData* emi;
    int dead;
    int i;
    GlobalWork* g;

    emi = (EmiData*) pG->pRoomEmi;
    g = pGS;  // the struct-view read is a second pG pseudo (`mr r11,r9`) that the pl_life test reads
    if (emi == 0) {
        return 0;
    }
    if ((s16) g->pl_life <= 0) {
        return 0;
    }
    dead = 1;
    if (!(pPL->flags_324 & 0xFFFF0000)) {
        dead = 0;
    }
    if (dead) {
        return 0;
    }
    for (i = 0; i < emi->n; i++) {
        EmiEntry* e = &emi->entry[i];

        if (e->type == 8) {
            if ((e->pos.x - pPL->pos.x) * (e->pos.x - pPL->pos.x) + (e->pos.y - pPL->pos.y) * (e->pos.y - pPL->pos.y) +
                    (e->pos.z - pPL->pos.z) * (e->pos.z - pPL->pos.z) >
                9000000.0f) {
                return 0;
            }
            return 1;
        }
    }
    return 0;
}

// Player damage routine of the rolling rock: the player turns, runs along the EMI route with the
// button-mash speed motions, and jumps to the side (or gets caught) at the goal.
void plemRockEscape(cPlayer* pl)
{
    EmRockWork* w = EMROCK_WK(PL_ROCK(pl));
    void* mot;
    void* mot2;
    Vec v;
    int lim;
    int n;
    int flag;

    pl->x378 = PL_ROCK(pl)->x378;
    mot2 = w->plMot[3];
    mot = w->plMot[2];
    switch (pl->xFE) {
    case 0:
        pl->x3E0 = 85;
        Cckpt.lifeMeterDisp(0);
        pl->pWep->setTrans(0, 0);
        switch (pG->room_no) {
        case 4:
        default:
            FSet(pPL->pos.x, 57947.0f);
            FSet(pPL->pos.y, 3273.0f);
            FSet(pPL->pos.z, -27900.0f);
            pl->rot.y = 1.67f;
            break;
        case 6:
            FSet(pPL->pos.x, 28428.0f);
            FSet(pPL->pos.y, -5465.0f);
            FSet(pPL->pos.z, 2765.0f);
            pl->rot.y = -1.99f;
            break;
        case 0xA:
            FSet(pPL->pos.x, -38340.0f);
            FSet(pPL->pos.y, 5111.0f);
            FSet(pPL->pos.z, 68313.0f);
            pl->rot.y = -1.86f;
            break;
        }
        pl->xFE++;
    case 1:
        pl->st.x325 = 0x1E;
        if (pl->x3E0) {
            pl->x3E0--;
            emRockPushCamMove(PL_ROCK(pl));
            MotionSetCore(pl, &pl->pMotion, w->plMot[0], (int) w->plMot[1], 0, 1, 0);
            pl->rot.y += Muku(&pl->pos, &PL_ROCK(pl)->pos, pl->rot.y, 3.1415927f);
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
            MotionMove(pl, 0);
        } else {
            emRockPushCamMove2(PL_ROCK(pl));
            ActBtn.set(0x18, 5, 0, 0, 2, 2, 0, 0);
            if (MotionMove(pl, 0)) {
                pl->xFE++;
            }
        }
        break;
    case 2:
        MotionSetCore(pl, &pl->pMotion, mot, (int) mot2, 10, 5, 0);
        pl->x3E0 = 0;
        pl->x3E4 = 0;
        pl->x3E8 = 0;
        pl->x3EC = plemRockSetEscapeRoute();
        pl->x3F0 = 0;
        pl->x3F4 = 0;
        pl->x3F8 = 0;
        pl->x3FC = Rnd() & 1;
        pl->x400 = 0.1f;
        w->xAE = 0;
        pl->xFE++;
    case 3:
        plemRockEscapeCamMove(pl, pl->x400);
        pl->x400 += 0.05f;
        if (pl->x400 > 1.0f) {
            pl->x400 = 1.0f;
        }
        lim = 8;
        if (pG->x4F88 <= 2) {
            lim = 12;
        }
        if (pG->x4F88 > 7) {
            lim = 5;
        }
        pl->x3F0++;
        if (pl->x3F0 > lim) {
            pl->x3F0 = lim;
            pl->x3E0 -= 5;
            if ((int) pl->x3E0 < 0) {
                pl->x3E0 = 0;
            }
        }
        n = (int) pl->x3E0 / 20;
        if (n > 7) {
            n = 7;
        }
        if (n != pl->x3E4) {
            f32 ratio;
            f32 f;
            u32 cnt;
            u32 fr;

            pl->x3E4 = n;
            switch (n) {
            case 0:
            default:
                mot2 = w->plMot[3];
                break;
            case 1:
                mot2 = w->plMot[4];
                break;
            case 2:
                mot2 = w->plMot[5];
                break;
            case 3:
                mot2 = w->plMot[6];
                break;
            case 4:
                mot2 = w->plMot[7];
                break;
            case 5:
                mot2 = w->plMot[8];
                break;
            case 6:
                mot2 = w->plMot[9];
                break;
            case 7:
                mot2 = w->plMot[10];
                break;
            }
            ratio = pl->frame / (f32) pl->frameMax;
            cnt = ((RockMotData*) mot2)->maxFrame;
            f = (f32) cnt * ratio;
            fr = (u32) f + 1;
            if (fr >= cnt) {
                fr = 0;
            }
            MotionSetCore(pl, &pl->pMotion, mot, (int) mot2, pl->x29D, 5, (u16) fr);
        }
        if (Key.trg & 0x80000) {
            pl->x3E0 += pl->x3F0;
            pl->x3F0 = 0;
            if ((int) pl->x3E0 > 0x9F) {
                pl->x3E0 = 0x9F;
            }
        }
        if (pl->x3EC != -1) {
            u32 o = pl->x3EC * 0x40 + 8;
            EmiEntry* e = (EmiEntry*) ((u8*) pG->pRoomEmi + o);

            RouteCkToPos(pl, &e->pos, &v, 0, 0);
            pl->rot.y += Muku(&pl->pos, &v, pl->rot.y, 0.024543693f);
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        }
        MotionMove(pl, 0);
        if (pl->x3F8 == 0) {
            if (plemRockEscapeCk(pl)) {
                pl->x3F8 = 1;
            }
        }
        if (pl->x3F8 && w->xAE == 0) {
            if (pl->x3FC) {
                ActBtn.set(0x25, 5, (int) plemRockEscAction, (int) pl, 0x42, 3, 0, 0);
            } else {
                ActBtn.set(0x25, 5, (int) plemRockEscAction, (int) pl, 0x42, 4, 0, 0);
            }
        } else {
            ActBtn.set(0x18, 5, 0, 0, 2, 2, 0, 0);
        }
        break;
    case 4:
        mot = w->plMot[11];
        flag = 1;
        if (pl->x3F4) {
            flag = 0x41;
        }
        MotionSetCore(pl, &pl->pMotion, mot, 0, 3, flag, 0);
        pl->x3E0 = 20;
        SndCall(1, 0x48, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        SndCall(1, 0x11, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        pl->xFE++;
    case 5:
        if (pl->x3E0) {
            pl->x3E0--;
            plemRockEscapeCamMove(pl, 1.0f);
        } else {
            plemRockEscapeCamMove2(pl, pl->x3F4);
        }
        pl->st.x325 = 0x78;
        if (pl->frame > 11.7f && pl->frame < 12.3f) {
            EstSet(0, -1, &pl->pos, 0, 3, 0x13, 0, 0, 0, 0);
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if (MotionMove(pl, 0)) {
            pG->flags_170 &= ~0x80000000;
            Cckpt.lifeMeterDisp(1);
            pl->pWep->setTrans(1, 0);
            GameSaveSave(&GameSave, pSaveData, -1);
            EndPlDamage();
        }
        break;
    }
    pl->x378 = pl->x37C;
}

void cEmRock::setPlMotion(void** mot)
{
    EmRockWork* w = EMROCK_WK(this);

    w->plMot[0] = *mot++;
    w->plMot[1] = *mot++;
    w->plMot[2] = *mot++;
    w->plMot[3] = *mot++;
    w->plMot[4] = *mot++;
    w->plMot[5] = *mot++;
    w->plMot[6] = *mot++;
    w->plMot[7] = *mot++;
    w->plMot[8] = *mot++;
    w->plMot[9] = *mot++;
    w->plMot[10] = *mot++;
    w->plMot[11] = *mot++;
    w->plMot[12] = *mot++;
    w->plMot[13] = *mot++;
    w->plMot[14] = *mot++;
    w->plMot[15] = *mot++;
}

void cEmRock::setScale(f32 s)
{
    scale.z = s;
    scale.y = s;
    scale.x = s;
    EMROCK_WK(this)->radius = s * 600.0f;
}

// Last EMI route point (type 6): the player runs towards it. -1 when there is none.
int plemRockSetEscapeRoute()
{
    EmiData* emi;
    int i;

    emi = (EmiData*) pG->pRoomEmi;
    if (emi == 0) {
        return -1;
    }
    for (i = emi->n - 1; i >= 0; i--) {
        if (emi->entry[i].type == 6) {
            return i;
        }
    }
    return -1;
}

// The player reached the escape goal (EMI type 7, 3000 units): its sub type goes to x3F4.
int plemRockEscapeCk(cPlayer* pl)
{
    u8* emi;
    EmiEntry* e;
    int i;
    int idx;

    emi = (u8*) pG->pRoomEmi;
    if (emi == 0) {
        return 0;
    }
    idx = -1;
    for (i = 0; i < *(int*) pG->pRoomEmi; i++) {
        u32 o = i * 0x40 + 8;

        if (((u8*) pG->pRoomEmi)[o] == 7) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        return 0;
    }
    {
        u32 o = idx * 0x40 + 8;

        e = (EmiEntry*) ((u8*) pG->pRoomEmi + o);
    }
    if ((pl->pos.x - e->pos.x) * (pl->pos.x - e->pos.x) + (pl->pos.z - e->pos.z) * (pl->pos.z - e->pos.z) > 9000000.0f) {
        return 0;
    }
    pl->x3F4 = e->sub;
    return 1;
}

void plemRockEscAction(cEmRock* em)
{
    EMROCK_WK(em)->xAE = 1;
    em->xFE++;
}

// Camera behind the running player, blended from the current camera by `rate`, shaken a little
// and pulled in front of the scenery.
void plemRockEscapeCamMove(cPlayer* pl, f32 rate)
{
    static Vec emRock_campos = { 500.0f, 200.0f, 3000.0f };
    static Vec emRock_target = { 250.0f, 1500.0f, 0.0f };
    Vec p0;
    Vec p1;
    Vec r;
    Vec hit;
    Vec d;
    f32 len;
    GlobalWork* g = pG;
    Camera* cam = &emRockCam;

    FSet(cam->param.fovy, 27.0f);
    PSMTXMultVec(pl->mat, &emRock_campos, &p0);
    PSMTXMultVec(pl->mat, &emRock_target, &p1);
    PosToPos(&g->Cam.param.at, &p1, &emRockCam.param.at, rate);
    PosToPos(&g->Cam.param.pos, &p0, &emRockCam.param.pos, rate);
    r.x = fRand1_1() * 10.0f;
    r.y = fRand1_1() * 10.0f;
    r.z = fRand1_1() * 10.0f;
    PSVECAdd(&emRockCam.param.pos, &r, &emRockCam.param.pos);
    PSVECAdd(&emRockCam.param.at, &r, &emRockCam.param.at);
    if (EatMgr.hitCheck(&emRockCam.param.at, &emRockCam.param.pos, &hit, 0, 0x8000, 0)) {
        PSVECSubtract(&hit, &emRockCam.param.at, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 2738 "D:/Bio4/Prog/emrock.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&emRockCam.param.at, &d, &emRockCam.param.pos);
    }
    {
        Camera* cam = &emRockCam;
        Vec* cp = &cam->param.pos;
        Vec* ca = &cam->param.at;

        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        cam->up.x = 0.0f;
        cam->up.y = 1.0f;
        cam->up.z = 0.0f;
        cam->dist = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.x250 = (s32) cam;
    }
}

// Camera of the side jump at the goal (`side` = the goal's sub type).
void plemRockEscapeCamMove2(cPlayer* pl, int side)
{
    static Vec emRock_campos = { -30.0f, 490.0f, -1424.0f };
    static Vec emRock_target = { 397.0f, 1301.0f, 1408.0f };
    Vec p0;
    Vec p1;
    Vec r;
    f32 len;
    Camera* gcam = &pG->Cam;

    emRockCam.param.fovy = 50.0f;
    if (side) {
        emRock_campos.x = 30.0f;
        emRock_target.x = -397.0f;
    } else {
        emRock_campos.x = -30.0f;
        emRock_target.x = 397.0f;
    }
    if ((pG->room_id32 & 0xFFFF0000) == 0x01060000) {
        emRock_campos.y = 690.0f;
    } else {
        emRock_campos.y = 490.0f;
    }
    PSMTXMultVec(pl->mat, &emRock_campos, &p0);
    PSMTXMultVec(pl->mat, &emRock_target, &p1);
    PosToPos(&gcam->param.at, &p1, &emRockCam.param.at, 1.0f);
    PosToPos(&gcam->param.pos, &p0, &emRockCam.param.pos, 1.0f);
    r.x = fRand1_1() * 10.0f;
    r.y = fRand1_1() * 10.0f;
    r.z = fRand1_1() * 10.0f;
    PSVECAdd(&emRockCam.param.pos, &r, &emRockCam.param.pos);
    PSVECAdd(&emRockCam.param.at, &r, &emRockCam.param.at);
    {
        Camera* cam = &emRockCam;
        Vec* cp = &emRockCam.param.pos;
        Vec* ca = &emRockCam.param.at;

        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        cam->up.x = 0.0f;
        cam->up.y = 1.0f;
        cam->up.z = 0.0f;
        cam->dist = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.x250 = (s32) cam;
    }
}

// Camera of the player crushed by the dropping rock: looks at him from the rock's side.
void plemRockDropDieCamMove(cEmRock* em)
{
    Vec p;
    f32 len;
    cModel* parts;
    Camera* gcam = &pG->Cam;

    emRockCam.param.fovy = 50.0f;
    if (Muku(&em->pos, &pPL->pos, em->rot.y, 3.1415927f) < 0.0f) {
        p.x = -10000.0f;
        p.y = 5000.0f;
        p.z = 0.0f;
    } else {
        p.x = 10000.0f;
        p.y = 5000.0f;
        p.z = 0.0f;
    }
    PSMTXMultVec(em->mat, &p, &p);
    parts = pPL->getPartsPtr(0);
    PosToPos(&gcam->param.at, &parts->worldPos, &emRockCam.param.at, 0.1f);
    PosToPos(&gcam->param.pos, &p, &emRockCam.param.pos, 0.1f);
    {
        Camera* cam = &emRockCam;
        Vec* cp = &emRockCam.param.pos;
        Vec* ca = &emRockCam.param.at;

        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        cam->up.x = 0.0f;
        cam->up.y = 1.0f;
        cam->up.z = 0.0f;
        cam->dist = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.x250 = (s32) cam;
    }
}

// Camera of the rock being pushed loose (per room), looking at the rock.
void emRockPushCamMove(cEmRock* em)
{
    Vec p;
    f32 len;
    cModel* parts;
    Camera* gcam = &pG->Cam;

    emRockCam.param.fovy = 50.0f;
    switch (pG->room_no) {
    case 4:
    default:
        p.x = 83070.0f;
        p.y = 11064.0f;
        p.z = -29906.0f;
        break;
    case 6:
        p.x = 15434.0f;
        p.y = 4482.0f;
        p.z = -7368.0f;
        break;
    case 0xA:
        p.x = -51287.0f;
        p.y = 15308.0f;
        p.z = 65768.0f;
        break;
    case 0:
        p.x = -6801.0f;
        p.y = -1833.0f;
        p.z = -9193.0f;
        break;
    }
    parts = em->getPartsPtr(0);
    PosToPos(&gcam->param.at, &parts->worldPos, &emRockCam.param.at, 1.0f);
    emRockCam.param.pos = p;
    {
        Camera* cam = &emRockCam;
        Vec* cp = &emRockCam.param.pos;
        Vec* ca = &emRockCam.param.at;

        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        cam->up.x = 0.0f;
        cam->up.y = 1.0f;
        cam->up.z = 0.0f;
        cam->dist = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.x250 = (s32) cam;
    }
}

// Fixed camera of the rock starting to roll (per room).
void emRockPushCamMove2(cEmRock* em)
{
    Vec p0;
    Vec p1;
    f32 len;

    emRockCam.param.fovy = 27.0f;
    switch (pG->room_no) {
    case 4:
    default:
        p0.x = 53244.0f;
        p0.y = 3116.0f;
        p0.z = -28110.0f;
        p1.x = 58819.0f;
        p1.y = 4385.0f;
        p1.z = -28363.0f;
        break;
    case 6:
        p0.x = 32002.0f;
        p0.y = -6025.0f;
        p0.z = 4672.0f;
        p1.x = 27490.0f;
        p1.y = -3769.0f;
        p1.z = 2875.0f;
        break;
    case 0xA:
        p0.x = -34645.0f;
        p0.y = 4086.0f;
        p0.z = 67755.0f;
        p1.x = -39140.0f;
        p1.y = 6867.0f;
        p1.z = 69202.0f;
        break;
    }
    emRockCam.param.pos = p0;
    emRockCam.param.at = p1;
    {
        Camera* cam = &emRockCam;
        Vec* cp = &cam->param.pos;
        Vec* ca = &cam->param.at;

        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        cam->up.x = 0.0f;
        cam->up.y = 1.0f;
        cam->up.z = 0.0f;
        cam->dist = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.x250 = (s32) cam;
    }
}

// Fixed camera of the drop scene.
void emRockDropCamMove(cEmRock* em)
{
    Vec p0;
    Vec p1;
    f32 len;
    Camera* cam = &emRockCam;
    Vec* cp = &cam->param.pos;
    Vec* ca = &cam->param.at;

    cam->param.fovy = 50.0f;
    p0.x = -5217.81f;
    p0.y = -12316.48f;
    p0.z = -16037.2f;
    p1.x = -5256.36f;
    p1.y = -10495.61f;
    p1.z = -15051.18f;
    cam->param.pos = p0;
    cam->param.at = p1;
    // `up` is set BEFORE `len`: with the up stores after the six len loads, the up.x store is the
    // 34th memory insn of the block and sched1 flushes its pending lists there (haifa's 32-entry
    // limit), which pins the 1.0/0.0 stores behind it and swaps the three pool highs (r27..r29).
    cam->up.x = 0.0f;
    cam->up.y = 1.0f;
    cam->up.z = 0.0f;
    len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
    cam->dist = SQRTF(len);
    CameraSetOrientationUp(cam);
    CamCtrl.x250 = (s32) cam;
}

// Enemies (ids 0x10..0x20) within 1.5 radii of the rock are knocked down (routine 3/4).
void emRockRunDownCk(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    cModel* p = em->getPartsPtr(0);
    cEm* e;
    Vec v;
    f32 len;
    f32 r;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        v = e->pos;
        v.y += 1000.0f;
        r = w->radius * 1.5f;
        len = (p->worldPos.x - v.x) * (p->worldPos.x - v.x) + (p->worldPos.y - v.y) * (p->worldPos.y - v.y) +
              (p->worldPos.z - v.z) * (p->worldPos.z - v.z);
        if (len < r * r) {
            e->hp = 0;
            e->xFC = 3;
            e->xFD = 4;
            e->xFE = 0;
            e->xFF = 0;
        }
    }
}

// Flying rock against the player (`atk` with the rock's radius as range): 1 on a hit.
int emRockAtkCk(cEmRock* em, EmAtkInfo* atk, int type, f32 r)
{
    EmRockWork* w = EMROCK_WK(em);
    EmAtkInfo a;

    if (atk) {
        a = *atk;
        a.range = w->radius;
        if (EmAtkHitCk(&a, &em->pos, &em->oldPos, 1)) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            if (w->se8D[0] != 0xFF && w->se8D[1] != 0xFF) {
                SndCall(w->se8D[0], w->se8D[1], &em->pos, w->se8D[2], 0, em);
            }
            SndStop(w->sndId, 0);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (type) {
                PlSetDamage(8, 0, 0);
            }
            if (w->eff9C[0] != 0xFF && w->eff9C[1] != 0xFF) {
                EmPlBloodSet2(em, &em->pos, 1, w->eff9C[0], w->eff9C[1]);
            } else {
                EmPlBloodSet2(em, &em->pos, 1, 0xFF, 0xFF);
            }
            return 1;
        }
    }
    return 0;
}

// Starts the push motions (plMot[13..15], round robin) on the enemies pushing the rock.
void emRockPushCk(cEmRock* em, int frame)
{
    EmRockWork* w = EMROCK_WK(em);
    cEm* e;
    u32 n;
    u32 i;

    n = 0;
    for (i = 0; i < EmMgr.nArray; i++) {
        e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x20) {
            continue;
        }
        if (e->x38D != 0x1C) {
            continue;
        }
        switch (n) {
        case 0:
        default:
            MotionSetCore(e, &e->pMotion, w->plMot[13], 0, 0, 1, (u16) frame);
            break;
        case 1:
            MotionSetCore(e, &e->pMotion, w->plMot[14], 0, 0, 1, (u16) frame);
            break;
        case 2:
            MotionSetCore(e, &e->pMotion, w->plMot[15], 0, 0, 1, (u16) frame);
            break;
        }
        n++;
        if (n > 2) {
            n = 0;
        }
        e->flags_3C8 |= 1;
    }
}

void cEmRock::setDropMot(void* a, void* b, void* c, void* d)
{
    EmRockWork* w = EMROCK_WK(this);

    w->mot0 = a;
    w->mot1 = b;
    w->mot2 = c;
    w->mot3 = d;
    xFC = 1;
    xFD = 7;
    xFE = 0;
    xFF = 0;
}

void cEmRock::setDropMot2(void* a, void* b, void* c, void* d, void* e, void* f, void* g)
{
    EmRockWork* w = EMROCK_WK(this);

    w->mot1 = a;
    w->mot2 = b;
    w->mot4 = c;
    w->mot5 = d;
    w->plMot[13] = e;
    w->plMot[14] = f;
    w->plMot[15] = g;
    xFC = 1;
    xFD = 8;
    xFE = 0;
    xFF = 0;
}

// The dropping rock reached the player (radius + 1000): starts the death routine. 1 on a hit.
int emRockDropHitCk(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    cModel* p;
    int dead;
    f32 len;
    f32 r;

    if ((s16) pG->pl_life <= 0) {
        return 0;
    }
    dead = 1;
    if (!(pPL->flags_324 & 0xFFFF0000)) {
        dead = 0;
    }
    if (dead) {
        return 0;
    }
    if (w->mot2 == 0) {
        return 0;
    }
    p = em->getPartsPtr(0);
    len = (p->worldPos.x - pPL->pos.x) * (p->worldPos.x - pPL->pos.x) + (p->worldPos.y - pPL->pos.y) * (p->worldPos.y - pPL->pos.y) +
          (p->worldPos.z - pPL->pos.z) * (p->worldPos.z - pPL->pos.z);
    r = w->radius + 1000.0f;
    if (len > r * r) {
        return 0;
    }
    SetPlDamage((int) em, plemDropDie);
    return 1;
}

// Same for the sub character.
int emRockDropHitCkSub(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    cModel* p;
    int dead;
    f32 len;
    f32 r;

    if (pSUB == 0) {
        return 0;
    }
    if ((s16) pG->sub_life <= 0) {
        return 0;
    }
    dead = 1;
    if (!(pSUB->flags_324 & 0xFFFF0000)) {
        dead = 0;
    }
    if (dead) {
        return 0;
    }
    if (w->mot3 == 0) {
        return 0;
    }
    p = em->getPartsPtr(0);
    len = (p->worldPos.x - pSUB->pos.x) * (p->worldPos.x - pSUB->pos.x) + (p->worldPos.y - pSUB->pos.y) * (p->worldPos.y - pSUB->pos.y) +
          (p->worldPos.z - pSUB->pos.z) * (p->worldPos.z - pSUB->pos.z);
    r = w->radius + 1000.0f;
    if (len > r * r) {
        return 0;
    }
    SetSubDamage((int) em, (void*) subemDropDie);
    return 1;
}

// The dropping rock hit an em2b (parts 2 within radius + 2000): knocks it down unless flagged. 1 on a hit.
int emRockDropHitCkEm2b(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    cModel* p = em->getPartsPtr(0);
    cEm* e;
    cModel* q;
    f32 len;
    f32 r;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->id != 0x2B) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e == em) {
            continue;
        }
        q = e->getPartsPtr(2);
        len = (q->worldPos.x - p->worldPos.x) * (q->worldPos.x - p->worldPos.x) +
              (q->worldPos.y - p->worldPos.y) * (q->worldPos.y - p->worldPos.y) +
              (q->worldPos.z - p->worldPos.z) * (q->worldPos.z - p->worldPos.z);
        r = w->radius + 2000.0f;
        if (len < r * r) {
            if (!(e->flags_3C8 & 8)) {
                e->xFC = 2;
                e->xFD = 4;
                e->xFE = 0;
                e->xFF = 0;
            }
            return 1;
        }
    }
    return 0;
}

// Player damage routine: crushed by the dropping rock.
void plemDropDie(cPlayer* pl)
{
    EmRockWork* w = EMROCK_WK(PL_ROCK(pl));

    pl->x378 = PL_ROCK(pl)->x378;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, &pl->pMotion, w->mot2, 0, 3, 1, 0);
        pG->pl_life = 0;
        PlSetDamageSe(0xD);
        pl->xFE++;
    case 1:
        if (pl->xFF == 0) {
            plemRockDropDieCamMove(PL_ROCK(pl));
        } else {
            emRockDropCamMove(PL_ROCK(pl));
        }
        MotionMove(pl, 0);
        break;
    }
    pl->x378 = pl->x37C;
}

// Sub character damage routine: crushed by the dropping rock.
void subemDropDie()
{
    cEm* sub = pSUB;
    EmRockWork* w = EMROCK_WK(PL_ROCK(sub));

    sub->x378 = PL_ROCK(sub)->x378;
    switch (sub->xFE) {
    case 0:
        MotionSetCore(sub, &sub->pMotion, w->mot3, 0, 3, 1, 0);
        pG->sub_life = 0;
        sub->xFE++;
    case 1:
        MotionMove(sub, 0);
        break;
    }
    sub->x378 = sub->x37C;
}

// Room 11E: the rock breaks (effect, sound) and stops.
void cEmRock::setBreakR11E()
{
    EmRockWork* w = EMROCK_WK(this);

    EstSet(0, -1, &getPartsPtr(0)->worldPos, 0, 1, 2, 0, 0, 0, 0);
    SndCall(6, 7, &pos, 0, 0, this);
    hp = 0;
    be_flag &= ~2;
    atari.flags &= ~0x200;
    xFC = 1;
    xFD = 1;
    xFE = 0;
    xFF = 0;
    EffectEspgenDelete(0, w->espKind, this);
}

void emRockSatClear(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);

    if (pG->room_id != 0x11E) {
        return;
    }
    if (em->type != 3) {
        return;
    }
    if (w->pSat == 0) {
        return;
    }
    w->pSat->flags &= ~4;
}

void emRockSatSet(cEmRock* em)
{
    EmRockWork* w = EMROCK_WK(em);
    Vec pos;
    Vec rot;

    if (pG->room_id != 0x11E) {
        return;
    }
    if (em->type != 3) {
        return;
    }
    emRockSatClear(em);
    if (!(em->be_flag & 2)) {
        return;
    }
    pos = em->getPartsPtr(0)->worldPos;
    pos.y -= 2800.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    if (w->pSat) {
        w->pSat->flags |= 4;
        w->pSat->setCoord(&pos, &rot);
    } else {
        w->pSat = EatMgr.create((void*) (((u32*) pG->pRoomArc)[5] + (u32) pG->pRoomArc), 0, &pos, &rot, 1);
    }
}
