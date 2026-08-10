/* CRI CVFS GameCube DVD device (GCCI): sector reads through the Dolphin DVD API */
#include "cvfs.h"
#include <string.h>
#include <dolphin/os.h>
#include <dolphin/dvd.h>

#define GCCI_MAX_OBJ 40
#define GCCI_DEF_SCTLEN 0x800
#define GCCI_PATH_LEN 260
#define GCCI_CANCEL_TIMEOUT 2000

/* GCCI_OBJ.stat */
#define GCCI_STAT_STOP 0
#define GCCI_STAT_COMPLETE 1
#define GCCI_STAT_READING 2
#define GCCI_STAT_ERROR 3

typedef struct {
	Sint8 used;                /* 0x00 */
	Sint8 pad01;
	Sint8 stat;                /* 0x02 */
	Sint8 pad03;
	Sint32 pad04;
	Uint8 *buf;                /* 0x08 */
	Sint32 cbstat;             /* 0x0C last DVDGetCommandBlockStatus */
	Sint32 sctlen;             /* 0x10 */
	Sint32 fsize_byte;         /* 0x14 */
	Sint32 fsize_sct;          /* 0x18 */
	Sint32 pos_sct;            /* 0x1C */
	Sint32 numtr;              /* 0x20 bytes transferred by the last request */
	Sint32 rqsct;              /* 0x24 sectors of the last request */
	DVDFileInfo fi;            /* 0x28 */
} GCCI_OBJ;

typedef GCCI_OBJ *GCCI;

/* debug information */
typedef struct {
	Sint32 cbstat;             /* 0x00 last command block status seen */
	Sint8 stat;                /* 0x04 last handle state set */
	Sint8 pad05[3];
	Sint32 cancel;             /* 0x08 inside DVDCancel */
} GCCI_DEBUG;

extern Sint32 gcg_ci_rdmode;
extern Char8 gcg_ci_root_dir[256];

const Char8 gcg_ci_build_str[] = "\nGCCI Ver.1.09 Build:Oct  8 2004 13:31:49\n";

GCCI_DEBUG gcg_ci_debug;
void *gcg_ci_err_obj;
CVFS_ERRFUNC gcg_ci_err_func;
GCCI_OBJ gcg_ci_obj[GCCI_MAX_OBJ];
const Char8 *volatile gcg_ci_build_ptr;

void gcCiExecServer(void);
void gcCiEntryErrFunc(CVFS_ERRFUNC func, void *obj);
Sint32 gcCiGetFileSize(const Char8 *fname);
GCCI gcCiOpen(const Char8 *fname, void *dir, Sint32 rw);
void gcCiClose(void *hn);
Sint32 gcCiSeek(GCCI gcci, Sint32 pos, Sint32 type);
Sint32 gcCiTell(GCCI gcci);
Sint32 gcCiReqRd(void *hn, Sint32 nsct, Uint8 *buf);
void gcCiStopTr(void *hn);
static Sint32 gcCiGetStat(GCCI gcci);
Sint32 gcCiGetSctLen(GCCI gcci);
void gcCiSetSctLen(GCCI gcci, Sint32 sctlen);
Sint32 gcCiGetNumTr(GCCI gcci);

CVFS_IF gcg_ci_vtbl = {
	gcCiExecServer,
	gcCiEntryErrFunc,
	gcCiGetFileSize,
	NULL,
	(void *(*)(const Char8 *, void *, Sint32))gcCiOpen,
	gcCiClose,
	(Sint32 (*)(void *, Sint32, Sint32))gcCiSeek,
	(Sint32 (*)(void *))gcCiTell,
	(Sint32 (*)(void *, Sint32, void *))gcCiReqRd,
	NULL,
	gcCiStopTr,
	(Sint32 (*)(void *))gcCiGetStat,
	(Sint32 (*)(void *))gcCiGetSctLen,
	(void (*)(void *, Sint32))gcCiSetSctLen,
	(Sint32 (*)(void *))gcCiGetNumTr,
};

/* dead: first references of the .bss objects (debug, err_obj, err_func) */
void gcCiInit(void)
{
	gcg_ci_debug.cbstat = 0;
	gcg_ci_err_obj = NULL;
	gcg_ci_err_func = NULL;
}

static void gcci_CallErr(const Char8 *msg, void *hn)
{
	if (gcg_ci_err_func != NULL) {
		gcg_ci_err_func(gcg_ci_err_obj, msg, hn);
	}
}

/* the handle is not reading */
static Bool gcci_IsIdle(GCCI gcci)
{
	Bool idle = 0;

	if (gcci->stat == GCCI_STAT_COMPLETE || gcci->stat == GCCI_STAT_STOP) {
		idle = 1;
	}
	return idle;
}

/* the last DVD command finished (done or canceled) */
static Bool gcci_IsCmdDone(GCCI gcci)
{
	Bool done = 0;

	if (gcci->cbstat == DVD_STATE_END || gcci->cbstat == DVD_STATE_CANCELED) {
		done = 1;
	}
	return done;
}

void gcci_rd_cbfn(s32 result, DVDFileInfo *fi)
{
}

Sint32 gcCiGetNumTr(GCCI gcci)
{
	if (gcci == NULL) {
		gcci_CallErr("E0092912:handl is null.", NULL);
		return 0;
	}
	return gcci->numtr;
}

void gcCiSetSctLen(GCCI gcci, Sint32 sctlen)
{
	Sint32 pos_byte;

	if (gcci == NULL) {
		gcci_CallErr("E0040302:handl is null.", NULL);
		return;
	}
	if (gcci->sctlen % 32 != 0) {
		gcci_CallErr("E0040303:invalidate size.", NULL);
		return;
	}
	pos_byte = gcci->pos_sct * gcci->sctlen;
	gcci->sctlen = sctlen;
	/* `fb + (sl - 1)`: the frontend moves the constant out (`add sl, fb; subi`) -- `(sl + fb - 1)` gives
	 * `subi fb; add`, and a named sum local is substituted back into the same shape */
	gcci->fsize_sct = (gcci->fsize_byte + (gcci->sctlen - 1)) / gcci->sctlen;
	gcci->pos_sct = pos_byte / gcci->sctlen;
	gcci->numtr = gcci->rqsct * sctlen;
}

Sint32 gcCiGetSctLen(GCCI gcci)
{
	if (gcci == NULL) {
		gcci_CallErr("E0040301:handl is null.", NULL);
		return 0;
	}
	return gcci->sctlen;
}

static Sint32 gcCiGetStat(GCCI gcci)
{
	if (gcci == NULL) {
		gcci_CallErr("E0092912:handl is null.", NULL);
		return 0;
	}
	return gcci->stat;
}

void gcCiStopTr(void *hn)
{
	GCCI gcci;
	Sint32 ret;
	Uint32 t0;
	Uint32 t;
	Uint32 dt;

	gcci = hn;
	if (gcci == NULL) {
		gcci_CallErr("E0092912:handl is null.", NULL);
		return;
	}
	if (gcci->stat == GCCI_STAT_COMPLETE || gcci->stat == GCCI_STAT_STOP) {
		return;
	}
	DVDGetCommandBlockStatus(&gcci->fi.cb);
	DVDGetDriveStatus();
	gcg_ci_debug.cancel = 1;
	ret = DVDCancel(&gcci->fi.cb);
	gcg_ci_debug.cancel = 0;
	if (ret < 0) {
		gcci_CallErr("E0092917:DVDCancel failed.", gcci);
		return;
	}
	t0 = OSTicksToMilliseconds(OSGetTick());
	while (!gcci_IsCmdDone(gcci)) {
		gcci->cbstat = DVDGetCommandBlockStatus(&gcci->fi.cb);
		gcg_ci_debug.cbstat = gcci->cbstat;
		t = OSTicksToMilliseconds(OSGetTick());
		dt = (t >= t0) ? (t - t0) : (0xFFFFFFFF - t0 + t);
		if (dt > GCCI_CANCEL_TIMEOUT) {
			gcci_CallErr("E0092918:DVDCancel time out.", gcci);
			break;
		}
	}
	gcci->stat = GCCI_STAT_STOP;
	gcg_ci_debug.stat = GCCI_STAT_STOP;
	DVDGetCommandBlockStatus(&gcci->fi.cb);
	DVDGetDriveStatus();
}

static GCCI gcci_GetFreeHn(void)
{
	GCCI gcci = NULL;
	Sint32 i;

	for (i = 0; i < GCCI_MAX_OBJ; i++) {
		if (gcg_ci_obj[i].used == 0) {
			gcci = &gcg_ci_obj[i];
			break;
		}
	}
	return gcci;
}

/* a handle of the table has a read in flight */
static Bool gcci_IsBusy(GCCI tbl)
{
	Sint32 i;

	for (i = 0; i < GCCI_MAX_OBJ; i++) {
		if (tbl[i].used == 1 && tbl[i].stat == GCCI_STAT_READING) {
			return 1;
		}
	}
	return 0;
}

/* finish one handle's read when it completed. Per-handle helper (over declared first): its locals are
 * created at inlining, so they rank above the later range-split copy of `over` (the CANCELED
 * redefinition takes over's register r27/r23 in both callers); the table loop is written twice
 * (gcci_ExecServer for gcCiReqRd, gcCiExecServer's own loop) because the counter must rank above the
 * induction pointer in ReqRd (inlined local) and below it in gcCiExecServer (own local); CRI pass 13 */
static inline void gcci_ExecOne(GCCI ci)
{
	Sint32 over;
	Sint32 nbyte;
	Uint8 *p;

	if (ci->used == 1 && ci->stat == GCCI_STAT_READING) {
		ci->cbstat = DVDGetCommandBlockStatus(&ci->fi.cb);
		gcg_ci_debug.cbstat = ci->cbstat;
		switch (ci->cbstat) {
		case DVD_STATE_FATAL_ERROR:
			ci->stat = GCCI_STAT_ERROR;
			gcg_ci_debug.stat = GCCI_STAT_ERROR;
			break;
		case DVD_STATE_END:
			nbyte = ci->rqsct * ci->sctlen;
			DCInvalidateRange(ci->buf, nbyte);
			ci->numtr = nbyte;
			ci->pos_sct += ci->rqsct;
			if (ci->pos_sct * ci->sctlen > ci->fsize_byte) {
				over = ci->pos_sct * ci->sctlen - ci->fsize_byte;
				p = ci->buf + ci->numtr - over;
				memset(p, 0, over);
				DCStoreRange(p, over);
			}
			ci->stat = GCCI_STAT_COMPLETE;
			gcg_ci_debug.stat = GCCI_STAT_COMPLETE;
			break;
		case DVD_STATE_CANCELED:
			over = DVDGetTransferredSize(&ci->fi);
			DCInvalidateRange(ci->buf, over);
			ci->numtr = ci->sctlen * (over / ci->sctlen);
			ci->pos_sct += over / ci->sctlen;
			ci->stat = GCCI_STAT_STOP;
			gcg_ci_debug.stat = GCCI_STAT_STOP;
			break;
		}
	}
}

/* finish the reads of the table that completed */
static inline void gcci_ExecServer(GCCI tbl)
{
	Sint32 i;

	for (i = 0; i < GCCI_MAX_OBJ; i++) {
		gcci_ExecOne(&tbl[i]);
	}
}

Sint32 gcCiReqRd(void *hn, Sint32 nsct, Uint8 *buf)
{
	GCCI gcci;
	Sint32 ofst;
	Sint32 nbyte;
	Sint32 ret;

	gcci = hn;
	if (gcci == NULL) {
		gcci_CallErr("E0092912:handl is null.", NULL);
		return 0;
	}
	if (nsct < 0) {
		gcci_CallErr("E0092913:nsct < 0.(gcCiReqRd)", gcci);
		return 0;
	}
	if (buf == NULL) {
		gcci_CallErr("E0092914:buf is null.(gcCiReqRd)", gcci);
		return 0;
	}
	if (!gcci_IsIdle(gcci)) {
		return 0;
	}
	if (!gcci_IsCmdDone(gcci)) {
		return 0;
	}
	if (gcci_IsBusy(gcg_ci_obj)) {
		return 0;
	}
	if (nsct == 0) {
		gcci->stat = GCCI_STAT_COMPLETE;
		gcg_ci_debug.stat = GCCI_STAT_COMPLETE;
		return 0;
	}
	gcci->numtr = 0;
	gcci->buf = buf;
	gcci->rqsct = nsct;
	gcci_ExecServer(gcg_ci_obj);
	ofst = gcci->pos_sct * gcci->sctlen;
	nbyte = gcci->rqsct * gcci->sctlen;
	if (ofst + nbyte > gcci->fsize_byte) {
		nbyte = gcci->fsize_byte - ofst;
		if (nbyte < 0) {
			gcci->stat = GCCI_STAT_COMPLETE;
			gcg_ci_debug.stat = GCCI_STAT_COMPLETE;
			return nsct;
		}
	}
	nbyte = (nbyte + 31) & ~31;
	DCInvalidateRange(buf, nbyte);
	if (gcg_ci_rdmode == 0) {
		ret = DVDReadAsyncPrio(&gcci->fi, buf, nbyte, ofst, gcci_rd_cbfn, 2);
	} else {
		ret = DVDReadPrio(&gcci->fi, buf, nbyte, ofst, 2);
	}
	if (ret == 0) {
		return 0;
	}
	gcci->stat = GCCI_STAT_READING;
	gcg_ci_debug.stat = GCCI_STAT_READING;
	return gcci->rqsct;
}

/* dead: the build string (referenced after gcg_ci_obj so that gcg_ci_build_ptr ends the .bss) */
const Char8 *gcCiGetVersion(void)
{
	gcg_ci_build_ptr = gcg_ci_build_str;
	return gcg_ci_build_str;
}

Sint32 gcCiTell(GCCI gcci)
{
	if (gcci == NULL) {
		gcci_CallErr("E0092912:handl is null.", NULL);
		return 0;
	}
	return gcci->pos_sct;
}

Sint32 gcCiSeek(GCCI gcci, Sint32 pos, Sint32 type)
{
	Sint32 max;

	if (gcci == NULL) {
		gcci_CallErr("E0092912:handl is null.", NULL);
		return 0;
	}
	if (type == CVFS_SEEK_SET) {
		gcci->pos_sct = pos;
	} else if (type == CVFS_SEEK_END) {
		gcci->pos_sct = gcci->fsize_sct + pos;
	} else if (type == CVFS_SEEK_CUR) {
		gcci->pos_sct += pos;
	}
	max = gcci->fsize_sct;
	if (gcci->pos_sct < max) {
		max = gcci->pos_sct;
	}
	gcci->pos_sct = max;
	gcci->pos_sct = (gcci->pos_sct > 0) ? gcci->pos_sct : 0;
	return gcci->pos_sct;
}

/* The CVFS interface hands the handle over as `void *`: the `void *` -> GCCI conversion keeps the copy
 * as its own node (CRI pass 12), and StopTr called with `hn` makes its inlined copy a second copy of r3
 * (`mr. r28, r3; mr r29, r3`) instead of a copy of gcci. */
void gcCiClose(void *hn)
{
	GCCI gcci;

	gcci = hn;
	if (gcci == NULL) {
		return;
	}
	gcCiStopTr(hn);
	DVDClose(&gcci->fi);
	gcci->used = 0;
	memset(gcci, 0, sizeof(GCCI_OBJ));
}

/* root directory + file name, '\' -> '/' */
static void gcci_MakePath(Char8 *path, const Char8 *fname)
{
	Uint32 len;
	Uint32 i;

	strcpy(path, gcg_ci_root_dir);
	strcat(path, fname);
	len = strlen(path);
	for (i = 0; i < len; i++) {
		if (path[i] == '\\') {
			path[i] = '/';
		}
	}
}

GCCI gcCiOpen(const Char8 *fname, void *dir, Sint32 rw)
{
	Char8 path[GCCI_PATH_LEN];
	GCCI gcci;
	Uint32 len;
	Sint32 n;

	if (fname == NULL) {
		gcci_CallErr("E0092908:fname is null.(gcCiOpen)", NULL);
		return NULL;
	}
	if (rw != 0) {
		gcci_CallErr("E0092909:rw is illigal.(gcCiOpen)", NULL);
		return NULL;
	}
	gcci = gcci_GetFreeHn();
	if (gcci == NULL) {
		gcci_CallErr("E0092910:not enough handle resource.(gcCiOpen)", NULL);
		return NULL;
	}
	gcci_MakePath(path, fname);
	if (DVDOpen(path, &gcci->fi) == 0) {
		gcci_CallErr("E0092911:DVDOpen fail.(gcCiOpen)", NULL);
		memset(gcci, 0, sizeof(GCCI_OBJ));
		return NULL;
	}
	gcci->sctlen = GCCI_DEF_SCTLEN;
	len = gcci->fi.length;
	if (len & 0x80000000) {
		len = 0x7FFFFFFF;
	}
	gcci->fsize_byte = len;
	n = gcci->sctlen;
	n += gcci->fsize_byte;
	gcci->fsize_sct = (n - 1) / gcci->sctlen;
	gcci->pos_sct = 0;
	gcci->buf = NULL;
	gcci->rqsct = 0;
	gcci->numtr = 0;
	gcci->stat = GCCI_STAT_STOP;
	gcci->used = 1;
	return gcci;
}

Sint32 gcCiGetFileSize(const Char8 *fname)
{
	Char8 path[GCCI_PATH_LEN];
	DVDFileInfo fi;
	Uint32 len;

	if (fname == NULL) {
		gcci_CallErr("E0092901:fname is null.(gcCiGetFileSize)", NULL);
		return 0;
	}
	gcci_MakePath(path, fname);
	if (DVDOpen(path, &fi) == 0) {
		gcci_CallErr("E0040201:can't open a file.(gcCiGetFileSize)", NULL);
		return 0;
	}
	len = fi.length;
	if (len & 0x80000000) {
		len = 0x7FFFFFFF;
	}
	if (DVDClose(&fi) == 0) {
		gcci_CallErr("E0040202:can't close a file.(gcCiGetFileSize)", NULL);
		return 0;
	}
	return len;
}

/* 24 zero bytes end the original .rodata after the gcCiGetFileSize strings: a dead zero table */
static const Sint32 gcg_ci_rsv[6] = {0, 0, 0, 0, 0, 0};

/* dead */
Sint32 gcCiGetRsv(Sint32 i)
{
	return gcg_ci_rsv[i];
}

void gcCiEntryErrFunc(CVFS_ERRFUNC func, void *obj)
{
	gcg_ci_err_func = func;
	gcg_ci_err_obj = obj;
}

void gcCiExecServer(void)
{
	Sint32 i;

	for (i = 0; i < GCCI_MAX_OBJ; i++) {
		gcci_ExecOne(&gcg_ci_obj[i]);
	}
}

CVFS_IF *gcCiGetInterface(void)
{
	gcg_ci_build_ptr = gcg_ci_build_ptr;
	memset(gcg_ci_root_dir, 0, sizeof(gcg_ci_root_dir));
	gcg_ci_err_func = NULL;
	gcg_ci_err_obj = NULL;
	memset(&gcg_ci_debug, 0, sizeof(gcg_ci_debug));
	return &gcg_ci_vtbl;
}
