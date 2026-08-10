#include "atari.h"
#include "at_sub.h"
#include "math_sub.h"
#include "dbmodule.h"
#include "eprintf.h"
#include "db_log.h"

Vec BoxTmp[16];

#define SQ_DIST(a, b) \
    (((a)->x - (b)->x) * ((a)->x - (b)->x) + ((a)->y - (b)->y) * ((a)->y - (b)->y) + ((a)->z - (b)->z) * ((a)->z - (b)->z))

int At_surface_line_ck(Vec* out, Vec* a, Vec* n, Vec* p0, Vec* p1)
{
    f32 d0;
    f32 d1;

    d0 = At_surface_point_rel(a, n, p0);
    d1 = At_surface_point_rel(a, n, p1);
    if (d0 * d1 < 0.0f) {
        f32 a0 = fabsf(d0);
        f32 a1 = fabsf(d1);
        InterVectorXYZ(out, p0, p1, a1 / (a0 + a1));
        return 1;
    }
    if (out) {
        *out = *p1;
    }
    return 0;
}

int At_poly_point_rel(Vec* poly, Vec* nrm, Vec* p)
{
    int next[3] = { 1, 2, 0 };
    Vec c;
    Vec b;
    Vec a;
    u32 i;

    for (i = 0; i < 3; i++) {
        PSVECSubtract(&poly[next[i]], &poly[i], &a);
        PSVECSubtract(p, &poly[i], &b);
        PSVECCrossProduct(&a, &b, &c);
        if (PSVECDotProduct(&c, nrm) < 0.0f) {
            return 0;
        }
    }
    return 1;
}

int At_box_sphere_ck(Vec* box, Vec* p, f32 r)
{
    static int ptbl[6][3] = {
        { 0, 2, 1 }, { 4, 5, 6 }, { 2, 6, 3 }, { 0, 1, 4 }, { 1, 3, 5 }, { 2, 0, 6 },
    };
    Vec tri[3];
    Vec n;
    f32 d;
    int i;

    for (i = 0; i < 6; i++) {
        tri[0] = box[ptbl[i][0]];
        tri[1] = box[ptbl[i][1]];
        tri[2] = box[ptbl[i][2]];
        Get_normal(tri, &n);
        d = PSVECDotProduct(&n, p) - PSVECDotProduct(&n, &tri[0]);
        if (d > r) {
            return 0;
        }
    }
    return 1;
}

void Get_normal(Vec* tri, Vec* out)
{
    Vec a;
    Vec b;

    PSVECSubtract(&tri[2], &tri[0], &a);
    PSVECSubtract(&tri[1], &tri[0], &b);
    PSVECCrossProduct(&a, &b, out);
    PSVECNormalize(out, out);
}

// Dead-stripped by the original linker (only its constant pool survives in .rodata).
static f32 At_half(f32 v)
{
    return v * 0.5f;
}

u32 AtBoxCapsuleCk3(Vec* box, Vec* p0, f32 r, Vec* p1)
{
    Vec dir;
    Vec p;
    f32 len;
    u32 n;

    if (At_box_sphere_ck(box, p0, r)) {
        return 1;
    }
    if (At_box_sphere_ck(box, p1, r)) {
        return 1;
    }
    PSVECSubtract(p0, p1, &dir);
    len = RootSumSquare3(&dir);
    n = (u32) (len / (r + r)) + 2;
    len = len / (f32) n;
    if (len < 0.01f) {
        return 0;
    }
#line 301 "D:/Bio4/Prog/at_sub.cpp"
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, len);
    p = *p1;
    n--;
    while (n-- != 0) {
        PSVECAdd(&p, &dir, &p);
        if (At_box_sphere_ck(box, &p, r)) {
            return 1;
        }
    }
    return 0;
}

u32 AtSphereCapsuleCk(Vec* c, Vec* p0, f32 r, f32 r2, Vec* p1)
{
    Vec dir;
    Vec p;
    f32 rr;
    f32 len;
    f32 d;   // the two end-point distances through one twice-set local: not tied to the dz chain
    u32 n;

    rr = (r + r2) * (r + r2);
    d = SQ_DIST(c, p0);
    if (d < rr) {
        return 1;
    }
    d = SQ_DIST(c, p1);
    if (d < rr) {
        return 1;
    }
    PSVECSubtract(p0, p1, &dir);
    len = RootSumSquare3(&dir);
    n = (u32) (len / (r2 + r2)) + 2;
    len = len / (f32) n;
    if (len < 0.01f) {
        return 0;
    }
#line 366 "D:/Bio4/Prog/at_sub.cpp"
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, len);
    if (r2 < 0.01f) {
        n = 2;
    }
    p = *p1;
    n--;
    while (n-- != 0) {
        PSVECAdd(&p, &dir, &p);
        if (SQ_DIST(c, &p) < rr) {
            return 1;
        }
    }
    return 0;
}

void AtCapsuleDisp(Vec* p0, Vec* p1, f32 r, u32 color)
{
    Vec dir;
    Vec p;
    f32 len;
    u32 n;

    Draw_sphere(p0, r, color, 1, 1);
    Draw_sphere(p1, r, color, 1, 1);
    PSVECSubtract(p0, p1, &dir);
    len = RootSumSquare3(&dir);
    n = (u32) (len / (r + r)) + 2;
    len = len / (f32) n;
    if (len < 0.01f) {
        return;
    }
#line 418 "D:/Bio4/Prog/at_sub.cpp"
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, len);
    p = *p1;
    n--;
    while (n-- != 0) {
        PSVECAdd(&p, &dir, &p);
        Draw_sphere(&p, r, color, 1, 1);
    }
}

void AtCubeDisp(Mtx m, f32 sx, f32 sy, f32 sz, Vec* pos, u32 color)
{
    Mtx mat;
    Vec c;
    Vec v[8];

    PSMTXMultVec(m, pos, &c);
    PSMTXCopy(m, mat);
    TransMatrix(mat, &c);
    color >>= 8;
    v[0].x = -sx;
    v[0].y = 0.0f;
    v[0].z = sz;
    v[1].x = sx;
    v[1].y = 0.0f;
    v[1].z = sz;
    v[2].x = sx;
    v[2].y = sy;
    v[2].z = sz;
    v[3].x = -sx;
    v[3].y = sy;
    v[3].z = sz;
    v[4].x = -sx;
    v[4].y = 0.0f;
    v[4].z = -sz;
    v[5].x = sx;
    v[5].y = 0.0f;
    v[5].z = -sz;
    v[6].x = sx;
    v[6].y = sy;
    v[6].z = -sz;
    v[7].x = -sx;
    v[7].y = sy;
    v[7].z = -sz;
    PSMTXMultVec(mat, &v[0], &v[0]);
    PSMTXMultVec(mat, &v[1], &v[1]);
    PSMTXMultVec(mat, &v[2], &v[2]);
    PSMTXMultVec(mat, &v[3], &v[3]);
    PSMTXMultVec(mat, &v[4], &v[4]);
    PSMTXMultVec(mat, &v[5], &v[5]);
    PSMTXMultVec(mat, &v[6], &v[6]);
    PSMTXMultVec(mat, &v[7], &v[7]);
    Draw_line3d(&v[0], &v[1], color, 0);
    Draw_line3d(&v[1], &v[2], color, 0);
    Draw_line3d(&v[2], &v[3], color, 0);
    Draw_line3d(&v[3], &v[0], color, 0);
    Draw_line3d(&v[4], &v[5], color, 0);
    Draw_line3d(&v[5], &v[6], color, 0);
    Draw_line3d(&v[6], &v[7], color, 0);
    Draw_line3d(&v[7], &v[4], color, 0);
    Draw_line3d(&v[0], &v[4], color, 0);
    Draw_line3d(&v[1], &v[5], color, 0);
    Draw_line3d(&v[2], &v[6], color, 0);
    Draw_line3d(&v[3], &v[7], color, 0);
}

u32 At_poly_line_ck(AtPolyData* pd, Vec* out, AtPoly* poly, Vec* p0, Vec* p1, u32 flag, u32 mask)
{
    Vec d0;
    Vec d1;
    Vec c;
    Vec a;
    Vec b;
    Vec* vtx = pd->vtx;
    Vec* v0 = &vtx[poly->v[0]];
    Vec* v1;
    Vec* v2;
    Vec* nrm = &pd->nrm[poly->n];
    f32 dp0;
    f32 dp1;
    f32 t;
    f32 s0;
    f32 s1;
    u32 attr;

    d0.x = p0->x - v0->x;
    d0.y = p0->y - v0->y;
    d0.z = p0->z - v0->z;
    d1.x = p1->x - v0->x;
    d1.y = p1->y - v0->y;
    d1.z = p1->z - v0->z;
    dp0 = d0.x * nrm->x + d0.y * nrm->y;
    dp0 += d0.z * nrm->z;
    dp1 = d1.x * nrm->x + d1.y * nrm->y;
    dp1 += d1.z * nrm->z;
    if (dp0 * dp1 > 0.0f) {
        return 0;
    }
    v1 = &vtx[poly->v[1]];
    PSVECSubtract(p1, p0, &a);
    PSVECSubtract(p0, v0, &b);
    PSVECCrossProduct(&pd->edge[poly->e[0]], &a, &c);
    if (PSVECDotProduct(&c, &b) < 0.0f) {
        return 0;
    }
    v2 = &vtx[poly->v[2]];
    PSVECSubtract(p0, v1, &b);
    PSVECCrossProduct(&pd->edge[poly->e[1]], &a, &c);
    if (PSVECDotProduct(&c, &b) < 0.0f) {
        return 0;
    }
    PSVECSubtract(p0, v2, &b);
    PSVECCrossProduct(&pd->edge[poly->e[2]], &a, &c);
    if (PSVECDotProduct(&c, &b) < 0.0f) {
        return 0;
    }
    t = -dp0 / PSVECDotProduct(&a, nrm);
    if (t >= 1.0f || t < 0.0f) {
        return 0;
    }
    s0 = PSVECDotProduct(nrm, p0) - PSVECDotProduct(nrm, v0);
    s1 = PSVECDotProduct(nrm, p1) - PSVECDotProduct(nrm, v0);
    if (s0 * s1 < 0.0f) {
        f32 a0 = fabsf(s0);
        f32 a1 = fabsf(s1);
        InterVectorXYZ(out, p0, p1, a1 / (a0 + a1));
    } else if (out) {
        *out = *p1;
    }
    attr = Get_poly_attr(poly);
    if (SEck == 0) {
        if ((flag & 0x1000) && (attr & 0x400000)) {
            return 0;
        }
        if ((flag & 0x2000) && (attr & 0x4000)) {
            return 0;
        }
        if ((flag & 0x4000) && (attr & 0x40)) {
            return 0;
        }
        if ((attr & 0x400) && !(flag & 0x8000)) {
            return 0;
        }
        if ((flag & 0x800) && (attr & 0x8000)) {
            return 0;
        }
        if ((flag & 0x8000) && (attr & 0x800000)) {
            return 0;
        }
    } else {
        if ((flag & 0x400) && (attr & 0x4000)) {
            return 0;
        }
        if ((flag & 0x800) && (attr & 0x400000)) {
            return 0;
        }
    }
    if (mask & attr) {
        return 0;
    }
    return attr | 0x01000000;
}

// Dead-stripped by the original linker (only its constant pool survives in .rodata).
static f32 At_line_rate(f32 a, f32 b)
{
    if (a == 0.0f) {
        return 1.0f;
    }
    return b / a;
}

u32 At_poly_sphere_ck(AtPolyData* pd, AtPoly* poly, Vec* oldPos, Vec* pos, f32 r, u32 flag, u32 mask)
{
    Vec tri[3];
    Vec n;
    u32 attr;

    tri[0].x = pd->vtx[poly->v[0]].x;
    tri[0].y = pd->vtx[poly->v[0]].y;
    tri[0].z = pd->vtx[poly->v[0]].z;
    tri[1].x = pd->vtx[poly->v[1]].x;
    tri[1].y = pd->vtx[poly->v[1]].y;
    tri[1].z = pd->vtx[poly->v[1]].z;
    tri[2].x = pd->vtx[poly->v[2]].x;
    tri[2].y = pd->vtx[poly->v[2]].y;
    tri[2].z = pd->vtx[poly->v[2]].z;
    n.x = pd->nrm[poly->n].x;
    n.y = pd->nrm[poly->n].y;
    n.z = pd->nrm[poly->n].z;
    attr = Get_poly_attr(poly);
    return At_poly_sphere_ck2(tri, &n, attr, oldPos, pos, r, flag, mask);
}

u32 At_poly_sphere_ck2(Vec* tri, Vec* n, u32 attr, Vec* oldPos, Vec* pos, f32 r, u32 flag, u32 mask)
{
    Vec hp;
    Vec nn;
    f32 d;
    u32 hit;
    u32 i;

    if (PSVECDotProduct(n, oldPos) - PSVECDotProduct(n, tri) < 0.0f) {
        return 0;
    }
    d = PSVECDotProduct(n, pos) - PSVECDotProduct(n, tri);
    {
        // `hit = 99` after the fabsf (a volatile asm, so the `li` cannot move above it); the
        // rim-hit `hit = 1` of the else arm jumps into the surface-hit arm's copy (`goto hit1`),
        // the original's cross-jump survivor.
        f32 ad = fabsf(d);
        hit = 99;
        if (ad > r) {
            Vec hp2;

            if (At_surface_line_ck(&hp, tri, n, oldPos, pos)) {
                hit = 0;
                nn = *n;
                hp2 = hp;
                if (At_poly_point_rel(tri, &nn, &hp2)) {
                hit1:
                    hit = 1;
                }
            } else {
                hit = 0;
            }
        } else {
            Vec v;
            Vec hp2;

            PSVECScale(n, &hp, d);
            PSVECSubtract(pos, &hp, &hp);
            nn = *n;
            hp2 = hp;
            if (At_poly_point_rel(tri, &nn, &hp2) == 0) {
                if (flag & 0x20) {
                    hit = 0;
                } else {
                    for (i = 0; i < 3; i++) {
                        f32 len;
                        f32 dot;
                        if (attr & (1 << (i + 29))) {
                            continue;
                        }
                        PSVECSubtract(&tri[(i + 1) % 3], &tri[i], &v);
                        PSVECSubtract(pos, &tri[i], &nn);
                        len = PSVECSquareMag(&v);
                        dot = PSVECDotProduct(&nn, &v);
                        if (dot < 0.01f) {
                            continue;
                        }
                        if (len < dot) {
                            continue;
                        }
                        PSVECScale(&v, &hp, dot / len);
                        PSVECAdd(&hp, &tri[i], &hp);
                        if (PSVECSquareDistance(pos, &hp) < r * r) {
                            hit = 2;
                            break;
                        }
                    }
                    if (i == 3) {
                        hit = 0;
                    }
                }
            } else {
                goto hit1;
            }
        }
    }
    if (hit != 0) {
        if (SEck == 0) {
            if ((flag & 0x1000) && (attr & 0x400000)) {
                hit = 0;
            } else if ((flag & 0x2000) && (attr & 0x4000)) {
                hit = 0;
            } else if ((flag & 0x4000) && (attr & 0x40)) {
                hit = 0;
            } else if ((attr & 0x400) && !(flag & 0x8000)) {
                hit = 0;
            } else if ((flag & 0x8000) && (attr & 0x800000)) {
                hit = 0;
            } else if ((flag & 0x800) && (attr & 0x8000)) {
                hit = 0;
            }
        } else {
            if ((flag & 0x400) && (attr & 0x4000)) {
                hit = 0;
            } else if ((flag & 0x800) && (attr & 0x400000)) {
                hit = 0;
            }
        }
        if (mask & attr) {
            hit = 0;
        }
    }
    switch (hit) {
    case 0:
        break;
    case 1:
        PSVECScale(n, &nn, r);
        PSVECAdd(&hp, &nn, pos);
        break;
    case 2:
        PSVECSubtract(pos, &hp, &nn);
        if (PSVECDotProduct(&nn, n) > 0.0f) {
            PSVECNormalize(&nn, &nn);
            PSVECScale(&nn, &nn, r * 0.99f);
            PSVECAdd(&hp, &nn, pos);
        }
        break;
    default:
        eprintf(80, 160, 0, 0, "HIT ERROR %d", hit);
        break;
    }
    return hit;
}

// Dead-stripped by the original linker (only its constant pool survives in .rodata).
static f32 At_zero_one(f64 a)
{
    if (a != 0.0) {
        return 1.0f;
    }
    return (f32) a;
}

u32 Get_poly_attr(AtPoly* poly)
{
    return (poly->attrHi << 16) | poly->attrLo;
}

int At_rect_point_ck(Vec* rect, Vec* p)
{
    f32 x0 = rect[0].x;
    f32 z0 = rect[0].z;
    f32 px = p->x;
    f32 pz = p->z;
    f32 x1 = rect[1].x;
    f32 z1 = rect[1].z;
    f32 x3 = rect[3].x;
    f32 z3 = rect[3].z;
    f32 dx;
    f32 dz;
    f32 ex1;
    f32 ez1;
    f32 ex3;
    f32 ez3;

    dx = px - x0;
    dz = pz - z0;
    ex1 = x1 - x0;
    ez1 = z1 - z0;
    ex3 = x3 - x0;
    ez3 = z3 - z0;
    if (ex1 * dz > ez1 * dx || ex3 * dz < ez3 * dx) {
        return 0;
    }
    x0 = rect[2].x;
    z0 = rect[2].z;
    dx = px - x0;
    dz = pz - z0;
    ex1 = x1 - x0;
    ez1 = z1 - z0;
    ex3 = x3 - x0;
    ez3 = z3 - z0;
    if (ex1 * dz < ez1 * dx || ex3 * dz > ez3 * dx) {
        return 0;
    }
    return 1;
}

int At_rect_rect_ck(Vec* ra, Vec* rb)
{
    Vec c;
    int i;

    for (i = 0; i < 4; i++) {
        if (At_rect_point_ck(ra, &rb[i])) {
            return 1;
        }
    }
    for (i = 0; i < 4; i++) {
        if (At_rect_point_ck(rb, &ra[i])) {
            return 1;
        }
    }
    c.x = 0.0f;
    c.y = 0.0f;
    c.z = 0.0f;
    for (i = 0; i < 4; i++) {
        PSVECAdd(&c, ra, &c);
    }
    PSVECScale(&c, &c, 0.25f);
    if (At_rect_point_ck(rb, &c)) {
        return 1;
    }
    return 0;
}

int Get_ang_dir(f32 ang)
{
    if (ang > 2.3561945f || ang < -2.3561945f) {
        return 2;
    }
    if (ang > 0.78539819f) {
        return 1;
    }
    if (ang > 0.78539819f) {
        return 1;
    } else {
        int dir = 0;
        if (ang < -0.78539819f) {
            dir = 3;
        }
        return dir;
    }
}

// Dead-stripped by the original linker (only its rectangle template and constant survive).
static int At_rect_default_ck(Vec* p)
{
    Vec rect[4] = {
        { 0.0f, 0.0f, 5000.0f }, { 5000.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -5000.0f }, { -5000.0f, 0.0f, 0.0f },
    };
    if (p->y > 300.0f) {
        return 0;
    }
    return At_rect_point_ck(rect, p);
}

void InterVectorXYZ(Vec* out, Vec* a, Vec* b, f32 t)
{
    Vec tmp;
    f32 s = 1.0f - t;

    PSVECScale(a, &tmp, t);
    PSVECScale(b, out, s);
    PSVECAdd(out, &tmp, out);
}

int EatGetEffectType(u32 attr)
{
    int type = 0;

    if (attr & 0x800000) {
        type = 1;
    }
    if (attr & 0x8000) {
        type += 2;
    }
    if (attr & 0x80) {
        type += 4;
    }
    return type;
}

f32 At_surface_point_rel(Vec* a, Vec* n, Vec* p)
{
    return PSVECDotProduct(n, p) - PSVECDotProduct(n, a);
}
