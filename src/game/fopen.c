/* SN ProDG replacement for newlib 1.8.2 libc/stdio/findfp.c: a static pool of 10 FILEs instead
 * of malloc'd glue. fopen itself was dead-stripped; only its error string survived. */
#include "newlib_stdio.h"

FILE _sn_iobf[10];
struct _glue _sn_stat_g;

static void snstd(FILE *ptr, int flags, int file, struct _reent *data)
{
    ptr->_p = 0;
    ptr->_r = 0;
    ptr->_w = 0;
    ptr->_flags = flags;
    ptr->_file = file;
    ptr->_bf._base = 0;
    ptr->_lbfsize = 0;
    ptr->_cookie = ptr;
    ptr->_read = __sread;
    ptr->_write = __swrite;
    ptr->_seek = __sseek;
    ptr->_close = __sclose;
    ptr->_data = data;
}

void _sn_sinit(struct _reent *s)
{
    /* make sure we clean up on exit */
    s->__cleanup = _cleanup_r; /* conservative */
    s->__sdidinit = 1;

    snstd(s->__sf + 0, __SRD, 0, s);
    snstd(s->__sf + 1, __SWR | __SLBF, 1, s);
    snstd(s->__sf + 2, __SWR | __SNBF, 2, s);

    s->__sglue._next = &_sn_stat_g;
    s->__sglue._niobs = 3;
    s->__sglue._iobs = s->__sf;
    _sn_stat_g._next = NULL;
    _sn_stat_g._niobs = 10;
    _sn_stat_g._iobs = _sn_iobf;
}

void _cleanup_r(struct _reent *ptr)
{
    /* (void) _fwalk(fclose); */
    (void)_fwalk(ptr, fflush); /* `cheating' */
}

FILE *fopen(const char *file, const char *mode)
{
    write(1, "\nfopen error:  **too many open streams**\n", 0x2b);
    return NULL;
}
