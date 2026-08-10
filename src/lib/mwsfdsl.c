#include "cri_xpt.h"
#include <stdio.h>

typedef struct {
	Uint8 pad0[0x40];
	void *sfd;              /* 0x40 */
	Uint8 pad44[8];
	void *lsc;              /* 0x4C */
	Uint8 pad50[0x74 - 0x50];
	Sint8 linkstm;          /* 0x74 */
	Sint8 linkstm_req;      /* 0x75 */
} MWPLY_OBJ;

extern Bool MWSFD_IsEnableHndl(MWPLY_OBJ *mwply);
extern void MWSFSVM_Error(const Char8 *fmt, ...);
extern Sint32 SFD_SetConcatPlay(void *sfd);
extern void LSC_SetFlowLimit(void *lsc, Sint32 nsct);
extern Sint32 LSC_GetStat(void *lsc);
extern void mwPlyEntryFnameSub(MWPLY_OBJ *mwply, const Char8 *fname);
extern Sint32 mwPlyEntryFnameCore(MWPLY_OBJ *mwply, const Char8 *fname);
extern void mwPlyStartSub(MWPLY_OBJ *mwply);
extern void mwPlyReleaseSub(MWPLY_OBJ *mwply);

/* Every mwPly*Lp / seamless / AFS entry point of this unit was dead-stripped by the linker; the
 * bodies below only reproduce their error strings in .rodata. */

static const Char8 mwsfdsl_fname_fmt[] = "%08x.%08x";

void mwPlyStartFnameLp(MWPLY_OBJ *mwply, const Char8 *fname)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122630: mwPlyStartFnameLp: handle is invalid.");
		return;
	}
	if (fname == NULL) {
		MWSFSVM_Error("E10915A: mwPlyStartFnameLp: fname is NULL.");
		return;
	}
	mwPlyEntryFnameSub(mwply, fname);
	mwPlyStartSub(mwply);
}

void mwPlyEntryFname(MWPLY_OBJ *mwply, const Char8 *fname)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122633: mwPlyEntryFname: handle is invalid.");
		return;
	}
	if (fname == NULL) {
		MWSFSVM_Error("E10915B: mwPlyEntryFname: fname is NULL.");
		return;
	}
	if (mwPlyEntryFnameCore(mwply, fname) != 0) {
		MWSFSVM_Error("E204021: mwPlyEntryFname: Can't entry file'%s'", fname);
	}
}

void mwPlySetLpFlg(MWPLY_OBJ *mwply, Sint32 flg)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122641: mwPlySetLpFlg: handle is invalid.");
		return;
	}
	mwply->linkstm_req = flg;
}

void mwPlyStartSeamless(MWPLY_OBJ *mwply)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122634: mwPlyStartSeamless: handle is invalid.");
		return;
	}
	mwPlyStartSub(mwply);
}

void mwPlyLinkStm(MWPLY_OBJ *mwply, Sint32 sw)
{
	void *sfd;

	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122642: mwPlyLinkStm: handle is invalid.");
		return;
	}
	sfd = mwply->sfd;
	if (mwply->linkstm == 1 && sw == 0) {
		mwply->linkstm_req = 1;
	}
	if (mwply->linkstm == 0 && sw == 1) {
		if (SFD_SetConcatPlay(sfd) != 0) {
			MWSFSVM_Error("E99072101 mwPlyLinkStm: can't link stream");
		}
	}
	mwply->linkstm = sw;
}

const Char8 *mwPlyGetSlFname(MWPLY_OBJ *mwply, Sint32 stm_no, Char8 *buf)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122637: mwPlyGetSlFname: handle is invalid.");
		return NULL;
	}
	if (stm_no < 0) {
		MWSFSVM_Error("E10821B : Invalid value of stm_no : %d", stm_no);
		return NULL;
	}
	sprintf(buf, mwsfdsl_fname_fmt, stm_no, stm_no);
	return buf;
}

void mwPlyStartFnameRangeLp(MWPLY_OBJ *mwply, const Char8 *fname, Sint32 ofst, Sint32 nsct)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E407024: mwPlyStartFnameRangeLp: handle is invalid.");
		return;
	}
	mwPlyEntryFnameSub(mwply, fname);
	mwPlyStartSub(mwply);
}

void mwPlyEntryFnameRange(MWPLY_OBJ *mwply, const Char8 *fname, Sint32 ofst, Sint32 nsct)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E407023: mwPlyEntryFnameRange: handle is invalid.");
		return;
	}
	mwPlyEntryFnameSub(mwply, fname);
}

void mwPlyStartAfsLp(MWPLY_OBJ *mwply, Sint32 pid, Sint32 fid)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122632: mwPlyStartAfsLp: handle is invalid.");
		return;
	}
	mwPlyStartSub(mwply);
}

void mwPlyEntryAfs(MWPLY_OBJ *mwply, Sint32 pid, Sint32 fid)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122636: mwPlyEntryAfs: handle is invalid.");
		return;
	}
	if (mwPlyEntryFnameCore(mwply, NULL) != 0) {
		MWSFSVM_Error("E008311 mwPlyEntryAfs: can't entry pid=%d fid=%d", pid, fid);
	}
}

void mwPlyReleaseSeamless(MWPLY_OBJ *mwply)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122635: mwPlyReleaseSeamless: handle is invalid.");
		return;
	}
	mwPlyReleaseSub(mwply);
}

void mwPlyReleaseLp(MWPLY_OBJ *mwply)
{
	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E1122631: mwPlyReleaseLp: handle is invalid.");
		return;
	}
	mwPlyReleaseSub(mwply);
}

void MWSFLSC_SetFlowLimit(MWPLY_OBJ *mwply, Sint32 nsct)
{
	if (mwply->lsc != NULL) {
		LSC_SetFlowLimit(mwply->lsc, nsct);
	}
}

Bool MWSFLSC_IsFsStatErr(void *lsc)
{
	return LSC_GetStat(lsc) == 3;
}
