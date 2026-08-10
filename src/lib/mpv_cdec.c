/* Sofdec MPEG video: macroblock coefficient decode (six 8x8 blocks) */
#include "cri_xpt.h"
#include "mpv.h"

void DCT_FsriTransCbp(Sint8 *cbp);
void DCT_FsriTrans6Blk(Sint8 *cbp);

#define MPVCDEC_NBLK 6

/* clear one 8x8 block (0x100 bytes) with 32 8-byte stores through a stepping cursor */
static inline void mpvcdec_ClearBlk(Float64 **cur)
{
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
	*(*cur)++ = 0.0;
}

Sint32 MPVCDEC_NintraBlocks(MPV mpv)
{
	Sint32 msk;
	Sint32 i;
	MPV_BLKPRM *prm;
	Sint8 *cbp;

	cbp = mpv->cbp;
	prm = &mpv->blkprm;
	prm->qscale = mpv->qscale;
	prm->iqm = mpv->nintra_iqm;
	prm->nintra = 1;
	msk = mpv->cbp_code << 2;
	mpv->cbp_msk = msk;
	for (i = 0; i < MPVCDEC_NBLK; i++) {
		if (msk < 0) {
			prm->dst = mpv->blk[i];
			cbp[i] = mpv->nintra_func(mpv, prm);
		}
		msk <<= 1;
	}
	DCT_FsriTransCbp(cbp);
	return 0;
}

Sint32 MPVCDEC_IntraBlocks(MPV mpv)
{
	MPV_BLKPRM *prm;
	Sint8 *cbp;

	{
		Float64 *cur = mpv->blk[0];
		mpvcdec_ClearBlk(&cur);
		mpvcdec_ClearBlk(&cur);
		mpvcdec_ClearBlk(&cur);
		mpvcdec_ClearBlk(&cur);
		mpvcdec_ClearBlk(&cur);
		mpvcdec_ClearBlk(&cur);
	}
	prm = &mpv->blkprm;
	cbp = mpv->cbp;
	prm->qscale = mpv->qscale;
	prm->iqm = mpv->intra_iqm;
	prm->nintra = 0;
	prm->dctbl = mpv->dctbl_y;
	prm->dcpred = &mpv->dcpred[0];
	prm->dst = mpv->blk[0];
	cbp[0] = mpv->intra_func(mpv, prm);
	prm->dst = mpv->blk[1];
	cbp[1] = mpv->intra_func(mpv, prm);
	prm->dst = mpv->blk[2];
	cbp[2] = mpv->intra_func(mpv, prm);
	prm->dst = mpv->blk[3];
	cbp[3] = mpv->intra_func(mpv, prm);
	prm->dctbl = mpv->dctbl_c;
	prm->dcpred = &mpv->dcpred[1];
	prm->dst = mpv->blk[4];
	cbp[4] = mpv->intra_func(mpv, prm);
	prm->dcpred = &mpv->dcpred[2];
	prm->dst = mpv->blk[5];
	cbp[5] = mpv->intra_func(mpv, prm);
	DCT_FsriTrans6Blk(cbp);
	return 0;
}

void MPVCDEC_InitFrm(MPV mpv)
{
}
