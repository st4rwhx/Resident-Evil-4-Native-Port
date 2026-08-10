#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "card.h"
#include "sofdec.h"
#include "flag_rsf.h"
#include "global.h"
#include "main.h"
#include "main_sub.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "player.h"
#include "cam_ctrl.h"
#include "mes.h"
#include "item.h"
#include "snd.h"
#include "fade.h"
#include "view.h"
#include "dvd.h"
#include "option.h"
#include "cDataSwap.h"
#include "mercenaries.h"
#include "TexRender.h"
#include "cSceObj.h"

// Room 4-0E (D:/Bio4/Prog/r40e.cpp): the elevator ride with its camera cuts, the enemy that
// appears on the way, the s00 event with the render-to-texture camera and the Assignment Ada
// result screen.

struct R40eWork {
    cSceObj elv;         // 0x000  the elevator mover
    TexRenderMng* tex;   // 0x0F8  event render target
    u8 texTbl[0x80];     // 0x0FC  blend table of the render target
    TexRenderCam cam;    // 0x17C  event render camera
    u32 str;             // 0x480  SndStrReq handle of the show-view event
};

static R40eWork* r40e_work;

// The original reads r4 although its prototype has one parameter (r11b).
void TexRenderModResP(cModel* m, int parts) asm("TexRenderModRes");
// The u8 result is passed on unmasked to SceDestroyEm (COMPILER-DIFF 4).
int GetEmIdFromListI(u32 no) asm("GetEmIdFromList");
// The result screen's init is called with r4 left as it was (no argument set up).
void AdaResultInit(AdaResult* r) asm("init__9AdaResulti");

static void r40e_execShowView_end();
static void r40e_execShowView();
static void r40e_setElvCamera(u32 mode);
static void r40e_moveElevator(u32 dir);
void r40e_initElevator();
static void r40e_execEmAppear_end();
static void r40e_execEmAppear();
static void r40e_checkEmDead();
static void R40EExecEventS00();
static void gameResult();
extern "C" void Evt_R40ES00_Func(Event* e);
void EvtTexRenderCamTrans(Event* e, int cut);

void R40eInit()
{
#line 60 "D:/Bio4/Prog/r40e.cpp"
    r40e_work = (R40eWork*) MEM_CALLOC(sizeof(R40eWork), 1, 0xd);
    r40e_initElevator();
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        if (RsfCheck(G_ROOM_ID, 1) == 0) {
            SceAtDataSet_exec(4, 0x12, 0, (TaskFunc) r40e_execEmAppear, 0, 1);
        } else {
            SceExec(0x12, (TaskFunc) r40e_checkEmDead, 0, 0, 2, 0);
        }
        r40e_work->elv.setReverse(1);
        SceAtSetEnable(5, 1);
    } else {
        SceAtSetEnable(5, 0);
    }
    EvtMgr.SetFunc("evt_r40es00_func", (void*) Evt_R40ES00_Func);
    EvtMgr.SetFunc("evt_r40es99_func", (void*) Evt_R40ES00_Func);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceAtDataSet_exec(3, 0x12, 0, (TaskFunc) R40EExecEventS00, 0, 1);
        EvtMgr.EvtReadAram("event/evd/r40es00.evd", (u8) GetEmIdFromListI(0xDD), 0, 0, 0);
    }
    TexRenderInit(&r40e_work->tex, 0, 1);
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceExec(0x12, (TaskFunc) r40e_execShowView, 0, 0, 2, 0);
    }
}

void R40eMain()
{
}

static void r40e_execShowView_end()
{
    SndStrReq(r40e_work->str, 4, 50, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

// Area 3: the camera shows the room (cut 11) with its stream.
static inline f32 FCRef(const f32& v) { return v; }

static void r40e_execShowView()
{
    // The 0.0 is loaded after the RsfSet store: a pool constant would move above it (pool loads never
    // depend on stores), a `static const` read through a reference stays below (docs/matching.md, r104).
    static const f32 vol = 0.0f;

    RsfSet(G_ROOM_ID, 3);
    r40e_work->str = SndStrReq(0, 0x33, 0x80000003, 0, 0, FCRef(vol));
    SceSetEventCancel(1, (TaskFunc) r40e_execShowView_end, 0, -1, 1);
    SceEventStart(0);
    CamCtrl.CutCall(0xB);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    r40e_execShowView_end();
}

// Camera cuts of the elevator ride (mode = the ride direction).
static void r40e_setElvCamera(u32 mode)
{
    pG->flags_174 &= ~0x80000000;
    switch (mode) {
    case 0:
        CamCtrl.CutCall(4);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        break;
    case 1:
        CamCtrl.CutCall(6);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        CamCtrl.CutCall(7);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        break;
    case 2:
        CamCtrl.CutCall(0xA);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        break;
    }
    pG->flags_174 |= 0x80000000;
}

// The ride: 0 up from the entrance, 1 down, 2 the return after the enemy fight.
static void r40e_moveElevator(u32 dir)
{
    cObj* obj = SmdGetObjPtr(0xA);
    u32 n;

    if (dir != 2) {
        SceEventStart(0);
    } else {
        SceEventStart(1);
    }
    pG->flags_174 &= ~0x80000000;
    if (dir == 0) {
        r40e_work->elv.setReverse(0);
    } else if (dir <= 2) {
        r40e_work->elv.setReverse(1);
    }
    if (obj) {
        obj->pInfo->flagsDC |= 1;
        if (dir == 0) {
            obj->pInfo->uvScrollU = 0.05f;
        } else {
            obj->pInfo->uvScrollU = -0.05f;
        }
    }
    SceExec(0x12, (TaskFunc) r40e_setElvCamera, dir, 0, 2, 0);
    if (dir <= 1) {
        cPlayer* pl = pPL;
        cSceObj* elv = &r40e_work->elv;

        if (pl) {
            for (n = 0; n < 4; n++) {
                if (elv->sub[n] == NULL) {
                    elv->sub[n] = pl;
                    break;
                }
            }
        }
        pPL->setNoSuspend(1);
    }
    SndCall(6, 0, 0, 0, 0, 0);
    switch (dir) {
    case 0:
    case 1:
        do {
            if (r40e_work->elv.move() == 0) {
                if ((int) pG->flags_174 < 0) {
                    break;
                }
            }
            SceSleep(1);
        } while (1);
        break;
    case 2:
        r40e_work->elv.move();
        SceSleep(1);
        r40e_work->elv.cnt = 90;
        do {
            if (r40e_work->elv.move() == 0) {
                if ((int) pG->flags_174 < 0) {
                    break;
                }
            }
            SceSleep(1);
        } while (1);
        break;
    }
    SndCall(6, 1, 0, 0, 0, 0);
    if (obj) {
        obj->pInfo->uvScrollU = 0.0f;
    }
    pPL->setNoSuspend(0);
    {
        cPlayer* pl = pPL;
        cSceObj* elv = &r40e_work->elv;

        if (pl) {
            for (n = 0; n < 4; n++) {
                if (elv->sub[n] == pl) {
                    elv->sub[n] = NULL;
                    break;
                }
            }
        }
    }
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

void r40e_initElevator()
{
    SceAtDataSet_exec(1, 0x12, 0, (TaskFunc) r40e_moveElevator, 0, 1);
    SceAtDataSet_exec(2, 0x12, 0, (TaskFunc) r40e_moveElevator, (void*) 1, 1);
    cObj* obj = SmdGetObjPtr(9);
    Vec d = {0.0f, 11656.0f, 0.0f};
    r40e_work->elv.initMove1_pos(obj, 210, &d, 20.0f, 20.0f);
    r40e_work->elv.setVibration(8, 8, 2.0f, 0.5f, 2.0f);
}

static void r40e_execEmAppear_end()
{
    SceEventEnd(0);
    pG->flags_5014 &= ~0x02000000;
    CamCtrl.Comeback(0);
    cEmWrap em;
    em.setPtr(0xDD, -1, 1);
    em.setNoSuspend(0);
    pPL->setNoSuspend(0);
    *EM_LIST(0xDD) = *EM_LIST(0xDE);
    EmListSetAlive(0xDD, 1);
    SceExec(0x12, (TaskFunc) r40e_checkEmDead, 0, 0, 2, 0);
}

// Area 4: the enemy drops in (cut 9).
static void r40e_execEmAppear()
{
    RsfSet(G_ROOM_ID, 1);
    while (SceCheckEventStart() != 1) {
        SceSleep(1);
    }
    SceSetEventCancel(1, (TaskFunc) r40e_execEmAppear_end, 0, -1, 1);
    SceEventStart(0);
    pG->flags_5014 |= 0x02000000;
    cEmWrap em;
    Vec p;
    Vec* pp = &p;
    em.setEm(0xDD, -1, 1, 1, 1);
    em.setNoSuspend(1);
    pPL->setNoSuspend(1);
    cPlayer* pl = pPL;
    p.x = 22716.0f;
    pp->y = -3970.0f;
    pp->z = 28044.0f;
    pl->setPos(pp);
    SndRoomStrStart(1, 0, 1);
    CamCtrl.CutCall(9);
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    SceSetEventCancel(0, 0, 0, -1, 1);
    r40e_execEmAppear_end();
}

// Waits for the enemy to die, then the elevator comes back.
static void r40e_checkEmDead()
{
    SceSleep(1);
    SndRoomStrStart(1, 0, 1);
    cEmWrap em;
    em.setPtr(0xDD, -1, 1);
    while (em.isActive() != 0) {
        SceDebugDisp("E[%d]", em.getHp());
        SceSleep(1);
    }
    SndRoomStrStop(3);
    RsfSet(G_ROOM_ID, 2);
    SceSleep(60);
    r40e_moveElevator(2);
    SceAtSetEnable(5, 0);
}

// Area 3 of the s00 event: the confirmation, then the event and the result screen.
static void R40EExecEventS00()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceEventStart(0);
        pPL->setNoSuspend(1);
        // Two sets of one pointer variable: `addi r31,r9,cMes@l; addi r31,r31,4` in place (a fresh
        // `cMes.getWork()` pseudo gives `addi r9,..; addi r31,r9,4`).
        MesWork* w = (MesWork*) &cMes;
        w = (MesWork*) ((u8*) w + 4);
        SceMesSet(0, 0, 1, 0x64, 0x150 - w->lineSpace - w->fontH - 1);
        if (SceMesGetSelection() != 1) {
            CamCtrl.Comeback(0);
            SceEventEnd(0);
        } else {
            SndCall(6, 2, 0, 0, 0, 0);
            if ((u32) ItemMgr.num(0xC) <= 4) {
                SceMesSet(2, 0, 1, 0x64, 0x150 - w->lineSpace - w->fontH - 1);
                CamCtrl.Comeback(0);
                SceEventEnd(0);
            } else {
                RsfSet(G_ROOM_ID, 0);
                SceDestroyEm(GetEmIdFromListI(0xDD), -1);
                SceSleep(1);
                EvtMgr.EvtReadExec("event/evd/r40es00.evd", (u8) GetEmIdFromListI(0xDD), 0);
                FadeSetW(0, 0, 0, 0);
                SceSleep(1);
                SceExec(0x12, (TaskFunc) gameResult, 0, 0, 2, 0);
                SceEventEnd(0);
            }
        }
    }
}

// The Assignment Ada ending movie and result screen (mercenaries.cpp MercSysResult's shape).
static void gameResult()
{
    static int FADE_TIME = 15;
    static u32 MARGIN = 0x20000;
    static char data_name[] = "SS/___/omk_r0.dat";
    static u32 stop_bak;
    static u32 disp_bak;
    cDataSwap swap;
    u32 size;
    AdaResult* res;

    disp_bak = pG->flags_58;
    BitSet(pG->flags_58, 0xFFFFFFFF);
    BitOff(pG->flags_58, 0x2000);
    BitOff(pG->flags_58, 0x800);
    BitOff(pG->flags_58, 0x10000);
    stop_bak = pG->flags_170;
    BitSet(pG->flags_170, 0xFFFFFFFF);
    BitOff(pG->flags_170, 0x00800000);
    BitOff(pG->flags_170, 0x80000000);
    BitOff(pG->flags_170, 0x40);
    SceSleep(2);
    systemVISetBlack(1);
    FadeKill(2);
    ScreenReSize(0x200, 0x1C0);
    if (!(pSys->x4 & 0x00200000)) {
        pSys->x4 |= 0x00200000;
        Sofdec.Initialize("movie/adaend_m.sfd", 0);
    } else {
        pSys->x4 &= ~0x00200000;
        Sofdec.Initialize("movie/adaend_c.sfd", 0);
    }
    SceSleep(1);
    while (Sofdec.isPlay()) {
        SceSleep(1);
    }
    // Loop-note barrier: FadeSetW's `li r28,0xff` (a pseudo live across later calls) is issued after
    // systemVISetBlack(0) in the original; as three plain calls sched1 hoists it to the block top.
    do {
        systemVISetBlack(1);
        ScreenReSize(0x280, 0x1C0);
        systemVISetBlack(0);
    } while (0);
    FadeSetW(2, 0, 0, 0);
    SceSleep(1);
    if (!(pSys->x4 & 0x20000000)) {
        pSys->x4 |= 0x20000000;
        SceEventStart(0);
        setLangExt3(data_name + 3);
        Dvd.FileExistCheck(data_name, &size);
        size += 0x34;
        size += MARGIN;
        swap.SwapOut((u32) pG->pRoomArc, size, 0);
        res = new AdaResult;
        AdaResultInit(res);
        FadeKillAll();
        FadeSetW(0x80000002, FADE_TIME, 0, 0);
        if (Fade[2].flags & 1) {
            SceSleep(1);
        }
        while (res->move(3) != 0) {
            SceSleep(1);
        }
        FadeSetW(2, FADE_TIME, 0, 0);
        if (Fade[2].flags & 1) {
            SceSleep(1);
        }
        res->quit();
        delete res;
        swap.SwapIn();
    }
    pG->flags_54 |= 0x04000000;
}

extern "C" void Evt_R40ES00_Func(Event* e)
{
    void* mod;

    switch (e->funcMode) {
    case 0:
        ZFAR = 100000000.0f;
        BitOn(pG->flags_5010, 0x800);
        break;
    case 1:
        if (e->cut == 0) {
            if (e->frame == 0) {
                if (e->GetMod(&mod, "pl0d00", 0, 0) == 1) {
                    ((cModel*) mod)->lightInfo.x50 = 0x40;
                }
                if (e->GetMod(&mod, "pl0c00", 0, 0) == 1) {
                    ((cModel*) mod)->lightInfo.x50 = 1;
                }
                if (e->GetMod(&mod, "evmb900", 0, 0) == 1) {
                    ((cModel*) mod)->lightInfo.x50 = 2;
                }
                if (e->GetMod(&mod, "pl0c00", 0, 0) == 1) {
                    Obj18Work* w = &((cObj*) mod)->o18;

                    if (w && w->child) {
                        ((cObj*) mod)->o18.x74 |= 0x04000000;
                        w->child->be_flag &= ~2;
                    }
                }
            }
        }
        switch (e->cut) {
        case 3:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "evmc100", 0, 0) == 1) {
                    TexRenderModSet((cModel*) mod, 0, r40e_work->texTbl, r40e_work->tex, 1, 1, 1, 1, 1.0f);
                }
            }
            EvtTexRenderCamTrans(e, 3);
            break;
        case 5:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "evmc100", 0, 0) == 1) {
                    TexRenderModSet((cModel*) mod, 0, r40e_work->texTbl, r40e_work->tex, 1, 1, 1, 1, 1.0f);
                }
            }
            EvtTexRenderCamTrans(e, 5);
            break;
        case 7:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "evmc100", 0, 0) == 1) {
                    TexRenderModSet((cModel*) mod, 0, r40e_work->texTbl, r40e_work->tex, 1, 1, 1, 1, 1.0f);
                }
            }
            EvtTexRenderCamTrans(e, 7);
            break;
        default:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "evmc100", 0, 0) == 1) {
                    TexRenderModResP((cModel*) mod, 0);
                }
            }
            break;
        }
        break;
    case 2:
        pG->flags_5010 &= ~0x800;
        break;
    }
}

// Render-to-texture pass of the event camera cuts 3 / 5 / 7.
void EvtTexRenderCamTrans(Event* e, int cut)
{
    void* mod;
    void* bin;
    int skip = 1;

    if ((e->status & 0x40000000) == 0) {
        skip = 0;
    }
    if (skip == 0) {
        if (e->GetMod(&mod, "pl0d00", 0, 0) == 1) {
            TexRenderModAddOt(0, (cModel*) mod);
        }
        switch (cut) {
        case 3:
            if (EvtMgr.GetBin(&bin, "event/r40e/s00/cam/etc_s00_003.fcv", 0) == 1) {
                TexRenderCamAddOt(0, &r40e_work->cam, (TexRenderEvt*) e, bin);
            }
            break;
        case 5:
            if (EvtMgr.GetBin(&bin, "event/r40e/s00/cam/etc_s00_005.fcv", 0) == 1) {
                TexRenderCamAddOt(0, &r40e_work->cam, (TexRenderEvt*) e, bin);
            }
            break;
        case 7:
            if (EvtMgr.GetBin(&bin, "event/r40e/s00/cam/etc_s00_007.fcv", 0) == 1) {
                TexRenderCamAddOt(0, &r40e_work->cam, (TexRenderEvt*) e, bin);
            }
            break;
        }
    }
}

// The split object's .data is 8-aligned (0x1A -> 0x20 bytes).
asm(".section .data\n\t.balign 8\n\t.text");
