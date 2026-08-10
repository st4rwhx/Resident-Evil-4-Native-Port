#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "scroll.h"
#include "obj.h"
#include "snd.h"
#include "math_sub.h"
#include "cSceObj.h"

// Room 4-0C (D:/Bio4/Prog/r40c.cpp): the treasure chest whose lid swings open (cSceObj) before the
// generic OpenBoxMain runs.

struct R40cWork {
    u8 dummy;
};

static R40cWork* r40c_work;

void R40cOpenBoxMain(int type, int mode, int se, int id1, int id2, int itemNo, int seNo);
static void OpenBoxTreasure(int id);
static void OpenedBoxTreasure(int id);

void R40cInit()
{
#line 53 "D:/Bio4/Prog/r40c.cpp"
    r40c_work = (R40cWork*) MEM_CALLOC(sizeof(R40cWork), 1, 0xd);
    SceSetItemEvent(1, 0x80, 1, 3, OpenBoxTreasure, (void (*)()) OpenedBoxTreasure, 0x80, 1);
}

void R40cMain()
{
}

// Box types 0x13/0x14: the lid (id1) is parented to the box (id2) and rotated open over 40 frames.
void R40cOpenBoxMain(int type, int mode, int se, int id1, int id2, int itemNo, int seNo)
{
    cObj* lid = NULL;
    cObj* box = NULL;

    if (id1 != -1) {
        lid = SmdGetObjPtr(id1);
    }
    if (id2 != -1) {
        box = SmdGetObjPtr(id2);
    }
    if (lid) {
        lid->be_flag |= 0x20;
    }
    if (box) {
        box->be_flag |= 0x20;
    }
    switch (type) {
    case 0x13:
    case 0x14:
        if (lid && box) {
            Vec d;
            Vec rot = {0.0f, 0.0f, 0.0f};

            PSVECSubtract(&box->pos, &lid->pos, &d);
            box->setParent(lid, &d, &rot);
        }
        break;
    }
    if (mode == 0) {
        if (seNo != -1) {
            SndCall(6, seNo, 0, 0, 0, 0);
        }
        switch (type) {
        case 0x13:
        case 0x14:
            if (lid && box) {
                cSceObj mov;
                Vec ang = {0.0f, 0.0f, -PI};

                mov.initMove1_ang(box, 40, &ang, 20.0f, 20.0f, 4);
                while (mov.move() == 1) {
                    SceSleep(1);
                }
                SceSleep(15);
            }
            break;
        }
        OpenBoxMain(type, mode, se, id1, id2, itemNo);
    } else {
        OpenBoxMain(type, mode, se, id1, id2, itemNo);
    }
}

static void OpenedBoxTreasure(int id)
{
    if (id == 0x80) {
        R40cOpenBoxMain(0x13, 1, 10, 0x15, 0x14, -1, 9);
    }
}

static void OpenBoxTreasure(int id)
{
    if (id == 0x80) {
        R40cOpenBoxMain(0x13, 0, 10, 0x15, 0x14, -1, 9);
    }
}
