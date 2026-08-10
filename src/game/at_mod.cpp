// game/at_mod.cpp: character-to-character collision (see at_mod.h) and the hit box ("yarare") setup.

#include "atari.h"
#include "at_mod.h"
#include "at_sub.h"
#include "emhit.h"
#include "obj.h"
#include "player.h"
#include "global.h"
#include "math_sub.h"
#include "motion.h"
#include "db_log.h"

extern "C" {
void yarareInit0(EmHitInfo* y, f32 x, f32 yy, f32 z, f32 w, f32 h, s16 no, u16 flags);
static int priorityCheck(cEm* a, cEm* b);
static int sphereRectCk(cAtariInfo* info, Vec* p, f32 rad);
// game/em_sub.cpp
int emLineCubeCrossCk(Vec* a, Vec* b, Mtx m, f32 sx, f32 sy, f32 sz, cAtariInfo* info, Vec* hit);
}

// Matrix copy written out as loops. The row counter is a do-while starting at 2 (`i_-- != 0`): the
// original's counter of ComnHitCheck's first copy is live across the getPartsPtr call (callee-saved
// r30), which a `while (i_--)` from 3 cannot give (cse folds the peeled test and re-materialises
// `li 2` after the call). The row pointers step destination first (`d_++; s_++;`): that LUID order
// gives the target's `addi d; addi s` pairs and keeps the second copy's `sp_ = *s_` a separate
// register copy (s_ r0, sp_ r9) -- ComnHitCheck is byte-identical with it.
// ObaLineHitChk (matching): one PSVECMag call squared (`mag * mag`), tc/s clamped with ternaries (s in
// place: its temporary is copied back into f30), den anchored in f0 (anchor gated by `de * ef` so the
// hoisted `mr r3,r27` keeps its slot), and the getPartsPtr `if` written with an explicit `else pm = m`
// so cse's extended block ends at the join and the `&p0` argument after the call stays a fresh
// `addi r4,r1,8` (with `pm = m` hoisted before the `if`, cse skips the arm and folds `&p0` into the
// copy's address pseudo).
#define MTX_COPY(src, dst)               \
    {                                    \
        MtxPtr d_ = (dst);               \
        int i_ = 2;                      \
        MtxPtr s_ = (src);               \
        int j_;                          \
        f32* sp_;                        \
        f32* dp_;                        \
        do {                             \
            dp_ = *d_;                   \
            sp_ = *s_;                   \
            for (j_ = 0; j_ < 4; j_++) { \
                *dp_++ = *sp_++;         \
            }                            \
            d_++;                        \
            s_++;                        \
        } while (i_-- != 0);             \
    }

void yarareInit0(EmHitInfo* y, f32 x, f32 yy, f32 z, f32 w, f32 h, s16 no, u16 flags)
{
    y->ofs.x = x;
    y->ofs.y = yy;
    y->ofs.z = z;
    y->width = w;
    y->height = h;
    y->partsNo = no;
    y->flags = flags;
    y->next = 0;
}

void YarareInit(cEm* em, f32 x, f32 y, f32 z, f32 w, f32 h, s16 no, u16 flags)
{
    yarareInit0(&em->hitInfo, x, y, z, w, h, no, flags);
}

void YarareInitCube(cEm* em, f32 x, f32 y, f32 z, f32 w, f32 h, f32 d, s16 no, u16 flags)
{
    yarareInit0(&em->hitInfo, x, y, z, w, h, no, flags);
    em->hitInfo.depth = d;
    em->hitInfo.flags |= 8;
}

void YarareAdd(cEm* em, EmHitInfo* box, f32 x, f32 y, f32 z, f32 w, f32 h, s16 no, u16 flags)
{
    EmHitInfo* p = &em->hitInfo;

    yarareInit0(box, x, y, z, w, h, no, flags);
    for (; p->next != 0; p = p->next) {
        if (p == box) {
            pLog->err(0, 0, "YarareAdd() Add same pointer, EM%02x", em->id);
            return;
        }
    }
    if (p == box) {
        pLog->err(0, 0, "YarareAdd() Add same pointer, EM%02x", em->id);
        return;
    }
    p->next = box;
}

void YarareAddCube(cEm* em, EmHitInfo* box, f32 x, f32 y, f32 z, f32 w, f32 h, f32 d, s16 no, u16 flags)
{
    EmHitInfo* p;

    yarareInit0(box, x, y, z, w, h, no, flags);
    box->depth = d;
    box->flags |= 8;
    p = &em->hitInfo;
    while (p->next != 0) {
        if (p == box) {
            pLog->err(0, 0, "YarareAdd() Add same pointer, EM%02x", em->id);
        }
        p = p->next;
    }
    if (p->next != box) {
        p->next = box;
    }
}

void EmAtCheck(cEm* em)
{
    cEm* m;

    if (!(em->atari.flags & 0x200) || em->atari.rectZ == 0.0f) {
        em->atari.x26 |= 1;
        return;
    }
    em->atari.getPos(em, &em->atari.worldPos);
    for (m = EmMgr.pAlive; m != 0; m = (cEm*) m->next) {
        if ((m->atari.flags & 0x200) && m->atari.rectZ != 0.0f) {
            m->atari.getPos(m, &m->atari.worldPos);
        }
    }
    for (m = EmMgr.pAlive; m != 0; m = (cEm*) m->next) {
        if ((m->atari.flags & 0x200) && m != em && m->atari.rectZ != 0.0f) {
            __em_at_core(em, m);
        }
    }
    for (m = EmMgr.pAlive; m != 0; m = (cEm*) m->next) {
        if ((m->atari.flags & 0x200) && m->atari.rectZ != 0.0f) {
            m->atari.oldWorldPos = m->atari.worldPos;
        }
    }
    for (m = (cEm*) ObjMgr.pAlive; m != 0; m = (cEm*) m->next) {
        if ((m->atari.flags & 0x200) && m->atari.rectZ != 0.0f) {
            m->atari.getPos(m, &m->atari.worldPos);
        }
    }
    for (m = (cEm*) ObjMgr.pAlive; m != 0; m = (cEm*) m->next) {
        if ((m->atari.flags & 0x200) && m != em && m->atari.rectZ != 0.0f) {
            __em_at_core(em, m);
        }
    }
    for (m = (cEm*) ObjMgr.pAlive; m != 0; m = (cEm*) m->next) {
        if ((m->atari.flags & 0x200) && m->atari.rectZ != 0.0f) {
            m->atari.oldWorldPos = m->atari.worldPos;
        }
    }
    PartsWorldPosCalc(em);
    em->atari.x26 &= ~1;
}

static int priorityCheck(cEm* a, cEm* b)
{
    u8 pa = a->atari.flags & 0x18;
    u8 pb = b->atari.flags & 0x18;

    if (pa != 0 && pa >= pb) {
        return 1;
    }
    return 0;
}

void __em_at_core(cEm* a, cEm* b)
{
    if (priorityCheck(a, b) == 1) {
        return;
    }
    if (a->atari.flags & 2) {
        if (b->atari.flags & 2) {
            At_em_rect_rect_ck(a, b);
        } else {
            At_em_sphere_rect_ck(b, a);
        }
    } else {
        if (b->atari.flags & 2) {
            At_em_sphere_rect_ck(a, b);
        } else {
            At_em_sphere_sphere_ck(a, b);
        }
    }
}

int At_em_rect_rect_ck(cEm* a, cEm* b)
{
    Vec ra;
    Vec rb;
    Vec posA;
    f32 aw;
    f32 ad;
    f32 bw;
    f32 bd;
    u8 pa;
    u8 pb;
    int ret;

    ret = em_rect2_ck_sub(a, b);
    if (ret != 0) {
    posA = a->pos;
    if (!(Get_ang_dir(a->rot.y) & 1)) {
        aw = a->atari.rectX;
        ad = a->atari.rectZ;
    } else {
        aw = a->atari.rectZ;
        ad = a->atari.rectX;
    }
    RotVector(&a->atari.pos, &a->rot, &ra);
    if (!(Get_ang_dir(b->rot.y) & 1)) {
        bw = b->atari.rectX;
        bd = b->atari.rectZ;
    } else {
        bw = b->atari.rectZ;
        bd = b->atari.rectX;
    }
    RotVector(&b->atari.pos, &b->rot, &rb);
    pa = a->atari.flags & 0x18;
    pb = b->atari.flags & 0x18;
    if (pa > pb || (pa == 0 && pb == 0)) {
        if (a->pos.x != a->oldPos.x) {
            f32 ax = a->pos.x + ra.x;
            f32 bx = b->pos.x + rb.x;
            if (ax < bx) {
                b->pos.x = ax + aw + bw + 1.0f - rb.x;
            } else {
                b->pos.x = ax - aw - bw - 1.0f - rb.x;
            }
        } else if (a->pos.z != a->oldPos.z) {
            f32 az = a->pos.z + ra.z;
            f32 bz = b->pos.z + rb.z;
            if (az > bz) {
                b->pos.z = a->pos.z + rb.z + ad + bd + 1.0f - rb.z;
            } else {
                b->pos.z = a->pos.z + rb.z - ad - bd - 1.0f - rb.z;
            }
        }
    } else if (pa != pb) {
        if (a->pos.x != a->oldPos.x) {
            f32 ax = a->pos.x + ra.x;
            f32 bx = b->pos.x + rb.x;
            if (ax > bx) {
                a->pos.x = bx + bw + aw + 1.0f - ra.x;
            } else {
                a->pos.x = bx - bw - aw - 1.0f - ra.x;
            }
        } else if (a->pos.z != a->oldPos.z) {
            f32 az = a->pos.z + ra.z;
            f32 bz = b->pos.z + rb.z;
            if (az > bz) {
                a->pos.z = bz + bd + ad + 1.0f - ra.z;
            } else {
                a->pos.z = bz - bd - ad - 1.0f - ra.z;
            }
        }
    }
    if (fabsf(a->pos.x - posA.x) >= 1.0f || fabsf(a->pos.z - posA.z) >= 1.0f) {
        ret = 1;
    } else {
        ret = 0;
    }
    }
    return ret;
}

int em_rect2_ck_sub(cEm* a, cEm* b)
{
    Vec v;
    Vec ra[4];
    Vec rb[4];
    cAtariInfo* ia = &a->atari;
    cAtariInfo* ib = &b->atari;
    int ret;

    {
        f32 rx = ia->rectX;
        f32 rz = ia->rectZ;
        v.x = rx;
        v.y = 0.0f;
        v.z = rz;
        PSVECAdd(&v, &ia->pos, &v);
        RotVector(&v, &a->rot, &v);
        PSVECAdd(&v, &a->pos, &ra[0]);
        v.x = rx;
        v.y = 0.0f;
        v.z = -rz;
        PSVECAdd(&v, &ia->pos, &v);
        RotVector(&v, &a->rot, &v);
        PSVECAdd(&v, &a->pos, &ra[1]);
        v.x = -rx;
        v.y = 0.0f;
        v.z = -rz;
        PSVECAdd(&v, &ia->pos, &v);
        RotVector(&v, &a->rot, &v);
        PSVECAdd(&v, &a->pos, &ra[2]);
        v.x = -rx;
        v.y = 0.0f;
        v.z = rz;
        PSVECAdd(&v, &ia->pos, &v);
        RotVector(&v, &a->rot, &v);
        PSVECAdd(&v, &a->pos, &ra[3]);
    }
    {
        f32 rx = ib->rectX;
        f32 rz = ib->rectZ;
        v.x = rx;
        v.y = 0.0f;
        v.z = rz;
        PSVECAdd(&v, &ib->pos, &v);
        RotVector(&v, &b->rot, &v);
        PSVECAdd(&v, &b->pos, &rb[0]);
        v.x = rx;
        v.y = 0.0f;
        v.z = -rz;
        PSVECAdd(&v, &ib->pos, &v);
        RotVector(&v, &b->rot, &v);
        PSVECAdd(&v, &b->pos, &rb[1]);
        v.x = -rx;
        v.y = 0.0f;
        v.z = -rz;
        PSVECAdd(&v, &ib->pos, &v);
        RotVector(&v, &b->rot, &v);
        PSVECAdd(&v, &b->pos, &rb[2]);
        v.x = -rx;
        v.y = 0.0f;
        v.z = rz;
        PSVECAdd(&v, &ib->pos, &v);
        RotVector(&v, &b->rot, &v);
        PSVECAdd(&v, &b->pos, &rb[3]);
    }
    if (fabsf(ra[0].y - rb[0].y) > ia->h + ib->h) {
        ret = 0;
    } else if (At_rect_rect_ck(ra, rb) != 0) {
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}

int At_em_sphere_rect_ck(cEm* sph, cEm* rect)
{
    Vec ps;
    Vec pr;
    Mtx m;
    Mtx inv;
    Vec c;
    Vec step;
    Vec p;
    Vec q;
    cAtariInfo* ir;
    f32 rad;
    int n;
    int i;
    int hit;

    ps = sph->atari.worldPos;
    pr = rect->atari.worldPos;
    if (ps.y + sph->atari.h < pr.y - rect->atari.h) {
        return 0;
    }
    if (ps.y - sph->atari.h > pr.y + rect->atari.h) {
        return 0;
    }
    ir = &rect->atari;
    if (ir->partsNo != 0) {
        cModel* pm = rect->getPartsPtr(ir->partsNo - 1);
        PSMTXRotRad(m, 'y', pm->rot.y);
        PSMTXMultVecSR(m, &ir->pos, &c);
        PSVECAdd(&c, &pr, &c);
        TransMatrix(m, &c);
    } else {
        PSMTXRotRad(m, 'y', rect->rot.y);
        TransMatrix(m, &rect->pos);
        PSMTXMultVec(m, &ir->pos, &c);
        m[0][3] = c.x;
        m[1][3] = c.y;
        m[2][3] = c.z;
    }
    PSMTXInverse(m, inv);
    c = sph->atari.worldPos;
    rad = sph->atari.rectX;
    if (!(sph->atari.x26 & 1)) {
        PSMTXMultVec(inv, &sph->atari.worldPos, &step);
        PSMTXMultVec(inv, &sph->atari.oldWorldPos, &p);
        if (rad >= 200.0f) {
            n = (int) (GetDistance3(&step, &p) / rad) + 1;
        } else {
            n = 1;
        }
        PSVECSubtract(&step, &p, &step);
        PSVECScale(&step, &step, 1.0f / (f32) n);
    } else {
        PSMTXMultVec(inv, &sph->atari.worldPos, &p);
        n = 1;
        step.x = 0.0f;
        step.y = 0.0f;
        step.z = 0.0f;
    }
    hit = 0;
    for (i = 0; i < n; i++) {
        PSVECAdd(&p, &step, &p);
        hit = sphereRectCk(ir, &p, rad);
        if (hit != 0) {
            break;
        }
    }
    if (hit != 0) {
        p.y = c.y;
        PSMTXMultVec(m, &p, &q);
        PSVECSubtract(&q, &c, &q);
        q.y = 0.0f;
        PSVECAdd(&sph->pos, &q, &sph->pos);
        sph->atari.getPos(sph, &sph->atari.worldPos);
    }
    return hit;
}

static int sphereRectCk(cAtariInfo* info, Vec* p, f32 rad)
{
    Vec corner;
    Vec d;
    f32 rx = info->rectX;
    f32 rz = info->rectZ;
    f32 rr;
    int hit = 0;

    if (p->z > -rz && p->z < rz) {
        if (p->x > 0.0f) {
            if (p->x < rx + rad) {
                p->x = rx + rad;
                hit = 1;
            }
        } else {
            if (p->x > -(rx + rad)) {
                p->x = -(rx + rad);
                hit = 1;
            }
        }
    }
    if (p->x > -rx && p->x < rx) {
        if (p->z > 0.0f) {
            if (p->z < rz + rad) {
                p->z = rz + rad;
                hit = 1;
            }
        } else {
            if (p->z > -(rz + rad)) {
                p->z = -(rz + rad);
                hit = 1;
            }
        }
    }
    if (hit == 0) {
        corner.x = rx;
        corner.y = 0.0f;
        corner.z = rz;
        p->y = 0.0f;
        rr = rad * rad;
        if (GetDistance(&corner, p) < rr) {
            PSVECSubtract(p, &corner, &d);
#line 788 "D:/Bio4/Prog/at_mod.cpp"
            VECNormalize(&d, &d);
            PSVECScale(&d, &d, rad);
            hit = 1;
            PSVECAdd(&corner, &d, p);
        } else {
            corner.x = -rx;
            corner.y = 0.0f;
            corner.z = rz;
            if (GetDistance(&corner, p) < rr) {
                PSVECSubtract(p, &corner, &d);
#line 797 "D:/Bio4/Prog/at_mod.cpp"
                VECNormalize(&d, &d);
                PSVECScale(&d, &d, rad);
                hit = 1;
                PSVECAdd(&corner, &d, p);
            } else {
                corner.x = rx;
                corner.y = 0.0f;
                corner.z = -rz;
                if (GetDistance(&corner, p) < rr) {
                    PSVECSubtract(p, &corner, &d);
#line 806 "D:/Bio4/Prog/at_mod.cpp"
                    VECNormalize(&d, &d);
                    PSVECScale(&d, &d, rad);
                    hit = 1;
                    PSVECAdd(&corner, &d, p);
                } else {
                    corner.x = -rx;
                    corner.y = 0.0f;
                    corner.z = -rz;
                    if (GetDistance(&corner, p) < rr) {
                        PSVECSubtract(p, &corner, &d);
#line 815 "D:/Bio4/Prog/at_mod.cpp"
                        VECNormalize(&d, &d);
                        PSVECScale(&d, &d, rad);
                        hit = 1;
                        PSVECAdd(&corner, &d, p);
                    }
                }
            }
        }
    }
    return hit;
}

int At_em_sphere_sphere_ck(cEm* a, cEm* b)
{
    Vec pa;
    Vec pb;
    Vec d;
    const f32 rate = 0.3f;
    f32 hh;
    f32 dist;
    f32 rr;
    cModel* m;

    pa = a->atari.worldPos;
    pb = b->atari.worldPos;
    hh = a->atari.h + b->atari.h;
    if (pa.y < pb.y - hh) {
        return 0;
    }
    if (pa.y > pb.y + hh) {
        return 0;
    }
    PSVECSubtract(&pb, &pa, &d);
    d.y = 0.0f;
    dist = RootSumSquare3(&d);
    rr = a->atari.rectZ + b->atari.rectZ;
    if (dist < rr) {
        if (dist < 0.1f) {
            d.x += 0.1f;
        }
#line 869 "D:/Bio4/Prog/at_mod.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, rr - dist);
        PSVECSubtract(&a->pos, &d, &a->pos);
        a->atari.getPos(a, &a->atari.worldPos);
        if (a->atari.pLink != 0) {
            m = a->atari.pLink;
            PSVECSubtract(&m->pos, &d, &m->pos);
            ((cEm*) m)->atari.getPos(m, &((cEm*) m)->atari.worldPos);
        }
        if ((b->atari.flags & 0x18) == 0 && (u32) b > (u32) a) {
            PSVECScale(&d, &d, rate);
            PSVECAdd(&b->pos, &d, &b->pos);
            b->atari.getPos(b, &b->atari.worldPos);
            if (b->atari.pLink != 0) {
                m = b->atari.pLink;
                PSVECAdd(&m->pos, &d, &m->pos);
                ((cEm*) m)->atari.getPos(m, &((cEm*) m)->atari.worldPos);
            }
        }
        return 1;
    }
    return 0;
}

// Dead-stripped by the original linker: only its constant pool (a double 0.0) survives in .rodata
// between At_em_sphere_sphere_ck's and EmHitCheck's pools. Body unknown.
static int atModIsZero(f32 x)
{
    return x == 0.0;
}

int EmHitCheck(Vec* hit, Vec* nrm, Vec* a, Vec* b, int flag)
{
    Vec h;
    Vec n;
    f32 dist = 100000.0f;
    f32 d;
    int ret = 0;
    cEm* m;

    if (hit != 0) {
        *hit = *b;
    }
    for (m = EmMgr.pAlive; m != 0; m = (cEm*) m->next) {
        if (!(m->atari.flags & 0x200)) {
            continue;
        }
        if (!(flag & 4) && m == pPL) {
            continue;
        }
        if (ComnHitCheck(&h, &n, m, a, b, flag) == 0) {
            continue;
        }
        d = GetDistance3(a, &h);
        if (d < dist) {
            dist = d;
            if (hit != 0) {
                *hit = h;
            }
            if (nrm != 0) {
                *nrm = n;
            }
            ret = 1;
        }
    }
    return ret;
}

int ObjHitCheck(Vec* hit, Vec* nrm, Vec* a, Vec* b, int flag)
{
    Vec h;
    Vec n;
    f32 dist = 10000000000.0f;
    f32 d;
    int ret = 0;
    cObj* o;

    if (hit != 0) {
        *hit = *b;
    }
    for (o = ObjMgr.pAlive; o != 0; o = (cObj*) o->next) {
        if ((o->be_flag & 0x201) != 1) {
            continue;
        }
        if (o->id == 2) {
            continue;
        }
        if (ComnHitCheck(&h, &n, (cEm*) o, a, b, flag) == 0) {
            continue;
        }
        d = (a->x - h.x) * (a->x - h.x) + (a->y - h.y) * (a->y - h.y) + (a->z - h.z) * (a->z - h.z);
        if (d < dist) {
            dist = d;
            if (hit != 0) {
                *hit = h;
            }
            if (nrm != 0) {
                *nrm = n;
            }
            ret = 1;
        }
    }
    return ret;
}

int ComnHitCheck(Vec* hit, Vec* nrm, cEm* m, Vec* a, Vec* b, int flag)
{
    Mtx mat;
    int r;

    if (m->atari.flags & 2) {
        if (!(flag & 1)) {
            return 0;
        }
        if (m->atari.partsNo > 0) {
            MTX_COPY(m->getPartsPtr(m->atari.partsNo - 1)->mat, mat);
        } else {
            MTX_COPY(m->mat, mat);
        }
        r = emLineCubeCrossCk(a, b, mat, m->atari.rectX, m->atari.h, m->atari.rectZ, &m->atari, hit);
        if (r != 0) {
            return 1;
        }
    } else if (flag & 2) {
        r = ObaLineHitChk(m, &m->atari, a, b, hit, nrm);
        if (r != 0) {
            return 1;
        }
    }
    if (hit != 0) {
        *hit = *b;
    }
    return 0;
}

void DrawOba(cEm* m)
{
    cAtariInfo* info;

    if (m->atari.flags & 0x200) {
        info = &m->atari;
        do {
            info->disp(m);
            info = info->next;
        } while (info != 0);
    }
}

int ObaLineHitChk(cEm* m, cAtariInfo* info, Vec* a, Vec* b, Vec* hit, Vec* nrm)
{
    Vec p0;
    Vec p1;
    Vec w0;
    Vec w1;
    Vec d;
    Vec e;
    Vec f;
    Vec g;
    Vec h;
    Vec q;
    Vec r;
    Vec n;
    cModel* pm;
    f32 rad;
    f32 dd;
    f32 ee;
    f32 df;
    f32 ef;
    f32 de;
    f32 den;
    f32 t;
    f32 s;
    f32 tc;
    f32 rr;
    f32 depth;
    int parts;

    p1 = info->pos;
    p0 = info->pos;
    p0.y += info->h;
    parts = info->partsNo;
    // COMPILER-DIFF: 12 (cse AROUND path): the target's `&p0` argument is a fresh `addi r4,r1,8` (the
    // copy's address pseudo dies at the copy). With `pm = m; if (parts) pm = ..;` cse skips the arm and
    // carries the copy pseudo into the join, folding `&p0` into it across the getPartsPtr call. The
    // explicit else arm is followed as a TAKEN branch and falls into the join label, which ends the
    // extended block: `&p0` stays a hard-reg argument set (never PRE'd, no longer cse-folded).
    if (parts != 0) {
        pm = m->getPartsPtr(parts - 1);
    } else {
        pm = m;
    }
    rad = info->rectX * 0.75f;
    PSMTXMultVec(pm->mat, &p0, &w0);
    PSMTXMultVec(pm->mat, &p1, &w1);
    PSVECSubtract(&w1, &w0, &d);
    PSVECSubtract(b, a, &e);
    PSVECSubtract(a, &w0, &f);
    dd = PSVECSquareMag(&d);
    ee = PSVECSquareMag(&e);
    df = PSVECDotProduct(&d, &f);
    ef = PSVECDotProduct(&e, &f);
    de = PSVECDotProduct(&d, &e);
    den = dd * ee - de * de;
    // COMPILER-DIFF: 13 (FPR naming): den is settled in f0 before the t numerator is formed. The
    // `de * ef` input makes the anchor ready one cycle later in sched2 (the product's fmuls, not den,
    // gates it), so the `mr r3,r27` hoisted argument takes the slot the codeless anchor used to
    // occupy; the extra `"f"(den)` reference keeps den's local-alloc priority above the product's
    // (which gained a third reference) so den stays in f0 and the product in f13.
    asm("" : "+f"(den) : "f"(den), "f"(de * ef));
    t = (ee * df - de * ef) / den;
    s = (de * df - dd * ef) / den;
    tc = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    // s is clamped in place: the ternary's temporary (f13) is copied back into s (f30) after the join.
    s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
    PSVECScale(&w0, &g, 1.0f - tc);
    rr = rad * rad;
    PSVECScale(&w1, &h, tc);
    PSVECAdd(&g, &h, &q);
    PSVECScale(a, &g, 1.0f - s);
    PSVECScale(b, &h, s);
    PSVECAdd(&g, &h, &r);
    if (PSVECSquareDistance(&q, &r) <= rr) {
        PSVECSubtract(&q, &r, &n);
        {
            f32 mag = PSVECMag(&n);
            depth = SQRTF(rr - mag * mag);
        }
#line 1434 "D:/Bio4/Prog/at_mod.cpp"
        VECNormalize(&e, &n);
        PSVECScale(&n, &n, -depth);
        PSVECAdd(&r, &n, hit);
        PSVECSubtract(hit, &p1, nrm);
        nrm->y = 0.0f;
#line 1440 "D:/Bio4/Prog/at_mod.cpp"
        VECNormalize(nrm, nrm);
        return 1;
    }
    return 0;
}
