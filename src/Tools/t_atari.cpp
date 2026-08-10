#include "types.h"
#include "atari.h"
#include "light.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "camera.h"
#include "dbmodule.h"
#include "main_mem.h"
#include "rnd.h"
#include "t_util.h"

// Scenario collision editor (Tools/t_atari.cpp): walks the polygons of the room (SatMgr) or effect
// (EatMgr) collision set, shows their attributes and lets a sphere / a line probe the collision.

struct AtariToolWork {
    cSat* sat;        // 0x000  piece being edited (satTbl0 / satTbl1)
    int camMode;      // 0x004  bit0: the pad drives the debug camera (Joy[1] = Joy[0])
    int x8;           // 0x008
    JOY joy;          // 0x00C  copy of Joy[0] (cleared in camera mode)
    JOY joy2;         // 0x274
    int mode;         // 0x4DC  routine index (atFunc)
    int plMode;       // 0x4E0  plmove sub routine (plFunc)
    int x4E4;         // 0x4E4
    int x4E8;         // 0x4E8
    int polyNo;       // 0x4EC
    int x4F0;         // 0x4F0
    int cursor;       // 0x4F4  menu cursor / edited vertex
    int x4F8;         // 0x4F8
    int x4FC;         // 0x4FC
    int x500;         // 0x500
    int x504;         // 0x504
    int x508;         // 0x508
    int x50C;         // 0x50C
    int x510;         // 0x510
    u32 count;        // 0x514  frame counter
    u32 editSub;      // 0x518  edit display group (0..4)
    int attrNo;       // 0x51C  attribute name shown (attrName)
    int satSel;       // 0x520  0: SatMgr set (satTbl0), 1: EatMgr set (satTbl1)
    u16 nA;           // 0x524
    u16 nB;           // 0x526
    u16 nC;           // 0x528
    u16 pad_52A;
    Vec pos;          // 0x52C  probe sphere position
};

static const char* attrName[32] = {
    "NORMAL MODE",   "1",              "2",           "3",           "4",           "5",
    "6",             "7",              "8",           "9",           "10",          "11",
    "G0 WATER",      "G1 SMALL NO HIT", "G2 SPECIAL", "G3 SPECIAL2", "G4 SPECIAL3", "G5",
    "G6",            "G7",             "R0 EM NO HIT", "R1 PL NO HIT", "R2 SEE THROUGH", "R3 UP 1m",
    "R4 DOWN 1m",    "R5 OBJ NO HIT",  "R6 STEP",     "R7",          "R8",          "R9",
    "R0",            "R1",
};

static cSat satTbl0[10];
static cSat satTbl1[10];
static AtariToolWork atWork;

void ToolAtari();
void init(AtariToolWork* w);
static void menu(AtariToolWork* w);
static void edit(AtariToolWork* w);
static void wk_edit(AtariToolWork* w);
static void plmove(AtariToolWork* w);
static void plmove00(AtariToolWork* w);
static void plmove10(AtariToolWork* w);
static void hitcheck(AtariToolWork* w);
static void load(AtariToolWork* w);
static void save(AtariToolWork* w);
static void option(AtariToolWork* w);
static void quit(AtariToolWork* w);
void draw_pl_pos(AtariToolWork* w);
void set_at(AtariToolWork* w, cSat* sat);
void clear_pad(AtariToolWork* w);

static void (*atFunc[])(AtariToolWork*) = {
    menu, edit, wk_edit, plmove, hitcheck, load, save, option, quit,
};

void ToolAtari()
{
    AtariToolWork* w = &atWork;

    init(w);
    for (;;) {
        w->joy = Joy[0];
        eprintf(16, 0, 4, 0, "SCROLL ATARI SET TOOL");
        if (w->joy.trg & 0x1000) {
            w->camMode ^= 1;
        }
        if (w->camMode & 1) {
            Joy[1] = Joy[0];
            clear_pad(w);
            if ((w->count & 0xF) > 4) {
                eprintf(280, 14, 0, 0, "CAMERA MODE");
            }
        }
        if (w->joy.trg & 0x800) {
            if (w->satSel == 0) {
                w->sat = satTbl1;
                w->satSel = 1;
            } else {
                w->satSel = 0;
                w->sat = satTbl0;
            }
            set_at(w, w->sat);
            w->polyNo = 0;
        }
        if (w->joy.rep & 0x200000) {
            w->attrNo = (w->attrNo + 1) & 0x1F;
        }
        if (w->joy.rep & 0x100000) {
            w->attrNo = (w->attrNo + 31) & 0x1F;
        }
        atFunc[w->mode](w);
        w->count++;
        if (w->sat->vtx) {
            int flag;

            w->sat->disp(w->polyNo, 0x808080FF, 1);
            flag = (w->satSel << 4) | 0x20;
            if (w->editSub != 4) {
                flag += w->editSub;
            }
            flag += w->attrNo << 8;
            switch (w->satSel) {
            case 0:
                SatMgr.disp(flag);
                break;
            case 1:
                EatMgr.disp(flag);
                break;
            }
        }
        Draw_floor(1000, 10, 0x00202020);
        CameraMove();
        TaskSleep(1);
    }
}

static const char* menuName[8] = {
    "EDIT", "WK EDIT", "PL MOVE", "HIT CHECK", "LOAD", "SAVE", "OPTION", "EXIT",
};

void init(AtariToolWork* w)
{
    TaskSuspend(0);
    w->x50C = 0;
    w->x508 = 0;
    w->x504 = 0;
    w->x500 = 0;
    w->x4FC = 0;
    w->x4F8 = 0;
    w->cursor = 0;
    w->x4E8 = 0;
    w->x4E4 = 0;
    w->plMode = 0;
    w->mode = 0;
    w->editSub = 0;
    w->satSel = 0;
    w->count = 0;
    set_at(w, satTbl0);
    TOOL_FLAG(OFS_DISP_FLG) |= 0x08000000;
    TaskSleep(4);
    w->polyNo = 0;
    w->x4F0 = 0;
}

static void menu(AtariToolWork* w)
{
    int i;

    for (i = 0; i < 8; i++) {
        eprintf(16, 48 + i * 16, i == w->cursor ? 0 : 7, 0, menuName[i]);
    }
    if ((w->joy.rep & 8) && w->cursor != 0) {
        w->cursor--;
    } else if ((w->joy.rep & 4) && w->cursor < 7) {
        w->cursor++;
    }
    if (w->joy.trg & 0x100) {
        w->mode = w->cursor + 1;
        w->x4E8 = 0;
        w->x4E4 = 0;
        w->plMode = 0;
        w->x500 = 0;
        w->x4FC = 0;
        w->x4F8 = 0;
        w->cursor = 0;
    } else if (w->joy.trg & 0x200) {
        w->cursor = 7;
    }
}

static void edit(AtariToolWork* w)
{
    cSat* s = w->sat;
    Vec* vtx = s->vtx;
    AtPoly* poly = s->poly;
    u32 attr;
    Vec* nrm;
    Vec* pv;
    u16 idx;
    Vec v;
    Vec v2;
    u8 r;

    eprintf(16, 32, 4, 0, "EDIT");
    eprintf(400, 14, 0, 0, w->satSel ? "EFFECT ATARI" : "SCROLL ATARI");
    eprintf(400, 28, 0, 0, "POLY %d/%d", w->polyNo, s->nPoly);
    switch (w->editSub) {
    case 0:
        eprintf(504, 32, 0, 0, "A");
        break;
    case 1:
        eprintf(504, 32, 0, 0, "F");
        break;
    case 2:
        eprintf(504, 32, 0, 0, "S");
        break;
    case 3:
        eprintf(504, 32, 0, 0, "W");
        break;
    case 4:
        eprintf(504, 32, 0, 0, "-");
        break;
    }
    eprintf(400, 48, 0, 0, "F:%d S:%d W:%d", w->nA, w->nB, w->nC);
    eprintf(400, 64, 0, 0, "V:%d %d %d %d", poly[w->polyNo].v[0], poly[w->polyNo].v[1], poly[w->polyNo].v[2],
            poly[w->polyNo].n);
    eprintf(400, 96, 0, 0, "%08x", (poly[w->polyNo].e[0] << 16) | poly[w->polyNo].e[1]);
    attr = Get_poly_attr(&poly[w->polyNo]);
    eprintf(400, 112, attr & 0x4000 ? 0 : 7, 0, "EM_HOHIT");
    eprintf(400, 128, attr & 0x400000 ? 0 : 7, 0, "PL_NOHIT");
    eprintf(400, 144, attr & 0x800000 ? 0 : 7, 0, "SEE_NOHIT");
    eprintf(400, 160, attr & 0x200000 ? 0 : 7, 0, "UP");
    eprintf(400, 176, attr & 0x2000 ? 0 : 7, 0, "DOWN");
    eprintf(400, 192, 7, 0, "WATER");
    eprintf(400, 208, attr & 0x8000 ? 0 : 7, 0, "SMALL_NOHIT");
    eprintf(400, 224, 7, 0, "SPECIAL");
    eprintf(400, 240, attr & 0x40 ? 0 : 7, 0, "ROUTE_NOHIT");
    eprintf(400, 256, attr & 0x80 ? 0 : 7, 0, "STEPS");
    eprintf(400, 272, attr & 0x400 ? 0 : 7, 0, "ONLY CAM HIT");
    eprintf(400, 288, 0, 0, attrName[w->attrNo]);
    if ((w->count & 7) <= 2) {
        draw_pl_pos(w);
    }
    idx = *(u16*) (w->cursor * 2 + (u32) &poly[w->polyNo]);
    nrm = s->nrm;
    pv = (Vec*) (idx * sizeof(Vec) + (u32) vtx);
    v.x = pv->x;
    v.y = pv->y;
    v.z = pv->z;
    eprintf(376, 384, 0, 0, "X:%5.0f", v.x);
    eprintf(376, 400, 0, 0, "Y:%5.0f", v.y);
    eprintf(376, 416, 0, 0, "Z:%5.0f", v.z);
    eprintf(440, 384, 0, 0, "X:%2.2f", nrm[w->polyNo].x);
    eprintf(440, 400, 0, 0, "Y:%2.2f", nrm[w->polyNo].y);
    eprintf(440, 416, 0, 0, "Z:%2.2f", nrm[w->polyNo].z);
    v2.x = v.x + nrm[poly[w->polyNo].n].x * 3000.0f;
    v2.y = v.y + nrm[poly[w->polyNo].n].y * 3000.0f;
    v2.z = v.z + nrm[poly[w->polyNo].n].z * 3000.0f;
    r = Rnd() % 20;
    v2.x += r;
    r = Rnd() % 20;
    v2.y += r;
    r = Rnd() % 20;
    v2.z += r;
    Draw_line3d(&v, &v2, 0xFEFF0000, 0);
    if (w->joy.rep & 2) {
        if (w->polyNo < w->sat->nPoly - 1) {
            w->polyNo++;
        } else {
            w->polyNo = 0;
        }
    } else if (w->joy.rep & 1) {
        if (w->polyNo != 0) {
            w->polyNo--;
        } else {
            w->polyNo = w->sat->nPoly - 1;
        }
    } else if (w->joy.rep & 8) {
        w->cursor = (w->cursor + 4) % 3;
    } else if (w->joy.rep & 4) {
        w->cursor = (w->cursor + 2) % 3;
    }
    // wk_edit's body repeated (the original is not inlined: its out-of-line copy follows this function)
    if (w->joy.trg & 0x400) {
    } else if (w->joy.trg & 0x800) {
    } else if (w->joy.trg & 0x100) {
    } else if (w->joy.trg & 0x200) {
        w->mode = 0;
        w->plMode = 0;
    } else if (w->joy.trg & 0x10) {
        w->editSub = (w->editSub + 1) % 5;
    }
}

static void wk_edit(AtariToolWork* w)
{
    if (w->joy.trg & 0x400) {
    } else if (w->joy.trg & 0x800) {
    } else if (w->joy.trg & 0x100) {
    } else if (w->joy.trg & 0x200) {
        w->mode = 0;
        w->plMode = 0;
    } else if (w->joy.trg & 0x10) {
        w->editSub = (w->editSub + 1) % 5;
    }
}

static void (*plFunc[])(AtariToolWork*) = {
    plmove00, plmove10,
};

static void plmove(AtariToolWork* w)
{
    eprintf(16, 16, 4, 0, "PL MOVE");
    plFunc[w->plMode](w);
    if (w->joy.trg & 0x10) {
        w->mode = 0;
        w->plMode = 0;
    }
}

static void plmove00(AtariToolWork* w)
{
    w->plMode = 1;
    w->pos.x = w->pos.y = w->pos.z = 0.0f;
}

static void plmove10(AtariToolWork* w)
{
    static Vec oldPos;
    Vec old;
    f32 spd;
    f32 y;

    if (w->joy.on & 0x100) {
        spd = 10.0f;
    } else {
        spd = 1.0f;
    }
    old = w->pos;
    w->pos.x += (f32) w->joy.sx * spd;
    w->pos.y += (f32) w->joy.sy * spd;
    w->pos.z = w->pos.z + (f32) w->joy.trigR * spd * 0.5f - (f32) w->joy.trigL * spd * 0.5f;
    {
        // pG read through a reference: a reference read is a MEM with neither the struct nor the scalar
        // flag, so alias.c's fixed_scalar_and_varying_struct_p does not exempt it from the three `w->pos`
        // stores above; the load then depends on the `stfs`s, which gives them a second dependent and
        // ranks them above the `old` copy's `stw`s in sched1 (the target's stfs-before-stw order). A plain
        // `pG->` read is a fixed scalar and floats above the stores.
        GlobalWork*& gp = pG;
        Draw_local_pos(&w->pos, 1000, gp->Cam.viewMat);
    }
    if (w->joy.on & 0x400) {
        int hit = At_poly_sphere_ck((AtPolyData*) satTbl0, &satTbl0[0].poly[w->polyNo], &oldPos, &w->pos, 100.0f, 0, 0);

        eprintf(100, 160, 0, 0, "HIT CK:%d = %d", w->polyNo, hit);
    } else {
        SatMgr.wallAdjust(0, &oldPos, &w->pos, 100.0f, 0, 0);
    }
    y = EatMgr.getFloor(&w->pos, 600.0f, 100000.0f, 0, 0);
    if (y != -100000.0f) {
        old.y = y;
    } else {
        eprintf(40, 64, 6, 0, "FLOOR LOST!!");
    }
    Draw_local_pos(&old, 1000, pG->Cam.viewMat);
    oldPos = w->pos;
    eprintf(40, 320, 6, 0, "%5.0f", w->pos.x);
    eprintf(40, 340, 6, 0, "%5.0f", w->pos.y);
    eprintf(40, 360, 6, 0, "%5.0f", w->pos.z);
}

static int hitSel = 0;

static void hitcheck(AtariToolWork* w)
{
    static Vec hitLine[2];
    Vec hit;
    Vec nrm;

    eprintf(16, 24, 4, 0, "HIT CHECK");
    Draw_line3d(&hitLine[0], &hitLine[1], 0xFFA0A0F0, 0);
    if (SatMgr.hitCheck(&hitLine[0], &hitLine[1], &hit, &nrm, 0, 0)) {
        eprintf(80, 160, 0, 0, "HIT");
        Draw_sphere(&hit, 500.0f, 0xFFFFFFFF, 1, 1);
    }
    if (w->joy.trg & 0x200) {
        w->mode = 0;
        w->plMode = 0;
    }
    if (w->joy.trg & 0x100) {
        hitSel++;
    }
    hitSel %= 2;
    hitLine[hitSel].x += (f32) w->joy.sx;
    hitLine[hitSel].z += (f32) w->joy.sy;
    hitLine[hitSel].y += (f32) w->joy.trigR;
    hitLine[hitSel].y -= (f32) w->joy.trigL;
}

static void load(AtariToolWork* w)
{
}

static void save(AtariToolWork* w)
{
}

static void option(AtariToolWork* w)
{
    eprintf(16, 24, 4, 0, "OPTION");
    eprintf(80, 140, 0, 0, "UNDER CONSTRUCTING...");
    if (w->joy.trg & 0x300) {
        w->mode = 0;
    }
}

static void quit(AtariToolWork* w)
{
    TOOL_FLAG(OFS_DEBUG_FLG) &= ~0x80000000;
    TOOL_FLAG(OFS_DISP_FLG) &= ~0x08000000;
    TaskSignal(0);
    TaskExit();
}

void draw_pl_pos(AtariToolWork* w)
{
}

void set_at(AtariToolWork* w, cSat* sat)
{
    w->sat = sat;
    w->nA = sat->nA;
    w->nB = sat->nB;
    w->nC = sat->nC;
}

void clear_pad(AtariToolWork* w)
{
    memclr_asm(&w->joy, sizeof(JOY));
    memclr_asm(&w->joy2, sizeof(JOY));
}
