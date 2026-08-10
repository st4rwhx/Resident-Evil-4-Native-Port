// game/embarrel.cpp: barrel enemy (cEmBarrel): explosive barrels that blow up when shot and the
// burning barrel of room 227 that rolls down the EMI route, running over the player and enemies.

#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "embarrel.h"
#include "emhit.h"
#include "etc_model.h"
#include "esp.h"
#include "snd.h"
#include "rnd.h"
#include "quake.h"
#include "pad.h"
#include "player.h"
#include "pl_wep.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" {
void EtcSetAddAmb(cModel* m, int a);                                                         // EtcModel.cpp
void LifeDownSet(cEm* em, int dmg, int a);                                                  // em_sub.cpp
void EmAtCheck(cEm* em);                                                                     // at_mod.cpp
u8 EspPullCoreKind();                                                                        // eff_sys.cpp
void EffectEspDelete(int a, int b, cModel* m, int c);                                        // est.cpp
void EffectEspgenDelete(int a, int b, cModel* m);
void EffectEfmDelete(int a, int b, cModel* m);
}

typedef void (*EmBarrelFunc)(cEmBarrel*);

static EmBarrelFunc EmBarrel_R0_move_tbl[4] = {
    emBarrel_R0_Init,
    emBarrel_R0_Move,
    0,
    0,
};

static EmBarrelFunc EmBarrel_R1_move_tbl[3] = {
    emBarrel_R1_Set,
    emBarrel_R1_Break,
    emBarrel_R1_R227Roll,
};

// SetBarrel and SetR227Barrel share the failure report and the light area origin: both live in
// inline helpers parsed before either function, which is where the string and `ofs` sit in .rodata
// (before SetBarrel's own `size` and constant pool).
static inline void barrelInitFailed(cEmBarrel* em)
{
    pLog->err(0, 0, "SetBarrel() failed.");
    EmMgr.destroy(em);
}

// One `.rodata` copy of the light offset shared by SetBarrel and SetR227Barrel (an inline parsed
// before both); the address must be an argument expression so that `&ofs` is evaluated before `&size`.
static inline const Vec* barrelLightOfs()
{
    static const Vec ofs = { 0.0f, 0.0f, 0.0f };

    return &ofs;
}

cEmBarrel* SetBarrel(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type, int etcNo)
{
    cEmBarrel* em;
    EmBarrelWork* w;
    u16* flg;
    int zero;

    em = (cEmBarrel*) EmMgr.create(0x48);
    if (em == 0) {
        return 0;
    }
    w = EMBARREL_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->rot = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        barrelInitFailed(em);
        return 0;
    }
    em->type = type;
    switch (em->type) {
    case 0:
        EtcSetAddAmb(em, 10);
        break;
    case 1:
    default:
        EtcSetAddAmb(em, 1);
        break;
    case 2:
        EtcSetAddAmb(em, 1);
        break;
    }
    w->eff = 0xFF;
    {
        cAtariInfo* at = &em->atari;

        atariInitF(at, 0.0f, 750.0f, 0.0f, 300.0f, 300.0f, 300.0f, 750.0f, 1, 0x2000, 10);
        at->setPriority(3);
        at->flags &= ~0x100;
    }
    if (em->type != 1) {
        YarareInitCube((cEmHit*) em, 0.0f, 0.0f, 0.0f, 330.0f, 1250.0f, 330.0f, 0, 1);
    } else {
        YarareInit((cEmHit*) em, 0.0f, 0.0f, 0.0f, 700.0f, 1250.0f, 1, 3);
    }
    em->hpMax = em->hp = 1000;
    {
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->lightInfo.init2(0, 1, barrelLightOfs(), &size, 0x10);
    }
    zero = 0;
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(1);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    w->etcNo = etcNo;
    w->sndId = 0;
    w->bombTimer = 0;
    w->flags = zero;
    flg = GetEtcFlgPtr(etcNo, pG->room_id);
    if (flg && (*flg & 1)) {
        em->hp = 0;
    }
    if (em->hp <= 0) {
        em->clearStatus(5);
        em->xFC = 1;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
    } else {
        em->setStatus(5);
        em->xFC = 1;
        em->xFD = 0;
        em->xFE = 0;
        em->xFF = 0;
    }
    emBarrelEatSet(em);
    return em;
}

cEmBarrel* SetR227Barrel(Vec* pos, Vec* rot)
{
    cEmBarrel* em;
    EmBarrelWork* w;
    int zero;

    if ((pGS->room_id32 & 0xFFFF0000) != 0x02270000) {
        return 0;
    }
    em = (cEmBarrel*) EmMgr.create(0x48);
    if (em == 0) {
        return 0;
    }
    w = EMBARREL_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->rot = *rot;
    }
    if (em->modelInit(ROOM_ARC_PTR(pG->pRoomArc, 0x20), ROOM_ARC_PTR(pG->pRoomArc, 0x21)) == 0) {
        barrelInitFailed(em);
        return 0;
    }
    em->type = 1;
    w->eff = 0xFF;
    w->espKind = EspPullCoreKind();
    zero = 0;
    {
        cAtariInfo* at = &em->atari;

        atariInitF(at, 0.0f, 750.0f, 0.0f, 300.0f, 300.0f, 300.0f, 750.0f, 1, 0x2000, 10);
        at->setPriority(3);
        at->flags &= ~0x100;
    }
    YarareInit((cEmHit*) em, -350.0f, 0.0f, 0.0f, 700.0f, 1250.0f, 1, 3);
    em->hpMax = em->hp = 1000;
    {
        static const Vec size = { 4000.0f, 4000.0f, 4000.0f };

        em->lightInfo.init2(0, 1, barrelLightOfs(), &size, 0x10);
    }
    em->lockParts = zero;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(1);
    em->be_flag &= ~0x01000000;
    em->be_flag &= ~0x10;
    em->xFC = 1;
    em->xFD = 2;
    w->flags = 0;
    em->xFE = zero;
    em->xFF = 0;
    return em;
}

void emBarrelDmCk(cEmBarrel* em)
{
    u8 wep;
    Vec hit;

    if (em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, &hit)) {
        case 1:
        case 4:
        case 5:
        case 7:
            emBarrelSetBreak(em, 2);
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
    em->hp = 0;
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
        emBarrelSetBreak(em, 0);
        break;
    case 7:
    case 8:
    case 0x21:
        if (em->dmRad > 36000000.0f) {
            emBarrelSetBreak(em, 0);
        } else {
            emBarrelSetBreak(em, 1);
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
    default:
        emBarrelSetBreak(em, 2);
        break;
    }
}

void emBarrelDmCk2(cEmBarrel* em)
{
    EmHitInfo* part;
    u8 wep;
    int dmg;
    int near;
    Vec hit;

    if (em->hp > 0) {
        switch (DmgMgr.hitCheck(&em->pos, &hit)) {
        case 1:
        case 4:
        case 5:
        case 7:
            emBarrelSetBreak(em, 2);
            return;
        }
    }
    if (em->dmHit == 0) {
        return;
    }
    em->dmHit = 0;
    part = em->dmPart;
    near = 0;
    if (part->rad < 36000000.0f) {
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
    if (wep == 0xE) {
        return;
    }
    em->dmType = 1;
    switch (wep) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0x11:
    case 0x26:
    case 0x2B:
        dmg = 500;
        break;
    case 0xB:
    case 0xC:
    case 0x1B:
    case 0x1D:
    case 0x27:
        dmg = 400;
        break;
    case 9:
    case 0xA:
    case 0x10:
    case 0x14:
    case 0x15:
    case 0x28:
        dmg = 1000;
        break;
    case 7:
    case 8:
    case 0x21:
        if (near) {
            dmg = 9999;
        } else {
            dmg = 500;
        }
        break;
    case 5:
    case 6:
    case 0xD:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x29:
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
    if (em->hp <= 0) {
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
            emBarrelSetBreak(em, 0);
            break;
        case 7:
        case 8:
        case 0x21:
            if (em->dmRad > 36000000.0f) {
                emBarrelSetBreak(em, 0);
            } else {
                emBarrelSetBreak(em, 1);
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
        default:
            emBarrelSetBreak(em, 2);
            break;
        }
    } else {
        EmDmBloodSet2(em, 1, 2, 0, 0, 0);
    }
}

void emBarrelSetBreak(cEmBarrel* em, int kind)
{
    EmBarrelWork* w = EMBARREL_WK(em);

    em->hp = 0;
    em->be_flag &= ~2;
    SndStop(w->sndId, 0);
    if (em->type != 1) {
        if (w->eff != 0xFF) {
            emBarrelSetBomb(em);
        }
    } else {
        EffectEspDelete(0, w->espKind, em, 0);
        EffectEspgenDelete(0, w->espKind, em);
        EffectEfmDelete(0, w->espKind, em);
        if (w->rollSe != 0) {
            EstSet(0, -1, &em->pos, 0, 1, 5, 0, 0, 0, 0);
            emBarrelSetBomb2(em);
            SndCall(6, 3, &em->pos, 0, 0, em);
        } else {
            EstSet(0, -1, &em->pos, 0, 1, 6, 0, 0, 0, 0);
            SndCall(6, 3, &em->pos, 0, 0, em);
        }
    }
    em->xFC = 1;
    em->xFD = 1;
    em->xFE = 0;
    em->xFF = 0;
}

void cEmBarrel::move()
{
    EmBarrelWork* w = EMBARREL_WK(this);

    if (type != 1) {
        emBarrelDmCk(this);
    } else {
        emBarrelDmCk2(this);
    }
    be_flag &= ~0x4000;
    EmBarrel_R0_move_tbl[xFC](this);
    if ((be_flag & 0x201) == 1) {
        EmAtCheck(this);
        atari.move();
        emBarrelEatSet(this);
        if (w->bombTimer) {
            w->bombTimer--;
            if (w->bombTimer == 0) {
                PlWepHitCheck2(0, &w->bombPos, &w->bombPos, 0x13, 3, w->bombRange);
                pG->flags_500C |= 0x00800000;
            }
        }
    }
}

void emBarrel_R0_Init(cEmBarrel* em)
{
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
}

void emBarrel_R0_Move(cEmBarrel* em)
{
    EmBarrel_R1_move_tbl[em->xFD](em);
}

void emBarrel_R1_Set(cEmBarrel* em)
{
    EmBarrelWork* w = EMBARREL_WK(em);

    if (em->xFE == 0) {
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        em->partsMatCalc();
        em->partsWorldCalc();
        w->timer = 30;
        em->xFE++;
    }
    em->be_flag |= 0x4000;
}

void emBarrel_R1_Break(cEmBarrel* em)
{
    EmBarrelWork* w = EMBARREL_WK(em);
    u16* flg;

    switch (em->xFE) {
    case 0:
        if (em->type == 0 || em->type == 2) {
            flg = GetEtcFlgPtr(w->etcNo, pG->room_id);
            if (flg) {
                *flg |= 1;
            }
        }
        em->be_flag &= ~2;
        em->hp = 0;
        em->atari.flags &= ~0x200;
        em->clearStatus(5);
        w->timer = 10;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else if (em->type == 1) {
            EmMgr.destroy(em);
        }
        break;
    }
}

void emBarrel_R1_R227Roll(cEmBarrel* em)
{
    EmBarrelWork* w = EMBARREL_WK(em);
    cModel* p;
    f32 floor;
    f32 ang;
    f32 dist;
    f32 spin;

    switch (em->xFE) {
    case 0:
        if (emBarrelSetRollRoute(em) == 0) {
            em->pos.x = em->mat[0][3];
            em->pos.y = em->mat[1][3];
            em->pos.z = em->mat[2][3];
            Matrix2AxisAngle(em->mat, &em->rot);
            em->xFC = 1;
            em->xFD = 1;
            em->xFE = 0;
            em->xFF = 0;
            return;
        }
        em->atari.throughOn();
        w->rollSe = 0;
        if ((Rnd() & 3) == 0) {
            w->rollSe = 1;
            EstSet((int) em, -1, 0, 0, 1, 4, 0, w->espKind, (u32) em, 0);
        }
        w->seTimer = 0;
        w->floorOfs = 700.0f;
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 0.0f;
        em->xFE++;
    case 1:
        if (emBarrelSetRollSpd(em)) {
            emBarrelSetBreak(em, 0);
            return;
        }
    default:
        if (w->rollSe) {
            if (w->seTimer) {
                w->seTimer--;
            } else {
                w->sndId = SndCall(6, 6, &em->pos, 0, 0, em);
                w->seTimer = 30;
            }
        }
        w->spd.y -= 10.0f;
        PSVECAdd(&em->pos, &w->spd, &em->pos);
        floor = EatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
        if (em->pos.y < floor + w->floorOfs) {
            em->pos.y = floor + w->floorOfs;
            w->spd.y *= -0.3f;
            if (w->spd.y > 20.0f) {
                Vec v;

                v = em->pos;
                v.y -= w->floorOfs;
                EstSet(0, -1, &v, 0, 1, 3, 0, 0, 0, 0);
                SndCall(6, 2, &em->pos, 0, 0, em);
            }
        }
        ang = GetXZAngle(&em->oldPos, &em->pos);
        em->rot.y += Muku2(em->rot.y, ang, 0.012271847f);
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        dist = SQRTF((em->pos.x - em->oldPos.x) * (em->pos.x - em->oldPos.x) +
                     (em->pos.y - em->oldPos.y) * (em->pos.y - em->oldPos.y) +
                     (em->pos.z - em->oldPos.z) * (em->pos.z - em->oldPos.z));
        if (dist > 500.0f) {
            dist = 500.0f;
        }
        spin = dist * 0.002f * 0.31415927f;
        p = em->getPartsPtr(0);
        p->rot.x += spin;
        p->rot.x = LIMIT_ANGLE(p->rot.x);
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        em->partsMatCalc();
        em->partsWorldCalc();
        if (emBarrelRollHitCk(em)) {
            emBarrelSetBreak(em, 0);
        } else {
            emBarrelRunDownCk(em);
        }
        break;
    }
}

int emBarrelSetRollRoute(cEmBarrel* em)
{
    EmBarrelWork* w = EMBARREL_WK(em);
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

int emBarrelSetRollSpd(cEmBarrel* em)
{
    EmBarrelWork* w = EMBARREL_WK(em);
    u8* emi;
    EmiEntry* e;
    int idx;
    int i;
    f32 spd;
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
#line 1017 "D:/Bio4/Prog/embarrel.cpp"
    VECNormalize(&dir, &dir);
    spd = SQRTF(w->spd.x * w->spd.x + w->spd.z * w->spd.z) + 1.0f;
    if (spd < 50.0f) {
        spd = 50.0f;
    }
    if (spd > 150.0f) {
        spd = 150.0f;
    }
    PSVECScale(&dir, &dir, spd);
    w->spd.x = dir.x;
    w->spd.z = dir.z;
    return 0;
}

void cEmBarrel::setEff(u8 eff)
{
    EmBarrelWork* w = EMBARREL_WK(this);

    w->eff = eff;
}

void emBarrelSetBomb(cEmBarrel* em)
{
    EmBarrelWork* w = EMBARREL_WK(em);
    Camera* cam;
    cModel* p;
    Vec v;
    f32 d2;
    f32 power;

    em->hp = 0;
    em->be_flag &= ~2;
    switch (em->type) {
    case 0:
    default:
        SndCall(6, 0x48, &em->pos, 0, 0, em);
        break;
    case 1:
        SndCall(6, 8, &em->pos, 0, 0, em);
        break;
    case 2:
        if (pG->room_id == 0x404) {
            SndCall(6, 0x53, &em->pos, 0, 0, em);
        } else {
            SndCall(6, 0x56, &em->pos, 0, 0, em);
        }
        break;
    }
    EstSet(0, -1, &em->pos, &em->rot, w->eff, 0, 0, 0, 0, 0);
    v = em->pos;
    v.y += 500.0f;
    w->bombTimer = 2;
    w->bombPos = v;
    w->bombRange = 6000.0f;
    cam = &pGS->Cam;
    p = em->getPartsPtr(1);
    d2 = (p->worldPos.x - cam->param.pos.x) * (p->worldPos.x - cam->param.pos.x) +
         (p->worldPos.y - cam->param.pos.y) * (p->worldPos.y - cam->param.pos.y) +
         (p->worldPos.z - cam->param.pos.z) * (p->worldPos.z - cam->param.pos.z);
    if (d2 < 400000000.0f) {
        power = 10.0f;
        if (d2 > 25000000.0f) {
            power = 8.0f;
        }
        if (d2 > 100000000.0f) {
            power = 6.0f;
        }
        if (d2 > 225000000.0f) {
            power = 4.0f;
        }
        QuakeExec(0, 0, 5, power, 2);
    }
}

void emBarrelSetBomb2(cEmBarrel* em)
{
    EmBarrelWork* w = EMBARREL_WK(em);
    Camera* cam;
    cModel* p;
    Vec v;
    f32 d2;
    f32 power;

    em->hp = 0;
    em->be_flag &= ~2;
    switch (em->type) {
    case 0:
    default:
        SndCall(6, 0x48, &em->pos, 0, 0, em);
        break;
    case 1:
        SndCall(6, 8, &em->pos, 0, 0, em);
        break;
    case 2:
        SndCall(6, 0x56, &em->pos, 0, 0, em);
        break;
    }
    v = em->pos;
    v.y += 500.0f;
    w->bombTimer = 2;
    w->bombPos = v;
    w->bombRange = 4000.0f;
    cam = &pGS->Cam;
    p = em->getPartsPtr(1);
    d2 = (p->worldPos.x - cam->param.pos.x) * (p->worldPos.x - cam->param.pos.x) +
         (p->worldPos.y - cam->param.pos.y) * (p->worldPos.y - cam->param.pos.y) +
         (p->worldPos.z - cam->param.pos.z) * (p->worldPos.z - cam->param.pos.z);
    if (d2 < 400000000.0f) {
        power = 10.0f;
        if (d2 > 25000000.0f) {
            power = 8.0f;
        }
        if (d2 > 100000000.0f) {
            power = 6.0f;
        }
        if (d2 > 225000000.0f) {
            power = 4.0f;
        }
        QuakeExec(0, 0, 5, power, 2);
    }
}

void emBarrelEatSet(cEmBarrel* em)
{
    EmBarrelWork* w = EMBARREL_WK(em);
    Vec v[4];

    if (em->type == 1) {
        return;
    }
    if (w->sat) {
        w->sat->flags &= ~4;
    }
    if (em->hp <= 0) {
        return;
    }
    if (w->sat == 0) {
        f32 r = 330.0f;

        v[0].x = -r;
        v[0].y = 0.0f;
        v[0].z = -r;
        v[1].x = r;
        v[1].y = 0.0f;
        v[1].z = -r;
        v[2].x = r;
        v[2].y = 0.0f;
        v[2].z = r;
        v[3].x = -r;
        v[3].y = 0.0f;
        v[3].z = r;
        w->sat = EatMgr.create(&em->pos, &em->rot, v, 0x400000, 0, 1250.0f);
    } else {
        w->sat->flags |= 4;
        w->sat->setCoord(&em->pos, &em->rot);
    }
}

int emBarrelRollHitCk(cEmBarrel* em)
{
    Mtx inv;
    Vec v;
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
    PSMTXInverse(em->mat, inv);
    PSMTXMultVec(inv, &pPL->pos, &v);
    if (v.x > 1250.0f) {
        return 0;
    }
    if (v.x < -1250.0f) {
        return 0;
    }
    if (v.y > 700.0f) {
        return 0;
    }
    if (v.y < -2500.0f) {
        return 0;
    }
    if (v.z > 700.0f) {
        return 0;
    }
    if (v.z < -700.0f) {
        return 0;
    }
    LifeDownSet(pPL, 600, 0);
    PlSetDamage(8, 0, 0);
    VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
    QuakeExec(0, 0, 5, 22.0f, 2);
    return 1;
}

void emBarrelRunDownCk(cEmBarrel* em)
{
    Mtx inv;
    Vec v;
    cEm* e;
    u32 i;

    PSMTXInverse(em->mat, inv);
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
        PSMTXMultVec(inv, &e->pos, &v);
        if (v.x > 1250.0f) {
            continue;
        }
        if (v.x < -1250.0f) {
            continue;
        }
        if (v.y > 700.0f) {
            continue;
        }
        if (v.y < -2500.0f) {
            continue;
        }
        if (v.z > 700.0f) {
            continue;
        }
        if (v.z < -700.0f) {
            continue;
        }
        e->hp = 0;
        e->xFC = 3;
        e->xFD = 4;
        e->xFE = 0;
        e->xFF = 0;
    }
}
