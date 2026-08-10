#include "types.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "t_util.h"

extern int ScreenShotTriggerType;

void ToolScreenShot()
{
    TOOL_MENU menu[2] = {
        {1, "SCREEN SHOT TRIGGER", NULL},
        {1, "DEBUG DISP", NULL},
    };
    static const char* trg_str[3] = {"A PUSHING", "A PUSH", "A PUSH_TO_PUSH"};
    u32 stop_bak;
    s8 cursor;
    int type;
    int n;

    stop_bak = pG->flags_170;
    BitOn(pG->flags_170, ~0x4000);
    BitOn(pG->flags_60, 0x80000000);
    cursor = 0;
    while (1) {
        ToolMenuDisp_cur(60, 80, 0, &cursor, menu, sizeof(menu), Joy);
        switch (cursor) {
        case 0:
            type = ScreenShotTriggerType;
            if (Joy[0].rep2 & 0x20002) {
                type++;
            }
            if (Joy[0].rep2 & 0x10001) {
                type--;
            }
            if (type >= 0) {
                n = type;
                if (n > 2) {
                    n = 2;
                }
            } else {
                n = 0;
            }
            ScreenShotTriggerType = n;
            break;
        case 1:
            if (Joy[0].rep2 & 0x30003) {
                if (pG->debug_disp == 0) {
                    pG->debug_disp = 1;
                } else {
                    pG->debug_disp = 0;
                }
            }
            break;
        }
        eprintf(228, 80, 0, 0, "%s", trg_str[ScreenShotTriggerType]);
        eprintf(228, 96, 0, 0, "%s", pG->debug_disp == 0 ? "OFF" : "ON");
        if (Joy[0].trg & JOY_B) {
            break;
        }
        TaskSleep(1);
    }
    BitSet(pG->flags_170, stop_bak);
    BitOff(pG->flags_60, 0x80000000);
    TaskExit();
}
