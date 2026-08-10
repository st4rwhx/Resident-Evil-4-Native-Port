#include "types.h"
#include "db_log.h"

// Per-room constants: a count, a validity bitmap and the values.
// ConsInitCore is unused in this build (only its message survives in .rodata).
struct ConsRoom {
    u32 num;
    u32 bits[1];  // (num >> 5) + 1 words, followed by u32 values[num]
};

static u32 ConsRoomDefault[12] = {
    60, 200, 1024, 256, 10, 100, 1300, 300, 0xA0000, 10, 30, 30,
};

ConsRoom* pConsRoom;

static inline int ConsInitCore(void* p)
{
    if (p == 0) {
        pLog->err(0, 0, "ConsInitCore() INVALID PTR %08x", p);
        return 0;
    }
    return 1;
}

int ConsInitRoom(ConsRoom* p)
{
    pConsRoom = p;
    return 1;
}

u32 ConsGetRoomValue(u32 no)
{
    ConsRoom* r = pConsRoom;
    u32* bits;
    u32* values;

    if (r == 0) {
        return ConsRoomDefault[no];
    }
    bits = r->bits;
    values = &r->bits[(r->num >> 5) + 1];
    if (no >= r->num || !(bits[no >> 5] & (1 << (no & 31)))) {
        return ConsRoomDefault[no];
    }
    return values[no];
}
