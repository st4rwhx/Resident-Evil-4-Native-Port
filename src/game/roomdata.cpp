#include "types.h"
#include "atari.h"
#include "global.h"
#include "room_data.h"
#include "main_mem.h"
#include "main_sub.h"
#include "scheduler.h"
#include "dvd.h"
#include "db_log.h"

extern "C" {
void* memcpy(void* dst, const void* src, unsigned int n);
}

#line 40 "D:/Bio4/Prog/roomdata.cpp"

RoomTblEntry St0_data_tbl[67] = {
    {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0},
    {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0},
    {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0},
    {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0},
    {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0},
    {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0},
    {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0},
    {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0},
    {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0},
    {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0},
    {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0}, {0, 0, 66, 0, 0},
    {0, 0, 66, 0, 0},
};

RoomTblEntry St1_data_tbl[33] = {
    {1, 0, 146, 0, 0}, {1, 0, 149, 0, 0}, {1, 0, 149, 0, 0}, {1, 0, 149, 0, 0}, {1, 0, 149, 0, 0}, {1, 0, 149, 0, 0},
    {1, 0, 149, 0, 0}, {1, 0, 149, 0, 0}, {1, 0, 149, 0, 0}, {1, 0, 149, 0, 0}, {1, 0, 149, 0, 0}, {1, 0, 149, 0, 0},
    {1, 0, 152, 0, 0}, {1, 0, 152, 0, 0}, {1, 0, 152, 0, 0}, {1, 0, 165, 0, 0}, {0, 0, 0, 0, 0}, {1, 0, 165, 0, 0},
    {1, 0, 165, 0, 0}, {1, 0, 165, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {1, 0, 165, 0, 0},
    {1, 0, 165, 0, 0}, {1, 0, 152, 0, 0}, {1, 0, 152, 0, 0}, {1, 0, 152, 0, 0}, {1, 0, 165, 0, 0}, {1, 0, 165, 0, 0},
    {1, 0, 165, 0, 0}, {1, 0, 165, 0, 0}, {0, 0, 146, 0, 0},
};

RoomTblEntry St2_data_tbl[46] = {
    {1, 0, 147, 0, 0}, {1, 0, 147, 0, 0}, {1, 0, 147, 0, 0}, {1, 0, 147, 0, 0}, {1, 0, 159, 0, 0}, {1, 0, 159, 0, 0},
    {1, 0, 159, 0, 0}, {1, 0, 147, 0, 0}, {1, 0, 147, 0, 0}, {1, 0, 159, 0, 0}, {1, 0, 159, 0, 0}, {1, 0, 159, 0, 0},
    {1, 0, 159, 0, 0}, {1, 0, 159, 0, 0}, {1, 0, 159, 0, 0}, {1, 0, 159, 0, 0}, {1, 0, 147, 0, 0}, {1, 0, 160, 0, 0},
    {1, 0, 160, 0, 0}, {1, 0, 160, 0, 0}, {1, 0, 160, 0, 0}, {1, 0, 160, 0, 0}, {1, 0, 160, 0, 0}, {1, 0, 160, 0, 0},
    {1, 0, 160, 0, 0}, {1, 0, 160, 0, 0}, {1, 0, 187, 0, 0}, {1, 0, 187, 0, 0}, {0, 0, 0, 0, 0}, {1, 0, 187, 0, 0},
    {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {1, 0, 187, 0, 0}, {1, 0, 187, 0, 0}, {1, 0, 147, 0, 0}, {1, 0, 187, 0, 0},
    {1, 0, 187, 0, 0}, {1, 0, 187, 0, 0}, {1, 0, 187, 0, 0}, {1, 0, 187, 0, 0}, {1, 0, 187, 0, 0}, {1, 0, 187, 0, 0},
    {1, 0, 187, 0, 0}, {1, 0, 187, 0, 0}, {1, 0, 243, 0, 0}, {0, 0, 0, 0, 0},
};

RoomTblEntry St3_data_tbl[52] = {
    {1, 0, 148, 0, 0}, {1, 0, 148, 0, 0}, {0, 0, 0, 0, 0}, {1, 0, 148, 0, 0}, {1, 0, 148, 0, 0}, {1, 0, 148, 0, 0},
    {1, 0, 148, 0, 0}, {1, 0, 148, 0, 0}, {1, 0, 148, 0, 0}, {1, 0, 148, 0, 0}, {1, 0, 148, 0, 0}, {1, 0, 148, 0, 0},
    {1, 0, 148, 0, 0}, {1, 0, 201, 0, 0}, {1, 0, 148, 0, 0}, {1, 0, 201, 0, 0}, {1, 0, 201, 0, 0}, {1, 0, 201, 0, 0},
    {1, 0, 201, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {1, 0, 202, 0, 0}, {1, 0, 202, 0, 0}, {1, 0, 202, 0, 0},
    {1, 0, 202, 0, 0}, {0, 0, 0, 0, 0}, {1, 0, 202, 0, 0}, {1, 0, 202, 0, 0}, {1, 0, 202, 0, 0}, {1, 0, 202, 0, 0},
    {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {1, 0, 203, 0, 0}, {1, 0, 203, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0}, {1, 0, 203, 0, 0}, {1, 0, 203, 0, 0}, {1, 0, 203, 0, 0}, {1, 0, 203, 0, 0}, {1, 0, 203, 0, 0},
    {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
    {1, 0, 203, 0, 0}, {1, 0, 203, 0, 0}, {1, 0, 203, 0, 0}, {1, 0, 203, 0, 0},
};

RoomTblEntry St4_data_tbl[18] = {
    {1, 0, 186, 0, 0}, {0, 0, 186, 0, 0}, {1, 0, 186, 0, 0}, {1, 0, 186, 0, 0}, {1, 0, 186, 0, 0}, {1, 0, 186, 0, 0},
    {1, 0, 186, 0, 0}, {0, 0, 186, 0, 0}, {0, 0, 186, 0, 0}, {0, 0, 186, 0, 0}, {1, 0, 186, 0, 0}, {1, 0, 186, 0, 0},
    {1, 0, 186, 0, 0}, {1, 0, 186, 0, 0}, {1, 0, 186, 0, 0}, {1, 0, 186, 0, 0}, {1, 0, 186, 0, 0}, {1, 0, 186, 0, 0},
};

static StageTbl Room_data_tbl[10] = {
    {St0_data_tbl, 67, 0}, {St1_data_tbl, 33, 0}, {St2_data_tbl, 46, 0}, {St3_data_tbl, 52, 0}, {St4_data_tbl, 18, 0},
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
};

static inline void U32Set(u32& d, u32 v) { d = v; }

cRoomData RoomData;

void cRoomData::init()
{
    u32 stage;
    int i;
    u32 ofs;
    u8* rec;

    pModule = 0;
    pBss = 0;
    x1C = 0;
    total = 0;
    for (stage = 0; stage <= 9; stage++) {
        if (Room_data_tbl[stage].tbl != 0) {
            total += Room_data_tbl[stage].num;
        }
    }
    num = 0;
    for (stage = 0; stage <= 9; stage++) {
        for (i = 0; checkRoomRange(stage, i) == 1; i++) {
            if (Room_data_tbl[stage].tbl[i].stat == 1) {
                num++;
            }
        }
    }
#line 306
    pSaveBuf = (RoomSaveHdr*) MEM_CALLOC(num * sizeof(RoomSave) + sizeof(RoomSaveHdr), 1, 13);
    pSaveBuf->size = num * sizeof(RoomSave) + sizeof(RoomSaveHdr);
    U32Set(pSaveBuf->num, num);
    pSave = (u8*) pSaveBuf + sizeof(RoomSaveHdr);
    stage = 0;
    ofs = 0;
    for (; stage <= 9; stage++) {
        for (i = 0; checkRoomRange(stage, i) == 1; i++) {
            if (Room_data_tbl[stage].tbl[i].stat == 1) {
                *(u8*) (ofs + (u32) pSave) = stage;
                rec = (u8*) (ofs + (u32) pSave);
                rec[1] = i;
                ofs += sizeof(RoomSave);
            }
        }
    }
}

void cRoomData::initRoomSet()
{
}

void cRoomData::save(void* dst)
{
    RoomSaveHdr* h = (RoomSaveHdr*) dst;

    memcpy(h, pSaveBuf, num * sizeof(RoomSave) + sizeof(RoomSaveHdr));
}

void cRoomData::load(void* src)
{
    RoomSaveHdr* h = (RoomSaveHdr*) src;
    RoomSave* rec = (RoomSave*) ((u8*) src + sizeof(RoomSaveHdr));
    RoomSave* dst;
    u32 j;
    int i;

    for (j = 0; j < h->num; j++, rec++) {
        for (i = 0; i < num; i++) {
            dst = (RoomSave*) (i * sizeof(RoomSave) + (u32) pSave);
            if (rec->id == dst->id) {
                *dst = *rec;
                break;
            }
        }
    }
}

void cRoomData::clear(void* src)
{
    RoomSaveHdr* h = (RoomSaveHdr*) src;
    RoomSave* rec = (RoomSave*) ((u8*) src + sizeof(RoomSaveHdr));
    RoomSave* dst;
    u32 j;
    int i;
    u16 id;

    for (j = 0; j < h->num; j++, rec++) {
        // The loop with the call in its body is only rotated (entry test + bottom test) when written
        // as an explicitly guarded do/while; a `for` keeps the initial jump to the test.
        i = 0;
        if (i < num) {
            do {
                dst = (RoomSave*) (i * sizeof(RoomSave) + (u32) pSave);
                id = rec->id;
                if (id == dst->id) {
                    memclr_asm(dst, sizeof(RoomSave));
                    ((RoomSave*) (i * sizeof(RoomSave) + (u32) pSave))->id = id;
                    break;
                }
                i++;
            } while (i < num);
        }
    }
}

u8* cRoomData::getRoomSavePtr(u16 room)
{
    u32 stage = room >> 8;
    u8 no = room;
    u32 s;
    int i;
    int k;
    StageTbl* p;

    if (!checkRoomRange(stage, no)) {
        return 0;
    }
    if (Room_data_tbl[stage].tbl[no].stat == 0) {
        return 0;
    }
    k = 0;
    p = Room_data_tbl;
    for (s = 0; s <= 9; s++, p++) {
        for (i = 0; checkRoomRange(s, i) == 1; i++) {
            if (p->tbl[i].stat == 1) {
                if (stage == s && no == i) {
                    return pSave + k * sizeof(RoomSave);
                }
                k++;
            }
        }
    }
    return 0;
}

void cRoomData::execInitFunc(u16 room)
{
    u8 no = room;
    u32 stage = room >> 8;
    void (*func)();

    if (checkRoomRange(stage, no) == 1) {
        func = Room_data_tbl[stage].tbl[no].init;
        if (func != 0) {
            func();
        }
    }
}

void cRoomData::execMainFunc(u16 room)
{
    u8 no = room;
    u32 stage = room >> 8;
    void (*func)();

    if (checkRoomRange(stage, no) == 1) {
        func = Room_data_tbl[stage].tbl[no].main;
        if (func != 0) {
            func();
        }
    }
}

int cRoomData::checkRoomRange(u8 stage, u8 no)
{
    if (stage <= 9 && no < Room_data_tbl[stage].num && Room_data_tbl[stage].tbl != 0) {
        return 1;
    }
    return 0;
}

int cRoomData::checkRelRead(u16 room)
{
    u8 no = room;
    u32 stage = room >> 8;
    u16 rel;

    if (checkRoomRange(stage, no) == 1) {
        rel = Room_data_tbl[stage].tbl[no].rel_no;
        if (rel != 0 && rel != x1C) {
            return 1;
        }
    }
    return 0;
}

void cRoomData::linkRelData(u16 room)
{
    u8 no = room;
    u32 stage = room >> 8;
    int id;
    int ret;

    if (checkRoomRange(stage, no) != 1) {
        return;
    }
    // The target stores x1C only after the zero test (`sth` behind the `beq`). Left: the promoted
    // value reaches the compare and the DvdRead argument as `clrlwi r3,r0,16` in the target, ours
    // folds the extension (`mr`) and compares the halfword register (#2 family).
    if (Room_data_tbl[stage].tbl[no].rel_no == 0) {
        return;
    }
    x1C = Room_data_tbl[stage].tbl[no].rel_no;
#line 484
    id = DvdRead(x1C, 0, 0, 0, 0, 0x104, __FILE__, __LINE__);
    while ((ret = Dvd.ReadCheck(id, 0, 0, (void**) &pModule)) != 1) {
        if (ret < 0) {
            pLog->err(0, 0, "cRoomData::readRelData(): RelDataReadError! %s", FileTbl[x1C]);
            x1C = 0;
            pModule = 0;
            return;
        }
        TaskSleep(1);
    }
    BitOff16(flag, 1);
    if (pModule->bssSize == 0) {
        pBss = 0;
    } else {
#line 503
        pBss = MEM_ALLOC(pModule->bssSize, 1, 13);
        pBssBak = MEM_ALLOC(pModule->bssSize, 1, 13);
    }
    DLL_Link(pModule, pBss);
    pModule->prolog();
}

void cRoomData::stopRelData()
{
    if ((flag & 1) == 0 && pModule != 0) {
        flag |= 1;
        if (pBss != 0) {
            memcpy(pBssBak, pBss, pModule->bssSize);
        }
        DLL_Unlink(pModule);
    }
}

void cRoomData::restartRelData()
{
    if ((flag & 1) && pModule != 0) {
        BitOff16(flag, 1);
        DLL_Link(pModule, pBss);
        if (pBss != 0) {
            memcpy(pBss, pBssBak, pModule->bssSize);
        }
    }
}

int cRoomData::checkPassed(u16 room, int bit)
{
    u8* p = getRoomSavePtr(room);

    if (p == 0) {
        return 0;
    }
    return p[2] & (0x80 >> bit);
}

void cRoomData::setPassed(u16 room, int bit)
{
    u8* p = getRoomSavePtr(room);

    if (p != 0) {
        p[2] |= 0x80 >> bit;
    }
}
