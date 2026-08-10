#ifndef EM10_H
#define EM10_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "emhit.h"
#include "emwep.h"
#include "emshield.h"
#include "pendulum.h"
#include "pl_cloth.h"
#include "camera.h"
#include "obj.h"

// Shared Ganado enemy library (em10.cpp, D:/Bio4/Prog/em10.cpp): the same object is linked into the
// 16 Ganado modules em10..em17, em19..em1f, em20 (config/G4BE08/modules.py). The per-enemy files of
// each module (EmXXInit / EmXXSet / EmXXWeaponSet) fill the motion tables of the work.
//
// Work of the Ganado enemy, overlaid on cEm from 0x3E0 (em10_R0_Init prints its size: 0x818).
// Field names are the work-relative offsets; the comment gives the cEm offset.
struct Em10Work {
    u32 flags;            // 0x000 (0x3E0)
    int x4;               // 0x004 (0x3E4)  routine timer
    int x8;               // 0x008 (0x3E8)
    int xC;               // 0x00C (0x3EC)
    int x10;              // 0x010 (0x3F0)
    int x14;              // 0x014 (0x3F4)
    f32 x18;              // 0x018 (0x3F8)
    f32 x1C;              // 0x01C (0x3FC)
    int x20;              // 0x020 (0x400)
    Vec x24;              // 0x024 (0x404)
    void* mot[79];        // 0x030 (0x410)  motion data table (Em10Set / Em10WeaponSet fill it; [0x29..] weapons)
    cEmWep* pWep;         // 0x16C (0x54C)  weapon in hand
    cEmWep* pWep2;        // 0x170 (0x550)
    cEmShield* pShield;   // 0x174 (0x554)
    cObj* x178;           // 0x178 (0x558)
    cObj* x17C;           // 0x17C (0x55C)
    cEm* pHead;           // 0x180 (0x560)  lost head enemy
    cModelInfo* x184;     // 0x184 (0x564)  hand parts info (setHand(1))
    cModelInfo* x188;     // 0x188 (0x568)  hand parts info (setHand(0))
    cModelInfo* x18C;     // 0x18C (0x56C)  head parts info (em10HeadSet)
    cModelInfo* x190;     // 0x190 (0x570)  type 6: body parts info (em10ModelInit)
    cModelInfo* x194;     // 0x194 (0x574)  type 6: cloth parts info (em10ClothPartsSet)
    cModelInfo* x198;     // 0x198 (0x578)  type 6: goods parts info (em10GoodsPartsSet)
    cModelInfo* x19C;     // 0x19C (0x57C)  chainsaw Ganado: sack parts info (em10SackSet)
    cModel* x1A0;         // 0x1A0 (0x580)
    cModel* x1A4;         // 0x1A4 (0x584)
    cModel* x1A8;         // 0x1A8 (0x588)
    cModel* x1AC;         // 0x1AC (0x58C)
    cModel* x1B0;         // 0x1B0 (0x590)
    cModel* x1B4;         // 0x1B4 (0x594)
    cModel* x1B8;         // 0x1B8 (0x598)
    cModel* x1BC;         // 0x1BC (0x59C)
    cModel* x1C0;         // 0x1C0 (0x5A0)
    EmHitInfo hit[10];    // 0x1C4 (0x5A4)  extra hit boxes (YarareAdd in em10_R0_Init)
    Camera cam;           // 0x3CC (0x7AC)  takeaway camera (em10CamMoveTakeaway installs it as CamCtrl.x250)
    u8 x4C4;              // 0x4C4 (0x8A4)  chgSet value (cEm::x38D copy)
    u8 pad_4C5[3];
    Vec startPos;         // 0x4C8 (0x8A8)  pos at init
    f32 startRotY;        // 0x4D4 (0x8B4)  rot.y at init
    Vec x4D8;             // 0x4D8 (0x8B8)
    class cObjLadder* pLadder;  // 0x4E4 (0x8C4)  ladder being climbed / reset
    cModel* pSwitch;      // 0x4E8 (0x8C8)  setGotoSwitch: the switch object walked to
    Vec x4EC;             // 0x4EC (0x8CC)
    Vec x4F8;             // 0x4F8 (0x8D8)
    f32 x504;             // 0x504 (0x8E4)
    f32 x508;             // 0x508 (0x8E8)
    f32 x50C;             // 0x50C (0x8EC)
    f32 x510;             // 0x510 (0x8F0)
    f32 x514;             // 0x514 (0x8F4)
    f32 x518;             // 0x518 (0x8F8)
    f32 x51C;             // 0x51C (0x8FC)
    f32 x520;             // 0x520 (0x900)
    f32 x524;             // 0x524 (0x904)
    f32 x528;             // 0x528 (0x908)
    f32 x52C;             // 0x52C (0x90C)
    f32 x530;             // 0x530 (0x910)
    Vec x534;             // 0x534 (0x914)
    Vec x540;             // 0x540 (0x920)
    Vec x54C;             // 0x54C (0x92C)
    class cEmWindow* pWindow;  // 0x558 (0x938)  window the Ganado breaks (em10_R1_WindowAtk)
    cEm* pTruck;          // 0x55C (0x93C)  truck enemy model the Ganado drives (em10SearchTruck)
    class cObjGondola* pGondola;  // 0x560 (0x940)  em10GetGondola (room 10F)
    u32 x564;             // 0x564 (0x944)
    class cObjGatling* pGatling;  // 0x568 (0x948)
    u8 gatlingMode;       // 0x56C (0x94C)
    u8 pad_56D[3];
    class cCtrl* pDragon; // 0x570 (0x950)  GetCtrlDragon (room 222 dragon statues)
    class cObj16* pParasite;  // 0x574 (0x954)  parasite object (em10SetParasite)
    cEm* x578[5];         // 0x578 (0x958)
    class cEmPartner* x58C;  // 0x58C (0x96C)  partner enemy of another module (virtual slots only)
    cModel* x590;         // 0x590 (0x970)  belt chain object (em10BeltSet: cObjChain)
    cModel* x594;         // 0x594 (0x974)  chain object of the chain Ganado (em10ChainSet: cObjChain)
    Vec x598;             // 0x598 (0x978)
    Vec x5A4;             // 0x5A4 (0x984)
    class cCtrl* pCtrl12; // 0x5B0 (0x990)  GetCtrlCtrl12()
    class cCtrl* pCtrl11; // 0x5B4 (0x994)  GetCtrlCtrl11()
    u32 x5B8;             // 0x5B8 (0x998)
    u32 x5BC;             // 0x5BC (0x99C)
    u32 x5C0;             // 0x5C0 (0x9A0)
    u32 x5C4;             // 0x5C4 (0x9A4)
    f32 x5C8;             // 0x5C8 (0x9A8)
    f32 x5CC;             // 0x5CC (0x9AC)
    f32 x5D0;             // 0x5D0 (0x9B0)
    f32 x5D4;             // 0x5D4 (0x9B4)
    f32 x5D8;             // 0x5D8 (0x9B8)
    f32 x5DC;             // 0x5DC (0x9BC)
    Vec x5E0;             // 0x5E0 (0x9C0)
    u32 x5EC;             // 0x5EC (0x9CC)  ckGoto
    Vec x5F0;             // 0x5F0 (0x9D0)
    Vec scaleBase;        // 0x5FC (0x9DC)  scale at init
    Vec x608;             // 0x608 (0x9E8)
    void* evtMot[8];      // 0x614 (0x9F4)  event motions (setEvtMotion / setGondolaMotion / setDrill / setGatling)
    u32 x634;             // 0x634 (0xA14)
    u32 x638;             // 0x638 (0xA18)
    u32 x63C;             // 0x63C (0xA1C)
    s32 x640;             // 0x640 (0xA20)  ckBombFire / bowgun ammo timer
    s16 x644;             // 0x644 (0xA24)
    u16 x646;             // 0x646 (0xA26)
    u16 x648;             // 0x648 (0xA28)
    u8 pad_64A[2];
    f32 x64C;             // 0x64C (0xA2C)
    f32 x650;             // 0x650 (0xA30)
    u16 x654;             // 0x654 (0xA34)
    u16 x656;             // 0x656 (0xA36)
    u32 x658;             // 0x658 (0xA38)
    u16 x65C;             // 0x65C (0xA3C)
    s16 x65E;             // 0x65E (0xA3E)  em10CsawSignSe: chainsaw rev sound timer
    u16 x660;             // 0x660 (0xA40)
    u8 pad_662[2];
    u32 x664;             // 0x664 (0xA44)
    u16 x668;             // 0x668 (0xA48)
    u8 pad_66A[2];
    f32 x66C;             // 0x66C (0xA4C)
    u16 x670;             // 0x670 (0xA50)  checkThrow
    u8 pad_672[2];
    u32 x674;             // 0x674 (0xA54)
    u8 pad_678[2];
    u16 x67A;             // 0x67A (0xA5A)
    s16 x67C;             // 0x67C (0xA5C)  em10CatchSubRtnCk: lha
    u16 x67E;             // 0x67E (0xA5E)
    u16 x680;             // 0x680 (0xA60)
    s16 x682;             // 0x682 (0xA62)
    u16 x684;             // 0x684 (0xA64)
    u16 x686;             // 0x686 (0xA66)
    u32 sndId;            // 0x688 (0xA68)  SndCall handle (chainsaw)
    u32 x68C;             // 0x68C (0xA6C)
    u8 pad_690[4];
    u8 x694;              // 0x694 (0xA74)
    u8 pad_695;
    u8 x696;              // 0x696 (0xA76)
    u8 x697;              // 0x697 (0xA77)
    u8 x698;              // 0x698 (0xA78)
    u8 pad_699;
    u8 x69A;              // 0x69A (0xA7A)
    u8 x69B;              // 0x69B (0xA7B)
    u8 x69C;              // 0x69C (0xA7C)
    u8 x69D;              // 0x69D (0xA7D)
    u8 x69E;              // 0x69E (0xA7E)  effect kind of the enemy (EffectEsp*Delete)
    u8 x69F;              // 0x69F (0xA7F)
    u8 x6A0;              // 0x6A0 (0xA80)
    s8 x6A1;              // 0x6A1 (0xA81)  em10SetWaterEff: in-water splash interval
    s8 x6A2;              // 0x6A2 (0xA82)  em10SetWaterEff: wading ripple interval
    u8 x6A3;              // 0x6A3 (0xA83)
    u8 x6A4;              // 0x6A4 (0xA84)
    u8 x6A5;              // 0x6A5 (0xA85)
    u8 x6A6;              // 0x6A6 (0xA86)
    u8 pad_6A7;
    u32 x6A8;             // 0x6A8 (0xA88)
    u8 x6AC;              // 0x6AC (0xA8C)
    u8 x6AD;              // 0x6AD (0xA8D)
    u8 x6AE;              // 0x6AE (0xA8E)
    u8 x6AF;              // 0x6AF (0xA8F)
    u8 x6B0;              // 0x6B0 (0xA90)
    u8 x6B1;              // 0x6B1 (0xA91)
    u8 wepType;           // 0x6B2 (0xA92)  weapon in hand kind (4 chainsaw, 8 bowgun, 9 ...)
    u8 wep2Type;          // 0x6B3 (0xA93)
    u8 x6B4;              // 0x6B4 (0xA94)
    u8 x6B5;              // 0x6B5 (0xA95)
    u8 x6B6;              // 0x6B6 (0xA96)
    u8 x6B7;              // 0x6B7 (0xA97)  ckResetEnable
    u8 x6B8;              // 0x6B8 (0xA98)
    u8 x6B9;              // 0x6B9 (0xA99)
    u8 x6BA;              // 0x6BA (0xA9A)
    u8 x6BB;              // 0x6BB (0xA9B)
    s8 x6BC;              // 0x6BC (0xA9C)  frames the atari flag 8 stays set
    u8 x6BD;              // 0x6BD (0xA9D)
    u8 x6BE;              // 0x6BE (0xA9E)
    u8 x6BF;              // 0x6BF (0xA9F)
    u8 x6C0;              // 0x6C0 (0xAA0)
    u8 x6C1;              // 0x6C1 (0xAA1)
    u8 x6C2;              // 0x6C2 (0xAA2)
    u8 x6C3;              // 0x6C3 (0xAA3)
    u8 x6C4;              // 0x6C4 (0xAA4)
    u8 x6C5;              // 0x6C5 (0xAA5)
    // 0x6C6 .. 0x6D9: sound numbers (Em10SetSeTbl)
    u8 se6C6;             // 0x6C6 (0xAA6)
    u8 se6C7;             // 0x6C7 (0xAA7)
    u8 se6C8;             // 0x6C8 (0xAA8)
    u8 se6C9;             // 0x6C9 (0xAA9)
    u8 se6CA;             // 0x6CA (0xAAA)
    u8 se6CB;             // 0x6CB (0xAAB)
    u8 se6CC;             // 0x6CC (0xAAC)
    u8 se6CD;             // 0x6CD (0xAAD)
    u8 se6CE;             // 0x6CE (0xAAE)
    u8 se6CF;             // 0x6CF (0xAAF)
    u8 se6D0;             // 0x6D0 (0xAB0)
    u8 se6D1;             // 0x6D1 (0xAB1)
    u8 se6D2;             // 0x6D2 (0xAB2)
    u8 se6D3;             // 0x6D3 (0xAB3)
    u8 se6D4;             // 0x6D4 (0xAB4)
    u8 se6D5;             // 0x6D5 (0xAB5)
    u8 se6D6;             // 0x6D6 (0xAB6)
    u8 se6D7;             // 0x6D7 (0xAB7)
    u8 se6D8;             // 0x6D8 (0xAB8)
    u8 pad_6D9[3];
    PenCloth cloth;       // 0x6DC (0xABC)  Em18ClothSet / Em1fClothSet / em10ChainSet / em10BeltSet
    f32 blendRate;        // 0x73C (0xB1C)  em10BlendMotSet
    int x740;             // 0x740 (0xB20)  em10BlendMotSet: hokan frames left (low byte passed)
    u32 x744;             // 0x744 (0xB24)  em10BlendMotSet: start frame (low half passed)
    MotionWorkSub blendMot;  // 0x748 (0xB28)
};

#define EM10_WK(em) ((Em10Work*) &(em)->x3E0)

class cObjGatling;

// The enemy attached at Em10Work 0x58C lives in another module: only its virtual slots are known
// (docs/matching.md: a class with undefined virtuals emits no vtable). Slot names are the vtable byte offsets.
class cEmPartner : public cEm {
public:
    virtual int v50();
    virtual void v58(cEm* em, int a, int b, int c);
    virtual int v60();
    virtual void v68();
    virtual int v70();
    virtual void v78();
    virtual int v80();
    virtual void v88(Vec* pos, f32 range);
    virtual void v90(Vec* pos, f32 range, void* sw);
    virtual int v98(int a);
    virtual void setReset();      // 0xA0
    virtual void vA8(u8 no);
    virtual void vB0();
    virtual int vB8();
    virtual void vC0();
    virtual int vC8();
};

// The Ganado (em10.cpp). Vtable order after the cEm virtuals: the declaration order below.
class cEm10 : public cEm {
public:
    // no user constructor: em10.cpp has no cEm10::cEm10 body (the in-class `cEm10() {}` would be
    // emitted out of line like every in-class member), EmXXInit's `new (em) cEm10()` synthesizes it
    virtual ~cEm10();
    virtual void move();
    virtual void setNoSuspend(int on);
    virtual int checkThrow();
    virtual void setHand(int no, int type);
    virtual void setWeaponFall();
    virtual int ckFindPL();
    virtual void setFindPL();
    virtual void clearFindPL();
    virtual int ckParasite();
    virtual u32 ckGoto();
    virtual void setGoto(Vec* pos, int range);
    virtual void setGotoSwitch(cModel* sw, int near, Vec* pos);
    virtual int ckResetEnable();
    virtual void setReset();
    virtual void chgSet(u8 no);
    virtual void setEvtMotion(void* m0, void* m1, void* m2, void* m3);
    virtual void setGondolaMotion(void* m0, void* m1, void* m2, void* m3);
    virtual void setR11DMotion(void* m0);
    virtual void setDrill(void* m0, void* m1, void* m2, void* m3);
    virtual void setGatling(cObjGatling* g, void* m0, void* m1, void* m2, void* m3);
    virtual void setGatlingMode(u8 mode);
    virtual int ckBombFire();
    virtual int ckShiled();
    virtual int ckBowgunFire();
    virtual void setSwitch(cModel* sw);
    virtual void setLost();
    virtual void setWeapon(void* bin, void* tpl, int type);
    virtual int ckWeapon();
    virtual int ckTakeAway();
    virtual void setUFOCatch(void* m0, void* m1);
    virtual int ckR305BomberEnable();
};

typedef void (*Em10Func)(cEm10*);
typedef void (*PlEm10Func)(cPlayer*);

// .data+0: the per-enemy set function _prolog stores (EmXXSet), run by em10_R0_Init.
extern Em10Func Em10SetFunc;

// Object enemies the Ganados interact with (DOL units without a header of their own).
class cObj16 : public cObj {
public:
    int ckAtkEnable();
    void setDamage();
    void setAtk(u8 a);
    void clearLostWait();
    void setMotData(void* a, void* b, void* c, void* d, void* e, void* f, void* g, void* h, void* i, void* j, void* k);
    void setLostWait(int a);
    void setBurn();
    void setPlDmgMot(void* m, int a);
    void setDieEff();
    void setCritical();
    int ckAtkHit();
    void setScale(Vec* s);
};

class cObjGatling : public cObj {
public:
    void stopFire();
    void setRide(cEm* em);
    void setReload();
    void setFire();
    int ckReload();
    int ckBreak();
};

class cObjGondola : public cObj {
public:
    void setVib();
    void setGetOffEm(cEm* em);
    void setDamage();
    void setBreak();
    int ckRide();
};

class cObjLadder : public cObj {
public:
    void setDown2();
    void setResetReserve();
    void setReset(int a);
    void setClimb();
    int getType();
    int getStatus();
    int getLadderNum();
    int ckReset();
    int ckClimb();
};

class cObjBell : public cObj {
public:
    void setBreak();
    int ckBreakEnable();
    int ckBreak();
};

// game/obj14.cpp: creates the bell object (st2 r218).
cObj* SetObjBell(void* bin, void* tpl, Vec* pos, Vec* rot);

class cObjBull : public cObj {
public:
    int ckBullRide(Vec* pos, u8* a, Vec* b);
};

// Hanging object (game/obj12.cpp): the Ganado's sack / lantern hangs on it.
class cObj12 : public cObj {
public:
    void setParent(cModel* parent, int parts, int flag);
    void setFall(Vec* spd, u8 type);
    void setBurn();
};

cObj* SetObj12(void* bin, void* tpl, Vec* pos, Vec* rot);
cObj* SetObj01(void* bin, void* tpl, Vec* pos, Vec* rot, Vec* v, f32 a, f32 b, int c, int d);
// game/obj08.cpp: the thrown projectile object (em2d poison; em10 declares them locally).
cObj* SetObj08(cModel* parent, void* bin, void* tpl, Vec* pos, Vec* rot, int flags, void* atk);
void SetObj08Spd(cObj* obj, Vec* spd, int life, f32 grav, f32 rad);
void SetObj08Est(cObj* obj, int no0, int prm0, int no1, int prm1, int no2, int prm2, int no3, int prm3, u8 flag);
void SetObj08Se(cObj* obj, u16 blk, u16 no);
void Obj01SetEst(cObj* obj, int a, int b, u32 c, int d, int e, int f, int g, int h, int i);
int GetWepDmVal(cEm* em, u32 a, int b);
void EmCatchSubSet(cEm* em, cEm* sub, u32 type, int a, f32 x, f32 y, f32 z, f32 w);
extern "C" {
void MotSetObj16(cObj* obj, void* mot, int a, int b);
int GetEm10EyeEffectEnable();
}

#endif
