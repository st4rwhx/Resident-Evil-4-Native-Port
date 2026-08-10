#ifndef CEMDOOR_H
#define CEMDOOR_H

#include "types.h"

class cEmDoor {
public:
    void setSeCancel();
private:
    u8 unk_00[0x81e];
    u8 mSeCancel;  // 0x81e - set to 1 (true) by setSeCancel()
};

#endif
