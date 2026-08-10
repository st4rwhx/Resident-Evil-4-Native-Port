#include "types.h"
#include "global.h"
#include "gx.h"
#include "os_vi.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "card.h"
#include "main.h"
#include "main_mem.h"
#include "main_sub.h"
#include "joy.h"
#include "scheduler.h"
#include "fade.h"
#include "mes.h"
#include "tv_mode.h"

#line 22 "D:/Bio4/Prog/tv_mode.cpp"

TvModeWork* pTv;
u8 tv_mode_cnt;

void SetTvMode(GXRenderModeObj* rmode)
{
    pTv = (TvModeWork*) MEM_CALLOC(sizeof(TvModeWork), 1, 0xD);
    pTv->rmode = rmode;
    switch (VIGetTvFormat()) {
    case 0:
    case 2:
        if (pRK->progressive == 0) {
            rmode->viTVmode = 0;
            rmode->xFBmode = 1;
        } else {
            rmode->viTVmode = 2;
            rmode->xFBmode = 0;
        }
        break;
    case 1:
        rmode->viTVmode = 4;
        break;
    case 5:
        rmode->viTVmode = 0x14;
        break;
    default:
#line 46
        OSPanic(__FILE__, __LINE__, "invalid TV format\n");
        break;
    }
    pTv->active = 1;
    TaskExec(1, tvModeCheckTask, 0);
}

void tvModeCheckTask()
{
    static void (*tvModeFuncTbl[3])(TvModeWork*) = {tvModeTrigger, tvModeMenu_progressive, tvModeExit};
    u32 c0 = 0x00000000;
    u32 c1 = 0x000000FF;

    FadeSet(0, (GXColor*) &c0, (GXColor*) &c1, 0, 0, 0);
    tv_mode_cnt = 0;
    pTv->state = 0;
    while (1) {
        tvModeFuncTbl[pTv->state](pTv);
        TaskSleep(1);
    }
}

void tvModeTrigger(TvModeWork* tv)
{
    if (VIGetDTVStatus() != 0 && pRK->tv_mode_done == 0) {
        if (Joy[0].x8 == -3 || Joy[0].x8 == -2) {
            tv_mode_cnt++;
            if (tv_mode_cnt <= 29) {
                return;
            }
        }
        if (OSGetProgressiveMode() == 1 || (Joy[0].on & JOY_B)) {
            systemVISetBlack(0);
            tv->state = 1;
        } else {
            tv->state = 2;
        }
    } else {
        tv->state = 2;
    }
    pRK->tv_mode_done = 1;
}

void tvModeMenu_progressive(TvModeWork* tv)
{
    int i;
    u32 timer;
    s8 sel;
    u8 old;
    MesWork* w;
    MessageControl* mes;

    switch (tv->sub) {
    case 0:
        MesData.ptr[tv->sub] = (u8*) (pG->pArc->ofs_70 + (u32) pG->pArc);
        cMes.setLayout(0, 5);
        cMes.MesSet(0, 100, 220, 0x1000051, 0, 0, 1);
        timer = 0;
        w = cMes.getWork();
        if ((sel = w->result) == 0) {
            do {
                timer++;
                if (Joy[0].trg & 0x30003) {
                    timer = 0;
                }
                if (timer > 300) {
                    sel = cMes.mes[0].cursor + 1;
                    break;
                }
                TaskSleep(1);
            } while ((sel = w->result) == 0);
        }
        old = pRK->progressive;
        if (sel == 1) {
            pRK->progressive = 1;
            OSSetProgressiveMode(1);
        } else {
            pRK->progressive = 0;
            OSSetProgressiveMode(0);
        }
        if (pRK->progressive != old) {
            if (pRK->progressive == 1) {
                tv->rmode->viTVmode = 2;
                tv->rmode->xFBmode = 0;
            } else {
                tv->rmode->viTVmode = 0;
                tv->rmode->xFBmode = 1;
            }
            systemVISetBlack(1);
            VIFlush();
            VIConfigure(tv->rmode);
            VIFlush();
            TaskSleep(100);
            systemVISetBlack(0);
        }
        tv->sub++;
        break;
    case 1:
        if (OSGetProgressiveMode() == 1) {
            cMes.MesSet(1, 100, 220, 0x1000051, 0, 0, 1);
        } else {
            cMes.MesSet(2, 100, 220, 0x1000051, 0, 0, 1);
        }
        if (Key.trg & 0x80000000) {
            mes = &cMes;
            for (i = 0; i < 16; i++) {
                mes->Delete(i);
            }
            tv->state = 2;
            tv->sub = 0;
        }
        break;
    }
}

void tvModeExit(TvModeWork* tv)
{
    tv->active = 0;
    CardFirstCheck();
}
