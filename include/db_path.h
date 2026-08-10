#ifndef DB_PATH_H
#define DB_PATH_H

#include "types.h"
#include "vec.h"
#include "path.h"

// B-spline path editor of the interface-design tool (t_id/db_path.cpp, D:/Bio4/Prog/db_path.cpp): edits a
// FuncPathData in place with a 3D cursor. t_id.cpp allocates the work and calls DbPath() every frame.
struct DbPathWork {
    s8 routine;     // 0x00  0 edit, 1 menu, 2 quit
    s8 step;        // 0x01
    s8 x2;
    s8 x3;
    s8 cursor;      // 0x04  menu row
    u8 pad_5[3];
    int x;          // 0x08  menu position
    int y;          // 0x0C
    FuncPathData* path;  // 0x10
    Vec* pEnd;      // 0x14  &path->pos[path->n]
    s8 grab;        // 0x18  grabbed control point, -1 = none
    s8 insertIdx;   // 0x19  insertion index found on the curve, -1 = none
    u8 yes;         // 0x1A  YES/NO of the delete / insert prompt
    u8 blink;       // 0x1B  frame counter (bits 3/4 blink the menu cursor)
    Vec insertPos;  // 0x1C
    Vec pos;        // 0x28  3D cursor
    u8 gridLock;    // 0x34
    u8 pad_35[3];
    Vec grid;       // 0x38  grid step (x, y)
    Vec ofs;        // 0x44  drawing offset
};

int DbPath(DbPathWork* w, int x, int y);
void pathInsertPoint(DbPathWork* w);
void pathCursor(DbPathWork* w);
void pathDraw(DbPathWork* w, Vec* ofs);
void pathGridLock(Vec* grid, Vec* in, Vec* out);

#endif
