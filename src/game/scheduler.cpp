// game/scheduler: cooperative task scheduler on top of OS threads (D:/Bio4/Prog/scheduler.cpp).
#include "types.h"
#include "global.h"
#include "main_mem.h"
#include "db_log.h"
#include "eprintf.h"
#include "os_vi.h"
#include "scheduler.h"

extern "C" {
void OSReport(const char* msg, ...);
BOOL OSDisableInterrupts(void);
BOOL OSRestoreInterrupts(BOOL level);
void GXSetCurrentGXThread(void);
}
void DbMenuRestoreStopFlag();

TASK Task[TASK_NUM];
static OSSemaphore Sema;
TASK* CTASK_MAIN = (TASK*) -1;
TASK* pCTask = CTASK_MAIN;
OSThread* pParentThread;
static int iTask_exec_flg = 0;

void TaskKill(TASK* t);

void TaskSchedulerInit()
{
    u32 total = 0;
    u8* stack;
    u32 i;

    for (i = 0; i < TASK_NUM; i++) {
        Task[i].stack_size = GetStackSize(i);
        total += Task[i].stack_size;
    }
#line 48 "D:/Bio4/Prog/scheduler.cpp"
    stack = (u8*) MEM_ALLOC(total, 1, 13);
    memset_asm(stack, 0xB3, total);
    for (i = 0; i < TASK_NUM; i++) {
        Task[i].no = i;
        Task[i].status = 0;
        Task[i].stack = stack + Task[i].stack_size;
        Task[i].suspend_cnt = 0;
        OSInitThreadQueue(&Task[i].queue);
        stack = Task[i].stack;
    }
    OSInitSemaphore(&Sema, 0);
    iTask_exec_flg = 0;
}

void TaskAllClear()
{
    u32 i;

    for (i = 0; i < TASK_NUM; i++) {
        TaskKill(i);
    }
    iTask_exec_flg = 0;
}

u32 GetStackSize(int no)
{
    if (no == 2) {
        return 0x3000;
    }
    if ((u32) no > 4) {
        return 0x1800;
    }
    return 0x2000;
}

void TaskScheduler()
{
    u32 i;
    TASK* t;

    pParentThread = OSGetCurrentThread();
    for (i = 0, t = Task; i <= TASK_ISR; i++, t++) {
        if (i == TASK_ISR) {
            continue;
        }
        if (i == 2) {
            DbMenuRestoreStopFlag();
        }
        pCTask = t;
        TaskSchedulerMain(t);
    }
    pCTask = CTASK_MAIN;
    if (pG->debug_mode == 6) {
        stackUsedCheck();
    }
}

void TaskSchedulerMain(TASK* t)
{
    if ((pG->flags_5010 & 0x10000000) && !(t->flag & 2)) {
        return;
    }
    if ((pG->flags_500C & 0x100000) && !(t->flag & 4)) {
        return;
    }
    switch (t->status) {
    case TASK_EXEC:
        OSCreateThread(&t->thread, t->hook, (void*) t->arg, t->stack, t->stack_size, t->level, 1);
        t->status = TASK_RUN;
        OSResumeThread(&t->thread);
        GXSetCurrentGXThread();
        break;
    case TASK_SLEEP:
        t->sleep--;
        if (t->sleep != 0) {
            return;
        }
        t->status = TASK_RUN;
        OSWakeupThread(&t->queue);
        GXSetCurrentGXThread();
        break;
    case TASK_RUN:
        OSResumeThread(&t->thread);
        GXSetCurrentGXThread();
        break;
    default:
        return;
    }
    if (CTASK->level > 0xF) {
        OSWaitSemaphore(&Sema);
        GXSetCurrentGXThread();
    }
    StackOverflowCheck(t);
}

void stackUsedCheck()
{
    u32 i;
    int y = 0x24;

    eprintf2(9, 0x10, 0x22, 0x24, 0, 6, "   SIZE REST");
    for (i = 0; i < TASK_NUM; i++) {
        u32* p = (u32*) (Task[i].stack - Task[i].stack_size);
        u32 n = 1;
        p++;
        while (n < (u32) (Task[i].stack_size >> 2) && *p == 0xB3B3B3B3) {
            n++;
            p++;
        }
        y += 0x10;
        eprintf(0x10, y, 0, 6, "%02d %4x %4x %08x", i, Task[i].stack_size, n * 4, Task[i].stack - Task[i].stack_size);
    }
}

void StackOverflowCheck(TASK* t)
{
    if (*(u32*) (t->stack - t->stack_size) != 0xDEADBABE) {
        OSReport("***************************************\n");
        OSReport("Stack overflow in Thread %d !!\n", t->no);
        OSReport("***************************************\n");
        OSPanic("D:/Bio4/Prog/scheduler.cpp", 217, "End of biohazard4");
    }
}

void* TaskExec_hook(void* arg)
{
    if (ParentThread() != NULL) {
        OSSuspendThread(ParentThread());
    }
    asm("li 3, 4\n"
        "oris 3, 3, 4\n"
        "mtspr 914, 3\n"
        "li 3, 5\n"
        "oris 3, 3, 5\n"
        "mtspr 915, 3\n"
        "li 3, 6\n"
        "oris 3, 3, 6\n"
        "mtspr 916, 3\n"
        "li 3, 7\n"
        "oris 3, 3, 7\n"
        "mtspr 917, 3"
        :
        :
        : "r3");
    GXSetCurrentGXThread();
    CTASK->func((int) arg);
    return NULL;
}

TASK* TaskExec(int prio, TaskFunc func, int arg)
{
    TASK* t;

    if (TaskStatus(prio) != 0) {
        pLog->err(0, 0, "TASK DON'T EXEC : level %d", prio);
        return NULL;
    }
    t = &Task[prio];
    t->hook = TaskExec_hook;
    t->func = (void (*)(int)) func;
    t->status = TASK_EXEC;
    t->arg = arg;
    t->flag = 6;
    t->level = 0xF;
    return t;
}

void TaskSleep(int frames)
{
    if (frames == 0) {
        return;
    }
    CTASK->sleep = frames;
    CTASK->status = (CTASK->status & TASK_SUSPEND) | TASK_SLEEP;
    if (ParentThread() != NULL) {
        OSResumeThread(ParentThread());
    }
    if (CTASK->level > 0xF) {
        OSSignalSemaphore(&Sema);
    }
    OSSleepThread(&pCTask->queue);
    if (ParentThread() != NULL) {
        OSSuspendThread(ParentThread());
    }
    GXSetCurrentGXThread();
}

void TaskChain(TaskFunc func, int arg)
{
    CTASK->hook = TaskExec_hook;
    CTASK->func = (void (*)(int)) func;
    CTASK->status = TASK_EXEC;
    CTASK->arg = arg;
    if (ParentThread() != NULL) {
        OSResumeThread(ParentThread());
    }
    if (CTASK->level > 0xF) {
        OSSignalSemaphore(&Sema);
    }
    OSExitThread(&pCTask->thread);
}

void TaskExit()
{
    TASK* t = pCTask;

    t->status = TASK_NONE;
    t->suspend_cnt = 0;
    if (ParentThread() != NULL) {
        OSResumeThread(ParentThread());
    }
    if (CTASK->level > 0xF) {
        OSSignalSemaphore(&Sema);
    }
    OSExitThread(&pCTask->thread);
}

void TaskKill(int prio)
{
    TaskKill(&Task[prio]);
}

void TaskKill(TASK* t)
{
    if (t->status == 0) {
        return;
    }
    switch (t->status & ~TASK_SUSPEND) {
    case TASK_NONE:
    case TASK_EXEC:
        break;
    case TASK_SLEEP:
        OSCancelThread(&t->thread);
        break;
    case TASK_RUN:
        if (t->status & TASK_SUSPEND) {
            OSCancelThread(&t->thread);
        } else {
            TaskExit();
        }
        break;
    }
    t->status = TASK_NONE;
    t->suspend_cnt = 0;
}

void TaskSuspend(int task)
{
    TASK* t = &Task[task];

    t->suspend_cnt++;
    t->status |= TASK_SUSPEND;
}

void TaskSignal(int task)
{
    TASK* t = &Task[task];

    if (t->suspend_cnt == 0) {
        return;
    }
    t->suspend_cnt--;
    if (t->suspend_cnt != 0) {
        return;
    }
    t->status &= ~TASK_SUSPEND;
}

u8 TaskStatus(int prio)
{
    return Task[prio].status;
}

void SetTaskModelPtr(void* model, TASK* t)
{
    if (t == NULL) {
        CTASK->model = model;
    } else {
        t->model = model;
    }
}

void iTaskScheduler()
{
    BOOL lv = OSDisableInterrupts();

    if (iTask_exec_flg == 1) {
        TASK* t = &Task[TASK_ISR];
        pParentThread = NULL;
        pCTask = t;
        t->status &= ~TASK_SUSPEND;
        TaskSchedulerMain(t);
        pCTask = CTASK_MAIN;
    }
    OSRestoreInterrupts(lv);
}

TASK* iTaskExec(TaskFunc func)
{
    TASK* t;
    int flg = iTask_exec_flg;

    if (flg == 0) {
        iTask_exec_flg = 1;
        t = TaskExec(TASK_ISR, func, 0);
        t->level = flg;
        return t;
    }
    return NULL;
}

void iTaskKill()
{
    iTask_exec_flg = 0;
    TaskKill(&Task[TASK_ISR]);
}

void iTaskExit()
{
    iTask_exec_flg = 0;
    TaskExit();
}

void iTaskSuspend()
{
    if (iTask_exec_flg == 1) {
        TASK* t = &Task[TASK_ISR];
        if (t->thread.state == 2) {
            t->status |= TASK_SUSPEND;
            OSSuspendThread(&t->thread);
        }
    }
}

int iTaskStatus()
{
    return iTask_exec_flg;
}
