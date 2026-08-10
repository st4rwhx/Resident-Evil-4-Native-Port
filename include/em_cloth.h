#ifndef EM_CLOTH_H
#define EM_CLOTH_H

#include "types.h"
#include "pl_cloth.h"

class cModel;

// game/em_cloth.cpp: cloth / hair chain set-up of the enemy costume models (obj18 selects them
// by type) and the em2b short rope chain object.
extern "C" {
void Em34ClothSet1(cModel* m, PlCloth* c);
void Em34ClothMove1(cModel* m, PlCloth* c);
void Em34ClothReset(cModel* m);
void Em34ClothSet2(cModel* m, PlCloth* c);
void Em34ClothMove2(cModel* m, PlCloth* c);
void Em18ClothSet(cModel* m, PlCloth* c, int a);
void Em18ClothMove(cModel* m, PlCloth* c);
void Em37HairSet(cModel* m, PlCloth* c);
void Em37HairMove(cModel* m, PlCloth* c);
void Em37ClothReset(cModel* m);
void Em37CoatSet(cModel* m, PlCloth* c);
void Em37CoatMove(cModel* m, PlCloth* c);
void Em33ClothSet(cModel* m, PlCloth* c, int small);
void Em33ClothMove(cModel* m, PlCloth* c);
void Em33ClothSet2(cModel* m, PlCloth* c, int small);
void Em33ClothMove2(cModel* m, PlCloth* c);
void Em33ClothReset(cModel* m);
cObjChain* Em2bShortRopeSet(cModel* m, PlCloth* c, void* bin, void* tpl);
void Em30ClothSet1(cModel* m, PlCloth* c);
void Em30ClothMove1(cModel* m, PlCloth* c);
void Em30ClothSet2(cModel* m, PlCloth* c);
void Em30ClothMove2(cModel* m, PlCloth* c);
void Em30ClothReset(cModel* m);
}

#endif
