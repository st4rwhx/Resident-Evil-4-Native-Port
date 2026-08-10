#include "mpv.h"

Sint32 MPV_GetLinkFlg(MPV_OBJ *mpv, Sint32 *flg1, Sint32 *flg2)
{
	if (MPVLIB_CheckHn(mpv) != 0) {
		return MPVERR_SetCode(NULL, 0xFF03020E);
	}
	*flg1 = mpv->linkflg1;
	*flg2 = mpv->linkflg2;
	return 0;
}

Sint32 MPV_GetVbvBufSiz(MPV_OBJ *mpv, Sint32 *bufsiz, Sint32 *delay, Sint32 *delay_byte)
{
	if (MPVLIB_CheckHn(mpv) != 0) {
		return MPVERR_SetCode(NULL, 0xFF03020F);
	}
	*bufsiz = mpv->vbv_size << 11;
	*delay = mpv->vbv_delay;
	if (mpv->bitrate == 0x3FFFF) {
		*delay_byte = -1;
	} else {
		*delay_byte = (mpv->vbv_delay * mpv->bitrate) / 1800;
	}
	return 0;
}

Sint32 MPV_GetBitRate(MPV_OBJ *mpv, Sint32 *bitrate)
{
	if (MPVLIB_CheckHn(mpv) != 0) {
		return MPVERR_SetCode(NULL, 0xFF03020D);
	}
	*bitrate = mpv->bitrate;
	return 0;
}

Sint32 MPV_GetPicAtr(MPV_OBJ *mpv, MPV_PICATR *picatr)
{
	if (MPVLIB_CheckHn(mpv) != 0) {
		return MPVERR_SetCode(NULL, 0xFF03020C);
	}
	*picatr = mpv->picatr;
	return 0;
}
