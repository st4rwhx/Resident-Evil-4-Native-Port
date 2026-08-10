#ifndef MAP_OBJ_H
#define MAP_OBJ_H

#include "types.h"
#include "cManager.h"
#include "model.h"
#include "main_mem.h"

// Scroll (map) object work (game/map_obj.cpp): a cModel flagged 0x1023 whose type byte
// (0x100) is the map id and whose index byte (0x320) counts the works of the same id.
class cMap : public cModel {
public:
    u8 index;             // 0x320  number of the works of the same id created before this one
    u8 pad_321[3];        // sizeof == 0x324

    cMap();
    virtual ~cMap() {}
    virtual void move();
};

// The map object manager. memAlloc's MEM_ALLOC is at header line 70: its __FILE__ /
// __LINE__ ("D:/Bio4/Prog/map_obj.h", 0x46) are in map_obj's .rodata (and this string is
// what every unit including this header emits first into its .rodata).
#line 67 "D:/Bio4/Prog/map_obj.h"
class cMapMgr : public cManager<cMap> {
public:
    cMapMgr();
    virtual void* memAlloc(u32 size) { return MEM_ALLOC(size, 1, 13); }
    virtual void memFree(void* p) { Mem_free(p); }
    virtual void memClear(cMap* p, u32 size) { memclr_asm(p, size); }
    virtual int construct(cMap* p, u32 id);

    // Work `no`, 0 when out of range. The callers' guarded do-while loops inline it with the
    // range check kept at the loop top only: the `m` copy stops thread_jumps (user-variable
    // operand) in the pass before cse1, after which cse propagates it and the pass after loop.c
    // threads only the back edge past the check (a plain `no >= nArray` loses the check).
    cMap* getWork(u32 no) {
        cMapMgr* m = this;
        if (no >= m->nArray) {
            return 0;
        }
        return (cMap*)((u8*)m->pArray + m->size * no);
    }

    cMap* room(int id, int no);   // the live work with type `id` and index `no`
    void move();
    int dispInfo();               // free work count at (0x1D8, 0x2A); 0 without an array
};

extern cMapMgr MapMgr;

#endif
