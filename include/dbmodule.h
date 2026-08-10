#ifndef DBMODULE_H
#define DBMODULE_H

#include "types.h"
#include "vec.h"
#include "gx.h"

class cObj;
struct JOY;

// game/dbmodule.cpp: debug primitive drawing (C linkage).
extern "C" {
void init_dbmodule();
void Render_tile(void* data);
void Draw_tile(s16 x, s16 y, s16 w, s16 h, GXColor* color);
void Draw_line(Vec* a, Vec* b, u32 color);
void Draw_quad(Vec* pos, Vec* size, u32 color);
void Draw_line3d(Vec* a, Vec* b, u32 color, int blend);
void Draw_line3d_local(Vec* a, Vec* b, Mtx mtx, u32 color, int blend);
void Draw_line3d_init();
void Draw_line3d_end();
void Draw_poly(Vec* p, u32 color, int zupd);
void Draw_poly_local(Vec* p, Mtx mtx, u32 color, int zupd);
void Draw_sphere(Vec* pos, f32 r, u32 color, int zcmp, int zupd);
void Draw_cylinder(Vec* pos, f32 r, f32 h, u32 color);
void Draw_cylinderMtx(Mtx mtx, Vec* pos, f32 r, f32 h, u32 color);
void Draw_corn3(Vec* pos, Vec* dir, f32 r, u32 color);
void Draw_corn(Vec* pos, Vec* rot, f32 len, f32 r, u32 color);
void Draw_corn2(Vec* pos, Vec* dir, f32 len, f32 ang, u32 color);
void Draw_box(Vec* v, u32 color, int flag);
void Draw_pos(Vec* pos, int size);
void Draw_local_pos(Vec* pos, int size, Mtx mtx);
void Draw_floor(int step, int n, u32 color);
void init_sphere();
void init_circle();
void init_cylinder();
void init_corn();
void DrawObjWireframe(cObj* obj, int color);
void DrawRoomWireframe();
void DispTime(s16 x, int y, int color, int time, int flag);
}

JOY* GetBugCheckController();

#endif
