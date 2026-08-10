#ifndef OBJROBO_H
#define OBJROBO_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Room-script view of the walking Salazar statue (game/objRobo.cpp defines the class with its
// virtuals; the rooms only call the out-of-line members, so no vtable is emitted here). The work is
// cObj::robo (RoboWork in obj.h).
class cObjRobo : public cObj {
public:
    void WalkSequence(cObjRobo* robo, int hitCk);
};

cObj* SetObjRobo(void* bin, void* tpl, Vec* pos, Vec* rot);

// SetBeginEvent / SetEndEvent are parameterless in objRobo.cpp; the r226 build passed one
// argument (`li r4, 0` at every call).
void cObjRoboSetBeginEvent(cObjRobo* robo, int a) asm("SetBeginEvent__8cObjRobo");
void cObjRoboSetEndEvent(cObjRobo* robo, int a) asm("SetEndEvent__8cObjRobo");

#endif
