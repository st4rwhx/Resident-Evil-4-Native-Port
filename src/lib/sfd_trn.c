/* Sofdec: transfer driver (tr) table management */
#include "cri_xpt.h"
#include "sfd.h"

#define SFTRN_TRIF_NUM 15

Sint32 SFTRN_IsSetup(SFD sfd, Sint32 strm)
{
	return sfd->tr[strm].trif != NULL;
}

Sint32 SFTRN_GetTermFlg(SFD sfd, Sint32 strm)
{
	return sfd->tr[strm].termflg;
}

void SFTRN_SetTermFlg(SFD sfd, Sint32 strm, Sint32 flg)
{
	sfd->tr[strm].termflg = flg;
}

Sint32 SFTRN_GetPrepFlg(SFD sfd, Sint32 strm)
{
	return sfd->tr[strm].prepflg;
}

void SFTRN_SetPrepFlg(SFD sfd, Sint32 strm, Sint32 flg)
{
	sfd->tr[strm].prepflg = flg;
}

Sint32 SFTRN_CallTrtTrif(SFD sfd, Sint32 strm, Sint32 fn, Sint32 a, Sint32 b)
{
	SFD_TR_FUNC *trif;

	trif = sfd->tr[strm].trif;
	if (trif == NULL) {
		return 0;
	}
	return trif[fn](sfd, a, b, 0);
}

Sint32 SFTRN_CallTrSetup(SFD sfd, Sint32 fn)
{
	Sint32 i;
	SFD_TR *tr;
	Sint32 ret;

	ret = 0;
	tr = sfd->tr;
	for (i = 0; i < SFD_TR_NUM; i++, tr++) {
		if (tr->trif != NULL) {
			ret = tr->trif[fn](sfd, 0, 0, 0);
			if (ret != 0) {
				break;
			}
		}
	}
	return ret;
}

void sftrn_BuildSystem(SFD sfd, SFD_TR_FUNC **tbl)
{
	sfd->buf[0].out_tr = 1;
	sfd->tr[1].bufin = 0;
	if (tbl[2] != NULL) {
		sfd->tr[1].bufout = 1;
		sfd->buf[1].in_tr = 1;
		sfd->buf[1].out_tr = 2;
		sfd->tr[2].bufin = 1;
		sfd->tr[2].bufout = 3;
		sfd->buf[3].in_tr = 2;
		if (tbl[4] != NULL) {
			sfd->buf[3].out_tr = 4;
			sfd->tr[4].bufin = 3;
			sfd->tr[4].bufout = 5;
			sfd->buf[5].in_tr = 4;
			sfd->buf[5].out_tr = 6;
			sfd->tr[6].bufin = 5;
		} else {
			sfd->buf[3].out_tr = 6;
			sfd->tr[6].bufin = 3;
		}
	} else {
		SFSET_SetCond(sfd, 5, 0);
		sfd->cond_def[5] = 0;
	}
	if (tbl[3] != NULL) {
		sfd->tr[1].bufout2 = 2;
		sfd->buf[2].in_tr = 1;
		sfd->buf[2].out_tr = 3;
		sfd->tr[3].bufin = 2;
		sfd->tr[3].bufout = 4;
		sfd->buf[4].in_tr = 3;
		if (tbl[5] != NULL) {
			sfd->buf[4].out_tr = 5;
			sfd->tr[5].bufin = 4;
			sfd->tr[5].bufout = 6;
			sfd->buf[6].in_tr = 5;
			sfd->buf[6].out_tr = 7;
			sfd->tr[7].bufin = 6;
		} else {
			sfd->buf[4].out_tr = 7;
			sfd->tr[7].bufin = 4;
		}
	} else {
		SFSET_SetCond(sfd, 6, 0);
		sfd->cond_def[6] = 0;
	}
	if (tbl[8] != NULL) {
		sfd->tr[1].bufout3 = 7;
		sfd->buf[7].in_tr = 1;
		sfd->buf[7].out_tr = 8;
		sfd->tr[8].bufin = 7;
	}
}

static Sint32 sftrn_BuildAll(SFD sfd, SFD_TR_FUNC **tbl)
{
	if (tbl[1] != NULL) {
		sfd->tr[0].bufout = 0;
		sfd->buf[0].in_tr = 0;
		sftrn_BuildSystem(sfd, tbl);
	} else if (tbl[2] != NULL) {
		sfd->tr[0].bufout = 1;
		sfd->buf[1].in_tr = 0;
		sfd->buf[1].out_tr = 2;
		sfd->tr[2].bufin = 1;
		sfd->tr[2].bufout = 3;
		sfd->buf[3].in_tr = 2;
		if (tbl[4] != NULL) {
			sfd->buf[3].out_tr = 4;
			sfd->tr[4].bufin = 3;
			sfd->tr[4].bufout = 5;
			sfd->buf[5].in_tr = 4;
			sfd->buf[5].out_tr = 6;
			sfd->tr[6].bufin = 5;
		} else {
			sfd->buf[3].out_tr = 6;
			sfd->tr[6].bufin = 3;
		}
		SFSET_SetCond(sfd, 6, 0);
		sfd->cond_def[6] = 0;
	} else if (tbl[3] != NULL) {
		sfd->tr[0].bufout = 2;
		sfd->buf[2].in_tr = 0;
		sfd->buf[2].out_tr = 3;
		sfd->tr[3].bufin = 2;
		sfd->tr[3].bufout = 4;
		sfd->buf[4].in_tr = 3;
		if (tbl[5] != NULL) {
			sfd->buf[4].out_tr = 5;
			sfd->tr[5].bufin = 4;
			sfd->tr[5].bufout = 6;
			sfd->buf[6].in_tr = 5;
			sfd->buf[6].out_tr = 7;
			sfd->tr[7].bufin = 6;
		} else {
			sfd->buf[4].out_tr = 7;
			sfd->tr[7].bufin = 4;
		}
		SFSET_SetCond(sfd, 5, 0);
		sfd->cond_def[5] = 0;
	} else if (tbl[8] != NULL) {
		sfd->tr[0].bufout = 7;
		sfd->buf[7].in_tr = 0;
		sfd->buf[7].out_tr = 8;
		sfd->tr[8].bufin = 7;
		SFSET_SetCond(sfd, 6, 0);
		SFSET_SetCond(sfd, 5, 0);
		sfd->cond_def[6] = 0;
		sfd->cond_def[5] = 0;
	} else {
		return -1;
	}
	return 0;
}

Sint32 SFTRN_InitHn(SFD sfd, SFD_TR *tr, SFTRN_PRM *prm)
{
	SFD_TR_FUNC **trif;
	SFD_TR_FUNC **tbl;
	SFD_TR_FUNC *f;
	Sint32 i;

	tbl = prm->trif_tbl;
	trif = tbl;
	for (i = 0; i < SFD_TR_NUM; i++, tr++, trif++) {
		tr->hn = NULL;
		f = *trif;
		tr->termflg = 0;
		tr->prepflg = 0;
		tr->trif = f;
		tr->bufin = 8;
		tr->bufout = 8;
		tr->bufout2 = 8;
		tr->bufout3 = 8;
		tr->x20 = -1;
	}
	if (sftrn_BuildAll(sfd, tbl) != 0) {
		return SFLIB_SetErr(sfd, 0xFF000302);
	}
	return 0;
}

static Sint32 sftrn_CallInit(SFD_TR_IF **p, Sint32 ret)
{
	SFD_TR_IF *trif;
	Sint32 i;

	for (i = 0; i < SFTRN_TRIF_NUM; i++, p++) {
		trif = *p;
		if (trif == NULL) {
			break;
		}
		ret = trif->Init(0, 0, 0, 0);
		if (ret != 0) {
			break;
		}
	}
	return ret;
}

Sint32 SFTRN_Init(SFTRN_TRIF_TBL *dst, SFTRN_TRIF_TBL *src)
{
	Sint32 ret;

	ret = 0;
	*dst = *src;
	ret = sftrn_CallInit(src->tbl, ret);
	return ret;
}
