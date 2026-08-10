#ifndef TV_MODE_H
#define TV_MODE_H

#include "types.h"
#include "gx.h"

// game/tv_mode.cpp: progressive-scan prompt task run at boot.
struct TvModeWork {
    u8 state;                // 0x00  index into tvModeFuncTbl
    u8 sub;                  // 0x01
    u8 active;               // 0x02
    u8 pad_3;
    GXRenderModeObj* rmode;  // 0x04
};

extern TvModeWork* pTv;
extern u8 tv_mode_cnt;

extern "C" {
void SetTvMode(GXRenderModeObj* rmode);
void tvModeCheckTask();
void tvModeTrigger(TvModeWork* tv);
void tvModeMenu_progressive(TvModeWork* tv);
void tvModeExit(TvModeWork* tv);
}

#endif
