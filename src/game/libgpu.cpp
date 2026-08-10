// game/libgpu: PlayStation ordering-table primitives drawn through GX (D:/Bio4/Prog/libgpu.cpp).
#include "types.h"
#include "vec.h"
#include "gx.h"
#include "libgpu.h"

// The primitive is handed over as the OT tag word and cast at every access: a pointer kept in an
// integer variable has no REG_POINTER flag, so its loads may alias the stack stores of the make_f*
// conversions and stay in source order (a POLY_F3* parameter hoists all loads above the stores).
extern "C" {
void make_g3(u32 tag);
void make_g4(u32 tag);
void make_f3(u32 tag);
void make_f4(u32 tag);
void make_tile(u32 tag);
void make_lg2(u32 tag);
void make_lg3(u32 tag);
void make_lg4(u32 tag);
void make_lf2(u32 tag);
void make_lf3(u32 tag);
void make_lf4(u32 tag);
}

void AddPrim(u32* ot, u32* prim)
{
    *prim = *ot;
    *ot = (u32) prim;
}

void DelPrim(u32* ot, u32* prim)
{
    if (*ot == 0xFFFFFFFF) {
        return;
    }
    do {
        u32* p = (u32*) *ot;
        if ((s32) *ot < 0 && p == prim) {
            *ot = *p;
            return;
        }
        ot = (u32*) (*ot | 0x80000000);
    } while (*ot != 0xFFFFFFFF);
}

void ClearOTagR(u32* ot, int n)
{
    int i;

    *ot = 0xFFFFFFFF;
    for (i = 0; i < n - 1; i++) {
        ot[1] = (u32) ot & 0x7FFFFFFF;
        ot++;
    }
}

void DrawOTag(u32* ot)
{
    static void (*tbl[])(u32) = {
        make_g3,
        make_g4,
        make_f3,
        make_f4,
        make_tile,
        make_lg2,
        make_lg3,
        make_lg4,
        make_lf2,
        make_lf3,
        make_lf4,
    };
    Mtx44 proj;
    Mtx mv;

    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mv);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    GXSetBlendMode(0, 1, 0, 0);
    GXSetCullMode(0);
    if (*ot == 0xFFFFFFFF) {
        return;
    }
    do {
        u32* p = (u32*) *ot;
        if ((s32) p < 0) {
            tbl[p[1] & 0x1F]((u32) p);
        }
        ot = (u32*) (*ot | 0x80000000);
    } while (*ot != 0xFFFFFFFF);
}

static inline void gpuSetup(int prim, int nverts)
{
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    GXSetBlendMode(1, 1, 0, 0);
    GXSetCullMode(0);
    GXSetZMode(0, 3, 0);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
    GXBegin(prim, 0, nverts);
}

void make_g3(u32 tag)
{
#define p ((POLY_G3*) tag)
    gpuSetup(0x90, 3);
    GXPosition3s16(p->x0, p->y0, p->z0);
    GXColor4u8(p->c0.r, p->c0.g, p->c0.b, 0xFF);
    GXPosition3s16(p->x1, p->y1, p->z1);
    GXColor4u8(p->c1.r, p->c1.g, p->c1.b, 0xFF);
    GXPosition3s16(p->x2, p->y2, p->z2);
    GXColor4u8(p->c2.r, p->c2.g, p->c2.b, 0xFF);
}
#undef p

void make_g4(u32 tag)
{
#define p ((POLY_G4*) tag)
    gpuSetup(0x98, 4);
    GXPosition3s16(p->x0, p->y0, p->z0);
    GXColor4u8(p->c0.r, p->c0.g, p->c0.b, 0xFF);
    GXPosition3s16(p->x1, p->y1, p->z1);
    GXColor4u8(p->c1.r, p->c1.g, p->c1.b, 0xFF);
    GXPosition3s16(p->x2, p->y2, p->z2);
    GXColor4u8(p->c2.r, p->c2.g, p->c2.b, 0xFF);
    GXPosition3s16(p->x3, p->y3, p->z3);
    GXColor4u8(p->c3.r, p->c3.g, p->c3.b, 0xFF);
}
#undef p

void make_f3(u32 tag)
{
#define p ((POLY_F3*) tag)
    POLY_G3 g;

    g.x0 = p->x0;
    g.y0 = p->y0;
    g.z0 = p->z0;
    g.x1 = p->x1;
    g.y1 = p->y1;
    g.z1 = p->z1;
    g.x2 = p->x2;
    g.y2 = p->y2;
    g.z2 = p->z2;
    g.c0.r = g.c1.r = g.c2.r = p->c0.r;
    g.c0.g = g.c1.g = g.c2.g = p->c0.g;
    g.c0.b = g.c1.b = g.c2.b = p->c0.b;
    g.c0.cd = g.c1.cd = g.c2.cd = p->c0.cd;
    make_g3((u32) &g);
}
#undef p

void make_f4(u32 tag)
{
#define p ((POLY_F4*) tag)
    POLY_G4 g;

    g.x0 = p->x0;
    g.y0 = p->y0;
    g.z0 = p->z0;
    g.x1 = p->x1;
    g.y1 = p->y1;
    g.z1 = p->z1;
    g.x2 = p->x2;
    g.y2 = p->y2;
    g.z2 = p->z2;
    g.x3 = p->x3;
    g.y3 = p->y3;
    g.z3 = p->z3;
    g.c0.r = g.c1.r = g.c2.r = g.c3.r = p->c0.r;
    g.c0.g = g.c1.g = g.c2.g = g.c3.g = p->c0.g;
    g.c0.b = g.c1.b = g.c2.b = g.c3.b = p->c0.b;
    g.c0.cd = g.c1.cd = g.c2.cd = g.c3.cd = p->c0.cd;
    make_g4((u32) &g);
}
#undef p

void make_tile(u32 tag)
{
#define p ((TILE*) tag)
    POLY_G4 g;

    g.x0 = g.x2 = p->x0;
    g.x1 = g.x3 = p->x0 + p->w;
    g.y0 = g.y1 = p->y0;
    g.y2 = g.y3 = p->y0 + p->h;
    g.z0 = g.z1 = g.z2 = g.z3 = p->z0;
    g.c0.r = g.c1.r = g.c2.r = g.c3.r = p->c0.r;
    g.c0.g = g.c1.g = g.c2.g = g.c3.g = p->c0.g;
    g.c0.b = g.c1.b = g.c2.b = g.c3.b = p->c0.b;
    g.c0.cd = g.c1.cd = g.c2.cd = g.c3.cd = p->c0.cd;
    make_g4((u32) &g);
}
#undef p

void make_lg2(u32 tag)
{
#define p ((LINE_G2*) tag)
    gpuSetup(0xB0, 2);
    GXPosition3s16(p->x0, p->y0, p->z0);
    GXColor4u8(p->c0.r, p->c0.g, p->c0.b, 0xFF);
    GXPosition3s16(p->x1, p->y1, p->z1);
    GXColor4u8(p->c1.r, p->c1.g, p->c1.b, 0xFF);
}
#undef p

void make_lg3(u32 tag)
{
#define p ((LINE_G3*) tag)
    gpuSetup(0xB0, 3);
    GXPosition3s16(p->x0, p->y0, p->z0);
    GXColor4u8(p->c0.r, p->c0.g, p->c0.b, 0xFF);
    GXPosition3s16(p->x1, p->y1, p->z1);
    GXColor4u8(p->c1.r, p->c1.g, p->c1.b, 0xFF);
    GXPosition3s16(p->x2, p->y2, p->z2);
    GXColor4u8(p->c2.r, p->c2.g, p->c2.b, 0xFF);
}
#undef p

void make_lg4(u32 tag)
{
#define p ((LINE_G4*) tag)
    gpuSetup(0xB0, 4);
    GXPosition3s16(p->x0, p->y0, p->z0);
    GXColor4u8(p->c0.r, p->c0.g, p->c0.b, 0xFF);
    GXPosition3s16(p->x1, p->y1, p->z1);
    GXColor4u8(p->c1.r, p->c1.g, p->c1.b, 0xFF);
    GXPosition3s16(p->x2, p->y2, p->z2);
    GXColor4u8(p->c2.r, p->c2.g, p->c2.b, 0xFF);
    GXPosition3s16(p->x3, p->y3, p->z3);
    GXColor4u8(p->c3.r, p->c3.g, p->c3.b, 0xFF);
}
#undef p

void make_lf2(u32 tag)
{
#define p ((LINE_F2*) tag)
    LINE_G2 g;

    g.x0 = p->x0;
    g.y0 = p->y0;
    g.z0 = p->z0;
    g.x1 = p->x1;
    g.y1 = p->y1;
    g.z1 = p->z1;
    g.c0.r = g.c1.r = p->c0.r;
    g.c0.g = g.c1.g = p->c0.g;
    g.c0.b = g.c1.b = p->c0.b;
    g.c0.cd = g.c1.cd = p->c0.cd;
    make_lg2((u32) &g);
}
#undef p

void make_lf3(u32 tag)
{
#define p ((LINE_F3*) tag)
    LINE_G3 g;

    g.x0 = p->x0;
    g.y0 = p->y0;
    g.z0 = p->z0;
    g.x1 = p->x1;
    g.y1 = p->y1;
    g.z1 = p->z1;
    g.x2 = p->x2;
    g.y2 = p->y2;
    g.z2 = p->z2;
    g.c0.r = g.c1.r = g.c2.r = p->c0.r;
    g.c0.g = g.c1.g = g.c2.g = p->c0.g;
    g.c0.b = g.c1.b = g.c2.b = p->c0.b;
    g.c0.cd = g.c1.cd = g.c2.cd = p->c0.cd;
    make_lg3((u32) &g);
}
#undef p

void make_lf4(u32 tag)
{
#define p ((LINE_F4*) tag)
    LINE_G4 g;

    g.x0 = p->x0;
    g.y0 = p->y0;
    g.z0 = p->z0;
    g.x1 = p->x1;
    g.y1 = p->y1;
    g.z1 = p->z1;
    g.x2 = p->x2;
    g.y2 = p->y2;
    g.z2 = p->z2;
    g.x3 = p->x3;
    g.y3 = p->y3;
    g.z3 = p->z3;
    g.c0.r = g.c1.r = g.c2.r = g.c3.r = p->c0.r;
    g.c0.g = g.c1.g = g.c2.g = g.c3.g = p->c0.g;
    g.c0.b = g.c1.b = g.c2.b = g.c3.b = p->c0.b;
    g.c0.cd = g.c1.cd = g.c2.cd = g.c3.cd = p->c0.cd;
    make_lg4((u32) &g);
}
#undef p
