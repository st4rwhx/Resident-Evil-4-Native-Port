#ifndef PATH_H
#define PATH_H

// game/path.cpp: paths (poly-lines with per-vertex parts weights, moved along by distance) and
// B-spline "FuncPath" curves (id_sys). PathHasWeight/PathGetLength/PathGetPos/PathGetPosEm are
// declared in esp.h (void* path).

#include "types.h"
#include "vec.h"

class cModel;

// One path vertex (0x28 bytes).
struct PathVtx {
    Vec pos;        // 0x00
    Vec nrm;        // 0x0C  up vector (PathGetMatEm interpolates it for the matrix)
    f32 dist;       // 0x18  distance from the path start
    u8 partsNo[3];  // 0x1C  model parts the vertex follows (PathGetVtxMat)
    u8 nWeight;     // 0x1F  entries in partsNo/weight, 0 = fixed vertex
    u8 weight[3];   // 0x20  percent
    u8 pad_23[5];
};

struct Path {
    u16 num;        // 0x00
    u8 pad_2[2];
    PathVtx vtx[1]; // 0x04  num entries
};

// B-spline control points (id_sys path0).
struct FuncPathData {
    s8 k;         // 0x00  spline order
    u8 pad_1[6];
    s8 n;         // 0x07  control point count
    Vec pos[1];   // 0x08
};

// Parametrised B-spline (id_sys path1).
struct FuncPathWork {
    int k;        // 0x00
    int n;        // 0x04
    Vec alpha[1]; // 0x08  n coefficients
};

extern "C" {
int PathGetMatEm(void* path, cModel* model, f32 dist, u16* seg, Mtx out);
void PathGetVtxMat(Mtx out, cModel* model, PathVtx* v);
int FuncPathParametrize(void* path, void* data);
int FuncPathCalc(void* path, void* data, Vec* out, f32 t);
void FuncPathClear(void* path);
}

#endif
