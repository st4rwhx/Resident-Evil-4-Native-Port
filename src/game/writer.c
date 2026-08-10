/* newlib 1.8.2 libc/reent/writer.c */
#include "newlib_stdio.h"

long _write_r(struct _reent *ptr, int fd, const void *buf, size_t cnt)
{
    long ret;

    errno = 0;
    if ((ret = write(fd, buf, cnt)) == -1 && errno != 0)
        ptr->_errno = errno;
    return ret;
}
