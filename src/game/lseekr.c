/* newlib 1.8.2 libc/reent/lseekr.c */
#include "newlib_stdio.h"

off_t _lseek_r(struct _reent *ptr, int fd, off_t pos, int whence)
{
    off_t ret;

    errno = 0;
    if ((ret = lseek(fd, pos, whence)) == (off_t)-1 && errno != 0)
        ptr->_errno = errno;
    return ret;
}
