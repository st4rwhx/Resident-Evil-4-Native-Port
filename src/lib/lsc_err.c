#include "cri_xpt.h"
#include <stdarg.h>
#include <stdio.h>

static void (*lsc_err_func)(void *obj, Char8 *msg) = NULL;
static void *lsc_err_obj = NULL;
static Char8 lsc_err_msg[256];

void LSC_CallErrFunc(Char8 *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsprintf(lsc_err_msg, fmt, ap);
	va_end(ap);
	if (lsc_err_func != NULL) {
		lsc_err_func(lsc_err_obj, lsc_err_msg);
	}
}

void LSC_EntryErrFunc(void (*func)(void *obj, Char8 *msg), void *obj)
{
	if (func == NULL) {
		lsc_err_func = NULL;
		lsc_err_obj = NULL;
	} else {
		lsc_err_func = func;
		lsc_err_obj = obj;
	}
}
