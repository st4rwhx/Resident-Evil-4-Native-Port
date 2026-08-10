#include "atari.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"


struct Esp0aWork {
    u8 type;   // 0x00 0: spawner (copies itself 50 times), 1: rim-lit sprite
    u8 alpha;  // 0x01 base alpha
};

static f32 RIMIT1 = 0.9f;
static f32 RIMIT2 = 0.6f;
static f32 RIMIT3 = 0.9f;
static f32 RIMIT4 = 0.6f;

// Rim highlight sprite: the alpha fades in when the sprite's direction (spd) points toward
// or away from the camera while the sprite is in front of it.
class cEsp0a : public cEsp {
public:
    Esp0aWork work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};

extern "C" {
void Esp0a_Trans(cEsp0a* esp);
void Esp0a_Trans2(cEsp* esp);
}

// The pulled copy is kept in a one-member struct: the original reloads the pointer from its
// stack slot after every store through it (esp_sub EspSeqSet idiom).
struct EspPtr {
    cEsp* p;
};

// Reference read of the world parent: an unflagged MEM that stays below the stores through e.p.
static inline cCoord* RefCoordP(cCoord*& p)
{
    return p;
}

cEsp* Esp0a_Create()
{
    return new cEsp0a;
}

void cEsp0a::move()
{
    Esp0aWork* w = &work;
    Vec dir;
    Vec vpos;
    Mtx m;
    f32 dot;

    switch (w->type) {
    case 0:
        PushEsp(this);
        break;
    case 1:
        PSMTXConcat(pG->Cam.viewMat, parent->mat, m);
        PSMTXMultVecSR(m, &spd, &dir);
        PSMTXMultVec(m, &pos, &vpos);
        if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f) {
            dir.y = 1.0f;
        }
        if (vpos.x == 0.0f && vpos.y == 0.0f && vpos.z == 0.0f) {
            vpos.y = 1.0f;
        }
#line 51 "D:/Bio4/Prog/esp0a.cpp"
        VECNormalize(&dir, &dir);
        VECNormalize(&vpos, &vpos);
        dot = PSVECDotProduct(&dir, &vpos);
        if (vpos.z < -RIMIT1 && dot < -RIMIT2) {
            colA = (f32)w->alpha * (1.0f - ((-dot - RIMIT2) / (1.0f - RIMIT2)) * (-vpos.z - RIMIT1) / (1.0f - RIMIT1));
        } else if (vpos.z < -RIMIT3 && dot > RIMIT4) {
            colA = (f32)w->alpha * (1.0f - ((dot - RIMIT4) / (1.0f - RIMIT4)) * (-vpos.z - RIMIT3) / (1.0f - RIMIT3));
        } else {
            colA = (f32)w->alpha;
        }
        break;
    }
}

void Esp0a_Trans(cEsp0a* esp)
{
    Esp0aWork* w = &esp->work;

    switch (w->type) {
    case 0:
        break;
    case 1: {
        cEsp* base;
        EspPtr e;
        u32 i = 0;

        if (PullEsp(&base, 0)) {
            *base = *esp;
            base->id = 0;
            for (; i < 50; i++) {
                if (PullEsp(&e.p, 0)) {
                    Vec wpos;
                    *e.p = *base;
                    int ot = 8;
                    e.p->spd.x = e.p->spd.y = e.p->spd.z = 0.0f;
                    e.p->scaleSpd = 0.0f;
                    e.p->colRSpd = 1.0f;
                    e.p->colGSpd = 1.0f;
                    e.p->colBSpd = 1.0f;
                    e.p->colASpd = 1.0f;
                    e.p->xA8 = 0;
                    e.p->xAA = 0;
                    e.p->spdCnt = 0;
                    e.p->scaleCnt = 0;
                    e.p->life = 1;
                    e.p->cnt = 0;
                    if (e.p->parent != RefCoordP(pEffParentWorld)) {
                        PSMTXMultVec(e.p->parent->mat, &e.p->pos, &wpos);
                    } else {
                        wpos = e.p->pos;
                    }
                    AddOtWorldPos(e.p, Esp0a_Trans2, &wpos, ot, 0.0f);
                }
                if (!base->CommonMove()) {
                    break;
                }
            }
            if (base->flag & 1) {
                PushEsp(base);
            }
        }
        break;
    }
    }
}

void Esp0a_Trans2(cEsp* esp)
{
    EspCommonTrans(esp);
}

int cEsp0a::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp0aWork* w = &work;

    if (gen->xC9 != 0) {
        pLog->err(0, 0, "ESP0a : WK1 not 0!!");
    }
    w->type = gen->xCA;
    w->alpha = x83;
    switch (w->type) {
    case 0: {
        cEsp* base;
        cEsp* p;
        u32 i = 0;

        if (PullEsp(&base, 0)) {
            *base = *this;
            base->id = 0;
            for (; i < 50; i++) {
                if (PullEsp(&p, 0)) {
                    *p = *base;
                    p->spd.x = p->spd.y = p->spd.z = 0.0f;
                    p->scaleSpd = 0.0f;
                    p->colRSpd = 1.0f;
                    p->colGSpd = 1.0f;
                    p->colBSpd = 1.0f;
                    p->colASpd = 1.0f;
                    p->xA8 = 0;
                    p->xAA = 0;
                    p->spdCnt = 0;
                    p->scaleCnt = 0;
                    p->cnt = 0;
                    p->life = (s8)gen->xC8;
                }
                if (!base->CommonMove()) {
                    break;
                }
            }
            if (base->flag & 1) {
                PushEsp(base);
            }
        }
        break;
    }
    case 1:
        flags |= 0x400;
        dispFlag |= 2;
        break;
    }
    return 1;
}
