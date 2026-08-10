#include "types.h"
#include "vec.h"
#include "gx.h"
#include "t_prim.h"

// Debug primitive drawing for the tool modules (D:/Bio4/Prog/t_prim.cpp, the same object in every
// t_*/Tools REL that draws). The DOL's game/t_prim.cpp is the dead-stripped version of this file: the
// linker kept only the functions the game calls, so this is where TprimDraw2D/TprimDrawPolyFn/
// TprimDrawCursor/TprimDrawMtxDirection come from (their constant pools survived in the DOL as the
// 0x60 anonymous .rodata words in front of t_prim's data).

void CameraCurrentProjection();

static void set_attr_common();
static void set_attr_f32();
static void set_vtx_flat_f32(Vec* v, GXColor* col, u16 n);
#ifdef TPRIM_FULL
// The Tools REL carries the full file (18 functions, src/Tools/t_prim.cpp defines TPRIM_FULL and includes
// this one); t_emlist/t_camera have the build without the 2D/s16/Htr helpers.
static void set_attr_s16();
static void set_vtx_flat_s16(S16Vec* v, GXColor* col, u16 n);
#endif

static TprimView Vrect = {{0.0f, 0.0f, 512.0f, 448.0f}, 0.0f, 1.0f};
static TprimRect Orect;
static MtxPtr ProjMtx;
static MtxPtr ViewMtx;
static int FlipMode = 0;
u8 ToolBuffer[0x100] __attribute__((aligned(32)));

void TprimInitEnv2D3D(TprimView* view, MtxPtr proj, MtxPtr view_mtx)
{
    Orect = view->rect;
    Vrect = *view;
    ProjMtx = proj;
    ViewMtx = view_mtx;
    FlipMode = 0;
}

#ifdef TPRIM_FULL
void TprimInitEnv2D(TprimRect* rect)
{
    Orect = *rect;
}
#endif

void TprimDraw2D(u32 blend)
{
    Mtx44 proj;
    Mtx pos;

    C_MTXOrtho(proj, Orect.y, Orect.h, Orect.x, Orect.w, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(pos);
    GXSetCurrentMtx(0);
    GXLoadPosMtxImm(pos, 0);
    TprimSetBlend(blend);
    set_attr_common();
}

void TprimDraw3D(u32 blend)
{
    CameraCurrentProjection();
    GXSetCurrentMtx(0);
    GXLoadPosMtxImm(ViewMtx, 0);
    TprimSetBlend(blend);
    set_attr_common();
}

void TprimSetBlend(u32 blend)
{
    static u32 bl[3][4] = {
        {0, 1, 0, 0},
        {1, 1, 1, 0},
        {1, 0, 2, 0},
    };

    if (blend <= 2) {
        GXSetBlendMode(bl[blend][0], bl[blend][1], bl[blend][2], bl[blend][3]);
        GXSetColorUpdate(1);
    }
}

static void set_attr_common()
{
    GXSetCullMode(0);
    if (FlipMode & 1) {
        GXSetZMode(1, 3, 1);
    } else {
        GXSetZMode(0, 3, 0);
    }
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOp(0, 4);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetNumChans(1);
    GXSetChanCtrl(0, 0, 0, 1, 1, 0, 2);
    GXSetLineWidth(6, 0);
    set_attr_f32();
}

static void set_attr_f32()
{
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
}

#ifdef TPRIM_FULL
static void set_attr_s16()
{
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
}

void TprimDrawLineFn(Vec* v, GXColor* col, u16 n)
{
    GXBegin(0xB0, 0, n);
    set_vtx_flat_f32(v, col, n);
}
#endif

void TprimDrawPolyFn(Vec* v, GXColor* col, u16 n)
{
    GXBegin(0x80, 0, n);
    set_vtx_flat_f32(v, col, n);
}

#ifdef TPRIM_FULL
void TprimDrawTile2D(TprimRect* rect, GXColor* col, f32 z)
{
    GXBegin(0x80, 0, 4);
    GXPosition3f32(rect->x, rect->y, z);
    GXColor4u8(col->r, col->g, col->b, col->a);
    GXPosition3f32(rect->x + rect->w, rect->y, z);
    GXColor4u8(col->r, col->g, col->b, col->a);
    GXPosition3f32(rect->x + rect->w, rect->y + rect->h, z);
    GXColor4u8(col->r, col->g, col->b, col->a);
    GXPosition3f32(rect->x, rect->y + rect->h, z);
    GXColor4u8(col->r, col->g, col->b, col->a);
}
#endif

// Cross-hair of four triangles around `pos` (the last one's tip has z 0 in the original).
void TprimDrawCursor(Vec* pos, GXColor* col, f32 z)
{
    Vec v[3];

    v[0].x = pos->x;
    v[0].y = pos->y - 2.0f;
    v[0].z = z;
    v[1].x = pos->x - 4.0f;
    v[1].y = v[0].y - 8.0f;
    v[1].z = z;
    v[2].x = pos->x + 4.0f;
    v[2].y = v[1].y;
    v[2].z = z;
    TprimDrawPolyFn(v, col, 3);

    v[0].x = pos->x;
    v[0].y = pos->y + 2.0f;
    v[0].z = z;
    v[1].x = pos->x + 4.0f;
    v[1].y = v[0].y + 8.0f;
    v[1].z = z;
    v[2].x = pos->x - 4.0f;
    v[2].y = v[1].y;
    v[2].z = z;
    TprimDrawPolyFn(v, col, 3);

    v[0].x = pos->x - 2.0f;
    v[0].y = pos->y;
    v[0].z = z;
    v[1].x = v[0].x - 8.0f;
    v[1].y = pos->y + 4.0f;
    v[1].z = z;
    v[2].x = v[1].x;
    v[2].y = pos->y - 4.0f;
    v[2].z = z;
    TprimDrawPolyFn(v, col, 3);

    v[0].x = pos->x + 2.0f;
    v[0].y = pos->y;
    v[0].z = 0.0f;
    v[1].x = v[0].x + 8.0f;
    v[1].y = pos->y - 4.0f;
    v[1].z = z;
    v[2].x = v[1].x;
    v[2].y = pos->y + 4.0f;
    v[2].z = z;
    TprimDrawPolyFn(v, col, 3);
}

#ifdef TPRIM_FULL
// The 10.0f word between TprimDrawCursor's and TprimDrawHtr's constant pools: a public const object
// (emitted at its definition; nothing references it).
extern const f32 TprimHtrSize;
const f32 TprimHtrSize = 10.0f;

// Hit marker: a horizontal square around `pos`.
void TprimDrawHtr(Vec* pos, GXColor* col)
{
    Vec v[4];

    GXBegin(0x80, 0, 4);
    v[0].x = pos->x;
    v[0].y = pos->y;
    v[0].z = pos->z - 300.0f;
    v[1].x = pos->x - 300.0f;
    v[1].y = pos->y;
    v[1].z = pos->z;
    v[2].x = pos->x;
    v[2].y = pos->y;
    v[2].z = pos->z + 300.0f;
    v[3].x = pos->x + 300.0f;
    v[3].y = pos->y;
    v[3].z = pos->z;
    set_vtx_flat_f32(v, col, 4);
}

// Hit marker cone: four triangles from the apex 1200 above `pos`, the last three in a darker colour.
void TprimDrawHtrCone(Vec* pos, GXColor* col)
{
    Vec v[3];
    GXColor c;

    v[0].x = pos->x;
    v[0].y = pos->y + 1200.0f;
    v[0].z = pos->z;
    v[1].x = pos->x;
    v[1].y = pos->y;
    v[1].z = pos->z - 300.0f;
    v[2].x = pos->x + 300.0f;
    v[2].y = pos->y;
    v[2].z = pos->z;
    TprimDrawPolyFn(v, col, 3);

    v[0].x = pos->x;
    v[0].y = pos->y + 1200.0f;
    v[0].z = pos->z;
    v[1].x = pos->x + 300.0f;
    v[1].y = pos->y;
    v[1].z = pos->z;
    v[2].x = pos->x;
    v[2].y = pos->y;
    v[2].z = pos->z + 300.0f;
    c.r = col->r >> 1;
    c.g = col->g >> 1;
    c.b = col->b >> 1;
    c.a = col->a;
    TprimDrawPolyFn(v, &c, 3);

    v[0].x = pos->x;
    v[0].y = pos->y + 1200.0f;
    v[0].z = pos->z;
    v[1].x = pos->x;
    v[1].y = pos->y;
    v[1].z = pos->z + 300.0f;
    v[2].x = pos->x - 300.0f;
    v[2].y = pos->y;
    v[2].z = pos->z;
    c.r = col->r >> 1;
    c.g = col->g >> 1;
    c.b = col->b >> 1;
    c.a = col->a;
    TprimDrawPolyFn(v, &c, 3);

    v[0].x = pos->x;
    v[0].y = pos->y + 1200.0f;
    v[0].z = pos->z;
    v[1].x = pos->x - 300.0f;
    v[1].y = pos->y;
    v[1].z = pos->z;
    v[2].x = pos->x;
    v[2].y = pos->y;
    v[2].z = pos->z - 300.0f;
    c.r = col->r >> 1;
    c.g = col->g >> 1;
    c.b = col->b >> 1;
    c.a = col->a;
    TprimDrawPolyFn(v, &c, 3);
}
#endif

// Never called. GCC 2.95 emits the initializer templates of local aggregates in inline functions at
// parse time, and the original object carries these 9 words between TprimDrawCursor's constant pool
// and TprimDrawMtxDirection's template. The values are the original's; the grouping and the body are a
// guess that reproduces them.
// Likewise the 0x20 bytes of .bss behind ToolBuffer (unreferenced, so the DOL link dropped them).
#ifndef TPRIM_FULL
static TprimView default_view;
static f32 default_clip[2];

static inline void tprim_default_view(Vec* axis)
{
    TprimView v = {{10.0f, 300.0f, 1200.0f, 300.0f}, 1.0f, 0.0f};
    Vec x = {1.0f, 0.0f, 0.0f};

    default_view = v;
    default_clip[0] = v.nearz;
    default_clip[1] = v.farz;
    *axis = x;
}
#else
// The full build has TprimDrawHtr/HtrCone (the 10/300 and 1200/300 words are their pools); only the
// {1, 0} and {1, 0, 0} templates remain unexplained.
static f32 default_clip[2];

static inline void tprim_default_view(Vec* axis)
{
    f32 clip[2] = {1.0f, 0.0f};
    Vec x = {1.0f, 0.0f, 0.0f};

    default_clip[0] = clip[0];
    default_clip[1] = clip[1];
    *axis = x;
}
#endif

// Arrow head along the matrix' z axis: a filled triangle and its outline. Nothing in the Tools REL calls
// it and only its template survives there (an unused inline).
#ifdef TPRIM_FULL
inline
#endif
void TprimDrawMtxDirection(Mtx m, GXColor* fill, GXColor* line)
{
    Vec v[3] = {{0.0f, 0.0f, 900.0f}, {300.0f, 0.0f, -300.0f}, {-300.0f, 0.0f, -300.0f}};

    PSMTXMultVec(m, &v[0], &v[0]);
    PSMTXMultVec(m, &v[1], &v[1]);
    PSMTXMultVec(m, &v[2], &v[2]);
    GXBegin(0x80, 0, 3);
    GXPosition3f32(v[0].x, v[0].y, v[0].z);
    GXColor4u8(fill->r, fill->g, fill->b, fill->a);
    GXPosition3f32(v[1].x, v[1].y, v[1].z);
    GXColor4u8(fill->r, fill->g, fill->b, fill->a);
    GXPosition3f32(v[2].x, v[2].y, v[2].z);
    GXColor4u8(fill->r, fill->g, fill->b, fill->a);
    GXBegin(0xB0, 0, 4);
    GXPosition3f32(v[0].x, v[0].y, v[0].z);
    GXColor4u8(line->r, line->g, line->b, line->a);
    GXPosition3f32(v[1].x, v[1].y, v[1].z);
    GXColor4u8(line->r, line->g, line->b, line->a);
    GXPosition3f32(v[2].x, v[2].y, v[2].z);
    GXColor4u8(line->r, line->g, line->b, line->a);
    GXPosition3f32(v[0].x, v[0].y, v[0].z);
    GXColor4u8(line->r, line->g, line->b, line->a);
}

#ifdef TPRIM_FULL
// Closed outline: the strip plus the first vertex again.
void TprimDrawFrameFn_s16(S16Vec* v, GXColor* col, u16 n)
{
    set_attr_s16();
    GXBegin(0xB0, 0, n + 1);
    set_vtx_flat_s16(v, col, n);
    GXPosition3s16(v[0].x, v[0].y, v[0].z);
    GXColor4u8(col->r, col->g, col->b, col->a);
    set_attr_f32();
}

void TprimDrawPolyFn_s16(S16Vec* v, GXColor* col, u16 n)
{
    set_attr_s16();
    GXBegin(0x80, 0, n);
    set_vtx_flat_s16(v, col, n);
    set_attr_f32();
}
#endif

static void set_vtx_flat_f32(Vec* v, GXColor* col, u16 n)
{
    u16 i = 0;

    do {
        GXPosition3f32(v[i].x, v[i].y, v[i].z);
        GXColor4u8(col->r, col->g, col->b, col->a);
    } while (++i < n);
}

#ifdef TPRIM_FULL
static void set_vtx_flat_s16(S16Vec* v, GXColor* col, u16 n)
{
    u16 i = 0;

    do {
        GXPosition3s16(v[i].x, v[i].y, v[i].z);
        GXColor4u8(col->r, col->g, col->b, col->a);
    } while (++i < n);
}

// the next unit's .data is 8-aligned
asm(".section .data; .balign 8");
#endif
