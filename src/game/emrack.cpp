// game/emrack.cpp: rack enemy (cEmRack): pushable shelves / crates that fall over when shot,
// shake when kicked and break (etc flag) on heavy damage.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "emrack.h"
#include "emhit.h"
#include "etc_model.h"
#include "snd.h"
#include "motion.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" {
void EtcSetAddAmb(cModel* m, int a);                                                         // EtcModel.cpp
void EmAtCheck(cEm* em);                                                                     // at_mod.cpp
void Em_R0_Scenario(cEm* em);                                                                // em_sub.cpp
// esp.h declares the effect id as int; this unit passes the u8 `eff` byte straight into r7
// (emRack_R1_Break: the byte load is shared by the compare and the calls), so it carries the
// prototype with a u8 parameter.
void EstSet(int a, int b, Vec* pos, Vec* rot, u8 c, int d, int e, int f, u32 g, void* h);
}

typedef void (*EmRackFunc)(cEmRack*);

static EmRackFunc EmRack_R0_move_tbl[5] = {
    emRack_R0_Init,
    emRack_R0_Move,
    0,
    0,
    (EmRackFunc) Em_R0_Scenario,
};

EmRackFunc EmRack_R1_move_tbl[4] = {
    emRack_R1_Set,
    emRack_R1_Down,
    emRack_R1_Break,
    emRack_R1_Shock,
};

cEmRack* SetRack(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type, int etcNo)
{
    cEmRack* em;
    EmRackWork* w;
    u16* flg;
    int zero;

    em = (cEmRack*) EmMgr.create(0x45);
    if (em == 0) {
        return 0;
    }
    w = EMRACK_WK(em);
    w->etcNo = etcNo;
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->rot = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetRack() failed.");
        EmMgr.destroy(em);
        return 0;
    }
    EtcSetAddAmb(em, 8);
    w->eff = 0xFF;
    em->type = type;
    switch (em->type) {
    case 0:
    default:
        w->size.x = 700.0f;
        w->size.y = 1000.0f;
        w->size.z = 400.0f;
        break;
    case 1:
        w->size.x = 700.0f;
        w->size.y = 2000.0f;
        w->size.z = 400.0f;
        break;
    case 2:
        w->size.x = 750.0f;
        w->size.y = 1500.0f;
        w->size.z = 750.0f;
        break;
    case 3:
        w->size.x = 500.0f;
        w->size.y = 4100.0f;
        w->size.z = 500.0f;
        break;
    case 5:
        w->size.x = 600.0f;
        w->size.y = 2400.0f;
        w->size.z = 600.0f;
        break;
    case 4:
        w->size.x = 1650.0f;
        w->size.y = 2250.0f;
        w->size.z = 1400.0f;
        break;
    }
    em->hpMax = em->hp = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 0x10);
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(1);
    em->setStatus(0xB);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    em->rackFlags = 0xF;
    {
        cAtariInfo* at = &em->atari;

        at->init(0, 2, 0, 0.0f, w->size.y * 0.5f, 0.0f, w->size.x - 100.0f, w->size.z - 100.0f,
                 w->size.z - 100.0f, w->size.y * 0.5f);
        at->setPriority(3);
        at->flags &= ~0x100;
    }
    w->xEC = zero;
    w->sat[2] = 0;
    w->sat[1] = 0;
    w->sat[0] = 0;
    emRackYarareInit(em);
    w->xE8 = 0.0f;
    w->flags = 0;
    if (em->type == 1) {
        w->xE8 = 1.0f;
    }
    w->etcNo = etcNo;
    flg = GetEtcFlgPtr(etcNo, pG->room_id);
    if (flg && (*flg & 1)) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        em->xFC = 1;
        em->xFD = 2;
        em->xFE = 0;
        em->xFF = 4;
    } else {
        em->xFC = 1;
        em->xFD = 0;
        em->xFE = 0;
        em->xFF = 0;
    }
    emRackSatSet(em);
    return em;
}

void emRackDmCk(cEmRack* em)
{
    EmRackWork* w = EMRACK_WK(em);
    EmHitInfo* part;
    u8 wep;
    int type;
    Vec hit;

    if (em->hp > 0) {
        type = em->type;
        if (type >= 0) {
            if (type <= 1) {
                switch (DmgMgr.hitCheck(&em->pos, &hit)) {
                case 1:
                case 4:
                case 5:
                case 7:
                    em->hp = 0;
                    em->xFC = 1;
                    em->xFD = 2;
                    em->xFE = 0;
                    em->xFF = 0;
                    return;
                }
            }
        }
    }
    if (em->dmHit == 0) {
        return;
    }
    em->dmHit = 0;
    if ((int) em->flags_3C8 < 0) {
        return;
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
    if (em->dmWep == 0x10) {
        em->dmType = 0x11;
    }
    switch (em->type) {
    case 2:
    case 3:
    case 5:
        if (w->eff != 0xFF) {
            EmDmBloodSet2(em, w->eff, 1, 0, 0, 0);
        }
        return;
    }
    part = em->dmPart;
    switch (em->dmWep) {
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
        if (w->eff != 0xFF) {
            EmDmBloodSet2(em, w->eff, 1, 0, 0, 0);
        }
        break;
    case 7:
    case 8:
        if (part->rad < 36000000.0f) {
            if (w->xE8 <= 0.0f) {
                em->xFC = 1;
                em->xFD = 2;
                em->xFE = 0;
                em->xFF = 3;
                return;
            }
            if (part->partsNo != 0) {
                cModel* p;

                if (w->eff != 0xFF) {
                    EstSet((int) em, -1, 0, 0, w->eff, 6, 0, 0, (u32) em, 0);
                }
                SndCall(6, 0x36, &em->pos, 0, 0, em);
                p = em->getPartsPtr(1);
                p->scale.x = 0.0f;
                p->scale.y = 0.0f;
                p->scale.z = 0.0f;
                part->flags &= ~1;
                return;
            }
            w->xE8 = 0.0f;
        }
        if (w->eff != 0xFF) {
            EmDmBloodSet2(em, w->eff, 2, 0, 0, 0);
        }
        break;
    case 5:
    case 6:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x15:
    case 0x29:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    default:
        em->xFC = 1;
        em->xFD = 2;
        em->xFE = 0;
        em->xFF = 2;
        break;
    case 0:
    case 0x14:
        em->setDown(&em->x328);
        break;
    }
}

void cEmRack::move()
{
    emRackDmCk(this);
    EmRack_R0_move_tbl[xFC](this);
    if (hp > 0) {
        EmAtCheck(this);
        atari.move();
    }
    emRackSatSet(this);
}

void emRack_R0_Init(cEmRack* em)
{
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
}

void emRack_R0_Move(cEmRack* em)
{
    EmRack_R1_move_tbl[em->xFD](em);
}

void emRack_R1_Set(cEmRack* em)
{
    EmRackWork* w = EMRACK_WK(em);

    if (MotionCheckCrossFrame((MotionWork*) &em->pMotion, 2.0f)) {
        if (w->eff != 0xFF) {
            if (em->type == 1) {
                EstSet((int) em, -1, 0, 0, w->eff, 7, 0, 0, (u32) em, 0);
            } else {
                EstSet((int) em, -1, 0, 0, w->eff, 5, 0, 0, (u32) em, 0);
            }
        }
    }
    em->matUpdate();
}

void emRack_R1_Down(cEmRack* em)
{
    EmRackWork* w = EMRACK_WK(em);
    cModel* p;
    int done;

    switch (em->xFE) {
    case 0:
        w->downSpd = 0.0f;
        em->hp = 0;
        em->xFE++;
    case 1:
        p = em->getPartsPtr(0);
        done = 0;
        switch (em->xFF) {
        case 0:
        default:
            p->rot.x += w->downSpd;
            if (p->rot.x > 1.2566371f) {
                done = 1;
            }
            break;
        case 1:
            p->rot.x -= w->downSpd;
            if (p->rot.x < -1.2566371f) {
                done = 1;
            }
            break;
        case 2:
            p->rot.z += w->downSpd;
            if (p->rot.z > 1.2566371f) {
                done = 1;
            }
            break;
        case 3:
            p->rot.z -= w->downSpd;
            if (p->rot.z < -1.2566371f) {
                done = 1;
            }
            break;
        }
        w->downSpd += 0.01f;
        if (done) {
            em->xFC = 1;
            em->xFD = 2;
            em->xFE = 0;
            em->xFF = 1;
        }
        break;
    }
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

void emRack_R1_Break(cEmRack* em)
{
    EmRackWork* w = EMRACK_WK(em);
    u16* flg;

    if (em->xFE == 0) {
        em->hp = 0;
        em->be_flag &= ~2;
        flg = GetEtcFlgPtr(w->etcNo, pGS->room_id);
        if (flg) {
            *flg |= 1;
        }
        switch (em->type) {
        default:
            if (w->eff == 0xFF) {
                break;
            }
            switch (em->xFF) {
            case 0:
            default:
                EstSet((int) em, -1, 0, 0, w->eff, 3, 0, 0, (u32) em, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 1:
                EstSet((int) em, -1, 0, 0, w->eff, 5, 0, 0, (u32) em, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 2:
                EstSet((int) em, -1, 0, 0, w->eff, 0, 0, 0, (u32) em, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 3:
                EstSet((int) em, -1, 0, 0, w->eff, 4, 0, 0, (u32) em, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 4:
                break;
            }
            break;
        case 4:
            if (w->eff == 0xFF) {
                break;
            }
            switch (em->xFF) {
            case 0:
            default:
                EstSet(0, -1, &em->pos, &em->rot, w->eff, 3, 0, 0, 0, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 1:
                EstSet(0, -1, &em->pos, &em->rot, w->eff, 5, 0, 0, 0, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 2:
                EstSet(0, -1, &em->pos, &em->rot, w->eff, 0, 0, 0, 0, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 3:
                EstSet(0, -1, &em->pos, &em->rot, w->eff, 4, 0, 0, 0, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            case 4:
                break;
            case 5:
                EstSet(0, -1, &em->pos, &em->rot, w->eff, 3, 0, 0, 0, 0);
                SndCall(6, 0x33, &em->pos, 0, 0, em);
                break;
            }
            break;
        case 2:
        case 3:
        case 5:
            break;
        }
        emRackSatClear(em);
        em->xFE++;
    }
}

void emRack_R1_Shock(cEmRack* em)
{
    EmRackWork* w = EMRACK_WK(em);
    cModel* p;

    switch (em->xFE) {
    case 0:
        w->shockTimer = 7;
        em->hp -= 50;
        if (em->hp <= 0) {
            em->hp = 1;
        }
        SndCall(6, 0x5F, &em->pos, 0, 0, em);
        em->xFE++;
    case 1:
        p = em->getPartsPtr(0);
        if (w->shockTimer != 0) {
            w->shockTimer--;
            p->rot.x = 0.0f;
            if (pGS->flags_51E4 & 1) {
                p->rot.x = fRand0_1() * 0.024543693f + 0.024543693f;
            }
        } else {
            p->rot.y = 0.0f;
            em->xFC = 1;
            em->xFD = 0;
            em->xFE = 0;
            em->xFF = 0;
        }
        break;
    }
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

void emRackSatSet(cEmRack* em)
{
    EmRackWork* w = EMRACK_WK(em);
    Vec v[4];
    f32 hx;
    f32 hz;
    f32 h;

    emRackSatClear(em);
    if (em->hp <= 0) {
        return;
    }
    {
        cAtariInfo* at = &em->atari;

        at->flags |= 0x200;
    }
    if (w->sat[0] != 0 && em->plDist2 > 225000000.0f) {
        return;
    }
    hx = w->size.x - 100.0f;
    hz = w->size.z - 100.0f;
    v[0].x = -hx;
    v[0].y = 0.0f;
    v[0].z = -hz;
    v[1].x = hx;
    v[1].y = 0.0f;
    v[1].z = -hz;
    v[2].x = hx;
    v[2].y = 0.0f;
    v[2].z = hz;
    v[3].x = -hx;
    v[3].y = 0.0f;
    v[3].z = hz;
    if (em->type == 1) {
        h = 1000.0f;
    } else {
        h = w->size.y;
    }
    if (w->sat[0] == 0) {
        w->sat[0] = EatMgr.create(&em->pos, &em->rot, v, 0x400000, 0, h);
    } else {
        w->sat[0]->flags |= 4;
        w->sat[0]->setCoord(&em->pos, &em->rot);
    }
    if (em->type != 1) {
        return;
    }
    v[0].y = 1000.0f;
    v[1].y = 1000.0f;
    v[2].y = 1000.0f;
    v[3].y = 1000.0f;
    h = 500.0f;
    if (w->sat[1] == 0) {
        w->sat[1] = EatMgr.create(&em->pos, &em->rot, v, 0x400000, 0, h);
    } else {
        w->sat[1]->flags |= 4;
        w->sat[1]->setCoord(&em->pos, &em->rot);
    }
    v[0].y = 1500.0f;
    v[1].y = 1500.0f;
    v[2].y = 1500.0f;
    v[3].y = 1500.0f;
    h = 500.0f;
    if (w->sat[2] == 0) {
        w->sat[2] = EatMgr.create(&em->pos, &em->rot, v, 0x400000, 0, h);
    } else {
        w->sat[2]->flags |= 4;
        w->sat[2]->setCoord(&em->pos, &em->rot);
    }
}

void emRackSatClear(cEmRack* em)
{
    EmRackWork* w = EMRACK_WK(em);

    em->atari.clrFlag200();
    if (w->sat[0]) {
        w->sat[0]->flags &= ~4;
    }
    if (w->sat[1]) {
        w->sat[1]->flags &= ~4;
    }
    if (w->sat[2]) {
        w->sat[2]->flags &= ~4;
    }
}

void emRackYarareInit(cEmRack* em)
{
    EmRackWork* w = EMRACK_WK(em);

    switch (em->type) {
    case 0:
    default:
        YarareInitCube((cEmHit*) em, 0.0f, 0.0f, 0.0f, w->size.x, w->size.y, w->size.z, 0, 1);
        break;
    case 1:
        YarareInitCube((cEmHit*) em, 0.0f, 0.0f, 0.0f, w->size.x, w->size.y, w->size.z, 0, 1);
        YarareAddCube((cEmHit*) em, &w->hit[0].info, 0.0f, 1800.0f, 0.0f, 700.0f, 200.0f, 400.0f, 0, 1);
        YarareAddCube((cEmHit*) em, &w->hit[1].info, -600.0f, 0.0f, 0.0f, 100.0f, w->size.y, 400.0f, 0, 1);
        YarareAddCube((cEmHit*) em, &w->hit[2].info, 600.0f, 0.0f, 0.0f, 100.0f, w->size.y, 400.0f, 0, 1);
        YarareAddCube((cEmHit*) em, &w->hit[3].info, 0.0f, 1000.0f, 0.0f, 500.0f, 800.0f, 450.0f, 2, 1);
        break;
    case 2:
        YarareInitCube((cEmHit*) em, 0.0f, 0.0f, 0.0f, w->size.x, w->size.y, w->size.z, 0, 0x41);
        break;
    case 3:
    case 5:
        YarareInitCube((cEmHit*) em, 0.0f, 0.0f, 0.0f, w->size.x, w->size.y, w->size.z, 0, 0x41);
        break;
    case 4:
        YarareInitCube((cEmHit*) em, 0.0f, 0.0f, 0.0f, w->size.x, w->size.y, w->size.z, 0, 0x41);
        break;
    }
}

void cEmRack::setBreak()
{
    if (type == 4) {
        xFC = 1;
        xFD = 2;
        xFE = 0;
        xFF = 5;
    } else {
        xFC = 1;
        xFD = 2;
        xFE = 0;
        xFF = 0;
    }
}

void cEmRack::setDown(Vec* target)
{
    f32 ang;
    f32 abs;

    if (hp <= 0) {
        return;
    }
    ang = Muku(&pos, target, rot.y, 3.1415927f);
    abs = fabsf(ang);
    if (ang < 0.0f) {
        xFF = 3;
    } else {
        xFF = 2;
    }
    if (abs < 0.78539819f) {
        xFF = 1;
    }
    if (abs > 2.3561945f) {
        xFF = 0;
    }
    if (xFF == 0 && type == 1) {
        xFC = 1;
        xFD = 1;
        xFE = 0;
    } else {
        xFC = 1;
        xFD = 2;
        xFE = 0;
        xFF = 0;
    }
}

void cEmRack::setShock()
{
    xFC = 1;
    xFD = 3;
    xFE = 0;
    xFF = 0;
}

void cEmRack::setEff(u8 eff)
{
    EmRackWork* w = EMRACK_WK(this);

    w->eff = eff;
}

void cEmRack::setRange(f32 a, f32 b, f32 c, f32 d)
{
    RotMatrix(rackMat, &rot);
    TransMatrix(rackMat, &pos);
    PSMTXInverse(rackMat, rackInvMat);
    if (a > 0.0f) {
        rackRange[0] = a;
    } else {
        rackRange[0] = 0.0f;
    }
    if (b > 0.0f) {
        rackRange[1] = b;
    } else {
        rackRange[1] = 0.0f;
    }
    if (c > 0.0f) {
        rackRange[2] = c;
    } else {
        rackRange[2] = 0.0f;
    }
    if (d > 0.0f) {
        rackRange[3] = d;
    } else {
        rackRange[3] = 0.0f;
    }
    rackFlags |= 0x10;
}

int cEmRack::adjustRange(u8 dir)
{
    Vec v;
    int ret;

    if (!(rackFlags & 0x10)) {
        return 0;
    }
    PSMTXMultVec(rackInvMat, &pos, &v);
    ret = 0;
    switch (dir) {
    case 0:
        if (v.z < -rackRange[2]) {
            v.z = -rackRange[2];
            ret = 1;
        }
        break;
    case 1:
        if (v.x > rackRange[1]) {
            v.x = rackRange[1];
            ret = 1;
        }
        break;
    case 2:
        if (v.z > rackRange[0]) {
            v.z = rackRange[0];
            ret = 1;
        }
        break;
    case 3:
        if (v.x < -rackRange[3]) {
            v.x = -rackRange[3];
            ret = 1;
        }
        break;
    }
    if (ret == 1) {
        PSMTXMultVec(rackMat, &v, &pos);
    }
    return ret;
}
