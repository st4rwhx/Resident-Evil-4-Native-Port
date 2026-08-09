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

private:
    u8 unk_00[0x2c];  // fields not yet recovered
    u32 mFlags;        // 0x2c - bit 0x8 and 0x10 confirmed by AreaCheckOnOff
};

#endif
