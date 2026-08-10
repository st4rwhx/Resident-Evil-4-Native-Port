// Sscrn/ss_file: file (document) screen of the sub screen DLL (D:/Bio4/Prog/ss_file.cpp).
#include "types.h"
#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "item.h"
#include "cockpit.h"
#include "mes.h"
#include "id_sys.h"
#include "fade.h"
#include "dvd.h"
#include "main_mem.h"
#include "main.h"
#include "snd.h"
#include "option.h"
#include "db_log.h"
#include "sscrn.h"
#include "ss_main.h"

extern "C" {
int sprintf(char* s, const char* fmt, ...);
}

// COMPILER-DIFF: item 4 (narrow-argument truncation). The font sizes are s16 table entries passed to
// the s8 parameters without the `extsb` our compiler adds: s16 view of MessageControl::setFontSize.
class MessageControlS : public MessageControl {
public:
    void setFontSizeS(int no, s16 w, s16 h) asm("setFontSize__14MessageControliScSc");
};
#define cMesS (*(MessageControlS*) &cMes)

// Message slot address written out as one expression on a pointer variable (not the getMes inline):
// a reference argument built from it is computed in place into the parameter register, so cse
// loses the `slot * sizeof` product and gcse PRE re-copies it for the next store (the `mr` +
// duplicated `add` chain of dispFileList / mes.cpp setLayout).
#define SS_MES(pm, no) ((Message*) ((no) * sizeof(Message) + (u32) (pm) + sizeof(u32)))

#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

// File screen state (SUB_SCREEN::pFileWk, 0x10 bytes).
struct SsFileWork {
    s8 mode;      // 0x00  0 category select, 1 file select
    s8 cat;       // 0x01  category (0..2)
    s8 scroll;    // 0x02  first listed file
    s8 cursor;    // 0x03  file within the category (0 = the terminal log)
    u8 page;      // 0x04
    u8 msgBase;   // 0x05  message number of page 0
    u8 pageNum;   // 0x06
    u8 x7;        // 0x07  MesSet colour
    u32 attr;     // 0x08  MesSet attribute (getMsgAttr)
    u8 layout;    // 0x0C  0..2: cMes layout 7 / 8 / 4
    u8 fileNo;    // 0x0D  1..29 (0 = the terminal log)
};

// The widget classes (SsFileInit / SsFileMain / FileSelect / MessageDisplay) are declared in ss_main.h.

extern "C" {
int getMsgNum(int no);
u32 getMsgAttr(u32 type);
int getTplName(int no, u32 page);
void setLogMesAddr(SUB_SCREEN* wk);
void fileCameraInit(SUB_SCREEN* wk, Camera* cam);
int fileId2No(u16 id);
u16 fileNo2Id(int no);
int fileNo(int cat, int no);
void sscrn_file_out_init(SUB_SCREEN* wk);
void dispFileList(SUB_SCREEN* wk, int n);
// ss_main.cpp
void sscrnCameraInit(SUB_SCREEN* wk, Camera* cam);
int sscrnKey2Game(SUB_SCREEN* wk);
void dispScrollBar(int top, int n, int num, IdUnit* bar, IdUnit* up, IdUnit* down);
void generalModelAlloc(SUB_SCREEN* wk);
void sscrnModelFree(SUB_SCREEN* wk);
int sscrnMainMenu(SUB_SCREEN* wk);
// ss_model.cpp
void playerModelInit();
}

// files per category
int fileNum[3] = {13, 11, 8};
// message numbers of every page (unreferenced)
u16 filePageTbl[29][8] = {
    {0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x1F, 0x0},
    {0x2A, 0x2A, 0x2A, 0x2A, 0x2B, 0x2B, 0x2B, 0x0},
    {0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3E, 0x0, 0x0},
    {0x4A, 0x4A, 0x4A, 0x4A, 0x4A, 0x4A, 0x0, 0x0},
    {0x6A, 0x6A, 0x6A, 0x6A, 0x6A, 0x6A, 0x0, 0x0},
    {0x7A, 0x7A, 0x7A, 0x7A, 0x7B, 0x7B, 0x7B, 0x0},
    {0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0x9F, 0x0},
    {0x10A, 0x10A, 0x10A, 0x10A, 0x10A, 0x10A, 0x10A, 0x0},
    {0x11A, 0x11A, 0x11A, 0x11A, 0x11A, 0x11A, 0x0, 0x0},
    {0x12A, 0x12A, 0x12A, 0x12A, 0x12A, 0x0, 0x0, 0x0},
    {0x13A, 0x13A, 0x13B, 0x13B, 0x13B, 0x0, 0x0, 0x0},
    {0x14A, 0x14A, 0x14A, 0x14A, 0x14A, 0x0, 0x0, 0x0},
    {0x15A, 0x15A, 0x15A, 0x15A, 0x15A, 0x15A, 0x15A, 0x0},
    {0x16A, 0x16A, 0x16A, 0x16A, 0x16B, 0x16B, 0x16B, 0x16B},
    {0x17A, 0x17A, 0x17A, 0x17A, 0x17A, 0x17A, 0x0, 0x0},
    {0x18A, 0x18A, 0x18A, 0x18A, 0x18A, 0x18A, 0x0, 0x0},
    {0x19A, 0x19A, 0x19A, 0x19A, 0x19A, 0x0, 0x0, 0x0},
    {0x20A, 0x20A, 0x20A, 0x20A, 0x20A, 0x0, 0x0, 0x0},
    {0x21A, 0x21A, 0x21A, 0x21B, 0x21B, 0x21B, 0x21B, 0x0},
    {0x22A, 0x22A, 0x22A, 0x22A, 0x22A, 0x0, 0x0, 0x0},
    {0x23A, 0x23A, 0x23A, 0x23A, 0x23A, 0x23A, 0x23A, 0x0},
    {0x24A, 0x24A, 0x24A, 0x0, 0x0, 0x0, 0x0, 0x0},
    {0x25A, 0x25A, 0x25A, 0x25A, 0x25A, 0x25A, 0x0, 0x0},
    {0x26A, 0x26A, 0x26A, 0x26A, 0x26A, 0x26A, 0x0, 0x0},
    {0x27A, 0x27A, 0x27A, 0x27A, 0x27A, 0x0, 0x0, 0x0},
    {0x28A, 0x28A, 0x28A, 0x28A, 0x28A, 0x0, 0x0, 0x0},
    {0x29A, 0x29A, 0x29A, 0x29A, 0x29A, 0x0, 0x0, 0x0},
    {0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x0},
    {0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x8A, 0x0},
};
// per file: message number of page 0, colour, attribute type, layout
u8 fileInfo[29][4] = {
    {0x00, 0x00, 0x02, 0x01}, {0x05, 0x00, 0x01, 0x00}, {0x0C, 0x00, 0x02, 0x01}, {0x10, 0x00, 0x01, 0x00},
    {0x15, 0x00, 0x01, 0x00}, {0x17, 0x00, 0x01, 0x00}, {0x1C, 0x00, 0x01, 0x00}, {0x22, 0x00, 0x01, 0x00},
    {0x26, 0x00, 0x02, 0x01}, {0x2B, 0x00, 0x01, 0x00}, {0x32, 0x00, 0x01, 0x00}, {0x37, 0x00, 0x01, 0x00},
    {0x3B, 0x00, 0x01, 0x00}, {0x3F, 0x00, 0x01, 0x00}, {0x46, 0x00, 0x01, 0x00}, {0x50, 0x00, 0x01, 0x00},
    {0x58, 0x00, 0x01, 0x00}, {0x5D, 0x00, 0x01, 0x00}, {0x63, 0x00, 0x01, 0x00}, {0x67, 0x00, 0x01, 0x00},
    {0x6C, 0x00, 0x01, 0x00}, {0x74, 0x00, 0x01, 0x00}, {0x78, 0x00, 0x01, 0x00}, {0x7F, 0x00, 0x01, 0x00},
    {0x81, 0x00, 0x01, 0x00}, {0x85, 0x00, 0x01, 0x00}, {0x8C, 0x00, 0x01, 0x00}, {0x90, 0x00, 0x01, 0x00},
    {0x96, 0x00, 0x01, 0x00},
};
// pages per file, per language (only columns 0 / 1 are used: JP / others)
u8 fileMsgNum[29][6] = {
    {5, 5, 5, 5, 5, 5}, {6, 7, 6, 6, 6, 6},  {4, 4, 4, 4, 4, 4}, {5, 5, 5, 5, 5, 5}, {2, 2, 2, 2, 2, 2},
    {5, 5, 5, 5, 5, 5}, {6, 6, 6, 6, 6, 6},  {4, 4, 4, 4, 4, 4}, {5, 5, 5, 5, 5, 5}, {6, 7, 6, 6, 6, 6},
    {5, 5, 5, 5, 5, 5}, {4, 4, 4, 4, 4, 4},  {4, 4, 4, 4, 4, 4}, {4, 7, 4, 4, 4, 4}, {6, 10, 6, 6, 6, 6},
    {7, 8, 7, 7, 7, 7}, {5, 5, 5, 5, 5, 5},  {5, 6, 5, 5, 5, 5}, {4, 4, 4, 4, 4, 4}, {4, 5, 4, 4, 4, 4},
    {6, 8, 6, 6, 6, 6}, {4, 4, 4, 4, 4, 4},  {6, 7, 6, 6, 6, 6}, {2, 2, 2, 2, 2, 2}, {5, 4, 5, 5, 5, 5},
    {5, 7, 5, 5, 5, 5}, {4, 4, 4, 4, 4, 4},  {4, 6, 4, 4, 4, 4}, {5, 6, 5, 5, 5, 5},
};
// picture pages per file: count, then the first page of every picture (JP / others)
u8 fileTplJpn[29][8] = {
    {5, 1, 2, 3, 4, 5, 0, 0}, {2, 1, 5, 0, 0, 0, 0, 0}, {4, 1, 2, 3, 4, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {2, 1, 5, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},
    {5, 1, 2, 3, 4, 5, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},
    {2, 1, 3, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {2, 1, 5, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},
    {2, 1, 4, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 0, 0},
};
u8 fileTplEng[29][8] = {
    {5, 1, 2, 3, 4, 5, 0, 0}, {2, 1, 5, 0, 0, 0, 0, 0}, {4, 1, 2, 3, 4, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {2, 1, 5, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},
    {5, 1, 2, 3, 4, 5, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},
    {2, 1, 3, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {2, 1, 6, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},
    {2, 1, 4, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 0, 0},
};

// One-element array: the in-struct store keeps the following `state++` load below it and stops cse
// folding case 1's `state++` (alias.c: a fixed scalar never aliases a varying struct member).
static int file_wait[1] = {0};
static s16 file_title_w[2] = {0, 17};
static s16 file_title_h[2] = {0, 19};
static s8 file_title_space[4] = {0, 0, 0, 0};
static s16 file_name_w[2] = {0, 17};
static s16 file_name_h[2] = {0, 19};
static s8 file_name_space[4] = {0, 0, 0, 0};
static char file_tpl_name[16] = "SS/___/f00a.tpl";
static int file_tpl_x = 0;
static int file_tpl_y = 0x38;
static int file_tpl_w = 0x280;
static int file_tpl_h = 0x150;

static int file_read_req;
static int file_tpl_req;
u8* fileLogMes[32];

int getMsgNum(int no)
{
    int ret;

    if (pSys->language == 0) {
        ret = fileMsgNum[no][0];
    } else {
        ret = fileMsgNum[no][1];
    }
    return ret;
}

u32 getMsgAttr(u32 type)
{
    u32 ret = 0;

    switch (type) {
    case 1:
        ret = 0x30054;
        break;
    case 2:
        ret = 0x80054;
        break;
    case 3:
        ret = 0x3010054;
        break;
    }
    return ret;
}

int getTplName(int no, u32 page)
{
    int ret = 0;
    int i;
    int n;

    if (pSys->language == 0) {
        n = fileTplJpn[no][0];
        for (i = 0; i < n; i++) {
            if (page + 1 >= fileTplJpn[no][1 + i]) {
                ret++;
            } else {
                break;
            }
        }
    } else {
        n = fileTplEng[no][0];
        for (i = 0; i < n; i++) {
            if (page + 1 >= fileTplEng[no][1 + i]) {
                ret++;
            } else {
                break;
            }
        }
    }
    return ret;
}

void setLogMesAddr(SUB_SCREEN* wk)
{
    u8** p = fileLogMes;

    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0xA);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0xB);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0xC);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0xD);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0xE);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0xF);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x10);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x11);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x12);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x13);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x14);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x15);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x16);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x17);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x18);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x19);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x1A);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x1B);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x1C);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x1D);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x1E);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x1F);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x20);
    *p++ = (u8*) SS_ARC_PTR(wk->pFile, 0x21);
}

void fileCameraInit(SUB_SCREEN* wk, Camera* cam)
{
    sscrnCameraInit(wk, cam);
}

void SsFileInit::init(SUB_SCREEN* wk)
{
    if (wk->type == 0x40) {
        state = 2;
    } else {
        state = 0;
    }
}

void SsFileInit::move(SUB_SCREEN* wk)
{
    switch (state) {
    case 0:
        if (wk->x4C(wk) != 1) {
            break;
        }
        if (wk->x266 == 2) {
            wk->x44 = 1;
        }
        IdSubErase();
        IdNumErase();
        IdFreeBuffer();
        IdSub.set(SS_ARC_PTR(wk->pCmmn, 0xC), 0xFF, 0x14, 0xC, 6, 0);
        file_wait[0] = 0;
        state++;
        break;
    case 1:
        if (--file_wait[0] >= 0) {
            break;
        }
        state++;
        break;
    case 2:
        IdSys.dispSw(0x21, 1);
        IdSub.dispSw(2, 1);
        sscrnDataFilename(wk, "ss_file.dat");
#line 437 "D:/Bio4/Prog/ss_file.cpp"
        file_read_req = DVD_READ_N(wk->path, wk->pPzzl, 0, 0, 0, 0x10);
        if (file_read_req <= 0) {
            break;
        }
        if (wk->x266 == 2 && wk->type != 0x40) {
            sscrnModelFree(wk);
            generalModelAlloc(wk);
            playerModelInit();
            sscrnLightClear(wk);
            {
                LifeMeter* life = &Cckpt.life;
                life->fix(1);
                life->frameIn();
            }
        } else {
            sscrnModelClear(wk);
        }
        wk->x44 = 0;
        state++;
    case 3: {
        int stat;
        int size;
        if (Dvd.ReadCheck(file_read_req, &stat, &size, 0) != 1) {
            break;
        }
        wk->pFile = wk->pPzzl;
        state++;
    }
    case 4:
        if (wk->type == 0x40) {
            FadeSetW(0x80000000, 5, 0, 0);
        }
        transit(0, wk);
        break;
    }
}

int fileId2No(u16 id)
{
    if (id >= 0xAC && id <= 0xB7) {
        return id - 0xAB;
    }
    if (id >= 0x48 && id <= 0x50) {
        return id - 0x3B;
    }
    if (id >= 0xF4 && id <= 0xFD) {
        return id - 0xDE;
    }
    return 0;
}

u16 fileNo2Id(int no)
{
    if (no <= 0xC) {
        return no + 0xAB;
    }
    if (no <= 0x15) {
        return no + 0x3B;
    }
    if (no <= 0x1F) {
        return no + 0xDE;
    }
    return 0xAC;
}

int fileNo(int cat, int no)
{
    int ret = 0;

    switch (cat) {
    case 0:
        ret = no;
        break;
    case 1:
        ret = no + 0xC;
        break;
    case 2:
        ret = no + 0x16;
        break;
    }
    return ret;
}

void SsFileMain::init(SUB_SCREEN* wk)
{
    sel = new FileSelect;
    disp = new MessageDisplay;
    sel->connect(0, disp);
    disp->connect(0, sel);
    fileCameraInit(wk, &pG->Cam);
    IdTexDataLoad(SS_ARC_PTR(wk->pFile, 6), 9);
    if (IdSub.setCk(0x14) == 0) {
        IdSub.set(SS_ARC_PTR(wk->pCmmn, 0xC), 0xFF, 0x14, 0xC, 6, 0);
    }
    IdSub.set(SS_ARC_PTR(wk->pFile, 7), 0xFF, 0x19, 9, 2, 0);
    IdSub.set(SS_ARC_PTR(wk->pFile, 9), 0xFF, 0x1D, 0x13, 4, 0);
    IdSub.set(SS_ARC_PTR(wk->pFile, 8), 0xFF, 0x1E, 0x13, 2, 0);
    IdSub.unitPtr(0, 0x1D)->flags &= ~8;
    IdSub.unitPtr(0, 0x1E)->flags &= ~8;
    IdSub.unitPtr(0, 0x1E)->dir |= 0xF;
    IdSub.unitPtr(4, 0x1E)->flags &= ~8;
    sscrnLightCreate(wk, (cLit*) SS_ARC_PTR(wk->pCmmn, 0x12));
    if (wk->x266 == 2 && wk->type != 0x40) {
        wk->x269 = 0;
        wk->x26A = 10;
    }
    MesData.setPtr(0, (u8*) SS_ARC_PTR(wk->pCmmn, 5));
    MesData.setPtr(2, (u8*) SS_ARC_PTR(wk->pFile, 5));
    sscrnMainMenuInit(wk, 0);
    state = 0;
#line 623 "D:/Bio4/Prog/ss_file.cpp"
    wk->pFileWk = (SsFileWork*) MEM_ALLOC(0x10, 1, 13);
    wk->pFileWk->mode = 0;
    wk->pFileWk->cat = 0;
    wk->pFileWk->scroll = 0;
    wk->pFileWk->cursor = 0;
#line 629 "D:/Bio4/Prog/ss_file.cpp"
    wk->pTplBuf = MEM_ALLOC(0x20000, 1, 13);
    cMes.setLayout(0, 2);
    if (wk->type == 0x40) {
        int no = fileId2No(wk->x2FA);
        ItemMgr.get(wk->x2FA, 0);
        S32Set(wk->x40, 1);
        if (pSys->language == 0) {
            cMes.setupFont(0x1C, 0x1C, (TEXPalette*) SS_ARC_PTR(wk->pFile, 4), 3);
        }
        {
            u8* p = fileInfo[no - 1];
            wk->pFileWk->msgBase = p[0];
            wk->pFileWk->x7 = p[1];
            wk->pFileWk->attr = getMsgAttr(p[2]);
            wk->pFileWk->layout = p[3];
            wk->pFileWk->pageNum = getMsgNum(no - 1);
            wk->pFileWk->page = 0;
            wk->pFileWk->cursor = 1;
            wk->pFileWk->fileNo = no;
        }
        cur = disp;
        cur->init(wk);
        sndWait = 0;
    } else {
        cur = sel;
        SndCall(0, 0x1E, 0, 0, 0, 0);
        sndCnt = 0;
        sndWait = 1;
    }
}

void SsFileMain::move(SUB_SCREEN* wk)
{
    switch (state) {
    case 0: {
        Widget<SUB_SCREEN>* w = cur;
        w->move(wk);
        next = w->cur;
        if (cur == sel) {
            switch (sel->state) {
            case 2:
                state = 1;
                wk->x34 |= 0x10;
                sscrnMainMenuInit(wk, 1);
                SndCall(0, 0xA, 0, 0, 0, 0);
                break;
            case 1:
                wk->x34 |= 0x10;
                transit(4, wk);
                break;
            }
        }
        cur = next;
        break;
    }
    case 1:
        if (sscrnMainMenu(wk)) {
            switch ((s8) wk->x264) {
            case 1:
                transit(0, wk);
                break;
            case 0:
                transit(1, wk);
                break;
            case 3:
                sscrnMainMenuInit(wk, 0);
                state = 0;
                break;
            case 2:
                transit(3, wk);
                break;
            case 4:
                transit(4, wk);
                break;
            }
        }
        if (Key.trg & 0x02000000) {
            sscrnMainMenuInit(wk, 0);
            state = 0;
        }
        dispFileList(wk, 5);
        break;
    }
    if (sndWait) {
        if (++sndCnt > 10) {
            sndWait = 0;
            SndCall(0, 0x1F, 0, 0, 0, 0);
        }
    }
}

static int sscrn_file_out(SUB_SCREEN* wk);

void SsFileMain::quit(SUB_SCREEN* wk)
{
    if (sel) {
        delete sel;
    }
    if (disp) {
        delete disp;
    }
    Mem_free(wk->pTplBuf);
    Mem_free(wk->pFileWk);
    sscrn_file_out_init(wk);
    wk->x4C = sscrn_file_out;
}

void sscrn_file_out_init(SUB_SCREEN* wk)
{
    IdSub.unitPtr(0, 0x19)->dir |= 1;
    if (wk->x265 == 2) {
        Cckpt.life.frameOut();
        wk->x269 = 1;
    }
}

static int sscrn_file_out(SUB_SCREEN* wk)
{
    if (IdSub.unitPtr(0, 0x19)->end & 1) {
        return 1;
    }
    return 0;
}

void dispFileList(SUB_SCREEN* wk, int n)
{
    SsFileWork* fw = wk->pFileWk;
    int num = fileNum[fw->cat];
    int top = fw->scroll;
    IdUnit* bar = IdSub.unitPtr(0xFC, 0x19);
    IdUnit* up = IdSub.unitPtr(0xFE, 0x19);
    IdUnit* u;
    IdUnit* pos;
    int i;
    int k = 0;
    int x;
    int y;

    dispScrollBar(top, n, num, bar, up, IdSub.unitPtr(0xFD, 0x19));
    if (fw->mode == 0) {
        IdUnit* c = IdSub.unitPtr(0x10, 0x19);
        IdSub.unitPtr(1, 0x19)->scr = c->scr;
    }
    u = IdSub.unitPtr(0xF9, 0x19);
    mes_col_tbl[8] = (u8) u->col[0] << 24;
    mes_col_tbl[8] |= (u8) u->col[1] << 16;
    mes_col_tbl[8] |= (u8) u->col[2] << 8;
    mes_col_tbl[8] |= (u8) u->col[3];
    u = IdSub.unitPtr(0xF8, 0x19);
    mes_col_tbl[9] = (u8) u->col[0] << 24;
    mes_col_tbl[9] |= (u8) u->col[1] << 16;
    mes_col_tbl[9] |= (u8) u->col[2] << 8;
    mes_col_tbl[9] |= (u8) u->col[3];
    pos = IdSub.unitPtr(0x20, 0x19);
    x = (int) ((pos->scr.x + 320.0f) * 0.8f);
    y = (int) ((240.0f - pos->scr.y) * 0.8f);
    cMesS.setFontSizeS(4, file_title_w[1], file_title_h[1]);
    cMes.getMes(4)->lineH = 0;
    cMes.getMes(4)->charSpace = file_title_space[3];
    cMes.MesSet(fw->cat + 3, x, y, 0x20081, 4, 8, 3);
    for (i = top; i < top + n; i++, k++) {
        int id = 0;
        int col;
        u8 slot;
        int x;
        int y;
        if (fw->mode == 1) {
            IdUnit* c = IdSub.unitPtr(k + 0x11, 0x19);
            if (i == fw->cursor) {
                IdSub.unitPtr(1, 0x19)->scr = c->scr;
            }
        }
        if (i == 0) {
            col = 9;
            if (OpeGetMdtNo() != 0x18) {
                col = 8;
            }
        } else {
            int no = fileNo(fw->cat, i);
            if (ItemMgr.search(fileNo2Id(no)) == 0) {
                continue;
            }
            col = 8;
            id = fileNo2Id(no);
        }
        {
            IdUnit* p = IdSub.unitPtr(k + 0x21, 0x19);
            slot = k + 8;
            x = (int) ((p->scr.x + 320.0f) * 0.8f);
            y = (int) ((240.0f - p->scr.y) * 0.8f);
        }
        cMesS.setFontSizeS(slot, file_name_w[1], file_name_h[1]);
        {
            MessageControl* pm = &cMes;
            u16 zero = 0;
            U16Set(SS_MES(pm, slot)->charSpace, file_name_space[3]);
            SS_MES(pm, slot)->lineH = zero;
            if (i == 0) {
                pm->MesSet(2, x, y, 0x20081, slot, col, 3);
            } else {
                pm->MesSet(id, x, y, 0x20088, slot, col, 4);
            }
        }
    }
}

void FileSelect::init(SUB_SCREEN* wk)
{
}

void FileSelect::move(SUB_SCREEN* wk)
{
    SsFileWork* fw = wk->pFileWk;

    dispFileList(wk, 5);
    state = 0;
    switch (fw->mode) {
    case 0:
        if (sscrnKey2Game(wk)) {
            state = 1;
            return;
        }
        if ((Key.trg & 0x40000000) || (Key.trg & 0x01000000)) {
            if (wk->type == 0x40) {
                return;
            }
            state = 2;
            return;
        }
        if ((Key.trg & 0x80000000) || (Key.trg & 0x02000000)) {
            fw->mode = 1;
            SndCall(0, 0x20, 0, 0, 0, 0);
            return;
        }
        {
            s8 old = fw->cat;
            if (Key.rep & 0x08000000) {
                fw->cat--;
            }
            if (Key.rep & 0x04000000) {
                fw->cat++;
            }
            if (pG->x4F8E != 0) {
                fw->cat = fw->cat < 0 ? 0 : (fw->cat > 2 ? 2 : fw->cat);
            } else {
                int st = (s8) wk->stage;
                if (st > 0) {
                    fw->cat = fw->cat < 0 ? 0 : (fw->cat > st - 1 ? st - 1 : fw->cat);
                } else {
                    fw->cat = 0;
                }
            }
            if (old != fw->cat) {
                fw->scroll = 0;
                fw->cursor = 0;
                SndCall(0, 0x21, 0, 0, 0, 0);
            }
        }
        break;
    case 1:
        if ((Key.trg & 0x40000000) || ((Key.trg & 0x01000000) && fw->cursor == 0)) {
            fw->mode = 0;
            SndCall(0, 0x20, 0, 0, 0, 0);
            return;
        }
        if (Key.trg & 0x80000000) {
            int ok = 0;
            if (fw->cursor > 0) {
                if (ItemMgr.search(fileNo2Id(fileNo(fw->cat, fw->cursor)))) {
                    MessageControl* m = &cMes;
                    int i;
                    int no;
                    for (i = 0; i < 16; i++) {
                        m->Delete(i);
                    }
                    MesData.setPtr(2, (u8*) SS_ARC_PTR(wk->pFile, 5));
                    ok = 1;
                    no = fileNo(fw->cat, fw->cursor);
                    {
                        u8* p = fileInfo[no - 1];
                        fw->msgBase = p[0];
                        fw->x7 = p[1];
                        fw->attr = getMsgAttr(p[2]);
                        fw->layout = p[3];
                    }
                    fw->pageNum = getMsgNum(no - 1);
                    fw->page = 0;
                    fw->fileNo = no;
                }
            } else {
                int mdt = OpeGetMdtNo();
                if (mdt != 0x18) {
                    MessageControl* m = &cMes;
                    int i;
                    for (i = 0; i < 16; i++) {
                        m->Delete(i);
                    }
                    setLogMesAddr(wk);
                    ok = 1;
                    MesData.setPtr(2, fileLogMes[mdt]);
                    fw->msgBase = 0;
                    fw->x7 = 0;
                    fw->attr = getMsgAttr(3);
                    fw->layout = 2;
                    fw->pageNum = MesData.getMesNum(2);
                    fw->page = 0;
                    fw->fileNo = 0;
                }
            }
            if (ok == 1) {
                transit(0, wk);
                SndCall(0, 4, 0, 0, 0, 0);
            } else {
                SndCall(0, 0x24, 0, 0, 0, 0);
            }
            return;
        }
        {
            s8 old = fw->cursor;
            if (Key.rep & 0x01000000) {
                fw->cursor--;
            }
            if (Key.rep & 0x02000000) {
                fw->cursor++;
            }
            fw->cursor = fw->cursor < 0 ? 0 : (fw->cursor > fileNum[fw->cat] - 1 ? fileNum[fw->cat] - 1 : fw->cursor);
            if (old != fw->cursor) {
                SndCall(0, 0x20, 0, 0, 0, 0);
            }
            if (fw->cursor > fw->scroll + 4) {
                fw->scroll = fw->cursor - 4;
            }
            if (fw->cursor < fw->scroll) {
                fw->scroll = fw->cursor;
            }
        }
        break;
    }
}

void FileSelect::quit(SUB_SCREEN* wk)
{
}

void MessageDisplay::init(SUB_SCREEN* wk)
{
    IdUnit* pos = IdSub.unitPtr(0xFE, 0x1E);
    IdUnit* u;
    SsFileWork* fw;

    S16Set(x, (int) ((pos->scr.x + 320.0f) * 0.8f));
    S16Set(y, (int) ((240.0f - pos->scr.y) * 0.8f));
    if (pSys->language == 0) {
        cMes.setupFont(0x1C, 0x1C, (TEXPalette*) SS_ARC_PTR(wk->pFile, 4), 3);
    }
    switch (wk->pFileWk->layout) {
    case 0:
        cMes.setLayout(0, 7);
        break;
    case 1:
        cMes.setLayout(0, 8);
        break;
    case 2:
        cMes.setLayout(0, 4);
        break;
    }
    fw = wk->pFileWk;
    cMes.MesSet(fw->msgBase + fw->page, x, y, fw->attr, 0, fw->x7, 4);
    IdSub.unitPtr(0, 0x1D)->flags |= 8;
    IdSub.unitPtr(0, 0x1E)->flags |= 8;
    IdSub.unitPtr(0, 0x1E)->dir &= ~0xF;
    u = IdSub.unitPtr(0xFC, 0x1E);
    if (wk->pFileWk->layout == 1) {
        u->scr = IdSub.unitPtr(0xFD, 0x1E)->scr;
    } else {
        u->scr = IdSub.unitPtr(0xFB, 0x1E)->scr;
    }
    state = 0;
    tplState = 0;
    tplFirst = 1;
}

void MessageDisplay::move(SUB_SCREEN* wk)
{
    SsFileWork* fw = wk->pFileWk;
    u8 page = fw->page;
    u16 cur;
    u16 nxt;
    int no;

    switch (state) {
    case 0:
        if (Key.trg & 0x40000000) {
            if (tplFirst == 0) {
                Dvd.ReadCancel(file_tpl_req, 0x40);
            }
            state = 1;
            break;
        }
        if (Key.trg & 0x80000000) {
            s8 c = fw->cursor;
            if (c == 0) {
                cMes.WaitEnd(0);
                if (cMes.mes[c].flags2 & 2) {
                    fw->page++;
                    if (fw->page >= fw->pageNum) {
                        state = 1;
                        break;
                    }
                }
            } else {
                if (fw->page >= fw->pageNum - 1) {
                    state = 1;
                    break;
                }
                fw->page++;
                goto CHANGE;
            }
        }
        if (Key.trg & 0x08000000) {
            if (fw->page != 0) {
                fw->page = fw->page - 1;
            }
        } else if (Key.trg & 0x04000000) {
            if (fw->page < fw->pageNum - 1) {
                fw->page = fw->page + 1;
            }
        }
    CHANGE:
        if (page != fw->page) {
            cMes.MesSet(fw->msgBase + fw->page, x, y, fw->attr, 0, fw->x7, 4);
            SndCall(0, 0x22, 0, 0, 0, 0);
        }
        break;
    case 1:
        IdSub.unitPtr(0, 0x1E)->dir |= 0xF;
        SndCall(0, 0x23, 0, 0, 0, 0);
        state = 2;
        break;
    case 2:
        if ((s16) IdSub.unitPtr(0, 0x1E)->timer[2] == 0) {
            transit(0, wk);
        }
        break;
    }
    no = fw->fileNo;
    cur = getTplName(no - 1, page);
    nxt = getTplName(no - 1, fw->page);
    switch (tplState) {
    case 0:
        if (cur != nxt || tplFirst == 1) {
            tplState = 1;
        }
        if (tplFirst == 0 && nxt != 0xFFFF) {
            ss_Draw_tpl(wk->pTplBuf, 0, file_tpl_x, file_tpl_y, file_tpl_w, file_tpl_h, 0x13, 3);
        }
        break;
    case 1:
        if (tplFirst == 1) {
            tplFirst = 0;
        }
        {
            // The request slot's address is taken before the sprintf (`lis` in a callee-saved
            // register across the three calls): a plain `x = call()` expands the call first.
            int* pReq = &file_tpl_req;
            sprintf(file_tpl_name, "SS/___/f%02dA.tpl", no);
            setLangExt3(file_tpl_name + 3);
            file_tpl_name[10] = nxt + 0x60;
#line 1338 "D:/Bio4/Prog/ss_file.cpp"
            *pReq = DVD_READ_N(file_tpl_name, wk->pTplBuf, 0, 0, 0, 0x10);
        }
        tplState = 2;
        break;
    case 2:
        if (Dvd.ReadCheck(file_tpl_req, 0, 0, 0)) {
            tplFirst = 0;
            tplState = 0;
        } else if (cur != nxt) {
            Dvd.ReadCancel(file_tpl_req, 0x40);
            tplState = 1;
        }
        break;
    }
    {
        int d[2];
        int e[2];
        int v;
        int i;
        IdUnit* u;
        IdUnit* a;
        IdUnit* b;

        v = fw->page + 1;
        for (i = 0; i < 2; i++) {
            d[i] = v % 10;
            v /= 10;
        }
        v = fw->pageNum;
        for (i = 0; i < 2; i++) {
            e[i] = v % 10;
            v /= 10;
        }
        if (fw->pageNum <= 9) {
            IdSub.unitPtr(0x10, 0x1E)->flags &= ~8;
            IdSub.unitPtr(0x13, 0x1E)->flags &= ~8;
            u = IdSub.unitPtr(0x11, 0x1E);
            u->flags_7F |= 2;
            u->no = d[0];
            u = IdSub.unitPtr(0x12, 0x1E);
            u->flags_7F |= 2;
            u->no = e[0];
        } else {
            IdSub.unitPtr(0x10, 0x1E)->flags |= 8;
            IdSub.unitPtr(0x13, 0x1E)->flags |= 8;
            u = IdSub.unitPtr(0x10, 0x1E);
            u->flags_7F |= 2;
            u->no = d[1];
            u = IdSub.unitPtr(0x11, 0x1E);
            u->flags_7F |= 2;
            u->no = d[0];
            u = IdSub.unitPtr(0x12, 0x1E);
            u->flags_7F |= 2;
            u->no = e[1];
            u = IdSub.unitPtr(0x13, 0x1E);
            u->flags_7F |= 2;
            u->no = e[0];
        }
        {
            IdUnit* p = IdSub.unitPtr(1, 0x1E);
            if (fw->page == 0) {
                p->flags &= ~8;
            } else {
                p->flags |= 8;
            }
        }
        a = IdSub.unitPtr(2, 0x1E);
        b = IdSub.unitPtr(3, 0x1E);
        if (fw->page == fw->pageNum - 1) {
            a->flags &= ~8;
            b->flags |= 8;
        } else {
            a->flags |= 8;
            b->flags &= ~8;
        }
    }
}

void MessageDisplay::quit(SUB_SCREEN* wk)
{
    MessageControl* m = &cMes;
    int i;

    for (i = 0; i < 16; i++) {
        m->Delete(i);
    }
    if (pSys->language == 0) {
        cMes.setupFont(0x1C, 0x1C, (TEXPalette*) SS_ARC_PTR(wk->pCmmn, 4), 3);
    }
    IdSub.unitPtr(0, 0x1D)->flags &= ~8;
}

// The split object's .data is 4 bytes longer than the variables: the next unit's (ss_item) .data
// starts 8-aligned in the REL.
asm(".section .data; .balign 8; .section .text");
