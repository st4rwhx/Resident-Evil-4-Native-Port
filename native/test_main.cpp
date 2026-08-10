// Compiles and exercises every currently-matched/equivalent CameraControl
// function with a native Windows toolchain, verifying both that the code
// builds clean and that it behaves identically to the confirmed PowerPC
// semantics.
//
// Note on a real finding from writing this test: the first version poked
// private fields directly at their documented GameCube byte offsets (e.g.
// 0x690 for mCurrentAreaNo) and silently read back wrong values / crashed.
// Reason: CameraControl.h's layout is only self-consistent on a 32-bit
// target - AreaData* and MotionBase* are 4 bytes there but 8 bytes on
// native x86_64, so every field declared after either pointer lands at a
// different real offset once compiled natively. This is a genuine, general
// Phase 2 concern (any matched struct with an embedded pointer needs this
// accounted for), not a one-off bug - see CameraControl.h's
// CameraControlTestAccess friend declaration for how this test sidesteps
// it (goes through the class by field *name*, letting whichever compiler
// built it pick the real offsets, instead of assuming GameCube's).
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "CameraControl.h"

struct CameraControlTestAccess {
    static u32& flags(CameraControl& c) { return c.mFlags; }
    static u32& flags2(CameraControl& c) { return c.mFlags2; }
    static u8& unk34(CameraControl& c) { return c.mUnk34; }
    static AreaData*& areaDataPtr(CameraControl& c) { return c.mAreaDataPtr; }
    static MotionBase*& motionBase(CameraControl& c) { return c.mMotionBase; }
    static s8& currentAreaNo(CameraControl& c) { return c.mCurrentAreaNo; }
    static s8& currentCameraNo(CameraControl& c) { return c.mCurrentCameraNo; }
};
using T = CameraControlTestAccess;

static int g_failures = 0;

#define CHECK(cond, desc)                                                    \
    do {                                                                     \
        if (cond) {                                                          \
            std::printf("  [PASS] %s\n", desc);                             \
        } else {                                                             \
            std::printf("  [FAIL] %s\n", desc);                             \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

int main()
{
    std::printf("RE4 native port scaffolding - matched-function smoke test\n");
    std::printf("(see native/CMakeLists.txt for what this does and doesn't prove)\n\n");

    CameraControl camObj{};
    CameraControl* cam = &camObj;

    std::printf("CurrentAreaNo / CurrentCameraNo (MATCHED, byte-identical):\n");
    T::currentAreaNo(*cam) = 7;
    T::currentCameraNo(*cam) = -3; // exercise sign-extension (s8 -> s32)
    CHECK(cam->CurrentAreaNo() == 7, "CurrentAreaNo() reads mCurrentAreaNo");
    CHECK(cam->CurrentCameraNo() == -3, "CurrentCameraNo() sign-extends mCurrentCameraNo");

    std::printf("\ngetMotionInfoPtr (MATCHED, byte-identical):\n");
    alignas(8) unsigned char motionBuf[0x1e0];
    std::memset(motionBuf, 0, sizeof(motionBuf));
    MotionBase* motionBase = reinterpret_cast<MotionBase*>(motionBuf);
    T::motionBase(*cam) = motionBase;
    void* infoPtr = cam->getMotionInfoPtr();
    CHECK(infoPtr == motionBuf + 0x100, "getMotionInfoPtr() == mMotionBase + 0x100");

    std::printf("\nsetMotionBaseMatPtr (Equivalent - logic verified, byte-exact TBD):\n");
    int dummyMat = 0;
    cam->setMotionBaseMatPtr(&dummyMat);
    CHECK(motionBase->matPtr == &dummyMat, "setMotionBaseMatPtr() writes mMotionBase->matPtr");

    std::printf("\nAreaNum (Equivalent):\n");
    AreaData areaData{};
    areaData.num = 42;
    T::areaDataPtr(*cam) = &areaData;
    CHECK(cam->AreaNum() == 42, "AreaNum() reads mAreaDataPtr->num");

    std::printf("\nAreaCheckOnOff (Equivalent - logic verified):\n");
    T::flags(*cam) = 0;
    cam->AreaCheckOnOff(0);
    CHECK((T::flags(*cam) & 0x8) != 0, "AreaCheckOnOff(0) sets bit 0x8");
    cam->AreaCheckOnOff(1);
    CHECK((T::flags(*cam) & 0x8) == 0 && (T::flags(*cam) & 0x10) != 0,
          "AreaCheckOnOff(1) clears bit 0x8, sets bit 0x10");

    std::printf("\nDisable (Equivalent):\n");
    T::flags(*cam) = 0;
    T::unk34(*cam) = 0xFF;
    cam->Disable();
    CHECK(T::unk34(*cam) == 0, "Disable() clears mUnk34");
    CHECK((T::flags(*cam) & 0x8) != 0, "Disable() sets mFlags bit 0x8");

    std::printf("\nIsChangeCamera (Equivalent):\n");
    T::flags2(*cam) = 0;
    CHECK(cam->IsChangeCamera() == false, "IsChangeCamera() false when bit 0x2 clear");
    T::flags2(*cam) = 0x2;
    CHECK(cam->IsChangeCamera() == true, "IsChangeCamera() true when bit 0x2 set");

    std::printf("\n%s\n", g_failures == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
    return g_failures == 0 ? 0 : 1;
}
