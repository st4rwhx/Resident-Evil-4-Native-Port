/* newlib 1.8.2 libc/reent/readr.c */
#include "newlib_stdio.h"

long _read_r(struct _reent *ptr, int fd, void *buf, size_t cnt)
{
    long ret;

    errno = 0;
    if ((ret = read(fd, buf, cnt)) == -1 && errno != 0)
        ptr->_errno = errno;
    return ret;
}
