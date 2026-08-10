#ifndef DB_LOG_H
#define DB_LOG_H

#include "types.h"
#include "va_ppc.h"

// One debug log line (0x4C bytes).
struct cLogWork {
    int key;        // 0x00  duplicate-suppression key (0 = none)
    u8 color;       // 0x04  eprintf color
    u8 pad_5[3];
    char str[64];   // 0x08
    u8 pad_48[4];

    void print(int x, int y);
    void clear();
};

// cLog::add flag bits
#define LOG_NO_SHOW    1  // do not restart the display timer
#define LOG_DUP_QUIET  2  // a duplicate line does not restart the display timer either
#define LOG_NO_PRINTF  4  // do not echo the line to the console

// Debug log (src/game/db_log.cpp), 0x1DC0 bytes, allocated from the debug heap by LogInit.
class cLog {
public:
    u8 flags;             // 0x00  bit1 = a line was added this frame, bit0 = it was a duplicate
    u8 blink;             // 0x01  duplicate marker animation counter (0..7)
    u8 pad_2[4];
    u8 timer;             // 0x06  frames left to display (0xFF = always)
    u8 cur;               // 0x07  index of the newest line
    u8 time;              // 0x08  display duration set by modeSet
    u8 lines;             // 0x09  visible lines
    s16 x;                // 0x0A
    s16 y;                // 0x0C
    u8 scr;               // 0x0E  scroll offset
    u8 pad_F;
    cLogWork work[100];   // 0x10

    void init();
    void mes(int a, int b, const char* fmt, ...);
    void err(int a, int b, const char* fmt, ...);
    void warn(int a, int b, const char* fmt, ...);
    void vmes(int a, int b, const char* fmt, va_list ap);
    void verr(int a, int b, const char* fmt, va_list ap);
    void vwarn(int a, int b, const char* fmt, va_list ap);
    void clear();
    int modeReset();
    int modeSet(int x, int y, int time, int lines);   // window position/duration/size (t_log: 0x30, 0x2A, 0xFF, 0x19)
    void disp();
    int on(int flag);
    cLogWork* add(int flag, int key, const char* fmt, va_list ap);
    int scrSet(s8 n);                          // scroll by `n`, clamped to [0, 100 - lines]
    int dispLineNum(int x, int y);
};

// The log pointer is wrapped in a struct: the original loads `pLog` as a struct member (GCC 2.95's
// MEM_IN_STRUCT_P alias heuristic then keeps the load after preceding stores through `this`
// pointers, e.g. cTexSys::Init stores nTexObj = 0 before loading pLog). A plain `cLog*` global
// lets the scheduler hoist the pLog load above such stores.
struct cLogPtr {
    cLog* p;
    cLog* operator->() { return p; }
};

extern cLogPtr pLog;

void LogInit();

// Debug break with source location (used by the header-inline range checks).
extern void dbgAssert(const char* file, int line);

#endif
