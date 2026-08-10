#include "types.h"
#include "vec.h"
#include "global.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "card.h"
#include "room_data.h"
#include "mes.h"
#include "main_mem.h"
#include "scheduler.h"
#include "math_sub.h"
#include "id_sys.h"
#include "etc_model.h"

#include "dvd.h"

// game/sce_com.cpp: per-scenario free counters (pG + 0x51E8)
extern "C" int GetFree(int no);
extern "C" void SetFree(int no, int val);

// game/merchant.cpp
struct MerchantData;
extern MerchantData merchantData;
extern u16 stock_1st_mission[];
extern "C" void stockDataAdd(MerchantData* m, u16* stock);

extern "C" {
int checkEmListNo(u16 room);
const char* getEmListName(u32 no);
const char* getEmListDbgName(int no);
int getEmListNum();
void StageSet();
void readEmList(int mode);
int checkSubMissionTarget(int stage, int no);
void subMissionSt1();
void subMissionSt2();
void subMissionSt3();
void SubMissionCheck();
}

static inline int emListVillage(int room)
{
    switch (room) {
    case 0x200:
    case 0x201:
    case 0x202:
    case 0x203:
    case 0x204:
    case 0x207:
    case 0x208:
        if (!(pG->flags_51C0 & 0x40000)) {
            return 2;
        }
    }
    return 3;
}

// Enemy list (ESL file) number for a room (stage << 8 | room), -1 = none.
int checkEmListNo(u16 room)
{
    int stage = room >> 8;
    u32 flags = pG->flags_54;

    if ((s32) flags < 0) {
        return 8;
    }
    if (flags & 0x40000000) {
        switch (room) {
        default:
            return 9;
        case 0x403:
        case 0x404:
            return 10;
        }
    }
    switch (stage) {
    case 1:
        if (room == 0x120) {
            return 0;
        }
        if (room == 0x10E) {
            if (flags & 0x2000) {
                return 1;
            }
            return -1;
        }
        return room > 0x10B;
    case 2:
        if (room == 0x22B) {
            return -1;
        }
        if (room == 0x22C) {
            return -1;
        }
        if (room == 0x22D) {
            return -1;
        }
        if (room == 0x200) {
            if (!(pG->flags_51C0 & 0x800000)) {
                return 1;
            }
            return 2;
        }
        if (room > 0x219) {
            if (room == 0x222) {
                return 4;
            }
            return 5;
        }
        if (room > 0x210) {
            return 4;
        }
        if (pG->flags_51C0 & 0x10000000) {
            return 4;
        }
        return emListVillage(room);
    case 3:
        if (room <= 0x314) {
            return 6;
        }
        return 7;
    case 4:
        if (room > 0x404) {
            return 8;
        }
        switch (room) {
        case 0x403:
        case 0x404:
            break;
        default:
            return 9;
        }
        return 10;
    }
    return -1;
}

static const char* emlist_name[11] = {
    "etc/emleon00.esl", "etc/emleon01.esl", "etc/emleon02.esl", "etc/emleon03.esl",
    "etc/emleon04.esl", "etc/emleon05.esl", "etc/emleon06.esl", "etc/emleon07.esl",
    "etc/omake00.esl",  "etc/omake01.esl",  "etc/omake02.esl",
};

static const char* emlist_dbg_name[11] = {
    "1st-1", "1st-2", "2st-1", "2st-2", "2st-3", "2st-4", "3st-1", "3st-2", "ada", "etc", "etc2",
};

const char* getEmListName(u32 no)
{
    const char* name;

    if (no <= 10) {
        name = emlist_name[no];
    } else {
        name = "...no string";
    }
    return name;
}

const char* getEmListDbgName(int no)
{
    const char* name;

    if (no >= 0) {
        if ((u32) no <= 10) {
            name = emlist_dbg_name[no];
        } else {
            name = "...no string";
        }
        return name;
    }
    return "?????";
}

int getEmListNum()
{
    return 11;
}

// Stage change: reload the stage data (heap 2) and link the room's relocatable data (heap 3).
void StageSet()
{
    GlobalWork* g = pG;
    u32 flags = g->flags_54;
    int reload = 0;
    int relink = 0;

    if (flags & 0x2000) {
        reload = 1;
    } else if (flags & 0x100) {
        reload = 1;
    } else if (flags & 0x80000) {
        reload = 1;
    } else if (g->stage_prev != g->stage_no) {
        reload = 1;
    }
    if (reload == 1) {
        RoomData.stopRelData();
        MemReplaceHeap(1, 2);
        MemSetCurrentHeap(2);
        cMes.stageInit();
        if (pG->stage_no == 1) {
            pG->flags_51BC |= 0x4;
        }
        TaskSleep(1);
    }
    if (RoomData.checkRelRead(G_ROOM_ID) == 1) {
        relink = 1;
    }
    if (relink == 1 || reload == 1) {
        RoomData.stopRelData();
        RoomData.x1C = 0;
        MemReplaceHeap(2, 3);
        MemSetCurrentHeap(3);
        RoomData.linkRelData(G_ROOM_ID);
    }
    readEmList(1);
}

#line 280 "D:/Bio4/Prog/stage.cpp"
void readEmList(int mode)
{
    const char* name = NULL;
    int result;
    int no;
    int req;

    no = checkEmListNo(G_ROOM_ID);
    if (no >= 0) {
        GlobalWork* g = pG;
        if (no > g->emlist_no || (g->flags_54 & 0x2000) || g->game_mode == 3 ||
            ((s32) g->flags_68 < 0 && g->emlist_no != no)) {
            name = getEmListName(no);
            pG->emlist_no = no;
        }
    }
    if (name != NULL) {
#line 296 "D:/Bio4/Prog/stage.cpp"
        req = DvdReadN(name, pG->emlist, 0, 0, 0, mode | 0x10, __FILE__, __LINE__);
        while (Dvd.ReadCheck(req, &result, 0, 0) != 1) {
            TaskSleep(1);
        }
        if (result == 0) {
            memclr_asm(pG->emlist, 0x2000);
        }
    }
}

// Sub-mission 1 (stage 1 blue medallions): room / room / item no per target.
struct SubMissionTarget {
    u16 room1;   // 0x00
    u16 room2;   // 0x02
    u16 no;      // 0x04
};

static SubMissionTarget st1_target_tbl[15] = {
    {0x103, 0x113, 0x0F}, {0x103, 0x113, 0x10}, {0x103, 0x113, 0x11}, {0x103, 0x113, 0x12},
    {0x103, 0x113, 0x14}, {0x103, 0x113, 0x13}, {0x103, 0x113, 0x15}, {0x108, 0x118, 0x04},
    {0x108, 0x118, 0x05}, {0x108, 0x118, 0x06}, {0x108, 0x118, 0x07}, {0x108, 0x118, 0x09},
    {0x108, 0x118, 0x08}, {0x108, 0x118, 0x0A}, {0x108, 0x118, 0x0B},
};

int checkSubMissionTarget(int stage, int no)
{
    u16* p1;
    u16* p2;

    if (stage != 1) {
        return 0;
    }
    p1 = GetEtcFlgPtr(st1_target_tbl[no].no, st1_target_tbl[no].room1);
    p2 = GetEtcFlgPtr(st1_target_tbl[no].no, st1_target_tbl[no].room2);
    if ((*p1 & 1) == 0) {
        if ((*p2 & 1) == 0) {
            return 1;
        }
    } else {
        return 0;
    }
    return 0;
}

void subMissionSt1()
{
    static EtcItem* pCoin = NULL;
    static s16 timer = 0;
    SubMissionTarget* tbl;
    SubMissionTarget* t;
    u16* p1;
    u16* p2;
    EtcItem* item;
    IdUnit* u;
    Vec scr;
    Vec pos;
    int digit[2];
    int count = 0;
    int i;
    int j;

    tbl = st1_target_tbl;
    {
    // `ofs = 0` between `tbl = ...` and `t = tbl` keeps cse from rewriting the lo_sum to set `t`
    // (cse_insn's (set REG0 REG1) swap needs the previous insn to be REG1's set), so the addi
    // result stays `tbl` and `t`/`t0` are copies. room1 is read through `(u32) t0 + ofs`: the
    // base is a register (base-first `lhzx`) that cse2 makes a copy of `t`.
    u32 ofs = 0;
    t = tbl;
    SubMissionTarget* t0 = t;
    do {
        p1 = GetEtcFlgPtr(t->no, t->room1);
        p2 = GetEtcFlgPtr(t->no, t->room2);
        if ((*p1 & 1) || (*p2 & 1)) {
            *p1 |= 1;
            *p2 |= 1;
        }
        if (*p1 & 1) {
            count++;
        }
        if (pG->x4 != 0) {
            if (G_ROOM_ID == ((SubMissionTarget*) ((u32) t0 + ofs))->room1 && !(*p1 & 1)) {
                if (getRoomEtcItem(t->no, &item, 1)) {
                    item->flags &= ~2;
                    pCoin = item;
                }
            }
            if (G_ROOM_ID == t->room2 && !(*p2 & 1)) {
                if (getRoomEtcItem(t->no, &item, 1)) {
                    item->flags &= ~2;
                    pCoin = item;
                }
            }
        }
        t++;
        ofs += sizeof(SubMissionTarget);
    } while (t <= &tbl[14]);
    }

    if (GetFree(0) != count) {
        timer = 150;
        SetFree(0, count);
        if (count == 10) {
            pG->flags_51BC |= 0x40000;
            timer = 450;
            stockDataAdd(&merchantData, stock_1st_mission);
            pG->flags_5014 &= ~0x40000;
        }
        if (count == 15) {
            pG->flags_51C0 |= 0x8000;
        }
        int n = 0;
        int base = 0;

        IdSys.kill(0xFF, 0x33);
        IdSys.set((void*) (pG->pArc->ofs_9C + (u32) pG->pArc), 0xFF, 0x33, 0x13, 5, 0);
        u = IdSys.unitPtr(0, 0x33);
        if (pCoin != NULL) {
            pos = pCoin->pos;
            GetScreenPos(&pos, &scr);
            scr.x = (scr.x - 256.0f) * 1.25f;
            scr.y = (scr.y - 224.0f) * -1.0714285f;
            u->scr = scr;
        }
        for (i = 0; i <= 1; i++) {
            switch (i) {
            case 0:
                n = count;
                base = 1;
                break;
            case 1:
                n = 15;
                base = 0x11;
                break;
            }
            for (j = 0; j <= 1; j++) {
                digit[j] = n % 10;
                n /= 10;
            }
            for (j = 0; j <= 1; j++) {
                u = IdSys.unitPtr(base + j, 0x33);
                u->flags |= 0x8;
                u->flags_7F |= 0x2;
                u->no = digit[j];
            }
        }
    }
    if (timer > 0) {
        timer--;
        if (timer == 0) {
            IdSys.kill(0xFF, 0x33);
        }
    }
}

void subMissionSt2()
{
}

void subMissionSt3()
{
}

void SubMissionCheck()
{
    switch (pG->stage_no) {
    case 1:
        subMissionSt1();
        break;
    case 2:
        subMissionSt2();
        break;
    case 3:
        subMissionSt3();
        break;
    }
}
