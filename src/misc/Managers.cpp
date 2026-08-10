#include "Managers.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80021764-8002176b, 8 bytes / 2 instructions: stw r4,0x24(r3); blr
void cManager_cMap_::setName(u32 name)
{
    mName = name;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800e70c0-800e70c7, 8 bytes / 2 instructions: stw r4,0x24(r3); blr
void cManager_cDmg_::setName(u32 name)
{
    mName = name;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8013b764-8013b76b, 8 bytes / 2 instructions: stw r4,0x24(r3); blr
void cManager_Event_::setName(u32 name)
{
    mName = name;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8015c3c8-8015c3cf, 8 bytes / 2 instructions: stw r4,0x24(r3); blr
void cManager_cModelInfo_::setName(u32 name)
{
    mName = name;
}
