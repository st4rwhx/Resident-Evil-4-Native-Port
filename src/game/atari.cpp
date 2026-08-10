#include "atari.h"
#include "atariInfo.h"
#include "global.h"
#include "model.h"
#include "em.h"
#include "math_sub.h"
#include "dbmodule.h"
#include "main.h"
#include "gx.h"

#line 30 "D:/Bio4/Prog/atari.cpp"

extern "C" {
// Dolphin SDK performance monitor registers (base/PPCArch.h)
void PPCMtpmc1(u32 v);
void PPCMtpmc2(u32 v);
void PPCMtpmc3(u32 v);
void PPCMtpmc4(u32 v);
void PPCMtmmcr0(u32 v);
void PPCMtmmcr1(u32 v);
u32 PPCMfpmc1();
void* memcpy(void* dst, const void* src, unsigned int n);
}

// at_sub attribute filter bypass mode (cSatMgr::seCk of the manager running the check)
int SEck;
struct SEckView {
    int v;
};

// game/game.cpp collision profiling counters (debug page 0x14)
extern u32 g_at2_total;
extern u32 g_at2_cnt[];
extern u32 g_at2_cyc[];
extern u32 g_at2_total_cyc;

// pointer to game memory (0x80000000 .. 0x82FFFFFF)
#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

// polygons already tested during one check (one bit per polygon index)
u8 polyBit[0x400];
// the piece the last hitCheck2 hit (hitCheck transforms the normal with its matrix)
static cSat* pBypassAt;

int atck(Vec* a, Vec* b, cAtariInfo* info, cModel* m, int flag);
int blkPolySphereCk(cSat* sat, cSatBlock* blk, Vec* a, Vec* b, f32 r, int flag, Vec* nrm, int mask);
int blkPolySphereCkCore(cSat* sat, cSatBlock* blk, Vec* a, Vec* b, f32 r, int flag, Vec* nrm, int mask);
int blkPolyLineCk(cSat* sat, cSatBlock* blk, Vec* a, Vec* b, int flag, int mask, Vec* hit, u32* pn);
int blkPolyLineCkCore(cSat* sat, cSatBlock* blk, Vec* a, Vec* b, int flag, int mask, Vec* hit, u32* pn);
void polyBitSet(u32 no);
int polyBitCk(u32 no);
cSatFile* createSat(Vec* v, u32 attr, f32 h);
cSatFile* createBoxSat(Vec* v, u32 attr, f32 h);
static cSatFile* createFloorSat(Vec* v, u32 attr, f32 h);
void at_pos_calc(cModel* m, Vec* d);

int cSatMgr::check(cModel* m, int flag)
{
    cAtariInfo* info = &((cEm*) m)->atari;
    int ret = 0;

    if (!(info->flags & 0x100)) {
        return 0;
    }
    if (info->flags & 2) {
        ret = checkRect(m);
    } else {
        while (info->next) {
            info = info->next;
            if (scrAtCheckSphere(m, info, flag) != 0.0f) {
                ret = 1;
            }
        }
        if (scrAtCheckSphere(m, &((cEm*) m)->atari, flag) != 0.0f) {
            ret = 1;
        }
    }
    return ret;
}

int cSatMgr::checkRect(cModel* m)
{
    cAtariInfo* info = &((cEm*) m)->atari;
    Vec a;
    Vec b;
    int ret;

    a.x = 0.0f;
    a.y = 0.0f;
    a.z = info->rectZ * 0.9f;
    b.x = info->rectX;
    b.y = 0.0f;
    b.z = info->rectZ * 0.9f;
    ret = atck(&a, &b, info, m, 0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = info->rectX;
    b.y = 0.0f;
    b.z = 0.0f;
    ret |= atck(&a, &b, info, m, 0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = -info->rectZ * 0.9f;
    b.x = info->rectX;
    b.y = 0.0f;
    b.z = -info->rectZ * 0.9f;
    ret |= atck(&a, &b, info, m, 0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = info->rectZ * 0.9f;
    b.x = -info->rectX;
    b.y = 0.0f;
    b.z = info->rectZ * 0.9f;
    ret |= atck(&a, &b, info, m, 0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = -info->rectX;
    b.y = 0.0f;
    b.z = 0.0f;
    ret |= atck(&a, &b, info, m, 0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = -info->rectZ * 0.9f;
    b.x = -info->rectX;
    b.y = 0.0f;
    b.z = -info->rectZ * 0.9f;
    ret |= atck(&a, &b, info, m, 0);
    a.x = info->rectX * 0.9f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = info->rectX * 0.9f;
    b.y = 0.0f;
    b.z = info->rectZ;
    ret |= atck(&a, &b, info, m, 0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 0.0f;
    b.z = info->rectZ;
    ret |= atck(&a, &b, info, m, 0);
    a.x = -info->rectX * 0.9f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = -info->rectX * 0.9f;
    b.y = 0.0f;
    b.z = info->rectZ;
    ret |= atck(&a, &b, info, m, 0);
    a.x = info->rectX * 0.9f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = info->rectX * 0.9f;
    b.y = 0.0f;
    b.z = -info->rectZ;
    ret |= atck(&a, &b, info, m, 0);
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = 0.0f;
    b.y = 0.0f;
    b.z = -info->rectZ;
    ret |= atck(&a, &b, info, m, 0);
    a.x = -info->rectX * 0.9f;
    a.y = 0.0f;
    a.z = 0.0f;
    b.x = -info->rectX * 0.9f;
    b.y = 0.0f;
    b.z = -info->rectZ;
    ret |= atck(&a, &b, info, m, 0);
    return ret;
}

int cSatMgr::checkAir(cModel* m, int flag)
{
    cAtariInfo* info = &((cEm*) m)->atari;
    int ret = 0;

    if (info->flags & 0x100) {
        while (info->next) {
            info = info->next;
            if (scrAtCheckSphereAir(m, info, flag) != 0.0f) {
                ret = 1;
            }
        }
        if (scrAtCheckSphereAir(m, &((cEm*) m)->atari, flag) != 0.0f) {
            ret = 1;
        }
    }
    return ret;
}

// Segment a-b (model local, offset by the info position) against the scenario; the model is
// pushed back to the hit point. Returns 1 when it moved by a metre or more.
int atck(Vec* a, Vec* b, cAtariInfo* info, cModel* m, int flag)
{
    Vec v0;
    Vec v1;
    Vec hit;
    Vec nrm;
    Vec old;

    PSVECAdd(a, &info->pos, &v0);
    PSVECAdd(b, &info->pos, &v1);
    RotVector(&v0, &m->rot, &v0);
    RotVector(&v1, &m->rot, &v1);
    PSVECAdd(&v0, &m->pos, &v0);
    PSVECAdd(&v1, &m->pos, &v1);
    v0.y = v1.y = m->pos.y + 300.0f;
    if (SatMgr.hitCheck(&v0, &v1, &hit, &nrm, flag, 0)) {
        old = m->pos;
        PSVECSubtract(&hit, &v1, &v1);
        PSVECAdd(&m->pos, &v1, &m->pos);
        PSVECAdd(&m->pos, &nrm, &m->pos);
        if (fabsf(m->pos.x - old.x) >= 1.0f || fabsf(m->pos.z - old.z) >= 1.0f) {
            return 1;
        }
    }
    return 0;
}

// Sphere of the collision info against the scenario (walls, then the floor). Returns the
// distance the model was pushed.
f32 cSatMgr::scrAtCheckSphere(cModel* m, cAtariInfo* info, int flag)
{
    Vec pos;
    Vec oldPos;
    Vec newPos;
    Vec up;
    f32 mag;
    f32 floor;
    cModel* link;
    u32 c;

    info->getSpeedVector(m, &oldPos, &pos);
    m->wallNrm.x = 0.0f;
    m->wallNrm.y = 0.0f;
    m->wallNrm.z = 0.0f;
    newPos = pos;
    wallAdjust(&m->wallNrm, &oldPos, &newPos, info->rectX, info->flags, flag);
    PSVECSubtract(&newPos, &pos, &pos);
    mag = PSVECMag(&pos);
    at_pos_calc(m, &pos);
    if (!(info->flags & 4)) {
        floor = getFloor(&m->pos, 600.0f, 100000.0f, (u32*) &m->pFloorNrm, flag);
        if (fabsf(floor - m->pos.y) < 1000.0f) {
            m->pos.y = floor;
        } else if (pG->x4 == 0) {
            f32 x = (f32) ((int) m->pos.x / 100) * 100.0f;
            f32 y = (f32) ((int) m->pos.y / 100) * 100.0f;
            f32 z = (f32) ((int) m->pos.z / 100) * 100.0f;
            if (floor == -100000.0f) {
                pLog->warn(6, 1, "FLOOR LOST %.0f %.0f %.0f", x, y, z);
            } else {
                pLog->warn(4, 2, "FLOOR ERR %.0f %.0f %.0f", x, y, z);
            }
            up = m->pos;
            up.y += 50000.0f;
            c = pG->flags_51E4 & 0x3F;
            c <<= 2;
            if (pG->debug_mode != 0) {
                Draw_line3d(&m->pos, &up, 0xFFFF0000 | (c << 8) | c, 0);
            }
        }
    }
    link = info->pLink;
    if (link) {
        at_pos_calc(link, &pos);
        if (!(((cEm*) link)->atari.flags & 4)) {
            floor = getFloor(&link->pos, 600.0f, 100000.0f, (u32*) &link->pFloorNrm, flag);
            if (fabsf(floor - link->pos.y) < 1000.0f) {
                link->pos.y = floor;
            }
        }
    }
    if (pG->flags_68 & 0x20000000) {
        Draw_sphere(&newPos, info->rectX, 0xA0A0A0A0, 1, 1);
    }
    return mag;
}

// Same for a model in the air: walls only, the height is kept.
f32 cSatMgr::scrAtCheckSphereAir(cModel* m, cAtariInfo* info, int flag)
{
    Vec pos;
    Vec oldPos;
    Vec newPos;
    Vec mpos;
    f32 mag = 0.0f;
    f32 y;
    cModel* link;

    if (!(info->flags & 0x100)) {
        return 0.0f;
    }
    {
        mpos = m->pos;
        info->getSpeedVector(m, &oldPos, &pos);
        m->wallNrm.x = 0.0f;
        m->wallNrm.y = 0.0f;
        m->wallNrm.z = 0.0f;
        newPos = pos;
        wallAdjust(&m->wallNrm, &oldPos, &newPos, info->rectX, ((cEm*) m)->atari.flags, flag);
        PSVECSubtract(&newPos, &pos, &pos);
        mag = PSVECMag(&pos);
        at_pos_calc(m, &pos);
        y = m->pos.y;
        PSVECAdd(&m->pos, &pos, &m->pos);
        m->pos.y = y;
        link = info->pLink;
        if (link) {
            PSVECSubtract(&m->pos, &mpos, &mpos);
            PSVECAdd(&link->pos, &mpos, &link->pos);
        }
        if (pG->flags_68 & 0x20000000) {
            Draw_sphere(&newPos, info->rectX, 0xA0A0A0A0, 1, 1);
        }
    }
    return mag;
}

// Sphere moving from oldPos to pos against the walls (flag bit0: the segment too); pos is
// pushed out, nrm receives the wall normal.
void cSatMgr::wallAdjust(Vec* nrm, Vec* oldPos, Vec* pos, f32 r, int flag, int mask)
{
    Vec hit;
    Vec tmp;
    Vec n;
    Vec d;
    Vec p0;

    if (flag & 1) {
        if (hitCheck(oldPos, pos, &hit, &n, flag, mask)) {
            PSVECScale(&n, &tmp, r);
            PSVECAdd(&hit, &tmp, pos);
            if (nrm) {
                *nrm = n;
            }
        }
    }
    PSVECSubtract(pos, oldPos, &d);
    p0 = *oldPos;
    PSVECAdd(oldPos, &d, pos);
    polySphereCk(&p0, pos, r, flag | 0xA0, nrm, mask);
    polySphereCk(&p0, pos, r, flag | 0x80, nrm, mask);
    if (flag & 1) {
        if (hitCheck(oldPos, pos, &hit, &n, flag, mask)) {
            PSVECScale(&n, &tmp, r);
            PSVECAdd(&hit, &tmp, pos);
            if (nrm) {
                *nrm = n;
            }
        }
    }
}

void cSatMgr::adjust(Vec* nrm, Vec* oldPos, Vec* pos, f32 r, int flag, int mask)
{
    Vec hit;
    Vec tmp;
    Vec n;
    Vec d;
    Vec p0;

    if (flag & 1) {
        if (hitCheck(oldPos, pos, &hit, &n, flag, mask)) {
            PSVECScale(&n, &tmp, r);
            PSVECAdd(&hit, &tmp, pos);
            if (nrm) {
                *nrm = n;
            }
        }
    }
    PSVECSubtract(pos, oldPos, &d);
    p0 = *oldPos;
    PSVECAdd(oldPos, &d, pos);
    polySphereCk(&p0, pos, r, flag, nrm, mask);
    if (flag & 1) {
        if (hitCheck(oldPos, pos, &hit, &n, flag, mask)) {
            PSVECScale(&n, &tmp, r);
            PSVECAdd(&hit, &tmp, pos);
            if (nrm) {
                *nrm = n;
            }
        }
    }
}

f32 cSatMgr::getFloor(Vec* pos, f32 up, f32 down, u32* attr, int flag)
{
    Vec top;
    Vec bottom;
    Vec hit;

    if (pG->flags_64 & 0x10000000) {
        return 0.0f;
    }
    top.x = pos->x;
    top.y = pos->y + up;
    top.z = pos->z;
    bottom.x = pos->x;
    bottom.y = pos->y - down;
    bottom.z = pos->z;
    if (hitCheck2(&top, &bottom, &hit, attr, 0x40, flag) == 0) {
        return -100000.0f;
    }
    return hit.y;
}

void cSatMgr::log(const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    pLog->vwarn(0, 0, fmt, ap);
}

void polyBitSet(u32 no)
{
    polyBit[no >> 3] |= 1 << (no & 7);
}

int polyBitCk(u32 no)
{
    return polyBit[no >> 3] & (1 << (no & 7));
}

// Segment (centre p, half direction dir, |dir| absDir) against the block's XZ box.
int cSatBlock::lineOverlap(Vec* p, Vec* dir, Vec* absDir)
{
    Vec d;
    Vec ad;

    d.x = (p->x - min.x) - size.x * 0.5f;
    d.z = (p->z - min.z) - size.z * 0.5f;
    ad.x = fabsf(d.x);
    ad.z = fabsf(d.z);
    if (ad.x > absDir->x + size.x * 0.5f) {
        return 0;
    }
    if (ad.z > absDir->z + size.z * 0.5f) {
        return 0;
    }
    if (fabsf(d.x * dir->z - d.z * dir->x) > (size.x * absDir->z + size.z * absDir->x) * 0.5f) {
        return 0;
    }
    return 1;
}

// XZ segment a-b against segment c-d.
static inline int lineCross(Vec* a, Vec* b, Vec* c, Vec* d)
{
    f32 denom = (b->x - a->x) * (d->z - c->z) - (b->z - a->z) * (d->x - c->x);
    f32 ax;
    f32 az;
    f32 t;
    f32 s;

    if (denom == 0.0f) {
        return 0;
    }
    ax = a->x - c->x;
    az = a->z - c->z;
    t = az * (d->x - c->x) - ax * (d->z - c->z);
    if (t < 0.0f) {
        if (denom >= 0.0f) {
            return 0;
        }
        if (t < denom) {
            return 0;
        }
    } else {
        if (denom < 0.0f) {
            return 0;
        }
        if (t > denom) {
            return 0;
        }
    }
    s = az * (b->x - a->x) - ax * (b->z - a->z);
    if (s < 0.0f) {
        if (denom >= 0.0f) {
            return 0;
        }
        if (s < denom) {
            return 0;
        }
    } else {
        if (denom < 0.0f) {
            return 0;
        }
        if (s > denom) {
            return 0;
        }
    }
    return 1;
}

// Sphere of radius r sweeping from a to b against the block's XZ box (expanded by r).
int cSatBlock::hitCheckSphere(Vec* a, Vec* b, f32 r)
{
    Vec c0;
    Vec c1;
    f32 x0 = min.x;
    f32 z0 = min.z;
    f32 cx = (a->x + b->x) * 0.5f;
    f32 cz = (a->z + b->z) * 0.5f;
    f32 hx = fabsf(a->x - b->x) * 0.5f + r;
    f32 hz = fabsf(a->z - b->z) * 0.5f + r;
    f32 x1;
    f32 z1;

    if (x0 + size.x < cx - hx) {
        return 0;
    }
    if (x0 - size.x > cx + hx) {
        return 0;
    }
    if (z0 + size.z < cz - hz) {
        return 0;
    }
    if (z0 - size.z > cz + hz) {
        return 0;
    }
    if (!(a->x < x0 - r || a->x > x0 + size.x + r || a->z < z0 - r || a->z > z0 + size.z + r)) {
        return 1;
    }
    if (!(b->x < x0 - r || b->x > x0 + size.x + r || b->z < z0 - r || b->z > z0 + size.z + r)) {
        return 1;
    }
    x1 = x0 + size.x + r;
    z1 = z0 + size.z + r;
    c0.x = x0;
    c0.y = 0.0f;
    c0.z = z0;
    c1.x = x1;
    c1.y = 0.0f;
    c1.z = z0;
    if (lineCross(a, b, &c0, &c1)) {
        return 1;
    }
    c0.z = c1.z = z1;
    if (lineCross(a, b, &c0, &c1)) {
        return 1;
    }
    c0.z = z0;
    c1.x = x0;
    if (lineCross(a, b, &c0, &c1)) {
        return 1;
    }
    c1.x = c0.x = x1;
    if (lineCross(a, b, &c0, &c1)) {
        return 1;
    }
    return 0;
}

cSatMgr::cSatMgr() : cManager<cSat>(sizeof(cSat), 2)
{
    setName("cSatMgr");
}

cEatMgr::cEatMgr()
{
    setName("cEatMgr");
}

void cEatMgr::initEffInfo()
{
    int i;

    for (i = 0; i < 8; i++) {
        memclr_asm(&effInfo[i], sizeof(AtEffInfo));
        effInfo[i].eff0[0] = 0xD2;
        effInfo[i].eff13[0] = 0xD2;
        effInfo[i].eff16[0] = 0xD2;
        effInfo[i].eff17[0] = 0xD2;
        effInfo[i].effGun[0] = 0xD2;
        effInfo[i].eff5[0] = 0xD2;
        effInfo[i].eff6[0] = 0xD2;
        effInfo[i].eff0D[0] = 0xD2;
        effOn[i] = 0;
    }
}

// Register the effect ids of one type; pairs left at the (0xD2, 1) default are not copied.
void cEatMgr::registEffInfo(int type, AtEffInfo* src)
{
    effOn[type] = 1;
    effInfo[type].flags = src->flags;
    if (src->eff0[0] != 0xD2 || src->eff0[1] == 1) {
        effInfo[type].eff0[0] = src->eff0[0];
        effInfo[type].eff0[1] = src->eff0[1];
    }
    if (src->eff13[0] != 0xD2 || src->eff0[1] == 1) {
        effInfo[type].eff13[0] = src->eff13[0];
        effInfo[type].eff13[1] = src->eff13[1];
    }
    if (src->eff16[0] != 0xD2 || src->eff0[1] == 1) {
        effInfo[type].eff16[0] = src->eff16[0];
        effInfo[type].eff16[1] = src->eff16[1];
    }
    if (src->eff17[0] != 0xD2 || src->eff0[1] == 1) {
        effInfo[type].eff17[0] = src->eff17[0];
        effInfo[type].eff17[1] = src->eff17[1];
    }
    if (src->effGun[0] != 0xD2 || src->eff0[1] == 1) {
        effInfo[type].effGun[0] = src->effGun[0];
        effInfo[type].effGun[1] = src->effGun[1];
    }
    if (src->eff5[0] != 0xD2 || src->eff0[1] == 1) {
        effInfo[type].eff5[0] = src->eff5[0];
        effInfo[type].eff5[1] = src->eff5[1];
    }
    if (src->eff6[0] != 0xD2 || src->eff0[1] == 1) {
        effInfo[type].eff6[0] = src->eff6[0];
        effInfo[type].eff6[1] = src->eff6[1];
    }
    if (src->eff0D[0] != 0xD2 || src->eff0[1] == 1) {
        effInfo[type].eff0D[0] = src->eff0D[0];
        effInfo[type].eff0D[1] = src->eff0D[1];
    }
}

AtEffInfo* cEatMgr::getEffInfo(int type)
{
    if (effOn[type] != 0) {
        return &effInfo[type];
    }
    return 0;
}

void cEatMgr::log(const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    pLog->vwarn(0, 0, fmt, ap);
}

cSat* cSatMgr::create(void* data, int flag, Vec* pos, Vec* rot, u8 type)
{
    cSat* sat = cManager<cSat>::create();
    cSatHeader* hdr = (cSatHeader*) data;

    if (!VALID_PTR(sat)) {
        return 0;
    }
    sat->x5C = 0;
    if (hdr->id != 0xFF && (hdr->id & 0x80)) {
        data = hdr->getSat(type);
    }
    sat->init((cSatFile*) data, pos ? pos : (Vec*) &vecZero, rot ? rot : (Vec*) &vecZero);
    return sat;
}

cSat* cSatMgr::create(Vec* pos, Vec* rot, Vec* poly, int attr, int flag, f32 h)
{
    cSatFile* f;
    cSat* sat;

    if (flag & 0x200) {
        f = createFloorSat(poly, attr, h);
    } else if (flag & 0x100) {
        f = createBoxSat(poly, attr, h);
    } else {
        f = createSat(poly, attr, h);
    }
    if (f == 0) {
        return 0;
    }
    sat = create(f, flag, pos, rot, 0);
    if (sat) {
        sat->flags |= 2;
    } else {
        Mem_free(f);
    }
    return sat;
}

// Sphere of radius r moving from oldPos to pos against every active piece; pos is pushed out
// of the polygons, nrm (when given) receives the last hit normal. Returns 1 on a hit.
int cSatMgr::polySphereCk(Vec* oldPos, Vec* pos, f32 r, int flag, Vec* nrm, int mask)
{
    int ret;
    u32 idx = 0;
    u32 i;

    if (pG->debug_mode == 0x14) {
        Vec d;
        PSVECSubtract(oldPos, pos, &d);
        idx = (u32) (PSVECMag(&d) / 1000.0f);
        if (idx > 0x14) {
            idx = 0x13;
        }
        g_at2_total++;
        g_at2_cnt[idx]++;
        PPCMtpmc1(0);
        PPCMtpmc2(0);
        PPCMtpmc3(0);
        PPCMtpmc4(0);
        PPCMtmmcr1(0x78000000);
        PPCMtmmcr0(0x42);
    }
    ret = 0;
    for (i = 0; i < nArray; i++) {
        cSat* sat = (cSat*) ((u8*) pArray + size * i);
        if (sat->isAlive()) {
            Vec lo;
            Vec lp;
            cSatBlock* blk = sat->block;
            memclr_asm(polyBit, (sat->nPoly + 7) / 8);
            PSMTXMultVec(sat->inv, oldPos, &lo);
            PSMTXMultVec(sat->inv, pos, &lp);
            if (blkPolySphereCk(sat, blk, &lo, &lp, r, flag, nrm, mask)) {
                PSMTXMultVec(sat->mat, &lp, pos);
                if (nrm) {
                    PSMTXMultVecSR(sat->mat, nrm, nrm);
                }
                ret = 1;
            }
        }
    }
    if (pG->debug_mode == 0x14) {
        PPCMtmmcr0(0);
        PPCMtmmcr1(0);
        g_at2_cyc[idx] += PPCMfpmc1() / 1000;
        g_at2_total_cyc += PPCMfpmc1() / 1000;
    }
    return ret;
}

int blkPolySphereCk(cSat* sat, cSatBlock* blk, Vec* a, Vec* b, f32 r, int flag, Vec* nrm, int mask)
{
    int ret = 0;
    int hit;

    while (blk) {
        if (blk->hitCheckSphere(a, b, r)) {
            if (blk->flag & 1) {
                hit = blkPolySphereCk(sat, (cSatBlock*) blk->idx, a, b, r, flag, nrm, mask);
            } else {
                hit = blkPolySphereCkCore(sat, blk, a, b, r, flag, nrm, mask);
            }
            if (hit) {
                ret = 1;
            }
        }
        blk = blk->next;
    }
    return ret;
}

int blkPolySphereCkCore(cSat* sat, cSatBlock* blk, Vec* a, Vec* b, f32 r, int flag, Vec* nrm, int mask)
{
    int ret = 0;
    int start;
    int end;
    int i;
    u16* idx;

    if (flag & 0x40) {
        start = 0;
        end = blk->n0 + blk->n1;
    } else if (flag & 0x80) {
        start = blk->n0 + blk->n1;
        end = start + blk->n2;
    } else {
        start = 0;
        end = blk->n0 + blk->n1 + blk->n2;
    }
    idx = &blk->idx[start];
    for (i = start; i < end; i++, idx++) {
        AtPoly* poly = &sat->poly[*idx];
        if (polyBitCk(*idx)) {
            continue;
        }
        polyBitSet(*idx);
        if (At_poly_sphere_ck((AtPolyData*) sat, poly, a, b, r, flag, mask)) {
            ret = 1;
            if (nrm) {
                *nrm = sat->nrm[sat->poly[*idx].n];
            }
            if (pG->flags_60 & 0x08000000) {
                sat->disp(*idx, 0x40FF0000, 1);
            }
        }
    }
    return ret;
}

int cSatMgr::hitCheck(Vec* a, Vec* b, Vec* hit, Vec* nrm, int flag, int mask)
{
    u32 pn;
    int ret;

    ret = hitCheck2(a, b, hit, &pn, flag, mask);
    if (nrm && ret) {
        PSMTXMultVecSR(pBypassAt->mat, (Vec*) pn, nrm);
    }
    return ret;
}

// Segment a-b against every active piece. The nearest hit goes to hit (world) and `attr`
// receives the address of the hit polygon's normal in the piece's space; b is moved onto the
// piece's grid (mat * inv * b). Returns the attribute word of the hit polygon or 0.
int cSatMgr::hitCheck2(Vec* a, Vec* b, Vec* hit, u32* attr, int flag, int mask)
{
    Vec cur;
    Vec la;
    Vec lb;
    Vec lcur;
    Vec tmp;
    u32 pn;
    int ret = 0;
    u32 i;

    // stored through a struct view: keeps the `cur = *b` loads below the store like the original
    ((SEckView*) &SEck)->v = seCk;
    cur = *b;
    for (i = 0; i < nArray; i++) {
        cSat* sat = (cSat*) ((u8*) pArray + size * i);
        if (sat->isAlive()) {
            cSatBlock* blk = sat->block;
            int r;
            memclr_asm(polyBit, (sat->nPoly >> 3) + 1);
            PSMTXMultVec(sat->inv, a, &la);
            PSMTXMultVec(sat->inv, b, &lb);
            PSMTXMultVec(sat->inv, &cur, &lcur);
            r = blkPolyLineCk(sat, blk, &la, &lb, flag, mask, &lcur, &pn);
            if (r) {
                PSMTXMultVec(sat->inv, &cur, &tmp);
                if (GetDistance(&la, &lcur) < GetDistance(&la, &tmp)) {
                    PSMTXMultVec(sat->mat, &lb, b);
                    ret = r;
                    PSMTXMultVec(sat->mat, &lcur, &cur);
                    pBypassAt = sat;
                }
            }
        }
    }
    if (hit) {
        *hit = cur;
    }
    if (ret && attr) {
        *attr = pn;
    }
    return ret;
}

int blkPolyLineCk(cSat* sat, cSatBlock* blk, Vec* a, Vec* b, int flag, int mask, Vec* hit, u32* pn)
{
    static int new_line_check = 1;
    Vec mid;
    Vec dir;
    Vec adir;
    int ret = 0;
    int r;

    PSVECAdd(a, b, &mid);
    PSVECScale(&mid, &mid, 0.5f);
    PSVECSubtract(a, &mid, &dir);
    adir.x = fabsf(dir.x);
    adir.y = fabsf(dir.y);
    adir.z = fabsf(dir.z);
    adir.y = 0.0f;
    dir.y = 0.0f;
    mid.y = 0.0f;
    while (blk) {
        if (new_line_check == 0) {
            if (blk->hitCheckSphere(a, b, 0.0f)) {
                if (blk->flag & 1) {
                    r = blkPolyLineCk(sat, (cSatBlock*) blk->idx, a, b, flag, mask, hit, pn);
                    if (r) {
                        ret = r;
                    }
                } else {
                    r = blkPolyLineCkCore(sat, blk, a, b, flag, mask, hit, pn);
                    if (r) {
                        ret = r;
                    }
                }
            }
        } else {
            if (blk->lineOverlap(&mid, &dir, &adir)) {
                if (blk->flag & 1) {
                    r = blkPolyLineCk(sat, (cSatBlock*) blk->idx, a, b, flag, mask, hit, pn);
                    if (r) {
                        ret = r;
                    }
                } else {
                    r = blkPolyLineCkCore(sat, blk, a, b, flag, mask, hit, pn);
                    if (r) {
                        ret = r;
                    }
                }
            }
        }
        blk = blk->next;
    }
    return ret;
}

int blkPolyLineCkCore(cSat* sat, cSatBlock* blk, Vec* a, Vec* b, int flag, int mask, Vec* hit, u32* pn)
{
    Vec h;
    int ret = 0;
    int start;
    int end;
    int n;
    u16* idx;

    if (flag & 0x40) {
        start = 0;
        end = blk->n0 + blk->n1;
    } else if (flag & 0x80) {
        start = blk->n0 + blk->n1;
        end = start + blk->n2;
    } else {
        start = 0;
        end = blk->n0 + blk->n1 + blk->n2;
    }
    n = end - start;
    idx = &blk->idx[start];
    idx--;
    while (n--) {
        u32 no;
        AtPoly* poly;
        u32 bit;
        u32 attr;
        idx++;
        no = *idx;
        poly = &sat->poly[no];
        bit = 1 << (no & 7);
        if (polyBit[no >> 3] & bit) {
            continue;
        }
        polyBit[no >> 3] |= bit;
        attr = At_poly_line_ck((AtPolyData*) sat, &h, poly, a, b, flag, mask);
        if (attr) {
            if (GetDistance(a, &h) < GetDistance(a, hit)) {
                *hit = h;
                ret = attr;
                if (pn) {
                    *pn = (u32) &sat->nrm[sat->poly[*idx].n];
                }
            }
        }
    }
    return ret;
}

void cSatMgr::destroy(cSat* p)
{
    if (!VALID_PTR(p)) {
        pLog->err(0, 0, "cSatMgr::destroy() PTR ERR 0x%08x", p);
        return;
    }
    if (p->flags & 2) {
        Mem_free(p->pFile);
    }
    cManager<cSat>::destroy(p);
}

int cSatMgr::construct(cSat* p, u32 id)
{
    // the alive flag before, the active flag after the constructor: keeps the vptr store last
    p->be_flag = 1;
    new (p) cSat();
    p->flags = 0;
    return 1;
}

// Debug draw: flag low nibble selects the polygon group, bits 24-31 an attribute bit to highlight.
void cSatMgr::disp(int flag)
{
    u32 i;
    u32 sel;

    GXSetLineWidth(6, 0);
    sel = (flag >> 8) & 0xFF0000;
    for (i = 0; i < nArray; i++) {
        cSat* sat = (cSat*) ((u8*) pArray + size * i);
        int s;
        int e;
        int j;
        if (!VALID_PTR(sat)) {
            continue;
        }
        if (!sat->isAlive()) {
            continue;
        }
        s = 0;
        e = 0;
        switch ((u32) flag & 0xF) {
        case 0:
            s = 0;
            e = sat->nPoly;
            break;
        case 1:
            s = 0;
            e = sat->nA;
            break;
        case 2:
            s = sat->nA;
            e = s + sat->nB;
            break;
        case 3:
            s = sat->nPoly - sat->nC;
            e = sat->nPoly;
            break;
        }
        for (j = s; j < e; j++) {
            AtPoly* poly = (AtPoly*) (j * sizeof(AtPoly) + (u32) sat->poly);
            u32 attr = (poly->attrHi & 0xFF) << 16;
            attr |= poly->attrLo;
            u32 color;
            int z;
            if (attr != 0) {
                color = attr | 0x40000000;
                if (sel != 0) {
                    color = 0;
                    if (attr & (1 << sel)) {
                        color = 0x80808080;
                    }
                }
                z = 1;
            } else {
                color = 0xA0A0A0A0;
                if (sel == 0) {
                    color = 0xFFFFFFFF;
                }
                z = 0;
            }
            sat->disp(j, color, z);
        }
    }
}

void cSat::init(cSatFile* f, Vec* pos, Vec* rot)
{
    if (!VALID_PTR(f)) {
        pLog->err(0, 0, "cSat::init() PTR ERR %08X", f);
        return;
    }
    if (!f->dataCheck()) {
        pLog->err(0, 0, "ATARI DATA ERROR 0x%08x", f);
    }
    flags = 4;
    *this = f;
    setCoord(pos, rot);
    blockInit(block);
}

void cSat::setCoord(Vec* pos, Vec* rot)
{
    RotMatrix(mat, rot);
    TransMatrix(mat, pos);
    PSMTXInverse(mat, inv);
}

void cSat::setMatrix(Mtx m)
{
    memcpy(mat, m, sizeof(Mtx));
    PSMTXInverse(mat, inv);
}

cSat& cSat::operator=(cSatFile* f)
{
    Vec* v;

    nVertex = f->nVertex;
    nPoly = f->nPoly;
    nNormal = f->nNormal;
    nEdge = f->nEdge;
    nA = f->nA;
    nB = f->nB;
    nC = f->nC;
    x26 = f->x12;
    v = f->getVertexPtr();
    pFile = f;
    x5C = 0;
    vtx = v;
    nrm = v + nVertex;
    edge = nrm + nNormal;
    poly = (AtPoly*) (edge + nEdge);
    block = (cSatBlock*) (poly + nPoly);
    return *this;
}

// Turn the relative block links of the file into pointers (once).
void cSat::blockInit(cSatBlock* blk)
{
    if (!VALID_PTR(blk)) {
        pLog->err(0, 0, "cSat::blockInit() INVALID PTR 0x%08x", blk);
        return;
    }
    if (VALID_PTR(blk->next)) {
        return;
    }
    do {
        if (blk->flag & 1) {
            blockInit((cSatBlock*) blk->idx);
        }
        {
            u32 ofs = (u32) blk->next;
            if (ofs != 0) {
                cSatBlock* p = (cSatBlock*) ((u8*) blk + ofs);
                if (p != 0 && !VALID_PTR(p)) {
                    pLog->err(0, 0, "cSat::blockInit() INVALID PTR 0x%08x ( %08x )", p, ofs);
                    return;
                }
                blk->next = p;
            }
        }
        blk = blk->next;
    } while (blk);
}

// Debug draw of polygon `no`: outline (zupd bits 0-1 == 0, edges flagged in e[0] bits 13-15
// in grey) or filled (== 1), plus the face normal (white when it faces the camera).
void cSat::disp(int no, u32 color, int zupd)
{
    Vec p[3];
    Mtx m;
    Vec n;
    Vec w;
    AtPoly* pt = poly;
    Vec* vt = vtx;
    u16 i;

    PSMTXConcat(pG->Cam.viewMat, mat, m);
    for (i = 0; i < 3; i++) {
        AtPoly* pl = (AtPoly*) (no * sizeof(AtPoly) + (u32) pt);
        Vec* v = (Vec*) (*(u16*) (i * 2 + (u32) pl) * sizeof(Vec) + (u32) vt);
        p[i].x = v->x;
        p[i].y = v->y;
        p[i].z = v->z;
    }
    switch (zupd & 3) {
    case 0: {
        AtPoly* pl = (AtPoly*) (no * sizeof(AtPoly) + (u32) pt);
        Draw_line3d_local(&p[0], &p[1], m, (pl->e[0] & 0x2000) ? 0x80808080 : color, 0);
        Draw_line3d_local(&p[1], &p[2], m, (pl->e[0] & 0x4000) ? 0x80808080 : color, 0);
        Draw_line3d_local(&p[0], &p[2], m, (pl->e[0] & 0x8000) ? 0x80808080 : color, 0);
        break;
    }
    case 1:
        Draw_poly_local(p, m, color, 1);
        break;
    }
    PSVECAdd(&p[0], &p[1], &p[0]);
    color = 0xFF;
    PSVECAdd(&p[0], &p[2], &p[0]);
    PSVECScale(&p[0], &p[0], 1.0f / 3.0f);
    {
        AtPoly* pl = (AtPoly*) (no * sizeof(AtPoly) + (u32) poly);
        n.x = nrm[pl->n].x;
        n.y = nrm[pl->n].y;
        n.z = nrm[pl->n].z;
    }
    PSVECScale(&n, &p[1], 100.0f);
    PSVECAdd(&p[0], &p[1], &p[1]);
    PSMTXMultVec(mat, &p[0], &w);
    PSVECSubtract(&w, &pG->Cam.param.pos, &w);
    PSMTXMultVecSR(mat, &n, &n);
    if (PSVECDotProduct(&n, &w) > 0.0f) {
        color = 0xFFFFFFFF;
    }
    Draw_line3d_local(&p[0], &p[1], m, color, 0);
}

Vec* cSatFile::getVertexPtr()
{
    return (Vec*) (this + 1);
}

int cSatFile::dataCheck()
{
    return nPoly <= 0x1FFF;
}

cSatFile* cSatHeader::getSat(int no)
{
    u32* tbl = ofs;

    return (cSatFile*) ((u8*) this + *(u32*) (no * 4 + (u32) tbl));
}

// The three builders below with precomputed normals were dead-stripped by the linker; their
// tables, strings and constant pools stayed in .rodata.
static cSatFile* createSat2(cSat* sat, Vec* v, u32 attr, f32 h)
{
    static const AtPoly poly0[8] = {
        { { 5, 4, 1 }, 0, { 0, 1, 2 } },
        { { 4, 0, 1 }, 0, { 3, 4, 5 } },
        { { 6, 7, 3 }, 1, { 6, 7, 8 } },
        { { 3, 2, 6 }, 1, { 9, 10, 11 } },
        { { 7, 5, 1 }, 2, { 12, 13, 14 } },
        { { 1, 3, 7 }, 2, { 15, 16, 17 } },
        { { 4, 6, 2 }, 3, { 18, 19, 20 } },
        { { 2, 0, 4 }, 3, { 21, 22, 23 } },
    };
    static const Vec norm0[6] = {
        { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { -1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },  { 0.0f, -1.0f, 0.0f },
    };
    cSatFile* f;
    Vec* vtx;
    Vec* nrm;
    u32 i;

    if (!VALID_PTR(sat)) {
        pLog->err(0, 0, "createSat() INVALID PTR %08X", sat);
        return 0;
    }
    f = (cSatFile*) MEM_ALLOC(0x298, 1, 13);
    if (h == 0.0f) {
        return 0;
    }
    vtx = (Vec*) (f + 1);
    nrm = &vtx[8];
    for (i = 0; i < 6; i++) {
        nrm[i].x = norm0[i].x * 1.1f;
        nrm[i].y = norm0[i].y * 0.1f;
        nrm[i].z = norm0[i].z * 2.2f;
    }
    memcpy(&nrm[6], poly0, sizeof(poly0));
    return f;
}

static cSatFile* createBoxSat2(cSat* sat, Vec* v, u32 attr, f32 h)
{
    static const AtPoly poly0[12] = {
        { { 1, 0, 2 }, 4, { 0, 1, 2 } },
        { { 3, 1, 2 }, 4, { 3, 4, 5 } },
        { { 5, 4, 1 }, 0, { 6, 7, 8 } },
        { { 4, 0, 1 }, 0, { 9, 10, 11 } },
        { { 6, 7, 3 }, 1, { 12, 13, 14 } },
        { { 3, 2, 6 }, 1, { 15, 16, 17 } },
        { { 7, 5, 1 }, 2, { 18, 19, 20 } },
        { { 1, 3, 7 }, 2, { 21, 22, 23 } },
        { { 4, 6, 2 }, 3, { 24, 25, 26 } },
        { { 2, 0, 4 }, 3, { 27, 28, 29 } },
        { { 4, 5, 6 }, 5, { 30, 31, 32 } },
        { { 7, 6, 5 }, 5, { 33, 34, 35 } },
    };
    static const Vec norm0[6] = {
        { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { -1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },  { 0.0f, -1.0f, 0.0f },
    };
    cSatFile* f;
    Vec* vtx;
    Vec* nrm;
    u32 i;

    if (!VALID_PTR(sat)) {
        pLog->err(0, 0, "createSat() INVALID PTR %08X", sat);
        return 0;
    }
    f = (cSatFile*) MEM_ALLOC(0x398, 1, 13);
    if (h == 0.0f) {
        return 0;
    }
    vtx = (Vec*) (f + 1);
    nrm = &vtx[8];
    for (i = 0; i < 6; i++) {
        nrm[i].x = norm0[i].x * 1.1f;
        nrm[i].y = norm0[i].y * 0.1f;
        nrm[i].z = norm0[i].z * 2.2f;
    }
    memcpy(&nrm[6], poly0, sizeof(poly0));
    return f;
}

static cSatFile* createFloorSat2(cSat* sat, Vec* v, u32 attr, f32 h)
{
    static const AtPoly poly0[2] = {
        { { 1, 0, 2 }, 0, { 0, 1, 2 } },
        { { 3, 1, 2 }, 0, { 3, 4, 5 } },
    };
    static const Vec norm0[1] = {
        { 0.0f, 1.0f, 0.0f },
    };
    static int floor_check = 0;
    cSatFile* f;
    Vec* vtx;
    Vec* nrm;
    u32 i;

    if (!VALID_PTR(sat)) {
        pLog->err(0, 0, "createSat() INVALID PTR %08X", sat);
        return 0;
    }
    f = (cSatFile*) MEM_ALLOC(0xE8, 1, 13);
    if (h == 0.0f || floor_check) {
        return 0;
    }
    vtx = (Vec*) (f + 1);
    nrm = &vtx[4];
    for (i = 0; i < 1; i++) {
        nrm[i].x = norm0[i].x * 1.1f;
        nrm[i].y = norm0[i].y * 0.1f;
        nrm[i].z = norm0[i].z * 2.2f;
    }
    memcpy(&nrm[1], poly0, sizeof(poly0));
    return f;
}

// Wall piece over the 4-corner polygon v (closed side box of height h, no top/bottom).
cSatFile* createSat(Vec* v, u32 attr, f32 h)
{
    static const AtPoly poly0[8] = {
        { { 5, 2, 1 }, 0, { 0, 1, 2 } },
        { { 2, 5, 6 }, 0, { 3, 4, 5 } },
        { { 3, 4, 0 }, 1, { 6, 7, 8 } },
        { { 3, 7, 4 }, 1, { 9, 10, 11 } },
        { { 0, 4, 1 }, 2, { 12, 13, 14 } },
        { { 1, 4, 5 }, 2, { 15, 16, 17 } },
        { { 2, 7, 3 }, 3, { 18, 19, 20 } },
        { { 2, 6, 7 }, 3, { 21, 22, 23 } },
    };
    cSatFile* f;
    Vec* vtx;
    Vec* nrm;
    Vec* e;
    AtPoly* poly;
    cSatBlock* blk;
    const AtPoly* p;
    u32 i;

#line 2621 "D:/Bio4/Prog/atari.cpp"
    f = (cSatFile*) MEM_ALLOC(0x298, 1, 13);
    if (!VALID_PTR(f)) {
        pLog->err(0, 0, "createSat() memory alloc failed.");
        return 0;
    }
    f->id = 0xFF;
    f->nVertex = 8;
    f->nNormal = 4;
    f->nEdge = 24;
    f->nPoly = 8;
    f->nA = 0;
    f->nB = 0;
    f->nC = 8;
    f->x12 = 1;
    vtx = (Vec*) (f + 1);
    vtx[0] = v[0];
    vtx[1] = v[1];
    vtx[2] = v[2];
    vtx[3] = v[3];
    vtx[4] = v[0];
    vtx[4].y += h;
    vtx[5] = v[1];
    vtx[5].y += h;
    vtx[6] = v[2];
    vtx[6].y += h;
    vtx[7] = v[3];
    vtx[7].y += h;
    nrm = &vtx[8];
    for (i = 0; i < 4; i++) {
        Vec d0;
        Vec d1;
        Vec* v0;
        p = &poly0[i * 2];
        v0 = &vtx[p->v[0]];
        PSVECSubtract(&vtx[p->v[1]], v0, &d0);
        PSVECSubtract(&vtx[p->v[2]], v0, &d1);
        PSVECCrossProduct(&d0, &d1, &nrm[i]);
#line 2661 "D:/Bio4/Prog/atari.cpp"
        VECNormalize(&nrm[i], &nrm[i]);
    }
    e = &nrm[4];
    for (i = 0; i < 8; i++) {
        Vec* v1 = (Vec*) (poly0[i].v[1] * sizeof(Vec) + (u32) vtx);
        Vec* v0 = (Vec*) (poly0[i].v[0] * sizeof(Vec) + (u32) vtx);
        Vec* v2 = (Vec*) (poly0[i].v[2] * sizeof(Vec) + (u32) vtx);
        e->x = v1->x - v0->x;
        e->y = v1->y - v0->y;
        e->z = v1->z - v0->z;
        e++;
        e->x = v2->x - v1->x;
        e->y = v2->y - v1->y;
        e->z = v2->z - v1->z;
        e++;
        e->x = v0->x - v2->x;
        e->y = v0->y - v2->y;
        e->z = v0->z - v2->z;
        e++;
    }
    poly = (AtPoly*) e;
    memcpy(poly, poly0, sizeof(poly0));
    for (i = 0; i < 8; i++) {
        poly[i].attr = attr;
    }
    blk = (cSatBlock*) (poly + 8);
    blk->min = vtx[0];
    blk->size = vtx[0];
    for (i = 1; i < 4; i++) {
        if (vtx[i].x < blk->min.x) {
            blk->min.x = vtx[i].x - 10.0f;
        }
        if (vtx[i].z < blk->min.z) {
            blk->min.z = vtx[i].z - 10.0f;
        }
        if (vtx[i].x > blk->size.x) {
            blk->size.x = vtx[i].x + 10.0f;
        }
        if (vtx[i].z > blk->size.z) {
            blk->size.z = vtx[i].z + 10.0f;
        }
    }
    blk->size.x -= blk->min.x;
    blk->size.z -= blk->min.z;
    blk->n0 = 0;
    blk->n1 = 0;
    blk->n2 = 8;
    blk->flag = 0;
    blk->next = 0;
    for (i = 0; i < 8; i++) {
        blk->idx[i] = i;
    }
    return f;
}

// Closed box piece over the 4-corner polygon v (height h): floor, walls and ceiling groups.
cSatFile* createBoxSat(Vec* v, u32 attr, f32 h)
{
    static const AtPoly poly0[12] = {
        { { 4, 6, 5 }, 0, { 0, 1, 2 } },
        { { 6, 4, 7 }, 0, { 3, 4, 5 } },
        { { 0, 1, 2 }, 1, { 6, 7, 8 } },
        { { 0, 2, 3 }, 1, { 9, 10, 11 } },
        { { 5, 2, 1 }, 2, { 12, 13, 14 } },
        { { 2, 5, 6 }, 2, { 15, 16, 17 } },
        { { 3, 4, 0 }, 3, { 18, 19, 20 } },
        { { 3, 7, 4 }, 3, { 21, 22, 23 } },
        { { 0, 4, 1 }, 4, { 24, 25, 26 } },
        { { 1, 4, 5 }, 4, { 27, 28, 29 } },
        { { 2, 7, 3 }, 5, { 30, 31, 32 } },
        { { 2, 6, 7 }, 5, { 33, 34, 35 } },
    };
    cSatFile* f;
    Vec* vtx;
    Vec* nrm;
    Vec* e;
    AtPoly* poly;
    cSatBlock* blk;
    const AtPoly* p;
    u32 i;

#line 2757 "D:/Bio4/Prog/atari.cpp"
    f = (cSatFile*) MEM_ALLOC(0x398, 1, 13);
    if (!VALID_PTR(f)) {
        pLog->err(0, 0, "createSat() memory alloc failed.");
        return 0;
    }
    f->id = 0xFF;
    f->nVertex = 8;
    f->nNormal = 6;
    f->nEdge = 36;
    f->nPoly = 12;
    f->nA = 0;
    f->nB = 4;
    f->nC = 8;
    f->x12 = 1;
    vtx = (Vec*) (f + 1);
    vtx[0] = v[0];
    vtx[1] = v[1];
    vtx[2] = v[2];
    vtx[3] = v[3];
    vtx[4] = v[0];
    vtx[4].y += h;
    vtx[5] = v[1];
    vtx[5].y += h;
    vtx[6] = v[2];
    vtx[6].y += h;
    vtx[7] = v[3];
    vtx[7].y += h;
    nrm = &vtx[8];
    for (i = 0; i < 6; i++) {
        Vec d0;
        Vec d1;
        Vec* v0;
        p = &poly0[i * 2];
        v0 = &vtx[p->v[0]];
        PSVECSubtract(&vtx[p->v[1]], v0, &d0);
        PSVECSubtract(&vtx[p->v[2]], v0, &d1);
        PSVECCrossProduct(&d0, &d1, &nrm[i]);
#line 2797 "D:/Bio4/Prog/atari.cpp"
        VECNormalize(&nrm[i], &nrm[i]);
    }
    e = &nrm[6];
    for (i = 0; i < 12; i++) {
        Vec* v1 = (Vec*) (poly0[i].v[1] * sizeof(Vec) + (u32) vtx);
        Vec* v0 = (Vec*) (poly0[i].v[0] * sizeof(Vec) + (u32) vtx);
        Vec* v2 = (Vec*) (poly0[i].v[2] * sizeof(Vec) + (u32) vtx);
        e->x = v1->x - v0->x;
        e->y = v1->y - v0->y;
        e->z = v1->z - v0->z;
        e++;
        e->x = v2->x - v1->x;
        e->y = v2->y - v1->y;
        e->z = v2->z - v1->z;
        e++;
        e->x = v0->x - v2->x;
        e->y = v0->y - v2->y;
        e->z = v0->z - v2->z;
        e++;
    }
    poly = (AtPoly*) e;
    memcpy(poly, poly0, sizeof(poly0));
    for (i = 0; i < 12; i++) {
        poly[i].attr = attr;
    }
    blk = (cSatBlock*) (poly + 12);
    blk->min = vtx[0];
    blk->size = vtx[0];
    for (i = 1; i < 4; i++) {
        if (vtx[i].x < blk->min.x) {
            blk->min.x = vtx[i].x - 10.0f;
        }
        if (vtx[i].z < blk->min.z) {
            blk->min.z = vtx[i].z - 10.0f;
        }
        if (vtx[i].x > blk->size.x) {
            blk->size.x = vtx[i].x + 10.0f;
        }
        if (vtx[i].z > blk->size.z) {
            blk->size.z = vtx[i].z + 10.0f;
        }
    }
    blk->size.x -= blk->min.x;
    blk->size.z -= blk->min.z;
    blk->n0 = 0;
    blk->n1 = 4;
    blk->n2 = 8;
    blk->flag = 0;
    blk->next = 0;
    for (i = 0; i < 12; i++) {
        blk->idx[i] = i;
    }
    return f;
}

// Floor piece: the 4-corner polygon v as two triangles.
static cSatFile* createFloorSat(Vec* v, u32 attr, f32 h)
{
    static const AtPoly poly0[2] = {
        { { 0, 2, 1 }, 0, { 0, 1, 2 } },
        { { 2, 0, 3 }, 0, { 3, 4, 5 } },
    };
    cSatFile* f;
    Vec* vtx;
    Vec* nrm;
    Vec* e;
    AtPoly* poly;
    cSatBlock* blk;
    const AtPoly* p;
    u32 i;

#line 2873 "D:/Bio4/Prog/atari.cpp"
    f = (cSatFile*) MEM_ALLOC(0xE8, 1, 13);
    if (!VALID_PTR(f)) {
        pLog->err(0, 0, "createSat() memory alloc failed.");
        return 0;
    }
    f->id = 0xFF;
    f->nVertex = 4;
    f->nNormal = 1;
    f->nEdge = 6;
    f->nPoly = 2;
    f->nA = 2;
    f->nB = 0;
    f->nC = 0;
    f->x12 = 1;
    vtx = (Vec*) (f + 1);
    vtx[0] = v[0];
    vtx[1] = v[1];
    vtx[2] = v[2];
    vtx[3] = v[3];
    nrm = &vtx[4];
    for (i = 0; i < 1; i++) {
        Vec d0;
        Vec d1;
        Vec* v0;
        p = &poly0[i * 2];
        v0 = &vtx[p->v[0]];
        PSVECSubtract(&vtx[p->v[1]], v0, &d0);
        PSVECSubtract(&vtx[p->v[2]], v0, &d1);
        PSVECCrossProduct(&d0, &d1, &nrm[i]);
#line 2910 "D:/Bio4/Prog/atari.cpp"
        VECNormalize(&nrm[i], &nrm[i]);
    }
    e = &nrm[1];
    for (i = 0; i < 2; i++) {
        Vec* v1 = (Vec*) (poly0[i].v[1] * sizeof(Vec) + (u32) vtx);
        Vec* v0 = (Vec*) (poly0[i].v[0] * sizeof(Vec) + (u32) vtx);
        Vec* v2 = (Vec*) (poly0[i].v[2] * sizeof(Vec) + (u32) vtx);
        e->x = v1->x - v0->x;
        e->y = v1->y - v0->y;
        e->z = v1->z - v0->z;
        e++;
        e->x = v2->x - v1->x;
        e->y = v2->y - v1->y;
        e->z = v2->z - v1->z;
        e++;
        e->x = v0->x - v2->x;
        e->y = v0->y - v2->y;
        e->z = v0->z - v2->z;
        e++;
    }
    poly = (AtPoly*) e;
    memcpy(poly, poly0, sizeof(poly0));
    for (i = 0; i < 2; i++) {
        poly[i].attr = attr;
    }
    blk = (cSatBlock*) (poly + 2);
    blk->min = vtx[0];
    blk->size = vtx[0];
    for (i = 1; i < 4; i++) {
        if (vtx[i].x < blk->min.x) {
            blk->min.x = vtx[i].x - 10.0f;
        }
        if (vtx[i].z < blk->min.z) {
            blk->min.z = vtx[i].z - 10.0f;
        }
        if (vtx[i].x > blk->size.x) {
            blk->size.x = vtx[i].x + 10.0f;
        }
        if (vtx[i].z > blk->size.z) {
            blk->size.z = vtx[i].z + 10.0f;
        }
    }
    blk->size.x -= blk->min.x;
    blk->size.z -= blk->min.z;
    blk->n0 = 2;
    blk->n1 = 0;
    blk->n2 = 0;
    blk->flag = 0;
    blk->next = 0;
    for (i = 0; i < 2; i++) {
        blk->idx[i] = i;
    }
    return f;
}

// Move the model (and its parts' world matrices) by d after a collision push.
void at_pos_calc(cModel* m, Vec* d)
{
    cModel* c = m->pParts;

    if (PSVECMag(d) != 0.0f) {
        PSVECAdd(&m->pos, d, &m->pos);
        while (c) {
            PSVECAdd(&c->worldPos, d, &c->worldPos);
            c->mat[0][3] += d->x;
            c->mat[1][3] += d->y;
            c->mat[2][3] += d->z;
            c = c->pParts;
        }
        m->mat[0][3] = m->pos.x;
        m->mat[1][3] = m->pos.y;
        m->mat[2][3] = m->pos.z;
        ((cEm*) m)->satPos = m->pos;
    } else {
        m->mat[0][3] = m->pos.x;
        m->mat[1][3] = m->pos.y;
        m->mat[2][3] = m->pos.z;
    }
}

cSatMgr SatMgr;
cEatMgr EatMgr;
