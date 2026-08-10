#ifndef TPL_H
#define TPL_H

#include "types.h"

// TPL texture palette layout (charPipeline/texPalette.h without the SDK includes). File offsets
// are relocated to pointers by cTexSys::CalcTplAddr.
struct CLUTHeader {
    u16 numEntries;  // 0x00
    u8 unpacked;     // 0x02
    u8 pad8;         // 0x03
    u32 format;      // 0x04
    void* data;      // 0x08
};

struct TEXHeader {
    u16 height;         // 0x00
    u16 width;          // 0x02
    u32 format;         // 0x04
    void* data;         // 0x08
    u32 wrapS;          // 0x0C
    u32 wrapT;          // 0x10
    u32 minFilter;      // 0x14
    u32 magFilter;      // 0x18
    f32 LODBias;        // 0x1C
    u8 edgeLODEnable;   // 0x20
    u8 minLOD;          // 0x21
    u8 maxLOD;          // 0x22
    u8 unpacked;        // 0x23
};

struct TEXDescriptor {
    TEXHeader* textureHeader;  // 0x00
    CLUTHeader* CLUTHeader;    // 0x04
};

struct TEXPalette {
    u32 versionNumber;               // 0x00
    u32 numDescriptors;              // 0x04
    TEXDescriptor* descriptorArray;  // 0x08
};

extern "C" TEXDescriptor* TEXGet(TEXPalette* pal, u32 id);

#endif
