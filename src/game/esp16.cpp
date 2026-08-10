#include "atari.h"
#include "light.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"

struct Esp16Work {
    u32 nPt;        // 0x00 number of chain points (gen->xC8 + 2)
    cEsp3f* pos;    // 0x04 point positions
    cEsp3f* spd;    // 0x08 point speeds
    Vec grav;       // 0x0C acceleration added every frame (gen->xE4..)
    Vec rnd;        // 0x18 random jitter amplitude (gen->xF0..)
    f32 spdRate;    // 0x24 how much of the constraint correction feeds back into the speed (gen->xE0 / 100)
    f32 damp;       // 0x28 speed damping (gen->xDC / 100)
    f32 len;        // 0x2C segment length (gen->xD8)
    cModel* parts;  // 0x30 model part the far end is attached to
};

// Rope / chain: a string of points held together by distance constraints, drawn as a textured
// strip facing the camera. The head follows the effect position, the tail can be attached to
// a model part.
class cEsp16 : public cEsp {
public:
    Esp16Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
    virtual void Destruct();
};

cEsp* Esp16_Create()
{
    return new cEsp16;
}

void cEsp16::move()
{
    Esp16Work* w = &work;
    Vec pos0;
    Vec d;
    Vec nrm;
    Vec* p;
    Vec* s;
    u32 i;

    if (!CommonMove()) {
        return;
    }
    if (!AnmMove()) {
        PushEsp(this);
        return;
    }
    FSet(xB8, 100000000.0f);
    if (parent == pEffParentWorld) {
        pos0 = pos;
    } else {
        PSMTXMultVec(parent->mat, &pos, &pos0);
    }
    for (i = 1; i < w->nPt; i++) {
        p = Esp3f_GetVecPtr(w->pos, i);
        s = Esp3f_GetVecPtr(w->spd, i);
        PSVECAdd(p, s, Esp3f_GetVecPtr(w->pos, i));
        PSVECSubtract(Esp3f_GetVecPtr(w->pos, i - 1), Esp3f_GetVecPtr(w->pos, i), &d);
        if (PSVECMag(&d) > w->len) {
            static f32 nen_mul = 1.0f;

#line 63 "D:/Bio4/Prog/esp16.cpp"
            VECNormalize(&d, &nrm);
            PSVECScale(&nrm, &d, -w->len);
            PSVECAdd(Esp3f_GetVecPtr(w->pos, i - 1), &d, Esp3f_GetVecPtr(w->pos, i));
            PSVECScale(&nrm, &d, -PSVECDotProduct(&nrm, Esp3f_GetVecPtr(w->spd, i)) * nen_mul);
            PSVECAdd(Esp3f_GetVecPtr(w->spd, i), &d, Esp3f_GetVecPtr(w->spd, i));
        }
        {
            Vec jit;

            jit.x = w->rnd.x * fRand1_1();
            jit.y = w->rnd.y * fRand1_1();
            jit.z = w->rnd.z * fRand1_1();
            PSVECAdd(Esp3f_GetVecPtr(w->pos, i), &jit, Esp3f_GetVecPtr(w->pos, i));
        }
        PSVECScale(&d, &d, w->spdRate);
        PSVECAdd(Esp3f_GetVecPtr(w->spd, i), &d, Esp3f_GetVecPtr(w->spd, i));
        PSVECSubtract(Esp3f_GetVecPtr(w->spd, i - 1), &d, Esp3f_GetVecPtr(w->spd, i - 1));
        PSVECAdd(Esp3f_GetVecPtr(w->spd, i), &w->grav, Esp3f_GetVecPtr(w->spd, i));
        PSVECScale(Esp3f_GetVecPtr(w->spd, i), Esp3f_GetVecPtr(w->spd, i), w->damp);
    }
    *Esp3f_GetVecPtr(w->pos, 0) = pos0;
    if (w->parts != NULL) {
        Vec v = { 0.0f, 0.01f, 0.0f };

        PSMTXMultVec(w->parts->mat, &v, &v);
        *Esp3f_GetVecPtr(w->pos, w->nPt - 1) = v;
        for (i = w->nPt - 2; i > 1; i--) {
            static f32 nen_mul = 1.0f;

            p = Esp3f_GetVecPtr(w->pos, i);
            s = Esp3f_GetVecPtr(w->spd, i);
            PSVECAdd(p, s, Esp3f_GetVecPtr(w->pos, i));
            PSVECSubtract(Esp3f_GetVecPtr(w->pos, i + 1), Esp3f_GetVecPtr(w->pos, i), &d);
            if (PSVECMag(&d) > w->len) {
#line 109 "D:/Bio4/Prog/esp16.cpp"
                VECNormalize(&d, &nrm);
                PSVECScale(&nrm, &d, -w->len);
                PSVECAdd(Esp3f_GetVecPtr(w->pos, i + 1), &d, Esp3f_GetVecPtr(w->pos, i));
                PSVECScale(&nrm, &d, -PSVECDotProduct(&nrm, Esp3f_GetVecPtr(w->spd, i)) * nen_mul);
                PSVECAdd(Esp3f_GetVecPtr(w->spd, i), &d, Esp3f_GetVecPtr(w->spd, i));
            }
            {
                Vec jit;

                jit.x = w->rnd.x * fRand1_1();
                jit.y = w->rnd.y * fRand1_1();
                jit.z = w->rnd.z * fRand1_1();
                PSVECAdd(Esp3f_GetVecPtr(w->pos, i), &jit, Esp3f_GetVecPtr(w->pos, i));
            }
            PSVECScale(&d, &d, w->spdRate);
            PSVECAdd(Esp3f_GetVecPtr(w->spd, i), &d, Esp3f_GetVecPtr(w->spd, i));
            PSVECSubtract(Esp3f_GetVecPtr(w->spd, i + 1), &d, Esp3f_GetVecPtr(w->spd, i + 1));
            PSVECAdd(Esp3f_GetVecPtr(w->spd, i), &w->grav, Esp3f_GetVecPtr(w->spd, i));
            PSVECScale(Esp3f_GetVecPtr(w->spd, i), Esp3f_GetVecPtr(w->spd, i), w->damp);
        }
    }
}

extern "C" void Esp16_Trans(cEsp16* esp)
{
    Esp16Work* w = &esp->work;
    Mtx inv;
    Vec cam;
    Vec q0;
    Vec q1;
    Vec dir;
    Vec toCam;
    Vec nrm;
    Vec cross;
    EspAnmData* anm;
    Vec* p0;
    Vec* p1;
    u32 n;
    u32 i;
    f32 t;
    f32 tw;
    f32 s0;
    f32 s1;
    f32 rate;
    f32 half;
    register f32 z asm("fr12"); // COMPILER-DIFF: #13

    if (esp->cnt < w->nPt) {
        n = esp->cnt + 1;
    } else {
        n = w->nPt;
    }
    if (!EspGetAnmAddr(esp->anmNo, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", esp->anmNo);
        return;
    }
    CameraCurrentProjection();
    EspTexSet(esp->anmNo, esp->anmPtn);
    esp->ChannelSet();
    GXSetBlendMode(esp->xA4, esp->xA5, esp->xA6, esp->xA7);
    esp->CommonStateSet();
    // The original sets t through an intermediate the copy never absorbed (`lfs f12, 0.0; fmr
    // f29, f12`) and its block ends right after the zero load: `lbz partsNo`, the 1.0 high, the
    // copy and `lfs 1.0` are scheduled as a second block (`lis; lfs f12 | lbz; lis; fmr; lfs`).
    // The dead test below is that block boundary (compare/branch gone at flow/jump2; sched1 and
    // sched2 both run with the blocks split); with the boundary combine cannot merge the load
    // into the copy either, so no keep-alive is needed, and the zero dying at the copy ranks
    // `fmr` above `lfs 1.0`. The pinned f12 is the register the original's zero took. The test's
    // three loads (three short local qtys: r0, r9, r11 before the zero high's qty) put the high
    // in r10 like the target; a one-load compare leaves it r9.
    z = 0.0f;                   // COMPILER-DIFF: #13
    // Dead in the original too: only its 0x43300000 constant survives, shared through the cse
    // path by both `(f32) w->nPt` conversions below (`lis r31, 0x4330` right after
    // CameraCurrentProjection, `stw r31` in both arms). A signed conversion: the arms reload
    // their unsigned magic double separately. Which expression it was is unknown. It must stay
    // in the first block so its constant is hoisted to the top.
    rate = (f32)(int)n;
    if (esp->cnt + w->nPt + esp->anmNo == 99) { // COMPILER-DIFF: candidate (sched block split)
        t = z;
    }
    t = z;
    tw = 1.0f;
    if ((s8)esp->partsNo >= -8 && (s8)esp->partsNo <= -3) {
        pLog->err(0, 0, "ESP_16 : Parent is screen.");
        return;
    }
    PSMTXIdentity(esp->mat);
    RotMatrix(esp->mat, &esp->rot);
    PSMTXConcat(pG->Cam.viewMat, esp->mat, esp->mat);
    PSMTXInverse(esp->mat, inv);
    PSMTXTranspose(inv, inv);
    GXLoadNrmMtxImm(inv, 0);
    GXLoadPosMtxImm(esp->mat, 0);
    GXSetCurrentMtx(0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xA, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xA, 0, 1, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    p1 = NULL;
    cam = pG->Cam.param.pos;
    if (esp->flags & 4) {
        t = tw;
        tw = -1.0f / (f32)w->nPt;
    } else {
        tw = tw / (f32)w->nPt;
    }
    if (esp->flags & 2) {
        s0 = 1.0f;
        s1 = 0.0f;
    } else {
        s0 = 0.0f;
        s1 = 1.0f;
    }
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    GXBegin(0x98, 0, n * 2);
    p0 = Esp3f_GetVecPtr(w->pos, 0);
    for (i = 0; i < n; i++) {
        if (i != n - 1) {
            p1 = Esp3f_GetVecPtr(w->pos, i + 1);
            PSVECSubtract(p1, p0, &dir);
        }
        PSVECSubtract(p0, &cam, &toCam);
        PSVECCrossProduct(&dir, &toCam, &cross);
        if (cross.x == 0.0f && cross.y == 0.0f && cross.z == 0.0f) {
        } else {
#line 306 "D:/Bio4/Prog/esp16.cpp"
            VECNormalize(&cross, &nrm);
        }
        rate = (f32)i / (f32)(n - 1);
        half = (rate * esp->sizeY + (1.0f - rate) * esp->sizeX) * esp->scale;
        PSVECScale(&nrm, &q0, half);
        PSVECScale(&nrm, &q1, -half);
        PSVECAdd(&q0, p0, &q0);
        PSVECAdd(&q1, p0, &q1);
        p0 = p1;
        GXPosition3f32(q0.x, q0.y, q0.z);
        GXNormal3s8(0, 1, 0);
        GXTexCoord2f32(s0, t);
        t += tw;
        GXPosition3f32(q1.x, q1.y, q1.z);
        GXNormal3s8(0, 1, 0);
        GXTexCoord2f32(s1, t);
    }
}

void cEsp16::Destruct()
{
    Esp16Work* w = &work;
    cEsp* b;

    b = (cEsp*)w->pos;
    if (b != NULL && (b->flag & 1)) {
        PushEsp(b);
    }
    b = (cEsp*)w->spd;
    if (b != NULL && (b->flag & 1)) {
        PushEsp(b);
    }
}

int cEsp16::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp16Work* w = &work;
    Vec p;
    Vec z;
    int i;

    BitSet(w->nPt, (u8)(gen->xC8 + 2));
    if (parent != pEffParentWorld && (parentCnt == 0xFF || parentCnt <= cnt)) {
        s8 no = gen->xC9;

        if (no != 0) {
            if ((u32)(no - 1) >= pModel->nParts) {
                pLog->err(0, 0, "ESP16 : Wk1 PartsNo > %d ", pModel->nParts);
                return 0;
            }
            w->parts = pModel->getPartsPtr(no - 1);
        }
    }
    w->grav = *(Vec*)&gen->xE4;
    w->len = gen->xD8;
    w->damp = gen->xDC * 0.01f;
    w->spdRate = gen->xE0 * 0.01f;
    w->rnd = *(Vec*)&gen->xF0;
    if (!Esp3f_Alloc(sizeof(Vec), w->nPt, &w->pos, &info) || !Esp3f_Alloc(sizeof(Vec), w->nPt, &w->spd, &info)) {
        pLog->warn(0, 0, "ESP_16 : Buf alloc failed.");
        return 0;
    }
    FSet(xB8, 100000000.0f);
    BitOn16(dispFlag, 2);
    z.z = 0.0f;
    z.y = 0.0f;
    z.x = 0.0f;
    if (parent == pEffParentWorld) {
        p = pos;
    } else {
        PSMTXMultVec(parent->mat, &pos, &p);
    }
    for (i = w->nPt - 1; i >= 0; i--) {
        *Esp3f_GetVecPtr(w->pos, i) = p;
        *Esp3f_GetVecPtr(w->spd, i) = z;
    }
    return 1;
}
