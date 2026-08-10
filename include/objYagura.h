#ifndef OBJYAGURA_H
#define OBJYAGURA_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Room-script view of the ladder / tower object (game/objYagura.cpp defines the class with its
// virtual; the rooms only call the out-of-line members, so no vtable is emitted here).
class cObjYagura : public cObj {
public:
    void setMotionVib(void* mot);
    void setVib();
};

cObj* SetYagura(void* bin, void* tpl, Vec* pos, Vec* rot);

#endif
