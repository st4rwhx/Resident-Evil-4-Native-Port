#include "types.h"
#include "vec.h"
#include "db_log.h"
#include "math_sub.h"
#include "geometry.h"

#line 100 "D:/Bio4/Prog/geometry.cpp"

// The functions below are never called in this build. GCC 2.95 still emits the string literals
// and the initializer templates of the local aggregates of inline functions at parse time, and the
// original geometry.o carries exactly these bytes in .rodata around the constant pools. The
// grouping and bodies are a guess that reproduces the bytes.
static inline const char* geo_name()
{
    return "";
}

// Matrix whose columns are the given axes and translation.
static inline void SetAxisMatrix(Mtx m, Vec* ax, Vec* ay, Vec* az, Vec* pos)
{
    m[0][0] = ax->x;
    m[1][0] = ax->y;
    m[2][0] = ax->z;
    m[0][1] = ay->x;
    m[1][1] = ay->y;
    m[2][1] = ay->z;
    m[0][2] = az->x;
    m[1][2] = az->y;
    m[2][2] = az->z;
    m[0][3] = pos->x;
    m[1][3] = pos->y;
    m[2][3] = pos->z;
}

// Never called: the original linker dropped the body but kept its constant pool (one 0.0f, the
// VECNormalize strings and the {0,1,0} template are shared with collision_point_cone_rev_play).
static void collision_cone_axis(GeoCone* cone, Vec* axis)
{
    Vec up = {0.0f, 1.0f, 0.0f};

    PSVECCrossProduct(&up, &cone->dir, axis);
    VECNormalize(axis, axis);
}
#line 220

int collision_point_cone_rev_play(Vec* p, GeoCone* cone, f32 margin)
{
    int ret = 0;
    Vec axis;
    Vec up = {0.0f, 1.0f, 0.0f};
    Vec c;
    Mtx m;
    Mtx inv;
    Vec lp;
    f32 r;
    f32 t;

    if (VecAngle(&cone->dir, &up) != 0.0f) {
        PSVECCrossProduct(&up, &cone->dir, &axis);
#line 245
        VECNormalize(&axis, &axis);
        VECNormalize(&cone->dir, &up);
        PSVECCrossProduct(&axis, &up, &c);
#line 248
        VECNormalize(&c, &c);
        SetAxisMatrix(m, &axis, &up, &c, &cone->pos);
    } else {
        PSMTXTrans(m, cone->pos.x, cone->pos.y, cone->pos.z);
    }
    PSMTXInverse(m, inv);
    PSMTXMultVec(inv, p, &lp);
    if (!(lp.y < 0.0f) && !(lp.y > cone->height)) {
        r = cone->height * sinf(cone->angle);
        cone->radius = r;
        t = lp.y * r / cone->height + margin;
        if (lp.x * lp.x + lp.z * lp.z < t * t) {
            ret = 1;
        }
    }
    return ret;
}

int collision_point_cone_rev_play_face(Vec* p, GeoCone* cone, Vec* face, f32 margin, f32 angle)
{
    Vec v;
    int ret = collision_point_cone_rev_play(p, cone, margin);

    if (ret) {
        PSVECScale(face, &v, -1.0f);
        if (VecAngle(&cone->dir, &v) < angle) {
            ret = 1;
        } else {
            ret = 0;
        }
    }
    return ret;
}

#line 300
static inline int collision_point_check(Vec* p)
{
    Vec lim = {0.01f, 0.5f, 0.0f};
    return p->x < lim.x && p->y < lim.y && p->z < lim.z;
}

int collision_sphere_hexahedron(GeoSphere* s, GeoHexahedron* h)
{
    int ret = 1;
    u32 i;
    Vec d;

    PSVECSubtract(&s->pos, &h->pointA, &d);
    for (i = 0; i <= 5; i++) {
        if (i == 3) {
            PSVECSubtract(&s->pos, &h->pointB, &d);
        }
        if (PSVECDotProduct(&d, &h->normal[i]) > s->r + 0.01f) {
            ret = 0;
            break;
        }
    }
    return ret;
}

#line 350
static inline int collision_fanpole_check(GeoCone* cone)
{
    Vec a = {1.0f, 0.0f, 0.5f};
    f32 b[4] = {0.0f, 0.5f, -0.5f, 0.0f};
    if (cone->dir.x != a.y || cone->dir.z != b[3]) {
        pLog->err(0, 0, "Fanpole is not vertical to the ground!\n");
        return 0;
    }
    return a.x != b[1];
}

static inline int collision_cylinder_check(GeoCone* cone)
{
    if (cone->dir.x != 0.0f || cone->dir.z != 0.0f) {
        pLog->err(0, 0, "Cylinder is not vertical to the ground!\n");
        return 0;
    }
    return 1;
}

static inline f32 collision_range_check(Vec* p)
{
    f32 rng[5] = {0.0f, 0.5f, 1.0f, -0.5f, 0.0f};
    return rng[0] + rng[1] * p->x + rng[2] * p->y + rng[3] * p->z + rng[4];
}
