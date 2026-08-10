#ifndef SCE_SYS_H
#define SCE_SYS_H

#include "types.h"
#include "em.h"
#include "scheduler.h"

// game/sce_sys.cpp: scenario task system. Scenario tasks run in scheduler slots 5..17 and are
// linked into an ordering table (libgpu OTag) by priority.

// One scenario task (0xC bytes), SceSys.prim[slot - 5]
struct ScePrim {
    u32 next;     // 0x00  OTag link
    u8 running;   // 0x04  scheduled this frame
    u8 cancel;    // 0x05  1 = this task is the event-cancel target
    u8 pad_6[2];
    TASK* task;   // 0x08
};

// Deferred scenario execution condition (0x14 bytes, SceExecOt list)
struct SceCond {
    u32 next;             // 0x00
    void (*func)();       // 0x04
    int arg;              // 0x08
    u8 prio;              // 0x0C
    u8 flag;              // 0x0D
    u8 type;              // 0x0E  0 em dead flag, 1 camera area, 2 enemy life, 3 callback, 4 etc break, 5 item flag
    u8 pad_F;
    void* param;          // 0x10
};

class cSceSys {
public:
    int wait;             // 0x00  1 = GXDrawDone before the next task
    int pause;            // 0x04  nonzero: scenario stopped
    int x8;               // 0x08
    int xC;               // 0x0C
    int x10;              // 0x10
    int x14;              // 0x14
    void (*cancelFunc)(); // 0x18  task started by SceExecEventCancel
    int cancelArg;        // 0x1C
    u32 otag[16];         // 0x20  ordering table, otag[15] is the list head
    u32 x60;              // 0x60  pG->flags_170 saved by SceUpCutStart
    u32 x64;              // 0x64  pG->flags_54 saved by SceEventStart
    u32 stop_bak;         // 0x68  pG->flags_170 before the event cancel
    u8 x6C;               // 0x6C  1 = an up-cut is running (flags_170 saved in x60)
    u8 x6D;               // 0x6D  1 = SceEventStart(0) told the managers
    u8 x6E;               // 0x6E  SceEventStart nesting count
    u8 x6F;               // 0x6F
    u8 x70;               // 0x70  task flag saved by SceEventStart / SceUpCutStart
    u8 eventCancel;       // 0x71  1 = the running event may be cancelled
    s8 cancelFlagNo;      // 0x72  flags_174 bit set when the event is cancelled (-1 = none)
    u8 x73;               // 0x73  set while readEmData waits inside a scenario task
    u8 x74;               // 0x74  chapter number (SceSetChapterEnd)
    u8 x75;               // 0x75
    u8 x76;               // 0x76
    u8 x77;               // 0x77
    s16 x78;              // 0x78  door area the chapter end returns through (-1 = none; sce_com)
    u16 x7A;              // 0x7A  0x3C, 0x5A with a sub character
    int sndFlag;          // 0x7C  1 = SndEventStrStop on event cancel
    cDmgInfo dmg;         // 0x80
    ScePrim prim[13];     // 0x98  slots 5..17
    ScePrim* x134;        // 0x134  task SceEventStart(!0) kills

    void scheduler();
    int checkCTaskRange();
};

extern cSceSys SceSys;

extern "C" {
void ScenarioInit();
void ScenarioRoomInit();
void ScenarioTaskAllOff();
void scenarioLoopBeforeInit();
void scenarioLoopAfterInit();
void ScenarioMove();
u32* scenarioSetOtStart();
u32* scenarioGetOtAddr(u32* p);
void SceTaskDelete(TASK* t);
ScePrim* SceExec(int prio, TaskFunc func, int arg, u8 flag, int otPrio, void* model);
void SceSleep(int frames);
void SceExit();
ScePrim* SceCTask();
void SceExecInitCondition();
int SceExecCheckCondition_sub(SceCond* c);
void SceExecCheckCondition();
void SceExecLinkCondition(int type, void* param, u8 prio, TaskFunc func, int arg, u8 flag);
void SceExecLinkEmDead(void* param, u8 prio, TaskFunc func, int arg, u8 flag);
int EmMoveActiveCheck(cEm* em);
void SceExecEventCancel();
void SceSetEventCancel(int on, TaskFunc func, int arg, int flagNo, int sndFlag);
int scenarioCheckEventCancel();
}

void SceKill(int prio);
void SceKill(ScePrim* p);
void SceKill(TASK* t);
void SceKill(void (*func)(int));

#endif
