#ifndef PL_BODY_H
#define PL_BODY_H

#include "types.h"
#include "vec.h"
#include "model.h"
#include "math_sub.h"

// One SPAE (spline attack effect) record built by cPlBody::makeSpaeData (0x58 bytes).
struct SpaeData {
    u32 id;         // 0x00  0x101
    u32 type;       // 0x04  2
    u32 x08;        // 0x08  0x18
    u16 x0C;        // 0x0C  0
    u16 x0E;        // 0x0E  2
    u32 x10;        // 0x10  0x38
    u16 x14;        // 0x14  1
    u16 x16;        // 0x16  2
    u32 x18;        // 0x18  0
    Vec scale;      // 0x1C  (1, 0, 0)
    u32 x28;        // 0x28  0x100
    Vec x2C;        // 0x2C  (0, 0, 0)
    u32 x38;        // 0x38  0
    Vec x3C;        // 0x3C  (0, 0, 0)
    u32 x48;        // 0x48  0x100
    Vec x4C;        // 0x4C  (1, 0, 0)
};

// Model part as seen by pl_body (cModel::getPartsPtr result): the waist twist writes its rotation.
struct PlBodyParts {
    u8 pad_0[0x128];
    Vec rot;        // 0x128
    u8 pad_134[0x1C0 - 0x134];
    u32 flags;      // 0x1C0  bit30: rotation override
};

// Player body helper (game/pl_body.cpp, `new`ed by cPlayer::init0 into cEm::pBody at 0x794, 0xF0
// bytes): the extra model infos hung off the player (hands, head, face, hair) and the data pointers
// they were built from (pl_leon setModel/setRightHand/...), the waist twist and the weapon hand.
class cPlBody {
public:
    void* pRightData;            // 0x00  right hand model data (0 = none)
    void* pLeftData;             // 0x04  left hand model data (0 = none)
    void* pHeadData;             // 0x08  head model data
    void* pWepHand;              // 0x0C  weapon hand model data (initWepHand; setRightHand(1) uses it)
    cModelInfo* pShape;          // 0x10  head model info (face shape animation target of ShapeSet/ShapeEnd)
    u32 x14;                     // 0x14
    u32 x18;                     // 0x18
    cModelInfo* pRight;          // 0x1C  right hand model info
    cModelInfo* pLeft;           // 0x20  left hand model info
    cModelInfo* pHair;           // 0x24
    cModelInfo* pEye;            // 0x28  (flags |= 0x40)
    cModelInfo* pFace;           // 0x2C  face model info (pl_knife zeroes/ones its 0x5C/0x70/0x84)
    u32 leftNo;                  // 0x30  current left hand item no
    u32 leftNoPrev;              // 0x34  previous one (setLeftHand(0x63) restores it)
    cModel* pModel;              // 0x38
    f32 waist;                   // 0x3C  waist twist angle (waistSet)
    SpaeData spae[2];            // 0x40

    cPlBody(cModel* model);
    void move();
    void waistSet(f32 angle);
    void waistMove();
    void makeSpaeData();
    void initWepHand(u32 hand);
};

#endif
