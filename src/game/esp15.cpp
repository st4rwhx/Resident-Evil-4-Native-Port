#include "atari.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"

struct Esp15Work {
    f32 range;     // 0x00 half size of the box around the camera
    f32 fadeRate;  // 0x04 1 - (distance ratio where the alpha starts fading)
    f32 alpha;     // 0x08 base alpha
    f32 floorY;    // 0x0C the sprite may not fall below this height (0 = none)
    u8 cntMax;     // 0x10 fade in frames
    u8 cnt;        // 0x11
};

// Camera-relative particle (rain / snow / dust): the position is wrapped so that it always
// stays inside a box around the camera.
class cEsp15 : public cEsp {
public:
    Esp15Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp15_Create()
{
    return new cEsp15;
}

void cEsp15::move()
{
    Esp15Work* w = &work;
    Vec tmp;
    Vec dir;
    Vec sc;
    Vec sc2;
    f32 range;
    f32 half;
    f32 d;
    f32 oldY;
    int n;
    int flag1;
    int flag2;

    colA = w->alpha;
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else {
            flag1 = 1;
            flag2 = 1;
            oldY = pos.y;
            if (w->floorY != 0.0f && pos.y < w->floorY) {
                flag1 = 0;
            }
            w->alpha = colA;
            if (w->cntMax != 0) {
                if (pG->flags_5010 & 0x02000000) {
                    w->cnt++;
                } else if (w->cnt != 0) {
                    w->cnt--;
                }
                if (w->cnt != 0) {
                    if (w->cnt >= w->cntMax) {
                        w->cnt = w->cntMax;
                    }
                    colA = colA * (1.0f - (f32)w->cnt / w->cntMax);
                }
            }
            range = w->range;
            half = range * 0.6f;

            PSVECSubtract(&pG->Cam.param.at, &pG->Cam.param.pos, &dir);
            PSVECCrossProduct(&dir, &pG->Cam.up, &dir);
#line 111 "D:/Bio4/Prog/esp15.cpp"
            VECNormalize(&dir, &dir);
            PSVECSubtract(&pos, &pG->Cam.param.pos, &tmp);
            d = PSVECDotProduct(&tmp, &dir);
            if (d >= 0.0f) {
                n = (int)((d + half) / (half * 2.0f));
            } else {
                n = (int)((d - half) / (half * 2.0f));
            }
            if (n != 0) {
                PSVECScale(&dir, &sc, -(half * 2.0f * (f32)n));
                PSVECAdd(&pos, &sc, &pos);
            }

#line 130 "D:/Bio4/Prog/esp15.cpp"
            VECNormalize(&pG->Cam.up, &dir);
            PSVECSubtract(&pos, &pG->Cam.param.pos, &tmp);
            d = PSVECDotProduct(&tmp, &dir);
            if (d >= 0.0f) {
                n = (int)((d + half) / (half * 2.0f));
            } else {
                n = (int)((d - half) / (half * 2.0f));
            }
            if (n != 0) {
                PSVECScale(&dir, &sc2, -(half * 2.0f * (f32)n));
                PSVECAdd(&pos, &sc2, &pos);
            }

            PSVECSubtract(&pG->Cam.param.at, &pG->Cam.param.pos, &dir);
#line 152 "D:/Bio4/Prog/esp15.cpp"
            VECNormalize(&dir, &dir);
            PSVECSubtract(&pos, &pG->Cam.param.pos, &tmp);
            d = PSVECDotProduct(&tmp, &dir);
            if (d >= 0.0f) {
                n = (int)(d / range);
            } else {
                n = -(int)(-d / range) - 1;
            }
            if (n != 0) {
                PSVECScale(&dir, &sc, -(range * (f32)n));
                PSVECAdd(&pos, &sc, &pos);
                PSVECSubtract(&pos, &pG->Cam.param.pos, &tmp);
                d = PSVECDotProduct(&tmp, &dir);
            }
            if (d > range * w->fadeRate) {
                colA = colA * (1.0f - (d - range * w->fadeRate) / (range * (1.0f - w->fadeRate)));
            }
            if (w->floorY != 0.0f) {
                if (pos.y < w->floorY) {
                    flag2 = 0;
                }
                if (flag1 == 1 && flag2 == 0) {
                    pos.y = oldY;
                }
            }
        }
    }
}

int cEsp15::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp15Work* w = &work;

    x16 = (s8)gen->xC8 * 10;
    x14 = (s8)gen->xC9 * 10;
    w->fadeRate = (f32)(s8)gen->xCA / 100.0f;
    if (w->fadeRate > 1.0f) {
        w->fadeRate = 1.0f;
    }
    w->fadeRate = 1.0f - w->fadeRate;
    w->cntMax = gen->xCB;
    w->range = gen->x20;
    pos.x += w->range * fRandSeed1_1(seed);
    pos.y += w->range * fRandSeed1_1(seed);
    pos.z += w->range * fRandSeed1_1(seed);
    w->alpha = colA;
    w->floorY = gen->xD8;
    if (pGS->flags_5010 & 0x02000000) {
        colA = 0.0f;
        w->cnt = w->cntMax;
    }
    return 1;
}
