#include "atari.h"
#include "obj.h"
#include "math_sub.h"

extern "C" void MotionMove(cModel* m, int a);

struct ObjScrSwingWork {
    f32 phaseZ;   // 0x00
    f32 ampZ;     // 0x04
    f32 freqZ;    // 0x08
    f32 time;     // 0x0C
    f32 phaseX;   // 0x10
    f32 ampX;     // 0x14
    f32 freqX;    // 0x18
    f32 phaseY;   // 0x1C
    f32 ampY;     // 0x20
    f32 freqY;    // 0x24
    Vec baseRot;  // 0x28
};

struct ObjScrRotWork {
    Vec rotSpd;   // 0x00
    u8 flag;      // 0x0C bit0: rotate the first parts instead of the object
};

// Scripted map object: per-type mover selected by `type`, optional motion and callback.
class cObjScr : public cObj {
public:
    cObjScr();
    virtual void move();

    void moveNormal();
    void moveRotate();
    void moveSwingRot();
    void SetCallBack(void (*func)(cObj*));
    void SetSwingRot(f32 amp, f32 period, f32 phase);
};

cObjScr::cObjScr()
{
    x3D0 = 0;
    callBack = 0;
}

void cObjScr::move()
{
    static void (cObjScr::*funcTbl[16])() = {
        &cObjScr::moveNormal, &cObjScr::moveRotate, &cObjScr::moveSwingRot, &cObjScr::moveNormal,
        &cObjScr::moveNormal, &cObjScr::moveNormal, &cObjScr::moveNormal, &cObjScr::moveNormal,
        &cObjScr::moveNormal, &cObjScr::moveNormal, &cObjScr::moveNormal, &cObjScr::moveNormal,
        &cObjScr::moveNormal, &cObjScr::moveNormal, &cObjScr::moveNormal, &cObjScr::moveNormal,
    };

    (this->*funcTbl[type])();
    if (pMotion) {
        MotionMove(this, 0);
    } else {
        matUpdate();
    }
    if (callBack) {
        callBack(this);
    }
    lightInfo.updateMatrix(this);
}

void cObjScr::moveNormal()
{
}

void cObjScr::moveRotate()
{
    ObjScrRotWork* w = (ObjScrRotWork*)work;

    if (w->flag & 1) {
        PSVECAdd(&pParts->rot, &w->rotSpd, &pParts->rot);
    } else {
        PSVECAdd(&rot, &w->rotSpd, &rot);
    }
}

void cObjScr::moveSwingRot()
{
    ObjScrSwingWork* w = (ObjScrSwingWork*)work;

    if (xFC == 0) {
        w->baseRot.x = rot.x;
        w->baseRot.y = rot.y;
        w->baseRot.z = rot.z;
        xFC = 1;
    }
    w->time += 1.0f;
    rot.x = w->baseRot.x + w->ampX * sinf(w->freqX * w->time + w->phaseX);
    rot.y = w->baseRot.y + w->ampY * sinf(w->freqY * w->time + w->phaseY);
    rot.z = w->baseRot.z + w->ampZ * sinf(w->freqZ * w->time + w->phaseZ);
}

// Never called: the original linker dead-stripped the body (unit in STRIP_UNUSED) and kept its
// pool [1.0, 10000.0, 2pi] right after moveSwingRot's; the body is a guess with that pool.
void cObjScr::SetSwingRot(f32 amp, f32 period, f32 phase)
{
    ObjScrSwingWork* w = (ObjScrSwingWork*)work;
    f32 f = 1.0f / period;

    f *= 10000.0f;
    w->ampY = amp;
    w->freqY = f * 6.2831855f;
    w->phaseY = phase;
}

void cObjScr::SetCallBack(void (*func)(cObj*))
{
    callBack = func;
}
