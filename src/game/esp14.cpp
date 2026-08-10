#include "atari.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"

struct Esp14Work {
    f32 lenRate;  // 0x00 sizeY per unit of camera distance
    f32 sizeY;    // 0x04 base sizeY
    Vec v0;       // 0x08 clip box half extents (x, z)
    Vec v1;       // 0x14 clip box center (x, z)
};

// Light shaft sprite: a vertical beam whose length depends on the camera view and is clipped
// to a box around the effect.
class cEsp14 : public cEsp {
public:
    Esp14Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp14_Create()
{
    return new cEsp14;
}

void cEsp14::move()
{
    Esp14Work* w = &work;
    Vec d;
    Vec cross;
    Vec camDir;
    f32 len;
    f32 rate;
    f32 sy, cy;
    f32 min;
    f32 t, t2;
    f32 nx, nz;
    f32 lim0, lim1, lim2, lim3;

    if (!AnmMove()) {
        PushEsp(this);
    } else {
        PSVECSubtract(&pG->Cam.param.at, &pG->Cam.param.pos, &camDir);
        PSVECSubtract(&pos, &pG->Cam.param.pos, &d);
        PSVECCrossProduct(&d, &pG->Cam.up, &cross);
        rot.x = PI / 2.0f;
        rot.y = atan2f(cross.z, -cross.x);
        len = SQRTF(d.x * d.x + d.z * d.z);
        rate = d.y / PSVECMag(&d);
        if (rate < 0.0f) {
            rate = -rate;
        }
        rate = 1.0f - rate;
        rate = rate * rate;
        rate = rate * rate;
        rate = rate * rate;
        sizeY = len * w->lenRate * rate + w->sizeY;
        if (sizeY < w->sizeY) {
            sizeY = sizeX;
        }
        if (w->v0.x != 0.0f || w->v0.z != 0.0f) {
            min = 1.0f;
            nx = -w->v1.x;
            nz = -w->v1.z;
            lim0 = nx + w->v0.x;
            lim1 = nx - w->v0.x;
            lim3 = nz + w->v0.z;
            lim2 = nz - w->v0.z;
            sy = sinf(rot.y) * sizeY;
            cy = cosf(rot.y) * sizeY;
            if (sy > lim0) {
                t = lim0 / sy;
                if (t < min) {
                    min = t;
                }
            }
            if (sy < lim1) {
                t = lim1 / sy;
                if (t < min) {
                    min = t;
                }
            }
            if (cy > lim3) {
                t2 = lim3 / cy;
                if (t2 < min) {
                    min = t2;
                }
            }
            if (cy < lim2) {
                t2 = lim2 / cy;
                if (t2 < min) {
                    min = t2;
                }
            }
            if (min != 1.0f) {
                cy = cy * min;
                sy = sy * min;
                sizeY = SQRTF(sy * sy + cy * cy);
            }
        }
        xB8 = 100000000.0f;
        dispFlag |= 2;
    }
}

int cEsp14::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp14Work* w = &work;

    w->lenRate = (f32)(s8)gen->xC8 * 0.05f + 0.25f;
    w->sizeY = sizeY;
    w->v0 = *(Vec*)&gen->xD8;
    w->v1 = *(Vec*)&gen->xE4;
    if (fabsf(gen->xD8) < fabsf(gen->xE4) || fabsf(gen->xE0) < fabsf(gen->xEC) ||
        fabsf(gen->xDC) != 0.0f || fabsf(gen->xE8) != 0.0f) {
        pLog->err(0, 0, "ESP14 : Vec0 or Vec1 Invalid Paramater.");
        return 0;
    }
    flags |= 1;
    return 1;
}
