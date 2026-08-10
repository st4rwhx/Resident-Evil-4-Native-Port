#ifndef EVENT_H
#define EVENT_H

#include "types.h"
#include "vec.h"
#include "db_log.h"
#include "cManager.h"
#include "main_mem.h"

#line 8 "D:/Bio4/Prog/event.h"

// Event system header (game/event.cpp: the cutscene player). db_work includes it between atari.h
// and light.h; only string literals of its inline members land in the including units' .rodata:
// the file name (EventMgr::memAlloc's MEM_ALLOC) followed by the empty string.
class cEvent {
public:
    u8* pData;
    u32 nData;

    u8* getData(u32 no) {
        if (no >= nData) {
            dbgAssert(__FILE__, __LINE__);
        }
        return pData + no;
    }
    // some inline in the original event.h carries an empty string literal: 4 zero bytes follow
    // the file-name string in every including unit's .rodata
    const char* emptyName() { return ""; }
};

class cModel;
class cLit;

// Named data slot table (game/event.cpp `cDatTbl`, demangled `DatTbl`): `num` entries of 0x3C.
struct DatTblEntry {
    char name[0x30];   // 0x00
    u8 flag;           // 0x30  bit0: in use, bit1: `dat2` is a debug-heap block freed with the entry
    u8 type;           // 0x31
    u16 count;         // 0x32  reference count (SetDat of an existing name increments it)
    void* dat;         // 0x34
    void* dat2;        // 0x38
};

class DatTbl {
public:
    int num;               // 0x00
    DatTblEntry* pWork;    // 0x04

    DatTbl();
    ~DatTbl();
    int init(int n);
    int end();
    int SetDat(const char* name, void* dat, u8 type, void* dat2, u8 flag, int* wkNo);
    int GetDat(void** dat, u8* type, const char* name, int* wkNo);
    int ChkDat(const char* name);
    int GetWkNo(int* wkNo, const char* name);
    int GetNumDat();
    int GetDatWkNo(void** dat, u8* type, int wkNo);
    int ChkDatWkNoName(int wkNo, const char* name);
    int DelDatWkNo(int wkNo);
    int DelDat(const char* name);
    int DelAll(int all);
};

// Event file ("even" "t" header) the room's evd data hands to EventMgr::SetEvt.
struct EvtBinEntry {
    char name[0x30];   // 0x00  file name of the bin/tpl
    u32 ofs;           // 0x30  offset of its data from the header
    u8 pad_34[0xC];
};

struct EvtHeader {
    char tag[8];       // 0x00  "even" "t"
    u8 pad_8[0x18];
    char room[8];      // 0x20  "r105"
    char no[0xC];      // 0x28  "s10"
    s32 sndFlag;       // 0x34  bit31: no SndEventInit / SndEventEnd
    u8 pad_38[8];
    u32 pacOfs;        // 0x40  first packet
    u32 pacSize;       // 0x44  size of the packet stream
    s32 nBin;          // 0x48  entries of the bin table
    u32 binOfs;        // 0x4C  EvtBinEntry[nBin]
};

// Event packet stream: a 0x10 header followed by the per-id parameters (`size` bytes in total).
// The union members below are the shapes the handlers read.
struct EvtPacket {
    int id;            // 0x00  packetTbl index (0..0x20)
    u32 flag;          // 0x04  bit31: position relative to the base model, bit30: OyaSetObj18, bit29: Str one-shot
    s16 cut;           // 0x08
    s16 frame;         // 0x0A
    s16 size;          // 0x0C  offset of the next packet
    s16 pad_E;
    union {
        struct {
            char name[0xC];   // 0x10  model name
            char bin[0x30];   // 0x1C
            char tpl[0x30];   // 0x4C
        } mod;
        struct {
            char name[0xC];   // 0x10
            char oya[0xC];    // 0x1C
            char bin[0x30];   // 0x28
            char tpl[0x30];   // 0x58
        } parts;
        struct {
            char name[0xC];   // 0x10
            char oya[0xC];    // 0x1C
            s32 pos[3];       // 0x28
            s32 rot[3];       // 0x34  degrees
            s32 partsNo;      // 0x40
        } pos;
        struct {
            char name[0xC];   // 0x10
            s32 type;         // 0x1C
            u8 pad_20[3];
            u8 parts;         // 0x23
        } esp;
        struct {
            s32 no;           // 0x10
            s32 arg;          // 0x14
            s32 time;         // 0x18
        } val;
    };
};

// One event model registered by the debug tool (EventDebug::pModel, 0x644 bytes).
struct EvtDebugModel {
    char name[0x30];       // 0x00
    char bin[16][0x30];    // 0x30
    char tpl[16][0x30];    // 0x330
    s32 nBin;              // 0x630
    cModel* pModel;        // 0x634
    u8 x638;               // 0x638  cModel::x12F
    u8 x639;               // 0x639  cModel::lightInfo.x50
    u8 pad_63A[2];
    u32 flags;             // 0x63C  bit31: scroll object, bit30: be_flag bit12
    cModel* pScr;          // 0x640  the model when its name starts with "scr"
};

// Event work (game/event.cpp): a cUnit managed by EventMgr, 0x13C bytes.
class Event : public cUnit {
public:
    u8 xC;                 // 0x0C
    s8 endStep;            // 0x0D  DelEvt: 0 run ExeEndEvt, 1 wait `endWait` frames
    s8 endWait;            // 0x0E
    u8 xF;                 // 0x0F
    u8 type;               // 0x10  constructor argument (EventMgr::construct id)
    u8 pad_11[3];
    int effNo;             // 0x14  effect owner slot: -1 none, 0/1 -> EspDataLoad owner 0xC4 + effNo
    char name[0x20];       // 0x18  event name ("r105s10")
    EvtHeader* pData;      // 0x38
    EvtPacket* pPacket;    // 0x3C  current packet
    EvtPacket* pPrevPacket;  // 0x40  packet executed before it
    u32 status;            // 0x44  EVT_ST_* bits (FlgOnStatus numbers them from bit 31 down)
    DatTbl datTbl;         // 0x48  models of the event (name -> cModel*, type)
    Mtx camMat;            // 0x50  camera base matrix (ExePacket_Pos "cam0000")
    cModel* pOya;          // 0x80  the "pl0000" object model (player stand-in)
    cModel* pPosOya;       // 0x84  "oya0000" position base
    void* x88;             // 0x88
    u32 funcTbl;           // 0x8C  ExePacket_Func table (void (*[])(Event*, int)), kept as an address
    int totalFrame;        // 0x90
    int maxTotalFrame;     // 0x94
    int frame;             // 0x98  frame in the cut
    int maxFrame;          // 0x9C  frames of the cut
    int cut;               // 0xA0
    int maxCut;            // 0xA4
    int dbgTotalFrame;     // 0xA8  DebugDisp copies (DebugDispTool prints them)
    int dbgMaxTotalFrame;  // 0xAC
    int dbgFrame;          // 0xB0
    int dbgMaxFrame;       // 0xB4
    int dbgCut;            // 0xB8
    int dbgMaxCut;         // 0xBC
    int strNo[2];          // 0xC0  stream number per block (-1 = none)
    u32 strId[2];          // 0xC8  SndStrReq id per block
    int cancelCut;         // 0xD0  RunEvtCancel: cut the cancel skips to
    int mesTimer;          // 0xD4
    int xD8;               // 0xD8
    int xDC;               // 0xDC
    int toolFrame;         // 0xE0  RunTool: frame the tool seeks to
    int actBtnOn;          // 0xE4
    int actBtnCount;       // 0xE8
    int actBtnNo;          // 0xEC
    int funcMode;          // 0xF0  ExeFunc mode the Evt_*_Func handler sees (0 begin, 1 run, 2 end, 3 cancel)
    int nEspModel;         // 0xF4  EspEvModList entries used
    void* pFog;            // 0xF8  fog Hermite curves (ExePacket_Fog)
    void* pFocus;          // 0xFC  focus Hermite curves (ExePacket_Focus)
    int mesNo;             // 0x100
    int mesWait;           // 0x104
    int strTime;           // 0x108  ExePacket_Str time override
    int nextCut;           // 0x10C  cut jump pending (CalNextFrame)
    int toolCut;           // 0x110
    int toolFrame2;        // 0x114
    cLit* pLit;            // 0x118  room lit set by ExePacket_Lit
    u8 pad_11C[0x13C - 0x11C];

    Event(u8 type);
    // int-parameter alias of the constructor: EventMgr::construct passes its u32 id without a clrlwi.
    Event* ctorI(int type) asm("__5EventUc");
    virtual ~Event();
    int init(char* name, EvtHeader* data);
    int Run();
    void EspSetModelPtr(cModel* m);
    int EspToolSetDat();
    void EspToolSetMod(int no, char* name);
    int GetModelPtrNo(int* no, cModel** mod, char* name);
    int RunTool(int mode, int arg);
    int RunEvtCancel();
    void CancelSet();
    void CancelNoSet();
    void ControlTransFlag();
    void DebugDisp();
    void DebugDispTool();
    int IsExePacket();
    int ExePacket();
    static int ExePacket_BeginEvt(Event* evt);
    static int ExePacket_SetPl(Event* evt);
    static int ExePacket_SetEm(Event* evt);
    static int ExePacket_SetOm(Event* evt);
    static int ExePacket_SetParts(Event* evt);
    int ExePacket_SetPartsSub(char* name, char* bin, char* tpl, char* oya);
    static int ExePacket_SetList(Event* evt);
    static int ExePacket_SetEff(Event* evt);
    static int ExePacket_SetMdt(Event* evt);
    static int ExePacket_Cam(Event* evt);
    static int ExePacket_CamPos(Event* evt);
    static int ExePacket_CamDammy(Event* evt);
    static int ExePacket_Pos(Event* evt);
    static int ExePacket_PosPl(Event* evt);
    static int ExePacket_Mot(Event* evt);
    static int ExePacket_Shp(Event* evt);
    static int ExePacket_Esp(Event* evt);
    static int ExePacket_Lit(Event* evt);
    static int ExePacket_Fog(Event* evt);
    static int ExePacket_Focus(Event* evt);
    static int ExePacket_Str(Event* evt);
    static int ExePacket_Se(Event* evt);
    static int ExePacket_Fade(Event* evt);
    static int ExePacket_Mes(Event* evt);
    static int ExePacket_Func(Event* evt);
    static int ExePacket_ParentOn(Event* evt);
    static int ExePacket_ParentOff(Event* evt);
    static int ExePacket_EndPl(Event* evt);
    static int ExePacket_EndEm(Event* evt);
    static int ExePacket_EndOm(Event* evt);
    static int ExePacket_EndParts(Event* evt);
    static int ExePacket_EndList(Event* evt);
    static int ExePacket_EndEvt(Event* evt);
    static int ExePacket_EndPac(Event* evt);
    void ExeBeginEvt(Event* evt, int mode);
    void ExeEndEvt(Event* evt, u32 mode);
    int ExeFunc(int mode, int arg);
    void CalNextPacket();
    void CalNextFrame();
    void ChkCutZero();
    int CalMaxCut(int* maxCut);
    int CalMaxFrame(int* maxFrame, int cut);
    int CalMaxTotalFrame(int* maxCut, int* maxTotal);
    void SetDiedemoExec();
    void BeginActBtn(int no);
    void EndActBtn();
    int GetActBtnCount();
    void ExecActBtn();
    void MesSet(int no, int time, int x, int y);
    void MesClear();
    void FogMove(Event* evt, void* fog);
    void FocusMove(Event* evt, void* focus);
    void MotClear();
    int SetMod(char* name, void* mod, u8 type, void* dat2, u8 flag, int* wkNo);
    int GetMod(void** mod, char* name, u8* type, int* wkNo);
    void FlgOnStatus(u32 no)
    {
        u32* f = &status;
        f[no >> 5] |= 0x80000000 >> (no & 0x1F);
    }
};

// Enemy module the manager loaded for an event (EventMgr::readEm[8], 4 bytes).
struct EvtReadEm {
    u8 em;       // 0x00  enemy id (0 = none)
    u8 swapped;  // 0x01  1 = its data was swapped into the module block
    u8 pad_2[2];
};

// Event manager (game/event.cpp `EvtMgr`, 0x180 bytes): a cManager<Event> (game.cpp instantiates
// roomInit / arrayAlloc / arrayFree / dispWorkNum on it).
class EventMgr : public cManager<Event> {
public:
    union {
        u32 x34;           // 0x34  running event key (sce_com SceChapterEnd: IsAliveEvt / GetEvt)
        char evtName[0x30];  // 0x34  name of the running event ("" = none)
    };
    EvtReadEm readEm[8];   // 0x64  enemy modules loaded per read slot
    char nameBuf[0x20];    // 0x84  NameChange result
    u32 xA4[0x20];         // 0xA4  cleared by myRoomInit
    u8 pad_124[0x144 - 0x124];
    void* emWindowFcv[3];  // 0x144  window jump motions (emwindow ExeWindowEvent)
    DatTbl evdTbl;         // 0x150  event data by name (0x20)
    DatTbl binTbl;         // 0x158  bin/tpl files by name (0x140)
    DatTbl funcTbl;        // 0x160  Evt_*_Func handlers by name (0x10)
    DatTbl readTbl;        // 0x168  data units being read (0x8)
    u8 pad_170[0x180 - 0x170];

    EventMgr();
    virtual ~EventMgr();
#line 1107 "D:/Bio4/Prog/event.h"
    virtual void* memAlloc(u32 size) { return MEM_ALLOC(size, 1, 0xD); }
    virtual void memFree(void* p) { Mem_free(p); }
    virtual void memClear(Event* p, u32 size) { memclr_asm(p, size); }
    virtual int construct(Event* p, u32 id);

    int init();
    int myRoomInit();           // room start (game gameRoomInit, after arrayAlloc(2))
    int DelAll();
    int SetEvs(void* evs);      // room "EVS" data (game gameRoomInit)
    int Run();
    int IsAliveEvt(u32* key, int out, int chk);
    int EvtReadAram(char* name, int em, int* out, int wait, u32 size);
    int EvtReadMram(char* name, int em, int* out, int wait, u32 size);
    int NameCheck(char* name);
    // Copies the event file name into the manager (Ashley costume 1 swaps the 'r' of the room
    // name for 's'); returns the stored copy (the rooms hand it to DC.setData).
    char* NameChange(char* name);
    int EvtReadSub(char* name, int aram, int em, int* out, int wait, u32 size);
    int EvtReadExec(char* name, int em, u32 flags);
    int EvtFree(char* name);
    void ToolCoreEvdDel();
    // Starts the loaded event data ("even" "t" header); `key` (optional) receives its key.
    int SetEvt(void* data, u32* key);
    int SetEvt(char* name, Event** out);
    int GetEvt(u32* key, void** out);
    int DelEvt(void* evt, int a);
    int SetBin(char* name, void* data, void* dat2, int flag);
    // Looks a file of the running event up by name; 0 when it is not loaded.
    int GetBin(void** out, const char* name, int a);
    int DelBin(char* name);
    int SetEvd(char* name, void* data, void* dat2, int flag);
    int GetEvd(void** out, char* name, int a);
    int DelEvd(char* name);
    int SetFunc(char* name, void* func);
    int GetFunc(void** out, char* name);
    int SetRead(char* name, int* wkNo, void* unit);
    int GetRead(void** out, int* wkNo, char* name);
    int DelRead(char* name);
    int EvtSndStrStop(u32* key, int blk, int wait);
    void EvtSndStrPlay(u32* key, int blk, int no, int wait, f32 vol);
    int GetZeroPartsWorldPos(cModel* m, Vec* pos, Vec* rot);
    void ClearEmWindowFcv();
    void SetEmWindowFcv(void* a, void* b, void* c);
    // Replaces the three window jump motions of the running event (emwindow ExeWindowEvent).
    void GetEmWindowFcv(void** a, void** b, void** c);
};

extern EventMgr EvtMgr;

// Event debug tool work (game/event.cpp `EvtDebug`, 0xE8 bytes).
class EventDebug {
public:
    u8 pad_0[0x60];
    char evName[0x30];     // 0x60  packet 6 name (EspToolSetDat)
    char camName[0x30];    // 0x90  packet 0xE name
    s32 mesCnt[3];         // 0xC0
    int strNo[2];          // 0xCC  last stream number per block
    s32 strWait;           // 0xD4
    int toolCut;           // 0xD8
    s32 nModel;            // 0xDC
    EvtDebugModel* pModel; // 0xE0  0x60 entries
    u32 flags;             // 0xE4  tool switches (bit19 fog off, bit20 focus off, bit18 lit off, bit21 mes off)

    EventDebug();
    ~EventDebug();
    int myRoomInit();          // room start (game gameRoomInit)
    char* getEvName() { return evName; }
    char* getCamName() { return camName; }
    void ClrModelFiles();
    int AddNameBinTpl(int no, char* bin, char* tpl);
};

extern EventDebug EvtDebug;

// game/event.cpp (C linkage): streamed sound blocks of the running event
extern "C" {
int SndStrPlayBlock(int a, int no, f32 vol);
void SndStrStopBlock(int blk);
}

#endif
