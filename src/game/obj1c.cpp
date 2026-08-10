#include "atari.h"
#include "light.h"
#include "obj.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "rnd.h"

// Floating island (the lake raft): drifts back to its home position, gets pushed and plays a
// crash motion when hit, spawns water effects while alive.
class cObj1c : public cObj {
public:
    virtual void move();

    void setMotion(void* idle, void* crash, void* idleBig, void* crashBig);
    void setCrash();
    void setCrashBig(Vec* from);
    int ckCrash();
};

extern "C" {
int MotionMove(cModel* m, int a);
u8 EspPullCoreKind();
void EffectEspDelete(int a, int kind, cObj* obj, int b);
void EffectEspgenDelete(int a, int kind, cObj* obj);
void EffectEfmDelete(int a, int kind, cObj* obj);
void obj1c_R1_Set(cObj1c* obj);
void obj1c_R1_Crash(cObj1c* obj);
void obj1c_R1_CrashBig(cObj1c* obj);
void obj1cSpdMove(cObj1c* obj);
}
int MotionSetCore(cModel* m, void* work, void* mot, int a, int b, int c, int d);

void (*Obj1c_R1_move_tbl[3])(cObj1c*) = { obj1c_R1_Set, obj1c_R1_Crash, obj1c_R1_CrashBig };

cObj* SetFloatIsland(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    IslandWork* w;

    obj = ObjMgr.create(0x1C);
    if (obj == 0) {
        return 0;
    }
    w = &obj->island;
    if (pos) {
        obj->pos = *pos;
    }
    if (rot) {
        obj->rot = *rot;
    }
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetObj1c() modelInit() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 3000.0f, 3000.0f, 0.0f };

    obj->sub2B4.atari.throughOn();
    obj->lightInfo.init2(0, 1, &p0, &p1, 0x10);
    w->x00 = 0;
    w->estTimer = (u8) ((u32) Rnd() % 30);
    w->crashEstWait = 0;
    w->motIdle = 0;
    w->motCrash = 0;
    w->home = obj->pos;
    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    w->espKind = EspPullCoreKind();
    obj->xFD = 0;
    obj->xFC = 1;
    obj->xFE = 0;
    obj->xFF = 0;
    RotMatrix(obj->mat, &obj->rot);
    TransMatrix(obj->mat, &obj->pos);
    ScaleMatrix(obj->mat, &obj->scale);
    obj->partsMatCalc();
    obj->partsWorldCalc();
    return obj;
}

void cObj1c::move()
{
    IslandWork* w = &island;
    u32 f;

    be_flag &= ~0x4000;
    if (w->crashTimer) {
        w->crashTimer--;
    }
    if (w->crashEstWait) {
        w->crashEstWait--;
    }
    Obj1c_R1_move_tbl[xFD](this);
    f = be_flag;
    if ((f & 0x201) == 1) {
        if (pG->flags_5010 & 0x80000) {
            be_flag = f & ~2;
            EffectEspDelete(0, w->espKind, this, 0);
            EffectEspgenDelete(0, w->espKind, this);
            EffectEfmDelete(0, w->espKind, this);
        } else {
            be_flag = f | 2;
        }
    }
}

void obj1c_R1_Set(cObj1c* obj)
{
    IslandWork* w = &obj->island;

    obj1cSpdMove(obj);
    if (w->estTimer) {
        w->estTimer--;
    } else {
        w->estTimer = 30;
        if (obj->be_flag & 2) {
            EstSet((int) obj, -1, 0, 0, 1, 0, 0, w->espKind, (u32) obj, 0);
        }
    }
    if (obj->pMotion) {
        MotionMove(obj, 0);
    } else {
        RotMatrix(obj->mat, &obj->rot);
        TransMatrix(obj->mat, &obj->pos);
        ScaleMatrix(obj->mat, &obj->scale);
        obj->partsMatCalc();
    }
    obj->partsWorldCalc();
}

void obj1c_R1_Crash(cObj1c* obj)
{
    IslandWork* w = &obj->island;

    obj1cSpdMove(obj);
    if (obj->pMotion) {
        if (MotionMove(obj, 0)) {
            if (w->motIdle) {
                if (obj->scale.x >= 1.5f) {
                    MotionSetCore(obj, &obj->pMotion, w->motIdleBig, 0, 0, 5, 0);
                } else {
                    MotionSetCore(obj, &obj->pMotion, w->motIdle, 0, 0, 5, 0);
                }
                obj->xFC = 1;
                obj->xFD = 0;
                obj->xFE = 0;
                obj->xFF = 0;
            }
        }
    } else {
        RotMatrix(obj->mat, &obj->rot);
        TransMatrix(obj->mat, &obj->pos);
        ScaleMatrix(obj->mat, &obj->scale);
        obj->partsMatCalc();
    }
    obj->partsWorldCalc();
}

void obj1c_R1_CrashBig(cObj1c* obj)
{
    IslandWork* w = &obj->island;

    obj1cSpdMove(obj);
    if (obj->pMotion) {
        if (MotionMove(obj, 0)) {
            if (w->motIdle) {
                if (obj->scale.x >= 1.5f) {
                    MotionSetCore(obj, &obj->pMotion, w->motIdleBig, 0, 0, 5, 0);
                } else {
                    MotionSetCore(obj, &obj->pMotion, w->motIdle, 0, 0, 5, 0);
                }
                obj->xFC = 1;
                obj->xFD = 0;
                obj->xFE = 0;
                obj->xFF = 0;
            }
        }
    } else {
        RotMatrix(obj->mat, &obj->rot);
        TransMatrix(obj->mat, &obj->pos);
        ScaleMatrix(obj->mat, &obj->scale);
        obj->partsMatCalc();
    }
    obj->partsWorldCalc();
}

void cObj1c::setMotion(void* idle, void* crash, void* idleBig, void* crashBig)
{
    IslandWork* w = &island;

    w->motIdle = idle;
    w->motCrash = crash;
    w->motIdleBig = idleBig;
    w->motCrashBig = crashBig;
    if (scale.x >= 1.5f) {
        MotionSetCore(this, &pMotion, idleBig, 0, 0, 5, 0);
    } else {
        MotionSetCore(this, &pMotion, idle, 0, 0, 5, 0);
    }
}

void cObj1c::setCrash()
{
    IslandWork* w = &island;

    if (w->motCrash) {
        if (scale.x >= 1.5f) {
            MotionSetCore(this, &pMotion, w->motCrashBig, 0, 0, 1, 0);
        } else {
            MotionSetCore(this, &pMotion, w->motCrash, 0, 0, 1, 0);
        }
        xFC = 1;
        xFE = 0;
        xFD = 1;
        xFF = 0;
    }
    if (w->crashEstWait == 0) {
        w->crashEstWait = 15;
        EstSet((int) this, -1, 0, 0, 1, 1, 0, w->espKind, (u32) this, 0);
    }
}

void cObj1c::setCrashBig(Vec* from)
{
    IslandWork* w = &island;
    Vec dir;

    PSVECSubtract(&pos, from, &dir);
    dir.y = 0.0f;
    if (dir.x == 0.0f && dir.z == 0.0f) {
        dir.z = 1.0f;
    } else {
#line 345 "D:/Bio4/Prog/obj1c.cpp"
        VECNormalize(&dir, &dir);
    }
    PSVECScale(&dir, &w->spd, 300.0f);
    if (w->motCrash) {
        if (scale.x >= 1.5f) {
            MotionSetCore(this, &pMotion, w->motCrashBig, 0, 0, 1, 0);
        } else {
            MotionSetCore(this, &pMotion, w->motCrash, 0, 0, 1, 0);
        }
        xFC = 1;
        xFD = 2;
        xFE = 0;
        xFF = 0;
    }
    if (w->crashEstWait == 0) {
        w->crashEstWait = 15;
        EstSet((int) this, -1, 0, 0, 1, 1, 0, w->espKind, (u32) this, 0);
    }
    w->crashTimer = 15;
}

int cObj1c::ckCrash()
{
    if (island.crashTimer) {
        return 1;
    }
    return 0;
}

void obj1cSpdMove(cObj1c* obj)
{
    IslandWork* w = &obj->island;
    Vec d;

    if (w->spd.x == 0.0f || w->spd.z == 0.0f) {
        PSVECSubtract(&w->home, &obj->pos, &d);
        if (d.x * d.x + d.z * d.z > 2500.0f) {
#line 408 "D:/Bio4/Prog/obj1c.cpp"
            VECNormalize(&d, &d);
            PSVECScale(&d, &d, 50.0f);
            PSVECAdd(&obj->pos, &d, &obj->pos);
        }
    } else {
        if (w->spd.x * w->spd.x + w->spd.z * w->spd.z < 2500.0f) {
            w->spd.x = 0.0f;
            w->spd.z = 0.0f;
        } else {
            PSVECAdd(&obj->pos, &w->spd, &obj->pos);
            PSVECScale(&w->spd, &w->spd, 0.9f);
        }
    }
}
