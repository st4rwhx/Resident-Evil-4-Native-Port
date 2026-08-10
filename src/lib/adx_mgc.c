#include "cri_xpt.h"
#include <dolphin/os.h>
#include <string.h>

extern void SVM_SetCbErr(void (*func)(void *obj, Char8 *msg), void *obj);
extern void SVM_ExecSvrMain(void);
extern void ADXERR_CallErrFunc1(const Char8 *msg);

/* The thread manager (ADXM_SetupThrd/ShutdownThrd/Lock/Unlock/GotoMwIdleBorder ...) was
 * dead-stripped by the linker; the bodies below only reproduce the .bss order of its statics. */
void (*adxgc_exec_svr)(void);
static Sint32 adxm_init_level;
static Sint32 adxm_lock_level;
static Sint32 adxm_goto_border_flag;
static Sint32 adxm_safe_cnt;
static Sint32 adxm_vsync_cnt;
static Sint32 adxm_fs_cnt;
static Sint32 adxm_mwidle_cnt;
static Sint32 adxm_mwidle_exec_flag;
static OSThread *adxm_main_thread;
OSThread adxm_mwidle_thread;
OSThread adxm_vsync_thread;
static OSThread adxm_fs_thread;
OSThread adxm_safe_thread;
static Sint32 adxm_cur_prio;
static Sint32 adxm_set_prio;
Sint32 adxm_fs_end;
static Sint32 adxm_fs_act;
static Sint32 adxm_vsync_end;
static Sint32 adxm_vsync_act;
static Sint32 adxm_mwidle_end;
Sint32 adxm_mwidle_act;
Sint32 adxm_safe_end;
Sint32 adxm_safe_act;
static Uint8 adxm_stack_mwidle[0x2000];
Uint8 adxm_stack_fs[0x2000];
Uint8 adxm_stack_vsync[0x2000];
static Uint8 adxm_stack_safe[0x1000];

Sint32 adxm_save_tprm[6] = {0};

/* dead: keeps the version string in .rodata */
const Char8 *ADXM_GetVersion(void)
{
	return "\nADXGC Ver.1.21 Build:Oct  8 2004 13:33:20\n";
}

void ADXM_SetupThrd(Sint32 *tprm)
{
	Sint32 i;

	adxgc_exec_svr = NULL;
	if (adxm_init_level != 0) {
		return;
	}
	adxm_lock_level = 0;
	adxm_goto_border_flag = 0;
	adxm_safe_cnt = 0;
	adxm_vsync_cnt = 0;
	adxm_fs_cnt = 0;
	adxm_mwidle_cnt = 0;
	adxm_mwidle_exec_flag = 0;
	adxm_main_thread = OSGetCurrentThread();
	memset(&adxm_mwidle_thread, 0, sizeof(OSThread));
	memset(&adxm_vsync_thread, 0, sizeof(OSThread));
	memset(&adxm_fs_thread, 0, sizeof(OSThread));
	memset(&adxm_safe_thread, 0, sizeof(OSThread));
	adxm_cur_prio = OSGetThreadPriority(adxm_main_thread);
	adxm_set_prio = adxm_cur_prio;
	adxm_fs_end = 0;
	adxm_fs_act = 0;
	adxm_vsync_end = 0;
	adxm_vsync_act = 0;
	adxm_mwidle_end = 0;
	adxm_mwidle_act = 0;
	adxm_safe_end = 0;
	adxm_safe_act = 0;
	for (i = 0; i < 6; i++) {
		adxm_save_tprm[i] = tprm[i];
	}
	OSCreateThread(&adxm_mwidle_thread, NULL, NULL, adxm_stack_mwidle + 0x2000, 0x2000, 16, 1);
	OSCreateThread(&adxm_fs_thread, NULL, NULL, adxm_stack_fs + 0x2000, 0x2000, 16, 1);
	OSCreateThread(&adxm_vsync_thread, NULL, NULL, adxm_stack_vsync + 0x2000, 0x2000, 16, 1);
	OSCreateThread(&adxm_safe_thread, NULL, NULL, adxm_stack_safe + 0x1000, 0x1000, 16, 1);
	adxm_init_level = 1;
}

void ADXM_ShutdownThrd(void)
{
	if (adxm_init_level == 0) {
		return;
	}
	adxm_fs_end = 1;
	adxm_vsync_end = 1;
	adxm_mwidle_end = 1;
	adxm_safe_end = 1;
	OSJoinThread(&adxm_mwidle_thread, NULL);
	OSJoinThread(&adxm_vsync_thread, NULL);
	OSJoinThread(&adxm_fs_thread, NULL);
	OSJoinThread(&adxm_safe_thread, NULL);
	adxm_init_level = 0;
}

void ADXM_Lock(void)
{
	if (adxm_lock_level == 0) {
		adxm_set_prio = OSGetThreadPriority(adxm_main_thread);
		OSSetThreadPriority(adxm_main_thread, 0);
	}
	adxm_lock_level++;
}

void ADXM_Unlock(void)
{
	adxm_lock_level--;
	if (adxm_lock_level == 0) {
		OSSetThreadPriority(adxm_main_thread, adxm_set_prio);
	}
}

/* a dead function used a 0.0f constant: the 4-byte pool word between the two strings */
Sint32 ADXM_CalcVsyncCnt(Float32 sec)
{
	if (sec <= 0.0f) {
		return 0;
	}
	return adxm_vsync_cnt;
}

void ADXM_GotoMwIdleBorder(void)
{
	if (adxm_init_level == 0) {
		ADXERR_CallErrFunc1("1060102: Internal Error: adxm_goto_mwidle_border");
		return;
	}
	adxm_goto_border_flag = 1;
	while (adxm_mwidle_exec_flag != 0) {
		OSYieldThread();
	}
	adxm_goto_border_flag = 0;
}

Sint32 ADXM_GetCnt(Sint32 id)
{
	switch (id) {
	case 0:
		return adxm_safe_cnt;
	case 1:
		return adxm_vsync_cnt;
	case 2:
		return adxm_fs_cnt;
	default:
		return adxm_mwidle_cnt;
	}
}

Sint32 ADXM_IsSetupThrd(void)
{
	return adxm_init_level != 0;
}

void ADXM_SetCbErr(void (*func)(void *obj, Char8 *msg), void *obj)
{
	SVM_SetCbErr(func, obj);
}

void ADXM_ExecMain(void)
{
	SVM_ExecSvrMain();
}

void ADXM_WaitVsync(void)
{
	VIWaitForRetrace();
}
