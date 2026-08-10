#include "mps.h"

Sint32 MPS_GetPketHd(MPS mps, MPS_PKETHD *hd)
{
	if (MPSLIB_CheckHn(mps) != 0) {
		return MPSLIB_SetErr(NULL, 0xFF020203);
	}
	*hd = mps->pkethd;
	return 0;
}

Sint32 MPS_GetLastSysHd(MPS mps, MPS_SYSHD *hd)
{
	if (MPSLIB_CheckHn(mps) != 0) {
		return MPSLIB_SetErr(NULL, 0xFF020202);
	}
	*hd = mps->last_syshd;
	return 0;
}

Sint32 MPS_GetSysHd(MPS mps, MPS_SYSHD *hd, Sint32 no)
{
	if (MPSLIB_CheckHn(mps) != 0) {
		return MPSLIB_SetErr(NULL, 0xFF020202);
	}
	*hd = mps->syshd[no];
	return 0;
}

Sint32 MPS_GetPackHd(MPS mps, MPS_PACKHD *hd)
{
	if (MPSLIB_CheckHn(mps) != 0) {
		return MPSLIB_SetErr(NULL, 0xFF020201);
	}
	*hd = mps->packhd;
	return 0;
}

void MPSGET_Finish(void)
{
}

void MPSGET_Init(void)
{
}
