#include "atari.h"
#include "filter.h"
#include "esp.h"

struct Esp46Work {
    int type;   // 0x00 filter03 type
    u8 x4;      // 0x04
    u8 x5;      // 0x05
};

// Screen filter (filter03) driver: never drawn itself, feeds its color into the filter.
class cEsp46 : public cEsp {
public:
    Esp46Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp46_Create()
{
    return new cEsp46;
}

void cEsp46::move()
{
    scale = 1e22f;
    CommonMove();
}

void Esp46_Trans(cEsp46* e)
{
    Esp46Work* w = &e->work;
    f32 a = e->colA * (1.0f / 255.0f);

    Filter03SetParam(w->type, (u8)(e->colR * a), (u8)(e->colG * a), (u8)(e->colB * a), w->x4, w->x5);
}

int cEsp46::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp46Work* w = &work;

    w->type = (s8)gen->xC8;
    w->x4 = gen->xC9;
    if ((s8)gen->xCA <= 1) {
        w->x5 = gen->xCA;
    } else {
        pLog->err(0, 0, "ESP46 : WK2[%x] invalid.", (s8)gen->xCA);
    }
    partsNo = 0xf8;
    return 1;
}
