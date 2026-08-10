#include "cObjLadder.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8002ae28-8002ae2f, 8 bytes / 2 instructions: lwz r3,0x32c(r3); blr
u32 cObjLadder::getStatus()
{
    return mStatus;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8002ae30-8002ae37, 8 bytes / 2 instructions: lbz r3,0x101(r3); blr
u8 cObjLadder::getType()
{
    return mType;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8002ae78-8002ae83, 12 bytes / 3 instructions: li r0,0x5a; stw r0,0x34c(r3); blr
void cObjLadder::setClimb()
{
    mClimb = 0x5a;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8002ae84-8002ae8b, 8 bytes / 2 instructions: lwz r3,0x334(r3); blr
u32 cObjLadder::getLadderNum()
{
    return mLadderNum;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8002ae8c-8002ae97, 12 bytes / 3 instructions:
//   stb r5,0x101(r3); stw r4,0x334(r3); blr
void cObjLadder::setLadderInfo(u32 num, u8 type)
{
    mType = type;
    mLadderNum = num;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8002b2a0-8002b2ab, 12 bytes / 3 instructions: li r0,0x3c; stw r0,0x350(r3); blr
void cObjLadder::setResetReserve()
{
    mResetReserve = 0x3c;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8002d4a0-8002d4a7, 8 bytes / 2 instructions: stw r4,0x35c(r3); blr
void cObjLadder::setCamera(u32 camera)
{
    mCamera = camera;
}
