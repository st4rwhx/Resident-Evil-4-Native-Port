#include "atari.h"
#include "light.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"

#define ESP02_STRIP_NUM 1
#define ESP_STRIP_PTS_MAX 16

struct Esp02Work {
    Mtx mat;   // 0x00 parent matrix at the time the sprite left its parent
    Vec pos0;  // 0x30 local position
};

// Single-segment camera-facing strip (a stretched sprite from pos along -x).
class cEsp02 : public cEsp {
public:
    Esp02Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" {
void EspStrip02_setup(cEsp02* esp);
void esp02Trans_sub(cEsp02* esp);
}

cEsp* Esp02_Create()
{
    return new cEsp02;
}

void cEsp02::move()
{
    Esp02Work* w = &work;

    if (parent != pEffParentWorld && parentCnt != 0xFF && parentCnt <= cnt) {
        PSMTXCopy(parent->mat, w->mat);
        parent = pEffParentWorld;
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
        PSMTXIdentity(mat);
        RotMatrix(mat, &rot);
        TransMatrix(mat, &w->pos0);
        PSMTXConcat(w->mat, mat, mat);
        PSMTXMultVec(mat, &w->pos0, &pos);
    }
}

extern "C" void Esp02_Trans(cEsp02* esp)
{
    EspStrip02_setup(esp);
    esp02Trans_sub(esp);
}

void EspStrip02_setup(cEsp02* esp)
{
    Esp02Work* w = &esp->work;
    Mtx id;
    Mtx m;

    CameraCurrentProjection();
    if ((s8)esp->partsNo >= -8 && (s8)esp->partsNo <= -3) {
        pLog->err(0, 0, "EspStrip_Trans():SCREEN MODE is invalid.");
        PushEsp(esp);
        return;
    }
    PSMTXIdentity(esp->mat);
    RotMatrix(esp->mat, &esp->rot);
    TransMatrix(esp->mat, &w->pos0);
    PSMTXConcat(w->mat, esp->mat, esp->mat);
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

void esp02Trans_sub(cEsp02* esp)
{
    Vec dir;
    Vec org;
    Vec pts[ESP_STRIP_PTS_MAX];
    Vec d;
    Vec tmp;
    Vec cross;
    Vec q[2];
    Vec v[4];
    Vec n2;
    f32 half;
    f32 nz;
    u32 i;

    dir.x = -esp->sizeX;
    dir.y = 0.0f;
    dir.z = 0.0f;
    PSMTXMultVecSR(esp->mat, &dir, &dir);
    org.x = 0.0f;
    org.y = 0.0f;
    org.z = 0.0f;
    PSMTXMultVec(esp->mat, &org, &org);
    pts[0] = org;
    pts[0].x += dir.x;
    pts[0].y += dir.y;
    pts[0].z += dir.z;
    pts[1] = org;
    for (i = 0; i < ESP02_STRIP_NUM; i++) {
        PSVECSubtract(&pts[i + 1], &pts[i], &d);
        tmp = pts[i];
        PSVECCrossProduct(&d, &tmp, &cross);
        if (cross.x == 0.0f && cross.y == 0.0f && cross.z == 0.0f) {
            continue;
        }
#line 243 "D:/Bio4/Prog/esp02.cpp"
        VECNormalize(&cross, &cross);
        half = esp->sizeY * 0.5f;
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
#line 267 "D:/Bio4/Prog/esp02.cpp"
        VECNormalize(&d, &n2);
        nz = n2.z;
        if (nz < 0.0f) {
            nz = -nz;
        }
        nz = nz * nz;
        nz = nz * nz;
        nz = nz * nz;
        nz = 1.0f - nz;
        {
            GXColor c;

            c.r = (u8)esp->colR;
            c.g = (u8)esp->colG;
            c.b = (u8)esp->colB;
            c.a = (u8)(esp->colA * nz);
            GXSetChanMatColor(4, c);
        }
        EspStrip_draw_poly(esp, i, v, 1, 1);
    }
}

int cEsp02::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp02Work* w = &work;

    w->pos0 = pos;
    PSMTXIdentity(w->mat);
    if ((s8)partsNo >= -8 && (s8)partsNo <= -3) {
        pLog->err(0, 0, "EspStrip_Trans():SCREEN MODE is invalid.");
        return 0;
    }
    return 1;
}
