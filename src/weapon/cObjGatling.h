#ifndef COBJGATLING_H
#define COBJGATLING_H

#include "types.h"

class cObjGatling {
public:
    void setFire();
private:
    u8 unk_00[0x334];
    u8 mFire;  // 0x334 - set to 1 (true) by setFire()
};

#endif
