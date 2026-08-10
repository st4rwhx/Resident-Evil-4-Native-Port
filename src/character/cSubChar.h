#ifndef CSUBCHAR_H
#define CSUBCHAR_H

#include "types.h"

class cSubChar {
public:
    bool ckPlRun();
    void setFace();
private:
    u8 unk_00[0x434];
    u32 mRunFlags;  // 0x434 - bit 0x8 tested/returned by ckPlRun
};

#endif
