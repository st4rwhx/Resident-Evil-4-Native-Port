#include "atari.h"
#include "light.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"
#include "cloth.h"

struct Esp4eWork {
    Cloth* cloth;      // 0x00
    GXTexObj tex;      // 0x04
    GXTlutObj tlut;    // 0x24
    f32 angX;          // 0x30 wave phase along x
    f32 angY;          // 0x34 wave phase along y
    s8 waveSpdX;       // 0x38
    s8 waveSpdY;       // 0x39
    s8 ampX;           // 0x3A
    s8 ampY;           // 0x3B
    f32 freqX;         // 0x3C
    f32 freqY;         // 0x40
    f32 power;         // 0x44
    f32 ang;           // 0x48 global phase
    f32 angSpd;        // 0x4C
    f32 powerRate;     // 0x50
    f32 rnd;           // 0x54 random factor
};

// Cloth sheet: a Cloth grid attached to the effect position, waving with a sine field.
class cEsp4e : public cEsp {
public:
    Esp4eWork w;       // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
    virtual void Destruct();
};

extern "C" {
cEsp* Esp4e_Create();
void Esp4e_Trans();
}

cEsp* Esp4e_Create()
{
    return new cEsp4e;
}

void cEsp4e::move()
{
    Vec pos0 = pos;
    Vec sp;
    Esp4eWork* wk = &w;
    Cloth* c;
    f32 base;
    f32 rand;
    f32 stepY;
    f32 stepX;
    f32 wx;
    f32 wy;
    f32 ay;
    f32 ax;
    f32 s1;
    f32 s;
    u32 i;
    u32 j;

    if (!CommonMove()) {
        return;
    }
    pos = pos0;
    c = wk->cloth;
    if (c == NULL) {
        return;
    }
    if (parent != pEffParentWorld) {
        Vec p;
        Vec r;
        Mtx m;
        PSMTXMultVec(parent->mat, &pos, &p);
        low_RotMatrix(m, &rot);
        PSMTXConcat(parent->mat, m, m);
        Matrix2AxisAngle(m, &r);
        c->SetPosAng(r, p);
    }
    {
        Mtx m;
        PSVECScale(&spd, &sp, 0.1f);
        RotMatrix(m, &rot);
        PSMTXInverse(m, m);
        PSMTXMultVec(m, &sp, &sp);
    }
    c->colR = (u8) colR;
    c->colG = (u8) colG;
    c->colB = (u8) colB;
    c->colA = (u8) colA;
    {
        static f32 DAMPING = 0.98f;
        c->calcSpeed(DAMPING);
    }
    c->move();
    c->calcNormal();

    wk->ang += wk->angSpd;
    rand = wk->rnd;
    wk->ang = LIMIT_ANGLE(wk->ang);
    s1 = SINF(wk->ang);
    s = wk->powerRate * s1 * SINF(wk->ang * 0.3f) + 1.0f;
    wk->angX = (f32) wk->waveSpdX * 0.01f + wk->angX;
    wk->angY = (f32) wk->waveSpdY * 0.01f + wk->angY;
    stepY = wk->freqX / c->ny;
    stepX = wk->freqY / c->nx;
    wx = (f32) wk->ampX * 0.025f;
    wy = (f32) wk->ampY * 0.025f;
    PSVECScale(&sp, &sp, s);
    ay = wk->angX;
    wk->angX = LIMIT_ANGLE(wk->angX);
    wk->angY = LIMIT_ANGLE(wk->angY);

    for (i = 0; i < c->ny; i++) {
        base = SINF(ay) * wx;
        ay += stepY * rand * fRand0_1() + stepY;
        ax = wk->angY;
        for (j = 0; j < c->nx; j++) {
            f32 v = SINF(ax) * wy;
            ax += stepX * rand * fRand0_1() + stepX;
            c->disturbance((base + v) * s + wk->power * s, j, i);
            PSVECAdd(&c->spd[j + c->nx * i], &sp, &c->spd[j + c->nx * i]);
        }
    }
}

void cEsp4e::Destruct()
{
    if (w.cloth) {
        w.cloth->Destroy();
    }
}

void Esp4e_Trans()
{
}

int cEsp4e::SetFreeWork(EspGenWork* gen, u32* seed)
{
    Esp4eWork* wk = &w;
    void* tpl;
    int ci;
    int nx;
    int ny;
    f32 width;
    f32 height;
    f32 d;
    u32 t;
    int flag;

    if (!EspGetTplAddr(anmNo, &tpl)) {
        pLog->err(0, 0, "ESP4e : tex init invalid.");
        return 0;
    }
    if (!PullCloth(&wk->cloth)) {
        pLog->err(0, 0, "ESP4e : init invalid.");
        return 0;
    }
    d = 60.857143f;
    ci = ClothTexSetUp(tpl, &wk->tex, 0, &wk->tlut);
    nx = (int) (sizeX / 200.0f * 36.0f);
    ny = (int) (sizeY / 200.0f * 24.0f);
    width = gen->xD8 * 0.1f + 1.0f;
    height = gen->xDC * 0.1f + 1.0f;
    if (width == 0.0f) {
        width = 0.001f;
    }
    if (height == 0.0f) {
        height = 0.001f;
    }
    t = gen->flags & 1;
    flag = t == 0;
    if (nx < 2) {
        nx = 2;
    }
    if (ny < 2) {
        ny = 2;
    }
    if (nx > 100) {
        nx = 100;
    }
    if (ny > 50) {
        ny = 50;
    }
    if (ci) {
        wk->cloth->Set(rot, pos, nx, ny, width, &wk->tex, height * (3000.0f / d / 23.0f), NULL, d, &wk->tlut, flag);
    } else {
        wk->cloth->Set(rot, pos, nx, ny, width, &wk->tex, height * (3000.0f / d / 23.0f), NULL, d, NULL, flag);
    }
    if (gen->xC2) {
        wk->cloth->x74 = 1;
    }
    wk->waveSpdX = gen->xC8;
    wk->ampX = gen->xC9;
    wk->waveSpdY = gen->xCA;
    wk->ampY = gen->xCB;
    wk->freqX = (f32) (int) (gen->prm.w.xCC + 1) * 0.5f;
    wk->freqY = (f32) (int) (gen->prm.w.xD0 + 1) * 0.5f;
    wk->power = (f32) (int) gen->xD4 * 0.025f;
    wk->angSpd = (f32) (gen->xFC + 1) * 0.0025f;
    wk->powerRate = (f32) (gen->xFD + 1) * 0.07f;
    wk->rnd = (f32) (gen->xFE + 1) * 0.2f;
    return 1;
}

asm(".section .sdata; .balign 8");
