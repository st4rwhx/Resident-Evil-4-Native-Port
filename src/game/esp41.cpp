#include "atari.h"
#include "global.h"
#include "player.h"
#include "em.h"
#include "math_sub.h"
#include "esp.h"

struct Esp41Work {
    f32 range;   // 0x00 attraction range
    f32 power;   // 0x04
    Vec ofs;     // 0x08 offset from the target position
    u8 type;     // 0x14
};

// Effect attracted towards the player (or enemy 0 in a cutscene).
class cEsp41 : public cEsp {
public:
    Esp41Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp41_Create()
{
    return new cEsp41;
}

void cEsp41::move()
{
    Esp41Work* w = &work;
    Vec d;
    Vec tgt;
    f32 dist;

    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else if (w->type == 0) {
            cModel* target = pPL;
            if (pG->flags_64 & 0x00800000) {
                target = EmMgrWork(0);
                if (!(target->be_flag & 1)) {
                    return;
                }
            }
            PSVECAdd(&target->pos, &w->ofs, &tgt);
            PSVECSubtract(&pos, &tgt, &d);
            dist = PSVECMag(&d);
            if (d.x == 0.0f && d.y == 0.0f && d.z == 0.0f) {
                d.y = 1.0f;
            }
#line 90 "D:/Bio4/Prog/esp41.cpp"
            VECNormalize(&d, &d);
            if (dist < w->range) {
                PSVECScale(&d, &d, (w->range - dist) * w->power);
                if (flags & 1) {
                    d.y = 0.0f;
                }
                PSVECAdd(&spd, &d, &spd);
            }
        }
    }
}

int cEsp41::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp41Work* w = &work;

    w->range = (f32)(s8)gen->xC8 * 100.0f;
    w->power = (f32)(s8)gen->xC9 * 0.00005f;
    w->type = gen->xFC;
    w->ofs = *(Vec*)&gen->xD8;
    if (w->type != 0) {
        pLog->err(0, 0, "ESP41 : Type[%x] invalid.", w->type);
        return 0;
    }
    return 1;
}
