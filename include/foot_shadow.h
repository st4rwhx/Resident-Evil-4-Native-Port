#ifndef FOOT_SHADOW_H
#define FOOT_SHADOW_H

#include "types.h"
#include "vec.h"
#include "gx.h"

// One foot shadow entry of a character (game/foot_shadow_tbl.cpp), 8 bytes.
struct FootShadowDat {
    u8 parts;   // 0x00  parts the shadow is drawn under
    u8 div;     // 0x01  shadows interpolated between this and the previous linked entry
    u8 flag;    // 0x02  bit0: link the next entry to this one
    u8 alpha;   // 0x03
    f32 size;   // 0x04
};

// Foot shadow table (cEm::pFootShadowTbl): entry count and the entries.
struct FootShadowTbl {
    u32 num;
    FootShadowDat* dat;
};

extern "C" {
// game/foot_shadow.cpp
void DrawFootShadow(class cEm* em);
void drawShadowParts(GXTexObj* tex, Vec* pos, f32 size, f32 alpha);
}

#endif
