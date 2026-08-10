// game/pl_push.cpp: player push-object control (catch a pushable enemy, push it, keep it on the scenario).

#include "pl_push.h"
#include "global.h"
#include "atari.h"
#include "db_log.h"
#include "math_sub.h"

int MotionSetCore(cModel* m, void* work, void* data, int a, int b, int c, int d);  // game/motion.cpp
extern "C" {
void MotionMove(cModel* m, int flag);                                    // game/motion.cpp
void AddSpeed(cModel* m, const Vec* speed);                              // game/sub2.cpp
int At_em_rect_rect_ck(cModel* pl, cEm* em);                            // game/at_mod.cpp
void EmAtCheck(cEm* em);                                                // game/at_mod.cpp
int GetWepTargetPos(Vec* a, Vec* b, int c, int d, int e, int f);        // game/em_sub.cpp
}

int cPlPush::catchCheck()
{
    cPlayer* pl = pPl;
    Vec bak;
    Vec pos;
    Vec rot;
    static const Vec sp = {0.0f, 0.0f, 300.0f};
    u32 i;

    bak = pl->pos;
    AddSpeed(pl, &sp);
    pTarget = 0;
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if ((em->be_flag & 0x201) != 1) {
            continue;
        }
        if (em->id != 0x45) {
            continue;
        }
        if (em->type == 4) {
            continue;
        }
        if (em->hp <= 0) {
            continue;
        }
        if (fabsf(pPl->pos.y - em->pos.y) > 500.0f) {
            continue;
        }
        pos.x = em->pos.x;
        pos.y = em->pos.y + 300.0f;
        pos.z = em->pos.z;
        if (SatMgr.hitCheck(&pPl->getPartsPtr(0)->worldPos, &pos, 0, 0, 0, 0x800) != 0) {
            continue;
        }
        if (At_em_rect_rect_ck(pPl, em)) {
            pTarget = em;
            break;
        }
    }
    pl->pos = bak;
    if (pTarget == 0) {
        return 0;
    }

    PSVECSubtract(&pl->pos, &pTarget->pos, &pos);
    rot.x = 0.0f;
    rot.y = -pTarget->rot.y;
    rot.z = 0.0f;
    RotVector(&pos, &rot, &pos);
    {
        f32 sx = pTarget->atari.rectX;
        f32 sz = pTarget->atari.rectZ;

        dir = 4;
        if (pos.z < sz && pos.z > -sz) {
            if (pos.x > sx) {
                dir = 3;
            } else if (pos.x < -sx) {
                dir = 1;
            }
        }
        if (pos.x < sx && pos.x > -sx) {
            if (pos.z > sz) {
                dir = 0;
            } else if (pos.z < -sz) {
                dir = 2;
            }
        }
    }
    if (dir == 4) {
        return 0;
    }
    x8 = 0;
    return 1;
}

void cPlPush::pushTargetInit(u8 flag)
{
    PlArc* arc = pG->pPlArc;

    MotionSetCore(pTarget, &pTarget->pMotion, PL_ARC_PTR(arc, 0x59), 0, 0, 5, 0);
    x9 = flag;
}

int cPlPush::pushTarget()
{
    int ret;
    cModel* t;

    switch (dir) {
    case 0:
        pTarget->rot.y += PI;
        break;
    case 1:
        pTarget->rot.y += PI * 0.5f;
        break;
    case 3:
        pTarget->rot.y += PI * 1.5f;
        break;
    case 2:
    case 4:
        break;
    }
    pTarget->rot.y = LIMIT_ANGLE(pTarget->rot.y);
    RotMatrix(pTarget->mat, &pTarget->rot);
    MotionMove(pTarget, 0);
    switch (dir) {
    case 0:
        pTarget->rot.y -= PI;
        break;
    case 1:
        pTarget->rot.y -= PI * 0.5f;
        break;
    case 3:
        pTarget->rot.y -= PI * 1.5f;
        break;
    case 2:
    case 4:
        break;
    }
    ret = 0;
    pTarget->rot.y = LIMIT_ANGLE(pTarget->rot.y);
    if (((cEmRack*) pTarget)->adjustRange(dir)) {
        ret = 1;
    }
    EmAtCheck(pTarget);
    if (scrHitCheck()) {
        ret = 1;
    }
    t = pTarget;
    RotMatrix(t->worldMat, &t->rot);
    TransMatrix(t->worldMat, &t->pos);
    ScaleMatrix(t->worldMat, &t->scale);
    PSMTXCopy(t->worldMat, t->mat);
    pTarget->partsWorldCalc();
    if (ret == 1) {
        pTarget->pMotion = 0;
    }
    return ret;
}

void cPlPush::stopTarget()
{
    pTarget->pMotion = 0;
}

void cPlPush::getWHY(f32* w, f32* h, f32* y)
{
    f32 sz = pTarget->atari.rectZ;
    f32 sx = pTarget->atari.rectX;
    u8 d;

    if (x9 & 1) {
        switch (dir) {
        default:
            pLog->err(0, 0, "cPlPush::getWHY() DIR ERR %d", dir);
        case 0:
            d = 2;
            break;
        case 1:
            d = 3;
            break;
        case 2:
            d = 0;
            break;
        case 3:
            d = 1;
            break;
        }
    } else {
        d = dir;
    }
    switch (d) {
    case 0:
        *w = sx;
        *h = sz;
        *y = pTarget->rot.y + PI;
        break;
    case 1:
        *w = sz;
        *h = sx;
        *y = pTarget->rot.y + PI * 0.5f;
        break;
    case 2:
        *w = sx;
        *h = sz;
        *y = pTarget->rot.y;
        break;
    case 3:
        *w = sz;
        *h = sx;
        *y = pTarget->rot.y + PI * 1.5f;
        break;
    }
    *y = LIMIT_ANGLE(*y);
}

int cPlPush::scrHitCheck()
{
    f32 w;
    f32 h;
    f32 y;
    int ret;

    getWHY(&w, &h, &y);
    if (emSandCheck(&pTarget->pos, w, h, y)) {
        return 1;
    }
    ret = 0;
    if (scrHitCheckSub(&pTarget->pos, w, h, y, 1.0f)) {
        ret = 1;
    }
    if (scrHitCheckSub(&pTarget->pos, w, h, y, -1.0f)) {
        ret = 1;
    }
    return ret;
}

int cPlPush::scrHitCheckSub(Vec* pos, f32 w, f32 h, f32 y, f32 side)
{
    Vec v0;
    Vec v1;
    Vec v2;
    Vec hit;
    Vec rot;
    int ret;

    rot.x = 0.0f;
    rot.y = y;
    rot.z = 0.0f;
    v0.x = -side * w;
    v0.y = 100.0f;
    v0.z = h;
    RotVector(&v0, &rot, &v0);
    PSVECAdd(&v0, pos, &v0);
    v1.x = side * w * 2.0f;
    v1.y = 0.0f;
    v1.z = 0.0f;
    RotVector(&v1, &rot, &v1);
    PSVECAdd(&v1, &v0, &v1);
    ret = SatMgr.hitCheck(&v0, &v1, &hit, 0, 0, 0x800);
    v2.x = 0.0f;
    v2.y = 0.0f;
    v2.z = h * 2.0f;
    RotVector(&v2, &rot, &v2);
    if (ret & 0x1000000) {
        PSVECSubtract(&v1, &v2, &v0);
        if (SatMgr.hitCheck(&v0, &v1, &hit, 0, 0, 0x800)) {
            PSVECSubtract(&hit, &v1, &v0);
            PSVECAdd(pos, &v0, pos);
            return 1;
        }
    }
    PSVECSubtract(&hit, &v2, &v2);
    if (SatMgr.hitCheck(&v2, &hit, &v1, 0, 0, 0x800)) {
        PSVECSubtract(&v1, &hit, &v2);
        PSVECAdd(pos, &v2, pos);
        return 1;
    }
    return 0;
}

// `const f32` locals: each takes a 4-byte frame slot and creates its pool entry at the declaration
// (800 before 300 before the 0.0 of rot.x) without emitting code, so `h + sand` is computed where it
// is used (after the first two calls) with only the `lis` hoisted into a callee-saved register.
int cPlPush::emSandCheck(Vec* pos, f32 w, f32 h, f32 y)
{
    Vec v0;
    Vec v1;
    Vec rot;
    const f32 sand = 800.0f;
    const f32 base = 300.0f;
    f32 len;

    rot.x = 0.0f;
    rot.y = y;
    rot.z = 0.0f;
    v0.x = w;
    v0.y = base;
    v0.z = 0.0f;
    RotVector(&v0, &rot, &v0);
    PSVECAdd(&v0, pos, &v0);
    v1.x = 0.0f;
    v1.y = 0.0f;
    v1.z = h + sand;
    RotVector(&v1, &rot, &v1);
    PSVECAdd(&v1, &v0, &v1);
    if (SatMgr.hitCheck(&v0, &v1, 0, 0, 0, 0) == 0) {
        v0.x = -w;
        v0.y = base;
        v0.z = 0.0f;
        RotVector(&v0, &rot, &v0);
        PSVECAdd(&v0, pos, &v0);
        v1.x = 0.0f;
        v1.y = 0.0f;
        v1.z = h + sand;
        RotVector(&v1, &rot, &v1);
        PSVECAdd(&v1, &v0, &v1);
        if (SatMgr.hitCheck(&v0, &v1, 0, 0, 0, 0) == 0) {
            return 0;
        }
    }
    len = h + 400.0f;
    v0.x = w;
    v0.y = 300.0f;
    v0.z = len;
    RotVector(&v0, &rot, &v0);
    PSVECAdd(&v0, pos, &v0);
    v1.x = -w;
    v1.y = 300.0f;
    v1.z = len;
    RotVector(&v1, &rot, &v1);
    PSVECAdd(&v1, pos, &v1);
    return GetWepTargetPos(&v0, &v1, 0, 0, 0, 0) == 2;
}

int cPlPush::plAdjust()
{
    cEm* t = pTarget;
    f32 ang;

    if (t == 0) {
        return 0;
    }
    switch (dir) {
    case 0:
        ang = t->rot.y + PI;
        break;
    case 1:
        ang = t->rot.y + PI * 0.5f;
        break;
    case 2:
        ang = t->rot.y;
        break;
    case 3:
        ang = t->rot.y + PI * 1.5f;
        break;
    default:
        pLog->err(0, 0, "cPlPush::plAdjust() DIR ERR %d", dir);
        return 0;
    }
    pPl->rot.y += Muku2(pPl->rot.y, ang, PI / 12.0f);
    return 1;
}

// the split object's .rodata is 8-aligned (0xE0, the pool ends at 0xDC)
asm(".section .rodata; .balign 8");
