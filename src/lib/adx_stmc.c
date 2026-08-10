/* CRI ADX stream controller: reads file sectors through the CVFS into a stream joint */
#include "cri_xpt.h"
#include "sj.h"
#include "adx_stm.h"
#include "adx_t.h"
#include <string.h>

extern void SVM_Lock(void);
extern void SVM_Unlock(void);
extern Sint32 SVM_TestAndSet(Sint32 *flg);
extern void ADXCRS_Lock(void);
extern void ADXCRS_Unlock(void);
extern void ADXERR_CallErrFunc2(const Char8 *msg1, const Char8 *msg2);
extern void *cvFsOpen(const Char8 *fname, void *dir, void *prm);
extern void cvFsClose(void *fs);
extern Sint32 cvFsSeek(void *fs, Sint32 pos, Sint32 type);
extern Sint32 cvFsTell(void *fs);
extern Sint32 cvFsGetFileSize(const Char8 *fname);
extern Sint32 cvFsReqRd(void *fs, Sint32 nsct, void *buf);
extern Sint32 cvFsGetStat(void *fs);
extern void cvFsStopTr(void *fs);

#define ADXSTM_MAX_OBJ 40
#define ADXSTM_SCT_SHIFT 11
#define ADXSTM_SCT_SIZE 2048
#define ADXSTM_NSCT_INF 0xFFFFF
#define ADXSTM_MIN(a, b) ((a) < (b) ? (a) : (b))

/* cvFsGetStat results */
#define CVFS_STAT_COMPLETE 1
#define CVFS_STAT_ERROR 3

typedef struct ADXSTM_OBJ {
	Sint8 used;                /* 0x00 */
	Sint8 stat;                /* 0x01 */
	Sint8 rd_flg;              /* 0x02 read request outstanding */
	Sint8 rtry_cnt;            /* 0x03 */
	SJ sj;                     /* 0x04 */
	void *fs;                  /* 0x08 */
	Sint32 ofst;               /* 0x0C file offset (sectors) */
	Sint32 fsize;              /* 0x10 bytes */
	Sint32 nsct;               /* 0x14 */
	Sint32 rd_max;             /* 0x18 chunk request size */
	Sint32 rd_min;             /* 0x1C buffer level below which reads are issued */
	Sint32 rd_nsct;            /* 0x20 sectors of the outstanding request */
	SJCK ck;                   /* 0x24 */
	Sint32 rd_lim;             /* 0x2C sectors per request */
	Sint32 eos_nsct;           /* 0x30 */
	Sint32 rd_byte;            /* 0x34 total bytes read */
	void (*eos_func)(void *obj); /* 0x38 */
	void *eos_obj;             /* 0x3C */
	Sint32 buf_size;           /* 0x40 */
	Sint8 x44;                 /* 0x44 */
	Sint8 bind_req;            /* 0x45 */
	Sint8 release_req;         /* 0x46 */
	Sint8 start_req;           /* 0x47 */
	Sint8 stop_req;            /* 0x48 */
	Sint8 bound;               /* 0x49 */
	Sint8 rtim;                /* 0x4A */
	Uint8 pad4b;
	Sint32 x4c;
	const Char8 *fname;        /* 0x50 */
	void *dir;                 /* 0x54 */
	Sint32 pos;                /* 0x58 */
	Uint32 lim_nsct;           /* 0x5C */
} ADXSTM_OBJ;

static Sint32 adxstmf_rtim_ofst = 0;
static Sint32 adxstmf_rtim_num = 16;
static Sint32 adxstmf_nrml_ofst = 16;
static Sint32 adxstmf_nrml_num = 24;
Sint32 adxstmf_execsvr_flg = 0;
Sint32 adxstmf_num_rtry = 0;
static Sint32 adxstm_sj_internal_error_cnt = 0;
ADXSTM_OBJ adxstmf_obj[ADXSTM_MAX_OBJ];

void ADXSTMF_ExecHndl(ADXSTM stm);
void adxstmf_stat_exec(ADXSTM stm);

Sint32 ADXSTM_SetBufSize(ADXSTM stm, Sint32 min_nsct, Sint32 max_nsct)
{
	stm->rd_min = min_nsct;
	stm->rd_max = max_nsct;
	return 1;
}

void ADXSTM_ExecServer(void)
{
	Sint32 i;
	ADXSTM stm;

	if (SVM_TestAndSet(&adxstmf_execsvr_flg) == 0) {
		return;
	}
	for (i = 0; i < ADXSTM_MAX_OBJ; i++) {
		stm = &adxstmf_obj[i];
		if (stm->used == 1) {
			ADXSTMF_ExecHndl(stm);
		}
	}
	adxstmf_execsvr_flg = 0;
}

void ADXSTMF_ExecHndl(ADXSTM stm)
{
	void *fs;
	Sint32 fnsct;
	Sint32 fsize;

	if (stm->rd_flg == 0) {
		if (stm->stop_req == 1) {
			stm->stop_req = 0;
			if (stm->start_req == 0) {
				stm->stat = ADXSTM_STAT_PREP;
			}
		}
		if (stm->release_req == 1) {
			fs = stm->fs;
			if (fs != NULL) {
				stm->fs = NULL;
				cvFsClose(fs);
			}
			stm->release_req = 0;
			stm->bound = 0;
		}
		SVM_Lock();
		if (stm->bind_req == 1) {
			stm->bound = 1;
			SVM_Unlock();
			if (stm->fs == NULL) {
				fs = cvFsOpen(stm->fname, stm->dir, NULL);
				stm->fs = fs;
				if (fs == NULL) {
					ADXERR_CallErrFunc2("E02110501 adxstmf_stat_exec: can't open ", stm->fname);
					stm->stat = ADXSTM_STAT_ERROR;
					stm->bound = 0;
					stm->bind_req = 0;
					return;
				}
				cvFsSeek(stm->fs, 0, 2);
				fnsct = cvFsTell(stm->fs);
				if (stm->dir == NULL) {
					fsize = cvFsGetFileSize(stm->fname);
				} else {
					fsize = fnsct << ADXSTM_SCT_SHIFT;
				}
				cvFsSeek(stm->fs, 0, 0);
				if (stm->fsize == (ADXSTM_NSCT_INF << ADXSTM_SCT_SHIFT)) {
					stm->fsize = fsize;
					stm->nsct = fnsct;
				}
				if (stm->ofst > fnsct) {
					stm->ofst = fnsct;
				}
				if (stm->nsct + stm->ofst > fnsct) {
					stm->nsct = fnsct - stm->ofst;
					stm->fsize = stm->nsct << ADXSTM_SCT_SHIFT;
				}
				stm->pos = 0;
				if (stm->pos > stm->nsct) {
					stm->pos = stm->nsct;
				}
				stm->bind_req = 0;
			}
		} else {
			SVM_Unlock();
		}
		if (stm->start_req == 1) {
			stm->start_req = 0;
		}
	}
	if (stm->stat == ADXSTM_STAT_EXEC && stm->bound == 1) {
		adxstmf_stat_exec(stm);
	}
}

static void adxstmf_retry(ADXSTM stm)
{
	if (adxstmf_num_rtry >= 0) {
		if (stm->rtry_cnt >= adxstmf_num_rtry) {
			stm->stat = ADXSTM_STAT_ERROR;
		} else {
			stm->rtry_cnt++;
		}
	}
}

void adxstmf_stat_exec(ADXSTM stm)
{
	SJ sj;
	Sint32 fstat;
	Sint32 nbyte;
	Sint32 nsct;
	Sint32 rd;
	Sint32 n;
	SJCK ck1;
	SJCK ck2;
	SJCK ck;

	sj = stm->sj;
	fstat = cvFsGetStat(stm->fs);
	SVM_Lock();
	if (stm->rd_flg == 1) {
		if (fstat == CVFS_STAT_COMPLETE) {
			stm->rd_flg = 0;
			SVM_Unlock();
			nbyte = stm->rd_nsct << ADXSTM_SCT_SHIFT;
			SJ_SplitChunk(&stm->ck, nbyte, &ck1, &ck2);
			SJ_PutChunk(sj, SJ_CK_DATA, &ck1);
			SJ_UngetChunk(sj, SJ_CK_FREE, &ck2);
			stm->pos += stm->rd_nsct;
			stm->rd_byte += nbyte;
			stm->ck.data = NULL;
			stm->ck.len = 0;
			nsct = stm->nsct;
			if (stm->pos == stm->eos_nsct && stm->eos_func != NULL) {
				stm->eos_func(stm->eos_obj);
			}
			if (stm->pos >= nsct) {
				stm->stat = ADXSTM_STAT_END;
			} else if ((Uint32)stm->rd_byte >> ADXSTM_SCT_SHIFT >= stm->lim_nsct &&
				   stm->lim_nsct < ADXSTM_NSCT_INF) {
				stm->stat = ADXSTM_STAT_END;
			}
			stm->rtry_cnt = 0;
		} else if (fstat == CVFS_STAT_ERROR) {
			stm->rd_flg = 0;
			SVM_Unlock();
			SJ_UngetChunk(sj, SJ_CK_FREE, &stm->ck);
			stm->ck.data = NULL;
			stm->ck.len = 0;
			adxstmf_retry(stm);
		} else {
			SVM_Unlock();
		}
		return;
	}
	stm->rd_flg = 1;
	stm->ck.data = NULL;
	stm->ck.len = 0;
	SVM_Unlock();
	if (stm->x44 == 1 || stm->stop_req == 1) {
		stm->rd_flg = 0;
		return;
	}
	if (stm->nsct == 0) {
		stm->rd_flg = 0;
		stm->rd_nsct = 0;
		stm->stat = ADXSTM_STAT_END;
		return;
	}
	if (sj == NULL || sj->vtbl == NULL) {
		stm->rd_flg = 0;
		adxstm_sj_internal_error_cnt++;
		return;
	}
	if (stm->buf_size - SJ_GetNumData(sj, SJ_CK_FREE) >= stm->rd_min) {
		stm->rd_flg = 0;
		return;
	}
	SJ_GetChunk(sj, SJ_CK_FREE, stm->rd_max, &ck);
	n = ck.len / ADXSTM_SCT_SIZE;
	n = ADXSTM_MIN(n, stm->eos_nsct - stm->pos);
	n = ADXSTM_MIN(n, stm->nsct - stm->pos);
	rd = ADXSTM_MIN(n, stm->rd_lim);
	cvFsSeek(stm->fs, stm->ofst + stm->pos, 0);
	if (stm->lim_nsct != ADXSTM_NSCT_INF) {
		n = stm->lim_nsct - stm->rd_byte / ADXSTM_SCT_SIZE;
		if (rd < n) {
			n = rd;
		}
		rd = n;
	}
	stm->rd_nsct = cvFsReqRd(stm->fs, rd, ck.data);
	stm->ck.data = ck.data;
	stm->ck.len = ck.len;
	if (stm->rd_nsct <= 0) {
		SJ_UngetChunk(sj, SJ_CK_FREE, &stm->ck);
		stm->ck.data = NULL;
		stm->ck.len = 0;
		stm->rd_flg = 0;
		if (cvFsGetStat(stm->fs) == CVFS_STAT_ERROR) {
			adxstmf_retry(stm);
		}
	}
}

void ADXSTM_SetEos(ADXSTM stm, Sint32 nsct)
{
	if (nsct >= 0) {
		stm->eos_nsct = nsct;
	} else {
		stm->eos_nsct = stm->nsct;
	}
}

void ADXSTM_EntryEosFunc(ADXSTM stm, void (*func)(void *obj), void *obj)
{
	stm->eos_func = func;
	stm->eos_obj = obj;
}

static void adxstm_stop_nw(ADXSTM stm)
{
	SVM_Lock();
	if (stm->stat == ADXSTM_STAT_EXEC && stm->rd_flg == 1) {
		stm->stop_req = 1;
		if (stm->start_req == 1) {
			stm->start_req = 0;
		}
	} else {
		stm->stat = ADXSTM_STAT_PREP;
	}
	SVM_Unlock();
}

static void adxstm_stop(ADXSTM stm)
{
	if (stm->fs != NULL && stm->release_req == 0) {
		cvFsStopTr(stm->fs);
	}
	SVM_Lock();
	stm->stat = ADXSTM_STAT_PREP;
	stm->rd_flg = 0;
	stm->ck.data = NULL;
	SVM_Unlock();
	adxstm_stop_nw(stm);
	do {
		ADXT_ExecFsSvr();
	} while (stm->stat != ADXSTM_STAT_PREP || stm->ck.data != NULL);
}

static void adxstm_release_nw(ADXSTM stm)
{
	adxstm_stop_nw(stm);
	SVM_Lock();
	if (stm->bound == 1) {
		stm->release_req = 1;
	}
	stm->bind_req = 0;
	SVM_Unlock();
}

static void adxstm_release(ADXSTM stm)
{
	adxstm_stop(stm);
	adxstm_release_nw(stm);
	for (;;) {
		if (stm->bound == 0) {
			break;
		}
		ADXT_ExecFsSvr();
	}
}

void ADXSTM_Stop(ADXSTM stm)
{
	adxstm_stop(stm);
}

void ADXSTM_StopNw(ADXSTM stm)
{
	adxstm_stop_nw(stm);
}

Sint32 ADXSTM_Start(ADXSTM stm)
{
	ADXCRS_Lock();
	stm->rd_byte = 0;
	stm->rtry_cnt = 0;
	if (stm->nsct == 0) {
		stm->stat = ADXSTM_STAT_END;
	} else {
		stm->stat = ADXSTM_STAT_EXEC;
	}
	stm->rd_flg = 0;
	stm->ck.data = NULL;
	stm->ck.len = 0;
	stm->start_req = 1;
	stm->lim_nsct = ADXSTM_NSCT_INF;
	ADXCRS_Unlock();
	return 1;
}

Sint32 ADXSTM_Tell(ADXSTM stm)
{
	if (stm->fs != NULL) {
		return stm->pos;
	}
	return 0;
}

Sint32 ADXSTM_Seek(ADXSTM stm, Sint32 pos)
{
	stm->pos = pos;
	if (stm->pos > stm->nsct) {
		stm->pos = stm->nsct;
	}
	return stm->pos;
}

Sint32 ADXSTM_GetStat(ADXSTM stm)
{
	return stm->stat;
}

void ADXSTM_ReleaseFile(ADXSTM stm)
{
	adxstm_release(stm);
}

void ADXSTM_ReleaseFileNw(ADXSTM stm)
{
	adxstm_release_nw(stm);
}

void ADXSTM_BindFileNw(ADXSTM stm, const Char8 *fname, void *dir, Sint32 ofst, Sint32 nsct)
{
	SVM_Lock();
	stm->ofst = ofst;
	stm->fsize = nsct << ADXSTM_SCT_SHIFT;
	stm->nsct = nsct;
	stm->fname = fname;
	stm->dir = dir;
	stm->bind_req = 1;
	SVM_Unlock();
}

void ADXSTM_Destroy(ADXSTM stm)
{
	if (stm == NULL) {
		return;
	}
	adxstm_stop(stm);
	adxstm_release(stm);
	stm->used = 0;
	memset(stm, 0, sizeof(ADXSTM_OBJ));
}

static inline ADXSTM adxstmf_create(SJ sj, Sint32 ofst, Sint32 num, Sint32 rtim)
{
	ADXSTM stm = NULL;
	Sint32 i;

	for (i = 0; i < num; i++) {
		stm = (ADXSTM)((Uint8 *)adxstmf_obj + ofst * sizeof(ADXSTM_OBJ));
		if (stm->used == 0) {
			break;
		}
		ofst++;
	}
	if (i == num) {
		return NULL;
	}
	ADXCRS_Lock();
	stm->stat = ADXSTM_STAT_PREP;
	stm->rd_flg = 0;
	stm->sj = sj;
	stm->fs = NULL;
	stm->ofst = 0;
	stm->fsize = 0;
	stm->nsct = 0;
	stm->rd_lim = 0x200;
	stm->pos = 0;
	stm->lim_nsct = ADXSTM_NSCT_INF;
	stm->eos_nsct = stm->nsct;
	if (stm->sj != NULL) {
		stm->buf_size = SJ_GetNumData(sj, SJ_CK_FREE) + SJ_GetNumData(sj, SJ_CK_DATA);
		stm->rd_min = stm->rd_max = stm->buf_size;
	}
	stm->x44 = 0;
	stm->used = 1;
	ADXCRS_Unlock();
	stm->rtim = rtim;
	return stm;
}

/* adxstmf_create's search: the byte-offset cast form `(Uint8 *)adxstmf_obj + ofst * sizeof(..)` with
 * `ofst++` after the test keeps the scaled index as the IV (stepped in the latch after the `beq`,
 * `add r31, base, ofs` per iteration); `&adxstmf_obj[ofst++]` steps before the load and
 * `&adxstmf_obj[ofst]; ...; ofst++` becomes a pointer IV. */
ADXSTM ADXSTM_Create(SJ sj, Sint32 mode)
{
	if (mode < 0x100) {
		return adxstmf_create(sj, adxstmf_rtim_ofst, adxstmf_rtim_num, 1);
	}
	return adxstmf_create(sj, adxstmf_nrml_ofst, adxstmf_nrml_num, 0);
}

void ADXSTM_Finish(void)
{
}

Sint32 ADXSTM_Init(void)
{
	memset(adxstmf_obj, 0, sizeof(adxstmf_obj));
	return 1;
}
