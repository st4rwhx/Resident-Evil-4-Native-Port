#ifndef STUBS_H
#define STUBS_H

#include "types.h"

// Empty (blr-only) methods across many unrelated classes - debug-build
// stubs/no-op hooks not yet implemented for these object types, or base-
// class virtual overrides with nothing to do.

class cObjEgg {
public:
    void waterExplode();
};

class CameraLookDownEm {
public:
    void move();
};

class IDApplication {
public:
    void init();
    void move();
    void quit();
    void operator_delete();
};

class cCamera {
public:
    void operator_delete();
};

class cCtrl {
public:
    void move();
    void trans();
};

class cEm {
public:
    void move();
    bool checkThrow();  // returns constant false - not empty, but tiny
};

class cEmRock {
public:
    void beginEvent();
};

class cEmTree {
public:
    void beginEvent();
    void setCatch();
private:
    u8 unk_00[0x455];
    u8 mCatch;  // 0x455
};

class cEsp {
public:
    bool SetFreeWork();  // returns constant true
    void Destruct();
};

#endif
