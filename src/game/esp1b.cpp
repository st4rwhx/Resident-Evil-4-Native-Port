#include "atari.h"
#include "esp.h"

extern "C" void* memcpy(void* dst, const void* src, unsigned int n);

struct Esp1bWork {
    int n;   // 0x00 number of points
    Vec v0;  // 0x04
    Vec v1;  // 0x10
    Vec v2;  // 0x1C
};

static f32 esp1b_scale = 0.005f;

// Spline sprite (drawn by Esp1b_SpTrans in esp_sub.cpp).
class cEsp1b : public cEsp {
public:
    Esp1bWork work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp1b_Create()
{
    return new cEsp1b;
}

void cEsp1b::move()
{
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

int cEsp1b::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp1bWork* w = &work;
    int n;

    n = (s8)gen->xC8 + 4;
    if (n <= 1) {
        pLog->err(0, 0, "ESP1B : Wk0[%d] Invalid.", (s8)gen->xC8);
        n = 2;
    }
    if (n > 0x40) {
        pLog->err(0, 0, "ESP1B : Wk0[%d] Invalid.", (s8)gen->xC8);
        n = 0x40;
    }
    dispFlag |= 0x10;
    w->n = n;
    memcpy((u8*)w + 4, &gen->xD8, sizeof(Vec));
    memcpy((u8*)w + 0x10, &gen->xE4, sizeof(Vec));
    memcpy((u8*)w + 0x1C, &gen->xF0, sizeof(Vec));
    PSVECScale(&w->v0, &w->v0, esp1b_scale);
    PSVECScale(&w->v1, &w->v1, esp1b_scale);
    PSVECScale(&w->v2, &w->v2, esp1b_scale);
    return 1;
}

// The split object's .sdata is padded to 8 bytes (the following unit is 8-aligned).
asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
