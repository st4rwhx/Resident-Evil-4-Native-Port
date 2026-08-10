// game/emBar.cpp: wooden bar enemy (cEmBar): boards the player breaks by shooting or climbs
// through with the action button.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "emBar.h"
#include "emhit.h"
#include "etc_model.h"
#include "esp.h"
#include "act_btn.h"
#include "snd.h"
#include "pl_wep.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

class cPlayer;

extern cEm* pPL;   // game/em.cpp

extern "C" {
int MotionMove(cModel* m, int a);
void EtcSetAddAmb(cModel* m, int a);                 // EtcModel.cpp
int PlBombHitCk(Vec* pos, f32 r);                    // em_sub.cpp
void SetPlDamage(int type, void (*func)(cPlayer*));  // pl_sub.cpp
void EndPlDamage();
void plemEscape(cPlayer* pl);
}
void MotionSetCore(cModel* m, void* mot, void* data, int a, int b, int c, int d);

typedef void (*EmBarFunc)(cEmBar*);

EmBarFunc EmBar_R0_move_tbl[4] = {
    emBar_R0_Init,
    emBar_R0_Move,
    0,
    0,
};

static EmBarFunc EmBar_R1_move_tbl[2] = {
    emBar_R1_Set,
    emBar_R1_Break,
};

cEmBar* SetBar(void* bin, void* tpl, Vec* pos, Vec* rot, int flagNo)
{
    cEmBar* em;
    EmBarWork* w;
    u16* flg;

    em = (cEmBar*) EmMgr.create(0x51);
    if (em == 0) {
        return 0;
    }
    w = EMBAR_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->rot = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetBar() failed.");
        EmMgr.destroy(em);
        return 0;
    }
    EtcSetAddAmb(em, 0xD);
    u32 zero = 0;
    w->size.x = 3500.0f;
    w->size.y = 400.0f;
    w->size.z = 10.0f;
    w->eff = 0xFF;
    em->atari.init(0, 2, 0, 0.0f, 0.0f, 0.0f, 700.0f, 400.0f, 500.0f, 500.0f);
    em->atari.throughOn();
    emBarYarareInit(em);
    em->hpMax = em->hp;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 0x10);
    }
    em->lockParts = 0;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(1);
    em->setStatus(0xB);
    em->hp = 1000;
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    w->flags = zero;
    w->flagNo = flagNo;
    flg = GetEtcFlgPtr(flagNo, pG->room_id);
    if (flg && (*flg & 1)) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        em->xFC = 1;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
    } else {
        em->xFC = 1;
        em->xFD = 0;
        em->xFE = 0;
        em->xFF = 0;
    }
    return em;
}

void emBarDmCk(cEmBar* em)
{
    u8 wep;

    if (em->dmHit == 0) {
        return;
    }
    wep = em->dmWep;
    em->dmHit = 0;
    if (wep == 0x14) {
        return;
    }
    if (wep == 0x16) {
        return;
    }
    if (wep == 0x17) {
        return;
    }
    if (wep == 0x2A) {
        return;
    }
    if (wep == 0xE) {
        return;
    }
    switch (wep) {
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        em->dmType = 0;
        break;
    }
    switch (em->dmWep) {
    case 7:
    case 8:
    case 0x12:
    case 0x13:
    case 0x1B:
    case 0x26:
    case 0x27:
    case 0x28:
        wep = 0;
        break;
    case 0x29:   // default-target node: makes the right sub-list 3 nodes so the 0x26-0x28 range is its root
        break;
    }
    em->hp = 0;
    // arm order matters for jump2's cross-jump: the SetBreak(0) arm must come first so the 7/8/0x21
    // then-block stays in place (`ble` to the else block) and the case bodies jump into it; the
    // explicit default-target cases (5, 6, 0xD..0xF, 0x12, 0x13, 0x29, 0x2A, 0x2C) shape the tree
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
        emBarSetBreak(em, 0);
        break;
    case 7:
    case 8:
    case 0x21:
        if (em->dmRad > 36000000.0f) {
            emBarSetBreak(em, 0);
        } else {
            emBarSetBreak(em, 1);
        }
        break;
    case 5:
    case 6:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2A:
    case 0x2C:
    default:
        emBarSetBreak(em, 1);
        break;
    }
}

void emBarSetBreak(cEmBar* em, u32 type)
{
    EmBarWork* w = EMBAR_WK(em);
    u8 eff = w->eff;

    em->hp = 0;
    if (eff != 0xFF) {
        switch (type) {
        case 0:
        default:
            EstSet(0, -1, &em->pos, &em->rot, eff, 0, 0, 0, 0, 0);
            SndCall(6, 8, &em->pos, 0, 0, em);
            break;
        case 1:
            EstSet(0, -1, &em->pos, &em->rot, eff, 1, 0, 0, 0, 0);
            SndCall(6, 8, &em->pos, 0, 0, em);
            break;
        case 2:
            EstSet(0, -1, &em->pos, &em->rot, eff, 2, 0, 0, 0, 0);
            SndCall(6, 7, &em->pos, 0, 0, em);
            break;
        }
    }
    em->be_flag &= ~2;
    em->xFC = 1;
    em->xFD = 1;
    em->xFE = 0;
    em->xFF = 0;
}

void cEmBar::move()
{
    emBarDmCk(this);
    be_flag &= ~0x4000;
    EmBar_R0_move_tbl[xFC](this);
}

void emBar_R0_Init(cEmBar* em)
{
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
}

void emBar_R0_Move(cEmBar* em)
{
    EmBar_R1_move_tbl[em->xFD](em);
}

void emBar_R1_Set(cEmBar* em)
{
    EmBarWork* w = EMBAR_WK(em);
    u8 step = em->xFE;

    if (step == 0) {
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        em->partsMatCalc();
        em->partsWorldCalc();
        w->escaping = step;
        w->timer = 30;
        em->xFE++;
    }
    em->be_flag |= 0x4000;
    if (em->plDist2 < 25000000.0f) {
        u8 esc = w->escaping;

        if (esc == 0) {
            if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, 3.1415927f)) < 1.5707964f) {
                ActBtn.set(0x25, 5, (int) emBarActEscape, (int) em, 1, 3, 0, esc);
            }
        }
    }
    emBarHitCk(em);
}

void emBarActEscape(cEmBar* em)
{
    EMBAR_WK(em)->escaping = 1;
    SetPlDamage((int) em, plemEscape);
}

void plemEscape(cPlayer* pl)
{
    cEm* em = (cEm*) pl;
    cEmBar* bar = (cEmBar*) em->dmgType;
    EmBarWork* w = EMBAR_WK(bar);

    em->x378 = ((cEm*) pPL->dmgType)->x378;
    em->dmg.set(0, 0xF);
    switch (em->xFE) {
    case 0:
        MotionSetCore(em, &em->pMotion, w->motion, 0, 5, 1, 0);
        em->xFE++;
    case 1:
        if (MotionMove(em, 0)) {
            EndPlDamage();
        }
        break;
    }
    em->x378 = em->x37C;
}

void emBar_R1_Break(cEmBar* em)
{
    EmBarWork* w = EMBAR_WK(em);
    u8 step = em->xFE;

    if (step == 0) {
        u16* flg = GetEtcFlgPtr(w->flagNo, pG->room_id);

        if (flg) {
            *flg |= 1;
        }
        em->hp = step;
        em->be_flag &= ~2;
        em->xFE++;
    }
    em->be_flag |= 0x4000;
}

void emBarYarareInit(cEmBar* em)
{
    EmBarWork* w = EMBAR_WK(em);

    YarareInitCube((cEmHit*) em, 0.0f, -w->size.y * 0.5f, 0.0f, w->size.x * 0.5f + 50.0f, w->size.y, w->size.z * 0.5f + 50.0f, 0, 1);
}

void cEmBar::setEff(u8 no)
{
    EMBAR_WK(this)->eff = no;
}

void cEmBar::setMotion(void* mot)
{
    EMBAR_WK(this)->motion = mot;
}

// em / p (= &parts->mat): global.c priority is floor_log2(refs)*refs/live_length, ours em 6 refs /
// 83 insns (1445) vs p 4 / 53 (1509) would give p r31. The `do {} while (0)` around emBarSetBreak
// doubles that em ref's weight (7 refs -> 1686) and em takes r31 like the original; no code changes.
int emBarHitCk(cEmBar* em)
{
    Vec v;
    cModel* p;

    if (em->hp <= 0) {
        return 0;
    }
    p = em->getPartsPtr(0);
    em->dmType = 1;
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(p->mat, &v, &v);
    if (PlBombHitCk(&v, 500.0f) == 0 && PlWepHitCheck2(0, &v, &v, 0x12, 3, 500.0f) == 0) {
        v.x = -400.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(p->mat, &v, &v);
        if (PlBombHitCk(&v, 500.0f) == 0 && PlWepHitCheck2(0, &v, &v, 0x12, 3, 500.0f) == 0) {
            v.x = 400.0f;
            v.y = 0.0f;
            v.z = 0.0f;
            PSMTXMultVec(p->mat, &v, &v);
            if (PlBombHitCk(&v, 500.0f) == 0 && PlWepHitCheck2(0, &v, &v, 0x12, 3, 500.0f) == 0) {
                em->dmType = 0;
                return 0;
            }
        }
    }
    do { emBarSetBreak(em, 2); } while (0);  // COMPILER-DIFF: tie (see above)
    return 1;
}
