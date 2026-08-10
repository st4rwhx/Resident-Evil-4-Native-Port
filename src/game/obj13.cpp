#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "obj.h"
#include "em.h"
#include "emwindow.h"
#include "global.h"
#include "math_sub.h"
#include "camera.h"
#include "cam_ctrl.h"
#include "act_btn.h"
#include "snd.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "at_mod.h"
#include "etc_model.h"

// Ladder (obj 0x13): the player and the partner climb it (plobjLadderClimb / subobjLadderClimb),
// the player kicks it down (plobjLadderDown) and puts it up again (plobjLadderReset); the ladder
// falls with a damage area (R1_Fall) and breaks the windows it lands on (breakWindow).
class cObjLadder : public cObj {
public:
    virtual void move();
    virtual ~cObjLadder() {}

    int getStatus();
    int getType();
    int ckClimb();
    void setClimb();
    int getLadderNum();
    void setLadderInfo(int num, u8 type);
    void setStand();
    void setDowned();
    void setDown(void* mot, int a);
    void setDown2();
    int ckReset();
    void setReset(int type);
    void setTransOld();
    void getTransOld();
    void setOff();
    void setOn();
    void setResetReserve();
    void setMotion(void** tbl);
    void breakWindow();
    void setCamera(int no);
};

// cMotBase.h drags in motion.h's one-argument MotionMove; only the base setter is needed here.
class cMotModel;
class cMotBase {
public:
    void set(cMotModel* m, Vec* pos, Vec* rot, u8 cnt);
};

extern "C" {
int MotionMove(cModel* m, int a);
cObj* SetLadder(void* bin, void* tpl, Vec* pos, Vec* rot, int no);
void objLadder_R1_Set(cObjLadder* obj);
void objLadder_R1_Fall(cObjLadder* obj);
void objLadder_R1_Down(cObjLadder* obj);
void objLadder_R1_Reset(cObjLadder* obj);
void objLadderSatSet(cObjLadder* obj);
void objLadderClimbActEvtCk(cObjLadder* obj);
void objLadderActClimb(cObjLadder* obj);
void plobjLadderClimb(cPlayer* pl);
int SubLadderClimbCk(cEm* em);
int SubLadderClimbCk2(cEm* em);
void subobjLadderClimb(cEm* em);
void objLadderClimbCamMove(cEm* em);
void objLadderDownActEvtCk(cObjLadder* obj);
void objLadderActDown(cObjLadder* obj);
void plobjLadderDown(cPlayer* pl);
void objLadderDownCamMove(cEm* em);
void objLadderResetActEvtCk(cObjLadder* obj);
void objLadderActReset(cObjLadder* obj);
void plobjLadderReset(cPlayer* pl);
void objLadderResetCamMove(cEm* em);
int LadderNearCk(Vec* pos);
void LadderEventTrans(int mode);
}
void MotionSetCore(cModel* m, void* work, void* mot, int a, int b, int c, int d);

void (*ObjLadder_R1_move_tbl[4])(cObjLadder*) = {
    objLadder_R1_Set, objLadder_R1_Fall, objLadder_R1_Down, objLadder_R1_Reset,
};

cObj* SetLadder(void* bin, void* tpl, Vec* pos, Vec* rot, int no)
{
    cObj* obj;
    LadderWork* w;
    cModel* parts;
    u16* flg;

    flg = GetEtcFlgPtr(no, pG->room_id);
    if (flg && (*flg & 1)) {
        return 0;
    }
    obj = ObjMgr.create(0x13);
    if (obj == 0) {
        return 0;
    }
    w = &obj->ladder;
    w->etcNo = no;
    if (obj->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetLadder() failed.");
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 5000.0f, 5000.0f, 5000.0f };

    obj->lightInfo.init2(0, 1, &p0, &p1, 0x10);
    AtariInit(&obj->sub2B4.atari, 0.0f, 1000.0f, -700.0f, 330.0f, 600.0f, 600.0f, 1000.0f, 0, 2, 0);
    obj->sub2B4.atari.flags &= ~0x100;
    obj->sub2B4.atari.flags |= 0x10;
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
    }
    parts = obj->getPartsPtr(0);
    parts->rot.x = -1.9198622f;
    parts->rot.y = 0.0f;
    parts->rot.z = 0.0f;
    w->ladderNum = 6;
    w->status = 0;
    w->basePos = obj->pos;
    w->baseRotY = obj->rot.y;
    w->climbTimer = 0;
    w->resetReserve = 0;
    w->downTimer = 0;
    obj->type = 0;
    w->x08 = 0;
    w->camera = -1;
    w->pair = 0;
    return obj;
}

void cObjLadder::move()
{
    LadderWork* w = &ladder;

    if (w->climbTimer) {
        w->climbTimer--;
    }
    if (w->resetReserve) {
        w->resetReserve--;
    }
    if (ladder.flags & 2) {
        sub2B4.atari.clrFlag200();
        if (w->pair) {
            w->pair->sub2B4.atari.clrFlag200();
        }
    } else {
        ObjLadder_R1_move_tbl[xFD](this);
        EmAtCheck((cEm*) this);
        sub2B4.atari.move();
    }
}

void objLadder_R1_Set(cObjLadder* obj)
{
    LadderWork* w = &obj->ladder;
    cModel* parts;

    obj->pos = w->basePos;
    obj->rot.x = 0.0f;
    obj->rot.y = w->baseRotY;
    obj->rot.z = 0.0f;
    parts = obj->getPartsPtr(0);
    parts->rot.x = -1.9198622f;
    parts->rot.y = 0.0f;
    parts->rot.z = 0.0f;
    obj->matUpdate();
    objLadderSatSet(obj);
    objLadderClimbActEvtCk(obj);
    objLadderDownActEvtCk(obj);
    obj->ladder.flags &= ~4;
    obj->sub2B4.atari.setFlag200();
    if (w->pair) {
        w->pair->sub2B4.atari.setFlag200();
    }
}

void objLadder_R1_Fall(cObjLadder* obj)
{
    LadderWork* w = &obj->ladder;
    Vec v;

    w->status = 4;
    switch (obj->xFE) {
    case 0:
        obj->xFE++;
    case 1:
        if (w->downTimer) {
            if (--w->downTimer == 0) {
                w->status = 3;
            }
        }
        if (obj->pMotion) {
            if (obj->motEvent & 1) {
                SndCall(6, 0x3F, &obj->pos, 0, 0, 0);
            }
            if (MotionMove(obj, 0)) {
                w->status = 1;
                obj->xFC = 1;
                obj->xFD = 2;
                obj->xFE = 0;
                obj->xFF = 0;
            }
        }
        break;
    }
    obj->partsWorldCalc();
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    PSMTXMultVec(obj->mat, &v, &v);
    v.y = obj->pos.y;
    DmgMgr.set(3, 2, &v, 1500.0f, 1000.0f);
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 2000.0f;
    PSMTXMultVec(obj->mat, &v, &v);
    v.y = obj->pos.y;
    DmgMgr.set(3, 2, &v, 1500.0f, 1000.0f);
    objLadderSatSet(obj);
    obj->sub2B4.atari.clrFlag200();
    if (w->pair) {
        w->pair->sub2B4.atari.clrFlag200();
    }
}

void objLadder_R1_Down(cObjLadder* obj)
{
    LadderWork* w = &obj->ladder;

    w->status = 1;
    obj->matUpdate();
    objLadderResetActEvtCk(obj);
    objLadderSatSet(obj);
    obj->sub2B4.atari.clrFlag200();
    if (w->pair) {
        w->pair->sub2B4.atari.clrFlag200();
    }
}

void objLadder_R1_Reset(cObjLadder* obj)
{
    LadderWork* w = &obj->ladder;

    w->status = 4;
    switch (obj->xFE) {
    case 0:
        obj->xFE++;
    case 1:
        if (obj->pMotion) {
            if (obj->motEvent & 1) {
                SndCall(6, 0x41, &obj->pos, 0, 0, 0);
                obj->breakWindow();
            }
            if (MotionMove(obj, 0)) {
                w->status = 0;
                obj->xFC = 1;
                obj->xFD = 0;
                obj->xFE = 0;
                obj->xFF = 0;
            }
        }
        break;
    }
    obj->partsWorldCalc();
    objLadderSatSet(obj);
    obj->sub2B4.atari.clrFlag200();
    if (w->pair) {
        w->pair->sub2B4.atari.clrFlag200();
    }
}

int cObjLadder::getStatus()
{
    return ladder.status;
}

int cObjLadder::getType()
{
    return type;
}

int cObjLadder::ckClimb()
{
    LadderWork* w = &ladder;

    if (w->status != 0) {
        return 0;
    }
    if (w->climbTimer != 0) {
        return 0;
    }
    u32 off = ladder.flags & 2;
    return off == 0;
}

void cObjLadder::setClimb()
{
    ladder.climbTimer = 90;
}

int cObjLadder::getLadderNum()
{
    return ladder.ladderNum;
}

void cObjLadder::setLadderInfo(int num, u8 t)
{
    ladder.ladderNum = num;
    type = t;
}

void cObjLadder::setStand()
{
    ladder.status = 0;
    xFC = 1;
    xFD = 0;
    xFE = 0;
    xFF = 0;
}

void cObjLadder::setDowned()
{
    LadderWork* w = &ladder;
    cModel* parts;

    w->status = 1;
    sub2B4.atari.clrFlag200();
    if (w->pair) {
        w->pair->sub2B4.atari.clrFlag200();
    }
    parts = getPartsPtr(0);
    parts->rot.x = 0.0f;
    parts->rot.y = 0.0f;
    parts->rot.z = 0.0f;
    xFC = 1;
    xFD = 2;
    xFE = 0;
    xFF = 0;
}

void cObjLadder::setDown(void* mot, int a)
{
    LadderWork* w = &ladder;

    w->status = 2;
    w->downTimer = 17;
    sub2B4.atari.clrFlag200();
    if (w->pair) {
        w->pair->sub2B4.atari.clrFlag200();
    }
    MotionSetCore(this, &pMotion, mot, a, 0, 1, 0);
    xFC = 1;
    xFD = 1;
    xFE = 0;
    xFF = 0;
}

void cObjLadder::setDown2()
{
    LadderWork* w = &ladder;
    void* mot = w->mot[10];
    void* a = w->mot[15];
    cModel* parts;
    int frame;

    w->downTimer = 0;
    w->status = 3;
    parts = getPartsPtr(0);
    frame = 0;
    if (parts->rot.x > -1.5707964f) {
        frame = 4;
    }
    if (parts->rot.x > -1.3962634f) {
        frame = 6;
    }
    if (parts->rot.x > -1.2217305f) {
        frame = 8;
    }
    if (parts->rot.x > -1.0471976f) {
        frame = 0xA;
    }
    if (parts->rot.x > -0.87266463f) {
        frame = 0xB;
    }
    if (parts->rot.x > -0.6981317f) {
        frame = 0xC;
    }
    if (parts->rot.x > -0.5235988f) {
        frame = 0xD;
    }
    if (parts->rot.x > -0.34906584f) {
        frame = 0xE;
    }
    sub2B4.atari.clrFlag200();
    if (w->pair) {
        w->pair->sub2B4.atari.clrFlag200();
    }
    MotionSetCore(this, &pMotion, mot, (int) a, 0, 1, frame);
    xFC = 1;
    xFD = 1;
    xFE = 0;
    xFF = 0;
}

int cObjLadder::ckReset()
{
    LadderWork* w = &ladder;

    if (w->status != 1) {
        return 0;
    }
    return w->resetReserve == 0;
}

void cObjLadder::setReset(int t)
{
    LadderWork* w = &ladder;

    w->status = 4;
    switch (t) {
    case 0:
    default:
        MotionSetCore(this, &pMotion, w->mot[6], (int) w->mot[11], 0, 1, 0);
        break;
    case 1:
        MotionSetCore(this, &pMotion, w->mot[8], (int) w->mot[13], 0, 1, 0);
        break;
    }
    xFC = 1;
    xFD = 3;
    xFE = 0;
    xFF = 0;
}

void cObjLadder::setTransOld()
{
    if (ladder.flags & 2) {
        ladder.flags |= 8;
    } else {
        ladder.flags &= ~8;
    }
}

void cObjLadder::getTransOld()
{
    if (ladder.flags & 8) {
        setOff();
    } else {
        setOn();
    }
}

void cObjLadder::setOff()
{
    ladder.flags |= 2;
    be_flag &= ~2;
}

void cObjLadder::setOn()
{
    ladder.flags &= ~2;
    be_flag |= 2;
}

void cObjLadder::setResetReserve()
{
    ladder.resetReserve = 60;
}

void objLadderSatSet(cObjLadder* obj)
{
    LadderWork* w = &obj->ladder;

    obj->sub2B4.atari.clrFlag200();
    if (w->pair) {
        w->pair->sub2B4.atari.clrFlag200();
    }
    if (w->status != 0) {
        return;
    }
    obj->sub2B4.atari.setFlag200();
    if (w->pair) {
        w->pair->sub2B4.atari.setFlag200();
    }
}

void objLadderClimbActEvtCk(cObjLadder* obj)
{
    LadderWork* w = &obj->ladder;
    Mtx m;
    Vec v;

    if (!(w->flags & 1)) {
        return;
    }
    if (w->status) {
        return;
    }
    if (w->climbTimer) {
        return;
    }
    if (pPL->xFC != 0) {
        return;
    }
    if (fabsf(Muku2(pPL->rot.y, obj->rot.y, PI)) < PI / 2.0f) {
        return;
    }
    PSMTXRotRad(m, 'y', obj->rot.y);
    TransMatrix(m, &obj->pos);
    PSMTXInverse(m, m);
    PSMTXMultVec(m, &pPL->pos, &v);
    if (v.z > 1000.0f) {
        return;
    }
    if (v.z < -500.0f) {
        return;
    }
    if (v.x > 800.0f) {
        return;
    }
    if (v.x < -800.0f) {
        return;
    }
    if (fabsf(v.y) > 500.0f) {
        return;
    }
    ActBtn.set(8, 5, (int) objLadderActClimb, (int) obj, 0, 1, 0, 0);
}

void objLadderActClimb(cObjLadder* obj)
{
    obj->setClimb();
    SetPlDamage((int) obj, plobjLadderClimb);
}

void plobjLadderClimb(cPlayer* pl)
{
    cEm* em = (cEm*) pl;
    cObjLadder* obj = (cObjLadder*) em->dmgType;
    LadderWork* w = &obj->ladder;
    Mtx m;
    Vec v;
    f32 fl;

    em->x378 = ((cEm*) pPL->dmgType)->x378;
    pGS->flags_5010 |= 0x00040000;
    em->dmg.set(0, 0xF);
    switch (em->xFE) {
    case 0:
        PSMTXRotRad(m, 'y', obj->rot.y);
        TransMatrix(m, &obj->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 300.0f;
        PSMTXMultVec(m, &v, &em->pos);
        em->rot.y = obj->rot.y + PI;
        em->rot.y = LIMIT_ANGLE(em->rot.y);
        MotionSetCore(em, &em->pMotion, w->mot[0], 0, 5, 1, 0);
        em->atari.throughOn();
        em->x3E0 = obj->getLadderNum();
        em->be_flag &= ~0x10;
        if (w->camera != -1) {
            CamCtrl.CutCall((s8) w->camera);
        }
        em->xFE++;
    case 1:
        if (em->frame > 9.7f && em->frame < 10.3f) {
            SndCall(6, 0x43, &em->pos, 0, 0, 0);
        }
        if (em->frame > 17.7f && em->frame < 18.3f) {
            SndCall(6, 0x42, &em->pos, 0, 0, 0);
        }
        if (MotionMove(em, 0)) {
            em->x3E0 -= 4;
            if ((int) em->x3E0 > 0) {
                em->xFE++;
            } else {
                em->xFE = 4;
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->pMotion, w->mot[1], 0, 5, 5, 0);
        em->xFE++;
    case 3:
        if (em->frame > 11.7f && em->frame < 12.3f) {
            SndCall(6, 0x43, &em->pos, 0, 0, 0);
        }
        if (em->frame > 21.7f && em->frame < 22.3f) {
            SndCall(6, 0x42, &em->pos, 0, 0, 0);
        }
        if (MotionMove(em, 0)) {
            em->x3E0 -= 2;
            if ((int) em->x3E0 > 0) {
                break;
            }
            em->xFE = 4;
        }
        break;
    case 4:
        if (obj->getType() == 1) {
            MotionSetCore(em, &em->pMotion, w->mot[3], 0, 5, 1, 0);
        } else {
            MotionSetCore(em, &em->pMotion, w->mot[2], 0, 5, 1, 0);
        }
        if (w->camera != -1) {
            CamCtrl.Comeback(0);
        }
        em->x3E0 = 0;
        em->xFE++;
    case 5:
        if (obj->getType() == 1) {
            if (em->frame > 10.7f && em->frame < 11.3f) {
                SndCall(6, 0x43, &em->pos, 0, 0, 0);
            }
            if (em->frame > 32.7f && em->frame < 33.3f) {
                SndCall(5, 0xD, &em->getPartsPtr(0x14)->worldPos, em->id, 0, 0);
            }
            if (em->frame > 35.7f && em->frame < 36.3f) {
                SndCall(5, 0xE, &em->getPartsPtr(0x18)->worldPos, em->id, 0, 0);
            }
        } else {
            if (em->frame > 10.7f && em->frame < 11.3f) {
                SndCall(6, 0x43, &em->pos, 0, 0, 0);
            }
            if (em->frame > 24.7f && em->frame < 25.3f) {
                SndCall(5, 0xD, &em->getPartsPtr(0x14)->worldPos, em->id, 0, 0);
            }
            if (em->frame > 34.7f && em->frame < 35.3f) {
                SndCall(5, 0xE, &em->getPartsPtr(0x18)->worldPos, em->id, 0, 0);
            }
        }
        em->x3E0++;
        if (obj->getType() != 1 && (int) em->x3E0 > 0x17) {
            fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
            if (em->pos.y < fl) {
                em->pos.y = em->pos.y * 0.9f + fl * 0.1f;
            }
        }
        if (MotionMove(em, 0)) {
            fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
            if (em->pos.y < fl) {
                em->pos.y = fl;
            }
            em->be_flag |= 0x10;
            EndPlDamage();
            em->atari.flags |= 0x100;
            em->atari.flags &= ~0x10;
        }
        break;
    }
    if (w->camera == -1) {
        objLadderClimbCamMove(em);
    }
    em->x378 = em->x37C;
}

int SubLadderClimbCk(cEm* em)
{
    const f32 distLim = 1000000.0f;
    const f32 heightLim = 40000.0f;
    const f32 angLim = PI / 2.0f;
    u32 i;

    if (pSUB == 0) {
        return 0;
    }
    if (pSUB->subX5C8 < 1000.0f) {
        return 0;
    }
    for (i = 0; i < ObjMgr.nArray; i++) {
        cObjLadder* obj = (cObjLadder*) ((u8*) ObjMgr.pArray + ObjMgr.size * i);

        if ((obj->be_flag & 0x201) == 1 && obj->id == 0x13 && obj->ckClimb()) {
            if ((em->pos.x - obj->pos.x) * (em->pos.x - obj->pos.x) + (em->pos.y - obj->pos.y) * (em->pos.y - obj->pos.y) +
                    (em->pos.z - obj->pos.z) * (em->pos.z - obj->pos.z) >
                distLim) {
                continue;
            }
            if (fabsf(Muku(&em->oldPos, &obj->pos, em->rot.y, PI)) > angLim) {
                continue;
            }
            if (fabsf(em->pos.y - obj->pos.y) > heightLim) {
                continue;
            }
            if (em->plDist2 > 100000000.0f || em->pos.y + 1000.0f < pPL->pos.y) {
                obj->ladder.flags |= 4;
                SetSubDamage((int) obj, (void*) subobjLadderClimb);
                obj->setClimb();
                return 1;
            }
        }
    }
    return 0;
}

int SubLadderClimbCk2(cEm* em)
{
    u32 i;

    for (i = 0; i < ObjMgr.nArray; i++) {
        cObjLadder* obj = (cObjLadder*) ((u8*) ObjMgr.pArray + ObjMgr.size * i);

        if ((obj->be_flag & 0x201) == 1 && obj->id == 0x13 && obj->getStatus() != 0) {
            f32 dy = em->pos.y - obj->pos.y;

            if ((em->pos.x - obj->pos.x) * (em->pos.x - obj->pos.x) + dy * dy +
                    (em->pos.z - obj->pos.z) * (em->pos.z - obj->pos.z) >
                1000000.0f) {
                continue;
            }
            if (fabsf(dy) > 40000.0f) {
                continue;
            }
            return 1;
        }
    }
    return 0;
}

void subobjLadderClimb(cEm* pl)
{
    cEm* em = pSUB;
    cObjLadder* obj = (cObjLadder*) em->dmgType;
    LadderWork* w = &obj->ladder;
    Mtx m;
    Vec p;
    Vec rot;
    f32 fl;

    obj->ladder.flags |= 4;
    em->dmg.set(0, 2);
    switch (em->xFE) {
    case 0:
        PSMTXRotRad(m, 'y', obj->rot.y);
        TransMatrix(m, &obj->pos);
        p.x = 0.0f;
        p.y = 0.0f;
        p.z = 300.0f;
        PSMTXMultVec(m, &p, &p);
        rot.x = rot.z = 0.0f;
        rot.y = obj->rot.y + PI;
        rot.y = LIMIT_ANGLE(rot.y);
        ((cMotBase*) &em->subFlags58C)->set((cMotModel*) em, &p, &rot, 10);
        MotionSetCore(em, &em->pMotion, w->mot[16], 0, 5, 1, 0);
        em->atari.flags &= ~0x100;
        em->atari.flags |= 0x10;
        em->subFlags |= 0x20;
        em->subHideMode = obj->getLadderNum();
        em->subX534 = 8;
        em->xFE++;
    case 1:
        if (em->subX534) {
            em->subX534--;
        } else {
            pG->flags_5010 |= 8;
        }
        if (em->frame > 8.7f && em->frame < 9.3f) {
            SndCall(6, 0x46, &em->pos, 0, 0, 0);
        }
        if (em->frame > 17.7f && em->frame < 18.3f) {
            SndCall(6, 0x45, &em->pos, 0, 0, 0);
        }
        if (MotionMove(em, 0)) {
            em->subHideMode -= 4;
            if (em->subHideMode > 0) {
                em->xFE++;
            } else {
                em->xFE = 4;
            }
        }
        break;
    case 2:
        MotionSetCore(em, &em->pMotion, w->mot[17], 0, 5, 5, 0);
        em->xFE++;
    case 3:
        pG->flags_5010 |= 8;
        if (em->frame > 11.7f && em->frame < 12.3f) {
            SndCall(6, 0x46, &em->pos, 0, 0, 0);
        }
        if (em->frame > 20.7f && em->frame < 21.3f) {
            SndCall(6, 0x45, &em->pos, 0, 0, 0);
        }
        if (MotionMove(em, 0)) {
            em->subHideMode -= 2;
            if (em->subHideMode > 0) {
                break;
            }
            em->xFE = 4;
        }
        break;
    case 4:
        if (obj->getType() == 1) {
            MotionSetCore(em, &em->pMotion, w->mot[19], 0, 5, 1, 0);
            em->subX534 = 0x28;
        } else {
            MotionSetCore(em, &em->pMotion, w->mot[18], 0, 5, 1, 0);
            em->subX534 = 0x23;
        }
        em->subHideMode = 0;
        em->xFE++;
    case 5:
        if (em->subX534) {
            em->subX534--;
            pGS->flags_5010 |= 8;
        }
        if (obj->getType() == 1) {
            if (em->frame > 11.7f && em->frame < 12.3f) {
                SndCall(6, 0x43, &em->pos, 0, 0, 0);
            }
            if (em->frame > 22.7f && em->frame < 23.3f) {
                SndCall(5, 0xD, &em->getPartsPtr(0x14)->worldPos, em->id, 0, 0);
            }
            if (em->frame > 42.7f && em->frame < 43.3f) {
                SndCall(5, 0xE, &em->getPartsPtr(0x18)->worldPos, em->id, 0, 0);
                BitOff16(em->subFlags, 0x20);
            }
        } else {
            if (em->frame > 11.7f && em->frame < 12.3f) {
                SndCall(6, 0x43, &em->pos, 0, 0, 0);
            }
            if (em->frame > 22.7f && em->frame < 23.3f) {
                SndCall(5, 0xD, &em->getPartsPtr(0x14)->worldPos, em->id, 0, 0);
            }
            if (em->frame > 35.7f && em->frame < 36.3f) {
                SndCall(5, 0xE, &em->getPartsPtr(0x18)->worldPos, em->id, 0, 0);
                BitOff16(em->subFlags, 0x20);
            }
        }
        em->subHideMode++;
        if (obj->getType() != 1 && em->subHideMode > 0x17) {
            fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
            if (em->pos.y < fl) {
                em->pos.y = em->pos.y * 0.9f + fl * 0.1f;
            }
        }
        if (MotionMove(em, 0)) {
            fl = SatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0);
            if (em->pos.y < fl) {
                em->pos.y = fl;
            }
            EndSubDamage();
            BitOff16(em->subFlags, 0x20);
            em->atari.flags |= 0x100;
            em->atari.flags &= ~0x10;
        }
        break;
    }
}

static inline f32 LadderCamDist(Vec* a, Vec* b)
{
    return SQRTF((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) + (a->z - b->z) * (a->z - b->z));
}

void objLadderClimbCamMove(cEm* em)
{
    static Camera objLadderClimbCam = { 0 };
    GlobalWork* g = pG;
    Vec camPos;
    Vec camAt;
    cModel* parts;

    parts = em->getPartsPtr(0);
    camPos.x = 0.0f;
    camPos.y = 300.0f;
    camPos.z = -1800.0f;
    camAt.x = 0.0f;
    camAt.y = 300.0f;
    camAt.z = 0.0f;
    PSMTXMultVec(parts->mat, &camPos, &camPos);
    PSMTXMultVec(parts->mat, &camAt, &camAt);
    PosToPos(&g->Cam.param.at, &camAt, &objLadderClimbCam.param.at, 1.0f);
    PosToPos(&g->Cam.param.pos, &camPos, &objLadderClimbCam.param.pos, 1.0f);
    objLadderClimbCam.up.x = 0.0f;
    objLadderClimbCam.up.y = 1.0f;
    objLadderClimbCam.up.z = 0.0f;
    objLadderClimbCam.dist = LadderCamDist(&objLadderClimbCam.param.pos, &objLadderClimbCam.param.at);
    objLadderClimbCam.param.fovy = 55.0f;
    CameraSetOrientationUp(&objLadderClimbCam);
    CamCtrl.x250 = (s32) &objLadderClimbCam;
}

void objLadderDownActEvtCk(cObjLadder* obj)
{
    LadderWork* w = &obj->ladder;
    Mtx m;
    Vec v;
    f32 n;

    if (!(w->flags & 1)) {
        return;
    }
    if (w->status) {
        return;
    }
    if (pPL->xFC != 0) {
        return;
    }
    if (fabsf(Muku2(pPL->rot.y, obj->rot.y, PI)) > PI / 2.0f) {
        return;
    }
    PSMTXRotRad(m, 'y', obj->rot.y);
    v.x = 0.0f;
    n = (f32) w->ladderNum;
    v.y = n * 533.3329f;
    v.z = n * -194.1173f;
    PSMTXMultVecSR(m, &v, &v);
    PSVECAdd(&obj->pos, &v, &v);
    if (obj->type == 1) {
        v.y -= 1000.0f;
    }
    TransMatrix(m, &v);
    PSMTXInverse(m, m);
    PSMTXMultVec(m, &pPL->pos, &v);
    if (v.z < -1000.0f) {
        return;
    }
    if (v.z > 500.0f) {
        return;
    }
    if (v.x > 800.0f) {
        return;
    }
    if (v.x < -800.0f) {
        return;
    }
    if (fabsf(v.y) > 500.0f) {
        return;
    }
    if (w->flags & 4) {
        ActBtn.set(0xA, 5, (int) objLadderActDown, (int) obj, 0x20, 1, 0, 0);
    } else {
        ActBtn.set(0xA, 5, (int) objLadderActDown, (int) obj, 0, 1, 0, 0);
    }
}

void objLadderActDown(cObjLadder* obj)
{
    LadderWork* w = &obj->ladder;

    if (!(w->flags & 4)) {
        SetPlDamage((int) obj, plobjLadderDown);
        w->climbTimer = 90;
    }
}

void plobjLadderDown(cPlayer* pl)
{
    cEm* em = (cEm*) pl;
    cObjLadder* obj = (cObjLadder*) em->dmgType;
    LadderWork* w = &obj->ladder;
    Mtx m;
    Vec v;

    em->x378 = ((cEm*) pPL->dmgType)->x378;
    em->dmg.set(0, 0xF);
    switch (em->xFE) {
    case 0:
        PSMTXRotRad(m, 'y', obj->rot.y);
        v.x = 0.0f;
        v.y = (f32) w->ladderNum * 533.3329f;
        v.z = (f32) w->ladderNum * -194.1173f;
        PSMTXMultVecSR(m, &v, &v);
        PSVECAdd(&obj->pos, &v, &v);
        TransMatrix(m, &v);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = -700.0f;
        PSMTXMultVec(m, &v, &v);
        em->pos.x = v.x;
        em->pos.z = v.z;
        em->rot.y = obj->rot.y;
        if (obj->getType() == 1) {
            MotionSetCore(em, &em->pMotion, w->mot[5], 0, 5, 1, 0);
            obj->setDown(w->mot[7], (int) w->mot[12]);
        } else {
            MotionSetCore(em, &em->pMotion, w->mot[9], 0, 5, 1, 0);
            obj->setDown(w->mot[10], (int) w->mot[14]);
        }
        em->atari.throughOn();
        em->xFE++;
    case 1:
        if (obj->getType() == 1) {
            if (em->frame > 16.7f && em->frame < 17.3f) {
                SndCall(6, 0x40, &em->pos, 0, 0, 0);
            }
        } else {
            if (em->frame > 12.7f && em->frame < 13.3f) {
                SndCall(6, 0x44, &em->pos, 0, 0, 0);
            }
        }
        if (MotionMove(em, 0)) {
            EndPlDamage();
            em->dmg.set(0, 0x1E);
            em->atari.throughOff();
        }
        break;
    }
    objLadderDownCamMove(em);
    em->x378 = em->x37C;
}

void objLadderDownCamMove(cEm* em)
{
    static Camera objLadderDownCam = { 0 };
    GlobalWork* g = pG;
    Vec camPos;
    Vec camAt;

    camPos.x = 0.0f;
    camPos.y = 2500.0f;
    camPos.z = -500.0f;
    camAt.x = 0.0f;
    camAt.y = 1000.0f;
    camAt.z = 500.0f;
    PSMTXMultVec(em->mat, &camPos, &camPos);
    PSMTXMultVec(em->mat, &camAt, &camAt);
    PosToPos(&g->Cam.param.at, &camAt, &objLadderDownCam.param.at, 1.0f);
    PosToPos(&g->Cam.param.pos, &camPos, &objLadderDownCam.param.pos, 1.0f);
    objLadderDownCam.up.x = 0.0f;
    objLadderDownCam.up.y = 1.0f;
    objLadderDownCam.up.z = 0.0f;
    objLadderDownCam.dist = LadderCamDist(&objLadderDownCam.param.pos, &objLadderDownCam.param.at);
    objLadderDownCam.param.fovy = 55.0f;
    CameraSetOrientationUp(&objLadderDownCam);
    CamCtrl.x250 = (s32) &objLadderDownCam;
}

void objLadderResetActEvtCk(cObjLadder* obj)
{
    LadderWork* w = &obj->ladder;

    if (!(w->flags & 1)) {
        return;
    }
    if (w->status != 1) {
        return;
    }
    if (pPL->xFC != 0) {
        return;
    }
    if ((obj->pos.x - pPL->pos.x) * (obj->pos.x - pPL->pos.x) + (obj->pos.z - pPL->pos.z) * (obj->pos.z - pPL->pos.z) > 2250000.0f) {
        return;
    }
    if (fabsf(obj->pos.y - pPL->pos.y) > 500.0f) {
        return;
    }
    ActBtn.set(0xB, 5, (int) objLadderActReset, (int) obj, 0, 1, 0, 0);
}

void objLadderActReset(cObjLadder* obj)
{
    SetPlDamage((int) obj, plobjLadderReset);
    obj->setResetReserve();
}

void plobjLadderReset(cPlayer* pl)
{
    cEm* em = (cEm*) pl;
    cObjLadder* obj = (cObjLadder*) em->dmgType;
    LadderWork* w = &obj->ladder;
    Mtx m;
    Vec v;
    int motA;

    em->x378 = ((cEm*) pPL->dmgType)->x378;
    em->dmg.set(0, 0xF);
    switch (em->xFE) {
    case 0:
        PSMTXRotRad(m, 'y', obj->rot.y);
        TransMatrix(m, &obj->pos);
        if (Muku2(obj->rot.y, em->rot.y, PI) < 0.0f) {
            v.x = 890.014f;
            v.y = 0.0f;
            v.z = 1450.51f;
            motA = 1;
            em->rot.y = obj->rot.y - PI / 2.0f;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        } else {
            v.x = -890.014f;
            v.y = 0.0f;
            v.z = 1450.51f;
            motA = 0x41;
            em->rot.y = obj->rot.y + PI / 2.0f;
            em->rot.y = LIMIT_ANGLE(em->rot.y);
        }
        PSMTXMultVec(m, &v, &em->pos);
        MotionSetCore(em, &em->pMotion, w->mot[4], 0, 5, motA, 0);
        obj->setReset(0);
        em->atari.flags |= 0x10;
        em->xFE++;
    case 1:
        if (MotionMove(em, 0)) {
            EndPlDamage();
            em->dmg.set(0, 0x1E);
            em->atari.flags &= ~0x10;
        }
        break;
    }
    objLadderResetCamMove(em);
    em->x378 = em->x37C;
}

void objLadderResetCamMove(cEm* em)
{
    static Camera objLadderResetCam = { 0 };
    GlobalWork* g = pG;
    Vec camPos;
    Vec camAt;

    camPos.x = 0.0f;
    camPos.y = 1300.0f;
    camPos.z = -1000.0f;
    camAt.x = 0.0f;
    camAt.y = 1800.0f;
    camAt.z = 0.0f;
    PSMTXMultVec(em->mat, &camPos, &camPos);
    PSMTXMultVec(em->mat, &camAt, &camAt);
    PosToPos(&g->Cam.param.at, &camAt, &objLadderResetCam.param.at, 1.0f);
    PosToPos(&g->Cam.param.pos, &camPos, &objLadderResetCam.param.pos, 1.0f);
    objLadderResetCam.up.x = 0.0f;
    objLadderResetCam.up.y = 1.0f;
    objLadderResetCam.up.z = 0.0f;
    objLadderResetCam.dist = LadderCamDist(&objLadderResetCam.param.pos, &objLadderResetCam.param.at);
    objLadderResetCam.param.fovy = 55.0f;
    CameraSetOrientationUp(&objLadderResetCam);
    CamCtrl.x250 = (s32) &objLadderResetCam;
}

void cObjLadder::setMotion(void** tbl)
{
    LadderWork* w = &ladder;

    w->mot[0] = *tbl++;
    w->mot[1] = *tbl++;
    w->mot[2] = *tbl++;
    w->mot[3] = *tbl++;
    w->mot[4] = *tbl++;
    w->mot[5] = *tbl++;
    w->mot[6] = *tbl++;
    w->mot[7] = *tbl++;
    w->mot[8] = *tbl++;
    w->mot[9] = *tbl++;
    w->mot[10] = *tbl++;
    w->mot[11] = *tbl++;
    w->mot[12] = *tbl++;
    w->mot[13] = *tbl++;
    w->mot[14] = *tbl++;
    w->mot[15] = *tbl++;
    w->mot[16] = *tbl++;
    w->mot[17] = *tbl++;
    w->mot[18] = *tbl++;
    w->mot[19] = *tbl++;
    ladder.flags |= 1;
}

// 0 when a standing ladder's top is within 2000 of `pos`.
int LadderNearCk(Vec* pos)
{
    Mtx m;
    Vec v;
    u32 i;

    for (i = 0; i < ObjMgr.nArray; i++) {
        cObjLadder* obj = (cObjLadder*) ((u8*) ObjMgr.pArray + ObjMgr.size * i);
        LadderWork* w = &obj->ladder;

        if ((obj->be_flag & 0x201) == 1 && obj->id == 0x13 && w->status == 0 && !(obj->ladder.flags & 2)) {
            PSMTXRotRad(m, 'y', obj->rot.y);
            v.x = 0.0f;
            v.y = (f32) w->ladderNum * 533.3329f;
            v.z = (f32) w->ladderNum * -194.1173f;
            PSMTXMultVecSR(m, &v, &v);
            PSVECAdd(&obj->pos, &v, &v);
            if (obj->type == 1) {
                v.y -= 1000.0f;
            }
            if ((pos->x - v.x) * (pos->x - v.x) + (pos->y - v.y) * (pos->y - v.y) + (pos->z - v.z) * (pos->z - v.z) < 4000000.0f) {
                return 0;
            }
        }
    }
    return 1;
}

void LadderEventTrans(int mode)
{
    u32 i;

    for (i = 0; i < ObjMgr.nArray; i++) {
        cObjLadder* obj = (cObjLadder*) ((u8*) ObjMgr.pArray + ObjMgr.size * i);

        if ((obj->be_flag & 0x201) == 1 && obj->id == 0x13) {
            if (mode == 1) {
                obj->getTransOld();
            } else {
                obj->setTransOld();
                obj->setOff();
            }
        }
    }
}

// Breaks the windows (em 0x46) within 2000 of the ladder's top.
void cObjLadder::breakWindow()
{
    LadderWork* w = &ladder;
    Mtx m;
    Vec v;
    f32 n;
    u32 i;

    PSMTXRotRad(m, 'y', rot.y);
    TransMatrix(m, &pos);
    v.x = 0.0f;
    n = (f32) w->ladderNum;
    v.y = n * 533.3329f;
    v.z = n * -194.1173f;
    PSMTXMultVec(m, &v, &v);
    if (type == 1) {
        v.y -= 1000.0f;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEmWindow* em = (cEmWindow*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((em->be_flag & 0x201) == 1 && em->id == 0x46 && em->hp > 0 && (em->ChkStatus() & 1) == 0) {
            if ((em->pos.x - v.x) * (em->pos.x - v.x) + (em->pos.y - v.y) * (em->pos.y - v.y) + (em->pos.z - v.z) * (em->pos.z - v.z) <
                4000000.0f) {
                em->SetBreakAll(&v, 0, 0);
            }
        }
    }
}

void cObjLadder::setCamera(int no)
{
    ladder.camera = no;
}
