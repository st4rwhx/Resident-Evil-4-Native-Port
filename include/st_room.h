#ifndef ST_ROOM_H
#define ST_ROOM_H

#include "types.h"

// Shared by the stage room scripts (src/st1/, src/st2/, src/st4/). Nothing here emits code or data
// into .text/.rodata; the original room headers are unknown.

// Every original stage module carries a 0x34-byte COMMON block: uninitialised static data members
// that g++ 2.95 emits as `.comm` in every object, merged by the -r link and appended to .bss by
// snmakerel (tools/make_rel.py); their names are not in the binary. The generated skeleton calls the
// block `common_<mod>`, so the compiled rooms define the same symbol (REL_MODULE comes from
// configure.py) and the link keeps merging them into the one block whatever mix of split and
// compiled objects the module has.
#define ST_STR2(x) #x
#define ST_STR(x) ST_STR2(x)
asm(".comm common_" ST_STR(REL_MODULE) ",52,4");

// Scalar stores through a reference: the store is not a struct-member access, so GCC 2.95 assumes
// it may clobber `pG` / the room work pointer and reloads them afterwards, and keeps following
// loads below the store (see global.h BitOn). The rooms store into pG and their work this way.
static inline void U8Set(u8& d, u8 v) { d = v; }
static inline void U16Set(u16& d, u16 v) { d = v; }
static inline void U32Set(u32& d, u32 v) { d = v; }
static inline void IntSet(int& d, int v) { d = v; }
static inline void FAdd(f32& d, f32 v) { d += v; }
static inline void FSub(f32& d, f32 v) { d -= v; }

// The stage-2/4 rooms call SceMesCamSndSet with a fourth argument (`li r6, 4`) the DOL's
// three-parameter definition (sce_com.cpp) never reads: the room build had an older prototype.
void SceMesCamSndSet4(int no, int cut, int se, int flags) asm("SceMesCamSndSet");

#endif
