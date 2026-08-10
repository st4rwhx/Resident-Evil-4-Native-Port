#ifndef EM_SET_H
#define EM_SET_H

#include "types.h"
#include "em.h"

// One entry of the room enemy list (ESL, pG->emlist: 256 entries of 0x20 bytes, game/em_set.cpp).
struct EmListData {
    u8 flags;       // 0x00  bit0: alive flag (EmListSetAlive), bit1: set (an enemy was created from it), bit2/bit3: set toggles
    u8 id;          // 0x01  enemy id (0 = empty entry, 0xF / 0x25 are created at the back of the work array)
    u8 type;        // 0x02  -> cModel::type
    u8 x3;          // 0x03  -> cEm::x38D
    u32 flags4;     // 0x04  -> cEm::flags_3C8
    u16 hp;         // 0x08
    u8 pad_A;
    u8 xB;          // 0x0B  -> cEm::x3D0
    s16 pos[3];     // 0x0C  * 10
    s16 rot[3];     // 0x12  * (pi / 0x4000)
    u16 room;       // 0x18  stage << 8 | room
    s16 x1A;        // 0x1A  * 1000 -> cEm::x3CC
    u8 pad_1C[4];
};

#define EM_LIST(no) ((EmListData*) &pG->emlist[(no) * 0x20])

extern cEm* errEm;   // returned by EmSetFromList2 when no enemy was created

extern "C" {
int checkListId(int no);                    // 0 when an alive enemy already carries list entry `no`
void EmSetFromList();                       // create every enemy of the current room from the list
cEm* EmSetFromList2(int no, int chkDead);   // create list entry `no`; errEm on failure
cEm* GetEmPtrFromList(int no);              // alive enemy created from list entry `no`
EmListData* GetListPtrFromEm(cEm* em);
u8 GetEmIdFromList(u32 no);
void EmListSetAlive(int no, int on);
void EmSetDie(cEm* em);                     // remember the death of `em` in pG->em_dead
void EmSetDieCnt();
void EmSetRoomInit();                       // clear the "set" bit of every entry
void EmListWaitDelete();
}

// Creates an enemy from a list record built by the caller (C++ linkage; sce_at, the stage rooms).
cEm* EmSetEvent(EmListData* d);

#endif
