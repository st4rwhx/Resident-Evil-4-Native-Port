#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "event.h"
#include "atari.h"
#include "flag_rsf.h"
#include "sofdec.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "global.h"
#include "main_sub.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "obj.h"
#include "em.h"
#include "id_sys.h"
#include "TexRender.h"
#include "sscrn.h"
#include "fade.h"

// Room 1-20 (D:/Bio4/Prog/r120.cpp): the opening; plays the movie, then the two intro events in the
// police car and jumps to room 1-00.

void Obj18CmfOn(cObj* o, u32 n);   // game/obj18.cpp

struct R120Work {
    TexRenderMng* mgr;    // 0x00  TexRenderInit output
    u8 pad_4[0x388 - 4];
};

static R120Work* r120_work;

extern "C" void R120Event();
extern "C" void Evt_R120S00_Func(Event* e);
extern "C" void Evt_R120S01_Func(Event* e);
extern "C" void EventCarInit(Event* e);
extern "C" void EvtTexRenderCamTrans(Event* e, int cut);

void R120Init()
{
#line 58 "D:/Bio4/Prog/r120.cpp"
    r120_work = (R120Work*) MEM_CALLOC(sizeof(R120Work), 1, 0xD);
    EvtMgr.SetFunc("evt_r120s00_func", (void*) Evt_R120S00_Func);
    EvtMgr.SetFunc("evt_r120s01_func", (void*) Evt_R120S01_Func);
    if (DebugTrg(1) == 0) {
        SceExec(0x12, R120Event, 0, 0, 2, 0);
    }
    TexRenderInit(&r120_work->mgr, 0x100, 1);
}

void R120Main()
{
}

// The movie, the two events, then the jump into the village road.
extern "C" void R120Event()
{
    SceSleep(1);
    if (pG->x4F8E != 0) {
        FadeSetW(1, 0, 0, 0);
        SubScreenOpen(0x10, 0);
        SceSleep(1);
        FadeSetW(0, 0, 0, 0);
    }
    pG->flags_51C0 &= ~0x10;
    systemVISetBlack(1);
    Sofdec.Initialize("movie/opening.sfd", 0);
    SceSleep(1);
    FadeSetW(2, 0, 0, 0);
    SceSleep(1);
    if (Sofdec.flag & 0x20) {
        pG->flags_51C0 |= 0x10;
    }
    SceEventStart(0);
    // Two sequential `if`s whose first test masks with a variable: the mask register keeps
    // jump1's thread_jumps from folding the first branch into the second (REG_USERVAR_P regs are
    // never equivalent there), so gcse sees the s01 block undominated and its EvtMgr/string highs
    // stay fresh; cse1 then propagates the constant and the post-loop thread_jumps redirects the
    // first `bne` past the s01 block, which merges the second test into the call block (its
    // tail jump gives the `li r7/r8` an extra dependent, so sched puts `addi r3,r30` last).
    u32 mask = 0x10;
    if (!(pG->flags_51C0 & mask)) {
        EvtMgr.EvtReadAram("event/evd/r120s01.evd", 0, 0, 0, 0);
        EvtMgr.EvtReadExec("event/evd/r120s00.evd", 0, 0);
    }
    if (!(pG->flags_51C0 & 0x10)) {
        EvtMgr.EvtReadExec("event/evd/r120s01.evd", 0, 0);
    }
    SceEventEnd(0);
    pG->flags_54 |= 0x400;
    {
        Vec pos = {-109450.0f, -515.0f, 820.0f};
        Vec rot = {0, 0, 0};

        SceAtExecRoomJump(0x100, &pos, &rot, 0);
    }
}

// Hides (on = 0) / shows (on = 1) the scroll objects the event models replace.
static inline void r120_setTrans(int on)
{
    int i;

    for (i = 0x1C; i <= 0x1F; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x25; i <= 0x29; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x2A; i <= 0x2C; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x30; i <= 0x32; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 0x20; i <= 0x24; i++) {
        SmdSetTrans(i, on);
    }
    for (i = 3; i <= 0xC; i++) {
        SmdSetTrans(i, on);
    }
}

extern "C" void Evt_R120S00_Func(Event* e)
{
    void* lmod;
    void* mod;
    int skip;

    switch (e->funcMode) {
    case 0:
        r120_setTrans(0);
        IdSys.dispSw(0x21, 0);
        break;
    case 1:
        if (e->cut == 1 || e->cut == 8) {
            if (e->frame == 0) {
                if (e->GetMod(&lmod, "obm3000c", 0, 0) == 1) {
                    cLight* l = LightMgr.getKindLight(1);

                    if (l) {
                        l->setParent((cModel*) lmod);
                    }
                }
            }
        }
        if (e->cut == 0) {
            EvtTexRenderCamTrans(e, 0);
        }
        switch (e->cut) {
        case 0:
            if (e->frame == 0) {
                skip = 1;
                if (!(e->status & 0x40000000)) {
                    skip = 0;
                }
                if (skip == 0) {
                    FadeSetW(2, 0, 0, 0);
                }
                if (e->GetMod(&mod, "pl0010", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag |= 0x100000;
                    ((cModel*) mod)->x12F = 4;
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->lightInfo.x50 = 0x20;
                }
                if (e->GetMod(&mod, "obm3010f", 0, 0) == 1) {
                    ((cModel*) mod)->x12F = 1;
                    ((cModel*) mod)->x12C = 1;
                    Obj18CmfOn((cObj*) mod, 5);
                }
                EventCarInit(e);
                if (e->GetMod(&mod, "pl0010", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag |= 2;
                }
                if (e->GetMod(&mod, "obm3010f", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag |= 2;
                }
                if (e->GetMod(&mod, "obm1a00", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag &= ~0x10;
                    ((cModel*) mod)->be_flag |= 0x80;
                }
            }
            if (e->frame == 120) {
                skip = 1;
                if (!(e->status & 0x40000000)) {
                    skip = 0;
                }
                if (skip == 0) {
                    FadeSetW(0x80000002, 60, 0, 0);
                }
            }
            break;
        case 1:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "pl0010", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag &= ~0x20;
                    ((cModel*) mod)->be_flag &= ~2;
                }
                if (e->GetMod(&mod, "obm3010f", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag &= ~0x20;
                    ((cModel*) mod)->be_flag &= ~2;
                }
            }
            break;
        case 8:
            if (e->frame == 50) {
                FadeSetW(2, 60, 0, 0);
            }
            break;
        }
        switch (e->cut) {
        case 0:
            if (e->frame == 0) {
                pG->flags_58 |= 0x08000000;
                if (e->GetMod(&mod, "obm3000f", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
            }
            break;
        case 2:
        case 6:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "obm3000a", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
            }
            break;
        default:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "obm3000a", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                    ((cModel*) mod)->be_flag |= 0x20;
                }
                if (e->GetMod(&mod, "obm3000f", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
                pG->flags_58 &= ~0x08000000;
            }
            break;
        }
        break;
    case 2:
        r120_setTrans(1);
        pG->flags_58 &= ~0x08000000;
        break;
    case 3:
        pG->flags_51C0 |= 0x10;
        break;
    }
}

extern "C" void Evt_R120S01_Func(Event* e)
{
    void* mod;
    int skip;
    int i;

    switch (e->funcMode) {
    case 0:
        r120_setTrans(0);
        for (i = 0x17; i <= 0x1A; i++) {
            SmdSetTrans(i, 0);
        }
        break;
    case 1:
        switch (e->cut) {
        case 3:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "obm3000a", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
                if (e->GetMod(&mod, "obm3000e", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
            }
            break;
        case 5:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "obm3000f", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
            }
            break;
        case 0xE:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "obm3000a", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
                if (e->GetMod(&mod, "obm3000b", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
                if (e->GetMod(&mod, "obm3000e", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
                if (e->GetMod(&mod, "obm3000f", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
            }
            break;
        default:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "obm3000a", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
                if (e->GetMod(&mod, "obm3000b", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
                if (e->GetMod(&mod, "obm3000e", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
                if (e->GetMod(&mod, "obm3000f", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
            }
            break;
        }
        switch (e->cut) {
        case 0:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "pl0010", 0, 0) == 1) {
                    ((cModel*) mod)->be_flag |= 0x100000;
                    ((cModel*) mod)->x12F = 4;
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->lightInfo.x50 = 0x20;
                }
                if (e->GetMod(&mod, "obm3010f", 0, 0) == 1) {
                    ((cModel*) mod)->x12F = 1;
                    ((cModel*) mod)->x12C = 1;
                    Obj18CmfOn((cObj*) mod, 5);
                }
                EventCarInit(e);
                if (e->GetMod(&mod, "pl0010", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
                if (e->GetMod(&mod, "obm3010f", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
                if (e->GetMod(&mod, "evm0000", 0, 0) == 1) {
                    ((cModel*) mod)->lightInfo.x50 = 0x20;
                }
                skip = 1;
                if (!(e->status & 0x40000000)) {
                    skip = 0;
                }
                if (skip == 0) {
                    FadeSetW(0x80000002, 150, 0, 0);
                }
            }
            break;
        case 2:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "pl0010", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
                if (e->GetMod(&mod, "obm3010f", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
                if (e->GetMod(&mod, "obm3000f", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
            }
            break;
        case 3:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "pl0010", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
                if (e->GetMod(&mod, "obm3010f", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
                if (e->GetMod(&mod, "obm3000f", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
            }
            break;
        case 6:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "pl0010", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
                if (e->GetMod(&mod, "obm3010f", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                    ((cModel*) mod)->pInfo->color[3] = 0xC0;
                }
                if (e->GetMod(&mod, "obm3000f", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
            }
            break;
        case 7:
            if (e->frame == 0) {
                if (e->GetMod(&mod, "pl0010", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                }
                if (e->GetMod(&mod, "obm3010f", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag &= ~2;
                    ((cModel*) mod)->pInfo->color[3] = 0xFF;
                }
                if (e->GetMod(&mod, "obm3000f", 0, 0) == 1) {
                    Obj18CmfOn((cObj*) mod, 5);
                    ((cModel*) mod)->be_flag |= 2;
                }
            }
            break;
        case 0xE:
            if (e->frame == 370) {
                FadeSetW(2, 215, 0, 0);
            }
            break;
        }
        switch (e->cut) {
        case 2:
            EvtTexRenderCamTrans(e, 2);
            break;
        case 6:
            EvtTexRenderCamTrans(e, 6);
            break;
        }
        break;
    case 2:
        r120_setTrans(1);
        BitOff(pG->flags_58, 0x08000000);
        pG->flags_54 |= 0x400;
        break;
    case 3:
        pG->flags_51C0 |= 0x10;
        break;
    }
}

// Model flags of the car event: the passengers, the car and its wheels, the villagers.
extern "C" void EventCarInit(Event* e)
{
    void* mod;
    cModel* p;

    if (e->GetMod(&mod, "pl0000", 0, 0) == 1) {
        ((cModel*) mod)->be_flag |= 0x100000;
        ((cModel*) mod)->x12F = 1;
    }
    if (e->GetMod(&mod, "pl0700", 0, 0) == 1) {
        ((cModel*) mod)->be_flag |= 0x10;
        ((cModel*) mod)->be_flag |= 0x04000000;
        ((cModel*) mod)->be_flag |= 0x01000000;
    }
    if (e->GetMod(&mod, "pl0700a", 0, 0) == 1) {
        ((cModel*) mod)->be_flag |= 0x10;
        ((cModel*) mod)->be_flag |= 0x04000000;
        ((cModel*) mod)->be_flag |= 0x01000000;
    }
    if (e->GetMod(&mod, "obm1a00", 0, 0) == 1) {
        ((cModel*) mod)->be_flag |= 0x10;
        ((cModel*) mod)->x12F = 4;
        p = ((cModel*) mod)->getPartsPtr(0x12);
        if (p) {
            p->scale.x = 0.0f;
            p->scale.y = 0.0f;
            p->scale.z = 0.0f;
        }
        p = ((cModel*) mod)->getPartsPtr(0x13);
        if (p) {
            p->scale.x = 0.0f;
            p->scale.y = 0.0f;
            p->scale.z = 0.0f;
        }
        p = ((cModel*) mod)->getPartsPtr(0x14);
        if (p) {
            p->scale.x = 0.0f;
            p->scale.y = 0.0f;
            p->scale.z = 0.0f;
        }
        p = ((cModel*) mod)->getPartsPtr(0x15);
        if (p) {
            p->scale.x = 0.0f;
            p->scale.y = 0.0f;
            p->scale.z = 0.0f;
        }
        p = ((cModel*) mod)->getPartsPtr(0x16);
        if (p) {
            p->scale.x = 0.0f;
            p->scale.y = 0.0f;
            p->scale.z = 0.0f;
        }
        p = ((cModel*) mod)->getPartsPtr(0x17);
        if (p) {
            p->scale.x = 0.0f;
            p->scale.y = 0.0f;
            p->scale.z = 0.0f;
        }
        p = ((cModel*) mod)->getPartsPtr(0xF);
        if (p) {
            p->scale.x = 0.0f;
            p->scale.y = 0.0f;
            p->scale.z = 0.0f;
        }
    }
    if (e->GetMod(&mod, "obm3000a", 0, 0) == 1) {
        ((cModel*) mod)->x12F = 1;
        ((cModel*) mod)->x12C = 1;
        ((cModel*) mod)->lightInfo.x50 = 2;
    }
    if (e->GetMod(&mod, "obm3000b", 0, 0) == 1) {
        ((cModel*) mod)->x12F = 1;
        ((cModel*) mod)->x12C = 1;
        ((cModel*) mod)->lightInfo.x50 = 2;
    }
    if (e->GetMod(&mod, "obm3000c", 0, 0) == 1) {
        ((cModel*) mod)->x12F = 1;
        ((cModel*) mod)->x12C = 1;
        ((cModel*) mod)->lightInfo.x50 = 2;
    }
    if (e->GetMod(&mod, "obm3000d", 0, 0) == 1) {
        ((cModel*) mod)->x12F = 1;
        ((cModel*) mod)->x12C = 1;
        ((cModel*) mod)->lightInfo.x50 = 2;
    }
    if (e->GetMod(&mod, "obm3000e", 0, 0) == 1) {
        ((cModel*) mod)->x12F = 1;
        ((cModel*) mod)->x12C = 1;
        ((cModel*) mod)->lightInfo.x50 = 2;
    }
    if (e->GetMod(&mod, "obm3000f", 0, 0) == 1) {
        ((cModel*) mod)->x12F = 1;
        ((cModel*) mod)->x12C = 1;
        ((cModel*) mod)->lightInfo.x50 = 2;
    }
}

// Registers the car window and the driver for the render-to-texture mirror.
extern "C" void EvtTexRenderCamTrans(Event* e, int cut)
{
    void* mod;
    int skip;

    skip = 1;
    if (!(e->status & 0x40000000)) {
        skip = 0;
    }
    if (skip == 0) {
        if (e->GetMod(&mod, "obm3010f", 0, 0) == 1) {
            ((cModel*) mod)->be_flag |= 2;
            TexRenderModAddOtMirror(0x10, (cModel*) mod);
            ((cModel*) mod)->be_flag &= ~2;
        }
        if (e->GetMod(&mod, "pl0010", 0, 0) == 1) {
            ((cModel*) mod)->be_flag |= 2;
            TexRenderModAddOt(0, (cModel*) mod);
            ((cModel*) mod)->be_flag &= ~2;
        }
    }
}
