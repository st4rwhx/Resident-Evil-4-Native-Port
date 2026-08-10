#include "cri_xpt.h"
#include <string.h>

static void (*rnaerr_func)(void *obj, Char8 *msg) = NULL;
static void *rnaerr_obj = NULL;
static Char8 rnaerr_msg[256];

void RNAERR_CallErrFunc(Char8 *msg)
{
	strncpy(rnaerr_msg, msg, 255);
	if (rnaerr_func != NULL) {
		rnaerr_func(rnaerr_obj, rnaerr_msg);
	}
}

void RNAERR_EntryErrFunc(void (*func)(void *obj, Char8 *msg), void *obj)
{
	rnaerr_func = func;
	rnaerr_obj = obj;
}
