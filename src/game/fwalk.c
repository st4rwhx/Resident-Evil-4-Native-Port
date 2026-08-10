/* newlib 1.8.2 libc/stdio/fwalk.c */
#include "newlib_stdio.h"

int _fwalk(struct _reent *ptr, register int (*function)())
{
    register FILE *fp;
    register int n, ret = 0;
    register struct _glue *g;

    for (g = &ptr->__sglue; g != NULL; g = g->_next)
        for (fp = g->_iobs, n = g->_niobs; --n >= 0; fp++)
            if (fp->_flags != 0)
                ret |= (*function)(fp);
    return ret;
}
