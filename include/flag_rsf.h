#ifndef FLAG_RSF_H
#define FLAG_RSF_H

#include "types.h"
#include "room_data.h"

// Room save flags (the original flag_rsf.h): bit `no` of the word after the room save record's
// header. Every unit that includes it carries the "HALT %s(%d)\n" / "D:/Bio4/Prog/flag_rsf.h"
// strings of the range checks (objRobo, sce_com, sce_at, every stage room); the rooms pass
// constant flag numbers, so the checks fold away (sce_at.cpp has the same bodies inline).
extern "C" void OSReport(const char* fmt, ...);

static inline u32* RsfFlags(u16 room)
{
    return (u32*) (RoomData.getRoomSavePtr(room) + 4);
}

// The stores are cast-then-deref MEMs (not MEM_IN_STRUCT_P): the rooms reload pG / their static
// work pointer after an RsfSet/RsfClear (r11d appearLittleSister, execEmAppear, checkEmDead), which
// only a store that may alias a fixed scalar produces.
// With a variable `no` (r104 EmReset) the +4 stays a separate `addi` before the indexed access:
// the word is formed from RsfFlags()' pointer, not from the record pointer.
static inline u32* RsfFlagWord(u16 room, int no)
{
    return (u32*) ((((u32) no >> 5) << 2) + (u32) RsfFlags(room));
}

static inline void RsfSet(u16 room, int no)
{
    if (no > 0x1F) {
#line 17 "D:/Bio4/Prog/flag_rsf.h"
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);
        *(volatile u32*) 0x11111111 = 0;
    }
    *RsfFlagWord(room, no) |= 0x80000000 >> (no & 31);
}

static inline void RsfClear(u16 room, int no)
{
    if (no > 0x1F) {
#line 21 "D:/Bio4/Prog/flag_rsf.h"
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);
        *(volatile u32*) 0x11111111 = 0;
    }
    *RsfFlagWord(room, no) &= ~(0x80000000 >> (no & 31));
}

// The masked word itself (objRobo R0Init tests it directly: `andis.; beq`; bit 0 folds to a sign
// test `cmpwi; bge`); `!= 0` would give the `li 1 / li 0 / cmpwi` flag chain.
static inline u32 RsfCheck(u16 room, int no)
{
    if (no > 0x1F) {
#line 25 "D:/Bio4/Prog/flag_rsf.h"
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);
        *(volatile u32*) 0x11111111 = 0;
    }
    return RsfFlags(room)[(u32) no >> 5] & (0x80000000 >> (no & 31));
}

#endif
