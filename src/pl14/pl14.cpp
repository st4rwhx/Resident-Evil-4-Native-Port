// pl14 module (D:/Bio4/Prog/pl14.cpp): Luis, the partner of the cabin fight (room 11C). See pl14.h.
// Matching notes: the byte flag fields are s8 (their bit clears compile to full-width rlwinm masks);
// `default:` comes first in most switches; two-case switches whose tree tests 1 before 0 carry an
// empty `case 2:`; cAnalysis::move's scan is a while loop with the scan in its condition.

#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "pl14.h"
#include "pl_npc.h"
#include "pl_sub.h"
#include "pl_wep.h"
#include "pl_mod.h"
#include "pl_cloth.h"
#include "global.h"
#include "motion.h"
#include "esp.h"
#include "est.h"
#include "em_sub.h"
#include "emhit.h"
#include "at_mod.h"
#include "dmg.h"
#include "etc_model.h"
#include "route_ck.h"
#include "sce_at.h"
#include "snd.h"
#include "mes.h"
#include "rnd.h"
#include "math_sub.h"
#include "db_log.h"

extern "C" void OSReport(const char* fmt, ...);
extern void (*EmInitFunc)(cEm* em);              // game/em.cpp
extern void (*ObjInitFunc[0x40])(cObj*);        // game/obj.cpp
u16 MotionMoveF(cModel* m, int flag) asm("MotionMove");

#line 1 "D:/Bio4/Prog/pl14.cpp"

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)
#define SUBARC(no) PL_ARC_PTR(subSelf->subArc, no)
#define OARC(no) PL_ARC_PTR(owner->subArc, no)
#define EM ((cEm*) this)
#define OEM ((cEm*) owner)
#define LITEM ((LuisItemWork*) work)
static inline void U32And(u32& d, u32 m) { d &= m; }
// Reference store: a MEM with neither the struct nor the scalar flag keeps a following member load below it.
static inline void PSet(void*& d, void* v) { d = v; }

static inline void RoutineSet(cSubLuis* o, int r0)
{
    o->xFC = r0;
    o->xFD = 0;
    o->xFE = 0;
    o->xFF = 0;
}

static inline void RoutineStepClear(cSubLuis* o)
{
    o->xFF = 0;
    o->xFE = 0;
    o->xFD = 0;
}

// LuisInit is public and defined before the first initialised public object: the static initializer's
// key (`global constructors keyed to LuisInit`) is the first public function/initialised object assembled.
void LuisInit(cEm* em)
{
    cSubLuis* luis = new (em) cSubLuis();
    luis->modelSet();
    luis->init();
    luis->equipWeapon();
}

// Routine handlers by routine number (owner->xFC); 4 (event) calls cSubLuis::evFunc instead.
void (cRoutine::*cRoutine_move_tbl[18])() = {
    &cRoutine::moveFootwork,
    &cRoutine::moveDamage,
    &cRoutine::moveDie,
    &cRoutine::moveEvent,
    0,
    &cRoutine::moveWalk,
    &cRoutine::moveRun,
    &cRoutine::moveTurn,
    &cRoutine::moveTurn180,
    &cRoutine::moveWepReady,
    &cRoutine::moveWepSet,
    &cRoutine::moveWepFire,
    &cRoutine::moveWepDown,
    &cRoutine::moveThrowItem,
    &cRoutine::moveDown,
    &cRoutine::moveAvoid,
    &cRoutine::moveUp,
    &cRoutine::moveBlast,
};

static int luisBlink = 0;        // frames to the next eye shift
static int luisUnused = 0;       // never read (the second .data word)

static cMot3Rate luisEye;        // [0] current eye yaw, [1] target, [2] mix

cSubLuis::cSubLuis()
{
    routine.init(this);
    action.flags = 0;
    action.init(this);
    analysis.flags = 0;
    analysis.init(this);
    flags = 0;
    pFootShadowTbl = pl_fs_tbl;
    subSelf = this;
    pSUB = (cSubChar*) this;
    luisEye.r[0] = luisEye.r[1] = 0.0f;   // chain: r[1] first in RTL, the 0.0 dies at r[0] (issued first)
    luisEye.r[2] = 0.4f;
}

cSubLuis::~cSubLuis()
{
    ObjMgr.destroy(pItem);
    PSet((void*&) pSUB, 0);   // the inlined ~cUnit's be_flag load stays below the store
}

// The light info origin both models use (one static: the inline is expanded where it is defined).
// A helper that RETURNS the address keeps the init2 argument order (`&zero` evaluated before `&size`);
// a helper taking `size` by pointer evaluates the parameter copy first (embarrel idiom).
static inline const Vec* LuisLightZero()
{
    static const Vec zero = { 0.0f, 0.0f, 0.0f };
    return &zero;
}

void cSubLuis::init()
{
    static const Vec p1 = { 1000.0f, 1000.0f, 0.0f };

    x12D = 1;
    lightInfo.init2(0, 1, LuisLightZero(), &p1, 0x40);
    // COMPILER-DIFF: #1 (FPR argument moves before the int `li`s)
    atariInitF(&atari, 0.0f, -200.0f, 0.0f, 300.0f, 200.0f, 400.0f, 900.0f, 1, 0x1000, 10);
    {
        cSubLuis* s = subSelf;
        s->lockParts = 4;
        s->lockOfs.x = 0.0f;
        s->lockOfs.y = 0.0f;
        s->lockOfs.z = 0.0f;
    }
    setStatus(1);
    hp = hpMax = 0x4B0;
    dmgCnt = 5;
    be_flag |= 0x2000000;
    lifeOld = pGS->pl_life;   // struct view: the pG load does not wait for the dmgCnt byte store
    voiceWait = 0;
    cnt = 0;
    x38D = 0;
    EspDataLoad((u32) SUBARC(0x34 / 4), 7, 0);
    PlClothSetLuis(this, &luisHair);
    YarareInit(EM, 0.0f, -30.0f, 0.0f, 150.0f, 100.0f, 2, 1);
    YarareAdd(EM, &hit[0], 0.0f, 0.0f, 0.0f, 170.0f, 120.0f, 3, 1);
    YarareAdd(EM, &hit[1], -20.0f, -300.0f, 0.0f, 120.0f, 300.0f, 0x13, 1);
    YarareAdd(EM, &hit[2], 20.0f, -300.0f, 0.0f, 120.0f, 300.0f, 0x17, 1);
    YarareAdd(EM, &hit[3], 0.0f, 0.0f, 20.0f, 140.0f, 65.0f, 5, 1);
    YarareAdd(EM, &hit[4], -20.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x14, 1);
    YarareAdd(EM, &hit[5], 20.0f, -400.0f, 0.0f, 150.0f, 400.0f, 0x18, 1);
    YarareAdd(EM, &hit[6], -350.0f, 0.0f, 0.0f, 100.0f, 350.0f, 9, 3);
    YarareAdd(EM, &hit[7], 0.0f, 0.0f, 0.0f, 100.0f, 350.0f, 0xF, 3);
    YarareAdd(EM, &hit[8], -180.0f, 0.0f, 0.0f, 120.0f, 180.0f, 8, 3);
    YarareAdd(EM, &hit[9], 0.0f, 0.0f, 0.0f, 120.0f, 180.0f, 0xE, 3);
    MotionSetCore(subSelf, &subSelf->mot, SUBARC(0x40 / 4), 0, 0, 5, 0);
    motionMove();
    getRoomEtcRack(0, &rack[0], 1);
    getRoomEtcRack(1, &rack[1], 1);
    getRoomEtcRack(2, &rack[2], 1);
}

void cSubLuis::modelSet()
{
    cModelInfo* info;

    if (!modelInit(SUBARC(0x10 / 4), SUBARC(0x14 / 4))) {
        pLog->err(0, 0, "cSubLuis::init() failed.");
    }
    info = ModInfoMgr.create(SUBARC(0x18 / 4), SUBARC(0x1C / 4));
    if (info) addModel(info);
    pFace = ModInfoMgr.create(SUBARC(0x20 / 4), SUBARC(0x1C / 4));
    if (pFace) addModel(pFace);
    if (0) pLog->err(0, 0, "setFace() FAILED. %d", 0);
    info = ModInfoMgr.create(SUBARC(0x24 / 4), SUBARC(0x14 / 4));
    if (info) addModel(info);
    info = ModInfoMgr.create(SUBARC(0x28 / 4), SUBARC(0x14 / 4));
    if (info) addModel(info);
    info = ModInfoMgr.create(SUBARC(0x2C / 4), SUBARC(0x14 / 4));
    if (info) addModel(info);
}

void cSubLuis::move()
{
    U32And(pG->flags_5010, ~0x10000);
    U32And(pG->flags_5014, ~0x20000000);
    damageCheck();
    analysis.move();
    think();
    action.move(&analysis, &routine);
    routine.move();
    moveEye();
    neckMove();
    partsWorldCalc();
    PlClothMoveLuis(this, &luisHair);
    EmAtCheck(EM);
    SatMgr.check(this, 0);
    atari.move();
    PartsWorldPosCalc(this);
    seqSeCtrl();
}

// Flag tests through a u8-parameter inline: integrate copies the byte into a QImode pseudo
// (`lbz r0; mr r11, r0` PRE copies, one shared `clrlwi` per extended block); a promoted u8/int local
// or a direct `flags & bit` read gives SImode pseudos and no copies.
static inline int Chk8(u8 f, int b) { return f & b; }

void cSubLuis::think()
{
    static const Vec upPos = { 112160.0f, 3182.64f, -51016.84f };

    {
        cAction* a = &action;
        if (a->mode == 5) return;
        if (a->mode == 6) return;
    }

    if (Chk8(flags, 1)) {
        if (xFC == 4) action.set(6);
        else action.set(5);
        flags &= ~1;
        analysis.flags &= ~4;
    } else {
        if (Chk8(analysis.flags, 0x10)) {
            action.set(0xA);
        } else if (!Chk8(analysis.flags, 2) && Chk8(analysis.flags, 4)) {
            action.set(9);
        } else if (Chk8(flags, 2)) {
            action.set(4);
        } else if (Chk8(analysis.flags, 2)) {
            action.set(8);
        } else if (x38D == 2 && !Chk8(flags, 4)) {
            action.set(3);
            if (GetDistance(*(Vec*) &upPos, pos) < 1000000.0f) flags |= 4;
        } else if (x38D == 1 && (rackCheck() || Chk8(analysis.flags, 0x80))) {
            action.set(0xC);
        } else if (x38D == 1 && !(action.flags & 2)) {
            action.set(0xB);
        } else if (analysis.pTarget) {
            if (Chk8(analysis.flags, 8) && !stairCheck(pPL) && !stairCheck(this) && sameFloorCheck(this, pPL) &&
                (s16) pG->pl_life > 0) {
                action.set(7);
                analysis.flags &= ~8;
            } else {
                action.set(1);
            }
        } else {
            action.set(2);
        }
    }

    if ((s16) pG->pl_life != lifeOld && (s16) pG->pl_life > 0 && sameFloorCheck(this, pPL)) {
        if (voiceWait == 0) {
            lifeOld = pG->pl_life;
            routine.voice.set(0x5A, 0x10, 60);
            voiceWait = 0x5A;
        }
    }
    if (voiceWait) voiceWait--;

    if (cnt == 1) {
        if (Rnd() & 0x30) routine.voice.set(0x5B, 7, 60);
        else routine.voice.set(0x5C, 8, 60);
    }
    if (cnt) cnt--;
}

int cSubLuis::rackCheck()
{
    static const Vec rackPos = { 107455.0f, 4.0f, -47540.0f };

    // Pool order (dist before zlim) and, with zlim a const local, the pos.z load is issued before the
    // pool `lis` (the rack pointer's r9 is then reused for the high half, pos.z lands in f0).
    const f32 dist = 9000000.0f;
    const f32 zlim = -49000.0f;

    if (rack[0]->hp <= 0) return 0;
    if (rack[0]->pos.z < zlim) return 0;
    if (GetDistance(&pos, (Vec*) &rackPos) > dist) return 0;
    analysis.flags |= (u8) 0x80;
    return 1;
}

void cRoutine::init(cSubLuis* o)
{
    owner = o;
    o->xFF = 0;
    o->xFE = 0;
    o->xFD = 0;
    o->xFC = 0;
    saved[2] = 0xFF;
    saved[1] = 0xFF;
    saved[0] = 0xFF;
    shotCnt = 0;
    set(0);
}

int cRoutine::move()
{
    if (owner->xFC == 4) {
        owner->evFunc();
    } else {
        (this->*cRoutine_move_tbl[owner->xFC])();
    }
    voice.move();
    return 1;
}

void cRoutine::moveFootwork()
{
    if (owner->xFD == 0) {
        owner->motionSet(OARC(0x40 / 4), 5, 0, 5, 0);
        owner->xFD = 1;
    }
    owner->motionMove();
}

void cRoutine::moveDamage()
{
    void* mot = 0;

    switch (owner->xFD) {
    case 0:
        switch (x110) {
        case 0: mot = OARC(0xB4 / 4); owner->xFD = 1; break;
        case 1: mot = OARC(0xB8 / 4); owner->xFD = 1; break;
        case 2: mot = OARC(0xBC / 4); owner->xFD = 1; break;
        case 3: mot = OARC(0xC0 / 4); owner->xFD = 1; break;
        case 4: mot = OARC(0xC4 / 4); owner->xFD = 1; break;
        case 5: mot = OARC(0xC8 / 4); owner->xFD = 1; break;
        case 6: mot = OARC(0xCC / 4); owner->xFD = 0xA; break;
        case 7: mot = OARC(0xD4 / 4); owner->xFD = 0xA; break;
        case 8: mot = OARC(0xDC / 4); owner->xFD = 0x14; break;
        }
        MotionSetCore(owner, &owner->mot, mot, 0, 3, 1, 0);
        SndCall(8, 9, &owner->pParts->worldPos, owner->id, 0, 0);
        owner->cnt = 0;
    case 1:
        if (MotionCheckCrossFrame(&owner->mot, 20.0f) && x114 && sameFloorCheck(owner, pPL)) {
            switch (x114) {
            case 4: voice.set(0x5D, 3, 60); break;
            case 3: voice.set(0x5E, 4, 60); break;
            case 2: voice.set(0x5F, 5, 60); break;
            case 1: voice.set(0x60, 6, 60); break;
            }
            x114 = 0;
        }
        if (owner->motionMove()) {
            owner->dmg.clear();
            owner->xFD = 0x32;
        }
        break;
    case 0xA:
        MotionMoveF(owner, 0);
        break;
    case 0x14:
        if (MotionMoveF(owner, 0)) owner->xFD = 0x15;
        break;
    case 0x15:
        MotionSetCore(owner, &owner->mot, OARC(0xD8 / 4), 0, 3, 1, 0);
        owner->xFD = 0x16;
    case 0x16:
        if (MotionMoveF(owner, 0)) {
            owner->dmg.clear();
            owner->xFD = 0x32;
        }
        break;
    case 0x32:
        owner->xFD = 0x33;
        break;
    case 0x33:
        end();
        break;
    }
}

void cRoutine::moveDie()
{
    switch (owner->xFD) {
    case 0:
        MotionSetCore(owner, &owner->mot, OARC(0xCC / 4), (int) OARC(0xD0 / 4), 5, 1, 0);
        SndCall(1, 0xD, &owner->getPartsPtr(4)->worldPos, owner->id, 0, 0);
        owner->dmType |= 0x80;
        owner->atari.partsNo = 4;
        owner->xFD = 1;
        break;
    case 1:
        if (owner->motionMove()) owner->xFD = 2;
        break;
    case 2:
        owner->motionMove();
        break;
    }
}

void cRoutine::moveEvent()
{
}

void cRoutine::moveWalk()
{
    Vec out;

    RouteCkToPos(OEM, &target, &out, 0, 0);
    if (owner->xFD == 0) {
        owner->motionSet(OARC(0x58 / 4), 5, 0, 5, 0);
        owner->xFD = 1;
    }
    owner->rot.y += Muku(&owner->pos, &out, owner->rot.y, 0.20943952f);
    owner->motionMove();
    if (GetDistance(&owner->pos, &target) < dist * dist) end();
}

void cRoutine::moveRun()
{
    Vec out;
    int r;

    r = RouteCkToPos(OEM, &target, &out, 0, 0);
    if (owner->xFD == 0) {
        owner->motionSet(OARC(0x68 / 4), 5, 0, 5, 0);
        owner->xFD = 1;
    }
    owner->rot.y += Muku(&owner->pos, &out, owner->rot.y, 0.20943952f);
    owner->motionMove();
    if (GetDistance(&owner->pos, &target) < dist * dist && r == 1) end();
}

void cRoutine::moveWepReady()
{
    switch (owner->xFD) {
    case 0:
        owner->motionSet(OARC(0x78 / 4), 10, 0, 1, 0);
        owner->xFD = 1;
    case 1:
        if (pTarget) owner->rot.y += Muku(&owner->pos, &pTarget->pos, owner->rot.y, 0.44879895f);
        if (owner->motionMove()) {
            end();
            set(0xA);
        }
        break;
    }
}

void cRoutine::moveWepSet()
{
    Vec d;

    switch (owner->xFD) {
    case 0:
        mot3.set(owner, OARC(0x7C / 4), OARC(0x94 / 4), OARC(0x98 / 4), 0, 3, 0, 4, 0);
        if (VALID_PTR(pTarget) && VALID_PTR(pTarget->pParts)) {
            PSVECSubtract(&pTarget->pParts->worldPos, &owner->pParts->worldPos, &d);
            rate = VecElevation(&d);
        } else {
            owner->motionMove();
            RoutineSet(owner, 0xC);
            break;
        }
        owner->xFD = 1;
        end();
    case 1:
        mot3.move(rate);
        if (VALID_PTR(pTarget)) owner->rot.y += Muku(&owner->pos, &pTarget->pos, owner->rot.y, 0.20943952f);
        break;
    }
    owner->motionMove();
}

void cRoutine::moveWepFire()
{
    Vec d;
    f32 a;
    const f32 lim = 0.19634955f;   // pool order: the fabsf limit precedes Muku's PI/8

    switch (owner->xFD) {
    case 0:
        if (pTarget == 0) {
            end();
            break;
        }
        PSVECSubtract(&pTarget->pParts->worldPos, &owner->pParts->worldPos, &d);
        rate = VecElevation(&d);
        mot3.set(owner, OARC(0x7C / 4), OARC(0x94 / 4), OARC(0x98 / 4), 0, 3, 0, 4, 0);
        owner->xFD = 1;
    case 1:
        mot3.move(rate);
        if (pTarget == 0) {
            end();
            break;
        }
        a = Muku(&owner->pos, &pTarget->pos, owner->rot.y, 0.3926991f);
        owner->rot.y += a;
        owner->motionMove();
        if (fabsf(a) < lim) owner->xFD = 2;
        break;
    case 2:
        if (!isTarget(owner, pTarget)) {
            if (pTarget && pTarget->hp <= 0) {
                cnt++;
                switch (cnt) {
                case 10: voice.set(0x62, 0xE, 60); break;
                case 30: voice.set(0x63, 0xF, 60); break;
                }
            }
            pTarget = 0;
            owner->motionMove();
            RoutineSet(owner, 0xB);
            break;
        }
        shotCnt++;
        if (shotCnt <= 10) {
            mot3.set(owner, OARC(0xF4 / 4), OARC(0xF8 / 4), OARC(0xFC / 4), 0, 3, 0, 4, 0);
            shot();
        } else {
            owner->motionSet(OARC(0x100 / 4), 10, 0, 1, 0);
            shotCnt = 0;
        }
        owner->xFD = 3;
    case 3:
        if (shotCnt) mot3.move(rate);
        if (owner->motionMove()) {
            if (SatMgr.hitCheck(&owner->pParts->worldPos, &pTarget->pParts->worldPos, 0, 0, 0, 0)) {
                RoutineSet(owner, 0xB);
            } else if (pTarget->hp > 0) {
                RoutineSet(owner, 0xB);
            } else {
                RoutineSet(owner, 0xB);
            }
        }
        break;
    }
}

void cRoutine::moveWepDown()
{
    switch (owner->xFD) {
    case 0:
        owner->motionSet(OARC(0x88 / 4), 10, 0, 1, 0);
        owner->xFD = 1;
    case 1:
        if (owner->motionMove()) end();
        break;
    case 2:
        break;
    }
}

void cRoutine::moveThrowItem()
{
    switch (owner->xFD) {
    case 0:
        owner->motionSet(OARC(0x124 / 4), 10, 0, 1, 0);
        voice.set(0x59, 1, 60);
        owner->xFD = 1;
    case 1:
        if (MotionCheckCrossFrame(&owner->mot, 18.0f)) setItem();
        if (owner->frame <= 30.0f) {
            owner->rot.y += Muku(&owner->pos, &pPL->pos, owner->rot.y, 0.31415927f);
        }
        if (owner->motionMove()) end();
        break;
    }
}

void cRoutine::setItem()
{
    cObjLuisItem* item = (cObjLuisItem*) ObjMgr.create(0x1E);
    item->init(&owner->getPartsPtr(10)->worldPos, owner->rot.y);
}

void cRoutine::moveDown()
{
    f32 a;

    switch (owner->xFD) {
    case 0:
        owner->motionSet(OARC(0x118 / 4), 10, 0, 1, 0);
        owner->xFD = 1;
    case 1:
        if (owner->motionMove()) {
            mot3.set(owner, OARC(0x11C / 4), OARC(0x128 / 4), OARC(0x12C / 4), 0, 3, 1, 4, 0);
            rate = 0.0f;
            owner->xFD = 2;
            end();
        }
        break;
    case 2:
        FSet(rate, rate * (PI / 2));   // reference store: the pPL load stays below it
        a = Muku(&owner->pos, &pPL->pos, owner->rot.y - rate, 0.31415927f);
        rate = (rate - a) / (PI / 2);
        if (rate > 1.0f) rate = 1.0f;
        else if (rate < -1.0f) rate = -1.0f;
        mot3.move(rate);
        owner->motionMove();
        break;
    }
}

void cRoutine::moveUp()
{
    switch (owner->xFD) {
    case 0:
        owner->motionSet(OARC(0x120 / 4), 10, 0, 1, 0);
        owner->xFD = 1;
    case 1:
        if (owner->motionMove()) end();
        break;
    }
}

void cRoutine::moveBlast()
{
    switch (owner->xFD) {
    case 0:
        owner->motionSet(OARC(0x130 / 4), 10, 0, 1, 0);
        owner->xFD = 1;
    case 1:
        if (owner->motionMove()) end();
        break;
    }
}

void cRoutine::moveAvoid()
{
    switch (owner->xFD) {
    case 0:
        if (GetDistance(&pPL->pos, &pSUB->pos) < 9000000.0f) {
            owner->rot.y = LIMIT_ANGLE(pPL->rot.y + PI);
        } else {
            owner->rot.y += Muku(&owner->pos, &pPL->pos, owner->rot.y, 2 * PI);
        }
        owner->motionSet(OARC(0x114 / 4), 10, 0, 1, 0);
        owner->dmg.set(0, 0x80);
        owner->xFD = 1;
    case 1:
        if (owner->motionMove()) {
            owner->dmg.clear();
            end();
        }
        break;
    }
}

void cRoutine::moveTurn()
{
    void* mot;

    if (owner->xFD == 0) {
        if (x110) mot = OARC(0x50 / 4);
        else mot = OARC(0x48 / 4);
        owner->motionSet(mot, 5, 0, 5, 0);
        owner->xFD = 1;
    }
    owner->motionMove();
    if (x114) {
        x114--;
        if (x114 == 0) end();
    }
}

void cRoutine::moveTurn180()
{
    if (owner->xFD == 0) {
        owner->motionSet(OARC(0x70 / 4), 5, 0, 5, 0);
        owner->xFD = 1;
    }
    if (owner->motionMove()) end();
}

// Starts routine `no` when its priority allows it (1) or refuses (0).
int cRoutine::set(int no)
{
    int p;
    int r;
    int ret;

    switch (no) {
    default:
        pLog->err(0, 0, "LUIS: unknnown routine was set %d", no);
        return 0;
    case 0: p = 0; r = 0; break;
    case 1: p = 2; r = 1; break;
    case 5: p = 0; r = 5; break;
    case 6: p = 0; r = 6; break;
    case 7: p = 0; r = 7; break;
    case 8: p = 0; r = 8; break;
    case 9: p = 0; r = 9; break;
    case 0xA: p = 0; r = 0xA; break;
    case 0xB: p = 1; r = 0xB; break;
    case 0xC: p = 0; r = 0xC; break;
    case 0xD: p = 1; r = 0xD; break;
    case 0xE: p = 1; r = 0xE; break;
    case 0xF: p = 2; r = 0xF; break;
    case 0x10: p = 1; r = 0x10; break;
    }

    if (p > prio) {
        saved[prio] = owner->xFC;
        prio = p;
        owner->xFC = r;
        RoutineStepClear(owner);
        flags &= ~1;
        ret = 1;
    } else if (p == prio) {
        owner->xFC = r;
        RoutineStepClear(owner);
        flags &= ~1;
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}

void cRoutine::end()
{
    prio = 0;
    flags |= 1;
}

int cRoutine::eor()
{
    int r = 0;
    if (flags & 1) r = 1;
    return r;
}

void cAction::init(cSubLuis* o)
{
    xE = 0;
    sub = 0;
    step = 0;
    owner = o;
    req = 2;
    mode = 2;
}

void cAction::move(cAnalysis* an, cRoutine* rt)
{
    switch (req) {
    case 0:
        if (step == 0) {
            rt->set(0);
            step = 1;
        }
        break;
    case 1: moveAttack(an, rt); break;
    case 2: moveChasePl(an, rt); break;
    case 3: moveGo2F(an, rt); break;
    case 4: moveAttackPl(an, rt); break;
    case 5:
        if (step == 0) {
            rt->set(1);
            step = 1;
        } else if (rt->eor()) {
            mode = 2;
            set(0);
        }
        break;
    case 6: break;
    case 7: moveGiveItem(an, rt); break;
    case 8: moveDown(an, rt); break;
    case 9: moveUp(an, rt); break;
    case 0xA: moveAvoid(an, rt); break;
    case 0xB: move11cBegin(an, rt); break;
    case 0xC: moveEscRack(an, rt); break;
    }
}

void cAction::moveAttack(cAnalysis* an, cRoutine* rt)
{
    switch (step) {
    case 0:
        if (an->pTarget) {
            rt->set(9);
            rt->pTarget = an->pTarget;
            step = 1;
        } else {
            rt->set(0);
        }
        break;
    case 1:
        if (rt->eor()) step = 2;
        break;
    case 2:
        rt->set(0xA);
        timer = (u8) (Rnd() % 30);
        step = 3;
        break;
    case 3:
        if (--timer == -1) {
            if (an->pTarget) {
                rt->set(0xB);
                rt->pTarget = an->pTarget;
                step = 4;
            } else {
                rt->pTarget = an->pTarget;
            }
        }
        break;
    case 4:
        if (rt->eor()) {
            if (an->pTarget) {
                if (an->pTarget != rt->pTarget && an->targetDist < 2000.0f) rt->pTarget = an->pTarget;
                if (!(rt->pTarget && (rt->pTarget->be_flag & 0x201) == 1 && rt->pTarget->hp > 0)) {
                    rt->pTarget = an->pTarget;
                }
                step = 3;
                timer = (u8) (Rnd() % 30);
            } else {
                rt->pTarget = an->pTarget;
                rt->set(0xC);
                step = 5;
            }
        }
        break;
    case 5:
        if (rt->eor()) step = 0;
        break;
    }
}

void cAction::moveGo2F(cAnalysis* an, cRoutine* rt)
{
    static const Vec stairPos = { 112500.0f, 1247.0f, -46690.0f };
    static const Vec upPos = { 112160.0f, 3182.64f, -51016.84f };
    const f32 lowY = 2500.0f;   // pool order: the player-height limit precedes the 1000.0 distance

    switch (step) {
    case 0:
        if (rt->set(6)) {
            rt->target = stairPos;
            rt->dist = 1000.0f;
            if (!(an->flags & 0x20)) {
                an->flags |= 0x20;
                if (pPL->pos.y < lowY) rt->voice.set(0x58, 2, 60);
            }
            step = 1;
        }
        break;
    case 1:
        if (rt->eor()) {
            rt->target = upPos;
            step = 2;
        }
        break;
    case 2:
        rt->eor();
        break;
    }
}

void cAction::moveAttackPl(cAnalysis* an, cRoutine* rt)
{
    if (step == 0) {
        pG->flags_174 |= 0x20000000;
        step = 1;
    }
}

void cAction::moveGiveItem(cAnalysis* an, cRoutine* rt)
{
    switch (step) {
    case 0:
        rt->set(0xD);
        rt->target = pPL->pos;
        rt->dist = 4000.0f;
        step = 1;
        break;
    case 1:
        rt->eor();
        break;
    }
}

void cAction::moveDown(cAnalysis* an, cRoutine* rt)
{
    switch (step) {
    case 0:
        rt->set(0xE);
        step = 1;
    case 1:
        if (rt->eor()) {
            an->flags |= 4;
            step = 2;
        }
        break;
    case 2:
        break;
    }
}

void cAction::moveUp(cAnalysis* an, cRoutine* rt)
{
    switch (step) {
    case 0:
        rt->set(0x10);
        step = 1;
    case 1:
        if (rt->eor()) {
            an->flags &= ~4;
            step = 2;
        }
        break;
    case 2:
        break;
    }
}

void cAction::moveAvoid(cAnalysis* an, cRoutine* rt)
{
    switch (step) {
    case 0:
        rt->set(0xF);
        step = 1;
    case 1:
        if (rt->eor()) an->flags &= ~0x10;
        break;
    }
}

void cAction::move11cBegin(cAnalysis* an, cRoutine* rt)
{
    cSubLuis* o = owner;

    switch (step) {
    case 0:
        if (flags & 1) {
            flags |= 2;
            break;
        }
        flags |= 1;
        rt->set(0);
        sub = 0;
        step = 1;
    case 1:
        switch (sub) {
        default: o->neckSet(-PI / 2, PI); break;
        case 0xF0: o->neckSet(-1.3962634f, PI); break;
        case 0xF1: o->neckSet(-1.0471976f, PI); break;
        case 0xF2: o->neckSet(-0.6981317f, PI); break;
        case 0xF3: o->neckSet(-0.34906584f, PI); break;
        case 0xF4: o->neckSet(-0.17453292f, PI); break;
        case 0xF5: o->neckSet(-0.08726646f, PI); break;
        case 0xF6: o->neckSet(-0.034906585f, PI); break;
        case 0xF7: o->neckSet(-0.017453292f, PI); break;
        case 0xF8:
            step = 2;
            flags |= 2;
            break;
        }
        sub++;
        break;
    case 2:
        break;
    }
}

void cAction::moveEscRack(cAnalysis* an, cRoutine* rt)
{
    static const Vec escPos = { 114536.0f, 4.0f, -51880.0f };

    switch (step) {
    case 0:
        if (rt->set(6)) {
            rt->target = escPos;
            rt->dist = 500.0f;
            step = 1;
        }
    case 1:
        if (rt->eor()) an->flags &= ~0x80;
        break;
    }
}

void cAction::moveChasePl(cAnalysis* an, cRoutine* rt)
{
    f32 plDist = an->plDist;

    switch (step) {
    case 0:
        rt->set(0);
        timer = (u8) (Rnd() % 90) + 30;
        step = 1;
    case 1:
        if (chasePlAreaCheck() == 1) {
            if (plDist > 5000.0f) step = 0x1E;
            else if (plDist > 2000.0f) step = 0x14;
        } else if (--timer == 0) {
            step = 2;
        }
        break;
    case 2:
        if (Rnd() & 7) {
            rt->set(7);
            rt->x110 = Rnd() & 1;
            rt->x114 = (u8) (Rnd() % 50) + 10;
        } else {
            rt->set(8);
        }
        step = 3;
        break;
    case 0x14:
        timer = 0;
        step = 0x15;
    case 0x15:
        if ((u32) ++timer > 210) step = 0x16;
        if (plDist > 5000.0f) step = 0x1E;
        break;
    case 0x16:
        rt->set(5);
        rt->target = pPL->pos;
        rt->dist = 1500.0f;
        step = 0x17;
        break;
    case 0x17:
        if (rt->eor()) step = 0;
        if (plDist > 5000.0f) step = 0x1E;
        break;
    case 0x1E:
        rt->set(6);
        rt->target = pPL->pos;
        rt->dist = 1500.0f;
        step = 0x1F;
    case 0x1F:
    case 3:
        if (rt->eor()) step = 0;
        break;
    }
}

void cAction::set(int m)
{
    int ok = 0;

    if (m == 6) {
        ok = 1;
    } else if (mode != 5) {
        if ((mode == 8 && m != 8) || (mode == 9 && m != 9)) {
            if (m == 0xA || m == 5 || step == 2) ok = 1;
        } else if (m != mode) {
            ok = 1;
        }
    }
    if (ok) {
        mode = m;
        req = m;
        xE = 0;
        sub = 0;
        step = 0;
    }
}

int cAction::chasePlAreaCheck()
{
    if ((pG->room_id32 & 0xFFFF0000) != 0x011C0000) return 1;
    switch (owner->x38D) {
    default:
        return 1;
    case 1:
        if (pPL->pos.z > -47920.0f || pPL->pos.y > 500.0f) return 0;
        return 1;
    case 2:
        if (pPL->pos.z > -47920.0f || pPL->pos.y < 3000.0f) return 0;
        return 1;
    }
}

void cAnalysis::init(cSubLuis* o)
{
    // Store order from the weight model: cnt is the zero's last use (issued first of the zero stores),
    // the byte RMW of flags comes last in source.
    owner = o;
    targetDist = 0.0f;
    pTarget = 0;
    idx = 0;
    cnt = 0;
    plDist = 1000000.0f;
    flags &= ~8;
}

// A door enemy between the two points.
int doorHitCheck(Vec* a, Vec* b)
{
    u32 i;

    for (i = 0; i < EmMgr.nArray; i++) {
        cEm* em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * i);
        if (em && (em->be_flag & 0x201) == 1 && em->hp > 0 && (em->id == 0x41 || em->id == 0x4E) &&
            emLineAtCk(em, a, b, 1e16f, 0)) {
            return 1;
        }
    }
    return 0;
}

void cAnalysis::move()
{
    cEm* found;
    cEm* em;
    u32 i;
    f32 d;

    cnt++;
    // Round-robin scan from the entry after idx, until a target is found or it wraps around.
    i = idx;
    while (!isTarget(owner, em = (cEm*) ((u8*) EmMgr.pArray + EmMgr.size * (i = (i + 1) % EmMgr.nArray)))) {
        if (i == idx) {
            found = 0;
            goto scanned;
        }
    }
    idx = i;
    // COMPILER-DIFF: register tie (global-alloc priority): the loop notes count em's refs double, so em
    // (r30) is allocated before `this` (r29) like the original.
    do { found = em; } while (0);
scanned:

    if (pTarget && !isTarget(owner, pTarget)) pTarget = 0;
    if (found && isTarget(owner, found)) {
        d = GetDistance(owner->pos, found->pos);
        if (pTarget) {
            if (d < GetDistance(owner->pos, pTarget->pos)) {
                pTarget = found;
                targetDist = d;
            }
        } else {
            pTarget = found;
            targetDist = d;
        }
    }

    plDist = RouteCkPosToPosDis(&owner->pos, &pPL->pos);
    aimCheck();
    if (cnt % 1800 == 0) flags |= 8;

    switch ((u32) greThrowCheck()) {   // unsigned range tests (cmplwi), the EQ tests stay cmpwi
    case 0x13:
        if (greCnt & 0x80) {
            greCnt = 0;
        } else {
            greCnt++;
            if (greCnt > 30) flags |= 0x10;
        }
        break;
    case 0x16:
    case 0x17:
        if (greCnt & 0x80) {
            greCnt = 0;
        } else {
            greCnt++;
            if (greCnt > 5) flags |= 0x10;
        }
        break;
    case 0xD:
        if (greCnt & 0x80) {
            greCnt = 0;
        } else {
            greCnt++;
            if (greCnt > 1) flags |= 0x10;
        }
        break;
    default:
        if (greCnt < -5) {
            if (flags & 0x10) flags &= ~0x10;
        } else {
            greCnt--;
        }
        break;
    }
}

// Is the player aiming at him? Sets flags bit1; returns 1 when the check applies to the weapon.
int cAnalysis::aimCheck()
{
    switch (pG->wep_no) {
    case 0x10:
        flags &= ~2;
        if ((PlGetStatus() & 0x10) && GetDistance(pPL->pos, pSUB->pos) < 4000000.0f &&
            Front_check(pPL, pSUB, pPL->rot.y)) {
            flags |= 2;
            return 1;
        }
        break;
    default:
        if (PlGetStatus() & 0x10) {
            if (pPL->pWep->pObj->wep.target == (cEm*) owner) {
                flags |= 2;
            } else {
                f32 dir = PlGetDirY();
                if (fabsf(GetXZAngleLocal(&pPL->pos, &owner->pos, dir)) > 0.17453292f) flags &= ~2;
            }
            return 1;
        }
    case 0x13:
    case 0x16:
    case 0x17:
        flags &= ~2;
        break;
    }
    return 0;
}

void cRoutine::shot()
{
    Vec p;
    Vec t;
    s16 hp;

    if (pTarget == 0) return;
    p.x = 0.0f;
    p.y = 1600.0f;
    p.z = 500.0f;
    PSMTXMultVecSR(owner->mat, &p, &p);
    PSVECAdd(&p, &owner->pos, &p);
    t = pTarget->getPartsPtr(pTarget->lockParts)->worldPos;
    hp = owner->hp;
    owner->hp = 0;
    PlWepHitCheck2(0, &p, &t, 3, 0, 6000.0f);
    owner->hp = hp;
    EstSet((int) owner->pItem, -1, 0, 0, 7, 0, 0, 0xA, 0, 0);
    SndCall(8, 0, &owner->pParts->worldPos, owner->id, 0, 0);
}

// Plays the motion key sound (seNo) at its parts.
void cSubLuis::seqSeCtrl()
{
    u8 k = seNo;
    u32 se;
    int parts;
    u16 blk;

    if (k == 0) return;
    se = k - 1;
    switch (se) {
    case 0:
    case 2:
        parts = 0x14;
        blk = 5;
        se += 7;
        break;
    case 1:
    case 3:
        parts = 0x18;
        blk = 5;
        se += 7;
        break;
    case 4:
    case 5:
        parts = 0;
        blk = 5;
        se += 7;
        break;
    default:
        parts = 0;
        blk = 8;
        break;
    }
    SndCall(blk, (u16) se, &getPartsPtr(parts)->worldPos, id, 0, 0);
    seNo = 0;
}

int cSubLuis::damageCheck()
{
    int dead;

    if ((stat & 0xFFFF0000) == 0x04000000) {
        action.set(6);
        return 1;
    }
    dead = (dmg.flags & 0xFFFF0000) != 0;
    if (!dead && (s16) pG->pl_life > 0 && DmgMgr.hitCheck(&getPartsPtr(0)->worldPos, 0) == 1) {
        routine.x110 = 3;
        dmHit = dead;
        dmType = 0x80;
        flags |= 1;
        return 1;
    }
    if (dmHit == 0) return 0;

    analysis.flags &= ~0x40;
    routine.x114 = 0;
    switch (dmWep) {
    default:
        dmgCnt--;
        if (dmgCnt == 0) {
            flags |= 2;
        } else {
            if (dmgCnt == 1) setStatus(0xA);
            analysis.flags |= 0x40;
            routine.x114 = dmgCnt;
        }
        routine.pTarget = pPL;
        dmType = 1;
        if (Front_check(this, &x328, PI / 2)) routine.x110 = 2;
        else routine.x110 = 3;
        SndCall(8, 0x13, &subSelf->pParts->worldPos, subSelf->id, 0, 0);
        break;
    case 0x13:
        dmType = 1;
        routine.x110 = 8;
        break;
    case 0x17:
        dmHit = 0;
        return 0;
    case 0x18:
        dmType = 1;
        routine.x110 = 2;
        break;
    }
    flags |= 1;
    dmHit = 0;
    dmType = 0x80;
    return 1;
}

void cSubLuis::equipWeapon()
{
    pItem = (cObjLuisItem*) ObjMgr.createBack(0xB);
    if (pItem == 0) {
        pLog->err(0, 0, "Luis.equipWeapon() CREATE FAILED");
    } else {
        static const Vec p1 = { 500.0f, 0.0f, 0.0f };
        pItem->modelInit(SUBARC(0x38 / 4), SUBARC(0x3C / 4));
        pItem->atari.flags &= 0xFCFF;
        pItem->pParts->pParent = getPartsPtr(10);
        pItem->lightInfo.init2(1, 1, LuisLightZero(), &p1, 1);
        pItem->wep.parent = this;
    }
}

void cSubLuis::endDamage()
{
    if ((flags & 0x40) && dmgType && ((cEm*) dmgType)->dmType) cnt = 30;
    flags &= ~0x40;
    action.set(0);
    routine.end();
}

// The eye rates are handled through inlines taking the object pointer (cMot3Rate methods in the original):
// each inlined call copies `&luisEye` into its own pseudo, so r[1]/r[2] go through `4(rP)`/`8(rP)` while
// cse rewrites the offset-0 `r[0]` access to the `luisEye@l(rHigh)` form inside the same extended block
// and leaves the pointer form after a join label (EyeLimit's snap store `stfs f0, 0(r10)`); the tail's
// EyeGet/EyeMove then get a fresh high/pointer pair after the getPartsPtr call instead of reusing the
// clamp's. The clamp bounds are inline arguments: both constants are loaded before the first compare.
static inline void EyeSet(cMot3Rate* e, f32 v) { e->r[1] = v; if (e->r[2] == 0.0f) e->r[0] = e->r[1]; }
static inline void EyeLimit(cMot3Rate* e, f32 lo, f32 hi)
{
    if (e->r[1] < lo) e->r[1] = lo;
    else if (e->r[1] > hi) e->r[1] = hi;
    if (e->r[2] == 0.0f) e->r[0] = e->r[1];
}
static inline f32 EyeGet(cMot3Rate* e) { return e->r[0]; }
static inline void EyeMove(cMot3Rate* e) { e->r[0] = e->r[0] * e->r[2] + e->r[1] * (1.0f - e->r[2]); }

void cSubLuis::moveEye()
{
    static int luisEyeTimer;   // eyelid animation frame; a function-local static so it precedes the ctor'd luisEye in .bss
    cModel* p = getPartsPtr(0x1C);
    // `u8 r` is block-scoped in both Rnd blocks: one function-scope `r` is a two-set global pseudo (r0)
    // where the target ties the masked remainder to the Rnd result (`clrlwi r3, r3, 24`).

    switch (luisEyeTimer++) {
    default: p->rot.x = 0.0f; break;
    case 0: {
        u8 r = Rnd() % 200;
        EyeSet(&luisEye, (r * 0.01f - 1.0f) * PI * 0.1f);
        p->rot.x = 0.17453292f;
        break;
    }
    case 1: p->rot.x = 0.34906584f; break;
    case 2: p->rot.x = 0.6981317f; break;
    case 3: p->rot.x = 0.62831855f; break;
    case 4: p->rot.x = 0.4886922f; break;
    case 5: p->rot.x = 0.34906584f; break;
    case 6: p->rot.x = 0.17453292f; break;
    case 0x1E:
        EyeSet(&luisEye, 0.0f);
        break;
    case 0x58:
        luisEyeTimer = (Rnd() & 3) ? 0 : 0x5A;
        break;
    case 0x5A: p->rot.x = 0.17453292f; break;
    case 0x5B: p->rot.x = 0.34906584f; break;
    case 0x5C: p->rot.x = 0.6981317f; break;
    case 0x5D: p->rot.x = 0.5934119f; break;
    case 0x5E: p->rot.x = 0.6632251f; break;
    case 0x5F: p->rot.x = 0.6981317f; break;
    case 0x60: p->rot.x = 0.5235988f; break;
    case 0x61: p->rot.x = 0.34906584f; break;
    case 0x62:
        p->rot.x = 0.17453292f;
        luisEyeTimer = 10;
        break;
    }
    p->matUpdate();

    if (--luisBlink < 0) {
        u8 r = Rnd() % 200;
        EyeSet(&luisEye, (r * 0.01f - 1.0f) * 0.03141593f + luisEye.r[1]);
        luisBlink = (u8) (Rnd() % 3) + 2;
    }
    EyeLimit(&luisEye, -0.31415927f, 0.31415927f);

    getPartsPtr(0x20)->rot.y = EyeGet(&luisEye);
    getPartsPtr(0x20)->cCoord::matUpdate();
    getPartsPtr(0x21)->rot.y = EyeGet(&luisEye);
    getPartsPtr(0x21)->cCoord::matUpdate();
    EyeMove(&luisEye);
}

void cSubLuis::neckSet(f32 ang, f32 limit)
{
    neckAng += Muku2(neckAng, ang, limit);
    flags |= 8;
}

void cSubLuis::neckMove()
{
    cModel* p;
    const f32 spd = 0.62831855f;   // pool order: the turn speed precedes the 0.0

    if (flags & 8) {
        flags &= ~8;
    } else {
        neckAng += Muku2(neckAng, 0.0f, spd);
    }
    p = subSelf->getPartsPtr(3);
    ((cParts*) p)->motParts.flags |= 0x40000000;
    ((cParts*) p)->addRot.y = neckAng;
}

cVoice::cVoice()
{
    on = 0;
    timer = 0;
    sndId = 0xF0F0F0F0;
}

void cVoice::set(int mesNo, u16 seNo, int time)
{
    int i;

    if (sndId != 0xF0F0F0F0) SndStop(sndId, 0);
    if (timer <= 1) {
        MessageControl* mes = &cMes;
        timer = 0;
        on = 0;
        for (i = 0; i < 16; i++) mes->Delete(i);
    }
    if ((s16) pG->pl_life > 0) {
        sndId = SndCall(8, seNo, &pSUB->pParts->worldPos, pSUB->id, 0, 0);
        cMes.MesSet(mesNo, 100, 336 - cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1, 0x1000051, 0, 0, 4);   // fold swaps the two subtrahends
    }
    timer = time;
    on = 1;
}

void cVoice::move()
{
    int i;

    if (on == 1) {
        if (timer-- < 0) {
            MessageControl* mes = &cMes;
            on = 0;
            for (i = 0; i < 16; i++) mes->Delete(i);
        }
    }
}

// On the stairs of room 11C.
int stairCheck(cModel* m)
{
    if ((pG->room_id32 & 0xFFFF0000) != 0x011C0000) return 0;
    if (m->pos.x > 109070.0f && m->pos.x < 114620.0f && m->pos.z > -47600.0f && m->pos.z < -45930.0f) return 1;
    return 0;
}

int sameFloorCheck(cModel* a, cModel* b)
{
    const f32 lim = 1000.0f;   // the pool `lis` is expanded here, above the fabsf barrier

    return fabsf(a->pos.y - b->pos.y) < lim;
}

void luisItemInit(cObj* obj)
{
    new (obj) cObjLuisItem();
}

void cObjLuisItem::init(Vec* p, f32 rotY)
{
    modelInit((void*) (pG->pArc->ofs_20 + (u32) pG->pArc), (void*) (pG->pArc->ofs_24 + (u32) pG->pArc));
    setPos(p);
    rot.y = rotY;
    rot.x = 0.0f;
    rot.z = 0.0f;
    EstSet((int) this, -1, 0, 0, 0, 0x2D, 0, 0x3C, (u32) this, 0);
    PSVECSubtract(&pPL->pos, &pos, &LITEM->spd);
    PSVECScale(&LITEM->spd, &LITEM->spd, 0.07f);
    LITEM->acc.x = 0.0f;
    LITEM->acc.y = -fabsf(LITEM->spd.y) * 0.03f;
    LITEM->acc.z = 0.0f;
    LITEM->timer = 0;
}

void cObjLuisItem::move()
{
    Vec hit;
    Vec nrm;
    int b;
    u8 r;
    u16 item;

    switch (xFC) {
    case 0:
        PSVECAdd(&pos, &LITEM->spd, &pos);
        PSVECAdd(&LITEM->spd, &LITEM->acc, &LITEM->spd);
        if (SatMgr.hitCheck(&oldPos, &pos, &hit, &nrm, 0, 0)) {
            if (nrm.y < 0.5f && nrm.y > -0.5f) {
                PSVECScale(&nrm, &nrm, 200.0f);
                PSVECAdd(&hit, &nrm, &pos);
            }
            pos.y = SatMgr.getFloor(&pos, 600.0f, 100000.0f, 0, 0);
            xFC = 1;
        }
        LITEM->timer++;
        if (LITEM->timer > 150) {
            EffectEspDelete(0, 0x3C, (u32) this, 0);
            EffectEspgenDelete(0, 0x3C, (int) this);
            EffectEfmDelete(0, 0x3C, (int) this);
            ObjMgr.destroy(this);
        }
        break;
    case 1:
        b = (u32) GetBulletPoint() < (u32) GetRecoveryPoint();
        r = Rnd() % 100;
        switch (b) {
        case 0:
            if (r > 0x4A) {
                if (r <= 0x4F) item = 1;
                else if (r <= 0x59) item = 2;
                else item = 0xE;
            } else {
                item = 4;
            }
            break;
        case 1:
            item = r > 0x14 ? 6 : 5;
            break;
        default:
            item = 4;
            break;
        }
        EffectEspDelete(0, 0x3C, (u32) this, 0);
        EffectEspgenDelete(0, 0x3C, (int) this);
        EffectEfmDelete(0, 0x3C, (int) this);
        SceAtCreateItemAt(&pos, item, 0, -1, -1, 0, -1);
        ObjMgr.destroy(this);
        break;
    }
    matUpdate();
}

// The grenade the player holds near his height: the throw routine to answer with, 0 = none.
int greThrowCheck()
{
    cObj* o;

    for (o = ObjMgr.pAlive; o; o = (cObj*) o->next) {
        if (fabsf(o->pos.y - (pSUB->pos.y + 2000.0f)) < 2500.0f) {
            switch (o->id) {
            case 0x1A: return 0x13;
            case 0x29: return 0x16;
            case 0x2A: return 0x17;
            case 0x22: return 0xD;
            }
        }
    }
    return 0;
}

int isTarget(cSubLuis* luis, cEm* em)
{
    WepTarget list[2];
    Vec hit;
    Vec nrm;
    u32 attr;

    if (!VALID_PTR(em)) {
        pLog->err(0, 0, "LUIS isTarget() INVALIED PTR 0x%08x", em);
        return 0;
    }
    if (!VALID_PTR(em) || (em->be_flag & 0x201) != 1 || em->hp <= 0 || em->id <= 0xF || em->checkStatus(1) ||
        EatMgr.hitCheck(&luis->pParts->worldPos, &em->pParts->worldPos, 0, 0, 0, 0x400000) ||
        doorHitCheck(&luis->pParts->worldPos, &em->pParts->worldPos)) {
        return 0;
    }
    if (GetWepTargetList2(&luis->pParts->worldPos, &em->pParts->worldPos, list, 2, &hit, &nrm, &attr, 2, 0) > 1 &&
        list[1].em != em) {
        return 0;
    }
    return 1;
}

extern "C" void _prolog()
{
    EmInitFunc = LuisInit;
    ObjInitFunc[0x1E] = luisItemInit;
    OSReport("LUIS prolog Ok\n");
}

extern "C" void _epilog()
{
    ObjInitFunc[0x1E] = 0;
    OSReport("LUIS epilog Ok\n");
}

extern "C" void _unresolved()
{
}
