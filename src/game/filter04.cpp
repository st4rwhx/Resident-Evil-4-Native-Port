#include "filter.h"
#include "light.h"
#include "gx.h"
#include "main_sub.h"
#include "main_mem.h"
#include "os_vi.h"
#include "db_log.h"
#include "trans_ot.h"
#include "joy.h"

// Radial glow filter: like filter03 (alpha glow), but the blurred copy is spread from
// (spread_center_x, spread_center_y) with pow_x/pow_y and blended additively.

#define SCR_W ((u32) Screen.width)
#define SCR_H ((u32) Screen.height)

struct Flt04Work {
    int on;      // 0x00
    u32 level;   // 0x04  blur spread (level_tbl4 rows)
    u32 loop;    // 0x08  feedback passes
    u8 r;        // 0x0C
    u8 g;        // 0x0D
    u8 b;        // 0x0E
    u8 alpha;    // 0x0F  feedback alpha
    u8 amb;      // 0x10  ambient color
};

extern f32 ZNEAR;
extern f32 ZFAR;

void* filter04_buff = 0;
void* filter04_buff2 = 0;
Flt04Work flt04;

static const f32 level_tbl4[32] = { 0.016f, 0.008f, 0.008f, 0.008f, 0.008f, 0.016f, 0.008f, 0.016f };

extern "C" {
void Filter04Render();
void Filter04GetEFB(int div, int div2, void* buf, int mipmap);
void Filter04GXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, u8 r, u8 g, u8 b, u8 a, f32 s, int div, int fmt, void* buf);;;
void Filter04DrawBuffer();
}

void Filter04Init()
{
    filter04_buff = 0;
}

void Filter04RoomInit()
{
    filter04_buff = 0;
    memclr_asm(&flt04, sizeof(Flt04Work));
    flt04.level = 0;
    flt04.loop = 20;
    flt04.r = 0xFF;
    flt04.g = 0x80;
    flt04.b = 0x40;
    flt04.alpha = 0x7B;
    flt04.amb = 0xFF;
}

void Filter04Trans()
{
    static int use_filter4 = 0;

    if (use_filter4 == 0) {
        return;
    }
    if (Render_checkBlurPermission()) {
        AddOtDirect(0x12, (void*) 0xCDCDCDCD, Filter04Render, 7, 0x400, 0, 0.0f);
    }
}

void Filter04GetEFB(int div, int div2, void* buf, int mipmap)
{
    GXRenderModeObj* rm = &Rmode;
    static u8 vfilter[7] __attribute__((aligned(32))) = { 32, 0, 0, 0, 0, 0, 32 };

    GXSetCopyFilter(0, rm->sample_pattern, 0, vfilter);
    GXSetTexCopySrc(0, 0, SCR_W / div, SCR_H / div);
    GXSetTexCopyDst((SCR_W >> 1) / div2, (SCR_H >> 1) / div2, 0x27, mipmap);
    GXCopyTex(buf, 0);
    GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, rm->vfilter);
    GXPixModeSync();
    GXInvalidateTexAll();
}

void Filter04Render()
{
    GXColor col = { 0, 0, 0, 0 };

    filter04_buff = GetDrawTmpBufAddr(9);
    if (filter04_buff == 0) {
        pLog->warn(0, 0, "Filter04Trans() : not enough memory");
        return;
    }
    filter04_buff2 = (u8*) GetDrawTmpBufAddr(9) + 0x1C000;
    DCInvalidateRange(filter04_buff, 0x38000);
    DCInvalidateRange(filter04_buff2, 0x38000);
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, col);
    Filter04DrawBuffer();
    LightMgr.setFog();
}

void Filter04GXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, u8 r, u8 g, u8 b, u8 a, f32 s, int div, int fmt, void* buf)
{
    GXTexObj tex;
    static f32 pow_x = 0.02f;
    static f32 pow_y = 0.025f;
    static f32 spread_center_x = 0.0f;
    static f32 spread_center_y = -1.0f;
    f32 cx;
    f32 cy;
    f32 cx2;
    f32 cy2;

    GXInitTexObj(&tex, buf, (SCR_W >> 1) / div, (SCR_H >> 1) / div, fmt, 0, 0, 0);
    GXInitTexObjLOD(&tex, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXLoadTexObj(&tex, 0);
    cx = spread_center_x + 0.5f;
    cy = spread_center_y + 0.5f;
    if (cx < -1.0f) {
        cx = -1.0f;
    }
    if (cx > 2.0f) {
        cx = 2.0f;
    }
    if (cy < -1.0f) {
        cy = -1.0f;
    }
    if (cy > 2.0f) {
        cy = 2.0f;
    }
    cx2 = 1.0f - cx;
    cy2 = 1.0f - cy;
    GXBegin(0x80, 0, 4);
    GXPosition3f32(x + 0.0f, y + 0.0f, z);
    GXColor4u8(r, g, b, a);
    GXTexCoord2f32(pow_x * cx, pow_y * cy);
    GXPosition3f32(x + (f32) SCR_W / s, y + 0.0f, z);
    GXColor4u8(r, g, b, a);
    GXTexCoord2f32(1.0f - pow_x * cx2, pow_y * cy);
    GXPosition3f32(x + (f32) SCR_W / s, y + (f32) SCR_H / s, z);
    GXColor4u8(r, g, b, a);
    GXTexCoord2f32(1.0f - pow_x * cx2, 1.0f - pow_y * cy2);
    GXPosition3f32(x + 0.0f, y + (f32) SCR_H / s, z);
    GXColor4u8(r, g, b, a);
    GXTexCoord2f32(pow_x * cx, 1.0f - pow_y * cy2);
}

void Filter04DrawBuffer()
{
    Mtx44 proj;
    Mtx mv;
    GXColor amb;
    f32 y = 0.0f;
    f32 x;
    f32 px;
    f32 py;
    f32 ppx;
    f32 z;
    u8 a;
    u32 i;
    int j;
    int n;
    static f32 x_old = 0.0f;
    static f32 y_old = 0.0f;
    static f32 ppy = 0.0f;
    static u8 av_alpha = 0xFF;

    GXSetAlphaCompare(7, 0, 1, 7, 0);
    GXSetColorUpdate(0);
    GXSetAlphaUpdate(1);
    GXSetCullMode(0);
    C_MTXOrtho(proj, 0.0f, (f32) SCR_H, 0.0f, (f32) SCR_W, 0.0f, -65536.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mv);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(1, 1, 0, 0);
    GXSetNumTevStages(1);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(0, 1, 4, 0x3C);
    GXSetChanCtrl(0, 1, 0, 1, 0, 2, 1);
    GXSetChanCtrl(2, 1, 0, 1, 0, 2, 1);
    amb.r = amb.g = amb.b = amb.a = flt04.amb;
    GXSetChanAmbColor(4, amb);
    GXSetNumChans(1);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevColorIn(0, 15, 10, 8, 15);
    GXSetTevColorOp(0, 0, 0, 1, 1, 0);
    GXSetTevAlphaIn(0, 7, 5, 4, 7);
    GXSetTevAlphaOp(0, 0, 0, 1, 1, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    y = 0.0f;
    GXSetZMode(0, 6, 0);
    px = 0.0f;
    py = 0.0f;
    ppx = 0.0f;
    x = 0.0f;
    z = 65530.0f;
    Filter04GetEFB(1, 1, filter04_buff, 1);
    Filter04GXDraw(px, py, z, 0.0f, 0.0f, 0xFF, 0xFF, 0xFF, 0xFF, 2.0f, 1, 1, filter04_buff);
    Filter04GetEFB(2, 2, filter04_buff, 1);
    Filter04GXDraw(px, py, z, 0.0f, 0.0f, 0xFF, 0xFF, 0xFF, 0xFF, 4.0f, 2, 1, filter04_buff);
    Filter04GetEFB(4, 2, filter04_buff, 0);
    Filter04GetEFB(4, 2, filter04_buff2, 0);
    a = flt04.alpha;
    for (i = 0; i < flt04.loop; i++) {
        n = 4;
        switch (flt04.level) {
        case 0:
            n = 1;
            break;
        case 1:
            n = 2;
            break;
        case 2:
            n = 3;
            break;
        case 3:
            n = 4;
            break;
        case 4:
            n = 5;
            break;
        case 5:
            n = 6;
            break;
        case 6:
            n = 7;
            break;
        case 7:
            n = 8;
            break;
        }
        for (j = 0; j < n; j++) {
            f32 t;

            if (Joy[0].on & 0x200) {
                GXSetBlendMode(1, 4, 5, 0);
                Filter04GXDraw(px, py, z, 0.0f, 0.0f, 0xFF, 0xFF, 0xFF, av_alpha, 4.0f, 2, 1, filter04_buff2);
            }
            t = level_tbl4[j];
            switch (j) {
            case 0:
            case 4:
                x = x_old - t * 0.0f;
                y = y_old - t;
                break;
            case 1:
            case 5:
                x = x_old + t;
                y = y_old;
                break;
            case 2:
            case 6:
                x = x_old;
                y = y_old - t;
                break;
            case 3:
            case 7:
                x = x_old;
                y = y_old + t;
                break;
            }
            y += ppy;
            GXSetBlendMode(1, 4, 5, 0);
            x += ppx;
            Filter04GXDraw(px, py, z, x, y, 0xFF, 0xFF, 0xFF, a, 4.0f, 2, 1, filter04_buff);
            Filter04GetEFB(4, 2, filter04_buff, 0);
            a = flt04.alpha;
        }
    }
    GXSetBlendMode(1, 1, 1, 0);
    Filter04GetEFB(4, 4, filter04_buff, 1);
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(0);
    Filter04GXDraw(px, py, z, x, y, flt04.r, flt04.g, flt04.b, 0xFF, 1.0f, 4, 1, filter04_buff);
    GXSetZMode(1, 3, 1);
    GXSetAlphaUpdate(1);
}

asm(".section .sdata,\"aw\"\n\t.balign 32\n\t.text");
