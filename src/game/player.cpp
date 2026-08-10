// game/player.cpp: player manager: PlayerInit / life reset, cPlayer construction (init0 / init1 /
// startUp), the per-frame cPlayer::move and the routine 0 / routine 1 (movement) functions.

#include "atari.h"
#include "atari_init.h"
#include "light.h"
#include "player.h"
#include "pl_push.h"
#include "pl_npc.h"
#include "global.h"
#include "db_log.h"
#include "main.h"
#include "joy.h"
#include "snd.h"
#include "motion.h"
#include "math_sub.h"
#include "cMotBase.h"
#include "at_mod.h"
#include "em_sub.h"
#include "esp.h"
#include "est.h"
#include "pl_sub.h"
#include "cam_ctrl.h"
#include "main_mem.h"

extern "C" {
double atan2(double y, double x);
void ReleaseWepData();                        // game/read.cpp
void ShapeMove(void* info);                   // game/shape.cpp
int fanceWidthCheck(cPlayer* pl);
void fanceAdjust(cPlayer* pl);
int fallCheck(cPlayer* pl);
f32 getHeighAdjust(cPlayer* pl);
void PlayerInit();
void PlayerLifeReset();
void pl_R0_Move(cPlayer* pl);
void pl_R1_Footwork(cPlayer* pl);
void pl_R1_Walk(cPlayer* pl);
void pl_R1_Back(cPlayer* pl);
void pl_R1_Run(cPlayer* pl);
void pl_R1_Turn(cPlayer* pl);
void pl_R1_Turn180(cPlayer* pl);
void pl_R1_Weapon(cPlayer* pl);
void pl_R1_Boat(cPlayer* pl);
void pl_R1_Ladder(cPlayer* pl);
void pl_R1_Crouch(cPlayer* pl);
void pl_R1_JumpFall(cPlayer* pl);
void pl_R1_Whistle(cPlayer* pl);
void pl_R1_Aux(cPlayer* pl);
void pl_R1_LevelUp(cPlayer* pl);
void pl_R1_LevelDown(cPlayer* pl);
void pl_R1_ObjPush(cPlayer* pl);
void pl_R1_Fance(cPlayer* pl);
void pl_R1_Fall(cPlayer* pl);
void pl_R0_Dijection(cPlayer* pl);
void pl_R1_BoatDrive(cPlayer* pl);
}
void Pl_R0_Damage(cPlayer* pl);   // game/pl_dmg.cpp
void Pl_R0_Die(cPlayer* pl);
// cSubChar::registPlAction(Vec*, f32) (pl_npc.h) called with an extra `li r5, 0` the original passes.
void SubCharRegistPlAction(cSubChar* sub, Vec* pos, int a, f32 ang) asm("registPlAction__8cSubCharP3Vecf");

#line 41 "D:/Bio4/Prog/player.cpp"
#define PL_MEM_ALLOC(size, line) mem_alloc(size, __FILE__, line, 1, 13)

static inline void U8Set(u8& d, u8 v) { d = v; }
static inline void U16Set(u16& d, u16 v) { d = v; }
// Life / life max pair: both addresses are taken before the stores (one pG load per pair).
static inline void U16Set2(u16& a, u16& b, u16 v)
{
    b = v;
    a = v;
}
static inline void PSet(void*& d, void* v) { d = v; }

// Routine bytes written through an inline taking ints (pl_class.cpp): the stores come out in the
// original's order and the zero stays an SImode constant. Routine changes that also set the hokan
// (x4FD = 5) or a frame are direct byte stores in the original (see pl_R1_Walk / pl_R1_Run).
static inline void PlRoutineSet(cPlayer* pl, int r0, int r1, int r2, int r3)
{
    pl->xFC = r0;
    pl->xFD = r1;
    pl->xFE = r2;
    pl->xFF = r3;
}

// 1 when the push target is gone or dead.
static inline int pushTargetDead(cPlPush* p)
{
    if (p->pTarget == 0 || p->pTarget->hp <= 0) {
        return 1;
    }
    return 0;
}

#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)

// Parts index mirror table (left <-> right parts of the player model).
u16 pl00_mirror[80] = {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F, 0x0010, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000A, 0x0011, 0x0016, 0x0017, 0x0018, 0x0019, 0x0012, 0x0013, 0x0014, 0x0015, 0x001B, 0x001A,
    0x001C, 0x001D, 0x001E, 0x001F, 0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029,
    0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045,
    0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
};

// Routine 0 table (cModel::xFC): move, damage, die, scenario, -, event, dijection.
void (*Pl_func_tbl[7])(cPlayer*) = {
    pl_R0_Move, Pl_R0_Damage, Pl_R0_Die, (void (*)(cPlayer*)) EmScenario, 0, Pl_R0_Event, pl_R0_Dijection,
};

// Routine 1 table (cModel::xFD) of routine 0.
void (*Pl_func_move_tbl[21])(cPlayer*) = {
    pl_R1_Footwork, pl_R1_Walk, pl_R1_Back, pl_R1_Run, pl_R1_Turn, pl_R1_Turn180, pl_R1_Weapon, pl_R1_LevelUp,
    pl_R1_LevelDown, pl_R1_ObjPush, pl_R1_Aux, PlKnifeMove, pl_R1_Fance, 0, pl_R1_Fall, pl_R1_Boat, pl_R1_Ladder,
    pl_R1_Crouch, 0, pl_R1_JumpFall, pl_R1_Whistle,
};

void* PlWepMot[3];
Vec PlFancePos;

u8 PlMode = 0;
u8 PlFormMode = 0;
u8 PlDbFlag = 0;
u8 lbl_803140DB = 0;
void (*WeaponMoveFunc)(cPlayer*) = 0;
void (*BoatMoveFunc)(cPlayer*) = 0;

cPlMaho* pMaho;
u8 PlKaiou;
int PlFanceFlag;

// Equipped weapon / life by the player character.
void PlayerInit()
{
    switch (pG->x4FB8) {
    case 0:
    case 1:
        U8Set(pG->wep_no, 2);
        U8Set(pG->wep_type, 0);
        U8Set(pG->wep_lv, 0);
        U8Set(pG->wep_lv_mag, 0);
        U8Set(pG->wep_lv_ex, 0);
        U8Set(pG->x4FBA, 0);
        break;
    case 2:
        U8Set(pG->wep_no, 1);
        U8Set(pG->wep_lv, 0);
        U8Set(pG->wep_lv_mag, 0);
        U8Set(pG->wep_lv_ex, 0);
        U8Set(pG->x4FBA, 0);
        U8Set(pG->wep_type, 0);
        break;
    case 4:
        U8Set(pG->wep_no, 0x1C);
        U8Set(pG->wep_lv, 0);
        U8Set(pG->wep_lv_mag, 0);
        U8Set(pG->wep_lv_ex, 0);
        U8Set(pG->x4FBA, 0);
        U8Set(pG->wep_type, 0);
        break;
    case 3:
        U8Set(pG->wep_no, 0xB);
        U8Set(pG->wep_lv, 0);
        U8Set(pG->wep_lv_mag, 0);
        U8Set(pG->wep_lv_ex, 0);
        U8Set(pG->x4FBA, 0);
        U8Set(pG->wep_type, 0);
        break;
    case 5:
        U8Set(pG->wep_no, 2);
        U8Set(pG->wep_type, 1);
        U8Set(pG->wep_lv, 0);
        U8Set(pG->wep_lv_mag, 0);
        U8Set(pG->wep_lv_ex, 0);
        U8Set(pG->x4FBA, 0);
        break;
    }
    PlayerLifeReset();
    U16Set(pG->pl_life_max, pG->pl_life);
    BitOff(pG->flags_68, 0x00040000);
    ReleaseWepData();
    U16Set(pG->flags_4FBE, 1);
    PlKaiou = 0;
}

// Life by the player character (Leon 1200 / 1860 with flags_54 bit30, Ashley 600, Ada 1560,
// HUNK 1680, Krauser 2400, Wesker 1860); flags_6C bits: 1440 / 1920.
void PlayerLifeReset()
{
    switch (pG->x4FB8) {
    case 0:
        U16Set2(pG->pl_life, pG->pl_life_max, 1200);
        if (pG->flags_54 & 0x40000000) {
            U16Set2(pG->pl_life, pG->pl_life_max, 1860);
        }
        break;
    case 1:
        U16Set2(pG->pl_life, pG->pl_life_max, 600);
        break;
    case 2:
        U16Set2(pG->pl_life, pG->pl_life_max, 1560);
        break;
    case 3:
        U16Set2(pG->pl_life, pG->pl_life_max, 1680);
        break;
    case 4:
        U16Set2(pG->pl_life, pG->pl_life_max, 2400);
        break;
    case 5:
        U16Set2(pG->pl_life, pG->pl_life_max, 1860);
        break;
    default:
        U16Set2(pG->pl_life, pG->pl_life_max, 1200);
        break;
    }
    U16Set2(pG->sub_life, pG->sub_life_max, 600);
    if (pG->flags_6C & 0x00800000) {
        U16Set2(pG->pl_life, pG->pl_life_max, 1440);
    }
    if (pG->flags_6C & 0x00040000) {
        U16Set2(pG->pl_life, pG->pl_life_max, 1920);
    }
}

cPlayer::cPlayer()
{
    flags_420 = 0;
    pPL = this;
    hp = pGS->pl_life;
    x378 = 0x807EC000;
    pMotTbl = (void**) PL_MEM_ALLOC(0x1B4, 373);
    memclr_asm(pMotTbl, 0x1B4);
    pRegistMot = (void**) PL_MEM_ALLOC(0x30, 376);
    memclr_asm(pRegistMot, 0x30);
    flags_420 |= 1;
    debugInit();
}

// Sub objects: weapon, body, push, waist, motion base.
void cPlayer::init0()
{
    cPlPush* push;

    pWep = new cPlWep;
    pBody = new cPlBody(this);
    push = new cPlPush;
    push->x8 = 0;
    push->pTarget = 0;
    push->pPl = this;
    pPush = push;
    pWaist = new cPlWaist;
    pMotBase = new cMotBase;
}

// Neck, light set, collision, hit boxes, routine 0, mirror table, first world calc.
void cPlayer::init1()
{
    if (!VALID_PTR(pParts)) {
        pLog->err(0, 0, "cPlayer::cPlayer() FAILED");
        EmMgr.destroy(this);
        return;
    }
    pNeck = new cPlNeck(this);
    be_flag |= 0x07000000;
    x12F = 7;
    {
        static const Vec lightOfs = { 0.0f, 0.0f, 0.0f };
        static const Vec lightSize = { 1000.0f, 1000.0f, 0.0f };
        lightInfo.init2(0, 1, &lightOfs, &lightSize, 1);
    }
    litArea.on(1);
    atariInitF(&atari, 0.0f, -200.0f, 0.0f, 400.0f, 200.0f, 400.0f, 800.0f, 1, 0x1000, 10);
    lockOfs.x = 0.0f;
    lockOfs.y = 0.0f;
    lockOfs.z = 0.0f;
    lockParts = 2;
    YarareInit(this, 0.0f, -30.0f, 0.0f, 200.0f, 100.0f, 2, 1);
    // TODO: the four extra hit boxes live at cEm+0x530/0x564/0x598/0x5CC (the cSubChar fields of em.h
    // overlay them); give them names in em.h.
    YarareAdd(this, (EmHitInfo*) ((u8*) this + 0x530), 0.0f, 0.0f, 0.0f, 210.0f, 130.0f, 3, 1);
    YarareAdd(this, (EmHitInfo*) ((u8*) this + 0x564), 0.0f, 0.0f, 0.0f, 120.0f, 80.0f, 5, 1);
    YarareAdd(this, (EmHitInfo*) ((u8*) this + 0x598), -20.0f, -300.0f, 0.0f, 170.0f, 300.0f, 0x13, 1);
    YarareAdd(this, (EmHitInfo*) ((u8*) this + 0x5CC), 20.0f, -300.0f, 0.0f, 170.0f, 300.0f, 0x17, 1);
    MOTION(this)->flip = pl00_mirror;
    x4FE = 0;
    alpha = 1.0f;
    flags_41C = 0;
    Pl_func_tbl[0] = pl_R0_Move;
    xButtonWait = 0;
    pRoomEff = 0;
    flags_420 |= 0x800;
    sndId504 = 0;
    eyeMode = 0;
    boss0 = 0;
    satCheckFlag = 0;
    partsWorldCalc();
    initCloth();
    if (pG->x4FB8 == 0) {
        pBody->makeSpaeData();
    }
    p2A4 = (EmWork2A4*) PL_MEM_ALLOC(0x98, 510);
}

// Place the player at the room start position and run the first frames of its motion.
void cPlayer::startUp()
{
    BitOn(be_flag, 0x00200000);
    FSet(rot.y, pG->sub_angle);
    setPos(&pG->sub_pos);
    matUpdate();
    x4FC = 0;
    x4FD = 0;
    PlRoutineSet(this, 0, 0, 0, 1);
    move();
    motionMove();
    motionMove();
    SatMgr.check(this, satCheckFlag);
    matUpdate();
}

// Per-frame update.
void cPlayer::move()
{
    Vec savePos;
    f32 water;
    int moved;
    int i;

    BitOff(pG->flags_5010, 4);
    BitOff(pG->flags_500C, 0x00800000);
    BitOff(pG->flags_5010, 0x40000000);
    BitOff(pG->flags_5010, 0x00800000);
    BitOff(pG->flags_5010, 0x00400000);
    BitOff(pG->flags_5010, 0x00200000);
    BitOff(pG->flags_5010, 0x00080000);
    BitOff(pG->flags_5010, 0x00008000);
    BitOff(pG->flags_5010, 0x00002000);
    BitOff(pG->flags_5014, 0x40000000);
    BitOff(pG->flags_5014, 0x01000000);
    BitOff(pG->flags_5010, 0x00040000);
    clearStatus(3);
    BitOff(pG->flags_5014, 0x80000000);
    if (pWep->pObj) {
        pWep->pObj->wep.target = 0;
    }
    pMotBase->adjust();
    dmg.move();
    keyConfig();
    dmgCheck();
    if (pBody->pShape) {
        ShapeMove(pBody->pShape);
    }
    if ((int) pos.x == (int) oldPos.x && (int) pos.y == (int) oldPos.y && (int) pos.z == (int) oldPos.z
        && (stat & 0xFFFFFF00) == 0x100 && !(pG->flags_500C & 0x20)) {
        moved = 0;
        if ((int) pG->flags_60 >= 0) {
            goto moveChecked;
        }
    }
    moved = 1;
moveChecked:
    if (Key.trg & 0x10) {
        flags_420 &= ~0x1000;
    }
    if (Key.trg & 0x800) {
        if (flags_420 & 0x1000) {
            flags_420 &= ~0x1000;
        } else {
            flags_420 |= 0x1000;
        }
    }
    moveBinocular();
    subCharLiveCheck();
    Pl_func_tbl[xFC](this);
    if (pG->flags_68 & 0x00010000) {
        // the u8 compared as an int (`cmpwi -1`): the original's test, always true
        if (PlKaiou != -1) {
            for (i = 0; i < PlKaiou + 1; i++) {
                Pl_func_tbl[xFC](this);
            }
        }
    }
    if ((stat & 0xFFFFFF00) == 0x00060000) {
        pParts->rot.y *= 0.5f;
    }
    rot.y = LIMIT_ANGLE(rot.y);
    flags_41C = (u16) flags_41C;
    shadowCtrl();
    pMotBase->move();
    pNeck->move();
    moveEye();
    pBody->waistSet(pWaist->cur);
    pBody->move();
    moveMatCalcBefore();
    partsWorldCalc();
    partsFixAdjust();
    PartsWorldPosCalc(this);
    if (!(pG->flags_68 & 8)) {
        savePos = pos;
        EmAtCheck(this);
        SatMgr.check(this, satCheckFlag);
        if ((int) savePos.x != (int) pos.x || (int) savePos.y != (int) pos.y || (int) savePos.z != (int) pos.z) {
            moved = 1;
        }
    }
    atari.move();
    PartsWorldPosCalc(this);
    pWep->move();
    moveCloth();
    seqSeCtrl();
    updateOldPos();
    visibleCtrl();
    if (moved) {
        CamCtrl.qfps.setPlayerLocation(mat, pFloorNrm);
    }
    if (GetWaterHeight(&pos, &water) && water > pos.y) {
        PlWaterProc(this);
    }
    be_flag &= ~0x20000000;
    debugMove();
}

// Routine 0/0: the movement sub routines (Pl_func_move_tbl by xFD).
void pl_R0_Move(cPlayer* pl)
{
    if (pl->dmgFlag520 == 0) {
        pl->dmgCnt522 = 0;
    }
    pl->dmgFlag520 = 0;
    Pl_func_move_tbl[pl->xFD](pl);
}

// Idle: footwork motion (pMotTbl[0] / the damaged pair 0x5F), neck motions 0x3F/0x40.
void pl_R1_Footwork(cPlayer* pl)
{
    switch (pl->xFE) {
    case 0: {
        int hokan;
        int frame;
        if (pl->xFF & 1) {
            hokan = pl->x4FD;
            frame = pl->x4FC;
        } else {
            hokan = 8;
            frame = 0;
        }
        pl->motionSet(pl->pMotTbl[0], pl->pMotTbl[1], pl->pMotTbl[0x5F], pl->pMotTbl[0x60], hokan, frame);
        if (dmMotCk()) {
            pl->pNeck->init(pl->pMotTbl[0x3F], pl->pMotTbl[0x40], 0);
        } else {
            pl->pNeck->init(PL_ARC_PTR(pG->pPlArc, 0x40), PL_ARC_PTR(pG->pPlArc, 0x41), 0);
        }
        pl->xFE = 1;
    }
    case 1:
        pl->motionMove();
        break;
    case 2:
        if (pl->motionMove()) {
            pl->xFE = 0;
        }
        break;
    }
    if (pl->actionSelect() == 0) {
        if ((Key.on & 1) && (Key.on & 0x40000000) && joyKamae() == 0) {
            PlRoutineSet(pl, 0, 3, 0, 0);
        } else {
            pl->checkCtrl();
        }
    }
}

// Walk (pMotTbl[2] / damaged 0x61), turning with SPEED_WALK_TURN; run (routine 3) on the run key.
void pl_R1_Walk(cPlayer* pl)
{
    if (pl->xFE == 0) {
        void** tbl;
        u32 frame;
        int hokan;
        if (pl->xFF & 4) {
            MotionData* data;
            if (dmMotCk()) {
                data = (MotionData*) pl->pMotTbl[6];
                tbl = pl->pMotTbl;
            } else {
                data = (MotionData*) PL_ARC_PTR(pG->pPlArc, 0x38);
                tbl = pl->pMotTbl;
            }
            frame = (u32) ((f32) (data->maxFrame & 0x3FFF) * (f32) pl->x4FC * 0.00390625f);
            hokan = pl->x4FD;
        } else {
            tbl = pl->pMotTbl;
            frame = 0;
            hokan = 8;
        }
        pl->motionSet(tbl[2], tbl[3], tbl[0x61], tbl[0x62], hokan, (u16) frame);
        if (dmMotCk()) {
            pl->pNeck->init(pl->pMotTbl[0x39], pl->pMotTbl[0x3A], (u16) frame);
        } else {
            pl->pNeck->init(PL_ARC_PTR(pG->pPlArc, 0x42), PL_ARC_PTR(pG->pPlArc, 0x43), (u16) frame);
        }
        pl->xFE = 1;
    }
    pl->motionMove();
    if (pl->actionSelect() == 0) {
        if ((Key.on & 0x40000000) && joyKamae() == 0) {
            pl->xFC = 0;
            pl->xFD = 3;
            pl->xFE = 2;
            pl->xFF = 4;
            pl->x4FD = 5;
            pl->x4FC = (u8) (pl->frame * 255.0f / (f32) pl->frameMax);
        } else if (!(Key.on & 1)) {
            PlRoutineSet(pl, 0, 0, 0, 0);
        } else {
            if (Key.on & 4) {
                pl->rot.y -= cPlayer::SPEED_WALK_TURN;
            }
            if (Key.on & 8) {
                pl->rot.y += cPlayer::SPEED_WALK_TURN;
            }
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
            pl->checkCtrl();
        }
    }
}

// Back step (pMotTbl[8] / damaged 0x63): turning with SPEED_WALK_TURN while the back key is held.
void pl_R1_Back(cPlayer* pl)
{
    if (pl->xFE == 0) {
        void** tbl = pl->pMotTbl;
        pl->motionSet(tbl[8], tbl[9], tbl[0x63], tbl[0x64], 8, 0);
        pl->pNeck->motL = 0;
        pl->xFE = 1;
    }
    pl->motionMove();
    if (pl->actionSelect() == 0) {
        if (!(Key.on & 2)) {
            PlRoutineSet(pl, 0, 0, 0, 0);
        } else {
            if (Key.on & 4) {
                pl->rot.y -= cPlayer::SPEED_WALK_TURN;
            }
            if (Key.on & 8) {
                pl->rot.y += cPlayer::SPEED_WALK_TURN;
            }
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
            pl->checkCtrl();
        }
    }
}

// Run (pMotTbl[6] / damaged 0x65): breathing SE past 150 frames of running while the life is high,
// SPEED_RUN_TURN turning, walk on the run key release.
void pl_R1_Run(cPlayer* pl)
{
    static s8 breath_ctr;
    static f32 pl_speed2_xxx = 1.1f;

    switch (pl->xFE) {
    case 1:
        if (pl->motionMove()) {
            pl->xFE = 2;
        }
        break;
    case 0:
    case 2: {
        void** tbl;
        u32 frame;
        int hokan;
        if (pl->xFF & 4) {
            MotionData* data;
            if (dmMotCk()) {
                data = (MotionData*) pl->pMotTbl[6];
                tbl = pl->pMotTbl;
            } else {
                data = (MotionData*) PL_ARC_PTR(pG->pPlArc, 0x38);
                tbl = pl->pMotTbl;
            }
            frame = (u32) ((f32) (data->maxFrame & 0x3FFF) * (f32) pl->x4FC * 0.00390625f);
            hokan = pl->x4FD;
        } else if (pl->xFF & 2) {
            frame = pl->x4FC;
            hokan = pl->x4FD;
            tbl = pl->pMotTbl;
        } else {
            tbl = pl->pMotTbl;
            frame = 0;
            hokan = 8;
        }
        pl->motionSet(tbl[6], tbl[7], tbl[0x65], tbl[0x66], hokan, (u16) frame);
        if (dmMotCk()) {
            pl->pNeck->init(pl->pMotTbl[0x41], pl->pMotTbl[0x42], (u16) frame);
        } else {
            pl->pNeck->init(PL_ARC_PTR(pG->pPlArc, 0x44), PL_ARC_PTR(pG->pPlArc, 0x45), (u16) frame);
        }
        pl->pWaist->cur = 0.0f;
        pl->x3E0 = 0;
        breath_ctr = 0;
        pl->xFE = 3;
    }
    default:
        pl->motionMove();
        if ((s16) pG->pl_life > (s16) pG->pl_life_max * 2 / 3) {
            if ((int) pl->x3E0 < 150) {
                pl->x3E0++;
            } else {
                if (pl->frame > 4.7f && pl->frame < 5.3f) {
                    switch (breath_ctr) {
                    case 0:
                        SndCall(1, 0x25, &pl->getPartsPtr(3)->worldPos, 0, 0, 0);
                        break;
                    case 1:
                        breath_ctr = -1;
                        break;
                    }
                    breath_ctr++;
                }
            }
        }
        break;
    }
    if (pl->actionSelect() == 0) {
        if ((Key.on & 1) && !(Key.on & 0x40000000)) {
            pl->xFC = 0;
            pl->xFD = 1;
            pl->xFE = 0;
            pl->xFF = 4;
            pl->x4FD = 5;
            pl->x4FC = (u8) (pl->frame * 255.0f / (f32) pl->frameMax);
        } else {
            u64 key = Key.on;
            if (key & 4) {
                pl->rot.y -= cPlayer::SPEED_RUN_TURN * pl_speed2_xxx;
            } else if (key & 8) {
                pl->rot.y += cPlayer::SPEED_RUN_TURN * pl_speed2_xxx;
            }
            pl->rot.y = LIMIT_ANGLE(pl->rot.y);
            if (joyKamae()) {
                PlRoutineSet(pl, 0, 0, 0, 0);
            } else if (!(Key.on & 0x40000000) || !(Key.on & 1)) {
                if (Key.on & 1) {
                    pl->x4FD = 5;
                    pl->xFC = 0;
                    pl->xFD = 1;
                    pl->xFF = 4;
                    pl->xFE = 0;
                    pl->x4FC = (u8) (pl->frame * 255.0f / (f32) pl->frameMax);
                } else {
                    PlRoutineSet(pl, 0, 0, 0, 0);
                }
                return;
            }
            pl->checkCtrl();
            if (PlDbFlag & 4) {
                PlWepHitCheck2(pl, &pl->pParts->worldPos, &pl->pParts->worldPos, 0x14, 0, 1000.0f);
            }
        }
    }
}

// Turn in place (pMotTbl[0xD] left / [0xF] right, damaged 0x67 / 0x69) while the side key is held.
void pl_R1_Turn(cPlayer* pl)
{
    switch (pl->xFE) {
    case 0:
        if (Key.on & 4) {
            pl->motionSet(pl->pMotTbl[0xD], pl->pMotTbl[0xE], pl->pMotTbl[0x67], pl->pMotTbl[0x6A], 7, 0);
            pl->xFE = 1;
        } else {
            pl->motionSet(pl->pMotTbl[0xF], pl->pMotTbl[0x10], pl->pMotTbl[0x69], pl->pMotTbl[0x6A], 7, 0);
            pl->xFE = 2;
        }
        pl->pNeck->motL = 0;
        pl->motionMove();
        break;
    case 1:
        if (!(Key.on & 4)) {
            PlRoutineSet(pl, 0, 0, 0, 0);
        }
        break;
    case 2:
        if (!(Key.on & 8)) {
            PlRoutineSet(pl, 0, 0, 0, 0);
        }
        break;
    }
    pl->motionMove();
    if (pl->actionSelect() == 0) {
        pl->checkCtrl();
    }
}

// Quick 180 degree turn (pMotTbl[0xB] / damaged 0x6B): the shoulder camera aims at the point
// 1000 behind the player; ends at a frame by character, earlier on a key.
void pl_R1_Turn180(cPlayer* pl)
{
    static Vec dd0;
    int end;

    switch (pl->xFE) {
    case 0: {
        Vec v;
        void** tbl = pl->pMotTbl;
        pl->motionSet(tbl[0xB], tbl[0xC], tbl[0x6B], tbl[0x6C], 5, 0);
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 1000.0f;
        PSMTXMultVec(pl->mat, &v, &dd0);
        CamCtrlShoulderSetSearchFrame(30);
        pl->pNeck->motL = 0;
        pl->xFE = 1;
    }
    case 1: {
        u32 frame;
        if (pl->frame <= 5.0f) {
            CamCtrlShoulderSetAim(&dd0);
        } else if (pl->frame > 5.7f && pl->frame < 6.3f) {
            CamCtrlShoulderSetSearchFrame(0);
        }
        end = pl->motionMove();
        switch (pG->x4FB8) {
        case 0:
            frame = dmMotCk() ? 0x10 : 0x13;
            break;
        case 5:
        default:
            frame = dmMotCk() ? 0x10 : 0x13;
            break;
        case 1:
            frame = dmMotCk() ? 0x15 : 0x17;
            break;
        case 2:
            frame = dmMotCk() ? 0xE : 0x11;
            break;
        case 4:
            frame = dmMotCk() ? 0xE : 0x13;
            break;
        }
        if (pl->frame >= (f32) frame) {
            if ((Key.on & 0x10F) || joyKamae()) {
                end |= 1;
            }
        }
        if (end) {
            pl->x4FE = 0;
            PlRoutineSet(pl, 0, 0, 0, 0);
            CamCtrlShoulderSetSearchFrame(0);
            return;
        }
        break;
    }
    }
    pl->checkXbutton();
}

// Defined after pl_R1_Turn180: both have constructors and are emitted at their definition, behind
// Turn180's static `dd0` in .bss.
cMot3 mot3;
cMot3Rate m3rObj;

// Weapon routine: WeaponMoveFunc (pl_wep.cpp registers it); Ashley has none.
void pl_R1_Weapon(cPlayer* pl)
{
    if (pG->x4FB8 == 1) {
        PlRoutineSet(pl, 0, 0, 0, 0);
    } else if (WeaponMoveFunc == 0) {
        pLog->err(0, 0, "pl_R1_Weapon(): Function is no regist!");
    } else {
        WeaponMoveFunc(pl);
        pl->checkXbutton();
    }
}

// Boat routine: BoatMoveFunc (the boat object registers it).
void pl_R1_Boat(cPlayer* pl)
{
    static void (*pl_move_func_tbl[21])(cPlayer*) = {
        pl_R1_BoatDrive, 0, 0, 0, 0, 0, pl_R1_Weapon, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };

    if (BoatMoveFunc == 0) {
        pLog->err(0, 0, "pl_R1_Boat(): Function is no regist!");
    } else {
        BoatMoveFunc(pl);
    }
}

// Dead-stripped in the original (STRIP_UNUSED): only its constant pool (2000.0, 300.0) survives
// between pl_R1_Boat's message and pl_R1_Ladder's pool.
static void plLadderPosSet(cPlayer* pl)
{
    pl->x400 = pl->pos.y + 2000.0f;
    pl->pos.y += 300.0f;
}

// Ladder: steps 0..3 climb up (x3E0 = middle sections left), 0xA..0xD climb down; x400 tracks the
// height reached, the foot SEs play at fixed frames.
void pl_R1_Ladder(cPlayer* pl)
{
    BitOn(pG->flags_5010, 0x00040000);
    switch (pl->xFE) {
    case 0:
        FSet(pl->x400, pl->pos.y);
        pl->motionSet(PL_ARC_PTR(pG->pPlArc, 0x27), 5, 0, 1, 0);
        pl->pNeck->motL = 0;
        pl->xFE = 1;
        pl->flags_420 &= ~0x800;
    case 1:
        if (pl->frame > 11.7f && pl->frame < 12.3f) {
            SndCall(6, 0x61, &pl->pos, 0, 0, 0);
            SndCall(1, 0x2A, &pl->pos, 0, 0, 0);
        }
        if (pl->motionMove()) {
            pl->x400 += 500.0f;
            if (pl->x3E0 != 0) {
                pl->motionSet(PL_ARC_PTR(pG->pPlArc, 0x28), 3, 0, 5, 0);
                pl->xFE = 2;
            } else {
                pl->motionSet(PL_ARC_PTR(pG->pPlArc, 0x29), 5, 0, 1, 0);
                pl->xFE = 3;
            }
        }
        break;
    case 2:
        if (pl->frame > 0.7f && pl->frame < 1.3f) {
            SndCall(6, 0x60, &pl->pos, 0, 0, 0);
            SndCall(1, 0x2A, &pl->pos, 0, 0, 0);
        }
        if (pl->frame > 12.7f && pl->frame < 13.3f) {
            SndCall(6, 0x61, &pl->pos, 0, 0, 0);
            SndCall(1, 0x2A, &pl->pos, 0, 0, 0);
        }
        if (pl->motionMove()) {
            pl->x400 += 1000.0f;
            pl->x3E0--;
            if (pl->x3E0 == 0) {
                pl->motionSet(PL_ARC_PTR(pG->pPlArc, 0x29), 5, 0, 1, 0);
                pl->shdCol = 0xFF;
                pl->xFE = 3;
            }
        }
        break;
    case 3:
        if (pl->frame > 0.7f && pl->frame < 1.3f) {
            SndCall(6, 0x60, &pl->pos, 0, 0, 0);
            SndCall(1, 0x2A, &pl->pos, 0, 0, 0);
        }
        if (pl->frame > 13.7f && pl->frame < 14.3f) {
            SndCall(6, 0x61, &pl->pos, 0, 0, 0);
            SndCall(1, 0x2A, &pl->pos, 0, 0, 0);
        }
        if (pl->frame > 23.7f && pl->frame < 24.3f) {
            SndCall(6, 0x60, &pl->pos, 0, 0, 0);
            SndCall(1, 0x2A, &pl->pos, 0, 0, 0);
        }
        if (pl->frame > 33.7f && pl->frame < 34.3f) {
            SndCall(5, 3, &pl->pos, 0, 0, 0);
            SndCall(1, 0x2A, &pl->pos, 0, 0, 0);
        }
        if (pl->frame > 38.7f && pl->frame < 39.3f) {
            SndCall(5, 2, &pl->pos, 0, 0, 0);
            SndCall(1, 0x2A, &pl->pos, 0, 0, 0);
        }
        if (pl->motionMove()) {
            pl->x400 += 1500.0f;
            pl->dmg.clear();
            pl->atari.throughOff();
            PlRoutineSet(pl, 0, 0, 0, 0);
            pl->flags_420 |= 0x800;
        }
        break;
    case 0xA:
        FSet(pl->x400, pl->pos.y);
        pl->motionSet(PL_ARC_PTR(pG->pPlArc, 0x2A), 5, 0, 1, 0);
        pl->pNeck->motL = 0;
        pl->xFE = 0xB;
        pl->flags_420 &= ~0x800;
    case 0xB:
        if (pl->frame > 7.7f && pl->frame < 8.3f) {
            SndCall(6, 0x60, &pl->pos, 0, 0, 0);
            SndCall(1, 0x2A, &pl->pos, 0, 0, 0);
        }
        if (pl->frame > 25.7f && pl->frame < 26.3f) {
            SndCall(6, 0x61, &pl->pos, 0, 0, 0);
            SndCall(1, 0x2A, &pl->pos, 0, 0, 0);
        }
        if (pl->motionMove()) {
            pl->x400 -= 1500.0f;
            if (pl->x3E0 != 0) {
                pl->motionSet(PL_ARC_PTR(pG->pPlArc, 0x2B), 3, 0, 5, 0);
                pl->xFE = 0xC;
            } else {
                pl->motionMove();
                pl->xFE = 0xD;
            }
        }
        break;
    case 0xC:
        if (pl->frame > 0.7f && pl->frame < 1.3f) {
            SndCall(6, 0x60, &pl->pos, 0, 0, 0);
            SndCall(1, 0x2A, &pl->pos, 0, 0, 0);
        }
        if (pl->frame > 12.7f && pl->frame < 13.3f) {
            SndCall(6, 0x61, &pl->pos, 0, 0, 0);
            SndCall(1, 0x2A, &pl->pos, 0, 0, 0);
        }
        if (pl->motionMove()) {
            pl->x400 -= 1000.0f;
            pl->x3E0--;
            if (pl->x3E0 == 0) {
                pl->motionSet(PL_ARC_PTR(pG->pPlArc, 0x2C), 5, 0, 1, 0);
                pl->motionMove();
                pl->xFE = 0xD;
            }
        }
        break;
    case 0xD:
        if (pl->frame > 3.7f && pl->frame < 4.3f) {
            SndCall(6, 0x60, &pl->pos, 0, 0, 0);
            SndCall(1, 0x2A, &pl->pos, 0, 0, 0);
        }
        if (pl->frame > 9.7f && pl->frame < 10.3f) {
            SndCall(6, 0x61, &pl->pos, 0, 0, 0);
            SndCall(1, 0x2A, &pl->pos, 0, 0, 0);
        }
        if (pl->frame > 22.7f && pl->frame < 23.3f) {
            SndCall(6, 0x60, &pl->pos, 0, 0, 0);
            SndCall(1, 0x2A, &pl->pos, 0, 0, 0);
        }
        if (pl->motionMove()) {
            pl->x400 -= 500.0f;
            pl->dmg.clear();
            pl->atari.throughOff();
            PlRoutineSet(pl, 0, 0, 0, 0);
        }
        break;
    }
}

// Crouch behind cover: 0/1 down, 0xA look around (the upper body turns with the side keys, motions
// 0x68 / 0x69 past 60 degrees), 0xB/0xC turned, 0x14 standing up; A/B stand up, the aim key aims.
void pl_R1_Crouch(cPlayer* pl)
{
    Vec spd;
    Vec rot;
    const f32 endFrame = 8.0f;

    switch (pl->xFE) {
    case 0:
        pl->endCamera();
        CamCtrl.resetCameraAngle();
        pl->motionSet(PL_ARC_PTR(pG->pPlArc, 0x5A), 3, 0, 0, 0);
        pl->pNeck->motL = 0;
        pl->xFE = 1;
        pl->flags_420 |= 0x40;
        pl->x400 = pl->rot.y;
    case 1:
        if (pl->motionMove()) {
            pl->motionSet(PL_ARC_PTR(pG->pPlArc, 0x5B), 3, 0, 5, 0);
            pl->motionMove();
            pl->xFE = 0xA;
        }
        break;
    case 0xA:
        if (Key.on & 4) {
            pl->rot.y -= cPlayer::SPEED_WALK_TURN;
            pl->pParts->rot.y += cPlayer::SPEED_WALK_TURN;
        }
        if (Key.on & 8) {
            pl->rot.y += cPlayer::SPEED_WALK_TURN;
            pl->pParts->rot.y -= cPlayer::SPEED_WALK_TURN;
        }
        if (pl->pParts->rot.y > 1.0471976f) {
            pl->motionSet(PL_ARC_PTR(pG->pPlArc, 0x68), 3, 0, 1, 0);
            pl->xFE = 0xB;
        }
        if (pl->pParts->rot.y < -1.0471976f) {
            pl->motionSet(PL_ARC_PTR(pG->pPlArc, 0x69), 3, 0, 1, 0);
            pl->xFE = 0xC;
        }
        pl->motionMove();
        break;
    case 0xB:
        if (Key.on & 4) {
            pl->rot.y -= cPlayer::SPEED_WALK_TURN;
            pl->pParts->rot.y += cPlayer::SPEED_WALK_TURN;
        }
        if (Key.on & 8) {
            pl->rot.y += cPlayer::SPEED_WALK_TURN;
            pl->pParts->rot.y -= cPlayer::SPEED_WALK_TURN;
        }
        MotionGetSpeed(pl, MOTION(pl), 0, &spd, &rot);
        pl->pParts->rot.y += rot.y;
        if (pl->motionMove()) {
            pl->motionSet(PL_ARC_PTR(pG->pPlArc, 0x5B), 3, 0, 5, 0);
            pl->motionMove();
            pl->xFE = 0xA;
        }
        break;
    case 0xC:
        if (Key.on & 4) {
            pl->rot.y -= cPlayer::SPEED_WALK_TURN;
            pl->pParts->rot.y += cPlayer::SPEED_WALK_TURN;
        }
        if (Key.on & 8) {
            pl->rot.y += cPlayer::SPEED_WALK_TURN;
            pl->pParts->rot.y -= cPlayer::SPEED_WALK_TURN;
        }
        MotionGetSpeed(pl, MOTION(pl), 0, &spd, &rot);
        pl->pParts->rot.y += rot.y;
        if (pl->motionMove()) {
            pl->motionSet(PL_ARC_PTR(pG->pPlArc, 0x5B), 3, 0, 5, 0);
            pl->motionMove();
            pl->xFE = 0xA;
        }
        break;
    case 0x14:
        pl->motionMove();
        pl->pParts->rot.y *= 0.5f;
        if (MotionCheckCrossFrame(MOTION(pl), endFrame)) {
            pl->pParts->rot.y = 0.0f;
            PlRoutineSet(pl, 0, 0, 2, 0);
        }
        break;
    }
    if (pl->xFE >= 0xA && pl->xFE <= 0x13) {
        if (Joy[0].trg & 0x300) {
            pl->motionSet(PL_ARC_PTR(pG->pPlArc, 0x5C), 5, 0, 1, 0);
            pl->motionMove();
            pl->xFE = 0x14;
            pl->flags_420 &= ~0x40;
        }
        if (joyKamae()) {
            PlRoutineSet(pl, 0, 6, 0, 0);
        }
    }
}

// Jump over a wall: 0x5E jump (the position climbs jumpHeight over frames 4..13), 0x5F fall once
// a floor is found, then the landing motion.
void pl_R1_JumpFall(cPlayer* pl)
{
    pl->flags_420 &= ~0x180;
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pG->pPlArc, 0x5E), (int) PL_ARC_PTR(pG->pPlArc, 0x60), 3, 0x201, 0);
        pl->pNeck->motL = 0;
        pl->atari.throughOn();
        pl->shdCol = 0xFF;
        pl->x3E0 = 0;
        pl->xFE = 1;
        pl->flags_420 &= ~0x800;
    case 1:
        pl->x3E0++;
        pl->flags_420 |= 0x100;
        if (pl->x3E0 >= 5 && pl->x3E0 <= 14) {
            pl->pos.y += pl->jumpHeight * 0.1f;
        }
        if (pl->frame >= 7.0f) {
            if (fallCheck(pl)) {
                MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pG->pPlArc, 0x5F), (int) PL_ARC_PTR(pG->pPlArc, 0x61), 3, 0x201, 0);
                pl->dmg.clear();
                pl->atari.throughOff();
                pl->xFE = 2;
                pl->flags_420 |= 0x880;
            }
        }
        pl->motionMove();
        break;
    case 2:
        pl->motionMove();
        if (pl->frame >= 20.0f) {
            PlRoutineSet(pl, 0, 0, 2, 0);
        }
        break;
    }
}

// Whistle (motion 0x7C): the SE and flags_500C bit23 at frame 20.
void pl_R1_Whistle(cPlayer* pl)
{
    switch (pl->xFE) {
    case 0:
        pl->motionSet(PL_ARC_PTR(pG->pPlArc, 0x7C), 7, 0, 1, 0);
        pl->pNeck->motL = 0;
        pl->xFE = 1;
    case 1:
        if (MotionCheckCrossFrame(MOTION(pl), 20.0f)) {
            SndCall(1, 0xE, &pl->pParts->worldPos, 0, 0, 0);
            pG->flags_500C |= 0x00800000;
        }
        if (pl->motionMove()) {
            PlRoutineSet(pl, 0, 0, 0, 0);
        }
        break;
    }
}

// Routine 1/0xA: the registered handler (cEm::pAuxFunc).
void pl_R1_Aux(cPlayer* pl)
{
    pl->pAuxFunc(pl);
}

// Climb up a ledge (registered motions 0/1, or 8/9 unless x3E0 == 1): the motion base walks the
// player to 400 in front of the action wall hit.
void pl_R1_LevelUp(cPlayer* pl)
{
    Vec pos;
    Vec rot;

    pl->dmg.set(0, 10);
    switch (pl->xFE) {
    case 0: {
        void* m0;
        void* m1;
        pl->flags_420 &= ~0x800;
        pl->atari.throughOn();
        if (pl->x3E0 == 1) {
            m0 = pl->pRegistMot[0];
            m1 = pl->pRegistMot[1];
        } else {
            m0 = pl->pRegistMot[8];
            m1 = pl->pRegistMot[9];
        }
        MotionSetCore(pl, MOTION(pl), m0, (int) m1, 6, 5, 0);
        pl->pNeck->motL = 0;
        PSVECScale(&pl->actWallNrm, &pos, 400.0f);
        PSVECAdd(&pos, &pl->actWallHit, &pos);
        pos.y = pl->pos.y;
        rot.x = 0.0f;
        rot.y = pl->x400;
        rot.z = 0.0f;
        pl->pMotBase->set((cMotModel*) (cModel*) pl, &pos, &rot, 10);
        pl->xFE = 1;
    }
    case 1:
        if (pl->motionMove()) {
            pl->flags_420 &= ~0x800;
            PlRoutineSet(pl, 0, 0, 0, 0);
            pl->atari.throughOff();
        }
        break;
    }
}

// Climb down a ledge (registered motions 10/11); the position drops 1000 at the end when x3E0 == 1.
void pl_R1_LevelDown(cPlayer* pl)
{
    Vec pos;
    Vec rot;

    pl->dmg.set(0, 10);
    switch (pl->xFE) {
    case 0:
        pl->flags_420 &= ~0x800;
        pl->atari.throughOn();
        MotionSetCore(pl, MOTION(pl), pl->pRegistMot[10], (int) pl->pRegistMot[11], 6, 5, 0);
        pl->pNeck->motL = 0;
        PSVECScale(&pl->actWallNrm, &pos, 400.0f);
        PSVECAdd(&pos, &pl->actWallHit, &pos);
        pos.y = pl->pos.y;
        rot.x = 0.0f;
        rot.y = pl->x400;
        rot.z = 0.0f;
        pl->pMotBase->set((cMotModel*) (cModel*) pl, &pos, &rot, 10);
        pl->xFE = 1;
    case 1:
        if (pl->motionMove()) {
            if (pl->x3E0 == 1) {
                pl->pos.y -= 1000.0f;
            }
            pl->flags_420 |= 0x800;
            PlRoutineSet(pl, 0, 0, 0, 0);
            pl->atari.throughOff();
        }
        break;
    }
}

// Push an object (cPlPush): 0/1 take hold (motion 0x20), 0x14/0x15 push (0x21, SE 0x55 per loop),
// 0x16 the object stopped (0x31), 0x28 release (0x22), 0x32 release without a target (idle).
void pl_R1_ObjPush(cPlayer* pl)
{
    switch (pl->xFE) {
    case 0:
        CamCtrl.startPushObject();
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pG->pPlArc, 0x20), 0, 6, 5, 0);
        pl->pNeck->motL = 0;
        pl->x3E0 = 0;
        pl->xFE = 1;
        pl->flags_420 |= 8;
    case 1:
        if (pl->motionMove()) {
            pl->xFE = 0x14;
        }
        if ((Joy[0].on & 0x100) && !pushTargetDead(pl->pPush)) {
            if (pl->pPush->plAdjust()) {
                break;
            }
        }
        pl->xFE = 0x32;
        break;
    case 0x14:
        pl->pPush->pushTargetInit(0);
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pG->pPlArc, 0x21), 0, 0, 5, 0);
        SndCall(6, 0x55, &pl->getPartsPtr(0)->worldPos, 0, 0, 0);
        pl->x3E0 = 0;
        pl->xFE = 0x15;
    case 0x15:
        if (!(Joy[0].on & 0x100) || pushTargetDead(pl->pPush)) {
            pl->pPush->stopTarget();
            pl->xFE = 0x28;
        }
        if (pl->motionMove()) {
            SndCall(6, 0x55, &pl->getPartsPtr(0)->worldPos, 0, 0, 0);
        }
        if (pl->pPush->pushTarget()) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pG->pPlArc, 0x31), 0, 3, 5, 0);
            pl->xFE = 0x16;
        }
        break;
    case 0x16:
        pl->motionMove();
        if (!(Joy[0].on & 0x100) || pushTargetDead(pl->pPush)) {
            pl->xFE = 0x28;
        }
        break;
    case 0x28:
        CamCtrl.endPushObject();
        BitOff(pl->flags_420, 8);
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pG->pPlArc, 0x22), 0, 5, 5, 0);
        PlRoutineSet(pl, 0, 0, 2, 0);
        break;
    case 0x32:
        CamCtrl.endPushObject();
        BitOff(pl->flags_420, 8);
        MotionSetCore(pl, MOTION(pl), pl->pMotTbl[0], 0, 0xF, 5, 0);
        PlRoutineSet(pl, 0, 0, 2, 0);
        break;
    }
}

// Climb over a fence / through a window (motions 0x53/0x54 wide, 0x55/0x56 narrow): the motion base
// walks to PlFancePos (PlFanceFlag) or 400 in front of the action wall; a key ends it past the
// frame by character, foot SEs at 27.7 and 29.7.
void pl_R1_Fance(cPlayer* pl)
{
    Vec pos;
    Vec rot;
    int end;

    switch (pl->xFE) {
    case 0:
        pl->atari.clrFlag100();
        pl->atari.setPriority(2);
        if (pG->x4FB8 == 1 || pG->x4FB8 == 2 || pG->x4FB8 == 4) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pG->pPlArc, 0x53), (int) PL_ARC_PTR(pG->pPlArc, 0x54), 3, 5, 0);
        } else if (pl->xFF & 4) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pG->pPlArc, 0x55), (int) PL_ARC_PTR(pG->pPlArc, 0x56), 3, 5, 0);
        } else if ((pl->xFF & 2) || fanceWidthCheck(pl)) {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pG->pPlArc, 0x53), (int) PL_ARC_PTR(pG->pPlArc, 0x54), 3, 5, 0);
        } else {
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pG->pPlArc, 0x55), (int) PL_ARC_PTR(pG->pPlArc, 0x56), 3, 5, 0);
        }
        PSet(pl->pNeck->motL, 0);
        if (PlFanceFlag & 1) {
            pos.x = PlFancePos.x;
            pos.y = pl->pos.y;
            pos.z = PlFancePos.z;
            rot.x = 0.0f;
            rot.y = pl->x400;
            rot.z = 0.0f;
        } else {
            PSVECScale(&pl->actWallNrm, &pos, 400.0f);
            PSVECAdd(&pos, &pl->actWallHit, &pos);
            pos.y = pl->pos.y;
            pos.y += getHeighAdjust(pl);
            rot.x = 0.0f;
            rot.y = pl->x400;
            rot.z = 0.0f;
        }
        pl->pMotBase->set((cMotModel*) (cModel*) pl, &pos, &rot, 10);
        PlFanceFlag = 0;
        pl->xFE = 1;
    case 1:
        if (!(pl->xFF & 1)) {
            if (pl->frame <= 24.0f) {
                fanceAdjust(pl);
            }
        }
        break;
    }
    end = pl->motionMove();
    if (Key.on & 0x10F) {
        switch (pG->x4FB8) {
        case 0:
        default:
            if (pl->frame >= 30.0f) {
                end |= 1;
            }
            break;
        case 1:
            if (pl->frame >= 60.0f) {
                end |= 1;
            }
            break;
        case 2:
            if (pl->frame >= 60.0f) {
                end |= 1;
            }
            break;
        }
    }
    if (pl->frame > 27.7f && pl->frame < 28.3f) {
        SndCall(5, 0xE, &pl->pos, 0, 0, 0);
    }
    if (pl->frame > 29.7f && pl->frame < 30.3f) {
        SndCall(5, 0xD, &pl->pos, 0, 0, 0);
    }
    if (end) {
        pl->atari.setPriority(0);
        pl->atari.setFlag100();
        PlRoutineSet(pl, 0, 0, 0, 0);
    }
    pl->dmg.set(0, 2);
}

// 1 when the fence is wide enough for the two-hand motion: nothing but attribute 0x20 between the
// points 1200 left of the wall normal at 500 / -500 depth, and no wall from the player to the first.
int fanceWidthCheck(cPlayer* pl)
{
    Vec rot;
    Vec p0;
    Vec p1;

    rot.x = rot.z = 0.0f;
    rot.y = (f32) atan2(-pl->actWallNrm.x, -pl->actWallNrm.z);
    p0.x = -1200.0f;
    p0.y = 500.0f;
    p0.z = -500.0f;
    RotVector(&p0, &rot, &p0);
    PSVECAdd(&p0, &pl->pos, &p0);
    p1.x = -1200.0f;
    p1.y = 500.0f;
    p1.z = 500.0f;
    RotVector(&p1, &rot, &p1);
    PSVECAdd(&p1, &pl->pos, &p1);
    if (SatMgr.hitCheck(&p0, &p1, 0, 0, 0, 0) & ~0x20) {
        return 0;
    }
    return SatMgr.hitCheck(&pl->pParts->worldPos, &p0, 0, 0, 0, 0) == 0;
}

// Slide the player sideways off a wall next to the fence: 50 to the left when the right side
// (300 across) is blocked, to the right when the left side (-700 across) is.
void fanceAdjust(cPlayer* pl)
{
    Vec p0;
    Vec p1;
    int hit;

    p0.x = 300.0f;
    p0.y = 400.0f;
    p0.z = 0.0f;
    RotVector(&p0, &pl->rot, &p0);
    PSVECAdd(&p0, &pl->pos, &p0);
    p1.x = 300.0f;
    p1.y = 400.0f;
    p1.z = 1000.0f;
    RotVector(&p1, &pl->rot, &p1);
    PSVECAdd(&p1, &pl->pos, &p1);
    hit = SatMgr.hitCheck(&p0, &p1, 0, 0, 0, 0);
    if (hit != 0 && !(hit & 0x20)) {
        p0.x = -50.0f;
        p0.y = 0.0f;
        p0.z = 0.0f;
        RotVector(&p0, &pl->rot, &p0);
        PSVECAdd(&pl->pos, &p0, &pl->pos);
        return;
    }
    p0.x = -700.0f;
    p0.y = 400.0f;
    p0.z = 0.0f;
    RotVector(&p0, &pl->rot, &p0);
    PSVECAdd(&p0, &pl->pos, &p0);
    p1.x = -700.0f;
    p1.y = 400.0f;
    p1.z = 1000.0f;
    RotVector(&p1, &pl->rot, &p1);
    PSVECAdd(&p1, &pl->pos, &p1);
    hit = SatMgr.hitCheck(&p0, &p1, 0, 0, 0, 0);
    if (hit != 0 && !(hit & 0x20)) {
        p0.x = 50.0f;
        p0.y = 0.0f;
        p0.z = 0.0f;
        RotVector(&p0, &pl->rot, &p0);
        PSVECAdd(&pl->pos, &p0, &pl->pos);
    }
}

// Drop from a ledge (motions 0x2D/0x2E, 0x2F/0x30 landing): the motion base walks to the ledge,
// the player is kept 40 away from side walls during the first 10 frames, turned towards fallDir
// until frame 20 (30 for Ashley / Ada), then falls until a floor is found; splash effects in water.
void pl_R1_Fall(cPlayer* pl)
{
    Vec pos;
    Vec rot;
    Vec a;
    Vec b;
    Vec d;
    Vec v;
    f32 water;
    int lim;
    const f32 adjustFrame = 10.0f;

    switch (pG->x4FB8) {
    default:
        lim = 30;
        break;
    case 0:
    case 3:
    case 4:
    case 5:
        lim = 20;
        break;
    }
    switch (pl->xFE) {
    case 0:
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pG->pPlArc, 0x2D), (int) PL_ARC_PTR(pG->pPlArc, 0x2E), 3, 5, 0);
        pl->atari.throughOn();
        pl->pNeck->motL = 0;
        pl->atari.setPriority(2);
        if (pSUB) {
            SubCharRegistPlAction(pSUB, &pl->pos, 0, pl->rot.y);
        }
        pl->x3E0 = 0;
        pl->flags_420 &= ~0x800;
        PSVECScale(&pl->actWallNrm, &pos, 400.0f);
        PSVECAdd(&pos, &pl->actWallHit, &pos);
        pos.y = pl->pos.y;
        rot.x = 0.0f;
        rot.y = pl->x400;
        rot.z = 0.0f;
        pl->pMotBase->set((cMotModel*) (cModel*) pl, &pos, &rot, 10);
        pl->xFE = 3;
    case 3:
        if (pl->frame <= adjustFrame) {
            v.x = 0.0f;
            v.y = 1.5707964f;
            v.z = 0.0f;
            RotVector(&pl->fallDir, &v, &d);
            PSVECScale(&d, &a, 400.0f);
            PSVECAdd(&a, &pl->pParts->worldPos, &a);
            PSVECScale(&pl->fallDir, &b, 1000.0f);
            PSVECAdd(&b, &a, &b);
            if (!(SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x00100000)) {
                PSVECScale(&d, &a, 40.0f);
                PSVECSubtract(&pl->pos, &a, &pl->pos);
            }
            v.x = 0.0f;
            v.y = -1.5707964f;
            v.z = 0.0f;
            RotVector(&pl->fallDir, &v, &d);
            PSVECScale(&d, &a, 400.0f);
            PSVECAdd(&a, &pl->pParts->worldPos, &a);
            PSVECScale(&pl->fallDir, &b, 1000.0f);
            PSVECAdd(&b, &a, &b);
            if (!(SatMgr.hitCheck(&a, &b, 0, 0, 0, 0) & 0x00100000)) {
                PSVECScale(&d, &a, 40.0f);
                PSVECSubtract(&pl->pos, &a, &pl->pos);
            }
        }
        if (pl->x3E0 == 0) {
            if (GetWaterHeight(&pl->pos, &water) && water > pl->pos.y) {
                EstSet((int) pl, -1, 0, 0, 1, 0x24, 0, 0, (u32) pl, 0);
                pl->x3E0 = 1;
            }
        }
        pl->motionMove();
        if (pl->frame <= (f32) lim) {
            pl->rot.y += Muku3(&pl->fallDir, pl->rot.y, 0.31415927f);
            break;
        }
        if (fallCheck(pl)) {
            if (pl->x3E0 == 0) {
                EstSet((int) pl, -1, 0, 0, 3, ChkWaterEffectEnable(&pl->pos) ? 0x12 : 0x11, 0, 0, (u32) pl, 0);
            }
            FSet(pl->pos.y, SatMgr.getFloor(&pl->pos, 600.0f, 100000.0f, 0, 0));
            MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pG->pPlArc, 0x2F), (int) PL_ARC_PTR(pG->pPlArc, 0x30), 0, 5, 0);
            pl->motionMove();
            pl->xFE = 4;
            pl->flags_420 |= 0x800;
        }
        break;
    case 4:
        if (pl->motionMove()) {
            pl->atari.setPriority(0);
            pl->atari.throughOff();
            pl->dmg.clear();
            PlRoutineSet(pl, 0, 0, 0, 0);
        }
        break;
    }
}

// Routine 6: dijection (motion 0x57/0x58) after the camera ends; the face blend is reset, SE 0x45
// at frame 60.
void pl_R0_Dijection(cPlayer* pl)
{
    if (pl->xFD == 0) {
        cModelInfo* face;
        pl->endCamera();
        face = pl->pBody->pFace;
        if (VALID_PTR(face)) {
            face->x84 = 0.0f;
            face->x70 = 0.0f;
            face->x5C = 0.0f;
        }
        MotionSetCore(pl, MOTION(pl), PL_ARC_PTR(pG->pPlArc, 0x57), (int) PL_ARC_PTR(pG->pPlArc, 0x58), 3, 1, 0);
        pl->xFD = 1;
    }
    if (MotionCheckCrossFrame(MOTION(pl), 60.0f)) {
        SndCall(1, 0x45, &pl->pParts->worldPos, 0, 0, 0);
    }
    pl->motionMove();
}

// Boat driving idle (pMotTbl[0] / archive 0x32 when damaged).
void pl_R1_BoatDrive(cPlayer* pl)
{
    if (pl->xFE == 0) {
        pl->motionSet(pl->pMotTbl[0], pl->pMotTbl[1], PL_ARC_PTR(pG->pPlArc, 0x32), PL_ARC_PTR(pG->pPlArc, 0x33), 3, 0);
        pl->xFE = 1;
    }
    pl->motionMove();
    pl->checkCtrl();
}

// Floor under the player between parts 4's height and the fall target: the player is put on it,
// evTarget receives the hit point. 1 when found.
int fallCheck(cPlayer* pl)
{
    Vec p;
    Vec hit;

    p.x = pl->pos.x;
    p.y = pl->getPartsPtr(4)->oldWorldPos.y;
    p.z = pl->pos.z;
    if (SatMgr.hitCheck(&p, &pl->pos, &pl->evTarget2, &hit, 0, 0)) {
        pl->pos.y = pl->evTarget2.y;
        pl->evTarget.x = hit.x;
        pl->evTarget.y = hit.y;
        pl->evTarget.z = hit.z;
        return 1;
    }
    return 0;
}

// Floor height 1000 behind the action wall hit relative to the player.
f32 getHeighAdjust(cPlayer* pl)
{
    Vec p;

    PSVECScale(&pl->actWallNrm, &p, -1000.0f);
    PSVECAdd(&p, &pl->actWallHit, &p);
    return SatMgr.getFloor(&p, 600.0f, 100000.0f, 0, 0) - pl->pos.y;
}
