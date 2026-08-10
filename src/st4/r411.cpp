#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "em.h"
#include "emdoor.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "cam_ctrl.h"

// Room 4-11 (D:/Bio4/Prog/r411.cpp): the locked door and the three Ganado waves.

struct R411Work {
    u8 dummy;
};

static R411Work* r411_work;

// The original passes an uninitialised int to cEmDoor::setCloseLock(int) (no r4 setup, r105 idiom).
void cEmDoorSetCloseLock(cEm* door) asm("setCloseLock__7cEmDoori");

static void r411_checkDoorUnlock();
extern "C" void r411_lockDoor();
static void r411_checkEmSet1_end();
static void r411_checkEmSet1();
static void r411_checkEmSet2();
static void r411_checkEmSet3();

void R411Init()
{
#line 33 "D:/Bio4/Prog/r411.cpp"
    r411_work = (R411Work*) MEM_CALLOC(sizeof(R411Work), 1, 0xd);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        r411_lockDoor();
        SceAtDataSet_exec(0xF, 0x12, 0, (TaskFunc) r411_checkEmSet1, 0, 1);
    } else if (RsfCheck(G_ROOM_ID, 3) == 0) {
        r411_lockDoor();
        SceExec(0x12, (TaskFunc) r411_checkDoorUnlock, 0, 0, 2, 0);
    } else {
        SceAtSetEnable(0xF, 0);
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(2, 0x12, 0, (TaskFunc) r411_checkEmSet2, 0, 1);
    }
}

void R411Main()
{
}

// The door unlocks once the first wave's leader is gone.
static void r411_checkDoorUnlock()
{
    cEm* door;

    SceSleep(1);
    cEmWrap em;
    em.setEm(0xE1, -1, 0, 1, 1);
    while (em.isActive() != 0) {
        SceSleep(1);
    }
    RsfClear(G_ROOM_ID, 3);
    SceAtSetEnable(0xF, 0);
    if (getRoomEtcDoor(1, &door, 1)) {
        ((cEmDoor*) door)->setNormal();
    }
    SceExec(0x12, (TaskFunc) r411_checkEmSet3, 0, 0, 2, 0);
}

extern "C" void r411_lockDoor()
{
    cEm* door;

    if (getRoomEtcDoor(1, &door, 1)) {
        cEmDoorSetCloseLock(door);
    }
    SceAtSetEnable(0xF, 1);
}

static void r411_checkEmSet1_end()
{
    cEmWrap em0;
    cEmWrap em1;
    cEmWrap em2;
    cEmWrap em3;
    cEmWrap em4;

    em0.setEm(0xE1, -1, 0, 1, 1);
    em1.setEm(0xE2, -1, 0, 1, 1);
    em2.setEm(0xE3, -1, 0, 1, 1);
    em3.setEm(0xE4, -1, 0, 1, 1);
    em4.setEm(0xE5, -1, 0, 1, 1);
    em0.setNoSuspend(0);
    em1.setNoSuspend(0);
    em2.setNoSuspend(0);
    em3.setNoSuspend(0);
    em4.setNoSuspend(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Area 0xF: the first wave walks in to the areas 0xA..0xE (camera cut 2).
static void r411_checkEmSet1()
{
    SceUpCut(0, -1, 0, 0);
    RsfSet(G_ROOM_ID, 0);
    cEmWrap em0;
    cEmWrap em1;
    cEmWrap em2;
    cEmWrap em3;
    cEmWrap em4;
    Vec pos;
    em0.setEm(0xE1, -1, 0, 1, 1);
    em1.setEm(0xE2, -1, 0, 1, 1);
    em2.setEm(0xE3, -1, 0, 1, 1);
    em3.setEm(0xE4, -1, 0, 1, 1);
    em4.setEm(0xE5, -1, 0, 1, 1);
    SceAtDataReset(0xF);
    SceExec(0x12, (TaskFunc) r411_checkDoorUnlock, 0, 0, 2, 0);
    SceEventStart(0);
    em0.setNoSuspend(1);
    em1.setNoSuspend(1);
    em2.setNoSuspend(1);
    em3.setNoSuspend(1);
    em4.setNoSuspend(1);
    SceAtGetCenterPos(&pos, 0xB);
    em1.setGoto(&pos, 1);
    SceAtGetCenterPos(&pos, 0xC);
    em2.setGoto(&pos, 1);
    SceAtGetCenterPos(&pos, 0xD);
    em3.setGoto(&pos, 1);
    SceAtGetCenterPos(&pos, 0xE);
    em4.setGoto(&pos, 1);
    SceAtGetCenterPos(&pos, 0xA);
    em0.setGoto(&pos, 1);
    SceSetEventCancel(1, (TaskFunc) r411_checkEmSet1_end, 0, -1, 1);
    CamCtrl.CutCall(2);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r411_checkEmSet1_end();
}

// Area 2: two more Ganado head for the player while the first wave is still large.
static void r411_checkEmSet2()
{
    RsfSet(G_ROOM_ID, 1);
    if ((u32) SceCountEmAlive(GetEmIdFromList(0xE1), -1) <= 10) {
        cEmWrap em0;
        cEmWrap em1;

        em0.setEm(0xD6, -1, 0, 1, 1);
        em1.setEm(0xD7, -1, 0, 1, 1);
        em0.setGoto(&pPL->pos, 0xB);
        em1.setGoto(&pPL->pos, 0xB);
    }
}

// The last two Ganado once the first wave is down to 9.
static void r411_checkEmSet3()
{
    int id;

    for (;;) {
        if (RsfCheck(G_ROOM_ID, 0) == 0) {
            SceSleep(1);
        } else {
            break;
        }
    }
    SceSleep(15);
    id = GetEmIdFromList(0xE1);
    while ((u32) SceCountEmAlive(id, -1) > 9) {
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 2);
    setEm(0xD9, -1, 0, 1, 1);
    setEm(0xDA, -1, 0, 1, 1);
}
