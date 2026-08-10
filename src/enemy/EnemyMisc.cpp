#include "EnemyMisc.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80012f44-80012f4b, 8 bytes / 2 instructions: stb r4,0x500(r3); blr
void cEmBar::setEff(u8 eff)
{
    mEff = eff;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80012f4c-80012f53, 8 bytes / 2 instructions: stw r4,0x4fc(r3); blr
void cEmBar::setMotion(u32 motion)
{
    mMotion = motion;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800194a0-800194a7, 8 bytes / 2 instructions: stw r4,0x474(r3); blr
void cEmWep::setAtNo(u32 atNo)
{
    mAtNo = atNo;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800eddac-800eddb7, 12 bytes / 3 instructions: stb r5,0x1(r3); stb r4,0x0(r3); blr
void cDmgInfo::set_0(u8 a, u8 b)
{
    mField1 = b;
    mField0 = a;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800fb138-800fb13f, 8 bytes / 2 instructions: stb r4,0x46c(r3); blr
void cEmBarrel::setEff(u8 eff)
{
    mEff = eff;
}
