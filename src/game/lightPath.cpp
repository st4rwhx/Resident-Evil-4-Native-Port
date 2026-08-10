#include "lightPath.h"
#include "db_log.h"

// pointer to game memory (0x80000000 .. 0x82FFFFFF)
#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

u32 cLightPathHeader::getSize()
{
    u8* p;

    if (num == 0) {
        return 4;
    }
    p = (u8*) this + (*(u32*) ((u8*) this + num * 4) + 4);
    while (*p != 0xFF) {
        p++;
    }
    p++;
    return (u32) p - (u32) this;
}

cLightPathData* cLightPathHeader::getPathData(u32 no)
{
    if (no >= num) {
        pLog->err(0, 0, "cLightPathHeader::getPathData() IDX OVER %d", no);
        return 0;
    }
    return (cLightPathData*) ((u8*) this + *(u32*) (no * 4 + (u32) this + 4));
}

u32 cLightPathData::getSize()
{
    u8* p = data;
    u32 n = 1;

    while (*p != 0xFF) {
        p++;
        n++;
    }
    return n;
}

int cLightPath::setPath(cLightPathData* data, u8 no)
{
    if (!VALID_PTR(data)) { pLog->err(0, 0, "setPath() INVALID PTR %08X", data); return 0; }
    pCur = pStart = data;
    flag = no;
    return 1;
}
int cLightPath::movePath()
{
    u8 v;
    if (!VALID_PTR(pStart) || !VALID_PTR(pCur)) { pLog->err(2, 0, "Light05() INVALID PATH DATA"); return 0; }
    v = pCur->data[0];
    if (v <= 200) {
        if (flag & 2) v = 200 - v;
        pCur = (cLightPathData*) ((u8*) pCur + 1);
    } else if ((flag & 1) == 0) {
        pCur = pStart;
        v = pCur->data[0];
        if (flag & 2) v = 200 - v;
        // COMPILER-DIFF: tie. The loop notes double this store's `this` ref weight (9 refs > pCur's
        // 5/14 priority), which puts `this` in r9 and pCur in r11 like the original; no code changes.
        do { pCur = (cLightPathData*) ((u8*) pCur + 1); } while (0);
    }
    return v;
}
