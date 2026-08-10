#ifndef BULLETINFO_H
#define BULLETINFO_H

#include "types.h"

class BulletInfo {
public:
    void roomInit();
private:
    u8 unk_00[0x10];
    s32 mRoomId;  // 0x10 - reset to -1 by roomInit()
};

#endif
