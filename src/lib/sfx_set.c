#include "cri_xpt.h"

typedef struct {
	Sint32 pad0;
	Sint32 compo_mode;
	Sint32 fxtype;
	Sint32 outbuf_width;
	Sint32 outbuf_height;
	Sint32 unit_width;
	Sint32 taginf_flg;
	Sint32 tag_a;
	Sint32 tag_b;
	Sint32 pad24;
	void *sfxz;
	Sint32 pad2C[3];
	void *coladj;
} SFX_OBJ;

typedef struct {
	Sint32 a;
	Sint32 b;
} SFX_TAGINF;

extern void *SJ_SearchTag(SFX_TAGINF *inf, const Char8 *tag, const Char8 *name, SFX_TAGINF *out);
extern void SFXZ_SetTagInf(void *sfxz, Sint32 a, Sint32 b);

void *SFX_GetColAdj(SFX_OBJ *sfx)
{
	return sfx->coladj;
}

void SFX_SetColAdj(SFX_OBJ *sfx, void *coladj)
{
	sfx->coladj = coladj;
}

void SFX_GetTagInf(SFX_OBJ *sfx, Sint32 *a, Sint32 *b)
{
	if (sfx->taginf_flg != 1) {
		*a = 0;
		*b = 0;
	} else {
		*a = sfx->tag_a;
		*b = sfx->tag_b;
	}
}

void SFX_SetTagInf(SFX_OBJ *sfx, Sint32 a, Sint32 b)
{
	SFX_TAGINF inf;
	SFX_TAGINF out;
	void *sfxz = sfx->sfxz;

	sfx->tag_a = a;
	sfx->tag_b = b;
	inf.a = a;
	inf.b = b;
	if (SJ_SearchTag(&inf, "SFXZ", "SFXINFE", &out) == NULL) {
		SFXZ_SetTagInf(sfxz, 0, 0);
	} else {
		SFXZ_SetTagInf(sfxz, out.a, out.b);
	}
	sfx->taginf_flg = 1;
}

void SFX_SetUnitWidth(SFX_OBJ *sfx, Sint32 width)
{
	sfx->unit_width = width;
}

void SFX_SetOutBufSize(SFX_OBJ *sfx, Sint32 width, Sint32 height)
{
	sfx->outbuf_width = width;
	sfx->outbuf_height = height;
}

Sint32 SFX_GetFxType(SFX_OBJ *sfx)
{
	return sfx->fxtype;
}

void SFX_SetFxType(SFX_OBJ *sfx, Sint32 fxtype)
{
	sfx->fxtype = fxtype;
}

void SFX_SetCompoMode(SFX_OBJ *sfx, Sint32 mode)
{
	sfx->compo_mode = mode;
}
