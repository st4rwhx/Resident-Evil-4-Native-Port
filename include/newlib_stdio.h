/* newlib 1.8.2 internals (sys/reent.h, stdio.h, stdio/local.h) as built into SN ProDG's libc.
 * SN changes: __sFILE._ubuf is 10 bytes (FILE is 0x60), stdio init is _sn_sinit() with a static
 * FILE pool (_sn_iobf) instead of malloc. */
#ifndef NEWLIB_STDIO_H
#define NEWLIB_STDIO_H

#include "newlib_local.h"

/* gcc 2.95 ginclude/va-ppc.h (System V.4) */
#include "va_ppc.h"

typedef long _fpos_t;
typedef _fpos_t fpos_t;
typedef long off_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

struct _glue {
    struct _glue *_next;
    int _niobs;
    struct __sFILE *_iobs;
};

struct _Bigint {
    struct _Bigint *_next;
    int _k, _maxwds, _sign, _wds;
    unsigned long _x[1];
};

#define _ATEXIT_SIZE 32

struct _atexit {
    struct _atexit *_next;
    int _ind;
    void (*_fns[_ATEXIT_SIZE])(void);
};

struct __sbuf {
    unsigned char *_base;
    int _size;
};

struct __sFILE {
    unsigned char *_p;
    int _r;
    int _w;
    short _flags;
    short _file;
    struct __sbuf _bf;
    int _lbfsize;

    void *_cookie;

    int (*_read)(void *_cookie, char *_buf, int _n);
    int (*_write)(void *_cookie, const char *_buf, int _n);
    _fpos_t (*_seek)(void *_cookie, _fpos_t _offset, int _whence);
    int (*_close)(void *_cookie);

    struct __sbuf _ub;
    unsigned char *_up;
    int _ur;

    unsigned char _ubuf[10];
    unsigned char _nbuf[1];

    struct __sbuf _lb;

    int _blksize;
    int _offset;

    struct _reent *_data;
};

typedef struct __sFILE FILE;

struct _reent {
    int _errno;
    struct __sFILE *_stdin, *_stdout, *_stderr;
    int _inc;
    char _emergency[25];
    int _current_category;
    const char *_current_locale;
    int __sdidinit;
    void (*__cleanup)(struct _reent *);
    struct _Bigint *_result;
    int _result_k;
    struct _Bigint *_p5s;
    struct _Bigint **_freelist;
    int _cvtlen;
    char *_cvtbuf;
    union {
        struct {
            unsigned int _rand_next;
            char *_strtok_last;
            char _asctime_buf[26];
            struct tm _localtime_buf;
            int _gamma_signgam;
        } _reent;
        struct {
#define _N_LISTS 30
            unsigned char *_nextf[_N_LISTS];
            unsigned int _nmalloc[_N_LISTS];
        } _unused;
    } _new;
    struct _atexit *_atexit;
    struct _atexit _atexit0;
    void (**(_sig_func))(int);
    struct _glue __sglue;
    struct __sFILE __sf[3];
};

#define _REENT_INIT(var) \
    { 0, &var.__sf[0], &var.__sf[1], &var.__sf[2], 0, "", 0, "C", \
      0, NULL, NULL, 0, NULL, NULL, 0, NULL, { {1, NULL, "", \
      { 0,0,0,0,0,0,0,0}, 0 } } }

extern struct _reent *_impure_ptr;
#define _REENT _impure_ptr

#define _stdin_r(x) ((x)->_stdin)
#define _stdout_r(x) ((x)->_stdout)
#define _stderr_r(x) ((x)->_stderr)

#define __SLBF 0x0001
#define __SNBF 0x0002
#define __SRD  0x0004
#define __SWR  0x0008
#define __SRW  0x0010
#define __SEOF 0x0020
#define __SERR 0x0040
#define __SMBF 0x0080
#define __SAPP 0x0100
#define __SSTR 0x0200
#define __SOPT 0x0400
#define __SNPT 0x0800
#define __SOFF 0x1000
#define __SMOD 0x2000

#define EOF (-1)
#define BUFSIZ 1024
#define INT_MAX 2147483647

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* sys/stat.h */
typedef short dev_t;
typedef unsigned short ino_t;
typedef unsigned int mode_t;
typedef unsigned short nlink_t;
typedef unsigned short uid_t;
typedef unsigned short gid_t;
typedef long time_t;

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    time_t st_atime;
    long st_spare1;
    time_t st_mtime;
    long st_spare2;
    time_t st_ctime;
    long st_spare3;
    long st_blksize;
    long st_blocks;
    long st_spare4[2];
};

#define S_IFMT  0170000
#define S_IFCHR 0020000
#define S_IFREG 0100000

/* stdio/local.h */
extern int __svfscanf(FILE *, const char *, va_list);
extern int __srefill(FILE *);
extern int __sread(void *, char *, int);
extern int __swrite(void *, char const *, int);
extern fpos_t __sseek(void *, fpos_t, int);
extern int __sclose(void *);
extern void _sn_sinit();
extern void _cleanup_r(struct _reent *);
extern void __smakebuf(FILE *);
extern int _fwalk(struct _reent *, int (*)());

#define CHECK_INIT(fp) \
    do { \
        if ((fp)->_data == 0) \
            (fp)->_data = _REENT; \
        if (!(fp)->_data->__sdidinit) \
            _sn_sinit((fp)->_data); \
    } while (0)

#define HASUB(fp) ((fp)->_ub._base != NULL)

/* stdio.h / reent.h prototypes */
extern int fflush(FILE *);
extern int ungetc(int, FILE *);
extern size_t fread(void *, size_t, size_t, FILE *);
extern int vfprintf(FILE *, const char *, va_list);
extern int _vfprintf_r(struct _reent *, FILE *, const char *, va_list);
extern int printf(const char *, ...);
extern int fprintf(FILE *, const char *, ...);
extern int sprintf(char *, const char *, ...);
extern int vprintf(const char *, va_list);
extern int vsprintf(char *, const char *, va_list);
extern int sscanf(const char *, const char *, ...);

extern int _close_r(struct _reent *, int);
extern int _fstat_r(struct _reent *, int, struct stat *);
extern off_t _lseek_r(struct _reent *, int, off_t, int);
extern long _read_r(struct _reent *, int, void *, size_t);
extern long _write_r(struct _reent *, int, const void *, size_t);

/* unistd.h (SN stubs in lib/dummy.c) */
extern int close(int);
extern int fstat();
extern off_t lseek(int, off_t, int);
extern int read(int, void *, size_t);
extern int write(int, const void *, size_t);
extern void _exit(int) __attribute__((noreturn));
extern void exit(int) __attribute__((noreturn));

extern int errno;

#endif
