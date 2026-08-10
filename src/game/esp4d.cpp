#include "atari.h"
#include "light.h"
#include "math_sub.h"
#include "esp.h"

extern "C" void AddWaterPower(Vec* pos, f32 power);

// Water ripple: pushes the water surface in rings around the effect position.
class cEsp4d : public cEsp {
public:
    u8 type;  // 0xF8 0: push down, 1: pull up

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp4d_Create()
{
    return new cEsp4d;
}

void cEsp4d::move()
{
    Vec p;
    f32 sign;
    f32 fade;
    f32 r;
    f32 ang;
    u32 nRing;
    u32 i;
    u32 j;
    u32 n;

    if (CommonMove()) {
        fade = 1.0f;
        sign = (type == 0) ? 1.0f : -1.0f;
        if (flags & 0x20000) {
            sign *= 4.0f;
        }
        nRing = 5;
        for (i = 0; i < nRing; i++) {
            n = i + 3;
            r = sizeX * scale * ((f32)(i + 1) * 0.25f);
            ang = 0.0f;
            for (j = 0; j < n; j++) {
                p = pos;
                p.x = SINF(ang) * r + p.x;
                p.z = COSF(ang) * r + p.z;
                AddWaterPower(&p, colA * 0.001f * fade * sign);
                ang += 6.2831855f / (f32)n;
            }
            fade -= 1.0f / (f32)nRing;
        }
    }
}

void Esp4d_Trans()
{
}

int cEsp4d::SetFreeWork(EspGenWork* gen, u32* seed)
{
    type = gen->xC8;
    if (type > 1) {
        pLog->err(0, 0, "ESP4D : Type[%d] is invalid.", type);
        type = 0;
    }
    return 1;
}
