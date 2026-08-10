// game/emmine.cpp: mine / arrow enemy (cEmMine): the mine thrower's mines (homing when the
// weapon level is high enough) and the crossbow arrows. They fly, stick to the scenario or an
// enemy, beep and explode (mines) or fall down as a three-node rope (arrows).

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "emmine.h"
#include "emhit.h"
#include "esp.h"
#include "snd.h"
#include "player.h"
#include "pl_wep.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "dbmodule.h"
#include "db_log.h"

// GetWepTargetList entry (em_sub.cpp).
struct WepTarget {
    cEm* em;
    EmHitInfo* part;
};

extern "C" {
void EffectEspDelete(int a, int b, cModel* m, int c);                                        // est.cpp
void EffectEspgenDelete(int a, int b, cModel* m);
void EffectEfmDelete(int a, int b, cModel* m);
// em_sub.cpp: enemies on the line p0-p1 (at most `prio` of them) into `list`; hit point / normal / attribute out.
u32 GetWepTargetList2(Vec* p0, Vec* p1, WepTarget* list, u32 prio, Vec* hit, Vec* nrm, u32* attr, int type, int flag);
int CheckInWater(cModel* m, int a);                                                          // em_sub.cpp
}

// Rope node of the falling arrow (emMine_R1_Fall).
struct MineNode {
    Vec pos;          // 0x00
    Vec oldPos;       // 0x0C
    Vec spd;          // 0x18
    f32 len;          // 0x24  rest distance to the next node
    int onFloor;      // 0x28
};

// lockParts = 0 through an int parameter: the zero becomes an SImode pseudo that every later
// `= 0` store of SetMine reuses (one `li r30, 0`; a direct `lockParts = 0` gets its own QImode zero).
static inline void LockPartsSet(cEm* em, int no)
{
    em->lockParts = no;
}

typedef void (*EmMineFunc)(cEmMine*);

EmMineFunc EmMine_R0_move_tbl[4] = {
    emMine_R0_Init,
    emMine_R0_Move,
    0,
    0,
};

static EmMineFunc EmMine_R1_move_tbl[9] = {
    emMine_R1_Shot,
    emMine_R1_ShotArrow,
    emMine_R1_Set,
    emMine_R1_SetWater,
    emMine_R1_Parent,
    emMine_R1_BombWait,
    emMine_R1_BombWait2,
    emMine_R1_Fall,
    emMine_R1_Lost,
};

cEmMine* SetMine(void* bin, void* tpl, Vec* pos, Vec* spd, int type)
{
    cEmMine* em;
    EmMineWork* w;
    cAtariInfo* at;
    Vec v;
    f32 len;

    em = (cEmMine*) EmMgr.createBack(0x4F);
    if (em == 0) {
        return 0;
    }
    w = EMMINE_WK(em);
    if (pos) {
        em->pos = *pos;
    }
    em->oldPos = em->pos;
    if (em->modelInit(bin, tpl) == 0) {
        pLog->err(0, 0, "SetWeapon() ModelInit failed.");
        EmMgr.destroy(em);
        return 0;
    }
    w->wepLv = pG->wep_lv;
    em->type = type;
    if (w->wepLv > 2) {
        em->type = 1;
    }
    YarareInit(em, 0.0f, 0.0f, -100.0f, 300.0f, 10.0f, 1, 1);
    at = &em->atari;
    at->init(1, 0x2000, 10, 0.0f, 0.0f, 0.0f, 150.0f, 150.0f, 150.0f, 300.0f);
    em->hp = 1;
    em->hpMax = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        em->lightInfo.init2(0, 1, &ofs, &size, 4);
    }
    LockPartsSet(em, 0);
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->be_flag &= ~0x01000000;
    at->setPriority(3);
    at->flags &= ~0x300;
    em->be_flag &= ~0x10;
    em->setStatus(1);
    em->setStatus(0xB);
    w->flags = 0;
    w->hitNrm.x = 0.0f;
    w->hitNrm.y = 1.0f;
    w->hitNrm.z = 0.0f;
    w->hitFlag = 0;
    w->pParent = 0;
    w->pTarget = 0;
    w->searchWait = 3;
    w->espKind = EspPullCoreKind();
    w->effKind = 0;
    w->effNo = 0x36;
    w->snd0 = 1;
    w->snd1 = 0x14;
    if (spd) {
        v = *spd;
        if (em->type == 1) {
            PSVECScale(&v, &v, 0.5f);
        }
    } else {
        v.x = fRand1_1() * 10.0f + 20.0f;
        v.y = fRand1_1() * 10.0f + 75.0f;
        v.z = fRand1_1() * 10.0f + 350.0f;
    }
    w->spd.x = v.x;
    w->spd.y = v.y;
    w->spd.z = v.z;
    len = SQRTF(v.x * v.x + v.z * v.z);
    em->rot.x = -atan2f(v.y, len);
    em->rot.y = atan2f(v.x, v.z);
    em->rot.z = 0.0f;
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    if (em->type == 1) {
        cEm* target = pPL->pWep->pObj->wep.target;

        if (target) {
            w->pTarget = target;
        } else {
            emMineSearchEm(em, 1);
        }
    }
    if (type == 2) {
        em->hp = 0;
        em->xFC = 1;
        em->xFD = 1;
        em->xFE = 0;
        em->xFF = 0;
    } else {
        em->xFC = 1;
        em->xFD = 0;
        em->xFE = 0;
        em->xFF = 0;
    }
    emMine_R0_Move(em);
    return em;
}

void cEmMine::beginEvent()
{
    EmMgr.destroy(this);
}

void emMineDmCk(cEmMine* em)
{
    u8 wep;

    if (em->dmHit == 0) {
        return;
    }
    wep = em->dmWep;
    em->dmHit = 0;
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
    em->setStatus(1);
    em->hp = 0;
    em->setBomb();
}

void cEmMine::move()
{
    emMineDmCk(this);
    EmMine_R0_move_tbl[xFC](this);
}

void emMine_R0_Init(cEmMine* em)
{
    em->xFC = 1;
    em->xFD = 8;
    em->xFE = 0;
    em->xFF = 0;
}

void emMine_R0_Move(cEmMine* em)
{
    EmMine_R1_move_tbl[em->xFD](em);
}

void emMine_R1_Shot(cEmMine* em)
{
    EmMineWork* w = EMMINE_WK(em);
    Vec d;
    Vec hit;
    f32 wh;
    f32 wh2;
    f32 len;
    int attr;
    AtEffInfo* info;
    AtEffInfo* wi;  // the water blocks' own pointer: its zero (not the HitCk result's) feeds their EstSet stack zeros

    switch (em->xFE) {
    case 0:
        EstSet((int) em, -1, 0, 0, 0, 0x38, 0, w->espKind, (u32) em, 0);
        w->life = 210;
        em->xFE++;
    case 1:
        if (w->life == 0) {
            em->setBomb();
            return;
        }
        w->life--;
        break;
    }
    if (em->type == 1) {
        if (w->searchWait != 0) {
            w->searchWait--;
        } else {
            emMineSearchEm(em, 0);
            emMineHomingEm(em);
        }
    }
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    if (emMineHitCk(em) != 0) {
        goto DELETE_EFFECT;
    }
    {
        w->hitFlag = 0;
        attr = EatMgr.hitCheck(&em->oldPos, &em->pos, &hit, &w->hitNrm, 0, 0x404000);
        if (attr) {
            if (attr & 0x40) {
                em->setBomb();
                return;
            }
            w->hitFlag = 1;
            em->pos = hit;
            info = EatMgr.getEffInfo(EatGetEffectType(attr));
            if (info) {
                if (info->flags & 1) {
                    if (!(info->eff0[0] == 0xD2 && info->eff0[1] == 1)) {
                        EstSet(0, -1, &em->pos, 0, info->eff0[0], (u8) info->eff0[1], 0, 0, 0, 0);
                    }
                }
                w->effKind = (u8) info->eff6[0];
                w->effNo = (u8) info->eff6[1];
                if (info->flags & 1) {
                    w->snd0 = 1;
                    w->hitFlag = 0;
                    w->snd1 = 0x17;
                    SndCall(5, 0x24, &em->pos, 0, 0, em);
                    em->xFC = 1;
                    em->xFD = 3;
                    em->xFE = 0;
                    em->xFF = 0;
                } else {
                    w->snd0 = 1;
                    w->snd1 = 0x14;
                    SndCall(2, 0x14, &em->pos, 0, 0, em);
                    em->xFC = 1;
                    em->xFD = 2;
                    em->xFE = 0;
                    em->xFF = 0;
                }
            } else {
                if (GetWaterHeight(&em->pos, &wh) && em->pos.y <= wh) {
                    em->pos.y = wh;
                    wi = EatMgr.getEffInfo(2);
                    if (wi) {
                        if (!(wi->eff0[0] == 0xD2 && wi->eff0[1] == 1)) {
                            EstSet(0, -1, &em->pos, 0, wi->eff0[0], (u8) wi->eff0[1], 0, 0, 0, 0);
                        }
                        w->effKind = (u8) wi->eff6[0];
                        w->effNo = (u8) wi->eff6[1];
                        SndCall(5, 0x24, &em->pos, 0, 0, em);
                        AddWaterPower(&em->pos, 0.5f);
                    } else {
                        EstSet(0, -1, &em->pos, 0, 0, 0x3A, 0, 0, 0, 0);
                        w->effKind = 0;
                        w->effNo = 0x39;
                        SndCall(5, 0x24, &em->pos, 0, 0, em);
                        AddWaterPower(&em->pos, 0.5f);
                        w->effKind = 0;
                        w->effNo = 0x39;
                    }
                    w->snd0 = 1;
                    w->snd1 = 0x17;
                    EffectEspDelete(0, w->espKind, em, 0);
                    EffectEspgenDelete(0, w->espKind, em);
                    EffectEfmDelete(0, w->espKind, em);
                    em->xFC = 1;
                    em->xFD = 3;
                    em->xFE = 0;
                    em->xFF = 0;
                    return;
                }
                w->effNo = 0x36;
                w->snd1 = 0x14;
                w->effKind = 0;
                w->snd0 = 1;
                em->xFC = 1;
                em->xFD = 2;
                em->xFE = 0;
                em->xFF = 0;
                SndCall(2, 0x14, &em->pos, 0, 0, em);
                // Tail written out (jump2 cross-jumps it into DELETE_EFFECT): the SndCall block then
                // runs through the three Effect*Delete calls, so its r7/r8 argument moves collect an
                // anti-dependence from every later call and issue first (r7, r8, r5, r4, r6, r3).
                EffectEspDelete(0, w->espKind, em, 0);
                EffectEspgenDelete(0, w->espKind, em);
                EffectEfmDelete(0, w->espKind, em);
                return;
            }
        DELETE_EFFECT:
            EffectEspDelete(0, w->espKind, em, 0);
            EffectEspgenDelete(0, w->espKind, em);
            EffectEfmDelete(0, w->espKind, em);
            return;
        }
        if (GetWaterHeight(&em->pos, &wh2) && em->pos.y <= wh2) {
            em->pos.y = wh2;
            wi = EatMgr.getEffInfo(2);
            if (wi) {
                if (!(wi->eff0[0] == 0xD2 && wi->eff0[1] == 1)) {
                    EstSet(0, -1, &em->pos, 0, wi->eff0[0], (u8) wi->eff0[1], 0, 0, 0, 0);
                }
                w->effKind = (u8) wi->eff6[0];
                w->effNo = (u8) wi->eff6[1];
                SndCall(5, 0x24, &em->pos, 0, 0, em);
                AddWaterPower(&em->pos, 0.5f);
            } else {
                EstSet(0, -1, &em->pos, 0, 0, 0x3A, 0, 0, 0, 0);
                w->effKind = 0;
                w->effNo = 0x39;
                SndCall(5, 0x24, &em->pos, 0, 0, em);
                AddWaterPower(&em->pos, 0.5f);
                w->effKind = 0;
                w->effNo = 0x39;
            }
            w->snd0 = 1;
            w->snd1 = 0x17;
            EffectEspDelete(0, w->espKind, em, 0);
            EffectEspgenDelete(0, w->espKind, em);
            EffectEfmDelete(0, w->espKind, em);
            em->xFC = 1;
            em->xFD = 3;
            em->xFE = 0;
            em->xFF = 0;
            return;
        }
        PSVECSubtract(&em->pos, &em->oldPos, &d);
        len = SQRTF(d.x * d.x + d.z * d.z);
        em->rot.x = -atan2f(d.y, len);
        em->rot.y = atan2f(d.x, d.z);
        em->rot.z = 0.0f;
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        em->partsWorldCalc();
    }
}

void emMine_R1_ShotArrow(cEmMine* em)
{
    EmMineWork* w = EMMINE_WK(em);
    Vec d;
    Vec hit;
    f32 wh;
    f32 wh2;
    f32 len;
    int attr;
    AtEffInfo* info;

    switch (em->xFE) {
    case 0:
        EstSet((int) em, -1, 0, 0, 0, 0x4C, 0, w->espKind, (u32) em, 0);
        w->life = 210;
        em->xFE++;
    case 1:
        if (w->life == 0) {
            em->setLost();
            return;
        }
        w->life--;
        break;
    }
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    if (emMineHitCk(em) != 0) {
        goto DELETE_EFFECT;
    }
    {
        w->hitFlag = 0;
        attr = EatMgr.hitCheck(&em->oldPos, &em->pos, &hit, &w->hitNrm, 0, 0x404000);
        if (attr) {
            if (attr & 0x40) {
                em->setFall();
                return;
            }
            w->hitFlag = 1;
            em->pos = hit;
            info = EatMgr.getEffInfo(EatGetEffectType(attr));
            if (info) {
                if (info->flags & 1) {
                    EstSet(0, -1, &em->pos, 0, info->eff0[0], (u8) info->eff0[1], 0, 0, 0, 0);
                }
                w->effKind = (u8) info->eff6[0];
                w->effNo = (u8) info->eff6[1];
                if (info->flags & 1) {
                    w->snd0 = 1;
                    w->hitFlag = 0;
                    w->snd1 = 0x17;
                    SndCall(5, 0x24, &em->pos, 0, 0, em);
                    EffectEspDelete(0, w->espKind, em, 0);
                    EffectEspgenDelete(0, w->espKind, em);
                    EffectEfmDelete(0, w->espKind, em);
                    em->setLost();
                    return;
                }
                w->snd0 = 1;
                w->snd1 = 0x14;
                SndCall(1, 0x50, &em->pos, 0, 0, em);
                em->xFC = 1;
                em->xFD = 2;
                em->xFE = 0;
                em->xFF = 0;
                return;
            }
            if (GetWaterHeight(&em->pos, &wh) && em->pos.y <= wh) {
                em->pos.y = wh;
                // Block-scoped water pointers (one single-set pseudo per block: each ranks below `em`
                // in global-alloc, so em keeps r29 and both take r28; one two-set `wi` outranks em).
                AtEffInfo* wi = EatMgr.getEffInfo(2);
                if (wi) {
                    EstSet(0, -1, &em->pos, 0, wi->eff0[0], (u8) wi->eff0[1], 0, 0, 0, 0);
                    w->effKind = (u8) wi->eff6[0];
                    w->effNo = (u8) wi->eff6[1];
                    SndCall(5, 0x24, &em->pos, 0, 0, em);
                    AddWaterPower(&em->pos, 0.5f);
                } else {
                    EstSet(0, -1, &em->pos, 0, 0, 0x3A, 0, 0, 0, 0);
                    w->effKind = 0;
                    w->effNo = 0x39;
                    SndCall(5, 0x24, &em->pos, 0, 0, em);
                    AddWaterPower(&em->pos, 0.5f);
                    w->effKind = 0;
                    w->effNo = 0x39;
                }
                w->snd0 = 1;
                w->snd1 = 0x17;
                EffectEspDelete(0, w->espKind, em, 0);
                EffectEspgenDelete(0, w->espKind, em);
                EffectEfmDelete(0, w->espKind, em);
                em->setLost();
                return;
            }
            w->effNo = 0x36;
            w->snd1 = 0x14;
            w->effKind = 0;
            w->snd0 = 1;
            em->xFC = 1;
            em->xFD = 2;
            em->xFE = 0;
            em->xFF = 0;
            SndCall(1, 0x50, &em->pos, 0, 0, em);
            EffectEspDelete(0, w->espKind, em, 0); // tail written out, see emMine_R1_Shot
            EffectEspgenDelete(0, w->espKind, em);
            EffectEfmDelete(0, w->espKind, em);
            return;
        DELETE_EFFECT:
            EffectEspDelete(0, w->espKind, em, 0);
            EffectEspgenDelete(0, w->espKind, em);
            EffectEfmDelete(0, w->espKind, em);
            return;
        }
        if (GetWaterHeight(&em->pos, &wh2) && em->pos.y <= wh2) {
            em->pos.y = wh2;
            AtEffInfo* wi = EatMgr.getEffInfo(2);
            if (wi) {
                EstSet(0, -1, &em->pos, 0, wi->eff0[0], (u8) wi->eff0[1], 0, 0, 0, 0);
                w->effKind = (u8) wi->eff6[0];
                w->effNo = (u8) wi->eff6[1];
                SndCall(5, 0x24, &em->pos, 0, 0, em);
                AddWaterPower(&em->pos, 0.5f);
            } else {
                EstSet(0, -1, &em->pos, 0, 0, 0x3A, 0, 0, 0, 0);
                w->effKind = 0;
                w->effNo = 0x39;
                SndCall(5, 0x24, &em->pos, 0, 0, em);
                AddWaterPower(&em->pos, 0.5f);
                w->effKind = 0;
                w->effNo = 0x39;
            }
            w->snd0 = 1;
            w->snd1 = 0x17;
            EffectEspDelete(0, w->espKind, em, 0);
            EffectEspgenDelete(0, w->espKind, em);
            EffectEfmDelete(0, w->espKind, em);
            em->setLost();
            return;
        }
        PSVECSubtract(&em->pos, &em->oldPos, &d);
        len = SQRTF(d.x * d.x + d.z * d.z);
        em->rot.x = -atan2f(d.y, len);
        em->rot.y = atan2f(d.x, d.z);
        em->rot.z = 0.0f;
        RotMatrix(em->mat, &em->rot);
        TransMatrix(em->mat, &em->pos);
        em->partsWorldCalc();
    }
}

void emMineSearchEm(cEmMine* em, int mode)
{
    EmMineWork* w = EMMINE_WK(em);
    Vec dir;
    Vec v;
    f32 best;
    f32 dot;
    u32 i;

    if (w->pTarget) {
        if (w->pTarget->hp > 0) {
            return;
        }
        w->pTarget = 0;
    }
#line 829 "D:/Bio4/Prog/emmine.cpp"
    VECNormalize(&w->spd, &dir);
    if (mode) {
        best = -0.7f;
    } else {
        best = 0.0f;
    }
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        cModel* parts;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e->id <= 0xF) {
            continue;
        }
        if (e->id > 0x3F) {
            continue;
        }
        if (!(e->be_flag & 2)) {
            continue;
        }
        switch (e->id) {
        case 0x21:
        case 0x24:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2E:
        case 0x3B:
        case 0x3D:
            continue;
        }
        if (mode == 0) {
            if ((em->pos.x - e->pos.x) * (em->pos.x - e->pos.x) + (em->pos.y - e->pos.y) * (em->pos.y - e->pos.y) +
                    (em->pos.z - e->pos.z) * (em->pos.z - e->pos.z) >
                225000000.0f) {
                continue;
            }
        }
        parts = e->getPartsPtr(0);
        PSVECSubtract(&parts->worldPos, &em->pos, &v);
        if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f) {
            w->pTarget = e;
            return;
        }
#line 877 "D:/Bio4/Prog/emmine.cpp"
        VECNormalize(&v, &v);
        dot = PSVECDotProduct(&dir, &v);
        if (dot < 0.0f) {
            continue;
        }
        if (dot < best) {
            continue;
        }
        if (EatMgr.hitCheck(&em->pos, &parts->worldPos, 0, 0, 0, 0x404000) != 0) {
            continue;
        }
        w->pTarget = e;
        best = dot;
    }
}

void emMineHomingEm(cEmMine* em)
{
    EmMineWork* w = EMMINE_WK(em);
    Vec axis;
    Mtx m;
    Vec dir;
    Vec to;
    cModel* parts;
    f32 ang;

    if (w->pTarget == 0) {
        return;
    }
    parts = w->pTarget->getPartsPtr(0);
    if (EatMgr.hitCheck(&em->pos, &parts->worldPos, 0, 0, 0, 0x404000)) {
        w->pTarget = 0;
        return;
    }
    if (!(w->pTarget->be_flag & 2)) {
        w->pTarget = 0;
        return;
    }
    if ((parts->worldPos.x - em->pos.x) * (parts->worldPos.x - em->pos.x) +
            (parts->worldPos.y - em->pos.y) * (parts->worldPos.y - em->pos.y) +
            (parts->worldPos.z - em->pos.z) * (parts->worldPos.z - em->pos.z) <
        10000.0f) {
        return;
    }
#line 932 "D:/Bio4/Prog/emmine.cpp"
    VECNormalize(&w->spd, &dir);
    PSVECSubtract(&parts->worldPos, &em->pos, &to);
#line 936 "D:/Bio4/Prog/emmine.cpp"
    VECNormalize(&to, &to);
    ang = acosf(PSVECDotProduct(&dir, &to));
    if (ang < 0.0017453292f) {
        return;
    }
    if (ang > 2.5307274f) {
        return;
    }
    ang = Muku2(0.0f, ang, PI / 16.0f);
    PSVECCrossProduct(&dir, &to, &axis);
    PSMTXRotAxisRad(m, &axis, ang);
    PSMTXMultVecSR(m, &w->spd, &w->spd);
    if (pG->debug_mode == 8) {
        Draw_line3d(&em->pos, &parts->worldPos, 0xFFFFFFFF, 0);
    }
}

void emMine_R1_Set(cEmMine* em)
{
    EmMineWork* w = EMMINE_WK(em);

    switch (em->xFE) {
    case 0:
        if (em->type == 2) {
            em->hp = 0;
        } else {
            em->hp = 1;
        }
        w->life = 150;
        w->timer = 1;
        w->count = 17;
        em->xFE++;
        break;
    case 1:
        if (em->type != 2 && w->timer != 0) {
            w->timer--;
            if (w->timer == 0) {
                Vec p;

                w->timer = w->count;
                w->count--;
                if (w->count <= 4) {
                    w->count = 5;
                }
                EstSet((int) em, -1, 0, 0, 0, 0x37, 0, 0, (u32) em, 0);
                p.x = 0.0f;
                p.y = 0.0f;
                p.z = -250.0f;
                PSMTXMultVec(em->mat, &p, &p);
                SndCall(1, 5, &p, 0, 0, em);
            }
        }
        if (w->life != 0) {
            w->life--;
        } else {
            if (em->type == 2) {
                em->setFall();
            } else {
                em->setBomb();
            }
            return;
        }
        break;
    }
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

void emMine_R1_SetWater(cEmMine* em)
{
    EmMineWork* w = EMMINE_WK(em);

    switch (em->xFE) {
    case 0:
        if (em->type == 2) {
            em->hp = 0;
        } else {
            em->hp = 1;
        }
        w->life = 150;
        em->be_flag &= ~2;
        em->xFE++;
        break;
    case 1:
        if (w->life == 0) {
            em->setBomb();
            return;
        }
        w->life--;
        break;
    }
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
}

void emMine_R1_Parent(cEmMine* em)
{
    EmMineWork* w = EMMINE_WK(em);
    cEm* parent = w->pParent;

    if ((parent->be_flag & 0x201) != 1) {
        w->pParent = 0;
        parent = 0;
    }
    if (parent == 0) {
        em->setLost();
    }
    switch (em->xFE) {
    case 0:
        if (em->type == 2) {
            em->hp = 0;
        } else {
            em->hp = 1;
        }
        w->life = 150;
        w->timer = 1;
        w->count = 17;
        em->xFE++;
        break;
    case 1:
        if (w->timer != 0 && em->type != 2) {
            w->timer--;
            if (w->timer == 0) {
                Vec p;

                w->timer = w->count;
                w->count--;
                if (w->count <= 4) {
                    w->count = 5;
                }
                EstSet((int) em, -1, 0, 0, 0, 0x37, 0, 0, (u32) em, 0);
                p.x = 0.0f;
                p.y = 0.0f;
                p.z = -250.0f;
                PSMTXMultVec(em->mat, &p, &p);
                SndCall(1, 5, &p, 0, 0, em);
            }
        }
        if (!(parent->be_flag & 2)) {
            w->life = 0;
        }
        if (w->life == 0) {
            if (em->type != 2) {
                em->setBomb();
                return;
            }
            em->setFall();
            return;
        }
        w->life--;
        break;
    }
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    if (parent) {
        if (parent->hp <= 0) {
            if (em->type != 2) {
                em->setBomb();
                return;
            }
            if (w->life > 30) {
                w->life = 30;
            }
        }
        if (parent->pParts) {
            Mtx m;
            Vec v0;
            Vec v1;
            Vec v2;

            PSMTXConcat(parent->getPartsPtr(w->partsNo)->mat, em->mat, m);
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
#line 1161 "D:/Bio4/Prog/emmine.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 1163 "D:/Bio4/Prog/emmine.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 1165 "D:/Bio4/Prog/emmine.cpp"
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
            PSMTXCopy(m, em->mat);
        }
    }
    em->partsMatCalc();
    em->partsWorldCalc();
}

void emMine_R1_BombWait(cEmMine* em)
{
    EmMineWork* w = EMMINE_WK(em);
    Vec v;

    switch (em->xFE) {
    case 0:
        w->timer = 4;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
            break;
        }
        em->hp = 0;
        em->pos.x = em->mat[0][3];
        em->pos.y = em->mat[1][3];
        em->pos.z = em->mat[2][3];
        if (w->hitFlag) {
            PSVECScale(&w->hitNrm, &v, 1000.0f);
            PSVECAdd(&em->pos, &v, &em->pos);
            TransMatrix(em->mat, &em->pos);
        }
        w->hitFlag = 0;
        em->setBomb();
        break;
    }
}

void emMine_R1_BombWait2(cEmMine* em)
{
    EmMineWork* w = EMMINE_WK(em);
    Vec p;
    f32 r;

    switch (em->xFE) {
    case 0:
        em->hp = 0;
        em->be_flag &= ~2;
        w->timer = 2;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
            break;
        }
        em->hp = 0;
        p.x = 0.0f;
        p.y = 0.0f;
        p.z = -250.0f;
        PSMTXMultVec(em->mat, &p, &p);
        switch (w->wepLv) {
        case 0:
            r = 2000.0f;
            break;
        case 1:
            r = 4000.0f;
            break;
        default:
            r = 6000.0f;
            break;
        }
        PlWepHitCheck2(0, &p, &p, 0x13, 0, r);
        em->setLost();
        break;
    }
}

void emMine_R1_Fall(cEmMine* em)
{
    EmMineWork* w = EMMINE_WK(em);
    Vec ofs[4] = { { 0.0f, 0.0f, 600.0f }, { 0.0f, 0.0f, -600.0f }, { 300.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
    MineNode node[3];
    Vec e1;
    Vec nrm;
    Vec e0;
    Vec d;
    MineNode* n;
    MineNode* nn;
    f32 floor;
    u32 i;
    u32 k;
    f32 mag;
    f32 dd;

    em->hp = 0;
    em->setStatus(1);
    floor = EatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) + 50.0f;
    // COMPILER-DIFF: candidate (gcse PRE pseudo numbering): two dead sets (deleted by flow) take the
    // expression table from 235 to 237 buckets, so `w+48`/`fp+100` (13389) hash below `fp+144` (13433)
    // and the three PRE'd addresses get the original's spill-slot order (256/260/264).
    i = 5; i = 6;
    for (i = 0; i < 3; i++) {
        n = &node[i];
        n->spd.x = w->pts[i].x;
        n->spd.y = w->pts[i].y;
        n->spd.z = w->pts[i].z;
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        PSMTXMultVec(em->mat, &ofs[i], &n->pos);
        n->oldPos = n->pos;
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        if (i == 2) {
            nn = &node[0];
        } else {
            nn = &node[i + 1];
        }
        n->len = GetDistance3(&n->pos, &nn->pos);
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        n->spd.y -= w->grav;
        PSVECAdd(&n->pos, &n->spd, &n->pos);
        n->onFloor = 0;
    }
    for (k = 0; k < 30; k++) {
        for (i = 0; i < 3; i++) {
            n = &node[i];
            if (i == 2) {
                nn = &node[0];
            } else {
                nn = &node[i + 1];
            }
            PSVECSubtract(&nn->pos, &n->pos, &d);
            // `mag` is assigned again after the loops (the speed test), so the call result is not
            // tied to it (`fmr f12, f1`); `dd` keeps the (len - mag) * 0.5 chain in f1 (emtree).
            mag = PSVECMag(&d);
            dd = (n->len - mag) * 0.5f;
            PSVECScale(&d, &d, (1.0f / mag) * dd);
            PSVECAdd(&nn->pos, &d, &nn->pos);
            PSVECSubtract(&n->pos, &d, &n->pos);
            if (n->pos.y < floor) {
                n->pos.y = floor;
                n->onFloor = 1;
            }
            if (nn->pos.y < floor) {
                nn->pos.y = floor;
                nn->onFloor = 1;
            }
        }
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        if (n->onFloor) {
            n->spd.x *= fRand0_1() * 0.2f + 0.5f;
            n->spd.y *= -(fRand0_1() * 0.2f + 0.5f);
            n->spd.z *= fRand0_1() * 0.2f + 0.5f;
            if (n->spd.y <= w->grav) {
                if (n->spd.y > 0.0f) {
                    n->spd.y = 0.0f;
                }
            }
        } else {
            PSVECSubtract(&n->pos, &n->oldPos, &n->spd);
        }
        PSVECScale(&n->spd, &n->spd, 0.999f);
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        w->pts[i].x = n->spd.x;
        w->pts[i].y = n->spd.y;
        w->pts[i].z = n->spd.z;
    }
    PSVECSubtract(&node[0].pos, &node[1].pos, &e0);
    PSVECSubtract(&node[2].pos, &node[1].pos, &e1);
    PSVECCrossProduct(&e0, &e1, &nrm);
    PSVECCrossProduct(&nrm, &e0, &e1);
#line 1442 "D:/Bio4/Prog/emmine.cpp"
    VECNormalize(&e1, &e1);
#line 1443 "D:/Bio4/Prog/emmine.cpp"
    VECNormalize(&nrm, &nrm);
#line 1444 "D:/Bio4/Prog/emmine.cpp"
    VECNormalize(&e0, &e0);
    em->mat[0][0] = e1.x;
    em->mat[1][0] = e1.y;
    em->mat[2][0] = e1.z;
    em->mat[0][1] = nrm.x;
    em->mat[1][1] = nrm.y;
    em->mat[2][1] = nrm.z;
    em->mat[0][2] = e0.x;
    em->mat[1][2] = e0.y;
    em->mat[2][2] = e0.z;
    PSVECScale(&ofs[0], &d, -1.0f);
    TransMatrix(em->mat, &node[0].pos);
    PSMTXMultVec(em->mat, &d, &d);
    TransMatrix(em->mat, &d);
    em->pos = d;
    mag = node[0].spd.x * node[0].spd.x + node[0].spd.y * node[0].spd.y + node[0].spd.z * node[0].spd.z
        + node[1].spd.x * node[1].spd.x + node[1].spd.y * node[1].spd.y + node[1].spd.z * node[1].spd.z
        + node[2].spd.x * node[2].spd.x + node[2].spd.y * node[2].spd.y + node[2].spd.z * node[2].spd.z;
    if (mag < 25.0f) {
        em->pos.x = em->mat[0][3];
        em->pos.y = em->mat[1][3];
        em->pos.z = em->mat[2][3];
        Matrix2AxisAngle(em->mat, &em->rot);
        em->setLost();
    }
    em->partsWorldCalc();
    if (w->waterSnd == 0) {
        if (CheckInWater(em, 0)) {
            SndCall(6, 0x17, &em->pos, 0, 0, em);
            w->waterSnd = 1;
        }
    }
}

void emMine_R1_Lost(cEmMine* em)
{
    EmMineWork* w = EMMINE_WK(em);

    if (em->xFE == 0) {
        em->hp = 0;
        em->be_flag &= ~2;
        EffectEspDelete(0, w->espKind, em, 0);
        EffectEspgenDelete(0, w->espKind, em);
        EffectEfmDelete(0, w->espKind, em);
        em->xFE++;
        EmMgr.destroy(em);
    }
}

void cEmMine::setParent(cEm* parent, int partsNo_)
{
    EmMineWork* w = EMMINE_WK(this);

    w->pParent = parent;
    w->partsNo = partsNo_;
    xFC = 1;
    xFD = 4;
    xFE = 0;
    xFF = 0;
}

void cEmMine::setLost()
{
    hp = 0;
    be_flag &= ~2;
    xFC = 1;
    xFD = 8;
    xFE = 0;
    xFF = 0;
}

void cEmMine::setBomb()
{
    EmMineWork* w = EMMINE_WK(this);
    Vec p;
    int hit;

    hp = 0;
    hit = w->hitFlag;
    if (hit) {
        xFC = 1;
        xFD = 5;
        xFE = 0;
        xFF = 0;
        return;
    }
    p.x = 0.0f;
    p.y = 0.0f;
    p.z = -250.0f;
    PSMTXMultVec(mat, &p, &p);
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    EstSet(0, -1, &pos, 0, w->effKind, w->effNo, 0, 0, 0, 0);
    SndCall(w->snd0, w->snd1, &p, 0, 0, this);
    AddWaterPower(&pos, 1.0f);
    EffectEspDelete(0, w->espKind, this, 0);
    EffectEspgenDelete(0, w->espKind, this);
    EffectEfmDelete(0, w->espKind, this);
    BitOn(pG->flags_500C, 0x800000);
    BitOn(pG->flags_5010, 0x20000000);
    memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &p, sizeof(Vec));
    pG->bell_stat = 1;
    setLost();
    // COMPILER-DIFF: candidate #12 (cse wider-mode zero fold): the original stores the known-zero
    // `hit` register into xFE (ours folds it to the HImode zero pseudo); the launder keeps `hit` as
    // the store source, the codeless keep-alive after the stores keeps `hit`/`this` from dying at
    // the xFE store (weight -1 would issue it first; the target has it last, source order).
    asm("" : "+r"(hit));
    xFF = 0;
    xFC = 1;
    xFD = 6;
    xFE = hit;
    asm("" : "=m"(hp) : "r"(hit));
}

void cEmMine::setFall()
{
    EmMineWork* w = EMMINE_WK(this);
    u32 i;

    pMotion = 0;
    for (i = 0; i < 3; i++) {
        w->pts[i].x = fRand1_1() * 10.0f;
        w->pts[i].y = fRand1_1() * 10.0f + 50.0f;
        w->pts[i].z = fRand1_1() * 10.0f;
    }
    w->pParent = 0;
    w->x10 = 0;
    hp = 0;
    w->grav = 15.0f;
    w->waterSnd = 0;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    Matrix2AxisAngle(mat, &rot);
    xFC = 1;
    xFD = 7;
    xFE = 0;
    xFF = 0;
}

int emMineHitCk(cEmMine* em)
{
    Vec hit;
    Vec nrm;
    Mtx inv;
    Vec dir;
    Vec a;
    Vec b;
    WepTarget list;
    u32 attr;
    cEm* hitEm;
    EmHitInfo* part;
    int type;
    int partsNo;
    f32 len;

    type = 0xE;
    if (em->type == 2) {
        type = 0x1C;
    }
    if (GetWepTargetList2(&em->oldPos, &em->pos, &list, 1, &hit, &nrm, &attr, type, 0) != 0) {
        hitEm = list.em;
        part = list.part;
        hitEm->dmg.set(0, 2, type, &em->oldPos, part->rad, part);
        if (part->flags & 0x4000) {
            partsNo = 0;
            if (part->partsNo != 0) {
                partsNo = part->partsNo - 1;
            }
            PSMTXInverse(hitEm->getPartsPtr(partsNo)->mat, inv);
            PSMTXMultVec(inv, &part->pos, &em->pos);
#line 1723 "D:/Bio4/Prog/emmine.cpp"
            VECNormalize(&em->pos, &dir);
            PSVECScale(&dir, &dir, -50.0f);
            PSVECAdd(&em->pos, &dir, &em->pos);
            switch (hitEm->id) {
            default:
                len = SQRTF(em->pos.x * em->pos.x + em->pos.z * em->pos.z);
                em->rot.x = -atan2f(-em->pos.y, len);
                em->rot.y = atan2f(-em->pos.x, -em->pos.z);
                em->rot.z = 0.0f;
                break;
            case 0x40:
            case 0x41:
            case 0x43:
            case 0x44:
            case 0x45:
            case 0x46:
            case 0x47:
            case 0x48:
            case 0x49:
            case 0x4A:
            case 0x4B:
            case 0x4C:
            case 0x4D:
            case 0x4E:
            case 0x50:
            case 0x51:
                a.x = 0.0f;
                a.y = 0.0f;
                a.z = 0.0f;
                b.x = 0.0f;
                b.y = 0.0f;
                b.z = 1.0f;
                PSMTXMultVec(em->mat, &a, &a);
                PSMTXMultVec(em->mat, &b, &b);
                PSMTXMultVec(inv, &a, &a);
                PSMTXMultVec(inv, &b, &b);
                PSVECSubtract(&b, &a, &dir);
                len = SQRTF(dir.x * dir.x + dir.z * dir.z);
                em->rot.x = -atan2f(dir.y, len);
                em->rot.y = atan2f(dir.x, dir.z);
                em->rot.z = 0.0f;
                break;
            }
        } else {
            partsNo = 0;
            em->pos.x = 0.0f;
            em->pos.y = 0.0f;
            em->pos.z = 0.0f;
            em->rot.x = 0.0f;
            em->rot.y = 0.0f;
            em->rot.z = 0.0f;
        }
        em->scale.x = 2.0f;
        em->scale.y = 2.0f;
        em->scale.z = 2.0f;
        switch (hitEm->id) {
        default:
            if (em->type == 2) {
                SndCall(1, 0x54, &em->pos, 0, 0, em);
            }
            break;
        case 0x2A:
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
        case 0x4C:
        case 0x4D:
        case 0x4E:
        case 0x4F:
        case 0x50:
        case 0x51:
            SndCall(1, 0x50, &em->pos, 0, 0, em);
            break;
        }
        em->setParent(hitEm, partsNo);
        emMine_R1_Parent(em);
        return 1;
    }
    return 0;
}
