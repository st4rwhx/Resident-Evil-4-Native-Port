// -*- coding: utf-8 -*-
// Les variantes « paired-single » de la bibliotheque matricielle.
//
// Le SDK Dolphin publie chaque fonction de MTX en deux exemplaires : `C_MTXConcat`, ecrite en
// C, et `PSMTXConcat`, ecrite a la main avec les instructions paired-single du Gekko. Les
// en-tetes du jeu choisissent la seconde (`#define MTXConcat PSMTXConcat`), si bien que tout
// le code appelle des symboles `PS...`.
//
// Elles calculent exactement la meme chose. aurora ne fournit que les versions en C, ce qui
// est le bon choix : les instructions paired-single n'existent que sur le Gekko, et la
// difference n'etait qu'une affaire de debit. On redirige donc, une ligne par fonction.
//
// Ce fichier ne contient que ce que l'editeur de liens a reellement reclame. Ajouter les 70
// autres serait du code mort : si le jeu en appelle une de plus un jour, il le dira.
#include <dolphin/mtx.h>

// Les en-tetes du jeu ne declarent pas toutes les versions en C ; celles-ci sont dans
// l en-tete d aurora, que l ordre d inclusion nous cache. On les reprend a l identique.
extern "C" void C_MTXReorder(const Mtx src, ROMtx dst);

extern "C" {

void PSMTXIdentity(Mtx m) { C_MTXIdentity(m); }
void PSMTXCopy(const Mtx src, Mtx dst) { C_MTXCopy(src, dst); }
void PSMTXConcat(const Mtx a, const Mtx b, Mtx ab) { C_MTXConcat(a, b, ab); }
u32 PSMTXInverse(const Mtx src, Mtx inv) { return C_MTXInverse(src, inv); }
void PSMTXRotRad(Mtx m, char axis, f32 rad) { C_MTXRotRad(m, axis, rad); }
void PSMTXRotAxisRad(Mtx m, const Vec *axis, f32 rad) { C_MTXRotAxisRad(m, axis, rad); }
void PSMTXMultVec(const Mtx m, const Vec *src, Vec *dst) { C_MTXMultVec(m, src, dst); }
void PSMTXMultVecSR(const Mtx m, const Vec *src, Vec *dst) { C_MTXMultVecSR(m, src, dst); }

void PSVECAdd(const Vec *a, const Vec *b, Vec *ab) { C_VECAdd(a, b, ab); }
void PSVECSubtract(const Vec *a, const Vec *b, Vec *a_b) { C_VECSubtract(a, b, a_b); }
void PSVECScale(const Vec *src, Vec *dst, f32 scale) { C_VECScale(src, dst, scale); }
f32 PSVECDotProduct(const Vec *a, const Vec *b) { return C_VECDotProduct(a, b); }
void PSVECCrossProduct(const Vec *a, const Vec *b, Vec *axb) { C_VECCrossProduct(a, b, axb); }

void PSMTXTrans(Mtx m, f32 x, f32 y, f32 z) { C_MTXTrans(m, x, y, z); }
void PSMTXTransApply(const Mtx src, Mtx dst, f32 x, f32 y, f32 z) { C_MTXTransApply(src, dst, x, y, z); }
void PSMTXScale(Mtx m, f32 x, f32 y, f32 z) { C_MTXScale(m, x, y, z); }
void PSMTXTranspose(const Mtx src, Mtx xPose) { C_MTXTranspose(src, xPose); }
void PSMTXReorder(const Mtx src, ROMtx dst) { C_MTXReorder(src, dst); }
void PSMTXQuat(Mtx m, const Quaternion *q) { C_MTXQuat(m, q); }
void PSMTXMultVecArray(const Mtx m, const Vec *srcBase, Vec *dstBase, u32 count)
{
    C_MTXMultVecArray(m, srcBase, dstBase, count);
}

// Celle-ci, aurora ne l a pas du tout - ni en paired-single ni en C. C est la multiplication
// par une matrice 4x4 complete, avec la division par w : la projection. Ecrite ici d apres sa
// definition dans le SDK, et non deduite de PSMTXMultVec, qui ignore la quatrieme ligne.
void PSMTX44MultVec(const Mtx44 m, const Vec *src, Vec *dst)
{
    const f32 x = src->x, y = src->y, z = src->z;
    f32 w = m[3][0] * x + m[3][1] * y + m[3][2] * z + m[3][3];
    if (w != 0.0f)
        w = 1.0f / w;
    dst->x = (m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3]) * w;
    dst->y = (m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3]) * w;
    dst->z = (m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3]) * w;
}

} // extern "C"
