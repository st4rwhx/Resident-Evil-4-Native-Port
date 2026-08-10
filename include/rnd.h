#ifndef RND_H
#define RND_H

#include "types.h"

// game/rnd.cpp: 16-bit LCG shared by the whole game (C linkage).
void RndInit(u16 seed);  // C++ linkage (main.cpp systemStartInit)
extern "C" {
u8 Rnd();
f32 fRand0_1();
f32 fRand1_1();
f32 fRandSeed0_1(u32* seed);
f32 fRandSeed1_1(u32* seed);
}

#endif
