#include "cri_xpt.h"

/* ADX renderer front end: thin wrappers around the AX renderer (AXRNA). */

typedef struct AXRNA_OBJ *AXRNA;

extern void AXRNA_SetAdjsfreqFlg(AXRNA rna, Sint32 flg);
extern void AXRNA_SetStmHdInfo(AXRNA rna, void *hdinfo);
extern void AXRNA_DiscardData(AXRNA rna, Sint32 nsmpl);
extern void AXRNA_SetBitPerSmpl(AXRNA rna, Sint32 bps);
extern void AXRNA_SetOutPan(AXRNA rna, Sint32 ch, Sint32 pan);
extern void AXRNA_SetOutVol(AXRNA rna, Sint32 vol);
extern void AXRNA_SetSfreq(AXRNA rna, Sint32 sfreq);
extern void AXRNA_SetNumChan(AXRNA rna, Sint32 nch);
extern void AXRNA_ExecServer(void);
extern Sint32 AXRNA_GetNumRoom(AXRNA rna);
extern Sint32 AXRNA_GetNumData(AXRNA rna);
extern void AXRNA_SetPlaySw(AXRNA rna, Sint32 sw);
extern void AXRNA_SetTransSw(AXRNA rna, Sint32 sw);
extern void AXRNA_Destroy(AXRNA rna);
extern AXRNA AXRNA_Create(Sint32 nch, void *sj);
extern void AXRNA_EntryErrFunc(void (*func)(void *obj, Char8 *msg), void *obj);
extern void AXRNA_Finish(void);
extern void AXRNA_Init(void);
extern void ADXERR_CallErrFunc1(Char8 *msg);

void ADXRNA_SetAdjsfreqFlg(AXRNA rna, Sint32 flg)
{
	AXRNA_SetAdjsfreqFlg(rna, flg);
}

Sint32 ADXRNA_SetStmHdInfo(AXRNA rna, void *hdinfo)
{
	AXRNA_SetStmHdInfo(rna, hdinfo);
	return 0;
}

void ADXRNA_SetTotalNumSmpl(AXRNA rna, Sint32 nsmpl)
{
}

void ADXRNA_DiscardData(AXRNA rna, Sint32 nsmpl)
{
	AXRNA_DiscardData(rna, nsmpl);
}

void ADXRNA_SetBitPerSmpl(AXRNA rna, Sint32 bps)
{
	AXRNA_SetBitPerSmpl(rna, bps);
}

void ADXRNA_SetOutBalance(AXRNA rna, Sint32 bal)
{
}

void ADXRNA_SetOutPan(AXRNA rna, Sint32 ch, Sint32 pan)
{
	AXRNA_SetOutPan(rna, ch, pan);
}

void ADXRNA_SetOutVol(AXRNA rna, Sint32 vol)
{
	AXRNA_SetOutVol(rna, vol);
}

void ADXRNA_SetSfreq(AXRNA rna, Sint32 sfreq)
{
	AXRNA_SetSfreq(rna, sfreq);
}

void ADXRNA_SetNumChan(AXRNA rna, Sint32 nch)
{
	AXRNA_SetNumChan(rna, nch);
}

void ADXRNA_ExecServer(void)
{
	AXRNA_ExecServer();
}

Sint32 ADXRNA_GetNumRoom(AXRNA rna)
{
	return AXRNA_GetNumRoom(rna);
}

Sint32 ADXRNA_GetNumData(AXRNA rna)
{
	return AXRNA_GetNumData(rna);
}

void ADXRNA_SetPlaySw(AXRNA rna, Sint32 sw)
{
	AXRNA_SetPlaySw(rna, sw);
}

void ADXRNA_SetTransSw(AXRNA rna, Sint32 sw)
{
	AXRNA_SetTransSw(rna, sw);
}

/* dead-stripped by the linker; its message stays in .rodata */
Sint32 ADXRNA_GetStat(AXRNA rna)
{
	ADXERR_CallErrFunc1("ADXRNA_GetStat: not implemented\n");
	return 0;
}

void ADXRNA_Destroy(AXRNA rna)
{
	AXRNA_SetPlaySw(rna, 0);
	AXRNA_SetTransSw(rna, 0);
	AXRNA_Destroy(rna);
}

AXRNA ADXRNA_Create(Sint32 nch, void *sj)
{
	return AXRNA_Create(nch, sj);
}

void ADXRNA_EntryErrFunc(void (*func)(void *obj, Char8 *msg), void *obj)
{
	AXRNA_EntryErrFunc(func, obj);
}

void ADXRNA_Finish(void)
{
	AXRNA_Finish();
}

void ADXRNA_Init(void)
{
	AXRNA_Init();
}
