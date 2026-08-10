#ifndef FADE_H
#define FADE_H

#include "types.h"
#include "gx.h"

// game/fade.cpp: up to 4 full-screen colour fades (C linkage).
struct FadeWork {
    GXColor start;  // 0x00
    GXColor end;    // 0x04
    GXColor cur;    // 0x08
    u8 pad_C[8];
    f32 z;          // 0x14
    u16 flags;      // 0x18  bit 0 = fading, bit 1 = keep drawing when done, bit 2 = late group (FadeControl(1))
    u16 x1A;        // 0x1A
    u32 time;       // 0x1C  frames
    u32 count;      // 0x20
};

extern FadeWork Fade[4];

extern "C" {
void FadeSet(int no, GXColor* start, GXColor* end, u32 time, u32 z, int late);
void FadeKillAll();
void FadeKill(int no);
void FadeInit();
void FadeControl(int late);
void fadeDraw(FadeWork* f);
}

// Full-screen fade between black and clear (every game-side FadeSet call). The colour pair is a
// local of this inline: a class with a user copy constructor is BLKmode (cp/class.c finish_struct_1),
// so it lives in the inline's frame and every inlined copy shares one temp slot, and integrate.c maps the inline frame to a pseudo P with a constant
// equivalence that it substitutes into the hard-register argument sets (`addi r5, r1, off` per
// call, never PRE'd; at frame offset 0 the frame register itself, so everything is direct). A
// store of a *constant* through P is rejected by recog (no store-immediate on PPC) and keeps P
// (`stw rZ, 4(rP)`, `mr r4, rP`, PRE'd across blocks); `black` is stale for that substitution
// after the label of the first `if`, so its store goes through the substituted frame address and
// its `li` follows the zero's. See docs/matching.md "FadeSet colour pair".
struct FadeColorPair {
    GXColor start;
    GXColor end;
    FadeColorPair() {}
    FadeColorPair(const FadeColorPair&) {}
};

static inline void FadeSetW(int no, u32 time, u32 z, int late)
{
    FadeColorPair col;
    u32 black;

    if (no & 0x80000000) {
        *(u32*) &col.start = 0xFF;
    } else {
        *(u32*) &col.start = 0;
    }
    black = 0xFF;
    if (no & 0x80000000) {
        *(u32*) &col.end = 0;
    } else {
        *(u32*) &col.end = black;
    }
    FadeSet(no, &col.start, &col.end, time, z, late);
}

#endif
