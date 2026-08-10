/* newlib libc/string/strncmp.c */
#include "newlib_local.h"

#define UNALIGNED(X, Y) (((long)X & (sizeof(long) - 1)) | ((long)Y & (sizeof(long) - 1)))

int strncmp(const char *s1, const char *s2, size_t n)
{
    unsigned long *a1;
    unsigned long *a2;

    if (n == 0)
        return 0;

    if (!UNALIGNED(s1, s2)) {
        a1 = (unsigned long *)s1;
        a2 = (unsigned long *)s2;
        while (n >= sizeof(long) && *a1 == *a2) {
            n -= sizeof(long);
            if (n == 0 || DETECTNULL(*a1))
                return 0;
            a1++;
            a2++;
        }
        s1 = (char *)a1;
        s2 = (char *)a2;
    }

    while (n-- > 0 && *s1 == *s2) {
        if (n == 0 || *s1 == '\0')
            return 0;
        s1++;
        s2++;
    }
    return (*(unsigned char *)s1) - (*(unsigned char *)s2);
}
