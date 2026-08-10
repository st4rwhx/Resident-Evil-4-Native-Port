#ifndef MATH_SUB_H
#define MATH_SUB_H

#include "types.h"
#include "vec.h"
#include "db_log.h"

#ifndef PI
#define PI 3.1415927f
#endif

// Float abs as the original SDK header defines it: a volatile asm, which also acts as a
// scheduling barrier (loads after it are not hoisted above it).
static inline f32 fabsf(f32 x)
{
    f32 r;
    asm volatile("fabs %0,%1" : "=f"(r) : "f"(x));
    return r;
}

// game/math_sub.cpp
void RotMatrix(Mtx m, Vec* rot);
void TransMatrix(Mtx m, Vec* pos);
void ScaleMatrix(Mtx m, Vec* scale);

extern "C" {
// game/math_sub.cpp (C linkage)
void SetOrientationZX(Vec* z, Vec* x, Mtx m);
void SetOrientationZY(Vec* z, Vec* y, Mtx m);
void low_RotMatrix(Mtx m, Vec* rot);
void RotMatrixZXY(Mtx m, Vec* rot);
void Matrix2AxisAngle(Mtx m, Vec* rot);
void VecRadLimit(Vec* v);
f32 VecElevation(Vec* v);
void MtxRotAxisPosRad(Mtx m, Vec* axis, Vec* pos, f32 rad);
void VecLinearCombination(Vec* a, Vec* b, f32 s, f32 t, Vec* out);
void VecInternalDivisionAngle(Vec* a, Vec* b, f32 s, Vec* out, f32 t);
void VecLinearDecomposition(Vec* v, Vec* a, Vec* b, f32* s, f32* t);
f32 hermite(f32* p, f32* v, f32 t);
f32** malloc_2dim_array_f32(int n, int m);
void free_2dim_array_f32(int n, int m, f32** p);
int de_Boor_Cox(int n, f32* knot, int k, f32 t, f32* out);
// COMPILER-DIFF 1: cam_ctrl BSpline issues `lfs f1` (t) before the `addi`/`lwz` of out/k; the
// floats-first redeclaration reproduces the original's argument-move order (ABI-identical).
int de_Boor_CoxF(int n, f32* knot, f32 t, int k, f32* out) asm("de_Boor_Cox");
f32 MtxNNLUDecomposition(int n, f32* a, int* ip);
f32 MtxNNInverse(int n, f32* m, f32* inv);
void MtxNNMultVecSR(int n, int m, f32* mtx, f32* v, f32* out);
void OrthographicProjection(Vec* p, Vec* out, Vec* dir, Vec* plane_p, Vec* plane_n);
f32 IPOW(f32 x, int n);
f32 SQRTF(f32 x);
f32 SINF(f32 x);
f32 COSF(f32 x);
f32 LIMIT_ANGLE(f32 x);
f32 VecAngle(Vec* a, Vec* b);
// game/sub2.cpp
f32 RootSumSquare3(Vec* v);
int GetScreenPos(Vec* pos, Vec* scr);
f32 GetDistance(Vec* a, Vec* b);      // squared distance
f32 GetDistance3(Vec* a, Vec* b);     // distance
f32 GetDistanceXZ(Vec* a, Vec* b);    // squared distance in the XZ plane
void RotVector(Vec* src, Vec* rot, Vec* dst);
// Angle step from `ang` towards `target` seen from `pos`, clamped to +-limit.
f32 Muku(Vec* pos, Vec* target, f32 ang, f32 limit);
// Step from `ang` towards `target`, at most +-limit.
f32 Muku2(f32 ang, f32 target, f32 limit);
// Muku2 towards the XZ direction of `dir`.
f32 Muku3(Vec* dir, f32 ang, f32 limit);
// out = a + (b - a) * t
void PosToPos(Vec* a, Vec* b, Vec* out, f32 t);
f32 GetXZAngle(Vec* from, Vec* to);   // atan2 of to - from in the XZ plane, limited to +-PI
f32 GetXYAngle(Vec* from, Vec* to);
f32 GetXZAngleLocal(Vec* from, Vec* to, f32 ang);   // GetXZAngle relative to `ang`
// Point `p` inside the XZ quad `quad[4]` (0-1-2-3 order)?
int HitCheckPoint4(Vec* p, Vec* quad);
// pos += speed rotated by the model's rot
void AddSpeed(struct cModel* m, const Vec* speed);
// dst[8] = rotate(src[8], rot) + pos
void BoxWorldCalc(Vec* src, Vec* dst, Vec* pos, Vec* rot);
// World point under screen position (sx, sy): the floor hit when y == 1e8f, else at height y.
void Get3DPosFrom2D(Vec* out, f32 sx, f32 sy, f32 y);
// Rotate `v` (x, -y on the ground plane) into the camera's heading.
void VecToCamVec(Vec* v, Vec* out);
// Segment a-b against the sphere (c, r): 1 with the entry point in `out` (a itself when a is inside).
int LineSphereCrossCk(Vec* a, Vec* b, Vec* c, Vec* out, f32 r);
int SphereHitCk(Vec* a, Vec* b, f32 ra, f32 rb);
// Launch vector for a parabola from `from` to `to` peaking `h` above the higher end (gravity 20).
void CalcParabolaVector(Vec* out, Vec* from, Vec* to, f32 h);
f32 CalcStopDist(f32 speed, f32 decel);
// Move `pos` `dist` towards `target`; 1 when it arrived.
int CalcMovePosDist(Vec* pos, Vec* target, f32 dist);
// lib math
f32 sqrtf(f32 x);
f32 sinf(f32 x);
f32 cosf(f32 x);
f32 atan2f(f32 y, f32 x);
f32 acosf(f32 x);
f32 asinf(f32 x);
}

// game/sub2.cpp (C++ linkage)
f32 GetDistance(Vec& a, Vec& b);
class cModel;
int Front_check(cModel* a, cModel* b, f32 ang);   // b within +-ang of a's heading
int Front_check(cModel* a, Vec* b, f32 ang);
int Front_check(Vec* a, Vec* b, f32 rot, f32 ang);

// Debug-checked normalize: zero vectors are reported with the caller's file/line.
#define VECNormalize(src, dst)                                                          \
    if (0.0f == (src)->x && 0.0f == (src)->y && 0.0f == (src)->z) {                    \
        pLog->err(0, 0, "VECNormalize:[%s/%d]", __FILE__, __LINE__);                    \
        (dst)->x = (dst)->y = (dst)->z = 0.0f;                                          \
    } else                                                                              \
        PSVECNormalize(src, dst)

#endif
