#ifndef JOY_H
#define JOY_H

#include "types.h"

// One vibration request (game/pad.cpp VibSet/VibControl), 0x10 bytes; Joy[0].vib[10].
struct VibWork {
    u16 type;   // 0x00  bit 15 = random level; low 4 bits = clear type (VibSetClearType)
    u16 wait;   // 0x02  frames before it starts
    u16 time;   // 0x04  frames left (0 = free)
    u8 pad_6[2];
    s32 level;  // 0x08  current level (<<7 / <<12 fixed point)
    s32 add;    // 0x0C  per-frame level step
};

// Controller state (game/main.cpp `Joy[4]`, 0x268 bytes each), filled by pad.cpp PadRead.
struct JOY {
    s8 sx;    // 0x00  main stick x
    s8 sy;    // 0x01  main stick y
    s8 ssx;   // 0x02  sub stick x
    s8 ssy;   // 0x03  sub stick y
    u8 trigL; // 0x04  analog L
    u8 trigR; // 0x05  analog R
    u8 anaA;  // 0x06
    u8 anaB;  // 0x07
    s8 x8;    // 0x08  PADStatus err (tv_mode: -3/-2 counts toward the progressive-mode prompt)
    u8 pad_9[3];
    u32 old;  // 0x0C  `on` of the previous frame
    u32 on;   // 0x10  buttons currently held
    u32 trg;  // 0x14  buttons pressed this frame
    u32 rel;  // 0x18  buttons released this frame
    u32 rep;  // 0x1C  buttons held, with auto-repeat (24/6 frames)
    u32 rep2; // 0x20  buttons held, with fast auto-repeat (18/3 frames)
    s8 rep_timer[32];   // 0x24
    s8 rep2_timer[32];  // 0x44
    u8 vib_state;       // 0x64  motor command last sent (Joy[0] only)
    u8 pad_65[3];
    VibWork vib[10];    // 0x68  (Joy[0] only)
    u8 pad_108[0x268 - 0x108];
};

extern JOY Joy[4];

// game/debug.cpp: pad used by the debug tools (currently &Joy[0])
JOY* GetBugCheckController();

extern "C" void* memcpy(void* dst, const void* src, unsigned int n);

// Pad snapshot used by the debug tools: copies Joy[no] into the tool work at byte offset `ofs`.
// The destination is byte-pointer arithmetic on purpose: only then does GCC 2.95 treat the
// stores as possibly aliasing the work pointer and reload it after each copy, which is what the
// original code does (a struct assignment or memcpy(&p->joy[no], ...) hoists the reload).
#define JOY_COPY(work, ofs, no) memcpy((u8*) (work) + (ofs), &Joy[no], sizeof(JOY))

// button bits
#define JOY_LEFT   0x0001
#define JOY_RIGHT  0x0002
#define JOY_DOWN   0x0004
#define JOY_UP     0x0008
#define JOY_Z      0x0010
#define JOY_R      0x0020
#define JOY_L      0x0040
#define JOY_A      0x0100
#define JOY_B      0x0200
#define JOY_X      0x0400
#define JOY_Y      0x0800
#define JOY_START  0x1000
// pad.cpp PadRead: main stick direction (>= 30 units), sub stick direction (> 10 units)
#define JOY_SRIGHT  0x00010000
#define JOY_SLEFT   0x00020000
#define JOY_SDOWN   0x00040000
#define JOY_SUP     0x00080000
#define JOY_SSLEFT  0x00100000
#define JOY_SSRIGHT 0x00200000
#define JOY_SSDOWN  0x00400000
#define JOY_SSUP    0x00800000

#endif
