#include "atari.h"
#include "light.h"
#include "esp.h"

struct Esp49Work {
    f32 depth;     // 0x00 depth below the water surface at which the effect dies
    f32 fadeDepth; // 0x04 depth over which the alpha fades
    f32 alpha;     // 0x08 base alpha
    u8 estOn;      // 0x0C spawn an est when the effect dies underwater
    u8 estNo;      // 0x0D
    u8 estPrm;     // 0x0E
};

// Effect that fades out and dies when it sinks below the water surface.
class cEsp49 : public cEsp {
public:
    Esp49Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp49_Create()
{
    return new cEsp49;
}

void cEsp49::move()
{
    Esp49Work* w = &work;
    Vec wpos;
    Vec r;
    Vec ep;
    f32 h;

    colA = w->alpha;
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
            return;
        }
        w->alpha = colA;
        if (parent != pEffParentWorldS && (parentCnt == 0xff || parentCnt <= cnt)) {
            PSMTXMultVec(parent->mat, &pos, &wpos);
        } else {
            wpos = pos;
        }
        if (GetWaterHeight(&pos, &h)) {
            f32 d = h - pos.y;
            if (d < w->depth) {
                if (w->estOn & 1) {
                    r.x = r.y = r.z = 0.0f;
                    ep = pos;
                    EstSet(0, -1, &ep, &r, w->estNo, w->estPrm, info.x0, info.x2, info.x8, 0);
                }
                PushEsp(this);
            } else if (d < w->fadeDepth) {
                colA *= (d - w->depth) / w->fadeDepth;
            }
        }
    }
}

int cEsp49::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp49Work* w = &work;

    w->depth = gen->xD8;
    w->fadeDepth = gen->xE0;
    if (w->depth > w->fadeDepth) {
        w->fadeDepth = w->depth;
    }
    w->estNo = gen->xC8;
    w->estPrm = gen->xC9;
    w->estOn = gen->xCA;
    if (w->estOn > 1) {
        pLog->err(0, 0, "ESP_49 : FLAG[%d] invalid.", w->estOn);
        return 0;
    }
    w->alpha = colA;
    return 1;
}
