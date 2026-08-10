#ifndef OBJTROLLEY_H
#define OBJTROLLEY_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Room-script view of the mine trolley (game/objTrolley.cpp defines the class with its virtuals; the
// rooms only call the out-of-line members, so no vtable is emitted here).
class cObjTrolley : public cObj {
public:
    void setMotion(void** tbl);
    void setStart();
    void set2ndStart();
    int ckStop();
};

cObj* SetTrolley(void* bin, void* tpl, Vec* pos, Vec* rot);

#endif
