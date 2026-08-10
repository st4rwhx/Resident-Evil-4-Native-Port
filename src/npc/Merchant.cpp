#include "Merchant.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80026858-8002685f, 8 bytes / 2 instructions: lbz r3,0x315(r3); blr
u8 Merchant::sellingItemNum()
{
    return mSellingItemNum;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80026a24-80026a2b, 8 bytes / 2 instructions: lbz r3,0x314(r3); blr
u8 Merchant::exerciseItemNum()
{
    return mExerciseItemNum;
}
