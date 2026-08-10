#include "sfd.h"

Sint32 SFVOM_Seek(void)
{
	return 0;
}

Sint32 SFVOM_AddRead(SFD sfd, void *frm)
{
	return SFBUF_VfrmAddRead(sfd, sfd->tr[6].bufin, frm);
}

Sint32 SFVOM_GetRead(SFD sfd, void **frm)
{
	Sint32 ret;

	if ((Uint32)(sfd->stat - 3) > 1) {
		*frm = NULL;
		return 0;
	}
	ret = SFBUF_VfrmGetRead(sfd, sfd->tr[6].bufin, frm);
	if (ret != 0) {
		return ret;
	}
	if (!SFTIM_IsGetFrmTime(sfd, *frm)) {
		*frm = NULL;
		return 0;
	}
	return 0;
}

Sint32 SFVOM_AddWrite(SFD sfd)
{
	SFLIB_SetErr(sfd, 0xFF000701);
}

Sint32 SFVOM_GetWrite(SFD sfd)
{
	SFLIB_SetErr(sfd, 0xFF000701);
}

Sint32 SFVOM_Pause(void)
{
	return 0;
}

Sint32 SFVOM_Stop(void)
{
	return 0;
}

Sint32 SFVOM_Start(void)
{
	return 0;
}

Sint32 SFVOM_Standby(void)
{
	return 0;
}

Sint32 SFVOM_Destroy(void)
{
	return 0;
}

Sint32 SFVOM_Create(void)
{
	return 0;
}

Sint32 SFVOM_ExecServer(SFD sfd)
{
	Sint32 term;

	if (SFSET_GetCond(sfd, 5) == 0) {
		return 0;
	}
	if (SFTRN_GetTermFlg(sfd, 6) != 1) {
		if (SFBUF_GetTermFlg(sfd, sfd->tr[6].bufin) == 1) {
			if (SFSET_GetCond(sfd, 15) == 0) {
				term = 1;
			} else if (SFTIM_IsVideoTerm(sfd) == 0) {
				term = 0;
			} else {
				term = 1;
			}
			if (term != 0) {
				SFTRN_SetTermFlg(sfd, 6, 1);
			}
		}
	}
	if (SFTRN_GetPrepFlg(sfd, 6) != 1) {
		if (SFBUF_GetPrepFlg(sfd, sfd->tr[6].bufin) == 1) {
			SFTRN_SetPrepFlg(sfd, 6, 1);
		}
	}
	return 0;
}

Sint32 SFVOM_Finish(void)
{
	return 0;
}

Sint32 SFVOM_Init(void)
{
	return 0;
}

const SFD_TR_IF SFD_tr_vo_manu = {
	SFVOM_Init,
	SFVOM_Finish,
	SFVOM_ExecServer,
	SFVOM_Create,
	SFVOM_Destroy,
	SFVOM_Standby,
	SFVOM_Start,
	SFVOM_Stop,
	SFVOM_Pause,
	SFVOM_GetWrite,
	SFVOM_AddWrite,
	SFVOM_GetRead,
	SFVOM_AddRead,
	SFVOM_Seek,
};
