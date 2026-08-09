#include "CameraControl.h"

// --- Matching status: Equivalent (logic verified correct, not yet byte-exact - see
// docs/matching/CameraControl_AreaCheckOnOff.md for the full investigation) ---
// Hand-derived from the debug build's disassembly (CameraControl::AreaCheckOnOff,
// 80003920-80003957, 56 bytes / 14 instructions - see
// build/areacheckonoff_ref.txt for the ground-truth disassembly + raw bytes).
// Written by hand tracing the asm:
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

// --- Matching status: Equivalent (see docs/matching/CameraControl_Disable.md) ---
// 80003908-8000391f, 24 bytes / 6 instructions, straight-line (no branches).
// Instruction sequence and order match the target exactly (lwz, li, stb,
// ori, stw, blr) - only the specific register numbers chosen by the
// allocator differ (target: r0 for the loaded mFlags value, r9 for the
// temp 0; every compile attempted here: r4 for mFlags, r0 for the temp 0).
// Tried across all 20 available CodeWarrior versions and several source
// phrasings - register choice never changed. Same class of compiler-
// internal, source-unreachable behavior as AreaCheckOnOff, just one level
// closer (opcodes/order match exactly, only register numbering doesn't).
void CameraControl::Disable()
{
    u32 flags = mFlags;
    mUnk34 = 0;
    mFlags = flags | 0x8;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80003964-8000396f, 12 bytes / 3 instructions: lbz r3,0x690(r3); extsb r3,r3; blr
s32 CameraControl::CurrentAreaNo()
{
    return mCurrentAreaNo;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80003970-8000397b, 12 bytes / 3 instructions: lbz r3,0x692(r3); extsb r3,r3; blr
s32 CameraControl::CurrentCameraNo()
{
    return mCurrentCameraNo;
}
