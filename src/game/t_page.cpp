#include "types.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "gx_sub.h"
#include "t_util.h"

void t_page_init();
void t_page_main();
void t_page_exit();

u8 tp_exit_flg;
u8 tp_page_bak;

void ToolDebugPage()
{
    t_page_init();
    while (tp_exit_flg == 0) {
        t_page_main();
        TaskSleep(1);
    }
    t_page_exit();
}

void t_page_init()
{
    TutilInitDefault();
    tp_exit_flg = 0;
    tp_page_bak = pG->debug_mode = pG->debug_disp;
}

void t_page_main()
{
    s8 page;

    if (Joy[0].rep & JOY_LEFT) {
        pG->debug_mode--;
    }
    if (Joy[0].rep & JOY_RIGHT) {
        pG->debug_mode++;
    }
    pG->debug_mode = (s8) pG->debug_mode < 0 ? 24 : ((s8) pG->debug_mode > 24 ? 0 : pG->debug_mode);
    if (Joy[0].trg & (JOY_A | JOY_B)) {
        pG->debug_disp = pG->debug_mode;
        tp_exit_flg = 1;
    }
    if ((s8) pG->debug_mode == 0) {
        bio4_GXSetCopyClear(GXColor(), 0xFFFFFF);
    } else {
        bio4_GXSetCopyClear(g_sysBgColor, 0xFFFFFF);
    }
    eprintf2(40, 40, 130, 300, 0, 0, "PAGE %d", (s8) pG->debug_mode);
}

void t_page_exit()
{
    TutilQuitDefault();
    TaskExit();
}
