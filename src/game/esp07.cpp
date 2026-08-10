#include "atari.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"

struct Esp07Work {
    Vec bounce;    // 0x00 x: horizontal damping, y: vertical damping (gen->xD8.. * 0.1)
    u8 estNo;      // 0x0C est on floor hit (gen->xC8)
    u8 estPrm;     // 0x0D (gen->xC9)
    u8 estNo2;     // 0x0E est on wall hit (gen->xCA)
    u8 estPrm2;    // 0x0F (gen->xCB)
    u32 hitType;   // 0x10 0: floor (cached), 1: floor, 2: wall (gen->xFC)
    u32 estCall;   // 0x14 0: bounce, 1: est + die, 2: est + bounce, 3: die (gen->xFD)
    u32 flags;     // 0x18 bit0: stopped, bit1: floor height cached
    f32 floorY;    // 0x1C
    u8 seType;     // 0x20 (gen->xFE)
};

// Bouncing particle: checks the floor (or walls) every frame, bounces / spawns an est / dies.
class cEsp07 : public cEsp {
public:
    Esp07Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" {
void Esp07_ChkGnd(cEsp07* esp, f32 floorY);
void Esp07_HitGndLight(cEsp07* esp);
void Esp07_HitGnd(cEsp07* esp);
void Esp07_HitWall(cEsp07* esp);
}

cEsp* Esp07_Create()
{
    return new cEsp07;
}

void Esp07_ChkGnd(cEsp07* esp, f32 floorY)
{
    Esp07Work* w = &esp->work;
    f32 half;

    half = esp->sizeY * 0.5f * esp->scale;
    if ((pG->flags_64 & 0x00800000) && !(pG->flags_60 & 0x00010000)) {
        floorY = 0.0f;
    }
    if (esp->pos.y - half < floorY) {
        esp->pos.y = floorY + half;
        if (w->seType != 0) {
            Vec d;
            f32 dist;

            PSVECSubtract(&pG->Cam.param.pos, &esp->pos, &d);
            dist = PSVECMag(&d);
            if (w->seType == 3 || dist < 8000.0f) {
                EspCallSeType(w->seType, &esp->pos);
            }
        }
        if (w->estCall == 1 || w->estCall == 2) {
            Vec rot;
            Vec p;

            if (esp->spd.x == 0.0f && esp->spd.z == 0.0f) {
                rot.x = rot.y = rot.z = 0.0f;
            } else {
                rot.x = rot.z = 0.0f;
                rot.y = atan2f(esp->spd.x, esp->spd.z);
            }
            p = esp->pos;
            p.y = floorY + 65.0f;
            EstSet(0, -1, &p, &rot, w->estNo, w->estPrm, esp->info.x0, esp->info.x2, esp->info.x8, NULL);
            if (w->estCall != 2) {
                PushEsp(esp);
                return;
            }
        }
        if (w->estCall == 3) {
            PushEsp(esp);
            return;
        }
        esp->spd.x = esp->spd.x * w->bounce.x;
        esp->spd.y = esp->spd.y * -w->bounce.y;
        esp->spd.z = esp->spd.z * w->bounce.x;
        if (PSVECMag(&esp->spd) < 10.0f) {
            w->flags = 1;
            PSVECScale(&esp->spd, &esp->spd, 0.0f);
            PSVECScale(&esp->acc, &esp->acc, 0.0f);
        }
    }
}

void Esp07_HitGndLight(cEsp07* esp)
{
    Esp07Work* w = &esp->work;
    u32 attr;

    if (!(w->flags & 2)) {
        w->flags |= 2;
        w->floorY = SatMgr.getFloor(&esp->pos, 600.0f, 100000.0f, &attr, 0);
    }
    Esp07_ChkGnd(esp, w->floorY);
}

void Esp07_HitGnd(cEsp07* esp)
{
    u32 attr;

    Esp07_ChkGnd(esp, SatMgr.getFloor(&esp->pos, 600.0f, 100000.0f, &attr, 0));
}

void Esp07_HitWall(cEsp07* esp)
{
    Esp07Work* w = &esp->work;
    Vec refl;
    Vec hit;
    Vec n2;
    Vec next;
    Vec nrm;
    Vec rot;
    f32 mag;

    PSVECAdd(&esp->pos, &esp->spd, &next);
    if (SatMgr.hitCheck(&esp->pos, &next, &hit, &nrm, 0, 0)) {
        esp->pos = hit;
        PSVECAdd(&nrm, &esp->pos, &esp->pos);
        if (w->seType != 0) {
            EspCallSeType(w->seType, &esp->pos);
        }
        if (w->estCall == 1 || w->estCall == 2) {
            if (esp->spd.x == 0.0f && esp->spd.z == 0.0f) {
                rot.x = rot.y = rot.z = 0.0f;
            } else {
                rot.x = rot.z = 0.0f;
                rot.y = atan2f(esp->spd.x, esp->spd.z);
            }
            if (nrm.y > 0.98f) {
                EstSet(0, -1, &esp->pos, &rot, w->estNo, w->estPrm, esp->info.x0, esp->info.x2, esp->info.x8, NULL);
            } else {
                EstSet(0, -1, &esp->pos, &rot, w->estNo2, w->estPrm2, esp->info.x0, esp->info.x2, esp->info.x8, NULL);
            }
            if (w->estCall != 2) {
                PushEsp(esp);
                return;
            }
        }
        if (w->estCall == 3) {
            PushEsp(esp);
            return;
        }
        mag = RootSumSquare3(&esp->spd);
        n2.x = -nrm.x;
        n2.y = -nrm.y;
        n2.z = -nrm.z;
        C_VECReflect(&esp->spd, &n2, &refl);
        PSVECScale(&refl, &esp->spd, mag * w->bounce.y);
        PSVECScale(&esp->rotSpd, &esp->rotSpd, -0.8f);
    }
}

void cEsp07::move()
{
    Esp07Work* w = &work;

    if (CommonMove()) {
        if (!(w->flags & 1)) {
            switch (w->hitType) {
            case 0:
                Esp07_HitGndLight(this);
                break;
            case 1:
                Esp07_HitGnd(this);
                break;
            case 2:
                Esp07_HitWall(this);
                break;
            default:
                pLog->err(0, 0, "ESP_07 : HitType[%x] is invalid.", w->hitType);
                PushEsp(this);
                return;
            }
        }
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

int cEsp07::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp07Work* w = &work;

    w->bounce = *(Vec*)&gen->xD8;
    PSVECScale(&w->bounce, &w->bounce, 0.1f);
    w->estNo = gen->xC8;
    w->estPrm = gen->xC9;
    w->estNo2 = gen->xCA;
    w->estPrm2 = gen->xCB;
    w->hitType = gen->xFC;
    w->estCall = gen->xFD;
    w->seType = gen->xFE;
    if (w->hitType > 2) {
        pLog->err(0, 0, "ESP_07 : HitType[%x] is invalid.", w->hitType);
        return 0;
    }
    if (w->estCall > 3) {
        pLog->err(0, 0, "ESP_07 : EstCall[%x] is invalid.", w->estCall);
        return 0;
    }
    if (w->seType > 3) {
        pLog->err(0, 0, "ESP_07 : SeType[%d] is invalid.", w->seType);
        return 0;
    }
    if (life == 0) {
        life = 0x80;
    }
    return 1;
}
