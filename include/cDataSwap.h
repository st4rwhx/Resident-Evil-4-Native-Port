#ifndef CDATASWAP_H
#define CDATASWAP_H

#include "types.h"

// Parks a memory range (MRAM heap or ARAM) so heap 11 can reuse it, and restores it later
// (game/cDataSwap.cpp; used by card/mercenaries/sce_com).
class cDataSwap {
public:
    u32 flag;  // 0x00  1: copy in MRAM (mram), 2: copy in ARAM (aram)
    u32 addr;  // 0x04  range start (heap 11 start)
    u32 aram;  // 0x08  ARAM copy address
    u32 size;  // 0x0C
    u32 heap;  // 0x10  heap that was current at SwapOut
    void* mram;// 0x14  MRAM copy (mem_alloc)

    cDataSwap();
    ~cDataSwap();
    int SwapOut(u32 addr, u32 size, u32 aram);
    void SwapIn();
};

#endif
