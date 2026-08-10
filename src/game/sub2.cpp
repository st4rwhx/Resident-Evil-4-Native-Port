// game/sub2: vector / angle helpers (D:/Bio4/Prog/sub2.cpp).
#include "types.h"
#include "global.h"
#include "atari.h"
#include "model.h"
#include "player.h"
#include "camera.h"
#include "math_sub.h"
#include "gx.h"
#include "db_log.h"

extern "C" {
double atan2(double y, double x);
void GXGetProjectionv(f32* p);
void GXGetViewportv(f32* vp);
void GXProject(f32 x, f32 y, f32 z, const Mtx mtx, const f32* pm, const f32* vp, f32* sx, f32* sy, f32* sz);
}

#define PI2 6.2831855f

#line 30 "D:/Bio4/Prog/sub2.cpp"

int HitCheckPoint4(Vec* p, Vec* quad)
{
    f32 px = p->x - quad[0].x;
    f32 pz = p->z - quad[0].z;
    f32 ax = quad[1].x - quad[0].x;
    f32 az = quad[1].z - quad[0].z;
    f32 bx = quad[3].x - quad[0].x;
    f32 bz = quad[3].z - quad[0].z;
    f32 cx;
    f32 cz;

    if (ax * pz > az * px || bx * pz < bz * px) {
        return 0;
    }
    cx = quad[2].x - quad[0].x;
    cz = quad[2].z - quad[0].z;
    px -= cx;
    pz -= cz;
    ax -= cx;
    az -= cz;
    bx -= cx;
    bz -= cz;
    if (ax * pz < az * px || bx * pz > bz * px) {
        return 0;
    }
    return 1;
}

f32 GetXZAngle(Vec* from, Vec* to)
{
    return LIMIT_ANGLE(atan2f(to->x - from->x, to->z - from->z));
}

f32 GetXYAngle(Vec* from, Vec* to)
{
    f32 dx = to->x - from->x;
    f32 dy = to->y - from->y;
    return LIMIT_ANGLE(atan2f(dy, dx));
}

f32 GetXZAngleLocal(Vec* from, Vec* to, f32 ang)
{
    return LIMIT_ANGLE(GetXZAngle(from, to) - ang);
}

f32 GetDistance(Vec* a, Vec* b)
{
    f32 x = b->x - a->x;
    f32 y = b->y - a->y;
    f32 z = b->z - a->z;
    return x * x + y * y + z * z;
}

f32 GetDistance(Vec& a, Vec& b)
{
    f32 x = b.x - a.x;
    f32 y = b.y - a.y;
    f32 z = b.z - a.z;
    return x * x + y * y + z * z;
}

f32 GetDistanceXZ(Vec* a, Vec* b)
{
    f32 x = b->x - a->x;
    f32 z = b->z - a->z;
    return x * x + z * z;
}

f32 Muku(Vec* pos, Vec* target, f32 ang, f32 limit)
{
    f32 d = LIMIT_ANGLE(GetXZAngle(pos, target) - ang);
    f32 ret;

    if (d > 0.0f) {
        if (d < limit) {
            limit = d;
        }
    } else {
        if (d > -limit) {
            limit = -d;
        }
    }
    ret = limit;
    if (!(d >= 0.0f)) {
        ret = -ret;
    }
    return ret;
}

f32 Muku2(f32 ang, f32 target, f32 limit)
{
    f32 d = target - ang;

    if (d < 0.0f) {
        d += PI2;
    }
    if (d < PI) {
        if (limit > d) {
            limit = d;
        }
    } else {
        d -= PI2;
        if (limit > -d) {
            limit = d;
        } else {
            limit = -limit;
        }
    }
    return limit;
}

f32 Muku3(Vec* dir, f32 ang, f32 limit)
{
    return Muku2(ang, (f32) atan2(dir->x, dir->z), limit);
}

// Dead-stripped in the original (STRIP_UNUSED): only the VECNormalize strings and the 0.0f pools
// survive in .rodata, at this position.
static void sub2_dead1(Vec* v)
{
    VECNormalize(v, v);
}

static int sub2_dead2(f32 x)
{
    if (x > 0.0f) {
        return 1;
    }
    return 0;
}

int Front_check(cModel* a, cModel* b, f32 ang)
{
    f32 d = GetXZAngleLocal(&a->pos, &b->pos, a->rot.y);
    int ret = 0;
    if (!(d < -ang) && !(d > ang)) {
        ret = 1;
    }
    return ret;
}

int Front_check(cModel* a, Vec* b, f32 ang)
{
    f32 d = GetXZAngleLocal(&a->pos, b, a->rot.y);
    int ret = 0;
    if (!(d < -ang) && !(d > ang)) {
        ret = 1;
    }
    return ret;
}

int Front_check(Vec* a, Vec* b, f32 rot, f32 ang)
{
    f32 d = GetXZAngleLocal(a, b, rot);
    int ret = 0;
    if (!(d < -ang) && !(d > ang)) {
        ret = 1;
    }
    return ret;
}

void AddSpeed(cModel* m, const Vec* speed)
{
    Vec v;
    Mtx mtx;

    low_RotMatrix(mtx, &m->rot);
    PSMTXMultVec(mtx, speed, &v);
    m->pos.x += v.x;
    m->pos.y += v.y;
    m->pos.z += v.z;
}

f32 RootSumSquare3(Vec* v)
{
    return SQRTF(v->x * v->x + v->y * v->y + v->z * v->z);
}

f32 GetDistance3(Vec* a, Vec* b)
{
    Vec d;

    d.x = b->x - a->x;
    d.y = b->y - a->y;
    d.z = b->z - a->z;
    return RootSumSquare3(&d);
}

void RotVector(Vec* src, Vec* rot, Vec* dst)
{
    Mtx mtx;

    low_RotMatrix(mtx, rot);
    PSMTXMultVec(mtx, src, dst);
}

void BoxWorldCalc(Vec* src, Vec* dst, Vec* pos, Vec* rot)
{
    Mtx mtx;
    int i;

    RotMatrix(mtx, rot);
    TransMatrix(mtx, pos);
    for (i = 0; i < 8; i++) {
        PSMTXMultVec(mtx, &src[i], &dst[i]);
    }
}

int GetScreenPos(Vec* pos, Vec* scr)
{
    f32 proj[7];
    f32 vp[6];
    Vec cam;

    CameraCurrentProjection();
    GXGetProjectionv(proj);
    GXGetViewportv(vp);
    GXProject(pos->x, pos->y, pos->z, pG->Cam.viewMat, proj, vp, &scr->x, &scr->y, &scr->z);
    PSMTXMultVec(pG->Cam.viewMat, pos, &cam);
    return cam.z < -0.0f;
}

void Get3DPosFrom2D(Vec* out, f32 sx, f32 sy, f32 y)
{
    Camera* cam = &pG->Cam;
    Vec far;
    Vec dir;
    Vec hit;

    CamPos2ScrnVec(&dir, sx, sy);
#line 518 "D:/Bio4/Prog/sub2.cpp"
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, 20000.0f);
    PSVECAdd(&cam->param.pos, &dir, &far);
    if (y == 100000000.0f) {
        if (SatMgr.hitCheck(&cam->param.pos, &far, &hit, 0, 0x40, 0)) {
            *out = hit;
            return;
        }
        y = pPL->pos.y;
    }
    {
        f32 t = (y - cam->param.pos.y) / (far.y - cam->param.pos.y);
        if (t > 0.0f) {
            PSVECScale(&dir, &dir, t);
            PSVECAdd(&cam->param.pos, &dir, out);
        } else {
            *out = far;
        }
    }
}

// Dead-stripped in the original (pools: PI/2, -1, 1, 127 and -1, 1, 0.5, 0).
static s8 sub2_dead3(f32 ang)
{
    f32 v = ang / 1.5707964f;
    if (v < -1.0f) {
        v = -1.0f;
    }
    if (v > 1.0f) {
        v = 1.0f;
    }
    return (s8) (v * 127.0f);
}

static f32 sub2_dead4(f32 x)
{
    if (x < -1.0f) {
        x = -1.0f;
    }
    if (x > 1.0f) {
        x = 1.0f;
    }
    x *= 0.5f;
    if (x == 0.0f) {
        return 0.0f;
    }
    return x;
}

void PosToPos(Vec* a, Vec* b, Vec* out, f32 t)
{
    Vec ta;
    Vec tb;

    PSVECScale(a, &ta, 1.0f - t);
    PSVECScale(b, &tb, t);
    PSVECAdd(&ta, &tb, out);
}

void VecToCamVec(Vec* v, Vec* out)
{
    Mtx mtx;
    Vec t;
    f32 ang;

    t.x = 0.0f;
    t.y = 0.0f;
    t.z = 1.0f;
    PSMTXMultVecSR(pG->Cam.mat, &t, &t);
    ang = atan2f(t.x, t.z);
    t.x = v->x;
    t.y = 0.0f;
    t.z = -v->y;
    PSMTXRotRad(mtx, 'y', ang);
    PSMTXMultVecSR(mtx, &t, out);
}

int LineSphereCrossCk(Vec* a, Vec* b, Vec* c, Vec* out, f32 r)
{
    Vec ab;
    Vec ac;
    Vec p;
    Vec q;
    Vec d1;
    Vec d2;
    f32 r2 = r * r;
    f32 dist2;
    f32 len2;
    f32 t;
    f32 h;

    dist2 = (a->x - c->x) * (a->x - c->x) + (a->y - c->y) * (a->y - c->y) + (a->z - c->z) * (a->z - c->z);
    if (dist2 < r2) {
        if (out) {
            *out = *a;
        }
        return 1;
    }
    len2 = (a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) + (a->z - b->z) * (a->z - b->z);
    if (len2 <= 0.1f) {
        return 0;
    }
    PSVECSubtract(b, a, &ab);
    PSVECSubtract(c, a, &ac);
    t = PSVECDotProduct(&ab, &ac) / len2;
    PSVECScale(&ab, &p, t);
    PSVECAdd(&p, a, &p);
    t = (p.x - c->x) * (p.x - c->x) + (p.y - c->y) * (p.y - c->y) + (p.z - c->z) * (p.z - c->z);
    if (t >= r2) {
        return 0;
    }
    h = SQRTF(r2 - t);
    PSVECSubtract(a, &p, &q);
    PSVECScale(&q, &q, h / PSVECMag(&q));
    PSVECAdd(&q, &p, &q);
    dist2 = (a->x - q.x) * (a->x - q.x) + (a->y - q.y) * (a->y - q.y) + (a->z - q.z) * (a->z - q.z);
    if (dist2 > len2) {
        return 0;
    }
    PSVECSubtract(a, &q, &d1);
    PSVECSubtract(a, b, &d2);
    if (PSVECDotProduct(&d1, &d2) < 0.0f) {
        return 0;
    }
    PSVECSubtract(b, &q, &d1);
    PSVECSubtract(b, a, &d2);
    if (PSVECDotProduct(&d1, &d2) < 0.0f) {
        return 0;
    }
    if (out) {
        *out = q;
    }
    return 1;
}

int SphereHitCk(Vec* a, Vec* b, f32 ra, f32 rb)
{
    Vec d;
    f32 r;

    PSVECSubtract(a, b, &d);
    r = ra + rb;
    return d.x * d.x + d.y * d.y + d.z * d.z < r * r;
}

void CalcParabolaVector(Vec* out, Vec* from, Vec* to, f32 h)
{
    f32 g = 20.0f;
    f32 t;

    if (h <= 0.0f) {
        PSVECSubtract(to, from, out);
        return;
    }
    if (from->y > to->y) {
        h += from->y;
    } else {
        h += to->y;
    }
    t = SQRTF((h - from->y) * 2.0f / 20.0f);
    t += SQRTF((h - to->y) * 2.0f / 20.0f);
    PSVECSubtract(to, from, out);
    PSVECScale(out, out, 1.0f / t);
    out->y = SQRTF((h - from->y) * 40.0f);
}

f32 CalcStopDist(f32 speed, f32 decel)
{
    f32 d = 0.0f;

    do {
        d += speed;
        speed -= decel;
    } while (!(speed <= 0.0f));
    return d;
}

int CalcMovePosDist(Vec* pos, Vec* target, f32 dist)
{
    Vec dir;

    if (GetDistance(pos, target) <= dist * dist) {
        *pos = *target;
        return 1;
    }
    PSVECSubtract(target, pos, &dir);
#line 908 "D:/Bio4/Prog/sub2.cpp"
    VECNormalize(&dir, &dir);
    PSVECScale(&dir, &dir, dist);
    PSVECAdd(pos, &dir, pos);
    return 0;
}

// Dead-stripped in the original (a double 0.0 pool).
static int sub2_dead5(f64 x)
{
    if (x != 0.0) {
        return 1;
    }
    return 0;
}
