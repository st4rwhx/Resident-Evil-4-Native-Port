#ifndef AT_SUB_H
#define AT_SUB_H

#include "types.h"
#include "vec.h"

// Collision polygon data block (game/at_sub.cpp, game/atari.cpp): vertex, face normal and
// edge normal tables the polygons index into.
struct AtPolyData {
    u8 pad_0[0xC];
    Vec* vtx;        // 0x0C
    Vec* nrm;        // 0x10  face normals
    Vec* edge;       // 0x14  edge normals
};

// One collision triangle (0x14 bytes): three vertex, one normal and three edge indices, attribute.
struct AtPoly {
    u16 v[3];        // 0x00
    u16 n;           // 0x06
    u16 e[3];        // 0x08
    u8 pad_E[2];
    union {
        struct {
            u16 attrHi;  // 0x10  attribute word high half
            u16 attrLo;  // 0x12
        };
        u32 attr;        // 0x10  the attribute word (atari createSat)
    };
};

extern "C" {
extern int SEck;   // game/atari.cpp: scenario-effect check mode (skips the attribute filters)

// Signed distance of `p` from the plane through `a` with normal `n`.
f32 At_surface_point_rel(Vec* a, Vec* n, Vec* p);
// Segment p0-p1 against that plane; the crossing point goes to `out` (p1 when it does not cross).
int At_surface_line_ck(Vec* out, Vec* a, Vec* n, Vec* p0, Vec* p1);
// Point `p` (on the plane) inside the triangle `poly` with normal `nrm`.
int At_poly_point_rel(Vec* poly, Vec* nrm, Vec* p);
// Sphere against a box given as 8 vertices.
int At_box_sphere_ck(Vec* box, Vec* p, f32 r);
// Normal of a triangle.
void Get_normal(Vec* tri, Vec* out);
u32 AtBoxCapsuleCk3(Vec* box, Vec* p0, f32 r, Vec* p1);
u32 AtSphereCapsuleCk(Vec* c, Vec* p0, f32 r, f32 r2, Vec* p1);
void AtCapsuleDisp(Vec* p0, Vec* p1, f32 r, u32 color);
void AtCubeDisp(Mtx m, f32 sx, f32 sy, f32 sz, Vec* pos, u32 color);
// Segment p0-p1 against one polygon; returns the attribute | 0x01000000 or 0.
u32 At_poly_line_ck(AtPolyData* pd, Vec* out, AtPoly* poly, Vec* p0, Vec* p1, u32 flag, u32 mask);
// Sphere moving from `oldPos` to `pos` against one polygon; `pos` is pushed out. Returns the hit
// kind (1 crossed the plane, 2 touching) or 0.
u32 At_poly_sphere_ck(AtPolyData* pd, AtPoly* poly, Vec* oldPos, Vec* pos, f32 r, u32 flag, u32 mask);
u32 At_poly_sphere_ck2(Vec* tri, Vec* n, u32 attr, Vec* oldPos, Vec* pos, f32 r, u32 flag, u32 mask);
u32 Get_poly_attr(AtPoly* poly);
// XZ rectangles of 4 corners.
int At_rect_point_ck(Vec* rect, Vec* p);
int At_rect_rect_ck(Vec* ra, Vec* rb);
// Quadrant of an angle: 0 front (|a| <= pi/4), 1 right, 2 back, 3 left.
int Get_ang_dir(f32 ang);
// out = a * t + b * (1 - t)
void InterVectorXYZ(Vec* out, Vec* a, Vec* b, f32 t);
int EatGetEffectType(u32 attr);
}

#endif
