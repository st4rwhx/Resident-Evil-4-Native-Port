#include "atari.h"
#include "esp.h"

class cEsp13 : public cEsp {
public:
    u32 xF8;

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp13_Create()
{
    return new cEsp13;
}

void cEsp13::move()
{
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

int cEsp13::SetFreeWork(EspGenWork* gen, u32* seed)
{
    pLog->err(0, 0, "ESP : Invalid ID 'ESP13' ");
    return 0;
}
