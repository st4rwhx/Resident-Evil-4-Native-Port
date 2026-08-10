#ifndef LIGHT00_H
#define LIGHT00_H

#include "types.h"

// Layout unknown beyond the two offsets Light00_Move touches - plain
// struct (not a class with its own methods elsewhere seen yet), matching
// how it's called: a raw object pointer, not a "this".
struct Light00Obj {
    u8 unk_00[0x20];
    u32 field_0x20;
    u8 unk_24[0x13c - 0x24];
    u32 field_0x13c;
};

void Light00_Move(Light00Obj* obj);

#endif
