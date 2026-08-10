/* Sofdec MW player: server manager glue */
#include "cri_xpt.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern void SVM_Init(void);
extern void SVM_GotoSvrBorder(Sint32 kind);
extern Sint32 SVM_TestAndSet(Sint32 *flag);
extern Sint32 SVM_SetCbSvr(Sint32 kind, Sint32 (*func)(void *obj), void *obj);
extern void SVM_SetCbSvrId(Sint32 kind, Sint32 id, Sint32 (*func)(void *obj), void *obj);
extern void SVM_CallErr1(Char8 *msg);

Sint32 mwg_main_fid;
Sint32 mwg_idle_fid;
Sint32 mwg_vsync_fid;
Sint32 mwg_vbin_fid;

void MWSFSVM_GotoIdleBorder(void)
{
	SVM_GotoSvrBorder(6);
}

void MWSFSVM_Error(const Char8 *fmt, ...)
{
	static Char8 errstr[256];
	va_list ap;

	memset(errstr, 0, sizeof(errstr));
	va_start(ap, fmt);
	vsprintf(errstr, fmt, ap);
	va_end(ap);
	SVM_CallErr1(errstr);
}

Sint32 MWSFSVM_TestAndSet(Sint32 *flag)
{
	return SVM_TestAndSet(flag);
}

void MWSFSVM_EntryMainFunc(Sint32 (*func)(void *obj), void *obj)
{
	mwg_main_fid = SVM_SetCbSvr(5, func, obj);
}

void MWSFSVM_EntryIdleFunc(Sint32 (*func)(void *obj), void *obj)
{
	mwg_idle_fid = SVM_SetCbSvr(6, func, obj);
}

void MWSFSVM_EntryIdVfunc(Sint32 id, Sint32 (*func)(void *obj), void *obj)
{
	SVM_SetCbSvrId(2, id, func, obj);
	mwg_vsync_fid = id;
}

void MWSFSVM_Init(void)
{
	SVM_Init();
	mwg_vbin_fid = 0;
	mwg_vsync_fid = 0;
	mwg_idle_fid = 0;
	mwg_main_fid = 0;
}
