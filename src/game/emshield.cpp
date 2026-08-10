// game/emshield.cpp: shield enemy (cEmShield): a wooden shield carried by an enemy that loses its
// planks when shot and falls to the ground as a three-node rope.
//
// Byte-identical. setFall carries a COMPILER-DIFF #8 keep-alive (the `fmr f29, gravity` prologue copy
// ranks last in the original: the incoming f1 did not die at the copy there, docs/matching.md #8).

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "emshield.h"
#include "at_mod.h"
#include "player.h"
#include "esp.h"
#include "snd.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" {
int MotionMove(cModel* m, int a);
void EffectEspDelete(int a, int b, cModel* m, int c);                                        // est.cpp
void EffectEspgenDelete(int a, int b, cModel* m);
void EffectEfmDelete(int a, int b, cModel* m);
int CheckInWater(cModel* m, int a);                                                          // em_sub.cpp
}

typedef void (*EmShieldFunc)(cEmShield*);

static EmShieldFunc EmShield_R0_move_tbl[4] = {
    emShield_R0_Init,
    emShield_R0_Move,
    0,
    0,
};

static EmShieldFunc EmShield_R1_move_tbl[5] = {
    emShield_R1_Set,
    emShield_R1_LostWait,
    emShield_R1_Lost,
    emShield_R1_Parent,
    emShield_R1_Fall,
};

cEmShield* SetShield(void* bin, void* tpl, Vec* pos, Vec* rot)
{
    cEmShield* em;
    EmShieldWork* w;

    em = (cEmShield*) EmMgr.createBack(0x50);
    if (em == 0) {
        return 0;
    }
    w = EMSHIELD_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    if (rot) {
        em->rot = *rot;
    }
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetWeapon() ModelInit failed.");
        EmMgr.destroy(em);
        return 0;
    }
    int no = 0;
    YarareInitCube(em, 0.0f, -50.0f, -100.0f, 200.0f, 100.0f, 300.0f, no, no);
    YarareAddCube(em, &w->hit[0], 0.0f, 0.0f, -70.0f, 100.0f, 90.0f, 300.0f, 2, 1);
    YarareAddCube(em, &w->hit[1], 0.0f, 0.0f, 100.0f, 150.0f, 90.0f, 300.0f, 3, 1);
    YarareAddCube(em, &w->hit[2], 0.0f, 0.0f, 0.0f, 100.0f, 90.0f, 300.0f, 4, 1);
    YarareAddCube(em, &w->hit[3], 0.0f, 0.0f, 0.0f, 100.0f, 90.0f, 300.0f, 5, 1);
    YarareAddCube(em, &w->hit[4], 0.0f, 0.0f, 0.0f, 150.0f, 90.0f, 200.0f, 6, 1);
    YarareAddCube(em, &w->hit[5], 0.0f, 0.0f, -75.0f, 100.0f, 90.0f, 300.0f, 7, 1);
    YarareAddCube(em, &w->hit[6], 0.0f, 0.0f, 0.0f, 150.0f, 90.0f, 250.0f, 8, 1);
    YarareAddCube(em, &w->hit[7], 0.0f, 0.0f, 0.0f, 150.0f, 90.0f, 250.0f, 9, 1);
    YarareAddCube(em, &w->hit[8], -50.0f, 0.0f, 0.0f, 150.0f, 90.0f, 250.0f, 10, 1);
    em->atari.init(1, 0x2000, 10, 0.0f, 0.0f, 0.0f, 150.0f, 150.0f, 150.0f, 300.0f);
    em->hpMax = em->hp = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 2);
    }
    em->lockParts = 0;
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(1);
    em->setStatus(0xB);
    em->be_flag &= ~0x01000000;
    em->atari.setPriority(3);
    em->atari.throughOn();
    w->flags = 0;
    em->be_flag &= ~0x10;
    w->fallTimer = 0;
    w->inWater = 0;
    w->breakCnt = 0;
    w->pParent = 0;
    w->pOldParent = 0;
    w->xA8 = 0;
    w->x38 = -1;
    w->hitCnt = (Rnd() % 3) + 2;
    w->seAlwaysWait = 4;
    w->gravity = 20.0f;
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
    w->se8F[0] = 0xFF;
    w->se8F[1] = 0xFF;
    w->se8F[2] = 0;
    w->seAlways[0] = 0xFF;
    w->seAlways[1] = 0xFF;
    w->seAlways[2] = 0;
    w->effFall[0] = 0xFF;
    w->effFall[1] = 0xFF;
    w->eff9D[0] = 0xFF;
    w->eff9D[1] = 0xFF;
    w->eff9B[0] = 0xFF;
    w->eff9B[1] = 0xFF;
    w->effWater[0] = 0xFF;
    w->effWater[1] = 0xFF;
    w->effAlways[0] = 0xFF;
    w->effAlways[1] = 0xFF;
    w->effParts = 0xFF;
    w->x34 = 0;
    w->effWait = 0;
    w->effTimer = 0;
    w->effOfs.x = 0.0f;
    w->effOfs.y = 0.0f;
    w->effOfs.z = 0.0f;
    w->estNo = 50;
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
    emShield_R0_Move(em);
    return em;
}

void cEmShield::beginEvent()
{
    if (EMSHIELD_WK(this)->pParent == 0) {
        EmMgr.destroy(this);
    }
}

// Damage: every few hits a plank (hit box parts 2..10) breaks off; the fourth plank, an explosion
// or a heavy weapon destroys the shield.
// Byte-identical. Shape notes: the compare tree needs the default-labelled members 0..4, 0xB, 0xE,
// 0x10, 0x11, 0x14, 0x1B, 0x1D, 0x26, 0x27, 0x2B (tools/research/casetree.py: [0,4] and [16,17] only add
// balance weight, their compares are jump-threaded away); the two plank bodies (default arm / B arm)
// stay separate copies only because they use different pointer variables (`parts0` = the top
// getPartsPtr(0) variable, also the breakAll/D one, so it crosses calls and is callee-saved r31;
// `parts` in B, `parts2` in the C continuation), the C arm falls through into D with `goto plank`
// for its own body laid out after D, and `!(rad < K)` gives the plain `bge`. The D arm's SndCall
// goes through a do-while(0) + void-returning alias: the loop notes give `&parts0->worldPos` a 5th
// weighted ref (global-alloc priority above `w`: r26/r25) and the void result keeps `li r3,8`
// ahead of the other argument `li`s inside the notes (u32 SndCall issues it last there).
void SndCallV(u16, u16, Vec*, int, int, cUnit*) asm("SndCall__FUsUsP3VeciiP5cUnit");
void emShieldDmCk(cEmShield* em)
{
    EmShieldWork* w = EMSHIELD_WK(em);
    Vec p;
    Vec r;
    u8 wep;
    EmHitInfo* part;
    cModel* parts;
    cModel* parts0;
    cModel* parts2;

    if (em->dmHit == 0) {
        return;
    }
    wep = em->dmWep;
    em->dmHit = 0;
    em->dmType = 1;
    part = em->dmPart;
    if (wep == 0x14) {
        return;
    }
    if (wep == 0x16) {
        return;
    }
    if (wep == 0x17) {
        return;
    }
    if (wep == 0x2A) {
        return;
    }
    if (wep == 0xE) {
        return;
    }
    parts0 = em->getPartsPtr(0);
    if (w->pParent) {
        SndCall(8, 0xAC, &parts0->worldPos, w->pParent->id, 0, em);
    }
    switch (em->dmWep) {
    default:
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 0xB:
    case 0xE:
    case 0x10:
    case 0x11:
    case 0x14:
    case 0x1B:
    case 0x1D:
    case 0x26:
    case 0x27:
    case 0x2B:
        if (part->partsNo == 0) {
            goto blood;
        }
        w->hitCnt--;
        if (w->hitCnt > 0) {
            goto blood;
        }
        w->breakCnt++;
        w->hitCnt = (Rnd() % 3) + 2;
        if (w->breakCnt > 3) {
            goto breakAll;
        }
        parts0 = em->getPartsPtr(part->partsNo - 1);
        p = parts0->worldPos;
        Matrix2AxisAngle(parts0->mat, &r);
        if (part->partsNo == 5) {
            EstSet(0, -1, &p, &r, 0x10, 0x63, 0, 0, 0, 0);
        } else {
            EstSet(0, -1, &p, &r, 0x10, 0x61, 0, 0, 0, 0);
        }
        if (w->pParent) {
            SndCall(8, 0xAD, &parts0->worldPos, w->pParent->id, 0, em);
        }
        parts0->scale.x = 0.0f;
        parts0->scale.y = 0.0f;
        parts0->scale.z = 0.0f;
        part->flags &= ~1;
        break;
    case 5:
    case 6:
    case 9:
    case 0xA:
    case 0xF:
    case 0x28:
    case 0x2C:
        if (part->partsNo == 0) {
            goto blood;
        }
        w->breakCnt++;
        w->hitCnt = (Rnd() % 3) + 2;
        if (w->breakCnt > 3) {
        breakAll:
            parts0 = em->getPartsPtr(0);
            p = parts0->worldPos;
            Matrix2AxisAngle(parts0->mat, &r);
            EstSet(0, -1, &p, &r, 0x10, 0x62, 0, 0, 0, 0);
            if (w->pParent) {
                SndCall(8, 0xAE, &parts0->worldPos, w->pParent->id, 0, em);
            }
            em->hp = 0;
            em->xFC = 1;
            em->xFD = 2;
            em->xFE = 0;
            em->xFF = 0;
            break;
        }
        parts = em->getPartsPtr(part->partsNo - 1);
        p = parts->worldPos;
        Matrix2AxisAngle(parts->mat, &r);
        if (part->partsNo == 5) {
            EstSet(0, -1, &p, &r, 0x10, 0x63, 0, 0, 0, 0);
        } else {
            EstSet(0, -1, &p, &r, 0x10, 0x61, 0, 0, 0, 0);
        }
        if (w->pParent) {
            SndCall(8, 0xAD, &parts->worldPos, w->pParent->id, 0, em);
        }
        parts->scale.x = 0.0f;
        parts->scale.y = 0.0f;
        parts->scale.z = 0.0f;
        part->flags &= ~1;
        break;
    blood:
        EmDmBloodSet2(em, 0x10, 0x60, 0, 0, 0);
        break;
    case 7:
    case 8:
    case 0x21:
        if (part->rad > 64000000.0f) {
            break;
        }
        if (w->breakCnt <= 3 && !(part->rad < 12250000.0f)) {
            goto plank;
        }
    case 0xD:
    case 0x12:
    case 0x13:
    case 0x15:
    case 0x1C:
    case 0x29:
    case 0x2D:
        parts0 = em->getPartsPtr(0);
        p = parts0->worldPos;
        Matrix2AxisAngle(parts0->mat, &r);
        EstSet(0, -1, &p, &r, 0x10, 0x62, 0, 0, 0, 0);
        em->hp = 0;
        em->xFC = 1;
        em->xFD = 2;
        em->xFE = 0;
        em->xFF = 0;
        if (w->pParent) {
            do {
                SndCallV(8, 0xAE, &parts0->worldPos, w->pParent->id, 0, em);
            } while (0);
        }
        break;
    plank:
        if (part->partsNo == 0) {
            break;
        }
        parts2 = em->getPartsPtr(part->partsNo - 1);
        p = parts2->worldPos;
        Matrix2AxisAngle(parts2->mat, &r);
        if (part->partsNo == 5) {
            EstSet(0, -1, &p, &r, 0x10, 0x63, 0, 0, 0, 0);
        } else {
            EstSet(0, -1, &p, &r, 0x10, 0x61, 0, 0, 0, 0);
        }
        w->hitCnt -= 3;
        if (w->hitCnt > 0) {
            break;
        }
        w->breakCnt++;
        parts2->scale.x = 0.0f;
        parts2->scale.y = 0.0f;
        parts2->scale.z = 0.0f;
        part->flags &= ~1;
        w->hitCnt = (Rnd() % 3) + 2;
        if (w->pParent) {
            SndCall(8, 0xAD, &em->getPartsPtr(0)->worldPos, w->pParent->id, 0, em);
        }
        break;
    }
}

void cEmShield::move()
{
    EmShieldWork* w = EMSHIELD_WK(this);
    Vec p;

    emShieldDmCk(this);
    EmShield_R0_move_tbl[xFC](this);
    if ((be_flag & 0x201) == 1) {
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
        if ((be_flag & 2) && w->effAlways[0] != 0xFF && w->effAlways[1] != 0xFF && w->effTimer != 0) {
            s16 t = --w->effTimer;

            if (t == 0) {
                PSMTXMultVec(getPartsPtr(w->effParts)->mat, &w->effOfs, &p);
                EstSet(0, -1, &p, 0, w->effAlways[0], w->effAlways[1], 0, 0, 0, 0);
                w->effTimer = w->effWait;
            }
        }
        if (w->pParent) {
            if (((cEm*) w->pParent)->hp <= 0) {
                hp = 0;
            }
            if (w->pParent && (w->pParent->be_flag & 0x201) != 1) {
                EmMgr.destroy(this);
            }
        }
    }
}

void emShield_R0_Init(cEmShield* em)
{
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
}

void emShield_R0_Move(cEmShield* em)
{
    EmShield_R1_move_tbl[em->xFD](em);
}

void emShield_R1_Set(cEmShield* em)
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

void emShield_R1_LostWait(cEmShield* em)
{
    EmShieldWork* w = EMSHIELD_WK(em);
    Vec scr;
    Vec pos;

    switch (em->xFE) {
    case 0:
        em->setStatus(1);
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

void emShield_R1_Lost(cEmShield* em)
{
    EmShieldWork* w = EMSHIELD_WK(em);

    if (em->xFE == 0) {
        em->hp = 0;
        em->be_flag &= ~2;
        em->be_flag &= ~0x20;
        em->setStatus(1);
        EffectEspDelete(0, w->estNo, em, 0);
        EffectEspgenDelete(0, w->estNo, em);
        EffectEfmDelete(0, w->estNo, em);
        em->xFE++;
        EmMgr.destroy(em);
    }
}

void emShield_R1_Parent(cEmShield* em)
{
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    EmShieldWork* w = EMSHIELD_WK(em);
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
#line 762 "D:/Bio4/Prog/emshield.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 764 "D:/Bio4/Prog/emshield.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 766 "D:/Bio4/Prog/emshield.cpp"
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
            em->setFall(20.0f, 0);
        }
    }
}

void emShield_R1_Fall(cEmShield* em)
{
    EmShieldWork* w = EMSHIELD_WK(em);
    Vec pt[3] = {
        { -200.0f, 30.0f, 500.0f },
        { -200.0f, 30.0f, -800.0f },
        { 500.0f, 30.0f, 0.0f },
    };
    EmTreeNode node[3];
    Vec b;
    Vec c;
    Vec a;
    Vec tmp;
    EmTreeNode* n;    // shared by every node loop (emtree emTree_R1_Fall: the giv final-value copy)
    EmTreeNode* nx;
    f32 floor;
    u32 i;
    u32 k;
    f32 mag;
    f32 d;
    cModel* parts0;

    em->hp = 0;
    em->setStatus(1);
    floor = EatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) + 80.0f;
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
        n->spd.y -= w->gravity;
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
                if (w->inWater == 0 && w->pOldParent) {
                    parts0 = em->getPartsPtr(0);
                    SndCall(8, 0xAF, &parts0->worldPos, w->pOldParent->id, 0, em);
                }
                if (w->effFall[0] != 0xFF && w->effFall[1] != 0xFF) {
                    EstSet((int) em, -1, 0, 0, w->effFall[0], w->effFall[1], 0, 0, (u32) em, 0);
                }
            }
            EffectEspDelete(0, w->estNo, em, 0);
            EffectEspgenDelete(0, w->estNo, em);
            EffectEfmDelete(0, w->estNo, em);
            n->spd.x *= fRand0_1() * 0.2f + 0.5f;
            n->spd.y *= -(fRand0_1() * 0.2f + 0.5f);
            n->spd.z *= fRand0_1() * 0.2f + 0.5f;
            if (n->spd.y <= w->gravity) {
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
    PSVECCrossProduct(&a, &b, &c);
    PSVECCrossProduct(&c, &a, &b);
#line 973 "D:/Bio4/Prog/emshield.cpp"
    VECNormalize(&b, &b);
#line 974 "D:/Bio4/Prog/emshield.cpp"
    VECNormalize(&c, &c);
#line 975 "D:/Bio4/Prog/emshield.cpp"
    VECNormalize(&a, &a);
    em->mat[0][0] = b.x;
    em->mat[1][0] = b.y;
    em->mat[2][0] = b.z;
    em->mat[0][1] = c.x;
    em->mat[1][1] = c.y;
    em->mat[2][1] = c.z;
    em->mat[0][2] = a.x;
    em->mat[1][2] = a.y;
    em->mat[2][2] = a.z;
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
    if (w->inWater == 0) {
        parts0 = em->getPartsPtr(0);
        if (CheckInWater(em, 0)) {
            if (w->effWater[0] != 0xFF && w->effWater[1] != 0xFF) {
                EstSet(0, -1, &em->pos, 0, w->effWater[0], w->effWater[1], 0, 0, 0, 0);
            }
            SndCall(6, 0x17, &parts0->worldPos, 0, 0, em);
            w->inWater = 1;
        }
    }
}

void cEmShield::setParent(cModel* parent, int partsNo, int flag)
{
    EmShieldWork* w = EMSHIELD_WK(this);

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
}

// Drops the shield: node speeds from `spd` (node 0 as is, nodes 1 / 2 rotated +-90 degrees around Y)
// or random when NULL.
void cEmShield::setFall(f32 gravity, Vec* spd)
{
    EmShieldWork* w = EMSHIELD_WK(this);
    Mtx m;
    Vec v;
    u32 i;
    register f64 hd asm("fr1"); // COMPILER-DIFF: #8

    pMotion = 0;
    // COMPILER-DIFF: #8 -- the original ranks `fmr f29,f1` as if f1 did not die at the copy. A DFmode
    // read of f1 after the copy keeps f1 live past it (regmove's optimize_reg_copy_1 only moves the
    // death when the dying mode matches the copy's SFmode); the "=m" output on a `this` field the
    // block does not touch keeps the codeless asm dependence-free.
    asm("" : "=m"(hp) : "f"(hd));
    for (i = 0; i < 3; i++) {
        if (spd) {
            f32 ang;

            switch (i) {
            case 0:
            default:
                w->pt[i].x = spd->x;
                w->pt[i].y = spd->y;
                w->pt[i].z = spd->z;
                break;
            case 1:
                if (spd->x == 0.0f && spd->z == 0.0f) {
                    ang = 0.0f;
                } else {
                    ang = atan2f(spd->x, spd->z);
                }
                PSMTXRotRad(m, 'y', ang + 1.5707964f);
                PSMTXMultVec(m, spd, &v);
                w->pt[i].x = v.x;
                w->pt[i].y = v.y;
                w->pt[i].z = v.z;
                break;
            case 2:
                if (spd->x == 0.0f && spd->z == 0.0f) {
                    ang = 0.0f;
                } else {
                    ang = atan2f(spd->x, spd->z);
                }
                PSMTXRotRad(m, 'y', ang - 1.5707964f);
                PSMTXMultVec(m, spd, &v);
                w->pt[i].x = v.x;
                w->pt[i].y = v.y;
                w->pt[i].z = v.z;
                break;
            }
        } else {
            w->pt[i].x = fRand1_1() * 20.0f;
            w->pt[i].y = fRand1_1() * 20.0f;
            w->pt[i].z = fRand1_1() * 20.0f;
        }
    }
    w->pOldParent = w->pParent;
    w->pParent = 0;
    hp = 0;
    w->gravity = gravity;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    Matrix2AxisAngle(mat, &rot);
    xFC = 1;
    xFD = 4;
    xFE = 0;
    xFF = 0;
}
