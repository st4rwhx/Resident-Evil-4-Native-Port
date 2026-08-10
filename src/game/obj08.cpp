#include "atari.h"
#include "light.h"
#include "obj.h"
#include "em.h"
#include "esp.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "pad.h"
#include "dbmodule.h"
#include "player.h"

// Thrown object (bottle, dynamite, ...): flies under gravity, optionally spinning, and checks
// the scenario, the enemies and the player for hits.
class cObj08 : public cObj {
public:
    virtual void move();
};

// GetWepTargetList entry.
struct WepTarget {
    cEm* em;
    EmHitInfo* part;
};

extern cModel* pSUB;

extern "C" {
int MotionMove(cModel* m, int a);
int EmAtkHitCk(void* atk, Vec* pos, Vec* oldPos, int a);
u32 GetWepTargetList(Vec* box, Vec* pos, WepTarget* list, int max, u16 flag);
void BoxWorldCalc(Vec* src, Vec* dst, Vec* pos, Vec* rot);
f32 GetXZAngle(Vec* from, Vec* to);
void obj08AddSpeed(cObj08* obj);
int obj08ScrHitCk(cObj08* obj);
int obj08ToEmHitCk(cObj08* obj);
int obj08ToPlHitCk(cObj08* obj);
void obj08DmEstSet(cObj08* obj, cModel* em, Vec* oldPos, EmHitInfo* part);
}
int MotionSetCore(cModel* m, void* work, void* mot, int a, int b, int c, int d);

Vec obj08HitBox[8] = {
    { -500.0f, -500.0f, 0.0f },   { 500.0f, -500.0f, 0.0f },
    { -500.0f, -500.0f, 500.0f }, { 500.0f, -500.0f, 500.0f },
    { -500.0f, 500.0f, 0.0f },    { 500.0f, 500.0f, 0.0f },
    { -500.0f, 500.0f, 500.0f },  { 500.0f, 500.0f, 500.0f },
};

cObj* SetObj08(cModel* parent, void* bin, void* tpl, Vec* pos, Vec* rot, int flags, void* atk)
{
    cObj* obj;
    Obj08Work* w;

    obj = ObjMgr.create(8);
    if (obj == 0) {
        return 0;
    }
    w = &obj->o8;
    obj->id = 8;
    if (bin == 0) {
        if (obj->modelInit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc),
                           (void*) (pG->pArc->ofs_24 + (u32) pG->pArc)) == 0) {
            ObjMgr.destroy(obj);
            return 0;
        }
        obj->be_flag &= ~2;
    } else {
        if (obj->modelInit(bin, tpl) == 0) {
            ObjMgr.destroy(obj);
            return 0;
        }
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 1000.0f, 1000.0f, 0.0f };

    obj->sub2B4.atari.throughOn();
    obj->lightInfo.init2(0, 1, &p0, &p1, 4);
    w->parent = parent;
    obj->pos = *pos;
    obj->oldPos = *pos;
    obj->rot = *rot;
    w->spd.x = 0.0f;
    w->spd.y = 0.0f;
    w->spd.z = 0.0f;
    w->grav = 0.0f;
    w->flags = 0;
    w->rad = 500.0f;
    w->life = -1;
    w->seBlk = 0xFFFF;
    w->seNo = 0xFFFF;
    w->estFlag = 0;
    if (flags < 0) {
        w->flags = 0x10;
    }
    if (flags & 0x40000000) {
        w->flags |= 0x20;
    }
    w->pAtk = atk;
    w->atkFlags = flags & 0xFFFF;
    return obj;
}

void SetObj08Spd(cObj* obj, Vec* spd, int life, f32 grav, f32 rad)
{
    Obj08Work* w;

    if (obj == 0) {
        return;
    }
    if ((obj->be_flag & 0x201) != 1) {
        return;
    }
    if (obj->id != 8) {
        return;
    }
    w = &obj->o8;
    w->spd = *spd;
    w->grav = grav;
    w->life = life;
    w->rad = rad;
    if (w->rad < 1.0f) {
        w->rad = 1.0f;
    }
}

void SetObj08Est(cObj* obj, int no0, int prm0, int no1, int prm1, int no2, int prm2, int no3, int prm3, u8 flag)
{
    Obj08Work* w;

    if (obj == 0) {
        return;
    }
    if ((obj->be_flag & 0x201) != 1) {
        return;
    }
    if (obj->id != 8) {
        return;
    }
    w = &obj->o8;
    w->estNo[0] = no0;
    w->estNo[1] = no1;
    w->estNo[2] = no2;
    w->estNo[3] = no3;
    w->estPrm[0] = prm0;
    w->estPrm[1] = prm1;
    w->estPrm[2] = prm2;
    w->estPrm[3] = prm3;
    w->estFlag = flag;
}

void SetObj08Se(cObj* obj, u16 blk, u16 no)
{
    Obj08Work* w;

    if (obj == 0) {
        return;
    }
    if ((obj->be_flag & 0x201) != 1) {
        return;
    }
    if (obj->id != 8) {
        return;
    }
    w = &obj->o8;
    w->seBlk = blk;
    w->seNo = no;
}

void cObj08::move()
{
    Obj08Work* w = &o8;

    if (w->life == 0) {
        if (w->estNo[1] && w->estPrm[1]) {
            EstSet(0, -1, &pos, &rot, w->estNo[1], (u8) w->estPrm[1], 0, 0, 0, 0);
        }
        ObjMgr.destroy(this);
        return;
    }
    w->life--;
    if (w->flags & 1) {
        MotionSetCore(this, &pMotion, w->pMot, 0, 0, w->motPrm, 0);
        w->flags = (w->flags & ~1) | 2;
    }
    if (w->flags & 2) {
        MotionMove(this, 0);
    }
    obj08AddSpeed(this);
    obj08ToEmHitCk(this);
    obj08ToPlHitCk(this);
    if (obj08ScrHitCk(this)) {
        return;
    }
    if (w->flags & 8) {
        PSVECAdd(&rot, &w->rotSpd, &rot);
        rot.x = LIMIT_ANGLE(rot.x);
        rot.y = LIMIT_ANGLE(rot.y);
        rot.z = LIMIT_ANGLE(rot.z);
    }
    RotMatrix(mat, &rot);
    TransMatrix(mat, &pos);
    ScaleMatrix(mat, &scale);
    partsMatCalc();
    partsWorldCalc();
}

void obj08AddSpeed(cObj08* obj)
{
    Obj08Work* w = &obj->o8;

    w->spd.y -= w->grav;
    PSVECAdd(&obj->pos, &w->spd, &obj->pos);
}

int obj08ScrHitCk(cObj08* obj)
{
    Obj08Work* w = &obj->o8;
    Vec hit;
    Vec nrm;
    Vec est;

    if (EatMgr.hitCheck(&obj->oldPos, &obj->pos, &hit, &nrm, 0, 0x4000)) {
        if (w->seBlk != 0xFFFF) {
            int id = 0;
            if (w->parent) {
                id = w->parent->id;
            }
            SndCall(w->seBlk, w->seNo, &obj->pos, id, 0, 0);
        }
        if (nrm.y > 0.7f) {
            if (w->estNo[2] && w->estPrm[2]) {
                est.x = 0.0f;
                est.y = obj->rot.y;
                est.z = 0.0f;
                hit.y += 10.0f;
                EstSet(0, -1, &hit, &est, w->estNo[2], (u8) w->estPrm[2], 0, 0, 0, 0);
            }
        } else if (w->estNo[1] && w->estPrm[1]) {
            f32 len = SQRTF(nrm.x * nrm.x + nrm.z * nrm.z);
            est.x = -atan2f(-nrm.y, len);
            est.y = atan2f(-nrm.x, -nrm.z);
            est.z = 0.0f;
            EstSet(0, -1, &hit, &est, w->estNo[1], (u8) w->estPrm[1], 0, 0, 0, 0);
        }
        ObjMgr.destroy(obj);
        return 1;
    }
    return 0;
}

int obj08ToEmHitCk(cObj08* obj)
{
    Obj08Work* w = &obj->o8;
    Vec box[8];
    WepTarget list[10];
    Vec ang;
    f32 len;
    u32 n;
    u32 i;

    if (!(w->flags & 0x10)) {
        return 0;
    }
    if (w->atkFlags == 0) {
        return 0;
    }
    ang.x = 0.0f;
    ang.y = 0.0f;
    ang.z = 0.0f;
    len = GetDistance3(&obj->pos, &obj->oldPos);
    if (len < 1.0f) {
        len = w->rad;
    } else {
        Vec d;
        PSVECSubtract(&obj->pos, &obj->oldPos, &d);
        ang.x = -atan2f(d.y, len);
        ang.y = atan2f(d.x, d.z);
    }
    if (len < w->rad) {
        len = w->rad;
    }
    obj08HitBox[0].x = -w->rad;
    obj08HitBox[1].x = w->rad;
    obj08HitBox[2].x = -w->rad;
    obj08HitBox[3].x = w->rad;
    obj08HitBox[4].x = -w->rad;
    obj08HitBox[5].x = w->rad;
    obj08HitBox[6].x = -w->rad;
    obj08HitBox[7].x = w->rad;
    obj08HitBox[0].y = -w->rad;
    obj08HitBox[1].y = -w->rad;
    obj08HitBox[2].y = -w->rad;
    obj08HitBox[3].y = -w->rad;
    obj08HitBox[4].y = w->rad;
    obj08HitBox[5].y = w->rad;
    obj08HitBox[6].y = w->rad;
    obj08HitBox[7].y = w->rad;
    obj08HitBox[2].z = len;
    obj08HitBox[3].z = len;
    obj08HitBox[6].z = len;
    obj08HitBox[7].z = len;
    BoxWorldCalc(obj08HitBox, box, &obj->oldPos, &ang);
    if (pG->flags_60 & 0x1000) {
        Draw_box(box, 0x20FFFFFF, 0);
    }
    n = GetWepTargetList(box, &obj->pos, list, 3, (u16) w->atkFlags);
    if (n == 0) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        EmHitInfo* part = list[i].part;
        list[i].em->dmg.set(0, 10, (u8) w->atkFlags, &obj->pos, part->rad, part);
        if (w->estNo[3] && w->estPrm[3]) {
            obj08DmEstSet(obj, pPL, &obj->oldPos, part);
        }
    }
    w->flags &= ~0x10;
    return 1;
}

int obj08ToPlHitCk(cObj08* obj)
{
    Obj08Work* w = &obj->o8;
    int hit;

    if (w->parent == 0) {
        return 0;
    }
    if (w->pAtk && (w->flags & 0x20)) {
        hit = EmAtkHitCk(w->pAtk, &obj->pos, &obj->oldPos, 0);
        if (hit) {
            if (w->estNo[3] && w->estPrm[3]) {
                if (hit & 1) {
                    obj08DmEstSet(obj, pPL, &obj->oldPos, &pPL->hitInfo);
                }
                if (hit & 2) {
                    if (pSUB) {
                        obj08DmEstSet(obj, pSUB, &obj->oldPos, &((cEm*) pSUB)->hitInfo);
                    }
                }
            } else {
                if (w->seBlk != 0xFFFF) {
                    int id = 0;
                    if (w->parent) {
                        id = w->parent->id;
                    }
                    SndCall(w->seBlk, w->seNo, &obj->pos, id, 0, 0);
                }
                VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            }
            w->flags &= ~0x20;
            return 1;
        }
    }
    return 0;
}

void obj08DmEstSet(cObj08* obj, cModel* em, Vec* oldPos, EmHitInfo* part)
{
    Obj08Work* w = &obj->o8;
    Mtx m;
    Vec p;
    Vec o;
    Vec rot;
    f32 dy;
    f32 lim;

    if (w->seBlk != 0xFFFF) {
        int id = 0;
        if (w->parent) {
            id = w->parent->id;
        }
        SndCall(w->seBlk, w->seNo, &obj->pos, id, 0, 0);
    }
    if (w->estFlag) {
        EstSet((int) em, -1, 0, 0, w->estNo[3], (u8) w->estPrm[3], 0, 0, (u32) em, 0);
        return;
    }
    if (part->partsNo != 0) {
        p = em->getPartsPtr(part->partsNo - 1)->worldPos;
    } else {
        p = em->pos;
    }
    dy = oldPos->y - p.y;
    lim = part->height * 0.7f;
    if (dy > lim) {
        dy = lim;
    }
    if (dy < -lim) {
        dy = -lim;
    }
    rot.x = 0.0f;
    rot.y = GetXZAngle(&p, oldPos);
    rot.z = 0.0f;
    RotMatrix(m, &rot);
    TransMatrix(m, &p);
    o.x = 0.0f;
    o.y = dy;
    o.z = part->width * 0.5f;
    PSMTXMultVec(m, &o, &o);
    EstSet(0, -1, &o, &rot, w->estNo[3], (u8) w->estPrm[3], 0, 0, 0, 0);
}
