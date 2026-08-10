#ifndef SYSTEMINIT_H
#define SYSTEMINIT_H

#include "types.h"

extern "C" {
void systemStartInit();
void memclr_asm(void* dst, u32 size);
void RndInit(s32 seed);
void LCEnable();
void systemVISetBlack(s32 black);
void postVSyncCallback();
}

#endif
