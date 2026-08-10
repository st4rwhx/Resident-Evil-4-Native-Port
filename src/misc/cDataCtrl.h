#ifndef CDATACTRL_H
#define CDATACTRL_H

#include "types.h"

class cDataCtrl {
public:
    void setAramSort(u32 sort);
private:
    u8 unk_00[0xa04];
    u32 mAramSort;  // 0xa04
};

#endif
