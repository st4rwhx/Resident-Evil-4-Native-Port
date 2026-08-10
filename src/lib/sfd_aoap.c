/* Sofdec audio output driver "auto-play" (tr[7]): forwards to the user's callbacks */
#include "cri_xpt.h"
#include "sfd.h"

#define SFAOAP_TR 7
#define SFAOAP_COND 6

Sint32 SFAOAP_Seek(void)
{
	return 0;
}

Sint32 SFAOAP_AddRead(SFD sfd)
{
	return SFLIB_SetErr(sfd, 0xFF000A01);
}

Sint32 SFAOAP_GetRead(SFD sfd)
{
	return SFLIB_SetErr(sfd, 0xFF000A01);
}

Sint32 SFAOAP_AddWrite(SFD sfd)
{
	return SFLIB_SetErr(sfd, 0xFF000A01);
}

Sint32 SFAOAP_GetWrite(SFD sfd)
{
	return SFLIB_SetErr(sfd, 0xFF000A01);
}

static Sint32 sfaoap_ChkRet(Sint32 r)
{
	Sint32 ret;

	ret = 0;
	if (r != 0) {
		ret = r;
	}
	return ret;
}

Sint32 SFAOAP_Pause(SFD sfd, Sint32 sw)
{
	if (SFSET_GetCond(sfd, SFAOAP_COND) == 0) {
		return 0;
	}
	return sfaoap_ChkRet(SFTRN_CallTrtTrif(sfd, 3, 8, sw, 0));
}

Sint32 SFAOAP_Stop(SFD sfd)
{
	if (SFSET_GetCond(sfd, SFAOAP_COND) == 0) {
		return 0;
	}
	return sfaoap_ChkRet(SFTRN_CallTrtTrif(sfd, 3, 7, 0, 0));
}

Sint32 SFAOAP_Start(SFD sfd)
{
	if (SFSET_GetCond(sfd, SFAOAP_COND) == 0) {
		return 0;
	}
	return sfaoap_ChkRet(SFTRN_CallTrtTrif(sfd, 3, 6, 0, 0));
}

Sint32 SFAOAP_Standby(SFD sfd)
{
	if (SFSET_GetCond(sfd, SFAOAP_COND) == 0) {
		return 0;
	}
	return sfaoap_ChkRet(SFTRN_CallTrtTrif(sfd, 3, 5, 0, 0));
}

Sint32 SFAOAP_Destroy(void)
{
	return 0;
}

Sint32 SFAOAP_Create(SFD sfd)
{
	if (SFSET_GetCond(sfd, SFAOAP_COND) == 0) {
		return 0;
	}
	sfd->tr[SFAOAP_TR].hn = &sfd->aoap;
	return 0;
}

Sint32 SFAOAP_ExecServer(SFD sfd)
{
	if (SFSET_GetCond(sfd, SFAOAP_COND) == 0) {
		return 0;
	}
	if (SFTRN_GetPrepFlg(sfd, SFAOAP_TR) != 1) {
		if (SFBUF_GetPrepFlg(sfd, sfd->tr[SFAOAP_TR].bufin) == 1) {
			SFTRN_SetPrepFlg(sfd, SFAOAP_TR, 1);
		}
	}
	if (SFTRN_GetTermFlg(sfd, SFAOAP_TR) != 1) {
		if (SFBUF_GetTermFlg(sfd, sfd->tr[SFAOAP_TR].bufin) == 1) {
			SFTRN_SetTermFlg(sfd, SFAOAP_TR, 1);
		}
	}
	return 0;
}

Sint32 SFAOAP_Finish(void)
{
	return 0;
}

Sint32 SFAOAP_Init(void)
{
	return 0;
}

void SFAOAP_SetSpeed(SFD sfd, Sint32 speed)
{
	SFAOAP *aoap;

	if (SFSET_GetCond(sfd, SFAOAP_COND) != 0) {
		aoap = sfd->tr[SFAOAP_TR].hn;
		if (aoap->SetSpeed != NULL) {
			aoap->SetSpeed(sfd, speed);
		}
	}
}

Sint32 SFD_GetOutVol(SFD sfd)
{
	SFAOAP *aoap;

	if (SFLIB_CheckHn(sfd) != 0) {
		SFLIB_SetErr(NULL, 0xFF0001A4);
		return 0;
	}
	if (SFSET_GetCond(sfd, SFAOAP_COND) == 0) {
		return 0;
	}
	aoap = sfd->tr[SFAOAP_TR].hn;
	return aoap->GetOutVol(sfd);
}

Sint32 SFD_SetOutVol(SFD sfd, Sint32 vol)
{
	SFAOAP *aoap;

	if (SFLIB_CheckHn(sfd) != 0) {
		return SFLIB_SetErr(NULL, 0xFF0001A3);
	}
	if (SFSET_GetCond(sfd, SFAOAP_COND) != 0) {
		aoap = sfd->tr[SFAOAP_TR].hn;
		return aoap->SetOutVol(sfd, vol);
	}
}

Sint32 SFD_GetOutPan(SFD sfd, Sint32 ch)
{
	SFAOAP *aoap;

	if (SFLIB_CheckHn(sfd) != 0) {
		SFLIB_SetErr(NULL, 0xFF0001A2);
		return 0;
	}
	if (SFSET_GetCond(sfd, SFAOAP_COND) == 0) {
		return 0;
	}
	aoap = sfd->tr[SFAOAP_TR].hn;
	return aoap->GetOutPan(sfd, ch);
}

Sint32 SFD_SetOutPan(SFD sfd, Sint32 ch, Sint32 pan)
{
	SFAOAP *aoap;

	if (SFLIB_CheckHn(sfd) != 0) {
		return SFLIB_SetErr(NULL, 0xFF0001A1);
	}
	if (SFSET_GetCond(sfd, SFAOAP_COND) != 0) {
		aoap = sfd->tr[SFAOAP_TR].hn;
		return aoap->SetOutPan(sfd, ch, pan);
	}
}

const SFD_TR_IF SFD_tr_ao_auto_p = {
	SFAOAP_Init,
	SFAOAP_Finish,
	SFAOAP_ExecServer,
	SFAOAP_Create,
	SFAOAP_Destroy,
	SFAOAP_Standby,
	SFAOAP_Start,
	SFAOAP_Stop,
	SFAOAP_Pause,
	SFAOAP_GetWrite,
	SFAOAP_AddWrite,
	SFAOAP_GetRead,
	SFAOAP_AddRead,
	SFAOAP_Seek,
};
