/* CRI CVFS memory-file device (MFCI): "files" are memory ranges named "%08x.%08x" (address.size). */
#include "cvfs.h"
#include <string.h>
#include <stdio.h>

unsigned long strtoul(const char *s, char **end, int base);

#define MFCI_MAX_OBJ 40
#define MFCI_DEF_SCTLEN 0x800
#define MFCI_FNAME_LEN 17

/* MFCI_OBJ.stat */
#define MFCI_STAT_STOP 0
#define MFCI_STAT_COMPLETE 1
#define MFCI_STAT_READING 2

typedef struct {
	Sint8 used;                /* 0x00 */
	Sint8 stat;                /* 0x01 */
	Uint8 pad02[2];
	Sint32 sctlen;             /* 0x04 */
	Sint32 fsize_byte;         /* 0x08 */
	Sint32 fsize_sct;          /* 0x0C */
	Sint32 pos_sct;            /* 0x10 */
	Sint32 numtr;              /* 0x14 bytes transferred by the last request */
	Sint32 rqsct;              /* 0x18 sectors of the last request */
	Char8 fname[20];           /* 0x1C */
	Sint32 rd_ofst;            /* 0x30 */
	Sint32 rd_nbyte;           /* 0x34 */
} MFCI_OBJ;

typedef MFCI_OBJ *MFCI;

/* volatile: the build string must stay referenced (dead `lwz` in mfCiGetInterface) */
const Char8 *const volatile mfci_build = "\nMFCI/GC Ver.1.09 Build:Oct  8 2004 13:32:28\n";

static CVFS_ERRFUNC mfci_err_func;
static void *mfci_err_obj;
Char8 mfci_err_str[300];
MFCI_OBJ mfci_obj[MFCI_MAX_OBJ];
static Sint32 mfci_init_flag;

void mfCiExecServer(void);
void mfCiEntryErrFunc(CVFS_ERRFUNC func, void *obj);
Sint32 mfCiGetFileSize(const Char8 *fname);
MFCI mfCiOpen(const Char8 *fname, void *dir, Sint32 rw);
void mfCiClose(MFCI mfci);
Sint32 mfCiSeek(MFCI mfci, Sint32 pos, Sint32 type);
Sint32 mfCiTell(MFCI mfci);
Sint32 mfCiReqRd(void *hn, Sint32 nsct, Uint8 *buf);
void mfCiStopTr(MFCI mfci);
Sint32 mfCiGetStat(MFCI mfci);
Sint32 mfCiGetSctLen(MFCI mfci);
void mfCiSetSctLen(MFCI mfci, Sint32 sctlen);
Sint32 mfCiGetNumTr(MFCI mfci);

CVFS_IF mfci_vtbl = {
	mfCiExecServer,
	mfCiEntryErrFunc,
	mfCiGetFileSize,
	NULL,
	(void *(*)(const Char8 *, void *, Sint32))mfCiOpen,
	(void (*)(void *))mfCiClose,
	(Sint32 (*)(void *, Sint32, Sint32))mfCiSeek,
	(Sint32 (*)(void *))mfCiTell,
	(Sint32 (*)(void *, Sint32, void *))mfCiReqRd,
	NULL,
	(void (*)(void *))mfCiStopTr,
	(Sint32 (*)(void *))mfCiGetStat,
	(Sint32 (*)(void *))mfCiGetSctLen,
	(void (*)(void *, Sint32))mfCiSetSctLen,
	(Sint32 (*)(void *))mfCiGetNumTr,
};

static void mfci_CallErr(const Char8 *msg, void *hn)
{
	if (mfci_err_func != NULL) {
		mfci_err_func(mfci_err_obj, msg, hn);
	}
}

/* dead-stripped: keeps the .rodata strings and the init flag */
MFCI mfCiOpenEntry(Sint32 entry, Sint32 rw)
{
	MFCI mfci;

	if (entry < 0 || entry >= MFCI_MAX_OBJ) {
		sprintf(mfci_err_str, "E1041001:invalid entry number.(mfCiOpenEntry)");
		mfci_CallErr(mfci_err_str, NULL);
		return NULL;
	}
	if (rw != 0) {
		sprintf(mfci_err_str, "E1041002:rw is illigal.(mfCiOpenEntry)");
		mfci_CallErr(mfci_err_str, NULL);
		return NULL;
	}
	mfci = &mfci_obj[entry];
	if (mfci->used != 0) {
		sprintf(mfci_err_str, "E1041002:not enough handle resource.(mfCiOpenEntry)");
		mfci_CallErr(mfci_err_str, NULL);
		return NULL;
	}
	mfci_init_flag = 1;
	mfci->used = 1;
	return mfci;
}

static Sint32 mfci_ByteToSct(Sint32 sctlen, Sint32 nbyte)
{
	Sint32 n;
	n = sctlen;
	n += nbyte;
	return (n - 1) / sctlen;
}

static MFCI mfci_GetFreeHn(void)
{
	MFCI mfci = NULL;
	Sint32 i;

	for (i = 0; i < MFCI_MAX_OBJ; i++) {
		if (mfci_obj[i].used == 0) {
			mfci = &mfci_obj[i];
			break;
		}
	}
	return mfci;
}

/* "%08x.%08x": memory address and size of the file */
static Uint32 mfci_get_adr_size(const Char8 *fname, Uint32 *size)
{
	Char8 *p;
	Uint32 adr;

	if (strlen(fname) != MFCI_FNAME_LEN) {
		sprintf(mfci_err_str, "E01100308:length of '%s' is not 17 bytes.(mfci_get_adr_size)", fname);
		mfci_CallErr(mfci_err_str, NULL);
	}
	if (fname[8] != '.') {
		sprintf(mfci_err_str, "E01100309:illegal file name format '%s'(mfci_get_adr_size)", fname);
		mfci_CallErr(mfci_err_str, NULL);
	}
	p = (Char8 *)fname;
	adr = strtoul(p, &p, 16);
	if (*p != '\0') {
		p++;
	}
	if (size != NULL) {
		*size = strtoul(p, &p, 16);
	}
	return adr;
}

Sint32 mfCiGetNumTr(MFCI mfci)
{
	if (mfci == NULL) {
		mfci_CallErr("E0092912:handl is null.", NULL);
		return 0;
	}
	return mfci->numtr;
}

void mfCiSetSctLen(MFCI mfci, Sint32 sctlen)
{
	Sint32 posbyte;

	if (mfci == NULL) {
		mfci_CallErr("E0040302:handl is null.", NULL);
		return;
	}
	posbyte = mfci->pos_sct * mfci->sctlen;
	mfci->sctlen = sctlen;
	mfci->fsize_sct = mfci_ByteToSct(mfci->sctlen, mfci->fsize_byte);
	mfci->pos_sct = posbyte / mfci->sctlen;
	mfci->numtr = mfci->rqsct * sctlen;
}

Sint32 mfCiGetSctLen(MFCI mfci)
{
	if (mfci == NULL) {
		mfci_CallErr("E0040301:handl is null.", NULL);
		return 0;
	}
	return mfci->sctlen;
}

Sint32 mfCiGetStat(MFCI mfci)
{
	if (mfci == NULL) {
		mfci_CallErr("E0092912:handl is null.", NULL);
		return 0;
	}
	return mfci->stat;
}

void mfCiStopTr(MFCI mfci)
{
	if (mfci == NULL) {
		mfci_CallErr("E0092912:handl is null.", NULL);
		return;
	}
	SVM_Lock();
	mfci->stat = MFCI_STAT_STOP;
	SVM_Unlock();
}

Sint32 mfCiReqRd(void *hn, Sint32 nsct, Uint8 *buf)
{
	Uint32 adr;
	Uint32 size;
	Sint32 cpy;
	Sint32 rem;
	Sint32 rd_ofst;
	Sint32 rd_nbyte;
	/* CVFS_IF slot: the `void *` handle's typed copy is kept (it is passed to mfci_CallErr in r5) and
	 * ranks mfci r29 above buf r28 / nsct r27; the copy is the prologue `mr. r29, r3`. */
	MFCI p = hn;

	if (p == NULL) {
		mfci_CallErr("E01100307:handl is null.", NULL);
		return 0;
	}
	if (nsct < 0) {
		mfci_CallErr("E01100308:nsct < 0.(mfCiReqRd)", p);
		return 0;
	}
	if (buf == NULL) {
		mfci_CallErr("E01100309:buf is null.(mfCiReqRd)", p);
		return 0;
	}
	if (nsct == 0) {
		p->stat = MFCI_STAT_COMPLETE;
		return 0;
	}
	if (p->stat == MFCI_STAT_READING) {
		return 0;
	}
	SVM_Lock();
	p->numtr = 0;
	rem = p->fsize_sct - p->pos_sct;
	if (nsct < rem) {
		rem = nsct;
	}
	p->rqsct = rem;
	rd_ofst = p->pos_sct * p->sctlen;
	rd_nbyte = p->rqsct * p->sctlen;
	if (rd_nbyte == 0) {
		p->stat = MFCI_STAT_COMPLETE;
		SVM_Unlock();
		return 0;
	}
	p->rd_ofst = rd_ofst;
	p->rd_nbyte = rd_nbyte;
	p->stat = MFCI_STAT_READING;
	adr = mfci_get_adr_size(p->fname, &size);
	cpy = p->rd_nbyte;
	if (cpy > (Sint32)(size - p->rd_ofst)) {
		cpy = size - p->rd_ofst;
	}
	SVM_Unlock();
	memcpy(buf, (void *)(adr + p->rd_ofst), p->rd_nbyte);
	memset(buf + cpy, 0, p->rd_nbyte - cpy);
	SVM_Lock();
	p->numtr = p->rqsct * p->sctlen;
	p->pos_sct += p->rqsct;
	p->stat = MFCI_STAT_COMPLETE;
	SVM_Unlock();
	return p->rqsct;
}

Sint32 mfCiTell(MFCI mfci)
{
	if (mfci == NULL) {
		mfci_CallErr("E01100306:handl is null.", NULL);
		return 0;
	}
	return mfci->pos_sct;
}

Sint32 mfCiSeek(MFCI mfci, Sint32 pos, Sint32 type)
{
	if (mfci == NULL) {
		mfci_CallErr("E01100305:handl is null.", NULL);
		return 0;
	}
	SVM_Lock();
	if (type == CVFS_SEEK_SET) {
		mfci->pos_sct = pos;
	} else if (type == CVFS_SEEK_END) {
		mfci->pos_sct = mfci->fsize_sct + pos;
	} else if (type == CVFS_SEEK_CUR) {
		mfci->pos_sct += pos;
	}
	mfci->pos_sct = (mfci->pos_sct < mfci->fsize_sct) ? mfci->pos_sct : mfci->fsize_sct;
	mfci->pos_sct = (mfci->pos_sct > 0) ? mfci->pos_sct : 0;
	SVM_Unlock();
	return mfci->pos_sct;
}

void mfCiClose(MFCI mfci)
{
	if (mfci == NULL) {
		return;
	}
	mfCiStopTr(mfci);
	if (mfci->used == 1) {
		mfci->used = 0;
		memset(mfci, 0, sizeof(MFCI_OBJ));
	}
}

MFCI mfCiOpen(const Char8 *fname, void *dir, Sint32 rw)
{
	MFCI mfci;
	Uint32 size;

	if (fname == NULL) {
		mfci_CallErr("E01100301:fname is null.(mfCiOpen)", NULL);
		return NULL;
	}
	if (rw != 0) {
		mfci_CallErr("E01100302:rw is illigal.(mfCiOpen)", NULL);
		return NULL;
	}
	mfci = mfci_GetFreeHn();
	if (mfci == NULL) {
		mfci_CallErr("E01100303:not enough handle resource.(mfCiOpen)", NULL);
		return NULL;
	}
	strcpy(mfci->fname, fname);
	mfci->sctlen = MFCI_DEF_SCTLEN;
	mfci_get_adr_size(mfci->fname, &size);
	mfci->fsize_byte = size;
	mfci->fsize_sct = mfci_ByteToSct(mfci->sctlen, mfci->fsize_byte);
	mfci->pos_sct = 0;
	mfci->rqsct = 0;
	mfci->numtr = 0;
	mfci->stat = MFCI_STAT_STOP;
	mfci->used = 1;
	return mfci;
}

Sint32 mfCiGetFileSize(const Char8 *fname)
{
	Uint32 size;

	mfci_get_adr_size(fname, &size);
	return size;
}

void mfCiEntryErrFunc(CVFS_ERRFUNC func, void *obj)
{
	mfci_err_func = func;
	mfci_err_obj = obj;
}

void mfCiExecServer(void)
{
	Sint32 i;

	for (i = 0; i < MFCI_MAX_OBJ; i++) {
	}
}

CVFS_IF *mfCiGetInterface(void)
{
	mfci_build;
	return &mfci_vtbl;
}
