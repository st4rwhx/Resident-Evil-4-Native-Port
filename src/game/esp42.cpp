#include "atari.h"
#include "esp.h"

// Effect that selects its blend mode from two lookup tables.
class cEsp42 : public cEsp {
public:
    u32 xF8;

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp42_Create()
{
    return new cEsp42;
}

void cEsp42::move()
{
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

int cEsp42::SetFreeWork(EspGenWork* gen, u32* seed)
{
    static u32 bl1[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    static u32 bl2[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };

    if ((s8)gen->xC8 > 8) {
        pLog->err(0, 0, "ESP42 : WK0[%d] invalid", (s8)gen->xC8);
        return 0;
    }
    if ((s8)gen->xC9 > 8) {
        pLog->err(0, 0, "ESP42 : WK1[%d] invalid", (s8)gen->xC9);
        return 0;
    }
    xA4 = 1;
    xA5 = bl1[(s8)gen->xC8];
    xA6 = bl2[(s8)gen->xC9];
    xA7 = 0;
    return 1;
}
