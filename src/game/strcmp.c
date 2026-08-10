/* newlib libc/string/strcmp.c */
#include "newlib_local.h"

#define UNALIGNED(X, Y) (((long)X & (sizeof(long) - 1)) | ((long)Y & (sizeof(long) - 1)))

int strcmp(const char *s1, const char *s2)
{
    unsigned long *a1;
    unsigned long *a2;

    if (!UNALIGNED(s1, s2)) {
        a1 = (unsigned long *)s1;
        a2 = (unsigned long *)s2;
        while (*a1 == *a2) {
            if (DETECTNULL(*a1))
                return 0;
            a1++;
            a2++;
        }
        s1 = (char *)a1;
        s2 = (char *)a2;
    }

    while (*s1 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (*(unsigned char *)s1) - (*(unsigned char *)s2);
}
