#include "cBlockUnit.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800cf59c-800cf5a7, 12 bytes / 3 instructions: stb r5,0xd(r3); stw r4,0x4(r3); blr
void cBlockUnit::setBlockCommand(u32 a, u8 b)
{
    mFieldD = b;
    mField4 = a;
}
