/* newlib 1.8.2 libc/locale/locale.c (MB_CAPABLE); _setlocale_r/setlocale were dead-stripped */
#include "newlib_stdio.h"

/* limits.h: ProDG chars are unsigned (-D__CHAR_UNSIGNED__) */
#ifdef __CHAR_UNSIGNED__
#define CHAR_MAX 255
#else
#define CHAR_MAX 127
#endif
#define LC_ALL 0
#define LC_CTYPE 2

struct lconv {
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
    char *int_curr_symbol;
    char *currency_symbol;
    char *mon_decimal_point;
    char *mon_thousands_sep;
    char *mon_grouping;
    char *positive_sign;
    char *negative_sign;
    char int_frac_digits;
    char frac_digits;
    char p_cs_precedes;
    char p_sep_by_space;
    char n_cs_precedes;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
};

extern int strcmp(const char *, const char *);
extern size_t strlen(const char *);

int __mb_cur_max = 1;

static const struct lconv lconv = {
    ".", "", "", "", "", "", "", "", "", "",
    CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX,
    CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX,
};

char *_setlocale_r(struct _reent *p, int category, const char *locale)
{
    static char lc_ctype[8] = "C";
    static char last_lc_ctype[8] = "C";

    if (locale) {
        if (category != LC_CTYPE) {
            if (strcmp(locale, "C") && strcmp(locale, ""))
                return 0;
            if (category == LC_ALL) {
                strcpy(last_lc_ctype, lc_ctype);
                strcpy(lc_ctype, locale);
                __mb_cur_max = 1;
            }
        } else {
            if (strcmp(locale, "C") && strcmp(locale, "") &&
                strcmp(locale, "C") && strcmp(locale, "C-JIS") &&
                strcmp(locale, "C-EUCJP") && strcmp(locale, "C-SJIS"))
                return 0;

            strcpy(last_lc_ctype, lc_ctype);
            strcpy(lc_ctype, locale);

            if (!strcmp(locale, "C-JIS"))
                __mb_cur_max = 8;
            else if (strlen(locale) > 1)
                __mb_cur_max = 2;
            else
                __mb_cur_max = 1;
        }
        p->_current_category = category;
        p->_current_locale = locale;

        if (category == LC_CTYPE)
            return last_lc_ctype;
    } else {
        if (category == LC_CTYPE)
            return lc_ctype;
    }

    return "C";
}

struct lconv *_localeconv_r(struct _reent *data)
{
    return (struct lconv *)&lconv;
}

char *setlocale(int category, const char *locale)
{
    return _setlocale_r(_REENT, category, locale);
}

struct lconv *localeconv(void)
{
    return _localeconv_r(_REENT);
}
