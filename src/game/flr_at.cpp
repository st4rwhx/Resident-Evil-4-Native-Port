#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flr_at.h"
#include "area.h"
#include "global.h"
#include "db_log.h"
#include "main_mem.h"

void* GetDataExt(void* arc, const char* tag, int no);   // game/read.cpp
extern "C" int strcmp(const char* a, const char* b);

FlrSys FlrAt_sys;
FlrSys* pFlrSys;

// Reference store: a scalar (non-struct) MEM, so the following pFlrSys load is not hoisted
// above it and is reloaded (light.cpp PSet).
static inline void PSet(void*& d, void* v) { d = v; }

void FlrAtInit()
{
    FlrAtHead* p;

    pFlrSys = &FlrAt_sys;
    memclr_asm(&FlrAt_sys, sizeof(FlrAt_sys));
    p = (FlrAtHead*) GetDataExt(pG->pRoomArc, "FSE", 0);
    if (p == 0) {
        return;
    }
    if (strcmp((char*) p, "FSE") != 0) {
        return;
    }
    if (p->version != 0x103) {
        pLog->warn(0, 0, "FlrAt DATA IS OLD VERSION");
        return;
    }
    PSet(pFlrSys->pData, p);
    pFlrSys->pList = (FlrAt*) (p + 1);
}

FlrAt* FlrAtCheck(int type, Vec* pos, int flag)
{
    Vec p;
    FlrAt* at;
    u32 i;
    int hit = 0;

    if (pG->x20 != 3) {
        return 0;
    }
    if (pG->flags_500C & 0x10000000) {
        return 0;
    }
    if (pFlrSys == 0) {
        return 0;
    }
    if (pFlrSys->pData == 0) {
        return 0;
    }
    p.x = pos->x;
    p.y = pos->y + 300.0f;
    p.z = pos->z;
    for (i = 0; i < ((FlrAtHead*) pFlrSys->pData)->num; i++) {
        at = &pFlrSys->pList[i];
        if ((at->x0 & 1) == 0) {
            continue;
        }
        if (at->group != pFlrSys->group && pFlrSys->group != 0xFF) {
            continue;
        }
        if (at->type != type) {
            continue;
        }
        if (AreaHitCheck(at->area, &p) != 1) {
            continue;
        }
        if (type != 0) {
            hit = 1;
        } else if (at->x46[1] & flag) {
            hit = 1;
        }
        if (hit == 1) {
            return at;
        }
    }
    return 0;
}

// Dead-stripped by the original linker (only their strings survive in .rodata).
static int FlrAtSetEnable(int no)
{
    if (pFlrSys->pData == 0) {
        pLog->err(0, 0, "FlrAtSetEnable() : AT DATA NOT FOUND");
        return 0;
    }
    pFlrSys->pList[no].x0 |= 1;
    return 1;
}

static int FlrAtSetDisable(int no)
{
    if (pFlrSys->pData == 0) {
        pLog->err(0, 0, "FlrAtSetDisable() : AT DATA NOT FOUND");
        return 0;
    }
    pFlrSys->pList[no].x0 &= ~1;
    return 1;
}

int FlrAtSetDefVal(u32 no, u8 a, u8 b)
{
    if (no != 0xFF && no > 0x3F) {
        pLog->err(0, 0, "FlrAt : group %d Illegal No.", no);
        return 0;
    }
    no = no == 0xFF ? 0x40 : no;
    pFlrSys->foot_se[no] = a;
    pFlrSys->foot_esp[no] = b;
    return 1;
}
