#include "atari.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"
#include "main_sub.h"

extern f32 ZNEAR;
extern f32 ZFAR;

struct Esp0eWork {
    Vec wpos;       // 0x00 world position
    Vec dir;        // 0x0C facing direction (world)
    f32 angle;      // 0x18 half angle of the visible cone (rad)
    f32 distRate;   // 0x1C 1 - gen->xD8 / 100: screen-centre fade factor
    f32 sizeRate;   // 0x20 gen->xDC / 100: how much the alpha scales the size
    f32 dist;       // 0x24 camera distance where the glow is gone (gen->xE0)
    Vec scr;        // 0x28 screen position (z: view depth)
    Vec scrOld;     // 0x34 previous screen position
    f32 hideAlpha;  // 0x40 alpha from the Z-buffer visibility test
    f32 hideR;      // 0x44 radius of the visibility test (gen->xE4)
    f32 alpha;      // 0x48 final alpha
    u16 flags;      // 0x4C bit0: direction test, bit1: visibility test
    u16 hideCnt;    // 0x4E frames the visibility test is forced to 0
    u32 seed;       // 0x50 random seed for the screen jitter
    EspGenWork* gen;  // 0x54
};

// Screen-space glow (lens flare style): the sprite is drawn in screen mode at the projected
// position, faded by distance from the screen centre, the facing direction and a Z-buffer test.
class cEsp0e : public cEsp {
public:
    Esp0eWork work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" {
static f32 GetDistAlpha(cEsp0e* esp);
static f32 GetDirAlpha(cEsp0e* esp, Vec* dir);
void Esp0e_HideCheck(cEsp* esp);
}

cEsp* Esp0e_Create()
{
    return new cEsp0e;
}

void cEsp0e::move()
{
    Esp0eWork* w = &work;
    Vec dir;
    Vec view;
    Vec scr;
    Vec d;
    f32 alpha;
    f32 sx;
    f32 sy;

    if (!CommonMove()) {
        return;
    }
    if (!AnmMove()) {
        PushEsp(this);
        return;
    }
    if (w->flags & 2) {
        if (pG->flags_5014 & 0x10000000) {
            w->hideCnt = 2;
        }
        if (w->hideCnt != 0) {
            w->hideCnt--;
            w->hideAlpha = 0.0f;
        }
    }
    if (parent == pEffParentWorld) {
        w->wpos = pos;
        dir = w->dir;
    } else {
        cModel* parts;

        if (partsNo >= pModel->nParts) {
            pLog->err(0, 0, "ESP0E :PARTS_NO[%d] is invalid(MAX:%d).", partsNo, pModel->nParts);
            PushEsp(this);
            return;
        }
        parts = pModel->getPartsPtr(partsNo);
        PSMTXMultVec(parts->mat, &pos, &w->wpos);
        {
            Mtx m;

            PSMTXCopy(parts->mat, m);
            m[0][3] = 0.0f;
            m[1][3] = 0.0f;
            m[2][3] = 0.0f;
            PSMTXMultVec(m, &w->dir, &dir);
        }
    }
    PSMTXMultVec(pG->Cam.viewMat, &w->wpos, &view);
    PSMTX44MultVec(pG->Cam.projMat, &view, &scr);
    sx = (scr.x * 0.5f + 0.5f) * Screen.width;
    sy = (-scr.y * 0.5f + 0.5f) * Screen.height;
    w->scrOld.x = w->scr.x;
    w->scrOld.y = w->scr.y;
    w->scrOld.z = w->scr.z;
    scr.x = sx;
    scr.y = sy;
    scr.z = 0.0f;
    w->scr.x = sx;
    w->scr.y = sy;
    if (view.z < 0.0f) {
        view.x = Screen.width * 0.5f;
        view.y = Screen.height * 0.5f;
        view.z = 0.0f;
        PSVECSubtract(&view, &scr, &d);
        alpha = PSVECMag(&d) / (Screen.height * (w->distRate * 0.7f));
        alpha *= alpha;
        alpha = 1.0f - alpha;
        if (w->flags & 1) {
            alpha *= GetDirAlpha(this, &dir);
        }
        alpha *= GetDistAlpha(this);
        if (w->flags & 2) {
            alpha *= w->hideAlpha;
        }
        w->alpha = alpha;
    } else {
        w->alpha = 0.0f;
    }
    if (flag & 1) {
        Vec v;

        PSMTXMultVec(pG->Cam.viewMat, &w->wpos, &v);
        w->scr.z = v.z;
        EspAddOtAfterRender(this, Esp0e_HideCheck);
    }
}

extern "C" void Esp0e_Trans(cEsp0e* esp)
{
    Esp0eWork* w = &esp->work;

    if (w->alpha > 0.01f) {
        cEsp tmp;
        cEsp* p = &tmp;
        Mtx m;
        PSMTXIdentity(m);
        *p = *esp;
        p->id = 0;
        p->pModel = NULL;
        p->partsNo = 0xF8;
        p->life = 1;
        p->pos.x = w->scr.x + w->gen->x18 * fRandSeed1_1(&w->seed);
        p->pos.y = w->scr.y + w->gen->x1C * fRandSeed1_1(&w->seed);
        p->pos.z = 1.0f;
        p->colA *= w->alpha;
        if (w->sizeRate != 0.0f) {
            f32 s;

            s = w->alpha * w->sizeRate + (1.0f - w->sizeRate);
            if (s < 0.0f) {
                s = 0.0f;
            }
            p->sizeX *= s;
            p->sizeY *= s;
        }
        EspCommonTrans(p);
    }
}

// Alpha from the distance to the camera: 1 at the camera, 0 at `dist`.
static f32 GetDistAlpha(cEsp0e* esp)
{
    Esp0eWork* w = &esp->work;
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

// Alpha from the angle between the facing direction and the camera: 1 when looking straight at
// the camera, 0 at the cone edge.
static f32 GetDirAlpha(cEsp0e* esp, Vec* dir)
{
    Esp0eWork* w = &esp->work;
    Camera* cam;
    Vec d;
    f32 ang;
    f32 c;
    f32 a;

    ang = LIMIT_ANGLE(w->angle);
    cam = &pG->Cam;
    d.x = w->wpos.x - cam->param.pos.x;
    d.y = w->wpos.y - cam->param.pos.y;
    d.z = w->wpos.z - cam->param.pos.z;
#line 295 "D:/Bio4/Prog/esp0e.cpp"
    VECNormalize(&d, &d);
    a = -PSVECDotProduct(&d, dir);
    c = cosf(ang);
    a -= c;
    if (a <= 0.0f) {
        a = 0.0f;
    } else {
        a /= 1.0f - c;
    }
    return a;
}

// Z-buffer visibility test around the screen position: hidden samples fade the glow out.
void Esp0e_HideCheck(cEsp* esp0)
{
    static f32 Zscale = 1.0f;
    static f32 Zoffset = 1.0f;
    static s32 Zs_bias0e = 0;
    static const f32 hide_x_tbl[12] = { 0.0f, 0.5f, 0.86f, 1.0f, 0.86f, 0.5f, 0.0f, -0.5f, -0.86f, -1.0f, -0.86f, -0.5f };
    static const f32 hide_y_tbl[12] = { 1.0f, 0.86f, 0.5f, 0.0f, -0.5f, -0.86f, -1.0f, -0.86f, -0.5f, 0.0f, 0.5f, 0.86f };
    static s32 Zs_bias0e_2 = 0;  // unreferenced 4-byte .sdata word after Zs_bias0e (name unknown)
    cEsp0e* esp = (cEsp0e*)esp0;
    Esp0eWork* w = &esp->work;
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

    if (!(esp->flag & 1)) {
        return;
    }
    if (!(w->flags & 2)) {
        return;
    }
    nz = w->scrOld.z + 150.0f;
    inv = 1.0f / (ZFAR - ZNEAR);
    inv2 = 1.0f / -nz;
    m22 = -(ZNEAR) * inv;
    m23 = -(ZFAR * ZNEAR) * inv;
    zv = (m23 + m22 * nz) * Zscale;
    // The result is written back into inv2: the 1.0 constant, inv2 and the final value are one
    // register chain (f12) with the most refs, so it is allocated first and -ZNEAR/inv take
    // f11/f10 (a separate result variable ties the fmadds to zv instead).
    inv2 = inv2 * zv + Zoffset;
    zi = (u32)(inv2 * 16777215.0f);
    if (pG->flags_54 & 0x800) {
        margin = 56.0f;
    } else {
        margin = 0.0f;
    }
    GXPixModeSync();
    GXDrawDone();
    hidden = 0;
    scale = 5000.0f / nz;
    for (i = 0; i < 12; i++) {
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
            GXPeekZ((u16)p.x, (u16)p.y, &z);
            if (zi > (s32)(z - Zs_bias0e)) {
                hidden++;
            }
        }
        // COMPILER-DIFF: candidate (loop.c pass-1 insn_count). Dead test (+3 real insns at loop
        // pass 1) so `high(Screen)` misses pass 1's threshold and is hoisted in pass 2, after the
        // giv init `li i4,0` (see esp45 Esp45_HideCheck); the operand must not add a ref to `w`.
        if (Zs_bias0e == 99) {
            ox = oy;
        }
    }
    if (hidden == 12) {
        w->hideAlpha = 0.0f;
    } else {
        f32 a;

        a = 1.0f - (f32)hidden * 0.1f;
        if (a < 0.0f) {
            a = 0.0f;
        }
        if (a > 1.0f) {
            a = 1.0f;
        }
        w->hideAlpha = w->hideAlpha + (a - w->hideAlpha) * 0.6f;
    }
}

int cEsp0e::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp0eWork* w = &work;

    dispFlag |= 8;
    w->flags = 0;
    if (gen->xF8 != 0.0f) {
        Mtx mx;
        Mtx my;
        f32 rx;
        f32 ry;

        w->angle = gen->xF8 * PI * 2.0f / 360.0f * 0.5f;
        w->dir.x = 0.0f;
        w->dir.y = 0.0f;
        w->dir.z = 1.0f;
        rx = gen->xF0 * PI * 2.0f / 360.0f;
        ry = gen->xF4 * PI * 2.0f / 360.0f;
        rx = LIMIT_ANGLE(rx);
        ry = LIMIT_ANGLE(ry);
        PSMTXRotRad(mx, 'Y', ry);
        PSMTXRotRad(my, 'X', rx);
        PSMTXConcat(mx, my, mx);
        PSMTXMultVec(mx, &w->dir, &w->dir);
#line 442 "D:/Bio4/Prog/esp0e.cpp"
        VECNormalize(&w->dir, &w->dir);
        w->flags |= 1;
    }
    w->distRate = 1.0f - gen->xD8 * 0.01f;
    if (w->distRate > 1.0f) {
        w->distRate = 1.0f;
    }
    w->sizeRate = gen->xDC * 0.01f;
    w->dist = gen->xE0;
    if (gen->xE4 != 0.0f) {
        w->hideR = gen->xE4;
        w->flags |= 2;
    }
    w->seed = 0x12345678;
    w->gen = gen;
    if (partsNo != 0xFE && parentCnt == 0) {
        parentCnt = 0xFF;
    }
    return 1;
}
