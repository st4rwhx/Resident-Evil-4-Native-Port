#include "types.h"
#define DB_TOOLBASE_IMPLEMENTATION
#include "dbg_tool.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "dbmodule.h"

// Debug tool window / button base (D:/Bio4/Prog/db_toolbase.cpp): colour packing, box drawing and the
// cDbgWindow button container the tool editors derive from.

u32 MakeCol(f32 r, f32 g, f32 b, f32 a)
{
    u32 col;

    col = (u8) (a * 255.0f) << 24;
    col += (u8) (r * 255.0f) << 16;
    col += (u8) (g * 255.0f) << 8;
    col += (u8) (b * 255.0f);
    return col;
}

void DbgDrawBox(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a)
{
    Vec p0;
    Vec p1;
    f32 x2 = x + w;
    f32 y2;

    p0.x = x;
    p0.y = y;
    p1.x = x2;
    p1.y = y;
    Draw_line(&p0, &p1, MakeCol(r, g, b, a));
    y2 = y + h;
    p0.x = x2;
    p0.y = y;
    p1.x = x2;
    p1.y = y2;
    Draw_line(&p0, &p1, MakeCol(r, g, b, a));
    p0.x = x2;
    p0.y = y2;
    p1.x = x;
    p1.y = y2;
    Draw_line(&p0, &p1, MakeCol(r, g, b, a));
    p0.x = x;
    p0.y = y2;
    p1.x = x;
    p1.y = y;
    Draw_line(&p0, &p1, MakeCol(r, g, b, a));
}

void DbgDrawBoxFill(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a)
{
    Vec p0;
    Vec p1;

    p0.x = x;
    p0.y = y;
    p1.x = w;
    p1.y = h;
    Draw_quad(&p0, &p1, MakeCol(r, g, b, a));
}

void cDbgWindow::AddButton(int bx, int by, const char* name, int bcx, int bcy, void (*func)(cDbgButton*),
                           void (*update)(cDbgButton*))
{
    cDbgButton* b;

    pButton[num] = b = new cDbgButton(bx, by, name, bcx, bcy);
    b->pUpdate = update;
    b->pFunc = func;
    if (pButton[num] == 0) {
        pLog->err(0, 0, "AddButton(): new failed.");
        return;
    }
    if (w < bx + strlen(name)) {
        w = bx + strlen(name);
    }
    if (h < by) {
        h = by;
    }
    if (bcx < 0xFFFF && bcy < 0xFFFF) {
        if (pCur == 0) {
            pTop = pCur = pButton[num];
        }
        pBottom = pButton[num];
        if (cxMax < bcx) {
            cxMax = bcx;
        }
        if (cyMax < bcy) {
            cyMax = bcy;
        }
    }
    num++;
}

int cDbgWindow::FindButton(int bcx, int bcy, cDbgButton** out)
{
    u32 i;

    *out = 0;
    for (i = 0; i < num; i++) {
        if (pButton[i]->cx == bcx && pButton[i]->cy == bcy) {
            *out = pButton[i];
            return 1;
        }
    }
    return 0;
}

int cDbgWindow::LocalUpdate()
{
    int ret = 1;
    int bcx;
    int bcy;
    u32 rep;

    bcx = GetCx();
    bcy = GetCy();
    rep = Joy[0].rep;
    if (rep & 0x10001) {
        bcx--;
    }
    if (rep & 0x20002) {
        bcx++;
    }
    if (rep & 0x80008) {
        bcy--;
    }
    if (rep & 0x40004) {
        bcy++;
    }
    if (bcx < 0) {
        bcx = cxMax;
    }
    if (bcy < 0) {
        bcy = cyMax;
    }
    if (bcx > cxMax) {
        bcx = 0;
    }
    if (bcy > cyMax) {
        bcy = 0;
    }
    if (bcx != GetCx() || bcy != GetCy()) {
        cDbgButton* b;

        if (FindButton(bcx, bcy, &b)) {
            pCur = b;
        }
    }
    ButtonAllUpdate();
    if (Joy[0].trg & 0x200) {
        ret = 0;
    }
    return ret;
}

void cDbgWindow::LocalDisp()
{
    u32 i;
    cDbgButton* cur;

    for (i = 0; i < num; i++) {
        cDbgButton* b = pButton[i];
        int by = y + 1;

        eprintf2(8, 12, (x + b->x) * 8, (by + b->y) * 14, 0x10, 0, b->pName);
    }
    cur = pCur;
    if (cur) {
        int bx = x;
        int by = y + 1;

        if (pG->flags_51E4 & 4) {
            eprintf2(8, 12, (bx + cur->x - 1) * 8, (by + cur->y) * 14, 0, 0, cDbgStr::cursor());
        }
        eprintf2(8, 12, (bx + cur->x) * 8, (by + cur->y) * 14, 0, 0, cur->pName);
        {
            f32 fx = (f32) ((bx + cur->x) * 8);
            f32 fh = 14.0f;
            f32 mgn = 2.0f;
            f32 zero = 0.0f;

            DbgDrawBoxFill(fx - mgn, (f32) ((by + cur->y) * 14) - mgn, (f32) (cur->nameLen * 8) + zero,
                           fh + mgn, 0.7f, 0.7f, zero, 0.3f);
        }
    }
}

