#include "atari.h"
#include "light.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"

struct Esp06Work {
    u8 pathNo;    // 0x00 (gen->xC9)
    u8 flags;     // 0x01 bit0: loop, bit1: stop at the end, bit2: stopped, bit7: has matrix (gen->xCA)
    u16 pathId;   // 0x02 (gen->xC8)
    u16 seg;      // 0x04 current path segment (PathGetPos reads/writes a halfword)
    u8 pad_6[2];
    void* path;   // 0x08
    f32 dist;     // 0x0C distance along the path
    Vec ofs;      // 0x10 base position
    Mtx mat;      // 0x1C rotation / scale applied to the path
    f32 spd;      // 0x4C
    f32 acc;      // 0x50
    u8 waitBase;  // 0x54 frames to wait at a loop restart (gen->xFC)
    u8 waitRnd;   // 0x55 random addition to waitBase (gen->xFD)
    u8 wait;      // 0x56
};

// Path follower: moves the sprite along an effect path (loops / stops / dies at the end).
class cEsp06 : public cEsp {
public:
    Esp06Work work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" {
int Esp06GetPathPos(cEsp06* esp);
void esp06_CommonMove(cEsp06* esp);
void esp06_Move00(cEsp06* esp);
void esp06_Move01(cEsp06* esp);
}

static void (*Esp06MoveTbl[])(cEsp06*) = { esp06_Move00, esp06_Move01 };

cEsp* Esp06_Create()
{
    return new cEsp06;
}

int Esp06GetPathPos(cEsp06* esp)
{
    Esp06Work* w = &esp->work;
    int ret;

    if (PathHasWeight(w->path)) {
        if (esp->pModel != NULL) {
            ret = PathGetPosEm(w->path, w->dist, esp->pModel, &w->seg, &esp->pos);
        } else {
            ret = PathGetPos(w->path, w->dist, &w->seg, &esp->pos);
        }
    } else {
        ret = PathGetPos(w->path, w->dist, &w->seg, &esp->pos);
    }
    return ret;
}

void esp06_CommonMove(cEsp06* esp)
{
    Esp06Work* w = &esp->work;
    Mtx m;

    if (esp->parent != pEffParentWorld && esp->parentCnt != 0xFF && esp->parentCnt <= esp->cnt) {
        PSMTXMultVecSR(esp->parent->mat, &w->ofs, &w->ofs);
        PSMTXMultVecSR(esp->parent->mat, &esp->spd, &esp->spd);
        PSMTXMultVecSR(esp->parent->mat, &esp->acc, &esp->acc);
        if (esp->flags & 1) {
            RotMatrix(m, &esp->rot);
            PSMTXConcat(esp->parent->mat, m, m);
            Matrix2AxisAngle(m, &esp->rot);
        }
        PSMTXConcat(esp->parent->mat, w->mat, w->mat);
        w->flags |= 0x80;
        esp->parent = pEffParentWorld;
    }
    if (esp->spdCnt <= esp->cnt) {
        PSVECAdd(&w->ofs, &esp->spd, &w->ofs);
        w->spd += w->acc;
        PSVECAdd(&esp->spd, &esp->acc, &esp->spd);
        PSVECScale(&esp->spd, &esp->spd, esp->spdScale);
    }
    if (esp->scaleCnt <= esp->cnt) {
        esp->scale += esp->scaleSpd;
        esp->scaleSpd *= esp->scaleScale;
        if (esp->scale <= 0.0f) {
            PushEsp(esp);
            return;
        }
    }
    PSVECAdd(&esp->rot, &esp->rotSpd, &esp->rot);
    if (esp->ColorUpdate()) {
        if (esp->life != 0 && esp->life <= esp->cnt) {
            PushEsp(esp);
            return;
        }
        esp->cnt++;
        if (!esp->AnmMove()) {
            PushEsp(esp);
            return;
        }
        {
            if (w->wait == 0) {
                w->dist += w->spd;
            } else {
                w->wait--;
            }
            if (!Esp06GetPathPos(esp)) {
                if (w->flags & 1) {
                    if (w->spd > 0.0f) {
                        w->dist -= PathGetLength(w->path);
                    } else {
                        w->dist += PathGetLength(w->path);
                    }
                    Esp06GetPathPos(esp);
                    w->wait = w->waitBase;
                    if (w->waitRnd != 0) {
                        w->waitBase += (u32)Rnd() % w->waitRnd;
                    }
                } else if (w->flags & 2) {
                    if (w->spd > 0.0f) {
                        w->dist = PathGetLength(w->path) - 0.1f;
                    } else {
                        w->dist = 0.0f;
                    }
                    Esp06GetPathPos(esp);
                    w->flags |= 4;
                } else {
                    PushEsp(esp);
                    return;
                }
            }
            if (w->flags & 0x80) {
                PSMTXMultVec(w->mat, &esp->pos, &esp->pos);
            }
            PSVECAdd(&esp->pos, &w->ofs, &esp->pos);
        }
    }
}

void esp06_Move00(cEsp06* esp)
{
    esp06_CommonMove(esp);
    esp->x10 = 1;
}

void esp06_Move01(cEsp06* esp)
{
    esp06_CommonMove(esp);
}

void cEsp06::move()
{
    Esp06MoveTbl[x10](this);
}

int cEsp06::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp06Work* w = &work;
    f32 t;

    w->pathId = gen->xC8;
    w->pathNo = gen->xC9;
    w->flags = gen->xCA;
    w->spd = (f32)(s32)gen->prm.w.xCC;
    w->acc = (f32)(s32)gen->prm.w.xD0 * 0.1f;
    w->spd += (f32)(s32)gen->xD4 * fRandSeed1_1(seed);
    w->waitBase = gen->xFC;
    w->waitRnd = gen->xFD;
    if ((f32)(s32)gen->prm.w.xCC != 0.0f) {
        if ((f32)(s32)gen->prm.w.xCC > 0.0f) {
            if (w->spd < 0.0f) {
                w->spd = -w->spd;
            }
        } else {
            if (w->spd > 0.0f) {
                w->spd = -w->spd;
            }
        }
    }
    w->path = EspGetPathAddr(w->pathId, w->pathNo);
    if (w->path == NULL) {
        return 0;
    }
    w->ofs = pos;
    if (gen->xE4 != 0.0f || gen->xE8 != 0.0f || gen->xEC != 0.0f) {
        Vec r;

        r = *(Vec*)&gen->xE4;
        w->flags |= 0x80;
        PSVECScale(&r, &r, 0.017453292f);
        RotMatrix(w->mat, &r);
    } else {
        PSMTXIdentity(w->mat);
    }
    if (gen->xD8 != 0.0f || gen->xDC != 0.0f || gen->xE0 != 0.0f) {
        Vec s;
        Mtx sm;

        s = *(Vec*)&gen->xD8;
        w->flags |= 0x80;
        PSVECScale(&s, &s, 0.1f);
        s.x += 1.0f;
        s.y += 1.0f;
        s.z += 1.0f;
        PSMTXScale(sm, s.x, s.y, s.z);
        PSMTXConcat(w->mat, sm, w->mat);
    }
    if (gen->xF0 != 0.0f || gen->xF4 != 0.0f) {
        t = gen->xF0 * 0.01f;
        t += gen->xF4 * 0.01f * fRandSeed0_1(seed);
        if (t > 1.0f) {
            t -= (f32)(u32)t;
        }
        if (t < 0.0f) {
            t += (f32)(u32)(-t) + 1.0f;
        }
        w->dist = t * PathGetLength(w->path);
    } else if (w->spd < 0.0f) {
        w->dist = PathGetLength(w->path);
    }
    return 1;
}
