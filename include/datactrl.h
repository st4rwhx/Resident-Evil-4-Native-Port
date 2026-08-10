#ifndef DATACTRL_H
#define DATACTRL_H

#include "types.h"

// One streamed data file (game/datactrl.cpp, 0x50 bytes).
class cDataUnit {
public:
    s32 condition;   // 0x00  0 none, 1 MRAM loading, 2 MRAM ok, 3 ARAM loading, 4 ARAM ok,
                     //       5 ARAM->MRAM, 6 MRAM->ARAM, 7 ARAM->ARAM, 8 MRAM->MRAM
    s32 command;     // 0x04  0 none, 1 load to MRAM, 2 load to ARAM, 3 clear, 4 delete
    s32 err;         // 0x08
    u8 flag;         // 0x0C  bit0 in use, bit1 memory allocated by the unit
    u8 wait;         // 0x0D  setCommand argument (1 = synchronous)
    u8 waitFlag;     // 0x0E  set while waitUseOk/waitLoadOk spin
    u8 heap;         // 0x0F  heap the allocation came from
    void* addr;      // 0x10  current address of the data
    u32 arg;         // 0x14  setCommand argument: destination (0 = allocate)
    void* mallocAddr;// 0x18
    u32 dest;        // 0x1C  destination of the running transfer
    u32 fixAddr;     // 0x20  fixed MRAM destination (fixMramAddr)
    u32 size;        // 0x24
    u8 pad_28[4];
    char name[0x20]; // 0x2C
    int reqNo;       // 0x4C  DVD / ARAM request number

    cDataUnit() {}
    ~cDataUnit() {}

    int chk(u32 bit) {
        if (flag & bit) {
            return 1;
        }
        return 0;
    }
    void setName(char* s);

    void setCommand(int cmd, u32 arg, u8 wait);
    int getCommand();
    void setCondition(int c);
    int getCondition();
    void checkMallocRelease();
    void setMallocInfo(int on, void* p);
    void fixMramAddr(u32 a);
    int isUseOk();
    int waitUseOk();
    int isLoadOk();
    int waitLoadOk();
    void setLoadToMram();
    void setLoadToAram();
    int setClear();
    int setDelete();
    void checkLoadToMram();
    void checkLoadToAram();
    void checkAramToMram();
    void checkMramToAram();
    void checkAramToAram();
    void checkMramToMram();
    void checkCommand();
    void checkCondition();
    // Inline accessors: as call arguments they make GCC precompute the values before the
    // stack argument stores (block.cpp dispDebugInfo).
    void* getAddr() { return addr; }
    u32 getArg() { return arg; }
    u32 getDest() { return dest; }
    u32 getSize() { return size; }
};

// Room data unit controller (game/datactrl.cpp, `DC`, 0xAA4 bytes).
class cDataCtrl {
public:
    cDataUnit unit[32];  // 0x000
    u32 aramEnd;         // 0xA00  first free ARAM address above the loaded units
    s32 aramSort;        // 0xA04  1 = repack the ARAM units (checkAramSort)
    s32 xA08;            // 0xA08  0 while the sub screen owns the ARAM area (sscrn), 1 otherwise
    s32 xA0C;            // 0xA0C  1 = commands are not executed immediately
    void* dispBuf;       // 0xA10  dispDebug tiles
    u32 dispBase;        // 0xA14
    u32 dispEnd;         // 0xA18
    s32 dbgHeap;         // 0xA1C  1 = allocate from the debug heap
    s32 dummyId[32];     // 0xA20  dummy.dat read requests (dev mode)
    void* dummyBuf;      // 0xAA0

    u32 getAramFree(u32 size);
    void init();
    void initDataUnit();
    void deleteAll();
    cDataUnit* setData(char* name);
    cDataUnit* getNewUnit();
    void setAramSort(int on);
    int checkAramSort();
    void dispDebug();
    void initDummyId();
    void setDummyId(int id);
    void checkDummyId();
    void check();
};
extern cDataCtrl DC;

#endif
