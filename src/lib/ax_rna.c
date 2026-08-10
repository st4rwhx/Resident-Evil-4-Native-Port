/* CRI ADX renderer for the Nintendo AX sound library (AXRNA): a handle owns up to two AX voices fed
 * from ARAM ring buffers (RNARES + SJRBF); PCM data is DMA'd from the input stream joints. */
#include "cri_xpt.h"
#include <string.h>
#include <dolphin/os.h>
#include <dolphin/ar.h>
#include <dolphin/ax.h>
#include <dolphin/mix.h>
#include "sj.h"

#define AXRNA_MAX_OBJ 16
#define AXRNA_MAX_NCH 2
#define AXRNA_BUF_NSMPL 0x1000   /* ring buffer length in 16-bit samples */
#define AXRNA_DEF_SFREQ 48000
#define AXRNA_DMA_ALIGN 32
#define AXRNA_PAN_MAX 15
#define AXRNA_VOL_MIN -999

/* AX voice parameter values (not in include/dolphin/ax.h) */
#define AX_PB_STATE_STOP 0
#define AX_PB_STATE_RUN 1
#define AX_PB_FORMAT_PCM16 10

/* AXRNA_OBJ.sw bits */
#define AXRNA_SW_TRANS 0x01
#define AXRNA_SW_PLAY 0x02
#define AXRNA_GET_PLAY_SW(rna) (((rna)->sw >> 1) & 1)
#define AXRNA_GET_TRANS_SW(rna) ((rna)->sw & 1)

#define AXRNA_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define AXRNA_MAX(a, b) (((a) > (b)) ? (a) : (b))

typedef struct {
	Sint32 used;    /* 0x0 */
	Uint32 buf;     /* 0x4 */
	Uint32 size;    /* 0x8 */
} RNARES_OBJ;
typedef RNARES_OBJ *RNARES;

extern void GCRNA_LockCs(void);
extern void GCRNA_UnlockCs(void);
extern void RNAERR_CallErrFunc(Char8 *msg);
extern void RNAERR_EntryErrFunc(void (*func)(void *obj, Char8 *msg), void *obj);
extern Sint32 RNARES_GetBufSize(RNARES res);
extern Uint32 RNARES_GetBuf(RNARES res);
extern void RNARES_Destroy(RNARES res);
extern RNARES RNARES_Create(void);
extern void RNARES_Finish(void);
extern void RNARES_Init(void);

typedef struct AXRNA_OBJ {
	Sint8 used;                          /* 0x00 */
	Uint8 sw;                            /* 0x01 AXRNA_SW_* bits */
	Sint8 maxnch;                        /* 0x02 */
	Sint8 nch;                           /* 0x03 */
	Sint32 play_pos;                     /* 0x04 last observed play position (-1: none) */
	AXVPB *voice[AXRNA_MAX_NCH];         /* 0x08 */
	RNARES res[AXRNA_MAX_NCH];           /* 0x10 */
	Sint32 buf[AXRNA_MAX_NCH];           /* 0x18 ARAM address of the ring buffer (16-bit units) */
	Sint32 bufsize;                      /* 0x20 */
	Sint32 sfreq;                        /* 0x24 */
	Uint32 arq_owner[AXRNA_MAX_NCH];     /* 0x28 */
	SJ sj[AXRNA_MAX_NCH];                /* 0x30 input stream joints */
	SJ sjrbf[AXRNA_MAX_NCH];             /* 0x38 ring buffer stream joints */
	SJCK trans_ck[AXRNA_MAX_NCH];        /* 0x40 data chunk being transferred */
	SJCK free_ck[AXRNA_MAX_NCH];         /* 0x50 ring buffer chunk being written */
	volatile Sint32 trans_busy[AXRNA_MAX_NCH]; /* 0x60 */
	Sint32 trans_smpl;                   /* 0x68 */
	Sint32 trans_total;                  /* 0x6C */
	volatile Sint32 flash_busy[AXRNA_MAX_NCH]; /* 0x70 */
	Sint32 flash_smpl;                   /* 0x78 */
	Sint32 flash_total;                  /* 0x7C */
	Sint32 bps;                          /* 0x80 */
	Sint32 outvol;                       /* 0x84 */
	Sint32 outpan[AXRNA_MAX_NCH];        /* 0x88 */
	Sint32 span;                         /* 0x90 */
	Sint32 auxa;                         /* 0x94 */
	Sint32 auxb;                         /* 0x98 */
	Sint32 fader;                        /* 0x9C */
	Sint16 adjsfreq_fg;                  /* 0xA0 */
	Sint16 adjsfreq_state;               /* 0xA2 */
	Sint32 src_type;                     /* 0xA4 */
	ARQRequest arq[AXRNA_MAX_NCH];       /* 0xA8 */
} AXRNA_OBJ;
typedef AXRNA_OBJ *AXRNA;

static void axrna_end_flash(u32 req);
static void axrna_end_trans(u32 req);
static void axrna_update_play(AXRNA rna);
static void axrna_voice_drop(void *p);
void AXRNA_SetPlaySw(AXRNA rna, Sint32 sw);
void AXRNA_SetTransSw(AXRNA rna, Sint32 sw);

static const Char8 *const volatile axrna_build = "\nAXRNA Ver.1.04 Build:Oct  8 2004 13:33:03\n";

Sint32 axrna_def_src_type = AX_SRC_TYPE_LINEAR;
Sint32 axrna_pan_tbl[2 * AXRNA_PAN_MAX + 1] = {
	0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x21, 0x25, 0x29, 0x2D, 0x31, 0x35, 0x39, 0x40,
	0x44, 0x48, 0x4C, 0x51, 0x55, 0x59, 0x5D, 0x62, 0x66, 0x6A, 0x6E, 0x73, 0x77, 0x7B, 0x7F,
};

static Uint32 axrna_init_cnt = 0;
static void *axrna_zero_dat = NULL;
Sint32 axrna_def_adjsfreq_fg = 0;
static Sint32 axrna_foo_cnt = 0;
Sint32 axrna_update_pos = 0;
static Sint32 axrna_update_hist[32];
Uint8 axrna_zero_dat_real[AXRNA_BUF_NSMPL + 2 * AXRNA_DMA_ALIGN];
AXRNA_OBJ axrna_obj[AXRNA_MAX_OBJ];

void AXRNA_SetAdjsfreqFlg(AXRNA rna, Sint32 flg)
{
	if (rna == NULL) {
		return;
	}
	rna->adjsfreq_fg = flg;
}

void AXRNA_SetSrcType(AXRNA rna, Sint32 type)
{
	if (rna == NULL) {
		return;
	}
	rna->src_type = type;
	rna->adjsfreq_state = 1;
}

/* dead (linker-stripped): fixes the .bss order of the uninitialised statics */
void AXRNA_DbgDump(void)
{
	const Char8 *sw_str[2] = { "OFF", "ON " };
	Sint32 i;

	for (i = 0; i < 32; i++) {
		axrna_foo_cnt += axrna_update_hist[i];
	}
	axrna_zero_dat_real[0] = 0;
	for (i = 0; i < AXRNA_MAX_OBJ; i++) {
		RNAERR_CallErrFunc((Char8 *)sw_str[AXRNA_GET_PLAY_SW(&axrna_obj[i])]);
	}
}

Sint32 AXRNA_GetPlaySw(AXRNA rna)
{
	if (rna == NULL) {
		return -1;
	}
	return AXRNA_GET_PLAY_SW(rna);
}

Sint32 AXRNA_GetTransSw(AXRNA rna)
{
	if (rna == NULL) {
		return -1;
	}
	return AXRNA_GET_TRANS_SW(rna);
}

Sint32 AXRNA_DiscardData(AXRNA rna, Sint32 nsmpl)
{
	return 0;
}

Sint32 AXRNA_SetStmHdInfo(AXRNA rna, void *hdinfo)
{
	return 0;
}

void AXRNA_SetBitPerSmpl(AXRNA rna, Sint32 bps)
{
	if (rna == NULL) {
		return;
	}
	rna->bps = bps;
}

void AXRNA_SetOutPan(AXRNA rna, Sint32 ch, Sint32 pan)
{
	Sint32 p;

	if (rna == NULL) {
		return;
	}
	if (ch >= rna->maxnch) {
		return;
	}
	p = AXRNA_MIN(pan, AXRNA_PAN_MAX);
	p = AXRNA_MAX(p, -AXRNA_PAN_MAX);
	if (p == rna->outpan[ch]) {
		return;
	}
	rna->outpan[ch] = p;
	GCRNA_LockCs();
	if (rna->voice[ch] != NULL) {
		MIXSetPan(rna->voice[ch], axrna_pan_tbl[p + AXRNA_PAN_MAX]);
	}
	GCRNA_UnlockCs();
}

void AXRNA_SetOutVol(AXRNA rna, Sint32 vol)
{
	Sint32 v;
	Sint32 i;

	if (rna == NULL) {
		return;
	}
	v = AXRNA_MIN(vol, 0);
	v = AXRNA_MAX(v, AXRNA_VOL_MIN);
	if (v == rna->outvol) {
		return;
	}
	rna->outvol = v;
	for (i = 0; i < rna->maxnch; i++) {
		GCRNA_LockCs();
		if (rna->voice[i] != NULL) {
			MIXSetInput(rna->voice[i], v);
		}
		GCRNA_UnlockCs();
	}
}

void AXRNA_SetSfreq(AXRNA rna, Sint32 sfreq)
{
	AXPBSRC src;
	Sint32 adj;
	Sint32 i;

	if (rna == NULL) {
		return;
	}
	rna->sfreq = sfreq;
	/* the AX DSP runs at 32028.5 Hz: sfreq * 1124 / 1125, rounded up */
	adj = (sfreq * 1124 + 1124) / 1125;
	for (i = 0; i < rna->maxnch; i++) {
		GCRNA_LockCs();
		if (rna->voice[i] != NULL) {
			if (rna->adjsfreq_fg == 1) {
				Uint32 f = adj;

				if (sfreq == 32000 && rna->adjsfreq_state == 0) {
					AXRNA_SetSrcType(rna, AX_SRC_TYPE_NONE);
				}
				src.ratioHi = f / 32000;
				src.ratioLo = (f << 8) / 125;
			} else {
				src.ratioHi = sfreq / 32000;
				src.ratioLo = ((sfreq << 8) / 125) & 0xFFFF;
			}
			src.currentAddressFrac = 0;
			src.last_samples[0] = 0;
			src.last_samples[1] = 0;
			src.last_samples[2] = 0;
			src.last_samples[3] = 0;
			AXSetVoiceSrcType(rna->voice[i], rna->src_type);
			AXSetVoiceSrc(rna->voice[i], &src);
		}
		GCRNA_UnlockCs();
	}
}

void AXRNA_SetNumChan(AXRNA rna, Sint32 nch)
{
	if (rna == NULL) {
		return;
	}
	rna->nch = nch;
}

static void AXRNA_ExecHndl(AXRNA rna);

void AXRNA_ExecServer(void)
{
	Uint32 i;

	for (i = 0; i < AXRNA_MAX_OBJ; i++) {
		if (axrna_obj[i].used == 1) {
			AXRNA_ExecHndl(&axrna_obj[i]);
		}
	}
}

static void axrna_exec_trans(AXRNA rna)
{
	SJCK data;
	SJCK data_rest;
	SJCK free;
	SJCK free_rest;
	Sint32 i;
	Sint32 n;

	for (i = 0; i < rna->nch; i++) {
		if (rna->voice[i] == NULL) {
			continue;
		}
		if (rna->trans_busy[i] != 0) {
			continue;
		}
		SJ_GetChunk(rna->sjrbf[i], SJ_CK_FREE, 0x2000, &free);
		SJ_GetChunk(rna->sj[i], SJ_CK_DATA, free.len, &data);
		n = AXRNA_MIN(data.len, free.len);
		n = (n / AXRNA_DMA_ALIGN) * AXRNA_DMA_ALIGN;
		SJ_SplitChunk(&free, n, &free, &free_rest);
		SJ_UngetChunk(rna->sjrbf[i], SJ_CK_FREE, &free_rest);
		SJ_SplitChunk(&data, n, &data, &data_rest);
		SJ_UngetChunk(rna->sj[i], SJ_CK_DATA, &data_rest);
		if (n == 0) {
			return;
		}
		if (data.len != free.len) {
			for (;;) {
			}
		}
		rna->trans_ck[i] = data;
		rna->free_ck[i] = free;
		rna->trans_smpl = n / sizeof(Sint16);
		DCFlushRange(rna->trans_ck[i].data, rna->trans_ck[i].len);
		rna->trans_busy[i] = 1;
		ARQPostRequest(&rna->arq[i], rna->arq_owner[i], ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH,
		               (u32)data.data, (u32)free.data, n, axrna_end_trans);
		while (rna->trans_busy[i] != 0) {
		}
	}
}

static void axrna_exec_flash(AXRNA rna)
{
	SJCK zero;
	SJCK zero_rest;
	Sint32 i;
	Sint32 n;

	if (rna->flash_total < rna->bufsize) {
		for (i = 0; i < rna->nch; i++) {
			if (rna->flash_busy[i] != 0) {
				continue;
			}
			SJ_GetChunk(rna->sjrbf[i], SJ_CK_FREE, 0x2000, &zero);
			n = (zero.len / AXRNA_DMA_ALIGN) * AXRNA_DMA_ALIGN;
			SJ_SplitChunk(&zero, n, &zero, &zero_rest);
			SJ_UngetChunk(rna->sjrbf[i], SJ_CK_FREE, &zero_rest);
			if (n == 0) {
				return;
			}
			rna->free_ck[i] = zero;
			rna->flash_smpl = n / sizeof(Sint16);
			DCFlushRange(axrna_zero_dat, AXRNA_BUF_NSMPL);
			rna->flash_busy[i] = 1;
			ARQPostRequest(&rna->arq[i], rna->arq_owner[i], ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH,
			               (u32)axrna_zero_dat, (u32)zero.data, n, axrna_end_flash);
			while (rna->flash_busy[i] != 0) {
			}
		}
	}
}

static void AXRNA_ExecHndl(AXRNA rna)
{
	if (rna == NULL) {
		return;
	}
	if (AXRNA_GetPlaySw(rna) == 1) {
		axrna_update_play(rna);
	}
	if (AXRNA_GetTransSw(rna) == 1) {
		axrna_exec_trans(rna);
	} else if (AXRNA_GetPlaySw(rna) == 1) {
		axrna_exec_flash(rna);
	}
}

static void axrna_end_flash(u32 req)
{
	Sint32 id;
	AXRNA rna;
	Sint32 ch;

	id = ((ARQRequest *)req)->owner & 0x7FFFFFFF;
	rna = &axrna_obj[id / 2];
	ch = id % 2;
	if (rna->flash_busy[ch] == 1) {
		SJ_PutChunk(rna->sjrbf[ch], SJ_CK_DATA, &rna->free_ck[ch]);
		rna->flash_busy[ch] = 0;
		if (ch == rna->nch - 1) {
			rna->flash_total += rna->flash_smpl;
		}
	}
}

static void axrna_end_trans(u32 req)
{
	Sint32 id;
	AXRNA rna;
	Sint32 ch;

	id = ((ARQRequest *)req)->owner & 0x7FFFFFFF;
	rna = &axrna_obj[id / 2];
	ch = id % 2;
	if (rna->trans_busy[ch] == 1) {
		SJ_PutChunk(rna->sj[ch], SJ_CK_FREE, &rna->trans_ck[ch]);
		SJ_PutChunk(rna->sjrbf[ch], SJ_CK_DATA, &rna->free_ck[ch]);
		rna->trans_busy[ch] = 0;
		if (ch == rna->nch - 1) {
			rna->trans_total += rna->trans_smpl;
		}
	}
}

static void axrna_update_play(AXRNA rna)
{
	SJCK ck;
	Sint32 nch = rna->nch;
	AXVPB *vpb = rna->voice[nch - 1];
	Sint32 prev = rna->play_pos;
	Sint32 cur;
	Sint32 diff;
	Sint32 i;
	Sint32 nbyte;

	if (vpb == NULL) {
		return;
	}
	cur = *(Sint32 *)&vpb->pb.addr.currentAddressHi - rna->buf[nch - 1];
	axrna_update_hist[axrna_update_pos++] = cur;
	if (axrna_update_pos == 32) {
		axrna_update_pos = 0;
	}
	if (cur < 0 || cur > rna->bufsize) {
		for (;;) {
		}
	}
	if (prev == -1) {
		if (cur == 0) {
			diff = 0;
		} else {
			rna->play_pos = 0;
			prev = 0;
		}
	}
	if (prev != -1) {
		if (cur > prev) {
			diff = cur - prev;
		} else {
			diff = AXRNA_BUF_NSMPL - (prev - cur);
		}
	}
	diff = (diff / 0x800) * 0x800;
	if (diff <= 0) {
		return;
	}
	nbyte = diff * sizeof(Sint16);
	for (i = 0; i < rna->nch; i++) {
		SJ_GetChunk(rna->sjrbf[i], SJ_CK_DATA, nbyte, &ck);
		SJ_PutChunk(rna->sjrbf[i], SJ_CK_FREE, &ck);
	}
	rna->play_pos += diff;
	if (rna->play_pos >= AXRNA_BUF_NSMPL) {
		rna->play_pos -= AXRNA_BUF_NSMPL;
	}
}

Sint32 AXRNA_GetNumRoom(AXRNA rna)
{
	if (rna == NULL) {
		return -1;
	}
	return SJ_GetNumData(rna->sjrbf[rna->nch - 1], SJ_CK_FREE) / sizeof(Sint16);
}

Sint32 AXRNA_GetNumData(AXRNA rna)
{
	Sint32 n;

	if (rna == NULL) {
		return -1;
	}
	n = AXRNA_BUF_NSMPL - SJ_GetNumData(rna->sjrbf[rna->nch - 1], SJ_CK_FREE) / sizeof(Sint16);
	n -= rna->flash_total;
	if (n < 0) {
		n = 0;
	}
	return n;
}

void AXRNA_SetPlaySw(AXRNA rna, Sint32 sw)
{
	AXPBADDR addr;
	Sint32 last;
	Sint32 loop;
	Sint32 cur;
	Sint32 i;

	if (rna == NULL) {
		return;
	}
	if (sw == AXRNA_GetPlaySw(rna)) {
		return;
	}
	GCRNA_LockCs();
	if (sw == 1) {
		rna->play_pos = -1;
		for (i = 0; i < rna->nch; i++) {
			if (rna->voice[i] != NULL) {
				last = rna->bufsize - 1;
				loop = rna->buf[i];
				cur = rna->buf[i];
				addr.loopFlag = 1;
				addr.format = AX_PB_FORMAT_PCM16;
				addr.loopAddressHi = loop >> 16;
				addr.loopAddressLo = loop;
				addr.endAddressHi = (loop + last) >> 16;
				addr.endAddressLo = loop + last;
				addr.currentAddressHi = cur >> 16;
				addr.currentAddressLo = cur;
				AXSetVoiceAddr(rna->voice[i], &addr);
				AXSetVoiceState(rna->voice[i], AX_PB_STATE_RUN);
			}
		}
		rna->sw |= AXRNA_SW_PLAY;
	} else if (sw == 0) {
		for (i = 0; i < rna->nch; i++) {
			if (rna->voice[i] != NULL) {
				AXSetVoiceState(rna->voice[i], AX_PB_STATE_STOP);
			}
		}
		for (i = 0; i < rna->maxnch; i++) {
			SJ_Reset(rna->sjrbf[i]);
		}
		rna->sw &= AXRNA_SW_TRANS;
	} else {
		RNAERR_CallErrFunc("E1070309:Illigal parameter(sw).\n");
	}
	GCRNA_UnlockCs();
}

void AXRNA_SetTransSw(AXRNA rna, Sint32 sw)
{
	Sint32 i;
	Sint32 j;
	Sint32 k;

	if (rna == NULL) {
		return;
	}
	if (sw == AXRNA_GetTransSw(rna)) {
		return;
	}
	if (sw == 1) {
		GCRNA_LockCs();
		for (i = 0; i < rna->nch; i++) {
			SJ_Reset(rna->sjrbf[i]);
			memset(&rna->trans_ck[i], 0, sizeof(SJCK));
			memset(&rna->free_ck[i], 0, sizeof(SJCK));
			memset(&rna->arq[i], 0, sizeof(ARQRequest));
			rna->trans_busy[i] = 0;
		}
		rna->trans_smpl = 0;
		rna->trans_total = 0;
		rna->flash_smpl = 0;
		rna->flash_total = 0;
		rna->play_pos = -1;
		rna->sw |= AXRNA_SW_TRANS;
		GCRNA_UnlockCs();
	} else if (sw == 0) {
		for (i = 0; i < rna->nch; i++) {
			for (j = 0; j < 200; j++) {
				if (rna->trans_busy[i] == 0) {
					break;
				}
				for (k = 0; k < 100000; k++) {
				}
			}
			if (j == 200) {
				RNAERR_CallErrFunc("E2071701:DMA transfer(data) to A-RAM did not finish.\n");
				return;
			}
			for (j = 0; j < 200; j++) {
				if (rna->flash_busy[i] == 0) {
					break;
				}
				for (k = 0; k < 100000; k++) {
				}
			}
			if (j == 200) {
				RNAERR_CallErrFunc("E2071701:DMA transfer(flash) to A-RAM did not finish.\n");
				return;
			}
		}
		rna->sw &= AXRNA_SW_PLAY;
	} else {
		RNAERR_CallErrFunc("E1070308:Illigal parameter(sw).\n");
	}
}

void AXRNA_Destroy(AXRNA rna)
{
	Sint32 i;

	if (rna == NULL) {
		return;
	}
	AXRNA_SetPlaySw(rna, 0);
	AXRNA_SetTransSw(rna, 0);
	for (i = 0; i < rna->maxnch; i++) {
		if (rna->sjrbf[i] != NULL) {
			SJ_Destroy(rna->sjrbf[i]);
		}
		if (rna->res[i] != NULL) {
			RNARES_Destroy(rna->res[i]);
		}
		GCRNA_LockCs();
		if (rna->voice[i] != NULL) {
			MIXReleaseChannel(rna->voice[i]);
			AXFreeVoice(rna->voice[i]);
		}
		GCRNA_UnlockCs();
	}
	memset(rna, 0, sizeof(AXRNA_OBJ));
}

AXRNA AXRNA_Create(SJ *sj, Sint32 maxnch)
{
	AXRNA rna;
	Uint32 i;
	Uint32 j;

	if (maxnch <= 0) {
		RNAERR_CallErrFunc("E1070301:Illigal parameter(maxnch<=0).\n");
		return NULL;
	}
	if (sj == NULL) {
		RNAERR_CallErrFunc("E1070302:Illigal parameter(sj=null).\n");
		return NULL;
	}
	for (i = 0; i < maxnch; i++) {
		if (sj[i] == NULL) {
			RNAERR_CallErrFunc("E1070303:Illigal parameter(sj[]=null).\n");
			return NULL;
		}
	}
	for (i = 0; i < AXRNA_MAX_OBJ; i++) {
		if (axrna_obj[i].used == 0) {
			break;
		}
	}
	if (i == AXRNA_MAX_OBJ) {
		RNAERR_CallErrFunc("E1070304:Not enough RNA handle.\n");
		return NULL;
	}
	rna = &axrna_obj[i];
	rna->nch = maxnch;
	rna->maxnch = maxnch;
	for (j = 0; j < rna->maxnch; j++) {
		rna->sj[j] = sj[j];
	}
	rna->outvol = 0;
	rna->span = 0x7F;
	rna->auxa = AXRNA_VOL_MIN;
	rna->auxb = AXRNA_VOL_MIN;
	rna->fader = 0;
	for (j = 0; j < rna->maxnch; j++) {
		rna->arq_owner[j] = 0x80000000 | (i * 2 + j);
		if ((rna->res[j] = RNARES_Create()) == NULL) {
			RNAERR_CallErrFunc("E1070305:Can't create RNARES.\n");
			AXRNA_Destroy(rna);
			return NULL;
		}
		rna->buf[j] = RNARES_GetBuf(rna->res[j]);
		rna->bufsize = RNARES_GetBufSize(rna->res[j]);
		rna->sjrbf[j] = SJRBF_Create((void *)(rna->buf[j] * 2), rna->bufsize * 2, 0);
		if (rna->sjrbf[j] == NULL) {
			RNAERR_CallErrFunc("E1070306:Can't create SJ.\n");
			AXRNA_Destroy(rna);
			return NULL;
		}
		if ((rna->voice[j] = AXAcquireVoice(31, axrna_voice_drop, 0)) == NULL) {
			RNAERR_CallErrFunc("E1070307:Can't acquire voice(AX).\n");
			AXRNA_Destroy(rna);
			return NULL;
		}
		GCRNA_LockCs();
		if (rna->voice[j] != NULL) {
			MIXInitChannel(rna->voice[j], 3, rna->outvol, rna->auxa, rna->auxb, 0x40, rna->span, rna->fader);
		}
		GCRNA_UnlockCs();
	}
	AXRNA_SetAdjsfreqFlg(rna, axrna_def_adjsfreq_fg);
	AXRNA_SetSrcType(rna, axrna_def_src_type);
	rna->adjsfreq_state = 0;
	AXRNA_SetSfreq(rna, AXRNA_DEF_SFREQ);
	AXRNA_SetBitPerSmpl(rna, 16);
	if (rna->maxnch == 2) {
		AXRNA_SetOutPan(rna, 0, -AXRNA_PAN_MAX);
		AXRNA_SetOutPan(rna, 1, AXRNA_PAN_MAX);
	} else {
		AXRNA_SetOutPan(rna, 0, 0);
	}
	rna->sw = 0;
	rna->used = 1;
	return rna;
}

static void axrna_voice_drop(void *p)
{
	Sint32 i;
	Sint32 j;

	for (i = 0; i < AXRNA_MAX_OBJ; i++) {
		for (j = 0; j < AXRNA_MAX_NCH; j++) {
			if (p == axrna_obj[i].voice[j]) {
				MIXReleaseChannel(axrna_obj[i].voice[j]);
				axrna_obj[i].voice[j] = NULL;
				return;
			}
		}
	}
}

void AXRNA_Finish(void)
{
	Sint32 i;

	if (--axrna_init_cnt == 0) {
		for (i = 0; i < AXRNA_MAX_OBJ; i++) {
			if (axrna_obj[i].used == 1) {
				AXRNA_Destroy(&axrna_obj[i]);
			}
		}
		memset(axrna_obj, 0, sizeof(axrna_obj));
		RNARES_Finish();
	}
}

void AXRNA_Init(void)
{
	axrna_build;
	if (axrna_init_cnt == 0) {
		RNARES_Init();
		memset(axrna_obj, 0, sizeof(axrna_obj));
		axrna_zero_dat = (void *)(((Uint32)axrna_zero_dat_real + (AXRNA_DMA_ALIGN - 1)) & ~(AXRNA_DMA_ALIGN - 1));
	}
	axrna_init_cnt++;
}

void AXRNA_EntryErrFunc(void (*func)(void *obj, Char8 *msg), void *obj)
{
	RNAERR_EntryErrFunc(func, obj);
}
