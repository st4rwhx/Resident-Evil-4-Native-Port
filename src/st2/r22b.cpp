#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "flag_rsf.h"
#include "event.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "fade.h"

// Room 2-2B (D:/Bio4/Prog/r22b.cpp): the s00 event (end of chapter 2-3) and its fades.

struct R22bWork {
    u8 dummy;
};

static R22bWork* r22b_work;

extern "C" void R22bEventS00();
extern "C" void Evt_R22bS00_Func(Event* e);

// 1 while the event is being skipped (EVT status bit 30).
static inline int r22b_evtSkip(Event* e)
{
    int skip = 1;

    if ((e->status & 0x40000000) == 0) {
        skip = 0;
    }
    return skip;
}

void R22bInit()
{
#line 33 "D:/Bio4/Prog/r22b.cpp"
    r22b_work = (R22bWork*) MEM_CALLOC(sizeof(R22bWork), 1, 0xd);
    EvtMgr.SetFunc("evt_r22bs00_func", (void*) Evt_R22bS00_Func);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        SceExec(0x12, (TaskFunc) R22bEventS00, 0, 2, 2, 0);
        EvtMgr.EvtReadAram("event/evd/r22bs00.evd", 0, 0, 0, 0);
    }
}

void R22bMain()
{
}

extern "C" void R22bEventS00()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        pG->flags_54 |= 0x400;
        EvtMgr.EvtReadExec("event/evd/r22bs00.evd", 0, 0);
        SceSetChapterEnd(0xA, 0);
        pG->flags_54 |= 0x400;
    }
}

extern "C" void Evt_R22bS00_Func(Event* e)
{
    if (e->funcMode == 1) {
        switch (e->cut) {
        case 0:
            if (e->frame == 0) {
                int skip = r22b_evtSkip(e);

                if (skip == 0) {
                    FadeSetW(0x80000002, 25, 0, 0);
                }
            }
            if (e->frame == 0x4E) {
                int skip = r22b_evtSkip(e);

                if (skip == 0) {
                    FadeSetW(2, 12, 0, 0);
                }
            }
            break;
        case 1:
            if (e->frame == 0) {
                int skip = r22b_evtSkip(e);

                if (skip == 0) {
                    FadeSetW(0x80000002, 12, 0, 0);
                }
            }
            if (e->frame == 0x7B) {
                int skip = r22b_evtSkip(e);

                if (skip == 0) {
                    FadeSetW(2, 6, 0, 0);
                }
            }
            break;
        case 2:
            if (e->frame == 0) {
                int skip = r22b_evtSkip(e);

                if (skip == 0) {
                    FadeSetW(2, 0, 0, 0);
                }
            }
            if (e->frame == 6) {
                int skip = r22b_evtSkip(e);

                if (skip == 0) {
                    FadeSetW(0x80000002, 5, 0, 0);
                }
            }
            break;
        case 5:
            if (e->frame == 0x59) {
                int skip = r22b_evtSkip(e);

                if (skip == 0) {
                    FadeSetW(2, 25, 0, 0);
                }
            }
            break;
        }
    }
}
