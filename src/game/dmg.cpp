// game/dmg: damage volumes (D:/Bio4/Prog/dmg.cpp).
#include "types.h"
#include "global.h"
#include "dmg.h"
#include "dbmodule.h"
#include "math_sub.h"

cDmgMgr::cDmgMgr() : cManager<cDmg>(0x118, 2)
{
    setName("cDmgMgr");
}

int cDmgMgr::construct(cDmg* p, int id)
{
    switch (id) {
    case 0:
    default:
        new (p) cDmgCyl;
        p->be_flag = 1;
        break;
    case 1:
        new (p) cDmgP4;
        p->be_flag = 1;
        break;
    }
    p->id = id;
    return 1;
}

int cDmgMgr::construct(cDmg* p, u32 id)
{
    return construct(p, (int) id);
}

void cDmgMgr::move()
{
    u32 i;

    for (i = 0; i < nArray; i++) {
        cDmg* p = (cDmg*) ((u8*) pArray + size * i);
        dieCheck();
        if ((p->be_flag & 0x201) == 1) {
            if (--p->timer == 0) {
                destroy(p);
            }
        }
    }
}

int cDmgMgr::set(int kind, int time, Vec* pos, f32 r, f32 h)
{
    cDmgCyl* p = (cDmgCyl*) create(0);

    if (p == 0) {
        return 0;
    }
    p->kind = kind;
    p->timer = time;
    p->pos = *pos;
    p->r = r;
    p->h = h;
    return 1;
}

int cDmgMgr::set(int kind, int time, Vec* pt, f32 h)
{
    cDmgP4* p = (cDmgP4*) create(1);

    if (p == 0) {
        return 0;
    }
    p->kind = kind;
    p->timer = time;
    p->pt[0] = pt[0];
    p->pt[1] = pt[1];
    p->pt[2] = pt[2];
    p->pt[3] = pt[3];
    p->h = h;
    return 1;
}

int cDmgMgr::hitCheck(Vec* pos, Vec* out)
{
    u32 i;

    for (i = 0; i < nArray; i++) {
        cDmg* p = (cDmg*) ((u8*) pArray + size * i);
        if ((p->be_flag & 0x201) == 1) {
            if (p->hitCheck(pos, out)) {
                return p->kind;
            }
        }
    }
    return 0;
}

int cDmgCyl::hitCheck(Vec* p, Vec* out)
{
    if (pG->flags_68 & 0x10000000) {
        Draw_cylinder(&pos, r, h, 0xFFFFFFFF);
    }
    if (p->y > pos.y + h) {
        return 0;
    }
    if (p->y < pos.y - h) {
        return 0;
    }
    if ((p->x - pos.x) * (p->x - pos.x) + (p->z - pos.z) * (p->z - pos.z) > r * r) {
        return 0;
    }
    if (out) {
        *out = pos;
    }
    return kind;
}

int cDmgP4::hitCheck(Vec* p, Vec* out)
{
    u32 i;

    if (HitCheckPoint4(p, pt)) {
        if (out) {
            out->x = 0.0f;
            out->y = 0.0f;
            out->z = 0.0f;
            for (i = 0; i < 4; i++) {
                PSVECAdd(out, &pt[i], out);
            }
            PSVECScale(out, out, 0.25f);
        }
        return kind;
    }
    return 0;
}

void cDmg::beginEvent()
{
    DmgMgr.destroy(this);
}

cDmgMgr DmgMgr;

// The split object carries 16 unnamed zero bytes after DmgMgr (0x802DA150): dvd.cpp's .bss follows
// at the next 32-byte boundary and ngcld does not pad for it. A zero-initialised static referenced
// only by a never-called inline is emitted after DmgMgr (first-declaration order) without a body.
static u8 dmg_pad[16];
static inline u8* dmgPad()
{
    return dmg_pad;
}
