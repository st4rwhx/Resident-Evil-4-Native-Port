#include "cri_xpt.h"

typedef struct {
	void (*func)(void *obj, Sint32 code);
	void *obj;
	Sint32 code;
	Sint32 reserved[2];
} MPVERR_INF;

typedef struct {
	Uint8 pad[0x250];
	MPVERR_INF errinf;
} MPV_OBJ;

extern Sint32 MPVLIB_CheckHn(MPV_OBJ *mpv);

MPVERR_INF mpverrinf;
Sint32 mpverr_work;

Sint32 MPVERR_SetCode(MPV_OBJ *mpv, Sint32 code)
{
	if (mpv == NULL) {
		mpverrinf.code = code;
		if (code != 0) {
			if (mpverrinf.func != NULL) {
				mpverrinf.func(mpverrinf.obj, code);
			}
		}
	} else {
		mpv->errinf.code = code;
		if (code != 0) {
			if (mpv->errinf.func != NULL) {
				mpv->errinf.func(mpv->errinf.obj, code);
			}
		}
	}
	return code;
}

Sint32 MPV_SetErrFunc(MPV_OBJ *mpv, void (*func)(void *obj, Sint32 code), void *obj)
{
	if (MPVLIB_CheckHn(mpv) != 0) {
		return MPVERR_SetCode(NULL, 0xFF030203);
	}
	mpv->errinf.func = func;
	mpv->errinf.obj = obj;
	return 0;
}

void MPVERR_InitErrInf(MPVERR_INF *inf)
{
	inf->func = NULL;
	inf->obj = NULL;
	inf->code = 0;
	inf->reserved[0] = 0;
	inf->reserved[1] = 0;
}

void MPVERR_Init(void)
{
	MPVERR_InitErrInf(&mpverrinf);
}
