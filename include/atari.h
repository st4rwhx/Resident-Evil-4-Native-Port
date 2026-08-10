#ifndef ATARI_H
#define ATARI_H

#include "types.h"
#include "vec.h"
#include "db_log.h"
#include "cManager.h"
#include "at_sub.h"
#include "at_sub2.h"
#include "main_mem.h"

class cModel;
class cAtariInfo;

#line 8 "D:/Bio4/Prog/atari.h"

// 16 one-bit flags with range-checked access (game/atari.cpp).
class cFlag {
public:
    u16 flags;

    void set(u32 stat) {
        if (stat > 15) {
            pLog->err(0, 0, "cFlag.set() arg stat OVER FLOW %d", stat);
            return;
        }
        flags |= 1 << stat;
    }
    void clr(u32 stat) {
        if (stat > 15) {
            dbgAssert(__FILE__, __LINE__);
            return;
        }
        flags &= ~(1 << stat);
    }
    int check(u32 stat) {
        if (stat > 15) {
            pLog->err(0, 0, "cFlag.set() arg stat OVER FLOW %d", stat);
            return 0;
        }
        return flags & (1 << stat);
    }
};

// Scenario collision data file (SAT): counts, then the vertex, face normal, edge, polygon and
// block tables back to back (cSat::operator= computes the table pointers).
class cSatFile {
public:
    u8 id;           // 0x00  0xFF for a file (a cSatHeader has bit7 set instead)
    u8 x1;
    u16 nVertex;     // 0x02
    u16 nNormal;     // 0x04
    u16 nEdge;       // 0x06
    u16 x8;
    u16 nPoly;       // 0x0A  polygon total (< 0x2000)
    u16 nA;          // 0x0C  polygon groups: [0, nA), [nA, nA + nB), the last nC (cSatMgr::disp)
    u16 nB;          // 0x0E
    u16 nC;          // 0x10
    u16 x12;         // 0x12
    // 0x14: Vec vtx[nVertex]; Vec nrm[nNormal]; Vec edge[nEdge]; AtPoly poly[nPoly]; cSatBlock blocks

    Vec* getVertexPtr();
    int dataCheck();
};

// Table of SAT files (room collision archive): offsets from the header.
class cSatHeader {
public:
    u8 id;           // 0x00  bit7 set
    u8 pad_1[3];
    u32 ofs[0];      // 0x04

    cSatFile* getSat(int no);
};

// Spatial partition of a SAT: an XZ box holding polygon indices, or (flag bit0) a child block
// chain in place of the indices. `next` is stored as a relative offset in the file
// (cSat::blockInit turns it into a pointer).
class cSatBlock {
public:
    Vec min;         // 0x00  box minimum (y unused)
    Vec size;        // 0x0C  box size
    u16 n0;          // 0x18  indices of group A (floors: flag 0x40 checks [0, n0 + n1))
    u16 n1;          // 0x1A  group B
    u16 n2;          // 0x1C  group C (walls: flag 0x80 checks [n0 + n1, n0 + n1 + n2))
    u16 flag;        // 0x1E  bit0: `idx` holds a child cSatBlock
    cSatBlock* next; // 0x20
    u16 idx[0];      // 0x24  polygon indices

    int lineOverlap(Vec* p, Vec* dir, Vec* absDir);
    int hitCheckSphere(Vec* a, Vec* b, f32 r);
};

// One scenario collision piece (game/atari.cpp), returned by cSatMgr::create. Owners toggle
// the flag byte (emobj setSatMain / clrSat: bit2 = active).
class cSat : public cUnit {
public:
    Vec* vtx;        // 0x0C  (the three table pointers double as the AtPolyData the at_sub checks take)
    Vec* nrm;        // 0x10
    Vec* edge;       // 0x14
    AtPoly* poly;    // 0x18
    u16 nVertex;     // 0x1C
    u16 nPoly;       // 0x1E
    u16 nA;          // 0x20
    u16 nB;          // 0x22
    u16 nC;          // 0x24
    u16 x26;         // 0x26
    u16 nNormal;     // 0x28
    s8 flags;        // 0x2A  bit1: pFile was allocated by cSatMgr::create (freed by destroy), bit2: piece takes part in the collision checks (signed: `&= ~4` is a word rlwinm)
    u8 pad_2B;
    u16 nEdge;       // 0x2C
    u8 pad_2E[2];
    cSatBlock* block;  // 0x30  root block
    u8 pad_34[0x58 - 0x34];
    cSatFile* pFile; // 0x58
    u32 x5C;         // 0x5C
    Mtx mat;         // 0x60  piece -> world
    Mtx inv;         // 0x90  world -> piece

    // Tools t_atari's static cSat arrays (stw 1; stw vptr; stb 0 per element in the static init loop) and
    // ss_map's cSat locals show the real constructor: alive flag through the base, active flags cleared.
    // The flags store goes through a reference: its address is then a register `f = this + 0x2A` whose
    // cse class holds the frame-direct `(plus fp N)` for an inlined local (find_best_addr's REG path
    // prefers the more expensive equivalent), so the `stb` is frame-relative while the vptr stores keep
    // `this` (ss_map mapPositionCheck); a plain member store stays `(plus this 0x2A)` (PLUS path cost
    // tie). No new header-level declaration: esp's static `max.<DECL_UID>` name is gcse-hash sensitive.
    cSat() : cUnit(1) { s8& f = flags; f = 0; }
    void init(cSatFile* f, Vec* pos, Vec* rot);
    void setCoord(Vec* pos, Vec* rot);
    void setMatrix(Mtx m);
    cSat& operator=(cSatFile* f);
    void blockInit(cSatBlock* blk);
    void disp(int no, u32 color, int zupd);
    // alive and taking part in the checks (hides cUnit::isAlive for cManager<cSat>::destroy)
    int isAlive();
};

inline int cSat::isAlive()
{
    if ((be_flag & 0x201) == 1 && (flags & 4)) {
        return 1;
    }
    return 0;
}

// Scenario collision manager (game/atari.cpp; SatMgr is the room, EatMgr the effect set).
class cSatMgr : public cManager<cSat> {
public:
    int seCk;        // 0x34  copied to SEck (at_sub attribute filter bypass) by every hitCheck2

    cSatMgr();
#line 381 "D:/Bio4/Prog/atari.h"
    virtual void* memAlloc(u32 size) { return MEM_ALLOC(size, 1, 13); }
    virtual void memFree(void* p) { Mem_free(p); }
    virtual void memClear(cSat* p, u32 size) { memclr_asm(p, size); }
    virtual void log(const char* fmt, ...);
    virtual void destroy(cSat* p);
    virtual int construct(cSat* p, u32 id);

    // Runtime scenario piece from a 4-corner polygon (createFloorSat / createBoxSat / createSat by
    // flag bits 0x200 / 0x100); returns the registered piece or NULL.
    cSat* create(Vec* pos, Vec* rot, Vec* poly, int attr, int flag, f32 h);
    // Piece from prebuilt collision data (obj15 cObjGatling::setEat: EatMgr.create(data, 0, &pos, &rot, type)).
    cSat* create(void* data, int flag, Vec* pos, Vec* rot, u8 type);
    // Ray from `top` down to `bottom`; returns the hit attribute, hit point in `hit`; `attr`
    // receives the address of the hit polygon's normal (in the piece's space).
    int hitCheck2(Vec* top, Vec* bottom, Vec* hit, u32* attr, int flag, int mask);
    // Line segment `a`-`b` against the scenario; hit point and normal out. Returns 0 when nothing was hit.
    int hitCheck(Vec* a, Vec* b, Vec* hit, Vec* nrm, int flag, int mask);
    // Floor height under `pos`, searching `up` above and `down` below it.
    f32 getFloor(Vec* pos, f32 up, f32 down, u32* attr, int flag);
    // Sphere of radius `r` moving from `a` to `b` against the scenario; `b` is pushed out of the
    // polygons (cLight::hitAdjust). Returns 1 when the sphere was adjusted.
    int polySphereCk(Vec* a, Vec* b, f32 r, int flag, Vec* nrm, int mask);
    // Debug draw of the collision polygons (t_option "SCROLL VIEW").
    void disp(int flag);
    // Model against the scenario (obj00: `SatMgr.check(this, 0)`).
    int check(cModel* m, int flag);
    int checkRect(cModel* m);
    int checkAir(cModel* m, int flag);
    f32 scrAtCheckSphere(cModel* m, cAtariInfo* info, int flag);
    f32 scrAtCheckSphereAir(cModel* m, cAtariInfo* info, int flag);
    void wallAdjust(Vec* nrm, Vec* oldPos, Vec* pos, f32 r, int flag, int mask);
    // Sphere of radius `r` moving from `oldPos` to `pos`; `pos` is pushed out of the polygons and
    // the hit normal goes to `nrm` (zero when nothing was hit). obj01 grenade bounce.
    void adjust(Vec* nrm, Vec* oldPos, Vec* pos, f32 r, int flag, int mask);
};

extern cSatMgr SatMgr;

// Effect collision manager (game/atari.cpp `EatMgr`, 0x260 bytes).
class cEatMgr : public cSatMgr {
public:
    AtEffInfo effInfo[8];  // 0x38  per effect type hit effect ids
    u8 effOn[8];           // 0x258  effInfo[i] registered

    cEatMgr();
    virtual void log(const char* fmt, ...);

    void initEffInfo();
    void registEffInfo(int type, AtEffInfo* src);
    AtEffInfo* getEffInfo(int type);
};

extern cEatMgr EatMgr;

extern "C" {
// Effect type of a hitCheck attribute word (game/at_sub.cpp).
int EatGetEffectType(u32 attr);
}

#endif
