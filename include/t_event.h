#ifndef T_EVENT_H
#define T_EVENT_H

#include "types.h"
#include "joy.h"
#include "hermite.h"

// Event tool (t_event REL, D:/Bio4/Prog/t_event.cpp): the ToolEvt task object, its preview state
// machine and the fog / focus (Hermite curve) and message (cDbgToolMain) sub tools.

class Event;
class cLightTool;
struct DbSctrlWork;

// One Hermite curve of the fog / focus data (64 keys), as game/event.cpp has it.
struct EvtCurve {
    s32 num;
    HermiteKey key[64];
};

struct EvtFogData {
    EvtCurve start;    // 0x000
    EvtCurve end;      // 0x404
};

struct EvtFocusData {
    EvtCurve near_;    // 0x000
    EvtCurve far_;     // 0x404
    f32 nearLevel;     // 0x808
    f32 farLevel;      // 0x80C
};

// Message list of the event (the tool's work, 0x964 bytes inside a 1,000,000-byte block): the
// cDbgEditWindow<EventMessageData::MessElem> rows.
struct EventMessageData {
    struct MessElem {
        int flag;      // 0x00  bit 0: in use
        int no;        // 0x04  row number
        int cutNo;     // 0x08
        int frame;     // 0x0C
        int messNo;    // 0x10  -1: continue the previous message
        int timer;     // 0x14
    };
    MessElem elem[100];  // 0x000
    int num;             // 0x960
};

// The first 0x40 bytes of the event file (copied into the tool and into EvtDebug at load).
struct EvtHdrCopy {
    u32 w[16];
};

class ToolEvt {
public:
    s16 mode;             // 0x00  main routine (0 menu, 1 preview, 2 exit)
    s16 step;             // 0x02  preview step
    s16 x04;              // 0x04
    s16 x06;              // 0x06
    s16 subMode;          // 0x08  preview sub routine (0 menu, 1 fog, 2 focus)
    s16 x0A;              // 0x0A
    s16 x0C;              // 0x0C
    s16 x0E;              // 0x0E
    s16 stopWait;         // 0x10  frames the start button is held before the stop toggles
    s16 startWait;        // 0x12  frames before the event starts
    u32 flags;            // 0x14
    u32 x18;              // 0x18
    u32 x1C;              // 0x1C
    int capCnt;           // 0x20  capture frame counter
    u8 curveNo;           // 0x24  edited curve (0 start / near, 1 end / far)
    s8 menuCur;           // 0x25  main menu cursor
    s8 subCur;            // 0x26  preview menu cursor
    s8 fogCur;            // 0x27  fog menu cursor
    s8 focusCur;          // 0x28  focus menu cursor
    u8 pad_29[3];
    void* pEvd;           // 0x2C  event file (8,000,000 bytes)
    u8 camMode;           // 0x30  debug camera on
    u8 camCnt;            // 0x31
    char fileName[0x22];  // 0x32  selected file name
    EvtHdrCopy hdr;       // 0x54
    u8 pad_94[4];
    cLightTool* pLightTool;  // 0x98
    JOY* pJoy0;           // 0x9C  &Joy[0] (&Joy[2] while a sub tool runs)
    JOY* pJoy1;           // 0xA0  &Joy[1] (&Joy[3])
    EvtFogData fog;       // 0xA4
    EvtFocusData focus;   // 0x8AC
    DbSctrlWork* pSctrl;  // 0x10BC  (1,000,000 bytes)
    u32 x10C0[8];         // 0x10C0
    u8 pad_10E0[0x28];
    u32 x1108;            // 0x1108
    u8 pad_110C[4];
    EventMessageData* pMess;  // 0x1110  (1,000,000 bytes)
    char room[0x10];      // 0x1114  "r10b" of the file name
    char no[0x10];        // 0x1124  "s10"
    u8 pad_1134[4];

    ToolEvt();
    ~ToolEvt();
    void EvtTaskSuspend(int task);
    void EvtTaskSignal(int task);
    void Run();
    void RunStop(ToolEvt* t, Event* ev);
    static void MainMenu(ToolEvt* t);
    static void MainPreview(ToolEvt* t);
    static void MainExit(ToolEvt* t);
    void EventDel(Event* ev);
    static void SubMenuMain(ToolEvt* t, Event* ev);
    static void SubMenuFog(ToolEvt* t, Event* ev);
    static void SubMenuFocus(ToolEvt* t, Event* ev);
    int SubMenuSelectYesNo(ToolEvt* t, const char* s1, const char* s2);
    int SubMenuEditFocusLevel(ToolEvt* t, Event* ev, const char* name, f32* level);
    int SubToolCameraMove(ToolEvt* t);
    void SubToolLightInit(ToolEvt* t, int sw);
    void SubToolLightMove(ToolEvt* t);
    int SubToolFogWkInit(ToolEvt* t, Event* ev);
    void SubToolFogInit(ToolEvt* t, int sw, Event* ev, int which);
    void SubToolFogMove(ToolEvt* t, Event* ev);
    void SubToolFocusWkInit(ToolEvt* t, Event* ev);
    void SubToolFocusInit(ToolEvt* t, int sw, Event* ev, int which);
    void SubToolFocusMove(ToolEvt* t, Event* ev);
    void SubToolMessInit(ToolEvt* t, int sw);
    void SubToolMessMove(ToolEvt* t, Event* ev);
    void SubToolIn(ToolEvt* t, int sw, int bit);
    void SctrlToolInit(ToolEvt* t, Hermite1* curve, f32 xMax, f32 yMax);
};

void ToolEvent();

#endif
