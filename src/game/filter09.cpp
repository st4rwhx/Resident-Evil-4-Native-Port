#include "filter.h"
#include "light.h"
#include "gx.h"
#include "main_sub.h"
#include "main_mem.h"
#include "os_vi.h"
#include "db_log.h"
#include "trans_ot.h"

// Screen fade-out filter: keeps a half-size copy of the last frame and blends it back,
// spreading it outwards (g_bSpred) while the game is stopped.

#define SCR_W ((u32) Screen.width)
#define SCR_H ((u32) Screen.height)

extern f32 ZNEAR;
extern f32 ZFAR;
int GetDrawTmpBufType();

extern "C" {
void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht);
void Filter09Render();
void Filter09GetEFB(int div, int div2);
static void Filter09GXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, f32 alpha, f32 s, f32 ofs, int div);
void Filter09DrawBuffer();
void Filter09GetEFB_801D19E0();
void Filter09SetbUse(int use, int spred);
}

void* filter09_buff = 0;
static int g_bUse;
int g_bSpred;
int g_bGet;

void Filter09Init()
{
    Filter09RoomInit();
}

void Filter09RoomInit()
{
    filter09_buff = 0;
    g_bUse = 0;
    g_bGet = 0;
}

// Never called (the original linker dropped the body; its 0.0f pool constant stayed).
static void Filter09Trans()
{
    if (Render_checkBlurPermission()) {
        AddOtDirect(0x12, (void*) 0xCDCDCDCD, Filter09Render, 7, 0x400, 0, 0.0f);
    }
}

void Filter09GetEFB(int div, int div2)
{
    GXRenderModeObj* rm = &Rmode;
    static u8 vfilter[7] __attribute__((aligned(32))) = { 32, 0, 0, 0, 0, 0, 32 };

    GXSetCopyFilter(0, rm->sample_pattern, 0, vfilter);
    GXSetTexCopySrc(0, 0, SCR_W / div, SCR_H / div);
    GXSetTexCopyDst((SCR_W >> 1) / div2, (SCR_H >> 1) / div2, 6, 1);
    GXCopyTex(filter09_buff, 0);
    GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, rm->vfilter);
    GXPixModeSync();
    GXInvalidateTexAll();
}

void Filter09Render()
{
    GXColor col = { 0, 0, 0, 0 };

    if (g_bUse == 0) {
        return;
    }
    if (g_bGet == 0 && GetDrawTmpBufType() != 0) {
        pLog->err(0, 0, "Filter09SetbUse() : someone use TmpBuffer!!");
        return;
    }
    filter09_buff = GetDrawTmpBufAddr(12);
    if (filter09_buff == 0) {
        pLog->warn(0, 0, "Filter09Trans() : not enough memory");
        return;
    }
    DCInvalidateRange(filter09_buff, 0x38000);
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, col);
    GXSetScissor(0, 56, SCR_W, SCR_H - 111);
    Filter09DrawBuffer();
    SetScissorState();
    LightMgr.setFog();
}

static void Filter09GXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, f32 alpha, f32 s, f32 ofs, int div)
{
    GXTexObj tex;

    GXInitTexObj(&tex, filter09_buff, (SCR_W >> 1) / div, (SCR_H >> 1) / div, 6, 0, 0, 0);
    GXInitTexObjLOD(&tex, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXLoadTexObj(&tex, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3f32(x - ofs, y - ofs, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(u + 0.0f, v + 0.0f);
    GXPosition3f32(x + (f32) SCR_W / s + ofs, y - ofs, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(u + 1.0f, v + 0.0f);
    GXPosition3f32(x + (f32) SCR_W / s + ofs, y + (f32) SCR_H / s + ofs, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(u + 1.0f, v + 1.0f);
    GXPosition3f32(x - ofs, y + (f32) SCR_H / s + ofs, z);
    GXColor4u8(0xFF, 0xFF, 0xFF, (u8) alpha);
    GXTexCoord2f32(u + 0.0f, v + 1.0f);
}

void Filter09DrawBuffer()
{
    Mtx44 proj;
    Mtx mv;
    GXColor amb = { 0, 0, 0, 0 };
    f32 y = 0.0f;
    f32 z;
    int a;
    static f32 fade_alpha = 64.0f;
    static f32 fade_scale = 20.0f;

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
    GXSetChanCtrl(0, 0, 1, 1, 0, 2, 1);
    GXSetChanCtrl(2, 0, 1, 1, 0, 2, 1);
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
    z = 65530.0f;
    if (g_bGet == 1) {
        g_bGet = 0;
        Filter09GetEFB(1, 1);
    }
    a = 0xFF;
    Filter09GXDraw(y, y, z, y, y, (f32) (u8) a, 1.0f, 1.0f, 1);
    if (g_bSpred == 1) {
        Filter09GXDraw(y, y, z, y, y, fade_alpha, 1.0f, fade_scale, 1);
        Filter09GetEFB(1, 1);
    }
    GXSetZMode(1, 3, 1);
    GXSetAlphaUpdate(1);
}

void Filter09GetEFB_801D19E0()
{
    g_bGet = 1;
}

void Filter09SetbUse(int use, int spred)
{
    g_bUse = use;
    g_bSpred = spred;
}

int Filter09GetbUse()
{
    return g_bUse;
}

asm(".section .sdata,\"aw\"\n\t.balign 32\n\t.text");
asm(".section .sbss,\"aw\",@nobits\n\t.balign 8\n\t.text");
