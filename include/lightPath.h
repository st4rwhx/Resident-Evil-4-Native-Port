#ifndef LIGHTPATH_H
#define LIGHTPATH_H

#include "types.h"

// One light path: brightness bytes (0..200) terminated by 0xFF (game/lightPath.cpp).
class cLightPathData {
public:
    u8 data[1];

    u32 getSize();
};

// Path file: count, then byte offsets of each path from the header.
class cLightPathHeader {
public:
    u8 num;      // 0x00
    u8 pad_1[3];
    // 0x04: u32[num] byte offset of each path from the header

    u32 getSize();
    cLightPathData* getPathData(u32 no);
};

// Light path follower kept in cLight::work.
class cLightPath {
public:
    cLightPathData* pStart;  // 0x00
    cLightPathData* pCur;    // 0x04  next brightness byte (0..200, 0xFF = end)
    u8 flag;                 // 0x08  bit0: stop at the end, bit1: invert (200 - v)

    int setPath(cLightPathData* data, u8 no);
    int movePath();
};

#endif
