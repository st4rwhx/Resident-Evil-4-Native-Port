#include "EnemyMisc2.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80105378-8010537f, 8 bytes / 2 instructions: lwz r3,0x3ec(r3); blr
u32 cEmHit::ckStatus()
{
    return mStatus;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 801062c4-801062cb, 8 bytes / 2 instructions: stb r4,0x641(r3); blr
void cEmItem::setEff(u8 eff)
{
    mEff = eff;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 801062cc-801062d3, 8 bytes / 2 instructions: lwz r3,0x3ec(r3); blr
u32 cEmItem::ckStatus()
{
    return mStatus;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80106310-80106317, 8 bytes / 2 instructions: stb r4,0x640(r3); blr
void cEmItem::setRotType(u8 type)
{
    mRotType = type;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8010a610-8010a617, 8 bytes / 2 instructions: stb r4,0x4dc(r3); blr
void cEmRack::setEff(u8 eff)
{
    mEff = eff;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80112144-8011214b, 8 bytes / 2 instructions: lwz r3,0x5f0(r3); blr
u32 cEmSwitch::ckSwitch()
{
    return mSwitch;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 801123cc-801123d3, 8 bytes / 2 instructions: stw r4,0x618(r3); blr
void cEmSwitch::setActButton(u32 button)
{
    mActButton = button;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 801125a4-801125af, 12 bytes / 3 instructions: li r0,1; stb r0,0x608(r3); blr
void cEmSwitch::setOpenOnly()
{
    mOpenMode = 1;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 801125b0-801125bb, 12 bytes / 3 instructions: li r0,3; stb r0,0x608(r3); blr
void cEmSwitch::setAutoOpen()
{
    mOpenMode = 3;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 801125bc-801125c7, 12 bytes / 3 instructions: li r0,1; stb r0,0x60a(r3); blr
void cEmSwitch::setBarrel()
{
    mBarrel = 1;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 801169e4-801169eb, 8 bytes / 2 instructions: lbz r3,0x69c(r3); blr
u8 cEmWindow::GetFloor()
{
    return mFloor;
}
