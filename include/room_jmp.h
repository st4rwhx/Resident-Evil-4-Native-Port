#ifndef ROOM_JMP_H
#define ROOM_JMP_H

#include "types.h"

// game/room_jmp.cpp: debug "AREA JUMP" tool over the room info table (roomInfoAddr).
//
// Table: u32 stage count, u32 offset per stage (0 = no rooms), each stage block starts with a u32
// room count followed by CRoomInfo[count]. The string offsets are relative to the table until
// cRoomJmp's constructor turns them into pointers.

struct CRoomInfo {
    u16 flags;      // 0x00  bit 0: pos/angle valid
    union {
        u16 room_id;  // 0x02  stage << 8 | room
        struct {
            u8 stage;  // 0x02
            u8 room;   // 0x03
        };
    };
    Vec pos;        // 0x04
    f32 angle;      // 0x10
    char* name;     // 0x14
    char* scr;      // 0x18
    char* soft;     // 0x1C

    void setNextPos();
};

class cRoomJmp {
public:
    u32* tbl;  // 0x00

    cRoomJmp(void* tbl);
    s8 getIndexNum(s8 stage);
    s8 getPointNum(s8 stage, s8 room);
    CRoomInfo* getRoomInfo(u8 stage, u8 idx);
    u8 getRoomIdx(u8 stage, u8 room);
    void setNextPos(u8 stage, u8 room);
    s8 getNextStageNo(s8 stage, int dir);
    s8 getNextRoomNo(s8 stage, s8 idx, int dir);
    s8 getNextPointNo(s8 stage, s8 room, s8 point, int dir);
    int checkRoomNo(s8 stage, int idx);
};

// room_jmp.cpp and title.cpp each own a file-scope `cRoomJmp* pRj` of their own.

void RoomJump();
extern "C" void GetNextPos(u8 stage, u8 room);

#endif
