/* newlib 1.8.2 libc/stdio/vsprintf.c */
#include "newlib_stdio.h"

int vsprintf(char *str, const char *fmt, va_list ap)
{
    int ret;
    FILE f;

    f._flags = __SWR | __SSTR;
    f._bf._base = f._p = (unsigned char *)str;
    f._bf._size = f._w = INT_MAX;
    f._data = _REENT;
    ret = vfprintf(&f, fmt, ap);
    *f._p = 0;
    return ret;
}
