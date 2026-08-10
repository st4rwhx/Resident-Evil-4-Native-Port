/* newlib 1.8.2 libc/stdio/vprintf.c */
#include "newlib_stdio.h"

int vprintf(const char *fmt, va_list ap)
{
    return vfprintf(_stdout_r(_REENT), fmt, ap);
}
