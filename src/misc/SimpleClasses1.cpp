#include "SimpleClasses1.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80010f3c-80010f43, 8 bytes / 2 instructions: stw r4,0x1c(r3); blr
void CardID::setAction(u32 action)
{
    mAction = action;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8001156c-8001156f, 4 bytes / 1 instruction: blr
void cUnit::beginEvent()
{
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80011570-80011573, 4 bytes / 1 instruction: blr
void cUnit::endEvent()
{
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80029964-8002996f, 12 bytes / 3 instructions: li r0,1; sth r0,0x8e(r3); blr
void Message::WaitEnd()
{
    mWaitFlag = 1;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800d9b80-800d9b87, 8 bytes / 2 instructions: stw r4,0x8(r3); blr
void CountDown::initTimeFrame(u32 frame)
{
    mFrame = frame;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800d9c94-800d9c9b, 8 bytes / 2 instructions: lwz r3,0x8(r3); blr
u32 CountDown::getFrame()
{
    return mFrame;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800d9fb4-800d9fbb, 8 bytes / 2 instructions: lwz r3,0x0(r3); blr
char* cString::c_str()
{
    return mData;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800e102c-800e1037, 12 bytes / 3 instructions: stb r4,0x6(r3); li r3,1; blr
bool cLog::on(u8 v)
{
    mFlag = v;
    return true;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800e8ed8-800e8edf, 8 bytes / 2 instructions: lwz r3,0x3c(r3); blr
u32 cDvdQueue::fileGetLength()
{
    return mFileLength;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800da818-800da81f, 8 bytes / 2 instructions: lwz r3,0x4(r3); blr
u32 cDataUnit::getCommand()
{
    return mCommand;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800da820-800da827, 8 bytes / 2 instructions: stw r4,0x0(r3); blr
void cDataUnit::setCondition(u32 cond)
{
    mCondition = cond;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800da828-800da82f, 8 bytes / 2 instructions: lwz r3,0x0(r3); blr
u32 cDataUnit::getCondition()
{
    return mCondition;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 800cccfc-800ccd03, 8 bytes / 2 instructions: addi r3,r3,0x14; blr
void* cSatFile::getVertexPtr()
{
    return (u8*)this + 0x14;
}
