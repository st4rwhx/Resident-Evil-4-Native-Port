#include "filter.h"
#include "light.h"
#include "gx.h"
#include "main_sub.h"
#include "main_mem.h"
#include "os_vi.h"
#include "db_log.h"
#include "trans_ot.h"

// Depth-of-field filter: the frame is blurred through the half-size feedback buffer
// (Filter02DrawBuffer) and blended back through a Z-derived C8 mask (Filter02DrawBuffer2).

#define SCR_W ((u32) Screen.width)
#define SCR_H ((u32) Screen.height)

extern f32 ZNEAR;
extern f32 ZFAR;

extern "C" {
void Filter02GetEFB(int div, void* buf, int mip, f32 scale);
// DrawBuffer's call sites issue the FPR argument move before the GPR ones (see docs/matching.md,
// cAtariInfo::init): ABI-identical redeclaration with the float first.
void Filter02GetEFBF(f32 scale, int div, void* buf, int mip) asm("Filter02GetEFB");
void Filter02Render();
void Filter02GXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, f32 alpha, f32 s, f32 s2, void* buf);
void Filter02DrawBuffer();
void Filter02DrawBuffer2();
}

void* filter02_buff = 0;
static void* filter02_buff2 = 0;
void* filter02_buff3 = 0;
int f02_isFast = 1;
u16 CLUTdata[256] __attribute__((aligned(32)));
static f32 g_filter02_clip_dist;

static const f32 level_tbl02[32] = { 0.002f, 0.004f, 0.002f, 0.004f, 0.008f, 0.016f, 0.008f, 0.016f };

void Filter02Init()
{
    filter02_buff = 0;
    filter02_buff2 = 0;
    filter02_buff3 = 0;
}

void Filter02RoomInit()
{
    filter02_buff = 0;
    filter02_buff2 = 0;
    filter02_buff3 = 0;
    g_filter02_clip_dist = 2000.0f;
}

void Filter02Trans()
{
    static int use_filter2 = 0;

    if (use_filter2 == 0) {
        return;
    }
    if (Render_checkBlurPermission()) {
        AddOtDirect(0x12, (void*) 0xCDCDCDCD, Filter02Render, 7, 0x400, 0, 0.0f);
    }
}

void Filter02GetEFB(int div, void* buf, int mip, f32 scale)
{
    GXRenderModeObj* rm = &Rmode;
    static u8 vfilter[8] __attribute__((aligned(32))) = { 32, 0, 0, 0, 0, 0, 32, 0 };

    GXSetCopyFilter(0, rm->sample_pattern, 0, vfilter);
    GXSetTexCopySrc(0, 0, SCR_W / div, SCR_H / div);
    GXSetTexCopyDst((u32) ((f32) (SCR_W >> 1) / scale), (u32) ((f32) (SCR_H >> 1) / scale), 6, mip);
    GXCopyTex(buf, 0);
    GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, rm->vfilter);
    GXPixModeSync();
    GXInvalidateTexAll();
}

void Filter02Render()
{
    GXColor col = { 0, 0, 0, 0 };
    f32 y;
    int a;

    SetNoScissor();
    filter02_buff = GetDrawTmpBufAddr(7);
    if (filter02_buff == 0) {
        pLog->warn(0, 0, "Filter02Trans() : not enough memory");
        return;
    }
    if (filter02_buff2 == 0) {
#line 148 "D:/Bio4/Prog/filter02.cpp"
        filter02_buff2 = MEM_ALLOC(0xE000, 1, 13);
        if (filter02_buff2 == 0) {
            pLog->warn(0, 0, "FILTER02:not enough memory!!");
            return;
        }
        DCInvalidateRange(filter02_buff2, 0xE000);
    }
    if (filter02_buff3 == 0) {
#line 157
        filter02_buff3 = MEM_ALLOC(0xE0000, 1, 13);
        if (filter02_buff3 == 0) {
            pLog->warn(0, 0, "FILTER02:not enough memory!!");
            return;
        }
        DCInvalidateRange(filter02_buff3, 0xE0000);
    }
    DCInvalidateRange(filter02_buff, 0x38000);
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, col);
    SetNearClipDist(g_filter02_clip_dist);
    Filter02GetEFB(1, filter02_buff3, 0, 0.5f);
    Filter02DrawBuffer();
    a = 0xFF;
    y = 0.0f;
    Filter02GXDraw(y, y, 65530.0f, y, y, (f32) (u8) a, 0.5f, 1.0f, filter02_buff3);
    SetScissorState();
    Filter02DrawBuffer2();
    LightMgr.setFog();
    SetScissorState();
}

void Filter02GXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, f32 alpha, f32 s, f32 s2, void* buf)
{
    GXTexObj tex;

    GXInitTexObj(&tex, buf, (u32) ((f32) (SCR_W >> 1) / s), (u32) ((f32) (SCR_H >> 1) / s), 6, 0, 0, 0);
    GXInitTexObjLOD(&tex, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXLoadTexObj(&tex, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3f32(x + 0.0f, y + 0.0f, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(u + 0.0f, v + 0.0f);
    GXPosition3f32(x + (f32) SCR_W / s2, y + 0.0f, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(u + 1.0f, v + 0.0f);
    GXPosition3f32(x + (f32) SCR_W / s2, y + (f32) SCR_H / s2, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(u + 1.0f, v + 1.0f);
    GXPosition3f32(x + 0.0f, y + (f32) SCR_H / s2, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(u + 0.0f, v + 1.0f);
}

void Filter02DrawBuffer()
{
    Mtx44 proj;
    Mtx mv;
    GXColor amb;
    f32 y = 0.0f;
    f32 x;
    f32 px;
    f32 py;
    f32 z;
    u8 a;
    u32 i;
    int j;
    int n;
    static u32 FocusLevel = 7;
    static f32 x_old = 0.0f;
    static f32 y_old = 0.0f;

    GXSetAlphaCompare(7, 0, 1, 7, 0);
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(0);
    GXSetCullMode(0);
    C_MTXOrtho(proj, 0.0f, (f32) SCR_H, 0.0f, (f32) SCR_W, 0.0f, -65536.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mv);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(1, 4, 5, 0);
    GXSetNumTevStages(1);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(0, 1, 4, 0x3C);
    GXSetChanCtrl(0, 1, 1, 1, 0, 2, 1);
    GXSetChanCtrl(2, 1, 1, 1, 0, 2, 1);
    GXSetChanAmbColor(4, amb);
    GXSetNumChans(1);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevColorIn(0, 15, 15, 15, 8);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 7, 7, 5);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
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
    z = 65530.0f;
    Filter02GetEFBF(1.0f, 1, filter02_buff, 1);
    py = 0.0f;
    a = 0xFF;
    Filter02GXDraw(px, py, z, 0.0f, 0.0f, (f32) a, 1.0f, 2.0f, filter02_buff);
    x = 0.0f;
    if (f02_isFast == 0) {
        static f32 ppx = 0.0f;
        static f32 ppy = 0.0f;
        static u32 nFeedLp = 2;

        Filter02GetEFBF(2.0f, 2, filter02_buff, 1);
        Filter02GXDraw(px, py, z, 0.0f, 0.0f, (f32) a, 2.0f, 4.0f, filter02_buff);
        for (i = 0; i < nFeedLp; i++) {
            n = 4;
            switch (FocusLevel) {
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

                a = 0x80;
                Filter02GetEFBF(4.0f, 4, filter02_buff, 1);
                t = level_tbl02[j];
                switch (j) {
                case 0:
                case 4:
                    x = x_old - t;
                    y = y_old;
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
                x += ppx;
                y += ppy;
                Filter02GXDraw(px, py, z, x, y, (f32) a, 4.0f, 4.0f, filter02_buff);
            }
        }
        Filter02GetEFBF(4.0f, 4, filter02_buff, 1);
    } else {
        static f32 ppx = 0.0f;
        static f32 ppy = 0.0f;
        static u32 nFeedLp = 2;

        for (i = 0; i < nFeedLp; i++) {
            n = 4;
            switch (FocusLevel) {
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

                a = 0x80;
                Filter02GetEFBF(2.0f, 2, filter02_buff, 1);
                t = level_tbl02[j];
                switch (j) {
                case 0:
                case 4:
                    x = x_old - t;
                    y = y_old;
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
                x += ppx;
                y += ppy;
                Filter02GXDraw(px, py, z, x, y, (f32) a, 2.0f, 2.0f, filter02_buff);
            }
        }
        Filter02GetEFBF(2.0f, 2, filter02_buff, 1);
    }
}

void Filter02DrawBuffer2()
{
    Mtx44 proj;
    Mtx mv;
    GXColor amb;
    f32 y = 0.0f;
    f32 x;
    f32 s;
    f32 z;
    int a = 0xFF;
    int d;
    u32 i;
    static u8 vfilter[8] __attribute__((aligned(32))) = { 32, 0, 0, 0, 0, 0, 32, 0 };
    static int f02_start_no = 0xE6;
    static int f02_add_num = 0x10;
    static int f02_set_alpha = 1;
    static int f02_set_alpha2 = 0;

    GXSetAlphaCompare(7, 0, 1, 7, 0);
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(0);
    GXSetCullMode(0);
    C_MTXOrtho(proj, 0.0f, (f32) SCR_H, 0.0f, (f32) SCR_W, 0.0f, -65536.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mv);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(1, 4, 5, 0);
    GXSetNumTevStages(1);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(0, 1, 4, 0x3C);
    GXSetChanCtrl(0, 1, 1, 1, 0, 2, 1);
    GXSetChanCtrl(2, 1, 1, 1, 0, 2, 1);
    GXSetChanAmbColor(4, amb);
    GXSetNumChans(1);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevColorIn(0, 15, 15, 15, 8);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 7, 7, 5);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    GXSetZMode(0, 6, 0);
    z = 65530.0f;
    y = 0.0f;
    {
        int div = 1;
        int div2 = 1;
        GXRenderModeObj* rm = &Rmode;

        GXSetCopyFilter(0, rm->sample_pattern, 0, vfilter);
        GXSetTexCopySrc(0, 0, SCR_W / div, SCR_H / div);
        GXSetTexCopyDst((SCR_W >> 1) / div2, (SCR_H >> 1) / div2, 0x11, 1);
        GXCopyTex(filter02_buff2, 0);
        GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, rm->vfilter);
        GXPixModeSync();
        GXInvalidateTexAll();
    }
    GXTexObj tex;
    GXTexObj texCI;
    GXTlutObj tlut;
    s = 1.0f;
    x = 0.0f;
    if (f02_isFast == 0) {
        d = 4;
    } else {
        d = 2;
    }
    GXInitTexObj(&tex, filter02_buff, (SCR_W >> 1) / d, (SCR_H >> 1) / d, 6, 0, 0, 0);
    GXInitTexObjLOD(&tex, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXLoadTexObj(&tex, 0);
    for (i = 0; i < 256; i++) {
        if (i < f02_start_no) {
            CLUTdata[i] = 0;
        } else {
            u16 c;

            c = i - f02_start_no;
            c *= f02_add_num;
            if (c > 0xFF) {
                c = 0xFF;
            }
            CLUTdata[i] = (c << 8) + c;
        }
    }
    GXInitTlutObj(&tlut, CLUTdata, 0, 256);
    {
        int div3 = 1;

        GXInitTexObjCI(&texCI, filter02_buff2, (SCR_W >> 1) / div3, (SCR_H >> 1) / div3, 9, 0, 0, 0, 0);
    }
    GXLoadTlut(&tlut, 0);
    GXLoadTexObj(&texCI, 1);
    GXSetNumTexGens(2);
    GXSetTexCoordGen(0, 1, 4, 0x3C);
    GXSetTexCoordGen(1, 1, 4, 0x3C);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevColorIn(0, 15, 15, 15, 8);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 7, 7, 5);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    GXSetNumTevStages(1);
    if (f02_set_alpha == 1) {
        GXSetTevOrder(1, 1, 1, 4);
        GXSetTevColorIn(1, 15, 0, 15, 0);
        GXSetTevColorOp(1, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(1, 7, 7, 7, 4);
        GXSetTevAlphaOp(1, 0, 0, 0, 1, 0);
        GXSetNumTevStages(2);
    }
    if (f02_set_alpha2 == 1) {
        GXSetTevOrder(1, 1, 1, 4);
        GXSetTevColorIn(1, 15, 0, 15, 8);
        GXSetTevColorOp(1, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(1, 7, 7, 7, 4);
        GXSetTevAlphaOp(1, 0, 0, 0, 1, 0);
        GXSetNumTevStages(2);
    }
    GXBegin(0x80, 0, 4);
    GXPosition3f32(y + 0.0f, y + 0.0f, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, a);
    GXTexCoord2f32(x + 0.0f, x + 0.0f);
    GXPosition3f32(y + (f32) SCR_W / s, y + 0.0f, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, a);
    GXTexCoord2f32(x + 1.0f, x + 0.0f);
    GXPosition3f32(y + (f32) SCR_W / s, y + (f32) SCR_H / s, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, a);
    GXTexCoord2f32(x + 1.0f, x + 1.0f);
    GXPosition3f32(y + 0.0f, y + (f32) SCR_H / s, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, a);
    GXTexCoord2f32(x + 0.0f, x + 1.0f);
    GXSetZMode(1, 3, 1);
    GXSetAlphaUpdate(1);
}

asm(".section .sdata,\"aw\"\n\t.balign 32\n\t.text");
