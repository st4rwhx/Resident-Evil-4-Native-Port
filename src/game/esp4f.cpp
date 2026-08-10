#include "atari.h"
#include "esp.h"

struct Esp4fWork {
    u8 areaNo;  // 0x00
};

// Effect that is only drawn while inside a given room area.
class cEsp4f : public cEsp {
public:
    Esp4fWork work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp4f_Create()
{
    return new cEsp4f;
}

void cEsp4f::move()
{
    Esp4fWork* w = &work;
    Vec wpos;

    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else {
            if (parent == pEffParentWorld) {
                wpos = pos;
            } else {
                PSMTXMultVec(parent->mat, &pos, &wpos);
            }
            if (EffAreaCheckNo(&wpos, w->areaNo)) {
                PushEsp(this);
            }
        }
    }
}

int cEsp4f::SetFreeWork(EspGenWork* gen, u32* seed)
{
    work.areaNo = gen->xC8;
    return 1;
}
