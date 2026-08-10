#include "types.h"
#include "event.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "global.h"
#include "main.h"
#include "main_mem.h"
#include "scheduler.h"
#include "libgpu.h"
#include "room_data.h"
#include "cam_ctrl.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_npc.h"
#include "em_set.h"
#include "mes.h"
#include "snd.h"
#include "fade.h"
#include "pad.h"
#include "db_log.h"
#include "sce_sys.h"

extern "C" {
void SceInitItemEvent();                       // game/sce_com.cpp
void SceAtSetSaveItem();                       // game/sce_at.cpp
void SceAtRoomSet();
void SceAtCheckMoveScrAt();
void* SceAtPtr(int no);
void SubMissionCheck();                        // game/stage.cpp
int getRoomEtcBreak(void* p, cEm** em, int a); // game/EtcModel.cpp
void SubScreenWait(int frames);                // game/sscrn.cpp
void GXDrawDone();
}

int SceAtItemFlgCk(int no);  // game/sce_at.cpp (C++ overload set)

#line 34 "D:/Bio4/Prog/sce_sys.cpp"

// Reference store of a SceSys byte: the inline's `&member` reaches the MEM as a CONST address and folds
// into `stb rX,SceSys+N@l(rH)` (SceSetEventCancel's first store); a plain `SceSys.x = v` legitimises
// `&SceSys` into a lo_sum pseudo first and stores through `N(rP)`.
static inline void U8SetI(u8& d, int v) { d = v; }
// Event flag words at pG->flags_174, indexed by flag number; as an inline the base stays a pointer
// register (lwzx/stwx) instead of folding into the displacement.
static inline u32* eventFlags()
{
    return &pG->flags_174;
}

cSceSys SceSys;
static ScePrim* pCSceTask;
// Struct-member view of pCSceTask (the pLog trick): the store keeps the following p->task load below it.
struct ScePrimPtr {
    ScePrim* p;
};
#define CSCE_TASK (((ScePrimPtr*) &pCSceTask)->p)
static u32 SceExecOt;

void ScenarioInit()
{
    memclr_asm(&SceSys, sizeof(cSceSys));
}

void ScenarioRoomInit()
{
    // Store order decides the zero registers and the schedule: the six byte zeros come first, so cse
    // makes their QImode pseudo before any SImode zero exists (a later word zero's low part would
    // otherwise replace it), eventCancel is the last use of that pseudo and pause the last use of
    // the word zero (sched1 issues the dying store of each group first), and the sched2 anti-
    // dependence of the byte stores on the pG load's r9 ranks their group and its `li` last.
    SceSys.x6D = 0;
    SceSys.x6C = 0;
    SceSys.x6E = 0;
    SceSys.x6F = 0;
    SceSys.x70 = 0;
    SceSys.eventCancel = 0;
    SceSys.sndFlag = 1;
    SceSys.cancelFlagNo = -1;
    SceSys.x8 = 0;
    SceSys.xC = 0;
    SceSys.x10 = 0;
    SceSys.x14 = 0;
    SceSys.cancelFunc = 0;
    SceSys.cancelArg = 0;
    SceSys.x134 = 0;
    SceSys.x75 = 0;
    SceSys.x76 = 0;
    SceSys.pause = 0;
    pGS->flags_51BC &= ~0x80;
    ScenarioTaskAllOff();
    SceInitItemEvent();
    SceAtSetSaveItem();
    if (!(pG->flags_68 & 0x4000000)) {
        SceExecInitCondition();
        RoomData.execInitFunc(pG->room_id);
        SceSys.scheduler();
    }
    if (pG->flags_5018 & 0x4000000) {
        SubCharInit(1, &pG->sub_pos, pG->sub_angle);
        SubCharCtrl(1, 0);
    }
    EmSetFromList();
    SceAtRoomSet();
}

void ScenarioTaskAllOff()
{
    u32 i;

    ClearOTagR(SceSys.otag, 16);
    memclr_asm(SceSys.prim, sizeof(SceSys.prim));
    for (i = 5; i <= 17; i++) {
        SceKill((int) i);
    }
}

void scenarioLoopBeforeInit()
{
    SceSys.x7A = 0x3C;
    if (pSUB != 0) {
        SceSys.x7A = 0x5A;
    }
    SceExecCheckCondition();
}

void scenarioLoopAfterInit()
{
    SubMissionCheck();
    SceAtCheckMoveScrAt();
}

void ScenarioMove()
{
    if (pG->flags_68 & 0x4000000) {
        return;
    }
    scenarioLoopBeforeInit();
    if (SceSys.x76 == 0 && SceSys.pause == 0) {
        if (!(pG->flags_500C & 0x100000)) {
            scenarioCheckEventCancel();
            RoomData.execMainFunc(pG->room_id);
        }
        if (!(pG->flags_170 & 0x400) || (pG->flags_6C & 0x80)) {
            EvtMgr.Run();
        }
    }
    SceSys.scheduler();
    scenarioLoopAfterInit();
}

u32* scenarioSetOtStart()
{
    return &SceSys.otag[15];
}

u32* scenarioGetOtAddr(u32* p)
{
    u32 v;

    while ((v = *p) != 0xFFFFFFFF) {
        p = (u32*) (v | 0x80000000);
        if ((s32) v < 0) {
            return p;
        }
    }
    return 0;
}

void SceTaskDelete(TASK* t)
{
    ScePrim* p = (ScePrim*) scenarioSetOtStart();

    while ((p = (ScePrim*) scenarioGetOtAddr((u32*) p)) != 0) {
        if (p->task == t) {
            DelPrim(&SceSys.otag[15], (u32*) p);
            if (p->cancel == 1) {
                p->cancel = 0;
                SceSys.eventCancel = 0;
            }
            break;
        }
    }
}

void cSceSys::scheduler()
{
    OSThread* parent = pParentThread;
    TASK* ctask;
    ScePrim* p;
    u8 running;
    u32 i;
    int waitRead;

    pParentThread = 0;
    ctask = pCTask;
    if (SceSys.x76 == 0 && SceSys.pause == 0) {
        for (i = 0; i < 13; i++) {
            prim[i].running = 0;
        }
    } else {
        prim[0].running = 0;
        for (i = 1; i < 13; i++) {
            prim[i].running = 1;
        }
    }
    p = (ScePrim*) scenarioSetOtStart();
    while ((p = (ScePrim*) scenarioGetOtAddr((u32*) p)) != 0) {
        if (pG->flags_170 & 0x800000) {
            break;
        }
        running = p->running;
        if (running != 0) {
            continue;
        }
        p->running = 1;
        CSCE_TASK = p;
        pCTask = p->task;
        TaskSchedulerMain(p->task);
        if (pCTask->status == 3) {
            SceTaskDelete(pCTask);
            pCTask->status = running;
        }
        if (pG->x20 == 2) {
            waitRead = (x73 != 0) ? 1 : 0;
            if (waitRead) {
                pParentThread = parent;
                pCTask = ctask;
                TaskSleep(1);
                parent = pParentThread;
                ctask = pCTask;
                pParentThread = 0;
                p->running = 0;
            }
        }
        if (wait == 1) {
            pParentThread = parent;
            pCTask = ctask;
            GXDrawDone();
            parent = pParentThread;
            ctask = pCTask;
            pParentThread = 0;
            p->running = 0;
            wait = 0;
        }
        p = (ScePrim*) scenarioSetOtStart();
    }
    pParentThread = parent;
    pCTask = ctask;
}

ScePrim* SceExec(int prio, TaskFunc func, int arg, u8 flag, int otPrio, void* model)
{
    TASK* t;
    ScePrim* p;
    u32 no;
    int busy;
    int i;

    if (prio == 0) {
        SetTaskModelPtr(model, 0);
        ((void (*)(int)) func)(arg);
        return 0;
    }
    if ((u32) prio > 17) {
        // A goto loop: no loop notes, so the Task[no] address is not strength-reduced.
        prio = 0;
        no = 17;
        if (Task[no].status == 0) {
            prio = 17;
        } else {
        retry:
            no--;
            if (no > 5) {
                if (Task[no].status != 0) {
                    goto retry;
                }
                prio = no;
            }
        }
    }
    if ((u32) (prio - 5) > 12) {
        pLog->err(0, 0, "SCE_TASK DON'T EXEC");
        return 0;
    }
    t = TaskExec(prio, func, arg);
    if (t == 0) {
        return 0;
    }
    p = &SceSys.prim[prio - 5];
    p->cancel = 0;
    p->task = t;
    AddPrim(&SceSys.otag[otPrio], (u32*) p);
    t->level = 0xE;
    SetTaskModelPtr(model, t);
    if (flag != 0) {
        t->flag = flag;
    } else if (SceSys.checkCTaskRange() == 0) {
        t->flag = 1;
    } else {
        t->flag = pCTask->flag;
    }
    if (!(pG->flags_64 & 0x200000)) {
        busy = 0;
        for (i = 17; i >= 6; i--) {
            if (Task[i].status != 0) {
                busy++;
            }
        }
        if ((u32) (12 - busy) <= 2) {
            pLog->warn(0, 0, "SCE_TASK remain num %d", 12 - busy);
        }
    }
    return p;
}

void SceSleep(int frames)
{
    if (SceSys.checkCTaskRange() != 0) {
        TaskSleep(frames);
    }
}

void SceExit()
{
    if (SceSys.checkCTaskRange() != 0) {
        SceTaskDelete(pCTask);
        TaskExit();
    }
}

void SceKill(int prio)
{
    SceTaskDelete(&Task[prio]);
    TaskKill(prio);
}

void SceKill(ScePrim* p)
{
    SceTaskDelete(p->task);
    TaskKill(p->task);
}

void SceKill(TASK* t)
{
    SceTaskDelete(t);
    TaskKill(t);
}

void SceKill(void (*func)(int))
{
    ScePrim* p = (ScePrim*) scenarioSetOtStart();

    while ((p = (ScePrim*) scenarioGetOtAddr((u32*) p)) != 0) {
        if (p->task->func == func) {
            SceKill(p->task);
        }
    }
}

int cSceSys::checkCTaskRange()
{
    if (pCTask == 0) {
        return 0;
    }
    if (pCTask == CTASK_MAIN) {
        return 0;
    }
    return (u32) (pCTask->no - 5) <= 12;
}

ScePrim* SceCTask()
{
    return pCSceTask;
}

void SceExecInitCondition()
{
    ClearOTagR(&SceExecOt, 1);
}

// em_dead row address as an integer (the original adds the list offset after the row index), as in sce_at.
static inline u32 emDeadRow(int n)
{
    return n * 32 + (u32) pG + 0x501C;
}

int SceExecCheckCondition_sub(SceCond* c)
{
    cEm* em;
    u32* row;
    u32 no;
    u32 bit;

    switch (c->type) {
    case 0:
        no = (u32) c->param;
        if (pG->emlist_no >= 0) {
            // Row address as integer arithmetic (index first, the list offset added last), like sce_at.
            bit = *(u32*) (((no >> 5) << 2) + emDeadRow(pG->emlist_no)) & (0x80000000 >> (no & 31));
        } else {
            bit = 0;
        }
        if (bit != 0) {
            return 1;
        }
        break;
    case 1:
        if (CamCtrl.CurrentAreaNo() == (int) c->param) {
            return 1;
        }
        break;
    case 2:
        em = (cEm*) c->param;
        // Raw (non-struct) read: keeps the load behind the store of `em` to its stack slot.
        if (*(s16*) ((u32) em + 0x320) <= 0 && em->xFC == 3) {
            return 1;
        }
        break;
    case 3:
        if (((int (*)()) c->param)() == 1) {
            return 1;
        }
        break;
    case 4:
        if (getRoomEtcBreak(c->param, &em, 1) == 1 && em->hp <= 0) {
            return 1;
        }
        break;
    case 5:
        if (SceAtPtr((int) c->param) != 0 && SceAtItemFlgCk((int) c->param) == 1) {
            return 1;
        }
        break;
    }
    return 0;
}

void SceExecCheckCondition()
{
    u32 v = SceExecOt;
    SceCond* c;

    if (v == 0xFFFFFFFF) {
        return;
    }
    do {
        c = (SceCond*) (v | 0x80000000);
        if ((s32) v < 0) {
            if (SceExecCheckCondition_sub(c) == 1) {
                if (c->func != 0) {
                    SceExec(c->prio, c->func, c->arg, c->flag, 2, 0);
                }
                DelPrim(&SceExecOt, (u32*) c);
                Mem_free(c);
            }
        }
        v = c->next;
    } while (v != 0xFFFFFFFF);
}

void SceExecLinkCondition(int type, void* param, u8 prio, TaskFunc func, int arg, u8 flag)
{
#line 608
    SceCond* c = (SceCond*) MEM_ALLOC(sizeof(SceCond), 1, 13);

    c->type = type;
    c->param = param;
    c->prio = prio;
    c->func = func;
    c->arg = arg;
    c->flag = flag;
    AddPrim(&SceExecOt, (u32*) c);
}

void SceExecLinkEmDead(void* param, u8 prio, TaskFunc func, int arg, u8 flag)
{
    SceExecLinkCondition(2, param, prio, func, arg, flag);
}

int EmMoveActiveCheck(cEm* em)
{
    if (!(em->be_flag & 1)) {
        return 0;
    }
    if (!(em->be_flag & 0x20)) {
        return 0;
    }
    if (!(em->be_flag & 2)) {
        return 0;
    }
    if (em->hp <= 0) {
        return 0;
    }
    if (em == pPL) {
        return 0;
    }
    return 1;
}

void SceExecEventCancel()
{
    MessageControl* mes = &cMes;
    cSceSys* s;
    int i;
    u32 no;
    u32 slot;

    pG->flags_170 = SceSys.stop_bak;
    for (i = 0; i <= 0xF; i++) {
        mes->Delete(i);
    }
    s = &SceSys;
    if (s->sndFlag == 1) {
        SndEventStrStop(0);
    }
    if (s->cancelFlagNo >= 0) {
        no = s->cancelFlagNo;
        eventFlags()[no >> 5] |= 0x80000000 >> (no & 31);
    }
    for (slot = 5; slot <= 17; slot++) {
        if (s->prim[slot - 5].cancel == 1) {
            SceKill((int) slot);
            s->prim[slot - 5].cancel = 0;
            break;
        }
    }
    if (SceSys.cancelFunc != 0) {
        SceExec(slot, SceSys.cancelFunc, SceSys.cancelArg, 2, 2, 0);
    }
    FadeSetW(0x80000000, 10, 0, 0);
}

void SceSetEventCancel(int on, TaskFunc func, int arg, int flagNo, int sndFlag)
{
    u32 no;
    cSceSys* s;

    if (on != 1) {
        on = 0;
    }
    U8SetI(SceSys.eventCancel, on);
    SceCTask()->cancel = on;
    if (flagNo >= 0) {
        no = flagNo;
        eventFlags()[no >> 5] &= ~(0x80000000 >> (no & 31));
    }
    // COMPILER-DIFF: 12 (AROUND form): the loop notes end cse1's path from the skipped `if` block, so
    // the tail's `&SceSys` is a fresh lis/addi instead of `eventCancel's address - 113`.
    do { } while (0);
    SceSys.cancelFunc = func;
    SceSys.cancelArg = arg;
    SceSys.cancelFlagNo = flagNo;
    SceSys.sndFlag = sndFlag;
    {
        // COMPILER-DIFF: candidate #17 (global.c pass 0 regs_used_so_far): r30 used-so-far makes `on`
        // take r30 in pass 0 and flagNo r31 (stock priorities give on r31, flagNo r30).
        register int pin asm("r30");
        asm volatile("" : "=r"(pin));
    }
}

int scenarioCheckEventCancel()
{
    cSceSys* s = &SceSys;
    GXColor start;
    GXColor end;

    if (s->eventCancel == 1 && (Key.trg & 0x20000000)) {
        *(u32*) &end = 0xFF;
        *(u32*) &start = 0;
        FadeSet(0, &start, &end, 0, 0, 0);
        s->stop_bak = pG->flags_170;
        pG->flags_170 = 0xFFFFFFFF;
        KeyStop(0xEFCF0000);
        SceExecEventCancel();
        s->eventCancel = 0;
        SubScreenWait(0x14);
        return 1;
    }
    return 0;
}
