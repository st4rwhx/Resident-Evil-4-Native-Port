#include "ObjectMisc.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80031164-8003116f, 12 bytes / 3 instructions: li r0,0x3; stw r0,0x354(r3); blr
void cObj16::setDieEff()
{
    mDieEff = 3;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80038980-80038987, 8 bytes / 2 instructions: lwz r3,0x374(r3); blr
u32 cObjBull::getMoveFrameToLift()
{
    return mMoveFrame;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80038988-8003898f, 8 bytes / 2 instructions: lwz r3,0x374(r3); blr
// Same field as getMoveFrameToLift - two logical names for one value.
u32 cObjBull::getMoveFrameRtn()
{
    return mMoveFrame;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800389a0-800389ab, 12 bytes / 3 instructions: li r0,1; stb r0,0x38b(r3); blr
void cObjBull::setBreakTruck()
{
    mBreakTruck = 1;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8003bea4-8003beab, 8 bytes / 2 instructions: stw r4,0x334(r3); blr
void cObjPillar::setMotion(u32 motion)
{
    mMotion = motion;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80045250-80045257, 8 bytes / 2 instructions: stw r4,0x348(r3); blr
void cObjYagura::setMotionVib(u32 vib)
{
    mMotionVib = vib;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800ce9fc-800cea07, 12 bytes / 3 instructions:
//   addi r0,r3,0x1c; stw r0,0x20(r3); blr
void cBlock::setOtStart()
{
    mOtStart = &unk_1c;
}
