#ifndef EM3D_H
#define EM3D_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "em3a.h"

// Work of the em3d enemy (em3d module, D:/Bio4/Prog/em3d.cpp; the support helicopter of the
// island: flies a fixed table of positions, fires its chain guns at the player's targets and its
// rockets at the enemies it finds), overlaid on cEm from 0x3E0. The vtable adds the room interface
// (setTarget / setTargetPos / setPatrolPos / ckMissileFire / setEmLocked / setFreeFire).
struct Em3dWork {
    u32 flags;            // 0x000 (0x3E0)  bit0: aimed at the target, bit1: select enable, bit2: rocket ready,
                          //                bit3: an enemy locked on it (per frame), bit4: target set (per frame),
                          //                bit5: free fire, bit6: patrolling (per frame)
    int timer;            // 0x004 (0x3E4)
    int count;            // 0x008 (0x3E8)  Patrol: hover frames; Atk: enemies found
    u8 pad_C[4];
    int mesDone;          // 0x010 (0x3F0)  Atk: the near-player message was given
    u8 pad_14[0x26C - 0x14];
    Vec vibAng;           // 0x26C (0x64C)  hover vibration phases (em3dVibMove)
    Vec vibSpd;           // 0x278 (0x658)  their per-frame increments
    Vec spd;              // 0x284 (0x664)  movement speed
    u8 pad_290[0x2A8 - 0x290];
    Vec patrolPos;        // 0x2A8 (0x688)  setPatrolPos
    u32 targetNo;         // 0x2B4 (0x694)  index into the position / target tables (0..7)
    u8 targetSet;         // 0x2B8 (0x698)  setTarget / setTargetPos: leave the patrol
    u8 pad_2B9[3];
    int x2BC;             // 0x2BC (0x69C)  (ctor: 150)
    cEm* pTargetEm;       // 0x2C0 (0x6A0)  enemy the rockets aim at (em3dGetTargetEm)
    u32 sndId;            // 0x2C4 (0x6A4)  voice handle (SndStop before the next one)
    int mesTimer;         // 0x2C8 (0x6A8)  frames the radio message is held
    f32 range;            // 0x2CC (0x6AC)  enemy search range (setTarget)
    cObj* pMissile[4];    // 0x2D0 (0x6B0)  the rockets hung on parts 0xC..0xF
    u8 pad_2E0;
    u8 gunTimer;          // 0x2E1 (0x6C1)  frames between chain gun shots
};

#define EM3D_WK(em) ((Em3dWork*) &(em)->x3E0)

class cEm3d : public cEm {
public:
    virtual void move();
    virtual void setNoSuspend(int on);
    virtual int ckSelectEnable();
    virtual void setTarget(u32 no, f32 range);
    virtual void setTargetPos(u32 no, Vec* pos, f32 rotY, f32 range);
    virtual void setPatrolPos(Vec* pos);
    virtual int ckMissileFire();
    virtual void setEmLocked();
    virtual void setFreeFire();
};

void Em3dInit(cEm* em);
void em3dDmCk(cEm3d* em);
void em3dRoterMove(cEm3d* em);
void em3dChainGunMove(cEm3d* em);
void em3dHeliPitchMove(cEm3d* em);
void em3dVibMove(cEm3d* em);
int em3dGetTargetEm(cEm3d* em);
void em3dTargetEmUpdate(cEm3d* em);
void em3dRocketFire(cEm3d* em);

#endif
