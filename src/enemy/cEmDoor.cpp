#include "cEmDoor.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 801048dc-801048e7, 12 bytes / 3 instructions: li r0,1; stb r0,0x81e(r3); blr
void cEmDoor::setSeCancel()
{
    mSeCancel = 1;
}
