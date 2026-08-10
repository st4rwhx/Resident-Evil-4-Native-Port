#include "types.h"
#include "vec.h"
#include "cManager.h"
#include "ctrl.h"
#include "atari.h"
#include "light.h"
#include "TexRender.h"
#include "esp.h"

void cCtrl12::move()
{
    Ctrl12Work* w = (Ctrl12Work*) work;
    int i;
    u32 j;

    for (i = 0; i < 13; i++) {
        if (w->timer[i] != 0) {
            w->timer[i]--;
        }
    }
    for (j = 0; j < 1; j++) {
        w->flag[j] = 0;
    }
}

cCtrl* GetCtrlCtrl12()
{
    cCtrl* c;
    u32 i;
    u32 n = CtrlMgr.nArray;

    for (i = 0; i < n; i++) {
        c = CtrlMgrWork(i);
        if ((c->be_flag & 0x201) == 1 && c->id == 0x12) {
            return c;
        }
    }
    c = CtrlMgr.createBack(0x12);
    if (c == 0) {
        return 0;
    }
    return c;
}

void Ctrl12Set(cCtrl* c, int idx, u16 val)
{
    Ctrl12Work* w;

    if (c == 0) {
        return;
    }
    if (c->id != 0x12) {
        return;
    }
    if (idx > 12) {
        return;
    }
    w = (Ctrl12Work*) c->work;
    w->timer[idx] = val;
}

int Ctrl12Ck(cCtrl* c, int idx)
{
    Ctrl12Work* w;

    if (c == 0) {
        return 0;
    }
    if (c->id != 0x12) {
        return 0;
    }
    if (idx > 12) {
        return 0;
    }
    w = (Ctrl12Work*) c->work;
    if (w->timer[idx] != 0) {
        return 1;
    }
    return 0;
}

void Ctrl12CntAdd(cCtrl* c, int idx, u16 add)
{
    Ctrl12Work* w;
    u16 v;

    if (c == 0) {
        return;
    }
    if (c->id != 0x12) {
        return;
    }
    if (idx > 5) {
        return;
    }
    w = (Ctrl12Work*) c->work;
    v = w->cnt[idx];
    w->cnt[idx] = v + add;
}

int Ctrl12CntCk(cCtrl* c, int idx, u16 val)
{
    Ctrl12Work* w;

    if (c == 0) {
        return 0;
    }
    if (c->id != 0x12) {
        return 0;
    }
    if (idx > 5) {
        return 0;
    }
    w = (Ctrl12Work*) c->work;
    return w->cnt[idx] >= val;
}

TexRenderMng* Ctrl12GetTexRenderEm2b(cCtrl* c)
{
    Ctrl12Work* w;
    TexRenderMng* t;

    if (c == 0) {
        return 0;
    }
    if (c->id != 0x12) {
        return 0;
    }
    w = (Ctrl12Work*) c->work;
    t = w->tex2b;
    if (t == 0) {
        GetTexRenderMgr(&w->tex2b);
        if (w->tex2b != 0) {
            EstSet(0, -1, 0, 0, 1, 0x42, w->tex2b->mask | 1, 0, (u32) t, t);
        }
    }
    return w->tex2b;
}

TexRenderMng* Ctrl12GetTexRenderEm2c(cCtrl* c)
{
    Ctrl12Work* w;

    if (c == 0) {
        return 0;
    }
    if (c->id != 0x12) {
        return 0;
    }
    w = (Ctrl12Work*) c->work;
    if (w->tex2c == 0) {
        GetTexRenderMgr(&w->tex2c);
    }
    return w->tex2c;
}

TexRenderMng* Ctrl12GetTexRenderEm32(cCtrl* c)
{
    Ctrl12Work* w;

    if (c == 0) {
        return 0;
    }
    if (c->id != 0x12) {
        return 0;
    }
    w = (Ctrl12Work*) c->work;
    if (w->tex32 == 0) {
        GetTexRenderMgr(&w->tex32);
    }
    return w->tex32;
}
