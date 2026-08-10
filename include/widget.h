#ifndef WIDGET_H
#define WIDGET_H

#include "types.h"
#include "db_log.h"
#include "main_mem.h"

#line 8 "D:/Bio4/Prog/widget.h"

// Header-only widget helpers. Contents unknown; the debug tools include it after light.h and
// its range check emits the file-name string into their .rodata. The file name is one string
// shared with the Widget<T> constructor below: a STRING_CST of an instantiated template body is not
// merged with the parse-time copy by output_constant_def (two copies), the inlined helper's
// SYMBOL_REF is.
static inline const char* widgetFileName()
{
    return __FILE__;
}

class cWidget {
public:
    u8* pData;
    u32 nData;

    u8* getData(u32 no) {
        if (no >= nData) {
            dbgAssert(widgetFileName(), __LINE__);
        }
        return pData + no;
    }
};

// Screen state machine node (Sscrn module: Widget<SUB_SCREEN>). Each widget owns a table of `num`
// links to other widgets; transit(no) quits this one and inits link[no], which becomes the current
// widget of the chain (`cur`). The link table comes from MEM_ALLOC at header line 89 (the module
// units' "D:/Bio4/Prog/widget.h" 0x59 mem_alloc calls). Virtuals in vtable order: dtor, init,
// quit, move.
#line 77 "D:/Bio4/Prog/widget.h"
template <class T>
class Widget {
public:
    int num;         // 0x00  link table size
    Widget** link;   // 0x04
    Widget* cur;     // 0x08  current widget of the chain (transit target)
    // 0x0C vptr

    Widget(int n = 1) {
        int i;
        num = n;
        cur = this;
        link = (Widget**) mem_alloc(sizeof(Widget*) * num, widgetFileName(), __LINE__, 1, 13);
        for (i = 0; i < num; i++) {
            link[i] = 0;
        }
    }
    virtual ~Widget() { Mem_free(link); }
    virtual void init(T* wk) {}
    virtual void quit(T* wk) {}
    virtual void move(T* wk) {}

    void connect(int no, Widget* w) {
        if (no < num) {
            link[no] = w;
        } else {
            pLog->err(0, 0, "Widget::connect() lack of table.");
        }
    }
    void transit(int no, T* wk) {
        if (link[no]) {
            quit(wk);
            cur = link[no];
            cur->init(wk);
            cur->cur = cur;
        } else {
            pLog->err(0, 0, "Widget::transit() jump to NULL.");
        }
    }
};

#endif
