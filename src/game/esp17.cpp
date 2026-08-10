#include "global.h"
#include "esp.h"

// Screen-space effect: keeps its own copy of the position and converts it into view space.
class cEsp17 : public cEsp {
public:
    Vec work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp17_Create()
{
    return new cEsp17;
}

void cEsp17::move()
{
    Mtx inv;

    pos = work;
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else {
            work = pos;
            FSet(pos.z, -pos.z);
            PSMTXInverse(pG->Cam.viewMat, inv);
            PSMTXMultVec(inv, &pos, &pos);
        }
    }
}

int cEsp17::SetFreeWork(EspGenWork* gen, u32* seed)
{
    work = pos;
    return 1;
}
