#include "types.h"
#include "vec.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"
#include "view.h"

#define VIEW_ASPECT 1.33333333f

VIEW View;
u8 ViewHit[0xD00];

void VIEW::gameInit(Camera* cam)
{
    pCam = cam;
    roomInit();
}

void VIEW::roomInit()
{
    init();
}

void VIEW::init()
{
    initPerspective(pCam->param.fovy, VIEW_ASPECT, ZNEAR, ZFAR);
    orientation();
    fovyOld = pCam->param.fovy;
    zfarOld = zfar;
}

void VIEW::move()
{
    if (fovyOld != pCam->param.fovy || zfarOld != zfar) {
        initPerspective(pCam->param.fovy, VIEW_ASPECT, ZNEAR, zfar);
    }
    orientation();
    fovyOld = pCam->param.fovy;
    zfarOld = zfar;
}

void VIEW::setFarPlane(f32 z)
{
    zfar = z;
}

// initPerspective: the original's algorithm -- three Vec temporaries (the second normal block uses
// t1/t3), `Vec q[4]` for the sphere points, the frustum points halved with an indexed loop, and
// the circumsphere numerators written with the point differences inline (recomputed from the q
// copies after the PSVECSquareMag calls).
// Shape (from the target's asm): ONE frustum pointer `b` for both halves (`b = &localFull`, then
// `b = &local; *b = localFull;` before the halving loop, then `b = &localFull` again for the
// sphere block). The two halves' `&b->point[k]`/`&b->normal[k]` are then the SAME gcse
// expressions: their hash-table indexes come from the first half's first occurrences (the order of
// the 13 `addi rX,r31,K` in the halving loop's preheader), the second half's occurrences are
// redundant after the loop and are inserted there, and the first half's PRE pattern (only
// `&point[4]` carried G1 -> G2, `mr r23,r29`) is what block LCM gives with the `b = &local` kill
// and the second-half occurrences in the same problem. Two distinct pointers (`b`/`c`) make the
// second half a separate expression set and the first half PREs every recurring address.
// `b = &local` is set before the 0xc0-byte copy loop so cse cannot fold `b + K` to `this + K`
// after it. Halving loop plain (`b->point[i].x *= 0.5f; ...`), no pointer locals; q copies
// field-wise (a struct copy forces `&b->point[k]` into a pseudo that cse folds to `this + K` and
// gcse then hoists).
// Sphere block: `s = &sphere` (a ViewSphere*) for the centre/radius stores and the Distance
// argument -- `&sphere` is then one gcse expression (PRE'd into the copy-loop preheader, r18,
// 5 refs, so it outranks `&q[1..3]` in global-alloc and `&q[2]`/`&q[3]` are the ones spilled);
// cse2's find_best_addr rewrites `(mem s)` (center.x) to the class member with the higher rtx
// cost, `this + 892`, while `s->center.y/.z`, `s->radius` stay s-based. The denominator is
// `2.0f * det` inline (folded to det + det; a fresh pseudo that does not cross the SquareMag calls,
// so sched anchors the fadds after the last call and the centre's FP pseudos allocate f27..f31).
// `zfar = zfar_` before `znear = znear_` (the two dying-argument stores issue in LUID order).
// Frustum point stores: one `z` variable holds -zn and then -zf (a two-set pseudo, allocated f11 in both
// blocks), `w` is likewise shared, the far block has its own `h2` (block-local, tied to the dying `t` in
// f31); each point is stored z, x, y (the far block's `lfs zf` depends on all twelve near stores, so the
// store order inside a block is the sched1 order: the dying store first, then source order).
void VIEW::initPerspective(f32 fovy_, f32 aspect_, f32 znear_, f32 zfar_)
{
    Vec t1;
    Vec t2;
    Vec t3;
    Vec q[4];
    ViewFrustum* b;
    ViewSphere* s;
    f32 t;
    f32 h;
    f32 w;
    f32 zn;
    f32 zf;
    f32 det;
    f32 z;
    f32 h2;
    f32 d0;
    f32 d1;
    f32 d2;
    int i;

    fovy = fovy_;
    aspect = aspect_;
    zfar = zfar_;
    znear = znear_;
    t = sinf(fovy * 0.5f * 3.1415927f / 180.0f) / cosf(fovy * 0.5f * 3.1415927f / 180.0f);
    b = &localFull;
    zn = znear;
    z = -zn;
    h = zn * t;
    w = h * aspect_;
    b->point[0].z = z;
    b->point[0].x = w;
    b->point[0].y = h;
    b->point[1].z = z;
    b->point[1].x = -w;
    b->point[1].y = h;
    b->point[2].z = z;
    b->point[2].x = -w;
    b->point[2].y = -h;
    b->point[3].z = z;
    b->point[3].x = w;
    b->point[3].y = -h;
    zf = zfar;
    h2 = zf * t;
    z = -zf;
    w = h2 * aspect_;
    b->point[4].z = z;
    b->point[4].x = w;
    b->point[4].y = h2;
    b->point[5].z = z;
    b->point[5].x = -w;
    b->point[5].y = h2;
    b->point[6].z = z;
    b->point[6].x = -w;
    b->point[6].y = -h2;
    b->point[7].z = z;
    b->point[7].x = w;
    b->point[7].y = -h2;

#line 190 "D:/Bio4/Prog/view.cpp"
    PSVECSubtract(&b->point[1], &b->point[0], &t1);
    PSVECSubtract(&b->point[3], &b->point[0], &t2);
    PSVECCrossProduct(&t1, &t2, &b->normal[0]);
    VECNormalize(&b->normal[0], &b->normal[0]);

    PSVECSubtract(&b->point[3], &b->point[0], &t1);
    PSVECSubtract(&b->point[4], &b->point[0], &t2);
    PSVECCrossProduct(&t1, &t2, &b->normal[1]);
    VECNormalize(&b->normal[1], &b->normal[1]);

    PSVECSubtract(&b->point[4], &b->point[0], &t1);
    PSVECSubtract(&b->point[1], &b->point[0], &t2);
    PSVECCrossProduct(&t1, &t2, &b->normal[2]);
    VECNormalize(&b->normal[2], &b->normal[2]);

    PSVECSubtract(&b->point[5], &b->point[1], &t1);
    PSVECSubtract(&b->point[2], &b->point[1], &t2);
    PSVECCrossProduct(&t1, &t2, &b->normal[3]);
    VECNormalize(&b->normal[3], &b->normal[3]);

    PSVECSubtract(&b->point[6], &b->point[2], &t1);
    PSVECSubtract(&b->point[3], &b->point[2], &t2);
    PSVECCrossProduct(&t1, &t2, &b->normal[4]);
    VECNormalize(&b->normal[4], &b->normal[4]);

    PSVECSubtract(&b->point[7], &b->point[4], &t1);
    PSVECSubtract(&b->point[5], &b->point[4], &t2);
    PSVECCrossProduct(&t1, &t2, &b->normal[5]);
    VECNormalize(&b->normal[5], &b->normal[5]);

    b = &local;
    *b = localFull;
    for (i = 0; i < 8; i++) {
        b->point[i].x *= 0.5f;
        b->point[i].y *= 0.5f;
    }
#line 236 "D:/Bio4/Prog/view.cpp"
    PSVECSubtract(&b->point[1], &b->point[0], &t1);
    PSVECSubtract(&b->point[3], &b->point[0], &t3);
    PSVECCrossProduct(&t1, &t3, &b->normal[0]);
    VECNormalize(&b->normal[0], &b->normal[0]);

    PSVECSubtract(&b->point[3], &b->point[0], &t1);
    PSVECSubtract(&b->point[4], &b->point[0], &t3);
    PSVECCrossProduct(&t1, &t3, &b->normal[1]);
    VECNormalize(&b->normal[1], &b->normal[1]);

    PSVECSubtract(&b->point[4], &b->point[0], &t1);
    PSVECSubtract(&b->point[1], &b->point[0], &t3);
    PSVECCrossProduct(&t1, &t3, &b->normal[2]);
    VECNormalize(&b->normal[2], &b->normal[2]);

    PSVECSubtract(&b->point[5], &b->point[1], &t1);
    PSVECSubtract(&b->point[2], &b->point[1], &t3);
    PSVECCrossProduct(&t1, &t3, &b->normal[3]);
    VECNormalize(&b->normal[3], &b->normal[3]);

    PSVECSubtract(&b->point[6], &b->point[2], &t1);
    PSVECSubtract(&b->point[3], &b->point[2], &t3);
    PSVECCrossProduct(&t1, &t3, &b->normal[4]);
    VECNormalize(&b->normal[4], &b->normal[4]);

    PSVECSubtract(&b->point[7], &b->point[4], &t1);
    PSVECSubtract(&b->point[5], &b->point[4], &t3);
    PSVECCrossProduct(&t1, &t3, &b->normal[5]);
    VECNormalize(&b->normal[5], &b->normal[5]);

    orientation();

    b = &localFull;
    q[0].x = b->point[0].x;
    q[0].y = b->point[0].y;
    q[0].z = b->point[0].z;
    q[1].x = b->point[4].x;
    q[1].y = b->point[4].y;
    q[1].z = b->point[4].z;
    q[2].x = b->point[5].x;
    q[2].y = b->point[5].y;
    q[2].z = b->point[5].z;
    q[3].x = b->point[6].x;
    q[3].y = b->point[6].y;
    q[3].z = b->point[6].z;
    det = (q[1].x - q[0].x) * (q[2].y - q[1].y) * (q[3].z - q[2].z) + (q[2].x - q[1].x) * (q[3].y - q[2].y) * (q[1].z - q[0].z) +
          (q[3].x - q[2].x) * (q[1].y - q[0].y) * (q[2].z - q[1].z) - (q[1].x - q[0].x) * (q[3].y - q[2].y) * (q[2].z - q[1].z) -
          (q[2].x - q[1].x) * (q[1].y - q[0].y) * (q[3].z - q[2].z) - (q[3].x - q[2].x) * (q[2].y - q[1].y) * (q[1].z - q[0].z);
    d0 = PSVECSquareMag(&q[0]) - PSVECSquareMag(&q[1]);
    d1 = PSVECSquareMag(&q[1]) - PSVECSquareMag(&q[2]);
    d2 = PSVECSquareMag(&q[2]) - PSVECSquareMag(&q[3]);
    s = &sphere;
    s->center.x = (d0 * ((q[3].y - q[2].y) * (q[2].z - q[1].z) - (q[2].y - q[1].y) * (q[3].z - q[2].z)) +
                       d1 * ((q[1].y - q[0].y) * (q[3].z - q[2].z) - (q[3].y - q[2].y) * (q[1].z - q[0].z)) +
                       d2 * ((q[2].y - q[1].y) * (q[1].z - q[0].z) - (q[1].y - q[0].y) * (q[2].z - q[1].z))) /
                      (2.0f * det);
    s->center.y = (d0 * ((q[3].z - q[2].z) * (q[2].x - q[1].x) - (q[2].z - q[1].z) * (q[3].x - q[2].x)) +
                       d1 * ((q[1].z - q[0].z) * (q[3].x - q[2].x) - (q[3].z - q[2].z) * (q[1].x - q[0].x)) +
                       d2 * ((q[2].z - q[1].z) * (q[1].x - q[0].x) - (q[1].z - q[0].z) * (q[2].x - q[1].x))) /
                      (2.0f * det);
    s->center.z = (d0 * ((q[3].x - q[2].x) * (q[2].y - q[1].y) - (q[2].x - q[1].x) * (q[3].y - q[2].y)) +
                       d1 * ((q[1].x - q[0].x) * (q[3].y - q[2].y) - (q[3].x - q[2].x) * (q[1].y - q[0].y)) +
                       d2 * ((q[2].x - q[1].x) * (q[1].y - q[0].y) - (q[1].x - q[0].x) * (q[2].y - q[1].y))) /
                      (2.0f * det);
    s->radius = PSVECDistance(&s->center, &q[0]);
}

// Never called: the original linker dead-stripped the bodies (STRIP_UNUSED) and kept the two
// constant pools after initPerspective's (`.rodata` 0x48..0x7f: DF 0.0, 1.0f, 0.0f; then 0.0f, the
// signed int->f32 magic, 2*pi, 12.0f, the unsigned int->f32 magic, 1/1024, pi/2).
static void viewSphereReset(ViewSphere* sp, f64 r)
{
    if (r > 0.0) {
        sp->radius = 1.0f;
    }
    sp->x10 = 0.0f;
}

static void viewSphereRing(ViewSphere* sp, Vec* out, int div, u32 col)
{
    Vec* p;
    f32 a;
    f32 c;

    p = out;
    p->y = 0.0f;
    a = (f32) div * 6.2831855f / 12.0f;
    c = (f32) col * 0.0009765625f;
    p->x = sp->center.x + sp->radius * (a + 1.5707964f);
    p->z = sp->center.z + sp->radius * c;
}

void VIEW::orientation()
{
    Mtx* m = &pG->Cam.mat;
    u32 i;

    for (i = 0; i < 6; i++) {
        PSMTXMultVecSR(*m, &localFull.normal[i], &worldFull.normal[i]);
        PSMTXMultVecSR(*m, &local.normal[i], &world.normal[i]);
    }
    for (i = 0; i < 8; i++) {
        PSMTXMultVec(*m, &localFull.point[i], &worldFull.point[i]);
        PSMTXMultVec(*m, &local.point[i], &world.point[i]);
    }
    sphereWorld = sphere;
    PSMTXMultVec(*m, &sphere.center, &sphereWorld.center);
}
