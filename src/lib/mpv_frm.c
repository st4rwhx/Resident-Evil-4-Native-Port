#include "mpv.h"

typedef struct {
	Uint32 pad[2];
	Uint32 gqr[6];
} UTY_GQR;

extern void UTY_PushGqr(UTY_GQR *gqr);
extern void UTY_PopGqr(UTY_GQR *gqr);
extern void MPVUMC_SetGqr(void);
extern void DCT_FsriSetGqr(void);
extern Sint32 MPVM2V_DecodeFrm(MPV_OBJ *mpv, SJ sj, MPV_FRM *frm);
extern void MPVUMC_InitOutRfb(MPV_OBJ *mpv);
extern void MPVCMC_InitMcOiRt(MPV_OBJ *mpv);
extern void MPVCMC_SetCcnt(MPV_OBJ *mpv);
extern void MPVCDEC_InitFrm(MPV_OBJ *mpv);
extern Sint32 MPVHDEC_DecPicture(MPV_OBJ *mpv, SJ sj);
extern void MPVUMC_EndOfFrame(MPV_OBJ *mpv);
extern Sint32 MPV_GoNextDelimSj(SJ sj);
extern Sint32 MPV_MoveChunk(SJ sj, Sint32 id, Sint32 nbyte);

Sint32 MPV_SkipFrmSj(MPV hn, SJ sj)
{
	MPV_OBJ *mpv = (MPV_OBJ *)hn;
	Sint32 code;
	Sint32 delim;

	if (MPVLIB_CheckHn(hn) != 0) {
		return MPVERR_SetCode(NULL, 0xFF03020A);
	}
	code = 0xFF030305;
	for (;;) {
		delim = MPV_GoNextDelimSj(sj);
		if (delim == 0) {
			break;
		}
		if (delim & 0xCC) {
			code = 0;
			break;
		}
		if (MPV_MoveChunk(sj, 1, 4) != 4) {
			break;
		}
	}
	return MPVERR_SetCode(mpv, code);
}

Sint32 MPV_DecodeFrmSj(MPV hn, SJ sj, MPV_FRM *frm)
{
	MPV_OBJ *mpv = (MPV_OBJ *)hn;
	UTY_GQR gqr;
	Sint32 nfrm, nbyte;
	Sint32 ret;

	if (MPVLIB_CheckHn(hn) != 0) {
		return MPVERR_SetCode(NULL, 0xFF030209);
	}
	UTY_PushGqr(&gqr);
	MPVUMC_SetGqr();
	DCT_FsriSetGqr();
	if (mpv->m2v_mode == 2) {
		return MPVM2V_DecodeFrm(mpv, sj, frm);
	}
	nfrm = mpv->nfrm_dec;
	nbyte = mpv->nbyte_dec;
	mpv->frm = *frm;
	MPVUMC_InitOutRfb(mpv);
	MPVCMC_InitMcOiRt(mpv);
	MPVCMC_SetCcnt(mpv);
	MPVCDEC_InitFrm(mpv);
	ret = MPVHDEC_DecPicture(mpv, sj);
	MPVUMC_EndOfFrame(mpv);
	*frm->picatr = mpv->picatr;
	frm->nfrm = mpv->nfrm_dec - nfrm;
	frm->nbyte = mpv->nbyte_dec - nbyte;
	UTY_PopGqr(&gqr);
	return ret;
}

void MPVFRM_Init(void)
{
}
