#ifndef PL_CLOTH_H
#define PL_CLOTH_H

#include "types.h"
#include "vec.h"

class cModel;

// Collision volume a cloth chain avoids (pl_cloth.cpp `*At` tables), 0x24 bytes: attached to a
// model part, sphere (p1 zero) or capsule between p0 and p1.
struct PlClothAt {
    u16 x0;          // 0x00
    u8 parts0;       // 0x02
    u8 parts1;       // 0x03
    f32 rate;        // 0x04
    f32 r;           // 0x08  radius
    Vec p0;          // 0x0C
    Vec p1;          // 0x18
};

// Cloth / pendulum chain work of one accessory (game/pl_cloth.cpp, game/pendulum.cpp), 0x60 bytes.
// pendulum.h's PenCloth is the same object with the obj units' field names.
struct PlCloth {
    int num;             // 0x00  number of chain links
    u8* pParts;          // 0x04  model parts index per link
    u8* pLeft;           // 0x08  left neighbour per link (0xFF = none)
    u8* pRight;          // 0x0C  right neighbour
    u8* pUpLeft;         // 0x10  (Ada dress)
    u32 x14;             // 0x14
    u8* pUp;             // 0x18  upper neighbour per link
    u8* pDown;           // 0x1C  lower neighbour per link
    u32 x20;             // 0x20
    f32* pRate;          // 0x24  per-link rate (em_cloth: em18ClothRate, em37HairRate, ...)
    f32* pMax;           // 0x28  max swing per link
    f32* pWindS;         // 0x2C  wind phase per link
    f32* pWindR;         // 0x30  wind rate per link
    PlClothAt* pAt;      // 0x34  collision volumes
    int nAt;             // 0x38
    f32 x3C;             // 0x3C  link length
    f32 x40;             // 0x40
    int x44;             // 0x44
    f32 x48;             // 0x48
    f32 x4C;             // 0x4C
    f32 x50;             // 0x50  gravity / stiffness rate (skirt: 0.9 under water, 0.5 otherwise)
    u32 x54;             // 0x54
    cModel* pModel;      // 0x58  (AdaRibbonSet)
    u32 flags;           // 0x5C  0x100 / 0x200 / 0x302
};

// The player units pass these in this order; PlClothSet*/Move* use them as (jacket, holster, hair)
// and (skirt, hair, sweater) respectively (the original naming does not match the use).
extern PlCloth leonHair;
extern PlCloth leonJacket;
extern PlCloth leonHolster;
extern PlCloth girlHair;
extern PlCloth girlSkirt;
extern PlCloth girlSweater;

extern PlCloth luisHair;
extern PlCloth adaDress;
extern PlCloth adaHair;
extern PlCloth adaRibbon;

// Chain object (game/obj1d.cpp): really a cObj subclass, kept opaque here (see pl_wep.h cObjWep).
// SetChain returns the cObj*; the player units only hand it back to the chain members.
struct PenCloth;
class cObjChain {
public:
    void setChain(PenCloth* c);
    void setParent(cModel* parent, int parts, Vec* ofs, int flag);
    void setParent2(cModel* parent, int parts1, Vec* ofs1, int parts2, Vec* ofs2, int flag);
};
cObjChain* SetChain(void* bin, void* tpl, Vec* pos, Vec* rot);

void PlClothSetLeon(cModel* pl, PlCloth* jacket, PlCloth* holster, PlCloth* hair);
void PlClothMoveLeon(cModel* pl, PlCloth* jacket, PlCloth* holster, PlCloth* hair);
void PlClothSetGirl(cModel* pl, PlCloth* skirt, PlCloth* hair, PlCloth* sweater, int evt);
void PlClothMoveGirl(cModel* pl, PlCloth* skirt, PlCloth* hair, PlCloth* sweater);
void PlClothSetLuis(cModel* pl, PlCloth* hair);
void PlClothMoveLuis(cModel* pl, PlCloth* hair);
void PlClothSetAda(cModel* pl, PlCloth* ribbon, PlCloth* dress, PlCloth* hair, int evt);
void PlClothMoveAda(cModel* pl, PlCloth* ribbon, PlCloth* dress, PlCloth* hair);
cObjChain* AdaRibbonSet(cModel* pl, PlCloth* ribbon, void* bin, void* tpl);

#endif
