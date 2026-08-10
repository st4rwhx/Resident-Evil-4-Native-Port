#include "cri_xpt.h"

extern void SJCRS_Lock(void);
extern void SJCRS_Unlock(void);

void LSC_UnlockCrs(Sint32 *msk)
{
	SJCRS_Unlock();
}

void LSC_LockCrs(Sint32 *msk)
{
	SJCRS_Lock();
}
