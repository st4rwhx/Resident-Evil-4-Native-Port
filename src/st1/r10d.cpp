#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "flag_rsf.h"

// Room 1-0d (D:/Bio4/Prog/r10d.cpp): nothing but the room work allocation.

struct R10dWork {
    u8 pad[1];
};

static R10dWork* r10d_work;

void R10dInit()
{
#line 26 "D:/Bio4/Prog/r10d.cpp"
    r10d_work = (R10dWork*) MEM_CALLOC(1, 1, 0xd);
}

void R10dMain()
{
}
