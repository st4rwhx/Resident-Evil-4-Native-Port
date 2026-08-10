#include "types.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "db_log.h"
#include "main_mem.h"
#include "t_util.h"


// Log viewer tool work (0x520 bytes).
struct DbLogInfo {
    u8 pad_0[0x2C];
    JOY joy[2];    // 0x2C
    u8 count;      // 0x4FC  frames since the last pad input
    u8 active;     // 0x4FD  0 = leave the tool
    u8 pad_4FE[4];
    u8 x502;       // 0x502
    u8 pad_503[0x520 - 0x503];
};

DbLogInfo dbInfo;
static DbLogInfo* pT;

void toolInit();
int LogMove();
void toolQuit();

void ToolLogView()
{
    TaskSuspend(0);
    TaskSleep(1);
    TutilInitDefault();
    toolInit();
    while (LogMove()) {
        TaskSleep(1);
    }
    toolQuit();
    TutilQuitDefault();
    TaskSignal(0);
    TaskExit();
}

void toolInit()
{
    pLog->modeSet(0x30, 0x2A, 0xFF, 0x19);
    pLog->on(0xFF);
    pT = &dbInfo;
    memclr_asm(&dbInfo, sizeof(dbInfo));
    pT->active = 1;
    pT->x502 = 0;
}

int LogMove()
{
    eprintf(24, 14, 0, 0, "LOG VIEWER [Ver.%s %s]", "Nov 25 2004", "10:19:49");
    JOY_COPY(pT, 0x2C, 0);
    JOY_COPY(pT, 0x2C + sizeof(JOY), 1);
    pT->count++;
    if (Joy[0].rep != 0) {
        pT->count = 0;
    }
    if (pT->joy[0].rep & JOY_UP) {
        pLog->scrSet(pT->joy[0].on & JOY_A ? -5 : -1);
    }
    if (pT->joy[0].rep & JOY_DOWN) {
        pLog->scrSet(pT->joy[0].on & JOY_A ? 5 : 1);
    }
    pLog->scrSet(-(Joy[0].sy / 20));
    pLog->dispLineNum(24, 42);
    if (pT->joy[0].rep & JOY_B) {
        pT->active = 0;
    }
    return pT->active;
}

void toolQuit()
{
    pLog->modeReset();
}
