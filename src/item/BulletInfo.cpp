#include "BulletInfo.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800d8d84-800d8d8f, 12 bytes / 3 instructions: li r0,-1; stw r0,0x10(r3); blr
void BulletInfo::roomInit()
{
    mRoomId = -1;
}
