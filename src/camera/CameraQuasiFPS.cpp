#include "CameraQuasiFPS.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800d3404-800d340b, 8 bytes / 2 instructions: stw r4,0x158(r3); blr
void CameraQuasiFPS::LRinfo(u32 info)
{
    mLRInfo = info;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800d3ab4-800d3abb, 8 bytes / 2 instructions: stfs f1,0x1e4(r3); blr
void CameraQuasiFPS::setBlendRatio(f32 ratio)
{
    mBlendRatio = ratio;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800d3ad4-800d3adb, 8 bytes / 2 instructions: lfs f1,0x208(r3); blr
f32 CameraQuasiFPS::getFloorRatio()
{
    return mFloorRatio;
}
