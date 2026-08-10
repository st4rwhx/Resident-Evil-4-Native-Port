#ifndef COCKPIT_H
#define COCKPIT_H

#include "types.h"

// HUD (game/cockpit.cpp), instance `Cckpt` (0xCC bytes): the life meter, the bullet counter, the
// count-down timer and the action button prompt. Id table types: 0x21 life meter, 0x20 action
// button, 0x23 count-down, 0x2F message window, 0x30 HUD frame, 0x32 bullet icon.
class LifeMeter {
public:
    u32 flags;         // 0x00
    u8 pad_4[0x18];    // 0x04
    f32 life;          // 0x1C  smoothed player life
    s8 level;          // 0x20  lifeLevel(20, pl_life_max, 1200)
    u8 pad_21[3];
    f32 col0[4];       // 0x24  smoothed player meter colours (unit 0x12 col0 / col1)
    f32 col1[4];       // 0x34
    f32 subLife;       // 0x44  smoothed partner life
    s8 subLevel;       // 0x48  lifeLevel(5, sub_life_max, 600)
    u8 pad_49[3];
    f32 subCol0[4];    // 0x4C  smoothed partner meter colours (unit 3)
    f32 subCol1[4];    // 0x5C
    u8 c0[3][4];       // 0x6C  colour templates: units 0x11 / 0x10 / 0x0F col0
    u8 c1[3][4];       // 0x78  and col1  (sizeof == 0x84)

    void roomInit();
    void move();
    void fix(int sw);
    void disp(int sw);
    void frameOut();
    void frameIn();
};

class BulletInfo {
public:
    u8 pad_0[0x10];
    s32 markNo;        // 0x10  bullet icon currently shown (type 0x32 id), -1 none
    u8 pad_14[0x2C - 0x14];

    void roomInit();
    void move();
};

class CountDown {
public:
    u32 flags;         // 0x00  bit0 running, bit3 paused by the game flags, bit4 hidden
    s8 min;            // 0x04
    s8 sec;            // 0x05
    s8 cs;             // 0x06  1/100 s
    u8 pad_7;
    u32 frame;         // 0x08  remaining time in frames
    u32 warnFrame;     // 0x0C  frame count below which the digits take unit 8's colour
    u32 counter;       // 0x10  1/100 digit jitter phase (0..5)
    u32 savedFlags;    // 0x14  saveDisp / loadDisp

    void roomInit();
    void move();
    void disp(int sw);
    void frameIn();
    void frameOut();
    void initTime(int m, int s, int c);
    void initTimeFrame(u32 frame);
    void warnTime(int m, int s, int c);
    void getTime(int* m, int* s, int* c);
    u32 getFrame();
    void saveDisp();
    void loadDisp();
    int checkState(u32 bit);  // game/mercenaries.cpp: (flags & bit) ? 1 : 0
};

class ActionButton {
public:
    u8 no;             // 0x00  button prompt to show (0 none)
    u8 old;            // 0x01  prompt shown last frame
    u8 pad_2[2];

    void roomInit();
    void move();
};

class Cockpit {
public:
    LifeMeter life;         // 0x00
    BulletInfo bullet;      // 0x84
    CountDown countDown;    // 0xB0
    ActionButton action;    // 0xC8  sizeof == 0xCC

    void gameInit();
    void roomInit();
    void move();
    void msgWindow(int mode);
    void lifeMeterDisp(int sw);
    // sscrn reaches the count-down through this: `&Cckpt` is computed first, then + 0xB0
    CountDown* getCountDown() { return &countDown; }
};

extern Cockpit Cckpt;
extern int g_boss_bar_flag;   // game/cockpit.cpp

// game/cockpit.cpp: bullet icon (type 0x32) id for a weapon number, 0xFF none
u8 dispBulletIconMarkNo(u8 wepNo);

#endif
