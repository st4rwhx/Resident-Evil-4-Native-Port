#include "atariInfo.h"
#include "atari.h"
#include "model.h"
#include "global.h"
#include "math_sub.h"
#include "dbmodule.h"
#include "main_mem.h"

cAtariInfo::cAtariInfo()
{
    memclr_asm(this, sizeof(cAtariInfo));
}

void cAtariInfo::init0(int parts, int cnt, int flag, f32 x, f32 y, f32 z, f32 rx, f32 rz, f32 w, f32 hh)
{
    pos.x = x;
    pos.y = y;
    pos.z = z;
    rectX2 = rx;
    rectZ2 = rz;
    rectX = rx;
    rectZ = rz;
    x2C = w;
    h = hh;
    partsNo = parts;
    this->cnt = cnt;
    flags = flag | 0x300;
    x48 = 0;
    x26 = 1;
}

void cAtariInfo::init(int parts, int flag, int cnt, f32 x, f32 y, f32 z, f32 rx, f32 rz, f32 w, f32 hh)
{
    init0(parts, cnt, flag, x, y, z, rx, rz, w, hh);
    flags |= 1;
}

void cAtariInfo::set(int mode, f32 a, f32 b)
{
    if (mode < 0) {
        rectX2 = a;
        rectZ2 = b;
        rectZ = 100.0f;
        rectX = 100.0f;
        mode = -mode;
    } else {
        if (mode == 0) {
            rectX = a;
            rectZ = b;
        }
        rectX2 = a;
        rectZ2 = b;
    }
    cnt = mode;
}

// Dead-stripped by the original linker (only its constant pool survives in .rodata).
static f32 atariInfoRange(f32 v)
{
    if (v < 250.0f) {
        return 600.0f;
    }
    return 100000.0f;
}

void cAtariInfo::getSpeedVector(cModel* m, Vec* oldPos, Vec* newPos)
{
    Vec v;

    if (partsNo != 0) {
        cModel* p = m->getPartsPtr(partsNo - 1);
        v.x = pos.x;
        v.y = pos.y;
        v.z = pos.z;
        RotVector(&v, &m->rot, &v);
        PSVECAdd(&p->worldPos, &v, newPos);
        PSVECAdd(&p->oldWorldPos, &v, oldPos);
        newPos->y = m->pos.y + rectX;
        oldPos->y = m->oldPos.y + rectX;
    } else {
        v.x = pos.x;
        v.y = pos.y + rectX;
        v.z = pos.z;
        RotVector(&v, &m->rot, &v);
        PSVECAdd(&m->pos, &v, newPos);
        PSVECAdd(&m->oldPos, &v, oldPos);
    }
}

void cAtariInfo::move()
{
    if (cnt != 0) {
        f32 c = (f32) cnt;
        cnt--;
        rectX += (rectX2 - rectX) / c;
        rectZ += (rectZ2 - rectZ) / c;
    }
}

void cAtariInfo::getPos(cModel* m, Vec* out)
{
    Vec v;

    RotVector(&pos, &m->rot, &v);
    if (partsNo != 0) {
        PSVECAdd(&m->getPartsPtr(partsNo - 1)->worldPos, &v, out);
    } else {
        PSVECAdd(&m->pos, &v, out);
    }
}

void cAtariInfo::setPriority(int prio)
{
    flags &= ~0x18;
    switch (prio) {
    case 0:
        break;
    case 1:
        flags |= 0x8;
        break;
    case 2:
        flags |= 0x10;
        break;
    case 3:
        flags |= 0x18;
        break;
    }
}

void cAtariInfo::disp(cModel* m)
{
    if (flags & 2) {
        dispRect(m);
    } else {
        Vec p;
        getPos(m, &p);
        p.y -= h;
        Draw_cylinder(&p, rectZ, h * 2.0f, 0xFFFFFFFF);
    }
}

void cAtariInfo::dispRect(cModel* m)
{
    static u8 ptbl[36] = {
        0, 2, 1, 2, 3, 1, 4, 5, 6, 5, 7, 6, 2, 6, 3, 6, 7, 3,
        0, 1, 4, 1, 5, 4, 1, 3, 5, 3, 7, 5, 2, 0, 6, 0, 4, 6,
    };
    Vec v[8];
    Vec w[8];
    Vec size;
    Mtx mat;
    int i;

    size.x = rectX;
    size.y = h;
    size.z = rectZ;
    v[0].x = -size.x;
    v[0].y = -size.y;
    v[0].z = -size.z;
    v[1].x = size.x;
    v[1].y = -size.y;
    v[1].z = -size.z;
    v[2].x = -size.x;
    v[2].y = -size.y;
    v[2].z = size.z;
    v[3].x = size.x;
    v[3].y = -size.y;
    v[3].z = size.z;
    v[4].x = -size.x;
    v[4].y = size.y;
    v[4].z = -size.z;
    v[5].x = size.x;
    v[5].y = size.y;
    v[5].z = -size.z;
    v[6].x = -size.x;
    v[6].y = size.y;
    v[6].z = size.z;
    v[7].x = size.x;
    v[7].y = size.y;
    v[7].z = size.z;
    for (i = 0; i < 8; i++) {
        PSVECAdd(&v[i], &pos, &v[i]);
    }
    if (partsNo != 0) {
        cModel* p = m->getPartsPtr(partsNo - 1);
        PSMTXRotRad(mat, 'y', p->rot.y);
        TransMatrix(mat, &p->worldPos);
        PSMTXConcat(pG->Cam.viewMat, mat, mat);
    } else {
        PSMTXRotRad(mat, 'y', m->rot.y);
        TransMatrix(mat, &m->pos);
        PSMTXConcat(pG->Cam.viewMat, mat, mat);
    }
    for (i = 0; i < 12; i++) {
        u8* t = &ptbl[i * 3];
        Draw_line3d_local(&v[t[0]], &v[t[1]], mat, 0xFFFFFFFF, 0);
        Draw_line3d_local(&v[t[1]], &v[t[2]], mat, 0xFFFFFFFF, 0);
        Draw_line3d_local(&v[t[2]], &v[t[0]], mat, 0xFFFFFFFF, 0);
    }
}
