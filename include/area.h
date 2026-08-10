#ifndef AREA_H
#define AREA_H

#include "types.h"
#include "vec.h"

// Trigger volumes (game/area.cpp): AreaData is a 4-byte header followed by a 0x2C-byte body whose
// layout depends on the type (0x30 bytes total, e.g. flr_at.h `area[0x30]`).
#define AREA_TYPE_XZ4      1  // vertical prism over a quadrilateral in the XZ plane
#define AREA_TYPE_CYLINDER 2
#define AREA_TYPE_EYE      3  // view cone trigger

struct AreaXZ {
    f32 x;
    f32 z;
};

struct AreaXZ4 {
    f32 y;         // 0x00  floor height
    f32 h;         // 0x04  height
    f32 r;         // 0x08  (editor) point marker radius
    AreaXZ p[4];   // 0x0C
};

struct AreaCylinder {
    f32 y;         // 0x00
    f32 h;         // 0x04
    f32 r;         // 0x08  radius
    f32 x;         // 0x0C
    f32 z;         // 0x10
    f32 x14;
    f32 x18;
    f32 x1C;
    f32 x20;
    f32 x24;
    f32 x28;
};

struct AreaEyeTrigger {
    f32 y;         // 0x00
    f32 h;         // 0x04
    f32 r;         // 0x08  cone length (margin)
    f32 x;         // 0x0C
    f32 z;         // 0x10
    f32 ang_x;     // 0x14  view direction (rotation about X)
    f32 ang_y;     // 0x18  view direction (rotation about Y)
    f32 x1C;
    f32 open;      // 0x20  opening angle in radians (0 = all round)
    f32 x24;
    f32 x28;
};

union AreaBody {
    AreaXZ4 xz4;
    AreaCylinder cyl;
    AreaEyeTrigger eye;
};

struct AreaData {
    u8 flag;       // 0x00  1 = in use
    u8 type;       // 0x01  AREA_TYPE_*
    u16 x2;        // 0x02
    AreaBody u;    // 0x04
};

struct GeoCone;

int AreaHitCheck(void* area, Vec* pos);

extern "C" {
int areaHitCheck_xz4(AreaXZ4* a, Vec* pos);
int areaHitCheck_Cylinder(AreaCylinder* a, Vec* pos);
int AreaViewCheck(AreaData* area, GeoCone* cone);
void AreaGetCenterPos(Vec* out, AreaData* area);
void AreaGetInsidePos(Vec* out, AreaData* area);
void AreaDataInit(AreaData* area, Vec* pos, u8 type, f32 size, f32 height);
void area_Draw_sphere(Vec pos, f32 r, u32 color, Mtx mtx);
void area_Draw_line(Vec a, Vec b, u32 color, Mtx mtx);
void AreaDataEdit(AreaData* area, u32 color, int flag, Mtx mtx, f32 rate);
void area_xz4_Edit(AreaXZ4* a, u32 color, int flag, Mtx mtx, u32 mode, Vec vx, Vec vy, f32 dx, f32 dy, f32 rate);
void area_cylinder_Edit(AreaCylinder* a, u32 color, int flag, Mtx mtx, u32 mode, Vec vx, Vec vy, f32 dx, f32 dy, f32 rate);
void area_eye_trigger_Edit(AreaEyeTrigger* a, u32 color, int flag, Mtx mtx, u32 mode, Vec vx, Vec vy, f32 dx, f32 dy, f32 rate);
void AreaDataDisp(AreaData* area, u32 color, int flag, Mtx mtx);
void area_xz4_Disp(AreaXZ4* a, u32 color, int flag, Mtx mtx);
void area_cylinder_Disp(AreaCylinder* a, u32 color, int flag, Mtx mtx);
void area_eye_trigger_Disp(AreaEyeTrigger* a, u32 color, int flag, Mtx mtx);
void AreaDataInfoDisp(AreaData* area, int x, s16 y);
void AreaDataHelpDisp(AreaData* area, int x, s16 y);
}

#endif
