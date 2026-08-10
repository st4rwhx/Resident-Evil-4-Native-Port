/* newlib 1.8.2 libc/stdio/ungetc.c, SN version: no __submore (static buffer only) */
#include "newlib_stdio.h"

int ungetc(int c, register FILE *fp)
{
    if (c == EOF)
        return (EOF);

    /* Ensure stdio has been initialized.
       ??? Might be able to remove this as some other stdio routine should
       have already been called to get the char we are un-getting.  */

    CHECK_INIT(fp);

    /* After ungetc, we won't be at eof anymore */
    fp->_flags &= ~__SEOF;

    if ((fp->_flags & __SRD) == 0) {
        /*
         * Not already reading: no good unless reading-and-writing.
         * Otherwise, flush any current write stuff.
         */
        if ((fp->_flags & __SRW) == 0)
            return EOF;
        if (fp->_flags & __SWR) {
            if (fflush(fp))
                return EOF;
            fp->_flags &= ~__SWR;
            fp->_w = 0;
            fp->_lbfsize = 0;
        }
        fp->_flags |= __SRD;
    }
    c = (unsigned char)c;

    /*
     * If we are in the middle of ungetc'ing, just continue.
     * This may require expanding the current ungetc buffer.
     */

    if (HASUB(fp)) {
        if (fp->_r >= fp->_ub._size) {
            write(1, "\nungetc error: *static buffer overflow* - char not added to buffer\n", 0x44);
            return EOF;
        }
        *--fp->_p = c;
        fp->_r++;
        return c;
    }

    /*
     * If we can handle this by simply backing up, do so,
     * but never replace the original character.
     * (This makes sscanf() work when scanning `const' data.)
     */

    if (fp->_bf._base != NULL && fp->_p > fp->_bf._base && fp->_p[-1] == c) {
        fp->_p--;
        fp->_r++;
        return c;
    }

    /*
     * Create an ungetc buffer.
     * Initially, we will use the `reserve' buffer.
     */

    fp->_ur = fp->_r;
    fp->_up = fp->_p;
    fp->_ub._base = fp->_ubuf;
    fp->_ub._size = sizeof(fp->_ubuf);
    fp->_ubuf[sizeof(fp->_ubuf) - 1] = c;
    fp->_p = &fp->_ubuf[sizeof(fp->_ubuf) - 1];
    fp->_r = 1;
    return c;
}
