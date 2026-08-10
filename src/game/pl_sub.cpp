// game/pl_sub.cpp: player / partner helpers: costume, data reload, damage entry points, partner
// (Ashley) control, ladder, motion registration, key helpers, water effects.

#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "atari.h"
#include "player.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "global.h"
#include "db_log.h"
#include "main.h"
#include "item.h"
#include "snd.h"
#include "esp.h"
#include "obj.h"
#include "act_btn.h"
#include "rnd.h"
#include "math_sub.h"

extern "C" {
void ReleaseWepData();                         // game/read.cpp
void ReadPlayerData(int type, int costume);    // game/read.cpp
void AddWaterPower(Vec* pos, f32 power);       // game/Espgen42.cpp
void* memset(void* dst, int c, unsigned int n);
}
f32 GetDistance(Vec& a, Vec& b);               // game/sub2.cpp (second overload)

extern void (*Pl_func_tbl[7])(cPlayer*);       // game/player.cpp

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

// Stores through references: scalar MEMs, so pG is reloaded after each of them (the original
// reloads pG after every store to a GlobalWork field in this unit).
static inline void U8Set(u8& d, u8 v) { d = v; }
static inline void U16Set(u16& d, u16 v) { d = v; }
static inline void U32Set(u32& d, u32 v) { d = v; }

// Routine bytes of the player set through one fresh load of pPL (the four byte stores share one
// register in the original even right after a call, unlike direct `pPL->xFC = ..` stores).
static inline void PlSetRoutine(int a, int b, int c, int d)
{
    cPlayer* p = pPL;

    p->xFC = a;
    p->xFD = b;
    p->xFE = c;
    p->xFF = d;
}

void PlSelect(int no)
{
    if (pG->x4FB8 != no) {
        s16 life = pG->pl_life_max;
        u32 tmp;

        U16Set(pG->pl_life_max, pG->sub_life_max);
        U16Set(pG->sub_life_max, life);
        pG->pl_life = pG->pl_life_max;
        ReleaseWepData();
        tmp = pG->x4F98;
        U32Set(pG->x4F98, pG->x832C);
        U32Set(pG->x832C, tmp);
    }
    U8Set(pG->x4FB8, no);
    PlSetCostume();
    BitOn16(pG->flags_4FBE, 1);
}

int PlSetCostume()
{
    if ((s32) pG->flags_54 < 0 || (pG->flags_54 & 0x40000000)) {
        return pG->costume;
    }
    if (pG->x4FB8 == 0) {
        if (pG->costume2 != 1) {
            if (ItemMgr.num(0xFE, 0)) {
                U8Set(pG->costume, 2);
            } else if (pG->flags_51BC & 0x200000) {
                U8Set(pG->costume, 1);
            } else {
                U8Set(pG->costume, 0);
            }
        } else {
            U8Set(pG->costume, 3);
        }
    } else {
        U8Set(pG->costume, pG->costume2);
    }
    BitOn16(pG->flags_4FBE, 1);
    return pG->costume;
}

void PlChangeData()
{
    cPlayer* pl;

    pPL->weaponRelease();
    PlDataRelease();
    EspDataRelease(3, 1, 1);
    pPL->push();
    BitOn16(pG->flags_4FBE, 1);
    ReadPlayerData(pG->x4FB8, pG->costume);
    pl = pPL;
    pl->setModel();
    pl->setMotion();
    EspDataLoad((u32) PL_ARC_PTR(pG->pPlArc, 0x1A), 3, 0);
    pPL->weaponInit();
    pl->be_flag |= 0x20;
    pl->xFC = 0;
    pl->xFD = 0;
    pl->xFE = 0;
    pl->xFF = 1;
    pl->x4FD = 0;
    pl->x4FC = 0;
    pl->initCloth();
}

void PlGachaInit()
{
    pPL->gachaCnt = 0;
}

void PlGachaMove()
{
    cPlayer* pl = pPL;

    ActBtn.set(0x2B, 5, 0, 0, 2, 0xB, 0, 0);
    if (Key.trg & 0xF) {
        pl->gachaCnt++;
    }
    if (Key.trg & 0xC0000000) {
        pl->gachaCnt++;
    }
    if (Key.trg & 0x0C000000) {
        pl->gachaCnt++;
    }
}

int PlGachaGet()
{
    int n = pPL->gachaCnt;

    if (pG->x4F88 <= 2) {
        n += n / 2;
    }
    return n;
}

void PlSetDamageSe(int no)
{
    cPlayer* pl = pPL;

    if (no == 0) {
        u8 r = (u32) Rnd() % 3;

        no = r + 9;
    }
    SndCall(1, no, &pl->getPartsPtr(4)->worldPos, 0, 0, 0);
}

u32 PlGetStatus()
{
    cPlayer* pl = pPL;
    u32 st = 0;

    switch (pl->xFC) {
    case 0:
        switch (pl->xFD) {
        case 0:
            st = 1;
            break;
        case 1:
        case 4:
        case 5:
            st |= 2;
            break;
        case 2:
            st = 4;
            break;
        case 3:
            st = 8;
            break;
        case 6:
        case 0xB:
            if (pl->xFE != 0) {
                st |= 0x10;
            }
            switch (pl->xFE) {
            case 1:
                st |= 0x20;
                break;
            case 2:
                st |= 0x40;
                break;
            case 4:
                st |= 0x800;
                break;
            case 6:
                st |= 0x4000;
                break;
            default:
                st |= 0x80000000;
                break;
            }
            break;
        case 0x11:
            st = 0x8000;
            break;
        case 9:
            st = 0x400;
            break;
        case 7:
            st = 0x100;
            break;
        case 0x10:
            st = 0x40000;
            break;
        case 8:
            st = 0x200;
            break;
        case 0x12:
            st = 0x10000;
            break;
        case 0xE:
            st = 0x80000;
            break;
        default:
            st |= 0x80000000;
            break;
        }
        break;
    case 1:
    case 4:
        st |= 0x80;
        break;
    case 2:
        st = 0x2000;
        break;
    default:
        st |= 0x80000000;
        break;
    }
    if (pl->flags_420 & 2) {
        st |= 0x20000;
    }
    return st;
}

void PlSetCrouch()
{
    cPlayer* pl = pPL;

    pl->xFC = 0;
    pl->xFE = 0;
    pl->xFD = 0x11;
    pl->xFF = 0;
}

void PlSetHand(int type, int on)
{
    int t = 1;

    if (type == 1) {
        t = 0;
    }
    pPL->pWep->setTrans(t, on);
}

void SubCharSetHand(int no)
{
    if (pSUB) {
        pSUB->setHand(no);
    }
}

void SetPlDamage(int type, void (*func)(cPlayer*))
{
    cPlayer* pl = pPL;

    pl->beginDamage();
    Pl_func_tbl[4] = func;
    PlSetRoutine(4, 0, 0, 0);
    pl->dmg.set(0, 10);
    pl->dmgType = type;
}

void EndPlDamage()
{
    cPlayer* pl = pPL;
    cAtariInfo* at = &pl->atari;

    pl->dmg.clear();
    PlSetRoutine(0, 0, 0, 0);
    pl->x378 = pl->x37C;
    at->throughOff();
    at->setPriority(0);
    at->set(10, 400.0f, 200.0f);
    pl->endDamage();
}

void SetSubAux(int a, int b)
{
    cSubChar* sub = pSUB;

    if (sub == 0) {
        pLog->err(0, 0, "ERROR: SetSubAux() ASHLEY NOT FOUND.");
        return;
    }
    sub->xFC = 0;
    sub->xFE = 0;
    sub->subAux0 = a;
    sub->subAux1 = b;
    sub->xFD = 0xF;
    sub->xFF = 0;
}

void SetSubBulldozer(int a, int b)
{
    cSubChar* sub = pSUB;

    if (sub == 0) {
        pLog->err(0, 0, "ERROR: SetSubAux() ASHLEY NOT FOUND.");
        return;
    }
    sub->xFD = 0;
    sub->xFE = 0;
    sub->subAux0 = a;
    sub->subAux1 = b;
    sub->xFC = 3;
    sub->xFF = 0;
}

void SetSubDamage(int type, void* mot)
{
    cSubChar* sub = pSUB;

    if (sub == 0) {
        return;
    }
    if (sub->id == 3) {
        sub->setEmFunc();
        sub->xFC = 4;
        sub->xFD = 0;
        sub->xFE = 0;
        sub->xFF = 0;
        sub->dmg.set(0, 10);
        sub->dmgType = type;
    } else {
        sub->subMot0 = mot;
        sub->subFlags58C |= 0x40;
        sub->xFC = 4;
        sub->xFD = 0;
        sub->xFE = 0;
        sub->xFF = 0;
        sub->dmg.set(0, 10);
        sub->dmgType = type;
    }
}

void EndSubDamage()
{
    cSubChar* sub = pSUB;
    cAtariInfo* at;

    if (sub == 0) {
        return;
    }
    sub->dmg.clear();
    if (sub->id == 4) {
        pSUB->modelSet();
    } else {
        sub->endDamage();
    }
    sub->x378 = sub->x37C;
    sub->xFC = 0;
    sub->xFD = 0;
    sub->xFE = 0;
    sub->xFF = 0;
    at = &sub->atari;
    at->throughOff();
    at->setPriority(0);
    at->set(10, 400.0f, 200.0f);
}

void SubCharInit(int type, Vec* pos, f32 ang)
{
    cSubChar* sub;

    if (pSUB != 0) {
        return;
    }
    switch (type) {
    case 0:
    default:
        sub = (cSubChar*) EmMgr.createBack(2);
        break;
    case 1:
        if (pG->costume2 != 1) {
            sub = (cSubChar*) EmMgr.createBack(3);
        } else {
            sub = (cSubChar*) EmMgr.createBack(5);
        }
        if (sub) {
            sub->id = 3;
        }
        break;
    case 2:
        sub = (cSubChar*) EmMgr.createBack(4);
        break;
    }
    if (!VALID_PTR(sub)) {
        pLog->err(0, 0, "SubCharInit() failed.");
        pSUB = 0;
        return;
    }
    sub->setPos(pos);
    {
        Vec rot;

        rot.x = 0.0f;
        rot.z = 0.0f;
        rot.y = ang;
        sub->setAng(&rot);
    }
    if (pos->x == pPL->pos.x && pos->z == pPL->pos.z) {
        sub->pos.x += 1.0f;
        sub->pos.z += 2.0f;
    }
    sub->flags_3C8 |= 1;
    pSUB = sub;
}

void SubCharCtrl(int mode, int flag)
{
    cSubChar* sub = pSUB;

    if (sub == 0) {
        return;
    }
    if (sub->hp <= 0) {
        return;
    }
    switch (mode) {
    case 0:
        sub->control(1);
        if (flag & 1) {
            sub->xFC = 0;
            sub->xFD = 0;
            sub->xFE = 0;
            sub->xFF = 1;
            sub->move();
        }
        break;
    case 1:
        sub->control(2);
        if (flag & 1) {
            sub->xFC = 0;
            sub->xFD = 0;
            sub->xFE = 0;
            sub->xFF = 1;
            sub->move();
        }
        break;
    case 2:
        EmMgr.destroy(sub);
        pSUB = 0;
        break;
    case 3:
        sub->control(0);
        break;
    case 4:
        sub->control(3);
        break;
    case 5:
        sub->xFC = 5;
        sub->xFD = 0;
        sub->xFE = 0;
        sub->xFF = 0;
        break;
    case 6:
        sub->control(5);
        break;
    case 7:
        sub->control(6);
        break;
    }
    if (flag & 2) {
        sub->subFlags |= 0x80;
    } else {
        BitOff16(sub->subFlags, 0x80);
    }
    if ((sub->stat & 0xFFFF0000) != 0x000F0000) {
        sub->subAux0 = 0;
    }
    sub->subAux1 = 0;
}

int SubCharCheckCtrl()
{
    cSubChar* sub = pSUB;

    if (sub->subFlags & 0x80) {
        return 1;
    }
    if (!(sub->subFlags & 0x40)) {
        return 0;
    }
    if (sub->subFlags & 1) {
        return 0;
    }
    if (sub->subFlags & 8) {
        return 0;
    }
    if (sub->xFC != 0) {
        return 0;
    }
    switch (sub->xFD) {
    case 0:
    case 1:
    case 2:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0xD:
    case 0xE:
    case 0x11:
    case 0x12:
    case 0x13:
        break;
    default:
        return 0;
    }
    return 1;
}

void SubCharCtrlHide(Vec* pos, int mode)
{
    cSubChar* sub = pSUB;

    switch (mode) {
    case 0:
        sub->subX534 = 1;
        break;
    case 1:
        sub->dmg.set(0, 0x80);
        sub->subHidePos = *pos;
        sub->subHideMode = 1;
        sub->xFC = 0;
        sub->xFD = 0x10;
        sub->xFE = 0;
        sub->xFF = 0;
        break;
    }
}

void SubCharMoveTo(int flag, f32 x, f32 y, f32 z, f32 w)
{
    cSubChar* sub = pSUB;

    if ((sub->subFlags & 8) && x == sub->subMoveTo[0] && y == sub->subMoveTo[1] && z == sub->subMoveTo[2] &&
        w == sub->subMoveTo[3]) {
        return;
    }
    sub->subMoveTo[0] = x;
    sub->subMoveTo[1] = y;
    sub->subMoveTo[2] = z;
    sub->subMoveTo[3] = w;
    sub->control(4);
    sub->analyze();
    sub->move();
    BitOff16(sub->subFlags2, 0x40);
    if (flag & 1) {
        sub->subFlags |= 0x10;
    }
}

void PlSetLadder(Vec* pos, int level, f32 ang)
{
    cPlayer* pl;

    if (level >= -1 && level <= 1) {
        pLog->err(0, 0, "ERROR PlSetLadder() level set error %d", level);
        return;
    }
    pl = pPL;
    pl->atari.throughOn();
    pl->dmg.set(0, 0x80);
    Vec v = {0.0f, 0.0f, 0.0f};
    Vec rot;

    PSMTXMultVecSR(pl->mat, &v, &v);
    PSVECAdd(&v, pos, &v);
    pl->setPos(&v);
    rot.x = 0.0f;
    rot.z = 0.0f;
    rot.y = ang;
    pl->setAng(&rot);
    pl->xFD = 0x10;
    pl->xFE = 0;
    pl->xFF = 0;
    pl->xFC = 0;
    if (level > 0) {
        pl->x3E0 = level - 2;
    } else {
        pl->xFE = 0xA;
        pl->x3E0 = -2 - level;
    }
}

void PlSetNeck(int mode)
{
    pPL->pNeck->setMode(mode);
}

void PlEndCamera()
{
    cPlayer* pl = pPL;

    if (pl->endCamera()) {
        if (PlGetStatus() & 0x10) {
            pl->pWep->pObj->setDisp(1, 1);
            pl->xFC = 0;
            pl->xFD = 0;
            pl->xFE = 0;
            pl->xFF = 0;
        }
    }
}

void PlRegistMotion(void* m0, void* m1, void* m2, void* m3, void* m4, void* m5, void* m6, void* m7, void* m8,
                    void* m9, void* m10, void* m11)
{
    cPlayer* pl = pPL;

    if (!VALID_PTR(pl)) {
        pLog->err(0, 0, "PlRegistMotion() pPL PTR ERROR. 0x%08x", pl);
        return;
    }
    if (m0) {
        pl->pRegistMot[0] = m0;
    }
    if (m1) {
        pl->pRegistMot[1] = m1;
    }
    if (m2) {
        pl->pRegistMot[2] = m2;
    }
    if (m3) {
        pl->pRegistMot[3] = m3;
    }
    if (m4) {
        pl->pRegistMot[4] = m4;
    }
    if (m5) {
        pl->pRegistMot[5] = m5;
    }
    if (m6) {
        pl->pRegistMot[6] = m6;
    }
    if (m7) {
        pl->pRegistMot[7] = m7;
    }
    if (m8) {
        pl->pRegistMot[8] = m8;
    }
    if (m9) {
        pl->pRegistMot[9] = m9;
    }
    if (m10) {
        pl->pRegistMot[10] = m10;
    }
    if (m11) {
        pl->pRegistMot[11] = m11;
    }
}

void SubCharRegistMotion(void* m0, void* m1)
{
    cSubChar* sub = pSUB;

    if (sub == 0) {
        pLog->err(0, 0, "SubCharRegistMotion() NO SUBCHAR");
        return;
    }
    if (m0) {
        sub->subMot0 = m0;
    }
    if (m1) {
        sub->subMot1 = m1;
    }
}

void PlRegistRoomEff(PlRoomEff* eff)
{
    pPL->pRoomEff = eff;
}

void PlReloadBullet()
{
    cPlayer* pl = pPL;

    switch (pG->wep_no) {
    case 0xD:
    case 0x13:
    case 0x16:
    case 0x17:
        pl->pWep->pObj->setMotion(pl);
        break;
    }
}

int joyFireOn()
{
    if (Key.on & 0x80) {
        if ((pG->flags_500C & 0x200000) || (pG->flags_5014 & 0x80000000)) {
            BitOn(pG->flags_500C, 0x4000);
            if ((G_ROOM_ID32 & 0xFFFF0000) == 0x011C0000 && (pG->flags_5014 & 0x80000000)) {
                BitOn(pG->flags_174, 0x20000000);
            }
            return 0;
        }
        return 1;
    }
    return 0;
}

int joyFireTrg()
{
    if (Key.trg & 0x80) {
        if ((pG->flags_500C & 0x200000) || (pG->flags_5014 & 0x80000000)) {
            pG->flags_500C |= 0x4000;
            return 0;
        }
        return 1;
    }
    return 0;
}

int joyKamae()
{
    cPlayer* pl = pPL;
    cPlWep* wep;

    if ((pSys->flags & 0x04000000) == 0) {
        switch (pG->x4FB8) {
        case 0:
        case 4:
            if (joyLKamae() != 0) {
                return 0;
            }
        case 2:
        case 3:
        case 5:
            break;
        default:
            return 0;
        }
        wep = pPL->pWep;
        if (wep == 0 || wep->pObj == 0) {
            pLog->err(0, 0, "joyKamae() PTR ERR");
            return 0;
        }
        // `goto` to the shared `return 0`: with a plain `return 0` jump.c hoists a set over the
        // branch (`li 1; bne end; li 0`) because the following `li 1` sets the same register; the
        // jump to the label gives the target's `beq ret0; li 1; b end`.
        if (wep->pObj->keyKamae() == 0) {
            goto ng;
        }
        return 1;
    } else {
        switch (pG->x4FB8) {
        case 0:
        case 2:
        case 3:
        case 4:
        case 5:
            break;
        default:
            return 0;
        }
        wep = pl->pWep;
        if (wep == 0 || wep->pObj == 0) {
            pLog->err(0, 0, "joyKamae() PTR ERR");
            return 0;
        }
        if (pl->flags_420 & 0x1000) {
            return 0;
        }
        if (wep->pObj->keyKamae()) {
            return 1;
        }
    }
ng:
    return 0;
}

int joyLKamae()
{
    cPlayer* pl = pPL;

    if ((pSys->flags & 0x04000000) == 0) {
        if (pG->x4FB8 == 0 || pG->x4FB8 == 4) {
            if (Key.on & 0x800) {
                return 1;
            }
        }
    } else {
        if (pG->x4FB8 == 0 || pG->x4FB8 == 4) {
            if (pl->flags_420 & 0x1000) {
                if (Key.on & 0x10) {
                    return 1;
                }
            } else {
                return 0;
            }
        }
    }
    return 0;
}

void PlWaterProc(cPlayer* pl)
{
    static f32 wavePower = 0.055f;
    static f32 spd0 = 1000.0f;
    static f32 spd1 = 6000.0f;
    static u8 hamonTimer;
    static u8 sibukiTimer;
    static Vec m_PosOldWater;
    f32 dist;

    if (pG->flags_5010 & 0x200000) {
        return;
    }
    if (pl->pRoomEff == 0) {
        pLog->err(2, 0, "PL WATER EFF NOT REGIST");
        return;
    }
    hamonTimer++;
    {
        u8 t = hamonTimer % 13;

        if (t == 0) {
            EstSet((int) pl, -1, 0, 0, pl->pRoomEff[0].id, pl->pRoomEff[0].type, 0, 0, (u32) pl, (void*) t);
        }
    }
    dist = GetDistance(&m_PosOldWater, &pl->pos);
    if (sibukiTimer) {
        sibukiTimer--;
    } else if (dist > spd1) {
        EstSet((int) pl, -1, 0, 0, pl->pRoomEff[2].id, pl->pRoomEff[2].type, 0, 0, (u32) pl, (void*) sibukiTimer);
        sibukiTimer = 10;
    } else if (dist > spd0) {
        EstSet((int) pl, -1, 0, 0, pl->pRoomEff[1].id, pl->pRoomEff[1].type, 0, 0, (u32) pl, (void*) sibukiTimer);
        sibukiTimer = 0x10;
    }
    if (dist > spd0) {
        AddWaterPower(&pPL->pos, wavePower);
    }
    m_PosOldWater = pl->pos;
}

void PlMotionReset()
{
    cPlayer* pl = pPL;

    if (pl->xFC != 0) {
        return;
    }
    if (pl->xFD == 0xF) {
        return;
    }
    if (pl->xFD == 0x11) {
        return;
    }
    pl->x4FC = 0;
    pl->x4FD = 0;
    pl->xFC = 0;
    pl->xFD = 0;
    pl->xFF = 1;
    pl->xFE = 0;
}

int SubCharCheckHealing()
{
    cSubChar* sub;
    // `const f32`: the pool address (`lis r30`) is computed at the declaration and kept across the
    // call while the `lfs` itself is issued after it; a plain `f32` local loads f31 before the call.
    const f32 limit = 25000000.0f;

    if (GetDistance(pPL->pos, pSUB->pos) > limit) {
        return 0;
    }
    sub = pSUB;
    if (sub->xFC != 0) {
        return -1;
    }
    switch (sub->xFD) {
    case 0:
    case 1:
    case 2:
    case 4:
    case 5:
    case 6:
    case 7:
    case 0xE:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
        break;
    default:
        return -1;
    }
    return 1;
}

int SubCharMotionReset()
{
    cSubChar* sub = pSUB;

    if (sub == 0) {
        return 0;
    }
    if (sub->xFC != 0) {
        return 0;
    }
    if (sub->xFD != 0 && sub->xFD != 1) {
        return 0;
    }
    sub->xFC = 0;
    sub->xFD = 0;
    sub->xFF = 1;
    sub->xFE = 0;
    return 1;
}

void PlSetEyeMode(u8 mode)
{
    pPL->eyeMode = mode;
}

f32 PlGetDirY()
{
    return pPL->rot.y + pPL->pWaist->cur;
}

void PlRegistBoss(void* a, void* b)
{
    pPL->boss0 = a;
    pPL->boss1 = b;
}

int PlIsArmor()
{
    if (pG->flags_54 & 0x20) {
        return 0;
    }
    if (pG->x4FB8 != 0) {
        return 0;
    }
    return pG->costume == 2 || pG->costume == 3;
}

int PlSetWhistle()
{
    cPlayer* pl;

    if (!(Key.trg & 0x200)) {
        return 0;
    }
    if (pG->flags_500C & 0x400) {
        return 0;
    }
    if (!(pG->flags_5010 & 4)) {
        return 0;
    }
    pl = pPL;
    if ((u32) pl->xFD > 6) {
        return 0;
    }
    if (pl->xFD == 6) {
        switch (pl->xFE) {
        case 2:
        case 4:
        case 6:
            return 0;
        }
    }
    pl->interrupt();
    pl->xFC = 0;
    pl->xFD = 0x14;
    pl->xFE = 0;
    pl->xFF = 0;
    return 1;
}

int PlGetWeaponNo()
{
    cPlayer* pl = pPL;

    if (((pl->stat & 0xFFFF0000) == 0x000B0000 && pl->xFE != 3) || joyLKamae()) {
        return 0x10;
    }
    return pG->wep_no;
}

void PlSetFace(int no)
{
    int f;

    switch (no) {
    default:
        f = 0;
        break;
    case 1:
        f = 1;
        break;
    case 2:
        f = 2;
        break;
    }
    pPL->setFace(f);
}

void SubCharSetFace(int no)
{
    if (pSUB->id == 3) {
        pSUB->setFace(no);
    }
}

void PlDataRelease()
{
    cObj* obj;
    cObj* objCur;
    cObj* objNext;
    cEm* em;
    cEm* emCur;
    cEm* emNext;

    // Guarded do/while loops testing `next` at the bottom: a `while (obj)` with the switch body
    // is not rotated by expand_end_loop (test at the top, `b top` at the bottom).
    obj = ObjMgr.pAlive;
    if (obj) {
        do {
            objCur = obj;
            objNext = (cObj*) objCur->next;
            obj = objNext;
            switch (objCur->id) {
            case 0x1A:
            case 0x23:
            case 0x29:
            case 0x2A:
            case 0x3A:
                ObjMgr.destroy(objCur);
                break;
            }
        } while (objNext);
    }
    em = EmMgr.pAlive;
    if (em) {
        do {
            emCur = em;
            emNext = (cEm*) emCur->next;
            em = emNext;
            if (emCur->id == 0x4F) {
                EmMgr.destroy(emCur);
            }
        } while (emNext);
    }
}

// the split object's .sdata is padded to 8 bytes after the three floats
asm(".section .sdata; .balign 8");
