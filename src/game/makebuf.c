/* newlib 1.8.2 libc/stdio/makebuf.c, SN version: no malloc, every stream ends up unbuffered */
#include "newlib_stdio.h"

void __smakebuf(register FILE *fp)
{
    struct stat st;

    if (fp->_flags & __SNBF) {
        fp->_bf._base = fp->_p = fp->_nbuf;
        fp->_bf._size = 1;
        return;
    }
    if (fp->_file < 0 || _fstat_r(fp->_data, fp->_file, &st) < 0) {
        /* do not try to optimise fseek() */
        fp->_flags |= __SNPT;
    } else {
        /*
         * Optimize fseek() only if it is a regular file.
         * (The test for __sseek is mainly paranoia.)
         */
        if ((st.st_mode & S_IFMT) == S_IFREG && fp->_seek == __sseek) {
            fp->_flags |= __SOPT;
            fp->_blksize = 1024;
        } else
            fp->_flags |= __SNPT;
    }
    fp->_flags |= __SNBF;
    fp->_bf._base = fp->_p = fp->_nbuf;
    fp->_bf._size = 1;
}
