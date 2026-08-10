// game/datactrl: streamed room data units in MRAM/ARAM (D:/Bio4/Prog/datactrl.cpp).
// All functions byte-identical (the static-initializer trio is only named differently); .rodata and
// all section sizes match.
//  - dispDebug: `x1` is ONE function-scope variable shared by the loop's second conversion and the
//    `over` block's bar width, so the allocno's conflicts are the union of both ranges (r0/r9/r11/r10/r8
//    temps of the over block, r6 = x0, r7 = the `x1 - x0` temp) and it lands in r5; a block-scoped x1
//    in the loop conflicts with nothing and takes the r7 preference set_preference inherits from the
//    local-alloc'd `subf` temp. The loop's `x0` stays block-scoped (p=r31 via reusing `p` for the tiles).
#include "types.h"
#include "global.h"
#include "datactrl.h"
#include "dvd.h"
#include "main.h"
#include "main_mem.h"
#include "db_log.h"
#include "eprintf.h"
#include "libgpu.h"
#include "snd.h"

extern "C" {
void OSReport(const char* fmt, ...);
void* memcpy(void* dst, const void* src, unsigned int n);
void DCFlushRange(void* addr, u32 nBytes);
unsigned int strlen(const char* s);
char* strcpy(char* dst, const char* src);
}

// Not the do { } while (0) form of the other units: the OSReport stays in the caller's block and
// the preceding pLog->err argument loads are scheduled against it (setData).
#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

// The debug bar primitive is 0x20 bytes here (tile[2] is 0x40).
struct DcTile {
    u32 tag;          // 0x00
    u32 code;         // 0x04
    GpuColor c0;      // 0x08
    s16 x0, y0;       // 0x0C
    s16 w, h;         // 0x10
    s16 z0;           // 0x14
    u8 pad_16[0xA];
};

#define ARAM_END 0xD00000

cDataCtrl DC;

#line 52 "D:/Bio4/Prog/datactrl.cpp"
inline void cDataUnit::setName(char* s)
{
    if (s != NULL) {
        if (strlen(s) > 0x1F) {
            pLog->err(0, 0, "DATANAME STRING OVER: %s", s);
            HALT();
        }
        strcpy(name, s);
    } else {
        name[0] = 0;
    }
}

void cDataUnit::setCommand(int cmd, u32 arg, u8 wait)
{
    command = cmd;
    this->arg = arg;
    this->wait = wait;
    if (wait != 0 || DC.xA0C != 1) {
        checkCommand();
    }
}

int cDataUnit::getCommand()
{
    return command;
}

void cDataUnit::setCondition(int c)
{
    condition = c;
}

int cDataUnit::getCondition()
{
    return condition;
}

void cDataUnit::checkMallocRelease()
{
    if (chk(2) == 1) {
        if (DC.dbgHeap == 1) {
            Debug_free_h(mallocAddr, heap);
        } else {
            Mem_free_h(mallocAddr, heap);
        }
        flag &= ~2;
    }
}

void cDataUnit::setMallocInfo(int on, void* p)
{
    if (chk(2) == 1) {
        checkMallocRelease();
    }
    if (on == 1) {
        flag |= 2;
    } else {
        flag &= ~2;
    }
    mallocAddr = p;
    if (DC.dbgHeap == 1) {
        heap = MemGetCurrentDbgHeap();
    } else {
        heap = MemGetCurrentHeap();
    }
}

void cDataUnit::fixMramAddr(u32 a)
{
    fixAddr = a;
}

int cDataUnit::isUseOk()
{
    if (condition == 0 && command == 0) {
        err = 5;
        return 0;
    }
    return getCondition() == 2;
}

int cDataUnit::waitUseOk()
{
    if (condition == 0 && command == 0) {
        err = 5;
        return 0;
    }
    while (isUseOk() == 0) {
        waitFlag = 1;
        checkCondition();
        if (getCondition() == 4) {
            checkCommand();
        }
        if (err != 0) {
            return 0;
        }
    }
    return 1;
}

int cDataUnit::isLoadOk()
{
    if (condition == 0 && command == 0) {
        err = 5;
        return 0;
    }
    if (condition == 2 || condition == 4) {
        return 1;
    }
    return 0;
}

int cDataUnit::waitLoadOk()
{
    if (condition == 0 && command == 0) {
        err = 5;
        return 0;
    }
    while (isLoadOk() == 0) {
        waitFlag = 1;
        checkCondition();
        if (err != 0) {
            return 0;
        }
    }
    return 1;
}

void cDataUnit::setLoadToMram()
{
    int no;

    switch (condition) {
    case 1:
    case 3:
    case 5:
    case 6:
    case 7:
    case 8:
        break;
    case 0:
        if (fixAddr == 0) {
            if (arg == 0) {
                if (DC.dbgHeap == 1) {
                    dest = (u32) Debug_alloc(size, 1);
                } else {
#line 200 "D:/Bio4/Prog/datactrl.cpp"
                    dest = (u32) MEM_ALLOC(size, 1, 0xD);
                }
                if (dest == 0) {
                    err = 1;
                    return;
                }
                setMallocInfo(1, (void*) dest);
            } else {
                dest = arg;
                setMallocInfo(0, NULL);
            }
        } else {
            dest = fixAddr;
            setMallocInfo(0, NULL);
        }
#line 222 "D:/Bio4/Prog/datactrl.cpp"
        no = DvdReadN(name, (void*) dest, 0, 0, 0, wait | 0x10, __FILE__, __LINE__);
        if (pG->dev_mode == 1) {
#line 226 "D:/Bio4/Prog/datactrl.cpp"
            DC.setDummyId(DvdReadN("dummy.dat", DC.dummyBuf, 0, 0, 0, wait | 0x10, __FILE__, __LINE__));
        }
        reqNo = no;
        if (no >= 0) {
            command = 0;
            condition = 1;
            if (wait == 1) {
                checkLoadToMram();
            }
            OSReport("DC:%s set MRAM_LOAD\n", name);
        } else {
            err = 2;
            checkMallocRelease();
            pLog->err(0, 0, "cDataUnit::setLoadToMram command error");
        }
        break;
    case 2:
        if (fixAddr == 0) {
            if (arg != 0 && arg != (u32) addr) {
                checkMallocRelease();
                memcpy((void*) arg, addr, size);
                addr = (void*) arg;
                DCFlushRange((void*) arg, size);
            }
        } else if (fixAddr != (u32) addr) {
            checkMallocRelease();
            memcpy((void*) fixAddr, addr, size);
            addr = (void*) fixAddr;
            DCFlushRange((void*) arg, size);
        }
        command = 0;
        OSReport("DC:%s set MRAM_TO_MRAM\n", name);
        break;
    case 4:
        if (fixAddr == 0) {
            if (arg == 0) {
                if (DC.dbgHeap == 1) {
                    dest = (u32) Debug_alloc(size, 1);
                } else {
#line 290 "D:/Bio4/Prog/datactrl.cpp"
                    dest = (u32) MEM_ALLOC(size, 1, 0xD);
                }
                if (dest == 0) {
                    err = 1;
                    return;
                }
                setMallocInfo(1, (void*) dest);
            } else {
                dest = arg;
                setMallocInfo(0, NULL);
            }
        } else {
            dest = fixAddr;
            setMallocInfo(0, NULL);
        }
        no = Aram.DmaTransReq(1, (u32) addr, dest, size, wait);
        reqNo = no;
        if (no >= 0) {
            command = 0;
            condition = 5;
            if (wait == 1) {
                checkAramToMram();
            }
            OSReport("DC:%s set ARAM_TO_MRAM\n", name);
        } else {
            err = 3;
            checkMallocRelease();
            pLog->err(0, 0, "cDataUnit::setLoadToMram command error");
        }
        break;
    }
}

void cDataUnit::setLoadToAram()
{
    int no;

    switch (condition) {
    case 1:
    case 3:
    case 5:
    case 6:
    case 7:
    case 8:
        break;
    case 0:
        if (arg == 0) {
            dest = DC.getAramFree(size);
            if (dest == 0) {
                err = 4;
                pLog->err(0, 0, "ARAM over: %s", name);
                break;
            }
        } else {
            dest = arg;
        }
#line 366 "D:/Bio4/Prog/datactrl.cpp"
        no = DvdReadN(name, NULL, dest, 0, 0, wait | 0x8, __FILE__, __LINE__);
        if (pG->dev_mode == 1) {
#line 370 "D:/Bio4/Prog/datactrl.cpp"
            DC.setDummyId(DvdReadN("dummy.dat", DC.dummyBuf, 0, 0, 0, wait | 0x10, __FILE__, __LINE__));
        }
        reqNo = no;
        if (no >= 0) {
            command = 0;
            condition = 3;
            if (wait == 1) {
                checkLoadToAram();
            }
            OSReport("DC:%s set ARAM_LOAD\n", name);
        } else {
            err = 2;
            checkMallocRelease();
            pLog->err(0, 0, "cDataUnit::setLoadToAram command error");
        }
        break;
    case 2:
        if (arg == 0) {
            dest = DC.getAramFree(size);
            if (dest == 0) {
                err = 4;
                pLog->err(0, 0, "ARAM over: %s", name);
                break;
            }
        } else {
            dest = arg;
        }
        no = Aram.DmaTransReq(0, (u32) addr, dest, size, wait);
        reqNo = no;
        if (no >= 0) {
            command = 0;
            condition = 6;
            if (wait == 1) {
                checkMramToAram();
            }
            OSReport("DC:%s set MRAM_TO_ARAM\n", name);
        } else {
            err = 3;
            checkMallocRelease();
            pLog->err(0, 0, "cDataUnit::setLoadToAram command error");
        }
        break;
    case 4:
        if (arg != 0 && arg != (u32) addr) {
            if (DC.dbgHeap == 1) {
                dest = (u32) Debug_alloc(size, 1);
            } else {
#line 452 "D:/Bio4/Prog/datactrl.cpp"
                dest = (u32) MEM_ALLOC(size, 0, 0xD);
            }
            if (dest != 0) {
                setMallocInfo(1, (void*) dest);
                no = Aram.DmaTransReq(1, (u32) addr, dest, size, wait);
                reqNo = no;
                if (no >= 0) {
                    command = 0;
                    condition = 7;
                    OSReport("DC:%s set ARAM_TO_ARAM\n", name);
                } else {
                    err = 3;
                    checkMallocRelease();
                    pLog->err(0, 0, "cDataUnit::setLoadToAram command error");
                }
            } else {
                setClear();
                setCommand(2, 0, 0);
            }
        }
        break;
    }
}

int cDataUnit::setClear()
{
    command = 0;
    switch (condition) {
    case 5:
    case 6:
    case 7:
        Aram.DmaCancel(reqNo);
        break;
    case 2:
    case 4:
    case 8:
        break;
    case 1:
    case 3:
        Dvd.ReadCancel(reqNo, 0x40);
        Dvd.ReadCheck(reqNo, NULL, NULL, NULL);
        break;
    case 0:
        goto clear;
    default:
        goto ret;
    }
    OSReport("DC:%s set CLEAR\n", name);
clear:
    checkMallocRelease();
    addr = NULL;
    dest = 0;
    condition = 0;
ret:
    return 1;
}

int cDataUnit::setDelete()
{
    setClear();
    size = 0;
    flag &= ~1;
    OSReport("DC:%s set DELETE\n", name);
    return 1;
}

void cDataUnit::checkLoadToMram()
{
    int ret;

    if (waitFlag == 1) {
        Dvd.ReadNblk2Blk(reqNo);
        waitFlag = 0;
    }
    ret = Dvd.ReadCheck(reqNo, NULL, NULL, NULL);
    if (ret > 0) {
        condition = 2;
        addr = (void*) dest;
        OSReport("DC:%s check MRAM_OK\n", name);
    } else if (ret < 0) {
        err = 2;
        checkMallocRelease();
        pLog->err(0, 0, "cDataUnit::checkLoadToMram command error");
    }
}

void cDataUnit::checkLoadToAram()
{
    int ret;

    if (waitFlag == 1) {
        Dvd.ReadNblk2Blk(reqNo);
        waitFlag = 0;
    }
    ret = Dvd.ReadCheck(reqNo, NULL, NULL, NULL);
    if (ret > 0) {
        condition = 4;
        addr = (void*) dest;
        OSReport("DC:%s check ARAM_OK\n", name);
    } else if (ret < 0) {
        err = 2;
        checkMallocRelease();
        pLog->err(0, 0, "cDataUnit::checkLoadToAram command error");
    }
}

void cDataUnit::checkAramToMram()
{
    int done = 0;

    if (waitFlag == 1) {
        while (Aram.TransCheck(reqNo) != 1) {
        }
        waitFlag = 0;
        done = 1;
    }
    if (Aram.TransCheck(reqNo) == 1 || done == 1) {
        condition = 2;
        addr = (void*) dest;
        OSReport("DC:%s check MRAM_OK\n", name);
    }
}

void cDataUnit::checkMramToAram()
{
    int done = 0;

    if (waitFlag == 1) {
        while (Aram.TransCheck(reqNo) != 1) {
        }
        waitFlag = 0;
        done = 1;
    }
    if (Aram.TransCheck(reqNo) == 1 || done == 1) {
        checkMallocRelease();
        condition = 4;
        addr = (void*) dest;
        OSReport("DC:%s check ARAM_OK\n", name);
    }
}

void cDataUnit::checkAramToAram()
{
    int done = 0;

    if (waitFlag == 1) {
        while (Aram.TransCheck(reqNo) != 1) {
        }
        waitFlag = 0;
        done = 1;
    }
    if (Aram.TransCheck(reqNo) == 1 || done == 1) {
        addr = (void*) dest;
        reqNo = Aram.DmaTransReq(0, dest, arg, size, wait);
        dest = arg;
        if (reqNo >= 0) {
            condition = 6;
            OSReport("DC:%s set MRAM_TO_ARAM\n", name);
        } else {
            err = 3;
            checkMallocRelease();
            pLog->err(0, 0, "cDataUnit::checkAramToAram command error");
        }
    }
}

void cDataUnit::checkMramToMram()
{
}

void cDataUnit::checkCommand()
{
    switch (getCommand()) {
    case 0:
        break;
    case 1:
        setLoadToMram();
        break;
    case 2:
        setLoadToAram();
        break;
    case 3:
        setClear();
        break;
    case 4:
        setDelete();
        break;
    }
}

void cDataUnit::checkCondition()
{
    switch (getCondition()) {
    case 0:
        break;
    case 1:
        checkLoadToMram();
        break;
    case 2:
        break;
    case 3:
        checkLoadToAram();
        break;
    case 4:
        break;
    case 5:
        checkAramToMram();
        break;
    case 6:
        checkMramToAram();
        break;
    case 7:
        checkAramToAram();
        break;
    case 8:
        checkMramToMram();
        break;
    }
}

struct AramArea {
    u32 addr;
    u32 size;
};

u32 cDataCtrl::getAramFree(u32 size)
{
    AramArea tbl[32];
    AramArea tmp;
    int n;
    int i, j;
    u32 k;
    u32 base;
    cDataUnit* u;

    n = 0;
    for (i = 0; i < 32; i++) {
        u = &unit[i];
        if (u->chk(1) != 0) {
            switch (u->getCondition()) {
            case 7:
                tbl[n].addr = u->arg;
                tbl[n].size = u->size;
                n++;
                // fallthrough
            case 4:
            case 5:
                tbl[n].addr = (u32) u->addr;
                tbl[n].size = u->size;
                n++;
                break;
            case 3:
            case 6:
                tbl[n].addr = u->dest;
                tbl[n].size = u->size;
                n++;
                break;
            case 0:
            case 1:
            case 2:
            case 8:
                break;
            }
        }
    }
    if (n != 0) {
        for (i = 0; i < n - 1; i++) {
            for (j = i; j < n; j++) {
                if (tbl[i].addr > tbl[j].addr) {
                    tmp = tbl[j];
                    tbl[j] = tbl[i];
                    tbl[i] = tmp;
                }
            }
        }
        base = ARAM_FREE_BASE;
        for (i = 0; i < n; i++) {
            if ((int) (tbl[i].addr - base) >= (int) size) {
                for (k = 0; k < 32; k++) {
                    u = &unit[k];
                    if (u->chk(1) != 0 && base == u->dest) {
#line 852 "D:/Bio4/Prog/datactrl.cpp"
                        HALT();
                    }
                }
                return base;
            }
            base = tbl[i].addr + tbl[i].size;
        }
        aramEnd = base + size;
        if (aramEnd > ARAM_END - 1) {
            return 0;
        }
        return base;
    }
    return ARAM_FREE_BASE;
}

void cDataCtrl::init()
{
    initDataUnit();
    dummyBuf = Debug_alloc(0x100, 1);
    dispBuf = Debug_alloc(0x400, 1);
}

void cDataCtrl::initDataUnit()
{
    cDataUnit* u;
    u32 i;

    aramEnd = ARAM_FREE_BASE;
    setAramSort(1);
    xA08 = 1;
    dbgHeap = 0;
    for (i = 0; i < 32; i++) {
        u = &unit[i];
        memclr_asm(u, sizeof(cDataUnit));
        u->flag &= ~1;
        u->setCondition(0);
        u->setCommand(0, 0, 0);
        u->err = 0;
        u->fixMramAddr(0);
    }
    initDummyId();
}

void cDataCtrl::deleteAll()
{
    u32 i;

    for (i = 0; i < 32; i++) {
        unit[i].setDelete();
    }
}

cDataUnit* cDataCtrl::setData(char* name)
{
    u32 len;
    cDataUnit* u;

    if (Dvd.FileExistCheck(name, &len) < 0) {
        pLog->err(0, 0, "cDataCtrl::setData(\"%s\") not found", name);
        return NULL;
    }
    u = getNewUnit();
    if (u != NULL) {
        OSReport("DataCtrl::setData(\"%s\") %d succeed\n", name, len);
        u->size = len;
        u->setName(name);
    }
    return u;
}

cDataUnit* cDataCtrl::getNewUnit()
{
    cDataUnit* u;
    u32 i;

    for (i = 0; i < 32; i++) {
        u = &unit[i];
        if (u->chk(1) == 0) {
            u->flag |= 1;
            u->setCondition(0);
            u->setCommand(0, 0, 0);
            u->err = 0;
            u->size = 0;
            u->name[0] = 0;
            u->addr = NULL;
            return u;
        }
    }
    pLog->err(0, 0, "cDataCtrl::setData  DC work over");
    return NULL;
}

void cDataCtrl::setAramSort(int on)
{
    aramSort = on;
}

int cDataCtrl::checkAramSort()
{
    cDataUnit* tbl[32];
    cDataUnit* tmp;
    u8 n;
    int i, j;
    u32 base;
    cDataUnit* u;

    if (aramSort == 0) {
        return 0;
    }
    n = 0;
    for (i = 0; i < 32; i++) {
        u = &unit[i];
        if (u->chk(1) != 0) {
            if (u->getCommand() == 0) {
                switch (u->getCondition()) {
                case 0:
                case 1:
                case 2:
                    break;
                case 4:
                    tbl[n] = u;
                    n++;
                    break;
                case 3:
                case 5:
                case 6:
                case 7:
                    return 0;
                case 8:
                    break;
                }
            } else {
                return 0;
            }
        }
    }
    if (n != 0) {
        for (i = 0; i < n - 1; i++) {
            for (j = i; j < n; j++) {
                if ((u32) tbl[i]->addr > (u32) tbl[j]->addr) {
                    tmp = tbl[j];
                    tbl[j] = tbl[i];
                    tbl[i] = tmp;
                }
            }
        }
        base = ARAM_FREE_BASE;
        for (i = 0; i < n; i++) {
            if (base < (u32) tbl[i]->addr) {
                tbl[i]->setCommand(2, base, 0);
                tbl[i]->setLoadToAram();
                return 1;
            }
            base += tbl[i]->size;
        }
        aramEnd = base;
    }
    return 0;
}

void cDataCtrl::dispDebug()
{
    static DcTile tile[2];
    DcTile* p;
    int over;
    int y;
    cDataUnit* u;
    u32 i;
    int x;
    u32 x1;

    dispBase = ARAM_FREE_BASE;
    dispEnd = ARAM_END;
    over = 0;
    p = NULL;
    if (dispBuf != NULL) {
        p = (DcTile*) dispBuf;
    } else {
        over = 1;
    }
    eprintf(0x28, 0x2E, 0, 0x17, "[ARAM DATA DISP]");
    y = 0x2E;
    x = 0x1F8;
    for (i = 0; i < 32; i++) {
        u = &unit[i];
        if (u->chk(1) != 0) {
            u32 addr = 0;
            u32 size = 0;
            u32 x0;
            switch (u->getCondition()) {
            case 0:
            case 1:
            case 2:
            case 8:
                continue;
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                addr = (u32) u->addr;
                size = u->size;
                break;
            }
            y += 0x10;
            eprintf(0x28, y, 0, 0x17, "%08x:%s", u->addr, u->name);
            x0 = (u32) ((f32) (addr - dispBase) * 400.0f / (f32) (dispEnd - dispBase));
            x1 = (u32) ((f32) (addr + size - dispBase) / (f32) (dispEnd - dispBase) * 400.0f);
            if (over == 0) {
                p->code = GPU_TILE;
                p->x0 = x;
                p->y0 = x0 + 0x1E;
                p->w = 5;
                p->h = x1 - x0;
                p->z0 = 0;
                p->c0.r = 0x90;
                p->c0.g = 0x50;
                p->c0.b = 0x50;
                p->c0.cd = 0xFF;
                AddPrim(&MainOt[1], (u32*) p);
                p++;
                if (u == &unit[32]) {
                    over = 1;
                }
            }
        }
    }
    if (over == 1) {
        p = &tile[0];
        x1 = (u32) ((f32) (aramEnd - dispBase) * 400.0f / (f32) (dispEnd - dispBase));
        p->code = GPU_TILE;
        p->x0 = x;
        p->y0 = 0x1E;
        p->z0 = 0;
        p->w = 5;
        p->h = x1;
        p->c0.r = 0x90;
        p->c0.g = 0x50;
        p->c0.b = 0x50;
        p->c0.cd = 0xFF;
        AddPrim(&MainOt[1], (u32*) p);
    }
    p = &tile[1];
    p->code = GPU_TILE;
    p->x0 = x;
    p->y0 = 0x1E;
    p->z0 = 0;
    p->w = 5;
    p->h = 400;
    p->c0.r = 0x20;
    p->c0.g = 0x20;
    p->c0.b = 0x20;
    p->c0.cd = 0xFF;
    AddPrim(&MainOt[1], (u32*) p);
}

void cDataCtrl::initDummyId()
{
    int i;

    for (i = 0; i < 32; i++) {
        dummyId[i] = -1;
    }
}

void cDataCtrl::setDummyId(int id)
{
    int i;

    if (pG->dev_mode != 0 && id >= 0) {
        for (i = 0; i < 32; i++) {
            if (dummyId[i] == -1) {
                dummyId[i] = id;
                return;
            }
        }
        pLog->err(0, 0, "cDataCtrl::setDummyId: DummyId work over!!");
    }
}

void cDataCtrl::checkDummyId()
{
    int i;
    int ret;

    if (pG->dev_mode != 0) {
        for (i = 0; i < 32; i++) {
            if (dummyId[i] != -1) {
                ret = Dvd.ReadCheck(dummyId[i], NULL, NULL, NULL);
                if (ret > 0) {
                    dummyId[i] = -1;
                } else if (ret < 0) {
                    pLog->err(0, 0, "cDataCtrl::checkDummyId: failed");
                }
            }
        }
    }
}

void cDataCtrl::check()
{
    cDataUnit* u;
    int i;

    if (xA08 != 0 && xA0C != 1) {
        while (checkAramSort() == 1) {
        }
        for (i = 0; i < 32; i++) {
            u = &unit[i];
            if (u->chk(1) != 0) {
                u->err = 0;
                u->checkCommand();
                u->checkCondition();
            }
        }
        checkDummyId();
    }
}
