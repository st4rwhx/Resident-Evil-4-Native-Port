#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "light.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "em.h"
#include "emwindow.h"
#include "etc_model.h"
#include "esp.h"
#include "player.h"
#include "flr_at.h"
#include "rnd.h"

// Room 1-11 (D:/Bio4/Prog/r111.cpp): the village house exterior in the storm; the rack ranges, the
// rain effects and the thunder task.

struct R111Work {
    u8 pad[0x1C];
};

static R111Work* r111_work;

static void r111_ThunderMove();

void R111Init()
{
    cEm* rack;
    cEm* win;

#line 47 "D:/Bio4/Prog/r111.cpp"
    r111_work = (R111Work*) MEM_CALLOC(sizeof(R111Work), 1, 0xd);

    if (getRoomEtcRack(0xD, &rack, 1)) {
        ((cEmRack*) rack)->setRange(0.0f, 1000.0f, 0.0f, 2000.0f);
    }
    if (getRoomEtcRack(0xF, &rack, 1)) {
        ((cEmRack*) rack)->setRange(2000.0f, 2000.0f, 0.0f, 2600.0f);
    }
    if (getRoomEtcRack(0x11, &rack, 1)) {
        FSet(rack->pos.x, 6656.0f);
        FSet(rack->pos.y, 902.0f);
        FSet(rack->pos.z, 8925.0f);
        ((cEmRack*) rack)->setRange(2000.0f, 800.0f, 0.0f, 4400.0f);
    }
    SceExec(0x12, (TaskFunc) r111_ThunderMove, 0, 0, 2, 0);
    {
        void* zero = 0;

        EstSet((int) pPL, -1, 0, 0, 1, 0, 0x800, 0, (u32) zero, zero);
        EstSet((int) pPL, -1, 0, 0, 3, 1, 0x800, 0, (u32) zero, zero);
        EstSet((int) pPL, -1, 0, 0, 0, 0x23, 0x800, 0, (u32) zero, zero);
    }
    pG->flags_5010 |= 0x400;
    if (getRoomEtcWindow(0, &win, 1)) {
        ((cEmWindow*) win)->SetBreakModel();
    }
    if (getRoomEtcWindow(0x13, &win, 1)) {
        ((cEmWindow*) win)->SetBreakModel();
    }
    FlrAtSetDefVal(0, 0, 3);
}

void R111Main()
{
}

// Thunder every 90..235 frames: the flash (indoors: the window flash), the lit rain areas, the sound.
static void r111_ThunderMove()
{
    int cnt;
    void* zero = 0;

    SceSleep(1);
    {
        u8 r = Rnd() % 30;
        cnt = r * 5 + 90;
    }
    for (;;) {
        if (cnt == 0) {
            if (!(pG->flags_5010 & 0x02000000)) {
                EstSet(0, -1, 0, 0, 1, 2, 1, 0, 0, 0);
            } else {
                EstSet(0, -1, 0, 0, 1, 0x10, 1, 0, 0, 0);
            }
            if (EffGetAreaState(0)) {
                EstSet(0, -1, 0, 0, 1, 4, 0, 0, (u32) zero, zero);
            }
            if (EffGetAreaState(1)) {
                EstSet(0, -1, 0, 0, 1, 6, 0, 0, (u32) zero, zero);
            }
            if (EffGetAreaState(2)) {
                EstSet(0, -1, 0, 0, 1, 8, 0, 0, (u32) zero, zero);
            }
            if (EffGetAreaState(3)) {
                EstSet(0, -1, 0, 0, 1, 0xA, 0, 0, (u32) zero, zero);
            }
            if (EffGetAreaState(4)) {
                EstSet(0, -1, 0, 0, 1, 0xC, 0, 0, (u32) zero, zero);
            }
            if (EffGetAreaState(5)) {
                EstSet(0, -1, 0, 0, 1, 0xE, 0, 0, (u32) zero, zero);
            }
            {
                u8 r = Rnd() % 30;
                cnt = r * 5 + 90;
            }
            SceSndCallThunder();
        }
        cnt--;
        SceSleep(1);
    }
}
