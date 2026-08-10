#ifndef EMMINE_H
#define EMMINE_H

#include "types.h"
#include "vec.h"
#include "em.h"

// Work of the mine enemy (game/emmine.cpp), overlaid on cEm from 0x3E0.
struct EmMineWork {
    u32 flags;            // 0x000 (0x3E0)
    int timer;            // 0x004 (0x3E4)  frames until the next beep (R1_Set / R1_Parent), bomb wait
    int count;            // 0x008 (0x3E8)  beep interval, shrinks from 17 to 5
    cEm* pParent;         // 0x00C (0x3EC)  enemy the mine sticks to (setParent)
    int x10;              // 0x010 (0x3F0)
    cEm* pTarget;         // 0x014 (0x3F4)  homing target (emMineSearchEm)
    int partsNo;          // 0x018 (0x3F8)  parts of pParent the mine sticks to
    int life;             // 0x01C (0x3FC)  frames until the mine explodes / the arrow is lost
    int searchWait;       // 0x020 (0x400)  frames between homing target searches
    Vec spd;              // 0x024 (0x404)  flight speed
    Vec pts[3];           // 0x030 (0x410)  fall: speeds of the three rope nodes
    f32 grav;             // 0x054 (0x434)  fall: gravity per frame (15)
    Vec hitNrm;           // 0x058 (0x438)  normal of the surface the mine hit
    u8 hitFlag;           // 0x064 (0x444)  1: stuck to the scenario (the bomb goes off 1000 along hitNrm)
    u8 wepLv;             // 0x065 (0x445)  pG->wep_lv at creation (blast radius of BombWait2)
    u8 waterSnd;          // 0x066 (0x446)  fall: water sound played
    u8 effKind;           // 0x067 (0x447)  EstSet id / type of the explosion (from the AtEffInfo hit)
    u8 effNo;             // 0x068 (0x448)
    u8 snd0;              // 0x069 (0x449)  SndCall block / number of the explosion
    u8 snd1;              // 0x06A (0x44A)
    u8 espKind;           // 0x06B (0x44B)  EspPullCoreKind at creation (trail effect owner)
};

#define EMMINE_WK(em) ((EmMineWork*) &(em)->x3E0)

// Mine / arrow enemy (game/emmine.cpp): the mine thrower's mine (type 0 / 1 homing) and the
// crossbow arrow (type 2). Flies (R1_Shot / R1_ShotArrow), sticks to the scenario or an enemy
// (R1_Set / R1_Parent), explodes (setBomb -> BombWait / BombWait2) or falls as a rope (R1_Fall).
class cEmMine : public cEm {
public:
    virtual void beginEvent();
    virtual void move();

    void setParent(cEm* parent, int partsNo);
    void setLost();
    void setBomb();
    void setFall();
};

// bin/tpl of the model, start position, initial speed (NULL: a random forward throw), type.
cEmMine* SetMine(void* bin, void* tpl, Vec* pos, Vec* spd, int type);

extern "C" {
void emMineDmCk(cEmMine* em);
void emMine_R0_Init(cEmMine* em);
void emMine_R0_Move(cEmMine* em);
void emMine_R1_Shot(cEmMine* em);
void emMine_R1_ShotArrow(cEmMine* em);
void emMineSearchEm(cEmMine* em, int mode);
void emMineHomingEm(cEmMine* em);
void emMine_R1_Set(cEmMine* em);
void emMine_R1_SetWater(cEmMine* em);
void emMine_R1_Parent(cEmMine* em);
void emMine_R1_BombWait(cEmMine* em);
void emMine_R1_BombWait2(cEmMine* em);
void emMine_R1_Fall(cEmMine* em);
void emMine_R1_Lost(cEmMine* em);
int emMineHitCk(cEmMine* em);
}

#endif
