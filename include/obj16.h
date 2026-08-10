#ifndef OBJ16_H
#define OBJ16_H

#include "types.h"
#include "vec.h"
#include "em10.h"

// game/obj16.cpp: the enemy head object (cObj16 itself is declared in em10.h).
extern "C" {
cObj* SetObj16(void* bin, void* tpl, cModel* target, cModel* body, int partsNo, u8 type, Vec* pos, Vec* rot);
void MotSetObj16(cObj* obj, void* mot, int a, int b);
}

#endif
