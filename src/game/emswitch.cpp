// game/emswitch.cpp: lever switch enemy (cEmSwitch): opens / closes barred gates, toggled by the
// action button or by a hit.

#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "emswitch.h"
#include "emhit.h"
#include "etc_model.h"
#include "act_btn.h"
#include "snd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern cEm* pPL;   // game/em.cpp

extern "C" {
void EmAtCheck(cModel* m);                        // at_mod.cpp
cEm* SetR227Barrel(Vec* pos, Vec* rot);           // embarrel.cpp
}

typedef void (*EmSwitchFunc)(cEmSwitch*);

static EmSwitchFunc EmSwitch_R1_move_tbl[3] = {
    emSwitch_R1_Set,
    emSwitch_R1_Open,
    emSwitch_R1_Close,
};

static void emSwitchDmCk(cEmSwitch* em)
{
    EmSwitchWork* w = EMSWITCH_WK(em);
    int near;
    u8 wep;

    if (em->dmHit == 0) {
        return;
    }
    em->dmHit = 0;
    near = 0;
    if (em->dmPart->rad < 36000000.0f) {
        near = 1;
    }
    wep = em->dmWep;
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
    em->dmType = 1;
    if (wep == 0x10) {
        em->dmType = 0x11;
    }
    switch (em->dmWep) {
    case 7:
    case 8:
    case 0x21:
        if (near) {
            EmDmBloodSet2(em, 0x62, 1, 0, 0, 0);
        } else {
            EmDmBloodSet2(em, 0x62, 0, 0, 0, 0);
        }
        break;
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x11:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        EmDmBloodSet2(em, 0x62, 0, 0, 0, 0);
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xE:
    case 0xF:
    case 0x28:
    case 0x2C:
        EmDmBloodSet2(em, 0x62, 0, 0, 0, 0);
        break;
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x29:
    case 0x2D:
        return;
    case 0x14:
    case 0x15:
        break;
    }
    if (w->dmToggle) {
        if (em->ckOpen()) {
            em->setClose();
        } else {
            em->setOpen();
        }
    }
}

cEmSwitch* SetEmSwitch(void* bin, void* tpl, Vec* pos, Vec* rot, int flagNo)
{
    cEmSwitch* em;
    EmSwitchWork* w;
    u16* flg;

    flg = GetEtcFlgPtr(flagNo, pG->room_id);
    if (flg && (*flg & 1)) {
        return 0;
    }
    em = (cEmSwitch*) EmMgr.createBack(0x4B);
    if (em == 0) {
        return 0;
    }
    w = EMSWITCH_WK(em);
    w->flagNo = flagNo;
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetSwitch() failed.");
        EmMgr.destroy(em);
        return em;
    }
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 0x10);
    }
    f32 zero = 0.0f;
    AtariInit(&em->atari, zero, zero, -700.0f, 350.0f, 700.0f, 700.0f, 2000.0f, 0, 2, 0);
    em->atari.flags &= ~0x300;
    em->atari.setPriority(3);
    em->setStatus(1);
    em->setStatus(0xB);
    YarareInitCube((cEmHit*) em, zero, -300.0f, zero, 250.0f, 600.0f, 200.0f, 0, 1);
    em->hpMax = 1000;
    em->hp = 0;
    if (pos) {
        em->pos = *pos;
    } else {
        em->pos.x = zero;
        em->pos.y = zero;
        em->pos.z = zero;
    }
    em->oldPos = em->pos;
    if (rot) {
        em->rot = *rot;
    }
    w->state = 1;
    w->opened = 1;
    w->pBarred = 0;
    w->pBarred2 = 0;
    w->pConnect = 0;
    w->mode = 0;
    w->dmToggle = 1;
    w->barrel = 0;
    w->timer = 0;
    w->ckDist = 1500.0f;
    em->setActButton(1);
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
    return em;
}

void cEmSwitch::move()
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    if (w->timer) {
        w->timer--;
    }
    emSwitchDmCk(this);
    EmSwitch_R1_move_tbl[xFD](this);
    EmAtCheck(this);
}

void emSwitch_R1_Set(cEmSwitch* em)
{
    em->matUpdate();
    emSwitchOperationActEvtCk(em);
}

void emSwitch_R1_Open(cEmSwitch* em)
{
    EmSwitchWork* w = EMSWITCH_WK(em);
    cModel* p;

    switch (em->xFE) {
    case 0:
        SndCall(6, 0x23, &em->pos, 0, 0, em);
        em->xFE++;
    case 1:
        p = em->getPartsPtr(1);
        p->rot.x -= 0.17453292f;
        if (p->rot.x < 0.0f) {
            p->rot.x = 0.0f;
            if (w->pBarred) {
                w->pBarred->setOpen(0);
            }
            if (w->pBarred2) {
                w->pBarred2->setOpen(0);
            }
            w->state = 1;
            if (w->mode == 2) {
                w->state = 0;
                w->opened = 0;
                em->xFC = 1;
                em->xFD = 2;
                em->xFE = 0;
                em->xFF = 0;
            } else {
                em->xFC = 1;
                em->xFD = 0;
                em->xFE = 0;
                em->xFF = 0;
            }
        }
        break;
    }
    em->matUpdate();
}

void emSwitch_R1_Close(cEmSwitch* em)
{
    EmSwitchWork* w = EMSWITCH_WK(em);
    cModel* p;

    switch (em->xFE) {
    case 0:
        SndCall(6, 0x23, &em->pos, 0, 0, em);
        em->xFE++;
    case 1:
        p = em->getPartsPtr(1);
        p->rot.x += 0.17453292f;
        if (p->rot.x > 1.3613569f) {
            p->rot.x = 1.3613569f;
            if (w->pBarred) {
                w->pBarred->setClose(0);
            }
            if (w->pBarred2) {
                w->pBarred2->setClose(0);
            }
            if (w->barrel && w->timer == 0) {
                Vec pos;
                Vec rot;

                pos.x = -63.0f;
                pos.y = 20000.0f;
                pos.z = -11699.0f;
                rot.x = 0.0f;
                rot.y = 1.3744467f;
                rot.z = 0.0f;
                SetR227Barrel(&pos, &rot);
                w->timer = 150;
            }
            w->state = 2;
            if (w->mode == 3) {
                w->state = 0;
                w->opened = 1;
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
        }
        break;
    }
    em->matUpdate();
}

int cEmSwitch::ckSwitch()
{
    return EMSWITCH_WK(this)->state;
}

int cEmSwitch::ckOpen()
{
    if (EMSWITCH_WK(this)->opened) {
        return 1;
    }
    return 0;
}

void cEmSwitch::setOpen()
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    if (w->state == 2) {
        w->state = 0;
        w->opened = 1;
        xFC = 1;
        xFD = 1;
        xFE = 0;
        xFF = 0;
        if (w->pConnect) {
            w->pConnect->setOpen();
        }
    }
}

void cEmSwitch::setClose()
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    if (w->state == 1 && w->mode != 1) {
        w->state = 0;
        w->opened = 0;
        xFC = 1;
        xFD = 2;
        xFE = 0;
        xFF = 0;
        if (w->pConnect) {
            w->pConnect->setClose();
        }
    }
}

void cEmSwitch::setOpened()
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    getPartsPtr(1)->rot.x = 0.0f;
    w->state = 1;
    w->opened = 1;
}

void cEmSwitch::setClosed()
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    getPartsPtr(1)->rot.x = 1.3613569f;
    w->state = 2;
    w->opened = 0;
}

void cEmSwitch::setBarred(cEmBarred* b)
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    w->pBarred = b;
    if (w->state == 1) {
        b->setOpened();
    }
    if (w->state == 2) {
        b->setClosed();
    }
}

void cEmSwitch::setBarred2nd(cEmBarred* b)
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    w->pBarred2 = b;
    if (w->state == 1) {
        b->setOpened();
    }
    if (w->state == 2) {
        b->setClosed();
    }
}

void cEmSwitch::setConnectSwitch(cEmSwitch* s)
{
    EmSwitchWork* w = EMSWITCH_WK(this);

    w->pConnect = s;
    if (w->state == 1) {
        s->setOpened();
    }
    if (w->state == 2) {
        s->setClosed();
    }
}

void cEmSwitch::setActButton(int on)
{
    EMSWITCH_WK(this)->actButton = on;
}

void emSwitchOperationActEvtCk(cEmSwitch* em)
{
    EmSwitchWork* w = EMSWITCH_WK(em);
    f32 dz;
    f32 dx;

    if (w->actButton == 0) {
        return;
    }
    if (w->state == 0) {
        return;
    }
    dz = em->pos.z - pPL->pos.z;
    dx = em->pos.x - pPL->pos.x;
    if (dx * dx + dz * dz > w->ckDist * w->ckDist) {
        return;
    }
    if (fabsf(em->pos.y - (pPL->pos.y + 1000.0f)) > 1000.0f) {
        return;
    }
    if (fabsf(Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f)) > 0.78539819f) {
        return;
    }
    if (em->type != 1) {
        if (fabsf(Muku(&em->pos, &pPL->pos, em->rot.y, 3.1415927f)) > 1.5707964f) {
            return;
        }
    }
    if (w->state == 2) {
        ActBtn.set(0x14, 5, (int) emSwitchActOpen, (int) em, 0, 1, 0, 0);
    }
    if (w->state == 1) {
        if (w->mode != 1) {
            ActBtn.set(0x14, 5, (int) emSwitchActClose, (int) em, 0, 1, 0, 0);
        }
    }
}

void emSwitchActOpen(cEmSwitch* em)
{
    em->setOpen();
}

void emSwitchActClose(cEmSwitch* em)
{
    em->setClose();
}

void cEmSwitch::setOpenOnly()
{
    EMSWITCH_WK(this)->mode = 1;
}

void cEmSwitch::setAutoOpen()
{
    EMSWITCH_WK(this)->mode = 3;
}

void cEmSwitch::setBarrel()
{
    EMSWITCH_WK(this)->barrel = 1;
}

void cEmSwitch::setLongCk()
{
    EMSWITCH_WK(this)->ckDist = 2000.0f;
}
