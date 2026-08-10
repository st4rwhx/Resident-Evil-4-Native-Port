#ifndef CMOTBASE_H
#define CMOTBASE_H

#include "types.h"
#include "vec.h"
#include "motion.h"

// Motion base follower (game/cMotBase.cpp, 0x38 bytes, cEm+0x7A4): moves a model by its
// motion's root speed from a base pose and eases it back onto the model over `cnt` frames.
class cMotBase {
public:
    cMotModel* pModel;  // 0x00
    Vec pos;            // 0x04  followed position
    Vec rot;            // 0x10  followed rotation
    Vec basePos;        // 0x1C  model position at the last move
    Vec baseRot;        // 0x28  model rotation at the last move
    u8 cnt;             // 0x34  frames left to ease back (0 = follow forever, 0xFF = off)
    u8 pad_35[3];

    cMotBase();
    void set(cMotModel* m, Vec* pos, Vec* rot, u8 cnt);
    void adjust();
    void move();

private:
    void set(cMotModel* m, MotionData* data, Vec* pos, Vec* rot, u8 cnt);
};

#endif
