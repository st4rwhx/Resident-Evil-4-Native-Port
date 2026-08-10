#ifndef COBJLADDER_H
#define COBJLADDER_H

#include "types.h"

class cObjLadder {
public:
    u32 getStatus();
    u8 getType();
    void setClimb();
    u32 getLadderNum();
    void setLadderInfo(u32 num, u8 type);
    void setResetReserve();
    void setCamera(u32 camera);
private:
    u8 unk_00[0x101];
    u8 mType;       // 0x101
    u8 unk_102[0x32c - 0x102];
    u32 mStatus;     // 0x32c
    u8 unk_330[0x334 - 0x330];
    u32 mLadderNum;  // 0x334
    u8 unk_338[0x34c - 0x338];
    u32 mClimb;      // 0x34c
    u32 mResetReserve; // 0x350 (immediately follows mClimb, no gap)
    u8 unk_354[0x35c - 0x354];
    u32 mCamera;      // 0x35c
};

#endif
