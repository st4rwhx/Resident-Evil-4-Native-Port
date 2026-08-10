#ifndef ROOM_DATA_H
#define ROOM_DATA_H

#include "types.h"

// Per-room save data and room DLL control (game/roomdata.cpp).
struct OSModuleHeader;

// One room of a stage table (St<n>_data_tbl), 0xC bytes.
struct RoomTblEntry {
    u8 stat;         // 0x00  1 = the room has a save record
    u8 pad_1;
    u16 rel_no;      // 0x02  FileTbl index of the room DLL (0 = none)
    void (*init)();  // 0x04
    void (*main)();  // 0x08
};

// Room_data_tbl[10]: one row per stage.
struct StageTbl {
    RoomTblEntry* tbl;  // 0x00
    u16 num;            // 0x04
    u16 pad_6;
};

// Save buffer header, followed by num records of 0xD8 bytes.
struct RoomSaveHdr {
    u32 size;  // 0x00  total bytes including this header
    u32 num;   // 0x04
    u8 pad_8[8];
};

// One room save record (0xD8 bytes): stage, room, passed bits, then the room's own data.
struct RoomSave {
    union {
        u16 id;      // 0x00  stage << 8 | room
        struct {
            u8 stage;  // 0x00
            u8 room;   // 0x01
        };
    };
    u8 passed;  // 0x02  bit (0x80 >> n): checkPassed/setPassed
    u8 data[0xD8 - 3];
};

class cRoomData {
public:
    u16 total;                // 0x00  rooms in all stage tables
    u16 num;                  // 0x02  rooms with a save record
    u16 flag;                 // 0x04  bit 0: room DLL unlinked (stopRelData)
    u8 pad_6[2];
    OSModuleHeader* pModule;  // 0x08  linked room DLL (exception.cpp loads its symbols)
    void* pBss;               // 0x0C  DLL bss
    void* pBssBak;            // 0x10  bss copy kept while the DLL is unlinked
    RoomSaveHdr* pSaveBuf;    // 0x14
    u8* pSave;                // 0x18  room save records, 0xD8 bytes each
    u16 x1C;                  // 0x1C  cleared before linkRelData (stage.cpp); FileTbl index of the room dll
    u16 x1E;

    cRoomData() { flag = 0; }
    ~cRoomData() {}  // the empty destructor is what makes GCC emit the static destructor function

    void init();
    void initRoomSet();
    void save(void* dst);
    void load(void* src);
    void clear(void* src);
    // record for room `room` (stage << 8 | room_no), or NULL when the room has none
    u8* getRoomSavePtr(u16 room);
    void execInitFunc(u16 room);
    void execMainFunc(u16 room);
    int checkRoomRange(u8 stage, u8 no);
    int checkRelRead(u16 room);
    void linkRelData(u16 room);
    void stopRelData();
    void restartRelData();
    int checkPassed(u16 room, int bit);
    void setPassed(u16 room, int bit);
};

extern cRoomData RoomData;

#endif
