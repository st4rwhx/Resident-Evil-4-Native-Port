#include "mwsfd.h"
#include "sfd.h"

/* This unit is compiled with `-inline auto,deferred` (CRI_CFLAG_OVERRIDES): every accessor inlines
 * MWSFD_IsEnableHndl / mwPlyGetSfdHn / mwPlyGetNumSkipDec, which are defined at the top of the file,
 * and deferred code generation emits the functions in reverse source order (the DOL's .text and
 * string order). Most mwPly* accessors were dead-stripped by the linker; their bodies below only
 * reproduce the error strings and constants left in .rodata (STRIP_UNUSED). */

void *mwPlyGetSfdHn(MWPLY mwply)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122640: mwPlyGetSfdHn: handle is invalid.");
		return NULL;
	}
	return mwply->sfd;
}

Sint32 mwPlyGetNumSkipDec(MWPLY mwply)
{
	SFD_PLYINF inf;

	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122619: mwPlyGetNumSkipDec: handle is invalid.");
		return 0;
	}
	SFD_GetPlyInf(mwPlyGetSfdHn(mwply), &inf);
	return inf.raw[1] - inf.raw[4];
}

void MWSFD_SetCond(MWPLY mwply, Sint32 id, Sint32 val)
{
	SFD_SetCond((mwply != NULL) ? mwply->sfd : NULL, id, val);
}

void MWSFD_SetFlowLimit(MWPLY mwply, Sint32 min_nsct, Sint32 max_nsct)
{
	MWSTM_SetFlowLimit(mwply->stm, min_nsct, max_nsct);
	MWSFLSC_SetFlowLimit(mwply, min_nsct);
}

Bool MWSFD_IsEnableHndl(MWPLY mwply)
{
	if (mwply == NULL) {
		return FALSE;
	}
	return mwply->used;
}

void mwPlySetAudioSw(MWPLY mwply, Sint32 sw)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122617 mwPlySetAudioSw: handle is invalid.");
		return;
	}
	SFD_SetCond(mwply->sfd, 28, sw);
}

void *mwPlyGetAdxtHn(MWPLY mwply)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122639: mwPlyGetAdxtHn: handle is invalid.");
		return NULL;
	}
	return NULL;
}

void mwPlySetPpicSkip(MWPLY mwply, Sint32 sw)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122628: mwPlySetBpicSkip mwPlySetPpicSkip: handle is invalid.");
		return;
	}
	SFD_SetCond(mwply->sfd, 27, sw);
}

void mwPlySetBpicSkip(MWPLY mwply, Sint32 sw)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122628: mwPlySetBpicSkip mwPlySetPpicSkip: handle is invalid.");
		return;
	}
	SFD_SetCond(mwply->sfd, 26, sw);
}

void mwPlySetAudioCh(MWPLY mwply, Sint32 ch)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122616 mwPlySetAudioCh: handle is invalid.");
		return;
	}
	if (ch < 0 || ch >= SFSET_GetCond(mwply->sfd, 24)) {
		MWSFSVM_Error("E10911A mwPlySetAudioCh: Invalid ch no.");
		return;
	}
	SFD_SetCond(mwply->sfd, 25, ch);
}

Sint32 mwPlyGetNumAudioCh(MWPLY mwply)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E2080801 mwPlyGetNumAudioCh: handle is invalid.");
		return 0;
	}
	return SFSET_GetCond(mwply->sfd, 24);
}

void mwPlySetVideoCh(MWPLY mwply, Sint32 ch)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E2080601 mwPlySetVideoCh: handle is invalid.");
		return;
	}
	if (ch < 0 || ch >= SFSET_GetCond(mwply->sfd, 22)) {
		MWSFSVM_Error("E2080602 mwPlySetVideoCh: Invalid ch no.");
		return;
	}
	SFD_SetCond(mwply->sfd, 23, ch);
}

Sint32 mwPlyGetNumVideoCh(MWPLY mwply)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E2080802 mwPlyGetNumVideoCh: handle is invalid.");
		return 0;
	}
	return SFSET_GetCond(mwply->sfd, 22);
}

void mwPlySetLimitTime(MWPLY mwply, Sint32 time)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122625: mwPlySetLimitTime: handle is invalid.");
		return;
	}
	SFD_SetCond(mwply->sfd, 21, time);
}

Sint32 mwSfdGetStat(MWPLY mwply)
{
	Sint32 stat;
	Sint32 sfdstat;

	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFLIB_SetErrCode(-12);
		MWSFSVM_Error("W2004 mwSfdGetStat: handle is invalid");
		return MWSFD_STAT_STOP;
	}
	stat = mwply->stat;
	if (stat == MWSFD_STAT_PLAYING) {
		sfdstat = SFD_GetHnStat(mwply->sfd);
		if (sfdstat == 4 || sfdstat == 6) {
			return MWSFD_STAT_PLAYING;
		}
		if (sfdstat < 0) {
			return MWSFD_STAT_ERROR;
		}
		return MWSFD_STAT_PREP;
	}
	return stat;
}

void mwPlySetSyncMode(MWPLY mwply, Sint32 mode)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122626: mwPlySetSyncMode: handle is invalid.");
		return;
	}
	SFD_SetCond(mwply->sfd, 9, mode);
}

Sint32 mwPlyGetSyncMode(MWPLY mwply)
{
	Sint32 mode;

	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E2010802: mwPlyGetSyncMode: handle is invalid.");
		return 0;
	}
	mode = SFSET_GetCond(mwply->sfd, 9);
	if (mode < 0 || mode > 2) {
		MWSFSVM_Error("E2010803: mwPlyGetSyncMode: mode is invalid.");
		return 0;
	}
	return mode;
}

Sint32 mwPlyGetNumDecPool(MWPLY mwply)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122621: mwPlyGetNumDecPool: handle is invalid.");
		return 0;
	}
	MWSFSVM_Error("E1121601 mwPlyGetNumDecPool");
	return 0;
}

Sint32 mwPlyGetNumTotalDec(MWPLY mwply)
{
	SFD_PLYINF inf;

	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122620: mwPlyGetNumTotalDec: handle is invalid.");
		return 0;
	}
	SFD_GetPlyInf(mwply->sfd, &inf);
	return inf.raw[1];
}

void mwSfdGetTime(MWPLY mwply, Sint32 *ncount, Sint32 *tscale)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122603 mwSfdGetTime; handle is invalid.");
		*ncount = 0;
		*tscale = 1;
		return;
	}
	if (SFD_GetTime(mwply->sfd, ncount, tscale) != 0) {
		MWSFLIB_SetErrCode(-0x135);
		MWSFSVM_Error("E2006 mwSfdGetTime; can't get time");
		*ncount = 0;
		*tscale = 1;
	}
	if (*ncount < 0) {
		*ncount = 0;
		*tscale = 1;
	}
}

void mwSfdSetOutVol(MWPLY mwply, Sint32 vol)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122605 mwSfdSetOutVol: handle is invalid.");
		return;
	}
	MWSFRNA_SetOutVol(mwply, vol);
	MWSST_SetOutVol(&mwply->sst, vol);
}

Sint32 mwSfdGetOutVol(MWPLY mwply)
{
	Sint32 vol;
	Sint32 sstvol;

	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122606 mwSfdGetOutVol: handle is invalid.");
		return 0;
	}
	vol = MWSFRNA_GetOutVol(mwply);
	sstvol = MWSST_GetOutVol(&mwply->sst);
	if (vol == sstvol) {
		return vol;
	}
	if (vol != 0) {
		return vol;
	}
	return sstvol;
}

void mwSfdSetOutPan(MWPLY mwply, Sint32 ch, Sint32 pan)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122607 mwSfdSetOutPan: handle is invalid.");
		return;
	}
	MWSFRNA_SetOutPan(mwply, ch, pan);
}

Sint32 mwSfdGetOutPan(MWPLY mwply, Sint32 ch)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122608 mwSfdGetOutPan: handle is invalid.");
		return 0;
	}
	return MWSFRNA_GetOutPan(mwply, ch);
}

void mwPlySetEmptyBpicSkip(MWPLY mwply, Sint32 sw)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122627: mwPlySetEmptyBpicSkip: handle is invalid.");
		return;
	}
	SFD_SetCond(mwply->sfd, 20, sw);
}

SJ mwPlyGetInputSj(MWPLY mwply)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122624: mwPlyGetInputSj: handle is invalid.");
		return NULL;
	}
	return mwply->sji;
}

Sint32 mwPlyGetNumSkipEmptyB(MWPLY mwply)
{
	SFD_PLYINF inf;

	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122623: mwPlyGetNumSkipEmptyB: handle is invalid.");
		return 0;
	}
	SFD_GetPlyInf(mwply->sfd, &inf);
	return inf.raw[5];
}

Sint32 mwPlyGetNumDropFrm(MWPLY mwply)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E202232: mwPlyGetNumDropFrm: handle is invalid.");
		return 0;
	}
	return mwPlyGetNumSkipDec(mwply) + mwPlyGetNumSkipEmptyB(mwply);
}

void mwPlyGetPlyInf(MWPLY mwply, SFD_PLYINF *inf)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E202191: mwPlyGetPlyInf: handle is invalid.");
		return;
	}
	SFD_GetPlyInf(mwply->sfd, inf);
}

void mwPlyGetFlowInf(MWPLY mwply, Sint32 *inf)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122643: mwPlyGetFlowInf: handle is invalid.");
		return;
	}
	*inf = 0;
}

void mwPlySetSpeed(MWPLY mwply, Sint32 speed)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E302251: mwPlySetSpeed: handle is invalid.");
		return;
	}
	SFD_SetSpeed(mwply->sfd, speed);
}

void mwPlySetFloatSpeed(MWPLY mwply, Float32 speed)
{
	SFD sfd;
	Sint32 ispeed;

	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E211051: mwPlySetFloatSpeed: handle is invalid.");
		return;
	}
	sfd = mwPlyGetSfdHn(mwply);
	ispeed = (Sint32)(speed * 1000.0f + 0.5f);
	if ((Float32)ispeed != speed * 1000.0f) {
		SFD_SetSpeed(sfd, ispeed);
	}
}
