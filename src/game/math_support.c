/* SN ProDG libc: floor/fmod/log/log10 for vfprintf's floating-point conversion, built from the
 * fdlibm sources (newlib 1.8.2 libm) under sn_ names so that printf does not pull in libm. */
#define floor sn_floor
#define __ieee754_fmod sn_fmod
#define __ieee754_log sn_log
#define __ieee754_log10 sn_log10

#include "lib/fdlibm/s_floor.c"
#include "lib/fdlibm/e_fmod.c"
#include "lib/fdlibm/e_log.c"
/* e_log10.c reuses two of e_log.c's constant names */
#define two54 log10_two54
#define zero log10_zero
#include "lib/fdlibm/e_log10.c"
