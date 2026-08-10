#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "dmg.h"
#include "map_obj.h"
#include "widget.h"
#include "obj.h"
#include "em.h"
#include "emwep.h"
#include "global.h"
#include "math_sub.h"
#include "cam_ctrl.h"
#include "act_btn.h"
#include "snd.h"
#include "esp.h"
#include "rnd.h"
#include "main.h"
#include "player.h"
#include "pl_sub.h"

// Mine trolley (obj 0x3B): three cars (parts 0 / 4 / 8) running along their motion with a scenario
// collision piece and an effect collision piece per car; the player and the enemies standing on a
// car are carried along, the break routine throws them off.
class cObjTrolley : public cObj {
public:
    virtual void move();
    virtual ~cObjTrolley() {}

    void setMotion(void** tbl);
    int ckTrolleyRide(Vec* pos, u8* partsNo, Vec* out);
    int ckTrolleyRideAdjust(Vec* pos, Vec* out);
    void setStart();
    void set2ndStart();
    int ckStop();
};

// Room module enemy (the em1x classes): a cEm with the module's own virtuals; the trolley only
// calls the one the cars' break throws them with (vtable slot 31).
class cEmRoom : public cEm {
public:
    virtual void v09();
    virtual void v10();
    virtual void v11();
    virtual void v12();
    virtual void v13();
    virtual void v14();
    virtual void v15();
    virtual void v16();
    virtual void v17();
    virtual void v18();
    virtual void v19();
    virtual void v20();
    virtual void v21();
    virtual void v22();
    virtual void v23();
    virtual void v24();
    virtual void v25();
    virtual void v26();
    virtual void v27();
    virtual void v28();
    virtual void v29();
    virtual void v30();
    virtual void setTrolleyLost();
};

extern "C" {
int MotionMove(cModel* m, int a);
void objTrolley_R0_Set(cObjTrolley* obj);
void objTrolley_R0_Move(cObjTrolley* obj);
void objTrolley_R0_Break(cObjTrolley* obj);
void objTrolleySatSet(cObjTrolley* obj);
void objTrolleyEscapeAction(cObjTrolley* obj);
void plobjTrolleyEscape(cPlayer* pl);
void plobjTrolleyDie(cPlayer* pl);
void objTrolleyPushMtx(cObjTrolley* obj);
int objTrolleyGetTrolleyNo(cObjTrolley* obj, Vec* pos);
int objTrolleyGetTrolleyNo2(cObjTrolley* obj, Vec* pos);
void objTrolleyGetAdjust(cObjTrolley* obj);
void objTrolleySetAdjust(cObjTrolley* obj, cEm* em);
void objTrolleyMoveAdjustPL(cObjTrolley* obj);
void objTrolleyMoveAdjustEM(cObjTrolley* obj);
void objTrolleyHitCk(cObjTrolley* obj);
void objTrolleyFallEM(cObjTrolley* obj);
void objTrolleyLostEM(cObjTrolley* obj);
}
void MotionSetCore(cModel* m, void* work, void* mot, int a, int b, int c, int d);
static void objTrolleySatClear(cObjTrolley* obj);

void (*ObjTrolley_R0_move_tbl[3])(cObjTrolley*) = {
    objTrolley_R0_Set, objTrolley_R0_Move, objTrolley_R0_Break,
};

Mtx Trolley_MatOld[3];      // car matrices of the previous frame (objTrolleyPushMtx)
Vec Trolley_vec[3];         // car movement of this frame (objTrolleyGetAdjust)
f32 Trolley_dir[3];         // car turn of this frame
u8 Trolley_parts_tbl[3] = { 0, 4, 8 };

cObj* SetTrolley(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    TrolleyWork* w;
    int i;
    void** p;

    obj = ObjMgr.create(0x3B);
    if (obj == 0) {
        return 0;
    }
    w = &obj->trolley;
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
    for (i = 0; i < 5; i++) {
        w->sat[i] = 0;
        w->sat2[i] = 0;
    }
    w->ride = 0;
    p = w->mot;
    for (i = 0; i < 9; i++) {
        *p++ = 0;
    }
    obj->xFC = 0;
    obj->xFD = 0;
    obj->xFE = 0;
    obj->xFF = 0;
    objTrolleySatSet((cObjTrolley*) obj);
    return obj;
}

void cObjTrolley::move()
{
    objTrolleySatClear(this);
    ObjTrolley_R0_move_tbl[xFC](this);
}

void objTrolley_R0_Set(cObjTrolley* obj)
{
    TrolleyWork* w = &obj->trolley;

    if (w->mot[0]) {
        MotionSetCore(obj, &obj->pMotion, w->mot[0], 0, 0, 0x8001, 0);
        MotionMove(obj, 0);
    } else {
        obj->matUpdate();
    }
    obj->partsWorldCalc();
    objTrolleyPushMtx(obj);
    objTrolleySatSet(obj);
    if (w->flags & 1) {
        pG->flags_500C |= 0x20;
        w->ride = 1;
        obj->xFC = 1;
        obj->xFD = 0;
        obj->xFE = 0;
        obj->xFF = 0;
    }
}

void objTrolley_R0_Move(cObjTrolley* obj)
{
    TrolleyWork* w = &obj->trolley;

    objTrolleyPushMtx(obj);
    switch (obj->xFE) {
    case 0:
        MotionSetCore(obj, &obj->pMotion, w->mot[0], 0, 0, 0x8001, 0);
        SndCall(6, 0, 0, 0, 0, 0);
        SndCall(6, 1, 0, 0, 0, 0);
        obj->xFE++;
    case 1:
        if (MotionMove(obj, 0)) {
            SndCall(6, 2, 0, 0, 0, 0);
            SndCall(6, 3, 0, 0, 0, 0);
            w->flags |= 4;
            obj->xFE++;
        }
        break;
    case 2:
        obj->xFE++;
    case 3:
        MotionMove(obj, 0);
        if (w->flags & 2) {
            obj->xFE++;
        }
        break;
    case 4:
        MotionSetCore(obj, &obj->pMotion, w->mot[1], 0, 0, 0x8001, 0);
        SndCall(6, 0, 0, 0, 0, 0);
        SndCall(6, 1, 0, 0, 0, 0);
        w->timer = 20;
        obj->xFF = Rnd() & 1;
        obj->xFE++;
    case 5:
        if (MotionMove(obj, 0) && w->ride) {
            w->ride = 0;
            SetPlDamage((int) obj, plobjTrolleyDie);
            obj->xFC = 2;
            obj->xFD = 0;
            obj->xFE = 0;
            obj->xFF = 0;
        } else {
            if (obj->motFrame > 2250.0f) {
                pG->flags_5014 |= 0x08000000;
            }
            if (obj->motFrame > 2300.0f) {
                EstSet((int) obj, -1, 0, 0, 1, 0x13, 0, 0, (u32) obj, 0);
            }
            if (obj->motFrame > 2865.0f) {
                if (obj->xFF) {
                    ActBtn.set(4, 0xB, (int) objTrolleyEscapeAction, (int) obj, 1, 3, 0, 0);
                } else {
                    ActBtn.set(4, 0xB, (int) objTrolleyEscapeAction, (int) obj, 1, 4, 0, 0);
                }
            }
        }
        break;
    }
    obj->partsWorldCalc();
    objTrolleyGetAdjust(obj);
    if (w->ride) {
        objTrolleyMoveAdjustPL(obj);
    }
    objTrolleyMoveAdjustEM(obj);
    objTrolleySatSet(obj);
    objTrolleyHitCk(obj);
}

void objTrolley_R0_Break(cObjTrolley* obj)
{
    TrolleyWork* w = &obj->trolley;

    objTrolleyPushMtx(obj);
    switch (obj->xFE) {
    case 0:
        obj->pos.x = -201000.0f;
        obj->pos.y = -52400.0f;
        obj->pos.z = 78000.0f;
        obj->rot.y = 0.0f;
        if (obj->xFF) {
            MotionSetCore(obj, &obj->pMotion, w->mot[2], 0, 0, 0x8001, 0);
        } else {
            MotionSetCore(obj, &obj->pMotion, w->mot[3], 0, 0, 0x8001, 0);
        }
        SndCall(6, 2, 0, 0, 0, 0);
        SndCall(6, 3, 0, 0, 0, 0);
        objTrolleyFallEM(obj);
        obj->xFE++;
    case 1:
        if (MotionMove(obj, 0)) {
            objTrolleyLostEM(obj);
            obj->be_flag &= ~2;
        }
        break;
    }
    obj->partsWorldCalc();
}

static void objTrolleySatClear(cObjTrolley* obj)
{
    TrolleyWork* w = &obj->trolley;
    int i;

    for (i = 0; i < 5; i++) {
        if (w->sat[i]) {
            w->sat[i]->flags &= ~4;
        }
        if (w->sat2[i]) {
            w->sat2[i]->flags &= ~4;
        }
    }
}

void objTrolleySatSet(cObjTrolley* obj)
{
    TrolleyWork* w = &obj->trolley;
    Vec pos;
    Vec rot;
    Vec v;
    cModel* parts;
    cSat** sat = w->sat;
    u32 i;

    for (i = 0; i < 3; i++) {
        parts = obj->getPartsPtr(Trolley_parts_tbl[i]);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1.0f;
        PSMTXMultVecSR(parts->mat, &v, &v);
        rot.x = 0.0f;
        rot.y = atan2f(v.x, v.z);
        rot.z = 0.0f;
        pos = parts->worldPos;
        pos.y += 500.0f;
        if (w->sat[i]) {
            w->sat[i]->flags |= 4;
            w->sat[i]->setCoord(&pos, &rot);
        } else {
            switch (i) {
            case 0:
            default:
                w->sat[i] = SatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 5), 0, &pos, &rot, 3);
                break;
            case 1:
                sat[1] = SatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 5), 0, &pos, &rot, 2);
                break;
            case 2:
                sat[2] = SatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 5), 0, &pos, &rot, 1);
                break;
            }
        }
        cSat** sat2 = w->sat2;
        if (w->sat2[i]) {
            w->sat2[i]->flags |= 4;
            w->sat2[i]->setCoord(&pos, &rot);
        } else {
            switch (i) {
            case 0:
            default:
                w->sat2[i] = EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 5), 0, &pos, &rot, 6);
                break;
            case 1:
                sat2[1] = EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 5), 0, &pos, &rot, 5);
                break;
            case 2:
                sat2[2] = EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 5), 0, &pos, &rot, 4);
                break;
            }
        }
    }
}

void objTrolleyEscapeAction(cObjTrolley* obj)
{
    obj->trolley.ride = 0;
    SetPlDamage((int) obj, plobjTrolleyEscape);
    obj->xFC = 2;
    obj->xFD = 0;
    obj->xFE = 0;
    obj->xFF = 1;
}

void plobjTrolleyEscape(cPlayer* pl)
{
    cEm* em = (cEm*) pl;
    cObjTrolley* obj = (cObjTrolley*) em->dmgType;
    TrolleyWork* w = &obj->trolley;
    cModel* parts = em->getPartsPtr(4);

    em->x378 = ((cEm*) pPL->dmgType)->x378;
    em->dmg.set(0, 0xF);
    switch (em->xFE) {
    case 0:
        em->pos.x = -201027.56f;
        em->pos.y = -51832.78f;
        em->pos.z = 73991.43f;
        em->rot.y = 0.0f;
        MotionSetCore(em, &em->pMotion, w->mot[4], 0, 0, 0x201, 0);
        em->atari.throughOn();
        em->be_flag &= ~0x10;
        em->xFE++;
    case 1:
        if (MotionMove(em, 0)) {
            em->xFE++;
        } else {
            if (em->frame > 15.7f && em->frame < 16.3f) {
                SndCall(1, 0x10, &parts->worldPos, 0, 0, em);
            }
            if (em->frame > 23.7f && em->frame < 24.3f) {
                SndCall(1, 0x34, &parts->worldPos, 0, 0, em);
            }
            if (em->frame > 29.7f && em->frame < 30.3f) {
                SndCall(1, 0x4F, &parts->worldPos, 0, 0, em);
            }
        }
        break;
    case 2:
        em->pos.x = -201073.98f;
        em->pos.y = -52360.74f;
        em->pos.z = 95802.8f;
        em->rot.y = 0.0f;
        MotionSetCore(em, &em->pMotion, w->mot[6], 0, 0, 0x201, 0);
        PlGachaInit();
        em->x3E0 = 90;
        em->x3E4 = 10;
        if (pG->x4F88 <= 2) {
            em->x3E4 = 5;
        }
        if (pG->x4F88 > 7) {
            em->x3E4 = 15;
        }
        em->xFE++;
    case 3:
        ActBtn.set(0x19, 5, 0, 0, 2, 2, 0, 0);
        if (Key.trg & 0x80000000) {
            if (em->x3E4) {
                em->x3E4--;
            }
        }
        MotionMove(em, 0);
        if (em->x3E0) {
            em->x3E0--;
        } else if (em->x3E4) {
            em->xFE = 6;
        } else {
            em->xFE = 4;
        }
        break;
    case 4:
        em->pos.x = -201078.39f;
        em->pos.y = -52361.7f;
        em->pos.z = 95773.43f;
        em->rot.y = 0.0f;
        MotionSetCore(em, &em->pMotion, w->mot[7], 0, 0, 0x201, 0);
        em->xFE++;
        SndRoomStrStop(2);
        SndStrReq(1, 0x2F, 0x80000003, 0, 0, 0.0f);
    case 5:
        if (MotionMove(em, 0)) {
            em->be_flag |= 0x10;
            em->atari.throughOff();
            EndPlDamage();
        } else {
            if (em->frame == 140.0f) {
                FootSeCall(1, &em->pos, 0, 0);
            }
            if (em->frame == 173.0f) {
                FootSeCall(0, &em->pos, 0, 0);
            }
        }
        break;
    case 6:
        em->pos.x = -201078.39f;
        em->pos.y = -52361.7f;
        em->pos.z = 95773.43f;
        em->rot.y = 0.0f;
        MotionSetCore(em, &em->pMotion, w->mot[8], 0, 0, 0x201, 0);
        SndCall(1, 0x4A, &pPL->getPartsPtr(4)->worldPos, 0, 0, em);
        pG->pl_life = 0;
        em->xFE++;
    case 7:
        MotionMove(em, 0);
        break;
    }
    em->x378 = em->x37C;
}

void plobjTrolleyDie(cPlayer* pl)
{
    cEm* em = (cEm*) pl;
    cObjTrolley* obj = (cObjTrolley*) em->dmgType;
    TrolleyWork* w = &obj->trolley;
    u8 step;

    em->x378 = ((cEm*) pPL->dmgType)->x378;
    em->dmg.set(0, 0xF);
    step = em->xFE;
    switch (step) {
    case 0:
        em->pos.x = -201000.0f;
        em->pos.y = -52400.0f;
        em->pos.z = 87300.0f;
        em->rot.y = 0.0f;
        MotionSetCore(em, &em->pMotion, w->mot[5], 0, 0, 1, 0);
        em->atari.throughOn();
        pGS->pl_life = step;
        em->be_flag &= ~0x10;
        em->xFE++;
    case 1:
        MotionMove(em, 0);
        break;
    }
    em->x378 = em->x37C;
}

void cObjTrolley::setMotion(void** tbl)
{
    TrolleyWork* w = &trolley;
    int i;

    for (i = 0; i < 9; i++) {
        w->mot[i] = tbl[i];
    }
    if (w->mot[0]) {
        MotionSetCore(this, &pMotion, w->mot[0], 0, 0, 0x8001, 0);
    }
}

void objTrolleyPushMtx(cObjTrolley* obj)
{
    u32 i;

    for (i = 0; i < 3; i++) {
        PSMTXCopy(obj->getPartsPtr(Trolley_parts_tbl[i])->mat, Trolley_MatOld[i]);
    }
}

int objTrolleyGetTrolleyNo(cObjTrolley* obj, Vec* pos)
{
    Mtx inv;
    Vec v;
    int i;

    for (i = 0; i < 3; i++) {
        PSMTXInverse(Trolley_MatOld[i], inv);
        PSMTXMultVec(inv, pos, &v);
        if (v.x > -900.0f && v.x < 900.0f && v.y > 0.0f && v.y < 1000.0f && v.z > -1950.0f && v.z < 1950.0f) {
            return i;
        }
    }
    return -1;
}

int objTrolleyGetTrolleyNo2(cObjTrolley* obj, Vec* pos)
{
    Mtx inv;
    Vec v;
    int i;

    for (i = 0; i < 3; i++) {
        PSMTXInverse(obj->getPartsPtr(Trolley_parts_tbl[i])->mat, inv);
        PSMTXMultVec(inv, pos, &v);
        if (v.x > -900.0f && v.x < 900.0f && v.y > 0.0f && v.y < 1000.0f && v.z > -1950.0f && v.z < 1950.0f) {
            return i;
        }
    }
    return -1;
}

void objTrolleyGetAdjust(cObjTrolley* obj)
{
    Vec p1;
    Vec p0;
    Vec d;
    cModel* parts;
    f32 a0;
    f32 a1;
    u32 i;

    for (i = 0; i < 3; i++) {
        p0.x = 0.0f;
        p0.y = 500.0f;
        p0.z = 0.0f;
        PSMTXMultVec(Trolley_MatOld[i], &p0, &p0);
        d.x = 0.0f;
        d.y = 0.0f;
        d.z = 1.0f;
        PSMTXMultVecSR(Trolley_MatOld[i], &d, &d);
        a0 = atan2f(d.x, d.z);
        parts = obj->getPartsPtr(Trolley_parts_tbl[i]);
        p1.x = 0.0f;
        p1.y = 500.0f;
        p1.z = 0.0f;
        PSMTXMultVec(parts->mat, &p1, &p1);
        d.x = 0.0f;
        d.y = 0.0f;
        d.z = 1.0f;
        PSMTXMultVecSR(parts->mat, &d, &d);
        a1 = atan2f(d.x, d.z);
        PSVECSubtract(&p1, &p0, &Trolley_vec[i]);
        Trolley_dir[i] = Muku2(a0, a1, PI);
    }
}

void objTrolleySetAdjust(cObjTrolley* obj, cEm* em)
{
    Mtx inv;
    Vec v;
    Vec d;
    cModel* parts;
    int no;

    no = objTrolleyGetTrolleyNo(obj, &em->pos);
    if (no == -1) {
        return;
    }
    em->be_flag |= 0x20000000;
    parts = obj->getPartsPtr(Trolley_parts_tbl[no]);
    PSMTXInverse(Trolley_MatOld[no], inv);
    PSMTXMultVec(inv, &em->pos, &v);
    PSMTXMultVec(parts->mat, &v, &v);
    PSVECSubtract(&v, &em->pos, &d);
    PSVECAdd(&em->x3A8, &d, &em->x3A8);
    em->rot.y += Trolley_dir[no];
    em->rot.y = LIMIT_ANGLE(em->rot.y);
    em->setPos(&v);
    if (em->id == 0) {
        if (CamCtrl.x250) {
            PSVECAdd(&((Camera*) CamCtrl.x250)->param.at, &d, &((Camera*) CamCtrl.x250)->param.at);
            PSVECAdd(&((Camera*) CamCtrl.x250)->param.pos, &d, &((Camera*) CamCtrl.x250)->param.pos);
        }
        pG->quake_ofs = d;
    }
}

void objTrolleyMoveAdjustPL(cObjTrolley* obj)
{
    objTrolleySetAdjust(obj, pPL);
}

void objTrolleyMoveAdjustEM(cObjTrolley* obj)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((em->be_flag & 0x201) == 1) {
            if (em->id == 0x42) {
                ((cEmWep*) em)->setParentMatCalc(1);
            } else if (em->id > 0xF) {
                if (em->id <= 0x40) {
                    objTrolleySetAdjust(obj, em);
                }
            }
        }
    }
}

int cObjTrolley::ckTrolleyRide(Vec* pos, u8* partsNo, Vec* out)
{
    Mtx inv;
    cModel* parts;
    int no;

    no = objTrolleyGetTrolleyNo2(this, pos);
    if (no == -1) {
        return 0;
    }
    parts = getPartsPtr(Trolley_parts_tbl[no]);
    PSMTXInverse(Trolley_MatOld[no], inv);
    PSMTXMultVec(inv, pos, out);
    *partsNo = Trolley_parts_tbl[no];
    return 1;
}

int cObjTrolley::ckTrolleyRideAdjust(Vec* pos, Vec* out)
{
    Mtx inv;
    Vec v;
    cModel* parts;
    int no;

    *out = *pos;
    no = objTrolleyGetTrolleyNo(this, pos);
    if (no == -1) {
        return 0;
    }
    parts = getPartsPtr(Trolley_parts_tbl[no]);
    PSMTXInverse(Trolley_MatOld[no], inv);
    PSMTXMultVec(inv, pos, &v);
    PSMTXMultVec(parts->mat, &v, out);
    return 1;
}

void cObjTrolley::setStart()
{
    trolley.flags |= 1;
}

void cObjTrolley::set2ndStart()
{
    trolley.flags |= 2;
    trolley.flags &= ~4;
}

int cObjTrolley::ckStop()
{
    if (trolley.flags & 4) {
        return 1;
    }
    return 0;
}

void objTrolleyHitCk(cObjTrolley* obj)
{
    Vec v;
    cModel* parts;

    parts = obj->getPartsPtr(0);
    if ((parts->worldPos.x - parts->x88.x) * (parts->worldPos.x - parts->x88.x) +
        (parts->worldPos.y - parts->x88.y) * (parts->worldPos.y - parts->x88.y) +
        (parts->worldPos.z - parts->x88.z) * (parts->worldPos.z - parts->x88.z) < 2500.0f) {
        return;
    }
    v.x = 0.0f;
    v.y = 500.0f;
    v.z = 2700.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 400.0f);
    v.x = 800.0f;
    v.y = 500.0f;
    v.z = 2700.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 400.0f);
    v.x = -800.0f;
    v.y = 500.0f;
    v.z = 2700.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 400.0f);
}

void objTrolleyFallEM(cObjTrolley* obj)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((em->be_flag & 0x201) == 1 && em->id > 0xF && em->id <= 0x20 && em->hp > 0) {
            em->hp = 0;
            em->x9BC = em->rot.y;
            em->be_flag |= 0x10000;
            em->xFC = 2;
            em->xFD = 7;
            em->xFE = 0;
            em->xFF = 1;
        }
    }
}

void objTrolleyLostEM(cObjTrolley* obj)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((em->be_flag & 0x201) == 1 && em->id > 0xF && em->id <= 0x20 && (em->be_flag & 2)) {
            ((cEmRoom*) em)->setTrolleyLost();
        }
    }
}

// The next unit's .sdata starts 8-byte aligned in the original link.
asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
