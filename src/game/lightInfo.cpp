#include "model.h"
#include "db_log.h"
#include "math_sub.h"

// pointer to game memory (0x80000000 .. 0x82FFFFFF)
#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

cLightInfo::cLightInfo()
{
    int i;

    for (i = 0; i < 8; i++) {
        pLight[i] = 0;
    }
    x50 = 0;
    x51 = 0;
    x52 = 0;
    x53 = 0;
    x54 = 0;
    ofs.x = ofs.y = ofs.z = 0.0f;
    size.x = size.y = size.z = 0.0f;
}

int cLightInfo::init2(int a, int b, const Vec* p0, const Vec* p1, int c)
{
    int i;

    if (!VALID_PTR(p0) || !VALID_PTR(p1)) {
        pLog->err(0, 0, "cLightInfo::init2() PTR ERROR %08X %08X", p0, p1);
        return 0;
    }
    for (i = 0; i < 8; i++) {
        pLight[i] = 0;
    }
    x51 = a;
    x50 = c;
    x52 = b;
    x54 = 0xFFFFFFFF;
    ofs = *p0;
    size = *p1;
    if ((x51 & 3) == 0) {
        radius = size.x + size.y;
    } else if ((x51 & 3) != 2) {
        radius = size.x;
    } else {
        radius = SQRTF(size.x * size.x + size.y * size.y + size.z * size.z);
    }
    return 1;
}

u32 cLightInfo::getLightNum()
{
    u8 n = 0;
    u8 i;

    for (i = 0; i < 8; i++) {
        if (pLight[i] != 0) {
            n++;
        }
    }
    return n;
}

void cLightInfo::updateMatrix(cModel* m)
{
    Vec v;
    Mtx tmp;

    v.x = ofs.x * m->scale.x;
    v.y = ofs.y * m->scale.y;
    v.z = ofs.z * m->scale.z;
    RotVector(&v, &m->rot, &v);
    if (x52 == 0) {
        PSVECAdd(&v, &m->pos, &v);
    } else {
        if (m->pParts == 0) {
            return;
        }
        PSVECAdd(&v, &m->getPartsPtr(x52 - 1)->worldPos, &v);
    }
    RotMatrix(tmp, &m->rot);
    TransMatrix(tmp, &v);
    PSMTXInverse(tmp, mat);
}

cModel* cLightInfo::getPos(cModel* m, Vec* out)
{
    cModel* c;

    if (x52 > 0) {
        c = m->getPartsPtr(x52 - 1);
        if (!VALID_PTR(c)) {
            pLog->err(0, 0, "litHitCk PNo%d %d %d %x %x", x52 - 1, m->x12E, m->id, x51, x50);
            c = m;
        }
        PSMTXMultVecSR(c->mat, &ofs, out);
        PSVECAdd(out, &c->worldPos, out);
    } else {
        c = m;
        PSMTXMultVecSR(c->mat, &ofs, out);
        PSVECAdd(out, &c->pos, out);
    }
    return c;
}
