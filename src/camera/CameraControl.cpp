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

// --- Matching status: Equivalent ---
// 80003898-800038af, 24 bytes / 6 instructions:
//   lwz r0,0x30(r3); li r3,1; andi. r9,r0,0x2; bnelr; li r3,0; blr
// Tried: "(flags&2)!=0", "if(flags&2) return true; return false;", ternary
// "(flags&2)?true:false" - every form produces a DIFFERENT boolean-
// canonicalization sequence (extrwi bitfield-extract at -opt on/all;
// rlwinm+cmplwi+beq/li at -opt off; subic+subfe branchless-compare trick
// for the ternary) but never target's specific andi.-record-bit +
// fused-bnelr + prefetched-li idiom. Same category as AreaCheckOnOff -
// moving on rather than continuing to guess boolean-conversion phrasings.
bool CameraControl::IsChangeCamera()
{
    return (mFlags2 & 0x2) != 0;
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

// --- Matching status: Equivalent (see docs/matching/CameraControl_pointers.md) ---
// 80003958-80003963, 12 bytes / 3 instructions:
//   lwz r9,0x5c(r3); lbz r3,0x5(r9); blr
// Compiles to the same 3 instructions/order/opcodes but keeps the loaded
// pointer in r3 (reused) instead of moving it to r9 like the target does.
u8 CameraControl::AreaNum()
{
    return mAreaDataPtr->num;
}

// --- Matching status: Equivalent (see docs/matching/CameraControl_pointers.md) ---
// 80008fd0-80008fdb, 12 bytes / 3 instructions:
//   lwz r9,0x68c(r3); stw r4,0x1d0(r9); blr
// Same story as AreaNum: r3 reused here instead of moved to r9.
void CameraControl::setMotionBaseMatPtr(void* mat)
{
    mMotionBase->matPtr = mat;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80008fdc-80008fe7, 12 bytes / 3 instructions:
//   lwz r3,0x68c(r3); addi r3,r3,0x100; blr
void* CameraControl::getMotionInfoPtr()
{
    return mMotionBase->unk_100;
}
