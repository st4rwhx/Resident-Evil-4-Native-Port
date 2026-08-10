#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "types.h"
#include "vec.h"

// game/geometry.cpp: collision primitives (C linkage).

// Cone (or fan/cylinder) volume: apex at pos, axis dir, half angle, height; radius is derived.
struct GeoCone {
    Vec pos;     // 0x00
    Vec dir;     // 0x0C
    f32 angle;   // 0x18
    f32 height;  // 0x1C
    f32 radius;  // 0x20  height * sin(angle), written by collision_point_cone_rev_play
};

// 0x18 bytes: the trans_ot AddOt* frames reserve 0x18 for the local sphere (only pos/r are touched).
struct GeoSphere {
    Vec pos;         // 0x00
    f32 r;           // 0x0C
    u8 pad_10[0x8];  // 0x10
};

// Six planes: outward normals, plus one corner point for planes 0-2 and another for 3-5.
struct GeoHexahedron {
    Vec normal[6];  // 0x00
    Vec pointA;     // 0x48
    u8 pad_54[0x90 - 0x54];
    Vec pointB;     // 0x90
};

extern "C" {
int collision_point_cone_rev_play(Vec* p, GeoCone* cone, f32 margin);
int collision_point_cone_rev_play_face(Vec* p, GeoCone* cone, Vec* face, f32 margin, f32 angle);
int collision_sphere_hexahedron(GeoSphere* s, GeoHexahedron* h);
}

#endif
