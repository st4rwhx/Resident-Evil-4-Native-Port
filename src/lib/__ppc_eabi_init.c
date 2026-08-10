/* Metrowerks PowerPC EABI runtime (__ppc_eabi_init.c) as linked into the ProDG build: the static
 * constructor walk runs over the game's own _ctors_data table (see tors.c) and abort() halts the CPU. */
#include <dolphin/base/PPCArch.h>

typedef void (*voidfunctionptr)(void);

extern voidfunctionptr _ctors_data[];

static void __init_cpp(void);
void _ExitProcess(void);

__declspec(weak) extern void __init_user(void)
{
    __init_cpp();
}

static void __init_cpp(void)
{
    voidfunctionptr *constructor;

    for (constructor = _ctors_data; *constructor; constructor++) {
        (*constructor)();
    }
}

__declspec(weak) extern void abort(void)
{
    _ExitProcess();
}

__declspec(weak) extern void _ExitProcess(void)
{
    PPCHalt();
}
