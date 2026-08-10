#include "light.h"
#include "atari.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"

struct Esp05Work {
    f32 amp;     // 0x00 wobble amplitude
    f32 angSpd;  // 0x04
    f32 ang;     // 0x08
};

// Fluttering sprite (falling leaf / feather): wobbles the position with sin/cos of a random angle.
class cEsp05 : public cEsp {
public:
    Esp05Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp05_Create()
{
    return new cEsp05;
}

void cEsp05::move()
{
    Esp05Work* w = &work;

    if (CommonMove()) {
        pos.x += w->amp * sinf(w->ang);
        pos.z += w->amp * cosf(w->ang * 1.7f);
        pos.y += w->amp * 0.3f * cosf(w->ang * 1.9f);
        w->ang += w->angSpd * (fRand0_1() + 0.2f);
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

int cEsp05::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp05Work* w = &work;

    w->amp = gen->xD8;
    w->angSpd = gen->xDC * 0.05f;
    w->amp += w->amp * fRand0_1() * (gen->xE0 * 0.1f);
    w->angSpd += w->angSpd * fRand0_1() * (gen->xE0 * 0.1f);
    w->ang = fRand0_1() * PI * 2.0f;
    return 1;
}
