#include "atari.h"
#include "global.h"
#include "esp.h"
#include "main_sub.h"
#include "filter.h"
#include "cam_ctrl.h"
#include "gx.h"

extern f32 ZNEAR;
extern f32 ZFAR;

struct Esp45Work {
    Vec wpos;       // 0x00 world position
    f32 sx;         // 0x0C screen position x
    f32 sy;         // 0x10 screen position y
    u8 type;        // 0x14 gen->xC8: Filter00 spread type
    u8 alpha;       // 0x15 colour alpha as a byte
    u8 rate;        // 0x16 gen->xC2
    u8 pad_17;
    f32 power;      // 0x18 scaleSpd: spread power
    f32 sz;         // 0x1C view depth
    Vec scrOld;     // 0x20 previous screen position (z: view depth)
    f32 hideAlpha;  // 0x2C alpha from the Z-buffer visibility test
    f32 hideR;      // 0x30 radius of the visibility test (gen->xE4)
    u8 pad_34[4];
    u16 flags;      // 0x38 bit1: visibility test
    u16 hideCnt;    // 0x3A
    f32 dist;       // 0x3C camera distance where the glow is gone (gen->xE0)
};

// Additive radial blur (Filter00 spread) at the projected position, faded by camera distance and
// a Z-buffer visibility test.
class cEsp45 : public cEsp {
public:
    Esp45Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" {
cEsp* Esp45_Create();
void Esp45_Trans(cEsp* esp);
static f32 GetDistAlpha(cEsp45* esp);
void Esp45_HideCheck(cEsp* esp);
}

cEsp* Esp45_Create()
{
    return new cEsp45;
}

void cEsp45::move()
{
    Esp45Work* w = &work;

    scale = 1.0e22f;
    if (!CommonMove()) {
        return;
    }
    w->alpha = (u8) colA;
    w->power = scaleSpd;
    if (flag & 1) {
        Vec v;

        PSMTXMultVec(pG->Cam.viewMat, &w->wpos, &v);
        w->sz = v.z;
        EspAddOtAfterRender(this, Esp45_HideCheck);
    }
}

void Esp45_Trans(cEsp* esp0)
{
    cEsp45* esp = (cEsp45*) esp0;
    Esp45Work* w = &esp->work;

    if (esp->partsNo >= 0xF8 && esp->partsNo <= 0xFD) {
        f32 cx = esp->pos.x * 0.001953125f - 0.5f;
        f32 cy = esp->pos.y * 0.001953125f - 0.5f;
        Filter00SetAddSpread(w->type, 1, (u8) esp->colR, (u8) esp->colG, (u8) esp->colB, w->alpha, w->rate, 1,
                             cx, cy, w->power);
    } else {
        Vec view;
        Vec scr;
        f32 cx;
        f32 cy;
        int a;
        f32 sx;
        f32 sy;

        if (esp->parent == pEffParentWorld) {
            w->wpos = esp->pos;
        } else {
            if (esp->partsNo >= esp->pModel->nParts) {
                pLog->err(0, 0, "ESP45 :PARTS_NO[%d] is invalid(MAX:%d).", esp->partsNo, esp->pModel->nParts);
                PushEsp(esp);
                return;
            }
            PSMTXMultVec(esp->pModel->getPartsPtr(esp->partsNo)->mat, &esp->pos, &w->wpos);
        }
        PSMTXMultVec(pG->Cam.viewMat, &w->wpos, &view);
        PSMTX44MultVec(pG->Cam.projMat, &view, &scr);
        scr.z = 0.0f;
        cx = scr.x * 0.5f;
        cy = scr.y * -0.5f;
        a = w->alpha;
        if (w->flags & 2) {
            a = (u8) ((f32) a * w->hideAlpha);
        }
        a = (u8) ((f32) a * GetDistAlpha(esp));
        Filter00SetAddSpread(w->type, 1, (u8) esp->colR, (u8) esp->colG, (u8) esp->colB, a, w->rate, 1, cx, cy,
                             w->power);
        PSMTX44MultVec(pG->Cam.projMat, &view, &scr);
        sx = (scr.x * 0.5f + 0.5f) * Screen.width;
        sy = (-scr.y * 0.5f + 0.5f) * Screen.height;
        scr.z = 0.0f;
        w->scrOld.x = w->sx;
        w->scrOld.y = w->sy;
        w->scrOld.z = w->sz;
        scr.x = sx;
        scr.y = sy;
        w->sx = sx;
        w->sy = sy;
    }
}

// Alpha from the distance to the camera: 1 at the camera, 0 at `dist`.
static f32 GetDistAlpha(cEsp45* esp)
{
    Esp45Work* w = &esp->work;
    Vec d;
    f32 a;

    if (w->dist != 0.0f) {
        Camera* cam = &pG->Cam;

        d.x = w->wpos.x - cam->param.pos.x;
        d.y = w->wpos.y - cam->param.pos.y;
        d.z = w->wpos.z - cam->param.pos.z;
        a = PSVECMag(&d) / w->dist;
        if (a > 1.0f) {
            a = 1.0f;
        }
        if (a < 0.0f) {
            a = 0.0f;
        }
        return 1.0f - a;
    }
    return 1.0f;
}

// Z-buffer visibility test around the screen position: hidden samples fade the glow out.
void Esp45_HideCheck(cEsp* esp0)
{
    static f32 Zscale = 1.0f;
    static f32 Zoffset = 1.0f;
    static s32 Zs_bias45 = 0;
    static const f32 hide_x_tbl[12] = { 0.0f, 0.5f, 0.86f, 1.0f, 0.86f, 0.5f, 0.0f, -0.5f, -0.86f, -1.0f, -0.86f, -0.5f };
    static const f32 hide_y_tbl[12] = { 1.0f, 0.86f, 0.5f, 0.0f, -0.5f, -0.86f, -1.0f, -0.86f, -0.5f, 0.0f, 0.5f, 0.86f };
    static s32 Zs_bias45_2 = 0;  // unreferenced 4-byte .sdata word after Zs_bias45 (name unknown)
    cEsp45* esp = (cEsp45*) esp0;
    Esp45Work* w = &esp->work;
    Vec p;
    u32 z;
    s32 zi;
    f32 nz;
    f32 inv;
    f32 inv2;
    f32 m22;
    f32 m23;
    f32 zv;
    f32 margin;
    f32 scale;
    u32 hidden;
    u32 i;

    if (!(w->flags & 2)) {
        return;
    }
    nz = w->scrOld.z + 150.0f;
    inv = 1.0f / (ZFAR - ZNEAR);
    inv2 = 1.0f / -nz;
    m22 = -(ZNEAR) * inv;
    m23 = -(ZFAR * ZNEAR) * inv;
    zv = (m23 + m22 * nz) * Zscale;
    zi = (u32) ((inv2 * zv + Zoffset) * 16777215.0f);
    if (pG->flags_54 & 0x800) {
        margin = 56.0f;
    } else {
        margin = 0.0f;
    }
    GXPixModeSync();
    GXDrawDone();
    hidden = 0;
    scale = 5000.0f / nz;
    for (i = 0; i < 24; i++) {
        f32 ox;
        f32 oy;

        ox = hide_x_tbl[i] * w->hideR;
        oy = hide_y_tbl[i] * w->hideR;
        if (scale < 1.0f) {
            ox *= scale;
            ox *= scale;
        }
        p.x = w->scrOld.x + ox;
        p.y = w->scrOld.y + oy;
        if (p.x < 0.0f || p.x >= Screen.width || p.y < 0.0f + margin || p.y >= Screen.height - margin) {
            hidden++;
        } else {
            GXPeekZ((u16) p.x, (u16) p.y, &z);
            if (zi > (s32) (z - Zs_bias45)) {
                hidden++;
            }
        }
        // COMPILER-DIFF: candidate (loop.c pass-1 insn_count). Dead test (+3 real insns: the store
        // goes at flow, compare/branch at jump2): the original's loop had >= 60 real insns at loop
        // pass 1, so `high(Screen)` (savings 1, life 1, threshold 71 - 3 per moved movable = 59)
        // was not hoisted until pass 2 and its `lis` lands AFTER pass 1's giv init `li i4,0`.
        if (w->flags == 99) {
            ox = oy;
        }
    }
    if (hidden == 24) {
        w->hideAlpha = 0.0f;
    } else {
        f32 a;

        a = 2.4f - (f32) hidden * 0.1f;
        if (a < 0.0f) {
            a = 0.0f;
        }
        if (a > 1.0f) {
            a = 1.0f;
        }
        w->hideAlpha = w->hideAlpha + (a - w->hideAlpha) * 0.6f;
    }
    if (CamCtrl.IsChangeCamera()) {
        w->hideCnt = 2;
    }
    if (w->hideCnt != 0) {
        w->hideCnt--;
        w->hideAlpha = 0.0f;
    }
}

int cEsp45::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp45Work* w = &work;

    w->type = gen->xC8;
    w->rate = gen->xC2;
    w->alpha = (u8) colA;
    w->power = scaleSpd;
    w->dist = gen->xE0;
    if (gen->xE4 != 0.0f) {
        w->hideR = gen->xE4;
        w->flags |= 2;
    }
    return 1;
}
