#ifndef CCARD_H
#define CCARD_H

#include "types.h"

class cCard {
public:
    void firstCheck30();
private:
    u8 unk_00[0x4];
    u8 mCheck30;  // 0x4 - set to 4 by firstCheck30()
};

#endif
