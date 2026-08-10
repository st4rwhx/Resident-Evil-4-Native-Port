/* Sofdec SFX: library init, handle creation, error callback */
#include "cri_xpt.h"
#include "sfx.h"
#include <string.h>

const Char8 sfx_ver_str[] = "\nCRI SFX/GC Ver.2.08 Build:Sep 22 2004 10:35:23\n";

Sint32 sfx_init_cnt = 0;
Sint32 sfxcnv_forcesplit = 0;
SFX_LIBWORK sfx_libwork;
const Char8 *sfx_dummy;

Sint32 SFX_GetCcirFx(void)
{
	return sfx_libwork.ccir_fx;
}

void SFXLIB_Error(SFX_OBJ *sfx, SFX_FRM *frm, const Char8 *msg)
{
	void (*fn)(void *obj, const Char8 *msg);
	void *obj;

	fn = sfx_libwork.errfn;
	obj = sfx_libwork.errobj;
	sfx_libwork.err_cnt++;
	if (fn != NULL) {
		fn(obj, msg);
	}
}

void SFX_Destroy(SFX_OBJ *sfx)
{
	SFXZ_OBJ *sfxz;
	SFXA_OBJ *sfxa;

	if (sfx == NULL) {
		return;
	}
	sfxz = sfx->sfxz;
	sfxa = sfx->sfxa;
	sfx->used = 0;
	SFXZ_Destroy(sfxz);
	SFXA_Destroy(sfxa);
	sfx_libwork.hn_cnt--;
}

static SFX_OBJ *sfx_GetFreeHn(void)
{
	SFX_OBJ *sfx;
	Sint32 i;

	sfx = sfx_libwork.hn;
	for (i = 0; i < sfx_libwork.max_hn; i++) {
		if (sfx->used == 0) {
			return sfx;
		}
		sfx++;
	}
	return NULL;
}

static Bool sfx_IsEnoughWork(Sint32 wsize)
{
	return wsize >= SFX_WORK_SIZE;
}

SFX_OBJ *SFX_Create(void *work, Sint32 wsize)
{
	SFX_OBJ *sfx;
	SFXZ_OBJ *sfxz;
	SFXA_OBJ *sfxa;

	sfx = sfx_GetFreeHn();
	if (sfx == NULL) {
		return sfx;
	}
	if (sfx_IsEnoughWork(wsize) != TRUE) {
		SFXLIB_Error(NULL, NULL, "E201194: sfx_InitHn: work size is short.");
		return NULL;
	}
	memset(sfx, 0, sizeof(SFX_OBJ));
	sfx->compo = 0;
	sfx->fxtype = SFX_COMPO_YCC420PLN;
	sfx->outbuf_width = 0;
	sfx->outbuf_height = 0;
	sfx->x2c = 1;
	sfx->x30 = 0;
	sfx->tbl_type = 0;
	sfx->buf[0] = (Uint8 *)(((Uint32)work + 0x1F) & ~0x1F);
	sfx->buf[1] = sfx->buf[0] + SFX_BUF_SIZE;
	sfx->buf[2] = sfx->buf[1] + SFX_BUF_SIZE;
	sfx->buf[3] = sfx->buf[2] + SFX_BUF_SIZE;
	sfx->work = work;
	sfx->wsize = wsize;
	sfx->x68 = -1;
	sfx->x74 = 0;
	sfx->used = 1;
	sfxz = SFXZ_Create();
	if (sfxz == NULL) {
		SFXLIB_Error(NULL, NULL, "E201281: SfxZHn: can't create.");
		SFX_Destroy(sfx);
		return NULL;
	}
	sfx->sfxz = sfxz;
	sfxa = SFXA_Create();
	if (sfxa == NULL) {
		SFXLIB_Error(NULL, NULL, "E202011: SfxAHn: can't create.");
		SFX_Destroy(sfx);
		return NULL;
	}
	sfx->sfxa = sfxa;
	sfx_libwork.hn_cnt++;
	return sfx;
}

void SFX_SetErrFn(void (*fn)(void *obj, const Char8 *msg), void *obj)
{
	sfx_libwork.errfn = fn;
	sfx_libwork.errobj = obj;
}

void SFX_Init(void)
{
	if (sfx_init_cnt < 1) {
		sfx_dummy = sfx_ver_str;
		memset(&sfx_libwork, 0, sizeof(sfx_libwork));
		sfx_libwork.max_hn = SFX_MAX_HN;
		sfx_libwork.ccir_fx = 1;
		CFT_Ycc420plnToArgb8888Init();
		SFXSUD_Init();
		SFXZ_Init();
		SFXA_Init();
		sfxcnv_forcesplit = 0;
		sfx_init_cnt++;
	}
}
