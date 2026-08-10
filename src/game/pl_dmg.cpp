// game/pl_dmg.cpp: player routine 0 (damage): normal hit, blow-away, blast, death.

#include "atari.h"
#include "light.h"
#include "player.h"
#include "global.h"
#include "db_log.h"
#include "main.h"
#include "pad.h"
#include "snd.h"
#include "esp.h"
#include "math_sub.h"

void MotionSetCore(cModel* m, void* work, void* data, int a, int b, int c, int d);  // game/motion.cpp (result unused here: void keeps the `mr r3` before the arg li`s)
extern "C" {
int MotionCheckCrossFrame(void* work, f32 frame);  // game/motion.cpp
void PlSetDamageSe(int no);                        // game/pl_sub.cpp
void EndPlDamage();                                // game/pl_sub.cpp
int ChkWaterEffectEnable(Vec* pos);                // game/est.cpp
}

void damageNormal(cPlayer* pl);
void damageBlow(cPlayer* pl);
void damageBlast(cPlayer* pl);
void Pl_R0_Die(cPlayer* pl);

// Routine bytes through int parameters: the four zeros become one SImode pseudo that cse cannot
// merge with the QImode zero of `st.x324` stored before the EndPlDamage call, so the original's
// second `li r0, 0` after the call is reproduced instead of a callee-saved zero.
static inline void PlRoutineSet(cPlayer* pl, int r0, int r1, int r2, int r3)
{
    pl->xFC = r0;
    pl->xFD = r1;
    pl->xFE = r2;
    pl->xFF = r3;
}

void Pl_R0_Damage(cPlayer* pl)
{
    static void (*funcTbl[])(cPlayer*) = {
        damageNormal,
        damageBlow,
        damageBlast,
    };

    funcTbl[pl->xFD](pl);
}

void damageNormal(cPlayer* pl)
{
    void* mot = 0;
    void* mot2 = 0;
    f32 ang;
    f32 wh;
    Vec* pos;

    switch (pl->xFE) {
    case 0:
        pl->beginDamage();
        if ((s16) pG->pl_life <= 0) {
            pl->xFF = 6;
        }
        switch (pl->xFF) {
        default:
            pl->xFF = 0;
        case 0:
            mot = PL_ARC_PTR(pG->pPlArc, 0x48);
            pl->x3E0 = 0x1E;
            break;
        case 1:
            mot = PL_ARC_PTR(pG->pPlArc, 0x49);
            pl->x3E0 = 0x23;
            break;
        case 2:
        case 4:
            mot = PL_ARC_PTR(pG->pPlArc, 0x4A);
            mot2 = PL_ARC_PTR(pG->pPlArc, 0x64);
            pl->x3E0 = 0x23;
            break;
        case 3:
        case 5:
            mot = PL_ARC_PTR(pG->pPlArc, 0x4B);
            mot2 = PL_ARC_PTR(pG->pPlArc, 0x65);
            pl->x3E0 = 0x23;
            break;
        case 6:
            mot = PL_ARC_PTR(pG->pPlArc, 0x4C);
            mot2 = PL_ARC_PTR(pG->pPlArc, 0x4D);
            pl->x3E0 = 0x3E7;
            break;
        }
        MotionSetCore(pl, &pl->pMotion, mot, (int) mot2, 5, 1, 0);
        if (pl->x400 != 123.0f) {
            ang = Muku2(pl->rot.y, pl->x400, PI);
            pl->rot.y += ang;
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
            pl->getPartsPtr(0)->rot.y -= ang;
            pl->x3E4 = 1;
        } else {
            pl->x3E4 = 0;
        }
        if (pl->xFF != 6) {
            PlSetDamageSe(0);
        }
        pl->setFace(1);
        pl->xFE = 1;
        pl->st.x325 |= 0x80;
    case 1:
        if (pl->frame > 19.7f && pl->frame < 20.3f) {
            pl->setFace(0);
        }
        if (pl->x3E0 != 0) {
            pl->x3E0--;
        }
        if (pl->motionMove() != 0 || (pl->x3E0 == 0 && (Key.on & 0x10F))) {
            if (pl->xFF == 6) {
                pl->xFC = 2;
                pl->xFE = 0;
                pl->xFD = 2;
                pl->xFF = 0;
            } else {
                pl->st.x324 = 0;
                pl->st.x325 = 5;
                EndPlDamage();
                PlRoutineSet(pl, 0, 0, 0, 0);
            }
        }
        if (pl->x3E4 != 0) {
            cModel* p = pl->getPartsPtr(0);
            p->rot.y += Muku2(pl->getPartsPtr(0)->rot.y, 0.0f, PI / 10.0f);
        }
        break;
    case 0xA:
        MotionSetCore(pl, &pl->pMotion, PL_ARC_PTR(pG->pPlArc, 0x4F), pG->pPlArc->ofs[0x50] + (u32) pG->pPlArc, 3, 5, 0);
        EstSet((int) pl, -1, 0, 0, 3, ChkWaterEffectEnable(&pl->pos) ? 0x10 : 0xF, 0, 0, (u32) pl, 0);
        pl->xFE = 0xB;
    case 0xB:
        pos = &pl->pos;
        if (pl->frame > 34.7f && pl->frame < 35.3f) {
            SndCall(5, 3, pos, 0, 0, 0);
        }
        if (pl->frame > 52.7f && pl->frame < 53.3f) {
            SndCall(5, 2, pos, 0, 0, 0);
        }
        if (pl->frame > 59.7f && pl->frame < 60.3f) {
            SndCall(1, 0x29, &pl->getPartsPtr(2)->worldPos, 0, 0, 0);
        }
        if (GetWaterHeight(pos, &wh) && wh > pl->pos.y) {
            if (MotionCheckCrossFrame(&pl->pMotion, 48.0f) || MotionCheckCrossFrame(&pl->pMotion, 54.0f) ||
                MotionCheckCrossFrame(&pl->pMotion, 65.0f)) {
                EstSet((int) pl, -1, 0, 0, 1, 0x23, 0, 0, (u32) pl, 0);
            }
        }
        if (GetWaterHeight(pos, &wh) && pl->pParts->worldPos.y < wh) {
            if (MotionCheckCrossFrame(&pl->pMotion, 18.0f)) {
                EstSet((int) pl, -1, 0, 0, 1, 0x24, 0, 0, (u32) pl, 0);
            }
        }
        if (pl->motionMove()) {
            pl->st.x324 = 0;
            pl->st.x325 = 5;
            EndPlDamage();
            PlRoutineSet(pl, 0, 0, 0, 0);
        }
        break;
    default:
        pLog->err(0, 0, "invalid r_no_2 %d", pl->xFE);
        break;
    }
}

void damageBlow(cPlayer* pl)
{
    void* mot;
    void* mot2;
    f32 ang;
    f32 wh;
    Vec* pos;
    int splash;
    u32 dead;
    u32 n;

    switch (pl->xFE) {
    case 0:
        pl->beginDamage();
        dead = 1;
        if ((s16) pG->pl_life > 0) {
            dead = 0;
        }
        BitSet(pl->x3E0, dead);
        if (dead) {
            mot = PL_ARC_PTR(pG->pPlArc, 0x4E);
            mot2 = PL_ARC_PTR(pG->pPlArc, 0x66);
        } else {
            mot = PL_ARC_PTR(pG->pPlArc, 0x51);
            mot2 = PL_ARC_PTR(pG->pPlArc, 0x67);
        }
        MotionSetCore(pl, &pl->pMotion, mot, (int) mot2, 5, 1, 0);
        n = pl->x3E0;
        if (n) {
            EstSet((int) pl, -1, 0, 0, 3, ChkWaterEffectEnable(&pl->pos) ? 0xE : 0xD, 0, 0, (u32) pl, 0);
        } else {
            EstSet((int) pl, -1, 0, 0, 3, ChkWaterEffectEnable(&pl->pos) ? 6 : 5, 0, 0, (u32) pl, (void*) n);
        }
        if (pl->x400 != 123.0f) {
            ang = Muku2(pl->rot.y, pl->x400, PI);
            pl->rot.y += ang;
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
            pl->getPartsPtr(0)->rot.y -= ang;
            pl->x3E4 = 1;
        } else {
            pl->x3E4 = 0;
        }
        if (pl->x3E0 == 0) {
            PlSetDamageSe(0);
        } else {
            SndCall(1, 0x4A, &pl->getPartsPtr(2)->worldPos, 0, 0, 0);
        }
        pl->setFace(1);
        pl->x3E8 = 0;
        pl->xFE = 1;
        pl->st.x325 |= 0x80;
    case 1:
        if (MotionCheckCrossFrame(&pl->pMotion, 20.0f)) {
            pl->setFace(0);
        }
        if (pl->x3E0 == 0 && pl->frame > 9.7f && pl->frame < 10.3f) {
            SndCall(5, 5, &pl->pos, 0, 0, 0);
        }
        if (pl->frame >= 5.0f) {
            splash = pl->x3E8;
            if (splash == 0 && GetWaterHeight(&pl->pParts->worldPos, &wh) && pl->pParts->worldPos.y < wh + 400.0f) {
                pl->x3E8 = 1;
                EstSet((int) pl, -1, 0, 0, 1, 0x24, 0, 0, (u32) pl, (void*) splash);
            }
        }
        if (pl->motionMove()) {
            if (pl->x3E0 != 0) {
                pl->xFC = 2;
                pl->xFD = 2;
                pl->xFE = 0;
                pl->xFF = 0;
            } else {
                pl->xFE = 0xA;
            }
        }
        break;
    case 0xA:
        MotionSetCore(pl, &pl->pMotion, PL_ARC_PTR(pG->pPlArc, 0x4F), pG->pPlArc->ofs[0x50] + (u32) pG->pPlArc, 3, 5, 0);
        EstSet((int) pl, -1, 0, 0, 3, ChkWaterEffectEnable(&pl->pos) ? 0x10 : 0xF, 0, 0, (u32) pl, 0);
        pl->xFE = 0xB;
    case 0xB:
        pos = &pl->pos;
        if (pl->frame > 34.7f && pl->frame < 35.3f) {
            SndCall(5, 3, pos, 0, 0, 0);
        }
        if (pl->frame > 52.7f && pl->frame < 53.3f) {
            SndCall(5, 2, pos, 0, 0, 0);
        }
        if (pl->frame > 59.7f && pl->frame < 60.3f) {
            SndCall(1, 0x29, &pl->getPartsPtr(2)->worldPos, 0, 0, 0);
        }
        if (GetWaterHeight(pos, &wh) && wh > pl->pos.y) {
            if (MotionCheckCrossFrame(&pl->pMotion, 48.0f) || MotionCheckCrossFrame(&pl->pMotion, 54.0f) ||
                MotionCheckCrossFrame(&pl->pMotion, 65.0f)) {
                EstSet((int) pl, -1, 0, 0, 1, 0x23, 0, 0, (u32) pl, 0);
            }
        }
        if (GetWaterHeight(pos, &wh) && pl->pParts->worldPos.y < wh) {
            if (MotionCheckCrossFrame(&pl->pMotion, 18.0f)) {
                EstSet((int) pl, -1, 0, 0, 1, 0x24, 0, 0, (u32) pl, 0);
            }
        }
        if (pl->motionMove()) {
            pl->st.x324 = 0;
            pl->st.x325 = 5;
            EndPlDamage();
            PlRoutineSet(pl, 0, 0, 0, 0);
        }
        break;
    }
}

void damageBlast(cPlayer* pl)
{
    f32 ang;
    int no = pl->xFE;

    switch (no) {
    case 0:
        pl->beginDamage();
        MotionSetCore(pl, &pl->pMotion, PL_ARC_PTR(pG->pPlArc, 0x52), 0, 5, 1, 0);
        if (pl->x400 != 123.0f) {
            ang = Muku2(pl->rot.y, pl->x400, PI);
            pl->rot.y += ang;
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
            pl->getPartsPtr(0)->rot.y -= ang;
            pl->x3E4 = 1;
        } else {
            pl->x3E4 = no;
        }
        pl->setFace(1);
        pl->xFE = 1;
        pl->st.x325 |= 0x80;
    case 1:
        if (pl->frame > 19.7f && pl->frame < 20.3f) {
            pl->setFace(0);
        }
        if (pl->motionMove()) {
            pl->st.x324 = 0;
            pl->st.x325 = 5;
            EndPlDamage();
            PlRoutineSet(pl, 0, 0, 0, 0);
        }
        break;
    }
}

void Pl_R0_Die(cPlayer* pl)
{
    int no = pl->xFD;

    switch (no) {
    case 0:
        pl->beginDamage();
        MotionSetCore(pl, &pl->pMotion, PL_ARC_PTR(pG->pPlArc, 0x4C), pG->pPlArc->ofs[0x4D] + (u32) pG->pPlArc, 5, 1, 0);
        EstSet((int) pl, -1, 0, 0, 3, ChkWaterEffectEnable(&pl->pos) ? 4 : 3, 0, 0, (u32) pl, (void*) no);
        pl->st.x325 |= 0x80;
        if (pl->pBody->pHair) {
            SndCall(1, 0xD, &pl->getPartsPtr(4)->worldPos, 0, 0, 0);
            pl->setFace(1);
        }
        pl->atari.partsNo = 4;
        pl->xFD = 1;
        pl->x3E0 = no;
    case 1:
        if (pl->frame > 39.7f && pl->frame < 40.3f) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 3, 1);
        }
        if (pl->frame > 69.7f && pl->frame < 70.3f) {
            VibSetData((VibDataTbl*) (pG->pArc->ofs_1C + (u32) pG->pArc), 3, 1);
        }
        if (pl->motionMove()) {
            pl->xFD = 2;
        }
        break;
    case 2:
        pl->motionMove();
        break;
    }
}
