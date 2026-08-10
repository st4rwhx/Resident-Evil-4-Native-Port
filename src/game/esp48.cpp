#include "atari.h"
#include "rnd.h"
#include "math_sub.h"
#include "esp.h"

struct Esp48Work {
    f32 ampX;    // 0x00
    f32 freqX;   // 0x04
    f32 ampY;    // 0x08
    f32 freqY;   // 0x0C
    f32 ampZ;    // 0x10
    f32 freqZ;   // 0x14
    f32 time;    // 0x18
    Vec swing;   // 0x1C rotation added this frame
};

// Effect whose rotation swings on a sine wave around the base rotation.
class cEsp48 : public cEsp {
public:
    Esp48Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp48_Create()
{
    return new cEsp48;
}

void cEsp48::move()
{
    Esp48Work* w = &work;

    PSVECSubtract(&rot, &w->swing, &rot);
    if (CommonMove()) {
        w->time += 0.01f;
        w->swing.x = w->ampX * sinf(w->time * w->freqX);
        w->swing.y = w->ampY * sinf(w->time * w->freqY);
        w->swing.z = w->ampZ * sinf(w->time * w->freqZ);
        PSVECAdd(&rot, &w->swing, &rot);
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

int cEsp48::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp48Work* w = &work;

    w->ampX = gen->xD8 * 0.1f;
    w->freqX = gen->xDC;
    w->ampY = gen->xE4 * 0.1f;
    w->freqY = gen->xE8;
    w->ampZ = gen->xF0 * 0.1f;
    w->freqZ = gen->xF4;
    w->time = fRandSeed1_1(seed) * 2.0f * PI;
    return 1;
}
