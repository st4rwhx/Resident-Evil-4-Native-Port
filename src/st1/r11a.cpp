#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "flag_rsf.h"
#include "player.h"
#include "esp.h"

// Room 1-1a (D:/Bio4/Prog/r11a.cpp): the lake shore; player water effects and the hit effect table.

struct R11aWork {
    u8 pad[1];
};

static R11aWork* r11a_work;

// Hit effects of attribute type 2 (water)
static const AtEffInfo r11a_eff_info = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};

void R11aInit()
{
    void* zero = 0;

#line 46 "D:/Bio4/Prog/r11a.cpp"
    r11a_work = (R11aWork*) MEM_CALLOC(1, 1, 0xd);

    EstSet((int) pPL, -1, 0, 0, 3, 2, 0, 0, (u32) pPL, zero);
    EstSet((int) pPL, -1, 0, 0, 1, 0, 0, 0, (u32) pPL, zero);
    EatMgr.registEffInfo(2, (AtEffInfo*) &r11a_eff_info);
}

void R11aMain()
{
    setPlWaterOtType();
}
