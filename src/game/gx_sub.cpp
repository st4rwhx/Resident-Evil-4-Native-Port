#include "types.h"
#include "vec.h"
#include "global.h"
#include "gx.h"
#include "light.h"
#include "gx_sub.h"

extern f32 ZNEAR;
extern f32 ZFAR;

GXColor g_sysBgColor = {0, 0, 0, 0};
GXColor clr_black = {0, 0, 0, 0};

extern "C" void bio4_AddBgColor();

// Copy-clear colour: the frame buffer is cleared to black, the background colour is drawn
// by bio4_AddBgColor instead.
void bio4_GXSetCopyClear(GXColor color, u32 z)
{
    pG->flags_5010 |= 0x40;
    g_sysBgColor = color;
    GXSetCopyClear(clr_black, z);
}

// Draw the background colour as a full-screen quad at the far plane.
void bio4_AddBgColor()
{
    static f32 TEST_Z = -0.99999f;
    Mtx44 proj;
    Mtx mtx;
    GXColor black = {0, 0, 0, 0};
    GXColor bg;

    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, black);
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(1);
    GXSetCullMode(0);
    GXSetZMode(1, 7, 1);
    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, 1.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mtx);
    GXLoadPosMtxImm(mtx, 0);
    GXSetCurrentMtx(0);
    GXSetNumTevStages(1);
    GXSetNumChans(1);
    GXSetNumTexGens(0);

    if ((s32) pG->flags_60 >= 0) {
        pG->flags_5010 &= ~0x40;
    }
    if (!(pG->flags_5010 & 0x40)) {
        cLightEnv* env = LightMgr.getEnvPtr();
        bg = env->bgColor;
        if (env->x8 == 0) {
            bg.b = 0;
            bg.g = 0;
            bg.r = 0;
        }
        bg.a = 0;
    } else {
        bg = g_sysBgColor;
    }
    GXSetChanMatColor(4, bg);
    GXSetChanAmbColor(4, bg);

    if (bg.r != 0 || bg.g != 0 || bg.b != 0 || bg.a != 0) {
        GXSetBlendMode(1, 1, 1, 0);
        GXSetChanCtrl(0, 0, 0, 0, 0, 2, 2);
        GXSetChanCtrl(2, 0, 0, 0, 0, 2, 2);
        GXSetTevOrder(0, 0xFF, 0xFF, 4);
        GXSetTevColorIn(0, 0xF, 0xF, 0xF, 0xA);
        GXSetTevColorOp(0, 0, 0, 0, 1, 0);
        GXSetTevAlphaIn(0, 7, 7, 7, 5);
        GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
        GXClearVtxDesc();
        GXSetVtxDesc(9, 1);
        GXSetVtxAttrFmt(0, 9, 1, 4, 0);
        GXBegin(0x80, 0, 4);
        GXPosition3f32(0.0f, 0.0f, TEST_Z);
        GXPosition3f32(512.0f, 0.0f, TEST_Z);
        GXPosition3f32(512.0f, 448.0f, TEST_Z);
        GXPosition3f32(0.0f, 448.0f, TEST_Z);
    }
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(0);
    LightMgr.setFog();
}
