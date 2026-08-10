#ifndef CSCEOBJ_H
#define CSCEOBJ_H

#include "types.h"
#include "vec.h"
#include "model.h"

// Scenario object mover (st/cSceObj.cpp, "D:/Bio4/Prog/cSceObj.cpp"; linked into the st2_0/st2_3/st4_0
// stage RELs): moves a cModel (plus up to four attached models) from a start to an end position/rotation
// over `frame` frames with an accelerate / constant / decelerate profile (move1), or drops it under
// gravity with a bounce (move3). No virtuals: `move` dispatches through a member-pointer table.
class cSceObj {
public:
    u8 step;          // 0x00  phase of the current mover (move1: 0..7, move3: 0..3)
    u8 reverse;       // 0x01  1: run from the end position back to the start
    u8 mode;          // 0x02  0 move1, 1 move2, 2 move3
    u8 flags;         // 0x03  bit0 no rotation, bit1 no position, bit2 rotate the parts parent, bit3 vibration, bit4 move3 bounce
    cModel* obj;      // 0x04
    cModel* sub[4];   // 0x08  attached models, moved with obj
    u32 cnt;          // 0x18  frame counter
    u32 frame;        // 0x1C  total frames
    u32 accFrame;     // 0x20
    u32 cstFrame;     // 0x24
    u32 decFrame;     // 0x28
    Vec spd;          // 0x2C  constant-phase position step (move3: velocity)
    Vec rotSpd;       // 0x38
    Vec basePos;      // 0x44  obj position when the move was set up
    Vec baseRot;      // 0x50
    Vec dPos;         // 0x5C  move amount
    Vec dRot;         // 0x68
    Vec dstPos;       // 0x74
    Vec dstRot;       // 0x80
    Vec srcPos;       // 0x8C
    Vec srcRot;       // 0x98
    Vec accPos;       // 0xA4  acceleration (move3: gravity)
    Vec decPos;       // 0xB0
    Vec accRot;       // 0xBC
    Vec decRot;       // 0xC8
    f32 accR;         // 0xD4  acceleration phase ratio in use
    f32 decR;         // 0xD8
    f32 accR0;        // 0xDC  ratios as set (setReverse swaps them)
    f32 decR0;        // 0xE0
    u16 vibStart;     // 0xE4  vibration: frames with vibAmp0, last vibEnd frames with vibAmp2
    u16 vibEnd;       // 0xE6
    f32 vibAmp0;      // 0xE8
    f32 vibAmp1;      // 0xEC
    f32 vibAmp2;      // 0xF0
    f32 bounce;       // 0xF4  move3 bounce factor

    // Rooms build movers on the stack (r220 moveElevator): the in-class constructor is inlined there.
    cSceObj() {
        u32 i;

        step = 0;
        reverse = 0;
        mode = 0;
        obj = NULL;
        frame = 0;
        accFrame = 0;
        cstFrame = 0;
        decFrame = 0;
        spd.x = 0.0f;
        spd.y = 0.0f;
        spd.z = 0.0f;
        rotSpd.x = 0.0f;
        rotSpd.y = 0.0f;
        rotSpd.z = 0.0f;
        basePos.x = 0.0f;
        basePos.y = 0.0f;
        basePos.z = 0.0f;
        baseRot.x = 0.0f;
        baseRot.y = 0.0f;
        baseRot.z = 0.0f;
        dPos.x = 0.0f;
        dPos.y = 0.0f;
        dPos.z = 0.0f;
        dRot.x = 0.0f;
        dRot.y = 0.0f;
        dRot.z = 0.0f;
        srcPos.x = 0.0f;
        srcPos.y = 0.0f;
        srcPos.z = 0.0f;
        dstPos.x = 0.0f;
        dstPos.y = 0.0f;
        dstPos.z = 0.0f;
        srcRot.x = 0.0f;
        srcRot.y = 0.0f;
        srcRot.z = 0.0f;
        dstRot.x = 0.0f;
        dstRot.y = 0.0f;
        dstRot.z = 0.0f;
        accPos.x = 0.0f;
        accPos.y = 0.0f;
        accPos.z = 0.0f;
        decPos.x = 0.0f;
        decPos.y = 0.0f;
        decPos.z = 0.0f;
        accRot.x = 0.0f;
        accRot.y = 0.0f;
        accRot.z = 0.0f;
        decRot.x = 0.0f;
        decRot.y = 0.0f;
        decRot.z = 0.0f;
        accR = 0.0f;
        decR = 0.0f;
        accR0 = 0.0f;
        decR0 = 0.0f;
        for (i = 0; i < 4; i++) {
            sub[i] = NULL;
        }
    }

    int move();
    int move1();
    int move2();
    int move3();
    void initMove1_pos(cModel* o, u32 nFrame, Vec* dp, f32 acc, f32 dec);
    void setMove1_pos(u32 nFrame, Vec* dp, f32 acc, f32 dec);
    void initMove1_ang(cModel* o, u32 nFrame, Vec* dr, f32 acc, f32 dec, int flg);
    void setMove1_ang(u32 nFrame, Vec* dr, f32 acc, f32 dec, int flg);
    void setMove1_all(u32 nFrame, Vec* dp, Vec* dr, f32 acc, f32 dec, int flg);
    void initMove3_y(cModel* o, Vec* v, f32 grav, f32 h, f32 bnc);
    void setMove3_y(Vec* v, f32 grav, f32 h, f32 bnc);
    void getVibrationValue(Vec* vp, Vec* vr);
    void setSrcDstPos();
    void setStartPos();
    void setEndPos();
    void setStart();
    void setReverse(int rev);
    void setVibration(u16 start, u16 end, f32 amp0, f32 amp1, f32 amp2);

private:
    inline void addPos(Vec* d);
    inline void addRot(Vec* d);
    inline void setPosTo(Vec* target);
    inline void moveTo(Vec* tp, Vec* tr);
    inline void setRotTo(Vec* target);
};

#endif
