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
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "player.h"
#include "act_btn.h"
#include "shadow.h"
#include "fade.h"

// Room 2-15 (D:/Bio4/Prog/r215.cpp): the s00/s01 event chain and its Evt_*_Func handlers.

struct R215Work {
    u8 dummy;
};

static R215Work* r215_work;

extern "C" void R215_Event();
static void r215_succeedAction();
extern "C" void Evt_R215S00_Func(Event* e);
extern "C" void Evt_R215S01_Func(Event* e);
extern "C" void Evt_R215S02_Func(Event* e);

// 1 while the event is being skipped / cancelled (an EVT status bit).
static inline int r215_evtStatus(Event* e, u32 bit)
{
    int on = 1;

    if ((e->status & bit) == 0) {
        on = 0;
    }
    return on;
}

// The rooms call Event::FlgOnStatus out of line (event.h has it in-class).
void EvtFlgOnStatus(Event* e, u32 no) asm("FlgOnStatus__5EventUl");

// The running event's key (&EvtMgr.x34 as an accessor result: the address is formed last).
static inline u32* evtKey(EventMgr* m) { return &m->x34; }

// Light kind mask / display flag of an event model.
#define R215_EVT_MOD_LIGHT(name, kind)                  \
    if (e->GetMod(&mod, name, 0, 0) == 1) {             \
        ((cModel*) mod)->lightInfo.x50 = kind;          \
    }
#define R215_EVT_MOD_FLAG(name)                         \
    if (e->GetMod(&mod, name, 0, 0) == 1) {             \
        ((cModel*) mod)->be_flag |= 0x80;               \
    }

void R215Init()
{
#line 44 "D:/Bio4/Prog/r215.cpp"
    r215_work = (R215Work*) MEM_CALLOC(sizeof(R215Work), 1, 0xd);
    if (pG->x4F9F == 1) {
        RsfSet(G_ROOM_ID, 0);
    }
    EvtMgr.SetFunc("evt_r215s00_func", (void*) Evt_R215S00_Func);
    EvtMgr.SetFunc("evt_r215s01_func", (void*) Evt_R215S01_Func);
    EvtMgr.SetFunc("evt_r215s02_func", (void*) Evt_R215S02_Func);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        EvtMgr.EvtReadMram("event/evd/r215s00.evd", 0, 0, 0, 0);
        SceExec(0x12, (TaskFunc) R215_Event, 0, 0, 2, 0);
    }
}

void R215Main()
{
}

extern "C" void R215_Event()
{
    SceEventStart(0);
    SceSleep(1);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        pG->flags_174 |= 0x80000000;
        EvtMgr.EvtReadExec("event/evd/r215s00.evd", 0, 0);
        if ((int) pG->flags_174 >= 0) {
            EvtMgr.EvtReadExec("event/evd/r215s01.evd", 0, 0);
            {
                Vec pos = {39050.0f, 3000.0f, 200.0f};
                Vec ang;
                Vec* pa = &ang;
                f32 ry = 1.5707964f;
                cPlayer* p = pPL;

                p->setPos(&pos);
                ang.x = 0.0f;
                pa->y = ry;
                ang.z = 0.0f;
                p->setAng(pa);
            }
        } else {
            EvtMgr.EvtReadExec("event/evd/r215s02.evd", 0, 2);
            for (;;) {
                SceSleep(1);
            }
        }
        SceEventEnd(0);
        pG->flags_54 |= 0x400;
        SceAtExecute(2);
    }
}

static void r215_succeedAction()
{
    pG->flags_174 &= ~0x80000000;
}

extern "C" void Evt_R215S00_Func(Event* e)
{
    void* mod;

    switch (e->funcMode) {
    case 0:
        EvtFlgOnStatus(e, 3);
        e->cancelCut = 9;
        break;
    case 1:
        switch (e->cut) {
        case 3:
            if (e->frame == 0) {
                SetShadowCamMoveSize(0.0f);
            }
            break;
        case 2:
        case 4:
            if (e->frame == 0) {
                ResetShadowCamMoveSize();
            }
            break;
        }
        switch (e->cut) {
        case 0:
            if (e->frame == 0) {
                R215_EVT_MOD_LIGHT("pl0100", 1)
                R215_EVT_MOD_LIGHT("em3700", 2)
                R215_EVT_MOD_LIGHT("evm7400", 4)
                R215_EVT_MOD_LIGHT("evm7410", 4)
                R215_EVT_MOD_LIGHT("evm5500", 8)
                R215_EVT_MOD_LIGHT("evm5510", 8)
                R215_EVT_MOD_LIGHT("evm5700", 8)
                R215_EVT_MOD_LIGHT("evm4900", 8)
                R215_EVT_MOD_LIGHT("evm5100", 0x20)
                R215_EVT_MOD_LIGHT("evm5200", 0x40)
                R215_EVT_MOD_FLAG("evm5500")
                R215_EVT_MOD_FLAG("evm5510")
                R215_EVT_MOD_FLAG("evm5600")
            }
            break;
        case 0xA:
            BitOff(pG->flags_170, 0x100);
            BitOff(pG->flags_58, 0x800);
            if (pG->flags_174 & 0x80000000) {
                // The button object and the callback are evaluated before the stack argument store.
                cActionButton* ab = &ActBtn;
                int func = (int) r215_succeedAction;

                ab->set(0x30, 5, func, 0, 0x42, 4, 0, 0);
            } else {
                e->CancelSet();
            }
            break;
        }
        break;
    case 2:
        ResetShadowCamMoveSize();
        break;
    case 3:
        if (r215_evtStatus(e, 0x4000) == 0) {
            EvtMgr.EvtSndStrPlay(evtKey(&EvtMgr), 1, 0x89, 1, 0.0f);
        }
        break;
    }
}

extern "C" void Evt_R215S01_Func(Event* e)
{
    void* mod;

    switch (e->funcMode) {
    case 0:
        break;
    case 1:
        if (e->cut == 0x10) {
            if (e->frame == 0) {
                SetShadowCamMoveSize(0.0f);
            }
        } else {
            if (e->frame == 0) {
                ResetShadowCamMoveSize();
            }
        }
        if (e->cut == 0x11) {
            if (e->frame == 0) {
                SmdSetTrans(0x2C, 0);
                SmdSetTrans(0x2E, 0);
            }
        } else {
            if (e->frame == 0) {
                SmdSetTrans(0x2C, 1);
                SmdSetTrans(0x2E, 1);
            }
        }
        switch (e->cut) {
        case 0:
            if (e->frame == 0) {
                R215_EVT_MOD_LIGHT("pl0100", 1)
                R215_EVT_MOD_LIGHT("em3700", 2)
                R215_EVT_MOD_LIGHT("evm7400", 4)
                R215_EVT_MOD_LIGHT("evm7410", 4)
                R215_EVT_MOD_LIGHT("evm2500a", 4)
                R215_EVT_MOD_LIGHT("evm3800", 4)
                R215_EVT_MOD_LIGHT("evm4000", 4)
                R215_EVT_MOD_LIGHT("evm5500", 8)
                R215_EVT_MOD_LIGHT("evm5510", 8)
                R215_EVT_MOD_LIGHT("evm5700", 8)
                R215_EVT_MOD_LIGHT("evm5800", 8)
                R215_EVT_MOD_LIGHT("evm5900", 8)
                R215_EVT_MOD_LIGHT("evm5100", 0x20)
                R215_EVT_MOD_LIGHT("evm5200", 0x40)
                R215_EVT_MOD_FLAG("evm5500")
                R215_EVT_MOD_FLAG("evm5510")
            }
            break;
        case 0x12:
            if (e->frame == 0x5A) {
                if (r215_evtStatus(e, 0x40000000) == 0) {
                    FadeSetW(2, 30, 0, 0);
                }
            }
            break;
        }
        break;
    case 2:
        SmdSetTrans(0x2C, 1);
        SmdSetTrans(0x2E, 1);
        ResetShadowCamMoveSize();
        break;
    }
}

extern "C" void Evt_R215S02_Func(Event* e)
{
}
