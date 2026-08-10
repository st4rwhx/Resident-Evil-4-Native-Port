/* newlib 1.8.2 libc/reent/fstatr.c */
#include "newlib_stdio.h"

int _fstat_r(struct _reent *ptr, int fd, struct stat *pstat)
{
    int ret;

    errno = 0;
    if ((ret = fstat(fd, pstat)) == -1 && errno != 0)
        ptr->_errno = errno;
    return ret;
}
