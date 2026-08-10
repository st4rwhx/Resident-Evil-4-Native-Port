#include "types.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "main_mem.h"
#include "main.h"
#include "mes.h"
#include "room_jmp.h"

// Room jump tool work (0x38 bytes)
struct test {
    s8 state;      // 0x00  tbl index
    s8 mode;       // 0x01  cursor line: 0 stage, 1 room, 2 point
    s8 stage;      // 0x02
    s8 room[12];   // 0x03  selected room index per stage
    s8 point;      // 0x0F
    u8 flag;       // 0x10  1 = a jump was executed
    u8 pad_11[3];
    u32 stop_bak;  // 0x14  pG->flags_170
    u8 pad_18[0x38 - 0x18];
};

extern "C" {
void roomJumpInit(test* w);
void roomJumpMove(test* w);
void roomJumpExec(test* w);
void roomJumpExit(test* w);
}

// The original stores GlobalWork fields through references: GCC then reloads pG after every store.
static inline void U8Set(u8& d, u8 v) { d = v; }
static inline void S8Set(s8& d, s8 v) { d = v; }
static inline void U16Set(u16& d, u16 v) { d = v; }
static inline void U32Set(u32& d, u32 v) { d = v; }

// Stage offset table right after the count; as an inline the base stays a pointer register (lwzx).
static inline u32* ofsTbl(u32* tbl)
{
    return tbl + 1;
}

cRoomJmp* pRj;

void CRoomInfo::setNextPos()
{
    if (flags & 1) {
        FSet(pG->next_pos.x, pos.x);
        FSet(pG->next_pos.y, pos.y);
        FSet(pG->next_pos.z, pos.z);
        FSet(pG->next_angle, angle);
    } else {
        FSet(pG->next_pos.x, 0.0f);
        FSet(pG->next_pos.y, 0.0f);
        FSet(pG->next_pos.z, 0.0f);
        FSet(pG->next_angle, 0.0f);
    }
    U16Set(pG->room_id_prev, pG->room_id);
    U8Set(pG->x4FA2, pG->x4F9E);
    U16Set(pG->next_room, room_id);
    U8Set(pG->next_point, 0);
}

cRoomJmp::cRoomJmp(void* p)
{
    u32 stage;
    int i;
    CRoomInfo* info;

    tbl = (u32*) p;
    for (stage = 0; stage < tbl[0]; stage++) {
        if (getIndexNum(stage) == 0) {
            continue;
        }
        for (i = 0; i < getIndexNum(stage); i++) {
            info = getRoomInfo(stage, i);
            if (info == 0) {
                continue;
            }
            if ((u32) info->name >= 0x80000000 && (u32) info->name <= 0x82FFFFFF) {
                return;
            }
            info->name = (char*) ((u32) tbl + (u32) info->name);
            info->scr = (char*) ((u32) tbl + (u32) info->scr);
            info->soft = (char*) ((u32) tbl + (u32) info->soft);
        }
    }
}

s8 cRoomJmp::getIndexNum(s8 stage)
{
    u32* p = tbl;
    u32 ofs = ofsTbl(p)[stage];

    if (ofs == 0) {
        return 0;
    }
    return *((s8*) p + ofs + 3);
}

s8 cRoomJmp::getPointNum(s8 stage, s8 room)
{
    int count = 1;
    u8 idx = getRoomIdx(stage, room);
    u32 n = getIndexNum(stage);

    do {
        idx = (n + idx + 1) % n;
        if (getRoomInfo(stage, idx)->room != room) {
            break;
        }
        count++;
    } while (1);
    return count;
}

CRoomInfo* cRoomJmp::getRoomInfo(u8 stage, u8 idx)
{
    u32* p = tbl;
    u32 ofs;
    u32 n;
    u32 base;

    if (stage >= p[0]) {
        return 0;
    }
    // COMPILER-DIFF: tie. The loop notes double the weight of this `ofs` set, so local-alloc
    // allocates ofs before n (ofs r0, n r11) and global-alloc can give base the freed r0.
    do { ofs = (p + 1)[stage]; } while (0);
    n = *(u32*) ((u8*) p + ofs);
    base = (u32) p + ofs;
    if (idx >= n) {
        return 0;
    }
    {
        u32 o = idx * sizeof(CRoomInfo) + 4;
        return (CRoomInfo*) (base + o);
    }
}

u8 cRoomJmp::getRoomIdx(u8 stage, u8 room)
{
    int i;
    u8 idx;

    for (i = 0; i < getIndexNum(stage); i++) {
        idx = i;
        if (getRoomInfo(stage, idx)->room == room) {
            return idx;
        }
    }
    return 0;
}

void cRoomJmp::setNextPos(u8 stage, u8 room)
{
    getRoomInfo(stage, getRoomIdx(stage, room))->setNextPos();
}

s8 cRoomJmp::getNextStageNo(s8 stage, int dir)
{
    u32* p = tbl;
    u32 n = p[0];

    do {
        stage = (n + stage + dir) % n;
    } while (ofsTbl(p)[stage] == 0);
    return stage;
}

s8 cRoomJmp::getNextRoomNo(s8 stage, s8 idx, int dir)
{
    u32 n;
    CRoomInfo* cur;
    CRoomInfo* info;
    s8 next;

    if (dir == 0) {
        return idx;
    }
    n = getIndexNum(stage);
    cur = getRoomInfo(stage, idx);
    for (;;) {
        idx = (n + idx + dir) % n;
        info = getRoomInfo(stage, idx);
        if (cur == info) {
            return idx;
        }
        if (cur->room_id != info->room_id) {
            break;
        }
    }
    if (dir >= 0) {
        return idx;
    }
    cur = info;
    for (;;) {
        next = (n + idx - 1) % n;
        info = getRoomInfo(stage, next);
        if (cur == info) {
            return idx;
        }
        if (info->room_id != cur->room_id) {
            return idx;
        }
        idx = next;
    }
}

s8 cRoomJmp::getNextPointNo(s8 stage, s8 room, s8 point, int dir)
{
    u32 n = getIndexNum(stage);
    s8 idx = getRoomIdx(stage, room) + point;
    u16 room_id = (stage << 8) | room;
    s8 next;

    next = (n + idx + dir) % n;
    if (getRoomInfo(stage, next)->room_id == room_id) {
        idx = next;
    }
    return idx - getRoomIdx(stage, room);
}

int cRoomJmp::checkRoomNo(s8 stage, int idx)
{
    if ((u8) stage >= tbl[0] || getIndexNum(stage) == 0 || getRoomInfo(stage, idx) == 0) {
        return -1;
    }
    return idx;
}

void RoomJump()
{
    static test test;
    static void (*tbl[])(struct test*) = {roomJumpInit, roomJumpMove, roomJumpExec, roomJumpExit};
    struct test* w = &test;

    memclr_asm(w, sizeof(test));
    for (;;) {
        tbl[w->state](w);
        TaskSleep(1);
    }
}

void roomJumpInit(test* w)
{
    w->state++;
    U32Set(w->stop_bak, pG->flags_170);
    BitOn(pG->flags_170, 0xFFFFBFFF);
    pRj = new cRoomJmp(roomInfoAddr);
    w->stage = pG->stage_no;
    w->room[w->stage] = pRj->getRoomIdx(pG->stage_no, pG->room_no);
    w->point = pG->x4F9F;
    w->flag = 0;
}

void roomJumpMove(test* w)
{
    JOY* joy = GetBugCheckController();
    CRoomInfo* info;
    int no;
    s8 room;
    int pt;

    if (joy->rep & 0x40004) {
        w->mode++;
    }
    if (joy->rep & 0x80008) {
        w->mode--;
    }
    w->mode = (w->mode < 0) ? 2 : ((w->mode > 2) ? 0 : w->mode);
    info = pRj->getRoomInfo(w->stage, w->room[w->stage] + w->point);
    eprintf(0xD8, 0x38, 0, 0, "STAGE = %d", w->stage);
    eprintf(0xD8, 0x46, 0, 0, "ROOM  = %02x", info->room);
    eprintf(0xD8, 0x54, 0, 0, "POINT = %d", w->point);
    eprintf(0xD8, 0x2A, 4, 0, "%s", info->name);
    if (info->soft[0] != 0) {
        eprintf(0xD8, 0x1C, 0, 0, "     SOFT(%s)", info->soft);
    }
    if (info->scr[0] != 0) {
        eprintf(0xD8, 0xE, 0, 0, "     SCR(%s)", info->scr);
    }
    eprintf(0xD0, (w->mode + 4) * 0xE, 0, 0, ">");
    if (joy->trg & 0x100) {
        w->state = 2;
    }
    if (joy->trg & 0x200) {
        w->state = 3;
    }
    no = w->mode;
    switch (no) {
    case 0:
        if (joy->rep2 & 0x20002) {
            w->stage = pRj->getNextStageNo(w->stage, 1);
            w->point = 0;
        }
        if (joy->rep2 & 0x10001) {
            w->stage = pRj->getNextStageNo(w->stage, -1);
            w->point = 0;
        }
        no = pRj->checkRoomNo(w->stage, w->room[w->stage]);
        if (no >= 0) {
            w->room[w->stage] = no;
        }
        break;
    case 1:
        if (joy->rep2 & 0x20002) {
            w->room[w->stage] = pRj->getNextRoomNo(w->stage, w->room[w->stage], 1);
            w->point = 0;
        }
        if (joy->rep2 & 0x10001) {
            w->room[w->stage] = pRj->getNextRoomNo(w->stage, w->room[w->stage], -1);
            w->point = 0;
        }
        break;
    case 2:
        room = pRj->getRoomInfo(w->stage, w->room[w->stage])->room;
        if (joy->rep2 & 0x20002) {
            w->point = pRj->getNextPointNo(w->stage, room, w->point, 1);
        }
        if (joy->rep2 & 0x10001) {
            w->point = pRj->getNextPointNo(w->stage, room, w->point, -1);
        }
        // Nested s8 ternary through an s8& setter: the value is a QImode temp (a promoted int local
        // needs two insns for the byte load), so jump1 hoists the `li 0` above the compare, the temp
        // conflicts with r3 and takes r9, and case 1's `stb r9` is cross-jumped into the final store.
        pt = w->point;
        S8Set(w->point, (pt < 0) ? pRj->getPointNum(w->stage, room) - 1
                                 : ((pt > pRj->getPointNum(w->stage, room) - 1) ? (s8) 0 : w->point));
        break;
    }
}

void roomJumpExec(test* w)
{
    int i;

    w->state++;
    BitSet(pG->flags_170, 0xFFFFFFFF);
    BitOn(pG->flags_68, 0x80000000);
    pRj->getRoomInfo(w->stage, w->room[w->stage] + w->point)->setNextPos();
    pG->x4F9F = w->point;
    cMes.roomInit();
    {
        // A pointer local for the loop keeps &cMes in one register (lis in a callee-saved one).
        MessageControl* mes = &cMes;
        for (i = 0; i <= 0xF; i++) {
            mes->Delete(i);
        }
    }
    U16Set(pG->pl_life, pG->pl_life_max);
    U16Set(pG->x4F90, 0);
    w->flag = 1;
}

void roomJumpExit(test* w)
{
    delete pRj;
    if (w->flag == 1) {
        pG->x20 = 4;
        pG->x21 = 0;
        pG->x22 = 0;
        pG->x23 = 0;
        pG->flags_54 &= ~0x40;
    }
    BitSet(pG->flags_170, w->stop_bak);
    BitOff(pG->flags_60, 0x80000000);
    TaskExit();
}

void GetNextPos(u8 stage, u8 room)
{
    pRj = new cRoomJmp(roomInfoAddr);
    pRj->setNextPos(stage, room);
    delete pRj;
}
