#include "atari.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"

struct Esp19Work {
    Vec target;  // 0x00 end point of the line
    f32 len;     // 0x0C maximum length
};

// 3D line effect (laser sight / tracer): draws a line from the effect toward a target point,
// fading the far end.
class cEsp19 : public cEsp {
public:
    Esp19Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp19_Create()
{
    return new cEsp19;
}

void cEsp19::move()
{
    if (CommonMove()) {
        xB8 = 100000000.0f;
        dispFlag |= 2;
    }
}

int cEsp19::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp19Work* w = &work;

    w->target = *(Vec*)&gen->xD8;
    if (gen->xE4 == 0.0f) {
        w->len = 12000.0f;
    } else {
        w->len = gen->xE4;
    }
    return 1;
}

static void Draw_line3d_local_222(Vec* p0, Vec* p1, Mtx mtx, u32 color, cEsp* esp, f32 len)
{
    Vec end;
    Vec dir;
    f32 rate = 1.0f;
    f32 d;
    u8 r, g, b, a;

    GXSetBlendMode(esp->xA4, esp->xA5, esp->xA6, esp->xA7);
    CameraCurrentProjection();
    GXSetCullMode(0);
    if ((color >> 24) == 0xFE) {
        GXSetZMode(0, 3, 1);
    } else {
        GXSetZMode(1, 3, 1);
    }
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    GXLoadPosMtxImm(mtx, 0);
    GXSetCurrentMtx(0);
    r = (color >> 16) & 0xFF;
    g = (color >> 8) & 0xFF;
    b = color & 0xFF;
    a = 0xFF;

    end = *p1;
    PSVECSubtract(p1, p0, &dir);
    d = PSVECMag(&dir);
    if (d > len) {
#line 148 "D:/Bio4/Prog/esp19.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, len);
        PSVECAdd(p0, &dir, &end);
        d = len;
    }
    rate = 1.0f - d / len;

    GXBegin(0xB0, 0, 2);
    GXPosition3f32(p0->x, p0->y, p0->z);
    GXColor4u8(r, g, b, a);
    GXPosition3f32(end.x, end.y, end.z);
    GXColor4u8((u8)(r * rate), (u8)(g * rate), (u8)(b * rate), (u8)(a * rate));
}

extern "C" void Esp19_Trans(cEsp19* esp)
{
    Esp19Work* w = &esp->work;
    Vec p;
    u32 color;

    color = ((u32)esp->colR << 16) + ((u32)esp->colG << 8) + (u32)esp->colB + ((u32)esp->colA << 24);
    PSVECAdd(&esp->pos, &pG->quake_ofs, &p);
    Draw_line3d_local_222(&p, &w->target, pG->Cam.viewMat, color, esp, w->len);
}
