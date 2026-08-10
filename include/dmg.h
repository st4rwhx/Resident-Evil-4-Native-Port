#ifndef DMG_H
#define DMG_H

#include "types.h"
#include "vec.h"
#include "cManager.h"
#include "main_mem.h"

// Damage volume (game/dmg.cpp): a cylinder (id 0) or an XZ quad (id 1) that hurts whatever
// stands in it for `timer` frames.
class cDmg : public cUnit {
public:
    u32 id;      // 0x0C  construct id: 0 cylinder, 1 quad
    int kind;    // 0x10  damage kind, returned by hitCheck (1/4/5/7 break the item enemies)
    int timer;   // 0x14  frames left

    virtual ~cDmg() {}
    virtual void beginEvent();
    virtual int hitCheck(Vec* pos, Vec* out) = 0;
};

class cDmgCyl : public cDmg {
public:
    Vec pos;  // 0x18
    f32 r;    // 0x24
    f32 h;    // 0x28  half height

    virtual int hitCheck(Vec* pos, Vec* out);
};

class cDmgP4 : public cDmg {
public:
    Vec pt[4];  // 0x18
    f32 h;      // 0x48

    virtual int hitCheck(Vec* pos, Vec* out);
};

// Damage volume manager (game/dmg.cpp `DmgMgr`, 0x34 bytes, work size 0x118).
#line 95 "D:/Bio4/Prog/dmg.h"
class cDmgMgr : public cManager<cDmg> {
public:
    cDmgMgr();
    virtual void* memAlloc(u32 size) { return MEM_ALLOC(size, 1, 13); }
    virtual void memFree(void* p) { Mem_free(p); }
    virtual void memClear(cDmg* p, u32 size) { memclr_asm(p, size); }
    virtual int construct(cDmg* p, u32 id);
    int construct(cDmg* p, int id);

    void move();
    // Registers a cylinder volume: kind, frames, centre, radius, half height. Returns 1 when a
    // volume was created (int result: the call's set of r3 changes the haifa depend counts, obj10 dmgSet).
    int set(int kind, int time, Vec* pos, f32 r, f32 h);
    // Registers a quad volume: kind, frames, the 4 XZ corners, half height.
    int set(int kind, int time, Vec* pt, f32 h);
    // Damage volume containing `pos`: its kind, 0 when none; `out` gets the volume's centre.
    int hitCheck(Vec* pos, Vec* out);
};

extern cDmgMgr DmgMgr;

#endif
