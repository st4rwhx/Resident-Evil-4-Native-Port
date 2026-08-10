/* newlib 1.8.2 libc/stdio/fread.c, SN version: byte copy instead of memcpy */
#include "newlib_stdio.h"

void memcpyalpha(char *dst, const char *src, int n)
{
    while (n--)
        *dst++ = *src++;
}

size_t fread(void *buf, size_t size, size_t count, FILE *fp)
{
    register size_t resid;
    register char *p;
    register int r;
    size_t total;

    if ((resid = count * size) == 0)
        return 0;
    if (fp->_r < 0)
        fp->_r = 0;
    total = resid;
    p = buf;
    while (resid > (r = fp->_r)) {
        memcpyalpha(p, (char *)fp->_p, r);
        fp->_p += r;
        /* fp->_r = 0 ... done in __srefill */
        p += r;
        resid -= r;
        if (__srefill(fp)) {
            /* no more input: return partial result */
            return (total - resid) / size;
        }
    }
    memcpyalpha(p, (char *)fp->_p, resid);
    fp->_r -= resid;
    fp->_p += resid;
    return count;
}
