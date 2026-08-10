#ifndef OBJECTMISC_H
#define OBJECTMISC_H

#include "types.h"

class cObj16 {
public:
    void setDieEff();
private:
    u8 unk_00[0x354];
    u32 mDieEff;  // 0x354
};

class cObjBull {
public:
    u32 getMoveFrameToLift();
    u32 getMoveFrameRtn();
    void setBreakTruck();
private:
    u8 unk_00[0x374];
    u32 mMoveFrame;  // 0x374 - both getters return this same field
    u8 unk_378[0x38b - 0x378];
    u8 mBreakTruck;   // 0x38b
};

class cObjPillar {
public:
    void setMotion(u32 motion);
private:
    u8 unk_00[0x334];
    u32 mMotion;  // 0x334
};

class cObjYagura {
public:
    void setMotionVib(u32 vib);
private:
    u8 unk_00[0x348];
    u32 mMotionVib;  // 0x348
};

class cBlock {
public:
    void setOtStart();
private:
    u8 unk_00[0x1c];    // fields not yet recovered
    u8 unk_1c[4];        // 0x1c - the object itself points into itself here
    void* mOtStart;       // 0x20 - set to &this[0x1c] by setOtStart()
};

#endif
