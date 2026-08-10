// game/event: the cutscene / event player (D:/Bio4/Prog/event.cpp). An Event work plays the
// packet stream of an "even""t" file (models, camera, motions, effects, messages, streams) cut by
// cut; EventMgr owns the loaded data tables and the running event; EventDebug is the t_event
// tool state; DatTbl is the name -> data slot table both use.
// 147/147 byte-identical, all sections equal (2026-09-10). Register/layout idioms used here:
//  - EventMgr::construct: `return 1` inside the err arm creates the BARRIER after the err call that
//    loop.c's find_and_verify_loops needs to move the `return i` block of the inlined EvtWorkNo loop
//    behind it (the target's `mr r0,r9; b` after the `bl err; b`).
//  - GetMod: the "pl0300" arm tests its own GetDat result and `goto err`s into the else arm's err
//    body (then arm `bl; cmpwi; beq err; b ok`, no cross-jump of the call).
//  - DelEvt uses fade.h's FadeSetW (P tied to r4); the zero's `li r0,0` after `lis r3` is #13 (the
//    original re-materialises the REG_EQUIV zero at the store; ours allocates r9 and hoists it).
//  - EspToolSetMod: `p = mname` after the strcpy + `c = p[i]` keeps the target's `mr r29,r30` copy
//    (COMPILER-DIFF candidate #3: the original's gcse copies the strcpy argument pseudo right after
//    the call for the loop's `&mname`; ours recomputes it at the block end and coalesces).
//    `BitOn(EvtDebug.pModel[no].flags, ..)` for the `lwzu/stw 0(r9)` RMW pairs.
//  - EspSetModelPtr: `u32 tbl = (u32) EspEvModList; *(cModel**) (tbl + (n << 2)) = m` -- an integer
//    base and a shift index keep both address operands unflagged, so regclass gives the index a BASE
//    register (`stwx r4,r11,r9`); `tbl[n]` on a pointer flags the base (index r0) and `n * 4` makes
//    expand put the MULT first (`add idx,tbl`).
//  - EvtSndStrStop/Play: evtStrNo/evtStrId helpers (u32 base of the member array) for the same reason.
//  - IsExePacket: the middle `return 0` is a `goto ng` to the final `return 0` (jump2 otherwise
//    cross-jumps it into the FIRST copy, COMPILER-DIFF 6 shape).
//  - NameChange: no `dst` local; `nameBuf` used directly, so the return-block use is a gcse PRE copy of
//    the strcpy argument register (`addi r3,r30,132; mr r29,r3`).
// 147/147 byte-identical (2026-09-10).
#include "types.h"
#include "atari.h"
#include "event.h"
#include "light.h"
#include "xml.h"
#include "dbg_button.h"
#include "map_obj.h"
#include "widget.h"
#include "global.h"
#include "main.h"
#include "model.h"
#include "obj.h"
#include "obj18.h"
#include "em.h"
#include "player.h"
#include "pl_sub.h"
#include "mes.h"
#include "cam_ctrl.h"
#include "motion.h"
#include "esp.h"
#include "espgen.h"
#include "est.h"
#include "snd.h"
#include "fade.h"
#include "sce.h"
#include "scheduler.h"
#include "sscrn.h"
#include "shadow.h"
#include "game.h"
#include "file.h"
#include "datactrl.h"
#include "read.h"
#include "dvd.h"
#include "act_btn.h"
#include "hermite.h"
#include "math_sub.h"
#include "eprintf.h"
#include "foot_shadow.h"

extern "C" {
void OSReport(const char* fmt, ...);
void* memset(void* dst, int c, unsigned int n);
char* strcpy(char* dst, const char* src);
char* strcat(char* dst, const char* src);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, unsigned int n);
unsigned int strlen(const char* s);
char* strchr(const char* s, int c);
char* strstr(const char* s, const char* sub);
// game/eff_sys.cpp
// game/read.cpp: SearchEmModule (C++ linkage) comes from read.h
// game/shape.cpp
void ClrShape(cModel* m);
// game/filter01.cpp
void Filter01SetParam_CamZ(int mode, u8 type, f32 level, f32 camz);
// game/foot_shadow_tbl.cpp (incomplete types: full address, not @sda21)
extern u8 pl_fs_tbl[];
extern u8 Em10_fs_tbl[];
extern u8 Em2c_fs_tbl[];
}

// game/emdata.cpp
void EspEmDataSwapPush(int id);
void EspEmDataSwapPop(int id);
// game/shape.cpp
int ShapeSet(void* work, int frame, void* data, int flags);

// The original cUnit::beginEvent takes an int (sscrn.cpp's view of the vtable).
class cUnitEvent {
public:
    u32 be_flag;
    cUnit* next;
    virtual ~cUnitEvent();
    virtual void beginEvent(int mode);
    virtual void endEvent(int mode);
};
#define BEGIN_EVENT(p, mode) ((cUnitEvent*) (p))->beginEvent(mode)

// Deletes every message slot (the &cMes pointer is hoisted into a callee-saved register).
static inline void EvtMesDeleteAll()
{
    MessageControl* mes = &cMes;
    int i;

    for (i = 0; i < 16; i++) {
        mes->Delete(i);
    }
}

// Reference store: keeps a following `pG` load below it (global.h FSet for ints).
static inline void IntSet(int& d, int v)
{
    d = v;
}

// Status / tool flag test: the `li 1; andis.; bne; li 0; cmpwi` chains.
static inline int EvtChk(u32 f, u32 mask)
{
    return (f & mask) ? 1 : 0;
}

// Reversed form (`li 0; andi.; beq; li 1`), compared against 1 by EspToolSetMod.
static inline int BeFlgChk(cUnit* u, u32 mask)
{
    if (u->be_flag & mask) {
        return 1;
    }
    return 0;
}

// The event model list entry when `no` is a valid index.
static inline cModel* EspEvModGet(int no)
{
    if (no >= 0 && no < 0x80) {
        return EspEvModList[no];
    }
    return 0;
}

// Index of `e` in the manager's work array (-1 when it is not one of them).
static inline int EvtWorkNo(EventMgr* mgr, Event* e)
{
    u32 i;
    for (i = 0; i < mgr->nArray; i++) {
        if ((Event*) ((u8*) mgr->pArray + i * mgr->size) == e) {
            return i;
        }
    }
    return -1;
}

// One Hermite curve of the fog / focus data (64 keys).
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

// 12-byte model name copied as words (cObj Obj18Work::evName).
struct EvtName {
    u32 w[3];
};

// Mtx as an assignable aggregate (block copy of the zero parts matrix).
struct EvtMtx {
    Mtx m;
};

// Room "EVS" data: a table of offsets to the room's event files.
struct EvsHeader {
    s32 num;     // 0x00
    u32 tblOfs;  // 0x04  EvsEntry[num]
};

struct EvsEntry {
    u32 ofs;     // 0x00  event file offset
    u32 x4;
};

// Object model type table of ExePacket_SetOm (name prefix, prefix length, SetObj18 type).
struct OmTbl {
    char name[0x10];
    int len;
    int type;
};

typedef int (*PacFunc)(Event*);
typedef void (*EvtFunc)(Event*, int);

template <class T>
void cManager<T>::destroyNow(T* p)
{
    u8 f = flag;

    flag = 0;
    destroy(p);
    flag = f;
}

EventMgr EvtMgr;
EventDebug EvtDebug;

#define EVT_STR_FRAME 26.85312f
#define EVT_FRAME_RATE 29.97f

Event::Event(u8 t) : cUnit(1)
{
    type = t;
}

Event::~Event()
{
    type = 0;
}

int Event::init(char* nm, EvtHeader* data)
{
    u32 i;
    int j;

    if ((int) data >= 0) {
        pLog->err(0, 0, "Event::init : non addr");
        return 0;
    }
    pData = data;
    pPacket = (EvtPacket*) (data->pacOfs + (u32) data);
    if (datTbl.init(0x60) == 0) {
        pLog->err(0, 0, "Event::init : memory failed");
        return 0;
    }
    for (i = 0; i < 0x80; i++) {
        EspEvModList[i] = 0;
    }
    xC = 0;
    endStep = 0;
    endWait = 0;
    xF = 0;
    pPrevPacket = 0;
    pPosOya = 0;
    totalFrame = 0;
    frame = 0;
    cut = 0;
    pOya = 0;
    nEspModel = 0;
    pFog = 0;
    pFocus = 0;
    mesWait = 0;
    strTime = 0;
    nextCut = 0;
    pLit = 0;
    for (i = 0; i < 2; i++) {
        strNo[i] = 0;
    }
    mesTimer = 0;
    for (j = 0; j < 2; j++) {
        strId[j] = 0;
        strNo[j] = -1;
    }
    EvtMgr.GetFunc((void**) &funcTbl, nm);
    strcpy(name, nm);
    if (CalMaxTotalFrame(&maxCut, &maxTotalFrame) != 0 && CalMaxFrame(&maxFrame, cut) != 0) {
        return 1;
    }
    pLog->err(0, 0, "Event::init : data failed");
    return 0;
}

int Event::Run()
{
    int flg;
    int i;
    f32 frm;
    u32 n;
    int wait;

    MesClear();
    if ((pG->flags_54 & 0x400) && (cut != 0 || frame != 0)) {
        pG->flags_54 &= ~0x400;
    }
    while ((flg = IsExePacket()) != 0) {
        ChkCutZero();
        if (ExePacket() == 0) {
            pLog->err(0, 0, "Event::Run : failed");
            return 0;
        }
        CalNextPacket();
    }
    if (EvtChk(status, 0x400)) {
        if (totalFrame == maxTotalFrame - 0x1E) {
            FadeSetW(2, 0x2D, 0, 0);
            IntSet(mesWait, 0xF);
            pG->flags_58 &= ~0x800;
            EvtMesDeleteAll();
        }
    }
    if (EvtChk(status, 0x200)) {
        if (!EvtChk(status, 0x100)) {
            if (totalFrame == maxTotalFrame - 0x1E || totalFrame == maxTotalFrame) {
                SetDiedemoExec();
                IntSet(mesWait, 0xF);
                pG->flags_58 &= ~0x800;
                EvtMesDeleteAll();
            }
        }
    }
    if (!EvtChk(EvtDebug.flags, 0x10000000)) {
        FogMove(this, pFog);
    }
    if (!EvtChk(EvtDebug.flags, 0x08000000)) {
        FocusMove(this, pFocus);
    }
    wait = EvtDebug.strWait;
    if (wait > 0) {
        wait = --EvtDebug.strWait;
        // COMPILER-DIFF: candidate (the mes `mr.` family). The original keeps `mr r9,r0; cmpwi r9,0`
        // (the decrement temp and `wait` in different registers, the copy not fused into the
        // compare); a volatile ASM_OPERANDS between the copy and the compare is what stops our
        // combine (an operand-less `asm volatile("")` is an ASM_INPUT and does not), and the dead
        // `wait == 1` test below gives `wait` a mention beyond the block so cse keeps it canonical.
        asm volatile("" : : "r"(wait));
        if (wait > 0) {
            goto func;
        }
    }
    if (wait == 1) {
        n = 0;
    }
    if (EvtChk(status, 0x10000)) {
        frm = (f32) totalFrame;
        n = (u32) (frm / EVT_STR_FRAME);
        if (frm - (f32) n * EVT_STR_FRAME < 1.0f) {
            EventMgr* m = &EvtMgr;
            status &= ~0x10000;
            m->EvtSndStrPlay(&m->x34, 1, EvtDebug.strNo[1], 1, frm / EVT_FRAME_RATE);
        }
    }
func:
    ExeFunc(1, 0);
    ControlTransFlag();
    ExecActBtn();
    CalNextFrame();
    return 1;
}

void Event::EspSetModelPtr(cModel* m)
{
    u32 tbl = (u32) EspEvModList;
    int n = nEspModel;

    if (n >= 0 && n < 0x80) {
        *(cModel**) (tbl + (n << 2)) = m;
    }
    nEspModel++;
}

int Event::EspToolSetDat()
{
    char nm[0x20];
    EvtPacket* pac;
    int no;
    char* p;

    EvtDebug.toolCut = cut;
    RunTool(3, 0);
    EvtDebug.nModel = 0;
    EvtDebug.ClrModelFiles();
    while (IsExePacket()) {
        pac = pPacket;
        if (pac->id > 0x20) {
            pLog->err(0, 0, "Event::ExePacket : id over");
            return 0;
        }
        switch (pac->id) {
        case 6:
            strcpy(EvtDebug.getEvName(), pac->mod.name);
            break;
        case 0xE:
            strcpy(EvtDebug.getCamName(), pac->mod.name);
            break;
        case 0xB:
            no = EvtDebug.nModel;
            strcpy(EvtDebug.pModel[no].name, pac->mod.bin);
            EspToolSetMod(no, pac->mod.name);
            EvtDebug.nModel++;
            break;
        }
        CalNextPacket();
    }
    p = nm;
    strcpy(p, (char*) pData);
    strcmp(p, "event/evd/r120s00.evd");
    return 1;
}

void Event::EspToolSetMod(int no, char* nm)
{
    char path[0x100];
    char bin[0x100];
    char tpl[0x100];
    char mname[0x10];
    XmlSimple xml;
    char* pos;
    int modNo;
    cModel* mod;
    char* buf;
    u32 i;
    int size;
    u8 c;
    char* p;

    buf = (char*) Debug_alloc(1000000, 1);
    EvtDebug.pModel[no].pScr = 0;
    strcpy(mname, nm);
    for (i = 2; i < strlen(mname); i++) {
        c = mname[i];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
            mname[i + 2] = '0';
            mname[i + 3] = '0';
            break;
        }
    }
    if (pG->costume == 1 && strcmp(mname, "pl0000") == 0) {
        strcpy(mname, "pl0800");
    }
    if (pG->costume == 2 && strcmp(mname, "pl0000") == 0) {
        strcpy(mname, "pl0a00");
    }
    sprintf(path, "%s/evt_bin_%s.xml", "x:/soft/room/event/evd", mname);
    size = HDRead(path, buf);
    if (size != 0) {
        buf[size] = 0;
        pos = buf;
        xml.GetXmlStart(&pos, buf, "NameBin");
        do {
            if (xml.GetXmlElem(bin, pos, "NameBin") == 1) {
                if (xml.GetXmlElem(tpl, pos, "NameTpl") == 1) {
                    EvtDebug.AddNameBinTpl(no, bin, tpl);
                }
            }
        } while (xml.GetXmlNext(&pos, pos, "NameBin") != 0);
    }
    strcpy(mname, nm);
    for (u32 j = 2; j < strlen(mname); j++) {
        c = mname[j];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
            mname[j + 3] = '0';
            break;
        }
    }
    if (GetModelPtrNo(&modNo, &mod, mname)) {
        EvtDebug.pModel[no].pModel = (cModel*) modNo;
        EvtDebug.pModel[no].x638 = mod->x12F;
        EvtDebug.pModel[no].x639 = mod->lightInfo.x50;
        if (mod->x12C == 1) {
            BitOn(EvtDebug.pModel[no].flags, 0x80000000);
        }
        if (BeFlgChk(mod, 0x1000) == 1) {
            BitOn(EvtDebug.pModel[no].flags, 0x40000000);
        }
        if (strncmp(mname, "scr", 3) == 0) {
            EvtDebug.pModel[no].pScr = mod;
        }
    }
    pLog->mes(0, 0, "t_event->t_esp:%s", nm);
    Debug_free(buf);
}

// Never called (only its string survives in .rodata).
static inline int EspToolSetDatOya(Event* evt, char* nm)
{
    pLog->err(0, 0, "Event::EspToolSetDatOya : no oya[%s]", nm);
    return 0;
}

int Event::GetModelPtrNo(int* no, cModel** mod, char* nm)
{
    u8 type;
    cModel* m;
    int i;

    if (no == 0 || mod == 0) {
        return 0;
    }
    *no = -1;
    *mod = 0;
    if (GetMod((void**) &m, nm, &type, 0) == 0) {
        pLog->err(0, 0, "Event::GetModelPtrNo : non name[%s]", nm);
        return 0;
    }
    for (i = 0; i < nEspModel; i++) {
        if (m == EspEvModGet(i)) {
            *mod = m;
            *no = i;
            return 1;
        }
    }
    pLog->err(0, 0, "Event::GetModelPtrNo : non no[%s]", nm);
    return 0;
}

int Event::RunTool(int mode, int arg)
{
    int frm = frame;
    int c = cut;

    switch (mode) {
    case 0:
        frm -= arg;
        if (frm < 0) {
            c--;
            if (c < 0) {
                c = 0;
                frm = 0;
            } else {
                if (CalMaxFrame(&frm, c) == 0) {
                    pLog->err(0, 0, "Event::RunTool : failed");
                    return 0;
                }
                frm -= 2;
            }
        }
        break;
    case 1:
        if (frm <= 1) {
            c--;
        }
        frm = 0;
        if (c < 0) {
            c = 0;
        }
        break;
    case 2:
        c++;
        frm = 0;
        if (c >= maxCut) {
            c = maxCut - 1;
        }
        break;
    case 3:
        frm = 0;
        if (c < 0) {
            c = 0;
        }
        break;
    }
    pPacket = (EvtPacket*) (pData->pacOfs + (u32) pData);
    toolCut = c;
    totalFrame = 0;
    frame = 0;
    cut = 0;
    toolFrame = frm;
    toolFrame2 = frm;
    FadeKillAll();
    if (CalMaxFrame(&maxFrame, 0) == 0) {
        pLog->err(0, 0, "Event::init : data failed");
        return 0;
    }
    status |= 0x40000000;
    while (frm > frame || c > cut) {
        if (Run() == 0) {
            pLog->err(0, 0, "Event::ToolRun : failed");
            return 0;
        }
    }
    toolFrame = 0;
    status &= ~0x80000000;
    if (CalMaxFrame(&maxFrame, cut) == 0) {
        pLog->err(0, 0, "Event::init : data failed");
        return 0;
    }
    return 1;
}

int Event::RunEvtCancel()
{
    u32* key;

    if (EvtChk(status, 0x10000000)) {
        if (cancelCut <= cut) {
            return 1;
        }
    }
    BitOn(status, 0x04000000);
    pG->flags_5018 |= 0x01000000;
    EvtMesDeleteAll();
    FadeSetW(1, 1, 0, 0);
    TaskSleep(2);
    status |= 0x08000000;
    while (!EvtChk(status, 0x00800000)) {
        if (EvtChk(status, 0x10000000)) {
            if (cancelCut <= cut) {
                goto cancel_end;
            }
        }
        if (Run() == 0) {
            pLog->err(0, 0, "Event::RunEvtCancel : failed");
            return 0;
        }
        if (cut >= maxCut - 1) {
            if (!(pG->flags_170 & 0x10000000) && (pPL->be_flag & 0x20)
                && (!(pG->flags_5010 & 0x10000000) || (pPL->be_flag & 0x800))) {
                pPL->move();
            }
            if (pOya != 0) {
                pOya->move();
            }
        }
    }
cancel_end:
    status &= ~0x08000000;
    EvtMesDeleteAll();
    key = (u32*) name;
    IntSet(mesTimer, 0);
    pG->flags_58 &= ~0x800;
    EvtMgr.EvtSndStrStop(key, 1, 1);
    ExeFunc(3, 0);
    if (EvtChk(status, 0x10000000)) {
        FadeKill(2);
        FadeSetW(0x80000001, 0xA, 0, 0);
    }
    return 1;
}

void Event::CancelSet()
{
    status |= 0x4000;
    status &= ~0x04000000;
    status &= ~0x10000000;
}

void Event::CancelNoSet()
{
    status |= 0x02000000;
}

void Event::ControlTransFlag()
{
    int n;
    int i;
    cModel* m;
    u8 type;
    cModel* oya;
    int state;
    Obj18Work* w;

    n = datTbl.GetNumDat();
    if (mesWait != 0) {
        return;
    }
    for (i = 0; i < n; i++) {
        if (datTbl.GetDatWkNo((void**) &m, &type, i) == 0) {
            continue;
        }
        if (pG->costume2 == 1) {
            if (datTbl.ChkDatWkNoName(i, "evmd100") == 1 || datTbl.ChkDatWkNoName(i, "evm8200") == 1
                || datTbl.ChkDatWkNoName(i, "evm7100") == 1) {
                m->be_flag &= ~0x20;
                m->be_flag &= ~2;
                continue;
            }
        }
        switch (type) {
        case 0:
        case 1:
        case 2:
            if (m == 0) {
                break;
            }
            state = MotionGetState(m);
            if (Obj18CmfGet((cObj*) m) & 0x04000000) {
                break;
            }
            if (m->x12E == 2) {
                return;
            }
            if (EvtChk(status, 0x00100000) || EvtChk(status, 0x00400000)) {
                if (cut >= maxCut) {
                    break;
                }
                if (cut == maxCut - 1 && frame > 1) {
                    break;
                }
            }
            if (state == -1 || (state & 4)) {
                m->be_flag &= ~0x20;
                m->be_flag &= ~2;
            } else {
                m->be_flag |= 0x20;
                m->be_flag |= 2;
            }
            if (m->x12E == 1 && m->id == 0x18) {
                w = &((cObj*) m)->o18;
                if (w->type == 3 && w->child != 0 && !(((cObj*) m)->o18.x74 & 0x04000000)) {
                    if ((m->be_flag & 0x20) == 0) {
                        w->child->be_flag &= ~0x20;
                    } else {
                        w->child->be_flag |= 0x20;
                    }
                    if (m->isTrans() == 0) {
                        w->child->be_flag &= ~2;
                    } else {
                        w->child->be_flag |= 2;
                    }
                }
                if (obj18GetOya(&oya, (cObj*) m) == 1) {
                    if ((oya->be_flag & 0x20) == 0) {
                        m->be_flag &= ~0x20;
                    } else {
                        m->be_flag |= 0x20;
                    }
                    if (oya->isTrans() == 0) {
                        m->be_flag &= ~2;
                    } else {
                        m->be_flag |= 2;
                    }
                }
            }
            break;
        }
    }
}

void Event::DebugDisp()
{
    char buf[0x20];
    int col = 0;

    sprintf(buf, "%s%s", pData->room, pData->no);
    if (EvtMgr.NameCheck(buf) == 1) {
        col = 5;
    }
    eprintf(0x10, 0x20, col, 0, "[EVENT EXEC] EV:%s%s CUT:%02d/%02d FRM:%03d/%03d ALL:%04d/%04d", pData->room, pData->no, cut, maxCut,
            frame, maxFrame, totalFrame, maxTotalFrame);
    dbgCut = cut;
    dbgMaxCut = maxCut;
    dbgFrame = frame;
    dbgMaxFrame = maxFrame;
    dbgTotalFrame = totalFrame;
    dbgMaxTotalFrame = maxTotalFrame;
}

void Event::DebugDispTool()
{
    char buf[0x20];
    int col = 0;

    sprintf(buf, "%s%s", pData->room, pData->no);
    if (EvtMgr.NameCheck(buf) == 1) {
        col = 5;
    }
    eprintf(0x10, 0x10, col, 0, "[EVENT TOOL] EV:%s%s CUT:%02d/%02d FRM:%03d/%03d ALL:%04d/%04d", pData->room, pData->no, dbgCut,
            dbgMaxCut, dbgFrame, dbgMaxFrame, dbgTotalFrame, dbgMaxTotalFrame);
}

int Event::IsExePacket()
{
    EvtPacket* pac;

    if (EvtChk(status, 0x20000000)) {
        if (cut >= maxCut) {
            return 0;
        }
    }
    if (EvtChk(status, 0x00800000)) {
        goto ng;
    }
    pac = pPacket;
    if ((pac->cut == cut && pac->frame <= frame) || pac->cut < cut) {
        return 1;
    }
ng:
    return 0;
}

int Event::ExePacket()
{
    static PacFunc packetTbl[] = {
        &Event::ExePacket_BeginEvt,
        &Event::ExePacket_SetPl,
        &Event::ExePacket_SetEm,
        &Event::ExePacket_SetOm,
        &Event::ExePacket_SetParts,
        &Event::ExePacket_SetList,
        &Event::ExePacket_Cam,
        &Event::ExePacket_CamPos,
        &Event::ExePacket_CamDammy,
        &Event::ExePacket_Pos,
        &Event::ExePacket_PosPl,
        &Event::ExePacket_Mot,
        &Event::ExePacket_Shp,
        &Event::ExePacket_Esp,
        &Event::ExePacket_Lit,
        &Event::ExePacket_Str,
        &Event::ExePacket_Se,
        &Event::ExePacket_Mes,
        &Event::ExePacket_Func,
        &Event::ExePacket_ParentOn,
        &Event::ExePacket_ParentOff,
        &Event::ExePacket_EndPl,
        &Event::ExePacket_EndEm,
        &Event::ExePacket_EndOm,
        &Event::ExePacket_EndParts,
        &Event::ExePacket_EndList,
        &Event::ExePacket_EndEvt,
        &Event::ExePacket_EndPac,
        &Event::ExePacket_SetEff,
        &Event::ExePacket_Fade,
        &Event::ExePacket_Fog,
        &Event::ExePacket_Focus,
        &Event::ExePacket_SetMdt,
    };
    int id = pPacket->id;

    if (id > 0x20) {
        pLog->err(0, 0, "Event::ExePacket : id over");
        return 0;
    }
    if (!EvtChk(status, 0x08000000)) {
        if (EvtChk(status, 0x40000000)) {
            switch (id) {
            case 6 ... 0xC:
            case 0xE:
            case 0x11 ... 0x14:
            case 0x1D ... 0x1F:
                break;
            default:
                return 1;
            }
        } else if (EvtChk(status, 0x20000000)) {
            switch (id) {
            case 6 ... 0x14:
            case 0x1D ... 0x1F:
                break;
            default:
                return 1;
            }
        }
    }
    if (packetTbl[pPacket->id](this) == 0) {
        pLog->err(0, 0, "Event::ExePacket : exec error");
        return 0;
    }
    return 1;
}

int Event::ExePacket_BeginEvt(Event* evt)
{
    return 1;
}

int Event::ExePacket_SetPl(Event* evt)
{
    EvtPacket* pac = evt->pPacket;

    BEGIN_EVENT(pPL, 0);
    pPL->setNoSuspend(1);
    if (evt->SetMod(pac->mod.name, pPL, 0, 0, 2, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetPl : failed");
        return 1;
    }
    evt->EspSetModelPtr(pPL);
    return 1;
}

int Event::ExePacket_SetEm(Event* evt)
{
    return 1;
}

int Event::ExePacket_SetOm(Event* evt)
{
    EvtPacket* pac = evt->pPacket;
    Vec pos = {0.0f, 0.0f, 0.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    void* bin;
    void* tpl;
    int type;
    cObj* obj;
    int i;

    if (EvtMgr.GetBin(&bin, pac->mod.bin, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetOm : dat failed");
        return 1;
    }
    if (EvtMgr.GetBin(&tpl, pac->mod.tpl, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetOm : dat failed");
        return 1;
    }
    {
        OmTbl tbl[] = {
            {"pl00", 4, 1},     {"pl01", 4, 2},      {"evm90", 5, 2},    {"evm72", 5, 0x10},  {"pl02", 4, 3},
            {"pl04", 4, 4},     {"pl0c", 4, 3},      {"pl03", 4, 3},     {"em10", 4, 6},      {"em11", 4, 6},
            {"em12", 4, 6},     {"em13", 4, 6},      {"em14", 4, 6},     {"em15", 4, 6},      {"em16", 4, 6},
            {"em17", 4, 6},     {"em18", 4, 7},      {"evm54", 5, 7},    {"em19", 4, 6},      {"em1a", 4, 6},
            {"em1b", 4, 6},     {"em1c", 4, 6},      {"em1d", 4, 6},     {"em1e", 4, 6},      {"em1h", 4, 6},
            {"evm50", 5, 6},    {"em1g", 4, 6},      {"em1f", 4, 6},     {"em2b", 4, 0xB},    {"em34", 4, 8},
            {"evm35", 5, 0x12}, {"em37", 4, 9},      {"em30", 4, 0xA},   {"em3300a", 7, 0x14}, {"em3300", 6, 0x13},
            {"evm51", 5, 0x15}, {"evm52", 5, 0x16},  {"evm53", 5, 0x15}, {"evm82", 5, 0x17},  {"em39", 4, 0x18},
            {"pl", 2, 5},       {"em", 2, 0xC},      {"evm", 3, 0},      {"ev", 2, 0xD},      {"obm", 3, 0},
            {"et", 2, 0xE},     {"scr", 3, 0xF},     {"wep", 3, 0x10},   {"eff", 3, 0x11},
        };
        int num = sizeof(tbl) / sizeof(OmTbl);
        type = 0;
        for (i = 0; i < num; i++) {
            if (strncmp(pac->mod.name, tbl[i].name, tbl[i].len) == 0) {
                type = tbl[i].type;
                break;
            }
        }
    }
    obj = SetObj18(bin, tpl, &pos, &rot, type);
    if (obj == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetOm : om set failed");
        return 1;
    }
    *(EvtName*) obj->o18.evName = *(EvtName*) pac->mod.name;
    obj->sub2B4.atari.throughOn();
    switch (type) {
    case 1 ... 4:
    case 7 ... 0xB:
    case 0x10:
    case 0x12 ... 0x16:
    case 0x18:
        obj->be_flag |= 0x10;
        obj->be_flag |= 0x05000000;
        break;
    }
    switch (type) {
    case 1 ... 4:
    case 0x18:
        obj->be_flag |= 0x10;
        obj->sub2B4.pFootShadowTbl = pl_fs_tbl;
        break;
    case 6:
        obj->be_flag |= 0x10;
        obj->sub2B4.pFootShadowTbl = Em10_fs_tbl;
        break;
    case 0x13 ... 0x16:
        obj->be_flag |= 0x10;
        obj->sub2B4.pFootShadowTbl = Em2c_fs_tbl;
        break;
    }
    obj->be_flag |= 0x02001000;
    obj->setNoSuspend(1);
    obj->be_flag &= ~2;
    Obj18CmfSet(obj, pac->flag);
    if (strcmp(pac->mod.name, "pl0000") == 0) {
        evt->pOya = obj;
    }
    if (evt->SetMod(pac->mod.name, obj, 2, 0, 2, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetOm : failed");
        return 1;
    }
    evt->EspSetModelPtr(obj);
    return 1;
}

int Event::ExePacket_SetParts(Event* evt)
{
    EvtPacket* pac = evt->pPacket;

    switch (pG->costume2) {
    case 0:
    default:
        switch (pG->costume) {
        case 1:
        case 2:
            if (strcmp(pac->parts.name, "ev000e") == 0 || strcmp(pac->parts.name, "ev001e") == 0) {
                return evt->ExePacket_SetPartsSub(pac->parts.name, "em/pl00/pl000e.bin", "em/pl00/pl000a.tpl", pac->parts.oya);
            }
            break;
        }
        return evt->ExePacket_SetPartsSub(pac->parts.name, pac->parts.bin, pac->parts.tpl, pac->parts.oya);
    case 1:
        if (strcmp(pac->parts.name, "ev000e") == 0) {
            return evt->ExePacket_SetPartsSub(pac->parts.name, "em/pl00/pl000e.bin", "em/pl00/pl000a.tpl", pac->parts.oya);
        }
        if (strcmp(pac->parts.name, "ev0104") == 0 || strcmp(pac->parts.name, "ev0105") == 0) {
            return evt->ExePacket_SetPartsSub(pac->parts.name, pac->parts.bin, "event/model/ev0100/ev0100.tpl", pac->parts.oya);
        }
        return evt->ExePacket_SetPartsSub(pac->parts.name, pac->parts.bin, pac->parts.tpl, pac->parts.oya);
    }
}

int Event::ExePacket_SetPartsSub(char* nm, char* bin, char* tpl, char* oya)
{
    void* b;
    void* t;
    cModel* m;
    cModelInfo* info;

    if (EvtMgr.GetBin(&b, bin, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetParts : dat failed");
        return 1;
    }
    if (EvtMgr.GetBin(&t, tpl, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetParts : dat failed");
        return 1;
    }
    if (GetMod((void**) &m, oya, 0, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetParts : oya non");
        return 1;
    }
    if (strcmp(nm, "ev0002") == 0 || strcmp(nm, "ev0102") == 0 || strcmp(nm, "ev0202") == 0 || strcmp(nm, "ev3002") == 0
        || strcmp(nm, "ev0402") == 0) {
        m->setPartsOffset(b);
    }
    info = ModInfoMgr.create(b, t);
    if (info == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetParts : Parts set failed");
        return 1;
    }
    m->addModel(info);
    if (SetMod(nm, info, 3, 0, 2, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetParts : failed");
        return 1;
    }
    return 1;
}

int Event::ExePacket_SetList(Event* evt)
{
    return 1;
}

int Event::ExePacket_SetEff(Event* evt)
{
    void* dat;
    EvtPacket* pac = evt->pPacket;

    if (evt->effNo == -1 || evt->effNo > 1) {
        pLog->err(0, 0, "Event::ExePacket_SetEff : NoWork failed");
        return 1;
    }
    if (EvtMgr.GetBin(&dat, pac->mod.name, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetEff : dat failed");
        return 1;
    }
    if (EspDataLoad((u32) dat, evt->effNo + 0xC4, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetEff : failed");
        return 1;
    }
    evt->status |= 0x00040000;
    return 1;
}

int Event::ExePacket_SetMdt(Event* evt)
{
    void* dat;
    EvtPacket* pac = evt->pPacket;

    if (EvtMgr.GetBin(&dat, pac->mod.name, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_SetMdt : dat failed");
        return 1;
    }
    MesData.ptr[1] = (u8*) dat;
    evt->status |= 0x2000;
    return 1;
}

int Event::ExePacket_Cam(Event* evt)
{
    void* dat;
    EvtPacket* pac = evt->pPacket;
    int frm = 0;
    void* zero;

    if (EvtMgr.GetBin(&dat, pac->mod.name, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Cam : dat failed");
        return 1;
    }
    zero = 0;
    if (EvtChk(evt->status, 0x40000000)) {
        frm = evt->toolFrame;
    }
    if (EvtChk(evt->status, 0x08000000)) {
        frm = evt->maxFrame - 1;
    }
    CamCtrl.MotionSet(dat, 0, (f32) frm);
    pPL->be_flag |= 0x00200000;
    evt->pFog = (EvtFogData*) zero;
    evt->pFocus = (EvtFocusData*) zero;
    evt->MotClear();
    if (!EvtChk(evt->status, 0x08000000)) {
        EventCutEffDelete();
        if (EvtChk(evt->status, 0x40000000) == 0 || (EvtChk(evt->status, 0x40000000) && pac->cut == evt->toolCut)) {
            EventCutEstSet(evt->effNo + 0xC4, evt->cut);
        }
    }
    return 1;
}

int Event::ExePacket_CamPos(Event* evt)
{
    return 1;
}

int Event::ExePacket_CamDammy(Event* evt)
{
    return 1;
}

int Event::ExePacket_Pos(Event* evt)
{
    Vec pos;
    Vec rot;
    cModel* m;
    cModel* oya;
    EvtPacket* pac = evt->pPacket;
    char* nm = pac->pos.name;

    if (strcmp(nm, "cam0000") != 0) {
        if (evt->GetMod((void**) &m, nm, 0, 0) == 0) {
            pLog->err(0, 0, "Event::ExePacket_Pos : mod failed");
            return 1;
        }
    }
    pos.x = (f32) pac->pos.pos[0];
    pos.y = (f32) pac->pos.pos[1];
    pos.z = (f32) pac->pos.pos[2];
    rot.x = (f32) pac->pos.rot[0] * 3.1415927f / 180.0f;
    rot.y = (f32) pac->pos.rot[1] * 3.1415927f / 180.0f;
    rot.z = (f32) pac->pos.rot[2] * 3.1415927f / 180.0f;
    if (strcmp(pac->pos.oya, "") != 0) {
        if (strcmp(pac->pos.oya, "oya0000") == 0) {
            if (evt->pPosOya == 0) {
                pLog->err(0, 0, "Event::ExePacket_Pos : oya failed");
                return 1;
            }
            oya = evt->pPosOya;
        } else if (evt->GetMod((void**) &oya, pac->pos.oya, 0, 0) == 0) {
            pLog->err(0, 0, "Event::ExePacket_Pos : oya failed");
            return 1;
        }
    }
    if ((s32) pac->flag < 0) {
        PSMTXMultVec(oya->mat, &pos, &pos);
        rot.x += oya->rot.x;
        rot.y += oya->rot.y;
        rot.z += oya->rot.z;
    }
    if (pac->flag & 0x40000000) {
        if (m->x12E == 1 && m->id == 0x18) {
            OyaSetObj18((cObj*) m, oya, pac->pos.partsNo);
            m->lightInfo.x51 = 1;
        }
    }
    if (strcmp(pac->pos.name, "cam0000") == 0) {
        RotMatrix(evt->camMat, &rot);
        TransMatrix(evt->camMat, &pos);
        CamCtrl.setMotionBaseMatPtr(&evt->camMat);
    } else {
        m->setPos(&pos);
        m->setAng(&rot);
    }
    return 1;
}

int Event::ExePacket_PosPl(Event* evt)
{
    return 1;
}

int Event::ExePacket_Mot(Event* evt)
{
    cModel* m;
    void* dat;
    EvtPacket* pac = evt->pPacket;
    int frm = 0;
    u32 t;

    if (EvtChk(evt->status, 0x40000000)) {
        frm = evt->toolFrame;
    }
    if (EvtChk(evt->status, 0x08000000)) {
        frm = evt->maxFrame - 1;
    }
    if (evt->GetMod((void**) &m, pac->mod.name, 0, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Mot : mod failed");
        return 1;
    }
    if (EvtMgr.GetBin(&dat, pac->mod.bin, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Mot : dat failed");
        return 1;
    }
    if (*(u16*) dat == 0) {
        return 1;
    }
    MotionClear(m, 1);
    MotionSetCore(m, &((cEm*) m)->pMotion, dat, 0, 0, 1, (u16) frm);
    if (m->x12E == 0 && m->id == 0) {
        m->be_flag |= 0x00200000;
    }
    ClrShape(m);
    if (m->x12E == 1 && m->id == 0x18) {
        Obj18Work* w = &((cObj*) m)->o18;
        t = w->type;
        if ((t >= 1 && t <= 4) || t == 7 || t == 8 || t == 9 || t == 0xA || t == 0x13 || t == 0x14 || t == 0x15 || t == 0x16
            || t == 0xB) {
            m->be_flag |= 0x00200000;
        }
        if (w->type == 3 && w->child != 0) {
            w->child->be_flag |= 0x00200000;
        }
    }
    return 1;
}

int Event::ExePacket_Shp(Event* evt)
{
    cModel* m;
    u8 type;
    void* dat;
    EvtPacket* pac = evt->pPacket;
    int frm = 0;
    void* w;

    if (EvtChk(evt->status, 0x40000000)) {
        frm = evt->toolFrame;
    }
    if (EvtChk(evt->status, 0x08000000)) {
        frm = evt->maxFrame - 1;
    }
    if (evt->GetMod((void**) &m, pac->mod.name, &type, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Shp : mod failed");
        return 1;
    }
    if (type == 2) {
        w = m->pInfo;
    } else {
        w = m;
    }
    if (EvtMgr.GetBin(&dat, pac->mod.bin, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Shp : dat failed");
        return 1;
    }
    ShapeSet(w, (s16) frm, dat, 2);
    return 1;
}

int Event::ExePacket_Esp(Event* evt)
{
    Vec pos;
    Vec rot;
    cModel* m;
    EvtPacket* pac = evt->pPacket;
    char* nm = pac->esp.name;
    int ret;
    int e;

    ret = strcmp(nm, "");
    if (ret == 0) {
        m = 0;
    } else if (evt->GetMod((void**) &m, nm, 0, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Esp : mod failed");
        return 1;
    }
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    rot.x = 0.0f;
    rot.y = 0.0f;
    rot.z = 0.0f;
    if ((s32) pac->flag < 0) {
        if (evt->pPosOya == 0) {
            pLog->err(0, 0, "Event::ExePacket_Esp : oya failed");
            return 1;
        }
        PSMTXMultVec(evt->pPosOya->mat, &pos, &pos);
        rot.x += evt->pPosOya->rot.x;
        rot.y += evt->pPosOya->rot.y;
        rot.z += evt->pPosOya->rot.z;
    }
    if (pac->esp.type == 0) {
        EstSet((int) m, -1, &pos, &rot, 1, pac->esp.parts, 1, 0, 0, 0);
    }
    if (pac->esp.type == 5) {
        e = evt->effNo;
        if (e == -1 || e > 1) {
            pLog->err(0, 0, "Event::ExePacket_SetEff : NoWork failed");
            return 1;
        }
        EstSet((int) m, -1, &pos, &rot, e + 0xC4, pac->esp.parts, 1, (u8) (e + 0x37), 0, 0);
    }
    if (pac->esp.type == 6) {
        EstSet((int) m, -1, &pos, &rot, 0x54, pac->esp.parts, 1, 0, 0, 0);
    }
    return 1;
}

int Event::ExePacket_Lit(Event* evt)
{
    cLit* dat;
    EvtPacket* pac = evt->pPacket;

    if (EvtChk(EvtDebug.flags, 0x20000000)) {
        return 1;
    }
    if (EvtMgr.GetBin((void**) &dat, pac->mod.name, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Lit : dat failed");
        return 1;
    }
    if (EvtChk(evt->status, 0x40000000)) {
        if (evt->toolCut != evt->cut || evt->pLit == dat) {
            return 1;
        }
    }
    evt->pLit = dat;
    LightMgr.roomLitSet(dat);
    LightMgr.update(0, -1);
    return 1;
}

int Event::ExePacket_Fog(Event* evt)
{
    void* dat;
    EvtPacket* pac = evt->pPacket;

    if (EvtChk(EvtDebug.flags, 0x10000000)) {
        return 1;
    }
    if (EvtMgr.GetBin(&dat, pac->mod.name, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Fog : dat failed");
        return 1;
    }
    evt->pFog = dat;
    return 1;
}

int Event::ExePacket_Focus(Event* evt)
{
    void* dat;
    EvtPacket* pac = evt->pPacket;

    if (EvtChk(EvtDebug.flags, 0x08000000)) {
        return 1;
    }
    if (EvtMgr.GetBin(&dat, pac->mod.name, 0) == 0) {
        pLog->err(0, 0, "Event::ExePacket_Focus : dat failed");
        return 1;
    }
    evt->pFocus = dat;
    return 1;
}

// Never called (only its string survives in .rodata).
static inline const char* EvtDebugEvdName()
{
    return "event/evd/r100s40.evd";
}

int Event::ExePacket_Str(Event* evt)
{
    char nm[0x40];
    EvtPacket* pac = evt->pPacket;
    char* key = nm;
    int no;
    int blk;

    strcpy(key, evt->name);
    blk = pac->val.no;
    no = pac->val.arg;
    if (evt->strTime != 0) {
        no = evt->strTime;
        evt->strTime = 0;
    }
    if (blk == 0) {
        EvtMgr.EvtSndStrPlay((u32*) key, 0, no, 0, 0.0f);
    } else if (!(pac->flag & 0x20000000)) {
        EvtMgr.EvtSndStrPlay((u32*) key, blk, no, 1, 0.0f);
    } else {
        EvtMgr.EvtSndStrPlay((u32*) key, blk, no, 0, 0.0f);
    }
    return 1;
}

int Event::ExePacket_Se(Event* evt)
{
    EvtPacket* pac = evt->pPacket;

    SndCall((u16) pac->val.no, (u16) pac->val.arg, &pPL->pos, 0, 0, 0);
    return 1;
}

int Event::ExePacket_Fade(Event* evt)
{
    EvtPacket* pac = evt->pPacket;

    if (pac->val.no == 0) {
        FadeSetW(0x80000002, pac->val.time, 0, 0);
    } else {
        FadeSetW(2, pac->val.time, 0, 0);
    }
    return 1;
}

int Event::ExePacket_Mes(Event* evt)
{
    EvtPacket* pac;

    if (EvtChk(EvtDebug.flags, 0x04000000)) {
        return 1;
    }
    if (pG->flags_68 & 0x400) {
        return 1;
    }
    pac = evt->pPacket;
    evt->MesSet(pac->val.no, pac->val.arg, 0x64, 0x150 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1);
    return 1;
}

int Event::ExePacket_Func(Event* evt)
{
    u32 tbl = evt->funcTbl;
    EvtPacket* pac = evt->pPacket;
    EvtFunc fn;

    if (tbl == 0) {
        pLog->err(0, 0, "Event::ExePacket_Func: func failed");
        return 1;
    }
    fn = *(EvtFunc*) (pac->val.no * 4 + tbl);
    fn(evt, pac->val.arg);
    return 1;
}

int Event::ExePacket_ParentOn(Event* evt)
{
    return 1;
}

int Event::ExePacket_ParentOff(Event* evt)
{
    return 1;
}

int Event::ExePacket_EndPl(Event* evt)
{
    return 1;
}

int Event::ExePacket_EndEm(Event* evt)
{
    return 1;
}

int Event::ExePacket_EndOm(Event* evt)
{
    return 1;
}

int Event::ExePacket_EndParts(Event* evt)
{
    return 1;
}

int Event::ExePacket_EndList(Event* evt)
{
    return 1;
}

int Event::ExePacket_EndEvt(Event* evt)
{
    return 1;
}

int Event::ExePacket_EndPac(Event* evt)
{
    return 1;
}

void Event::ExeBeginEvt(Event* evt, int mode)
{
    int i;

    if (EvtChk(evt->status, 0x80)) {
        pLog->mes(0, 0, "Event::ExeBeginEvt : SceEventStart(true)");
        SceEventStart(1);
    } else {
        SceEventStart(0);
    }
    BitOn(pG->flags_5014, 0x00080000);
    BitOn(pG->flags_5014, 0x00010000);
    BitOff(pG->flags_5018, 0x01000000);
    cMes.loadEventFont();
    ExeFunc(0, 0);
    if (pG->x4FB8 == 0) {
        EvtMgr.SetBin("em/pl00/pl000a.bin", PL_ARC_PTR(pG->pPlArc, 4), 0, 2);
        EvtMgr.SetBin("em/pl00/pl000a.tpl", PL_ARC_PTR(pG->pPlArc, 5), 0, 2);
        EvtMgr.SetBin("em/pl00/pl000d.bin", PL_ARC_PTR(pG->pPlArc, 9), 0, 2);
        EvtMgr.SetBin("em/pl00/pl000b.tpl", PL_ARC_PTR(pG->pPlArc, 7), 0, 2);
        EvtMgr.SetBin("em/pl00/pl000e.bin", PL_ARC_PTR(pG->pPlArc, 0xA), 0, 2);
        EvtMgr.SetBin("em/pl00/pl000l.bin", PL_ARC_PTR(pG->pPlArc, 0x10), 0, 2);
        EvtMgr.SetBin("etc/core/dummy.bin", (void*) (pG->pArc->ofs_20 + (u32) pG->pArc), 0, 2);
        EvtMgr.SetBin("etc/core/dummy.tpl", (void*) (pG->pArc->ofs_24 + (u32) pG->pArc), 0, 2);
    }
    EvtMesDeleteAll();
    pG->flags_54 |= 0x400;
    if (!EvtChk(evt->pData->sndFlag, 0x80000000)) {
        SndEventInit();
    }
}

void Event::ExeEndEvt(Event* evt, u32 mode)
{
    Vec pos;
    Vec rot;
    u8 type;
    cModel* m;
    int n;
    int i;
    cPlayer* pl;

    if (!EvtChk(evt->status, 0x800)) {
        pos = pPL->pos;
        rot = pPL->rot;
        if (evt->pOya != 0) {
            EvtMgr.GetZeroPartsWorldPos(evt->pOya, &pos, &rot);
            evt->pOya = 0;
        }
        pPL->zeroPartsPosInit(&pos, &rot);
    }
    if (!EvtChk(evt->status, 0x40)) {
        SubCharCtrl(4, 0);
    }
    n = evt->datTbl.GetNumDat();
    for (i = 0; i < n; i++) {
        if (evt->datTbl.GetDatWkNo((void**) &m, &type, i) == 0) {
            continue;
        }
        switch (type) {
        case 0:
            pl = pPL;
            if (mode & 0x10000000) {
                pl->endEvent0(1);
            } else {
                pl->endEvent0(0);
            }
            break;
        case 2:
            OyaSetObj18((cObj*) m, 0, 0);
            DelObj18((cObj*) m);
        case 3:
            ObjMgr.destroy((cObj*) m);
            break;
        case 5:
            MotionClear(m, 0);
            break;
        case 1:
        case 4:
            break;
        }
        evt->datTbl.DelDatWkNo(i);
    }
    if (evt->effNo != -1 && evt->effNo <= 1) {
        if (EvtChk(evt->status, 0x00040000)) {
            EspDataRelease(evt->effNo + 0xC4, 1, 1);
        } else {
            pLog->err(0, 0, "Event::ExeEndEvt: no EspDataRelease");
        }
    }
    EventAllEffDelete();
    if (LightMgr.roomLitCheck() == 0) {
        LightMgr.roomLitSet(0);
        LightMgr.update(0, -1);
    }
    if (CamCtrl.IsMotionSet() == 1) {
        CamCtrl.setMotionBaseMatPtr(0);
        CamCtrl.Comeback(0);
    }
    pG->flags_58 &= ~0x800;
    cMes.roomInit();
    EvtMesDeleteAll();
    ShadowMemClear();
    ExeFunc(2, 0);
    pPL->move();
    pG->flags_54 |= 0x40;
    SubScreenWait(0xF);
    cMes.loadStageFont();
    if (!EvtChk(evt->pData->sndFlag, 0x80000000)) {
        SndEventEnd();
    }
    BitOff(pG->flags_5014, 0x00080000);
    BitOff(pG->flags_5014, 0x00010000);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
}

int Event::ExeFunc(int mode, int arg)
{
    char nm[0x30];
    char a[8];
    char b[8];
    void* fn;

    if (EvtChk(status, 0x00020000)) {
        return 1;
    }
    if (mode == 1 && EvtChk(status, 0x08000000)) {
        return 1;
    }
    funcMode = mode;
    strcpy(a, pData->room);
    strcpy(b, pData->no);
    strcpy(nm, "evt_");
    strcat(nm, a);
    strcat(nm, b);
    strcat(nm, "_func");
    if (EvtMgr.GetFunc(&fn, nm) == 0) {
        return 0;
    }
    if (fn == 0) {
        pLog->err(0, 0, "Event::ExeFunc: func failed");
        return 1;
    }
    ((EvtFunc) fn)(this, arg);
    return 1;
}

void Event::CalNextPacket()
{
    pPrevPacket = pPacket;
    pPacket = (EvtPacket*) ((u8*) pPacket + pPacket->size);
    if ((u32) pPacket >= (u32) pData + pData->pacOfs + pData->pacSize) {
        status |= 0x00800000;
    }
}

void Event::CalNextFrame()
{
    char buf[0x20];
    int zero = 0;

    if (EvtChk(status, 0x20000000)) {
        if (cut >= maxCut) {
            return;
        }
    }
    if (nextCut != 0) {
        cut = nextCut - 1;
        frame = maxFrame;
        nextCut = zero;
    }
    frame++;
    totalFrame++;
    if (frame < maxFrame) {
        return;
    }
    frame = zero;
    cut++;
    if (CalMaxFrame(&maxFrame, cut) == 0) {
        pLog->err(0, 0, "Event::init : data failed");
    }
}

void Event::ChkCutZero()
{
    if (pPrevPacket != 0 && pPrevPacket->cut < 0 && pPacket->cut == 0) {
        ExeFunc(1, 0);
    }
}

int Event::CalMaxCut(int* out)
{
    EvtPacket* p;
    int n;

    if (out == 0) {
        return 0;
    }
    *out = 0;
    n = 0;
    p = (EvtPacket*) (pData->pacOfs + (u32) pData);
    while (p->id != 0x1B) {
        if (p->id > 0x20) {
            pLog->err(0, 0, "Event::CalMaxFrame : id over");
            return 0;
        }
        if (p->id == 6) {
            n++;
        }
        if (p->id == 8) {
            n++;
        }
        p = (EvtPacket*) ((u8*) p + p->size);
    }
    *out = n;
    return 1;
}

int Event::CalMaxFrame(int* out, int c)
{
    void* dat;
    EvtPacket* p;
    int n;

    if (out == 0) {
        return 0;
    }
    *out = 0;
    n = 0;
    p = (EvtPacket*) (pData->pacOfs + (u32) pData);
    while (p->id != 0x1B) {
        if (p->id > 0x20) {
            pLog->err(0, 0, "Event::CalMaxFrame : id over");
            return 0;
        }
        if (p->id == 6) {
            if (c == n) {
                if (EvtMgr.GetBin(&dat, p->mod.name, 0) == 0) {
                    pLog->err(0, 0, "Event::CalMaxFrame : dat failed");
                    return 0;
                }
                *out = *(u16*) dat + 1;
                return 1;
            }
            n++;
        }
        if (p->id == 8) {
            if (c == n) {
                *out = p->val.no;
                return 1;
            }
            n++;
        }
        p = (EvtPacket*) ((u8*) p + p->size);
    }
    *out = 0;
    return 1;
}

int Event::CalMaxTotalFrame(int* outCut, int* outTotal)
{
    int mc;
    int mf;
    int sum;
    int i;

    if (outCut == 0 || outTotal == 0) {
        return 0;
    }
    *outCut = 0;
    *outTotal = 0;
    if (CalMaxCut(&mc) == 0) {
        pLog->err(0, 0, "Event::CalMaxTotalFrame : CalMaxCut failed");
        return 0;
    }
    mf = 0;
    sum = 0;
    for (i = 0; i < mc; i++) {
        if (CalMaxFrame(&mf, i) == 0) {
            pLog->err(0, 0, "Event::CalMaxTotalFrame : CalMaxFrame failed");
            return 0;
        }
        sum += mf;
    }
    *outCut = mc;
    *outTotal = sum;
    return 1;
}


static inline void EvtFadeSetW(int no, u32 time, u32 z, int late)
{
    FadeColorPair col;
    u32 c;

    if (no & 0x80000000) {
        c = 0xFF;
        *(u32*) &col.start = c;
        c = 0;
        *(u32*) &col.end = c;
    } else {
        *(u32*) &col.start = 0;
        *(u32*) &col.end = 0xFF;
    }
    FadeSet(no, &col.start, &col.end, time, z, late);
}

void Event::SetDiedemoExec()
{
    status |= 0x100;
    if (EvtChk(status, 0x04000000)) {
        FadeKill(2);
        EvtFadeSetW(0x80000001, 0xA, 0, 0);
    }
    DiedemoExec(0, 1);
}

void Event::BeginActBtn(int no)
{
    memset(&actBtnOn, 0, 0xC);
    actBtnNo = no;
    actBtnOn = 1;
}

void Event::EndActBtn()
{
    actBtnOn = 0;
}

int Event::GetActBtnCount()
{
    return actBtnCount;
}

void Event::ExecActBtn()
{
    if (actBtnOn != 1) {
        return;
    }
    pG->flags_58 &= ~0x800;
    ActBtn.set(actBtnNo, 5, 0, 0, 2, 2, 0, 0);
    pG->flags_170 &= ~0x100;
    if (Key.trg & 0x80000) {
        actBtnCount++;
    }
}

void Event::MesSet(int no, int time, int x, int y)
{
    int i;

    if (pSys->language != 1) {
        pG->flags_58 &= ~0x800;
        if (no == -1) {
            cMes.WaitEnd(0);
        } else {
            EvtMesDeleteAll();
            if (EvtChk(status, 0x2000)) {
                SceMesSet(no, 0xF2, 1, x, y);
            } else {
                SceMesSet(no, 0xF0, 1, x, y);
            }
        }
    }
    mesNo = no;
    mesTimer = time;
}

void Event::MesClear()
{
    int no;

    if (mesTimer > 0) {
        mesTimer--;
        if (mesTimer <= 0) {
            IntSet(mesTimer, 0);
            pG->flags_58 |= 0x800;
        }
    }
    no = 0;
    EvtDebug.mesCnt[no]++;
}

void Event::FogMove(Event* evt, void* fog)
{
    f32 start;
    f32 end;
    f32 t;
    EvtFogData* d = (EvtFogData*) fog;
    int frame = evt->frame;

    if (d == 0) {
        return;
    }
    t = (f32) frame;
    if (Hermite_1CurveCalc((Hermite1*) &d->start, t, &start)) {
        LightMgr.setFogStart(start);
    }
    if (Hermite_1CurveCalc((Hermite1*) &d->end, t, &end)) {
        LightMgr.setFogEnd(end);
    }
    LightMgr.setFog();
}

void Event::FocusMove(Event* evt, void* focus)
{
    f32 near_;
    f32 far_;
    f32 t;
    EvtFocusData* d = (EvtFocusData*) focus;
    int frame = evt->frame;

    if (EvtChk(evt->status, 0x40000000)) {
        return;
    }
    if (d == 0) {
        return;
    }
    t = (f32) frame;
    if (Hermite_1CurveCalc((Hermite1*) &d->near_, t, &near_)) {
        Filter01SetParam_CamZ(0, 1, d->nearLevel, near_);
    }
    if (Hermite_1CurveCalc((Hermite1*) &d->far_, t, &far_)) {
        Filter01SetParam_CamZ(1, 1, d->farLevel, far_);
    }
}

void Event::MotClear()
{
    cModel* m;
    u8 type;
    int n;
    int i;

    n = datTbl.GetNumDat();
    for (i = 0; i < n; i++) {
        if (datTbl.GetDatWkNo((void**) &m, &type, i) == 0) {
            continue;
        }
        switch (type) {
        case 0:
        case 1:
        case 2:
        case 5:
            if (Obj18CmfGet((cObj*) m) & 0x04000000) {
                break;
            }
            if (m->x12E == 2) {
                return;
            }
            MotionClear(m, 1);
            break;
        }
    }
}

int Event::SetMod(char* nm, void* mod, u8 type, void* dat2, u8 flag, int* wkNo)
{
    int no;

    if (wkNo != 0) {
        *wkNo = 0;
    }
    if (datTbl.SetDat(nm, mod, type, dat2, flag, &no) == 0) {
        pLog->err(0, 0, "Event::SetMod : failed");
        return 0;
    }
    if (wkNo != 0) {
        *wkNo = no;
    }
    return 1;
}

int Event::GetMod(void** mod, char* nm, u8* type, int* wkNo)
{
    u8 t;
    void* m;
    int no;
    int ret;

    if (mod == 0) {
        return 0;
    }
    *mod = 0;
    if (type != 0) {
        *type = 0;
    }
    if (wkNo != 0) {
        *wkNo = 0;
    }
    if ((pG->flags_6C & 8) && strcmp(nm, "pl0200") == 0) {
        nm = "pl0300";
        if (datTbl.GetDat(&m, &t, nm, &no) == 0) {
            goto err;
        }
    } else if (datTbl.GetDat(&m, &t, nm, &no) == 0) {
    err:
        {
            register int pin asm("r27"); // COMPILER-DIFF: #17
            asm volatile("" : "=r"(pin));
            asm volatile("" : : "r"(pin));
        }
        pLog->err(0, 0, "Event::GetMod : mod failed[%s]", nm);
        return 0;
    }
    {
        // COMPILER-DIFF: #17. r27 was used-so-far in the original's global-alloc pass 0 and
        // conflicted with nm (err block) and type/wkNo/mod (tail) but not with `this`, which
        // therefore took r27 while mod fell to r28. No code is emitted.
        register int pin asm("r27");
        asm volatile("" : "=r"(pin));
        asm volatile("" : : "r"(pin));
    }
    if (type != 0) {
        *type = t;
    }
    // COMPILER-DIFF: #17 (companion). A codeless memory-operand asm = one more real insn inside the
    // wkNo compare's live range only, so the type compare (equal length, lower pseudo) is allocated
    // first and takes cr4 as in the original; a register-tied asm here is deleted as dead.
    asm("" : "+m"(no));
    if (wkNo != 0) {
        *wkNo = no;
    }
    *mod = m;
    return 1;
}

// Never called (only its string survives in .rodata).
static inline int EventDelMod(Event* evt, char* nm)
{
    if (evt->datTbl.DelDat(nm) == 0) {
        pLog->err(0, 0, "Event::DelMod : failed");
        return 0;
    }
    return 1;
}

EventMgr::EventMgr() : cManager<Event>(sizeof(Event), 2)
{
}

EventMgr::~EventMgr()
{
}

int EventMgr::construct(Event* p, u32 id)
{
    Event* e;
    int no;

    e = p->ctorI(id);
    if (e) {
        no = EvtWorkNo(this, e);
        e->effNo = no;
        if (no == -1 || no > 1) {
            pLog->err(0, 0, "EventMgr::construct : getWorkNo failed");
            return 1;
        }
    }
    return 1;
}

int EventMgr::init()
{
    setName("EventMgr");
    return 1;
}

int EventMgr::myRoomInit()
{
    int i;

    if (evdTbl.init(0x20) == 0 || binTbl.init(0x140) == 0 || funcTbl.init(0x10) == 0 || readTbl.init(8) == 0) {
        pLog->err(0, 0, "EventMgr::init : memory failed");
        return 0;
    }
    memclr_asm(&x34, sizeof(u32));
    for (i = 0; i < 0x20; i++) {
        xA4[i] = 0;
    }
    ClearEmWindowFcv();
    return 1;
}

// Never called (only its string survives in .rodata).
static inline int EventMgrEnd(EventMgr* mgr)
{
    if (mgr->evdTbl.end() == 0 || mgr->binTbl.end() == 0 || mgr->funcTbl.end() == 0 || mgr->readTbl.end() == 0) {
        pLog->err(0, 0, "EventMgr::end : failed");
        return 0;
    }
    return 1;
}

int EventMgr::DelAll()
{
    u32 i;
    Event* e;

    for (i = 0; i < nArray; i++) {
        e = (Event*) ((u8*) pArray + size * i);
        if (e->isAlive()) {
            DelEvt(e, 0);
        }
    }
    return 1;
}

int EventMgr::Run()
{
    u32 i;
    Event* e;

    dieCheck();
    for (i = 0; i < nArray; i++) {
        e = (Event*) ((u8*) pArray + size * i);
        if (!e->isAlive()) {
            continue;
        }
        if (EvtChk(e->status, 0x00080000)) {
            continue;
        }
        if (EvtChk(e->status, 0x00200000)) {
            continue;
        }
        if (EvtChk(e->status, 0x80000000)) {
            continue;
        }
        e->DebugDisp();
        if (EvtChk(e->status, 0x01000000)) {
            e->status &= ~0x01000000;
            e->ExeBeginEvt(e, 0);
        }
        if (!EvtChk(e->status, 0x00800000)) {
            if (e->Run() == 0) {
                pLog->err(0, 0, "EventMgr::Run : failed");
                DelEvt(e, 0);
                continue;
            }
            if (EvtChk(e->status, 0x20000000)) {
                continue;
            }
            if (!EvtChk(e->status, 0x02000000) && !EvtChk(e->status, 0x04000000) && !EvtChk(e->status, 0x00800000)
                && !EvtChk(e->status, 0x100) && ((Key.trg & 0x20000000) || EvtChk(e->status, 0x4000))) {
                e->RunEvtCancel();
            }
        }
        if (EvtChk(e->status, 0x00800000)) {
            if (e->mesWait != 0) {
                e->mesWait--;
                continue;
            }
            if (EvtChk(e->status, 0x00100000)) {
                e->status |= 0x00080000;
                continue;
            }
            if (EvtChk(e->status, 0x00400000)) {
                e->status |= 0x00200000;
                continue;
            }
            DelEvt(e, 1);
        }
    }
    return 1;
}

int EventMgr::IsAliveEvt(u32* key, int out, int chk)
{
    char nm[0x20];
    u32 i;
    Event* e;

    for (i = 0; i < nArray; i++) {
        char* p = nm;
        e = (Event*) ((u8*) pArray + size * i);
        if (!e->isAlive()) {
            continue;
        }
        if (chk != 1) {
            if (EvtChk(e->status, 0x00080000)) {
                continue;
            }
        }
        strcpy(p, e->name);
        if (strcmp(p, (char*) key) != 0) {
            continue;
        }
        if (out != 0) {
            *(Event**) out = e;
        }
        return 1;
    }
    return 0;
}

int EventMgr::EvtReadAram(char* nm, int em, int* out, int wait, u32 sz)
{
    int ret = 0;

    if (!(pG->flags_60 & 0x02000000)) {
        ret = EvtReadSub(nm, 1, em, out, wait, sz);
    }
    return ret;
}

int EventMgr::EvtReadMram(char* nm, int em, int* out, int wait, u32 sz)
{
    return EvtReadSub(nm, 0, em, out, wait, sz);
}

int EventMgr::NameCheck(char* nm)
{
    char tbl[37][0x20] = {
        "r105s10", "r117s00", "r117s10", "r11cs00", "r11cs10", "r11fs00", "r200s00", "r201s00", "r203s00", "r204s00",
        "r206s10", "r206s20", "r20bs00", "r212s00", "r213s00", "r214s00", "r215s00", "r215s01", "r22as00", "r300s00",
        "r304s00", "r30as00", "r30bs00", "r30cs00", "r310s00", "r316s00", "r317s05", "r325s00", "r329s00", "r330s00",
        "r331s00", "r331s10", "r332s00", "r332s10", "r332s20", "r333s00", "r333s10",
    };
    int i;
    int n = 37;

    if (pG->costume2 != 1) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (strstr(nm, tbl[i]) != 0) {
            return 1;
        }
    }
    return 0;
}

char* EventMgr::NameChange(char* nm)
{
    char* p;

    if (strlen(nm) > 0x1F) {
        pLog->err(0, 0, "EventMgr::EvtRead : Name size long failed [%s]", nm);
        return nm;
    }
    strcpy(nameBuf, nm);
    if (pG->costume2 == 1) {
        p = strchr(nameBuf, 'r');
        if (p != 0 && NameCheck(p) == 1) {
            *p = 's';
        }
    }
    return nameBuf;
}

int EventMgr::EvtReadSub(char* nm, int aram, int em, int* out, int wait, u32 sz)
{
    cDataUnit* unit = 0;
    u32 no = 0;
    int fresh = 0;
    char* p;
    u32 size;
    void* r;
    void* addr;
    ReadModule* mod;

    if (out != 0) {
        *out = 0;
    }
    if (GetRead((void**) &unit, (int*) &no, nm) == 0) {
        p = strstr(nm, "evd/");
        if (p == 0) {
            pLog->err(0, 0, "EventMgr::EvtRead : Name failed [%s]", nm);
            return 0;
        }
        p = NameChange(p);
        unit = DC.setData(p);
        if (unit == 0) {
            pLog->err(0, 0, "EventMgr::EvtRead : DC.setData failed [%s]", p);
            return 0;
        }
        if (SetRead(nm, (int*) &no, unit) == 0) {
            pLog->err(0, 0, "EventMgr::EvtRead : SetRead failed");
            return 0;
        }
        fresh = 1;
    }
    if (no > 7) {
        DelRead(nm);
        pLog->err(0, 0, "EventMgr::EvtRead : WkNo failed [%d]", no);
        return 0;
    }
    readEm[no].em = em;
    readEm[no].swapped = 0;
    if (aram == 0) {
        if (em != 0) {
            if (fresh == 1) {
                if (sz > unit->size) {
                    size = sz;
                } else {
                    size = unit->size;
                }
                r = EmReadSearch(em, 0, size);
                if (out != 0) {
                    *out = (int) r;
                }
                unit->setCommand(2, 0, 1);
            }
            if (unit->waitLoadOk() == 0) {
                unit->setCommand(3, 0, 0);
                DelRead(nm);
                pLog->err(0, 0, "readEvent() : out of memory (0x%x)[%s]", unit->size, nm);
                return 0;
            }
            EspEmDataSwapPush(em);
            mod = SearchEmModule(em);
            if (mod == 0) {
                DelRead(nm);
                pLog->err(0, 0, "EventMgr::EvtRead : no id SearchEmModule [%x]", em);
                return 0;
            }
            if (unit->size > mod->size) {
                DelRead(nm);
                pLog->err(0, 0, "EventMgr::EvtRead : event size too large!![%d]>[%d]", unit->size, mod->size);
                return 0;
            }
            MemorySwap(mod->pArc, (u32) unit->addr, unit->size);
            readEm[no].swapped = 1;
            r = mod->pArc;
            if (out != 0) {
                *out = (int) r;
            }
        } else {
            unit->setCommand(1, 0, 1);
            if (unit->waitUseOk() == 0) {
                unit->setCommand(3, 0, 0);
                DelRead(nm);
                pLog->err(0, 0, "readEvent() : out of memory (0x%x)[%s]", unit->size, nm);
                return 0;
            }
            addr = unit->addr;
            if (out != 0) {
                *out = (int) addr;
            }
        }
    } else {
        if (em != 0) {
            if (sz > unit->size) {
                size = sz;
            } else {
                size = unit->size;
            }
            r = EmReadSearch(em, 0, size);
            if (out != 0) {
                *out = (int) r;
            }
        }
        if (wait == 0) {
            unit->setCommand(2, 0, 0);
        } else {
            unit->setCommand(2, 0, 1);
        }
    }
    return 1;
}

int EventMgr::EvtReadExec(char* nm, int em, u32 flags)
{
    int addr;
    Event* evt;
    int ret = 1;

    if (flags & 0x200) {
        while (SceCheckEventStart() != 1) {
            SceSleep(1);
        }
    }
    if (flags & 0x80) {
        pLog->mes(0, 0, "EventMgr::EvtReadExec : SceEventStart(true)");
        SceEventStart(1);
    } else {
        SceEventStart(0);
    }
    BitOn(pG->flags_5014, 0x00080000);
    BitOn(pG->flags_5014, 0x00010000);
    BitOn(pG->flags_54, 0x400);
    if (em != 0) {
        SceSleep(2);
    }
    if (EvtReadMram(nm, em, &addr, 0, 0)) {
        if (EvtMgr.SetEvt((void*) addr, (u32*) &evt)) {
            if (flags & 2) {
                BitOn(evt->status, 0x00100000);
                BitOn(evt->status, 0x200);
            }
            if (flags & 0x40) {
                BitOn(evt->status, 0x00100000);
            }
            if (flags & 0x20) {
                BitOn(evt->status, 0x800);
            }
            if (flags & 0x10) {
                BitOn(evt->status, 0x400);
            }
            if (flags & 0x80) {
                BitOn(evt->status, 0x80);
            }
            if (flags & 0x100) {
                BitOn(evt->status, 0x40);
            }
        }
        if (flags & 4) {
            SceSleep(1);
            FadeSetW(0x80000002, 0x1E, 0, 0);
        }
        {
            EventMgr* m = &EvtMgr;
            while (IsAliveEvt(&m->x34, 0, 0) != 0) {
                SceSleep(1);
            }
        }
        if (flags & 2) {
            return 1;
        }
        if (flags & 0x40) {
            return 1;
        }
        EvtFree(nm);
    } else {
        pLog->err(0, 0, "EventMgr::EvtReadExec : mem over");
        ret = 0;
    }
    BitOff(pG->flags_54, 0x400);
    BitOff(pG->flags_5014, 0x00080000);
    BitOff(pG->flags_5014, 0x00010000);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    return ret;
}

int EventMgr::EvtFree(char* nm)
{
    cDataUnit* unit = 0;
    u32 no = 0;
    int em;
    ReadModule* mod;

    if (GetRead((void**) &unit, (int*) &no, nm) == 0) {
        pLog->err(0, 0, "EventMgr::EvtFree : NameEvt failed [%s]", nm);
        return 0;
    }
    DelRead(nm);
    if (no > 7) {
        pLog->err(0, 0, "EventMgr::EvtFree : WkNo failed [%d]", no);
        return 0;
    }
    em = readEm[no].em;
    if (unit != 0) {
        if (unit->waitLoadOk() == 0) {
            pLog->err(0, 0, "EvtFree() : out of memory (0x%x)[%s]", unit->size, nm);
        }
        if (em != 0 && readEm[no].swapped == 1) {
            mod = SearchEmModule(em);
            MemorySwap(mod->pArc, (u32) unit->addr, unit->size);
            readEm[no].swapped = 0;
            EspEmDataSwapPop(em);
        }
        unit->setCommand(3, 0, 0);
    }
    return 1;
}

void EventMgr::ToolCoreEvdDel()
{
    evdTbl.DelAll(0);
    binTbl.DelAll(0);
}

int EventMgr::SetEvt(void* data, u32* key)
{
    Event* evt;
    EvtHeader* hdr = (EvtHeader*) data;

    if (pG->flags_170 & 0x400) {
        return 0;
    }
    if (pG->flags_6C & 0x80) {
        return 0;
    }
    if (key != 0) {
        *key = 0;
    }
    if ((int) hdr >= 0) {
        pLog->err(0, 0, "EventMgr::SetEvs : non addr[%x]", hdr);
        return 0;
    }
    if (*(u32*) hdr->tag != 0x6576656E || hdr->tag[4] != 't') {
        pLog->err(0, 0, "EventMgr::SetEvt : invalid data");
        return 0;
    }
    if (EvtMgr.SetEvd((char*) hdr, hdr, 0, 2) == 0) {
        pLog->err(0, 0, "EventMgr::SetEvt : SetEvd failed[%s]", hdr);
        return 0;
    }
    if (EvtMgr.SetEvt((char*) hdr, &evt) == 0) {
        pLog->err(0, 0, "EventMgr::SetEvt : SetEvt failed[%s]", hdr);
        return 0;
    }
    if (key != 0) {
        *key = (u32) evt;
    }
    return 1;
}

int EventMgr::SetEvt(char* nm, Event** out)
{
    void* evd;
    Event* evt;

    if (pG->flags_170 & 0x400) {
        return 0;
    }
    if (pG->flags_6C & 0x80) {
        return 0;
    }
    if (out != 0) {
        *out = 0;
    }
    evt = create(0);
    if (evt == 0) {
        pLog->err(0, 0, "EventMgr::SetEvt : create failed[%s]", nm);
        return 0;
    }
    if (GetEvd(&evd, nm, 0) == 0) {
        pLog->err(0, 0, "EventMgr::SetEvt : non read[%s]", nm);
        DelEvt(evt, 0);
        return 0;
    }
    if (evt->init(nm, (EvtHeader*) evd) == 0) {
        pLog->err(0, 0, "EventMgr::SetEvt : init failed[%s]", nm);
        DelEvt(evt, 0);
        return 0;
    }
    evt->status |= 0x01000000;
    {
        EventMgr* m = &EvtMgr;
        strcpy(m->evtName, nm);
    }
    if (out != 0) {
        *out = evt;
    }
    return 1;
}

int EventMgr::GetEvt(u32* key, void** out)
{
    return IsAliveEvt(key, (int) out, 1);
}

int EventMgr::DelEvt(void* evt_, int flag)
{
    char nm[0x20];
    Event* evt = (Event*) evt_;
    int fade = EvtChk(evt->status, 0x04000000);
    int zero;

    switch (evt->endStep) {
    case 0:
        evt->ExeEndEvt(evt, 0);
        if (flag == 1) {
            pG->flags_54 |= 0x400;
            evt->endWait = 0;
            evt->endStep++;
            return 1;
        }
        break;
    case 1:
        evt->endWait++;
        if (evt->endWait <= 0) {
            return 1;
        }
        pG->flags_54 &= ~0x400;
        break;
    }
    pG->flags_54 &= ~0x400;
    {
        char* p = nm;
        strcpy(p, evt->name);
        destroyNow(evt);
        DelEvd(p);
    }
    strcpy(evtName, "");
    zero = 0; // COMPILER-DIFF: #13 (single-use zero set in another block: update_equiv_regs moves the li to the store, r0)
    if (fade) {
        FadeKill(2);
        {
            u32 col[2];
            u32* c = col;
            c[0] = 0xFF;
            c[1] = zero; // FadeSetW written out: the zero-offset store folds to the frame, `4(P)` keeps P tied to r4
            FadeSet(0x80000001, (GXColor*) c, (GXColor*) (c + 1), 0xA, 0, 0);
        }
    }
    return 1;
}

int EventMgr::SetBin(char* nm, void* data, void* dat2, int flag)
{
    if ((int) data >= 0) {
        pLog->err(0, 0, "EventMgr::SetBin : non addr[%s]", nm);
        return 0;
    }
    if (binTbl.SetDat(nm, data, 7, dat2, flag, 0) == 0) {
        pLog->err(0, 0, "EventMgr::SetBin : failed");
        return 0;
    }
    return 1;
}

int EventMgr::GetBin(void** out, const char* nm, int a)
{
    u8 type;
    void* dat;

    if (out == 0) {
        return 0;
    }
    *out = 0;
    if (binTbl.GetDat(&dat, &type, nm, 0) == 0) {
        char path[0x100];
        pLog->warn(0, 0, "EventMgr::GetBin : non data[%s]", nm);
        if (a == 0) {
            strcpy(path, "x:/soft/room/");
            strcat(path, nm);
            if (HDReadDebugAlloc(path, &dat, 1) == 0) {
                pLog->err(0, 0, "EventMgr::GetBin : non read[%s]", path);
                return 0;
            }
            if (SetBin((char*) nm, dat, dat, 2) == 0) {
                pLog->err(0, 0, "EventMgr::GetBin : non SetBin[%s]", path);
                return 0;
            }
        } else {
            pLog->err(0, 0, "EventMgr::GetBin : non read[%s]", nm);
            return 0;
        }
    }
    *out = dat;
    return 1;
}

int EventMgr::DelBin(char* nm)
{
    if (binTbl.DelDat(nm) == 0) {
        pLog->err(0, 0, "EventMgr::DelBin : failed");
        return 0;
    }
    return 1;
}

int EventMgr::SetEvd(char* nm, void* data, void* dat2, int flag)
{
    EvtHeader* hdr = (EvtHeader*) data;
    EvtBinEntry* e;
    int i;

    if ((int) hdr >= 0) {
        pLog->err(0, 0, "EventMgr::SetEvd : non addr[%s]", nm);
        return 0;
    }
    if (*(u32*) hdr->tag != 0x6576656E || hdr->tag[4] != 't') {
        pLog->err(0, 0, "EventMgr::SetEvd : invalid data[%s]", nm);
        return 0;
    }
    if (evdTbl.ChkDat(nm) == 1) {
        return 1;
    }
    if (evdTbl.SetDat(nm, hdr, 8, dat2, flag, 0) == 0) {
        pLog->err(0, 0, "EventMgr::SetEvd : failed");
        return 0;
    }
    for (i = 0; i < hdr->nBin; i++) {
        e = (EvtBinEntry*) (i * sizeof(EvtBinEntry) + (hdr->binOfs + (u32) hdr));
        if (SetBin(e->name, (u8*) (e->ofs + (u32) hdr), 0, flag) == 0) {
            pLog->err(0, 0, "EventMgr::SetEvd : failed");
            return 0;
        }
    }
    return 1;
}

int EventMgr::GetEvd(void** out, char* nm, int a)
{
    u8 type;
    void* dat;

    if (out == 0) {
        return 0;
    }
    *out = 0;
    if (evdTbl.GetDat(&dat, &type, nm, 0) == 0) {
        char path[0x100];
        pLog->warn(0, 0, "EventMgr::GetEvd : non data[%s]", nm);
        if (a == 0) {
            strcpy(path, "x:/soft/room/");
            strcat(path, nm);
            if (HDReadDebugAlloc(path, &dat, 1) == 0) {
                pLog->err(0, 0, "EventMgr::GetEvd : non read[%s]", path);
                return 0;
            }
            if (SetEvd(nm, dat, dat, 2) == 0) {
                pLog->err(0, 0, "EventMgr::GetEvd : non SetBin[%s]", path);
                return 0;
            }
        } else {
            pLog->err(0, 0, "EventMgr::GetEvd : non read[%s]", nm);
            return 0;
        }
    }
    *out = dat;
    return 1;
}

int EventMgr::DelEvd(char* nm)
{
    EvtHeader* hdr;
    int i;

    if (GetEvd((void**) &hdr, nm, 1) == 0) {
        pLog->err(0, 0, "EventMgr::GetEvd : non read[%s]", nm);
        return 0;
    }
    for (i = 0; i < hdr->nBin; i++) {
        DelBin(((EvtBinEntry*) (hdr->binOfs + (u32) hdr))[i].name);
    }
    if (evdTbl.DelDat(nm) == 0) {
        pLog->err(0, 0, "EventMgr::DelEvd : failed");
        return 0;
    }
    return 1;
}

int EventMgr::SetFunc(char* nm, void* func)
{
    if (funcTbl.SetDat(nm, func, 0, 0, 0, 0) == 0) {
        pLog->err(0, 0, "EventMgr::SetFunc : failed");
        return 0;
    }
    return 1;
}

// Never called (only its string survives in .rodata).
static inline int EventMgrDelFunc(EventMgr* mgr, char* nm)
{
    if (mgr->funcTbl.DelDat(nm) == 0) {
        pLog->err(0, 0, "EventMgr::DelFunc : failed");
        return 0;
    }
    return 1;
}

int EventMgr::GetFunc(void** out, char* nm)
{
    u8 type;
    void* f;

    if (out == 0) {
        return 0;
    }
    *out = 0;
    if (funcTbl.GetDat(&f, &type, nm, 0) == 0) {
        return 0;
    }
    *out = f;
    return 1;
}

int EventMgr::SetRead(char* nm, int* wkNo, void* unit)
{
    int no = 0;

    if (wkNo == 0) {
        return 0;
    }
    *wkNo = 0;
    if (readTbl.SetDat(nm, unit, 0, 0, 2, &no) == 0) {
        pLog->err(0, 0, "EventMgr::SetRead : failed");
        return 0;
    }
    *wkNo = no;
    return 1;
}

int EventMgr::GetRead(void** out, int* wkNo, char* nm)
{
    u8 type;
    int d;

    if (out == 0 || wkNo == 0) {
        return 0;
    }
    *out = 0;
    *wkNo = 0;
    if (readTbl.GetDat((void**) &d, &type, nm, 0) == 0) {
        return 0;
    }
    *out = (void*) d;
    if (readTbl.GetWkNo(&d, nm) == 0) {
        return 0;
    }
    *wkNo = d;
    return 1;
}

int EventMgr::DelRead(char* nm)
{
    if (readTbl.DelDat(nm) == 0) {
        pLog->err(0, 0, "EventMgr::DelRead : failed");
        return 0;
    }
    return 1;
}

int EventMgr::SetEvs(void* evs)
{
    EvsHeader* hdr = (EvsHeader*) evs;
    EvsEntry* tbl;
    u8* p;
    int i;

    if ((int) hdr >= 0) {
        pLog->err(0, 0, "EventMgr::SetEvs : non addr");
        return 0;
    }
    tbl = (EvsEntry*) (hdr->tblOfs + (u32) hdr);
    for (i = 0; i < hdr->num; i++) {
        EvsEntry* e = &tbl[i];
        p = (u8*) (e->ofs + (u32) hdr);
        if (SetEvd((char*) p, p, 0, 0) == 0) {
            pLog->err(0, 0, "EventMgr::SetEvs : SetEvd failed[%s]", p);
            return 0;
        }
    }
    return 1;
}

// Event stream slot accessors through an integer base: `evt->strNo[blk]` forces `evt + 0xC0` into a
// pointer-flagged temp (regclass then wants the index in GENERAL_REGS, r0); a `u32` base variable
// keeps both unflagged so the shifted index takes a BASE register (`lwzx r29,r10,r11`).
static inline int& evtStrNo(Event* evt, int blk) { u32 p = (u32) evt->strNo; return *(int*) (p + (blk << 2)); }
static inline u32& evtStrId(Event* evt, int blk) { u32 p = (u32) evt->strId; return *(u32*) (p + (blk << 2)); }

int EventMgr::EvtSndStrStop(u32* key, int blk, int mode)
{
    Event* evt;
    int no;
    u32 id;

    if (GetEvt(key, (void**) &evt) != 1) {
        return 0;
    }
    no = evtStrNo(evt, blk);
    id = evtStrId(evt, blk);
    if (no != -1) {
        if (id != 0) {
            if (SndStrReq(id, 8, 0, 0) == 1) {
                do {
                    if (mode == 0) {
                        break;
                    }
                    if (mode == 1) {
                        TaskSleep(1);
                    }
                    if (mode == 2) {
                        SceSleep(1);
                    }
                } while (SndStrStatusCk(id, 0x10) != 0);
            }
        } else {
            if (SndStrReq(blk, no, 8, 0, 0, 0.0f) == 1) {
                do {
                    if (mode == 0) {
                        break;
                    }
                    if (mode == 1) {
                        TaskSleep(1);
                    }
                    if (mode == 2) {
                        SceSleep(1);
                    }
                } while (SndStrStatusCk(blk, no, 0x10) != 0);
            }
        }
        evtStrId(evt, blk) = 0;
        evtStrNo(evt, blk) = -1;
        OSReport("EventMgr::EvtSndStrStop : stop (%d)\n", blk);
        return 1;
    }
    return 0;
}

void EventMgr::EvtSndStrPlay(u32* key, int blk, int no, int mode, f32 vol)
{
    Event* evt;
    u32 id;
    int cnt;

    if (GetEvt(key, (void**) &evt) == 1) {
        id = 0;
        if (no == -1) {
            pLog->err(0, 0, "EventMgr::EvtSndStrPlay noStr == TarNon");
            return;
        }
        if (blk == 0) {
            SndRoomStrStart(1, no, 1);
        } else if (mode == 0) {
            SndStrReq(blk, no, 0x80000003, 0, 0, 0.0f);
        } else {
            id = SndStrReq(blk, no, 1, 0, 0, vol);
            if (id != 0) {
                cnt = 0;
                for (;;) {
                    if (mode == 1) {
                        TaskSleep(1);
                    }
                    if (mode == 2) {
                        SceSleep(1);
                    }
                    if (SndStrStatusCk(id, 2) == 1) {
                        break;
                    }
                    cnt++;
                    if (cnt > 0x95) {
                        SndStrReq(id, 8, 0, 0);
                        pLog->err(0, 0, "EventMgr::EvtSndStrPlay : sleep timer over");
                        return;
                    }
                }
                SndStrReq(id, 2, 0, 0);
            }
        }
        evtStrId(evt, blk) = id;
        evtStrNo(evt, blk) = no;
        EvtDebug.strNo[blk] = no;
        OSReport("EventMgr::EvtSndStrPlay : start (%d)-(%d)\n", blk, no);
    }
}

int EventMgr::GetZeroPartsWorldPos(cModel* m, Vec* pos, Vec* rot)
{
    Vec v;
    EvtMtx mtx;
    cModel* parts = m->pParts;

    if (parts == 0) {
        return 0;
    }
    pos->x = parts->worldPos.x;
    pos->y = parts->worldPos.y;
    pos->z = parts->worldPos.z;
    pos->y = SatMgr.getFloor(pos, 600.0f, 100000.0f, 0, 0);
    if (parts->pParts == 0) {
        return 0;
    }
    v.x = 0.0f;
    v.z = 1.0f;
    v.y = 0.0f;
    mtx = *(EvtMtx*) parts->pParts->mat;
    mtx.m[0][3] = 0.0f;
    mtx.m[1][3] = 0.0f;
    mtx.m[2][3] = 0.0f;
    PSMTXMultVec(mtx.m, &v, &v);
    rot->y = LIMIT_ANGLE(atan2f(v.x, v.z));
    rot->x = 0.0f;
    rot->z = 0.0f;
    return 1;
}

void EventMgr::ClearEmWindowFcv()
{
    emWindowFcv[0] = 0;
    emWindowFcv[1] = 0;
    emWindowFcv[2] = 0;
}

void EventMgr::SetEmWindowFcv(void* a, void* b, void* c)
{
    emWindowFcv[0] = a;
    emWindowFcv[1] = b;
    emWindowFcv[2] = c;
}

void EventMgr::GetEmWindowFcv(void** a, void** b, void** c)
{
    if (a != 0) {
        *a = emWindowFcv[0];
    }
    if (b != 0) {
        *b = emWindowFcv[1];
    }
    if (c != 0) {
        *c = emWindowFcv[2];
    }
}

EventDebug::EventDebug()
{
}

EventDebug::~EventDebug()
{
}

int EventDebug::myRoomInit()
{
    flags = 0;
    return 1;
}

void EventDebug::ClrModelFiles()
{
    int i;

    for (i = 0; i < 0x60; i++) {
        memset_asm(&pModel[i], 0, sizeof(EvtDebugModel));
    }
}

int EventDebug::AddNameBinTpl(int no, char* bin, char* tpl)
{
    EvtDebugModel* m = &pModel[no];
    int n = m->nBin;

    if (n > 0xF) {
        pLog->err(0, 0, "EventDebug::AddNameBinTpl : num failed");
        return 0;
    }
    strcpy(m->bin[n], bin);
    strcpy(m->tpl[n], tpl);
    m->nBin++;
    return 1;
}

DatTbl::DatTbl()
{
    pWork = 0;
}

DatTbl::~DatTbl()
{
    end();
}

int DatTbl::init(int n)
{
    num = n;
    if (n < 0) {
        num = 0;
    }
#line 5749 "D:/Bio4/Prog/event.cpp"
    pWork = (DatTblEntry*) MEM_ALLOC(num * sizeof(DatTblEntry), 1, 0xD);
    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::init : memory failed");
        return 0;
    }
    memclr_asm(pWork, num * sizeof(DatTblEntry));
    return 1;
}

int DatTbl::end()
{
    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::end : memory failed");
        return 0;
    }
    if (DelAll(1) == 0) {
        pLog->err(0, 0, "cDatTbl::end : failed");
        return 0;
    }
    Mem_free(pWork);
    pWork = 0;
    return 1;
}

int DatTbl::SetDat(const char* nm, void* dat, u8 type, void* dat2, u8 flag, int* wkNo)
{
    int i;

    if (wkNo != 0) {
        *wkNo = 0;
    }
    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::SetDat : memory failed[%s]", nm);
        return 0;
    }
    if (strlen(nm) > 0x2F) {
        pLog->err(0, 0, "cDatTbl::SetDat : Name length[%s] %d", nm, strlen(nm));
        return 0;
    }
    for (i = 0; i < num; i++) {
        if ((pWork[i].flag & 1) && strcmp(pWork[i].name, nm) == 0) {
            pWork[i].count++;
            return 1;
        }
    }
    for (i = 0; i < num; i++) {
        if (!(pWork[i].flag & 1)) {
            memclr_asm(&pWork[i], sizeof(DatTblEntry));
            pWork[i].flag = flag | 1;
            strcpy(pWork[i].name, nm);
            pWork[i].dat = dat;
            pWork[i].type = type;
            pWork[i].dat2 = dat2;
            pWork[i].count = 1;
            if (wkNo != 0) {
                *wkNo = i;
            }
            return 1;
        }
    }
    pLog->err(0, 0, "cDatTbl::SetDat : non space[%s]", nm);
    return 0;
}

int DatTbl::GetDat(void** dat, u8* type, const char* nm, int* wkNo)
{
    int i;

    if (dat == 0 || type == 0) {
        return 0;
    }
    *dat = 0;
    *type = 0;
    if (wkNo != 0) {
        *wkNo = 0;
    }
    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::GetDat : memory failed[%s]", nm);
        return 0;
    }
    if (strlen(nm) > 0x2F) {
        pLog->err(0, 0, "cDatTbl::GetDat : Name length[%s] %d", nm, strlen(nm));
        return 0;
    }
    for (i = 0; i < num; i++) {
        if ((pWork[i].flag & 1) && strcmp(pWork[i].name, nm) == 0) {
            *dat = pWork[i].dat;
            *type = pWork[i].type;
            if (wkNo != 0) {
                *wkNo = i;
            }
            return 1;
        }
    }
    return 0;
}

int DatTbl::ChkDat(const char* nm)
{
    int i;

    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::ChkDat : memory failed[%s]", nm);
        return 0;
    }
    if (strlen(nm) > 0x2F) {
        pLog->err(0, 0, "cDatTbl::ChkDat : Name length[%s] %d", nm, strlen(nm));
        return 0;
    }
    for (i = 0; i < num; i++) {
        if ((pWork[i].flag & 1) && strcmp(pWork[i].name, nm) == 0) {
            return 1;
        }
    }
    return 0;
}

int DatTbl::GetWkNo(int* wkNo, const char* nm)
{
    int i;

    if (wkNo != 0) {
        *wkNo = 0;
    }
    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::ChkDat : memory failed[%s]", nm);
        return 0;
    }
    if (strlen(nm) > 0x2F) {
        pLog->err(0, 0, "cDatTbl::ChkDat : Name length[%s] %d", nm, strlen(nm));
        return 0;
    }
    for (i = 0; i < num; i++) {
        if ((pWork[i].flag & 1) && strcmp(pWork[i].name, nm) == 0) {
            *wkNo = i;
            return 1;
        }
    }
    return 0;
}

int DatTbl::GetNumDat()
{
    return num;
}

int DatTbl::GetDatWkNo(void** dat, u8* type, int wkNo)
{
    if (dat == 0) {
        return 0;
    }
    *dat = 0;
    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::GetDatWkNo : memory failed[%d]", wkNo);
        return 0;
    }
    if (wkNo >= num) {
        pLog->err(0, 0, "cDatTbl::GetDatWkNo : work_no failed[%d]", wkNo);
        return 0;
    }
    if (pWork[wkNo].flag & 1) {
        *dat = pWork[wkNo].dat;
        *type = pWork[wkNo].type;
        return 1;
    }
    return 0;
}

int DatTbl::ChkDatWkNoName(int wkNo, const char* nm)
{
    DatTblEntry* e;

    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::GetDatWkNo : memory failed[%d]", wkNo);
        return 0;
    }
    if (wkNo >= num) {
        pLog->err(0, 0, "cDatTbl::GetDatWkNo : work_no failed[%d]", wkNo);
        return 0;
    }
    e = (DatTblEntry*) (wkNo * sizeof(DatTblEntry) + (u32) pWork);
    if ((e->flag & 1) && strcmp(e->name, nm) == 0) {
        return 1;
    }
    return 0;
}

int DatTbl::DelDatWkNo(int wkNo)
{
    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::DelDatWkNo : memory failed[%d]", wkNo);
        return 0;
    }
    if (wkNo >= num) {
        pLog->err(0, 0, "cDatTbl::DelDatWkNo : work_no failed[%d]", wkNo);
        return 0;
    }
    if (pWork[wkNo].flag & 1) {
        pWork[wkNo].count--;
        if ((s16) pWork[wkNo].count <= 0 && (pWork[wkNo].flag & 2)) {
            if (pWork[wkNo].dat2 != 0) {
                Debug_free(pWork[wkNo].dat2);
            }
            memclr_asm(&pWork[wkNo], sizeof(DatTblEntry));
        }
        return 1;
    }
    pLog->err(0, 0, "cDatTbl::DelDatWkNo : non dat[%d]", wkNo);
    return 0;
}

int DatTbl::DelDat(const char* nm)
{
    int i;

    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::DelDat : memory failed[%s]", nm);
        return 0;
    }
    if (strlen(nm) > 0x2F) {
        pLog->err(0, 0, "cDatTbl::DelDat : Name length[%s] %d", nm, strlen(nm));
        return 0;
    }
    for (i = 0; i < num; i++) {
        if ((pWork[i].flag & 1) && strcmp(pWork[i].name, nm) == 0) {
            pWork[i].count--;
            if ((s16) pWork[i].count <= 0 && (pWork[i].flag & 2)) {
                if (pWork[i].dat2 != 0) {
                    Debug_free(pWork[i].dat2);
                }
                memclr_asm(&pWork[i], sizeof(DatTblEntry));
            }
            return 1;
        }
    }
    pLog->err(0, 0, "cDatTbl::DelDat : non dat[%s]", nm);
    return 0;
}

int DatTbl::DelAll(int all)
{
    int i;

    if (pWork == 0) {
        pLog->err(0, 0, "cDatTbl::DelAll : memory failed");
        return 0;
    }
    for (i = 0; i < num; i++) {
        if ((pWork[i].flag & 1) && ((pWork[i].flag & 2) || all == 0)) {
            if (pWork[i].dat2 != 0) {
                Debug_free(pWork[i].dat2);
            }
            memclr_asm(&pWork[i], sizeof(DatTblEntry));
        }
    }
    return 1;
}

int SndStrPlayBlock(int blk, int no, f32 vol)
{
    u32 id = SndStrReq(blk, no, 1, 0, 0, vol);

    if (id != 0) {
        do {
            SceSleep(1);
        } while (SndStrStatusCk(id, 2) != 1);
        SndStrReq(id, 2, 0, 0);
    }
    return id;
}

void SndStrStopBlock(int blk)
{
    u32 id = blk;

    if (SndStrReq(id, 8, 0, 0) == 1) {
        do {
            SceSleep(1);
        } while (SndStrStatusCk(id, 0x10) != 0);
    }
}

// Six unreferenced zero-initialised words (Bio4.sym has no name for them).
int lbl_803149C8 = 0;
int lbl_803149CC = 0;
int lbl_803149D0 = 0;
int lbl_803149D4 = 0;
int lbl_803149D8 = 0;
int lbl_803149DC = 0;
