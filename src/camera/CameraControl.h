#ifndef CAMERACONTROL_H
#define CAMERACONTROL_H

#include "types.h"

// Minimal stub structs for pointed-to types CameraControl references but
// whose own layout isn't recovered yet - only the specific offset each
// known function actually touches. Standard decomp practice: pad up to
// the known offset, name only what's confirmed.
struct AreaData {
    u8 unk_00[5];
    u8 num;  // +5 - read by CameraControl::AreaNum
};

struct MotionBase {
    u8 unk_000[0x100];
    u8 unk_100[0x1d0 - 0x100];  // whatever getMotionInfoPtr() actually returns a pointer to
    void* matPtr;                 // +0x1d0 - written by CameraControl::setMotionBaseMatPtr
};

// Layout is only known at the offsets touched by matched/attempted
// functions so far. Everything else is unrecovered padding - standard
// practice until each field is individually confirmed by matching the
// functions that touch it.
class CameraControl {
public:
    void AreaCheckOnOff(s32 mode);
    void Disable();
    bool IsChangeCamera();
    s32 CurrentAreaNo();
    s32 CurrentCameraNo();
    u8 AreaNum();
    void setMotionBaseMatPtr(void* mat);
    void* getMotionInfoPtr();

    // Grants native/test_main.cpp access to private fields by name so it can
    // set up test state and verify results without depending on this
    // struct's byte offsets - those are GameCube/32-bit-pointer specific
    // (see the padding arrays below) and don't hold on a native 64-bit
    // build where AreaData*/MotionBase* are 8 bytes instead of 4. Friendship
    // is compile-time only; it changes nothing about this class's generated
    // code or memory layout, so it has zero effect on PowerPC byte-matching.
    friend struct CameraControlTestAccess;

private:
    u8 unk_00[0x2c];  // fields not yet recovered
    u32 mFlags;        // 0x2c - bit 0x8 and 0x10 confirmed by AreaCheckOnOff
    u32 mFlags2;        // 0x30 - bit 0x2 tested by IsChangeCamera; also touched by Comeback()
    u8 mUnk34;          // 0x34 - cleared to 0 by Disable(); purpose not yet known
    u8 unk_35[0x5c - 0x35];  // fields not yet recovered
    AreaData* mAreaDataPtr; // 0x5c - read by AreaNum
    u8 unk_60[0x68c - 0x60];  // fields not yet recovered
    MotionBase* mMotionBase; // 0x68c - read/written by [gs]etMotionBase*/getMotionInfoPtr
    s8 mCurrentAreaNo;        // 0x690
    u8 unk_691[1];             // gap between the two - purpose not yet known
    s8 mCurrentCameraNo;      // 0x692
};

#endif
