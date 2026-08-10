#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"

// Floor decal: dropped onto the ground (or the water surface) when created.
class cEsp10 : public cEsp {
public:
    u32 xF8;

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" int EffAreaCheckInRoom(Vec* pos);

cEsp* Esp10_Create()
{
    return new cEsp10;
}

void cEsp10::move()
{
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

f32 getFloor_attr(Vec* pos, u32* attr, int x, f32 up, f32 down)
{
    Vec top;
    Vec bottom;
    Vec hit;
    int r;

    if (pG->flags_64 & 0x10000000) {
        return 0.0f;
    }
    top.x = pos->x;
    top.y = pos->y + up;
    top.z = pos->z;
    bottom.x = pos->x;
    bottom.y = pos->y - down;
    bottom.z = pos->z;
    r = SatMgr.hitCheck2(&top, &bottom, &hit, attr, 0x40, x);
    if (r != 0 && !(r & 4)) {
        return hit.y;
    }
    return -100000.0f;
}

int cEsp10::SetFreeWork(EspGenWork* gen, u32* seed)
{
    u32 attr;
    f32 h;

    if (parent != pEffParentWorld) {
        ApplyMatrix(parent->mat);
        parent = pEffParentWorld;
    }
    FSet(pos.y, getFloor_attr(&pos, &attr, 0, 600.0f, 100000.0f) + 65.0f + gen->xDC);
    if ((pG->flags_64 & 0x00800000) && !(pG->flags_60 & 0x00010000)) {
        pos.y = 0.0f;
    }
    switch ((s8)gen->xCB) {
    case 0:
        break;
    case 1:
        if (EffAreaCheckInRoom(&pos) == 1) {
            PushEsp(this);
        }
        break;
    case 2:
        if (GetWaterHeight(&pos, &h)) {
            if (pos.y < h + gen->xDC) {
                pos.y = h + gen->xDC;
            }
        }
        break;
    default:
        pLog->err(0, 0, "ESP10 : Type[%d] Invalid Trans.", (s8)gen->xCB);
        return 0;
    }
    return 1;
}
