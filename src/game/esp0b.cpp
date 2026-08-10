#include "atari.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"

struct Esp0bWork {
    Vec ofs;  // 0x00 offset applied to the position this frame
    Vec prm;  // 0x0C x: sideways jitter, y: vertical jitter, z: distance toward the camera
};

// Camera-relative jitter: every frame the sprite is moved by a random offset in the camera's
// side/up plane (and toward the camera by prm.z). With info bit 0x8000 set the offset is
// applied only while drawing.
class cEsp0b : public cEsp {
public:
    Esp0bWork work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

cEsp* Esp0b_Create()
{
    return new cEsp0b;
}

void cEsp0b::move()
{
    Esp0bWork* w = &work;
    Vec look;
    Vec up;
    Vec side;
    Vec tmp;
    Vec wpos;
    Mtx inv;
    Camera* cam;

    if (!(info.x0 & 0x8000)) {
        PSVECSubtract(&pos, &w->ofs, &pos);
    }
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else if (!(info.x0 & 0x8000)) {
            cam = &pG->Cam;
            if (parent != pEffParentWorld) {
                PSMTXMultVec(parent->mat, &pos, &wpos);
            } else {
                wpos = pos;
            }
            PSVECSubtract(&wpos, &cam->param.pos, &look);
            if (look.x == 0.0f && look.y == 0.0f && look.z == 0.0f) {
                pLog->warn(0, 0, "Esp0b : look vec is ZERO");
                look.x = look.y = look.z = 0.0f;
            } else {
#line 101 "D:/Bio4/Prog/esp0b.cpp"
                VECNormalize(&look, &look);
            }
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

extern "C" void Esp0b_Trans(cEsp0b* esp)
{
    Vec look;
    Vec up;
    Vec side;
    Vec tmp;
    Vec wpos;
    Mtx inv;
    Camera* cam;

    if (esp->info.x0 & 0x8000) {
        Esp0bWork* w = &esp->work;
        cam = &pG->Cam;
        if (esp->parent != pEffParentWorld) {
            PSMTXMultVec(esp->parent->mat, &esp->pos, &wpos);
        } else {
            wpos = esp->pos;
        }
        PSVECSubtract(&wpos, &cam->param.pos, &look);
        if (look.x == 0.0f && look.y == 0.0f && look.z == 0.0f) {
            pLog->warn(0, 0, "Esp0b : look vec is ZERO");
            look.x = look.y = look.z = 0.0f;
        } else {
#line 159 "D:/Bio4/Prog/esp0b.cpp"
            VECNormalize(&look, &look);
        }
        CameraGetUpVec(cam, &up);
        PSVECCrossProduct(&look, &up, &side);
        PSVECScale(&look, &w->ofs, -w->prm.z);
        PSVECScale(&side, &tmp, w->prm.x * fRand1_1());
        PSVECAdd(&w->ofs, &tmp, &w->ofs);
        PSVECScale(&up, &tmp, w->prm.y * fRand1_1());
        PSVECAdd(&w->ofs, &tmp, &w->ofs);
        if (esp->parent != pEffParentWorld) {
            PSMTXInverse(esp->parent->mat, inv);
            PSMTXMultVecSR(inv, &w->ofs, &w->ofs);
        }
        PSVECAdd(&esp->pos, &w->ofs, &esp->pos);
        EspCommonTrans(esp);
        PSVECSubtract(&esp->pos, &w->ofs, &esp->pos);
    } else {
        EspCommonTrans(esp);
    }
}

int cEsp0b::SetFreeWork(EspGenWork* gen, u32* seed)
{
    work.prm = *(Vec*)&gen->xD8;
    if (gen->xC8 != 0) {
        pLog->err(0, 0, "ESP : 'ESP15' WK0 not 0!! ");
    }
    if (gen->xC9 != 0) {
        pLog->err(0, 0, "ESP : 'ESP15' WK1 not 0!! ");
    }
    return 1;
}
