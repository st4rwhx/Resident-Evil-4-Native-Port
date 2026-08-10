#ifndef ROUTE_CK_H
#define ROUTE_CK_H

// game/route_ck.cpp: enemy routing over the room's "RTP" way-point graph (pG->pRoomRtp).

#include "types.h"
#include "vec.h"

class cEm;

// One way point (16 bytes).
struct RtpPoint {
    Vec pos;       // 0x00
    u16 linkOfs;   // 0x0C  first entry in the link table
    u16 nLink;     // 0x0E  linked points
};

// Link table entry (4 bytes).
struct RtpLink {
    s16 point;     // 0x00  way point index
    u16 x2;        // 0x02
};

// RTP file header; the tables are at byte offsets from the header.
struct RtpData {
    u8 pad_0[6];
    u16 nPoint;    // 0x06
    u8 pad_8[4];
    u32 pointOfs;  // 0x0C  RtpPoint[nPoint]
    u32 linkOfs;   // 0x10  RtpLink[]
    u32 nextOfs;   // 0x14  s8 next[nPoint][nPoint]: next hop from row to column, -1 = unreachable
};

extern "C" {
void RouteCk();
// Next position for `em` on its way to `target`; returns 1 when the target itself is reachable.
int RouteCkToEm(cEm* em, cEm* target, Vec* out, int flag);
// Position away from `from` along the nearest point's links.
void RouteCkEscEm(cEm* em, cEm* from, Vec* out);
int RouteCkToPos(cEm* em, Vec* target, Vec* out, int flag, f32* dist);
int RouteCkPosToPos(Vec* from, Vec* to, Vec* out);
int RouteCkConnectPosCk(Vec* a, Vec* b);
f32 RouteCkPosToPosDis(Vec* from, Vec* to);
void RouteCkGetPoint(int no, Vec* out);
int RouteCkGetPointNumber();
f32 RouteCkGetDist(int a, int b);
int RouteCkGetNearPoint(Vec* pos);
// Nearest way point of `em`, cached in rckNear for the frame.
int getNearInfo(cEm* em, int a, int mask);
// Nearest way point to `pos` that the position can reach (a != 0: nearest regardless), -1 = none.
s8 getNearPoint(Vec* pos, int a, int mask);
void Draw_rtp();
void Draw_eminfo();
}

#endif
