/* SN Systems' rewrite of newlib 1.8.2 libc/stdio/vfprintf.c (FLOATING_POINT, WANT_PRINTF_LONG_LONG):
 * the __sfvwrite/iov machinery is replaced by direct writes (string streams) or a static 128-byte
 * write() buffer (`_vfwrite`), and the dtoa-based `cvt`/`exponent` by SN's own `fftoa`.
 * game/vfiprintf.c includes this file with INTEGER_ONLY defined (as newlib's vfiprintf.c does). */
#include "newlib_stdio.h"

#ifdef INTEGER_ONLY
#define _VFPRINTF_R _vfiprintf_r
#define _vfwrite _vfwrite_8006E684 /* sym_map name of vfiprintf's own static copy */
#else
#define _VFPRINTF_R _vfprintf_r
#define FLOATING_POINT
#endif

typedef int wchar_t;
typedef long long quad_t;
typedef unsigned long long u_quad_t;
typedef unsigned long u_long;
typedef unsigned int u_int;
typedef unsigned short u_short;

extern int __mb_cur_max;
#define MB_CUR_MAX __mb_cur_max
extern int _mbtowc_r(struct _reent *, wchar_t *, const char *, size_t, int *);
extern size_t strlen(const char *);
extern void *memchr(const void *, int, size_t);
extern void *memcpy(void *, const void *, size_t);
#ifdef FLOATING_POINT
struct lconv {
    char *decimal_point; /* only member used here (the full struct is in game/locale) */
};
extern struct lconv *localeconv(void);

/* game/math_support: fdlibm floor/fmod/log10 under SN names */
extern double sn_floor(double);
extern double sn_fmod(double, double);
extern double sn_log10(double);

extern int _vfiprintf_r();
#endif

#ifdef FLOATING_POINT
/* floatio.h */
#define MAXEXP 308
#define MAXFRACT 39
#define BUF (MAXEXP + MAXFRACT + 1) /* + decimal point */
#define DEFPREC 6
#else
#define BUF 40
#endif

/*
 * Macros for converting digits to letters and vice versa
 */
#define to_digit(c) ((c) - '0')
#define is_digit(c) ((unsigned)to_digit(c) <= 9)
#define to_char(n) ((n) + '0')

/*
 * Flags used during conversion.
 */
#define ALT 0x001       /* alternate form */
#define HEXPREFIX 0x002 /* add 0x or 0X prefix */
#define LADJUST 0x004   /* left adjustment */
#define LONGDBL 0x008   /* long double; unimplemented */
#define LONGINT 0x010   /* long integer */
#define QUADINT 0x020   /* quad integer */
#define SHORTINT 0x040  /* short integer */
#define ZEROPAD 0x080   /* zero (as opposed to blank) pad */
#define FPT 0x100       /* Floating point number */

#ifdef FLOATING_POINT
static int strround(char *str, int len)
{
    int i;

    if (len > 0) {
        i = len - 1;
        if (str[i] > '4') {
            do {
                str[i] = '0';
                i--;
            } while (i > 0 && str[i] == '9');
            if (str[i] == '9')
                return 0;
            str[i]++;
        }
    }
    return 1;
}

void strrev(char *str)
{
    int i, j;
    char c;

    for (j = 0; str[j] != 0; j++)
        ;
    for (i = 0, j = j - 1; i < j; i++, j--) {
        c = str[i];
        str[i] = str[j];
        str[j] = c;
    }
}

static void itoa(unsigned int value, char *str, int base)
{
    static char lower[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    int neg = (int)value < 0;
    int i;

    if (neg && base == 10)
        value = -value;
    i = 0;
    do {
        str[i++] = lower[value % base];
        value /= base;
    } while (value != 0);
    if (neg)
        str[i++] = '-';
    str[i] = 0;
    strrev(str);
}

char *fftoa(double value, int prec, char fmt, int strip, char *sign)
{
    static char str[164];
    union {
        double d;
        unsigned int w[2];
    } u;
    unsigned int hi, lo, se, sgn, ex, mant;
    int i, j, k;
    char *p;
    int ndig, exp, total, dp, n;
    double x, eps;

    /* COMPILER-DIFF: tie. The two do/while(0) blocks are loop notes only (no code): the second one doubles the
     * weight of the `lo`/`hi` word reads so global-alloc takes lo before sgn (lo r11, sgn r10 as in
     * the original; a plain block gives sgn r11 / lo r8), and the first one is the sched1 barrier
     * that keeps the `sign` parameter copy `mr r25,r4` ahead of the union's `fmr f0,f30` reload. */
    do { u.d = value; } while (0);
    do {
        lo = u.w[1];
        hi = u.w[0];
    } while (0);
    mant = hi & 0xfffff;
    se = hi >> 20;
    sgn = se & 0x800;
    ex = se & 0x7ff;
    if (ex == 0x7ff) {
        if (mant == 0 && lo == 0) {
            if (sgn)
                strcpy(str, "-Inf");
            else
                strcpy(str, "Inf");
        } else {
            strcpy(str, "NaN");
        }
        return str;
    }

    ndig = prec;
    i = 0;
    if (value < 0.0) {
        if (sign != NULL) {
            *sign = '-';
            p = str;
        } else {
            str[0] = '-';
            p = str + 1;
        }
        value = -value;
    } else {
        p = str;
    }

    if (value >= 1.0) {
        x = sn_floor(value);
        value -= x;
        while (i <= 162) {
            n = (int)sn_fmod(x, 10.0);
            p[i++] = to_char(n);
            x -= (double)n;
            x /= 10.0;
            if (!(x >= 1.0))
                break;
        }
        for (j = 0, k = i - 1; j < k; j++, k--) {
            char c = p[j];
            p[j] = p[k];
            p[k] = c;
        }
    }

    exp = i;
    if (exp == 0) {
        if (fmt != 'f') {
            if (value != 0.0) {
                value *= 10.0;
                while (value < 1.0 && exp > -1021) {
                    value *= 10.0;
                    exp--;
                }
                exp--;
            }
            if (value >= 1.0) {
                ndig = prec + 1;
                value /= 10.0;
            }
        } else {
            p[i++] = '0';
        }
    }

    if (exp < 0)
        total = ndig;
    else if (fmt == 'f')
        total = ndig + (exp > 0 ? exp : 1);
    else
        total = ndig + 1;

    eps = 5.5511151231257827e-17;
    for (;;) {
        value *= 10.0;
        n = (int)value;
        eps *= 10.0;
        value -= (double)n;
        if (i < total && value >= eps && value <= 1.0 - eps)
            p[i++] = to_char(n);
        else
            break;
    }
    if (value >= 0.5)
        n++;
    p[i++] = to_char(n);
    while (i <= total)
        p[i++] = '0';

    if (strround(p, total + 1) == 0) {
        p[0] = '1';
        total++;
        exp++;
    }

    if (prec != 0) {
        if (fmt == 'f') {
            if (exp > 0)
                dp = exp;
            else
                dp = 1;
        } else {
            dp = 1;
        }
        for (i = total; i > dp; i--)
            p[i] = p[i - 1];
        p[dp] = '.';
    } else {
        total--;
    }

    if (strip) {
        while (total != 0 && p[total] == '0')
            total--;
        if (p[total] == '.')
            total--;
    }

    if (fmt != 'f') {
        p[++total] = fmt;
        if (exp >= 0) {
            p[++total] = '+';
            if (exp != 0)
                exp--;
            if (exp <= 9)
                p[++total] = '0';
        } else {
            p[++total] = '-';
            if (exp > -10)
                p[++total] = '0';
            exp = -exp;
        }
        total++;
        itoa(exp, p + total, 10);
    } else {
        p[total + 1] = 0;
    }
    return str;
}
#endif /* FLOATING_POINT */

static int _vfwrite(int fd, const char *buf, size_t len, int flush)
{
    static char pch[128];
    static int cumulative_written = 0;
    static char *pch_pointer = pch;
    size_t i;

    if (flush == 1) {
        int r = write(fd, pch, cumulative_written);
        cumulative_written = 0;
        pch_pointer = pch;
        return r;
    }
    for (i = 0; i < len; i++) {
        *pch_pointer = buf[i];
        pch_pointer++;
        cumulative_written++;
        if (cumulative_written > 127) {
            int r = write(fd, pch, cumulative_written);
            pch_pointer = pch;
            cumulative_written = 0;
            if (r == 0)
                return 0;
        }
    }
    return i;
}

#ifndef INTEGER_ONLY
int vfprintf(FILE *fp, const char *fmt0, va_list ap)
{
    const char *p = fmt0;

    while (*p) {
        if (*p == '%' && p[1] != 0) {
            p++;
            while (*p <= '@' && p[1] != 0)
                p++;
            switch (*p) {
            case 'e':
            case 'E':
            case 'f':
            case 'g':
            case 'G':
            case 'L':
                return _vfprintf_r(fp->_data, fp, fmt0, ap);
            }
        }
        p++;
    }
    return _vfiprintf_r(fp->_data, fp, fmt0, ap);
}
#endif

int _VFPRINTF_R(struct _reent *data, FILE *fp, const char *fmt0, va_list ap)
{
    register char *fmt;   /* format string */
    register int ch;      /* character from fmt */
    register int n, m;    /* handy integers (short term usage) */
    register char *cp;    /* handy char pointer (short term usage) */
    register int flags;   /* flags as above */
    int ret;              /* return value accumulator */
    int width;            /* width from format (%8d), or 0 */
    int prec;             /* precision from format (%.3d), or -1 */
    char sign;            /* sign prefix (' ', '+', '-', or \0) */
    wchar_t wc;
#ifdef FLOATING_POINT
    char *decimal_point = localeconv()->decimal_point;
    double _double;       /* double precision arguments %[eEfgG] */
    int strip;
    char expstr[7];       /* buffer for exponent string */
#endif
    u_quad_t _uquad;      /* integer arguments %[diouxX] */
    enum { OCT, DEC, HEX } base; /* base for [diouxX] conversion */
    int dprec;            /* a copy of prec if [diouxX], 0 otherwise */
    int realsz;           /* field size expanded by dprec */
    int size;             /* size of converted field or string */
    int zpad;
    char *xdigs;          /* digits for [xX] conversion */
    char buf[BUF];        /* space for %c, %[diouxX], %[eEfgG] */
    char ox[2];           /* space for 0x hex-prefix */
    int state = 0;        /* mbtowc calls from library must not change state */

#define PADSIZE 16 /* pad chunk size */
    static const char blanks[PADSIZE] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    static const char zeroes[PADSIZE] = {'0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0'};

#define PRINT(ptr, len)                                     \
    {                                                       \
        if (fp->_flags & __SSTR) {                          \
            memcpy(fp->_p, (ptr), (len));                   \
            fp->_p += (len);                                \
        } else {                                            \
            if (fp->_flags == 0 && fp->_file == 0)          \
                fp->_file = 1;                              \
            _vfwrite(fp->_file, (ptr), (len), 0);           \
        }                                                   \
    }
#define PAD(howmany, with)                                  \
    {                                                       \
        if ((n = (howmany)) > 0) {                          \
            while (n > PADSIZE) {                           \
                PRINT(with, PADSIZE);                       \
                n -= PADSIZE;                               \
            }                                               \
            PRINT(with, n);                                 \
        }                                                   \
    }
#define FLUSH()                                             \
    {                                                       \
        if (!(fp->_flags & __SSTR))                         \
            _vfwrite(fp->_file, NULL, 0, 1);                \
    }

#define SARG()                                                        \
    (flags & QUADINT ? va_arg(ap, quad_t)                             \
     : flags & LONGINT ? va_arg(ap, long)                             \
     : flags & SHORTINT ? (long)(short)va_arg(ap, int)                \
     : (long)va_arg(ap, int))
#define UARG()                                                        \
    (flags & QUADINT ? va_arg(ap, u_quad_t)                           \
     : flags & LONGINT ? va_arg(ap, u_long)                           \
     : flags & SHORTINT ? (u_long)(u_short)va_arg(ap, int)            \
     : (u_long)va_arg(ap, u_int))

    fmt = (char *)fmt0;
    ret = 0;

    /*
     * Scan the format for conversions (`%' character).
     */
    for (;;) {
        cp = fmt;
        while ((n = _mbtowc_r(_REENT, &wc, fmt, MB_CUR_MAX, &state)) > 0) {
            fmt += n;
            if (wc == '%') {
                fmt--;
                break;
            }
        }
        if ((m = fmt - cp) != 0) {
            PRINT(cp, m);
            ret += m;
        }
        if (n <= 0)
            goto done;
        fmt++; /* skip over '%' */

        flags = 0;
        dprec = 0;
        width = 0;
        prec = -1;
        sign = '\0';

    rflag:
        ch = *fmt++;
    reswitch:
        switch (ch) {
        case ' ':
            if (!sign)
                sign = ' ';
            goto rflag;
        case '#':
            flags |= ALT;
            goto rflag;
        case '*':
            if ((width = va_arg(ap, int)) >= 0)
                goto rflag;
            width = -width;
            /* FALLTHROUGH */
        case '-':
            flags |= LADJUST;
            goto rflag;
        case '+':
            sign = '+';
            goto rflag;
        case '.':
            if ((ch = *fmt++) == '*') {
                n = va_arg(ap, int);
                prec = n < 0 ? -1 : n;
                goto rflag;
            }
            n = 0;
            while (is_digit(ch)) {
                n = 10 * n + to_digit(ch);
                ch = *fmt++;
            }
            prec = n < 0 ? -1 : n;
            goto reswitch;
        case '0':
            flags |= ZEROPAD;
            goto rflag;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            n = 0;
            do {
                n = 10 * n + to_digit(ch);
                ch = *fmt++;
            } while (is_digit(ch));
            width = n;
            goto reswitch;
#ifdef FLOATING_POINT
        case 'L':
            flags |= LONGDBL;
            goto rflag;
#endif
        case 'h':
            flags |= SHORTINT;
            goto rflag;
        case 'l':
            if (*fmt == 'l') {
                fmt++;
                flags |= QUADINT;
            } else {
                flags |= LONGINT;
            }
            goto rflag;
        case 'q':
            flags |= QUADINT;
            goto rflag;
        case 'c':
            *(cp = buf) = va_arg(ap, int);
            size = 1;
            sign = '\0';
            break;
        case 'D':
            flags |= LONGINT;
            /*FALLTHROUGH*/
        case 'd':
        case 'i':
            _uquad = SARG();
            if ((quad_t)_uquad < 0) {
                _uquad = -_uquad;
                sign = '-';
            }
            base = DEC;
            goto number;
#ifdef FLOATING_POINT
        case 'e':
        case 'E':
        case 'f':
        case 'g':
        case 'G':
            strip = 0;
            if (prec == -1)
                prec = DEFPREC;
            _double = va_arg(ap, double);
            if (ch == 'g' || ch == 'G') {
                double x;
                if (_double != 0)
                    x = sn_log10(_double < 0 ? -_double : _double);
                else
                    x = 1.0;
                if (x < -4.0 || x >= (double)prec)
                    ch = (ch == 'g') ? 'e' : 'E';
                else
                    ch = 'f';
                strip = 1;
            }
            if (flags & ALT) {
                if (prec == 0)
                    prec = 1;
                strip = 0;
            }
            cp = fftoa(_double, prec, ch, strip, &sign);
            size = strlen(cp);
            break;
#endif /* FLOATING_POINT */
        case 'n':
            if (flags & QUADINT)
                *va_arg(ap, quad_t *) = ret;
            else if (flags & LONGINT)
                *va_arg(ap, long *) = ret;
            else if (flags & SHORTINT)
                *va_arg(ap, short *) = ret;
            else
                *va_arg(ap, int *) = ret;
            continue; /* no output */
        case 'O':
            flags |= LONGINT;
            /*FALLTHROUGH*/
        case 'o':
            _uquad = UARG();
            base = OCT;
            goto nosign;
        case 'p':
            _uquad = (u_long)(unsigned int)va_arg(ap, void *);
            base = HEX;
            xdigs = "0123456789abcdef";
            flags |= HEXPREFIX;
            ch = 'x';
            goto nosign;
        case 's':
            if ((cp = va_arg(ap, char *)) == NULL)
                cp = "(null)";
            if (prec >= 0) {
                char *p = memchr(cp, 0, prec);

                if (p != NULL) {
                    size = p - cp;
                    if (size > prec)
                        size = prec;
                } else
                    size = prec;
            } else
                size = strlen(cp);
            sign = '\0';
            break;
        case 'U':
            flags |= LONGINT;
            /*FALLTHROUGH*/
        case 'u':
            _uquad = UARG();
            base = DEC;
            goto nosign;
        case 'X':
            xdigs = "0123456789ABCDEF";
            goto hex;
        case 'x':
            xdigs = "0123456789abcdef";
        hex:
            _uquad = UARG();
            base = HEX;
            /* leading 0x/X only if non-zero */
            if (flags & ALT && _uquad != 0)
                flags |= HEXPREFIX;

            /* unsigned conversions */
        nosign:
            sign = '\0';
        number:
            if ((dprec = prec) >= 0)
                flags &= ~ZEROPAD;

            cp = buf + BUF;
            if (_uquad != 0 || prec != 0) {
                switch (base) {
                case OCT:
                    do {
                        *--cp = to_char(_uquad & 7);
                        _uquad >>= 3;
                    } while (_uquad);
                    /* handle octal leading 0 */
                    if (flags & ALT && *cp != '0')
                        *--cp = '0';
                    break;

                case DEC:
                    /* many numbers are 1 digit */
                    while (_uquad >= 10) {
                        *--cp = to_char(_uquad % 10);
                        _uquad /= 10;
                    }
                    *--cp = to_char(_uquad);
                    break;

                case HEX:
                    do {
                        *--cp = xdigs[_uquad & 15];
                        _uquad >>= 4;
                    } while (_uquad);
                    break;

                default:
                    cp = "bug in vfprintf: bad base";
                    size = strlen(cp);
                    goto skipsize;
                }
            }
            size = buf + BUF - cp;
        skipsize:
            break;
        default: /* "%?" prints ?, unless ? is NUL */
            if (ch == '\0')
                goto done;
            /* pretend it was %c with argument ch */
            cp = buf;
            *cp = ch;
            size = 1;
            sign = '\0';
            break;
        }

        /*
         * Compute actual size, so we know how much to pad.
         * size excludes decimal prec; realsz includes it.
         */
        realsz = dprec > size ? dprec : size;
        if (sign)
            realsz++;
        else if (flags & HEXPREFIX)
            realsz += 2;
        zpad = dprec - size;

        /* right-adjusting blank padding */
        if ((flags & (LADJUST | ZEROPAD)) == 0)
            PAD(width - realsz, blanks);

        /* prefix */
        if (sign) {
            PRINT(&sign, 1);
        } else if (flags & HEXPREFIX) {
            ox[0] = '0';
            ox[1] = ch;
            PRINT(ox, 2);
        }

        /* right-adjusting zero padding */
        if ((flags & (LADJUST | ZEROPAD)) == ZEROPAD)
            PAD(width - realsz, zeroes);

        /* leading zeroes from decimal precision */
        PAD(zpad, zeroes);

        /* the string or number proper */
        PRINT(cp, size);

        /* left-adjusting padding (always blank) */
        if (flags & LADJUST)
            PAD(width - realsz, blanks);

        /* finally, adjust ret */
        ret += width > realsz ? width : realsz;
    }
done:
    FLUSH();
    return ret;
}
