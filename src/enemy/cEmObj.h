#ifndef CEMOBJ_H
#define CEMOBJ_H

#include "types.h"

class cEmObj {
public:
    void setEff(u8 eff);
private:
    u8 unk_00[0x63c];
    u8 mEff;  // 0x63c
};

#endif
