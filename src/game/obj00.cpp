#include "atari.h"
#include "atari_init.h"
#include "obj.h"
#include "global.h"
#include "math_sub.h"
#include "snd.h"

// Hanging object (lamp, sign, ...): follows a parts of its parent with a slerp blend, falls as a
// three-point rope when cut, fades out when flagged.
class cObj00 : public cObj {
public:
    virtual void move();
    virtual ~cObj00() {}

    void setScrAtari(f32 r);
};

// One point of the falling rope (obj00FallMove).
struct Obj00Node {
    Vec pos;
    Vec old;
    Vec spd;
    f32 len;
    int hit;
};

extern "C" {
int MotionMove(cModel* m, int a);
void obj00FallMove(cObj00* obj);
void obj00SetOya(cObj00* obj);
}
int MotionSetCore(cModel* m, void* work, void* mot, int a, int b, int c, int d);

void cObj00::move()
{
    Obj00Work* w = &o0;

    if (pMotion) {
        MotionMove(this, 0);
    } else if (!(w->flags & 0x16)) {
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
    }
    obj00SetOya(this);
    obj00FallMove(this);
    if (pMotion == 0) {
        if (!(w->flags & 0x16)) {
            partsMatCalc();
        }
    }
    partsWorldCalc();
    sub2B4.atari.move();
    SatMgr.check(this, 0);
    if (w->flags & 0x20) {
        alpha -= 0.1f;
        if (alpha < 0.0f) {
            alpha = 0.0f;
            be_flag &= ~2;
        }
    }
}

cObj* SetObj00(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cObj* obj;
    Obj00Work* w;

    obj = ObjMgr.create(0);
    if (obj == 0) {
        return 0;
    }
    w = &obj->o0;
    if (obj->modelInit(bin, tpl) == 0) {
        ObjMgr.destroy(obj);
        return 0;
    }
    static const Vec p0 = { 0.0f, 0.0f, 0.0f };
    static const Vec p1 = { 3000.0f, 3000.0f, 0.0f };

    obj->sub2B4.atari.throughOn();
    obj->lightInfo.init2(0, 1, &p0, &p1, 0x10);
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
    w->oya = 0;
    w->partsNo = 0;
    w->rate = 1.0f;
    w->rateSpd = 0.0f;
    return obj;
}

void MotSetObj00(cObj* obj, void* mot, int prm, int a)
{
    Obj00Work* w = &obj->o0;

    if (obj == 0) {
        return;
    }
    w->pMot = mot;
    w->motPrm = prm;
    w->motA = a;
    MotionSetCore(obj, &obj->pMotion, mot, a, 0, (u16) w->motPrm, 0);
}

void OyaSetObj00(cObj* obj, cModel* oya, int partsNo)
{
    Obj00Work* w = &obj->o0;

    if (obj == 0) {
        return;
    }
    w->oya = oya;
    w->partsNo = partsNo;
    obj->pMotion = 0;
    w->flags &= ~8;
}

// Never called: the original linker dropped the body but kept its constant pool.
static void obj00SetRate(cObj* obj, u32 rate)
{
    f32 r = (f32) rate;

    if (r < 1.0f) {
        r = 1.0f;
    }
    obj->o0.rateSpd = r / 100.0f;
}

void obj00FallMove(cObj00* obj)
{
    Obj00Work* w = &obj->o0;
    Vec ofs[3] = { { 0.0f, 0.0f, 300.0f }, { 0.0f, 0.0f, -300.0f }, { 300.0f, 0.0f, 0.0f } };
    Obj00Node node[3];
    Vec vx;
    Vec vy;
    Vec vz;
    Vec d;
    u32 i;
    u32 k;
    Obj00Node* p;
    Obj00Node* n;
    f32 mag;
    f32 diff;

    if (!(w->flags & 4)) {
        return;
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        p->spd.x = (f32) w->fallSpd[i][0] * 0.1f;
        p->spd.y = (f32) w->fallSpd[i][1] * 0.1f;
        p->spd.z = (f32) w->fallSpd[i][2] * 0.1f;
    }
    for (i = 0; i < 3; i++) {
        p = &node[i];
        PSMTXMultVec(obj->mat, &ofs[i], &p->pos);
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
            if (p->pos.y < 30.0f) {
                p->pos.y = 30.0f;
                p->hit = 1;
            }
            if (n->pos.y < 30.0f) {
                n->pos.y = 30.0f;
                n->hit = 1;
            }
        }
    }
    {
    // COMPILER-DIFF: candidate #17 (global.c pass 0 regs_used_so_far): a codeless call-crossing
    // pseudo (3 refs, ranked between the hit-loop `end` and the hoisted `sePlayed = 1` constant)
    // occupies r24 across the hit loop so the constant takes r23 like the original; the four dead
    // `i` sets keep the gcse bucket count (spill-slot order of the PRE'd w+32/34/36, fp+136).
    int junk;
    asm("" : "=r"(junk) : "m"(node[0].hit));
    i = 5; i = 6; i = 7; i = 8;
    for (i = 0; i < 3; i++) {
        p = &node[i];
        if (p->hit) {
            p->spd.x *= 0.8f;
            p->spd.y *= -0.8f;
            p->spd.z *= 0.8f;
            if (w->sePlayed == 0) {
                w->sePlayed = 1;
                SndCall(w->seBlk, w->seNo, &obj->pos, w->seId, 0, 0);
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
        asm("" : "=m"(pG) : "r"(junk));
    }
    }
    PSVECSubtract(&node[0].pos, &node[1].pos, &vz);
    PSVECSubtract(&node[2].pos, &node[1].pos, &vx);
    PSVECCrossProduct(&vz, &vx, &vy);
    PSVECCrossProduct(&vy, &vz, &vx);
#line 539 "D:/Bio4/Prog/obj00.cpp"
    VECNormalize(&vx, &vx);
    VECNormalize(&vy, &vy);
    VECNormalize(&vz, &vz);
    obj->mat[0][0] = vx.x;
    obj->mat[1][0] = vx.y;
    obj->mat[2][0] = vx.z;
    obj->mat[0][1] = vy.x;
    obj->mat[1][1] = vy.y;
    obj->mat[2][1] = vy.z;
    obj->mat[0][2] = vz.x;
    obj->mat[1][2] = vz.y;
    obj->mat[2][2] = vz.z;
    PSVECAdd(&node[0].pos, &node[1].pos, &d);
    PSVECScale(&d, &d, 0.5f);
    TransMatrix(obj->mat, &d);
    obj->pos = d;
}

void obj00SetOya(cObj00* obj)
{
    Obj00Work* w = &obj->o0;
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    Vec p;
    Quaternion q0;
    Quaternion q1;
    Quaternion q;

    if (w->oya == 0) {
        return;
    }
    if (w->oya->pParts == 0) {
        return;
    }
    PSMTXCopy(w->oya->getPartsPtr(w->partsNo)->mat, m);
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
#line 597 "D:/Bio4/Prog/obj00.cpp"
    VECNormalize(&v0, &v0);
    if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
        v1.y = 1.0f;
    }
#line 599 "D:/Bio4/Prog/obj00.cpp"
    VECNormalize(&v1, &v1);
    if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
        v2.z = 1.0f;
    }
#line 601 "D:/Bio4/Prog/obj00.cpp"
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
    PSMTXConcat(m, obj->mat, m);
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
        PSMTXQuat(obj->mat, &q);
        TransMatrix(obj->mat, &p);
        PSMTXCopy(obj->mat, w->mat);
    } else {
        PSMTXCopy(m, obj->mat);
    }
    if (w->oya) {
        if (w->oya->lightInfo.x50 & 2) {
            obj->lightInfo.x50 &= ~0x10;
            obj->lightInfo.x50 |= 2;
        }
    }
}

void cObj00::setScrAtari(f32 r)
{
    atariInitF(&sub2B4.atari, 0.0f, 0.0f, 0.0f, r, r, r * 0.8f, r, 1, 0x2000, 10);
    sub2B4.atari.scrOn();
}
