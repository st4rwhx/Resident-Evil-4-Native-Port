#ifndef PL_PUSH_H
#define PL_PUSH_H

#include "types.h"
#include "vec.h"
#include "player.h"
#include "em.h"

// Player push-object control (game/pl_push.cpp): catching a pushable enemy (id 0x45) and
// pushing it while checking the scenario. Partial layout (0x10 bytes known).
class cPlPush {
public:
    cPlayer* pPl;       // 0x00
    cEm* pTarget;       // 0x04  object being pushed (NULL = none)
    u8 x8;              // 0x08  cleared when a target is caught
    u8 x9;              // 0x09  pushTargetInit flag (bit0: getWHY uses `dir`, else `dirSub`)
    u8 pad_A[2];
    int dir;            // 0x0C  side of the object the player stands on (0..3, 4 = none); read as u8 too (lbz 0xF)

    int catchCheck();
    void pushTargetInit(u8 flag);
    int pushTarget();
    void stopTarget();
    void getWHY(f32* w, f32* h, f32* y);
    int scrHitCheck();
    int scrHitCheckSub(Vec* pos, f32 w, f32 h, f32 y, f32 side);
    int emSandCheck(Vec* pos, f32 w, f32 h, f32 y);
    int plAdjust();
};

#endif
