#include "types.h"
#include "atari.h"
#include "light.h"
#include "event.h"
#include "map_obj.h"
#include "widget.h"
#include "card.h"
#include "dmg.h"
#include "global.h"
#include "main.h"
#include "main_sub.h"
#include "main_mem.h"
#include "scheduler.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "sce.h"
#include "em.h"
#include "em_set.h"
#include "obj.h"
#include "player.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "cockpit.h"
#include "id_sys.h"
#include "mes.h"
#include "snd.h"
#include "fade.h"
#include "pad.h"
#include "est.h"
#include "shadow.h"
#include "room_data.h"
#include "cDataSwap.h"
#include "dvd.h"
#include "scroll.h"
#include "rnd.h"
#include "math_sub.h"
#include "sscrn.h"
#include "option.h"
#include "game.h"
#include "eprintf.h"
#include "db_log.h"
#include "va_ppc.h"

// Scenario helpers shared by the room scripts: event brackets, messages, chapter end, elevators.

extern "C" {
void OSReport(const char* fmt, ...);
void* __builtin_new(unsigned int size);
void __builtin_delete(void* p);
int sprintf(char* dst, const char* fmt, ...);
int vsprintf(char* dst, const char* fmt, va_list ap);
void* memcpy(void* dst, const void* src, unsigned int n);
void SubScreenWait(int frames);
}

// cUnit::beginEvent / endEvent take an int in the original (see sscrn.cpp).
class cUnitEvent {
public:
    u32 be_flag;
    cUnit* next;
    virtual ~cUnitEvent();
    virtual void beginEvent(int mode);
    virtual void endEvent(int mode);
};
#define BEGIN_EVENT(p, mode) ((cUnitEvent*) (p))->beginEvent(mode)
#define END_EVENT(p, mode) ((cUnitEvent*) (p))->endEvent(mode)

#define HALT()                                                    \
    do {                                                          \
        const char* file_ = __FILE__;                             \
        OSReport("HALT %s(%d)\n", file_, __LINE__);               \
        *(volatile u32*) 0x11111111 = 0;                          \
    } while (0)

#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

// One pending item event (SceSetItemEvent), 0x20 bytes, new'd.
struct SceItemEvent {
    u8 flag;              // 0x00  room save flag set when the event ran
    u8 pad_1;
    s16 atNo;             // 0x02  trigger area
    s16 cut;              // 0x04  camera cut (-1 = none)
    s16 item[8];          // 0x06  item areas enabled by the event (-1 = none)
    void (*func)(int);    // 0x18
    int arg;              // 0x1C
};

// Elevator script data (SceElevator task argument).
struct SceElevatorData {
    s32 dir;              // 0x00  0/2: arrive, 1/3: leave (1/0 move down)
    u32 objId;            // 0x04  scroll object of the cage
    Vec pos;              // 0x08  cage rest position
    Vec plPos;            // 0x14  player position on the cage
    Vec plRot;            // 0x20
    s32 cut;              // 0x2C  camera cut (-1 = none)
    u16 pad_30;
    u16 seStart;          // 0x32
    u16 pad_34;
    u16 seStop;           // 0x36
    Vec jumpPos;          // 0x38  room jump destination
    Vec jumpRot;          // 0x44
    u16 room;             // 0x50
};

static void* ItemEventTbl[16];
static Camera SceCam;

void SceEventStart(int mode)
{
    cSceSys* s;

    if (SceSys.checkCTaskRange() == 1) {
        s = &SceSys;
        if (s->x70 == 0) {
            s->x70 = SceCTask()->task->flag;
            SceCTask()->task->flag |= 2;
        }
    }
    if (SceSys.x6E != 0) {
        SceSys.x6E++;
        return;
    }
    SceSys.x6E++;
    s = &SceSys;
    s->x64 = pG->flags_54;
    if (mode == 0) {
        EmMgr.beginEvent(0);
        ObjMgr.beginEvent(0);
        s->x6D = 1;
        EffectEventDelete();
        DmgMgr.beginEvent(0);
        SceKill(5);
    } else {
        if (s->x134) {
            SceKill(s->x134);
        }
    }
    PlEndCamera();
    LightMgr.beginEvent();
    BitOn(pG->flags_500C, 0x1000);
    BitOn(pG->flags_5010, 0x10000000);
    KeyStop(0xEFCF0000);
    if (pG->flags_500C & 0x400) {
        CamCtrl.LowerBinocular();
    }
    Cckpt.lifeMeterDisp(0);
    IdSys.dispSw(0x21, 0);
    SceSys.dmg = pPL->dmg;
    pPL->dmg.set(0, 0x80);
    BitOn(pG->flags_54, 0x800);
    BitOn(pG->flags_170, 0x100);
    BitOn(pG->flags_170, 0x400000);
    SndBlkStop(2);
}

// The value is evaluated before the `->task` load (`lbz x70` between the call and `lwz 8(r3)`).
static inline void SceTaskFlagSet(ScePrim* p, u8 v) { p->task->flag = v; }

void SceEventEnd(int mode)
{
    cSceSys* s = &SceSys;

    if (s->x6E == 0) {
        pLog->err(0, 0, "SceEventEnd: CALLS TO MACH");
    }
    if (--s->x6E != 0) {
        return;
    }
    if (s->x6D == 1) {
        s->x6D = 0;
        EmMgr.endEvent(mode);
        ObjMgr.endEvent(0);
        CamCtrl.Comeback(0);
        pPL->dmg.clear();
    } else {
        pPL->dmg = s->dmg;
    }
    LightMgr.endEvent();
    BitOff(pG->flags_5018, 0x1000000);
    BitOff(pG->flags_500C, 0x1000);
    BitOff(pG->flags_5010, 0x10000000);
    BitOff(pG->flags_170, 0x80000000);
    BitOff(pG->flags_54, 0x400);
    Cckpt.lifeMeterDisp(1);
    IdSys.dispSw(0x21, 1);
    BitOff(pG->flags_170, 0x100);
    BitOff(pG->flags_170, 0x400000);
    ShadowMemClear();
    if (SceSys.x64 & 0x800) {
        BitOn(pG->flags_54, 0x800);
    } else {
        BitOff(pG->flags_54, 0x800);
    }
    if (SceSys.checkCTaskRange() == 1) {
        s = &SceSys;
        if (s->x70 != 0) {
            SceTaskFlagSet(SceCTask(), s->x70);
        }
    }
    SceSys.x70 = 0;
    SubScreenWait(10);
}

void SceUpCutStart()
{
    cSceSys* s;

    if (SceSys.checkCTaskRange() == 1) {
        s = &SceSys;
        if (s->x70 == 0) {
            s->x70 = SceCTask()->task->flag;
            SceCTask()->task->flag |= 2;
        }
    }
    if (SceSys.x6C == 0) {
        SceSys.x60 = pG->flags_170;
        SceSys.x6C = 1;
    }
    KeyStop(0xEFCF0000);
    BitOn(pG->flags_58, 0x40000000);
    BitOn(pG->flags_58, 0x20000000);
    pPL->atari.clrFlag100();
    BitOn(pGS->flags_5010, 0x10000000);  // the pG load waits for the clrFlag100 store
    BitSet(pG->flags_170, 0xFFFFFFFF);
    BitOff(pG->flags_170, 0x40000000);
    BitOff(pG->flags_170, 0x10000);
    BitOff(pG->flags_170, 0x20000000);
    BitOff(pG->flags_170, 0x08000000);
    BitOff(pG->flags_170, 0x04000000);
    BitOff(pG->flags_170, 0x00800000);
    BitOff(pG->flags_170, 0x800);
    BitOff(pG->flags_170, 0x01000000);
    BitOff(pG->flags_170, 0x40);
    Cckpt.lifeMeterDisp(0);
    IdSys.dispSw(0x21, 0);
}

void SceUpCutEnd()
{
    cSceSys* s = &SceSys;

    BitOff(pG->flags_170, 0x80000000);
    BitOff(pG->flags_58, 0x40000000);
    BitOff(pG->flags_58, 0x20000000);
    pPL->atari.setFlag100();
    BitOff(pGS->flags_5010, 0x10000000);  // the pG load waits for the setFlag100 store
    if (s->x6C == 1) {
        pG->flags_170 = s->x60;
        s->x6C = 0;
    }
    if (s->checkCTaskRange() == 1) {
        if (s->x70 != 0) {
            ScePrim* p = SceCTask();
            u8 v = s->x70;  // read before the task pointer (both loads after the call)
            p->task->flag = v;
        }
    }
    SceSys.x70 = 0;
    Cckpt.lifeMeterDisp(1);
    IdSys.dispSw(0x21, 1);
    SubScreenWait(10);
}

int SceCheckEventStart()
{
    return pPL->checkEvent() == 1;
}

void SceSetRoomExitFunc(int a, int b)
{
    SceSys.x8 = a;
    SceSys.xC = b;
}

void SetFree(int no, u32 v)
{
    u32* tbl;

    if (no > 0x3F) {
        return;
    }
    tbl = pG->sce_free;
    tbl[no] = v;
}

u32 GetFree(int no)
{
    u32* tbl;

    if (no <= 0x3F) {
        tbl = pG->sce_free;
        return tbl[no];
    }
    return 0;
}

void SceMesSet(int no, u32 flags, int sel, int x, int y)
{
    u32 attr;

    attr = 0x1002;
    if (flags & 1) {
        attr = 0x1001;
    }
    if (flags & 0x20) {
        attr |= 0x10;
    }
    if (flags & 0x40) {
        attr |= 0x1000000;
    }
    if (flags & 0x80) {
        attr |= 0x40;
    }
    if (flags & 0x100) {
        attr |= 0x800000;
    }
    if (flags & 0x200) {
        attr |= 0x100000;
    }
    if (flags & 2) {
        attr |= 0x2000000;
    }
    cMes.MesSet(no, x, y, attr, 0, 0, 4);
    cMes.mes[0].cursor = sel - 1;
    Cckpt.lifeMeterDisp(0);
    if (!(flags & 0x10)) {
        SceMesWait();
    }
}

void SceMesCamSndSet(int no, int cut, int se)
{
    if (cut != -1) {
        CamCtrl.CutCall((s8) cut);
    }
    if (se != -1) {
        SndCall(6, se, 0, 0, 0, 0);
    }
    SceMesSet(no, cut == -1 ? 0 : 0x20, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
}

void SceUpCut(int a, int b, int c, int flags)
{
    SceAtMesData m;

    // Both flag bytes stored in each arm (jump2 cross-jumps the else arm's store into the
    // then arm's): the byte stays in r0 and `sth a` precedes the `&m` argument.
    if (flags & 1) {
        m.type = 1;
    } else {
        m.type = 0;
    }
    if (flags & 2) {
        m.x5 = 1;
    } else {
        m.x5 = 0;
    }
    m.no = a;
    m.x4 = b + 1;
    m.x6 = c + 1;
    m.x8 = flags;
    SceAtSetMes(&m);
    SceMesWait();
}

int SceMesGetSelection()
{
    int r;

    if ((r = cMes.getWork()->result) == 0) {
        do {
            SceSleep(1);
        } while ((r = cMes.getWork()->result) == 0);
    }
    return r;
}

void SceMesWait()
{
    while (cMes.mes[0].flags2 & 1) {
        SceSleep(1);
    }
}

void SceSndCallThunder()
{
    SndCall(6, 0x1D, 0, 0, 0, 0);
}

int SceCheckEmAlive(cEm* em)
{
    if (em == 0) {
        return 0;
    }
    if (!em->isAlive()) {
        return 0;
    }
    if (em->checkStatus(5) == 0) {
        return 0;
    }
    return 1;
}

int SceCountEmAlive(int lo, int hi)
{
    int cnt = 0;
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if (hi == -1) {
            hi = lo;
        }
        if (em->id >= lo && em->id <= hi) {
            if (SceCheckEmAlive(em) == 1) {
                cnt++;
            }
        }
    }
    return cnt;
}

void SceDestroyEm(int lo, int hi)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if (hi == -1) {
            hi = lo;
        }
        if (em->id >= lo && em->id <= hi) {
            if (em->isAlive()) {
                EmListData* l = GetListPtrFromEm(em);
                if (l) {
                    l->flags &= ~1;
                }
                EmMgr.destroy(em);
            }
        }
    }
}

void SceInitItemEvent()
{
    void** p = ItemEventTbl;
    int i;

    for (i = 0; i < 16; i++) {
        *p++ = 0;
    }
    // Strings of a debug prompt the original kept around (dead code); its HALT() emits the
    // "D:/Bio4/Prog/sce_com.cpp" string before "HALT %s(%d)\n" (the flag_rsf.h checks below reuse
    // both, with the fmt high first).
    if (0) {
        pLog->err(0, 0, "Do you use the PLANTER?");
        pLog->err(0, 0, "You used the PLANTER.");
        pLog->err(0, 0, "You cannot use a PLANTER.");
        pLog->err(0, 0, "              >YES  NO");
        pLog->err(0, 0, "               YES >NO");
#line 444 "D:/Bio4/Prog/sce_com.cpp"
        HALT();
    }
}

#include "flag_rsf.h"

extern "C" void SceExecItemEvent(SceItemEvent* e);

void SceExecItemEvent(SceItemEvent* e)
{
    u32 i;
    int flag;  // `lbz` straight into the callee-saved register (a u8 local adds an `mr` copy)
    u16 room;

    SceAtSetEnable(e->atNo, 0);
    for (i = 0; i <= 7; i++) {
        if (e->item[i] >= 0) {
            cModel* m;
            SceAtSetEnable(e->item[i], 1);
            m = SceAtItemModelPtr(e->item[i]);
            if (m) {
                m->setNoSuspend(1);
            }
        }
    }
    flag = e->flag;
    room = pG->room_id;
    RsfSet(room, flag);
    SceUpCutStart();
    if (e->cut >= 0) {
        CamCtrl.CutCall((s8) e->cut);
        e->func(e->arg);
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        SceSleep(0xF);
        CamCtrl.Comeback(0);
    } else {
        e->func(e->arg);
    }
    SceUpCutEnd();
    for (i = 0; i < 16; i++) {
        SceItemEvent* p = (SceItemEvent*) ItemEventTbl[i];
        if (p && p->atNo == e->atNo) {
            ItemEventTbl[i] = 0;
        }
    }
    __builtin_delete(e);
}

void SceSetItemEvent(int atNo, int itemNo, int flagNo, int cut, void (*func)(int), TaskFunc doneFunc, int arg, int enable)
{
    u16 room = pG->room_id;
    SceItemEvent* e;   // the searched entry; the new'd one is a second variable (one pseudo for both
                       // is live across the search loop's j / e+6 / j*2 temps and takes r8 there)
    SceItemEvent* ne;
    u32 i;
    u32 j;
    u32 k;

    if (RsfCheck(room, flagNo)) {
        SceAtSetEnable(atNo, 0);
        if (itemNo >= 0) {
            if (SceAtItemFlgCk(itemNo) == 0) {
                SceAtSetEnable(itemNo, 1);
            }
        }
        SceExec(0x12, doneFunc, arg, 0, 2, 0);
        return;
    }
    if (itemNo >= 0) {
        if (enable == 0) {
            SceAtSetEnable(itemNo, 0);
        } else {
            cModel* m;
            SceAtSetEnable(itemNo, 1);
            m = SceAtItemModelPtr(itemNo);
            SceAtSetEnable(itemNo, 0);
            if (m) {
                m->be_flag |= 2;
            }
        }
    }
    for (i = 0; i < 16; i++) {
        e = (SceItemEvent*) ItemEventTbl[i];
        if (e && e->atNo == atNo) {
            // The slot search: item[0] tested and stored with the folded offset, then a loop entered
            // by a `goto` INTO its body (a jump into the loop invalidates it for loop.c: no giv for
            // j*2, `e + 6` recomputed per iteration, and the exit block's guard targets a label of
            // another loop so find_and_verify_loops leaves `item[0] = itemNo; return` in place).
            // COMPILER-DIFF: `li j,0` spelled as a mask combine folds to 0. A C `j = 0` gets cse's
            // REG_EQUAL note, and update_equiv_regs (the first set of j in chain order) doubles j's
            // live length (11 -> 22: priority 30909 < e+6 48000 / j*2 40000, so j is allocated third
            // and lands in r10). cse cannot fold `(e >> 16) & 0xFFFF0000` (no nonzero-bits logic) so
            // the set carries no note; combine folds it to `(set j 0)` after cse2, j keeps 11 (61818)
            // and is allocated first (r9, e+6 r11, j*2 r10, e r8 = the target). Also keeps j a
            // pseudo: a hard-reg pin makes combine fold expand_mult's `copy + j` into `slwi` where
            // the target has `add`.
            j = ((u32) e >> 16) & 0xFFFF0000;
            if (e->item[0] >= 0) {
                goto next;
            }
            e->item[0] = itemNo;
            return;
            do {
            next:
                j++;
                if (j > 7) {
                    return;
                }
            } while (e->item[j] >= 0);
            e->item[j] = itemNo;
            return;
        }
    }
    for (i = 0; i <= 15; i++) {
        if (ItemEventTbl[i] == 0) {
            break;
        }
    }
    if (i == 16) {
        pLog->err(0, 0, "SceSetItemEvent(): TBL num over");
        return;
    }
    if (SceAtPtr(atNo)) {
        SceAtPtr(atNo)->x38 = 8;
        SceAtPtr(atNo)->x4A = 0x10;
        SceAtPtr(atNo)->x44 = 5;
    }
    ne = (SceItemEvent*) __builtin_new(sizeof(SceItemEvent));
    for (k = 0; k < 8; k++) {
        ne->item[k] = -1;
    }
    ne->item[0] = itemNo;
    ItemEventTbl[i] = ne;
    ne->cut = cut;
    ne->func = func;
    ne->arg = arg;
    ne->flag = flagNo;
    ne->atNo = atNo;
    SceAtDataSet_exec(atNo, 0x12, 0, (TaskFunc) SceExecItemEvent, ne, 1);
}

void getChapterSection(int chapter, int* chap, int* sec)
{
    switch (chapter) {
    case 0:
        *chap = 1;
        *sec = 1;
        break;
    case 1:
        *chap = 1;
        *sec = 2;
        break;
    case 2:
        *chap = 1;
        *sec = 3;
        break;
    case 3:
        *chap = 2;
        *sec = 1;
        break;
    case 4:
        *chap = 2;
        *sec = 2;
        break;
    case 5:
        *chap = 2;
        *sec = 3;
        break;
    case 6:
        *chap = 3;
        *sec = 1;
        break;
    case 7:
        *chap = 3;
        *sec = 2;
        break;
    case 8:
        *chap = 3;
        *sec = 3;
        break;
    case 9:
        *chap = 3;
        *sec = 4;
        break;
    case 0xA:
        *chap = 4;
        *sec = 1;
        break;
    case 0xB:
        *chap = 4;
        *sec = 2;
        break;
    case 0xC:
        *chap = 4;
        *sec = 3;
        break;
    case 0xD:
        *chap = 4;
        *sec = 4;
        break;
    case 0xE:
        *chap = 5;
        *sec = 1;
        break;
    case 0xF:
        *chap = 5;
        *sec = 2;
        break;
    case 0x10:
        *chap = 5;
        *sec = 3;
        break;
    case 0x11:
        *chap = 5;
        *sec = 4;
        break;
    case 0x12:
        *chap = 6;
        *sec = 1;
        break;
    default:
        *chap = 1;
        *sec = 1;
        break;
    }
}

// Reference setters: the original stores these GlobalWork fields through references (pG reloaded after each).
static inline void U8Set(u8& d, u8 v) { d = v; }
static inline void U16Set(u16& d, u16 v) { d = v; }
static inline void U32Set(u32& d, u32 v) { d = v; }
static inline void U16Zero(u16& d) { d = 0; }  // HImode zero (its own `li`), reference store

void SceChapterEnd()
{
    cDataSwap swap;
    static u32 stop_bak;
    static u32 disp_bak;
    static char chap_data_name[0x20];
    static u32 MARGIN = 0x20000;
    void* evt;
    int chap;
    int sec;
    u32 len;
    void* data;
    ChapterEnd* ce;
    int req;
    int sel;
    u16 room;
    u8 x4F9E;
    EventMgr* ev = &EvtMgr;
    u32* key = &ev->x34;

    pG->x4F8A = SceSys.x74 + 1;
    if (ev->IsAliveEvt(key, 0, 1)) {
        ev->GetEvt(key, &evt);
        ev->DelEvt(evt, 0);
    }
    if (Fade[1].flags & 1) {
        SceSleep(1);
    }
    sel = 0;
    disp_bak = pG->flags_58;
    BitSet(pG->flags_58, 0xFFFFFFFF);
    BitOff(pG->flags_58, 0x2000);
    BitOff(pG->flags_58, 0x800);
    BitOff(pG->flags_58, 0x10000);
    stop_bak = pG->flags_170;
    BitSet(pG->flags_170, 0xFFFFFFFF);
    BitOff(pG->flags_170, 0x800000);
    BitOff(pG->flags_170, 0x40);
    SceSleep(2);
    chap = 0;
    sec = 0;
    getChapterSection(SceSys.x74, &chap, &sec);
    if (SceSys.x74 == 0x11) {
        sprintf(chap_data_name, "SS/___/chap06.dat");
    } else if (SceSys.x74 == 0xD) {
        sprintf(chap_data_name, "SS/___/chap07.dat");
    } else {
        sprintf(chap_data_name, "SS/___/chap%02ld.dat", chap);
    }
    setLangExt3(chap_data_name + 3);
    Dvd.FileExistCheck(chap_data_name, &len);
    len = len + 0xC;
    len = len + MARGIN;
    swap.SwapOut((u32) pG->pRoomArc, len, 0);
    ce = (ChapterEnd*) __builtin_new(sizeof(ChapterEnd));
#line 994 "D:/Bio4/Prog/sce_com.cpp"
    req = DVD_READ_N(chap_data_name, 0, 0, 0, 0, 5);
    Dvd.ReadCheck(req, 0, 0, &data);
    ce->init(data, SceSys.x74);
    ce->move();
    FadeKillAll();
    FadeSetW(0x80000000, 10, 0, 0);
    SceSleep(0xF);
    SceMesSet(0x80, 1, 1, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
    Vec plPos;
    Vec plRot;
    // The two zeros are assigned after the FadeSetW so its `col.end = 0` keeps its own zero pseudo (the
    // one the U8Set/U16Set stores below reuse, r27); room before x4F9E (sched1 LUID order of the `li`s),
    // room declared first (the global-alloc tie for r25/r24).
    room = 0;
    x4F9E = 0;
    if (SceSys.x78 >= 0) {
        plPos = pPL->pos;
        plRot = pPL->rot;
        room = pG->room_id;
        x4F9E = pG->x4F9E;
        if (SceAtPtr(SceSys.x78)->x35 == 1) {
            pPL->pos.x = SceAtPtr(SceSys.x78)->dstPos.x;
            pPL->pos.y = SceAtPtr(SceSys.x78)->dstPos.y;
            pPL->pos.z = SceAtPtr(SceSys.x78)->dstPos.z;
            FSet(pPL->rot.y, SceAtPtr(SceSys.x78)->dstAngle);  // the pG load of room_id_prev waits for the store
            U16Set(pG->room_id_prev, pG->room_id);
            U8Set(pG->x4FA2, pG->x4F9E);
            U8Set(pG->stage_no, SceAtPtr(SceSys.x78)->dstStage);
            U8Set(pG->room_no, SceAtPtr(SceSys.x78)->dstRoom);
            U8Set(pG->x4F9E, SceAtPtr(SceSys.x78)->dstX4F9E);
            U8Set(pG->x4F9F, 0);
            U16Set(pG->x4F90, 0);
        } else {
            pLog->err(0, 0, "SceChapterEnd(): Door at faild");
        }
    }
    U16Zero(pG->x8338);
    U32Set(pG->em_die_cnt, 0);
    U32Set(pG->shotHit, 0);
    U32Set(pG->shotTotal, 0);
    GameSaveSave(&GameSave, pSaveData, 2);
    sel = SceMesGetSelection();
    if (sel == 1) {
        SndCall(0, 4, 0, 0, 0, 0);
    } else {
        SndCall(0, 5, 0, 0, 0, 0);
    }
    FadeSetW(0, 5, 0, 0);
    SceSleep(5);
    ce->quit();
    __builtin_delete(ce);
    swap.SwapIn();
    if (sel == 1) {
        CardSave(0, 10);
        SceSleep(1);
    }
    BitSet(pG->flags_58, disp_bak);
    BitSet(pG->flags_170, stop_bak);
    FadeSetW(0, 0, 0, 0);
    FadeKill(2);
    if (SceSys.x78 >= 0) {
        memcpy((u8*) pPL + 0x94, &plPos, sizeof(Vec));
        memcpy((u8*) pPL + 0xA0, &plRot, sizeof(Vec));
        U16Set(pG->room_id, room);
        U8Set(pG->x4F9E, x4F9E);
        if (SceAtPtr(SceSys.x78)) {
            SceAtPtr(SceSys.x78)->x77 = 2;
            SceAtExecute(SceSys.x78);
        }
    } else {
        SndRoomBgmStartCheck(1);
        SndRoomStrStartCheck();
        FadeSetW(0x80000000, 10, 0, 0);
        SceSys.pause = 0;
    }
}

void SceSetChapterEnd(int chapter, int doorAt)
{
    GXColor c0;
    GXColor c1;

    *(u32*) &c0 = 0;
    *(u32*) &c1 = 0xFF;
    FadeSet(0, &c0, &c1, 1, 0, 0);
    SndRoomStrStop(1);
    SndRoomBgmStop(0, 0);
    SndRoomBgmStop(1, 0);
    SndSeAbsFadeOutAll_sec(1);
    SceEventStart(0);
    BitOff(pG->flags_170, 0x800000);
    SceSys.pause = 1;
    SceSys.x74 = chapter;
    SceSys.x78 = doorAt;
    SetGameTime();
    SceExec(5, (TaskFunc) SceChapterEnd, 0, 0, 2, 0);
    SceSleep(1);
    SceEventEnd(0);
}

static inline f32 vecDist(Vec* a, Vec* b)
{
    return SQRTF((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) + (a->z - b->z) * (a->z - b->z));
}

void SceCamMove(Vec* pos, Vec* at, f32 fovy)
{
    SceCam.param.pos = *pos;
    SceCam.param.at = *at;
    SceCam.param.fovy = fovy;
    SceCam.up.x = 0.0f;
    SceCam.up.y = 1.0f;
    SceCam.up.z = 0.0f;
    SceCam.dist = vecDist(&SceCam.param.pos, &SceCam.param.at);
    CameraSetOrientationUp(&SceCam);
    CamCtrl.x250 = (s32) &SceCam;
}

void OpenBoxMain(int type, int mode, int se, u32 id1, u32 id2, int itemNo)
{
    cObj* o1 = 0;
    cObj* o2 = 0;
    cModel* item = 0;
    f32 dy = 0.0f;
    // Case bodies are laid out in source order: case 0x17 (both doors, 160 deg) follows case 0 in
    // both switches. Every loop declares its own `int i`: the two-frame waits' counters are then
    // short-lived pseudos (5 refs / ~8 insns) that global alloc places first, taking r31 before
    // `type` (r30) and `o1` (r29); id2 and the 30-frame counter reuse r31 afterwards. One shared
    // `int i` (35 refs / 482 insns) sorts below them and rotates the three.
    // Constant-pool order: the 30-frame totals, their per-frame steps (folded divisions: the
    // decimal step literals are one ulp off) and the drop step enter the pool here; every use
    // below is folded to the literal.
    const f32 ryA = -1.9198622f, ryB = 1.9198622f, ryC = -2.7925267f, ryD = 2.7925267f;
    const f32 rxA = 1.7f, rxB = -1.7f, rzA = 1.5707964f, rzB = -1.5707964f, pxA = 500.0f, pxB = -500.0f;
    const f32 syA = ryA / 30.0f, syB = ryB / 30.0f, syC = ryC / 30.0f, syD = ryD / 30.0f;
    const f32 sxA = rxA / 30.0f, sxB = rxB / 30.0f, szA = rzA / 30.0f, szB = rzB / 30.0f, spA = pxA / 30.0f, spB = pxB / 30.0f;
    const f32 dropStep = 10.0f;

    if (id1 != -1) {
        o1 = SmdGetObjPtr(id1);
    }
    if (id2 != -1) {
        o2 = SmdGetObjPtr(id2);
    }
    if (itemNo != -1) {
        item = SceAtItemModelPtr(itemNo);
    }
    if (o1) {
        o1->be_flag |= 0x20;
    }
    if (o2) {
        o2->be_flag |= 0x20;
    }
    if (mode == 0) {
        switch (type) {
        case 0x15:
            for (int i = 0; i < 2; i++) {
                SceSleep(1);
            }
            for (int i = 0; i < 2; i++) {
                if (o1) {
                    o1->pParts->rot.z += -0.034906585f;
                }
                SceSleep(1);
            }
            for (int i = 0; i < 3; i++) {
                SceSleep(1);
            }
            break;
        case 0x16:
            for (int i = 0; i < 5; i++) {
                SceSleep(1);
            }
            break;
        }
        if (se != -1) {
            SndCall(6, se, 0, 0, 0, 0);
        }
        for (int i = 0; i < 30; i++) {
            switch (type) {
            case 0:
                if (o1) {
                    o1->rot.y += (-1.9198622f / 30.0f);
                }
                if (o2) {
                    o2->rot.y += (1.9198622f / 30.0f);
                }
                break;
            case 0x17:
                if (o1) {
                    o1->rot.y += (-2.7925267f / 30.0f);
                }
                if (o2) {
                    o2->rot.y += (2.7925267f / 30.0f);
                }
                break;
            case 1:
            case 0x13:
                if (o1) {
                    o1->rot.y += (-1.9198622f / 30.0f);
                }
                break;
            case 2:
            case 0x14:
                if (o1) {
                    o1->rot.y += (1.9198622f / 30.0f);
                }
                break;
            case 0x18:
                if (o1) {
                    o1->rot.y += (-2.7925267f / 30.0f);
                }
                break;
            case 0x19:
                if (o1) {
                    o1->rot.y += (2.7925267f / 30.0f);
                }
                break;
            case 3:
                if (o1) {
                    o1->rot.x += (1.7f / 30.0f);
                }
                break;
            case 4:
                if (o1) {
                    o1->rot.x += (-1.7f / 30.0f);
                }
                break;
            case 5:
                if (o1) {
                    o1->rot.z += (1.7f / 30.0f);
                }
                break;
            case 6:
                if (o1) {
                    o1->rot.z += (-1.7f / 30.0f);
                }
                break;
            case 7:
                if (o1) {
                    o1->pParts->rot.x += (1.7f / 30.0f);
                }
                break;
            case 8:
                if (o1) {
                    o1->pParts->rot.x += (-1.7f / 30.0f);
                }
                break;
            case 9:
                if (o1) {
                    o1->pParts->rot.z += (1.7f / 30.0f);
                }
                break;
            case 0xA:
                if (o1) {
                    o1->pParts->rot.z += (-1.7f / 30.0f);
                }
                break;
            case 0xB:
                if (o1) {
                    o1->rot.x += (1.5707964f / 30.0f);
                }
                break;
            case 0xC:
                if (o1) {
                    o1->rot.x += (-1.5707964f / 30.0f);
                }
                break;
            case 0xD:
                if (o1) {
                    o1->rot.z += (1.5707964f / 30.0f);
                }
                break;
            case 0xE:
                if (o1) {
                    o1->rot.z += (-1.5707964f / 30.0f);
                }
                break;
            case 0xF:
                if (o1) {
                    o1->pos.x += (500.0f / 30.0f);
                }
                if (item) {
                    item->pos.x += (500.0f / 30.0f);
                }
                break;
            case 0x10:
                if (o1) {
                    o1->pos.x += (-500.0f / 30.0f);
                }
                if (item) {
                    item->pos.x += (-500.0f / 30.0f);
                }
                break;
            case 0x11:
                if (o1) {
                    o1->pos.z += (500.0f / 30.0f);
                }
                if (item) {
                    item->pos.z += (500.0f / 30.0f);
                }
                break;
            case 0x12:
                if (o1) {
                    o1->pos.z += (-500.0f / 30.0f);
                }
                if (item) {
                    item->pos.z += (-500.0f / 30.0f);
                }
                break;
            case 0x15:
                dy -= 10.0f;
                if (o1) {
                    o1->pos.y += dy;
                    o1->pParts->rot.z += -0.017453292f;
                }
                break;
            case 0x16:
                dy -= 10.0f;
                if (o1) {
                    o1->pos.y += dy;
                }
                break;
            }
            SceSleep(1);
        }
    } else {
        switch (type) {
        case 0:
            if (o1) {
                o1->rot.y += -1.9198622f;
            }
            if (o2) {
                o2->rot.y += 1.9198622f;
            }
            break;
        case 0x17:
            if (o1) {
                o1->rot.y += -2.7925267f;
            }
            if (o2) {
                o2->rot.y += 2.7925267f;
            }
            break;
        case 1:
        case 0x13:
            if (o1) {
                o1->rot.y += -1.9198622f;
            }
            break;
        case 2:
        case 0x14:
            if (o1) {
                o1->rot.y += 1.9198622f;
            }
            break;
        case 0x18:
            if (o1) {
                o1->rot.y += -2.7925267f;
            }
            break;
        case 0x19:
            if (o1) {
                o1->rot.y += 2.7925267f;
            }
            break;
        case 3:
            if (o1) {
                o1->rot.x += 1.7f;
            }
            break;
        case 4:
            if (o1) {
                o1->rot.x += -1.7f;
            }
            break;
        case 5:
            if (o1) {
                o1->rot.z += 1.7f;
            }
            break;
        case 6:
            if (o1) {
                o1->rot.z += -1.7f;
            }
            break;
        case 7:
            if (o1) {
                o1->pParts->rot.x += 1.7f;
            }
            break;
        case 8:
            if (o1) {
                o1->pParts->rot.x += -1.7f;
            }
            break;
        case 9:
            if (o1) {
                o1->pParts->rot.z += 1.7f;
            }
            break;
        case 0xA:
            if (o1) {
                o1->pParts->rot.z += -1.7f;
            }
            break;
        case 0xB:
            if (o1) {
                o1->rot.x += 1.5707964f;
            }
            break;
        case 0xC:
            if (o1) {
                o1->rot.x += -1.5707964f;
            }
            break;
        case 0xD:
            if (o1) {
                o1->rot.z += 1.5707964f;
            }
            break;
        case 0xE:
            if (o1) {
                o1->rot.z += -1.5707964f;
            }
            break;
        case 0xF:
            if (o1) {
                o1->pos.x += 500.0f;
            }
            if (item) {
                item->pos.x += 500.0f;
            }
            break;
        case 0x10:
            if (o1) {
                o1->pos.x += -500.0f;
            }
            if (item) {
                item->pos.x += -500.0f;
            }
            break;
        case 0x11:
            if (o1) {
                o1->pos.z += 500.0f;
            }
            if (item) {
                item->pos.z += 500.0f;
            }
            break;
        case 0x12:
            if (o1) {
                o1->pos.z += -500.0f;
            }
            if (item) {
                item->pos.z += -500.0f;
            }
            break;
        case 0x15:
        case 0x16:
            break;
        }
    }
}

extern "C" void SceElevator(SceElevatorData* d);

// Inline helpers owning their locals (r225.cpp SceElevator_r225 has the same function): the inlined
// frame is one BLKmode temp slot popped at the end of each statement, so every call shares frame slot
// 8. Argument MEMs are evaluated lazily (the pointer before a call in another argument, the load
// after it: `lwz r30,pPL; bl fRand1_1; lfs 148(r30)`).
static inline void SetPosXYZ(cModel* m, f32 x, f32 y, f32 z)
{
    Vec v;

    v.x = x;
    v.y = y;
    v.z = z;
    m->setPos(&v);
}

// The two fade colours must live in a BLKmode object: a 4-byte GXColor local becomes an ADDRESSOF
// pseudo (SImode) and purge_addressof gives it a permanent frame slot instead of the shared temp at
// 8/12; a 12-byte struct reuses the Vec slot (temp reuse needs equal modes).
struct FadeColors {
    GXColor c0;
    GXColor c1;
    u32 pad;
};

static inline void FadeSetRGBA(u32 mode, u32 rgba0, u32 rgba1)
{
    FadeColors c;

    *(u32*) &c.c0 = rgba0;
    *(u32*) &c.c1 = rgba1;
    FadeSet(mode, &c.c0, &c.c1, 30, 0, 0);
}

// Shape from r225.cpp's SceElevator_r225 (SetPosXYZ / FadeSetRGBA inline helpers, the goto-entered up
// loop, the down loop with its tail inside). Residue closed by the `jp` pin: `done` (10 refs, live
// length 106 x4 from update_equiv_regs' two `done = 0` REG_EQUIV doublings = 424, priority 707) sorts
// below gcse's `&d->pos` copy (9 refs / 380 = 710) in global alloc, so the copy takes r25 and done r24;
// the target has done r25 / copy r24 (done's length there is 105 -> 420 -> 714: one pre-reload insn
// fewer somewhere in its range). Holding `&d->jumpPos` (the target's r25 in the up loop, where done is
// dead) in r25 makes the copy take r24 and done r25 without touching anything else.
void SceElevator(SceElevatorData* d)
{
    cPlayer* pl = pPL;
    cObj* obj;
    f32 accel;
    f32 maxSpd;
    f32 minSpd;
    f32 stopDist;
    f32 stopDist2;
    f32 spd;
    f32 step;
    f32 move;
    int faded;
    int done;
    FadeWork* fade;
    u32 white;
    int i;
    int j;
    u32 hSnd;
    register Vec* jp asm("r25");  // COMPILER-DIFF: register pin (see the comment above the function)

    obj = SmdGetObjPtr(d->objId);
    if (obj == 0) {
        return;
    }
    maxSpd = 100.0f;
    minSpd = 10.0f;
    accel = 2.0f;
    stopDist = CalcStopDist(maxSpd, accel);
    stopDist2 = stopDist + 4000.0f;
    SceEventStart(0);
    faded = 0;
    done = 0;
    BitOn(pG->flags_5014, 0x20000);
    obj->setNoSuspend(1);
    obj->setPos(&d->pos);
    pPL->setNoSuspend(1);
    BEGIN_EVENT(pPL, 0);
    pPL->setPos(&d->plPos);
    pPL->setAng(&d->plRot);
    pPL->be_flag &= ~0x10;
    CamCtrl.Comeback(0);
    if (d->cut != -1) {
        CamCtrl.CutCall((s8) d->cut);
    }
    if (d->dir == 1 || d->dir == 3) {
        SndCall(6, d->seStart, &obj->pos, 0, 0, 0);
        spd = accel;
        jp = &d->jumpPos;
        for (i = 0; i < 10; i++) {
            obj->setPos(&d->pos);
            pPL->setPos(&d->plPos);
            SetPosXYZ(obj, obj->pos.x, fRand1_1() * 10.0f + obj->pos.y, obj->pos.z);
            SetPosXYZ(pPL, pPL->pos.x, fRand1_1() * 10.0f + pPL->pos.y, pPL->pos.z);
            SceSleep(1);
        }
        obj->setPos(&d->pos);
        pPL->setPos(&d->plPos);
        // Up loop: a noted loop that loop.c does not process (entered by the goto below = "multiple
        // entry points"), laid out `b TOP; SLEEP: SceSleep; spd += accel; TOP: ...` (r225.cpp).
        fade = &Fade[2];
        white = 0xFF;
        goto up_top;
        for (;;) {
            SceSleep(1);
            // COMPILER-DIFF: candidate (sched1 loop-note barrier). The target issues `spd += accel`
            // after the SceSleep call; sched1 only keeps it there behind a loop note.
            do { } while (0);
            spd += accel;
        up_top:
            if (spd > maxSpd) {
                spd = maxSpd;
            }
            step = spd;
            if (d->dir == 1) {
                step = -spd;
            }
            SetPosXYZ(obj, obj->pos.x, obj->pos.y + step, obj->pos.z);
            SetPosXYZ(pPL, pPL->pos.x, pPL->pos.y + step, pPL->pos.z);
            if (faded == 0) {
                if (spd >= maxSpd) {
                    FadeSetRGBA(2, 0, white);
                    faded = 1;
                }
            } else if ((fade->flags & 1) == 0) {
                BitOff(pG->flags_5014, 0x20000);
                SceAtExecRoomJump(d->room, jp, &d->jumpRot, 0);
                break;
            }
        }
    }
    if (d->dir == 0 || d->dir == 2) {
        BitOff(pG->flags_5010, 0x10000000);
        spd = maxSpd;
        move = stopDist2;
        if (d->dir == 0) {
            move = -move;
        }
        SetPosXYZ(obj, obj->pos.x, obj->pos.y + move, obj->pos.z);
        SetPosXYZ(pPL, pl->pos.x, pPL->pos.y + move, pl->pos.z);
        CamCtrl.Comeback(0);
        FadeSetRGBA(0x80000002, 0xFF, 0);
        hSnd = SndCall(6, d->seStart, &obj->pos, 0, 0, 0);
        // Down loop: `for (;;) { body; if (done) { tail; break; } SceSleep(1); }` (r225.cpp): the
        // rotated loop with the tail inside it; `y` is only the fabs operand, `move` is a second
        // step variable so `step` dies in the up loop.
        for (;;) {
            f32 y = obj->pos.y;
            if (__builtin_fabsf(d->pos.y - y) < stopDist) {
                spd -= accel;
                if (spd < minSpd) {
                    spd = minSpd;
                }
            }
            move = spd;
            if (d->dir != 0) {
                move = -move;
            }
            SetPosXYZ(obj, obj->pos.x, obj->pos.y + move, obj->pos.z);
            SetPosXYZ(pPL, pPL->pos.x, pPL->pos.y + move, pPL->pos.z);
            {
                Vec q = {0.0f, 0.0f, 0.0f};
                q.y = move;
                pG->quake_ofs = q;
            }
            done = 0;
            if (d->dir == 0) {
                if (obj->pos.y >= d->pos.y) {
                    done = 1;
                }
            }
            if (d->dir == 2) {
                if (obj->pos.y <= d->pos.y) {
                    done = 1;
                }
            }
            if (done != 0) {
                if (hSnd) {
                    SndStop(hSnd, 0);
                }
                SndCall(6, d->seStop, &obj->pos, 0, 0, 0);
                obj->setPos(&d->pos);
                pPL->setPos(&d->plPos);
                pPL->setAng(&d->plRot);
                break;
            }
            SceSleep(1);
        }
        for (j = 0; j < 10; j++) {
            obj->setPos(&d->pos);
            pPL->setPos(&d->plPos);
            SetPosXYZ(obj, obj->pos.x, fRand1_1() * 10.0f + obj->pos.y, obj->pos.z);
            SetPosXYZ(pPL, pPL->pos.x, fRand1_1() * 10.0f + pPL->pos.y, pPL->pos.z);
            SceSleep(1);
        }
        obj->setPos(&d->pos);
        pPL->setPos(&d->plPos);
    }
    BitOff(pG->flags_5014, 0x20000);
    pPL->be_flag |= 0x10;
    SceEventEnd(0);
    SceExit();
}

void SceDebugDisp(const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    char buf[0x100];  // declared after va_start: the register save area and ap get their slots first
    vsprintf(buf, fmt, ap);
    eprintf(0x14, (s16) SceSys.x7A, 0, 1, "%s", buf);
    SceSys.x7A += 0xF;
}

// Called from title.cpp with an argument (`DebugTrg(1)`): the parameter exists, the body ignores it.
int DebugTrg(int)
{
    return 0;
}

template <class T>
void cManager<T>::beginEvent(int mode)
{
    u32 i;

    for (i = 0; i < nArray; i++) {
        T* p = (T*) ((u8*) pArray + size * i);
        if (p->isAlive()) {
            BEGIN_EVENT(p, mode);
        }
    }
}

template <class T>
void cManager<T>::endEvent(int mode)
{
    u32 i;

    for (i = 0; i < nArray; i++) {
        T* p = (T*) ((u8*) pArray + size * i);
        if (p->isAlive()) {
            END_EVENT(p, mode);
        }
    }
}

asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
