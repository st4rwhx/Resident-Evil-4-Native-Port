#include "cri_xpt.h"

typedef struct {
	Uint8 pad[0xC];
	void *rna;
} ADXT_OBJ;

extern void ADXRNA_SetAdjsfreqFlg(void *rna, Sint32 flg);

void ADXGC_SetAdjsfreqFlg(ADXT_OBJ *adxt, Sint32 flg)
{
	ADXRNA_SetAdjsfreqFlg(adxt->rna, flg);
}
