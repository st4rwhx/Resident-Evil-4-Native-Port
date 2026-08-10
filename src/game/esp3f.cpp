#include "atari.h"
#include "light.h"
#include "esp.h"

#define ESP3F_BUF_MAX 0x12

// Vector buffer effect: a parent esp owns up to 0x12 child esps whose work areas hold Vec arrays.
struct Esp3fWork {
    u8 pad_0[2];
    u8 nBuf;                     // 0x02 number of child buffers
    u8 nPerBuf;                  // 0x03 vectors per child buffer
    cEsp* pBuf[ESP3F_BUF_MAX];   // 0x04
};

class cEsp3f : public cEsp {
public:
    Esp3fWork work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
    virtual void Destruct();
};

// Child buffer: its work area is the vector array.
class cEsp3fBuf : public cEsp {
public:
    Vec vec[1];  // 0xF8
};

cEsp* Esp3f_Create()
{
    return new cEsp3f;
}

void cEsp3f::move()
{
}

void cEsp3f::Destruct()
{
    if (x10 == 0) {
        Esp3fWork* w = &work;
        u32 i;
        for (i = 0; i < w->nBuf; i++) {
            cEsp* p = w->pBuf[i];
            if (p && (p->flag & 1)) {
                PushEsp(p);
            }
        }
    }
}

int Esp3f_Alloc(u32 size, u32 num, cEsp3f** out, EspInfo* info)
{
    cEsp* dmy = EspGetDmyPtr();
    u32 per = 0x58 / size;
    u32 n;
    cEsp* p;
    cEsp* c;
    cEsp3f* e;
    Esp3fWork* w;
    u32 i;

    *out = 0;
    n = num / per + 1;
    if (n > ESP3F_BUF_MAX) {
        pLog->err(0, 0, "ESP_3F : Buf size over.[%d/%d]", n, ESP3F_BUF_MAX);
        return 0;
    }
    if (!PullEsp(&p, 0x3f)) {
        return 0;
    }
    e = (cEsp3f*)p;
    e->info = *info;
    w = &e->work;
    w->nPerBuf = per;
    w->nBuf = n;
    for (i = 0; i < w->nBuf; i++) {
        if (PullEsp(&c, 0x3f)) {
            c->x10 = 1;
            c->info = *info;
            w->pBuf[i] = c;
        } else {
            u32 j;
            for (j = 0; j < w->nBuf; j++) {
                if (w->pBuf[j]) {
                    PushEsp(w->pBuf[j]);
                }
            }
            PushEsp(p);
            return 0;
        }
    }
    *out = (cEsp3f*)p;
    return 1;
}

Vec* Esp3f_GetVecPtr(cEsp3f* p, u32 no)
{
    Esp3fWork* w = &p->work;
    u32 per = w->nPerBuf;
    u32 n = w->nBuf;
    u32 buf = no / per;
    Vec* ret;

    if (buf < n) {
        cEsp3fBuf* b = (cEsp3fBuf*)w->pBuf[buf];
        Vec* vp = b->vec;
        ret = &vp[no % per];
        if ((int)ret < 0) {
            return ret;
        }
    }
    pLog->err(0, 0, "ESP_3F : access out of range.[%d/%d]", buf, n);
    return 0;
}

int cEsp3f::SetFreeWork(EspGenWork* gen, u32* seed)
{
    return 1;
}
