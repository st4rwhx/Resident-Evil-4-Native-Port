#include "cLightMgr.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 801502b8-801502bf, 8 bytes / 2 instructions: addi r3,r3,0x34; blr
// Pure address computation (no load) - same shape as
// CameraControl::getMotionInfoPtr, which matched byte-identical.
void* cLightMgr::getLitPPtr()
{
    return (u8*)this + 0x34;
}
