#include "cri_xpt.h"

extern void cvFsEntryErrFunc(void (*func)(void *obj, Char8 *msg), void *obj);
extern void cvFsAddDev(const Char8 *name, void *(*getif)(void), void *arg);
extern void cvFsSetDefDev(const Char8 *name);
extern void *mfCiGetInterface(void);
extern void *gcCiGetInterface(void);
extern void gcCiSetRdMode(void *obj, Sint32 a, Sint32 b, Sint32 mode);
extern void ADXERR_CallErrFunc1(Char8 *msg);

typedef struct {
	Sint32 rdmode;
} ADXGC_DVDFS_PRM;

/* volatile: the build string must stay referenced (dead `lwz` through the .rodata pool) */
static const Char8 *const volatile adxgcsdk_build =
	"\nADXGCSDK Ver.20Apr2004Patch1 Build:Oct  8 2004 13:33:21\n";

void adxgc_err_dvd(void *obj, Char8 *msg);

void ADXGC_SetupDvdFs(ADXGC_DVDFS_PRM *prm)
{
	adxgcsdk_build;
	cvFsEntryErrFunc(adxgc_err_dvd, NULL);
	cvFsAddDev("MFS", mfCiGetInterface, NULL);
	cvFsEntryErrFunc(adxgc_err_dvd, NULL);
	cvFsAddDev("GCD", gcCiGetInterface, NULL);
	cvFsSetDefDev("GCD");
	if (prm != NULL) {
		gcCiSetRdMode(NULL, 0, 0, prm->rdmode);
	} else {
		gcCiSetRdMode(NULL, 0, 0, 0);
	}
}

void adxgc_err_dvd(void *obj, Char8 *msg)
{
	ADXERR_CallErrFunc1(msg);
}
