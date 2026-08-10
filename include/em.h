#ifndef EM_H
#define EM_H

#include "types.h"
#include "cManager.h"
#include "model.h"
#include "atariInfo.h"
#include "main_mem.h"

// The player classes derive from cEm, so the player-only fields the pl_* units touch live in
// cEm too (they all sit below 0xDE0).
// Hit box ("yarare") / damage part info (cEm+0x33C for the player; GetWepTargetList returns
// pointers to these per target), 0x34 bytes; extra boxes are chained through `next` (at_mod.cpp
// YarareAdd / YarareAddCube).
struct EmHitInfo {
    Vec ofs;              // 0x00  box centre offset from the model / parts (yarareInit0 x, y, z)
    Vec pos;              // 0x0C  hit position in the parts (obj1b: the spear sticks here)
    f32 width;            // 0x18
    f32 height;           // 0x1C
    f32 depth;            // 0x20  cube depth (YarareInitCube / YarareAddCube set it with flags bit3)
    u16 flags;            // 0x24  bit3 (0x8): cube, bit5 (0x20): the hit sets cDmgInfo bit5 too (pl_wep PlWepHitCheck2)
    s16 partsNo;          // 0x26  parts the effect is placed at (0 = the model itself), 1-based
    f32 rad;              // 0x28  squared distance hit point -> line start (em_sub emLineAtCk / emBoxAtCk)
    f32 dist;             // 0x2C  squared distance of the hit from the aim line (em_sub GetWepTargetList sorts on it)
    EmHitInfo* next;      // 0x30  next hit box of the model (YarareAdd)
};

// Damage info at cEm+0x324 (game/em.cpp), 0x18 bytes. set(0, 10, kind, pos, rad, part) registers a hit.
class cDmgInfo {
public:
    union {
        u32 flags;        // 0x00
        struct {
            u8 stat;      // 0x00  bit0: a hit is registered, bit5 (pl_wep)
            u8 x1;        // 0x01  frames the hit stays registered (move: bit7 = hold, low bits count down)
            u16 x2;
        };
        struct {
            u8 pad_0[2];
            u8 kind;      // 0x02  set() kind
            u8 x3;
        };
    };
    Vec pos;              // 0x04  hit position
    f32 rad;              // 0x10
    EmHitInfo* part;      // 0x14  hit part

    cDmgInfo();
    void set(int a, int b, u8 kind, Vec* pos, f32 rad, EmHitInfo* part);
    void set(int a, int b);   // stores the two bytes at 0/1 (pl_sub: set(0, 10), set(0, 0x80))
    void clear();
    void move();              // counts x1 down; clears stat when it reaches 0
};

// Room water effect table registered at cEm::pRoomEff (pl_sub PlRegistRoomEff): 3 entries of
// {u32 id; u8 pad[3]; u8 type;} used as EstSet(..., id, type, ...) for the ripple / splash effects.
struct PlRoomEff {
    u32 id;
    u8 pad_4[3];
    u8 type;
};

// Blend motion work (0xD0 bytes): a MotionWork (model.h) without the trailing blend/flip/blendTbl
// pointers. cEm::neckMot (0x42C) and cMot3::work are one; MotionWork::blend points at it.
struct MotionWorkSub {
    void* data;           // 0x00  MotionData*, NULL = no motion
    u8 pad_4[0x44 - 0x4];
    u32 flags2;           // 0x44  MotionWork::flags2 (bit28: no IK, bit31)
    u8 pad_48[0xC0 - 0x48];
    f32 speedRate;        // 0xC0  MotionWork::speedRate (em38 shell motion: 1.0 before every MotionSetCore)
    u8 pad_C4[4];
    f32 blendRate;        // 0xC8  weight of this work in the owner's MotionMove blend
    u8 pad_CC[4];
};

struct PlArc;      // global.h
struct EmiEntry;   // embarrel.h
class cSubChar;    // pl_npc.h
class cLight;      // light.h

// Character work (game/em.cpp), sizeof 0xDE0: the cModel (0x320, which carries the motion work,
// the cAtariInfo, pFootShadowTbl and the light area) plus the fields below.
class cEm : public cModel {
public:
    s16 hp;               // 0x320
    s16 hpMax;            // 0x322
    // 0x324 .. 0x33C: the cDmgInfo (em.cpp constructs it explicitly; a class with a constructor
    // cannot sit in a union directly) and the same bytes under the names the other units use.
    union {
        struct {
            cDmgInfo dmg; // 0x324  (obj08: dmg.set on a hit target)
        };
        struct {
            union {
                u32 flags_324;    // 0x324  (db_cam: upper 16 bits set = dead)
                struct {
                    u8 x324;      // 0x324  (pl_dmg: cleared when the damage motion ends)
                    u8 x325;      // 0x325  (pl_dmg: 5 at the end, bit7 while the damage motion plays)
                    u16 x326;
                } st;
                struct {
                    u8 dmHit;     // 0x324  damage registered this frame (emhit emHitDmCk consumes it)
                    u8 dmType;    // 0x325  emhit: 1, 0x11 for weapon 0x10
                    u8 dmWep;     // 0x326  weapon id of the damage (cEmHit::ckDmgWeapon)
                    u8 dm327;
                };
            };
            Vec x328;             // 0x328  (obj14: damage position when EmGetDmPos has none)
            f32 dmRad;            // 0x334  cDmgInfo::set rad
            EmHitInfo* dmPart;    // 0x338  cDmgInfo::set part (emswitch: its rad decides the blood type)
        };
    };
    EmHitInfo hitInfo;    // 0x33C .. 0x370  (obj08: the player's hit part for the damage effect)
    f32 plDist2;          // 0x370  squared distance to the player (db_work prints its sqrt)
    f32 x374;             // 0x374  (em_set: 1e16 at creation)
    union {
        u32 x378;         // 0x378  (pl_sub EndPlDamage/EndSubDamage: x378 = x37C)
        PlArc* subArc;          // 0x378  cSubChar: motion archive the routines index (pl_npc.cpp)
    };
    union {
        u32 x37C;         // 0x37C
        PlArc* subArc2;         // 0x37C  cSubChar: the archive restored after a damage routine
    };
    Vec lockOfs;          // 0x380  lock-on point offset in the lockParts' matrix (pl_wep)
    u8 lockParts;         // 0x38C  parts the lock-on point follows (pl_wep; AutoTrack uses the low 3 bits)
    u8 x38D;              // 0x38D  (db_cam "set=")
    u8 pad_38E[2];
    void (*pScenario)(cEm*);  // 0x390  em_sub EmScenario: called with the enemy when set
    u8 pad_394[4];
    u8 emsetNo;           // 0x398
    u8 pad_399[3];
    union {
        struct {
            u8 x39C;
            u8 x39D;      // 0x39D  (obj16: the type 1 head is drawn at half scale while set)
            u8 pad_39E[0x3A8 - 0x39E];
        };
        Vec catchOfs;     // 0x39C  em_sub EmCatchPLSet: offset the caught model keeps to the catcher
    };
    Vec x3A8;             // 0x3A8  (objTrolley objTrolleySetAdjust adds the car movement to it)
    f32 catchTurn;        // 0x3B4  em_sub EmCatchPLSet: rot.y left to turn (EmCatchMotionMove eats it)
    int dmgType;          // 0x3B8  (pl_sub SetPlDamage/SetSubDamage first argument)
    u8 rckFlag;           // 0x3BC  route_ck: bit0 = rckNear valid this frame (RouteCk clears it)
    s8 rckPoint;          // 0x3BD  route_ck: way point the enemy heads to (-1 = none)
    s8 rckNext;           // 0x3BE  route_ck: way point nearest to the target
    s8 rckNear;           // 0x3BF  route_ck: way point nearest to the enemy
    u8 pad_3C0[4];
    u32 status;           // 0x3C4  setStatus / clearStatus / checkStatus bits (bit0 = in battle, bit1, bit11)
    u32 flags_3C8;        // 0x3C8  (db_cam "Flag=")
    f32 x3CC;             // 0x3CC  (em_set: list entry s16 x1A * 1000)
    u8 x3D0;              // 0x3D0  (em_set: list entry byte 0xB)
    u8 itemFlag;          // 0x3D1  setItem 5th argument (setNoItem: 0)
    u8 pad_3D2[4];
    u16 itemNo;           // 0x3D6  setItem a (setNoItem: 0xFFFF)
    u16 itemNum;          // 0x3D8  setItem b
    u16 item3DA;          // 0x3DA  setItem c
    u16 item3DC;          // 0x3DC  setItem d
    u8 pad_3DE[2];
    union {
        u32 x3E0;         // 0x3E0  player: event walk flag / damage timer
        cSubChar* subSelf;         // 0x3E0  cSubChar: the model the routines animate (itself)
    };
    // 0x3E4 .. 0x400: player fields, and the partner's neck control (cSubChar::neckCtrl) on the same bytes
    union {
        struct {
            int x3E4;             // 0x3E4  player damage: 1 = turning towards x400
            u32 x3E8;             // 0x3E8  player damage (blow): water splash done
            int x3EC;             // 0x3EC  player damage (emrock plemRockEscape): EMI route point run to (-1 = none)
            int x3F0;             // 0x3F0  emrock escape: frames since the last button press
            int x3F4;             // 0x3F4  emrock escape: EMI goal sub type (plemRockEscapeCk)
            int x3F8;             // 0x3F8  emrock escape: goal reached
            int x3FC;             // 0x3FC  emrock escape: Rnd() & 1 (action button variant)
        };
        struct {
            int subNeckOn;        // 0x3E4  cSubChar: neckSet() called this frame
            f32 subNeckX;         // 0x3E8
            f32 subNeckAng;       // 0x3EC  cSubChar: current neck angle (parts 3)
            f32 subNeckZ;         // 0x3F0
            Vec subNeckPos;       // 0x3F4  cSubChar: position looked at
        };
    };
    union {
        f32 x400;         // 0x400  player: event turn limit / damage direction angle (123.0 = none)
        struct {
            u16 subFlags;   // 0x400  sub character (cSubChar): bit7 (0x80) manual control, bit6 (0x40) ok to control, bit4 (0x10), bit3 (0x8) move-to, bit0
            u16 subFlags2;  // 0x402  cSubChar (pl_sub SubCharMoveTo clears 0x60)
        };
    };
    // 0x404 .. 0x520: player fields, and the same bytes as the partner (cSubChar, pl_npc.cpp) uses them
    union {
        struct {
            Vec evTarget;         // 0x404  player event: walk-to position
            Vec evTarget2;        // 0x410  player: position setPos'd while flags_420 bit7 is set (objRobo R0WaitGondola)
            u32 flags_41C;        // 0x41C  player: bit8 (0x100) event motion done -> reset routine
            u32 flags_420;        // 0x420  player: bit6 (0x40) knife routine ends into routine 0x11
            void** pMotTbl;       // 0x424  player: motion data table ([0] walk, [2] turn, [0x5F..0x6C] set by setMotion)
            void** pRegistMot;    // 0x428  player: registered motion table (pl_sub PlRegistMotion fills [0..11])
            MotionWorkSub neckMot;   // 0x42C .. 0x4FC  player: neck turn motion (pl_class cPlNeck::motSet), blended via blendMot
            u8 x4FC;              // 0x4FC  (pl_sub PlChangeData/PlMotionReset clear it)
            u8 x4FD;              // 0x4FD
            u8 x4FE;              // 0x4FE
            u8 xButtonWait;       // 0x4FF  player: frames until the X button (partner command) is accepted again
            f32 blendRate500;     // 0x500  player: em2b plBlendMotSet: neckMot blend rate source (the strangle button mash 0..255)
            u32 sndId504;         // 0x504  player: SndCall handle cPlayer::interrupt stops
            cModel* pLockEm;      // 0x508  player: locked-on enemy (pl_wep lock, knife aim)
            cEm* pBoat;           // 0x50C  player: the jet ski the player rides (pl0e cPl0e::setRide / PlBoatMove)
            class cObjSpear* pSpear;  // 0x510  player: the harpoon in hand (pl0f plboatSetSpear / plboatSpearThrow)
            f32 sightRate;        // 0x514  player: pl0f harpoon aim: vertical sight rate (-0.3927 .. 0.3927)
            int gachaCnt;         // 0x518  player: button mash counter (pl_sub PlGacha*)
            u8 pad_51C[2];
            u8 eyeMode;           // 0x51E  player (pl_sub PlSetEyeMode)
            u8 binoMode;          // 0x51F  player: binocular step (cPlayer::moveBinocular 1 -> 2 -> 3 -> 0)
            u8 dmgFlag520;        // 0x520  player: 1 once setDamage ran
            u8 pad_521;
            u16 dmgCnt522;        // 0x522  player: accumulated setDamage counts; a damage reaction starts past 0xFE
        };
        struct {
            u8 sub404;            // 0x404  cSubChar
            u8 sub405;            // 0x405
            u16 sub406;           // 0x406  frame counter
            u8 sub408;            // 0x408
            u8 sub409;            // 0x409
            u8 sub40A;            // 0x40A  timer
            u8 pad_40B;
            f32 subBlendRate;     // 0x40C  cSubChar: blend rate of subBackMot (pl0e subBlendMotSet, like the player's blendRate500)
            f32 subAng;           // 0x410  angle to the player (analyze)
            f32 subDist;          // 0x414  distance to the player (analyze)
            Vec subTarget;        // 0x418  position to walk to
            f32 sub424;           // 0x424
            Vec subOfs;           // 0x428  offset behind the player (atckPos)
            u32 subPlStatus;      // 0x434  PlGetStatus() of the frame
            u32 sub438;           // 0x438  scenario attribute of the wall in front (anaSatInfo)
            Vec sub43C;           // 0x43C  hit point of the action wall check (actionCheck)
            Vec sub448;           // 0x448  its normal
            MotionWorkSub subBackMot;   // 0x454 .. 0x524  look-back motion blended in (backCheckSet -> blendMot)
        };
    };
    cModelInfo* subHand[2];   // 0x524  cSubChar: hand model infos (pl11 cSubAshley::setHand)
    f32 sub52C;           // 0x52C  cSubChar: fence / window action direction
    int subHideMode;      // 0x530  cSubChar (pl_sub SubCharCtrlHide); pl_npc: general step counter
    int subX534;          // 0x534  cSubChar (SubCharCtrlHide mode 0 sets 1)
    int sub538;           // 0x538  cSubChar: step counter
    int sub53C;           // 0x53C  cSubChar: the catch action button is set (moveFallWait)
    int sub540;           // 0x540  cSubChar: frames waiting for the player
    Vec subHidePos;       // 0x544  cSubChar hide position
    u8 sub550;            // 0x550  cSubChar: frames until the route is re-checked
    u8 pad_551[3];
    struct EmiEntry* sub554;   // 0x554  cSubChar: EMI route entry (type 0xB) walked to (embarrel.h)
    Vec sub558;           // 0x558  cSubChar: ledge position to wait at (catchOn / actionCheck)
    f32 sub564;           // 0x564  cSubChar: angle to turn to while waiting to be caught
    int subAux0;          // 0x568  cSubChar (SetSubAux/SetSubBulldozer arguments)
    int subAux1;          // 0x56C
    f32 subMoveTo[4];     // 0x570  cSubChar (SubCharMoveTo x, y, z, w)
    u8 sub580;            // 0x580  cSubChar: timer
    u8 sub581;            // 0x581
    u8 pad_582[2];
    void* subMot0;        // 0x584  cSubChar registered motions (SubCharRegistMotion, SetSubDamage)
    void* subMot1;        // 0x588
    // 0x58C .. 0x5C4 is the partner's cMotBase (pl_npc.cpp / obj13: `(cMotBase*) &subFlags58C`)
    u8 subFlags58C;       // 0x58C  cSubChar (SetSubDamage sets 0x40)
    u8 pad_58D[0x5C4 - 0x58D];
    u32 subSndId;         // 0x5C4  cSubChar: SndCall handle of the bulldozer SEs (objBull Sub_bull_*)
    f32 subX5C8;          // 0x5C8  cSubChar (obj13 SubLadderClimbCk: the partner climbs only while >= 1000)
    EmHitInfo subHit[3];  // 0x5CC .. 0x668  cSubChar: extra hit boxes (YarareAdd in cSubChar::init)
    u8 pad_668[0x738 - 0x668];
    int satCheckFlag;     // 0x738  player: SatMgr.check flag (player.cpp startUp / move)
    void (*pAuxFunc)(class cPlayer*);  // 0x73C  player: routine 1/0xA (pl_R1_Aux) handler
    struct PlRoomEff* pRoomEff;  // 0x740  player: room water effect table (pl_sub PlRegistRoomEff/PlWaterProc)
    void* boss0;          // 0x744  player (pl_sub PlRegistBoss)
    void* boss1;          // 0x748
    Vec fallDir;          // 0x74C  player: -wallNrm of the ledge to drop from (pl_class fallCheck)
    Vec jumpDir;          // 0x758  player: -normal of the jump-over wall (pl_class jumpCheck)
    f32 jumpHeight;       // 0x764  player: floor height behind the jump wall minus pos.y
    Vec actWallHit;       // 0x768  player: hit point of the action wall check (pl_class actWallCheck)
    Vec actWallNrm;       // 0x774  player: its normal
    u32 actWallAttr;      // 0x780  player: its scenario attribute (0 = no wall in front)
    u8 pad_784[4];
    class cPlWep* pWep;   // 0x788  player: weapon control (pl_wep.cpp, 0x44 bytes)
    class cPlNeck* pNeck; // 0x78C  player: neck control (pl_class.cpp, 0x1C bytes)
    class cPlWaist* pWaist;  // 0x790  player: waist control (pl_class.cpp, 0xC bytes)
    class cPlBody* pBody; // 0x794  player: body / face / hand model set (pl_body.cpp, 0xF0 bytes)
    u8 pad_798[8];
    class cPlPush* pPush; // 0x7A0  player: push-object control (pl_push.cpp, 0x10 bytes)
    class cMotBase* pMotBase;  // 0x7A4  (0x38 bytes)
    u8 pad_7A8[4];
    union {
        Vec bustBase[3];      // 0x7AC  Ashley: rest positions of parts 0x1D, 0x1E, 0x1A (pl_ashley moveBust)
        struct {              // Krauser (pl0a pl_klauser.cpp): the three fading model infos and their state
            cModelInfo* krModel[3];   // 0x7AC  [0]/[1] arm models faded against each other, [2] the tex-render one
            int krX7B8;               // 0x7B8  (ctor: 0)
            int krX7BC;               // 0x7BC
            int krX7C0;               // 0x7C0  (ctor: 0)
            u8 krPad_7C4[0x7D0 - 0x7C4];
        };
    };
    u8 pad_7D0[4];
    cLight* subLight;         // 0x7D4  cSubChar: back light (cLightMgr::createBack)
    void* subShape;       // 0x7D8  cSubChar: ShapeMove work (NULL = none)
    void (*subFunc)();        // 0x7DC  cSubChar: routine 4 (damage) handler (cSubChar::move)
    Vec subBustBase[3];   // 0x7E0  cSubChar: rest positions of parts 0x1D, 0x1E, 0x1A (moveBust)
    u8 pad_804[0x880 - 0x804];
    f32 x880;             // 0x880  player (Krauser): ctor 1.0
    u8 pad_884[0x890 - 0x884];
    int x890;             // 0x890  player (Krauser): cleared by cPlayer::interrupt with pG->flags_5018 bit23
    int x894;             // 0x894  player (Krauser): -1 -> 1 there
    int x898;             // 0x898  player (Krauser): tex-render model alpha pulse counter (0..0x1F, transMove)
    u8 pad_89C[0x9BC - 0x89C];
    f32 x9BC;             // 0x9BC  (objTrolley objTrolleyFallEM: rot.y when thrown off the car)
    u8 pad_9C0[0xD60 - 0x9C0];
    Mtx rackMat;          // 0xD60  cEmRack push range matrix (setRange: rot * trans of the rack)
    Mtx rackInvMat;       // 0xD90  its inverse (adjustRange transforms the position into range space)
    f32 rackRange[4];     // 0xDC0  cEmRack push limits (adjustRange dir 0: [1], 1: -[2], 2: [0], 3: -[3])
    u8 rackFlags;         // 0xDD0  cEmRack: bit4 (0x10) range set; SetRack initialises it to 0xF
    u8 pad_DD1[0xDE0 - 0xDD1];

    cEm();
    virtual ~cEm() {}
    virtual void move();
    virtual void setItem(u16 a, u16 b, u16 c, u16 d, u8 e);  // 0x3D6.. item drop (0x3D1 flag)
    virtual void setNoItem();
    virtual int checkThrow();
    void setStatus(int bit);     // status |= 1 << bit
    void clearStatus(int bit);
    int checkStatus(int stat);
    int initWork();              // be_flag = 0x21, x12E = 0 (the constructor)
};

// Enemy manager (game/em.cpp). The construct id selects the class: 0 player, 1..0xE / others a
// read-table enemy (EmInitFunc), 0x40.. the object enemies (cEmObj, cEmDoor, ...), 0xFF a plain cEm.
class cEmMgr : public cManager<cEm> {
public:
    u32 x34;              // 0x34  next cModel::serial (construct)

    static const char* idName[96];   // debug names per construct id

    cEmMgr();
    // no user destructor: the synthesized one (and cManager<cEm>'s) land after the other inlines
    virtual void* memAlloc(u32 size) { return MemAlloc(size, 1); }
    virtual void memFree(void* p) { MemFree(p); }
    virtual void memClear(cEm* p, u32 size) { memclr_asm(p, size); }
    virtual void log(const char* fmt, ...);
    virtual void destroy(cEm* p);   // em.cpp overrides the cManager one (pl_sub SubCharCtrl / PlDataRelease)
    virtual int construct(cEm* p, u32 id);

    int arrayAlloc(u32 n);        // cManager<cEm>::arrayAlloc + pPL = pSUB = 0; returns 1
    void move();                  // dieCheck, RouteCk, emMove for every alive work (or only pSUB when stopped)
    // first alive enemy with model id `id`, searching from `start->next` (or the list head)
    cEm* getEmPtr(int id, cEm* start);
    int isBattle();               // 1 when any alive enemy has status bit0
    void destroyAll();            // killEm on every alive work (id != 0)
};

extern cEmMgr EmMgr;

// Work `no` of the enemy manager, NULL when out of range. A free function: a cEmMgr member (even an
// out-of-class inline) is emitted out of line into em.cpp, which owns the vtable (ctrl.h CtrlMgrWork).
static inline cEm* EmMgrWork(u32 no)
{
    if (no >= EmMgr.nArray) {
        return 0;
    }
    return (cEm*)((u8*)EmMgr.pArray + EmMgr.size * no);
}

// Pushable rack/crate enemy (game/emrack.cpp); only what pl_push calls.
class cEmRack : public cEm {
public:
    virtual void move();   // key function: keeps the vtable in emrack.o (cEmMgr::construct stores it)

    void setBreak();
    void setDown(Vec* pos);
    void setShock();
    void setEff(u8 eff);
    void setRange(f32 a, f32 b, f32 c, f32 d);
    int adjustRange(u8 dir);
};

extern "C" {
void emMove(cEm* em);        // per-frame update of one alive work: distance to the player, damage info, move()
void battleCheck(cEm* em);
void killEm(cEm* em);
}

#endif
