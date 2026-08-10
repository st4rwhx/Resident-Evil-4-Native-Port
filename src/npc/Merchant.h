#ifndef MERCHANT_H
#define MERCHANT_H

#include "types.h"

class Merchant {
public:
    u8 sellingItemNum();
    u8 exerciseItemNum();
private:
    u8 unk_00[0x314];
    u8 mExerciseItemNum;  // 0x314
    u8 mSellingItemNum;    // 0x315
};

#endif
