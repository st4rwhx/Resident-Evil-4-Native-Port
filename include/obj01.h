#ifndef OBJ01_H
#define OBJ01_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// game/obj01.cpp: the thrown flame bottle / dynamite (the rooms throw them from Ganado positions).
cObj* SetObj01(void* bin, void* tpl, Vec* pos, Vec* rot, Vec* v, f32 a, f32 b, int c, int d);
void Obj01SetEst(cObj* obj, int a, int b, u32 c, int d, int e, int f, int g, int h, int i);

#endif
