#ifndef ROOM_TEX_H
#define ROOM_TEX_H

#include "types.h"
#include "texture.h"

// game/room_tex.cpp: the room's cTexSys ("RoomTex", 256 texture objects).
extern cTexSys* g_pRoomTexSys;

void RoomTexRoomInit();   // C++ linkage (RoomTexRoomInit__Fv)

extern "C" {
void RoomTexInit();
int RoomTexDataLoad(TexData* data, u32 owner);
int RoomGetTplAddr(u32 id, TEXPalette** out);
int RoomGetTexObj(u32 id, u32 no, GXTexObj** out);
int RoomGetTlutObj(u32 id, GXTlutObj** out);
}

#endif
