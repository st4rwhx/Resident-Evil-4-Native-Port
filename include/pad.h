#ifndef PAD_H
#define PAD_H

#include "types.h"
#include "joy.h"

// Dolphin PAD (the SDK header pulls in the CodeWarrior libc).
struct PADStatus {
    u16 button;       // 0x00
    s8 stickX;        // 0x02
    s8 stickY;        // 0x03
    s8 substickX;     // 0x04
    s8 substickY;     // 0x05
    u8 triggerLeft;   // 0x06
    u8 triggerRight;  // 0x07
    u8 analogA;       // 0x08
    u8 analogB;       // 0x09
    s8 err;           // 0x0A
    u8 pad_B;
};

#define PAD_CHAN0_BIT 0x80000000
#define PAD_ERR_NONE 0
#define PAD_ERR_NO_CONTROLLER -1
#define PAD_ERR_NOT_READY -2
#define PAD_ERR_TRANSFER -3

// Vibration pattern table (VibSetData): offsets from the table start to VibData blocks.
struct VibDataEntry {
    u16 type;  // 0x00
    u16 wait;  // 0x02
    u16 time;  // 0x04
    u8 lvl0;   // 0x06  start level
    u8 lvl1;   // 0x07  end level
};

struct VibData {
    u32 num;
    VibDataEntry e[1];
};

struct VibDataTbl {
    u32 num;
    u32 ofs[1];
};

extern "C" {
void PADInit();
u32 PADRead(PADStatus* status);
void PADClamp(PADStatus* status);
BOOL PADReset(u32 mask);
void PADControlMotor(int chan, u32 cmd);
void PADSetAnalogMode(u32 mode);

// game/pad.cpp
void PadInit();
void PadRead();
void KeyStop(u64 mask);
void KeyClear(u64 mask);
void VibControl();
VibWork* PullVibWork();
void VibSet(u32 time, u32 level, u16 wait, u16 type);
void VibSetDataCore(VibData* d, u32 type);
void VibSetData(VibDataTbl* t, u32 no, u32 type);
void VibSetClearType(u32 type);
int PadCheckStatus(JOY* joy);
void Pad_test();

extern u32 Key_type_tbl[2][64];
}

#endif
