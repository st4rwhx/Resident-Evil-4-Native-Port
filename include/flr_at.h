#ifndef FLR_AT_H
#define FLR_AT_H

#include "types.h"
#include "vec.h"

// Floor attribute record returned by FlrAtCheck (game/flr_at.cpp), 0x84 bytes. The layout
// depends on the attribute type; only the bytes game/snd.cpp reads are named.
struct FlrAt {
    u8 x0;           // 0x00  bit0: active
    u8 type;         // 0x01  attribute type asked for in FlrAtCheck
    u8 x2;           // 0x02  (type 2) BGM control id
    u8 group;        // 0x03  group (FlrSys::group 0xFF = any)
    u8 pad_4[0x14 - 0x04];
    u8 area[0x30];   // 0x14  area passed to AreaHitCheck
    u8 x44;          // 0x44  (type 0) foot SE variation, (type 2) BGM slot bits / 0x10 stream
    u8 x45;          // 0x45  (type 0) foot effect, (type 2) set volume bits / 0x10 stream set
    u8 x46[2];       // 0x46  (type 0, weapon) SE offset, (type 2) BGM volume per slot; 0x47 also the type 0 flag mask
    s32 x48[2];      // 0x48  (type 2) BGM fade time per slot
    u16 str_blk;     // 0x50  (type 2) stream block
    u16 str_no;      // 0x52  (type 2) stream number
    s32 str_vol;     // 0x54  (type 2) stream volume
    u8 pad_58[0x84 - 0x58];
};

// "FSE" room file header (pG->pRoomArc), followed by the FlrAt records at 0x10.
struct FlrAtHead {
    char magic[4];   // 0x00  "FSE"
    u16 version;     // 0x04  0x103
    u16 num;         // 0x06  record count
    u8 pad_8[8];
};

// Floor system work (`pFlrSys` -> FlrAt_sys, 0x8C bytes).
struct FlrSys {
    void* pData;         // 0x00  room floor attribute data (NULL when the room has none)
    FlrAt* pList;        // 0x04  its records
    u8 group;            // 0x08  current group (0xFF = any)
    u8 foot_se[0x41];    // 0x09  foot SE variation per material (FlrAtSetDefVal a)
    u8 foot_esp[0x42];   // 0x4A  foot effect per material (FlrAtSetDefVal b)
};

extern FlrSys* pFlrSys;

FlrAt* FlrAtCheck(int type, Vec* pos, int flag);

extern "C" {
void FlrAtInit();
int FlrAtSetDefVal(u32 no, u8 a, u8 b);
}

#endif
