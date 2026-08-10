#include "cri_xpt.h"
#include "adx_stm.h"

Sint32 MWSTM_GetStat(ADXSTM stm)
{
	return ADXSTM_GetStat(stm);
}

void MWSTM_ReqStop(ADXSTM stm)
{
	ADXSTM_StopNw(stm);
	ADXSTM_ReleaseFileNw(stm);
}

void MWSTM_ReqStart(ADXSTM stm)
{
	ADXSTM_Start(stm);
}

void MWSTM_SetFileRange(ADXSTM stm, const Char8 *fname, void *dir, Sint32 ofst, Sint32 nsct)
{
	ADXSTM_ReleaseFileNw(stm);
	ADXSTM_BindFileNw(stm, fname, dir, ofst, nsct);
	ADXSTM_SetEos(stm, nsct);
}

void MWSTM_Destroy(ADXSTM stm)
{
	ADXSTM_Destroy(stm);
}

ADXSTM MWSTM_Create(void *sj)
{
	return ADXSTM_Create(sj, 0);
}

void MWSTM_SetFlowLimit(ADXSTM stm, Sint32 min_nsct, Sint32 max_nsct)
{
	if (stm != NULL) {
		ADXSTM_SetBufSize(stm, min_nsct, max_nsct);
	}
}

Bool MWSTM_IsFsStatErr(ADXSTM stm)
{
	return ADXSTM_GetStat(stm) == 4;
}

Sint32 MWSTM_InitStatic(void)
{
	return 0;
}
