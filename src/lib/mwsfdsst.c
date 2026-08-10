/* Sofdec MW player: sub-stream (side audio stream) wrapper around the registered core library */
#include "cri_xpt.h"
#include "sj.h"
#include "mwsfd.h"

extern void SFD_SetElementOutSj(void *sfd, void *buf, SJ sj, Sint32 a, Sint32 b);

MWSST_MNG mwsstmng = {0};

/* every interface call is guarded: mwsstmng.ifc and the function pointer may be NULL */
#define MWSST_CALL(func, args) \
	if (mwsstmng.ifc != NULL && mwsstmng.ifc->func != NULL) { \
		mwsstmng.ifc->func args; \
	}

static Bool mwsst_IsValid(MWSST sst)
{
	if (mwsstmng.ifc == NULL) {
		return FALSE;
	}
	if (sst->used != 1) {
		return FALSE;
	}
	if (sst->hn == NULL) {
		return FALSE;
	}
	return TRUE;
}

static void mwsst_Stop(MWSST sst)
{
	MWSST hn;

	if (mwsst_IsValid(sst) == TRUE) {
		hn = sst->hn;
		MWSST_CALL(Stop, (hn));
	}
}

static void mwsst_DestroyHn(MWSST hn)
{
	MWSST_IF *ifc;

	ifc = mwsstmng.ifc;
	if (hn != NULL && ifc != NULL && ifc->Destroy != NULL) {
		ifc->Destroy(hn);
	}
}

static void mwsst_ReleaseLib(void)
{
	MWSST_IF *ifc;

	ifc = mwsstmng.ifc;
	if (ifc != NULL && mwsstmng.cnt != 0) {
		mwsstmng.cnt--;
		if (mwsstmng.cnt == 0 && ifc->Finish != NULL) {
			ifc->Finish();
		}
	}
}

MWSST MWSST_Create(void *work, Sint32 wksize, MWSST_IF *ifc)
{
	MWSST sst;

	if (wksize < 0x100) {
		MWSFSVM_Error("E303111: MWSST_Create: worksize is short.");
		return NULL;
	}
	sst = work;
	sst->sj = SJRBF_Create((Uint8 *)work + 0xC0, wksize - 0xC0, 0);
	if (sst->sj == NULL) {
		MWSFSVM_Error("E303112: MWSST_Create: can't create SJ.");
		return NULL;
	}
	sst->hn = ifc->x0c;
	if (sst->hn == NULL) {
		MWSFSVM_Error("E303112: MWSST_Create: can't create corehn.");
		return NULL;
	}
	sst->used = 1;
	return sst;
}

void MWSST_Destroy(MWSST sst)
{
	MWSST hn;
	SJ sj;

	if (mwsst_IsValid(sst) == TRUE) {
		hn = sst->hn;
		sj = sst->sj;
		if (hn != NULL) {
			MWSFSVM_GotoIdleBorder();
			mwsst_Stop(hn);
			sst->used = 0;
			mwsst_DestroyHn(hn);
			SJ_Destroy(sj);
			sst->hn = NULL;
			mwsst_ReleaseLib();
		}
	}
}

void MWSST_Reset(MWPLY mwply)
{
	void *sfd;
	MWSST sst;
	MWSST hn;
	SJ sj;
	void *buf;

	sfd = mwply->sfd;
	sst = &mwply->sst;
	hn = sst->hn;
	sj = sst->sj;
	buf = sst->buf;
	if (mwsst_IsValid(sst) == TRUE) {
		if (hn != NULL) {
			mwsst_Stop(hn);
		}
		SJ_Reset(sj);
		SFD_SetElementOutSj(sfd, (Uint8 *)buf + 0xC0, sj, 0, 0);
	}
}

Sint32 MWSST_GetOutVol(MWSST sst)
{
	Sint32 vol = 0;
	MWSST hn;

	if (mwsst_IsValid(sst) != TRUE) {
		return 0;
	}
	hn = sst->hn;
	if (mwsstmng.ifc != NULL && mwsstmng.ifc->GetOutVol != NULL) {
		vol = mwsstmng.ifc->GetOutVol(hn);
	}
	return vol;
}

void MWSST_SetOutVol(MWSST sst, Sint32 vol)
{
	MWSST hn;

	if (mwsst_IsValid(sst) == TRUE) {
		hn = sst->hn;
		MWSST_CALL(SetOutVol, (hn, vol));
	}
}

void MWSST_Pause(MWSST sst, Sint32 sw)
{
	MWSST hn;

	if (mwsst_IsValid(sst) == TRUE) {
		hn = sst->hn;
		MWSST_CALL(Pause, (hn, sw));
	}
}

Sint32 MWSST_GetStat(MWSST sst)
{
	Sint32 stat = 0;
	MWSST hn;

	if (mwsst_IsValid(sst) != TRUE) {
		return 0;
	}
	hn = sst->hn;
	if (mwsstmng.ifc != NULL && mwsstmng.ifc->GetStat != NULL) {
		stat = mwsstmng.ifc->GetStat(hn);
	}
	return stat;
}

void MWSST_Stop(MWSST sst)
{
	mwsst_Stop(sst);
}

void MWSST_StartSj(MWSST sst)
{
	MWSST hn;
	SJ sj;

	if (mwsst_IsValid(sst) == TRUE) {
		hn = sst->hn;
		sj = sst->sj;
		MWSST_CALL(StartSj, (hn, sj));
	}
}
