#include "types.h"
#include "light.h"
#include "map_obj.h"
#include "widget.h"
#include "atari.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "camera.h"
#include "db_cam.h"
#include "gx_sub.h"
#include "main_mem.h"
#include "t_util.h"
#include "db_mod.h"
#include "motion.h"

// Motion viewer debug tool (Tools/t_mv.cpp): a three-step menu (init / main / quit through mvFunc) around
// db_mod's model viewer, with the debug camera on the Z button.

void drawGround(int on);
int SetToolLight(int no);      // db_light_tools.cpp exports it (asm .globl; static in db_light.cpp)
void ToolArrayPush(int flag);  // tools.cpp linkonce tail
void ToolWorkPop(int flag);

struct MvWork {
    s8 step;      // 0x00  index into mvFunc
    s8 cursor;    // 0x01  menu cursor (0: the model, 1: the exit prompt)
    u8 pad_2[5];
    u8 exitSel;   // 0x07  exit prompt answer (1 = yes)
    u8 camMode;   // 0x08  1 = the debug camera has the pad
    u8 pad_9;
    s8 model;     // 0x0A  db_mod slot shown
    u8 pad_B[0x30 - 0xB];
};

static int mvInit();
static int mvMain();
static int mvQuit();

MvWork mvWork;
MvWork* pMv = &mvWork;
static int (*mvFunc[])() = {mvInit, mvMain, mvQuit};
static int mvModelMode = 0;
static int mvUnused = 0;

void ToolMotionViewer()
{
    JOY* joy = &Joy[0];

    pMv->step = pMv->cursor = 0;
    pMv->camMode = 0;
    for (;;) {
        switch (pMv->camMode) {
        case 0:
            if (mvFunc[pMv->step]() == 0) {
                if (joy->trg & 0x1000) {
                    pMv->camMode = 1;
                }
            }
            break;
        case 1:
            if (joy->trg & 0x1000) {
                pMv->camMode = 0;
            }
            CamDbg.move(&pG->Cam, joy, 1);
            break;
        }
        dbModMotionMove();
        if (dbModGetViewFlag() & 8) {
            drawGround(1);
        } else {
            drawGround(0);
        }
        TaskSleep(1);
    }
}

static int mvInit()
{
    GXColor bg;
    u8 zero = 0;

    TOOL_FLAG(OFS_STOP_FLG) |= 0x10000000;
    TOOL_FLAG(OFS_DISP_FLG) |= 0x2000000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x800000;
    TOOL_FLAG(OFS_SYSTEM_FLG) &= ~0x800;
    TOOL_FLAG(OFS_DEBUG_FLG) |= 0x10000000;
    ToolArrayPush(0);
    bg.r = bg.g = bg.b = 0x30;
    bg.a = zero;
    bio4_GXSetCopyClear(bg, 0xFFFFFF);
    {
        Camera* cam = &pG->Cam;

        cam->param.at.x = 0.0f;
        cam->param.at.y = 1000.0f;
        cam->param.at.z = 0.0f;
        cam->param.pos.x = 0.0f;
        cam->param.pos.y = 1000.0f;
        cam->param.pos.z = 3000.0f;
        cam->param.roll = 0.0f;
        CameraSetOrientationRoll(cam);
    }
    TutilInitDefault();
    dbModelInit();
    SetToolLight(2);
    memclr_asm(pMv, 0x18);
    // COMPILER-DIFF: candidate #12 (fallthrough-arm form). The original's then arm has its own `li r10,0`
    // for `cursor = 0`; our cse1 re-walks the entry block with the `bne` NOT_TAKEN, so a plain literal in
    // the fallthrough arm is canonicalised to the `zero` register (r29) and the arms cross-jump. The
    // code-less `do {} while (0)` puts a NOTE_INSN_LOOP_END at the arm top, which ends cse's path, so
    // the arm's zero stays its own pseudo (gcse cprop cannot store a constant).
    if (pDbModState->loaded == 0) {
        do { } while (0);
        pMv->step = 1;
        pMv->cursor = 0;
    } else {
        pMv->step = 2;
        pMv->cursor = zero;
    }
    return 0;
}

static int mvMain()
{
    JOY* joy = &Joy[0];
    int ret;

    ret = dbModel(mvModelMode);
    if (ret != 4) {
        eprintf(0x20, 0xE, 5, 0, "[MOTION VIEWER]");
    }
    switch (pMv->cursor) {
    case 0:
        mvModelMode = 0;
        switch (ret) {
        case 2:
            pMv->cursor++;
            pMv->exitSel = 0;
            mvModelMode = 1;
            break;
        case 3:
            pMv->step = 2;
            break;
        }
        break;
    case 1:
        if (joy->rep & 0x10001) {
            pMv->exitSel = 1;
        }
        if (joy->rep & 0x20002) {
            pMv->exitSel = 0;
        }
        if (joy->trg & 0x200) {
            pMv->cursor--;
        } else if (joy->trg & 0x100) {
            if (pMv->exitSel == 1) {
                pMv->step = 2;
            } else {
                pMv->cursor = 0;
            }
        } else {
            eprintf(0xC8, 0xA8, 0, 0, "EXIT: ---/---");
            if (pMv->exitSel != 0) {
                eprintf(0xF8, 0xA8, 4, 0, "YES");
            } else {
                eprintf(0x118, 0xA8, 4, 0, "NO");
            }
        }
        break;
    }
    if (ret != 4) {
        cModel* m = dbModSlot[pMv->model].pModel;

        if (m) {
            MotionWork* mot = &m->mot;

            eprintf(0xD8, 0x1A4, 0, 0, "[%3d/%3d]", (int) mot->seqFrame - 1, mot->seqMax);
        }
        return 0;
    }
    return 1;
}

static int mvQuit()
{
    GXColor bg;
    int ret = 0;

    dbModelQuit();
    ToolWorkPop(0);
    bg = g_sysBgColor;
    bio4_GXSetCopyClear(bg, 0xFFFFFF);
    TOOL_FLAG(OFS_STOP_FLG) &= ~0x10000000;
    TOOL_FLAG(OFS_DISP_FLG) &= ~0x2000000;
    TOOL_FLAG(OFS_STOP_FLG) &= ~0x800000;
    TOOL_FLAG(OFS_SYSTEM_FLG) |= 0x800;
    TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x10000000;
    TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x80000000;
    SetToolLight(-1);
    pMv->step = ret;
    TaskExit();
    return ret;
}
