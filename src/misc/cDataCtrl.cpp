#include "cDataCtrl.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800dbde0-800dbde7, 8 bytes / 2 instructions: stw r4,0xa04(r3); blr
void cDataCtrl::setAramSort(u32 sort)
{
    mAramSort = sort;
}
