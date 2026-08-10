#include "atari.h"
#include "math_sub.h"
#include "esp.h"

struct Esp0dWork {
    u32 type;        // 0x00
    f32 dist;        // 0x04
    f32 power;       // 0x08
    cCoord* target;  // 0x0C
};

// Attractor effect: pulls the sprite toward its parent while inside `dist`.
class cEsp0d : public cEsp {
public:
    Esp0dWork work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp0d_Create()
{
    return new cEsp0d;
}

void cEsp0d::move()
{
    Esp0dWork* w = &work;
    Vec v;
    f32 len;

    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else if (w->type == 0) {
            PSVECSubtract(&pos, &w->target->worldPos, &v);
            len = PSVECMag(&v);
            if (len == 0.0f) {
                v.x = 0.0f;
                v.y = 1.0f;
                v.z = 0.0f;
            } else {
#line 77 "D:/Bio4/Prog/esp0d.cpp"
                VECNormalize(&v, &v);
            }
            if (len < w->dist) {
                PSVECScale(&v, &v, (w->dist - len) * w->power);
                if (flags & 1) {
                    v.y = 0.0f;
                }
                PSVECAdd(&spd, &v, &spd);
            }
        }
    }
}

int cEsp0d::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp0dWork* w = &work;

    w->dist = (f32)(s8)gen->xC8 * 100.0f;
    w->power = (f32)(s8)gen->xC9 * 0.00005f;
    w->type = gen->xFC;
    if (flags & 0x20) {
        w->target = pModel->getPartsPtr(partsNo);
    } else {
        w->target = parent;
    }
    if (w->type != 0) {
        pLog->err(0, 0, "ESP0D : Type[%x] invalid.", w->type);
        return 0;
    }
    return 1;
}
