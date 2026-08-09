#ifndef CAMERACONTROL_H
#define CAMERACONTROL_H

#include "types.h"

// Layout is only known up to offset 0x2c so far (the one field used by
// AreaCheckOnOff, the first function we're attempting to match). Everything
// before it is unrecovered padding - standard practice until each field is
// individually confirmed by matching the functions that touch it.
class CameraControl {
public:
    void AreaCheckOnOff(s32 mode);
    void Disable();
    s32 CurrentAreaNo();
    s32 CurrentCameraNo();

private:
    u8 unk_00[0x2c];  // fields not yet recovered
    u32 mFlags;        // 0x2c - bit 0x8 and 0x10 confirmed by AreaCheckOnOff
    u8 unk_30[4];       // fields not yet recovered
    u8 mUnk34;          // 0x34 - cleared to 0 by Disable(); purpose not yet known
    u8 unk_35[0x690 - 0x35];  // fields not yet recovered
    s8 mCurrentAreaNo;   // 0x690
    u8 unk_691[1];        // gap between the two - purpose not yet known
    s8 mCurrentCameraNo; // 0x692
};

#endif
