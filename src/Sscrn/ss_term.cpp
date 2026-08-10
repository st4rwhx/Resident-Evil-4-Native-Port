// Sscrn/ss_term: the codec call screen of the sub screen DLL (D:/Bio4/Prog/ss_term.cpp): Hunnigan
// / the partner and the player model talk through the op/opNN.das message sequences. Also carries a
// leftover debug button window (cDbgWindow) and a host file list (cFileList) nothing calls.
#include "types.h"
#include "global.h"
#include "light.h"
#include "event.h"
#include "map_obj.h"
#include "widget.h"
#include "sscrn.h"

// Widget<SUB_SCREEN> is completed here (by the specialization declaration), before dbg_button.h:
// its vtable is the last one of the unit (vtables come out in reverse declaration order), after the
// cDbg* ones.
//
// COMPILER-DIFF: candidate #8 (end-of-file order: the round of Widget<SUB_SCREEN>::~Widget). The
// original outputs ~Widget in finish_file round 1, right after Widget::quit and before
// ~cDbgButtonBase / the cDbgWindowBase inlines, while its vtable is only written in round 2
// (nothing here constructs a widget). Our cc1plus outputs a comdat inline only once its assembler
// name has been referenced (decl2.c finish_file "stop lying"): ~Widget is inlined everywhere, so
// ours would wait for the round-2 vtable. This explicit specialization makes ss_main.h's
// ssWidgetDelete leave the destructor uninstantiated (saved_inlines position after quit) and lets
// the never-called ssTermWidgetKill below, compiled before the definition, emit a real
// `bl _._t6Widget...` that references the name; the body is defined inline after SsTermInit::move
// (same code as the template, inlined into the synthesized ~SsTermInit/~SsTermMain like the
// instantiation). The original REL link dead-stripped ssTermWidgetKill's body (modules.py
// STRIP_UNUSED).
template <> Widget<SUB_SCREEN>::~Widget();

static void ssTermWidgetKill(Widget<SUB_SCREEN>* w)
{
    w->Widget<SUB_SCREEN>::~Widget();  // COMPILER-DIFF: candidate #8
}

#include "dbg_button.h"
#include "item.h"
#include "mes.h"
#include "id_sys.h"
#include "fade.h"
#include "dvd.h"
#include "main_mem.h"
#include "main.h"
#include "joy.h"
#include "snd.h"
#include "db_log.h"
#include "eprintf.h"
#include "file.h"
#include "dbmodule.h"
#include "camera.h"
#include "view.h"
#include "model.h"
#include "motion.h"
#include "math_sub.h"

extern "C" {
int sprintf(char* s, const char* fmt, ...);
void* memset(void* p, int c, unsigned int n);
char* strchr(const char* s, int c);
char* strstr(const char* s, const char* k);
unsigned int strlen(const char* s);
char* strcpy(char* d, const char* s);
// game/shape.cpp
int ShapeMove(cModelInfo* info);
void ClrShape(cModel* m);
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");
}
int ShapeSet(void* work, int frame, void* data, int flags);
void* GetModelInfoAddr(cModelInfo* info, int no);

#define DVD_READ_N(name, dst, a, b, c, mode) DvdReadN(name, dst, a, b, c, mode, __FILE__, __LINE__)

extern "C" {
u32 MakeCol(f32 r, f32 g, f32 b, f32 a);
void DbgDrawBoxFill(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a);
void partnerDataName(char* name, int no);
int partnerType(int no);
void termMotionSet(void* data, int no);
void termMotionCancel(void* data, int no);
void termModelAlloc(SUB_SCREEN* wk);
void terminalCameraInit(SUB_SCREEN* wk, Camera* cam);
}

u32 MakeCol(f32 r, f32 g, f32 b, f32 a)
{
    u32 col = 0;

    col += (u8) (a * 255.0f) << 24;
    col += (u8) (r * 255.0f) << 16;
    col += (u8) (g * 255.0f) << 8;
    col += (u8) (b * 255.0f);
    return col;
}

void DbgDrawBoxFill(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a)
{
    Vec pos;
    Vec size;

    pos.x = x;
    pos.y = y;
    size.x = w;
    size.y = h;
    Draw_quad(&pos, &size, MakeCol(r, g, b, a));
}

// Debug button window: up to 128 buttons on a character grid, a cursor moved with the pad.
class cDbgWindow : public cDbgWindowBase {
public:
    u32 num;               // 0x28
    cDbgButton* btn[128];  // 0x2C
    cDbgButton* cur;       // 0x22C
    cDbgButton* top;       // 0x230
    cDbgButton* bottom;    // 0x234

    virtual ~cDbgWindow() {
        u32 i;
        for (i = 0; i < num; i++) {
            if (btn[i]) {
                delete btn[i];
            }
        }
    }
    virtual int GetCx() {
        if (cur) {
            return cur->cx;
        }
        return 0;
    }
    virtual int GetCy() {
        if (cur) {
            return cur->cy;
        }
        return 0;
    }
    virtual void SetCurrentTopButton() { cur = top; }
    virtual void SetCurrentBottomButton() { cur = bottom; }
    virtual void ButtonAllUpdate() {
        u32 i;
        for (i = 0; i < num; i++) {
            cDbgButton* b = btn[i];
            if (b && b->func) {
                b->func(b);
            }
        }
    }
    int AddButton(int bx, int by, const char* name, int bcx, int bcy);
    int FindButton(int cx, int cy, cDbgButton** out);
    virtual int LocalUpdate();
    virtual void LocalDisp();
};

// Never called: the original REL link dead-stripped the body (modules.py STRIP_UNUSED) and kept
// its string after MakeCol's pool. Compiled at parse time it inlines cDbgButton's implicit
// constructor, which references `_vt.10cDbgButton`: the vtable is then written in finish_file
// round 1 (fourth of the unit's vtables) and ~cDbgButton is the first end-of-file function, while
// `_vt.14cDbgButtonBase` / ~cDbgButtonBase wait for round 2 (the base vptr store is elided).
int cDbgWindow::AddButton(int bx, int by, const char* name, int bcx, int bcy)
{
    cDbgButton* b;

    btn[num] = b = new cDbgButton;
    if (b == 0) {
        pLog->err(0, 0, "AddButton(): new failed.");
        return 0;
    }
    num++;
    return 1;
}

int cDbgWindow::FindButton(int cx, int cy, cDbgButton** out)
{
    u32 i;

    *out = 0;
    for (i = 0; i < num; i++) {
        cDbgButton* b = btn[i];
        if (b->cx == cx && b->cy == cy) {
            *out = b;
            return 1;
        }
    }
    return 0;
}

int cDbgWindow::LocalUpdate()
{
    int ret = 1;
    int cx = GetCx();
    int cy = GetCy();

    if (Joy[0].rep & 0x00010001) {
        cx--;
    }
    if (Joy[0].rep & 0x00020002) {
        cx++;
    }
    if (Joy[0].rep & 0x00080008) {
        cy--;
    }
    if (Joy[0].rep & 0x00040004) {
        cy++;
    }
    if (cx < 0) {
        cx = maxCx;
    }
    if (cy < 0) {
        cy = maxCy;
    }
    if (cx > maxCx) {
        cx = 0;
    }
    if (cy > maxCy) {
        cy = 0;
    }
    if (cx != GetCx() || cy != GetCy()) {
        cDbgButton* b;
        if (FindButton(cx, cy, &b)) {
            cur = b;
        }
    }
    ButtonAllUpdate();
    if (Joy[0].trg & 0x200) {
        ret = 0;
    }
    return ret;
}

// Text row of a button (the window's row 0 is its title line).
static inline int dbgWindowRow(int y)
{
    return y + 1;
}

void cDbgWindow::LocalDisp()
{
    u32 i;
    cDbgButton* c;

    for (i = 0; i < num; i++) {
        cDbgButton* b = btn[i];
        eprintf2(8, 0xC, (x + b->x) * 8, (dbgWindowRow(y) + b->y) * 14, 0x10, 0, b->name);
    }
    c = cur;
    if (c) {
        int wx = x;
        int wy = dbgWindowRow(y);
        if (pG->flags_51E4 & 4) {
            eprintf2(8, 0xC, (wx + c->x - 1) * 8, (wy + c->y) * 14, 0, 0, ">");
        }
        eprintf2(8, 0xC, (wx + c->x) * 8, (wy + c->y) * 14, 0, 0, c->name);
        {
            f32 px = (f32) ((wx + c->x) * 8);
            f32 py = (f32) ((wy + c->y) * 14);
            f32 pw = (f32) (c->w * 8);
            f32 ph = 14.0f;
            f32 bd = 2.0f;
            DbgDrawBoxFill(px - bd, py - bd, pw + 0.0f, ph + bd, 0.7f, 0.7f, 0.0f, 0.3f);
        }
    }
}

#include "ss_main.h"

static inline void IntSet(int& d, int v) { d = v; }

// Struct-member view of the cModel manager pointers (game/sscrn.cpp MGR_PTR).
struct MgrPtr {
    void* p;
};
#define MGR_PTR(g) (((MgrPtr*) &(g))->p)

// Host file list (d:\bio4\room\filelist.txt through the SN file server): a scrolling list of the
// names under one directory.
class cFileList {
public:
    char* filter;  // 0x00  prefix stripped from every name
    char* pattern; // 0x04  search pattern
    char* text;    // 0x08  file list text
    char** list;   // 0x0C  one pointer per line
    int num;       // 0x10
    s16 cursor;    // 0x14
    s16 top;       // 0x16

    // Empty: the file-scope instance below gives the unit its (empty) static init/destroy pair.
    cFileList() {}
    ~cFileList() {}
    void init();
    char* disp(int x, int y, int rows);
    int update();
    void dir(char* d, char* f);
};

void cFileList::init()
{
    char* d;
    char* f;

    text = 0;
    list = 0;
    cursor = 0;
    pattern = 0;
    filter = 0;
    dir(d, f);
    update();
}

char* cFileList::disp(int x, int y, int rows)
{
    JOY* joy = &Joy[0];
    int end;
    int i;

    if (joy->rep2 & 0x000C000C) {
        if (joy->rep2 & 4) {
            cursor++;
        }
        if (joy->rep2 & 8) {
            cursor--;
        }
        if (joy->rep2 & 0x40000) {
            cursor += rows / 2;
        }
        if (joy->rep2 & 0x80000) {
            cursor -= rows / 2;
        }
        cursor = cursor < 0 ? 0 : (cursor > num - 1 ? num - 1 : cursor);
    }
    if (top < cursor - rows + 1) {
        top = cursor - rows + 1;
    }
    if (top > cursor) {
        top = cursor;
    }
    if (top + rows > num) {
        end = num;
    } else {
        end = top + rows;
    }
    for (i = top; i < end; i++) {
        int col = 0;
        if (i == cursor) {
            col = 6;
        }
        eprintf(x, y, col, 0, "%s", list[i]);
        y += 16;
    }
    return list[cursor];
}

int cFileList::update()
{
    char* p;
    int i;

    if (text) {
        Debug_free(text);
    }
    if (list) {
        Debug_free(list);
    }
    HDReadDebugAlloc("d:\\bio4\\room\\filelist.txt", (void**) &text, 1);
    num = 0;
    if (text == 0) {
        pLog->err(0, 0, "cFileList::update : file not found");
        return 0;
    }
    p = text;
    while ((p = strchr(p, '\\')) != 0) {
        *p = '/';
    }
    p = text;
    num = 0;
    while ((p = strchr(p, '\r')) != 0) {
        *p = 0;
        p++;
        num++;
    }
    list = (char**) Debug_alloc(num * 4, 1);
    p = text;
    for (i = 0; i < num; i++) {
        if (filter) {
            p = strstr(p, filter);
            p += strlen(filter);
        }
        list[i] = p;
        p += strlen(p);
        p += 2;
    }
    return 1;
}

void cFileList::dir(char* d, char* f)
{
    if (pattern) {
        Debug_free(pattern);
    }
    if (filter) {
        Debug_free(filter);
    }
    if (d == 0) {
        char defDir[15] = "\\bio4\\data\\*.*";
        pattern = (char*) Debug_alloc(strlen(defDir), 1);
        strcpy(pattern, defDir);
        char defFilter[12] = "/bio4/data/";
        filter = (char*) Debug_alloc(strlen(defFilter), 1);
        strcpy(filter, defFilter);
    } else {
        pattern = (char*) Debug_alloc(strlen(d), 1);
        strcpy(pattern, d);
        if (f) {
            char* p;
            filter = (char*) Debug_alloc(strlen(f), 1);
            strcpy(filter, f);
            p = filter;
            while ((p = strchr(p, '\\')) != 0) {
                *p = '/';
            }
        } else {
            filter = f;
        }
    }
}

// The op table: 24 ops, message / sequence data filled from op/opNN.das at init.
TermOpe term_ope_tbl[24] = {
    {0x8C}, {0x8D}, {0x8E}, {0x8F}, {0x90}, {0x91}, {0x92}, {0x93}, {0x94}, {0x95}, {0x96}, {0x97},
    {0x98}, {0x99}, {0x9A}, {0x9B}, {0x9C}, {0x9D}, {0x9E}, {0x9F}, {0xA0}, {0xA1}, {0xA2}, {0xA3},
};

// The archive inside op/opNN.das starts 0x400 bytes in. Read through an inline (not a macro on the
// member): the table stores may alias wk->pOpData, so the pointer is reloaded per statement, and
// `ofs + (u32) arc` is not reassociated with the +0x400.
static inline SsArc* opArc(SUB_SCREEN* wk)
{
    return (SsArc*) ((u8*) wk->pOpData + 0x400);
}
#define OP_ARC_PTR(wk, no) SS_ARC_PTR(opArc(wk), no)

void SsTermMain::OpeMesTblInit(SUB_SCREEN* wk)
{
    term_ope_tbl[0].mes = OP_ARC_PTR(wk, 4);
    term_ope_tbl[1].mes = OP_ARC_PTR(wk, 5);
    term_ope_tbl[2].mes = OP_ARC_PTR(wk, 6);
    term_ope_tbl[3].mes = OP_ARC_PTR(wk, 7);
    term_ope_tbl[4].mes = OP_ARC_PTR(wk, 8);
    term_ope_tbl[5].mes = OP_ARC_PTR(wk, 9);
    term_ope_tbl[6].mes = OP_ARC_PTR(wk, 10);
    term_ope_tbl[7].mes = OP_ARC_PTR(wk, 11);
    term_ope_tbl[8].mes = OP_ARC_PTR(wk, 12);
    term_ope_tbl[9].mes = OP_ARC_PTR(wk, 13);
    term_ope_tbl[10].mes = OP_ARC_PTR(wk, 14);
    term_ope_tbl[11].mes = OP_ARC_PTR(wk, 15);
    term_ope_tbl[12].mes = OP_ARC_PTR(wk, 16);
    term_ope_tbl[13].mes = OP_ARC_PTR(wk, 4);
    term_ope_tbl[14].mes = OP_ARC_PTR(wk, 5);
    term_ope_tbl[15].mes = OP_ARC_PTR(wk, 6);
    term_ope_tbl[16].mes = OP_ARC_PTR(wk, 7);
    term_ope_tbl[17].mes = OP_ARC_PTR(wk, 8);
    term_ope_tbl[18].mes = OP_ARC_PTR(wk, 9);
    term_ope_tbl[19].mes = OP_ARC_PTR(wk, 4);
    term_ope_tbl[20].mes = OP_ARC_PTR(wk, 5);
    term_ope_tbl[21].mes = OP_ARC_PTR(wk, 6);
    term_ope_tbl[22].mes = OP_ARC_PTR(wk, 7);
    term_ope_tbl[23].mes = OP_ARC_PTR(wk, 8);
    term_ope_tbl[0].seq = OP_ARC_PTR(wk, 17);
    term_ope_tbl[1].seq = OP_ARC_PTR(wk, 18);
    term_ope_tbl[2].seq = OP_ARC_PTR(wk, 19);
    term_ope_tbl[3].seq = OP_ARC_PTR(wk, 20);
    term_ope_tbl[4].seq = OP_ARC_PTR(wk, 21);
    term_ope_tbl[5].seq = OP_ARC_PTR(wk, 22);
    term_ope_tbl[6].seq = OP_ARC_PTR(wk, 23);
    term_ope_tbl[7].seq = OP_ARC_PTR(wk, 24);
    term_ope_tbl[8].seq = OP_ARC_PTR(wk, 25);
    term_ope_tbl[9].seq = OP_ARC_PTR(wk, 26);
    term_ope_tbl[10].seq = OP_ARC_PTR(wk, 27);
    term_ope_tbl[11].seq = OP_ARC_PTR(wk, 28);
    term_ope_tbl[12].seq = OP_ARC_PTR(wk, 29);
    term_ope_tbl[13].seq = OP_ARC_PTR(wk, 10);
    term_ope_tbl[14].seq = OP_ARC_PTR(wk, 11);
    term_ope_tbl[15].seq = OP_ARC_PTR(wk, 12);
    term_ope_tbl[16].seq = OP_ARC_PTR(wk, 13);
    term_ope_tbl[17].seq = OP_ARC_PTR(wk, 14);
    term_ope_tbl[18].seq = OP_ARC_PTR(wk, 15);
    term_ope_tbl[19].seq = OP_ARC_PTR(wk, 9);
    term_ope_tbl[20].seq = OP_ARC_PTR(wk, 10);
    term_ope_tbl[21].seq = OP_ARC_PTR(wk, 11);
    term_ope_tbl[22].seq = OP_ARC_PTR(wk, 12);
    term_ope_tbl[23].seq = OP_ARC_PTR(wk, 13);
}

void SsTermMain::OpeMdtSet()
{
    OpeMdtSetNo(OpeMdtSetInit());
}

void SsTermMain::OpeMdtSetNo(int no)
{
    if (no > 0x17) {
        pLog->err(0, 0, "SsTermMain::OpeMdtSetNo [%d]", no);
    } else {
        OpeMdtSetSub(term_ope_tbl[no].mdtNo, term_ope_tbl[no].seq, term_ope_tbl[no].mes);
    }
}

void SsTermMain::OpeMdtSetSub(int mdtNo, void* seq, void* mes)
{
    ope.mdtNo = mdtNo;
    ope.seq = (TermSeq*) seq;
    ope.mes = mes;
    MesData.setPtr(2, (u8*) mes);
    ope.seqIdx = 0;
    ope.mesWait = 0;
    ope.flags &= ~0x08000000;
    ope.seqCnt = 0;
    ope.str = 0;
}

int SsTermMain::OpeMesMove()
{
    if (ope.seqCnt == 0 && ope.str == 0) {
        if (ope.mdtNo != 0) {
            SndStrStopBlock(SubScreenWk.strBlk);
            ope.str = SndStrReq(1, ope.mdtNo, 1, 0, 0, 0.0f);
            ope.flags |= 0x08000000;
            return 0;
        }
    }
    if (ope.str != 0 && (ope.flags & 0x08000000)) {
        IdUnit* u;
        if (SndStrStatusCk(ope.str, 2) == 0) {
            return 0;
        }
        SndStrReq(ope.str, 2, 0, 0);
        ope.flags &= ~0x08000000;
        FadeKillAll();
        termMotionSet(SubScreenWk.pPartner, 10);
        modelOn = 1;
        u = IdSub.unitPtr(0x13, 0x10);
        IdSub.setTime(u, 0);
        u->flags |= 8;
        u->dir &= 0xF0;
        u = IdSub.unitPtr(0x14, 0x10);
        IdSub.setTime(u, 0);
        u->flags |= 8;
        u->dir &= 0xF0;
    }
    OpeMesClear();
    if (!(ope.flags & 0x10000000)) {
        if (Key.trg & 0x40000) {
            OpeSndStrStop();
            ope.flags |= 0x10000000;
        }
        for (;;) {
            TermSeq* s = &ope.seq[ope.seqIdx];
            if (!(s->time > ope.seqCnt)) {
                if (OpeSeqMove(s) == 0) {
                    return 1;
                }
            } else {
                break;
            }
        }
        ope.seqCnt++;
    } else {
        if ((Key.trg & 0x80000) || (Key.trg & 0x40000)) {
            if (OpeSeqMove(&ope.seq[ope.seqIdx]) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

int SsTermMain::OpeSeqMove(TermSeq* s)
{
    TermSub* w = &sub;

    w->x14 = s->x2;
    w->x18 = s->mesNo;
    if (s->arg == -1) {
        OpeSndStrStop();
        MessageControl* m = &cMes;
        int i = 0;
        for (; i < 16; i++) {
            m->Delete(i);
        }
        return 0;
    }
    OpeMesSet(s->mesNo, s->arg);
    ope.seqIdx++;
    return 1;
}

void SsTermMain::OpeMesSet(int no, int wait)
{
    pG->flags_58 &= ~0x800;
    if (no == -1) {
        cMes.WaitEnd(0);
    } else {
        IdUnit* u = IdSub.unitPtr(0xFE, 0x10);
        int x = (int) ((u->scr.x + 320.0f) * 0.8f);
        int y = (int) ((240.0f - u->scr.y) * 0.8f);
        MessageControl* m = &cMes;
        int i;
        for (i = 0; i < 16; i++) {
            m->Delete(i);
        }
        cMes.MesSet(no, x, y, 0x03000054, 0, 0, 4);
    }
    ope.mesNo = no;
    ope.mesWait = wait;
    sub.count++;
}

void SsTermMain::OpeMesClear()
{
    if (ope.mesWait > 0) {
        ope.mesWait--;
        if (ope.mesWait <= 0) {
            ope.mesWait = 0;
            pG->flags_58 |= 0x800;
        }
    }
    sub.count++;
}

void SsTermMain::OpeSndStrStop()
{
    if (ope.str) {
        SndStrReq(ope.str, 8, 0, 0);
        ope.str = 0;
    }
}

void partnerDataName(char* name, int no)
{
    const char* tbl[24] = {
        "101", "102", "103a", "103b", "104", "105", "106", "107", "108", "109", "110", "111",
        "112", "201", "202", "203", "204", "205", "206", "301", "302", "303", "304", "305",
    };
    sprintf(name, "SS/cmn/ss_oc%s.dat", tbl[no]);
}

int partnerType(int no)
{
    int tbl[24] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3};
    return tbl[no];
}

// The partner data file (SS/cmn/ss_ocNNN.dat) is an offset table like the other archives: 4 = the
// cancel motion, 12/13 = the player model's motion / shape data, 14/15 = the partner's.
void termMotionSet(void* data, int no)
{
    SsArc* d = (SsArc*) data;
    cModel* m;

    m = MapMgr.getWork(0);
    MotionSetCore(m, &((cMotModel*) m)->mot, SS_ARC_PTR(d, 12), 0, (u8) no, 0x8000, 0);
    ShapeSet(GetModelInfoAddr(m->pInfo, 3), 0, SS_ARC_PTR(d, 13), 2);
    m = MapMgr.getWork(2);
    MotionSetCore(m, &((cMotModel*) m)->mot, SS_ARC_PTR(d, 14), 0, (u8) no, 0x8000, 0);
    ShapeSet(GetModelInfoAddr(m->pInfo, 3), 0, SS_ARC_PTR(d, 15), 0xA);
}

void termMotionCancel(void* data, int no)
{
    SUB_SCREEN* wk = &SubScreenWk;
    SsArc* d = (SsArc*) data;
    cModel* m;

    m = MapMgr.getWork(0);
    MotionSetCore(m, &((cMotModel*) m)->mot, SS_ARC_PTR(wk->pTerm, 14), 0, (u8) no, 0x8004, 0);
    m = MapMgr.getWork(2);
    MotionSetCore(m, &((cMotModel*) m)->mot, SS_ARC_PTR(d, 4), 0, (u8) no, 0x8004, 0);
}

static cFileList term_file_list;

void SsTermInit::init(SUB_SCREEN* wk)
{
    state = 2;
}

void SsTermInit::move(SUB_SCREEN* wk)
{
    // .bss order: the read request is the unit's first .bss word, before the file-scope
    // term_file_list (a constructed object is emitted with the static-init function, a plain
    // file-scope static only at the end): a function-local static is emitted at its function.
    static int term_read_req;
    void* term;
    void* op;
    void* partner;

    switch (state) {
    case 0:
    case 1:
    case 2:
        IdSys.dispSw(0x21, 0);
        IdSub.dispSw(2, 0);
        sscrnModelFree(wk);
        sscrnLightClear(wk);
        wk->pTerm = (SsArc*) (wk->pzzlOfs + (u32) wk->pBuf);
        sscrnDataFilename(wk, "ss_term.dat");
#line 1101 "D:/Bio4/Prog/ss_term.cpp"
        term_read_req = DVD_READ_N(wk->path, 0, 0, 0, 0, 5);
        IdSubErase();
        IdNumErase();
        IdFreeBuffer();
        state++;
    case 3:
        Dvd.ReadCheck(term_read_req, 0, 0, &term);
        wk->pTerm = (SsArc*) term;
        state++;
    case 4: {
        char name[32];
        sprintf(name, "op/op%02d.das", pG->stage_no);
#line 1134 "D:/Bio4/Prog/ss_term.cpp"
        term_read_req = DVD_READ_N(name, 0, 0, 0, 0, 5);
        Dvd.ReadCheck(term_read_req, 0, 0, &op);
        wk->pOpData = op;
        state++;
    }
    case 5: {
        char name[32];
        partnerDataName(name, wk->mdtNo);
#line 1156 "D:/Bio4/Prog/ss_term.cpp"
        term_read_req = DVD_READ_N(name, 0, 0, 0, 0, 5);
        Dvd.ReadCheck(term_read_req, 0, 0, &partner);
        wk->pPartner = partner;
        state++;
    }
    case 6:
        if (wk->type == 0x20) {
            FadeSetW(0x80000000, 5, 0, 0);
        }
        transit(0, wk);
        break;
    }
}

// The Widget<SUB_SCREEN> destructor body (see the specialization declaration at the top): defined
// after SsTermInit::move, whose inlined transit instantiates Widget::quit, so the deferred
// destructor follows quit at the end of the file.
template <> inline Widget<SUB_SCREEN>::~Widget()  // COMPILER-DIFF: candidate #8
{
    Mem_free(link);
}

void termModelAlloc(SUB_SCREEN* wk)
{
    int i;

    wk->x38 |= 1;
    ssModInfoMgr.roomInit();
    ssModInfoMgr.arrayAlloc(0x10);
    ssPartsMgr.roomInit();
    ssPartsMgr.arrayAlloc(0x180);
    MGR_PTR(cModel::mm) = &ssModInfoMgr;
    MGR_PTR(cModel::pm) = &ssPartsMgr;
    MapMgr.roomInit();
    MapMgr.arrayAlloc(0x10);
    for (i = 0; i < 0x10; i++) {
        MapMgr.create(0, i);
    }
}

void terminalCameraInit(SUB_SCREEN* wk, Camera* cam)
{
    const f32 zero = 0.0f;

    cam->param.pos.z = 2000.0f;
    cam->up.y = 1.0f;
    cam->param.at.x = 0.0f;
    cam->param.at.y = 0.0f;
    cam->param.at.z = 0.0f;
    cam->param.pos.x = 0.0f;
    cam->param.pos.y = 0.0f;
    cam->up.x = 0.0f;
    cam->up.z = 0.0f;
    cam->param.fovy = 50.0f;
    CameraSetOrientationUp(cam);
    C_MTXPerspective(cam->projMat, cam->param.fovy, 1.3333334f, ZNEAR, ZFAR);
    cam->dist = PSVECDistance(&cam->param.pos, &cam->param.at);
    C_MTXLookAt(cam->viewMat, &cam->param.pos, &cam->up, &cam->param.at);
}

extern "C" f64 tan(f64 x);

// Never called (the ss_pzzl screenPos2puzzlePos formula): the original REL link dead-stripped the
// body (modules.py STRIP_UNUSED) and kept its pool, the four floats 0.5 / pi / 180 / 240 that
// follow terminalCameraInit's 1.3333334 in .rodata.
static void screenPos2terminalPos(Vec* pos, Vec* out)
{
    Camera* cam = &pG->Cam;
    f32 pz = cam->param.pos.z;
    f32 h = fabsf((f32) (pz * tan(cam->param.fovy * 0.5f * 3.1415927f / 180.0f)));

    out->x = pos->x * h / 240.0f;
    out->y = pos->y * h / 240.0f;
}

static Vec term_cam_pos = {435.0f, -1580.0f, 850.0f};
static Vec term_pl_pos = {0.0f, -1600.0f, 950.0f};
static Vec term_zero0 = {0.0f, 0.0f, 0.0f};
static Vec term_zero1 = {0.0f, 0.0f, 0.0f};

void SsTermMain::init(SUB_SCREEN* wk)
{
    IdUnit* u;
    cModel* m;

    IdTexDataLoad(SS_ARC_PTR(wk->pTerm, 5), 9);
    IdSub.set(SS_ARC_PTR(wk->pTerm, 8), 0xFF, 0x14, 0xC, 5, 0);
    IdSub.set(SS_ARC_PTR(wk->pTerm, 9), 0xFF, 0x10, 0xF, 2, 0);
    u = IdSub.unitPtr(0x12, 0x10);
    u->flags &= ~8;
    u->dir |= 0xF;
    u = IdSub.unitPtr(0x13, 0x10);
    u->flags &= ~8;
    u->dir |= 0xF;
    u = IdSub.unitPtr(0x14, 0x10);
    u->flags &= ~8;
    u->dir |= 0xF;
    sscrnMainMenuInit(wk, 0);
    IntSet(x10, 0);
    if (pSys->language == 0) {
        cMes.setupFont(0x1C, 0x1C, (TEXPalette*) SS_ARC_PTR(wk->pTerm, 4), 3);
    }
    cMes.setLayout(0, 4);
    memset(&ope, 0, sizeof(ope));
    SndCall(0, 0x14, 0, 0, 0, 0);
    OpeMesTblInit(wk);
    ope.wait = 0x1E;
    IdAllocBuffer();
    sscrnLightCreate(wk, (cLit*) SS_ARC_PTR(wk->pCmmn, 0x15));
    terminalCameraInit(wk, &pG->Cam);
    termModelAlloc(wk);
    ssPlModel = MapMgr.getWork(0);
    ssWepModel = MapMgr.getWork(1);
    ssPlMotion = 0;
    ssWepModel2 = 0;
    tel00ModelInit(MapMgr.getWork(0), wk->pTerm);
    m = MapMgr.getWork(2);
    hunniganModelInit(m, wk->pPartner, partnerType(wk->mdtNo));
    modelOn = 0;
    ended = 0;
    {
        Vec pos;
        Vec ang = {0.0f, 0.0f, 0.0f};
        pos = term_pl_pos;
        MapMgr.getWork(2)->pos = pos;
        MapMgr.getWork(2)->rot = ang;
        MapMgr.getWork(2)->matUpdate();
        {
            Vec d;
            Vec ang2;
            pos = term_cam_pos;
            PSVECSubtract(&pG->Cam.param.pos, &pos, &d);
            ang2.x = 0.0f;
            ang2.y = atan2f(d.x, d.z);
            ang2.z = 0.0f;
            MapMgr.getWork(0)->pos = pos;
            MapMgr.getWork(0)->rot = ang2;
            MapMgr.getWork(0)->matUpdate();
        }
    }
    termMotionCancel(wk->pPartner, 0);
    modelOn = 1;
}

void SsTermMain::move(SUB_SCREEN* wk)
{
    if (modelOn != 0) {
        if (ended == 0 && (ope.flags & 0x10000000)) {
            IdUnit* u;
            termMotionCancel(wk->pPartner, 10);
            ClrShape(MapMgr.getWork(0));
            ClrShape(MapMgr.getWork(2));
            ended = 1;
            u = IdSub.unitPtr(0x13, 0x10);
            IdSub.setTime(u, 0);
            u->flags |= 8;
            u->dir &= 0xF0;
            u = IdSub.unitPtr(0x14, 0x10);
            IdSub.setTime(u, 0);
            u->flags |= 8;
            u->dir &= 0xF0;
        }
        MotionMoveF(MapMgr.getWork(0), 0);
        ShapeMove(MapMgr.getWork(0)->pInfo);
        MotionMoveF(MapMgr.getWork(2), 0);
        ShapeMove(MapMgr.getWork(2)->pInfo);
    }
    if (x10 == 0) {
        if (ope.wait != 0) {
            ope.wait--;
            if (ope.wait <= 0) {
                OpeMdtSet();
            }
        } else {
            if (OpeMesMove() == 1 || (Key.trg & 0x20000000)) {
                OpeSndStrStop();
                MessageControl* m = &cMes;
                int i = 0;
                wk->x34 |= 8;
                for (; i < 16; i++) {
                    m->Delete(i);
                }
                transit(0, wk);
            }
        }
    }
}

void SsTermMain::quit(SUB_SCREEN* wk)
{
    SndCall(0, 0x15, 0, 0, 0, 0);
    if (pSys->language == 0) {
        cMes.releaseFont(3);
    }
    wk->x34 |= 8;
}
