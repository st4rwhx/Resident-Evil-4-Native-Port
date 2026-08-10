#include "cri_xpt.h"

typedef struct {
	Uint8 pad[0x40];
	void *sfd;
} MWPLY_OBJ;

extern Sint32 SFD_GetOutPan(void *sfd, Sint32 ch);
extern void SFD_SetOutPan(void *sfd, Sint32 ch, Sint32 pan);
extern Sint32 SFD_GetOutVol(void *sfd);
extern void SFD_SetOutVol(void *sfd, Sint32 vol);

Sint32 MWSFRNA_GetOutPan(MWPLY_OBJ *mwply, Sint32 ch)
{
	return SFD_GetOutPan(mwply->sfd, ch);
}

void MWSFRNA_SetOutPan(MWPLY_OBJ *mwply, Sint32 ch, Sint32 pan)
{
	SFD_SetOutPan(mwply->sfd, ch, pan);
}

Sint32 MWSFRNA_GetOutVol(MWPLY_OBJ *mwply)
{
	return SFD_GetOutVol(mwply->sfd);
}

void MWSFRNA_SetOutVol(MWPLY_OBJ *mwply, Sint32 vol)
{
	SFD_SetOutVol(mwply->sfd, vol);
}
