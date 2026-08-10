#include "cri_xpt.h"

extern void SJCRS_Lock(void);
extern void SJCRS_Unlock(void);

static Sint32 gcrna_cs_lvl;
static Sint32 gcrna_cs_msk;

void GCRNA_UnlockCs(void)
{
	SJCRS_Unlock();
}

void GCRNA_LockCs(void)
{
	SJCRS_Lock();
}
