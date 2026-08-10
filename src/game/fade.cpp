#include "types.h"
#include "vec.h"
#include "gx.h"
#include "main_sub.h"
#include "main_mem.h"
#include "fade.h"

FadeWork Fade[4];

void FadeSet(int no, GXColor* start, GXColor* end, u32 time, u32 z, int late)
{
    FadeWork* f = &Fade[no & 0x7FFFFFFF];
    GXColor s = *start;
    GXColor e = *end;

    if (no < 0) {
        f->flags = 1;
    } else {
        f->flags = 3;
    }
    if (late) {
        f->flags |= 4;
    }
    f->start = s;
    f->x1A = 0;
    f->end = e;
    f->time = time;
    f->count = 0;
    f->z = (f32) z;
    f->cur = s;
}

void FadeKillAll()
{
    u32 i;

    for (i = 0; i < 4; i++) {
        FadeKill(i);
    }
}

void FadeKill(int no)
{
    Fade[no].flags = 0;
}

void FadeInit()
{
    int i;
    FadeWork* f = Fade;

    memclr_asm(f, sizeof(Fade));
    for (i = 0; i < 4; i++, f++) {
        f->z = 0.0f;
    }
}

void FadeControl(int late)
{
    int i;
    FadeWork* f;

    for (i = 0; i < 4; i++) {
        f = &Fade[i];
        if (late == 0) {
            if (f->flags & 4) {
                continue;
            }
        } else {
            if (!(f->flags & 4)) {
                continue;
            }
        }
        if (f->flags & 1) {
            if (f->count < f->time) {
                u32 rem = f->time - f->count;

                f->cur.r = (f->start.r * rem + f->end.r * f->count) / f->time;
                f->cur.g = (f->start.g * rem + f->end.g * f->count) / f->time;
                f->cur.b = (f->start.b * rem + f->end.b * f->count) / f->time;
                f->cur.a = (f->start.a * rem + f->end.a * f->count) / f->time;
                f->count++;
            } else {
                f->cur = f->end;
                if (f->flags & 2) {
                    f->flags &= ~1;
                } else {
                    f->flags = 0;
                }
            }
        }
        if (f->flags & 3) {
            fadeDraw(f);
        }
    }
}

void fadeDraw(FadeWork* f)
{
    GXRenderModeObj* rmode = &Rmode;
    Mtx44 proj;
    Mtx mv;

    GXSetBlendMode(1, 4, 5, 0);
    GXSetColorUpdate(1);
    C_MTXOrtho(proj, 0.0f, (f32) rmode->xfbHeight, 0.0f, (f32) rmode->fbWidth, 0.0f, -10000.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mv);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    GXSetCullMode(0);
    GXSetZMode(1, 7, 0);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOp(0, 4);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 1, 0, 2);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3f32(0.0f, 56.0f, f->z);
    GXColor4u8(f->cur.r, f->cur.g, f->cur.b, f->cur.a);
    GXPosition3f32((f32) rmode->fbWidth, 56.0f, f->z);
    GXColor4u8(f->cur.r, f->cur.g, f->cur.b, f->cur.a);
    GXPosition3f32((f32) rmode->fbWidth, rmode->xfbHeight - 56.0f + 1.0f, f->z);
    GXColor4u8(f->cur.r, f->cur.g, f->cur.b, f->cur.a);
    GXPosition3f32(0.0f, rmode->xfbHeight - 56.0f + 1.0f, f->z);
    GXColor4u8(f->cur.r, f->cur.g, f->cur.b, f->cur.a);
}
