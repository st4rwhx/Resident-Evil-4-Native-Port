#include "filter.h"
#include "light.h"
#include "gx.h"
#include "main_sub.h"
#include "main_mem.h"
#include "os_vi.h"
#include "db_log.h"
#include "trans_ot.h"

// Captured-screen overlay: Filter0bCapture copies the frame buffer to a half-size texture that
// is blended back with filter0b_alpha (once per SetAlpha) inside the letterbox area.

#define SCR_W ((u32) Screen.width)
#define SCR_H ((u32) Screen.height)

extern f32 ZNEAR;
extern f32 ZFAR;

extern "C" {
void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht);
void Filter0bAllocBuf();
void Filter0bFreeBuf();
void Filter0bCapture();
void Filter0bSetAlpha(u8 alpha);
void Filter0bRender();
void Filter0bGetEFB(int div, int div2);
void Filter0bGXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, f32 alpha, f32 s, f32 ofs, int div);
void Filter0bDrawBuffer();
}

void* filter0b_buff = 0;
u8 filter0b_alpha = 0;

void Filter0bInit()
{
    Filter0bRoomInit();
}

void Filter0bRoomInit()
{
    filter0b_buff = 0;
    filter0b_alpha = 0;
}

void Filter0bAllocBuf()
{
    if (filter0b_buff == 0) {
#line 76 "D:/Bio4/Prog/filter0b.cpp"
        filter0b_buff = MEM_ALLOC(0x38000, 1, 13);
    }
}

void Filter0bFreeBuf()
{
    if (filter0b_buff) {
        Mem_free(filter0b_buff);
        filter0b_buff = 0;
    }
}

void Filter0bCapture()
{
    Filter0bGetEFB(1, 1);
}

void Filter0bSetAlpha(u8 alpha)
{
    filter0b_alpha = alpha;
}

void Filter0bTrans()
{
    if (filter0b_alpha == 0) {
        return;
    }
    if (Render_checkBlurPermission()) {
        AddOtDirect(0x12, (void*) 0xCDCDCDCD, Filter0bRender, 7, 0x400, 0, 0.0f);
    }
}

void Filter0bGetEFB(int div, int div2)
{
    GXRenderModeObj* rm = &Rmode;
    static u8 vfilter[7] __attribute__((aligned(32))) = { 32, 0, 0, 0, 0, 0, 32 };

    GXSetCopyFilter(0, rm->sample_pattern, 0, vfilter);
    GXSetTexCopySrc(0, 0, SCR_W / div, SCR_H / div);
    GXSetTexCopyDst((SCR_W >> 1) / div2, (SCR_H >> 1) / div2, 6, 1);
    GXCopyTex(filter0b_buff, 0);
    GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, rm->vfilter);
    GXPixModeSync();
    GXInvalidateTexAll();
}

void Filter0bRender()
{
    DCInvalidateRange(filter0b_buff, 0x38000);
    GXColor col = { 0, 0, 0, 0 };
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, col);
    GXSetScissor(0, 56, SCR_W, SCR_H - 111);
    Filter0bDrawBuffer();
    SetScissorState();
    LightMgr.setFog();
}

void Filter0bGXDraw(f32 x, f32 y, f32 z, f32 u, f32 v, f32 alpha, f32 s, f32 ofs, int div)
{
    GXTexObj tex;

    GXInitTexObj(&tex, filter0b_buff, (SCR_W >> 1) / div, (SCR_H >> 1) / div, 6, 0, 0, 0);
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

void Filter0bDrawBuffer()
{
    Mtx44 proj;
    Mtx mv;
    GXColor amb = { 0, 0, 0, 0 };
    f32 y = 0.0f;
    f32 z;
    u8 a;

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
    a = filter0b_alpha;
    filter0b_alpha = 0;
    Filter0bGXDraw(y, y, z, y, y, (f32) a, 1.0f, 1.0f, 1);
    GXSetZMode(1, 3, 1);
    GXSetAlphaUpdate(1);
}

asm(".section .sdata,\"aw\"\n\t.balign 32\n\t.text");
