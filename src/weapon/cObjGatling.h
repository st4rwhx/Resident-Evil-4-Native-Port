#ifndef COBJGATLING_H
#define COBJGATLING_H

#include "types.h"

class cObjGatling {
public:
    void setFire();
    void setRide(u32 ride);
    void stopFire();
    void setReload();
    void setMaxRot(f32 rot);
    void setBreakMode(u8 mode);
private:
    u8 unk_00[0x334];
    u8 mFire;         // 0x334 - set to 1 (true) by setFire()
    u8 mFireStop;      // 0x335 - set to 0 by stopFire()
    u8 mReload;         // 0x336 - set to 0x28 by setReload()
    u8 mBreakMode;       // 0x337 - set by setBreakMode(u8)
    u8 unk_338[0x33c - 0x338];
    f32 mMaxRot;          // 0x33c
    u8 unk_340[0x364 - 0x340];
    u32 mRide;             // 0x364
};

#endif
