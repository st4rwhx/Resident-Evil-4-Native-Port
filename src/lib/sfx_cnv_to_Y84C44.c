#include "cri_xpt.h"

typedef struct {
	Sint32 pad0;
	Sint32 compo;
} SFX_OBJ;

typedef struct {
	Sint32 frmfmt;
} SFX_FRM;

extern Sint32 SFXINF_GetStmInf(SFX_FRM *frm, const Char8 *tag);
extern void SFX_CnvFrmYcc420plnToY84C44(SFX_OBJ *sfx, SFX_FRM *frm, void *ybuf, void *cbuf);
extern void SFXLIB_Error(SFX_OBJ *sfx, SFX_FRM *frm, const Char8 *msg);

void SFX_CnvFrmY84C44(SFX_OBJ *sfx, SFX_FRM *frm, void *ybuf, void *cbuf)
{
	Sint32 frmfmt = frm->frmfmt;

	if (sfx->compo == 0) {
		sfx->compo = SFXINF_GetStmInf(frm, "COMPO");
	}
	switch (frmfmt) {
	case 3:
		SFX_CnvFrmYcc420plnToY84C44(sfx, frm, ybuf, cbuf);
		break;
	case 2:
	default:
		SFXLIB_Error(sfx, frm, "E201193: SFX_CnvFrmY84C44 : frmfmt is not support.");
		break;
	}
}
