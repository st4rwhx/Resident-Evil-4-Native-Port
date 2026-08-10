#ifndef OBJ00_H
#define OBJ00_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Plain scroll-less object (game/obj00.cpp): a model with a motion, hung on a parent model's parts
// with OyaSetObj00 (room scripts: r11d's sister object).
cObj* SetObj00(void* bin, void* tpl, Vec* pos, Vec* rot);
void MotSetObj00(cObj* obj, void* mot, int prm, int a);
void OyaSetObj00(cObj* obj, cModel* oya, int partsNo);

#endif
