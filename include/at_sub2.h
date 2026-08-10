#ifndef AT_SUB2_H
#define AT_SUB2_H

#include "types.h"

// Per-weapon hit effect ids (game/at_sub2.cpp). Layout partially known.
class AtEffInfo {
public:
    u32 flags;      // 0x00 bit0: registered (est.cpp EspSetEatEffect)
    u32 eff0[2];    // 0x04  (cEatMgr::initEffInfo presets every pair's first id to 0xD2)
    u32 eff13[2];   // 0x0C weapon 0x13
    u32 eff16[2];   // 0x14 weapon 0x16
    u32 eff17[2];   // 0x1C weapon 0x17
    u32 effGun[2];  // 0x24 weapons 1..12, 0x0F, 0x11, 0x21
    u32 eff5[2];    // 0x2C
    u32 eff6[2];    // 0x34
    u32 eff0D[2];   // 0x3C weapon 0x0D

    int getWepEff(int wepId, u32* eff1, u32* eff2);
};

#endif
