#ifndef EM_SUB_H
#define EM_SUB_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "emhit.h"

// game/em_sub.cpp: the shared enemy helper library (hit boxes, damage, blood effects, weapon
// target lists, catch motions, item drops). EmGetDmPos, EmDmBloodSet2, EmPlBloodSet2,
// EmAtkLineHitCk, EmAtkSetDamagePL, VehicleAdjust and PlSetDamage are declared in emhit.h.

// GetWepTargetList* entry: the enemy and the hit box that was hit.
struct WepTarget {
    cEm* em;
    EmHitInfo* part;
};

extern u32 No_drop_cnt;
extern u32 No_drop_cnt2;

extern "C" {
void Em_R0_Scenario(cEm* em);
void EmDmBloodSet(cEm* em);
void EmDmBloodSet3(cEm* em, int no, int prm, int rnd, int e, int f);
void EmPlBloodSet(cEm* em, Vec* pos, int type, int a, int b);
void EmSubBloodSet(cEm* em, Vec* pos, int type, int a, int b);
// Hit box of `em` touched by the capsule of the 8-corner `box`; the best one by squared distance.
EmHitInfo* emBoxAtCk(cEm* em, Vec* box, Vec* pos, int flag);
EmHitInfo* emLineAtCk(cEm* em, Vec* a, Vec* b, f32 len, int flag);
EmHitInfo* emLineAtCk2(cEm* em, Vec* a, Vec* b, f32 len, Vec* out, int flag);
int emLineCapsuleCrossCk(Vec* a, Vec* b, Vec* top, Vec* bottom, f32 r, Vec* hit);
int emLineCubeCrossCk(Vec* a, Vec* b, Mtx m, f32 sx, f32 sy, f32 sz, Vec* ofs, Vec* hit);
int emLinePolyCrossCk(Vec* a, Vec* b, Vec* poly, Vec* hit);
EmHitInfo* emSphereAtCk(cEm* em, Vec* pos, Vec* pos2, f32 r, int flag, f32 r2);
u32 GetWepTargetList(Vec* box, Vec* pos, WepTarget* list, u32 max, int flag);
u32 GetWepTargetList2(Vec* p0, Vec* p1, WepTarget* list, u32 max, Vec* hit, Vec* nrm, u32* attr, int type,
                      int flag);
int GetWepTargetListBomb(Vec* pos, f32 r, WepTarget* list, int max, int type, int flag);
int PlBombHitCk(Vec* pos, f32 r);
int GetWepTargetPos(Vec* p0, Vec* p1, int plCheck, int wepNo, cEm** outEm, int* outAttr);
EmHitInfo* EmYarareContactCk(cEm* em, Vec* pos, Vec* out, f32 r);
void EmYarareDisp(cEm* em);
void EmScenario(cEm* em);
int LifeDownSet(cEm* em, int dmg, int flag);
int LifeDownSet2(cEm* em, int dmg, int rnd, int flag);
int EmAtkHitCk(EmAtkInfo* info, Vec* a, Vec* b, int noSub);
int EmAtkHitCk2(EmAtkInfo* info, Vec* a, Vec* b);
EmHitInfo* EmAtkLineHitCkSub(Vec* a, Vec* b, Vec* hit, Vec* nrm);
void EmAtkSetDamageSub(EmHitInfo* part, EmAtkInfo* info, Vec* a, Vec* b);
EmHitInfo* EmAtkHitSubCk2(EmAtkInfo* info, Vec* a, Vec* b);
void EmCatchPLSet(cEm* em, f32 ang, u32 type, int a, f32 x, f32 y, f32 z);
int EmCatchMotionMove(cEm* em, f32 rate, f32 rate2);
int EmRackCk(cEm* em, Vec* pos, f32 ang);
int GetBulletPoint();
void GetDropBullet(int* id, int* num);
int GetRecoveryPoint();
void EmSetDropItem(cEm* em);
void EmReserveDropItem(cEm* em);
void RandomItemSet(cEm* em);
int RandomItemCk(int id, int* outId, int* outNum, int flag);
int CheckInWater(cModel* m, int parts);
int HandgunCk(int wep);
int TrolleyItemSetCk(Vec* pos, u16 id, int num);
int BullItemSetCk(Vec* pos, u16 id, int num);
void adjust_add_set(Vec* v);
}

// Position of `em` (the player when NULL) plus `t` of its parts 0 movement this frame (C++ linkage;
// Bio4.sym marks it local but the em3c module calls it).
void GetPlPos(Vec* out, cEm* em, f32 t);

#endif
