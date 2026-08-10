#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"

// Dolphin OS thread/semaphore types for game code. The SDK's dolphin/types.h drags in the
// CodeWarrior libc, so its guard is defined here and the OS headers are included directly.
#define _DOLPHIN_TYPES_H_
#include <dolphin/os/OSThread.h>
#include <dolphin/os/OSSemaphore.h>

// Cooperative task scheduler (game/scheduler.cpp). One OSThread per task slot; TaskScheduler
// runs slots 0..4 once a frame, iTaskScheduler runs slot 4 from interrupt level.
#define TASK_NUM 18
#define TASK_ISR 4  // slot used by iTaskExec

// status: low 7 bits are the state, bit 7 = suspended by TaskSuspend
#define TASK_NONE 0
#define TASK_EXEC 1   // requested, thread not created yet
#define TASK_SLEEP 2  // sleeping `sleep` frames
#define TASK_RUN 3
#define TASK_SUSPEND 0x80

struct TASK {
    u8* stack;                // 0x00  stack top
    u8 status;                // 0x04
    u8 level;                 // 0x05  > 0xF: waits on the scheduler semaphore
    u8 no;                    // 0x06
    u8 flag;                  // 0x07  2: skip while flags_5010 bit 28, 4: skip while flags_500C bit 20
    u16 suspend_cnt;          // 0x08
    u16 sleep;                // 0x0A
    u16 stack_size;           // 0x0C
    u16 pad_E;
    int arg;                  // 0x10
    u8 pad_14[4];
    OSThread thread;          // 0x18
    OSThreadQueue queue;      // 0x330
    void* (*hook)(void*);     // 0x338
    void (*func)(int);        // 0x33C
    void* model;              // 0x340
    u8 pad_344[4];
};                            // 0x348

extern TASK* CTASK_MAIN;  // sentinel "main thread" task (-1)
extern TASK* pCTask;      // task currently being scheduled
extern OSThread* pParentThread;  // thread to return to from the scheduler

// Struct-member views of the same symbols (the pLog trick, db_log.h): the original reloads pCTask
// after every store through it and keeps the following pParentThread load in order, which GCC
// 2.95 only does for struct-member loads. Stores to the pointers themselves are plain
// (`stw rX, pCTask@sda21`), which a struct-member store would not give, hence the second
// declarations with asm labels.
struct TaskPtr {
    TASK* p;
    TASK* operator->() { return p; }
};
struct ThreadPtr {
    OSThread* p;
    OSThread* operator()() { return p; }  // a direct `.p` gets its address CSE'd across blocks
};
extern TaskPtr CTASK __asm__("pCTask");
extern ThreadPtr ParentThread __asm__("pParentThread");
extern TASK Task[TASK_NUM];

void TaskSleep(int frames);
void TaskExit();
void TaskSuspend(int task);
void TaskSignal(int task);
extern "C" {
// Inside extern "C" GCC 2.95 reads `void (*)()` as `void (*)(...)` and then mangles a function
// taking it by value; the same type through a typedef keeps C linkage.
typedef void (*TaskFunc)();
void TaskSchedulerInit();
void TaskAllClear();
u32 GetStackSize(int no);
void TaskScheduler();
void TaskSchedulerMain(TASK* t);
void stackUsedCheck();
void StackOverflowCheck(TASK* t);
void* TaskExec_hook(void* arg);
TASK* TaskExec(int prio, TaskFunc func, int arg);
void TaskChain(TaskFunc func, int arg);
void TaskKill(int prio);
u8 TaskStatus(int prio);
void SetTaskModelPtr(void* model, TASK* t);
// interrupt-level task variants (dvd.cpp uses them for reads flagged 0x100)
void iTaskScheduler();
TASK* iTaskExec(TaskFunc func);
void iTaskKill();
void iTaskExit();
void iTaskSuspend();
int iTaskStatus();
}

// C++ overload: kill the task `t` (sce_sys SceKill).
void TaskKill(TASK* t);

#endif
