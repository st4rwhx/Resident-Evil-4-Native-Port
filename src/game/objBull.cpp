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
#include "snd.h"
#include "rnd.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"

// Bulldozer (obj 0x3E): parts 2 carries the player, the partner and the enemies standing on it
// through the break / move / lift routines of its motion table; the partner drives it
// (Sub_bull_*) while the player shoots the pursuers (objBullHitCk).
class cObjBull : public cObj {
public:
    virtual void move();
    virtual ~cObjBull() {}

    void setMotion(void** tbl);
    int ckBullRide(Vec* pos, u8* partsNo, Vec* out);
    int ckBullRideAdjust(Vec* pos, Vec* out);
    int ckGoal();
    void setRide();
    int ckBreak1st();
    int ckBreak2nd();
    int ckBreak3rd();
    int ckBreak4th();
    int ckLift();
    int ckTruckGo();
    int ckLiftWait();
    void setSubBullDrive();
    void setSubBullFinger();
    void setSubBullLookBack();
    int getMoveFrameToLift();
    int getMoveFrameRtn();
    void setAdjustMode(u8 mode, void (*func)(cObj*));
    void setBreakTruck();
};

extern "C" {
int MotionMove(cModel* m, int a);
void LifeDownSet(cEm* em, int dmg, int a);
void objBull_R0_Set(cObjBull* obj);
void objBull_R0_Break1st(cObjBull* obj);
void objBull_R0_To2nd(cObjBull* obj);
void objBull_R0_Break2nd(cObjBull* obj);
static void objBull_R0_ToLift(cObjBull* obj);
void objBull_R0_LiftWait(cObjBull* obj);
void objBull_R0_Lift(cObjBull* obj);
void objBull_R0_To3rd(cObjBull* obj);
void objBull_R0_Break3rd(cObjBull* obj);
void objBull_R0_To4th(cObjBull* obj);
void objBull_R0_Break4th(cObjBull* obj);
void objBull_R0_Collision(cObjBull* obj);
void objBullSatClear(cObjBull* obj);
void objBullSatSet(cObjBull* obj, int moving);
void objBullPushMtx(cObjBull* obj);
static int objBullGetBullNo(cObjBull* obj, Vec* pos);
int objBullGetBullNo2(cObjBull* obj, Vec* pos);
void objBullGetAdjust(cObjBull* obj);
void objBullSetAdjust(cObjBull* obj, cEm* em);
static void objBullMoveAdjustPL(cObjBull* obj);
void objBullMoveAdjustEM(cObjBull* obj);
void objBullHitCk(cObjBull* obj);
void Sub_bull_drive(cEm* em);
void Sub_bull_operation(cEm* em);
void Sub_bull_lookback(cEm* em);
void Sub_bull_look(cEm* em);
void Sub_dm_bull(cEm* em);
int SubCkNearEm();
}
void MotionSetCore(cModel* m, void* work, void* mot, int a, int b, int c, int d);

void (*ObjBull_R0_move_tbl[12])(cObjBull*) = {
    objBull_R0_Set,      objBull_R0_Break1st, objBull_R0_To2nd, objBull_R0_Break2nd,
    objBull_R0_ToLift,   objBull_R0_LiftWait, objBull_R0_Lift,  objBull_R0_To3rd,
    objBull_R0_Break3rd, objBull_R0_To4th,    objBull_R0_Break4th, objBull_R0_Collision,
};

static Mtx Bull_MatOld;     // parts matrix of the previous frame (objBullPushMtx)
Vec Bull_vec;               // movement of this frame (objBullGetAdjust)
static u8 Bull_parts = 2;   // the parts the riders stand on
static f32 Bull_dir;        // turn of this frame

cObj* SetBull(void* bin, void* tpl, Vec* pos, Vec* rot, u32 type)
{
    cObj* obj;
    BullWork* w;
    int i;
    void** p;

    obj = ObjMgr.create(0x3E);
    if (obj == 0) {
        return 0;
    }
    w = &obj->bull;
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
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 5000.0f, 5000.0f, 5000.0f };

    obj->lightInfo.init2(0, 1, &p0, &p1, 0x10);
    AtariInit(&obj->sub2B4.atari, 0.0f, 1000.0f, -700.0f, 350.0f, 700.0f, 700.0f, 1000.0f, 0, 2, 0);
    obj->sub2B4.atari.throughOn();
    w->sat = 0;
    w->sat2 = 0;
    w->eat = 0;
    w->ride = 0;
    p = w->mot;
    for (i = 0; i < 12; i++) {
        *p++ = 0;
    }
    w->break1st = 1;
    w->break2nd = 3;
    w->break4th = 3;
    w->break3rd = 1;
    w->type = type;
    w->x50 = type;
    w->adjustMode = 1;
    obj->xFC = 0;
    obj->xFD = 0;
    obj->xFE = 0;
    obj->xFF = 0;
    objBullSatSet((cObjBull*) obj, 0);
    return obj;
}

void cObjBull::move()
{
    objBullSatClear(this);
    ObjBull_R0_move_tbl[xFC](this);
}

void objBull_R0_Set(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    w->cnt = 0;
    w->timer = 0;
    if (w->mot[0]) {
        MotionSetCore(obj, &obj->pMotion, w->mot[0], 0, 0, 0x8001, 0);
        MotionMove(obj, 0);
    } else {
        obj->matUpdate();
    }
    obj->partsWorldCalc();
    objBullSatSet(obj, 0);
}

void objBull_R0_Break1st(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->xFE) {
    case 0:
        SndCall(6, 6, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        SndCall(6, 7, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        w->x62 = 0;
        w->timer = 0;
        obj->xFE++;
    case 1:
        MotionSetCore(obj, &obj->pMotion, w->mot[0], 0, 0, 0x8001, 0);
        MotionMove(obj, 0);
        if ((s16) pG->sub_life > 0) {
            SetSubBulldozer((int) Sub_bull_operation, (int) Sub_dm_bull);
        }
        obj->xFE++;
        break;
    case 2:
        MotionSetCore(obj, &obj->pMotion, w->mot[0], 0, 0, 0x8001, 0);
        if (w->break1st == 0 || --w->break1st == 0) {
            obj->bull.flags |= 2;
        }
        obj->xFE++;
    case 3:
        if (MotionMove(obj, 0)) {
            if (w->break1st == 0) {
                obj->xFC = 2;
                obj->xFD = 0;
                obj->xFE = 0;
                obj->xFF = 0;
            } else {
                obj->xFE = 0;
            }
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->ride) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

void objBull_R0_To2nd(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->xFE) {
    case 0:
        w->timer = 0;
        MotionSetCore(obj, &obj->pMotion, w->mot[1], 0, 0, 0x8001, 0);
        SndCall(6, 8, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        SndCall(6, 9, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        obj->xFE++;
    case 1:
        w->cnt++;
        if (MotionMove(obj, 0)) {
            obj->xFC = 3;
            obj->xFD = 0;
            obj->xFE = 0;
            obj->xFF = 0;
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->ride) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

void objBull_R0_Break2nd(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->xFE) {
    case 0:
        w->timer = 0;
        SndCall(6, 0xA, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        SndCall(6, 0xB, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        w->x62 = 0;
        obj->xFE++;
    case 1:
        MotionSetCore(obj, &obj->pMotion, w->mot[2], 0, 0, 0x8001, 0);
        MotionMove(obj, 0);
        if ((s16) pG->sub_life > 0) {
            SetSubBulldozer((int) Sub_bull_operation, (int) Sub_dm_bull);
        }
        obj->xFE++;
        break;
    case 2:
        MotionSetCore(obj, &obj->pMotion, w->mot[2], 0, 0, 0x8001, 0);
        if (w->break2nd == 0 || --w->break2nd == 0) {
            obj->bull.flags |= 4;
        }
        obj->xFE++;
    case 3:
        if (MotionMove(obj, 0)) {
            if (w->break2nd == 0) {
                obj->xFC = 4;
                obj->xFD = 0;
                obj->xFE = 0;
                obj->xFF = 0;
            } else {
                obj->xFE = 0;
            }
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->ride) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

static void objBull_R0_ToLift(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->xFE) {
    case 0:
        w->timer = 0;
        SndCall(6, 8, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        SndCall(6, 9, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        MotionSetCore(obj, &obj->pMotion, w->mot[3], 0, 0, 0x8001, 0);
        obj->xFE++;
    case 1:
        w->cnt++;
        if (MotionMove(obj, 0)) {
            obj->xFC = 5;
            obj->xFD = 0;
            obj->xFE = 0;
            obj->xFF = 0;
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->ride) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

void objBull_R0_LiftWait(cObjBull* obj)
{
    BullWork* w = &obj->bull;
    int zero;

    objBullPushMtx(obj);
    switch (obj->xFE) {
    case 0:
        w->timer = 0;
        SndCall(6, 0xA, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        SndCall(6, 0xB, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        obj->xFE++;
    case 1:
        MotionSetCore(obj, &obj->pMotion, w->mot[3], 0, 0, 0x8001, (u16) ((*(u16*) w->mot[3] & 0x3FFF) - 1));
        MotionMove(obj, 0);
        zero = 0;
        if (pG->flags_174 & 0x08000000) {
            obj->xFC = 6;
            obj->xFD = zero;
            obj->xFE = zero;
            obj->xFF = zero;
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->ride) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 0);
    w->timer++;
}

void objBull_R0_Lift(cObjBull* obj)
{
    BullWork* w = &obj->bull;
    int zero;

    objBullPushMtx(obj);
    switch (obj->xFE) {
    case 0:
        w->timer = 0;
        MotionSetCore(obj, &obj->pMotion, w->mot[4], 0, 0, 0x8001, 0);
        SndCall(6, 0xA, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        SndCall(6, 0xB, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        obj->bull.flags |= 0x20;
        obj->xFE++;
    case 1:
        w->cnt++;
        if (MotionMove(obj, 0)) {
            w->flags |= 0x100;
            obj->xFE++;
        }
        break;
    case 2:
        obj->xFE++;
    case 3:
        MotionMove(obj, 0);
        zero = 0;
        if (pG->flags_174 & 0x00400000) {
            obj->xFC = 7;
            obj->xFD = zero;
            obj->xFE = zero;
            obj->xFF = zero;
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->ride) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 0);
    w->timer++;
}

void objBull_R0_To3rd(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->xFE) {
    case 0:
        w->timer = 0;
        MotionSetCore(obj, &obj->pMotion, w->mot[5], 0, 0, 0x8001, 0);
        SndCall(6, 8, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        SndCall(6, 9, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        obj->bull.flags &= ~0x20;
        obj->xFE++;
    case 1:
        w->cnt++;
        if (MotionMove(obj, 0)) {
            obj->xFC = 8;
            obj->xFD = 0;
            obj->xFE = 0;
            obj->xFF = 0;
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->ride) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

void objBull_R0_Break3rd(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->xFE) {
    case 0:
        w->timer = 0;
        SndCall(6, 0xA, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        SndCall(6, 0xB, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        w->x62 = 0;
        obj->xFE++;
    case 1:
        MotionSetCore(obj, &obj->pMotion, w->mot[6], 0, 0, 0x8001, 0);
        MotionMove(obj, 0);
        if ((s16) pG->sub_life > 0) {
            SetSubBulldozer((int) Sub_bull_operation, (int) Sub_dm_bull);
        }
        obj->xFE++;
        break;
    case 2:
        MotionSetCore(obj, &obj->pMotion, w->mot[6], 0, 0, 0x8001, 0);
        if (w->break3rd == 0 || --w->break3rd == 0) {
            obj->bull.flags |= 8;
        }
        obj->xFE++;
    case 3:
        if (MotionMove(obj, 0)) {
            if (w->break3rd == 0) {
                obj->xFC = 9;
                obj->xFD = 0;
                obj->xFE = 0;
                obj->xFF = 0;
            } else {
                obj->xFE = 0;
            }
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->ride) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

void objBull_R0_To4th(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->xFE) {
    case 0:
        w->timer = 0;
        MotionSetCore(obj, &obj->pMotion, w->mot[7], 0, 0, 0x8001, 0);
        SndCall(6, 8, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        SndCall(6, 9, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        obj->xFE++;
    case 1:
        w->cnt++;
        if (MotionMove(obj, 0)) {
            obj->xFC = 0xA;
            obj->xFD = 0;
            obj->xFE = 0;
            obj->xFF = 0;
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->ride) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

void objBull_R0_Break4th(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->xFE) {
    case 0:
        w->timer = 0;
        w->x62 = 0;
        SndCall(6, 0xA, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        SndCall(6, 0xB, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        obj->xFE++;
    case 1:
        MotionSetCore(obj, &obj->pMotion, w->mot[8], 0, 0, 0x8001, 0);
        MotionMove(obj, 0);
        if ((s16) pG->sub_life > 0) {
            SetSubBulldozer((int) Sub_bull_operation, (int) Sub_dm_bull);
        }
        obj->xFE++;
        break;
    case 2:
        MotionSetCore(obj, &obj->pMotion, w->mot[8], 0, 0, 0x8001, 0);
        if (w->break4th == 0 || --w->break4th == 0) {
            obj->bull.flags |= 0x10;
        }
        obj->xFE++;
    case 3:
        if (MotionMove(obj, 0)) {
            if (w->break4th == 0) {
                obj->xFC = 0xB;
                obj->xFD = 0;
                obj->xFE = 0;
                obj->xFF = 0;
            } else {
                obj->xFE = 0;
            }
        }
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->ride) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

void objBull_R0_Collision(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    objBullPushMtx(obj);
    switch (obj->xFE) {
    case 0:
        w->timer = 0;
        MotionSetCore(obj, &obj->pMotion, w->mot[9], 0, 0, 0x8001, 0);
        SndCall(6, 8, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        SndCall(6, 9, &((cParts*) obj->pParts)[2].worldPos, 0, 0, obj);
        w->frame = (*(u16*) w->mot[9] & 0x3FFF) - 30;
        obj->bull.flags |= 0x40;
        w->x62 = 0;
        w->breakTruck = 0;
        obj->xFE++;
    case 1:
        w->cnt++;
        if (MotionMove(obj, 0)) {
            if (w->breakTruck) {
                obj->xFE = 2;
            } else {
                obj->xFE = 4;
            }
        }
        break;
    case 2:
        MotionSetCore(obj, &obj->pMotion, w->mot[10], 0, 0, 0x8201, 0);
        if ((s16) pG->sub_life > 0) {
            SetSubBulldozer((int) Sub_bull_operation, (int) Sub_dm_bull);
        }
        obj->xFE++;
    case 3:
        w->cnt++;
        if (MotionMove(obj, 0)) {
            w->flags |= 1;
        }
        break;
    case 4:
        MotionSetCore(obj, &obj->pMotion, w->mot[11], 0, 0, 0x8001, 0);
        obj->bull.flags |= 0x80;
        obj->xFE++;
    case 5:
        w->cnt++;
        MotionMove(obj, 0);
        break;
    }
    obj->partsWorldCalc();
    objBullGetAdjust(obj);
    if (w->ride) {
        objBullMoveAdjustPL(obj);
    }
    objBullMoveAdjustEM(obj);
    objBullSatSet(obj, 1);
    objBullHitCk(obj);
    w->timer++;
}

void objBullSatClear(cObjBull* obj)
{
    BullWork* w = &obj->bull;

    if (w->sat) {
        w->sat->flags &= ~4;
    }
    if (w->sat2) {
        w->sat2->flags &= ~4;
    }
    if (w->eat) {
        w->eat->flags &= ~4;
    }
}

void objBullSatSet(cObjBull* obj, int moving)
{
    BullWork* w = &obj->bull;
    Vec pos;
    Vec rot;
    Vec v;
    cModel* parts;

    parts = obj->getPartsPtr(Bull_parts);
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1.0f;
    PSMTXMultVecSR(parts->mat, &v, &v);
    rot.x = 0.0f;
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    pos = parts->worldPos;
    pos.y += 500.0f;
    if (w->sat) {
        w->sat->flags |= 4;
        w->sat->setCoord(&pos, &rot);
    } else {
        w->sat = SatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 5), 0, &pos, &rot, 1);
    }
    if (w->eat) {
        w->eat->flags |= 4;
        w->eat->setCoord(&pos, &rot);
    } else {
        w->eat = EatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 5), 0, &pos, &rot, 7);
    }
    if (moving) {
        if (w->sat2) {
            w->sat2->flags |= 4;
            w->sat2->setCoord(&pos, &rot);
        } else {
            w->sat2 = SatMgr.create(ROOM_ARC_PTR(pG->pRoomArc, 5), 0, &pos, &rot, 8);
        }
    }
}

void cObjBull::setMotion(void** tbl)
{
    BullWork* w = &bull;

    w->mot[0] = tbl[0];
    w->mot[1] = tbl[1];
    w->mot[2] = tbl[2];
    w->mot[3] = tbl[3];
    w->mot[4] = tbl[4];
    w->mot[5] = tbl[5];
    w->mot[6] = tbl[6];
    w->mot[7] = tbl[7];
    w->mot[8] = tbl[8];
    w->mot[9] = tbl[9];
    w->mot[10] = tbl[10];
    w->mot[11] = tbl[11];
    MotionSetCore(this, &pMotion, tbl[0], 0, 0, 0x8001, 0);
}

void objBullPushMtx(cObjBull* obj)
{
    PSMTXCopy(obj->getPartsPtr(Bull_parts)->mat, Bull_MatOld);
}

static int objBullGetBullNo(cObjBull* obj, Vec* pos)
{
    Mtx inv;
    Vec v;

    PSMTXInverse(Bull_MatOld, inv);
    PSMTXMultVec(inv, pos, &v);
    if (v.x > -2300.0f && v.x < 2300.0f && v.y > -500.0f && v.y < 1000.0f && v.z > -4400.0f && v.z < 4400.0f) {
        return 1;
    }
    return 0;
}

int objBullGetBullNo2(cObjBull* obj, Vec* pos)
{
    Mtx inv;
    Vec v;

    PSMTXInverse(obj->getPartsPtr(Bull_parts)->mat, inv);
    PSMTXMultVec(inv, pos, &v);
    if (v.x > -2300.0f && v.x < 2300.0f && v.y > 0.0f && v.y < 1000.0f && v.z > -4400.0f && v.z < 4400.0f) {
        return 1;
    }
    return 0;
}

void objBullGetAdjust(cObjBull* obj)
{
    Vec p1;
    Vec p0;
    Vec d;
    cModel* parts;
    f32 a0;
    f32 a1;

    p0.x = 0.0f;
    p0.y = 500.0f;
    p0.z = 0.0f;
    PSMTXMultVec(Bull_MatOld, &p0, &p0);
    d.x = 0.0f;
    d.y = 0.0f;
    d.z = 1.0f;
    PSMTXMultVecSR(Bull_MatOld, &d, &d);
    a0 = atan2f(d.x, d.z);
    parts = obj->getPartsPtr(Bull_parts);
    p1.x = 0.0f;
    p1.y = 500.0f;
    p1.z = 0.0f;
    PSMTXMultVec(parts->mat, &p1, &p1);
    d.x = 0.0f;
    d.y = 0.0f;
    d.z = 1.0f;
    PSMTXMultVecSR(parts->mat, &d, &d);
    a1 = atan2f(d.x, d.z);
    PSVECSubtract(&p1, &p0, &Bull_vec);
    Bull_dir = Muku2(a0, a1, PI);
}

void objBullSetAdjust(cObjBull* obj, cEm* em)
{
    BullWork* w = &obj->bull;
    Mtx inv;
    Vec v;
    Vec d;
    cModel* parts;

    if (objBullGetBullNo(obj, &em->pos) == 0) {
        return;
    }
    em->be_flag |= 0x20000000;
    if (w->adjustMode == 0) {
        return;
    }
    parts = obj->getPartsPtr(Bull_parts);
    PSMTXInverse(Bull_MatOld, inv);
    PSMTXMultVec(inv, &em->pos, &v);
    PSMTXMultVec(parts->mat, &v, &v);
    PSVECSubtract(&v, &em->pos, &d);
    PSVECAdd(&em->x3A8, &d, &em->x3A8);
    em->rot.y += Bull_dir;
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

static void objBullMoveAdjustPL(cObjBull* obj)
{
    if (obj->bull.adjustFunc) {
        obj->bull.adjustFunc(obj);
    }
    objBullSetAdjust(obj, pPL);
}

void objBullMoveAdjustEM(cObjBull* obj)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((em->be_flag & 0x201) == 1) {
            if (em->id == 0x42) {
                ((cEmWep*) em)->setParentMatCalc(1);
            } else if (em->id == 3) {
                objBullSetAdjust(obj, em);
            } else if (em->id > 0xF) {
                if (em->id <= 0x40) {
                    objBullSetAdjust(obj, em);
                }
            }
        }
    }
}

int cObjBull::ckBullRide(Vec* pos, u8* partsNo, Vec* out)
{
    Mtx inv;
    cModel* parts;

    if (objBullGetBullNo2(this, pos) == 0) {
        return 0;
    }
    parts = getPartsPtr(Bull_parts);
    PSMTXInverse(Bull_MatOld, inv);
    if (out) {
        PSMTXMultVec(inv, pos, out);
    }
    if (partsNo) {
        *partsNo = Bull_parts;
    }
    return 1;
}

int cObjBull::ckBullRideAdjust(Vec* pos, Vec* out)
{
    Mtx inv;
    Vec v;
    cModel* parts;

    if (objBullGetBullNo(this, pos) == 0) {
        return 0;
    }
    parts = getPartsPtr(Bull_parts);
    PSMTXInverse(Bull_MatOld, inv);
    PSMTXMultVec(inv, pos, &v);
    PSMTXMultVec(parts->mat, &v, out);
    return 1;
}

void objBullHitCk(cObjBull* obj)
{
    Vec v;
    cModel* parts;

    parts = obj->getPartsPtr(4);
    if ((parts->worldPos.x - parts->x88.x) * (parts->worldPos.x - parts->x88.x) +
        (parts->worldPos.y - parts->x88.y) * (parts->worldPos.y - parts->x88.y) +
        (parts->worldPos.z - parts->x88.z) * (parts->worldPos.z - parts->x88.z) < 2500.0f) {
        return;
    }
    v.x = 0.0f;
    v.y = 500.0f;
    v.z = 2500.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 1000.0f);
    v.x = 1000.0f;
    v.y = 500.0f;
    v.z = 2500.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 1000.0f);
    v.x = 2000.0f;
    v.y = 500.0f;
    v.z = 2500.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 1000.0f);
    v.x = -1000.0f;
    v.y = 500.0f;
    v.z = 2500.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 1000.0f);
    v.x = -2000.0f;
    v.y = 500.0f;
    v.z = 2500.0f;
    PSMTXMultVec(parts->mat, &v, &v);
    PlWepHitCheck2(0, &v, &v, 0x12, 3, 1000.0f);
}

int cObjBull::ckGoal()
{
    if (bull.flags & 1) {
        return 1;
    }
    return 0;
}

void cObjBull::setRide()
{
    BullWork* w = &bull;
    Vec p;
    cModel* parts;
    int zero = 0;

    parts = getPartsPtr(Bull_parts);
    p = parts->worldPos;
    p.y += 1000.0f;
    pPL->setPos(&p);
    pG->flags_500C |= 0x20;
    w->ride = 1;
    if (pSUB) {
        p = parts->worldPos;
        p.y += 1000.0f;
        p.z += -500.0f;
        pSUB->setPos(&p);
    }
    w->timer = zero;
    switch (w->type) {
    case 0:
    default:
        xFC = 1;
        xFD = 0;
        xFE = 0;
        xFF = 0;
        break;
    case 1:
        xFC = 4;
        xFD = 0;
        xFE = 0;
        xFF = 0;
        break;
    case 2:
        xFC = 5;
        xFD = 0;
        xFE = 0;
        xFF = 0;
        break;
    case 3:
        xFC = 7;
        xFD = 0;
        xFE = 0;
        xFF = 0;
        break;
    case 4:
        xFC = 9;
        xFD = 0;
        xFE = 0;
        xFF = 0;
        break;
    }
}

int cObjBull::ckBreak1st()
{
    if (bull.flags & 2) {
        return 1;
    }
    return 0;
}

int cObjBull::ckBreak2nd()
{
    if (bull.flags & 4) {
        return 1;
    }
    return 0;
}

int cObjBull::ckBreak3rd()
{
    if (bull.flags & 8) {
        return 1;
    }
    return 0;
}

int cObjBull::ckBreak4th()
{
    if (bull.flags & 0x10) {
        return 1;
    }
    return 0;
}

int cObjBull::ckLift()
{
    if (bull.flags & 0x20) {
        return 1;
    }
    return 0;
}

int cObjBull::ckTruckGo()
{
    if (bull.flags & 0x40) {
        return 1;
    }
    return 0;
}

int cObjBull::ckLiftWait()
{
    if (bull.flags & 0x100) {
        return 1;
    }
    return 0;
}

// The partner (`em`) sits in the driver's seat (parts 2 of the bulldozer it drives, em->dmgType).
static inline void SubBullSeat(cEm* em)
{
    Vec v;
    cModel* parts;

    if (em->dmgType) {
        parts = ((cObj*) em->dmgType)->getPartsPtr(2);
        v.x = 0.0f;
        v.y = 452.29f;
        v.z = 3173.64f;
        PSMTXMultVec(parts->mat, &v, &em->pos);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1.0f;
        PSMTXMultVecSR(parts->mat, &v, &v);
        em->rot.y = atan2f(v.x, v.z);
    }
}

void Sub_bull_drive(cEm* em)
{
    pG->flags_5014 |= 0x00800000;
    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        em->atari.throughOn();
        if (em->xFF) {
            MotionSetCore(em, &em->pMotion, ROOM_ARC_PTR(pG->pRoomArc, 50), 0, 0, 5, 0);
        } else {
            MotionSetCore(em, &em->pMotion, ROOM_ARC_PTR(pG->pRoomArc, 50), 0, 3, 5, 0);
        }
        em->subHideMode = (u8) ((u32) Rnd() % 100);
        em->xFE++;
    case 1:
        SubBullSeat(em);
        MotionMove(em, 0);
        if (em->subHideMode) {
            em->subHideMode--;
        } else if (SubCkNearEm()) {
            if ((s16) pG->sub_life > 0) {
                SetSubBulldozer((int) Sub_bull_lookback, (int) Sub_dm_bull);
            }
        }
        break;
    }
}

void Sub_bull_operation(cEm* em)
{
    pG->flags_5014 |= 0x00800000;
    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        em->atari.throughOn();
        MotionSetCore(em, &em->pMotion, ROOM_ARC_PTR(pGS->pRoomArc, 51), 0, 3, 1, 0);
        em->xFE++;
    case 1:
        SubBullSeat(em);
        if (MotionMove(em, 0)) {
            if ((s16) pG->sub_life > 0) {
                SetSubBulldozer((int) Sub_bull_drive, (int) Sub_dm_bull);
            }
        }
        break;
    }
}

void Sub_bull_lookback(cEm* em)
{
    cModel* parts;

    pG->flags_5014 |= 0x00800000;
    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        em->atari.throughOn();
        MotionSetCore(em, &em->pMotion, ROOM_ARC_PTR(pGS->pRoomArc, 66), 0, 3, 1, 0);
        parts = em->getPartsPtr(3);
        if (em->xFF) {
            SndStop(em->subSndId, 0);
            em->subSndId = SndCall(6, 3, &parts->worldPos, 0, 0, em);
        } else {
            SndStop(em->subSndId, 0);
            em->subSndId = SndCall(6, 0x19, &parts->worldPos, 0, 0, em);
        }
        em->xFE++;
    case 1:
        SubBullSeat(em);
        if (MotionMove(em, 0)) {
            if ((s16) pG->sub_life > 0) {
                SetSubBulldozer((int) Sub_bull_drive, (int) Sub_dm_bull);
            }
        }
        break;
    }
}

void Sub_bull_look(cEm* em)
{
    pG->flags_5014 |= 0x00800000;
    em->setStatus(3);
    switch (em->xFE) {
    case 0:
        em->atari.throughOn();
        MotionSetCore(em, &em->pMotion, ROOM_ARC_PTR(pGS->pRoomArc, 67), 0, 3, 1, 0);
        em->xFE++;
    case 1:
        SubBullSeat(em);
        if (MotionMove(em, 0)) {
            if ((s16) pG->sub_life > 0) {
                SetSubBulldozer((int) Sub_bull_drive, (int) Sub_dm_bull);
            }
        }
        break;
    }
}

void Sub_dm_bull(cEm* em)
{
    int dmg;
    int type = 2;

    pG->flags_5014 |= 0x00800000;
    em->setStatus(3);
    em->dmType = type;
    switch (em->xFE) {
    case 0:
        em->atari.throughOn();
        dmg = 0;
        switch (em->dmWep) {
        default:
            dmg = 9999;
            break;
        case 0xD:
        case 0x12:
        case 0x13:
            if (em->dmRad < 9000000.0f) {
                dmg = 9999;
            } else {
                dmg = 500;
            }
            break;
        case 0x18:
            dmg = 500;
            break;
        case 0xE:
        case 0x17:
        case 0x2A:
            break;
        }
        LifeDownSet(em, dmg, 0);
        if ((s16) pG->sub_life <= 0) {
            MotionSetCore(em, &em->pMotion, ROOM_ARC_PTR(pG->pRoomArc, 53), 0, 3, 1, 0);
            SndStop(em->subSndId, 0);
            em->subSndId = SndCall(8, 0xD, &em->pParts->worldPos, em->id, 0, 0);
        } else {
            MotionSetCore(em, &em->pMotion, ROOM_ARC_PTR(pG->pRoomArc, 52), 0, 3, 1, 0);
            em->dmType = 1;
            SndStop(em->subSndId, 0);
            em->subSndId = SndCall(8, 9, &em->pParts->worldPos, em->id, 0, 0);
        }
        em->xFE++;
    case 1:
        SubBullSeat(em);
        if (MotionMove(em, 0)) {
            if ((s16) pG->sub_life > 0) {
                SetSubBulldozer((int) Sub_bull_drive, (int) Sub_dm_bull);
            }
        }
        break;
    }
}

void cObjBull::setSubBullDrive()
{
    if (pSUB) {
        if ((s16) pG->sub_life > 0) {
            SetSubBulldozer((int) Sub_bull_drive, (int) Sub_dm_bull);
            pSUB->xFF = 1;
            pSUB->dmgType = (int) this;
        }
    }
}

void cObjBull::setSubBullFinger()
{
    if (pSUB) {
        if ((s16) pG->sub_life > 0) {
            SetSubBulldozer((int) Sub_bull_look, (int) Sub_dm_bull);
            pSUB->dmgType = (int) this;
        }
    }
}

void cObjBull::setSubBullLookBack()
{
    if (pSUB) {
        if ((s16) pG->sub_life > 0) {
            SetSubBulldozer((int) Sub_bull_lookback, (int) Sub_dm_bull);
            pSUB->xFF = 1;
            pSUB->dmgType = (int) this;
        }
    }
}

int cObjBull::getMoveFrameToLift()
{
    return bull.timer;
}

int cObjBull::getMoveFrameRtn()
{
    return bull.timer;
}

void cObjBull::setAdjustMode(u8 mode, void (*func)(cObj*))
{
    BullWork* w = &bull;

    w->adjustMode = mode;
    w->adjustFunc = func;
}

void cObjBull::setBreakTruck()
{
    bull.breakTruck = 1;
}

// An enemy stands in the box in front of the partner's seat (the player's position widens it).
int SubCkNearEm()
{
    Mtx inv;
    Vec v;
    f32 zmin;
    u32 i;

    if (pSUB == 0) {
        return 0;
    }
    PSMTXInverse(pSUB->mat, inv);
    zmin = -1000.0f;
    PSMTXMultVec(inv, &pPL->pos, &v);
    if (v.x < -2000.0f) {
        zmin = -6000.0f;
    }
    if (v.z < -6000.0f) {
        zmin = -6000.0f;
    }
    if (v.z > 0.0f) {
        zmin = -6000.0f;
    }
    if (v.y < -1000.0f) {
        zmin = -6000.0f;
    }
    if (v.y > 2000.0f) {
        zmin = -6000.0f;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);

        if ((em->be_flag & 0x201) == 1 && em->id > 0xF && em->id <= 0x20 && em->hp > 0 && (em->be_flag & 2)) {
            PSMTXMultVec(inv, &em->pos, &v);
            if (v.x < -2000.0f) {
                continue;
            }
            if (v.x < -2000.0f) {
                continue;
            }
            if (v.z < zmin) {
                continue;
            }
            if (v.z > 0.0f) {
                continue;
            }
            if (v.y < -1000.0f) {
                continue;
            }
            if (v.y > 2000.0f) {
                continue;
            }
            return 1;
        }
    }
    return 0;
}

// The next unit's .sdata starts 8-byte aligned in the original link.
asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
