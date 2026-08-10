#ifndef ATARI_INIT_H
#define ATARI_INIT_H

#include "atariInfo.h"

// cAtariInfo::init(int parts, int flags, int cnt, f32 x, y, z, rx, rz, w, h) called with immediate
// ints: the original's RTL sets the seven FPR arguments before the three `li` GPR arguments
// (`mr r3,this; fmr f1; fmr f6; li r4; fmr f7; li r5; li r6` in SetTrolley/SetGondola/SetYagura/
// SetHeliMissile/SetPillar/SetBox/SetEmSwitch/setScrAtari, Espgen44_Destruct's Filter05SetParam,
// emrock/emBarred/embarrel). GCC 2.95 emits the argument moves in declaration order, so sched1/sched2
// (equal priority, equal register weight, LUID tie-break) always issue the int immediates first.
// The GPR and FPR argument registers are assigned independently by the SysV ABI, so declaring the
// same symbol with the float parameters first gives exactly that move order (see docs/matching.md).
void atariInitF(cAtariInfo* at, f32 x, f32 y, f32 z, f32 rx, f32 rz, f32 w, f32 h, int parts, int flags,
                int cnt) asm("init__10cAtariInfoiiifffffff");

// Float constants must reach the call as pseudos (cse then shares the 0.0 with the `pos = 0`
// stores that follow, and the 1000.0 load gets the longer dependence chain the original schedules
// first): pass them through an inline whose float parameters are the arguments.
static inline void AtariInit(cAtariInfo* at, f32 x, f32 y, f32 z, f32 rx, f32 rz, f32 w, f32 h, int parts,
                             int flags, int cnt)
{
    atariInitF(at, x, y, z, rx, rz, w, h, parts, flags, cnt);
}

#endif
