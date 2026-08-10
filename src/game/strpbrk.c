/* newlib libc/string/strpbrk.c */
#include "newlib_local.h"

char *strpbrk(const char *s1, const char *s2)
{
    const char *c = s2;
    if (!*s1)
        return (char *)NULL;

    while (*s1) {
        for (c = s2; *c; c++) {
            if (*s1 == *c)
                break;
        }
        if (*c)
            break;
        s1++;
    }

    if (*c == '\0')
        s1 = NULL;

    return (char *)s1;
}
