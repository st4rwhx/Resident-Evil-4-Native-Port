#ifndef EMRACK_H
#define EMRACK_H

#include "types.h"
#include "vec.h"
#include "em.h"

class cSat;

// One extra yarare cube of the rack (YarareAddCube target); 0x34 bytes per entry.
struct EmRackHit {
    EmHitInfo info;       // 0x00
};

// Work of the rack enemy (game/emrack.cpp), overlaid on cEm from 0x3E0.
struct EmRackWork {
    u32 flags;            // 0x000 (0x3E0)
    int shockTimer;       // 0x004 (0x3E4)  frames the rack shakes (emRack_R1_Shock)
    f32 downSpd;          // 0x008 (0x3E8)  fall rotation speed (emRack_R1_Down)
    Vec size;             // 0x00C (0x3EC)  yarare box size
    EmRackHit hit[4];     // 0x018 (0x3F8)  extra yarare cubes of type 1
    f32 xE8;              // 0x0E8 (0x4C8)  shotgun hits left before the shock (1.0 for type 1)
    u32 xEC;              // 0x0EC (0x4CC)
    cSat* sat[3];         // 0x0F0 (0x4D0)  runtime collision pieces (emRackSatSet)
    u8 eff;               // 0x0FC (0x4DC)  setEff: effect owner id, 0xFF = none
    u8 etcNo;             // 0x0FD (0x4DD)  etc flag index (broken flag)
};
// The push range (matrix, inverse, 4 limits, flags) sits at cEm+0xD60 .. 0xDD0 and is addressed
// through `this`: cEm::rackMat / rackInvMat / rackRange / rackFlags.

#define EMRACK_WK(em) ((EmRackWork*) &(em)->x3E0)

extern "C" {
cEmRack* SetRack(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type, int etcNo);
void emRackDmCk(cEmRack* em);
void emRack_R0_Init(cEmRack* em);
void emRack_R0_Move(cEmRack* em);
void emRack_R1_Set(cEmRack* em);
void emRack_R1_Down(cEmRack* em);
void emRack_R1_Break(cEmRack* em);
void emRack_R1_Shock(cEmRack* em);
void emRackSatSet(cEmRack* em);
void emRackSatClear(cEmRack* em);
void emRackYarareInit(cEmRack* em);
}

#endif
