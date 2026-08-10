// game/cinesco: cinema-scope letterbox bars.
#include "types.h"
#include "global.h"
#include "vec.h"
#include "gx.h"
#include "view.h"
#include "main_mem.h"
#include "cinesco.h"

CineWork cine_work;

extern "C" {
void cine_polling(CineWork* w);
void cine_on_move(CineWork* w);
void cine_off_move(CineWork* w);
}

void CinescoMove(void)
{
    static void (*cine_tbl[])(CineWork*) = {
        cine_polling,
        cine_on_move,
        cine_off_move,
    };

    pG->flags_500C |= 0x1000000;
    cine_tbl[cine_work.mode](&cine_work);
}

void cine_polling(CineWork* w)
{
    int on;

    if (!(pG->flags_500C & 0x1000000)) {
        on = 0;
    } else {
        on = 1;
    }
    if (w->on != on) {
        w->on = on;
        if (on) {
            w->mode = 1;
            w->timer = 15.0f;
        } else {
            w->mode = 2;
            w->timer = 15.0f;
        }
    }
}

void cine_on_move(CineWork* w)
{
    w->timer -= 1.0f;
    w->alpha = (u8) ((15.0f - w->timer) / 15.0f * 255.0f);
    if (w->timer <= 0.0f) {
        w->mode = 0;
        w->alpha = 255;
    }
}

void cine_off_move(CineWork* w)
{
    w->timer -= 1.0f;
    w->alpha = (u8) (w->timer / 15.0f * 255.0f);
    if (w->timer <= 0.0f) {
        w->mode = 0;
        w->alpha = 0;
    }
}

void Draw_cinesco(void)
{
    Mtx44 proj;
    Mtx mv;
    GXColor fog;
    u8 a = cine_work.alpha;

    if (a == 0) {
        return;
    }
    GXSetBlendMode(1, 4, 5, 0);
    GXSetColorUpdate(1);
    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mv);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    fog.r = fog.g = fog.b = fog.a = 0;
    GXSetFog(0, 0.0f, 0.0f, ZNEAR, ZFAR, fog);
    GXSetCullMode(0);
    GXSetZMode(0, 7, 1);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOp(0, 4);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 1, 0, 2);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
    GXBegin(0x80, 0, 8);
    GXPosition3s16(0, 0, 10);
    GXColor4u8(0, 0, 0, a);
    GXPosition3s16(512, 0, 10);
    GXColor4u8(0, 0, 0, a);
    GXPosition3s16(512, 56, 10);
    GXColor4u8(0, 0, 0, a);
    GXPosition3s16(0, 56, 10);
    GXColor4u8(0, 0, 0, a);
    GXPosition3s16(0, 393, 10);
    GXColor4u8(0, 0, 0, a);
    GXPosition3s16(512, 393, 10);
    GXColor4u8(0, 0, 0, a);
    GXPosition3s16(512, 449, 10);
    GXColor4u8(0, 0, 0, a);
    GXPosition3s16(0, 449, 10);
    GXColor4u8(0, 0, 0, a);
}

void CinescoInit(void)
{
    memclr_asm(&cine_work, sizeof(CineWork));
}
