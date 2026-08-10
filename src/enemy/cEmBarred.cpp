#include "cEmBarred.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800f9940-800f9947, 8 bytes / 2 instructions: stb r4,0x62d(r3); blr
void cEmBarred::setEff(u8 eff)
{
    mEff = eff;
}
