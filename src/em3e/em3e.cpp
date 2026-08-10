// em3e module (D:/Bio4/Prog/emmark.cpp): the shooting-gallery targets (cEmMark) of room 22c. The room
// creates a target from an EmMarkData record; the target then runs its instruction list (begin: rise,
// stay: wait N frames, move: walk to an integer position, end: fold down / vanish) and reports hits to
// the room through R22cHitMark / R22cHitEffect.

#include "atari.h"
#include "light.h"
#include "dmg.h"
#include "em3e.h"
#include "emhit.h"
#include "em_sub.h"
#include "esp.h"
#include "snd.h"
#include "scroll.h"
#include "pl_wep.h"
#include "player.h"
#include "global.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);   // game/em.cpp

typedef void (*EmMarkFunc)(cEmMark*);

static void emmark_begin(cEmMark* em);
static void emmark_end(cEmMark* em);
static void emmark_stay(cEmMark* em);
static void emmark_move(cEmMark* em);
static void emmark_none(cEmMark* em);

#define ARC(no) PL_ARC_PTR(subArc, no)

void em3eInit(cEm* em)
{
    new (em) cEmMark();
}

cEmMark::cEmMark()
{
    setStatus(1);
}

void cEmMark::init(EmMarkData* d)
{
    init(d->type, d->inst, (f32) d->x, (f32) d->y, (f32) d->z);
}

void cEmMark::init(u8 type, EmMarkInst* inst, f32 x, f32 y, f32 z)
{
    static const Vec ofs = { 0.0f, 0.0f, 0.0f };
    void* bin;
    void* tpl;

    pos.x = x;
    pos.y = y;
    pos.z = z;
    this->type = type;
    switch (this->type) {
    case 0:
    default:
        bin = ARC(5);
        tpl = ARC(6);
        break;
    case 1:
        bin = ARC(9);
        tpl = ARC(0xA);
        break;
    case 2:
        bin = ARC(0xD);
        tpl = ARC(0xE);
        break;
    case 3:
        bin = ARC(0x15);
        tpl = ARC(0x16);
        break;
    case 4:
        bin = ARC(5);
        tpl = ARC(6);
        break;
    case 5:
        bin = ARC(5);
        tpl = ARC(6);
        break;
    case 6:
        bin = ARC(0xF);
        tpl = ARC(0x10);
        break;
    case 0xA:
        bin = SmdGetObjPtr(5)->pInfo->pData;
        tpl = SmdGetObjPtr(5)->pInfo->pTpl;
        break;
    case 0xB:
        bin = SmdGetObjPtr(6)->pInfo->pData;
        tpl = SmdGetObjPtr(6)->pInfo->pTpl;
        break;
    case 0xC:
        bin = SmdGetObjPtr(0x33)->pInfo->pData;
        tpl = SmdGetObjPtr(0x33)->pInfo->pTpl;
        break;
    case 0xD:
        bin = SmdGetObjPtr(0x34)->pInfo->pData;
        tpl = SmdGetObjPtr(0x34)->pInfo->pTpl;
        break;
    case 0xE:
        bin = SmdGetObjPtr(0x35)->pInfo->pData;
        tpl = SmdGetObjPtr(0x35)->pInfo->pTpl;
        break;
    case 0xF:
        bin = SmdGetObjPtr(0x36)->pInfo->pData;
        tpl = SmdGetObjPtr(0x36)->pInfo->pTpl;
        break;
    }
    modelInit(bin, tpl);
    if (this->type == 5) {
        scale.x = 6.0f;
        scale.y = 6.0f;
        scale.z = 6.0f;
        pos.y -= 1000.0f;
        pParts->rot.x = PI;
        pParts->rot.y = 0.0f;
        pParts->rot.z = 0.0f;
    }
    {
        Vec size;
        f32 r;
        int lv;

        if (this->type <= 9) {
            r = 1000.0f;
            lv = 2;
        } else {
            r = 10000.0f;
            lv = 0x10;
        }
        size.x = r;
        size.y = r;
        size.z = 0.0f;
        lightInfo.init2(0, 1, &ofs, &size, lv);
    }
    switch (this->type) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 6:
        hp = 1;
        break;
    case 4:
        hp = 5;
        break;
    case 5:
        hp = 10;
        break;
    default:
        hp = 1000;
        break;
    }
    EspDataLoad((u32) ARC(4), 0x33, 0);
    {
        const f32 depth = 150.0f;

        switch (this->type) {
        case 0:
        case 1:
        case 2:
        case 4:
        case 6:
            YarareInitCube(this, 0.0f, 1800.0f, 0.0f, 180.0f, 400.0f, depth, 1, 1);
            YarareAddCube(this, &EMMARK(this)->hit[0], 0.0f, 1400.0f, 0.0f, 370.0f, 400.0f, depth, 1, 1);
            YarareAddCube(this, &EMMARK(this)->hit[1], 0.0f, 1000.0f, 0.0f, 350.0f, 500.0f, depth, 1, 1);
            YarareAddCube(this, &EMMARK(this)->hit[2], 0.0f, 400.0f, 0.0f, 300.0f, 600.0f, depth, 1, 1);
            if (this->type == 6) {
                YarareAddCube(this, &EMMARK(this)->hit[3], -650.0f, 1200.0f, 0.0f, 200.0f, 1100.0f, 50.0f, 1, 1);
            }
            break;
        case 3:
            YarareInit(this, 0.0f, -500.0f, 0.0f, 300.0f, depth, 1, 1);
            break;
        case 5:
            YarareInit(this, 0.0f, 400.0f, 0.0f, 200.0f, 0.0f, 0, 1);
            break;
        case 0xA:
            YarareInitCube(this, -2600.0f, 0.0f, -27000.0f, 2600.0f, 6000.0f, depth, 1, 1);
            break;
        case 0xB:
            YarareInitCube(this, 2600.0f, 0.0f, -27000.0f, 2600.0f, 6000.0f, depth, 1, 1);
            break;
        case 0xC:
            YarareInitCube(this, -2600.0f, 0.0f, -27000.0f, 2600.0f, 6000.0f, depth, 1, 1);
            YarareAddCube(this, &EMMARK(this)->hit[0], -2700.0f, 800.0f, -26800.0f, 1700.0f, 4000.0f, depth, 1, 1);
            break;
        case 0xD:
            YarareInitCube(this, 2600.0f, 0.0f, -27000.0f, 2600.0f, 6000.0f, depth, 1, 1);
            YarareAddCube(this, &EMMARK(this)->hit[0], 600.0f, 2800.0f, -26800.0f, 500.0f, 1500.0f, depth, 1, 1);
            YarareAddCube(this, &EMMARK(this)->hit[1], 3900.0f, 2400.0f, -26800.0f, 600.0f, 1200.0f, depth, 1, 1);
            YarareAddCube(this, &EMMARK(this)->hit[2], 1300.0f, 700.0f, -26800.0f, 500.0f, 1500.0f, depth, 1, 1);
            break;
        case 0xE:
            YarareInitCube(this, -2600.0f, 0.0f, -27000.0f, 2600.0f, 6000.0f, depth, 1, 1);
            YarareAddCube(this, &EMMARK(this)->hit[0], 400.0f, 2200.0f, -26800.0f, 400.0f, 1500.0f, depth, 1, 1);
            YarareAddCube(this, &EMMARK(this)->hit[1], -2300.0f, 3600.0f, -26800.0f, 2300.0f, 1800.0f, depth, 1, 1);
            YarareAddCube(this, &EMMARK(this)->hit[2], -600.0f, 3900.0f, -26800.0f, 700.0f, 1400.0f, depth, 1, 1);
            break;
        case 0xF:
            YarareInitCube(this, 2600.0f, 0.0f, -27000.0f, 2600.0f, 6000.0f, depth, 1, 1);
            YarareAddCube(this, &EMMARK(this)->hit[0], 1400.0f, 2200.0f, -26800.0f, 1400.0f, 1600.0f, depth, 1, 1);
            YarareAddCube(this, &EMMARK(this)->hit[1], 2300.0f, 3600.0f, -26800.0f, 2300.0f, 1800.0f, depth, 1, 1);
            break;
        }
    }
    if (this->type == 6) {
        EstSet((int) this, -1, 0, 0, 0x33, 7, 0, 0, (u32) this, 0);
    }
    EMMARK(this)->pInst = inst;
    EMMARK(this)->age = 0;
}

EmMarkFunc emmark_tbl[6] = {
    emmark_begin,
    emmark_end,
    emmark_stay,
    emmark_move,
    emmark_none,
    0,
};

void cEmMark::move()
{

    damageCheck();
    if (xFC > 1) {
        downCheck();
    }
    emmark_tbl[xFC](this);
    switch (xFC) {
    case 2:
    case 3:
        EMMARK(this)->age++;
        break;
    }
    matUpdate();
}

int countOldMark(cEmMark* self, int age)
{
    int n = 0;
    cEm* em;

    for (em = (cEm*) EmMgr.pAlive; em; em = (cEm*) em->next) {
        if (em == self) {
            continue;
        }
        if ((em->be_flag & 0x201) != 1) {
            continue;
        }
        if (em->id != 0x3E) {
            continue;
        }
        if (em->hp <= 0) {
            continue;
        }
        switch (em->type) {
        case 2:
        case 3:
            break;
        default:
            if (EMMARK(em)->age >= age) {
                n++;
            }
            break;
        }
    }
    return n;
}

void cEmMark::downCheck()
{

    if (type != 2) {
        return;
    }
    if (countOldMark(this, EMMARK(this)->age)) {
        EMMARK(this)->downTimer = 10;
    } else if (EMMARK(this)->downTimer > 0) {
        EMMARK(this)->downTimer--;
    } else {
        setDown();
    }
}

static void emmark_begin(cEmMark* em)
{
    int st = em->xFD;

    switch (st) {
    case 0:
        if (em->type <= 9) {
            SndCall(6, 0, &em->pos, 0, 0, 0);
        }
        em->xFD = 1;
        em->xFE = 0;
        em->rot.x = PI / 2.0f;
        break;
    case 1:
        em->xFE++;
        em->rot.x -= PI / 20.0f;
        if (em->xFE > 9) {
            em->rot.x = 0.0f;
            em->setNextInstruction();
        }
        break;
    }
}

static void emmark_end(cEmMark* em)
{
    switch (em->xFD) {
    case 0:
        if (em->hp > 0) {
            em->xFE = 10;
            em->xFD = 1;
        } else {
            em->xFD = 10;
        }
        em->hp = 0;
        break;
    case 1:
        em->xFE--;
        if (em->xFE == 0) {
            em->xFD = 10;
        }
        break;
    case 0xA:
        SndCall(6, 0, &em->pos, 0, 0, 0);
        em->xFE = 0;
        em->xFD = 0xB;
        break;
    case 0xB:
        em->xFE++;
        em->rot.x += PI / 20.0f;
        if (em->xFE > 9) {
            em->xFD = 0xC;
        }
        break;
    case 0xC:
        EmMgr.destroy(em);
        em->xFD = 0xD;
        break;
    case 0xD:
        break;
    }
}

static void emmark_stay(cEmMark* em)
{
    EmMarkInst* inst = EMMARK(em)->pInst;

    switch (em->xFD) {
    case 0:
        EMMARK(em)->timer = inst->count;
        em->xFD = 1;
        break;
    case 1:
        EMMARK(em)->timer--;
        if (EMMARK(em)->timer <= 0) {
            em->setNextInstruction();
        }
        break;
    }
    em->standSpring();
}

static void emmark_move(cEmMark* em)
{
    EmMarkInst* inst = EMMARK(em)->pInst;
    Vec target;
    Vec dir;

    switch (em->xFD) {
    case 0:
        em->xFD = 1;
    case 1:
        target.x = (f32) inst->x;
        target.y = (f32) inst->y;
        target.z = (f32) inst->z;
        if (GetDistance(target, em->pos) > (f32) (inst->spd * inst->spd)) {
            PSVECSubtract(&target, &em->pos, &dir);
#line 533 "D:/Bio4/Prog/emmark.cpp"
            VECNormalize(&dir, &dir);
            PSVECScale(&dir, &dir, (f32) inst->spd);
            PSVECAdd(&em->pos, &dir, &em->pos);
        } else {
            em->pos = target;
            em->setNextInstruction();
        }
        break;
    }
    em->standSpring();
}

static void emmark_none(cEmMark* em)
{
}

void cEmMark::damageCheck()
{
    int eff;

    if (hp > 0 && dmHit) {
        if (type > 9) {
            setEffWall();
            dmg.clear();
            return;
        }
        hp--;
        if (dmWep == 0x12 || dmWep == 0x13) {
            eff = 5;
        } else if (type == 6 && dmPart == &EMMARK(this)->hit[3]) {
            eff = 2;
        } else {
            eff = dmPart == &hitInfo;
        }
        if (hp <= 0) {
            setEff(1, eff);
            R22cHitMark(type, eff != 0, &dmPart->pos, 1, EMMARK(this)->age);
            xFC = 1;
            xFD = 0;
            xFE = 0;
            xFF = 0;
        } else {
            setEff(0, eff);
            R22cHitMark(type, eff != 0 ? 3 : 2, &dmPart->pos, 0, EMMARK(this)->age);
            rot.x -= PI / 7.0f;
            dmg.clear();
        }
    }
}

int cEmMark::setEff(int a, int kind)
{
    const f32 range = 6000.0f;
    Vec p;

    p = pos;
    if (type != 3) {
        p.y += 1000.0f;
    } else {
        p.y -= 500.0f;
    }
    if (type == 3) {
        int k;

        k = 9;
        if (dmWep != 7 && dmWep != 0x13) {
            k = 8;
        }
        EstSet((int) this, -1, &pos, 0, 0x33, k, 0, 0, (u32) this, 0);
        SndCall(6, 7, &p, 0, 0, 0);
        be_flag &= ~2;
    } else if (type == 2) {
        int k = dmWep == 7;

        EstSet((int) this, -1, &dmPart->pos, 0, 0x33, k, 0, 0, (u32) this, 0);
        SndCall(6, 1, &p, 0, 0, 0);
    } else if (kind == 2) {
        modelInit(ARC(0x13), ARC(0x14));
        EstSet((int) this, -1, &pos, 0, 0x33, 6, 0, 0, (u32) this, 0);
        SndCall(6, 1, &p, 0, 0, 0);
        PlWepHitCheck2(0, &pos, &pos, 0x13, 0, range);
    } else if (kind != 0) {
        headBomb();
    } else if (dmWep == 0x13) {
        static const Vec up = { 0.0f, 1500.0f, 0.0f };
        static const Vec down = { 0.0f, -500.0f, 0.0f };

        headBomb();
        if (type != 3) {
            PSVECAdd(&pos, &up, &dmPart->pos);
        } else {
            PSVECAdd(&pos, &down, &dmPart->pos);
        }
    } else {
        int k = dmWep == 7;

        EstSet((int) this, -1, &dmPart->pos, 0, 0x33, k, 0, 0, (u32) this, 0);
        SndCall(6, 1, &p, 0, 0, 0);
    }
    return 1;
}

int cEmMark::setEffWall()
{

    switch (type) {
    case 0xC:
        if (dmPart == &EMMARK(this)->hit[0]) {
            R22cHitEffect(0);
        } else {
            setEffWallNormal();
        }
        break;
    case 0xD:
        if (dmPart == &EMMARK(this)->hit[0]) {
            R22cHitEffect(1);
        } else if (dmPart == &EMMARK(this)->hit[1]) {
            R22cHitEffect(2);
        } else if (dmPart == &EMMARK(this)->hit[2]) {
            R22cHitEffect(3);
        } else {
            setEffWallNormal();
        }
        break;
    case 0xE:
        if (dmPart == &EMMARK(this)->hit[0]) {
            R22cHitEffect(5);
        } else if (dmPart == &EMMARK(this)->hit[1]) {
            R22cHitEffect(4);
        } else if (dmPart == &EMMARK(this)->hit[2]) {
            R22cHitEffect(6);
        } else {
            setEffWallNormal();
        }
        break;
    case 0xF:
        if (dmPart == &EMMARK(this)->hit[0]) {
            R22cHitEffect(5);
        } else if (dmPart == &EMMARK(this)->hit[1]) {
            R22cHitEffect(4);
        } else {
            setEffWallNormal();
        }
        break;
    }
    return 1;
}

int cEmMark::setEffWallNormal()
{
    Vec p;

    int k = dmWep == 7;

    EstSet((int) this, -1, &dmPart->pos, 0, 0x33, k, 0, 0, (u32) this, 0);
    p.x = pos.x;
    p.y = pos.y + 1000.0f;
    p.z = pos.z;
    return 1;
}

void cEmMark::headBomb()
{
    void* bin;
    void* tpl;
    int kind;

    switch (dmWep) {
    case 2:
    case 0xB:
    case 0xC:
    default:
        switch (type) {
        case 0:
        default:
            kind = 2;
            bin = ARC(7);
            tpl = ARC(8);
            break;
        case 1:
            kind = 3;
            bin = ARC(0xB);
            tpl = ARC(0xC);
            break;
        case 2:
        case 3:
            kind = 8;
            bin = ARC(7);
            tpl = ARC(8);
            break;
        case 4:
            kind = 3;
            bin = ARC(7);
            tpl = ARC(8);
            break;
        case 5:
            kind = 3;
            bin = ARC(7);
            tpl = ARC(8);
            break;
        case 6:
            kind = 2;
            bin = ARC(0x11);
            tpl = ARC(0x12);
            break;
        }
        break;
    case 7:
    case 0x13:
        switch (type) {
        case 0:
        default:
            kind = 4;
            bin = ARC(7);
            tpl = ARC(8);
            break;
        case 1:
            kind = 5;
            bin = ARC(0xB);
            tpl = ARC(0xC);
            break;
        case 2:
        case 3:
            kind = 9;
            bin = ARC(7);
            tpl = ARC(8);
            break;
        case 4:
            kind = 5;
            bin = ARC(7);
            tpl = ARC(8);
            break;
        case 5:
            kind = 5;
            bin = ARC(7);
            tpl = ARC(8);
            break;
        case 6:
            kind = 4;
            bin = ARC(0x11);
            tpl = ARC(0x12);
            break;
        }
        break;
    }
    modelInit(bin, tpl);
    EstSet((int) this, -1, &pos, 0, 0x33, kind, 0, 0, (u32) this, 0);
    SndCall(6, 2, &pos, 0, 0, 0);
}

void cEmMark::setNextInstruction()
{
    int size;

    switch (EMMARK(this)->pInst->type) {
    case 0:
        size = 4;
        break;
    case 1:
        size = 4;
        break;
    case 2:
        size = 8;
        break;
    case 3:
        size = 0x14;
        break;
    case 4:
        size = 0;
        break;
    default:
        pLog->err(0, 0, "cEmMark::setNextInst() ERR %d", EMMARK(this)->pInst->type);
        return;
    }
    EMMARK(this)->pInst = (EmMarkInst*) ((u8*) EMMARK(this)->pInst + size);
    xFC = ((u8*) EMMARK(this)->pInst)[3];
    xFD = 0;
    xFE = 0;
    xFF = 0;
}

void cEmMark::standSpring()
{
    const f32 step = PI / 20.0f;

    if (rot.x < 0.0f) {
        if (rot.x < -step) {
            rot.x += step;
        } else {
            rot.x = 0.0f;
        }
    }
}

void cEmMark::setDown()
{
    if (hp <= 0) {
        return;
    }
    if (dmHit != 0) {
        return;
    }
    xFC = 1;
    xFD = 0;
    xFE = 0;
    xFF = 0;
}

extern "C" void _prolog()
{
    OSReport("emMark prolog Ok\n");
    EmInitFunc = em3eInit;
}

extern "C" void _epilog()
{
}

extern "C" void _unresolved()
{
}
