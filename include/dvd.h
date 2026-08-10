#ifndef DVD_H
#define DVD_H

#include "types.h"

// Dolphin DVD / ARQ types straight from the SDK headers (the SDK's dolphin/types.h drags in
// the CodeWarrior libc, so its guard is defined here, like include/snd_sdk.h does).
#define _DOLPHIN_TYPES_H_
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#include <dolphin/dvd.h>
#include <dolphin/ar.h>

// One entry of the file header read in front of a multi-part file (header_buff, 64 entries).
struct DvdHeader {
    u32 type;    // 0x00  0 MRAM, 1 snd ARAM, 2 snd MRAM, 3 MRAM (explicit dest), 4 nested
                 //       header, -2 skip, -1 end of table
    u32 size;    // 0x04
    u32 dest;    // 0x08  explicit destination (0 = next free MRAM/ARAM address)
    u32 ofs;     // 0x0C  offset inside the file
    u32 sndType; // 0x10  sound block (0..8), 8 = enemy (index from SndEmDataReadCheck)
    u32 sndArg;  // 0x14
    u32 sndNo;   // 0x18
    u32 x1C;
};

// Read request parameters handed to cDvd::ReadReq by DvdRead/DvdReadN (`DvdReqWork`, 0x6C).
struct DvdReq {
    void* dst;       // 0x00
    u32 aram;        // 0x04  ARAM destination
    u16 fileNo;      // 0x08  FileTbl index, 0xFFFF = by name
    u16 prio;        // 0x0A  (4)
    u16 mode;        // 0x0C  bit0 sync, bit1 debug heap, bit2 main heap, bit3 type 3 header,
                     //       bit5 0x10, bit6 keep, bit8 interrupt task, bit15 headered file
    u8 pad_E[2];
    u32 ofs;         // 0x10
    u32 length;      // 0x14  0 = whole file
    char name[0x20]; // 0x18
    char file[0x30]; // 0x38  __FILE__ of the caller
    int line;        // 0x68
};

// Result block filled by cDvd::readCheckMain for a finished request (0x208 bytes).
struct DvdReadInfo {
    u32 addr[2][32];  // 0x000  destination of every part, per header level
    u32 size[2][32];  // 0x100
    u32 mramSize;     // 0x200
    u32 aramSize;     // 0x204
};

// One read queue slot (16 in cDvd, 0x310 bytes each).
class cDvdQueue {
public:
    volatile u32 flag;   // 0x00  bit0 in use, bit1 main heap, bit2 debug heap, bit4 (0x10),
                         //       bit5 reading, bit8 interrupt task, bit11 done,
                         //       bits16-18 status (0 PUSH 1 READ 2 COMPLETE 3 CANCEL 4 ERROR),
                         //       0x100000 cancelled, 0x200000 read error, 0x400000 exit,
                         //       0x01000000 ARAM DMA busy, 0x02000000 DVD read busy,
                         //       0x04000000 all parts done, 0x20000000 keep slot,
                         //       0x40000000 synchronous, 0x80000000 headered file
    s8 mode;             // 0x04  func_tbl index (0 init, 1 main, 2 cancel wait, 3 exit)
    s8 step;             // 0x05
    u8 pad_6[2];
    DVDFileInfo fileInfo;  // 0x08
    u8 pad_44[4];
    s32 entrynum;        // 0x48
    u16 fileNo;          // 0x4C
    u8 prio;             // 0x4E
    u8 no;               // 0x4F  request number returned to the caller
    cDvdQueue* next;     // 0x50
    void* pBuff;         // 0x54  destination (allocated when 0)
    u32 aram;            // 0x58
    u32 mramAddr;        // 0x5C  next free MRAM address
    u32 aramAddr;        // 0x60  next free ARAM address
    u32 destAddr;        // 0x64  destination of the current part
    u32 length;          // 0x68
    s32 remain;          // 0x6C
    u32 readSize;        // 0x70
    u32 ofs;             // 0x74
    u32 hedOfs[2];       // 0x78  file offset of the header per level
    u32 tick;            // 0x80
    ARQRequest arq;      // 0x84
    u8 hedType;          // 0xA4  type given to the fake header of a plain file (0 / 3)
    u8 depth;            // 0xA5  header nesting level
    u8 heap;             // 0xA6
    s8 err;              // 0xA7
    char name[0x20];     // 0xA8
    u32 addrTbl[2][32];  // 0xC8
    u32 sizeTbl[2][32];  // 0x1C8
    u32 mramSize;        // 0x2C8
    u32 aramSize;        // 0x2CC
    s32 pcMode;          // 0x2D0  pG->flags_54 & 0x20000 at Initialize
    u16 cnt[2];          // 0x2D4  parts done per level
    char file[0x30];     // 0x2D8
    int line;            // 0x308
    u32 startTick;       // 0x30C

    int chk(u32 bit) { return (flag & bit) ? 1 : 0; }
    void setStatus(int s) {
        flag &= ~0x70000;
        flag |= s << 16;
    }
    int getStatus() { return (flag >> 16) & 7; }
    char* getName() { return name; }

    void trans2mram(void* buf, u32 addr, u32 size);
    void trans2aram(void* buf, u32 addr, u32 size);
    void readInit();
    void readMain();
    void readCancelWait();
    void readExit();
    int Read();
    void Initialize();
    int LinkQueue();
    void PushQueue();
    void ErrMemFree();
    int fileOpen();
    int fileGetLength();
    int fileReadAsync(void* buf, u32 size, u32 ofs);
    int fileClose();
};

// One ARAM DMA request (16 in cAram, 0x18 bytes).
struct AramReq {
    volatile u32 flag;  // 0x00  bit0 in use, 0x04000000 done
    u32 type;           // 0x04  ARQ_TYPE_MRAM_TO_ARAM / ARAM_TO_MRAM
    u32 src;            // 0x08
    u32 dst;            // 0x0C
    u32 len;            // 0x10
    AramReq* next;      // 0x14

    int chk(u32 bit) { return (flag & bit) ? 1 : 0; }
    void clear() { flag = 0; }
};

// ARAM DMA queue (`Aram`, 0x1A8 bytes).
class cAram {
public:
    AramReq* pCur;       // 0x00
    AramReq* pList;      // 0x04
    ARQRequest arq;      // 0x08
    AramReq queue[16];   // 0x28

    int DmaTransReq(int type, u32 src, u32 dst, u32 len, int wait);
    AramReq* pullAramQueue(int* no);
    void DmaTrans(AramReq* req, int wait);
    int TransCheck(int no);
    int DmaCancel(int no);
    void DmaCancelAll();
};

// DVD read queue (game/dvd.cpp, `Dvd`, 0x311C bytes).
class cDvd {
public:
    u32 freeSize;         // 0x00    memory above the FST
    cDvdQueue* pRead;     // 0x04    request being read
    cDvdQueue* pList;     // 0x08    pending requests sorted by prio
    cDvdQueue queue[16];  // 0x0C
    void* pSizeTbl;       // 0x310C
    s32 driveStatus;      // 0x3110
    u8 reqNo;             // 0x3114  next request number (never 0)
    u8 pad_3115[3];
    s32 discChanged;      // 0x3118

    void Init();
    void SizeTableRead();
    void ReadProc();
    void readProcMain(cDvdQueue* q);
    void ReadNblk2Blk(int no);
    void FileTblExistCheck();
    int FileExistCheck(const char* name, u32* pLength);
    int ReadReq();
    void blockRead(cDvdQueue* q);
    void Watcher();
    int ReadCancel(int no, int mode);
    void ReadCancelAll();
    cDvdQueue* pullReadQueue();
    // Polls request `req`. Returns 1 when done and then stores the result word, the size and
    // the destination address through the non-NULL pointers; < 0 on failure.
    int ReadCheck(int req, int* result, int* size, void** addr);
    int ReadCheck(int req);
    // read.cpp calls ReadCheck(int) with a DvdReadInfo* as a third argument (r5): the callee
    // passes its uninitialised `info` pointer straight to readCheckMain, so the info block gets
    // filled. This declaration is that entry point with the arguments the callers really pass.
    int ReadCheckInfo(int req, DvdReadInfo* info) asm("ReadCheck__4cDvdi");
    int readCheckMain(int req, DvdReadInfo* info);
    cDvdQueue* getQueuePtr(u8 no);
    int ErrCheck(int disc, int flag);
    int DiscChange(int disc);
    int GetDiscNo();
    void queueStatusDisp();
    void DiscReadInfo();
};

extern DvdReq DvdReqWork;
extern cDvd Dvd;
extern cAram Aram;

// File name table (game/dvd.cpp); entrynum -1 = not on the disc.
struct FileTblEntry {
    const char* name;
    s32 entrynum;
};
extern FileTblEntry FileTbl[];

extern "C" {
int DvdRead(int fileNo, void* dst, u32 aram, u32 ofs, u32 length, int mode, const char* file, int line);
// Queue a file read; returns the request number. `mode` 3 = allocate the destination.
int DvdReadN(const char* name, void* dst, int a, int b, int c, int mode, const char* file, int line);
void MemorySwap(void* mram, u32 aram, u32 size);
void DvdReadProc();
void MesSysMessage(int msg, int disc);
void RomFontPrint(int x, int y, const char* str);
void RomFontMessage(u32 msg, int disc);
void RomFontSetting();
}

#endif
