#include "cObjGatling.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8002edc4-8002edcf, 12 bytes / 3 instructions: li r0,1; stb r0,0x334(r3); blr
void cObjGatling::setFire()
{
    mFire = 1;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8002edbc-8002edc3, 8 bytes / 2 instructions: stw r4,0x364(r3); blr
void cObjGatling::setRide(u32 ride)
{
    mRide = ride;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8002edd0-8002eddb, 12 bytes / 3 instructions: li r0,0; stb r0,0x335(r3); blr
void cObjGatling::stopFire()
{
    mFireStop = 0;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8002edec-8002edf7, 12 bytes / 3 instructions: li r0,0x28; stb r0,0x336(r3); blr
void cObjGatling::setReload()
{
    mReload = 0x28;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8002ef50-8002ef57, 8 bytes / 2 instructions: stfs f1,0x33c(r3); blr
void cObjGatling::setMaxRot(f32 rot)
{
    mMaxRot = rot;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8002ef70-8002ef77, 8 bytes / 2 instructions: stb r4,0x337(r3); blr
void cObjGatling::setBreakMode(u8 mode)
{
    mBreakMode = mode;
}
