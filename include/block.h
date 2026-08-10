#ifndef BLOCK_H
#define BLOCK_H

#include "types.h"
#include "vec.h"
#include "area.h"

class cDataUnit;

// Room block streaming (game/block.cpp): the room is split into blocks whose models (cSmd data
// units, "st%x/r%03x_%02x.dat") are loaded to MRAM/ARAM or deleted as the player moves between
// trigger areas. `BLK` file layout: header, per-block link table, OT-linked trigger areas and the
// per-area connect table (which blocks live in MRAM/ARAM while the player stands in that area).

// One block of the `BLK` link table (stride 0xC).
struct BlockLink {
    u8 flags;       // 0x00  bit0: block exists
    u8 pad_1[3];
    s8 link[8];     // 0x04  neighbouring blocks (-1: none)
};

// Trigger area of the `BLK` file (stride 0x38), chained through the cBlock ordering table.
struct BlockArea {
    u32 tag;        // 0x00  OT link
    AreaData area;  // 0x04
    u8 flags;       // 0x34  bit0: active
    u8 x35;
    s8 areaNo;      // 0x36
    u8 pri;         // 0x37  OT slot (0..7)
};

// Connect table entry (stride 0x14): which blocks to keep for one area.
struct BlockConnect {
    u8 flags;       // 0x00  bit0: entry used
    u8 blockNo;     // 0x01  block the area belongs to
    u8 pad_2[2];
    s8 mram[8];     // 0x04  blocks loaded to MRAM (-1: none)
    s8 aram[8];     // 0x0C  blocks loaded to ARAM (-1: none)
};

struct BlockHeader {
    char tag[4];    // 0x00  "BLK"
    u16 version;    // 0x04  0x100
    u8 x6;
    u8 nBlock;      // 0x07
    u16 nArea;      // 0x08
    u16 nConnect;   // 0x0A
    u32 ofsLink;    // 0x0C  -> BlockLink[nBlock]
    u32 ofsArea;    // 0x10  -> BlockArea[nArea]
    u32 ofsConnect; // 0x14  -> BlockConnect[nConnect]
};

// cBlockUnit::state
#define BLOCK_NO_DATA       0
#define BLOCK_MRAM_LOAD_SET 1
#define BLOCK_MRAM_LOAD     2
#define BLOCK_CREATE        3
#define BLOCK_ARAM_LOAD_SET 4
#define BLOCK_ARAM_LOAD     5
#define BLOCK_ARAM_OK       6
#define BLOCK_DELETE        7

// cBlockUnit::command
#define BLOCK_CMD_NONE      0
#define BLOCK_CMD_MRAM_LOAD 1
#define BLOCK_CMD_ARAM_LOAD 2
#define BLOCK_CMD_DELETE    3

// Per-block work (0x10 bytes, cBlock::pUnit[nBlock]).
class cBlockUnit {
public:
    int state;          // 0x00  BLOCK_*
    int command;        // 0x04  BLOCK_CMD_*
    cDataUnit* pData;   // 0x08  data unit of the block file
    u8 flags;           // 0x0C  bit0: in use
    u8 arg;             // 0x0D  setBlockCommand argument, passed on to cDataUnit::setCommand
    u8 no;              // 0x0E  block number
    u8 pad_F;

    void setBlockCommand(int cmd, int a);
    void setTrans(int on);
    void setBlockLoadToMram();
    void setBlockLoadToAram();
    void setBlockDelete();
    int checkBlockLoadToMramSet();
    int checkBlockLoadToMram();
    int checkBlockLoadToAramSet();
    int checkBlockLoadToAram();
    int checkBlockDelete();
    void recalcModelAddr(int ofs);
    void moveBlockData(void* dst);
};

class cBlock {
public:
    u32 ot[8];              // 0x00  ordering table of the trigger areas (slot = BlockArea::pri)
    u32* pOt;               // 0x20  getOtAddr cursor
    u8 nBlock;              // 0x24
    s8 prevArea;            // 0x25
    u8 pad_26[2];
    int stopFlagSet;        // 0x28  1 = a block is loading: pG->flags_170 is saved and forced to -1
    u32 stopFlag;           // 0x2C  saved pG->flags_170
    void* memTop;           // 0x30  block model memory
    void* memEnd;           // 0x34
    void* memCur;           // 0x38  next free address (getBlockMemFree)
    BlockHeader* pData;     // 0x3C  BLK file
    BlockLink* pLink;       // 0x40
    BlockArea* pArea;       // 0x44
    BlockConnect* pConnect; // 0x48
    cBlockUnit* pUnit;      // 0x4C
    u8 pad_50[0x60 - 0x50];
    u32 mramSet;            // 0x60  bit set (bit 31 - n) of the blocks wanted in MRAM
    u32 aramSet;            // 0x64
    int noMemCtrl;          // 0x68  1 = data units allocate their own memory (no memTop pool)
    int debugData;          // 0x6C  1 = pData / pUnit come from the debug heap
    int debugMem;           // 0x70  1 = memTop is a debug heap allocation (dispAllBlock)
    int allDisp;            // 0x74  1 = every block displayed (t_option "BLOCK ALL DISP")
    void* saveMemTop;       // 0x78  memTop / memEnd while debugMem is set
    void* saveMemEnd;       // 0x7C

    cBlock() {}   // `Block` has (empty) static constructor and destructor functions
    ~cBlock() {}
    void setOtStart();
    u32* getOtAddr();
    void roomInit(void* data);
    int checkBlockMemory();
    void useDebugMemory(int on, u32 size);
    void dispAllBlock(int on);
    void check(int arg);
    s8 checkBlockArea(Vec* pos, int now);
    void checkBlockConnect(BlockConnect* c, BlockLink* link, u32* mram, u32* aram);
    void checkBlockConnect_sub(u8 blk, BlockLink* link, u32* set);
    cBlockUnit* getUnitPtr(u8 no);
    int getBlockWork(u8 no);
    void checkCommand();
    void checkCondition();
    void checkBlockMemSort();
    void* getBlockMemFree(u32 size);
    void dispDebugInfo();
};

extern cBlock Block;

#endif
