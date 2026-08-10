#ifndef OPTION_H
#define OPTION_H

#include "types.h"

// game/option.cpp: the pause/title option menu, the result screens and the chapter end screen.

// Option menu work (`OptScrn`, 0x14 bytes).
class OptionScreen {
public:
    s8 mode;          // 0x00  0 top menu, 1 sub menu (cursor selects which)
    s8 cursor;        // 0x01  top menu item: 0 retry/load, 1 controller, 2 brightness, 3 audio, 4 back
    s8 sub;           // 0x02  sub menu cursor
    s8 step;          // 0x03  sub menu state (retry/load: 1 confirm, 2 loading, 3 wait for the SE)
    s32 fromTitle;    // 0x04  init argument: 1 = opened from the title screen
    u32 mesAttr;      // 0x08  MesSet attribute word (0x91 in game, 0x94 from the title)
    s8 keyA;          // 0x0C  controller: pSys->flags bit 31
    s8 keyB;          // 0x0D  controller: vibration (bit 27)
    s8 keyC;          // 0x0E  controller: bit 26
    u8 pad_F;
    s8 sound;         // 0x10  audio: 0 stereo, 1 mono, 2 surround
    u8 pad_11[3];

    void init(int fromTitle);
    int move();
    void quit();
};

extern OptionScreen OptScrn;

// Game result screen (id table type 0x28).
class GameResult {
public:
    void* data;       // 0x00  result id archive
    u8 x4;
    u8 x5;
    u8 x6;
    u8 x7;

    void init(void* data);
    int move();
    void quit();
    void omake_init(void* data);
    int omake_move();
};

// Chapter end screen (new'd by sce_com SceChapterEnd, 0xC bytes).
class ChapterEnd {
public:
    void* data;       // 0x00  chapter id archive
    s32 chapter;      // 0x04
    u8 pad_8[4];

    void init(void* data, u8 chapter);
    int move();
    void quit();
};

extern "C" {
// Replaces the language part of an "SS/___/..." path (name + 3).
void setLangExt3(char* name);
int OptionOpenCheck();
}

#endif
