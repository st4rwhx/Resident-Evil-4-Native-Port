#ifndef CPLNECK_H
#define CPLNECK_H

#include "types.h"

class cPlNeck {
public:
    void setMode(u8 mode);
private:
    u8 unk_00[0xe];
    u8 mMode;  // 0xe
};

#endif
