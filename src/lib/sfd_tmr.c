#include "cri_xpt.h"

typedef struct {
	Sint64 sum;
	Sint64 min;
	Sint64 max;
	Sint32 cnt;
} SFTMR_TSUM;

#define SFTMR_MAX(a, b) ((a) > (b) ? (a) : (b))

void SFTMR_AddTsum(SFTMR_TSUM *ts, Sint64 t)
{
	Sint64 min;

	ts->sum += t;
	min = ts->min;
	if (t < min) {
		min = t;
	}
	ts->min = min;
	ts->max = SFTMR_MAX(t, ts->max);
	ts->cnt++;
}

void SFTMR_InitTsum(SFTMR_TSUM *ts)
{
	ts->sum = 0;
	ts->min = 0x7FFFFFFFFFFFFFFF;
	ts->max = 0;
	ts->cnt = 0;
}
