#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "map_obj.h"
#include "widget.h"
#include "obj.h"
#include "em.h"
#include "emhit.h"
#include "global.h"
#include "math_sub.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "act_btn.h"
#include "snd.h"
#include "esp.h"
#include "est.h"
#include "rnd.h"
#include "quake.h"
#include "pad.h"
#include "player.h"
#include "pl_sub.h"

// Falling pillar (obj 0x1F): breaks (setBreak) or is thrown (setThrow) at the player, who can
// escape with the action button; the escape / die sequences run as player damage routines.
class cObjPillar : public cObj {
public:
    virtual void move();
    virtual ~cObjPillar() {}

    void setMotion(void* mot);
    int ckSet();
    void setBreak(Vec* pos, void* mot, int a);
    void setThrow(void* mot0, void* mot1, void* motEscape, void* plMot, int a);
    void setFall(void* mot0, void* mot1);
};

extern "C" {
int MotionMove(cModel* m, int a);
void objPillar_R0_Set(cObjPillar* obj);
void objPillar_R0_Break(cObjPillar* obj);
void objPillar_R0_Throw(cObjPillar* obj);
void objPillar_R0_Escape(cObjPillar* obj);
void objPillar_R0_Fall(cObjPillar* obj);
void objPillarAtkCk(cObjPillar* obj, Vec* pos);
void EscapeAction(cObjPillar* obj);
void EscapeCamMove();
void EscapeAction2(cObjPillar* obj);
void plemEscape2(cPlayer* pl);
void objPillarEatSet(cObjPillar* obj);
void GameAddPoint(int no);   // game/game.cpp
int EmAtkHitCk(void* atk, Vec* pos, Vec* oldPos, int a);   // em_sub.cpp (obj08/obj12 declare it the same way)
}
void MotionSetCore(cModel* m, void* work, void* mot, int a, int b, int c, int d);
// The original is a `static plemEscape` (emBar.cpp has a global one); the name carries the split's
// address suffix so the report can pair it with the local symbol.
#define plemEscape plemEscape_8003C33C
extern "C" {
static void plemEscape(cPlayer* pl);

// struct view of pPL: the load stays below the preceding member stores (see cam_ctrl.cpp)
struct PlayerPtr {
    cPlayer* p;
};
#define pPLS (((PlayerPtr*) &pPL)->p)
}

void (*ObjPillar_R0_move_tbl[5])(cObjPillar*) = {
    objPillar_R0_Set, objPillar_R0_Break, objPillar_R0_Throw, objPillar_R0_Escape, objPillar_R0_Fall,
};

EmAtkInfo ObjPillar_atk_info = { 1000.0f, 8, 1000, 0, 10, 0 };

Camera Cam;   // escape sequence camera

cObj* SetPillar(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    PillarWork* w;

    obj = ObjMgr.create(0x1F);
    if (obj == 0) {
        return 0;
    }
    w = &obj->pillar;
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
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetLadder() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    obj->setNoSuspend(1);
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 5000.0f, 5000.0f, 5000.0f };

    obj->lightInfo.init2(0, 1, &p0, &p1, 0x10);
    AtariInit(&obj->sub2B4.atari, 0.0f, 0.0f, 0.0f, 400.0f, 400.0f, 400.0f, 5000.0f, 0, 2, 0);
    obj->sub2B4.atari.clrFlag100();
    w->plMot = 0;
    w->plMotA = 0;
    w->basePos = obj->pos;
    w->sat = 0;
    obj->xFC = 0;
    obj->xFD = 0;
    obj->xFE = 0;
    obj->xFF = 0;
    return obj;
}

void cObjPillar::move()
{
    if (pillar.sat) {
        pillar.sat->flags &= ~4;
    }
    ObjPillar_R0_move_tbl[xFC](this);
}

void objPillar_R0_Set(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;

    w->flags |= 1;
    w->basePos = obj->pos;
    obj->matUpdate();
    obj->partsWorldCalc();
    objPillarEatSet(obj);
}

void objPillar_R0_Break(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;
    Mtx inv;
    Vec v;
    u8 step = obj->xFE;

    switch (step) {
    case 0:
        w->timer = (*(u16*) w->motBreak & 0x3FFF) - 10;
        MotionSetCore(obj, &obj->pMotion, w->motBreak, 0, 0, 0x8001, 0);
        w->rnd = Rnd() & 1;
        w->escaped = step;
        w->seHandle = step;
        obj->xFE++;
    case 1:
        if (MotionMove(obj, 0)) {
            obj->be_flag &= ~2;
            obj->xFE++;
        } else {
            objPillarAtkCk(obj, &obj->getPartsPtr(1)->worldPos);
            objPillarAtkCk(obj, &obj->getPartsPtr(2)->worldPos);
            if (w->seHandle == 0) {
                cModel* parts = obj->getPartsPtr(1);

                if ((parts->worldPos.x - pPL->pos.x) * (parts->worldPos.x - pPL->pos.x) +
                    (parts->worldPos.y - pPL->pos.y) * (parts->worldPos.y - pPL->pos.y) +
                    (parts->worldPos.z - pPL->pos.z) * (parts->worldPos.z - pPL->pos.z) < 16000000.0f) {
                    w->seHandle = SndCall(8, 0x2C, &parts->worldPos, 0x31, 0, obj);
                }
            }
            if (w->timer) {
                w->timer--;
            } else {
                obj->alpha -= 0.1f;
                if (obj->alpha < 0.0f) {
                    obj->alpha = 0.0f;
                    obj->be_flag &= ~2;
                }
            }
        }
        break;
    }
    obj->partsWorldCalc();
    if (pG->flags_51C0 & 0x200) {
        ObjMgr.destroy(obj);
        return;
    }
    step = w->escaped;
    if (step == 0) {
        PSMTXInverse(obj->mat, inv);
        PSMTXMultVec(inv, &pPL->pos, &v);
        if (v.x > -2000.0f && v.x < 2000.0f && v.z > -1000.0f) {
            if (w->rnd) {
                ActBtn.set(0x25, 0xB, (int) EscapeAction, (int) obj, 1, 3, 0, 0);
            } else {
                ActBtn.set(0x25, 0xB, (int) EscapeAction, (int) obj, 1, 4, 0, 0);
            }
        }
    }
}

void objPillar_R0_Throw(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;
    Vec d;
    Vec v;
    Mtx m;
    cModel* parts;
    f32 len;
    u8 step = obj->xFE;
    u8 esc;

    switch (step) {
    case 0:
        MotionSetCore(obj, &obj->pMotion, w->motThrow0, 0, 0, 0x8001, 0x1F);
        w->rnd = Rnd() & 1;
        w->escaped = 1;
        EstSet((int) obj, -1, 0, 0, 0x29, 0x22, 0, 0, (u32) obj, (void*) step);
        w->seHandle = step;
        obj->xFE++;
    case 1:
        if (MotionMove(obj, 0)) {
            obj->xFE++;
        }
        break;
    case 2:
        parts = obj->getPartsPtr(0);
        obj->pos = parts->worldPos;
        w->spd.x = 0.0f;
        w->spd.y = 0.0f;
        w->spd.z = 500.0f;
        v = pPLS->pos;
        v.y += 1000.0f;
        PSVECSubtract(&v, &obj->pos, &d);
        len = SQRTF(d.x * d.x + d.z * d.z) / 500.0f;
        if (len == 0.0f) {
            w->spd.y = 0.0f;
        } else {
            w->spd.y = d.y / len;
        }
        PSMTXMultVecSR(obj->mat, &w->spd, &w->spd);
        MotionSetCore(obj, &obj->pMotion, w->motThrow1, 0, 0, 0x8005, 0);
        w->rnd = Rnd() & 1;
        w->escaped = 0;
        w->timer = 90;
        EstSet((int) obj, -1, 0, 0, 0x29, 0x23, 1, 0x40, (u32) obj, 0);
        obj->xFE++;
    case 3:
        PSVECAdd(&obj->pos, &w->spd, &obj->pos);
        MotionMove(obj, 0);
        if (w->seHandle == 0) {
            parts = obj->getPartsPtr(0);
            len = (parts->worldPos.x - pPL->pos.x) * (parts->worldPos.x - pPL->pos.x) +
                  (parts->worldPos.y - pPL->pos.y) * (parts->worldPos.y - pPL->pos.y) +
                  (parts->worldPos.z - pPL->pos.z) * (parts->worldPos.z - pPL->pos.z);
            if (len < 16000000.0f) {
                w->seHandle = SndCall(8, 0x2C, &parts->worldPos, 0x31, 0, obj);
            }
        }
        if (w->timer == 0) {
            obj->alpha -= 0.1f;
            if (obj->alpha < 0.0f) {
                EffectEspDelete(1, 0x40, (u32) obj, 0);
                EffectEspgenDelete(1, 0x40, (int) obj);
                EffectEfmDelete(1, 0x40, (int) obj);
                obj->alpha = 0.0f;
                ObjMgr.destroy(obj);
                return;
            }
        } else {
            w->timer--;
        }
        parts = obj->getPartsPtr(0);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        PSMTXMultVec(obj->mat, &v, &v);
        objPillarAtkCk(obj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1000.0f;
        PSMTXMultVec(obj->mat, &v, &v);
        objPillarAtkCk(obj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 2000.0f;
        PSMTXMultVec(obj->mat, &v, &v);
        objPillarAtkCk(obj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 3000.0f;
        PSMTXMultVec(obj->mat, &v, &v);
        objPillarAtkCk(obj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -1000.0f;
        PSMTXMultVec(obj->mat, &v, &v);
        objPillarAtkCk(obj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -2000.0f;
        PSMTXMultVec(obj->mat, &v, &v);
        objPillarAtkCk(obj, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -3000.0f;
        PSMTXMultVec(obj->mat, &v, &v);
        objPillarAtkCk(obj, &v);
        break;
    }
    obj->partsWorldCalc();
    if (pG->flags_51C0 & 0x200) {
        ObjMgr.destroy(obj);
        return;
    }
    esc = w->escaped;
    if (esc == 0) {
        PSMTXRotRad(m, 'y', GetXZAngle(&obj->oldPos, &obj->pos));
        TransMatrix(m, &obj->pos);
        PSMTXInverse(m, m);
        PSMTXMultVec(m, &pPL->pos, &d);
        if (d.z < 1000.0f) {
            w->escaped = 1;
        }
        if (w->rnd) {
            ActBtn.set(0x25, 0xB, (int) EscapeAction2, (int) obj, 1, 3, 0, 0);
        } else {
            ActBtn.set(0x25, 0xB, (int) EscapeAction2, (int) obj, 1, 4, 0, 0);
        }
    }
}

void objPillar_R0_Escape(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;

    switch (obj->xFE) {
    case 0:
        memcpy((u8*) obj + ((u32) &((cObj*) 0)->pos), &pPL->pos, sizeof(Vec));
        memcpy((u8*) obj + ((u32) &((cObj*) 0)->rot), &pPL->rot, sizeof(Vec));
        MotionSetCore(obj, &obj->pMotion, w->motEscape, 0, 0, 0x8001, 0);
        SndStop(w->seHandle, 0);
        w->seHandle = SndCall(8, 0x2C, &obj->getPartsPtr(0)->worldPos, 0x31, 0, obj);
        obj->xFE++;
    case 1:
        if (MotionMove(obj, 0)) {
            EffectEspDelete(1, 0x40, (u32) obj, 0);
            EffectEspgenDelete(1, 0x40, (int) obj);
            EffectEfmDelete(1, 0x40, (int) obj);
            obj->alpha = 0.0f;
            ObjMgr.destroy(obj);
            return;
        }
        break;
    }
    obj->partsWorldCalc();
    if (pG->flags_51C0 & 0x200) {
        ObjMgr.destroy(obj);
    }
}

void objPillar_R0_Fall(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;
    f32 floor;

    switch (obj->xFE) {
    case 0:
        MotionSetCore(obj, &obj->pMotion, w->motFall0, 0, 0, 0x8004, 0);
        w->spd.x = 0.0f;
        w->spd.y = -100.0f;
        w->spd.z = 0.0f;
        obj->xFE++;
    case 1:
        PSVECAdd(&obj->pos, &w->spd, &obj->pos);
        w->spd.y -= 15.0f;
        floor = EatMgr.getFloor(&obj->oldPos, 600.0f, 100000.0f, 0, 0);
        if (obj->pos.y < floor) {
            obj->pos.y = floor;
            SndCall(8, 0x26, &obj->pos, 0x31, 0, obj);
            MotionSetCore(obj, &obj->pMotion, w->motFall1, 0, 0, 0x8001, 0);
            MotionMove(obj, 0);
            obj->xFE++;
        } else {
            MotionMove(obj, 0);
        }
        break;
    case 2:
        obj->xFE++;
        w->timer = 30;
    case 3:
        MotionMove(obj, 0);
        if (w->timer == 0) {
            obj->alpha -= 0.1f;
            if (obj->alpha < 0.0f) {
                EffectEspDelete(1, 0x40, (u32) obj, 0);
                EffectEspgenDelete(1, 0x40, (int) obj);
                EffectEfmDelete(1, 0x40, (int) obj);
                obj->alpha = 0.0f;
                ObjMgr.destroy(obj);
                return;
            }
        } else {
            w->timer--;
        }
        break;
    }
    obj->partsWorldCalc();
    if (pG->flags_51C0 & 0x200) {
        ObjMgr.destroy(obj);
    }
}

void cObjPillar::setMotion(void* mot)
{
    pillar.motBreak = mot;
}

int cObjPillar::ckSet()
{
    if (pillar.flags & 1) {
        return 1;
    }
    return 0;
}

void cObjPillar::setBreak(Vec* pos, void* mot, int a)
{
    PillarWork* w = &pillar;

    BitOff(w->flags, 1);
    FSet(rot.y, GetXZAngle(pos, &this->pos));
    if (fabsf(Muku(&this->pos, &pPL->pos, rot.y, PI)) < PI / 2) {
        rot.y = GetXZAngle(&this->pos, &pPL->pos);
    }
    w->plMot = mot;
    w->plMotA = a;
    w->target = *pos;
    sub2B4.atari.throughOn();
    xFC = 1;
    xFD = 0;
    xFE = 0;
    xFF = 0;
}

void cObjPillar::setThrow(void* mot0, void* mot1, void* motEscape, void* plMot, int a)
{
    PillarWork* w = &pillar;

    BitOff(w->flags, 1);
    if (fabsf(Muku(&pos, &pPL->pos, rot.y, PI)) < 0.5235988f) {
        rot.y = GetXZAngle(&pos, &pPL->pos);
    }
    w->motThrow0 = mot0;
    w->motThrow1 = mot1;
    w->motEscape = motEscape;
    w->plMot = plMot;
    w->plMotA = a;
    sub2B4.atari.throughOn();
    xFC = 2;
    xFD = 0;
    xFE = 0;
    xFF = 0;
}

void cObjPillar::setFall(void* mot0, void* mot1)
{
    PillarWork* w = &pillar;

    BitOff(w->flags, 1);
    pos = getPartsPtr(0)->worldPos;
    w->motFall0 = mot0;
    w->motFall1 = mot1;
    sub2B4.atari.throughOn();
    xFC = 4;
    xFD = 0;
    xFE = 0;
    xFF = 0;
}

void objPillarAtkCk(cObjPillar* obj, Vec* pos)
{
    PillarWork* w = &obj->pillar;
    EmAtkInfo* atk = &ObjPillar_atk_info;
    int hit;

    if ((s16) pG->pl_life > 500) {
        atk->x0A |= 4;
    } else {
        atk->x0A &= ~4;
    }
    hit = EmAtkHitCk(atk, pos, &w->basePos, 1);
    if (hit) {
        if (hit & 1) {
            pPL->rot.y = GetXZAngle(&pPL->pos, &w->basePos);
            PlSetDamage(8, 0, 0);
            EmPlBloodSet2(obj, pos, 1, 0x29, 0x3D);
            if ((s16) pG->pl_life <= 0) {
                EstSet((int) pPL, -1, 0, 0, 0x29, 0x3A, 0, 0, (u32) pPL, 0);
            } else {
                EstSet((int) pPL, -1, 0, 0, 0x29, 0x3B, 0, 0, (u32) pPL, 0);
            }
            QuakeExec(0, 0, 5, 22.0f, 2);
            SndCall(8, 0x25, &pPL->pos, 0x31, 0, pPL);
            w->escaped = 1;
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        }
        if (hit & 2) {
            QuakeExec(0, 0, 5, 22.0f, 2);
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        }
    }
}

void EscapeAction(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;
    u8 one = 1;

    if (!(pG->flags_51C0 & 0x200)) {
        w->escaped = one;
        SetPlDamage((int) obj, plemEscape);
        GameAddPoint(9);
    }
}

static void plemEscape(cPlayer* pl)
{
    cEm* em = (cEm*) pl;
    cObjPillar* obj = (cObjPillar*) em->dmgType;
    PillarWork* w = &obj->pillar;
    f32 ang;

    em->st.x325 = 2;
    switch (em->xFE) {
    case 0:
        // the dead 0.0f store creates the pool `lis` in this block before the call, so
        // update_equiv_regs leaves it there (callee-saved) instead of moving it after Muku
        ang = 0.0f;
        ang = Muku(&em->pos, &w->target, em->rot.y, PI);
        if (ang < 0.0f) {
            MotionSetCore(em, &em->pMotion, w->plMot, w->plMotA, 3, 0x41, 0);
        } else {
            MotionSetCore(em, &em->pMotion, w->plMot, w->plMotA, 3, 1, 0);
        }
        SndCall(1, 0x48, &em->pos, 0, 0, em);
        SndCall(1, 0x11, &em->getPartsPtr(4)->worldPos, 0, 0, em);
        memclr_asm(&Cam, sizeof(Camera));
        em->x3E0 = 50;
        em->x3E4 = 15;
        em->xFE++;
    case 1:
        EscapeCamMove();
        if (em->x3E4) {
            em->rot.y += Muku(&em->pos, &w->target, em->rot.y, PI / 16);
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        MotionMove(em, 0);
        if (em->frame > 11.7f && em->frame < 12.3f) {
            EstSet(0, -1, &em->pos, 0, 3, 0x13, 0, 0, 0, 0);
            SndCall(5, 5, &em->pos, 0, 0, em);
        }
        if (em->x3E0) {
            em->x3E0--;
        } else {
            EndPlDamage();
        }
        break;
    }
}

void EscapeCamMove()
{
    Vec p0;
    Vec p1;
    Vec hit;
    Vec d;
    f32 len;
    GlobalWork* g = pG;

    Cam.param.fovy = g->Cam.param.fovy;
    p0.x = -376.0f;
    p0.y = 575.0f;
    p0.z = -1831.0f;
    p1.x = -244.0f;
    p1.y = 809.0f;
    p1.z = 52.6f;
    PSMTXMultVec(pPL->mat, &p0, &p0);
    PSMTXMultVec(pPL->mat, &p1, &p1);
    PosToPos(&g->Cam.param.at, &p1, &Cam.param.at, 1.0f);
    PosToPos(&g->Cam.param.pos, &p0, &Cam.param.pos, 1.0f);
    if (EatMgr.hitCheck(&Cam.param.at, &Cam.param.pos, &hit, 0, 0x8000, 0)) {
        PSVECSubtract(&hit, &Cam.param.at, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 875 "D:/Bio4/Prog/objPillar.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&Cam.param.at, &d, &Cam.param.pos);
    }
    {
        Camera* cam = &Cam;
        Vec* cp = &cam->param.pos;
        Vec* ca = &cam->param.at;

        len = (cp->x - ca->x) * (cp->x - ca->x) + (cp->y - ca->y) * (cp->y - ca->y) + (cp->z - ca->z) * (cp->z - ca->z);
        cam->up.x = 0.0f;
        cam->up.y = 1.0f;
        cam->up.z = 0.0f;
        cam->dist = SQRTF(len);
        CameraSetOrientationUp(cam);
        CamCtrl.x250 = (s32) cam;
    }
}

void EscapeAction2(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;

    if (!(pG->flags_51C0 & 0x200)) {
        w->escaped = 1;
        SetPlDamage((int) obj, plemEscape2);
        obj->xFC = 3;
        obj->xFD = 0;
        obj->xFE = 0;
        obj->xFF = 0;
    }
}

void plemEscape2(cPlayer* pl)
{
    cEm* em = (cEm*) pl;
    cObjPillar* obj = (cObjPillar*) em->dmgType;
    PillarWork* w = &obj->pillar;
    u8 step;

    em->st.x325 = 2;
    step = em->xFE;
    switch (step) {
    case 0:
        em->rot.y = GetXZAngle(&em->pos, &w->basePos);
        MotionSetCore(em, &em->pMotion, w->plMot, w->plMotA, 0, 1, 0);
        EstSet((int) em, -1, 0, 0, 0x29, 0x39, 0, 0, (u32) em, (void*) step);
        SndCall(1, 0x48, &em->pos, 0, 0, em);
        SndCall(1, 0x11, &em->getPartsPtr(4)->worldPos, 0, 0, em);
        em->xFE++;
    case 1:
        if (MotionMove(em, 0)) {
            EndPlDamage();
        }
        break;
    }
}

void objPillarEatSet(cObjPillar* obj)
{
    PillarWork* w = &obj->pillar;
    Vec poly[4];
    f32 r = 400.0f;
    f32 h = 5000.0f;

    if (w->sat == 0) {
        poly[0].x = -r;
        poly[0].y = 0.0f;
        poly[0].z = -r;
        poly[1].x = r;
        poly[1].y = 0.0f;
        poly[1].z = -r;
        poly[2].x = r;
        poly[2].y = 0.0f;
        poly[2].z = r;
        poly[3].x = -r;
        poly[3].y = 0.0f;
        poly[3].z = r;
        w->sat = EatMgr.create(&obj->pos, &obj->rot, poly, 0, 0, h);
    } else {
        w->sat->flags |= 4;
        w->sat->setCoord(&obj->pos, &obj->rot);
    }
}
