#include "atari.h"
#include "event.h"
#include "obj.h"
#include "global.h"
#include "db_log.h"
#include "va_ppc.h"
#include "main_mem.h"
#include "pl_wep.h"

// Map object manager (ObjMgr): 0x3D8-byte cObj works, constructed by id (construct), moved once per
// frame (move / objMove). The per-id classes live in the obj* units; only their constructors are
// needed here.

#define HALT()                                                    \
    do {                                                          \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    } while (0)

extern "C" {
void OSReport(const char* fmt, ...);
void ShapeMove(cModelInfo* info);    // shape.cpp
void DrawOba(cModel* m);             // at_mod.cpp
void objMove(cObj* p);
}

// Per-id classes constructed by cObjMgr::construct. Each declares its `move` so the vtable stays
// with the unit that defines it (a class without a key function would emit a linkonce copy here).
class cObj00 : public cObj {
public:
    virtual void move();
};
class cObj01 : public cObj {
public:
    virtual void move();
};
class cObjScr : public cObj {
public:
    cObjScr();
    virtual void move();
};
class cObj03 : public cObj {
public:
    cObj03();
    virtual void move();
};
class cObj04 : public cObj {
public:
    virtual void move();
};
class cObj05 : public cObj {
public:
    virtual void move();
};
class cObjBox : public cObj {
public:
    cObjBox();
    virtual void move();
};
class cObj08 : public cObj {
public:
    virtual void move();
};
class cObj09 : public cObj {
public:
    virtual void move();
};
class cWepItem : public cObj {
public:
    virtual void move();
};
class cObj12 : public cObj {
public:
    virtual void move();
};
class cObjLadder : public cObj {
public:
    virtual void move();
};
class cObjBell : public cObj {
public:
    virtual void move();
};
class cObjGatling : public cObj {
public:
    virtual void move();
};
class cObj16 : public cObj {
public:
    virtual void move();
};
class cObj18 : public cObj {
public:
    virtual void move();
};
class cItemObj : public cObj {
public:
    cItemObj();
    virtual void move();
};
class cObjGrenade : public cObj {
public:
    cObjGrenade();
    virtual void move();
};
class cObjSpear : public cObj {
public:
    virtual void move();
};
class cObj1c : public cObj {
public:
    virtual void move();
};
class cObjChain : public cObj {
public:
    virtual void move();
};
class cObjPillar : public cObj {
public:
    virtual void move();
};
class cObjObaModel : public cObj {
public:
    virtual void move();
};
class cObj26 : public cObj {
public:
    virtual void move();
};
class cObjGreFire : public cObj {
public:
    cObjGreFire();
    virtual void move();
};
class cObjGreLight : public cObj {
public:
    cObjGreLight();
    virtual void move();
};
class cObjGondola : public cObj {
public:
    virtual void move();
};
class cObjRobo : public cObj {
public:
    virtual void move();
};
class cObjMissile : public cObj {
public:
    virtual void move();
};
class cObjYagura : public cObj {
public:
    virtual void move();
};
class cObjEgg : public cObj {
public:
    cObjEgg();
    virtual void move();
};
class cObjTrolley : public cObj {
public:
    virtual void move();
};
class cObjBull : public cObj {
public:
    virtual void move();
};

void (*ObjInitFunc[0x40])(cObj*);

cObjMgr::cObjMgr() : cManager<cObj>(sizeof(cObj), 2)
{
    setName("cObjMgr");
    x34 = 0;
}

void cObjMgr::log(const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    pLog->vwarn(6, 0, fmt, ap);
}

#line 130 "D:/Bio4/Prog/obj.cpp"
int cObjMgr::construct(cObj* p, int id)
{
    switch (id) {
    case 0:
        p = new (p) cObj00();
        break;
    case 1:
        p = new (p) cObj01();
        break;
    case 2:
        p = new (p) cObjScr();
        break;
    case 3:
        p = new (p) cObj03();
        break;
    case 4:
        p = new (p) cObj04();
        break;
    case 5:
        p = new (p) cObj05();
        break;
    case 6:
        p = new (p) cObjBox();
        break;
    case 8:
        p = new (p) cObj08();
        break;
    case 9:
        p = new (p) cObj09();
        break;
    case 0xA:
        p = new (p) cWepItem();
        break;
    case 0xB:
        p = new (p) cObjWep();
        break;
    case 0x12:
        p = new (p) cObj12();
        break;
    case 0x13:
        p = new (p) cObjLadder();
        break;
    case 0x14:
        p = new (p) cObjBell();
        break;
    case 0x15:
        p = new (p) cObjGatling();
        break;
    case 0x16:
        p = new (p) cObj16();
        break;
    case 0x18:
        p = new (p) cObj18();
        break;
    case 0x19:
        p = new (p) cItemObj();
        break;
    case 0x1A:
        p = new (p) cObjGrenade();
        break;
    case 0x1B:
        p = new (p) cObjSpear();
        break;
    case 0x1C:
        p = new (p) cObj1c();
        break;
    case 0x1D:
        p = new (p) cObjChain();
        break;
    case 0x20:
        p = new (p) cObjObaModel();
        break;
    case 0x22:
        p = new (p) cObjRocket();
        break;
    case 0x23:
        p = new (p) cObjLauncher();
        break;
    case 0x26:
        p = new (p) cObj26();
        break;
    case 0x29:
        p = new (p) cObjGreFire();
        break;
    case 0x2A:
        p = new (p) cObjGreLight();
        break;
    case 0x35:
        p = new (p) cObjGondola();
        break;
    case 0x37:
        p = new (p) cObjRobo();
        break;
    case 0x38:
        p = new (p) cObjMissile();
        break;
    case 0x39:
        p = new (p) cObjYagura();
        break;
    case 0x3A:
        p = new (p) cObjEgg();
        break;
    case 0x3B:
        p = new (p) cObjTrolley();
        break;
    case 0x3E:
        p = new (p) cObjBull();
        break;
    case 0x1F:
        p = new (p) cObjPillar();
        break;
    default:
        if (id > 0x3F) {
#line 175 "D:/Bio4/Prog/obj.cpp"
            HALT();
        }
        ObjInitFunc[id](p);
        break;
    }
    p->serial = x34;
    x34++;
    p->id = id;
    return 1;
}

int cObjMgr::construct(cObj* p, u32 id)
{
    return construct(p, (int) id);
}

void cObjMgr::move()
{
    cObj* p;
    cObj* n;
    void (*func)(cObj*);

    dieCheck();
    func = objMove;
    p = pAlive;
    while (p) {
        n = p;
        p = (cObj*) p->next;
        func(n);
    }
}

void objMove(cObj* p)
{
    if (!(p->be_flag & 0x20)) {
        return;
    }
    if ((pG->flags_5010 & 0x10000000) && !(p->be_flag & 0x800)) {
        return;
    }
    p->move();
    ShapeMove(p->pInfo);
    p->updateOldPos();
    if (pG->flags_68 & 0x10000000) {
        DrawOba(p);
    }
    if (pG->flags_64 & 0x08000000) {
        p->debugSkeletonDisp();
    }
    if ((int) p->be_flag < 0) {
        p->drawAllBoundingBox(p->pInfo);
    }
}

void cObjMgr::destroy(cObj* p)
{
    if ((p->be_flag & 0x201) != 1) {
        return;
    }
    p->push();
    cManager<cObj>::destroy(p);
}

cObj::cObj()
{
    be_flag |= 0x21;
    x12E = 1;
}

cObjMgr ObjMgr;
