#ifndef MANAGERS_H
#define MANAGERS_H

#include "types.h"

// Four unrelated manager classes (cMap, cDmg, Event, cModelInfo) all have
// an identical setName(u32) at the same offset (0x24) - almost certainly
// instantiations of a shared template base (naming pattern
// "cManager_<Type>_" is a classic CodeWarrior template mangling shape),
// but we don't need to know that to match the compiled output; four
// separate minimal stub classes with the same layout do the job.

class cManager_cMap_ {
public:
    void setName(u32 name);
private:
    u8 unk_00[0x24];
    u32 mName;
};

class cManager_cDmg_ {
public:
    void setName(u32 name);
private:
    u8 unk_00[0x24];
    u32 mName;
};

class cManager_Event_ {
public:
    void setName(u32 name);
private:
    u8 unk_00[0x24];
    u32 mName;
};

class cManager_cModelInfo_ {
public:
    void setName(u32 name);
private:
    u8 unk_00[0x24];
    u32 mName;
};

#endif
