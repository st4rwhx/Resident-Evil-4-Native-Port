#ifndef DEBUG_H
#define DEBUG_H

#include "types.h"

// game/debug.cpp (C linkage): debug overlays, process timing bars and the debug/config.txt reader.
extern "C" {
void DebugControl();
void debugPadInfoDisp();
void processBarDisp();
void ProcessTickGet(int no, const char* name);
void ProcessTickInit();
void PrimitiveBuffDisp();
void ConfigSet();
// config.txt parser helpers: `*p` is advanced past what was consumed
int symbol_check(char** p, const char* sym);
char* space_skip(char* p);
int comment_check(char** p);
int num_get(char** p);
}

// proc_name is defined in debug.cpp only: an extern here would push its decl before proc_tick's and
// swap the two arrays in .bss (deferred file-scope variables are emitted in first-declaration order).
extern u32 zero_tick;
extern int proc_tick_idx;
extern int proc_tick_idx_bak;
extern int g_proc_cnt;

#endif
