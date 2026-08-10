/* Sofdec video driver: decoded frame pool management (standby / reference / free frames). */
#include "sfd.h"

#define SFMPVF_MPV(sfd) ((SFMPV_WORK *)(sfd)->tr[2].hn)

/* frame `a` is displayed before frame `b` (a NULL `b` always loses) */
static Bool sfmpvf_IsPrior(SFMPV_FRM *a, SFMPV_FRM *b)
{
	if (b == NULL) {
		return TRUE;
	}
	if (a->gopno < b->gopno) {
		return TRUE;
	}
	if (a->gopno > b->gopno) {
		return FALSE;
	}
	if (a->x8c < b->x8c) {
		return TRUE;
	}
	if (a->x8c > b->x8c) {
		return FALSE;
	}
	if (a->x88 < b->x88) {
		return TRUE;
	}
	if (a->x88 > b->x88) {
		return FALSE;
	}
	if (a->tmpref - b->tmpref > 0x200) {
		return TRUE;
	}
	if (b->tmpref - a->tmpref > 0x200) {
		return FALSE;
	}
	if (a->tmpref < b->tmpref) {
		return TRUE;
	}
	return FALSE;
}

/* the two earliest standby/reference frames */
void sfmpvf_SearchStbyFrm(SFD sfd, SFMPV_FRM **frm1, SFMPV_FRM **frm2)
{
	SFMPV_WORK *mpv;
	SFMPV_FRM *frm;
	Sint32 nfrm;
	Sint32 i;
	Sint32 n;

	mpv = SFMPVF_MPV(sfd);
	n = 0;
	nfrm = mpv->nfrm;
	frm = mpv->frm;
	*frm1 = NULL;
	*frm2 = NULL;
	for (i = 0; i < nfrm; i++, frm++) {
		if (frm->stat == SFMPV_FRM_STBY || frm->stat == SFMPV_FRM_REF) {
			n++;
			if (sfmpvf_IsPrior(frm, *frm1)) {
				*frm2 = *frm1;
				*frm1 = frm;
			} else if (sfmpvf_IsPrior(frm, *frm2)) {
				*frm2 = frm;
			}
		}
	}
	if (mpv->termflg == 0) {
		n--;
	}
	if (n <= 0) {
		*frm1 = NULL;
		*frm2 = NULL;
		return;
	}
	if (n == 1) {
		*frm2 = NULL;
	}
}

/* the original did not inline sfmpvf_SearchStbyFrm here */
#pragma dont_inline on
SFMPV_FRM *sfmpvf_ReferNextFrmReady(SFD sfd)
{
	SFMPV_FRM *frm1;
	SFMPV_FRM *frm;
	Sint32 cs;

	SFLIB_LockCs(&cs);
	if (sfd->stat != 4) {
		frm = NULL;
	} else {
		sfmpvf_SearchStbyFrm(sfd, &frm1, &frm);
		if (frm != NULL) {
			if (SFSET_GetCond(sfd, 15) != 0) {
				if (SFTIM_IsGetFrmTimeTunit(sfd, frm->ftime, frm->tunit) == 0) {
					frm = NULL;
				}
			}
		}
	}
	SFLIB_UnlockCs(&cs);
	return frm;
}
#pragma dont_inline off

Bool SFD_IsNextFrmReady(SFD sfd)
{
	if (SFLIB_CheckHn(sfd) != 0) {
		SFLIB_SetErr(NULL, 0xFF000183);
		return FALSE;
	}
	return sfmpvf_ReferNextFrmReady(sfd) != NULL;
}

/* the earliest standby frame; *lastflg is set when it is the last one of a terminated stream */
SFMPV_FRM *SFMPVF_HoldFrm(SFD sfd, Sint32 *lastflg)
{
	Sint32 nfrm;
	Sint32 n;
	SFMPV_FRM *frm;
	SFMPV_WORK *mpv;
	SFMPV_FRM *hold;
	Sint32 i;
	Sint32 cs;

	SFLIB_LockCs(&cs);
	mpv = SFMPVF_MPV(sfd);
	hold = NULL;
	n = 0;
	nfrm = mpv->nfrm;
	frm = mpv->frm;
	*lastflg = 0;
	for (i = 0; i < nfrm; i++, frm++) {
		if (frm->stat == SFMPV_FRM_STBY || frm->stat == SFMPV_FRM_REF) {
			n++;
			if (sfmpvf_IsPrior(frm, hold)) {
				hold = frm;
			}
		}
	}
	if (n == 1) {
		if (mpv->termflg != 0) {
			*lastflg = 1;
		} else if (mpv->gopstat == 0) {
			hold = NULL;
		}
	}
	SFLIB_UnlockCs(&cs);
	return hold;
}

void SFMPVF_EndRefFrm(SFMPV_FRM *frm)
{
	if (frm == NULL) {
		return;
	}
	if (frm->stat == SFMPV_FRM_REF) {
		frm->stat = SFMPV_FRM_STBY;
	} else {
		frm->stat = SFMPV_FRM_FREE;
	}
}

void SFMPVF_EndDrawFrm(SFMPV_FRM *frm)
{
	if (frm == NULL) {
		return;
	}
	if (frm->stat == SFMPV_FRM_REF) {
		frm->stat = SFMPV_FRM_DRAWN;
	} else {
		frm->stat = SFMPV_FRM_FREE;
	}
}

void SFMPVF_RefStbyFrm(SFMPV_FRM *frm)
{
	if (frm == NULL) {
		return;
	}
	frm->stat = SFMPV_FRM_REF;
}

void SFMPVF_StbyFrm(SFMPV_FRM *frm)
{
	if (frm == NULL) {
		return;
	}
	frm->stat = SFMPV_FRM_STBY;
}

void SFMPVF_FreeFrm(SFMPV_FRM *frm)
{
	if (frm == NULL) {
		return;
	}
	frm->stat = SFMPV_FRM_FREE;
}

SFMPV_FRM *SFMPVF_AllocFrm(SFD sfd)
{
	SFMPV_FRM *frm;
	Sint32 i;
	SFMPV_WORK *mpv;
	Sint32 nfrm;
	Sint32 cs;

	SFLIB_LockCs(&cs);
	mpv = SFMPVF_MPV(sfd);
	i = 0;
	nfrm = mpv->nfrm;
	frm = mpv->frm;
	for (; i < nfrm; i++, frm++) {
		if (frm->stat == SFMPV_FRM_FREE && frm->lock == 0) {
			frm->stat = SFMPV_FRM_ALLOC;
			break;
		}
	}
	if (i == nfrm) {
		frm = NULL;
	}
	SFLIB_UnlockCs(&cs);
	return frm;
}

Sint32 SFMPVF_GetNumFrm(SFD sfd)
{
	SFMPV_WORK *mpv;
	SFMPV_FRM *frm;
	Sint32 i;
	Sint32 n;
	Sint32 cs;

	SFLIB_LockCs(&cs);
	mpv = SFMPVF_MPV(sfd);
	n = 0;
	frm = mpv->frm;
	for (i = 0; i < mpv->nfrm; i++, frm++) {
		if (frm->stat == SFMPV_FRM_STBY || frm->stat == SFMPV_FRM_REF) {
			n++;
		}
	}
	if (mpv->termflg == 1 && n == 0) {
		n = -1;
	}
	SFLIB_UnlockCs(&cs);
	return n;
}

void SFMPVF_SetGopStat(SFD sfd, Sint32 stat)
{
	SFMPVF_MPV(sfd)->gopstat = stat;
}

Sint32 SFMPVF_IsTermDec(SFD sfd)
{
	return SFMPVF_MPV(sfd)->termflg;
}

void SFMPVF_TermDec(SFD sfd)
{
	SFMPVF_MPV(sfd)->termflg = 1;
}

SFD_VFRM *SFMPVF_SearchVfrmData(SFD sfd, SFMPV_FRM *frm)
{
	Sint32 i;
	SFMPV_WORK *mpv;
	SFMPV_FRM *p;

	mpv = SFMPVF_MPV(sfd);
	p = mpv->frm;
	for (i = 0; i < mpv->nfrm; i++, p++) {
		if (p == frm) {
			return &sfd->vfrm[i];
		}
	}
	return NULL;
}

SFMPV_FRM *SFMPVF_SearchFrmObj(SFD sfd, SFD_VFRM_INF *inf)
{
	Sint32 i;
	SFMPV_FRM *frm;
	SFD_VFRM *vf;

	frm = SFMPVF_MPV(sfd)->frm;
	vf = sfd->vfrm;
	for (i = 0; i < SFD_VFRM_NUM; i++, vf++) {
		if (&vf->inf == inf) {
			return &frm[i];
		}
	}
	return NULL;
}
