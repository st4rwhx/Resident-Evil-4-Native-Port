#ifndef CARD_H
#define CARD_H

#include "types.h"
#include "db_log.h"
#include "cDataSwap.h"
#include "main_mem.h"

// Memory card save/load front end (game/card.cpp).

// Dolphin CARD SDK (the SDK header drags in the CodeWarrior libc).
struct CardFileInfo {
    s32 chan;    // 0x00
    s32 fileNo;  // 0x04
    s32 offset;  // 0x08
    s32 length;  // 0x0C
    u16 iBlock;  // 0x10
    u16 pad_12;
};

struct CardStat {
    char fileName[32];   // 0x00
    u32 length;          // 0x20
    u32 time;            // 0x24
    u8 gameName[4];      // 0x28
    u8 company[2];       // 0x2C
    u8 bannerFormat;     // 0x2E
    u8 pad_2F;
    u32 iconAddr;        // 0x30
    u16 iconFormat;      // 0x34
    u16 iconSpeed;       // 0x36
    u32 commentAddr;     // 0x38
    u32 offsetBanner;    // 0x3C
    u32 offsetBannerTlut;// 0x40
    u32 offsetIcon[8];   // 0x44
    u32 offsetIconTlut;  // 0x64
    u32 offsetData;      // 0x68
};                       // 0x6C

typedef void (*CardCallback)(s32 chan, s32 result);

extern "C" {
void CARDInit();
s32 CARDGetResultCode(s32 chan);
s32 CARDCheckAsync(s32 chan, CardCallback callback);
s32 CARDFreeBlocks(s32 chan, s32* byteNotUsed, s32* filesNotUsed);
s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size, CardFileInfo* fileInfo, CardCallback callback);
s32 CARDDeleteAsync(s32 chan, const char* fileName, CardCallback callback);
s32 CARDFormatAsync(s32 chan, CardCallback callback);
s32 CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize);
s32 CARDMountAsync(s32 chan, void* workArea, CardCallback detachCallback, CardCallback attachCallback);
s32 CARDUnmount(s32 chan);
s32 CARDGetSerialNo(s32 chan, u64* serialNo);
s32 CARDOpen(s32 chan, const char* fileName, CardFileInfo* fileInfo);
s32 CARDClose(CardFileInfo* fileInfo);
s32 CARDReadAsync(CardFileInfo* fileInfo, void* buf, s32 length, s32 offset, CardCallback callback);
s32 CARDWriteAsync(CardFileInfo* fileInfo, void* buf, s32 length, s32 offset, CardCallback callback);
s32 CARDGetStatus(s32 chan, s32 fileNo, CardStat* stat);
s32 CARDSetStatusAsync(s32 chan, s32 fileNo, CardStat* stat, CardCallback callback);
}

// One memory card slot (chan 0 = slot A, 1 = slot B, 2 = the host "HARD DISK" of the dev kit).
struct CardSlot {
    void* workArea;         // 0x00  CARDMountAsync work area (0xA000, slot A only)
    s32 chan;               // 0x04
    u32 flags;              // 0x08  error bits (2 no card, 4 no space, 0x10 broken, 0x20 wrong device,
                            //       0x40 fatal, 0x80 sector size, 0x100 save file seen, 0x200 system
                            //       file seen, 0x400/0x800 which file lacks space)
    s32 memSize;            // 0x0C  Mbit
    s32 sectorSize;         // 0x10
    s32 freeBytes;          // 0x14
    s32 freeFiles;          // 0x18
    u32 fileFlag[20];       // 0x1C  per save file: 1 exists, 2 corrupted, 4 wrong version
    u32 pad_6C;
    u64 serial;             // 0x70
    CardFileInfo fileInfo;  // 0x78
    CardStat stat;          // 0x8C
};                          // 0xF8

// Stream slot saved across the card screen.
struct CardStr {
    u32 id;      // 0x00
    s8 vol;      // 0x04
    u8 pad_5[3];
};

class cCard {
public:
    u32 useMemSize;      // 0x000  heap range parked in `swap`
    u8 mode;             // 0x004  state (row of the MainLoop table)
    u8 step;             // 0x005
    u8 sub;              // 0x006  async sub-step of the CARD helpers
    u8 sub2;             // 0x007
    u32 saveFlags170;    // 0x008  pG->flags_170 while the card screen runs
    u32 saveFlags58;     // 0x00C  pG->flags_58
    u32 flag;            // 0x010  bit 0: file written, bit 1: file list changed
    s32 type;            // 0x014  0 load, 1 save, 2 first check
    s32 isSystem;        // 0x018  saveMain: 1 = writing the system file
    u8 slot;             // 0x01C
    s8 fileNo;           // 0x01D  0..19
    u8 pad_1E;
    u8 retry;            // 0x01F
    s32 errCode;         // 0x020  CARD result / -0x2xx game error shown by errorDisp
    u8 pad_24[4];
    CardSlot slotw[3];   // 0x028
    u8* pSaveBuf;        // 0x310
    u32 saveBufSize;     // 0x314  0xEAFC
    u32 saveBlocks;      // 0x318  8
    u8* pInfoBuf;        // 0x31C  20 * 0x200 save headers
    u8* pInfo[20];       // 0x320
    u8* pSysBuf;         // 0x370
    u32 sysBufSize;      // 0x374  0x1E7C
    u32 sysBlocks;       // 0x378  1
    struct CardArc* pSubData;  // 0x37C  sub screen data archive (SndMem.sub_adr)
    void* pIdData;       // 0x380  ss/cmn/save_?.dat
    s32 timer;           // 0x384
    s32 bgmTimer;        // 0x388
    s32 result;          // 0x38C  last CARD result code
    CardStr str[4];      // 0x390
    u32 bgmStrId;        // 0x3B0
    s32 formatted;       // 0x3B4
    s32 exitFlag;        // 0x3B8
    s32 dispFlag;        // 0x3BC
    char fileName[0x40]; // 0x3C0
    cDataSwap swap;      // 0x400
    s32 scrWidth;        // 0x418
    u32 sysFlags;        // 0x41C
                         // 0x420

    cCard();
    ~cCard();
    void slotSelect();
    void inSlotCheck();
    void dataSelect();
    void loadMain();
    void makeSaveData();
    void makeSystemSaveData();
    void saveMain();
    void exit();
    void workDestroy();
    void format();
    void fileDelete();
    void errorDisp();
    void errorSet(int code);
    int initialize(int type);
    int initSub();
    int workAlloc();
    u32 getUseMemSize();
    int fileCreate(u8* sub, int blocks, CardSlot* s);
    void makeCardStatus(CardSlot* s);
    void firstCheck00();
    void firstCheck10();
    void firstCheck20();
    void firstCheck30();
    void MainLoop(int arg);
    int existCheck(int chan, CardSlot* s);
    int mount(u8* sub, CardSlot* s);
    int unmount(int chan);
    int verifyCheck(u8* sub, CardSlot* s);
    int freeCheck(u8* sub, CardSlot* s);
    int fileOpen(CardSlot* s);
    int fileClose(CardSlot* s);
    int saveFileCheck(u8* sub, CardSlot* s);
    int systemFileCheck(u8* sub, CardSlot* s);
    int fileRead(u8* sub, void* buf, s32 len, s32 ofs, CardSlot* s);
    int fileWrite(u8* sub, void* buf, int blocks, CardSlot* s);
    int sysfileRead(u8* sub, u8* sub2, int errMode);
    void createSysfile();
    void screenTrans();
    void cardMesSet(int no, int slot, u32 attr);
    void calcTplAddr(struct TEXPalette* tpl);
    void setMsgWindow(int a, int b);

#line 386 "D:/Bio4/Prog/card.h"
    void* operator new(unsigned int size) { return MEM_CALLOC(size, 1, 13); }
    void operator delete(void* p) { Mem_free(p); }
};

extern "C" {
void CardFirstCheck();
int CardCheckDone();
void CardSave(int a, int b);
int CardLoad();
void CardSysSave();
void CardInit();
void CardDbgCacheSet();
void CardMainTask(int arg);
}

#endif
