#ifndef CAMERAQUASIFPS_H
#define CAMERAQUASIFPS_H

#include "types.h"

class CameraQuasiFPS {
public:
    void LRinfo(u32 info);
    void setBlendRatio(f32 ratio);
    f32 getFloorRatio();
private:
    u8 unk_00[0x158];
    u32 mLRInfo;      // 0x158
    u8 unk_15c[0x1e4 - 0x15c];
    f32 mBlendRatio;   // 0x1e4
    u8 unk_1e8[0x208 - 0x1e8];
    f32 mFloorRatio;   // 0x208
};

#endif
