/* newlib 1.8.2 libc/reent/closer.c */
#include "newlib_stdio.h"

int _close_r(struct _reent *ptr, int fd)
{
    int ret;

    errno = 0;
    if ((ret = close(fd)) == -1 && errno != 0)
        ptr->_errno = errno;
    return ret;
}
