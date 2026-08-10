#ifndef OBJ15_H
#define OBJ15_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Room-script view of the gatling gun object (game/obj15.cpp defines the full class with its
// virtuals; this declares only the out-of-line members the rooms call, so no vtable is emitted here).
class cObjGatling : public cObj {
public:
    void setEat(void* data, int type);   // EatMgr.create(data, 0, &pos, &rot, type)
};

// game/obj15.cpp: creates the gatling object from the room archive model (r209 GatlingAppear).
cObjGatling* SetObjGatling(void* bin, void* tpl, Vec* pos, Vec* rot);

#endif
