#include "types.h"
#include "event.h"
#include "light.h"
#include "xml.h"
#include "dbg_tool.h"
#include "atari.h"
#include "global.h"
#include "joy.h"
#include "scheduler.h"
#include "file.h"
#include "est.h"
#include "sce.h"
#include "fade.h"
#include "snd.h"
#include "mes.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "view.h"
#include "db_cam.h"
#include "cockpit.h"
#include "main_sub.h"
#include "st_mgr_event.h"
#include "db_light.h"
#include "db_filelist.h"
#include "db_sctrl.h"
#include "t_util.h"
#include "t_event.h"

// Event tool (D:/Bio4/Prog/t_event.cpp): a task object (ToolEvt) with a file menu, the event preview
// (EventMgr::SetEvt of a host .evd file, stop / capture / message display) and the preview sub tools
// (light editor, ESP tool hand-off, fog and focus Hermite curve editors, message list editor).

extern "C" {
void memclr_asm(void* p, u32 size);
void* memset(void* p, int c, unsigned int n);
int sscanf(const char* s, const char* fmt, ...);
unsigned int strlen(const char* s);
char* strcpy(char* dst, const char* src);
}

void DbMenuSetExecTool(const char* name);
class cPlayer;
extern cPlayer* pPL;

// cFileList::init really takes the list buffer and the host directory (the symbol keeps the
// parameterless name); XmlSimple::SetXmlElemStart/End take the element name as well.
int FileListInit(cFileList* l, char* buf, const char* dir) __asm__("init__9cFileList");
int XmlElemStart(XmlSimple* x, char** cur, char* buf, const char* name) __asm__("SetXmlElemStart__9XmlSimplePiPc");
int XmlElemEnd(XmlSimple* x, char** cur, char* buf, const char* name) __asm__("SetXmlElemEnd__9XmlSimplePiPc");

// EvtDebug's leading fields: the event name and the header copy the tool fills at load
struct EvtDebugView {
    char name[0x20];   // 0x00
    EvtHdrCopy hdr;    // 0x20
};
#define EVTDBG ((EvtDebugView*) &EvtDebug)

// cFlag-style bit numbering (from the MSB of flags) over the tool's flag word
static inline u32 FlagBit(u32 f, u32 bit) { return f & bit; }
static inline void TE_FLG_ON(ToolEvt* t, int bit) { u32* p = (u32*) &t->flags; p[(u32) bit >> 5] |= 0x80000000 >> (bit & 0x1F); }
static inline void TE_FLG_OFF(ToolEvt* t, int bit) { u32* p = (u32*) &t->flags; p[(u32) bit >> 5] &= ~(0x80000000 >> (bit & 0x1F)); }

#define CAM_MOTION_FLAGS(p) (*(u16*) ((u8*) (p) + 0x40))

#define EVT_MES_Y (336 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1)

static inline int EvtStatusChk(Event* ev, u32 bit)
{
    return (ev->status & bit) ? 1 : 0;
}

// One "Node" record of the message xml: the eleven text elements in file order.
struct XmlNode {
    char s[11][0x10];
};
enum {
    XN_SETFLG,
    XN_SETOWNER,
    XN_SETEDIT,
    XN_NAMEPAC,
    XN_CUTNO,
    XN_FRAME,
    XN_COMFLAG,
    XN_SETBIN,
    XN_SETTPL,
    XN_DAT0,
    XN_DAT1
};
struct XmlNodeData {
    XmlNode node[100];
    int num;
};

#define XML_NODE_MAX 100
#define XML_BUF_SIZE 0x30D40

static inline void XmlNodeDataClear(XmlNodeData* d)
{
    XmlNode* n = d->node;
    int i;
    int j;

    i = XML_NODE_MAX;
    while (i--) {
        char* p = (char*) n;
        j = 11;
        while (j--) {
            memset(p, 0, 0x10);
            p += 0x10;
        }
        n++;
    }
}

static inline int XmlStrToBool(const char* s)
{
    if (strcmp(s, "true") == 0 || strcmp(s, "True") == 0) {
        return 1;
    }
    return strcmp(s, "TRUE") == 0;
}

static inline long XmlStrToLong(const char* s)
{
    long v;

    sscanf(s, "%ld", &v);
    return v;
}

// Reads the xml file into `d`: 0 when it is missing or too big.
// `size` is HDRead's result: the read itself is in the array owner (EvtMessRead), so HDRead's buffer
// argument is a fresh `addi r4,r1,N` (no pseudo equivalent to the address exists yet in that ebb).
static inline int EvtReadXml(const char* name, XmlNodeData* d, char* tmp, char* buf, u32 size)
{
    XmlSimple xml;
    char* cur;
    int i;

    buf[size] = 0;
    if (size > XML_BUF_SIZE - 1) {
        pLog->err(0, 0, "ReadXml : FileSize over [%d]", size);
        return 0;
    }
    if (size == 0) {
        pLog->err(0, 0, "ReadXml : File Not Found [%s]", name);
        return 0;
    }
    cur = buf;
    xml.GetXmlStart(&cur, cur, "Node");
    d->num = 0;
    for (i = 0; i < XML_NODE_MAX; i++) {
        XmlNode* n = &d->node[i];

        if (xml.GetXmlElem(tmp, cur, "SetFlg") == 1) {
            strcpy(n->s[XN_SETFLG], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "SetOwner") == 1) {
            strcpy(n->s[XN_SETOWNER], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "SetEdit") == 1) {
            strcpy(n->s[XN_SETEDIT], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "NamePac") == 1) {
            strcpy(n->s[XN_NAMEPAC], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "CutNo") == 1) {
            strcpy(n->s[XN_CUTNO], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "Frame") == 1) {
            strcpy(n->s[XN_FRAME], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "ComFlag") == 1) {
            strcpy(n->s[XN_COMFLAG], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "SetBin") == 1) {
            strcpy(n->s[XN_SETBIN], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "SetTpl") == 1) {
            strcpy(n->s[XN_SETTPL], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "Dat0") == 1) {
            strcpy(n->s[XN_DAT0], tmp);
        }
        if (xml.GetXmlElem(tmp, cur, "Dat1") == 1) {
            strcpy(n->s[XN_DAT1], tmp);
        }
        d->num++;
        if (xml.GetXmlNext(&cur, cur, "Node") == 0) {
            break;
        }
    }
    return 1;
}

// The message list from its xml file (path built by the caller).
static inline void EvtMessRead(EventMessageData* m, const char* path)
{
    XmlNodeData d;
    char tmp[0x80];
    char buf[XML_BUF_SIZE];
    u32 size;
    int i;

    XmlNodeDataClear(&d);
    m->num = 0;
    memset(m, 0, sizeof(m->elem));
    size = HDRead(path, buf);
    if (EvtReadXml(path, &d, tmp, buf, size) == 0) {
        pLog->err(0, 0, "ReadData : File Not Found [%s]", path);
        return;
    }
    m->num = d.num;
    for (i = 0; i < m->num; i++) {
        int on = XmlStrToBool(d.node[i].s[XN_SETFLG]);

        if (on == 1) {
            m->elem[i].flag = on;
            m->elem[i].no = i;
            m->elem[i].cutNo = XmlStrToLong(d.node[i].s[XN_CUTNO]);
            m->elem[i].frame = XmlStrToLong(d.node[i].s[XN_FRAME]);
            m->elem[i].messNo = XmlStrToLong(d.node[i].s[XN_DAT0]);
            m->elem[i].timer = XmlStrToLong(d.node[i].s[XN_DAT1]);
        } else {
            m->elem[i].flag = 0;
            m->elem[i].no = 0;
            m->elem[i].cutNo = 0;
            m->elem[i].frame = 0;
            m->elem[i].messNo = 0;
            m->elem[i].timer = 0;
        }
    }
}

// One-member struct: the tool pointer is a struct member (not a fixed scalar), so it is re-read after
// every store made through it (SubToolMessInit's Set*Func / callback stores reload it each time).
// Not static: the REL's `MessTool@l` fields hold 0 (a global's A only), a local's would hold S+A.
struct MessToolWork {
    cDbgToolMain<EventMessageData::MessElem>* p;
} MessTool;

// cDbgToolMain::CreateMenuWindow of this build of db_toolbase.h: the menu sits one row lower
// (Init(5, 4)) than in the Tools REL's header (Init(5, 3)).
template <class T> static inline void MessCreateMenuWindow(cDbgToolMain<T>* tool)
{
    cDbgWindow* w = new cDbgWindow;

    w->Init(5, 4, " MENU ");
    tool->pMenu = w;
    if (tool->pMenu == 0) {
        pLog->err(0, 0, "CreateMenuWindow(): new failed.");
        return;
    }
    tool->pMenu->AddButton(1, 0, "Edit  ", 0, 0, 0, 0);
    tool->pMenu->AddButton(1, 1, "Load  ", 0, 1, 0, 0);
    tool->pMenu->AddButton(1, 2, "Save  ", 0, 2, 0, 0);
    tool->pMenu->AddButton(1, 3, "Option", 0, 3, 0, 0);
    tool->pMenu->AddButton(1, 4, "Exit  ", 0, 4, 0, 0);
}

// the save / load callback setters (one tool-pointer read for the two stores of each)
template <class T> static inline void MessSetSaveFunc(cDbgToolMain<T>* tool, int (*f)(void*), void* arg)
{
    tool->pSaveFunc = f;
    tool->saveArg = arg;
}
template <class T> static inline void MessSetLoadFunc(cDbgToolMain<T>* tool, int (*f)(void*), void* arg)
{
    tool->pLoadFunc = f;
    tool->loadArg = arg;
}

int IsWorkAlive(EventMessageData::MessElem* w);
void SetWorkAlive(EventMessageData::MessElem* w, int alive);
int GetWorkNo(EventMessageData::MessElem* w);
void SetWorkNo(EventMessageData::MessElem* w, int no);
void InitWork(EventMessageData::MessElem* w, int no);
int CallbackCutNoExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
void CallbackCutNoUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
int CallbackFrameExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
void CallbackFrameUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
int CallbackMessNoExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
void CallbackMessNoUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
int CallbackTimerExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
void CallbackTimerUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b);
int CallbackSave(void* arg);
int CallbackLoad(void* arg);

void ToolEvent()
{
    ToolEvt tool;

    tool.Run();
}

void ToolEvt::EvtTaskSuspend(int task)
{
    if (!(flags & 0x4000)) {
        TaskSuspend(task);
        flags |= 0x4000;
    }
}

void ToolEvt::EvtTaskSignal(int task)
{
    if (flags & 0x4000) {
        TaskSignal(task);
        flags &= ~0x4000;
    }
}

ToolEvt::ToolEvt()
{
    char path[0x100];

    mode = 0;
    step = 0;
    x04 = 0;
    x06 = 0;
    subMode = 0;
    x0A = 0;
    x0C = 0;
    x0E = 0;
    stopWait = 0;
    startWait = 0;
    curveNo = 0;
    menuCur = 0;
    subCur = 0;
    fogCur = 0;
    focusCur = 0;
    camMode = 0;
    camCnt = 0;
    flags = 0;
    x18 = 0;
    x1C = 0;
    capCnt = 0;
    pEvd = 0;
    pLightTool = 0;
    pJoy0 = 0;
    pJoy1 = 0;
    x10C0[0] = 0;
    x10C0[1] = 0;
    x10C0[2] = 0;
    x10C0[3] = 0;
    x10C0[4] = 0;
    x10C0[5] = 0;
    x10C0[6] = 0;
    x10C0[7] = 0;
    x1108 = 0;
    EvtTaskSuspend(0);
    TutilInitDefault();
    if (pG->room_id == 0x10B) {
        EffectEspDelete(0, 2, 0, 0);
        EffectEspgenDelete(0, 2, 0);
        EffectEfmDelete(0, 2, 0);
        EffectEspDelete(0, 3, 0, 0);
        EffectEspgenDelete(0, 3, 0);
        EffectEfmDelete(0, 3, 0);
    }
    EvtMgr.ToolCoreEvdDel();
    sprintf(path, "%sr%x%02xs??.evd", "x:\\soft\\room\\event\\evd\\", pG->stage_no, pG->room_no);
    if (FileListInit(&DbgFileList, path, "x:\\soft\\room\\event\\evd\\") == 0) {
        flags |= 0x80000000;
    }
    pG->flags_60 |= 0x02000000;
    pEvd = Debug_alloc(8000000, 1);
    memclr_asm(pEvd, 4);
    pSctrl = (DbSctrlWork*) Debug_alloc(1000000, 1);
    memclr_asm(pSctrl, 1000000);
    pMess = (EventMessageData*) Debug_alloc(1000000, 1);
    memclr_asm(pMess, 1000000);
    menuCur = 0;
    pJoy0 = &Joy[0];
    pJoy1 = &Joy[1];
    subCur = 0;
    fogCur = 0;
    focusCur = 0;
    Cckpt.countDown.flags &= ~1;
    {
        CountDown* cd = &Cckpt.countDown;
        cd->frameOut();
        cd->frameOut();
    }
    LightMgr.roomLitSet(0);
    LightMgr.update(0, -1);
    pLightTool = new cLightTool;
}

ToolEvt::~ToolEvt()
{
    delete pLightTool;
    BitOff(pG->flags_60, 0x02000000);
    ((cUnitEventView*) pPL)->endEvent(0);
    EvtTaskSignal(0);
    TutilQuitDefault();
    TaskExit();
}

static void (*runTbl[3])(ToolEvt*) = {ToolEvt::MainMenu, ToolEvt::MainPreview, ToolEvt::MainExit};

void ToolEvt::Run()
{
    while ((int) flags >= 0) {
        runTbl[mode](this);
        TaskSleep(1);
    }
}

static inline void MessDeleteAll()
{
    MessageControl* mes = &cMes;
    int i;

    for (i = 0; i < 16; i++) {
        mes->Delete(i);
    }
}

void ToolEvt::RunStop(ToolEvt* t, Event* ev)
{
    int i;

    ev->status |= 0x80000000;
    EvtTaskSuspend(0);
    if ((t->pJoy0->on & 0x30000) || (t->pJoy0->trg & 0xC00)) {
        int flg;

        if (!(t->pJoy0->on & 0x10)) {
            // COMPILER-DIFF: candidate #12 (fallthrough-arm form): the original stores a fresh `li r0,0`; a
            // literal 0 here is related by cse (record_jump_equiv on the not-taken `bne`) to the `andi.`
            // result and that register is stored instead (t_mv mvInit). `(t & 8) >> 4` is a zero cse cannot
            // fold; combine folds it (nonzero_bits) to a fresh constant after cse2.
            t->stopWait = ((u32) t & 0x8) >> 4;
        }
        flg = 0;
        if (--t->stopWait <= 0) {
            t->stopWait = 10;
            flg = 1;
        }
        if (flg == 0 && !(t->pJoy0->trg & 0xC00)) {
            return;
        }
        FadeKill(0);
        EvtTaskSignal(0);
        ev->DebugDisp();
        if (t->pJoy0->trg & 0x400) {
            MessDeleteAll();
            ev->RunTool(2, 0);
        } else if (t->pJoy0->trg & 0x800) {
            MessDeleteAll();
            ev->RunTool(1, 0);
        } else if (t->pJoy0->on & 0x10000) {
            pG->flags_64 |= 0x01000000;
            ev->RunTool(0, 2);
        } else if (ev->Run() == 0) {
            pLog->err(0, 0, "EventMgr::Run : failed");
        }
    } else {
        t->stopWait = 0;
    }
}

static TOOL_MENU mainMenu[2] = {
    {1, "PREVIEW", 0},
    {1, "TOOL EXIT", 0},
};

void ToolEvt::MainMenu(ToolEvt* t)
{
    int sel;
    int zero = 0;

    eprintf(0x38, 0x30, 5, 0, "MENU");
    t->flags &= ~0x40000000;
    sel = ToolMenuDisp_cur(0x40, 0x40, 1, &t->menuCur, mainMenu, sizeof(mainMenu), t->pJoy0);
    if (sel != -1) {
        t->mode = sel + 1;
        t->step = zero;
        t->x04 = zero;
        t->x06 = zero;
    }
}

static TOOL_MENU previewMenu[3] = {
    {1, "YES", 0},
    {1, "NO", 0},
    {1, "CONVERT AND LOAD", 0},
};

static void (*subRunTbl[3])(ToolEvt*, Event*) = {ToolEvt::SubMenuMain, ToolEvt::SubMenuFog, ToolEvt::SubMenuFocus};

void ToolEvt::MainPreview(ToolEvt* t)
{
    char path[0x140];

    switch (t->step) {
    case 0:
        strcpy(t->fileName, DbgFileList.disp(0x40, 0x30, 0x14));
        if (t->pJoy0->trg & 0x100) {
            strcpy(EVTDBG->name, t->fileName);
            t->step++;
        }
        if (t->pJoy0->trg & 0x200) {
            t->mode = 0;
            t->step = 0;
            t->x04 = 0;
            t->x06 = 0;
        }
        break;
    case 1:
        memclr_asm(t->room, 0x10);
        memclr_asm(t->no, 0x10);
        sscanf(t->fileName, "%c%c%c%c%c%c%c.evd", &t->room[0], &t->room[1], &t->room[2], &t->room[3], &t->no[0],
               &t->no[1], &t->no[2]);
        eprintf(0x38, 0x30, 5, 0, "DATA LOAD OK?");
        switch (ToolMenuDisp(0x40, 0x40, 1, previewMenu, sizeof(previewMenu), t->pJoy0)) {
        case 0:
            sprintf(path, "%s/%s", "x:/soft/room/event/evd", EvtMgr.NameChange(t->fileName));
            HDRead(path, t->pEvd);
            t->step++;
            break;
        case 2:
            sprintf(path, "%s/%s", "x:/soft/room/event/evd", EvtMgr.NameChange(t->fileName));
            HDRead(path, t->pEvd);
            t->step++;
            break;
        default:
            if (!(t->pJoy0->trg & 0x200)) {
                break;
            }
        case 1:
            t->mode = 0;
            t->step = 0;
            t->x04 = 0;
            t->x06 = 0;
            break;
        }
        break;
    case 2: {
        Event* ev;

        t->EvtTaskSignal(0);
        SceEventStart(0);
        EvtHdrCopy* h = (EvtHdrCopy*) t->pEvd;

        t->hdr = *h;
        EvtDebugView* d = EVTDBG;

        d->hdr = *h;
        if (EvtMgr.SetEvt(t->pEvd, (u32*) &ev) == 0) {
            pLog->err(0, 0, "ToolEvt_Main_Preview : failed");
            t->mode = 1;
            t->step = 4;
            t->x04 = 0;
            t->x06 = 0;
            break;
        }
        t->flags &= ~0x02000000;
        t->SubToolFogWkInit(t, ev);
        t->SubToolFocusWkInit(t, ev);
        t->startWait = 1;
        ev->status |= 0x8000;
        t->step++;
        break;
    }
    case 3: {
        Event* ev;

        if (pG->flags_54 & 0x400) {
            pG->flags_54 &= ~0x400;
        }
        if (EvtMgr.GetEvt(&EvtMgr.x34, (void**) &ev) == 0) {
            pLog->err(0, 0, "ToolEvt_Main_Preview : failed");
            t->mode = 1;
            t->step = 4;
            t->x04 = 0;
            t->x06 = 0;
            break;
        }
        if (t->flags & 0x00040000) {
            t->SubToolLightMove(t);
        } else if (t->SubToolCameraMove(t) != 0) {
            break;
        }
        if (t->flags & 0x00020000) {
            t->SubToolFogMove(t, ev);
        }
        if (t->flags & 0x00010000) {
            t->SubToolFocusMove(t, ev);
        }
        if (t->pJoy1->trg & 0x400) {
            pG->debug_mode = 0;
        }
        if (t->pJoy1->trg & 0x800) {
            pG->debug_mode = 1;
        }
        if (t->flags & 0x40000000) {
            eprintf(0x1D0, 0x10, 0x16, 0, "STOP");
        }
        if (t->flags & 0x02000000) {
            subRunTbl[t->subMode](t, ev);
            break;
        }
        if (t->pJoy0->trg & 0x200) {
            t->flags |= 0x02000000;
        }
        if (t->flags & 0x01000000) {
            if (++t->capCnt > 1) {
                t->flags &= ~0x01000000;
                t->flags |= 0x00400000;
                if (t->flags & 0x00800000) {
                    ScreenShotStart("D:/bio4/Room/Sc_shot/r100", 0, 0);
                } else {
                    ScreenShotStart("D:/bio4/Room/Sc_shot/r100", 0, 1);
                }
            }
        }
        if (t->flags & 0x00400000) {
            if (t->flags & 0x00200000) {
                if (++t->capCnt > 1) {
                    t->flags &= ~0x00200000;
                    t->flags |= 0x02000000;
                    if (t->flags & 0x00400000) {
                        u32* fp = &t->flags;

                        *fp &= ~0x00400000;
                        pG->debug_mode = 1;
                        ScreenShotEnd();
                    }
                }
            }
            if ((ev->cut >= ev->maxCut && (t->flags & 0x00400000)) || (t->pJoy0->trg & 0x200)) {
                if (!(t->flags & 0x00200000)) {
                    t->flags |= 0x00200000;
                    t->capCnt = 0;
                }
            }
        }
        ev->DebugDispTool();
        if (t->startWait != 0) {
            if (--t->startWait <= 0) {
                t->startWait = 0;
                t->flags |= 0x40000000;
                ev->status |= 0x20000000;
            }
        }
        if ((!(t->flags & 0x40000000) && ((t->pJoy0->on & 0x30000) || (t->pJoy0->trg & 0xE00))) ||
            FlagBit(t->flags, 0x20000000) || FlagBit(t->flags, 0x10000000) || (t->pJoy0->trg & 0x100)) {
            t->flags ^= 0x40000000;
            if (t->flags & 0x20000000) {
                t->flags |= 0x40000000;
            }
            if (t->flags & 0x10000000) {
                t->flags &= ~0x40000000;
            }
            t->stopWait = 0;
            t->flags &= ~0x30000000;
            if (t->flags & 0x40000000) {
                t->EvtTaskSuspend(0);
                EventMgr* m = &EvtMgr;
                u32* pp = &m->x34;

                m->EvtSndStrStop(pp, 1, 0);
                m->EvtSndStrStop(pp, 0, 0);
            } else {
                t->EvtTaskSignal(0);
                if (!FlagBit(t->flags, 0x00400000) && !FlagBit(t->flags, 0x01000000)) {
                    if (EvtStatusChk(ev, 0x8000) == 0) {
                        ev->status |= 0x10000;
                        EvtDebug.strWait = 60;
                        SndAllStop();
                    }
                }
            }
            ev->status &= ~0x8000;
        }
        {
            u32* sp = &ev->status;

            *sp &= ~0x80000000;
            sp = &ev->status;
            *sp &= ~0x40000000;
        }
        pG->flags_64 &= ~0x01000000;
        if (t->flags & 0x40000000) {
            t->RunStop(t, ev);
        }
        if (t->flags & 0x8000) {
            t->SubToolMessMove(t, ev);
        }
        break;
    }
    case 4:
        t->EvtTaskSignal(0);
        TaskSleep(2);
        t->EvtTaskSuspend(0);
        EventMgr* m = &EvtMgr;
        u32* pp = &m->x34;

        m->EvtSndStrStop(pp, 1, 1);
        m->EvtSndStrStop(pp, 0, 1);
        SceEventEnd(0);
        if (!(t->flags & 0x00080000)) {
            t->mode = 0;
            t->step = 0;
            t->x04 = 0;
            t->x06 = 0;
        } else {
            t->flags |= 0x80000000;
        }
        break;
    }
}

static TOOL_MENU yesNoMenu[2] = {
    {1, "YES", 0},
    {1, "NO", 0},
};

void ToolEvt::MainExit(ToolEvt* t)
{
    eprintf(0x38, 0x30, 5, 0, "EXIT OK?");
    switch (ToolMenuDisp(0x40, 0x40, 3, yesNoMenu, sizeof(yesNoMenu), t->pJoy0)) {
    case 0:
        t->flags |= 0x80000000;
    default:
        if (!(t->pJoy0->trg & 0x200)) {
            break;
        }
    case 1:
        t->mode = 0;
        t->step = 0;
        t->x04 = 0;
        t->x06 = 0;
        break;
    }
}

void ToolEvt::EventDel(Event* ev)
{
    EvtTaskSignal(0);
    ev->status &= ~0x20000000;
    ev->status |= 0x00020000;
    ev->RunEvtCancel();
    EvtMgr.DelEvt(ev, 0);
    ev->status &= ~0x00020000;
}

static TOOL_MENU subMainMenu[8] = {
    {1, "CONTINUE", 0},
    {1, "LIGHT TOOL", 0},
    {1, "ESP   TOOL", 0},
    {1, "FOG   TOOL", 0},
    {1, "FOCUS TOOL", 0},
    {1, "MESS  TOOL", 0},
    {1, "CAPTURE", 0},
    {1, "PREVIEW EXIT", 0},
};

void ToolEvt::SubMenuMain(ToolEvt* t, Event* ev)
{
    eprintf(0x38, 0x30, 5, 0, "PREVIEW MENU");
    switch (ToolMenuDisp_cur(0x40, 0x40, 1, &t->subCur, subMainMenu, sizeof(subMainMenu), t->pJoy0)) {
    case 0:
        t->flags &= ~0x02000000;
        break;
    case 1:
        if (!(t->flags & 0x00040000)) {
            t->SubToolLightInit(t, 1);
        } else {
            t->SubToolLightInit(t, 0);
        }
        break;
    case 2:
        ev->EspToolSetDat();
        t->flags |= 0x00080000;
        EvtDebug.flags |= 0x80000000;
        DbMenuSetExecTool("ESP TOOL");
        t->EventDel(ev);
        t->step = 4;
        break;
    case 3:
        t->subMode = 1;
        t->x0A = 0;
        t->x0C = 0;
        t->x0E = 0;
        t->fogCur = 0;
        break;
    case 4:
        t->subMode = 2;
        t->x0A = 0;
        t->x0C = 0;
        t->x0E = 0;
        t->focusCur = 0;
        break;
    case 5:
        if (!(t->flags & 0x8000)) {
            t->SubToolMessInit(t, 1);
        } else {
            t->SubToolMessInit(t, 0);
        }
        break;
    case 6:
        t->capCnt = 0;
        t->flags |= 0x11000000;
        t->flags &= ~0x02000000;
        t->flags &= ~0x00800000;
        if (t->pJoy0->on & 0x10) {
            t->flags |= 0x00800000;
        } else {
            pG->debug_mode = 0;
        }
        break;
    case 7:
        t->EventDel(ev);
        t->step = 4;
        break;
    }
}

static TOOL_MENU fogMenu[6] = {
    {1, "EDIT START", 0},
    {1, "EDIT END", 0},
    {1, "INIT", 0},
    {1, "LOAD", 0},
    {1, "SAVE", 0},
    {1, "FOG TOOL END", 0},
};

void ToolEvt::SubMenuFog(ToolEvt* t, Event* ev)
{
    char dir[0x100];
    char path[0x100];
    char name[0x100];

    strcpy(dir, "x:/soft/room/event");
    sprintf(path, "%s/%s/%s/etc/%s_%03d.fog", dir, t->room, t->no, t->no, ev->cut);
    sprintf(name, "[%s_%03d.fog]", t->no, ev->cut);
    ev->FogMove(ev, &t->fog);
    eprintf(0x38, 0x30, 5, 0, "FOG TOOL MENU");
    switch (ToolMenuDisp_cur(0x40, 0x40, 1, &t->fogCur, fogMenu, sizeof(fogMenu), t->pJoy0)) {
    case 0:
        if (!(t->flags & 0x00020000)) {
            t->SubToolFogInit(t, 1, ev, 0);
        } else {
            t->SubToolFogInit(t, 0, 0, 0);
        }
        break;
    case 1:
        if (!(t->flags & 0x00020000)) {
            t->SubToolFogInit(t, 1, ev, 1);
        } else {
            t->SubToolFogInit(t, 0, 0, 0);
        }
        break;
    case 2:
        if (t->SubMenuSelectYesNo(t, "INIT", "")) {
            memset(&t->fog, 0, sizeof(EvtFogData));
        }
        t->SubToolFogWkInit(t, ev);
        break;
    case 3:
        if (t->SubMenuSelectYesNo(t, "LOAD", name)) {
            HDRead(path, &t->fog);
        }
        break;
    case 4:
        if (t->SubMenuSelectYesNo(t, "SAVE", name)) {
            HDWrite(path, &t->fog, sizeof(EvtFogData));
        }
        break;
    case 5:
        if (t->SubMenuSelectYesNo(t, "EXIT", "")) {
            t->subMode = 0;
        }
        break;
    }
}

static TOOL_MENU focusMenu[8] = {
    {1, "EDIT NEAR", 0},
    {1, "EDIT FAR", 0},
    {1, "LEVEL NEAR", 0},
    {1, "LEVEL FAR", 0},
    {1, "INIT", 0},
    {1, "LOAD", 0},
    {1, "SAVE", 0},
    {1, "FOCUS TOOL END", 0},
};

void ToolEvt::SubMenuFocus(ToolEvt* t, Event* ev)
{
    char dir[0x100];
    char path[0x100];
    char name[0x100];

    strcpy(dir, "x:/soft/room/event");
    sprintf(path, "%s/%s/%s/etc/%s_%03d.fcs", dir, t->room, t->no, t->no, ev->cut);
    sprintf(name, "[%s_%03d.fcs]", t->no, ev->cut);
    ev->FocusMove(ev, &t->focus);
    eprintf(0x38, 0x30, 5, 0, "FOCUS TOOL MENU");
    switch (ToolMenuDisp_cur(0x40, 0x40, 1, &t->focusCur, focusMenu, sizeof(focusMenu), t->pJoy0)) {
    case 0:
        if (!(t->flags & 0x00010000)) {
            t->SubToolFocusInit(t, 1, ev, 0);
        } else {
            t->SubToolFocusInit(t, 0, 0, 0);
        }
        break;
    case 1:
        if (!(t->flags & 0x00010000)) {
            t->SubToolFocusInit(t, 1, ev, 1);
        } else {
            t->SubToolFocusInit(t, 0, 0, 0);
        }
        break;
    case 2:
        t->SubMenuEditFocusLevel(t, ev, "NEAR", &t->focus.nearLevel);
        break;
    case 3:
        t->SubMenuEditFocusLevel(t, ev, "FAR ", &t->focus.farLevel);
        break;
    case 4:
        if (t->SubMenuSelectYesNo(t, "INIT", "")) {
            memset(&t->focus, 0, sizeof(EvtFocusData));
        }
        t->SubToolFocusWkInit(t, ev);
        break;
    case 5:
        if (t->SubMenuSelectYesNo(t, "LOAD", name)) {
            HDRead(path, &t->focus);
        }
        break;
    case 6:
        if (t->SubMenuSelectYesNo(t, "SAVE", name)) {
            HDWrite(path, &t->focus, sizeof(EvtFocusData));
        }
        break;
    case 7:
        if (t->SubMenuSelectYesNo(t, "EXIT", "")) {
            t->subMode = 0;
        }
        break;
    }
}

static TOOL_MENU yesNoMenu2[2] = {
    {1, "YES", 0},
    {1, "NO", 0},
};

int ToolEvt::SubMenuSelectYesNo(ToolEvt* t, const char* s1, const char* s2)
{
    TaskSleep(1);
    for (;;) {
        eprintf(0x38, 0x30, 5, 0, "%s %s OK?", s1, s2);
        switch (ToolMenuDisp(0x40, 0x40, 3, yesNoMenu2, sizeof(yesNoMenu2), t->pJoy0)) {
        case 0:
            return 1;
        case 1:
            return 0;
        }
        TaskSleep(1);
    }
}

int ToolEvt::SubMenuEditFocusLevel(ToolEvt* t, Event* ev, const char* name, f32* level)
{
    TaskSleep(1);
    while (1) {
        eprintf(0x38, 0x30, 0x16, 0, "EDIT FOCUS LEVEL [%s] : %f", name, *level);
        if (Joy[0].rep & 0x10000) {
            *level -= 1.0f;
        }
        if (Joy[0].rep & 0x20000) {
            *level += 1.0f;
        }
        if (Joy[0].rep & 0x1) {
            *level -= 0.1f;
        }
        if (Joy[0].rep & 0x2) {
            *level += 0.1f;
        }
        if (*level < 0.0f) {
            *level = 0.0f;
        }
        if (*level > 10.0f) {
            *level = 10.0f;
        }
        if (FlagBit(t->pJoy0->trg, 0x100) || FlagBit(t->pJoy0->trg, 0x200)) {
            break;
        }
        ev->FocusMove(ev, &t->focus);
        TaskSleep(1);
    }
    return 0;
}

int ToolEvt::SubToolCameraMove(ToolEvt* /*t*/)
{
    if (camMode != 0) {
        if (pJoy0->trg & 0x1000) {
            camMode = 0;
            if (!(pG->flags_60 & 0x10000000)) {
                pG->flags_170 &= ~0x40000000;
            }
        } else {
            CamDbg.move(&pG->Cam, &Joy[0], 0);
            if (camCnt++ & 8) {
                eprintf2(0xE, 0x12, 0xAA, 0x18, 6, 0, "CAMERA MODE");
            }
            if (pJoy0->trg & 0x200) {
                CAM_MOTION_FLAGS(CamCtrl.getMotionInfoPtr()) |= 8;
                pG->flags_170 &= ~0x40000000;
            } else {
                CAM_MOTION_FLAGS(CamCtrl.getMotionInfoPtr()) &= ~8;
                pG->flags_170 |= 0x40000000;
            }
            CameraMove();
        }
        return 1;
    }
    if (pJoy0->trg & 0x1000) {
        camMode = 1;
        flags |= 0x20000000;
    }
    return 0;
}

void ToolEvt::SubToolLightInit(ToolEvt* t, int sw)
{
    int i;

    if (sw == 1) {
        MessDeleteAll();
        EvtDebug.flags |= 0x20000000;
        pG->flags_60 |= 0x20000000;
    } else {
        EvtDebug.flags &= ~0x20000000;
        BitOff(pG->flags_170, 0x40000000);
        pG->flags_60 &= ~0x20000000;
        TaskSleep(1);
    }
    SubToolIn(t, sw, 13);
}

void ToolEvt::SubToolLightMove(ToolEvt* /*t*/)
{
    cLightTool* lt = pLightTool;

    if ((u32) lt >= 0x80000000 && (u32) lt <= 0x82FFFFFF) {
        if (lt->move() == 0) {
            SubToolLightInit(this, 0);
        }
        View.move();
    }
}

int ToolEvt::SubToolFogWkInit(ToolEvt* t, Event* ev)
{
    t->fog.start.num = 2;
    t->fog.start.key[0].t = 0.0f;
    t->fog.start.key[0].v = LightMgr.getFogStart();
    t->fog.start.key[0].out = 0.0f;
    t->fog.start.key[0].in = 0.0f;
    t->fog.start.key[1].t = (f32) ev->maxFrame;
    t->fog.start.key[1].v = LightMgr.getFogStart();
    t->fog.start.key[1].out = 0.0f;
    t->fog.start.key[1].in = 0.0f;
    t->fog.end.num = 2;
    t->fog.end.key[0].t = 0.0f;
    t->fog.end.key[0].v = LightMgr.getFogEnd();
    t->fog.end.key[0].out = 0.0f;
    t->fog.end.key[0].in = 0.0f;
    t->fog.end.key[1].t = (f32) ev->maxFrame;
    t->fog.end.key[1].v = LightMgr.getFogEnd();
    t->fog.end.key[1].out = 0.0f;
    t->fog.end.key[1].in = 0.0f;
    return 1;
}

void ToolEvt::SubToolFogInit(ToolEvt* t, int sw, Event* ev, int which)
{
    if (sw == 1) {
        EvtDebug.flags |= 0x10000000;
        if (ev->cut > 99) {
            return;
        }
        if (which == 0) {
            t->SctrlToolInit(t, (Hermite1*) &t->fog.start, (f32) ev->maxFrame, 100000.0f);
        } else {
            t->SctrlToolInit(t, (Hermite1*) &t->fog.end, (f32) ev->maxFrame, 100000.0f);
        }
        t->curveNo = which;
    } else {
        EvtDebug.flags &= ~0x10000000;
        TaskSleep(1);
    }
    SubToolIn(t, sw, 14);
}

void ToolEvt::SubToolFogMove(ToolEvt* t, Event* ev)
{
    if (DbSctrl(t->pSctrl, 0x20, 0x20) == 0) {
        SubToolFogInit(t, 0, 0, 0);
    }
    if (t->curveNo == 0) {
        eprintf(0x38, 0x30, 0x16, 0, "FOG START");
    } else {
        eprintf(0x38, 0x30, 0x16, 0, "FOG END");
    }
    ev->FogMove(ev, &t->fog);
}

void ToolEvt::SubToolFocusWkInit(ToolEvt* t, Event* ev)
{
    t->focus.near_.num = 2;
    t->focus.near_.key[0].t = 0.0f;
    t->focus.near_.key[0].v = 0.0f;
    t->focus.near_.key[0].out = 0.0f;
    t->focus.near_.key[0].in = 0.0f;
    t->focus.near_.key[1].t = (f32) ev->maxFrame;
    t->focus.near_.key[1].v = 0.0f;
    t->focus.near_.key[1].out = 0.0f;
    t->focus.near_.key[1].in = 0.0f;
    t->focus.far_.num = 2;
    t->focus.far_.key[0].t = 0.0f;
    t->focus.far_.key[0].v = 10000.0f;
    t->focus.far_.key[0].out = 0.0f;
    t->focus.far_.key[0].in = 0.0f;
    t->focus.far_.key[1].t = (f32) ev->maxFrame;
    t->focus.far_.key[1].v = 10000.0f;
    t->focus.far_.key[1].out = 0.0f;
    t->focus.far_.key[1].in = 0.0f;
    t->focus.nearLevel = 5.0f;
    t->focus.farLevel = 5.0f;
}

void ToolEvt::SubToolFocusInit(ToolEvt* t, int sw, Event* ev, int which)
{
    if (sw == 1) {
        EvtDebug.flags |= 0x08000000;
        if (ev->cut > 99) {
            return;
        }
        if (which == 0) {
            t->SctrlToolInit(t, (Hermite1*) &t->focus.near_, (f32) ev->maxFrame, 10000.0f);
        } else {
            t->SctrlToolInit(t, (Hermite1*) &t->focus.far_, (f32) ev->maxFrame, 10000.0f);
        }
        t->curveNo = which;
    } else {
        EvtDebug.flags &= ~0x08000000;
        TaskSleep(1);
    }
    SubToolIn(t, sw, 15);
}

void ToolEvt::SubToolFocusMove(ToolEvt* t, Event* ev)
{
    if (DbSctrl(t->pSctrl, 0x20, 0x20) == 0) {
        SubToolFocusInit(t, 0, 0, 0);
    }
    if (t->curveNo == 0) {
        eprintf(0x38, 0x30, 0x16, 0, "FOCUS NEAR");
    } else {
        eprintf(0x38, 0x30, 0x16, 0, "FOCUS FAR");
    }
    ev->FocusMove(ev, &t->focus);
}

void ToolEvt::SubToolMessInit(ToolEvt* t, int sw)
{
    EventMessageData* m = t->pMess;
    int i;

    if (sw == 1) {
        char path[0x80];

        EvtDebug.flags |= 0x04000000;
        MessDeleteAll();
        sprintf(path, "%s/evt_%s%s_mes.xml", "x:/soft/room/event/evd", t->room, t->no);
        // COMPILER-DIFF: candidate (gcse PRE pseudo numbering): 35 dead pseudos before the inlined
        // clear loop put its `i - 1` PRE pseudo in a lower hash bucket than `n + 1` (allocated first -> higher register).
        int dead0, dead1, dead2, dead3, dead4, dead5, dead6, dead7, dead8, dead9, dead10, dead11, dead12, dead13, dead14, dead15, dead16, dead17, dead18, dead19, dead20, dead21, dead22, dead23, dead24, dead25, dead26, dead27, dead28, dead29, dead30, dead31, dead32, dead33, dead34;
        // COMPILER-DIFF: candidate (gcse table size): the edit-window ctor anchor (dbg_tool.h) is one
        // more insn at gcse entry (1219 -> 1220), which turns the expression hash table from 609 to
        // 611 buckets and wraps `t->room`/`t->no` (raw hashes 17713/17729 -> buckets 605/10) and the
        // clear loop's `i - 1`/`p + 0xb0` (13400/13574) into the wrong PRE numbering = the wrong
        // allocno order on their priority ties. 17 insns nobody ever executes (set, cmpwi, branch,
        // 7 x mulli+addi) move it to 619 buckets, where both pairs are in the original order. cse1
        // cannot see `z` past the LOOP_END note, cse2 folds the test, the arm is unreachable and the
        // set dead before sched1: no code, no live-length change.
        {
            int z = 0;
            do { } while (0);
            if (z > 128) { z = z * 77 + 1; z = z * 78 + 2; z = z * 79 + 3; z = z * 80 + 4; z = z * 81 + 5; z = z * 82 + 6; z = z * 83 + 7; }
        }
        EvtMessRead(m, path);
        MessTool.p = new cDbgToolMain<EventMessageData::MessElem>;
        MessCreateMenuWindow(MessTool.p);
        MessTool.p->CreateFileWindows(0x16, 0xA, "X:\\Soft\\Room\\event\\", "test", ".txt");
        MessSetSaveFunc(MessTool.p, CallbackSave, t);
        MessSetLoadFunc(MessTool.p, CallbackLoad, t);
        // COMPILER-DIFF: candidate #5 (sched1 tie): the original issues `loadArg = t` before `pLoadFunc =
        // CallbackLoad` (both prio 13, LUID order ours); a codeless anchor keeps the CallbackLoad address
        // alive past its store so the store no longer kills a register (INSN_REG_WEIGHT 0 vs -1).
        asm("" : "=m"(path[0]) : "r"(CallbackLoad));
        MessTool.p->CreateEditWindow(0xA, 4, m->elem, "No  ==CutNo== ==Frame== ==MessNo= ==Timer==", 5, XML_NODE_MAX);
        MessTool.p->AddEditColumn(4, "         ", 1, CallbackCutNoExec, CallbackCutNoUpdate);
        MessTool.p->AddEditColumn(0xE, "         ", 2, CallbackFrameExec, CallbackFrameUpdate);
        MessTool.p->AddEditColumn(0x18, "         ", 3, CallbackMessNoExec, CallbackMessNoUpdate);
        MessTool.p->AddEditColumn(0x22, "         ", 4, CallbackTimerExec, CallbackTimerUpdate);
        MessTool.p->SetIsWorkAliveFunc(IsWorkAlive);
        MessTool.p->SetSetWorkAliveFunc(SetWorkAlive);
        MessTool.p->SetGetWorkNoFunc(GetWorkNo);
        MessTool.p->SetSetWorkNoFunc(SetWorkNo);
        MessTool.p->SetInitWorkFunc(InitWork);
        MessTool.p->InitAllWork();
        CallbackLoad(t);
    } else {
        EvtDebug.flags &= ~0x04000000;
        MessDeleteAll();
        if (MessTool.p) {
            delete MessTool.p;
        }
        TaskSleep(1);
    }
    SubToolIn(t, sw, 16);
}

void ToolEvt::SubToolMessMove(ToolEvt* t, Event* ev)
{
    EventMessageData::MessElem unused; // COMPILER-DIFF: frame-only T local (24 bytes) of the original
    int i;

    if (MessTool.p->Update() == 0) {
        SubToolMessInit(t, 0);
        return;
    }
    MessTool.p->Disp();
    if (t->flags & 0x40000000) {
        // the message column (cx 3) of the cursor row shows its message
        int cx = MessTool.p->pEdit->GetCx();
        int no = MessTool.p->pEdit->GetCurrentNo();

        if (cx == 3) {
            EventMessageData* m;
            EventMessageData::MessElem* e;

            m = t->pMess;
            e = &m->elem[no];

            if (IsWorkAlive(e)) {
                if (e->messNo == -1) {
                    EventMessageData::MessElem* p = 0;
                    int cnt = 1;
                    int j;
                    int k;

                    // Back-search over the preceding -1 records, hand-peeled: the target's loop is
                    // loop.c-shaped (`subi p; addi cnt; subic. k; blt; lwzu messNo; mr p; cmpwi; beq`)
                    // with giv inits `(m + no*24) - 8` (reload_cse'd to `mr rT,e; subi rT,rT,8`) and
                    // `e - 24`; no loop.c spelling found gives biv init `no` with a reduced `k - 1`
                    // giv, so the induction variables are written out. The giv init `m + no*24` is
                    // spelled `m - (-(no*24))` so cse does not fold it into `e` (a different
                    // expression until combine makes it `add T,m,A`, which reload_cse then rewrites
                    // to the target's `mr T,e`); a plain `(u8*) m + ofs` is cse'd to `subi T,e,8`.
                    k = no - 1;
                    if (k >= 0 && (p = &m->elem[k])->messNo == -1) {
                        EventMessageData::MessElem* q = e - 1;
                        u32 ofs = no * sizeof(EventMessageData::MessElem);
                        s32 nofs = -(s32) ofs;
                        u8* base = (u8*) m - nofs;
                        s32* mp = (s32*) (base - 8);
                        do {
                            q--;
                            cnt++;
                            if (--k < 0) {
                                break;
                            }
                            mp -= 6;
                            p = q;
                        } while (*mp == -1);
                    }
                    ev->MesSet(p->messNo, 0, 100, EVT_MES_Y);
                    for (j = 0; j < cnt; j++) {
                        cMes.Move();
                        ev->MesSet(-1, 0, 100, EVT_MES_Y);
                    }
                    cMes.Move();
                } else {
                    ev->MesSet(e->messNo, 0, 100, EVT_MES_Y);
                }
            }
        }
    }
    {
        EventMessageData::MessElem* e;
        // The mesCnt block of the original: a pointer to the struct address (`addi rB,rD,0xc4`) for
        // mesCnt[2]/[1] (`4(rB)`, `0(rB)`) and a second one formed after the fourth eprintf for
        // mesCnt[0] (`lwzu`, the same register carries it into the loop's `stwx no,rA,no`); the
        // loop's mesCnt[1]/[2] stores go through a third, loop-fresh pointer (hoisted, `mr r26,r28`).
        // Only a struct pointer with a leading array reproduces this: `p->v[k]` is an ARRAY_REF
        // whose address stays inside the MEM (`(plus rB idx)`: cse leaves it, combine folds the
        // zero index), a plain `s32*` computes the address as a value that cse rewrites to
        // `0xc4(rD)`, and a reference/pointer to array is pointer arithmetic in this frontend.
        // A struct pointer also keeps `&EvtDebug` the cse class head (a bare `&EvtDebug.mesCnt[1]`
        // makes the `EvtDebug+0xc4` constant the head and derives `&EvtDebug` from it with a `subi`).
        struct MesCntView { s32 v[3]; };
        EventDebug* d = &EvtDebug;
        MesCntView* m = (MesCntView*) &d->mesCnt[1];
        MesCntView* x;

        i = 0;
        eprintf(0x50, 0x90, 0, 0, "%3d", m->v[1]);
        eprintf(0xA0, 0x90, 0, 0, "%3d", ev->cut);
        eprintf(0xF0, 0x90, 0, 0, "%3d", ev->frame);
        eprintf(0x140, 0x90, 0, 0, "%3d", m->v[i]);
        x = (MesCntView*) &d->mesCnt[0];
        eprintf(0x190, 0x90, 0, 0, "%3d", x->v[i]);
        e = t->pMess->elem;
        for (i = 0; i < XML_NODE_MAX; i++, e++) {
            if (IsWorkAlive(e) && ev->cut == e->cutNo && ev->frame == e->frame) {
                int no = 0;
                int mes;
                MesCntView* y = (MesCntView*) &d->mesCnt[1];

                ev->MesSet(e->messNo, e->timer, 100, EVT_MES_Y);
                // the record's message number is re-read into a local before the three stores
                // (sched1: the load ahead of `stwx no`, then the stores in statement order)
                mes = e->messNo;
                x->v[no] = no;
                y->v[no] = mes;
                y->v[1] = i;
            }
        }
    }
}

void ToolEvt::SubToolIn(ToolEvt* t, int sw, int bit)
{
    if (sw == 1) {
        t->flags &= ~0x02000000;
        TE_FLG_ON(t, bit);
        t->pJoy0 = &Joy[2];
        t->pJoy1 = &Joy[3];
    } else {
        t->flags |= 0x02000000;
        TE_FLG_OFF(t, bit);
        t->pJoy0 = &Joy[0];
        t->pJoy1 = &Joy[1];
    }
}

void ToolEvt::SctrlToolInit(ToolEvt* t, Hermite1* curve, f32 xMax, f32 yMax)
{
    memset(t->pSctrl, 0, sizeof(DbSctrlWork));
    t->pSctrl->curve = curve;
    SctrlSetAxisLabel(t->pSctrl, "Frame", "Param");
    t->pSctrl->gridX = xMax;
    t->pSctrl->gridY = yMax;
    t->pSctrl->grid.x = 1.0f;
    t->pSctrl->grid.y = 1.0f;
    t->pSctrl->flags = 1;
    SctrlInitAxisRange(t->pSctrl, xMax * 1.2f, xMax * -0.2f, yMax * 1.2f, yMax * -0.2f);
    if (t->pSctrl->curve->num <= 1) {
        SctrlInitCursor(t->pSctrl, 0.0f, 0.0f);
    } else {
        SctrlInitCursor(t->pSctrl, t->pSctrl->curve->key[0].t, t->pSctrl->curve->key[0].v);
    }
}

int IsWorkAlive(EventMessageData::MessElem* w)
{
    if (w->flag & 1) {
        return 1;
    }
    return 0;
}

void SetWorkAlive(EventMessageData::MessElem* w, int alive)
{
    if (alive == 1) {
        w->flag |= 1;
    } else {
        w->flag &= ~1;
    }
}

int GetWorkNo(EventMessageData::MessElem* w)
{
    return w->no;
}

void SetWorkNo(EventMessageData::MessElem* w, int no)
{
    w->no = no;
}

void InitWork(EventMessageData::MessElem* w, int no)
{
    memclr_asm(w, sizeof(EventMessageData::MessElem));
    w->no = no;
}

// pad step of the value editors: -1 / +1 (x10 with A held)
static inline int EvtEditStep()
{
    int step = 0;

    if (Joy[0].rep & 0x10001) {
        step = -1;
    }
    if (Joy[0].rep & 0x20002) {
        step = 1;
    }
    if (Joy[0].on & 0x100) {
        step *= 10;
    }
    return step;
}

// exec callbacks return 1 while editing
static inline int EvtEditDone()
{
    u32 t = Joy[0].trg & 0x200;
    return t == 0;
}

int CallbackCutNoExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    w->cutNo += EvtEditStep();
    eprintf(0xAA, 0xA0, 4, 0, "CutNo   : ");
    eprintf(0xAA, 0xA0, 0, 0, "          %d", w->cutNo);
    return EvtEditDone();
}

void CallbackCutNoUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    char buf[0x40];

    sprintf(buf, "%9ld", w->cutNo);
    DbgButtonSetName(b, buf);
}

int CallbackFrameExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    w->frame += EvtEditStep();
    eprintf(0xAA, 0xA0, 4, 0, "Frame   : ");
    eprintf(0xAA, 0xA0, 0, 0, "          %d", w->frame);
    return EvtEditDone();
}

void CallbackFrameUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    char buf[0x40];

    sprintf(buf, "%9ld", w->frame);
    DbgButtonSetName(b, buf);
}

int CallbackMessNoExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    w->messNo += EvtEditStep();
    if (w->messNo < -1) {
        w->messNo = -1;
    }
    eprintf(0xAA, 0xA0, 4, 0, "MessNo  : ");
    eprintf(0xAA, 0xA0, 0, 0, "          %d", w->messNo);
    return EvtEditDone();
}

void CallbackMessNoUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    char buf[0x40];

    sprintf(buf, "%9ld", w->messNo);
    DbgButtonSetName(b, buf);
}

int CallbackTimerExec(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    w->timer += EvtEditStep();
    eprintf(0xAA, 0xA0, 4, 0, "Timer   : ");
    eprintf(0xAA, 0xA0, 0, 0, "          %d", w->timer);
    return EvtEditDone();
}

void CallbackTimerUpdate(int no, EventMessageData::MessElem* w, cDbgButtonTemplate<EventMessageData::MessElem>* b)
{
    char buf[0x40];

    sprintf(buf, "%9ld", w->timer);
    DbgButtonSetName(b, buf);
}

// Writes the node records of `d` as the message xml into buf and saves it as `name`.
// Returns the write cursor; the HDWrite is in the array owner (EvtMessWrite): its `cur - buf` forces
// the buffer address into a pseudo right before the call, which cse then uses for the r4 argument
// (`mr r4,r14`), where a `buf` parameter would be substituted into a fresh `addi`.
static inline char* EvtWriteXml(XmlNodeData* d, char* tmp, char* buf)
{
    XmlSimple xml;
    char* cur;
    int i;

    cur = buf;
    xml.SetXmlStart((int*) &cur, buf);
    for (i = 0; i < d->num; i++) {
        XmlNode* n = &d->node[i];

        XmlElemStart(&xml, &cur, cur, "Node");
        strcpy(tmp, n->s[XN_SETFLG]);
        xml.SetXmlElem((int*) &cur, cur, "SetFlg", tmp);
        strcpy(tmp, n->s[XN_SETOWNER]);
        xml.SetXmlElem((int*) &cur, cur, "SetOwner", "3");
        strcpy(tmp, n->s[XN_SETEDIT]);
        xml.SetXmlElem((int*) &cur, cur, "SetEdit", "true");
        strcpy(tmp, n->s[XN_NAMEPAC]);
        xml.SetXmlElem((int*) &cur, cur, "NamePac", "\203\201\203b\203Z\201[\203W");
        strcpy(tmp, n->s[XN_CUTNO]);
        xml.SetXmlElem((int*) &cur, cur, "CutNo", tmp);
        strcpy(tmp, n->s[XN_FRAME]);
        xml.SetXmlElem((int*) &cur, cur, "Frame", tmp);
        strcpy(tmp, n->s[XN_COMFLAG]);
        xml.SetXmlElem((int*) &cur, cur, "ComFlag", "0");
        strcpy(tmp, n->s[XN_SETBIN]);
        xml.SetXmlElem((int*) &cur, cur, "SetBin", "false");
        strcpy(tmp, n->s[XN_SETTPL]);
        xml.SetXmlElem((int*) &cur, cur, "SetTpl", "false");
        strcpy(tmp, n->s[XN_DAT0]);
        xml.SetXmlElem((int*) &cur, cur, "Dat0", tmp);
        strcpy(tmp, n->s[XN_DAT1]);
        xml.SetXmlElem((int*) &cur, cur, "Dat1", tmp);
        XmlElemEnd(&xml, &cur, cur, "Node");
    }
    xml.SetXmlEnd((int*) &cur, cur);
    return cur;
}

// The message list to its xml file (path built by the caller).
static inline void EvtMessWrite(EventMessageData* m, const char* path)
{
    XmlNodeData d;
    char tmp[0x80];
    char buf[XML_BUF_SIZE];
    char* cur;
    EventMessageData::MessElem* e;
    int i;

    XmlNodeDataClear(&d);
    d.num = 0;
    for (i = 0; i < XML_NODE_MAX; i++) {
        e = &m->elem[i];
        if (e->flag & 1) {
            // integer arithmetic keeps the written order (`mulli; add rMul, rBase; addi off`);
            // `d.node[d.num]` puts the base first.
#define CUR_NODE ((XmlNode*) (d.num * sizeof(XmlNode) + (u32) d.node))
            sprintf(CUR_NODE->s[XN_SETFLG], "true");
            sprintf(CUR_NODE->s[XN_CUTNO], "%ld", e->cutNo);
            sprintf(CUR_NODE->s[XN_FRAME], "%ld", e->frame);
            sprintf(CUR_NODE->s[XN_DAT0], "%ld", e->messNo);
            sprintf(CUR_NODE->s[XN_DAT1], "%ld", e->timer);
#undef CUR_NODE
            d.num++;
        }
    }
    cur = EvtWriteXml(&d, tmp, buf);
    HDWrite(path, buf, cur - buf);
}

int CallbackSave(void* arg)
{
    ToolEvt* t = (ToolEvt*) arg;
    EventMessageData* m = t->pMess;
    // COMPILER-DIFF: candidate (gcse PRE pseudo numbering): 13 dead pseudos before the inlined
    // clear loop put its `i - 1` PRE pseudo in a lower hash bucket than `n + 1` (allocated first -> higher register).
    int dead0, dead1, dead2, dead3, dead4, dead5, dead6, dead7, dead8, dead9, dead10, dead11, dead12;
    char path[0x100];

    sprintf(path, "%s/evt_%s%s_mes.xml", "x:/soft/room/event/evd", t->room, t->no);
    EvtMessWrite(m, path);
    return 0;
}

int CallbackLoad(void* arg)
{
    ToolEvt* t = (ToolEvt*) arg;
    EventMessageData* m = t->pMess;
    char path[0x100];

    sprintf(path, "%s/evt_%s%s_mes.xml", "x:/soft/room/event/evd", t->room, t->no);
    EvtMessRead(m, path);
    return 0;
}
