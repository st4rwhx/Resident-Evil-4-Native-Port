#ifndef ENEMYMISC2_H
#define ENEMYMISC2_H

#include "types.h"

class cEmHit {
public:
    u32 ckStatus();
private:
    u8 unk_00[0x3ec];
    u32 mStatus;  // 0x3ec
};

class cEmItem {
public:
    void setEff(u8 eff);
    u32 ckStatus();
    void setRotType(u8 type);
private:
    u8 unk_00[0x3ec];
    u32 mStatus;   // 0x3ec
    u8 unk_3f0[0x640 - 0x3f0];
    u8 mRotType;    // 0x640
    u8 mEff;         // 0x641
};

class cEmRack {
public:
    void setEff(u8 eff);
private:
    u8 unk_00[0x4dc];
    u8 mEff;  // 0x4dc
};

// Fields laid out in ascending offset order (0x5f0 comes before 0x608).
class cEmSwitch {
public:
    u32 ckSwitch();
    void setActButton(u32 button);
    void setOpenOnly();
    void setAutoOpen();
    void setBarrel();
private:
    u8 unk_00[0x5f0];
    u32 mSwitch;     // 0x5f0
    u8 unk_5f4[0x608 - 0x5f4];
    u8 mOpenMode;      // 0x608 - setOpenOnly() sets 1, setAutoOpen() sets 3
    u8 unk_609[0x60a - 0x609];
    u8 mBarrel;          // 0x60a
    u8 unk_60b[0x618 - 0x60b];
    u32 mActButton;        // 0x618
};

class cEmWindow {
public:
    u8 GetFloor();
private:
    u8 unk_00[0x69c];
    u8 mFloor;  // 0x69c
};

#endif
