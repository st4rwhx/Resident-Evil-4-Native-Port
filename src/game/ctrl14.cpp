#include "types.h"
#include "vec.h"
#include "cManager.h"
#include "ctrl.h"
#include "atari.h"
#include "light.h"
#include "global.h"
#include "model.h"
#include "player.h"
#include "scroll.h"
#include "esp.h"
#include "snd.h"
#include "math_sub.h"

extern "C" {
u8 EspPullCoreKind();
void EffectEspgenDelete(int a, int kind, cModel* obj);
}

cCtrl* GetCtrlDragon(u32 type)
{
    cModel* obj[5];
    Vec pos;
    Vec rot;
    cCtrl* c;
    Ctrl14Work* w;
    u32 no;

    switch (type) {
    case 0:
        obj[0] = SmdGetObjPtr(0xA);
        obj[1] = SmdGetObjPtr(0xB);
        obj[3] = SmdGetObjPtr(0xD);
        no = 0xE;
        break;
    case 1:
        obj[0] = SmdGetObjPtr(0xF);
        obj[1] = SmdGetObjPtr(0x10);
        obj[3] = SmdGetObjPtr(0x12);
        no = 0x13;
        break;
    case 2:
        obj[0] = SmdGetObjPtr(0x14);
        obj[1] = SmdGetObjPtr(0x15);
        obj[3] = SmdGetObjPtr(0x16);
        no = 0x17;
        break;
    default:
        goto create;
    }
    obj[4] = SmdGetObjPtr(no);
    if (obj[0] == NULL || obj[1] == NULL || obj[3] == NULL || obj[4] == NULL) {
        return NULL;
    }
    obj[0]->be_flag |= 0x20;
    obj[1]->be_flag |= 0x20;
    obj[3]->be_flag |= 0x20;
    obj[4]->be_flag |= 0x20;
create:
    c = CtrlMgr.createBack(0x14);
    if (c == NULL) {
        return NULL;
    }
    w = (Ctrl14Work*) c->work;
    w->obj[0] = obj[0];
    w->obj[1] = obj[1];
    w->obj[3] = obj[3];
    w->obj[4] = obj[4];
    w->type = type;
    w->espKind = EspPullCoreKind();
    pos = w->obj[1]->pos;
    rot = w->obj[1]->rot;
    if (obj[0]) {
        w->sat[0] = EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 5), 0, &pos, &rot, 1);
    }
    w->sat[1] = EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 5), 0, &pos, &rot, 2);
    // pGS: the original loads pG after the sat[1] store; a plain pG here is hoisted above the argument setup
    w->sat[2] = EatMgr.create(ROOM_ARC_PTR(pGS->pRoomArc, 5), 0, &pos, &rot, 3);
    return c;
}

void cCtrl14::move()
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    Vec pos;
    Vec rot;
    u8 flags;

    if (w->fireDelay) {
        w->fireDelay--;
        if (w->fireDelay == 0) {
            EstSet((int) w->obj[1], -1, NULL, NULL, 1, 2, 1, w->espKind, (u32) w->obj[1], NULL);
            SndCall(6, 2, &w->obj[1]->pos, 0, 0, w->obj[1]);
            w->fireTimer = 60;
        }
    }
    if (w->fireTimer) {
        w->fireTimer--;
        if (w->fireTimer == 0) {
            EffectEspgenDelete(0, w->espKind, w->obj[1]);
            EstSet((int) w->obj[1], -1, NULL, NULL, 1, 4, 1, 0, (u32) w->obj[1], NULL);
        }
    }
    pos = w->obj[1]->getPartsPtr(0)->worldPos;
    rot = w->obj[1]->rot;
    if (w->obj[0]) {
        w->sat[0]->setCoord(&pos, &rot);
    }
    w->sat[1]->setCoord(&pos, &rot);
    w->sat[2]->setCoord(&pos, &rot);
    flags = w->flags;
    if (flags & 1) {
        if (!(flags & 2)) {
            w->flags = flags & ~1;
            switch (w->type) {
            case 0:
                SndCall(6, 8, &w->obj[1]->pos, 0, 0, w->obj[1]);
                break;
            case 1:
                SndCall(6, 0xA, &w->obj[1]->pos, 0, 0, w->obj[1]);
                break;
            case 2:
                SndCall(6, 0xC, &w->obj[1]->pos, 0, 0, w->obj[1]);
                break;
            }
        }
    } else {
        if (flags & 2) {
            w->flags = flags | 1;
            switch (w->type) {
            case 0:
                SndCall(6, 7, &w->obj[1]->pos, 0, 0, w->obj[1]);
                break;
            case 1:
                SndCall(6, 9, &w->obj[1]->pos, 0, 0, w->obj[1]);
                break;
            case 2:
                SndCall(6, 0xB, &w->obj[1]->pos, 0, 0, w->obj[1]);
                break;
            }
        }
    }
    w->flags &= ~2;
}

void cCtrl14::getBaseMtx(Mtx m, int idx)
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    cModel* o = w->obj[idx];

    if (o) {
        PSMTXCopy(o->mat, m);
    }
}

void cCtrl14::getPos(Vec* out)
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    cModel* o = w->obj[1];

    if (o) {
        *out = o->pos;
    }
}

f32 cCtrl14::getDir()
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    cModel* o = w->obj[1];

    if (o == NULL) {
        return 0.0f;
    }
    return o->rot.y;
}

f32 cCtrl14::getDir2()
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    f32 ret = 0.0f;

    if (w->obj[1] && w->obj[0]) {
        ret = Muku2(w->obj[0]->rot.y, w->obj[1]->rot.y, PI);
    } else {
        ret = 0.0f;
    }
    return ret;
}

void cCtrl14::addWidth(f32 x)
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    cModel* o = w->obj[0];
    Vec v;
    Vec p;

    if (o) {
        v.x = x;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVecSR(o->mat, &v, &v);
        PSVECAdd(&w->obj[0]->pos, &v, &p);
        switch (w->type) {
        case 0:
            if (p.x > -6300.0f) {
                p.x = -6300.0f;
            }
            if (p.x < -65600.0f) {
                p.x = -65600.0f;
            }
            break;
        case 1:
            if (p.x > -32000.0f) {
                p.x = -32000.0f;
            }
            if (p.x < -65600.0f) {
                p.x = -65600.0f;
            }
            break;
        case 2:
            return;
        }
        PSVECSubtract(&p, &w->obj[0]->pos, &v);
        v.y = 0.0f;
        if (w->obj[0]) {
            PSVECAdd(&w->obj[0]->pos, &v, &w->obj[0]->pos);
        }
        if (w->obj[1]) {
            PSVECAdd(&w->obj[1]->pos, &v, &w->obj[1]->pos);
        }
        if (w->obj[3]) {
            PSVECAdd(&w->obj[3]->pos, &v, &w->obj[3]->pos);
        }
        if (w->obj[4]) {
            PSVECAdd(&w->obj[4]->pos, &v, &w->obj[4]->pos);
        }
    }
}

void cCtrl14::addHeight(f32 y)
{
    Ctrl14Work* w = (Ctrl14Work*) work;

    if (w->type == 2) {
        if (w->obj[0]->pos.y > 30000.0f && y > 0.0f) {
            return;
        }
    } else {
        if (w->obj[0]->pos.y > 5000.0f && y > 0.0f) {
            return;
        }
    }
    if (w->obj[0]) {
        w->obj[0]->pos.y += y;
    }
    if (w->obj[1]) {
        w->obj[1]->pos.y += y;
    }
    if (w->obj[3]) {
        w->obj[3]->pos.y += y;
    }
    if (w->obj[4]) {
        w->obj[4]->pos.y += y;
    }
    y = fabsf(y);
    if (y > 1.0f) {
        w->flags |= 2;
    }
}

void cCtrl14::addDir(f32 d)
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    cModel* o = w->obj[1];

    if (o) {
        o->rot.y += d;
        if (w->obj[0]) {
            w->obj[1]->rot.y = w->obj[0]->rot.y + Muku2(w->obj[0]->rot.y, w->obj[1]->rot.y, PI / 4.0f);
            w->obj[1]->rot.y = LIMIT_ANGLE(w->obj[1]->rot.y);
        }
    }
}

void cCtrl14::setDir(f32 d)
{
    Ctrl14Work* w = (Ctrl14Work*) work;

    if (w->obj[1] && w->obj[0]) {
        w->obj[1]->rot.y = w->obj[0]->rot.y + d;
        w->obj[1]->rot.y = LIMIT_ANGLE(w->obj[1]->rot.y);
    }
}

void cCtrl14::resetDir()
{
    Ctrl14Work* w = (Ctrl14Work*) work;

    if (w->obj[1]) {
        w->obj[1]->rot.y += Muku2(w->obj[1]->rot.y, w->obj[0]->rot.y, 0.0061359233f);
        w->obj[1]->rot.y = LIMIT_ANGLE(w->obj[1]->rot.y);
    }
}

void cCtrl14::setFire()
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    cModel* o = w->obj[1];

    if (o) {
        if (w->fireTimer) {
            w->fireTimer = 0;
            EffectEspgenDelete(0, w->espKind, o);
            EstSet((int) w->obj[1], -1, NULL, NULL, 1, 4, 1, 0, (u32) w->obj[1], NULL);
        }
        w->fireDelay = 30;
        EstSet((int) w->obj[1], -1, NULL, NULL, 1, 3, 1, w->espKind, (u32) w->obj[1], NULL);
        SndCall(6, 1, &w->obj[1]->pos, 0, 0, w->obj[1]);
    }
}

int cCtrl14::ckHitFire(Vec* p)
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    cModel* o = w->obj[1];
    Mtx inv;
    Vec v;

    if (o == NULL) {
        return 0;
    }
    if (w->fireTimer == 0) {
        return 0;
    }
    PSMTXInverse(o->mat, inv);
    PSMTXMultVec(inv, p, &v);
    if (v.z < 5000.0f) {
        return 0;
    }
    if (v.z > 15000.0f) {
        return 0;
    }
    if (v.x < -1500.0f) {
        return 0;
    }
    if (v.x > 1500.0f) {
        return 0;
    }
    if (v.y < -5000.0f) {
        return 0;
    }
    if (v.y > 5000.0f) {
        return 0;
    }
    return 1;
}

int cCtrl14::ckHitFireBlocked()
{
    Ctrl14Work* w = (Ctrl14Work*) work;
    Vec a;
    Vec b;

    if (w->obj[1]) {
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 6000.0f;
        b.x = -2000.0f;
        b.y = 0.0f;
        b.z = 15000.0f;
        PSMTXMultVec(w->obj[1]->mat, &a, &a);
        PSMTXMultVec(w->obj[1]->mat, &b, &b);
        a.y = pPL->pos.y + 1600.0f;
        b.y = pPL->pos.y + 1600.0f;
        if (EatMgr.hitCheck(&a, &b, NULL, NULL, 0, 0x400000)) {
            return 0;
        }
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 6000.0f;
        b.x = 0.0f;
        b.y = 0.0f;
        b.z = 15000.0f;
        PSMTXMultVec(w->obj[1]->mat, &a, &a);
        PSMTXMultVec(w->obj[1]->mat, &b, &b);
        a.y = pPL->pos.y + 1600.0f;
        b.y = pPL->pos.y + 1600.0f;
        if (EatMgr.hitCheck(&a, &b, NULL, NULL, 0, 0x400000)) {
            return 0;
        }
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 6000.0f;
        b.x = 2000.0f;
        b.y = 0.0f;
        b.z = 15000.0f;
        PSMTXMultVec(w->obj[1]->mat, &a, &a);
        PSMTXMultVec(w->obj[1]->mat, &b, &b);
        a.y = pPL->pos.y + 1600.0f;
        b.y = pPL->pos.y + 1600.0f;
        return EatMgr.hitCheck(&a, &b, NULL, NULL, 0, 0x400000) == 0;
    }
    return 0;
}
