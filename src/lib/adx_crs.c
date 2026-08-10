#include "cri_xpt.h"

extern void SVM_Lock(void);
extern void SVM_Unlock(void);

Sint32 adxcrs_lvl;
static Sint32 adxcrs_msk;

void ADXCRS_Unlock(void)
{
	SVM_Unlock();
}

void ADXCRS_Lock(void)
{
	SVM_Lock();
}

void ADXCRS_Init(void)
{
	adxcrs_lvl = 0;
}
