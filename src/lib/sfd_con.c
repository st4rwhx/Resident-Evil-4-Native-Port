/* Sofdec: concatenated playback support */
#include "cri_xpt.h"
#include "sfd.h"

#define SFCON_QUE_NUM 32

Sint32 SFCON_ReadTotSmplQue(SFD sfd, Sint32 *val, Sint32 *last)
{
	SFCON *con = &sfd->con;
	Sint32 cs;
	Sint32 ret;

	SFLIB_LockCs(&cs);
	if (con->que_wr - con->que_rd <= 0) {
		*val = -1;
		ret = 0;
	} else {
		*last = con->tot_last;
		ret = 1;
		*val = con->tot_que[con->que_rd % SFCON_QUE_NUM];
		con->que_rd++;
	}
	SFLIB_UnlockCs(&cs);
	return ret;
}

Sint32 SFCON_WriteTotSmplQue(SFD sfd, Sint32 val, Sint32 last)
{
	SFCON *con = &sfd->con;
	Sint32 cs;
	Sint32 ret;

	SFLIB_LockCs(&cs);
	if (con->que_wr - con->que_rd >= SFCON_QUE_NUM) {
		ret = 0;
	} else {
		con->tot_last = last;
		ret = 1;
		con->tot_que[con->que_wr % SFCON_QUE_NUM] = val;
		con->que_wr++;
	}
	SFLIB_UnlockCs(&cs);
	return ret;
}

void SFCON_UpdateConcatTime(SFD sfd, Sint32 t)
{
	SFCON *con = &sfd->con;
	Sint32 cs;
	Sint32 idx;

	SFLIB_LockCs(&cs);
	con->ctime += t;
	idx = con->ctime_idx;
	idx++;
	con->ctime_que[idx % SFCON_QUE_NUM] = con->ctime;
	con->ctime_idx = idx;
	SFLIB_UnlockCs(&cs);
}

Sint32 SFCON_IsVideoEndcodeSkip(SFD sfd)
{
	if (SFSET_GetCond(sfd, 0x31) != 0 || SFSET_GetCond(sfd, 0x39) != 0) {
		return 1;
	}
	return 0;
}

Sint32 SFCON_IsSystemEndcodeSkip(SFD sfd)
{
	if (SFSET_GetCond(sfd, 0x31) != 0 || SFSET_GetCond(sfd, 0x38) != 0) {
		return 1;
	}
	return 0;
}

Sint32 SFCON_IsEndcodeSkip(SFD sfd)
{
	return SFSET_GetCond(sfd, 0x31) != 0;
}

Sint32 SFD_SetConcatPlay(SFD sfd)
{
	if (SFLIB_CheckHn(sfd) != 0) {
		return SFLIB_SetErr(NULL, 0xFF000161);
	}
	SFSET_SetCond(sfd, 0x31, 1);
	return 0;
}
