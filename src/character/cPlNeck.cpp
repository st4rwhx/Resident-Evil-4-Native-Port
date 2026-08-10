#include "cPlNeck.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8017590c-80175913, 8 bytes / 2 instructions: stb r4,0xe(r3); blr
void cPlNeck::setMode(u8 mode)
{
    mMode = mode;
}
