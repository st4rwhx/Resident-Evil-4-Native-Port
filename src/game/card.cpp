// game/card: memory card save/load screen (D:/Bio4/Prog/card.cpp).
#include "types.h"
#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "card.h"
#include "id_sys.h"
#include "mes.h"
#include "main.h"
#include "main_sub.h"
#include "scheduler.h"
#include "snd.h"
#include "cockpit.h"
#include "sscrn.h"
#include "sce.h"
#include "game.h"
#include "merchant.h"
#include "dvd.h"
#include "file.h"
#include "fade.h"
#include "eprintf.h"
#include "tpl.h"
#include "path.h"
#include "hermite.h"
#include "room_data.h"

typedef s64 OSTime;

struct OSCalendarTime {
    int sec;   // 0x00
    int min;   // 0x04
    int hour;  // 0x08
    int mday;  // 0x0C
    int mon;   // 0x10
    int year;  // 0x14
    int wday;  // 0x18
    int yday;  // 0x1C
    int msec;  // 0x20
    int usec;  // 0x24
};

extern "C" {
void OSReport(const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
void* memcpy(void* dst, const void* src, unsigned int n);
void DCFlushRange(void* addr, u32 nBytes);
OSTime OSGetTime();
void OSTicksToCalendarTime(OSTime ticks, OSCalendarTime* td);
OSTime OSCalendarTimeToTicks(OSCalendarTime* td);
void OSResetSystem(int reset, u32 resetCode, int forceMenu);
int DBIsDebuggerPresent();
void debugInfoDisp(int slot, int type);
void CRCInit();
u32 CRCCalc(u8* data, u32 len);
int CRCVerify(u8* data, u32 len, u32 saved);
void setMsgBG(int a, int b);
}

// cGameSave::save reads a mode in r5 (see emrock.cpp).
int GameSaveSave(cGameSave* g, void* data, int mode) asm("save__9cGameSavePv");

// Sub screen data archive (SndMem.sub_adr): offsets to its sub-files.
// Archive header shared by the sub screen sound data (SndMem.sub_adr: [0] icon/banner TPL,
// [1] message table) and ss/cmn/save_?.dat (CardID textures, save/load frames, file list, ...).
struct CardArc {
    u32 pad_0[4];
    u32 ofs[6];      // 0x10  offsets from the archive start
};

// Pointers of the game save block (pSaveData).
struct SaveDataPtrs {
    u8 pad_0[8];
    u8* p8;    // 0x08
    u8* pC;    // 0x0C
    u8* p10;   // 0x10  room save records (RoomData.num * 0xD8 + 0x10)
    u8* p14;   // 0x14  sub screen data
    u8* p18;   // 0x18  merchant data
};
#define SD ((SaveDataPtrs*) pSaveData)

// Save file record header: the first 0x200 bytes at SAVE_HDR are kept per file (pInfo).
struct SaveInfo {
    u32 crc;              // 0x00  of the 0x1FC bytes after it
    u32 magic;            // 0x04  0x116
    OSCalendarTime time;  // 0x08
    u32 mode;             // 0x30  1 normal, 2 (x8 & 0x20), 3 (x8 & 0x40)
    u8 pad_34[9];
    u8 x3D;               // 0x3D  difficulty (from game data 0x33D4)
    u8 chapter;           // 0x3E
    u8 pad_3F;
    u16 x40;              // 0x40
    u16 count;            // 0x42
    u8 pad_44[4];
    u32 playTime;         // 0x48
    u8 pad_4C[4];
    u16 room;             // 0x50
};

struct SaveHeader {
    u32 crc;              // 0x00
    u32 magic;            // 0x04
    OSCalendarTime time;  // 0x08
    u32 mode;             // 0x30
};

struct MesPos {
    u8 x;
    u8 y;
    u16 no;
};

#define ICON_NUM 1

// Save file image (0xEAFC bytes) and system file image (0x1E7C bytes).
#define SAVE_COMMENT2 0x20
#define SAVE_BANNER 0x40
#define SAVE_ICON 0x1840
#define SAVE_CLUT 0x1C40
#define SAVE_HDR 0x2000
#define SAVE_HDR_MAGIC 0x2004
#define SAVE_HDR_TIME 0x2008
#define SAVE_HDR_MODE 0x2030
#define SAVE_HDR_X3D 0x203D
#define SAVE_GAME 0x2034
#define SAVE_GAME_SIZE 0x36F8
#define SAVE_DATA2 0x572C
#define SAVE_DATA2_SIZE 0x1204
#define SAVE_ROOM 0x6930
#define SAVE_SSCRN 0xE7D0
#define SAVE_MERCHANT 0xE7D4
#define SAVE_CRC 0xEAF8
#define SAVE_SIZE 0xEAFC
#define SYS_WORK 0x1E40
#define SYS_CRC 0x1E78
#define SYS_SIZE 0x1E7C

static inline void U16Inc(u16& v) { v++; }
// Member read through a reference (no struct flag): stays below a preceding store to a static.
static inline s32 IRef(s32& v) { return v; }
static inline u32 bitChk(u32 f, u32 b) { return f & b; }

#define KEY_A 0x80000000
#define KEY_B 0x40000000
#define KEY_UP 0x01000000
#define KEY_DOWN 0x02000000
#define KEY_START 0x00080000
#define KEY_Z 0x00040000

// Card screen widgets (CardID), 0x78 bytes.
class CardID {
public:
    u32 x0;
    void* pTex;      // 0x04
    void* pSaveDat;  // 0x08  save frame
    void* pFile;     // 0x0C  file list entries (types 0x40..0x46)
    void* pFrame;    // 0x10
    void* pLoadDat;  // 0x14  load frame
    void* pBg;       // 0x18  message background
    s32 action;      // 0x1C  1 up, 2 down, 4 decided, 8 moving
    IDSystem idsys;  // 0x20
    s32 type;        // 0x70
    s8 state;        // 0x74
    s8 step;         // 0x75
    u8 x76;
    u8 x77;

    void updateSaveInfo(cCard* c);
    void init(int type, CardArc* data);
    void move(cCard* c);
    void wait(cCard* c);
    void start(cCard* c);
    void normal(cCard* c);
    void up_down(cCard* c);
    void save(cCard* c);
    void quit();
    void setAction(int a);
};

void dispSaveInfo(int no, SaveInfo* info, u8 type, int broken);
// cCard::exit passes the saved int width as a full word (`lwz`, not the `lhz 0x41a` narrowing a u16
// parameter gets): int view of ScreenReSize.
extern "C" void ScreenReSizeI(int w, int h) asm("ScreenReSize");
// CardID::updateSaveInfo passes `0x40 + i` without the `clrlwi` truncation: int view of `type`.
void dispSaveInfoI(int no, SaveInfo* info, int type, int broken) asm("dispSaveInfo__FiP8SaveInfoUci");

int isDbgInfoAlloc = 0;
static int isDbgInfoCached = 0;

// Struct-member view of the cache bits (the pLog trick): a load through it stays below a
// preceding fileFlag store (saveFileCheck).
struct IntView {
    int v;
};
#define DBG_CACHED (((IntView*) &isDbgInfoCached)->v)
static cCard* pCard = 0;
static CardID* g_id = 0;

char idpath[] = "ss/cmn/save_j.dat";
char fileext[] = "jeegfsie";

MesPos mes_pos_tbl_jpn[] = {
    { 0x50, 0x87, 0x0000 }, { 0x78, 0xD2, 0x0001 }, { 0x32, 0x6E, 0x0002 }, { 0x3C, 0x82, 0x0003 },
    { 0x3C, 0x6E, 0x0004 }, { 0x3C, 0x64, 0x0005 }, { 0x3C, 0x96, 0x0006 }, { 0x2D, 0x6E, 0x0007 },
    { 0x50, 0x8C, 0x0008 }, { 0x50, 0x6E, 0x0009 }, { 0x8C, 0xA0, 0x000A }, { 0x8C, 0xD2, 0x000B },
    { 0x8C, 0xD2, 0x000C }, { 0x8C, 0xD2, 0x000D }, { 0x8C, 0xD2, 0x000E }, { 0x6E, 0x8C, 0x000F },
    { 0x8C, 0xD2, 0x0010 }, { 0x6E, 0x8C, 0x0011 }, { 0x8C, 0xD2, 0x0012 }, { 0x8C, 0xD2, 0x0013 },
    { 0x64, 0xA0, 0x0014 }, { 0x28, 0x6E, 0x0015 }, { 0x3C, 0x6E, 0x0016 }, { 0x28, 0x64, 0x0017 },
    { 0x28, 0x5A, 0x0018 }, { 0x28, 0x5A, 0x0019 }, { 0x28, 0x5A, 0x001A }, { 0x28, 0x50, 0x001B },
    { 0x28, 0x64, 0x001C }, { 0x3C, 0x78, 0x001D }, { 0x50, 0x6E, 0x001E }, { 0x8C, 0xA0, 0x001F },
    { 0x28, 0x82, 0x0020 }, { 0x28, 0x6E, 0x0021 }, { 0x28, 0x64, 0x0022 }, { 0x28, 0x64, 0x0023 },
    { 0x28, 0x64, 0x0024 }, { 0x28, 0x6E, 0x0025 }, { 0x8C, 0xD2, 0x0026 }, { 0x28, 0x6E, 0x0027 },
    { 0x28, 0x6E, 0x0028 }, { 0x28, 0x6E, 0x0029 }, { 0x28, 0x6E, 0x002A }, { 0x28, 0x6E, 0x002B },
    { 0x28, 0x6E, 0x002C }, { 0x8C, 0x6E, 0x0026 }, { 0x28, 0x6E, 0x0027 },
};

MesPos mes_pos_tbl_usa[] = {
    { 0x64, 0x6E, 0x0000 }, { 0x64, 0xD2, 0x0001 }, { 0x64, 0x5A, 0x0002 }, { 0x64, 0x6E, 0x0003 },
    { 0x64, 0x6E, 0x0004 }, { 0x64, 0x6E, 0x0005 }, { 0x64, 0x6E, 0x0006 }, { 0x64, 0x6E, 0x0007 },
    { 0x64, 0x82, 0x0008 }, { 0x64, 0x6E, 0x0009 }, { 0x64, 0x96, 0x000A }, { 0x64, 0xD2, 0x000B },
    { 0x64, 0xD2, 0x000C }, { 0x64, 0xD2, 0x000D }, { 0x64, 0xD2, 0x000E }, { 0xC8, 0x6E, 0x000F },
    { 0x64, 0xD2, 0x0010 }, { 0xC8, 0x6E, 0x0011 }, { 0x64, 0xD2, 0x0012 }, { 0x64, 0xD2, 0x0013 },
    { 0x64, 0xD2, 0x0014 }, { 0x64, 0x8C, 0x0015 }, { 0x64, 0x6E, 0x0016 }, { 0x64, 0x46, 0x0017 },
    { 0x64, 0x46, 0x0018 }, { 0x64, 0x46, 0x0019 }, { 0x64, 0x46, 0x001A }, { 0x64, 0x46, 0x001B },
    { 0x64, 0x6E, 0x001C }, { 0x64, 0x5A, 0x001D }, { 0x64, 0x82, 0x001E }, { 0x64, 0x96, 0x001F },
    { 0x64, 0x6E, 0x0020 }, { 0x64, 0x5A, 0x0021 }, { 0x64, 0x5A, 0x0022 }, { 0x64, 0x5A, 0x0023 },
    { 0x64, 0x5A, 0x0024 }, { 0x64, 0x5A, 0x0025 }, { 0x64, 0xD2, 0x0026 }, { 0x64, 0x6E, 0x0027 },
    { 0x64, 0x96, 0x0028 }, { 0x64, 0x96, 0x0029 }, { 0x64, 0x6E, 0x002A }, { 0x64, 0x82, 0x002B },
    { 0x64, 0x6E, 0x002C }, { 0x64, 0x82, 0x0026 }, { 0x64, 0xD2, 0x0027 },
};

MesPos* mes_pos_tbl[8] = {
    mes_pos_tbl_jpn, mes_pos_tbl_usa, mes_pos_tbl_usa, mes_pos_tbl_usa,
    mes_pos_tbl_usa, mes_pos_tbl_usa, mes_pos_tbl_usa, mes_pos_tbl_usa,
};

Vec g_pos0_org;
u8* pDbgSaveInfo[20];
u32 CRCTable[256];

// Aggregates (MEM_IN_STRUCT_P): the stores in CardID::init stay ordered against the `u->` loads,
// which plain pointer variables would not be (scalar at a fixed address vs struct member).
static void* g_p_path_org[1];
static void* g_p_hrmt_org[1];
static void* g_p_spln_org[1];

#define ROUNDUP(x, a) (((x) + ((a) - 1)) / (a) * (a))

// Free blocks of a slot (free bytes rounded up to the sector size).
#define FREE_BLOCKS(s) (((s).sectorSize ? ROUNDUP((s).freeBytes, (s).sectorSize) : 0) / (s).sectorSize)

static inline int isLang(u8 lang, int n)
{
    return lang == n;
}

static inline int isEurope(u8 lang)
{
    if (isLang(lang, 2) || isLang(lang, 3) || isLang(lang, 4) || isLang(lang, 5) || isLang(lang, 6)) {
        return 1;
    }
    return 0;
}

static inline void deleteAllMes()
{
    MessageControl* m = &cMes;
    int i;
    for (i = 0; i < 16; i++) {
        m->Delete(i);
    }
}

void debugInfoDisp(int slot, int type)
{
    static u8 col_tbl[2] = { 0x14, 0x05 };

    if (pG->dev_mode == 1) {
        eprintf2(12, 16, 32, 38, col_tbl[slot == 0], 0, "CARD SLOT A");
        eprintf2(12, 16, 32, 62, col_tbl[slot == 2], 0, "HARD DISK");
    }
}

cCard::cCard()
{
}

cCard::~cCard()
{
}

void CardInit()
{
    CARDInit();
    CRCInit();
    pG->card_serial = 1;
}

void cCard::slotSelect()
{
    int i;

    flag = 0;
    switch (step) {
    case 0:
        if (unmount(0) == 1) {
            for (i = 0; i < 20; i++) {
                slotw[0].fileFlag[i] = 0;
                slotw[2].fileFlag[i] = 0;
            }
            step++;
        }
        break;
    case 1:
        if (pG->dev_mode == 1) {
            if (Key.trg & KEY_DOWN) {
                slot = 2;
            } else if (Key.trg & KEY_UP) {
                slot = 0;
            } else if (Key.trg & KEY_A) {
                mode = 1;
                step = 0;
                sub = 0;
                sub2 = 0;
                formatted = 0;
            } else if (Key.trg & KEY_B) {
                mode = 4;
                step = 0;
                sub = 0;
                sub2 = 0;
            }
        } else {
            mode = 1;
            step = 0;
            sub = 0;
            sub2 = 0;
        }
        break;
    }
    if (0) {
        eprintf(0, 0, 0, 0, "MES NO:%d", 0);
    }
}

void cCard::inSlotCheck()
{
    int ret;
    CardSlot* s;

    switch (step) {
    case 0:
        setMsgWindow(1, 1);
        if (pG->x8 & 0x80) {
            cardMesSet(0x2D, 0, 0);
        } else {
            cardMesSet(0x26, 0, 0);
        }
        if (unmount(0) == 1) {
            step++;
        }
        break;
    case 1:
        ret = existCheck(slot, &slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            step++;
        } else if (ret < 0) {
            errorSet(result);
        }
        break;
    case 2:
        ret = mount(&sub, &slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            step++;
        } else if (ret < 0) {
            errorSet(result);
        }
        break;
    case 3:
        ret = verifyCheck(&sub, &slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            step++;
            if (pG->x8 & 0x80) {
                step++;
            }
        } else if (ret < 0) {
            errorSet(result);
        }
        break;
    case 4:
        if (saveFileCheck(&sub, &slotw[slot]) == 1) {
            step++;
        }
        break;
    case 5:
        if (systemFileCheck(&sub, &slotw[slot]) == 1) {
            step++;
        }
        break;
    case 6:
        ret = freeCheck(&sub, &slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            step++;
        } else if (ret < 0) {
            if (type == 2) {
                errorSet(result);
            } else {
                step++;
            }
        }
        break;
    case 7:
        if (slot != 2) {
            ret = CARDGetSerialNo(slot, &slotw[slot].serial);
            if (ret == -1) {
                break;
            }
            if (ret == 0) {
                if (pG->x8 & 0x80) {
                    step++;
                } else {
                                        mode = 2;
                    step = 0;
                    sub = 0;
                    sub2 = 0;
                }
            } else {
                errorSet(ret);
            }
        } else {
            if (pG->x8 & 0x80) {
                mode = 8;
            } else {
                mode = 2;
            }
            step = 0;
            sub = 0;
            sub2 = 0;
        }
        break;
    case 8:
        s = &slotw[slot];
        if (s->flags & 0x200) {
                        mode = 8;
            step = 0;
            sub = 0;
            sub2 = 0;
/*/BF*/
        } else {
            int blocks = FREE_BLOCKS(*s);
            if (blocks == 0 || s->freeFiles == 0) {
                errorSet(-0x20A);
            } else {
                                mode = 8;
                step = 0;
                sub = 0;
                sub2 = 0;
            }
        }
        break;
    }
}

void cCard::dataSelect()
{
    int i;
    CardSlot* s;
    int sel;
    SaveInfo* info;

    if (slot != 2) {
        if (CARDProbeEx(slot, 0, 0) == -3) {
            errorSet(-3);
            return;
        }
        eprintf(32, 300, 0, 0, "Card    : %2dMbit", slotw[slot].memSize);
        eprintf(32, 320, 0, 0, "Sector  : 0x%x", slotw[slot].sectorSize);
        eprintf(32, 340, 0, 0, "F size  : %d", slotw[slot].freeBytes);
        eprintf(32, 360, 0, 0, "F entry : %d", slotw[slot].freeFiles);
        eprintf(32, 380, 0, 0, "F block : %d", FREE_BLOCKS(slotw[slot]));
    }
    if (slotw[slot].fileFlag[fileNo] & 1) {
        info = (SaveInfo*) pInfo[fileNo];
        if (info->magic != 0x116) {
            eprintf2(12, 16, 220, 380, 0, 0, "DATA IS CORRUPTED");
            slotw[slot].fileFlag[fileNo] |= 2;
        } else {
            eprintf2(12, 16, 220, 380, 0, 0, "R%03X", info->room);
            eprintf2(12, 16, 220, 400, 0, 0, "%02d/%02d/%02d %02d:%02d:%02d", info->time.year % 100, info->time.mon + 1,
                     info->time.mday, info->time.hour, info->time.min, info->time.sec);
        }
    } else {
        eprintf2(12, 16, 220, 380, 0, 0, "NO DATA");
    }

    switch (step) {
    case 0: {
        if (pG->card_serial == slotw[slot].serial) {
            fileNo = pG->save_no;
        } else {
            int found = 0;
            u32 n;
            for (n = 0; n < 20; n++) {
                if (slotw[slot].fileFlag[n] & 1) {
                    fileNo = n;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                fileNo = 0;
            } else {
                n = fileNo + 1;
                while (n <= 19) {
                    if (slotw[slot].fileFlag[n] & 1) {
                        info = (SaveInfo*) pInfo[fileNo];
                        if (OSCalendarTimeToTicks(&((SaveInfo*) pInfo[n])->time) > OSCalendarTimeToTicks(&info->time)) {
                            fileNo = n;
                            n = fileNo + 1;
                            continue;
                        }
                    }
                    n++;
                }
            }
        }
        step++;
        flag |= 2;
        setMsgWindow(1, 0);
        deleteAllMes();
    }
        // fallthrough
    case 1:
        if (g_id->action != 0) {
            break;
        }
        if (Key.rep & (KEY_UP | KEY_DOWN)) {
            if (Key.rep & KEY_UP) {
                fileNo--;
                g_id->setAction(1);
                SndCall(0, 0x2F, 0, 0, 0, 0);
            } else if (Key.rep & KEY_DOWN) {
                fileNo++;
                g_id->setAction(2);
                SndCall(0, 0x2C, 0, 0, 0, 0);
            }
            fileNo = fileNo < 0 ? 19 : (fileNo > 19 ? 0 : fileNo);
            break;
        } else if (Key.trg & KEY_B) {
            /*BF:ds1b*/
            mode = 5;
            step = 2;
            sub = 0;
            sub2 = 0;
/*/BF*/
        } else if (Key.trg & KEY_A) {
            /*BF:ds1a*/
            step++;
            sub = 0;
            sub2 = 0;
/*/BF*/
        }
        break;
    case 2: {
        int blocks;
        s = &slotw[slot];
        blocks = FREE_BLOCKS(*s);
        if (s->fileFlag[fileNo] != 0) {
            if (type == 1) {
                step = 3;
                if (slot == 2) {
                    break;
                }
                if (s->flags & 0x200) {
                    break;
                }
                if (blocks != 0 && s->freeFiles != 0) {
                    break;
                }
                errorSet(-0x20A);
            } else {
                if (s->fileFlag[fileNo] & 2) {
                    errorSet(-0x202);
                } else {
                    step = 5;
                }
            }
        } else {
            if (type == 1) {
                if (slot == 2) {
                    step = 4;
                    break;
                }
                step = 4;
                if (!(s->flags & 0x200)) {
                    if ((u32) blocks < saveBlocks + sysBlocks || s->freeFiles < 2) {
                        errorSet(-0x201);
                    }
                } else {
                    if ((u32) blocks < saveBlocks || s->freeFiles < 1) {
                        errorSet(-0x201);
                    }
                }
            } else {
                step = 1;
            }
        }
        break;
    }
    case 3:
        setMsgWindow(1, 1);
        cardMesSet(0x13, 0, 0);
        cMes.mes[0].cursor = 1;
        step = 6;
        break;
    case 4:
        setMsgWindow(1, 1);
        cardMesSet(0x10, 0, 0);
        cMes.mes[0].cursor = 1;
        step = 6;
        break;
    case 5:
        setMsgWindow(1, 1);
        cardMesSet(0x12, 0, 0);
        cMes.mes[0].cursor = 1;
        step = 6;
        break;
    case 6:
        if (Key.trg & KEY_B) {
            sel = 2;
        } else {
            sel = cMes.mes[0].result;
        }
        switch (sel) {
        case 1:
            if (type == 0) {
                CoreSeCall(0x38, 0, 0, 0, 0);
            } else {
                CoreSeCall(4, 0, 0, 0, 0);
            }
            /*BF:ds6*/
            mode = 3;
            step = 0;
            sub = 0;
            sub2 = 0;
/*/BF*/
            pG->save_no = fileNo;
            pG->card_serial = slotw[slot].serial;
            deleteAllMes();
            break;
        case 2:
            CoreSeCall(5, 0, 0, 0, 0);
            step = 1;
            deleteAllMes();
            setMsgWindow(1, 0);
            break;
        }
        break;
    }
    fileNo = fileNo < 0 ? 0 : (fileNo > 19 ? 19 : fileNo);
}

void cCard::loadMain()
{
    u8* buf = pSaveBuf;
    char* name;
    int ret;

    if (slot == 2) {
        sprintf(fileName, "d:\\bio4/room/savedata%02d.dat", fileNo);
    } else {
        sprintf(fileName, "bh4_data%02d", fileNo);
    }
    name = fileName;
    if (slotw[slot].fileFlag[fileNo] & 4) {
        errorSet(-0x202);
        return;
    }
    switch (step) {
    case 0:
        setMsgWindow(1, 1);
        BitOn(pG->flags_54, 0x200);
        cardMesSet(0xD, 0, 0);
        if (slot == 2) {
            step = 2;
            break;
        }
        retry = 0;
        step++;
        // fallthrough
    case 1:
        ret = fileOpen(&slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            step++;
        } else if (ret < 0) {
            errorSet(-0x204);
        }
        break;
    case 2:
        if (slot == 2) {
            int dbg = 1;
            if (!(pG->flags_54 & 0x20000)) {
                dbg = 0;
            }
            if (DBIsDebuggerPresent()) {
                BitOn(pG->flags_54, 0x20000);
            }
            HDRead(name, pSaveBuf);
            if (dbg == 0) {
                BitOff(pG->flags_54, 0x20000);
            }
            step++;
        } else {
            ret = fileRead(&sub, pSaveBuf, saveBlocks << 13, 0, &slotw[slot]);
            if (ret == 0) {
            } else if (ret > 0) {
                step++;
            } else {
                errorSet(-0x204);
            }
        }
        break;
    case 3:
        if (CRCVerify(pSaveBuf, SAVE_CRC, *(u32*) (pSaveBuf + SAVE_CRC)) == 0) {
            if (retry == 3) {
                slotw[slot].fileFlag[fileNo] |= 2;
                errorSet(-0x202);
            } else {
                step = 2;
                retry++;
            }
        } else {
            if (slot == 2) {
                step = 5;
            } else {
                step++;
            }
        }
        break;
    case 4:
        ret = fileClose(&slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            step++;
        } else if (ret < 0) {
            errorSet(-0x204);
        }
        break;
    case 5:
        cardMesSet(0xC, 0, 0);
        memcpy(SD->p8, buf + SAVE_GAME, SAVE_GAME_SIZE);
        memcpy(SD->pC, buf + SAVE_DATA2, SAVE_DATA2_SIZE);
        memcpy(SD->p10, buf + SAVE_ROOM, RoomData.num * 0xD8 + 0x10);
        memcpy(SD->p14, buf + SAVE_SSCRN, SscrnDataSize());
        memcpy(SD->p18, buf + SAVE_MERCHANT, MerchantDataSize());
        GameSave.load(pSaveData);
        GameSaveSave(&GameSave, pSaveData, pG->game_mode);
        BitOn(pG->x8, 4);
        BitOff(pG->flags_54, 0x200);
        setMsgWindow(1, 0);
                mode++;
        step = 0;
        sub = 0;
        sub2 = 0;
        break;
    }
}

void cCard::makeSaveData()
{
    int i;
    TEXPalette* tpl = (TEXPalette*) (pSubData->ofs[0] + (u32) pSubData);
    u8* buf = pSaveBuf;
    TEXDescriptor* d;
    u8* src;
    OSCalendarTime cal;

    OSTicksToCalendarTime(OSGetTime(), &cal);
    for (i = 0; i < ICON_NUM; i++) {
        d = TEXGet(tpl, i + 2);
        src = (u8*) d->textureHeader->data;
        memcpy(buf + SAVE_ICON + (i << 10), src, 0x400);
    }
    src = (u8*) d->CLUTHeader->data;
    memcpy(buf + SAVE_CLUT, src, 0x200);
    if (pSys->region == 0) {
        sprintf((char*) buf, "biohazard4 FILE%02d", fileNo + 1);
        sprintf((char*) buf + SAVE_COMMENT2, "%04d/%02d/%02d %02d:%02d:%02d \x8dX\x90V", cal.year, cal.mon + 1, cal.mday,
                cal.hour, cal.min, cal.sec);
        i = 0;
    } else {
        sprintf((char*) buf, "resident evil 4");
        sprintf((char*) buf + SAVE_COMMENT2, "FILE %02d", fileNo + 1);
        i = 1;
    }
    d = TEXGet(tpl, i);
    src = (u8*) d->textureHeader->data;
    memcpy(buf + SAVE_BANNER, src, 0x1800);
    U16Inc(pG->save_cnt);
    if (!(pG->x8 & 0x60)) {
        SetGameTime();
    }
    if (pG->x8 & 0x20) {
        *(u32*) (buf + SAVE_HDR_MODE) = 2;
    } else if (pG->x8 & 0x40) {
        *(u32*) (buf + SAVE_HDR_MODE) = 3;
    } else {
        *(u32*) (buf + SAVE_HDR_MODE) = 1;
    }
    GameSaveSave(&GameSave, pSaveData, *(u32*) (buf + SAVE_HDR_MODE));
    memcpy(buf + SAVE_GAME, SD->p8, SAVE_GAME_SIZE);
    memcpy(buf + SAVE_DATA2, SD->pC, SAVE_DATA2_SIZE);
    memcpy(buf + SAVE_ROOM, SD->p10, RoomData.num * 0xD8 + 0x10);
    memcpy(buf + SAVE_SSCRN, SD->p14, SscrnDataSize());
    memcpy(buf + SAVE_MERCHANT, SD->p18, MerchantDataSize());
    *(u32*) (buf + SAVE_HDR_MAGIC) = 0x116;
    buf[SAVE_HDR_X3D] = buf[0x5408];
    *(OSCalendarTime*) (buf + SAVE_HDR_TIME) = cal;
    *(u32*) (buf + SAVE_HDR) = CRCCalc(buf + SAVE_HDR_MAGIC, 0x1FC);
    *(u32*) (buf + SAVE_CRC) = CRCCalc(buf, SAVE_CRC);
    DCFlushRange(pSaveBuf, SAVE_SIZE);
}

void cCard::makeSystemSaveData()
{
    int i;
    TEXPalette* tpl = (TEXPalette*) (pSubData->ofs[0] + (u32) pSubData);
    u8* buf = pSysBuf;
    TEXDescriptor* d;
    u8* src;
    OSCalendarTime cal;

    OSTicksToCalendarTime(OSGetTime(), &cal);
    for (i = 0; i < ICON_NUM; i++) {
        d = TEXGet(tpl, i + 3);
        src = (u8*) d->textureHeader->data;
        memcpy(buf + SAVE_ICON + (i << 10), src, 0x400);
    }
    src = (u8*) d->CLUTHeader->data;
    memcpy(buf + SAVE_CLUT, src, 0x200);
    if (pSys->region == 0) {
        sprintf((char*) buf, "biohazard4 \x83V\x83X\x83" "e\x83\x80\x83t\x83@\x83" "C\x83\x8b");
        sprintf((char*) buf + SAVE_COMMENT2, "%04d/%02d/%02d %02d:%02d:%02d \x8dX\x90V", cal.year, cal.mon + 1, cal.mday,
                cal.hour, cal.min, cal.sec);
        i = 0;
    } else {
        sprintf((char*) buf, "resident evil 4");
        sprintf((char*) buf + SAVE_COMMENT2, "Systemfile");
        i = 1;
    }
    d = TEXGet(tpl, i);
    src = (u8*) d->textureHeader->data;
    memcpy(buf + SAVE_BANNER, src, 0x1800);
    *(SystemWork*) (buf + SYS_WORK) = *pSys;
    *(u32*) (buf + SYS_WORK + 4) |= sysFlags;
    *(u32*) (buf + SYS_CRC) = CRCCalc(buf, SYS_CRC);
    DCFlushRange(pSaveBuf, SYS_SIZE);
}

void cCard::saveMain()
{
    void (cCard::*makeFunc)();
    char* name;
    u8* buf;
    int blocks;
    int nextMode;
    int ret;

    if (mode == 3) {
        isSystem = 0;
    } else {
        isSystem = 1;
    }
    if (isSystem == 0) {
        if (slot == 2) {
            name = fileName;
            sprintf(name, "d:\\bio4/room/savedata%02d.dat", fileNo);
        } else {
            name = fileName;
            sprintf(name, "bh4_data%02d", fileNo);
        }
        blocks = saveBlocks;
        buf = pSaveBuf;
        makeFunc = &cCard::makeSaveData;
        nextMode = 8;
    } else {
        if (slot == 2) {
            name = fileName;
            sprintf(name, "d:\\bio4/room/sysdata.dat");
        } else {
            name = fileName;
            sprintf(name, "bh4_system");
        }
        blocks = sysBlocks;
        buf = pSysBuf;
        makeFunc = &cCard::makeSystemSaveData;
        nextMode = 4;
    }

    switch (step) {
    case 0:
        setMsgWindow(1, 1);
        BitOn(pG->flags_54, 0x200);
        if (isSystem == 0) {
            cardMesSet(0xB, 0, 0);
            step = 2;
            if (slot == 2) {
                step = 5;
            }
        } else {
            if (pG->x8 & 0x80) {
                cardMesSet(0x27, 0, 0);
            }
            step++;
        }
        sub = 0;
        sub2 = 0;
        break;
    case 1:
        ret = sysfileRead(&sub, &sub2, 0);
        switch (ret) {
        case 0:
            break;
        case 1:
            sysFlags = *(u32*) (pSysBuf + SYS_WORK + 4);
            // fallthrough
        case -1:
            step++;
            if (slot == 2) {
                step = 5;
            }
            sub = 0;
            sub2 = 0;
            break;
        }
        break;
    case 2:
        ret = fileOpen(&slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            if (result == 0) {
                step = 4;
            } else {
                step++;
            }
            sub = 0;
            sub2 = 0;
        } else if (ret < 0) {
            errorSet(-0x203);
        }
        break;
    case 3:
        ret = fileCreate(&sub, blocks, &slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            sub = 0;
            sub2 = 0;
            step++;
        } else if (ret < 0) {
            if (result == -5) {
                errorSet(-5);
            } else {
                errorSet(-0x203);
            }
        }
        break;
    case 4:
        ret = CARDGetStatus(slot, slotw[slot].fileInfo.fileNo, &slotw[slot].stat);
        if (ret == -1) {
            break;
        }
        if (ret == 0) {
            step++;
        } else {
            errorSet(-0x203);
        }
        break;
    case 5:
        (this->*makeFunc)();
        if (isSystem == 0) {
            slotw[slot].fileFlag[fileNo] = 1;
            memcpy(pInfo[fileNo], pSaveBuf + 0x2000, 0x200);
            g_id->setAction(4);
            flag &= ~1;
        }
        step++;
        // fallthrough
    case 6:
        if (slot == 2) {
            int dbg = 1;
            if (!(pG->flags_54 & 0x20000)) {
                dbg = 0;
            }
            if (DBIsDebuggerPresent()) {
                BitOn(pG->flags_54, 0x20000);
            }
            HDWrite_only(name, buf, blocks << 13);
            if (dbg == 0) {
                BitOff(pG->flags_54, 0x20000);
            }
            if (mode == 3) {
                // The zero stored into step shares `no`'s pseudo with the shift count: its `li` then
                // trails the rotlw (sched1 anti-dependence), takes r0, and jump2 cross-jumps this
                // arm's `stb step` (and case 2's `step = 4`, the `step = 0xA` arm) into case 1's copy.
                // A fresh zero is hoisted above the extsb, lands in r10 and keeps its own stb.
                int no = fileNo;
                isDbgInfoCached &= ~(1 << no);
                mode = nextMode;
                no = 0;
                step = no;
            } else {
                if (pG->x8 & 0x80) {
                    step = 0xB;
                    timer = 0xF;
                } else {
                    step = 0xA;
                }
            }
            sub = 0;
            sub2 = 0;
        } else {
            ret = fileWrite(&sub, buf, blocks, &slotw[slot]);
            if (ret == 0) {
            } else if (ret > 0) {
                step++;
            } else if (ret < 0) {
                if (result == -5) {
                    errorSet(-5);
                } else {
                    errorSet(-0x203);
                }
            }
        }
        break;
    case 7:
        makeCardStatus(&slotw[slot]);
        CARDSetStatusAsync(slot, slotw[slot].fileInfo.fileNo, &slotw[slot].stat, 0);
        step++;
        // fallthrough
    case 8:
        switch (CARDGetResultCode(slot)) {
        case -1:
            break;
        case 0:
            step++;
            break;
        case -5:
            errorSet(-5);
            break;
        default:
            errorSet(-0x203);
            break;
        }
        break;
    case 9:
        ret = fileClose(&slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            if (mode == 3) {
                mode = nextMode;
                step = 0;
                sub = 0;
                sub2 = 0;
            } else {
                if (pG->x8 & 0x80) {
                    step = 0xB;
                    timer = 0xF;
                } else {
                    step++;
                }
                sub = 0;
                sub2 = 0;
            }
        } else if (ret < 0) {
            errorSet(-0x203);
        }
        break;
    case 10:
        BitOn(flag, 1);
        BitOff(pG->flags_54, 0x200);
        cardMesSet(0xC, 0, 0);
        if (Key.trg & (KEY_START | KEY_Z)) {
            deleteAllMes();
            setMsgWindow(1, 0);
            mode = nextMode;
            step = 0;
            sub = 0;
            sub2 = 0;
        }
        break;
    case 11:
        cardMesSet(0x28, 0, 0);
        if (timer == 0) {
            deleteAllMes();
            setMsgWindow(1, 0);
            BitOff(pG->flags_54, 0x200);
            mode = 4;
            step = 0;
            sub = 0;
            sub2 = 0;
        } else {
            timer--;
        }
        break;
    }
}

void cCard::exit()
{
    u32 i;
    u32 c0;
    u32 c1;

    switch (step) {
    case 0:
        if (unmount(0) == 1) {
            step++;
        }
        break;
    case 1:
        deleteAllMes();
        if (type == 2) {
            BitOn(pG->x8, 0x80000000);
            systemVISetBlack(1);
            workDestroy();
            exitFlag = 1;
        } else {
            if (!(pG->x8 & 0x80)) {
                c0 = 0;
                c1 = 0xFF;
                FadeSet(0, (GXColor*) &c0, (GXColor*) &c1, 10, 0, 0);
            }
            step++;
        }
        break;
    case 2:
        if (Fade[0].flags & 1) {
            break;
        }
        BitSet((u32&) dispFlag, 0);
        if (!(pG->x8 & 0x80)) {
            g_id->quit();
        }
        workDestroy();
        pG->flags_170 = saveFlags170;
        TaskSignal(0);
        if (!(pG->x8 & 0x80)) {
            pG->flags_58 = saveFlags58;
            SndStrReq(bgmStrId, 4, 200, 0);
            ScreenReSizeI(scrWidth, 448);
            if (type != 0 || !(pG->x8 & 4)) {
                if (!(pG->x8 & 0x10)) {
                    c0 = 0xFF;
                    c1 = 0;
                    FadeSet(0x80000000, (GXColor*) &c0, (GXColor*) &c1, 10, 0, 0);
                }
                for (i = 0; i < 4; i++) {
                    if (str[i].id != 0 && SndStrStatusCk(str[i].id, 0x10) != 0) {
                        SndStrReq(str[i].id, 4, 100, str[i].vol);
                    }
                }
                SndSePauseAll(0);
                SndRoomBgmMuteAll(0, -1);
            }
        }
        BitOff(pG->x8, 0x7FFFFFF8);
        MesData.ptr[0] = (u8*) (pG->pArc->ofs_28 + (u32) pG->pArc);
        exitFlag = 1;
        break;
    }
}

void cCard::workDestroy()
{
    if (slotw[0].workArea) {
        Mem_free(slotw[0].workArea);
    }
    if (pSaveBuf) {
        Mem_free(pSaveBuf);
    }
    if (pSysBuf) {
        Mem_free(pSysBuf);
    }
    if (pIdData) {
        Mem_free(pIdData);
    }
    if (pInfoBuf) {
        Mem_free(pInfoBuf);
    }
    swap.SwapIn();
}

void cCard::format()
{
    int noCard = 0;
    int ret;
    int sel;

    ret = CARDProbeEx(slot, 0, 0);
    switch (step) {
    case 0:
        ret = mount(&sub, &slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            if (type == 2) {
                step = 3;
            } else {
                step++;
            }
        } else if (ret < 0) {
            if (type == 2) {
                mode = 0;
                step = 0;
                sub = 0;
                sub2 = 0;
            } else {
                errorSet(result);
            }
        }
        break;
    case 1:
        setMsgWindow(0, 1);
        cardMesSet(5, 0, 0x800000);
        cMes.mes[0].cursor = 1;
        step++;
        // fallthrough
    case 2:
        if (ret == -3) {
            noCard = 1;
            break;
        }
        sel = cMes.mes[0].result;
        switch (sel) {
        case 1:
            CoreSeCall(4, 0, 0, 0, 0);
            step++;
            break;
        case 2:
            CoreSeCall(5, 0, 0, 0, 0);
            mode = 0;
            step = 0;
            sub = 0;
            sub2 = 0;
            break;
        }
        break;
    case 3:
        cardMesSet(7, 0, 0x800000);
        cMes.mes[0].cursor = 1;
        step++;
        // fallthrough
    case 4:
        if (ret == -3) {
            noCard = 1;
            break;
        }
        sel = cMes.mes[0].result;
        switch (sel) {
        case 1:
            CoreSeCall(4, 0, 0, 0, 0);
            timer = 0;
            step++;
            CARDFormatAsync(slot, 0);
            break;
        case 2:
            CoreSeCall(5, 0, 0, 0, 0);
            errCode = -0x209;
            mode = 5;
            step = sel;
            sub = 0;
            sub2 = 0;
            deleteAllMes();
            break;
        }
        break;
    case 5:
        cardMesSet(9, 0, 0);
        ret = CARDGetResultCode(slot);
        switch (ret) {
        case 0:
            formatted = 1;
            step++;
            break;
        case -3:
            if (type == 2) {
                mode = 0;
                step = 0;
                sub = 0;
                sub2 = 0;
            } else {
                errorSet(-3);
            }
            break;
        case -0x80:
        case -5:
            errorSet(ret);
            break;
        case -1:
            break;
        }
        break;
    case 6:
        cardMesSet(0xA, 0, 0);
        if (ret == -3) {
            noCard = 1;
            break;
        }
        if (Key.trg & KEY_A) {
            deleteAllMes();
            if (type == 2) {
                mode = 0;
            } else {
                mode = 0;
                setMsgWindow(0, 0);
            }
            step = 0;
            sub = 0;
            sub2 = 0;
        }
        break;
    }
    if (noCard == 1) {
        if (type == 2) {
            mode = 0;
            step = 0;
            sub = 0;
            sub2 = 0;
        } else {
            errorSet(ret);
        }
    }
}

void cCard::fileDelete()
{
    int ret;
    int sel;

    switch (step) {
    case 0:
        if (type == 2) {
            cardMesSet(0x1D, 0, 0x800000);
            sprintf(fileName, "bh4_system");
        } else {
            setMsgWindow(0, 1);
            cardMesSet(0x16, 0, 0x800000);
            sprintf(fileName, "bh4_data%02d", fileNo);
        }
        cMes.mes[0].cursor = 1;
        step++;
        // fallthrough
    case 1:
        if (CARDProbeEx(slot, 0, 0) == -3) {
            if (type == 2) {
                mode = 0;
                step = 0;
                sub = 0;
                sub2 = 0;
            } else {
                errorSet(-3);
            }
            break;
        }
        sel = cMes.mes[0].result;
        switch (sel) {
        case 1:
            CoreSeCall(4, 0, 0, 0, 0);
            timer = 0;
            step++;
            CARDDeleteAsync(slot, fileName, 0);
            break;
        case 2:
            CoreSeCall(5, 0, 0, 0, 0);
            sub = 0;
            errCode = -0x208;
            mode = 5;
            step = sel;
            sub2 = 0;
            break;
        }
        break;
    case 2:
        cardMesSet(0x1E, 0, 0);
        ret = CARDGetResultCode(slot);
        switch (ret) {
        case -1:
            break;
        case 0:
            step++;
            break;
        case -3:
            if (type == 2) {
                mode = 0;
                step = 0;
                sub = 0;
                sub2 = 0;
            } else {
                errorSet(-3);
            }
            break;
        default:
            errorSet(ret);
            break;
        }
        break;
    case 3:
        if (type != 2) {
            slotw[slot].fileFlag[fileNo] &= ~1;
        }
        cardMesSet(0x1F, 0, 0);
        if (Key.trg & KEY_A) {
            deleteAllMes();
            if (type == 2) {
                mode = 0;
            } else {
                mode = 0;
                setMsgWindow(0, 0);
            }
            step = 0;
            sub = 0;
            sub2 = 0;
        }
        break;
    }
}

void cCard::errorDisp()
{
    static int cardcheck;
    u32 attr = 0x800000;
    int mesNo = 0;
    int probe = 0;

    if (slot != 2) {
        probe = CARDProbeEx(slot, 0, 0);
    }
    BitOff(pG->flags_54, 0x200);
    switch (step) {
    case 0:
        CoreSeCall(0x2A, 0, 0, 0, 0);
        cardcheck = 1;
        switch (IRef(errCode)) {
        case -3:
            if (type == 2) {
                mesNo = 0x18;
            } else {
                setMsgWindow(0, 1);
                mesNo = 0;
            }
            break;
        case -2:
            if (type == 2) {
                mesNo = 0x1A;
            } else {
                setMsgWindow(0, 1);
                mesNo = 4;
            }
            break;
        case -0x80:
        case -5:
            if (type == 2) {
                mesNo = 0x19;
            } else {
                setMsgWindow(0, 1);
                mesNo = 3;
            }
            break;
        case -6:
            if (formatted == 1) {
                if (type == 2) {
                    mesNo = 0x19;
                } else {
                    setMsgWindow(0, 1);
                    mesNo = 3;
                }
                break;
            }
            // fallthrough
        case -0xD:
            if (type == 2) {
                mesNo = 0x1B;
            } else {
                mode = 6;
                step = 0;
                sub = 0;
                sub2 = 0;
                return;
            }
            break;
        case -0x200:
            eprintf2(10, 16, 80, 170, 0, 0, "The Memory Card in Slot %c is not supported.", slot + 'A');
            break;
        case -0x201:
            cMes.getWork()->setNumber(saveBlocks + sysBlocks, 2);
            if (type == 2) {
                mesNo = 0x17;
            } else {
                setMsgWindow(0, 1);
                mesNo = 2;
                attr = 0;
            }
            break;
        case -0x205:
            setMsgWindow(0, 1);
            attr = 0;
            mesNo = 0x29;
            cardcheck = 0;
            break;
        case -0x203:
            // `attr = 0` in each arm (as in -0x20A): the arms then share a tail, jump1 cannot hoist
            // `mesNo = 1` above the branch (jump2 does, after sched1), and `cardcheck = 0` after the join
            // gets its own zero.
            if (pG->x8 & 0x80) {
                mesNo = 0x29;
                attr = 0;
            } else {
                mesNo = 1;
                attr = 0;
            }
            cardcheck = 0;
            break;
        case -0x204:
            // mesNo first: `cardcheck = 0` then takes the switch index's zero (step == 0 here) instead of
            // mesNo's, which gives the index the refs that put it in r31 before mesNo (r29).
            mesNo = 0x14;
            cardcheck = 0;
            attr = 0;
            break;
        case -0x202:
            mode = 7;
            step = 0;
            sub = 0;
            sub2 = 0;
            return;
        case -0x206:
            mesNo = 0x25;
            cardcheck = 0;
            break;
        case -0x207:
            mesNo = 0x1C;
            break;
        case -0x20A:
            if (type == 2) {
                mesNo = 0x23;
            } else if (pG->x8 & 0x80) {
                mesNo = 0x2A;
                attr = 0;
            } else {
                mesNo = 0x22;
                attr = 0;
            }
            break;
        case -0x20B:
            mesNo = 0x24;
            break;
        }
        cardMesSet(mesNo, 0, attr);
        if (attr == 0) {
            step = 1;
        } else {
            step = 3;
            cMes.mes[0].cursor = 1;
        }
        sub = 0;
        sub2 = 0;
        break;
    case 1:
        if (Key.trg & (KEY_START | KEY_Z)) {
            step++;
        }
        break;
    case 2:
        switch (type) {
        case 0:
            setMsgWindow(0, 1);
            mesNo = 0x11;
            break;
        case 1:
            if (pG->x8 & 0x80) {
                mesNo = 0x2B;
            } else {
                setMsgWindow(0, 1);
                mesNo = 0xF;
            }
            break;
        case 2:
            mode = 3;
            step = 0;
            sub = 0;
            sub2 = 0;
            deleteAllMes();
            return;
        }
        cardMesSet(mesNo, 0, 0x800000);
        if (pG->x8 & 0x80) {
            cMes.mes[0].cursor = 0;
        } else {
            cMes.mes[0].cursor = 1;
        }
        step++;
        break;
    case 3:
        switch (cMes.mes[0].result) {
        case 1:
            if (type == 2) {
                CoreSeCall(4, 0, 0, 0, 0);
                mode = 3;
            } else if (pG->x8 & 0x80) {
                CoreSeCall(4, 0, 0, 0, 0);
                mode = 0;
            } else {
                CoreSeCall(5, 0, 0, 0, 0);
                mode = 4;
            }
            step = 0;
            sub = 0;
            sub2 = 0;
            deleteAllMes();
            break;
        case 2:
            if (type == 2) {
                CoreSeCall(5, 0, 0, 0, 0);
                mode = 0;
            } else if (pG->x8 & 0x80) {
                CoreSeCall(5, 0, 0, 0, 0);
                mode = 4;
            } else {
                CoreSeCall(4, 0, 0, 0, 0);
                mode = 0;
            }
            step = 0;
            sub = 0;
            sub2 = 0;
            deleteAllMes();
            break;
        case 3:
            CoreSeCall(4, 0, 0, 0, 0);
            deleteAllMes();
            switch (errCode) {
            case -0x201:
            case -0x20A:
            case -0x20B:
                OSResetSystem(1, 1, 1);
                break;
            case -0xD:
            case -6:
                mode = 6;
                step = 0;
                sub = 0;
                sub2 = 0;
                break;
            }
            break;
        }
        break;
    }

    if (slot != 2 && cardcheck == 1 && probe != -1) {
        switch (probe) {
        case -3:
            if (!(slotw[slot].flags & 2)) {
                deleteAllMes();
                if (type == 2) {
                    mode = 0;
                } else {
                    mode = 1;
                }
                step = 0;
                sub = 0;
                sub2 = 0;
                formatted = 0;
            }
            break;
        case -0x80:
        case -2:
        case 0:
            if (slotw[slot].flags & 2) {
                deleteAllMes();
                if (type == 2) {
                    mode = 0;
                } else {
                    mode = 1;
                }
                step = 0;
                sub = 0;
                sub2 = 0;
                formatted = 0;
            }
            break;
        }
    }
    if (type != 2 && mode != 5) {
        setMsgWindow(0, 0);
    }
    eprintf(470, 10, 0, 0, "%d", errCode);
}

void cCard::errorSet(int code)
{
    errCode = code;
    mode = 5;
    step = 0;
}

int cCard::initialize(int type)
{
    u32 c0;
    u32 c1;
    u8 heap;
    u32 addr;

    this->type = type;
    if (pSys->region == 0) {
        idpath[12] = fileext[0];
    } else if (pSys->region == 1) {
        idpath[12] = fileext[1];
    } else if (isEurope(pSys->region)) {
        idpath[12] = fileext[pSys->language];
    } else {
        idpath[12] = fileext[7];
    }
    useMemSize = getUseMemSize();
    if (useMemSize == 0) {
        return 0;
    }
    if (type == 2) {
        if (initSub() == 0) {
            return 0;
        }
    } else {
        if (type == 0) {
            BitOff(pG->x8, 4);
        }
        heap = MemGetCurrentHeap();
        TaskSuspend(0);
        if (pG->x8 & 0x80) {
            addr = (u32) pG->pOptionData;
        } else {
            addr = MemGetHeapStartAddr(heap);
            if (!(pG->x8 & 8)) {
                c0 = 0;
                c1 = 0xFF;
                FadeSet(0, (GXColor*) &c0, (GXColor*) &c1, 10, 0, 0);
            }
            while (Fade[0].flags & 1) {
                TaskSleep(1);
            }
            BitSet(saveFlags58, pG->flags_58);
            BitSet(pG->flags_58, 0xFFFFFFFF);
            BitOff(pG->flags_58, 0x800);
            BitOff(pG->flags_58, 0x2000);
        }
        if (swap.SwapOut(addr, useMemSize, 0) == 0) {
            return 0;
        }
        BitSet(saveFlags170, pG->flags_170);
        BitSet(pG->flags_170, 0xFFFFFFFF);
        BitOff(pG->flags_170, 0x80000000);
        BitOff(pG->flags_170, 0x40);
        if (initSub() == 0) {
            return 0;
        }
        if (!(pG->x8 & 0x80)) {
            SndPlayWork* s;
            int i;
            g_id->init(this->type, (CardArc*) pIdData);
            scrWidth = (int) Screen.width;
            ScreenReSize(640, 448);
            c0 = 0xFF;
            c1 = 0;
            FadeSet(0x80000000, (GXColor*) &c0, (GXColor*) &c1, 10, 0, 0);
            FadeKill(2);
            FadeKill(1);
            s = Snd.str_work;
            i = 0;
            do {
                if ((*(u32*) s & 0xFFFF0000) == 0x01000000) {
                    str[i].id = s->id;
                    str[i].vol = s->vol;
                    SndStrReq(s->id, 4, 100, 1);
                } else {
                    str[i].id = 0;
                }
                i++;
                s++;
            } while (s <= &Snd.str_work[3]);
            SndRoomBgmMuteAll(1, -1);
            SndSeAbsPause();
        }
    }
    dispFlag = 1;
    CRCInit();
    slotw[0].chan = 0;
    slotw[1].chan = 1;
    slotw[2].chan = 2;
    return 1;
}

int cCard::initSub()
{
    if (workAlloc() == 0) {
        return 0;
    }
    pSubData = (CardArc*) SndMem.sub_adr;
    calcTplAddr((TEXPalette*) (pSubData->ofs[0] + (u32) pSubData));
    if (!(pG->x8 & 0x80)) {
        void* addr;
        int req;
#line 2170 "D:/Bio4/Prog/card.cpp"
        req = DvdReadN(idpath, 0, 0, 0, 0, 4, __FILE__, __LINE__);
        while (Dvd.ReadCheck(req, 0, 0, &addr) != 1) {
            TaskSleep(1);
        }
        pIdData = addr;
    }
    MesData.ptr[0] = (u8*) (pSubData->ofs[1] + (u32) pSubData);
    return 1;
}

int cCard::workAlloc()
{
    int i;

#line 2190 "D:/Bio4/Prog/card.cpp"
    slotw[0].workArea = MEM_ALLOC(0xA000, 1, 13);
    if (slotw[0].workArea == 0) {
        OSReport("CARD workarea alloc error!!\n");
        return 0;
    }
#line 2197 "D:/Bio4/Prog/card.cpp"
    pSysBuf = (u8*) MEM_CALLOC((sysBufSize + 0x1FFF) & ~0x1FFF, 1, 13);
    if (pSysBuf == 0) {
        OSReport("System Savedata workarea alloc error!!\n");
        return 0;
    }
    if (!(pG->x8 & 0x80)) {
#line 2205 "D:/Bio4/Prog/card.cpp"
        pSaveBuf = (u8*) MEM_CALLOC((saveBufSize + 0x1FFF) & ~0x1FFF, 1, 13);
        if (pSaveBuf == 0) {
            OSReport("Savedata workarea alloc error!!\n");
            return 0;
        }
#line 2211 "D:/Bio4/Prog/card.cpp"
        pInfoBuf = (u8*) MEM_CALLOC(0x2800, 1, 13);
        if (pInfoBuf == 0) {
            OSReport("Savedata Infomation workarea alloc error!!\n");
            return 0;
        }
        for (i = 0; i < 20; i++) {
            pInfo[i] = pInfoBuf + i * 0x200;
        }
    }
    return 1;
}

u32 cCard::getUseMemSize()
{
    u32 size;

    sysBufSize = SYS_SIZE;
    sysBlocks = 1;
    size = 0;
    if (!(pGS->x8 & 0x80)) {
        saveBufSize = SAVE_SIZE;
        saveBlocks = 8;
        if (Dvd.FileExistCheck(idpath, &size) < 0) {
            return 0;
        }
        size += 0x10000;
        size += (saveBufSize + 0x1FFF) & ~0x1FFF;
        size += 0x6000;
    }
    size += 0x10000;
    size += (sysBufSize + 0x1FFF) & ~0x1FFF;
    size -= 0x4000;
    return size;
}

int cCard::fileCreate(u8* sub, int blocks, CardSlot* s)
{
    int ret = 0;

    switch (*sub) {
    case 0:
        CARDCreateAsync(s->chan, fileName, blocks << 13, &s->fileInfo, 0);
        (*sub)++;
        // fallthrough
    case 1:
        result = CARDGetResultCode(s->chan);
        if (result == -1) {
            break;
        }
        if (result == 0) {
            ret = 1;
        } else {
            ret = -1;
        }
        break;
    }
    if (ret != 0) {
        *sub = 0;
    }
    return ret;
}

// commentAddr stored before iconAddr (the zero's qty is born first and takes r0); the tail mask
// through a two-use temp (see below).
void cCard::makeCardStatus(CardSlot* s)
{
    u32 fmt;
    u32 spd;
    u32 fmt2;
    u32 spd2;
    int i;

    s->stat.bannerFormat = (u8) ((s->stat.bannerFormat & ~3) | 2);
    s->stat.commentAddr = 0;
    s->stat.iconAddr = 0x40;
    fmt = s->stat.iconFormat;
    spd = s->stat.iconSpeed;
    for (i = 0; i < ICON_NUM; i++) {
        fmt2 = (fmt & ~(3 << (2 * i))) | (1 << (2 * i));
        spd2 = (spd & ~(3 << (2 * i))) | (3 << (2 * i));
        fmt = fmt2;
        spd = spd2;
    }
    s->stat.iconFormat = fmt2;
    {
        // The mask must stay a 32-bit `rlwinm` (a two-use temp keeps combine from folding it into
        // the u16 store as `andi. 0xfff3`); spd2 keeps 5 refs so spd (r11) is coloured before it.
        u32 t = spd2 & ~(3 << (2 * ICON_NUM));
        s->stat.iconSpeed = t;
        spd2 = t;  // COMPILER-DIFF: dead statement (second use of t)
    }
    s->stat.bannerFormat |= 4;
    DCFlushRange(&s->stat, sizeof(CardStat));
}

void cCard::firstCheck00()
{
    int ret;

    switch (step) {
    case 0:
        slot = 0;
        step++;
        // fallthrough
    case 1:
        if (unmount(slot) == 1) {
            step++;
        }
        break;
    case 2:
        ret = existCheck(slot, &slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            step++;
        } else if (ret < 0) {
            step = 2;
            slot++;
        }
        break;
    case 3:
        ret = mount(&sub, &slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            step++;
        } else if (ret < 0) {
            step = 2;
            slot++;
        }
        break;
    case 4:
        ret = verifyCheck(&sub, &slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            step++;
        } else if (ret < 0) {
            step = 2;
            slot++;
        }
        break;
    case 5:
        if (systemFileCheck(&sub, &slotw[slot]) == 1) {
            step++;
        }
        break;
    case 6:
        if (saveFileCheck(&sub, &slotw[slot]) == 1) {
            step++;
        }
        break;
    case 7:
        if (freeCheck(&sub, &slotw[slot]) != 0) {
            step = 2;
            slot++;
        }
        break;
    }
    if (slot == 1) {
        slot = 0;
        if (slotw[0].flags & 0x200) {
            mode++;
        } else if (pG->dev_mode == 1) {
            slot = 2;
            mode++;
        } else {
            mode = 2;
        }
        step = 0;
        sub = 0;
        systemVISetBlack(0);
    }
}

void cCard::firstCheck10()
{
    u8* buf = pSysBuf;
    int ret;

    sprintf(fileName, "%s", "bh4_system");
    switch (step) {
    case 0:
        timer = 30;
        if (slot != 2) {
            cardMesSet(8, 0, 0);
        }
        retry = 0;
        step++;
        break;
    case 1:
        ret = sysfileRead(&sub, &sub2, 1);
        if (ret == 1) {
            step++;
        } else if (ret == -1) {
            step = 3;
        }
        break;
    case 2:
        *pSys = *(SystemWork*) (buf + SYS_WORK);
        BitOn(pGS->x8, 1);
        SndSetOutputMode(pSys->sound_mode, 1);
        step++;
        break;
    case 3:
        if (timer == 0) {
            deleteAllMes();
            if (slot == 2) {
                mode = 3;
            } else {
                mode++;
            }
            step = 0;
            sub = 0;
            sub2 = 0;
            if (pG->dev_mode == 1) {
                pSys->language = 1;
            }
        }
        break;
    }
    if (timer != 0) {
        timer--;
    }
}

void cCard::firstCheck20()
{
    u32 f = slotw[0].flags;

    if (f & 2) {
        errorSet(-3);
    } else if (f & 4) {
        if (f & 0x400) {
            errorSet(-0x20A);
        } else if (f & 0x800) {
            errorSet(-0x20B);
        } else {
            errorSet(-0x201);
        }
    } else if (f & 0x40) {
        errorSet(-5);
    } else if (f & 0x10) {
        errorSet(-6);
    } else if (bitChk(f, 0x20) || bitChk(f, 0x80)) {
        errorSet(-2);
    } else if (!(f & 0x200)) {
        mode = 8;
        step = 0;
        sub = 0;
        sub2 = 0;
    } else {
        step = 0;
        mode++;
    }
}

void cCard::firstCheck30()
{
    mode = 4;
}

int CardCheckDone()
{
    return (pG->x8 & 0x80000000) != 0;
}

void cCard::MainLoop(int arg)
{
    static void (cCard::*tbl[9][3])() = {
        { &cCard::slotSelect, &cCard::slotSelect, &cCard::firstCheck00 },
        { &cCard::inSlotCheck, &cCard::inSlotCheck, &cCard::firstCheck10 },
        { &cCard::dataSelect, &cCard::dataSelect, &cCard::firstCheck20 },
        { &cCard::loadMain, &cCard::saveMain, &cCard::firstCheck30 },
        { &cCard::exit, &cCard::exit, &cCard::exit },
        { &cCard::errorDisp, &cCard::errorDisp, &cCard::errorDisp },
        { &cCard::format, &cCard::format, &cCard::format },
        { &cCard::fileDelete, &cCard::fileDelete, &cCard::fileDelete },
        { 0, &cCard::saveMain, &cCard::createSysfile },
    };

    if (pCard->initialize(arg) == 0) {
        mode = 4;
        step = 2;
    }
    while (exitFlag == 0) {
        if (type != 2) {
            if (bgmTimer == 0) {
                bgmStrId = SndStrReq(0, 0x1D, 0x80000003, 0, 0, 0.0f);
                bgmTimer = 0xA8C;
            } else {
                bgmTimer--;
            }
            eprintf(24, 32, 0, 0, "%d", bgmTimer);
        }
        TaskSleep(1);
        (this->*tbl[mode][arg])();
        if (dispFlag == 1) {
            screenTrans();
        }
    }
}

void CardMainTask(int arg)
{
    BitOn(pG->flags_54, 0x1000);
    pCard = new cCard;
    g_id = new CardID;
    if (pSys->language == 0) {
        cMes.loadFont(28, 28, "Font/common_j.fnt", 0);
    } else {
        cMes.loadFont(32, 32, "Font/common_p.fnt", 0);
    }
    cMes.setLanguage(pSys->language);
    cMes.setLayout(0, 3);
    cMes.setLayout(1, 3);
    cMes.setLayout(2, 3);
    pCard->MainLoop(arg);
    if (pCard) {
        delete pCard;
    }
    pCard = 0;
    delete g_id;
    g_id = 0;
    cMes.setLayout(0, 0);
    cMes.setLayout(1, 0);
    cMes.setLayout(2, 0);
    BitOff(pG->flags_54, 0x1000);
    TaskExit();
}

int CardLoad()
{
    int ret = 0;

    systemVISetBlack(0);
    TaskExec(1, (TaskFunc) CardMainTask, 0);
    TaskSleep(1);
    if (pG->x8 & 4) {
        SndAllFadeOut();
        ret = 1;
    }
    return ret;
}

void CardSave(int no, int f)
{
    if (f & 2) {
        BitOn(pG->x8, 8);
    }
    if (f & 4) {
        BitOn(pG->x8, 0x10);
    }
    if (f & 8) {
        BitOn(pG->x8, 0x20);
    }
    if (f & 0x10) {
        BitOn(pG->x8, 0x40);
    }
    if (f & 0x20) {
        BitOn(pG->x8, 0x98);
    }
    pG->snd_tbl_no = no;
    TaskExec(1, (TaskFunc) CardMainTask, 1);
    TaskSleep(1);
}

void CardSysSave()
{
    BitOn(pG->x8, 0x98);
    TaskExec(1, (TaskFunc) CardMainTask, 1);
    TaskSleep(1);
}

void CardFirstCheck()
{
    if (pRK->valid != 0 && pRK->x3C == 1) {
        BitOn(pG->x8, 0x80000000);
        TaskExit();
    }
    TaskChain((TaskFunc) CardMainTask, 2);
}

int cCard::existCheck(int chan, CardSlot* s)
{
    int ret = 0;

    if (chan == 2) {
        return 1;
    }
    result = CARDProbeEx(chan, &s->memSize, &s->sectorSize);
    switch (result) {
    case 0:
        ret = 1;
        if (s->sectorSize != 0x2000) {
            s->flags |= 0x80;
            ret = -1;
            result = -0x200;
        }
        break;
    case -3:
        s->flags |= 2;
        ret = -1;
        break;
    case -2:
        s->flags |= 0x20;
        ret = -1;
        break;
    case -0x80:
        s->flags |= 0x40;
        ret = -1;
        break;
    case -1:
        break;
    }
    return ret;
}

int cCard::mount(u8* sub, CardSlot* s)
{
    int ret = 0;

    if (s->chan == 2) {
        return 1;
    }
    switch (*sub) {
    case 0:
        CARDMountAsync(s->chan, s->workArea, 0, 0);
        (*sub)++;
        // fallthrough
    case 1:
        result = CARDGetResultCode(s->chan);
        switch (result) {
        case 0:
        case -6:
        case -0xD:
            OSReport("Slot %c Mount\n", s->chan + 'A');
            ret = 1;
            break;
        case -3:
            s->flags |= 2;
            ret = -1;
            break;
        case -2:
            s->flags |= 0x20;
            ret = -1;
            break;
        case -5:
        case -0x80:
            s->flags |= 0x40;
            ret = -1;
            break;
        case -1:
            break;
        }
        break;
    }
    if (ret != 0) {
        *sub = 0;
    }
    return ret;
}

int cCard::unmount(int chan)
{
    int ret = 0;

    result = CARDUnmount(chan);
    switch (result) {
    case -3:
        ret = 1;
        break;
    case -1:
        break;
    case 0:
        ret = 1;
        break;
    default:
        ret = 1;
        break;
    }
    if (ret == 1) {
        OSReport("Slot %c Unmount\n", chan + 'A');
        slotw[chan].flags = 0;
    }
    return ret;
}

int cCard::verifyCheck(u8* sub, CardSlot* s)
{
    int ret = 0;

    if (s->chan == 2) {
        return 1;
    }
    switch (*sub) {
    case 0:
        CARDCheckAsync(s->chan, 0);
        (*sub)++;
        // fallthrough
    case 1:
        result = CARDGetResultCode(s->chan);
        switch (result) {
        case 0:
            *sub = 0;
            ret = 1;
            break;
        case -6:
        case -0xD:
            s->flags |= 0x10;
            ret = -1;
            break;
        case -3:
            s->flags |= 2;
            ret = -1;
            break;
        case -5:
        case -0x80:
            s->flags |= 0x40;
            ret = -1;
            break;
        case -1:
            break;
        }
        break;
    }
    if (ret != 0) {
        *sub = 0;
    }
    return ret;
}

int cCard::freeCheck(u8* sub, CardSlot* s)
{
    int ret = 0;

    if (s->chan == 2) {
        return 1;
    }
    switch (*sub) {
    case 0:
        switch (CARDFreeBlocks(s->chan, &s->freeBytes, &s->freeFiles)) {
        case 0:
            (*sub)++;
            break;
        case -6:
            s->flags |= 0x10;
            ret = -1;
            break;
        case -3:
            s->flags |= 2;
            ret = -1;
            break;
        case -0x80:
            s->flags |= 0x40;
            ret = -1;
            break;
        case -1:
            break;
        }
        break;
    case 1: {
        u32 f = s->flags & 0x300;
        if (f == 0x200) {
            if (s->freeFiles > 0 && (u32) (s->freeBytes + 0x1FFF) / 0x2000 >= saveBlocks) {
                ret = 1;
            } else {
                ret = -1;
                result = -0x201;
                s->flags |= 0x804;
            }
        } else if (f == 0x100) {
            if (s->freeFiles > 0 && (u32) (s->freeBytes + 0x1FFF) / 0x2000 >= sysBlocks) {
                ret = 1;
            } else {
                ret = -1;
                result = -0x201;
                s->flags |= 0x404;
            }
        } else if (f == 0) {
            if (s->freeFiles > 1 && (u32) (s->freeBytes + 0x1FFF) / 0x2000 >= saveBlocks + sysBlocks) {
                ret = 1;
            } else {
                ret = -1;
                result = -0x201;
                s->flags |= 4;
            }
        } else {
            ret = 1;
        }
        break;
    }
    }
    if (ret != 0) {
        *sub = 0;
    }
    return ret;
}

int cCard::fileOpen(CardSlot* s)
{
    int ret = 0;

    result = CARDOpen(s->chan, fileName, &s->fileInfo);
    switch (result) {
    case 0:
    case -4:
        ret = 1;
        break;
    case -0x80:
        s->flags |= 0x40;
        ret = -1;
        break;
    case -3:
        s->flags |= 2;
        ret = -1;
        break;
    case -6:
    case -0xA:
        ret = -1;
        break;
    case -1:
        break;
    }
    return ret;
}

int cCard::fileClose(CardSlot* s)
{
    int ret = 0;

    result = CARDClose(&s->fileInfo);
    switch (result) {
    case 0:
        ret = 1;
        break;
    case -3:
        s->flags |= 2;
        ret = -1;
        break;
    case -0x80:
        s->flags |= 0x40;
        ret = -1;
        break;
    case -1:
        break;
    }
    return ret;
}

int cCard::saveFileCheck(u8* sub, CardSlot* s)
{
    int ret = 0;
    int r;
    int bit;

    switch (*sub) {
    case 0:
        BitOff(pSys->flags, 0x02000000);
        fileNo = 0;
        memclr_asm(s->fileFlag, sizeof(s->fileFlag));
        retry = 0;
        (*sub)++;
        // fallthrough
    case 1:
        if (fileNo == 20) {
            fileNo = 0;
            ret = 1;
            *sub = 0;
            break;
        }
        if (s->chan == 2) {
            int dbg = 1;
            if (!(pG->flags_54 & 0x20000)) {
                dbg = 0;
            }
            if (DBIsDebuggerPresent()) {
                BitOn(pG->flags_54, 0x20000);
            }
            sprintf(fileName, "d:\\bio4/room/savedata%02d.dat", fileNo);
            if (file_exist(fileName)) {
                BitOn(pSys->flags, 0x02000000);
                s->fileFlag[fileNo] |= 1;
                bit = 1 << fileNo;
                if (DBG_CACHED & bit) {
                    memcpy(pInfo[fileNo], pDbgSaveInfo[fileNo], 0x200);
                    OSReport("save Info data%d from cache.\n", fileNo);
                } else {
                    HDReadSeekLen(fileName, pInfo[fileNo], 0x2000, 0x200);
                    memcpy(pDbgSaveInfo[fileNo], pInfo[fileNo], 0x200);
                    isDbgInfoCached |= 1 << fileNo;
                    OSReport("save Info data%d cached.\n", fileNo);
                }
                if (((SaveInfo*) pInfo[fileNo])->magic != 0x116) {
                    s->fileFlag[fileNo] |= 4;
                }
            } else {
                memclr_asm(pDbgSaveInfo[fileNo], 0x200);
            }
            if (dbg == 0) {
                BitOff(pG->flags_54, 0x20000);
            }
            fileNo++;
        } else {
            sprintf(fileName, "bh4_data%02d", fileNo);
            r = fileOpen(s);
            if (r == 0) {
            } else if (r > 0) {
                if (result == 0) {
                    BitOn(pSys->flags, 0x02000000);
                    s->fileFlag[fileNo] |= 1;
                    s->flags |= 0x100;
                    if (type == 2) {
                        *sub = 3;
                    } else {
                        (*sub)++;
                    }
                } else {
                    retry = 0;
                    fileNo++;
                }
            } else if (r < 0) {
                if (type == 2) {
                    mode = 0;
                    step = 0;
                } else {
                    errorSet(result);
                }
            }
        }
        break;
    case 2:
        r = CARDGetStatus(slot, slotw[slot].fileInfo.fileNo, &slotw[slot].stat);
        if (r == -1) {
            break;
        }
        if (r == 0) {
            if (s->stat.commentAddr == 0xFFFFFFFF) {
                s->fileFlag[fileNo] |= 2;
                *sub = 4;
            } else {
                (*sub)++;
            }
        } else {
            errorSet(r);
        }
        break;
    case 3:
        r = fileRead(&sub2, pInfo[fileNo], 0x200, 0x2000, s);
        if (r == 0) {
        } else if (r > 0) {
            if (CRCVerify(pInfo[fileNo] + 4, 0x1FC, ((SaveInfo*) pInfo[fileNo])->crc) == 0) {
                if (retry == 3) {
                    s->fileFlag[fileNo] |= 2;
                    (*sub)++;
                } else {
                    retry++;
                }
            } else {
                (*sub)++;
                if (((SaveInfo*) pInfo[fileNo])->magic != 0x116) {
                    s->fileFlag[fileNo] |= 6;
                }
            }
        } else {
            if (retry == 3) {
                s->fileFlag[fileNo] |= 2;
                (*sub)++;
            } else {
                retry++;
            }
        }
        break;
    case 4:
        if (fileClose(s) != 0) {
            *sub = 1;
            retry = 0;
            fileNo++;
        }
        break;
    }
    return ret;
}

int cCard::systemFileCheck(u8* sub, CardSlot* s)
{
    int ret = 0;
    int r;

    if (s->chan == 2) {
        return 1;
    }
    switch (*sub) {
    case 0:
        fileNo = 0;
        sprintf(fileName, "bh4_system");
        (*sub)++;
        // fallthrough
    case 1:
        r = fileOpen(s);
        if (r == 0) {
        } else if (r > 0) {
            if (result == 0) {
                (*sub)++;
                s->flags |= 0x200;
            } else {
                ret = 1;
                *sub = 0;
            }
        } else if (r < 0) {
            if (type == 2) {
                mode = 0;
                step = 0;
            } else {
                errorSet(result);
            }
        }
        break;
    case 2:
        if (fileClose(s) != 0) {
            ret = 1;
            *sub = 0;
        }
        break;
    }
    return ret;
}

int cCard::fileRead(u8* sub, void* buf, s32 len, s32 ofs, CardSlot* s)
{
    int ret = 0;

    switch (*sub) {
    case 0:
        CARDReadAsync(&s->fileInfo, buf, len, ofs, 0);
        (*sub)++;
        // fallthrough
    case 1:
        result = CARDGetResultCode(s->chan);
        if (result == -1) {
            break;
        }
        if (result == 0) {
            ret = 1;
        } else {
            ret = -1;
        }
        break;
    }
    if (ret != 0) {
        DCFlushRange(buf, len);
        *sub = 0;
    }
    return ret;
}

int cCard::fileWrite(u8* sub, void* buf, int blocks, CardSlot* s)
{
    int ret = 0;

    switch (*sub) {
    case 0:
        CARDWriteAsync(&s->fileInfo, buf, blocks << 13, 0, 0);
        (*sub)++;
        // fallthrough
    case 1:
        result = CARDGetResultCode(s->chan);
        if (result == -1) {
            break;
        }
        if (result == 0) {
            ret = 1;
        } else {
            ret = -1;
        }
        break;
    }
    if (ret != 0) {
        *sub = 0;
    }
    return ret;
}

int cCard::sysfileRead(u8* sub, u8* sub2, int errMode)
{
    int ret = 0;
    int r;

    switch (*sub) {
    case 0:
        if (slot == 2) {
            *sub = 2;
            break;
        }
        r = fileOpen(&slotw[slot]);
        if (r == 0) {
        } else if (r > 0) {
            (*sub)++;
        } else if (r < 0) {
            if (errMode != 0) {
                errorSet(-0x206);
                return 0;
            }
            ret = -1;
        }
        break;
    case 1:
        r = CARDGetStatus(slot, slotw[slot].fileInfo.fileNo, &slotw[slot].stat);
        if (r == -1) {
            break;
        }
        if (r != 0) {
            if (errMode != 0) {
                errorSet(-0x206);
                return 0;
            }
            ret = -1;
            break;
        }
        if (slotw[slot].stat.commentAddr != 0xFFFFFFFF) {
            (*sub)++;
        } else {
            if (errMode != 0) {
                errorSet(-0x202);
                return 0;
            }
            ret = -1;
        }
        break;
    case 2:
        if (slot == 2) {
            int dbg = 1;
            if (!(pG->flags_54 & 0x20000)) {
                dbg = 0;
            }
            if (DBIsDebuggerPresent()) {
                BitOn(pG->flags_54, 0x20000);
            }
            sprintf(fileName, "d:\\bio4/room/sysdata.dat");
            r = HDRead(fileName, pSysBuf);
            if (dbg == 0) {
                BitOff(pG->flags_54, 0x20000);
            }
            if (r == 0) {
                ret = -1;
            } else {
                *sub = 3;
            }
        } else {
            r = fileRead(sub2, pSysBuf, sysBlocks << 13, 0, &slotw[slot]);
            if (r == 0) {
            } else if (r > 0) {
                (*sub)++;
            } else {
                if (errMode != 0) {
                    errorSet(-0x206);
                    return 0;
                }
                ret = -1;
            }
        }
        break;
    case 3:
        if (CRCVerify(pSysBuf, SYS_CRC, *(u32*) (pSysBuf + SYS_CRC)) == 0) {
            if (retry == 3) {
                if (errMode != 0) {
                    errorSet(-0x202);
                    return 0;
                }
                ret = -1;
            } else {
                *sub2 = 1;
                retry++;
            }
        } else {
            if (slot == 2) {
                ret = 1;
            } else {
                (*sub)++;
            }
        }
        break;
    case 4:
        r = fileClose(&slotw[slot]);
        if (r == 0) {
        } else if (r > 0) {
            ret = 1;
        } else if (r < 0) {
            if (errMode != 0) {
                errorSet(-0x206);
                return 0;
            }
            ret = -1;
        }
        break;
    }
    if (ret != 0) {
        *sub = 0;
        *sub2 = 0;
    }
    return ret;
}

void cCard::createSysfile()
{
    int noCard = 0;
    int ret;
    int sel;

    ret = CARDProbeEx(slot, 0, 0);
    sprintf(fileName, "bh4_system");
    switch (step) {
    case 0:
        cardMesSet(0x2C, 0, 0x800000);
        cMes.mes[0].cursor = 1;
        step++;
        // fallthrough
    case 1:
        if (ret == -3) {
            noCard = 1;
            break;
        }
        sel = cMes.mes[0].result;
        switch (sel) {
        case 1:
            CoreSeCall(4, 0, 0, 0, 0);
            cardMesSet(0x27, 0, 0);
            BitOn(pG->flags_54, 0x200);
            step++;
            break;
        case 2:
            CoreSeCall(5, 0, 0, 0, 0);
            errCode = -0x208;
            mode = 5;
            step = sel;
            sub = 0;
            sub2 = 0;
            deleteAllMes();
            break;
        }
        break;
    case 2:
        ret = mount(&sub, &slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            step++;
        } else if (ret < 0) {
            errorSet(result);
        }
        break;
    case 3:
        ret = fileCreate(&sub, sysBlocks, &slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            sub = 0;
            sub2 = 0;
            step++;
        } else if (ret < 0) {
            if (result == -5) {
                errorSet(-5);
            } else {
                errorSet(-0x205);
            }
        }
        break;
    case 4:
        ret = CARDGetStatus(slot, slotw[slot].fileInfo.fileNo, &slotw[slot].stat);
        if (ret == -1) {
            break;
        }
        if (ret == 0) {
            step++;
        } else {
            errorSet(-0x205);
        }
        break;
    case 5:
        makeSystemSaveData();
        step++;
        // fallthrough
    case 6:
        ret = fileWrite(&sub, pSysBuf, sysBlocks, &slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            step++;
        } else if (ret < 0) {
            if (result == -5) {
                errorSet(-5);
            } else {
                errorSet(-0x205);
            }
        }
        break;
    case 7:
        makeCardStatus(&slotw[slot]);
        CARDSetStatusAsync(slot, slotw[slot].fileInfo.fileNo, &slotw[slot].stat, 0);
        step++;
        // fallthrough
    case 8:
        ret = CARDGetResultCode(slot);
        switch (ret) {
        case -1:
            break;
        case 0:
            step++;
            break;
        case -5:
            errorSet(-5);
            break;
        default:
            errorSet(-0x205);
            break;
        }
        break;
    case 9:
        ret = fileClose(&slotw[slot]);
        if (ret == 0) {
        } else if (ret > 0) {
            timer = 0xF;
            step++;
            cardMesSet(0x28, 0, 0);
        } else if (ret < 0) {
            errorSet(-0x205);
        }
        break;
    case 10:
        if (timer == 0) {
            deleteAllMes();
            BitOff(pG->flags_54, 0x200);
            mode = 4;
            step = 0;
            sub = 0;
            sub2 = 0;
        } else {
            timer--;
        }
        break;
    }
    if (noCard == 1) {
        mode = 0;
        step = 0;
        sub = 0;
        sub2 = 0;
    }
}

void cCard::screenTrans()
{
    if (type != 2) {
        debugInfoDisp(slot, type);
    }
    eprintf(24, 16, 0, 0, "%02d%02d%02d%02d", mode, step, sub, sub2);
    if (type != 2 && !(pG->x8 & 0x80)) {
        g_id->move(this);
        g_id->idsys.move();
        g_id->idsys.trans();
    }
}

void cCard::cardMesSet(int no, int slot, u32 attr)
{
    MesPos* p = &mes_pos_tbl[pSys->language][no];
    cMes.MesSet(p->no, p->x, p->y, attr | 0x01020051, slot, 0, 4);
}

void cCard::calcTplAddr(TEXPalette* tpl)
{
    u32 i;
    TEXDescriptor* desc;

    if ((s32) tpl->descriptorArray < 0) {
        return;
    }
    tpl->descriptorArray = (TEXDescriptor*) ((u32) tpl->descriptorArray + (u32) tpl);
    desc = tpl->descriptorArray;
    for (i = 0; i < tpl->numDescriptors; i++, desc++) {
        desc->textureHeader = (TEXHeader*) ((u8*) tpl + (u32) desc->textureHeader);
        desc->CLUTHeader = (CLUTHeader*) ((u8*) tpl + (u32) desc->CLUTHeader);
        if (desc->textureHeader->unpacked == 0) {
            desc->textureHeader->data = (u8*) tpl + (u32) desc->textureHeader->data;
            desc->textureHeader->unpacked = 1;
        }
        if (desc->CLUTHeader->unpacked == 0) {
            desc->CLUTHeader->data = (u8*) tpl + (u32) desc->CLUTHeader->data;
            desc->CLUTHeader->unpacked = 1;
        }
    }
}

void cCard::setMsgWindow(int a, int b)
{
    if (type != 2) {
        if (pG->x8 & 0x80) {
            Cckpt.msgWindow(b);
        } else {
            setMsgBG(a, b);
        }
    }
}

void CRCInit()
{
    u32 i;
    int j;

    for (i = 0; i < 256; i++) {
        u32 c = i << 24;
        for (j = 0; j < 8; j++) {
            if ((s32) c < 0) {
                c = (c << 1) ^ 0x04C11DB7;
            } else {
                c = c << 1;
            }
        }
        CRCTable[i] = c;
    }
}

u32 CRCCalc(u8* data, u32 len)
{
    u32 crc = 0;
    u32 i;

    for (i = 0; i < len; i++) {
        crc = (crc << 8) ^ CRCTable[(crc >> 24) ^ data[i]];
    }
    return crc;
}

int CRCVerify(u8* data, u32 len, u32 saved)
{
    u32 crc = CRCCalc(data, len);
    if (saved == crc) {
        return 1;
    }
    OSReport("CRCVerifyCRC Error: CRCs doesn't match!!\nSaved CRC  = 0x%x\nActual CRC = 0x%x\n", saved, crc);
    return 0;
}

void CardDbgCacheSet()
{
    u8* p;
    int i;

    if (pG->dev_mode == 1 && isDbgInfoAlloc == 0) {
        p = (u8*) Debug_alloc(0x2800, 0);
        if (p != 0) {
            memclr_asm(p, 0x2800);
            for (i = 19; i >= 0; i--) {
                pDbgSaveInfo[i] = p + i * 0x200;
            }
            isDbgInfoAlloc = 1;
        }
    }
}

// Digits of `num` into id units idNo, idNo-1, ... (ones first). A macro: every expansion shares
// dispSaveInfo's `u` (the unit pointer is copied to the same register each time).
#define putNumber(id, num_, idNo_, digits_, type)         \
    {                                                     \
        int d[3];                                         \
        int i;                                            \
        num = (num_);                                     \
        for (i = 0; i <= (digits_) - 1; i++) {            \
            d[i] = num % 10;                              \
            num /= 10;                                    \
            u = (id)->unitPtr((idNo_) - i, type);         \
            u->flags_7F |= 2;                             \
            u->no = d[i];                                 \
        }                                                 \
    }

void dispSaveInfo(int no, SaveInfo* info, u8 type, int broken)
{
    IDSystem* id = &g_id->idsys;
    IdUnit* u;
    int chapter;
    int special;
    int chap;
    int sec;
    u32 h;
    u32 m;
    u32 s;
    int num;

    putNumber(id, no + 1, 2, 2, type);
    u = id->unitPtr(0x16, type);
    if (info == 0) {
        u->flags &= ~8;
        return;
    }
    u->flags |= 8;
    special = 0;
    chapter = info->chapter;
    id->unitPtr(0x20, type)->flags &= ~8;
    id->unitPtr(7, type)->flags &= ~8;
    id->unitPtr(6, type)->flags &= ~8;
    id->unitPtr(0x21, type)->flags &= ~8;
    id->unitPtr(0x19, type)->flags &= ~8;
    if (broken) {
        u = id->unitPtr(0x20, type);
    } else {
        switch (info->mode) {
        case 1:
            if (chapter == 0x12) {
                u = id->unitPtr(0x19, type);
                special = 1;
            } else {
                u = id->unitPtr(7, type);
            }
            break;
        case 2:
            chapter--;
            u = id->unitPtr(6, type);
            break;
        case 3:
            u = id->unitPtr(0x21, type);
            special = 1;
            break;
        default:
            goto skip;
        }
    }
    u->flags |= 8;
skip:
    getChapterSection(chapter, &chap, &sec);
    u = id->unitPtr(5, type);
    u->flags_7F |= 2;
    u->no = sec;
    u = id->unitPtr(3, type);
    u->flags_7F |= 2;
    u->no = chap;
    if (broken || special) {
        id->unitPtr(3, type)->flags &= ~8;
        id->unitPtr(4, type)->flags &= ~8;
        id->unitPtr(5, type)->flags &= ~8;
    } else {
        id->unitPtr(3, type)->flags |= 8;
        id->unitPtr(4, type)->flags |= 8;
        id->unitPtr(5, type)->flags |= 8;
    }
    putNumber(id, info->x40, 0xA, 3, type);
    if (broken) {
        id->unitPtr(8, type)->flags &= ~8;
        id->unitPtr(9, type)->flags &= ~8;
        id->unitPtr(0xA, type)->flags &= ~8;
    } else {
        id->unitPtr(8, type)->flags |= 8;
        id->unitPtr(9, type)->flags |= 8;
        id->unitPtr(0xA, type)->flags |= 8;
    }
    SecToTime(info->playTime, &h, &m, &s);
    putNumber(id, h, 0xC, 2, type);
    u = id->unitPtr(0xD, type);
    u->no = 0xB;
    u->flags_7F |= 2;
    putNumber(id, m, 0xF, 2, type);
    u = id->unitPtr(0x10, type);
    u->no = 0xB;
    u->flags_7F |= 2;
    putNumber(id, s, 0x12, 2, type);
    if (broken) {
        id->unitPtr(0xB, type)->flags &= ~8;
        id->unitPtr(0xC, type)->flags &= ~8;
        id->unitPtr(0xD, type)->flags &= ~8;
        id->unitPtr(0xE, type)->flags &= ~8;
        id->unitPtr(0xF, type)->flags &= ~8;
        id->unitPtr(0x10, type)->flags &= ~8;
        id->unitPtr(0x11, type)->flags &= ~8;
        id->unitPtr(0x12, type)->flags &= ~8;
    } else {
        id->unitPtr(0xB, type)->flags |= 8;
        id->unitPtr(0xC, type)->flags |= 8;
        id->unitPtr(0xD, type)->flags |= 8;
        id->unitPtr(0xE, type)->flags |= 8;
        id->unitPtr(0xF, type)->flags |= 8;
        id->unitPtr(0x10, type)->flags |= 8;
        id->unitPtr(0x11, type)->flags |= 8;
        id->unitPtr(0x12, type)->flags |= 8;
    }
    num = info->count + 1;
    if (info->mode == 3) {
        num = info->count;
    }
    putNumber(id, num, 0x14, 2, type);
    if (broken) {
        id->unitPtr(0x13, type)->flags &= ~8;
        id->unitPtr(0x14, type)->flags &= ~8;
    } else {
        id->unitPtr(0x13, type)->flags |= 8;
        id->unitPtr(0x14, type)->flags |= 8;
    }
    id->unitPtr(0x22, type)->flags &= ~8;
    id->unitPtr(0x17, type)->flags &= ~8;
    id->unitPtr(0x18, type)->flags &= ~8;
    if (pSys->language == 0) {
        switch (info->x3D) {
        case 1:
            id->unitPtr(0x22, type)->flags |= 8;
            break;
        case 3:
        default:
            id->unitPtr(0x17, type)->flags |= 8;
            break;
        case 5:
            id->unitPtr(0x18, type)->flags |= 8;
            break;
        }
    } else if (pSys->language == 1) {
        switch (info->x3D) {
        case 5:
        default:
            id->unitPtr(0x17, type)->flags |= 8;
            break;
        case 6:
            id->unitPtr(0x18, type)->flags |= 8;
            break;
        }
    } else {
        switch (info->x3D) {
        case 3:
            id->unitPtr(0x22, type)->flags |= 8;
            break;
        case 5:
        default:
            id->unitPtr(0x17, type)->flags |= 8;
            break;
        case 6:
            id->unitPtr(0x18, type)->flags |= 8;
            break;
        }
    }
}

void CardID::updateSaveInfo(cCard* c)
{
    int i;

    for (i = 0; i < 7; i++) {
        int type = 0x40 + i;
        s8 sl = c->slot;
        int base = c->fileNo - 3;
        int no = base + i;
        u32 f;
        // The original loop body spans more than 100 insn slots (notes included), so haifa never
        // schedules it as one interblock region; the empty blocks add the missing BLOCK notes.
        {{{{{{{{{{{{{{{{{{{{}}}}}}}}}}}}}}}}}}}}
        if (no < 0) {
            no += 20;
        }
        if (no > 19) {
            no -= 20;
        }
        g_id->idsys.unitPtrI(0x15, type)->flags |= 8;
        f = (&c->slotw[sl])->fileFlag[no];
        if (f & 1) {
            if (f & 2) {
                dispSaveInfoI(no, (SaveInfo*) c->pInfo[(s8) no], type, 1);
            } else {
                dispSaveInfoI(no, (SaveInfo*) c->pInfo[(s8) no], type, 0);
            }
        } else {
            dispSaveInfoI(no, 0, type, 0);
        }
    }
}

void CardID::init(int type, CardArc* data)
{
    int i;
    IdUnit* u;
    IdUnit* v;
    f32 zero;

    this->type = type;
    pTex = (u8*) (data->ofs[0] + (u32) data);
    pSaveDat = (u8*) (data->ofs[1] + (u32) data);
    pFile = (u8*) (data->ofs[2] + (u32) data);
    pFrame = (u8*) (data->ofs[3] + (u32) data);
    pLoadDat = (u8*) (data->ofs[4] + (u32) data);
    pBg = (u8*) (data->ofs[5] + (u32) data);
    idsys.gameInit(0x100);
    IdTexRoomInit();
    IdTexDataLoad(pTex, 6);
    IdSys.kill(0xFF, 0x28);
    IdSys.kill(0xFF, 0x29);
    IdSys.kill(0xFF, 0x21);
    IdSys.kill(0xFF, 0x20);
    IdSys.kill(0xFF, 0x23);
    IdSys.kill(0xFF, 0x30);
    IdSys.kill(0xFF, 0x2B);
    IdSys.kill(0xFF, 0x2A);
    idsys.set(pFrame, 0xFF, 0x18, 9, 3, 0);
    for (i = 0; i < 7; i++) {
        idsys.setI(pFile, 0xFF, 0x40 + i, 0xC, 6, 0);
    }
    if (this->type == 1) {
        IdSys.set(pSaveDat, 0xFF, 0x10, 0xF, 2, 0);
        IdSys.unitPtr(1, 0x10)->dir |= 0xF;
    } else if (this->type == 0) {
        IdSys.set(pLoadDat, 0xFF, 0x10, 0xF, 2, 0);
    }
    IdSys.set(pBg, 0xFF, 0x11, 0xF, 1, 0);
    IdSys.unitPtr(0, 0x11)->flags &= ~8;
    IdSys.unitPtr(0, 0x11)->dir |= 0xF;
    IdSys.unitPtr(1, 0x11)->flags &= ~8;
    IdSys.unitPtr(1, 0x11)->dir |= 0xF;
    zero = 0.0f;
    for (int j = 0; j < 7; j++) {
        IdUnit* p = g_id->idsys.unitPtrI(0x15, 0x40 + j);
        IdUnit* q = g_id->idsys.unitPtr((u8) (j + 0x10), 0x18);
        q->kind = 1;
        FSet(p->scr.z, zero);
        FSet(p->scr.y, zero);
        FSet(p->scr.x, zero);
        g_id->idsys.unitParent(q, p);
    }
    u = g_id->idsys.unitPtr(0, 0x18);
    g_p_path_org[0] = u->path0;
    g_p_hrmt_org[0] = u->curve[0];
    g_p_spln_org[0] = u->path1;
    g_pos0_org = u->scr;
    v = g_id->idsys.unitPtr(0xA, 0x18);
    u->path0 = 0;
    u->curve[0] = 0;
    u->path1 = 0;
    u->scr = v->scr;
    u->dir |= 0xF;
    IdSys.unitPtr(5, 0x10)->dir |= 0xF;
    {
        IdUnit* w = IdSys.unitPtr(2, 0x10);
        w->no = 0;
        w->flags_7F |= 2;
    }
    state = 0;
    step = 0;
    x76 = 0;
    x77 = 0;
}

void CardID::move(cCard* c)
{
    static void (CardID::*tbl[6])(cCard*) = {
        &CardID::wait, &CardID::start, &CardID::normal, &CardID::up_down, &CardID::up_down, &CardID::save,
    };
    IdUnit* a;
    IdUnit* b;

    (this->*tbl[state])(c);
    if (state == 3) {
        idsys.unitPtr(1, 0x18)->flags &= ~8;
        idsys.unitPtr(0x15, 0x40)->flags &= ~8;
    } else {
        idsys.unitPtr(1, 0x18)->flags |= 8;
        idsys.unitPtr(0x15, 0x40)->flags |= 8;
    }
    if (c->mode == 5) {
        a = idsys.unitPtr(0, 0x18);
        b = idsys.unitPtr(0xA, 0x18);
        if ((a->dir & 0xF) == 0) {
            SndCall(0, 0x2A, 0, 0, 0, 0);
            a->path0 = b->path0;
            a->curve[0] = b->curve[0];
            a->path1 = b->path1;
            a->scr = b->scr;
            FuncPathParametrize(a->path0, a->path1);
            idsys.setTimeS(a, (s8) a->curve[0]->key[a->curve[0]->num - 1].t);
            a->dir |= 0xF;
            setAction(0);
            IdSys.unitPtr(5, 0x10)->dir |= 0xF;
            state = 0;
        }
    }
}

void CardID::wait(cCard* c)
{
    IdUnit* a;
    IdUnit* b;
    int v = 1;

    if (!(c->flag & 2)) {
        v = 0;
    }
    if (v) {
        BitOff(c->flag, 2);
        a = g_id->idsys.unitPtr(0, 0x18);
        b = g_id->idsys.unitPtr(0xA, 0x18);
        a->path0 = b->path0;
        a->curve[0] = b->curve[0];
        a->path1 = b->path1;
        a->scr = b->scr;
        FuncPathParametrize(a->path0, a->path1);
        IdSys.setTime(a, 0);
        a->dir &= ~0xF;
        IdSys.unitPtr(5, 0x10)->dir &= ~0xF;
        updateSaveInfo(c);
        state = 1;
        SndCall(0, 0x2D, 0, 0, 0, 0);
    }
}

void CardID::start(cCard* c)
{
    state = 2;
    if (idsys.unitPtr(0, 0x18)->end & 1) {
        state = 2;
    }
}

void CardID::normal(cCard* c)
{
    IdUnit* u;

    if (action & 4) {
        u = IdSys.unitPtr(1, 0x10);
        u->dir &= ~0xF;
        IdSys.setTime(u, 0);
        state = 5;
        SndCall(0, 0x2D, 0, 0, 0, 0);
    } else if (action & 1) {
        state = 3;
        updateSaveInfo(c);
        up_down(c);
    } else if (action & 2) {
        state = 4;
        updateSaveInfo(c);
        up_down(c);
    } else {
        u = IdSys.unitPtr(2, 0x10);
        u->flags_7F |= 2;
    }
}

void CardID::up_down(cCard* c)
{
    IdUnit* a;
    IdUnit* b;
    IdUnit* w;

    switch (step) {
    case 0:
        a = idsys.unitPtr(0, 0x18);
        setAction(8);
        if (state == 4) {
            u8 ids[2];
            int i;
            b = idsys.unitPtr(8, 0x18);
            if (type == 1) {
                ids[0] = 3;
                ids[1] = 4;
            } else {
                ids[0] = 1;
                ids[1] = 3;
            }
            for (i = 0; i < 2; i++) {
                IdSys.setTime(IdSys.unitPtr(ids[0], 0x10), 0);
                IdSys.setTime(IdSys.unitPtr(ids[1], 0x10), 0);
            }
            w = IdSys.unitPtr(2, 0x10);
            w->texCnt = 0;
            w->flags_7F |= 2;
        } else {
            b = idsys.unitPtr(9, 0x18);
            w = IdSys.unitPtr(2, 0x10);
            w->texCnt = 0;
            w->flags_7F &= ~2;
        }
        a->path0 = b->path0;
        a->curve[0] = b->curve[0];
        a->path1 = b->path1;
        a->scr = b->scr;
        FuncPathParametrize(a->path0, a->path1);
        IdSys.setTime(a, 0);
        step++;
        break;
    case 1:
        if (idsys.unitPtr(0, 0x18)->end & 1) {
            IdUnit* u = idsys.unitPtr(0, 0x18);
            u->path0 = g_p_path_org[0];
            u->curve[0] = (Hermite1*) g_p_hrmt_org[0];
            u->path1 = g_p_spln_org[0];
            u->scr = g_pos0_org;
            FuncPathParametrize(u->path0, u->path1);
            IdSys.setTime(u, 0);
            setAction(0);
            state = 2;
            step = 0;
        } else {
            if (action & 1) {
                state = 3;
                step = 0;
            } else if (action & 2) {
                state = 4;
                step = 0;
            }
        }
        if (state == 3) {
            w = IdSys.unitPtr(2, 0x10);
            if (w->texCnt > 4) {
                w->texCnt = 0;
                w->flags_7F |= 2;
            }
        }
        break;
    }
}

void CardID::save(cCard* c)
{
    IdUnit* u = IdSys.unitPtr(1, 0x10);
    Hermite1* h = u->curve[0];
    int n = ((s8*) h)[3];
    int i;

    if (c->mode == 5) {
        u->dir |= 0xF;
        return;
    }
    if (u->end & 1) {
        setAction(0);
        return;
    }
    for (i = 0; i < n; i++) {
        if ((s8) h->key[i].t == (s16) u->timer[0]) {
            switch (i) {
            case 0:
                break;
            case 1:
                updateSaveInfo(c);
                break;
            case 2:
            case 3:
                break;
            default:
                SndCall(0, 0x2E, 0, 0, 0, 0);
                break;
            }
            break;
        }
    }
}

void CardID::quit()
{
    idsys.free();
    if (!(pG->flags_5014 & 0x8000)) {
        Cckpt.roomInit();
    }
}

void CardID::setAction(int a)
{
    action = a;
}

void setMsgBG(int a, int b)
{
    IdUnit* u;

    if (a == 0) {
        u = IdSys.unitPtr(0, 0x11);
    } else {
        u = IdSys.unitPtr(1, 0x11);
    }
    switch (b) {
    case 1:
        u->flags |= 8;
        u->dir &= ~0xF;
        break;
    case 0:
        u->dir |= 0xF;
        break;
    }
}

asm(".section .sdata; .balign 32");
