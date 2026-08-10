#include "cri_xpt.h"

typedef struct {
	Sint32 pad0;
	Sint32 compo;
} SFX_OBJ;

typedef struct {
	Sint32 frmfmt;
} SFX_FRM;

extern Sint32 SFXINF_GetStmInf(SFX_FRM *frm, const Char8 *tag);
extern void SFX_CnvFrmYcc420plnToArgb8888(SFX_OBJ *sfx, SFX_FRM *frm, void *buf);
extern void SFXLIB_Error(SFX_OBJ *sfx, SFX_FRM *frm, const Char8 *msg);

void SFX_CnvFrmARGB8888(SFX_OBJ *sfx, SFX_FRM *frm, void *buf)
{
	Sint32 frmfmt = frm->frmfmt;

	if (sfx->compo == 0) {
		sfx->compo = SFXINF_GetStmInf(frm, "COMPO");
	}
	switch (frmfmt) {
	case 3:
		SFX_CnvFrmYcc420plnToArgb8888(sfx, frm, buf);
		break;
	default:
		SFXLIB_Error(sfx, frm, "E201181: SFX_CnvFrmArgb8888 : frmfmt is not support.");
		break;
	}
}
