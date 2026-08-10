#ifndef LIBGPU_H
#define LIBGPU_H

// PlayStation libgpu ordering-table emulation on GX (game/libgpu.cpp). OT entries are the next
// pointer with bit 31 cleared for empty slots (ClearOTagR) and set for real primitives; the list
// ends with -1. Primitive layouts differ from the PSX ones: `code` is a full word (low 5 bits
// select the GPU_* drawer) and colors come before coordinates. Sizes are what the make_f*
// conversion locals occupy; the fields after the coordinates are unknown padding.

#include "types.h"

typedef struct {
    u8 r, g, b, cd;
} GpuColor;

#define GPU_G3 0
#define GPU_G4 1
#define GPU_F3 2
#define GPU_F4 3
#define GPU_TILE 4
#define GPU_LG2 5
#define GPU_LG3 6
#define GPU_LG4 7
#define GPU_LF2 8
#define GPU_LF3 9
#define GPU_LF4 10

typedef struct {
    u32 tag;          // 0x00
    u32 code;         // 0x04
    GpuColor c0;      // 0x08
    GpuColor c1;      // 0x0C
    GpuColor c2;      // 0x10
    s16 x0, y0;       // 0x14
    s16 x1, y1;       // 0x18
    s16 x2, y2;       // 0x1C
    s16 z0, z1, z2;   // 0x20
    u8 pad_26[0x1A];
} POLY_G3;            // 0x40 (make_f3's local is 0x40 bytes)

typedef struct {
    u32 tag;          // 0x00
    u32 code;         // 0x04
    GpuColor c0;      // 0x08
    GpuColor c1;      // 0x0C
    GpuColor c2;      // 0x10
    GpuColor c3;      // 0x14
    s16 x0, y0;       // 0x18
    s16 x1, y1;       // 0x1C
    s16 x2, y2;       // 0x20
    s16 x3, y3;       // 0x24
    s16 z0, z1, z2, z3;  // 0x28
    u8 pad_30[0x10];
} POLY_G4;            // 0x40

typedef struct {
    u32 tag;          // 0x00
    u32 code;         // 0x04
    GpuColor c0;      // 0x08
    s16 x0, y0;       // 0x0C
    s16 x1, y1;       // 0x10
    s16 x2, y2;       // 0x14
    s16 z0, z1, z2;   // 0x18
} POLY_F3;            // 0x20

typedef struct {
    u32 tag;          // 0x00
    u32 code;         // 0x04
    GpuColor c0;      // 0x08
    s16 x0, y0;       // 0x0C
    s16 x1, y1;       // 0x10
    s16 x2, y2;       // 0x14
    s16 x3, y3;       // 0x18
    s16 z0, z1, z2, z3;  // 0x1C
} POLY_F4;            // 0x24

typedef struct {
    u32 tag;          // 0x00
    u32 code;         // 0x04
    GpuColor c0;      // 0x08
    s16 x0, y0;       // 0x0C
    s16 w, h;         // 0x10
    s16 z0;           // 0x14
} TILE;               // 0x18

typedef struct {
    u32 tag;          // 0x00
    u32 code;         // 0x04
    GpuColor c0;      // 0x08
    GpuColor c1;      // 0x0C
    s16 x0, y0;       // 0x10
    s16 x1, y1;       // 0x14
    s16 z0, z1;       // 0x18
    u8 pad_1C[4];
} LINE_G2;            // 0x20

typedef POLY_G3 LINE_G3;
typedef POLY_G4 LINE_G4;

typedef struct {
    u32 tag;          // 0x00
    u32 code;         // 0x04
    GpuColor c0;      // 0x08
    s16 x0, y0;       // 0x0C
    s16 x1, y1;       // 0x10
    s16 z0, z1;       // 0x14
} LINE_F2;            // 0x18

typedef POLY_F3 LINE_F3;
typedef POLY_F4 LINE_F4;

#ifdef __cplusplus
extern "C" {
#endif
void AddPrim(u32* ot, u32* prim);
void DelPrim(u32* ot, u32* prim);
void ClearOTagR(u32* ot, int n);
void DrawOTag(u32* ot);
#ifdef __cplusplus
}
#endif

#endif
