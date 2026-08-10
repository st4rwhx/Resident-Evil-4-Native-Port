#include "types.h"
#include "light.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "main_mem.h"
#include "area.h"
#include "player.h"
#include "file.h"
#include "db_filelist.h"
#include "t_util.h"

// Data-read area editor (Tools/t_dr.cpp): trigger areas of a room saved as a `.dra` file (header, area
// records, file-name strings). The tool entry is gone from tools.cpp; the routine table and the
// routines remain.

extern "C" unsigned int strlen(const char* s);
extern "C" char* strcpy(char* dst, const char* src);
extern "C" int strcmp(const char* a, const char* b);
extern "C" char* strrchr(const char* s, int c);
extern "C" void* memcpy(void* dst, const void* src, unsigned int n);
int SetToolLight(int no);  // db_light_tools.cpp
extern "C" int sprintf(char* s, const char* fmt, ...);

struct DrArea {
    u32 x0;           // 0x00
    AreaData data;    // 0x04
    u8 flag;          // 0x34  bit0: in use, bit1: initialised
    u8 type;          // 0x35  load type (drTypeName)
    u8 no;            // 0x36  slot (saved)
    u8 file;          // 0x37  file-name index (saved)
};

struct DrHeader {
    char id[4];       // 0x00  "DRA"
    u16 version;      // 0x04  0x100
    u16 nArea;        // 0x06
    u16 nFile;        // 0x08
    u16 pad_A;
    u32 strOfs;       // 0x0C  offset of the file-name strings
};

struct DrWork {
    u8 mode;          // 0x0000  routine (tDrFunc)
    u8 step;          // 0x0001
    u8 sub;           // 0x0002
    u8 x3;            // 0x0003
    u8 pad_4[2];
    s8 cursor;        // 0x0006
    s8 mainCursor;    // 0x0007
    s8 exitCursor;    // 0x0008
    s8 sel;           // 0x0009
    s16 timer;        // 0x000A
    s16 x;            // 0x000C
    s16 y;            // 0x000E
    u8 pad_10[4];
    s16 listTop;      // 0x0014
    char fileA[64];   // 0x0016  local path
    char fileB[64];   // 0x0056  server path
    s16 areaNo;       // 0x0096
    u32 saveSize;     // 0x0098
    u8 pad_9C[4];
    DrArea area[128];        // 0x00A0
    char* fileName[128];     // 0x1CA0
    DrHeader hdr;            // 0x1EA0  save image
    DrArea hdrArea[128];     // 0x1EB0
    char strBuf[0x8000];     // 0x3AB0
    u8* pData;               // 0xBAB0
    DrArea* pArea;           // 0xBAB4
    char* pStr;              // 0xBAB8
};

// every store through the work pointer reloads it: the pointer is a struct member
struct DrWorkPtr {
    DrWork* p;
};
static DrWorkPtr drWork;
#define DR (drWork.p)

static void tDrExit();
static void tDrMainMenu();
static void tDrArea();
static void tDrArea_ListDisp();
void dispAreaList1(int x, int y, int no);
static void tDrArea_Menu();
static void tDrArea_Menu_filelist();
static void tDrArea_Menu_main();
static void tDrArea_Create();
static void tDrArea_Delete();
static void tDrArea_Move();
char* tDr_getFilename(u8 no, int flag);
static void tDrDataLoad();
static void tDrDataSave();
void tDrSaveDataCreate();

extern void (*const tDrFunc[5])();
void (*const tDrFunc[5])() = {
    tDrMainMenu, tDrArea, tDrDataLoad, tDrDataSave, tDrExit,
};

// never called (the tools.cpp menu has no entry for this tool): only its strings remain
static inline void tDrInit()
{
    DR = (DrWork*) Debug_alloc(sizeof(DrWork), 0);
    eprintf(DR->x, DR->y, 4, 0, "[DATAREAD AREA EDIT TOOL]");
    sprintf(DR->fileA, "d:\\bio4\\room\\st%1x\\r%03x\\r%03x.dra", pG->stage_no, pG->room_no, pG->room_no);
    sprintf(DR->fileB, "x:\\soft\\room\\st%1x\\r%03x\\r%03x.dra", pG->stage_no, pG->room_no, pG->room_no);
}

static TOOL_MENU drExitMenu[2] = {
    {1, "YES", 0},
    {1, "NO", 0},
};

static void tDrExit()
{
    s8 ret;

    eprintf(DR->x += 40, DR->y += 90, 4, 0, "EXIT?");
    DR->x += 8;
    DR->y += 32;
    switch (DR->step) {
    case 0:
        DR->exitCursor = 1;
        DR->step++;
    case 1:
        ret = ToolMenuDisp_cur(DR->x, DR->y, 1, &DR->exitCursor, drExitMenu, sizeof(drExitMenu), Joy);
        if (ret < 0) {
            break;
        }
        switch (ret) {
        case 0:
            DR->step = 9;
            break;
        case 1:
            DR->mode = 0;
            DR->step = 0;
            DR->sub = 0;
            DR->x3 = 0;
            break;
        }
        break;
    case 9:
        file_unlock(DR->fileB);
        Debug_free(DR);
        TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x10000000;
        SetToolLight(-1);
        TutilQuitDefault();
        TaskExit();
        break;
    }
}

static TOOL_MENU drMainMenu[4] = {
    {1, "AREA EDIT", 0},
    {1, "DATA LOAD", 0},
    {1, "DATA SAVE", 0},
    {1, "EXIT", 0},
};

static void tDrMainMenu()
{
    s8 ret = ToolMenuDisp_cur(DR->x, DR->y, 1, &DR->mainCursor, drMainMenu, sizeof(drMainMenu), Joy);

    if (ret < 0) {
        return;
    }
    switch (ret) {
    case 0:
        DR->mode = 1;
        break;
    case 1:
        DR->mode = 2;
        break;
    case 2:
        DR->mode = 3;
        break;
    case 3:
        DR->mode = 4;
        break;
    }
    DR->listTop = 0;
    DR->step = 0;
    DR->sub = 0;
    DR->x3 = 0;
}

static void tDrArea()
{
    void (*tbl[3])() = {tDrArea_ListDisp, tDrArea_Menu, tDrArea_Move};

    DR->x += 8;
    eprintf(DR->x, DR->y, 0, 0, "AREA  FILENAME");
    DR->y += 16;
    tbl[DR->step]();
}

static inline void tDrLimit(DrWork* w, int lo, int hi)
{
    u16 v = w->areaNo;

    if ((s16) v >= lo) {
        if ((s16) v > hi) {
            v = hi;
        }
    } else {
        v = lo;
    }
    w->areaNo = v;
}

static void tDrArea_ListDisp()
{
    u16 v;
    int end;
    int i;

    if (Joy[0].rep2 & 0x6C) {
        if (DR->step == 0) {
            if (Joy[0].rep2 & 4) {
                DR->areaNo++;
            }
            if (Joy[0].rep2 & 8) {
                DR->areaNo--;
            }
        }
        tDrLimit(DR, 0, 127);
    }
    if (Joy[0].trg & 0x100) {
        DR->step = 1;
        DR->sub = 0;
        DR->x3 = 0;
    }
    if (Joy[0].trg & 0x200) {
        DR->cursor = 0;
        DR->mode = 0;
        DR->step = 0;
        DR->sub = 0;
        DR->x3 = 0;
    }
    if (DR->listTop < DR->areaNo - 6) {
        DR->listTop = DR->areaNo - 6;
    }
    if (DR->listTop > DR->areaNo) {
        DR->listTop = DR->areaNo;
    }
    // the else arm re-reads the expression (t_sce_item ListDisp idiom): cse1 then stops at the join
    // label and the eprintf block reloads DR
    if (DR->listTop + 7 > 128) {
        end = 128;
    } else {
        end = DR->listTop + 7;
    }
    eprintf(DR->x - 8, DR->y + (DR->areaNo - DR->listTop) * 16, 0, 0, ">");
    for (i = DR->listTop; i < end; i++) {
        dispAreaList1(DR->x, DR->y, i);
        DR->y += 16;
    }
}

void dispAreaList1(int x, int y, int no)
{
    DrArea* a = (DrArea*) (no * sizeof(DrArea) + (u32) DR->area);

    if (a->flag & 1) {
        int col;

        eprintf(x, y, no == DR->areaNo ? 6 : 0, 0, "[%3d]", no);
        col = no == DR->areaNo ? 6 : 0;
        eprintf(x, y, (u8) col, 0, "       %s", tDr_getFilename(no, 0));
    } else {
        eprintf(x, y, 20, 0, "[%3d]", no);
        eprintf(x, y, 20, 0, "       no data...");
    }
}

static void tDrArea_Menu()
{
    void (*tbl[2])() = {tDrArea_Menu_main, tDrArea_Menu_filelist};

    tbl[DR->sub]();
}

static void tDrArea_Menu_filelist()
{
    char* name;

    DR->y += 32;
    name = DbgFileList.disp(DR->x, DR->y, 20);
    if (Joy[0].trg & 0x100) {
        DR->fileName[DR->areaNo] = name;
        DR->sub = 0;
    }
    if (Joy[0].trg & 0x200) {
        DR->sub = 0;
    }
}

static TOOL_MENU drCreateMenu[1] = {
    {1, "AREA CREATE", tDrArea_Create},
};
static TOOL_MENU drEditMenu[4] = {
    {1, "AREA MOVE", 0},
    {1, "ID", 0},
    {1, "FILE", 0},
    {1, "AREA DELEAT", tDrArea_Delete},
};
static const char* drTypeName[2] = {"MRAM_LOAD", "ARAM_LOAD"};

static void tDrArea_Menu_main()
{
    s16 x = DR->x;
    s16 y = DR->y;
    DrArea* a = &DR->area[DR->areaNo];
    s8 ret;

    dispAreaList1(x, y, DR->areaNo);
    x += 8;
    y += 32;
    if (!(a->flag & 1)) {
        ToolMenuDisp_cur(x, y, 0, &DR->cursor, drCreateMenu, sizeof(drCreateMenu), Joy);
    } else {
        ret = ToolMenuDisp_cur(x, y, 0, &DR->cursor, drEditMenu, sizeof(drEditMenu), Joy);
        if (ret >= 0) {
            switch (ret) {
            case 0:
                DR->step = 2;
                DR->sub = 0;
                DR->x3 = 0;
                break;
            case 1:
                break;
            case 2:
                DR->sub = 1;
                break;
            }
        }
        if (DR->cursor == 1) {
            int type = a->type;

            if (Joy[0].rep2 & 0x20002) {
                type++;
            }
            if (Joy[0].rep2 & 0x10001) {
                type--;
            }
            int n;

            if (type >= 0) {
                n = type;
                if (n > 1) {
                    n = 1;
                }
            } else {
                n = 0;
            }
            a->type = n;
        }
        x += 128;
        y += 16;
        eprintf(x, y, 0, 0, "%s", drTypeName[a->type]);
        y += 16;
        eprintf(x, y, 0, 0, "%s", tDr_getFilename(DR->areaNo, 0));
        y += 16;
    }
    if (Joy[0].trg & 0x200) {
        DR->cursor = 0;
        DR->step = 0;
        DR->sub = 0;
        DR->x3 = 0;
    }
    DR->y = y;
}

static void tDrArea_Create()
{
    DrArea* a = &DR->area[DR->areaNo];

    if (!(a->flag & 2)) {
        AreaDataInit(&a->data, &pPL->pos, 1, 5000.0f, 2000.0f);
    }
    a->type = 0;
    a->flag |= 3;
    DR->cursor = 0;
}

static void tDrArea_Delete()
{
    DrArea* a = &DR->area[DR->areaNo];

    a->flag &= ~1;
    DR->cursor = 0;
}

static void tDrArea_Move()
{
    DrArea* a = &DR->area[DR->areaNo];

    if (a->flag & 1) {
        s16 hx;
        s16 hy;

        AreaDataEdit(&a->data, 0x00FF8080, 0, 0, 2.0f);
        AreaDataInfoDisp(&a->data, DR->x, DR->y);
        hx = DR->x + 224;
        hy = DR->y - 32;
        AreaDataHelpDisp(&a->data, hx, hy);
    }
    if (Joy[0].trg & 0x200) {
        DR->step = 1;
        DR->sub = 0;
        DR->x3 = 0;
    }
}

// never called: player position display (strings only)
static inline void tDrPlayerDisp()
{
    eprintf(DR->x, DR->y, 4, 0, "[PLAYER]");
    eprintf(DR->x, DR->y + 16, 0, 0, "X:%.0f", pPL->pos.x);
    eprintf(DR->x, DR->y + 32, 0, 0, "Y:%.0f", pPL->pos.y);
    eprintf(DR->x, DR->y + 48, 0, 0, "Z:%.0f", pPL->pos.z);
    eprintf(DR->x, DR->y + 64, 0, 0, "ANG:%f", pPL->rot.y);
}

char* tDr_getFilename(u8 no, int flag)
{
    char* name = DR->fileName[no];

    if (name != 0) {
        if (flag == 1) {
            char* p = strrchr(name, '/');

            if (p) {
                return p + 1;
            }
            return DR->fileName[no];
        }
        return name;
    }
    return "no file...";
}

static TOOL_MENU drLoadMenu[3] = {
    {1, "SERVER", 0},
    {0, "LOCAL", 0},
    {1, "don't load", 0},
};

static void tDrDataLoad()
{
    s8 ret;
    int ok;
    u32 i;
    u8* data;

    eprintf(DR->x, DR->y, 4, 0, "[DATA LOAD]");
    DR->y += 16;
    switch (DR->step) {
    case 0:
        ret = ToolMenuDisp(DR->x, DR->y, 0, drLoadMenu, sizeof(drLoadMenu), Joy);
        eprintf(DR->x + 90, DR->y, 6, 0, "%s", DR->fileB);
        eprintf(DR->x + 90, DR->y + 16, 6, 0, "%s", DR->fileA);
        if (ret >= 0) {
            ok = 0;
            switch (ret) {
            case 0:
                ok = HDRead(DR->fileB, &DR->hdr);
                break;
            case 1:
                ok = HDRead(DR->fileA, &DR->hdr);
                break;
            case 2:
                DR->mode = 0;
                DR->step = 0;
                DR->sub = 0;
                DR->x3 = 0;
                return;
            }
            DR->timer = 30;
            if (ok) {
                DR->step = 1;
                DR->sub = 0;
                DR->x3 = 0;
            } else {
                DR->step = 9;
                DR->sub = 0;
                DR->x3 = 0;
            }
        }
        if (Joy[0].trg & 0x200) {
            DR->mode = 0;
            DR->step = 0;
            DR->sub = 0;
            DR->x3 = 0;
        }
        break;
    case 1:
        DR->pData = (u8*) &DR->hdr;
        if (strcmp((char*) DR->pData, "DRA") != 0) {
            DR->step = 9;
            DR->sub = 0;
            DR->x3 = 0;
        } else if (((DrHeader*) DR->pData)->version != 0x100) {
            DR->step = 9;
            DR->sub = 0;
            DR->x3 = 0;
        } else {
            memclr_asm(DR->area, sizeof(DR->area));
            memclr_asm(DR->fileName, sizeof(DR->fileName));
            data = DR->pData;
            DR->pArea = (DrArea*) (data + 0x10);
            DR->pStr = (char*) (((DrHeader*) DR->pData)->strOfs + (u32) data);
            for (i = 0; i < ((DrHeader*) DR->pData)->nArea; i++) {
                DrArea* src = (DrArea*) (i * sizeof(DrArea) + (u32) DR->pArea);

                *(DrArea*) (src->no * sizeof(DrArea) + (u32) DR->area) = *src;
            }
            for (i = 0; i < ((DrHeader*) DR->pData)->nFile; i++) {
                if (*DR->pStr != 0) {
                    DR->fileName[i] = DR->pStr;
                    DR->pStr += strlen(DR->pStr);
                }
                DR->pStr++;
            }
            DR->step = 8;
            DR->sub = 0;
            DR->x3 = 0;
        }
        break;
    case 8:
        eprintf(DR->x, DR->y, 6, 0, "DATA LOAD COMPLETE.");
        if ((Joy[0].trg & 0x300) || DR->timer <= 0) {
            DR->mode = 0;
            DR->step = 0;
            DR->sub = 0;
            DR->x3 = 0;
        }
        DR->timer--;
        break;
    case 9:
        eprintf(DR->x, DR->y, 2, 0, "DATA LOAD ERROR.");
        if ((Joy[0].trg & 0x300) || DR->timer <= 0) {
            DR->step = 0;
            DR->sub = 0;
            DR->x3 = 0;
        }
        DR->timer--;
        break;
    }
}

static TOOL_MENU drSaveMenu[3] = {
    {1, "SERVER", 0},
    {0, "LOCAL", 0},
    {1, "don't save", 0},
};

static void tDrDataSave()
{
    int ok = 0;

    eprintf(DR->x, DR->y, 4, 0, "[DATA SAVE]");
    DR->y += 16;
    switch (DR->step) {
    case 0:
        DR->step = 1;
        DR->sub = 0;
        DR->x3 = 0;
    case 1:
        DR->sel = ToolMenuDisp(DR->x, DR->y, 0, drSaveMenu, sizeof(drSaveMenu), Joy);
        eprintf(DR->x + 100, DR->y, 6, 0, "%s", DR->fileB);
        eprintf(DR->x + 100, DR->y + 16, 6, 0, "%s", DR->fileA);
        if (DR->sel >= 0) {
            if (DR->sel == 2) {
                DR->mode = 0;
                DR->step = 0;
                DR->sub = 0;
                DR->x3 = 0;
            } else {
                DR->step = 2;
                DR->sub = 0;
                DR->x3 = 0;
            }
        }
        if (Joy[0].trg & 0x200) {
            DR->mode = 0;
            DR->step = 0;
            DR->sub = 0;
            DR->x3 = 0;
        }
        break;
    case 2:
        tDrSaveDataCreate();
        switch (DR->sel) {
        case 0:
            ok = HDWrite(DR->fileB, &DR->hdr, DR->saveSize);
            break;
        case 1:
            ok = HDWrite(DR->fileA, &DR->hdr, DR->saveSize);
            break;
        }
        DR->timer = 30;
        if (ok) {
            DR->step = 8;
            DR->sub = 0;
            DR->x3 = 0;
        } else {
            DR->step = 9;
            DR->sub = 0;
            DR->x3 = 0;
        }
        break;
    case 8:
        eprintf(DR->x, DR->y, 6, 0, "DATA SAVE COMPLETE.");
        if ((Joy[0].trg & 0x300) || DR->timer <= 0) {
            DR->mode = 0;
            DR->step = 0;
            DR->sub = 0;
            DR->x3 = 0;
        }
        DR->timer--;
        break;
    case 9:
        eprintf(DR->x, DR->y, 2, 0, "DATA SAVE ERROR.");
        if ((Joy[0].trg & 0x300) || DR->timer <= 0) {
            DR->step = 1;
            DR->sub = 0;
            DR->x3 = 0;
        }
        DR->timer--;
        break;
    }
}

void tDrSaveDataCreate()
{
    int nArea = 0;
    int nFile = 0;
    char* sp;
    int i;
    int j;
    u32 len;
    u32 size;

    memclr_asm(DR->strBuf, sizeof(DR->strBuf));
    DR->pArea = DR->hdrArea;
    sp = DR->strBuf;
    for (i = 0; i < 128; i++) {
        if (DR->area[i].flag & 1) {
            char* name;
            char* p;
            int found;

            DR->area[i].no = i;
            *DR->pArea = DR->area[i];
            name = tDr_getFilename(i, 0);
            found = 0;
            p = DR->strBuf;
            for (j = 0; j < nFile; j++) {
                if (strcmp(p, name) == 0) {
                    found = 1;
                    DR->pArea->file = j;
                    break;
                }
                p += strlen(p);
                p++;
            }
            if (!found) {
                strcpy(sp, name);
                sp += strlen(name);
                *sp++ = 0;
                DR->pArea->file = nFile;
                nFile++;
            }
            nArea++;
            DR->pArea++;
        }
    }
    DR->pStr = (char*) DR->pArea;
    len = sp - DR->strBuf;
    if (DR->pStr != DR->strBuf) {
        memcpy(DR->pStr, DR->strBuf, len);
    }
    DR->hdr.id[0] = 'D';
    DR->hdr.id[1] = 'R';
    DR->hdr.id[2] = 'A';
    DR->hdr.id[3] = 0;
    DR->hdr.version = 0x100;
    DR->hdr.nArea = nArea;
    DR->hdr.nFile = nFile;
    size = len + sizeof(DrHeader);
    DR->hdr.strOfs = nArea * sizeof(DrArea) + sizeof(DrHeader);
    DR->saveSize = nArea * sizeof(DrArea) + size;
}

// never called: the tool's main loop (strings only)
static inline void ToolDr()
{
    for (;;) {
        eprintf(280, 14, 0, 0, "CAMERA MODE");
        tDrFunc[DR->mode]();
        TaskSleep(1);
    }
}

asm(".section .data; .balign 8");
