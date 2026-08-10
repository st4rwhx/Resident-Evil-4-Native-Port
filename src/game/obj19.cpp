#include "obj.h"

// Item pickup model: a static model with a light set, placed once by setItemObj().
class cItemObj : public cObj {
public:
    static const Vec zero;

    cItemObj();
    virtual void move();
};

// A file-scope `static const` would be deferred to the end of the unit (after the cManager
// strings); a class static member is emitted here, in front of the two function-local ones.
const Vec cItemObj::zero = { 0.0f, 0.0f, 0.0f };

cItemObj::cItemObj()
{
    static const Vec p1 = { 1000.0f, 1000.0f, 0.0f };

    sub2B4.clrFlags(0xFCFF);
    lightInfo.init2(0, 1, &zero, &p1, 4);
}

void cItemObj::move()
{
    matUpdate();
}

cObj* setItemObj(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    static const Vec p1 = { 500.0f, 500.0f, 0.0f };
    cObj* obj;

    obj = ObjMgr.create(0x19);
    if (obj == 0) {
        return 0;
    }
    if (obj->modelInit(bin, tpl) == 0) {
        return 0;
    }
    obj->be_flag |= 0x4000;
    obj->pos = *pos;
    obj->rot = *rot;
    obj->setNoSuspend(1);
    obj->lightInfo.init2(1, 1, &cItemObj::zero, &p1, 0x20);
    return obj;
}
