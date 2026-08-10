#ifndef PLAYER_H
#define PLAYER_H

#include "types.h"
#include "vec.h"
#include "model.h"
#include "em.h"
#include "global.h"
#include "pl_body.h"
#include "pl_wep.h"
#include "pl_cloth.h"

// 0x98-byte work at cEm::p2A4 (player.cpp init1 mem_alloc); only the byte cam_ctrl reads is named.
struct EmWork2A4 {
    u8 x0;
    u8 pad_1[4];
    u8 x5;                       // 0x05
};

class cPlayer;

// Neck control (game/pl_class.cpp), 0x1C bytes at cEm::pNeck: turns the head towards the nearest
// enemy with the neck motions blended into the player's motion (cEm::neckMot).
class cPlNeck {
public:
    cPlayer* pl;         // 0x00
    cEm* target;         // 0x04  enemy looked at
    int timer;           // 0x08  frames left looking (0x7FFFFFFF: until the target changes)
    u16 flags;           // 0x0C  bit0: the right-turn motion is set
    u8 mode;             // 0x0E  0 off, 1 on, 2 -> 1 next frame (PlSetNeck)
    u8 pad_F;
    f32 ang;             // 0x10  current neck angle
    void* motL;          // 0x14  left turn motion data
    void* motR;          // 0x18  right turn motion data

    cPlNeck(cPlayer* pl);
    void init(void* motL, void* motR, int frame);   // range-checked pointers (motSet), frame passed on
    void move();
    void motSet(void* data, int frame);
    cEm* getTarget();
    void setMode(int mode);   // stores byte 0xE (pl_sub PlSetNeck)
};

// Waist control (game/pl_class.cpp), 0xC bytes at cEm::pWaist.
class cPlWaist {
public:
    u32 x0;
    f32 cur;                     // 0x04  current angle
    u32 x8;

    cPlWaist();
    // cur = cur * (1 - rate) + target * rate; returns the delta applied
    f32 set(f32 target, f32 rate);

    static const f32 ROT_LIMIT;   // pl_class.cpp (.sdata2), unused there
};

// Three-way motion blend (game/pl_class.cpp), 0xE8 bytes; `mot3` in player.cpp: the model's own
// motion (mot0) blended with mot1 (rate < 0) or mot2 (rate > 0) through MotionWork::blend.
class cMot3 {
public:
    cModel* model;       // 0x00
    f32 rate;            // 0x04  last move() rate, clamped to -1..1
    void* mot0;          // 0x08
    void* mot1;          // 0x0C
    void* mot2;          // 0x10
    int x14;             // 0x14  set() 7th argument: 1 = the blend work gets flags2 bit31
    MotionWorkSub work;  // 0x18  the blended motion (em.h)

    cMot3();
    // set(model, motion0, motion1, motion2, MotionSetCore 4th arg, u8 mode, int, u16, u16)
    void set(cModel* m, void* m0, void* m1, void* m2, int a, u8 b, int c, u16 d, u16 e);
    void set0(void* m, u8 a, int b);
    void move(f32 rate);
};

extern cMot3 mot3;      // game/player.cpp
extern f32 m3r[3];      // game/player.cpp  mot3 blend rates ([0] current, [1] target, [2] mix)
// m3r is an object with a constructor in the original (player.o's static initializer stores 0.0
// into the three rates); player.cpp defines it under this type, everyone else reads the f32[3].
class cMot3Rate {
public:
    f32 r[3];
    cMot3Rate() { r[0] = r[1] = r[2] = 0.0f; }
};
extern cMot3Rate m3rObj asm("m3r");

// Player (game/player.cpp, pl_*.cpp): a cEm with the player virtuals. Its fields are the cEm ones
// (all below 0xDE0, see em.h). Vtable order (pl_class.cpp): cUnit/cCoord/cModel/cEm virtuals, then
// the player ones below.
// pl_npc.h's `pSUB` under a second name: pl_leon.cpp declares its own `cModel* pSUB`, so player.h
// cannot declare the real one (cPlayer::subCharLiveCheck reads it).
extern cEm* pSubEm asm("pSUB");

// In-class bodies below are the ones the original emits after ~cPlayer at the end of pl_class.o
// (in-class inline members of the class whose vtable the unit owns); other units drop their
// linkonce copies (fold_linkonce). Add none that pl_class's target lacks.
class cPlayer : public cEm {
public:
    cPlayer();
    virtual ~cPlayer() {}
    // The original cUnit::beginEvent/endEvent take an int (KNOWN DEBT, cManager.h); cPlayer's
    // read it from r4 (pl_class.cpp).
    virtual void beginEvent();
    virtual void endEvent();
    virtual void move();
    virtual void setNoSuspend(int on);
    virtual int checkXbutton() { return 0; }
    virtual void setModel() = 0;
    virtual void setMotion() {}
    virtual void setRightHand(int no) = 0;
    virtual void setLeftHand(u32 no) = 0;
    virtual void setFace(int no) = 0;
    virtual void setHead(int no) {}
    virtual void setHead(void* bin, void* tpl) {}
    virtual void setWound() {}
    virtual void moveMatCalcBefore() {}
    virtual void initCloth() {}
    virtual void moveCloth() {}
    // Partner (id 3) dead while the player is in routine 0: routine 6 (die), damage info 0x80.
    // Inline, but defined in pl_class.cpp: player.cpp's move() calls it out of line.
    void subCharLiveCheck();

    // game/player.cpp
    void init0();
    void init1();
    void startUp();
    // game/pl_class.cpp
    // m0/seq0 when dmMotCk(), else m1/seq1. The overload hides cModel::motionSet: keep it reachable.
    void motionSet(void* m0, void* seq0, void* m1, void* seq1, int hokan, int frame);
    void motionSet(void* data, int a, int b, int c, int d) asm("motionSet__6cModelPviiii");
    int actionSelect();  // routine 1 selection from the keys / action buttons; returns checkXbutton()
    void dmgCheck();     // DmgMgr areas -> setDamage
    void visibleCtrl();  // alpha fade with pG->flags_500C bit13
    void seqSeCtrl();    // motion sequence sound (seNo) -> SndCall
    void keyConfig();
    void keyConfigTypeA();
    void endEvent0(u32 mode);   // 0: to routine 0/1 idle, 1: flags_41C bit8, 2: routine 0
    void beginAction();
    void endAction(int routine);
    void setSlow(f32 rate);
    void moveEye();
    void moveEyeNormal();
    void moveEyeMotion();
    void setLaserSight(int draw, int noCalc);
    void moveBinocular();
    void shadowCtrl();
    int keyReload();
    void setFootwork();
    void beginDamage();
    void endDamage();
    int endCamera();
    int isKamae();       // aiming (weapon routine ready/fire states, or the aim key held)
    int actCheck();      // 1 when the player may take an action button (act_btn checkPLStatus)
    void interrupt();
    void checkCtrl();    // Key 0x400/0x100000 -> pG->flags_500C bits
    int subScrCheck();   // 1 when the sub screen may open (sscrn SubScreenCall)
    int checkEvent();    // 1 when the event routine is ready (sscrn OpeSetOpenTerm)
    int getLifeLevel();  // 0 fine, 1 caution, 2 danger (cockpit meter colours)

    static const f32 SPEED_WALK_TURN;   // pl_class.cpp (.sdata2)
    static const f32 SPEED_RUN_TURN;
    // game/pl_debug.cpp
    void debugInit();
    void debugMove();
    void emSearch();
    // game/pl_wep.cpp
    void weaponRelease();
    void weaponLoad(int no, int type);  // stores pG 0x4FB0/0x4FB1, then ReadWepData
    void weaponInit();
    // game/pl_class.cpp: scenario damage area hit (sce_at sceAtFunc_damage)
    void setDamage(u8 kind, int arg, f32 power, int a, int b);
};

// Leon (game/pl_leon.cpp): the player model set for the main character.
class cPlLeon : public cPlayer {
public:
    cPlLeon();
    virtual ~cPlLeon() {}
    virtual void move();
    virtual int checkXbutton();
    virtual void setModel();
    virtual void setMotion();
    virtual void setRightHand(int no);
    virtual void setLeftHand(u32 no);
    virtual void setFace(int no);
    virtual void setHead(int no);
    virtual void setHead(void* bin, void* tpl);
    virtual void setWound();
    // In-class on purpose: the original emits these after the destructor at the end of the unit
    // (in-class inline members of the class whose vtable the unit owns), not in source order.
    virtual void initCloth()
    {
        if (pG->costume != 2) {
            PlClothSetLeon(this, &leonHair, &leonJacket, &leonHolster);
        }
    }
    virtual void moveCloth()
    {
        if (pG->costume != 2) {
            PlClothMoveLeon(this, &leonHair, &leonJacket, &leonHolster);
        }
    }
};

// Ashley (game/pl_ashley.cpp): the partner character's model set and bust motion.
class cPlAshley : public cPlayer {
public:
    cPlAshley();
    virtual ~cPlAshley() {}
    virtual void move();
    virtual void setModel();
    virtual void setRightHand(int no);
    virtual void setLeftHand(u32 no);
    virtual void setFace(int no);
    virtual void moveMatCalcBefore();
    virtual void initCloth() { PlClothSetGirl(this, &girlHair, &girlSkirt, &girlSweater, 0); }
    virtual void moveCloth() { PlClothMoveGirl(this, &girlHair, &girlSkirt, &girlSweater); }
    void moveBust();
};

void pl01weaponSet(cPlayer* pl);  // game/pl_ashley.cpp: fills pMotTbl from the player archive

// Debug cheat ("maho") command table (game/pl_debug.cpp), 0x16C bytes, `new`ed by cPlayer::debugInit.
struct PlMahoEntry {
    u8 x0;               // 0x00
    u8 x1;               // 0x01
    void (*func)();      // 0x04
    const char* code;    // 0x08  button sequence string
};

class cPlMaho {
public:
    PlMahoEntry tbl[30]; // 0x000
    u32 num;             // 0x168

    cPlMaho();
    void reset();
    void regist(const char* code, void (*func)());
};

extern cPlMaho* pMaho;   // game/player.cpp
extern u8 PlKaiou;       // game/player.cpp  kaiouken level (0..2)
extern u8 PlDbFlag;      // game/player.cpp  bit1: draw the player position marker
extern void* PlWepMot[3];  // game/player.cpp  weapon motion data

void PlWepMotSet(int no);
void DrawGage(int x, int y, int h, int w, int now, int max, int color);

// game/pl_event.cpp: routine 0 (event) and its sub-routines (index cModel::xFD)
void Pl_R0_Event(cPlayer* pl);
void pl_R1_Event_Normal(cPlayer* pl);
void pl_R1_Event_ToWalk(cPlayer* pl);
void pl_R1_Event_Smooth(cPlayer* pl);

extern cPlayer* pPL;

// game/player.cpp
extern Vec PlFancePos;    // point behind the fence / window the player climbs to (pl_class windowCheck)
extern int PlFanceFlag;   // 1 while a fence / window action runs
// game/pl_class.cpp
extern int PlKeyReloadType;   // reload key layout (cPlayer::keyReload)
extern int PlReloadDirect;
extern const f32 PlReloadSpeedTbl[45][3];  // per weapon: reload motion speed by level
extern const f32 PlReloadEndTbl[45][3];    // per weapon: reload end frame by level
extern const f32 PlShotFrameTbl[45][5];    // per weapon: shot frame by level

// game/pl_sub.cpp: control helpers
int joyFireOn();
int joyFireTrg();
int joyKamae();
int joyLKamae();

// game/pl_class.cpp
int dmMotCk();

// game/pl_knife.cpp: routine 2 (knife) and its sub-routines (index cModel::xFE / xFF)
void PlKnifeMove(cPlayer* pl);
void knife_r2_ready(cPlayer* pl);
void knife_r2_set(cPlayer* pl);
void knife_r2_fire(cPlayer* pl);
void knife_r2_down(cPlayer* pl);
void setWepTrans(cPlayer* pl, int on);

// (u32) view of pG->wep_no/wep_type (`(G_WEP_ID & 0xFFFF0000) == 0x0D020000`: rocket launcher)
#define G_WEP_ID (*(u32*) &pG->wep_no)
// (u32) view of pG->stage_no/room_no
#define G_ROOM_ID32 (*(u32*) &pG->stage_no)

#endif
