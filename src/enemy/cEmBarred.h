#ifndef CEMBARRED_H
#define CEMBARRED_H

#include "types.h"

class cEmBarred {
public:
    void setEff(u8 eff);
private:
    u8 unk_00[0x62d];
    u8 mEff;  // 0x62d
};

#endif
