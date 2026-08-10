#include "types.h"

// Debug tool module entry (D:/Bio4/Prog/tools.cpp; the same object ends every t_* / Tools REL): the SN REL
// entry points _prolog (ctors, then ToolsTask), _epilog (dtors) and _unresolved (HALT), and ToolsTask,
// which dispatches DebugMenuSelected to the Tool* entry of the selected tool (Tool* functions live in the
// DOL or in other tool modules). The game headers are included after the functions: their inline strings
// follow the entry points' strings in the original .rodata.

extern "C" void OSReport(const char* fmt, ...);

#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

// _ctors/_dtors: the linker script's labels on the .ctors/.dtors lists (null terminated)
extern void (*_ctors[])(void);
extern void (*_dtors[])(void);

extern int DebugMenuSelected;

void ToolOption();
void ToolMotSeq();
void ToolCamera();
void ToolLight();
void ToolEsp();
void ToolEmList();
void ToolRctRouteCheck();
void ToolAtari();
void ToolCons();
void ToolVibEdit();
void ToolScroll();
void ToolMotionViewer();
void ToolTplView();
void ToolSceAt();
void ToolInterfaceDesign();
void ToolFlrAt();
void ToolMes();
void ToolEvent();
void ToolBlock();
void ToolEspArea();
void ToolSceItem();
void ToolEmInfo();
void ToolLightArea();

void ToolsTask();

extern "C" void _prolog()
{
    void (**p)(void);

    for (p = _ctors; *p; p++) {
        (*p)();
    }
    OSReport("prolog...\n");
    ToolsTask();
}

void ToolsTask()
{
    switch (DebugMenuSelected) {
    case 5:
        ToolOption();
        break;
    case 6:
        ToolMotSeq();
        break;
    case 7:
        ToolCamera();
        break;
    case 8:
        ToolLight();
        break;
    case 9:
        ToolEsp();
        break;
    case 10:
        ToolEmList();
        break;
    case 12:
        ToolRctRouteCheck();
        break;
    case 13:
        ToolAtari();
        break;
    case 14:
        ToolCons();
        break;
    case 15:
        ToolVibEdit();
        break;
    case 16:
        ToolScroll();
        break;
    case 17:
        ToolMotionViewer();
        break;
    case 18:
        ToolTplView();
        break;
    case 19:
        ToolSceAt();
        break;
    case 20:
        ToolInterfaceDesign();
        break;
    case 21:
        ToolFlrAt();
        break;
    case 27:
        ToolMes();
        break;
    case 30:
        ToolEvent();
        break;
    case 31:
        ToolBlock();
        break;
    case 32:
        ToolEspArea();
        break;
    case 34:
        ToolSceItem();
        break;
    case 35:
        ToolEmInfo();
        break;
    case 36:
        ToolLightArea();
        break;
    }
}

extern "C" void _epilog()
{
    void (**p)(void);

    for (p = _dtors; *p; p++) {
        (*p)();
    }
    OSReport("epilog...\n");
}

extern "C" void _unresolved()
{
    OSReport("unresolved...\n");
#line 142 "D:/Bio4/Prog/tools.cpp"
    HALT();
}

#include "atari.h"
#include "light.h"
#include "event.h"
#include "ctrl.h"

#ifdef TOOLS_ARRAY
// t_id / t_esp / Tools: the work-array helpers follow _unresolved in .text, before the cManager<cLight>
// block and the arrayPush/arrayPop instantiations they pull in (the .sym lists them after tools.cpp).
#include "model.h"
#include "em.h"
#include "obj.h"
#include "esp.h"
#include "espgen.h"
#include "cons.h"

// the bits say which pools to leave alone (the callers pass the pools they do not need)
void ToolArrayPush(int flags)
{
    if (!(flags & 1)) {
        PartsMgr.arrayPush(500);
    }
    if (!(flags & 2)) {
        EmMgr.arrayPush(64);
    }
    if (!(flags & 4)) {
        ObjMgr.arrayPush(500);
    }
    if (!(flags & 8)) {
        EspArrayPush(ConsGetRoomValue(2));
    }
    if (!(flags & 0x10)) {
        EspgenArrayPush(0x80);
    }
    if (!(flags & 0x20)) {
        CtrlMgr.arrayPush(0x80);
    }
    if (!(flags & 0x80)) {
        EvtMgr.arrayPush(4);
    }
    if (!(flags & 0x100)) {
        LightMgr.arrayPush(100);
    }
}

void ToolWorkPop(int flags)
{
    if (!(flags & 1)) {
        PartsMgr.arrayPop();
    }
    if (!(flags & 2)) {
        EmMgr.arrayPop();
    }
    if (!(flags & 4)) {
        ObjMgr.arrayPop();
    }
    if (!(flags & 8)) {
        EspArrayPop();
    }
    if (!(flags & 0x10)) {
        EspgenArrayPop();
    }
    if (!(flags & 0x20)) {
        CtrlMgr.arrayPop();
    }
    if (!(flags & 0x80)) {
        EvtMgr.arrayPop();
    }
    if (!(flags & 0x100)) {
        LightMgr.arrayPop();
    }
}

#ifdef TOOLS_EM_ARRAY
void ToolEmArraySet(int on)
{
    if (on) {
        EmMgr.arrayPush(10);
    } else {
        EmMgr.arrayPop();
    }
}
#endif
#endif
