#include "atari.h"
#include "light.h"
#include "esp.h"

// Est generator 45 parameter block filled from this effect (game/espgen45.cpp).
struct Esp4cWork {
    u8 type;      // 0x00
    u8 x1;        // 0x01
    u8 x2;        // 0x02
    u8 x3;        // 0x03
    u16 x4;       // 0x04
    u16 x6;       // 0x06
    f32 spread;   // 0x08
    f32 damp;     // 0x0C
    Vec dir;      // 0x10
    u8 flag;      // 0x1C
    u8 x1D;       // 0x1D
};

extern "C" {
void Estgen45SetTargetCamera(int on);
void Estgen45SetTargetPos(f32 x, f32 z);
void Estgen45SetTargetHeight(int on);
void Estgen45SetHeight(f32 h);
void Estgen45SetSizeOverWrite(int on);
void Estgen45SetSize(f32 size);
void Estgen45SetColorMul(int on);
void Estgen45SetColor(u8 r, u8 g, u8 b, u8 a, f32 rs, f32 gs, f32 bs, f32 as);
void Estgen45SetColorOverWrite(int on);
void Estgen45SetParamOverWrite(int on);
void Estgen45SetParam(Esp4cWork* w);
}

// Weather (est generator 45) controller: pushes its color/size into the generator every frame.
class cEsp4c : public cEsp {
public:
    Esp4cWork work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
    virtual void Destruct();
};

cEsp* Esp4c_Create()
{
    return new cEsp4c;
}

void cEsp4c::move()
{
    Esp4cWork* w = &work;
    f32 r = colR;
    f32 g = colG;
    f32 b = colB;
    f32 a = colA;

    if (CommonMove()) {
        colR = r;
        colG = g;
        colB = b;
        colA = a;
        Estgen45SetTargetCamera(0);
        Estgen45SetTargetPos(pos.x, pos.z);
        if (flags & 2) {
            Estgen45SetTargetHeight(1);
            Estgen45SetHeight(pos.y);
        } else {
            Estgen45SetTargetHeight(0);
        }
        Estgen45SetSizeOverWrite(1);
        Estgen45SetSize(sizeX * scale);
        Estgen45SetColorMul(1);
        Estgen45SetColor((u8)colR, (u8)colG, (u8)colB, (u8)colA, colRSpd, colGSpd, colBSpd, colASpd);
        if (flags & 1) {
            Estgen45SetColorOverWrite(1);
            Estgen45SetParamOverWrite(1);
            Estgen45SetParam(w);
        } else {
            Estgen45SetColorOverWrite(0);
            Estgen45SetParamOverWrite(0);
        }
    }
}

void cEsp4c::Destruct()
{
}

void Esp4c_Trans()
{
}

int cEsp4c::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp4cWork* w = &work;

    w->x3 = gen->xFE;
    w->type = gen->xC8;
    if (w->type == 2) {
        w->spread = 0.5f - (f32)(s8)gen->xC9 * 0.005f;
        if (w->spread > 0.5f) {
            w->spread = 0.5f;
        }
        if (w->spread < 0.0f) {
            w->spread = 0.0f;
        }
        w->damp = 0.99f - gen->xCA * 0.001f;
    }
    w->x2 = gen->x2;
    w->x4 = gen->prm.h.xCE;
    w->x6 = gen->prm.h.xD2;
    w->x1 = gen->xCB;
    w->dir = gen->x58;
    PSVECScale(&w->dir, &w->dir, 3.14 / 180);
    if (gen->flags & 0x4000) {
        w->flag |= 2;
        w->x1D = gen->xC5;
        w->flag |= 1;
    }
    move();
    return 1;
}
