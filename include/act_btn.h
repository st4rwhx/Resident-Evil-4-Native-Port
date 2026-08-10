#ifndef ACT_BTN_H
#define ACT_BTN_H

#include "types.h"

// One action button prompt (0x18 bytes), linked into cActionButton::ot by slot.
struct ActBtnWork {
    u32 tag;       // 0x00  OTag link
    void* func;    // 0x04  void (*)(int arg, int d): the action (NULL = none)
    int arg;       // 0x08  first argument (type 2: the SceAtWork*)
    u8 kind;       // 0x0C  prompt message: cMes number kind + 0x16 (clamped to 0x41)
    u8 slot;       // 0x0D  ot slot / SceExec priority
    u8 type;       // 0x0E  0 call func, 1 SceExec(0x12, func...), 2 SceAt area action
    u8 btn;        // 0x0F  button kind (checkButton)
    int d;         // 0x10  second argument
    u32 flags;     // 0x14  bit0 sets pG->flags_500C 0x200000, bit1 no actCheck / trigger, bit2 exec flag 2,
                   //       bit3 no prompt, bit4 hold, bit5 skip, bit6 exclusive, bit7 message colour 7
};

// Action button prompt manager (game/act_btn.cpp `ActBtn`, 0x104 bytes).
class cActionButton {
public:
    u32 ot[16];          // 0x00
    u8 num;              // 0x40  works pulled this frame
    u8 stop;             // 0x41  pG->flags_170 bit8 at init: prompts disabled
    u8 active;           // 0x42  a prompt was shown this frame
    u8 pad_43;
    ActBtnWork work[8];  // 0x44

    cActionButton() {}
    ~cActionButton() {}
    void init();
    void move();
    void disp(ActBtnWork* w);
    int checkButton(ActBtnWork* w);
    int checkPLStatus(ActBtnWork* w);
    ActBtnWork* pullWork();
    // set(kind, slot, func, arg, flags, btn, type, d): pulls a work, fills it and adds the prim
    void set(int kind, int slot, int func, int arg, int flags, int btn, int type, int d);
};

extern cActionButton ActBtn;

#endif
