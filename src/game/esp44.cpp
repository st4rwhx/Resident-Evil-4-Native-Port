#include "atari.h"
#include "light.h"
#include "esp.h"

// Room SE trigger effect: plays the generator's sound once, draws nothing.
class cEsp44 : public cEsp {
public:
    u32 xF8;

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp44_Create()
{
    return new cEsp44;
}

void cEsp44::move()
{
}

int cEsp44::SetFreeWork(EspGenWork* gen, u32* seed)
{
    if (EspGenGetMoveLoop() == 0) {
        EffCallRoomSeFunc((s8)gen->xC8, &pos);
    }
    return 0;
}
