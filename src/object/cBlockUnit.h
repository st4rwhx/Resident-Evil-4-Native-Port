#ifndef CBLOCKUNIT_H
#define CBLOCKUNIT_H

#include "types.h"

class cBlockUnit {
public:
    void setBlockCommand(u32 a, u8 b);
private:
    u8 unk_00[0x4];
    u32 mField4;  // 0x4
    u8 unk_8[0xd - 0x8];
    u8 mFieldD;     // 0xd
};

#endif
