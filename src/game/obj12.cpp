#include "atari.h"
#include "obj.h"
#include "emhit.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"
#include "rnd.h"
#include "pad.h"

// Hanging object that can be thrown and falls as a three-point rope (obj00 variant with a rope
// type, a life counter and a throw routine).
class cObj12 : public cObj {
public:
    virtual void move();
    virtual ~cObj12() {}

    void setParent(cModel* oya, int partsNo, int noNormalize);
    void chainMove();
    void setFall(Vec* spd, u8 type);
    void setFallSe(u8 blk, u8 no, u8 id);
    void fallMove();
    void throwMove();
    void setBurn();
};

// One point of the falling rope (fallMove).
struct Obj12Node {
    Vec pos;
    Vec old;
    Vec spd;
    f32 len;
    int hit;
};

extern "C" {
int MotionMove(cModel* m, int a);
int EmAtkHitCk(void* atk, Vec* pos, Vec* oldPos, int a);
}

void cObj12::move()
{
    Obj12Work* w = &o12;
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    Vec p;
    Quaternion q0;
    Quaternion q1;
    Quaternion q;

    w->motResult = 0;
    if (pMotion) {
        w->motResult = MotionMove(this, 0);
    }
    if (!(w->flags & 0x106)) {
        RotMatrix(worldMat, &rot);
        TransMatrix(worldMat, &pos);
        ScaleMatrix(worldMat, &scale);
        PSMTXCopy(worldMat, mat);
    }
    if (w->oya) {
        if ((w->oya->be_flag & 0x201) != 1) {
            ObjMgr.destroy(this);
            return;
        }
        if (w->oya->pParts) {
            PSMTXConcat(w->oya->getPartsPtr(w->partsNo)->mat, mat, m);
            if (!(w->flags & 0x80)) {
                v0.x = m[0][0];
                v0.y = m[1][0];
                v0.z = m[2][0];
                v1.x = m[0][1];
                v1.y = m[1][1];
                v1.z = m[2][1];
                v2.x = m[0][2];
                v2.y = m[1][2];
                v2.z = m[2][2];
                if (v0.x == 0.0f && v0.y == 0.0f && v0.z == 0.0f) {
                    v0.x = 1.0f;
                }
#line 102 "D:/Bio4/Prog/obj12.cpp"
                VECNormalize(&v0, &v0);
                if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                    v1.y = 1.0f;
                }
#line 104 "D:/Bio4/Prog/obj12.cpp"
                VECNormalize(&v1, &v1);
                if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                    v2.z = 1.0f;
                }
#line 106 "D:/Bio4/Prog/obj12.cpp"
                VECNormalize(&v2, &v2);
                m[0][0] = v0.x;
                m[1][0] = v0.y;
                m[2][0] = v0.z;
                m[0][1] = v1.x;
                m[1][1] = v1.y;
                m[2][1] = v1.z;
                m[0][2] = v2.x;
                m[1][2] = v2.y;
                m[2][2] = v2.z;
            }
            if (w->rate < 1.0f) {
                w->rate += w->rateSpd;
                if (w->rate >= 1.0f) {
                    w->rate = 1.0f;
                    w->flags &= ~8;
                }
            }
            if (w->flags & 8) {
                f32 rate = w->rate;
                f32 inv = 1.0f - rate;

                p.x = m[0][3] * rate + w->mat[0][3] * inv;
                p.y = m[1][3] * rate + w->mat[1][3] * inv;
                p.z = m[2][3] * rate + w->mat[2][3] * inv;
                C_QUATMtx(&q0, m);
                C_QUATMtx(&q1, w->mat);
                C_QUATSlerp(&q0, &q1, &q, w->rate);
                PSMTXQuat(mat, &q);
                TransMatrix(mat, &p);
                PSMTXCopy(mat, w->mat);
            } else {
                PSMTXCopy(m, mat);
            }
        }
        if (w->oya) {
            if (w->oya->lightInfo.x50 & 2) {
                lightInfo.x50 &= ~0x10;
                lightInfo.x50 |= 2;
            }
        }
    }
    throwMove();
    fallMove();
    if ((be_flag & 0x201) == 1) {
        if (!(w->flags & 6)) {
            partsMatCalc();
        }
        partsWorldCalc();
        chainMove();
        if (w->oya) {
            alpha = w->oya->alpha;
            x158 = w->oya->x158;
            if (w->oya->be_flag & 2) {
                be_flag |= 2;
            } else {
                be_flag &= ~2;
            }
        }
        if (G_ROOM_ID == 0x30F && (w->flags & 4)) {
            ObjMgr.destroy(this);
            return;
        }
        if (w->flags & 0x200) {
            if (w->life) {
                w->life--;
            } else {
                alpha -= 0.1f;
                if (alpha < 0.0f) {
                    alpha = 0.0f;
                    be_flag &= ~2;
                    ObjMgr.destroy(this);
                }
            }
        }
    }
}

cObj* SetObj12(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    Obj12Work* w;

    obj = ObjMgr.createBack(0x12);
    if (obj) {
        w = &obj->o12;
        if (obj->modelInit(bin, tpl) == 0) {
            pLog->err(0, 0, "SetObj12() modelInit() failed.");
            ObjMgr.destroy(obj);
            return 0;
        } else {
            static const Vec p0 = { 0.0f, 0.0f, 0.0f };
            static const Vec p1 = { 500.0f, 500.0f, 500.0f };

            obj->sub2B4.atari.throughOn();
            obj->lightInfo.init2(0, 1, &p0, &p1, 0x10);
            obj->pos = *pos;
            obj->oldPos = *pos;
            obj->rot = *rot;
            w->rate = 1.0f;
            w->rateSpd = 0.0f;
            w->oya = 0;
            w->partsNo = 0;
            w->motResult = 0;
            w->life = 0;
            return obj;
        }
    }
    return 0;
}

void cObj12::setParent(cModel* oya, int partsNo, int noNormalize)
{
    Obj12Work* w = &o12;

    w->oya = oya;
    w->partsNo = partsNo;
    w->flags &= ~8;
    w->flags &= ~3;
    if (noNormalize) {
        w->flags |= 0x80;
    } else {
        w->flags &= ~0x80;
    }
}

void cObj12::chainMove()
{
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
}

// Never called (dead-stripped by the original linker, STRIP_UNUSED): its constant pool survives
// (double 0.0, the u32 -> f32 magic, 1.0).
static void obj12SetRate(cObj* obj, u32 rate)
{
    Obj12Work* w = &obj->o12;

    if (w->rate == 0.0) {
        return;
    }
    w->rateSpd = (f32) rate;
    if (w->rateSpd < 1.0f) {
        w->rateSpd = 1.0f;
    }
}

void cObj12::setFall(Vec* spd, u8 type)
{
    Obj12Work* w = &o12;
    u32 i;
    f32 r;

    w->flags |= 4;
    w->oya = 0;
    for (i = 0; i < 3; i++) {
        if (spd) {
            if (i == 0) {
                r = fRand0_1();
                w->fallSpd[i][0] = (s16) ((spd->x * 0.5f + spd->x * r) * 10.0f);
                r = fRand0_1();
                w->fallSpd[i][1] = (s16) ((spd->y * 0.5f + spd->y * r) * 10.0f);
                r = fRand0_1();
                w->fallSpd[i][2] = (s16) ((spd->z * 0.5f + spd->z * r) * 10.0f);
            } else {
                r = fRand0_1();
                w->fallSpd[i][0] = (s16) ((spd->x * 0.5f + spd->x * r * 2.0f) * 10.0f);
                r = fRand0_1();
                w->fallSpd[i][1] = (s16) ((spd->y * 0.5f + spd->y * r * 2.0f) * 10.0f);
                r = fRand0_1();
                w->fallSpd[i][2] = (s16) ((spd->z * 0.5f + spd->z * r * 2.0f) * 10.0f);
            }
        } else {
            w->fallSpd[i][0] = (s16) (fRand1_1() * 100.0f);
            w->fallSpd[i][1] = (s16) (fRand1_1() * 100.0f) + 500;
            w->fallSpd[i][2] = (s16) (fRand1_1() * 100.0f);
        }
    }
    w->flags |= 0x200;
    w->seBlk = 0xFF;
    w->seId = 0;
    w->sePlayed = 0;
    w->type = type;
    w->life = 90;
    w->seNo = 0xFF;
}

void cObj12::setFallSe(u8 blk, u8 no, u8 id)
{
    Obj12Work* w = &o12;

    w->seBlk = blk;
    w->seNo = no;
    w->seId = id;
    w->sePlayed = 0;
}

void cObj12::fallMove()
{
    Obj12Work* w = &o12;
    Vec ofs[5][3] = {
        { { 0.0f, 0.0f, 600.0f }, { 0.0f, 0.0f, -600.0f }, { 300.0f, 0.0f, 0.0f } },
        { { 0.0f, 0.0f, 1500.0f }, { 0.0f, 0.0f, 0.0f }, { 300.0f, 0.0f, 1300.0f } },
        { { -140.0f, 60.0f, 140.0f }, { -140.0f, 60.0f, -140.0f }, { 200.0f, 60.0f, 0.0f } },
        { { -140.0f, 30.0f, 140.0f }, { -140.0f, 30.0f, -140.0f }, { 200.0f, 30.0f, 0.0f } },
        { { 140.0f, -140.0f, 0.0f }, { -140.0f, -140.0f, 0.0f }, { 0.0f, 200.0f, 0.0f } },
    };
    Obj12Node node[3];
    Vec vx;
    Vec vy;
    Vec vz;
    Vec d;
    u32 i;
    u32 k;
    Obj12Node* p;
    Obj12Node* n;
    f32 mag;
    f32 diff;
    f32 floor;

    if (!(w->flags & 4)) {
        return;
    }
    floor = EatMgr.getFloor(&pos, 600.0f, 100000.0f, 0, 0) + 50.0f;
    for (i = 0; i < 3; i++) {
        p = &node[i];
        p->spd.x = (f32) w->fallSpd[i][0] * 0.1f;
        p->spd.y = (f32) w->fallSpd[i][1] * 0.1f;
        p->spd.z = (f32) w->fallSpd[i][2] * 0.1f;
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        PSMTXMultVec(mat, &ofs[w->type][i], &p->pos);
        p->old = p->pos;
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        if (i == 2) {
            n = node;
        } else {
            n = &node[i + 1];
        }
        p->len = GetDistance3(&p->pos, &n->pos);
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        p->spd.y -= 20.0f;
        PSVECAdd(&p->pos, &p->spd, &p->pos);
        p->hit = 0;
    }
    for (k = 0; k < 30; k++) {
        for (i = 0; i < 3; i++) {
            p = &node[i];
            if (i == 2) {
                n = node;
            } else {
                n = &node[i + 1];
            }
            PSVECSubtract(&n->pos, &p->pos, &d);
            mag = PSVECMag(&d);
            diff = (p->len - mag) * 0.5f;
            PSVECScale(&d, &d, (1.0f / mag) * diff);
            PSVECAdd(&n->pos, &d, &n->pos);
            PSVECSubtract(&p->pos, &d, &p->pos);
            if (p->pos.y < floor) {
                p->pos.y = floor;
                p->hit = 1;
            }
            if (n->pos.y < floor) {
                n->pos.y = floor;
                n->hit = 1;
            }
        }
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        // dead in this loop, but its `i == 2` compare makes loop.c compute the `&node[2]`
        // bound in the preheader, where cse2 copies it from the k loop's final giv value
        if (i == 2) {
            n = node;
        } else {
            n = &node[i + 1];
        }
        if (p->hit) {
            if (w->sePlayed == 0 && p->spd.y < -50.0f) {
                w->sePlayed = 1;
                if (w->seBlk != 0xFF) {
                    SndCall(w->seBlk, w->seNo, &pos, w->seId, 0, 0);
                }
            }
            switch (w->type) {
            default:
                p->spd.x *= fRand0_1() * 0.2f + 0.5f;
                p->spd.y *= -(fRand0_1() * 0.2f + 0.5f);
                p->spd.z *= fRand0_1() * 0.2f + 0.5f;
                break;
            case 2:
            case 3:
                p->spd.x *= fRand0_1() * 0.2f + 0.4f;
                p->spd.y *= -(fRand0_1() * 0.1f + 0.3f);
                p->spd.z *= fRand0_1() * 0.2f + 0.4f;
                break;
            }
            if (p->spd.y <= 20.0f && p->spd.y > 0.0f) {
                p->spd.y = 0.0f;
            }
        } else {
            PSVECSubtract(&p->pos, &p->old, &p->spd);
        }
        PSVECScale(&p->spd, &p->spd, 0.999f);
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        w->fallSpd[i][0] = (s16) (p->spd.x * 10.0f);
        w->fallSpd[i][1] = (s16) (p->spd.y * 10.0f);
        w->fallSpd[i][2] = (s16) (p->spd.z * 10.0f);
    }
    if (w->type != 4) {
        PSVECSubtract(&node[0].pos, &node[1].pos, &vz);
        PSVECSubtract(&node[2].pos, &node[1].pos, &vx);
        PSVECCrossProduct(&vz, &vx, &vy);
        PSVECCrossProduct(&vy, &vz, &vx);
#line 1039 "D:/Bio4/Prog/obj12.cpp"
        VECNormalize(&vx, &vx);
        VECNormalize(&vy, &vy);
        VECNormalize(&vz, &vz);
    } else {
        PSVECSubtract(&node[0].pos, &node[1].pos, &vx);
        PSVECSubtract(&node[2].pos, &node[1].pos, &vy);
        PSVECCrossProduct(&vx, &vy, &vz);
        PSVECCrossProduct(&vz, &vx, &vy);
#line 1048 "D:/Bio4/Prog/obj12.cpp"
        VECNormalize(&vx, &vx);
        VECNormalize(&vy, &vy);
        VECNormalize(&vz, &vz);
    }
    mat[0][0] = vx.x;
    mat[1][0] = vx.y;
    mat[2][0] = vx.z;
    mat[0][1] = vy.x;
    mat[1][1] = vy.y;
    mat[2][1] = vy.z;
    mat[0][2] = vz.x;
    mat[1][2] = vz.y;
    mat[2][2] = vz.z;
    PSVECScale(&ofs[w->type][0], &d, -1.0f);
    TransMatrix(mat, &node[0].pos);
    PSMTXMultVec(mat, &d, &d);
    TransMatrix(mat, &d);
    pos = d;
    mag = node[0].spd.x * node[0].spd.x + node[0].spd.y * node[0].spd.y + node[0].spd.z * node[0].spd.z +
          node[1].spd.x * node[1].spd.x + node[1].spd.y * node[1].spd.y + node[1].spd.z * node[1].spd.z +
          node[2].spd.x * node[2].spd.x + node[2].spd.y * node[2].spd.y + node[2].spd.z * node[2].spd.z;
    if (mag < 25.0f) {
        pos.x = mat[0][3];
        pos.y = mat[1][3];
        pos.z = mat[2][3];
        Matrix2AxisAngle(mat, &rot);
        w->flags &= ~4;
    }
}

// Never called (dead-stripped, STRIP_UNUSED): constant pool only (10, 75, 350, 0.0, pi/2).
static void obj12ThrowSet(cObj* obj, Vec* spd)
{
    Obj12Work* w = &obj->o12;
    f32 ang;

    w->fallSpd[0][0] = (s16) (spd->x * 10.0f);
    w->fallSpd[0][1] = (s16) (spd->y * 75.0f);
    w->fallSpd[0][2] = (s16) (spd->z * 350.0f);
    ang = atan2f(spd->x, spd->z);
    if (ang < 0.0f) {
        ang += 1.5707964f;
    }
    obj->rot.y = ang;
}

void cObj12::throwMove()
{
    Obj12Work* w = &o12;
    Vec spd;
    Mtx m;
    Vec up;
    Vec dir;
    f32 ang;

    if (!(w->flags & 0x100)) {
        return;
    }
    static EmAtkInfo obj12Atk = { 300.0f, 8, 400, 0, 10, 0 };

    w->fallSpd[0][1] -= 15;
    spd.x = (f32) w->fallSpd[0][0];
    spd.y = (f32) w->fallSpd[0][1];
    spd.z = (f32) w->fallSpd[0][2];
    PSVECAdd(&pos, &spd, &pos);
    if (EatMgr.hitCheck(&oldPos, &pos, 0, 0, 0, 0)) {
        w->flags &= ~0x100;
        setFall(0, 0);
    } else if (EmAtkHitCk(&obj12Atk, &pos, &oldPos, 1)) {
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        w->flags &= ~0x100;
        setFall(0, 0);
    }
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    PSMTXRotRad(m, 'y', rot.y);
    dir.x = 0.0f;
    dir.y = 0.0f;
    dir.z = 1.0f;
    PSMTXMultVecSR(m, &dir, &dir);
    if (dir.x == 0.0f) {
        dir.y = 0.0f;
    }
#line 1195 "D:/Bio4/Prog/obj12.cpp"
    VECNormalize(&dir, &dir);
    ang = acosf(PSVECDotProduct(&up, &dir));
    if (ang > 0.01f && ang < 3.1315927f) {
        PSVECCrossProduct(&up, &dir, &up);
        PSMTXRotAxisRad(m, &up, 0.62831855f);
        PSMTXConcat(m, mat, mat);
    }
    TransMatrix(mat, &pos);
}

void cObj12::setBurn()
{
    cModelInfo* info;

    for (info = pInfo; info; info = info->pNext) {
        info->color[0] = 0x20;
        info->color[1] = 0x20;
        info->color[2] = 0x20;
    }
}
