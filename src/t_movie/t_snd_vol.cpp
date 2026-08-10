#include "types.h"
#include "global.h"
#include "main_mem.h"
#include "scheduler.h"
#include "joy.h"
#include "eprintf.h"
#include "snd.h"
#include "t_prim.h"
#include "t_util.h"
#include "file.h"
#include "db_log.h"

extern "C" int sprintf(char* s, const char* fmt, ...);
static inline int IRef(int& v) { return v; }


// Distance curve entry as edited (SndCurveEnt with a signed value).
struct TblEnt {
    f32 dist;  // 0x0
    u16 flag;  // 0x4  bit 0 = user point
    s16 val;   // 0x6
};

// Editable copy of a SndCurveTbl: 100 points + one spare.
struct EditTbl {
    u32 num;         // 0x000
    f32 scale;       // 0x004
    TblEnt e[101];   // 0x008
};

// Which curves a SIT set uses ([0] stereo, [1] DPL2), 8 bytes.
struct CombSel {
    s8 vol[2];     // 0x0
    s8 pitch[2];   // 0x2
    s8 filter[2];  // 0x4
    u8 used;       // 0x6
    u8 pad;
};

struct SndVolWork {
    s16 mode;        // 0x00  index into mode_func
    s16 sub;         // 0x02  sub mode
    s16 step;        // 0x04  step inside the sub mode
    s16 x6;
    u32 x8;
    f32 curDist;     // 0x0C  dist of the edited point
    int curVal;      // 0x10  value of the edited point
    int left;        // 0x14  first visible dist column
    int cur;         // 0x18  selected table (0..31)
    int pt;          // 0x1C  selected point
    int tblType;     // 0x20  0 volume, 1 pitch, 2 filter
    int editMode;    // 0x24
    s8 menuCur;      // 0x28
    s8 x29;          // 0x29  reverb: DPL2/stereo set; combine: column
    s8 efxCur[2];    // 0x2A  cursor per set
    SndEfxParam efx[2];     // 0x2C
    CombSel sel[32];        // 0x6C
    CombSel selBackup;      // 0x16C
    EditTbl* curTbl;        // 0x174
    EditTbl vol[32];        // 0x178
    EditTbl pitch[32];      // 0x6778
    EditTbl filter[32];     // 0xCD78
    EditTbl backup;         // 0x13378
    u8 fileBuf[0x20000];    // 0x136A8  save/load image (SndRoomHdr + curve data)
    char path[0x40];        // 0x336A8
    u8 dest;         // 0x336E8  0 local, 1 server
    s8 stage;        // 0x336E9
    s8 room;         // 0x336EA
    s8 loadCur;      // 0x336EB
    u8 pad_336EC[4];
    int timer;       // 0x336F0
    u8 yesno;        // 0x336F4
    s8 copySrc;      // 0x336F5
    s8 copyDst;      // 0x336F6
    u8 pad_336F7;
};

// the work pointer is a struct member: every store through it reloads it
struct SndVolWorkPtr {
    SndVolWork* p;
};
static SndVolWorkPtr sndVolWork = {NULL};
#define work sndVolWork.p

static u32 cursorCol[16] = {0x808080FF, 0x909090FF, 0xA0A0A0FF, 0xB0B0B0FF, 0xC0C0C0FF, 0xD0D0D0FF, 0xE0E0E0FF, 0xF0F0F0FF,
                            0xF0F0F0FF, 0xE0E0E0FF, 0xD0D0D0FF, 0xC0C0C0FF, 0xB0B0B0FF, 0xA0A0A0FF, 0x909090FF, 0x808080FF};

void getInfoData(SndRoomHdr* hdr);
void init();
void exit();
static void edit_menu();
void ListLineDraw(TblEnt* e, u32 col, int x, int y, int type);
void ListDraw(s16 x, s16 y, u32 col, s8 no, u8 type);
void select_cur_move();
static void data_select_main();
static void data_copy();
static void data_delete();
static void data_select();
static void data_edit();
void markDraw(s16 val, u32 col, int kind, f32 dist);
// COMPILER-DIFF: 1 -- floats-first view of markDraw: the original loads `lha val` before `lfs dist`
void markDrawF(f32 dist, s16 val, u32 col, int kind) asm("markDraw__FsUlif");
void mainFrameDisp();
void editDataLineDraw(TblEnt* e, u32 col);
void editDataDraw(EditTbl* tbl);
void editScreenDisp();
static void edit_reverb_param();
void combine_tbl_disp();
static void combine_tbl_select();
static void combine_tbl_edit();
static void combine_tbl_copy();
static void combine_tbl_delete();
static void edit_combine_tbl();
static void file_save();
static void file_load();
void ToolSndVolEdit();

void getInfoData(SndRoomHdr* hdr)
{
    int i;
    u32 ofs;
    u32 size;
    SndCurveTbl* t;

    work->efx[0] = hdr->efx[0];
    work->efx[1] = hdr->efx[1];
    for (i = 0; i < 32; i++) {
        int pad0 = i, pad1 = i, pad2 = i, pad3 = i, pad4 = i, pad5 = i; // COMPILER-DIFF: 5 (interblock region size: the original does not hoist the giv increments of this loop)
        ofs = hdr->curve_sel[i];
        if (ofs != 0) {
            memcpy(&work->sel[i], (u8*) hdr + ofs, sizeof(CombSel));
        }
        ofs = hdr->vol_ofs[i];
        if (ofs != 0) {
            t = (SndCurveTbl*) ((u8*) hdr + ofs);
            size = t->num * 8 + 8;
            memcpy(&work->vol[i], t, size);
        }
        ofs = hdr->pitch_ofs[i];
        if (ofs != 0) {
            t = (SndCurveTbl*) ((u8*) hdr + ofs);
            size = t->num * 8 + 8;
            memcpy(&work->pitch[i], t, size);
        }
        ofs = hdr->filter_ofs[i];
        if (ofs != 0) {
            t = (SndCurveTbl*) ((u8*) hdr + ofs);
            size = t->num * 8 + 8;
            memcpy(&work->filter[i], t, size);
        }
    }
}

void init()
{
    u8 i;
    SndVolWork*& wp = sndVolWork.p;

    TaskSuspend(0);
    TutilInitDefault();
    wp = (SndVolWork*) Debug_alloc(sizeof(SndVolWork), 1);
    memclr_asm(work, sizeof(SndVolWork));
    for (i = 0; i < 32; i++) {
        work->vol[i].scale = 1000.0f;
        work->pitch[i].scale = 1000.0f;
        work->filter[i].scale = 1000.0f;
    }
    work->mode = 5;
    work->sub = 0;
    work->step = 0;
    work->x6 = 0;
}

void exit()
{
    TutilQuitDefault();
    TaskSignal(0);
    TaskExit();
}

static void edit_menu()
{
    s8 c;

    eprintf(0x30, 0x40, work->menuCur == 0 ? 6 : 0, 0, "REVERB PARAMETER EDIT");
    eprintf(0x30, 0x50, work->menuCur == 1 ? 6 : 0, 0, "VOLUME TABLE EDIT");
    eprintf(0x30, 0x60, work->menuCur == 2 ? 6 : 0, 0, "PITCH TABLE EDIT");
    eprintf(0x30, 0x70, work->menuCur == 3 ? 6 : 0, 0, "FILTER TABLE EDIT");
    eprintf(0x30, 0x80, work->menuCur == 4 ? 6 : 0, 0, "SET TABLE EDIT");
    eprintf(0x30, 0x90, work->menuCur == 5 ? 6 : 0, 0, "LOAD");
    eprintf(0x30, 0xA0, work->menuCur == 6 ? 6 : 0, 0, "SAVE");
    eprintf(0x30, 0xB0, work->menuCur == 7 ? 6 : 0, 0, "EXIT");
    if (Joy[0].rep & 0x80008) {
        work->menuCur--;
    } else if (Joy[0].rep & 0x40004) {
        work->menuCur++;
    } else if (Joy[0].trg & 0x200) {
        work->menuCur = 7;
    } else if (Joy[0].trg & 0x100) {
        work->cur = 0;
        c = work->menuCur;
        switch (c) {
        case 0:
            work->mode = 3;
            work->step = 0;
            work->x29 = 1;
            break;
        case 1:
            work->mode = 1;
            work->step = 0;
            work->curTbl = work->vol;
            work->tblType = 0;
            break;
        case 2:
            work->mode = 1;
            work->step = 0;
            work->curTbl = work->pitch;
            work->tblType = 1;
            break;
        case 3:
            work->mode = 1;
            work->step = 0;
            work->curTbl = work->filter;
            work->tblType = 2;
            break;
        case 4:
            work->mode = 4;
            work->step = 0;
            work->cur = 0;
            break;
        case 5:
            work->mode = c;
            work->step = 0;
            break;
        case 6:
            work->mode = c;
            work->step = 0;
            break;
        case 7:
            exit();
            break;
        }
    }
    work->menuCur = work->menuCur < 0 ? 7 : work->menuCur > 7 ? 0 : work->menuCur;
}

void ListLineDraw(TblEnt* e, u32 col, int x, int y, int type)
{
    S16Vec pt[2];
    int x0 = (int) e[0].dist;
    int x1 = (int) e[1].dist;
    int y0 = 0;
    int y1 = 0;

    if (type & 0x80) {
        type &= ~0x80;
        x0 *= 2;
        x1 *= 2;
        switch (type) {
        case 0:
            y0 = (0x82 - e[0].val) / 2;
            y1 = (0x82 - e[1].val) / 2;
            break;
        case 1:
            y0 = 0x20 - e[0].val;
            y1 = 0x20 - e[1].val;
            break;
        case 2:
            y0 = (e[0].val + 4) * 2;
            y1 = (e[1].val + 4) * 2;
            break;
        }
    } else if (type & 0x40) {
        type &= ~0x40;
        switch (type) {
        case 0:
            y0 = (0x82 - e[0].val) / 2;
            y1 = (0x82 - e[1].val) / 2;
            break;
        case 1:
            y0 = 0x20 - e[0].val;
            y1 = 0x20 - e[1].val;
            break;
        case 2:
            y0 = (e[0].val + 4) * 2;
            y1 = (e[1].val + 4) * 2;
            break;
        }
    } else {
        switch (type) {
        case 0:
            y0 = (0x82 - e[0].val) / 5;
            y1 = (0x82 - e[1].val) / 5;
            break;
        case 1:
            y0 = (0x1A - e[0].val) / 2;
            y1 = (0x1A - e[1].val) / 2;
            break;
        case 2:
            y0 = e[0].val + 1;
            y1 = e[1].val + 1;
            break;
        }
    }
    pt[0].x = x + x0;
    pt[0].y = y + y0;
    pt[0].z = 0;
    pt[1].x = x + x1;
    pt[1].y = y + y1;
    pt[1].z = 0;
    TprimDrawFrameFn_s16(pt, (GXColor*) &col, 2);
}

void ListDraw(s16 x, s16 y, u32 col, s8 no, u8 type)
{
    S16Vec pt[4];
    GXColor gray;
    EditTbl* tbl;
    u16 i;

    if (type & 0x80) {
        *(u32*) &gray = 0x80800080; // r, g, a = 0x80, b = 0 (as in the original)
        pt[0].x = x + (s16) (work->curDist * 2.0f);
        pt[0].y = y - 1;
        pt[0].z = 0;
        pt[1].x = x + (s16) (work->curDist * 2.0f);
        pt[1].y = y + 0x41;
        pt[1].z = 0;
        TprimDrawFrameFn_s16(pt, &gray, 2);
        pt[0].x = x - 1;
        pt[0].y = y - 1;
        pt[0].z = 0;
        pt[1].x = x - 1;
        pt[1].y = y + 0x41;
        pt[1].z = 0;
        pt[2].x = x + 0xC9;
        pt[2].y = y + 0x41;
        pt[2].z = 0;
        pt[3].x = x + 0xC9;
        pt[3].y = y - 1;
        pt[3].z = 0;
    } else if (type & 0x40) {
        pt[0].x = x - 1;
        pt[0].y = y - 1;
        pt[0].z = 0;
        pt[1].x = x - 1;
        pt[1].y = y + 0x41;
        pt[1].z = 0;
        pt[2].x = x + 0x65;
        pt[2].y = y + 0x41;
        pt[2].z = 0;
        pt[3].x = x + 0x65;
        pt[3].y = y - 1;
        pt[3].z = 0;
    } else {
        pt[0].x = x - 1;
        pt[0].y = y - 1;
        pt[0].z = 0;
        pt[1].x = x - 1;
        pt[1].y = y + 0x1A;
        pt[1].z = 0;
        pt[2].x = x + 0x65;
        pt[2].y = y + 0x1A;
        pt[2].z = 0;
        pt[3].x = x + 0x65;
        pt[3].y = y - 1;
        pt[3].z = 0;
    }
    TprimDrawFrameFn_s16(pt, (GXColor*) &col, 4);
    if ((u8) no <= 31) {
        tbl = &work->curTbl[no];
        if (tbl->num != 0) {
            for (i = 0; i < (int) tbl->num - 1; i++) {
                ListLineDraw(&tbl->e[i], col, x, y, type);
            }
        } else if (type & 0x40) {
            eprintf(x + 0x18, y + 0x18, 0, 0, "NO DATA");
        } else if (!(type & 0x80)) {
            eprintf(x + 0x18, y + 5, 0, 0, "NO DATA");
        }
    } else if (no == -1 && (type & 0x40)) {
        eprintf(x + 0x18, y + 0x18, 0, 0, "NOT USE");
    }
}

void select_cur_move()
{
    if (Joy[0].rep & 0x80008) {
        if (work->cur / 4 != 0) {
            work->cur -= 4;
        }
    } else if (Joy[0].rep & 0x40004) {
        if (work->cur / 4 != 7) {
            work->cur += 4;
        }
    } else if (Joy[0].rep & 0x10001) {
        if (work->cur % 4 != 0) {
            work->cur--;
        }
    } else if (Joy[0].rep & 0x20002) {
        if (work->cur % 4 != 3) {
            work->cur++;
        }
    }
    work->cur = work->cur < 0 ? 0 : work->cur > 31 ? 31 : work->cur;
}

static void data_select_main()
{
    if (Joy[0].trg & 0x100) {
        work->mode = 2;
        work->sub = 0;
        work->step = 0;
        work->x6 = 0;
        work->backup = work->curTbl[work->cur];
        if (work->curTbl[work->cur].num != 0) {
            work->editMode = 0;
        } else {
            work->editMode = 1;
        }
    } else if (Joy[0].trg & 0x200) {
        work->mode = 0;
        work->sub = 0;
        work->step = 0;
        work->x6 = 0;
    } else if (Joy[0].trg & 0x10) {
        work->sub = 1;
        work->step = 0;
        work->x6 = 0;
    } else if (Joy[0].trg & 0x800) {
        work->sub = 2;
        work->step = 0;
        work->x6 = 0;
    }
    select_cur_move();
}

static void data_copy()
{
    int move = 1;

    switch (work->step) {
    case 0:
        if (work->curTbl[work->cur].num != 0) {
            work->step++;
            work->copySrc = work->cur;
        } else {
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
            return;
        }
        break;
    case 1:
        work->copyDst = work->cur;
        if (Joy[0].trg & 0x200) {
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        } else if (Joy[0].trg & 0x100) {
            if (work->copySrc != work->copyDst) {
                work->step++;
                work->yesno = 1;
            }
        }
        break;
    case 2:
        eprintf(0x28, 0x16C, 0, 0, "DATA COPY OK?");
        move = 0;
        eprintf(0x98, 0x16C, 0, 0, "[   /  ]");
        eprintf(0xA0, 0x16C, work->yesno == 0 ? 6 : 7, 0, "YES");
        eprintf(0xC0, 0x16C, work->yesno == 1 ? 6 : 7, 0, "NO");
        if (Joy[0].trg & 0x200) {
            work->step--;
        } else if (Joy[0].trg & 0x100) {
            if (work->copySrc != work->copyDst) {
                if (work->yesno == 1) {
                    work->step--;
                } else {
                    work->curTbl[work->copyDst] = work->curTbl[work->copySrc];
                    work->step++;
                    work->timer = 30;
                }
            }
        } else if (Joy[0].trg & 0x30003) {
            work->yesno ^= 1;
        }
        break;
    case 3:
        eprintf(0x40, 0x18C, 6, 0, "DATA COPY COMPLETE.");
        move = 0;
        if ((Joy[0].trg & 0x300) || work->timer <= 0) {
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        }
        work->timer--;
        break;
    }
    eprintf(0x28, 0x15C, 0, 0, "DATA COPY  [DATA %2d] -> [DATA %2d]", work->copySrc, work->copyDst);
    if (move == 1) {
        select_cur_move();
    }
}

static void data_delete()
{
    switch (work->step) {
    case 0:
        if (work->curTbl[work->cur].num == 0) {
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
            return;
        }
        work->step++;
        work->yesno = 1;
    case 1:
        if (Joy[0].trg & 0x30003) {
            work->yesno ^= 1;
        } else if (Joy[0].trg & 0x200) {
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        } else if (Joy[0].trg & 0x100) {
            if (work->yesno == 1) {
                work->sub = 0;
                work->step = 0;
                work->x6 = 0;
            } else {
                memclr_asm(&work->curTbl[work->cur], sizeof(EditTbl));
                work->step++;
                work->timer = 30;
            }
        }
        break;
    case 2:
        eprintf(0x28, 0x16C, 6, 0, "DATA DELETE COMPLETE.");
        if ((Joy[0].trg & 0x300) || work->timer <= 0) {
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        }
        work->timer--;
        break;
    }
    eprintf(0x28, 0x15C, 0, 0, "[DATA %2d] DELETE OK? [   /  ]", work->cur);
    eprintf(0xD8, 0x15C, work->yesno == 0 ? 6 : 7, 0, "YES");
    eprintf(0xF8, 0x15C, work->yesno == 1 ? 6 : 7, 0, "NO");
}

static char* select_title[3] = {"EDIT VOLUME TABLE DATA SELECT", "EDIT PITCH TABLE DATA SELECT",
                                "EDIT FILTER TABLE DATA SELECT"};
static void (*select_func[3])() = {data_select_main, data_delete, data_copy};

static void data_select()
{
    int i;
    int j;
    u32 col;

    eprintf(0x28, 0x28, 0, 0, "%s", select_title[work->tblType]);
    eprintf(0x198, 0x28, 0, 0, "[DATA %2d]", work->cur);
    select_func[work->sub]();
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 4; j++) {
            if (i * 4 + j == work->cur) {
                col = cursorCol[pG->flags_51E4 & 0xF];
            } else {
                col = 0xFFFFFFFF;
            }
            ListDraw(0x30 + j * 0x6E, 0x44 + i * 0x23, col, i * 4 + j, work->tblType);
        }
    }
    eprintf(0x15E, 0x15C, 0, 0, "Y ... DATA COPY");
    eprintf(0x15E, 0x16C, 0, 0, "Z ... DATA DELETE");
    eprintf(0x15E, 0x17C, 0, 0, "A ... EDIT");
    eprintf(0x15E, 0x18C, 0, 0, "B ... RETURN MENU");
}

static char* edit_title[3] = {"[VOLUME TABLE EDIT]", "[PITCH TABLE EDIT]", "[FILTER TABLE EDIT]"};
static s16 val_range[3][2] = {{0, 0x7F}, {-24, 24}, {0, 0x17}};

static void data_edit()
{
    EditTbl* tbl = &work->curTbl[work->cur];
    TblEnt* e;
    int step;
    int i;

    eprintf(0x28, 0x28, 0, 0, "%s", edit_title[work->tblType]);
    eprintf(0x1A0, 0x28, 0, 0, "[DATA %2d]", work->cur);
    step = 1;
    if (Joy[0].on & 0x400) {
        step = 10;
    }
    if (work->sub == 0) {
        work->sub = 1;
        work->step = 0;
        work->x6 = 0;
        if (tbl->num != 0) {
            work->curDist = tbl->e[0].dist;
            work->curVal = tbl->e[0].val;
            if (tbl->num == 1) {
                work->editMode = 1;
            }
        } else {
            work->curDist = 0.0f;
            if (work->tblType == 0) {
                work->curVal = 0x7F;
            } else if (work->tblType == 1) {
                work->curVal = 0;
            } else {
                work->curVal = 0;
            }
        }
    }
    if (tbl->num == 0) {
        work->pt = 0;
        tbl->e[0].dist = work->curDist;
        tbl->e[0].val = work->curVal;
        tbl->num++;
        work->editMode = 1;
    }
    if ((Joy[0].trg & 0x10) && tbl->num != 0) {
        for (i = work->pt; i < (int) tbl->num - 1; i++) {
            tbl->e[i] = tbl->e[i + 1];
        }
        tbl->num--;
        work->pt--;
    } else if (Joy[0].trg & 0x100) {
        if (work->editMode == 1) {
            tbl->e[work->pt].flag |= 1;
            if (work->pt == tbl->num - 1) {
                if (tbl->e[work->pt].dist != 100.0f) {
                    // COMPILER-DIFF: register pin (local-alloc gives ne r10 / editMode r8; the target has r8 / r7)
                    register TblEnt* ne asm("r8") = &tbl->e[tbl->num];

                    ne->dist = ne[-1].dist + 1.0f;
                    ne->val = ne[-1].val;
                    work->curDist = ne->dist;
                    work->curVal = ne->val;
                    work->pt = tbl->num;
                    tbl->num++;
                    work->editMode = 1;
                }
            } else {
                work->editMode = 0;
            }
        } else {
            work->editMode = 1;
        }
    } else if (Joy[0].trg & 0x200) {
        if (work->editMode == 1) {
            work->editMode = 0;
        } else {
            work->mode = 1;
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        }
    } else if (Joy[0].rep & 0x40) {
        if (work->editMode == 0) {
            work->pt--;
        }
    } else if (Joy[0].rep & 0x20) {
        if (work->editMode == 0) {
            work->pt++;
        }
    }
    work->pt = work->pt < 0 ? 0 : work->pt > (int) tbl->num - 1 ? (int) tbl->num - 1 : work->pt;
    if (Joy[0].trg & 0x800) {
        switch ((u32) tbl->scale) {
        case 1000:
            tbl->scale = 10000.0f;
            break;
        case 10000:
            tbl->scale = 100000.0f;
            break;
        case 100000:
            tbl->scale = 1000.0f;
            break;
        default:
            tbl->scale = 1000.0f;
            break;
        }
    }
    e = &tbl->e[work->pt];
    if (work->editMode == 1) {
        if (work->tblType == 2) {
            step = -step;
        }
        if (Joy[0].rep2 & 0x80008) {
            e->val += step;
        } else if (Joy[0].rep2 & 0x40004) {
            e->val -= step;
        }
        if (work->tblType == 2) {
            step = -step;
        }
        if (Joy[0].rep & 0x10001) {
            e->dist -= (f32) step;
        } else if (Joy[0].rep & 0x20002) {
            e->dist += (f32) step;
        }
        e->val = e->val < val_range[work->tblType][0] ? val_range[work->tblType][0]
                 : e->val > val_range[work->tblType][1] ? val_range[work->tblType][1] : e->val;
        if (work->pt == 0) {
            if (tbl->num == 1) {
                e->dist = e->dist < 0.0f ? 0.0f : e->dist > 100.0f ? 100.0f : e->dist;
            } else {
                e->dist = e->dist < 0.0f ? 0.0f : e->dist > e[1].dist - 1.0f ? e[1].dist - 1.0f : e->dist;
            }
        } else if (work->pt == tbl->num - 1) {
            e->dist = e->dist < e[-1].dist + 1.0f ? e[-1].dist + 1.0f : e->dist > 100.0f ? 100.0f : e->dist;
        } else {
            e->dist = e->dist < e[-1].dist + 1.0f ? e[-1].dist + 1.0f
                      : e->dist > e[1].dist - 1.0f ? e[1].dist - 1.0f : e->dist;
        }
    }
    work->curDist = e->dist;
    work->curVal = e->val;
    ListDraw(0x40, 0x40, 0xFFFFFFFF, work->cur, work->tblType | 0x80);
    editScreenDisp();
    if (work->editMode == 0) {
        eprintf(0x158, 0x38, 0, 0, "Y    SCALE CHANGE");
        eprintf(0x158, 0x48, 0, 0, "L,R  POINT CHANGE");
        eprintf(0x158, 0x58, 0, 0, "Z    POINT DELETE");
        eprintf(0x158, 0x68, 0, 0, "A    EDIT MODE ON");
        eprintf(0x158, 0x78, 0, 0, "B    RETURN MENU");
    } else {
        eprintf(0x158, 0x38, 0, 0, "Y    SCALE CHANGE");
        eprintf(0x158, 0x48, 0, 0, "Z    POINT DELETE");
        if (work->pt == tbl->num - 1) {
            eprintf(0x158, 0x58, 0, 0, "A    SET NEW POINT");
            eprintf(0x158, 0x68, 0, 0, "B    EDIT MODE OFF");
        } else {
            eprintf(0x158, 0x58, 0, 0, "A,B  EDIT MODE OFF");
        }
    }
}

static u8 blink_r = 0;
static s8 blink_dir = 1;

void markDraw(s16 val, u32 col, int kind, f32 dist)
{
    S16Vec pt[4];
    int x = (int) dist - IRef(work->left);
    s16 px;
    s16 py = 0;

    if (x < 0) {
        return;
    }
    if (x > 13) {
        return;
    }
    px = x * 32 + 0x40;
    switch (work->tblType) {
    case 0:
        py = (0x82 - val) * 2 + 0x8C;
        break;
    case 1:
        py = (0x1E - val) * 4 + 0x96;
        break;
    case 2:
        py = val * 8 + 0xC4;
        break;
    }
    switch ((u32) kind) {
    case 0:
        pt[0].x = px - 9;
        pt[0].y = py - 9;
        pt[0].z = 0;
        pt[1].x = px + 9;
        pt[1].y = py - 9;
        pt[1].z = 0;
        pt[2].x = px + 9;
        pt[2].y = py + 9;
        pt[2].z = 0;
        pt[3].x = px - 9;
        pt[3].y = py + 9;
        pt[3].z = 0;
        break;
    case 1:
        pt[0].x = px;
        pt[0].y = py - 5;
        pt[0].z = 0;
        pt[1].x = px + 5;
        pt[1].y = py;
        pt[1].z = 0;
        pt[2].x = px;
        pt[2].y = py + 5;
        pt[2].z = 0;
        pt[3].x = px - 5;
        pt[3].y = py;
        pt[3].z = 0;
        break;
    case 2:
        pt[0].x = px;
        pt[0].y = py - blink_r;
        pt[0].z = 0;
        pt[1].x = px + blink_r;
        pt[1].y = py;
        pt[1].z = 0;
        pt[2].x = px;
        pt[2].y = py + blink_r;
        pt[2].z = 0;
        pt[3].x = px - blink_r;
        pt[3].y = py;
        pt[3].z = 0;
        blink_r += blink_dir;
        if (blink_dir == 1 && blink_r == 6) {
            blink_dir = -1;
        }
        if (blink_dir == -1 && blink_r == 0) {
            blink_dir = 1;
        }
        break;
    }
    TprimDrawFrameFn_s16(pt, (GXColor*) &col, 4);
}

void mainFrameDisp()
{
    S16Vec pt[3];
    GXColor col;

    *(u32*) &col = 0x20202000;
    pt[0].x = 0;
    pt[0].y = 0;
    pt[0].z = 0;
    pt[1].x = 0x200;
    pt[1].y = 0;
    pt[1].z = 0;
    pt[2].x = 0x200;
    pt[2].y = 0x1C0;
    pt[2].z = 0;
    TprimDrawPolyFn_s16(pt, &col, 3);
    pt[0].x = 0;
    pt[0].y = 0;
    pt[0].z = 0;
    pt[1].x = 0x200;
    pt[1].y = 0x1C0;
    pt[1].z = 0;
    pt[2].x = 0;
    pt[2].y = 0x1C0;
    pt[2].z = 0;
    TprimDrawPolyFn_s16(pt, &col, 3);
}

void editDataLineDraw(TblEnt* e, u32 col)
{
    S16Vec pt[2];
    s16 x0;
    s16 x1;
    s16 base = 0;
    s16 y0 = 0;
    s16 y1 = 0;
    f32 v;
    f32 dv;
    f32 prev;
    int x;
    f32 d0 = e[0].dist;
    f32 d1 = e[1].dist;
    int left = IRef(work->left);

    if (d0 >= (f32) (left + 13)) {
        return;
    }
    if (d1 <= (f32) left) {
        return;
    }
    x0 = (s16) (d0 - (f32) left);
    x1 = (s16) (d1 - (f32) left);
    dv = ((f32) e[1].val - (f32) e[0].val) / (d1 - d0);
    v = (f32) e[0].val;
    for (x = x0; x < x1; x++) {
        prev = v;
        v += dv;
        if (x < 0) {
            continue;
        }
        if (x > 12) {
            break;
        }
        switch (work->tblType) {
        case 0:
            base = 0x8C;
            y0 = (s16) ((130.0f - prev) * 2.0f);
            y1 = (s16) ((130.0f - v) * 2.0f);
            break;
        case 1:
            base = 0x96;
            y0 = (s16) ((30.0f - prev) * 4.0f);
            y1 = (s16) ((30.0f - v) * 4.0f);
            break;
        case 2:
            base = 0xB4;
            y0 = (s16) ((prev + 2.0f) * 8.0f);
            y1 = (s16) ((v + 2.0f) * 8.0f);
            break;
        }
        pt[0].x = x * 32 + 0x40;
        pt[1].x = x * 32 + 0x60;
        pt[0].y = base + y0;
        pt[0].z = 0;
        pt[1].y = base + y1;
        pt[1].z = 0;
        TprimDrawFrameFn_s16(pt, (GXColor*) &col, 2);
    }
}

void editDataDraw(EditTbl* tbl)
{
    s16 i;
    u32 col;
    int kind;

    if (tbl->num == 0) {
        return;
    }
    for (i = 0; i < (int) tbl->num; i++) {
        TblEnt* e = &tbl->e[i];

        col = 0xFFFFFFFF;
        kind = 1;
        if (i == work->pt) {
            col = 0x00FF00FF;
            if (work->editMode == 1) {
                kind = 2;
            }
        }
        markDrawF(e->dist, e->val, col, kind);
    }
    for (i = 0; i < (int) tbl->num - 1; i++) {
        editDataLineDraw(&tbl->e[i], 0xFFFFFFFF);
    }
}

static char* filter_name[24] = {"16000Hz", "12800Hz", "10240Hz", " 8000Hz", " 6400Hz", " 5120Hz", " 4000Hz", " 3200Hz",
                                " 2560Hz", " 2000Hz", " 1600Hz", " 1280Hz", " 1000Hz", "  800Hz", "  640Hz", "  500Hz",
                                "  400Hz", "  320Hz", "  256Hz", "  200Hz", "  160Hz", "  128Hz", "  100Hz", "   80Hz"};

void editScreenDisp()
{
    EditTbl* tbl = &work->curTbl[work->cur];
    S16Vec pt[3];
    GXColor col;
    s16 i;
    s16 base = 0;
    s16 rows = 0;
    s16 curY = 0;
    s16 x;
    s16 v;

    v = (s16) (work->curDist - 10.0f);
    if (work->left > v) {
        work->left = v;
    }
    v = (s16) (work->curDist - 4.0f);
    if (work->left < v) {
        work->left = v;
    }
    work->left = work->left < 0 ? 0 : work->left > 0x57 ? 0x57 : work->left;
    switch (work->tblType) {
    case 0:
        rows = 13;
        base = 0x8C;
        for (i = 0; i <= 13; i++) {
            eprintf2(8, 14, 0x20, 0x84 + i * 0x14, 0, 0, "%3d", (13 - i) * 10);
        }
        curY = (0x82 - work->curVal) * 2 + 0x8C;
        break;
    case 1:
        rows = 12;
        base = 0x96;
        for (i = 0; i <= 12; i++) {
            eprintf2(8, 14, 0x20, 0x8E + i * 0x14, 0, 0, "%3d", 0x1E - i * 5);
        }
        curY = (0x1E - work->curVal) * 4 + 0x96;
        break;
    case 2:
        rows = 10;
        base = 0xB4;
        curY = work->curVal * 8 + 0xC4;
        break;
    }
    for (i = 0; i <= 13; i++) {
        if ((work->left + i) % 5 == 0) {
            *(u32*) &col = 0x808080FF;
        } else {
            *(u32*) &col = 0x404040FF;
        }
        x = i * 32 + 0x40;
        pt[0].x = x;
        pt[0].y = base;
        pt[0].z = 0;
        pt[1].x = x;
        pt[1].y = base + rows * 0x14;
        pt[1].z = 0;
        TprimDrawFrameFn_s16(pt, &col, 2);
    }
    *(u32*) &col = 0x80808080;
    x = 0x40;
    for (i = 0; i <= rows; i++) {
        // value pins (local-alloc fake-lifetime parity): the original keeps the SI sum in r0 and the
        // sign-extended row in a fresh r10; ours lets the extsh reuse the dying r0
        register int v2 asm("r10"); // COMPILER-DIFF: candidate (local-alloc qty order)
        register int t asm("r0");   // COMPILER-DIFF: candidate (local-alloc qty order)
        t = base + i * 0x14;
        v2 = (s16) t;
        pt[0].x = x;
        pt[0].y = v2;
        pt[0].z = 0;
        pt[1].x = 0x1E0;
        pt[1].y = v2;
        pt[1].z = 0;
        TprimDrawFrameFn_s16(pt, &col, 2);
    }
    *(u32*) &col = 0x80800080;
    x = (u16) (s16) ((work->curDist - (f32) work->left) * 32.0f) + 0x40;
    {
        // COMPILER-DIFF: 3 -- the original computes `base - 4` and `base + rows * 0x14 + 4` here from a copy of
        // base (`mr r10,r23`); our block LCM PREs the single `base - 4` occurrence above the two loops. The
        // copy into a pinned hard register is that copy (a hard-register operand is not anticipatable at the
        // block entry, so neither sum is PRE'd, and a plain register copy is never a gcse expression).
        // the copy shares the dead work-pointer register r10 and `b - 4` is a fresh r0 (not in place)
        register int b asm("r10"); // COMPILER-DIFF: candidate (local-alloc qty order)
        register int t asm("r0");  // COMPILER-DIFF: candidate (local-alloc qty order)
        b = base;
        pt[0].x = x;
        pt[0].z = 0;
        t = b - 4; // before the pt[1] stores: the hard-reg `subi r0` then follows `li r30,0; lfd f0` in LUID order
        pt[1].x = x;
        pt[1].y = b + rows * 0x14 + 4;
        pt[0].y = t;
        pt[1].z = 0;
    }
    TprimDrawFrameFn_s16(pt, &col, 2);
    pt[0].x = 0x3C;
    pt[0].y = curY;
    pt[0].z = 0;
    pt[1].x = 0x1E4;
    pt[1].y = curY;
    pt[1].z = 0;
    TprimDrawFrameFn_s16(pt, &col, 2);
    switch (work->tblType) {
    case 0:
        eprintf2(8, 14, 0x48, 0x194, 0, 0, "[DIST %5.0f : VOL %3d]", work->curDist, work->curVal);
        break;
    case 1:
        eprintf2(8, 14, 0x48, 0x194, 0, 0, "[DIST %5.0f : PITCH %5d]", work->curDist, work->curVal * 100);
        break;
    case 2:
        eprintf2(8, 14, 0x48, 0x194, 0, 0, "[DIST %5.0f : FILTER %s]", work->curDist, filter_name[work->curVal]);
        break;
    }
    eprintf2(8, 14, 0x164, 0x194, 0, 0, "[SCALE : %f m]", tbl->scale / 1000.0f);
    editDataDraw(tbl);
}

static char* efx_help[12][2] = {
    {"DELAY (0.00 - 0.10)", "DELAY (0.00 - 0.10)"},
    {"TIME (0.01 - 10.00)", "TIME (0.01 - 10.00)"},
    {"COLORATION (0.00 - 1.00)", "COLORATION (0.00 - 1.00)"},
    {"DAMPING (0.00 - 1.00)", "DAMPING (0.00 - 1.00)"},
    {"MIX (0.00 - 1.00)", "CROSSTALK (0.00 - 1.00)"},
    {"CORE (0 - 127)", "MIX (0.00 - 1.00)"},
    {"ROOM (0 - 127)", "CORE (0 - 127)"},
    {"ENEMY (0 - 127)", "ROOM (0 - 127)"},
    {"WEAPON (0 - 127)", "ENEMY (0 - 127)"},
    {"DIST (0.0 - )", "WEAPON (0 - 127)"},
    {"DOPPLER (0.0 - )", "DIST (0.0 - ) "},
    {"", "DOPPLER (0.0 - )"},
};

// One reverb parameter step (dir = -1/+1) on the DPL2 (sel 0) or stereo (sel 1) set; R held = 10x step.
// Written out per step size and direction: the original has one switch per (big, dir) pair. Macros, not an
// inline function: an inlined function's pool loads lose RTX_UNCHANGING_P, which gives them an anti-dependence
// on the arm's store and flips sched2's depend-count tie-break (the target issues the field load before the
// pool load in the cross-jumped +=0.01f arms).
#define EFX_SW_DPL2(sel, op, fs, is)                \
    switch (work->efxCur[sel]) {                    \
    case 0: p->preDelay op fs; break;               \
    case 1: p->time op fs; break;                   \
    case 2: p->coloration op fs; break;             \
    case 3: p->damping op fs; break;                \
    case 4: p->mix op fs; break;                    \
    case 5: p->aux_core op is; break;               \
    case 6: p->aux_room op is; break;               \
    case 7: p->aux_em op is; break;                 \
    case 8: p->aux_wep op is; break;                \
    }
#define EFX_SW_ST(sel, op, fs, is)                  \
    switch (work->efxCur[sel]) {                    \
    case 0: p->preDelay op fs; break;               \
    case 1: p->time op fs; break;                   \
    case 2: p->coloration op fs; break;             \
    case 3: p->damping op fs; break;                \
    case 4: p->crosstalk op fs; break;              \
    case 5: p->mix op fs; break;                    \
    case 6: p->aux_core op is; break;               \
    case 7: p->aux_room op is; break;               \
    case 8: p->aux_em op is; break;                 \
    case 9: p->aux_wep op is; break;                \
    }
#define EFX_PARAM_MOVE(SW, sel, op)                 \
    if (Joy[0].on & 0x400) {                        \
        SW(sel, op, 0.1f, 10)                       \
    } else {                                        \
        SW(sel, op, 0.01f, 1)                       \
    }

// Reverb parameter clamps, written out in place (an inlined function's pool loads lose RTX_UNCHANGING_P).
#define EFX_CLAMP_COMMON(p) \
    p->preDelay = p->preDelay < 0.0f ? 0.0f : p->preDelay > 0.1f ? 0.1f : p->preDelay; \
    p->time = p->time < 0.01f ? 0.01f : p->time > 10.0f ? 10.0f : p->time; \
    p->coloration = p->coloration < 0.0f ? 0.0f : p->coloration > 1.0f ? 1.0f : p->coloration; \
    p->damping = p->damping < 0.0f ? 0.0f : p->damping > 1.0f ? 1.0f : p->damping;
#define EFX_CLAMP_AUX(p) \
    { int v = p->aux_core; int e = (s16) v; int r; if (e >= 0) { r = v; if (e > 0x7F) r = 0x7F; } else r = 0; p->aux_core = r; } \
    { int v = p->aux_room; int e = (s16) v; int r; if (e >= 0) { r = v; if (e > 0x7F) r = 0x7F; } else r = 0; p->aux_room = r; } \
    { int v = p->aux_em; int e = (s16) v; int r; if (e >= 0) { r = v; if (e > 0x7F) r = 0x7F; } else r = 0; p->aux_em = r; } \
    { int v = p->aux_wep; int e = (s16) v; int r; if (e >= 0) { r = v; if (e > 0x7F) r = 0x7F; } else r = 0; p->aux_wep = r; }

static void edit_reverb_param()
{
    S16Vec pt[4];
    u32 col;
    int active;
    s16 y;

    eprintf(0x40, 0x28, 0, 0, "[REVERB PARAMETER EDIT]");
    if (Joy[0].trg & 0x200) {
        work->mode = 0;
        work->sub = 0;
        work->step = 0;
        work->x6 = 0;
    } else if (Joy[0].trg & 0x60) {
        work->x29 ^= 1;
    } else if (Joy[0].rep & 0x80008) {
        work->efxCur[work->x29]--;
    } else if (Joy[0].rep & 0x40004) {
        work->efxCur[work->x29]++;
    } else if (Joy[0].rep & 0x10001) {
        if (work->x29 == 0) {
            register SndEfxParam* p asm("r10") = &work->efx[0]; // COMPILER-DIFF: pin (global-alloc order: the target allocates work before p: work r11, p r10, Joy r10)
            EFX_PARAM_MOVE(EFX_SW_DPL2, 0, -=)
        } else {
            register SndEfxParam* p asm("r10") = &work->efx[1]; // COMPILER-DIFF: pin
            EFX_PARAM_MOVE(EFX_SW_ST, 1, -=)
        }
    } else if (Joy[0].rep & 0x20002) {
        if (work->x29 == 0) {
            register SndEfxParam* p asm("r10") = &work->efx[0]; // COMPILER-DIFF: pin
            EFX_PARAM_MOVE(EFX_SW_DPL2, 0, +=)
        } else {
            register SndEfxParam* p asm("r10") = &work->efx[1]; // COMPILER-DIFF: pin
            EFX_PARAM_MOVE(EFX_SW_ST, 1, +=)
        }
    }
    if (work->x29 == 0) {
        work->efxCur[0] = work->efxCur[0] < 0 ? 0 : work->efxCur[0] > 8 ? 8 : work->efxCur[0];
    } else {
        work->efxCur[1] = work->efxCur[1] < 0 ? 0 : work->efxCur[1] > 9 ? 9 : work->efxCur[1];
    }
    {
        SndEfxParam* p = &work->efx[0];

        EFX_CLAMP_COMMON(p);
        p->mix = p->mix < 0.0f ? 0.0f : p->mix > 1.0f ? 1.0f : p->mix;
        EFX_CLAMP_AUX(p);
    }
    {
        SndEfxParam* p = &work->efx[1];

        EFX_CLAMP_COMMON(p);
        p->crosstalk = p->crosstalk < 0.0f ? 0.0f : p->crosstalk > 1.0f ? 1.0f : p->crosstalk;
        p->mix = p->mix < 0.0f ? 0.0f : p->mix > 1.0f ? 1.0f : p->mix;
        EFX_CLAMP_AUX(p);
    }

    // DPL2 panel
    if (work->x29 == 0) {
        col = cursorCol[pG->flags_51E4 % 15];
        active = 1;
    } else {
        col = 0xFFFFFFFF;
        active = 0;
    }
    pt[0].x = 0x118; pt[0].y = 0x48; pt[0].z = 0;
    pt[1].x = 0x1B0; pt[1].y = 0x48; pt[1].z = 0;
    pt[2].x = 0x1B0; pt[2].y = 0x148; pt[2].z = 0;
    pt[3].x = 0x118; pt[3].y = 0x148; pt[3].z = 0;
    y = 0x80;
    // COMPILER-DIFF: launder (cse/cprop fold y = 0x80 into the first row and y += 0x10; the target keeps the y chain)
    asm("" : "+r"(y));
    TprimDrawFrameFn_s16(pt, (GXColor*) &col, 4);
    eprintf(0x120, 0x50, 5, 0, "SURROUND MODE");
    eprintf(0x120, 0x70, 4, 0, "REVERB");
    eprintf(0x120, y, active ? (work->efxCur[0] == 0 ? 6 : 0) : 0, 0, "DELAY       %2.2f", work->efx[0].preDelay);
    y += 0x10;
    eprintf(0x120, y, active ? (work->efxCur[0] == 1 ? 6 : 0) : 0, 0, "TIME        %2.2f", work->efx[0].time);
    y += 0x10;
    eprintf(0x120, y, active ? (work->efxCur[0] == 2 ? 6 : 0) : 0, 0, "COLORATION  %2.2f", work->efx[0].coloration);
    y += 0x10;
    eprintf(0x120, y, active ? (work->efxCur[0] == 3 ? 6 : 0) : 0, 0, "DAMPING     %2.2f", work->efx[0].damping);
    y += 0x10;
    eprintf(0x120, y, active ? (work->efxCur[0] == 4 ? 6 : 0) : 0, 0, "MIX         %2.2f", work->efx[0].mix);
    y += 0x30;
    eprintf(0x120, y, 4, 0, "AUX A");
    y += 0x10;
    eprintf(0x120, y, active ? (work->efxCur[0] == 5 ? 6 : 0) : 0, 0, "CORE          %3d", (s16) work->efx[0].aux_core);
    y += 0x10;
    eprintf(0x120, y, active ? (work->efxCur[0] == 6 ? 6 : 0) : 0, 0, "ROOM          %3d", (s16) work->efx[0].aux_room);
    y += 0x10;
    eprintf(0x120, y, active ? (work->efxCur[0] == 7 ? 6 : 0) : 0, 0, "ENEMY         %3d", (s16) work->efx[0].aux_em);
    y += 0x10;
    eprintf(0x120, y, active ? (work->efxCur[0] == 8 ? 6 : 0) : 0, 0, "WEAPON        %3d", (s16) work->efx[0].aux_wep);

    // stereo panel
    if (work->x29 == 1) {
        col = cursorCol[pG->flags_51E4 % 15];
        active = 1;
    } else {
        col = 0xFFFFFFFF;
        active = 0;
    }
    {
        u32 c;
        if (work->x29 == 1) {
            c = cursorCol[pG->flags_51E4 % 15];
        } else {
            c = 0xFFFFFFFF;
        }
        col = c;
    }
    pt[0].x = 0x58; pt[0].y = 0x48; pt[0].z = 0;
    pt[1].x = 0xF0; pt[1].y = 0x48; pt[1].z = 0;
    pt[2].x = 0xF0; pt[2].y = 0x148; pt[2].z = 0;
    pt[3].x = 0x58; pt[3].y = 0x148; pt[3].z = 0;
    y = 0x80;
    // COMPILER-DIFF: launder (cse/cprop fold y = 0x80 into the first row and y += 0x10; the target keeps the y chain)
    asm("" : "+r"(y));
    TprimDrawFrameFn_s16(pt, (GXColor*) &col, 4);
    eprintf(0x60, 0x50, 5, 0, "STEREO MODE");
    eprintf(0x60, 0x70, 4, 0, "REVERB");
    eprintf(0x60, y, active ? (work->efxCur[1] == 0 ? 6 : 0) : 0, 0, "DELAY       %2.2f", work->efx[1].preDelay);
    y += 0x10;
    eprintf(0x60, y, active ? (work->efxCur[1] == 1 ? 6 : 0) : 0, 0, "TIME        %2.2f", work->efx[1].time);
    y += 0x10;
    eprintf(0x60, y, active ? (work->efxCur[1] == 2 ? 6 : 0) : 0, 0, "COLORATION  %2.2f", work->efx[1].coloration);
    y += 0x10;
    eprintf(0x60, y, active ? (work->efxCur[1] == 3 ? 6 : 0) : 0, 0, "DAMPING     %2.2f", work->efx[1].damping);
    y += 0x10;
    eprintf(0x60, y, active ? (work->efxCur[1] == 4 ? 6 : 0) : 0, 0, "CROSSTALK   %2.2f", work->efx[1].crosstalk);
    y += 0x10;
    eprintf(0x60, y, active ? (work->efxCur[1] == 5 ? 6 : 0) : 0, 0, "MIX         %2.2f", work->efx[1].mix);
    y += 0x20;
    eprintf(0x60, y, 4, 0, "AUX A");
    y += 0x10;
    eprintf(0x60, y, active ? (work->efxCur[1] == 6 ? 6 : 0) : 0, 0, "CORE          %3d", (s16) work->efx[1].aux_core);
    y += 0x10;
    eprintf(0x60, y, active ? (work->efxCur[1] == 7 ? 6 : 0) : 0, 0, "ROOM          %3d", (s16) work->efx[1].aux_room);
    y += 0x10;
    eprintf(0x60, y, active ? (work->efxCur[1] == 8 ? 6 : 0) : 0, 0, "ENEMY         %3d", (s16) work->efx[1].aux_em);
    y += 0x10;
    eprintf(0x60, y, active ? (work->efxCur[1] == 9 ? 6 : 0) : 0, 0, "WEAPON        %3d", (s16) work->efx[1].aux_wep);
    y += 0x30;
    eprintf(0x58, y, 0, 0, "%s", efx_help[work->efxCur[work->x29]][work->x29]);
}

void combine_tbl_disp(CombSel* sel)
{
    int i;
    int j;
    int ybase;
    int w = 0x6E;
    u32 col[3];
    u8 c[3];

    if (work->sub == 2) {
        eprintf(0xA5, 0x58, 0, 0, "SET %2d", work->copySrc);
        eprintf(0xA5, 0xFC, 0, 0, "SET %2d", work->copyDst);
        for (i = 0; i < 4; i++) {
            CombSel* s = i <= 1 ? &work->sel[work->copySrc] : &work->sel[work->copyDst];
            if (s->used != 0) {
                int yb = 0x6A + i * 0x48;
                int y = yb + (i / 2) * 0x14;

                work->curTbl = work->vol;
                ListDraw(0xA5, y, 0xFFFFFFFF, s->vol[i % 2], 0x40);
                work->curTbl = work->pitch;
                ListDraw(0xA5 + w, y, 0xFFFFFFFF, s->pitch[i % 2], 0x41);
                work->curTbl = work->filter;
                ListDraw(0xA5 + w * 2, y, 0xFFFFFFFF, s->filter[i % 2], 0x42);
            }
        }
    } else if (sel->used != 0) {
        eprintf(0x129, 0x38, 0, 0, "STEREO  DPL2");
        eprintf(0xE1, 0x5A, 0, 0, "VOLUME");
        eprintf(0xE1, 0x6C, 0, 0, "PITCH");
        eprintf(0xE1, 0x7E, 0, 0, "FILTER");
        ybase = 0xBA;
        for (i = 1; i >= 0; i--) {
            for (j = 0; j < 3; j++) {
                col[j] = 0xFFFFFFFF;
                c[j] = 0;
                if (work->sub == 1 && work->x29 == i && work->efxCur[i] == j) {
                    col[j] = cursorCol[pG->flags_51E4 % 15];
                    c[j] = 6;
                }
            }
            eprintf(i * 64 + 0x139, 0x5A, c[0], 0, "%2d", sel->vol[i]);
            work->curTbl = work->vol;
            eprintf(0xA5, ybase - 0x12, 0, 0, "VOLUME TBL");
            int y = i * 0x4B + ybase;

            ListDraw(0xA5, y, col[0], sel->vol[i], 0x40);
            eprintf(i * 64 + 0x139, 0x6C, c[1], 0, "%2d", sel->pitch[i]);
            work->curTbl = work->pitch;
            eprintf(0xA5 + w, ybase - 0x12, 0, 0, "PITCH TBL");
            ListDraw(0xA5 + w, y, col[1], sel->pitch[i], 0x41);
            eprintf(i * 64 + 0x139, 0x7E, c[2], 0, "%2d", sel->filter[i]);
            work->curTbl = work->filter;
            eprintf(0xA5 + w * 2, ybase - 0x12, 0, 0, "FILTER TBL");
            ListDraw(0xA5 + w * 2, y, col[2], sel->filter[i], 0x42);
        }
    }
}

static void combine_tbl_select()
{
    if (Joy[0].rep2 & 0x80008) {
        if (work->cur % 16 != 0) {
            work->cur--;
        }
    } else if (Joy[0].rep2 & 0x40004) {
        if (work->cur % 16 != 15) {
            work->cur++;
        }
    } else if (Joy[0].rep2 & 0x10001) {
        if (work->cur / 16 != 0) {
            work->cur -= 16;
        }
    } else if (Joy[0].rep2 & 0x20002) {
        if (work->cur / 16 == 0) {
            work->cur += 16;
        }
    } else if (Joy[0].trg & 0x100) {
        work->selBackup = work->sel[work->cur];
        work->sel[work->cur].used = 1;
        work->sub = 1;
        work->step = 0;
        work->x6 = 0;
    } else if (Joy[0].trg & 0x800) {
        if (work->sel[work->cur].used != 0) {
            work->copySrc = work->copyDst = work->cur;
            work->sub = 2;
            work->step = 0;
            work->x6 = 0;
        }
    } else if (Joy[0].trg & 0x10) {
        if (work->sel[work->cur].used != 0) {
            work->sub = 3;
            work->step = 0;
            work->x6 = 0;
        }
    } else if (Joy[0].trg & 0x200) {
        work->mode = 0;
        work->sub = 0;
        work->step = 0;
        work->x6 = 0;
    }
    eprintf(0x15E, 0x15C, 0, 0, "Y ... DATA COPY");
    eprintf(0x15E, 0x16C, 0, 0, "Z ... DATA DELETE");
    eprintf(0x15E, 0x17C, 0, 0, "A ... EDIT");
    eprintf(0x15E, 0x18C, 0, 0, "B ... RETURN MENU");
}

static void combine_tbl_edit()
{
    CombSel* sel = &work->sel[work->cur];

    if (Joy[0].trg & 0x20) {
        work->x29 = 1;
    } else if (Joy[0].trg & 0x40) {
        work->x29 = 0;
    } else if (Joy[0].trg & 0x80008) {
        work->efxCur[work->x29]--;
    } else if (Joy[0].trg & 0x40004) {
        work->efxCur[work->x29]++;
    } else if (Joy[0].rep2 & 0x10001) {
        switch (work->efxCur[work->x29]) {
        case 0:
            sel->vol[work->x29]--;
            sel->vol[work->x29] = sel->vol[work->x29] < -1 ? -1 : sel->vol[work->x29] > 31 ? 31 : sel->vol[work->x29];
            break;
        case 1:
            sel->pitch[work->x29]--;
            sel->pitch[work->x29] = sel->pitch[work->x29] < -1 ? -1 : sel->pitch[work->x29] > 31 ? 31 : sel->pitch[work->x29];
            break;
        case 2:
            sel->filter[work->x29]--;
            sel->filter[work->x29] = sel->filter[work->x29] < -1 ? -1 : sel->filter[work->x29] > 31 ? 31 : sel->filter[work->x29];
            break;
        }
    } else if (Joy[0].rep2 & 0x20002) {
        switch (work->efxCur[work->x29]) {
        case 0:
            sel->vol[work->x29]++;
            sel->vol[work->x29] = sel->vol[work->x29] < -1 ? -1 : sel->vol[work->x29] > 31 ? 31 : sel->vol[work->x29];
            break;
        case 1:
            sel->pitch[work->x29]++;
            sel->pitch[work->x29] = sel->pitch[work->x29] < -1 ? -1 : sel->pitch[work->x29] > 31 ? 31 : sel->pitch[work->x29];
            break;
        case 2:
            sel->filter[work->x29]++;
            sel->filter[work->x29] = sel->filter[work->x29] < -1 ? -1 : sel->filter[work->x29] > 31 ? 31 : sel->filter[work->x29];
            break;
        }
    } else if (Joy[0].trg & 0x100) {
        work->sub = 0;
        work->step = 0;
        work->x6 = 0;
    } else if (Joy[0].trg & 0x200) {
        work->sel[work->cur] = work->selBackup;
        work->sub = 0;
        work->step = 0;
        work->x6 = 0;
    }
    work->efxCur[work->x29] = work->efxCur[work->x29] < 0 ? 0 : work->efxCur[work->x29] > 2 ? 2 : work->efxCur[work->x29];
    eprintf(0x12E, 0x15C, 0, 0, "L,R ... STEREO <-> DPL2");
    eprintf(0x12E, 0x17C, 0, 0, "A ..... SET EDIT DATA");
    eprintf(0x12E, 0x18C, 0, 0, "B ..... CANCEL");
}

static void combine_tbl_copy()
{
    eprintf(0xF0, 0x1A, 0, 0, "DATA COPY");
    eprintf(0xF0, 0x2A, 0, 0, "[SET %2d] -> [SET %2d]", work->copySrc, work->copyDst);
    switch (work->step) {
    case 0:
        if (Joy[0].rep2 & 0x80008) {
            if (work->copyDst % 16 != 0) {
                work->copyDst--;
            }
        } else if (Joy[0].rep2 & 0x40004) {
            if (work->copyDst % 16 != 15) {
                work->copyDst++;
            }
        } else if (Joy[0].rep2 & 0x10001) {
            if (work->copyDst / 16 != 0) {
                work->copyDst -= 16;
            }
        } else if (Joy[0].rep2 & 0x20002) {
            if (work->copyDst / 16 == 0) {
                work->copyDst += 16;
            }
        } else if (Joy[0].trg & 0x100) {
            if (work->copySrc != work->copyDst) {
                work->step++;
            }
        } else if (Joy[0].trg & 0x200) {
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        }
        work->copyDst = work->copyDst < 0 ? 0 : work->copyDst > 31 ? 31 : work->copyDst;
        break;
    case 1:
        eprintf(0xF0, 0x3A, 0, 0, "DATA COPY OK?");
        eprintf(0x160, 0x3A, 0, 0, "[   /  ]");
        eprintf(0x168, 0x3A, work->yesno == 0 ? 6 : 7, 0, "YES");
        eprintf(0x188, 0x3A, work->yesno == 1 ? 6 : 7, 0, "NO");
        if (Joy[0].trg & 0x30003) {
            work->yesno ^= 1;
        } else if (Joy[0].trg & 0x100) {
            if (work->yesno == 0) {
                memcpy(&work->sel[work->copyDst], &work->sel[work->copySrc], sizeof(CombSel));
                work->step++;
                work->timer = 30;
            } else {
                work->step--;
            }
        } else if (Joy[0].trg & 0x200) {
            work->step--;
        }
        break;
    case 2:
        eprintf(0xF0, 0x3A, 6, 0, "DATA COPY COMPLETE.");
        if ((Joy[0].trg & 0x300) || work->timer <= 0) {
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        }
        work->timer--;
        break;
    }
}

static void combine_tbl_delete()
{
    switch (work->step) {
    case 0:
        eprintf(0x28, 0x15C, 0, 0, "[DATA %2d] DELETE OK? [   /  ]", work->cur);
        eprintf(0xD8, 0x15C, work->yesno == 0 ? 6 : 7, 0, "YES");
        eprintf(0xF8, 0x15C, work->yesno == 1 ? 6 : 7, 0, "NO");
        if (Joy[0].trg & 0x30003) {
            work->yesno ^= 1;
        } else if (Joy[0].trg & 0x100) {
            if (work->yesno == 0) {
                memclr_asm(&work->sel[work->cur], sizeof(CombSel));
                work->step++;
                work->timer = 30;
            } else {
                work->sub = 0;
                work->step = 0;
                work->x6 = 0;
            }
        } else if (Joy[0].trg & 0x200) {
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        }
        break;
    case 1:
        eprintf(0x28, 0x15C, 6, 0, "DATA DELETE COMPLETE.");
        if ((Joy[0].trg & 0x300) || work->timer <= 0) {
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        }
        work->timer--;
        break;
    }
    eprintf(0x186, 0x15C, 0, 0, "A ... DECIDE");
    eprintf(0x186, 0x16C, 0, 0, "B ... CANCEL");
}

static void (*combine_func[4])() = {combine_tbl_select, combine_tbl_edit, combine_tbl_copy, combine_tbl_delete};

static void edit_combine_tbl()
{
    int i;
    int j;
    int col;

    eprintf(0x28, 0x28, 0, 0, "%s", "COMBINE TABLE EDIT");
    combine_func[work->sub]();
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 2; j++) {
            if (work->cur == i + j * 16) {
                col = 6;
            } else if (work->sub == 2 && work->copyDst == i + j * 16) {
                col = 5;
            } else {
                col = 0;
            }
            eprintf(0x28 + j * 0x40, i * 16 + 0x48, col, 0, "SET %2d", i + j * 16);
        }
    }
    combine_tbl_disp(&work->sel[work->cur]);
}

static void file_save()
{
    u8* p;
    u32 ofs;
    int size = 0;
    int i;
    int n;
    CombSel* s;

    eprintf(0x40, 0x28, 0, 0, "[DATA SAVE]");
    eprintf(0x40, 0x60, 0, 0, "SELECT SAVE FILE");
    eprintf(0x40, 0x80, 0, 0, "LOCAL  :");
    eprintf(0x88, 0x80, work->dest == 0 ? 6 : 7, 0, "d:/bio4/room/snd/r%03x.stb", pG->room_id);
    eprintf(0x40, 0x90, 0, 0, "SERVER :");
    eprintf(0x88, 0x90, work->dest == 1 ? 6 : 7, 0, "x:/soft/room/snd/r%03x.stb", pG->room_id);
    switch (work->sub) {
    case 0:
        if (Joy[0].trg & 0xC000C) {
            work->dest ^= 1;
        } else if (Joy[0].trg & 0x200) {
            work->mode = 0;
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        } else if (Joy[0].trg & 0x100) {
            work->sub = 1;
            work->step = 0;
            work->x6 = 0;
            if (work->dest == 0) {
                sprintf(work->path, "d:/bio4/room/snd/r%03x.stb", pG->room_id);
            } else {
                sprintf(work->path, "x:/soft/room/snd/r%03x.stb", pG->room_id);
            }
            work->yesno = 1;
        }
        break;
    case 1:
        eprintf(0x40, 0xB0, 0, 0, "DATA SAVE OK?");
        eprintf(0x40, 0xC0, work->yesno == 0 ? 6 : 7, 0, "YES");
        eprintf(0x68, 0xC0, work->yesno == 1 ? 6 : 7, 0, "NO");
        if (Joy[0].trg & 0x200) {
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        } else if (Joy[0].trg & 0x100) {
            if (work->yesno == 0) {
                work->sub = 2;
                work->step = 0;
                work->x6 = 0;
            } else {
                work->sub = 0;
                work->step = 0;
                work->x6 = 0;
            }
        } else if (Joy[0].trg & 0x30003) {
            work->yesno ^= 1;
        }
        break;
    case 2: {
        SndRoomHdr* hdr;

        p = work->fileBuf;
        hdr = (SndRoomHdr*) p;
        hdr->efx[0] = work->efx[0];
        hdr->efx[1] = work->efx[1];
        ofs = sizeof(SndRoomHdr);
        for (i = 0; i < 32; i++) {
            s = &work->sel[i];

            if (s->used != 0) {
                hdr->curve_sel[i] = ofs;
                ofs += sizeof(CombSel);
            } else {
                hdr->curve_sel[i] = 0;
            }
        }
        for (i = 0; i < 32; i++) {
            EditTbl* t = &work->vol[i];

            if (t->num != 0) {
                hdr->vol_ofs[i] = ofs;
                ofs += 8 + t->num * 8;
            } else {
                hdr->vol_ofs[i] = 0;
            }
        }
        for (i = 0; i < 32; i++) {
            EditTbl* t = &work->pitch[i];

            if (t->num != 0) {
                hdr->pitch_ofs[i] = ofs;
                ofs += 8 + t->num * 8;
            } else {
                hdr->pitch_ofs[i] = 0;
            }
        }
        for (i = 0; i < 32; i++) {
            EditTbl* t = &work->filter[i];

            if (t->num != 0) {
                hdr->filter_ofs[i] = ofs;
                ofs += 8 + t->num * 8;
            } else {
                hdr->filter_ofs[i] = 0;
            }
        }
        n = sizeof(SndRoomHdr);
        memcpy(p, hdr, n);
        p += n;
        size = n;
        for (i = 0; i < 32; i++) {
            s = &work->sel[i];

            if (s->used != 0) {
                n = sizeof(CombSel);
                memcpy(p, s, n);
                p += n;
                size += n;
            }
        }
        for (i = 0; i < 32; i++) {
            EditTbl* t = &work->vol[i];

            if (t->num != 0) {
                n = t->num * 8 + 8;
                memcpy(p, t, n);
                p += n;
                size += n;
            }
        }
        for (i = 0; i < 32; i++) {
            EditTbl* t = &work->pitch[i];

            if (t->num != 0) {
                n = t->num * 8 + 8;
                memcpy(p, t, n);
                p += n;
                size += n;
            }
        }
        for (i = 0; i < 32; i++) {
            EditTbl* t = &work->filter[i];

            if (t->num != 0) {
                n = t->num * 8 + 8;
                memcpy(p, t, n);
                p += n;
                size += n;
            }
        }
        n = HDWrite_only(work->path, work->fileBuf, size);
        work->timer = 30;
        if (n != 0) {
            work->sub = 3;
            work->step = 0;
            work->x6 = 0;
        } else {
            work->sub = 4;
            work->step = 0;
            work->x6 = 0;
        }
        break;
    }
    case 3:
        eprintf(0x40, 0xB0, 6, 0, "DATA SAVE COMPLETE.");
        if ((Joy[0].trg & 0x300) || work->timer <= 0) {
            work->mode = 0;
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        }
        work->timer--;
        break;
    case 4:
        eprintf(0x40, 0xB0, 6, 0, "DATA SAVE ERROR.");
        if ((Joy[0].trg & 0x300) || work->timer <= 0) {
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        }
        work->timer--;
        break;
    }
}

static void file_load()
{
    int ret;

    eprintf(0x40, 0x28, 0, 0, "[DATA LOAD]");
    eprintf(0x40, 0x60, 0, 0, "SELECT LOAD FILE");
    switch (work->sub) {
    case 0:
        work->sub = 1;
        work->step = 0;
        work->x6 = 0;
        work->dest = 1;
        work->stage = pG->stage_no;
        work->room = pG->room_no;
    case 1:
        if (Joy[0].rep & 0x80008) {
            switch (work->loadCur) {
            case 0:
                work->dest ^= 1;
                break;
            case 1:
                work->stage++;
                break;
            case 2:
                work->room++;
                break;
            }
        } else if (Joy[0].rep & 0x40004) {
            switch (work->loadCur) {
            case 0:
                work->dest ^= 1;
                break;
            case 1:
                work->stage--;
                break;
            case 2:
                work->room--;
                break;
            }
        } else if (Joy[0].trg & 0x10001) {
            work->loadCur--;
        } else if (Joy[0].trg & 0x20002) {
            work->loadCur++;
        } else if (Joy[0].trg & 0x200) {
            work->mode = 0;
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        } else if (Joy[0].trg & 0x100) {
            work->sub = 2;
            work->step = 0;
            work->x6 = 0;
            work->yesno = 1;
        }
        work->loadCur = work->loadCur < 0 ? 0 : work->loadCur > 2 ? 2 : work->loadCur;
        work->stage = work->stage < 0 ? 0 : work->stage > 9 ? 9 : work->stage;
        work->room = work->room < 0 ? 0 : work->room;
        if (work->dest == 0) {
            sprintf(work->path, "d:/bio4/room/snd/r%d%02x.stb", work->stage, work->room);
        } else {
            sprintf(work->path, "x:/soft/room/snd/r%d%02x.stb", work->stage, work->room);
        }
        break;
    case 2:
        eprintf(0x40, 0xC0, 0, 0, "DATA LOAD OK?");
        eprintf(0x40, 0xD0, work->yesno == 0 ? 6 : 7, 0, "YES");
        eprintf(0x68, 0xD0, work->yesno == 1 ? 6 : 7, 0, "NO");
        if (Joy[0].trg & 0x200) {
            work->sub = 1;
            work->step = 0;
            work->x6 = 0;
        } else if (Joy[0].trg & 0x100) {
            if (work->yesno == 0) {
                work->sub = 3;
                work->step = 0;
                work->x6 = 0;
            } else {
                work->sub = 1;
                work->step = 0;
                work->x6 = 0;
            }
        } else if (Joy[0].trg & 0x30003) {
            work->yesno ^= 1;
        }
        break;
    case 3:
        ret = HDRead(work->path, work->fileBuf);
        if (ret == 0) {
            pLog->err(0, 0, "%s : LOAD ERROR !!!!", work->path);
        } else {
            getInfoData((SndRoomHdr*) work->fileBuf);
        }
        work->timer = 30;
        if (ret != 0) {
            work->sub = 4;
            work->step = 0;
            work->x6 = 0;
        } else {
            work->sub = 5;
            work->step = 0;
            work->x6 = 0;
        }
        break;
    case 4:
        eprintf(0x40, 0xC0, 6, 0, "DATA LOAD COMPLETE.");
        if ((Joy[0].trg & 0x300) || work->timer <= 0) {
            work->mode = 0;
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        }
        work->timer--;
        break;
    case 5:
        eprintf(0x40, 0xC0, 6, 0, "DATA LOAD ERROR.");
        if ((Joy[0].trg & 0x300) || work->timer <= 0) {
            work->sub = 0;
            work->step = 0;
            work->x6 = 0;
        }
        work->timer--;
        break;
    }
    {
        int col;
        if (work->sub == 1) {
            col = work->loadCur == 0 ? 6 : 0;
        } else {
            col = 0;
            // COMPILER-DIFF: launder (combine drops the (u8) clrlwi because every set of col is a constant)
            asm("" : "+r"(col));
        }
        eprintf(0x40, 0x80, (u8) col, 0, "%s", work->dest == 0 ? "LOCAL" : "SERVER");
    }
    eprintf(0x80, 0x80, work->sub == 1 ? (work->loadCur == 1 ? 6 : 0) : 0, 0, "STAGE %2d", work->stage);
    eprintf(0xD0, 0x80, work->sub == 1 ? (work->loadCur == 2 ? 6 : 0) : 0, 0, "ROOM %02x", work->room);
    eprintf(0x40, 0xA0, 0, 0, "%s", work->path);
}

static void (*mode_func[8])() = {edit_menu, data_select, data_edit, edit_reverb_param, edit_combine_tbl,
                                 file_load, file_save, NULL};

void ToolSndVolEdit()
{
    init();
    for (;;) {
        TprimDraw2D(0);
        mainFrameDisp();
        eprintf(0x28, 0x10, 4, 0, "SOUND TABLE EDITOR");
        mode_func[work->mode]();
        TaskSleep(1);
    }
}
