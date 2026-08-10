/* Sofdec: playback control (speed / standby / pause) */
#include "cri_xpt.h"
#include "sfd.h"

#define SFD_REQ_STANDBY 3
#define SFD_REQ_START 4
#define SFD_STAT_PLAYING 4

Sint32 SFD_SetSpeed(SFD sfd, Sint32 speed)
{
	if (SFLIB_CheckHn(sfd) != 0) {
		return SFLIB_SetErr(NULL, 0xFF000144);
	}
	SFTIM_SetSpeed(sfd, speed);
	SFAOAP_SetSpeed(sfd, speed);
	return 0;
}

Sint32 SFPL2_Standby(SFD sfd)
{
	sfd->req = SFD_REQ_STANDBY;
	return 0;
}

Sint32 SFD_Standby(SFD sfd)
{
	if (SFLIB_CheckHn(sfd) != 0) {
		return SFLIB_SetErr(NULL, 0xFF000143);
	}
	SFPL2_Standby(sfd);
	return 0;
}

static Sint32 sfpl2_PauseSub(SFD sfd, Sint32 sw)
{
	Sint32 ret;
	Sint32 r;

	if (sfd->req != SFD_REQ_STANDBY && sfd->req != SFD_REQ_START) {
		ret = 0;
	} else {
		SFTIM_Pause(sfd, sw);
		r = SFTRN_CallTrtTrif(sfd, 7, 8, sw, 0);
		ret = 0;
		if (r != 0) {
			ret = r;
		}
	}
	return ret;
}

Sint32 SFPL2_Pause(SFD sfd, Sint32 sw)
{
	Sint32 ret;

	ret = 0;
	switch (sw) {
	case 2:
		if (sfd->stat == SFD_STAT_PLAYING) {
			ret = sfpl2_PauseSub(sfd, 2);
		}
		break;
	case 1:
		if (sfd->pause_cnt++ == 0) {
			ret = sfpl2_PauseSub(sfd, 1);
		}
		break;
	case 0:
		if (--sfd->pause_cnt == 0) {
			ret = sfpl2_PauseSub(sfd, 0);
		}
		break;
	}
	return ret;
}

Sint32 SFD_Pause(SFD sfd, Sint32 sw)
{
	Sint32 psw;
	Sint32 mode;
	Sint32 ret;

	if (SFLIB_CheckHn(sfd) != 0) {
		return SFLIB_SetErr(NULL, 0xFF000142);
	}
	psw = sfd->pause_sw;
	if (sw == 0) {
		if (psw == 0) {
			return 0;
		}
		mode = 0;
	} else {
		if (psw == 0) {
			mode = 1;
		} else {
			mode = 2;
		}
	}
	sfd->pause_sw = sw;
	ret = SFPL2_Pause(sfd, mode);
	sfd->chg_flg = 1;
	return ret;
}
