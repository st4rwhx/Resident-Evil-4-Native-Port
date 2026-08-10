// Sscrn/ss_cap: attache case (weapon case) selection screen of the sub screen DLL
// (D:/Bio4/Prog/ss_cap.cpp).
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
#include "db_log.h"
#include "sscrn.h"
#include "ss_main.h"

#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

// The widget classes (SsCapInit / SsCapMain / CapSelect) are declared in ss_main.h.

extern "C" {
void dispCapList(SUB_SCREEN* wk);
}

// item ids of the 4 x 6 case grid
u16 cap_id_tbl[24] = {
    0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xF0,
    0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xF1,
    0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2,
    0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF3,
};

int cap_read_req;

void SsCapInit::init(SUB_SCREEN* wk)
{
    if (wk->type == 0x100) {
        state = 2;
    } else {
        state = 0;
    }
}

void SsCapInit::move(SUB_SCREEN* wk)
{
    switch (state) {
    case 0:
        FadeSetW(0, 5, 0, 0);
        state++;
    case 1:
        if (Fade[0].flags & 1) {
            break;
        }
        state++;
    case 2:
        IdSys.dispSw(0x21, 0);
        IdSub.dispSw(2, 0);
        IdSub.dispSw(0, 0);
        sscrnDataFilename(wk, "ss_cap.dat");
#line 108 "D:/Bio4/Prog/ss_cap.cpp"
        cap_read_req = DVD_READ_N(wk->path, wk->pPzzl, 0, 0, 0, 0x10);
        if (cap_read_req <= 0) {
            break;
        }
        IdSubErase();
        IdNumErase();
        IdFreeBuffer();
        sscrnModelClear(wk);
        sscrnLightClear(wk);
        wk->x269 = 1;
        wk->x26A = 0;
        state++;
    case 3: {
        int stat;
        int size;
        if (Dvd.ReadCheck(cap_read_req, &stat, &size, 0) != 1) {
            break;
        }
        wk->pExam = wk->pPzzl;
        state++;
    }
    case 4:
        FadeSetW(0x80000000, 5, 0, 0);
        transit(0, wk);
        break;
    }
}

void SsCapMain::init(SUB_SCREEN* wk)
{
    sel = new CapSelect;
    exam = new SsItemExamine;
    sel->connect(0, exam);
    exam->connect(0, sel);
    IdTexDataLoad(SS_ARC_PTR(wk->pExam, 5), 9);
    IdSub.set(SS_ARC_PTR(wk->pExam, 6), 0xFF, 0x14, 0xC, 6, 0);
    sscrnLightCreate(wk, (cLit*) SS_ARC_PTR(wk->pCmmn, 0x14));
    MesData.setPtr(2, (u8*) SS_ARC_PTR(wk->pExam, 4));
    sscrnMainMenuInit(wk, 0);
    state = 0;
#line 191 "D:/Bio4/Prog/ss_cap.cpp"
    wk->x310 = (s8*) MEM_ALLOC(3, 1, 13);
    wk->x310[0] = 0;
    wk->x310[1] = 0;
    wk->x310[2] = 0;
    if (wk->type == 0x100) {
        cur = exam;
    } else {
        cur = sel;
    }
}

void SsCapMain::move(SUB_SCREEN* wk)
{
    cMes.setLayout(0, 2);
    if (state == 0) {
        Widget<SUB_SCREEN>* w = cur;
        w->move(wk);
        next = w->cur;
        if (cur == sel) {
            switch (sel->state) {
            case 1:
                wk->x34 |= 0x80000;
                transit(0, wk);
                break;
            case 2:
                wk->x34 |= 0x80000;
                transit(1, wk);
                break;
            }
        }
        cur = next;
    }
}

static void sscrn_cap_out_init(SUB_SCREEN* wk);
static int sscrn_cap_out(SUB_SCREEN* wk);

void SsCapMain::quit(SUB_SCREEN* wk)
{
    sscrnLightClear(wk);
    sscrnLightCreate(wk, (cLit*) SS_ARC_PTR(wk->pCmmn, 0x12));
    Mem_free(wk->x310);
    if (sel) {
        delete sel;
    }
    if (exam) {
        delete exam;
    }
    sscrn_cap_out_init(wk);
    wk->x4C = sscrn_cap_out;
}

static void sscrn_cap_out_init(SUB_SCREEN* wk)
{
    FadeSetW(0, 7, 0, 0);
}

static int sscrn_cap_out(SUB_SCREEN* wk)
{
    int ret;

    if (Fade[0].flags & 1) {
        ret = 0;
    } else {
        IdSub.dispSw(0, 1);
        wk->x269 = 0;
        wk->x26A = 0;
        Cckpt.life.fix(0);
        FadeSetW(0x80000000, 7, 0, 0);
        ret = 1;
    }
    return ret;
}

void dispCapList(SUB_SCREEN* wk)
{
    s8* sel = wk->x310;
    IdUnit* u;
    IdUnit* c;
    int i;

    for (i = 0; i < 24; i++) {
        u = IdSub.unitPtr(i + 1, 0x14);
        if (ItemMgr.search(cap_id_tbl[i])) {
            u->flags |= 8;
            u->flags_7F |= 2;
            u->no = cap_id_tbl[i] + 0x25;
        } else {
            u->flags &= ~8;
        }
    }
    u = IdSub.unitPtr(0xFE, 0x14);
    c = IdSub.unitPtr(sel[2] + 1, 0x14);
    u->scr = c->scr;
}

void CapSelect::init(SUB_SCREEN* wk)
{
}

void CapSelect::move(SUB_SCREEN* wk)
{
    s8* sel = wk->x310;

    dispCapList(wk);
    state = 0;
    if (Key.trg & 0x100000) {
        state = 2;
        return;
    }
    if (Key.trg & 0x40000000) {
        state = 1;
        SndCall(0, 5, 0, 0, 0, 0);
        return;
    }
    if (Key.trg & 0x80000000) {
        wk->x248 = ItemMgr.search(cap_id_tbl[sel[2]]);
        if (wk->x248) {
            wk->x24C = MapMgr.getWork(2);
            transit(0, wk);
            SndCall(0, 0x1A, 0, 0, 0, 0);
        }
        return;
    }
    if (Key.trg & 0x20000) {
        if (ItemMgr.search(cap_id_tbl[sel[2]]) == 0) {
            ItemMgr.get(cap_id_tbl[sel[2]], 0);
        }
        return;
    }
    {
        s8 old = sel[2];
        if (Key.rep & 0x02000000) {
            sel[0]--;
        }
        if (Key.rep & 0x01000000) {
            sel[0]++;
        }
        sel[0] = sel[0] < 0 ? 3 : (sel[0] > 3 ? 0 : sel[0]);
        if (Key.rep & 0x08000000) {
            sel[1]--;
        }
        if (Key.rep & 0x04000000) {
            sel[1]++;
        }
        sel[1] = sel[1] < 0 ? 5 : (sel[1] > 5 ? 0 : sel[1]);
        sel[2] = sel[1] + sel[0] * 6;
        if (old != sel[2]) {
            SndCall(0, 0xA, 0, 0, 0, 0);
        }
    }
}

void CapSelect::quit(SUB_SCREEN* wk)
{
}
