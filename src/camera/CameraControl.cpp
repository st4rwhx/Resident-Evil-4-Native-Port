#include "CameraControl.h"

// --- Matching status: UNVERIFIED ---
// Hand-derived from the debug build's disassembly (CameraControl::AreaCheckOnOff,
// 80003920-80003957, 56 bytes / 14 instructions - see
// build/areacheckonoff_ref.txt for the ground-truth disassembly + raw bytes).
// This has NOT been compiled with the real CodeWarrior GC compiler or
// diffed with objdiff yet - see docs/RESEARCH.md for why. Written by hand
// tracing the asm:
//
//   or. r4,r4,r4        -> test mode for zero (sets cr0)
//   beq body_zero
//   cmpwi r4,1
//   beq body_one
//   blr                 -> else: no-op
//   body_zero: mFlags |= 0x8; return;
//   body_one:  mFlags = (mFlags & ~0x8) | 0x10; return;
//              (rlwinm r0,r0,0,0x1d,0x1b clears exactly bit 0x8 - mb=29 >
//              me=27 wraps the "keep" mask around everything except bit 28,
//              i.e. clears only that one bit; equivalent to & ~0x8)
void CameraControl::AreaCheckOnOff(s32 mode)
{
    if (!mode) {
        mFlags |= 0x8;
        return;
    }
    if (mode == 1) {
        mFlags = (mFlags & ~0x8) | 0x10;
        return;
    }
}
