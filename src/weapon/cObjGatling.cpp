#include "cObjGatling.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8002edc4-8002edcf, 12 bytes / 3 instructions: li r0,1; stb r0,0x334(r3); blr
void cObjGatling::setFire()
{
    mFire = 1;
}
