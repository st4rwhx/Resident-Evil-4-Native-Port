#ifndef ATARIINFO_H
#define ATARIINFO_H

#include "types.h"
#include "vec.h"

class cModel;

// Character collision info (game/atariInfo.cpp), 0x4C bytes; embedded in cPlayer at 0x2B4.
// The flag helpers are parameterless in-class inlines on purpose: the original accesses go
// `addi rX,this,0x2B4; lhz 0x1A(rX); andi. 0xFCFF` (address computed once, used by the load and
// the store; the mask folded to 16 bits). A direct member access folds the address into one
// displacement; an inline taking the bit as a parameter gives `rlwinm` instead of `andi.`.
class cAtariInfo {
public:
    Vec pos;         // 0x00  offset from the model (rotated by the model's rot)
    f32 rectX;       // 0x0C  push rectangle half size along local X (pl_push) / cylinder radius
    f32 rectZ;       // 0x10  along local Z
    f32 h;           // 0x14  half height
    s16 partsNo;     // 0x18  parts index + 1 the info follows, 0 = the model (pl_dmg Pl_R0_Die sets 4)
    u16 flags;       // 0x1A  bit1: rectangle (dispRect), bits 3-4: priority, bits 8-9 (0x300): collide with enemies
    f32 rectX2;      // 0x1C  rect size `move` interpolates rectX/rectZ towards
    f32 rectZ2;      // 0x20
    u16 cnt;         // 0x24  frames left of the interpolation
    u16 x26;         // 0x26  (init0 sets 1) bit0: no character collision this frame (at_mod EmAtCheck)
    cModel* pLink;   // 0x28  model pushed along with this one (at_mod At_em_sphere_sphere_ck)
    f32 x2C;         // 0x2C
    Vec worldPos;    // 0x30  getPos result of this frame (at_mod EmAtCheck)
    Vec oldWorldPos; // 0x3C  worldPos of the previous frame
    union {
        u32 x48;             // 0x48
        cAtariInfo* next;    // 0x48  next info of the chain (at_mod DrawOba)
    };

    cAtariInfo();
    void init0(int parts, int cnt, int flags, f32 x, f32 y, f32 z, f32 rx, f32 rz, f32 w, f32 h);
    // init(parts, flags, cnt, ...) = init0(parts, cnt, flags, ...); flags |= 1
    void init(int parts, int flags, int cnt, f32 x, f32 y, f32 z, f32 rx, f32 rz, f32 w, f32 h);
    void setPriority(int prio);  // flags bits 3-4
    // mode < 0: rect = (100, 100), rect2 = a/b, cnt = -mode; mode == 0: rect = rect2 = a/b; > 0: rect2 only, cnt = mode
    void set(int mode, f32 a, f32 b);
    void move();
    // World position (`getPos`) and the positions before/after this frame's move (`getSpeedVector`).
    void getSpeedVector(cModel* m, Vec* oldPos, Vec* pos);
    void getPos(cModel* m, Vec* out);
    void disp(cModel* m);
    void dispRect(cModel* m);
    void throughOn() { flags &= ~0x300; }   // pass through enemies (mahoThroughOn)
    void throughOff() { flags |= 0x300; }
    void clrFlag100() { flags &= ~0x100; }  // obj20 SetObaModel
    void setFlag100() { flags |= 0x100; }   // sce_com SceUpCutEnd
    void clrFlag200() { flags &= ~0x200; }  // emhit setParent: the parent no longer collides with enemies
    void setFlag200() { flags |= 0x200; }   // obj13 objLadderSatSet
    void scrOn() { flags &= ~0x200; flags |= 0x100; }  // obj00 setScrAtari
};

#endif
