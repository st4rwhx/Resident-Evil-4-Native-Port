#ifndef _DOLPHIN_OSCACHE_H_
#define _DOLPHIN_OSCACHE_H_

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void DCInvalidateRange(void* addr, u32 nBytes);
void DCFlushRange(void* addr, u32 nBytes);
void DCStoreRange(void* addr, u32 nBytes);
void DCFlushRangeNoSync(void* addr, u32 nBytes);
void DCStoreRangeNoSync(void* addr, u32 nBytes);
void DCZeroRange(void* addr, u32 nBytes);
void DCTouchRange(void* addr, u32 nBytes);
void ICInvalidateRange(void* addr, u32 nBytes);

#ifdef RE4_PORT
/* Portage : 0xE0000000 est la fenetre du cache L1 verrouille du Gekko. Elle n'existe pas sur
 * ARM, et y ecrire ferait tomber le processus. Le portage fournit a la place 16 Ko de memoire
 * ordinaire (port/os.cpp) : meme taille, meme role, sans le verrouillage - qui n'etait qu'une
 * optimisation, jamais une condition de correction. */
void *port_lc_base(void);
#define LCGetBase() port_lc_base()
#define LC_BASE ((u32) port_lc_base())
#else
#define LC_BASE_PREFIX 0xE000
#define LC_BASE (LC_BASE_PREFIX << 16)
#define LCGetBase() ((void*)LC_BASE)
#endif

void LCEnable(void);
void LCDisable(void);
void LCLoadBlocks(void* destTag, void* srcAddr, u32 numBlocks);
void LCStoreBlocks(void* destAddr, void* srcTag, u32 numBlocks);
u32 LCLoadData(void* destAddr, void* srcAddr, u32 nBytes);
u32 LCStoreData(void* destAddr, void* srcAddr, u32 nBytes);
u32 LCQueueLength(void);
void LCQueueWait(u32 len);
void LCFlushQueue(void);
void __OSCacheInit(void);

#ifdef __cplusplus
}
#endif

#endif
