#ifndef QUAKE_H
#define QUAKE_H

#include "types.h"

// game/quake.cpp: camera shake requests. Up to 16 concurrent entries; the scheduler picks the
// strongest active one each frame and QuakeMain jitters pG->Cam by it.
struct QuakeEntry {
    u8 active;   // 0x00  bit 0
    u8 id;       // 0x01
    u16 delay;   // 0x02  frames before it starts
    s16 time;    // 0x04  frames left (0 = finished)
    f32 power;   // 0x08
    u8 axis;     // 0x0C  bit 0 = x, bit 1 = y, bit 2 = z
    u8 pad_D[3];
};

struct QuakeWork {
    s32 active;           // 0x000  something is shaking this frame
    f32 power;            // 0x004  strongest active power
    QuakeEntry ent[16];   // 0x008
    u8 rnd_idx;           // 0x108
    u8 pad_109[3];
    u32 axis;             // 0x10C  union of the axes of the entries at max power
};

extern QuakeWork Quake;

void QuakeInit();

extern "C" {
void QuakeMove();
void QuakeExec(u8 id, u16 delay, s16 time, f32 power, u8 axis);
void QuakeScheduler();
void QuakeMain();
}

#endif
