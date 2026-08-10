#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "game.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "obj13.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "emdoor.h"
#include "etc_model.h"
#include "player.h"
#include "cam_ctrl.h"
#include "mercenaries.h"
#include "snd.h"
#include "db_log.h"

extern "C" void* memset(void* dst, int c, unsigned int n);

// Room 4-02 (D:/Bio4/Prog/r402.cpp): the Mercenaries waterworld; the enemy waves per area (sce_at
// flags 0x17C), the sliding doors, the two boat events and the treasure cases.

struct R402Work {
    cEmWrap em[0x70];    // 0x000
    cSat* sat[2];        // 0x540  the door 02 collision pieces
    cSat* eat[2];        // 0x548
    int c550;            // 0x550  per-wave respawn counts (R402EmSetSubMugen)
    int c554;
    int c558;
    int c55C;
    int c560;
    int c564;
    int c568;
    int c56C;
    int c570;
    int c574;
    int c578;
    int c57C;
    int c580;
    int c584;            // 0x584  frames since event 03-00 (event 03-01 after 150)
    int timer;           // 0x588  em_destroy every 300 frames
};

// One-member struct: every store through the work reloads the pointer.
struct R402WorkPtr {
    R402Work* p;
};

// Typed view of pG->emlist (r400): pG is loaded before the index shift.
struct EmListView {
    u8 pad[0x52E8];
    EmListData emlist[0x100];
};
#define EM_LIST_V(no) (((EmListView*) pG)->emlist[(no)])

static R402WorkPtr r402_work;


Vec r402_gotoPos0 = {14140.0f, 14313.0f, -55300.0f};
Vec r402_gotoPos1 = {14080.0f, 14313.0f, -51760.0f};
Vec r402_gotoPos2 = {14400.0f, 14313.0f, -53225.0f};
u32 r402_boatObj0[2] = {0x42, 0x43};
u32 r402_boatObj1[2] = {0x44, 0x45};
f32 r402_boatZ0[2] = {-66909.0f, -60250.0f};
f32 r402_boatZ1[2] = {-64639.0f, -62520.0f};
int r402_boatNum = 2;

// The room's MercSysInitRoom parameters are 0x6C bytes (the DOL reads the first 0x5C).
struct R402MercInit {
    MercInit m;
    u32 x5C[4];
};

static void setLadderMotion(int no);
static void OpenBoxTreasure(int no);
static void OpenedBoxTreasure(int no);
static void R402ExecEvent01Main();
static void R402ExecEvent01End();
static void R402ExecEvent02Main();
void R402InitDoor02();
static void R402MoveDoor02(int dir);
static void R402ExecEvent03Main00();
static void R402ExecEvent03End00();
static void R402ExecEvent03Main01();
static void R402ExecEvent03End01();
static void em_destroy();
int R402EmSetSubMugen(int no, int list, int* cnt, int max, int findPl);
void R402EmSetSub(int no, int list, int findPl);
static void R402EmSetMain();
int R402CalcActiveEmWarp();

void R402Init()
{
    cEm* door0;
    cEm* door1;
    cEm* ladder;

    R402Work*& wp = r402_work.p;
#line 45 "D:/Bio4/Prog/r402.cpp"
    wp = (R402Work*) MEM_CALLOC(sizeof(R402Work), 1, 0xd);
    SceSetItemEvent(0x14, 0x80, 2, -1, OpenBoxTreasure, (void (*)()) OpenedBoxTreasure, 0x80, 0);
    SceSetItemEvent(0x15, 0x81, 3, -1, OpenBoxTreasure, (void (*)()) OpenedBoxTreasure, 0x81, 0);
    SceSetItemEvent(0x16, 0x82, 4, -1, OpenBoxTreasure, (void (*)()) OpenedBoxTreasure, 0x82, 0);
    if (getRoomEtcDoor(0x18, &door0, 1) && getRoomEtcDoor(0x19, &door1, 1)) {
        ((cEmDoor*) door0)->setDoor((cEmDoor*) door1);
    }
    R402EmSetSub(0x27, 0xA0, 0);
    R402EmSetSub(0x28, 0xA1, 0);
    R402EmSetSub(0x29, 0xA2, 1);
    R402EmSetSub(0x2A, 0xA3, 1);
    R402EmSetSub(0x2B, 0xA4, 1);
    R402EmSetSub(0x2C, 0xA5, 1);
    R402EmSetSub(0x2D, 0xA6, 1);
    R402EmSetSub(0x2E, 0xA7, 1);
    SceExec(0x12, (TaskFunc) R402EmSetMain, 0, 0, 2, 0);
    R402InitDoor02();
    SceAtDataSet_exec(0x10, 0x12, 0, (TaskFunc) R402ExecEvent02Main, 0, 1);
    SceAtDataSet_exec(0xF, 0x12, 0, (TaskFunc) R402ExecEvent01Main, 0, 1);
    SceAtSetEnable(0x11, 0);
    if (getRoomEtcLadder(0x15, &ladder, 1)) {
        ((cObjLadder*) ladder)->setOff();
    }
    if (getRoomEtcLadder(0x1E, &ladder, 1)) {
        ((cObjLadder*) ladder)->setOff();
    }
    setLadderMotion(0x15);
    setLadderMotion(0x1E);
    {
        R402MercInit init;

        memset(&init, 0, sizeof(init));
        init.m.pos.x = 21035.0f;
        init.m.pos.y = 3065.0f;
        init.m.pos.z = -26370.0f;
        init.m.rot.x = 0.0f;
        init.m.rot.y = -1.53f;
        init.m.rot.z = 0.0f;
        init.m.x18 = 0;
        init.m.smdMot = ROOM_ARC_PTR(pG->pRoomArc, 0x26);
        init.m.x20 = 30000;
        init.m.mesStart = 2;
        init.m.mesA8 = 0xD;
        init.m.mesAC = 0xE;
        init.m.x58 = 0xF;
        init.m.mes[0] = 3;
        init.m.mes[1] = 4;
        init.m.mes[2] = 5;
        init.m.mes[3] = 6;
        init.m.mes[4] = 7;
        init.m.mes[5] = 8;
        init.m.mes[6] = 9;
        init.m.mes[7] = 0xA;
        init.m.mes[8] = 0xB;
        init.m.mes[9] = 0xC;
        MercSysInitRoom(&init.m);
    }
}

void R402Main()
{
    SceDebugDisp("");
    SceDebugDisp("");
    SceDebugDisp("");
    SceDebugDisp("");
    SceDebugDisp("");
    r402_work.p->timer++;
    if (r402_work.p->timer > 300) {
        r402_work.p->timer = 1;
        em_destroy();
    }
}

// The ladder motions of the Ada game (her own climb set from the etc archive).
static void setLadderMotion(int no)
{
    cEm* ladder;
    void* das;

    if (getRoomEtcLadder(no, &ladder, 1)) {
        if (pG->x4FB8 == 2) {
            if (EtcGetDasAddr(6, &das)) {
                void* mot[20];

                mot[0] = ROOM_ARC_PTR(pG->pRoomArc, 0x1F);
                mot[1] = ROOM_ARC_PTR(pG->pRoomArc, 0x20);
                mot[2] = ROOM_ARC_PTR(pG->pRoomArc, 0x21);
                mot[3] = ROOM_ARC_PTR(pG->pRoomArc, 0x22);
                mot[4] = ROOM_ARC_PTR(pG->pRoomArc, 0x23);
                mot[5] = ROOM_ARC_PTR(pG->pRoomArc, 0x24);
                mot[6] = GetEtcAddr(das, "et06000.fcv");
                mot[7] = GetEtcAddr(das, "et06001.fcv");
                mot[8] = GetEtcAddr(das, "et06002.fcv");
                // struct view: the pG load stays below the mot[8] frame store (the target issues
                // the das reload for mot[10] first); a plain pG read is hoisted above it
                mot[9] = ROOM_ARC_PTR(pGS->pRoomArc, 0x25);
                mot[10] = GetEtcAddr(das, "et06003.fcv");
                mot[11] = GetEtcAddr(das, "et060000.seq");
                mot[12] = GetEtcAddr(das, "et060010.seq");
                mot[13] = GetEtcAddr(das, "et060020.seq");
                mot[14] = GetEtcAddr(das, "et060030.seq");
                mot[15] = GetEtcAddr(das, "et060031.seq");
                mot[16] = GetEtcAddr(das, "pl01106.fcv");
                mot[17] = GetEtcAddr(das, "pl01107.fcv");
                mot[18] = GetEtcAddr(das, "pl01108.fcv");
                mot[19] = GetEtcAddr(das, "pl01118.fcv");
                ((cObjLadder*) ladder)->setMotion(mot);
            }
        }
    }
}

// The three treasure cases (item events 0x14..0x16, arg = the item id).
static void OpenedBoxTreasure(int no)
{
    if (no == 0x80) {
        OpenBoxMain(9, 1, 0x5B, 0x3D, -1, -1);
    }
    if (no == 0x81) {
        OpenBoxMain(9, 1, 0x5B, 0x3E, -1, -1);
    }
    if (no == 0x82) {
        OpenBoxMain(9, 1, 0x5B, 0x3C, -1, -1);
    }
}

static void OpenBoxTreasure(int no)
{
    if (no == 0x80) {
        OpenBoxMain(9, 0, 0x5B, 0x3D, -1, -1);
    }
    if (no == 0x81) {
        OpenBoxMain(9, 0, 0x5B, 0x3E, -1, -1);
    }
    if (no == 0x82) {
        OpenBoxMain(9, 0, 0x5B, 0x3C, -1, -1);
    }
}

// Area 15: door 02 opens with the camera on it and three enemies walk in.
static void R402ExecEvent01Main()
{
    cEm* ladder;
    int i;

    if (pG->flags_174 & 0x80000000) {
        return;
    }
    pG->flags_174 |= 0x80000000;
    SceAtSetEnable(0xF, 0);
    pG->flags_174 |= 0x1000;
    R402EmSetSub(0x5A, 0xE4, 1);
    R402EmSetSub(0x5B, 0xE5, 1);
    R402EmSetSub(0x5C, 0xE6, 1);
    R402EmSetSub(0x39, 0xB4, 1);
    R402EmSetSub(0x3A, 0xB5, 1);
    r402_work.p->em[0x5A].setNoSuspend(1);
    r402_work.p->em[0x5B].setNoSuspend(1);
    r402_work.p->em[0x5C].setNoSuspend(1);
    if (getRoomEtcLadder(0x15, &ladder, 1)) {
        ((cObjLadder*) ladder)->setOn();
        ((cObjLadder*) ladder)->setDowned();
    }
    if (getRoomEtcLadder(0x1E, &ladder, 1)) {
        ((cObjLadder*) ladder)->setOn();
        ((cObjLadder*) ladder)->setDowned();
    }
    SceEventStart(1);
    SceSetEventCancel(1, (TaskFunc) R402ExecEvent01End, 0, -1, 1);
    SceExec(0x12, (TaskFunc) R402MoveDoor02, 1, 2, 2, 0);
    CamCtrl.CutCall(0xB);
    for (i = 0; i < 20; i++) {
        SceSleep(1);
    }
    r402_work.p->em[0x5A].setGoto(&r402_gotoPos0, 2);
    r402_work.p->em[0x5B].setGoto(&r402_gotoPos1, 1);
    r402_work.p->em[0x5C].setGoto(&r402_gotoPos2, 1);
    for (i = 0; i < 60; i++) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    R402ExecEvent01End();
}

static void R402ExecEvent01End()
{
    SceExec(0x12, (TaskFunc) R402MoveDoor02, 0, 2, 2, 0);
    r402_work.p->em[0x5A].setPos(&r402_gotoPos0);
    r402_work.p->em[0x5B].setPos(&r402_gotoPos1);
    r402_work.p->em[0x5C].setPos(&r402_gotoPos2);
    r402_work.p->em[0x5A].setNoSuspend(0);
    r402_work.p->em[0x5B].setNoSuspend(0);
    r402_work.p->em[0x5C].setNoSuspend(0);
    r402_work.p->em[0x39].setNoSuspend(0);
    r402_work.p->em[0x3A].setNoSuspend(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// Area 16: the wave behind door 02 (when few enemies are about) and the door closes.
static void R402ExecEvent02Main()
{
    if (!(pG->flags_174 & 0x40000000)) {
        pG->flags_174 |= 0x40000000;
        SceAtSetEnable(0x10, 0);
        if (!(pG->flags_174 & 0x400)) {
            if (R402CalcActiveEmWarp() <= 4) {
                pG->flags_174 |= 0x400;
                R402EmSetSub(0x63, 0xF0, 1);
                R402EmSetSub(0x64, 0xF1, 1);
                R402EmSetSub(0x65, 0xF2, 1);
                R402EmSetSub(0x66, 0xF3, 1);
                R402EmSetSub(0x67, 0xF4, 1);
                R402EmSetSub(0x68, 0xF5, 1);
                R402EmSetSub(0x69, 0xF6, 1);
            }
        }
        R402MoveDoor02(1);
    }
}

// The collision pieces of the two door 02 halves (objects 0x40 / 0x41).
// COMPILER-DIFF: #1 (the original issues `fmr f1,h` before the `li 0x40; li 0x100` argument moves)
cSat* SatMgrCreateF(cSatMgr* m, Vec* pos, Vec* rot, Vec* poly, f32 h, int attr, int flag) asm("create__7cSatMgrP3VecN21iif");

void R402InitDoor02()
{
    Vec poly[4] = {{0.0f, -2495.0f, -200.0f}, {1630.0f, -2495.0f, -200.0f}, {1630.0f, -2495.0f, 200.0f}, {0.0f, -2495.0f, 200.0f}};
    u32 id[2] = {0x40, 0x41};
    f32 h = 5000.0f;
    int i;

    for (i = 0; i < 2; i++) {
        cObj* obj = SmdGetObjPtr(id[i]);

        if (obj) {
            r402_work.p->sat[i] = SatMgrCreateF(&SatMgr, &obj->pos, &obj->rot, poly, h, 0x40, 0x100);
            r402_work.p->eat[i] = SatMgrCreateF(&EatMgr, &obj->pos, &obj->rot, poly, h, 0x40, 0x100);
        }
    }
}

// Door 02 slides (dir 1: open) over 40 frames, the collision pieces following.
static void R402MoveDoor02(int dir)
{
    u32 id[2] = {0x40, 0x41};
    f32 x0[2] = {17209.0f, 10668.0f};
    f32 x1[2] = {15559.0f, 12318.0f};
    Vec snd = {14000.0f, 16000.0f, -56000.0f};
    int frames = 40;
    int t;
    int i;

    SndCall(6, 0, &snd, 0, 0, 0);
    t = 0;
    // The goto loop keeps `lfd 2^52`/`cmpwi cr4,dir` in the outer body (no loop notes, no loop.c
    // hoist); the dead `do {} while (0)` around it only supplies LOOP_BEG/END notes so flow weights
    // the body's register refs at depth 2 (the target's r22/r23/r24 assignment of 40 / t+1 / &id).
    // The r31 (frame pointer) clobber in the eat arm makes `&id` non-transparent for gcse's block LCM,
    // so the occurrence stays in the inner body and loop.c pass 2 hoists it (`addi r24,r1,0x28` after
    // `li r31,0`) instead of PRE inserting it before `top:`; it replaces the flow nop after the call.
top:
    do {
        for (i = 0; i < 2; i++) {
            cObj* obj = SmdGetObjPtr(id[i]);

            if (obj) {
                f32 x;
                Vec v;

                if (dir == 1) {
                    x = (x0[i] - x1[i]) * (f32) t / (f32) frames + x1[i];
                } else {
                    x = (x1[i] - x0[i]) * (f32) t / (f32) frames + x0[i];
                }
                f32 y = obj->pos.y;
                f32 z = obj->pos.z;

                v.x = x;
                v.y = y;
                v.z = z;
                obj->setPos(&v);
                if (r402_work.p->sat[i]) {
                    r402_work.p->sat[i]->setCoord(&obj->pos, &obj->rot);
                }
                if (r402_work.p->eat[i]) {
                    r402_work.p->eat[i]->setCoord(&obj->pos, &obj->rot);
                    asm("" : "=r"(obj) : "0"(obj) : "r31"); // COMPILER-DIFF: 3
                }
            }
        }
        SceSleep(1);
        t++;
        if (t <= 40) {
            goto top;
        }
    } while (0);
}

// Area 18 / 19: a boat (objects 0x42+0x43 / 0x44+0x45) drifts in over 30 frames with the camera on it.
static void R402ExecEvent03Main00()
{
    cObj* obj;
    int frames = 30;
    int t;
    int i;
    int j;

    pG->flags_174 |= 0x200;
    R402EmSetSub(0x2F, 0xA9, 1);
    SceEventStart(1);
    SceSetEventCancel(1, (TaskFunc) R402ExecEvent03End00, 0, -1, 1);
    SceAtSetEnable(0x12, 0);
    CamCtrl.CutCall(0xC);
    obj = SmdGetObjPtr(r402_boatObj0[0]);
    if (obj) {
        SndCall(6, 1, &obj->pos, 0, 0, 0);
    }
    r402_work.p->em[0x2F].setNoSuspend(1);
    for (t = 0; t <= 30; t++) {
        for (i = 0; i < r402_boatNum; i++) {
            obj = SmdGetObjPtr(r402_boatObj0[i]);
            if (obj) {
                f32 z = (r402_boatZ0[i] - r402_boatZ1[i]) * (f32) t / (f32) frames + r402_boatZ1[i];
                f32 x = obj->pos.x;
                f32 y = obj->pos.y;
                Vec v;

                v.x = x;
                v.y = y;
                v.z = z;
                obj->setPos(&v);
            }
        }
        SceSleep(1);
    }
    for (j = 0; j < 5; j++) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    R402ExecEvent03End00();
}

static void R402ExecEvent03End00()
{
    int i;

    SceAtSetEnable(0x12, 0);
    for (i = 0; i < r402_boatNum; i++) {
        cObj* obj = SmdGetObjPtr(r402_boatObj0[i]);

        if (obj) {
            f32 x = obj->pos.z;
            f32 y = obj->pos.y;
            f32 z = r402_boatZ0[i];
            Vec v;

            v.x = x;
            v.y = y;
            v.z = z;
            obj->setPos(&v);
        }
    }
    r402_work.p->em[0x2F].setNoSuspend(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

static void R402ExecEvent03Main01()
{
    cObj* obj;
    int frames = 30;
    int t;
    int i;
    int j;

    pG->flags_174 |= 0x100;
    R402EmSetSub(0x30, 0xAA, 1);
    SceEventStart(1);
    SceSetEventCancel(1, (TaskFunc) R402ExecEvent03End01, 0, -1, 1);
    SceAtSetEnable(0x13, 0);
    CamCtrl.CutCall(0xD);
    obj = SmdGetObjPtr(r402_boatObj1[0]);
    if (obj) {
        SndCall(6, 1, &obj->pos, 0, 0, 0);
    }
    r402_work.p->em[0x30].setNoSuspend(1);
    for (t = 0; t <= 30; t++) {
        for (i = 0; i < r402_boatNum; i++) {
            obj = SmdGetObjPtr(r402_boatObj1[i]);
            if (obj) {
                f32 z = (r402_boatZ0[i] - r402_boatZ1[i]) * (f32) t / (f32) frames + r402_boatZ1[i];
                f32 x = obj->pos.x;
                f32 y = obj->pos.y;
                Vec v;

                v.x = x;
                v.y = y;
                v.z = z;
                obj->setPos(&v);
            }
        }
        SceSleep(1);
    }
    for (j = 0; j < 5; j++) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    R402ExecEvent03End01();
}

static void R402ExecEvent03End01()
{
    int i;

    SceAtSetEnable(0x13, 0);
    for (i = 0; i < r402_boatNum; i++) {
        cObj* obj = SmdGetObjPtr(r402_boatObj1[i]);

        if (obj) {
            f32 x = obj->pos.z;
            f32 y = obj->pos.y;
            f32 z = r402_boatZ0[i];
            Vec v;

            v.x = x;
            v.y = y;
            v.z = z;
            obj->setPos(&v);
        }
    }
    r402_work.p->em[0x30].setNoSuspend(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// Every 300 frames: the alive but inactive (out of range) enemies are removed.
static void em_destroy()
{
    int i;

    for (i = 0; i < 0x70; i++) {
        if (r402_work.p->em[i].isAlive() == 1 && r402_work.p->em[i].isActive() == 0) {
            r402_work.p->em[i].destroy();
        }
    }
}

// Respawning slot: sets list entry `list` into slot `no` while fewer than `max` were set; 1 when set.
int R402EmSetSubMugen(int no, int list, int* cnt, int max, int findPl)
{
    if (*cnt < max) {
        if (r402_work.p->em[no].isAlive() == 0) {
            R402EmSetSub(no, list, findPl);
            (*cnt)++;
            return 1;
        }
    }
    return 0;
}

// Creates list entry `list` into slot `no`; findPl 1: walk to the player, else search for him.
void R402EmSetSub(int no, int list, int findPl)
{
    cEm* em = EmSetEvent(&EM_LIST_V(list));

    if (em == errEm) {
        pLog->err(0, 0, "R402EmSetSub : EmSetEvent error");
    } else {
        r402_work.p->em[no].setPtr(em, 1);
        r402_work.p->em[no].setFlag(1);
        if (findPl == 1) {
            r402_work.p->em[no].setGoto(&pPL->pos, 0xC);
        } else {
            r402_work.p->em[no].setFindPL();
        }
    }
}

// The wave task: every frame, the waves of the areas Leon has touched (pG->sceat_x17C bits) while the
// active count allows it.
static void R402EmSetMain()
{
    int n = 0;

    r402_work.p->c550 = 0;
    r402_work.p->c554 = 0;
    r402_work.p->c558 = 0;
    r402_work.p->c55C = 0;
    r402_work.p->c560 = 0;
    r402_work.p->c564 = 0;
    r402_work.p->c568 = 0;
    r402_work.p->c584 = 0;
    r402_work.p->c56C = 0;
    r402_work.p->c570 = 0;
    r402_work.p->c574 = 0;
    r402_work.p->c578 = 0;
    r402_work.p->c57C = 0;
    r402_work.p->c580 = 0;
    SceSleep(1);
    for (;;) {
        n = R402CalcActiveEmWarp();
        SceDebugDisp("LiveNum:[%d]", n);
        if (n <= 8) {
            if (!(pG->flags_178 & 0x20000)) {
                if (r402_work.p->em[0x27].isActive() == 0 && r402_work.p->em[0x28].isActive() == 0) {
                    pG->flags_178 |= 0x20000;
                }
            } else {
                SceDebugDisp("DeadCount:[%d]", r402_work.p->c578);
                Vec p0 = {11320.0f, 12295.0f, -46200.0f};
                Vec p1 = {9240.0f, 12295.0f, -46200.0f};
                if (R402EmSetSubMugen(0x31, 0xAB, &r402_work.p->c578, 5, 1)) {
                    r402_work.p->em[0x31].setGoto(&p0, 0xC);
                }
                if (R402EmSetSubMugen(0x32, 0xAC, &r402_work.p->c578, 5, 1)) {
                    r402_work.p->em[0x32].setGoto(&p1, 0xC);
                }
            }
        }
        if ((pG->sceat_x17C & 0x100000) && n <= 6) {
            if (!(pG->flags_174 & 0x80)) {
                pG->flags_174 |= 0x80;
                R402EmSetSub(0x6A, 0xFA, 1);
                R402EmSetSub(0x6B, 0xF8, 1);
            } else if (!(pG->flags_174 & 0x40)) {
                pG->flags_174 |= 0x40;
                R402EmSetSub(0x6C, 0xFB, 1);
                R402EmSetSub(0x6D, 0xFC, 1);
            } else if (!(pG->flags_174 & 0x20)) {
                pG->flags_174 |= 0x20;
                R402EmSetSub(0x6E, 0xFD, 1);
            } else if (!(pG->flags_174 & 0x10)) {
                pG->flags_174 |= 0x10;
                R402EmSetSub(0x6F, 0xF9, 1);
            }
        }
        if ((pG->sceat_x17C & 0x80000) && n <= 6) {
            if (!(pG->flags_174 & 8)) {
                pG->flags_174 |= 8;
                R402EmSetSub(0x1A, 0x93, 0);
            } else if (!(pG->flags_174 & 4)) {
                pG->flags_174 |= 4;
                R402EmSetSub(0x1B, 0x94, 1);
            } else if (!(pG->flags_174 & 2)) {
                pG->flags_174 |= 2;
                R402EmSetSub(0x1C, 0x97, 1);
            } else if ((pG->flags_174 & 1) == 0) {
                pG->flags_174 |= 1;
                R402EmSetSub(0x1D, 0x9B, 1);
            } else if (!(pG->flags_178 & 0x80000000)) {
                pG->flags_178 |= 0x80000000;
                R402EmSetSub(0x1E, 0x9C, 1);
            } else if (!(pG->flags_178 & 0x40000000)) {
                pG->flags_178 |= 0x40000000;
                R402EmSetSub(0x1F, 0x95, 1);
            } else if (!(pG->flags_178 & 0x20000000)) {
                pG->flags_178 |= 0x20000000;
                R402EmSetSub(0x20, 0x96, 1);
            } else if (!(pG->flags_178 & 0x10000000)) {
                pG->flags_178 |= 0x10000000;
                R402EmSetSub(0x21, 0x98, 1);
            } else if (!(pG->flags_178 & 0x8000000)) {
                pG->flags_178 |= 0x8000000;
                R402EmSetSub(0x22, 0x99, 1);
            } else if (!(pG->flags_178 & 0x4000000)) {
                pG->flags_178 |= 0x4000000;
                R402EmSetSub(0x23, 0x9D, 1);
            } else if (!(pG->flags_178 & 0x2000000)) {
                pG->flags_178 |= 0x2000000;
                R402EmSetSub(0x24, 0x9E, 1);
            } else if (!(pG->flags_178 & 0x1000000)) {
                pG->flags_178 |= 0x1000000;
                R402EmSetSub(0x25, 0x9A, 1);
            } else if (!(pG->flags_178 & 0x800000)) {
                pG->flags_178 |= 0x800000;
                R402EmSetSub(0x26, 0x9F, 1);
            } else {
                SceDebugDisp("DeadCount:[%d]", r402_work.p->c564);
                R402EmSetSubMugen(0xF, 0x85, &r402_work.p->c564, 10, 1);
                R402EmSetSubMugen(0x10, 0x86, &r402_work.p->c564, 10, 1);
                R402EmSetSubMugen(0x11, 0x87, &r402_work.p->c564, 10, 1);
                R402EmSetSubMugen(0x12, 0x88, &r402_work.p->c564, 10, 1);
            }
        }
        if ((pG->sceat_x17C & 0x40000) && n <= 6) {
            SceDebugDisp("DeadCount:[%d]", r402_work.p->c568);
            R402EmSetSubMugen(0xC, 0x81, &r402_work.p->c568, 20, 1);
            R402EmSetSubMugen(0xD, 0x82, &r402_work.p->c568, 20, 1);
            R402EmSetSubMugen(0xE, 0x83, &r402_work.p->c568, 20, 1);
        }
        if ((pG->sceat_x17C & 0x80000000) && n <= 8) {
            if (!(pG->flags_174 & 0x800000)) {
                pG->flags_174 |= 0x800000;
                R402EmSetSub(0x4D, 0xD5, 0);
                R402EmSetSub(0x4E, 0xD6, 0);
            }
        }
        u32 at = pG->sceat_x17C;   // a user variable stops thread_jumps from folding this test into the previous one
        if ((at & 0x80000000) && n <= 7) {
            if (!(pG->flags_174 & 0x4000000)) {
                pG->flags_174 |= 0x4000000;
                R402EmSetSub(0x4B, 0xD2, 1);
                R402EmSetSub(0x48, 0xCE, 1);
                R402EmSetSub(0x49, 0xCF, 1);
            } else if (!(pG->flags_174 & 0x2000000)) {
                pG->flags_174 |= 0x2000000;
                R402EmSetSub(0x4C, 0xD3, 1);
                R402EmSetSub(0x4A, 0xDA, 1);
            } else {
                SceDebugDisp("DeadCount:[%d]", r402_work.p->c554);
                R402EmSetSubMugen(0x15, 0x8C, &r402_work.p->c554, 30, 1);
                R402EmSetSubMugen(0x16, 0x8D, &r402_work.p->c554, 30, 1);
                R402EmSetSubMugen(0x17, 0x8E, &r402_work.p->c554, 30, 1);
            }
        }
        if ((pG->sceat_x17C & 0x40000000) && n <= 7) {
            if (!(pG->flags_174 & 0x400000)) {
                pG->flags_174 |= 0x400000;
                R402EmSetSub(0x43, 0xC8, 0);
                R402EmSetSub(0x44, 0xC9, 0);
                R402EmSetSub(0x45, 0xCA, 1);
            } else if (!(pG->flags_174 & 0x200000)) {
                pG->flags_174 |= 0x200000;
                R402EmSetSub(0x46, 0xCB, 1);
                R402EmSetSub(0x47, 0xCC, 1);
            } else {
                SceDebugDisp("DeadCount:[%d]", r402_work.p->c558);
                R402EmSetSubMugen(0x61, 0xED, &r402_work.p->c558, 18, 1);
                R402EmSetSubMugen(0x62, 0xEE, &r402_work.p->c558, 18, 1);
            }
        }
        if ((pG->sceat_x17C & 0x2000000) && n <= 8) {
            if (!(pG->flags_174 & 0x80000)) {
                pG->flags_174 |= 0x80000;
                R402EmSetSub(0x51, 0xDA, 1);
                R402EmSetSub(0x52, 0xDB, 1);
            }
        }
        if ((pG->sceat_x17C & 0x20000000) && n <= 6) {
            if (!(pG->flags_174 & 0x40000)) {
                pG->flags_174 |= 0x40000;
                R402EmSetSub(0x4F, 0xD8, 1);
                R402EmSetSub(0x50, 0xD9, 1);
            } else {
                SceDebugDisp("DeadCount:[%d]", r402_work.p->c55C);
                R402EmSetSubMugen(0x18, 0x90, &r402_work.p->c55C, 25, 1);
                R402EmSetSubMugen(0x19, 0x91, &r402_work.p->c55C, 25, 1);
                R402EmSetSubMugen(0x13, 0x8A, &r402_work.p->c55C, 25, 1);
                R402EmSetSubMugen(0x14, 0x8B, &r402_work.p->c55C, 25, 1);
            }
        }
        if ((pG->sceat_x17C & 0x800000) && n <= 8) {
            if (!(pG->flags_174 & 0x8000)) {
                cEm* door;

                getRoomEtcDoor(0x1B, &door, 1);
                if (door->flags_3C8 & 0x10000000) {
                    pG->flags_174 |= 0x8000;
                    R402EmSetSub(0x53, 0xDC, 1);
                    R402EmSetSub(0x54, 0xDD, 1);
                }
            }
        }
        if ((pG->sceat_x17C & 0x10000000) && n <= 8) {
            SceDebugDisp("DeadCount:[%d]", r402_work.p->c560);
            R402EmSetSubMugen(0x58, 0xE1, &r402_work.p->c560, 10, 1);
            R402EmSetSubMugen(0x59, 0xE2, &r402_work.p->c560, 10, 1);
        }
        if ((pG->sceat_x17C & 0x8000000) && n <= 5) {
            SceDebugDisp("DeadCount:[%d]", r402_work.p->c550);
            R402EmSetSubMugen(0x33, 0xAD, &r402_work.p->c550, 45, 1);
            R402EmSetSubMugen(0x34, 0xAE, &r402_work.p->c550, 45, 0);
            R402EmSetSubMugen(0x35, 0xAF, &r402_work.p->c550, 45, 1);
            R402EmSetSubMugen(0x36, 0xB0, &r402_work.p->c550, 45, 1);
            R402EmSetSubMugen(0x37, 0xB1, &r402_work.p->c550, 45, 1);
        }
        if ((pG->sceat_x17C & 0x8000) && n <= 9) {
            if (!(pG->flags_178 & 0x10000) && MercSysWk.kill > 24) {
                pG->flags_178 |= 0x10000;
                R402EmSetSub(0x38, 0xB3, 1);
            }
        }
        if (pG->sceat_x17C & 0x4000000) {
            if (n <= 6) {
                if (!(pG->flags_174 & 0x200)) {
                    if (MercSysWk.kill > 19) {
                        if (r402_work.p->em[0x38].isActive() != 1 || !(pG->flags_178 & 0x10000)) {
                            pG->flags_174 |= 0x200;
                            SceExec(0x12, (TaskFunc) R402ExecEvent03Main00, 0, 0, 2, 0);
                        }
                    }
                } else if (!(pG->flags_174 & 0x100)) {
                    if (r402_work.p->c584 > 149) {
                        if (MercSysWk.kill > 89) {
                            pG->flags_174 |= 0x100;
                            SceExec(0x12, (TaskFunc) R402ExecEvent03Main01, 0, 0, 2, 0);
                        }
                    } else {
                        r402_work.p->c584++;
                    }
                }
            }
            if (n <= 6) {
                SceDebugDisp("DeadCount:[%d]", r402_work.p->c574);
                R402EmSetSubMugen(0, 0x78, &r402_work.p->c574, 30, 1);
                R402EmSetSubMugen(1, 0x79, &r402_work.p->c574, 30, 1);
                R402EmSetSubMugen(2, 0x7A, &r402_work.p->c574, 30, 1);
                R402EmSetSubMugen(3, 0x7B, &r402_work.p->c574, 30, 1);
            }
        }
        if ((pG->sceat_x17C & 0x20000) && n <= 6) {
            if (!(pG->flags_174 & 0x40000000)) {
                SceDebugDisp("DeadCount:[%d]", r402_work.p->c56C);
                R402EmSetSubMugen(4, 0xB7, &r402_work.p->c56C, 20, 1);
                R402EmSetSubMugen(5, 0xB8, &r402_work.p->c56C, 20, 1);
                R402EmSetSubMugen(6, 0xB9, &r402_work.p->c56C, 20, 1);
                R402EmSetSubMugen(7, 0xBA, &r402_work.p->c56C, 20, 1);
            } else {
                SceDebugDisp("DeadCount:[%d]", r402_work.p->c570);
                R402EmSetSubMugen(8, 0x7C, &r402_work.p->c570, 25, 1);
                R402EmSetSubMugen(9, 0x7D, &r402_work.p->c570, 25, 1);
                R402EmSetSubMugen(0xA, 0x7E, &r402_work.p->c570, 25, 1);
                R402EmSetSubMugen(0xB, 0x7F, &r402_work.p->c570, 25, 1);
            }
        }
        if ((pG->sceat_x17C & 0x10000) && n <= 7) {
            SceDebugDisp("DeadCount:[%d]", r402_work.p->c57C);
            R402EmSetSubMugen(0x5D, 0xE8, &r402_work.p->c57C, 15, 1);
            R402EmSetSubMugen(0x5E, 0xE9, &r402_work.p->c57C, 15, 1);
            R402EmSetSubMugen(0x5F, 0xEA, &r402_work.p->c57C, 15, 1);
        }
        if ((pG->sceat_x17C & 0x4000) && n <= 7) {
            SceDebugDisp("DeadCount:[%d]", r402_work.p->c580);
            R402EmSetSubMugen(0x55, 0xDE, &r402_work.p->c580, 20, 1);
            R402EmSetSubMugen(0x56, 0xDF, &r402_work.p->c580, 20, 1);
            R402EmSetSubMugen(0x57, 0xE0, &r402_work.p->c580, 20, 1);
        }
        SceSleep(1);
    }
}

// The number of active (in range) enemies.
int R402CalcActiveEmWarp()
{
    int n = 0;
    int i;

    for (i = 0; i < 0x70; i++) {
        if (r402_work.p->em[i].isActive() == 1) {
            n++;
        }
    }
    return n;
}
