#include "cri_xpt.h"

typedef struct {
	Uint8 pad[0x60];
	void *usrdat;
	Sint32 usrdat_size;
} SFX_OBJ;

extern Sint32 SUD_AnalyTypeCcs(void *dat, Sint32 size);
extern Sint32 SUD_AnalyTypeDivField(void *dat, Sint32 size);
extern void SUD_Init(void);

Sint32 SFX_GetTypeCcs(SFX_OBJ *sfx)
{
	return SUD_AnalyTypeCcs(sfx->usrdat, sfx->usrdat_size);
}

Sint32 SFX_GetTypeDivField(SFX_OBJ *sfx)
{
	return SUD_AnalyTypeDivField(sfx->usrdat, sfx->usrdat_size);
}

void SFX_SetPicUsrDat(SFX_OBJ *sfx, void *dat, Sint32 size)
{
	sfx->usrdat = dat;
	sfx->usrdat_size = size;
}

void SFXSUD_Init(void)
{
	SUD_Init();
}
