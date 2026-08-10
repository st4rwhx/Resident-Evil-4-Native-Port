#include "filter.h"
#include "light.h"
#include "gx.h"
#include "main_sub.h"
#include "os_vi.h"
#include "db_log.h"
#include "trans_ot.h"

// Depth-of-field filter: copies the frame buffer to a half-size texture and draws it back
// shifted by `level_tbl1[level]` pixels, in front of / behind the focus depth.

#define SCR_W ((u32) Screen.width)
#define SCR_H ((u32) Screen.height)

struct LensEffectWork {
    int on;     // 0x00
    int z;      // 0x04  focus depth (screen z, 0..65535)
    f32 level;  // 0x08  blur level (index into level_tbl1)
    u8 mode;    // 0x0C  0 near, 1 far
    u8 type;    // 0x0D  0 quads by level, 1 four quads + fade
};

extern f32 ZNEAR;
extern f32 ZFAR;
void* GetDrawTmpBufAddr(int type);

void* filter01_buff = 0;
LensEffectWork g_LeNear;
LensEffectWork g_LeFar;
LensEffectWork g_LeLit;

static const f32 level_tbl1[11] = { 0.0f, 0.6f, 1.5f, 2.6f, 1.5f, 1.6f, 1.6f, 2.5f, 2.6f, 4.5f, 6.6f };

extern "C" {
void Filter01Render(LensEffectWork* w);
void Filter01SetParam(int mode, int z, u8 type, f32 level);
void Filter01SetParam_CamZ(int mode, u8 type, f32 level, f32 camz);
}

void Filter01Init()
{
    filter01_buff = 0;
    LightMgr.getEnvPtr()->x2D = 0;
    g_LeNear.on = 0;
    g_LeNear.mode = 0;
    g_LeFar.on = 0;
    g_LeFar.mode = 1;
}

void Filter01RoomInit()
{
    filter01_buff = 0;
    LightMgr.getEnvPtr()->x2D = 0;
    g_LeNear.on = 0;
    g_LeNear.mode = 0;
    g_LeFar.on = 0;
    g_LeFar.mode = 1;
}

void Filter01Trans()
{
    if (Render_checkBlurPermission()) {
        cLightEnv* env = LightMgr.getEnvPtr();
        if (g_LeNear.on == 0 && g_LeFar.on == 0 && env->x2D) {
            g_LeLit.on = 1;
            g_LeLit.z = env->x28;
            g_LeLit.level = (f32) env->x2D;
            g_LeLit.mode = env->x2E;
            AddOtDirect(0x12, &g_LeLit, (void (*)()) Filter01Render, 7, 0x400, 0, 0.0f);
        }
        if (g_LeNear.on) {
            AddOtDirect(0x12, &g_LeNear, (void (*)()) Filter01Render, 7, 0x400, 0, 0.0f);
        }
        if (g_LeFar.on) {
            AddOtDirect(0x12, &g_LeFar, (void (*)()) Filter01Render, 7, 0x400, 0, 0.0f);
        }
        g_LeNear.on = 0;
        g_LeFar.on = 0;
    }
}

// Copy the frame buffer to the half-size blur texture.
static inline void Filter01CopyEFB(u8* vf)
{
    GXRenderModeObj* rm = &Rmode;

    GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, vf);
    GXSetTexCopySrc(0, 0, SCR_W, SCR_H);
    GXSetTexCopyDst(SCR_W / 2, SCR_H / 2, 6, 1);
    GXCopyTex(filter01_buff, 0);
    GXSetCopyFilter(rm->aa, rm->sample_pattern, 1, rm->vfilter);
    GXPixModeSync();
    GXInvalidateTexAll();
}

void Filter01Render(LensEffectWork* w)
{
    GXTexObj tex;
    Mtx44 proj;
    Mtx mv;
    GXColor col = { 0, 0, 0, 0 };
    GXColor amb;
    f32 lv = level_tbl1[(u8) w->level];
    f32 z = 0.0f;
    f32 x;
    f32 y;
    f32 cx;
    f32 cy;
    int i;
    static u8 vfilter[7] __attribute__((aligned(32))) = { 32, 0, 0, 0, 0, 0, 32 };

    filter01_buff = GetDrawTmpBufAddr(6);
    if (filter01_buff == 0) {
        pLog->warn(0, 0, "Filter01Trans() : not enough memory");
        return;
    }
    DCInvalidateRange(filter01_buff, 0x38000);
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, col);
    Filter01CopyEFB(vfilter);
    GXSetAlphaCompare(7, 0, 1, 7, 0);
    GXInitTexObj(&tex, filter01_buff, SCR_W / 2, SCR_H / 2, 6, 0, 0, 0);
    GXInitTexObjLOD(&tex, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
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
    GXLoadTexObj(&tex, 0);
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
    x = 0.0f;
    y = 0.0f;
    cx = 0.0f;
    cy = 0.0f;
    switch (w->mode) {
    case 0:
        GXSetZMode(1, 6, 0);
        z = (f32) w->z;
        break;
    case 1:
        GXSetZMode(1, 3, 0);
        z = (f32) w->z;
        break;
    default:
        pLog->err(0, 0, "Filter01: [%d]invalid FocusMode.", w->mode);
        break;
    }
    if (w->type == 0) {
        int n = 4;
        static f32 fc_z_plus = 100.0f;

        switch ((u8) w->level) {
        case 0:
            n = 0;
            break;
        case 1:
            n = 1;
            break;
        case 2:
            n = 1;
            break;
        case 3:
            n = 1;
            break;
        case 4:
            n = 2;
            break;
        case 5:
            n = 3;
            break;
        }
        for (i = 0; i < n; i++) {
            switch (i) {
            case 0:
                x = cx - lv;
                y = cy - lv;
                break;
            case 1:
                x = cx + lv;
                y = cy + lv;
                break;
            case 2:
                x = cx + lv;
                y = cy - lv;
                break;
            case 3:
                x = cx - lv;
                y = cy + lv;
                break;
            }
            z += fc_z_plus;
            if (z > 65536.0f) {
                z = 65536.0f;
            }
            GXBegin(0x80, 0, 4);
            GXPosition3f32(x + 0.0f, y + 0.0f, z);
            GXColor4u8(0xFF, 0xFF, 0xFF, 0x80);
            GXTexCoord2f32(0.0f, 0.0f);
            GXPosition3f32(x + (f32) SCR_W, y + 0.0f, z);
            GXColor4u8(0xFF, 0xFF, 0xFF, 0x80);
            GXTexCoord2f32(1.0f, 0.0f);
            GXPosition3f32(x + (f32) SCR_W, y + (f32) SCR_H, z);
            GXColor4u8(0xFF, 0xFF, 0xFF, 0x80);
            GXTexCoord2f32(1.0f, 1.0f);
            GXPosition3f32(x + 0.0f, y + (f32) SCR_H, z);
            GXColor4u8(0xFF, 0xFF, 0xFF, 0x80);
            GXTexCoord2f32(0.0f, 1.0f);
        }
    } else if (w->type == 1) {
        int n = 4;
        static f32 fc_z_plus = 100.0f;

        lv = w->level * 0.33f;
        for (i = 0; i < n; i++) {
            switch (i) {
            case 0:
                x = cx - lv;
                y = cy - lv;
                break;
            case 1:
                x = cx + lv;
                y = cy + lv;
                break;
            case 2:
                x = cx + lv;
                y = cy - lv;
                break;
            case 3:
                x = cx - lv;
                y = cy + lv;
                break;
            }
            z += fc_z_plus;
            if (z > 65536.0f) {
                z = 65536.0f;
            }
            GXBegin(0x80, 0, 4);
            GXPosition3f32(x + 0.0f, y + 0.0f, z);
            GXColor4u8(0xFF, 0xFF, 0xFF, 0x80);
            GXTexCoord2f32(0.0f, 0.0f);
            GXPosition3f32(x + (f32) SCR_W, y + 0.0f, z);
            GXColor4u8(0xFF, 0xFF, 0xFF, 0x80);
            GXTexCoord2f32(1.0f, 0.0f);
            GXPosition3f32(x + (f32) SCR_W, y + (f32) SCR_H, z);
            GXColor4u8(0xFF, 0xFF, 0xFF, 0x80);
            GXTexCoord2f32(1.0f, 1.0f);
            GXPosition3f32(x + 0.0f, y + (f32) SCR_H, z);
            GXColor4u8(0xFF, 0xFF, 0xFF, 0x80);
            GXTexCoord2f32(0.0f, 1.0f);
        }
        if (w->level > 1.0f) {
            f32 a = w->level - 1.0f;
            u8 alpha;
            static u8 vfilter[7] __attribute__((aligned(32))) = { 32, 0, 0, 0, 0, 0, 32 };

            a *= 25.0f;
            if (a > 255.0f) {
                a = 255.0f;
            }
            alpha = (u8) a;
            if (a > 0.0f) {
                Filter01CopyEFB(vfilter);
                GXBegin(0x80, 0, 4);
                GXPosition3f32(x + 0.25f, y + 0.25f, z);
                GXColor4u8(0xFF, 0xFF, 0xFF, alpha);
                GXTexCoord2f32(0.0f, 0.0f);
                GXPosition3f32(x + (f32) SCR_W + 0.25f, y + 0.25f, z);
                GXColor4u8(0xFF, 0xFF, 0xFF, alpha);
                GXTexCoord2f32(1.0f, 0.0f);
                GXPosition3f32(x + (f32) SCR_W + 0.25f, y + (f32) SCR_H + 0.25f, z);
                GXColor4u8(0xFF, 0xFF, 0xFF, alpha);
                GXTexCoord2f32(1.0f, 1.0f);
                GXPosition3f32(x + 0.25f, y + (f32) SCR_H + 0.25f, z);
                GXColor4u8(0xFF, 0xFF, 0xFF, alpha);
                GXTexCoord2f32(0.0f, 1.0f);
            }
        }
    }
    GXSetZMode(1, 3, 1);
    GXSetAlphaUpdate(1);
    LightMgr.setFog();
}

// Never called: the original linker dropped the body but kept its statics and constant pool.
static void Filter01SetParam_ScrZ(int mode, u8 type, f32 level, f32 z)
{
    static f32 Zscale = 1.0f;
    static f32 Zoffset = 1.0f;

    z = (1.0f - z) * Zscale + Zoffset;
    Filter01SetParam(mode, (u32) (z * 65535.0f), type, level);
}

void Filter01SetParam_CamZ(int mode, u8 type, f32 level, f32 camz)
{
    static f32 Zscale = 1.0f;
    static f32 Zoffset = 1.0f;
    f32 inv;
    f32 zv;

    if (camz == 0.0f) {
        camz = 0.01f;
    }
    camz = -camz;
    inv = 1.0f / (ZFAR - ZNEAR);
    zv = (-(ZFAR * ZNEAR) * inv + (-ZNEAR * inv) * camz) * Zscale;
    Filter01SetParam(mode, (u32) (((1.0f / -camz) * zv + Zoffset) * 65535.0f), type, level);
}

void Filter01SetParam(int mode, int z, u8 type, f32 level)
{
    if (mode == 0) {
        g_LeNear.on = 1;
        g_LeNear.mode = 0;
        g_LeNear.level = level;
        g_LeNear.z = z;
        g_LeNear.type = type;
    } else {
        g_LeFar.on = 1;
        g_LeFar.mode = 1;
        g_LeFar.level = level;
        g_LeFar.z = z;
        g_LeFar.type = type;
    }
}

// The next unit's .sdata (filter06: 32-byte aligned vfilter tables) starts 32-byte aligned.
asm(".section .sdata,\"aw\"\n\t.balign 32\n\t.text");
