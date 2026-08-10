#include "cEmObj.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 801b4fb4-801b4fbb, 8 bytes / 2 instructions: stb r4,0x63c(r3); blr
void cEmObj::setEff(u8 eff)
{
    mEff = eff;
}
