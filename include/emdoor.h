#ifndef EMDOOR_H
#define EMDOOR_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cSat;
class cObj12;
class cEmDoor;

// Work of the door enemy (game/emdoor.cpp), overlaid on cEm from 0x3E0.
struct EmDoorWork {
    u32 flags;            // 0x000 (0x3E0)  bit0: locked (setOpenLock / setCloseLock, setNormal clears), bit1: setLock strong mode (lock hp drops by 4 per hit)
    int timer;            // 0x004 (0x3E4)  shake frames (R1_Open / Close / Shock / OpenLock)
    int kickCnt;          // 0x008 (0x3E8)  R1_Open / Down: enemies still to be hit by the opening door
    f32 spd;              // 0x00C (0x3EC)  R1_Down: fall rotation speed
    u8 pad_10[0x10];
    f32 height;           // 0x020 (0x400)  door height (2300, type 6: 4400)
    f32 width;            // 0x024 (0x404)  half width (650): the door spans -2 * width .. 0 along its local x
    EmHitInfo hit[16];    // 0x028 (0x408)  [0..2] door boxes, [3..10] the breakable panes (parts 2..9), [11] right lock, [12] left lock, [13..15] chain
    u32 x368;             // 0x368 (0x748)
    u32 rnd;              // 0x36C (0x74C)  Rnd() % 5 at creation
    u32 x370;             // 0x370 (0x750)
    f32 dmg;              // 0x374 (0x754)  pane hits left before the next pane breaks
    int chainHp[3];       // 0x378 (0x758)
    int lockHpL;          // 0x384 (0x764)
    int lockHpR;          // 0x388 (0x768)
    f32 bendL;            // 0x38C (0x76C)  lock parts 1 rot.x, decays by 0.7 per frame
    f32 bendR;            // 0x390 (0x770)
    f32 bendChain;        // 0x394 (0x774)
    f32 rotY;             // 0x398 (0x778)  rot.y of the closed door
    Mtx mat;              // 0x39C (0x77C)  closed door -> world (rot.y, pos, shifted by -width)
    Mtx inv;              // 0x3CC (0x7AC)  world -> closed door
    u32 keyNo;            // 0x3FC (0x7DC)  pG->door_unlock bit (setKey), 0x36 = none
    int x400;             // 0x400 (0x7E0)  0x96 while opening
    Vec target;           // 0x404 (0x7E4)  position of the one that opened the door (setOpen: the open direction)
    cSat* sat[6];         // 0x410 (0x7F0)  effect collision pieces: [1] door, [2] panel above, [3..5] the panels of type 4 / 5 ([0] unused)
    cObj12* pLockL;       // 0x428 (0x808)  left lock object (setLock side 1)
    cObj12* pLockR;       // 0x42C (0x80C)  right lock object (setLock side 0)
    cObj12* pChain;       // 0x430 (0x810)  chain object (setChain)
    cEmDoor* pDoor;       // 0x434 (0x814)  the other door of a double door (setDoor)
    u32 sndId;            // 0x438 (0x818)  SndCall id of the opening sound
    u8 eff;               // 0x43C (0x81C)  setEff: effect owner id, 0xFF = none
    u8 dir;               // 0x43D (0x81D)  open direction: 1 = rot.y - PI/2
    u8 seCancel;          // 0x43E (0x81E)  setSeCancel: no opening sound (the paired door plays it)
    u8 flagNo;            // 0x43F (0x81F)  etc flag number (bit0 broken, bit1 right lock broken, bit2 left lock broken, bit3..5 chains broken, bit6/7 fallen direction)
};

#define EMDOOR_WK(em) ((EmDoorWork*) &(em)->x3E0)

// Door enemy (game/emdoor.cpp): the room doors the player opens or kicks, with locks, chains and
// breakable panes.
class cEmDoor : public cEm {
public:
    virtual void move();   // key function: the vtable stays in this unit (cEmMgr::construct stores it)

    void setLock(void* bin, void* tpl, int side, int strong);
    int ckLock();
    void setChain(void* bin, void* tpl);
    void setEff(u8 eff);
    void setYarare();
    u32 ckOpen();           // 0 can be opened, 1 open / broken, 2 an object blocks it, 3 locked
    int ckKick(Vec* pos);
    void setOpen(Vec* pos, int a, int b, int c);
    void setOpen2(int a);
    void setShock(int a, Vec* pos, int b);
    void setBreak(Vec* pos);
    int ckObj();            // 0 when a cEm 0x45 object stands in the door
    void setOpenLock(int a);
    void setCloseLock(int a);
    void setClose();
    void setDowned(int dir);
    void setNormal();
    void setKey(int no);
    void setDoor(cEmDoor* other);
    void setSeCancel();
};

extern "C" {
cEmDoor* SetDoor(void* bin, void* tpl, Vec* pos, Vec* rot, int type, int flagNo);
void emDoorDmCkWood(cEmDoor* em);
void emDoorDmCkIron(cEmDoor* em);
void emDoorDmCkIron2(cEmDoor* em);
void emDoorDmCkIronDown(cEmDoor* em);
void emDoorSetDmgLock_L(cEmDoor* em, int mode);
void emDoorSetDmgLock_R(cEmDoor* em, int mode);
void emDoorSetDmgChain(cEmDoor* em, u32 no);
void emDoorSetDmgDoor(cEmDoor* em);
void emDoorSetBrkDoor(cEmDoor* em, Vec* pos);
int emDoorBrkCk(cEmDoor* em);
void emDoor_R0_Init(cEmDoor* em);
void emDoor_R0_Move(cEmDoor* em);
void emDoor_R1_Set(cEmDoor* em);
void emDoor_R1_Open(cEmDoor* em);
void emDoor_R1_Down(cEmDoor* em);
void emDoor_R1_Downed(cEmDoor* em);
void emDoor_R1_Close(cEmDoor* em);
void emDoor_R1_Break(cEmDoor* em);
void emDoor_R1_Shock(cEmDoor* em);
void emDoor_R1_OpenLock(cEmDoor* em);
void emDoor_R1_CloseLock(cEmDoor* em);
void emDoorLockBendMove(cEmDoor* em);
void emDoorSatSet(cEmDoor* em);
void emDoorSatClear(cEmDoor* em);
void emDoorLockMove(cEmDoor* em);
int emDoorDoorAutoCloseCk(cEmDoor* em);
void emDoorYarareInit(cEmDoor* em);
void emDoorActEvtCk(cEmDoor* em);
void emDoorAction(cEmDoor* em);
void emDoorAction2(cEmDoor* em);
void plemDoorKick(class cPlayer* pl);
void plemDoorOpen(class cPlayer* pl);
// The door in front of `m` that it may open (pl_npc cSubChar::doorCheck), NULL when none.
cEmDoor* DoorOpenCk(cModel* m);
void SubOpenDoorSet(cEmDoor* door);
void subDoorKick();
void emDoorDropWeapon(cEmDoor* em);
}

#endif
