#include "cCard.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8000d8ac-8000d8b7, 12 bytes / 3 instructions: li r0,4; stb r0,0x4(r3); blr
void cCard::firstCheck30()
{
    mCheck30 = 4;
}
