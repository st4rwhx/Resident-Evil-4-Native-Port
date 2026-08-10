#include "types.h"
#include "light.h"
#include "atari.h"
#include "global.h"
#include "main_mem.h"
#include "vec.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "view.h"
#include "db_cam.h"
#include "player.h"
#include "eprintf.h"
#include "math_sub.h"
#include "scheduler.h"
#include "file.h"
#include "db_light.h"
#include "t_camera.h"

// Camera tool entry object (D:/Bio4/Prog/t_camera.cpp): the task loop, menus and the camera / area
// editors. PARTIAL: the editors (tcEdit_select, tcEdit_area .. tcDrawRail, tcLoad, tcSave) are not
// written yet; see the report.

extern "C" {
void memclr_asm(void* p, u32 size);
void* memset(void* p, int c, unsigned int n);
}

static inline void LdatSet(TcLdat*& d, TcLdat* v) { d = v; }
struct TcWorkPtr {
    TcWork* p;
};
#define PTC (((TcWorkPtr*) &pTc)->p)
static inline void FAdd(f32& d, f32 v) { d += v; }
// element i of a Vec array through a raw address: the store aliases the tool pointer (pTc is reloaded after it)
#define VEC_ELEM(p, i) (*(f32*) ((u32) (p) + (i) * 12))
#define MTX_SET_COLUMNS(m, c0, c1, c2, c3)                                                    \
    (m)[0][0] = (c0).x; (m)[1][0] = (c0).y; (m)[2][0] = (c0).z;                               \
    (m)[0][1] = (c1).x; (m)[1][1] = (c1).y; (m)[2][1] = (c1).z;                               \
    (m)[0][2] = (c2).x; (m)[1][2] = (c2).y; (m)[2][2] = (c2).z;                               \
    (m)[0][3] = (c3).x; (m)[1][3] = (c3).y; (m)[2][3] = (c3).z

#define TC_ON (*(u32*) ((u8*) PTC + 0x11C))
#define TC_TRG (*(u32*) ((u8*) PTC + 0x120))
#define TC_REP (*(u32*) ((u8*) PTC + 0x128))

static void tcInit();
static void tcMenu();
static void tcEdit();
static void tcLoad();
static void tcSave();
static void tcQuit();
void tcSubMenu();
void tcEdit_select();
void tcEdit_area();
void tcDrawArea();
void tcEdit_camera();
void tcDrawOffset();
void tcDrawRail();
TcCdat* tcNextCdatPtr(s8 no, int dir);
TcAdat* tcNextAdatPtr(s8 area, int cam, int dir);
int head_suffix(s8 area);
int tail_suffix(s8 area);
int next_suffix(s8 cam, int dir);
int next_suffixI(int cam, int dir) asm("next_suffix__FSci"); // COMPILER-DIFF: #4 int-view (no truncation at the call site)
void tcCameraPullPoint(TcCdat* c);
void tcToolCameraMove(Camera* cam);
void tcPreviewOnOff(int on);

// camera type names (tcTypeTbl[..][0]) and the on/off pair
const char* tcTypeName[9] = {"FIX     ", "PAN     ", "TRACK   ", "RAIL PAN", "BEHIND  ", "FREE    ", "MOTION  ",
                             "UP CUT  ", "BESIDE  "};
const char* tcOnOff[2] = {"OFF", "ON"};
// menu positions: {x, y} in 8 / 14 pixel units for the main menu, sub menu, edit header, ...
int tcMenuPos[12] = {3, 2, 0x12, 0x11, 0x1C, 0x14, 0x28, 5, 0x28, 7, 0x1B, 0x12};
u8 tcTypeTbl[64][16];
TcAdat tcAdat[0x60];
TcCdat tcCdat[0x40];
TcLdat tcLdat[0x40];
TcWork tcWork;
TcWork* pTc = &tcWork;
static void (*tcRoutineTbl[6])() = {tcInit, tcMenu, tcEdit, tcLoad, tcSave, tcQuit};
static const char* tcMainMenuName[4] = {"EDIT", "LOAD", "SAVE", "EXIT"};

void ToolCamera()
{
    tcInit();
    for (;;) {
        TcWork* w = pTc;
        if (w->lightTool == 0) {
            memcpy((u8*) w + 0x10C, &Joy[0], sizeof(JOY));
            memcpy((u8*) pTc + 0x374, &Joy[1], sizeof(JOY));
            switch (PTC->mode) {
            case 0:
                if (PTC->preview == 0) {
                    tcRoutineTbl[PTC->routine]();
                    tcToolCameraMove(&PTC->cam);
                    tcCameraDebugMove();
                } else {
                    tcDataExport((u8*) g_pToolCamData);
                    CamCtrl.RoomDataRead((CameraDataHeader*) g_pToolCamData);
                    CamCtrl.Check();
                    tcPlayerMove();
                    CameraMove();
                    LightMgr.move();
                    PTC->x634 = CamCtrl.camera_no;
                    PTC->x635 = CamCtrl.area_no;
                    PTC->x636 = CamCtrl.x691;
                    tcGameCamera2ToolCamera();
                }
                if (TC_TRG & 0x1000) {
                    PTC->mode = 1;
                    PTC->previewReq = PTC->preview;
                }
                break;
            case 1:
                tcSubMenu();
                if (TC_TRG & 0x1200) {
                    PTC->mode = 0;
                }
                break;
            }
            tcCameraMove();
        } else {
            int ret = w->pLightTool->move();
            if (ret == 0) {
                delete PTC->pLightTool;
                PTC->lightTool = ret;
            }
        }
        PTC->blink++;
        TaskSleep(1);
    }
}

void tcDataInitialize()
{
    int i;

    for (i = 0x3F; i >= 0; i--) {
        tcTypeTbl[i][0] = 0;
    }
    for (i = 0x5F; i >= 0; i--) {
        tcAdat[i].enable = 0xFF;
    }
    for (i = 0x3F; i >= 0; i--) {
        tcCdat[i].enable = 0xFF;
    }
    for (i = 0x3F; i >= 0; i--) {
        tcLdat[i].enable = 0xFF;
    }
    PTC->adatNum = 0;
    PTC->cdatNum = 0;
    PTC->ldatNum = 0;
    for (i = 0; i < 0x40; i++) {
        PTC->adatTypeNum[i] = 0;
    }
}

static void tcInit()
{
    if (g_pToolCamData == 0) {
        g_pToolCamData = Debug_alloc(0x19000, 0);
    }
    BitOn(pG->flags_60, 0x20000000);
    BitOn(pG->flags_60, 0x10000000);
    BitOn(pG->flags_170, 0x400000);
    TaskSuspend(0);
    tcGameCameraStore();
    memclr_asm(pTc, sizeof(TcWork));
    tcGameCamera2ToolCamera();
    PTC->routine = 1;
    PTC->x637 = pSys->key_type;
    tcDataInitialize();
    PTC->cdatNo = -1;
    PTC->adatNo = -1;
    PTC->x5E1 = -1;
    PTC->x634 = CamCtrl.camera_no;
    PTC->x635 = CamCtrl.area_no;
    PTC->x636 = CamCtrl.x691;
    if (CamCtrl.data) {
        if (cameraDataVersion((char*) CamCtrl.data) > 1) {
            tcDataImport((u8*) CamCtrl.data);
        }
    }
}

static void tcMenu()
{
    int x = tcMenuPos[0];
    int y = tcMenuPos[1];
    int i;
    int row;

    if (TC_REP & 0xC) {
        PTC->blink = 8;
    }
    if (TC_REP & 0x8) {
        PTC->cursor--;
    }
    if (TC_REP & 0x4) {
        PTC->cursor++;
    }
    PTC->cursor = PTC->cursor < 0 ? 3 : (PTC->cursor > 3 ? 0 : PTC->cursor);
    if (TC_TRG & 0x200) {
        PTC->blink = 8;
        PTC->cursor = 3;
    }
    if (TC_TRG & 0x100) {
        PTC->editSel = 1;
        switch (PTC->cursor) {
        case 0:
            PTC->routine = 2;
            break;
        case 1:
            PTC->routine = 3;
            break;
        case 2:
            PTC->routine = 4;
            break;
        case 3:
            PTC->routine = 5;
            break;
        }
    }
    eprintf(x * 8, y * 14, 4, 0, "Main Menu --- Tool Ver %1.2f", 1.1f);
    y++;
    for (i = 0; i < 4; i++) {
        if (i == PTC->cursor && (PTC->blink & 0x18)) {
            eprintf((x - 1) * 8, y * 14, 0, 0, ">");
        }
        eprintf(x * 8, y * 14, 0, 0, "%s", tcMainMenuName[i]);
        y++;
    }
}

void tcSubMenu()
{
    TcMenu menu[6] = {{1, "PREVIEW    :"}, {1, "LIGHT TOOL :"}, {1, "VIEW MODE  :"},
                      {1, "AREA DETAIL:"}, {1, "BATTLE CAM :"}, {1, "PROJECTION :"}};
    int x = tcMenuPos[2];
    int y = tcMenuPos[3];
    int i;

    eprintf(x * 8, y * 14, 5, 0, "SUB MENU ---------");
    y++;
    tcMenuSelect(x * 8, y * 14, 0, menu, 6, &PTC->subCursor);
    if (TC_TRG & 0x100) {
        PTC->mode = 0;
    }
    switch (PTC->subCursor) {
    case 0:
        if (TC_TRG & 0x1) {
            PTC->previewReq = 1;
        }
        if (TC_TRG & 0x2) {
            PTC->previewReq = 0;
        }
        if (TC_TRG & 0x100) {
            tcPreviewOnOff(PTC->previewReq);
        }
        break;
    case 1:
        if (TC_TRG & 0x100) {
            PTC->pLightTool = new cLightTool();
            PTC->lightTool = 1;
        }
        break;
    case 2:
        if (TC_TRG & 0x3) {
            PTC->viewMode = PTC->viewMode == 0;
            if (PTC->viewMode == 0) {
                tcCameraPullPoint(tcCdatPtr(PTC->cdatNo));
            }
        }
        break;
    case 3:
        if (TC_TRG & 0x1) {
            PTC->areaDetail = 1;
        }
        if (TC_TRG & 0x2) {
            PTC->areaDetail = 0;
        }
        break;
    case 4:
        if (pG->flags_6C & 0x40000000) {
            if (TC_TRG & 0x2) {
                pG->flags_6C &= ~0x40000000;
            }
        } else {
            if (TC_TRG & 0x1) {
                pG->flags_6C |= 0x40000000;
            }
        }
        break;
    case 5:
        switch (CameraGetProjection()) {
        case 1:
            if (TC_TRG & 0x3) {
                CameraSetProjection(2);
            }
            break;
        case 2:
            if (TC_TRG & 0x3) {
                CameraSetProjection(1);
            }
            break;
        }
        break;
    }
    x += 12;
    for (i = 0; i < 6; i++) {
        switch (i) {
        case 0:
            if (PTC->previewReq == 0) {
                eprintf(x * 8, y * 14, 0, 0, "---/OFF");
            } else {
                eprintf(x * 8, y * 14, 0, 0, "ON-/---");
            }
            break;
        case 1:
            break;
        case 2:
            if (PTC->viewMode) {
                eprintf(x * 8, y * 14, 0, 0, "------------/WORKING VIEW");
            } else {
                eprintf(x * 8, y * 14, 0, 0, "CAMERA VIEW-/------------");
            }
            break;
        case 3:
            if (PTC->areaDetail == 0) {
                eprintf(x * 8, y * 14, 0, 0, "---/OFF");
            } else {
                eprintf(x * 8, y * 14, 0, 0, "ON-/---");
            }
            break;
        case 4:
            if (pG->flags_6C & 0x40000000) {
                eprintf(x * 8, y * 14, 0, 0, "ON-/---");
            } else {
                eprintf(x * 8, y * 14, 0, 0, "---/OFF");
            }
            break;
        case 5:
            if (CameraGetProjection() == 1) {
                eprintf(x * 8, y * 14, 0, 0, "PERSPECTIVE");
            } else {
                eprintf(x * 8, y * 14, 0, 0, "ORTHOGRAPHIC");
            }
            break;
        }
        y++;
    }
}

static void tcEdit()
{
    TcWork* w = pTc;
    int x;
    int y;

    switch (w->editMode) {
    case 0:
        tcEdit_select();
        break;
    case 1:
        if (TC_ON & 0x10) {
            if (w->editSel == 1) {
                TcCdat* c = tcCdatPtr(w->cdatNo);
                if (TC_REP & 0x1) {
                    c = tcNextCdatPtr(PTC->cdatNo, -1);
                }
                if (TC_REP & 0x2) {
                    c = tcNextCdatPtr(PTC->cdatNo, 1);
                }
                PTC->cdatNo = c->cam_no;
                PTC->adatNo = PTC->cdatNo;
                PTC->x5E1 = head_suffix(PTC->adatNo);
                x = tcMenuPos[4];
                y = tcMenuPos[5];
                eprintf(x * 8, y * 14, 5, 0, "Camera[  ]");
                x += 7;
                eprintf(x * 8, y * 14, 0, 0, "%02d", PTC->cdatNo);
            } else if (w->editSel == 0) {
                if (TC_REP & 0x1) {
                    PTC->pAdat = tcNextAdatPtr(w->adatNo, w->x5E1, -1);
                }
                if (TC_REP & 0x2) {
                    PTC->pAdat = tcNextAdatPtr(PTC->adatNo, PTC->x5E1, 1);
                }
                PTC->adatNo = PTC->pAdat->area_no;
                PTC->x5E1 = PTC->pAdat->cam_no;
                PTC->cdatNo = PTC->adatNo;
                x = tcMenuPos[4];
                y = tcMenuPos[5];
                if ((s8) PTC->adatTypeNum[PTC->adatNo] <= 1) {
                    eprintf(x * 8, y * 14, 5, 0, "Area[  ]");
                    x += 5;
                    eprintf(x * 8, y * 14, 0, 0, "%02d", PTC->adatNo);
                } else {
                    eprintf(x * 8, y * 14, 5, 0, "Area[    ]");
                    x += 5;
                    eprintf(x * 8, y * 14, 0, 0, "%02d-%1d", PTC->adatNo, PTC->x5E1);
                }
            }
            if (PTC->editSel == 1 && (s8) PTC->viewMode == 0) {
                static s8 lastCam = 0;
                if (lastCam != PTC->cdatNo) {
                    TcAdat* a;
                    int no;
                    s8 n;
                    lastCam = PTC->cdatNo;
                    PTC->x2 = 0;
                    PTC->x6 = 0;
                    PTC->x627 = 0;
                    tcCameraPullPoint(tcCdatPtr(PTC->cdatNo));
                    no = PTC->cdatNo;
                    n = no;
                    a = tcAdatPtr(n, (s8) head_suffix(n));
                    if (!(a->attr & 0x80)) {
                        LightMgr.update(no, -1);
                    }
                }
            }
        } else {
            switch (w->editSel) {
            case 1:
                tcEdit_camera();
                break;
            case 0:
                tcEdit_area();
                break;
            }
        }
        break;
    }
    tcDrawArea();
    tcDrawOffset();
    tcDrawRail();
}

// data selection screen: the camera / area / link list, the FLAG/COPY/DELETE menu (selMode 1), the
// link editor (selMode 2) and the camera attribute editor (selMode 3)
void tcEdit_select()
{
    TcMenu menu[3] = {{1, "FLAG"}, {1, "COPY"}, {1, "DELETE"}};
    static s8 cursor = 0;   // column: 0 camera, 1 attribute, 2 link, 3.. area suffix
    static s8 no = 0;       // row: camera / area number
    static s8 blink = 8;
    static const char* attrShort[8] = {"NORMAL:", "BATTLE:", "EVENT :", "DOOR  :", "ONCE  :", "AHEAD :", "DIRECT:",
                                       "DISLGT:"};
    static const char* attrChar[8] = {"N", "B", "E", "D", "O", "A", "R", "L"};
    static int sel = 0;      // 0 camera data, 1 area data
    static int top = 0;      // first listed row
    static int keyMode = 0;  // 1: cursor keys move the row, 2: also the column (copy destination)
    static s8 copyCursor;
    static s8 copyNo;
    static int flag;
    int col2 = 0;
    int col = 0;
    int x;
    int y;
    int cx;
    s8 i;
    s8 j;
    s8 dst;
    s8 dstSfx;
    TcCdat* c;
    TcAdat* a;

    if (keyMode != 0) {
        if (TC_REP & 0x8) {
            no--;
        }
        if (TC_REP & 0x4) {
            no++;
        }
        no = no < 0 ? 0 : (no > 0x3F ? 0x3F : no);
        if (top + 4 < no) {
            top = no - 4;
        } else if (top > no) {
            top = no;
        }
        if (TC_REP & 0x1) {
            cursor--;
        }
        if (TC_REP & 0x2) {
            cursor++;
        }
        cursor = cursor < 0 ? 0 : (cursor > 10 ? 10 : cursor);
        if (keyMode == 2) {
            if (sel == 0) {
                cursor = cursor < 0 ? 0 : (cursor > 0 ? 0 : cursor);
            } else if (sel == 1) {
                cursor = cursor < 3 ? 3 : (cursor > 10 ? 10 : cursor);
            }
        }
    }
    keyMode = 0;
    switch (PTC->x2) {
    case 0:
        keyMode = 1;
        if (TC_TRG & 0x200) {
            PTC->routine = 1;
            return;
        }
        PTC->cdatNo = no;
        PTC->adatNo = no;
        if (cursor <= 2) {
            PTC->x5E1 = head_suffix(PTC->adatNo);
        } else {
            PTC->x5E1 = cursor - 3;
        }
        c = tcCdatPtr(PTC->cdatNo);
        a = tcAdatPtr(PTC->adatNo, PTC->x5E1);
        if (TC_TRG & 0x100) {
            switch (cursor) {
            case 0:
                if (c == 0) {
                    c = tcCdatNew();
                    tcCdatInit(c, PTC->cdatNo);
                }
                PTC->editSel = 1;
                PTC->editMode = 1;
                PTC->x6 = 0;
                if (c->type == 8) {
                    tcSetBesideCamera();
                }
                break;
            case 1:
                break;
            case 2:
                if (c != 0 && a != 0) {
                    PTC->x2 = 3;
                    PTC->x6 = 0;
                }
                break;
            default:
                if (a == 0) {
                    a = tcAdatNew();
                    if (PTC->adatTypeNum[PTC->adatNo] == 0) {
                        tcTypeTbl[PTC->adatNo][0] = 3;
                    }
                    tcAdatInit(a, PTC->adatNo, PTC->x5E1);
                }
                PTC->editSel = 0;
                PTC->editMode = 1;
                PTC->x6 = 0;
                break;
            }
            switch (cursor) {
            case 1:
            case 2:
                break;
            case 0:
            default:
                if (PTC->viewMode == 0 && c != 0) {
                    int cno;
                    s8 n;
                    tcCameraPullPoint(c);
                    cno = PTC->cdatNo;
                    n = cno;
                    a = tcAdatPtr(n, (s8) head_suffix(n));
                    if (a && !(a->attr & 0x80)) {
                        LightMgr.update(cno, -1);
                    }
                }
                break;
            }
        } else if (TC_TRG & 0x800) {
            if (cursor == 0) {
                if (c != 0) {
                    PTC->x2 = 1;
                    PTC->x3 = 0;
                    PTC->x6 = 0;
                    sel = 0;
                }
            } else if (cursor != 1) {
                if (a != 0) {
                    PTC->x2 = 1;
                    PTC->x3 = 0;
                    PTC->x6 = 0;
                    sel = 1;
                }
            }
        } else if (TC_TRG & 0x400) {
            switch (cursor) {
            case 0:
            case 1:
                break;
            default:
                if (a != 0) {
                    PTC->x2 = 2;
                    PTC->x3 = 0;
                    sel = 1;
                }
                break;
            }
        }
        break;
    case 1:
        c = tcCdatPtr(PTC->cdatNo);
        a = tcAdatPtr(PTC->adatNo, PTC->x5E1);
        switch (PTC->x3) {
        case 0:
            if (TC_TRG & 0x200) {
                PTC->x2 = 0;
                break;
            }
            blink = 0x18;
            switch (PTC->x6) {
            case 0:
                if (TC_TRG & 0x100) {
                    PTC->x2 = 0;
                }
                if (sel == 0) {
                    flag = c->enable;
                }
                if (sel == 1) {
                    flag = a->enable;
                }
                if (TC_TRG & 0x1) {
                    flag = 1;
                }
                if (TC_TRG & 0x2) {
                    flag = 0;
                }
                if (sel == 0) {
                    c->enable = flag;
                }
                if (sel == 1) {
                    a->enable = flag;
                }
                if (flag == 1) {
                    eprintf(0x30 * 8, 0x19 * 14, 0, 0, "ON-/---");
                } else {
                    eprintf(0x30 * 8, 0x19 * 14, 0, 0, "---/OFF");
                }
                break;
            case 1:
                if (TC_TRG & 0x100) {
                    copyCursor = cursor;
                    copyNo = no;
                    PTC->x3++;
                }
                if (sel == 0) {
                    eprintf(0x2B * 8, 0x1A * 14, 0, 0, "[%02d] -> ", PTC->cdatNo);
                } else if (sel == 1) {
                    eprintf(0x2B * 8, 0x1A * 14, 0, 0, "[%02d-%1d] -> ", PTC->adatNo, PTC->x5E1);
                }
                break;
            case 2:
                if (TC_TRG & 0x100) {
                    if (sel == 0) {
                        tcCdatDel(c);
                    } else if (sel == 1) {
                        tcAdatDel(a);
                        PTC->adatTypeNum[PTC->adatNo]--;
                    }
                    PTC->x2 = 0;
                }
                if (sel == 0) {
                    eprintf(0x2B * 8, 0x1B * 14, 0, 0, "[%02d]", PTC->cdatNo);
                } else if (sel == 1) {
                    eprintf(0x2B * 8, 0x1B * 14, 0, 0, "[%02d-%1d]", PTC->adatNo, PTC->x5E1);
                }
                break;
            }
            break;
        case 1: {
            s8 dstNo;
            keyMode = 2;
            if (TC_TRG & 0x200) {
                PTC->x3--;
                break;
            }
            dstNo = no;
            dst = dstNo;
            dstSfx = cursor - 3;
            if (TC_TRG & 0x100) {
                if (sel == 0) {
                    if (dst != PTC->cdatNo) {
                        TcCdat* src = tcCdatPtr(PTC->cdatNo);
                        TcCdat* n = tcCdatNew();
                        *n = *src;
                        n->cam_no = dstNo;
                    }
                } else if (sel == 1) {
                    if (dst != PTC->adatNo || dstSfx != PTC->x5E1) {
                        TcAdat* src = tcAdatPtr(PTC->adatNo, PTC->x5E1);
                        TcAdat* n = tcAdatNew();
                        if (PTC->adatTypeNum[dst] != 0) {
                            tcTypeTbl[dst][0] = 3;
                        }
                        *n = *src;
                        n->area_no = dstNo;
                        n->cam_no = dstSfx;
                        PTC->adatTypeNum[n->area_no]++;
                    }
                }
                PTC->x2 = 0;
                cursor = copyCursor;
                no = copyNo;
            }
            if (sel == 0) {
                eprintf(0x2B * 8, 0x1A * 14, 0, 0, "[%02d] -> [%02d]", PTC->cdatNo, dstNo);
            } else if (sel == 1) {
                eprintf(0x2B * 8, 0x1A * 14, 0, 0, "[%02d-%1d] -> [%02d-%1d]", PTC->adatNo, PTC->x5E1, dstNo, dstSfx);
            }
            break;
        }
        }
        switch (PTC->x3) {
        case 0:
            col = 0;
            break;
        case 1:
            col = 6;
            break;
        }
        tcMenuSelect(0x23 * 8, 0x19 * 14, col, menu, 3, &PTC->x6);
        break;
    case 2: {
        int x;
        int y;
        switch (PTC->x3) {
        case 0:
            keyMode = 2;
            if (TC_TRG & 0x200) {
                PTC->x2 = 0;
                break;
            }
            dstSfx = cursor - 3;
            dst = no;
            if (TC_TRG & 0x100) {
                if (tcAdatPtr(dst, dstSfx) != 0) {
                    LdatSet(PTC->pLdat, tcLdatPtr(PTC->adatNo, PTC->x5E1, dst, dstSfx));
                    if (PTC->pLdat == 0) {
                        LdatSet(PTC->pLdat, tcLdatNew());
                        tcLdatInit(PTC->pLdat, PTC->adatNo, PTC->x5E1, dst, dstSfx, 0);
                    }
                    PTC->x3++;
                }
            }
            break;
        case 1:
            if (TC_TRG & 0x200) {
                PTC->x3--;
                break;
            }
            if (TC_REP & 0x1) {
                PTC->pLdat->frame--;
            }
            if (TC_REP & 0x2) {
                PTC->pLdat->frame++;
            }
            PTC->pLdat->frame = PTC->pLdat->frame < 0 ? 0 : (PTC->pLdat->frame > 0x708 ? 0x708 : PTC->pLdat->frame);
            if (TC_TRG & 0x100) {
                if (PTC->pLdat->frame == 0) {
                    tcLdatDel(PTC->pLdat);
                }
                PTC->x2 = 0;
            }
            break;
        }
        x = 0x10;
        y = 0x14;
        eprintf(x * 8, y * 14, 0, 0, "-SRC- -DST- -FRM-");
        x++;
        y++;
        eprintf(x * 8, y * 14, 0, 0, "%02d-%1d", PTC->adatNo, PTC->x5E1);
        x += 6;
        for (i = 0; i < 0x40; i++) {
            if (tcLdat[i].enable != 0xFF) {
                TcLdat* l = &tcLdat[i];
                if (PTC->adatNo == l->area_from) {
                    if (PTC->x5E1 == l->cam_from) {
                        eprintf(x * 8, y * 14, 0, 0, "%02d-%1d", l->area_to, l->cam_to, tcLdat[i].frame);
                        eprintf((x + 7) * 8, y * 14, 0, 0, "%3d", tcLdat[i].frame);
                        if (PTC->pLdat == l) {
                            if (PTC->x3 == 0) {
                                eprintf((x - 1) * 8, y * 14, 0, 0, ">");
                            } else {
                                eprintf((x + 6) * 8, y * 14, 0, 0, ">");
                            }
                        }
                        y++;
                    }
                }
            }
        }
        break;
    }
    case 3: {
        s8 attr = tcTypeTbl[PTC->cdatNo][0];
        if (TC_TRG & 0x200) {
            PTC->x2 = 0;
            break;
        }
        if (TC_TRG & 0x100) {
            PTC->x2 = 0;
            break;
        }
        if (TC_REP & 0x8) {
            PTC->x6--;
        }
        if (TC_REP & 0x4) {
            PTC->x6++;
        }
        PTC->x6 = PTC->x6 < 0 ? 7 : (PTC->x6 > 7 ? 0 : PTC->x6);
        if (TC_TRG & 0x1) {
            attr |= 1 << PTC->x6;
        }
        if (TC_TRG & 0x2) {
            attr &= ~(1 << PTC->x6);
        }
        if (attr & 0x8) {
            attr |= 0x20;
        }
        if (attr & 0x4) {
            attr = 4;
        }
        tcTypeTbl[PTC->cdatNo][0] = attr;
        {
            int x = 0x10;
            int y = 0x14;
            eprintf(x * 8, y * 14, 5, 0, "-- ATTRIBUTE --");
            y++;
            for (i = 0; i < 8; i++) {
                if (PTC->x6 == i) {
                    eprintf((x - 1) * 8, y * 14, 0, 0, ">");
                }
                eprintf(x * 8, y * 14, 0, 0, "%s", attrShort[i]);
                if ((attr >> i) & 1) {
                    eprintf((x + 7) * 8, y * 14, 0, 0, "ON-/---");
                } else {
                    eprintf((x + 7) * 8, y * 14, 0, 0, "---/OFF");
                }
                y++;
            }
        }
        break;
    }
    }
    x = tcMenuPos[0];
    y = tcMenuPos[1];
    eprintf(x * 8, y * 14, 4, 0, "--- SELECT DATA ---");
    y++;
    eprintf(x * 8, y * 14, 0, 0, "CUT CAM_TYPE -ATTRIB- AREA------------------------------");
    y++;
    for (i = 0; i < 5; i++) {
        eprintf(x * 8, (y + i) * 14, 0x11, 0, "--- -------- -------- ---- ---- ---- ---- ---- ---- ----");
    }
    x++;
    for (i = top; i < top + 5; i++) {
        int on = 0;
        TcCdat* c = tcCdatPtr(i);
        if (c) {
            switch (c->enable) {
            case 0:
                col2 = 2;
                break;
            case 1:
                col2 = 0;
                break;
            }
            if (i != PTC->x634 || (pG->flags_51E4 & 0x18)) {
                on = 1;
            }
            if (on) {
                eprintf(x * 8, (y + i - top) * 14, (u8) col2, 0, "%02d", c->cam_no);
                eprintf((x + 3) * 8, (y + i - top) * 14, (u8) col2, 0, "%s", tcTypeName[c->type]);
                for (j = 0; j < 8; j++) {
                    int bit = 1 << j;
                    if (tcTypeTbl[i][0] & bit) {
                        eprintf((x + 12 + j) * 8, (y + i - top) * 14, (u8) col2, 0, "%s", attrChar[j]);
                    } else {
                        eprintf((x + 12 + j) * 8, (y + i - top) * 14, (u8) col2, 0, "-");
                    }
                }
            }
        }
    }
    for (i = top; i < top + 5; i++) {
        for (j = 0; j < 7; j++) {
            int on = 0;
            int xa = x + 0x15;
            TcAdat* a = tcAdatPtr(i, j);
            if (a) {
                switch (a->enable) {
                case 0:
                    col2 = 2;
                    break;
                case 1:
                    col2 = 0;
                    break;
                }
                if (i != PTC->x635 || j != PTC->x636 || (pG->flags_51E4 & 0x18)) {
                    on = 1;
                }
                if (on) {
                    if ((s8) PTC->adatTypeNum[i] <= 1) {
                        eprintf((xa + j * 5) * 8, (y + i - top) * 14, col2, 0, "%02d", i);
                    } else {
                        eprintf((xa + j * 5) * 8, (y + i - top) * 14, col2, 0, "%02d-%1d", i, j);
                    }
                }
            }
        }
    }
    x--;
    y += no - top;
    switch (cursor) {
    case 0:
        cx = 2;
        break;
    case 1:
        x += 3;
        cx = 8;
        break;
    case 2:
        x += 12;
        cx = 8;
        break;
    default:
        x += 0x15 + (cursor - 3) * 5;
        cx = 4;
        break;
    }
    asm("" : : "r"(x));  // COMPILER-DIFF: tie (global-alloc priority of x vs the cut pointer)
    if (blink & 0x18) {
        int xr;
        eprintf(x * 8, y * 14, 0, 0, ">");
        xr = x + 1;
        eprintf((xr + cx) * 8, y * 14, 0, 0, "<");
    }
    blink++;
}

TcCdat* tcCdatNew()
{
    int i;

    for (i = 0; i < 0x40; i++) {
        if (tcCdat[i].enable == 0xFF) {
            tcCdat[i].enable = 1;
            PTC->cdatNum++;
            return &tcCdat[i];
        }
    }
    return 0;
}

void tcCdatDel(TcCdat* c)
{
    c->enable = 0xFF;
    PTC->cdatNum--;
}

void tcCdatInit(TcCdat* c, int cam_no)
{
    Camera* cam = &PTC->cam;
    int i;

    c->type = 2;
    c->cam_no = cam_no;
    c->flags = 0;
    c->aim_ofs.x = 0.0f;
    c->aim_ofs.y = 1000.0f;
    c->aim_ofs.z = 0.0f;
    c->num = 1;
    for (i = 0; i < 2; i++) {
        c->at[i] = cam->param.at;
        c->pos[i] = cam->param.pos;
        c->roll[i] = cam->param.roll;
        c->fovy[i] = cam->param.fovy;
    }
    for (i = 25; i >= 0; i--) {
        c->frame[i] = 0;
    }
}

TcCdat* tcCdatPtr(int cam_no)
{
    int i;

    for (i = 0; i < 0x40; i++) {
        TcCdat* c = &tcCdat[i];
        if (c->enable != 0xFF && cam_no == c->cam_no) {
            return c;
        }
    }
    return 0;
}

TcAdat* tcAdatNew()
{
    int i;

    for (i = 0; i < 0x60; i++) {
        if (tcAdat[i].enable == 0xFF) {
            tcAdat[i].enable = 1;
            PTC->adatNum++;
            return &tcAdat[i];
        }
    }
    return 0;
}

void tcAdatDel(TcAdat* a)
{
    a->enable = 0xFF;
    PTC->adatNum--;
}

void tcAdatInit(TcAdat* a, int area_no, int cam_no)
{
    Mtx m;
    Vec axis = {0.0f, 1.0f, 0.0f};
    Vec pos;
    TcPoly* poly = &a->poly;
    int i;

    a->cam_no = cam_no;
    a->area_no = area_no;
    a->attr = 0;
    PTC->adatTypeNum[area_no]++;
    a->attr2 = 1;
    a->x9 = 0xFF;
    PSMTXIdentity(a->mat);
    a->height = 1000.0f;
    a->base_y = 0.0f;
    a->num = 4;
    poly->pt[0].z = 2000.0f;
    poly->pt[0].x = 2000.0f;
    poly->pt[1].z = -2000.0f;
    poly->pt[1].x = 2000.0f;
    poly->pt[2].z = -2000.0f;
    poly->pt[2].x = -2000.0f;
    poly->pt[3].z = 2000.0f;
    poly->pt[3].x = -2000.0f;
    pos = pPL->pos;
    PSMTXIdentity(m);
    PSMTXRotAxisRad(m, &axis, pPL->rot.y);
    PSMTXTransApply(m, m, pos.x, pos.y, pos.z);
    // COMPILER-DIFF: tie -- the phony do-while doubles the body's REG_N_REFS (the pt pointer giv then takes
    // r31 ahead of `a`/`poly`); the increment must sit INSIDE it so that its `addi` is not pinned behind
    // the call by the LOOP_END note (the target has `addi r31,r31,0xc` before the `bl`).
    for (i = 0; i < 4;) {
        do {
            PSMTXMultVec(m, &a->pt[i], &a->pt[i]);
            i++;
        } while (0);
    }
    poly->pt[0].y = poly->pt[1].y = poly->pt[2].y = poly->pt[3].y = a->base_y;
}

TcAdat* tcAdatPtr(int area_no, int cam_no)
{
    int i;
    TcAdat* a = tcAdat;

    for (i = 0; i < 0x60; i++, a++) {
        if (a->enable != 0xFF && area_no == a->area_no && cam_no == a->cam_no) {
            return a;
        }
    }
    return 0;
}

TcLdat* tcLdatNew()
{
    int i;

    for (i = 0; i < 0x40; i++) {
        if (tcLdat[i].enable == 0xFF) {
            tcLdat[i].enable = 1;
            PTC->ldatNum++;
            return &tcLdat[i];
        }
    }
    return 0;
}

void tcLdatDel(TcLdat* l)
{
    l->enable = 0xFF;
    PTC->ldatNum--;
}

void tcLdatInit(TcLdat* l, int area_from, int cam_from, int area_to, int cam_to, int frame)
{
    l->area_from = area_from;
    l->cam_from = cam_from;
    l->area_to = area_to;
    l->cam_to = cam_to;
    l->frame = frame;
}

TcLdat* tcLdatPtr(int area_from, int cam_from, int area_to, int cam_to)
{
    int i;
    TcLdat* l = tcLdat;

    for (i = 0; i < 0x40; i++, l++) {
        if (l->enable != 0xFF && area_from == l->area_from && cam_from == l->cam_from && area_to == l->area_to &&
            cam_to == l->cam_to) {
            return l;
        }
    }
    return 0;
}

void tcAreaMoveVertex(TcAdat* a, int mode);
void tcAreaSelectVertex(TcAdat* a);
void tcAreaSelectSide(TcAdat* a);
void tcAreaInsertVertex(TcAdat* a);
void tcAreaDeleteVertex(TcAdat* a);

#define PI2 6.2831855f

// area editor: vertex / floor / height / attribute / direction / character / address / camera link
void tcEdit_area()
{
    TcMenu menu[2] = {{1, "INS VERTEX:"}, {1, "DEL VERTEX:"}};
    static int vtxSave = 0;
    static const char* areaMenuName[8] = {"Vertex", "Floor", "Height", "Attrib", "Direct", "Char", "Address", "Edit-->"};
    static const char* attrLong[8] = {"NORMAL   ", "BATTLE   ", "EVENT    ", "DOOR     ", "ONCE     ", "AHEAD    ",
                                      "DIRECTION", "DIS LIGHT"};
    static const char* charName[8] = {"LEON", "LEON_ASHLEY", "ASHLEY", "ADA", "WESKER", "HUNK", "KLAUSER", "???????????"};
    static const char* addrName[8] = {"NORMAL", "HIGH", "GRENADE", "???????", "???????", "???????", "???????", "???????"};
    TcAdat* a = tcAdatPtr(PTC->adatNo, PTC->x5E1);
    TcAdat* ad;
    TcPoly* p;
    int x;
    int x2;
    int y;
    int i;

    switch (PTC->x2) {
    case 0:
        if (TC_TRG & 0x200) {
            PTC->editMode = 0;
            return;
        }
        if (!(TC_ON & 0x500)) {
            if (TC_TRG & 0x8) {
                PTC->x6--;
            }
            if (TC_TRG & 0x4) {
                PTC->x6++;
            }
            PTC->x6 = PTC->x6 < 0 ? 0 : (PTC->x6 > 7 ? 7 : PTC->x6);
            if (TC_TRG & 0xC) {
                PTC->x630 = 0;
            }
        }
        switch (PTC->x6) {
        case 0:
            if (TC_ON & 0x500) {
                if (TC_ON & 0x400) {
                    PTC->x625 = -1;
                }
                tcAreaMoveVertex(a, 0);
            } else {
                tcAreaSelectVertex(a);
            }
            if (TC_TRG & 0x800) {
                PTC->x2 = 1;
            }
            break;
        case 1:
            if (TC_ON & 0x500) {
                vtxSave = PTC->x625;
                PTC->x625 = -1;
                tcAreaMoveVertex(a, 1);
            } else {
                PTC->x625 = vtxSave;
            }
            break;
        case 2:
            if (TC_ON & 0x500) {
                tcAreaMoveVertex(a, 2);
            }
            break;
        case 3:
            if (TC_REP & 0x1) {
                PTC->x630++;
            }
            if (TC_REP & 0x2) {
                PTC->x630--;
            }
            PTC->x630 = PTC->x630 < 0 ? 0 : (PTC->x630 > 7 ? 7 : PTC->x630);
            if (TC_TRG & 0x100) {
                tcTypeTbl[a->area_no][0] ^= 1 << PTC->x630;
            }
            break;
        case 4:
            if (TC_REP & 0x1) {
                a->dir -= 0.09817477f;
            }
            if (TC_REP & 0x2) {
                a->dir += 0.09817477f;
            }
            while (a->dir >= PI) {
                a->dir -= PI2;
            }
            while (a->dir < -PI) {
                a->dir += PI2;
            }
            break;
        case 5:
            if (TC_REP & 0x1) {
                PTC->x630++;
            }
            if (TC_REP & 0x2) {
                PTC->x630--;
            }
            PTC->x630 = PTC->x630 < 0 ? 0 : (PTC->x630 > 7 ? 7 : PTC->x630);
            if (TC_TRG & 0x100) {
                a->attr2 ^= 1 << PTC->x630;
            }
            break;
        case 6:
            if (TC_REP & 0x1) {
                PTC->x630++;
            }
            if (TC_REP & 0x2) {
                PTC->x630--;
            }
            PTC->x630 = PTC->x630 < 0 ? 0 : (PTC->x630 > 7 ? 7 : PTC->x630);
            if (TC_TRG & 0x100) {
                a->x9 ^= 1 << PTC->x630;
            }
            break;
        case 7:
            if (TC_ON & 0x100) {
                if (tcCdatPtr(PTC->cdatNo)) {
                    PTC->editSel = 1;
                }
            }
            break;
        }
        break;
    case 1:
        if (TC_TRG & 0x200) {
            PTC->x2 = 0;
            break;
        }
        tcMenuSelect(tcMenuPos[6] * 8, tcMenuPos[7] * 14, 0, menu, 2, &PTC->x7);
        switch (PTC->x7) {
        case 0:
            tcAreaSelectSide(a);
            if (TC_TRG & 0x100) {
                tcAreaInsertVertex(a);
                PTC->x2 = 0;
            }
            break;
        case 1:
            tcAreaSelectVertex(a);
            if (TC_TRG & 0x100) {
                tcAreaDeleteVertex(a);
                PTC->x2 = 0;
            }
            break;
        }
        break;
    }
    ad = tcAdatPtr(PTC->adatNo, PTC->x5E1);
    p = &ad->poly;
    x = tcMenuPos[0];
    y = tcMenuPos[1];
    if ((s8) PTC->adatTypeNum[PTC->adatNo] <= 1) {
        eprintf(x * 8, y * 14, 5, 0, "<--- AREA EDIT [%02d] --->", PTC->adatNo);
    } else {
        eprintf(x * 8, y * 14, 5, 0, "<--- AREA EDIT [%02d-%1d] --->", PTC->adatNo, PTC->x5E1);
    }
    y++;
    x2 = x + 9;
    for (i = 0; i < 8; i++) {
        eprintf(x * 8, (y + i) * 14, i == PTC->x6 ? 4 : 0, 0, areaMenuName[i]);
    }
    for (i = 0; i < 8; i++) {
        switch (i) {
        case 0:
            if (PTC->x625 != -1) {
                eprintf(x2 * 8, y * 14, 0, 0, "V[%02d] = (%f, %f, %f)", PTC->x625, p->pt[PTC->x625].x, p->pt[PTC->x625].y,
                        p->pt[PTC->x625].z);
            } else {
                eprintf(x2 * 8, y * 14, 0, 0, "ALL VERTEX");
            }
            break;
        case 1:
            eprintf(x2 * 8, y * 14, 0, 0, "Y = %f", ad->base_y);
            break;
        case 2:
            eprintf(x2 * 8, y * 14, 0, 0, "H = %f", ad->height);
            break;
        case 3:
            eprintf(x2 * 8, y * 14, 0, 0, "%08x", BtoX(tcTypeTbl[ad->area_no][0]));
            if (PTC->x6 == 3) {
                eprintf((x2 + 7 - PTC->x630) * 8, y * 14, 4, 0, "%1x", BtoX((tcTypeTbl[ad->area_no][0] >> PTC->x630) & 1));
                eprintf((x2 + 9) * 8, y * 14, 0, 0, "[%10s]", attrLong[PTC->x630]);
            }
            break;
        case 4:
            eprintf(x2 * 8, y * 14, 0, 0, "(%f, %f, %f)", 0.0, ad->dir, 0.0);
            break;
        case 5:
            eprintf(x2 * 8, y * 14, 0, 0, "%08x", BtoX(ad->attr2));
            if (PTC->x6 == 5) {
                eprintf((x2 + 7 - PTC->x630) * 8, y * 14, 4, 0, "%1x", BtoX((ad->attr2 >> PTC->x630) & 1));
                eprintf((x2 + 9) * 8, y * 14, 0, 0, "[%11s]", charName[PTC->x630]);
            }
            break;
        case 6:
            eprintf(x2 * 8, y * 14, 0, 0, "%08x", BtoX(ad->x9));
            if (PTC->x6 == 6) {
                eprintf((x2 + 7 - PTC->x630) * 8, y * 14, 4, 0, "%1x", BtoX((ad->x9 >> PTC->x630) & 1));
                eprintf((x2 + 9) * 8, y * 14, 0, 0, "[%7s]", addrName[PTC->x630]);
            }
            break;
        case 7:
            if (tcCdatPtr(PTC->cdatNo) == 0) {
                eprintf(x2 * 8, y * 14, 0, 0, "Camera[XX]");
            } else {
                eprintf(x2 * 8, y * 14, 0, 0, "Camera[%02d]", PTC->cdatNo);
            }
            break;
        }
        y++;
    }
}

static f32 tcVertexStep = 200.0f;

void tcAreaMoveVertex(TcAdat* a, int mode)
{
    Vec d = {0.0f, 0.0f, 0.0f};
    TcPoly* p = &a->poly;
    int i;

    switch (mode) {
    case 0:
        if (TC_ON & 0x8) {
            d.y = tcVertexStep;
        }
        if (TC_ON & 0x4) {
            d.y = -tcVertexStep;
        }
        if (TC_ON & 0x1) {
            d.x = -tcVertexStep;
        }
        if (TC_ON & 0x2) {
            d.x = tcVertexStep;
        }
        if (d.x != 0.0f || d.y != 0.0f) {
            moveOnPlaneXZ(&d, &d);
            if (PTC->x625 != -1) {
                f32* px = &a->pt[0].x;
                f32* pz = &a->pt[0].z;
                VEC_ELEM(px, PTC->x625) += d.x;
                VEC_ELEM(pz, PTC->x625) += d.z;
            } else {
                for (i = 0; i < p->num; i++) {
                    p->pt[i].x += d.x;
                    p->pt[i].z += d.z;
                }
            }
        }
        break;
    case 1:
        if (TC_ON & 0x8) {
            d.y = tcVertexStep;
        }
        if (TC_ON & 0x4) {
            d.y = -tcVertexStep;
        }
        a->base_y += d.y;
        for (i = 0; i < p->num; i++) {
            p->pt[i].y = a->base_y;
        }
        break;
    case 2:
        if (TC_ON & 0x8) {
            d.y = tcVertexStep;
        }
        if (TC_ON & 0x4) {
            d.y = -tcVertexStep;
        }
        a->height += d.y;
        break;
    }
}

void tcAreaSelectVertex(TcAdat* a)
{
    if (TC_REP & 0x1) {
        PTC->x625--;
    }
    if (TC_REP & 0x2) {
        PTC->x625++;
    }
    PTC->x625 = PTC->x625 < 0 ? a->num - 1 : (PTC->x625 > a->num - 1 ? 0 : PTC->x625);
}

void tcAreaSelectSide(TcAdat* a)
{
    if (TC_REP & 0x1) {
        PTC->x626--;
    }
    if (TC_REP & 0x2) {
        PTC->x626++;
    }
    PTC->x626 = PTC->x626 < 0 ? a->num - 1 : (PTC->x626 > a->num - 1 ? 0 : PTC->x626);
}

void tcAreaInsertVertex(TcAdat* a)
{
    TcPoly* p = &a->poly;
    Vec v;
    int i;
    int i0;
    int i1;

    if (p->num <= 15) {
        for (i = p->num; i > PTC->x626; i--) {
            p->pt[i] = p->pt[i - 1];
        }
        p->num++;
        i0 = PTC->x626;
        if (i0 < 0) {
            i0 += p->num;
        }
        i1 = PTC->x626 + 2;
        if (i1 > p->num - 1) {
            i1 -= p->num;
        }
        PSVECAdd(&p->pt[i0], &p->pt[i1], &v);
        PSVECScale(&v, &p->pt[PTC->x626 + 1], 0.5f);
        PTC->x625 = PTC->x626 + 1;
    }
}

void tcAreaDeleteVertex(TcAdat* a)
{
    TcPoly* p = &a->poly;
    int i;

    if (p->num <= 3) {
        return;
    }
    p->num--;
    for (i = PTC->x625; i < p->num; i++) {
        p->pt[i] = p->pt[i + 1];
    }
}

void tcDrawArea()
{
    TcPoly poly;
    Vec center = {0.0f, 0.0f, 0.0f};
    static s8 rad = 60;
    u32 col = 0;
    u32 col2 = -1;
    TcAdat* ad;
    TcPoly* p;
    int i;
    int j;

    for (i = 0; i < 0x60; i++) {
        ad = &tcAdat[i];
        if (PTC->preview == 0) {
            if (ad == tcAdatPtr(PTC->adatNo, PTC->x5E1)) {
                continue;
            }
        }
        if (ad->enable == 0xFF) {
            continue;
        }
        p = &ad->poly;
        switch (ad->enable) {
        case 0:
            col = 0x802020FE;
            if (PTC->editSel == 1 && ad->area_no == PTC->adatNo) {
                col = 0xFF0000FE;
            }
            break;
        case 1:
            col = 0x808080FE;
            if (PTC->editSel == 1 && ad->area_no == PTC->adatNo) {
                col = 0xFFFFFFFE;
            }
            break;
        }
        tcDrawNgon((TcNgon*) p, col);
        if (PTC->areaDetail != 0) {
            poly = *p;
            for (j = 0; j < p->num; j++) {
                poly.pt[j].y += ad->height;
                tcDrawLine3D(&poly.pt[j], &p->pt[j], col);
            }
            tcDrawNgon((TcNgon*) &poly, col);
        }
    }
    if (PTC->preview == 0) {
        ad = tcAdatPtr(PTC->adatNo, PTC->x5E1);
    } else {
        ad = 0;
    }
    if (ad != 0) {
        switch (ad->enable) {
        case 0:
            col2 = 0xFF0000FF;
            break;
        case 1:
            col2 = 0xFFFFFFFF;
            break;
        }
        p = &ad->poly;
        if (PTC->editMode == 1 && PTC->editSel == 0) {
            tcFillNgon((TcNgon*) p, col2 & 0x808080FF);
        }
        for (i = 0; i < p->num; i++) {
            if (PTC->editMode == 0 && PTC->editSel == 0 && PTC->x2 == 1 && PTC->x7 == 0) {
                if (i == PTC->x626) {
                    col = 0x00FF00FF;
                } else {
                    col = col2;
                }
            } else {
                col = col2;
            }
            tcDrawLine3D(&p->pt[i], &p->pt[(i + 1) % p->num], col);
        }
        if (PTC->editMode == 1 && PTC->editSel == 0) {
            for (i = 1; i < p->num - 1; i++) {
                tcDrawLine3D(&p->pt[0], &p->pt[i + 1], col2);
            }
        }
        poly = *p;
        for (i = 0; i < p->num; i++) {
            col = col2;
            if (PTC->editMode == 1 && PTC->editSel == 0) {
                if ((PTC->x6 == 0 && i == PTC->x625) || (PTC->x6 == 2 && (TC_ON & 0x500))) {
                    col = 0x00FF00FF;
                }
            }
            poly.pt[i].y += ad->height;
            tcDrawLine3D(&poly.pt[i], &p->pt[i], col);
        }
        tcDrawNgon((TcNgon*) &poly, col2);
        for (i = 0; i < p->num; i++) {
            f32 r = 60.0f;
            if (PTC->editMode == 1 && PTC->editSel == 0) {
                if (PTC->x6 == 0 && (PTC->x625 == -1 || i == PTC->x625)) {
                    r = (f32) rad;
                } else if ((TC_ON & 0x500) && (PTC->x6 == 1 || PTC->x6 == 2)) {
                    r = (f32) rad;
                }
            }
            if (PTC->editMode == 1 && PTC->editSel == 0 && PTC->x6 == 2 && (TC_ON & 0x500)) {
                tcDrawSphere(&poly.pt[i], 0x00FF00FE, r);
            } else {
                tcDrawSphere(&p->pt[i], 0x00FF00FE, r);
            }
            PSVECAdd(&center, &p->pt[i], &center);
        }
        if (tcTypeTbl[ad->area_no][0] & 0x40) {
            Vec tip = {0.0f, 0.0f, 1500.0f};
            Vec wing[2] = {{100.0f, 0.0f, 1400.0f}, {-100.0f, 0.0f, 1400.0f}};
            Mtx m;
            PSVECScale(&center, &center, 1.0f / (f32) p->num);
            center.y += 100.0f;
            PSMTXRotRad(m, 'y', ad->dir);
            PSMTXMultVecSR(m, &tip, &tip);
            PSMTXMultVecSR(m, &wing[0], &wing[0]);
            PSMTXMultVecSR(m, &wing[1], &wing[1]);
            PSVECAdd(&center, &tip, &tip);
            PSVECAdd(&center, &wing[0], &wing[0]);
            PSVECAdd(&center, &wing[1], &wing[1]);
            tcDrawLine3D(&center, &tip, 0xFFFFFFFF);
            tcDrawLine3D(&wing[0], &tip, 0xFFFFFFFF);
            tcDrawLine3D(&wing[1], &tip, 0xFFFFFFFF);
        }
    }
    rad += 4;
    if (rad > 120) {
        rad = 60;
    }
}

void tcEdit_camera_qfps();
void tcEdit_camera_rail();
void fix_camera_dat(TcCdat* c);
void edit_rail_figure();
void edit_rail_point();
void edit_frame_no();
void tcMoveOffsetPoint();

void tcEdit_camera()
{
    TcCdat* c = tcCdatPtr(PTC->cdatNo);

    if (PTC->x62C != 0) {
        if (TC_ON & 0x100) {
            if (TC_TRG & 0x1) {
                c->type--;
            }
            if (TC_TRG & 0x2) {
                c->type++;
            }
            c->type = c->type < 0 ? 8 : (c->type > 8 ? 0 : c->type);
        } else {
            if (PTC->x62D != c->type) {
                PTC->x62B = 1;
            }
            PTC->x62D = c->type;
            PTC->x62C = 0;
        }
        fix_camera_dat(tcCdatPtr(PTC->cdatNo));
    }
    if (c->type == 8) {
        tcEdit_camera_qfps();
    } else {
        tcEdit_camera_rail();
    }
}

// shoulder camera (type 8) editor: adjust_qFPS drives the offset tables, this draws the menu
void tcEdit_camera_qfps()
{
    int sel[1];
    static int oldRet = 0;
    static const char* qfpsMenuName[8] = {"TYPE   ", "", "", "", "", "", "Attrib ", "Edit-->"};
    static const char* qfpsAttrName[8] = {"OFFSET", "????", "RAIL_EDGE", "RAIL_1WAY", "QFPS_BOTH", "QFPS_READY", "????",
                                          "????"};
    TcCdat* c = tcCdatPtr(PTC->cdatNo);
    int ret;
    int x;
    int y;
    int i;

    sel[0] = 0;
    g_local_floor_ratio = c->u44.floor;
    ret = adjust_qFPS(Joy, tcMenuPos[0] * 8, (tcMenuPos[1] + 2) * 14, 2, sel);
    sel[0]++;
    switch (ret) {
    case -1:
        PTC->editMode = 0;
        return;
    case 3:
        c->num = 0x18;
        tcSetBesideOffset(g_local_ready, g_local_trans);
        PTC->cam = CamCtrl.camera;
        break;
    case 4:
        c->num = 0;
        break;
    case 6:
        if (oldRet != 6) {
            CamCtrl.sub_state = 0;
        }
        CamCtrl.state = 10;
        CamCtrl.Move();
        PTC->cam = CamCtrl.camera;
        break;
    }
    oldRet = ret;
    tcSetBesideFloor(g_local_floor_ratio);
    switch (sel[0]) {
    case 0:
        if (TC_TRG & 0x100) {
            PTC->x62D = c->type;
            PTC->x62C = 1;
        }
        break;
    case 6:
        if (TC_REP & 0x1) {
            PTC->x630++;
        }
        if (TC_REP & 0x2) {
            PTC->x630--;
        }
        PTC->x630 = PTC->x630 < 0 ? 0 : (PTC->x630 > 7 ? 7 : PTC->x630);
        if (TC_TRG & 0x100) {
            c->flags ^= 1 << PTC->x630;
        }
        break;
    case 7:
        if (TC_REP & 0x1) {
            PTC->x5E1 = next_suffix(PTC->x5E1, -1);
        }
        if (TC_REP & 0x2) {
            PTC->x5E1 = next_suffix(PTC->x5E1, 1);
        }
        PTC->pAdat = tcAdatPtr(PTC->adatNo, PTC->x5E1);
        if (TC_TRG & 0x100) {
            if (PTC->pAdat != 0) {
                PTC->editSel = 0;
            }
        }
        break;
    }
    x = tcMenuPos[0];
    y = tcMenuPos[1];
    eprintf(x * 8, y * 14, 5, 0, "<--- CAMERA EDIT [%02d] --->", PTC->cdatNo);
    y++;
    for (i = 0; i < 8; i++) {
        switch (i) {
        case 0:
        case 6:
        case 7:
            eprintf(x * 8, (y + i) * 14, i == sel[0] ? 4 : 0, 0, "%s", qfpsMenuName[i]);
            break;
        }
    }
    x += 12;
    for (i = 0; i < 8; i++) {
        switch (i) {
        case 0:
            eprintf(x * 8, y * 14, 0, 0, "%s", tcTypeName[c->type]);
            break;
        case 6:
            eprintf(x * 8, y * 14, 0, 0, "%08x", BtoX(c->flags));
            if (sel[0] == 6) {
                eprintf((x + 7 - PTC->x630) * 8, y * 14, 4, 0, "%1x", BtoX((c->flags >> PTC->x630) & 1));
                eprintf((x + 9) * 8, y * 14, 0, 0, "[%10s]", qfpsAttrName[PTC->x630]);
            }
            break;
        case 7:
            if (PTC->pAdat == 0) {
                eprintf(x * 8, y * 14, 0, 0, "Area[XX]");
            } else if ((s8) PTC->adatTypeNum[PTC->adatNo] <= 1) {
                eprintf(x * 8, y * 14, 0, 0, "Area[%02d]", PTC->adatNo);
            } else {
                eprintf(x * 8, y * 14, 0, 0, "Area[%02d-%1d]", PTC->adatNo, PTC->x5E1);
            }
            break;
        }
        y++;
    }
}

// fixed / pan / track / rail camera editor: point editing, preview, attributes, offset, area link
void tcEdit_camera_rail()
{
    TcCdat* c = tcCdatPtr(PTC->cdatNo);
    int size;
    int x;
    int y;
    int i;

    PTC->x62F = 0;
    if (!(TC_ON & 0x500) && PTC->x2 == 0) {
        if (TC_TRG & 0x8) {
            PTC->x6--;
        }
        if (TC_TRG & 0x4) {
            PTC->x6++;
        }
        PTC->x6 = PTC->x6 < 0 ? 0 : (PTC->x6 > 8 ? 8 : PTC->x6);
        if (TC_TRG & 0x200) {
            PTC->editMode = 0;
            return;
        }
    }
    switch (PTC->x6) {
    case 0:
        if (TC_TRG & 0x100) {
            PTC->x62D = c->type;
            PTC->x62C = 1;
        }
        break;
    case 1:
    case 2:
    case 3:
    case 4:
        switch (PTC->x2) {
        case 0:
            if (!(TC_ON & 0x500) && (TC_TRG & 0x800)) {
                PTC->x2 = 1;
            }
            edit_rail_figure();
            break;
        case 1:
            if (TC_TRG & 0x200) {
                PTC->x2 = 0;
            }
            edit_rail_point();
            break;
        }
        break;
    case 5:
        switch (PTC->x2) {
        case 0:
            if ((TC_TRG & 0x800) && (tcCdatPtr(PTC->cdatNo)->type == 6 || tcCdatPtr(PTC->cdatNo)->type == 7)) {
                PTC->x2++;
                CamCtrl.flags_2C &= ~1;
                pG->flags_60 &= ~0x10000000;
            } else {
                edit_frame_no();
            }
            break;
        case 1:
            if (tcCdatPtr(PTC->cdatNo)->type == 6) {
                CamCtrl.CutCall(PTC->cdatNo);
            } else if (tcCdatPtr(PTC->cdatNo)->type == 7) {
                Vec pos = {0.0f, 0.0f, 0.0f};
                Vec at = {0.0f, 0.0f, 0.0f};
                Vec up = {0.0f, 0.0f, 0.0f};
                if (PTC->x640 != 0) {
                    CamCtrl.UpCutCall(PTC->cdatNo, &pos, &at, &up, 0);
                } else {
                    CamCtrl.UpCutCall(PTC->cdatNo, &pos, &at, &up, 1);
                }
            }
            PTC->x2++;
            break;
        case 2:
            if (CamCtrl.IsMotionEnd()) {
                PTC->x2++;
            }
            break;
        case 3:
            tcPreviewOnOff(0);
            PTC->x2 = 0;
            break;
        }
        if (PTC->x2 != 0) {
            tcDataExport((u8*) g_pToolCamData);
            if (PTC->x640 != 0) {
                CamCtrl.CoreDataRead((CameraDataHeader*) g_pToolCamData);
            } else {
                CamCtrl.RoomDataRead((CameraDataHeader*) g_pToolCamData);
            }
            CameraMove();
            LightMgr.move();
            tcGameCamera2ToolCamera();
        }
        break;
    case 6:
        if (TC_REP & 0x1) {
            PTC->x630++;
        }
        if (TC_REP & 0x2) {
            PTC->x630--;
        }
        PTC->x630 = PTC->x630 < 0 ? 0 : (PTC->x630 > 7 ? 7 : PTC->x630);
        if (TC_TRG & 0x100) {
            c->flags ^= 1 << PTC->x630;
        }
        break;
    case 7:
        tcMoveOffsetPoint();
        break;
    case 8:
        if (TC_REP & 0x1) {
            PTC->x5E1 = next_suffix(PTC->x5E1, -1);
        }
        if (TC_REP & 0x2) {
            PTC->x5E1 = next_suffix(PTC->x5E1, 1);
        }
        PTC->pAdat = tcAdatPtr(PTC->adatNo, PTC->x5E1);
        if (TC_TRG & 0x100) {
            if (PTC->pAdat != 0) {
                PTC->editSel = 0;
            }
        }
        break;
    }
    size = tcDataExport((u8*) g_pToolCamData);
    if (size > 0x19000) {
        pLog->err(0, 0, "Camera data overflow: 0x%x > TC_CAM_DATA_BUFF(0x%x)", size, 0x19000);
    }
    {
    static const char* railMenuName[9] = {"TYPE  ", "Campos", "Target", "Roll  ", "Fovy  ", "Frame ", "Attrib", "Offset",
                                          "Edit-->"};
    static const char* railAttrName[8] = {"OFFSET", "????", "RAIL_EDGE", "RAIL_1WAY", "BESIDE_FWD", "????", "????",
                                          "????"};
    CamCtrl.RoomDataRead((CameraDataHeader*) g_pToolCamData);
    Parametrize(CamCtrl.DataSearch(PTC->cdatNo), &CamBSpline);
    x = tcMenuPos[0];
    y = tcMenuPos[1];
    eprintf(x * 8, y * 14, 5, 0, "<--- CAMERA EDIT [%02d] --->", PTC->cdatNo);
    y++;
    for (i = 0; i < 9; i++) {
        int col = i == PTC->x6 ? 4 : 0;
        if (i >= 1 && i <= 5) {
            eprintf(x * 8, (y + i) * 14, col, 0, "%s[%02d]", railMenuName[i], PTC->x627);
        } else {
            eprintf(x * 8, (y + i) * 14, col, 0, "%s", railMenuName[i]);
        }
    }
    x += 12;
    for (i = 0; i < 9; i++) {
        switch (i) {
        case 0:
            eprintf(x * 8, y * 14, 0, 0, "%s", tcTypeName[c->type]);
            break;
        case 1:
            eprintf(x * 8, y * 14, 0, 0, "(%f, %f, %f)", c->pos[PTC->x627].x, c->pos[PTC->x627].y, c->pos[PTC->x627].z);
            break;
        case 2:
            eprintf(x * 8, y * 14, 0, 0, "(%f, %f, %f)", c->at[PTC->x627].x, c->at[PTC->x627].y, c->at[PTC->x627].z);
            break;
        case 3:
            eprintf(x * 8, y * 14, 0, 0, "%.3f", c->roll[PTC->x627] * 57.29578f);
            break;
        case 4:
            eprintf(x * 8, y * 14, 0, 0, "%2.2f", c->fovy[PTC->x627]);
            break;
        case 6:
            eprintf(x * 8, y * 14, 0, 0, "%08x", BtoX(c->flags));
            if (PTC->x6 == 6) {
                eprintf((x + 7 - PTC->x630) * 8, y * 14, 4, 0, "%1x", BtoX((c->flags >> PTC->x630) & 1));
                eprintf((x + 9) * 8, y * 14, 0, 0, "[%10s]", railAttrName[PTC->x630]);
            }
            break;
        case 5:
            if (c->type == 6 || c->type == 7) {
                eprintf(x * 8, y * 14, 0, 0, "%3d", (s16) c->frame[PTC->x627]);
            }
            break;
        case 7:
            if (c->type != 4) {
                eprintf(x * 8, y * 14, 0, 0, "(%f, %f, %f)", c->aim_ofs.x, c->aim_ofs.y, c->aim_ofs.z);
            } else if (PTC->x631 == 0) {
                eprintf(x * 8, y * 14, 0, 0, "(%f, %f, %f)", c->aim_ofs.x, c->aim_ofs.y, c->aim_ofs.z);
                eprintf((x - 6) * 8, y * 14, 0, 0, "[CMPS]");
            } else {
                eprintf(x * 8, y * 14, 0, 0, "(%f, %f, %f)", c->u44.dir.x, c->u44.dir.y, c->u44.dir.z);
                eprintf((x - 6) * 8, y * 14, 0, 0, "[TRGT]");
            }
            break;
        case 8:
            if (PTC->pAdat == 0) {
                eprintf(x * 8, y * 14, 0, 0, "Area[XX]");
            } else if ((s8) PTC->adatTypeNum[PTC->adatNo] <= 1) {
                eprintf(x * 8, y * 14, 0, 0, "Area[%02d]", PTC->adatNo);
            } else {
                eprintf(x * 8, y * 14, 0, 0, "Area[%02d-%1d]", PTC->adatNo, PTC->x5E1);
            }
            break;
        }
        y++;
    }
    if (pG->flags_51E4 & 0x38) {
        if (PTC->viewMode != 0) {
            eprintf(25 * 8, 31 * 14, 4, 0, "WORKING VIEW");
        } else {
            eprintf(25 * 8 + 4, 31 * 14, 0x16, 0, "CAMERA VIEW");
        }
    }
    }
}

TcCdat* tcNextCdatPtr(s8 no, int dir)
{
    int i;

    for (i = 0; i < 0x40; i++) {
        TcCdat* c;
        no = (s8) (no + dir);
        no = (no + 0x40) % 0x40;
        c = tcCdatPtr(no);
        if (c) {
            return c;
        }
    }
    return 0;
}

TcAdat* tcNextAdatPtr(s8 area, int cam, int dir)
{
    int suffix = next_suffixI(cam, dir);

    // the result copy `mr r4,r3` and the `dir > 0` compare are both ready after the call; the original
    // issues the copy first, ours the compare (its branch gives it the higher priority). The launder is a
    // same-block consumer of the copy, which restores the tie and the LUID order.
    asm("" : "+r"(suffix)); // COMPILER-DIFF: 5 (sched2 tie: call-result copy vs compare)
    if ((dir > 0 && suffix > cam) || (dir < 0 && suffix < cam)) {
        return tcAdatPtr(area, suffix);
    }
    do {
        area = (s8) (area + dir);
        area = (area + 0x40) % 0x40;
    } while (PTC->adatTypeNum[area] == 0);
    if (dir > 0) {
        suffix = head_suffix(area);
    } else {
        suffix = 0;
        if (dir >= 0) {
            return 0;
        }
        suffix = tail_suffix(area);
    }
    return tcAdatPtr(area, suffix);
}

int head_suffix(s8 area)
{
    int i;

    for (i = 0; i < 8; i++) {
        s8 s = i;
        if (tcAdatPtr(area, s)) {
            return s;
        }
    }
    return -1;
}

int tail_suffix(s8 area)
{
    int i;

    for (i = 7; i >= 0; i--) {
        s8 s = i;
        if (tcAdatPtr(area, s)) {
            return s;
        }
    }
    return -1;
}

int next_suffix(s8 cam, int dir)
{
    int i;
    s8 n = cam;

    if (n == -1) {
        return -1;
    }
    for (i = 0; i < 8; i++) {
        n = (s8) (n + dir);
        n = (n + 8) % 8;
        if (tcAdatPtr(PTC->adatNo, n)) {
            return n;
        }
    }
    return 0;
}

void tcCameraSetPoint(TcCdat* c);
void tcCameraMovePoint(TcCdat* c, int mode);
void tcCameraSelectPoint(TcCdat* c);
void tcCameraSelectSegment(TcCdat* c);
void tcCameraInsertPoint(TcCdat* c);
void tcCameraSelectLR(TcCdat* c);
void tcCameraCopyPoint(TcCdat* c);
void tcCameraDeletePoint(TcCdat* c);

// after a camera type change: give the cut the data its type needs
void fix_camera_dat(TcCdat* c)
{
    int dummy[1];
    int i;

    if (PTC->x62B != 0) {
        switch (c->type) {
        case 0:
        case 1:
            PTC->x627 = 0;
            break;
        case 2:
        case 3:
            if (c->num <= 1) {
                c->num = 2;
                c->pos[1] = c->pos[0];
                c->at[1] = c->at[0];
                c->roll[1] = c->roll[0];
                c->fovy[1] = c->fovy[0];
                PTC->x627 = 1;
            }
            break;
        case 6:
        case 7:
            for (i = 1; i < c->num; i++) {
                if (c->frame[i] == 0) {
                    c->frame[i] = i * 30;
                }
            }
            break;
        case 8:
            if (c->num <= 0x17) {
                c->num = 0;
            }
            break;
        }
        if (c->type == 4) {
            c->aim_ofs.x = 0.0f;
            c->aim_ofs.y = 1800.0f;
            c->aim_ofs.z = -1200.0f;
            c->u44.dir.x = 0.0f;
            c->u44.dir.y = 1550.0f;
            c->u44.dir.z = 0.0f;
        } else {
            c->aim_ofs.x = 0.0f;
            c->aim_ofs.y = 1000.0f;
            c->aim_ofs.z = 0.0f;
        }
        if (PTC->viewMode == 0) {
            tcCameraPullPoint(c);
        }
        PTC->x62B = 0;
    }
    adjust_qFPS(Joy, tcMenuPos[0] * 8, (tcMenuPos[1] + 2) * 14, 3, dummy);
}

// point editing of the campos / target / roll / fovy rows
void edit_rail_figure()
{
    TcCdat* c = tcCdatPtr(PTC->cdatNo);

    if (c == 0) {
        return;
    }
    switch (PTC->x6) {
    case 1:
        if (PTC->viewMode != 0 && (TC_ON & 0x500)) {
            tcCameraMovePoint(c, 0);
        } else {
            tcCameraSelectPoint(c);
        }
        break;
    case 2:
        if (PTC->viewMode == 0) {
            PTC->x62F = 1;
        }
        if (PTC->viewMode != 0 && (TC_ON & 0x500)) {
            tcCameraMovePoint(c, 1);
        } else {
            tcCameraSelectPoint(c);
        }
        break;
    case 3:
        if (TC_ON & 0x500) {
            tcCameraMovePoint(c, 2);
        } else {
            tcCameraSelectPoint(c);
        }
        break;
    case 4:
        if (TC_ON & 0x500) {
            tcCameraMovePoint(c, 3);
        } else {
            tcCameraSelectPoint(c);
        }
        break;
    }
    tcCameraSetPoint(c);
}

// point insert / copy / delete menu of the rail editor
void edit_rail_point()
{
    TcMenu menu[2] = {{1, "INS: POINT"}, {1, "DEL: POINT"}};
    TcCdat* c = tcCdatPtr(PTC->cdatNo);

    if (c == 0) {
        return;
    }
    tcMenuSelect(tcMenuPos[8] * 8, tcMenuPos[9] * 14, 0, menu, 2, &PTC->x7);
    switch (PTC->x7) {
    case 0:
        if (PTC->viewMode != 0) {
            tcCameraSelectSegment(c);
            if (TC_TRG & 0x100) {
                tcCameraInsertPoint(c);
            }
        } else {
            tcCameraSelectLR(c);
            if (TC_TRG & 0x100) {
                tcCameraCopyPoint(c);
            }
        }
        break;
    case 1:
        tcCameraSelectPoint(c);
        if (TC_TRG & 0x100) {
            tcCameraDeletePoint(c);
        }
        break;
    }
}

// key frame numbers of the rail / motion cameras
void edit_frame_no()
{
    TcCdat* c = tcCdatPtr(PTC->cdatNo);

    if (c == 0) {
        return;
    }
    if (c->type != 6 && c->type != 7) {
        return;
    }
    if (TC_ON & 0x500) {
        if (TC_ON & 0x100) {
            if (TC_REP & 0x1) {
                c->frame[PTC->x627]--;
            }
            if (TC_REP & 0x2) {
                c->frame[PTC->x627]++;
            }
        } else if (TC_ON & 0x400) {
            if (TC_REP & 0x1) {
                c->frame[PTC->x627] -= 10;
            }
            if (TC_REP & 0x2) {
                c->frame[PTC->x627] += 10;
            }
        }
        if (TC_ON & 0x500) {
            if (PTC->x627 == 0) {
                if (c->frame[PTC->x627] < 0) {
                    c->frame[PTC->x627] = 0;
                }
            } else {
                if (c->frame[PTC->x627] <= c->frame[PTC->x627 - 1]) {
                    c->frame[PTC->x627] = c->frame[PTC->x627 - 1] + 1;
                }
            }
            if (PTC->x627 != c->num - 1) {
                if (c->frame[PTC->x627] >= c->frame[PTC->x627 + 1]) {
                    c->frame[PTC->x627] = c->frame[PTC->x627 + 1] - 1;
                }
            }
        }
    } else {
        tcCameraSelectPoint(c);
    }
    tcCameraSetPoint(c);
}

// aim offset (and the type-4 target direction) editor
void tcMoveOffsetPoint()
{
    Vec* ofs;

    if (tcCdatPtr(PTC->cdatNo)->type == 4) {
        if ((TC_ON & 0x101) == 0x1) {
            PTC->x631 = 0;
        }
        if ((TC_ON & 0x102) == 0x2) {
            PTC->x631 = 1;
        }
        if (PTC->x631 == 0) {
            ofs = &tcCdatPtr(PTC->cdatNo)->aim_ofs;
        } else {
            ofs = &tcCdatPtr(PTC->cdatNo)->u44.dir;
        }
        {
            TcWork* w = PTC;
            Camera* cam = &w->cam;
            cPlayer* pl = pPL;
            PSMTXMultVec(pl->mat, &tcCdatPtr(PTC->cdatNo)->aim_ofs, &cam->param.pos);
            PSMTXMultVec(pl->mat, &tcCdatPtr(PTC->cdatNo)->u44.dir, &cam->param.at);
            CameraSetOrientationUp(cam);
        }
    } else {
        ofs = &tcCdatPtr(PTC->cdatNo)->aim_ofs;
    }
    if (TC_ON & 0x100) {
        if (TC_ON & 0x1) {
            ofs->x -= 10.0f;
        }
        if (TC_ON & 0x2) {
            ofs->x += 10.0f;
        }
        if (TC_ON & 0x4) {
            ofs->z += 10.0f;
        }
        if (TC_ON & 0x8) {
            ofs->z -= 10.0f;
        }
    }
    if (TC_ON & 0x400) {
        if (TC_ON & 0x8) {
            ofs->y += 10.0f;
        }
        if (TC_ON & 0x4) {
            ofs->y -= 10.0f;
        }
    }
}

// a pan / track cut edited with one key gets its second key from the first
static inline void tcCdatFixPan(TcCdat* c)
{
    switch (c->type) {
    case 1:
    case 2:
        if (c->num == 1) {
            c->num = 2;
            c->pos[1] = c->pos[0];
            c->at[1] = c->at[0];
            c->roll[1] = c->roll[0];
            c->fovy[1] = c->fovy[0];
        }
        break;
    }
}

// tool camera -> current key (and the automatic frame number of a new key)
void tcCameraSetPoint(TcCdat* c)
{
    if (PTC->viewMode != 0) {
        return;
    }
    if ((PTC->x6 == 1 || PTC->x6 == 2) && (TC_TRG & 0x100)) {
        c->pos[PTC->x627] = PTC->cam.param.pos;
        c->at[PTC->x627] = PTC->cam.param.at;
        tcCdatFixPan(c);
    }
    if ((PTC->x6 == 3 || PTC->x6 == 4) && (TC_ON & 0x500)) {
        PTC->cam.param.roll = c->roll[PTC->x627];
        PTC->cam.param.fovy = c->fovy[PTC->x627];
    }
    CameraSetOrientationRoll(&PTC->cam);
    if ((c->type == 6 || c->type == 7) && (TC_TRG & 0x100)) {
        int i = PTC->x627;
        if (i > 0) {
            if (c->frame[i] == 0) {
                f32 f;
                if (i == c->num - 1) {
                    f = (f32) c->frame[i - 1] / 30.0f + 1.0f;
                } else {
                    f = (f32) (c->frame[i - 1] + c->frame[i + 1]) * 0.5f / 30.0f;
                }
                c->frame[PTC->x627] = (int) (f * 30.0f);
            }
        }
    }
}

// move the current key by the pad: mode 0 campos, 1 target, 2 roll, 3 fovy
void tcCameraMovePoint(TcCdat* c, int mode)
{
    Camera* cam = &PTC->cam;
    Vec d = {0.0f, 0.0f, 0.0f};

    switch (mode) {
    case 0:
    case 1:
        if ((TC_ON & 0x500) == 0x100) {
            if (TC_ON & 0x8) {
                d.z = -100.0f;
            }
            if (TC_ON & 0x4) {
                d.z = 100.0f;
            }
            if (TC_ON & 0x1) {
                d.x = -100.0f;
            }
            if (TC_ON & 0x2) {
                d.x = 100.0f;
            }
        }
        if (TC_ON & 0x400) {
            if (TC_ON & 0x8) {
                d.y = 100.0f;
            }
            if (TC_ON & 0x4) {
                d.y = -100.0f;
            }
        }
        if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f) {
            Mtx m;
            Vec right;
            Vec axis = {0.0f, 1.0f, 0.0f};
            Vec fwd;
            Vec zero;
            fwd.x = cam->mat[0][2];
            fwd.y = cam->mat[1][2];
            fwd.z = cam->mat[2][2];
            if (fwd.x == 0.0f && fwd.z == 0.0f) {
                fwd.x = cam->mat[0][1];
                fwd.y = cam->mat[1][1];
                fwd.z = cam->mat[2][1];
            }
            fwd.y = 0.0f;
#line 3204 "D:/Bio4/Prog/t_camera.cpp"
            VECNormalize(&fwd, &fwd);
            PSVECCrossProduct(&axis, &fwd, &right);
            MTX_SET_COLUMNS(m, right, axis, fwd, zero);
            PSMTXMultVecSR(m, &d, &d);
        }
        break;
    case 2:
    case 3:
        if (TC_REP & 0x1) {
            d.x = -1.0f;
        }
        if (TC_REP & 0x2) {
            d.x = 1.0f;
        }
        if (TC_ON & 0x400) {
            d.x *= 10.0f;
        }
        break;
    }
    switch (mode) {
    case 0: {
        f32* px = &c->pos[0].x;
        f32* py = &c->pos[0].y;
        f32* pz = &c->pos[0].z;
        VEC_ELEM(px, PTC->x627) += d.x;
        VEC_ELEM(py, PTC->x627) += d.y;
        VEC_ELEM(pz, PTC->x627) += d.z;
        break;
    }
    case 1: {
        f32* px = &c->at[0].x;
        f32* py = &c->at[0].y;
        f32* pz = &c->at[0].z;
        VEC_ELEM(px, PTC->x627) += d.x;
        VEC_ELEM(py, PTC->x627) += d.y;
        VEC_ELEM(pz, PTC->x627) += d.z;
        break;
    }
    case 2: {
        f32 deg = c->roll[PTC->x627] * 57.29578f + d.x;
        deg = deg < -180.0f ? 180.0f : (deg > 180.0f ? -180.0f : deg);
        c->roll[PTC->x627] = deg * 0.017453292f;
        break;
    }
    case 3:
        c->fovy[PTC->x627] += d.x;
        c->fovy[PTC->x627] =
            c->fovy[PTC->x627] < 10.0f ? 10.0f : (c->fovy[PTC->x627] > 90.0f ? 90.0f : c->fovy[PTC->x627]);
        break;
    }
    tcCdatFixPan(c);
}

// tool camera <- current key
void tcCameraPullPoint(TcCdat* c)
{
    if (c->num == 0) {
        return;
    }
    if (c->type != 8) {
        PTC->x627 = PTC->x627 < 0 ? c->num - 1 : (PTC->x627 > c->num - 1 ? 0 : PTC->x627);
        PTC->cam.param.pos = c->pos[PTC->x627];
        PTC->cam.param.at = c->at[PTC->x627];
        PTC->cam.param.roll = c->roll[PTC->x627];
        PTC->cam.param.fovy = c->fovy[PTC->x627];
    }
    CameraSetOrientationRoll(&PTC->cam);
}

void tcCameraSelectPoint(TcCdat* c)
{
    int old = PTC->x627;

    if (TC_REP & 0x1) {
        PTC->x627--;
    }
    if (TC_REP & 0x2) {
        PTC->x627++;
    }
    switch (c->type) {
    case 0:
    case 1:
        PTC->x627 = 0;
        break;
    default:
        PTC->x627 = PTC->x627 < 0 ? c->num - 1 : (PTC->x627 > c->num - 1 ? 0 : PTC->x627);
        break;
    }
    if (PTC->viewMode == 0 && old != PTC->x627) {
        tcCameraPullPoint(c);
    }
}

void tcCameraSelectSegment(TcCdat* c)
{
    if (TC_REP & 0x1) {
        PTC->x628--;
    }
    if (TC_REP & 0x2) {
        PTC->x628++;
    }
    PTC->x628 = PTC->x628 < 0 ? c->num - 2 : (PTC->x628 > c->num - 2 ? 0 : PTC->x628);
}

// insert a key in the middle of the selected segment
void tcCameraInsertPoint(TcCdat* c)
{
    Vec v;
    int i;
    int i0;
    int i1;

    if (c->num <= 25) {
        for (i = c->num; i > PTC->x628; i--) {
            c->pos[i] = c->pos[i - 1];
            c->at[i] = c->at[i - 1];
            c->roll[i] = c->roll[i - 1];
            c->fovy[i] = c->fovy[i - 1];
        }
        c->num++;
        i0 = PTC->x628;
        i1 = i0 + 2;
        PSVECAdd(&c->pos[i0], &c->pos[i1], &v);
        PSVECScale(&v, &c->pos[PTC->x628 + 1], 0.5f);
        PSVECAdd(&c->at[i0], &c->at[i1], &v);
        PSVECScale(&v, &c->at[PTC->x628 + 1], 0.5f);
        c->roll[PTC->x628 + 1] = (c->roll[i0] + c->roll[i1]) * 0.5f;
        c->fovy[PTC->x628 + 1] = (c->fovy[i0] + c->fovy[i1]) * 0.5f;
        PTC->x627 = PTC->x628 + 1;
    }
}

void tcCameraSelectLR(TcCdat* c)
{
    int x = tcMenuPos[8];
    int y = tcMenuPos[9];

    x += 10;
    if (TC_REP & 0x1) {
        PTC->x629 = 0;
    }
    if (TC_REP & 0x2) {
        PTC->x629 = 1;
    }
    if (PTC->x629 != 0) {
        eprintf(x * 8, y * 14, 0, 0, "-----/RIGHT");
    } else {
        eprintf(x * 8, y * 14, 0, 0, "LEFT-/-----");
    }
}

// duplicate the current key to its left or right (the side is picked from the screen side of the neighbour)
void tcCameraCopyPoint(TcCdat* c)
{
    Vec v;
    int i;
    int j;
    TcWork* p;

    if (c->num <= 25) {
        i = PTC->x627;
        // both arms: `v.x >= 0` test first, a shared `p` and one `x627++` behind `goto skip` -- the
        // layout jump2 needs to cross-jump A2 into B1 and B2 into A1 (four per-arm copies leave 13 words)
        if (i - 1 >= 0) {
            PSMTXMultVec(pG->Cam.viewMat, &c->at[i - 1], &v);
            if (v.x >= 0.0f) {
                p = PTC;
                if (p->x629 != 0) {
                    goto skip;
                }
            } else {
                p = PTC;
                if (p->x629 == 0) {
                    goto skip;
                }
            }
            p->x627++;
        } else {
            PSMTXMultVec(pG->Cam.viewMat, &c->at[i + 1], &v);
            if (v.x >= 0.0f) {
                p = PTC;
                if (p->x629 == 0) {
                    goto skip;
                }
            } else {
                p = PTC;
                if (p->x629 != 0) {
                    goto skip;
                }
            }
            p->x627++;
        }
    skip:
        for (j = c->num; j > i; j--) {
            c->pos[j] = c->pos[j - 1];
            c->at[j] = c->at[j - 1];
            c->roll[j] = c->roll[j - 1];
            c->fovy[j] = c->fovy[j - 1];
            c->frame[j] = c->frame[j - 1];
        }
        c->frame[i] = (c->frame[i + 1] + c->frame[i - 1]) / 2;
        c->num++;
    }
}

void tcCameraDeletePoint(TcCdat* c)
{
    int i;

    if (c->num > 2) {
        c->num--;
        for (i = PTC->x627; i < c->num; i++) {
            c->pos[i] = c->pos[i + 1];
            c->at[i] = c->at[i + 1];
            c->roll[i] = c->roll[i + 1];
            c->fovy[i] = c->fovy[i + 1];
            c->frame[i] = c->frame[i + 1];
        }
        c->frame[c->num] = 0;
    }
}

// the aim offset: player -> aim point (on the floor) -> aim height
void tcDrawOffset()
{
    CameraControl* cc = &CamCtrl;
    TcCdat* c = tcCdatPtr(PTC->cdatNo);

    if (c) {
        Vec a;
        Vec b;
        cc->CalcAim((CameraCut*) c);
        a = cc->aim;
        a.y = 0.0f;
        b = cc->aim;
        tcDrawLine3D(&pPL->pos, &a, 0x202080FF);
        tcDrawLine3D(&a, &b, 0x202080FF);
        tcDrawSphere(&b, 0x202080FF, 100.0f);
    }
}

// key points of the current cut: campos / target spheres, floor lines, the selected segment
void tcDrawRail()
{
    TcCdat* c = tcCdatPtr(PTC->cdatNo);
    static s8 rad = 60;
    int i;

    if (c == 0) {
        return;
    }
    for (i = 0; i < c->num; i++) {
        Vec v;
        u32 col;
        f32 r;
        col = 0xFF0000FE;
        if (i == PTC->x627 && PTC->x6 == 1) {
            r = (f32) rad;
            col = 0x00FF00FE;
        } else {
            r = 60.0f;
        }
        if (PTC->editMode == 0 || PTC->editSel == 0) {
            r = 60.0f;
        }
        tcDrawSphere(&c->pos[i], col, r);
        if (PTC->editMode == 1 && PTC->editSel == 1 && i == PTC->x627 && PTC->x6 == 1) {
            v = c->pos[i];
            {
                register Vec* a3 asm("r3");  // COMPILER-DIFF: candidate #18 (struct-return-like address in r3 before a no-argument call)
                a3 = &v;
                asm("" : "=m"(v.y) : "r"(a3));
            }
            v.y = tcGetFloor();
            tcDrawLine3D(&c->pos[i], &v, 0xFF0000FE);
        }
        col = 0x0000FFFE;
        if (i == PTC->x627 && PTC->x6 == 2) {
            r = (f32) rad;
            col = 0x00FF00FE;
        } else {
            r = 60.0f;
        }
        if (PTC->editMode == 0 || PTC->editSel == 0) {
            r = 60.0f;
        }
        tcDrawSphere(&c->at[i], col, r);
        if (PTC->editMode == 1 && PTC->editSel == 1 && i == PTC->x627 && PTC->x6 == 2) {
            v = c->at[i];
            {
                register Vec* a3 asm("r3");  // COMPILER-DIFF: candidate #18 (struct-return-like address in r3 before a no-argument call)
                a3 = &v;
                asm("" : "=m"(v.y) : "r"(a3));
            }
            v.y = tcGetFloor();
            tcDrawLine3D(&c->at[i], &v, 0xFF0000FE);
        }
        if (PTC->editMode == 1 && PTC->editSel == 1) {
            if (i == PTC->x627) {
                tcDrawLine3D(&c->pos[i], &c->at[i], 0xFFFF00FE);
            }
            if (c->type != 0 && c->type != 1 && PTC->x2 == 1 && PTC->x7 == 0 && i == PTC->x628) {
                tcDrawLine3D(&c->pos[i], &c->pos[i + 1], 0x00FF00FE);
                tcDrawLine3D(&c->at[i], &c->at[i + 1], 0x00FF00FE);
            }
        }
    }
    rad += 4;
    if (rad > 120) {
        rad = 60;
    }
    tcDrawParametricCurve();
}

// file menu of the load screen: FILE #0..2 / CORE, yes-no confirmation, HD read
static void tcLoad()
{
    TcMenu menu[4] = {{1, "FILE #0:"}, {1, "FILE #1:"}, {1, "FILE #2:"}, {1, "CORE   :"}};
    static int yesNo = 0;
    static int flags = 0;
    static int failTimer = 0;
    static char path[256];
    int x = tcMenuPos[0];
    int y = tcMenuPos[1];
    int col;

    eprintf(x * 8, y * 14, 4, 0, "--- LOAD FILE ---");
    col = 6;
    if (PTC->x2 == 0) {
        col = 0;
    }
    y++;
    tcMenuSelect(x * 8, y * 14, col, menu, 4, &PTC->x6);
    x += 16;
    switch (PTC->x2) {
    case 0:
        if (TC_ON & 0x2) {
            flags |= 1;
        } else {
            flags &= ~1;
        }
        if (TC_TRG & 0x200) {
            PTC->routine = 1;
            PTC->x6 = 0;
            return;
        } else if (TC_TRG & 0x100) {
            PTC->x2 = 1;
            yesNo = 0;
        }
        break;
    case 1:
        if (TC_TRG & 0x3) {
            yesNo = yesNo == 0;
        }
        if (yesNo != 0) {
            eprintf(x * 8, (y + PTC->x6) * 14, 0, 0, "YES/---");
        } else {
            eprintf(x * 8, (y + PTC->x6) * 14, 0, 0, "---/NO-");
        }
        if (TC_TRG & 0x200) {
            PTC->x2 = 0;
            return;
        } else if (TC_TRG & 0x100) {
            if (yesNo != 0) {
                PTC->x2 = 2;
            } else {
                PTC->x2 = 0;
            }
        }
        break;
    case 2:
        if (PTC->x6 == 3) {
            PTC->x640 = 1;
        } else {
            PTC->x640 = 0;
        }
        if (PTC->x640 != 0) {
            flags |= 2;
        } else {
            flags &= ~2;
        }
        tcGetFileName(path, (u8) PTC->x6, flags);
        if (HDRead(path, g_pToolCamData) != 0) {
            if (cameraDataVersion((char*) g_pToolCamData) > 1) {
                if (PTC->x640 != 0) {
                    CamCtrl.CoreDataRead((CameraDataHeader*) g_pToolCamData);
                } else {
                    CamCtrl.RoomDataRead((CameraDataHeader*) g_pToolCamData);
                }
                tcDataInitialize();
                tcDataImport((u8*) g_pToolCamData);
            }
            PTC->routine = 1;
            PTC->editMode = 0;
            PTC->x2 = 0;
        } else {
            PTC->x2 = 3;
            failTimer = 0;
        }
        break;
    case 3:
        if (failTimer++ <= 89) {
            eprintf(x * 8, (y + PTC->x6) * 14, 2, 0, "FAILED!!");
        } else {
            PTC->routine = 1;
            PTC->editMode = 0;
            PTC->x2 = 0;
        }
        break;
    }
    x -= 7;
    if (flags & 1) {
        eprintf(x * 8, (y + PTC->x6) * 14, 5, 0, "SERVER");
    } else {
        eprintf(x * 8, (y + PTC->x6) * 14, 0, 0, "LOCAL-");
    }
}

static void tcSave()
{
    TcMenu menu[4] = {{1, "FILE #0:"}, {1, "FILE #1:"}, {1, "FILE #2:"}, {1, "CORE   :"}};
    static int yesNo = 0;
    static int flags = 0;
    static int failTimer = 0;
    static char path[256];
    int x = tcMenuPos[0];
    int y = tcMenuPos[1];
    int col;
    int size;
    int ret;

    eprintf(x * 8, y * 14, 4, 0, "--- SAVE FILE ---");
    col = 6;
    if (PTC->x2 == 0) {
        col = 0;
    }
    y++;
    tcMenuSelect(x * 8, y * 14, col, menu, 4, &PTC->x6);
    x += 16;
    switch (PTC->x2) {
    case 0:
        if (TC_ON & 0x2) {
            flags |= 1;
        } else {
            flags &= ~1;
        }
        if (TC_TRG & 0x200) {
            PTC->routine = 1;
            PTC->x6 = 0;
            return;
        } else if (TC_TRG & 0x100) {
            PTC->x2 = 1;
            yesNo = 0;
        }
        break;
    case 1:
        if (TC_TRG & 0x3) {
            yesNo = yesNo == 0;
        }
        if (yesNo != 0) {
            eprintf(x * 8, (y + PTC->x6) * 14, 0, 0, "YES/---");
        } else {
            eprintf(x * 8, (y + PTC->x6) * 14, 0, 0, "---/NO-");
        }
        if (TC_TRG & 0x200) {
            PTC->x2 = 0;
            return;
        } else if (TC_TRG & 0x100) {
            if (yesNo != 0) {
                PTC->x2 = 2;
            } else {
                PTC->x2 = 0;
            }
        }
        break;
    case 2:
        if (PTC->x6 == 3) {
            PTC->x640 = 1;
        } else {
            PTC->x640 = 0;
        }
        if (PTC->x640 != 0) {
            flags |= 2;
        } else {
            flags &= ~2;
        }
        tcGetFileName(path, (u8) PTC->x6, flags);
        size = tcDataExport((u8*) g_pToolCamData);
        ret = HDWrite(path, g_pToolCamData, size);
        if (PTC->x640 != 0) {
            CamCtrl.CoreDataRead((CameraDataHeader*) g_pToolCamData);
        } else {
            CamCtrl.RoomDataRead((CameraDataHeader*) g_pToolCamData);
        }
        if (ret != size) {
            PTC->x2 = 3;
            failTimer = 0;
        } else {
            PTC->routine = 1;
            PTC->editMode = 0;
            PTC->x2 = 0;
        }
        break;
    case 3:
        if (failTimer++ <= 89) {
            eprintf(x * 8, (y + PTC->x6) * 14, 2, 0, "FAILED!!");
        } else {
            PTC->routine = 1;
            PTC->editMode = 0;
            PTC->x2 = 0;
        }
        break;
    }
    x -= 7;
    if (flags & 1) {
        eprintf(x * 8, (y + PTC->x6) * 14, 5, 0, "SERVER");
    } else {
        eprintf(x * 8, (y + PTC->x6) * 14, 0, 0, "LOCAL-");
    }
}

static void tcQuit()
{
    if (*(u16*) &PTC->cdatNum != 0) {
        tcDataExport((u8*) g_pToolCamData);
        CamCtrl.RoomDataRead((CameraDataHeader*) g_pToolCamData);
        CamCtrl.flags_2C = (CamCtrl.flags_2C & ~1) | 0x10;
    }
    BitOff(pG->flags_60, 0x80000000);
    BitOff(pG->flags_60, 0x20000000);
    BitOff(pG->flags_60, 0x10000000);
    BitOff(pG->flags_170, 0x400000);
    pSys->key_type = PTC->x637;
    CameraSetProjection(1);
    if (!(Joy[0].on & 0x400)) {
        CamCtrl.Comeback(0);
    }
    tcGameCameraLoad();
    TaskSignal(0);
    TaskExit();
}

static f32 tcDollySpeed = 5.0f;
static f32 tcDollyDummy = 0.0f;

void tcToolCameraMove(Camera* cam)
{
    Vec d = {0.0f, 0.0f, 0.0f};
    f32 z;

    if (TC_ON & 0x60) {
        if (TC_ON & 0x20) {
            z = (f32) -(int) PTC->joy.trigR;
        } else {
            z = (f32) PTC->joy.trigL;
        }
        z *= tcDollySpeed;
        switch (CameraGetProjection()) {
        case 1: {
            f32 dist = cam->dist + z;
            if (dist < 500.0f) {
                d.z = dist - 500.0f;
                dist = 500.0f;
            }
            if (PTC->x62F) {
                CameraTargetDistance(cam, dist);
            } else {
                CameraCamposDistance(cam, dist);
            }
            break;
        }
        case 2:
            ORTHO_T = z * 3.0f * 0.25f + ORTHO_T;
            ORTHO_L = ORTHO_L - z;
            ORTHO_B = -ORTHO_T;
            ORTHO_R = -ORTHO_L;
            break;
        }
    }
    if (PTC->joy.ssx) {
        d.x = (f32) PTC->joy.ssx * 5.0f;
    }
    if (PTC->joy.ssy) {
        d.y = (f32) PTC->joy.ssy * 5.0f;
    }
    if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f) {
        PSMTXMultVecSR(cam->mat, &d, &d);
        CameraDolly(cam, &d);
    }
    if (PTC->joy.sx) {
        Vec axis = {0.0f, 1.0f, 0.0f};
        if (PTC->x62F) {
            CameraRotAxisPosRad(cam, &axis, &cam->param.pos, (f32) PTC->joy.sx / 20.0f * 0.017453292f);
        } else {
            CameraRotAxisPosRad(cam, &axis, &cam->param.at, (f32) PTC->joy.sx / 20.0f * 0.017453292f);
        }
    }
    if (PTC->joy.sy) {
        if (PTC->x62F) {
            CameraTargetRot(cam, 'x', (f32) PTC->joy.sy / -20.0f * 0.017453292f);
        } else {
            CameraCamposRot(cam, 'x', (f32) PTC->joy.sy / -20.0f * 0.017453292f);
        }
    }
    CameraDrawTarget(cam, 1);
    drawGround(0);
}

void tcPreviewOnOff(int on)
{
    PTC->preview = on;
    CamCtrl.flags_2C = (CamCtrl.flags_2C & ~1) | 0x10;
    if (PTC->preview) {
        pG->flags_60 &= ~0x10000000;
    } else {
        pG->flags_60 |= 0x10000000;
    }
}

int tcCurrentCameraNo()
{
    return PTC->cdatNo;
}
