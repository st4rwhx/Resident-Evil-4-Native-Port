#include "cri_xpt.h"

static Sint32 utytmr_ch = 0;
static volatile Sint32 utytmr_init_cnt = 0;
static Sint64 utytmr_unit;
static Uint32 utytmr_work[6];

/* time base read (upper, lower, upper again until stable) */
static Sint64 utytmr_read(void)
{
	register Uint32 hi, lo, hi2;
	asm {
	loop:
		mftb hi, 269
		mftb lo, 268
		mftb hi2, 269
		cmpw hi2, hi
		bne loop
	}
	return ((Sint64)hi << 32) | lo;
}

/* dead-stripped by the linker; they own the .rodata constants (int->float magic, 1000.0f, 1000000.0f;
 * MWCC emits one pool per function in function order) */
Float32 UTY_TmrToFloat(Sint32 cnt)
{
	return (Float32)cnt;
}

Float32 UTY_TmrToMsec(Float32 t)
{
	return t * 1000.0f;
}

Float32 UTY_TmrToUsec(Float32 t)
{
	return t * 1000000.0f;
}

Sint64 UTY_GetTmrUnit(void)
{
	return utytmr_unit;
}

Bool UTY_IsTmrVoid(void)
{
	if (utytmr_init_cnt > 0 && utytmr_ch != -1) {
		utytmr_read();
	}
	return utytmr_unit == 1;
}

Sint64 UTY_GetTmr(void)
{
	if (utytmr_init_cnt <= 0 || utytmr_ch == -1) {
		return 0;
	}
	return utytmr_read();
}

void UTY_FinishTmr(void)
{
	Sint32 cnt;

	utytmr_init_cnt--;
	cnt = utytmr_init_cnt;
	if (utytmr_init_cnt < 0) {
		utytmr_init_cnt = 0;
	}
}

void UTY_InitTmr(Sint32 ch)
{
	utytmr_init_cnt++;
	if (utytmr_init_cnt > 1) {
		if (utytmr_ch == ch) {
			return;
		}
	}
	utytmr_ch = ch;
	if (ch == -1) {
		utytmr_unit = 1;
		return;
	}
	utytmr_unit = (*(Uint32 *)0x800000F8) >> 2;
}
