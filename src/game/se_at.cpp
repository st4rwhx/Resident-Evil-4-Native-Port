#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "snd.h"
#include "global.h"
#include "db_log.h"
#include "rnd.h"

void* GetDataExt(void* arc, const char* tag, int no);   // game/read.cpp
extern int DebugMenuSelected;                            // game/db_menu.cpp

void SeAtInit()
{
    SndWork* s = &Snd;

    s->se_at = (SeAtHead*) GetDataExt(pG->pRoomArc, "ESE", 0);
    if (s->se_at == 0) {
        return;
    }
    if (s->se_at->version != 0x100) {
        pLog->err(0, 0, "SeAt DATA IS OLD VERSION");
        s->se_at = 0;
        s->se_at_list = 0;
        return;
    }
    s->se_at_list = (SeAt*) (s->se_at + 1);
}

void SeAtCheck()
{
    SndWork* s = &Snd;
    SeAt* at;
    Vec* pos;
    int i;

    if (pG->flags_170 & 0x800) {
        return;
    }
    if ((s32) pG->flags_60 < 0 && DebugMenuSelected != 0x18) {
        return;
    }
    if (pG->x20 != 3) {
        return;
    }
    if (s->se_at == 0) {
        return;
    }
    for (i = 0; i < s->se_at->num; i++) {
        at = &s->se_at_list[i];
        if ((at->flags & 1) == 0) {
            continue;
        }
        if (at->repeat < 0) {
            continue;
        }
        if (at->wait == 0) {
            if (at->cnt == 0) {
                pos = &at->pos;
                if (at->flags2 & 1) {
                    pos = 0;
                }
                if (SndCall(at->blk, at->se_no, pos, 0, 0, 0) == 0) {
                    continue;
                }
                if (at->repeat == 1) {
                    at->repeat = -1;
                    continue;
                }
                if (at->repeat != 0) {
                    at->repeat--;
                }
                if (at->interval == 0) {
                    at->cnt = at->rnd_base + Rnd() % at->rnd_range;
                } else {
                    at->cnt = at->interval;
                }
            } else {
                at->cnt--;
            }
        } else {
            at->wait--;
        }
    }
}

int SeAtSetOnOff(int no, int on)
{
    SeAt* at = GetSeAtPtr(no);

    if (at == 0) {
        if (on == 1) {
            pLog->err(0, 0, "SeAtSetEnable() : AT DATA NOT FOUND");
        } else {
            pLog->err(0, 0, "SeAtSetDisable() : AT DATA NOT FOUND");
        }
        return 0;
    }
    if (on == 1) {
        at->flags |= 1;
    } else {
        at->flags &= ~1;
    }
    return 1;
}

SeAt* GetSeAtPtr(int no)
{
    SeAt* at;
    u32 i;

    if (Snd.se_at == 0) {
        return 0;
    }
    for (i = 0; i < Snd.se_at->num; i++) {
        at = &Snd.se_at_list[i];
        if (at->no == no) {
            return at;
        }
    }
    return 0;
}

u32 SeAtSndCall(int no)
{
    SeAt* at = GetSeAtPtr(no);

    if (at != 0) {
        if (at->flags2 & 1) {
            return SndCall(at->blk, at->se_no, 0, 0, 0, 0);
        }
        return SndCall(at->blk, at->se_no, &at->pos, 0, 0, 0);
    }
    pLog->err(0, 0, "SeAtSeCall() : AT DATA NOT FOUND");
    return 0;
}

