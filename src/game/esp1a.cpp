#include "atari.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"

struct Esp1aWork {
    Vec ofs;  // 0x00 camera-relative jitter applied this frame
    Vec prm;  // 0x0C x: sideways jitter, y: vertical jitter, z: distance toward the camera
};

// Jittering sprite spawned in a random cone around a model part (esp0b-style camera jitter).
class cEsp1a : public cEsp {
public:
    Esp1aWork work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" void get_angle(Vec* v, f32* rx, f32* ry);

cEsp* Esp1a_Create()
{
    return new cEsp1a;
}

void cEsp1a::move()
{
    Esp1aWork* w = &work;
    Vec look;
    Vec up;
    Vec side;
    Vec tmp;
    Vec wpos;
    Mtx inv;
    Camera* cam;

    PSVECSubtract(&pos, &w->ofs, &pos);
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else {
            cam = &pG->Cam;
            if (parent != pEffParentWorld) {
                PSMTXMultVec(parent->mat, &pos, &wpos);
            } else {
                wpos = pos;
            }
            PSVECSubtract(&wpos, &cam->param.pos, &look);
#line 84 "D:/Bio4/Prog/esp1a.cpp"
            VECNormalize(&look, &look);
            CameraGetUpVec(cam, &up);
            PSVECCrossProduct(&look, &up, &side);
            PSVECScale(&look, &w->ofs, -w->prm.z);
            PSVECScale(&side, &tmp, w->prm.x * fRand1_1());
            PSVECAdd(&w->ofs, &tmp, &w->ofs);
            PSVECScale(&up, &tmp, w->prm.y * fRand1_1());
            PSVECAdd(&w->ofs, &tmp, &w->ofs);
            if (parent != pEffParentWorld) {
                PSMTXInverse(parent->mat, inv);
                PSMTXMultVecSR(inv, &w->ofs, &w->ofs);
            }
            PSVECAdd(&pos, &w->ofs, &pos);
        }
    }
}

// Rotation angles (around x then y) that turn +z onto `v`.
void get_angle(Vec* v, f32* rx, f32* ry)
{
    Vec t;
    Mtx m;

    if (v->x == 0.0f && v->z == 0.0f) {
        *ry = 0.0f;
    } else {
        *ry = atan2f(v->x, v->z);
    }
    PSMTXRotRad(m, 'y', -*ry);
    PSMTXMultVec(m, v, &t);
    if (t.y == 0.0f && t.z == 0.0f) {
        *rx = 0.0f;
    } else {
        *rx = -atan2f(t.y, t.z);
    }
}

int cEsp1a::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp1aWork* w = &work;

    if (parent != pEffParentWorld && (parentCnt == 0xFF || parentCnt <= cnt)) {
        cModel* parts;

        if ((s8)gen->xC8 >= pModel->nParts) {
            pLog->err(0, 0, "ESP1a : Wk0 PartsNo > %d ", pModel->nParts);
            return 0;
        }
        parts = pModel->getPartsPtr((s8)gen->xC8);
        pos = *(Vec*)&gen->x0C;
        {
            Vec dir = { 0.0f, 0.01f, 0.0f };
            Vec sc;
            Mtx inv;
            Vec rot;
            Mtx rm;
            Vec off;
            Mtx m2;
            f32 a;
            f32 len;
            f32 lo;
            f32 hi;
            f32 t;

            PSMTXMultVec(parts->mat, &dir, &dir);
            PSMTXInverse(parent->mat, inv);
            PSMTXMultVec(inv, &dir, &dir);
            get_angle(&dir, &rot.x, &rot.y);
            rot.z = 0.0f;
            RotMatrix(rm, &rot);
            a = fRandSeed0_1(seed) * 2.0f * PI;
            off.x = SINF(a) * gen->x20 * fRandSeed0_1(seed);
            off.y = COSF(a) * gen->x20 * fRandSeed0_1(seed);
            off.z = 0.0f;
            PSMTXMultVec(rm, &off, &off);
            PSVECAdd(&pos, &off, &pos);
            len = PSVECMag(&dir);
            lo = gen->x18 / len;
            hi = gen->x1C / len;
            t = 1.0f - lo + hi;
            PSVECScale(&dir, &sc, fRandSeed0_1(seed) * t + lo);
            PSVECAdd(&pos, &sc, &pos);
            PSMTXMultVec(parent->mat, &pos, &pos);
            parts = pModel->getPartsPtr(partsNo);
            PSMTXIdentity(m2);
            low_RotMatrix(m2, &pModel->rot);
            PSMTXMultVecSR(m2, &spd, &spd);
            PSMTXMultVecSR(m2, &acc, &acc);
            pModel = NULL;
            parent = pEffParentWorld;
        }
    } else {
        pLog->err(0, 0, "ESP1a : no parent!!");
        return 0;
    }
    w->prm = *(Vec*)&gen->xD8;
    return 1;
}
