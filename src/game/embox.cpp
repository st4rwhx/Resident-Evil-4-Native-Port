// game/embox.cpp: box enemy (cEmBox): breakable boxes, barrels, vases and cabinets that drop an
// item when shot, kicked (action button) or caught in a damage volume.

#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "embox.h"
#include "emhit.h"
#include "etc_model.h"
#include "esp.h"
#include "snd.h"
#include "mes.h"
#include "act_btn.h"
#include "joy.h"
#include "eprintf.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern cEm* pPL;   // game/em.cpp
// Struct-member view of pPL (the pGS trick): the load stays below the preceding stack stores.
struct EmPtr {
    cEm* p;
};

extern "C" {
void EtcSetAddAmb(cModel* m, int a);                                                         // EtcModel.cpp
void LifeDownSet(cEm* em, int dmg, int a);                                                  // em_sub.cpp
void EmAtCheck(cEm* em);                                                                     // at_mod.cpp
void SetEffModel(void* bin, void* tpl, Vec* pos, Vec* rot);                                  // esp_efm.cpp
int getRoomEtc(int no, int kind, cEm** out, int a);                                          // EtcModel.cpp
void SceAtCreateItemAt(Vec* pos, u16 no, int num, int a, int b, int c, int d);              // sce_at.cpp
extern u32 tubo_amb;                                                                         // esp.cpp
}

typedef void (*EmBoxFunc)(cEmBox*);

static EmBoxFunc EmBox_R0_move_tbl[4] = {
    emBox_R0_Init,
    emBox_R0_Move,
    0,
    0,
};

EmBoxFunc EmBox_R1_move_tbl[2] = {
    emBox_R1_Set,
    emBox_R1_Break,
};

cEmBox* SetBox(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type, int etcNo)
{
    cEmBox* em;
    EmBoxWork* w;
    u16* flg;
    int zero;

    em = (cEmBox*) EmMgr.create(0x43);
    if (em == 0) {
        return 0;
    }
    w = EMBOX_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->rot = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetBox() failed.");
        EmMgr.destroy(em);
        return 0;
    }
    em->type = type;
    switch (em->type) {
    default:
        EtcSetAddAmb(em, 3);
        break;
    case 4:
        EtcSetAddAmb(em, 9);
        break;
    case 6:
    case 7:
        EtcSetAddAmb(em, 11);
        break;
    case 3:
    case 5:
        EtcSetAddAmb(em, 1);
        break;
    }
    w->eff = 0xFF;
    switch (em->type) {
    case 0:
    default:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 150.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 200.0f;
        w->size.y = 350.0f;
        w->size.z = 550.0f;
        break;
    case 1:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 300.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 450.0f;
        w->size.y = 600.0f;
        w->size.z = 1000.0f;
        break;
    case 2:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 300.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 200.0f;
        w->size.y = 350.0f;
        w->size.z = 550.0f;
        break;
    case 3:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 800.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 660.0f;
        w->size.y = 1250.0f;
        w->size.z = 660.0f;
        break;
    case 4:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 0.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 200.0f;
        w->size.y = 350.0f;
        w->size.z = 550.0f;
        break;
    case 5:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 300.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 630.0f;
        w->size.y = 1250.0f;
        w->size.z = 630.0f;
        break;
    case 6:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 250.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 250.0f;
        w->size.y = 500.0f;
        w->size.z = 250.0f;
        break;
    case 7:
        em->lockParts = 0;
        em->lockOfs.x = 0.0f;
        em->lockOfs.y = 1000.0f;
        em->lockOfs.z = 0.0f;
        w->size.x = 550.0f;
        w->size.y = 1450.0f;
        w->size.z = 550.0f;
        break;
    }
    w->sat0 = 0;
    w->sat1 = 0;
    em->type = type;
    switch (em->type) {
    default: {
        cAtariInfo* at = &em->atari;

        atariInitF(at, 0.0f, 0.0f, 0.0f, 700.0f, 400.0f, 500.0f, 500.0f, 0, 2, 0);
        at->setPriority(3);
        at->flags &= ~0x300;
        break;
    }
    case 3: {
        cAtariInfo* at = &em->atari;

        atariInitF(at, 0.0f, 750.0f, 0.0f, 350.0f, 350.0f, 350.0f, 750.0f, 1, 0x2000, 10);
        at->setPriority(3);
        at->flags &= ~0x100;
        break;
    }
    case 5:
    case 7: {
        cAtariInfo* at = &em->atari;

        atariInitF(at, 0.0f, 750.0f, 0.0f, 300.0f, 300.0f, 300.0f, 750.0f, 1, 0x2000, 10);
        at->setPriority(3);
        at->flags &= ~0x100;
        break;
    }
    }
    emBoxYarareInit(em);
    em->hpMax = em->hp = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 1000.0f, 1000.0f, 0.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 0x10);
    }
    em->setStatus(1);
    em->setStatus(0xB);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    w->etcNo = etcNo;
    w->flags = 0;
    w->breakBin = 0;
    w->breakTpl = 0;
    w->itemNum = 0;
    w->itemNo = -1;
    flg = GetEtcFlgPtr(etcNo, pGS->room_id);
    if (flg && (*flg & 1)) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        em->xFC = 1;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
        em->clearStatus(5);
    } else {
        em->xFC = 1;
        em->xFD = 0;
        em->xFE = 0;
        em->xFF = 0;
        em->setStatus(5);
    }
    return em;
}

void emBoxDmCk(cEmBox* em)
{
    EmBoxWork* w = EMBOX_WK(em);
    u8 wep;
    int dmg;
    Vec hit;

    if (em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, &hit)) {
        case 1:
        case 4:
        case 5:
        case 7:
            emBoxSetBreak(em, 0);
            return;
        }
    }
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
    case 0x14:
    case 0x15:
    case 0x2A:
    case 0x2E:
    default:
        dmg = 1000;
        break;
    case 7:
    case 8:
    case 0x21:
        if (em->plDist2 > 36000000.0f) {
            dmg = 1000;
        } else {
            dmg = 9999;
        }
        break;
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
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x2B:
        dmg = 1000;
        break;
    case 5:
    case 6:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2C:
    case 0x2D:
        dmg = 9999;
        break;
    }
    LifeDownSet(em, dmg, 0);
    if (em->hp <= 0) {
        switch (em->dmWep) {
        case 0x14:
        case 0x15:
        case 0x2A:
        case 0x2E:
        default:
            emBoxSetBreak(em, 0);
            break;
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
        case 0x1B:
        case 0x1D:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x2B:
            emBoxSetBreak(em, 0);
            break;
        case 7:
        case 8:
        case 0x21:
            if (em->dmRad > 36000000.0f) {
                emBoxSetBreak(em, 0);
            } else {
                emBoxSetBreak(em, 1);
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
        case 0x2C:
        case 0x2D:
            emBoxSetBreak(em, 2);
            break;
        }
    }
    if (w->eff != 0xFF && em->type != 4) {
        EmDmBloodSet2(em, w->eff, 3, 0, 0, 0);
    }
}

// Each break kind carries its own EstSet + fallback pair (a macro in the original: the arms are
// full copies whose tails the compiler cross-jumps).
#define EMBOX_BREAK_EFF(no, fallback)                                                       \
    EstSet(0, -1, &em->pos, &em->rot, w->eff, no, 1, 0, (u32) em, 0);                     \
    if (w->breakBin == 0 && w->breakTpl == 0) {                                            \
        EstSet(0, -1, &em->pos, &em->rot, w->eff, fallback, 1, 0, (u32) em, 0);           \
    }

void emBoxSetBreak(cEmBox* em, u32 kind)
{
    EmBoxWork* w = EMBOX_WK(em);

    em->hp = 0;
    em->be_flag &= ~2;
    if (w->eff != 0xFF) {
        switch (em->type) {
        default:
            switch (kind) {
            case 0:
            default:
                EMBOX_BREAK_EFF(0, 4);
                break;
            case 1:
                EMBOX_BREAK_EFF(1, 4);
                break;
            case 2:
                EMBOX_BREAK_EFF(2, 4);
                break;
            }
            break;
        case 5:
            switch (kind) {
            case 0:
            default:
                EMBOX_BREAK_EFF(5, 8);
                break;
            case 1:
                EMBOX_BREAK_EFF(6, 8);
                break;
            case 2:
                EMBOX_BREAK_EFF(7, 8);
                break;
            }
            break;
        case 4:
            EstSet(0, -1, &em->pos, &em->rot, w->eff, 0, 1, 0, (u32) em, 0);
            break;
        }
    }
    if (w->breakBin && w->breakTpl) {
        SetEffModel(w->breakBin, w->breakTpl, &em->pos, &em->rot);
    }
    switch (em->type) {
    default:
        SndCall(1, 0x1A, &em->pos, 0, 0, em);
        break;
    case 3:
    case 5:
        SndCall(1, 0x3C, &em->pos, 0, 0, em);
        break;
    case 4:
        SndCall(6, 0x34, &em->pos, 0, 0, em);
        break;
    case 6:
        SndCall(6, 0x5A, &em->pos, 0, 0, em);
        break;
    case 7:
        SndCall(6, 0x57, &em->pos, 0, 0, em);
        break;
    }
    emBoxSatClear(em);
    if (w->itemNo != -1) {
        emBoxSetItem(em);
    }
    em->xFC = 1;
    em->xFD = 1;
    em->xFE = 0;
    em->xFF = 0;
}

void cEmBox::move()
{
    if (Joy[2].on & 0x10) {
        u8 amb = (u8) tubo_amb;

        if (amb == 0) {
            be_flag &= ~8;
        } else {
            be_flag |= 8;
        }
        x139 = amb;
        x13A = amb;
        x13B = amb;
        eprintf(80, 200, 0, 0, "amb = %d", tubo_amb);
    }
    emBoxDmCk(this);
    be_flag &= ~0x4000;
    EmBox_R0_move_tbl[xFC](this);
    EmAtCheck(this);
    atari.move();
    emBoxActEvtCk(this);
}

void emBox_R0_Init(cEmBox* em)
{
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
}

void emBox_R0_Move(cEmBox* em)
{
    EmBox_R1_move_tbl[em->xFD](em);
}

void emBox_R1_Set(cEmBox* em)
{
    if (em->xFE == 0) {
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        em->partsMatCalc();
        em->partsWorldCalc();
        em->xFE++;
    }
    em->be_flag |= 0x4000;
}

void emBox_R1_Break(cEmBox* em)
{
    EmBoxWork* w = EMBOX_WK(em);
    u16* flg;

    if (em->xFE == 0) {
        flg = GetEtcFlgPtr(w->etcNo, pG->room_id);
        if (flg) {
            *flg |= 1;
        }
        em->hp = 0;
        em->be_flag &= ~2;
        em->clearStatus(5);
        w->timer = 150;
        em->atari.throughOn();
        em->xFE++;
    }
    em->be_flag |= 0x4000;
}

void emBoxSatClear(cEmBox* em)
{
    EmBoxWork* w = EMBOX_WK(em);

    if (w->sat0) {
        w->sat0->flags &= ~4;
    }
    if (w->sat1) {
        w->sat1->flags &= ~4;
    }
}

// Unused in the shipped build (nothing calls it and the linker dropped it from .text; the unit
// is in STRIP_UNUSED), but its constants (0.5f, 0.0f, 225000000.0f) are still in the unit's
// constant pool between cEmBox::move's string and emBoxYarareInit's pool. The body only has
// to reproduce that pool.
static void emBoxSatSet(cEmBox* em)
{
    EmBoxWork* w = EMBOX_WK(em);
    f32 hx = w->size.x * 0.5f;

    if (hx == 0.0f) {
        return;
    }
    if (w->sat0 != 0 && em->plDist2 > 225000000.0f) {
        return;
    }
    emBoxSatClear(em);
}

void emBoxYarareInit(cEmBox* em)
{
    EmBoxWork* w = EMBOX_WK(em);

    YarareInitCube((cEmHit*) em, 0.0f, 0.0f, 0.0f, w->size.x * 0.5f + 50.0f, w->size.y, w->size.z * 0.5f + 50.0f, 0, 1);
    YarareAddCube((cEmHit*) em, &w->hit, 0.0f, 0.0f, 0.0f, w->size.x * 0.5f * 0.5f, w->size.y * 0.8f, w->size.z * 0.5f * 0.8f, 0, 1);
}

void cEmBox::setEff(u8 eff)
{
    EmBoxWork* w = EMBOX_WK(this);

    w->eff = eff;
    if (hp > 0) {
        return;
    }
    if (w->breakBin == 0 && w->breakTpl == 0 && w->eff != 0xFF && type != 4) {
        if (type != 5) {
            EstSet(0, -1, &pos, &rot, w->eff, 4, 1, 0, (u32) this, 0);
        } else {
            EstSet(0, -1, &pos, &rot, w->eff, 8, 1, 0, (u32) this, 0);
        }
    }
    if (w->breakBin && w->breakTpl) {
        SetEffModel(w->breakBin, w->breakTpl, &pos, &rot);
    }
}

void cEmBox::setItem(int no, int num, u16 c, u16 d)
{
    EmBoxWork* w = EMBOX_WK(this);

    w->itemNo = no;
    w->itemNum = num;
    w->item22C = c;
    w->item22E = d;
}

void emBoxActEvtCk(cEmBox* em)
{
    EmBoxWork* w = EMBOX_WK(em);
    Mtx inv;
    Vec lp;
    Vec a;
    Vec b;

    if (em->hp <= 0) {
        return;
    }
    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f)) > 0.7853982f) {
        return;
    }
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &lp);
    if (lp.x > w->size.x + 1000.0f) {
        return;
    }
    if (lp.x < -(w->size.x + 1000.0f)) {
        return;
    }
    if (lp.y > w->size.y + 0.0f) {
        return;
    }
    if (lp.y < -(w->size.y + 2000.0f)) {
        return;
    }
    if (lp.z > w->size.z + 1000.0f) {
        return;
    }
    if (lp.z < -(w->size.z + 1000.0f)) {
        return;
    }
    a = em->pos;
    b = (((EmPtr*) &pPL)->p)->pos;
    a.y += 200.0f;
    b.y = a.y;
    if (EatMgr.hitCheck(&a, &b, 0, 0, 0, 0)) {
        return;
    }
    if (pG->room_id == 0x100 || pG->room_id == 0x101 || pG->room_id == 0x103 || pG->room_id == 0x106) {
        ActBtn.set(1, 5, (int) emBoxAction, (int) em, 0, 1, 0, 0);
    }
}

int checkNearOtherBarrel(cEmBox* em)
{
    u32 i;
    cEm* other;

    for (i = 0; i <= 0x3F; i++) {
        if (getRoomEtc(i, 0x11, &other, 0) == 1 || getRoomEtc(i, 0x1E, &other, 0) == 1) {
            if (em != other && other->hp > 0 && PSVECSquareDistance(&em->pos, &other->pos) < 2250000.0f) {
                return 1;
            }
        }
    }
    return 0;
}

void emBoxAction(cEmBox* em)
{
    switch (em->type) {
    case 0:
    case 1:
    case 2:
    case 4:
    case 6:
    case 7:
    default:
        cMes.MesSet(3, 100, 336 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1, 1, 0, 0, 4);
        break;
    case 3:
    case 5:
        if (checkNearOtherBarrel(em) == 1) {
            cMes.MesSet(5, 100, 336 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1, 1, 0, 0, 4);
        } else {
            cMes.MesSet(4, 100, 336 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1, 1, 0, 0, 4);
        }
        break;
    }
}

void emBoxSetItem(cEmBox* em)
{
    EmBoxWork* w = EMBOX_WK(em);

    if (w->itemNo != -1) {
        SceAtCreateItemAt(&em->pos, w->itemNo, w->itemNum, -1, -1, 0, -1);
    }
}

void cEmBox::setBreakModel(void* bin, void* tpl)
{
    EmBoxWork* w = EMBOX_WK(this);

    w->breakBin = bin;
    w->breakTpl = tpl;
}
