// -*- coding: utf-8 -*-
// La bibliotheque de matrices et de vecteurs du SDK Nintendo, en C.
//
// Le SDK en a deux versions : `C_MTX*`, du C ordinaire, et `PSMTX*`, la meme chose ecrite en
// instructions paired-single du Gekko. Le jeu appelle la seconde. Le portage fournit donc les
// noms `PS*`, avec le comportement de la version C - qui est la **definition** de ce que la
// version paired-single calcule, a l'ordre des arrondis pres.
//
// Conventions, parce que s'y tromper donne une scene qui ne plante pas et n'a aucun sens :
//  * `Mtx` est `f32[3][4]` : une transformation affine 3x4, **rangee par lignes**, la quatrieme
//    colonne etant la translation ;
//  * `PSMTXConcat(a, b, ab)` calcule `ab = a * b`, donc b s'applique en premier ;
//  * `MultVec` applique la translation, `MultVecSR` ne l'applique pas - c'est ce qui distingue
//    un point d'une direction, et une normale transformee comme un point pointe n'importe ou.
#include <cstdio>

#include <dolphin.h>
#include <dolphin/mtx.h>

namespace {

inline f32 dot3(const f32 *r, f32 x, f32 y, f32 z) { return r[0] * x + r[1] * y + r[2] * z; }

} // namespace

extern "C" {

// ---------------------------------------------------------------- matrices

void PSMTXIdentity(Mtx m)
{
    m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
    m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = 0.0f;
}

void PSMTXCopy(const Mtx src, Mtx dst)
{
    if (src == dst)
        return;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j)
            dst[i][j] = src[i][j];
}

void PSMTXConcat(const Mtx a, const Mtx b, Mtx ab)
{
    // Par une matrice intermediaire : le SDK autorise ab == a ou ab == b, et le jeu s'en sert.
    Mtx t;
    for (int i = 0; i < 3; ++i) {
        t[i][0] = a[i][0] * b[0][0] + a[i][1] * b[1][0] + a[i][2] * b[2][0];
        t[i][1] = a[i][0] * b[0][1] + a[i][1] * b[1][1] + a[i][2] * b[2][1];
        t[i][2] = a[i][0] * b[0][2] + a[i][1] * b[1][2] + a[i][2] * b[2][2];
        t[i][3] = a[i][0] * b[0][3] + a[i][1] * b[1][3] + a[i][2] * b[2][3] + a[i][3];
    }
    PSMTXCopy(t, ab);
}

void PSMTXConcatArray(const Mtx a, const Mtx *srcBase, Mtx *dstBase, u32 count)
{
    for (u32 i = 0; i < count; ++i)
        PSMTXConcat(a, srcBase[i], dstBase[i]);
}

void PSMTXTranspose(const Mtx src, Mtx xPose)
{
    Mtx t;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            t[i][j] = src[j][i];
    // Le SDK met la translation a zero : transposer une affine n'a de sens que pour sa partie
    // lineaire.
    t[0][3] = t[1][3] = t[2][3] = 0.0f;
    PSMTXCopy(t, xPose);
}

u32 PSMTXInverse(const Mtx src, Mtx inv)
{
    const f32 det =
        src[0][0] * (src[1][1] * src[2][2] - src[2][1] * src[1][2]) -
        src[1][0] * (src[0][1] * src[2][2] - src[2][1] * src[0][2]) +
        src[2][0] * (src[0][1] * src[1][2] - src[1][1] * src[0][2]);
    if (det == 0.0f)
        return 0;   // le SDK rend 0 sur une matrice singuliere, et le jeu teste ce retour

    const f32 d = 1.0f / det;
    Mtx t;
    t[0][0] = (src[1][1] * src[2][2] - src[2][1] * src[1][2]) * d;
    t[0][1] = -(src[0][1] * src[2][2] - src[2][1] * src[0][2]) * d;
    t[0][2] = (src[0][1] * src[1][2] - src[1][1] * src[0][2]) * d;
    t[1][0] = -(src[1][0] * src[2][2] - src[2][0] * src[1][2]) * d;
    t[1][1] = (src[0][0] * src[2][2] - src[2][0] * src[0][2]) * d;
    t[1][2] = -(src[0][0] * src[1][2] - src[1][0] * src[0][2]) * d;
    t[2][0] = (src[1][0] * src[2][1] - src[2][0] * src[1][1]) * d;
    t[2][1] = -(src[0][0] * src[2][1] - src[2][0] * src[0][1]) * d;
    t[2][2] = (src[0][0] * src[1][1] - src[1][0] * src[0][1]) * d;
    // La translation inverse est -A^-1 * t, pas -t : l'oublier decale toute la scene.
    t[0][3] = -(t[0][0] * src[0][3] + t[0][1] * src[1][3] + t[0][2] * src[2][3]);
    t[1][3] = -(t[1][0] * src[0][3] + t[1][1] * src[1][3] + t[1][2] * src[2][3]);
    t[2][3] = -(t[2][0] * src[0][3] + t[2][1] * src[1][3] + t[2][2] * src[2][3]);
    PSMTXCopy(t, inv);
    return 1;
}

u32 PSMTXInvXpose(const Mtx src, Mtx invX)
{
    Mtx t;
    if (!PSMTXInverse(src, t))
        return 0;
    PSMTXTranspose(t, invX);
    return 1;
}

void PSMTXRotTrig(Mtx m, char axis, f32 sinA, f32 cosA)
{
    PSMTXIdentity(m);
    switch (axis) {
    case 'x': case 'X':
        m[1][1] = cosA;  m[1][2] = -sinA;
        m[2][1] = sinA;  m[2][2] = cosA;
        break;
    case 'y': case 'Y':
        m[0][0] = cosA;  m[0][2] = sinA;
        m[2][0] = -sinA; m[2][2] = cosA;
        break;
    default:
        m[0][0] = cosA;  m[0][1] = -sinA;
        m[1][0] = sinA;  m[1][1] = cosA;
        break;
    }
}

void PSMTXRotRad(Mtx m, char axis, f32 rad)
{
    PSMTXRotTrig(m, axis, __builtin_sinf(rad), __builtin_cosf(rad));
}

void PSMTXRotAxisRad(Mtx m, const Vec *axis, f32 rad)
{
    // Rodrigues. L'axe est normalise d'abord : le SDK le fait, et le jeu passe parfois un axe
    // deja mis a l'echelle.
    f32 x = axis->x, y = axis->y, z = axis->z;
    const f32 n = __builtin_sqrtf(x * x + y * y + z * z);
    if (n > 0.0f) {
        x /= n; y /= n; z /= n;
    }
    const f32 s = __builtin_sinf(rad), c = __builtin_cosf(rad), t = 1.0f - c;

    m[0][0] = t * x * x + c;      m[0][1] = t * x * y - s * z;  m[0][2] = t * x * z + s * y;  m[0][3] = 0.0f;
    m[1][0] = t * x * y + s * z;  m[1][1] = t * y * y + c;      m[1][2] = t * y * z - s * x;  m[1][3] = 0.0f;
    m[2][0] = t * x * z - s * y;  m[2][1] = t * y * z + s * x;  m[2][2] = t * z * z + c;      m[2][3] = 0.0f;
}

void PSMTXTrans(Mtx m, f32 xT, f32 yT, f32 zT)
{
    PSMTXIdentity(m);
    m[0][3] = xT; m[1][3] = yT; m[2][3] = zT;
}

void PSMTXTransApply(const Mtx src, Mtx dst, f32 xT, f32 yT, f32 zT)
{
    PSMTXCopy(src, dst);
    dst[0][3] = src[0][3] + xT;
    dst[1][3] = src[1][3] + yT;
    dst[2][3] = src[2][3] + zT;
}

void PSMTXScale(Mtx m, f32 xS, f32 yS, f32 zS)
{
    PSMTXIdentity(m);
    m[0][0] = xS; m[1][1] = yS; m[2][2] = zS;
}

void PSMTXScaleApply(const Mtx src, Mtx dst, f32 xS, f32 yS, f32 zS)
{
    for (int j = 0; j < 4; ++j) {
        dst[0][j] = src[0][j] * xS;
        dst[1][j] = src[1][j] * yS;
        dst[2][j] = src[2][j] * zS;
    }
}

void PSMTXQuat(Mtx m, const Quaternion *q)
{
    const f32 x = q->x, y = q->y, z = q->z, w = q->w;
    const f32 n = x * x + y * y + z * z + w * w;
    const f32 s = (n > 0.0f) ? (2.0f / n) : 0.0f;
    const f32 xs = x * s, ys = y * s, zs = z * s;
    const f32 wx = w * xs, wy = w * ys, wz = w * zs;
    const f32 xx = x * xs, xy = x * ys, xz = x * zs;
    const f32 yy = y * ys, yz = y * zs, zz = z * zs;

    m[0][0] = 1.0f - (yy + zz); m[0][1] = xy - wz;          m[0][2] = xz + wy;          m[0][3] = 0.0f;
    m[1][0] = xy + wz;          m[1][1] = 1.0f - (xx + zz); m[1][2] = yz - wx;          m[1][3] = 0.0f;
    m[2][0] = xz - wy;          m[2][1] = yz + wx;          m[2][2] = 1.0f - (xx + yy); m[2][3] = 0.0f;
}

void PSMTXReflect(Mtx m, const Vec *p, const Vec *n)
{
    const f32 nx = n->x, ny = n->y, nz = n->z;
    const f32 d = nx * p->x + ny * p->y + nz * p->z;
    m[0][0] = 1.0f - 2.0f * nx * nx; m[0][1] = -2.0f * nx * ny;       m[0][2] = -2.0f * nx * nz;       m[0][3] = 2.0f * d * nx;
    m[1][0] = -2.0f * ny * nx;       m[1][1] = 1.0f - 2.0f * ny * ny; m[1][2] = -2.0f * ny * nz;       m[1][3] = 2.0f * d * ny;
    m[2][0] = -2.0f * nz * nx;       m[2][1] = -2.0f * nz * ny;       m[2][2] = 1.0f - 2.0f * nz * nz; m[2][3] = 2.0f * d * nz;
}

// ---------------------------------------------------------------- matrice x vecteur

void PSMTXMultVec(const Mtx m, const Vec *src, Vec *dst)
{
    const f32 x = src->x, y = src->y, z = src->z;   // src peut etre dst
    dst->x = dot3(m[0], x, y, z) + m[0][3];
    dst->y = dot3(m[1], x, y, z) + m[1][3];
    dst->z = dot3(m[2], x, y, z) + m[2][3];
}

void PSMTXMultVecArray(const Mtx m, const Vec *srcBase, Vec *dstBase, u32 count)
{
    for (u32 i = 0; i < count; ++i)
        PSMTXMultVec(m, &srcBase[i], &dstBase[i]);
}

void PSMTXMultVecSR(const Mtx m, const Vec *src, Vec *dst)
{
    const f32 x = src->x, y = src->y, z = src->z;
    dst->x = dot3(m[0], x, y, z);
    dst->y = dot3(m[1], x, y, z);
    dst->z = dot3(m[2], x, y, z);
}

void PSMTXMultVecArraySR(const Mtx m, const Vec *srcBase, Vec *dstBase, u32 count)
{
    for (u32 i = 0; i < count; ++i)
        PSMTXMultVecSR(m, &srcBase[i], &dstBase[i]);
}

// ---------------------------------------------------------------- vecteurs

void PSVECAdd(const Vec *a, const Vec *b, Vec *ab)
{
    ab->x = a->x + b->x; ab->y = a->y + b->y; ab->z = a->z + b->z;
}

void PSVECSubtract(const Vec *a, const Vec *b, Vec *a_b)
{
    a_b->x = a->x - b->x; a_b->y = a->y - b->y; a_b->z = a->z - b->z;
}

void PSVECScale(const Vec *src, Vec *dst, f32 scale)
{
    dst->x = src->x * scale; dst->y = src->y * scale; dst->z = src->z * scale;
}

f32 PSVECDotProduct(const Vec *a, const Vec *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

void PSVECCrossProduct(const Vec *a, const Vec *b, Vec *axb)
{
    const f32 x = a->y * b->z - a->z * b->y;   // a peut etre axb
    const f32 y = a->z * b->x - a->x * b->z;
    const f32 z = a->x * b->y - a->y * b->x;
    axb->x = x; axb->y = y; axb->z = z;
}

f32 PSVECMag(const Vec *v)
{
    return __builtin_sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

f32 PSVECSquareMag(const Vec *v)
{
    return v->x * v->x + v->y * v->y + v->z * v->z;
}

f32 PSVECSquareDistance(const Vec *a, const Vec *b)
{
    const f32 dx = a->x - b->x, dy = a->y - b->y, dz = a->z - b->z;
    return dx * dx + dy * dy + dz * dz;
}

f32 PSVECDistance(const Vec *a, const Vec *b)
{
    return __builtin_sqrtf(PSVECSquareDistance(a, b));
}

void PSVECNormalize(const Vec *src, Vec *unit)
{
    const f32 m = PSVECMag(src);
    // Un vecteur nul rend un vecteur nul : le SDK divise sans garde et produit un NaN, qui se
    // propage ensuite dans toute la scene sans jamais dire d'ou il vient.
    if (m > 0.0f) {
        const f32 k = 1.0f / m;
        unit->x = src->x * k; unit->y = src->y * k; unit->z = src->z * k;
    } else {
        unit->x = unit->y = unit->z = 0.0f;
    }
}

void PSVECReflect(const Vec *src, const Vec *normal, Vec *dst)
{
    const f32 d = 2.0f * PSVECDotProduct(src, normal);
    dst->x = src->x - d * normal->x;
    dst->y = src->y - d * normal->y;
    dst->z = src->z - d * normal->z;
}

// ---------------------------------------------------------------- projection de texture

// Matrice de projection d'une lumiere sur une texture : le resultat est en coordonnees de
// texture (s, t) dans [0, 1], d'ou l'echelle de 0,5 et le decalage. C'est ce que RE4 utilise
// pour ses projecteurs et ses ombres projetees.
void C_MTXLightPerspective(Mtx m, f32 fovY, f32 aspect, f32 scaleS, f32 scaleT,
                           f32 transS, f32 transT)
{
    const f32 angle = fovY * 0.5f * 0.01745329252f;
    const f32 cot = __builtin_cosf(angle) / __builtin_sinf(angle);

    m[0][0] = (cot / aspect) * scaleS;
    m[0][1] = 0.0f;
    m[0][2] = -transS;
    m[0][3] = 0.0f;

    m[1][0] = 0.0f;
    m[1][1] = cot * scaleT;
    m[1][2] = -transT;
    m[1][3] = 0.0f;

    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = -1.0f;
    m[2][3] = 0.0f;
}


// ---------------------------------------------------------------- projection et camera

// Matrices 4x4 de projection. **La convention de profondeur du GameCube n'est pas celle
// d'OpenGL** : GX borne z dans [-w, 0] et non [-w, +w]. C'est la forme d'origine qui est
// reproduite ici ; c'est la couche GX (port/gx.cpp) qui fait la conversion au moment de
// construire la matrice envoyee au shader, en un seul endroit.
void C_MTXPerspective(Mtx44 m, f32 fovY, f32 aspect, f32 n, f32 f)
{
    const f32 angle = fovY * 0.5f * 0.01745329252f;
    const f32 cot = __builtin_cosf(angle) / __builtin_sinf(angle);

    m[0][0] = cot / aspect; m[0][1] = 0.0f; m[0][2] = 0.0f;              m[0][3] = 0.0f;
    m[1][0] = 0.0f;         m[1][1] = cot;  m[1][2] = 0.0f;              m[1][3] = 0.0f;
    // Termes de profondeur du SDK (src/lib/mtx44.c) : z de [-n, -f] vers [-1, 0] (convention GX). La
    // version d'avant (f/(n-f), nf/(n-f)) donnait [0, 1] : invisible avec l'ancien rendu de test, mais le
    // rendu Nightjar, fidele au GX, rejetait tout le decor en perspective (11/11, phase N3).
    const f32 tmp = 1.0f / (f - n);
    m[2][0] = 0.0f;         m[2][1] = 0.0f; m[2][2] = -n * tmp;          m[2][3] = tmp * -(f * n);
    m[3][0] = 0.0f;         m[3][1] = 0.0f; m[3][2] = -1.0f;             m[3][3] = 0.0f;
}

// Pyramide de vue quelconque (src/lib/mtx44.c, memes formules) : remplace la souche faible.
void C_MTXFrustum(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f)
{
    f32 tmp = 1.0f / (r - l);
    m[0][0] = (2.0f * n) * tmp; m[0][1] = 0.0f; m[0][2] = tmp * (r + l); m[0][3] = 0.0f;
    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f; m[1][1] = (2.0f * n) * tmp; m[1][2] = tmp * (t + b); m[1][3] = 0.0f;
    tmp = 1.0f / (f - n);
    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = -n * tmp; m[2][3] = tmp * -(f * n);
    m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = -1.0f; m[3][3] = 0.0f;
}

void C_MTXOrtho(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f)
{
    const f32 tmp0 = 1.0f / (r - l);
    const f32 tmp1 = 1.0f / (t - b);
    const f32 tmp2 = 1.0f / (f - n);

    m[0][0] = 2.0f * tmp0; m[0][1] = 0.0f;        m[0][2] = 0.0f;  m[0][3] = -(r + l) * tmp0;
    m[1][0] = 0.0f;        m[1][1] = 2.0f * tmp1; m[1][2] = 0.0f;  m[1][3] = -(t + b) * tmp1;
    m[2][0] = 0.0f;        m[2][1] = 0.0f;        m[2][2] = -tmp2; m[2][3] = -f * tmp2;
    m[3][0] = 0.0f;        m[3][1] = 0.0f;        m[3][2] = 0.0f;  m[3][3] = 1.0f;
}

void C_MTXLookAt(Mtx m, const Point3d *camPos, const Vec *camUp, const Point3d *target)
{
    Vec look, right, up;
    look.x = camPos->x - target->x;
    look.y = camPos->y - target->y;
    look.z = camPos->z - target->z;
    PSVECNormalize(&look, &look);            // l'axe z de la camera pointe **vers l arriere**
    PSVECCrossProduct(camUp, &look, &right);
    PSVECNormalize(&right, &right);
    PSVECCrossProduct(&look, &right, &up);

    m[0][0] = right.x; m[0][1] = right.y; m[0][2] = right.z;
    m[1][0] = up.x;    m[1][1] = up.y;    m[1][2] = up.z;
    m[2][0] = look.x;  m[2][1] = look.y;  m[2][2] = look.z;
    m[0][3] = -(right.x * camPos->x + right.y * camPos->y + right.z * camPos->z);
    m[1][3] = -(up.x * camPos->x + up.y * camPos->y + up.z * camPos->z);
    m[2][3] = -(look.x * camPos->x + look.y * camPos->y + look.z * camPos->z);
}

void PSMTX44MultVec(const Mtx44 m, const Vec *src, Vec *dst)
{
    const f32 x = src->x, y = src->y, z = src->z;
    const f32 w = m[3][0] * x + m[3][1] * y + m[3][2] * z + m[3][3];
    const f32 iw = (w != 0.0f) ? (1.0f / w) : 1.0f;
    dst->x = (m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3]) * iw;
    dst->y = (m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3]) * iw;
    dst->z = (m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3]) * iw;
}

void PSMTX44MultVecSR(const Mtx44 m, const Vec *src, Vec *dst)
{
    const f32 x = src->x, y = src->y, z = src->z;
    dst->x = m[0][0] * x + m[0][1] * y + m[0][2] * z;
    dst->y = m[1][0] * x + m[1][1] * y + m[1][2] * z;
    dst->z = m[2][0] * x + m[2][1] * y + m[2][2] * z;
}

void PSMTX44MultVecArray(const Mtx44 m, const Vec *srcBase, Vec *dstBase, u32 count)
{
    for (u32 i = 0; i < count; ++i)
        PSMTX44MultVec(m, &srcBase[i], &dstBase[i]);
}

void PSMTX44MultVecArraySR(const Mtx44 m, const Vec *srcBase, Vec *dstBase, u32 count)
{
    for (u32 i = 0; i < count; ++i)
        PSMTX44MultVecSR(m, &srcBase[i], &dstBase[i]);
}

// Mtx (3 lignes de 4) -> ROMtx (4 lignes de 3) : une simple transposition, mais **c'est elle**
// qui donne a la palette de matrices la disposition que lit le noyau de skinning - trois vec3
// qui sont les colonnes de la transformation, puis la translation.
void PSMTXReorder(const Mtx src, ROMtx dest)
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j)
            dest[j][i] = src[i][j];
}

void C_VECReflect(const Vec *src, const Vec *normal, Vec *dst)
{
    PSVECReflect(src, normal, dst);
}

} // extern "C"
