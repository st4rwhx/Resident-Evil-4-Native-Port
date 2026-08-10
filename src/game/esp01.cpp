#include "atari.h"
#include "light.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"

#define ESP_STRIP_PTS_MAX 16

struct Esp01Work {
    u16 nSeg;      // 0x00 number of strip segments (15 - gen->xC8, clamped)
    u16 interval;  // 0x02 frames between two trail points (gen->xC9)
    u32 x4;        // 0x04
    Vec pos0;      // 0x08 position at the time the sprite left its parent
};

// Motion trail strip: replays the speed/acceleration backwards to get the last positions and
// draws them as a textured strip.
class cEsp01 : public cEsp {
public:
    Esp01Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" {
void EspStrip01_setup(cEsp01* esp);
void esp01Trans_sub(cEsp01* esp);
}

cEsp* Esp01_Create()
{
    return new cEsp01;
}

void cEsp01::move()
{
    Esp01Work* w = &work;

    if (parent != pEffParentWorld && parentCnt != 0xFF && parentCnt <= cnt) {
        ApplyMatrix(parent->mat);
        w->pos0 = pos;
        parent = pEffParentWorldS;
    }
    if (scaleCnt <= cnt) {
        scale += scaleSpd;
        scaleSpd *= scaleScale;
        if (scale <= 0.0f) {
            PushEsp(this);
            return;
        }
    }
    if (ColorUpdate()) {
        if (life != 0 && life <= cnt) {
            PushEsp(this);
            return;
        }
        cnt++;
        if (!AnmMove()) {
            PushEsp(this);
            return;
        }
        xB8 = 100000000.0f;
        dispFlag |= 2;
    }
}

extern "C" void Esp01_Trans(cEsp01* esp)
{
    EspStrip01_setup(esp);
    esp01Trans_sub(esp);
}

void EspStrip01_setup(cEsp01* esp)
{
    Esp01Work* w = &esp->work;
    Mtx id;
    Mtx m;

    CameraCurrentProjection();
    if ((s8)esp->partsNo >= -8 && (s8)esp->partsNo <= -3) {
        pLog->err(0, 0, "ESP_STRIP : SCREEN MODE is invalid.");
        PushEsp(esp);
        return;
    }
    PSMTXIdentity(esp->mat);
    RotMatrix(esp->mat, &esp->rot);
    TransMatrix(esp->mat, &w->pos0);
    PSMTXConcat(pG->Cam.viewMat, esp->parent->mat, m);
    PSMTXConcat(m, esp->mat, esp->mat);
    PSMTXIdentity(id);
    GXLoadPosMtxImm(id, 0);
    GXSetCurrentMtx(0);
    EspTexSet(esp->anmNo, esp->anmPtn);
    esp->ChannelSet();
    GXSetBlendMode(esp->xA4, esp->xA5, esp->xA6, esp->xA7);
    esp->CommonStateSet();
    GXClearVtxDesc();
    GXSetVtxDesc(0, 1);
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
}

void esp01Trans_sub(cEsp01* esp)
{
    static Vec tmp_poss[48];
    static int tmp_n[ESP_STRIP_PTS_MAX];
    Esp01Work* w = &esp->work;
    Vec spd;
    Vec acc;
    Vec org;
    Vec pts[ESP_STRIP_PTS_MAX];
    Vec s;
    int max;
    int min;
    int i;
    int j;

    PSMTXMultVecSR(esp->mat, &esp->spd, &spd);
    PSMTXMultVecSR(esp->mat, &esp->acc, &acc);
    org.x = 0.0f;
    org.y = 0.0f;
    org.z = 0.0f;
    PSMTXMultVec(esp->mat, &org, &org);
    max = 0;
    min = 999999;
    for (i = 0; i < w->nSeg + 1; i++) {
        int n;

        n = esp->cnt - i * (w->interval + 1);
        if (n < 0) {
            n = 0;
        }
        tmp_n[i] = n;
        if (n > max) {
            max = n;
        }
        if (n < min) {
            min = n;
        }
    }
    if (max - min <= 47) {
        Vec p;

        p = org;
        tmp_poss[0] = p;
        s = spd;
        for (j = 1; j <= max; j++) {
            p.x += s.x;
            p.y += s.y;
            p.z += s.z;
            if (j >= min) {
                tmp_poss[j - min] = p;
            }
            s.x += acc.x;
            s.y += acc.y;
            s.z += acc.z;
            PSVECScale(&s, &s, esp->spdScale);
        }
        for (i = 0; i < w->nSeg + 1; i++) {
            pts[i] = tmp_poss[tmp_n[i] - min];
        }
    } else {
        for (i = 0; i < w->nSeg + 1; i++) {
            Vec s2;
            int n;

            s2 = spd;
            n = esp->cnt - i * (w->interval + 1);
            pts[i] = org;
            if (n < 0) {
                n = 0;
            }
            for (j = 0; j < n; j++) {
                pts[i].x += s2.x;
                pts[i].y += s2.y;
                pts[i].z += s2.z;
                s2.x += acc.x;
                s2.y += acc.y;
                s2.z += acc.z;
                PSVECScale(&s2, &s2, esp->spdScale);
            }
        }
    }
    for (i = 0; i < w->nSeg; i++) {
        Vec d;
        Vec tmp;
        Vec cross;
        Vec q[2];
        Vec v[4];
        f32 rate;
        f32 half;

        PSVECSubtract(&pts[i + 1], &pts[i], &d);
        tmp = pts[i];
        PSVECCrossProduct(&d, &tmp, &cross);
        if (cross.x == 0.0f && cross.y == 0.0f && cross.z == 0.0f) {
            continue;
        }
#line 379 "D:/Bio4/Prog/esp01.cpp"
        VECNormalize(&cross, &cross);
        rate = (f32)i / (f32)(w->nSeg - 1);
        half = (rate * esp->sizeY + (1.0f - rate) * esp->sizeX) * esp->scale;
        PSVECScale(&cross, &q[0], half);
        PSVECScale(&cross, &q[1], -half);
        if (i == 0) {
            PSVECAdd(&pts[i], &q[0], &v[0]);
            PSVECAdd(&pts[i], &q[1], &v[1]);
        } else {
            v[0] = v[2];
            v[1] = v[3];
        }
        PSVECAdd(&pts[i + 1], &q[0], &v[2]);
        PSVECAdd(&pts[i + 1], &q[1], &v[3]);
        EspStrip_draw_poly(esp, i, v, w->nSeg, 0);
    }
}

void EspStrip_draw_poly(cEsp* esp, int no, Vec* v, u8 texRepeat, int flag)
{
    EspAnmData* anm;
    f32 s;
    f32 t;
    f32 sw;
    f32 tw;

    if (!EspGetAnmAddr(esp->anmNo, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", esp->anmNo);
        return;
    }
    if (flag) {
        if (esp->flags & 4) {
            sw = 1.0f / (f32)texRepeat;
            tw = 1.0f;
            s = 1.0f - 1.0f / texRepeat * no;
            t = 0.0f;
        } else {
            sw = 1.0f / (f32)texRepeat;
            tw = 1.0f;
            s = 1.0f / texRepeat * no;
            t = 0.0f;
        }
    } else {
        if (esp->flags & 4) {
            sw = 1.0f;
            tw = 1.0f / (f32)texRepeat;
            s = 0.0f;
            t = 1.0f - 1.0f / texRepeat * no;
        } else {
            sw = 1.0f;
            tw = 1.0f / (f32)texRepeat;
            s = 0.0f;
            t = 1.0f / texRepeat * no;
        }
    }
    GXBegin(0x80, 0, 4);
    if (flag) {
        if (esp->flags & 2) {
            if (esp->flags & 4) {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s + sw, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s + sw, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32(s, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s, t);
            } else {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s + sw, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s + sw, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32(s, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s, t + tw);
            }
        } else {
            if (esp->flags & 4) {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32((f32)(s16)(s + sw), t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s + sw, t + tw);
            } else {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32(s + sw, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s + sw, t);
            }
        }
    } else {
        if (esp->flags & 2) {
            if (esp->flags & 4) {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s + sw, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32(s, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s + sw, t);
            } else {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s + sw, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32(s, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s + sw, t + tw);
            }
        } else {
            if (esp->flags & 4) {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s + sw, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32(s + sw, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s, t);
            } else {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s + sw, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32(s + sw, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s, t + tw);
            }
        }
    }
}

int cEsp01::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp01Work* w = &work;

    w->pos0 = pos;
    w->nSeg = (s8)gen->xC8;
    w->interval = (s8)gen->xC9;
    if (w->nSeg > 12) {
        w->nSeg = 2;
    } else {
        w->nSeg = 15 - w->nSeg;
    }
    if (w->nSeg > 15) {
        w->nSeg = 15;
    }
    return 1;
}
