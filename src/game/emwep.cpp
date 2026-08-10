// game/emwep.cpp: weapon enemy (cEmWep): the weapons the enemies hold (setParent), drop (setFall,
// a three-node rope), throw (axes, scythes, dynamite, grenades) or shoot (arrows, rockets) at the
// player, with the player's escape routines of the grenade.
//

#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "ctrl.h"
#include "atari_init.h"
#include "emwep.h"
#include "at_mod.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "esp.h"
#include "est.h"
#include "snd.h"
#include "quake.h"
#include "pad.h"
#include "main.h"
#include "act_btn.h"
#include "cam_ctrl.h"
#include "sce_at.h"
#include "obj.h"
#include "rnd.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" {
int MotionMove(cModel* m, int a);
int EmAtkHitCk(void* info, Vec* a, Vec* b, int flag);                                        // em_sub.cpp
EmHitInfo* EmAtkLineHitCkSub(Vec* a, Vec* b, Vec* hit, Vec* nrm);                            // em_sub.cpp
void EmAtkSetDamageSub(EmHitInfo* part, EmAtkInfo* info, Vec* a, Vec* b);                     // em_sub.cpp
EmHitInfo* emLineAtCk(cEm* em, Vec* a, Vec* b, f32 len, int flag);                           // em_sub.cpp
int CheckInWater(cModel* m, int a);                                                          // em_sub.cpp
void GameAddPoint(int no);                                                                   // game.cpp
static void emWep_R1_Parent(cEmWep* em);
// The original is a `static plemEscape` (emBar.cpp has a global one); the name carries the split's
// address suffix in sym_map.
#define plemEscape plemEscape_80017688
static void plemEscape(cPlayer* pl);
}
void MotionSetCore(cModel* m, void* w, void* data, int seq, int hokan, int flags, int frame);   // motion.cpp (C++ linkage)
cObj* SetObj01(void* bin, void* tpl, Vec* pos, Vec* rot, Vec* spd, f32 grav, f32 rad, int life, int flags);   // obj01.cpp
void Obj01SetEst(cObj* obj, int no0, int prm0, u32 type, int no1, int prm1, int no2, int prm2, int no3, int prm3);


// setYarareCube(0, x, y, z) with the float arguments' moves issued before the `li r4, 0`
// (atari_init.h: GCC emits the argument moves in declaration order).
void setYarareCubeF(cEmWep* em, f32 x, f32 y, f32 z, Vec* size) asm("setYarareCube__6cEmWepP3Vecfff");


// The weapon the player damage callbacks belong to.
#define PL_WEP(pl) ((cEmWep*) (pl)->dmgType)

// One rope node of the falling weapon (emWep_R1_Fall): three point masses joined by distance
// constraints; the model matrix is rebuilt from them every frame.
struct EmWepNode {
    Vec pos;      // 0x00
    Vec old;      // 0x0C
    Vec spd;      // 0x18
    f32 len;      // 0x24
    int onFloor;  // 0x28
};

// lockParts = 0 through an int parameter: the zero becomes an SImode pseudo shared with the
// later `= 0` stores (emrock SetRock).
static inline void LockPartsSet(cEm* em, int no)
{
    em->lockParts = no;
}

typedef void (*EmWepFunc)(cEmWep*);

EmWepFunc EmWep_R0_move_tbl[4] = {
    emWep_R0_Init,
    emWep_R0_Move,
    0,
    0,
};

EmWepFunc EmWep_R1_move_tbl[13] = {
    emWep_R1_Set,
    emWep_R1_LostWait,
    emWep_R1_Lost,
    emWep_R1_Parent,
    emWep_R1_Fall,
    emWep_R1_Throw,
    emWep_R1_Shot,
    emWep_R1_ShotArrow,
    emWep_R1_Rocket,
    emWep_R1_BombThrow,
    emWep_R1_ThrowScythe,
    emWep_R1_FlashThrow,
    emWep_R1_GrenadeThrow,
};

EmAtkInfo emWepAtk = { 200.0f, 8, 400, 0, 10, 0 };

// Cloth chain of the whip-like weapons (setCloth): parts per link and the neighbour tables.
u8 emWepClothP[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
u8 emWepClothUp[10] = { 0xFF, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
static u8 emWepClothDp[10] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 0xFF };
f32 emWepClothMax[10] = { 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f };
PlClothAt emWepAt[3] = {
    { 0, 4, 4, 1.0f, 200.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 3, 3, 0.5f, 250.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { 0, 2, 2, 1.0f, 250.0f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
};

cEmWep* SetWeapon(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type)
{
    cEmWep* em;
    EmWepWork* w;

    em = (cEmWep*) EmMgr.createBack(0x42);
    if (em == 0) {
        return 0;
    }
    w = EMWEP_WK(em);
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
    em->type = type;
    atariInitF(&em->atari, 0.0f, 0.0f, 0.0f, 150.0f, 150.0f, 150.0f, 300.0f, 1, 0x2000, 10);
    em->hp = 0;
    em->hpMax = 1000;
    {
        static const Vec ofs = { 0.0f, 0.0f, 0.0f };
        static const Vec size = { 2000.0f, 2000.0f, 2000.0f };

        if (em->type == 1) {
            em->lightInfo.init2(0, 1, &ofs, &size, 0x20);
        } else {
            em->lightInfo.init2(0, 1, &ofs, &size, 2);
        }
    }
    LockPartsSet(em, 0);
    em->lockOfs.x = 0.0f;
    em->lockOfs.y = 0.0f;
    em->lockOfs.z = 0.0f;
    em->setStatus(0xB);
    em->be_flag &= ~0x01000000;
    em->atari.setPriority(3);
    em->atari.throughOn();
    em->be_flag &= ~0x10;
    w->sceAtNo = -1;
    w->seThrow[3] = 4;
    w->grav = 20.0f;
    w->seFall[0] = 0xFF;
    w->seFall[1] = 0xFF;
    w->seHit[0] = 0xFF;
    w->seHit[1] = 0xFF;
    w->seHitWall[0] = 0xFF;
    w->seHitWall[1] = 0xFF;
    w->seDamage[0] = 0xFF;
    w->seDamage[1] = 0xFF;
    w->seThrow[0] = 0xFF;
    w->seThrow[1] = 0xFF;
    w->flags = 0;
    w->timer4 = 0;
    w->inWater = 0;
    w->pParent = 0;
    w->pOwner = 0;
    w->pAtk = 0;
    w->fallType = 0;
    w->seFall[2] = 0;
    w->seFall[3] = 0;
    w->seHit[2] = 0;
    w->seHitWall[2] = 0;
    w->seDamage[2] = 0;
    w->seThrow[2] = 0;
    w->alwaysTimer = 0;
    w->sndId = 0;
    w->effFall[0] = 0xFF;
    w->espKind = 50;
    w->effFall[1] = 0xFF;
    w->effDamage[0] = 0xFF;
    w->effDamage[1] = 0xFF;
    w->effHit[0] = 0xFF;
    w->effHit[1] = 0xFF;
    w->effWater[0] = 0xFF;
    w->effWater[1] = 0xFF;
    w->effAlways[0] = 0xFF;
    w->effAlways[1] = 0xFF;
    w->effAlwaysParts = 0xFF;
    w->effAlwaysWait = 0;
    w->effAlwaysTimer = 0;
    w->effAlwaysOfs.x = 0.0f;
    w->effAlwaysOfs.y = 0.0f;
    w->motEscape = 0;
    w->motBackjump = 0;
    w->motFront = 0;
    w->motEscape2 = 0;
    w->effAlwaysOfs.z = 0.0f;
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
    RotMatrix(em->mat, &em->rot);
    TransMatrix(em->mat, &em->pos);
    ScaleMatrix(em->mat, &em->scale);
    em->partsMatCalc();
    em->partsWorldCalc();
    emWep_R0_Move(em);
    return em;
}

void cEmWep::beginEvent()
{
    if (EMWEP_WK(this)->pParent == 0 && type == 0) {
        EmMgr.destroy(this);
    }
}

void emWepDmCk(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    u8 wep;
    u8 stat;
    int one;
    Vec p;
    Mtx m;
    Vec v;
    Vec r;

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
    if (wep == 0xE) {
        return;
    }
    em->setStatus(1);
    em->hp = 0;
    switch (em->xFD) {
    case 3:
    case 0xA:
    default:
        PSMTXRotRad(m, 'y', GetXZAngle(&em->pos, &em->x328));
        v.x = fRand1_1() * 200.0f;
        v.y = fRand0_1() * 50.0f + 50.0f;
        v.z = fRand0_1() * 100.0f + -250.0f;
        PSMTXMultVecSR(m, &v, &v);
        em->setFall(0, &v, 20.0f);
        if (w->seDamage[0] != 0xFF) {
            SndCall(w->seDamage[0], w->seDamage[1], &em->pos, w->seDamage[2], 0, em);
        }
        if (w->effDamage[0] != 0xFF && w->effDamage[1] != 0xFF) {
            EstSet(0, -1, &em->pos, &em->rot, w->effDamage[0], w->effDamage[1], 0, 0, 0, 0);
        }
        BitOn(pG->flags_5010, 0x20000);
        GameAddPoint(9);
        break;
    case 7:
        BitOn(pG->flags_5010, 0x20000);
        GameAddPoint(9);
        emWepArrowBomb(em);
        break;
    case 8:
        BitOn(pG->flags_5010, 0x20000);
        GameAddPoint(9);
        emWepRocketBobm(em);
        break;
    case 9:
        stat = 1;
        BitOn(pG->flags_5010, 0x20000);
        GameAddPoint(9);
        r.x = 0.0f;
        r.y = GetXZAngle(&em->pos, &pG->Cam.param.pos);
        r.z = 0.0f;
        EstSet(0, -1, &em->pos, &r, 0x10, 0x42, 0, 0, 0, 0);
        if (w->pOwner) {
            SndCall(8, 0x96, &em->pos, w->pOwner->id, 0, em);
        }
        BitOn(pG->flags_500C, 0x800000);
        p = em->pos;
        p.y += 800.0f;
        PlWepHitCheck2(0, &p, &p, 0x13, 3, 5000.0f);
        BitOn(pG->flags_5010, 0x20000000);
        memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &em->pos, sizeof(Vec));
        pG->bell_stat = stat;
        em->setLost();
        break;
    case 0xC:
        BitOn(pG->flags_5010, 0x20000);
        GameAddPoint(9);
        EstSet(0, -1, &em->pos, 0, 0, 0xD, 0, 0, 0, 0);
        EstSet(0, -1, &em->pos, 0, 0, 0x1A, 0, 0, 0, 0);
        one = 1;
        SndCall(one, 0x14, &em->pos, 0, 0, em);
        BitOn(pG->flags_500C, 0x800000);
        p = em->pos;
        p.y += 800.0f;
        PlWepHitCheck2(0, &p, &p, 0x13, 3, 5000.0f);
        BitOn(pG->flags_5010, 0x20000000);
        memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &em->pos, sizeof(Vec));
        pG->bell_stat = one;
        em->setLost();
        break;
    case 0:
    case 2:
        if (w->effDamage[0] != 0xFF && w->effDamage[1] != 0xFF) {
            EstSet(0, -1, &em->pos, &em->rot, w->effDamage[0], w->effDamage[1], 0, 0, 0, 0);
        }
        em->xFC = 1;
        em->xFD = 2;
        em->xFE = 0;
        em->xFF = 0;
        if (w->seDamage[0] != 0xFF) {
            SndCall(w->seDamage[0], w->seDamage[1], &em->pos, w->seDamage[2], 0, em);
        }
        if (w->sceAtNo != -1) {
            SceAtDestroy(w->sceAtNo);
            w->sceAtNo = -1;
        }
        break;
    }
}

void cEmWep::move()
{
    EmWepWork* w = EMWEP_WK(this);
    Vec v;

    emWepDmCk(this);
    EmWep_R0_move_tbl[xFC](this);
    if ((be_flag & 0x201) != 1) {
        return;
    }
    moveCloth();
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
    if (be_flag & 2) {
        if (w->effAlways[0] != 0xFF && w->effAlways[1] != 0xFF && w->effAlwaysTimer != 0) {
            w->effAlwaysTimer--;
            if ((s16) w->effAlwaysTimer == 0) {
                PSMTXMultVec(getPartsPtr(w->effAlwaysParts)->mat, &w->effAlwaysOfs, &v);
                EstSet(0, -1, &v, 0, w->effAlways[0], w->effAlways[1], 0, 0, 0, 0);
                w->effAlwaysTimer = w->effAlwaysWait;
            }
        }
        if (w->alwaysTimer) {
            w->alwaysTimer--;
            if (w->alwaysTimer == 0) {
                cModel* p = getPartsPtr(0);

                w->alwaysTimer = w->alwaysWait;
                SndCall(w->seAlways[0], w->seAlways[1], &p->worldPos, w->seAlways[2], 0, this);
            }
        }
    }
    if (w->pParent && (w->pParent->be_flag & 0x201) != 1) {
        EmMgr.destroy(this);
    }
}

void emWep_R0_Init(cEmWep* em)
{
    em->xFC = 1;
    em->xFD = 0;
    em->xFE = 0;
    em->xFF = 0;
}

void emWep_R0_Move(cEmWep* em)
{
    EmWep_R1_move_tbl[em->xFD](em);
}

void emWep_R1_Set(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);

    switch (em->xFE) {
    case 0:
        w->timer = 3;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        }
        if (em->pMotion) {
            MotionMove(em, 0);
        } else {
            if (em->type != 1 || w->timer != 0) {
                RotMatrix(em->mat, &em->rot);
                TransMatrix(em->mat, &em->pos);
                ScaleMatrix(em->mat, &em->scale);
            }
            em->partsMatCalc();
        }
        break;
    }
    em->partsWorldCalc();
    if (em->type == 1 && !(em->be_flag & 2)) {
        em->hp = 0;
        em->xFC = 1;
        em->xFD = 2;
        em->xFE = 0;
        em->xFF = 0;
    }
}

void emWep_R1_LostWait(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec scr;
    Vec pos;

    switch (em->xFE) {
    case 0:
        em->setStatus(1);
        if (pG->room_id == 0x30F) {
            w->timer = 1;
        } else {
            w->timer = 90;
        }
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

void emWep_R1_Lost(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);

    switch (em->xFE) {
    case 0:
        em->hp = 0;
        em->be_flag &= ~2;
        em->be_flag &= ~0x20;
        em->setStatus(1);
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        em->xFE++;
        EmMgr.destroy(em);
        break;
    }
}

static void emWep_R1_Parent(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);

    em->setParentMatCalc(0);
    if (w->timer4) {
        w->timer4--;
        if (w->timer4 == 0) {
            em->setFall(0, 0, 20.0f);
        }
    }
}

void emWep_R1_Fall(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec ofs[4][3] = {
        { { 0.0f, 0.0f, 600.0f }, { 0.0f, 0.0f, -600.0f }, { 300.0f, 0.0f, 0.0f } },
        { { 0.0f, 0.0f, 1500.0f }, { 0.0f, 0.0f, 0.0f }, { 300.0f, 0.0f, 1300.0f } },
        { { -140.0f, 60.0f, 140.0f }, { -140.0f, 60.0f, -140.0f }, { 200.0f, 60.0f, 0.0f } },
        { { -140.0f, 30.0f, 140.0f }, { -140.0f, 30.0f, -140.0f }, { 200.0f, 30.0f, 0.0f } },
    };
    EmWepNode node[3];
    // one pointer shared by every node loop (emtree emTree_R1_Fall): the later mentions keep the
    // k-body loop's giv from being marked replaceable, loop.c emits its final value `&node[2]`
    // after that loop and cse2 makes the last loop's bound a copy of it (`mr r25, r0`)
    EmWepNode* n;
    EmWepNode* nx;
    Vec b;
    Vec c;
    Vec a;
    Vec tmp;
    f32 floor;
    u32 i;
    u32 k;
    f32 mag;
    f32 d;

    em->hp = 0;
    em->setStatus(1);
    floor = EatMgr.getFloor(&em->pos, 600.0f, 100000.0f, 0, 0) + 50.0f;
    for (i = 0; i < 3; i++) {
        n = &node[i];
        n->spd.x = w->pt[i].x;
        n->spd.y = w->pt[i].y;
        n->spd.z = w->pt[i].z;
    }
    for (i = 0; i < 3; i++) {
        n = &node[i];
        PSMTXMultVec(em->mat, &ofs[w->fallType][i], &n->pos);
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
        n->spd.y -= w->grav;
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
        // dead in this loop (flow deletes it) but its `i == 2` compare is what makes loop.c put the
        // loop bound in the preheader instead of rematerialising `&node[2]` at the bottom
        if (i == 2) {
            nx = node;
        } else {
            nx = &node[i + 1];
        }
        if (n->onFloor) {
            if (w->seFall[3] == 0 && n->spd.y < -50.0f) {
                w->seFall[3] = 1;
                if (w->seFall[0] != 0xFF && w->inWater == 0) {
                    SndCall(w->seFall[0], w->seFall[1], &em->pos, w->seFall[2], 0, em);
                }
                if (w->effFall[0] != 0xFF && w->effFall[1] != 0xFF) {
                    EstSet((int) em, -1, 0, 0, w->effFall[0], w->effFall[1], 0, 0, (u32) em, 0);
                }
            }
            EffectEspDelete(0, w->espKind, (u32) em, 0);
            EffectEspgenDelete(0, w->espKind, (int) em);
            EffectEfmDelete(0, w->espKind, (int) em);
            switch (w->fallType) {
            default:
                n->spd.x *= fRand0_1() * 0.2f + 0.5f;
                n->spd.y *= -(fRand0_1() * 0.2f + 0.5f);
                n->spd.z *= fRand0_1() * 0.2f + 0.5f;
                break;
            case 2:
            case 3:
                n->spd.x *= fRand0_1() * 0.2f + 0.4f;
                n->spd.y *= -(fRand0_1() * 0.1f + 0.3f);
                n->spd.z *= fRand0_1() * 0.2f + 0.4f;
                break;
            }
            if (n->spd.y <= w->grav) {
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
#line 906 "D:/Bio4/Prog/emwep.cpp"
    VECNormalize(&b, &b);
#line 907 "D:/Bio4/Prog/emwep.cpp"
    VECNormalize(&c, &c);
#line 908 "D:/Bio4/Prog/emwep.cpp"
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
    PSVECScale(&ofs[w->fallType][0], &tmp, -1.0f);
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
    if (w->inWater == 0 && CheckInWater(em, 0)) {
        if (w->effWater[0] != 0xFF && w->effWater[1] != 0xFF) {
            EstSet(0, -1, &em->pos, 0, w->effWater[0], w->effWater[1], 0, 0, 0, 0);
        }
        SndCall(6, 0x17, &em->pos, 0, 0, em);
        w->inWater = 1;
    }
}

void emWep_R1_Throw(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec d;
    Mtx m;
    Vec up;
    Vec fwd;
    f32 ang;
    cCtrl* c;

    switch (em->xFE) {
    case 0:
        w->timer = 0;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->timer = w->seThrow[3];
            if (w->seThrow[0] != 0xFF && w->seThrow[1] != 0xFF && w->inWater == 0) {
                w->sndId = SndCall(w->seThrow[0], w->seThrow[1], &em->pos, w->seThrow[2], 0, em);
            }
        }
        break;
    }
    w->spd.y -= w->grav;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    if (EatMgr.hitCheck(&em->oldPos, &em->pos, 0, 0, 0, 0)) {
        em->setFall(0, 0, 20.0f);
        if (w->seHitWall[0] != 0xFF && w->seHitWall[1] != 0xFF && w->inWater == 0) {
            SndCall(w->seHitWall[0], w->seHitWall[1], &em->pos, w->seHitWall[2], 0, em);
        }
        SndStop(w->sndId, 0);
    } else if (w->pAtk) {
        if (EmAtkHitCk(w->pAtk, &em->pos, &em->oldPos, 0)) {
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
            em->setFall(0, 0, 20.0f);
            c = GetCtrlCtrl12();
            Ctrl12Set(c, 6, 0x1E);
            Ctrl12Set(c, 8, 0x78);
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
#line 1068 "D:/Bio4/Prog/emwep.cpp"
    VECNormalize(&fwd, &fwd);
    ang = acosf(PSVECDotProduct(&up, &fwd));
    if (ang > 0.01f && ang < 3.1315927f) {
        PSVECCrossProduct(&up, &fwd, &up);
        PSMTXRotAxisRad(m, &up, 0.62831855f);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    if (w->inWater == 0 && CheckInWater(em, 0)) {
        if (w->effWater[0] != 0xFF && w->effWater[1] != 0xFF) {
            EstSet(0, -1, &em->pos, 0, w->effWater[0], w->effWater[1], 0, 0, 0, 0);
        }
        SndCall(6, 0x17, &em->pos, 0, 0, em);
        w->inWater = 1;
    }
}

void emWep_R1_ThrowScythe(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec d;
    Mtx m;
    cCtrl* c;

    switch (em->xFE) {
    case 0:
        w->timer = 0;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->timer = w->seThrow[3];
            if (w->seThrow[0] != 0xFF && w->seThrow[1] != 0xFF && w->inWater == 0) {
                w->sndId = SndCall(w->seThrow[0], w->seThrow[1], &em->pos, w->seThrow[2], 0, em);
            }
        }
        break;
    }
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    if (EatMgr.hitCheck(&em->oldPos, &em->pos, 0, 0, 0, 0)) {
        em->setFall(0, 0, 20.0f);
        if (w->seHitWall[0] != 0xFF && w->seHitWall[1] != 0xFF && w->inWater == 0) {
            SndCall(w->seHitWall[0], w->seHitWall[1], &em->pos, w->seHitWall[2], 0, em);
        }
        SndStop(w->sndId, 0);
    } else if (w->pAtk) {
        if (EmAtkHitCk(w->pAtk, &em->pos, &em->oldPos, 0)) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 7, 1);
            if (w->seHit[0] != 0xFF && w->seHit[1] != 0xFF) {
                SndCall(w->seHit[0], w->seHit[1], &em->pos, w->seHit[2], 0, em);
            }
            SndStop(w->sndId, 0);
            QuakeExec(0, 0, 5, 22.0f, 2);
            EmPlBloodSet2(em, &em->pos, 1, 0x10, 7);
            if ((s16) pG->pl_life <= 0) {
                emWepPlHeadLost();
            }
            c = GetCtrlCtrl12();
            Ctrl12Set(c, 6, 0x1E);
            Ctrl12Set(c, 8, 0x78);
        }
    }
    PSVECSubtract(&em->pos, &em->oldPos, &d);
    PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
    PSMTXRotRad(m, 'y', -0.62831855f);
    PSMTXConcat(em->mat, m, em->mat);
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    if (w->inWater == 0 && CheckInWater(em, 0)) {
        if (w->effWater[0] != 0xFF && w->effWater[1] != 0xFF) {
            EstSet(0, -1, &em->pos, 0, w->effWater[0], w->effWater[1], 0, 0, 0, 0);
        }
        SndCall(6, 0x17, &em->pos, 0, 0, em);
        w->inWater = 1;
    }
}

void emWep_R1_Shot(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec hit;
    Vec hitPos;
    Vec nrm;
    Mtx inv;
    EmHitInfo* part;
    int no;
    f32 len;
    cCtrl* c;

    switch (em->xFE) {
    case 0:
        PSVECSubtract(&em->pos, &w->spd, &em->oldPos);
        w->timer = 0;
        w->timer2 = 90;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->timer = w->seThrow[3];
            if (w->seThrow[0] != 0xFF && w->seThrow[1] != 0xFF && w->inWater == 0) {
                w->sndId = SndCall(w->seThrow[0], w->seThrow[1], &em->pos, w->seThrow[2], 0, em);
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
        w->pOwner = 0;
        w->timer4 = 60;
        em->hp = 0;
        em->setStatus(1);
        em->xFE++;
    case 3:
        em->partsWorldCalc();
        if (w->timer4) {
            w->timer4--;
        } else {
            em->setFall(0, 0, 20.0f);
            SndStop(w->sndId, 0);
        }
        return;
    }
    w->spd.y -= 0.0f;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    if (emWepShotHitVaseCk(&em->oldPos, &em->pos) || emWepShotHitWindowCk(&em->oldPos, &em->pos)) {
        em->setFall(0, 0, 20.0f);
        return;
    }
    if (EatMgr.hitCheck(&em->oldPos, &em->pos, &hit, 0, 0, 0x404000)) {
        if (w->seHitWall[0] != 0xFF && w->seHitWall[1] != 0xFF && w->inWater == 0) {
            SndCall(w->seHitWall[0], w->seHitWall[1], &em->pos, w->seHitWall[2], 0, em);
        }
        em->hp = 0;
        SndStop(w->sndId, 0);
        em->pos = hit;
        TransMatrix(em->mat, &em->pos);
        em->partsWorldCalc();
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        em->xFE = 2;
        return;
    }
    if (w->pAtk) {
        part = (EmHitInfo*) EmAtkLineHitCk(&em->oldPos, &em->pos, &hitPos, &nrm, 0);
        if (part) {
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
                em->setFall(0, 0, 20.0f);
                return;
            }
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
                w->timer4 = 0;
            } else {
                w->timer4 = 30;
            }
            em->setParent(pPL, no, 0);
            em->hp = 0;
            emWep_R1_Parent(em);
            EffectEspDelete(0, w->espKind, (u32) em, 0);
            EffectEspgenDelete(0, w->espKind, (int) em);
            EffectEfmDelete(0, w->espKind, (int) em);
            c = GetCtrlCtrl12();
            Ctrl12Set(c, 6, 0x1E);
            Ctrl12Set(c, 8, 0x78);
            return;
        }
        part = EmAtkLineHitCkSub(&em->oldPos, &em->pos, &hitPos, &nrm);
        if (part) {
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
            EmAtkSetDamageSub(part, w->pAtk, &em->oldPos, &em->pos);
            if ((part->flags & 0x4000) == 0) {
                em->setFall(0, 0, 20.0f);
                return;
            }
            no = 0;
            if (part->partsNo != 0) {
                no = part->partsNo - 1;
            }
            PSMTXInverse(pSUB->getPartsPtr(no)->mat, inv);
            PSMTXMultVec(inv, &part->pos, &em->pos);
            len = SQRTF(em->pos.x * em->pos.x + em->pos.z * em->pos.z);
            em->rot.x = -atan2f(-em->pos.y, len);
            em->rot.y = atan2f(-em->pos.x, -em->pos.z);
            em->rot.z = 0.0f;
            if ((s16) pGS->sub_life <= 0) {
                w->timer4 = 0;
            } else {
                w->timer4 = 30;
            }
            em->setParent(pSUB, no, 0);
            em->hp = 0;
            emWep_R1_Parent(em);
            EffectEspDelete(0, w->espKind, (u32) em, 0);
            EffectEspgenDelete(0, w->espKind, (int) em);
            EffectEfmDelete(0, w->espKind, (int) em);
            return;
        }
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    if (w->inWater == 0 && CheckInWater(em, 0)) {
        if (w->effWater[0] != 0xFF && w->effWater[1] != 0xFF) {
            EstSet(0, -1, &em->pos, 0, w->effWater[0], w->effWater[1], 0, 0, 0, 0);
        }
        SndCall(6, 0x17, &em->pos, 0, 0, em);
        w->inWater = 1;
    }
}

void emWep_R1_ShotArrow(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec hit;
    Vec nrm;
    EmHitInfo* part;

    switch (em->xFE) {
    case 0:
        EstSet((int) em, -1, 0, 0, 0x2F, 7, 0x800, w->espKind, (u32) em, 0);
        w->timer = 0;
        w->timer2 = 90;
        w->timer3 = 3;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->timer = w->seThrow[3];
            if (w->seThrow[0] != 0xFF && w->seThrow[1] != 0xFF && w->inWater == 0) {
                w->sndId = SndCall(w->seThrow[0], w->seThrow[1], &em->pos, w->seThrow[2], 0, em);
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
        w->pOwner = 0;
        em->setStatus(1);
        w->fuse = 63;
        w->timer2 = 15;
        w->timer = 0;
        EstSet((int) em, -1, 0, 0, 0x2F, 8, 0x800, w->espKind, (u32) em, 0);
        em->xFE++;
    case 3:
        em->partsWorldCalc();
        if (w->fuse == 0) {
            emWepArrowBomb(em);
            return;
        }
        w->fuse--;
        if (w->timer) {
            w->timer--;
        } else {
            w->timer = w->timer2;
            if (w->timer2 > 3) {
                w->timer2 -= 2;
            }
            SndCall(8, 0x14, &em->pos, 0x39, 0, em);
        }
        return;
    }
    w->spd.y -= 0.0f;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    if (w->timer3) {
        w->timer3--;
    } else if (EatMgr.hitCheck(&em->oldPos, &em->pos, &hit, 0, 0, 0x404000)) {
        if (w->seHitWall[0] != 0xFF && w->seHitWall[1] != 0xFF && w->inWater == 0) {
            SndCall(w->seHitWall[0], w->seHitWall[1], &em->pos, w->seHitWall[2], 0, em);
        }
        SndStop(w->sndId, 0);
        em->pos = hit;
        TransMatrix(em->mat, &em->pos);
        em->partsWorldCalc();
        em->xFE = 2;
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        return;
    }
    if (w->pAtk) {
        part = (EmHitInfo*) EmAtkLineHitCk(&em->oldPos, &em->pos, &hit, &nrm, 0);
        if (part) {
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
            // `mr r3,part` is the LAST argument move in the original (part does not die there).
            asm("" : "=m"(hit) : "r"(part));  // COMPILER-DIFF: #13 (keep-alive)
        } else {
            part = EmAtkLineHitCkSub(&em->oldPos, &em->pos, &hit, &nrm);
            if (part == 0) {
                goto fly;
            }
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
            EmAtkSetDamageSub(part, w->pAtk, &em->oldPos, &em->pos);
            asm("" : "=m"(hit) : "r"(part));  // COMPILER-DIFF: #13 (keep-alive)
        }
        emWepArrowBomb(em);
        EffectEspDelete(0, w->espKind, (u32) em, 0);
        EffectEspgenDelete(0, w->espKind, (int) em);
        EffectEfmDelete(0, w->espKind, (int) em);
        return;
    }
fly:
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    if (w->inWater == 0 && CheckInWater(em, 0)) {
        if (w->effWater[0] != 0xFF && w->effWater[1] != 0xFF) {
            EstSet(0, -1, &em->pos, 0, w->effWater[0], w->effWater[1], 0, 0, 0, 0);
        }
        SndCall(6, 0x17, &em->pos, 0, 0, em);
        w->inWater = 1;
    }
}

void emWep_R1_Rocket(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec d;
    Mtx m;
    Vec hit;
    cModel* p;

    switch (em->xFE) {
    case 0:
        w->timer = 0;
        w->timer2 = 500;
        w->timer3 = 2;
        w->x1BC = 0.0f;
        w->rocketSpd = 0.0f;
        em->xFE++;
    case 1:
        if (w->timer) {
            w->timer--;
        } else {
            w->timer = w->seThrow[3];
            if (w->seThrow[0] != 0xFF && w->seThrow[1] != 0xFF && w->inWater == 0) {
                w->sndId = SndCall(w->seThrow[0], w->seThrow[1], &em->pos, w->seThrow[2], 0, em);
            }
        }
        w->rocketSpd += 3.0f;
        if (w->rocketSpd > 30.0f) {
            w->rocketSpd = 30.0f;
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
    }
    w->spd.y -= 0.0f;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    if (w->timer3 == 0) {
        if (EatMgr.hitCheck(&em->oldPos, &em->pos, &hit, 0, 0, 0x4000)) {
            emWepRocketBobm(em);
            return;
        }
    } else {
        w->timer3--;
    }
    p = pPL->getPartsPtr(2);
    if ((em->pos.x - p->worldPos.x) * (em->pos.x - p->worldPos.x) + (em->pos.y - p->worldPos.y) * (em->pos.y - p->worldPos.y)
            + (em->pos.z - p->worldPos.z) * (em->pos.z - p->worldPos.z) < 250000.0f) {
        emWepRocketBobm(em);
        return;
    }
    if (pSUB) {
        p = pSUB->getPartsPtr(2);
        if ((em->pos.x - p->worldPos.x) * (em->pos.x - p->worldPos.x) + (em->pos.y - p->worldPos.y) * (em->pos.y - p->worldPos.y)
                + (em->pos.z - p->worldPos.z) * (em->pos.z - p->worldPos.z) < 250000.0f) {
            emWepRocketBobm(em);
            return;
        }
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
}

void emWepRocketBobm(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Camera* cam = &pG->Cam;
    cModel* p;
    Vec r;
    Vec pos;
    f32 len;

    SndStop(w->sndId, 0);
    EffectEspDelete(0, w->espKind, (u32) em, 0);
    EffectEspgenDelete(0, w->espKind, (int) em);
    EffectEfmDelete(0, w->espKind, (int) em);
    p = em->getPartsPtr(0);
    len = (cam->param.pos.x - p->worldPos.x) * (cam->param.pos.x - p->worldPos.x)
        + (cam->param.pos.y - p->worldPos.y) * (cam->param.pos.y - p->worldPos.y)
        + (cam->param.pos.z - p->worldPos.z) * (cam->param.pos.z - p->worldPos.z);
    r.x = 0.0f;
    r.y = GetXZAngle(&p->worldPos, &cam->param.pos);
    r.z = 0.0f;
    if (len < 16000000.0f) {
        EstSet(0, -1, &em->pos, &r, 0x10, 0x48, 0, 0, 0, 0);
    } else {
        EstSet(0, -1, &em->pos, &r, 0x10, 0x41, 0, 0, 0, 0);
    }
    em->hp = 0;
    if (w->pOwner) {
        SndCall(8, 0x96, &em->pos, w->pOwner->id, 0, em);
    }
    BitOn(pG->flags_500C, 0x800000);
    pos = em->oldPos;
    pos.y += 1200.0f;
    PlWepHitCheck2(0, &pos, &pos, 0x13, 3, 5000.0f);
    BitOn(pG->flags_5010, 0x20000000);
    memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &em->oldPos, sizeof(Vec));
    pG->bell_stat = 1;
    em->setLost();
}

void emWepArrowBomb(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec pos;

    SndStop(w->sndId, 0);
    EffectEspDelete(0, w->espKind, (u32) em, 0);
    EffectEspgenDelete(0, w->espKind, (int) em);
    EffectEfmDelete(0, w->espKind, (int) em);
    EstSet(0, -1, &em->pos, 0, 0, 0xD, 0, 0, 0, 0);
    EstSet(0, -1, &em->pos, 0, 0, 0x1A, 0, 0, 0, 0);
    em->hp = 0;
    SndCall(8, 0x15, &em->pos, 0x39, 0, em);
    BitOn(pG->flags_500C, 0x800000);
    pos = em->pos;
    pos.y += 1200.0f;
    PlWepHitCheck2(0, &pos, &pos, 0x13, 3, 5000.0f);
    BitOn(pG->flags_5010, 0x20000000);
    memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &em->pos, sizeof(Vec));
    pG->bell_stat = 1;
    em->setLost();
}

void emWep_R1_BombThrow(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec d;
    Vec nrm;
    f32 spd;
    f32 ang;
    f32 len;
    f32 rad;

    switch (em->xFE) {
    case 0:
        w->bounce = 1;
        w->timer = 0;
        w->timer2 = 0;
        em->xFE++;
    case 1:
        if (w->timer2 % 6 == 0 && w->pOwner) {
            SndCall(8, 0x95, &em->pos, w->pOwner->id, 0, em);
        }
        w->timer2++;
        break;
    }
    if (w->fuse) {
        w->fuse--;
    }
    if (w->fuse == 0) {
        GlobalWork* g = pG;
        Camera* cam = &g->Cam;
        cModel* p;
        Vec r;
        Vec pos;
        f32 dist;

        p = em->getPartsPtr(0);
        dist = (cam->param.pos.x - p->worldPos.x) * (cam->param.pos.x - p->worldPos.x)
            + (cam->param.pos.y - p->worldPos.y) * (cam->param.pos.y - p->worldPos.y)
            + (cam->param.pos.z - p->worldPos.z) * (cam->param.pos.z - p->worldPos.z);
        r.x = 0.0f;
        r.y = GetXZAngle(&p->worldPos, &cam->param.pos);
        r.z = 0.0f;
        if (dist < 16000000.0f) {
            EstSet(0, -1, &em->pos, &r, 0x10, 0x48, 0, 0, 0, 0);
        } else {
            EstSet(0, -1, &em->pos, &r, 0x10, 0x41, 0, 0, 0, 0);
        }
        em->hp = 0;
        if (w->pOwner) {
            SndCall(8, 0x96, &em->pos, w->pOwner->id, 0, em);
        }
        BitOn(pG->flags_500C, 0x800000);
        pos = em->pos;
        pos.y += 1200.0f;
        PlWepHitCheck2(0, &pos, &pos, 0x13, 3, 5000.0f);
        BitOn(pG->flags_5010, 0x20000000);
        memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &em->pos, sizeof(Vec));
        pG->bell_stat = 1;
        em->setLost();
        return;
    }
    w->spd.y -= 15.0f;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &em->oldPos, &em->pos, 100.0f, 0x2001, 0x4000);
    if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
        spd = RootSumSquare3(&w->spd);
        C_VECReflect(&w->spd, &nrm, &d);
        PSVECScale(&d, &w->spd, spd * 0.5f);
        if (w->bounce) {
            w->bounce = 0;
            SndCall(5, 6, &em->pos, 0, 0, em);
        }
        if (w->seHitWall[0] != 0xFF && w->seHitWall[1] != 0xFF && w->inWater == 0) {
            SndCall(w->seHitWall[0], w->seHitWall[1], &em->pos, w->seHitWall[2], 0, em);
        }
        SndStop(w->sndId, 0);
    }
    {
        Vec up;
        Mtx m;
        Vec fwd;

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
#line 2096 "D:/Bio4/Prog/emwep.cpp"
        VECNormalize(&fwd, &fwd);
        ang = acosf(PSVECDotProduct(&up, &fwd));
        if (ang > 0.01f && ang < 3.1315927f) {
            len = SQRTF(w->spd.x * w->spd.x + w->spd.y * w->spd.y + w->spd.z * w->spd.z);
            if (len > 200.0f) {
                len = 200.0f;
            }
            rad = len * 0.005f * 0.62831855f;
            PSVECCrossProduct(&up, &fwd, &up);
            PSMTXRotAxisRad(m, &up, rad);
            PSMTXConcat(m, em->mat, em->mat);
        }
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
    if (w->inWater == 0 && CheckInWater(em, 0)) {
        if (w->effWater[0] != 0xFF && w->effWater[1] != 0xFF) {
            EstSet(0, -1, &em->pos, 0, w->effWater[0], w->effWater[1], 0, 0, 0, 0);
        }
        SndCall(6, 0x17, &em->pos, 0, 0, em);
        w->inWater = 1;
        em->setFall(0, 0, 20.0f);
    }
}

void emWep_R1_FlashThrow(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec d;
    Vec nrm;
    Mtx m;
    Vec up;
    Vec fwd;
    f32 spd;
    f32 ang;
    f32 len;
    f32 rad;
    int dead;

    switch (em->xFE) {
    case 0:
        em->hp = 0;
        w->timer = 0;
        w->timer2 = 0;
        w->bounce = 1;
        em->xFE++;
    case 1:
        w->timer2++;
        break;
    }
    if (w->fuse) {
        w->fuse--;
    }
    if (w->fuse == 0) {
        EstSet(0, -1, &em->pos, 0, 0x2F, 5, 0, 0, 0, 0);
        EstSet(0, -1, 0, 0, 0x2F, 6, 0, 0, 0, 0);
        SndCall(1, 0x13, &em->pos, 0, 0, 0);
        if ((s16) pG->pl_life > 0) {
            dead = 1;
            if (!(pPL->flags_324 & 0xFFFF0000)) {
                dead = 0;
            }
            if (dead == 0) {
                PlSetDamage(9, 0, 0);
            }
        }
        em->setLost();
        return;
    }
    w->spd.y -= 15.0f;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &em->oldPos, &em->pos, 100.0f, 0x2001, 0x4000);
    if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
        spd = RootSumSquare3(&w->spd);
        C_VECReflect(&w->spd, &nrm, &d);
        PSVECScale(&d, &w->spd, spd * 0.5f);
        if (w->bounce) {
            w->bounce = 0;
            SndCall(5, 6, &em->pos, 0, 0, em);
        }
        if (w->seHitWall[0] != 0xFF && w->seHitWall[1] != 0xFF && w->inWater == 0) {
            SndCall(w->seHitWall[0], w->seHitWall[1], &em->pos, w->seHitWall[2], 0, em);
        }
        SndStop(w->sndId, 0);
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
#line 2232 "D:/Bio4/Prog/emwep.cpp"
    VECNormalize(&fwd, &fwd);
    ang = acosf(PSVECDotProduct(&up, &fwd));
    if (ang > 0.01f && ang < 3.1315927f) {
        len = SQRTF(w->spd.x * w->spd.x + w->spd.y * w->spd.y + w->spd.z * w->spd.z);
        if (len > 200.0f) {
            len = 200.0f;
        }
        rad = len * 0.005f * 0.62831855f;
        PSVECCrossProduct(&up, &fwd, &up);
        PSMTXRotAxisRad(m, &up, rad);
        PSMTXConcat(m, em->mat, em->mat);
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
}

void emWep_R1_GrenadeThrow(cEmWep* em)
{
    EmWepWork* w = EMWEP_WK(em);
    Vec d;
    Vec nrm;
    f32 spd;
    f32 ang;
    f32 len;
    f32 rad;

    switch (em->xFE) {
    case 0:
        w->bounce = 1;
        w->timer = 0;
        w->timer2 = 0;
        w->escaped = 0;
        em->xFE++;
    case 1:
        w->timer2++;
        break;
    }
    if (w->fuse) {
        w->fuse--;
    }
    if (w->fuse == 0) {
        Vec pos;

        EstSet(0, -1, &em->pos, 0, 0, 0xD, 0, 0, 0, 0);
        EstSet(0, -1, &em->pos, 0, 0, 0x1A, 0, 0, 0, 0);
        em->hp = 0;
        SndCall(1, 0x14, &em->pos, 0, 0, em);
        BitOn(pG->flags_500C, 0x800000);
        pos = em->pos;
        pos.y += 1200.0f;
        PlWepHitCheck2(0, &pos, &pos, 0x13, 3, 5000.0f);
        BitOn(pG->flags_5010, 0x20000000);
        memcpy((u8*) pG + ((u32) &((GlobalWork*) 0)->bell_pos), &em->pos, sizeof(Vec));
        pG->bell_stat = 1;
        em->setLost();
        return;
    }
    if (w->fuse <= 0x18 && em->plDist2 < 36000000.0f && w->escaped == 0) {
        ActBtn.set(0x25, 0xB, (int) emWepEscapeAction, (int) em, 1, 3, 0, 0);
    }
    w->spd.y -= 15.0f;
    PSVECAdd(&em->pos, &w->spd, &em->pos);
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    EatMgr.adjust(&nrm, &em->oldPos, &em->pos, 100.0f, 0x2001, 0x4000);
    if (nrm.x != 0.0f || nrm.y != 0.0f || nrm.z != 0.0f) {
        spd = RootSumSquare3(&w->spd);
        C_VECReflect(&w->spd, &nrm, &d);
        PSVECScale(&d, &w->spd, spd * 0.5f);
        if (w->bounce) {
            w->bounce = 0;
            SndCall(5, 6, &em->pos, 0, 0, em);
        }
        if (w->seHitWall[0] != 0xFF && w->seHitWall[1] != 0xFF && w->inWater == 0) {
            SndCall(w->seHitWall[0], w->seHitWall[1], &em->pos, w->seHitWall[2], 0, em);
        }
        SndStop(w->sndId, 0);
    }
    {
        Mtx m;
        Vec up;
        Vec fwd;

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
#line 2366 "D:/Bio4/Prog/emwep.cpp"
        VECNormalize(&fwd, &fwd);
        ang = acosf(PSVECDotProduct(&up, &fwd));
        if (ang > 0.01f && ang < 3.1315927f) {
            len = SQRTF(w->spd.x * w->spd.x + w->spd.y * w->spd.y + w->spd.z * w->spd.z);
            if (len > 200.0f) {
                len = 200.0f;
            }
            rad = len * 0.005f * 0.62831855f;
            PSVECCrossProduct(&up, &fwd, &up);
            PSMTXRotAxisRad(m, &up, rad);
            PSMTXConcat(m, em->mat, em->mat);
        }
    }
    TransMatrix(em->mat, &em->pos);
    em->partsWorldCalc();
}

// Action button of the grenade: the player dodges according to where the grenade lies.
void emWepEscapeAction(cEmWep* em)
{
    f32 ang;
    f32 a;

    EMWEP_WK(em)->escaped = 1;
    ang = Muku(&pPL->pos, &em->pos, pPL->rot.y, 3.1415927f);
    a = fabsf(ang);
    if (a < 0.7853982f) {
        SetPlDamage((int) em, plemBackjump);
    } else if (a < 2.3561945f) {
        SetPlDamage((int) em, plemFrontEscape);
    } else if (ang > 0.0f) {
        SetPlDamage((int) em, plemEscape);
    } else {
        SetPlDamage((int) em, plemEscape);
        pPL->xFF = 1;
        GameAddPoint(9);
    }
}

// Player damage routine: runs away from the grenade.
static void plemEscape(cPlayer* pl)
{
    EmWepWork* w = EMWEP_WK(PL_WEP(pl));

    pl->x378 = PL_WEP(pl)->x378;
    pl->st.x325 = 2;
    switch (pl->xFE) {
    case 0:
        if (pl->xFF) {
            MotionSetCore(pl, &pl->pMotion, w->motEscape, (int) w->motEscape2, 3, 0x41, 0);
        } else {
            MotionSetCore(pl, &pl->pMotion, w->motEscape, (int) w->motEscape2, 3, 1, 0);
        }
        SndCall(1, 0x48, &pl->pos, 0, 0, pl);
        SndCall(1, 0x11, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        pl->x3E0 = 50;
        pl->x3E4 = 15;
        pl->xFE++;
    case 1:
        emWepEscapeCamMove(PL_WEP(pl));
        if (pl->x3E4 && w->pOwner) {
            pl->rot.y += Muku(&pl->pos, &w->pOwner->pos, pl->rot.y, 0.19634955f);
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
        }
        MotionMove(pl, 0);
        if (pl->frame > 11.7f && pl->frame < 12.3f) {
            EstSet(0, -1, &pl->pos, 0, 3, 0x13, 0, 0, 0, 0);
            SndCall(5, 5, &pl->pos, 0, 0, pl);
        }
        if (pl->x3E0) {
            pl->x3E0--;
        } else {
            EndPlDamage();
        }
        break;
    }
    pl->x378 = pl->x37C;
}

// Player damage routine: back jump away from the grenade.
void plemBackjump(cPlayer* pl)
{
    EmWepWork* w = EMWEP_WK(PL_WEP(pl));

    pl->x378 = PL_WEP(pl)->x378;
    pl->st.x325 = 0x1E;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, &pl->pMotion, w->motBackjump, 0, 3, 1, 5);
        EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, 0);
        SndCall(1, 0x43, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        SndCall(1, 0x44, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        GameAddPoint(0xB);
        pl->x3E0 = 35;
        pl->x3E4 = 0;
        pl->xFE++;
    case 1:
        if (pl->x3E0) {
            pl->x3E0--;
        } else if (Key.on & 0x1F) {
            pl->x3E4 = 1;
        }
        if (pl->frame > 10.7f && pl->frame < 11.3f) {
            SndCall(1, 0x4F, &pl->pos, 0, 0, pl);
        }
        if (pl->frame > 21.7f && pl->frame < 22.3f) {
            SndCall(5, 0x14, &pl->pos, 0, 0, pl);
        }
        if ((pl->frame > 36.7f && pl->frame < 37.3f) || (pl->frame > 49.7f && pl->frame < 50.3f)) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if ((pl->frame > 37.7f && pl->frame < 38.3f) || (pl->frame > 50.7f && pl->frame < 51.3f)) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMove(pl, 0) || pl->x3E4) {
            EndPlDamage();
        }
        break;
    }
    pl->x378 = pl->x37C;
}

// Player damage routine: dive forward over the grenade.
void plemFrontEscape(cPlayer* pl)
{
    EmWepWork* w = EMWEP_WK(PL_WEP(pl));

    pl->x378 = PL_WEP(pl)->x378;
    pl->st.x325 = 0x1E;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, &pl->pMotion, w->motFront, 0, 3, 1, 5);
        EstSet((int) pl, -1, 0, 0, 3, 0x14, 0, 0, (u32) pl, 0);
        SndCall(1, 0x43, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        SndCall(1, 0x44, &pl->getPartsPtr(4)->worldPos, 0, 0, pl);
        GameAddPoint(0xB);
        pl->x3E0 = 35;
        pl->x3E4 = 0;
        pl->xFE++;
    case 1:
        if (pl->x3E0) {
            pl->x3E0--;
        } else if (Key.on & 0x1F) {
            pl->x3E4 = 1;
        }
        if (pl->frame > 21.7f && pl->frame < 22.3f) {
            SndCall(5, 2, &pl->pos, 0, 0, pl);
        }
        if (pl->frame > 34.7f && pl->frame < 35.3f) {
            SndCall(5, 3, &pl->pos, 0, 0, pl);
        }
        if (MotionMove(pl, 0) || pl->x3E4) {
            EndPlDamage();
        }
        break;
    }
    pl->x378 = pl->x37C;
}

// Camera of the grenade escape: behind the player, pulled in front of the scenery.
void emWepEscapeCamMove(cEmWep* em)
{
    GlobalWork* g = pG;
    EmWepWork* w = EMWEP_WK(em);
    Vec p0;
    Vec p1;
    Vec hit;
    Vec d;
    f32 len;

    // Store through a cast pointer (no MEM_IN_STRUCT_P): the store may alias the `pPL` load below,
    // which keeps `lwz pPL` after it and ranks the `w` chain above the constant-pool `lis`es.
    *(f32*) (u8*) &w->cam.param.fovy = g->Cam.param.fovy;
    p0.x = -376.0f;
    p0.y = 575.0f;
    p0.z = -1831.0f;
    p1.x = -244.0f;
    p1.y = 809.0f;
    p1.z = 52.6f;
    PSMTXMultVec(pPL->mat, &p0, &p0);
    PSMTXMultVec(pPL->mat, &p1, &p1);
    PosToPos(&g->Cam.param.at, &p1, &w->cam.param.at, 1.0f);
    PosToPos(&g->Cam.param.pos, &p0, &w->cam.param.pos, 1.0f);
    if (EatMgr.hitCheck(&w->cam.param.at, &w->cam.param.pos, &hit, 0, 0x8000, 0)) {
        PSVECSubtract(&hit, &w->cam.param.at, &d);
        len = SQRTF(d.x * d.x + d.y * d.y + d.z * d.z) - 250.0f;
#line 2682 "D:/Bio4/Prog/emwep.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, len);
        PSVECAdd(&w->cam.param.at, &d, &w->cam.param.pos);
    }
    len = (w->cam.param.pos.x - w->cam.param.at.x) * (w->cam.param.pos.x - w->cam.param.at.x)
        + (w->cam.param.pos.y - w->cam.param.at.y) * (w->cam.param.pos.y - w->cam.param.at.y)
        + (w->cam.param.pos.z - w->cam.param.at.z) * (w->cam.param.pos.z - w->cam.param.at.z);
    w->cam.up.x = 0.0f;
    w->cam.up.y = 1.0f;
    w->cam.up.z = 0.0f;
    w->cam.dist = SQRTF(len);
    CameraSetOrientationUp(&w->cam);
    CamCtrl.x250 = (s32) &w->cam;
}

void cEmWep::setParent(cEm* parent, int partsNo_, int flag)
{
    EmWepWork* w = EMWEP_WK(this);

    w->pParent = parent;
    w->partsNo = partsNo_;
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

// Drops the weapon: it falls as a three-node rope (emWep_R1_Fall) with `spd` as the initial
// speed of the nodes (random when NULL).
void cEmWep::setFall(int type_, Vec* spd, f32 grav)
{
    EmWepWork* w = EMWEP_WK(this);
    Mtx m;
    Vec v;
    f32 ang;
    u32 i;

    pMotion = 0;
    for (i = 0; i < 3; i++) {
        if (spd) {
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
            w->pt[i].x = fRand1_1() * 10.0f;
            w->pt[i].y = fRand1_1() * 10.0f + 50.0f;
            w->pt[i].z = fRand1_1() * 10.0f;
        }
    }
    w->fallType = type_;
    w->pParent = 0;
    w->pOwner = 0;
    hp = 0;
    w->grav = grav;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    Matrix2AxisAngle(mat, &rot);
    xFC = 1;
    xFD = 4;
    xFE = 0;
    xFF = 0;
}

// Throws the weapon with speed `spd` (a random forward throw in the parent's frame when NULL).
void cEmWep::setThrow(Vec* spd, f32 grav, EmAtkInfo* atk)
{
    EmWepWork* w = EMWEP_WK(this);
    Vec v;
    Mtx m;
    register f64 hd asm("fr1"); // COMPILER-DIFF: #8

    // COMPILER-DIFF: #8 -- the original ranks `fmr f30,f1` (grav) after `mr r26,r5; addi w`, i.e. as
    // if f1 did not die at the copy; the DFmode read of f1 keeps it live past the copy (see
    // emshield setFall / docs/matching.md #8).
    asm("" : "=m"(hp) : "f"(hd));
    if (spd) {
        v = *spd;
    } else {
        v.x = fRand1_1() * 10.0f + 20.0f;
        v.y = fRand1_1() * 10.0f + 75.0f;
        v.z = fRand1_1() * 10.0f + 350.0f;
        if (w->pParent) {
            PSMTXMultVecSR(w->pParent->mat, &v, &v);
        } else {
            PSMTXMultVecSR(mat, &v, &v);
        }
    }
    w->spd.x = v.x;
    w->spd.y = v.y;
    w->spd.z = v.z;
    w->grav = grav;
    rot.x = 0.0f;
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &rot);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    oldPos = pos;
    if (w->pParent) {
        w->pOwner = w->pParent;
    }
    w->pParent = 0;
    hp = 1;
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emWepAtk;
    }
    xFC = 1;
    xFD = 5;
    xFE = 0;
    xFF = 0;
}

// Scythe throw: flies straight (no gravity) spinning about its axis (emWep_R1_ThrowScythe).
void cEmWep::setThrowScythe(Vec* spd, EmAtkInfo* atk)
{
    EmWepWork* w = EMWEP_WK(this);
    Vec v;

    if (spd) {
        v = *spd;
    } else {
        v.x = fRand1_1() * 10.0f + 20.0f;
        v.y = fRand1_1() * 10.0f + 75.0f;
        v.z = fRand1_1() * 10.0f + 350.0f;
        if (w->pParent) {
            PSMTXMultVecSR(w->pParent->mat, &v, &v);
        } else {
            PSMTXMultVecSR(mat, &v, &v);
        }
    }
    w->spd.x = v.x;
    w->spd.y = v.y;
    w->spd.z = v.z;
    w->grav = 15.0f;
    rot.x = 0.0f;
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    oldPos = pos;
    if (w->pParent) {
        w->pOwner = w->pParent;
    }
    w->pParent = 0;
    hp = 1;
    setYarareCubeF(this, 1500.0f, 1500.0f, 1500.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emWepAtk;
    }
    xFC = 1;
    xFD = 0xA;
    xFE = 0;
    xFF = 0;
}

// Shoots the weapon along `spd` (emWep_R1_Shot): it sticks into the player on a hit.
void cEmWep::setShot(Vec* spd, EmAtkInfo* atk)
{
    EmWepWork* w = EMWEP_WK(this);
    Vec v;
    Mtx m;
    f32 len;

    if (spd) {
        v = *spd;
    } else {
        v.x = fRand1_1() * 10.0f + 20.0f;
        v.y = fRand1_1() * 10.0f + 75.0f;
        v.z = fRand1_1() * 10.0f + 350.0f;
        if (w->pParent) {
            PSMTXMultVecSR(w->pParent->mat, &v, &v);
        } else {
            PSMTXMultVecSR(mat, &v, &v);
        }
    }
    w->spd.x = v.x;
    w->spd.y = v.y;
    w->spd.z = v.z;
    len = SQRTF(v.x * v.x + v.z * v.z);
    rot.x = -atan2f(v.y, len);
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    w->grav = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &rot);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    oldPos = pos;
    if (w->pParent) {
        w->pOwner = w->pParent;
    }
    hp = 1;
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emWepAtk;
    }
    xFC = 1;
    xFD = 6;
    xFE = 0;
    xFF = 0;
}

// Shoots an (explosive) arrow (emWep_R1_ShotArrow).
void cEmWep::setShotArrow(Vec* spd, EmAtkInfo* atk)
{
    EmWepWork* w = EMWEP_WK(this);
    Vec v;
    Mtx m;
    f32 len;

    if (spd) {
        v = *spd;
    } else {
        v.x = fRand1_1() * 10.0f + 20.0f;
        v.y = fRand1_1() * 10.0f + 75.0f;
        v.z = fRand1_1() * 10.0f + 350.0f;
        if (w->pParent) {
            PSMTXMultVecSR(w->pParent->mat, &v, &v);
        } else {
            PSMTXMultVecSR(mat, &v, &v);
        }
    }
    w->spd.x = v.x;
    w->spd.y = v.y;
    w->spd.z = v.z;
    len = SQRTF(v.x * v.x + v.z * v.z);
    rot.x = -atan2f(v.y, len);
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    w->grav = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &rot);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    oldPos = pos;
    w->pOwner = w->pParent;
    w->pParent = 0;
    hp = 1;
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emWepAtk;
    }
    xFC = 1;
    xFD = 7;
    xFE = 0;
    xFF = 0;
}

// Fires the weapon as a rocket (emWep_R1_Rocket) for `owner`.
void cEmWep::setRocket(cEm* owner, Vec* spd, EmAtkInfo* atk)
{
    EmWepWork* w = EMWEP_WK(this);
    Vec v;
    Mtx m;
    f32 len;

    if (spd) {
        v = *spd;
    } else {
        v.x = fRand1_1() * 10.0f + 20.0f;
        v.y = fRand1_1() * 10.0f + 75.0f;
        v.z = fRand1_1() * 10.0f + 350.0f;
        if (w->pParent) {
            PSMTXMultVecSR(w->pParent->mat, &v, &v);
        } else {
            PSMTXMultVecSR(mat, &v, &v);
        }
    }
    w->spd.x = v.x;
    w->spd.y = v.y;
    w->spd.z = v.z;
    len = SQRTF(v.x * v.x + v.z * v.z);
    rot.x = -atan2f(v.y, len);
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    w->grav = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &rot);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    oldPos = pos;
    w->pOwner = owner;
    hp = 1;
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
    if (atk) {
        w->pAtk = atk;
    } else {
        w->pAtk = &emWepAtk;
    }
    xFC = 1;
    xFD = 8;
    xFE = 0;
    xFF = 0;
}

// Throws the weapon as dynamite with a `fuse` frame fuse (emWep_R1_BombThrow).
void cEmWep::setBombThrow(Vec* spd, int fuse)
{
    EmWepWork* w = EMWEP_WK(this);
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
        } else {
            PSMTXMultVecSR(mat, &v, &v);
        }
    }
    w->grav = 15.0f;
    w->spd.x = v.x;
    w->spd.y = v.y;
    w->spd.z = v.z;
    rot.x = 0.0f;
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &rot);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    oldPos = pos;
    if (w->pParent) {
        w->pOwner = w->pParent;
    }
    w->pParent = 0;
    hp = 1;
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
    w->fuse = fuse;
    w->pAtk = 0;
    xFC = 1;
    xFD = 9;
    xFE = 0;
    xFF = 0;
}

// Throws the weapon as a flash grenade (emWep_R1_FlashThrow).
void cEmWep::setFlashThrow(Vec* spd, int fuse)
{
    EmWepWork* w = EMWEP_WK(this);
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
        } else {
            PSMTXMultVecSR(mat, &v, &v);
        }
    }
    w->grav = 15.0f;
    w->spd.x = v.x;
    w->spd.y = v.y;
    w->spd.z = v.z;
    rot.x = 0.0f;
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &rot);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    oldPos = pos;
    if (w->pParent) {
        w->pOwner = w->pParent;
    }
    w->pParent = 0;
    hp = 0;
    w->fuse = fuse;
    w->pAtk = 0;
    xFC = 1;
    xFD = 0xB;
    xFE = 0;
    xFF = 0;
}

// Throws the weapon as a hand grenade (emWep_R1_GrenadeThrow) with the player's escape motions.
void cEmWep::setGrenadeThrow(Vec* spd, int fuse, void* motEscape, void* motEscape2, void* motBackjump, void* motFront)
{
    EmWepWork* w = EMWEP_WK(this);
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
        } else {
            PSMTXMultVecSR(mat, &v, &v);
        }
    }
    w->grav = 15.0f;
    w->spd.x = v.x;
    w->spd.y = v.y;
    w->spd.z = v.z;
    rot.x = 0.0f;
    rot.y = atan2f(v.x, v.z);
    rot.z = 0.0f;
    pos.x = mat[0][3];
    pos.y = mat[1][3];
    pos.z = mat[2][3];
    RotMatrix(mat, &rot);
    PSMTXRotRad(m, 'z', 1.5707964f);
    PSMTXConcat(mat, m, mat);
    TransMatrix(mat, &pos);
    oldPos = pos;
    if (w->pParent) {
        w->pOwner = w->pParent;
    }
    w->pParent = 0;
    hp = 1;
    setYarareCubeF(this, 400.0f, 800.0f, 400.0f, 0);
    w->motFront = motFront;
    w->fuse = fuse;
    w->motEscape = motEscape;
    w->motEscape2 = motEscape2;
    w->motBackjump = motBackjump;
    w->pAtk = 0;
    xFC = 1;
    xFD = 0xC;
    xFE = 0;
    xFF = 0;
}

void cEmWep::setSeFall(u8 blk, u8 no, u8 vol)
{
    EmWepWork* w = EMWEP_WK(this);

    w->seFall[0] = blk;
    w->seFall[1] = no;
    w->seFall[2] = vol;
    w->seFall[3] = 0;
}

void cEmWep::setSeDamage(u8 blk, u8 no, u8 vol)
{
    EmWepWork* w = EMWEP_WK(this);

    w->seDamage[0] = blk;
    w->seDamage[1] = no;
    w->seDamage[2] = vol;
}

void cEmWep::setSeHit(u8 blk, u8 no, u8 vol)
{
    EmWepWork* w = EMWEP_WK(this);

    w->seHit[0] = blk;
    w->seHit[1] = no;
    w->seHit[2] = vol;
}

void cEmWep::setSeHitWall(u8 blk, u8 no, u8 vol)
{
    EmWepWork* w = EMWEP_WK(this);

    w->seHitWall[0] = blk;
    w->seHitWall[1] = no;
    w->seHitWall[2] = vol;
}

void cEmWep::setSeThrow(u8 blk, u8 no, u8 vol, u8 wait)
{
    EmWepWork* w = EMWEP_WK(this);

    w->seThrow[0] = blk;
    w->seThrow[1] = no;
    w->seThrow[2] = vol;
    w->seThrow[3] = wait;
}

void cEmWep::setSeAlways(u8 blk, u8 no, u8 vol, u8 wait)
{
    EmWepWork* w = EMWEP_WK(this);

    w->alwaysTimer = wait;
    w->seAlways[0] = blk;
    w->seAlways[1] = no;
    w->seAlways[2] = vol;
    w->alwaysWait = wait;
}

void cEmWep::setEffFall(u8 id, u8 type_)
{
    EmWepWork* w = EMWEP_WK(this);

    w->effFall[0] = id;
    w->effFall[1] = type_;
}

void cEmWep::setEffDamage(u8 id, u8 type_)
{
    EmWepWork* w = EMWEP_WK(this);

    w->effDamage[0] = id;
    w->effDamage[1] = type_;
}

void cEmWep::setEffHit(u8 id, u8 type_)
{
    EmWepWork* w = EMWEP_WK(this);

    w->effHit[0] = id;
    w->effHit[1] = type_;
}

void cEmWep::setEffWater(u8 id, u8 type_)
{
    EmWepWork* w = EMWEP_WK(this);

    w->effWater[0] = id;
    w->effWater[1] = type_;
}

void cEmWep::setEffAlways(int id, int type_)
{
    EstSet((int) this, -1, 0, 0, id, type_, 0x800, EMWEP_WK(this)->espKind, (u32) this, 0);
}

void cEmWep::setEffAlways2(u8 id, u8 type_, u8 parts, Vec* ofs, u16 wait)
{
    EmWepWork* w = EMWEP_WK(this);

    w->effAlways[0] = id;
    w->effAlways[1] = type_;
    w->effAlwaysParts = parts;
    w->effAlwaysWait = wait;
    w->effAlwaysTimer = 1;
    w->effAlwaysOfs = *ofs;
}

void cEmWep::setYarare(Vec* size, f32 w, f32 h)
{
    if (size) {
        YarareInit(this, size->x, size->y, size->z, w, h, 0, 1);
    } else {
        YarareInit(this, 0.0f, 0.0f, 0.0f, w, h, 0, 1);
    }
    hp = 1;
}

void cEmWep::setYarareCube(Vec* size, f32 x, f32 y, f32 z)
{
    if (size) {
        YarareInitCube(this, size->x, size->y, size->z, x, y, z, 0, 1);
    } else {
        YarareInitCube(this, 0.0f, -400.0f, 0.0f, x, y, z, 0, 1);
    }
    hp = 1;
}

void cEmWep::setTransMode(int on)
{
    EmWepWork* w = EMWEP_WK(this);

    if (on) {
        w->flags &= ~2;
        be_flag |= 2;
    } else {
        w->flags |= 2;
    }
}

void cEmWep::setAtNo(int no)
{
    EMWEP_WK(this)->sceAtNo = no;
}

void cEmWep::setLost()
{
    xFC = 1;
    be_flag &= ~2;
    xFD = 2;
    hp = 0;
    xFE = 0;
    xFF = 0;
}

void cEmWep::setWaitDrop()
{
    if (EMWEP_WK(this)->timer4) {
        setFall(0, 0, 20.0f);
    }
}

// The scythe took the player's head off: the head becomes an obj01 (blood effects on both).
void emWepPlHeadLost()
{
    Vec p0;
    Vec p1;
    cModel* p;
    cObj* obj;

    if (pSys->region == 0) {
        PlSetDamageSe(0xD);
        EstSet((int) pPL, -1, 0, 0, 0x10, 0x57, 0, 0, (u32) pPL, 0);
        return;
    }
    pPL->setHead(0);
    p = pPL->getPartsPtr(3);
    p0.x = 0.0f;
    p0.y = 68.0f;
    p0.z = 28.0f;
    p1.x = 0.0f;
    p1.y = 50.0f;
    p1.z = -25.0f;
    PSMTXMultVec(p->mat, &p0, &p0);
    PSMTXMultVecSR(pPL->mat, &p1, &p1);
    obj = SetObj01(PL_ARC_PTR(pG->pPlArc, 0xC), PL_ARC_PTR(pG->pPlArc, 7), &p0, &pPL->rot, &p1, 10.0f, 150.0f, 1000, 0x11);
    if (obj) {
        obj->lightInfo.x50 = 1;
        Obj01SetEst(obj, 0, -1, 4, 0, -1, 0, -1, 0, -1);
    }
    EstSet((int) obj, -1, 0, 0, 0x10, 0x46, 0, 0, (u32) obj, 0);
    EstSet((int) pPL, -1, 0, 0, 0x10, 0x45, 0, 0, (u32) pPL, 0);
    SndCall(1, 0x3E, &pPL->pos, 0, 0, pPL);
}

// The shot weapon (a-b) against the vase enemies (id 0x43, types 6/7) in front of the player:
// registers the damage on the hit one. 1 on a hit.
int emWepShotHitVaseCk(Vec* a, Vec* b)
{
    Mtx m;
    Vec hit;
    Vec hitPos;
    Vec d;
    cEm* hitEm;
    EmHitInfo* hitPart;
    f32 len;
    u32 i;
    int r;

    r = EatMgr.hitCheck(a, b, &hit, 0, 0, 0x404000);
    hitEm = 0;
    hitPart = 0;
    if (r == 0) {
        hit = *b;
    }
    len = (a->x - hit.x) * (a->x - hit.x) + (a->y - hit.y) * (a->y - hit.y) + (a->z - hit.z) * (a->z - hit.z);
    PSVECSubtract(b, a, &d);
    if (d.x == d.z) {
        PSMTXIdentity(m);
    } else {
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
    }
    TransMatrix(m, a);
    PSMTXInverse(m, m);
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        EmHitInfo* part;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e->id != 0x43) {
            continue;
        }
        switch (e->type) {
        case 6:
        case 7:
            break;
        default:
            continue;
        }
        PSMTXMultVec(m, &e->getPartsPtr(0)->worldPos, &d);
        if (d.z < -10000.0f) {
            continue;
        }
        if (d.x > 10000.0f) {
            continue;
        }
        if (d.x < -10000.0f) {
            continue;
        }
        if (Front_check(pPL, e, 1.5707964f) == 0) {
            continue;
        }
        part = emLineAtCk(e, a, b, len, 0);
        if (part) {
            part->flags |= 0x4000;
            hitEm = e;
            hitPart = part;
            hit = hitPos;
        }
    }
    if (hitEm) {
        hitEm->dmg.set(0, 10, 0x18, a, hitPart->rad, hitPart);
    }
    return hitEm != 0;
}

// Same for the window enemies (id 0x46).
int emWepShotHitWindowCk(Vec* a, Vec* b)
{
    Mtx m;
    Vec hit;
    Vec hitPos;
    Vec d;
    cEm* hitEm;
    EmHitInfo* hitPart;
    f32 len;
    u32 i;
    int r;

    r = EatMgr.hitCheck(a, b, &hit, 0, 0, 0x404000);
    hitEm = 0;
    hitPart = 0;
    if (r == 0) {
        hit = *b;
    }
    len = (a->x - hit.x) * (a->x - hit.x) + (a->y - hit.y) * (a->y - hit.y) + (a->z - hit.z) * (a->z - hit.z);
    PSVECSubtract(b, a, &d);
    if (d.x == d.z) {
        PSMTXIdentity(m);
    } else {
        PSMTXRotRad(m, 'y', atan2f(d.x, d.z));
    }
    TransMatrix(m, a);
    PSMTXInverse(m, m);
    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* e = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        EmHitInfo* part = 0;

        if ((e->be_flag & 0x201) != 1) {
            continue;
        }
        if (e->hp <= 0) {
            continue;
        }
        if (e->id != 0x46) {
            continue;
        }
        PSMTXMultVec(m, &e->getPartsPtr(0)->worldPos, &d);
        if (d.z < -10000.0f) {
            continue;
        }
        if (d.x > 10000.0f) {
            continue;
        }
        if (d.x < -10000.0f) {
            continue;
        }
        if (Front_check(pPL, e, 1.5707964f) == 0) {
            continue;
        }
        part = emLineAtCk(e, a, b, len, 0);
        if (part) {
            part->flags |= 0x4000;
            hitEm = e;
            hitPart = part;
            hit = hitPos;
        }
    }
    if (hitEm) {
        hitEm->dmg.set(0, 10, 0x18, a, hitPart->rad, hitPart);
    }
    return hitEm != 0;
}

// Chain weapons (whips): the parts hang as a pendulum cloth from `owner`'s collision volumes.
void cEmWep::setCloth(cModel* owner)
{
    EmWepWork* w = EMWEP_WK(this);

    w->cloth.num = 10;
    w->cloth.x08 = 0;
    w->cloth.x0C = 0;
    w->cloth.x10 = 0;
    w->cloth.x14 = 0;
    w->cloth.x2C = 0;
    w->cloth.x30 = 0;
    w->cloth.x20 = 0;
    w->cloth.x24 = 0;
    w->cloth.x44 = 0;
    w->cloth.flags = 0;
    w->cloth.pParts = emWepClothP;
    w->cloth.pUp = emWepClothUp;
    w->cloth.pDown = emWepClothDp;
    w->cloth.pMax = emWepClothMax;
    w->cloth.x34 = emWepAt;
    w->cloth.x58 = owner;
    w->cloth.x38 = 3;
    w->cloth.x3C = 25.0f;
    w->cloth.x40 = 0.6f;
    w->cloth.x48 = 0.0f;
    w->cloth.x4C = 1.0f;
    w->cloth.x50 = 0.0f;
    w->cloth.x54 = 0;
    PenClothSet(this, &w->cloth, 100.0f);
    w->flags |= 4;
}

void cEmWep::moveCloth()
{
    EmWepWork* w = EMWEP_WK(this);
    cModel* p;
    Mtx inv;

    if (w->flags & 4) {
        if (w->cloth.x58 && (w->cloth.x58->be_flag & 0x201) != 1) {
            w->cloth.x58 = 0;
        }
        PenClothMove2(this, &w->cloth);
        for (p = getPartsPtr(1); p; p = p->pParts) {
            PSMTXInverse(p->pParent->mat, inv);
            PSMTXConcat(inv, p->mat, p->worldMat);
        }
    }
}

// Matrix of a weapon hanging on its parent's parts (emWep_R1_Parent; objTrolley calls it with
// noMotion = 1 to skip the motion update).
void cEmWep::setParentMatCalc(int noMotion)
{
    Mtx m;
    Vec v0;
    Vec v1;
    Vec v2;
    EmWepWork* w = EMWEP_WK(this);
    cEm* parent = w->pParent;

    if (parent == 0) {
        return;
    }
    RotMatrix(mat, &rot);
    TransMatrix(mat, &pos);
    ScaleMatrix(mat, &scale);
    if (parent->pParts) {
        PSMTXConcat(parent->getPartsPtr(w->partsNo)->mat, mat, m);
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
#line 4302 "D:/Bio4/Prog/emwep.cpp"
            VECNormalize(&v0, &v0);
            if (v1.x == 0.0f && v1.y == 0.0f && v1.z == 0.0f) {
                v1.y = 1.0f;
            }
#line 4304 "D:/Bio4/Prog/emwep.cpp"
            VECNormalize(&v1, &v1);
            if (v2.x == 0.0f && v2.y == 0.0f && v2.z == 0.0f) {
                v2.z = 1.0f;
            }
#line 4306 "D:/Bio4/Prog/emwep.cpp"
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
        PSMTXCopy(m, mat);
    }
    if (noMotion == 0) {
        if (pMotion) {
            motFlags2 |= 0x40000000;
            MotionMove(this, 0);
        } else {
            partsMatCalc();
        }
    }
    partsWorldCalc();
}
