#include "types.h"
#include "light.h"
#include "atari.h"
#include "dmg.h"
#include "map_obj.h"
#include "widget.h"
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
#include "sce_at.h"
#include "player.h"
#include "math_sub.h"
#include "room_jmp.h"
#include "pad.h"
#include "debug.h"
#include "t_util.h"

// Scenario attribute ("AEV" room file) editor: the same object in the Tools and t_sce RELs
// (D:/Bio4/Prog/t_sce_at.cpp). Same skeleton as t_flr_at.cpp / t_movie's t_se_at.cpp.

extern "C" {
int sprintf(char* buf, const char* fmt, ...);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, unsigned int n);
char* strstr(const char* s, const char* sub);
char* strpbrk(const char* s, const char* set);
void* memset(void* dst, int c, unsigned int n);
}
int SetToolLight(int no);  // db_light_tools.cpp / t_sce's db_light_v2 copy

// AEV file: header + records (game/sce_at.cpp SceAtFileHead).
struct TSceAtFileHead {
    char magic[4];    // "AEV"
    u16 version;      // 0x04  0x104
    u16 num;          // 0x06
    u8 pad_8[8];
};

struct TSceAtFile {
    TSceAtFileHead head;   // 0x00
    SceAtWork work[128];   // 0x10
};

// Tool views of the type payloads sce_at.h leaves unnamed.
struct TSceAtLadder {
    Vec pos;          // 0x00
    f32 angle;        // 0x0C
    s8 level;         // 0x10
    u8 pad_11;
    u8 posSet;        // 0x12
    u8 cut1;          // 0x13
    u8 cut2;          // 0x14
    u8 cut3;          // 0x15
};

struct AreaXZ4Pts {
    AreaXZ p[4];
};

struct TSceAtHide {
    u8 mode;          // 0x00
    u8 posSet;        // 0x01
    u8 areaSet;       // 0x02
    u8 step;          // 0x03
    AreaXZ4Pts pts;   // 0x04
    Vec pos;          // 0x24
    void (*func)(int);  // 0x30
    u8 cut;           // 0x34
};

struct TSceAtPosJump {
    Vec pos;          // 0x00
    f32 angle;        // 0x0C
    u8 posSet;        // 0x10
};

struct TSceAtValue {
    int value;        // 0x00
};

// game/sce_at.cpp's SceAtSysWork (static there; the REL imports it by name).
struct TSceAtSys {
    TSceAtFile* pAtData;   // 0x00
    u32 pAtWork;           // 0x04
    void* pItemData;       // 0x08
    u8 pad_C[0x11C - 0xC];
    u8 x11C;               // 0x11C  pAtData was allocated by the tool
    u8 pad_11D[7];
};
extern TSceAtSys SceAtSys;

struct TSceAtWork {
    u8 mode;            // 0x00  routine index
    u8 sub;             // 0x01
    u8 step;            // 0x02
    u8 step2;           // 0x03
    u8 camMode;         // 0x04  debug camera
    s8 cursor;          // 0x05  main menu
    s8 editCursor;      // 0x06  area edit menu
    s8 inputCursor;     // 0x07  data input line
    s8 exitCursor;      // 0x08
    u8 copySrc;         // 0x09
    u8 copyValid;       // 0x0A
    s8 saveSel;         // 0x0B
    u8 pad_C;
    u8 saveStage;       // 0x0D  room saved around the door jump test
    u8 saveRoom;        // 0x0E
    u8 saveX4F9E;       // 0x0F
    u8 light;           // 0x10
    u8 pad_11[3];
    Vec savePos;        // 0x14
    Vec saveRot;        // 0x20
    s16 timer;          // 0x2C
    s16 areaNo;         // 0x2E
    s16 x;              // 0x30
    s16 y;              // 0x32
    s16 x0;             // 0x34
    s16 y0;             // 0x36
    s16 mesNum;         // 0x38  room message names
    s16 cmesNum;        // 0x3A  common message names
    u8 pad_3C[8];
    int saveNum;        // 0x44
    int loaded;         // 0x48  a file was loaded: DATA SAVE enabled
    char path[0x40];    // 0x4C  d:\ path
    char pathX[0x40];   // 0x8C  x:\ path
    AreaData editArea;  // 0xCC  scratch area of the point editors
    TSceAtFileHead head;   // 0xFC
    SceAtWork area[128];   // 0x10C
    TSceAtFile file;       // 0x4F0C  load / save image
    SceAtWork copyBuf;     // 0x9D1C
    char mesName[512][64];   // 0x9DB8
    char cmesName[512][64];  // 0x11DB8
};

struct TSceAtWorkPtr {
    TSceAtWork* p;
};
static TSceAtWorkPtr sceAtWk;
#define pW (sceAtWk.p)
struct SceAtWorkPtr {
    SceAtWork* p;
};
static SceAtWorkPtr sceAtCur;
#define pCur (sceAtCur.p)

static const char* tSceAtTypeName[21] = {"NORMAL", "DOOR",     "EXEC",      "",           "FLG",       "MESSAGE",  "PLANTER",
                                          "JUMP",   "SAVE",     "SHD_DISP",  "DAMAGE",     "SCR_AT",    "VIEW_CTRL", "FIELD_INFO",
                                          "STOOP",  "SMALL_KEY", "LADDER",   "USE",        "HIDE",      "POS_JUMP", "ITEM_PARENT"};

extern "C" {
void tSceAtInit_base();
void tSceAtInit();
void set_filename();
static void tSceAtExit();
static void tSceAtMainMenu();
static void tSceAtAreaEdit();
static void tSceAtAreaEdit_EditMenu();
static void tSceAtAreaEdit_AreaCreate();
static void tSceAtAreaEdit_AreaCopy();
static void tSceAtAreaEdit_AreaPaste();
static void tSceAtAreaEdit_CopyBuffClear();
static void tSceAtAreaEdit_AreaDelete();
void angle_arrow_disp(SceAtWork* a);
void tSceAtAreaEdit_disp();
static void tSceAtAreaEdit_AreaMove();
static void tSceAtAreaEdit_DataInput();
void tSceAtDataInput_basic_menu(int sel, TOOL_MENU* menu);
static void tSceAtDataInput_normal();
static void tSceAtDataInput_door();
void tSceAtDataInput_door_PosSet();
static void tSceAtDataInput_mes();
static void tSceAtDataInput_flg();
static void tSceAtDataInput_jump();
static void tSceAtDataInput_shd_disp();
static void tSceAtDataInput_damage();
static void tSceAtDataInput_scr_at();
static void tSceAtDataInput_cam_ctrl();
static void tSceAtDataInput_cam_ctrl_main();
static void tSceAtDataInput_cam_ctrl_pos_edit();
void tSceAtCamCtrlDataDisp(SceAtCamCtrl* c, int cur);
static void tSceAtDataInput_field_info();
static void tSceAtDataInput_save();
static void tSceAtDataInput_ladder();
static void tSceAtDataInput_ladder_main();
static void tSceAtDataInput_ladder_ETedit();
void tSceAtLadderDataDisp(TSceAtLadder* l, int cur);
static void tSceAtDataInput_use();
static void tSceAtDataInput_hide();
static void tSceAtDataInput_hide_main();
static void tSceAtDataInput_hide_pos_edit();
static void tSceAtDataInput_hide_area_edit();
void tSceAtHideDataDisp(TSceAtHide* h, int cur);
static void tSceAtDataInput_pos_jump();

static void tSceAtDataInput_pos_jump_main();
static void tSceAtDataInput_pos_jump_ETedit();
void tSceAtPosJumpDataDisp(TSceAtPosJump* j, int cur);
void tSceAt_PointDisp(f32 x, f32 y, f32 z);
static void tSceAtDataLoad();
void tSceAtLoadDataCopy();
static void tSceAtDataSave();
void tSceAtSaveDataCreate();
void tSceAtData_DebugCamera();
static void tSceAtPreview();
static void tSceAtPreview_init();
static void tSceAtPreview_main();
void tSceAtPreview_pl_pos();
static void tSceAtPreview_exit();
int loadMesName(const char* path, char* names);
}

#define AREA_NUM 128
#define TYPE_NAME(t) ((u32) (t) <= 0x14 ? tSceAtTypeName[t] : "...no string")

// pad masks of the +/- inputs (the sub stick bits are the header's SLEFT/SRIGHT swapped)
#define REP_RIGHT (JOY_RIGHT | 0x20000)
#define REP_LEFT (JOY_LEFT | 0x10000)

// +1 / -1 on a value
#define STEP(j, v)                     \
    if (Joy[0].j & REP_RIGHT) v++;     \
    if (Joy[0].j & REP_LEFT) v--;

// 0..hi clamp with a second variable (blt / li-at-end shape)
#define CLAMP(v, n, hi)      \
    if ((v) >= 0) {          \
        n = v;               \
        if (n > (hi)) n = hi; \
    } else {                 \
        n = 0;               \
    }

// 0..hi wrap-around
#define WRAP(v, hi) ((v) < 0 ? (hi) : ((v) > (hi) ? 0 : (v)))

// +-a (digital) / +-b (sub stick) on a float field
#define FSTEP(j, f, a, b)                    \
    if (Joy[0].j & JOY_RIGHT) f += a;        \
    if (Joy[0].j & JOY_LEFT) f -= a;         \
    if (Joy[0].j & 0x20000) f += b;          \
    if (Joy[0].j & 0x10000) f -= b;

#define ANG_CLAMP(f) f = (f) < -PI ? -PI : ((f) > PI ? PI : (f))

#define SUB_RESET() \
    pW->sub = 0;    \
    pW->step = 0;   \
    pW->step2 = 0;

#define MODE_RESET() \
    pW->mode = 0;    \
    pW->sub = 0;     \
    pW->step = 0;    \
    pW->step2 = 0;

void ToolSceAt()
{
    void (*routine[6])() = {tSceAtMainMenu, tSceAtAreaEdit, tSceAtPreview, tSceAtDataLoad, tSceAtDataSave, tSceAtExit};

    pW = (TSceAtWork*) Debug_alloc(sizeof(TSceAtWork), 1);
    tSceAtInit();
    pW->mode = 3;
    pW->sub = 0;
    pW->step = 0;
    pW->step2 = 0;
    while (1) {
        if (pW->camMode == 0 && pW->mode != 2) {
            if (Joy[0].on & JOY_SSRIGHT) pW->x0 += 8;
            if (Joy[0].on & JOY_SSLEFT) pW->x0 -= 8;
            if (Joy[0].on & JOY_SSDOWN) pW->y0 += 8;
            if (Joy[0].on & JOY_SSUP) pW->y0 -= 8;
        }
        pW->x = pW->x0;
        pW->y = pW->y0;
        eprintf(pW->x, pW->y, 4, 0, "[SCENARIO ATARI TOOL]");
        pW->y += 0x20;
        pW->x += 8;
        tSceAtAreaEdit_disp();
        if (Joy[0].trg & JOY_START) {
            if (pW->mode != 2) pW->camMode ^= 1;
        }
        if (Joy[0].trg & JOY_Z) {
            if (pW->light) {
                pW->light = 0;
                SetToolLight(-1);
            } else {
                pW->light = 1;
                SetToolLight(1);
            }
        }
        if (pW->camMode) {
            tSceAtData_DebugCamera();
        } else {
            routine[pW->mode]();
        }
        TaskSleep(1);
    }
}

void tSceAtInit_base()
{
    *(TOOL_PTR(0x8678)) = 0x11;
    TOOL_FLAG(OFS_DEBUG_FLG) |= 0x20000000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x20000000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x10000000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x8000000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x800000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x400000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x10000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x2000;
    TOOL_FLAG(OFS_DISP_FLG) |= 0x20000000;
    TOOL_FLAG(OFS_DISP_FLG) |= 0x40000000;
    TOOL_FLAG(OFS_DISP_FLG) |= 0x80000000;
    TOOL_FLAG(OFS_DISP_FLG) |= 0x4000000;
    TOOL_FLAG(OFS_DISP_FLG) |= 0x2000000;
    TOOL_FLAG(OFS_DISP_FLG) |= 0x100000;
    TOOL_FLAG(OFS_DEBUG_FLG) |= 0x10000000;
    pW->light = 1;
    SetToolLight(1);
    TOOL_FLAG(0x5010) |= 0x1000000;
}

void tSceAtInit()
{
    char buf[0x100];

    TutilInitDefault();
    tSceAtInit_base();
    pW->x0 = 0x28;
    pW->y0 = 0xA;
    pW->head.magic[0] = 'A';
    pW->head.magic[1] = 'E';
    pW->head.magic[2] = 'V';
    pW->head.magic[3] = 0;
    pW->head.version = 0x104;
    pW->head.num = AREA_NUM;
    pW->copySrc = 0;
    pW->copyValid = 0;
    pW->loaded = 0;
    set_filename();
    file_lock(pW->pathX);
    if (SceAtSys.pAtData != NULL) {
        memcpy(&pW->file, SceAtSys.pAtData, SceAtSys.pAtData->head.num * sizeof(SceAtWork) + sizeof(TSceAtFileHead));
        tSceAtLoadDataCopy();
    }
    sprintf(buf, "d:\\bio4/prog/head/r%03xmes.h", pG->room_id);
    pW->mesNum = loadMesName(buf, (char*) pW->mesName);
    sprintf(buf, "d:\\bio4/prog/head/cmesmes.h");
    pW->cmesNum = loadMesName(buf, (char*) pW->cmesName);
    CamDbg.pad_10[0] = CamDbg.target_type;
    CamDbg.target_type = 4;
}

void set_filename()
{
    sprintf(pW->path, "d:\\bio4\\room\\st%1x\\r%03x\\r%03x.aev", pG->stage_no, pG->room_id, pG->room_id);
    sprintf(pW->pathX, "x:\\soft\\room\\st%1x\\r%03x\\r%03x.aev", pG->stage_no, pG->room_id, pG->room_id);
}

static TOOL_MENU tSceAtExitMenu[2] = {
    {1, "YES", NULL},
    {1, "NO", NULL},
};

static void tSceAtExit()
{
    s8 sel;

    eprintf(pW->x += 0x28, pW->y += 0x5A, 4, 0, "EXIT?");
    pW->x += 8;
    pW->y += 0x20;
    switch (pW->sub) {
    case 0:
        pW->exitCursor = 1;
        pW->sub++;
    case 1:
        sel = ToolMenuDisp_cur(pW->x, pW->y, 1, &pW->exitCursor, tSceAtExitMenu, sizeof(tSceAtExitMenu), &Joy[0]);
        if (sel >= 0) {
            switch (sel) {
            case 0:
                pW->sub = 9;
                break;
            case 1:
                MODE_RESET();
                break;
            }
        }
        break;
    case 9:
        file_unlock(pW->pathX);
        Debug_free(pW);
        CamDbg.target_type = CamDbg.pad_10[0];
        TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x10000000;
        SetToolLight(-1);
        TOOL_FLAG(0x5010) &= ~0x1000000;
        TutilQuitDefault();
        TaskExit();
        break;
    }
}

static TOOL_MENU tSceAtMainMenuTbl[5] = {
    {1, "AREA EDIT", NULL},
    {1, "PREVIEW", NULL},
    {1, "DATA LOAD", NULL},
    {1, "DATA SAVE", NULL},
    {1, "EXIT", NULL},
};

static void tSceAtMainMenu()
{
    s8 sel;

    if (pW->loaded == 1) {
        tSceAtMainMenuTbl[3].enable = 1;
    } else {
        tSceAtMainMenuTbl[3].enable = 0;
    }
    sel = ToolMenuDisp_cur(pW->x, pW->y, 1, &pW->cursor, tSceAtMainMenuTbl, sizeof(tSceAtMainMenuTbl), &Joy[0]);
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
        case 3:
            pW->mode = 4;
            break;
        case 4:
            pW->mode = 5;
            break;
        }
        SUB_RESET();
    }
}

static void tSceAtAreaEdit()
{
    void (*routine[4])() = {tSceAtAreaEdit_EditMenu, tSceAtAreaEdit_AreaMove, tSceAtAreaEdit_DataInput,
                            tSceAtAreaEdit_AreaCreate};

    if (Joy[0].rep2 & (JOY_R | JOY_L)) {
        if (Joy[0].rep2 & JOY_R) pW->areaNo++;
        if (Joy[0].rep2 & JOY_L) pW->areaNo--;
        pW->areaNo = WRAP(pW->areaNo, AREA_NUM - 1);
        pW->step = 0;
        pW->step2 = 0;
    }
    if (pW->copyValid) {
        eprintf(pW->x, pW->y - 0x10, 7, 0, "->AREA[ %d ]  ID:%s", pW->copySrc, TYPE_NAME(pW->area[pW->copySrc].x35));
    }
    pCur = &pW->area[pW->areaNo];
    eprintf(pW->x, pW->y, 4, 0, "AREA[ %d ]", pW->areaNo);
    if (pCur->flag & 1) {
        eprintf(pW->x + 0x58, pW->y, 0, 0, "ID:");
        eprintf(pW->x + 0x58, pW->y, 6, 0, "   %s", TYPE_NAME(pCur->x35));
    } else {
        eprintf(pW->x + 0x58, pW->y, 2, 0, "no data:");
    }
    pW->x += 8;
    pW->y += 0x10;
    routine[pW->sub]();
}

static TOOL_MENU tSceAtCreateMenu[3] = {
    {1, "AREA CREATE", NULL},
    {0, "AREA PASTE", tSceAtAreaEdit_AreaPaste},
    {0, "COPY BUFF CLEAR", tSceAtAreaEdit_CopyBuffClear},
};

static TOOL_MENU tSceAtEditMenu[6] = {
    {1, "AREA MOVE", NULL},
    {1, "DATA INPUT", NULL},
    {1, "AREA COPY", tSceAtAreaEdit_AreaCopy},
    {0, "AREA PASTE", tSceAtAreaEdit_AreaPaste},
    {0, "COPY BUFF CLEAR", tSceAtAreaEdit_CopyBuffClear},
    {1, "AREA DELEAT", tSceAtAreaEdit_AreaDelete},
};

static void tSceAtAreaEdit_EditMenu()
{
    s8 sel;
    u8 valid = pW->copyValid;

    tSceAtCreateMenu[1].enable = tSceAtCreateMenu[2].enable = tSceAtEditMenu[3].enable = tSceAtEditMenu[4].enable = valid;
    if (pCur->flag & 1) {
        sel = ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->editCursor, tSceAtEditMenu, sizeof(tSceAtEditMenu), &Joy[0]);
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
            break;
        }
    } else {
        sel = ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->editCursor, tSceAtCreateMenu, sizeof(tSceAtCreateMenu), &Joy[0]);
        if (sel == 0) {
            pW->sub = 3;
            pW->step = sel;
            pW->step2 = sel;
            pW->editCursor = sel;
        }
    }
    if (Joy[0].trg & JOY_B) {
        MODE_RESET();
    }
}

static TOOL_MENU tSceAtShapeMenu[3] = {
    {1, "SQUARE", NULL},
    {1, "CIRCLE", NULL},
    {0, "EYE TRG", NULL},
};

static void tSceAtAreaEdit_AreaCreate()
{
    s8 sel;

    switch (pW->step) {
    case 0:
        if (pCur->flag) pW->editCursor = pCur->area.type - 1;
        pW->step++;
    case 1:
        sel = ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->editCursor, tSceAtShapeMenu, sizeof(tSceAtShapeMenu), &Joy[0]);
        if (sel >= 0) {
            if ((pW->editCursor != pCur->area.type - 1 && pCur->flag) || pCur->flag == 0) {
                switch (sel) {
                case 0:
                    AreaDataInit(&pCur->area, &pPL->pos, AREA_TYPE_XZ4, 1500.0f, 1000.0f);
                    break;
                case 1:
                    AreaDataInit(&pCur->area, &pPL->pos, AREA_TYPE_CYLINDER, 1000.0f, 1000.0f);
                    break;
                case 2:
                    AreaDataInit(&pCur->area, &pPL->pos, AREA_TYPE_EYE, 200.0f, 1000.0f);
                    break;
                }
            }
            pCur->flag |= 3;
            pCur->x38 = 2;
            pCur->x39 = 1;
            pCur->x37 |= 1;
            pCur->angle = 0;
            pCur->angleRange = 0x2D;
            pCur->x44 = 2;
            pW->editCursor = 0;
            SUB_RESET();
        }
        break;
    }
    if (Joy[0].trg & JOY_B) {
        SUB_RESET();
    }
}

static void tSceAtAreaEdit_AreaCopy()
{
    pW->copyBuf = *pCur;
    pW->copySrc = pW->areaNo;
    pW->copyValid = 1;
}

static void tSceAtAreaEdit_AreaPaste()
{
    *pCur = pW->copyBuf;
}

static void tSceAtAreaEdit_CopyBuffClear()
{
    memclr_asm(&pW->copyBuf, sizeof(SceAtWork));
    pW->copySrc = 0;
    pW->copyValid = 0;
}

static void tSceAtAreaEdit_AreaDelete()
{
    pCur->flag &= ~1;
    pW->editCursor = 0;
}

#define DEG2RAD 0.017453292f

// the hit-angle arrow of an area: centre, direction and the +-range fan
void angle_arrow_disp(SceAtWork* a)
{
    Vec center;
    Vec p;
    Mtx m;
    Vec v = {0.0f, 0.0f, 1000.0f};
    Vec rot = {0.0f, 0.0f, 0.0f};
    f32 sweep;
    u32 i;

    AreaGetCenterPos(&center, &a->area);
    center.y += 100.0f;
    rot.y = (f32) (a->angle * 2) * DEG2RAD;
    low_RotMatrix(m, &rot);
    TransMatrix(m, &center);
    PSMTXMultVec(m, &v, &p);
    Draw_sphere(&center, 150.0f, 0xFFFFA0FF, 1, 1);
    Draw_sphere(&center, 80.0f, 0x808040FF, 0, 0);
    Draw_sphere(&p, 80.0f, 0xFFFFA0FF, 0, 0);
    Draw_line3d(&center, &p, 0xFEFFFFA0, 0);
    sweep = (f32) ((a->angle + a->angleRange) * 2) * DEG2RAD - (f32) ((a->angle - a->angleRange) * 2) * DEG2RAD;
    for (i = 0; i <= 10; i++) {
        if (i == 5) continue;
        rot.y = (f32) ((a->angle - a->angleRange) * 2) * DEG2RAD + sweep * (f32) i / 10.0f;
        rot.y = LIMIT_ANGLE(rot.y);
        low_RotMatrix(m, &rot);
        TransMatrix(m, &center);
        PSMTXMultVec(m, &v, &p);
        if (i == 0 || i == 10) {
            Draw_line3d(&center, &p, 0xFEFFFFA0, 0);
        } else {
            Draw_line3d(&center, &p, 0xFE40FFFF, 0);
        }
    }
}

void tSceAtAreaEdit_disp()
{
    int i;
    u32 col;

    for (i = 0; i < AREA_NUM; i++) {
        if ((pW->area[i].flag & 1) == 0) continue;
        col = 0x60808080;
        if (pW->areaNo == i) col = 0xA0FF8080;
        if (pW->area[i].x35 != 0xC) {
            AreaDataDisp(&pW->area[i].area, col, 1, NULL);
            if (pW->area[i].x37 & 2) angle_arrow_disp(&pW->area[i]);
        }
        if (pW->area[i].x35 == 0xC) {
            if (pW->areaNo == i) {
                tSceAtCamCtrlDataDisp(&pW->area[i].cam, 1);
            } else {
                tSceAtCamCtrlDataDisp(&pW->area[i].cam, 0);
            }
        }
        if (pW->area[i].x35 == 0x10) {
            if (pW->areaNo == i) {
                tSceAtLadderDataDisp((TSceAtLadder*) pW->area[i].data, 1);
            } else {
                tSceAtLadderDataDisp((TSceAtLadder*) pW->area[i].data, 0);
            }
        }
        if (pW->area[i].x35 == 0x13) {
            if (pW->areaNo == i) {
                tSceAtPosJumpDataDisp((TSceAtPosJump*) pW->area[i].data, 1);
            } else {
                tSceAtPosJumpDataDisp((TSceAtPosJump*) pW->area[i].data, 0);
            }
        }
        if (pW->area[i].x35 == 0x12) {
            if (pW->areaNo == i) {
                tSceAtHideDataDisp((TSceAtHide*) pW->area[i].data, 1);
            } else {
                tSceAtHideDataDisp((TSceAtHide*) pW->area[i].data, 0);
            }
        }
    }
    tSceAtPreview_pl_pos();
    eprintf(0x1AE, 0x24, 0, 0, "[PLAYER]");
    eprintf(0x1AE, 0x34, 0, 0, "X:%.0f", pPL->pos.x);
    eprintf(0x1AE, 0x44, 0, 0, "Y:%.0f", pPL->pos.y);
    eprintf(0x1AE, 0x54, 0, 0, "Z:%.0f", pPL->pos.z);
    eprintf(0x1AE, 0x64, 0, 0, "ANG:%f", pPL->rot.y);
}

static void tSceAtAreaEdit_AreaMove()
{
    if (pCur->flag & 1) {
        AreaDataEdit(&pCur->area, 0xA0FF8080, 0, NULL, 0.75f);
        AreaDataInfoDisp(&pCur->area, pW->x, pW->y);
        AreaDataHelpDisp(&pCur->area, (s16) (pW->x + 0xE0), (s16) (pW->y - 0x20));
    }
    if (Joy[0].trg & JOY_B) {
        SUB_RESET();
    }
}

static void tSceAtAreaEdit_DataInput()
{
    void (*routine[21])() = {tSceAtDataInput_normal,     tSceAtDataInput_door,     tSceAtDataInput_normal,
                             tSceAtDataInput_normal,     tSceAtDataInput_flg,      tSceAtDataInput_mes,
                             tSceAtDataInput_normal,     tSceAtDataInput_jump,     tSceAtDataInput_save,
                             tSceAtDataInput_shd_disp,   tSceAtDataInput_damage,   tSceAtDataInput_scr_at,
                             tSceAtDataInput_cam_ctrl,   tSceAtDataInput_field_info, tSceAtDataInput_normal,
                             tSceAtDataInput_normal,     tSceAtDataInput_ladder,   tSceAtDataInput_use,
                             tSceAtDataInput_hide,       tSceAtDataInput_pos_jump, tSceAtDataInput_normal};

    if (pCur->flag & 1) {
        routine[pCur->x35]();
    }
    if (Joy[0].trg & JOY_B) {
        SUB_RESET();
    }
}

static const char* tSceAtHitTypeName[4] = {"UNDER", "FRONT", "UNDER+ANGLE", "FRONT+ANGLE"};
static const char* tSceAtTrgName[9] = {"", "AUTO", "MANUAL", "", "SEMIAUTO", "", "", "", "ACT_BTN"};
static const char* tSceAtTargetName[16] = {"",              "PL",            "   EM",         "PL+EM",
                                            "      OBJ",     "PL+   OBJ",     "   EM+OBJ",     "PL+EM+OBJ",
                                            "          SUB", "PL+       SUB", "   EM+    SUB", "PL+EM+    SUB",
                                            "      OBJ+SUB", "PL+   OBJ+SUB", "   EM+OBJ+SUB", "PL+EM+OBJ+SUB"};

// the eight lines every type shares (ID .. PRIORITY); `menu` gets its angle / action lines enabled
// Nine fresh-`lis` sites of the original (the menu[5] test and the first pCur read of each case): our
// cse1 carries the earlier high pseudo along the AROUND/taken paths into them and gcse then
// copy-propagates the PRE reg into every later site of the case; the original kept them as
// separate occurrences.  Distinct SYMBOL_REFs for the same object keep them apart.
extern SceAtWorkPtr sceAtCur_m5 asm("sceAtCur"); // COMPILER-DIFF: #12 (cse path / PRE copy)
extern SceAtWorkPtr sceAtCur_c0 asm("sceAtCur"); // COMPILER-DIFF: #12 (cse path / PRE copy)
extern SceAtWorkPtr sceAtCur_c1 asm("sceAtCur"); // COMPILER-DIFF: #12 (cse path / PRE copy)
extern SceAtWorkPtr sceAtCur_c2 asm("sceAtCur"); // COMPILER-DIFF: #12 (cse path / PRE copy)
extern SceAtWorkPtr sceAtCur_c3 asm("sceAtCur"); // COMPILER-DIFF: #12 (cse path / PRE copy)
extern SceAtWorkPtr sceAtCur_c4 asm("sceAtCur"); // COMPILER-DIFF: #12 (cse path / PRE copy)
extern SceAtWorkPtr sceAtCur_c5 asm("sceAtCur"); // COMPILER-DIFF: #12 (cse path / PRE copy)
extern SceAtWorkPtr sceAtCur_c6 asm("sceAtCur"); // COMPILER-DIFF: #12 (cse path / PRE copy)
extern SceAtWorkPtr sceAtCur_c7 asm("sceAtCur"); // COMPILER-DIFF: #12 (cse path / PRE copy)
#define PC(n) (sceAtCur_##n.p)
void tSceAtDataInput_basic_menu(int sel, TOOL_MENU* menu)
{
    s16 x;
    s16 y;
    int n;
    int m;
    u8 on;

    on = pCur->x37 & 2;
    if (on) on = 1;
    menu[2].enable = on;
    menu[3].enable = on;
    do { } while (0); // COMPILER-DIFF: #12 (ends the cse1 path from bb0; sched region split)
    on = PC(m5)->x38 & 8;
    if (on) on = 1;
    menu[5].enable = on;
    x = pW->x + 0x80;
    y = pW->y;
    switch (sel) {
    case 0:
        n = PC(c0)->x35;
        if (Joy[0].rep & REP_RIGHT) {
            n++;
            if (n == 3) n = 4;
        }
        if (Joy[0].rep & REP_LEFT) {
            n--;
            if (n == 3) n = 2;
        }
        CLAMP(n, m, 0x14);
        pCur->x35 = m;
        break;
    case 1:
        n = PC(c1)->x37;
        if (Joy[0].rep & REP_RIGHT) n--;
        if (Joy[0].rep & REP_LEFT) n++;
        m = WRAP(n, 3);
        pCur->x37 = m;
        break;
    case 2: {
        SceAtWork* a = PC(c2);
        if (a->x37 & 2) {
            n = a->angle;
            if (Joy[0].rep & 0x20000) n += 0x2D;
            if (Joy[0].rep & 0x10000) n -= 0x2D;
            if (Joy[0].rep & JOY_RIGHT) n += 5;
            if (Joy[0].rep & JOY_LEFT) n -= 5;
            if (n > 0x59) n -= 0xB4;
            if (n < -0x5A) n += 0xB4;
            a->angle = n;
        }
        break;
    }
    case 3:
        if (PC(c3)->x37 & 2) {
            n = PC(c3)->angleRange;
            if (Joy[0].rep & REP_RIGHT) n += 5;
            if (Joy[0].rep & REP_LEFT) n -= 5;
            CLAMP(n, m, 0x5A);
            pCur->angleRange = m;
        }
        break;
    case 4: {
        SceAtWork* a = PC(c4);
        u8 f = a->x38;
        n = f & 0x7F;
        if (Joy[0].rep & REP_RIGHT) {
            switch (n) {
            case 2:
                n = 1;
                break;
            case 1:
                n = 4;
                break;
            case 4:
                n = 8;
                break;
            case 8:
                if (!(f & 0x80)) {
                    a->x38 = f | 0x80;
                    n = 2;
                }
                break;
            }
        }
        if (Joy[0].rep & REP_LEFT) {
            switch (n) {
            case 2:
                if (pCur->x38 & 0x80) {
                    pCur->x38 &= 0x7F;
                    n = 8;
                }
                break;
            case 1:
                n = 2;
                break;
            case 4:
                n = 1;
                break;
            case 8:
                n = 4;
                break;
            }
        }
        pCur->x38 = (pCur->x38 & 0x80) | n;
        break;
    }
    case 5:
        if (PC(c5)->x38 & 8) {
            n = PC(c5)->x4A;
            STEP(rep2, n);
            m = WRAP(n, 0x41);
            pCur->x4A = m;
        }
        break;
    case 6:
        n = PC(c6)->x39;
        STEP(rep, n);
        CLAMP(n, m, 0xF);
        pCur->x39 = m;
        break;
    case 7:
        n = PC(c7)->x44;
        STEP(rep, n);
        CLAMP(n, m, 0xF);
        pCur->x44 = m;
        break;
    default:
        eprintf(x + 0x40, y - 0x10, 5, 0, "(push Y:data init.)");
        if (Joy[0].trg & JOY_Y) {
            memclr_asm(pCur->data, sizeof(pCur->data));
        }
        break;
    }
    eprintf(x, y, 0, 0, "%s", TYPE_NAME(pCur->x35));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", tSceAtHitTypeName[pCur->x37]);
    y += 0x10;
    if (pCur->x37 & 2) {
        eprintf(x, y, 0, 0, "%d", pCur->angle * 2);
        y += 0x10;
        eprintf(x, y, 0, 0, "%d", pCur->angleRange * 2);
        y += 0x10;
    } else {
        y += 0x20;
    }
    {
        u32 t = pCur->x38 & 0x7F;
        eprintf(x, y, 0, 0, "%s", t <= 8 ? tSceAtTrgName[t] : "...no string");
    }
    if (pCur->x38 & 0x80) {
        eprintf(x, y, 6, 0, "         (boot up only ones.)");
    }
    y += 0x10;
    if (pCur->x38 & 8) {
        eprintf(x, y, 0, 0, "%d", pCur->x4A);
    }
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", tSceAtTargetName[pCur->x39]);
    y += 0x10;
    if (pCur->x44 == 2) {
        eprintf(x, y, 0, 0, "default");
    } else {
        eprintf(x, y, 0, 0, "%d", pCur->x44);
    }
    eprintf(x, y, 6, 0, "        [0:low - 15:high]");
    pW->y = y + 0x10;
}

static TOOL_MENU tSceAtNormalMenu[8] = {
    {1, "ID", NULL},
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "TRIGGER_TYPE", NULL},
    {0, " ACTION TYPE", NULL},
    {1, "TARGET TYPE", NULL},
    {1, "PRIORITY", NULL},
};

static void tSceAtDataInput_normal()
{
    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceAtNormalMenu, sizeof(tSceAtNormalMenu), &Joy[0]);
    tSceAtDataInput_basic_menu(pW->inputCursor, tSceAtNormalMenu);
}

static TOOL_MENU tSceAtDoorMenu[21] = {
    {1, "ID", NULL},
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "TRIGGER_TYPE", NULL},
    {0, " ACTION TYPE", NULL},
    {1, "TARGET TYPE", NULL},
    {1, "PRIORITY", NULL},
    {1, " NEXT_STAGE_NO", NULL},
    {1, " NEXT_ROOM_NO", NULL},
    {1, " NEXT_PART_NO", NULL},
    {1, " NEXT_POS_SET", NULL},
    {1, "  NEXT_POS_X", NULL},
    {1, "  NEXT_POS_Y", NULL},
    {1, "  NEXT_POS_Z", NULL},
    {1, "  NEXT_ANG_Y", NULL},
    {1, " KEY_ID", NULL},
    {1, " KEY_FLG", NULL},
    {1, " KEY_SE", NULL},
    {1, " OPEN_SE", NULL},
    {1, " FADE_EFF", NULL},
};
static const char* tSceAtLockName[3] = {"DEFAULT", "IN_LOCK_CLOSE", "IN_LOCK_OPEN"};
static const char* tSceAtFadeName[3] = {"NORMAL", "FADE", "BLACK"};

static void tSceAtDataInput_door()
{
    s16 x;
    s16 y;
    int n;
    int m;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceAtDoorMenu, sizeof(tSceAtDoorMenu), &Joy[0]);
    tSceAtDataInput_basic_menu(pW->inputCursor, tSceAtDoorMenu);
    switch (pW->inputCursor) {
    case 8:
        n = pCur->dstStage;
        STEP(rep2, n);
        m = WRAP(n, 8);
        pCur->dstStage = m;
        break;
    case 9:
        n = pCur->dstRoom;
        STEP(rep2, n);
        m = WRAP(n, 0x7F);
        pCur->dstRoom = m;
        break;
    case 0xA:
        n = pCur->dstX4F9E;
        STEP(rep2, n);
        m = WRAP(n, 0x7F);
        pCur->dstX4F9E = m;
        break;
    case 0xB:
        if (Joy[0].trg & JOY_A) tSceAtDataInput_door_PosSet();
        break;
    case 0xC:
        FSTEP(rep2, pCur->dstPos.x, 20.0f, 200.0f);
        break;
    case 0xD:
        FSTEP(rep2, pCur->dstPos.y, 20.0f, 200.0f);
        break;
    case 0xE:
        FSTEP(rep2, pCur->dstPos.z, 20.0f, 200.0f);
        break;
    case 0xF:
        FSTEP(rep2, pCur->dstAngle, PI / 128.0f, PI / 32.0f);
        ANG_CLAMP(pCur->dstAngle);
        break;
    case 0x10:
        n = pCur->lockType;
        STEP(rep, n);
        CLAMP(n, m, 2);
        pCur->lockType = m;
        break;
    case 0x11:
        n = pCur->lockFlag;
        STEP(rep2, n);
        m = WRAP(n, 0x3F);
        pCur->lockFlag = m;
        break;
    case 0x12:
        n = pCur->doorSe;
        STEP(rep2, n);
        m = WRAP(n, 0xA);
        pCur->doorSe = m;
        break;
    case 0x13:
        n = pCur->doorNo;
        STEP(rep2, n);
        m = WRAP(n, 0xFF);
        pCur->doorNo = m;
        break;
    case 0x14:
        n = pCur->x77;
        STEP(rep2, n);
        m = WRAP(n, 2);
        pCur->x77 = m;
        break;
    }
    x = pW->x + 0x80;
    y = pW->y;
    eprintf(x, y, 0, 0, "%01x", pCur->dstStage);
    y += 0x10;
    eprintf(x, y, 0, 0, "%02x", pCur->dstRoom);
    y += 0x10;
    eprintf(x, y, 0, 0, "%02x", pCur->dstX4F9E);
    y += 0x20;
    eprintf(x, y, 0, 0, " %.0f", pCur->dstPos.x);
    y += 0x10;
    eprintf(x, y, 0, 0, " %.0f", pCur->dstPos.y);
    y += 0x10;
    eprintf(x, y, 0, 0, " %.0f", pCur->dstPos.z);
    y += 0x10;
    eprintf(x, y, 0, 0, " %f", pCur->dstAngle);
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", (u32) pCur->lockType <= 2 ? tSceAtLockName[pCur->lockType] : "...no string");
    y += 0x10;
    eprintf(x, y, 0, 0, "%02x", pCur->lockFlag);
    y += 0x10;
    eprintf(x, y, 0, 0, "%d", pCur->doorSe);
    y += 0x10;
    eprintf(x, y, 0, 0, "%d", pCur->doorNo);
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", (u32) pCur->x77 <= 2 ? tSceAtFadeName[pCur->x77] : "...no string");
    pW->y = y + 0x10;
}

// jumps through the door to set the destination point with the player, then comes back
static inline f32 FCRef(const f32& v) { return v; }
// scalar-reference stores: the following pG load stays below them and pG is reloaded
static inline void U8Set(u8& d, u8 v) { d = v; }
static inline void U16Set(u16& d, u16 v) { d = v; }

void tSceAtDataInput_door_PosSet()
{
    static const f32 zero = 0.0f;
    f32 spd;
    f32 z;

    TOOL_FLAG(0x6C) |= 0x10000;
    TOOL_FLAG(0x68) |= 0x4000000;
    pW->saveStage = pG->stage_no;
    pW->saveRoom = pG->room_no;
    pW->saveX4F9E = pG->x4F9E;
    memcpy((u8*) pW + 0x14, &pPL->pos, sizeof(Vec));
    memcpy((u8*) pW + 0x20, &pPL->rot, sizeof(Vec));
    if (pCur->dstPos.x == (z = FCRef(zero)) && pCur->dstPos.y == z && pCur->dstPos.z == z) {
        GetNextPos(pCur->dstStage, pCur->dstRoom);
    } else {
        FSet(pG->next_pos.x, pCur->dstPos.x);
        FSet(pG->next_pos.y, pCur->dstPos.y);
        FSet(pG->next_pos.z, pCur->dstPos.z);
        FSet(pG->next_angle, pCur->dstAngle);
        U16Set(pG->room_id_prev, pG->room_id);
        U8Set(pG->x4FA2, pG->x4F9E);
        U8Set(pG->next_stage, pCur->dstStage);
        U8Set(pG->next_room_no, pCur->dstRoom);
        U8Set(pG->next_point, pCur->dstX4F9E);
    }
    *(TOOL_PTR(0x8678)) = 7;
    TOOL_FLAG(0x68) |= 0x80000000;
    pG->x20 = 4;
    pG->x21 = 0;
    pG->x22 = 0;
    pG->x23 = 0;
    while (pG->x20 != 3) TaskSleep(1);
    while (!(Joy[0].trg & JOY_START)) {
        if (Joy[0].on & JOY_X) {
            TOOL_FLAG(0x68) |= 8;
            KeyStop(0xEFCF0000);
            if (Joy[0].on & JOY_A) {
                spd = 10.0f;
            } else {
                spd = 1.0f;
            }
            Vec d = {0.0f, 0.0f, 0.0f};
            d.x = spd * (f32) Joy[0].sx + d.x;
            d.y = spd * (f32) Joy[0].sy + d.y;
            moveOnPlaneXZ(&d, &d);
            d.y = spd * (f32) (int) Joy[0].trigR + d.y - spd * (f32) (int) Joy[0].trigL;
            PSVECAdd(&pPL->pos, &d, &pPL->pos);
            Draw_pos(&pPL->pos, 2000);
        } else {
            TOOL_FLAG(OFS_STOP_FLG) &= ~0x80000000;
            TOOL_FLAG(0x68) &= ~8;
        }
        TaskSleep(1);
    }
    FSet(pCur->dstPos.x, pPL->pos.x);
    FSet(pCur->dstPos.y, pPL->pos.y);
    FSet(pCur->dstPos.z, pPL->pos.z);
    FSet(pCur->dstAngle, pPL->rot.y);
    FSet(pG->next_pos.x, pW->savePos.x);
    FSet(pG->next_pos.y, pW->savePos.y);
    FSet(pG->next_pos.z, pW->savePos.z);
    FSet(pG->next_angle, pW->saveRot.y);
    U16Set(pG->room_id_prev, pG->room_id);
    U8Set(pG->x4FA2, pG->x4F9E);
    U8Set(pG->next_stage, pW->saveStage);
    U8Set(pG->next_room_no, pW->saveRoom);
    U8Set(pG->next_point, pW->saveX4F9E);
    TOOL_FLAG(0x68) |= 0x80000000;
    pG->x20 = 4;
    pG->x21 = 0;
    pG->x22 = 0;
    pG->x23 = 0;
    while (pG->x20 != 3) TaskSleep(1);
    tSceAtInit_base();
    TOOL_FLAG(0x6C) &= ~0x10000;
    TOOL_FLAG(0x68) &= ~0x4000000;
}

static TOOL_MENU tSceAtMesMenu[13] = {
    {1, "ID", NULL},
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "TRIGGER_TYPE", NULL},
    {0, " ACTION TYPE", NULL},
    {1, "TARGET TYPE", NULL},
    {1, "PRIORITY", NULL},
    {1, " MES_TYPE", NULL},
    {1, " MES_NO", NULL},
    {1, " CAM_NO", NULL},
    {1, " SE_TYPE", NULL},
    {1, " SE_NO", NULL},
};

static void tSceAtDataInput_mes()
{
    SceAtMesData* d = &pCur->mes;
    s16 x;
    s16 y;
    int n;
    int m;
    int num;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceAtMesMenu, sizeof(tSceAtMesMenu), &Joy[0]);
    tSceAtDataInput_basic_menu(pW->inputCursor, tSceAtMesMenu);
    switch (pW->inputCursor) {
    case 8:
        n = d->type;
        STEP(rep, n);
        CLAMP(n, m, 1);
        d->type = m;
        break;
    case 9:
        n = d->no;
        STEP(rep2, n);
        if (d->type == 0) {
            num = pW->mesNum;
            if (num > 0) {
                if (n >= -1) {
                    int max = num - 1;
                    m = n;
                    if (m > max) m = max;
                } else {
                    m = -1;
                }
                n = m;
            }
        } else {
            num = pW->cmesNum;
            if (num > 0) {
                if (n >= -1) {
                    int max = num - 1;
                    m = n;
                    if (m > max) m = max;
                } else {
                    m = -1;
                }
                n = m;
            }
        }
        d->no = n;
        break;
    case 0xA:
        n = d->x4;
        STEP(rep2, n);
        CLAMP(n, m, 0xFF);
        d->x4 = m;
        break;
    case 0xB:
        n = d->x5;
        STEP(rep, n);
        CLAMP(n, m, 1);
        d->x5 = m;
        break;
    case 0xC:
        n = d->x6;
        STEP(rep2, n);
        CLAMP(n, m, 0x1FF);
        d->x6 = m;
        break;
    }
    x = pW->x + 0x80;
    y = pW->y;
    eprintf(x, y, 0, 0, "%s", d->type != 0 ? "COMMON_MES" : "ROOM_MES");
    y += 0x10;
    if (d->type == 0) {
        if (pW->mesNum != 0 && d->no < pW->mesNum && d->no >= 0) {
            eprintf(x, y, 0, 0, "%d  %s", d->no, pW->mesName[d->no]);
        } else {
            eprintf(x, y, 0, 0, "%d", d->no);
        }
    } else {
        if (pW->cmesNum != 0 && d->no < pW->cmesNum && d->no >= 0) {
            eprintf(x, y, 0, 0, "%d  %s", d->no, pW->cmesName[d->no]);
        } else {
            eprintf(x, y, 0, 0, "%d", d->no);
        }
    }
    y += 0x10;
    if (d->x4 == 0) {
        eprintf(x, y, 0, 0, "no cam");
    } else {
        eprintf(x, y, 0, 0, "%d", d->x4 - 1);
    }
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", d->x5 != 0 ? "SE_PL" : "SE_ROOM");
    y += 0x10;
    if (d->x6 == 0) {
        eprintf(x, y, 0, 0, "no se");
    } else {
        eprintf(x, y, 0, 0, "%d", d->x6 - 1);
    }
}

static TOOL_MENU tSceAtFlgMenu[11] = {
    {1, "ID", NULL},
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "TRIGGER_TYPE", NULL},
    {0, " ACTION TYPE", NULL},
    {1, "TARGET TYPE", NULL},
    {1, "PRIORITY", NULL},
    {1, " FLG_ID", NULL},
    {1, " FLG_NO", NULL},
    {1, " FLG_ACT", NULL},
};
static const char* tSceAtFlgName[3] = {"ROOM_FLG", "ROOM_SAVE_FLG", "SCENARIO_FLG"};

static void tSceAtDataInput_flg()
{
    SceAtFlg* d = &pCur->flg;
    s16 x;
    s16 y;
    int n;
    int m;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceAtFlgMenu, sizeof(tSceAtFlgMenu), &Joy[0]);
    tSceAtDataInput_basic_menu(pW->inputCursor, tSceAtFlgMenu);
    switch (pW->inputCursor) {
    case 8:
        n = d->kind;
        STEP(rep, n);
        CLAMP(n, m, 2);
        d->kind = m;
        break;
    case 9:
        n = d->no;
        STEP(rep2, n);
        m = WRAP(n, 0x3FF);
        d->no = m;
        break;
    case 0xA:
        if (Joy[0].rep & REP_RIGHT) d->off = 1;
        if (Joy[0].rep & REP_LEFT) d->off = 0;
        break;
    }
    x = pW->x + 0x80;
    y = pW->y;
    eprintf(x, y, 0, 0, "%s", (u32) d->kind <= 2 ? tSceAtFlgName[d->kind] : "...no string");
    y += 0x10;
    eprintf(x, y, 0, 0, "%04x(%d)", d->no, d->no);
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", d->off == 0 ? "ON" : "OFF");
}

static TOOL_MENU tSceAtJumpMenu[11] = {
    {1, "ID", NULL},
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "TRIGGER_TYPE", NULL},
    {0, " ACTION TYPE", NULL},
    {1, "TARGET TYPE", NULL},
    {1, "PRIORITY", NULL},
    {1, " JUMP_POS_X", NULL},
    {1, " JUMP_POS_Y", NULL},
    {1, " JUMP_POS_Z", NULL},
};

static void tSceAtDataInput_jump()
{
    s16 x;
    s16 y;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceAtJumpMenu, sizeof(tSceAtJumpMenu), &Joy[0]);
    tSceAtDataInput_basic_menu(pW->inputCursor, tSceAtJumpMenu);
    switch (pW->inputCursor) {
    case 8:
        FSTEP(rep2, pCur->jumpPos.x, 20.0f, 200.0f);
        break;
    case 9:
        FSTEP(rep2, pCur->jumpPos.y, 20.0f, 200.0f);
        break;
    case 0xA:
        FSTEP(rep2, pCur->jumpPos.z, 20.0f, 200.0f);
        break;
    }
    tSceAt_PointDisp(pCur->jumpPos.x, pCur->jumpPos.y, pCur->jumpPos.z);
    x = pW->x + 0x80;
    y = pW->y;
    eprintf(x, y, 0, 0, "%f", pCur->jumpPos.x);
    y += 0x10;
    eprintf(x, y, 0, 0, "%f", pCur->jumpPos.y);
    y += 0x10;
    eprintf(x, y, 0, 0, "%f", pCur->jumpPos.z);
    pW->y = y + 0x10;
}

static TOOL_MENU tSceAtShdDispMenu[10] = {
    {1, "ID", NULL},
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "TRIGGER_TYPE", NULL},
    {0, " ACTION TYPE", NULL},
    {1, "TARGET TYPE", NULL},
    {1, "PRIORITY", NULL},
    {1, " SHD_NO", NULL},
    {1, " DISP_FLG", NULL},
};

static void tSceAtDataInput_shd_disp()
{
    SceAtShdDisp* d = &pCur->shd;
    s16 x;
    s16 y;
    int n;
    int m;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceAtShdDispMenu, sizeof(tSceAtShdDispMenu), &Joy[0]);
    tSceAtDataInput_basic_menu(pW->inputCursor, tSceAtShdDispMenu);
    switch (pW->inputCursor) {
    case 8:
        n = d->no;
        STEP(rep, n);
        CLAMP(n, m, 0xFF);
        d->no = m;
        break;
    case 9:
        n = d->on;
        STEP(rep, n);
        CLAMP(n, m, 1);
        d->on = m;
        break;
    }
    x = pW->x + 0x80;
    y = pW->y;
    eprintf(x, y, 0, 0, "%d", d->no);
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", d->on != 0 ? "ON" : "OFF");
}

static TOOL_MENU tSceAtDamageMenu[14] = {
    {1, "ID", NULL},
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "TRIGGER_TYPE", NULL},
    {0, " ACTION TYPE", NULL},
    {1, "TARGET TYPE", NULL},
    {1, "PRIORITY", NULL},
    {1, " DAMAGE_TYPE", NULL},
    {1, " DAMAGE_TIMER", NULL},
    {1, " DAMAGE_VOLUME", NULL},
    {1, " DAMAGE_DIE_FLG", NULL},
    {1, " DAMAGE_ANG_SET", NULL},
    {1, "  ANGLE", NULL},
};
static const char* tSceAtDamageName[7] = {"NO HIT", "FIRE", "ELEC", "ENV_LIGHT", "ENV_FIRE", "GRENADE_BLAST", "PUSH"};

static void tSceAtDataInput_damage()
{
    SceAtDamage* d = &pCur->dmg;
    s16 x;
    s16 y;
    int n;
    int m;
    u8 em;
    u8 fl;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceAtDamageMenu, sizeof(tSceAtDamageMenu), &Joy[0]);
    tSceAtDataInput_basic_menu(pW->inputCursor, tSceAtDamageMenu);
    switch (pW->inputCursor) {
    case 8:
        n = d->kind;
        STEP(rep, n);
        em = pCur->x39 & 9;
        if (em != 0) {
            CLAMP(n, m, 0x1E);
            n = m;
        }
        d->kind = n;
        break;
    case 9:
        n = d->time;
        if (Joy[0].rep2 & JOY_RIGHT) n++;
        if (Joy[0].rep2 & JOY_LEFT) n--;
        if (Joy[0].rep2 & 0x20000) n += 30;
        if (Joy[0].rep2 & 0x10000) n -= 30;
        CLAMP(n, m, 1800);
        d->time = m;
        break;
    case 0xA:
        n = d->arg;
        STEP(rep2, n);
        CLAMP(n, m, 1000);
        d->arg = m;
        break;
    case 0xB:
        if (Joy[0].rep2 & REP_RIGHT) d->flags |= 1;
        if (Joy[0].rep2 & REP_LEFT) d->flags &= ~1;
        break;
    case 0xC:
        if (Joy[0].rep2 & REP_RIGHT) d->flags |= 2;
        if (Joy[0].rep2 & REP_LEFT) d->flags &= ~2;
        break;
    case 0xD:
        if (d->flags & 2) {
            if (Joy[0].rep2 & REP_RIGHT) d->power += 5.0f * DEG2RAD;
            if (Joy[0].rep2 & REP_LEFT) d->power -= 5.0f * DEG2RAD;
            d->power = LIMIT_ANGLE(d->power);
        }
        break;
    }
    x = pW->x + 0x80;
    y = pW->y;
    em = pCur->x39 & 9;
    if (em != 0) {
        eprintf(x, y, 0, 0, "%d", d->kind);
    } else {
        eprintf(x, y, 0, 0, "%s", (u32) d->kind <= 6 ? tSceAtDamageName[d->kind] : "...no string");
    }
    y += 0x10;
    if (d->time == 0) {
        eprintf(x, y, 0, 0, "default");
    } else {
        eprintf(x, y, 0, 0, "%d", d->time);
    }
    y += 0x10;
    eprintf(x, y, 0, 0, "%d", d->arg);
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", (d->flags & 1) ? "ON" : "OFF");
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", (d->flags & 2) ? "ON" : "OFF");
    y += 0x10;
    if (d->flags & 2) {
        eprintf(x, y, 0, 0, "%f", d->power);
    }
    fl = d->flags;
    if (fl & 2) {
        Vec center;
        Vec p;
        Mtx mtx;
        Vec v = {0.0f, 0.0f, 1000.0f};
        Vec rot = {0.0f, 0.0f, 0.0f};

        AreaGetCenterPos(&center, &pCur->area);
        center.y += 100.0f;
        rot.y = d->power;
        low_RotMatrix(mtx, &rot);
        TransMatrix(mtx, &center);
        PSMTXMultVec(mtx, &v, &p);
        Draw_sphere(&center, 150.0f, 0xFFFFA0FF, 1, 1);
        Draw_sphere(&center, 80.0f, 0x808040FF, 0, 0);
        Draw_sphere(&p, 80.0f, 0xFFFFA0FF, 0, 0);
        Draw_line3d(&center, &p, 0xFEFFFFA0, 0);
    }
}

static TOOL_MENU tSceAtScrAtMenu[27] = {
    {1, "ID", NULL},
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "TRIGGER_TYPE", NULL},
    {0, " ACTION TYPE", NULL},
    {1, "TARGET TYPE", NULL},
    {1, "PRIORITY", NULL},
    {1, " EAT_NO_SET", NULL},
    {1, " SAT_NO_SET", NULL},
    {1, " S:EM_NOHIT", NULL},
    {1, " S:1m_UP", NULL},
    {1, " S:1m_DOWN", NULL},
    {1, " S:2m_UP", NULL},
    {1, " S:2m_DOWN", NULL},
    {1, " F_BOX", NULL},
    {1, " F_FLOOR", NULL},
    {1, " S:FALL", NULL},
    {1, " E:EFF_BIT", NULL},
    {1, " E:SMALL_NOHIT", NULL},
    {1, " E:MIDDLE_NOHIT", NULL},
    {1, "  :NO_EFF_SET", NULL},
    {1, " S:CLIFF", NULL},
    {1, " S:ROUTE_NOHIT", NULL},
    {1, " S:PL_NO_HIT", NULL},
    {1, " S:FANCE", NULL},
    {1, " S:SEE_NOHIT", NULL},
};

#define ONOFF(c) ((c) ? "ON" : "OFF")

// bit toggles on the +/- keys
#define BITSEL(f, bit)                              \
    if (Joy[0].rep & REP_RIGHT) f |= bit;           \
    if (Joy[0].rep & REP_LEFT) f &= ~(bit);

static void tSceAtDataInput_scr_at()
{
    SceAtScrAt* d = &pCur->scr;
    s16 x;
    s16 y;
    int eff;
    int m;
    u32 a2;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceAtScrAtMenu, sizeof(tSceAtScrAtMenu), &Joy[0]);
    tSceAtDataInput_basic_menu(pW->inputCursor, tSceAtScrAtMenu);
    a2 = d->attr2;
    eff = (a2 >> 23) & 1;
    if (a2 & 0x8000) eff |= 2;
    if (a2 & 0x80) eff |= 4;
    switch (pW->inputCursor) {
    case 8:
        BITSEL(d->flags, 1);
        break;
    case 9:
        BITSEL(d->flags, 2);
        break;
    case 0xA:
        BITSEL(d->attr, 0x4000);
        break;
    case 0xB:
        BITSEL(d->attr, 0x200000);
        break;
    case 0xC:
        BITSEL(d->attr, 0x2000);
        break;
    case 0xD:
        BITSEL(d->attr, 0x1000);
        break;
    case 0xE:
        BITSEL(d->attr, 0x10);
        break;
    case 0xF:
        BITSEL(d->flag, 0x100);
        break;
    case 0x10:
        BITSEL(d->flag, 0x200);
        break;
    case 0x11:
        BITSEL(d->attr, 0x100000);
        break;
    case 0x12:
        STEP(rep, eff);
        CLAMP(eff, m, 7);
        eff = m;
        if (eff & 1) {
            d->attr2 |= 0x800000;
        } else {
            d->attr2 &= ~0x800000;
        }
        if (eff & 2) {
            d->attr2 |= 0x8000;
        } else {
            d->attr2 &= ~0x8000;
        }
        if (eff & 4) {
            d->attr2 |= 0x80;
        } else {
            d->attr2 &= ~0x80;
        }
        break;
    case 0x13:
        BITSEL(d->attr2, 0x400000);
        break;
    case 0x14:
        BITSEL(d->attr2, 0x4000);
        break;
    case 0x15:
        if (Joy[0].rep & REP_RIGHT) {
            d->attr2 |= 0x40;
            d->attr |= 4;
        }
        if (Joy[0].rep & REP_LEFT) {
            d->attr2 &= ~0x40;
            d->attr &= ~4;
        }
        break;
    case 0x16:
        BITSEL(d->attr, 0x800);
        break;
    case 0x17:
        BITSEL(d->flags, 4);
        break;
    case 0x18:
        BITSEL(d->attr, 0x400000);
        break;
    case 0x19:
        BITSEL(d->attr, 0x20);
        break;
    case 0x1A:
        BITSEL(d->attr, 0x800000);
        break;
    }
    x = pW->x + 0x80;
    y = pW->y;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->flags & 1));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->flags & 2));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->attr & 0x4000));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->attr & 0x200000));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->attr & 0x2000));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->attr & 0x1000));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->attr & 0x10));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->flag & 0x100));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->flag & 0x200));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->attr & 0x100000));
    y += 0x10;
    eprintf(x, y, 0, 0, "%d", eff);
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->attr2 & 0x400000));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->attr2 & 0x4000));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->attr2 & 0x40));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->attr & 0x800));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", (d->flags & 4) ? "OFF" : "ON");
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->attr & 0x400000));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->attr & 0x20));
    y += 0x10;
    eprintf(x, y, 0, 0, "%s", ONOFF(d->attr & 0x800000));
}

static void tSceAtDataInput_cam_ctrl()
{
    void (*routine[2])() = {tSceAtDataInput_cam_ctrl_main, tSceAtDataInput_cam_ctrl_pos_edit};

    routine[pW->step]();
    tSceAtCamCtrlDataDisp(&pCur->cam, 1);
}

static TOOL_MENU tSceAtCamCtrlMenu[13] = {
    {1, "ID", NULL},
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "TRIGGER_TYPE", NULL},
    {0, " ACTION TYPE", NULL},
    {1, "TARGET TYPE", NULL},
    {1, "PRIORITY", NULL},
    {1, " TYPE", NULL},
    {1, " POS_SET", NULL},
    {1, " ANG_Y", NULL},
    {1, " RADIUS", NULL},
    {1, " OUT_RANGE", NULL},
};

#define RANGE_CLAMP(f) f = (f) < 0.0f ? 0.0f : ((f) > 5000.0f ? 5000.0f : (f))

static void tSceAtDataInput_cam_ctrl_main()
{
    s16 x;
    s16 y;
    int n;
    int m;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceAtCamCtrlMenu, sizeof(tSceAtCamCtrlMenu), &Joy[0]);
    tSceAtDataInput_basic_menu(pW->inputCursor, tSceAtCamCtrlMenu);
    switch (pW->inputCursor) {
    case 8:
        n = pCur->cam.mode;
        STEP(rep, n);
        CLAMP(n, m, 1);
        pCur->cam.mode = m;
        break;
    case 9:
        if (Joy[0].trg & JOY_A) pW->step = 1;
        break;
    case 0xA:
        FSTEP(rep, pCur->cam.angle, PI / 32.0f, PI / 2.0f);
        ANG_CLAMP(pCur->cam.angle);
        break;
    case 0xB:
        FSTEP(rep, pCur->cam.range, 10.0f, 100.0f);
        RANGE_CLAMP(pCur->cam.range);
        break;
    case 0xC:
        FSTEP(rep, pCur->cam.range2, 10.0f, 100.0f);
        RANGE_CLAMP(pCur->cam.range2);
        break;
    }
    x = pW->x + 0x80;
    y = pW->y;
    eprintf(x, y, 0, 0, "%d", pCur->cam.mode);
    y += 0x20;
    eprintf(x, y, 0, 0, "%f", pCur->cam.angle);
    y += 0x10;
    eprintf(x, y, 0, 0, "%f", pCur->cam.range);
    y += 0x10;
    eprintf(x, y, 0, 0, "%f", pCur->cam.range2);
    pW->y = y + 0x10;
}

// the point editors move pW->editArea (an eye trigger) and copy its position back
#define POINT_EDIT(pos, rate)                                                              \
    AreaDataEdit(&pW->editArea, 0xA0FF8080, 0, NULL, rate);                                \
    AreaDataDisp(&pW->editArea, 0xA0FF8080, 1, NULL);                                      \
    AreaDataInfoDisp(&pW->editArea, pW->x, pW->y);                                         \
    AreaDataHelpDisp(&pW->editArea, (s16) (pW->x + 0xE0), (s16) (pW->y - 0x20));           \
    pos.x = pW->editArea.u.eye.x;                                                          \
    pos.y = pW->editArea.u.eye.y;                                                          \
    pos.z = pW->editArea.u.eye.z;                                                          \
    if (Joy[0].trg & JOY_B) {                                                              \
        pW->step = 0;                                                                      \
        pW->step2 = 0;                                                                     \
        Joy[0].trg = 0;                                                                    \
    }

static void tSceAtDataInput_cam_ctrl_pos_edit()
{
    SceAtCamCtrl* c = &pCur->cam;
    Vec center;

    switch (pW->step2) {
    case 0:
        if (c->pad_10 == 0) {
            AreaGetCenterPos(&center, &pCur->area);
            AreaDataInit(&pW->editArea, &center, AREA_TYPE_EYE, 200.0f, 1000.0f);
            c->range = 1000.0f;
            c->range2 = 500.0f;
            c->pad_10 = 1;
        } else {
            pW->editArea.u.eye.x = c->pos.x;
            pW->editArea.u.eye.y = c->pos.y;
            pW->editArea.u.eye.z = c->pos.z;
        }
        pW->step2++;
    case 1:
        POINT_EDIT(c->pos, 0.75f);
        break;
    }
}

void tSceAtCamCtrlDataDisp(SceAtCamCtrl* c, int cur)
{
    Mtx m;
    Vec v = {0.0f, 0.0f, 800.0f};
    Vec r;
    Vec rot = {0.0f, 0.0f, 0.0f};
    u32 col = 0x408040;

    rot.y = c->angle;
    r = rot;
    low_RotMatrix(m, &r);
    TransMatrix(m, &c->pos);
    PSMTXMultVec(m, &v, &rot);
    if (cur == 1) col = 0xA0FFA0;
    Draw_sphere(&c->pos, 150.0f, (col << 8) | 0xFF, 1, 1);
    Draw_sphere(&c->pos, 80.0f, 0x808040FF, 0, 0);
    Draw_sphere(&rot, 50.0f, (col << 8) | 0xFF, 0, 0);
    Draw_line3d(&c->pos, &rot, col | 0xFE000000, 0);
    Draw_cylinder(&c->pos, c->range, 1000.0f, (col << 8) | 0x40);
    Draw_cylinder(&c->pos, c->range + c->range2, 1000.0f, (col << 8) | 0x40);
}

static TOOL_MENU tSceAtFieldInfoMenu[9] = {
    {1, "ID", NULL},
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "TRIGGER_TYPE", NULL},
    {0, " ACTION TYPE", NULL},
    {1, "TARGET TYPE", NULL},
    {1, "PRIORITY", NULL},
    {1, " FIELD_ID", NULL},
};

static void tSceAtDataInput_field_info()
{
    SceAtField* d = &pCur->field;
    int n;
    int m;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceAtFieldInfoMenu, sizeof(tSceAtFieldInfoMenu), &Joy[0]);
    tSceAtDataInput_basic_menu(pW->inputCursor, tSceAtFieldInfoMenu);
    if (pW->inputCursor == 8) {
        n = d->value;
        STEP(rep, n);
        CLAMP(n, m, 3);
        d->value = m;
    }
    eprintf((s16) (pW->x + 0x80), pW->y, 0, 0, "%d", d->value);
}

static TOOL_MENU tSceAtSaveAtMenu[9] = {
    {1, "ID", NULL},
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "TRIGGER_TYPE", NULL},
    {0, " ACTION TYPE", NULL},
    {1, "TARGET TYPE", NULL},
    {1, "PRIORITY", NULL},
    {1, " TERM_NO", NULL},
};

static void tSceAtDataInput_save()
{
    TSceAtValue* d = (TSceAtValue*) pCur->data;
    int n;
    int m;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceAtSaveAtMenu, sizeof(tSceAtSaveAtMenu), &Joy[0]);
    tSceAtDataInput_basic_menu(pW->inputCursor, tSceAtSaveAtMenu);
    if (pW->inputCursor == 8) {
        n = d->value;
        STEP(rep, n);
        CLAMP(n, m, 0xA);
        d->value = m;
    }
    eprintf((s16) (pW->x + 0x80), pW->y, 0, 0, "%d", d->value);
}

static void tSceAtDataInput_ladder()
{
    void (*routine[2])() = {tSceAtDataInput_ladder_main, tSceAtDataInput_ladder_ETedit};

    routine[pW->step]();
}

static TOOL_MENU tSceAtLadderMenu[17] = {
    {1, "ID", NULL},
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "TRIGGER_TYPE", NULL},
    {0, " ACTION TYPE", NULL},
    {1, "TARGET TYPE", NULL},
    {1, "PRIORITY", NULL},
    {1, " LADDER_POS_SET", NULL},
    {1, "  LADDER_POS_X", NULL},
    {1, "  LADDER_POS_Y", NULL},
    {1, "  LADDER_POS_Z", NULL},
    {1, " LADDER_ANG", NULL},
    {1, " LADDER_HEIGHT", NULL},
    {1, " CAM_NO", NULL},
    {1, " CAM_NO2", NULL},
    {1, " CAM_NO3", NULL},
};

static void tSceAtDataInput_ladder_main()
{
    s16 x;
    s16 y;
    int n;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceAtLadderMenu, sizeof(tSceAtLadderMenu), &Joy[0]);
    tSceAtDataInput_basic_menu(pW->inputCursor, tSceAtLadderMenu);
    switch (pW->inputCursor) {
    case 8:
        if (Joy[0].trg & JOY_A) pW->step = 1;
        break;
    case 9:
        FSTEP(rep, pCur->ladder.pos.x, 10.0f, 100.0f);
        break;
    case 0xA:
        FSTEP(rep, pCur->ladder.pos.y, 10.0f, 100.0f);
        break;
    case 0xB:
        FSTEP(rep, pCur->ladder.pos.z, 10.0f, 100.0f);
        break;
    case 0xC:
        FSTEP(rep, pCur->ladder.angle, PI / 256.0f, PI / 32.0f);
        ANG_CLAMP(pCur->ladder.angle);
        break;
    case 0xD:
        n = pCur->ladder.level;
        STEP(rep, n);
        pCur->ladder.level = n;
        break;
    case 0xE:
        n = pCur->ladder.cut1;
        STEP(rep, n);
        pCur->ladder.cut1 = n;
        break;
    case 0xF:
        n = pCur->ladder.cut2;
        STEP(rep, n);
        pCur->ladder.cut2 = n;
        break;
    case 0x10:
        n = pCur->ladder.cut3;
        STEP(rep, n);
        pCur->ladder.cut3 = n;
        break;
    }
    x = pW->x + 0x80;
    y = pW->y;
    y += 0x10;
    eprintf(x, y, 0, 0, "%.0f", pCur->ladder.pos.x);
    y += 0x10;
    eprintf(x, y, 0, 0, "%.0f", pCur->ladder.pos.y);
    y += 0x10;
    eprintf(x, y, 0, 0, "%.0f", pCur->ladder.pos.z);
    y += 0x10;
    eprintf(x, y, 0, 0, "%2.2f", pCur->ladder.angle);
    y += 0x10;
    eprintf(x, y, 0, 0, "%d [m]", pCur->ladder.level);
    y += 0x10;
    if (pCur->ladder.cut1) {
        eprintf(x, y, 0, 0, "%d", pCur->ladder.cut1 - 1);
    } else {
        eprintf(x, y, 0, 0, "no cam");
    }
    y += 0x10;
    if (pCur->ladder.cut2) {
        eprintf(x, y, 0, 0, "%d", pCur->ladder.cut2 - 1);
    } else {
        eprintf(x, y, 0, 0, "no cam");
    }
    y += 0x10;
    if (pCur->ladder.cut3) {
        eprintf(x, y, 0, 0, "%d", pCur->ladder.cut3 - 1);
    } else {
        eprintf(x, y, 0, 0, "no cam");
    }
    pW->y = y + 0x10;
}

static void tSceAtDataInput_ladder_ETedit()
{
    TSceAtLadder* l = (TSceAtLadder*) pCur->data;
    Vec center;

    switch (pW->step2) {
    case 0:
        AreaGetCenterPos(&center, &pCur->area);
        AreaDataInit(&pW->editArea, &center, AREA_TYPE_EYE, 100.0f, 1000.0f);
        if (l->posSet == 0) {
            l->posSet = 1;
        } else {
            pW->editArea.u.eye.x = l->pos.x;
            pW->editArea.u.eye.y = l->pos.y;
            pW->editArea.u.eye.z = l->pos.z;
        }
        pW->step2++;
    case 1:
        POINT_EDIT(l->pos, 0.5f);
        break;
    }
}

// position, direction and the two side arrows of a ladder / position jump point
#define POINT_DIR_DISP(P, SET)                                                  \
    Mtx m;                                                                      \
    Vec v = {0.0f, 0.0f, 300.0f};                                               \
    Vec r;                                                                      \
    Vec rot = {0.0f, 0.0f, 0.0f};                                               \
    u32 col;                                                                    \
                                                                                \
    rot.y = P->angle;                                                           \
    r = rot;                                                                    \
    if (P->SET != 0) {                                                          \
        low_RotMatrix(m, &r);                                                   \
        TransMatrix(m, &P->pos);                                                \
        PSMTXMultVec(m, &v, &rot);                                              \
        col = 0x408040;                                                         \
        if (cur == 1) col = 0xA0FFA0;                                           \
        Draw_sphere(&P->pos, 150.0f, (col << 8) | 0xFF, 1, 1);                  \
        Draw_sphere(&P->pos, 80.0f, 0x808040FF, 0, 0);                          \
        Draw_sphere(&rot, 50.0f, (col << 8) | 0xFF, 0, 0);                      \
        v.z = 1000.0f;                                                          \
        PSMTXMultVec(m, &v, &rot);                                              \
        Draw_line3d(&P->pos, &rot, col | 0xFE000000, 0);                        \
        v.z = 800.0f;                                                           \
        r.y += PI / 2.0f;                                                       \
        low_RotMatrix(m, &r);                                                   \
        TransMatrix(m, &P->pos);                                                \
        PSMTXMultVec(m, &v, &rot);                                              \
        Draw_line3d(&P->pos, &rot, col | 0xFE000000, 0);                        \
        r.y -= PI;                                                              \
        low_RotMatrix(m, &r);                                                   \
        TransMatrix(m, &P->pos);                                                \
        PSMTXMultVec(m, &v, &rot);                                              \
        Draw_line3d(&P->pos, &rot, col | 0xFE000000, 0);                        \
    }

void tSceAtLadderDataDisp(TSceAtLadder* l, int cur)
{
    POINT_DIR_DISP(l, posSet);
}

static TOOL_MENU tSceAtUseMenu[9] = {
    {1, "ID", NULL},
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "TRIGGER_TYPE", NULL},
    {0, " ACTION TYPE", NULL},
    {1, "TARGET TYPE", NULL},
    {1, "PRIORITY", NULL},
    {1, " USE_ID", NULL},
};

static void tSceAtDataInput_use()
{
    TSceAtValue* d = (TSceAtValue*) pCur->data;
    int n;
    int m;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceAtUseMenu, sizeof(tSceAtUseMenu), &Joy[0]);
    tSceAtDataInput_basic_menu(pW->inputCursor, tSceAtUseMenu);
    if (pW->inputCursor == 8) {
        n = d->value;
        if (Joy[0].rep2 & JOY_RIGHT) n++;
        if (Joy[0].rep2 & JOY_LEFT) n--;
        if (Joy[0].rep2 & 0x20000) n += 0x10;
        if (Joy[0].rep2 & 0x10000) n -= 0x10;
        CLAMP(n, m, 0xFE);
        d->value = m;
    }
    eprintf((s16) (pW->x + 0x80), pW->y, 0, 0, "%x", d->value);
}

static void tSceAtDataInput_hide()
{
    void (*routine[3])() = {tSceAtDataInput_hide_main, tSceAtDataInput_hide_pos_edit, tSceAtDataInput_hide_area_edit};

    routine[pW->step]();
}

static TOOL_MENU tSceAtHideMenu[12] = {
    {1, "ID", NULL},
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "TRIGGER_TYPE", NULL},
    {0, " ACTION TYPE", NULL},
    {1, "TARGET TYPE", NULL},
    {1, "PRIORITY", NULL},
    {1, " HIDE_TYPE", NULL},
    {1, " HIDE_POS_SET", NULL},
    {1, " HIDE_AREA", NULL},
    {1, " CAM_NO", NULL},
};

static void tSceAtDataInput_hide_main()
{
    s16 x;
    s16 y;
    int n;
    int m;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceAtHideMenu, sizeof(tSceAtHideMenu), &Joy[0]);
    tSceAtDataInput_basic_menu(pW->inputCursor, tSceAtHideMenu);
    switch (pW->inputCursor) {
    case 8:
        n = pCur->hide.mode;
        STEP(rep, n);
        CLAMP(n, m, 3);
        pCur->hide.mode = m;
        break;
    case 9:
        if (Joy[0].trg & JOY_A) pW->step = 1;
        break;
    case 0xA:
        if (Joy[0].trg & JOY_A) pW->step = 2;
        break;
    case 0xB:
        n = pCur->hide.cut;
        STEP(rep, n);
        CLAMP(n, m, 0xFF);
        pCur->hide.cut = m;
        break;
    }
    x = pW->x + 0x80;
    y = pW->y;
    eprintf(x, y, 0, 0, "%d", pCur->hide.mode);
    y += 0x30;
    if (pCur->hide.cut == 0) {
        eprintf(x, y, 0, 0, "no cam");
    } else {
        eprintf(x, y, 0, 0, "%d", pCur->hide.cut - 1);
    }
    pW->y = y + 0x10;
}

static void tSceAtDataInput_hide_pos_edit()
{
    TSceAtHide* h = (TSceAtHide*) pCur->data;
    Vec center;

    switch (pW->step2) {
    case 0:
        AreaGetCenterPos(&center, &pCur->area);
        AreaDataInit(&pW->editArea, &center, AREA_TYPE_EYE, 100.0f, 1000.0f);
        if (h->posSet == 0) {
            h->posSet = 1;
        } else {
            pW->editArea.u.eye.x = h->pos.x;
            pW->editArea.u.eye.y = h->pos.y;
            pW->editArea.u.eye.z = h->pos.z;
        }
        pW->step2++;
    case 1:
        POINT_EDIT(h->pos, 0.5f);
        break;
    }
}

#define EDIT_PTS (*(AreaXZ4Pts*) pW->editArea.u.xz4.p)

static void tSceAtDataInput_hide_area_edit()
{
    TSceAtHide* h = (TSceAtHide*) pCur->data;
    Vec center;

    switch (pW->step2) {
    case 0:
        AreaGetCenterPos(&center, &pCur->area);
        AreaDataInit(&pW->editArea, &center, AREA_TYPE_XZ4, 1500.0f, 1000.0f);
        if (h->areaSet == 0) {
            h->areaSet = 1;
        } else {
            EDIT_PTS = h->pts;
        }
        pW->step2++;
    case 1:
        AreaDataEdit(&pW->editArea, 0xA0FF8080, 0, NULL, 0.5f);
        AreaDataDisp(&pW->editArea, 0xA0FF8080, 1, NULL);
        AreaDataInfoDisp(&pW->editArea, pW->x, pW->y);
        AreaDataHelpDisp(&pW->editArea, (s16) (pW->x + 0xE0), (s16) (pW->y - 0x20));
        h->pts = EDIT_PTS;
        if (Joy[0].trg & JOY_B) {
            pW->step = 0;
            pW->step2 = 0;
            Joy[0].trg = 0;
        }
        break;
    }
}

void tSceAtHideDataDisp(TSceAtHide* h, int cur)
{
    AreaData a;
    u32 col = 0x408040;

    if (cur == 1) col = 0xA0FFA0;
    if (h->posSet == 1) {
        AreaDataInit(&a, &h->pos, AREA_TYPE_EYE, 100.0f, 1000.0f);
        AreaDataDisp(&a, col, 1, NULL);
    }
    if (h->areaSet == 1) {
        AreaDataInit(&a, &h->pos, AREA_TYPE_XZ4, 1500.0f, 1000.0f);
        *(AreaXZ4Pts*) a.u.xz4.p = h->pts;
        AreaDataDisp(&a, col, 1, NULL);
    }
}

static void tSceAtDataInput_pos_jump()
{
    void (*routine[2])() = {tSceAtDataInput_pos_jump_main, tSceAtDataInput_pos_jump_ETedit};

    routine[pW->step]();
}

static TOOL_MENU tSceAtPosJumpMenu[13] = {
    {1, "ID", NULL},
    {1, "HIT TYPE", NULL},
    {0, " HIT ANGLE", NULL},
    {0, " OPEN ANGLE", NULL},
    {1, "TRIGGER_TYPE", NULL},
    {0, " ACTION TYPE", NULL},
    {1, "TARGET TYPE", NULL},
    {1, "PRIORITY", NULL},
    {1, " JUMP_POS_SET", NULL},
    {1, "  JUMP_POS_X", NULL},
    {1, "  JUMP_POS_Y", NULL},
    {1, "  JUMP_POS_Z", NULL},
    {1, " JUMP_ANG", NULL},
};

#define PJ ((TSceAtPosJump*) pCur->data)

static void tSceAtDataInput_pos_jump_main()
{
    s16 x;
    s16 y;

    ToolMenuDisp_cur(pW->x, pW->y, 0, &pW->inputCursor, tSceAtPosJumpMenu, sizeof(tSceAtPosJumpMenu), &Joy[0]);
    tSceAtDataInput_basic_menu(pW->inputCursor, tSceAtPosJumpMenu);
    switch (pW->inputCursor) {
    case 8:
        if (Joy[0].trg & JOY_A) pW->step = 1;
        break;
    case 9:
        FSTEP(rep, pCur->jumpPos.x, 10.0f, 100.0f);
        break;
    case 0xA:
        FSTEP(rep, pCur->jumpPos.y, 10.0f, 100.0f);
        break;
    case 0xB:
        FSTEP(rep, pCur->jumpPos.z, 10.0f, 100.0f);
        break;
    case 0xC:
        FSTEP(rep, PJ->angle, PI / 256.0f, PI / 32.0f);
        ANG_CLAMP(PJ->angle);
        break;
    }
    x = pW->x + 0x80;
    y = pW->y;
    y += 0x10;
    eprintf(x, y, 0, 0, "%.0f", pCur->jumpPos.x);
    y += 0x10;
    eprintf(x, y, 0, 0, "%.0f", pCur->jumpPos.y);
    y += 0x10;
    eprintf(x, y, 0, 0, "%.0f", pCur->jumpPos.z);
    y += 0x10;
    eprintf(x, y, 0, 0, "%2.2f", PJ->angle);
    pW->y = y + 0x10;
}

static void tSceAtDataInput_pos_jump_ETedit()
{
    TSceAtPosJump* j = (TSceAtPosJump*) pCur->data;
    Vec center;

    switch (pW->step2) {
    case 0:
        AreaGetCenterPos(&center, &pCur->area);
        AreaDataInit(&pW->editArea, &center, AREA_TYPE_EYE, 100.0f, 1000.0f);
        if (j->posSet == 0) {
            j->posSet = 1;
        } else {
            pW->editArea.u.eye.x = j->pos.x;
            pW->editArea.u.eye.y = j->pos.y;
            pW->editArea.u.eye.z = j->pos.z;
        }
        pW->step2++;
    case 1:
        POINT_EDIT(j->pos, 0.5f);
        break;
    }
}

void tSceAtPosJumpDataDisp(TSceAtPosJump* j, int cur)
{
    POINT_DIR_DISP(j, posSet);
}

// marks a point with the floor below it
void tSceAt_PointDisp(f32 x, f32 y, f32 z)
{
    Vec p;
    Vec f;

    p.x = x;
    p.y = y;
    p.z = z;
    Draw_sphere(&p, 100.0f, 0xFFFFFF80, 1, 1);
    f = p;
    f.y = SatMgr.getFloor(&p, 600.0f, 100000.0f, NULL, 0);
    Draw_line3d(&p, &f, 0x80808020, 0);
    p = f;
    p.x += 200.0f;
    f.x -= 200.0f;
    Draw_line3d(&p, &f, 0x80808020, 0);
    p.x -= 200.0f;
    f.x += 200.0f;
    p.z += 200.0f;
    f.z -= 200.0f;
    Draw_line3d(&p, &f, 0x80808020, 0);
}

static TOOL_MENU tSceAtLoadMenu[3] = {
    {1, "SERVER", NULL},
    {0, "LOCAL", NULL},
    {1, "don't load", NULL},
};

static void tSceAtDataLoad()
{
    s8 sel;
    int cmp;

    eprintf(pW->x, pW->y, 4, 0, "[DATA LOAD]");
    pW->y += 0x10;
    switch (pW->sub) {
    case 0:
        sel = ToolMenuDisp(pW->x, pW->y, 0, tSceAtLoadMenu, sizeof(tSceAtLoadMenu), &Joy[0]);
        eprintf(pW->x + 0x5A, pW->y, 6, 0, "%s", pW->pathX);
        eprintf(pW->x + 0x5A, pW->y + 0x10, 6, 0, "%s", pW->path);
        if (sel >= 0) {
            int ret = 0;
            switch (sel) {
            case 0:
                ret = HDRead(pW->pathX, &pW->file);
                break;
            case 1:
                ret = HDRead(pW->path, &pW->file);
                break;
            case 2:
                pW->mode = ret;
                pW->sub = ret;
                pW->step = ret;
                pW->step2 = ret;
                return;
            }
            pW->timer = 30;
            if (ret != 0) {
                pW->sub = 1;
                pW->step = 0;
                pW->step2 = 0;
            } else {
                pW->sub = 9;
                pW->step = ret;
                pW->step2 = ret;
            }
        }
        if (Joy[0].trg & JOY_B) {
            MODE_RESET();
        }
        break;
    case 1:
        if (pW->file.head.version != 0x104) {
            pW->sub = 9;
            pW->step = 0;
            pW->step2 = 0;
        } else {
            cmp = strcmp(pW->file.head.magic, "AEV");
            if (cmp != 0) {
                pW->sub = 9;
                pW->step = 0;
                pW->step2 = 0;
            } else {
                tSceAtLoadDataCopy();
                pW->sub = 8;
                pW->step = cmp;
                pW->step2 = cmp;
            }
        }
        break;
    case 8:
        pW->loaded = 1;
        eprintf(pW->x, pW->y, 6, 0, "DATA LOAD COMPLETE.");
        if ((Joy[0].trg & (JOY_A | JOY_B)) || pW->timer <= 0) {
            MODE_RESET();
        }
        pW->timer--;
        break;
    case 9:
        eprintf(pW->x, pW->y, 2, 0, "DATA LOAD ERROR.");
        if ((Joy[0].trg & (JOY_A | JOY_B)) || pW->timer <= 0) {
            SUB_RESET();
        }
        pW->timer--;
        break;
    }
}

// spreads the loaded records over the area table by their number
void tSceAtLoadDataCopy()
{
    u32 i;

    memclr_asm(pW->area, sizeof(pW->area));
    for (i = 0; i < pW->file.head.num; i++) {
        pW->area[pW->file.work[i].no] = pW->file.work[i];
    }
}

static TOOL_MENU tSceAtSaveMenu[3] = {
    {1, "SERVER", NULL},
    {1, "LOCAL", NULL},
    {1, "don't save", NULL},
};

static void tSceAtDataSave()
{
    int ret = 0;
    int size;
    TSceAtFile* p;

    eprintf(pW->x, pW->y, 4, 0, "[DATA SAVE]");
    pW->y += 0x10;
    switch (pW->sub) {
    case 0:
        pW->sub = 1;
        pW->step = ret;
        pW->step2 = ret;
    case 1:
        pW->saveSel = ToolMenuDisp(pW->x, pW->y, 0, tSceAtSaveMenu, sizeof(tSceAtSaveMenu), &Joy[0]);
        eprintf(pW->x + 0x64, pW->y, 6, 0, "%s", pW->pathX);
        eprintf(pW->x + 0x64, pW->y + 0x10, 6, 0, "%s", pW->path);
        if (pW->saveSel >= 0) {
            if (pW->saveSel == 2) {
                MODE_RESET();
            } else {
                pW->sub = 2;
                pW->step = 0;
                pW->step2 = 0;
            }
        }
        if (Joy[0].trg & JOY_B) {
            MODE_RESET();
        }
        break;
    case 2:
        tSceAtSaveDataCreate();
        size = pW->saveNum * sizeof(SceAtWork) + sizeof(TSceAtFileHead);
        switch (pW->saveSel) {
        case 0:
            ret = HDWrite(pW->pathX, &pW->file, size);
            break;
        case 1:
            ret = HDWrite(pW->path, &pW->file, size);
            break;
        }
        if (SceAtSys.x11C == 1) Mem_free(SceAtSys.pAtData);
#line 3043 "D:/Bio4/Prog/t_sce_at.cpp"
        p = (TSceAtFile*) MEM_ALLOC(size, 1, 0xD);
        memcpy(p, &pW->file, size);
        SceAtInit(p, SceAtSys.pItemData);
        SceAtRoomSet();
        SceAtSys.x11C = 1;
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

// packs the live areas (type 3 dropped) into the file image
void tSceAtSaveDataCreate()
{
    u32 i;

    pW->saveNum = 0;
    for (i = 0; i < AREA_NUM; i++) {
        if ((pW->area[i].flag & 1) && pW->area[i].x35 != 3) {
            pW->area[i].no = i;
            pW->area[i].func = NULL;
            pW->area[i].arg = 0;
            pW->area[i].prioBak = 0;
            pW->area[i].pParent = NULL;
            pW->file.work[pW->saveNum] = pW->area[i];
            pW->saveNum++;
        }
    }
    pW->file.head.magic[0] = 'A';
    pW->file.head.magic[1] = 'E';
    pW->file.head.magic[2] = 'V';
    pW->file.head.magic[3] = 0;
    pW->file.head.version = 0x104;
    pW->file.head.num = pW->saveNum;
}

void tSceAtData_DebugCamera()
{
    CamDbg.move(&pG->Cam, &Joy[0], 0);
    pW->timer++;
    if (pW->timer & 8) {
        eprintf(0xD0, 0x10, 6, 0, "CAMERA MODE");
    }
}

static void tSceAtPreview()
{
    void (*routine[3])() = {tSceAtPreview_init, tSceAtPreview_main, tSceAtPreview_exit};

    routine[pW->sub]();
}

static void tSceAtPreview_init()
{
    TOOL_FLAG(OFS_STOP_FLG) &= ~0x10000000;
    TOOL_FLAG(OFS_DISP_FLG) &= ~0x40000000;
    TOOL_FLAG(OFS_DISP_FLG) &= ~0x80000000;
    TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x10000000;
    pW->sub = 1;
    pW->step = 0;
    pW->step2 = 0;
}

static void tSceAtPreview_main()
{
    tSceAtPreview_pl_pos();
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

// the player position and a point in front of him, lit when inside an area
void tSceAtPreview_pl_pos()
{
    Vec pos;
    Vec front;
    u32 i;
    int hit;
    int hitFront;

    pos = pPL->pos;
    pos.y += 250.0f;
    front.x = 0.0f;
    front.y = 250.0f;
    front.z = 550.0f;
    PSMTXMultVec(pPL->mat, &front, &front);
    SatMgr.hitCheck(&pos, &front, &front, NULL, 0, 0);
    hit = 0;
    hitFront = 0;
    for (i = 0; i < pW->head.num; i++) {
        if (pW->area[i].flag & 1) {
            if (AreaHitCheck(&pW->area[i].area, &pos) == 1) hit = 1;
            if (AreaHitCheck(&pW->area[i].area, &front) == 1) hitFront = 1;
        }
    }
    if (hit) {
        Draw_sphere(&pos, 150.0f, 0xFF404080, 0, 0);
    } else {
        Draw_sphere(&pos, 150.0f, 0xFFFFFF80, 0, 0);
    }
    if (hitFront) {
        Draw_sphere(&front, 150.0f, 0xFF404080, 0, 0);
    } else {
        Draw_sphere(&front, 150.0f, 0xFFFFFF80, 0, 0);
    }
    Draw_line3d(&pos, &front, 0xFF40FF80, 0);
    front = pos;
    front.y += 1000.0f;
    Draw_line3d(&pos, &front, 0xFF40FF80, 0);
}

static void tSceAtPreview_exit()
{
    TOOL_FLAG(OFS_STOP_FLG) |= 0x20000000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x10000000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x8000000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x800000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x400000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x10000;
    TOOL_FLAG(OFS_STOP_FLG) |= 0x2000;
    TOOL_FLAG(OFS_DISP_FLG) |= 0x40000000;
    TOOL_FLAG(OFS_DISP_FLG) |= 0x80000000;
    TOOL_FLAG(OFS_DEBUG_FLG) |= 0x10000000;
    MODE_RESET();
}

// reads the `#define MES_xxx` names of a message header into `names` (64 chars each); the count
int loadMesName(const char* path, char* names)
{
    void* buf;
    char* p;
    char* e;
    char* dst;
    u32 n;
    int len;
    int cmp;

    if (HDReadDebugAlloc(path, &buf, 1) == 0) return 0;
    p = (char*) buf;
    n = 0;
    while ((p = strstr(p, "#define")) != NULL) {
        p = space_skip(p + 7);
        cmp = strncmp(p, "MES_", 4);
        if (cmp != 0) continue;
        p += 4;
        e = strpbrk(p, "\t (/\r\n");
        if (e == NULL) break;
        len = e - p;
        if (len > 62) len = 63;
        dst = (char*) (n * 64 + (u32) names);
        memcpy(dst, p, len);
        dst[len] = cmp;
        n++;
        if (n > 511) {
            pLog->err(0, 0, "loadMesName:symbol num over!!");
            break;
        }
    }
    Debug_free(buf);
    return n;
}
