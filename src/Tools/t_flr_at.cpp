#include "types.h"
#include "light.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "camera.h"
#include "db_cam.h"
#include "main_mem.h"
#include "file.h"
#include "dbmodule.h"
#include "db_log.h"
#include "area.h"
#include "flr_at.h"
#include "player.h"
#include "t_util.h"

// Floor attribute (room "FSE" file) editor of the Tools REL. No __FILE__ string: the real name is
// unknown (t_flr_at.cpp by its function prefix). Same skeleton as t_movie's t_se_at.cpp.

extern "C" {
int sprintf(char* buf, const char* fmt, ...);
int strcmp(const char* a, const char* b);
}
int SetToolLight(int no);  // db_light_tools.cpp

// Reference stores (global.h FSet/BitSet): a scalar MEM, so the following global load (pG, pFlrSys)
// is not hoisted above it and is reloaded.
static inline void ISet(int& d, int v) { d = v; }
static inline void PSet(void*& d, void* v) { d = v; }
static inline void ASet(FlrAt*& d, FlrAt* v) { d = v; }
// Reference read: a MEM with neither flag stays below the preceding struct stores (the save counter).
static inline int IGet(int& v) { return v; }

// Tool-side view of the FlrAt record (flr_at.h), 0x84 bytes.
struct TFlrAt {
    u8 flags;        // 0x00  bit 0 enabled, bit 1 created
    u8 type;         // 0x01  0 foot SE, 1 SE volume, 2 BGM volume, 3 thunder volume
    u8 no;           // 0x02  record index (set on save)
    u8 group;        // 0x03
    u8 priority;     // 0x04  save order (15 first)
    u8 pad_5[0x14 - 0x05];
    AreaData area;   // 0x14
    u8 x44;          // 0x44  (0) foot SE, (2) BGM block bits / 0x10 stream, (3) volume
    u8 x45;          // 0x45  (0) foot effect, (2) set-volume bits / 0x10 stream play, (3) sub volume
    u8 x46[2];       // 0x46  (0) cartridge, (2) BGM volume per block; 0x47 the type 0 flag bits
    s32 x48[2];      // 0x48  (2) fade time per block
    u16 str_blk;     // 0x50  (2) stream block
    u16 str_no;      // 0x52  (2) stream number
    s32 str_vol;     // 0x54  (2) stream volume
    u8 pad_58[0x84 - 0x58];
};

struct FlrAtWork {
    u8 mode;          // 0x00  flrAtRoutine index
    u8 sub;           // 0x01  sub routine / step
    u8 step;          // 0x02
    u8 step2;         // 0x03
    int firstLoad;    // 0x04  the initial load of the room file
    u8 camMode;       // 0x08  debug camera active in the area editor
    s8 cursor;        // 0x09  main menu
    s8 subCursor;     // 0x0A  edit type menu
    s8 editCursor;    // 0x0B  area edit menu
    s8 inputCursor;   // 0x0C  data input line
    u8 copySrc;       // 0x0D
    u8 copyValid;     // 0x0E
    s8 stage;         // 0x0F
    s8 room;          // 0x10
    u8 server;        // 0x11  load from x:/ instead of d:/
    s8 editType;      // 0x12  -1 none, 0 SE, 1 BGM
    s8 loadCursor;    // 0x13
    u8 yesNo;         // 0x14
    u8 pad_15;
    s16 areaNo[2];    // 0x16  edited area per edit type
    s16 copyNo[2];    // 0x1A
    s16 timer;        // 0x1E
    s16 curNo;        // 0x20  area index (BGM areas at 0x80)
    s16 x;            // 0x22
    s16 y;            // 0x24
    s16 x0;           // 0x26
    s16 y0;           // 0x28
    u8 pad_2A[2];
    u32 saveStop;     // 0x2C  pG->flags_170
    u32 saveDisp;     // 0x30  pG->flags_58
    int dispGroup;    // 0x34  display filter (-1 = all)
    int dispType;     // 0x38
    char path[0x40];  // 0x3C
    s8 flagCursor;    // 0x7C
    u8 defCartridge;  // 0x7D  FlrAtHead pad_8[0]
    u8 pad_7E[2];
    FlrAtHead head;   // 0x80
    TFlrAt area[256]; // 0x90
    FlrAtHead fileHead;  // 0x8490
    TFlrAt file[256];    // 0x84A0
    TFlrAt copyBuf;      // 0x108A0
    FlrSys flrSys;       // 0x10924
    FlrSys* saveFlrSys;  // 0x109B0
};

static int flrAtSaveNum;
struct FlrAtWorkPtr {
    FlrAtWork* p;
};
static FlrAtWorkPtr flrAtWk;
#define pW (flrAtWk.p)
struct TFlrAtPtr {
    TFlrAt* p;
};
static TFlrAtPtr flrAtCur;
#define pCur (flrAtCur.p)

static const char* flrAtTypeName[4] = {"FOOT SE", "SE VOL CTRL", "BGM VOL CTRL", "THUNDER VOL"};
static int flrAtSeType[3] = {0, 1, 3};
static int flrAtBgmType[1] = {2};
static u32 flrAtTypeCol[4] = {0x80808080, 0x80004040, 0x80505000, 0x80007000};

void flrAtInit();
static void flrAtExit();
static void flrAtMainMenu();
static void flrAtSubMenu();
static void flrAtAreaEdit();
static void flrAtAreaEdit_EditMenu();
static void flrAtAreaEdit_AreaCreate();
static void flrAtAreaEdit_AreaCopy();
static void flrAtAreaEdit_AreaPaste();
static void flrAtAreaEdit_CopyBuffClear();
static void flrAtAreaEdit_AreaDelete();
void flrAtAreaEdit_disp();
static void flrAtAreaEdit_AreaMove();
static void flrAtAreaEdit_DataInput();
void flrAtDataInput_common_menu(int sel);
static void flrAtDataInput_sedata();
static void flrAtDataInput_se_volctrl();
static void flrAtDataInput_bgm_volctrl();
static void flrAtDataInput_thunder_volctrl();
void flrAtPreview_pl_pos();
static void flrAtDataLoad();
static void flrAtDataSave();
void flrAtData_DebugCamera();
static void flrAtPreview();
static void preview_init();
static void preview_main();
static void preview_exit();

void flrAtInit()
{
    int zero = 0;

    TutilInitDefault();
    BitSet(pW->saveStop, TOOL_FLAG(OFS_STOP_FLG));
    TOOL_FLAG(OFS_STOP_FLG) |= 0x20000000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x10000000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x800000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x400000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x10000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x2000;
    BitSet(pW->saveDisp, TOOL_FLAG(OFS_DISP_FLG));
    TOOL_FLAG(OFS_DISP_FLG) |= 0x40000000;
    TOOL_FLAG(OFS_DISP_FLG) |= 0x80000000;
    TOOL_FLAG(OFS_DISP_FLG) |= 0x2000000;
    TOOL_FLAG(OFS_DISP_FLG) |= 0x100000;
    TOOL_FLAG(OFS_DEBUG_FLG) |= 0x10000000;
    SetToolLight(1);
    pW->x0 = 0x2D;
    pW->y0 = 0x1E;
    pW->head.magic[0] = 'F';
    pW->head.magic[1] = 'S';
    pW->head.magic[2] = 'E';
    pW->head.magic[3] = zero;
    pW->head.version = 0x103;
    pW->head.num = 256;
    pW->head.pad_8[0] = zero;
    pW->copySrc = zero;
    pW->copyValid = zero;
    pW->saveFlrSys = pFlrSys;
    pW->dispGroup = -1;
    pW->firstLoad = 1;
    pW->mode = 3;
    pW->sub = 3;
    pW->editType = -1;
}

static void flrAtExit()
{
    TOOL_FLAG(OFS_DISP_FLG) = pW->saveDisp;
    TOOL_FLAG(OFS_STOP_FLG) = pW->saveStop;
    TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x10000000;
    SetToolLight(-1);
    pFlrSys = pW->saveFlrSys;
    TutilQuitDefault();
    TaskExit();
}

static TOOL_MENU flrAtMainMenuTbl[5] = {
    {1, "SE  DATA EDIT", NULL},
    {1, "BGM DATA EDIT", NULL},
    {1, "DATA LOAD", NULL},
    {1, "DATA SAVE", NULL},
    {1, "EXIT", flrAtExit},
};

static void flrAtMainMenu()
{
    s8 sel = ToolMenuDisp_cur(pW->x, pW->y, 1, &pW->cursor, flrAtMainMenuTbl, sizeof(flrAtMainMenuTbl), &Joy[0]);

    pW->dispType = -1;
    pW->dispGroup = -1;
    pW->editType = -1;
    if (sel >= 0) {
        switch (sel) {
        case 0:
            pW->mode = 5;
            pW->editType = sel;
            pW->subCursor = sel;
            break;
        case 1:
            pW->mode = 5;
            pW->editType = sel;
            pW->subCursor = 0;
            break;
        case 2:
            pW->mode = 3;
            break;
        case 3:
            pW->mode = 4;
            break;
        }
        pW->sub = 0;
        pW->step = 0;
        pW->step2 = 0;
    }
}

static TOOL_MENU flrAtSubMenuTbl[3] = {
    {1, "AREA EDIT", NULL},
    {1, "PREVIEW", NULL},
    {1, "DATA LOAD", NULL},
};

static void flrAtSubMenu()
{
    s8 sel = ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->subCursor, flrAtSubMenuTbl, sizeof(flrAtSubMenuTbl), &Joy[0]);

    if (sel >= 0) {
        switch (sel) {
        case 0:
            pW->mode = 1;
            break;
        case 1:
            pW->mode = 2;
            break;
        case 2:
            pW->mode = 3;
            break;
        }
        pW->sub = 0;
        pW->step = 0;
        pW->step2 = 0;
    }
    if (Joy[0].trg & JOY_B) {
        pW->mode = 0;
    }
}

static void flrAtAreaEdit()
{
    void (*routine[4])() = {flrAtAreaEdit_EditMenu, flrAtAreaEdit_AreaMove, flrAtAreaEdit_DataInput,
                            flrAtAreaEdit_AreaCreate};
    JOY* joy = &Joy[0];

    if (joy->trg & JOY_START) pW->camMode ^= 1;
    if (pW->camMode) {
        flrAtData_DebugCamera();
        return;
    }
    if (joy->rep2 & JOY_R) pW->areaNo[pW->editType]++;
    if (joy->rep2 & JOY_L) pW->areaNo[pW->editType]--;
    pW->areaNo[pW->editType] =
        pW->areaNo[pW->editType] < 0 ? 127 : (pW->areaNo[pW->editType] > 127 ? 0 : pW->areaNo[pW->editType]);
    if (pW->editType == 0) {
        pW->curNo = pW->areaNo[0];
    } else {
        pW->curNo = pW->areaNo[1] + 0x80;
    }
    if (pW->copyValid) {
        eprintf(pW->x, pW->y - 0x10, 5, 0, "AREA[ %d ]  ID:%s", pW->copyNo[pW->editType],
                (u32) pW->area[pW->copySrc].type <= 3 ? flrAtTypeName[pW->area[pW->copySrc].type] : "...no string");
    }
    pCur = &pW->area[pW->curNo];
    eprintf(pW->x, pW->y, 4, 0, "AREA[ %d ]", pW->areaNo[pW->editType]);
    if (pCur->flags & 1) {
        eprintf(pW->x + 0x58, pW->y, 0, 0, "ID:");
        eprintf(pW->x + 0x58, pW->y, 6, 0, "   %s", (u32) pCur->type <= 3 ? flrAtTypeName[pCur->type] : "...no string");
    } else {
        eprintf(pW->x + 0x58, pW->y, 2, 0, "NO DATA:");
    }
    pW->x += 8;
    pW->y += 0x20;
    routine[pW->sub]();
}

static TOOL_MENU flrAtCreateMenu[3] = {
    {1, "AREA CREATE", NULL},
    {0, "AREA PASTE", flrAtAreaEdit_AreaPaste},
    {0, "COPY BUFF CLEAR", flrAtAreaEdit_CopyBuffClear},
};

static TOOL_MENU flrAtEditMenu[6] = {
    {1, "AREA MOVE", NULL},
    {1, "DATA INPUT", NULL},
    {1, "AREA COPY", flrAtAreaEdit_AreaCopy},
    {0, "AREA PASTE", flrAtAreaEdit_AreaPaste},
    {0, "COPY BUFF CLEAR", flrAtAreaEdit_CopyBuffClear},
    {1, "AREA DELETE", flrAtAreaEdit_AreaDelete},
};

static void flrAtAreaEdit_EditMenu()
{
    s8 sel;
    u8 valid = pW->copyValid;

    flrAtCreateMenu[1].enable = flrAtCreateMenu[2].enable = flrAtEditMenu[3].enable = flrAtEditMenu[4].enable = valid;
    if (pCur->flags & 1) {
        sel = ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->editCursor, flrAtEditMenu, sizeof(flrAtEditMenu), &Joy[0]);
        switch (sel) {
        case 0:
            pW->sub = 1;
            pW->step = sel;
            pW->step2 = sel;
            break;
        case 1:
            pW->inputCursor = 0;
            pW->sub = 2;
            pW->step = 0;
            pW->step2 = 0;
            pW->flagCursor = 0;
            break;
        }
    } else {
        sel = ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->editCursor, flrAtCreateMenu, sizeof(flrAtCreateMenu), &Joy[0]);
        if (sel == 0) {
            pW->sub = 3;
            pW->step = sel;
            pW->step2 = sel;
            pW->editCursor = sel;
        }
    }
    if (Joy[0].trg & JOY_B) {
        pW->mode = 5;
        pW->sub = 0;
        pW->step = 0;
        pW->step2 = 0;
    }
}

static TOOL_MENU flrAtShapeMenu[2] = {
    {1, "SQUARE", NULL},
    {1, "CIRCLE", NULL},
};

static void flrAtAreaEdit_AreaCreate()
{
    s8 sel = ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->editCursor, flrAtShapeMenu, sizeof(flrAtShapeMenu), &Joy[0]);

    if (sel < 0) return;
    switch (sel) {
    case 0:
        AreaDataInit(&pCur->area, &pPL->pos, AREA_TYPE_XZ4, 1000.0f, 1000.0f);
        break;
    case 1:
        AreaDataInit(&pCur->area, &pPL->pos, AREA_TYPE_CYLINDER, 1000.0f, 1000.0f);
        break;
    }
    pCur->priority = 8;
    pCur->flags |= 3;
    if (pW->editType == 0) {
        pCur->x46[1] = 7;
    } else {
        pCur->type = 2;
    }
    pW->editCursor = 0;
    pW->sub = 0;
    pW->step = 0;
    pW->step2 = 0;
}

static void flrAtAreaEdit_AreaCopy()
{
    pW->copyBuf = *pCur;
    pW->copySrc = pW->curNo;
    pW->copyNo[pW->editType] = pW->areaNo[pW->editType];
    pW->copyValid = 1;
}

static void flrAtAreaEdit_AreaPaste()
{
    *pCur = pW->copyBuf;
}

static void flrAtAreaEdit_CopyBuffClear()
{
    memclr_asm(&pW->copyBuf, sizeof(TFlrAt));
    pW->copySrc = 0;
    pW->copyValid = 0;
}

static void flrAtAreaEdit_AreaDelete()
{
    pCur->flags &= ~1;
    pW->editCursor = 0;
}

void flrAtAreaEdit_disp()
{
    int i;
    u32 col;

    for (i = 0; i < 256; i++) {
        if ((pW->area[i].flags & 1) == 0) continue;  // (`!(x & 1)` here compiles to xori)
        if (pW->editType == -1) {
            if (pW->dispType != -1 && pW->dispType != pW->area[i].type) continue;
            if (pW->dispGroup != -1 && pW->dispGroup != pW->area[i].group) continue;
            AreaDataDisp(&pW->area[i].area, flrAtTypeCol[pW->area[i].type], 1, NULL);
        } else {
            switch (pW->area[i].type) {
            case 0:
            case 1:
            case 3:
                if (pW->editType != 0) break;
                if (pW->mode != 2) {
                    if (pW->curNo == i) {
                        col = 0x80F08080;
                    } else {
                        col = flrAtTypeCol[pW->area[i].type];
                    }
                } else if (AreaHitCheck(&pW->area[i].area, &pPL->pos) == 1) {
                    col = 0x80F08080;
                } else {
                    col = flrAtTypeCol[pW->area[i].type];
                }
                AreaDataDisp(&pW->area[i].area, col, 1, NULL);
                break;
            case 2:
                if (pW->editType != 1) break;
                if (pW->mode != 2) {
                    if (pW->curNo == i) {
                        col = 0x80F08080;
                    } else {
                        col = flrAtTypeCol[2];
                    }
                } else if (AreaHitCheck(&pW->area[i].area, &pPL->pos) == 1) {
                    col = 0x80F08080;
                } else {
                    col = flrAtTypeCol[pW->area[i].type];
                }
                AreaDataDisp(&pW->area[i].area, col, 1, NULL);
                break;
            }
        }
    }
    flrAtPreview_pl_pos();
    eprintf(0x1AE, 0x24, 0, 0, "[PLAYER]");
    eprintf(0x1AE, 0x34, 0, 0, "X:%.0f", pPL->pos.x);
    eprintf(0x1AE, 0x44, 0, 0, "Y:%.0f", pPL->pos.y);
    eprintf(0x1AE, 0x54, 0, 0, "Z:%.0f", pPL->pos.z);
    eprintf(0x1AE, 0x64, 0, 0, "A:%f", pPL->rot.y);
}

static void flrAtAreaEdit_AreaMove()
{
    if (pCur->flags & 1) {
        AreaDataEdit(&pCur->area, 0x80F08080, 0, NULL, 1.0f);
        AreaDataInfoDisp(&pCur->area, pW->x, pW->y);
        AreaDataHelpDisp(&pCur->area, (s16) (pW->x + 0xE0), (s16) (pW->y - 0x20));
    }
    if (Joy[0].trg & JOY_B) {
        pW->sub = 0;
        pW->step = 0;
        pW->step2 = 0;
    }
}

static void (*flrAtInputRoutine[4])() = {flrAtDataInput_sedata, flrAtDataInput_se_volctrl, flrAtDataInput_bgm_volctrl,
                                          flrAtDataInput_thunder_volctrl};

static void flrAtAreaEdit_DataInput()
{
    if (pCur->flags & 1) {
        flrAtInputRoutine[pCur->type]();
    }
    if (Joy[0].trg & JOY_B) {
        pW->sub = 0;
        pW->step = 0;
        pW->step2 = 0;
    }
}

// pad masks of the +/- inputs (the sub stick bits are the header's SLEFT/SRIGHT swapped)
#define REP_RIGHT (JOY_RIGHT | 0x20000)
#define REP_LEFT (JOY_LEFT | 0x10000)
#define REP_UP (JOY_UP | JOY_SUP)
#define REP_DOWN (JOY_DOWN | JOY_SDOWN)
#define REP_LR (REP_RIGHT | REP_LEFT)

// +-1 / +-10 (with A) on a value driven by the fast auto-repeat
#define FLRAT_STEP(v)                                    \
    {                                                    \
        int step = (Joy[0].on & JOY_A) ? 10 : 1;         \
        if (Joy[0].rep2 & REP_RIGHT) v += step;          \
        if (Joy[0].rep2 & REP_LEFT) v -= step;           \
    }

// ID / GROUP NO / PRIORITY lines shared by the four input menus (the type table bounds are size_t:
// the index compares are unsigned)
#define SE_TYPE_NUM (sizeof(flrAtSeType) / sizeof(flrAtSeType[0]))
#define SE_TYPE_MAX (SE_TYPE_NUM - 1)
#define BGM_TYPE_NUM (sizeof(flrAtBgmType) / sizeof(flrAtBgmType[0]))
#define BGM_TYPE_MAX (BGM_TYPE_NUM - 1)
void flrAtDataInput_common_menu(int sel)
{
    int i;
    int n;
    s16 x;
    s16 y;

    switch (sel) {
    case 0:
        if (pW->editType == 0) {
            for (i = 0; i < SE_TYPE_NUM; i++) {
                if (pCur->type == flrAtSeType[i]) break;
            }
        } else {
            for (i = 0; i < BGM_TYPE_NUM; i++) {
                if (pCur->type == flrAtBgmType[i]) break;
            }
        }
        if (Joy[0].rep & REP_RIGHT) i++;
        if (Joy[0].rep & REP_LEFT) i--;
        if (pW->editType == 0) {
            n = i < 0 ? 0 : (i > SE_TYPE_MAX ? SE_TYPE_MAX : i);
            pCur->type = flrAtSeType[n];
        } else {
            n = i < 0 ? 0 : (i > BGM_TYPE_MAX ? BGM_TYPE_MAX : i);
            pCur->type = flrAtBgmType[n];
        }
        break;
    case 1:
        i = pCur->group;
        if (Joy[0].rep & REP_RIGHT) i++;
        if (Joy[0].rep & REP_LEFT) i--;
        i = i < 0 ? 0 : (i > 0x3F ? 0x3F : i);
        pCur->group = i;
        break;
    case 2:
        i = pCur->priority;
        if (Joy[0].rep & REP_RIGHT) i++;
        if (Joy[0].rep & REP_LEFT) i--;
        i = i < 0 ? 0 : (i > 0xF ? 0xF : i);
        pCur->priority = i;
        break;
    }
    x = pW->x + 0x80;
    y = pW->y;
    eprintf(x, y, 0, 0, "%s", (u32) pCur->type <= 3 ? flrAtTypeName[pCur->type] : "...no string");
    y += 0x10;
    eprintf(x, y, 0, 0, "%d", pCur->group);
    y += 0x10;
    eprintf(x, y, 0, 0, "%d", pCur->priority);
    pW->y = y;
}

static TOOL_MENU flrAtSeMenu[8] = {
    {1, "ID", NULL},
    {1, "GROUP NO", NULL},
    {1, "PRIORITY", NULL},
    {1, "FLAG", NULL},
    {1, "SE TYPE", NULL},
    {1, "EFF TYPE", NULL},
    {1, "CARTRIDGE TYPE", NULL},
    {1, "DEF CARTRIDGE", NULL},
};
static const char* flrAtSeFlagName[8] = {"PL FOOT", "EM FOOT", "CARTRIDGE", "", "", "", "", ""};

static void flrAtDataInput_sedata()
{
    s16 x;
    s16 y;
    s8 i;
    int n;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, flrAtSeMenu, sizeof(flrAtSeMenu), &Joy[0]);
    flrAtDataInput_common_menu(pW->inputCursor);
    x = pW->x + 0x80;
    y = pW->y;
    switch (pW->inputCursor) {
    case 3:
        y += 0x10;
        if (Joy[0].rep & REP_RIGHT) pW->flagCursor--;
        if (Joy[0].rep & REP_LEFT) pW->flagCursor++;
        pW->flagCursor = pW->flagCursor < 0 ? 0 : (pW->flagCursor > 7 ? 7 : pW->flagCursor);
        for (i = 0; i < 8; i++) {
            eprintf(x + i * 8, y, (pW->flagCursor == 7 - i) ? 4 : 0, 0, "%d", (pCur->x46[1] >> (7 - i)) & 1);
        }
        eprintf(x + 0x58, y, 4, 0, "%s", flrAtSeFlagName[pW->flagCursor]);
        if (Joy[0].trg & JOY_A) {
            pCur->x46[1] ^= 1 << pW->flagCursor;
        }
        break;
    case 4:
        n = pCur->x44;
        if (Joy[0].rep & REP_RIGHT) n++;
        if (Joy[0].rep & REP_LEFT) n--;
        n = n < 0 ? 0 : (n > 0x32 ? 0x32 : n);
        pCur->x44 = n;
        break;
    case 5:
        n = pCur->x45;
        if (Joy[0].rep & REP_RIGHT) n++;
        if (Joy[0].rep & REP_LEFT) n--;
        n = n < 0 ? 0 : (n > 0xA ? 0xA : n);
        pCur->x45 = n;
        break;
    case 6:
        n = pCur->x46[0];
        if (Joy[0].rep & REP_RIGHT) n++;
        if (Joy[0].rep & REP_LEFT) n--;
        n = n < 0 ? 0 : (n > 0xA ? 0xA : n);
        pCur->x46[0] = n;
        break;
    case 7:
        n = pW->defCartridge;
        if (Joy[0].rep & REP_RIGHT) n++;
        if (Joy[0].rep & REP_LEFT) n--;
        n = n < 0 ? 0 : (n > 0x64 ? 0x64 : n);
        pW->defCartridge = n;
        break;
    }
    if (pW->inputCursor != 3) {
        y += 0x10;
        for (i = 0; i < 8; i++) {
            eprintf(x + i * 8, y, 0, 0, "%d", (pCur->x46[1] >> (7 - i)) & 1);
        }
    }
    y += 0x10;
    eprintf(x, y, 0, 0, "%d", pCur->x44);
    y += 0x10;
    eprintf(x, y, 0, 0, "%d", pCur->x45);
    y += 0x10;
    eprintf(x, y, 0, 0, "%d", pCur->x46[0]);
    y += 0x10;
    eprintf(x, y, 0, 0, "%d", pW->defCartridge);
}

static TOOL_MENU flrAtVolMenu[3] = {
    {1, "ID", NULL},
    {1, "GROUP NO", NULL},
    {1, "PRIORITY", NULL},
};

static void flrAtDataInput_se_volctrl()
{
    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, flrAtVolMenu, sizeof(flrAtVolMenu), &Joy[0]);
    flrAtDataInput_common_menu(pW->inputCursor);
}

static TOOL_MENU flrAtBgmMenu[16] = {
    {1, "ID", NULL},
    {1, "GROUP NO", NULL},
    {1, "PRIORITY", NULL},
    {1, "BLK1 ON/OFF", NULL},
    {1, " CHANGE TIME", NULL},
    {1, " VOL SET/RESET", NULL},
    {1, "  SET VOL", NULL},
    {1, "BLK2 ON/OFF", NULL},
    {1, " CHANGE TIME", NULL},
    {1, " VOL SET/RESET", NULL},
    {1, "  SET VOL", NULL},
    {1, "STR ON/OFF", NULL},
    {1, " PLAY/STOP", NULL},
    {1, " BLK", NULL},
    {1, " NO", NULL},
    {1, " FADE TIME", NULL},
};

static void flrAtDataInput_bgm_volctrl()
{
    s16 x;
    s16 y;
    int i;
    int v;
    int col;

    if (pCur->x44 & 1) {
        flrAtBgmMenu[4].enable = 1;
        flrAtBgmMenu[5].enable = 1;
        if (pCur->x45 & 1) {
            flrAtBgmMenu[6].enable = 1;
        } else {
            flrAtBgmMenu[6].enable = 0;
        }
    } else {
        flrAtBgmMenu[4].enable = 0;
        flrAtBgmMenu[5].enable = 0;
        flrAtBgmMenu[6].enable = 0;
    }
    if (pCur->x44 & 2) {
        flrAtBgmMenu[8].enable = 1;
        flrAtBgmMenu[9].enable = 1;
        if (pCur->x45 & 2) {
            flrAtBgmMenu[10].enable = 1;
        } else {
            flrAtBgmMenu[10].enable = 0;
        }
    } else {
        flrAtBgmMenu[8].enable = 0;
        flrAtBgmMenu[9].enable = 0;
        flrAtBgmMenu[10].enable = 0;
    }
    if (pCur->x44 & 0x10) {
        flrAtBgmMenu[12].enable = 1;
        flrAtBgmMenu[13].enable = 1;
        flrAtBgmMenu[14].enable = 1;
        flrAtBgmMenu[15].enable = 1;
    } else {
        flrAtBgmMenu[12].enable = 0;
        flrAtBgmMenu[13].enable = 0;
        flrAtBgmMenu[14].enable = 0;
        flrAtBgmMenu[15].enable = 0;
    }
    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, flrAtBgmMenu, sizeof(flrAtBgmMenu), &Joy[0]);
    flrAtDataInput_common_menu(pW->inputCursor);
    switch (pW->inputCursor) {
    case 3:
        if (Joy[0].trg & REP_LR) pCur->x44 ^= 1;
        break;
    case 4:
        if (pCur->x44 & 1) {
            v = pCur->x48[0];
            FLRAT_STEP(v);
            v = v < 0 ? 0 : (v > 50000 ? 50000 : v);
            pCur->x48[0] = v;
        }
        break;
    case 5:
        if ((pCur->x44 & 1) && (Joy[0].trg & REP_LR)) pCur->x45 ^= 1;
        break;
    case 6:
        if ((pCur->x44 & 1) && (pCur->x45 & 1)) {
            v = (s8) pCur->x46[0];
            FLRAT_STEP(v);
            v = v < 0 ? 0 : (v > 0x7F ? 0x7F : v);
            pCur->x46[0] = v;
        }
        break;
    case 7:
        if (Joy[0].trg & REP_LR) pCur->x44 ^= 2;
        break;
    case 8:
        if (pCur->x44 & 2) {
            v = pCur->x48[1];
            FLRAT_STEP(v);
            v = v < 0 ? 0 : (v > 50000 ? 50000 : v);
            pCur->x48[1] = v;
        }
        break;
    case 9:
        if ((pCur->x44 & 2) && (Joy[0].trg & REP_LR)) pCur->x45 ^= 2;
        break;
    case 10:
        // (separate tests: `&&` folds the two byte tests into one halfword andis.)
        if (pCur->x44 & 2) {
            if (pCur->x45 & 2) {
                v = (s8) pCur->x46[1];
                FLRAT_STEP(v);
                v = v < 0 ? 0 : (v > 0x7F ? 0x7F : v);
                pCur->x46[1] = v;
            }
        }
        break;
    case 11:
        if (Joy[0].trg & REP_LR) pCur->x44 ^= 0x10;
        break;
    case 12:
        if ((pCur->x44 & 0x10) && (Joy[0].trg & REP_LR)) pCur->x45 ^= 0x10;
        break;
    case 13:
        if (pCur->x44 & 0x10) {
            if (Joy[0].rep2 & REP_RIGHT) pCur->str_blk = 1;
            if (Joy[0].rep2 & REP_LEFT) pCur->str_blk = 0;
        }
        break;
    case 14:
        if (pCur->x44 & 0x10) {
            v = pCur->str_no;
            FLRAT_STEP(v);
            v = v < 0 ? 0 : (v > 0x200 ? 0x200 : v);
            pCur->str_no = v;
        }
        break;
    case 15:
        if (pCur->x44 & 0x10) {
            v = pCur->str_vol;
            FLRAT_STEP(v);
            v = v < 0 ? 0 : (v > 50000 ? 50000 : v);
            pCur->str_vol = v;
        }
        break;
    }
    x = pW->x + 0x80;
    y = pW->y + 0x10;
    for (i = 0; i < 2; i++) {
        eprintf(x, y, 0, 0, "%s", ((pCur->x44 >> i) & 1) ? "ON" : "OFF");
        y += 0x10;
        col = 0x14;
        if ((pCur->x44 >> i) & 1) col = 0;
        eprintf(x, y, col, 0, "%d", pCur->x48[i]);
        y += 0x10;
        eprintf(x, y, col, 0, "%s", ((pCur->x45 >> i) & 1) ? "SET" : "RESET");
        y += 0x10;
        if (col == 0) {
            col = 0x14;
            if ((pCur->x45 >> i) & 1) col = 0;
        }
        eprintf(x, y, col, 0, "%d", (s8) pCur->x46[i]);
        y += 0x10;
    }
    eprintf(x, y, 0, 0, "%s", (pCur->x44 & 0x10) ? "ON" : "OFF");
    y += 0x10;
    col = 0x14;
    if (pCur->x44 & 0x10) col = 0;
    eprintf(x, y, col, 0, "%s", (pCur->x45 & 0x10) ? "PLAY" : "STOP");
    y += 0x10;
    eprintf(x, y, col, 0, "%s", pCur->str_blk == 0 ? "BGM" : "VOICE");
    y += 0x10;
    eprintf(x, y, col, 0, "%d", pCur->str_no);
    y += 0x10;
    eprintf(x, y, col, 0, "%d", pCur->str_vol);
}

static TOOL_MENU flrAtThunderMenu[5] = {
    {1, "ID", NULL},
    {1, "GROUP NO", NULL},
    {1, "PRIORITY", NULL},
    {1, "SET VOL", NULL},
    {1, "SET SVOL", NULL},
};

static void flrAtDataInput_thunder_volctrl()
{
    s16 x;
    s16 y;
    int v;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, flrAtThunderMenu, sizeof(flrAtThunderMenu), &Joy[0]);
    flrAtDataInput_common_menu(pW->inputCursor);
    switch (pW->inputCursor) {
    case 3:
        v = (s8) pCur->x44;
        FLRAT_STEP(v);
        v = v < 0 ? 0 : (v > 0x7F ? 0x7F : v);
        pCur->x44 = v;
        break;
    case 4:
        v = (s8) pCur->x45;
        FLRAT_STEP(v);
        v = v < 0 ? 0 : (v > 0x7F ? 0x7F : v);
        pCur->x45 = v;
        break;
    }
    x = pW->x + 0x80;
    y = pW->y + 0x10;
    eprintf(x, y, 0, 0, "%d", (s8) pCur->x44);
    eprintf(x, y + 0x10, 0, 0, "%d", (s8) pCur->x45);
}

// marks the player position, lit when it is inside an area of the edited kind
void flrAtPreview_pl_pos()
{
    Vec pos;
    int i;
    int hit;

    pos.x = pPL->pos.x;
    pos.y = pPL->pos.y + 300.0f;
    pos.z = pPL->pos.z;
    for (i = 0; i < 256; i++) {
        hit = 0;
        if (pW->area[i].flags & 1) {
            if (pW->editType == 0) {
                switch (pW->area[i].type) {
                case 0:
                case 1:
                    hit = AreaHitCheck(&pW->area[i].area, &pos) == 1;
                    break;
                }
            } else {
                if (pW->area[i].type == 2) {
                    if (AreaHitCheck(&pW->area[i].area, &pos) == 1) hit = 1;
                }
            }
        }
        if (hit) break;
    }
    if (hit) {
        Draw_sphere(&pos, 150.0f, 0xFF404080, 0, 0);
    } else {
        Draw_sphere(&pos, 150.0f, 0xFFFFFF80, 0, 0);
    }
}

// COMPILER-DIFF: 2 (the original masks the first footer colour once after its arms, `clrlwi r5,r5,24`)
static inline u8 flrAtColU8(int c)
{
    asm volatile("" : "+r"(c));
    return c;
}

static void flrAtDataLoad()
{
    const char* title[3] = {"[ALL DATA LOAD]", "[SE DATA LOAD]", "[BGM DATA LOAD]"};
    u16 roomId = (pW->stage << 8) | pW->room;
    int ret;
    u32 i;
    u32 j;

    eprintf(pW->x, pW->y, 4, 0, "%s", title[pW->editType + 1]);
    switch (pW->sub) {
    case 0:
        pW->sub = 1;
        pW->step = 0;
        pW->step2 = 0;
        pW->stage = pG->stage_no;
        pW->room = pG->room_no;
        pW->server = 1;
    case 1:
        if (Joy[0].rep2 & REP_UP) {
            switch (pW->loadCursor) {
            case 0:
                pW->server ^= 1;
                break;
            case 1:
                pW->stage++;
                break;
            case 2:
                pW->room++;
                break;
            }
        } else if (Joy[0].rep2 & REP_DOWN) {
            switch (pW->loadCursor) {
            case 0:
                pW->server ^= 1;
                break;
            case 1:
                pW->stage--;
                break;
            case 2:
                pW->room--;
                break;
            }
        } else if (Joy[0].trg & REP_LEFT) {
            pW->loadCursor--;
        } else if (Joy[0].trg & REP_RIGHT) {
            pW->loadCursor++;
        } else if (Joy[0].trg & JOY_B) {
            // (both arms spelled out: the tails are cross-jumped)
            if (pW->editType == -1) {
                pW->mode = 0;
                pW->sub = 0;
                pW->step = 0;
                pW->step2 = 0;
            } else {
                pW->mode = 5;
                pW->sub = 0;
                pW->step = 0;
                pW->step2 = 0;
            }
        } else if (Joy[0].trg & JOY_A) {
            pW->sub = 2;
            pW->step = 0;
            pW->step2 = 0;
            pW->yesNo = 1;
        }
        pW->loadCursor = pW->loadCursor < 0 ? 0 : (pW->loadCursor > 3 ? 3 : pW->loadCursor);
        pW->stage = pW->stage < 0 ? 0 : (pW->stage > 9 ? 9 : pW->stage);
        pW->room = pW->room < 0 ? 0 : (pW->room > 0x70 ? 0x70 : pW->room);
        if (pW->server == 0) {
            sprintf(pW->path, "d:/bio4/room/st%1x/r%03x/r%03x.fse", pW->stage, roomId, roomId);
        } else {
            sprintf(pW->path, "x:/soft/room/st%1x/r%03x/r%03x.fse", pW->stage, roomId, roomId);
        }
        break;
    case 2:
        eprintf(pW->x, pW->y + 0x60, 0, 0, "DATA LOAD OK?");
        eprintf(pW->x, pW->y + 0x70, pW->yesNo == 0 ? 6 : 7, 0, "YES");
        eprintf(pW->x + 0x28, pW->y + 0x70, pW->yesNo == 1 ? 6 : 7, 0, "NO");
        if (Joy[0].trg & JOY_B) {
            pW->sub = 1;
            pW->step = 0;
            pW->step2 = 0;
        } else if (Joy[0].trg & JOY_A) {
            if (pW->yesNo == 0) {
                pW->sub = 3;
                pW->step = 0;
                pW->step2 = 0;
            } else {
                pW->sub = 1;
                pW->step = 0;
                pW->step2 = 0;
            }
        } else if (Joy[0].trg & REP_LR) {
            pW->yesNo ^= 1;
        }
        break;
    case 3:
        if (pW->firstLoad == 1) {
            sprintf(pW->path, "x:/soft/room/st%1x/r%03x/r%03x.fse", pGS->stage_no, pGS->room_id, pGS->room_id);
        }
        ret = HDRead(pW->path, &pW->fileHead);
        if (ret == 0) {
            pLog->err(0, 0, "%s : LOAD ERROR !!!!", pW->path);
            pW->sub = 9;
            pW->step = ret;
            pW->step2 = ret;
        } else {
            if (strcmp(pW->fileHead.magic, "FSE") != 0) {
                pW->sub = 9;
                pW->step = 0;
                pW->step2 = 0;
                pW->timer = 30;
                break;
            }
            switch (pW->editType) {
            case -1:
                memclr_asm(pW->area, sizeof(pW->area));
                break;
            case 0:
                memclr_asm(pW->area, sizeof(pW->area) / 2);
                break;
            case 1:
                memclr_asm(&pW->area[0x80], sizeof(pW->area) / 2);
                break;
            }
            for (i = 0; i < pW->fileHead.num; i++) {
                switch (pW->editType) {
                case -1:
                    pW->area[pW->file[i].no] = pW->file[i];
                    pW->defCartridge = pW->fileHead.pad_8[0];
                    if (pW->area[i].type == 2) pW->area[i].type = 2;
                    break;
                case 0:
                    switch (pW->file[i].type) {
                    case 0:
                    case 1:
                        pW->area[pW->file[i].no] = pW->file[i];
                        break;
                    }
                    break;
                case 1:
                    if (pW->file[i].type == 2) {
                        pW->area[pW->file[i].no] = pW->file[i];
                    }
                    break;
                }
            }
            // BGM areas saved in the SE half move to the BGM half
            for (i = 0; i < 0x80; i++) {
                if (pW->area[i].type == 2) {
                    for (j = 0x80; j < 0x100; j++) {
                        if (pW->area[j].flags == 0) {
                            pW->area[j] = pW->area[i];
                            pW->area[j].no = j;
                            memclr_asm(&pW->area[i], sizeof(TFlrAt));
                            break;
                        }
                    }
                }
            }
            pW->sub = 8;
            pW->step = 0;
            pW->step2 = 0;
        }
        pW->timer = 30;
        break;
    case 8:
        if (pW->firstLoad == 1) {
            pW->firstLoad = 0;
            pW->mode = 0;
            pW->sub = 0;
            pW->step = 0;
            pW->step2 = 0;
            break;
        }
        eprintf(pW->x, pW->y + 0x60, 6, 0, "DATA LOAD COMPLETE.");
        if ((Joy[0].trg & (JOY_A | JOY_B)) || pW->timer <= 0) {
            if (pW->editType == -1) {
                pW->mode = 0;
                pW->sub = 0;
                pW->step = 0;
                pW->step2 = 0;
            } else {
                pW->mode = 5;
                pW->sub = 0;
                pW->step = 0;
                pW->step2 = 0;
            }
        }
        pW->timer--;
        break;
    case 9:
        if (pW->firstLoad == 1) {
            pW->firstLoad = 0;
            pW->mode = 0;
            pW->sub = 0;
            pW->step = 0;
            pW->step2 = 0;
            break;
        }
        eprintf(pW->x, pW->y + 0x60, 2, 0, "DATA LOAD ERROR.");
        if ((Joy[0].trg & (JOY_A | JOY_B)) || pW->timer <= 0) {
            pW->sub = 1;
            pW->step = 0;
            pW->step2 = 0;
        }
        pW->timer--;
        break;
    }
    eprintf(pW->x, pW->y + 0x20, flrAtColU8(pW->sub == 1 ? (pW->loadCursor == 0 ? 6 : 0) : 0), 0, "%s", pW->server == 0 ? "LOCAL" : "SERVER");
    eprintf(pW->x + 0x40, pW->y + 0x20, pW->sub == 1 ? (pW->loadCursor == 1 ? 6 : 0) : 0, 0, "STAGE %2d", pW->stage);
    eprintf(pW->x + 0x90, pW->y + 0x20, pW->sub == 1 ? (pW->loadCursor == 2 ? 6 : 0) : 0, 0, "ROOM %02x", pW->room);
    eprintf(pW->x, pW->y + 0x40, 0, 0, "%s", pW->path);
}

static TOOL_MENU flrAtSaveMenu[3] = {
    {1, "SERVER", NULL},
    {1, "LOCAL", NULL},
    {1, "DON'T SAVE", NULL},
};

static void flrAtDataSave()
{
    char pathX[0x40];
    char pathD[0x40];
    int ret = 0;
    int i;
    int j;
    s8 sel;

    eprintf(pW->x, pW->y, 4, 0, "[DATA SAVE]");
    pW->y += 0x10;
    sprintf(pathX, "x:\\soft\\room\\st%1x\\r%03x\\r%03x.fse", pGS->stage_no, pGS->room_id, pGS->room_id);
    sprintf(pathD, "d:\\bio4\\room\\st%1x\\r%03x\\r%03x.fse", pGS->stage_no, pGS->room_id, pGS->room_id);
    switch (pW->sub) {
    case 0:
        flrAtSaveNum = 0;
        for (j = 0; j < 16; j++) {
            for (i = 0; i < 256; i++) {
                if (pW->area[i].flags & 1) {
                    if (pW->area[i].priority == 15 - j) {
                        pW->area[i].no = i;
                        pW->file[flrAtSaveNum] = pW->area[i];
                        ISet(flrAtSaveNum, IGet(flrAtSaveNum) + 1);
                    }
                }
            }
        }
        pW->fileHead.magic[0] = 'F';
        pW->fileHead.magic[1] = 'S';
        pW->fileHead.magic[2] = 'E';
        pW->fileHead.magic[3] = 0;
        pW->fileHead.version = 0x103;
        pW->fileHead.num = *(u16*) ((u8*) &flrAtSaveNum + 2);
        pW->fileHead.pad_8[0] = pW->defCartridge;
        pW->sub = 1;
        pW->step = 0;
        pW->step2 = 0;
    case 1:
        sel = ToolMenuDisp(pW->x, pW->y, 0, flrAtSaveMenu, sizeof(flrAtSaveMenu), &Joy[0]);
        eprintf(pW->x + 0x64, pW->y, 6, 0, "%s", pathX);
        eprintf(pW->x + 0x64, pW->y + 0x10, 6, 0, "%s", pathD);
        if (sel >= 0) {
            switch (sel) {
            case 0:
            case 1:
                ret = HDWrite_only(pathX + sel * 0x40, &pW->fileHead, flrAtSaveNum * sizeof(TFlrAt) + 0x10);
                break;
            case 2:
                pW->mode = 0;
                pW->sub = 0;
                pW->step = 0;
                pW->step2 = 0;
                return;
            }
            pW->timer = 30;
            if (ret != 0) {
                pW->sub = 8;
                pW->step = 0;
                pW->step2 = 0;
            } else {
                pW->sub = 9;
                pW->step = ret;
                pW->step2 = ret;
            }
        }
        if (Joy[0].trg & JOY_B) {
            pW->mode = 0;
            pW->sub = 0;
            pW->step = 0;
            pW->step2 = 0;
        }
        break;
    case 8:
        eprintf(pW->x, pW->y, 6, 0, "DATA SAVE COMPLETE.");
        if ((Joy[0].trg & (JOY_A | JOY_B)) || pW->timer <= 0) {
            pW->mode = ret;
            pW->sub = ret;
            pW->step = ret;
            pW->step2 = ret;
        }
        pW->timer--;
        break;
    case 9:
        eprintf(pW->x, pW->y, 2, 0, "DATA SAVE ERROR.");
        if ((Joy[0].trg & (JOY_A | JOY_B)) || pW->timer <= 0) {
            pW->sub = 1;
            pW->step = ret;
            pW->step2 = ret;
        }
        pW->timer--;
        break;
    }
}

void flrAtData_DebugCamera()
{
    CamDbg.move(&pG->Cam, &Joy[0], 0);
    pW->timer++;
    if (pW->timer & 8) {
        eprintf(0xD0, 0x10, 0, 0, "CAMERA MODE");
    }
}

static void (*flrAtPreviewRoutine[3])() = {preview_init, preview_main, preview_exit};

static void flrAtPreview()
{
    flrAtPreviewRoutine[pW->sub]();
}

static void preview_init()
{
    ISet(pW->dispType, -1);
    TOOL_FLAG(OFS_STOP_FLG) &= ~0x10000000;
    TOOL_FLAG(OFS_DISP_FLG) &= ~0x40000000;
    TOOL_FLAG(OFS_DISP_FLG) &= ~0x80000000;
    TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x10000000;
    pFlrSys = &pW->flrSys;
    PSet(pFlrSys->pData, &pW->head);
    ASet(pFlrSys->pList, (FlrAt*) pW->area);
    pFlrSys->group = 0xFF;
    pW->sub = 1;
    pW->step = 0;
    pW->step2 = 0;
}

static void preview_main()
{
    pW->timer++;
    if (pW->timer & 8) {
        eprintf(0xD0, 0x10, 6, 0, "PREVIEW MODE");
    }
    if (Joy[0].trg & JOY_START) {
        pW->sub = 2;
        pW->step = 0;
        pW->step2 = 0;
    }
}

static void preview_exit()
{
    TOOL_FLAG(OFS_STOP_FLG) |= 0x20000000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x10000000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x800000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x400000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x10000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x2000;
    TOOL_FLAG(OFS_DISP_FLG) |= 0x40000000;
    TOOL_FLAG(OFS_DISP_FLG) |= 0x80000000;
    TOOL_FLAG(OFS_DEBUG_FLG) |= 0x10000000;
    pW->dispGroup = -1;
    pW->mode = 5;
    pW->sub = 0;
    pW->step = 0;
    pW->step2 = 0;
}

static void (*flrAtRoutine[6])() = {flrAtMainMenu, flrAtAreaEdit, flrAtPreview, flrAtDataLoad, flrAtDataSave,
                                     flrAtSubMenu};
static const char* flrAtTitleName[3] = {"SE DATA EDIT", "BGM DATA EDIT", NULL};

void ToolFlrAt()
{
    FlrAtWork*& wp = flrAtWk.p;

    wp = (FlrAtWork*) Debug_alloc(sizeof(FlrAtWork), 1);
    flrAtInit();
    while (1) {
        pW->x = pW->x0;
        pW->y = pW->y0;
        eprintf(pW->x, pW->y, 5, 0, "FLOOR ATARI EDIT TOOL");
        pW->y += 0x10;
        if (pW->editType >= 0) {
            eprintf(pW->x, pW->y, 6, 0, "%s", *(const char**) (pW->editType * 4 + (u32) flrAtTitleName));
        }
        flrAtAreaEdit_disp();
        pW->y += 0x20;
        pW->x += 8;
        flrAtRoutine[pW->mode]();
        TaskSleep(1);
    }
}
