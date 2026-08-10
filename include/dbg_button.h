#ifndef DBG_BUTTON_H
#define DBG_BUTTON_H

#include "types.h"
#include "db_log.h"

// Debug button-menu classes (header only; the original file name is unknown, it has no range
// check). event.cpp and sscrn.cpp include it after event.h: only the string literals of its
// inline members land in their .rodata, in this order, and no code (nothing here has a key
// function, so no vtable is written unless a unit constructs one of these). The Sscrn module's
// ss_term.cpp derives its cDbgWindow / cDbgButton from the two bases below (their vtables and
// inline virtuals then come out of that unit).
class cDbgButtonBase {
public:
    u32 x;        // 0x00  text column
    u32 y;        // 0x04  text row
    int cx;       // 0x08  cursor cell
    int cy;       // 0x0C
    char* name;   // 0x10  (allocated; freed by the destructor)
    u32 w;        // 0x14  width in characters
    // 0x18 vptr

    virtual ~cDbgButtonBase() { delete name; }
    int Init(int n) {
        if (name == 0) {
            pLog->err(0, 0, "cDbgButtonBase::Init(): new failed.");
            return 0;
        }
        w = n;
        return 1;
    }
    const char* cursorMark() { return ">"; }
    const char* nameHeader() { return "Name:                 "; }
    const char* nameBlank() { return "               "; }
    const char* noHeader() { return "No  :"; }
    const char* noBlank() { return " xx "; }
    const char* okMark() { return "[OK]"; }
    const char* fmtNo() { return "%s%02d%s"; }
    const char* fmtName() { return "%s%s%02d%s"; }
    const char* fmtNum() { return " %02d"; }
    const char* okButton() { return " [OK] "; }
    const char* cancelButton() { return "[CANCEL]"; }
};

// Button window base: the window position, the cursor range and the virtual interface the
// derived window (ss_term.cpp cDbgWindow) fills in.
class cDbgWindowBase {
public:
    u32 x;        // 0x00  window column
    u32 y;        // 0x04  window row
    int x8;
    int xC;
    int maxCx;    // 0x10  cursor wraps past this column
    int maxCy;    // 0x14  cursor wraps past this row
    int x18;
    int x1C;
    int x20;
    // 0x24 vptr

    virtual ~cDbgWindowBase() {}
    virtual int GetCx() { return 0; }
    virtual int GetCy() { return 0; }
    virtual void SetCurrentBottomButton() {}
    virtual void ButtonAllUpdate() = 0;
    virtual int LocalUpdate() = 0;
    virtual void LocalDisp() = 0;
};

// A button with an update callback.
class cDbgButton : public cDbgButtonBase {
public:
    int x1C;
    void (*func)(cDbgButton* b);  // 0x20  called by cDbgWindow::ButtonAllUpdate when set

    virtual ~cDbgButton() {}
};

#endif
