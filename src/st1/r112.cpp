#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "esp.h"
#include "rnd.h"

// Room 1-12 (D:/Bio4/Prog/r112.cpp): the farm in the storm; r102's BGM task and the thunder task.

struct R112Work {
    u8 pad[1];
};

static R112Work* r112_work;

void r102_checkBgm();
static void r112_ThunderMove();

void R112Init()
{
#line 37 "D:/Bio4/Prog/r112.cpp"
    r112_work = (R112Work*) MEM_CALLOC(sizeof(R112Work), 1, 0xd);

    Espgen42SetNoWater(1);
    SceExec(0x12, (TaskFunc) r112_ThunderMove, 0, 0, 2, 0);
    SceExec(0x12, (TaskFunc) r102_checkBgm, 0, 0, 2, 0);
}

void R112Main()
{
}

// Thunder every 90..235 frames while the storm flag is set.
static void r112_ThunderMove()
{
    int cnt;

    SceSleep(1);
    {
        u8 r = Rnd() % 30;
        cnt = r * 5 + 90;
    }
    for (;;) {
        if (cnt == 0) {
            if (pG->flags_5010 & 0x02000000) {
                EstSet(0, -1, 0, 0, 1, 3, 1, 0, 0, 0);
                SceSndCallThunder();
            }
            {
                u8 r = Rnd() % 30;
                cnt = r * 5 + 90;
            }
        }
        cnt--;
        SceSleep(1);
    }
}
