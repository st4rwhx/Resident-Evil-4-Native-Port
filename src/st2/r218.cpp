#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em10.h"
#include "emdoor.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "read.h"
#include "cam_ctrl.h"
#include "math_sub.h"
#include "snd.h"

// Room 2-18 (D:/Bio4/Prog/r218.cpp): the two bells and the chainsaw sisters dropping from the ceiling.

struct R218Work {
    cObjBell* bell[2];   // 0x00
    f32 y0;              // 0x08  rest height of the first cage
    f32 y1;              // 0x0C  rest height of the second cage
    u32 snd;             // 0x10  SndCall id of the cage sound
};

// The work pointer is a struct member: every store through the work reloads it.
struct R218WorkPtr {
    R218Work* p;
};

static R218WorkPtr r218_work;
// COMPILER-DIFF: candidate #12 (loop-exit form). After a `do { ..; if (c) break; SceSleep(1); } while (1)`
// our cse1 follows the exit branch AROUND the SceSleep block and reuses the loop's `lis work@ha`
// register and hoisted 2500.0 in the exit block; the original re-materialises both. The dead
// `do { } while (0);` after each poll loop ends cse1's path at the loop notes (the r117 idiom).

static void r218_checkEmSet();
static void r218_checkBellBreak();
static void r218_checkClawManDead_end();
static void r218_checkClawManDead();
static void r218_appearClawMan_end();
static void r218_appearClawMan();

// Death bit of entry `no` of the loaded enemy list (0 while no list is loaded).
static inline u32 r218_emDead(int no)
{
    int list = pG->emlist_no;
    u32 v;

    if (list >= 0) {
        v = *(u32*) ((list << 5) + (u32) pG + 0x501C) & (0x80000000 >> (no & 31));
    } else {
        v = 0;
    }
    return v;
}

void R218Init()
{
    Vec pos;
    Vec rot;
    cEm* d0;
    cEm* d1;

#line 37 "D:/Bio4/Prog/r218.cpp"
    r218_work.p = (R218Work*) MEM_CALLOC(sizeof(R218Work), 1, 0xd);
    if (getRoomEtcDoor(3, &d0, 1) && getRoomEtcDoor(4, &d1, 1)) {
        ((cEmDoor*) d0)->setDoor((cEmDoor*) d1);
    }
    SceAtSetEnable(2, 0);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        EmReadSearch(0x1C, 0, 0);
        SceExec(0x12, (TaskFunc) r218_appearClawMan, 0, 0, 2, 0);
    } else {
        cObj* o28 = SmdGetObjPtr(0x28);
        cObj* o29 = SmdGetObjPtr(0x29);

        if (o28) {
            o28->be_flag &= ~2;
        }
        if (o29) {
            o29->be_flag &= ~2;
        }
    }
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceExec(0x12, (TaskFunc) r218_checkEmSet, 0, 0, 2, 0);
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        pos.x = -13650.0f;
        pos.y = 1060.0f;
        pos.z = 4367.0f;
        rot.x = 0.0f;
        rot.y = PI;
        rot.z = 0.0f;
        r218_work.p->bell[0] = (cObjBell*) SetObjBell(ROOM_ARC_PTR(pG->pRoomArc, 0x20), ROOM_ARC_PTR(pG->pRoomArc, 0x21), &pos, &rot);
    }
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        pos.x = -5352.0f;
        pos.y = 2131.0f;
        pos.z = 4378.0f;
        rot.x = 0.0f;
        rot.y = PI;
        rot.z = 0.0f;
        r218_work.p->bell[1] = (cObjBell*) SetObjBell(ROOM_ARC_PTR(pG->pRoomArc, 0x20), ROOM_ARC_PTR(pG->pRoomArc, 0x21), &pos, &rot);
    }
    SceExec(0x12, (TaskFunc) r218_checkBellBreak, 0, 0, 2, 0);
}

void R218Main()
{
}

// Once both sisters are dead and the player reaches area 4, three more Ganados come.
static void r218_checkEmSet()
{
    while (1) {
        if (r218_emDead(3)) {
            if (r218_emDead(4)) {
                break;
            }
        }
        SceSleep(1);
    }
    while (SceAtHitCheck(4) == 0) {
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 3);
    setEm(0x11, -1, 1, 1, 1);
    setEm(0x12, -1, 1, 1, 1);
    setEm(0x13, -1, 1, 1, 1);
}

static void r218_checkBellBreak()
{
    RsfCheck(G_ROOM_ID, 1);
    RsfCheck(G_ROOM_ID, 2);
    while (1) {
        if (RsfCheck(G_ROOM_ID, 1) && RsfCheck(G_ROOM_ID, 2)) {
            break;
        }
        if (RsfCheck(G_ROOM_ID, 1) == 0 && r218_work.p->bell[0] && r218_work.p->bell[0]->ckBreak() == 1) {
            RsfSet(G_ROOM_ID, 1);
        }
        if (RsfCheck(G_ROOM_ID, 2) == 0 && r218_work.p->bell[1] && r218_work.p->bell[1]->ckBreak() == 1) {
            RsfSet(G_ROOM_ID, 2);
        }
        SceSleep(1);
    }
}

static void r218_checkClawManDead_end()
{
    cObj* o29;
    cObj* o28;

    o28 = SmdGetObjPtr(0x28);
    o29 = SmdGetObjPtr(0x29);

    if ((int) pG->flags_174 < 0) {
        if (r218_work.p->snd) {
            SndStop(r218_work.p->snd, 0);
        }
        if (o29) {
            o29->pos.y = r218_work.p->y1 + 2500.0f;
        }
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SndRoomStrStop(3);
    SndCall(6, 4, 0, 0, 0, 0);
    f32 spd = 40.0f;
    if (o28) {
        do {
            o28->pos.y += spd;
            if (o28->pos.y >= r218_work.p->y0 + 2500.0f) {
                break;
            }
            SceSleep(1);
        } while (1);
        do { } while (0);   // COMPILER-DIFF: candidate #12 (loop-exit form)
        o28->pos.y = r218_work.p->y0 + 2500.0f;
        SndCall(6, 5, 0, 0, 0, 0);
    }
}

static void r218_checkClawManDead()
{
    cEmWrap em0;
    cEmWrap em1;
    cObj* o29;
    u32 snd;

    em0.setPtr(0, -1, 1);
    em1.setPtr(1, -1, 1);
    o29 = SmdGetObjPtr(0x29);
    while (em0.isActive() == 1 || em1.isActive() == 1) {
        SceSleep(1);
    }
    RsfSet(G_ROOM_ID, 0);
    pG->door_flags_51C8 |= 0x20;
    SceAtSetEnable(0, 1);
    SceAtSetEnable(2, 0);
    snd = 0;
    r218_work.p->snd = snd;
    SceSetEventCancel(1, (TaskFunc) r218_checkClawManDead_end, 0, 0, 1);
    f32 spd = 40.0f;
    SceEventStart(1);
    CamCtrl.CutCall(1);
    r218_work.p->snd = SndCall(6, 0, 0, 0, 0, 0);
    if (o29) {
        do {
            o29->pos.y += spd;
            if (o29->pos.y >= r218_work.p->y1 + 2500.0f) {
                break;
            }
            SceSleep(1);
        } while (1);
        do { } while (0);   // COMPILER-DIFF: candidate #12 (loop-exit form)
        o29->pos.y = r218_work.p->y1 + 2500.0f;
        r218_work.p->snd = 0;
        SndCall(6, 1, 0, 0, 0, 0);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r218_checkClawManDead_end();
}

static void r218_appearClawMan_end()
{
    if ((int) pG->flags_174 < 0) {
        cObj* o28;
        cObj* o29;

        if (r218_work.p->snd) {
            SndStop(r218_work.p->snd, 0);
        }
        o28 = SmdGetObjPtr(0x28);
        o29 = SmdGetObjPtr(0x29);
        if (o28) {
            o28->pos.y = r218_work.p->y0;
        }
        if (o29) {
            o29->pos.y = r218_work.p->y1;
        }
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExec(0x12, (TaskFunc) r218_checkClawManDead, 0, 0, 2, 0);
    pG->door_flags_51C8 &= ~0x20;
}

// The two cages drop when the player enters area 3.
static void r218_appearClawMan()
{
    cEmWrap em0;
    cEmWrap em1;
    cObj* o28;
    cObj* o29;
    f32 spd;

    em0.setEm(0, -1, 1, 1, 1);
    em1.setEm(1, -1, 1, 1, 1);
    o28 = SmdGetObjPtr(0x28);
    o29 = SmdGetObjPtr(0x29);
    if (o28) {
        o28->be_flag |= 0x20;
        o28->setNoSuspend(1);
        r218_work.p->y0 = o28->pos.y;
        o28->pos.y += 2500.0f;
    }
    if (o29) {
        o29->be_flag |= 0x20;
        o29->setNoSuspend(1);
        r218_work.p->y1 = o29->pos.y;
        o29->pos.y += 2500.0f;
    }
    SndRoomStrStart(1, 0, 1);
    SceSleep(1);
    while (SceAtHitCheck(3) == 1) {
        if (em0.isActive() == 0 && em1.isActive() == 0) {
            RsfSet(G_ROOM_ID, 0);
            SndRoomStrStop(3);
            SceExit();
        }
        SceSleep(1);
    }
    SceAtSetEnable(0, 0);
    SceAtSetEnable(2, 1);
    r218_work.p->snd = 0;
    SceSetEventCancel(1, (TaskFunc) r218_appearClawMan_end, 0, 0, 1);
    SceEventStart(1);
    CamCtrl.CutCall(1);
    r218_work.p->snd = SndCall(6, 2, 0, 0, 0, 0);
    spd = 0.0f;
    if (o29) {
        do {
            spd += 18.0f;
            o29->pos.y -= spd;
            if (o29->pos.y < r218_work.p->y1) {
                break;
            }
            SceSleep(1);
        } while (1);
        do { } while (0);   // COMPILER-DIFF: candidate #12 (loop-exit form)
        r218_work.p->snd = 0;
        SndCall(6, 3, 0, 0, 0, 0);
        o29->pos.y = r218_work.p->y1;
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    CamCtrl.CutCall(2);
    r218_work.p->snd = SndCall(6, 6, 0, 0, 0, 0);
    spd = 0.0f;
    if (o28) {
        do {
            spd += 18.0f;
            o28->pos.y -= spd;
            if (o28->pos.y < r218_work.p->y0) {
                break;
            }
            SceSleep(1);
        } while (1);
        do { } while (0);   // COMPILER-DIFF: candidate #12 (loop-exit form)
        r218_work.p->snd = 0;
        SndCall(6, 7, 0, 0, 0, 0);
        o28->pos.y = r218_work.p->y0;
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r218_appearClawMan_end();
}
