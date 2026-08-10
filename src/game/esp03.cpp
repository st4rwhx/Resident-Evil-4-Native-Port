#include "atari.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"

struct Esp03Work {
    s8 n;         // 0x00 number of trail points (2..6), 10: single camera-facing quad
    u8 hitWall;   // 0x01 bit0: stop at walls (gen->xC9)
    u8 pad_2[7];
    u8 idx;       // 0x09 ring buffer index of the next point
    u16 width;    // 0x0A line width
    Vec* cur;     // 0x0C current point
    Vec pts[6];   // 0x10 position history (only 4 are used)
};

// Line trail: keeps the last positions in a ring buffer and draws them as a line strip.
class cEsp03 : public cEsp {
public:
    Esp03Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" void Esp03_HitWall(cEsp03* esp);

cEsp* Esp03_Create()
{
    return new cEsp03;
}

void cEsp03::move()
{
    Esp03Work* w = &work;
    Vec* p;

    if (parent != pEffParentWorld && parentCnt != 0xFF && parentCnt <= cnt) {
        ApplyMatrix(parent->mat);
        parent = pEffParentWorld;
    }
    if (w->hitWall & 1) {
        Esp03_HitWall(this);
    }
    if (spdCnt <= cnt) {
        p = &w->pts[w->idx];
        PSVECAdd(w->cur, &spd, p);
        PSVECAdd(&spd, &acc, &spd);
        PSVECScale(&spd, &spd, spdScale);
        w->cur = p;
    }
    if (scaleCnt <= cnt) {
        scale += scaleSpd;
        scaleSpd *= scaleScale;
        if (scale <= 0.0f) {
            PushEsp(this);
            return;
        }
    }
    w->width = (u16)(sizeX * scale / (100.0f / 3.0f));
    if (ColorUpdate()) {
        if (life != 0 && life <= cnt) {
            PushEsp(this);
            return;
        }
        cnt++;
        w->idx = (w->idx + 1) & 3;
    }
}

extern "C" void Esp03_Trans(cEsp03* esp)
{
    Esp03Work* w = &esp->work;
    Vec* p;
    Vec* v;
    int idx;
    int i;

    GXSetZMode(1, 3, 0);
    GXSetCullMode(0);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    CameraCurrentProjection();
    if ((s8)esp->partsNo >= -8 && (s8)esp->partsNo <= -3) {
        pLog->err(0, 0, "ESP_03 : SCREEN MODE is invalid.");
        PushEsp(esp);
        return;
    }
    {
        Mtx m;

        PSMTXIdentity(esp->mat);
        RotMatrix(esp->mat, &esp->rot);
        TransMatrix(esp->mat, &esp->pos);
        PSMTXConcat(pG->Cam.viewMat, esp->parent->mat, m);
        PSMTXConcat(m, esp->mat, esp->mat);
    }
    GXLoadPosMtxImm(esp->mat, 0);
    GXSetCurrentMtx(0);
    esp->ChannelSet();
    GXSetTevOp(0, 4);
    GXSetAlphaCompare(4, 1, 1, 4, 1);
    GXSetBlendMode(esp->xA4, esp->xA5, esp->xA6, esp->xA7);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    if (w->n == 10) {
        Vec d;
        Vec up;
        Vec q[4];

        p = &w->pts[(w->idx - 1) & 3];
        v = q;
#line 187 "D:/Bio4/Prog/esp03.cpp"
        VECNormalize(&pG->Cam.up, &up);
        PSVECScale(&up, &up, esp->sizeX * 0.5f);
        PSVECSubtract(p, &pG->Cam.param.pos, &d);
        PSVECCrossProduct(&up, &d, &d);
#line 193 "D:/Bio4/Prog/esp03.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, esp->sizeX * 0.5f);
        PSVECAdd(p, &d, &q[0]);
        PSVECAdd(p, &up, &q[1]);
        PSVECSubtract(p, &d, &q[2]);
        PSVECSubtract(p, &up, &q[3]);
        GXBegin(0x80, 0, 4);
        GXPosition3f32(v->x, v->y, v->z);
        GXColor4u8((u8)esp->colR, (u8)esp->colG, (u8)esp->colB, (u8)esp->colA);
        v++;
        GXPosition3f32(v->x, v->y, v->z);
        GXColor4u8((u8)esp->colR, (u8)esp->colG, (u8)esp->colB, (u8)esp->colA);
        v++;
        GXPosition3f32(v->x, v->y, v->z);
        GXColor4u8((u8)esp->colR, (u8)esp->colG, (u8)esp->colB, (u8)esp->colA);
        v++;
        GXPosition3f32(v->x, v->y, v->z);
        GXColor4u8((u8)esp->colR, (u8)esp->colG, (u8)esp->colB, (u8)esp->colA);
    } else {
        idx = w->idx;
        GXSetLineWidth((u8)w->width, 0);
        GXBegin(0xB0, 0, (u16)w->n);
        for (i = 0; i < w->n; i++) {
            idx--;
            idx &= 3;
            p = &w->pts[idx];
            GXPosition3f32(p->x, p->y, p->z);
            GXColor4u8((u8)esp->colR, (u8)esp->colG, (u8)esp->colB, (u8)esp->colA);
        }
        GXSetLineWidth(6, 0);
    }
}

int cEsp03::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp03Work* w = &work;
    int n;

    w->cur = &w->pts[0];
    w->idx = 1;
    n = (s8)gen->xC8;
    if (n == 10) {
        w->n = n;
    } else {
        w->n = 4 - gen->xC8;
        if (w->n <= 1) {
            w->n = 2;
        } else if (w->n > 6) {
            w->n = 6;
        }
    }
    w->hitWall = gen->xC9;
    if (w->hitWall > 1) {
        pLog->err(0, 0, "ESP_03 : WK1 invalid.");
        return 0;
    }
    xB8 = 100000000.0f;
    dispFlag |= 2;
    return 1;
}

void Esp03_HitWall(cEsp03* esp)
{
    Esp03Work* w = &esp->work;
    Vec hit;
    Vec next2;
    Vec nrm;
    Vec next;

    PSVECAdd(w->cur, &esp->pos, &next);
    PSVECAdd(&next, &esp->spd, &next2);
    if (EatMgr.hitCheck(&next, &next2, &hit, &nrm, 0, 0)) {
        esp->life = 1;
        esp->cnt = 1;
    }
}
