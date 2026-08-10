// game/cDataSwap: swap a memory range out to a heap copy or ARAM so heap 11 can reuse it
// (D:/Bio4/Prog/cDataSwap.cpp).
#include "types.h"
#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dvd.h"
#include "main_mem.h"
#include "datactrl.h"
#include "cDataSwap.h"

extern "C" {
void SubScreenAramRead();
}

cDataSwap::cDataSwap()
{
    flag = 0;
    addr = 0;
    aram = 0;
    size = 0;
}

cDataSwap::~cDataSwap()
{
}

int cDataSwap::SwapOut(u32 addr, u32 size, u32 aram)
{
    int ret = 0;

    if (flag != 0) {
        return 0;
    }
    heap = MemGetCurrentHeap();
    this->size = size;
#line 64 "D:/Bio4/Prog/cDataSwap.cpp"
    mram = MEM_ALLOC(size, 0, 13);
    if (mram == NULL) {
        this->aram = DC.getAramFree(size);
        if (this->aram != 0) {
            flag |= 2;
        } else if (aram != 0) {
            this->aram = aram;
            flag |= 2;
        } else if (size > 0x2FFFFF) {
            return 0;
        } else {
            this->aram = 0xD00000;
            flag |= 2;
        }
        if (flag & 2) {
            this->addr = addr;
            Aram.DmaTransReq(0, addr, this->aram, this->size, 1);
        }
    } else {
        this->addr = (u32) mram;
        flag |= 1;
    }
    if (flag & 3) {
        MemSuspendHeap(heap);
        ret = 1;
        MemCreateHeap(11, this->addr, this->addr + this->size);
        MemSetCurrentHeap(11);
    }
    return ret;
}

void cDataSwap::SwapIn()
{
    if (flag != 0) {
        MemDestroyHeap(11);
        if (flag & 2) {
            Aram.DmaTransReq(1, aram, addr, size, 1);
            if (aram == 0xD00000) {
                SubScreenAramRead();
            }
        }
        MemSignalHeap(heap);
        MemSetCurrentHeap(heap);
        if (mram != NULL) {
            Mem_free(mram);
        }
        flag = 0;
    }
}
