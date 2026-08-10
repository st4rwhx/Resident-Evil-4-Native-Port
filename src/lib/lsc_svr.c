/* CRI LSC: per-handle server */
#include "cri_xpt.h"
#include "lsc.h"
#include <string.h>

static Uint32 lsc_CalcSum(Char8 *fname)
{
	Uint32 len;
	Uint32 sum;
	Sint32 i;

	len = strlen(fname);
	sum = 0;
	for (i = 0; i < len; i++) {
		sum += (Uint8)fname[i];
	}
	return sum;
}

static void lsc_NextEntry(LSC lsc)
{
	LSC_ENTRY *ent;
	Char8 *fname = NULL;
	void *dir = NULL;
	Sint32 ofst = 0;
	Sint32 nsct = 0;

	if (lsc->stm != NULL) {
		if (lsc->loop_flag == 1) {
			ent = &lsc->tbl[lsc->rd_idx];
			fname = ent->fname;
			dir = ent->dir;
			ofst = ent->ofst;
			nsct = ent->nsct;
		}
		lsc->num_stm--;
		lsc->rd_idx = (lsc->rd_idx + 1) % LSC_MAX_ENTRY;
		if (lsc->num_stm <= 0) {
			LSC_CallStatFunc();
			lsc->stat = LSC_STAT_PREP;
		}
		if (lsc->loop_flag == 1) {
			LSC_EntryFileRange(lsc, fname, dir, ofst, nsct);
		}
	}
}

void lsc_ExecHndl(LSC lsc)
{
	LSC_ENTRY *ent;
	Uint32 sum;

	if (lsc->pause_flag == 1) {
		return;
	}
	if (lsc->stat != LSC_STAT_EXEC) {
		return;
	}
	if (lsc->num_stm <= 0) {
		return;
	}

	if (lsc->tbl[lsc->rd_idx].stat == LSC_ENT_LOADING) {
		if (lsc->stm == NULL) {
			LSC_CallErrFunc("E0007: lsc->fp=NULL\n");
		} else {
			ent = &lsc->tbl[lsc->rd_idx];
			switch (ADXSTM_GetStat(lsc->stm)) {
			case ADXSTM_STAT_ERROR:
				lsc->stat = LSC_STAT_ERROR;
				break;
			case ADXSTM_STAT_EXEC:
				ent->pos = ADXSTM_Tell(lsc->stm);
				break;
			case ADXSTM_STAT_END:
				ent->pos = lsc->cur_nsct;
				ent->stat = LSC_ENT_DONE;
				break;
			}
		}
	}

	if (lsc->tbl[lsc->rd_idx].stat == LSC_ENT_DONE) {
		lsc_NextEntry(lsc);
	}

	if (lsc->tbl[lsc->rd_idx].stat == LSC_ENT_WAIT) {
		ent = &lsc->tbl[lsc->rd_idx];
		if (lsc->num_stm > 0) {
			ADXSTM_StopNw(lsc->stm);
			ADXSTM_ReleaseFileNw(lsc->stm);
			sum = lsc_CalcSum(ent->fname);
			if (sum != ent->fname_sum) {
				LSC_CallErrFunc("E0013: '%s' is different from entry file name.(LSC_ExecServer)\n", ent->fname);
			} else {
				ADXSTM_BindFileNw(lsc->stm, ent->fname, ent->dir, ent->ofst, ent->nsct);
				ADXSTM_SetEos(lsc->stm, ent->nsct);
				lsc->cur_nsct = ent->nsct;
				ent->pos = 0;
				lsc->stm_start = 0;
				if (lsc->stm_start == 0) {
					ADXSTM_SetBufSize(lsc->stm, lsc->min_val, lsc->bufsize);
					ADXSTM_Seek(lsc->stm, 0);
					ADXSTM_Start(lsc->stm);
					lsc->stm_start = 1;
				}
				ent->stat = LSC_ENT_LOADING;
			}
		}
	}
}
