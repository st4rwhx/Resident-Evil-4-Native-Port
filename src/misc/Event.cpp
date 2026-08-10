#include "Event.h"

// --- Matching status: MATCHED (byte-identical, verified against ground truth), all 15 ExePacket_* below ---
bool Event::ExePacket_BeginEvt() { return true; }
bool Event::ExePacket_SetEm() { return true; }
bool Event::ExePacket_SetList() { return true; }
bool Event::ExePacket_CamPos() { return true; }
bool Event::ExePacket_CamDammy() { return true; }
bool Event::ExePacket_PosPl() { return true; }
bool Event::ExePacket_ParentOn() { return true; }
bool Event::ExePacket_ParentOff() { return true; }
bool Event::ExePacket_EndPl() { return true; }
bool Event::ExePacket_EndEm() { return true; }
bool Event::ExePacket_EndOm() { return true; }
bool Event::ExePacket_EndParts() { return true; }
bool Event::ExePacket_EndList() { return true; }
bool Event::ExePacket_EndEvt() { return true; }
bool Event::ExePacket_EndPac() { return true; }

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 8013814c-80138157, 12 bytes / 3 instructions: li r0,0; stw r0,0xe4(r3); blr
void Event::EndActBtn()
{
    mActBtnCount = 0;
}

// --- Matching status: MATCHED (byte-identical, verified against ground truth) ---
// 80138158-8013815f, 8 bytes / 2 instructions: lwz r3,0xe8(r3); blr
u32 Event::GetActBtnCount()
{
    return mActBtnCount2;
}
