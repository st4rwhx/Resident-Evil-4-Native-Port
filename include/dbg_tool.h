#ifndef DBG_TOOL_H
#define DBG_TOOL_H

#include "types.h"
#include "db_toolbase.h"
#include "joy.h"
#include "global.h"
#include "eprintf.h"
#include "main_mem.h"

// Debug-tool editor templates (the second half of D:/Bio4/Prog/db_toolbase.h, from the cDbgWindow
// classes on: the HALT() checks below carry that file name and its line numbers). Used by Tools'
// t_esp_area.cpp (cDbgToolMain<ESP_AREA>) / t_lightarea.cpp (LIGHT_AREA) and t_event's
// cDbgEditWindow<EventMessageData::MessElem>. Everything here is header-only: the non-template
// classes' members are `inline` (cDbgFileSelectWindow / cDbgOkCancelWindow have no key function,
// so every unit that constructs one carries linkonce copies of Init / LocalUpdate / the destructors;
// in a module only the first unit's copies keep their names), the template members are instantiated
// per unit.

#include "file.h"

extern "C" int sprintf(char* s, const char* fmt, ...);
extern "C" void OSReport(const char* fmt, ...);

// HALT() as the original header spells it (a plain block, see docs/matching.md)
#define DBG_TOOL_HALT()                                    \
    {                                                      \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);     \
        *(volatile u32*) 0x11111111 = 0;                   \
    }

// Save-file layout of every editor: a 0x10 header (count) followed by the live works.
struct DbgToolFileHeader {
    u32 num;
    u32 x4;
    u32 x8;
    u32 xC;
};

// Replaces a button label (truncated to the allocated length).
static inline void DbgButtonSetName(cDbgButtonBase* b, const char* s)
{
    if (strlen(s) > b->nameLen) {
        u32 i;

        for (i = 0; i < b->nameLen - 2; i++) {
            b->pName[i] = s[i];
        }
        b->pName[i] = 0;
    } else {
        strcpy(b->pName, s);
    }
}

// File selector: a 0..99 file number with the name preview, "[OK]" on the second cursor row.
class cDbgFileSelectWindow : public cDbgWindow {
public:
    int fileNo;            // 0x238
    const char* pPath1;    // 0x23C  directory
    const char* pPath2;    // 0x240  file stem
    const char* pExt;      // 0x244
    char fileName[0x100];  // 0x248  path1 + path2 + "%02d" + ext

    void Init(int wx, int wy, const char* name, const char* path1, const char* path2, const char* ext);
    virtual int LocalUpdate();
    virtual ~cDbgFileSelectWindow();
};

// Init wrapper defined before Init's body: the call stays out of line in the saved RTL of this inline
// (GCC 2.95 inlines while generating the caller's RTL), which is what the original objects show
// (`bl cDbgFileSelectWindow::Init` after the `new`, the name literal materialised at the call).
static inline void DbgFileSelectWindowInit(cDbgFileSelectWindow* w, int wx, int wy, const char* name,
                                           const char* path1, const char* path2, const char* ext)
{
    w->Init(wx, wy, name, path1, path2, ext);
}

// The selector's buttons (header-owned strings; inlined into the tool's window creation).
static inline void DbgFileSelectWindowAddButtons(cDbgFileSelectWindow* w)
{
    w->AddButton(1, 1, "Name:                 ", 0xFFFF, 0xFFFF, 0, 0);
    w->AddButton(8, 1, "               ", 0x10000, 0xFFFF, 0, 0);
    w->AddButton(1, 3, "No  :", 0x10001, 0xFFFF, 0, 0);
    w->AddButton(7, 3, " xx ", 0, 0, 0, 0);
    w->AddButton(7, 4, "[OK]", 0, 1, 0, 0);
}

inline void cDbgFileSelectWindow::Init(int wx, int wy, const char* name, const char* path1, const char* path2,
                                       const char* ext)
{
    x = wx;
    y = wy;
    w = strlen(name);
    h = 1;
    cxMax = 1;
    cyMax = 1;
    pName = name;
    x1C = 0;
    x20 = 0;
    num = 0;
    // COMPILER-DIFF: #13 -- the cDbgWindow::Init region-split recipe (db_toolbase.h): the original's
    // zero is a reload-materialised constant (no `li` in sched1, no death at its last store), so the
    // block is issued in source order with `li 1` before `li 0`. The three dead loop notes split our
    // sched1 regions so that the zero has <= 3 dependents in the first region (`li 1` ranks first),
    // the dying pPath1/pPath2/pExt stores cannot pass pCur/fileName, and the last zero store stays
    // last (18 -> 0 words in t_event, t_esp_area, t_lightarea).
    do {
    } while (0);
    pCur = 0;
    fileName[0] = 0;
    do {
    } while (0);
    pPath1 = path1;
    pPath2 = path2;
    pExt = ext;
    pTop = 0;
    pBottom = 0;
    do {
    } while (0);
    fileNo = 0;
}

inline int cDbgFileSelectWindow::LocalUpdate()
{
    int ret = 1;
    int bcx;
    int bcy;
    u32 rep;
    char buf[0xC0];

    bcx = GetCx();
    bcy = GetCy();
    rep = Joy[0].rep;
    if (rep & 0x10001) {
        bcx--;
    }
    if (rep & 0x20002) {
        bcx++;
    }
    if (rep & 0x80008) {
        bcy--;
    }
    if (rep & 0x40004) {
        bcy++;
    }
    if (bcx < 0) {
        bcx = cxMax;
    }
    if (bcy < 0) {
        bcy = cyMax;
    }
    if (bcx > cxMax) {
        bcx = 0;
    }
    if (bcy > cyMax) {
        bcy = 0;
    }
    if (bcx != GetCx() || bcy != GetCy()) {
        cDbgButton* b;

        if (FindButton(bcx, bcy, &b)) {
            pCur = b;
        }
    }
    if (Joy[0].trg & 0x100) {
        cDbgButton* c = pCur;

        if (c && c->pFunc) {
            c->pFunc(c);
        }
    }
    ButtonAllUpdate();
    if (GetCy() == 0) {
        int step = 0;

        if (Joy[0].rep & 0x10001) {
            step = -1;
        }
        if (Joy[0].rep & 0x20002) {
            step = 1;
        }
        if (Joy[0].on & 0x100) {
            step *= 10;
        }
        fileNo += step;
        if (fileNo < 0) {
            fileNo = 0;
        }
        if (fileNo > 99) {
            fileNo = 99;
        }
        if ((Joy[0].on & 0x800) && (Joy[0].trg & 0x100)) {
            fileNo = 0;
        }
    }
    if (GetCy() == 1 && (Joy[0].trg & 0x100)) {
        ret = 0;
    }
    {
        cDbgButton* nb;

        if (FindButton(0x10000, 0xFFFF, &nb)) {
            sprintf(buf, "%s%02d%s", pPath2, fileNo, pExt);
            sprintf(fileName, "%s%s%02d%s", pPath1, pPath2, fileNo, pExt);
            DbgButtonSetName(nb, buf);
        }
        if (FindButton(0, 0, &nb)) {
            sprintf(buf, " %02d", fileNo);
            DbgButtonSetName(nb, buf);
        }
    }
    if (Joy[0].trg & 0x200) {
        ret = 0;
        SetCurrentTopButton();
    }
    return ret;
}

// " [OK] " / "[CANCEL]" confirmation: LocalUpdate returns 0 once decided, GetCx() tells which.
// explicit (empty) destructor: as a deferred inline it is emitted after Init/LocalUpdate, where the
// original has it; the synthesized one would be emitted first (the OkCancel window keeps the implicit one)
inline cDbgFileSelectWindow::~cDbgFileSelectWindow() {}

class cDbgOkCancelWindow : public cDbgWindow {
public:
    void Init(int wx, int wy, const char* name);
    void InitLast(int wx, int wy, const char* name);
    virtual int LocalUpdate();
};

// The three inlined copies (CreateFileWindows) share the `1`/`0` pseudos (r28/r31). In the first
// two copies the original stores pBottom/pTop/pCur from a fresh `li r0,0` issued after the six
// constant stores, with the AddButton argument moves between pCur and pBottom (the #13
// constant-store shape); the last copy stores them from the shared zero (pBottom, the dying store,
// first). Region split + asm-emitted zero reproduce the first form, InitLast the last one.
inline void cDbgOkCancelWindow::Init(int wx, int wy, const char* name)
{
    x = wx;
    y = wy;
    w = strlen(name);
    h = 1;
    cxMax = 1;
    cyMax = 1;
    pName = name;
    x1C = 0;
    x20 = 0;
    num = 0;
    do { } while (0); // COMPILER-DIFF: #13 (sched region split)
    {
        cDbgButton* z;
        asm("li %0,0" : "=r"(z) : "m"(w)); // COMPILER-DIFF: #13 (asm-emitted zero, reload-placed li)
        pBottom = pTop = pCur = z;
    }
    AddButton(1, 2, " [OK] ", 0, 0, 0, 0);
    AddButton(9, 2, "[CANCEL]", 1, 0, 0, 0);
}

inline void cDbgOkCancelWindow::InitLast(int wx, int wy, const char* name)
{
    x = wx;
    y = wy;
    w = strlen(name);
    h = 1;
    cxMax = 1;
    cyMax = 1;
    pName = name;
    x1C = 0;
    x20 = 0;
    num = 0;
    do { } while (0); // COMPILER-DIFF: #13 (sched region split)
    pBottom = pTop = pCur = 0;
    AddButton(1, 2, " [OK] ", 0, 0, 0, 0);
    AddButton(9, 2, "[CANCEL]", 1, 0, 0, 0);
}

inline int cDbgOkCancelWindow::LocalUpdate()
{
    int ret = 1;
    int bcx;
    int bcy;
    u32 rep;
    u32 trg;
    cDbgButton* b;

    bcx = GetCx();
    bcy = GetCy();
    rep = Joy[0].rep;
    if (rep & 0x10001) {
        bcx--;
    }
    if (rep & 0x20002) {
        bcx++;
    }
    if (rep & 0x80008) {
        bcy--;
    }
    if (rep & 0x40004) {
        bcy++;
    }
    if (bcx < 0) {
        bcx = cxMax;
    }
    if (bcy < 0) {
        bcy = cyMax;
    }
    if (bcx > cxMax) {
        bcx = 0;
    }
    if (bcy > cyMax) {
        bcy = 0;
    }
    if (bcx != GetCx() || bcy != GetCy()) {
        if (FindButton(bcx, bcy, &b)) {
            pCur = b;
        }
    }
    if (Joy[0].trg & 0x100) {
        cDbgButton* c = pCur;

        if (c && c->pFunc) {
            c->pFunc(c);
        }
    }
    ButtonAllUpdate();
    trg = Joy[0].trg;
    if (trg & 0x100) {
        ret = 0;
    }
    if (trg & 0x200) {
        ret = 0;
        SetCurrentBottomButton();
    }
    return ret;
}

// A button of the work editor: the callbacks get the work index and the work.
template <class T> class cDbgButtonTemplate : public cDbgButtonBase {
public:
    int (*pFunc)(int no, T* w, cDbgButtonTemplate<T>* b);     // 0x1C  exec callback (returns 0 when done)
    void (*pUpdate)(int no, T* w, cDbgButtonTemplate<T>* b);  // 0x20  label update, every frame

    cDbgButtonTemplate(int x_, int y_, const char* name, int cx_, int cy_) { Init(x_, y_, name, cx_, cy_); }
    virtual ~cDbgButtonTemplate() {}
};

// The work-list editor window: `rows` visible rows of an array of `numWork` works, scrolled by
// `top`; one button per column and row; copy/cut/paste of whole works through `buf`.
template <class T> class cDbgEditWindow : public cDbgWindowBase {
public:
    T* pWork;                          // 0x28
    u32 numWork;                       // 0x2C
    u32 rows;                          // 0x30
    int top;                           // 0x34  first displayed work
    int execMode;                      // 0x38  a button's exec callback is running
    int copyCursor;                    // 0x3C  0 COPY 1 CUT 2 PASTE
    int copyWinMode;                   // 0x40  the copy window is open
    int bufValid;                      // 0x44
    T buf;                             // 0x48
    u32 num;                           // number of buttons
    cDbgButtonTemplate<T>* pButton[128];
    cDbgButtonTemplate<T>* pCur;
    cDbgButtonTemplate<T>* pTop;
    cDbgButtonTemplate<T>* pBottom;
    int (*pIsWorkAlive)(T* w);
    void (*pSetWorkAlive)(T* w, int alive);
    int (*pGetWorkNo)(T* w);
    void (*pSetWorkNo)(T* w, int no);
    void (*pInitWork)(T* w, int no);

    cDbgEditWindow(int wx, int wy, const char* name, T* work, u32 n, u32 nRows)
    {
        u32 i;

        x = wx;
        y = wy;
        w = strlen(name);
        // One codeless RA-time insn between `stw x` and `stw pWork` (sched1 issues it at c11 beside
        // `stw w`; every earlier slot goes to a higher-priority insn): local-alloc then sees `pWork`
        // stored two suids later, so `work`'s qty (2 refs / 50) ranks below the `4` constant (2/48)
        // and takes r28 while `li 4` / `li 5` share r29 (ToolEspArea's e08-e98). Reading `name`
        // adds no ref to the ranked qtys; `"=m"(x)` is one true dependence and no store.
        asm("" : "=m"(x) : "r"(name)); // COMPILER-DIFF: candidate (local-alloc qty order)
        h = 1;
        cxMax = 1;
        cyMax = 1;
        pName = name;
        x1C = 0;
        x20 = 0;
        // rows before pWork/numWork: the dying stores come out rows, pWork, numWork and the
        // rows constant (lower LUID) is the one hoisted above the strlen call
        rows = nRows;
        pWork = work;
        numWork = n;
        top = 0;
        execMode = 0;
        copyCursor = 0;
        copyWinMode = 0;
        bufValid = 0;
        num = 0;
        pCur = 0;
        // the last six zero stores form their own sched region (pSetWorkNo, the dying one, first).
        // The LOOP_END note also blinds cse1 to everything after it, so the dead test below reaches
        // gcse unfolded.
        do { } while (0); // COMPILER-DIFF: #13 (sched region split)
        // Emit-nothing sched barrier: sched2 must not see a real insn as the first one after the
        // notes (that insn would be forced to issue first; the target's region starts with the
        // free `lis; lis | cmpwi; stw` schedule). The volatile asm is the barrier at both sched
        // passes and produces no code; gcse's end-of-block insertions (the hoisted compare, the
        // `Joy`/`pPL` highs) land between it and the jump, i.e. inside this region.
        asm(""); // COMPILER-DIFF: codeless sched barrier
        // Dead test on an already-stored parameter: the jump survives cse1 (blind, see above) and
        // splits the block at gcse; PRE then inserts `cmpwi edit,0` (the `edit == 0` test after
        // the AddButton loop, carried in a CR field via mfcr/mtcrf) at the end of this block.
        // The constant `n` folds the test before flow1 (gcse cprop / cse2), flow1 merges the
        // blocks again and the zero stays block-local (r0). `n` must be a parameter that is written here: a read-only inline
        // parameter is replaced by its constant actual and the test folds at expand time.
        if (n > 128) n = 128; // COMPILER-DIFF: dead test (gcse block boundary)
        pTop = 0;
        pBottom = 0;
        pIsWorkAlive = 0;
        pSetWorkAlive = 0;
        pGetWorkNo = 0;
        pSetWorkNo = 0;
        {
            // pointer locals: the full addresses live in callee-saved registers (`mr r6/r10`
            // inside the loop); literal arguments keep the `addi` in the loop body
            const char* label = "00";
            void (*cb)(int, T*, cDbgButtonTemplate<T>*) = NoButtonUpdate_callback;
            for (i = 0; i < rows; i++) {
                AddButton(0, i, label, 0, i, 0, cb);
            }
        }
    }
    virtual ~cDbgEditWindow()
    {
        u32 i;

        for (i = 0; i < num; i++) {
            if (pButton[i]) {
                delete pButton[i];
            }
        }
    }

    void AddButton(int bx, int by, const char* name, int bcx, int bcy, int (*func)(int, T*, cDbgButtonTemplate<T>*),
                   void (*update)(int, T*, cDbgButtonTemplate<T>*));
    int FindButton(int bcx, int bcy, cDbgButtonTemplate<T>** out);
    void copyBuffer();
    void cutBuffer();
    void pasteBuffer();
    int execCopyWindow();
    virtual int LocalUpdate();
    virtual void LocalDisp();

    // work index under the cursor
    int GetCurrentNo()
    {
        if (pCur) {
            return pCur->cy + top;
        }
        return 0;
    }
    // tool-style integer address: the index is the first `add` operand
    T* WorkPtr(int no) { return (T*) (no * sizeof(T) + (u32) pWork); }
#line 571 "D:/Bio4/Prog/db_toolbase.h"
    int IsWorkAlive(T* w) { if (pIsWorkAlive) { return pIsWorkAlive(w); } DBG_TOOL_HALT(); return 0; }
    void SetWorkAlive(T* w, int alive) { if (pSetWorkAlive) { pSetWorkAlive(w, alive); return; } DBG_TOOL_HALT(); }
    int GetWorkNo(T* w) { if (pGetWorkNo) { return pGetWorkNo(w); } DBG_TOOL_HALT(); return 0; }
    void SetWorkNo(T* w, int no) { if (pSetWorkNo) { pSetWorkNo(w, no); return; } DBG_TOOL_HALT(); }
    void InitWork(T* w, int no) { if (pInitWork) { pInitWork(w, no); return; } DBG_TOOL_HALT(); }
#line 403 "include/dbg_tool.h"

    // the "00" work-number column
    static void NoButtonUpdate_callback(int no, T* w, cDbgButtonTemplate<T>* b)
    {
        static char digits[] = "0123456789";
        u32 n = no;
        char buf[3];

        if (n > 99) {
            n = 99;
        }
        buf[2] = 0;
        buf[0] = digits[n / 10];
        buf[1] = digits[n % 10];
        DbgButtonSetName(b, buf);
    }

    virtual void SetCurrentTopButton() { pCur = pTop; }
    virtual int GetCx()
    {
        if (pCur == 0) {
            return 0;
        }
        return pCur->cx;
    }
    virtual int GetCy()
    {
        if (pCur == 0) {
            return 0;
        }
        return pCur->cy;
    }
    virtual void SetCurrentBottomButton() { pCur = pBottom; }
    virtual void ButtonAllUpdate()
    {
        u32 i;

        for (i = 0; i < num; i++) {
            cDbgButtonTemplate<T>* b = pButton[i];
            int no = b->cy + top;

            if (b) {
                T* w = WorkPtr(no);

                if (b->pUpdate) {
                    b->pUpdate(no, w, b);
                }
            }
        }
    }
    virtual void ButtonPushCheck()
    {
        if (Joy[0].trg & 0x100) {
            cDbgButtonTemplate<T>* b = pCur;

            if (b) {
                int no = b->cy + top;

                if (b->cx == 0) {
                    if (IsWorkAlive(WorkPtr(no))) {
                        SetWorkAlive(WorkPtr(no), 0);
                    } else {
                        SetWorkAlive(WorkPtr(no), 1);
                    }
                } else if (IsWorkAlive(WorkPtr(no))) {
                    execMode = 1;
                }
            }
        }
        if (Joy[0].trg & 0x800) {
            copyWinMode = 1;
            copyCursor = 0;
        }
    }
};

template <class T>
void cDbgEditWindow<T>::AddButton(int bx, int by, const char* name, int bcx, int bcy,
                                  int (*func)(int, T*, cDbgButtonTemplate<T>*),
                                  void (*update)(int, T*, cDbgButtonTemplate<T>*))
{
    cDbgButtonTemplate<T>* b;

    pButton[num] = b = new cDbgButtonTemplate<T>(bx, by, name, bcx, bcy);
    b->pUpdate = update;
    b->pFunc = func;
    if (pButton[num] == 0) {
        pLog->err(0, 0, "AddButton(): new failed.");
        return;
    }
    if (w < bx + strlen(name)) {
        w = bx + strlen(name);
    }
    if (h < by) {
        h = by;
    }
    if (pCur == 0) {
        pTop = pCur = pButton[num];
    }
    pBottom = pButton[num];
    if (cxMax < bcx) {
        cxMax = bcx;
    }
    if (cyMax < bcy) {
        cyMax = bcy;
    }
    num++;
}

template <class T> void cDbgEditWindow<T>::copyBuffer()
{
    bufValid = 1;
    buf = pWork[GetCurrentNo()];
}

template <class T> void cDbgEditWindow<T>::cutBuffer()
{
    u32 i;
    int cur;

    copyBuffer();
    cur = GetCurrentNo();
    for (i = cur; i < numWork - 1; i++) {
        pWork[i] = pWork[i + 1];
    }
    InitWork(&pWork[numWork - 1], numWork - 1);
    {
        T* w;
        u32 j;

        for (j = 0, w = pWork; j < numWork; j++, w++) {
            SetWorkNo(w, j);
        }
    }
}

template <class T> void cDbgEditWindow<T>::pasteBuffer()
{
    int cur = GetCurrentNo();
    int i;

    for (i = numWork - 2; i >= cur; i--) {
        pWork[i + 1] = pWork[i];
    }
    pWork[cur] = buf;
    {
        T* w;
        u32 j;

        for (j = 0, w = pWork; j < numWork; j++, w++) {
            SetWorkNo(w, j);
        }
    }
}

template <class T> int cDbgEditWindow<T>::execCopyWindow()
{
    if (Joy[0].trg & 0x200) {
        return 0;
    }
    if (Joy[0].rep & 0x80008) {
        copyCursor--;
    }
    if (Joy[0].rep & 0x40004) {
        copyCursor++;
    }
    if (copyCursor < 0) {
        copyCursor = 3;
    }
    if (copyCursor > 2) {
        copyCursor = 0;
    }
    if (Joy[0].trg & 0x100) {
        switch (copyCursor) {
        case 0:
            copyBuffer();
            break;
        case 1:
            cutBuffer();
            break;
        case 2:
            pasteBuffer();
            break;
        }
        return 0;
    }
    eprintf2(8, 12, 0x28, 0x8C, 0x12, 0, " EDIT");
    eprintf2(8, 12, 0x28, 0x9A, 0, 0, " COPY");
    eprintf2(8, 12, 0x28, 0xA8, 0, 0, " CUT");
    eprintf2(8, 12, 0x28, 0xB6, 0, 0, " PASTE");
    if (pG->flags_51E4 & 4) {
        eprintf2(8, 12, 0x28, (copyCursor + 11) * 14, 0, 0, ">");
    }
    return 1;
}

template <class T> int cDbgEditWindow<T>::FindButton(int bcx, int bcy, cDbgButtonTemplate<T>** out)
{
    u32 i;

    *out = 0;
    for (i = 0; i < num; i++) {
        if (pButton[i]->cx == bcx && pButton[i]->cy == bcy) {
            *out = pButton[i];
            return 1;
        }
    }
    return 0;
}

template <class T> int cDbgEditWindow<T>::LocalUpdate()
{
    int ret = 1;

    if (copyWinMode) {
        if (execCopyWindow() == 0) {
            copyWinMode = 0;
        }
    } else if (execMode) {
        int no = GetCurrentNo();
        T* w = WorkPtr(no);
        int r;

        if (pCur->pFunc) {
            r = pCur->pFunc(no, w, pCur);
        } else {
            r = 0;
        }
        if (r == 0) {
            execMode = 0;
        }
    } else {
        int bcx;
        int bcy;
        u32 rep;

        bcx = GetCx();
        bcy = GetCy();
        rep = Joy[0].rep;
        if (rep & 0x10001) {
            bcx--;
        }
        if (rep & 0x20002) {
            bcx++;
        }
        if (rep & 0x80008) {
            bcy--;
        }
        if (rep & 0x40004) {
            bcy++;
        }
        if (bcx < 0) {
            bcx = cxMax;
        }
        if (bcx > cxMax) {
            bcx = 0;
        }
        if (bcy < 0) {
            if (top != 0) {
                top--;
            } else if (Joy[0].trg & 0x80008) {
                top = numWork - cyMax - 1;
                bcy = cyMax;
            } else {
                top = 0;
                bcy = 0;
            }
        }
        if (bcy > cyMax) {
            u32 last = numWork - cyMax - 1;

            if ((u32) top < last) {
                top++;
            } else if (Joy[0].trg & 0x40004) {
                top = 0;
                bcy = 0;
            } else {
                top = last;
                bcy = cyMax;
            }
        }
        if (bcx != GetCx() || bcy != GetCy()) {
            cDbgButtonTemplate<T>* b;

            if (FindButton(bcx, bcy, &b)) {
                pCur = b;
            }
        }
        if (Joy[0].trg & 0x200) {
            ret = 0;
        }
        ButtonPushCheck();
    }
    ButtonAllUpdate();
    return ret;
}

template <class T> void cDbgEditWindow<T>::LocalDisp()
{
    u32 i;
    cDbgButtonTemplate<T>* cur;

    for (i = 0; i < num; i++) {
        int no = pButton[i]->cy + top;

        if (pButton[i]) {
            int alive = IsWorkAlive(WorkPtr(no));
            int by = y + 1;
            cDbgButtonTemplate<T>* b = pButton[i];
            int bx = x;

            if (alive) {
                eprintf2(8, 12, (bx + b->x) * 8, (by + b->y) * 14, 0x10, 0, b->pName);
            } else {
                eprintf2(8, 12, (bx + b->x) * 8, (by + b->y) * 14, 0x14, 0, b->pName);
            }
        }
    }
    if (execMode == 0) {
        if (pCur) {
            int alive = IsWorkAlive(WorkPtr(pCur->cy + top));
            int by = y + 1;
            int bx = x;

            cur = pCur;
            if (pG->flags_51E4 & 4) {
                eprintf2(8, 12, (bx + cur->x - 1) * 8, (by + cur->y) * 14, 0, 0, ">");
            }
            if (alive) {
                eprintf2(8, 12, (bx + cur->x) * 8, (by + cur->y) * 14, 0, 0, cur->pName);
            } else {
                eprintf2(8, 12, (bx + cur->x) * 8, (by + cur->y) * 14, 0x14, 0, cur->pName);
            }
            {
                f32 fx = (f32) ((bx + cur->x) * 8);
                f32 fh = 14.0f;
                f32 mgn = 2.0f;
                f32 zero = 0.0f;

                DbgDrawBoxFill(fx - mgn, (f32) ((by + cur->y) * 14) - mgn, (f32) (cur->nameLen * 8) + zero,
                               fh + mgn, 0.7f, 0.7f, zero, 0.3f);
            }
        }
    }
}

// The editor tool: menu / edit / load / save / option / exit windows and the mode state machine
// every area editor runs from its Tool* entry.
template <class T> class cDbgToolMain {
public:
    cDbgWindow* pMenu;                 // 0x00
    cDbgEditWindow<T>* pEdit;          // 0x04
    cDbgFileSelectWindow* pLoad;       // 0x08
    cDbgFileSelectWindow* pSave;       // 0x0C
    cDbgOkCancelWindow* pLoadOk;       // 0x10
    cDbgOkCancelWindow* pSaveOk;       // 0x14
    cDbgOkCancelWindow* pExitOk;       // 0x18
    int mode;                          // 0x1C  0 menu 1 edit 2 load 3 save 4 option 5 quit 6 load? 7 save? 8 exit?
    void* saveArg;                     // 0x20
    void* loadArg;                     // 0x24
    void* optionArg;                   // 0x28
    int (*pIsWorkAlive)(T* w);         // 0x2C
    void (*pSetWorkAlive)(T* w, int);  // 0x30
    int (*pGetWorkNo)(T* w);           // 0x34
    void (*pSetWorkNo)(T* w, int);     // 0x38
    void (*pInitWork)(T* w, int);      // 0x3C
    int (*pSaveFunc)(void* arg);       // 0x40  replaces the file window when set
    int (*pLoadFunc)(void* arg);       // 0x44
    int (*pOptionFunc)(void* arg);     // 0x48
    // 0x4C vptr

    cDbgToolMain()
    {
        pMenu = 0;
        pEdit = 0;
        pLoad = 0;
        pSave = 0;
        pLoadOk = 0;
        pSaveOk = 0;
        pExitOk = 0;
        mode = 0;
        saveArg = 0;
        loadArg = 0;
        optionArg = 0;
        pIsWorkAlive = 0;
        pSetWorkAlive = 0;
        pGetWorkNo = 0;
        pSetWorkNo = 0;
        pSaveFunc = 0;
        pLoadFunc = 0;
        pOptionFunc = 0;
    }
    virtual ~cDbgToolMain()
    {
        if (pMenu) {
            delete pMenu;
        }
        if (pEdit) {
            delete pEdit;
        }
        if (pLoad) {
            delete pLoad;
        }
        if (pSave) {
            delete pSave;
        }
        if (pLoadOk) {
            delete pLoadOk;
        }
        if (pSaveOk) {
            delete pSaveOk;
        }
        if (pExitOk) {
            delete pExitOk;
        }
    }

    void CreateMenuWindow()
    {
        cDbgWindow* w = new cDbgWindow;

        w->Init(5, 3, " MENU ");
        pMenu = w;
        if (pMenu == 0) {
            pLog->err(0, 0, "CreateMenuWindow(): new failed.");
            return;
        }
        pMenu->AddButton(1, 0, "Edit  ", 0, 0, 0, 0);
        pMenu->AddButton(1, 1, "Load  ", 0, 1, 0, 0);
        pMenu->AddButton(1, 2, "Save  ", 0, 2, 0, 0);
        pMenu->AddButton(1, 3, "Option", 0, 3, 0, 0);
        pMenu->AddButton(1, 4, "Exit  ", 0, 4, 0, 0);
    }
    // the save / load selectors and the three confirmations; a failure stops the sequence
    void CreateFileWindows(int wx, int wy, const char* path1, const char* path2, const char* ext)
    {
        cDbgFileSelectWindow* save;
        cDbgFileSelectWindow* load;
        cDbgOkCancelWindow* saveOk;
        cDbgOkCancelWindow* loadOk;
        cDbgOkCancelWindow* exitOk;

        save = new cDbgFileSelectWindow;
        DbgFileSelectWindowInit(save, wx, wy, "  Save ", path1, path2, ext);
        DbgFileSelectWindowAddButtons(save);
        pSave = save;
        if (save == 0) {
            pLog->err(0, 0, "CreateSaveWindow(): new failed.");
            return;
        }
        load = new cDbgFileSelectWindow;
        DbgFileSelectWindowInit(load, wx, wy, "  Load ", path1, path2, ext);
        DbgFileSelectWindowAddButtons(load);
        pLoad = load;
        if (load == 0) {
            pLog->err(0, 0, "CreateSaveWindow(): new failed.");
            return;
        }
        saveOk = new cDbgOkCancelWindow;
        saveOk->Init(wx, wy, "    SAVE OK? ");
        pSaveOk = saveOk;
        if (saveOk == 0) {
            pLog->err(0, 0, "CreateMenuWindow(): new failed.");
            return;
        }
        loadOk = new cDbgOkCancelWindow;
        loadOk->Init(wx, wy, "    LOAD OK? ");
        pLoadOk = loadOk;
        if (loadOk == 0) {
            pLog->err(0, 0, "CreateMenuWindow(): new failed.");
            return;
        }
        exitOk = new cDbgOkCancelWindow;
        exitOk->InitLast(wx, wy, "    EXIT OK? "); // COMPILER-DIFF: #13 (the last copy's shape)
        pExitOk = exitOk;
        if (exitOk == 0) {
            pLog->err(0, 0, "CreateMenuWindow(): new failed.");
            return;
        }
    }
    // parameter order (work before name, nRows before n) is the inline entry's pseudo order:
    // `lis work` is issued before `lis name` and the rows constant is the one hoisted above strlen
    void CreateEditWindow(int wx, int wy, T* work, const char* name, u32 nRows, u32 n)
    {
        // frame-only: the original's helper had a T-sized local here (no code refers to it; it is
        // the sizeof(T) gap below the tool's spill slots in both ToolEspArea and ToolLightAreaMain)
        T unused;
        cDbgEditWindow<T>* edit;

        // the pointer local (like the other Create* helpers): the `new` expression's own null test
        // and `edit == 0` are one compare, kept in a CR field across the inlined ctor loop
        edit = new cDbgEditWindow<T>(wx, wy, name, work, n, nRows);
        pEdit = edit;
        if (edit == 0) {
            pLog->err(0, 0, "CreateEditWindow(): new failed.");
        }
    }
    // one button column on the edit window, one button per row; the window pointer is read once
    // (`lwz rE, 4(tool)` before the loop, `lwz 0x30(rE)` per iteration) - not through pEdit
    void AddEditColumn(int x, const char* name, int cx, int (*exec)(int, T*, cDbgButtonTemplate<T>*),
                       void (*update)(int, T*, cDbgButtonTemplate<T>*))
    {
        cDbgEditWindow<T>* e = pEdit;
        u32 i;

        for (i = 0; i < e->rows; i++) {
            e->AddButton(x, i, name, cx, i, exec, update);
        }
    }

    void SetIsWorkAliveFunc(int (*f)(T*))
    {
        pIsWorkAlive = f;
        pEdit->pIsWorkAlive = f;
    }
    void SetSetWorkAliveFunc(void (*f)(T*, int))
    {
        pSetWorkAlive = f;
        pEdit->pSetWorkAlive = f;
    }
    void SetGetWorkNoFunc(int (*f)(T*))
    {
        pGetWorkNo = f;
        pEdit->pGetWorkNo = f;
    }
    void SetSetWorkNoFunc(void (*f)(T*, int))
    {
        pSetWorkNo = f;
        pEdit->pSetWorkNo = f;
    }
    void SetInitWorkFunc(void (*f)(T*, int))
    {
        pInitWork = f;
        pEdit->pInitWork = f;
    }
    // window and work pointer read once (locals); the work pointer steps, numWork/pInitWork are
    // re-read per iteration (calls in the loop)
    void InitAllWork()
    {
        cDbgEditWindow<T>* e = pEdit;
        T* w = e->pWork;
        u32 i;

        for (i = 0; i < e->numWork; i++, w++) {
            e->InitWork(w, i);
        }
    }

#line 1376 "D:/Bio4/Prog/db_toolbase.h"
    int IsWorkAlive(T* w) { if (pIsWorkAlive) { return pIsWorkAlive(w); } DBG_TOOL_HALT(); return 0; }
    void SetWorkAlive(T* w, int alive) { if (pSetWorkAlive) { pSetWorkAlive(w, alive); return; } DBG_TOOL_HALT(); }
    int GetWorkNo(T* w) { if (pGetWorkNo) { return pGetWorkNo(w); } DBG_TOOL_HALT(); return 0; }
    void SetWorkNo(T* w, int no) { if (pSetWorkNo) { pSetWorkNo(w, no); return; } DBG_TOOL_HALT(); }
    void InitWork(T* w, int no) { if (pInitWork) { pInitWork(w, no); return; } DBG_TOOL_HALT(); }
#line 911 "include/dbg_tool.h"

    // reads `filename` into `work` (works are stored by their own number); the tool's own default
    // file at start-up and the load window's file both go through here
    void LoadData(const char* filename, T* work, u32 num)
    {
        u32 i;
        DbgToolFileHeader* mem;

        InitAllWork();
        mem = (DbgToolFileHeader*) Debug_alloc(num * sizeof(T) + sizeof(DbgToolFileHeader), 1);
        if (mem == 0) {
            pLog->err(0, 0, "DataLoad(): alloc failed.");
            return;
        }
        if (HDRead(filename, mem) == 0) {
            pLog->err(0, 0, "DataLoad(): file open failed!!");
            return;
        }
        {
            T* src = (T*) (mem + 1);

            for (i = 0; i < mem->num; i++) {
                work[GetWorkNo(src)] = *src;
                src++;
            }
        }
        Debug_free(mem);
    }
    // the file image of the live works of `work` into `mem` (header + packed works); returns the end.
    // Also used on its own by t_lightarea, which feeds the image to the game every frame.
    T* MakeSaveData(DbgToolFileHeader* mem, T* work, u32 num)
    {
        u32 cnt;
        u32 i;
        T* dst;
        T* src;

        {
            // count loop: the SAME stepped pointer as the copy loop (one multi-set pseudo whose
            // global priority drops below dst's, so dst takes r29 first and the pointer r28 in both
            // loops) with its OWN counter (an `i` shared with the copy loop makes cse canon the entry
            // test to `cmplw i,num`). `cnt = 0` sits between the `work` load and `src = work`: cse's
            // (set REG0 REG1) swap rule needs the load as the previous insn (t_lightarea's own call).
            u32 j;

            cnt = 0;
            src = work;
            for (j = 0; j < num; j++, src++) {
                if (IsWorkAlive(src)) {
                    cnt++;
                }
            }
        }
        memclr_asm(mem, sizeof(DbgToolFileHeader));
        mem->num = cnt;
        src = work; // stepped pointer (an indexed copy source would be `mulli`)
        dst = (T*) (mem + 1);
        for (i = 0; i < num; i++, src++) {
            if (IsWorkAlive(src)) {
                *dst = *src;
                dst++;
            }
        }
        return dst;
    }
    // writes the live works of `work`
    void SaveData(const char* filename, T* work, u32 num)
    {
        DbgToolFileHeader* mem;
        T* end;

        mem = (DbgToolFileHeader*) Debug_alloc(num * sizeof(T) + sizeof(DbgToolFileHeader), 1);
        if (mem == 0) {
            pLog->err(0, 0, "DataSave(): alloc failed.");
            return;
        }
        end = MakeSaveData(mem, work, num);
        HDWrite(filename, mem, (u8*) end - (u8*) mem);
        Debug_free(mem);
    }

    // decide / cancel flags of a window from the pad, before its LocalUpdate
    void KeyCheck(cDbgWindowBase* w)
    {
        w->x1C = 0;
        w->x20 = 0;
        if (Joy[0].trg & 0x100) {
            w->x1C = 1;
        }
        if (Joy[0].trg & 0x200) {
            w->x20 = 1;
        }
    }
    // KeyCheck + LocalUpdate through one pointer parameter: the window pointer is not re-read from
    // the tool after KeyCheck's stores (the original keeps it in a register across them)
    int WinUpdate(cDbgWindowBase* w)
    {
        KeyCheck(w);
        return w->LocalUpdate();
    }
    // the active window: title, single and double frame, then its own display
    void DispWindow(cDbgWindowBase* w)
    {
        eprintf2(8, 12, w->x * 8, w->y * 14, 0x12, 0, w->pName);
        DbgDrawBox(((f32) w->x - 0.5f) * 8.0f - 1.0f, (f32) (w->y * 14) - 1.0f, ((f32) w->w + 1.5f) * 8.0f + 2.0f,
                   14.0f, 0.7f, 0.7f, 0.7f, 0.45f);
        DbgDrawBox(((f32) w->x - 0.5f) * 8.0f - 2.0f, (f32) (w->y * 14) - 2.0f, ((f32) w->w + 1.5f) * 8.0f + 4.0f,
                   (f32) ((w->h + 2) * 14) + 8.0f, 0.6f, 0.6f, 0.6f, 0.7f);
        w->LocalDisp();
    }

    int GetMode() { return mode; }
    cDbgEditWindow<T>* GetEdit() { return pEdit; }

    // returns 0 when the tool has to quit
    int Update()
    {
        int ret = 1;
        int r;

        // arm order 0,1,2,6,3,7,4,5,8 = the target's layout (case 6 right after 2, 7 after 3):
        // t_event SubToolMessMove 274 -> 106, t_lightarea ToolLightAreaMain 462 -> 121 words.
        switch (mode) {
        case 0:
            if (WinUpdate(pMenu) == 0) {
                pMenu->SetCurrentBottomButton();
            }
            if (pMenu->x1C) {
                switch (pMenu->GetCy()) {
                case 0:
                    mode = 1;
                    pEdit->SetCurrentTopButton();
                    break;
                case 1:
                    mode = 2;
                    pLoad->SetCurrentTopButton();
                    break;
                case 2:
                    mode = 3;
                    pSave->SetCurrentTopButton();
                    break;
                case 3:
                    mode = 4;
                    break;
                case 4:
                    mode = 8;
                    pExitOk->SetCurrentBottomButton();
                    break;
                }
            }
            break;
        case 1:
            if (WinUpdate(pEdit) == 0) {
                mode = 0;
            }
            break;
        case 2:
            if (pLoadFunc) {
                if (pLoadFunc(loadArg) == 0) {
                    mode = 0;
                }
            } else {
                r = WinUpdate(pLoad);
                if (r == 0) {
                    pSave->fileNo = pLoad->fileNo;
                    if (pLoad->GetCy() == 1) {
                        pLoadOk->SetCurrentBottomButton();
                        mode = 6;
                    } else {
                        mode = 0;
                    }
                }
            }
            break;
        case 6:
            if (WinUpdate(pLoadOk) == 0) {
                if (pLoadOk->GetCx() == 0) {
                    LoadData(pLoad->fileName, pEdit->pWork, pEdit->numWork);
                }
                mode = 0;
            }
            break;
        case 3:
            if (pSaveFunc) {
                if (pSaveFunc(saveArg) == 0) {
                    mode = 0;
                }
            } else {
                r = WinUpdate(pSave);
                if (r == 0) {
                    pLoad->fileNo = pSave->fileNo;
                    if (pSave->GetCy() == 1) {
                        pSaveOk->SetCurrentBottomButton();
                        mode = 7;
                    } else {
                        mode = 0;
                    }
                }
            }
            break;
        case 7:
            if (WinUpdate(pSaveOk) == 0) {
                if (pSaveOk->GetCx() == 0) {
                    SaveData(pSave->fileName, pEdit->pWork, pEdit->numWork);
                }
                mode = 0;
            }
            break;
        case 4:
            if (pOptionFunc) {
                if (pOptionFunc(optionArg) == 0) {
                    mode = 0;
                }
            } else if (Joy[0].on & 0x200) {
                mode = 0;
            }
            break;
        case 8:
            r = WinUpdate(pExitOk);
            if (r == 0) {
                if (pExitOk->GetCx() == 0) {
                    mode = 5;
                } else {
                    mode = 0;
                }
            }
            break;
        case 5: // last arm: its `li ret,0` falls through into the caller's `cmpwi ret,0`
            ret = 0;
            break;
        }
        return ret;
    }

    void Disp()
    {
        switch (mode) {
        case 0:
            DispWindow(pMenu);
            break;
        case 1:
            DispWindow(pEdit);
            break;
        case 2:
            if (pLoadFunc == 0) {
                DispWindow(pLoad);
            }
            break;
        case 3:
            if (pSaveFunc == 0) {
                DispWindow(pSave);
            }
            break;
        case 4: // empty labels shape the compare tree (`cmpwi 4; bge` node in the original)
        case 5:
            break;
        case 6:
            DispWindow(pLoadOk);
            break;
        case 7:
            DispWindow(pSaveOk);
            break;
        case 8:
            DispWindow(pExitOk);
            break;
        }
    }
};

#endif
