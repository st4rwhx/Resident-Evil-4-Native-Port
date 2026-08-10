/* ADXPD: ADX block expander (decoder driver) handles */
#include "cri_xpt.h"
#include "adx_b.h"
#include <string.h>

#define ADXPD_MAX_OBJ 16

/* ADXPD_OBJ.stat */
#define ADXPD_STAT_STOP 0
#define ADXPD_STAT_START 1
#define ADXPD_STAT_EXEC 2
#define ADXPD_STAT_DONE 3

/* ADXPD_OBJ.mode */
#define ADXPD_MODE_MONO 1
#define ADXPD_MODE_STE 2

typedef struct ADXPD_OBJ {
	Sint32 used;        /* 0x00 */
	Sint32 id;          /* 0x04 */
	Sint32 x08;         /* 0x08 */
	Sint32 stat;        /* 0x0C */
	Sint32 nblk;        /* 0x10 decoded blocks */
	Sint32 mode;        /* 0x14 */
	void *in;           /* 0x18 */
	Sint32 nblk_in;     /* 0x1C */
	Sint16 *out0;       /* 0x20 */
	Sint16 *out1;       /* 0x24 */
	Sint16 dly[2][2];   /* 0x28 */
	Sint16 k1;          /* 0x30 */
	Sint16 k2;          /* 0x32 */
	Sint16 ext1;        /* 0x34 */
	Sint16 ext2;        /* 0x36 */
	Sint16 ext3;        /* 0x38 */
	Sint16 pad3a;
} ADXPD_OBJ;

extern Sint32 adx_decode_output_mono_flag;
extern Sint32 ADX_DecodeMono4(void *src, Sint32 nblk, Sint16 *out, Sint16 *dly, Sint16 k1, Sint16 k2,
			      Sint16 *ext, Sint16 e2, Sint16 e3);
extern Sint32 ADX_DecodeSte4(void *src, Sint32 nblk, Sint16 *out0, Sint16 *dly0, Sint16 *out1, Sint16 *dly1,
			     Sint16 k1, Sint16 k2, Sint16 *ext, Sint16 e2, Sint16 e3);
extern void ADX_GetCoefficient(Sint32 cutoff, Sint32 sfreq, Sint16 *k1, Sint16 *k2);

Sint32 adxpd_internal_error;
ADXPD_OBJ adxpd_obj[ADXPD_MAX_OBJ];

void ADXPD_ExecHndl(ADXPD pd)
{
	if (pd->stat == ADXPD_STAT_START) {
		pd->stat = ADXPD_STAT_EXEC;
	}
	if (pd->stat == ADXPD_STAT_EXEC) {
		if (pd->mode == ADXPD_MODE_MONO) {
			pd->nblk = ADX_DecodeMono4(pd->in, pd->nblk_in, pd->out0, pd->dly[0], pd->k1, pd->k2,
						   &pd->ext1, pd->ext2, pd->ext3);
		} else {
			pd->nblk = ADX_DecodeSte4(pd->in, pd->nblk_in, pd->out0, pd->dly[0], pd->out1, pd->dly[1],
						  pd->k1, pd->k2, &pd->ext1, pd->ext2, pd->ext3);
			if (pd->nblk % 2 == 1) {
				adxpd_internal_error = 1;
			}
		}
		pd->stat = ADXPD_STAT_DONE;
	}
}

Sint32 ADXPD_GetNumBlk(ADXPD pd)
{
	return pd->nblk;
}

void ADXPD_Reset(ADXPD pd)
{
	if (pd->stat == ADXPD_STAT_DONE) {
		pd->stat = ADXPD_STAT_STOP;
	}
}

void ADXPD_Stop(ADXPD pd)
{
	pd->stat = ADXPD_STAT_STOP;
	memset(pd->dly, 0, sizeof(pd->dly));
}

void ADXPD_Start(ADXPD pd)
{
	if (pd->stat == ADXPD_STAT_STOP) {
		pd->nblk = 0;
		pd->stat = ADXPD_STAT_START;
	}
}

Sint32 ADXPD_EntryPl2(ADXPD pd, void *in, Sint32 nblk, Sint16 *out0, Sint16 *out1)
{
	if (pd->stat == ADXPD_STAT_STOP) {
		pd->mode = ADXPD_MODE_MONO;
		pd->in = in;
		pd->nblk_in = nblk;
		pd->out0 = out0;
		pd->out1 = out1;
		return 1;
	}
	return 0;
}

Sint32 ADXPD_EntrySte(ADXPD pd, void *in, Sint32 nblk, Sint16 *out0, Sint16 *out1)
{
	if (pd->stat == ADXPD_STAT_STOP) {
		pd->mode = ADXPD_MODE_STE;
		pd->in = in;
		pd->nblk_in = nblk;
		pd->out0 = out0;
		pd->out1 = out1;
		return 1;
	}
	return 0;
}

Sint32 ADXPD_EntryMono(ADXPD pd, void *in, Sint32 nblk, Sint16 *out0, Sint16 *out1)
{
	if (pd->stat == ADXPD_STAT_STOP) {
		pd->mode = ADXPD_MODE_MONO;
		pd->in = in;
		pd->nblk_in = nblk;
		pd->out0 = out0;
		pd->out1 = out1;
		return 1;
	}
	return 0;
}

Sint32 ADXPD_GetStat(ADXPD pd)
{
	return pd->stat;
}

void ADXPD_Destroy(ADXPD pd)
{
	if (pd != NULL) {
		pd->used = 0;
		memset(pd, 0, sizeof(ADXPD_OBJ));
	}
}

void ADXPD_GetExtPrm(ADXPD pd, Sint16 *e1, Sint16 *e2, Sint16 *e3)
{
	*e1 = pd->ext1;
	*e2 = pd->ext2;
	*e3 = pd->ext3;
}

void ADXPD_SetExtPrm(ADXPD pd, Sint16 e1, Sint16 e2, Sint16 e3)
{
	pd->ext1 = e1;
	pd->ext2 = e2;
	pd->ext3 = e3;
}

void ADXPD_GetDly(ADXPD pd, Sint16 *d0, Sint16 *d1)
{
	d0[0] = pd->dly[0][0];
	d1[0] = pd->dly[0][1];
	d0[1] = pd->dly[1][0];
	d1[1] = pd->dly[1][1];
}

void ADXPD_SetDly(ADXPD pd, Sint16 *d0, Sint16 *d1)
{
	pd->dly[0][0] = d0[0];
	pd->dly[0][1] = d1[0];
	pd->dly[1][0] = d0[1];
	pd->dly[1][1] = d1[1];
}

void ADXPD_SetCoef(ADXPD pd, Sint32 sfreq, Sint32 cutoff)
{
	ADX_GetCoefficient(cutoff, sfreq, &pd->k1, &pd->k2);
}

ADXPD ADXPD_Create(void)
{
	Sint32 i;
	ADXPD pd;

	for (i = 0; i < ADXPD_MAX_OBJ; i++) {
		if (adxpd_obj[i].used == 0) {
			break;
		}
	}
	if (i == ADXPD_MAX_OBJ) {
		return NULL;
	}
	pd = &adxpd_obj[i];
	memset(pd, 0, sizeof(ADXPD_OBJ));
	pd->used = 1;
	pd->id = i;
	pd->x08 = 0;
	pd->stat = ADXPD_STAT_STOP;
	ADX_GetCoefficient(500, 44100, &pd->k1, &pd->k2);
	memset(pd->dly, 0, sizeof(pd->dly));
	return pd;
}

void ADXPD_Init(void)
{
	memset(adxpd_obj, 0, sizeof(adxpd_obj));
}
