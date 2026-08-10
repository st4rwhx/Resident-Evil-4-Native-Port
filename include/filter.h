#ifndef FILTER_H
#define FILTER_H

#include "types.h"
#include "vec.h"

// Screen post-process filters (game/filter.cpp dispatches to game/filter00.cpp .. filter0b.cpp).
int FilterInit();
int FilterRoomInit();
void FilterTrans();

void Filter00Init();
void Filter00RoomInit();
void Filter00Trans();
void Filter01Init();
void Filter01RoomInit();
void Filter01Trans();
void Filter02Init();
void Filter02RoomInit();
void Filter02Trans();
void Filter03Init();
void Filter03RoomInit();
void Filter03Trans();
void Filter04Init();
void Filter04RoomInit();
void Filter04Trans();
void Filter05Init();
void Filter05RoomInit();
void Filter05Trans();
void Filter05SetParam(int a, int b, int c, int d, int e, int f, int g, f32 x, f32 y, f32 z);
void Filter06Init();
void Filter06RoomInit();
void Filter06Trans();
void Filter07Init();
void Filter07RoomInit();
void Filter07Trans();
void Filter08Init();
void Filter08RoomInit();
void Filter08Trans();
void Filter09Init();
void Filter09RoomInit();
int Filter09GetbUse();
void Filter0aInit();
void Filter0aRoomInit();
void Filter0aTrans();
void Filter0bInit();
void Filter0bRoomInit();
void Filter0bTrans();

// filter00.cpp blur parameters (cLightMgr::setBlur)
void Filter00SetAlpha(u8 alpha);
void Filter00SetPower(s8 power);
void Filter00SetType(u32 type);
void Filter00SetContrast(u8 r, u8 g, u8 b);

extern "C" {
// filter00.cpp: additive radial blur request (highest priority wins)
void Filter00SetAddSpread(u32 pri, int on, u8 r, u8 g, u8 b, u8 rate, u8 type, u32 num, f32 cx, f32 cy, f32 pow);
// filter03.cpp
void Filter03SetParam(int level, u8 r, u8 g, u8 b, u8 pri, int flag);
// filter06.cpp
void Filter06SetParam(u32 level, int r, int g, int b, int a, f32 rate, Vec* spd, f32 alpha, Vec* spdRand, f32 scale,
                      int alphaMin);
}

#endif
