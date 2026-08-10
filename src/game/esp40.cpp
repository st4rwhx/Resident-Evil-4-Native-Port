#include "atari.h"
#include "light.h"
#include "esp.h"

struct Esp40Work {
    Vec pos;      // 0x00 position kept in parent space
    f32 ofsY;     // 0x0C height above the water surface
    f32 posY;     // 0x10 bobbing offset
    f32 spdY;     // 0x14
    f32 accY;     // 0x18
};

// Effect floating on the water surface of its parent.
class cEsp40 : public cEsp {
public:
    Esp40Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp40_Create()
{
    return new cEsp40;
}

void cEsp40::move()
{
    Vec wpos;
    Mtx inv;
    f32 h;

    if (parent != pEffParentWorld && (parentCnt == 0xff || parentCnt - 1 > cnt)) {
        pos = work.pos;
    }
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else if (parent != pEffParentWorld && (parentCnt == 0xff || parentCnt > cnt)) {
            Esp40Work* w = &work;
            w->pos = pos;
            if (spdCnt == 0 || spdCnt <= cnt) {
                w->posY += w->spdY;
                w->spdY += w->accY;
                w->spdY *= spdScale;
            }
            PSMTXMultVec(parent->mat, &pos, &wpos);
            if (GetWaterHeight(&wpos, &h)) {
                wpos.y = h + w->ofsY + w->posY;
                PSMTXInverse(parent->mat, inv);
                PSMTXMultVec(inv, &wpos, &pos);
            }
        }
    }
}

int cEsp40::SetFreeWork(EspGenWork* gen, u32* seed)
{
    f32 h;

    if (parent != pEffParentWorld && parentCnt == 0) {
        ApplyMatrix(parent->mat);
        parent = pEffParentWorld;
    }
    if (parent != pEffParentWorld && (parentCnt == 0xff || parentCnt <= cnt)) {
        Esp40Work* w = &work;
        w->ofsY = gen->x10;
        w->pos = pos;
        w->spdY = spd.y;
        w->accY = acc.y;
    } else if (GetWaterHeight(&pos, &h)) {
        pos.y = h + gen->x10;
    }
    return 1;
}
