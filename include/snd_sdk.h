#ifndef SND_SDK_H
#define SND_SDK_H

// Dolphin SDK declarations for the C sound library (src/game/snd_*.c). The SDK's own
// dolphin/types.h drags in the CodeWarrior libc, so its guard is defined here and the audio
// headers (which only need the scalar types) are included directly. OS is declared by hand.

#include "types.h"

#define _DOLPHIN_TYPES_H_
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#define ATTRIBUTE_ALIGN(num) __attribute__((aligned(num)))

#include <dolphin/ax.h>
#include <dolphin/syn.h>
#include <dolphin/seq.h>
#include <dolphin/mix.h>
#include <dolphin/axart.h>
#include <dolphin/axfx.h>
#include <dolphin/ar.h>
#include <dolphin/dvd.h>

#ifdef __cplusplus
extern "C" {
#endif

void OSReport(const char* msg, ...);
void OSPanic(const char* file, int line, const char* msg, ...);
BOOL OSDisableInterrupts(void);
BOOL OSEnableInterrupts(void);
BOOL OSRestoreInterrupts(BOOL level);
u32 OSGetSoundMode(void);
void OSSetSoundMode(u32 mode);
void DCFlushRange(void* addr, u32 nBytes);
void DCStoreRange(void* addr, u32 nBytes);
void DCInvalidateRange(void* addr, u32 nBytes);
u32 OSGetTick(void);
void AXSetCompressor(u32 switch_);
void AIInit(u8* stack);
void AIReset(void);
void memclr_asm(void* dst, u32 n);
char* strcpy(char* dst, const char* src);
f64 pow(f64 x, f64 y);
int strcmp(const char* a, const char* b);
void AXFXSetHooks(void* (*alloc)(u32), void (*free)(void*));

// The SDK's dolphin/gx/GXGeometry.h is included by every sound unit through dolphin.h. Its
// inline GXEnd is never called, but its two string literals are emitted (in this order) at the
// start of each unit's .rodata. game/snd.cpp does not include the GX headers (no such strings
// in its .rodata) and defines SND_SDK_NO_GX before including this file.
#ifndef SND_SDK_NO_GX
#line 118 "C:/DolphinSDK1.0/include/dolphin/gx/GXGeometry.h"
static inline void GXEnd(void)
{
    extern u8 __GXinBegin;
    if (!__GXinBegin) {
        OSPanic(__FILE__, 118, "GXEnd: called without a GXBegin");
    }
    __GXinBegin = 0;
}
#line 64 "snd_sdk.h"
#endif

#ifdef __cplusplus
}
#endif

#endif
