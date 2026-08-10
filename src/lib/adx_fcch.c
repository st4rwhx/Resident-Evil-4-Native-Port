#include "cri_xpt.h"

extern void DCInvalidateRange(void *addr, Uint32 nbytes);

void ADXF_Ocbi(void *addr, Uint32 nbytes)
{
	DCInvalidateRange(addr, nbytes);
}
