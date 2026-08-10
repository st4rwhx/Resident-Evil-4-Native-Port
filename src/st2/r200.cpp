#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "etc_model.h"
#include "player.h"
#include "pl_npc.h"
#include "cam_ctrl.h"
#include "stage.h"
#include "read.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "merchant.h"

// Room 2-00 (D:/Bio4/Prog/r200.cpp): the village entrance after the chapter break — the show-view
// pan, the s00 event (the truck), the Ganado wave and the event handler.

struct R200Work {
    u8 pad_0[8];
    u32 snd;            // 0x08  show-view stream request
    int eff0C;          // 0x0C  effect kind of the truck event
    int eff10;          // 0x10  effect kind of the room
    cEmWrap em0;        // 0x14  the truck driver
    cEmWrap em1;        // 0x20
    int eff2C;          // 0x2C  effect kind of the show view
};

// The work pointer is a struct member: every store through the work reloads it.
struct R200WorkPtr {
    R200Work* p;
};

static R200WorkPtr r200_work;

// game/EtcModel.cpp (Bio4.sym marks it local; the room imports it)
extern "C" int setRoomEtcBreakDisp(int no, int on, int flag);
void Obj18CmfOn(cObj* o, u32 n);   // game/obj18.cpp
extern LevelEntry level_r200[];
extern StockEntry stock_2st_first[];

void r200_openBox_main(int id, int mode);
static void r200_openedBox(int id);
static void r200_openBox(int id);
static void r200_execShowView_end();
static void r200_execShowView();
static void r200_execEvent00();
static void r200_checkDoor();
void r200_lockDoor();
static void r200_checkEmSetEvent();
static void r200_execTruckEvent_end();
static void r200_execTruckEvent();
extern "C" void Evt_R200S00_Func(Event* e);

void R200Init()
{
#line 51 "D:/Bio4/Prog/r200.cpp"
    R200Work*& wp = r200_work.p;   // reference: the following `lwz pG` stays below the store (r227 idiom)
    wp = (R200Work*) MEM_CALLOC(sizeof(R200Work), 1, 0xd);
    if (pG->x4F9F == 1) {
        RsfSet(G_ROOM_ID, 4);
        RsfSet(G_ROOM_ID, 2);
    }
    if (RsfCheck(G_ROOM_ID, 4) == 0) {
        SceExec(0x12, (TaskFunc) r200_execShowView, 0, 0, 2, 0);
    }
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        SceAtDataSet_exec(4, 0x12, 0, (TaskFunc) r200_execEvent00, 0, 1);
        EvtMgr.EvtReadAram("event/evd/r200s00.evd", 0, 0, 0, 0);
        EmReadSearch(3, 0, 0);
        EmReadSearch(0x12, 0, 0);
        EmReadSearch(0x3B, 0, 0);
        if (RsfCheck(G_ROOM_ID, 0) == 0) {
            SceAtDataSet_exec(0, 0x12, 0, (TaskFunc) r200_execTruckEvent, 0, 1);
            SceAtSetEnable(0x8A, 0);
        }
    } else {
        SmdGetObjPtr(8)->be_flag |= 0x20;
        SmdGetObjPtr(8)->pParts->rot.x = -0.87266463f;
        SmdGetObjPtr(9)->be_flag |= 0x20;
        SmdGetObjPtr(9)->pParts->rot.x = 0.87266463f;
    }
    EvtMgr.SetFunc("evt_r200s00_func", (void*) Evt_R200S00_Func);
    SceSetItemEvent(8, 0x84, 5, 6, r200_openBox, (void (*)()) r200_openedBox, 0, 0);
    r200_work.p->eff10 = EspPullCoreKind();
    EstSet(0, -1, 0, 0, 1, 2, 1, (u8) r200_work.p->eff10, 0, 0);
    if ((int) pG->em_dead[2][0] < 0 || (int) pG->em_dead[3][0] < 0 || (pG->em_dead[4][0] & 4)) {
        switch (checkEmListNo(G_ROOM_ID)) {
        case 2:
            EmListSetAlive(0, 0);
            break;
        case 3:
            EmListSetAlive(0, 0);
            break;
        case 4:
            EmListSetAlive(0x1D, 0);
            break;
        }
    }
}

void R200Main()
{
}

// The box lid (scroll object 0x36): opened at once (mode 1) or swung over 30 frames.
void r200_openBox_main(int id, int mode)
{
    f32 spd = 0.0f;
    cObj* obj = NULL;

    if (id == 0) {
        obj = SmdGetObjPtr(0x36);
        spd = 1.7f;
    } else {
        SceExit();
    }
    if (obj) {
        obj->be_flag |= 0x20;
        if (mode == 1) {
            obj->pParts->rot.x = spd;
        } else {
            int i;

            spd /= 30.0f;
            SndCall(6, 0x5B, 0, 0, 0, 0);
            for (i = 0; i < 30; i++) {
                if (obj) {
                    obj->pParts->rot.x += spd;
                }
                SceSleep(1);
            }
        }
    }
}

static void r200_openedBox(int id)
{
    r200_openBox_main(id, 1);
}

static void r200_openBox(int id)
{
    r200_openBox_main(id, 0);
}

static void r200_execShowView_end()
{
    SndStrReq(r200_work.p->snd, 4, 200, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    EffectEspDelete(0, (u8) r200_work.p->eff2C, 0, 0);
    EffectEspgenDelete(0, (u8) r200_work.p->eff2C, 0);
    EffectEfmDelete(0, (u8) r200_work.p->eff2C, 0);
}

// The camera pans over the village on the first visit.
static inline f32 FCRef(const f32& v) { return v; }

static void r200_execShowView()
{
    // The 0.0 is loaded after the BitOn store: a pool constant would move above it (pool loads never
    // depend on stores), a `static const` read through a reference stays below (docs/matching.md, cSceObj).
    static const f32 vol = 0.0f;

    RsfSet(G_ROOM_ID, 4);
    if ((pG->flags_54 & 0x40) == 0) {
        BitOn(pG->flags_54, 0x40);
        r200_work.p->snd = SndStrReq(0, 0x18, 0x80000003, 0, 0, FCRef(vol));
        SceSetEventCancel(1, (TaskFunc) r200_execShowView_end, 0, -1, 1);
        SceEventStart(0);
        r200_work.p->eff2C = EspPullCoreKind();
        EstSet(0, -1, 0, 0, 1, 4, 1, (u8) r200_work.p->eff2C, 0, 0);
        CamCtrl.CutCall(5);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        r200_execShowView_end();
    }
}

// Area 4: the s00 event; chapter 2-1 ends.
static void r200_execEvent00()
{
    RsfSet(G_ROOM_ID, 2);
    SndRoomStrStop(3);
    SceEventStart(0);
    pG->flags_54 |= 0x400;
    EmMgr.destroyAll();
    SceSleep(2);
    EmReadInit();
    EvtMgr.EvtReadExec("event/evd/r200s00.evd", 0, 0x50);
    SceEventEnd(0);
    pG->flags_51C0 |= 0x00800000;
    SceAtInitSaveItem();
    levelDataAdd(merchantData, level_r200);
    stockDataAdd(merchantData, stock_2st_first);
    SceSetChapterEnd(5, 9);
}

static void r200_checkDoor()
{
    SceUpCut(0, -1, 0, 0);
}

void r200_lockDoor()
{
    SceAtDataSet_exec(2, 0x12, 0, (TaskFunc) r200_checkDoor, 0, 1);
    memclr_asm(pG->save_item, 0x1000);
}

// The Ganado wave once the player has turned away from the truck (or reached area 0xB).
static void r200_checkEmSetEvent()
{
    int cnt = 0;

    for (;;) {
        if (SceAtHitCheck(3) == 0 && pPL->rot.y >= 0.0f && pPL->rot.y <= 3.14f) {
            cnt++;
            if (cnt > 15) {
                goto found;
            }
        } else {
            cnt = 0;
        }
        if (SceAtHitCheck(0xB) == 1) {
            goto found;
        }
        SceSleep(1);
    }
found:
    SndCall(6, 3, 0, 0, 0, 0);
    setEm(0x69, 1, 1, 1, 1);
    setEm(0x6A, 1, 1, 1, 1);
    setEm(0x6B, 1, 1, 1, 1);
    setEm(0x6C, 1, 1, 1, 1);
    setEm(0x6D, 1, 1, 1, 1);
    setEm(0x6E, 1, 1, 1, 1);
    setEm(0x6F, 1, 1, 1, 1);
    setEm(0x70, 1, 1, 1, 1);
    SndRoomStrStart(1, 3, 1);
    while (SceCountEmAlive(0x10, 0x20) != 0) {
        if (RsfCheck(G_ROOM_ID, 2)) {
            break;
        }
        SceSleep(1);
    }
    SndRoomStrStop(3);
}

// The truck has passed: Leon is placed at the gate, the collision of the truck is added.
// COMPILER-DIFF: 11 -- a 16-byte `ang` makes the merged free slot of the first block (12 + 16 = 28)
// large enough for assign_stack_temp to split it, so the second block's Vecs reuse both slots (frame 0x50).
struct R200Vec4 { Vec v; f32 pad; };
static void r200_execTruckEvent_end()
{
    {
        Vec pos = {10850.0f, 88.0f, 86.0f};
        R200Vec4 ang;
        Vec* pa = &ang.v;
        cPlayer* pl = pPL;
        f32 ry = 1.352f;

        pl->setPos(&pos);
        ang.v.x = 0.0f;
        pa->y = ry;
        ang.v.z = 0.0f;
        pl->setAng(pa);
        CamCtrl.Comeback(0);
        EffectEspDelete(0, (u8) r200_work.p->eff0C, 0, 0);
        EffectEspgenDelete(0, (u8) r200_work.p->eff0C, 0);
        EffectEfmDelete(0, (u8) r200_work.p->eff0C, 0);
        SceEventEnd(0);
        if ((pG->flags_174 & 0x80000000) == 0) {
            cEm* em = r200_work.p->em0.getPtr();

            EstSet((int) em, -1, 0, 0, 1, 0x20, 0, 0, (u32) r200_work.p->em0.getPtr(), 0);
        }
        r200_work.p->em0.setNoSuspend(0);
        r200_work.p->em1.setNoSuspend(0);
        while (r200_work.p->em0.isActive() == 1) {
            SceSleep(1);
        }
        SceAtSetEnable(0x8A, 1);
    }
    {
        // The collision pieces of the truck: fresh zero vectors in the freed slots.
        Vec pos = {0.0f, 0.0f, 0.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};

        SatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 5), 0, &pos, &rot, 1);
        EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 0x12), 0, &pos, &rot, 1);
    }
    SceExec(0x12, (TaskFunc) r200_checkEmSetEvent, 0, 0, 2, 0);
}

// Area 0: the truck drives in.
static void r200_execTruckEvent()
{
    r200_lockDoor();
    r200_work.p->em0.setPtr(0x64, -1, 1);
    r200_work.p->em1.setPtr(0x66, -1, 1);
    if (r200_work.p->em0.getPtr() == 0) {
        SceExit();
    }
    r200_work.p->em0.setNoSuspend(1);
    r200_work.p->em1.setNoSuspend(1);
    RsfSet(G_ROOM_ID, 0);
    SceEventStart(1);
    CamCtrl.CutCall(2);
    r200_work.p->em0.setFlag(1);
    SceSetEventCancel(1, (TaskFunc) r200_execTruckEvent_end, 0, -1, 1);
    r200_work.p->eff0C = 0;
    r200_work.p->eff0C = EspPullCoreKind();
    SceSleep(10);
    SndCall(6, 5, &r200_work.p->em0.getPtr()->getPartsPtr(1)->worldPos, 0, 0, 0);
    SceSleep(10);
    pG->flags_174 |= 0x80000000;
    {
        cEm* em = r200_work.p->em0.getPtr();

        EstSet((int) em, -1, 0, 0, 1, 0x20, 0, 0, (u32) r200_work.p->em0.getPtr(), 0);
    }
    EstSet((int) r200_work.p->em0.getPtr(), -1, 0, 0, 1, 1, 1, (u8) r200_work.p->eff0C, 0, 0);
    SceSleep(70);
    {
        cEm* em = r200_work.p->em0.getPtr();

        EstSet((int) em, -1, 0, 0, 1, 0x22, 1, 0, (u32) r200_work.p->em0.getPtr(), 0);
    }
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r200_execTruckEvent_end();
}

extern "C" void Evt_R200S00_Func(Event* e)
{
    switch (e->funcMode) {
    case 0:
        SmdGetObjPtr(8)->setNoSuspend(0);
        SmdGetObjPtr(9)->setNoSuspend(0);
        SmdGetObjPtr(0x18)->setNoSuspend(0);
        SmdGetObjPtr(0x33)->setNoSuspend(0);
        SmdGetObjPtr(0x34)->setNoSuspend(0);
        EffectEspDelete(1, (u8) r200_work.p->eff10, 0, 0);
        EffectEspgenDelete(1, (u8) r200_work.p->eff10, 0);
        EffectEfmDelete(1, (u8) r200_work.p->eff10, 0);
        setRoomEtcBreakDisp(6, 0, 1);
        break;
    case 1:
        switch (e->cut) {
        case 0:
            if (e->frame == 0) {
                void* mod;

                if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
                    ((cModel*) mod)->x12F = 1;
                }
                if (e->GetMod(&mod, "pl0100", 0, 0) == 1) {
                    ((cModel*) mod)->x12F = 1;
                }
                if (e->GetMod(&mod, "evm0900", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cObj*) mod)->be_flag |= 2;
                }
                if (e->GetMod(&mod, "evm0910", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cObj*) mod)->be_flag |= 2;
                }
                if (e->GetMod(&mod, "evm1000", 0, 0) == 1) {
                    ((cObj*) mod)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod, "evm1010", 0, 0) == 1) {
                    ((cObj*) mod)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod, "evm1100", 0, 0) == 1) {
                    ((cObj*) mod)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod, "evm1110", 0, 0) == 1) {
                    ((cObj*) mod)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod, "evm1120", 0, 0) == 1) {
                    ((cObj*) mod)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod, "evm1300", 0, 0) == 1) {
                    ((cObj*) mod)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod, "evm1310", 0, 0) == 1) {
                    ((cObj*) mod)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod, "evm1500", 0, 0) == 1) {
                    ((cObj*) mod)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod, "evm1510", 0, 0) == 1) {
                    ((cObj*) mod)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod, "evm1600", 0, 0) == 1) {
                    ((cObj*) mod)->be_flag |= 0x10;
                }
                if (e->GetMod(&mod, "evm1610", 0, 0) == 1) {
                    ((cObj*) mod)->be_flag |= 0x10;
                }
            }
            break;
        case 3:
        case 6:
        case 0xA:
        case 0x10:
        case 0x13:
            if (e->frame == 0) {
                EventCutEstSet(1, e->cut);
            }
            break;
        case 0x14:
            if (e->frame == 2) {
                SndRoomStrStop(3);
            }
            break;
        }
        break;
    case 2:
        SmdGetObjPtr(8)->be_flag |= 0x20;
        SmdGetObjPtr(8)->pParts->rot.x = -0.87266463f;
        SmdGetObjPtr(9)->be_flag |= 0x20;
        SmdGetObjPtr(9)->pParts->rot.x = 0.87266463f;
        SmdGetObjPtr(0x18)->setNoSuspend(1);
        SmdGetObjPtr(0x33)->setNoSuspend(1);
        SmdGetObjPtr(0x34)->setNoSuspend(1);
        {
            cSubChar* sub = pSUB;

            if (sub) {
                cPlayer* pl = pPL;
                Vec* rot = &pl->rot;

                sub->setPos(&pl->pos);
                sub->setAng(rot);
            }
        }
        setRoomEtcBreakDisp(6, 1, 1);
        SndRoomStrStop(3);
        break;
    }
}
