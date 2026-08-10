#include "cri_xpt.h"

typedef struct {
	Uint8 pad[0xB4];
	void *ahx;
	Sint32 nsmpl;
	Sint32 nsmpl96;
} ADXB_OBJ;

void (*ahxsetsjifunc)(void *ahx, void *sji);
void (*ahxsetdecsmplfunc)(void *ahx, Sint32 nsmpl);
void (*ahxtermsupplyfunc)(void *ahx);
void (*ahxexecfunc)(ADXB_OBJ *adxb);

/* dead-stripped by the linker (nothing calls it); fixes the .bss order of the four pointers */
void ADXB_EntryAhxFunc(void (*setsji)(void *, void *), void (*setdecsmpl)(void *, Sint32),
		       void (*termsupply)(void *), void (*exec)(ADXB_OBJ *))
{
	ahxsetsjifunc = setsji;
	ahxsetdecsmplfunc = setdecsmpl;
	ahxtermsupplyfunc = termsupply;
	ahxexecfunc = exec;
}

void ADXB_AhxTermSupply(ADXB_OBJ *adxb)
{
	if (adxb->ahx != NULL) {
		ahxtermsupplyfunc(adxb->ahx);
	}
}

void ADXB_ExecOneAhx(ADXB_OBJ *adxb)
{
	ahxexecfunc(adxb);
}

void ADXB_SetAhxDecSmpl(ADXB_OBJ *adxb, Sint32 nsmpl)
{
	if (adxb->ahx != NULL) {
		ahxsetdecsmplfunc(adxb->ahx, nsmpl);
	}
	adxb->nsmpl = nsmpl;
	adxb->nsmpl96 = nsmpl / 96;
}

void ADXB_SetAhxInSj(ADXB_OBJ *adxb, void *sji)
{
	if (adxb->ahx != NULL) {
		ahxsetsjifunc(adxb->ahx, sji);
	}
}
