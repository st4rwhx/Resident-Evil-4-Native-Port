#ifndef PL_NPC_H
#define PL_NPC_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cEmWindow;

// Partner character base (game/pl_npc.cpp, `pSUB`): a cEm with the partner virtuals. Vtable order
// from pl_npc's `cSubChar virtual table`: cUnit/cCoord/cModel/cEm virtuals, then the ones below.
// The cSubChar-only fields (sub*) live in cEm (they sit below 0xDE0, see em.h).
class cSubChar : public cEm {
public:
    cSubChar();
    virtual ~cSubChar();
    virtual void beginEvent();
    virtual void endEvent();
    virtual void move();
    virtual void modelSet() = 0;   // slot 9: pure here (`cSubAshley::modelSet` in pl11); pl_sub EndSubDamage calls it for id 4
    virtual void setFace(int no);
    virtual void setHand(int no);
    virtual void initCloth();
    virtual void moveCloth();
    virtual void setEmFunc();           // pl_sub SetSubDamage (Ashley)

    static const Vec atckPos;    // offset behind the player while he aims (moveBehind)
    static const Vec atckPos2;   // the same for the two-handed weapons

    int mot_ck();                // 1 while the partner's life is at or below half
    void init();
    void moveCore();
    void moveFootwork();
    void moveMove();
    int readyOkCheck();
    void moveBehind();
    void moveKagamu();
    void movePants();
    void moveDown();
    int getScrActionPoint(Vec* opos, Vec* orot, u32 attr);
    void moveFance();
    int landCheck();
    void moveFall();
    void moveAction();
    void moveLadder();
    f32 getJumpAdjY();
    void jumpAdjust();
    void moveBack();
    void moveAux();
    void moveHide();
    void moveStoop();
    void moveFallWait();
    void moveLadderWait();
    void moveWindowWait();
    u32 checkSatAttr(f32 len);
    f32 getAdjustX(int n);
    void moveDamage();
    void moveDie();
    void moveBull();
    void moveEvent();
    void moveDijection();
    void movePos(Vec* target, f32 spd);
    void neckInit();
    void neckCtrl();
    void neckSet(Vec* pos);
    int actCheck();
    int cautionCheck();
    int plDownCheck();
    int fanceCheck();
    int windowCheck();
    int fallLadderCheck();
    int doorCheck();
    int readyCheck();
    int actionCheck();
    int ladder2Check();
    f32 getCliffHeight(f32 ang);
    void pantsCheck();
    int ckPlRun();
    void seqSeCtrl();
    void backCheckSet(void* mot);
    void backCheckMove();
    void backCheckCtrlFootwork();
    void backCheckCtrlMove();
    int checkBackEm();
    void analyze();
    void frontCheck();
    void anaSatInfo();
    void control(int mode);
    int checkAnotherRoute();
    int moveAnotherRoute();
    void damageCheck();
    // scenario damage area hit (sce_at sceAtFunc_damage)
    void setDamage(u8 kind, int arg, f32 power, int a, int b);
    void registPlAction(Vec* pos, f32 ang);
    void moveBust();
    void moveFace();
    void shadowCtrl();
    void dmgCheck();
    void beginDamage();
    void endDamage();
    void interrupt();
    void inSat();
    void debugMove();
    int farCheck();   // never called; dead-stripped in the DOL, only its pool word (1000) survives
};

extern cSubChar* pSUB;   // game/em.cpp

u32 SubCharGetStatus();  // game/pl_npc.cpp: routine bits for the camera / scenario (C++ linkage)

extern "C" {
// game/pl_npc.cpp: partner condition bits for the HUD (cockpit: 1, 2, 8, 0x10, 0x24)
u32 SubCharGetCondition();
int SubCharHideCheck();
}

#endif
