// game/block.cpp: room block streaming (see block.h).

#include "atari.h"
#include "block.h"
#include "global.h"
#include "player.h"
#include "obj.h"
#include "scroll.h"
#include "datactrl.h"
#include "libgpu.h"
#include "main_mem.h"
#include "db_log.h"
#include "eprintf.h"
#include "joy.h"
#include "rnd.h"

extern "C" {
void OSReport(const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
int strcmp(const char* a, const char* b);
void* memcpy(void* dst, const void* src, unsigned int n);
void DCFlushRange(void* addr, u32 nBytes);
}
void TaskSleep(int frames);   // game/scheduler.cpp
void* GetDataExt(void* arc, const char* tag, int no);   // game/read.cpp

cBlock Block;

static inline int bitChk(u32* set, u32 no)
{
    return set[no >> 5] & (0x80000000 >> (no & 0x1F));
}

static inline void bitOn(u32* set, u32 no)
{
    set[no >> 5] |= 0x80000000 >> (no & 0x1F);
}

static inline void bitOff(u32* set, u32 no)
{
    set[no >> 5] &= ~(0x80000000 >> (no & 0x1F));
}

static inline int isFree(u8 flags)
{
    return !(flags & 1);
}

void cBlock::setOtStart()
{
    pOt = &ot[7];
}

u32* cBlock::getOtAddr()
{
    u32 v;

    while ((v = *pOt) != 0xFFFFFFFF) {
        pOt = (u32*) (v | 0x80000000);
        if ((s32) v < 0) {
            return pOt;
        }
    }
    return 0;
}

void cBlock::roomInit(void* data)
{
    BlockHeader* h = (BlockHeader*) data;
    int i;

    pData = 0;
    pLink = 0;
    pArea = 0;
    pConnect = 0;
    stopFlagSet = 0;
    if (!(pG->flags_60 & 0x80000000)) {
        debugData = 0;
        debugMem = 0;
        allDisp = 0;
        noMemCtrl = 0;
    }
    if (h == 0) {
        nBlock = 0;
        return;
    }
    if (strcmp((char*) h, "BLK") != 0) {
        pLog->err(0, 0, "THIS DATA IS NOT BLOCK AREA DATA");
        return;
    }
    if (h->version != 0x100) {
        pLog->err(0, 0, "BLOCK AREA DATA IS OLD VERSION");
        return;
    }
    pData = h;
    nBlock = h->nBlock;
    pLink = (BlockLink*) ((u8*) h + h->ofsLink);
    pArea = (BlockArea*) ((u8*) h + h->ofsArea);
    pConnect = (BlockConnect*) ((u8*) h + h->ofsConnect);
    if (debugData == 0) {
#line 94 "D:/Bio4/Prog/block.cpp"
        pUnit = (cBlockUnit*) MEM_CALLOC(nBlock * sizeof(cBlockUnit), 1, 13);
    }
    if (nBlock == 0) {
        if (Block.debugData == 1) {
            if (pData != 0) {
                Debug_free(pData);
            }
            if (getUnitPtr(0) != 0) {
                Debug_free(getUnitPtr(0));
            }
            Block.debugData = 0;
        }
        pData = 0;
        return;
    }
    ClearOTagR(ot, 8);
    for (i = pData->nArea - 1; i >= 0; i--) {
        AddPrim(&ot[pArea[i].pri], &pArea[i].tag);
    }
    for (i = 0; i < nBlock; i++) {
        if (pLink[i].flags & 1) {
            getBlockWork(i);
        }
    }
    if (debugMem == 0 && checkBlockMemory() == 0) {
        if (Block.debugData == 1) {
            if (pData != 0) {
                Debug_free(pData);
            }
            if (getUnitPtr(0) != 0) {
                Debug_free(getUnitPtr(0));
            }
            Block.debugData = 0;
        }
        pData = 0;
        return;
    }
    pG->area_no = -1;
    check(1);
}

int cBlock::checkBlockMemory()
{
    u32 max = 0;
    u32 size;
    u32 i;
    u32 j;

    for (i = 0; i < pData->nConnect; i++) {
        BlockConnect* c = (BlockConnect*) (i * sizeof(BlockConnect) + (u32) pConnect);
        size = 0;
        if (c->flags & 1) {
            checkBlockConnect(c, pLink, &mramSet, &aramSet);
            for (j = 0; j < nBlock; j++) {
                if (bitChk(&mramSet, j)) {
                    size += getUnitPtr(j)->pData->size;
                }
            }
        }
        if (max < size) {
            max = size;
        }
    }
    if (max == 0) {
        return 0;
    }
#line 182 "D:/Bio4/Prog/block.cpp"
    memCur = memTop = MEM_ALLOC(max, 1, 13);
    if (memTop == 0) {
        pLog->err(0, 0, "checkBlockMemory: malloc failed");
        return 0;
    }
    memEnd = (u8*) memTop + max;
    return 1;
}

void cBlock::useDebugMemory(int on, u32 size)
{
    if (debugMem == 1) {
        if (memTop != 0) {
            Debug_free(memTop);
        }
        memTop = saveMemTop;
        memEnd = saveMemEnd;
        debugMem = 0;
    }
    if (on == 1) {
        saveMemTop = memTop;
        saveMemEnd = memEnd;
        memTop = Debug_alloc(size, 0);
        memEnd = (u8*) memTop + size;
        debugMem = on;
    }
}

void cBlock::dispAllBlock(int on)
{
    u32 size;
    u32 i;

    if (nBlock == 0) {
        return;
    }
    if (on == 1) {
        if (allDisp != 0) {
            return;
        }
        size = 0;
        for (i = 0; i < nBlock; i++) {
            cBlockUnit* u = getUnitPtr(i);
            if (u->flags & 1) {
                size += u->pData->size;
                u->setBlockDelete();
                u->checkBlockDelete();
            }
        }
        useDebugMemory(1, size);
        allDisp = 1;
        for (i = 0; i < nBlock; i++) {
            cBlockUnit* u = getUnitPtr(i);
            if (u->flags & 1) {
                u->setBlockCommand(BLOCK_CMD_MRAM_LOAD, 1);
            }
        }
    } else {
        if (allDisp != 1) {
            return;
        }
        for (i = 0; i < nBlock; i++) {
            cBlockUnit* u = getUnitPtr(i);
            if (u->flags & 1) {
                while (u->state != BLOCK_CREATE) {
                    TaskSleep(1);
                }
                u->setBlockDelete();
                u->checkBlockDelete();
            }
        }
        for (i = 0; i < 2; i++) {
            ObjMgr.dieCheck();
        }
        useDebugMemory(0, 0);
        allDisp = 0;
        pGS->area_no = -1;
    }
}

void cBlock::check(int arg)
{
    cBlockUnit* u;
    int area;
    u32 i;

    if (pData == 0) {
        pG->area_no = 0;
        return;
    }
    if (allDisp == 1) {
        return;
    }
    if (pG->debug_mode == 0x12 && pG->room_id == 5) {
        area = pG->area_no;
        if (Joy[0].trg & 0x10) {
            area = Rnd() % nBlock;
            if (pG->area_no == area) {
                area++;
            }
            area = area % nBlock;
        }
    } else {
        area = checkBlockArea(&pPL->pos, pG->area_no);
    }
    if (pG->area_no != area) {
        prevArea = pG->area_no;
        pG->area_no = area;
        checkBlockConnect(&pConnect[area], pLink, &mramSet, &aramSet);
        for (i = 0; i < nBlock; i++) {
            if (isFree(pLink[i].flags)) {
                continue;
            }
            u = getUnitPtr(i);
            if (bitChk(&mramSet, i)) {
                u->setBlockCommand(BLOCK_CMD_MRAM_LOAD, arg);
            } else if (bitChk(&aramSet, i)) {
                u->setBlockCommand(BLOCK_CMD_ARAM_LOAD, arg);
            } else {
                u->setBlockCommand(BLOCK_CMD_DELETE, arg);
            }
        }
    }
    for (i = 0; i < nBlock; i++) {
        u = getUnitPtr(i);
        switch (u->command) {
        case BLOCK_CMD_ARAM_LOAD:
        case BLOCK_CMD_DELETE:
            u->setTrans(0);
            break;
        }
    }
    dispDebugInfo();
}

s8 cBlock::checkBlockArea(Vec* pos, int now)
{
    Vec p;
    BlockArea* a;
    u32 pri;
    s8 ret;

    if (pData == 0) {
        return 0;
    }
    pri = 0;
    ret = now;
    p = *pos;
    p.y += 200.0f;
    setOtStart();
    while ((a = (BlockArea*) getOtAddr()) != 0) {
        if (isFree(a->flags)) {
            continue;
        }
        if (AreaHitCheck(&a->area, &p) != 1) {
            continue;
        }
        if (pri > a->pri) {
            break;
        }
        ret = a->areaNo;
        pri = a->pri;
        if (ret == now) {
            break;
        }
    }
    return ret;
}

void cBlock::checkBlockConnect(BlockConnect* c, BlockLink* link, u32* mram, u32* aram)
{
    u32 i;

    memclr_asm(mram, 4);
    memclr_asm(aram, 4);
    if (isFree(c->flags)) {
        return;
    }
    bitOn(mram, c->blockNo);
    checkBlockConnect_sub(c->blockNo, link, mram);
    for (i = 0; i < nBlock; i++) {
        if (bitChk(mram, i)) {
            checkBlockConnect_sub(i, link, aram);
        }
    }
    for (i = 0; i < nBlock; i++) {
        if (bitChk(mram, i)) {
            bitOff(aram, i);
        }
    }
    for (i = 0; i < 8; i++) {
        if (c->mram[i] != -1) {
            bitOn(aram, c->mram[i]);
        }
    }
    for (i = 0; i < 8; i++) {
        if (c->aram[i] != -1) {
            bitOff(aram, c->aram[i]);
        }
    }
}

void cBlock::checkBlockConnect_sub(u8 blk, BlockLink* link, u32* set)
{
    u32 i;
    int j;

    for (i = 0; i < nBlock; i++) {
        BlockLink* l = &link[i];
        if (i == blk) {
            for (j = 0; j < 8; j++) {
                if (l->link[j] != -1) {
                    bitOn(set, l->link[j]);
                }
            }
        } else {
            for (j = 0; j < 8; j++) {
                if (l->link[j] == blk) {
                    bitOn(set, i);
                }
            }
        }
    }
}

void cBlockUnit::setBlockCommand(int cmd, int a)
{
    command = cmd;
    arg = a;
}

void cBlockUnit::setTrans(int on)
{
    u32 i;

    for (i = 0; i < ObjMgr.nArray; i++) {
        cObj* o = (cObj*) ((u8*) ObjMgr.pArray + ObjMgr.size * i);
        if ((o->be_flag & 0x201) == 1 && o->id == 2 && o->blk == no) {
            if (on == 1) {
                o->be_flag |= 2;
            } else {
                o->be_flag &= ~2;
            }
        }
    }
}

void cBlockUnit::setBlockLoadToMram()
{
    switch (state) {
    case BLOCK_ARAM_LOAD_SET:
    case BLOCK_ARAM_LOAD:
    case BLOCK_DELETE:
        break;
    case BLOCK_NO_DATA:
    case BLOCK_ARAM_OK:
        state = BLOCK_MRAM_LOAD_SET;
        command = BLOCK_CMD_NONE;
        OSReport("BLOCK:%2d MRAM_LOAD_SET\n", no);
        break;
    case BLOCK_MRAM_LOAD_SET:
    case BLOCK_MRAM_LOAD:
    case BLOCK_CREATE:
        command = BLOCK_CMD_NONE;
        break;
    }
}

void cBlockUnit::setBlockLoadToAram()
{
    switch (state) {
    case BLOCK_MRAM_LOAD_SET:
    case BLOCK_MRAM_LOAD:
    case BLOCK_DELETE:
        break;
    case BLOCK_CREATE:
        recalcModelAddr(0);
        BlockDestroy(no);
        OSReport("BLOCK:%2d BlockDestroy\n", no);
    case BLOCK_NO_DATA:
        state = BLOCK_ARAM_LOAD_SET;
        command = BLOCK_CMD_NONE;
        OSReport("BLOCK:%2d ARAM_LOAD_SET\n", no);
        break;
    case BLOCK_ARAM_LOAD_SET:
    case BLOCK_ARAM_LOAD:
    case BLOCK_ARAM_OK:
        command = BLOCK_CMD_NONE;
        break;
    }
}

void cBlockUnit::setBlockDelete()
{
    switch (state) {
    case BLOCK_MRAM_LOAD_SET:
    case BLOCK_MRAM_LOAD:
    case BLOCK_ARAM_LOAD_SET:
    case BLOCK_ARAM_LOAD:
        break;
    case BLOCK_CREATE:
        BlockDestroy(no);
        OSReport("BLOCK:%2d BlockDestroy\n", no);
    case BLOCK_NO_DATA:
    case BLOCK_ARAM_OK:
    case BLOCK_DELETE:
        state = BLOCK_DELETE;
        command = BLOCK_CMD_NONE;
        OSReport("BLOCK:%2d DELETE\n", no);
        break;
    }
}

int cBlockUnit::checkBlockLoadToMramSet()
{
    void* p;

    switch (pData->getCondition()) {
    case 0:
    case 1:
        Block.stopFlagSet = 1;
    case 4:
        if (Block.noMemCtrl == 1) {
            pData->setCommand(1, 0, arg);
        } else {
            p = Block.getBlockMemFree(pData->size);
            if (p == 0) {
                return 1;
            }
            pData->setCommand(1, (u32) p, arg);
        }
    case 2:
    case 5:
    case 8:
        state = BLOCK_MRAM_LOAD;
        break;
    case 3:
    case 6:
    case 7:
        Block.stopFlagSet = 1;
        break;
    }
    return 1;
}

int cBlockUnit::checkBlockLoadToMram()
{
    switch (pData->getCondition()) {
    case 0:
    case 1:
        Block.stopFlagSet = 1;
        break;
    case 2:
        BlockCreate(no, (cSmd*) GetDataExt(pData->addr, "SMD", 0));
        setTrans(1);
        state = BLOCK_CREATE;
        if (Block.allDisp == 1) {
            ObjMgr.move();
        }
        OSReport("BLOCK:%2d BlockCreate\n", no);
        return 1;
    }
    return 0;
}

int cBlockUnit::checkBlockLoadToAramSet()
{
    int ret = 0;

    switch (pData->getCondition()) {
    case 1:
    case 5:
        break;
    case 0:
        ret = 1;
    case 2:
        pData->setCommand(2, 0, arg);
    case 6:
        state = BLOCK_ARAM_LOAD;
        break;
    case 3:
    case 4:
    case 7:
        state = BLOCK_ARAM_LOAD;
        ret = 1;
        break;
    }
    return ret;
}

int cBlockUnit::checkBlockLoadToAram()
{
    switch (pData->getCondition()) {
    case 4:
        state = BLOCK_ARAM_OK;
        return 1;
    case 3:
    case 7:
        return 1;
    }
    return 0;
}

int cBlockUnit::checkBlockDelete()
{
    int ret = 0;

    switch (pData->getCondition()) {
    case 1:
    case 5:
    case 6:
    case 8:
        break;
    case 0:
    case 2:
    case 4:
        pData->setClear();
        state = BLOCK_NO_DATA;
    case 3:
    case 7:
        ret = 1;
        break;
    }
    return ret;
}

void cBlockUnit::recalcModelAddr(int ofs)
{
    cObj* o;

    for (o = ObjMgr.pAlive; o != 0; o = (cObj*) o->next) {
        if (o->id == 2 && o->blk == no) {
            o->moveDataAddr(ofs);
        }
    }
}

void cBlockUnit::moveBlockData(void* dst)
{
    int ofs;

    memcpy(dst, pData->addr, pData->size);
    ofs = (int) dst - (int) pData->addr;
    pData->addr = dst;
    DCFlushRange(dst, pData->size);
    recalcModelAddr(ofs);
    ((cSmd*) GetDataExt(dst, "SMD", 0))->slide(ofs);
}

cBlockUnit* cBlock::getUnitPtr(u8 no)
{
    if (no >= nBlock) {
        return 0;
    }
    return &pUnit[no];
}

int cBlock::getBlockWork(u8 no)
{
    char name[64];
    cBlockUnit* u;

    if (no >= nBlock) {
        return 0;
    }
    u = getUnitPtr(no);
    sprintf(name, "st%x/r%03x_%02x.dat", pG->stage_no, pG->room_id, no);
    u->pData = DC.setData(name);
    if (u->pData == 0) {
        return 0;
    }
    u->flags |= 1;
    u->no = no;
    return 1;
}

void cBlock::checkCommand()
{
    cBlockUnit* u;
    u32 i;

    if (noMemCtrl == 0 && pData == 0) {
        return;
    }
    for (i = 0; i < nBlock; i++) {
        u = getUnitPtr(i);
        if (u->flags & 1) {
            switch (u->command) {
            case BLOCK_CMD_NONE:
                break;
            case BLOCK_CMD_MRAM_LOAD:
                u->setBlockLoadToMram();
                break;
            case BLOCK_CMD_ARAM_LOAD:
                u->setBlockLoadToAram();
                break;
            case BLOCK_CMD_DELETE:
                u->setBlockDelete();
                break;
            }
        }
    }
}

void cBlock::checkCondition()
{
    cBlockUnit* u;
    int ok;
    int r;
    u32 i;

    if (noMemCtrl == 0 && pData == 0) {
        return;
    }
    if (stopFlagSet == 1) {
        pG->flags_170 = stopFlag;
    }
    stopFlagSet = 0;
    ok = 1;
    for (i = 0; i < nBlock; i++) {
        u = getUnitPtr(i);
        if (u->flags & 1) {
            r = 1;
            switch (u->state) {
            case BLOCK_MRAM_LOAD:
                r = u->checkBlockLoadToMram();
                break;
            case BLOCK_ARAM_LOAD_SET:
                r = u->checkBlockLoadToAramSet();
                break;
            case BLOCK_ARAM_LOAD:
                r = u->checkBlockLoadToAram();
                break;
            case BLOCK_DELETE:
                r = u->checkBlockDelete();
                break;
            case BLOCK_NO_DATA:
            case BLOCK_MRAM_LOAD_SET:
            case BLOCK_CREATE:
            case BLOCK_ARAM_OK:
                break;
            }
            if (r == 0) {
                ok = 0;
            }
        }
    }
    if (ok == 1) {
        if (Block.noMemCtrl == 0) {
            checkBlockMemSort();
        }
        for (i = 0; i < nBlock; i++) {
            u = getUnitPtr(i);
            if ((u->flags & 1) && u->state == BLOCK_MRAM_LOAD_SET) {
                u->checkBlockLoadToMramSet();
            }
        }
    }
    if (stopFlagSet == 1) {
        BitSet(stopFlag, pG->flags_170);
        pG->flags_170 = 0xFFFFFFFF;
    }
}

void cBlock::checkBlockMemSort()
{
    cBlockUnit* u;
    cBlockUnit* tbl[nBlock];
    u8 cnt = 0;
    int i;
    int j;
    u8* addr;

    for (i = 0; i < nBlock; i++) {
        u = getUnitPtr(i);
        if (isFree(u->flags)) {
            continue;
        }
        if (u->state != BLOCK_CREATE) {
            continue;
        }
        tbl[cnt] = u;
        cnt++;
    }
    if (cnt != 0) {
        for (i = 0; i < cnt - 1; i++) {
            for (j = i; j < cnt; j++) {
                if ((u32) tbl[i]->pData->addr > (u32) tbl[j]->pData->addr) {
                    u = tbl[j];
                    tbl[j] = tbl[i];
                    tbl[i] = u;
                }
            }
        }
        addr = (u8*) memTop;
        for (i = 0; i < cnt; i++) {
            u = tbl[i];
            if ((u32) addr < (u32) u->pData->addr) {
                u->moveBlockData(addr);
            }
            addr += tbl[i]->pData->size;
        }
        memCur = addr;
    } else {
        memCur = memTop;
    }
}

void* cBlock::getBlockMemFree(u32 size)
{
    u8* p = (u8*) memCur;

    if (p + size > (u8*) memEnd) {
        pLog->err(0, 0, "cBlock::getBlockMemFree: error");
        return 0;
    }
    memCur = p + size;
    return p;
}

void cBlock::dispDebugInfo()
{
    cBlockUnit* u;
    int y;
    u32 i;

    if (pG->debug_mode != 0x12) {
        return;
    }
    const char* cmdName[4] = {"NONE", "MRAM_LOAD", "ARAM_LOAD", "DELETE"};
    const char* stateName[8] = {"NO_DATA", "MRAM_LOAD_SET", "MRAM_LOAD", "CREATE", "ARAM_LOAD_SET", "ARAM_LOAD", "ARAM_OK", "DELETE"};
    const char* dataCmdName[5] = {"NONE", "MRAM_LOAD", "ARAM_LOAD", "CLEAR_DATA", "DEL_DATA"};
    const char* dataCondName[9] = {"NO_DATA", "MRAM_LOAD", "MRAM_OK", "ARAM_LOAD", "ARAM_OK", "ARAM_TO_MRAM", "MRAM_TO_ARAM", "ARAM_TO_ARAM", "MRAM_TO_MRAM"};

    eprintf(40, 30, 0, 18, "[BLOCK INFO]  (now area:%d) stop_flg %08X", pG->area_no, pG->flags_170);
    eprintf(228, 58, 0, 18, "ADDR     DEST     ARG      SIZE");
    y = 58;
    for (i = 0; i < nBlock; i++) {
        u = getUnitPtr(i);
        if (isFree(u->flags)) {
            continue;
        }
        y += 16;
        eprintf(20, y, 4, 18, "%2d %9s %13s", i, cmdName[u->command], stateName[u->state]);
        y += 16;
        eprintf(20, y, 7, 18, "   %9s %12s %8X %8X %8X %X", dataCmdName[u->pData->getCommand()],
                dataCondName[u->pData->getCondition()], u->pData->getAddr(), u->pData->getDest(), u->pData->getArg(),
                u->pData->getSize());
    }
}
