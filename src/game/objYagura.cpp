#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "map_obj.h"
#include "widget.h"
#include "obj.h"
#include "global.h"

// Ladder (yagura = tower): a static collision model that can play a vibration motion.
class cObjYagura : public cObj {
public:
    virtual void move();

    void setMotionVib(void* mot);
    void setVib();
};

extern "C" {
int MotionMove(cModel* m, int a);
void objYagura_R0_Set(cObjYagura* obj);
}
int MotionSetCore(cModel* m, void* work, void* mot, int a, int b, int c, int d);

void (*ObjYagura_R0_move_tbl[1])(cObjYagura*) = { objYagura_R0_Set };

cObj* SetYagura(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    YaguraWork* w;

    obj = ObjMgr.create(0x39);
    if (obj == 0) {
        return 0;
    }
    w = &obj->yagura;
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetLadder() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 5000.0f, 5000.0f, 5000.0f };

    obj->lightInfo.init2(0, 1, &p0, &p1, 0x10);
    AtariInit(&obj->sub2B4.atari, 0.0f, 1000.0f, -700.0f, 350.0f, 700.0f, 700.0f, 1000.0f, 0, 2, 0);
    obj->sub2B4.atari.throughOn();
    if (pos) {
        obj->pos = *pos;
    } else {
        obj->pos.x = 0.0f;
        obj->pos.y = 0.0f;
        obj->pos.z = 0.0f;
    }
    obj->oldPos = obj->pos;
    if (rot) {
        obj->rot = *rot;
    } else {
        obj->rot.x = 0.0f;
        obj->rot.y = 0.0f;
        obj->rot.z = 0.0f;
    }
    w->pMotionVib = 0;
    obj->xFC = 0;
    obj->xFD = 0;
    obj->xFE = 0;
    obj->xFF = 0;
    return obj;
}

void cObjYagura::move()
{
    ObjYagura_R0_move_tbl[xFC](this);
}

void objYagura_R0_Set(cObjYagura* obj)
{
    if (obj->pMotion) {
        if (MotionMove(obj, 0)) {
            obj->pMotion = 0;
        }
    } else {
        obj->matUpdate();
    }
}

void cObjYagura::setMotionVib(void* mot)
{
    yagura.pMotionVib = mot;
}

void cObjYagura::setVib()
{
    if (yagura.pMotionVib) {
        MotionSetCore(this, &pMotion, yagura.pMotionVib, 0, 0, 0, 0);
    }
}

// The next unit's .sdata starts 8-byte aligned in the original link.
asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
