/* newlib 1.8.2 libc/stdio/stdio.c */
#include "newlib_stdio.h"

int __sread(void *cookie, char *buf, int n)
{
    register FILE *fp = (FILE *)cookie;
    register int ret;

    ret = _read_r(fp->_data, fp->_file, buf, n);

    /* If the read succeeded, update the current offset.  */

    if (ret >= 0)
        fp->_offset += ret;
    else
        fp->_flags &= ~__SOFF; /* paranoia */
    return ret;
}

int __swrite(void *cookie, char const *buf, int n)
{
    register FILE *fp = (FILE *)cookie;

    if (fp->_flags & __SAPP)
        (void)_lseek_r(fp->_data, fp->_file, (off_t)0, SEEK_END);
    fp->_flags &= ~__SOFF; /* in case O_APPEND mode is set */
    return _write_r(fp->_data, fp->_file, buf, n);
}

fpos_t __sseek(void *cookie, fpos_t offset, int whence)
{
    register FILE *fp = (FILE *)cookie;
    register off_t ret;

    ret = _lseek_r(fp->_data, fp->_file, (off_t)offset, whence);
    if (ret == -1L)
        fp->_flags &= ~__SOFF;
    else {
        fp->_flags |= __SOFF;
        fp->_offset = ret;
    }
    return ret;
}

int __sclose(void *cookie)
{
    FILE *fp = (FILE *)cookie;

    return _close_r(fp->_data, fp->_file);
}
