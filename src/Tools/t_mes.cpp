#include "types.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "mes.h"
#include "t_util.h"

extern "C" unsigned int strlen(const char* s);

// Message debug tool (Tools/t_mes.cpp): shows one message of the current message data set, lets the pad
// move it and edit the message colour table.

// simple vertical text menu: tbl[0] is the header, the list ends with "\\"
class cIdToolMenu {
public:
    char** tbl;     // 0x00
    int num;        // 0x04  entries (without the header)
    int cursor;     // 0x08
    int maxLen;     // 0x0C
    s16 x;          // 0x10
    s16 y;          // 0x12
    // 0x14 vptr

    cIdToolMenu(char** tbl, int x, int y);
    virtual ~cIdToolMenu() {}
    int ToolMenuMove(int flag);
    void ToolMenuLocate(int dx, int dy, int clamp);
};

class cMessageDebug {
public:
    u8 step;              // 0x00
    u8 sub;               // 0x01
    u8 x2;                // 0x02
    u8 x3;                // 0x03
    u8* buf;              // 0x04
    cIdToolMenu* pMenu;   // 0x08
    int xC;               // 0x0C
    u32 attr;             // 0x10
    s16 mesNo;            // 0x14
    s16 mesMax;           // 0x16
    s16 x;                // 0x18
    s16 y;                // 0x1A
    // 0x1C vptr

    cMessageDebug() { step = sub = x2 = x3 = 0; }
    virtual ~cMessageDebug() {}
    void move();
    void init();
    void menu();
    void message();
    void color();
    void locate();
    void type();
    void language();
    void data();
    void quit();
};

static char* fileTbl[] = {
    "-MESSAGE FILES-", "cardmes.mdt", "commumes.mdt", "edmes.mdt",  "helpmes.mdt", "lobbymes.mdt",
    "optmes.mdt",      "shopmes.mdt", "sortiemes.mdt", "submes.mdt", "\\",
};
static char* menuTbl[] = {
    "-MENU-", "MESSAGE", "LOCATE", "COLOR", "TYPE", "LANGUAGE", "LOAD", "QUIT", "\\",
};

void ToolMes()
{
    cMessageDebug dbg;

    dbg.init();
    TaskSuspend(0);
    dbg.move();
    TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x80000000;
    TaskSignal(0);
    TaskExit();
}

void cMessageDebug::move()
{
    int loop = 1;

    do {
        switch (step) {
        case 0:
            menu();
            break;
        case 1:
            message();
            break;
        case 2:
            color();
            break;
        case 3:
            locate();
            break;
        case 4:
            type();
            break;
        case 5:
            language();
            break;
        case 6:
            data();
            break;
        case 7:
            quit();
            break;
        default:
            loop = 0;
            break;
        }
        if (pMenu) {
            pMenu->ToolMenuLocate(Joy[0].ssx / 4, Joy[0].ssy / 4, 1);
        }
        TaskSleep(1);
    } while (loop);
}

void cMessageDebug::init()
{
    MesData.lang = 0;
    attr = 1;
    x = 30;
    y = 360;
    mesNo = 0;
    mesMax = 0;
    pMenu = 0;
    buf = new u8[0x200000];
}

void cMessageDebug::menu()
{
    int ret;

    switch (sub) {
    case 0:
        pMenu = new cIdToolMenu(menuTbl, 50, 120);
        sub++;
        break;
    case 1:
        ret = pMenu->ToolMenuMove(0);
        if (ret != -1) {
            if (pMenu) {
                delete pMenu;
                pMenu = 0;
            }
            switch (ret) {
            case 0:
                step = 7;
                break;
            case 1:
                step = 1;
                break;
            case 2:
                step = 3;
                break;
            case 3:
                step = 2;
                break;
            case 4:
                step = 4;
                break;
            case 5:
                step = 5;
                break;
            case 6:
                step = 6;
                break;
            case 7:
                step = 7;
                break;
            }
            sub = 0;
        }
        break;
    }
}

void cMessageDebug::message()
{
    MessageControl* pm = &cMes;
    int i;

    if (pm->mes[0].flags2 & 1) {
        pm->Move();
        return;
    }
    if (Joy[0].trg & 0x100) {
        pm->MesSet(mesNo, x, y, attr, 0, 0, 4);
        for (i = 0; i < 3; i++) {
            cMes.getMes(0)->setJump(0xFFFF);
        }
    } else if (Joy[0].trg & 0x200) {
        pm->Delete(0);
        step = 0;
    } else if (Joy[0].rep & 0x80008) {
        mesNo--;
        if (mesNo < 0) {
            mesNo = mesMax - 1;
            if (mesNo < 0) {
                mesNo = 0;
            }
        }
    } else if (Joy[0].rep & 0x40004) {
        mesNo++;
        if (mesNo >= mesMax) {
            mesNo = 0;
        }
    }
    eprintf(50, 120, 0, 0, "MESSAGE: %d / %d", mesNo, mesMax);
    eprintf(50, 136, 0, 0, "SELECT UP/DOWN");
}

static u8 colIdx = 0;
static u8 colCur = 0;

void cMessageDebug::color()
{
    const char* names[6] = {"WHITE", "RED", "GREEN", "BLUE", "ORANGE", "BLACK"};
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    JOY* joy = &Joy[0];
    u8* p;

    eprintf(40, 140, colCur == 0 ? 4 : 0, 15, "MODE: %s", names[colIdx]);
    if (joy->trg & 0x200) {
        step = 0;
        return;
    }
    if (joy->trg & 0x80008) {
        if (colCur != 0) {
            colCur--;
        }
    } else if (joy->trg & 0x40004) {
        if (colCur != 4) {
            colCur++;
        }
    }
    if (colCur == 0) {
        if (joy->trg & 0x10001) {
            if (colIdx != 0) {
                colIdx--;
            }
        } else if (joy->trg & 0x20002) {
            if (colIdx != 5) {
                colIdx++;
            }
        }
    }
    r = mes_col_tbl[colIdx] >> 24;
    g = (mes_col_tbl[colIdx] >> 16) & 0xFF;
    b = (mes_col_tbl[colIdx] >> 8) & 0xFF;
    a = mes_col_tbl[colIdx] & 0xFF;
    switch (colCur) {
    case 1:
        p = &r;
        break;
    case 2:
        p = &g;
        break;
    case 3:
        p = &b;
        break;
    case 4:
        p = &a;
        break;
    default:
        p = 0;
        break;
    }
    if (p) {
        if (joy->rep & 0x10001) {
            if (*p != 0) {
                (*p)--;
            }
        } else if (joy->rep & 0x20002) {
            if (*p != 0xFF) {
                (*p)++;
            }
        }
    }
    mes_col_tbl[colIdx] = (r << 24) | (g << 16) | (b << 8) | a;
    eprintf(40, 220, colCur == 1 ? 4 : 0, 0, "R: %02x", r);
    eprintf(40, 240, colCur == 2 ? 4 : 0, 0, "G: %02x", g);
    eprintf(40, 260, colCur == 3 ? 4 : 0, 0, "B: %02x", b);
    eprintf(40, 280, colCur == 4 ? 4 : 0, 0, "A: %02x", a);
}

void cMessageDebug::locate()
{
    JOY* joy = &Joy[0];

    x += joy->sx / 4;
    y -= joy->sy / 4;
    if (joy->rep & 8) {
        y--;
    } else if (joy->rep & 4) {
        y++;
    } else if (joy->rep & 1) {
        x--;
    } else if (joy->rep & 2) {
        x++;
    } else if (joy->trg & 0x100) {
        x = 30;
        y = 360;
    } else if (joy->trg & 0x200) {
        step = 0;
    }
    eprintf(x, y, 0, 0, "LOCATE: %d / %d", x, y);
}

static char* typeTbl[] = {"-TYPE-", " CORE", " ROOM", " FREE", "\\"};

void cMessageDebug::type()
{
    int ret;

    switch (sub) {
    case 0:
        pMenu = new cIdToolMenu(typeTbl, 50, 120);
        sub++;
        break;
    case 1:
        ret = pMenu->ToolMenuMove(0);
        if (ret != -1) {
            if (pMenu) {
                delete pMenu;
                pMenu = 0;
            }
            switch (ret) {
            case 0:
                break;
            case 1:
                attr = 1;
                mesMax = MesData.getMesNum(0);
                break;
            case 2:
                attr = 2;
                mesMax = MesData.getMesNum(1);
                break;
            case 3:
                attr = 4;
                mesMax = MesData.getMesNum(2);
                break;
            }
            mesNo = 0;
            step = 0;
            sub = 0;
        }
        break;
    }
}

static char* langTbl[] = {"-LANGUAGE-", " ENGLISH", "\\"};
static int langBak = 0;  // unreferenced (.data 0x21E4)

void cMessageDebug::language()
{
    int ret;

    switch (sub) {
    case 0:
        pMenu = new cIdToolMenu(langTbl, 50, 120);
        sub++;
        break;
    case 1:
        ret = pMenu->ToolMenuMove(0);
        if (ret != -1) {
            if (pMenu) {
                delete pMenu;
                pMenu = 0;
            }
            switch (ret) {
            case 0:
                break;
            case 1:
                MesData.lang = 1;
                break;
            }
            step = 0;
            sub = 0;
        }
        break;
    }
}

void cMessageDebug::data()
{
    step = 0;
}

void cMessageDebug::quit()
{
    delete buf;
    step = 8;
}

cIdToolMenu::cIdToolMenu(char** t, int px, int py)
{
    int i;

    tbl = t;
    x = px;
    y = py;
    cursor = 0;
    num = 0;
    while (t[num][0] != '\\') {
        num++;
    }
    if (num > 0) {
        num--;
    }
    maxLen = 0;
    for (i = 0; i < num; i++) {
        int len = strlen(tbl[i]);

        if (maxLen < len) {
            maxLen = len;
        }
    }
}

int cIdToolMenu::ToolMenuMove(int flag)
{
    int ret = -1;
    int i;

    eprintf(x, y, 4, 0, "%s", tbl[0]);
    for (i = 1; i <= num; i++) {
        int col = 0;

        if (cursor == i - 1) {
            col = 5;
        }
        eprintf(x, i * 16 + y, col, 0, "%s", tbl[i]);
    }
    if (Joy[0].trg & 0x100) {
        ret = cursor + 1;
    } else if (Joy[0].trg & 0x200) {
        if (flag) {
            ret = 0;
        } else {
            if (cursor != num - 1) {
                cursor = num - 1;
            } else {
                ret = 0;
            }
        }
    } else if (Joy[0].rep & 0x80008) {
        cursor--;
        if (cursor < 0) {
            cursor = num - 1;
        }
    } else if (Joy[0].rep & 0x40004) {
        cursor++;
        if (cursor > num - 1) {
            cursor = 0;
        }
    }
    return ret;
}

void cIdToolMenu::ToolMenuLocate(int dx, int dy, int clamp)
{
    x += dx;
    y -= dy;
    if (clamp) {
        if (x < 16) {
            x = 16;
        } else if (x > 496) {
            x = 496;
        }
        if (y < 0) {
            y = 0;
        } else if (y > 432) {
            y = 432;
        }
    }
}
