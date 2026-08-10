#include "types.h"
#include "vec.h"
#include "db_log.h"
#include "main_mem.h"
#include "math_sub.h"

extern "C" {
f32 asinf(f32 x);
f32 acosf(f32 x);
void* memcpy(void* dst, const void* src, unsigned int n);
int printf(const char* fmt, ...);
int fprintf(void* fp, const char* fmt, ...);
struct ReentStd {
    int _errno;
    void* _stdin;
    void* _stdout;
    void* _stderr;
};
extern ReentStd* _impure_ptr;
#define stderr (_impure_ptr->_stderr)

}

#define PI2 6.2831855f

// Never called in this build: only their static results survive (.bss).
static inline Vec* VecScaled(Vec* v, f32 s)
{
    static Vec ans;
    PSVECScale(v, &ans, s);
    return &ans;
}

static inline Vec* VecSum(Vec* a, Vec* b)
{
    static Vec ans;
    PSVECAdd(a, b, &ans);
    return &ans;
}

// Build a rotation matrix whose Z axis is `z` and whose X axis is `x` (orthonormalised).
#line 15 "D:/Bio4/Prog/math_sub.cpp"
void SetOrientationZX(Vec* z, Vec* x, Mtx m)
{
    Vec vx;
    Vec vy;
    Vec vz;

    VECNormalize(x, &vx);
    VECNormalize(z, &vz);

    PSVECCrossProduct(&vz, &vx, &vy);
    VECNormalize(&vy, &vy);
    PSVECCrossProduct(&vy, &vz, &vx);
    VECNormalize(&vx, &vx);

    PSMTXIdentity(m);
    m[0][0] = vx.x;
    m[1][0] = vx.y;
    m[2][0] = vx.z;
    m[0][1] = vy.x;
    m[1][1] = vy.y;
    m[2][1] = vy.z;
    m[0][2] = vz.x;
    m[1][2] = vz.y;
    m[2][2] = vz.z;
}

void SetOrientationZY(Vec* z, Vec* y, Mtx m)
{
    Vec vx;
    Vec vy;
    Vec vz;

#line 46 "D:/Bio4/Prog/math_sub.cpp"
    VECNormalize(y, &vy);
    VECNormalize(z, &vz);

    PSVECCrossProduct(&vy, &vz, &vx);
    VECNormalize(&vx, &vx);
    PSVECCrossProduct(&vz, &vx, &vy);
    VECNormalize(&vy, &vy);

    PSMTXIdentity(m);
    m[0][0] = vx.x;
    m[1][0] = vx.y;
    m[2][0] = vx.z;
    m[0][1] = vy.x;
    m[1][1] = vy.y;
    m[2][1] = vy.z;
    m[0][2] = vz.x;
    m[1][2] = vz.y;
    m[2][2] = vz.z;
}

#define MTX_COL(m, c, v)   \
    (v).x = (m)[0][c];     \
    (v).y = (m)[1][c];     \
    (v).z = (m)[2][c]

static inline void MtxGetCol(Mtx m, int c, Vec* v)
{
    v->x = m[0][c];
    v->y = m[1][c];
    v->z = m[2][c];
}

void Matrix2AxisAngle(Mtx m, Vec* rot)
{
    Vec v0;
    Vec v1;
    Vec v2;
    Vec v3;
    Mtx r;
    Mtx inv;
    Mtx t;

    PSMTXTranspose(m, t);
    MTX_COL(r, 0, v3);
    MtxGetCol(t, 0, &v0);
    MTX_COL(t, 1, v1);
    MTX_COL(t, 2, v2);
    rot->x = rot->y = rot->z = 0.0f;

    rot->x = atan2f(-v2.y, v2.z);
    if (v2.x > 1.0f) {
        rot->y = asinf(1.0f);
    } else if (v2.x < -1.0f) {
        rot->y = asinf(-1.0f);
    } else {
        rot->y = asinf(v2.x);
    }
    rot->x = -rot->x;
    rot->y = -rot->y;
    RotMatrix(r, rot);
    PSMTXInverse(r, inv);
    PSMTXConcat(m, inv, r);
    MTX_COL(r, 0, v3);
    rot->z = atan2f(v3.y, v3.x);
}

void VecRadLimit(Vec* v)
{
    f32* p = (f32*) v;
    int i;

    for (i = 0; i < 3; i++) {
        while (p[i] >= PI) {
            p[i] -= PI2;
        }
        while (p[i] < -PI) {
            p[i] += PI2;
        }
    }
}

f32 VecAngle(Vec* a, Vec* b)
{
    f32 d = PSVECDotProduct(a, b);
    f32 l = PSVECMag(a);

    l *= PSVECMag(b);
    d /= l;
    return acosf(d < -1.0f ? -1.0f : (d > 1.0f ? 1.0f : d));
}

f32 VecElevation(Vec* v)
{
    return atan2f(v->y, SQRTF(v->x * v->x + v->z * v->z));
}

void MtxRotAxisPosRad(Mtx m, Vec* axis, Vec* pos, f32 rad)
{
    Vec p;
    Vec d;
    Mtx t1;
    Mtx t2;
    Mtx t3;
    Mtx t4;
    Mtx t5;

    PSMTXIdentity(t4);
    PSMTXRotAxisRad(m, axis, rad);
    PSMTXMultVecSR(m, pos, &p);
    PSVECSubtract(pos, &p, &d);
    m[0][3] += d.x;
    m[1][3] += d.y;
    m[2][3] += d.z;

    PSMTXTrans(t1, -pos->x, -pos->y, -pos->z);
    PSMTXRotAxisRad(t2, axis, rad);
    PSMTXTrans(t3, pos->x, pos->y, pos->z);
    PSMTXConcat(t2, t1, m);
    PSMTXConcat(t3, m, m);
}

void VecLinearCombination(Vec* a, Vec* b, f32 s, f32 t, Vec* out)
{
    Vec ta;
    Vec tb;

    PSVECScale(a, &ta, s);
    PSVECScale(b, &tb, t);
    PSVECAdd(&ta, &tb, out);
}

// Interpolate the direction of `a` towards `b` by the ratio s : t.
void VecInternalDivisionAngle(Vec* a, Vec* b, f32 s, Vec* out, f32 t)
{
    Mtx r;
    Vec axis;
    f32 ang = VecAngle(a, b);

    if (ang == 0.0f) {
#line 342 "D:/Bio4/Prog/math_sub.cpp"
        VECNormalize(a, out);
    } else if (ang == PI) {
    } else {
        PSVECCrossProduct(a, b, &axis);
        PSMTXRotAxisRad(r, &axis, s / (t + s) * ang);
        PSMTXMultVecSR(r, a, out);
#line 354 "D:/Bio4/Prog/math_sub.cpp"
        VECNormalize(out, out);
    }
}

// Decompose `v` on the plane spanned by `a` and `b`: v = s * a + t * b.
void VecLinearDecomposition(Vec* v, Vec* a, Vec* b, f32* s, f32* t)
{
    Vec n;
    Vec m;
    Vec ab;
    Vec va;
    f32 k;
    f32 l;

    PSVECCrossProduct(a, b, &n);
    PSVECCrossProduct(v, &n, &m);
    PSVECSubtract(b, a, &ab);
    PSVECSubtract(v, a, &va);
    k = PSVECDotProduct(&m, &va);
    k /= PSVECDotProduct(&m, &ab);
    PSVECScale(&ab, &va, k);
    PSVECAdd(a, &va, &va);
    l = PSVECMag(v);
    l /= PSVECMag(&va);
    *s = l * (1.0f - k);
    *t = l * k;
}

void ScaleMatrix(Mtx m, Vec* s)
{
    m[0][0] *= s->x;
    m[0][1] *= s->y;
    m[0][2] *= s->z;
    m[1][0] *= s->x;
    m[1][1] *= s->y;
    m[1][2] *= s->z;
    m[2][0] *= s->x;
    m[2][1] *= s->y;
    m[2][2] *= s->z;
}

void TransMatrix(Mtx m, Vec* pos)
{
    m[0][3] = pos->x;
    m[1][3] = pos->y;
    m[2][3] = pos->z;
}

void RotMatrix(Mtx m, Vec* rot)
{
    f32 sx;
    f32 sy;
    f32 sz;
    f32 cx;
    f32 cy;
    f32 cz;
    f32 szcx;
    f32 czsx;
    f32 szsx;
    f32 czcx;

    sx = sinf(rot->x);
    sy = sinf(rot->y);
    sz = sinf(rot->z);
    cx = cosf(rot->x);
    cy = cosf(rot->y);
    cz = cosf(rot->z);
    szcx = sz * cx;
    szsx = sz * sx;
    czsx = cz * sx;
    czcx = cz * cx;

    m[0][0] = cz * cy;
    m[0][1] = czsx * sy - szcx;
    m[0][2] = czcx * sy + szsx;
    m[0][3] = 0.0f;
    m[1][0] = sz * cy;
    m[1][1] = szsx * sy + czcx;
    m[1][2] = szcx * sy - czsx;
    m[1][3] = 0.0f;
    m[2][0] = -sy;
    m[2][1] = cy * sx;
    m[2][2] = cy * cx;
    m[2][3] = 0.0f;
}

void low_RotMatrix(Mtx m, Vec* rot)
{
    f32 sx;
    f32 cx;
    f32 sy;
    f32 cy;
    f32 sz;
    f32 cz;
    f32 szsx;
    f32 czcx;
    f32 szcx;
    f32 czsx;

    if (rot->x == 0.0f) {
        sx = 0.0f;
        cx = 1.0f;
    } else {
        sx = SINF(rot->x);
        cx = COSF(rot->x);
    }
    if (rot->y == 0.0f) {
        sy = 0.0f;
        cy = 1.0f;
    } else {
        sy = SINF(rot->y);
        cy = COSF(rot->y);
    }
    if (rot->z == 0.0f) {
        sz = 0.0f;
        cz = 1.0f;
    } else {
        sz = SINF(rot->z);
        cz = COSF(rot->z);
    }
    szsx = sz * sx;
    czcx = cz * cx;
    szcx = sz * cx;
    czsx = cz * sx;

    m[0][0] = cz * cy;
    m[0][1] = czsx * sy - szcx;
    m[0][2] = czcx * sy + szsx;
    m[0][3] = 0.0f;
    m[1][0] = sz * cy;
    m[1][1] = szsx * sy + czcx;
    m[1][2] = szcx * sy - czsx;
    m[1][3] = 0.0f;
    m[2][0] = -sy;
    m[2][1] = cy * sx;
    m[2][2] = cy * cx;
    m[2][3] = 0.0f;
}

void RotMatrixZXY(Mtx m, Vec* rot)
{
    Mtx t;

    PSMTXRotRad(m, 'y', rot->y);
    PSMTXRotRad(t, 'x', rot->x);
    PSMTXConcat(m, t, m);
    PSMTXRotRad(t, 'z', rot->z);
    PSMTXConcat(m, t, m);
}

// Cubic Hermite interpolation of p[0]..p[1] with tangents v[0]..v[1].
f32 hermite(f32* p, f32* v, f32 t)
{
    f32 t2 = t * t;
    f32 t3 = t * t2;
    f32 h01 = -(t3 + t3) + 3.0f * t2;
    f32 h11 = t3 - t2;
    f32 h10 = h11 - t2 + t;
    f32 h00 = -h01 + 1.0f;

    return p[0] * h00 + p[1] * h01 + v[0] * h10 + v[1] * h11;
}

f32** malloc_2dim_array_f32(int n, int m)
{
    f32** p;
    int i;
    int j;

#line 954 "D:/Bio4/Prog/math_sub.cpp"
    p = (f32**) MEM_ALLOC(n * sizeof(f32*), 1, 13);
    if (p == NULL) {
        pLog->err(0, 0, "malloc_2dim_array_f32(): Memory Allocation Error!");
        return NULL;
    }
    for (i = 0; i < n; i++) {
#line 962 "D:/Bio4/Prog/math_sub.cpp"
        p[i] = (f32*) MEM_ALLOC(m * sizeof(f32), 1, 13);
        if (p[i] == NULL) {
            pLog->err(0, 0, "malloc_2dim_array_f32(): Memory Allocation Error!");
            for (j = 0; j < i; j++) {
                Mem_free(p[j]);
            }
            return NULL;
        }
    }
    return p;
}

void free_2dim_array_f32(int n, int m, f32** p)
{
    int i;

    for (i = 0; i < n; i++) {
        Mem_free(p[i]);
    }
    Mem_free(p);
}

// B-spline basis functions of order k + 1 for n control points at parameter t (de Boor-Cox
// recursion). `knot` may be NULL for a uniform knot vector. Returns 0 on allocation failure.
int de_Boor_Cox(int n, f32* knot, int k, f32 t, f32* out)
{
    int m = k + 1;
    f32** tmp_B;
    f32* q;
    int i;
    int j;

    tmp_B = malloc_2dim_array_f32(n + m, m);
    if (tmp_B == NULL) {
        pLog->err(0, 0, "de_Boor_Cox(): tmp_B -> Memory Allocation Error!");
        return 0;
    }
#line 1048 "D:/Bio4/Prog/math_sub.cpp"
    q = (f32*) MEM_ALLOC((n + m) * sizeof(f32), 1, 13);
    if (q == NULL) {
        pLog->err(0, 0, "de_Boor_Cox(): q -> Memory Allocation Error!");
        free_2dim_array_f32(n + m, m, tmp_B);
        return 0;
    }

    for (i = 0; i < n + m; i++) {
        for (j = 0; j < m; j++) {
            tmp_B[i][j] = 0.0f;
        }
    }

    if (knot != NULL) {
        for (j = 0; j < m; j++) {
            q[j] = knot[0];
        }
        for (j = m; j < n; j++) {
            q[j] = (knot[j - m] + knot[j]) * 0.5f;
        }
        for (j = n; j < n + m; j++) {
            q[j] = knot[n - 1];
        }
    } else {
        for (j = 0; j < m; j++) {
            q[j] = 0.0f;
        }
        for (j = m; j < n; j++) {
            q[j] = (f32) (j - m) + (f32) m * 0.5f;
        }
        for (j = n; j < n + m; j++) {
            q[j] = (f32) (n - 1);
        }
    }

    for (i = 0; i < n; i++) {
        if (q[i] <= t && t < q[i + 1]) {
            tmp_B[i][0] = 1.0f;
        }
    }
    if (q[n + m - 2] <= t && t <= q[n + m - 1] + 0.00001f) {
        tmp_B[n - 1][0] = 1.0f;
    }

    for (j = 1; j < m; j++) {
        for (i = 0; i < n; i++) {
            tmp_B[i][j] = 0.0f;
            if (q[i + 1] != q[i + j + 1]) {
                tmp_B[i][j] += (q[i + j + 1] - t) * tmp_B[i + 1][j - 1] / (q[i + j + 1] - q[i + 1]);
            }
            if (q[i] != q[i + j]) {
                tmp_B[i][j] += (t - q[i]) * tmp_B[i][j - 1] / (q[i + j] - q[i]);
            }
        }
    }

    for (i = 0; i < n; i++) {
        out[i] = tmp_B[i][m - 1];
    }
    Mem_free(q);
    free_2dim_array_f32(n + m, m, tmp_B);
    return 1;
}

// Never called in this build. GCC 2.95 emits the string literal and the initializer templates of
// the local aggregates of an unused inline function at parse time; the original object carries
// exactly these bytes between de_Boor_Cox's and MtxNNLUDecomposition's constant pools (the
// message is shared with MtxNNLUDecomposition). The body is a guess that reproduces the bytes.
static inline f32 MtxNNPivotSign(int n, f32* a, int* ip)
{
    fprintf(stderr, "Error: Can't calc Inverse Matrix !\n");
    {
        f32 sign[2] = {1.0f, -1.0f};
        Vec zaxis = {0.0f, 0.0f, 1.0f};
        return sign[n & 1] * zaxis.z * a[ip[0]];
    }
}

// LU decomposition with partial pivoting of the n x n matrix `a` (row permutation in `ip`).
// Returns the determinant, 0 if singular.
f32 MtxNNLUDecomposition(int n, f32* a, int* ip)
{
    int i;
    int j;
    int k;
    int l = 0;
    f32 det;
    f32 max;
    f32 v;
    f32 t;

    for (i = 0; i < n; i++) {
        ip[i] = i;
    }
    det = 1.0f;
    for (k = 0; k < n; k++) {
        max = -1.0f;
        for (i = k; i < n; i++) {
            v = a[ip[i] * n + k];
            v = fabsf(v);
            if (v > max) {
                max = v;
                l = i;
            }
        }
        if (l != k) {
            j = ip[l];
            ip[l] = ip[k];
            ip[k] = j;
            det = -det;
        }
        max = a[ip[k] * n + k];
        det *= max;
        if (max == 0.0f) {
            fprintf(stderr, "Error: Can't calc Inverse Matrix !\n");
            return 0.0f;
        }
        for (i = k + 1; i < n; i++) {
            t = a[ip[i] * n + k] / max;
            a[ip[i] * n + k] = t;
            for (j = k + 1; j < n; j++) {
                a[ip[i] * n + j] -= t * a[ip[k] * n + j];
            }
        }
    }
    return det;
}

// Inverse of the n x n matrix `m` into `inv`; returns the determinant (0 = singular / no memory).
f32 MtxNNInverse(int n, f32* m, f32* inv)
{
    int* ip;
    f32* m_tmp;
    int i;
    int j;
    int k;
    int p;
    f32 det;
    f32 t;

#line 1278 "D:/Bio4/Prog/math_sub.cpp"
    ip = (int*) MEM_ALLOC(n * sizeof(int), 1, 13);
    if (ip == NULL) {
        pLog->err(0, 0, "MtxNNInverse(): ip, Memory allocation error!");
        return 0.0f;
    }
#line 1286 "D:/Bio4/Prog/math_sub.cpp"
    m_tmp = (f32*) MEM_ALLOC(n * n * sizeof(f32), 1, 13);
    if (m_tmp == NULL) {
        pLog->err(0, 0, "MtxNNInverse(): m_tmp, Memory allocation error!");
        Mem_free(ip);
        return 0.0f;
    }
    memcpy(m_tmp, m, n * n * sizeof(f32));
    det = MtxNNLUDecomposition(n, m_tmp, ip);
    if (det != 0.0f) {
        for (k = 0; k < n; k++) {
            for (i = 0; i < n; i++) {
                p = ip[i];
                t = (p == k) ? 1.0f : 0.0f;
                for (j = 0; j < i; j++) {
                    t -= m_tmp[p * n + j] * inv[j * n + k];
                }
                inv[i * n + k] = t;
            }
            for (i = n - 1; i >= 0; i--) {
                p = ip[i];
                t = inv[i * n + k];
                for (j = i + 1; j < n; j++) {
                    t -= m_tmp[p * n + j] * inv[j * n + k];
                }
                inv[i * n + k] = t / m_tmp[p * n + i];
            }
        }
    }
    Mem_free(m_tmp);
    Mem_free(ip);
    return det;
}

// out = mtx (n x m) * v
void MtxNNMultVecSR(int n, int m, f32* mtx, f32* v, f32* out)
{
    int i;
    int j;

    for (i = 0; i < n; i++) {
        out[i] = 0.0f;
        for (j = 0; j < m; j++) {
            out[i] += mtx[m * i + j] * v[j];
        }
    }
}

// Project `p` along `dir` onto the plane (plane_p, plane_n).
void OrthographicProjection(Vec* p, Vec* out, Vec* dir, Vec* plane_p, Vec* plane_n)
{
    Vec d;
    f32 s;

    PSVECSubtract(plane_p, p, &d);
    s = PSVECDotProduct(plane_n, &d);
    s /= PSVECDotProduct(plane_n, dir);
    PSVECScale(dir, out, s);
    PSVECAdd(p, out, out);
}

f32 IPOW(f32 x, int n)
{
    f32 r = 1.0f;
    int i;

    for (i = 0; i < n; i++) {
        r *= x;
    }
    return r;
}

// Fast reciprocal-square-root based sqrt (one Newton step), as the SDK inline asm.
f32 SQRTF(f32 x)
{
    f32 half = 0.5f;
    f32 three = 3.0f;
    f32 r;

    if (x <= 0.00001f) {
        return 0.0f;
    }
    asm("frsqrte 2, %1\n\t"
        "fmuls 3, 2, 2\n\t"
        "fmuls 4, 2, %2\n\t"
        "fnmsubs 3, 3, %1, %3\n\t"
        "fmuls 2, 3, 4\n\t"
        "fmuls %0, %1, 2"
        : "=f"(r)
        : "f"(x), "f"(half), "f"(three)
        : "fr2", "fr3", "fr4");
    return r;
}

// Never called in this build (see MtxNNPivotSign): the sin/cos accuracy and timing test whose
// strings and local aggregate initializers sit between SQRTF's and COSF's constant pools.
static inline void SinCosTest()
{
    int i;
    f32 a;

    printf("\nsinf\n");
    for (i = -30; i <= 30; i++) {
        a = (f32) i * PI / 10.0f;
        printf("% f:\t% f,% f\tdiff(% f)\n", a, sinf(a), SINF(a), sinf(a) - SINF(a));
    }
    printf("\n");
    printf("\ncosf\n");
    for (i = -30; i <= 30; i++) {
        a = (f32) i * PI / 10.0f;
        printf("% f:\t% f,% f\tdiff(% f)\n", a, cosf(a), COSF(a), cosf(a) - COSF(a));
    }
    printf("sinf =%d\n", 0);
    printf("SINF =%d\n", 0);
    printf("cosf =%d\n", 0);
    printf("COSF =%d\n", 0);
    {
        f64 magic[1] = {4503601774854144.0};
        f32 range[4] = {PI, 3.0f * PI, -3.0f * PI, 0.0f};
        a = (f32) magic[0] + range[i & 3];
    }
}

// Taylor series sin/cos on paired singles: Coeff holds the odd/even coefficients pairwise.
f32 Coeff[10] = {
    1.0000012f, 2.9073722e-06f, -0.16666685f, -5.727683e-06f, 0.008331681f,
    4.281338e-06f, -0.00019622728f, -6.4099936e-07f, 2.363633e-06f, 4.0048576e-08f,
};
f32 powx[2] = {1.0f, 1.0f};
f32 sum[2] = {0.0f, 0.0f};

f32 SINF(f32 x)
{
    f32 r;

    x = LIMIT_ANGLE(x);
    asm volatile(
        "lis 9, Coeff@ha\n\t"
        "li 10, powx@sda21\n\t"
        "addi 9, 9, Coeff@l\n\t"
        "li 11, sum@sda21\n\t"
        "psq_l 3, 0(10), 0, 0\n\t"
        "psq_l 4, 0(11), 0, 0\n\t"
        "ps_merge00 2, 3, %1\n\t"
        "ps_muls0 2, 2, %1\n\t"
        "psq_l 5, 0(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 8(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 16(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 24(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 32(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "ps_sum0 %0, 4, 4, 4"
        : "=f"(r)
        : "f"(x)
        : "r9", "r10", "r11", "fr2", "fr3", "fr4", "fr5");
    return r;
}

f32 COSF(f32 x)
{
    f32 r;

    x = LIMIT_ANGLE(x + 1.5707964f);
    asm volatile(
        "lis 9, Coeff@ha\n\t"
        "li 10, powx@sda21\n\t"
        "addi 9, 9, Coeff@l\n\t"
        "li 11, sum@sda21\n\t"
        "psq_l 3, 0(10), 0, 0\n\t"
        "psq_l 4, 0(11), 0, 0\n\t"
        "ps_merge00 2, 3, %1\n\t"
        "ps_muls0 2, 2, %1\n\t"
        "psq_l 5, 0(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 8(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 16(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 24(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "psq_l 5, 32(9), 0, 0\n\t"
        "ps_muls1 3, 2, 3\n\t"
        "ps_madd 4, 3, 5, 4\n\t"
        "ps_sum0 %0, 4, 4, 4"
        : "=f"(r)
        : "f"(x)
        : "r9", "r10", "r11", "fr2", "fr3", "fr4", "fr5");
    return r;
}

// Wrap an angle into [-PI, PI) (SDK-style inline asm loop).
f32 LIMIT_ANGLE(f32 x)
{
    f32 min = -PI;
    f32 max = PI;
    f32 step = PI2;

    asm("fcmpu 0, %0, %2\n\t"
        "blt 1f\n"
        "0:\n\t"
        "fsubs %0, %0, %3\n\t"
        "fcmpu 0, %0, %2\n\t"
        "bge 0b\n\t"
        "b 2f\n"
        "1:\n\t"
        "fcmpu 0, %0, %1\n\t"
        "bge 2f\n"
        "3:\n\t"
        "fadds %0, %0, %3\n\t"
        "fcmpu 0, %0, %1\n\t"
        "blt 3b\n"
        "2:"
        : "+f"(x)
        : "f"(min), "f"(max), "f"(step)
        : "cr0");
    return x;
}
