// game/emtree.cpp: tree enemy (cEmTree): a felled trunk that hangs on a parent's parts, falls as a
// three-node rope, or is thrown / shot at the player.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "emtree.h"
#include "at_mod.h"
#include "player.h"
#include "esp.h"
#include "snd.h"
#include "quake.h"
#include "pad.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" {
int MotionMove(cModel* m, int a);
void EffectEspDelete(int a, int b, cModel* m, int c);                                        // est.cpp
void EffectEspgenDelete(int a, int b, cModel* m);
void EffectEfmDelete(int a, int b, cModel* m);
int EmAtkHitCk(void* info, Vec* a, Vec* b, int flag);                                        // em_sub.cpp (obj12 declares it the same way)
static void emTree_R0_Move(cEmTree* em);
}

typedef void (*EmTreeFunc)(cEmTree*);

EmTreeFunc EmTree_R0_move_tbl[4] = {
    emTree_R0_Init,
    emTree_R0_Move,
    0,
    0,
};

EmTreeFunc EmTree_R1_move_tbl[7] = {
    emTree_R1_Set,
    emTree_R1_LostWait,
    emTree_R1_Lost,
    emTree_R1_Parent,
    emTree_R1_Fall,
    emTree_R1_Throw,
    emTree_R1_Shot,
};

EmAtkInfo emTreeAtk = { 200.0f, 8, 400, 0, 10, 0 };

cEmTree* SetTree(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cEmTree* em;
    EmTreeWork* w;

    em = (cEmTree*) EmMgr.createBack(0x49);
    if (em == 0) {
        return 0;
    }
    w = EMTREE_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->rot = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetTree() ModelInit failed.");
        EmMgr.destroy(em);
        return 0;
    }
    YarareInit(em, 0.0f, 0.0f, 0.0f, 250.0f, 10000.0f, 1, 1);
    int parts = 0;
    f32 zero = 0.0f;
    f32 h = 5000.0f;
    f32 r = 200.0f;
    em->atari.init(parts, 2, parts, zero, h, zero, r, r, r, h);
    em->hpMax = em->hp = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 20000.0f, 20000.0f, 20000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 0x10);
    }
    em->lockParts = 0;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(1);
    em->setStatus(0xB);
    em->be_flag &= ~0x01000000;
    em->atari.setPriority(3);
    em->atari.clrFlag100();
    w->flags = 0;
    em->be_flag &= ~0x10;
    w->fallTimer = 0;
    w->pParent = 0;
    w->x20 = 0;
    w->pAtk = 0;
    w->caught = 0;
    w->seFall[0] = 0xFF;
    w->seFall[1] = 0xFF;
    w->seFall[2] = 0;
    w->landed = 0;
    w->seHit[0] = 0xFF;
    w->seHit[1] = 0xFF;
    w->seHit[2] = 0;
    w->seWall[0] = 0xFF;
    w->seWall[1] = 0xFF;
    w->seWall[2] = 0;
    w->se64[0] = 0xFF;
    w->se64[1] = 0xFF;
    w->se64[2] = 0;
    w->seAlways[0] = 0xFF;
    w->seAlways[1] = 0xFF;
    w->seAlways[2] = 0;
    w->seAlwaysWait = 4;
    w->effFall[0] = 0xFF;
    w->effFall[1] = 0xFF;
    w->eff72[0] = 0xFF;
    w->eff72[1] = 0xFF;
    w->sndId = 0;
    w->effHit[0] = 0xFF;
    w->effHit[1] = 0xFF;
    em->pMotion = 0;
    w->estNo = 50;
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
    emTree_R0_Move(em);
    return em;
}

void cEmTree::beginEvent()
{
}

void emTreeDmCk(cEmTree* em)
{
    u8 wep;

    if (em->dmHit == 0) {
        return;
    }
    wep = em->dmWep;
    em->dmHit = 0;
    if (wep == 0x10) {
        em->dmType = 0x11;
    }
    EmDmBloodSet2(em, 1, 12, 0, 0, 0);
}

void cEmTree::move()
{
    EmTreeWork* w = EMTREE_WK(this);

    motFlags2 &= ~0x40000000;
    emTreeDmCk(this);
    EmTree_R0_move_tbl[xFC](this);
    if ((be_flag & 0x201) == 1) {
        EmAtCheck(this);
        atari.move();
        if (w->pParent) {
            alpha = w->pParent->alpha;
            x158 = w->pParent->x158;
            if (w->pParent->be_flag & 2) {
                be_flag |= 2;
            } else {
                be_flag &= ~2;
            }
        }
        if (w->flags & 2) {
            be_flag &= ~2;
        }
    }
}

void emTree_R0_Init(cEmTree* em)
{
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
}

static void emTree_R0_Move(cEmTree* em)
{
    EmTree_R1_move_tbl[em->xFD](em);
}

void emTree_R1_Set(cEmTree* em)
{
    if (em->pMotion) {
        MotionMove(em, 0);
    } else {
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        ScaleMatrix(em->mat, &em->scale);
        em->partsMatCalc();
    }
    em->partsWorldCalc();
}

void emTree_R1_LostWait(cEmTree* em)
{
    EmTreeWork* w = EMTREE_WK(em);
    Vec scr;
    Vec pos;

    switch (em->xFE) {
    case 0:
        w->timer = 90;
        em->xFE++;
    case 1:
        if (w->timer == 0) {
            em->alpha -= 0.1f;
            if (em->alpha <= 0.0f) {
                em->alpha = 0.0f;
                em->xFC = 1;
                em->xFD = 2;
                em->xFE = 0;
                em->xFF = 0;
                break;
            }
        } else {
            w->timer--;
        }
        pos = em->pos;
        GetScreenPos(&pos, &scr);
        if (scr.z > 1.0f) {
            em->xFC = 1;
            em->xFD = 2;
            em->xFE = 0;
            em->xFF = 0;
        }
        break;
    }
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

void emTree_R1_Lost(cEmTree* em)
{
    EmTreeWork* w = EMTREE_WK(em);

    switch (em->xFE) {
    case 0:
        em->hp = 0;
        em->atari.flags &= ~0x200;
        em->be_flag &= ~2;
        w->timer = 30;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            EmMgr.destroy(em);
        }
        break;
    }
}

void emTree_R1_Parent(cEmTree* em)
{
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    EmTreeWork* w = EMTREE_WK(em);
    cModel* parent = w->pParent;

    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    if (parent && parent->pParts) {
        PSMTXConcat(parent->getPartsPtr(w->partsNo)->mat, em->mat, m);
        if (!(w->flags & 1)) {
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
#line 471 "D:/Bio4/Prog/emtree.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 473 "D:/Bio4/Prog/emtree.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 475 "D:/Bio4/Prog/emtree.cpp"
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
        PSMTXCopy(m, em->mat);
    }
    if (em->pMotion) {
        em->motFlags2 |= 0x40000000;
        MotionMove(em, 0);
    } else {
        em->partsMatCalc();
    }
    em->partsWorldCalc();
    if (w->fallTimer) {
        w->fallTimer--;
        if (w->fallTimer == 0) {
            em->setFall();
        }
    }
}

void emTree_R1_Fall(cEmTree* em)
{
    EmTreeWork* w = EMTREE_WK(em);
    Vec pt[3] = {
        { 0.0f, 7000.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },
        { 500.0f, 3500.0f, 0.0f },
    };
    EmTreeNode node[3];
    EmTreeNode* n;
    EmTreeNode* nx;
    Vec b;
    Vec a;
    Vec c;
    Vec tmp;
    f32 floor;
    u32 i;
    u32 k;
    f32 mag;
    f32 d;

    em->hp = 0;
    floor = EatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) + 300.0f;
    for (i = 0; i < 3; i++) {
        n = &node[i];
        n->spd.x = w->pt[i].x;
        n->spd.y = w->pt[i].y;
        n->spd.z = w->pt[i].z;
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        PSMTXMultVec(em->mat, &pt[i], &n->pos);
        n->old = n->pos;
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        if (i == 2) {
            nx = node;
        } else {
            nx = &node[i + 1];
        }
        n->len = GetDistance3(&n->pos, &nx->pos);
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        n->spd.y -= 20.0f;
        PSVECAdd(&n->pos, &n->spd, &n->pos);
        n->onFloor = 0;
    }
    for (k = 0; k < 30; k++) {
        for (i = 0; i < 3; i++) {
            n = &node[i];
            if (i == 2) {
                nx = node;
            } else {
                nx = &node[i + 1];
            }
            PSVECSubtract(&nx->pos, &n->pos, &tmp);
            mag = PSVECMag(&tmp);
            d = (n->len - mag) * 0.5f;
            PSVECScale(&tmp, &tmp, (1.0f / mag) * d);
            PSVECAdd(&nx->pos, &tmp, &nx->pos);
            PSVECSubtract(&n->pos, &tmp, &n->pos);
            if (n->pos.y < floor) {
                n->pos.y = floor;
                n->onFloor = 1;
            }
            if (nx->pos.y < floor) {
                nx->pos.y = floor;
                nx->onFloor = 1;
            }
        }
    }
    // `n` is one function-scope pointer shared by all the loops: the later mentions keep the
    // inner loop's giv from being marked replaceable by record_giv, so loop.c emits its final value
    // (`addi r0, node, 0x58` after the inner loop, inside the k loop) and cse2 turns this loop's
    // bound into a copy of it (`mr r22, r0`). Block-scoped `n`s give one `addi r22` here.
    for (i = 0; i < 3; i++) {
        n = &node[i];
        if (i == 2) {
            nx = node;
        } else {
            nx = &node[i + 1];
        }
        if (n->onFloor) {
            if (w->landed == 0 && n->spd.y < -50.0f) {
                w->landed = 1;
                if (w->seFall[0] != 0xFF) {
                    SndCall(w->seFall[0], w->seFall[1], &em->pos, w->seFall[2], 0, em);
                }
                if (w->effFall[0] != 0xFF && w->effFall[1] != 0xFF) {
                    EstSet((int) em, -1, 0, 0, w->effFall[0], w->effFall[1], 0, 0, (u32) em, 0);
                    em->be_flag &= ~2;
                    em->xFC = 1;
                    em->xFD = 2;
                    em->xFE = 0;
                    em->xFF = 0;
                    return;
                }
            }
            EffectEspDelete(0, w->estNo, em, 0);
            EffectEspgenDelete(0, w->estNo, em);
            EffectEfmDelete(0, w->estNo, em);
            n->spd.x *= fRand0_1() * 0.2f + 0.5f;
            n->spd.y *= -(fRand0_1() * 0.2f + 0.5f);
            n->spd.z *= fRand0_1() * 0.2f + 0.5f;
            if (n->spd.y <= 20.0f) {
                if (n->spd.y > 0.0f) {
                    n->spd.y = 0.0f;
                }
            }
        } else {
            PSVECSubtract(&n->pos, &n->old, &n->spd);
        }
        PSVECScale(&n->spd, &n->spd, 0.999f);
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        w->pt[i].x = n->spd.x;
        w->pt[i].y = n->spd.y;
        w->pt[i].z = n->spd.z;
    }
    PSVECSubtract(&node[0].pos, &node[1].pos, &a);
    PSVECSubtract(&node[2].pos, &node[1].pos, &b);
    PSVECCrossProduct(&b, &a, &c);
    PSVECCrossProduct(&a, &c, &b);
#line 685 "D:/Bio4/Prog/emtree.cpp"
    VECNormalize(&b, &b);
#line 686 "D:/Bio4/Prog/emtree.cpp"
    VECNormalize(&a, &a);
#line 687 "D:/Bio4/Prog/emtree.cpp"
    VECNormalize(&c, &c);
    em->mat[0][0] = b.x;
    em->mat[1][0] = b.y;
    em->mat[2][0] = b.z;
    em->mat[0][1] = a.x;
    em->mat[1][1] = a.y;
    em->mat[2][1] = a.z;
    em->mat[0][2] = c.x;
    em->mat[1][2] = c.y;
    em->mat[2][2] = c.z;
    PSVECScale(&pt[0], &tmp, -1.0f);
    TransMatrix(em->mat, &node[0].pos);
    PSMTXMultVec(em->mat, &tmp, &tmp);
    TransMatrix(em->mat, &tmp);
    em->pos = tmp;
    mag = node[0].spd.x * node[0].spd.x + node[0].spd.y * node[0].spd.y + node[0].spd.z * node[0].spd.z
        + node[1].spd.x * node[1].spd.x + node[1].spd.y * node[1].spd.y + node[1].spd.z * node[1].spd.z
        + node[2].spd.x * node[2].spd.x + node[2].spd.y * node[2].spd.y + node[2].spd.z * node[2].spd.z;
    if (mag < 25.0f) {
        em->pos.x = em->mat[0][3];
        em->pos.y = em->mat[1][3];
        em->pos.z = em->mat[2][3];
        Matrix2AxisAngle(em->mat, &em->rot);
        em->xFC = 1;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
    }
    em->partsWorldCalc();
}

void emTree_R1_Throw(cEmTree* em)
{
    EmTreeWork* w = EMTREE_WK(em);
    Vec d;
    Mtx m;
    Vec up;
    Vec fwd;
    f32 ang;

    switch (em->xFE) {
    case 0:
        w->timer = 0;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->timer = w->seAlwaysWait;
            if (w->seAlways[0] != 0xFF && w->seAlways[1] != 0xFF) {
                w->sndId = SndCall(w->seAlways[0], w->seAlways[1], &em->pos, w->seAlways[2], 0, em);
            }
        }
        break;
    }
    w->spd.y -= 15.0f;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    if (EatMgr.hitCheck(&em->oldPos, &em->pos, 0, 0, 0, 0)) {
        em->setFall();
        if (w->seWall[0] != 0xFF && w->seWall[1] != 0xFF) {
            SndCall(w->seWall[0], w->seWall[1], &em->pos, w->seWall[2], 0, em);
        }
        SndStop(w->sndId, 0);
    } else if (w->pAtk) {
        if (EmAtkHitCk(w->pAtk, &em->pos, &em->oldPos, 1)) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            if (w->seHit[0] != 0xFF && w->seHit[1] != 0xFF) {
                SndCall(w->seHit[0], w->seHit[1], &em->pos, w->seHit[2], 0, em);
            }
            SndStop(w->sndId, 0);
            QuakeExec(0, 0, 5, 22.0f, 2);
            if (w->effHit[0] != 0xFF && w->effHit[1] != 0xFF) {
                EmPlBloodSet2(em, &em->pos, 1, w->effHit[0], w->effHit[1]);
            } else {
                EmPlBloodSet2(em, &em->pos, 1, 0xFF, 0xFF);
            }
            em->setFall();
        }
    }
    PSVECSubtract(&em->pos, &em->oldPos, &d);
    PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    fwd.x = 0.0f;
    fwd.y = 0.0f;
    fwd.z = 1.0f;
    PSMTXMultVecSR(m, &fwd, &fwd);
    if (fwd.x == 0.0f) {
        fwd.y = 0.0f;
    }
#line 825 "D:/Bio4/Prog/emtree.cpp"
    VECNormalize(&fwd, &fwd);
    ang = acosf(PSVECDotProduct(&up, &fwd));
    if (ang > 0.01f && ang < 3.1315927f) {
        PSVECCrossProduct(&up, &fwd, &up);
        PSMTXRotAxisRad(m, &up, 0.62831855f);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
}

void emTree_R1_Shot(cEmTree* em)
{
    EmTreeWork* w = EMTREE_WK(em);
    Vec hit;
    Vec hitPos;
    Vec nrm;
    Mtx inv;
    EmHitInfo* part;
    int no;
    f32 len;

    switch (em->xFE) {
    case 0:
        w->timer = 0;
        w->timer2 = 90;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->timer = w->seAlwaysWait;
            if (w->seAlways[0] != 0xFF && w->seAlways[1] != 0xFF) {
                w->sndId = SndCall(w->seAlways[0], w->seAlways[1], &em->pos, w->seAlways[2], 0, em);
            }
        }
        if (w->timer2) {
            w->timer2--;
        } else {
            em->xFC = 1;
            em->xFD = 2;
            em->xFE = 0;
            em->xFF = 0;
            return;
        }
        break;
    case 2:
        w->x20 = 0;
        w->timer = 60;
        em->hp = 0;
        em->xFE++;
    case 3:
        em->partsWorldCalc();
        if (w->timer) {
            w->timer--;
        } else {
            em->setFall();
            SndStop(w->sndId, 0);
        }
        return;
    }
    w->spd.y -= 0.0f;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    if (EatMgr.hitCheck(&em->oldPos, &em->pos, &hit, 0, 0, 0)) {
        if (w->seWall[0] != 0xFF && w->seWall[1] != 0xFF) {
            SndCall(w->seWall[0], w->seWall[1], &em->pos, w->seWall[2], 0, em);
        }
        SndStop(w->sndId, 0);
        em->pos = hit;
        TransMatrix(em->mat, &em->pos);
        em->partsWorldCalc();
        em->xFE = 2;
    } else if (w->pAtk && (part = (EmHitInfo*) EmAtkLineHitCk(&em->oldPos, &em->pos, &hitPos, &nrm, 0)) != 0) {
        VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
        if (w->seHit[0] != 0xFF && w->seHit[1] != 0xFF) {
            SndCall(w->seHit[0], w->seHit[1], &em->pos, w->seHit[2], 0, em);
        }
        SndStop(w->sndId, 0);
        QuakeExec(0, 0, 5, 22.0f, 2);
        if (w->effHit[0] != 0xFF && w->effHit[1] != 0xFF) {
            EmPlBloodSet2(em, &em->pos, 1, w->effHit[0], w->effHit[1]);
        } else {
            EmPlBloodSet2(em, &em->pos, 1, 0xFF, 0xFF);
        }
        EmAtkSetDamagePL((cEm*) part, w->pAtk, &em->oldPos, &em->pos);
        if ((part->flags & 0x4000) == 0) {
            em->setFall();
        } else {
            no = 0;
            if (part->partsNo != 0) {
                no = part->partsNo - 1;
            }
            PSMTXInverse(pPL->getPartsPtr(no)->mat, inv);
            PSMTXMultVec(inv, &part->pos, &em->pos);
            len = SQRTF(em->pos.x * em->pos.x + em->pos.z * em->pos.z);
            em->rot.x = -atan2f(-em->pos.y, len);
            em->rot.y = atan2f(-em->pos.x, -em->pos.z);
            em->rot.z = 0.0f;
            if ((s16) pGS->pl_life <= 0) {
                w->fallTimer = 0;
            } else {
                w->fallTimer = 30;
            }
            em->setParent(pPL, no, 0);
            emTree_R1_Parent(em);
        }
    } else {
        TransMatrix(em->mat, &em->pos);
        em->partsWorldCalc();
    }
}

void cEmTree::setParent(cModel* parent, int partsNo, int flag)
{
    EmTreeWork* w = EMTREE_WK(this);

    w->pParent = parent;
    w->partsNo = partsNo;
    if (flag) {
        w->flags |= 1;
    } else {
        w->flags &= ~1;
    }
    xFC = 1;
    xFD = 3;
    xFE = 0;
    xFF = 0;
    ((cEm*) parent)->atari.flags &= ~0x200;
}

void cEmTree::clearParent()
{
    EmTreeWork* w = EMTREE_WK(this);

    w->pParent = 0;
    xFC = 1;
    xFD = 0;
    xFE = 0;
    xFF = 0;
}

void cEmTree::setFall()
{
    EmTreeWork* w = EMTREE_WK(this);
    cModel* parts;
    u32 i;

    pMotion = 0;
    for (i = 0; i < 3; i++) {
        w->pt[i].x = fRand1_1() * 10.0f;
        w->pt[i].y = fRand1_1() * 10.0f + 50.0f;
        w->pt[i].z = fRand1_1() * 10.0f;
    }
    w->pParent = 0;
    w->x20 = 0;
    hp = 0;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    Matrix2AxisAngle(mat, &rot);
    parts = getPartsPtr(0);
    parts->rot.x = 0.0f;
    parts->rot.y = 0.0f;
    parts->rot.z = 0.0f;
    RotMatrix(parts->worldMat, &parts->rot);
    TransMatrix(parts->worldMat, &parts->pos);
    xFC = 1;
    xFD = 4;
    xFE = 0;
    xFF = 0;
}

// Never called in the DOL: the linker dropped the bodies and kept the constant pools
// ({10, 20, 75, 350, 0, PI/2} twice after setFall's), see STRIP_UNUSED.
void cEmTree::setThrow(Vec* spd, EmAtkInfo* atk)
{
    EmTreeWork* w = EMTREE_WK(this);
    Vec v;
    Mtx m;

    if (spd) {
        v = *spd;
    } else {
        v.x = fRand1_1() * 10.0f + 20.0f;
        v.y = fRand1_1() * 10.0f + 75.0f;
        v.z = fRand1_1() * 10.0f + 350.0f;
        if (w->pParent) {
            PSMTXMultVecSR(w->pParent->mat, &v, &v);
        }
    }
    PSMTXMultVecSR(mat, &v, &v);
    w->spd = v;
    rot.x = 0.0f;
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &rot);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(m, mat, mat);
    TransMatrix(mat, &pos);
    oldPos = pos;
    w->pParent = 0;
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emTreeAtk;
    }
    xFC = 1;
    xFD = 5;
    xFE = 0;
    xFF = 0;
}

void cEmTree::setShot(Vec* spd, EmAtkInfo* atk)
{
    EmTreeWork* w = EMTREE_WK(this);
    Vec v;
    Mtx m;

    if (spd) {
        v = *spd;
    } else {
        v.x = fRand1_1() * 10.0f + 20.0f;
        v.y = fRand1_1() * 10.0f + 75.0f;
        v.z = fRand1_1() * 10.0f + 350.0f;
        if (w->pParent) {
            PSMTXMultVecSR(w->pParent->mat, &v, &v);
        }
    }
    PSMTXMultVecSR(mat, &v, &v);
    w->spd = v;
    rot.x = 0.0f;
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &rot);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(m, mat, mat);
    TransMatrix(mat, &pos);
    oldPos = pos;
    w->pParent = 0;
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emTreeAtk;
    }
    xFC = 1;
    xFD = 6;
    xFE = 0;
    xFF = 0;
}

int cEmTree::ckCatch()
{
    return EMTREE_WK(this)->caught == 0;
}

void cEmTree::setCatch()
{
    EMTREE_WK(this)->caught = 1;
}

void cEmTree::setLost()
{
    be_flag &= ~2;
    atari.flags &= ~0x200;
    hp = 0;
    xFC = 1;
    xFD = 2;
    xFE = 0;
    xFF = 0;
}
