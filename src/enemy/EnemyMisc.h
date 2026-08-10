#ifndef ENEMYMISC_H
#define ENEMYMISC_H

#include "types.h"

class cEmBar {
public:
    void setEff(u8 eff);
    void setMotion(u32 motion);
private:
    u8 unk_00[0x4fc];
    u32 mMotion;  // 0x4fc
    u8 mEff;       // 0x500 (immediately follows mMotion, no gap)
};

class cEmWep {
public:
    void setAtNo(u32 atNo);
private:
    u8 unk_00[0x474];
    u32 mAtNo;  // 0x474
};

class cDmgInfo {
public:
    void set_0(u8 a, u8 b);
private:
    u8 mField0;  // 0x0
    u8 mField1;  // 0x1
};

class cEmBarrel {
public:
    void setEff(u8 eff);
private:
    u8 unk_00[0x46c];
    u8 mEff;  // 0x46c
};

#endif
