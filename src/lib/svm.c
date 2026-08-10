/* CRI ADX server manager (SVM): server function tables, lock/unlock and error callbacks */
#include "cri_xpt.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define SVM_MAX_SVTYPE 8
#define SVM_MAX_SVR_FUNC 6
#define SVM_SVTYPE_MAIN 5
#define SVM_ERR_MSG_LEN 128

typedef struct {
	Sint32 (*func)(void *obj);
	void *obj;
} SvmSvrFunc;

typedef struct {
	void (*func)(void *obj);
	void *obj;
} SvmVoidFunc;

typedef struct {
	void (*func)(void *obj, Char8 *msg);
	void *obj;
} SvmErrFunc;

void SVM_CallErr(Char8 *fmt, ...);

/* version string, kept by its pointer in .rodata */
const Char8 *const svm_build = "\nSVM/GC Ver.1.54 Build:Oct  8 2004 13:32:08\n";

volatile Sint32 svm_init_level = 0;
volatile Sint32 svm_lock_level = 0;
volatile Sint32 svm_locking_type = 0;

SvmVoidFunc svm_post_waitv_func;
SvmVoidFunc svm_pre_waitv_func;
static Sint32 (*svm_tas_fptr)(Sint32 *flag);
Char8 svmerr_msg[SVM_ERR_MSG_LEN];
static SvmErrFunc svm_err_func;
SvmVoidFunc svm_unlock_func;
static SvmVoidFunc svm_lock_func;
SvmVoidFunc svm_goto_border_func[SVM_MAX_SVTYPE];
SvmSvrFunc svm_svr_ftbl[SVM_MAX_SVTYPE][SVM_MAX_SVR_FUNC];
static Sint32 svm_svr_exec_flag[SVM_MAX_SVTYPE];
Sint32 svm_exec_cnt[SVM_MAX_SVTYPE];

/* dead (linker-stripped) helpers: they fix the .bss order (first reference) and keep their statics */
static void svm_itoa(Sint32 val, Char8 *str, Sint32 len)
{
	static Char8 buf[32];
	Sint32 i;
	Sint32 n;
	Sint32 l;

	for (i = 0; i < 32; i++) {
		buf[i] = val % 10;
		val /= 10;
		if (val == 0) {
			buf[i] = '\0';
			break;
		}
	}
	l = strlen(buf);
	n = len - 1;
	if (l < n) {
		n = l;
	}
	for (i = 0; i < n; i++) {
		str[i] = buf[n - 1 - i];
	}
	str[i] = '\0';
}

void SVM_SetCbWaitVsync(void (*pre)(void *obj), void *pre_obj, void (*post)(void *obj), void *post_obj)
{
	svm_post_waitv_func.func = post;
	svm_post_waitv_func.obj = post_obj;
	svm_pre_waitv_func.func = pre;
	svm_pre_waitv_func.obj = pre_obj;
}

void SVM_SetCbTestAndSet(Sint32 (*func)(Sint32 *flag))
{
	svm_tas_fptr = func;
}

static void svm_call_err1(Char8 *msg)
{
	strncpy(svmerr_msg, msg, SVM_ERR_MSG_LEN - 1);
	if (svm_err_func.func != NULL) {
		svm_err_func.func(svm_err_func.obj, svmerr_msg);
	}
}

static void svm_unlock(void)
{
	if (svm_unlock_func.func != NULL) {
		svm_lock_level--;
		if (svm_lock_level == 0) {
			if (svm_locking_type != 1) {
				SVM_CallErr("2103102:SVM:svm_unlock:lock type miss match.(type org=%d, type now=%d)",
				            svm_locking_type, 1);
			}
			svm_locking_type = 0;
		}
		svm_unlock_func.func(svm_unlock_func.obj);
	}
}

static void svm_lock(void)
{
	if (svm_lock_func.func != NULL) {
		svm_lock_func.func(svm_lock_func.obj);
		if (svm_lock_level == 0) {
			svm_locking_type = 1;
		}
		svm_lock_level++;
	}
}

void SVM_SetCbLock(void (*lock)(void *obj), void *lock_obj, void (*unlock)(void *obj), void *unlock_obj)
{
	svm_lock_func.func = lock;
	svm_lock_func.obj = lock_obj;
	svm_unlock_func.func = unlock;
	svm_unlock_func.obj = unlock_obj;
}

void SVM_SetCbGotoSvrBorder(Sint32 svtype, void (*func)(void *obj), void *obj)
{
	svm_goto_border_func[svtype].func = func;
	svm_goto_border_func[svtype].obj = obj;
}

Sint32 SVM_GetNumCbSvr(Sint32 svtype)
{
	Sint32 i;
	Sint32 n = 0;

	for (i = 0; i < SVM_MAX_SVR_FUNC; i++) {
		if (svm_svr_ftbl[svtype][i].func != NULL) {
			n++;
		}
	}
	return n;
}

Sint32 SVM_TestAndSet(Sint32 *flag)
{
	Sint32 ret;

	if (svm_tas_fptr != NULL) {
		ret = svm_tas_fptr(flag);
	} else {
		Sint32 old;

		svm_lock();
		old = *flag;
		*flag = 1;
		ret = (old != 1);
		svm_unlock();
	}
	return ret;
}

static void svm_clear(void)
{
	Sint32 i;
	Sint32 *p;

	memset(svm_svr_exec_flag, 0, sizeof(svm_svr_exec_flag));
	memset(&svm_lock_func, 0, sizeof(svm_lock_func));
	memset(&svm_unlock_func, 0, sizeof(svm_unlock_func));
	memset(&svm_pre_waitv_func, 0, sizeof(svm_pre_waitv_func));
	memset(&svm_post_waitv_func, 0, sizeof(svm_post_waitv_func));
	p = svm_exec_cnt;
	for (i = 0; i < SVM_MAX_SVR_FUNC; i++) {
		*p++ = 0;
	}
	svm_tas_fptr = NULL;
}

void SVM_Finish(void)
{
	svm_init_level--;
	if (svm_init_level == 0) {
		svm_clear();
		memset(&svm_err_func, 0, sizeof(svm_err_func));
	}
}

void SVM_Init(void)
{
	if (svm_init_level == 0) {
		svm_clear();
	}
	svm_init_level++;
}

static Sint32 svm_exec_svr(Sint32 svtype)
{
	SvmSvrFunc *p;
	Sint32 i;
	Sint32 ret;

	ret = 0;
	p = svm_svr_ftbl[svtype];
	for (i = 0; i < SVM_MAX_SVR_FUNC; i++) {
		if (p->func != NULL) {
			svm_svr_exec_flag[svtype] = 1;
			ret |= p->func(p->obj);
			svm_svr_exec_flag[svtype] = 0;
		}
		p++;
	}
	svm_exec_cnt[svtype]++;
	return ret;
}

Sint32 SVM_ExecSvrMain(void)
{
	return svm_exec_svr(SVM_SVTYPE_MAIN);
}

void SVM_SetCbErr(void (*func)(void *obj, Char8 *msg), void *obj)
{
	svm_lock();
	svm_err_func.func = func;
	svm_err_func.obj = obj;
	svm_unlock();
}

void SVM_GotoSvrBorder(Sint32 svtype)
{
	if (svm_goto_border_func[svtype].func != NULL) {
		svm_goto_border_func[svtype].func(svm_goto_border_func[svtype].obj);
	}
}

Sint32 SVM_ExecSvrFuncId(Sint32 svtype, Sint32 id)
{
	Sint32 ret = 0;
	SvmSvrFunc *p;

	if (id < 0 || id >= SVM_MAX_SVR_FUNC) {
		svm_call_err1("1071301:SVM_ExecSvrFuncId:illegal id");
		return 0;
	}
	if (svtype < 0 || svtype >= SVM_MAX_SVTYPE) {
		svm_call_err1("1071302:SVM_ExecSvrFuncId:illegal svtype");
		return 0;
	}
	p = &svm_svr_ftbl[svtype][id];
	if (p->func != NULL) {
		svm_svr_exec_flag[svtype] = 1;
		ret = p->func(p->obj);
		svm_svr_exec_flag[svtype] = 0;
	}
	svm_exec_cnt[svtype]++;
	return ret;
}

void SVM_SetCbSvrId(Sint32 svtype, Sint32 id, Sint32 (*func)(void *obj), void *obj)
{
	SvmSvrFunc *p;

	if (id < 0 || id >= SVM_MAX_SVR_FUNC) {
		svm_call_err1("1071201:SVM_SetCbSvrId:illegal id");
	}
	if (svtype < 0 || id >= SVM_MAX_SVTYPE) {
		svm_call_err1("1071202:SVM_SetCbSvrId:illegal svtype");
	}
	svm_lock();
	p = &svm_svr_ftbl[svtype][id];
	if (p->func != NULL) {
		svm_call_err1("2100801:SVM_SetCbSvrId:over write callback function.");
	}
	p->func = func;
	p->obj = obj;
	svm_unlock();
}

void SVM_DelCbSvr(Sint32 svtype, Sint32 id)
{
	if (id < 0 || id >= SVM_MAX_SVR_FUNC) {
		svm_call_err1("1051002:SVM_DelCbSvr:illegal id");
	}
	svm_lock();
	svm_svr_ftbl[svtype][id].func = NULL;
	svm_svr_ftbl[svtype][id].obj = NULL;
	svm_unlock();
}

Sint32 SVM_SetCbSvr(Sint32 svtype, Sint32 (*func)(void *obj), void *obj)
{
	Sint32 i;
	SvmSvrFunc *p;

	svm_lock();
	p = svm_svr_ftbl[svtype];
	for (i = 0; i < SVM_MAX_SVR_FUNC; i++) {
		if (p->func == NULL) {
			p->func = func;
			p->obj = obj;
			break;
		}
		p++;
	}
	if (i == SVM_MAX_SVR_FUNC) {
		svm_call_err1("1051001:SVM_SetCbSvr:too many server function");
	}
	svm_unlock();
	if (i == SVM_MAX_SVR_FUNC) {
		return -1;
	}
	return i;
}

void SVM_CallErr1(Char8 *msg)
{
	svm_call_err1(msg);
}

void SVM_CallErr(Char8 *fmt, ...)
{
	va_list ap;

	memset(svmerr_msg, 0, sizeof(svmerr_msg));
	va_start(ap, fmt);
	vsprintf(svmerr_msg, fmt, ap);
	va_end(ap);
	if (svm_err_func.func != NULL) {
		svm_err_func.func(svm_err_func.obj, svmerr_msg);
	}
}

void SVM_ItoA2(Sint32 a, Sint32 b, Char8 *str, Sint32 len)
{
	svm_itoa(a, str, len);
	strncat(str, " ", len - strlen(str) - 1);
	svm_itoa(b, str + strlen(str), len - strlen(str));
}

void SVM_Unlock(void)
{
	svm_unlock();
}

void SVM_Lock(void)
{
	svm_lock();
}
