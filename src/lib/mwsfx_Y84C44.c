#include "mwsfd.h"

void mwPlyFxCnvFrmY84C44(MWPLY_OBJ *mwply, MWS_FRM *frm, void *ybuf, void *cbuf)
{
	SFX_FRM sfxfrm;
	SFX_OBJ *sfx;

	if (!MWSFD_IsEnableHndl(mwply)) {
		MWSFSVM_Error("E201199: mwPlyFxCnvFrmYUV422: handle is invalid.");
		return;
	}
	if (frm->bufadr == NULL) {
		MWSFSVM_Error("E2011910: mwPlyFxCnvFrmYUV422: getfrm is failed.");
		return;
	}
	sfx = MWSFSFX_GetSfxHn(mwply);
	MWSFSFX_CnvFrmInfToSfx(mwply, frm, &sfxfrm);
	SFX_CnvFrmY84C44(sfx, &sfxfrm, ybuf, cbuf);
}
