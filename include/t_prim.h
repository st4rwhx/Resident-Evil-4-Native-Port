#ifndef T_PRIM_H
#define T_PRIM_H

#include "types.h"
#include "vec.h"
#include "gx.h"

// Debug primitive drawing environment (game/t_prim.cpp).
struct TprimRect {
    f32 x, y, w, h;
};

struct TprimView {
    TprimRect rect;
    f32 nearz;
    f32 farz;
};

void TprimInitEnv2D3D(TprimView* view, MtxPtr proj, MtxPtr view_mtx);
void TprimDraw3D(u32 blend);
void TprimSetBlend(u32 blend);
// The tool modules link the full t_prim (tools/t_prim.cpp); the DOL link dead-stripped these.
void TprimDraw2D(u32 blend);
void TprimDrawPolyFn(Vec* v, GXColor* col, u16 n);
void TprimDrawCursor(Vec* pos, GXColor* col, f32 z);
void TprimDrawMtxDirection(Mtx m, GXColor* fill, GXColor* line);

// The full build (Tools REL, tools/t_prim.cpp with TPRIM_FULL): 2D-only environment, line strips, tiles,
// the hit marker (Htr) and its cone, and the s16 vertex variants.
struct S16Vec {
    s16 x, y, z;
};

void TprimInitEnv2D(TprimRect* rect);
void TprimDrawLineFn(Vec* v, GXColor* col, u16 n);
void TprimDrawTile2D(TprimRect* rect, GXColor* col, f32 z);
void TprimDrawHtr(Vec* pos, GXColor* col);
void TprimDrawHtrCone(Vec* pos, GXColor* col);
void TprimDrawFrameFn_s16(S16Vec* v, GXColor* col, u16 n);
void TprimDrawPolyFn_s16(S16Vec* v, GXColor* col, u16 n);

#endif
