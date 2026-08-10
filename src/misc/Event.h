#ifndef EVENT_H
#define EVENT_H

#include "types.h"

// The ExePacket_* family are event-script opcode handlers. Every one
// dumped so far just returns true unconditionally in this debug build -
// consistent with a base/default no-op implementation shared across all
// packet types (real per-type behavior might come from a virtual
// override elsewhere not yet matched, or simply wasn't implemented yet
// at the point this debug build was compiled).
class Event {
public:
    bool ExePacket_BeginEvt();
    bool ExePacket_SetEm();
    bool ExePacket_SetList();
    bool ExePacket_CamPos();
    bool ExePacket_CamDammy();
    bool ExePacket_PosPl();
    bool ExePacket_ParentOn();
    bool ExePacket_ParentOff();
    bool ExePacket_EndPl();
    bool ExePacket_EndEm();
    bool ExePacket_EndOm();
    bool ExePacket_EndParts();
    bool ExePacket_EndList();
    bool ExePacket_EndEvt();
    bool ExePacket_EndPac();
    void EndActBtn();
    u32 GetActBtnCount();
private:
    u8 unk_00[0xe4];
    u32 mActBtnCount;  // 0xe4 - cleared to 0 by EndActBtn()
    u32 mActBtnCount2;  // 0xe8 - returned by GetActBtnCount() (different offset - separate field)
};

#endif
