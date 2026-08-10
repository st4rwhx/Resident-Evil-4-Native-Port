// game/cockpit: HUD (life meter, bullet counter, count-down, action button) (D:/Bio4/Prog/cockpit.cpp).
#include "types.h"
#include "global.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "id_sys.h"
#include "item.h"
#include "player.h"
#include "pl_npc.h"
#include "cockpit.h"

// game/item.cpp: life bar level (0..levels) of `max` against `base`
int lifeLevel(int levels, s16 max, int base);
extern "C" u16 WeaponId2BulletId(u16 id, int attr);

#define ARC_PTR(ofs) ((void*) (pG->pArc->ofs + (u32) pG->pArc))

#define ID_LIFE 0x21
#define ID_ACT 0x20
#define ID_CDOWN 0x23
#define ID_MSG 0x2F
#define ID_FRAME 0x30
#define ID_BULLET 0x32

Cockpit Cckpt;
int g_boss_bar_flag = 0;

static int dispBulletDigit(u8 no);

void Cockpit::gameInit()
{
    IdSys.roomInit();
}

void Cockpit::roomInit()
{
    IdSys.roomInit();
    IdTexRoomInit();
    IdTexDataLoad(ARC_PTR(ofs_74), 4);
    IdSys.set(ARC_PTR(ofs_88), 0xFF, ID_FRAME, 0x13, 0, 0);
    IdSys.kill(0xFF, ID_MSG);
    action.roomInit();
    life.roomInit();
    bullet.roomInit();
    countDown.roomInit();
}

void Cockpit::move()
{
    if ((s32) pG->flags_60 < 0 && !(pG->flags_60 & 0x02000000)) {
        pG->flags_64 |= 0x80000000;
    } else {
        pG->flags_64 &= ~0x80000000;
    }
    if (IdSys.setCk(ID_LIFE)) {
        life.move();
        bullet.move();
        action.move();
        countDown.move();
    }
}

void Cockpit::msgWindow(int mode)
{
    switch (mode) {
    case 1:
        if (!IdSys.setCk(ID_MSG)) {
            IdSys.set(ARC_PTR(ofs_94), 0xFF, ID_MSG, 0x13, 0, 0);
        }
        break;
    case 0:
        IdSys.kill(0xFF, ID_MSG);
        break;
    }
}

void Cockpit::lifeMeterDisp(int sw)
{
    switch (sw) {
    case 1:
        if (IdSys.setCk(ID_LIFE)) {
            life.disp(1);
        } else {
            roomInit();
        }
        break;
    case 0:
        if (IdSys.setCk(ID_LIFE)) {
            life.disp(0);
        }
        break;
    }
}

// ---------------------------------------------------------------- LifeMeter

static f32 a_ratio = 0.9f;

// Colour smoothing. The cast-then-deref store is not MEM_IN_STRUCT_P, so a_ratio is reloaded
// after every store (the original reloads it per statement); a plain v[i] hoists it out of the loop
// and a reference parameter turns the address into a stepping pointer.
#define FIDX(p, i) (*(f32*) ((u8*) (p) + (i) * 4))
static inline void approachIdx(f32* v, f32* t, int i)
{
    FIDX(v, i) = a_ratio * FIDX(v, i) + (1.0f - a_ratio) * t[i];
}

void LifeMeter::roomInit()
{
    IdUnit* u;

    IdSys.set(ARC_PTR(ofs_7C), 0xFF, ID_LIFE, 0x13, 5, 0);
    IdSys.unitPtr(0x40, ID_LIFE)->flags &= ~8;
    IdSys.unitPtr(0x41, ID_LIFE)->flags &= ~8;
    IdSys.unitPtr(0x42, ID_LIFE)->flags &= ~8;
    switch (pG->x4FB8) {
    case 0:
        IdSys.unitPtr(0x40, ID_LIFE)->flags |= 8;
        break;
    case 1:
        IdSys.unitPtr(0x41, ID_LIFE)->flags |= 8;
        break;
    case 2:
        IdSys.unitPtr(0x42, ID_LIFE)->flags |= 8;
        break;
    }
    disp(1);
    FSet(life, (f32) (s16) pG->pl_life);
    FSet(subLife, (f32) (s16) pG->sub_life);
    u = IdSys.unitPtr(0x11, ID_LIFE);
    c0[0][0] = u->col0[0];
    c0[0][1] = u->col0[1];
    c0[0][2] = u->col0[2];
    c0[0][3] = u->col0[3];
    c1[0][0] = u->col1[0];
    c1[0][1] = u->col1[1];
    c1[0][2] = u->col1[2];
    c1[0][3] = u->col1[3];
    u = IdSys.unitPtr(0x10, ID_LIFE);
    c0[1][0] = u->col0[0];
    c0[1][1] = u->col0[1];
    c0[1][2] = u->col0[2];
    c0[1][3] = u->col0[3];
    c1[1][0] = u->col1[0];
    c1[1][1] = u->col1[1];
    c1[1][2] = u->col1[2];
    c1[1][3] = u->col1[3];
    u = IdSys.unitPtr(0x0F, ID_LIFE);
    c0[2][0] = u->col0[0];
    c0[2][1] = u->col0[1];
    c0[2][2] = u->col0[2];
    c0[2][3] = u->col0[3];
    c1[2][0] = u->col1[0];
    c1[2][1] = u->col1[1];
    c1[2][2] = u->col1[2];
    c1[2][3] = u->col1[3];
    flags = 0;
    move();
}

#define METER_ANGLE(lv, max, range, base) ((f32) (lv) * (range) / (max) + (base))
#define METER_ROT(base, rate, lo) ((base) - ((rate) - (lo)) * 45.0f)

void LifeMeter::move()
{
    f32 a[4];
    f32 b[4];
    f32 c[4];
    f32 d[4];
    IdUnit* u;
    IdUnit* u2;
    IdUnit* u3;
    IdUnit* u4;
    cPlayer* pl = pPL;
    IdUnit* src = 0;
    IdUnit* src2;
    f32 ang;
    f32 rate;
    int i;

    if (pSUB && pSUB->id == 3) {
        IdUnit* p = IdSys.unitPtr(1, ID_LIFE);
        p->flags |= 8;
        p = IdSys.unitPtr(3, ID_LIFE);
        p->flags |= 8;
    } else {
        IdUnit* p = IdSys.unitPtr(1, ID_LIFE);
        p->flags &= ~8;
        p = IdSys.unitPtr(3, ID_LIFE);
        p->flags &= ~8;
    }
    level = lifeLevel(20, pG->pl_life_max, 1200);
    subLevel = lifeLevel(5, pG->sub_life_max, 600);
    ang = METER_ANGLE(level, 20.0f, -135.0f, -45.0f);
    FSet(IdSys.unitPtr(0xFE, ID_LIFE)->rot.z, ang);
    ang = METER_ANGLE(subLevel, 5.0f, 90.0f, 0.0f);
    FSet(IdSys.unitPtr(2, ID_LIFE)->rot.z, ang);

    FSet(life, a_ratio * life + (1.0f - a_ratio) * (f32) (s16) pG->pl_life);
    FSet(subLife, a_ratio * subLife + (1.0f - a_ratio) * (f32) (s16) pG->sub_life);

    u = IdSys.unitPtr(7, ID_LIFE);
    u2 = IdSys.unitPtr(8, ID_LIFE);
    u3 = IdSys.unitPtr(9, ID_LIFE);
    u->flags &= ~8;
    u2->flags &= ~8;
    u3->flags &= ~8;
    rate = life / 400.0f;
    if (rate > 4.0f) {
        u->flags |= 8;
        u2->flags |= 8;
        u3->flags |= 8;
        u->rot.z = 90.0f;
        u2->rot.z = 0.0f;
        u3->rot.z = METER_ROT(0.0f, rate, 4.0f);
    } else if (rate > 2.0f) {
        u->flags |= 8;
        u2->flags |= 8;
        u->rot.z = 90.0f;
        u2->rot.z = METER_ROT(90.0f, rate, 2.0f);
    } else if (rate >= 0.0f) {
        u->flags |= 8;
        u->rot.z = METER_ROT(180.0f, rate, 0.0f);
    }

    u = IdSys.unitPtr(4, ID_LIFE);
    u2 = IdSys.unitPtr(5, ID_LIFE);
    u->flags &= ~8;
    u2->flags &= ~8;
    rate = subLife / 200.0f;
    if (rate > 3.0f) {
        u->flags |= 8;
        u2->flags |= 8;
        u->rot.z = 180.0f;
        u2->rot.z = (rate - 3.0f) * 30.0f - 180.0f;
    } else if (rate >= 0.0f) {
        u->flags |= 8;
        u->rot.z = (rate - 0.0f) * 30.0f + 88.0f;
    }

    switch (pl->getLifeLevel()) {
    case 0:
        for (i = 0; i < 4; i++) {
            a[i] = (f32) c0[0][i];
            b[i] = (f32) c1[0][i];
        }
        break;
    case 1:
        for (i = 0; i < 4; i++) {
            a[i] = (f32) c0[1][i];
            b[i] = (f32) c1[1][i];
        }
        break;
    case 2:
        for (i = 0; i < 4; i++) {
            a[i] = (f32) c0[2][i];
            b[i] = (f32) c1[2][i];
        }
        break;
    }
    for (i = 0; i < 4; i++) {
        approachIdx(col0, a, i);
        approachIdx(col1, b, i);
    }

    if ((s16) pG->sub_life > (s16) pG->sub_life_max * 3 / 4) {
        for (i = 0; i < 4; i++) {
            c[i] = (f32) c0[0][i];
            d[i] = (f32) c1[0][i];
        }
    } else if ((s16) pG->sub_life > (s16) pG->sub_life_max / 4) {
        for (i = 0; i < 4; i++) {
            c[i] = (f32) c0[1][i];
            d[i] = (f32) c1[1][i];
        }
    } else {
        for (i = 0; i < 4; i++) {
            c[i] = (f32) c0[2][i];
            d[i] = (f32) c1[2][i];
        }
    }
    for (i = 0; i < 4; i++) {
        approachIdx(subCol0, c, i);
        approachIdx(subCol1, d, i);
    }

    switch (pl->getLifeLevel()) {
    case 0:
        src = IdSys.unitPtr(0x11, ID_LIFE);
        break;
    case 1:
        src = IdSys.unitPtr(0x10, ID_LIFE);
        break;
    case 2:
        src = IdSys.unitPtr(0x0F, ID_LIFE);
        break;
    }
    {
        IdUnit* p = IdSys.unitPtr(0x12, ID_LIFE);

        p->col0[0] = (u8) col0[0];
        p->col0[1] = (u8) col0[1];
        p->col0[2] = (u8) col0[2];
        p->col0[3] = 0xFF;
        p->col1[0] = (u8) col1[0];
        p->col1[1] = (u8) col1[1];
        p->col1[2] = (u8) col1[2];
        p->col1[3] = 0xFF;
        p->curve[2] = src->curve[2];
        p->loop |= 4;
    }

    if ((s16) pG->sub_life > (s16) pG->sub_life_max * 3 / 4) {
        src2 = IdSys.unitPtr(0x11, ID_LIFE);
    } else if ((s16) pG->sub_life > (s16) pG->sub_life_max / 4) {
        src2 = IdSys.unitPtr(0x10, ID_LIFE);
    } else {
        src2 = IdSys.unitPtr(0x0F, ID_LIFE);
    }
    {
        IdUnit* p = IdSys.unitPtr(3, ID_LIFE);

        p->col0[0] = (u8) subCol0[0];
        p->col0[1] = (u8) subCol0[1];
        p->col0[2] = (u8) subCol0[2];
        p->col0[3] = 0xFF;
        p->col1[0] = (u8) subCol1[0];
        p->col1[1] = (u8) subCol1[1];
        p->col1[2] = (u8) subCol1[2];
        p->col1[3] = 0xFF;
        p->curve[2] = src2->curve[2];
        p->loop |= 4;
    }

    u = IdSys.unitPtr(0x13, ID_LIFE);
    u4 = IdSys.unitPtr(0x14, ID_LIFE);
    switch (pl->getLifeLevel()) {
    case 0:
        u->flags &= ~8;
        u4->flags &= ~8;
        break;
    case 1:
        u->flags |= 8;
        u4->flags &= ~8;
        break;
    case 2:
        u->flags &= ~8;
        u4->flags |= 8;
        break;
    }

    u = IdSys.unitPtr(0x18, ID_LIFE);
    u4 = IdSys.unitPtr(0x19, ID_LIFE);
    if ((s16) pG->sub_life > (s16) pG->sub_life_max * 3 / 4) {
        u->flags &= ~8;
        u4->flags &= ~8;
    } else if ((s16) pG->sub_life > (s16) pG->sub_life_max / 4) {
        u->flags |= 8;
        u4->flags &= ~8;
    } else {
        u->flags &= ~8;
        u4->flags |= 8;
    }

    if (pSUB) {
        IdSys.unitPtr(0x1A, ID_LIFE)->flags &= ~8;
        IdSys.unitPtr(0x1B, ID_LIFE)->flags &= ~8;
        IdSys.unitPtr(0x1C, ID_LIFE)->flags &= ~8;
        IdSys.unitPtr(0x1D, ID_LIFE)->flags &= ~8;
        IdSys.unitPtr(0x1E, ID_LIFE)->flags &= ~8;
        if (pSUB->id == 3) {
            u32 cond = SubCharGetCondition();
            if (cond & 8) {
                IdSys.unitPtr(0x1C, ID_LIFE)->flags |= 8;
            } else {
                if (cond & 1) {
                    IdSys.unitPtr(0x1E, ID_LIFE)->flags |= 8;
                } else if (cond & 2) {
                    IdSys.unitPtr(0x1A, ID_LIFE)->flags |= 8;
                }
                if (cond & 0x10) {
                    IdSys.unitPtr(0x1D, ID_LIFE)->flags |= 8;
                }
                if (cond & 0x24) {
                    IdSys.unitPtr(0x1B, ID_LIFE)->flags |= 8;
                }
            }
        }
    }
}

void LifeMeter::fix(int sw)
{
    IdUnit* u = IdSys.unitPtr(0, ID_LIFE);

    u->flags |= 8;
    if (sw == 0) {
        u->dir |= 0xF;
        IdSys.setTime(u, 0);
    } else {
        Hermite1* h = u->curve[0];
        u8 t = (u8) h->key[h->num - 1].t;

        u->dir &= ~0xF;
        IdSys.setTime(u, t);
    }
}

void LifeMeter::disp(int sw)
{
    IdUnit* u;

    switch (sw) {
    case 1:
        u = IdSys.unitPtr(0, ID_LIFE);
        u->dir |= 0xF;
        u->flags |= 8;
        break;
    case 0:
        u = IdSys.unitPtr(0, ID_LIFE);
        u->dir &= ~0xF;
        u->flags &= ~8;
        break;
    }
}

void LifeMeter::frameOut()
{
    IdUnit* u = IdSys.unitPtr(0, ID_LIFE);

    u->dir &= ~0xF;
    u->flags |= 8;
}

void LifeMeter::frameIn()
{
    IdUnit* u = IdSys.unitPtr(0, ID_LIFE);

    u->dir |= 0xF;
    u->flags |= 8;
}

// ---------------------------------------------------------------- ActionButton

void ActionButton::roomInit()
{
    IdSys.set(ARC_PTR(ofs_80), 1, ID_ACT, 0x13, 5, 0);
    IdSys.set(ARC_PTR(ofs_80), 0xF0, ID_ACT, 0x13, 5, 0);
    old = 0;
    no = 0;
}

void ActionButton::move()
{
    if (no != old) {
        u8 id;

        switch (no) {
        case 6:
            id = 9;
            break;
        case 2:
            id = 2;
            break;
        case 3:
            id = 3;
            break;
        case 4:
            id = 4;
            break;
        case 0xC:
            id = 5;
            break;
        case 0xB:
            id = 6;
            break;
        case 5:
            id = 7;
            break;
        case 9:
            id = 0xA;
            break;
        case 0xA:
            id = 0xB;
            break;
        case 0xD:
            id = 0xC;
            break;
        case 0xE:
            id = 8;
            break;
        case 0xF:
            id = 0xD;
            break;
        case 0x10:
            id = 0xE;
            break;
        case 1:
            id = 0;
            break;
        default:
            id = 0;
            break;
        }
        IdSys.kill(0xFF, ID_ACT);
        IdSys.set(ARC_PTR(ofs_80), 1, ID_ACT, 0x13, 5, 0);
        IdSys.set(ARC_PTR(ofs_80), 0xF0, ID_ACT, 0x13, 5, 0);
        if (no != 0) {
            IdSys.set(ARC_PTR(ofs_80), id, ID_ACT, 0x13, 5, 0);
        }
    }
    old = no;
}

// ---------------------------------------------------------------- BulletInfo

void BulletInfo::roomInit()
{
    markNo = -1;
}

void BulletInfo::move()
{
    u8 digit[3];
    IdUnit* u[3];
    IdUnit* empty;
    ItemInfo info;
    int noBullet = 0;
    cItemMgr* im = &ItemMgr;
    int wepNo;
    u16 num;
    u8 mark;
    int i;

    wepNo = WeaponId2WeaponNo(im->armId);
    num = im->bulletNum();
    if (num == 0) {
        u16 id;

        itemInfo(im->armId, &info);
        if (info.type == 1) {
            id = WeaponId2BulletId(im->pArm->id, im->pArm->x8 >> 13);
        } else {
            id = WeaponId2BulletId(im->armId, 0);
        }
        noBullet = ItemMgr.search(id) == 0;
    }
    for (i = 0; i < 3; i++) {
        digit[i] = num % 10;
        num /= 10;
    }
    empty = IdSys.unitPtr(0x3F, ID_LIFE);
    empty->flags &= ~8;
    u[0] = IdSys.unitPtr(0xB, ID_LIFE);
    u[0]->flags_7F |= 2;
    u[0]->no = digit[0];
    u[1] = IdSys.unitPtr(0xA, ID_LIFE);
    u[1]->flags_7F |= 2;
    u[1]->no = digit[1];
    u[2] = IdSys.unitPtr(0x17, ID_LIFE);
    u[2]->flags_7F |= 2;
    u[2]->no = digit[2];

    mark = dispBulletIconMarkNo(wepNo);
    if (mark == 0xFF) {
        IdSys.kill(0xFF, ID_BULLET);
    }
    if (markNo != mark) {
        if (mark == 0xFF) {
            IdSys.kill(0xFF, ID_BULLET);
        } else {
            IdSys.kill(0xFF, ID_BULLET);
            IdSys.set(ARC_PTR(ofs_98), mark, ID_BULLET, 0x13, 5, 0);
            IdUnit* p = IdSys.unitPtr(mark, ID_BULLET);
            IdSys.unitParent(IdSys.unitPtr(0x30, ID_LIFE), p);
        }
    }
    markNo = mark;

    if (dispBulletDigit(wepNo) == 1) {
        if (noBullet == 0) {
            u[0]->flags |= 8;
            u[1]->flags |= 8;
            u[2]->flags |= 8;
        } else {
            u[0]->flags &= ~8;
            u[1]->flags &= ~8;
            u[2]->flags &= ~8;
            empty->flags |= 8;
        }
    } else {
        u[0]->flags &= ~8;
        u[1]->flags &= ~8;
        u[2]->flags &= ~8;
    }
    if (digit[2] == 0) {
        u[2]->flags &= ~8;
    }
}

static int dispBulletDigit(u8 no)
{
    if (pG->x4FB8 == 1) {
        return 0;
    }
    if ((pPL->stat & 0xFFFF0000) == 0x000F0000) {
        return 0;
    }
    switch (no) {
    case 1:
    case 2:
    case 3:
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
    case 0xF:
    case 0x11:
    case 0x13:
    case 0x16:
    case 0x17:
    case 0x19:
    case 0x1C:
    case 0x1D:
    case 0x1F:
    case 0x20:
    case 0x21:
        return 1;
    }
    return 0;
}

u8 dispBulletIconMarkNo(u8 no)
{
    if (pG->x4FB8 == 1) {
        return 0xFF;
    }
    if ((pPL->stat & 0xFFFF0000) == 0x000F0000) {
        if (pG->room_id == 0x333) {
            return 0xFF;
        }
        return 0x33;
    }
    switch (no) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 0xB:
    case 0xC:
    case 0xF:
    case 0x11:
        return 0x31;
    case 7:
    case 8:
    case 0x21:
        return 0x37;
    case 9:
    case 0xA:
    case 0x1D:
        return 0x36;
    case 0xD:
        return 0x38;
    case 0xE:
        return 0x39;
    case 0x10:
        return 0x34;
    case 0x13:
    case 0x16:
    case 0x17:
        return 0x32;
    case 0x15:
        return 0x33;
    case 0x19:
    case 0x1F:
    case 0x20:
        return 0x3A;
    }
    return 0xFF;
}

// ---------------------------------------------------------------- CountDown

void CountDown::roomInit()
{
    IdSys.set(ARC_PTR(ofs_84), 0xFF, ID_CDOWN, 0x13, 5, 0);
    IdSys.unitPtr(0x10, ID_CDOWN)->flags &= ~8;
    IdSys.unitPtr(0x10, ID_CDOWN)->dir &= ~0xF;
    flags &= ~1;
    flags &= ~0x10;
    initTime(0, 0, 0);
    warnTime(0, 0, 0);
}

struct Digits {
    u8 hi;
    u8 lo;
};

static inline void U32Add(u32& d, u32 v)
{
    d += v;
}

// the inline keeps the two tests apart (fold would merge them into one `andis. 0xa`)
static inline u32 chkFlag5014(u32 b)
{
    return pG->flags_5014 & b;
}

void CountDown::move()
{
    f32 tbl[6] = {0.0f, 1.0f, -1.0f, 0.0f, 1.5f, -0.5f};
    int run = 1;
    IdUnit* u;
    IdUnit* p;
    u32 t;
    Digits d;
    s8 oldTens;
    s8 newTens;
    f32 ft;

    if ((flags & 1) == 0) {
        run = 0;
    }
    if (run == 0) {
        return;
    }
    if (!chkFlag5014(0x00080000) && !chkFlag5014(0x00020000) &&
        ((pG->flags_5010 & 0x10000000) || (pG->flags_170 & 0x10000000))) {
        flags |= 8;
    } else {
        flags &= ~8;
    }
    if (pG->cdown_add_sec != 0) {
        U32Add(frame, pG->cdown_add_sec * 30);
        pG->cdown_add_sec = 0;
    }
    if (!(pG->flags_64 & 0x00010000) && !(pG->flags_500C & 0x00040000) && !(flags & 8)) {
        if (frame != 0) {
            frame = frame - 1;
        } else {
            frame = 0;
        }
    }
    if (frame < warnFrame) {
        IdUnit* s;

        u = IdSys.unitPtr(0x10, ID_CDOWN);
        s = IdSys.unitPtr(8, ID_CDOWN);
        u->col0[0] = (u8) s->col[0];
        u->col0[1] = (u8) s->col[1];
        u->col0[2] = (u8) s->col[2];
        u->col0[3] = (u8) s->col[3];
    }
    oldTens = cs / 10;
    ft = (f32) frame * 10.0f / 3.0f + 0.5f;
    t = (u32) ft;
    cs = t % 100;
    t /= 100;
    min = t / 60;
    sec = t % 60;
    newTens = cs / 10;
    if (oldTens != newTens) {
        counter = (counter + 1) % 6;
    }

    p = IdSys.unitPtr(6, ID_CDOWN);
    p->no = 0xB;
    p->flags_7F |= 2;
    p = IdSys.unitPtr(7, ID_CDOWN);
    p->no = 0xC;
    p->flags_7F |= 2;

    d.hi = min / 10;
    d.lo = min % 10;
    p = IdSys.unitPtr(0, ID_CDOWN);
    p->no = d.hi;
    p->flags_7F |= 2;
    p = IdSys.unitPtr(1, ID_CDOWN);
    p->no = d.lo;
    p->flags_7F |= 2;

    d.hi = sec / 10;
    d.lo = sec % 10;
    p = IdSys.unitPtr(2, ID_CDOWN);
    p->no = d.hi;
    p->flags_7F |= 2;
    p = IdSys.unitPtr(3, ID_CDOWN);
    p->no = d.lo;
    p->flags_7F |= 2;

    d.hi = cs / 10;
    d.lo = cs % 10;
    p = IdSys.unitPtr(4, ID_CDOWN);
    p->no = d.hi;
    p->flags_7F |= 2;
    if (ft != 0.0f) {
        t = (u32) ft;
        t = t / 10 * 10;
        ft -= (f32) t;
        d.lo = (u8) (ft + tbl[counter]);
        d.lo = d.lo % 10;
    }
    p = IdSys.unitPtr(5, ID_CDOWN);
    p->no = d.lo;
    p->flags_7F |= 2;
}

void CountDown::disp(int sw)
{
    IdUnit* u;

    switch (sw) {
    case 1:
        u = IdSys.unitPtr(0x10, ID_CDOWN);
        u->dir |= 0xF;
        u->flags |= 8;
        flags &= ~0x10;
        break;
    case 0:
        u = IdSys.unitPtr(0x10, ID_CDOWN);
        u->dir &= ~0xF;
        u->flags &= ~8;
        flags |= 0x10;
        break;
    }
}

void CountDown::frameIn()
{
    IdUnit* u = IdSys.unitPtr(0x10, ID_CDOWN);

    u->flags |= 8;
    u->dir &= ~0xF;
    IdSys.setTime(u, 0x1E);
}

void CountDown::frameOut()
{
    IdSys.unitPtr(0x10, ID_CDOWN)->dir |= 0xF;
}

#define TIME_FRAME(m, s, c) ((u32) ((((f32) (m) * 60.0f + (f32) (s)) * 100.0f + (f32) (c)) * 3.0f / 10.0f + 0.5f))

void CountDown::initTime(int m, int s, int c)
{
    IdUnit* u;

    frame = TIME_FRAME(m, s, c);
    min = m;
    sec = s;
    cs = c;
    counter = 0;
    u = IdSys.unitPtr(0x10, ID_CDOWN);
    u->col0[0] = 0xFF;
    u->col0[1] = 0xFF;
    u->col0[2] = 0xFF;
    u->col0[3] = 0xFF;
}

void CountDown::initTimeFrame(u32 f)
{
    frame = f;
}

void CountDown::warnTime(int m, int s, int c)
{
    warnFrame = TIME_FRAME(m, s, c);
}

// Dead-stripped in the original (STRIP_UNUSED): only its constant pool survives after warnTime's.
static u32 cockpit_dead_time(int m, int s, int c)
{
    return TIME_FRAME(m, s, c);
}

void CountDown::getTime(int* m, int* s, int* c)
{
    *m = min;
    *s = sec;
    *c = cs;
}

u32 CountDown::getFrame()
{
    return frame;
}

void CountDown::saveDisp()
{
    int run;

    savedFlags = flags;
    run = 1;
    if ((flags & 1) == 0) {
        run = 0;
    }
    if (run) {
        disp(0);
    }
}

void CountDown::loadDisp()
{
    int run = 1;

    if ((flags & 1) == 0) {
        run = 0;
    }
    if (run) {
        if (!(savedFlags & 0x10)) {
            flags = savedFlags;
            disp(1);
            frameIn();
        }
    }
}
