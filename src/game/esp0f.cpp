#include "light.h"
#include "atari.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"
#include "main_sub.h"

extern f32 ZNEAR;
extern f32 ZFAR;

struct Esp0fWork {
    u8 power;  // 0x00 TEV colour scale of the copied frame (0..2)
};

// Screen distortion sprite: copies the frame buffer into a texture and draws the sprite with
// that texture projected onto it, modulated by the sprite's own texture.
class cEsp0f : public cEsp {
public:
    Esp0fWork work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp0f_Create()
{
    return new cEsp0f;
}

void cEsp0f::move()
{
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

extern "C" void Esp0f_Trans(cEsp0f* esp)
{
    static Mtx Matrix = {
        { 0.001953125f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0029762f, -0.167f, 0.0f },
        { 0.0f, 0.0f, 1.0f, 0.0f },
    };
    Esp0fWork* w = &esp->work;
    Mtx44 proj;
    Mtx inv;
    EspAnmData* anm;
    GXColor fog;
    void* buf;
    f32 sx;
    f32 sy;
    f32 ox;
    f32 oy;
    f32 x0;
    f32 y0;
    f32 z;
    f32 zero;
    f32 s0;
    f32 s1;
    f32 t0;
    f32 t1;

    if (!EspGetAnmAddr(esp->anmNo, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", esp->anmNo);
        return;
    }
    CameraCurrentProjection();
    if ((s8)esp->partsNo >= -8 && (s8)esp->partsNo <= -3) {
        PSMTXIdentity(esp->mat);
        RotMatrix(esp->mat, &esp->rot);
        TransMatrix(esp->mat, &esp->pos);
        C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
        GXSetProjection(proj, 1);
    } else if (!(esp->flags & 1)) {
        Vec p;
        Mtx m;

        PSMTXIdentity(esp->mat);
        PSMTXRotRad(esp->mat, 'z', esp->rot.z);
        PSMTXConcat(pG->Cam.viewMat, esp->parent->mat, m);
        PSMTXMultVec(m, &esp->pos, &p);
        esp->mat[0][3] = p.x;
        esp->mat[1][3] = p.y;
        esp->mat[2][3] = p.z;
    } else {
        Mtx m;

        PSMTXIdentity(esp->mat);
        RotMatrix(esp->mat, &esp->rot);
        TransMatrix(esp->mat, &esp->pos);
        PSMTXConcat(pG->Cam.viewMat, esp->parent->mat, m);
        PSMTXConcat(m, esp->mat, esp->mat);
    }
    GXTexObj tex;
    PSMTXInverse(esp->mat, inv);
    PSMTXTranspose(inv, inv);
    GXLoadNrmMtxImm(inv, 0);
    GXLoadPosMtxImm(esp->mat, 0);
    GXSetCurrentMtx(0);
    EspTexSet(esp->anmNo, esp->anmPtn);
    esp->ChannelSet();
    GXSetBlendMode(esp->xA4, esp->xA5, esp->xA6, esp->xA7);
    esp->CommonStateSet();
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xA, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xA, 0, 1, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    sx = esp->sizeX * esp->scale;
    sy = esp->sizeY * esp->scale;
    ox = -anm->x4;
    oy = (f32)anm->x6;
    z = 1.0f;
    zero = 0.0f;
    if (ox == zero) {
        ox = -anm->x0 * 0.5f;
    }
    if (oy == zero) {
        oy = anm->x2 * 0.5f;
    }
    x0 = ox * sx / anm->x0;
    y0 = oy * sy / anm->x2;
    ESP_SPRITE_CORNERS(esp, zero, z, s0, s1, t0, t1)
    fog.r = fog.g = fog.b = fog.a = 0;
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, fog);
    buf = GetDrawTmpBufAddr(3);
    if (buf == NULL) {
        pLog->warn(0, 0, "Esp0d() : not enough memory");
        return;
    }
    GXSetTexCopySrc(0, 0, (u32)Screen.width, (u32)Screen.height);
    GXSetTexCopyDst((u32)Screen.width / 2, (u32)Screen.height / 2, 6, 1);
    GXCopyTex(buf, 0);
    GXPixModeSync();
    GXInvalidateTexAll();
    GXInitTexObj(&tex, buf, (u32)Screen.width / 2, (u32)Screen.height / 2, 6, 0, 0, 0);
    GXInitTexObjLOD(&tex, 1, 1, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXLoadTexObj(&tex, 1);
    if ((s8)esp->partsNo >= -8 && (s8)esp->partsNo <= -3) {
        Mtx tm;

        PSMTXConcat(Matrix, esp->mat, tm);
        GXLoadTexMtxImm(tm, 0x1E, 1);
        GXSetTexCoordGen(0, 1, 0, 0x1E);
    } else {
        Mtx tm;
        Mtx pm;

        C_MTXLightPerspective(pm, pG->Cam.param.fovy, 1.3333334f, 0.5f, -0.5f, 0.5f, 0.5f);
        PSMTXConcat(pm, esp->mat, tm);
        GXLoadTexMtxImm(tm, 0x1E, 0);
        GXSetTexCoordGen(0, 0, 0, 0x1E);
    }
    GXSetTevOrder(0, 0, 1, 4);
    GXSetTevColorIn(0, 0xF, 0xF, 0xF, 8);
    switch (w->power) {
    case 0:
        GXSetTevColorOp(0, 0, 0, 0, 1, 0);
        break;
    case 1:
        GXSetTevColorOp(0, 0, 0, 1, 1, 0);
        break;
    case 2:
        GXSetTevColorOp(0, 0, 0, 2, 1, 0);
        break;
    }
    GXSetTevAlphaIn(0, 7, 7, 7, 5);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    GXSetTevOrder(1, 0xFF, 0xFF, 4);
    GXSetTevColorIn(1, 0xA, 0xF, 0, 0xF);
    GXSetTevColorOp(1, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(1, 7, 7, 7, 5);
    GXSetTevAlphaOp(1, 0, 0, 0, 1, 0);
    GXSetNumTexGens(2);
    GXSetTexCoordGen(1, 1, 4, 0x3C);
    GXSetNumTevStages(2);
    GXSetTevOrder(2, 1, 0, 4);
    GXSetTevColorIn(2, 0xF, 0xF, 0xF, 0);
    GXSetTevColorOp(2, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(2, 7, 0, 4, 7);
    GXSetTevAlphaOp(2, 0, 0, 0, 1, 0);
    GXSetNumTevStages(3);
    GXBegin(0x80, 0, 4);
    GXPosition3f32(x0, y0, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s0, t0);
    GXPosition3f32(x0 + sx, y0, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s1, t0);
    GXPosition3f32(x0 + sx, y0 - sy, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s1, t1);
    GXPosition3f32(x0, y0 - sy, z);
    GXNormal3s8(0, 1, 0);
    GXTexCoord2f32(s0, t1);
    GXSetNumTevStages(1);
    GXSetNumTexGens(0);
    GXSetNumIndStages(0);
    GXSetTevDirect(0);
    GXSetTevDirect(1);
    GXSetAlphaUpdate(0);
    LightMgr.setFog();
}

int cEsp0f::SetFreeWork(EspGenWork* gen, u32* seed)
{
    work.power = gen->xC8;
    if (work.power > 2) {
        pLog->err(0, 0, "ESP_0F : Power[%d] invalid", work.power);
        return 0;
    }
    return 1;
}
