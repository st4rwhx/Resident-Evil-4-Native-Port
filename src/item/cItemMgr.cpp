#include "cItemMgr.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8001fe5c-8001fe63, 8 bytes / 2 instructions: li r3,0x1204; blr
// Returns a compile-time constant (0x1204 = 4612), not a field read - the
// save-data struct's fixed serialized size.
s32 cItemMgr::saveDataSize()
{
    return 0x1204;
}
